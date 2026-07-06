#include "font_patcher.h"
#include <windows.h>
#include <algorithm>
#include <cwctype>
#include <cstring>
#include <string>
#include <vector>

// Big Endian helpers
static WORD ReadU16BE(const BYTE* p) { return (p[0] << 8) | p[1]; }
static DWORD ReadU32BE(const BYTE* p) { return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }
static void WriteU16BE(BYTE* p, WORD v) {
    p[0] = (BYTE)((v >> 8) & 0xFF);
    p[1] = (BYTE)(v & 0xFF);
}
static void WriteU32BE(BYTE* p, DWORD v) {
    p[0] = (BYTE)((v >> 24) & 0xFF);
    p[1] = (BYTE)((v >> 16) & 0xFF);
    p[2] = (BYTE)((v >> 8) & 0xFF);
    p[3] = (BYTE)(v & 0xFF);
}

static bool IsSfntVersion(DWORD version) {
    return version == 0x00010000 || version == 0x4F54544F || version == 0x74727565;
}

static bool IsFontCollection(const void* data, size_t size) {
    if (!data || size < 12) return false;
    return ReadU32BE((const BYTE*)data) == 0x74746366; // 'ttcf'
}

static bool FindSfntTableEntryAt(const BYTE* data, size_t size, size_t fontOffset, DWORD tag,
    size_t* outEntryOffset, size_t* tableOffset, size_t* tableLength) {
    if (!data || fontOffset > size || size - fontOffset < 12) return false;
    if (!IsSfntVersion(ReadU32BE(data + fontOffset))) return false;

    WORD numTables = ReadU16BE(data + fontOffset + 4);
    size_t tableDirOffset = fontOffset + 12;
    if (tableDirOffset + (size_t)numTables * 16 > size) return false;

    for (WORD i = 0; i < numTables; ++i) {
        size_t entryOffset = tableDirOffset + (size_t)i * 16;
        const BYTE* entry = data + entryOffset;
        if (ReadU32BE(entry) != tag) continue;

        size_t offset = ReadU32BE(entry + 8);
        size_t length = ReadU32BE(entry + 12);
        if (offset > size || length > size - offset) return false;

        if (outEntryOffset) *outEntryOffset = entryOffset;
        if (tableOffset) *tableOffset = offset;
        if (tableLength) *tableLength = length;
        return true;
    }

    return false;
}

static bool FindSfntTableAt(const BYTE* data, size_t size, size_t fontOffset, DWORD tag,
    size_t* tableOffset, size_t* tableLength) {
    return FindSfntTableEntryAt(data, size, fontOffset, tag, NULL, tableOffset, tableLength);
}

static bool GetFontOffsets(const BYTE* data, size_t size, std::vector<size_t>& offsets) {
    offsets.clear();
    if (!data || size < 12) return false;

    if (IsFontCollection(data, size)) {
        DWORD count = ReadU32BE(data + 8);
        if (count == 0 || count > 256) return false;
        if (12 + (size_t)count * 4 > size) return false;
        for (DWORD i = 0; i < count; ++i) {
            size_t offset = ReadU32BE(data + 12 + (size_t)i * 4);
            if (offset > size || size - offset < 12) return false;
            if (!IsSfntVersion(ReadU32BE(data + offset))) return false;
            offsets.push_back(offset);
        }
        return !offsets.empty();
    }

    if (IsSfntVersion(ReadU32BE(data))) {
        offsets.push_back(0);
        return true;
    }

    return false;
}

static DWORD CalcSfntChecksum(const BYTE* data, size_t length) {
    DWORD sum = 0;
    for (size_t i = 0; i < length; i += 4) {
        DWORD word = 0;
        for (size_t j = 0; j < 4; ++j) {
            word <<= 8;
            if (i + j < length)
                word |= data[i + j];
        }
        sum += word;
    }
    return sum;
}

static bool UpdateTableChecksumAt(BYTE* data, size_t size, size_t fontOffset, DWORD tag) {
    size_t entryOffset = 0;
    size_t tableOffset = 0;
    size_t tableLength = 0;
    if (!FindSfntTableEntryAt(data, size, fontOffset, tag, &entryOffset, &tableOffset, &tableLength))
        return false;

    WriteU32BE(data + entryOffset + 4, CalcSfntChecksum(data + tableOffset, tableLength));
    return true;
}

static bool UpdateChecksumAdjustmentAt(BYTE* data, size_t size, size_t fontOffset) {
    size_t headOffset = 0;
    size_t headLength = 0;
    if (!FindSfntTableAt(data, size, fontOffset, 0x68656164, &headOffset, &headLength)) return false; // 'head'
    if (headLength < 12) return false;

    WriteU32BE(data + headOffset + 8, 0);
    DWORD sum = IsFontCollection(data, size)
        ? 0
        : CalcSfntChecksum(data, size);
    if (IsFontCollection(data, size)) {
        WORD numTables = ReadU16BE(data + fontOffset + 4);
        size_t dirLength = 12 + (size_t)numTables * 16;
        if (fontOffset > size || dirLength > size - fontOffset) return false;
        sum += CalcSfntChecksum(data + fontOffset, dirLength);
        for (WORD i = 0; i < numTables; ++i) {
            const BYTE* entry = data + fontOffset + 12 + (size_t)i * 16;
            size_t tableOffset = ReadU32BE(entry + 8);
            size_t tableLength = ReadU32BE(entry + 12);
            if (tableOffset > size || tableLength > size - tableOffset) return false;
            sum += CalcSfntChecksum(data + tableOffset, tableLength);
        }
    }
    WriteU32BE(data + headOffset + 8, 0xB1B0AFBAu - sum);
    return true;
}

static bool UpdateChecksumAdjustment(BYTE* data, size_t size) {
    return UpdateChecksumAdjustmentAt(data, size, 0);
}

