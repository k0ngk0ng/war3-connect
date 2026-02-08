/*
 * handler.c – Message handler implementation.
 *
 * Parses incoming JSON messages, dispatches by "type" field, and sends
 * appropriate responses / broadcasts.
 */

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
    typedef int ssize_t;
#else
#   include <sys/socket.h>
#   include <unistd.h>
#endif

/* ================================================================== */
/*  Internal helpers                                                   */
/* ================================================================== */

/*
 * Send a framed JSON string to a specific socket fd.
 * Handles partial sends.
 */
static void SendToFd(int fd, const char *json_str)
{
    uint32_t frame_len = 0;
    uint8_t *frame = Protocol_Frame(json_str, &frame_len);
    if (frame == NULL) return;

    uint32_t sent = 0;
    while (sent < frame_len) {
        int n = send(fd, (const char *)(frame + sent),
                     (int)(frame_len - sent), 0);
        if (n <= 0) break;          /* connection error – give up */
        sent += (uint32_t)n;
    }

    free(frame);
}

/*
 * Build and send a room_peers message to every user in the given room.
 *
 * JSON format:
 *   {"type":"room_peers","peers":[{"username":"p1","ip":"1.2.3.4"}, ...]}
 *
 * All peers in the room are included (the client filters itself out).
 */
static void BroadcastRoomPeers(int room_id,
                                User users[], int user_count)
{
    /* Build the peers JSON array. */
    cJSON *root  = cJSON_CreateObject();
    cJSON *peers = cJSON_AddArrayToObject(root, "peers");
    cJSON_AddStringToObject(root, "type", MSG_ROOM_PEERS);

    for (int i = 0; i < user_count; i++) {
        if (users[i].fd != -1 && users[i].room_id == room_id) {
            cJSON *peer = cJSON_CreateObject();
            cJSON_AddStringToObject(peer, "username", users[i].username);
            cJSON_AddStringToObject(peer, "ip", users[i].ip);
            cJSON_AddItemToArray(peers, peer);
        }
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str == NULL) return;

    /* Send to every user in the room. */
    for (int i = 0; i < user_count; i++) {
        if (users[i].fd != -1 && users[i].room_id == room_id) {
            SendToFd(users[i].fd, json_str);
        }
    }

    free(json_str);
}

/*
 * Send a simple JSON error message to a single fd.
 */
static void SendError(int fd, const char *message)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", MSG_ERROR);
    cJSON_AddStringToObject(root, "message", message);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str) {
        SendToFd(fd, json_str);
        free(json_str);
    }
}

/* ================================================================== */
/*  Per-type handlers                                                  */
/* ================================================================== */

/* ---- login -------------------------------------------------------- */
static void HandleLogin(cJSON *root, User *sender,
                         User users[], int user_count)
{
    cJSON *j_name = cJSON_GetObjectItem(root, "username");
    if (!cJSON_IsString(j_name) || j_name->valuestring[0] == '\0') {
        SendError(sender->fd, "missing or empty username");
        return;
    }

    const char *name = j_name->valuestring;

    /* Check if username is already taken by another connected user. */
    User *existing = Users_FindByName(users, user_count, name);
    if (existing != NULL && existing != sender) {
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, "type", MSG_LOGIN_FAIL);
        cJSON_AddStringToObject(resp, "reason", "username already taken");
        char *s = cJSON_PrintUnformatted(resp);
        cJSON_Delete(resp);
        if (s) { SendToFd(sender->fd, s); free(s); }
        printf("[login] rejected '%s' from fd %d – name taken\n", name, sender->fd);
        return;
    }

    /* Accept login. */
    strncpy(sender->username, name, MAX_USERNAME - 1);
    sender->username[MAX_USERNAME - 1] = '\0';

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "type", MSG_LOGIN_OK);
    cJSON_AddStringToObject(resp, "username", sender->username);
    char *s = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    if (s) { SendToFd(sender->fd, s); free(s); }

    printf("[login] '%s' logged in from %s (fd %d)\n",
           sender->username, sender->ip, sender->fd);
}

/* ---- room_list ---------------------------------------------------- */
static void HandleRoomList(User *sender,
                            User users[], int user_count,
                            Room rooms[], int room_count)
{
    RoomInfo list[MAX_ROOMS];
    int n = Rooms_GetList(rooms, room_count, list, MAX_ROOMS,
                          users, user_count);

    cJSON *root  = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", MSG_ROOM_LIST_RES);
    cJSON *arr = cJSON_AddArrayToObject(root, "rooms");

    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "id",      list[i].id);
        cJSON_AddStringToObject(item, "name",    list[i].name);
        cJSON_AddNumberToObject(item, "players", list[i].player_count);
        cJSON_AddNumberToObject(item, "max",     list[i].max_players);
        cJSON_AddItemToArray(arr, item);
    }

    char *s = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (s) { SendToFd(sender->fd, s); free(s); }
}

