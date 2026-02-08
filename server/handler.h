/*
 * handler.h – Message handler for War3 Lobby Server.
 */

#ifndef HANDLER_H
#define HANDLER_H

#include "user.h"
#include "room.h"

/*
 * Process a complete JSON message received from `sender`.
 * May send responses back to the sender and/or broadcast to room members.
 */
void Handler_ProcessMessage(const char *json_str,
                            User *sender,
                            User users[], int user_count,
                            Room rooms[], int room_count);

#endif /* HANDLER_H */
