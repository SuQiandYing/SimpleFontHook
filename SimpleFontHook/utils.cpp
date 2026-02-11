#include "framework.h"
#include "font_patcher.h"
#include <stdarg.h>
#include <shlwapi.h>
#include <vector>
#include <fstream>

#pragma comment(lib, "shlwapi.lib")

namespace Config {
    bool EnableFontHook = false;
    bool EnableFaceNameReplace = false;
    bool EnableCharsetReplace = false;
    bool EnableFontHeightScale = false;
    bool EnableFontWidthScale = false;
    bool EnableFontWeight = false;
    bool EnableCodepageSpoof = false;
    wchar_t ForcedFontNameW[LF_FACESIZE] = L"galgame";
    char ForcedFontNameA[LF_FACESIZE] = "galgame";
    DWORD ForcedCharset = DEFAULT_CHARSET;
    DWORD DetectedCharset = DEFAULT_CHARSET;
    float FontHeightScale = 1.0f;
    float FontWidthScale = 1.0f;
    int FontWeight = 400;
    wchar_t FontFileName[MAX_PATH] = L"galgame.ttf";
    DWORD SpoofFromCharset = SHIFTJIS_CHARSET;   // 128
    DWORD SpoofToCharset = GB2312_CHARSET;        // 134
    volatile LONG ConfigVersion = 0;
    volatile LONG NeedFontReload = 0;
}

namespace Utils {
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

    BOOL LoadCustomFont(HMODULE hModule) {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(hModule, path, MAX_PATH);
        PathRemoveFileSpecW(path);
        PathAppendW(path, Config::FontFileName);

        if (!PathFileExistsW(path)) {
            LogW(L"[Font] Font file not found: %s", path);
            return FALSE;
        }

        // AddFontResourceExW makes the font enumerable and more robustly available to GDI.
        // This is often more reliable than AddFontMemResourceEx for many game engines.
        int resCount = AddFontResourceExW(path, FR_PRIVATE, NULL);
        if (resCount > 0) {
            LogW(L"[Font] Loaded custom font via AddFontResourceExW: %s (count=%d)", path, resCount);
            if (!Config::EnableCodepageSpoof) return TRUE;
        } else {
            LogW(L"[Font] AddFontResourceExW failed (err=%lu) for: %s", GetLastError(), path);
        }

        // If codepage spoofing is enabled, we still want to load a patched memory version.
        // Read file into memory
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return (resCount > 0);
        
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<BYTE> fontData(size);
        if (!file.read((char*)fontData.data(), size)) return (resCount > 0);
        file.close();

        // Patch OS/2 table if needed
        if (Config::EnableCodepageSpoof) {
            int bitToSet = -1;
            if (Config::SpoofFromCharset == SHIFTJIS_CHARSET) bitToSet = 17;
            else if (Config::SpoofFromCharset == GB2312_CHARSET) bitToSet = 18;
            
            if (bitToSet != -1) {
                if (FontPatcher::PatchOS2CodePageRange(fontData, bitToSet)) {
                    LogW(L"[Font] Patched font OS/2 table bit %d for charset spoofing.", bitToSet);
                }
            }
        }

        DWORD numFonts = 0;
        HANDLE hFont = AddFontMemResourceEx(fontData.data(), (DWORD)fontData.size(), NULL, &numFonts);
        if (hFont) {
            LogW(L"[Font] Loaded custom font from memory: %s (num=%lu)", path, numFonts);
            return TRUE;
        }

        return (resCount > 0);
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