static bool UpdateChecksumAdjustment(std::vector<BYTE>& data) {
    if (data.empty()) return false;
    return UpdateChecksumAdjustment(data.data(), data.size());
}

static int ClampIntLocal(int value, int lo, int hi) {
    return value < lo ? lo : (value > hi ? hi : value);
}

static int ScalePermilleToDesignUnits(int unitsPerEm, int permille) {
    long long value = (long long)unitsPerEm * (long long)permille;
    if (value >= 0) return (int)((value + 500) / 1000);
    return (int)((value - 500) / 1000);
}

static int ClampS16(int value) {
    return ClampIntLocal(value, -32768, 32767);
}

static WORD ClampU16(int value) {
    value = ClampIntLocal(value, 0, 65535);
    return (WORD)value;
}

static void AppendU16BE(std::vector<BYTE>& data, WORD value) {
    data.push_back((BYTE)((value >> 8) & 0xFF));
    data.push_back((BYTE)(value & 0xFF));
}

static void AppendU32BE(std::vector<BYTE>& data, DWORD value) {
    data.push_back((BYTE)((value >> 24) & 0xFF));
    data.push_back((BYTE)((value >> 16) & 0xFF));
    data.push_back((BYTE)((value >> 8) & 0xFF));
    data.push_back((BYTE)(value & 0xFF));
}

static bool IsReplaceNameId(WORD nameId) {
    switch (nameId) {
    case 1:  // Font Family
    case 4:  // Full font name
    case 6:  // PostScript name
    case 16: // Typographic family
    case 18: // Compatible full name
    case 21: // WWS family
        return true;
    default:
        return false;
    }
}

static std::string MakeAsciiName(const wchar_t* text, bool postScriptSafe) {
    std::string out;
    if (text) {
        for (const wchar_t* p = text; *p; ++p) {
            wchar_t ch = *p;
            bool alphaNum =
                (ch >= L'0' && ch <= L'9') ||
                (ch >= L'A' && ch <= L'Z') ||
                (ch >= L'a' && ch <= L'z');
            if (alphaNum) {
                out.push_back((char)ch);
            } else if (!postScriptSafe && ch == L' ') {
                out.push_back(' ');
            } else if (!postScriptSafe && ch > 0 && ch < 0x80 && ch != L'\t' && ch != L'\r' && ch != L'\n') {
                out.push_back((char)ch);
            }
        }
    }
    if (out.empty()) out = postScriptSafe ? "SFHMetricPS" : "SFH Metric";
    return out;
}

static std::vector<BYTE> EncodeNameString(WORD platformId, WORD, WORD nameId, const wchar_t* familyName) {
    std::vector<BYTE> bytes;

    if (platformId == 0 || platformId == 3) {
        if (familyName) {
            for (const wchar_t* p = familyName; *p; ++p) {
                WORD ch = (WORD)*p;
                bytes.push_back((BYTE)((ch >> 8) & 0xFF));
                bytes.push_back((BYTE)(ch & 0xFF));
            }
        }
        return bytes;
    }

    std::string ascii = MakeAsciiName(familyName, nameId == 6);
    bytes.assign(ascii.begin(), ascii.end());
    return bytes;
}

static std::wstring DecodeNameString(WORD platformId, const BYTE* bytes, size_t length) {
    std::wstring out;
    if (!bytes || length == 0) return out;

    if (platformId == 0 || platformId == 3) {
        length &= ~(size_t)1;
        out.reserve(length / 2);
        for (size_t i = 0; i + 1 < length; i += 2) {
            WORD ch = ReadU16BE(bytes + i);
            if (ch) out.push_back((wchar_t)ch);
        }
        return out;
    }

    out.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        BYTE ch = bytes[i];
        if (ch >= 0x20 && ch < 0x7F)
            out.push_back((wchar_t)ch);
    }
    return out;
}

static std::wstring NormalizeNameForMatch(const wchar_t* text) {
    std::wstring out;
    if (!text) return out;

    for (const wchar_t* p = text; *p; ++p) {
        wchar_t ch = *p;
        if (iswspace(ch) || ch == L'-' || ch == L'_' || ch == L'.' ||
            ch == L'(' || ch == L')' || ch == L'\xff08' || ch == L'\xff09')
            continue;
        if (ch >= L'A' && ch <= L'Z')
            ch = (wchar_t)(ch - L'A' + L'a');
        out.push_back(ch);
    }
    return out;
}

static bool FontRecordNameMatchesFace(const std::wstring& recordName, const wchar_t* faceName, int* score) {
    std::wstring record = NormalizeNameForMatch(recordName.c_str());
    std::wstring face = NormalizeNameForMatch(faceName);
    if (record.empty() || face.empty()) return false;

    if (record == face) {
        if (score) *score = 100;
        return true;
    }
    if (record.find(face) != std::wstring::npos || face.find(record) != std::wstring::npos) {
        if (score) *score = 60;
        return true;
    }
    return false;
}

static bool IsFamilyMatchNameId(WORD nameId) {
    switch (nameId) {
    case 1:  // Font Family
    case 4:  // Full font name
    case 16: // Typographic family
    case 18: // Compatible full name
    case 21: // WWS family
        return true;
    default:
        return false;
    }
}

