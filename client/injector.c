/*
 * injector.c – DLL injection via CreateRemoteThread + LoadLibraryA.
 *
 * This is the same injection logic used by the standalone launcher,
 * copied here so the GUI client is self-contained.
 */

#include "injector.h"
#include <stdio.h>
#include <string.h>

BOOL InjectDLL(HANDLE hProcess, const char *dllPath)
{
    SIZE_T pathLen = strlen(dllPath) + 1;

    /* Allocate memory in target process for the DLL path. */
    LPVOID remoteBuf = VirtualAllocEx(hProcess, NULL, pathLen,
                                       MEM_COMMIT | MEM_RESERVE,
                                       PAGE_READWRITE);
    if (!remoteBuf) {
        return FALSE;
    }

    /* Write DLL path into target process. */
    if (!WriteProcessMemory(hProcess, remoteBuf, dllPath, pathLen, NULL)) {
        VirtualFreeEx(hProcess, remoteBuf, 0, MEM_RELEASE);
        return FALSE;
    }

    /* Get LoadLibraryA address (same in all processes). */
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    FARPROC pLoadLibrary = GetProcAddress(hKernel32, "LoadLibraryA");
    if (!pLoadLibrary) {
        VirtualFreeEx(hProcess, remoteBuf, 0, MEM_RELEASE);
        return FALSE;
    }

    /* Create remote thread that calls LoadLibraryA(dllPath). */
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0,
                                         (LPTHREAD_START_ROUTINE)pLoadLibrary,
                                         remoteBuf, 0, NULL);
    if (!hThread) {
        VirtualFreeEx(hProcess, remoteBuf, 0, MEM_RELEASE);
        return FALSE;
    }

    /* Wait for LoadLibrary to finish. */
    WaitForSingleObject(hThread, 10000);

    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);

    /* Free the remote buffer. */
    VirtualFreeEx(hProcess, remoteBuf, 0, MEM_RELEASE);

    if (exitCode == 0) {
        /* LoadLibraryA returned NULL – DLL load failed. */
        return FALSE;
    }

    return TRUE;
}
