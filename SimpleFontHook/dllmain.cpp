#include "framework.h"
#include "winmm_proxy.h"
#include "font_picker.h"
#include <detours.h>
#include <algorithm>
#include <gdiplus.h>
#include <process.h>
#include <psapi.h>
#include <shlwapi.h>
#include <dwrite.h>
#include <intrin.h>
#include <mutex>

#ifdef _WIN64
#pragma comment(lib, "detours_x64.lib")
#else
#pragma comment(lib, "detours.lib")
#endif
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shlwapi.lib")

#pragma intrinsic(_ReturnAddress)

using namespace Gdiplus;

// ===================== GDI Font Creation Types =====================
typedef HFONT(WINAPI* pCreateFontA)(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCSTR);
typedef HFONT(WINAPI* pCreateFontIndirectA)(const LOGFONTA*);
typedef HFONT(WINAPI* pCreateFontW)(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCWSTR);
typedef HFONT(WINAPI* pCreateFontIndirectW)(const LOGFONTW*);

static pCreateFontA orgCreateFontA = CreateFontA;
static pCreateFontIndirectA orgCreateFontIndirectA = CreateFontIndirectA;
static pCreateFontW orgCreateFontW = CreateFontW;
static pCreateFontIndirectW orgCreateFontIndirectW = CreateFontIndirectW;

// ===================== GDI Text Output Types =====================
typedef BOOL(WINAPI* pTextOutA)(HDC, int, int, LPCSTR, int);
typedef BOOL(WINAPI* pTextOutW)(HDC, int, int, LPCWSTR, int);
typedef BOOL(WINAPI* pExtTextOutA)(HDC, int, int, UINT, const RECT*, LPCSTR, UINT, const int*);
typedef BOOL(WINAPI* pExtTextOutW)(HDC, int, int, UINT, const RECT*, LPCWSTR, UINT, const int*);
typedef int (WINAPI* pDrawTextA)(HDC, LPCSTR, int, LPRECT, UINT);
typedef int (WINAPI* pDrawTextW)(HDC, LPCWSTR, int, LPRECT, UINT);
typedef int (WINAPI* pDrawTextExA)(HDC, LPSTR, int, LPRECT, UINT, LPDRAWTEXTPARAMS);
typedef int (WINAPI* pDrawTextExW)(HDC, LPWSTR, int, LPRECT, UINT, LPDRAWTEXTPARAMS);
typedef BOOL(WINAPI* pPolyTextOutA)(HDC, const POLYTEXTA*, int);
typedef BOOL(WINAPI* pPolyTextOutW)(HDC, const POLYTEXTW*, int);
typedef LONG(WINAPI* pTabbedTextOutA)(HDC, int, int, LPCSTR, int, int, const int*, int);
typedef LONG(WINAPI* pTabbedTextOutW)(HDC, int, int, LPCWSTR, int, int, const int*, int);

static pTextOutA orgTextOutA = TextOutA;
static pTextOutW orgTextOutW = TextOutW;
static pExtTextOutA orgExtTextOutA = ExtTextOutA;
static pExtTextOutW orgExtTextOutW = ExtTextOutW;
static pDrawTextA orgDrawTextA = DrawTextA;
static pDrawTextW orgDrawTextW = DrawTextW;
static pDrawTextExA orgDrawTextExA = DrawTextExA;
static pDrawTextExW orgDrawTextExW = DrawTextExW;
static pPolyTextOutA orgPolyTextOutA = PolyTextOutA;
static pPolyTextOutW orgPolyTextOutW = PolyTextOutW;
static pTabbedTextOutA orgTabbedTextOutA = TabbedTextOutA;
static pTabbedTextOutW orgTabbedTextOutW = TabbedTextOutW;

// ===================== Glyph & Metrics Types (critical for engines that cache glyphs) =====================
typedef DWORD(WINAPI* pGetGlyphOutlineA)(HDC, UINT, UINT, LPGLYPHMETRICS, DWORD, LPVOID, const MAT2*);
typedef DWORD(WINAPI* pGetGlyphOutlineW)(HDC, UINT, UINT, LPGLYPHMETRICS, DWORD, LPVOID, const MAT2*);
typedef BOOL(WINAPI* pGetCharABCWidthsA)(HDC, UINT, UINT, LPABC);
typedef BOOL(WINAPI* pGetCharABCWidthsW)(HDC, UINT, UINT, LPABC);
typedef BOOL(WINAPI* pGetCharABCWidthsFloatA)(HDC, UINT, UINT, LPABCFLOAT);
typedef BOOL(WINAPI* pGetCharABCWidthsFloatW)(HDC, UINT, UINT, LPABCFLOAT);
typedef BOOL(WINAPI* pGetCharWidthA)(HDC, UINT, UINT, LPINT);
typedef BOOL(WINAPI* pGetCharWidthW)(HDC, UINT, UINT, LPINT);
typedef BOOL(WINAPI* pGetCharWidth32A)(HDC, UINT, UINT, LPINT);
typedef BOOL(WINAPI* pGetCharWidth32W)(HDC, UINT, UINT, LPINT);
typedef BOOL(WINAPI* pGetTextExtentPoint32A)(HDC, LPCSTR, int, LPSIZE);
typedef BOOL(WINAPI* pGetTextExtentPoint32W)(HDC, LPCWSTR, int, LPSIZE);
typedef BOOL(WINAPI* pGetTextExtentExPointA)(HDC, LPCSTR, int, int, LPINT, LPINT, LPSIZE);
typedef BOOL(WINAPI* pGetTextExtentExPointW)(HDC, LPCWSTR, int, int, LPINT, LPINT, LPSIZE);

static pGetGlyphOutlineA orgGetGlyphOutlineA = GetGlyphOutlineA;
static pGetGlyphOutlineW orgGetGlyphOutlineW = GetGlyphOutlineW;
static pGetCharABCWidthsA orgGetCharABCWidthsA = GetCharABCWidthsA;
static pGetCharABCWidthsW orgGetCharABCWidthsW = GetCharABCWidthsW;
static pGetCharABCWidthsFloatA orgGetCharABCWidthsFloatA = GetCharABCWidthsFloatA;
static pGetCharABCWidthsFloatW orgGetCharABCWidthsFloatW = GetCharABCWidthsFloatW;
static pGetCharWidthA orgGetCharWidthA = GetCharWidthA;
static pGetCharWidthW orgGetCharWidthW = GetCharWidthW;
static pGetCharWidth32A orgGetCharWidth32A = GetCharWidth32A;
static pGetCharWidth32W orgGetCharWidth32W = GetCharWidth32W;
static pGetTextExtentPoint32A orgGetTextExtentPoint32A = GetTextExtentPoint32A;
static pGetTextExtentPoint32W orgGetTextExtentPoint32W = GetTextExtentPoint32W;
static pGetTextExtentExPointA orgGetTextExtentExPointA = GetTextExtentExPointA;
static pGetTextExtentExPointW orgGetTextExtentExPointW = GetTextExtentExPointW;

// ===================== SelectObject Hook Type =====================
typedef HGDIOBJ(WINAPI* pSelectObject)(HDC, HGDIOBJ);
static pSelectObject orgSelectObject = SelectObject;

// ===================== GetObjectW/A Hook Types =====================
typedef int (WINAPI* pGetObjectA)(HANDLE, int, LPVOID);
typedef int (WINAPI* pGetObjectW)(HANDLE, int, LPVOID);
static pGetObjectA orgGetObjectA = GetObjectA;
static pGetObjectW orgGetObjectW = GetObjectW;

// ===================== GetTextMetrics Hook Types =====================
typedef BOOL(WINAPI* pGetTextMetricsA)(HDC, LPTEXTMETRICA);
typedef BOOL(WINAPI* pGetTextMetricsW)(HDC, LPTEXTMETRICW);
static pGetTextMetricsA orgGetTextMetricsA = GetTextMetricsA;
static pGetTextMetricsW orgGetTextMetricsW = GetTextMetricsW;

// ===================== GetTextFace Hook Types =====================
typedef int (WINAPI* pGetTextFaceA)(HDC, int, LPSTR);
typedef int (WINAPI* pGetTextFaceW)(HDC, int, LPWSTR);
static pGetTextFaceA orgGetTextFaceA = GetTextFaceA;
static pGetTextFaceW orgGetTextFaceW = GetTextFaceW;

// ===================== EnumFontFamiliesEx Hook Types =====================
typedef int (WINAPI* pEnumFontFamiliesExA)(HDC, LPLOGFONTA, FONTENUMPROCA, LPARAM, DWORD);
typedef int (WINAPI* pEnumFontFamiliesExW)(HDC, LPLOGFONTW, FONTENUMPROCW, LPARAM, DWORD);
static pEnumFontFamiliesExA orgEnumFontFamiliesExA = EnumFontFamiliesExA;
static pEnumFontFamiliesExW orgEnumFontFamiliesExW = EnumFontFamiliesExW;

// ===================== CreateFontIndirectEx Hook Types =====================
typedef HFONT(WINAPI* pCreateFontIndirectExA)(const ENUMLOGFONTEXDVA*);
typedef HFONT(WINAPI* pCreateFontIndirectExW)(const ENUMLOGFONTEXDVW*);
static pCreateFontIndirectExA orgCreateFontIndirectExA = CreateFontIndirectExA;
static pCreateFontIndirectExW orgCreateFontIndirectExW = CreateFontIndirectExW;

// ===================== Font Resource Types =====================
typedef int (WINAPI* pAddFontResourceA)(LPCSTR);
typedef int (WINAPI* pAddFontResourceW)(LPCWSTR);
typedef int (WINAPI* pAddFontResourceExA)(LPCSTR, DWORD, PVOID);
typedef int (WINAPI* pAddFontResourceExW)(LPCWSTR, DWORD, PVOID);
typedef HANDLE(WINAPI* pAddFontMemResourceEx)(PVOID, DWORD, PVOID, DWORD*);
typedef BOOL(WINAPI* pRemoveFontResourceA)(LPCSTR);
typedef BOOL(WINAPI* pRemoveFontResourceW)(LPCWSTR);
typedef BOOL(WINAPI* pRemoveFontResourceExA)(LPCSTR, DWORD, PVOID);
typedef BOOL(WINAPI* pRemoveFontResourceExW)(LPCWSTR, DWORD, PVOID);
typedef BOOL(WINAPI* pRemoveFontMemResourceEx)(HANDLE);
static pAddFontResourceA orgAddFontResourceA = AddFontResourceA;
static pAddFontResourceW orgAddFontResourceW = AddFontResourceW;
static pAddFontResourceExA orgAddFontResourceExA = AddFontResourceExA;
static pAddFontResourceExW orgAddFontResourceExW = AddFontResourceExW;
static pAddFontMemResourceEx orgAddFontMemResourceEx = AddFontMemResourceEx;
static pRemoveFontResourceA orgRemoveFontResourceA = RemoveFontResourceA;
static pRemoveFontResourceW orgRemoveFontResourceW = RemoveFontResourceW;
static pRemoveFontResourceExA orgRemoveFontResourceExA = RemoveFontResourceExA;
static pRemoveFontResourceExW orgRemoveFontResourceExW = RemoveFontResourceExW;
static pRemoveFontMemResourceEx orgRemoveFontMemResourceEx = RemoveFontMemResourceEx;

