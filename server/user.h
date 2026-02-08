/*
 * user.h – User slot management for War3 Lobby Server.
 */

#ifndef USER_H
#define USER_H

#include <stdint.h>
#include <time.h>
#include "../common/message.h"
#include "../common/protocol.h"

#define MAX_USERS 256

typedef struct {
    int fd;                      /* socket fd, -1 if slot unused */
    char username[MAX_USERNAME]; /* from message.h               */
    char ip[MAX_IP_STR];        /* client's public IP (from accept) */
    int room_id;                /* -1 if not in a room           */
    time_t last_heartbeat;

    /* Receive buffer for TCP framing */
    uint8_t recv_buf[MAX_MSG_SIZE];
    uint32_t recv_len;
} User;

/* Initialise all user slots to "unused". */
void Users_Init(User users[], int count);

/* Find the user slot that owns the given socket fd.  Returns NULL if none. */
User *Users_FindByFd(User users[], int count, int fd);

/* Find the user slot with the given username.  Returns NULL if none. */
User *Users_FindByName(User users[], int count, const char *name);

/* Allocate the first free slot (fd == -1).  Returns NULL if full. */
User *Users_AllocSlot(User users[], int count);

/* Release a user slot back to the pool. */
void Users_FreeSlot(User *user);

/* Count how many active users are in the given room. */
int Users_CountInRoom(User users[], int count, int room_id);

#endif /* USER_H */
