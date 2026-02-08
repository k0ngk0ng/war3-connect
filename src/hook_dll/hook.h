#ifndef HOOK_H
#define HOOK_H

#include <winsock2.h>
#include <windows.h>

/* Install inline hook on ws2_32.dll!sendto */
BOOL Hook_Install(void);

/* Restore original sendto bytes */
void Hook_Uninstall(void);

#endif /* HOOK_H */
