#include "config.h"
#include <ws2tcpip.h>
#include <stdio.h>

TargetConfig g_config = {0};

/* Get the directory where this DLL resides */
static BOOL GetThisDllDirectory(char *buf, DWORD bufSize)
{
    HMODULE hMod = NULL;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)GetThisDllDirectory, &hMod);
    if (!hMod) return FALSE;

    DWORD len = GetModuleFileNameA(hMod, buf, bufSize);
    if (len == 0 || len >= bufSize) return FALSE;

    /* Strip filename, keep trailing backslash */
    char *p = strrchr(buf, '\\');
    if (p) *(p + 1) = '\0';
    else buf[0] = '\0';
    return TRUE;
}

int Config_Load(TargetConfig *cfg)
{
    char path[MAX_PATH];
    cfg->count = 0;

    if (!GetThisDllDirectory(path, MAX_PATH)) {
        OutputDebugStringA("[war3hook] Failed to get DLL directory\n");
        return 0;
    }
    strcat_s(path, MAX_PATH, "war3hook.cfg");

    FILE *fp = fopen(path, "r");
    if (!fp) {
        OutputDebugStringA("[war3hook] Cannot open war3hook.cfg\n");
        return 0;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp) && cfg->count < MAX_TARGET_IPS) {
        /* Trim newline */
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        nl = strchr(line, '\r');
        if (nl) *nl = '\0';

        /* Skip empty lines and comments */
        if (line[0] == '\0' || line[0] == '#') continue;

        if (inet_pton(AF_INET, line, &cfg->addrs[cfg->count]) == 1) {
            char msg[512];
            snprintf(msg, sizeof(msg), "[war3hook] Target IP #%d: %s\n", cfg->count + 1, line);
            OutputDebugStringA(msg);
            cfg->count++;
        } else {
            char msg[512];
            snprintf(msg, sizeof(msg), "[war3hook] Invalid IP: %s\n", line);
            OutputDebugStringA(msg);
        }
    }

    fclose(fp);

    char msg[256];
    snprintf(msg, sizeof(msg), "[war3hook] Loaded %d target IP(s)\n", cfg->count);
    OutputDebugStringA(msg);
    return cfg->count;
}
