# mkgui architecture

Internal reference for the structure, subsystems, and data flow of mkgui.

## Build model

The toolkit is compiled as one translation unit, `mkgui.c`, which includes the
widget implementations and platform backend. `build.sh` builds Linux shared and
static libraries and, when MinGW is available, the Windows DLL and static archive.
The demo and editor link the shared library.

`MKGUI_LIBRARY` enables exported public definitions. Windows uses explicit
`dllexport`/`dllimport`; ELF uses default visibility for the public API and hidden
visibility for everything else. `MKGUI_STATIC` selects ordinary external linkage
for static archive consumers. Including `mkgui.c` directly still gives a unity
build with static functions.

`mkgui_internal.h` contains private types shared with the editor. The editor
obtains drawing/layout functions and shared renderer state through
`mkgui_editor_api_v2()`, a private bridge with its own revision. Changes to the
internal layouts or bridge table require a new private bridge revision and a
matching editor rebuild. Applications use only the opaque public types in
`mkgui.h`. Private headers are not installed.

`mkgui.version` controls the complete Linux filename and SONAME. Version 1.0.0 installs as `libmkgui.so.1.0.0`, with a release-specific development symlink `libmkgui1.0.0.so`, archive `libmkgui1.0.0.a`, headers in `include/mkgui1.0.0`, and pkg-config package `mkgui1.0.0`. Every full release can coexist with the others.
Windows uses `mkgui.dll` and a `libmkgui.dll.a` import library. The independently
versioned icon library remains a dependency of the toolkit.

### Include order inside mkgui.c

```
mkgui.c
  mkgui.h                  Public API (types, enums, function prototypes)
  mkgui_internal.h         Private toolkit/editor types
  mkwin/mkwin.h            Windowing API
  mkwin_types_win32.h      Platform struct definitions (Win32)
  mkwin_types_linux.h      Platform struct definitions (X11/FreeType/Fontconfig)
  plutovg/plutosvg          Embedded SVG rasterizer (PlutoVG + PlutoSVG, MIT)
  mkgui_render.c            Drawing primitives, text command queue
  mkgui_drop.c              OS file drag-and-drop
  mkwin_win32.c            Window, events, fonts, framebuffer (Win32)
  mkwin_linux.c            Window, events, fonts, framebuffer (X11)
  mkgui_icon.c              Icon loading (SVG via PlutoSVG), hashing, rendering
  mkgui_textedit.c          Shared text-edit primitives used by input/combobox/pathbar/cell edit
  mkgui_undo.c              Undo/redo ring buffers
  mkgui_button.c            Widget implementations (one .c per widget)
  mkgui_label.c
  ...
  mkgui_celledit.c          Inline cell edit overlay (F2 rename)
  mkgui_tooltip.c
  mkgui_dialogs.c           Message box, confirm, input dialogs
  mkgui_filedialog.c        Open/save file dialog
  mkgui_iconbrowser.c       Shared Fluent icon browser
  mkgui_colorpicker.c       Color picker dialog
```

### Targets

Linux (X11, GCC) and Windows (Win32, MinGW cross-compile). Both are first-class. The build script compiles both in parallel when a MinGW cross-compiler is available.

Dependencies:
- Linux: FreeType2, Fontconfig, Xlib + XShm.
- Windows: GDI (font rendering, framebuffer blit).
- OpenGL is optional and only required when an application actually instantiates a `MKGUI_GLVIEW` widget. `libmkgui.so.1.0.0`, the editor, and all other widgets build and run without it.

SVG icons use PlutoSVG to parse the document and PlutoVG to rasterize paths, strokes, gradients, and transparency before atlas insertion. The unity build defines `PLUTOVG_NO_FONT` and `PLUTOVG_NO_IMAGE_IO`: PlutoVG's font implementation, stb_truetype, stb_image, and stb_image_write are excluded from compilation. Widget text uses the platform font backend. The font option is a local addition to the vendored PlutoVG canvas, internal structures, and declarations.

### Widget exclusions

`mkgui_features.h` maps optional `MKGUI_NO_<WIDGET>` definitions to internal
`MKGUI_HAS_<WIDGET>` constants. No exclusions means all widget types are
enabled. The window type always remains available, and enum values and public
structures do not change between configurations.

