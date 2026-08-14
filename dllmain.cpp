#include "framework.h"
#include "hooks/font_hooks.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        FontHooks::Install(hModule);
        // Diagnostics are optional and are enabled after the configuration has
        // been loaded by FontHooks::Install.  Keeping them out of the loader-lock
        // attach path avoids installing a process-wide exception filter by default.
        Utils::InstallDiagnostics(hModule);
        break;
    case DLL_PROCESS_DETACH:
        UNREFERENCED_PARAMETER(reserved);
        // DllMain runs under the loader lock.  Only publish the stop signal here;
        // worker threads observe it and leave without loader-lock waits/callbacks.
        Utils::RequestShutdown();
        break;
    }
    return TRUE;
}
