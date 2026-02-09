/*
 * gui_style.c – Visual theme implementation for War3 Platform.
 */

#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string.h>

#include "gui_style.h"

/* ================================================================== */
/*  Global theme instance                                              */
/* ================================================================== */

StyleTheme g_theme;

/* ================================================================== */
/*  Style_Init / Style_Cleanup                                         */
/* ================================================================== */

static HFONT MakeFont(int size, int weight, const wchar_t *face)
{
    return CreateFontW(
        size, 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
}

void Style_Init(void)
{
    memset(&g_theme, 0, sizeof(g_theme));

    /* Brushes */
    g_theme.brBgMain    = CreateSolidBrush(CLR_BG_MAIN);
    g_theme.brBgPanel   = CreateSolidBrush(CLR_BG_PANEL);
    g_theme.brBgCard    = CreateSolidBrush(CLR_BG_CARD);
    g_theme.brEditBg    = CreateSolidBrush(CLR_EDIT_BG);
    g_theme.brListBg    = CreateSolidBrush(CLR_LIST_BG);
    g_theme.brListAlt   = CreateSolidBrush(CLR_LIST_ALT);
    g_theme.brListSel   = CreateSolidBrush(CLR_LIST_SEL);
    g_theme.brStatusBg  = CreateSolidBrush(CLR_STATUS_BG);
    g_theme.brAccent    = CreateSolidBrush(CLR_ACCENT);
    g_theme.brBtnNormal = CreateSolidBrush(CLR_BTN_NORMAL);

    /* Fonts – L"\x5FAE\x8F6F\x96C5\x9ED1" = "微软雅黑" */
    const wchar_t *face = L"\x5FAE\x8F6F\x96C5\x9ED1";
    g_theme.fontTitle    = MakeFont(-28, FW_BOLD,   face);
    g_theme.fontSubtitle = MakeFont(-16, FW_NORMAL, face);
    g_theme.fontBody     = MakeFont(-14, FW_NORMAL, face);
    g_theme.fontSmall    = MakeFont(-12, FW_NORMAL, face);
    g_theme.fontBtn      = MakeFont(-14, FW_BOLD,   face);

    /* Fallbacks */
    if (!g_theme.fontTitle)    g_theme.fontTitle    = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    if (!g_theme.fontSubtitle) g_theme.fontSubtitle = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    if (!g_theme.fontBody)     g_theme.fontBody     = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    if (!g_theme.fontSmall)    g_theme.fontSmall    = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    if (!g_theme.fontBtn)      g_theme.fontBtn      = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
}

void Style_Cleanup(void)
{
    if (g_theme.brBgMain)    DeleteObject(g_theme.brBgMain);
    if (g_theme.brBgPanel)   DeleteObject(g_theme.brBgPanel);
    if (g_theme.brBgCard)    DeleteObject(g_theme.brBgCard);
    if (g_theme.brEditBg)    DeleteObject(g_theme.brEditBg);
    if (g_theme.brListBg)    DeleteObject(g_theme.brListBg);
    if (g_theme.brListAlt)   DeleteObject(g_theme.brListAlt);
    if (g_theme.brListSel)   DeleteObject(g_theme.brListSel);
    if (g_theme.brStatusBg)  DeleteObject(g_theme.brStatusBg);
    if (g_theme.brAccent)    DeleteObject(g_theme.brAccent);
    if (g_theme.brBtnNormal) DeleteObject(g_theme.brBtnNormal);

    /* Don't delete stock objects */
    HFONT stock = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    if (g_theme.fontTitle    && g_theme.fontTitle    != stock) DeleteObject(g_theme.fontTitle);
    if (g_theme.fontSubtitle && g_theme.fontSubtitle != stock) DeleteObject(g_theme.fontSubtitle);
    if (g_theme.fontBody     && g_theme.fontBody     != stock) DeleteObject(g_theme.fontBody);
    if (g_theme.fontSmall    && g_theme.fontSmall    != stock) DeleteObject(g_theme.fontSmall);
    if (g_theme.fontBtn      && g_theme.fontBtn      != stock) DeleteObject(g_theme.fontBtn);

    memset(&g_theme, 0, sizeof(g_theme));
}

/* ================================================================== */
/*  Owner-draw button subclass                                         */
/* ================================================================== */

/* We store the original wndproc and style flag via properties. */
static const wchar_t *PROP_ORIGPROC  = L"StyleOrigProc";
static const wchar_t *PROP_BTNSTYLE  = L"StyleBtnFlag";
static const wchar_t *PROP_HOVER     = L"StyleHover";

static LRESULT CALLBACK BtnSubclassProc(HWND hwnd, UINT msg,
                                         WPARAM wParam, LPARAM lParam)
{
    WNDPROC origProc = (WNDPROC)GetPropW(hwnd, PROP_ORIGPROC);

    switch (msg) {
    case WM_MOUSEMOVE: {
        BOOL wasHover = (BOOL)(INT_PTR)GetPropW(hwnd, PROP_HOVER);
        if (!wasHover) {
            SetPropW(hwnd, PROP_HOVER, (HANDLE)(INT_PTR)TRUE);
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        break;
    }
    case WM_MOUSELEAVE:
        SetPropW(hwnd, PROP_HOVER, (HANDLE)(INT_PTR)FALSE);
        InvalidateRect(hwnd, NULL, FALSE);
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);

        int style_flag = (int)(INT_PTR)GetPropW(hwnd, PROP_BTNSTYLE);
        BOOL hover     = (BOOL)(INT_PTR)GetPropW(hwnd, PROP_HOVER);
        BOOL pressed   = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) && hover;
        BOOL disabled  = !IsWindowEnabled(hwnd);

        /* Choose colors based on style and state */
        COLORREF bgColor, textColor;

        if (disabled) {
            bgColor   = RGB(50, 55, 65);
            textColor = RGB(100, 105, 115);
        } else if (style_flag == BTN_STYLE_PRIMARY) {
            bgColor   = pressed ? CLR_ACCENT_PRESSED
                      : hover   ? CLR_ACCENT_HOVER
                      :           CLR_ACCENT;
            textColor = pressed ? RGB(40, 30, 10) : RGB(30, 20, 5);
        } else if (style_flag == BTN_STYLE_DANGER) {
            bgColor   = pressed ? RGB(160, 40, 40)
                      : hover   ? RGB(200, 60, 60)
                      :           RGB(180, 50, 50);
            textColor = CLR_TEXT_PRIMARY;
        } else {
            bgColor   = pressed ? CLR_BTN_PRESSED
                      : hover   ? CLR_BTN_HOVER
                      :           CLR_BTN_NORMAL;
            textColor = CLR_TEXT_PRIMARY;
        }

        /* Draw rounded rect background */
        HBRUSH br = CreateSolidBrush(bgColor);
        HPEN pen = CreatePen(PS_SOLID, 1, bgColor);
        HBRUSH oldBr = (HBRUSH)SelectObject(hdc, br);
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
        SelectObject(hdc, oldBr);
        SelectObject(hdc, oldPen);
        DeleteObject(br);
        DeleteObject(pen);

        /* Draw text */
        wchar_t text[128] = {0};
        GetWindowTextW(hwnd, text, 128);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, textColor);
        HFONT oldFont = (HFONT)SelectObject(hdc, g_theme.fontBtn);
        DrawTextW(hdc, text, -1, &rc,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldFont);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;  /* We handle painting fully */

    case WM_NCDESTROY:
        RemovePropW(hwnd, PROP_ORIGPROC);
        RemovePropW(hwnd, PROP_BTNSTYLE);
        RemovePropW(hwnd, PROP_HOVER);
        break;
    }

    return CallWindowProcW(origProc, hwnd, msg, wParam, lParam);
}

