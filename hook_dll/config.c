#include "config.h"
#include <ws2tcpip.h>
#include <stdio.h>

TargetConfig g_config = {0};
CRITICAL_SECTION g_configLock;

/* Cached config file path and last modification time */
static char     g_cfgPath[MAX_PATH] = {0};
static FILETIME g_lastModTime       = {0, 0};
static BOOL     g_pathInitialized   = FALSE;

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

/* Update the cached last-modification time for the config file.
   Returns TRUE if the timestamp was successfully read. */
static BOOL UpdateLastModTime(void)
{
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (GetFileAttributesExA(g_cfgPath, GetFileExInfoStandard, &fad)) {
        g_lastModTime = fad.ftLastWriteTime;
        return TRUE;
    }
    return FALSE;
}

int Config_Load(TargetConfig *cfg)
{
    EnterCriticalSection(&g_configLock);

    /* Build and cache the config file path on first call */
    if (!g_pathInitialized) {
        char dir[MAX_PATH];
        if (!GetThisDllDirectory(dir, MAX_PATH)) {
            OutputDebugStringA("[war3hook] Failed to get DLL directory\n");
            LeaveCriticalSection(&g_configLock);
            return 0;
        }
        snprintf(g_cfgPath, MAX_PATH, "%swar3hook.cfg", dir);
        g_pathInitialized = TRUE;
    }

    cfg->count = 0;

    FILE *fp = fopen(g_cfgPath, "r");
    if (!fp) {
        OutputDebugStringA("[war3hook] Cannot open war3hook.cfg\n");
        LeaveCriticalSection(&g_configLock);
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

    /* Record the file's current modification time */
    UpdateLastModTime();

    char msg[256];
    snprintf(msg, sizeof(msg), "[war3hook] Loaded %d target IP(s)\n", cfg->count);
    OutputDebugStringA(msg);

    LeaveCriticalSection(&g_configLock);
    return cfg->count;
}

BOOL Config_CheckReload(TargetConfig *cfg)
{
    if (!g_pathInitialized) return FALSE;

    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(g_cfgPath, GetFileExInfoStandard, &fad)) {
        return FALSE;  /* file not accessible, skip */
    }

    /* Compare last-write timestamps */
    if (CompareFileTime(&fad.ftLastWriteTime, &g_lastModTime) == 0) {
        return FALSE;  /* unchanged */
    }

    OutputDebugStringA("[war3hook] Config file changed, reloading...\n");

    /* Config_Load acquires the critical section internally */
    Config_Load(cfg);
    return TRUE;
}
