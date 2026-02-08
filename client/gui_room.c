/*
 * gui_room.c – Room page for War3 Platform client.
 *
 * Shows the player list, chat area, and game-launch / leave controls.
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
#include "game_launcher.h"
#include "../common/message.h"
#include "../third_party/cJSON/cJSON.h"

/* ------------------------------------------------------------------ */
/*  Internal state                                                    */
/* ------------------------------------------------------------------ */

static HWND s_hwndPanel       = NULL;
static HWND s_lblRoomName     = NULL;
static HWND s_listPlayers     = NULL;   /* ListBox – player list      */
static HWND s_listChat        = NULL;   /* ListBox – chat messages    */
static HWND s_editChat        = NULL;   /* Edit    – chat input       */
static HWND s_btnSend         = NULL;
static HWND s_btnStartGame    = NULL;
static HWND s_btnLeave        = NULL;

static HFONT s_hFont          = NULL;

/* Peer IP cache for game launcher. */
#define MAX_PEERS 16
static char  s_peer_ips[MAX_PEERS][48];
static int   s_peer_count = 0;

/* ------------------------------------------------------------------ */
/*  Forward declarations                                              */
/* ------------------------------------------------------------------ */

static LRESULT CALLBACK RoomPanelProc(HWND, UINT, WPARAM, LPARAM);
static void OnSendClicked(void);
static void OnStartGameClicked(void);
static void OnLeaveClicked(void);
static void AppendChatW(const wchar_t *line);
static void AppendChatSystemW(const wchar_t *line);

/* ------------------------------------------------------------------ */
/*  RoomPage_Create                                                   */
/* ------------------------------------------------------------------ */

HWND RoomPage_Create(HWND hwndParent, HINSTANCE hInst)
{
    static const wchar_t *cls = L"War3Room";
    static BOOL registered = FALSE;
    if (!registered) {
        WNDCLASSEXW wc = {
            .cbSize        = sizeof(wc),
            .style         = CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc   = RoomPanelProc,
            .hInstance     = hInst,
            .hCursor       = LoadCursor(NULL, IDC_ARROW),
            .hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1),
            .lpszClassName = cls,
        };
        RegisterClassExW(&wc);
        registered = TRUE;
    }

    s_hwndPanel = CreateWindowExW(
        0, cls, NULL,
        WS_CHILD | WS_CLIPCHILDREN,
        0, 0, 0, 0,
        hwndParent, NULL, hInst, NULL);

    /* ── Room name label (top bar) ────────────────────────────────── */
    s_lblRoomName = CreateWindowExW(0, L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
        0, 0, 0, 0,
        s_hwndPanel, (HMENU)IDC_STATIC_ROOM_NAME, hInst, NULL);

    /* ── Player list (left side) ──────────────────────────────────── */
    s_listPlayers = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOINTEGRALHEIGHT |
        LBS_NOSEL,
        0, 0, 0, 0,
        s_hwndPanel, (HMENU)IDC_LIST_PLAYERS, hInst, NULL);

    /* ── Chat list (right side) ───────────────────────────────────── */
    s_listChat = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
        LBS_NOINTEGRALHEIGHT | LBS_NOSEL,
        0, 0, 0, 0,
        s_hwndPanel, (HMENU)IDC_LIST_CHAT, hInst, NULL);

    /* ── Chat input ───────────────────────────────────────────────── */
    s_editChat = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0,
        s_hwndPanel, (HMENU)IDC_EDIT_CHAT, hInst, NULL);

    /* L"发送" */
    s_btnSend = CreateWindowExW(0, L"BUTTON",
        L"\x53D1\x9001",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 0, 0,
        s_hwndPanel, (HMENU)IDC_BTN_SEND_CHAT, hInst, NULL);

    /* L"启动游戏" */
    s_btnStartGame = CreateWindowExW(0, L"BUTTON",
        L"\x542F\x52A8\x6E38\x620F",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 0, 0,
        s_hwndPanel, (HMENU)IDC_BTN_START_GAME, hInst, NULL);

    /* L"离开房间" */
    s_btnLeave = CreateWindowExW(0, L"BUTTON",
        L"\x79BB\x5F00\x623F\x95F4",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 0, 0,
        s_hwndPanel, (HMENU)IDC_BTN_LEAVE_ROOM, hInst, NULL);

    /* ── Font ─────────────────────────────────────────────────────── */
    s_hFont = CreateFontW(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"\x5FAE\x8F6F\x96C5\x9ED1");  /* L"微软雅黑" */
    if (!s_hFont)
        s_hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    HWND ctrls[] = { s_lblRoomName, s_listPlayers, s_listChat,
                     s_editChat, s_btnSend, s_btnStartGame, s_btnLeave };
    for (int i = 0; i < (int)(sizeof(ctrls)/sizeof(ctrls[0])); i++)
        SendMessage(ctrls[i], WM_SETFONT, (WPARAM)s_hFont, TRUE);

    /* Use a slightly larger bold font for the room name label. */
    HFONT hBold = CreateFontW(
        -18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"\x5FAE\x8F6F\x96C5\x9ED1");
    if (hBold)
        SendMessage(s_lblRoomName, WM_SETFONT, (WPARAM)hBold, TRUE);

    return s_hwndPanel;
}