// ===================== Old-Style Font Enumeration Types =====================
typedef int (WINAPI* pEnumFontsA)(HDC, LPCSTR, FONTENUMPROCA, LPARAM);
typedef int (WINAPI* pEnumFontsW)(HDC, LPCWSTR, FONTENUMPROCW, LPARAM);
typedef int (WINAPI* pEnumFontFamiliesA)(HDC, LPCSTR, FONTENUMPROCA, LPARAM);
typedef int (WINAPI* pEnumFontFamiliesW)(HDC, LPCWSTR, FONTENUMPROCW, LPARAM);
static pEnumFontsA orgEnumFontsA = EnumFontsA;
static pEnumFontsW orgEnumFontsW = EnumFontsW;
static pEnumFontFamiliesA orgEnumFontFamiliesA = EnumFontFamiliesA;
static pEnumFontFamiliesW orgEnumFontFamiliesW = EnumFontFamiliesW;

// ===================== Glyph/Width Supplement Types =====================
typedef BOOL(WINAPI* pGetCharWidthFloatA)(HDC, UINT, UINT, PFLOAT);
typedef BOOL(WINAPI* pGetCharWidthFloatW)(HDC, UINT, UINT, PFLOAT);
typedef BOOL(WINAPI* pGetCharWidthI)(HDC, UINT, UINT, LPWORD, LPINT);
typedef BOOL(WINAPI* pGetCharABCWidthsI)(HDC, UINT, UINT, LPWORD, LPABC);
typedef DWORD(WINAPI* pGetCharacterPlacementA)(HDC, LPCSTR, int, int, LPGCP_RESULTSA, DWORD);
typedef DWORD(WINAPI* pGetCharacterPlacementW)(HDC, LPCWSTR, int, int, LPGCP_RESULTSW, DWORD);
typedef DWORD(WINAPI* pGetKerningPairsA)(HDC, DWORD, LPKERNINGPAIR);
typedef DWORD(WINAPI* pGetKerningPairsW)(HDC, DWORD, LPKERNINGPAIR);
typedef DWORD(WINAPI* pGetGlyphIndicesA)(HDC, LPCSTR, int, LPWORD, DWORD);
typedef DWORD(WINAPI* pGetGlyphIndicesW)(HDC, LPCWSTR, int, LPWORD, DWORD);
static pGetCharWidthFloatA orgGetCharWidthFloatA = GetCharWidthFloatA;
static pGetCharWidthFloatW orgGetCharWidthFloatW = GetCharWidthFloatW;
static pGetCharWidthI orgGetCharWidthI = GetCharWidthI;
static pGetCharABCWidthsI orgGetCharABCWidthsI = GetCharABCWidthsI;
static pGetCharacterPlacementA orgGetCharacterPlacementA = GetCharacterPlacementA;
static pGetCharacterPlacementW orgGetCharacterPlacementW = GetCharacterPlacementW;
static pGetKerningPairsA orgGetKerningPairsA = GetKerningPairsA;
static pGetKerningPairsW orgGetKerningPairsW = GetKerningPairsW;
static pGetGlyphIndicesA orgGetGlyphIndicesA = GetGlyphIndicesA;
static pGetGlyphIndicesW orgGetGlyphIndicesW = GetGlyphIndicesW;

// ===================== Font Info Supplement Types =====================
typedef UINT(WINAPI* pGetOutlineTextMetricsA)(HDC, UINT, LPOUTLINETEXTMETRICA);
typedef UINT(WINAPI* pGetOutlineTextMetricsW)(HDC, UINT, LPOUTLINETEXTMETRICW);
typedef BOOL(WINAPI* pGetTextExtentPointA)(HDC, LPCSTR, int, LPSIZE);
typedef BOOL(WINAPI* pGetTextExtentPointW)(HDC, LPCWSTR, int, LPSIZE);
typedef BOOL(WINAPI* pGetTextExtentPointI)(HDC, LPWORD, int, LPSIZE);
typedef BOOL(WINAPI* pGetTextExtentExPointI)(HDC, LPWORD, int, int, LPINT, LPINT, LPSIZE);
typedef DWORD(WINAPI* pGetFontData)(HDC, DWORD, DWORD, PVOID, DWORD);
typedef DWORD(WINAPI* pGetFontLanguageInfo)(HDC);
typedef DWORD(WINAPI* pGetFontUnicodeRanges)(HDC, LPGLYPHSET);
static pGetOutlineTextMetricsA orgGetOutlineTextMetricsA = GetOutlineTextMetricsA;
static pGetOutlineTextMetricsW orgGetOutlineTextMetricsW = GetOutlineTextMetricsW;
static pGetTextExtentPointA orgGetTextExtentPointA = GetTextExtentPointA;
static pGetTextExtentPointW orgGetTextExtentPointW = GetTextExtentPointW;
static pGetTextExtentPointI orgGetTextExtentPointI = GetTextExtentPointI;
static pGetTextExtentExPointI orgGetTextExtentExPointI = GetTextExtentExPointI;
static pGetFontData orgGetFontData = GetFontData;
static pGetFontLanguageInfo orgGetFontLanguageInfo = GetFontLanguageInfo;
static pGetFontUnicodeRanges orgGetFontUnicodeRanges = GetFontUnicodeRanges;

// ===================== User32 Text Supplement Types =====================
typedef BOOL(WINAPI* pGrayStringA)(HDC, HBRUSH, GRAYSTRINGPROC, LPARAM, int, int, int, int, int);
typedef BOOL(WINAPI* pGrayStringW)(HDC, HBRUSH, GRAYSTRINGPROC, LPARAM, int, int, int, int, int);
typedef DWORD(WINAPI* pGetTabbedTextExtentA)(HDC, LPCSTR, int, int, const int*);
typedef DWORD(WINAPI* pGetTabbedTextExtentW)(HDC, LPCWSTR, int, int, const int*);
static pGrayStringA orgGrayStringA = GrayStringA;
static pGrayStringW orgGrayStringW = GrayStringW;
static pGetTabbedTextExtentA orgGetTabbedTextExtentA = GetTabbedTextExtentA;
static pGetTabbedTextExtentW orgGetTabbedTextExtentW = GetTabbedTextExtentW;

// ===================== GDI+ Measure Types =====================
typedef GpStatus(WINAPI* pGdipMeasureString)(GpGraphics*, const WCHAR*, INT, const GpFont*, const RectF*, const GpStringFormat*, RectF*, INT*, INT*);
typedef GpStatus(WINAPI* pGdipMeasureCharacterRanges)(GpGraphics*, const WCHAR*, INT, const GpFont*, const RectF*, const GpStringFormat*, INT, GpRegion**);
typedef GpStatus(WINAPI* pGdipMeasureDriverString)(GpGraphics*, const UINT16*, INT, const GpFont*, const PointF*, INT, const Matrix*, RectF*);
static pGdipMeasureString orgGdipMeasureString = NULL;
static pGdipMeasureCharacterRanges orgGdipMeasureCharacterRanges = NULL;
static pGdipMeasureDriverString orgGdipMeasureDriverString = NULL;

// ===================== GDI+ Types =====================
typedef GpStatus(WINAPI* pGdipCreateFontFamilyFromName)(const WCHAR*, GpFontCollection*, GpFontFamily**);
typedef GpStatus(WINAPI* pGdipCreateFontFromLogfontW)(HDC, const LOGFONTW*, GpFont**);
typedef GpStatus(WINAPI* pGdipCreateFontFromLogfontA)(HDC, const LOGFONTA*, GpFont**);
typedef GpStatus(WINAPI* pGdipCreateFontFromHFONT)(HDC, HFONT, GpFont**);
typedef GpStatus(WINAPI* pGdipCreateFontFromDC)(HDC, GpFont**);
typedef GpStatus(WINAPI* pGdipCreateFont)(const GpFontFamily*, REAL, INT, Unit, GpFont**);
typedef GpStatus(WINAPI* pGdipNewPrivateFontCollection)(GpFontCollection**);
typedef GpStatus(WINAPI* pGdipPrivateAddFontFile)(GpFontCollection*, const WCHAR*);
typedef GpStatus(WINAPI* pGdipDrawString)(GpGraphics*, const WCHAR*, INT, const GpFont*, const RectF*, const GpStringFormat*, const GpBrush*);
typedef GpStatus(WINAPI* pGdipDrawDriverString)(GpGraphics*, const UINT16*, INT, const GpFont*, const GpBrush*, const PointF*, INT, const Matrix*);

static pGdipCreateFontFamilyFromName orgGdipCreateFontFamilyFromName = NULL;
static pGdipCreateFontFromLogfontW orgGdipCreateFontFromLogfontW = NULL;
static pGdipCreateFontFromLogfontA orgGdipCreateFontFromLogfontA = NULL;
static pGdipCreateFontFromHFONT orgGdipCreateFontFromHFONT = NULL;
static pGdipCreateFontFromDC orgGdipCreateFontFromDC = NULL;
static pGdipCreateFont orgGdipCreateFont = NULL;
static pGdipNewPrivateFontCollection ptrGdipNewPrivateFontCollection = NULL;
static pGdipPrivateAddFontFile ptrGdipPrivateAddFontFile = NULL;
static pGdipDrawString orgGdipDrawString = NULL;
static pGdipDrawDriverString orgGdipDrawDriverString = NULL;

// ===================== DirectWrite Types =====================
typedef HRESULT(WINAPI* pDWriteCreateFactory)(DWRITE_FACTORY_TYPE, REFIID, IUnknown**);
static pDWriteCreateFactory orgDWriteCreateFactory = NULL;

// 新增：IDWriteFactory::CreateTextFormat 虚函数索引为 15
typedef HRESULT(STDMETHODCALLTYPE* pCreateTextFormat)(
    IDWriteFactory*, const WCHAR*, IDWriteFontCollection*, DWRITE_FONT_WEIGHT,
    DWRITE_FONT_STYLE, DWRITE_FONT_STRETCH, FLOAT, const WCHAR*, IDWriteTextFormat**);
static pCreateTextFormat orgCreateTextFormat = NULL;

// IDWriteFactory::CreateTextLayout 虚函数索引为 12
typedef HRESULT(STDMETHODCALLTYPE* pCreateTextLayout)(
    IDWriteFactory*, const WCHAR*, UINT32, IDWriteTextFormat*, FLOAT, FLOAT, IDWriteTextLayout**);
static pCreateTextLayout orgCreateTextLayout = NULL;

typedef HRESULT(STDMETHODCALLTYPE* pIDWriteTextLayout_Draw)(
    IDWriteTextLayout*, void*, IDWriteTextRenderer*, FLOAT, FLOAT);
static pIDWriteTextLayout_Draw orgIDWriteTextLayout_Draw = NULL;

// ===================== Library Types =====================
typedef HMODULE(WINAPI* pLoadLibraryW)(LPCWSTR);
typedef HMODULE(WINAPI* pLoadLibraryExW)(LPCWSTR, HANDLE, DWORD);

static pLoadLibraryW orgLoadLibraryW = LoadLibraryW;
static pLoadLibraryExW orgLoadLibraryExW = LoadLibraryExW;

// Forward declarations
void InstallGdiPlusHooks();

