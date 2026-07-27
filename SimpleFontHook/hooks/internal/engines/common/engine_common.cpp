#include "engine_common.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <cwctype>
#include <mutex>
#include <psapi.h>
#include <shlwapi.h>
#include <vector>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shlwapi.lib")

namespace EngineCommon {
namespace {

thread_local unsigned g_internalFileQueryDepth = 0;

class InternalFileQueryScope {
public:
    InternalFileQueryScope() { ++g_internalFileQueryDepth; }
    ~InternalFileQueryScope() { --g_internalFileQueryDepth; }
    InternalFileQueryScope(const InternalFileQueryScope&) = delete;
    InternalFileQueryScope& operator=(const InternalFileQueryScope&) = delete;
};

bool IsReadableProtection(DWORD protection) {
    if ((protection & (PAGE_GUARD | PAGE_NOACCESS)) != 0) return false;
    switch (protection & 0xFF) {
    case PAGE_READONLY:
    case PAGE_READWRITE:
    case PAGE_WRITECOPY:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
        return true;
    default:
        return false;
    }
}

bool QueryModuleRange(HMODULE module, const BYTE** begin, const BYTE** end) {
    if (!module || !begin || !end) return false;
    MODULEINFO info = {};
    if (!GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info)) ||
        !info.lpBaseOfDll || info.SizeOfImage == 0) {
        return false;
    }
    *begin = static_cast<const BYTE*>(info.lpBaseOfDll);
    *end = *begin + info.SizeOfImage;
    return *end > *begin;
}

bool ReadableRangeContains(const BYTE* begin, const BYTE* end,
    const BYTE* marker, size_t markerLength) {
    if (!begin || !end || !marker || markerLength == 0 || begin >= end) return false;

    std::array<size_t, 256> skip = {};
    skip.fill(markerLength);
    for (size_t i = 0; i + 1 < markerLength; ++i) {
        skip[marker[i]] = markerLength - i - 1;
    }

    const BYTE* cursor = begin;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION memory = {};
        if (VirtualQuery(cursor, &memory, sizeof(memory)) != sizeof(memory)) break;

        const BYTE* regionBegin = (std::max)(cursor,
            static_cast<const BYTE*>(memory.BaseAddress));
        const BYTE* rawRegionEnd = static_cast<const BYTE*>(memory.BaseAddress) +
            memory.RegionSize;
        const BYTE* regionEnd = (std::min)(end, rawRegionEnd);
        if (regionEnd <= cursor) break;

        if (memory.State == MEM_COMMIT && IsReadableProtection(memory.Protect) &&
            static_cast<size_t>(regionEnd - regionBegin) >= markerLength) {
            const BYTE* candidate = regionBegin;
            const BYTE* last = regionEnd - markerLength;
            while (candidate <= last) {
                BYTE tail = candidate[markerLength - 1];
                if (tail == marker[markerLength - 1] &&
                    memcmp(candidate, marker, markerLength) == 0) {
                    return true;
                }
                candidate += skip[tail];
            }
        }
        cursor = regionEnd;
    }
    return false;
}

bool ModuleContainsBytes(HMODULE module, const void* marker, size_t markerLength) {
    const BYTE* begin = nullptr;
    const BYTE* end = nullptr;
    return QueryModuleRange(module, &begin, &end) &&
        ReadableRangeContains(begin, end, static_cast<const BYTE*>(marker), markerLength);
}

bool FileStartsWithAnyAscii(const std::wstring& path,
    const char* const* magics, size_t magicCount) {
    if (path.empty() || !magics || magicCount == 0) return false;
    InternalFileQueryScope queryScope;

    size_t maximumLength = 0;
    for (size_t i = 0; i < magicCount; ++i) {
        if (magics[i]) maximumLength = (std::max)(maximumLength, strlen(magics[i]));
    }
    if (maximumLength == 0 || maximumLength > 4096) return false;

    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;

    std::vector<BYTE> bytes(maximumLength);
    DWORD read = 0;
    BOOL ok = ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
    CloseHandle(file);
    if (!ok) return false;

    for (size_t i = 0; i < magicCount; ++i) {
        if (!magics[i]) continue;
        size_t length = strlen(magics[i]);
        if (length <= read && memcmp(bytes.data(), magics[i], length) == 0) return true;
    }
    return false;
}

std::wstring NormalizeFontAlias(const std::wstring& value) {
    std::wstring normalized;
    normalized.reserve(value.size());
    bool insideQualifier = false;
    for (wchar_t ch : value) {
        if (ch == L'(' || ch == 0xFF08) {
            insideQualifier = true;
            continue;
        }
        if (ch == L')' || ch == 0xFF09) {
            insideQualifier = false;
            continue;
        }
        if (!insideQualifier && iswalnum(ch))
            normalized.push_back(static_cast<wchar_t>(towlower(ch)));
    }
    return normalized;
}

