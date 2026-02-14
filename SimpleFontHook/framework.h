#pragma once
#define NOMINMAX
#include <windows.h>
#include <string>

namespace Config {
    extern bool EnableFontHook;
    extern bool EnableFaceNameReplace;
    extern bool EnableCharsetReplace;
    extern bool EnableFontHeightScale;
    extern bool EnableFontWidthScale;
    extern bool EnableFontWeight;
    extern bool EnableCodepageSpoof;

    extern wchar_t ForcedFontNameW[LF_FACESIZE];
    extern char ForcedFontNameA[LF_FACESIZE];
    extern DWORD ForcedCharset;
    extern DWORD DetectedCharset;  // auto-detected from game's first CreateFont call
    extern float FontHeightScale;
    extern float FontWidthScale;
    extern int FontWeight;
    extern wchar_t FontFileName[MAX_PATH];
    extern DWORD SpoofFromCharset;
    extern DWORD SpoofToCharset;
    extern volatile LONG ConfigVersion;
    extern volatile LONG NeedFontReload;
}

namespace Utils {
    void Log(const char* format, ...);
    void LogW(const wchar_t* format, ...);
    BOOL LoadCustomFont(HMODULE hModule);
    std::string GetFontEnglishName(HFONT hFont);
    void SaveConfig(HMODULE hModule);
    bool LoadConfig(HMODULE hModule);
}
