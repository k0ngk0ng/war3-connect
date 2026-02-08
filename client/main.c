/*
 * main.c – WinMain entry point for War3 Platform client.
 *
 * Initialises Winsock, creates the GUI, and runs the message loop.
 */

#include <winsock2.h>
#include <windows.h>
#include "gui.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev,
                   LPSTR lpCmd, int nShow)
{
    (void)hPrev;
    (void)lpCmd;
    (void)nShow;

    /* Initialise Winsock 2.2. */
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 1;

    /* Create and show the GUI. */
    if (!GUI_Init(hInstance)) {
        WSACleanup();
        return 1;
    }

    /* Run the Win32 message loop (blocks until WM_QUIT). */
    GUI_Run();

    WSACleanup();
    return 0;
}
