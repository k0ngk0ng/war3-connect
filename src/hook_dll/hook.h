#ifndef HOOK_H
#define HOOK_H

#include <winsock2.h>
#include <windows.h>

/* Install IAT hook on sendto in the main module */
BOOL Hook_Install(void);

/* Restore original sendto */
void Hook_Uninstall(void);

#endif /* HOOK_H */
