/*
 * net_client.h – Network client for War3 Platform.
 *
 * Runs a background receive thread and posts complete JSON messages
 * to the GUI thread via WM_NETWORK_MSG.  The lParam of that message
 * is a malloc'd char* that the GUI must free().
 */

#ifndef NET_CLIENT_H
#define NET_CLIENT_H

#include <winsock2.h>
#include <windows.h>

/* Connect to the lobby server.  hwnd will receive:
 *   WM_NETWORK_MSG      – lParam = (char*) malloc'd JSON string
 *   WM_NET_DISCONNECTED  – server connection lost                    */
BOOL NetClient_Connect(const char *server_ip, int port, HWND hwnd);

/* Gracefully disconnect and stop the receive thread. */
void NetClient_Disconnect(void);

/* Send a JSON string (will be length-prefixed on the wire).
 * Returns TRUE on success. */
BOOL NetClient_Send(const char *json_str);

/* Returns TRUE if currently connected. */
BOOL NetClient_IsConnected(void);

#endif /* NET_CLIENT_H */
