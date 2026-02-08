#include "hook.h"
#include "config.h"
#include <ws2tcpip.h>
#include <stdio.h>

/* ── Original function pointer ──────────────────────────────────────────── */
typedef int (WSAAPI *sendto_fn)(SOCKET, const char*, int, int,
                                 const struct sockaddr*, int);
static sendto_fn  g_originalSendto = NULL;
static sendto_fn *g_iatSlot        = NULL;   /* pointer to the IAT entry */

/* War3 LAN broadcast port */
#define WAR3_PORT 6112

/* ── Hooked sendto ──────────────────────────────────────────────────────── */
static int WSAAPI Hooked_sendto(
    SOCKET s, const char *buf, int len, int flags,
    const struct sockaddr *to, int tolen)
{
    if (to && to->sa_family == AF_INET && g_config.count > 0) {
        const struct sockaddr_in *sin = (const struct sockaddr_in *)to;
        USHORT port = ntohs(sin->sin_port);

        if (sin->sin_addr.s_addr == INADDR_BROADCAST && port == WAR3_PORT) {
            /* Send to each configured target IP */
            for (int i = 0; i < g_config.count; i++) {
                struct sockaddr_in target = *sin;
                target.sin_addr = g_config.addrs[i];

                g_originalSendto(s, buf, len, flags,
                                 (const struct sockaddr *)&target, sizeof(target));
            }
            /* Also send to original broadcast so LAN still works */
            return g_originalSendto(s, buf, len, flags, to, tolen);
        }
    }
    return g_originalSendto(s, buf, len, flags, to, tolen);
}

/* ── IAT walking helpers ────────────────────────────────────────────────── */
static PIMAGE_IMPORT_DESCRIPTOR GetImportDescriptor(HMODULE hModule)
{
    PBYTE base = (PBYTE)hModule;
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return NULL;

    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return NULL;

    DWORD importRVA = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (importRVA == 0) return NULL;

    return (PIMAGE_IMPORT_DESCRIPTOR)(base + importRVA);
}

static sendto_fn* FindSendtoIATEntry(HMODULE hModule)
{
    PBYTE base = (PBYTE)hModule;
    PIMAGE_IMPORT_DESCRIPTOR imp = GetImportDescriptor(hModule);
    if (!imp) return NULL;

    HMODULE hWs2 = GetModuleHandleA("ws2_32.dll");
    if (!hWs2) hWs2 = GetModuleHandleA("WS2_32.dll");
    if (!hWs2) hWs2 = GetModuleHandleA("WS2_32.DLL");

    FARPROC realSendto = GetProcAddress(hWs2, "sendto");
    if (!realSendto) {
        OutputDebugStringA("[war3hook] Cannot find sendto in ws2_32.dll\n");
        return NULL;
    }

    for (; imp->Name; imp++) {
        const char *dllName = (const char *)(base + imp->Name);
        if (_stricmp(dllName, "ws2_32.dll") != 0 &&
            _stricmp(dllName, "wsock32.dll") != 0)
            continue;

        PIMAGE_THUNK_DATA origThunk = (PIMAGE_THUNK_DATA)(base + imp->OriginalFirstThunk);
        PIMAGE_THUNK_DATA iatThunk  = (PIMAGE_THUNK_DATA)(base + imp->FirstThunk);

        for (; origThunk->u1.AddressOfData; origThunk++, iatThunk++) {
            /* Compare by address: if the IAT currently points to the real sendto */
            if ((FARPROC)iatThunk->u1.Function == realSendto) {
                return (sendto_fn *)&iatThunk->u1.Function;
            }
        }
    }
    return NULL;
}

/* ── Public API ─────────────────────────────────────────────────────────── */
BOOL Hook_Install(void)
{
    HMODULE hExe = GetModuleHandleA(NULL);

    /* Make sure winsock is loaded */
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    g_iatSlot = FindSendtoIATEntry(hExe);
    if (!g_iatSlot) {
        OutputDebugStringA("[war3hook] sendto IAT entry not found, trying wsock32 fallback\n");
        /* War3 might use wsock32.dll which forwards to ws2_32.dll.
           Try to hook wsock32's sendto as well. */
        HMODULE hWsock = GetModuleHandleA("wsock32.dll");
        if (hWsock) {
            /* For wsock32, we still hook the main exe's IAT */
        }
        /* If still not found, try Detours-style inline hook as fallback:
           For simplicity we just report failure here. */
        OutputDebugStringA("[war3hook] HOOK FAILED: could not locate sendto in IAT\n");
        return FALSE;
    }

    g_originalSendto = *g_iatSlot;

    DWORD oldProtect;
    VirtualProtect(g_iatSlot, sizeof(void*), PAGE_READWRITE, &oldProtect);
    *g_iatSlot = (sendto_fn)Hooked_sendto;
    VirtualProtect(g_iatSlot, sizeof(void*), oldProtect, &oldProtect);

    OutputDebugStringA("[war3hook] sendto hook installed successfully!\n");
    return TRUE;
}

void Hook_Uninstall(void)
{
    if (g_iatSlot && g_originalSendto) {
        DWORD oldProtect;
        VirtualProtect(g_iatSlot, sizeof(void*), PAGE_READWRITE, &oldProtect);
        *g_iatSlot = g_originalSendto;
        VirtualProtect(g_iatSlot, sizeof(void*), oldProtect, &oldProtect);
        OutputDebugStringA("[war3hook] sendto hook removed\n");
    }
    g_iatSlot = NULL;
    g_originalSendto = NULL;
}
