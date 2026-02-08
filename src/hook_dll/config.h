#ifndef CONFIG_H
#define CONFIG_H

#include <winsock2.h>
#include <windows.h>

#define MAX_TARGET_IPS 16

typedef struct {
    struct in_addr addrs[MAX_TARGET_IPS];
    int count;
} TargetConfig;

/* Reads target IPs from war3hook.cfg (one IP per line) next to the DLL.
   Returns number of IPs loaded, or 0 on failure. */
int Config_Load(TargetConfig *cfg);

/* Global config instance */
extern TargetConfig g_config;

#endif /* CONFIG_H */
