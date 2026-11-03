#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ExportTable {
    BYTE* base;
    IMAGE_EXPORT_DIRECTORY* directory;
};

static ExportTable GetExports(HMODULE module) {
    BYTE* base = reinterpret_cast<BYTE*>(module);
    const auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    const auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const auto& data = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    return { base, reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(base + data.VirtualAddress) };
}

static FARPROC FindRawExport(const ExportTable& table, const char* name, WORD* ordinal) {
    const auto names = reinterpret_cast<DWORD*>(table.base + table.directory->AddressOfNames);
    const auto ordinals = reinterpret_cast<WORD*>(table.base + table.directory->AddressOfNameOrdinals);
    const auto functions = reinterpret_cast<DWORD*>(table.base + table.directory->AddressOfFunctions);
    for (DWORD i = 0; i < table.directory->NumberOfNames; ++i) {
        if (strcmp(name, reinterpret_cast<char*>(table.base + names[i])) == 0) {
            *ordinal = static_cast<WORD>(table.directory->Base + ordinals[i]);
            return reinterpret_cast<FARPROC>(table.base + functions[ordinals[i]]);
        }
    }
    return nullptr;
}

static bool IsExecutable(FARPROC address) {
    MEMORY_BASIC_INFORMATION info = {};
    if (!VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info))) return false;
    const DWORD protection = info.Protect & 0xff;
    return info.State == MEM_COMMIT && (protection == PAGE_EXECUTE ||
        protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY);
}

struct ParallelCall {
    HANDLE start;
    FARPROC function;
};

static DWORD WINAPI RunParallelCall(void* parameter) {
    const auto call = static_cast<ParallelCall*>(parameter);
    if (WaitForSingleObject(call->start, 5000) != WAIT_OBJECT_0) return 1;
    using NoArguments = DWORD (WINAPI*)();
    reinterpret_cast<NoArguments>(call->function)();
    return 0;
}

