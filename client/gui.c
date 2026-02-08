/*
 * gui.c – Main GUI framework implementation.
 *
 * Registers the window class, creates the top-level window and the three
 * child panels (login / lobby / room), and dispatches network messages
 * to the appropriate page handler.
 */

#define CJSON_HIDE_SYMBOLS
#include <winsock2.h>
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gui.h"
#include "resource.h"
#include "net_client.h"
#include "../common/message.h"
#include "../third_party/cJSON/cJSON.h"

/* ------------------------------------------------------------------ */
/*  Globals                                                           */
/* ------------------------------------------------------------------ */

AppState g_app;

static const wchar_t *CLASS_NAME = L"War3Platform";
static const wchar_t *WINDOW_TITLE = L"War3 \x5BF9\x6218\x5E73\x53F0";
/* L"War3 对战平台" encoded as escape sequences for portability */

#define WINDOW_W  800
#define WINDOW_H  600
#define HEARTBEAT_INTERVAL_MS  15000

/* ------------------------------------------------------------------ */
/*  Forward declarations                                              */
/* ------------------------------------------------------------------ */

static LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);
static void SendHeartbeat(void);

/* ------------------------------------------------------------------ */
/*  GUI_Init                                                          */
/* ------------------------------------------------------------------ */

BOOL GUI_Init(HINSTANCE hInstance)
{
    memset(&g_app, 0, sizeof(g_app));
    g_app.hInstance = hInstance;

    /* Initialise Common Controls (for ListView etc.). */
    INITCOMMONCONTROLSEX icc = {
        .dwSize = sizeof(icc),
        .dwICC  = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES
    };
    InitCommonControlsEx(&icc);

    /* Register window class. */
    WNDCLASSEXW wc = {
        .cbSize        = sizeof(wc),
        .style         = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc   = MainWndProc,
        .hInstance     = hInstance,
        .hCursor       = LoadCursor(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1),
        .lpszClassName = CLASS_NAME,
        .hIcon         = LoadIcon(NULL, IDI_APPLICATION),
        .hIconSm       = LoadIcon(NULL, IDI_APPLICATION),
    };

    if (!RegisterClassExW(&wc))
        return FALSE;

    /* Centre the window on screen. */
    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);
    int x  = (sx - WINDOW_W) / 2;
    int y  = (sy - WINDOW_H) / 2;

    g_app.hwndMain = CreateWindowExW(
        0, CLASS_NAME, WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        x, y, WINDOW_W, WINDOW_H,
        NULL, NULL, hInstance, NULL);

    if (!g_app.hwndMain)
        return FALSE;

    /* Create the three page panels. */
    g_app.hwndLogin = LoginPage_Create(g_app.hwndMain, hInstance);
    g_app.hwndLobby = LobbyPage_Create(g_app.hwndMain, hInstance);
    g_app.hwndRoom  = RoomPage_Create(g_app.hwndMain, hInstance);

    /* Start on the login page. */
    GUI_SwitchPage(PAGE_LOGIN);

    ShowWindow(g_app.hwndMain, SW_SHOW);
    UpdateWindow(g_app.hwndMain);

    return TRUE;
}

/* ------------------------------------------------------------------ */
/*  GUI_SwitchPage                                                    */
/* ------------------------------------------------------------------ */

void GUI_SwitchPage(PageId page)
{
    ShowWindow(g_app.hwndLogin, (page == PAGE_LOGIN) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_app.hwndLobby, (page == PAGE_LOBBY) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_app.hwndRoom,  (page == PAGE_ROOM)  ? SW_SHOW : SW_HIDE);
}

/* ------------------------------------------------------------------ */
/*  GUI_Run – message loop                                            */
/* ------------------------------------------------------------------ */

