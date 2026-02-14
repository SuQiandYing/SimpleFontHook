#include "framework.h"
#include "font_patcher.h"
#include <stdarg.h>
#include <shlwapi.h>
#include <vector>
#include <string>
#include <map>
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

    // --- Parse a simple INI from UTF-8 string into key=value map ---
    static std::map<std::string, std::string> ParseIni(const std::string& content) {
        std::map<std::string, std::string> kv;
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
            std::string val = line.substr(eq + 1);
            kv[key] = val;
        }
        return kv;
    }

    void SaveConfig(HMODULE hModule) {
        std::wstring iniPath = GetIniPath(hModule);

        // Build UTF-8 content
        std::string out;
        out += "[FontHook]\r\n";
        out += "FontNameW=" + WideToUtf8(Config::ForcedFontNameW) + "\r\n";
        // Save FontNameA as wide->utf8 for lossless round-trip
        wchar_t fontNameAW[LF_FACESIZE];
        MultiByteToWideChar(CP_ACP, 0, Config::ForcedFontNameA, -1, fontNameAW, LF_FACESIZE);
        out += "FontNameA=" + WideToUtf8(fontNameAW) + "\r\n";

        auto boolStr = [](bool v) -> const char* { return v ? "1" : "0"; };
        auto dwordStr = [](DWORD v) { char b[32]; sprintf_s(b, "%lu", v); return std::string(b); };
        auto intStr = [](int v) { char b[32]; sprintf_s(b, "%d", v); return std::string(b); };

        out += "EnableFontHook=" + std::string(boolStr(Config::EnableFontHook)) + "\r\n";
        out += "EnableFaceNameReplace=" + std::string(boolStr(Config::EnableFaceNameReplace)) + "\r\n";
        out += "EnableCharsetReplace=" + std::string(boolStr(Config::EnableCharsetReplace)) + "\r\n";
        out += "ForcedCharset=" + dwordStr(Config::ForcedCharset) + "\r\n";

        out += "EnableCodepageSpoof=" + std::string(boolStr(Config::EnableCodepageSpoof)) + "\r\n";
        out += "SpoofFromCharset=" + dwordStr(Config::SpoofFromCharset) + "\r\n";
        out += "SpoofToCharset=" + dwordStr(Config::SpoofToCharset) + "\r\n";

        out += "EnableFontHeightScale=" + std::string(boolStr(Config::EnableFontHeightScale)) + "\r\n";
        out += "EnableFontWidthScale=" + std::string(boolStr(Config::EnableFontWidthScale)) + "\r\n";
        out += "EnableFontWeight=" + std::string(boolStr(Config::EnableFontWeight)) + "\r\n";

        out += "FontHeightScale1000=" + dwordStr((DWORD)(Config::FontHeightScale * 1000.0f)) + "\r\n";
        out += "FontWidthScale1000=" + dwordStr((DWORD)(Config::FontWidthScale * 1000.0f)) + "\r\n";
        out += "FontWeight=" + intStr(Config::FontWeight) + "\r\n";

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

        auto kv = ParseIni(content);
        if (kv.find("FontNameW") == kv.end()) return false;

        // Restore font names
        std::wstring fontW = Utf8ToWide(kv["FontNameW"]);
        wcsncpy_s(Config::ForcedFontNameW, fontW.c_str(), LF_FACESIZE - 1);

        if (kv.find("FontNameA") != kv.end()) {
            std::wstring fontAW = Utf8ToWide(kv["FontNameA"]);
            WideCharToMultiByte(CP_ACP, 0, fontAW.c_str(), -1, Config::ForcedFontNameA, LF_FACESIZE - 1, NULL, NULL);
        } else {
            WideCharToMultiByte(CP_ACP, 0, Config::ForcedFontNameW, -1, Config::ForcedFontNameA, LF_FACESIZE - 1, NULL, NULL);
        }
        Config::ForcedFontNameA[LF_FACESIZE - 1] = '\0';

        auto getInt = [&](const char* key, int def) -> int {
            auto it = kv.find(key);
            if (it != kv.end()) return atoi(it->second.c_str());
            return def;
        };

        Config::EnableFontHook = getInt("EnableFontHook", 0) != 0;
        Config::EnableFaceNameReplace = getInt("EnableFaceNameReplace", 0) != 0;
        Config::EnableCharsetReplace = getInt("EnableCharsetReplace", 0) != 0;
        Config::ForcedCharset = (DWORD)getInt("ForcedCharset", DEFAULT_CHARSET);

        Config::EnableCodepageSpoof = getInt("EnableCodepageSpoof", 0) != 0;
        Config::SpoofFromCharset = (DWORD)getInt("SpoofFromCharset", SHIFTJIS_CHARSET);
        Config::SpoofToCharset = (DWORD)getInt("SpoofToCharset", GB2312_CHARSET);

        Config::EnableFontHeightScale = getInt("EnableFontHeightScale", 0) != 0;
        Config::EnableFontWidthScale = getInt("EnableFontWidthScale", 0) != 0;
        Config::EnableFontWeight = getInt("EnableFontWeight", 0) != 0;

        Config::FontHeightScale = getInt("FontHeightScale1000", 1000) / 1000.0f;
        Config::FontWidthScale = getInt("FontWidthScale1000", 1000) / 1000.0f;
        Config::FontWeight = getInt("FontWeight", 400);

        LogW(L"[Config] Loaded config from %s (Font: %s, Hook: %d)", iniPath.c_str(), Config::ForcedFontNameW, Config::EnableFontHook);
        return Config::EnableFontHook;
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
