#include "../framework.h"
#include "../winmm_proxy.h"
#include "../font/font_patcher.h"
#include "../ui/font_picker.h"
#include "hook_policy.h"
#include <detours.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <gdiplus.h>
#include <process.h>
#include <psapi.h>
#include <shlwapi.h>
#include <wincrypt.h>
#include <dwrite.h>
#include <intrin.h>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <stdarg.h>
#include <cctype>
#include <cwctype>

#ifdef _WIN64
#pragma comment(lib, "detours_x64.lib")
#else
#pragma comment(lib, "detours.lib")
#endif
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")

#pragma intrinsic(_ReturnAddress)

using namespace Gdiplus;

static bool IsPickerThread();
static bool SoftpalShouldReplaceFontDataQueries();
static bool MiraiShouldReplaceFontDataQueries();
static bool MajiroShouldReplaceFontDataQueries();
static bool DxLibShouldReplaceFontDataQueries();
static bool MajiroShouldDecodeAnsiTextAsCp932Wide();
static bool ArtemisLegacyTrySubstituteMultiByteToWideChar(UINT codepage, DWORD flags,
    LPCCH multiByteText, int multiByteCount, LPWSTR wideText, int wideCount, int* result);
static bool MajiroShouldPreserveDefaultCharset(const LOGFONTW& sourceLogfont);
static bool SoftpalShouldUseNaturalReplacementWidth();


#include "internal/font_hooks_state.cppinc"
#include "internal/font_hooks_font_model.cppinc"
#include "internal/font_hooks_runtime.cppinc"
#include "internal/font_hooks_codepage_redirect.cppinc"
#include "internal/font_hooks_text_substitution.cppinc"
#include "internal/engines/artemis_legacy/artemis_legacy_paths.cppinc"
#include "internal/engines/artemis_legacy/artemis_legacy_text_substitution.cppinc"
#include "internal/engines/artemis_legacy/artemis_legacy_iet_patch.cppinc"
#include "internal/engines/artemis_legacy/artemis_legacy_file_hooks.cppinc"
#include "internal/engines/artemis_legacy/artemis_legacy_notify.cppinc"
#include "internal/engines/artemis/artemis_paths.cppinc"
#include "internal/engines/escude/escude_config.cppinc"
#include "internal/engines/mirai/mirai_font_data.cppinc"
#include "internal/engines/krkr/krkr_paths.cppinc"
#include "internal/engines/softpal/softpal_default_font.cppinc"
#include "internal/engines/majiro/majiro_font_cache.cppinc"
#include "internal/engines/dxlib/dxlib_font_cache.cppinc"
#include "internal/engines/artemis/artemis_pfs.cppinc"
#include "internal/engines/artemis/artemis_table_patch.cppinc"
#include "internal/engines/artemis/artemis_cache.cppinc"
#include "internal/engines/artemis/artemis_file_hooks.cppinc"
#include "internal/engines/engine_file_hooks.cppinc"
#include "internal/font_hooks_text_render.cppinc"
#include "internal/font_hooks_font_queries.cppinc"
#include "internal/font_hooks_font_creation.cppinc"
#include "internal/font_hooks_platform.cppinc"
#include "internal/font_hooks_install.cppinc"