static bool CollectionFontMatchesFace(const BYTE* data, size_t size, size_t fontOffset,
    const wchar_t* faceName, int* bestScore) {
    if (!faceName || !faceName[0]) return false;

    size_t nameOffset = 0;
    size_t nameLength = 0;
    if (!FindSfntTableAt(data, size, fontOffset, 0x6E616D65, &nameOffset, &nameLength))
        return false; // 'name'
    if (nameLength < 6) return false;

    const BYTE* name = data + nameOffset;
    WORD count = ReadU16BE(name + 2);
    WORD stringOffset = ReadU16BE(name + 4);
    if (6 + (size_t)count * 12 > nameLength || stringOffset > nameLength) return false;

    int localBest = -1;
    for (WORD i = 0; i < count; ++i) {
        const BYTE* record = name + 6 + (size_t)i * 12;
        WORD platformId = ReadU16BE(record + 0);
        WORD languageId = ReadU16BE(record + 4);
        WORD nameId = ReadU16BE(record + 6);
        WORD length = ReadU16BE(record + 8);
        WORD offset = ReadU16BE(record + 10);
        if (!IsFamilyMatchNameId(nameId)) continue;

        size_t textOffset = (size_t)stringOffset + offset;
        if (textOffset > nameLength || length > nameLength - textOffset) continue;

        std::wstring decoded = DecodeNameString(platformId, name + textOffset, length);
        int matchScore = 0;
        if (!FontRecordNameMatchesFace(decoded, faceName, &matchScore)) continue;

        if (nameId == 1 || nameId == 16) matchScore += 30;
        if (nameId == 4 || nameId == 18) matchScore += 20;
        if (platformId == 0 || platformId == 3) matchScore += 20;
        if (languageId == 0x0804 || languageId == 0x0404 || languageId == 0x0411) matchScore += 5;
        if (matchScore > localBest) localBest = matchScore;
    }

    if (localBest < 0) return false;
    if (bestScore) *bestScore = localBest;
    return true;
}

static bool ExtractSfntAtOffset(const BYTE* data, size_t size, size_t fontOffset,
    std::vector<BYTE>& outFontData) {
    outFontData.clear();
    if (!data || fontOffset > size || size - fontOffset < 12) return false;
    DWORD version = ReadU32BE(data + fontOffset);
    if (!IsSfntVersion(version)) return false;

    WORD numTables = ReadU16BE(data + fontOffset + 4);
    if (numTables == 0 || numTables > 256) return false;

    size_t sourceDir = fontOffset + 12;
    if (sourceDir + (size_t)numTables * 16 > size) return false;

    struct TableRecord {
        DWORD tag;
        size_t offset;
        size_t length;
    };
    std::vector<TableRecord> tables;
    tables.reserve(numTables);
    for (WORD i = 0; i < numTables; ++i) {
        const BYTE* record = data + sourceDir + (size_t)i * 16;
        TableRecord table = {};
        table.tag = ReadU32BE(record + 0);
        table.offset = ReadU32BE(record + 8);
        table.length = ReadU32BE(record + 12);
        if (table.offset > size || table.length > size - table.offset) return false;
        tables.push_back(table);
    }

    size_t dirSize = 12 + (size_t)numTables * 16;
    outFontData.assign(dirSize, 0);

    WriteU32BE(outFontData.data(), version);
    WriteU16BE(outFontData.data() + 4, numTables);
    WORD maxPower = 1;
    WORD entrySelector = 0;
    while ((WORD)(maxPower * 2) <= numTables) {
        maxPower = (WORD)(maxPower * 2);
        ++entrySelector;
    }
    WORD searchRange = (WORD)(maxPower * 16);
    WORD rangeShift = (WORD)(numTables * 16 - searchRange);
    WriteU16BE(outFontData.data() + 6, searchRange);
    WriteU16BE(outFontData.data() + 8, entrySelector);
    WriteU16BE(outFontData.data() + 10, rangeShift);

    for (WORD i = 0; i < numTables; ++i) {
        size_t alignedOffset = (outFontData.size() + 3) & ~(size_t)3;
        if (alignedOffset > outFontData.size())
            outFontData.insert(outFontData.end(), alignedOffset - outFontData.size(), 0);

        size_t destOffset = outFontData.size();
        const TableRecord& table = tables[i];
        outFontData.insert(outFontData.end(), data + table.offset, data + table.offset + table.length);
        while ((outFontData.size() & 3) != 0)
            outFontData.push_back(0);

        BYTE* record = outFontData.data() + 12 + (size_t)i * 16;
        WriteU32BE(record + 0, table.tag);
        WriteU32BE(record + 4, 0);
        WriteU32BE(record + 8, (DWORD)destOffset);
        WriteU32BE(record + 12, (DWORD)table.length);
    }

    size_t headOffset = 0;
    size_t headLength = 0;
    if (FindSfntTableAt(outFontData.data(), outFontData.size(), 0, 0x68656164,
        &headOffset, &headLength) && headLength >= 12) {
        WriteU32BE(outFontData.data() + headOffset + 8, 0);
    }

    for (const TableRecord& table : tables)
        UpdateTableChecksumAt(outFontData.data(), outFontData.size(), 0, table.tag);
    UpdateChecksumAdjustment(outFontData);

    return IsSfntVersion(ReadU32BE(outFontData.data()));
}

