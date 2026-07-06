# Hooks Module Map

All Detours hook implementation belongs in this folder. `dllmain.cpp` is only the
process-load adapter and should stay limited to calling `FontHooks::Install`.
UI code lives in `../ui`; raw font-file mutation lives in `../font`.

## External seam

- `font_hooks.h`
  - Public interface for the hook subsystem.
  - Current interface: `FontHooks::Install(HMODULE module)`.

## Hook implementation

- `font_hooks.cpp`
  - Aggregates internal implementation slices into one translation unit.
  - Keeping one translation unit avoids exposing Detours state and original API
    pointers as broad `extern` state.
- `internal/font_hooks_*.cppinc`
  - Detours adapters and hook handlers grouped by behaviour:
    - `AttachFontCreationHooks`: `CreateFont*` and `CreateFontIndirect*`.
    - `AttachRuntimeLibraryHooks`: late `gdiplus.dll` and `dwrite.dll` loading.
    - `AttachTextDrawingHooks`: `TextOut`, `ExtTextOut`, `DrawText`.
    - `AttachDcFontSelectionHooks`: selected HFONT tracking through DCs.
    - `AttachGlyphOutlineHooks`: glyph bitmap/outline extraction.
    - `AttachTextLayoutMetricHooks`: layout width/extent/ABC metrics.
    - `AttachFontIdentityQueryHooks`: `GetObject`, `GetTextMetrics`, `GetTextFace`.
    - `AttachFontEnumerationHooks`: modern and old font enumeration.
    - `AttachGlyphMetricSupplementHooks`: glyph index, kerning, placement extras.
    - `AttachFontDataQueryHooks`: font data, ranges, language, outline metrics.
    - `AttachDirectWriteHooks`: `DWriteCreateFactory` and factory vtable hooks.
- `internal/model/font_hooks_*.cppinc`
  - Font replacement model internals split by compatibility concern:
    - codepage spoofing and epoch suffixing.
    - text metric and spacing adjustment.
    - replacement font cache and external LOGFONT views.
    - glyph index virtualization.
- `internal/queries/font_hooks_*.cppinc`
  - Font query hooks split by API family:
    - identity queries.
    - enumeration/create-indirect-ex queries.
    - metric/font-data queries.

## Compatibility and debug policy

- `hook_policy.h`
- `hook_policy.cpp`
  - Central place for hook allow/deny decisions.
  - Central place for debug sampling decisions.
  - Add new compatibility toggles here first, then consume them from hook code.

## Adding a new hook

1. Add the typedef and original function pointer in `font_hooks.cpp`.
2. Add the hook handler near related handlers.
3. Register it in the matching `Attach*Hooks` module.
4. If the hook can crash or hang a game, add a config flag and policy function in
   `hook_policy.*` instead of scattering `if (Config::...)` checks everywhere.
5. Add debug trace at the policy decision point when the failure mode is unclear.
