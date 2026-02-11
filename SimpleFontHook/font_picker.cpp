#define NOMINMAX
#include "font_picker.h"
#include "font_patcher.h"
#include "framework.h"
#include <algorithm>
#include <commctrl.h>
#include <windowsx.h>
#include <vector>
#include <string>
#include <cmath>
#include <set>
#include <dwmapi.h>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "dwmapi.lib")

using std::min;
using std::max;

namespace FontPicker {

// ═══════════════════════════════════════════════════════════════
//  State
// ═══════════════════════════════════════════════════════════════
static HWND g_hWnd = NULL;
static HWND g_hSearchEdit = NULL;
static HMODULE g_hModule = NULL;
static bool g_visible = false;
static std::vector<std::wstring> g_allFonts;
static std::vector<std::set<BYTE>> g_fontCharsets;  // charsets each font supports
static std::vector<std::wstring> g_recentFonts;
static std::vector<int> g_filteredIndices;
static int g_selectedIndex = 0;
static int g_scrollOffset = 0;
static int g_hoveredIndex = -1;
static std::wstring g_appliedFont;   // currently applied font name
static std::vector<HANDLE> g_loadedLocalFonts;  // handles of locally loaded memory fonts
static std::vector<std::wstring> g_loadedLocalFontPaths;  // paths of fonts loaded via AddFontResourceExW

// Charset selector state
struct CharsetEntry {
    const wchar_t* label;
    BYTE value;
};
static const CharsetEntry g_charsets[] = {
    { L"默认",       DEFAULT_CHARSET },    // 默认
    { L"中文",       GB2312_CHARSET },     // 中文
    { L"日文",       SHIFTJIS_CHARSET },   // 日文
    { L"繁体",       CHINESEBIG5_CHARSET },// 繁体
    { L"韩文",       HANGUL_CHARSET },     // 韩文
};
static const int g_charsetCount = _countof(g_charsets);
static int g_selectedCharset = 0;  // index into g_charsets, 0 = All
static int g_hoveredCharset = -1;
static int g_charsetScrollX = 0;   // horizontal scroll offset in pixels

// Spoof bar state
static bool g_spoofEnabled = false;
static int g_spoofFromIdx = 2;   // index into g_charsets, default 日文
static int g_spoofToIdx = 1;     // index into g_charsets, default 中文
static bool g_spoofToggleHovered = false;
static bool g_spoofFromHovered = false;
static bool g_spoofToHovered = false;

static void EnumerateFonts();


// ═══════════════════════════════════════════════════════════════
//  Layout constants
// ═══════════════════════════════════════════════════════════════
static int       ITEM_HEIGHT      = 56;
static int       WINDOW_WIDTH     = 480;
static int       WINDOW_HEIGHT    = 720;
static const int TITLE_HEIGHT     = 48;
static const int SEARCH_HEIGHT    = 50;
static const int PREVIEW_HEIGHT   = 155;
static const int CHARSET_HEIGHT   = 36;
static const int SPOOF_HEIGHT     = 40;  // codepage spoof bar
static const int SCROLLBAR_WIDTH  = 8;
static const int SCROLLBAR_MARGIN = 4;
static int LIST_HEIGHT;
static int VISIBLE_ITEMS;

// Resize support
static const int MIN_WIDTH        = 360;
static const int MIN_HEIGHT       = 480;
static const int RESIZE_BORDER    = 6;   // pixels from edge for resize hit-test
static const wchar_t* WND_CLASS = L"FontPickerOverlay";

// ═══════════════════════════════════════════════════════════════
//  Color Palette — Sakura Night (二次元 Galgame aesthetic)
//  Inspired by visual novel UI: deep indigo sky + cherry blossom
// ═══════════════════════════════════════════════════════════════
//  Background layers
static const COLORREF COL_BG_TOP       = RGB(28, 22, 48);
static const COLORREF COL_BG_BOT       = RGB(12, 10, 24);
static const COLORREF COL_TITLE_TOP    = RGB(60, 36, 88);
static const COLORREF COL_TITLE_BOT    = RGB(40, 24, 60);
static const COLORREF COL_SEARCH_BG    = RGB(32, 26, 52);
static const COLORREF COL_PREVIEW_TOP  = RGB(34, 24, 56);
static const COLORREF COL_PREVIEW_BOT  = RGB(18, 12, 32);

//  Interactive
static const COLORREF COL_SELECTED_BG  = RGB(100, 35, 75);
static const COLORREF COL_SELECTED_GLO = RGB(255, 150, 200);
static const COLORREF COL_HOVER_BG     = RGB(52, 40, 78);
static const COLORREF COL_ITEM_BORDER  = RGB(75, 50, 100);

//  Text
static const COLORREF COL_TEXT         = RGB(250, 245, 255);
static const COLORREF COL_TEXT_DIM     = RGB(175, 160, 210);
static const COLORREF COL_TEXT_HINT    = RGB(120, 100, 160);
static const COLORREF COL_ACCENT       = RGB(255, 170, 210);     // bright sakura
static const COLORREF COL_ACCENT2      = RGB(160, 180, 255);     // sky blue
static const COLORREF COL_ACCENT3      = RGB(210, 160, 255);     // soft violet

//  UI chrome
static const COLORREF COL_BORDER       = RGB(85, 60, 120);
static const COLORREF COL_CLOSE_HOV    = RGB(240, 70, 100);
static const COLORREF COL_BTN_HOV      = RGB(75, 55, 110);

//  Scrollbar
static const COLORREF COL_SB_TRACK     = RGB(32, 26, 50);
static const COLORREF COL_SB_THUMB     = RGB(120, 80, 150);
static const COLORREF COL_SB_THUMB_HOV = RGB(180, 120, 200);
static const COLORREF COL_SB_THUMB_DRG = RGB(255, 150, 190);

//  Badges & decorations
static const COLORREF COL_BADGE_BG     = RGB(70, 35, 62);
static const COLORREF COL_BADGE_TEXT   = RGB(255, 185, 215);
static const COLORREF COL_APPLIED_BG   = RGB(30, 65, 50);
static const COLORREF COL_APPLIED_TEXT = RGB(140, 255, 200);
static const COLORREF COL_SEPARATOR    = RGB(45, 35, 65);

// ═══════════════════════════════════════════════════════════════
//  Internal state
// ═══════════════════════════════════════════════════════════════
static WNDPROC g_OldEditProc = NULL;
static bool g_closeHovered = false;
static bool g_minHovered = false;
static bool g_zoomInHovered = false;
static bool g_zoomOutHovered = false;

// UI font scale
static float g_uiScale = 1.0f;
static const float UI_SCALE_MIN = 0.7f;
static const float UI_SCALE_MAX = 1.8f;
static const float UI_SCALE_STEP = 0.1f;
static int SF(int base) { return (int)(base * g_uiScale + 0.5f); }
static bool g_scrollbarHovered = false;
static bool g_scrollbarDragging = false;
static int g_scrollDragStartY = 0;
static int g_scrollDragStartOffset = 0;
static HFONT g_editFont = NULL;

// ═══════════════════════════════════════════════════════════════
//  Helpers
// ═══════════════════════════════════════════════════════════════
static BYTE GetGameCharset() {
    if (Config::DetectedCharset != DEFAULT_CHARSET)
        return (BYTE)Config::DetectedCharset;
    return DEFAULT_CHARSET;
}

static int GetListTop() { return TITLE_HEIGHT + SEARCH_HEIGHT + CHARSET_HEIGHT + SPOOF_HEIGHT; }

static void RecalcLayout() {
    ITEM_HEIGHT = SF(56);
    LIST_HEIGHT = WINDOW_HEIGHT - TITLE_HEIGHT - SEARCH_HEIGHT - CHARSET_HEIGHT - SPOOF_HEIGHT - PREVIEW_HEIGHT;
    if (LIST_HEIGHT < 0) LIST_HEIGHT = 0;
    VISIBLE_ITEMS = LIST_HEIGHT / ITEM_HEIGHT;
    if (VISIBLE_ITEMS < 1) VISIBLE_ITEMS = 1;
    // Reposition search edit and update its font
    if (g_hSearchEdit) {
        int editH = SF(20);
        int editY = TITLE_HEIGHT + (SEARCH_HEIGHT - editH) / 2;
        MoveWindow(g_hSearchEdit, 22, editY, WINDOW_WIDTH - 62, editH, TRUE);
        if (g_editFont) DeleteObject(g_editFont);
        g_editFont = CreateFontW(-SF(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        SendMessage(g_hSearchEdit, WM_SETFONT, (WPARAM)g_editFont, TRUE);
    }
}

static void FillRoundRect(HDC hdc, const RECT& rc, int r, COLORREF col) {
    HBRUSH br = CreateSolidBrush(col);
    HPEN pen = CreatePen(PS_SOLID, 1, col);
    HGDIOBJ oldBr = SelectObject(hdc, br);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, r, r);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(br);
    DeleteObject(pen);
}

static void DrawRoundRectOutline(HDC hdc, const RECT& rc, int r, COLORREF col) {
    HPEN pen = CreatePen(PS_SOLID, 1, col);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBr = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, r, r);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

static void GradientV(HDC hdc, const RECT& rc, COLORREF top, COLORREF bottom) {
    TRIVERTEX v[2];
    v[0].x = rc.left;  v[0].y = rc.top;
    v[0].Red   = (COLOR16)(GetRValue(top) << 8);
    v[0].Green = (COLOR16)(GetGValue(top) << 8);
    v[0].Blue  = (COLOR16)(GetBValue(top) << 8);
    v[0].Alpha = 0;
    v[1].x = rc.right; v[1].y = rc.bottom;
    v[1].Red   = (COLOR16)(GetRValue(bottom) << 8);
    v[1].Green = (COLOR16)(GetGValue(bottom) << 8);
    v[1].Blue  = (COLOR16)(GetBValue(bottom) << 8);
    v[1].Alpha = 0;
    GRADIENT_RECT gr = { 0, 1 };
    GradientFill(hdc, v, 2, &gr, 1, GRADIENT_FILL_RECT_V);
}

static bool PointInRect(int x, int y, const RECT& rc) {
    return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom;
}

// Mix two colors: ratio 0.0 = a, 1.0 = b
static COLORREF MixColor(COLORREF a, COLORREF b, float ratio) {
    int r = (int)(GetRValue(a) * (1.f - ratio) + GetRValue(b) * ratio);
    int g = (int)(GetGValue(a) * (1.f - ratio) + GetGValue(b) * ratio);
    int bl = (int)(GetBValue(a) * (1.f - ratio) + GetBValue(b) * ratio);
    return RGB(min(255, max(0, r)), min(255, max(0, g)), min(255, max(0, bl)));
}

// ═══════════════════════════════════════════════════════════════
//  Scrollbar geometry
// ═══════════════════════════════════════════════════════════════
static void GetScrollbarRect(RECT* thumbRc, RECT* trackRc) {
    int itemCount = (int)g_filteredIndices.size();
    int listTop = GetListTop();
    int listBot = WINDOW_HEIGHT - PREVIEW_HEIGHT;
    int trackH = listBot - listTop;

    int sbLeft = WINDOW_WIDTH - SCROLLBAR_WIDTH - SCROLLBAR_MARGIN;
    int sbRight = WINDOW_WIDTH - SCROLLBAR_MARGIN;

    if (trackRc) {
        trackRc->left = sbLeft;  trackRc->right = sbRight;
        trackRc->top = listTop + 4; trackRc->bottom = listBot - 4;
    }
    if (thumbRc) {
        if (itemCount <= VISIBLE_ITEMS) {
            *thumbRc = { 0, 0, 0, 0 };
            return;
        }
        int effTrackH = (listBot - 4) - (listTop + 4);
        int calcH = effTrackH * VISIBLE_ITEMS / itemCount;
        int thumbH = max(28, calcH);
        int maxScroll = max(1, itemCount - VISIBLE_ITEMS);
        int scrollRange = effTrackH - thumbH;
        int thumbY = (listTop + 4) + scrollRange * g_scrollOffset / maxScroll;
        thumbRc->left = sbLeft;  thumbRc->right = sbRight;
        thumbRc->top = thumbY;   thumbRc->bottom = thumbY + thumbH;
    }
}

// ═══════════════════════════════════════════════════════════════
//  Charset bar geometry helper
// ═══════════════════════════════════════════════════════════════
// Returns the charset pill index at (x,y), or -1 if none.
// Also computes total content width for scroll clamping.
static int CharsetHitTest(int x, int y, int* outTotalWidth = nullptr) {
    int csTop = TITLE_HEIGHT + SEARCH_HEIGHT;
    int pillY = csTop + 6;
    int pillH = CHARSET_HEIGHT - 12;
    bool yInRange = (y >= pillY && y < pillY + pillH);

    // We need an HDC to measure text
    HDC hdc = GetDC(NULL);
    HFONT csFont = CreateFontW(-SF(11), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HGDIOBJ old = SelectObject(hdc, csFont);

    int pillX = 8 - g_charsetScrollX;
    int result = -1;
    for (int i = 0; i < g_charsetCount; i++) {
        SIZE sz;
        GetTextExtentPoint32W(hdc, g_charsets[i].label, (int)wcslen(g_charsets[i].label), &sz);
        int pillW = sz.cx + SF(16);
        if (yInRange && x >= pillX && x < pillX + pillW) result = i;
        pillX += pillW + 5;
    }

    if (outTotalWidth) *outTotalWidth = pillX + g_charsetScrollX;

    SelectObject(hdc, old);
    DeleteObject(csFont);
    ReleaseDC(NULL, hdc);
    return result;
}

// ═══════════════════════════════════════════════════════════════
//  Spoof bar hit test: returns 0=toggle, 1=from, 2=to, -1=none
// ═══════════════════════════════════════════════════════════════
static int SpoofBarHitTest(int x, int y, RECT* outToggle = nullptr, RECT* outFrom = nullptr, RECT* outTo = nullptr) {
    int spTop = TITLE_HEIGHT + SEARCH_HEIGHT + CHARSET_HEIGHT;
    int py = spTop + (SPOOF_HEIGHT - 24) / 2;
    if (y < py || y >= py + 24) return -1;

    HDC hdc = GetDC(NULL);
    HFONT spFontBold = CreateFontW(-SF(11), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT spFont = CreateFontW(-SF(11), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

    int px = 10;
    int result = -1;

    // Toggle
    {
        HGDIOBJ old = SelectObject(hdc, spFontBold);
        const wchar_t* label = g_spoofEnabled ? L"伪装: 开" : L"伪装: 关";  // 伪装: 开/关
        SIZE sz; GetTextExtentPoint32W(hdc, label, (int)wcslen(label), &sz);
        int bw = sz.cx + 16;
        RECT r = { px, py, px + bw, py + 24 };
        if (outToggle) *outToggle = r;
        if (x >= r.left && x < r.right) result = 0;
        px += bw + 8;
        SelectObject(hdc, old);
    }

    // From
    {
        HGDIOBJ old = SelectObject(hdc, spFont);
        const wchar_t* fromLabel = g_charsets[g_spoofFromIdx].label;
        SIZE sz; GetTextExtentPoint32W(hdc, fromLabel, (int)wcslen(fromLabel), &sz);
        int bw = sz.cx + 16;
        RECT r = { px, py, px + bw, py + 24 };
        if (outFrom) *outFrom = r;
        if (result < 0 && x >= r.left && x < r.right) result = 1;
        px += bw + 4 + 24 + 4;  // skip arrow
        SelectObject(hdc, old);
    }

    // To
    {
        HGDIOBJ old = SelectObject(hdc, spFont);
        const wchar_t* toLabel = g_charsets[g_spoofToIdx].label;
        SIZE sz; GetTextExtentPoint32W(hdc, toLabel, (int)wcslen(toLabel), &sz);
        int bw = sz.cx + 16;
        RECT r = { px, py, px + bw, py + 24 };
        if (outTo) *outTo = r;
        if (result < 0 && x >= r.left && x < r.right) result = 2;
        SelectObject(hdc, old);
    }

    DeleteObject(spFontBold);
    DeleteObject(spFont);
    ReleaseDC(NULL, hdc);
    return result;
}

// ═══════════════════════════════════════════════════════════════
//  Aggressive force-redraw for game windows
// ═══════════════════════════════════════════════════════════════
// ═══════════════════════════════════════════════════════════════
//  Aggressive force-redraw for game windows
// ═══════════════════════════════════════════════════════════════
static void ForceGameRedraw() {
    InterlockedIncrement(&Config::ConfigVersion);
    DWORD pid = GetCurrentProcessId();

    // Step 1: Broadcast WM_FONTCHANGE
    PostMessage(HWND_BROADCAST, WM_FONTCHANGE, 0, 0);
    
    EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
        DWORD wp = 0;
        GetWindowThreadProcessId(hwnd, &wp);
        if (wp == (DWORD)lp && hwnd != g_hWnd) {
            PostMessage(hwnd, WM_FONTCHANGE, 0, 0);
            PostMessage(hwnd, WM_SETTINGCHANGE, 0, (LPARAM)L"Font");
            
            // Just invalidate, don't resize (flashing is annoying)
            RedrawWindow(hwnd, NULL, NULL,
                RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME | RDW_UPDATENOW);
        }
        return TRUE;
    }, (LPARAM)pid);
}

// Apply spoof config to Config namespace and force redraw
static void ApplySpoofConfig() {
    Config::EnableCodepageSpoof = g_spoofEnabled;
    Config::SpoofFromCharset = g_charsets[g_spoofFromIdx].value;
    Config::SpoofToCharset = g_charsets[g_spoofToIdx].value;

    Utils::Log("[Config] Spoof: %s, %u -> %u (Reloading...)", 
        g_spoofEnabled ? "ON" : "OFF", 
        (DWORD)Config::SpoofFromCharset, 
        (DWORD)Config::SpoofToCharset);

    // Reload fonts to apply/remove OS/2 patches
    EnumerateFonts();
    
    // Just force redraw to let TextOut hook pick up changes
    ForceGameRedraw();
}

// ═══════════════════════════════════════════════════════════════
//  Decorative drawing: sakura petal (simple diamond/rhombus)
// ═══════════════════════════════════════════════════════════════
static void DrawPetal(HDC hdc, int cx, int cy, int w, int h, COLORREF col) {
    POINT pts[4] = { {cx, cy - h}, {cx + w, cy}, {cx, cy + h}, {cx - w, cy} };
    HBRUSH br = CreateSolidBrush(col);
    HPEN pen = CreatePen(PS_SOLID, 1, col);
    HGDIOBJ oldBr = SelectObject(hdc, br);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    Polygon(hdc, pts, 4);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(br);
    DeleteObject(pen);
}

static void DrawDiagonalPattern(HDC hdc, const RECT& rc, COLORREF col) {
    HPEN pen = CreatePen(PS_SOLID, 1, col);
    HGDIOBJ old = SelectObject(hdc, pen);
    int step = SF(12);
    for (int x = rc.left - rc.bottom; x < rc.right; x += step) {
        MoveToEx(hdc, x, rc.top, NULL);
        LineTo(hdc, x + (rc.bottom - rc.top), rc.bottom);
    }
    SelectObject(hdc, old);
    DeleteObject(pen);
}

// Sparkle: 4-pointed star
static void DrawSparkle(HDC hdc, int cx, int cy, int sz, COLORREF col) {
    HPEN pen = CreatePen(PS_SOLID, 1, col);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    MoveToEx(hdc, cx - sz, cy, NULL); LineTo(hdc, cx + sz + 1, cy);
    MoveToEx(hdc, cx, cy - sz, NULL); LineTo(hdc, cx, cy + sz + 1);
    int d = sz * 6 / 10;
    MoveToEx(hdc, cx - d, cy - d, NULL); LineTo(hdc, cx + d + 1, cy + d + 1);
    MoveToEx(hdc, cx + d, cy - d, NULL); LineTo(hdc, cx - d - 1, cy + d + 1);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

// ═══════════════════════════════════════════════════════════════
//  Font enumeration
// ═══════════════════════════════════════════════════════════════
static int CALLBACK EnumFontProc(const LOGFONTW* lf, const TEXTMETRICW*, DWORD fontType, LPARAM lParam) {
    std::wstring name(lf->lfFaceName);
    if (name.empty() || name[0] == L'@') return 1;
    if (!(fontType & TRUETYPE_FONTTYPE)) return 1;
    BYTE cs = lf->lfCharSet;
    for (int i = 0; i < (int)g_allFonts.size(); i++) {
        if (g_allFonts[i] == name) {
            g_fontCharsets[i].insert(cs);
            return 1;
        }
    }
    g_allFonts.push_back(name);
    g_fontCharsets.push_back({ cs });
    return 1;
}

static void UpdateFilter() {
    wchar_t buf[256];
    GetWindowTextW(g_hSearchEdit, buf, 256);
    std::wstring search(buf);
    std::transform(search.begin(), search.end(), search.begin(), ::towlower);
    g_filteredIndices.clear();

    BYTE filterCs = g_charsets[g_selectedCharset].value;

    // Helper: does font at index i pass the charset filter?
    auto passCharset = [&](int i) -> bool {
        if (filterCs == DEFAULT_CHARSET) return true;  // "All"
        return g_fontCharsets[i].count(filterCs) > 0;
    };

    // Recents first when no search
    if (search.empty()) {
        for (const auto& recent : g_recentFonts) {
            for (int i = 0; i < (int)g_allFonts.size(); i++) {
                if (g_allFonts[i] == recent && passCharset(i)) {
                    g_filteredIndices.push_back(i); break;
                }
            }
        }
    }

    for (int i = 0; i < (int)g_allFonts.size(); i++) {
        if (!passCharset(i)) continue;
        if (search.empty()) {
            bool dup = false;
            for (int idx : g_filteredIndices) if (idx == i) { dup = true; break; }
            if (dup) continue;
            g_filteredIndices.push_back(i);
            continue;
        }
        std::wstring name = g_allFonts[i];
        std::transform(name.begin(), name.end(), name.begin(), ::towlower);
        if (name.find(search) != std::wstring::npos)
            g_filteredIndices.push_back(i);
    }

    if (g_selectedIndex >= (int)g_filteredIndices.size())
        g_selectedIndex = (int)g_filteredIndices.size() - 1;
    if (g_selectedIndex < 0 && !g_filteredIndices.empty())
        g_selectedIndex = 0;
}


static void LoadLocalFonts() {
    // Clean up previously loaded memory fonts
    for (HANDLE h : g_loadedLocalFonts) {
        RemoveFontMemResourceEx(h);
    }
    g_loadedLocalFonts.clear();

    // Get DLL directory
    wchar_t rootDir[MAX_PATH];
    GetModuleFileNameW(g_hModule, rootDir, MAX_PATH);
    PathRemoveFileSpecW(rootDir);

    static const wchar_t* exts[] = { L"\\*.ttf", L"\\*.otf", L"\\*.ttc", L"\\*.fon" };
    for (const wchar_t* ext : exts) {
        wchar_t pattern[MAX_PATH];
        wcscpy_s(pattern, rootDir);
        wcscat_s(pattern, ext);

        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(pattern, &fd);
        if (hFind == INVALID_HANDLE_VALUE) continue;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            wchar_t fullPath[MAX_PATH];
            swprintf_s(fullPath, MAX_PATH, L"%s\\%s", rootDir, fd.cFileName);

            // AddFontResourceExW makes the font enumerable by GDI
            if (AddFontResourceExW(fullPath, FR_PRIVATE, NULL) > 0) {
                g_loadedLocalFontPaths.push_back(fullPath); 
                Utils::LogW(L"[FontPicker] Loaded via AddFontResourceExW: %s", fd.cFileName);
            }

            // Also load into memory for patching/GDI+ if needed
            HANDLE hFile = CreateFileW(fullPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                DWORD size = GetFileSize(hFile, NULL);
                if (size > 0 && size != INVALID_FILE_SIZE) {
                    std::vector<BYTE> fontData(size);
                    DWORD read;
                    if (ReadFile(hFile, fontData.data(), size, &read, NULL) && read == size) {
                        // Apply OS/2 Patch if enabled
                        if (Config::EnableCodepageSpoof) {
                             int bitToSet = -1;
                             if (Config::SpoofFromCharset == SHIFTJIS_CHARSET) bitToSet = 17;
                             else if (Config::SpoofFromCharset == GB2312_CHARSET) bitToSet = 18;
                             
                             if (bitToSet != -1) {
                                 FontPatcher::PatchOS2CodePageRange(fontData, bitToSet);
                             }
                        }

                        DWORD numFonts = 0;
                        HANDLE hFont = AddFontMemResourceEx(fontData.data(), (DWORD)fontData.size(), NULL, &numFonts);
                        if (hFont) {
                            g_loadedLocalFonts.push_back(hFont);
                        }
                    }
                }
                CloseHandle(hFile);
            }

        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
}

static void EnumerateFonts() {
    g_allFonts.clear();
    g_fontCharsets.clear();

    // Load font files from game root directory first
    LoadLocalFonts();

    HDC hdc = GetDC(NULL);
    static const BYTE charsets[] = {
        DEFAULT_CHARSET, ANSI_CHARSET, SHIFTJIS_CHARSET, HANGUL_CHARSET,
        GB2312_CHARSET, CHINESEBIG5_CHARSET, GREEK_CHARSET, TURKISH_CHARSET,
        VIETNAMESE_CHARSET, HEBREW_CHARSET, ARABIC_CHARSET, BALTIC_CHARSET,
        RUSSIAN_CHARSET, THAI_CHARSET, EASTEUROPE_CHARSET, OEM_CHARSET,
        JOHAB_CHARSET, SYMBOL_CHARSET, MAC_CHARSET,
    };
    for (BYTE cs : charsets) {
        LOGFONTW lf = {};
        lf.lfCharSet = cs;
        EnumFontFamiliesExW(hdc, &lf, (FONTENUMPROCW)EnumFontProc, 0, 0);
    }
    ReleaseDC(NULL, hdc);
    std::sort(g_allFonts.begin(), g_allFonts.end());
    UpdateFilter();
}

static bool IsRecentFont(int filteredIdx) {
    if (filteredIdx < 0 || filteredIdx >= (int)g_filteredIndices.size()) return false;
    const auto& name = g_allFonts[g_filteredIndices[filteredIdx]];
    for (const auto& r : g_recentFonts) if (r == name) return true;
    return false;
}

static bool IsAppliedFont(int filteredIdx) {
    if (filteredIdx < 0 || filteredIdx >= (int)g_filteredIndices.size()) return false;
    return g_allFonts[g_filteredIndices[filteredIdx]] == g_appliedFont;
}

// ═══════════════════════════════════════════════════════════════
//  Apply selected font
// ═══════════════════════════════════════════════════════════════
static void ApplySelectedFont() {
    if (g_selectedIndex < 0 || g_selectedIndex >= (int)g_filteredIndices.size()) return;
    const std::wstring& name = g_allFonts[g_filteredIndices[g_selectedIndex]];

    auto it = std::find(g_recentFonts.begin(), g_recentFonts.end(), name);
    if (it != g_recentFonts.end()) g_recentFonts.erase(it);
    g_recentFonts.insert(g_recentFonts.begin(), name);
    if (g_recentFonts.size() > 5) g_recentFonts.pop_back();

    g_appliedFont = name;
    wcsncpy_s(Config::ForcedFontNameW, name.c_str(), LF_FACESIZE - 1);

    // Use English face name for ANSI hooks if possible to avoid encoding issues with Chinese names
    LOGFONTW lf = { 0 };
    wcscpy_s(lf.lfFaceName, name.c_str());
    HFONT hFont = CreateFontIndirectW(&lf);
    bool nameConverted = false;
    if (hFont) {
        std::string engName = Utils::GetFontEnglishName(hFont);
        if (!engName.empty()) {
            strcpy_s(Config::ForcedFontNameA, engName.c_str());
            Utils::LogW(L"[FontPicker] Using English face name for ANSI hooks: %S (was %s)", engName.c_str(), name.c_str());
            nameConverted = true;
        }
        DeleteObject(hFont);
    }

    if (!nameConverted) {
        WideCharToMultiByte(CP_ACP, 0, name.c_str(), -1, Config::ForcedFontNameA, LF_FACESIZE - 1, NULL, NULL);
    }
    Config::ForcedFontNameA[LF_FACESIZE - 1] = '\0';
    Config::EnableFaceNameReplace = true;
    Config::EnableFontHook = true;

    // Use the charset selected in the charset bar.
    // "All" (DEFAULT_CHARSET) = don't override charset, let the game keep its original charset.
    // A specific charset forces GDI to use that codepage.
    if (g_selectedCharset == 0) {
        // "All" selected: don't force charset, preserve the game's original charset
        Config::EnableCharsetReplace = false;
    } else {
        Config::ForcedCharset = g_charsets[g_selectedCharset].value;
        Config::EnableCharsetReplace = true;
    }

    ForceGameRedraw();
}

// ═══════════════════════════════════════════════════════════════
//  Paint
// ═══════════════════════════════════════════════════════════════
static void PaintWindow(HWND hWnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    RECT rc;
    GetClientRect(hWnd, &rc);

    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HBITMAP oldBmp = (HBITMAP)SelectObject(mem, bmp);

    SetBkMode(mem, TRANSPARENT);

    // ── Background gradient ───────────────────────────────────
    RECT bgRc = { 0, 0, rc.right, rc.bottom };
    GradientV(mem, bgRc, COL_BG_TOP, COL_BG_BOT);
    
    // Subtle background pattern
    DrawDiagonalPattern(mem, bgRc, MixColor(COL_BG_TOP, COL_BG_BOT, 0.2f));

    // ── Title bar ─────────────────────────────────────────────
    RECT titleRc = { 0, 0, rc.right, TITLE_HEIGHT };
    GradientV(mem, titleRc, COL_TITLE_TOP, COL_TITLE_BOT);

    // Decorative brackets for title area (Galgame style)
    {
        HPEN bpen = CreatePen(PS_SOLID, 2, COL_ACCENT);
        HGDIOBJ old = SelectObject(mem, bpen);
        // Top-left bracket
        MoveToEx(mem, 10, 10, NULL); LineTo(mem, 25, 10);
        MoveToEx(mem, 10, 10, NULL); LineTo(mem, 10, 25);
        // Bottom-right bracket (preview area)
        int py = rc.bottom - PREVIEW_HEIGHT;
        MoveToEx(mem, rc.right - 10, rc.bottom - 10, NULL); LineTo(mem, rc.right - 25, rc.bottom - 10);
        MoveToEx(mem, rc.right - 10, rc.bottom - 10, NULL); LineTo(mem, rc.right - 10, rc.bottom - 25);
        SelectObject(mem, old);
        DeleteObject(bpen);
    }

    // Bottom accent line (sakura pink, 2px)
    HPEN accentPen = CreatePen(PS_SOLID, 2, COL_ACCENT);
    HGDIOBJ oldPen = SelectObject(mem, accentPen);
    MoveToEx(mem, 0, TITLE_HEIGHT - 1, NULL);
    LineTo(mem, rc.right, TITLE_HEIGHT - 1);
    SelectObject(mem, oldPen);
    DeleteObject(accentPen);

    // Decorative petals on title bar
    DrawPetal(mem, 14, TITLE_HEIGHT / 2, 5, 7, MixColor(COL_ACCENT, COL_TITLE_TOP, 0.3f));
    DrawPetal(mem, 28, TITLE_HEIGHT / 2 - 6, 3, 4, MixColor(COL_ACCENT2, COL_TITLE_TOP, 0.5f));
    DrawSparkle(mem, rc.right - 116, 12, 3, MixColor(COL_ACCENT2, COL_TITLE_TOP, 0.4f));
    DrawSparkle(mem, 50, 8, 2, COL_ACCENT3);

    // Title text
    HFONT titleFont = CreateFontW(-SF(15), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"MS UI Gothic");  // Use a more anime-style font if available
    SelectObject(mem, titleFont);
    SetTextColor(mem, COL_ACCENT);
    RECT ttRc = { 45, 0, rc.right - 168, TITLE_HEIGHT };
    DrawTextW(mem, L"✧ 字体选择 ✧", -1, &ttRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);  // ✧ 字体选择 ✧
    DeleteObject(titleFont);

    // Font count in title (subtle)
    HFONT countFont = CreateFontW(-SF(11), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    SelectObject(mem, countFont);
    SetTextColor(mem, COL_TEXT_DIM);
    wchar_t countBuf[64];
    wsprintfW(countBuf, L"%d 个字体", (int)g_filteredIndices.size());  // 个字体
    RECT cntRc = { 40, 0, rc.right - 168, TITLE_HEIGHT };
    DrawTextW(mem, countBuf, -1, &cntRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(countFont);

    // Zoom A-/A+ buttons (before close/minimize)
    int zoomBtnW = 34;
    int zoomOutLeft = rc.right - 88 - zoomBtnW * 2 - 4;
    int zoomInLeft = zoomOutLeft + zoomBtnW + 2;
    RECT zoomOutRc = { zoomOutLeft, 0, zoomOutLeft + zoomBtnW, TITLE_HEIGHT };
    RECT zoomInRc  = { zoomInLeft, 0, zoomInLeft + zoomBtnW, TITLE_HEIGHT };
    if (g_zoomOutHovered)
        FillRoundRect(mem, { zoomOutRc.left + 2, 6, zoomOutRc.right - 2, TITLE_HEIGHT - 6 }, 8, COL_BTN_HOV);
    if (g_zoomInHovered)
        FillRoundRect(mem, { zoomInRc.left + 2, 6, zoomInRc.right - 2, TITLE_HEIGHT - 6 }, 8, COL_BTN_HOV);
    HFONT zoomFont = CreateFontW(-SF(13), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    SelectObject(mem, zoomFont);
    SetTextColor(mem, g_zoomOutHovered ? COL_ACCENT : COL_TEXT_DIM);
    DrawTextW(mem, L"A-", -1, &zoomOutRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SetTextColor(mem, g_zoomInHovered ? COL_ACCENT : COL_TEXT_DIM);
    DrawTextW(mem, L"A+", -1, &zoomInRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(zoomFont);

    // Close button
    RECT closeRc = { rc.right - 46, 0, rc.right, TITLE_HEIGHT };
    if (g_closeHovered)
        FillRoundRect(mem, { rc.right - 42, 6, rc.right - 4, TITLE_HEIGHT - 6 }, 8, COL_CLOSE_HOV);
    HFONT symFont = CreateFontW(-SF(16), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI Symbol");
    SelectObject(mem, symFont);
    SetTextColor(mem, g_closeHovered ? RGB(255, 255, 255) : COL_TEXT);
    DrawTextW(mem, L"×", -1, &closeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(symFont);

    // Minimize button
    RECT minRc = { rc.right - 88, 0, rc.right - 46, TITLE_HEIGHT };
    if (g_minHovered)
        FillRoundRect(mem, { rc.right - 86, 6, rc.right - 48, TITLE_HEIGHT - 6 }, 8, COL_BTN_HOV);
    HFONT minFont = CreateFontW(-SF(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    SelectObject(mem, minFont);
    SetTextColor(mem, COL_TEXT);
    DrawTextW(mem, L"—", -1, &minRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(minFont);

    // ── Search area ───────────────────────────────────────────
    RECT searchAreaRc = { 0, TITLE_HEIGHT, rc.right, TITLE_HEIGHT + SEARCH_HEIGHT };
    {
        HBRUSH sbr = CreateSolidBrush(COL_SEARCH_BG);
        FillRect(mem, &searchAreaRc, sbr);
        DeleteObject(sbr);
    }

    // Search box outline (rounded, with pink glow when focused)
    HWND focusWnd = GetFocus();
    COLORREF searchBorderCol = (focusWnd == g_hSearchEdit) ? COL_ACCENT : COL_BORDER;
    RECT searchBoxRc = { 14, TITLE_HEIGHT + 10, WINDOW_WIDTH - 14, TITLE_HEIGHT + SEARCH_HEIGHT - 10 };
    DrawRoundRectOutline(mem, searchBoxRc, 14, searchBorderCol);

    // Search icon (magnifying glass)
    HPEN iconPen = CreatePen(PS_SOLID, 2, COL_TEXT_DIM);
    oldPen = SelectObject(mem, iconPen);
    HGDIOBJ oldIconBr = SelectObject(mem, GetStockObject(NULL_BRUSH));
    int iconCx = WINDOW_WIDTH - 32, iconCy = TITLE_HEIGHT + SEARCH_HEIGHT / 2 - 2;
    Ellipse(mem, iconCx - 6, iconCy - 6, iconCx + 4, iconCy + 4);
    MoveToEx(mem, iconCx + 2, iconCy + 2, NULL);
    LineTo(mem, iconCx + 7, iconCy + 7);
    SelectObject(mem, oldIconBr);
    SelectObject(mem, oldPen);
    DeleteObject(iconPen);

    // ── Charset selector bar ─────────────────────────────────
    {
        int csTop = TITLE_HEIGHT + SEARCH_HEIGHT;
        RECT csBg = { 0, csTop, rc.right, csTop + CHARSET_HEIGHT };
        HBRUSH csBgBr = CreateSolidBrush(RGB(24, 18, 40));
        FillRect(mem, &csBg, csBgBr);
        DeleteObject(csBgBr);

        // Bottom separator line
        HPEN csSep = CreatePen(PS_SOLID, 1, COL_SEPARATOR);
        HGDIOBJ oldCsSep = SelectObject(mem, csSep);
        MoveToEx(mem, 0, csTop + CHARSET_HEIGHT - 1, NULL);
        LineTo(mem, rc.right, csTop + CHARSET_HEIGHT - 1);
        SelectObject(mem, oldCsSep);
        DeleteObject(csSep);

        HFONT csFont = CreateFontW(-SF(11), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        HGDIOBJ oldCsFont = SelectObject(mem, csFont);

        // Clip to charset bar area
        HRGN clipRgn = CreateRectRgn(4, csTop, rc.right - 4, csTop + CHARSET_HEIGHT);
        SelectClipRgn(mem, clipRgn);

        int pillX = 8 - g_charsetScrollX;
        int pillY = csTop + 6;
        int pillH = CHARSET_HEIGHT - 12;

        for (int i = 0; i < g_charsetCount; i++) {
            SIZE sz;
            GetTextExtentPoint32W(mem, g_charsets[i].label, (int)wcslen(g_charsets[i].label), &sz);
            int pillW = sz.cx + SF(16);

            if (pillX + pillW > 0 && pillX < rc.right) {
                RECT pillRc = { pillX, pillY, pillX + pillW, pillY + pillH };
                bool isSel = (i == g_selectedCharset);
                bool isHov = (i == g_hoveredCharset);

                if (isSel) {
                    FillRoundRect(mem, pillRc, 10, COL_SELECTED_BG);
                    DrawRoundRectOutline(mem, pillRc, 10, COL_ACCENT);
                    SetTextColor(mem, COL_ACCENT);
                } else if (isHov) {
                    FillRoundRect(mem, pillRc, 10, COL_HOVER_BG);
                    SetTextColor(mem, COL_TEXT);
                } else {
                    FillRoundRect(mem, pillRc, 10, RGB(32, 26, 50));
                    SetTextColor(mem, COL_TEXT_DIM);
                }
                DrawTextW(mem, g_charsets[i].label, -1, &pillRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            pillX += pillW + 5;
        }

        // Restore clip region
        SelectClipRgn(mem, NULL);
        DeleteObject(clipRgn);
        SelectObject(mem, oldCsFont);
        DeleteObject(csFont);
    }

    // ── Codepage Spoof bar ───────────────────────────────────
    {
        int spTop = TITLE_HEIGHT + SEARCH_HEIGHT + CHARSET_HEIGHT;
        RECT spBg = { 0, spTop, rc.right, spTop + SPOOF_HEIGHT };
        HBRUSH spBgBr = CreateSolidBrush(RGB(20, 16, 34));
        FillRect(mem, &spBg, spBgBr);
        DeleteObject(spBgBr);

        // Bottom separator
        HPEN spSep = CreatePen(PS_SOLID, 1, COL_SEPARATOR);
        HGDIOBJ oldSpSep = SelectObject(mem, spSep);
        MoveToEx(mem, 0, spTop + SPOOF_HEIGHT - 1, NULL);
        LineTo(mem, rc.right, spTop + SPOOF_HEIGHT - 1);
        SelectObject(mem, oldSpSep);
        DeleteObject(spSep);

        HFONT spFont = CreateFontW(-SF(11), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        HFONT spFontBold = CreateFontW(-SF(11), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        HGDIOBJ oldSpFont = SelectObject(mem, spFont);

        int py = spTop + (SPOOF_HEIGHT - 24) / 2;
        int px = 10;

        // Toggle button: [Spoof: ON/OFF]
        {
            const wchar_t* label = g_spoofEnabled ? L"伪装: 开" : L"伪装: 关";  // 伪装: 开/关
            SelectObject(mem, spFontBold);
            SIZE sz; GetTextExtentPoint32W(mem, label, (int)wcslen(label), &sz);
            int bw = sz.cx + 16;
            RECT btnRc = { px, py, px + bw, py + 24 };
            COLORREF bgCol = g_spoofEnabled ? RGB(40, 80, 55) : RGB(50, 35, 65);
            if (g_spoofToggleHovered) bgCol = g_spoofEnabled ? RGB(50, 100, 65) : RGB(65, 48, 85);
            FillRoundRect(mem, btnRc, 10, bgCol);
            DrawRoundRectOutline(mem, btnRc, 10, g_spoofEnabled ? RGB(100, 220, 140) : COL_BORDER);
            SetTextColor(mem, g_spoofEnabled ? RGB(100, 255, 160) : COL_TEXT_DIM);
            DrawTextW(mem, label, -1, &btnRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            px += bw + 8;
        }

        // "From" pill
        SelectObject(mem, spFont);
        {
            const wchar_t* fromLabel = g_charsets[g_spoofFromIdx].label;
            SIZE sz; GetTextExtentPoint32W(mem, fromLabel, (int)wcslen(fromLabel), &sz);
            int bw = sz.cx + 16;
            RECT pillRc = { px, py, px + bw, py + 24 };
            COLORREF bgCol = g_spoofFromHovered ? COL_HOVER_BG : RGB(38, 30, 56);
            FillRoundRect(mem, pillRc, 10, bgCol);
            DrawRoundRectOutline(mem, pillRc, 10, RGB(200, 120, 120));
            SetTextColor(mem, RGB(255, 160, 160));
            DrawTextW(mem, fromLabel, -1, &pillRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            px += bw + 4;
        }

        // Arrow
        {
            SetTextColor(mem, COL_TEXT_DIM);
            RECT arrowRc = { px, py, px + 24, py + 24 };
            DrawTextW(mem, L"→", -1, &arrowRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            px += 24 + 4;
        }

        // "To" pill
        {
            const wchar_t* toLabel = g_charsets[g_spoofToIdx].label;
            SIZE sz; GetTextExtentPoint32W(mem, toLabel, (int)wcslen(toLabel), &sz);
            int bw = sz.cx + 16;
            RECT pillRc = { px, py, px + bw, py + 24 };
            COLORREF bgCol = g_spoofToHovered ? COL_HOVER_BG : RGB(38, 30, 56);
            FillRoundRect(mem, pillRc, 10, bgCol);
            DrawRoundRectOutline(mem, pillRc, 10, RGB(120, 200, 140));
            SetTextColor(mem, RGB(160, 255, 180));
            DrawTextW(mem, toLabel, -1, &pillRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            px += bw + 8;
        }

        // Hint text
        {
            SetTextColor(mem, RGB(70, 55, 95));
            wchar_t hint[64];
            wsprintfW(hint, L"(%d \x2192 %d)",
                (int)g_charsets[g_spoofFromIdx].value,
                (int)g_charsets[g_spoofToIdx].value);
            RECT hintRc = { px, py, rc.right - 8, py + 24 };
            DrawTextW(mem, hint, -1, &hintRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }

        SelectObject(mem, oldSpFont);
        DeleteObject(spFont);
        DeleteObject(spFontBold);
    }

    // ── Font list ─────────────────────────────────────────────
    int yStart = GetListTop();
    int itemCount = (int)g_filteredIndices.size();
    int maxVisible = min(VISIBLE_ITEMS, itemCount - g_scrollOffset);
    if (maxVisible < 0) maxVisible = 0;

    HFONT uiSmall = CreateFontW(-SF(11), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HFONT uiBadge = CreateFontW(-SF(10), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

    BYTE gameCs = GetGameCharset();

    for (int i = 0; i < maxVisible; i++) {
        int idx = g_scrollOffset + i;
        int itemTop = yStart + i * ITEM_HEIGHT;
        RECT itemRc = { 8, itemTop + 2, rc.right - SCROLLBAR_WIDTH - SCROLLBAR_MARGIN * 2 - 4, itemTop + ITEM_HEIGHT - 2 };

        bool selected = (idx == g_selectedIndex);
        bool hovered = (idx == g_hoveredIndex);
        bool isRecent = IsRecentFont(idx);
        bool isApplied = IsAppliedFont(idx);
        const std::wstring& fontName = g_allFonts[g_filteredIndices[idx]];

        // Item background
        if (selected) {
            // Selected: filled rounded rect + left accent bar + glow effect
            RECT glowRc = { itemRc.left - 2, itemRc.top - 2, itemRc.right + 2, itemRc.bottom + 2 };
            DrawRoundRectOutline(mem, glowRc, 12, COL_SELECTED_GLO);
            FillRoundRect(mem, itemRc, 10, COL_SELECTED_BG);
            RECT bar = { itemRc.left + 1, itemRc.top + 8, itemRc.left + 4, itemRc.bottom - 8 };
            FillRoundRect(mem, bar, 2, COL_ACCENT);
        } else if (hovered) {
            FillRoundRect(mem, itemRc, 10, COL_HOVER_BG);
            DrawRoundRectOutline(mem, itemRc, 10, MixColor(COL_HOVER_BG, COL_ACCENT, 0.3f));
        }

        int textLeft = itemRc.left + 14;
        int badgeRight = itemRc.right - 8;

        // Badges (from right to left)
        if (isApplied) {
            SelectObject(mem, uiBadge);
            SIZE sz; GetTextExtentPoint32W(mem, L"✓ 已应用", 5, &sz);  // 已应用
            int bw = sz.cx + 12;
            RECT bdgRc = { badgeRight - bw, itemRc.top + (ITEM_HEIGHT - 20) / 2, badgeRight, itemRc.top + (ITEM_HEIGHT + 16) / 2 };
            FillRoundRect(mem, bdgRc, 9, COL_APPLIED_BG);
            SetTextColor(mem, COL_APPLIED_TEXT);
            DrawTextW(mem, L"✓ 已应用", -1, &bdgRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);  // 已应用
            badgeRight -= bw + 6;
        }
        if (isRecent && !isApplied) {
            SelectObject(mem, uiBadge);
            SIZE sz; GetTextExtentPoint32W(mem, L"☆ 最近", 4, &sz);  // 最近
            int bw = sz.cx + 12;
            RECT bdgRc = { badgeRight - bw, itemRc.top + (ITEM_HEIGHT - 20) / 2, badgeRight, itemRc.top + (ITEM_HEIGHT + 16) / 2 };
            FillRoundRect(mem, bdgRc, 9, COL_BADGE_BG);
            SetTextColor(mem, COL_BADGE_TEXT);
            DrawTextW(mem, L"☆ 最近", -1, &bdgRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);  // 最近
            badgeRight -= bw + 6;
        }

        // Font name in its own typeface (primary line)
        HFONT itemFont = CreateFontW(-SF(17), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            gameCs, 0, 0, CLEARTYPE_QUALITY, 0, fontName.c_str());
        SelectObject(mem, itemFont);
        SetTextColor(mem, selected ? COL_ACCENT : COL_TEXT);
        RECT nameRc = { textLeft, itemRc.top, badgeRight - 4, itemRc.bottom };
        DrawTextW(mem, fontName.c_str(), -1, &nameRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        DeleteObject(itemFont);

        // Separator
        if (i < maxVisible - 1 && !selected) {
            HPEN sepPen = CreatePen(PS_SOLID, 1, COL_SEPARATOR);
            HGDIOBJ oldSep = SelectObject(mem, sepPen);
            MoveToEx(mem, textLeft, itemRc.bottom + 1, NULL);
            LineTo(mem, itemRc.right - 14, itemRc.bottom + 1);
            SelectObject(mem, oldSep);
            DeleteObject(sepPen);
        }
    }

    DeleteObject(uiSmall);
    DeleteObject(uiBadge);

    // ── Scrollbar ─────────────────────────────────────────────
    if (itemCount > VISIBLE_ITEMS) {
        RECT thumbRc, trackRc;
        GetScrollbarRect(&thumbRc, &trackRc);

        // Track
        FillRoundRect(mem, trackRc, 4, COL_SB_TRACK);

        // Thumb
        COLORREF thumbCol = COL_SB_THUMB;
        if (g_scrollbarDragging) thumbCol = COL_SB_THUMB_DRG;
        else if (g_scrollbarHovered) thumbCol = COL_SB_THUMB_HOV;
        FillRoundRect(mem, thumbRc, 4, thumbCol);
    }

    // ── Preview panel ─────────────────────────────────────────
    int prevTop = rc.bottom - PREVIEW_HEIGHT;
    RECT prevBg = { 0, prevTop, rc.right, rc.bottom };
    GradientV(mem, prevBg, COL_PREVIEW_TOP, COL_PREVIEW_BOT);

    // Dialogue box style border
    {
        RECT boxRc = { 10, prevTop + 10, rc.right - 10, rc.bottom - 10 };
        DrawRoundRectOutline(mem, boxRc, 12, COL_BORDER);
        // Inner thin line for dialogue box feel
        RECT innerRc = { 12, prevTop + 12, rc.right - 12, rc.bottom - 12 };
        DrawRoundRectOutline(mem, innerRc, 10, MixColor(COL_BORDER, COL_ACCENT, 0.2f));
    }

    // Top border + accent line
    {
        HPEN bp = CreatePen(PS_SOLID, 1, COL_BORDER);
        HGDIOBJ op = SelectObject(mem, bp);
        MoveToEx(mem, 0, prevTop, NULL); LineTo(mem, rc.right, prevTop);
        SelectObject(mem, op); DeleteObject(bp);
    }
    {
        HPEN ap = CreatePen(PS_SOLID, 1, MixColor(COL_ACCENT, COL_PREVIEW_TOP, 0.6f));
        HGDIOBJ op = SelectObject(mem, ap);
        MoveToEx(mem, 0, prevTop + 1, NULL); LineTo(mem, rc.right, prevTop + 1);
        SelectObject(mem, op); DeleteObject(ap);
    }

    // Decorative sparkle in preview corner
    DrawSparkle(mem, rc.right - 24, prevTop + 14, 3, MixColor(COL_ACCENT2, COL_PREVIEW_TOP, 0.5f));
    DrawPetal(mem, rc.right - 42, prevTop + 20, 3, 4, MixColor(COL_ACCENT, COL_PREVIEW_TOP, 0.6f));
    DrawPetal(mem, 24, rc.bottom - 20, 4, 5, COL_ACCENT3);

    if (g_selectedIndex >= 0 && g_selectedIndex < (int)g_filteredIndices.size()) {
        const std::wstring& fontName = g_allFonts[g_filteredIndices[g_selectedIndex]];

        // Full preview text
        HFONT prevFont = CreateFontW(-SF(28), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            gameCs, 0, 0, CLEARTYPE_QUALITY, 0, fontName.c_str());
        SelectObject(mem, prevFont);
        SetTextColor(mem, COL_TEXT);
        RECT contentRc = { 24, prevTop + 24, rc.right - 24, rc.bottom - 32 };
        DrawTextW(mem, L"「测试文字 Sample Text」\nあいうえお 0123456789", -1,  // 「测试文字」
            &contentRc, DT_LEFT | DT_WORDBREAK);
        DeleteObject(prevFont);

        // Bottom info line
        HFONT infoFont = CreateFontW(-SF(11), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        SelectObject(mem, infoFont);

        SetTextColor(mem, COL_TEXT_HINT);
        RECT infoRc = { 18, rc.bottom - 22, rc.right / 2 + 60, rc.bottom - 4 };
        wchar_t infoBuf[256];
        
        // Map detected charset to name
        const wchar_t* detName = L"未知"; // 未知
        for(int i=0; i<g_charsetCount; i++) {
            if(g_charsets[i].value == Config::DetectedCharset) {
                detName = g_charsets[i].label;
                break;
            }
        }

        wsprintfW(infoBuf, L"游戏: %s (%d)  |  选择: %s (%d)  |  %d/%d", 
            detName, (int)Config::DetectedCharset,
            g_charsets[g_selectedCharset].label, (int)g_charsets[g_selectedCharset].value,
            g_selectedIndex + 1, itemCount); // 游戏: ... | 选择: ...
        DrawTextW(mem, infoBuf, -1, &infoRc, DT_LEFT | DT_SINGLELINE);

        SetTextColor(mem, RGB(80, 62, 110));
        RECT hintRc = { rc.right / 2 + 60, rc.bottom - 22, rc.right - 14, rc.bottom - 4 };
        DrawTextW(mem, L"Enter=应用  Esc=隐藏", -1, &hintRc, DT_RIGHT | DT_SINGLELINE);
        DeleteObject(infoFont);
    }

    // ── Window border ─────────────────────────────────────────
    {
        HPEN bp = CreatePen(PS_SOLID, 1, COL_BORDER);
        HGDIOBJ op = SelectObject(mem, bp);
        HGDIOBJ ob = SelectObject(mem, GetStockObject(NULL_BRUSH));
        Rectangle(mem, 0, 0, rc.right, rc.bottom);
        SelectObject(mem, ob);
        SelectObject(mem, op);
        DeleteObject(bp);
    }

    BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, oldBmp);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(hWnd, &ps);
}

// ═══════════════════════════════════════════════════════════════
//  Scroll / selection helpers
// ═══════════════════════════════════════════════════════════════
static void EnsureVisible() {
    if (g_selectedIndex < g_scrollOffset)
        g_scrollOffset = g_selectedIndex;
    if (g_selectedIndex >= g_scrollOffset + VISIBLE_ITEMS)
        g_scrollOffset = g_selectedIndex - VISIBLE_ITEMS + 1;
}

static void ClampScroll() {
    int mx = (int)g_filteredIndices.size() - VISIBLE_ITEMS;
    if (mx < 0) mx = 0;
    if (g_scrollOffset < 0) g_scrollOffset = 0;
    if (g_scrollOffset > mx) g_scrollOffset = mx;
}

// ═══════════════════════════════════════════════════════════════
//  WndProc
// ═══════════════════════════════════════════════════════════════
static LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_UP || wParam == VK_DOWN || wParam == VK_PRIOR || wParam == VK_NEXT || wParam == VK_RETURN) {
            SendMessage(GetParent(hWnd), msg, wParam, lParam);
            return 0;
        }
    }
    return CallWindowProc(g_OldEditProc, hWnd, msg, wParam, lParam);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_PAINT:
        PaintWindow(hWnd);
        return 0;

    case WM_NCHITTEST: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hWnd, &pt);
        int x = pt.x, y = pt.y;
        int w = WINDOW_WIDTH, h = WINDOW_HEIGHT;
        int b = RESIZE_BORDER;

        // Corners (prioritized)
        if (x < b && y < b)             return HTTOPLEFT;
        if (x >= w - b && y < b)        return HTTOPRIGHT;
        if (x < b && y >= h - b)        return HTBOTTOMLEFT;
        if (x >= w - b && y >= h - b)   return HTBOTTOMRIGHT;
        // Edges
        if (x < b)                      return HTLEFT;
        if (x >= w - b)                 return HTRIGHT;
        if (y < b)                      return HTTOP;
        if (y >= h - b)                 return HTBOTTOM;
        // Title bar (caption for drag, but not over buttons including zoom A-/A+)
        {
            int zoomBtnW = 34;
            int zoomOutL = w - 88 - zoomBtnW * 2 - 4;
            if (y < TITLE_HEIGHT && x < zoomOutL) return HTCAPTION;
        }
        return HTCLIENT;
    }

    case WM_NCLBUTTONDBLCLK: return 0;

    // Remove system non-client frame so our custom border is used
    case WM_NCCALCSIZE:
        if (wParam == TRUE) return 0;  // client area = full window
        break;

    case WM_COMMAND:
        if (HIWORD(wParam) == EN_CHANGE) {
            UpdateFilter();
            g_scrollOffset = 0;
            g_selectedIndex = g_filteredIndices.empty() ? -1 : 0;
            InvalidateRect(hWnd, NULL, FALSE);
        }
        break;

    case WM_KEYDOWN:
        switch (wParam) {
        case VK_UP:
            if (g_selectedIndex > 0) { g_selectedIndex--; EnsureVisible(); InvalidateRect(hWnd, NULL, FALSE); }
            return 0;
        case VK_DOWN:
            if (g_selectedIndex < (int)g_filteredIndices.size() - 1) { g_selectedIndex++; EnsureVisible(); InvalidateRect(hWnd, NULL, FALSE); }
            return 0;
        case VK_PRIOR:
            g_selectedIndex -= VISIBLE_ITEMS;
            if (g_selectedIndex < 0) g_selectedIndex = 0;
            EnsureVisible(); InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        case VK_NEXT:
            g_selectedIndex += VISIBLE_ITEMS;
            if (g_selectedIndex >= (int)g_filteredIndices.size()) g_selectedIndex = (int)g_filteredIndices.size() - 1;
            EnsureVisible(); InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        case VK_HOME:
            g_selectedIndex = 0; EnsureVisible(); InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        case VK_END:
            g_selectedIndex = (int)g_filteredIndices.size() - 1; EnsureVisible(); InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        case VK_RETURN:
            ApplySelectedFont(); InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        case VK_ESCAPE:
            ShowWindow(hWnd, SW_HIDE); g_visible = false;
            return 0;
        }
        break;

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        // Check if mouse is over charset bar for horizontal scroll
        POINT wpt;
        GetCursorPos(&wpt);
        ScreenToClient(hWnd, &wpt);
        int csTop = TITLE_HEIGHT + SEARCH_HEIGHT;
        if (wpt.y >= csTop && wpt.y < csTop + CHARSET_HEIGHT) {
            g_charsetScrollX -= (delta / WHEEL_DELTA) * 40;
            int totalW = 0;
            CharsetHitTest(0, csTop + 8, &totalW);
            int maxScrlX = max(0, totalW - WINDOW_WIDTH + 8);
            if (g_charsetScrollX < 0) g_charsetScrollX = 0;
            if (g_charsetScrollX > maxScrlX) g_charsetScrollX = maxScrlX;
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }
        g_scrollOffset -= (delta / WHEEL_DELTA) * 3;
        ClampScroll();
        InvalidateRect(hWnd, NULL, FALSE);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hWnd, 0 };
        TrackMouseEvent(&tme);

        // Title buttons (zoom A-/A+, minimize, close)
        bool inClose = (x >= WINDOW_WIDTH - 46 && y < TITLE_HEIGHT);
        bool inMin = (x >= WINDOW_WIDTH - 88 && x < WINDOW_WIDTH - 46 && y < TITLE_HEIGHT);
        int zBtnW = 34;
        int zOutL = WINDOW_WIDTH - 88 - zBtnW * 2 - 4;
        int zInL = zOutL + zBtnW + 2;
        bool inZoomOut = (y < TITLE_HEIGHT && x >= zOutL && x < zOutL + zBtnW);
        bool inZoomIn  = (y < TITLE_HEIGHT && x >= zInL && x < zInL + zBtnW);
        if (inClose != g_closeHovered || inMin != g_minHovered ||
            inZoomOut != g_zoomOutHovered || inZoomIn != g_zoomInHovered) {
            g_closeHovered = inClose; g_minHovered = inMin;
            g_zoomOutHovered = inZoomOut; g_zoomInHovered = inZoomIn;
            InvalidateRect(hWnd, NULL, FALSE);
        }

        // Scrollbar drag
        if (g_scrollbarDragging) {
            int cnt = (int)g_filteredIndices.size();
            int maxScroll = max(1, cnt - VISIBLE_ITEMS);
            int listTop = GetListTop() + 4;
            int listBot = WINDOW_HEIGHT - PREVIEW_HEIGHT - 4;
            int trackH = listBot - listTop;
            int calcH = trackH * VISIBLE_ITEMS / cnt;
            int thumbH = max(28, calcH);
            int scrollRange = max(1, trackH - thumbH);

            int dy = y - g_scrollDragStartY;
            int newOff = g_scrollDragStartOffset + dy * maxScroll / scrollRange;
            newOff = max(0, min(maxScroll, newOff));
            if (newOff != g_scrollOffset) { g_scrollOffset = newOff; InvalidateRect(hWnd, NULL, FALSE); }
            return 0;
        }

        // Scrollbar hover
        if ((int)g_filteredIndices.size() > VISIBLE_ITEMS) {
            RECT thumbRc;
            GetScrollbarRect(&thumbRc, NULL);
            bool inThumb = PointInRect(x, y, thumbRc);
            if (inThumb != g_scrollbarHovered) { g_scrollbarHovered = inThumb; InvalidateRect(hWnd, NULL, FALSE); }
        }

        // Charset bar hover
        {
            int csTop = TITLE_HEIGHT + SEARCH_HEIGHT;
            int newHov = -1;
            if (y >= csTop && y < csTop + CHARSET_HEIGHT)
                newHov = CharsetHitTest(x, y);
            if (newHov != g_hoveredCharset) {
                g_hoveredCharset = newHov;
                InvalidateRect(hWnd, NULL, FALSE);
            }
        }

          // Spoof bar hover
          {
              int spTop = TITLE_HEIGHT + SEARCH_HEIGHT + CHARSET_HEIGHT;
              bool newToggle = false, newFrom = false, newTo = false;
              if (y >= spTop && y < spTop + SPOOF_HEIGHT) {
                  int hit = SpoofBarHitTest(x, y);
                  newToggle = (hit == 0);
                  newFrom = (hit == 1);
                  newTo = (hit == 2);
              }
              if (newToggle != g_spoofToggleHovered || newFrom != g_spoofFromHovered || newTo != g_spoofToHovered) {
                  g_spoofToggleHovered = newToggle;
                  g_spoofFromHovered = newFrom;
                  g_spoofToHovered = newTo;
                  InvalidateRect(hWnd, NULL, FALSE);
              }
          }

          // List hover
          int listY = y - GetListTop();
        int hi = g_scrollOffset + listY / ITEM_HEIGHT;
        if (listY >= 0 && y < WINDOW_HEIGHT - PREVIEW_HEIGHT && hi >= 0 && hi < (int)g_filteredIndices.size()) {
            if (g_hoveredIndex != hi) { g_hoveredIndex = hi; InvalidateRect(hWnd, NULL, FALSE); }
        } else if (g_hoveredIndex != -1) {
            g_hoveredIndex = -1; InvalidateRect(hWnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        if (g_hoveredIndex != -1 || g_closeHovered || g_minHovered || g_scrollbarHovered || g_hoveredCharset != -1
            || g_zoomInHovered || g_zoomOutHovered
            || g_spoofToggleHovered || g_spoofFromHovered || g_spoofToHovered) {
            g_hoveredIndex = -1; g_closeHovered = false; g_minHovered = false; g_scrollbarHovered = false;
            g_hoveredCharset = -1; g_zoomInHovered = false; g_zoomOutHovered = false;
            g_spoofToggleHovered = false; g_spoofFromHovered = false; g_spoofToHovered = false;
            InvalidateRect(hWnd, NULL, FALSE);
        }
        return 0;

    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        // Zoom A-/A+ buttons
        if (y < TITLE_HEIGHT) {
            int zBtnW = 34;
            int zOutL = WINDOW_WIDTH - 88 - zBtnW * 2 - 4;
            int zInL = zOutL + zBtnW + 2;
            if (x >= zOutL && x < zOutL + zBtnW) {
                g_uiScale = max(UI_SCALE_MIN, g_uiScale - UI_SCALE_STEP);
                RecalcLayout(); ClampScroll(); InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            if (x >= zInL && x < zInL + zBtnW) {
                g_uiScale = min(UI_SCALE_MAX, g_uiScale + UI_SCALE_STEP);
                RecalcLayout(); ClampScroll(); InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
        }

        // Close
        if (x >= WINDOW_WIDTH - 46 && y < TITLE_HEIGHT) {
            ShowWindow(hWnd, SW_HIDE); g_visible = false; return 0;
        }
        // Minimize
        if (x >= WINDOW_WIDTH - 88 && x < WINDOW_WIDTH - 46 && y < TITLE_HEIGHT) {
            ShowWindow(hWnd, SW_MINIMIZE); return 0;
        }

        // Charset bar click
        {
            int csTop = TITLE_HEIGHT + SEARCH_HEIGHT;
            if (y >= csTop && y < csTop + CHARSET_HEIGHT) {
                int hit = CharsetHitTest(x, y);
                if (hit >= 0 && hit != g_selectedCharset) {
                    g_selectedCharset = hit;
                    g_scrollOffset = 0;
                    UpdateFilter();
                    InvalidateRect(hWnd, NULL, FALSE);
                }
          return 0;
              }
          }

          // Spoof bar click
          {
              int spTop = TITLE_HEIGHT + SEARCH_HEIGHT + CHARSET_HEIGHT;
              if (y >= spTop && y < spTop + SPOOF_HEIGHT) {
                  int hit = SpoofBarHitTest(x, y);
                  if (hit == 0) {
                      // Toggle spoof on/off
                      g_spoofEnabled = !g_spoofEnabled;
                      ApplySpoofConfig();
                    } else if (hit == 1) {
                        // Cycle "from" charset (include all charsets)
                        g_spoofFromIdx++;
                        if (g_spoofFromIdx >= g_charsetCount) g_spoofFromIdx = 0;
                        if (g_spoofFromIdx == g_spoofToIdx) { g_spoofFromIdx++; if (g_spoofFromIdx >= g_charsetCount) g_spoofFromIdx = 0; }
                        ApplySpoofConfig();
                    } else if (hit == 2) {
                        // Cycle "to" charset (include all charsets)
                        g_spoofToIdx++;
                        if (g_spoofToIdx >= g_charsetCount) g_spoofToIdx = 0;
                        if (g_spoofToIdx == g_spoofFromIdx) { g_spoofToIdx++; if (g_spoofToIdx >= g_charsetCount) g_spoofToIdx = 0; }
                        ApplySpoofConfig();
                  }
                  InvalidateRect(hWnd, NULL, FALSE);
                  return 0;
              }
          }

          // Scrollbar
        if ((int)g_filteredIndices.size() > VISIBLE_ITEMS) {
            RECT thumbRc, trackRc;
            GetScrollbarRect(&thumbRc, &trackRc);
            if (PointInRect(x, y, thumbRc)) {
                g_scrollbarDragging = true;
                g_scrollDragStartY = y;
                g_scrollDragStartOffset = g_scrollOffset;
                SetCapture(hWnd);
                return 0;
            }
            if (PointInRect(x, y, trackRc)) {
                int cnt = (int)g_filteredIndices.size();
                int maxScroll = max(1, cnt - VISIBLE_ITEMS);
                float ratio = (float)(y - trackRc.top) / (float)(trackRc.bottom - trackRc.top);
                g_scrollOffset = (int)(ratio * maxScroll);
                ClampScroll();
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
        }

        // List click
        int listY = y - GetListTop();
        int ci = g_scrollOffset + listY / ITEM_HEIGHT;
        if (listY >= 0 && y < WINDOW_HEIGHT - PREVIEW_HEIGHT && ci >= 0 && ci < (int)g_filteredIndices.size()) {
            g_selectedIndex = ci;
            InvalidateRect(hWnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONUP:
        if (g_scrollbarDragging) {
            g_scrollbarDragging = false;
            ReleaseCapture();
            InvalidateRect(hWnd, NULL, FALSE);
        }
        return 0;

    case WM_LBUTTONDBLCLK: {
        int y = GET_Y_LPARAM(lParam);
        int listY = y - GetListTop();
        int di = g_scrollOffset + listY / ITEM_HEIGHT;
        if (listY >= 0 && y < WINDOW_HEIGHT - PREVIEW_HEIGHT && di >= 0 && di < (int)g_filteredIndices.size()) {
            g_selectedIndex = di;
            ApplySelectedFont();
            InvalidateRect(hWnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_CTLCOLOREDIT: {
        HDC hdcEdit = (HDC)wParam;
        SetTextColor(hdcEdit, COL_TEXT);
        SetBkColor(hdcEdit, COL_SEARCH_BG);
        static HBRUSH editBrush = CreateSolidBrush(COL_SEARCH_BG);
        return (LRESULT)editBrush;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = MIN_WIDTH;
        mmi->ptMinTrackSize.y = MIN_HEIGHT;
        return 0;
    }

    case WM_SIZE: {
        if (wParam == SIZE_MINIMIZED) break;
        RECT cr;
        GetClientRect(hWnd, &cr);
        WINDOW_WIDTH = cr.right;
        WINDOW_HEIGHT = cr.bottom;
        RecalcLayout();
        ClampScroll();
        InvalidateRect(hWnd, NULL, FALSE);
        return 0;
    }

    case WM_ERASEBKGND: return 1;
    case WM_DESTROY: g_hWnd = NULL; PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ═══════════════════════════════════════════════════════════════
//  Window creation
// ═══════════════════════════════════════════════════════════════
static void CreatePickerWindow() {
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = g_hModule;
    wc.lpszClassName = WND_CLASS;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.style = CS_DBLCLKS;
    RegisterClassExW(&wc);

    RecalcLayout();

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    g_hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_APPWINDOW,
        WND_CLASS, L"Font Selection Assistant", WS_POPUP | WS_THICKFRAME,
        (screenW - WINDOW_WIDTH) / 2, (screenH - WINDOW_HEIGHT) / 2,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, g_hModule, NULL);

    // Try to set DWM shadow (Windows 7+)
    BOOL val = TRUE;
    DwmSetWindowAttribute(g_hWnd, 2 /*DWMWA_NCRENDERING_POLICY*/, &val, sizeof(val));
    MARGINS margins = { 1, 1, 1, 1 };
    DwmExtendFrameIntoClientArea(g_hWnd, &margins);

    // Make window slightly transparent
    SetLayeredWindowAttributes(g_hWnd, 0, 248, LWA_ALPHA);
    // Need WS_EX_LAYERED for this to work
    SetWindowLongPtrW(g_hWnd, GWL_EXSTYLE,
        GetWindowLongPtrW(g_hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
    SetLayeredWindowAttributes(g_hWnd, 0, 245, LWA_ALPHA);

    // Search edit
    g_hSearchEdit = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        22, TITLE_HEIGHT + 14, WINDOW_WIDTH - 62, 20,
        g_hWnd, NULL, g_hModule, NULL);
    SendMessage(g_hSearchEdit, EM_SETCUEBANNER, FALSE, (LPARAM)L"  \x2727 Search fonts...");

    if (g_editFont) DeleteObject(g_editFont);
    g_editFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    SendMessage(g_hSearchEdit, WM_SETFONT, (WPARAM)g_editFont, TRUE);

    g_OldEditProc = (WNDPROC)SetWindowLongPtr(g_hSearchEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
}

// ═══════════════════════════════════════════════════════════════
//  Public API
// ═══════════════════════════════════════════════════════════════
void Init(HMODULE hModule) {
    g_hModule = hModule;
    EnumerateFonts();
    CreatePickerWindow();
    ShowWindow(g_hWnd, SW_SHOW);
    SetForegroundWindow(g_hWnd);
    SetFocus(g_hSearchEdit);
    g_visible = true;

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void Toggle() {
    if (!g_hWnd) return;
    g_visible = !g_visible;
    ShowWindow(g_hWnd, g_visible ? SW_SHOW : SW_HIDE);
    if (g_visible) {
        SetForegroundWindow(g_hWnd);
        SetFocus(g_hSearchEdit);
        InvalidateRect(g_hWnd, NULL, FALSE);
    }
}

bool IsVisible() { return g_visible; }

} // namespace FontPicker