std::wstring FontPathFromRegistryValue(const wchar_t* value) {
    if (!value || !value[0]) return L"";
    wchar_t expanded[MAX_PATH] = {};
    if (wcschr(value, L'%') &&
        ExpandEnvironmentStringsW(value, expanded, _countof(expanded)) > 0 &&
        expanded[0]) {
        value = expanded;
    }
    std::wstring path = value;
    if (PathIsRelativeW(path.c_str())) {
        path = WindowsFontsDirectory() + L"\\" + path;
    }
    return IsFile(path) ? path : L"";
}

void SearchFontRegistry(HKEY rootKey, REGSAM flags, const std::wstring& targetAlias,
    std::wstring* bestPath, DWORD* bestScore) {
    if (!bestPath || !bestScore) return;
    HKEY key = nullptr;
    if (RegOpenKeyExW(rootKey,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 0,
        KEY_READ | flags, &key) != ERROR_SUCCESS) {
        return;
    }

    for (DWORD index = 0;; ++index) {
        wchar_t valueName[512] = {};
        BYTE valueData[MAX_PATH * sizeof(wchar_t)] = {};
        DWORD nameLength = _countof(valueName);
        DWORD dataLength = sizeof(valueData);
        DWORD type = 0;
        LONG status = RegEnumValueW(key, index, valueName, &nameLength, nullptr,
            &type, valueData, &dataLength);
        if (status == ERROR_NO_MORE_ITEMS) break;
        if (status != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ))
            continue;

        std::wstring path = FontPathFromRegistryValue(
            reinterpret_cast<const wchar_t*>(valueData));
        if (path.empty()) continue;

        std::wstring nameAlias = NormalizeFontAlias(valueName);
        wchar_t stem[MAX_PATH] = {};
        wcsncpy_s(stem, PathFindFileNameW(path.c_str()), _TRUNCATE);
        PathRemoveExtensionW(stem);
        std::wstring fileAlias = NormalizeFontAlias(stem);

        DWORD score = 0;
        if (!targetAlias.empty() && nameAlias == targetAlias) score = 340;
        else if (!targetAlias.empty() &&
            (nameAlias.find(targetAlias) != std::wstring::npos ||
             targetAlias.find(nameAlias) != std::wstring::npos)) score = 220;
        if (!targetAlias.empty() && fileAlias == targetAlias) score = (std::max<DWORD>)(score, 320);
        else if (!targetAlias.empty() &&
            (fileAlias.find(targetAlias) != std::wstring::npos ||
             targetAlias.find(fileAlias) != std::wstring::npos)) score = (std::max<DWORD>)(score, 200);

        if (score > *bestScore) {
            *bestScore = score;
            *bestPath = path;
        }
    }
    RegCloseKey(key);
}

} // namespace

bool IsInternalFileQuery() {
    return g_internalFileQueryDepth != 0;
}

const std::wstring& GameRoot() {
    static const std::wstring root = []() {
        wchar_t path[MAX_PATH] = {};
        if (!GetModuleFileNameW(nullptr, path, _countof(path))) return std::wstring();
        PathRemoveFileSpecW(path);
        return std::wstring(path);
    }();
    return root;
}

const std::wstring& WindowsFontsDirectory() {
    static const std::wstring directory = []() {
        wchar_t windowsDirectory[MAX_PATH] = {};
        if (!GetWindowsDirectoryW(windowsDirectory, _countof(windowsDirectory)))
            return std::wstring(L"C:\\Windows\\Fonts");
        wchar_t path[MAX_PATH] = {};
        if (wcscpy_s(path, windowsDirectory) != 0 || !PathAppendW(path, L"Fonts"))
            return std::wstring(L"C:\\Windows\\Fonts");
        return std::wstring(path);
    }();
    return directory;
}

std::wstring BuildRootPath(const wchar_t* relativePath) {
    std::wstring path = GameRoot();
    if (path.empty() || !relativePath || !relativePath[0]) return path;
    wchar_t full[MAX_PATH] = {};
    if (wcscpy_s(full, path.c_str()) != 0 || !PathAppendW(full, relativePath)) return L"";
    return full;
}

