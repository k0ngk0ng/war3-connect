/*
 * gui_lobby.c – Lobby page for War3 Platform client.
 *
 * Shows a list of rooms with create / join / refresh controls.
 */

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
/*  Control handles                                                   */
/* ------------------------------------------------------------------ */

static HWND s_hwndPanel       = NULL;
static HWND s_listRooms       = NULL;   /* ListView */
static HWND s_btnRefresh      = NULL;
static HWND s_lblRoomName     = NULL;
static HWND s_editRoomName    = NULL;
static HWND s_lblMaxPlayers   = NULL;
static HWND s_editMaxPlayers  = NULL;
static HWND s_btnCreate       = NULL;
static HWND s_btnJoin         = NULL;

static HFONT s_hFont          = NULL;

/* ------------------------------------------------------------------ */
/*  Forward declarations                                              */
/* ------------------------------------------------------------------ */

static LRESULT CALLBACK LobbyPanelProc(HWND, UINT, WPARAM, LPARAM);
static void OnRefreshClicked(void);
static void OnCreateClicked(void);
static void OnJoinClicked(void);

/* ------------------------------------------------------------------ */
/*  LobbyPage_Create                                                  */
/* ------------------------------------------------------------------ */

HWND LobbyPage_Create(HWND hwndParent, HINSTANCE hInst)
{
    static const wchar_t *cls = L"War3Lobby";
    static BOOL registered = FALSE;
    if (!registered) {
        WNDCLASSEXW wc = {
            .cbSize        = sizeof(wc),
            .style         = CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc   = LobbyPanelProc,
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

    /* ── ListView for rooms ───────────────────────────────────────── */
    s_listRooms = CreateWindowExW(WS_EX_CLIENTEDGE,
        WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL |
        LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
        0, 0, 0, 0,
        s_hwndPanel, (HMENU)IDC_LIST_ROOMS, hInst, NULL);

    ListView_SetExtendedListViewStyle(s_listRooms,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    /* Columns: 房间名 | 人数 | 最大人数 */
    LVCOLUMNW col = {0};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    col.fmt  = LVCFMT_LEFT;

    /* L"房间名" */
    col.pszText = L"\x623F\x95F4\x540D";
    col.cx      = 320;
    SendMessageW(s_listRooms, LVM_INSERTCOLUMNW, 0, (LPARAM)&col);

    /* L"人数" */
    col.pszText = L"\x4EBA\x6570";
    col.cx      = 80;
    col.fmt     = LVCFMT_CENTER;
    SendMessageW(s_listRooms, LVM_INSERTCOLUMNW, 1, (LPARAM)&col);

    /* L"最大人数" */
    col.pszText = L"\x6700\x5927\x4EBA\x6570";
    col.cx      = 80;
    SendMessageW(s_listRooms, LVM_INSERTCOLUMNW, 2, (LPARAM)&col);

    /* ── Bottom controls ──────────────────────────────────────────── */

    /* L"刷新" */
    s_btnRefresh = CreateWindowExW(0, L"BUTTON",
        L"\x5237\x65B0",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 0, 0,
        s_hwndPanel, (HMENU)IDC_BTN_REFRESH, hInst, NULL);

    /* L"房间名:" */
    s_lblRoomName = CreateWindowExW(0, L"STATIC",
        L"\x623F\x95F4\x540D\xFF1A",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        0, 0, 0, 0,
        s_hwndPanel, (HMENU)IDC_STATIC, hInst, NULL);

    s_editRoomName = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0,
        s_hwndPanel, (HMENU)IDC_EDIT_ROOM_NAME, hInst, NULL);

    /* L"最大人数:" */
    s_lblMaxPlayers = CreateWindowExW(0, L"STATIC",
        L"\x6700\x5927\x4EBA\x6570\xFF1A",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        0, 0, 0, 0,
        s_hwndPanel, (HMENU)IDC_STATIC, hInst, NULL);

    s_editMaxPlayers = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
        L"10",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER,
        0, 0, 0, 0,
        s_hwndPanel, (HMENU)IDC_EDIT_MAX_PLAYERS, hInst, NULL);

    /* L"创建房间" */
    s_btnCreate = CreateWindowExW(0, L"BUTTON",
        L"\x521B\x5EFA\x623F\x95F4",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 0, 0,
        s_hwndPanel, (HMENU)IDC_BTN_CREATE_ROOM, hInst, NULL);

    /* L"加入房间" */
    s_btnJoin = CreateWindowExW(0, L"BUTTON",
        L"\x52A0\x5165\x623F\x95F4",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 0, 0,
        s_hwndPanel, (HMENU)IDC_BTN_JOIN_ROOM, hInst, NULL);

    /* ── Font ─────────────────────────────────────────────────────── */
    s_hFont = CreateFontW(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"\x5FAE\x8F6F\x96C5\x9ED1");  /* L"微软雅黑" */
    if (!s_hFont)
        s_hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    HWND ctrls[] = { s_listRooms, s_btnRefresh, s_lblRoomName,
                     s_editRoomName, s_lblMaxPlayers, s_editMaxPlayers,
                     s_btnCreate, s_btnJoin };
    for (int i = 0; i < (int)(sizeof(ctrls)/sizeof(ctrls[0])); i++)
        SendMessage(ctrls[i], WM_SETFONT, (WPARAM)s_hFont, TRUE);

    return s_hwndPanel;
}

/* ------------------------------------------------------------------ */
/*  LobbyPage_OnSize                                                  */
/* ------------------------------------------------------------------ */

void LobbyPage_OnSize(int cx, int cy)
{
    if (!s_hwndPanel) return;

    int margin   = 12;
    int btn_h    = 30;
    int edit_h   = 26;
    int row2_y   = cy - margin - btn_h;
    int row1_y   = row2_y - 6 - btn_h;
    int list_h   = row1_y - margin - 6;

    /* Room list fills the top area. */
    MoveWindow(s_listRooms, margin, margin,
               cx - 2 * margin, list_h, TRUE);

    /* Row 1: room name + max players + create button */
    int x = margin;
    int lbl_w = 70;
    int edit_room_w = 180;
    int lbl_max_w = 80;
    int edit_max_w = 50;
    int btn_create_w = 90;

    MoveWindow(s_lblRoomName, x, row1_y + 3, lbl_w, edit_h, TRUE);
    x += lbl_w + 4;
    MoveWindow(s_editRoomName, x, row1_y, edit_room_w, edit_h, TRUE);
    x += edit_room_w + 10;
    MoveWindow(s_lblMaxPlayers, x, row1_y + 3, lbl_max_w, edit_h, TRUE);
    x += lbl_max_w + 4;
    MoveWindow(s_editMaxPlayers, x, row1_y, edit_max_w, edit_h, TRUE);
    x += edit_max_w + 10;
    MoveWindow(s_btnCreate, x, row1_y, btn_create_w, btn_h, TRUE);

    /* Row 2: refresh + join */
    int btn_w = 90;
    x = margin;
    MoveWindow(s_btnRefresh, x, row2_y, btn_w, btn_h, TRUE);
    x += btn_w + 10;
    MoveWindow(s_btnJoin, x, row2_y, btn_w, btn_h, TRUE);
}

/* ------------------------------------------------------------------ */
/*  LobbyPage_RequestRoomList                                         */
/* ------------------------------------------------------------------ */

void LobbyPage_RequestRoomList(void)
{
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "type", MSG_ROOM_LIST);
    char *str = cJSON_PrintUnformatted(msg);
    if (str) {
        NetClient_Send(str);
        free(str);
    }
    cJSON_Delete(msg);
}

