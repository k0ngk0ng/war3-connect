/*
 * server.c – TCP server core implementation using select().
 *
 * Cross-platform: compiles on Windows (winsock2) and POSIX (Linux/macOS).
 */

#include "server.h"
#include "handler.h"
#include "../common/protocol.h"
#include "../common/message.h"
#include "../third_party/cJSON/cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#   define CLOSE_SOCKET(s) closesocket(s)
#else
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <sys/select.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#   include <unistd.h>
#   include <fcntl.h>
#   include <errno.h>
#   define CLOSE_SOCKET(s) close(s)
#endif

/* Heartbeat timeout in seconds.  Users that don't send a heartbeat
 * within this interval are disconnected. */
#define HEARTBEAT_TIMEOUT 60

/* ================================================================== */
/*  Internal helpers                                                   */
/* ================================================================== */

/*
 * Disconnect a user: leave room if in one, close socket, free slot.
 */
static void DisconnectUser(User *user, User users[], int user_count,
                           Room rooms[], int room_count)
{
    printf("[server] disconnecting '%s' (fd %d, ip %s)\n",
           user->username[0] ? user->username : "(no name)",
           user->fd, user->ip);

    /* If the user is in a room, perform a room-leave. */
    if (user->room_id != -1) {
        int room_id = user->room_id;
        user->room_id = -1;

        /* Notify remaining room members. */
        if (user->username[0] != '\0') {
            /* Build player_left JSON. */
            cJSON *note = cJSON_CreateObject();
            cJSON_AddStringToObject(note, "type", MSG_PLAYER_LEFT);
            cJSON_AddStringToObject(note, "username", user->username);
            char *s = cJSON_PrintUnformatted(note);
            cJSON_Delete(note);
            if (s) {
                for (int i = 0; i < user_count; i++) {
                    if (users[i].fd != -1 && users[i].room_id == room_id) {
                        /* Re-use SendToFd logic inline to avoid circular dep. */
                        uint32_t frame_len = 0;
                        uint8_t *frame = Protocol_Frame(s, &frame_len);
                        if (frame) {
                            uint32_t sent = 0;
                            while (sent < frame_len) {
                                int n = send(users[i].fd,
                                             (const char *)(frame + sent),
                                             (int)(frame_len - sent), 0);
                                if (n <= 0) break;
                                sent += (uint32_t)n;
                            }
                            free(frame);
                        }
                    }
                }
                free(s);
            }
        }

        /* Destroy room if empty, otherwise broadcast updated peers. */
        int remaining = Users_CountInRoom(users, user_count, room_id);
        if (remaining == 0) {
            printf("[server] room %d is empty, destroying\n", room_id);
            Rooms_Destroy(rooms, room_count, room_id);
        } else {
            /* Broadcast room_peers to remaining members. */
            cJSON *root  = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "type", MSG_ROOM_PEERS);
            cJSON *peers = cJSON_AddArrayToObject(root, "peers");
            for (int i = 0; i < user_count; i++) {
                if (users[i].fd != -1 && users[i].room_id == room_id) {
                    cJSON *peer = cJSON_CreateObject();
                    cJSON_AddStringToObject(peer, "username",
                                            users[i].username);
                    cJSON_AddStringToObject(peer, "ip", users[i].ip);
                    cJSON_AddItemToArray(peers, peer);
                }
            }
            char *s = cJSON_PrintUnformatted(root);
            cJSON_Delete(root);
            if (s) {
                for (int i = 0; i < user_count; i++) {
                    if (users[i].fd != -1 && users[i].room_id == room_id) {
                        uint32_t frame_len = 0;
                        uint8_t *frame = Protocol_Frame(s, &frame_len);
                        if (frame) {
                            uint32_t sent = 0;
                            while (sent < frame_len) {
                                int n = send(users[i].fd,
                                             (const char *)(frame + sent),
                                             (int)(frame_len - sent), 0);
                                if (n <= 0) break;
                                sent += (uint32_t)n;
                            }
                            free(frame);
                        }
                    }
                }
                free(s);
            }
        }
    }

    CLOSE_SOCKET(user->fd);
    Users_FreeSlot(user);
}

/* ================================================================== */
/*  Public API                                                         */
/* ================================================================== */

int Server_Init(Server *srv, int port)
{
    if (srv == NULL) return -1;

    memset(srv, 0, sizeof(*srv));
    srv->port      = port;
    srv->listen_fd = -1;

    Users_Init(srv->users, MAX_USERS);
    Rooms_Init(srv->rooms, MAX_ROOMS);

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("[server] WSAStartup failed\n");
        return -1;
    }
#endif

    /* Create listening socket. */
    srv->listen_fd = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (srv->listen_fd < 0) {
        printf("[server] socket() failed\n");
        return -1;
    }

    /* Allow address reuse. */
    int opt = 1;
#ifdef _WIN32
    setsockopt(srv->listen_fd, SOL_SOCKET, SO_REUSEADDR,
               (const char *)&opt, sizeof(opt));
#else
    setsockopt(srv->listen_fd, SOL_SOCKET, SO_REUSEADDR,
               &opt, sizeof(opt));
