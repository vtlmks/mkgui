# Adding a new widget to mkgui

Checklist of every location that must be updated when adding a new widget type.
Forgetting any of these will cause the widget to be missing from the editor
palette, code generation, or event handling.

## mkgui.c - core library

1. **Widget type enum** (~line 156)
   Add `MKGUI_YOURWIDGET` to the enum near the other widget types.

2. **Event type enum** (~line 238)
   Add any new events for the widget (e.g. `MKGUI_EVENT_YOURWIDGET_CHANGED`).

3. **Widget name string** in `mkgui_widget_type_name()` switch
   Return the human-readable name for the new type.

4. **Default size** in `mkgui_add_widget()` switch
   Set default w/h for the widget type.

5. **Rendering** in `mkgui_draw_widget()` switch
   Add the rendering case. For complex widgets, put the rendering code in a
   separate `mkgui_yourwidget.c` include file and call from here.

6. **Event handling** in `mkgui_process_event()` / input handling
   Wire up mouse/keyboard input to produce the widget's events.

7. **Aux data** (if needed)
   If the widget has per-instance state beyond the base `struct mkgui_widget`,
   add a struct (e.g. `struct mkgui_yourwidget_data`) and use the aux data
   system (`find_*_data()` pattern) to allocate/find it.

8. **Public API functions** (if needed)
   Add `mkgui_yourwidget_set()`, `mkgui_yourwidget_get()`, etc. as needed.

## editor.c - visual editor

All of these are required for the widget to appear in the editor and for code
generation to work correctly.

1. **`ed_widgets[]` palette array** (~line 112)
   Add `{ "YourWidget", MKGUI_YOURWIDGET }` in alphabetical order.
   If it is a container, add it to `ed_containers[]` instead.

2. **`ed_type_events[]` array** (~line 167)
   Map the widget type to its events and event count:
   `{ MKGUI_YOURWIDGET, { MKGUI_EVENT_YOURWIDGET_CHANGED }, 1 }`
   Only needed if the widget fires events the user would handle.

3. **`ed_event_name()` switch** (~line 203)
   Add a case for each new event returning its string name:
   `case MKGUI_EVENT_YOURWIDGET_CHANGED: { return "YOURWIDGET_CHANGED"; }`

4. **`ed_event_from_name()` map array** (~line 240)
   Add the reverse mapping for each new event:
   `{ "YOURWIDGET_CHANGED", MKGUI_EVENT_YOURWIDGET_CHANGED }`

5. **`ed_gen_id_name()` switch** (~line 1152)
   Add the ID prefix for the widget type:
   `case MKGUI_YOURWIDGET: { prefix = "ID_YOURWIDGET"; } break;`

6. **`ed_add_widget()` switch** (~line 1198)
   Set default dimensions and label for the new widget:
   ```
   case MKGUI_YOURWIDGET: {
       w->w = 150;
       w->h = 24;
   } break;
   ```

7. **`ed_type_name_upper()` switch** (~line 3564)
   Return the enum constant as a string for code generation:
   `case MKGUI_YOURWIDGET: { return "MKGUI_YOURWIDGET"; }`

8. **`ed_generate_code()` special handling** (~line 3607, only if needed)
   If the widget needs setup code beyond `mkgui_add_widget()` (e.g. populating
   items for a dropdown/combobox, setting min/max for a slider), add a case in
   the code generation section.

9. **Data panel in `ed_draw_data_ui()`** (only if needed)
   If the widget has pre-populated items (like dropdown/combobox), add its type
   to the check that gates the data editing UI.

## documentation/doc.md

Add the widget to the widget reference section with its events and API.

## Quick sanity check

After adding everything, verify:
- The widget appears in the editor palette
- You can drag it onto the canvas
- Its events appear in the event picker panel
- Code generation produces correct output including setup code
- The generated code compiles