/* ------------------------------------------------------------------ */
/*  LobbyPage_HandleMessage                                           */
/* ------------------------------------------------------------------ */

void LobbyPage_HandleMessage(const char *type, void *json_root)
{
    cJSON *root = (cJSON *)json_root;

    if (strcmp(type, MSG_ROOM_LIST_RES) == 0) {
        /* Clear the list. */
        ListView_DeleteAllItems(s_listRooms);

        cJSON *rooms = cJSON_GetObjectItem(root, "rooms");
        if (!rooms || !cJSON_IsArray(rooms)) return;

        int idx = 0;
        cJSON *room = NULL;
        cJSON_ArrayForEach(room, rooms) {
            cJSON *jid   = cJSON_GetObjectItem(room, "id");
            cJSON *jname = cJSON_GetObjectItem(room, "name");
            cJSON *jcnt  = cJSON_GetObjectItem(room, "players");
            cJSON *jmax  = cJSON_GetObjectItem(room, "max");

            /* Convert room name from UTF-8 to wide. */
            const char *name_utf8 = (jname && cJSON_IsString(jname))
                                    ? jname->valuestring : "???";
            wchar_t wname[128] = {0};
            MultiByteToWideChar(CP_UTF8, 0, name_utf8, -1, wname, 128);

            LVITEMW lvi = {0};
            lvi.mask    = LVIF_TEXT | LVIF_PARAM;
            lvi.iItem   = idx;
            lvi.pszText = wname;
            lvi.lParam  = (jid && cJSON_IsNumber(jid)) ? jid->valueint : 0;
            SendMessageW(s_listRooms, LVM_INSERTITEMW, 0, (LPARAM)&lvi);

            /* Player count column. */
            wchar_t wbuf[32];
            int cnt = (jcnt && cJSON_IsNumber(jcnt)) ? jcnt->valueint : 0;
            wsprintfW(wbuf, L"%d", cnt);
            lvi.mask    = LVIF_TEXT;
            lvi.iSubItem = 1;
            lvi.pszText  = wbuf;
            SendMessageW(s_listRooms, LVM_SETITEMW, 0, (LPARAM)&lvi);

            /* Max players column. */
            int maxp = (jmax && cJSON_IsNumber(jmax)) ? jmax->valueint : 0;
            wsprintfW(wbuf, L"%d", maxp);
            lvi.iSubItem = 2;
            lvi.pszText  = wbuf;
            SendMessageW(s_listRooms, LVM_SETITEMW, 0, (LPARAM)&lvi);

            idx++;
        }
    }
    else if (strcmp(type, MSG_ROOM_CREATED) == 0 ||
             strcmp(type, MSG_ROOM_JOINED) == 0) {
        /* Extract room info and switch to room page. */
        cJSON *jid   = cJSON_GetObjectItem(root, "room_id");
        cJSON *jname = cJSON_GetObjectItem(root, "name");

        if (jid && cJSON_IsNumber(jid))
            g_app.current_room_id = jid->valueint;
        if (jname && cJSON_IsString(jname))
            strncpy(g_app.current_room_name, jname->valuestring,
                    sizeof(g_app.current_room_name) - 1);

        RoomPage_ClearAll();
        GUI_SwitchPage(PAGE_ROOM);
    }
}