void GUI_Run(void)
{
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

/* ------------------------------------------------------------------ */
/*  GUI_HandleNetworkMessage                                          */
/* ------------------------------------------------------------------ */

void GUI_HandleNetworkMessage(const char *json_str)
{
    if (!json_str) return;

    cJSON *root = cJSON_Parse(json_str);
    if (!root) return;

    cJSON *jtype = cJSON_GetObjectItem(root, "type");
    if (!jtype || !cJSON_IsString(jtype)) {
        cJSON_Delete(root);
        return;
    }

    const char *type = jtype->valuestring;

    /* Dispatch to the appropriate page handler. */
    /* Login responses */
    if (strcmp(type, MSG_LOGIN_OK) == 0 ||
        strcmp(type, MSG_LOGIN_FAIL) == 0) {
        LoginPage_HandleMessage(type, root);
    }
    /* Lobby responses */
    else if (strcmp(type, MSG_ROOM_LIST_RES) == 0 ||
             strcmp(type, MSG_ROOM_CREATED) == 0 ||
             strcmp(type, MSG_ROOM_JOINED) == 0) {
        LobbyPage_HandleMessage(type, root);
    }
    /* Room responses */
    else if (strcmp(type, MSG_ROOM_PEERS) == 0 ||
             strcmp(type, MSG_CHAT_MSG) == 0 ||
             strcmp(type, MSG_PLAYER_JOINED) == 0 ||
             strcmp(type, MSG_PLAYER_LEFT) == 0 ||
             strcmp(type, MSG_ROOM_LEFT) == 0) {
        RoomPage_HandleMessage(type, root);
    }
    /* Error – show a message box */
    else if (strcmp(type, MSG_ERROR) == 0) {
        cJSON *jmsg = cJSON_GetObjectItem(root, "message");
        if (jmsg && cJSON_IsString(jmsg)) {
            /* Convert UTF-8 to wide string for MessageBoxW. */
            int wlen = MultiByteToWideChar(CP_UTF8, 0,
                           jmsg->valuestring, -1, NULL, 0);
            if (wlen > 0) {
                wchar_t *wmsg = (wchar_t *)malloc(wlen * sizeof(wchar_t));
                if (wmsg) {
                    MultiByteToWideChar(CP_UTF8, 0,
                        jmsg->valuestring, -1, wmsg, wlen);
                    MessageBoxW(g_app.hwndMain, wmsg,
                                L"\x9519\x8BEF", MB_OK | MB_ICONERROR);
                    /* L"错误" */
                    free(wmsg);
                }
            }
        }
    }
    /* heartbeat_ack – silently ignore */

    cJSON_Delete(root);
}

/* ------------------------------------------------------------------ */
/*  GUI_HandleDisconnect                                              */
/* ------------------------------------------------------------------ */

void GUI_HandleDisconnect(void)
{
    KillTimer(g_app.hwndMain, IDT_HEARTBEAT);
    NetClient_Disconnect();

    MessageBoxW(g_app.hwndMain,
                L"\x4E0E\x670D\x52A1\x5668\x65AD\x5F00\x8FDE\x63A5",
                /* L"与服务器断开连接" */
                L"\x63D0\x793A",  /* L"提示" */
                MB_OK | MB_ICONWARNING);

    g_app.user_id = 0;
    g_app.current_room_id = 0;
    g_app.current_room_name[0] = '\0';
    GUI_SwitchPage(PAGE_LOGIN);
}

/* ------------------------------------------------------------------ */
/*  Heartbeat                                                         */
/* ------------------------------------------------------------------ */

static void SendHeartbeat(void)
{
    if (!NetClient_IsConnected()) return;

    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "type", MSG_HEARTBEAT);
    char *str = cJSON_PrintUnformatted(msg);
    if (str) {
        NetClient_Send(str);
        free(str);
    }
    cJSON_Delete(msg);
}

/* ------------------------------------------------------------------ */
/*  Main window procedure                                             */
/* ------------------------------------------------------------------ */

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg,
                                     WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_SIZE: {
        int cx = LOWORD(lParam);
        int cy = HIWORD(lParam);
        /* Resize all page panels to fill the client area. */
        if (g_app.hwndLogin)
            MoveWindow(g_app.hwndLogin, 0, 0, cx, cy, TRUE);
        if (g_app.hwndLobby)
            MoveWindow(g_app.hwndLobby, 0, 0, cx, cy, TRUE);
        if (g_app.hwndRoom)
            MoveWindow(g_app.hwndRoom, 0, 0, cx, cy, TRUE);
        /* Let each page reposition its own controls. */
        LoginPage_OnSize(cx, cy);
        LobbyPage_OnSize(cx, cy);
        RoomPage_OnSize(cx, cy);
        return 0;
    }

    case WM_NETWORK_MSG: {
        char *json_str = (char *)lParam;
        if (json_str) {
            GUI_HandleNetworkMessage(json_str);
            free(json_str);
        }
        return 0;
    }

    case WM_NET_DISCONNECTED:
        GUI_HandleDisconnect();
        return 0;

    case WM_TIMER:
        if (wParam == IDT_HEARTBEAT) {
            SendHeartbeat();
            return 0;
        }
        break;

    case WM_COMMAND:
        /* Forward WM_COMMAND to the visible page's parent (this window).
         * Child controls already send WM_COMMAND to their parent, which
         * is the page panel.  The page panels forward unhandled commands
         * here via DefWindowProc, but we also handle some directly. */
        break;

    case WM_DESTROY:
        KillTimer(hwnd, IDT_HEARTBEAT);
        NetClient_Disconnect();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
