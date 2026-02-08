/*
 * net_client.c – Network client implementation.
 *
 * Creates a TCP connection to the lobby server and spawns a background
 * thread that reads length-prefixed JSON frames (via Protocol_Extract)
 * and posts them to the GUI thread with PostMessage.
 */

#include "net_client.h"
#include "resource.h"
#include "../common/protocol.h"

#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Internal state                                                    */
/* ------------------------------------------------------------------ */

static SOCKET           g_sock      = INVALID_SOCKET;
static HWND             g_hwnd      = NULL;
static HANDLE           g_thread    = NULL;
static volatile BOOL    g_running   = FALSE;
static CRITICAL_SECTION g_cs;
static BOOL             g_cs_init   = FALSE;

/* ------------------------------------------------------------------ */
/*  Receive thread                                                    */
/* ------------------------------------------------------------------ */

static DWORD WINAPI RecvThreadProc(LPVOID param)
{
    (void)param;

    uint8_t buf[MAX_MSG_SIZE * 2];
    uint32_t buf_len = 0;

    while (g_running) {
        /* Receive into the tail of the buffer. */
        int space = (int)(sizeof(buf) - buf_len);
        if (space <= 0) {
            /* Buffer overflow – discard everything (protocol error). */
            buf_len = 0;
            continue;
        }

        int n = recv(g_sock, (char *)(buf + buf_len), space, 0);
        if (n <= 0) {
            /* Connection closed or error. */
            break;
        }
        buf_len += (uint32_t)n;

        /* Extract as many complete frames as possible. */
        for (;;) {
            char *json = NULL;
            uint32_t consumed = Protocol_Extract(buf, buf_len, &json);
            if (consumed == 0)
                break;

            /* Post the JSON string to the GUI thread.
             * The GUI is responsible for calling free(). */
            if (json) {
                char *copy = _strdup(json);
                free(json);
                if (copy) {
                    if (!PostMessage(g_hwnd, WM_NETWORK_MSG, 0, (LPARAM)copy)) {
                        free(copy);
                    }
                }
            }

            /* Shift remaining data to the front. */
            if (consumed < buf_len) {
                memmove(buf, buf + consumed, buf_len - consumed);
            }
            buf_len -= consumed;
        }
    }

    g_running = FALSE;

    /* Notify the GUI that we disconnected. */
    if (g_hwnd) {
        PostMessage(g_hwnd, WM_NET_DISCONNECTED, 0, 0);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

BOOL NetClient_Connect(const char *server_ip, int port, HWND hwnd)
{
    if (!g_cs_init) {
        InitializeCriticalSection(&g_cs);
        g_cs_init = TRUE;
    }

    /* Disconnect any previous session. */
    NetClient_Disconnect();

    /* Create socket. */
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        return FALSE;
    }

    /* Resolve and connect. */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((u_short)port);

    /* Try inet_pton first (numeric), then fall back to getaddrinfo. */
    if (inet_pton(AF_INET, server_ip, &addr.sin_addr) != 1) {
        struct addrinfo hints, *res = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(server_ip, NULL, &hints, &res) != 0 || !res) {
            closesocket(s);
            return FALSE;
        }
        addr.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
        freeaddrinfo(res);
    }

    if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(s);
        return FALSE;
    }

    EnterCriticalSection(&g_cs);
    g_sock    = s;
    g_hwnd    = hwnd;
    g_running = TRUE;
    LeaveCriticalSection(&g_cs);

    /* Start receive thread. */
    g_thread = CreateThread(NULL, 0, RecvThreadProc, NULL, 0, NULL);
    if (!g_thread) {
        closesocket(s);
        EnterCriticalSection(&g_cs);
        g_sock    = INVALID_SOCKET;
        g_running = FALSE;
        LeaveCriticalSection(&g_cs);
        return FALSE;
    }

    return TRUE;
}

void NetClient_Disconnect(void)
{
    if (!g_cs_init) return;

    EnterCriticalSection(&g_cs);
    g_running = FALSE;
    if (g_sock != INVALID_SOCKET) {
        shutdown(g_sock, SD_BOTH);
        closesocket(g_sock);
        g_sock = INVALID_SOCKET;
    }
    LeaveCriticalSection(&g_cs);

    if (g_thread) {
        WaitForSingleObject(g_thread, 3000);
        CloseHandle(g_thread);
        g_thread = NULL;
    }
}

BOOL NetClient_Send(const char *json_str)
{
    if (!json_str) return FALSE;
    if (!g_cs_init) return FALSE;

    EnterCriticalSection(&g_cs);

    if (g_sock == INVALID_SOCKET || !g_running) {
        LeaveCriticalSection(&g_cs);
        return FALSE;
    }

    uint32_t frame_len = 0;
    uint8_t *frame = Protocol_Frame(json_str, &frame_len);
    if (!frame) {
        LeaveCriticalSection(&g_cs);
        return FALSE;
    }

    /* Send the entire frame. */
    uint32_t sent = 0;
    BOOL ok = TRUE;
    while (sent < frame_len) {
        int n = send(g_sock, (const char *)(frame + sent),
                     (int)(frame_len - sent), 0);
        if (n == SOCKET_ERROR) {
            ok = FALSE;
            break;
        }
        sent += (uint32_t)n;
    }

    free(frame);
    LeaveCriticalSection(&g_cs);
    return ok;
}

BOOL NetClient_IsConnected(void)
{
    return g_running && (g_sock != INVALID_SOCKET);
}
