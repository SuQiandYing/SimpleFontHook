#pragma once
#include <windows.h>
#include <vector>

namespace FontPatcher {
    // Check if buffer is a valid TTF/OTF (basic check)
    bool IsFontFile(const void* data, size_t size);

    // Modify OS/2 table ulCodePageRange1
    // bitIndex: 0-63. (Range1 covers 0-31, Range2 covers 32-63)
    // GalFontTool uses bit 17 for ShiftJIS, 18 for GB2312.
    // Returns true if modified.
    bool PatchOS2CodePageRange(std::vector<BYTE>& fontData, int bitIndex);
}
