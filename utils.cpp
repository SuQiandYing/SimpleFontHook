#include "framework.h"
#include "font/font_patcher.h"
#include <stdarg.h>
#include <shlwapi.h>
#include <fstream>
#include <limits>
#include <vector>
#include <string>
#include <mutex>
#include <cstring>
#include <utility>

#pragma comment(lib, "shlwapi.lib")

namespace Config {
    bool EnableFontHook = false;
    bool EnableFaceNameReplace = false;
    bool EnableCharsetReplace = false;
    bool EnableFontHeightScale = false;
    bool EnableFontWidthScale = false;
    bool EnableFontCharSpacing = false;
    bool EnableFontVerticalMetrics = false;
    bool EnableFontLineSpacing = false;
    bool EnableFontWeight = false;
    bool EnableCodepageSpoof = false;
    bool EnableCodepageRuntimeReplace = false;
    bool EnableCodepageRedirect = false;
    bool EnableTextSubstitution = false;
    bool PickerShowOnStartup = true;
    int TextSubstitutionMode = TextSubstitutionModeJapaneseTraditional;
    UINT TextSubstitutionCodepage = 932;
    UINT YurisAtlasCodepage = 0;
    bool EnableDebugLog = false;
    bool CompatSkipDrawTextA = true;
    bool CompatSkipFontDataQueries = true;
    bool CompatSelectObjectTrackedOnly = false;
    bool CompatHookCreateFontW = true;
    bool CompatHookCreateFontIndirectW = true;
    bool CompatHookGetTextFace = false;
    bool EnableBgiHook = true;
    bool BgiPatchGdiImports = true;
    bool BgiClearGlyphCacheOnSwitch = true;
    bool EnableArtemisHook = true;
    bool ArtemisPatchTables = true;
    bool ArtemisRedirectFontFiles = true;
    bool ArtemisClearFontCacheOnSwitch = true;
    bool EnableKrkrHook = true;
    bool KrkrDisablePrerenderedFonts = true;
    bool EnableEntisHook = true;
    bool EntisDisableBitmapFonts = true;
    bool EntisRefreshFontOnSwitch = true;
    bool EnableSoftpalHook = true;
    bool SoftpalDisableDefaultFontDat = true;
    bool SoftpalForceDefaultOptionToSystemFont = true;
    bool EnableEscudeHook = true;
    bool EscudeVirtualFontConfig = true;
    bool EnableMiraiHook = true;
    bool MiraiReplaceFontDataQueries = true;
    bool MiraiRedirectFontFiles = true;
    bool MiraiPinFontDataSource = true;
    bool EnableMajiroHook = true;
    bool MajiroDisableFontCache = true;
    bool EnableDxLibHook = true;
    bool DxLibDisableFontCache = false;
    bool DxLibReplaceFontDataQueries = true;
    bool DxLibClearRuntimeFontCacheOnSwitch = false;
    bool EnableTinkerBellHook = true;
    bool EnableCatSystemUnityHook = true;
    bool EnableTyranoHook = true;
    bool TyranoRedirectWebFonts = true;
    bool EnableRenPyHook = true;
    bool RenPyRedirectFonts = true;
    bool RenPyRefreshFontOnSwitch = true;
    wchar_t DxLibCachedFontNameW[LF_FACESIZE] = L"";
    wchar_t ForcedFontNameW[LF_FACESIZE] = L"galgame";
    wchar_t SourceFontNameW[LF_FACESIZE] = L"galgame";
    char ForcedFontNameA[LF_FACESIZE] = "galgame";
    DWORD ForcedCharset = DEFAULT_CHARSET;
    DWORD DetectedCharset = DEFAULT_CHARSET;
    float FontHeightScale = 1.0f;
    float FontWidthScale = 1.0f;
    int FontCharSpacing = 0;
    int FontAscentPermille = 880;
    int FontDescentPermille = -120;
    int FontLineSpacing = 0;
    int FontWeight = 400;
    wchar_t FontFileName[MAX_PATH] = L"";
    DWORD SpoofFromCharset = GB2312_CHARSET;      // 134
    DWORD SpoofToCharset = SHIFTJIS_CHARSET;      // 128
    UINT CodepageRedirectFrom = 932;
    UINT CodepageRedirectTo = CP_UTF8;
    volatile LONG ConfigVersion = 0;
    volatile LONG NeedFontReload = 0;
    int DebugSlowMs = 50;
    int DebugTraceSampleLimit = 0;
    int DebugPickerThreadLogLimit = 0;
    wchar_t ArtemisFontPath[MAX_PATH] = L"";
    int ArtemisFontSize = 0;
    int ArtemisRubySize = -1;
}

namespace Utils {
    static std::string WideToUtf8(const wchar_t* w);

    struct DiagnosticBreadcrumb {
        LONG seq;
        DWORD tick;
        DWORD tid;
        char text[512];
    };

    static HMODULE g_diagModule = NULL;
    static PVOID g_vectoredExceptionHandler = NULL;
    static HANDLE g_diagnosticsThread = NULL;
    static DWORD g_diagnosticsThreadId = 0;
    static HANDLE g_diagnosticsStopEvent = NULL;
    static volatile LONG g_diagnosticsInstalled = 0;
    static volatile LONG g_shutdownRequested = 0;
    static volatile LONG g_exceptionLogCount = 0;
    static volatile LONG g_firstChanceExceptionLogCount = 0;
    static volatile LONG g_breadcrumbSeq = 0;
    static DiagnosticBreadcrumb g_breadcrumbs[64] = {};
    static volatile LONG g_watchdogStageActive = 0;
    static volatile LONG g_watchdogStageStartTick = 0;
    static volatile LONG g_watchdogLastReportTick = 0;
    static char g_watchdogStage[512] = {};
    static volatile LONG g_fontSwitchWatchTick = 0;
    static volatile LONG g_fontSwitchWatchVersion = 0;
    static volatile LONG g_fontSwitchWatchLastReportTick = 0;
    static char g_fontSwitchWatchFont[LF_FACESIZE * 4] = {};
    static std::mutex g_traceFileMutex;
    static bool g_traceSessionInitialized = false;

    static void GetTracePath(wchar_t* path, size_t count) {
        if (!path || count == 0) return;
        path[0] = L'\0';
        GetModuleFileNameW(NULL, path, (DWORD)count);
        PathRemoveFileSpecW(path);
        PathAppendW(path, L"FontHook.trace.log");
    }