// ===================== State =====================
static bool g_Initialized = false;
static bool g_CharsetDetected = false;
static bool g_CustomFontLoaded = false;
static bool g_GdiPlusHooksInstalled = false;
static HMODULE g_hModule = NULL;
static GpFontCollection* g_PrivateFontCollection = NULL;
static wchar_t g_FontFilePath[MAX_PATH] = { 0 };

// ===================== Codepage Spoof =====================
static DWORD SpoofCharset(DWORD cs) {
    if (!Config::EnableCodepageSpoof) return cs;
    if (cs == Config::SpoofFromCharset) {
        return Config::SpoofToCharset;
    }
    return cs;
}
static BYTE SpoofCharsetB(BYTE cs) { 
    return (BYTE)SpoofCharset((DWORD)cs); 
}

// ===================== Charset Detection =====================
static void DetectCharset(DWORD charset) {
    if (g_CharsetDetected) return;
    if (charset != DEFAULT_CHARSET && charset != OEM_CHARSET) {
        g_CharsetDetected = true;
        Config::DetectedCharset = charset;
        Utils::Log("[Charset] Game uses charset: %u (System ACP: %u)", charset, GetACP());
    } else if (charset == DEFAULT_CHARSET) {
        // DEFAULT_CHARSET means "use system default". Infer from system ACP.
        UINT acp = GetACP();
        DWORD inferred = DEFAULT_CHARSET;
        if (acp == 932) inferred = SHIFTJIS_CHARSET;
        else if (acp == 936) inferred = GB2312_CHARSET;
        else if (acp == 949) inferred = HANGUL_CHARSET;
        else if (acp == 950) inferred = CHINESEBIG5_CHARSET;
        else if (acp == 1252) inferred = ANSI_CHARSET;
        if (inferred != DEFAULT_CHARSET) {
            g_CharsetDetected = true;
            Config::DetectedCharset = inferred;
            Utils::Log("[Charset] Game uses DEFAULT_CHARSET, inferred %u from ACP %u", inferred, acp);
        }
    }
}

// ===================== Font Picker Thread =====================
static DWORD g_pickerThreadId = 0;

static bool IsPickerThread() {
    return g_pickerThreadId != 0 && GetCurrentThreadId() == g_pickerThreadId;
}

static unsigned __stdcall FontPickerThread(void* param) {
    g_pickerThreadId = GetCurrentThreadId();
    FontPicker::Init((HMODULE)param);
    return 0;
}

// ===================== Private Font Collection (GDI+) =====================
static bool FindFontFilePath() {
    if (g_FontFilePath[0] != 0) return true;
    WCHAR rootDir[MAX_PATH];
    GetModuleFileNameW(g_hModule, rootDir, MAX_PATH);
    PathRemoveFileSpecW(rootDir);
    swprintf_s(g_FontFilePath, MAX_PATH, L"%s\\%s", rootDir, Config::FontFileName);
    if (PathFileExistsW(g_FontFilePath)) return true;
    g_FontFilePath[0] = 0;
    return false;
}

static bool LoadGdiPlusPrivateFont() {
    if (g_PrivateFontCollection != NULL) return true;
    HMODULE hGdiPlus = GetModuleHandleW(L"gdiplus.dll");
    if (!hGdiPlus) hGdiPlus = LoadLibraryW(L"gdiplus.dll");
    if (!hGdiPlus) return false;
    ptrGdipNewPrivateFontCollection = (pGdipNewPrivateFontCollection)GetProcAddress(hGdiPlus, "GdipNewPrivateFontCollection");
    ptrGdipPrivateAddFontFile = (pGdipPrivateAddFontFile)GetProcAddress(hGdiPlus, "GdipPrivateAddFontFile");
    if (!ptrGdipNewPrivateFontCollection || !ptrGdipPrivateAddFontFile) return false;
    if (!FindFontFilePath()) return false;
    GpStatus status = ptrGdipNewPrivateFontCollection(&g_PrivateFontCollection);
    if (status != 0 || g_PrivateFontCollection == NULL) return false;
    status = ptrGdipPrivateAddFontFile(g_PrivateFontCollection, g_FontFilePath);
    if (status != 0) { g_PrivateFontCollection = NULL; return false; }
    return true;
}

void EnsureInitialized() {
    if (g_Initialized) return;
    g_Initialized = true;
    g_CustomFontLoaded = Utils::LoadCustomFont(g_hModule) != FALSE;
    InstallGdiPlusHooks();
}

// ===================== HDC Font Replacement Helper =====================
static HFONT ReplaceHdcFont(HDC hdc, HFONT* pOldFont) {
    *pOldFont = NULL;
    if (IsPickerThread()) return NULL;
    if (!Config::EnableFontHook && !Config::EnableCodepageSpoof) return NULL;

    HFONT hCurFont = (HFONT)GetCurrentObject(hdc, OBJ_FONT);
    if (!hCurFont) return NULL;

    LOGFONTW lf;
    if (GetObjectW(hCurFont, sizeof(lf), &lf) == 0) return NULL;

    bool changed = false;

    if (Config::EnableFontHook && Config::EnableFaceNameReplace) {
        if (wcscmp(lf.lfFaceName, Config::ForcedFontNameW) != 0) {
            wcscpy_s(lf.lfFaceName, Config::ForcedFontNameW);
            changed = true;
        }
    }

    if (Config::EnableFontHook && Config::EnableCharsetReplace) {
        if (lf.lfCharSet != (BYTE)Config::ForcedCharset) {
            lf.lfCharSet = (BYTE)Config::ForcedCharset;
            changed = true;
        }
    }

    {
        BYTE spoofed = SpoofCharsetB(lf.lfCharSet);
        if (spoofed != lf.lfCharSet) {
            lf.lfCharSet = spoofed;
            changed = true;
        }
    }

    if (Config::EnableFontHook && Config::EnableFontHeightScale) {
        lf.lfHeight = (LONG)(lf.lfHeight * Config::FontHeightScale);
        changed = true;
    }
    if (Config::EnableFontHook && Config::EnableFontWidthScale) {
        lf.lfWidth = (LONG)(lf.lfWidth * Config::FontWidthScale);
        changed = true;
    }

    if (Config::EnableFontHook && Config::EnableFontWeight && Config::FontWeight > 0) {
        lf.lfWeight = Config::FontWeight;
        changed = true;
    }

    if (!changed) return NULL;

    HFONT hNewFont = orgCreateFontIndirectW(&lf);
    if (!hNewFont) return NULL;

    *pOldFont = (HFONT)orgSelectObject(hdc, hNewFont);
    return hNewFont;
}

// ===================== GDI Text Output Hooks (Active Font Replacement) =====================
BOOL WINAPI newTextOutA(HDC hdc, int x, int y, LPCSTR lpString, int nCount) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgTextOutA(hdc, x, y, lpString, nCount);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

BOOL WINAPI newTextOutW(HDC hdc, int x, int y, LPCWSTR lpString, int nCount) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgTextOutW(hdc, x, y, lpString, nCount);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

BOOL WINAPI newExtTextOutA(HDC hdc, int x, int y, UINT options, const RECT* lprect, LPCSTR lpString, UINT nCount, const int* lpDx) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgExtTextOutA(hdc, x, y, options, lprect, lpString, nCount, lpDx);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

BOOL WINAPI newExtTextOutW(HDC hdc, int x, int y, UINT options, const RECT* lprect, LPCWSTR lpString, UINT nCount, const int* lpDx) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgExtTextOutW(hdc, x, y, options, lprect, lpString, nCount, lpDx);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

int WINAPI newDrawTextA(HDC hdc, LPCSTR lpchText, int nCount, LPRECT lpRect, UINT format) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    int ret = orgDrawTextA(hdc, lpchText, nCount, lpRect, format);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

int WINAPI newDrawTextW(HDC hdc, LPCWSTR lpchText, int nCount, LPRECT lpRect, UINT format) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    int ret = orgDrawTextW(hdc, lpchText, nCount, lpRect, format);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

int WINAPI newDrawTextExA(HDC hdc, LPSTR lpchText, int nCount, LPRECT lpRect, UINT format, LPDRAWTEXTPARAMS lpdtp) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    int ret = orgDrawTextExA(hdc, lpchText, nCount, lpRect, format, lpdtp);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

int WINAPI newDrawTextExW(HDC hdc, LPWSTR lpchText, int nCount, LPRECT lpRect, UINT format, LPDRAWTEXTPARAMS lpdtp) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    int ret = orgDrawTextExW(hdc, lpchText, nCount, lpRect, format, lpdtp);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

BOOL WINAPI newPolyTextOutA(HDC hdc, const POLYTEXTA* ppt, int nTexts) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgPolyTextOutA(hdc, ppt, nTexts);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

BOOL WINAPI newPolyTextOutW(HDC hdc, const POLYTEXTW* ppt, int nTexts) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgPolyTextOutW(hdc, ppt, nTexts);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

LONG WINAPI newTabbedTextOutA(HDC hdc, int x, int y, LPCSTR lpString, int nCount, int nTabPositions, const int* lpnTabPositions, int nTabOrigin) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    LONG ret = orgTabbedTextOutA(hdc, x, y, lpString, nCount, nTabPositions, lpnTabPositions, nTabOrigin);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

LONG WINAPI newTabbedTextOutW(HDC hdc, int x, int y, LPCWSTR lpString, int nCount, int nTabPositions, const int* lpnTabPositions, int nTabOrigin) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    LONG ret = orgTabbedTextOutW(hdc, x, y, lpString, nCount, nTabPositions, lpnTabPositions, nTabOrigin);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

// ===================== SelectObject Hook =====================
// This is the KEY hook for games that cache HFONT handles.
// When the game selects a cached font into a DC, we intercept it and
// replace with our font before any glyph rendering happens.
static __declspec(thread) bool g_inSelectObject = false;
static __declspec(thread) HFONT g_hLastOrgFont = NULL;
static __declspec(thread) HFONT g_hLastNewFont = NULL;
static __declspec(thread) LONG g_hLastConfigVersion = -1;

