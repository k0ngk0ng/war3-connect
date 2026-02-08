/*
 * resource.h – Dialog/Control IDs and custom messages for War3 Platform client.
 */

#ifndef RESOURCE_H
#define RESOURCE_H

/* ------------------------------------------------------------------ */
/*  Generic                                                           */
/* ------------------------------------------------------------------ */
#define IDC_STATIC              -1

/* ------------------------------------------------------------------ */
/*  Login page controls                                               */
/* ------------------------------------------------------------------ */
#define IDC_EDIT_SERVER         1001
#define IDC_EDIT_USERNAME       1002
#define IDC_BTN_LOGIN           1003

/* ------------------------------------------------------------------ */
/*  Lobby page controls                                               */
/* ------------------------------------------------------------------ */
#define IDC_LIST_ROOMS          1101
#define IDC_BTN_CREATE_ROOM     1102
#define IDC_BTN_JOIN_ROOM       1103
#define IDC_BTN_REFRESH         1104
#define IDC_EDIT_ROOM_NAME      1105
#define IDC_EDIT_MAX_PLAYERS    1106

/* ------------------------------------------------------------------ */
/*  Room page controls                                                */
/* ------------------------------------------------------------------ */
#define IDC_LIST_PLAYERS        1201
#define IDC_LIST_CHAT           1202
#define IDC_EDIT_CHAT           1203
#define IDC_BTN_SEND_CHAT       1204
#define IDC_BTN_LEAVE_ROOM      1205
#define IDC_BTN_START_GAME      1206
#define IDC_STATIC_ROOM_NAME    1207

/* ------------------------------------------------------------------ */
/*  Timer IDs                                                         */
/* ------------------------------------------------------------------ */
#define IDT_HEARTBEAT           2001
#define HEARTBEAT_INTERVAL_MS   15000

/* ------------------------------------------------------------------ */
/*  Custom window messages                                            */
/* ------------------------------------------------------------------ */
#define WM_NETWORK_MSG          (WM_USER + 1)
#define WM_NET_DISCONNECTED     (WM_USER + 2)

#endif /* RESOURCE_H */
