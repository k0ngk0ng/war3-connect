/*
 * game_launcher.c – Launch War3 with DLL injection.
 *
 * 1. Writes war3hook.cfg next to the executable with peer IPs.
 * 2. Locates war3hook.dll next to the executable.
 * 3. Creates war3.exe in a suspended state.
 * 4. Injects war3hook.dll via the injector module.
 * 5. Resumes the main thread.
 */

#include "game_launcher.h"
#include "injector.h"

#include <stdio.h>
#include <string.h>

#define WAR3_DEFAULT_EXE  "war3.exe"
#define CONFIG_FILENAME   "war3hook.cfg"
#define DLL_FILENAME      "war3hook.dll"

/* ------------------------------------------------------------------ */
/*  Helper: get directory of the running executable                   */
/* ------------------------------------------------------------------ */

static BOOL GetExeDirectory(char *buf, DWORD bufSize)
{
    DWORD len = GetModuleFileNameA(NULL, buf, bufSize);
    if (len == 0 || len >= bufSize) return FALSE;
    char *p = strrchr(buf, '\\');
    if (p) *(p + 1) = '\0';
    return TRUE;
}

/* ------------------------------------------------------------------ */
/*  GameLauncher_Start                                                */
/* ------------------------------------------------------------------ */

BOOL GameLauncher_Start(const char **peer_ips, int peer_count,
                        const char *war3_path)
{
    if (!peer_ips || peer_count <= 0)
        return FALSE;

    /* ── Resolve paths ────────────────────────────────────────────── */
    char exeDir[MAX_PATH];
    if (!GetExeDirectory(exeDir, MAX_PATH))
        return FALSE;

    char dllPath[MAX_PATH];
    snprintf(dllPath, MAX_PATH, "%s%s", exeDir, DLL_FILENAME);

    /* Check DLL exists. */
    if (GetFileAttributesA(dllPath) == INVALID_FILE_ATTRIBUTES)
        return FALSE;

    /* ── Write config file ────────────────────────────────────────── */
    char cfgPath[MAX_PATH];
    snprintf(cfgPath, MAX_PATH, "%s%s", exeDir, CONFIG_FILENAME);

    FILE *fp = fopen(cfgPath, "w");
    if (!fp) return FALSE;

    fprintf(fp, "# war3hook target IPs (one per line)\n");
    for (int i = 0; i < peer_count; i++) {
        fprintf(fp, "%s\n", peer_ips[i]);
    }
    fclose(fp);

    /* ── Build war3 command line ──────────────────────────────────── */
    char war3Exe[MAX_PATH];
    if (war3_path && war3_path[0] != '\0') {
        strncpy(war3Exe, war3_path, MAX_PATH - 1);
        war3Exe[MAX_PATH - 1] = '\0';
    } else {
        snprintf(war3Exe, MAX_PATH, "%s%s", exeDir, WAR3_DEFAULT_EXE);
    }

    /* Check war3.exe exists. */
    if (GetFileAttributesA(war3Exe) == INVALID_FILE_ATTRIBUTES)
        return FALSE;

    char cmdLine[4096];
    snprintf(cmdLine, sizeof(cmdLine), "\"%s\"", war3Exe);

    /* ── Create process suspended ─────────────────────────────────── */
    STARTUPINFOA si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));

    if (!CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE,
                        CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        return FALSE;
    }

    /* ── Inject DLL ───────────────────────────────────────────────── */
    if (!InjectDLL(pi.hProcess, dllPath)) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return FALSE;
    }

    /* ── Resume main thread ───────────────────────────────────────── */
    ResumeThread(pi.hThread);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return TRUE;
}
