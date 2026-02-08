#include "hook.h"
#include "config.h"
#include <stdio.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        OutputDebugStringA("[war3hook] DLL loaded into process\n");

        if (Config_Load(&g_config) == 0) {
            OutputDebugStringA("[war3hook] WARNING: no target IPs configured!\n");
        }

        if (!Hook_Install()) {
            OutputDebugStringA("[war3hook] Hook installation failed\n");
        }
        break;

    case DLL_PROCESS_DETACH:
        Hook_Uninstall();
        OutputDebugStringA("[war3hook] DLL unloaded\n");
        break;
    }
    return TRUE;
}
