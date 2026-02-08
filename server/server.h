/*
 * server.h – TCP server core for War3 Lobby Server.
 */

#ifndef SERVER_H
#define SERVER_H

#include "user.h"
#include "room.h"

typedef struct {
    int listen_fd;
    int port;
    User users[MAX_USERS];
    Room rooms[MAX_ROOMS];
} Server;

/* Initialise the server: create listening socket, bind, listen. */
int Server_Init(Server *srv, int port);

/* Main event loop (blocking).  Uses select() for multiplexing. */
void Server_Run(Server *srv);

/* Graceful shutdown: close all connections and the listening socket. */
void Server_Shutdown(Server *srv);

#endif /* SERVER_H */
