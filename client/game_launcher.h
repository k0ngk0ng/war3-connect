/*
 * game_launcher.h – Launch War3 with DLL injection.
 */

#ifndef GAME_LAUNCHER_H
#define GAME_LAUNCHER_H

#include <windows.h>

/*
 * Write war3hook.cfg with peer IPs, then launch war3.exe with
 * war3hook.dll injected via CreateRemoteThread.
 *
 *   peer_ips   – array of IP strings for other players in the room
 *   peer_count – number of entries in peer_ips
 *   war3_path  – full path to war3.exe (NULL = look next to this exe)
 *
 * Returns TRUE on success.
 */
BOOL GameLauncher_Start(const char **peer_ips, int peer_count,
                        const char *war3_path);

#endif /* GAME_LAUNCHER_H */