    static bool EnsureTraceSessionInitializedLocked(wchar_t* path,
        size_t count) {
        GetTracePath(path, count);
        if (!path || !path[0]) return false;
        if (g_traceSessionInitialized) return true;

        HANDLE hFile = CreateFileW(path, GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return false;
        CloseHandle(hFile);
        g_traceSessionInitialized = true;
        return true;
    }

    static void AppendTraceLineDirect(const char* message) {
        if (!message) return;

        std::lock_guard<std::mutex> lock(g_traceFileMutex);
        wchar_t path[MAX_PATH] = {};
        if (!EnsureTraceSessionInitializedLocked(path, _countof(path))) return;

        HANDLE hFile = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return;

        SYSTEMTIME st = {};
        GetLocalTime(&st);
        char line[4096] = {};
        sprintf_s(line, "[%02u:%02u:%02u.%03u][pid=%lu][tid=%lu] %s\r\n",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            GetCurrentProcessId(), GetCurrentThreadId(), message);

        DWORD written = 0;
        WriteFile(hFile, line, (DWORD)strlen(line), &written, NULL);
        CloseHandle(hFile);
    }

    static bool ModuleInfoFromAddress(const void* address, uintptr_t* baseOut,
        char* moduleOut, size_t moduleOutCount) {
        if (baseOut) *baseOut = 0;
        if (moduleOut && moduleOutCount) moduleOut[0] = '\0';
        if (!address) return false;

        MEMORY_BASIC_INFORMATION mbi = {};
        if (VirtualQuery(address, &mbi, sizeof(mbi)) == 0 || !mbi.AllocationBase)
            return false;

        HMODULE module = (HMODULE)mbi.AllocationBase;
        if (baseOut) *baseOut = (uintptr_t)module;

        wchar_t modulePath[MAX_PATH] = {};
        if (GetModuleFileNameW(module, modulePath, MAX_PATH) && moduleOut && moduleOutCount) {
            std::string utf8 = WideToUtf8(modulePath);
            strncpy_s(moduleOut, moduleOutCount, utf8.c_str(), _TRUNCATE);
        }
        return true;
    }

    static void FormatAddress(char* out, size_t outCount, uintptr_t address) {
        if (!out || outCount == 0) return;
        uintptr_t base = 0;
        char module[MAX_PATH * 4] = {};
        if (ModuleInfoFromAddress((const void*)address, &base, module, sizeof(module))) {
            sprintf_s(out, outCount, "%p %s+0x%IX",
                (void*)address, module[0] ? module : "<module>", address - base);
        } else {
            sprintf_s(out, outCount, "%p <unknown>", (void*)address);
        }
    }

    static bool IsInterestingException(DWORD code) {
        switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        case EXCEPTION_DATATYPE_MISALIGNMENT:
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        case EXCEPTION_ILLEGAL_INSTRUCTION:
        case EXCEPTION_IN_PAGE_ERROR:
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_PRIV_INSTRUCTION:
        case EXCEPTION_STACK_OVERFLOW:
            return true;
        default:
            return false;
        }
    }

    static void DumpRecentBreadcrumbsDirect() {
        LONG latest = InterlockedCompareExchange(&g_breadcrumbSeq, 0, 0);
        char line[1024] = {};
        LONG first = latest > 15 ? latest - 15 : 1;
        for (LONG i = first; i <= latest; ++i) {
            const DiagnosticBreadcrumb& b = g_breadcrumbs[i & 63];
            if (b.seq != i) continue;
            sprintf_s(line, "[DIAG][breadcrumb] #%ld tick=%lu age=%lums tid=%lu %s",
                b.seq, b.tick, GetTickCount() - b.tick, b.tid, b.text);
            AppendTraceLineDirect(line);
        }
    }

    struct WindowProbeContext {
        DWORD pid;
        int total;
        int unresponsive;
    };

    static BOOL CALLBACK ProbeProcessWindowProc(HWND hwnd, LPARAM lParam) {
        WindowProbeContext* ctx = (WindowProbeContext*)lParam;
        if (!ctx) return TRUE;

        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != ctx->pid || hwnd == NULL || !IsWindowVisible(hwnd))
            return TRUE;