HGDIOBJ WINAPI newSelectObject(HDC hdc, HGDIOBJ h) {
    if (g_inSelectObject || IsPickerThread() || (!Config::EnableFontHook && !Config::EnableCodepageSpoof))
        return orgSelectObject(hdc, h);

    // Only intercept font objects (OBJ_FONT)
    if (h && GetObjectType(h) == OBJ_FONT) {
        
        // CHECK IF RELOAD IS PENDING -- Clear Cache!
        if (Config::NeedFontReload) {
            if (g_hLastNewFont) {
                DeleteObject(g_hLastNewFont);
                g_hLastNewFont = NULL;
            }
            g_hLastOrgFont = NULL;
            g_hLastConfigVersion = -1;
            return orgSelectObject(hdc, h);
        }

        // Fast path: if this is the same original font we just handled, 
        // AND the config hasn't changed, use our cached replacement
        if (h == g_hLastOrgFont && g_hLastNewFont && g_hLastConfigVersion == Config::ConfigVersion) {
            return orgSelectObject(hdc, g_hLastNewFont);
        }

        LOGFONTW lf;
        if (GetObjectW((HFONT)h, sizeof(lf), &lf)) {
            bool changed = false;

            if (Config::EnableFontHook && Config::EnableFaceNameReplace) {
                if (wcscmp(lf.lfFaceName, Config::ForcedFontNameW) != 0) {
                    wcscpy_s(lf.lfFaceName, Config::ForcedFontNameW);
                    changed = true;
                }
            }
            if (Config::EnableFontHook && Config::EnableCharsetReplace) {
                if (lf.lfCharSet != (BYTE)Config::ForcedCharset) {
                    lf.lfCharSet = (BYTE)Config::ForcedCharset;
                    changed = true;
                }
            }
            {
                BYTE spoofed = SpoofCharsetB(lf.lfCharSet);
                if (spoofed != lf.lfCharSet) {
                    lf.lfCharSet = spoofed;
                    changed = true;
                }
            }
            if (Config::EnableFontHook && Config::EnableFontHeightScale) {
                lf.lfHeight = (LONG)(lf.lfHeight * Config::FontHeightScale);
                changed = true;
            }
            if (Config::EnableFontHook && Config::EnableFontWidthScale) {
                lf.lfWidth = (LONG)(lf.lfWidth * Config::FontWidthScale);
                changed = true;
            }
            if (Config::EnableFontHook && Config::EnableFontWeight && Config::FontWeight > 0) {
                lf.lfWeight = Config::FontWeight;
                changed = true;
            }

            if (changed) {
                g_inSelectObject = true;
                HFONT hNew = orgCreateFontIndirectW(&lf);
                if (hNew) {
                    // Cleanup previous cached font for this thread to avoid leaks
                    if (g_hLastNewFont) DeleteObject(g_hLastNewFont);
                    
                    g_hLastOrgFont = (HFONT)h;
                    g_hLastNewFont = hNew;
                    g_hLastConfigVersion = Config::ConfigVersion;

                    HGDIOBJ old = orgSelectObject(hdc, hNew);
                    g_inSelectObject = false;
                    return old;
                }
                g_inSelectObject = false;
            }
        }
    }

    return orgSelectObject(hdc, h);
}

// ===================== GetGlyphOutline Hooks =====================
// Many Galgame engines (KiriKiri, BGI, CMVS, YU-RIS, etc.) use GetGlyphOutlineW
// to extract glyph bitmaps and render them as textures. This is the most critical
// hook for engines that cache rendered text.
DWORD WINAPI newGetGlyphOutlineA(HDC hdc, UINT uChar, UINT fuFormat, LPGLYPHMETRICS lpgm, DWORD cjBuffer, LPVOID pvBuffer, const MAT2* lpmat2) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    DWORD ret = orgGetGlyphOutlineA(hdc, uChar, fuFormat, lpgm, cjBuffer, pvBuffer, lpmat2);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

DWORD WINAPI newGetGlyphOutlineW(HDC hdc, UINT uChar, UINT fuFormat, LPGLYPHMETRICS lpgm, DWORD cjBuffer, LPVOID pvBuffer, const MAT2* lpmat2) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    DWORD ret = orgGetGlyphOutlineW(hdc, uChar, fuFormat, lpgm, cjBuffer, pvBuffer, lpmat2);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

// ===================== Character Width / Metrics Hooks =====================
// Engines measure text layout using these APIs. Without hooking them, the
// text layout uses the OLD font's metrics, causing misaligned/clipped text.
BOOL WINAPI newGetCharABCWidthsA(HDC hdc, UINT wFirst, UINT wLast, LPABC lpABC) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgGetCharABCWidthsA(hdc, wFirst, wLast, lpABC);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

BOOL WINAPI newGetCharABCWidthsW(HDC hdc, UINT wFirst, UINT wLast, LPABC lpABC) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgGetCharABCWidthsW(hdc, wFirst, wLast, lpABC);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

BOOL WINAPI newGetCharABCWidthsFloatA(HDC hdc, UINT iFirst, UINT iLast, LPABCFLOAT lpABC) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgGetCharABCWidthsFloatA(hdc, iFirst, iLast, lpABC);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

BOOL WINAPI newGetCharABCWidthsFloatW(HDC hdc, UINT iFirst, UINT iLast, LPABCFLOAT lpABC) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgGetCharABCWidthsFloatW(hdc, iFirst, iLast, lpABC);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

BOOL WINAPI newGetCharWidthA(HDC hdc, UINT iFirst, UINT iLast, LPINT lpBuffer) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgGetCharWidthA(hdc, iFirst, iLast, lpBuffer);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

BOOL WINAPI newGetCharWidthW(HDC hdc, UINT iFirst, UINT iLast, LPINT lpBuffer) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgGetCharWidthW(hdc, iFirst, iLast, lpBuffer);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

BOOL WINAPI newGetCharWidth32A(HDC hdc, UINT iFirst, UINT iLast, LPINT lpBuffer) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgGetCharWidth32A(hdc, iFirst, iLast, lpBuffer);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

BOOL WINAPI newGetCharWidth32W(HDC hdc, UINT iFirst, UINT iLast, LPINT lpBuffer) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgGetCharWidth32W(hdc, iFirst, iLast, lpBuffer);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

BOOL WINAPI newGetTextExtentPoint32A(HDC hdc, LPCSTR lpString, int c, LPSIZE psizl) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgGetTextExtentPoint32A(hdc, lpString, c, psizl);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

BOOL WINAPI newGetTextExtentPoint32W(HDC hdc, LPCWSTR lpString, int c, LPSIZE psizl) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgGetTextExtentPoint32W(hdc, lpString, c, psizl);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

BOOL WINAPI newGetTextExtentExPointA(HDC hdc, LPCSTR lpszString, int cchString, int nMaxExtent, LPINT lpnFit, LPINT lpnDx, LPSIZE lpSize) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgGetTextExtentExPointA(hdc, lpszString, cchString, nMaxExtent, lpnFit, lpnDx, lpSize);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

BOOL WINAPI newGetTextExtentExPointW(HDC hdc, LPCWSTR lpszString, int cchString, int nMaxExtent, LPINT lpnFit, LPINT lpnDx, LPSIZE lpSize) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgGetTextExtentExPointW(hdc, lpszString, cchString, nMaxExtent, lpnFit, lpnDx, lpSize);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

// ===================== GetTextFace Hooks =====================
// Some engines call GetTextFace to verify the current font. If the returned
// name doesn't match what the engine expects, it may recreate the font or
// skip rendering. We intercept and return our forced font name.
int WINAPI newGetTextFaceA(HDC hdc, int c, LPSTR lpName) {
    if (IsPickerThread() || !Config::EnableFontHook || !Config::EnableFaceNameReplace)
        return orgGetTextFaceA(hdc, c, lpName);
    int needed = (int)strlen(Config::ForcedFontNameA) + 1;
    if (lpName == NULL || c == 0) return needed;
    strncpy_s(lpName, c, Config::ForcedFontNameA, _TRUNCATE);
    return std::min(needed, c);
}

int WINAPI newGetTextFaceW(HDC hdc, int c, LPWSTR lpName) {
    if (IsPickerThread() || !Config::EnableFontHook || !Config::EnableFaceNameReplace)
        return orgGetTextFaceW(hdc, c, lpName);
    int needed = (int)wcslen(Config::ForcedFontNameW) + 1;
    if (lpName == NULL || c == 0) return needed;
    wcsncpy_s(lpName, c, Config::ForcedFontNameW, _TRUNCATE);
    return std::min(needed, c);
}

// ===================== GetObject Hooks =====================
// When the game queries a cached HFONT to check font name/charset,
// we patch the returned LOGFONT to match our replacement font.
// This makes engines believe their cached font IS the replacement font,
// preventing them from ignoring changes or reverting.
int WINAPI newGetObjectA(HANDLE h, int c, LPVOID pv) {
    int ret = orgGetObjectA(h, c, pv);
    if (IsPickerThread() || (!Config::EnableFontHook && !Config::EnableCodepageSpoof))
        return ret;
    if (ret > 0 && pv && h && GetObjectType(h) == OBJ_FONT && c >= (int)sizeof(LOGFONTA)) {
        LOGFONTA* lf = (LOGFONTA*)pv;
        if (Config::EnableFontHook && Config::EnableFaceNameReplace) {
            strncpy_s(lf->lfFaceName, Config::ForcedFontNameA, LF_FACESIZE - 1);
        }
        if (Config::EnableFontHook && Config::EnableCharsetReplace) {
            lf->lfCharSet = (BYTE)Config::ForcedCharset;
        }
        lf->lfCharSet = SpoofCharsetB(lf->lfCharSet);
    }
    return ret;
}

int WINAPI newGetObjectW(HANDLE h, int c, LPVOID pv) {
    int ret = orgGetObjectW(h, c, pv);
    if (IsPickerThread() || (!Config::EnableFontHook && !Config::EnableCodepageSpoof))
        return ret;
    if (ret > 0 && pv && h && GetObjectType(h) == OBJ_FONT && c >= (int)sizeof(LOGFONTW)) {
        LOGFONTW* lf = (LOGFONTW*)pv;
        if (Config::EnableFontHook && Config::EnableFaceNameReplace) {
            wcscpy_s(lf->lfFaceName, Config::ForcedFontNameW);
        }
        if (Config::EnableFontHook && Config::EnableCharsetReplace) {
            lf->lfCharSet = (BYTE)Config::ForcedCharset;
        }
        lf->lfCharSet = SpoofCharsetB(lf->lfCharSet);
    }
    return ret;
}

// ===================== GetTextMetrics Hooks =====================
// Some engines call GetTextMetrics to verify charset or measure text.
// We replace the font in the DC before querying metrics, ensuring
// the engine gets metrics for our replacement font.
BOOL WINAPI newGetTextMetricsA(HDC hdc, LPTEXTMETRICA lptm) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgGetTextMetricsA(hdc, lptm);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

BOOL WINAPI newGetTextMetricsW(HDC hdc, LPTEXTMETRICW lptm) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgGetTextMetricsW(hdc, lptm);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

// ===================== EnumFontFamiliesEx Hooks =====================
// Some engines re-enumerate fonts when the user changes settings. We inject
// our replacement font into the enumeration so the engine "discovers" it
// and rebuilds its font cache using our font.
int WINAPI newEnumFontFamiliesExA(HDC hdc, LPLOGFONTA lpLogfont, FONTENUMPROCA lpProc, LPARAM lParam, DWORD dwFlags) {
    // Always call original first
    int ret = orgEnumFontFamiliesExA(hdc, lpLogfont, lpProc, lParam, dwFlags);
    // Then inject our font if hook is active
    if (Config::EnableFontHook && Config::EnableFaceNameReplace && lpProc) {
        LOGFONTA lf = {};
        strncpy_s(lf.lfFaceName, Config::ForcedFontNameA, LF_FACESIZE - 1);
        lf.lfCharSet = Config::EnableCharsetReplace ? (BYTE)Config::ForcedCharset : DEFAULT_CHARSET;
        TEXTMETRICA tm = {};
        lpProc(&lf, &tm, TRUETYPE_FONTTYPE, lParam);
    }
    return ret;
}

int WINAPI newEnumFontFamiliesExW(HDC hdc, LPLOGFONTW lpLogfont, FONTENUMPROCW lpProc, LPARAM lParam, DWORD dwFlags) {
    int ret = orgEnumFontFamiliesExW(hdc, lpLogfont, lpProc, lParam, dwFlags);
    if (Config::EnableFontHook && Config::EnableFaceNameReplace && lpProc) {
        LOGFONTW lf = {};
        wcsncpy_s(lf.lfFaceName, Config::ForcedFontNameW, LF_FACESIZE - 1);
        lf.lfCharSet = Config::EnableCharsetReplace ? (BYTE)Config::ForcedCharset : DEFAULT_CHARSET;
        TEXTMETRICW tm = {};
        lpProc(&lf, &tm, TRUETYPE_FONTTYPE, lParam);
    }
    return ret;
}