static bool DecodeCmapFormat4Bmp(const BYTE* subtable, size_t length, std::vector<WORD>& glyphs) {
    if (!subtable || length < 16 || ReadU16BE(subtable) != 4) return false;
    WORD subLength = ReadU16BE(subtable + 2);
    if (subLength < 16 || subLength > length) return false;
    length = subLength;

    WORD segCountX2 = ReadU16BE(subtable + 6);
    if ((segCountX2 & 1) != 0) return false;
    WORD segCount = segCountX2 / 2;
    if (segCount == 0) return false;

    size_t endOffset = 14;
    size_t reservedOffset = endOffset + (size_t)segCount * 2;
    size_t startOffset = reservedOffset + 2;
    size_t deltaOffset = startOffset + (size_t)segCount * 2;
    size_t rangeOffset = deltaOffset + (size_t)segCount * 2;
    size_t glyphArrayOffset = rangeOffset + (size_t)segCount * 2;
    if (glyphArrayOffset > length) return false;

    bool any = false;
    for (WORD i = 0; i < segCount; ++i) {
        WORD endCode = ReadU16BE(subtable + endOffset + (size_t)i * 2);
        WORD startCode = ReadU16BE(subtable + startOffset + (size_t)i * 2);
        SHORT idDelta = (SHORT)ReadU16BE(subtable + deltaOffset + (size_t)i * 2);
        WORD idRangeOffset = ReadU16BE(subtable + rangeOffset + (size_t)i * 2);
        if (startCode > endCode) continue;

        for (DWORD ch = startCode; ch <= endCode && ch < 0xFFFF; ++ch) {
            WORD glyph = 0;
            if (idRangeOffset == 0) {
                glyph = (WORD)((ch + idDelta) & 0xFFFF);
            } else {
                size_t rangeWordOffset = rangeOffset + (size_t)i * 2;
                size_t glyphOffset = rangeWordOffset + idRangeOffset + (size_t)(ch - startCode) * 2;
                if (glyphOffset + 2 > length) continue;
                glyph = ReadU16BE(subtable + glyphOffset);
                if (glyph != 0) glyph = (WORD)((glyph + idDelta) & 0xFFFF);
            }
            if (glyph != 0) {
                glyphs[(size_t)ch] = glyph;
                any = true;
            }
        }
    }
    return any;
}

static bool DecodeCmapFormat12Bmp(const BYTE* subtable, size_t length, std::vector<WORD>& glyphs) {
    if (!subtable || length < 16 || ReadU16BE(subtable) != 12) return false;
    DWORD subLength = ReadU32BE(subtable + 4);
    if (subLength < 16 || subLength > length) return false;
    length = subLength;

    DWORD groupCount = ReadU32BE(subtable + 12);
    if (groupCount > (length - 16) / 12) return false;

    bool any = false;
    for (DWORD i = 0; i < groupCount; ++i) {
        const BYTE* group = subtable + 16 + (size_t)i * 12;
        DWORD startChar = ReadU32BE(group + 0);
        DWORD endChar = ReadU32BE(group + 4);
        DWORD startGlyph = ReadU32BE(group + 8);
        if (startChar > endChar || startChar > 0xFFFF) continue;

        DWORD end = std::min<DWORD>(endChar, 0xFFFE);
        for (DWORD ch = startChar; ch <= end; ++ch) {
            DWORD glyph = startGlyph + (ch - startChar);
            if (glyph == 0 || glyph > 0xFFFF) continue;
            glyphs[(size_t)ch] = (WORD)glyph;
            any = true;
        }
    }
    return any;
}

static int UnicodeCmapRecordScore(WORD platformId, WORD encodingId, WORD format) {
    if (format != 4 && format != 12) return 0;
    if (platformId == 3 && encodingId == 10 && format == 12) return 700;
    if (platformId == 0 && encodingId == 4 && format == 12) return 650;
    if (platformId == 0 && format == 12) return 620;
    if (platformId == 3 && encodingId == 1 && format == 4) return 600;
    if (platformId == 0 && (encodingId == 3 || encodingId == 4) && format == 4) return 560;
    if (platformId == 0 && format == 4) return 540;
    if (platformId == 3 && encodingId == 0 && format == 4) return 500;
    return 0;
}

static size_t CountMappedBmpGlyphs(const std::vector<WORD>& glyphs) {
    size_t count = 0;
    for (WORD glyph : glyphs) {
        if (glyph != 0) ++count;
    }
    return count;
}

static bool CollectBestUnicodeCmapBmp(const BYTE* data, size_t size, size_t fontOffset,
    std::vector<WORD>& glyphs) {
    glyphs.assign(65536, 0);

    size_t cmapOffset = 0;
    size_t cmapLength = 0;
    if (!FindSfntTableAt(data, size, fontOffset, 0x636D6170, &cmapOffset, &cmapLength))
        return false; // 'cmap'
    if (cmapLength < 4) return false;

    const BYTE* cmap = data + cmapOffset;
    WORD recordCount = ReadU16BE(cmap + 2);
    if (4 + (size_t)recordCount * 8 > cmapLength) return false;

    int bestScore = 0;
    size_t bestCount = 0;
    std::vector<WORD> bestGlyphs(65536, 0);
    for (WORD i = 0; i < recordCount; ++i) {
        const BYTE* record = cmap + 4 + (size_t)i * 8;
        WORD platformId = ReadU16BE(record + 0);
        WORD encodingId = ReadU16BE(record + 2);
        DWORD offset = ReadU32BE(record + 4);
        if (offset > cmapLength || cmapLength - offset < 2) continue;

        const BYTE* subtable = cmap + offset;
        WORD format = ReadU16BE(subtable);
        int score = UnicodeCmapRecordScore(platformId, encodingId, format);
        if (score == 0) continue;

        std::vector<WORD> decoded(65536, 0);
        bool ok = format == 12
            ? DecodeCmapFormat12Bmp(subtable, cmapLength - offset, decoded)
            : DecodeCmapFormat4Bmp(subtable, cmapLength - offset, decoded);
        if (!ok) continue;

        size_t decodedCount = CountMappedBmpGlyphs(decoded);
        if (score > bestScore || (score == bestScore && decodedCount > bestCount)) {
            bestScore = score;
            bestCount = decodedCount;
            bestGlyphs.swap(decoded);
        }
    }

    if (bestCount == 0) return false;
    glyphs.swap(bestGlyphs);
    return true;
}

struct Cmap4Segment {
    WORD start;
    WORD end;
    size_t glyphStart;
    size_t glyphCount;
};

