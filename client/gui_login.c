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
#include "gui_style.h"
#include "resource.h"
#include "net_client.h"
#include "../common/message.h"
#include "../common/protocol.h"
#include "../third_party/cJSON/cJSON.h"

/* ------------------------------------------------------------------ */
/*  Control handles                                                   */
/* ------------------------------------------------------------------ */

static HWND s_hwndPanel      = NULL;
static HWND s_lblTitle       = NULL;
static HWND s_lblSubtitle    = NULL;
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
    static const wchar_t *cls = L"War3Login";
    static BOOL registered = FALSE;
    if (!registered) {
        WNDCLASSEXW wc = {
            .cbSize        = sizeof(wc),
            .style         = CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc   = LoginPanelProc,
            .hInstance     = hInst,
            .hCursor       = LoadCursor(NULL, IDC_ARROW),
            .hbrBackground = NULL,  /* We paint our own background */
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

    /* L"War3 对战平台" */
    s_lblTitle = CreateWindowExW(0, L"STATIC",
        L"War3 \x5BF9\x6218\x5E73\x53F0",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        0, 0, 0, 0,
        s_hwndPanel, (HMENU)IDC_STATIC, hInst, NULL);

    /* L"输入服务器地址和用户名开始联机" */
    s_lblSubtitle = CreateWindowExW(0, L"STATIC",
        L"\x8F93\x5165\x670D\x52A1\x5668\x5730\x5740"
        L"\x548C\x7528\x6237\x540D\x5F00\x59CB\x8054\x673A",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        0, 0, 0, 0,
        s_hwndPanel, (HMENU)IDC_STATIC, hInst, NULL);

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

    /* Apply theme fonts. */
    SendMessage(s_lblTitle,     WM_SETFONT, (WPARAM)g_theme.fontTitle, TRUE);
    SendMessage(s_lblSubtitle,  WM_SETFONT, (WPARAM)g_theme.fontSmall, TRUE);
    SendMessage(s_lblServer,    WM_SETFONT, (WPARAM)g_theme.fontBody, TRUE);
    SendMessage(s_editServer,   WM_SETFONT, (WPARAM)g_theme.fontBody, TRUE);
    SendMessage(s_lblUsername,  WM_SETFONT, (WPARAM)g_theme.fontBody, TRUE);
    SendMessage(s_editUsername, WM_SETFONT, (WPARAM)g_theme.fontBody, TRUE);
    SendMessage(s_btnLogin,     WM_SETFONT, (WPARAM)g_theme.fontBtn, TRUE);

    /* Owner-draw gold button. */
    Style_SetButton(s_btnLogin, BTN_STYLE_PRIMARY);

    return s_hwndPanel;
}

/* ------------------------------------------------------------------ */
/*  LoginPage_OnSize – centre controls in the panel                   */
/* ------------------------------------------------------------------ */

void LoginPage_OnSize(int cx, int cy)
{
    if (!s_hwndPanel) return;

    int lbl_w   = 110;
    int edit_w  = 220;
    int row_h   = 30;
    int gap     = 14;
    int btn_w   = 220;
    int btn_h   = 38;
    int title_h = 40;
    int sub_h   = 24;

    int card_w  = lbl_w + edit_w + 80;
    int card_h  = title_h + 8 + sub_h + 30 + row_h + gap + row_h + gap + btn_h + 40;
    int card_x  = (cx - card_w) / 2;
    int card_y  = (cy - card_h) / 2;

    int y = card_y + 24;
    MoveWindow(s_lblTitle, card_x, y, card_w, title_h, TRUE);
    y += title_h + 4;
    MoveWindow(s_lblSubtitle, card_x, y, card_w, sub_h, TRUE);
    y += sub_h + 24;

    int startX = (cx - lbl_w - edit_w - 6) / 2;

    MoveWindow(s_lblServer,   startX, y + 3, lbl_w, row_h, TRUE);
    MoveWindow(s_editServer,  startX + lbl_w + 6, y, edit_w, row_h, TRUE);

    y += row_h + gap;
    MoveWindow(s_lblUsername, startX, y + 3, lbl_w, row_h, TRUE);
    MoveWindow(s_editUsername, startX + lbl_w + 6, y, edit_w, row_h, TRUE);

    y += row_h + gap + 6;
    int btnX = (cx - btn_w) / 2;
    MoveWindow(s_btnLogin, btnX, y, btn_w, btn_h, TRUE);
}

/* ------------------------------------------------------------------ */
/*  LoginPage_HandleMessage                                           */
/* ------------------------------------------------------------------ */

void LoginPage_HandleMessage(const char *type, void *json_root)
{
    cJSON *root = (cJSON *)json_root;

    if (strcmp(type, MSG_LOGIN_OK) == 0) {
        cJSON *jid   = cJSON_GetObjectItem(root, "user_id");
        cJSON *jname = cJSON_GetObjectItem(root, "username");

        if (jid && cJSON_IsNumber(jid))
            g_app.user_id = jid->valueint;
        if (jname && cJSON_IsString(jname))
            strncpy(g_app.username, jname->valuestring,
                    sizeof(g_app.username) - 1);

        SetTimer(g_app.hwndMain, IDT_HEARTBEAT, HEARTBEAT_INTERVAL_MS, NULL);

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
            MessageBoxW(g_app.hwndMain, wreason,
                        L"\x767B\x5F55\x5931\x8D25",
                        MB_OK | MB_ICONERROR);
            free(wreason);
        }

        EnableWindow(s_btnLogin, TRUE);
    }
}

