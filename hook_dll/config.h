#ifndef CONFIG_H
#define CONFIG_H

#include <winsock2.h>
#include <windows.h>

#define MAX_TARGET_IPS 16

typedef struct {
    struct in_addr addrs[MAX_TARGET_IPS];
    int count;
} TargetConfig;

/* Reads target IPs from war3hook.cfg next to the DLL. Returns count of IPs loaded. */
int Config_Load(TargetConfig *cfg);

/* Check if config file has been modified since last load. If so, reload.
   Call this periodically (e.g., every few seconds from the hook).
   Returns TRUE if config was reloaded. */
BOOL Config_CheckReload(TargetConfig *cfg);

/* Global config instance */
extern TargetConfig g_config;

/* Critical section for thread-safe config access */
extern CRITICAL_SECTION g_configLock;

#endif /* CONFIG_H */