static bool BuildCmapFormat4(const std::vector<WORD>& glyphs, std::vector<BYTE>& subtable) {
    subtable.clear();
    if (glyphs.size() != 65536) return false;

    std::vector<Cmap4Segment> segments;
    std::vector<WORD> glyphArray;
    for (DWORD ch = 0; ch < 0xFFFF;) {
        if (glyphs[(size_t)ch] == 0) {
            ++ch;
            continue;
        }

        DWORD start = ch;
        size_t glyphStart = glyphArray.size();
        while (ch < 0xFFFF && glyphs[(size_t)ch] != 0) {
            glyphArray.push_back(glyphs[(size_t)ch]);
            ++ch;
        }

        Cmap4Segment segment = {};
        segment.start = (WORD)start;
        segment.end = (WORD)(ch - 1);
        segment.glyphStart = glyphStart;
        segment.glyphCount = glyphArray.size() - glyphStart;
        segments.push_back(segment);
    }

    size_t segCount = segments.size() + 1; // plus 0xFFFF terminator
    size_t length = 16 + segCount * 8 + glyphArray.size() * 2;
    if (segCount > 0x7FFF || length > 0xFFFF) return false;

    WORD maxPower = 1;
    WORD entrySelector = 0;
    while ((WORD)(maxPower * 2) <= segCount) {
        maxPower = (WORD)(maxPower * 2);
        ++entrySelector;
    }

    AppendU16BE(subtable, 4);
    AppendU16BE(subtable, (WORD)length);
    AppendU16BE(subtable, 0); // language
    AppendU16BE(subtable, (WORD)(segCount * 2));
    AppendU16BE(subtable, (WORD)(maxPower * 2));
    AppendU16BE(subtable, entrySelector);
    AppendU16BE(subtable, (WORD)(segCount * 2 - maxPower * 2));

    for (const Cmap4Segment& segment : segments) AppendU16BE(subtable, segment.end);
    AppendU16BE(subtable, 0xFFFF);
    AppendU16BE(subtable, 0); // reservedPad

    for (const Cmap4Segment& segment : segments) AppendU16BE(subtable, segment.start);
    AppendU16BE(subtable, 0xFFFF);

    for (size_t i = 0; i < segments.size(); ++i) AppendU16BE(subtable, 0);
    AppendU16BE(subtable, 1); // terminator maps 0xFFFF to glyph 0

    size_t rangeOffsetBase = subtable.size();
    size_t glyphArrayBase = rangeOffsetBase + segCount * 2;
    for (size_t i = 0; i < segments.size(); ++i) {
        size_t rangeWordOffset = rangeOffsetBase + i * 2;
        size_t glyphOffset = glyphArrayBase + segments[i].glyphStart * 2;
        size_t delta = glyphOffset - rangeWordOffset;
        if (delta > 0xFFFF) return false;
        AppendU16BE(subtable, (WORD)delta);
    }
    AppendU16BE(subtable, 0);

    for (WORD glyph : glyphArray) AppendU16BE(subtable, glyph);
    return subtable.size() == length;
}

struct Cmap12Group {
    DWORD startChar;
    DWORD endChar;
    DWORD startGlyph;
};

static bool BuildCmapFormat12(const std::vector<WORD>& glyphs, std::vector<BYTE>& subtable) {
    subtable.clear();
    if (glyphs.size() != 65536) return false;

    std::vector<Cmap12Group> groups;
    for (DWORD ch = 0; ch <= 0xFFFF;) {
        WORD glyph = glyphs[(size_t)ch];
        if (glyph == 0) {
            ++ch;
            continue;
        }

        DWORD start = ch;
        DWORD startGlyph = glyph;
        DWORD end = ch;
        while (end + 1 <= 0xFFFF) {
            WORD nextGlyph = glyphs[(size_t)end + 1];
            if (nextGlyph == 0 || nextGlyph != (WORD)(startGlyph + (end + 1 - start))) break;
            ++end;
        }

        Cmap12Group group = {};
        group.startChar = start;
        group.endChar = end;
        group.startGlyph = startGlyph;
        groups.push_back(group);
        ch = end + 1;
    }

    size_t length = 16 + groups.size() * 12;
    if (length > 0xFFFFFFFFu) return false;

    AppendU16BE(subtable, 12);
    AppendU16BE(subtable, 0); // reserved
    AppendU32BE(subtable, (DWORD)length);
    AppendU32BE(subtable, 0); // language
    AppendU32BE(subtable, (DWORD)groups.size());
    for (const Cmap12Group& group : groups) {
        AppendU32BE(subtable, group.startChar);
        AppendU32BE(subtable, group.endChar);
        AppendU32BE(subtable, group.startGlyph);
    }
    return true;
}

struct CmapRecordBuild {
    WORD platformId;
    WORD encodingId;
    DWORD offset;
};

static bool BuildPatchedCmapTable(const std::vector<WORD>& glyphs, std::vector<BYTE>& cmapTable) {
    cmapTable.clear();

    std::vector<BYTE> format4;
    bool hasFormat4 = BuildCmapFormat4(glyphs, format4);

    std::vector<BYTE> format12;
    if (!BuildCmapFormat12(glyphs, format12)) return false;

    std::vector<CmapRecordBuild> records;
    if (hasFormat4) {
        CmapRecordBuild r0 = { 0, 3, 0 };
        CmapRecordBuild r1 = { 3, 1, 0 };
        records.push_back(r0);
        records.push_back(r1);
    }
    CmapRecordBuild r2 = { 0, 4, 0 };
    CmapRecordBuild r3 = { 3, 10, 0 };
    records.push_back(r2);
    records.push_back(r3);

    size_t headerSize = 4 + records.size() * 8;
    if (headerSize > 0xFFFFFFFFu) return false;
    cmapTable.assign(headerSize, 0);
    WriteU16BE(cmapTable.data(), 0);
    WriteU16BE(cmapTable.data() + 2, (WORD)records.size());

    DWORD format4Offset = 0;
    if (hasFormat4) {
        format4Offset = (DWORD)cmapTable.size();
        cmapTable.insert(cmapTable.end(), format4.begin(), format4.end());
    }
    DWORD format12Offset = (DWORD)cmapTable.size();
    cmapTable.insert(cmapTable.end(), format12.begin(), format12.end());

    for (size_t i = 0; i < records.size(); ++i) {
        DWORD offset = (records[i].platformId == 3 && records[i].encodingId == 1) ||
            (records[i].platformId == 0 && records[i].encodingId == 3)
            ? format4Offset
            : format12Offset;
        BYTE* record = cmapTable.data() + 4 + i * 8;
        WriteU16BE(record + 0, records[i].platformId);
        WriteU16BE(record + 2, records[i].encodingId);
        WriteU32BE(record + 4, offset);
    }

    return cmapTable.size() <= 0xFFFFFFFFu;
}