#endif

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons((uint16_t)port);

    if (bind(srv->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("[server] bind() failed on port %d\n", port);
        CLOSE_SOCKET(srv->listen_fd);
        srv->listen_fd = -1;
        return -1;
    }

    if (listen(srv->listen_fd, 16) < 0) {
        printf("[server] listen() failed\n");
        CLOSE_SOCKET(srv->listen_fd);
        srv->listen_fd = -1;
        return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------ */

void Server_Run(Server *srv)
{
    if (srv == NULL || srv->listen_fd < 0) return;

    printf("[server] entering main event loop\n");

    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(srv->listen_fd, &readfds);

        int max_fd = srv->listen_fd;

        for (int i = 0; i < MAX_USERS; i++) {
            if (srv->users[i].fd != -1) {
                FD_SET(srv->users[i].fd, &readfds);
                if (srv->users[i].fd > max_fd) {
                    max_fd = srv->users[i].fd;
                }
            }
        }

        struct timeval tv;
        tv.tv_sec  = 1;
        tv.tv_usec = 0;

        int ready = select(max_fd + 1, &readfds, NULL, NULL, &tv);
        if (ready < 0) {
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err == WSAEINTR) continue;
#else
            if (errno == EINTR) continue;
#endif
            printf("[server] select() error\n");
            break;
        }

        /* ---- Accept new connections ---- */
        if (ready > 0 && FD_ISSET(srv->listen_fd, &readfds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int client_fd = (int)accept(srv->listen_fd,
                                        (struct sockaddr *)&client_addr,
                                        &addr_len);
            if (client_fd >= 0) {
                User *slot = Users_AllocSlot(srv->users, MAX_USERS);
                if (slot == NULL) {
                    printf("[server] no user slots available, rejecting\n");
                    CLOSE_SOCKET(client_fd);
                } else {
                    slot->fd             = client_fd;
                    slot->room_id        = -1;
                    slot->last_heartbeat = time(NULL);
                    slot->recv_len       = 0;
                    slot->username[0]    = '\0';

                    /* Store client IP from accept(). */
                    inet_ntop(AF_INET, &client_addr.sin_addr,
                              slot->ip, MAX_IP_STR);

                    printf("[server] new connection from %s (fd %d)\n",
                           slot->ip, slot->fd);
                }
            }
        }

        /* ---- Handle readable client sockets ---- */
        for (int i = 0; i < MAX_USERS; i++) {
            if (srv->users[i].fd == -1) continue;
            if (!FD_ISSET(srv->users[i].fd, &readfds)) continue;

            User *user = &srv->users[i];
            int space  = (int)(MAX_MSG_SIZE - user->recv_len);
            if (space <= 0) {
                /* Buffer full with no complete message – protocol error. */
                printf("[server] recv buffer overflow for fd %d\n", user->fd);
                DisconnectUser(user, srv->users, MAX_USERS,
                               srv->rooms, MAX_ROOMS);
                continue;
            }

            int n = recv(user->fd,
                         (char *)(user->recv_buf + user->recv_len),
                         space, 0);
            if (n <= 0) {
                /* Connection closed or error. */
                DisconnectUser(user, srv->users, MAX_USERS,
                               srv->rooms, MAX_ROOMS);
                continue;
            }

            user->recv_len += (uint32_t)n;

            /* Extract and process complete messages. */
            while (1) {
                char *json_str = NULL;
                uint32_t consumed = Protocol_Extract(user->recv_buf,
                                                     user->recv_len,
                                                     &json_str);
                if (consumed == 0) break;

                Handler_ProcessMessage(json_str, user,
                                       srv->users, MAX_USERS,
                                       srv->rooms, MAX_ROOMS);
                free(json_str);

                /* Shift remaining data to the front of the buffer. */
                uint32_t remaining = user->recv_len - consumed;
                if (remaining > 0) {
                    memmove(user->recv_buf,
                            user->recv_buf + consumed,
                            remaining);
                }
                user->recv_len = remaining;

                /* Safety: if user was disconnected during processing,
                 * stop processing further messages. */
                if (user->fd == -1) break;
            }
        }

        /* ---- Heartbeat timeout check ---- */
        {
            time_t now = time(NULL);
            for (int i = 0; i < MAX_USERS; i++) {
                if (srv->users[i].fd == -1) continue;
                if (now - srv->users[i].last_heartbeat > HEARTBEAT_TIMEOUT) {
                    printf("[server] heartbeat timeout for '%s' (fd %d)\n",
                           srv->users[i].username[0]
                               ? srv->users[i].username : "(no name)",
                           srv->users[i].fd);
                    DisconnectUser(&srv->users[i], srv->users, MAX_USERS,
                                   srv->rooms, MAX_ROOMS);
                }
            }
        }
    }
}

/* ------------------------------------------------------------------ */

void Server_Shutdown(Server *srv)
{
    if (srv == NULL) return;

    printf("[server] shutting down\n");

    /* Close all client connections. */
    for (int i = 0; i < MAX_USERS; i++) {
        if (srv->users[i].fd != -1) {
            CLOSE_SOCKET(srv->users[i].fd);
            Users_FreeSlot(&srv->users[i]);
        }
    }

    /* Close listening socket. */
    if (srv->listen_fd >= 0) {
        CLOSE_SOCKET(srv->listen_fd);
        srv->listen_fd = -1;
    }

#ifdef _WIN32
    WSACleanup();
#endif
}