        ctx->total++;
        DWORD_PTR result = 0;
        LRESULT ok = SendMessageTimeoutW(hwnd, WM_NULL, 0, 0,
            SMTO_ABORTIFHUNG | SMTO_BLOCK, 1000, &result);
        if (ok == 0) {
            ctx->unresponsive++;
            wchar_t title[256] = {};
            wchar_t klass[128] = {};
            GetWindowTextW(hwnd, title, _countof(title));
            GetClassNameW(hwnd, klass, _countof(klass));

            char line[2048] = {};
            sprintf_s(line, "[DIAG][hang-watchdog] unresponsive-window hwnd=%p class='%s' title='%s'",
                hwnd, WideToUtf8(klass).c_str(), WideToUtf8(title).c_str());
            AppendTraceLineDirect(line);
        }
        return TRUE;
    }

    static void ProbeProcessWindowsDirect() {
        WindowProbeContext ctx = {};
        ctx.pid = GetCurrentProcessId();
        EnumWindows(ProbeProcessWindowProc, (LPARAM)&ctx);

        char line[512] = {};
        sprintf_s(line, "[DIAG][hang-watchdog] window-probe total=%d unresponsive=%d",
            ctx.total, ctx.unresponsive);
        AppendTraceLineDirect(line);
    }

    static void DumpStackPointersDirect(CONTEXT* ctx) {
        if (!ctx) return;
#if defined(_M_X64)
        uintptr_t sp = (uintptr_t)ctx->Rsp;
#else
        uintptr_t sp = (uintptr_t)ctx->Esp;
#endif
        char line[1024] = {};
        sprintf_s(line, "[DIAG][crash] stack-scan sp=%p", (void*)sp);
        AppendTraceLineDirect(line);

        for (int i = 0; i < 96; ++i) {
            uintptr_t* slot = (uintptr_t*)(sp + (uintptr_t)i * sizeof(uintptr_t));
            uintptr_t value = 0;
            __try {
                value = *slot;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                break;
            }

            uintptr_t base = 0;
            char module[MAX_PATH * 4] = {};
            if (!ModuleInfoFromAddress((const void*)value, &base, module, sizeof(module)))
                continue;
            sprintf_s(line, "[DIAG][crash] stack[%02d] %p -> %s+0x%IX",
                i, (void*)slot, module[0] ? module : "<module>", value - base);
            AppendTraceLineDirect(line);
        }
    }

    static void LogExceptionDirect(EXCEPTION_POINTERS* info, const char* source) {
        if (!info || !info->ExceptionRecord) return;

        LONG count = InterlockedIncrement(&g_exceptionLogCount);
        if (count > 8 && (!source || strcmp(source, "unhandled") != 0)) return;

        EXCEPTION_RECORD* record = info->ExceptionRecord;
        CONTEXT* ctx = info->ContextRecord;
        uintptr_t address = (uintptr_t)record->ExceptionAddress;
        char formattedAddress[1024] = {};
        FormatAddress(formattedAddress, sizeof(formattedAddress), address);

        char line[2048] = {};
        sprintf_s(line, "[DIAG][crash] source=%s code=0x%08lX flags=0x%08lX address=%s params=%lu",
            source ? source : "unknown", record->ExceptionCode, record->ExceptionFlags,
            formattedAddress, record->NumberParameters);
        AppendTraceLineDirect(line);

        for (DWORD i = 0; i < record->NumberParameters && i < EXCEPTION_MAXIMUM_PARAMETERS; ++i) {
            char paramAddress[1024] = {};
            FormatAddress(paramAddress, sizeof(paramAddress), (uintptr_t)record->ExceptionInformation[i]);
            sprintf_s(line, "[DIAG][crash] param[%lu]=0x%IX %s",
                i, (uintptr_t)record->ExceptionInformation[i], paramAddress);
            AppendTraceLineDirect(line);
        }

        if (ctx) {
#if defined(_M_X64)
            char rip[1024] = {};
            FormatAddress(rip, sizeof(rip), (uintptr_t)ctx->Rip);
            sprintf_s(line, "[DIAG][crash] regs RIP=%s RSP=%p RBP=%p RAX=%p RBX=%p RCX=%p RDX=%p RSI=%p RDI=%p",
                rip, (void*)ctx->Rsp, (void*)ctx->Rbp, (void*)ctx->Rax, (void*)ctx->Rbx,
                (void*)ctx->Rcx, (void*)ctx->Rdx, (void*)ctx->Rsi, (void*)ctx->Rdi);
#else
            char eip[1024] = {};
            FormatAddress(eip, sizeof(eip), (uintptr_t)ctx->Eip);
            sprintf_s(line, "[DIAG][crash] regs EIP=%s ESP=%p EBP=%p EAX=%p EBX=%p ECX=%p EDX=%p ESI=%p EDI=%p",
                eip, (void*)ctx->Esp, (void*)ctx->Ebp, (void*)ctx->Eax, (void*)ctx->Ebx,
                (void*)ctx->Ecx, (void*)ctx->Edx, (void*)ctx->Esi, (void*)ctx->Edi);
#endif
            AppendTraceLineDirect(line);
        }

        DumpRecentBreadcrumbsDirect();
        DumpStackPointersDirect(ctx);
    }

    static LONG WINAPI SfhVectoredExceptionHandler(EXCEPTION_POINTERS* info) {
        if (info && info->ExceptionRecord &&
            IsInterestingException(info->ExceptionRecord->ExceptionCode)) {
            if (InterlockedIncrement(&g_firstChanceExceptionLogCount) > 2)
                return EXCEPTION_CONTINUE_SEARCH;
            LogExceptionDirect(info, "vectored-first-chance");
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }

    static DWORD WINAPI DiagnosticsWatchdogThread(void*) {
        g_diagnosticsThreadId = GetCurrentThreadId();
        for (;;) {
            if (g_diagnosticsStopEvent &&
                WaitForSingleObject(g_diagnosticsStopEvent, 2000) == WAIT_OBJECT_0)
                break;
            if (InterlockedCompareExchange(&g_shutdownRequested, 0, 0) != 0)
                break;
            DWORD now = GetTickCount();

            LONG active = InterlockedCompareExchange(&g_watchdogStageActive, 0, 0);
            if (active) {
                DWORD start = (DWORD)InterlockedCompareExchange(&g_watchdogStageStartTick, 0, 0);
                DWORD elapsed = now - start;
                if (elapsed >= 10000) {
                    DWORD last = (DWORD)InterlockedCompareExchange(&g_watchdogLastReportTick, 0, 0);
                    if (last == 0 || now - last >= 5000) {
                        InterlockedExchange(&g_watchdogLastReportTick, (LONG)now);

                        char stage[512] = {};
                        strncpy_s(stage, g_watchdogStage, _TRUNCATE);
                        char line[1024] = {};
                        sprintf_s(line, "[DIAG][hang-watchdog] stage-active-for=%lums stage='%s' configVersion=%ld font='%s'",
                            elapsed, stage, Config::ConfigVersion, WideToUtf8(Config::ForcedFontNameW).c_str());
                        AppendTraceLineDirect(line);
                        DumpRecentBreadcrumbsDirect();
                    }
                }
            }

            DWORD switchTick = (DWORD)InterlockedCompareExchange(&g_fontSwitchWatchTick, 0, 0);
            if (!switchTick) continue;
            DWORD sinceSwitch = now - switchTick;
            if (sinceSwitch > 60000) {
                InterlockedExchange(&g_fontSwitchWatchTick, 0);
                continue;
            }
            if (sinceSwitch < 10000) continue;

            DWORD lastSwitchReport = (DWORD)InterlockedCompareExchange(&g_fontSwitchWatchLastReportTick, 0, 0);
            if (lastSwitchReport != 0 && now - lastSwitchReport < 5000) continue;
            InterlockedExchange(&g_fontSwitchWatchLastReportTick, (LONG)now);

            char line[1024] = {};
            sprintf_s(line, "[DIAG][hang-watchdog] post-font-switch age=%lums version=%ld font='%s'",
                sinceSwitch, InterlockedCompareExchange(&g_fontSwitchWatchVersion, 0, 0),
                g_fontSwitchWatchFont);
            AppendTraceLineDirect(line);
            ProbeProcessWindowsDirect();
            DumpRecentBreadcrumbsDirect();
        }
        g_diagnosticsThreadId = 0;
        return 0;
    }

    void InstallDiagnostics(HMODULE hModule) {
        if (!Config::EnableDebugLog || IsShuttingDown()) return;
        if (InterlockedCompareExchange(&g_diagnosticsInstalled, 1, 0) != 0)
            return;
        g_diagModule = hModule;
        g_diagnosticsStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        // A process-wide unhandled-exception filter is deliberately not installed:
        // Unity and its crash handler own that contract.  The optional VEH and
        // watchdog are only enabled for explicit debug logging.
        if (Config::EnableDebugLog && !IsShuttingDown()) {
            g_vectoredExceptionHandler = AddVectoredExceptionHandler(1, SfhVectoredExceptionHandler);
            g_diagnosticsThread = CreateThread(NULL, 0, DiagnosticsWatchdogThread,
                NULL, 0, &g_diagnosticsThreadId);
            if (!g_diagnosticsThread) g_diagnosticsThreadId = 0;
        }
        Breadcrumb("diagnostics installed module=%p", hModule);
        Trace("[DIAG] diagnostics installed module=%p", hModule);
    }

    void RequestShutdown() {
        InterlockedExchange(&g_shutdownRequested, 1);
        HANDLE event = g_diagnosticsStopEvent;
        if (event) SetEvent(event);
    }

    bool IsShuttingDown() {
        return InterlockedCompareExchange(&g_shutdownRequested, 0, 0) != 0;
    }

    void ShutdownDiagnostics(bool processTerminating) {
        RequestShutdown();

        HANDLE thread = (HANDLE)InterlockedExchangePointer(
            reinterpret_cast<PVOID volatile*>(&g_diagnosticsThread), NULL);
        if (thread && !processTerminating &&
            GetCurrentThreadId() != g_diagnosticsThreadId) {
            WaitForSingleObject(thread, 1500);
        }
        if (thread) CloseHandle(thread);

        if (!processTerminating && g_vectoredExceptionHandler) {
            RemoveVectoredExceptionHandler(g_vectoredExceptionHandler);
        }
        g_vectoredExceptionHandler = NULL;

        HANDLE event = (HANDLE)InterlockedExchangePointer(
            reinterpret_cast<PVOID volatile*>(&g_diagnosticsStopEvent), NULL);
        if (event) {
            SetEvent(event);
            CloseHandle(event);
        }
        g_diagnosticsThreadId = 0;
        g_diagModule = NULL;
    }

    void Breadcrumb(const char* format, ...) {
        char text[512] = {};
        if (format) {
            va_list args;
            va_start(args, format);
            vsprintf_s(text, format, args);
            va_end(args);
        }

        LONG seq = InterlockedIncrement(&g_breadcrumbSeq);
        DiagnosticBreadcrumb& b = g_breadcrumbs[seq & 63];
        b.tick = GetTickCount();
        b.tid = GetCurrentThreadId();
        strncpy_s(b.text, text, _TRUNCATE);
        b.seq = seq;
    }

    void BeginWatchdogStage(const char* format, ...) {
        char text[512] = {};
        if (format) {
            va_list args;
            va_start(args, format);
            vsprintf_s(text, format, args);
            va_end(args);
        }
        strncpy_s(g_watchdogStage, text, _TRUNCATE);
        InterlockedExchange(&g_watchdogStageStartTick, (LONG)GetTickCount());
        InterlockedExchange(&g_watchdogLastReportTick, 0);
        InterlockedExchange(&g_watchdogStageActive, 1);
        Breadcrumb("begin %s", text);
    }

    void EndWatchdogStage(const char* label) {
        char stage[512] = {};
        strncpy_s(stage, g_watchdogStage, _TRUNCATE);
        DWORD start = (DWORD)InterlockedCompareExchange(&g_watchdogStageStartTick, 0, 0);
        DWORD elapsed = start ? GetTickCount() - start : 0;
        InterlockedExchange(&g_watchdogStageActive, 0);
        Breadcrumb("end %s elapsed=%lums%s%s",
            stage, elapsed, label ? " label=" : "", label ? label : "");
    }

    void MarkFontSwitchForWatchdog(LONG version, const wchar_t* fontName) {
        std::string font = WideToUtf8(fontName);
        strncpy_s(g_fontSwitchWatchFont, font.c_str(), _TRUNCATE);
        InterlockedExchange(&g_fontSwitchWatchVersion, version);
        InterlockedExchange(&g_fontSwitchWatchLastReportTick, 0);
        InterlockedExchange(&g_fontSwitchWatchTick, (LONG)GetTickCount());
        Breadcrumb("font-switch-watch version=%ld font='%s'", version, g_fontSwitchWatchFont);
    }

    static bool LooksLikeInternalMetricCloneName(const std::wstring& name) {
        if (name.size() != 6 && name.size() != 10) return false;
        if (name[0] != L'S' || name[1] != L'F') return false;
        for (size_t i = 2; i < name.size(); ++i) {
            wchar_t ch = name[i];
            bool hex =
                (ch >= L'0' && ch <= L'9') ||
                (ch >= L'A' && ch <= L'F') ||
                (ch >= L'a' && ch <= L'f');
            if (!hex) return false;
        }
        return true;
    }

    void Log(const char* format, ...) {
        char buffer[2048];
        va_list args;
        va_start(args, format);
        vsprintf_s(buffer, format, args);
        va_end(args);
        OutputDebugStringA(buffer);
    }

    void LogW(const wchar_t* format, ...) {
        wchar_t buffer[2048];
        va_list args;
        va_start(args, format);
        vswprintf_s(buffer, format, args);
        va_end(args);
        OutputDebugStringW(buffer);
    }

    void Trace(const char* format, ...) {
        char message[3072];
        va_list args;
        va_start(args, format);
        vsprintf_s(message, format, args);
        va_end(args);

        std::lock_guard<std::mutex> lock(g_traceFileMutex);

        wchar_t path[MAX_PATH] = {};
        if (!EnsureTraceSessionInitializedLocked(path, _countof(path))) return;

        HANDLE hFile = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return;

        SYSTEMTIME st = {};
        GetLocalTime(&st);

        char line[4096];
        sprintf_s(line, "[%02u:%02u:%02u.%03u][pid=%lu][tid=%lu] %s\r\n",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            GetCurrentProcessId(), GetCurrentThreadId(), message);

        DWORD written = 0;
        WriteFile(hFile, line, (DWORD)strlen(line), &written, NULL);
        CloseHandle(hFile);
    }

    BOOL LoadCustomFont(HMODULE hModule) {
        if (!Config::FontFileName[0]) return FALSE;

        wchar_t dir[MAX_PATH];
        GetModuleFileNameW(hModule, dir, MAX_PATH);
        PathRemoveFileSpecW(dir);

        wchar_t path[MAX_PATH];
        wcscpy_s(path, dir);
        PathAppendW(path, Config::FontFileName);

        auto findFontFile = [&](const wchar_t* pattern) -> bool {
            wchar_t search[MAX_PATH];
            wcscpy_s(search, dir);
            PathAppendW(search, pattern);

            WIN32_FIND_DATAW data = {};
            HANDLE hFind = FindFirstFileW(search, &data);
            if (hFind == INVALID_HANDLE_VALUE) return false;
            FindClose(hFind);
            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return false;

            wcscpy_s(path, dir);
            PathAppendW(path, data.cFileName);
            wcsncpy_s(Config::FontFileName, data.cFileName, MAX_PATH - 1);
            return true;
        };

        if (!PathFileExistsW(path)) {
            if (!findFontFile(L"galgame*.ttf") && !findFontFile(L"*.ttf")) {
                LogW(L"[Font] Font file not found: %s", path);
                Trace("[DEBUG][FontFile] not-found configured='%s'", WideToUtf8(Config::FontFileName).c_str());
                return FALSE;
            }
        }

        Trace("[DEBUG][FontFile] selected='%s'", WideToUtf8(path).c_str());

        // AddFontResourceExW makes the font enumerable and more robustly available to GDI.
        // This is often more reliable than AddFontMemResourceEx for many game engines.
        BeginWatchdogStage("LoadCustomFont AddFontResourceExW path='%s'", WideToUtf8(path).c_str());
        int resCount = AddFontResourceExW(path, FR_PRIVATE, NULL);
        EndWatchdogStage(resCount > 0 ? "ok" : "failed");
        if (resCount > 0) {
            LogW(L"[Font] Loaded custom font via AddFontResourceExW: %s (count=%d)", path, resCount);
            Trace("[DEBUG][FontFile] AddFontResourceExW ok count=%d", resCount);
            if (!Config::EnableCodepageSpoof && !Config::EnableFontVerticalMetrics) return TRUE;
        } else {
            LogW(L"[Font] AddFontResourceExW failed (err=%lu) for: %s", GetLastError(), path);
            Trace("[DEBUG][FontFile] AddFontResourceExW failed err=%lu", GetLastError());
        }

        // If codepage spoofing is enabled, we still want to load a patched memory version.
        // Read file into memory
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return (resCount > 0);
        
        std::streamoff fileSize = file.tellg();
        if (fileSize <= 0 ||
            fileSize > static_cast<std::streamoff>(std::numeric_limits<size_t>::max()) ||
            fileSize > static_cast<std::streamoff>(std::numeric_limits<std::streamsize>::max())) {
            return (resCount > 0);
        }

        size_t size = static_cast<size_t>(fileSize);
        file.seekg(0, std::ios::beg);
        
        std::vector<BYTE> fontData(size);
        if (!file.read((char*)fontData.data(), static_cast<std::streamsize>(size))) return (resCount > 0);
        file.close();

        if (FontPatcher::IsFontCollection(fontData.data(), fontData.size())) {
            std::vector<BYTE> extracted;
            const wchar_t* faceHint = Config::ForcedFontNameW[0] ? Config::ForcedFontNameW : NULL;
            if (FontPatcher::ExtractFontFromCollectionByName(fontData, faceHint, extracted)) {
                Trace("[DEBUG][FontFile] extracted TTC face='%s' bytes=%lu",
                    WideToUtf8(faceHint ? faceHint : L"").c_str(), (unsigned long)extracted.size());
                fontData.swap(extracted);
            } else {
                Trace("[DEBUG][FontFile] TTC extract failed face='%s'; memory patch skipped",
                    WideToUtf8(faceHint ? faceHint : L"").c_str());
                return (resCount > 0);
            }
        }

        // Patch OS/2 table if needed
        if (Config::EnableCodepageSpoof) {
            int bitToSet = FontPatcher::CodePageRangeBitForCharset(Config::SpoofToCharset);
            if (FontPatcher::PatchOS2CodePageRangeForCharset(fontData, Config::SpoofToCharset)) {
                LogW(L"[Font] Patched font OS/2 table for charset spoofing: charset=%lu bit=%d.",
                    Config::SpoofToCharset, bitToSet);
            }
        }

        if (Config::EnableFontVerticalMetrics) {
            int lineGap = Config::EnableFontLineSpacing ? Config::FontLineSpacing : 0;
            if (FontPatcher::PatchVerticalMetrics(fontData,
                Config::FontAscentPermille, Config::FontDescentPermille, lineGap)) {
                LogW(L"[Font] Patched font hhea/OS/2 vertical metrics in memory.");
                Trace("[DEBUG][FontFile] vertical metrics patched asc=%d desc=%d gap=%d",
                    Config::FontAscentPermille, Config::FontDescentPermille, lineGap);
            }
        }

        DWORD numFonts = 0;
        BeginWatchdogStage("LoadCustomFont AddFontMemResourceEx bytes=%lu path='%s'",
            (unsigned long)fontData.size(), WideToUtf8(path).c_str());
        HANDLE hFont = AddFontMemResourceEx(fontData.data(), (DWORD)fontData.size(), NULL, &numFonts);
        if (hFont) {
            EndWatchdogStage("ok");
            LogW(L"[Font] Loaded custom font from memory: %s (num=%lu)", path, numFonts);
            Trace("[DEBUG][FontFile] AddFontMemResourceEx ok num=%lu", numFonts);
            return TRUE;
        }
        EndWatchdogStage("failed");

        Trace("[DEBUG][FontFile] AddFontMemResourceEx failed err=%lu fallbackResCount=%d", GetLastError(), resCount);
        return (resCount > 0);
    }

    // --- UTF-8 helpers ---
    static std::string WideToUtf8(const wchar_t* w) {
        if (!w || !w[0]) return "";
        int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
        if (len <= 0) return "";
        std::vector<char> buf(len);
        WideCharToMultiByte(CP_UTF8, 0, w, -1, buf.data(), len, NULL, NULL);
        return std::string(buf.data());
    }

    static std::wstring Utf8ToWide(const std::string& s) {
        if (s.empty()) return L"";
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
        if (len <= 0) return L"";
        std::vector<wchar_t> buf(len);
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, buf.data(), len);
        return std::wstring(buf.data());
    }

    // Get INI file path (next to the DLL)
    static std::wstring GetIniPath(HMODULE hModule) {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(hModule, path, MAX_PATH);
        PathRemoveFileSpecW(path);
        PathAppendW(path, L"FontHook.ini");
        return path;
    }

    struct IniEntry {
        std::string key;
        std::string value;
    };

    using IniEntries = std::vector<IniEntry>;

    static const std::string* FindIniValue(const IniEntries& entries, const char* key) {
        for (const IniEntry& entry : entries) {
            if (entry.key == key) return &entry.value;
        }
        return nullptr;
    }

    static int ReadIniInt(const IniEntries& entries, const char* key, int defaultValue) {
        const std::string* value = FindIniValue(entries, key);
        return value ? atoi(value->c_str()) : defaultValue;
    }

    // Configuration files are small and read once. A compact linear table avoids
    // pulling the std::map tree implementation into every release binary.
    static IniEntries ParseIni(const std::string& content) {
        IniEntries entries;
        entries.reserve(80);
        size_t pos = 0;
        while (pos < content.size()) {
            size_t eol = content.find('\n', pos);
            if (eol == std::string::npos) eol = content.size();
            std::string line = content.substr(pos, eol - pos);
            pos = eol + 1;
            // trim \r
            if (!line.empty() && line.back() == '\r') line.pop_back();
            // skip empty, comments, section headers
            if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[') continue;
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = line.substr(0, eq);
            std::string value = line.substr(eq + 1);
            bool replaced = false;
            for (IniEntry& entry : entries) {
                if (entry.key != key) continue;
                entry.value = std::move(value);
                replaced = true;
                break;
            }
            if (!replaced) entries.push_back({ std::move(key), std::move(value) });
        }
        return entries;
    }

    static __declspec(noinline) void AppendIniText(std::string& output,
        const char* key, const char* value) {
        output.append(key);
        output.push_back('=');
        output.append(value ? value : "");
        output.append("\r\n");
    }

    static __declspec(noinline) void AppendIniText(std::string& output,
        const char* key, const std::string& value) {
        output.append(key);
        output.push_back('=');
        output.append(value);
        output.append("\r\n");
    }

    static __declspec(noinline) void AppendIniBool(std::string& output,
        const char* key, bool value) {
        AppendIniText(output, key, value ? "1" : "0");
    }

    static __declspec(noinline) void AppendIniInt(std::string& output,
        const char* key, int value) {
        char buffer[32];
        sprintf_s(buffer, "%d", value);
        AppendIniText(output, key, buffer);
    }

    static __declspec(noinline) void AppendIniUnsigned(std::string& output,
        const char* key, DWORD value) {
        char buffer[32];
        sprintf_s(buffer, "%lu", value);
        AppendIniText(output, key, buffer);
    }

    void SaveConfig(HMODULE hModule) {
        std::wstring iniPath = GetIniPath(hModule);

        std::string out;
        out.reserve(4096);
        out.append("[FontHook]\r\n");
        AppendIniText(out, "FontNameW", WideToUtf8(Config::ForcedFontNameW));

        wchar_t fontNameAW[LF_FACESIZE];
        MultiByteToWideChar(CP_ACP, 0, Config::ForcedFontNameA, -1, fontNameAW, LF_FACESIZE);
        AppendIniText(out, "FontNameA", WideToUtf8(fontNameAW));

        AppendIniBool(out, "EnableFontHook", Config::EnableFontHook);
        AppendIniBool(out, "EnableFaceNameReplace", Config::EnableFaceNameReplace);
        AppendIniBool(out, "EnableCharsetReplace", Config::EnableCharsetReplace);
        AppendIniUnsigned(out, "ForcedCharset", Config::ForcedCharset);

        AppendIniBool(out, "EnableCodepageSpoof", Config::EnableCodepageSpoof);
        AppendIniBool(out, "EnableCodepageRuntimeReplace", Config::EnableCodepageRuntimeReplace);
        AppendIniBool(out, "EnableCodepageRedirect", Config::EnableCodepageRedirect);
        AppendIniUnsigned(out, "CodepageRedirectFrom", Config::CodepageRedirectFrom);
        AppendIniUnsigned(out, "CodepageRedirectTo", Config::CodepageRedirectTo);
        AppendIniBool(out, "EnableTextSubstitution", Config::EnableTextSubstitution);
        AppendIniBool(out, "PickerShowOnStartup", Config::PickerShowOnStartup);
        AppendIniInt(out, "TextSubstitutionMode", Config::TextSubstitutionMode);
        AppendIniUnsigned(out, "TextSubstitutionCodepage", Config::TextSubstitutionCodepage);
        AppendIniUnsigned(out, "SpoofFromCharset", Config::SpoofFromCharset);
        AppendIniUnsigned(out, "SpoofToCharset", Config::SpoofToCharset);
        AppendIniBool(out, "EnableDebugLog", Config::EnableDebugLog);
        AppendIniInt(out, "DebugSlowMs", Config::DebugSlowMs);
        AppendIniInt(out, "DebugTraceSampleLimit", Config::DebugTraceSampleLimit);
        AppendIniInt(out, "DebugPickerThreadLogLimit", Config::DebugPickerThreadLogLimit);

        AppendIniBool(out, "CompatSkipDrawTextA", Config::CompatSkipDrawTextA);
        AppendIniBool(out, "CompatSkipFontDataQueries", Config::CompatSkipFontDataQueries);
        AppendIniBool(out, "CompatSelectObjectTrackedOnly", Config::CompatSelectObjectTrackedOnly);
        AppendIniBool(out, "CompatHookCreateFontW", Config::CompatHookCreateFontW);
        AppendIniBool(out, "CompatHookCreateFontIndirectW", Config::CompatHookCreateFontIndirectW);
        AppendIniBool(out, "CompatHookGetTextFace", Config::CompatHookGetTextFace);
        AppendIniBool(out, "EnableBgiHook", Config::EnableBgiHook);
        AppendIniBool(out, "BgiPatchGdiImports", Config::BgiPatchGdiImports);
        AppendIniBool(out, "BgiClearGlyphCacheOnSwitch", Config::BgiClearGlyphCacheOnSwitch);
        AppendIniBool(out, "EnableArtemisHook", Config::EnableArtemisHook);
        AppendIniBool(out, "ArtemisPatchTables", Config::ArtemisPatchTables);
        AppendIniBool(out, "ArtemisRedirectFontFiles", Config::ArtemisRedirectFontFiles);
        AppendIniBool(out, "ArtemisClearFontCacheOnSwitch", Config::ArtemisClearFontCacheOnSwitch);
        AppendIniText(out, "ArtemisFontPath", WideToUtf8(Config::ArtemisFontPath));
        AppendIniInt(out, "ArtemisFontSize", Config::ArtemisFontSize);
        AppendIniInt(out, "ArtemisRubySize", Config::ArtemisRubySize);
        AppendIniBool(out, "EnableKrkrHook", Config::EnableKrkrHook);
        AppendIniBool(out, "KrkrDisablePrerenderedFonts", Config::KrkrDisablePrerenderedFonts);
        AppendIniBool(out, "EnableEntisHook", Config::EnableEntisHook);
        AppendIniBool(out, "EntisDisableBitmapFonts", Config::EntisDisableBitmapFonts);
        AppendIniBool(out, "EntisRefreshFontOnSwitch", Config::EntisRefreshFontOnSwitch);
        AppendIniBool(out, "EnableSoftpalHook", Config::EnableSoftpalHook);
        AppendIniBool(out, "SoftpalDisableDefaultFontDat", Config::SoftpalDisableDefaultFontDat);
        AppendIniBool(out, "SoftpalForceDefaultOptionToSystemFont", Config::SoftpalForceDefaultOptionToSystemFont);
        AppendIniBool(out, "EnableEscudeHook", Config::EnableEscudeHook);
        AppendIniBool(out, "EscudeVirtualFontConfig", Config::EscudeVirtualFontConfig);
        AppendIniBool(out, "EnableMiraiHook", Config::EnableMiraiHook);
        AppendIniBool(out, "MiraiReplaceFontDataQueries", Config::MiraiReplaceFontDataQueries);
        AppendIniBool(out, "MiraiRedirectFontFiles", Config::MiraiRedirectFontFiles);
        AppendIniBool(out, "MiraiPinFontDataSource", Config::MiraiPinFontDataSource);
        AppendIniBool(out, "EnableMajiroHook", Config::EnableMajiroHook);
        AppendIniBool(out, "MajiroDisableFontCache", Config::MajiroDisableFontCache);
        AppendIniBool(out, "EnableDxLibHook", Config::EnableDxLibHook);
        AppendIniBool(out, "DxLibDisableFontCache", Config::DxLibDisableFontCache);
        AppendIniBool(out, "DxLibReplaceFontDataQueries", Config::DxLibReplaceFontDataQueries);
        AppendIniBool(out, "DxLibClearRuntimeFontCacheOnSwitch", Config::DxLibClearRuntimeFontCacheOnSwitch);
        AppendIniText(out, "DxLibCachedFontNameW", WideToUtf8(Config::DxLibCachedFontNameW));
        AppendIniBool(out, "EnableTinkerBellHook", Config::EnableTinkerBellHook);
        AppendIniUnsigned(out, "YurisAtlasCodepage", Config::YurisAtlasCodepage);
        AppendIniBool(out, "EnableCatSystemUnityHook", Config::EnableCatSystemUnityHook);
        AppendIniBool(out, "EnableTyranoHook", Config::EnableTyranoHook);
        AppendIniBool(out, "TyranoRedirectWebFonts", Config::TyranoRedirectWebFonts);
        AppendIniBool(out, "EnableRenPyHook", Config::EnableRenPyHook);
        AppendIniBool(out, "RenPyRedirectFonts", Config::RenPyRedirectFonts);
        AppendIniBool(out, "RenPyRefreshFontOnSwitch", Config::RenPyRefreshFontOnSwitch);

        AppendIniBool(out, "EnableFontHeightScale", Config::EnableFontHeightScale);
        AppendIniBool(out, "EnableFontWidthScale", Config::EnableFontWidthScale);
        AppendIniBool(out, "EnableFontCharSpacing", Config::EnableFontCharSpacing);
        AppendIniBool(out, "EnableFontVerticalMetrics", Config::EnableFontVerticalMetrics);
        AppendIniBool(out, "EnableFontLineSpacing", Config::EnableFontLineSpacing);
        AppendIniBool(out, "EnableFontWeight", Config::EnableFontWeight);

        AppendIniUnsigned(out, "FontHeightScale1000", (DWORD)(Config::FontHeightScale * 1000.0f));
        AppendIniUnsigned(out, "FontWidthScale1000", (DWORD)(Config::FontWidthScale * 1000.0f));
        AppendIniInt(out, "FontCharSpacing", Config::FontCharSpacing);
        AppendIniInt(out, "FontAscentPermille", Config::FontAscentPermille);
        AppendIniInt(out, "FontDescentPermille", Config::FontDescentPermille);
        AppendIniInt(out, "FontLineSpacing", Config::FontLineSpacing);
        AppendIniInt(out, "FontWeight", Config::FontWeight);

        // Write with UTF-8 BOM
        HANDLE hFile = CreateFileW(iniPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            Log("[Config] Failed to write INI file");
            return;
        }
        DWORD written;
        // UTF-8 BOM
        const BYTE bom[3] = { 0xEF, 0xBB, 0xBF };
        WriteFile(hFile, bom, 3, &written, NULL);
        WriteFile(hFile, out.c_str(), (DWORD)out.size(), &written, NULL);
        CloseHandle(hFile);

        LogW(L"[Config] Saved config to %s (Font: %s)", iniPath.c_str(), Config::ForcedFontNameW);
    }

    bool LoadConfig(HMODULE hModule) {
        std::wstring iniPath = GetIniPath(hModule);

        HANDLE hFile = CreateFileW(iniPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return false;

        DWORD fileSize = GetFileSize(hFile, NULL);
        if (fileSize == 0 || fileSize == INVALID_FILE_SIZE) {
            CloseHandle(hFile);
            return false;
        }

        std::vector<char> buf(fileSize + 1, 0);
        DWORD bytesRead;
        ReadFile(hFile, buf.data(), fileSize, &bytesRead, NULL);
        CloseHandle(hFile);

        std::string content(buf.data(), bytesRead);
        // Skip UTF-8 BOM if present
        if (content.size() >= 3 && (unsigned char)content[0] == 0xEF && (unsigned char)content[1] == 0xBB && (unsigned char)content[2] == 0xBF) {
            content = content.substr(3);
        }

        IniEntries kv = ParseIni(content);
        const std::string* fontNameWValue = FindIniValue(kv, "FontNameW");
        if (!fontNameWValue) return false;

        // Restore font names
        std::wstring fontW = Utf8ToWide(*fontNameWValue);
        if (LooksLikeInternalMetricCloneName(fontW)) {
            Trace("[Config] Ignored saved internal metric clone font name.");
            fontW.clear();
        }
        wcsncpy_s(Config::ForcedFontNameW, fontW.c_str(), LF_FACESIZE - 1);
        wcsncpy_s(Config::SourceFontNameW, fontW.c_str(), LF_FACESIZE - 1);

        if (const std::string* fontNameAValue = FindIniValue(kv, "FontNameA")) {
            std::wstring fontAW = Utf8ToWide(*fontNameAValue);
            if (LooksLikeInternalMetricCloneName(fontAW)) fontAW.clear();
            WideCharToMultiByte(CP_ACP, 0, fontAW.c_str(), -1, Config::ForcedFontNameA, LF_FACESIZE - 1, NULL, NULL);
        } else {
            WideCharToMultiByte(CP_ACP, 0, Config::ForcedFontNameW, -1, Config::ForcedFontNameA, LF_FACESIZE - 1, NULL, NULL);
        }
        Config::ForcedFontNameA[LF_FACESIZE - 1] = '\0';

        Config::EnableFontHook = ReadIniInt(kv, "EnableFontHook", 0) != 0;
        Config::EnableFaceNameReplace = ReadIniInt(kv, "EnableFaceNameReplace", 0) != 0;
        if (Config::ForcedFontNameW[0] == L'\0') {
            Config::EnableFontHook = false;
            Config::EnableFaceNameReplace = false;
        }
        Config::EnableCharsetReplace = ReadIniInt(kv, "EnableCharsetReplace", 0) != 0;
        Config::ForcedCharset = (DWORD)ReadIniInt(kv, "ForcedCharset", DEFAULT_CHARSET);

        Config::EnableCodepageSpoof = ReadIniInt(kv, "EnableCodepageSpoof", 0) != 0;
        Config::EnableCodepageRuntimeReplace = ReadIniInt(kv, "EnableCodepageRuntimeReplace",
            0) != 0;
        int legacyCodepageRedirectEnable =
            (FindIniValue(kv, "FromCodePage") || FindIniValue(kv, "ToCodePage"))
                ? ReadIniInt(kv, "Enable", 0)
                : 0;
        Config::EnableCodepageRedirect = ReadIniInt(kv, "EnableCodepageRedirect",
            ReadIniInt(kv, "CodePageConvertEnable", legacyCodepageRedirectEnable)) != 0;
        Config::CodepageRedirectFrom = (UINT)ReadIniInt(kv, "CodepageRedirectFrom",
            ReadIniInt(kv, "FromCodePage", 932));
        Config::CodepageRedirectTo = (UINT)ReadIniInt(kv, "CodepageRedirectTo",
            ReadIniInt(kv, "ToCodePage", CP_UTF8));
        Config::EnableTextSubstitution = ReadIniInt(kv, "EnableTextSubstitution", 0) != 0;
        Config::PickerShowOnStartup = ReadIniInt(kv, "PickerShowOnStartup", 1) != 0;
        Config::TextSubstitutionMode = ReadIniInt(kv, "TextSubstitutionMode", Config::TextSubstitutionModeJapaneseTraditional);
        if (Config::TextSubstitutionMode < Config::TextSubstitutionModeJapaneseTraditional ||
            Config::TextSubstitutionMode > Config::TextSubstitutionModeSimplifiedToTraditional) {
            Config::TextSubstitutionMode = Config::TextSubstitutionModeJapaneseTraditional;
        }
        {
            int textSubstitutionCodepage = ReadIniInt(kv, "TextSubstitutionCodepage", 932);
            Config::TextSubstitutionCodepage = textSubstitutionCodepage < 0 ? 932 : (UINT)textSubstitutionCodepage;
        }
        if (Config::TextSubstitutionCodepage == 0) Config::TextSubstitutionCodepage = CP_ACP;
        Config::SpoofFromCharset = (DWORD)ReadIniInt(kv, "SpoofFromCharset", GB2312_CHARSET);
        Config::SpoofToCharset = (DWORD)ReadIniInt(kv, "SpoofToCharset", SHIFTJIS_CHARSET);
        // Runtime replacement follows the active spoof mapping. The saved key is
        // still parsed above so existing configuration files remain readable.
        Config::EnableCodepageRuntimeReplace =
            Config::EnableCodepageSpoof &&
            Config::SpoofFromCharset != Config::SpoofToCharset;
        Config::EnableDebugLog = ReadIniInt(kv, "EnableDebugLog", 0) != 0;
        Config::DebugSlowMs = ReadIniInt(kv, "DebugSlowMs", 50);
        if (Config::DebugSlowMs < 0) Config::DebugSlowMs = 0;
        Config::DebugTraceSampleLimit = ReadIniInt(kv, "DebugTraceSampleLimit", 0);
        if (Config::DebugTraceSampleLimit < 0) Config::DebugTraceSampleLimit = 0;
        Config::DebugPickerThreadLogLimit = ReadIniInt(kv, "DebugPickerThreadLogLimit", 0);
        if (Config::DebugPickerThreadLogLimit < 0) Config::DebugPickerThreadLogLimit = 0;

        Config::CompatSkipDrawTextA = ReadIniInt(kv, "CompatSkipDrawTextA", 1) != 0;
        Config::CompatSkipFontDataQueries = ReadIniInt(kv, "CompatSkipFontDataQueries", 1) != 0;
        Config::CompatSelectObjectTrackedOnly = ReadIniInt(kv, "CompatSelectObjectTrackedOnly", 0) != 0;
        Config::CompatHookCreateFontW = ReadIniInt(kv, "CompatHookCreateFontW", 1) != 0;
        Config::CompatHookCreateFontIndirectW = ReadIniInt(kv, "CompatHookCreateFontIndirectW", 1) != 0;
        Config::CompatHookGetTextFace = ReadIniInt(kv, "CompatHookGetTextFace", 0) != 0;
        Config::EnableBgiHook = ReadIniInt(kv, "EnableBgiHook", 1) != 0;
        Config::BgiPatchGdiImports = ReadIniInt(kv, "BgiPatchGdiImports", 1) != 0;
        Config::BgiClearGlyphCacheOnSwitch = ReadIniInt(kv, "BgiClearGlyphCacheOnSwitch", 1) != 0;
        Config::EnableArtemisHook = ReadIniInt(kv, "EnableArtemisHook", 1) != 0;
        Config::ArtemisPatchTables = ReadIniInt(kv, "ArtemisPatchTables", 1) != 0;
        Config::ArtemisRedirectFontFiles = ReadIniInt(kv, "ArtemisRedirectFontFiles", 1) != 0;
        Config::ArtemisClearFontCacheOnSwitch = ReadIniInt(kv, "ArtemisClearFontCacheOnSwitch", 1) != 0;
        Config::EnableKrkrHook = ReadIniInt(kv, "EnableKrkrHook", 1) != 0;
        Config::KrkrDisablePrerenderedFonts = ReadIniInt(kv, "KrkrDisablePrerenderedFonts", 1) != 0;
        Config::EnableEntisHook = ReadIniInt(kv, "EnableEntisHook", 1) != 0;
        Config::EntisDisableBitmapFonts = ReadIniInt(kv, "EntisDisableBitmapFonts", 1) != 0;
        Config::EntisRefreshFontOnSwitch = ReadIniInt(kv, "EntisRefreshFontOnSwitch", 1) != 0;
        Config::EnableSoftpalHook = ReadIniInt(kv, "EnableSoftpalHook", 1) != 0;
        Config::SoftpalDisableDefaultFontDat = ReadIniInt(kv, "SoftpalDisableDefaultFontDat", 1) != 0;
        Config::SoftpalForceDefaultOptionToSystemFont = ReadIniInt(kv, "SoftpalForceDefaultOptionToSystemFont", 1) != 0;
        Config::EnableEscudeHook = ReadIniInt(kv, "EnableEscudeHook", 1) != 0;
        Config::EscudeVirtualFontConfig = ReadIniInt(kv, "EscudeVirtualFontConfig", 1) != 0;
        Config::EnableMiraiHook = ReadIniInt(kv, "EnableMiraiHook", 1) != 0;
        Config::MiraiReplaceFontDataQueries = ReadIniInt(kv, "MiraiReplaceFontDataQueries", 1) != 0;
        Config::MiraiRedirectFontFiles = ReadIniInt(kv, "MiraiRedirectFontFiles", 1) != 0;
        Config::MiraiPinFontDataSource = ReadIniInt(kv, "MiraiPinFontDataSource", 1) != 0;
        Config::EnableMajiroHook = ReadIniInt(kv, "EnableMajiroHook", 1) != 0;
        Config::MajiroDisableFontCache = ReadIniInt(kv, "MajiroDisableFontCache", 1) != 0;
        Config::EnableDxLibHook = ReadIniInt(kv, "EnableDxLibHook", 1) != 0;
        Config::DxLibDisableFontCache = ReadIniInt(kv, "DxLibDisableFontCache", 0) != 0;
        Config::DxLibReplaceFontDataQueries = ReadIniInt(kv, "DxLibReplaceFontDataQueries", 1) != 0;
        Config::DxLibClearRuntimeFontCacheOnSwitch = ReadIniInt(kv, "DxLibClearRuntimeFontCacheOnSwitch", 0) != 0;
        Config::EnableTinkerBellHook = ReadIniInt(kv, "EnableTinkerBellHook", 1) != 0;
        {
            int yurisAtlasCodepage = ReadIniInt(kv, "YurisAtlasCodepage", 0);
            Config::YurisAtlasCodepage = yurisAtlasCodepage > 0 &&
                IsValidCodePage(static_cast<UINT>(yurisAtlasCodepage))
                ? static_cast<UINT>(yurisAtlasCodepage) : 0;
        }
        Config::EnableCatSystemUnityHook = ReadIniInt(kv, "EnableCatSystemUnityHook", 1) != 0;
        Config::EnableTyranoHook = ReadIniInt(kv, "EnableTyranoHook", 1) != 0;
        Config::TyranoRedirectWebFonts = ReadIniInt(kv, "TyranoRedirectWebFonts", 1) != 0;
        Config::EnableRenPyHook = ReadIniInt(kv, "EnableRenPyHook", 1) != 0;
        Config::RenPyRedirectFonts = ReadIniInt(kv, "RenPyRedirectFonts", 1) != 0;
        Config::RenPyRefreshFontOnSwitch = ReadIniInt(kv, "RenPyRefreshFontOnSwitch", 1) != 0;
        if (const std::string* cachedDxLibValue = FindIniValue(kv, "DxLibCachedFontNameW")) {
            std::wstring cachedDxLibFont = Utf8ToWide(*cachedDxLibValue);
            wcsncpy_s(Config::DxLibCachedFontNameW, cachedDxLibFont.c_str(), LF_FACESIZE - 1);
        } else {
            Config::DxLibCachedFontNameW[0] = L'\0';
        }
        if (const std::string* artemisFontValue = FindIniValue(kv, "ArtemisFontPath")) {
            std::wstring artemisFontPath = Utf8ToWide(*artemisFontValue);
            std::wstring normalized = artemisFontPath;
            for (wchar_t& ch : normalized) {
                if (ch == L'/') ch = L'\\';
                if (ch >= L'A' && ch <= L'Z') ch = (wchar_t)(ch - L'A' + L'a');
            }
            if (normalized == L"font\\fonthook.ttf" || normalized == L".\\font\\fonthook.ttf") {
                Config::ArtemisFontPath[0] = L'\0';
                Trace("[Config] Migrated legacy ArtemisFontPath='font/FontHook.ttf' to root virtual default.");
            } else {
                wcsncpy_s(Config::ArtemisFontPath, artemisFontPath.c_str(), MAX_PATH - 1);
            }
        }
        Config::ArtemisFontSize = ReadIniInt(kv, "ArtemisFontSize", 0);
        if (Config::ArtemisFontSize < 0) Config::ArtemisFontSize = 0;
        Config::ArtemisRubySize = ReadIniInt(kv, "ArtemisRubySize", -1);
        if (Config::ArtemisRubySize < -1) Config::ArtemisRubySize = -1;

        Config::EnableFontHeightScale = ReadIniInt(kv, "EnableFontHeightScale", 0) != 0;
        Config::EnableFontWidthScale = ReadIniInt(kv, "EnableFontWidthScale", 0) != 0;
        Config::EnableFontCharSpacing = ReadIniInt(kv, "EnableFontCharSpacing", 0) != 0;
        Config::EnableFontVerticalMetrics = ReadIniInt(kv, "EnableFontVerticalMetrics", 0) != 0;
        Config::EnableFontLineSpacing = ReadIniInt(kv, "EnableFontLineSpacing", 0) != 0;
        Config::EnableFontWeight = ReadIniInt(kv, "EnableFontWeight", 0) != 0;

        Config::FontHeightScale = ReadIniInt(kv, "FontHeightScale1000", 1000) / 1000.0f;
        Config::FontWidthScale = ReadIniInt(kv, "FontWidthScale1000", 1000) / 1000.0f;
        Config::FontCharSpacing = ReadIniInt(kv, "FontCharSpacing", 0);
        Config::FontAscentPermille = ReadIniInt(kv, "FontAscentPermille", 880);
        Config::FontDescentPermille = ReadIniInt(kv, "FontDescentPermille", -120);
        Config::FontLineSpacing = ReadIniInt(kv, "FontLineSpacing", 0);
        Config::FontWeight = ReadIniInt(kv, "FontWeight", 400);

        LogW(L"[Config] Loaded config from %s (Font: %s, Hook: %d)", iniPath.c_str(), Config::ForcedFontNameW, Config::EnableFontHook);
        return true;
    }

    std::string GetFontEnglishName(HFONT hFont) {
        HDC hdc = CreateCompatibleDC(NULL);
        HGDIOBJ old = SelectObject(hdc, hFont);
        std::string result = "";

        // 'name' table tag is 0x656D616E (little-endian for 'name')
        DWORD size = GetFontData(hdc, 0x656D616E, 0, NULL, 0);
        if (size != GDI_ERROR && size > 6) {
            std::vector<BYTE> data(size);
            if (GetFontData(hdc, 0x656D616E, 0, data.data(), size) != GDI_ERROR) {
                // Parse Name Table Header
                unsigned short count = (data[2] << 8) | data[3];
                unsigned short stringOffset = (data[4] << 8) | data[5];

                // Name Records start at byte 6
                int bestScore = -1;
                for (int i = 0; i < count; i++) {
                    size_t recIdx = 6 + i * 12;
                    if (recIdx + 12 > size) break;

                    unsigned short pid = (data[recIdx + 0] << 8) | data[recIdx + 1];
                    unsigned short eid = (data[recIdx + 2] << 8) | data[recIdx + 3];
                    unsigned short lid = (data[recIdx + 4] << 8) | data[recIdx + 5];
                    unsigned short nid = (data[recIdx + 6] << 8) | data[recIdx + 7];
                    unsigned short len = (data[recIdx + 8] << 8) | data[recIdx + 9];
                    unsigned short off = (data[recIdx + 10] << 8) | data[recIdx + 11];

                    if (nid != 1 && nid != 4) continue; // We want Family (1) or Full (4)
                    
                    int score = 0;
                    if (lid == 1033) score += 100; // English (US)
                    else if (pid == 3 && lid == 0) score += 50; // Generic Windows
                    if (nid == 1) score += 10; // Family name preferred

                    if (score > bestScore && (size_t)stringOffset + off + len <= size) {
                        BYTE* sPtr = data.data() + stringOffset + off;
                        if (pid == 3 || pid == 0) { // UTF-16BE
                            std::wstring wstr;
                            for (int j = 0; j < len; j += 2) wstr += (wchar_t)((sPtr[j] << 8) | sPtr[j + 1]);
                            int aLen = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
                            if (aLen > 0) {
                                std::vector<char> aBuffer(aLen);
                                WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, aBuffer.data(), aLen, NULL, NULL);
                                result = aBuffer.data();
                                bestScore = score;
                            }
                        } else if (pid == 1) { // MacRoman (treat as ANSI)
                            result.assign((char*)sPtr, len);
                            bestScore = score;
                        }
                    }
                }
            }
        }

        SelectObject(hdc, old);
        DeleteDC(hdc);
        return result;
    }
}