/* ------------------------------------------------------------------ */
/*  Button handlers                                                   */
/* ------------------------------------------------------------------ */

static void OnRefreshClicked(void)
{
    LobbyPage_RequestRoomList();
}

static void OnCreateClicked(void)
{
    /* Read room name. */
    wchar_t wname[128] = {0};
    GetWindowTextW(s_editRoomName, wname, 128);
    char name[128] = {0};
    WideCharToMultiByte(CP_UTF8, 0, wname, -1, name, sizeof(name),
                        NULL, NULL);

    if (strlen(name) == 0) {
        /* L"请输入房间名" */
        MessageBoxW(g_app.hwndMain,
                    L"\x8BF7\x8F93\x5165\x623F\x95F4\x540D",
                    L"\x63D0\x793A", MB_OK | MB_ICONWARNING);
        return;
    }

    /* Read max players. */
    wchar_t wmax[16] = {0};
    GetWindowTextW(s_editMaxPlayers, wmax, 16);
    int max_players = _wtoi(wmax);
    if (max_players <= 0 || max_players > MAX_ROOM_PLAYERS)
        max_players = 10;

    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "type", MSG_ROOM_CREATE);
    cJSON_AddStringToObject(msg, "name", name);
    cJSON_AddNumberToObject(msg, "max_players", max_players);
    char *str = cJSON_PrintUnformatted(msg);
    if (str) {
        NetClient_Send(str);
        free(str);
    }
    cJSON_Delete(msg);
}

static void OnJoinClicked(void)
{
    /* Get selected room. */
    int sel = ListView_GetNextItem(s_listRooms, -1, LVNI_SELECTED);
    if (sel < 0) {
        /* L"请先选择一个房间" */
        MessageBoxW(g_app.hwndMain,
                    L"\x8BF7\x5148\x9009\x62E9\x4E00\x4E2A\x623F\x95F4",
                    L"\x63D0\x793A", MB_OK | MB_ICONWARNING);
        return;
    }

    /* Get room_id from lParam. */
    LVITEMW lvi = {0};
    lvi.mask  = LVIF_PARAM;
    lvi.iItem = sel;
    SendMessageW(s_listRooms, LVM_GETITEMW, 0, (LPARAM)&lvi);
    int room_id = (int)lvi.lParam;

    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "type", MSG_ROOM_JOIN);
    cJSON_AddNumberToObject(msg, "room_id", room_id);
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

static LRESULT CALLBACK LobbyPanelProc(HWND hwnd, UINT msg,
                                        WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BTN_REFRESH:
            if (HIWORD(wParam) == BN_CLICKED) { OnRefreshClicked(); return 0; }
            break;
        case IDC_BTN_CREATE_ROOM:
            if (HIWORD(wParam) == BN_CLICKED) { OnCreateClicked(); return 0; }
            break;
        case IDC_BTN_JOIN_ROOM:
            if (HIWORD(wParam) == BN_CLICKED) { OnJoinClicked(); return 0; }
            break;
        }
        break;

    case WM_NOTIFY: {
        NMHDR *nm = (NMHDR *)lParam;
        if (nm->idFrom == IDC_LIST_ROOMS && nm->code == NM_DBLCLK) {
            OnJoinClicked();
            return 0;
        }
        break;
    }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