/* ------------------------------------------------------------------ */
/*  RoomPage_OnSize                                                   */
/* ------------------------------------------------------------------ */

void RoomPage_OnSize(int cx, int cy)
{
    if (!s_hwndPanel) return;

    int margin    = 12;
    int top_h     = 32;       /* room name bar height */
    int btn_h     = 30;
    int edit_h    = 26;
    int gap       = 6;

    /* Top bar: room name */
    MoveWindow(s_lblRoomName, margin, margin, cx - 2 * margin, top_h, TRUE);

    int content_y = margin + top_h + gap;
    int bottom_row_h = btn_h;
    int input_row_h  = edit_h;
    int content_h = cy - content_y - margin - bottom_row_h - gap - input_row_h - gap;

    /* Left 1/3: player list */
    int left_w = (cx - 2 * margin - gap) / 3;
    MoveWindow(s_listPlayers, margin, content_y, left_w, content_h, TRUE);

    /* Right 2/3: chat list */
    int right_x = margin + left_w + gap;
    int right_w = cx - right_x - margin;
    MoveWindow(s_listChat, right_x, content_y, right_w, content_h, TRUE);

    /* Chat input row (below chat list) */
    int input_y = content_y + content_h + gap;
    int btn_send_w = 70;
    MoveWindow(s_editChat, right_x, input_y,
               right_w - btn_send_w - gap, edit_h, TRUE);
    MoveWindow(s_btnSend, right_x + right_w - btn_send_w, input_y,
               btn_send_w, edit_h, TRUE);

    /* Bottom button row */
    int btn_y = input_y + input_row_h + gap;
    int btn_w = 100;
    int x = margin;
    MoveWindow(s_btnStartGame, x, btn_y, btn_w, btn_h, TRUE);
    x += btn_w + 10;
    MoveWindow(s_btnLeave, x, btn_y, btn_w, btn_h, TRUE);
}

/* ------------------------------------------------------------------ */
/*  RoomPage_ClearAll                                                 */
/* ------------------------------------------------------------------ */

void RoomPage_ClearAll(void)
{
    if (s_listPlayers)
        SendMessageW(s_listPlayers, LB_RESETCONTENT, 0, 0);
    if (s_listChat)
        SendMessageW(s_listChat, LB_RESETCONTENT, 0, 0);
    if (s_editChat)
        SetWindowTextW(s_editChat, L"");

    s_peer_count = 0;
    memset(s_peer_ips, 0, sizeof(s_peer_ips));

    /* Update room name label. */
    if (s_lblRoomName) {
        wchar_t wname[128] = {0};
        /* L"房间: " prefix */
        wchar_t prefix[] = L"\x623F\x95F4\xFF1A ";
        wcscpy(wname, prefix);
        if (g_app.current_room_name[0]) {
            wchar_t wrn[64] = {0};
            MultiByteToWideChar(CP_UTF8, 0, g_app.current_room_name, -1,
                                wrn, 64);
            wcscat(wname, wrn);
        }
        SetWindowTextW(s_lblRoomName, wname);
    }
}

/* ------------------------------------------------------------------ */
/*  RoomPage_HandleMessage                                            */
/* ------------------------------------------------------------------ */

