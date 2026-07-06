#pragma once
#include <windows.h>
#include <vector>

namespace FontPatcher {
    struct CmapAlias {
        DWORD fromCodepoint;
        DWORD toCodepoint;
    };

    // Check if buffer is a valid TTF/OTF (basic check)
    bool IsFontFile(const void* data, size_t size);
    bool IsFontCollection(const void* data, size_t size);
    bool ExtractFontFromCollectionByName(const std::vector<BYTE>& collectionData,
        const wchar_t* faceName, std::vector<BYTE>& outFontData);

    // Modify OS/2 table ulCodePageRange1
    // bitIndex: 0-63. (Range1 covers 0-31, Range2 covers 32-63)
    // GalFontTool uses bit 17 for ShiftJIS, 18 for GB2312.
    // Returns true if modified.
    int CodePageRangeBitForCharset(DWORD charset);
    bool PatchOS2CodePageRange(BYTE* data, size_t size, int bitIndex);
    bool PatchOS2CodePageRange(std::vector<BYTE>& fontData, int bitIndex);
    bool PatchOS2CodePageRangeForCharset(BYTE* data, size_t size, DWORD charset);
    bool PatchOS2CodePageRangeForCharset(std::vector<BYTE>& fontData, DWORD charset);

    // Patch vertical metrics in hhea and OS/2, matching GalFontTool's
    // font-table repair path. Values are permille of unitsPerEm.
    bool PatchVerticalMetrics(BYTE* data, size_t size,
        int ascentPermille, int descentPermille, int lineGapPermille);
    bool PatchVerticalMetrics(std::vector<BYTE>& fontData,
        int ascentPermille, int descentPermille, int lineGapPermille);

    // Rebuild the name table so a patched in-memory clone can be selected by
    // a unique face name instead of racing the original installed/local font.
    bool PatchNameTableFamily(std::vector<BYTE>& fontData, const wchar_t* familyName);

    // Rebuild the Unicode cmap so a source codepoint resolves to the glyph
    // used by another codepoint. This is for engines that render through
    // FreeType directly and never call the text APIs where substitution runs.
    bool PatchCmapAliases(std::vector<BYTE>& fontData, const CmapAlias* aliases, size_t aliasCount);
}
