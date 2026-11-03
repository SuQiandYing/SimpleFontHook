#if defined(_M_IX86)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

extern "C" {
void* volatile g_winmmTargets[196] = {};
}

static void* volatile g_systemWinmm = nullptr;

static void ProxyFailure(DWORD status) {
    RaiseException(status, EXCEPTION_NONCONTINUABLE, 0, nullptr);
    TerminateProcess(GetCurrentProcess(), status);
}

extern "C" void __cdecl ResolveWinmmExport(unsigned ordinal) {
    HMODULE system = static_cast<HMODULE>(
        InterlockedCompareExchangePointer(&g_systemWinmm, nullptr, nullptr));
    if (!system) {
        wchar_t path[MAX_PATH] = {};
        const UINT length = GetSystemDirectoryW(path, MAX_PATH);
        if (!length || length >= MAX_PATH || wcscat_s(path, L"\\winmm.dll") != 0) {
            ProxyFailure(0xC0000135); // STATUS_DLL_NOT_FOUND
        }
        HMODULE loaded = LoadLibraryW(path);
        if (!loaded) ProxyFailure(0xC0000135);
        system = static_cast<HMODULE>(InterlockedCompareExchangePointer(
            &g_systemWinmm, loaded, nullptr));
        if (system) {
            FreeLibrary(loaded);
        } else {
            system = loaded;
        }
    }

    const char* name = nullptr;
    switch (ordinal) {
    case 2: name = MAKEINTRESOURCEA(2); break;
#define WINMM_EXPORT(exportName, exportOrdinal) case exportOrdinal: name = #exportName; break;
#include "winmm_exports_x86.inc"
#undef WINMM_EXPORT
    default: ProxyFailure(0xC0000139); // STATUS_ENTRYPOINT_NOT_FOUND
    }
    FARPROC target = GetProcAddress(system, name);
    if (!target) ProxyFailure(0xC0000139);
    InterlockedExchangePointer(&g_winmmTargets[ordinal], reinterpret_cast<void*>(target));
}

// The tail jump preserves the caller's stack and calling convention, including
// undocumented winmm exports whose signatures are not available in headers.
#define WINMM_STUB(exportName, exportOrdinal) \
    extern "C" __declspec(naked) void winmm_##exportName() { \
        __asm { cmp dword ptr [g_winmmTargets + exportOrdinal * 4], 0 } \
        __asm { jne resolved } \
        __asm { pushfd } \
        __asm { pushad } \
        __asm { push exportOrdinal } \
        __asm { call ResolveWinmmExport } \
        __asm { add esp, 4 } \
        __asm { popad } \
        __asm { popfd } \
        __asm { resolved: } \
        __asm { jmp dword ptr [g_winmmTargets + exportOrdinal * 4] } \
    }

#define WINMM_EXPORT(exportName, exportOrdinal) \
    __pragma(comment(linker, "/export:" #exportName "=_winmm_" #exportName ",@" #exportOrdinal)) \
    WINMM_STUB(exportName, exportOrdinal)
#include "winmm_exports_x86.inc"
#undef WINMM_EXPORT

#pragma comment(linker, "/export:winmm_ordinal2=_winmm_ordinal2,@2,NONAME")
WINMM_STUB(ordinal2, 2)
#undef WINMM_STUB
#endif