std::wstring FullPath(const wchar_t* path) {
    if (!path || !path[0]) return L"";

    wchar_t combined[MAX_PATH] = {};
    if (PathIsRelativeW(path)) {
        if (wcscpy_s(combined, GameRoot().c_str()) != 0 || !PathAppendW(combined, path))
            return L"";
        path = combined;
    }

    wchar_t absolute[MAX_PATH] = {};
    DWORD length = GetFullPathNameW(path, _countof(absolute), absolute, nullptr);
    if (length == 0 || length >= _countof(absolute)) return L"";

    wchar_t canonical[MAX_PATH] = {};
    return PathCanonicalizeW(canonical, absolute) ? std::wstring(canonical) :
        std::wstring(absolute);
}

std::wstring NormalizePath(std::wstring path) {
    for (wchar_t& ch : path) {
        if (ch == L'/') ch = L'\\';
        ch = static_cast<wchar_t>(towlower(ch));
    }
    return path;
}

bool IsUnderGameRoot(const std::wstring& fullPath) {
    static const std::wstring root = NormalizePath(GameRoot());
    std::wstring normalized = NormalizePath(fullPath);
    if (root.empty() || normalized.size() < root.size() ||
        normalized.compare(0, root.size(), root) != 0) {
        return false;
    }
    return normalized.size() == root.size() || normalized[root.size()] == L'\\';
}

bool HasExtension(const std::wstring& path, const wchar_t* extension) {
    if (!extension || !extension[0] || path.size() < wcslen(extension)) return false;
    return _wcsicmp(path.c_str() + path.size() - wcslen(extension), extension) == 0;
}

bool SamePath(const std::wstring& left, const std::wstring& right) {
    return !left.empty() && !right.empty() &&
        NormalizePath(left) == NormalizePath(right);
}

bool IsFile(const std::wstring& path) {
    if (path.empty()) return false;
    InternalFileQueryScope queryScope;
    DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool IsDirectory(const std::wstring& path) {
    if (path.empty()) return false;
    InternalFileQueryScope queryScope;
    DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::wstring WideFromAnsi(const char* text) {
    if (!text || !text[0]) return L"";
    int length = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
    if (length <= 0) return L"";
    std::vector<wchar_t> buffer(static_cast<size_t>(length));
    MultiByteToWideChar(CP_ACP, 0, text, -1, buffer.data(), length);
    return std::wstring(buffer.data());
}

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) return "";
    int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1,
        nullptr, 0, nullptr, nullptr);
    if (length <= 0) return "";
    std::vector<char> buffer(static_cast<size_t>(length));
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, buffer.data(), length,
        nullptr, nullptr);
    return std::string(buffer.data());
}

std::wstring FindSystemFontFileByFace(const wchar_t* faceName) {
    if (!faceName || !faceName[0]) return L"";
    struct CacheEntry {
        std::wstring key;
        std::wstring path;
    };
    static std::mutex cacheMutex;
    static std::vector<CacheEntry> cache;
    static size_t nextCacheSlot = 0;
    std::wstring cacheKey = NormalizeFontAlias(faceName);
    if (cacheKey.empty()) return L"";
    std::lock_guard<std::mutex> lock(cacheMutex);
    for (const CacheEntry& entry : cache) {
        if (entry.key == cacheKey) return entry.path;
    }

    std::wstring bestPath;
    DWORD bestScore = 0;
    SearchFontRegistry(HKEY_CURRENT_USER, 0, cacheKey, &bestPath, &bestScore);
    SearchFontRegistry(HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY, cacheKey,
        &bestPath, &bestScore);
    SearchFontRegistry(HKEY_LOCAL_MACHINE, 0, cacheKey, &bestPath, &bestScore);
    if (cache.size() < 32) {
        cache.push_back({ cacheKey, bestPath });
    } else {
        cache[nextCacheSlot] = { cacheKey, bestPath };
        nextCacheSlot = (nextCacheSlot + 1) % 32;
    }
    return bestPath;
}

std::wstring FindLocalFontFile(const wchar_t* configuredFileName) {
    InternalFileQueryScope queryScope;
    if (configuredFileName && configuredFileName[0]) {
        std::wstring configured = PathIsRelativeW(configuredFileName) ?
            BuildRootPath(configuredFileName) : FullPath(configuredFileName);
        if (IsFile(configured)) return configured;
    }

    static const wchar_t* const patterns[] = {
        L"*.ttf", L"*.otf", L"*.ttc"
    };
    for (const wchar_t* pattern : patterns) {
        std::wstring search = BuildRootPath(pattern);
        WIN32_FIND_DATAW data = {};
        HANDLE find = FindFirstFileW(search.c_str(), &data);
        if (find == INVALID_HANDLE_VALUE) continue;

        std::wstring result;
        do {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                result = BuildRootPath(data.cFileName);
                break;
            }
        } while (FindNextFileW(find, &data));
        FindClose(find);
        if (!result.empty()) return result;
    }
    return L"";
}