void RoomPage_HandleMessage(const char *type, void *json_root)
{
    cJSON *root = (cJSON *)json_root;

    /* ── room_peers: full player list ─────────────────────────────── */
    if (strcmp(type, MSG_ROOM_PEERS) == 0) {
        SendMessageW(s_listPlayers, LB_RESETCONTENT, 0, 0);
        s_peer_count = 0;

        cJSON *peers = cJSON_GetObjectItem(root, "peers");
        if (!peers || !cJSON_IsArray(peers)) return;

        cJSON *peer = NULL;
        cJSON_ArrayForEach(peer, peers) {
            cJSON *juser = cJSON_GetObjectItem(peer, "username");
            cJSON *jip   = cJSON_GetObjectItem(peer, "ip");

            const char *uname = (juser && cJSON_IsString(juser))
                                ? juser->valuestring : "???";
            const char *ip    = (jip && cJSON_IsString(jip))
                                ? jip->valuestring : "";

            /* Add to player listbox. */
            wchar_t wline[128] = {0};
            wchar_t wuser[64] = {0};
            MultiByteToWideChar(CP_UTF8, 0, uname, -1, wuser, 64);
            wsprintfW(wline, L"%s", wuser);
            SendMessageW(s_listPlayers, LB_ADDSTRING, 0, (LPARAM)wline);

            /* Cache peer IP (skip self). */
            if (strcmp(uname, g_app.username) != 0 &&
                s_peer_count < MAX_PEERS && strlen(ip) > 0) {
                strncpy(s_peer_ips[s_peer_count], ip,
                        sizeof(s_peer_ips[0]) - 1);
                s_peer_count++;
            }
        }
    }
    /* ── chat_msg ─────────────────────────────────────────────────── */
    else if (strcmp(type, MSG_CHAT_MSG) == 0) {
        cJSON *jfrom = cJSON_GetObjectItem(root, "from");
        cJSON *jmsg  = cJSON_GetObjectItem(root, "message");

        const char *from = (jfrom && cJSON_IsString(jfrom))
                           ? jfrom->valuestring : "???";
        const char *text = (jmsg && cJSON_IsString(jmsg))
                           ? jmsg->valuestring : "";

        /* Build "from: text" wide string. */
        char line[512];
        snprintf(line, sizeof(line), "%s: %s", from, text);
        wchar_t wline[512] = {0};
        MultiByteToWideChar(CP_UTF8, 0, line, -1, wline, 512);
        AppendChatW(wline);
    }
    /* ── player_joined ────────────────────────────────────────────── */
    else if (strcmp(type, MSG_PLAYER_JOINED) == 0) {
        cJSON *juser = cJSON_GetObjectItem(root, "username");
        const char *uname = (juser && cJSON_IsString(juser))
                            ? juser->valuestring : "???";

        /* System message: "xxx 加入了房间" */
        char line[256];
        snprintf(line, sizeof(line), "*** %s \xe5\x8a\xa0\xe5\x85\xa5"
                 "\xe4\xba\x86\xe6\x88\xbf\xe9\x97\xb4 ***", uname);
        wchar_t wline[256] = {0};
        MultiByteToWideChar(CP_UTF8, 0, line, -1, wline, 256);
        AppendChatSystemW(wline);
    }
    /* ── player_left ──────────────────────────────────────────────── */
    else if (strcmp(type, MSG_PLAYER_LEFT) == 0) {
        cJSON *juser = cJSON_GetObjectItem(root, "username");
        const char *uname = (juser && cJSON_IsString(juser))
                            ? juser->valuestring : "???";

        /* System message: "xxx 离开了房间" */
        char line[256];
        snprintf(line, sizeof(line), "*** %s \xe7\xa6\xbb\xe5\xbc\x80"
                 "\xe4\xba\x86\xe6\x88\xbf\xe9\x97\xb4 ***", uname);
        wchar_t wline[256] = {0};
        MultiByteToWideChar(CP_UTF8, 0, line, -1, wline, 256);
        AppendChatSystemW(wline);
    }
    /* ── room_left (self left the room) ───────────────────────────── */
    else if (strcmp(type, MSG_ROOM_LEFT) == 0) {
        g_app.current_room_id = 0;
        g_app.current_room_name[0] = '\0';
        GUI_SwitchPage(PAGE_LOBBY);
        LobbyPage_RequestRoomList();
    }
}

