#define NOMINMAX
#include "font_picker.h"
#include "../font/font_patcher.h"
#include "../framework.h"
#include <algorithm>
#include <commctrl.h>
#include <windowsx.h>
#include <vector>
#include <string>
#include <cmath>
#include <cstdlib>
#include <set>
#include <dwmapi.h>
#include <shlwapi.h>
#include <unordered_map>
#include <unordered_set>
#include <process.h>

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

#include "internal/font_picker_state_layout.cppinc"
#include "internal/font_picker_apply_config.cppinc"
#include "internal/font_picker_font_list.cppinc"
#include "internal/font_picker_paint.cppinc"
#include "internal/font_picker_input.cppinc"
#include "internal/font_picker_lifecycle.cppinc"

} // namespace FontPicker