static bool PatchCmapAliasesAt(std::vector<BYTE>& fontData, size_t fontOffset,
    const FontPatcher::CmapAlias* aliases, size_t aliasCount) {
    if (fontData.empty() || !aliases || aliasCount == 0) return false;
    if (fontOffset != 0 || ::IsFontCollection(fontData.data(), fontData.size())) return false;

    std::vector<WORD> glyphs;
    if (!CollectBestUnicodeCmapBmp(fontData.data(), fontData.size(), fontOffset, glyphs)) return false;

    bool changed = false;
    for (size_t i = 0; i < aliasCount; ++i) {
        DWORD from = aliases[i].fromCodepoint;
        DWORD to = aliases[i].toCodepoint;
        if (from > 0xFFFE || to > 0xFFFE || from == to) continue;
        WORD targetGlyph = glyphs[(size_t)to];
        if (targetGlyph == 0) continue;
        if (glyphs[(size_t)from] == targetGlyph) continue;
        glyphs[(size_t)from] = targetGlyph;
        changed = true;
    }
    if (!changed) return false;

    std::vector<BYTE> newCmap;
    if (!BuildPatchedCmapTable(glyphs, newCmap) || newCmap.empty()) return false;

    size_t entryOffset = 0;
    size_t oldCmapOffset = 0;
    size_t oldCmapLength = 0;
    if (!FindSfntTableEntryAt(fontData.data(), fontData.size(), fontOffset,
        0x636D6170, &entryOffset, &oldCmapOffset, &oldCmapLength))
        return false; // 'cmap'

    size_t alignedOffset = (fontData.size() + 3) & ~(size_t)3;
    if (alignedOffset > fontData.size())
        fontData.insert(fontData.end(), alignedOffset - fontData.size(), 0);

    size_t newCmapOffset = fontData.size();
    fontData.insert(fontData.end(), newCmap.begin(), newCmap.end());
    while ((fontData.size() & 3) != 0)
        fontData.push_back(0);

    BYTE* data = fontData.data();
    WriteU32BE(data + entryOffset + 4, CalcSfntChecksum(fontData.data() + newCmapOffset, newCmap.size()));
    WriteU32BE(data + entryOffset + 8, (DWORD)newCmapOffset);
    WriteU32BE(data + entryOffset + 12, (DWORD)newCmap.size());
    UpdateChecksumAdjustment(fontData);
    return true;
}

namespace FontPatcher {
    bool IsFontFile(const void* data, size_t size) {
        if (!data || size < 12) return false;
        const BYTE* p = (const BYTE*)data;
        DWORD sfntVersion = ReadU32BE(p);
        // 0x00010000 (TTF) or 'OTTO' (OpenType) or 'true'
        return IsSfntVersion(sfntVersion);
    }

    bool IsFontCollection(const void* data, size_t size) {
        return ::IsFontCollection(data, size);
    }

    bool ExtractFontFromCollectionByName(const std::vector<BYTE>& collectionData,
        const wchar_t* faceName, std::vector<BYTE>& outFontData) {
        outFontData.clear();
        if (!::IsFontCollection(collectionData.data(), collectionData.size())) return false;

        std::vector<size_t> offsets;
        if (!GetFontOffsets(collectionData.data(), collectionData.size(), offsets)) return false;

        size_t bestOffset = offsets[0];
        bool matched = !faceName || !faceName[0];
        int bestScore = -1;
        if (faceName && faceName[0]) {
            for (size_t offset : offsets) {
                int score = 0;
                if (!CollectionFontMatchesFace(collectionData.data(), collectionData.size(),
                    offset, faceName, &score))
                    continue;
                if (score > bestScore) {
                    bestScore = score;
                    bestOffset = offset;
                    matched = true;
                }
            }
        }

        if (!matched) return false;
        return ExtractSfntAtOffset(collectionData.data(), collectionData.size(), bestOffset, outFontData);
    }

    int CodePageRangeBitForCharset(DWORD charset) {
        switch (charset) {
        case SHIFTJIS_CHARSET: return 17;       // 128 - Shift-JIS
        case GB2312_CHARSET: return 18;         // 134 - GB2312
        case HANGUL_CHARSET: return 19;         // 129 - Hangeul
        case CHINESEBIG5_CHARSET: return 20;    // 136 - Big5
        case DEFAULT_CHARSET:
        case ANSI_CHARSET:
            return 0;                           // Latin 1 / default
        default:
            return -1;
        }
    }

    static bool PatchOS2CodePageRangeAt(BYTE* data, size_t size, size_t fontOffset, int bitIndex) {
        if (!data) return false;
        if (bitIndex < 0 || bitIndex > 63) return false;

        size_t os2Offset = 0;
        size_t os2Length = 0;
        if (!FindSfntTableAt(data, size, fontOffset, 0x4F532F32, &os2Offset, &os2Length)) return false;

        BYTE* os2 = data + os2Offset;
        // OS/2 codepage ranges start at fixed offsets in the table:
        // ulCodePageRange1 at byte 78 and ulCodePageRange2 at byte 82.
        
        if (os2Length < 82) return false; // Need at least enough bytes for Range1

        if (bitIndex < 32) {
            // Range1
            DWORD range1 = ReadU32BE(os2 + 78);
            range1 |= (1u << bitIndex);
            WriteU32BE(os2 + 78, range1);
        } else {
            // Range2
            if (os2Length < 86) return false;
            DWORD range2 = ReadU32BE(os2 + 82);
            range2 |= (1u << (bitIndex - 32));
            WriteU32BE(os2 + 82, range2);
        }

        UpdateTableChecksumAt(data, size, fontOffset, 0x4F532F32); // 'OS/2'
        UpdateChecksumAdjustmentAt(data, size, fontOffset);
        return true;
    }

