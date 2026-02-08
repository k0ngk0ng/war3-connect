/*
 * gui_login.c – Login page for War3 Platform client.
 *
 * Provides server address and username fields with a login button.
 * On successful login the page switches to the lobby view.
 */

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gui.h"
#include "resource.h"
#include "net_client.h"
#include "../common/message.h"
#include "../common/protocol.h"
#include "../third_party/cJSON/cJSON.h"

/* ------------------------------------------------------------------ */
/*  Control handles                                                   */
/* ------------------------------------------------------------------ */

static HWND s_hwndPanel      = NULL;
static HWND s_lblServer      = NULL;
static HWND s_editServer     = NULL;
static HWND s_lblUsername    = NULL;
static HWND s_editUsername   = NULL;
static HWND s_btnLogin       = NULL;

/* ------------------------------------------------------------------ */
/*  Forward declarations                                              */
/* ------------------------------------------------------------------ */

static LRESULT CALLBACK LoginPanelProc(HWND, UINT, WPARAM, LPARAM);
static void OnLoginClicked(void);

/* ------------------------------------------------------------------ */
/*  LoginPage_Create                                                  */
/* ------------------------------------------------------------------ */

HWND LoginPage_Create(HWND hwndParent, HINSTANCE hInst)
{
    /* Register a child window class for this panel. */
    static const wchar_t *cls = L"War3Login";
    static BOOL registered = FALSE;
    if (!registered) {
        WNDCLASSEXW wc = {
            .cbSize        = sizeof(wc),
            .style         = CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc   = LoginPanelProc,
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

    /* ── Controls ─────────────────────────────────────────────────── */

    /* L"服务器地址:" */
    s_lblServer = CreateWindowExW(0, L"STATIC",
        L"\x670D\x52A1\x5668\x5730\x5740\xFF1A",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        0, 0, 0, 0,
        s_hwndPanel, (HMENU)IDC_STATIC, hInst, NULL);

    s_editServer = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
        L"127.0.0.1:12000",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0,
        s_hwndPanel, (HMENU)IDC_EDIT_SERVER, hInst, NULL);

    /* L"用户名:" */
    s_lblUsername = CreateWindowExW(0, L"STATIC",
        L"\x7528\x6237\x540D\xFF1A",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        0, 0, 0, 0,
        s_hwndPanel, (HMENU)IDC_STATIC, hInst, NULL);

    s_editUsername = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0,
        s_hwndPanel, (HMENU)IDC_EDIT_USERNAME, hInst, NULL);

    /* L"登录" */
    s_btnLogin = CreateWindowExW(0, L"BUTTON",
        L"\x767B\x5F55",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        0, 0, 0, 0,
        s_hwndPanel, (HMENU)IDC_BTN_LOGIN, hInst, NULL);

    /* Set a nicer font for all controls. */
    HFONT hFont = CreateFontW(
        -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"\x5FAE\x8F6F\x96C5\x9ED1");  /* L"微软雅黑" */
    if (!hFont) {
        hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    }
    SendMessage(s_lblServer,    WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(s_editServer,   WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(s_lblUsername,  WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(s_editUsername, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(s_btnLogin,     WM_SETFONT, (WPARAM)hFont, TRUE);

    return s_hwndPanel;
}

/* ------------------------------------------------------------------ */
/*  LoginPage_OnSize – centre controls in the panel                   */
/* ------------------------------------------------------------------ */

void LoginPage_OnSize(int cx, int cy)
{
    if (!s_hwndPanel) return;

    /* Layout constants */
    int lbl_w   = 110;
    int edit_w  = 200;
    int row_h   = 28;
    int gap     = 14;
    int btn_w   = 100;
    int btn_h   = 34;

    int total_h = row_h + gap + row_h + gap + btn_h;
    int startY  = (cy - total_h) / 2;
    int startX  = (cx - lbl_w - edit_w) / 2;

    int y = startY;
    MoveWindow(s_lblServer,   startX, y + 2, lbl_w, row_h, TRUE);
    MoveWindow(s_editServer,  startX + lbl_w + 6, y, edit_w, row_h, TRUE);

    y += row_h + gap;
    MoveWindow(s_lblUsername, startX, y + 2, lbl_w, row_h, TRUE);
    MoveWindow(s_editUsername, startX + lbl_w + 6, y, edit_w, row_h, TRUE);

    y += row_h + gap;
    int btnX = startX + lbl_w + 6 + (edit_w - btn_w) / 2;
    MoveWindow(s_btnLogin, btnX, y, btn_w, btn_h, TRUE);
}

/* ------------------------------------------------------------------ */
/*  LoginPage_HandleMessage                                           */
/* ------------------------------------------------------------------ */

void LoginPage_HandleMessage(const char *type, void *json_root)
{
    cJSON *root = (cJSON *)json_root;

    if (strcmp(type, MSG_LOGIN_OK) == 0) {
        /* Extract user_id and username. */
        cJSON *jid   = cJSON_GetObjectItem(root, "user_id");
        cJSON *jname = cJSON_GetObjectItem(root, "username");

        if (jid && cJSON_IsNumber(jid))
            g_app.user_id = jid->valueint;
        if (jname && cJSON_IsString(jname))
            strncpy(g_app.username, jname->valuestring,
                    sizeof(g_app.username) - 1);

        /* Start heartbeat timer. */
        SetTimer(g_app.hwndMain, IDT_HEARTBEAT, HEARTBEAT_INTERVAL_MS, NULL);

        /* Switch to lobby and request room list. */
        GUI_SwitchPage(PAGE_LOBBY);
        LobbyPage_RequestRoomList();
    }
    else if (strcmp(type, MSG_LOGIN_FAIL) == 0) {
        cJSON *jreason = cJSON_GetObjectItem(root, "reason");
        const char *reason = (jreason && cJSON_IsString(jreason))
                             ? jreason->valuestring
                             : "Unknown error";

        int wlen = MultiByteToWideChar(CP_UTF8, 0, reason, -1, NULL, 0);
        wchar_t *wreason = (wchar_t *)malloc(wlen * sizeof(wchar_t));
        if (wreason) {
            MultiByteToWideChar(CP_UTF8, 0, reason, -1, wreason, wlen);
            /* L"登录失败" */
            MessageBoxW(g_app.hwndMain, wreason,
                        L"\x767B\x5F55\x5931\x8D25",
                        MB_OK | MB_ICONERROR);
            free(wreason);
        }

        /* Re-enable the login button. */
        EnableWindow(s_btnLogin, TRUE);
    }
}

/* ------------------------------------------------------------------ */
/*  OnLoginClicked                                                    */
/* ------------------------------------------------------------------ */

static void OnLoginClicked(void)
{
    /* Read server address. */
    wchar_t wserver[256] = {0};
    GetWindowTextW(s_editServer, wserver, 256);
    char server[256] = {0};
    WideCharToMultiByte(CP_UTF8, 0, wserver, -1, server, sizeof(server),
                        NULL, NULL);

    /* Read username. */
    wchar_t wuser[64] = {0};
    GetWindowTextW(s_editUsername, wuser, 64);
    char user[64] = {0};
    WideCharToMultiByte(CP_UTF8, 0, wuser, -1, user, sizeof(user),
                        NULL, NULL);

    /* Validate. */
    if (strlen(user) == 0) {
        /* L"请输入用户名" */
        MessageBoxW(g_app.hwndMain,
                    L"\x8BF7\x8F93\x5165\x7528\x6237\x540D",
                    L"\x63D0\x793A", MB_OK | MB_ICONWARNING);
        return;
    }

    /* Parse server:port. */
    char ip[256] = {0};
    int port = DEFAULT_SERVER_PORT;
    {
        char tmp[256];
        strncpy(tmp, server, sizeof(tmp) - 1);
        char *colon = strrchr(tmp, ':');
        if (colon) {
            *colon = '\0';
            port = atoi(colon + 1);
            if (port <= 0 || port > 65535)
                port = DEFAULT_SERVER_PORT;
        }
        strncpy(ip, tmp, sizeof(ip) - 1);
    }

    if (strlen(ip) == 0) {
        strncpy(ip, "127.0.0.1", sizeof(ip) - 1);
    }

    /* Disable button while connecting. */
    EnableWindow(s_btnLogin, FALSE);

    /* Connect to server. */
    if (!NetClient_Connect(ip, port, g_app.hwndMain)) {
        /* L"无法连接到服务器" */
        MessageBoxW(g_app.hwndMain,
                    L"\x65E0\x6CD5\x8FDE\x63A5\x5230\x670D\x52A1\x5668",
                    L"\x9519\x8BEF", MB_OK | MB_ICONERROR);
        EnableWindow(s_btnLogin, TRUE);
        return;
    }

    /* Store server address. */
    strncpy(g_app.server_addr, server, sizeof(g_app.server_addr) - 1);
    strncpy(g_app.username, user, sizeof(g_app.username) - 1);

    /* Send login message. */
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "type", MSG_LOGIN);
    cJSON_AddStringToObject(msg, "username", user);
    char *json_str = cJSON_PrintUnformatted(msg);
    if (json_str) {
        NetClient_Send(json_str);
        free(json_str);
    }
    cJSON_Delete(msg);
}

/* ------------------------------------------------------------------ */
/*  Panel window procedure                                            */
/* ------------------------------------------------------------------ */

static LRESULT CALLBACK LoginPanelProc(HWND hwnd, UINT msg,
                                        WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_BTN_LOGIN && HIWORD(wParam) == BN_CLICKED) {
            OnLoginClicked();
            return 0;
        }
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