/* ---- room_create -------------------------------------------------- */
static void HandleRoomCreate(cJSON *root, User *sender,
                              User users[], int user_count,
                              Room rooms[], int room_count)
{
    if (sender->room_id != -1) {
        SendError(sender->fd, "already in a room");
        return;
    }

    cJSON *j_name = cJSON_GetObjectItem(root, "name");
    cJSON *j_max  = cJSON_GetObjectItem(root, "max_players");

    const char *rname = cJSON_IsString(j_name) ? j_name->valuestring : "Unnamed";
    int max_p = cJSON_IsNumber(j_max) ? j_max->valueint : MAX_ROOM_PLAYERS;
    if (max_p < 1)                max_p = 1;
    if (max_p > MAX_ROOM_PLAYERS) max_p = MAX_ROOM_PLAYERS;

    Room *room = Rooms_Create(rooms, room_count, rname, max_p, sender->fd);
    if (room == NULL) {
        SendError(sender->fd, "no room slots available");
        return;
    }

    /* Auto-join the creator. */
    sender->room_id = room->id;

    printf("[room] '%s' created room %d '%s' (max %d)\n",
           sender->username, room->id, room->name, room->max_players);

    /* Send room_created to the creator. */
    {
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, "type", MSG_ROOM_CREATED);
        cJSON_AddNumberToObject(resp, "room_id", room->id);
        cJSON_AddStringToObject(resp, "name", room->name);
        char *s = cJSON_PrintUnformatted(resp);
        cJSON_Delete(resp);
        if (s) { SendToFd(sender->fd, s); free(s); }
    }

    /* Send room_peers to everyone in the room (just the creator for now). */
    BroadcastRoomPeers(room->id, users, user_count);
}

/* ---- room_join ---------------------------------------------------- */
static void HandleRoomJoin(cJSON *root, User *sender,
                            User users[], int user_count,
                            Room rooms[], int room_count)
{
    if (sender->room_id != -1) {
        SendError(sender->fd, "already in a room");
        return;
    }

    cJSON *j_id = cJSON_GetObjectItem(root, "room_id");
    if (!cJSON_IsNumber(j_id)) {
        SendError(sender->fd, "missing room_id");
        return;
    }

    int room_id = j_id->valueint;
    Room *room = Rooms_FindById(rooms, room_count, room_id);
    if (room == NULL) {
        SendError(sender->fd, "room not found");
        return;
    }

    int cur = Users_CountInRoom(users, user_count, room_id);
    if (cur >= room->max_players) {
        SendError(sender->fd, "room is full");
        return;
    }

    /* Join the room. */
    sender->room_id = room_id;

    printf("[room] '%s' joined room %d '%s'\n",
           sender->username, room->id, room->name);

    /* Send room_joined to the joiner. */
    {
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, "type", MSG_ROOM_JOINED);
        cJSON_AddNumberToObject(resp, "room_id", room->id);
        cJSON_AddStringToObject(resp, "name", room->name);
        char *s = cJSON_PrintUnformatted(resp);
        cJSON_Delete(resp);
        if (s) { SendToFd(sender->fd, s); free(s); }
    }

    /* Send player_joined to other members. */
    {
        cJSON *note = cJSON_CreateObject();
        cJSON_AddStringToObject(note, "type", MSG_PLAYER_JOINED);
        cJSON_AddStringToObject(note, "username", sender->username);
        cJSON_AddStringToObject(note, "ip", sender->ip);
        char *s = cJSON_PrintUnformatted(note);
        cJSON_Delete(note);
        if (s) {
            for (int i = 0; i < user_count; i++) {
                if (users[i].fd != -1 &&
                    users[i].room_id == room_id &&
                    users[i].fd != sender->fd)
                {
                    SendToFd(users[i].fd, s);
                }
            }
            free(s);
        }
    }

    /* Broadcast updated room_peers to everyone in the room. */
    BroadcastRoomPeers(room_id, users, user_count);
}

