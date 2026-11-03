#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

static bool CheckFace(const wchar_t* expected) {
    LOGFONTW request = {};
    request.lfHeight = -20;
    wcscpy_s(request.lfFaceName, L"Times New Roman");
    HFONT font = CreateFontIndirectW(&request);
    LOGFONTW actual = {};
    const bool ok = font && GetObjectW(font, sizeof(actual), &actual) == sizeof(actual) &&
                    wcscmp(actual.lfFaceName, expected) == 0;
    wprintf(L"requested=%s expected=%s actual=%s\n", request.lfFaceName, expected, actual.lfFaceName);
    if (font) DeleteObject(font);
    return ok;
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        fwprintf(stderr, L"usage: font_hook_smoke.exe <absolute-path-to-proxy-with-test-config>\n");
        return 2;
    }
    if (!CheckFace(L"Times New Roman")) return 1;
    if (!LoadLibraryW(argv[1])) {
        fprintf(stderr, "LoadLibrary failed: %lu\n", GetLastError());
        return 2;
    }
    return CheckFace(L"Segoe UI") ? 0 : 1;
}
