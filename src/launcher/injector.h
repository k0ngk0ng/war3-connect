#ifndef INJECTOR_H
#define INJECTOR_H

#include <windows.h>

/*
 * Inject a DLL into a target process.
 *   hProcess  – handle with PROCESS_ALL_ACCESS
 *   dllPath   – full path to the DLL (ANSI)
 * Returns TRUE on success.
 */
BOOL InjectDLL(HANDLE hProcess, const char *dllPath);

#endif /* INJECTOR_H */