`MKGUI_HAS_ALL` clears all known widget and dialog exclusions at the start of
feature resolution. No special handling is required in widget implementations.

Dispatch entries, layout branches, auxiliary initialization, event paths, and
teardown are specialized by those constants. Disabled auxiliary arrays acquire
no arenas. Both initial window creation and dynamic insertion reject excluded
widget types. Built-in dialogs are compiled only when their widget dependencies
are available and their own exclusion is absent: `MKGUI_NO_DIALOGS`,
`MKGUI_NO_FILEDIALOG`, `MKGUI_NO_COLORPICKER`, and `MKGUI_NO_ICONBROWSER`.

The editor exports project-wide exclusions and closes dependencies for selected
dialogs, tab/menu children, toolbar buttons, and explicit dynamic widget types.
Compile the toolkit with the generated header as well as the consuming source
when using a static library. Function sections and linker garbage collection
remain useful with `-O2` or other application optimization settings.

## File map

| File | Role |
|------|------|
| `mkgui.h` | Public API. All types, enums, and function prototypes. Single source of truth for consumers. |
| `mkgui.c` | Core: context lifecycle, widget management, layout engine, event dispatch, main loop. Includes every other `.c` file. |
| `mkgui_render.c` | Drawing primitives (`draw_rect_fill`, `draw_rounded_rect`, `draw_text_sw`), text command queue, icon drawing. |
| `mkwin/mkwin_linux.c` | X11 backend: window creation, XShm framebuffer, event translation, FreeType + Fontconfig font loading, timerfd, XDnd. |
| `mkwin/mkwin_types_linux.h` | Platform struct definitions for Linux. |
| `mkwin/mkwin_win32.c` | Win32 backend: window creation, DIB section framebuffer, message loop, GDI font loading, waitable timers, WM_DROPFILES. |
| `mkwin/mkwin_types_win32.h` | Platform struct definitions for Windows. |
| `mkgui_icon.c` | Icon hash table, SVG source cache, PlutoSVG rasterization, shared catalogue lookup, icon drawing with clipping. |
| `mkgui_textedit.c` | Shared text-edit primitives (cursor movement, selection, insert/delete, render) used by input/combobox/pathbar/cell edit. |
| `mkgui_celledit.c` | Inline cell edit overlay used by listview, treeview, gridview, and itemview. F2 / slow-double-click rename. |
| `mkgui_drop.c` | OS file drag-and-drop (XDnd on Linux, WM_DROPFILES on Windows). |
| `mkgui_*.c` (~50 files) | One file per widget type, add a handful of subsystems (accel, ctxmenu, fs, navigation, notify, etc.). Each contains rendering, event handling, and public API for that unit. |
| `mkgui_dialogs.c` | Built-in message box, confirm, and input dialogs. |
| `mkgui_filedialog.c` | Built-in file open/save dialog. |
| `mkgui_iconbrowser.c` | Built-in Shared Fluent icon browser. |
| `mkgui_colorpicker.c` | Built-in color picker dialog (SV square, wheel, RGB sliders). |
| `plutovg/`, `plutosvg/` | Embedded PlutoVG path renderer and PlutoSVG parser (MIT). Compiled into the unity build. |
| `editor.c` | Visual widget designer. Links the shared toolkit through its public API and private editor bridge. |
| `mkgui_internal.h`, `mkgui_editor.h`, `mkgui_editor_api.c` | Private toolkit/editor types and bridge. |
| `demo.c` | Comprehensive demo application. Includes `mkgui.h` and links the shared library. |
| `tests/test_layout.c` | Layout geometry unit tests. |
| `tests/test_widgets.c` | Widget behavior unit tests. |
| `tests/test_events.c` | Event dispatch tests. |
| `mkguiicons.c`, `mkguiicons_data.S` | Shared SVG catalogue and stable enumeration/lookup ABI. |
| `tools/generate_icon_library.py` | Generates catalogue tables, SVG blob, and public name macros from the vendored snapshot. |

## Core data structures

### struct mkgui_ctx

One context exists per process. It owns application defaults, the quit flag, the primary window, and a registry of up to `MKGUI_MAX_WINDOWS` windows.

