/*
 * room.h – Room management for War3 Lobby Server.
 */

#ifndef ROOM_H
#define ROOM_H

#include "../common/message.h"
#include "user.h"

#define MAX_ROOMS 64

typedef struct {
    int id;                      /* room id, 0 if slot unused */
    char name[MAX_ROOM_NAME];
    int max_players;
    int creator_fd;              /* fd of the creator */
} Room;

/* Initialise all room slots to "unused". */
void Rooms_Init(Room rooms[], int count);

/*
 * Create a new room.  Picks the first free slot, assigns a unique id.
 * Returns a pointer to the new room, or NULL if no slots available.
 */
Room *Rooms_Create(Room rooms[], int count,
                   const char *name, int max_players, int creator_fd);

/* Find a room by its id.  Returns NULL if not found. */
Room *Rooms_FindById(Room rooms[], int count, int id);

/* Destroy a room (mark its slot as unused). */
void Rooms_Destroy(Room rooms[], int count, int id);

/*
 * Fill out_list with RoomInfo entries for every active room.
 * player_count is computed by scanning the users array.
 * Returns the number of entries written (at most out_max).
 */
int Rooms_GetList(Room rooms[], int count,
                  RoomInfo *out_list, int out_max,
                  User users[], int user_count);

#endif /* ROOM_H */
