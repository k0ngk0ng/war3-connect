/*
 * gui_style.h – Visual theme constants and helpers for War3 Platform.
 *
 * Provides a unified color palette, owner-draw button support,
 * and custom background painting for a modern look.
 */

#ifndef GUI_STYLE_H
#define GUI_STYLE_H

#include <windows.h>

/* ================================================================== */
/*  Color palette                                                      */
/* ================================================================== */

/* Main background – dark blue-grey */
#define CLR_BG_MAIN         RGB(30, 36, 48)
#define CLR_BG_PANEL         RGB(38, 45, 60)

/* Card / elevated surface */
#define CLR_BG_CARD          RGB(48, 56, 74)
#define CLR_BG_CARD_HOVER    RGB(58, 66, 84)

/* Primary accent – War3 gold */
#define CLR_ACCENT           RGB(218, 165, 32)
#define CLR_ACCENT_HOVER     RGB(240, 190, 50)
#define CLR_ACCENT_PRESSED   RGB(180, 135, 20)

/* Secondary button */
#define CLR_BTN_NORMAL       RGB(60, 70, 90)
#define CLR_BTN_HOVER        RGB(75, 85, 108)
#define CLR_BTN_PRESSED      RGB(45, 52, 68)

/* Text colors */
#define CLR_TEXT_PRIMARY      RGB(230, 235, 245)
#define CLR_TEXT_SECONDARY    RGB(160, 170, 190)
#define CLR_TEXT_ACCENT       RGB(218, 165, 32)
#define CLR_TEXT_SYSTEM       RGB(120, 200, 120)
#define CLR_TEXT_ERROR        RGB(230, 80, 80)

/* Input fields */
#define CLR_EDIT_BG          RGB(25, 30, 40)
#define CLR_EDIT_BORDER      RGB(70, 80, 100)
#define CLR_EDIT_TEXT         RGB(220, 225, 235)

/* List / chat area */
#define CLR_LIST_BG          RGB(28, 33, 44)
#define CLR_LIST_ALT         RGB(34, 40, 52)
#define CLR_LIST_SEL         RGB(50, 70, 110)
#define CLR_LIST_TEXT         RGB(210, 215, 225)

/* Status bar */
#define CLR_STATUS_BG        RGB(22, 26, 35)
#define CLR_STATUS_TEXT       RGB(140, 150, 170)

/* ================================================================== */
/*  Cached GDI objects (call Style_Init / Style_Cleanup)               */
/* ================================================================== */

typedef struct {
    HBRUSH brBgMain;
    HBRUSH brBgPanel;
    HBRUSH brBgCard;
    HBRUSH brEditBg;
    HBRUSH brListBg;
    HBRUSH brListAlt;
    HBRUSH brListSel;
    HBRUSH brStatusBg;
    HBRUSH brAccent;
    HBRUSH brBtnNormal;

    HFONT  fontTitle;      /* 24pt bold  – login title */
    HFONT  fontSubtitle;   /* 14pt       – subtitles   */
    HFONT  fontBody;       /* 12pt       – body text   */
    HFONT  fontSmall;      /* 10pt       – status bar  */
    HFONT  fontBtn;        /* 12pt bold  – buttons     */
} StyleTheme;

extern StyleTheme g_theme;

/* Initialise cached GDI objects. Call once at startup. */
void Style_Init(void);

/* Free cached GDI objects. Call at shutdown. */
void Style_Cleanup(void);

/* ================================================================== */
/*  Owner-draw button helpers                                          */
/* ================================================================== */

/* Button style flags (stored in GWLP_USERDATA) */
#define BTN_STYLE_PRIMARY   1   /* Gold accent button */
#define BTN_STYLE_SECONDARY 2   /* Dark button */
#define BTN_STYLE_DANGER    3   /* Red-ish button */

/*
 * Subclass a button for owner-draw rendering.
 * Call after CreateWindowEx. style_flag = BTN_STYLE_PRIMARY etc.
 */
void Style_SetButton(HWND hwndBtn, int style_flag);

/*
 * Handle WM_DRAWITEM for owner-draw buttons.
 * Returns TRUE if handled.
 */
BOOL Style_DrawButton(DRAWITEMSTRUCT *dis);

/* ================================================================== */
/*  Background painting helpers                                        */
/* ================================================================== */

/* Fill a rect with the main background color. */
void Style_FillBackground(HDC hdc, const RECT *rc);

/* Fill a rect with the panel background color. */
void Style_FillPanel(HDC hdc, const RECT *rc);

/* Fill a rect with the card background color. */
void Style_FillCard(HDC hdc, const RECT *rc);

/* Draw a rounded rectangle card with shadow effect. */
void Style_DrawCard(HDC hdc, const RECT *rc, int radius);

/*
 * Handle WM_CTLCOLORSTATIC / WM_CTLCOLOREDIT / WM_CTLCOLORLISTBOX
 * Returns the brush to use, or NULL if not handled.
 */
HBRUSH Style_OnCtlColor(UINT msg, HDC hdc, HWND hwndCtl);

#endif /* GUI_STYLE_H */
