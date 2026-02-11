#include "font_patcher.h"
#include <windows.h>
#include <vector>

// Big Endian helpers
static WORD ReadU16BE(const BYTE* p) { return (p[0] << 8) | p[1]; }
static DWORD ReadU32BE(const BYTE* p) { return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }
static void WriteU32BE(BYTE* p, DWORD v) {
    p[0] = (BYTE)((v >> 24) & 0xFF);
    p[1] = (BYTE)((v >> 16) & 0xFF);
    p[2] = (BYTE)((v >> 8) & 0xFF);
    p[3] = (BYTE)(v & 0xFF);
}

namespace FontPatcher {
    bool IsFontFile(const void* data, size_t size) {
        if (!data || size < 12) return false;
        const BYTE* p = (const BYTE*)data;
        DWORD sfntVersion = ReadU32BE(p);
        // 0x00010000 (TTF) or 'OTTO' (OpenType) or 'true'
        return sfntVersion == 0x00010000 || sfntVersion == 0x4F54544F || sfntVersion == 0x74727565;
    }

    bool PatchOS2CodePageRange(std::vector<BYTE>& data, int bitIndex) {
        if (!IsFontFile(data.data(), data.size())) return false;
        if (bitIndex < 0 || bitIndex > 63) return false;

        const BYTE* p = data.data();
        WORD numTables = ReadU16BE(p + 4);

        // Directory follows header (12 bytes)
        size_t tableDirOffset = 12;
        size_t os2Offset = 0;
        size_t os2Length = 0;

        for (int i = 0; i < numTables; i++) {
            size_t entryOffset = tableDirOffset + i * 16;
            if (entryOffset + 16 > data.size()) return false;

            const BYTE* entry = p + entryOffset;
            DWORD tag = ReadU32BE(entry);
            
            // Tag 'OS/2' = 0x4F532F32
            if (tag == 0x4F532F32) {
                os2Offset = ReadU32BE(entry + 8);
                os2Length = ReadU32BE(entry + 12);
                break;
            }
        }

        if (os2Offset == 0 || os2Offset + os2Length > data.size()) return false;

        // OS/2 table
        BYTE* os2 = data.data() + os2Offset;
        
        // ulCodePageRange1 (bits 0-31) is at offset 78
        // ulCodePageRange2 (bits 32-63) is at offset 82
        // Both exist in version 0 (length 78+?) 
        // Actually version 0 length is 78 bytes, so range1 is at the very end... wait.
        // Apple spec says V0 is 78 bytes. MS spec says 78 bytes. 
        // ulCodePageRange1 is at byte 78? No, 78 is the size.
        // Field offsets:
        // version: 0 (uint16)
        // ...
        // ulCodePageRange1: 78
        // ulCodePageRange2: 82
        // So table size must be at least 86 bytes for Range2, or 82 bytes for Range1?
        // Let's check offsets.
        // http://www.microsoft.com/typography/otspec/os2.htm
        // 0: version
        // ...
        // 78: ulCodePageRange1
        // 82: ulCodePageRange2
        
        if (os2Length < 82) return false; // Need at least enough bytes for Range1

        if (bitIndex < 32) {
            // Range1
            DWORD range1 = ReadU32BE(os2 + 78);
            range1 |= (1 << bitIndex);
            WriteU32BE(os2 + 78, range1);
        } else {
            // Range2
            if (os2Length < 86) return false;
            DWORD range2 = ReadU32BE(os2 + 82);
            range2 |= (1 << (bitIndex - 32));
            WriteU32BE(os2 + 82, range2);
        }

        return true;
    }
}
