# UI Module Map

`font_picker.cpp` is intentionally small. It keeps the external picker interface
in one place and includes implementation slices from `internal/`.

The internal slices compile as part of `font_picker.cpp`. This preserves the
current picker state locality while making each behaviour area easier to edit.

- `internal/font_picker_state_layout.cppinc`
  - Aggregates `internal/state/*`.
- `internal/font_picker_apply_config.cppinc`
  - Forced redraw, hook enable refresh, metric changes, spoof config application.
- `internal/font_picker_font_list.cppinc`
  - Font enumeration, filtering, local font loading, applying selected fonts.
- `internal/font_picker_paint.cppinc`
  - Aggregates `internal/paint/*`.
- `internal/font_picker_input.cppinc`
  - Aggregates `internal/input/*`.
- `internal/font_picker_lifecycle.cppinc`
  - Window creation, config restore, and public picker loop.

When adding UI controls, put the rendering in `paint`, the input handling in
`input`, and the config mutation in `apply_config`.
