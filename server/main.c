/*
 * main.c – War3 Lobby Server entry point.
 */

#include "server.h"
#include "../common/protocol.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    int port = DEFAULT_SERVER_PORT;  /* 12000 */
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            printf("Invalid port: %s\n", argv[1]);
            return 1;
        }
    }

    Server srv;
    if (Server_Init(&srv, port) != 0) {
        printf("Failed to initialise server on port %d\n", port);
        return 1;
    }

    printf("War3 Lobby Server running on port %d\n", port);

    Server_Run(&srv);
    Server_Shutdown(&srv);

    return 0;
}