// ===================== CreateFontIndirectEx Hooks =====================
HFONT WINAPI newCreateFontIndirectExA(const ENUMLOGFONTEXDVA* lpelfe) {
    if (!lpelfe) return orgCreateFontIndirectExA(lpelfe);
    EnsureInitialized();
    if (!Config::EnableFontHook && !Config::EnableCodepageSpoof) return orgCreateFontIndirectExA(lpelfe);
    ENUMLOGFONTEXDVA elfe = *lpelfe;
    if (Config::EnableFaceNameReplace) strncpy_s(elfe.elfEnumLogfontEx.elfLogFont.lfFaceName, Config::ForcedFontNameA, LF_FACESIZE - 1);
    if (Config::EnableCharsetReplace) elfe.elfEnumLogfontEx.elfLogFont.lfCharSet = (BYTE)Config::ForcedCharset;
    elfe.elfEnumLogfontEx.elfLogFont.lfCharSet = SpoofCharsetB(elfe.elfEnumLogfontEx.elfLogFont.lfCharSet);
    if (Config::EnableFontHeightScale) elfe.elfEnumLogfontEx.elfLogFont.lfHeight = (LONG)(elfe.elfEnumLogfontEx.elfLogFont.lfHeight * Config::FontHeightScale);
    if (Config::EnableFontWidthScale) elfe.elfEnumLogfontEx.elfLogFont.lfWidth = (LONG)(elfe.elfEnumLogfontEx.elfLogFont.lfWidth * Config::FontWidthScale);
    if (Config::EnableFontWeight && Config::FontWeight > 0) elfe.elfEnumLogfontEx.elfLogFont.lfWeight = Config::FontWeight;
    return orgCreateFontIndirectExA(&elfe);
}

HFONT WINAPI newCreateFontIndirectExW(const ENUMLOGFONTEXDVW* lpelfe) {
    if (!lpelfe) return orgCreateFontIndirectExW(lpelfe);
    EnsureInitialized();
    if (!Config::EnableFontHook && !Config::EnableCodepageSpoof) return orgCreateFontIndirectExW(lpelfe);
    ENUMLOGFONTEXDVW elfe = *lpelfe;
    if (Config::EnableFaceNameReplace) wcscpy_s(elfe.elfEnumLogfontEx.elfLogFont.lfFaceName, Config::ForcedFontNameW);
    if (Config::EnableCharsetReplace) elfe.elfEnumLogfontEx.elfLogFont.lfCharSet = (BYTE)Config::ForcedCharset;
    elfe.elfEnumLogfontEx.elfLogFont.lfCharSet = SpoofCharsetB(elfe.elfEnumLogfontEx.elfLogFont.lfCharSet);
    if (Config::EnableFontHeightScale) elfe.elfEnumLogfontEx.elfLogFont.lfHeight = (LONG)(elfe.elfEnumLogfontEx.elfLogFont.lfHeight * Config::FontHeightScale);
    if (Config::EnableFontWidthScale) elfe.elfEnumLogfontEx.elfLogFont.lfWidth = (LONG)(elfe.elfEnumLogfontEx.elfLogFont.lfWidth * Config::FontWidthScale);
    if (Config::EnableFontWeight && Config::FontWeight > 0) elfe.elfEnumLogfontEx.elfLogFont.lfWeight = Config::FontWeight;
    return orgCreateFontIndirectExW(&elfe);
}

// ===================== Font Resource Hooks (Passthrough) =====================
int WINAPI newAddFontResourceA(LPCSTR lpFileName) { return orgAddFontResourceA(lpFileName); }
int WINAPI newAddFontResourceW(LPCWSTR lpFileName) { return orgAddFontResourceW(lpFileName); }
int WINAPI newAddFontResourceExA(LPCSTR name, DWORD fl, PVOID res) { return orgAddFontResourceExA(name, fl, res); }
int WINAPI newAddFontResourceExW(LPCWSTR name, DWORD fl, PVOID res) { return orgAddFontResourceExW(name, fl, res); }
HANDLE WINAPI newAddFontMemResourceEx(PVOID pFileView, DWORD cjSize, PVOID pvResrved, DWORD* pNumFonts) { return orgAddFontMemResourceEx(pFileView, cjSize, pvResrved, pNumFonts); }
BOOL WINAPI newRemoveFontResourceA(LPCSTR lpFileName) { return orgRemoveFontResourceA(lpFileName); }
BOOL WINAPI newRemoveFontResourceW(LPCWSTR lpFileName) { return orgRemoveFontResourceW(lpFileName); }
BOOL WINAPI newRemoveFontResourceExA(LPCSTR name, DWORD fl, PVOID res) { return orgRemoveFontResourceExA(name, fl, res); }
BOOL WINAPI newRemoveFontResourceExW(LPCWSTR name, DWORD fl, PVOID res) { return orgRemoveFontResourceExW(name, fl, res); }
BOOL WINAPI newRemoveFontMemResourceEx(HANDLE fh) { return orgRemoveFontMemResourceEx(fh); }

// ===================== Old-Style Font Enumeration Hooks =====================
int WINAPI newEnumFontsA(HDC hdc, LPCSTR lpFaceName, FONTENUMPROCA lpProc, LPARAM lParam) {
    int ret = orgEnumFontsA(hdc, lpFaceName, lpProc, lParam);
    if (Config::EnableFontHook && Config::EnableFaceNameReplace && lpProc) {
        LOGFONTA lf = {};
        strncpy_s(lf.lfFaceName, Config::ForcedFontNameA, LF_FACESIZE - 1);
        lf.lfCharSet = Config::EnableCharsetReplace ? (BYTE)Config::ForcedCharset : DEFAULT_CHARSET;
        TEXTMETRICA tm = {};
        lpProc(&lf, &tm, TRUETYPE_FONTTYPE, lParam);
    }
    return ret;
}

int WINAPI newEnumFontsW(HDC hdc, LPCWSTR lpFaceName, FONTENUMPROCW lpProc, LPARAM lParam) {
    int ret = orgEnumFontsW(hdc, lpFaceName, lpProc, lParam);
    if (Config::EnableFontHook && Config::EnableFaceNameReplace && lpProc) {
        LOGFONTW lf = {};
        wcsncpy_s(lf.lfFaceName, Config::ForcedFontNameW, LF_FACESIZE - 1);
        lf.lfCharSet = Config::EnableCharsetReplace ? (BYTE)Config::ForcedCharset : DEFAULT_CHARSET;
        TEXTMETRICW tm = {};
        lpProc(&lf, &tm, TRUETYPE_FONTTYPE, lParam);
    }
    return ret;
}

int WINAPI newEnumFontFamiliesA(HDC hdc, LPCSTR lpFaceName, FONTENUMPROCA lpProc, LPARAM lParam) {
    int ret = orgEnumFontFamiliesA(hdc, lpFaceName, lpProc, lParam);
    if (Config::EnableFontHook && Config::EnableFaceNameReplace && lpProc) {
        LOGFONTA lf = {};
        strncpy_s(lf.lfFaceName, Config::ForcedFontNameA, LF_FACESIZE - 1);
        lf.lfCharSet = Config::EnableCharsetReplace ? (BYTE)Config::ForcedCharset : DEFAULT_CHARSET;
        TEXTMETRICA tm = {};
        lpProc(&lf, &tm, TRUETYPE_FONTTYPE, lParam);
    }
    return ret;
}

int WINAPI newEnumFontFamiliesW(HDC hdc, LPCWSTR lpFaceName, FONTENUMPROCW lpProc, LPARAM lParam) {
    int ret = orgEnumFontFamiliesW(hdc, lpFaceName, lpProc, lParam);
    if (Config::EnableFontHook && Config::EnableFaceNameReplace && lpProc) {
        LOGFONTW lf = {};
        wcsncpy_s(lf.lfFaceName, Config::ForcedFontNameW, LF_FACESIZE - 1);
        lf.lfCharSet = Config::EnableCharsetReplace ? (BYTE)Config::ForcedCharset : DEFAULT_CHARSET;
        TEXTMETRICW tm = {};
        lpProc(&lf, &tm, TRUETYPE_FONTTYPE, lParam);
    }
    return ret;
}

// ===================== Glyph/Width Supplement Hooks (HDC Replace) =====================
BOOL WINAPI newGetCharWidthFloatA(HDC hdc, UINT iFirst, UINT iLast, PFLOAT lpBuffer) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgGetCharWidthFloatA(hdc, iFirst, iLast, lpBuffer);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

BOOL WINAPI newGetCharWidthFloatW(HDC hdc, UINT iFirst, UINT iLast, PFLOAT lpBuffer) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgGetCharWidthFloatW(hdc, iFirst, iLast, lpBuffer);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

BOOL WINAPI newGetCharWidthI(HDC hdc, UINT giFirst, UINT cgi, LPWORD pgi, LPINT piWidths) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgGetCharWidthI(hdc, giFirst, cgi, pgi, piWidths);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

BOOL WINAPI newGetCharABCWidthsI(HDC hdc, UINT giFirst, UINT cgi, LPWORD pgi, LPABC lpabc) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgGetCharABCWidthsI(hdc, giFirst, cgi, pgi, lpabc);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

DWORD WINAPI newGetCharacterPlacementA(HDC hdc, LPCSTR lpString, int nCount, int nMexExtent, LPGCP_RESULTSA lpResults, DWORD dwFlags) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    DWORD ret = orgGetCharacterPlacementA(hdc, lpString, nCount, nMexExtent, lpResults, dwFlags);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

DWORD WINAPI newGetCharacterPlacementW(HDC hdc, LPCWSTR lpString, int nCount, int nMexExtent, LPGCP_RESULTSW lpResults, DWORD dwFlags) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    DWORD ret = orgGetCharacterPlacementW(hdc, lpString, nCount, nMexExtent, lpResults, dwFlags);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

DWORD WINAPI newGetKerningPairsA(HDC hdc, DWORD nPairs, LPKERNINGPAIR lpKernPair) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    DWORD ret = orgGetKerningPairsA(hdc, nPairs, lpKernPair);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

DWORD WINAPI newGetKerningPairsW(HDC hdc, DWORD nPairs, LPKERNINGPAIR lpKernPair) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    DWORD ret = orgGetKerningPairsW(hdc, nPairs, lpKernPair);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

DWORD WINAPI newGetGlyphIndicesA(HDC hdc, LPCSTR lpstr, int c, LPWORD pgi, DWORD fl) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    DWORD ret = orgGetGlyphIndicesA(hdc, lpstr, c, pgi, fl);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

DWORD WINAPI newGetGlyphIndicesW(HDC hdc, LPCWSTR lpstr, int c, LPWORD pgi, DWORD fl) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    DWORD ret = orgGetGlyphIndicesW(hdc, lpstr, c, pgi, fl);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

// ===================== Font Info Supplement Hooks (HDC Replace) =====================
UINT WINAPI newGetOutlineTextMetricsA(HDC hdc, UINT cbData, LPOUTLINETEXTMETRICA lpOTM) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    UINT ret = orgGetOutlineTextMetricsA(hdc, cbData, lpOTM);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