    bool PatchOS2CodePageRange(BYTE* data, size_t size, int bitIndex) {
        if (!data) return false;
        std::vector<size_t> offsets;
        if (!GetFontOffsets(data, size, offsets)) return false;

        bool changed = false;
        for (size_t offset : offsets)
            changed = PatchOS2CodePageRangeAt(data, size, offset, bitIndex) || changed;
        return changed;
    }

    bool PatchOS2CodePageRange(std::vector<BYTE>& data, int bitIndex) {
        if (data.empty()) return false;
        return PatchOS2CodePageRange(data.data(), data.size(), bitIndex);
    }

    bool PatchOS2CodePageRangeForCharset(BYTE* data, size_t size, DWORD charset) {
        int bit = CodePageRangeBitForCharset(charset);
        if (bit < 0) return false;

        bool changed = PatchOS2CodePageRange(data, size, bit);
        if (bit != 0)
            changed = PatchOS2CodePageRange(data, size, 0) || changed;
        return changed;
    }

    bool PatchOS2CodePageRangeForCharset(std::vector<BYTE>& data, DWORD charset) {
        if (data.empty()) return false;
        return PatchOS2CodePageRangeForCharset(data.data(), data.size(), charset);
    }

    static bool PatchVerticalMetricsAt(BYTE* data, size_t size, size_t fontOffset,
        int ascentPermille, int descentPermille, int lineGapPermille) {
        size_t headOffset = 0;
        size_t headLength = 0;
        if (!FindSfntTableAt(data, size, fontOffset, 0x68656164, &headOffset, &headLength)) return false; // 'head'
        if (headLength < 20) return false;

        int unitsPerEm = ReadU16BE(data + headOffset + 18);
        if (unitsPerEm <= 0) return false;

        int ascPermille = ClampIntLocal(ascentPermille, 100, 2000);
        int descPermille = ClampIntLocal(descentPermille, -2000, -1);
        int gapPermille = ClampIntLocal(lineGapPermille, -2000, 2000);

        int ascent = ClampS16(ScalePermilleToDesignUnits(unitsPerEm, ascPermille));
        int descent = ClampS16(ScalePermilleToDesignUnits(unitsPerEm, descPermille));
        int lineGap = ClampS16(ScalePermilleToDesignUnits(unitsPerEm, gapPermille));
        bool changed = false;

        size_t hheaOffset = 0;
        size_t hheaLength = 0;
        if (FindSfntTableAt(data, size, fontOffset, 0x68686561, &hheaOffset, &hheaLength) && hheaLength >= 10) { // 'hhea'
            BYTE* hhea = data + hheaOffset;
            WriteU16BE(hhea + 4, (WORD)(SHORT)ascent);
            WriteU16BE(hhea + 6, (WORD)(SHORT)descent);
            WriteU16BE(hhea + 8, (WORD)(SHORT)lineGap);
            UpdateTableChecksumAt(data, size, fontOffset, 0x68686561);
            changed = true;
        }

        size_t os2Offset = 0;
        size_t os2Length = 0;
        if (FindSfntTableAt(data, size, fontOffset, 0x4F532F32, &os2Offset, &os2Length) && os2Length >= 78) { // 'OS/2'
            BYTE* os2 = data + os2Offset;
            WriteU16BE(os2 + 68, (WORD)(SHORT)ascent);          // sTypoAscender
            WriteU16BE(os2 + 70, (WORD)(SHORT)descent);         // sTypoDescender
            WriteU16BE(os2 + 72, (WORD)(SHORT)lineGap);         // sTypoLineGap
            WriteU16BE(os2 + 74, ClampU16(ascent));             // usWinAscent
            WriteU16BE(os2 + 76, ClampU16(-descent));           // usWinDescent
            UpdateTableChecksumAt(data, size, fontOffset, 0x4F532F32);
            changed = true;
        }

        if (changed) UpdateChecksumAdjustmentAt(data, size, fontOffset);
        return changed;
    }

    bool PatchVerticalMetrics(BYTE* data, size_t size,
        int ascentPermille, int descentPermille, int lineGapPermille) {
        std::vector<size_t> offsets;
        if (!GetFontOffsets(data, size, offsets)) return false;

        bool changed = false;
        for (size_t offset : offsets) {
            changed = PatchVerticalMetricsAt(data, size, offset,
                ascentPermille, descentPermille, lineGapPermille) || changed;
        }
        return changed;
    }

    bool PatchVerticalMetrics(std::vector<BYTE>& fontData,
        int ascentPermille, int descentPermille, int lineGapPermille) {
        if (fontData.empty()) return false;
        return PatchVerticalMetrics(fontData.data(), fontData.size(),
            ascentPermille, descentPermille, lineGapPermille);
    }

