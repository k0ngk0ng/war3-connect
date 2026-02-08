#include "hook.h"
#include "config.h"
#include <ws2tcpip.h>
#include <stdio.h>

/*
 * Inline Hook (Trampoline) for ws2_32.dll!sendto
 *
 * Strategy:
 *   1. Save the first 5 bytes of the real sendto()
 *   2. Overwrite them with a JMP to our Hooked_sendto()
 *   3. Build a "trampoline" that executes the saved 5 bytes
 *      then JMPs back to sendto+5, so we can call the original.
 *
 * This works regardless of how War3 calls sendto (IAT, GetProcAddress,
 * wsock32 forwarding, etc.) because we patch the actual function body.
 *
 * x86 only (32-bit). JMP rel32 = 0xE9 + 4-byte relative offset.
 */

#define JMP_SIZE 5  /* sizeof(E9 xx xx xx xx) */

typedef int (WSAAPI *sendto_fn)(SOCKET, const char*, int, int,
                                 const struct sockaddr*, int);

/* ── Globals ────────────────────────────────────────────────────────────── */
static BYTE   g_originalBytes[JMP_SIZE] = {0};  /* saved prologue        */
static BYTE  *g_sendtoAddr              = NULL;  /* address of real sendto */
static BOOL   g_hookInstalled           = FALSE;

/*
 * Trampoline: a small executable buffer that contains:
 *   [0..4]  the original 5 bytes of sendto
 *   [5..9]  JMP back to sendto+5
 *
 * Allocated with VirtualAlloc(PAGE_EXECUTE_READWRITE).
 */
static BYTE  *g_trampoline = NULL;
static sendto_fn g_trampolineFn = NULL;  /* callable pointer to trampoline */

/* War3 LAN broadcast port */
#define WAR3_PORT 6112

/* Hot-reload: check config every 3 seconds */
static DWORD g_lastReloadCheck = 0;

/* ── Hooked sendto ──────────────────────────────────────────────────────── */
static int WSAAPI Hooked_sendto(
    SOCKET s, const char *buf, int len, int flags,
    const struct sockaddr *to, int tolen)
{
    /* Periodic hot-reload check */
    DWORD now = GetTickCount();
    if (now - g_lastReloadCheck > 3000) {
        g_lastReloadCheck = now;
        Config_CheckReload(&g_config);
    }

    if (to && to->sa_family == AF_INET) {
        const struct sockaddr_in *sin = (const struct sockaddr_in *)to;
        USHORT port = ntohs(sin->sin_port);

        if (sin->sin_addr.s_addr == INADDR_BROADCAST && port == WAR3_PORT) {
            EnterCriticalSection(&g_configLock);

            if (g_config.count > 0) {
                char dbg[256];
                snprintf(dbg, sizeof(dbg),
                         "[war3hook] Intercepted broadcast to 255.255.255.255:%d, "
                         "redirecting to %d target(s)\n", port, g_config.count);
                OutputDebugStringA(dbg);

                /* Send to each configured target IP */
                for (int i = 0; i < g_config.count; i++) {
                    struct sockaddr_in target = *sin;
                    target.sin_addr = g_config.addrs[i];

                    g_trampolineFn(s, buf, len, flags,
                                   (const struct sockaddr *)&target,
                                   sizeof(target));
                }
            }

            LeaveCriticalSection(&g_configLock);

            /* Also send to original broadcast so LAN still works */
            return g_trampolineFn(s, buf, len, flags, to, tolen);
        }
    }
    return g_trampolineFn(s, buf, len, flags, to, tolen);
}

/* ── Helper: write a JMP rel32 at `src` targeting `dst` ─────────────────── */
static void WriteJump(BYTE *src, BYTE *dst)
{
    src[0] = 0xE9;  /* JMP rel32 */
    *(DWORD *)(src + 1) = (DWORD)(dst - src - JMP_SIZE);
}

/* ── Public API ─────────────────────────────────────────────────────────── */
BOOL Hook_Install(void)
{
    if (g_hookInstalled) return TRUE;

    /* Make sure winsock is loaded */
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    /* Get the real sendto address from ws2_32.dll */
    HMODULE hWs2 = GetModuleHandleA("ws2_32.dll");
    if (!hWs2) {
        hWs2 = LoadLibraryA("ws2_32.dll");
    }
    if (!hWs2) {
        OutputDebugStringA("[war3hook] Cannot load ws2_32.dll\n");
        return FALSE;
    }

    g_sendtoAddr = (BYTE *)GetProcAddress(hWs2, "sendto");
    if (!g_sendtoAddr) {
        OutputDebugStringA("[war3hook] Cannot find sendto in ws2_32.dll\n");
        return FALSE;
    }

    {
        char dbg[256];
        snprintf(dbg, sizeof(dbg), "[war3hook] sendto @ 0x%p\n", g_sendtoAddr);
        OutputDebugStringA(dbg);
    }

    /* Allocate executable memory for the trampoline */
    g_trampoline = (BYTE *)VirtualAlloc(NULL, JMP_SIZE + JMP_SIZE,
                                         MEM_COMMIT | MEM_RESERVE,
                                         PAGE_EXECUTE_READWRITE);
    if (!g_trampoline) {
        OutputDebugStringA("[war3hook] VirtualAlloc for trampoline failed\n");
        return FALSE;
    }

    /* Save original bytes */
    memcpy(g_originalBytes, g_sendtoAddr, JMP_SIZE);

    /* Build trampoline:
     *   [0..4] = original 5 bytes of sendto
     *   [5..9] = JMP (sendto + 5)
     */
    memcpy(g_trampoline, g_originalBytes, JMP_SIZE);
    WriteJump(g_trampoline + JMP_SIZE, g_sendtoAddr + JMP_SIZE);

    g_trampolineFn = (sendto_fn)g_trampoline;

    /* Patch sendto: overwrite first 5 bytes with JMP to Hooked_sendto */
    DWORD oldProtect;
    if (!VirtualProtect(g_sendtoAddr, JMP_SIZE, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        OutputDebugStringA("[war3hook] VirtualProtect failed on sendto\n");
        VirtualFree(g_trampoline, 0, MEM_RELEASE);
        g_trampoline = NULL;
        return FALSE;
    }

    WriteJump(g_sendtoAddr, (BYTE *)Hooked_sendto);
    FlushInstructionCache(GetCurrentProcess(), g_sendtoAddr, JMP_SIZE);

    VirtualProtect(g_sendtoAddr, JMP_SIZE, oldProtect, &oldProtect);

    g_hookInstalled = TRUE;
    OutputDebugStringA("[war3hook] Inline hook on sendto installed successfully!\n");
    return TRUE;
}

void Hook_Uninstall(void)
{
    if (!g_hookInstalled || !g_sendtoAddr) return;

    /* Restore original bytes */
    DWORD oldProtect;
    VirtualProtect(g_sendtoAddr, JMP_SIZE, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(g_sendtoAddr, g_originalBytes, JMP_SIZE);
    FlushInstructionCache(GetCurrentProcess(), g_sendtoAddr, JMP_SIZE);
    VirtualProtect(g_sendtoAddr, JMP_SIZE, oldProtect, &oldProtect);

    /* Free trampoline */
    if (g_trampoline) {
        VirtualFree(g_trampoline, 0, MEM_RELEASE);
        g_trampoline = NULL;
    }

    g_trampolineFn = NULL;
    g_sendtoAddr = NULL;
    g_hookInstalled = FALSE;

    OutputDebugStringA("[war3hook] Inline hook removed, sendto restored\n");
}