| Member | Purpose |
|--------|---------|
| `windows`, `window_count` | Window registry |
| `primary` | First live window and main-loop anchor |
| `should_quit` | Application-wide quit flag |
| `app_class`, `theme`, `scale` | Defaults inherited by new windows |

### struct mkgui_window

Each window owns its platform handle, framebuffer, widget tree, per-type auxiliary arrays, layout rectangles, event state, timers, accelerators, popups, dirty rectangles, and rendering metrics. Its `ctx` member points back to the process context and `parent` identifies an optional transient parent window.

### struct mkgui_widget

Flat, value-type definition of a widget:

```
type, id, parent_id, w, h, flags, style, weight, margin_l, margin_r, margin_t, margin_b, label[256], icon[64]
```

There are no `x, y` fields. All positioning is container-based (VBOX, HBOX, FORM, PANEL acts as VBOX). The `w` and `h` fields specify size; when `h` is 0, the widget uses its natural height derived from the font height. The `flags` field holds universal layout/visibility flags; the `style` field holds per-widget-type style flags (each widget type has its own bit-0-based namespace). The actual computed position lives in the parallel `rects[]` array.

### Auxiliary data (the aux system)

Many widget types need per-instance state beyond what `struct mkgui_widget` holds (cursor position, scroll offset, selected items, etc.). These are stored in separate typed arrays on the window:

```
win->inputs[]       -- cursor, selection, scroll for MKGUI_INPUT
win->textareas[]    -- text buffer, cursor, selection for MKGUI_TEXTAREA
win->listvs[]       -- columns, row callback, selection for MKGUI_LISTVIEW
win->treeviews[]    -- node array, expanded state for MKGUI_TREEVIEW
...
```

Each array commits additional pages through `MKGUI_AUX_GROW` in `MKGUI_GROW_AUX` increments within its reserved arena. Its base pointer stays fixed. Lookup helpers such as `find_input_data(win, widget_id)` scan the corresponding auxiliary array.

Aux data is created automatically by `init_widget_aux()` when a widget is added, and freed by `mkgui_remove_aux_*()` when removed.

## Memory management

### Virtual memory arenas (vm_arena)

The hot-path allocations (text rendering commands and layout tree arrays) use a reserve-commit virtual memory model instead of realloc. This avoids pointer invalidation and eliminates realloc from the render and layout paths.

```c
struct vm_arena {
	uint8_t *base;
	size_t reserved;
	size_t committed;
};
```

| Platform | Reserve | Commit | Release |
|----------|---------|--------|---------|
| Linux | `mmap(PROT_NONE, MAP_PRIVATE \| MAP_ANONYMOUS)` | `mprotect(PROT_READ \| PROT_WRITE)` | `munmap` |
| Windows | `VirtualAlloc(MEM_RESERVE, PAGE_NOACCESS)` | `VirtualAlloc(MEM_COMMIT, PAGE_READWRITE)` | `VirtualFree(MEM_RELEASE)` |

Window and layout arrays keep a fixed reservation, so their base pointers never change. Pages are committed on demand as the arrays grow. Uncommitted pages cost only virtual address space. Icon metadata, lookup slots, custom SVG records, and the glyph atlas use growing reservations through `vm_arena_grow()`. When a reservation fills, a larger mapping replaces it; icon indices remain stable. The private editor bridge follows the current metadata base.

### What uses vm_arena

| Arena | What it holds | Max entries | Virtual reservation |
|-------|---------------|-------------|---------------------|
| `text_cmd_arena` | Text render commands (per-frame) | 32,768 | ~9 MB |
| `icon_arena` | Cached icon metadata | Grows to the signed index limit | Grows from 64 KiB |
| `icon_hash_arena` | Icon lookup slots | Grows with cached icons | Grows from 64 KiB |
| `svg_source_arena` | Custom SVG source records | Grows with loaded sources | Grows from 64 KiB |
| `glyph_arena` | Packed glyph coverage | 32-bit atlas offsets | Grows from 64 KiB |
| `layout_parent_arena` | Widget parent index | 1,048,576 | 4 MiB |
| `layout_child_arena` | First-child index | 1,048,576 | 4 MiB |
| `layout_sibling_arena` | Next-sibling index | 1,048,576 | 4 MiB |
| `layout_hash_arena` | Widget ID hash table | 2,097,152 | 16 MiB |

