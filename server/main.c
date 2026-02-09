/*
 * main.c – War3 Lobby Server entry point.
 */

#include "server.h"
#include "../common/protocol.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
/*
 * On Windows, if the user double-clicks the exe, a new console is
 * allocated and will vanish the moment the process exits.  We detect
 * this case and pause before exiting so the user can read any error
 * messages.
 */
static int s_should_pause = 0;

static void maybe_pause(void)
{
    if (s_should_pause) {
        printf("\nPress Enter to exit...");
        fflush(stdout);
        getchar();
    }
}

static void detect_console(void)
{
    /* If this process is the sole owner of the console (i.e. the user
     * double-clicked the exe rather than running from cmd/PowerShell),
     * GetConsoleProcessList returns 1. */
    DWORD pids[2];
    DWORD count = GetConsoleProcessList(pids, 2);
    if (count <= 1)
        s_should_pause = 1;
}
#endif

int main(int argc, char *argv[])
{
#ifdef _WIN32
    /* Set console output to UTF-8 so Chinese characters display correctly. */
    SetConsoleOutputCP(65001);
    detect_console();
    atexit(maybe_pause);
#endif

    int port = DEFAULT_SERVER_PORT;  /* 12000 */
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            printf("Invalid port: %s\n", argv[1]);
            return 1;
        }
    }

    printf("========================================\n");
    printf("  War3 Lobby Server\n");
    printf("========================================\n");
    printf("Initialising on port %d ...\n", port);
    fflush(stdout);

    Server srv;
    if (Server_Init(&srv, port) != 0) {
        printf("[ERROR] Failed to initialise server on port %d\n", port);
        printf("Possible causes:\n");
        printf("  - Port %d is already in use\n", port);
        printf("  - Insufficient permissions\n");
        return 1;
    }

    printf("[OK] Server is running on port %d\n", port);
    printf("Waiting for connections... (Ctrl+C to stop)\n\n");
    fflush(stdout);

    Server_Run(&srv);
    Server_Shutdown(&srv);

    return 0;
}
