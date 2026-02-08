/*
 * room.c – Room management implementation.
 */

#include "room.h"
#include <string.h>

/* Simple monotonically increasing room-id counter. */
static int s_next_room_id = 1;

/* ------------------------------------------------------------------ */
/*  Rooms_Init                                                        */
/* ------------------------------------------------------------------ */

void Rooms_Init(Room rooms[], int count)
{
    for (int i = 0; i < count; i++) {
        rooms[i].id          = 0;
        rooms[i].name[0]     = '\0';
        rooms[i].max_players = 0;
        rooms[i].creator_fd  = -1;
    }
}

/* ------------------------------------------------------------------ */
/*  Rooms_Create                                                      */
/* ------------------------------------------------------------------ */

Room *Rooms_Create(Room rooms[], int count,
                   const char *name, int max_players, int creator_fd)
{
    for (int i = 0; i < count; i++) {
        if (rooms[i].id == 0) {
            rooms[i].id          = s_next_room_id++;
            rooms[i].max_players = max_players;
            rooms[i].creator_fd  = creator_fd;

            strncpy(rooms[i].name, name, MAX_ROOM_NAME - 1);
            rooms[i].name[MAX_ROOM_NAME - 1] = '\0';

            return &rooms[i];
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Rooms_FindById                                                    */
/* ------------------------------------------------------------------ */

Room *Rooms_FindById(Room rooms[], int count, int id)
{
    for (int i = 0; i < count; i++) {
        if (rooms[i].id == id) {
            return &rooms[i];
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Rooms_Destroy                                                     */
/* ------------------------------------------------------------------ */

void Rooms_Destroy(Room rooms[], int count, int id)
{
    for (int i = 0; i < count; i++) {
        if (rooms[i].id == id) {
            rooms[i].id          = 0;
            rooms[i].name[0]     = '\0';
            rooms[i].max_players = 0;
            rooms[i].creator_fd  = -1;
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Rooms_GetList                                                     */
/* ------------------------------------------------------------------ */

int Rooms_GetList(Room rooms[], int count,
                  RoomInfo *out_list, int out_max,
                  User users[], int user_count)
{
    int n = 0;
    for (int i = 0; i < count && n < out_max; i++) {
        if (rooms[i].id != 0) {
            out_list[n].id           = rooms[i].id;
            out_list[n].max_players  = rooms[i].max_players;
            out_list[n].player_count = Users_CountInRoom(users, user_count,
                                                         rooms[i].id);

            strncpy(out_list[n].name, rooms[i].name, MAX_ROOM_NAME - 1);
            out_list[n].name[MAX_ROOM_NAME - 1] = '\0';

            n++;
        }
    }
    return n;
}