UINT WINAPI newGetOutlineTextMetricsW(HDC hdc, UINT cbData, LPOUTLINETEXTMETRICW lpOTM) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    UINT ret = orgGetOutlineTextMetricsW(hdc, cbData, lpOTM);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

BOOL WINAPI newGetTextExtentPointA(HDC hdc, LPCSTR lpString, int c, LPSIZE lpsz) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgGetTextExtentPointA(hdc, lpString, c, lpsz);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

BOOL WINAPI newGetTextExtentPointW(HDC hdc, LPCWSTR lpString, int c, LPSIZE lpsz) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgGetTextExtentPointW(hdc, lpString, c, lpsz);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

BOOL WINAPI newGetTextExtentPointI(HDC hdc, LPWORD pgiIn, int cgi, LPSIZE pSize) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgGetTextExtentPointI(hdc, pgiIn, cgi, pSize);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

BOOL WINAPI newGetTextExtentExPointI(HDC hdc, LPWORD lpwszString, int cwchString, int nMaxExtent, LPINT lpnFit, LPINT lpnDx, LPSIZE lpSize) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgGetTextExtentExPointI(hdc, lpwszString, cwchString, nMaxExtent, lpnFit, lpnDx, lpSize);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

DWORD WINAPI newGetFontData(HDC hdc, DWORD dwTable, DWORD dwOffset, PVOID pvBuffer, DWORD cjBuffer) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    DWORD ret = orgGetFontData(hdc, dwTable, dwOffset, pvBuffer, cjBuffer);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

DWORD WINAPI newGetFontLanguageInfo(HDC hdc) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    DWORD ret = orgGetFontLanguageInfo(hdc);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

DWORD WINAPI newGetFontUnicodeRanges(HDC hdc, LPGLYPHSET lpgs) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    DWORD ret = orgGetFontUnicodeRanges(hdc, lpgs);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

// ===================== User32 Text Supplement Hooks =====================
BOOL WINAPI newGrayStringA(HDC hdc, HBRUSH hBrush, GRAYSTRINGPROC lpOutputFunc, LPARAM lpData, int nCount, int X, int Y, int nWidth, int nHeight) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgGrayStringA(hdc, hBrush, lpOutputFunc, lpData, nCount, X, Y, nWidth, nHeight);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

BOOL WINAPI newGrayStringW(HDC hdc, HBRUSH hBrush, GRAYSTRINGPROC lpOutputFunc, LPARAM lpData, int nCount, int X, int Y, int nWidth, int nHeight) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    BOOL ret = orgGrayStringW(hdc, hBrush, lpOutputFunc, lpData, nCount, X, Y, nWidth, nHeight);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

DWORD WINAPI newGetTabbedTextExtentA(HDC hdc, LPCSTR lpString, int chCount, int nTabPositions, const int* lpnTabStopPositions) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    DWORD ret = orgGetTabbedTextExtentA(hdc, lpString, chCount, nTabPositions, lpnTabStopPositions);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

DWORD WINAPI newGetTabbedTextExtentW(HDC hdc, LPCWSTR lpString, int chCount, int nTabPositions, const int* lpnTabStopPositions) {
    HFONT hOld, hNew = ReplaceHdcFont(hdc, &hOld);
    DWORD ret = orgGetTabbedTextExtentW(hdc, lpString, chCount, nTabPositions, lpnTabStopPositions);
    if (hNew) { orgSelectObject(hdc, hOld); DeleteObject(hNew); }
    return ret;
}

// ===================== GDI+ Measure Hooks (Passthrough) =====================
GpStatus WINAPI newGdipMeasureString(GpGraphics* graphics, const WCHAR* string, INT length, const GpFont* font, const RectF* layoutRect, const GpStringFormat* stringFormat, RectF* boundingBox, INT* codepointsFitted, INT* linesFilled) {
    return orgGdipMeasureString(graphics, string, length, font, layoutRect, stringFormat, boundingBox, codepointsFitted, linesFilled);
}

GpStatus WINAPI newGdipMeasureCharacterRanges(GpGraphics* graphics, const WCHAR* string, INT length, const GpFont* font, const RectF* layoutRect, const GpStringFormat* stringFormat, INT regionCount, GpRegion** regions) {
    return orgGdipMeasureCharacterRanges(graphics, string, length, font, layoutRect, stringFormat, regionCount, regions);
}

GpStatus WINAPI newGdipMeasureDriverString(GpGraphics* graphics, const UINT16* text, INT length, const GpFont* font, const PointF* positions, INT flags, const Matrix* matrix, RectF* boundingBox) {
    return orgGdipMeasureDriverString(graphics, text, length, font, positions, flags, matrix, boundingBox);
}

// ===================== GDI Font Creation Hooks =====================
HFONT WINAPI newCreateFontA(int nH, int nW, int nE, int nO, int nWt, DWORD fI, DWORD fU, DWORD fS, DWORD fC, DWORD fOP, DWORD fCP, DWORD fQ, DWORD fPF, LPCSTR lpszF) {
    EnsureInitialized();
    DetectCharset(fC);

    if (IsPickerThread() || (!Config::EnableFontHook && !Config::EnableCodepageSpoof))
        return orgCreateFontA(nH, nW, nE, nO, nWt, fI, fU, fS, fC, fOP, fCP, fQ, fPF, lpszF);

    DWORD fCs = Config::EnableCharsetReplace ? Config::ForcedCharset : fC;
    fCs = SpoofCharset(fCs);

    int fH = Config::EnableFontHeightScale ? (int)(nH * Config::FontHeightScale) : nH;
    int fW = Config::EnableFontWidthScale ? (int)(nW * Config::FontWidthScale) : nW;
    int fWt = (Config::EnableFontWeight && Config::FontWeight > 0) ? Config::FontWeight : nWt;

    if (Config::EnableFontHook && Config::EnableFaceNameReplace) {
        return orgCreateFontW(fH, fW, nE, nO, fWt, fI, fU, fS, fCs, fOP, fCP, fQ, fPF, Config::ForcedFontNameW);
    }
    return orgCreateFontA(fH, fW, nE, nO, fWt, fI, fU, fS, fCs, fOP, fCP, fQ, fPF, lpszF);
}

HFONT WINAPI newCreateFontIndirectA(const LOGFONTA* lplf) {
    if (!lplf) return orgCreateFontIndirectA(lplf);
    EnsureInitialized();
    DetectCharset(lplf->lfCharSet);

    if (!Config::EnableFontHook && !Config::EnableCodepageSpoof) return orgCreateFontIndirectA(lplf);

    if (Config::EnableFontHook && Config::EnableFaceNameReplace) {
        LOGFONTW lfW = {};
        lfW.lfHeight = lplf->lfHeight;
        lfW.lfWidth = lplf->lfWidth;
        lfW.lfEscapement = lplf->lfEscapement;
        lfW.lfOrientation = lplf->lfOrientation;
        lfW.lfWeight = lplf->lfWeight;
        lfW.lfItalic = lplf->lfItalic;
        lfW.lfUnderline = lplf->lfUnderline;
        lfW.lfStrikeOut = lplf->lfStrikeOut;
        lfW.lfCharSet = Config::EnableCharsetReplace ? (BYTE)Config::ForcedCharset : lplf->lfCharSet;
        lfW.lfCharSet = SpoofCharsetB(lfW.lfCharSet);
        lfW.lfOutPrecision = lplf->lfOutPrecision;
        lfW.lfClipPrecision = lplf->lfClipPrecision;
        lfW.lfQuality = lplf->lfQuality;
        lfW.lfPitchAndFamily = lplf->lfPitchAndFamily;
        wcscpy_s(lfW.lfFaceName, Config::ForcedFontNameW);

        if (Config::EnableFontHeightScale) lfW.lfHeight = (LONG)(lfW.lfHeight * Config::FontHeightScale);
        if (Config::EnableFontWidthScale) lfW.lfWidth = (LONG)(lfW.lfWidth * Config::FontWidthScale);
        if (Config::EnableFontWeight && Config::FontWeight > 0) lfW.lfWeight = Config::FontWeight;

        return orgCreateFontIndirectW(&lfW);
    }

    LOGFONTA lf = *lplf;
    if (Config::EnableCharsetReplace) lf.lfCharSet = (BYTE)Config::ForcedCharset;
    lf.lfCharSet = SpoofCharsetB(lf.lfCharSet);

    if (Config::EnableFontHeightScale) lf.lfHeight = (LONG)(lf.lfHeight * Config::FontHeightScale);
    if (Config::EnableFontWidthScale) lf.lfWidth = (LONG)(lf.lfWidth * Config::FontWidthScale);
    if (Config::EnableFontWeight && Config::FontWeight > 0) lf.lfWeight = Config::FontWeight;

    return orgCreateFontIndirectA(&lf);
}

HFONT WINAPI newCreateFontW(int nH, int nW, int nE, int nO, int nWt, DWORD fI, DWORD fU, DWORD fS, DWORD fC, DWORD fOP, DWORD fCP, DWORD fQ, DWORD fPF, LPCWSTR lpszF) {
    EnsureInitialized();
    DetectCharset(fC);
    if (!Config::EnableFontHook && !Config::EnableCodepageSpoof)
        return orgCreateFontW(nH, nW, nE, nO, nWt, fI, fU, fS, fC, fOP, fCP, fQ, fPF, lpszF);

    LPCWSTR fF = Config::EnableFaceNameReplace ? Config::ForcedFontNameW : lpszF;
    DWORD fCs = Config::EnableCharsetReplace ? Config::ForcedCharset : fC;
    fCs = SpoofCharset(fCs);

    int fH = Config::EnableFontHeightScale ? (int)(nH * Config::FontHeightScale) : nH;
    int fW = Config::EnableFontWidthScale ? (int)(nW * Config::FontWidthScale) : nW;
    int fWt = (Config::EnableFontWeight && Config::FontWeight > 0) ? Config::FontWeight : nWt;

    return orgCreateFontW(fH, fW, nE, nO, fWt, fI, fU, fS, fCs, fOP, fCP, fQ, fPF, fF);
}

HFONT WINAPI newCreateFontIndirectW(const LOGFONTW* lplf) {
    if (!lplf) return orgCreateFontIndirectW(lplf);
    EnsureInitialized();
    DetectCharset(lplf->lfCharSet);
    if (!Config::EnableFontHook && !Config::EnableCodepageSpoof) return orgCreateFontIndirectW(lplf);

    LOGFONTW lf = *lplf;
    if (Config::EnableFaceNameReplace) wcscpy_s(lf.lfFaceName, Config::ForcedFontNameW);
    if (Config::EnableCharsetReplace) lf.lfCharSet = (BYTE)Config::ForcedCharset;
    lf.lfCharSet = SpoofCharsetB(lf.lfCharSet);

    if (Config::EnableFontHeightScale) lf.lfHeight = (LONG)(lf.lfHeight * Config::FontHeightScale);
    if (Config::EnableFontWidthScale) lf.lfWidth = (LONG)(lf.lfWidth * Config::FontWidthScale);
    if (Config::EnableFontWeight && Config::FontWeight > 0) lf.lfWeight = Config::FontWeight;

    return orgCreateFontIndirectW(&lf);
}

