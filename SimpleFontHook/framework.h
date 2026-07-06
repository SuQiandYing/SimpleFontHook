#pragma once
#define NOMINMAX
#include <windows.h>
#include <string>

namespace Config {
    enum TextSubstitutionModeValue : int {
        TextSubstitutionModeJapaneseTraditional = 0,
        TextSubstitutionModeTraditionalToSimplified = 1,
        TextSubstitutionModeSimplifiedToTraditional = 2,
    };

    extern bool EnableFontHook;
    extern bool EnableFaceNameReplace;
    extern bool EnableCharsetReplace;
    extern bool EnableFontHeightScale;
    extern bool EnableFontWidthScale;
    extern bool EnableFontCharSpacing;
    extern bool EnableFontVerticalMetrics;
    extern bool EnableFontLineSpacing;
    extern bool EnableFontWeight;
    extern bool EnableCodepageSpoof;
    extern bool EnableCodepageRuntimeReplace;
    extern bool EnableCodepageRedirect;
    extern bool EnableTextSubstitution;
    extern bool PickerShowOnStartup;
    extern int TextSubstitutionMode;
    extern UINT TextSubstitutionCodepage;
    extern bool EnableDebugLog;
    extern bool CompatSkipDrawTextA;
    extern bool CompatSkipFontDataQueries;
    extern bool CompatSelectObjectTrackedOnly;
    extern bool CompatHookCreateFontW;
    extern bool CompatHookCreateFontIndirectW;
    extern bool CompatHookGetTextFace;
    extern bool EnableArtemisHook;
    extern bool ArtemisPatchTables;
    extern bool ArtemisRedirectFontFiles;
    extern bool ArtemisClearFontCacheOnSwitch;
    extern bool EnableKrkrHook;
    extern bool KrkrDisablePrerenderedFonts;
    extern bool EnableSoftpalHook;
    extern bool SoftpalDisableDefaultFontDat;
    extern bool SoftpalForceDefaultOptionToSystemFont;
    extern bool EnableEscudeHook;
    extern bool EscudeVirtualFontConfig;
    extern bool EnableMiraiHook;
    extern bool MiraiReplaceFontDataQueries;
    extern bool MiraiRedirectFontFiles;
    extern bool MiraiPinFontDataSource;
    extern bool EnableMajiroHook;
    extern bool MajiroDisableFontCache;
    extern bool EnableDxLibHook;
    extern bool DxLibDisableFontCache;
    extern bool DxLibReplaceFontDataQueries;
    extern bool DxLibClearRuntimeFontCacheOnSwitch;
    extern wchar_t DxLibCachedFontNameW[LF_FACESIZE];

    extern wchar_t ForcedFontNameW[LF_FACESIZE];
    extern char ForcedFontNameA[LF_FACESIZE];
    extern DWORD ForcedCharset;
    extern DWORD DetectedCharset;  // auto-detected from game's first CreateFont call
    extern float FontHeightScale;
    extern float FontWidthScale;
    extern int FontCharSpacing;
    extern int FontAscentPermille;
    extern int FontDescentPermille;
    extern int FontLineSpacing;
    extern int FontWeight;
    extern wchar_t FontFileName[MAX_PATH];
    extern DWORD SpoofFromCharset;
    extern DWORD SpoofToCharset;
    extern UINT CodepageRedirectFrom;
    extern UINT CodepageRedirectTo;
    extern volatile LONG ConfigVersion;
    extern volatile LONG NeedFontReload;
    extern int DebugSlowMs;
    extern int DebugTraceSampleLimit;
    extern int DebugPickerThreadLogLimit;
    extern wchar_t ArtemisFontPath[MAX_PATH];
    extern int ArtemisFontSize;
    extern int ArtemisRubySize;
}

namespace Utils {
    void Log(const char* format, ...);
    void LogW(const wchar_t* format, ...);
    void Trace(const char* format, ...);
    void InstallDiagnostics(HMODULE hModule);
    void Breadcrumb(const char* format, ...);
    void BeginWatchdogStage(const char* format, ...);
    void EndWatchdogStage(const char* label = nullptr);
    void MarkFontSwitchForWatchdog(LONG version, const wchar_t* fontName);
    BOOL LoadCustomFont(HMODULE hModule);
    std::string GetFontEnglishName(HFONT hFont);
    void SaveConfig(HMODULE hModule);
    bool LoadConfig(HMODULE hModule);
}

namespace FontHooks {
    void NotifyConfigChanged(LONG version);
}
