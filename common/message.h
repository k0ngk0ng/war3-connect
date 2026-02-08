/*
 * message.h – War3 Connect message type constants and shared structures.
 *
 * Every message exchanged between client and server is a JSON object that
 * contains at least a "type" field whose value is one of the string
 * constants defined below.
 */

#ifndef MESSAGE_H
#define MESSAGE_H

/* ------------------------------------------------------------------ */
/*  Size limits                                                       */
/* ------------------------------------------------------------------ */

#define MAX_USERNAME     32
#define MAX_ROOM_NAME    64
#define MAX_CHAT_MSG     256
#define MAX_IP_STR       46   /* enough for IPv6 + NUL */
#define MAX_ROOM_PLAYERS 16

/* ------------------------------------------------------------------ */
/*  Client → Server message types                                     */
/* ------------------------------------------------------------------ */

#define MSG_LOGIN        "login"
#define MSG_ROOM_LIST    "room_list"
#define MSG_ROOM_CREATE  "room_create"
#define MSG_ROOM_JOIN    "room_join"
#define MSG_ROOM_LEAVE   "room_leave"
#define MSG_CHAT         "chat"
#define MSG_HEARTBEAT    "heartbeat"

/* ------------------------------------------------------------------ */
/*  Server → Client message types                                     */
/* ------------------------------------------------------------------ */

#define MSG_LOGIN_OK       "login_ok"
#define MSG_LOGIN_FAIL     "login_fail"
#define MSG_ROOM_LIST_RES  "room_list_result"
#define MSG_ROOM_CREATED   "room_created"
#define MSG_ROOM_JOINED    "room_joined"
#define MSG_ROOM_PEERS     "room_peers"
#define MSG_ROOM_LEFT      "room_left"
#define MSG_CHAT_MSG       "chat_msg"
#define MSG_PLAYER_JOINED  "player_joined"
#define MSG_PLAYER_LEFT    "player_left"
#define MSG_ERROR          "error"
#define MSG_HEARTBEAT_ACK  "heartbeat_ack"

/* ------------------------------------------------------------------ */
/*  Shared data structures                                            */
/* ------------------------------------------------------------------ */

typedef struct {
    int  id;
    char name[MAX_ROOM_NAME];
    int  player_count;
    int  max_players;
} RoomInfo;

typedef struct {
    char username[MAX_USERNAME];
    char ip[MAX_IP_STR];
} PeerInfo;

#endif /* MESSAGE_H */