void Style_SetButton(HWND hwndBtn, int style_flag)
{
    if (!hwndBtn) return;

    WNDPROC origProc = (WNDPROC)SetWindowLongPtrW(
        hwndBtn, GWLP_WNDPROC, (LONG_PTR)BtnSubclassProc);
    SetPropW(hwndBtn, PROP_ORIGPROC, (HANDLE)origProc);
    SetPropW(hwndBtn, PROP_BTNSTYLE, (HANDLE)(INT_PTR)style_flag);
    SetPropW(hwndBtn, PROP_HOVER,    (HANDLE)(INT_PTR)FALSE);
}

BOOL Style_DrawButton(DRAWITEMSTRUCT *dis)
{
    /* Not used – we handle via subclass WM_PAINT instead */
    (void)dis;
    return FALSE;
}

/* ================================================================== */
/*  Background painting                                                */
/* ================================================================== */

void Style_FillBackground(HDC hdc, const RECT *rc)
{
    FillRect(hdc, rc, g_theme.brBgMain);
}

void Style_FillPanel(HDC hdc, const RECT *rc)
{
    FillRect(hdc, rc, g_theme.brBgPanel);
}

void Style_FillCard(HDC hdc, const RECT *rc)
{
    FillRect(hdc, rc, g_theme.brBgCard);
}

void Style_DrawCard(HDC hdc, const RECT *rc, int radius)
{
    HBRUSH br = CreateSolidBrush(CLR_BG_CARD);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(60, 70, 90));
    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, br);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    RoundRect(hdc, rc->left, rc->top, rc->right, rc->bottom,
              radius, radius);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(br);
    DeleteObject(pen);
}

/* ================================================================== */
/*  WM_CTLCOLOR* handler                                               */
/* ================================================================== */

HBRUSH Style_OnCtlColor(UINT msg, HDC hdc, HWND hwndCtl)
{
    (void)hwndCtl;

    switch (msg) {
    case WM_CTLCOLORSTATIC:
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, CLR_TEXT_PRIMARY);
        return g_theme.brBgPanel;

    case WM_CTLCOLOREDIT:
        SetBkColor(hdc, CLR_EDIT_BG);
        SetTextColor(hdc, CLR_EDIT_TEXT);
        return g_theme.brEditBg;

    case WM_CTLCOLORLISTBOX:
        SetBkColor(hdc, CLR_LIST_BG);
        SetTextColor(hdc, CLR_LIST_TEXT);
        return g_theme.brListBg;
    }

    return NULL;
}