/* ------------------------------------------------------------------ */
/*  OnLoginClicked                                                    */
/* ------------------------------------------------------------------ */

static void OnLoginClicked(void)
{
    wchar_t wserver[256] = {0};
    GetWindowTextW(s_editServer, wserver, 256);
    char server[256] = {0};
    WideCharToMultiByte(CP_UTF8, 0, wserver, -1, server, sizeof(server),
                        NULL, NULL);

    wchar_t wuser[64] = {0};
    GetWindowTextW(s_editUsername, wuser, 64);
    char user[64] = {0};
    WideCharToMultiByte(CP_UTF8, 0, wuser, -1, user, sizeof(user),
                        NULL, NULL);

    if (strlen(user) == 0) {
        MessageBoxW(g_app.hwndMain,
                    L"\x8BF7\x8F93\x5165\x7528\x6237\x540D",
                    L"\x63D0\x793A", MB_OK | MB_ICONWARNING);
        return;
    }

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

    EnableWindow(s_btnLogin, FALSE);

    if (!NetClient_Connect(ip, port, g_app.hwndMain)) {
        MessageBoxW(g_app.hwndMain,
                    L"\x65E0\x6CD5\x8FDE\x63A5\x5230\x670D\x52A1\x5668",
                    L"\x9519\x8BEF", MB_OK | MB_ICONERROR);
        EnableWindow(s_btnLogin, TRUE);
        return;
    }

    strncpy(g_app.server_addr, server, sizeof(g_app.server_addr) - 1);
    strncpy(g_app.username, user, sizeof(g_app.username) - 1);

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

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        /* Fill entire panel with main background. */
        Style_FillBackground(hdc, &rc);

        /* Draw a card behind the form area. */
        int card_w = 440;
        int card_h = 340;
        int card_x = (rc.right - card_w) / 2;
        int card_y = (rc.bottom - card_h) / 2;
        RECT cardRc = { card_x, card_y, card_x + card_w, card_y + card_h };
        Style_DrawCard(hdc, &cardRc, 10);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        HWND hCtl = (HWND)lParam;
        SetBkMode(hdc, TRANSPARENT);
        if (hCtl == s_lblTitle) {
            SetTextColor(hdc, CLR_TEXT_ACCENT);
            return (LRESULT)g_theme.brBgCard;
        }
        if (hCtl == s_lblSubtitle) {
            SetTextColor(hdc, CLR_TEXT_SECONDARY);
            return (LRESULT)g_theme.brBgCard;
        }
        SetTextColor(hdc, CLR_TEXT_PRIMARY);
        return (LRESULT)g_theme.brBgCard;
    }

    case WM_CTLCOLOREDIT: {
        HBRUSH br = Style_OnCtlColor(msg, (HDC)wParam, (HWND)lParam);
        if (br) return (LRESULT)br;
        break;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
