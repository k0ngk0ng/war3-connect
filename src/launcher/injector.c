#include "injector.h"
#include <stdio.h>

BOOL InjectDLL(HANDLE hProcess, const char *dllPath)
{
    SIZE_T pathLen = strlen(dllPath) + 1;

    /* Allocate memory in target process for the DLL path */
    LPVOID remoteBuf = VirtualAllocEx(hProcess, NULL, pathLen,
                                       MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteBuf) {
        fprintf(stderr, "[injector] VirtualAllocEx failed: %lu\n", GetLastError());
        return FALSE;
    }

    /* Write DLL path into target process */
    if (!WriteProcessMemory(hProcess, remoteBuf, dllPath, pathLen, NULL)) {
        fprintf(stderr, "[injector] WriteProcessMemory failed: %lu\n", GetLastError());
        VirtualFreeEx(hProcess, remoteBuf, 0, MEM_RELEASE);
        return FALSE;
    }

    /* Get LoadLibraryA address (same in all processes due to kernel32 base) */
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    FARPROC pLoadLibrary = GetProcAddress(hKernel32, "LoadLibraryA");
    if (!pLoadLibrary) {
        fprintf(stderr, "[injector] Cannot find LoadLibraryA\n");
        VirtualFreeEx(hProcess, remoteBuf, 0, MEM_RELEASE);
        return FALSE;
    }

    /* Create remote thread that calls LoadLibraryA(dllPath) */
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0,
                                         (LPTHREAD_START_ROUTINE)pLoadLibrary,
                                         remoteBuf, 0, NULL);
    if (!hThread) {
        fprintf(stderr, "[injector] CreateRemoteThread failed: %lu\n", GetLastError());
        VirtualFreeEx(hProcess, remoteBuf, 0, MEM_RELEASE);
        return FALSE;
    }

    /* Wait for LoadLibrary to finish */
    WaitForSingleObject(hThread, 10000);

    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);

    /* Free the remote buffer */
    VirtualFreeEx(hProcess, remoteBuf, 0, MEM_RELEASE);

    if (exitCode == 0) {
        fprintf(stderr, "[injector] LoadLibraryA returned NULL (DLL load failed)\n");
        return FALSE;
    }

    printf("[injector] DLL injected successfully (module base = 0x%lX)\n", exitCode);
    return TRUE;
}
