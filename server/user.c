/*
 * user.c – User slot management implementation.
 */

#include "user.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Users_Init                                                        */
/* ------------------------------------------------------------------ */

void Users_Init(User users[], int count)
{
    for (int i = 0; i < count; i++) {
        users[i].fd             = -1;
        users[i].username[0]    = '\0';
        users[i].ip[0]          = '\0';
        users[i].room_id        = -1;
        users[i].last_heartbeat = 0;
        users[i].recv_len       = 0;
    }
}

/* ------------------------------------------------------------------ */
/*  Users_FindByFd                                                    */
/* ------------------------------------------------------------------ */

User *Users_FindByFd(User users[], int count, int fd)
{
    for (int i = 0; i < count; i++) {
        if (users[i].fd == fd) {
            return &users[i];
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Users_FindByName                                                  */
/* ------------------------------------------------------------------ */

User *Users_FindByName(User users[], int count, const char *name)
{
    if (name == NULL) return NULL;

    for (int i = 0; i < count; i++) {
        if (users[i].fd != -1 && strcmp(users[i].username, name) == 0) {
            return &users[i];
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Users_AllocSlot                                                   */
/* ------------------------------------------------------------------ */

User *Users_AllocSlot(User users[], int count)
{
    for (int i = 0; i < count; i++) {
        if (users[i].fd == -1) {
            return &users[i];
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Users_FreeSlot                                                    */
/* ------------------------------------------------------------------ */

void Users_FreeSlot(User *user)
{
    if (user == NULL) return;

    user->fd             = -1;
    user->username[0]    = '\0';
    user->ip[0]          = '\0';
    user->room_id        = -1;
    user->last_heartbeat = 0;
    user->recv_len       = 0;
    memset(user->recv_buf, 0, sizeof(user->recv_buf));
}

/* ------------------------------------------------------------------ */
/*  Users_CountInRoom                                                 */
/* ------------------------------------------------------------------ */

int Users_CountInRoom(User users[], int count, int room_id)
{
    int n = 0;
    for (int i = 0; i < count; i++) {
        if (users[i].fd != -1 && users[i].room_id == room_id) {
            n++;
        }
    }
    return n;
}
