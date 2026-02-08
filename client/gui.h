/*
 * gui.h – Main GUI framework for War3 Platform client.
 *
 * Manages a single top-level window with three "pages" (login, lobby, room)
 * implemented as child panels that are shown/hidden to switch views.
 */

#ifndef GUI_H
#define GUI_H

#include <windows.h>

/* ------------------------------------------------------------------ */
/*  Page identifiers                                                  */
/* ------------------------------------------------------------------ */

typedef enum {
    PAGE_LOGIN,
    PAGE_LOBBY,
    PAGE_ROOM
} PageId;

/* ------------------------------------------------------------------ */
/*  Global application state                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    HINSTANCE hInstance;
    HWND      hwndMain;
    HWND      hwndLogin;     /* Login panel   */
    HWND      hwndLobby;     /* Lobby panel   */
    HWND      hwndRoom;      /* Room panel    */

    char      server_addr[256];
    char      username[32];
    int       user_id;
    int       current_room_id;
    char      current_room_name[64];
    char      war3_path[MAX_PATH];
} AppState;

extern AppState g_app;

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

/* Initialise and show the main window.  Returns FALSE on failure. */
BOOL GUI_Init(HINSTANCE hInstance);

/* Switch the visible page (hides others). */
void GUI_SwitchPage(PageId page);

/* Run the Win32 message loop (blocks until WM_QUIT). */
void GUI_Run(void);

/* Called from WndProc when a complete JSON message arrives. */
void GUI_HandleNetworkMessage(const char *json_str);

/* Called from WndProc when the server connection is lost. */
void GUI_HandleDisconnect(void);

/* ------------------------------------------------------------------ */
/*  Page creation (implemented in gui_login.c, gui_lobby.c, etc.)     */
/* ------------------------------------------------------------------ */

HWND LoginPage_Create(HWND hwndParent, HINSTANCE hInst);
void LoginPage_OnSize(int cx, int cy);
void LoginPage_HandleMessage(const char *type, void *json_root);

HWND LobbyPage_Create(HWND hwndParent, HINSTANCE hInst);
void LobbyPage_OnSize(int cx, int cy);
void LobbyPage_HandleMessage(const char *type, void *json_root);
void LobbyPage_RequestRoomList(void);

HWND RoomPage_Create(HWND hwndParent, HINSTANCE hInst);
void RoomPage_OnSize(int cx, int cy);
void RoomPage_HandleMessage(const char *type, void *json_root);
void RoomPage_ClearAll(void);

#endif /* GUI_H */
