#pragma once

#include <windows.h>
#include <cstddef>
#include <string>

#include "engine_identity_policy.h"

namespace EngineCommon {

const std::wstring& GameRoot();
const std::wstring& WindowsFontsDirectory();
std::wstring BuildRootPath(const wchar_t* relativePath);
std::wstring FullPath(const wchar_t* path);
std::wstring NormalizePath(std::wstring path);
bool IsUnderGameRoot(const std::wstring& fullPath);
bool HasExtension(const std::wstring& path, const wchar_t* extension);
bool SamePath(const std::wstring& left, const std::wstring& right);
bool IsFile(const std::wstring& path);
bool IsDirectory(const std::wstring& path);
std::wstring WideFromAnsi(const char* text);
std::string WideToUtf8(const std::wstring& text);
std::wstring FindSystemFontFileByFace(const wchar_t* faceName);
std::wstring FindLocalFontFile(const wchar_t* configuredFileName);
bool LooksLikeSfnt(const void* bytes, size_t byteCount);
bool IsReadOnlyOpen(DWORD desiredAccess, DWORD creationDisposition);
HANDLE CreateTemporaryReadHandle(const void* bytes, size_t byteCount);
bool IsInternalFileQuery();

bool ModuleContainsAscii(HMODULE module, const char* marker);
bool ModuleContainsWide(HMODULE module, const wchar_t* marker);
bool ModuleContainsAnyAscii(HMODULE module, const char* const* markers,
    size_t markerCount);
bool ModuleContainsAllAscii(HMODULE module, const char* const* markers,
    size_t markerCount);
bool MainModuleContainsAnyAscii(const char* const* markers, size_t markerCount);
bool MainModuleContainsAllAscii(const char* const* markers, size_t markerCount);
bool ModuleHasAllExports(HMODULE module, const char* const* exportNames,
    size_t exportCount);
bool AnyRootFileStartsWithAnyAscii(const wchar_t* pattern,
    const char* const* magics, size_t magicCount, size_t maxFiles = 64);

} // namespace EngineCommon