    static bool PatchNameTableFamilyAt(std::vector<BYTE>& fontData, size_t fontOffset,
        const wchar_t* familyName, bool allowAppend) {
        if (fontData.empty() || !familyName || !familyName[0]) return false;
        if (fontOffset > fontData.size() || fontData.size() - fontOffset < 12) return false;
        if (!IsSfntVersion(ReadU32BE(fontData.data() + fontOffset))) return false;

        size_t entryOffset = 0;
        size_t nameOffset = 0;
        size_t nameLength = 0;
        if (!FindSfntTableEntryAt(fontData.data(), fontData.size(), fontOffset,
            0x6E616D65, &entryOffset, &nameOffset, &nameLength))
            return false; // 'name'
        if (nameLength < 6) return false;

        const BYTE* oldName = fontData.data() + nameOffset;
        WORD count = ReadU16BE(oldName + 2);
        WORD stringOffset = ReadU16BE(oldName + 4);
        if (6 + (size_t)count * 12 > nameLength || stringOffset > nameLength) return false;

        struct NameRecordData {
            WORD platformId;
            WORD encodingId;
            WORD languageId;
            WORD nameId;
            std::vector<BYTE> bytes;
        };

        std::vector<NameRecordData> records;
        records.reserve(count);
        bool canPatchInPlace = true;
        for (WORD i = 0; i < count; ++i) {
            const BYTE* record = oldName + 6 + (size_t)i * 12;
            NameRecordData item = {};
            item.platformId = ReadU16BE(record + 0);
            item.encodingId = ReadU16BE(record + 2);
            item.languageId = ReadU16BE(record + 4);
            item.nameId = ReadU16BE(record + 6);
            WORD oldLength = ReadU16BE(record + 8);
            WORD oldOffset = ReadU16BE(record + 10);

            if (IsReplaceNameId(item.nameId)) {
                item.bytes = EncodeNameString(item.platformId, item.encodingId, item.nameId, familyName);
                size_t sourceOffset = (size_t)stringOffset + oldOffset;
                if (sourceOffset > nameLength || oldLength > nameLength - sourceOffset ||
                    item.bytes.size() > oldLength) {
                    canPatchInPlace = false;
                }
            } else {
                size_t sourceOffset = (size_t)stringOffset + oldOffset;
                if (sourceOffset <= nameLength && oldLength <= nameLength - sourceOffset) {
                    const BYTE* source = oldName + sourceOffset;
                    item.bytes.assign(source, source + oldLength);
                }
            }
            if (item.bytes.size() > 65535) return false;
            records.push_back(item);
        }

        if (canPatchInPlace) {
            BYTE* writableName = fontData.data() + nameOffset;
            for (WORD i = 0; i < count; ++i) {
                BYTE* record = writableName + 6 + (size_t)i * 12;
                WORD nameId = ReadU16BE(record + 6);
                if (!IsReplaceNameId(nameId)) continue;

                WORD oldOffset = ReadU16BE(record + 10);
                size_t destOffset = (size_t)stringOffset + oldOffset;
                const std::vector<BYTE>& bytes = records[i].bytes;
                memcpy(writableName + destOffset, bytes.data(), bytes.size());
                WriteU16BE(record + 8, (WORD)bytes.size());
            }

            UpdateTableChecksumAt(fontData.data(), fontData.size(), fontOffset, 0x6E616D65); // 'name'
            UpdateChecksumAdjustmentAt(fontData.data(), fontData.size(), fontOffset);
            return true;
        }

        if (!allowAppend)
            return false;

        std::vector<BYTE> newName;
        size_t storageOffset = 6 + (size_t)records.size() * 12;
        if (storageOffset > 65535) return false;
        AppendU16BE(newName, 0); // format 0, enough for standard GDI names
        AppendU16BE(newName, (WORD)records.size());
        AppendU16BE(newName, (WORD)storageOffset);

        std::vector<BYTE> storage;
        for (const auto& item : records) {
            if (storage.size() > 65535) return false;
            AppendU16BE(newName, item.platformId);
            AppendU16BE(newName, item.encodingId);
            AppendU16BE(newName, item.languageId);
            AppendU16BE(newName, item.nameId);
            AppendU16BE(newName, (WORD)item.bytes.size());
            AppendU16BE(newName, (WORD)storage.size());
            storage.insert(storage.end(), item.bytes.begin(), item.bytes.end());
        }

        newName.insert(newName.end(), storage.begin(), storage.end());
        if (newName.empty()) return false;

        size_t alignedOffset = (fontData.size() + 3) & ~(size_t)3;
        if (alignedOffset > fontData.size())
            fontData.insert(fontData.end(), alignedOffset - fontData.size(), 0);

        size_t newNameOffset = fontData.size();
        fontData.insert(fontData.end(), newName.begin(), newName.end());
        while ((fontData.size() & 3) != 0)
            fontData.push_back(0);

        BYTE* data = fontData.data();
        WriteU32BE(data + entryOffset + 4, CalcSfntChecksum(fontData.data() + newNameOffset, newName.size()));
        WriteU32BE(data + entryOffset + 8, (DWORD)newNameOffset);
        WriteU32BE(data + entryOffset + 12, (DWORD)newName.size());
        UpdateChecksumAdjustment(fontData);
        return true;
    }

    bool PatchNameTableFamily(std::vector<BYTE>& fontData, const wchar_t* familyName) {
        if (fontData.empty() || !familyName || !familyName[0]) return false;

        std::vector<size_t> offsets;
        if (!GetFontOffsets(fontData.data(), fontData.size(), offsets)) return false;

        bool isCollection = IsFontCollection(fontData.data(), fontData.size());
        bool changed = false;
        for (size_t offset : offsets) {
            bool ok = PatchNameTableFamilyAt(fontData, offset, familyName, !isCollection && offset == 0);
            changed = ok || changed;
        }
        return changed;
    }

    bool PatchCmapAliases(std::vector<BYTE>& fontData, const CmapAlias* aliases, size_t aliasCount) {
        if (fontData.empty() || !aliases || aliasCount == 0) return false;
        if (IsFontCollection(fontData.data(), fontData.size())) return false;

        std::vector<size_t> offsets;
        if (!GetFontOffsets(fontData.data(), fontData.size(), offsets) || offsets.empty()) return false;
        return PatchCmapAliasesAt(fontData, offsets[0], aliases, aliasCount);
    }
}