bool LooksLikeSfnt(const void* bytes, size_t byteCount) {
    if (!bytes || byteCount < 12) return false;
    const BYTE* data = static_cast<const BYTE*>(bytes);
    return (data[0] == 0x00 && data[1] == 0x01 && data[2] == 0x00 && data[3] == 0x00) ||
        memcmp(data, "OTTO", 4) == 0 || memcmp(data, "ttcf", 4) == 0 ||
        memcmp(data, "true", 4) == 0;
}

bool IsReadOnlyOpen(DWORD desiredAccess, DWORD creationDisposition) {
    if (creationDisposition != OPEN_EXISTING && creationDisposition != OPEN_ALWAYS)
        return false;
    return (desiredAccess & (GENERIC_WRITE | FILE_WRITE_DATA | FILE_APPEND_DATA |
        FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA)) == 0;
}

HANDLE CreateTemporaryReadHandle(const void* bytes, size_t byteCount) {
    if (byteCount > MAXDWORD || (byteCount > 0 && !bytes)) return INVALID_HANDLE_VALUE;
    InternalFileQueryScope queryScope;
    wchar_t tempDirectory[MAX_PATH] = {};
    wchar_t tempPath[MAX_PATH] = {};
    if (!GetTempPathW(_countof(tempDirectory), tempDirectory) ||
        !GetTempFileNameW(tempDirectory, L"sfh", 0, tempPath)) {
        return INVALID_HANDLE_VALUE;
    }

    HANDLE file = CreateFileW(tempPath, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE |
        FILE_ATTRIBUTE_NOT_CONTENT_INDEXED, nullptr);
    if (file == INVALID_HANDLE_VALUE) return INVALID_HANDLE_VALUE;

    DWORD written = 0;
    if (byteCount > 0 && (!WriteFile(file, bytes, static_cast<DWORD>(byteCount),
        &written, nullptr) || written != byteCount)) {
        CloseHandle(file);
        return INVALID_HANDLE_VALUE;
    }
    SetFilePointer(file, 0, nullptr, FILE_BEGIN);
    return file;
}

bool ModuleContainsAscii(HMODULE module, const char* marker) {
    return marker && marker[0] && ModuleContainsBytes(module, marker, strlen(marker));
}

bool ModuleContainsWide(HMODULE module, const wchar_t* marker) {
    return marker && marker[0] && ModuleContainsBytes(module, marker,
        wcslen(marker) * sizeof(wchar_t));
}

bool ModuleContainsAnyAscii(HMODULE module, const char* const* markers,
    size_t markerCount) {
    if (!module || !markers) return false;
    for (size_t i = 0; i < markerCount; ++i) {
        if (ModuleContainsAscii(module, markers[i])) return true;
    }
    return false;
}

bool ModuleContainsAllAscii(HMODULE module, const char* const* markers,
    size_t markerCount) {
    if (!module || !markers || markerCount == 0) return false;
    for (size_t i = 0; i < markerCount; ++i) {
        if (!ModuleContainsAscii(module, markers[i])) return false;
    }
    return true;
}

bool MainModuleContainsAnyAscii(const char* const* markers, size_t markerCount) {
    return ModuleContainsAnyAscii(GetModuleHandleW(nullptr), markers, markerCount);
}

bool MainModuleContainsAllAscii(const char* const* markers, size_t markerCount) {
    return ModuleContainsAllAscii(GetModuleHandleW(nullptr), markers, markerCount);
}

bool ModuleHasAllExports(HMODULE module, const char* const* exportNames,
    size_t exportCount) {
    if (!module || !exportNames || exportCount == 0) return false;
    for (size_t i = 0; i < exportCount; ++i) {
        if (!exportNames[i] || !GetProcAddress(module, exportNames[i])) return false;
    }
    return true;
}

bool AnyRootFileStartsWithAnyAscii(const wchar_t* pattern,
    const char* const* magics, size_t magicCount, size_t maxFiles) {
    if (!pattern || !pattern[0] || !magics || magicCount == 0 || maxFiles == 0)
        return false;

    InternalFileQueryScope queryScope;
    std::wstring search = BuildRootPath(pattern);
    WIN32_FIND_DATAW data = {};
    HANDLE find = FindFirstFileW(search.c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) return false;

    bool found = false;
    size_t checked = 0;
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) continue;
        if (++checked > maxFiles) break;
        std::wstring path = BuildRootPath(data.cFileName);
        if (FileStartsWithAnyAscii(path, magics, magicCount)) {
            found = true;
            break;
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return found;
}

} // namespace EngineCommon
