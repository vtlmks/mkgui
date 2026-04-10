# Adding a new widget to mkgui

Checklist of every location that must be updated when adding a new widget type.
Forgetting any of these will cause the widget to be missing from the editor
palette, code generation, rendering, or event handling.

## mkgui.h - public API

1. **Widget type enum**
   Add `MKGUI_YOURWIDGET` to the widget type enum (near `MKGUI_BUTTON`,
   `MKGUI_LABEL`, etc.). Order is stable -- append at the end rather than
   inserting in the middle to avoid shifting existing values in saved editor
   project files.

2. **Event type enum** (if the widget emits events)
   Add any new events to the event type enum
   (e.g. `MKGUI_EVENT_YOURWIDGET_CHANGED`).

3. **Per-widget style flags** (if needed)
   Add `MKGUI_YOURWIDGET_*` style flags to the "Widget style flags" section.
   Each widget type has its own bit-0-based namespace in the `style` field, so
   start from `(1u << 0)`.

4. **Public API prototypes** (if the widget has a setup/getter/setter)
   Add `MKGUI_API void mkgui_yourwidget_setup(...)`, etc. under a
   `// YourWidget` comment divider.

## mkgui.c + mkgui_yourwidget.c - implementation

1. **Create `mkgui_yourwidget.c`**
   One file per widget type. It contains:
   - `struct mkgui_yourwidget_data` if the widget has aux state (cursor, scroll,
     selection, callbacks, etc.).
   - `find_yourwidget_data(ctx, widget_id)` aux lookup helper.
   - `render_yourwidget(ctx, idx)` rendering function.
   - Event/input handlers called from the main dispatch path.
   - `MKGUI_API` public setters/getters.

2. **Include it in `mkgui.c`**
   Add `#include "mkgui_yourwidget.c"` in the widget include block in mkgui.c
   (between the other `mkgui_*.c` includes).

3. **Wire up the render dispatch**
   Add a case to the `render_widget()` switch in mkgui.c that calls
   `render_yourwidget(ctx, idx)`.

4. **Natural size**
   If the widget has a text-derived or font-derived natural height, add a case
   to `natural_height()` in mkgui.c. If it has a font-derived natural width for
   leaf widgets (label, button, checkbox, etc.), update `natural_width()` as
   well.

5. **Aux data initialization**
   If the widget has aux data, add a case to `init_widget_aux()` in mkgui.c
   that allocates and initializes the aux struct. Add a matching cleanup case
   to the per-type aux removal path so `mkgui_remove_widget` frees any
   allocations.

6. **Event dispatch**
   Wire up mouse press/release/motion, keyboard input, focus, and hover to
   produce the widget's events. Most widgets hook into the existing dispatch
   functions in mkgui.c; complex widgets can forward to helpers in their own
   `.c` file.

7. **Dirty tracking**
   Widget setters that mutate visible state should call `dirty_widget_id(ctx,
   id)` so the widget is redrawn without a full-screen refresh.

## editor.c - visual editor

All of these are required for the widget to appear in the editor and for code
generation to work correctly.

1. **Palette** (`ed_widgets[]` or `ed_containers[]`)
   Add `{ "YourWidget", MKGUI_YOURWIDGET }`. Use `ed_containers[]` only if the
   widget can host children; otherwise use `ed_widgets[]`.

2. **Event mapping** (`ed_type_events[]`)
   Map the widget type to the list of events it can emit:
   `{ MKGUI_YOURWIDGET, { MKGUI_EVENT_YOURWIDGET_CHANGED }, 1 }`.
   Only required if the widget fires events the user would handle.

3. **Event name switch** (`ed_event_name()`)
   Add a case for each new event returning its string name:
   `case MKGUI_EVENT_YOURWIDGET_CHANGED: { return "YOURWIDGET_CHANGED"; }`.

4. **Event name reverse lookup** (`ed_event_from_name()`)
   Add the reverse mapping so loaded project files resolve event names back to
   constants.

5. **ID prefix** (`ed_gen_id_name()`)
   Add the ID prefix for the widget type so default IDs look like
   `ID_YOURWIDGET_1`:
   `case MKGUI_YOURWIDGET: { prefix = "ID_YOURWIDGET"; } break;`

6. **Default dimensions** (`ed_add_widget()`)
   Set sensible default width/height and any required label for the new
   widget when it is placed from the palette.

7. **Type name for code generation** (`ed_type_name_upper()`)
   Return the enum constant as a string for code generation:
   `case MKGUI_YOURWIDGET: { return "MKGUI_YOURWIDGET"; }`.

8. **Code generation setup** (`ed_generate_code()`)
   If the widget needs setup code beyond the widget array (e.g. populating
   items, setting a range, installing a callback), add a case in the code
   generation section that emits the setup call.

9. **Editor canvas rendering**
   - If the widget can render using the real `render_widget()` with no aux
     data, add its type to the `ed_real_render` bitfield in editor.c.
   - Otherwise add a case to `ed_draw_widget_fallback()` that draws a
     simplified preview.

10. **Data panel** (`ed_draw_data_ui()`)
    If the widget has pre-populated items (like dropdown, combobox, statusbar
    sections), extend the data editing UI to include it.

## documentation/doc.md

Add the widget to the widget reference section with its events, style flags,
and public API. Include a minimal `MKGUI_W(...)` example and describe any
required setup calls.

## Quick sanity check

After adding everything, verify:

- The widget appears in the editor palette.
- You can drag it onto the canvas and the preview renders correctly.
- Its events appear in the event picker panel.
- Saving and reloading an editor project preserves the widget.
- Code generation produces a compilable output file that actually uses the
  widget (including any setup calls).
- `./build.sh` completes with zero warnings.
- `out/test_layout` and `out/test_widgets` still pass.
