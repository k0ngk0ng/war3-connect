#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>
#include "injector.h"

#define WAR3_DEFAULT_EXE "war3.exe"
#define CONFIG_FILENAME  "war3hook.cfg"
#define DLL_FILENAME     "war3hook.dll"

static void PrintUsage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s -ip <TARGET_IP> [OPTIONS]\n"
        "\n"
        "Options:\n"
        "  -ip <IP>        Target IP to redirect broadcast to (required, can repeat)\n"
        "  -war3 <PATH>    Path to war3.exe (default: war3.exe in current dir)\n"
        "  -args <ARGS>    Additional arguments to pass to war3.exe\n"
        "  -wait           Wait for war3.exe to exit\n"
        "\n"
        "Examples:\n"
        "  %s -ip 192.168.1.100\n"
        "  %s -ip 10.0.0.5 -ip 10.0.0.6 -war3 \"D:\\Games\\Warcraft III\\war3.exe\"\n",
        prog, prog, prog);
}

/* Get directory of current executable */
static BOOL GetExeDirectory(char *buf, DWORD bufSize)
{
    DWORD len = GetModuleFileNameA(NULL, buf, bufSize);
    if (len == 0 || len >= bufSize) return FALSE;
    char *p = strrchr(buf, '\\');
    if (p) *(p + 1) = '\0';
    return TRUE;
}

int main(int argc, char *argv[])
{
    const char *targetIPs[16] = {0};
    int         targetCount   = 0;
    const char *war3Path      = NULL;
    const char *war3Args      = NULL;
    BOOL        waitForExit   = FALSE;

    /* ── Parse arguments ────────────────────────────────────────────────── */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-ip") == 0 && i + 1 < argc) {
            if (targetCount < 16)
                targetIPs[targetCount++] = argv[++i];
        } else if (strcmp(argv[i], "-war3") == 0 && i + 1 < argc) {
            war3Path = argv[++i];
        } else if (strcmp(argv[i], "-args") == 0 && i + 1 < argc) {
            war3Args = argv[++i];
        } else if (strcmp(argv[i], "-wait") == 0) {
            waitForExit = TRUE;
        } else {
            PrintUsage(argv[0]);
            return 1;
        }
    }

    if (targetCount == 0) {
        fprintf(stderr, "Error: at least one -ip is required.\n\n");
        PrintUsage(argv[0]);
        return 1;
    }

    /* ── Resolve paths ──────────────────────────────────────────────────── */
    char exeDir[MAX_PATH];
    if (!GetExeDirectory(exeDir, MAX_PATH)) {
        fprintf(stderr, "Error: cannot determine executable directory\n");
        return 1;
    }

    char dllPath[MAX_PATH];
    snprintf(dllPath, MAX_PATH, "%s%s", exeDir, DLL_FILENAME);

    /* Check DLL exists */
    if (GetFileAttributesA(dllPath) == INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "Error: %s not found\n", dllPath);
        return 1;
    }

    /* ── Write config file ──────────────────────────────────────────────── */
    char cfgPath[MAX_PATH];
    snprintf(cfgPath, MAX_PATH, "%s%s", exeDir, CONFIG_FILENAME);

    FILE *fp = fopen(cfgPath, "w");
    if (!fp) {
        fprintf(stderr, "Error: cannot write %s\n", cfgPath);
        return 1;
    }
    fprintf(fp, "# war3hook target IPs (one per line)\n");
    for (int i = 0; i < targetCount; i++) {
        fprintf(fp, "%s\n", targetIPs[i]);
        printf("[config] Target IP #%d: %s\n", i + 1, targetIPs[i]);
    }
    fclose(fp);

    /* ── Build war3 command line ────────────────────────────────────────── */
    char war3Exe[MAX_PATH];
    if (war3Path) {
        strncpy(war3Exe, war3Path, MAX_PATH - 1);
        war3Exe[MAX_PATH - 1] = '\0';
    } else {
        snprintf(war3Exe, MAX_PATH, "%s%s", exeDir, WAR3_DEFAULT_EXE);
    }

    /* Check war3.exe exists */
    if (GetFileAttributesA(war3Exe) == INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "Error: %s not found\n", war3Exe);
        return 1;
    }

    char cmdLine[4096];
    if (war3Args)
        snprintf(cmdLine, sizeof(cmdLine), "\"%s\" %s", war3Exe, war3Args);
    else
        snprintf(cmdLine, sizeof(cmdLine), "\"%s\"", war3Exe);

    printf("[launcher] Starting: %s\n", cmdLine);

    /* ── Create process suspended ───────────────────────────────────────── */
    STARTUPINFOA si = { .cb = sizeof(si) };
    PROCESS_INFORMATION pi = {0};

    if (!CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE,
                        CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "Error: CreateProcess failed: %lu\n", GetLastError());
        return 1;
    }

    printf("[launcher] Process created (PID %lu), injecting DLL...\n", pi.dwProcessId);

    /* ── Inject DLL ─────────────────────────────────────────────────────── */
    if (!InjectDLL(pi.hProcess, dllPath)) {
        fprintf(stderr, "Error: DLL injection failed, terminating process\n");
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 1;
    }

    /* ── Resume main thread ─────────────────────────────────────────────── */
    ResumeThread(pi.hThread);
    printf("[launcher] War3 is running with hook active!\n");

    if (waitForExit) {
        printf("[launcher] Waiting for war3 to exit...\n");
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        printf("[launcher] War3 exited with code %lu\n", exitCode);
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}