/* ------------------------------------------------------------------ */
/*  Chat helpers                                                      */
/* ------------------------------------------------------------------ */

static void AppendChatW(const wchar_t *line)
{
    SendMessageW(s_listChat, LB_ADDSTRING, 0, (LPARAM)line);
    /* Auto-scroll to bottom. */
    int count = (int)SendMessageW(s_listChat, LB_GETCOUNT, 0, 0);
    if (count > 0)
        SendMessageW(s_listChat, LB_SETTOPINDEX, count - 1, 0);
}

static void AppendChatSystemW(const wchar_t *line)
{
    /* System messages are displayed the same way, just with *** markers. */
    AppendChatW(line);
}

/* ------------------------------------------------------------------ */
/*  Button handlers                                                   */
/* ------------------------------------------------------------------ */

static void OnSendClicked(void)
{
    wchar_t wtext[512] = {0};
    GetWindowTextW(s_editChat, wtext, 512);

    char text[512] = {0};
    WideCharToMultiByte(CP_UTF8, 0, wtext, -1, text, sizeof(text),
                        NULL, NULL);

    if (strlen(text) == 0) return;

    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "type", MSG_CHAT);
    cJSON_AddStringToObject(msg, "message", text);
    char *str = cJSON_PrintUnformatted(msg);
    if (str) {
        NetClient_Send(str);
        free(str);
    }
    cJSON_Delete(msg);

    SetWindowTextW(s_editChat, L"");
    SetFocus(s_editChat);
}

static void OnStartGameClicked(void)
{
    if (s_peer_count == 0) {
        /* L"房间内没有其他玩家" */
        MessageBoxW(g_app.hwndMain,
                    L"\x623F\x95F4\x5185\x6CA1\x6709\x5176\x4ED6"
                    L"\x73A9\x5BB6",
                    L"\x63D0\x793A", MB_OK | MB_ICONWARNING);
        return;
    }

    const char *ips[MAX_PEERS];
    for (int i = 0; i < s_peer_count; i++)
        ips[i] = s_peer_ips[i];

    const char *war3 = (g_app.war3_path[0] != '\0')
                       ? g_app.war3_path : NULL;

    if (!GameLauncher_Start(ips, s_peer_count, war3)) {
        /* L"启动游戏失败，请检查 war3.exe 和 war3hook.dll 是否存在" */
        MessageBoxW(g_app.hwndMain,
                    L"\x542F\x52A8\x6E38\x620F\x5931\x8D25\xFF0C"
                    L"\x8BF7\x68C0\x67E5 war3.exe \x548C "
                    L"war3hook.dll \x662F\x5426\x5B58\x5728",
                    L"\x9519\x8BEF", MB_OK | MB_ICONERROR);
    } else {
        /* L"游戏已启动！" */
        AppendChatW(L"*** \x6E38\x620F\x5DF2\x542F\x52A8\xFF01 ***");
    }
}

static void OnLeaveClicked(void)
{
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "type", MSG_ROOM_LEAVE);
    char *str = cJSON_PrintUnformatted(msg);
    if (str) {
        NetClient_Send(str);
        free(str);
    }
    cJSON_Delete(msg);
}

/* ------------------------------------------------------------------ */
/*  Panel window procedure                                            */
/* ------------------------------------------------------------------ */

static LRESULT CALLBACK RoomPanelProc(HWND hwnd, UINT msg,
                                       WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BTN_SEND_CHAT:
            if (HIWORD(wParam) == BN_CLICKED) { OnSendClicked(); return 0; }
            break;
        case IDC_BTN_START_GAME:
            if (HIWORD(wParam) == BN_CLICKED) { OnStartGameClicked(); return 0; }
            break;
        case IDC_BTN_LEAVE_ROOM:
            if (HIWORD(wParam) == BN_CLICKED) { OnLeaveClicked(); return 0; }
            break;
        case IDC_EDIT_CHAT:
            /* Allow Enter key to send chat. */
            break;
        }
        break;

    /* Handle Enter key in the chat edit box. */
    case WM_KEYDOWN:
        if (wParam == VK_RETURN && GetFocus() == s_editChat) {
            OnSendClicked();
            return 0;
        }
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