// ===================== GDI+ Hooks =====================
GpStatus WINAPI newGdipCreateFontFamilyFromName(const WCHAR* name, GpFontCollection* fontCollection, GpFontFamily** FontFamily) {
    EnsureInitialized();
    if (Config::EnableFontHook && Config::EnableFaceNameReplace && g_PrivateFontCollection) {
        GpStatus result = orgGdipCreateFontFamilyFromName(Config::ForcedFontNameW, g_PrivateFontCollection, FontFamily);
        if (result == 0) return result;
    }
    if (Config::EnableFontHook && Config::EnableFaceNameReplace) {
        return orgGdipCreateFontFamilyFromName(Config::ForcedFontNameW, fontCollection, FontFamily);
    }
    return orgGdipCreateFontFamilyFromName(name, fontCollection, FontFamily);
}

GpStatus WINAPI newGdipCreateFontFromLogfontW(HDC hdc, const LOGFONTW* logfont, GpFont** font) {
    EnsureInitialized();
    if ((Config::EnableFontHook || Config::EnableCodepageSpoof) && logfont) {
        LOGFONTW lf = *logfont;
        if (Config::EnableFaceNameReplace) wcscpy_s(lf.lfFaceName, Config::ForcedFontNameW);
        if (Config::EnableCharsetReplace) lf.lfCharSet = (BYTE)Config::ForcedCharset;
        lf.lfCharSet = SpoofCharsetB(lf.lfCharSet);
        return orgGdipCreateFontFromLogfontW(hdc, &lf, font);
    }
    return orgGdipCreateFontFromLogfontW(hdc, logfont, font);
}

GpStatus WINAPI newGdipCreateFontFromLogfontA(HDC hdc, const LOGFONTA* logfont, GpFont** font) {
    EnsureInitialized();
    if ((Config::EnableFontHook || Config::EnableCodepageSpoof) && logfont) {
        LOGFONTA lf = *logfont;
        if (Config::EnableFaceNameReplace) strcpy_s(lf.lfFaceName, Config::ForcedFontNameA);
        if (Config::EnableCharsetReplace) lf.lfCharSet = (BYTE)Config::ForcedCharset;
        lf.lfCharSet = SpoofCharsetB(lf.lfCharSet);
        return orgGdipCreateFontFromLogfontA(hdc, &lf, font);
    }
    return orgGdipCreateFontFromLogfontA(hdc, logfont, font);
}

GpStatus WINAPI newGdipCreateFontFromHFONT(HDC hdc, HFONT hfont, GpFont** font) {
    return orgGdipCreateFontFromHFONT(hdc, hfont, font);
}

GpStatus WINAPI newGdipCreateFontFromDC(HDC hdc, GpFont** font) {
    return orgGdipCreateFontFromDC(hdc, font);
}

GpStatus WINAPI newGdipCreateFont(const GpFontFamily* fontFamily, REAL emSize, INT style, Unit unit, GpFont** font) {
    return orgGdipCreateFont(fontFamily, emSize, style, unit, font);
}

GpStatus WINAPI newGdipDrawString(GpGraphics* graphics, const WCHAR* string, INT length, const GpFont* font, const RectF* layoutRect, const GpStringFormat* stringFormat, const GpBrush* brush) {
    return orgGdipDrawString(graphics, string, length, font, layoutRect, stringFormat, brush);
}

GpStatus WINAPI newGdipDrawDriverString(GpGraphics* graphics, const UINT16* text, INT length, const GpFont* font, const GpBrush* brush, const PointF* positions, INT flags, const Matrix* matrix) {
    return orgGdipDrawDriverString(graphics, text, length, font, brush, positions, flags, matrix);
}

// ===================== DirectWrite Hooks =====================

// 核心：拦截字体创建，强制修改字体名称
HRESULT STDMETHODCALLTYPE newCreateTextFormat(
    IDWriteFactory* This,
    const WCHAR* fontFamilyName,
    IDWriteFontCollection* fontCollection,
    DWRITE_FONT_WEIGHT fontWeight,
    DWRITE_FONT_STYLE fontStyle,
    DWRITE_FONT_STRETCH fontStretch,
    FLOAT fontSize,
    const WCHAR* localeName,
    IDWriteTextFormat** textFormat
) {
    const WCHAR* targetFont = fontFamilyName;
    if (Config::EnableFontHook && Config::EnableFaceNameReplace) {
        targetFont = Config::ForcedFontNameW;
    }
    return orgCreateTextFormat(This, targetFont, fontCollection, fontWeight, fontStyle, fontStretch, fontSize, localeName, textFormat);
}

HRESULT STDMETHODCALLTYPE newIDWriteTextLayout_Draw(IDWriteTextLayout* This, void* clientDrawingContext, IDWriteTextRenderer* renderer, FLOAT originX, FLOAT originY) {
    return orgIDWriteTextLayout_Draw(This, clientDrawingContext, renderer, originX, originY);
}

HRESULT STDMETHODCALLTYPE newCreateTextLayout(IDWriteFactory* This, const WCHAR* string, UINT32 stringLength, IDWriteTextFormat* textFormat, FLOAT maxWidth, FLOAT maxHeight, IDWriteTextLayout** textLayout) {
    HRESULT hr = orgCreateTextLayout(This, string, stringLength, textFormat, maxWidth, maxHeight, textLayout);
    if (SUCCEEDED(hr) && textLayout && *textLayout) {
        static std::mutex dwriteMutex;
        std::lock_guard<std::mutex> lock(dwriteMutex);
        // 如果需要进一步拦截文本渲染，在这里 hook IDWriteTextLayout 的 vtable
        if (orgIDWriteTextLayout_Draw == NULL) {
            void** vtable = *(void***)(*textLayout);
            orgIDWriteTextLayout_Draw = (pIDWriteTextLayout_Draw)vtable[18]; // Draw 索引通常为 18
            DetourTransactionBegin(); DetourUpdateThread(GetCurrentThread());
            DetourAttach(&(PVOID&)orgIDWriteTextLayout_Draw, newIDWriteTextLayout_Draw);
            DetourTransactionCommit();
        }
    }
    return hr;
}

// DWrite 工厂创建时的拦截入口
HRESULT WINAPI newDWriteCreateFactory(DWRITE_FACTORY_TYPE factoryType, REFIID iid, IUnknown** factory) {
    HRESULT hr = orgDWriteCreateFactory(factoryType, iid, factory);
    if (SUCCEEDED(hr) && factory && *factory) {
        void** vtable = *(void***)(*factory);
        
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        // Hook CreateTextLayout (Index 12)
        if (orgCreateTextLayout == NULL) {
            orgCreateTextLayout = (pCreateTextLayout)vtable[12];
            DetourAttach(&(PVOID&)orgCreateTextLayout, newCreateTextLayout);
        }

        // Hook CreateTextFormat (Index 15) - 替换字体的关键点
        if (orgCreateTextFormat == NULL) {
            orgCreateTextFormat = (pCreateTextFormat)vtable[15];
            DetourAttach(&(PVOID&)orgCreateTextFormat, newCreateTextFormat);
        }

        DetourTransactionCommit();
        Utils::Log("[DWrite] Factory vtable hooks installed (CreateTextFormat & CreateTextLayout).");
    }
    return hr;
}

// ===================== Library Load Hooks =====================
HMODULE WINAPI newLoadLibraryW(LPCWSTR name) {
    HMODULE h = orgLoadLibraryW(name);
    if (!h || !name) return h;
    
    if (wcsstr(name, L"gdiplus") || wcsstr(name, L"GDIPLUS")) {
        InstallGdiPlusHooks();
    }
    else if (wcsstr(name, L"dwrite") || wcsstr(name, L"DWRITE")) {
        if (!orgDWriteCreateFactory) {
            orgDWriteCreateFactory = (pDWriteCreateFactory)GetProcAddress(h, "DWriteCreateFactory");
            if (orgDWriteCreateFactory) {
                DetourTransactionBegin(); DetourUpdateThread(GetCurrentThread());
                DetourAttach(&(PVOID&)orgDWriteCreateFactory, newDWriteCreateFactory);
                if (DetourTransactionCommit() == NO_ERROR) {
                    Utils::Log("[DWrite] Late-hooked DWriteCreateFactory via LoadLibraryW.");
                }
            }
        }
    }
    return h;
}

HMODULE WINAPI newLoadLibraryExW(LPCWSTR name, HANDLE f, DWORD fl) {
    HMODULE h = orgLoadLibraryExW(name, f, fl);
    if (!h || !name || (fl & LOAD_LIBRARY_AS_DATAFILE) || (fl & LOAD_LIBRARY_AS_IMAGE_RESOURCE)) return h;

    if (wcsstr(name, L"gdiplus") || wcsstr(name, L"GDIPLUS")) {
        InstallGdiPlusHooks();
    }
    else if (wcsstr(name, L"dwrite") || wcsstr(name, L"DWRITE")) {
        if (!orgDWriteCreateFactory) {
            orgDWriteCreateFactory = (pDWriteCreateFactory)GetProcAddress(h, "DWriteCreateFactory");
            if (orgDWriteCreateFactory) {
                DetourTransactionBegin(); DetourUpdateThread(GetCurrentThread());
                DetourAttach(&(PVOID&)orgDWriteCreateFactory, newDWriteCreateFactory);
                if (DetourTransactionCommit() == NO_ERROR) {
                    Utils::Log("[DWrite] Late-hooked DWriteCreateFactory via LoadLibraryExW.");
                }
            }
        }
    }
    return h;
}

// ===================== GDI+ Hook Installation =====================
void InstallGdiPlusHooks() {
    if (g_GdiPlusHooksInstalled) return;

    HMODULE hGdiPlus = GetModuleHandleW(L"gdiplus.dll");
    if (!hGdiPlus) return;

    orgGdipCreateFontFamilyFromName = (pGdipCreateFontFamilyFromName)GetProcAddress(hGdiPlus, "GdipCreateFontFamilyFromName");
    orgGdipCreateFontFromLogfontW = (pGdipCreateFontFromLogfontW)GetProcAddress(hGdiPlus, "GdipCreateFontFromLogfontW");
    orgGdipCreateFontFromLogfontA = (pGdipCreateFontFromLogfontA)GetProcAddress(hGdiPlus, "GdipCreateFontFromLogfontA");
    orgGdipCreateFontFromHFONT = (pGdipCreateFontFromHFONT)GetProcAddress(hGdiPlus, "GdipCreateFontFromHFONT");
    orgGdipCreateFontFromDC = (pGdipCreateFontFromDC)GetProcAddress(hGdiPlus, "GdipCreateFontFromDC");
    orgGdipCreateFont = (pGdipCreateFont)GetProcAddress(hGdiPlus, "GdipCreateFont");
    orgGdipDrawString = (pGdipDrawString)GetProcAddress(hGdiPlus, "GdipDrawString");
    orgGdipDrawDriverString = (pGdipDrawDriverString)GetProcAddress(hGdiPlus, "GdipDrawDriverString");
    orgGdipMeasureString = (pGdipMeasureString)GetProcAddress(hGdiPlus, "GdipMeasureString");
    orgGdipMeasureCharacterRanges = (pGdipMeasureCharacterRanges)GetProcAddress(hGdiPlus, "GdipMeasureCharacterRanges");
    orgGdipMeasureDriverString = (pGdipMeasureDriverString)GetProcAddress(hGdiPlus, "GdipMeasureDriverString");

    LoadGdiPlusPrivateFont();

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    if (orgGdipCreateFontFamilyFromName) DetourAttach(&(PVOID&)orgGdipCreateFontFamilyFromName, newGdipCreateFontFamilyFromName);
    if (orgGdipCreateFontFromLogfontW) DetourAttach(&(PVOID&)orgGdipCreateFontFromLogfontW, newGdipCreateFontFromLogfontW);
    if (orgGdipCreateFontFromLogfontA) DetourAttach(&(PVOID&)orgGdipCreateFontFromLogfontA, newGdipCreateFontFromLogfontA);
    if (orgGdipCreateFontFromHFONT) DetourAttach(&(PVOID&)orgGdipCreateFontFromHFONT, newGdipCreateFontFromHFONT);
    if (orgGdipCreateFontFromDC) DetourAttach(&(PVOID&)orgGdipCreateFontFromDC, newGdipCreateFontFromDC);
    if (orgGdipCreateFont) DetourAttach(&(PVOID&)orgGdipCreateFont, newGdipCreateFont);
    if (orgGdipDrawString) DetourAttach(&(PVOID&)orgGdipDrawString, newGdipDrawString);
    if (orgGdipDrawDriverString) DetourAttach(&(PVOID&)orgGdipDrawDriverString, newGdipDrawDriverString);
    if (orgGdipMeasureString) DetourAttach(&(PVOID&)orgGdipMeasureString, newGdipMeasureString);
    if (orgGdipMeasureCharacterRanges) DetourAttach(&(PVOID&)orgGdipMeasureCharacterRanges, newGdipMeasureCharacterRanges);
    if (orgGdipMeasureDriverString) DetourAttach(&(PVOID&)orgGdipMeasureDriverString, newGdipMeasureDriverString);
    DetourTransactionCommit();

    g_GdiPlusHooksInstalled = true;
}