Window widget and auxiliary arenas belong to each window. Layout scratch, text commands, fonts, and icons use process-private globals shared by windows in that process. Loading the shared library in another process creates independent writable state; the library code and immutable catalogue can be shared. Icon arenas also initialize when icons are added before the first window. Last-window teardown releases them; context destruction also releases any preloaded icons when no window was opened.

The limits are `#define`s at the top of the arena section in mkgui.c:

```c
#define MKGUI_VM_MAX_TEXT_CMDS   32768
#define MKGUI_VM_MAX_WIDGETS     (1u << 20)
#define MKGUI_VM_MAX_HASH_SLOTS  (MKGUI_VM_MAX_WIDGETS * 2)
```

Glyphs are copied directly from the platform rasterizer into a replacement packed atlas. There is no fixed per-glyph staging slot. A successful build replaces the previous atlas; allocation failure preserves it. Last-window teardown releases the font atlas and all icon arenas.

### What still uses malloc/realloc

- **Textarea text buffers**: per-instance, exponential doubling. Appropriate for small per-widget buffers.
- **Treeview node arrays**: per-instance, exponential doubling.
- **Image/icon pixel buffers**: one-time allocation.

### Framebuffer

The active framebuffer (`win->pixels`) is a 32-bit ARGB pixel array, `win_w * win_h * 4` bytes. Linux alternates two XShm images, with completion events protecting each image from premature reuse. Windows uses a DIB section. Shared segments retain their capacity across resizes and grow when needed. Buffer acquisition refreshes `win->pixels` before drawing.

Each popup has its own pixel buffer, separately allocated. Linux popup presentation waits for its XShm completion before returning, so the next popup redraw can safely reuse that buffer.

Each window retains the previous frame's damage. When an acquired buffer is two frames old, rendering covers both the current and previous damage; an undefined buffer receives a full redraw. Only the current damage is submitted to X11. Repair damage is kept out of subsequent history, preventing it from accumulating across frames.

## Rendering pipeline

Every frame follows the same sequence inside `mkgui_flush()`:

```text
1. layout_widgets(win) and glview_sync_all(win)
2. mkgui_frame_begin(win): acquire a writable buffer and repair its damage history
3. render_widgets(win) and flush_text(win)
4. render callback, notifications, and tooltip, flushing queued text as needed
5. mkgui_frame_present(win): submit the current damage
6. render/present each popup
```

### Layout phase

`layout_widgets()` rebuilds the widget tree every frame:

1. **Build index** (`layout_build_index`): populate parent/child/sibling arrays and the ID hash table from the flat widget array. Uses vm_arena memory.
2. **Position and measure** (`lc_layout_node`, `lc_measure_container`): recursively position children, measuring their content minimums as needed during the walk. Container rules, weights, explicit sizes, and natural sizes determine the result. Menus, toolbars, statusbars, and pathbars occupy window chrome; splits use a divider ratio, and tabs lay out their active page.
3. Store results in `win->rects[]` (parallel to `win->widgets[]`). Zero size hints follow the parent container's flex rules and the widget's natural size.
4. **Window minimum** (`layout_compute_min_size`): update the native minimum size after structural changes, using the same calculation as the editor canvas.

### layout_ctx -- shared layout abstraction

The layout engine (measure, build index, position) operates on a `struct layout_ctx` instead of reading directly from `struct mkgui_window`. This allows the same layout code to be used by both the runtime and the visual editor.

```c
struct layout_ctx {
	struct mkgui_widget *widgets;
	struct mkgui_rect *rects;
	uint32_t *first_child;
	uint32_t *next_sibling;
	uint32_t *parent;
	struct mkgui_layout_hash_entry *id_map;
	uint32_t hash_mask;
	uint32_t widget_count;
	uint32_t has_scroll;
	size_t widget_stride;
};
```

The key field is `widget_stride`. At runtime this is `sizeof(struct mkgui_widget)`. In the editor it is `sizeof(struct ed_widget)` (which extends `mkgui_widget` with editor-specific fields). `lw_get(lc, idx)` uses stride-based pointer arithmetic to index into the widget array, so the layout code handles both struct sizes transparently.