/* ---- room_leave --------------------------------------------------- */
static void HandleRoomLeave(User *sender,
                             User users[], int user_count,
                             Room rooms[], int room_count)
{
    if (sender->room_id == -1) {
        SendError(sender->fd, "not in a room");
        return;
    }

    int room_id = sender->room_id;
    sender->room_id = -1;

    printf("[room] '%s' left room %d\n", sender->username, room_id);

    /* Send room_left to the leaver. */
    {
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, "type", MSG_ROOM_LEFT);
        char *s = cJSON_PrintUnformatted(resp);
        cJSON_Delete(resp);
        if (s) { SendToFd(sender->fd, s); free(s); }
    }

    /* Send player_left to remaining members. */
    {
        cJSON *note = cJSON_CreateObject();
        cJSON_AddStringToObject(note, "type", MSG_PLAYER_LEFT);
        cJSON_AddStringToObject(note, "username", sender->username);
        char *s = cJSON_PrintUnformatted(note);
        cJSON_Delete(note);
        if (s) {
            for (int i = 0; i < user_count; i++) {
                if (users[i].fd != -1 && users[i].room_id == room_id) {
                    SendToFd(users[i].fd, s);
                }
            }
            free(s);
        }
    }

    /* If room is now empty, destroy it. */
    int remaining = Users_CountInRoom(users, user_count, room_id);
    if (remaining == 0) {
        printf("[room] room %d is empty, destroying\n", room_id);
        Rooms_Destroy(rooms, room_count, room_id);
    } else {
        /* Broadcast updated room_peers to remaining members. */
        BroadcastRoomPeers(room_id, users, user_count);
    }
}

/* ---- chat --------------------------------------------------------- */
static void HandleChat(cJSON *root, User *sender,
                        User users[], int user_count)
{
    if (sender->room_id == -1) {
        SendError(sender->fd, "not in a room");
        return;
    }

    cJSON *j_msg = cJSON_GetObjectItem(root, "message");
    if (!cJSON_IsString(j_msg)) {
        SendError(sender->fd, "missing message");
        return;
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "type", MSG_CHAT_MSG);
    cJSON_AddStringToObject(resp, "from", sender->username);
    cJSON_AddStringToObject(resp, "message", j_msg->valuestring);

    char *s = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);

    if (s) {
        for (int i = 0; i < user_count; i++) {
            if (users[i].fd != -1 &&
                users[i].room_id == sender->room_id)
            {
                SendToFd(users[i].fd, s);
            }
        }
        free(s);
    }
}

/* ---- heartbeat ---------------------------------------------------- */
static void HandleHeartbeat(User *sender)
{
    sender->last_heartbeat = time(NULL);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "type", MSG_HEARTBEAT_ACK);
    char *s = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    if (s) { SendToFd(sender->fd, s); free(s); }
}

/* ================================================================== */
/*  Public API                                                         */
/* ================================================================== */

void Handler_ProcessMessage(const char *json_str,
                            User *sender,
                            User users[], int user_count,
                            Room rooms[], int room_count)
{
    if (json_str == NULL || sender == NULL) return;

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        printf("[handler] failed to parse JSON from fd %d\n", sender->fd);
        return;
    }

    cJSON *j_type = cJSON_GetObjectItem(root, "type");
    if (!cJSON_IsString(j_type)) {
        printf("[handler] message missing 'type' from fd %d\n", sender->fd);
        cJSON_Delete(root);
        return;
    }

    const char *type = j_type->valuestring;

    if (strcmp(type, MSG_LOGIN) == 0) {
        HandleLogin(root, sender, users, user_count);
    }
    else if (strcmp(type, MSG_ROOM_LIST) == 0) {
        HandleRoomList(sender, users, user_count, rooms, room_count);
    }
    else if (strcmp(type, MSG_ROOM_CREATE) == 0) {
        HandleRoomCreate(root, sender, users, user_count, rooms, room_count);
    }
    else if (strcmp(type, MSG_ROOM_JOIN) == 0) {
        HandleRoomJoin(root, sender, users, user_count, rooms, room_count);
    }
    else if (strcmp(type, MSG_ROOM_LEAVE) == 0) {
        HandleRoomLeave(sender, users, user_count, rooms, room_count);
    }
    else if (strcmp(type, MSG_CHAT) == 0) {
        HandleChat(root, sender, users, user_count);
    }
    else if (strcmp(type, MSG_HEARTBEAT) == 0) {
        HandleHeartbeat(sender);
    }
    else {
        printf("[handler] unknown message type '%s' from fd %d\n",
               type, sender->fd);
        SendError(sender->fd, "unknown message type");
    }

    cJSON_Delete(root);
}