// ===================== Main Hook Installation =====================
void InstallHooks(HMODULE hModule) {
    g_hModule = hModule;
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    // GDI Font Creation
    DetourAttach(&(PVOID&)orgCreateFontA, newCreateFontA);
    DetourAttach(&(PVOID&)orgCreateFontIndirectA, newCreateFontIndirectA);
    DetourAttach(&(PVOID&)orgCreateFontW, newCreateFontW);
    DetourAttach(&(PVOID&)orgCreateFontIndirectW, newCreateFontIndirectW);

    // Library Loading
    DetourAttach(&(PVOID&)orgLoadLibraryW, newLoadLibraryW);
    DetourAttach(&(PVOID&)orgLoadLibraryExW, newLoadLibraryExW);

    // GDI Text Output
    DetourAttach(&(PVOID&)orgTextOutA, newTextOutA);
    DetourAttach(&(PVOID&)orgTextOutW, newTextOutW);
    DetourAttach(&(PVOID&)orgExtTextOutA, newExtTextOutA);
    DetourAttach(&(PVOID&)orgExtTextOutW, newExtTextOutW);
    DetourAttach(&(PVOID&)orgDrawTextA, newDrawTextA);
    DetourAttach(&(PVOID&)orgDrawTextW, newDrawTextW);
    DetourAttach(&(PVOID&)orgDrawTextExA, newDrawTextExA);
    DetourAttach(&(PVOID&)orgDrawTextExW, newDrawTextExW);
    DetourAttach(&(PVOID&)orgPolyTextOutA, newPolyTextOutA);
    DetourAttach(&(PVOID&)orgPolyTextOutW, newPolyTextOutW);
    DetourAttach(&(PVOID&)orgTabbedTextOutA, newTabbedTextOutA);
    DetourAttach(&(PVOID&)orgTabbedTextOutW, newTabbedTextOutW);

    // SelectObject — intercepts cached font selection into any DC
    DetourAttach(&(PVOID&)orgSelectObject, newSelectObject);

    // Glyph extraction — critical for engines that render glyphs to textures
    DetourAttach(&(PVOID&)orgGetGlyphOutlineA, newGetGlyphOutlineA);
    DetourAttach(&(PVOID&)orgGetGlyphOutlineW, newGetGlyphOutlineW);

    // Character metrics — ensures text layout uses the replacement font's metrics
    DetourAttach(&(PVOID&)orgGetCharABCWidthsA, newGetCharABCWidthsA);
    DetourAttach(&(PVOID&)orgGetCharABCWidthsW, newGetCharABCWidthsW);
    DetourAttach(&(PVOID&)orgGetCharABCWidthsFloatA, newGetCharABCWidthsFloatA);
    DetourAttach(&(PVOID&)orgGetCharABCWidthsFloatW, newGetCharABCWidthsFloatW);
    DetourAttach(&(PVOID&)orgGetCharWidthA, newGetCharWidthA);
    DetourAttach(&(PVOID&)orgGetCharWidthW, newGetCharWidthW);
    DetourAttach(&(PVOID&)orgGetCharWidth32A, newGetCharWidth32A);
    DetourAttach(&(PVOID&)orgGetCharWidth32W, newGetCharWidth32W);
    DetourAttach(&(PVOID&)orgGetTextExtentPoint32A, newGetTextExtentPoint32A);
    DetourAttach(&(PVOID&)orgGetTextExtentPoint32W, newGetTextExtentPoint32W);
    DetourAttach(&(PVOID&)orgGetTextExtentExPointA, newGetTextExtentExPointA);
    DetourAttach(&(PVOID&)orgGetTextExtentExPointW, newGetTextExtentExPointW);

    // GetObject — intercepts font info queries so engines see our replacement font
    DetourAttach(&(PVOID&)orgGetObjectA, newGetObjectA);
    DetourAttach(&(PVOID&)orgGetObjectW, newGetObjectW);

    // GetTextMetrics — ensures charset/metrics queries return replacement font data
    DetourAttach(&(PVOID&)orgGetTextMetricsA, newGetTextMetricsA);
    DetourAttach(&(PVOID&)orgGetTextMetricsW, newGetTextMetricsW);

    // GetTextFace — return our font name when engine queries current font
    DetourAttach(&(PVOID&)orgGetTextFaceA, newGetTextFaceA);
    DetourAttach(&(PVOID&)orgGetTextFaceW, newGetTextFaceW);

    // EnumFontFamiliesEx — inject our font into font enumeration
    DetourAttach(&(PVOID&)orgEnumFontFamiliesExA, newEnumFontFamiliesExA);
    DetourAttach(&(PVOID&)orgEnumFontFamiliesExW, newEnumFontFamiliesExW);

    // CreateFontIndirectEx
    DetourAttach(&(PVOID&)orgCreateFontIndirectExA, newCreateFontIndirectExA);
    DetourAttach(&(PVOID&)orgCreateFontIndirectExW, newCreateFontIndirectExW);

    // Font resource loading/unloading
    DetourAttach(&(PVOID&)orgAddFontResourceA, newAddFontResourceA);
    DetourAttach(&(PVOID&)orgAddFontResourceW, newAddFontResourceW);
    DetourAttach(&(PVOID&)orgAddFontResourceExA, newAddFontResourceExA);
    DetourAttach(&(PVOID&)orgAddFontResourceExW, newAddFontResourceExW);
    DetourAttach(&(PVOID&)orgAddFontMemResourceEx, newAddFontMemResourceEx);
    DetourAttach(&(PVOID&)orgRemoveFontResourceA, newRemoveFontResourceA);
    DetourAttach(&(PVOID&)orgRemoveFontResourceW, newRemoveFontResourceW);
    DetourAttach(&(PVOID&)orgRemoveFontResourceExA, newRemoveFontResourceExA);
    DetourAttach(&(PVOID&)orgRemoveFontResourceExW, newRemoveFontResourceExW);
    DetourAttach(&(PVOID&)orgRemoveFontMemResourceEx, newRemoveFontMemResourceEx);

    // Old-style font enumeration
    DetourAttach(&(PVOID&)orgEnumFontsA, newEnumFontsA);
    DetourAttach(&(PVOID&)orgEnumFontsW, newEnumFontsW);
    DetourAttach(&(PVOID&)orgEnumFontFamiliesA, newEnumFontFamiliesA);
    DetourAttach(&(PVOID&)orgEnumFontFamiliesW, newEnumFontFamiliesW);

    // Glyph/width supplement
    DetourAttach(&(PVOID&)orgGetCharWidthFloatA, newGetCharWidthFloatA);
    DetourAttach(&(PVOID&)orgGetCharWidthFloatW, newGetCharWidthFloatW);
    DetourAttach(&(PVOID&)orgGetCharWidthI, newGetCharWidthI);
    DetourAttach(&(PVOID&)orgGetCharABCWidthsI, newGetCharABCWidthsI);
    DetourAttach(&(PVOID&)orgGetCharacterPlacementA, newGetCharacterPlacementA);
    DetourAttach(&(PVOID&)orgGetCharacterPlacementW, newGetCharacterPlacementW);
    DetourAttach(&(PVOID&)orgGetKerningPairsA, newGetKerningPairsA);
    DetourAttach(&(PVOID&)orgGetKerningPairsW, newGetKerningPairsW);
    DetourAttach(&(PVOID&)orgGetGlyphIndicesA, newGetGlyphIndicesA);
    DetourAttach(&(PVOID&)orgGetGlyphIndicesW, newGetGlyphIndicesW);

    // Font info supplement
    DetourAttach(&(PVOID&)orgGetOutlineTextMetricsA, newGetOutlineTextMetricsA);
    DetourAttach(&(PVOID&)orgGetOutlineTextMetricsW, newGetOutlineTextMetricsW);
    DetourAttach(&(PVOID&)orgGetTextExtentPointA, newGetTextExtentPointA);
    DetourAttach(&(PVOID&)orgGetTextExtentPointW, newGetTextExtentPointW);
    DetourAttach(&(PVOID&)orgGetTextExtentPointI, newGetTextExtentPointI);
    DetourAttach(&(PVOID&)orgGetTextExtentExPointI, newGetTextExtentExPointI);
    DetourAttach(&(PVOID&)orgGetFontData, newGetFontData);
    DetourAttach(&(PVOID&)orgGetFontLanguageInfo, newGetFontLanguageInfo);
    DetourAttach(&(PVOID&)orgGetFontUnicodeRanges, newGetFontUnicodeRanges);

    // User32 text supplement
    DetourAttach(&(PVOID&)orgGrayStringA, newGrayStringA);
    DetourAttach(&(PVOID&)orgGrayStringW, newGrayStringW);
    DetourAttach(&(PVOID&)orgGetTabbedTextExtentA, newGetTabbedTextExtentA);
    DetourAttach(&(PVOID&)orgGetTabbedTextExtentW, newGetTabbedTextExtentW);

    // DirectWrite
    HMODULE hDWrite = GetModuleHandleW(L"dwrite.dll");
    if (hDWrite) {
        orgDWriteCreateFactory = (pDWriteCreateFactory)GetProcAddress(hDWrite, "DWriteCreateFactory");
        if (orgDWriteCreateFactory) DetourAttach(&(PVOID&)orgDWriteCreateFactory, newDWriteCreateFactory);
    }

    DetourTransactionCommit();

    // GDI+
    InstallGdiPlusHooks();

    // Launch font picker UI on a separate thread
    _beginthreadex(NULL, 0, FontPickerThread, hModule, 0, NULL);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        InstallHooks(hModule);
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