**Runtime path:** `layout_build_index()` and `layout_node()` are thin wrappers that construct a `layout_ctx` from `win->widgets` and the global vm_arena arrays, then call the `lc_*` functions. `lc_window_min_size()` shares the content minimum calculation between the runtime and editor.

**Editor path:** `ed_fit_canvas_to_content()` and `ed_render_canvas()` prepare runtime widget descriptions and use the editor's static layout arrays with the same `lc_window_min_size()` and `lc_layout_node()` functions. The editor arrays have a fixed capacity of `ED_MAX_WIDGETS = 4096`.

### Render phase

`render_widgets()` walks widgets in order (which gives correct painter's-order depth):

1. Set clip rectangle to parent bounds via `set_parent_clip()`.
2. Skip hidden widgets and inactive tab children.
3. Dispatch through the type-indexed `render_desc[]` table.
4. Each renderer draws into `win->pixels` using primitives from `mkgui_render.c`.

### Text rendering

Text is not drawn immediately. Instead, `push_text_clip()` queues a text command (position, string, color, clip rect) into the `text_cmds` array (vm_arena backed). `flush_text()` then renders all queued commands in one batch via `draw_text_sw()`.

`draw_text_sw()` iterates characters, looks up cached glyphs, and alpha-blends each glyph bitmap into the pixel buffer. Clipping is per-glyph.

### Dirty tracking

Widgets mark regions dirty via `dirty_add()`, `dirty_widget()`, `dirty_widget_id()`, or `dirty_all()`. The dirty rect array (max 32 rects) enables partial redraws and partial blits. Adjacent dirty rects are merged when their union area is within 1.5x the sum of individual areas to avoid overdraw. If the array overflows (32+ rects), it falls back to full-screen redraw via `dirty_all()`.

Each dirty rect is processed independently in `render_widgets()`: the clip region is set to the rect bounds, the rect is cleared to the background color, and all widgets overlapping that rect are rendered clipped to it. `mkgui_frame_present()` submits the current frame's damage through `mkwin_present_region()`. Buffer repair regions are rendered but excluded from presentation history.

Most widget API setters (label, progress, meter, slider, textarea, etc.) and event dispatch hot paths (scroll, drag, selection) use `dirty_widget_id()` / `dirty_widget()` to only redraw the affected widget. Full-screen `dirty_all()` is reserved for events that affect the entire layout: window resize, expose, tab switch, focus change, widget add/remove, theme change, treeview expand/collapse.

## Event system

### Platform events

Each platform translates native events into a uniform `struct mkgui_plat_event`:

| Platform event | mkgui_plat_event type |
|----------------|----------------------|
| X11 Expose / WM_PAINT | `MKGUI_PLAT_EXPOSE` |
| X11 ConfigureNotify / WM_SIZE | `MKGUI_PLAT_RESIZE` |
| X11 MotionNotify / WM_MOUSEMOVE | `MKGUI_PLAT_MOTION` |
| X11 ButtonPress / WM_LBUTTONDOWN | `MKGUI_PLAT_BUTTON_PRESS` |
| X11 ButtonRelease / WM_LBUTTONUP | `MKGUI_PLAT_BUTTON_RELEASE` |
| X11 KeyPress / WM_KEYDOWN+WM_CHAR | `MKGUI_PLAT_KEY` |
| WM_DELETE_WINDOW / WM_CLOSE | `MKGUI_PLAT_CLOSE` |
| X11 LeaveNotify / WM_MOUSELEAVE | `MKGUI_PLAT_LEAVE` |
| X11 FocusOut / WM_KILLFOCUS | `MKGUI_PLAT_FOCUS_OUT` |

### Dispatch (internal mkgui_window_poll)

`mkgui_ctx_run()` provides the standard event loop. Hosts can also use the public `mkgui_window_poll()`, `mkgui_ctx_wait_until()`, and `mkgui_ctx_wait()` functions. Frame rendering through `mkgui_flush()` remains internal. Built-in dialogs use the same event and rendering paths.

`mkgui_window_poll()` does the following on each call:

1. Update animation time.
2. Fire expired timer callbacks.
3. Process focus/hover state changes (emit `MKGUI_EVENT_FOCUS`, `MKGUI_EVENT_UNFOCUS`, `MKGUI_EVENT_HOVER_ENTER`, `MKGUI_EVENT_HOVER_LEAVE`).
4. Dequeue one platform event and dispatch:
   - **Mouse motion**: hit-test to find widget under cursor, update `hover_id`, handle drag operations (splits, listview column resize, scrollbar thumb, etc.).
   - **Button press**: set `press_id`, delegate to widget-specific press handler.
   - **Button release**: generate click events, delegate to widget-specific release handler.
   - **Key press**: route to focused widget's key handler (text input, arrow navigation, etc.). Accelerators are matched after context menu keys but before Tab and clipboard shortcuts.
   - **Resize**: reallocate framebuffer, re-layout.
   - **Close**: set `should_close` on the window.
5. Return the resulting `struct mkgui_event` to the application via the registered callback.

### Application event loop

```c
// Public API: one function, no manual poll/wait:
mkgui_ctx_run(ctx, on_event, userdata);
```

`mkgui_ctx_run()` is the canonical event loop. Internally it is roughly:

```c
while(!ctx->should_quit && !ctx->primary->should_close) {
    struct mkgui_event ev;
    while(mkgui_window_poll(ctx->primary, &ev)) {
        cb(ctx->primary, &ev, userdata);
    }
    mkgui_ctx_pump_others(ctx);    // pump every non-primary window
    mkgui_ctx_wait(ctx);            // flush + block until next event or timer
}
```

`mkgui_ctx_wait()` flushes windows before waiting through mkwin. Linux uses `ppoll` for display/timer readiness; Windows uses `MsgWaitForMultipleObjects`. Registered callbacks and the context loop route events to secondary windows.

## Platform abstraction

mkwin implements the X11 and Win32 windowing backends. mkgui includes its
source from the pinned `mkwin/` submodule, selecting a backend with `_WIN32`.
It is compiled into the toolkit library and has no separate runtime library.

| Function | Purpose |
|----------|---------|
| `mkwin_open()`, `mkwin_map()`, `mkwin_destroy()` | Create, show, and destroy top-level/transient windows |
| `mkwin_begin_frame()` | Acquire a writable framebuffer and return its age |
| `mkwin_framebuffer()` | Get the current framebuffer pointer |
| `mkwin_present()`, `mkwin_present_region()` | Submit full or partial presentation |
| `mkwin_sync()` | Wait for outstanding server work |
| `mkwin_resize()`, `mkwin_set_min_size()` | Resize and publish the content minimum |
| `mkwin_popup_open()`, `mkwin_popup_present()`, `mkwin_popup_close()` | Popup window lifecycle and presentation |
| `mkwin_poll()`, `mkwin_pending()`, `mkwin_wait()` | Event delivery and waiting |
| `mkwin_translate_coords()`, `mkwin_screen_size()` | Coordinate and desktop queries |
| `mkwin_set_cursor()` | Select the platform cursor |
| `mkwin_font_open()`, `mkwin_font_set_size()`, `mkwin_font_glyph()`, `mkwin_font_close()` | Native font loading and glyph rasterization |
| `mkwin_clipboard_set()`, `mkwin_clipboard_get()` | Clipboard transfer |
| `mkwin_glview_create()`, `mkwin_glview_destroy()`, `mkwin_glview_show()`, `mkwin_glview_reposition()` | Native OpenGL child windows |

See [mkwin's API documentation](../mkwin/documentation/doc.md) for the complete
platform contract. For mkwin development, use its standalone repository and then
update mkgui's submodule pin to the adopted commit. A release's pin records the
revision required to build that release.

## Icon system

`libmkguiicons.so.1` / `mkguiicons.dll` provides immutable SVG data and catalogue
metadata through a small C ABI. Names are binary-searched; a preferred source
size selects the nearest available vector design. Generated tables and an
assembly `incbin` keep SVG bytes out of the C compilation unit. The library does
not render and has no dependency on mkgui or PlutoSVG. Monochrome fills are
normalized to `currentColor` in the generated data; vendored sources stay intact.

mkgui resolves names lazily and rasterizes SVGs at the exact requested pixel size
using PlutoSVG/PlutoVG. A hash of name and pixel size finds cached images in the
ARGB atlas. Scale and theme changes re-render these vectors. Explicit custom SVG
and raster registration remain available. Missing names use the placeholder.

The editor's browser enumerates the provider and renders visible thumbnails into
its own cache. Filtering supports names, upstream keywords, styles, and groups
formed from the first name component. Browser thumbnails do not consume the
runtime icon table. See [icons.md](icons.md) for the provider ABI.

## Popup system

Popups are lightweight platform windows (override-redirect on X11, WS_POPUP on Windows) used for:

- Dropdown lists
- Context menus and submenus
- Tooltips
- Datepicker calendar
- File dialog internals
- Combobox dropdown

Each popup has its own pixel buffer and is rendered/blitted independently. Max 8 concurrent popups (`MKGUI_MAX_POPUPS`). Submenus stack (each level is a new popup); `popup_destroy_from()` closes a popup and everything above it.

## Multi-window support

A single `mkgui_ctx` owns up to `MKGUI_MAX_WINDOWS` windows. Each window has its own X11/Win32 window, framebuffer, and widget tree; all windows on the ctx share the display connection, font data, and icon cache. Create additional windows with `mkgui_window_create(ctx, parent_win, ...)`; passing a non-NULL `parent` marks the new window as transient of that parent. Every window is destroyed with `mkgui_window_destroy()` regardless of whether it was top-level or transient.

Each window can register an event callback via `mkgui_window_set_callback()`. A global window registry maps platform window handles to their owning `mkgui_window`, so `mkgui_ctx_run()` routes events to the correct window without manual pumping. For events that arrive while another window's modal loop is running, the event is pushed onto the target window's deferred event queue and delivered on its next iteration.

## Widget lifecycle

1. **Define**: create a `struct mkgui_widget` with type, id, parent_id, size, flags.
2. **Add**: `mkgui_widget_add(win, widget, after_id)` inserts into the flat array. If the widget type has aux data, `init_widget_aux()` allocates and initializes it.
3. **Layout**: next `mkgui_flush()` recomputes `win->rects[]` for all widgets.
4. **Render**: type-specific renderer draws the widget into the framebuffer.
5. **Interact**: platform events are dispatched; widget-specific handlers update state and emit `mkgui_event`s.
6. **Remove**: `mkgui_widget_remove(win, id)` removes the widget and all descendants, frees aux data, compacts the arrays.

## Performance notes

- **SSE2**: `draw_rect_fill` uses SSE2 128-bit stores for fast rectangle clearing.
- **Glyph cache**: all 224 Latin-1 glyphs (codepoints 32-255) are pre-rasterized at startup. No FreeType calls occur during rendering. The glyph atlas covers Latin-1; input editing advances over UTF-8 sequences.
- **Virtual callbacks**: listview, gridview, and richlist fetch row data on demand via callbacks, supporting millions of rows without pre-allocating data.
- **Deferred text rendering**: text commands are batched and flushed once per frame (or per popup), not per-widget.
- **vm_arena**: layout and text command arrays never realloc during rendering. Pointer stability guaranteed.

## Limits

| Resource | Maximum | Defined by |
|----------|---------|------------|
| Windows per context | 16 | `MKGUI_MAX_WINDOWS` |
| Widgets per window | 1,048,576 | `MKGUI_VM_MAX_WIDGETS` |
| Text commands/frame | 32,768 | `MKGUI_VM_MAX_TEXT_CMDS` |
| Icons | Grows on demand | Stable signed indices |
| Popups | 8 | `MKGUI_MAX_POPUPS` |
| Timers per window | 8 | `MKGUI_MAX_TIMERS` |
| Accelerators per window | 64 | `MKGUI_MAX_ACCELS` |
| Dirty rects per window | 32 | Fixed array on window |
| Widget label length | 256 chars | `MKGUI_MAX_TEXT` |
| Icon name length | 64 chars | `MKGUI_ICON_NAME_LEN` |
| Context navigation items | 64 | `MKGUI_MAX_CTXMENU` |
| Statusbar sections | 8 | `MKGUI_MAX_STATUSBAR_SECTIONS` |
| Dropdown items | 64 | `MKGUI_MAX_DROPDOWN` |
| Pathbar segments | 64 | `MKGUI_PATHBAR_MAX_SEGS` |
| File drop items | 256 | `MKGUI_DROP_MAX` |
| Multi-selection rows | 4,096 | `MKGUI_MAX_MULTI_SEL` |