static bool TestParallelFirstCalls(HMODULE proxy) {
    const char* names[] = { "waveOutGetNumDevs", "waveOutGetNumDevs", "waveInGetNumDevs",
                            "midiInGetNumDevs", "midiOutGetNumDevs", "joyGetNumDevs" };
    HANDLE start = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HANDLE threads[_countof(names)] = {};
    ParallelCall calls[_countof(names)] = {};
    if (!start) return false;
    bool ok = true;
    DWORD count = 0;
    for (; count < _countof(names); ++count) {
        calls[count] = { start, GetProcAddress(proxy, names[count]) };
        if (!calls[count].function) { ok = false; break; }
        threads[count] = CreateThread(nullptr, 0, RunParallelCall, &calls[count], 0, nullptr);
        if (!threads[count]) { ok = false; break; }
    }
    SetEvent(start);
    if (count && WaitForMultipleObjects(count, threads, TRUE, 10000) != WAIT_OBJECT_0) {
        fprintf(stderr, "parallel first calls timed out\n");
        TerminateProcess(GetCurrentProcess(), 1);
    }
    for (DWORD i = 0; i < count; ++i) {
        DWORD code = 1;
        if (!GetExitCodeThread(threads[i], &code) || code != 0) ok = false;
        CloseHandle(threads[i]);
    }
    CloseHandle(start);
    return ok;
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        fwprintf(stderr, L"usage: winmm_proxy_smoke.exe <absolute-path-to-proxy>\n");
        return 2;
    }

    HMODULE proxy = LoadLibraryW(argv[1]);
    if (!proxy) {
        fprintf(stderr, "LoadLibrary(proxy) failed: %lu\n", GetLastError());
        return 2;
    }

    wchar_t systemPath[MAX_PATH] = {};
    const UINT length = GetSystemDirectoryW(systemPath, MAX_PATH);
    if (!length || length >= MAX_PATH || wcscat_s(systemPath, L"\\winmm.dll") != 0) return 2;
    HMODULE system = LoadLibraryW(systemPath);
    if (!system || system == proxy) {
        fprintf(stderr, "LoadLibrary(system winmm) failed or returned the proxy: %lu\n", GetLastError());
        return 2;
    }

    const ExportTable proxyExports = GetExports(proxy);
    const ExportTable systemExports = GetExports(system);
    const auto names = reinterpret_cast<DWORD*>(systemExports.base + systemExports.directory->AddressOfNames);
    const auto ordinals = reinterpret_cast<WORD*>(systemExports.base + systemExports.directory->AddressOfNameOrdinals);
    unsigned missing = 0, notExecutable = 0, wrongOrdinal = 0, wrongResolution = 0;
    for (DWORD i = 0; i < systemExports.directory->NumberOfNames; ++i) {
        const char* name = reinterpret_cast<char*>(systemExports.base + names[i]);
        WORD ordinal = 0;
        FARPROC raw = FindRawExport(proxyExports, name, &ordinal);
        if (!raw) {
            ++missing;
            continue;
        }
        if (!IsExecutable(raw)) ++notExecutable;
        if (ordinal != systemExports.directory->Base + ordinals[i]) ++wrongOrdinal;
        if (GetProcAddress(proxy, name) != raw ||
            GetProcAddress(proxy, MAKEINTRESOURCEA(ordinal)) != raw) ++wrongResolution;
    }
    printf("system named exports=%lu missing=%u nonexecutable=%u ordinal_mismatch=%u resolution_mismatch=%u\n",
           systemExports.directory->NumberOfNames, missing, notExecutable, wrongOrdinal, wrongResolution);

    WORD ordinal = 0;
    FARPROC rawTime = FindRawExport(proxyExports, "timeGetTime", &ordinal);
    if (!rawTime || !IsExecutable(rawTime)) {
        fprintf(stderr, "manual timeGetTime resolution is not executable\n");
        return 1;
    }
    FARPROC ordinalOnly = GetProcAddress(proxy, MAKEINTRESOURCEA(2));
    if (!ordinalOnly || !IsExecutable(ordinalOnly) || !TestParallelFirstCalls(proxy)) {
        fprintf(stderr, "ordinal-only lookup or parallel first calls failed\n");
        return 1;
    }
    printf("ordinal-only lookup and parallel first calls succeeded\n");

    using TimeGetTime = DWORD (WINAPI*)();
    const DWORD actual = reinterpret_cast<TimeGetTime>(rawTime)();
    const DWORD expected = reinterpret_cast<TimeGetTime>(GetProcAddress(system, "timeGetTime"))();
    if (expected - actual > 1000) {
        fprintf(stderr, "timeGetTime result differs from system: %lu vs %lu\n", actual, expected);
        return 1;
    }
    printf("manual timeGetTime call succeeded: ordinal=%u value=%lu\n", ordinal, actual);

    using TimeGetDevCaps = MMRESULT (WINAPI*)(LPTIMECAPS, UINT);
    const auto proxyCaps = reinterpret_cast<TimeGetDevCaps>(GetProcAddress(proxy, "timeGetDevCaps"));
    const auto systemCaps = reinterpret_cast<TimeGetDevCaps>(GetProcAddress(system, "timeGetDevCaps"));
    TIMECAPS actualCaps = {}, expectedCaps = {};
    if (!proxyCaps || !systemCaps || proxyCaps(&actualCaps, sizeof(actualCaps)) !=
        systemCaps(&expectedCaps, sizeof(expectedCaps)) ||
        memcmp(&actualCaps, &expectedCaps, sizeof(actualCaps)) != 0) {
        fprintf(stderr, "timeGetDevCaps differs from system\n");
        return 1;
    }
    using StringToFourCC = FOURCC (WINAPI*)(LPCSTR, UINT);
    const auto proxyFourCC = reinterpret_cast<StringToFourCC>(GetProcAddress(proxy, "mmioStringToFOURCCA"));
    const auto systemFourCC = reinterpret_cast<StringToFourCC>(GetProcAddress(system, "mmioStringToFOURCCA"));
    if (!proxyFourCC || !systemFourCC || proxyFourCC("abcd", 0) != systemFourCC("abcd", 0)) {
        fprintf(stderr, "mmioStringToFOURCCA differs from system\n");
        return 1;
    }
    printf("multi-argument stdcall results match system winmm\n");
    return missing || notExecutable || wrongOrdinal || wrongResolution ? 1 : 0;
}
