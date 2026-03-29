# mkgui architecture

Internal reference for the structure, subsystems, and data flow of mkgui.

## Build model

Unity build. A single `.c` file (your application) includes `mkgui.c`, which in turn `#include`s every other `.c` file. All functions are `static` (or `MKGUI_API static`). There is no linker step between mkgui and the application -- everything compiles as one translation unit.

For shared library use, define `MKGUI_LIBRARY` to make `MKGUI_API` expand to nothing (external linkage).

### Include order inside mkgui.c

```
mkgui.c
  mkgui.h                  Public API (types, enums, function prototypes)
  platform_win32_types.h    Platform struct definitions (Win32)
  platform_linux_types.h    Platform struct definitions (X11/FreeType)
  mkgui_render.c            Drawing primitives, text command queue
  platform_win32.c          Window, events, fonts, framebuffer (Win32)
  platform_linux.c          Window, events, fonts, framebuffer (X11)
  mkgui_icon.c              Icon loading, hashing, rendering
  mkgui_button.c            Widget implementations (35 files)
  mkgui_label.c
  ...
  mkgui_tooltip.c
  mkgui_dialogs.c           Built-in dialogs (message box, file dialog)
  mkgui_filedialog.c
  mkgui_iconbrowser.c
```

### Targets

Linux (X11, GCC) and Windows (Win32, MinGW cross-compile). Both are first-class. The build script compiles both in parallel when a MinGW cross-compiler is available.

Dependencies: FreeType2, Xlib + XShm (Linux), GDI (Windows), OpenGL (optional, for MKGUI_GLVIEW).

## File map

| File | Role |
|------|------|
| `mkgui.h` | Public API. All types, enums, and function prototypes. |
| `mkgui.c` | Core: context lifecycle, widget management, layout engine, event dispatch, main loop. ~6200 lines. |
| `mkgui_render.c` | Drawing primitives (`draw_rect_fill`, `draw_rounded_rect`, `draw_text_sw`), text command queue, icon drawing. |
| `platform_linux.c` | X11 backend: window creation, XShm framebuffer, event translation, FreeType font loading, timerfd. |
| `platform_linux_types.h` | Platform struct definitions for Linux (Display, Window, XShmSegmentInfo, FT_Face). |
| `platform_win32.c` | Win32 backend: window creation, DIB section framebuffer, message loop, font loading, waitable timers. |
| `platform_win32_types.h` | Platform struct definitions for Windows (HWND, HDC, HBITMAP, HFONT). |
| `mkgui_icon.c` | Icon hash table, pixel pool, MDI icon pack loading, icon drawing with clipping. |
| `mkgui_*.c` (35 files) | One file per widget type. Each contains rendering, event handling, and public API for that widget. |
| `mkgui_dialogs.c` | Built-in message box dialog. |
| `mkgui_filedialog.c` | Built-in file open/save dialog. |
| `mkgui_iconbrowser.c` | Built-in icon browser dialog. |
| `incbin.h` | Helper macro for embedding binary data (icon packs). |
| `editor.c` | Visual widget designer (GadToolsBox-style). Includes `mkgui.c`. |
| `demo.c` | Comprehensive demo application. Includes `mkgui.c`. |
| `test_dynamic.c` | Stress tests for widget add/remove, boundary growth, aux data consistency. |
| `tools/gen_icons.c` | Offline tool: reads a TTF icon font and generates a binary icon pack (`.dat`). |

## Core data structures

### struct mkgui_ctx (the context)

Everything lives here. One context per window. Key members:

| Member | Purpose |
|--------|---------|
| `plat` | Platform-specific data (X11 Display/Window or HWND/HDC) |
| `pixels`, `win_w`, `win_h` | Framebuffer: 32-bit ARGB pixel array, window dimensions |
| `widgets`, `widget_count`, `widget_cap` | Flat array of widget definitions |
| `rects` | Parallel array of computed layout rectangles (output of layout pass) |
| `tooltip_texts` | Parallel array of tooltip strings (one per widget slot) |
| Aux arrays (`inputs`, `textareas`, `listvs`, ...) | Per-type auxiliary data with separate count/cap |
| `popups[8]` | Active popup windows (dropdowns, context menus, tooltips, file dialogs) |
| `glyphs[224]` | Cached font glyphs (Latin-1: codepoints 32-255) |
| `theme` | Current color theme |
| `hover_id`, `press_id`, `focus_id` | Interaction state |
| `dirty_rects[32]`, `dirty_count`, `dirty_full` | Dirty region tracking |
| `timers[16]` | Active timers (timerfd on Linux, WaitableTimer on Windows) |
| `parent` | Pointer to parent context (for child windows) |

### struct mkgui_widget

Flat, value-type definition of a widget:

```
type, id, label[256], icon[64], parent_id, w, h, flags, weight, margin_l, margin_r, margin_t, margin_b
```

There are no `x, y` fields. All positioning is container-based (VBOX, HBOX, FORM, PANEL acts as VBOX). The `w` and `h` fields specify size; when `h` is 0, the widget uses its natural height derived from the font height. The actual computed position lives in the parallel `rects[]` array.

### Auxiliary data (the aux system)

Many widget types need per-instance state beyond what `struct mkgui_widget` holds (cursor position, scroll offset, selected items, etc.). These are stored in separate typed arrays on the context:

```
ctx->inputs[]       -- cursor, selection, scroll for MKGUI_INPUT
ctx->textareas[]    -- text buffer, cursor, selection for MKGUI_TEXTAREA
ctx->listvs[]       -- columns, row callback, selection for MKGUI_LISTVIEW
ctx->treeviews[]    -- node array, expanded state for MKGUI_TREEVIEW
...                 (22 aux types total)
```

Each array grows independently via `MKGUI_AUX_GROW` (realloc +16 entries). Lookup is via `find_*_data(ctx, widget_id)` which does a linear scan of the aux array (fast because counts are small -- typically single digits).

Aux data is created automatically by `init_widget_aux()` when a widget is added, and freed by `mkgui_remove_aux_*()` when removed.

## Memory management

### Virtual memory arenas (vm_arena)

The hot-path allocations (text rendering commands and layout tree arrays) use a reserve-commit virtual memory model instead of realloc. This avoids pointer invalidation and eliminates realloc from the render and layout paths.

```c
struct vm_arena {
    uint8_t *base;       // fixed address, never moves
    size_t reserved;     // virtual address space reserved
    size_t committed;    // pages actually backed by physical memory
};
```

| Platform | Reserve | Commit | Release |
|----------|---------|--------|---------|
| Linux | `mmap(PROT_NONE, MAP_PRIVATE \| MAP_ANONYMOUS)` | `mprotect(PROT_READ \| PROT_WRITE)` | `munmap` |
| Windows | `VirtualAlloc(MEM_RESERVE, PAGE_NOACCESS)` | `VirtualAlloc(MEM_COMMIT, PAGE_READWRITE)` | `VirtualFree(MEM_RELEASE)` |

The base pointer never changes. Pages are committed on demand as the arrays grow. Uncommitted pages cost only virtual address space (not RAM).

### What uses vm_arena

| Arena | What it holds | Max entries | Virtual reservation |
|-------|---------------|-------------|---------------------|
| `text_cmd_arena` | Text render commands (per-frame) | 32,768 | ~9 MB |
| `layout_parent_arena` | Widget parent index | 65,536 | ~256 KB |
| `layout_child_arena` | First-child index | 65,536 | ~256 KB |
| `layout_sibling_arena` | Next-sibling index | 65,536 | ~256 KB |
| `layout_hash_arena` | Widget ID hash table | 131,072 | ~1 MB |

These are static globals (shared across all contexts). Initialized on first `mkgui_create()`, destroyed on last `mkgui_destroy()`.

The limits are `#define`s at the top of the arena section in mkgui.c:

```c
#define MKGUI_VM_MAX_TEXT_CMDS   32768
#define MKGUI_VM_MAX_WIDGETS     65536
#define MKGUI_VM_MAX_HASH_SLOTS  (MKGUI_VM_MAX_WIDGETS * 2)
```

### What still uses malloc/realloc

- **Widget arrays** (`ctx->widgets`, `ctx->rects`, `ctx->tooltip_texts`): grow via `mkgui_grow_widgets()` (+256 entries). Cold path, only during `mkgui_add_widget`.
- **Aux data arrays**: grow via `MKGUI_AUX_GROW` (+16 entries). Cold path.
- **Textarea text buffers**: per-instance, exponential doubling. Appropriate for small per-widget buffers.
- **Treeview node arrays**: per-instance, exponential doubling.
- **Image/icon pixel buffers**: one-time allocation.

### Framebuffer

The main framebuffer (`ctx->pixels`) is a 32-bit ARGB pixel array, `win_w * win_h * 4` bytes. Allocated via XShm shared memory on Linux (for zero-copy blit to X11) or a DIB section on Windows. Reallocated on window resize.

Each popup has its own pixel buffer, separately allocated.

## Rendering pipeline

Every frame follows the same sequence inside `mkgui_flush()`:

```
1. layout_widgets(ctx)     -- recompute positions into ctx->rects[]
2. render_widgets(ctx)     -- draw all visible widgets into ctx->pixels
3. flush_text(ctx)         -- batch-render queued text commands
4. render_tooltip(ctx)     -- draw tooltip overlay
5. flush_text(ctx)         -- render tooltip text
6. platform_blit(ctx)      -- copy ctx->pixels to the screen
7. render/blit each popup  -- popups have independent pixel buffers
```

### Layout phase

`layout_widgets()` rebuilds the widget tree every frame:

1. **Build index** (`layout_build_index`): populate parent/child/sibling arrays and the ID hash table from the flat widget array. Uses vm_arena memory.
2. **Measure** (`lc_measure_container`): bottom-up pass computing minimum sizes for containers (VBOX, HBOX, FORM, GROUP, TABS).
3. **Position** (`lc_layout_node`): top-down recursive pass. Each widget's final rect is computed from its parent's rect using container-based layout (VBOX, HBOX, FORM stacking), weights, and measured sizes. Special handling for menus, toolbars, statusbars, pathbars (auto-positioned window chrome), splits (draggable ratio), and tabs (only active tab children laid out). PANEL behaves as a VBOX container. Widgets with `h=0` use a natural height derived from the font height.
4. Results stored in `ctx->rects[]` (parallel to `ctx->widgets[]`).

### layout_ctx -- shared layout abstraction

The layout engine (measure, build index, position) operates on a `struct layout_ctx` instead of reading directly from `struct mkgui_ctx`. This allows the same layout code to be used by both the runtime and the visual editor.

```c
struct layout_ctx {
    struct mkgui_widget *widgets;     // widget array (may be ed_widget cast)
    struct { int32_t x, y, w, h; } *rects;  // output rectangles
    uint32_t *first_child;            // tree index arrays
    uint32_t *next_sibling;
    uint32_t *parent;
    struct mkgui_layout_hash_entry *id_map;  // ID -> index hash table
    uint32_t hash_mask;
    uint32_t widget_count;
    uint32_t has_scroll;              // enable scroll measurement
    size_t widget_stride;             // sizeof one widget entry
};
```

The key field is `widget_stride`. At runtime this is `sizeof(struct mkgui_widget)`. In the editor it is `sizeof(struct ed_widget)` (which extends `mkgui_widget` with editor-specific fields). `lw_get(lc, idx)` uses stride-based pointer arithmetic to index into the widget array, so the layout code handles both struct sizes transparently.

**Runtime path:** `layout_build_index()`, `measure_container()`, and `layout_node()` are thin wrappers that construct a `layout_ctx` from `ctx->widgets` and the global vm_arena arrays, then call the `lc_*` functions.

**Editor path:** `ed_layout_build_index()` and `ed_layout_preview()` construct a `layout_ctx` from the editor's own `ed.widgets[]` and static arrays (`ed_layout_parent[]`, `ed_layout_first_child[]`, etc.), then call the same `lc_build_index()` and `lc_layout_node()`. The editor's layout arrays are fixed-size (`ED_MAX_WIDGETS = 512`) since the editor doesn't need to handle thousands of widgets.

### Render phase

`render_widgets()` walks widgets in order (which gives correct painter's-order depth):

1. Set clip rectangle to parent bounds via `set_parent_clip()`.
2. Skip hidden widgets and inactive tab children.
3. Dispatch to type-specific renderer (big switch on `w->type`).
4. Each renderer draws into `ctx->pixels` using primitives from `mkgui_render.c`.

### Text rendering

Text is not drawn immediately. Instead, `push_text_clip()` queues a text command (position, string, color, clip rect) into the `text_cmds` array (vm_arena backed). `flush_text()` then renders all queued commands in one batch via `draw_text_sw()`.

`draw_text_sw()` iterates characters, looks up cached glyphs, and alpha-blends each glyph bitmap into the pixel buffer. Clipping is per-glyph.

### Dirty tracking

Widgets mark regions dirty via `dirty_add()`, `dirty_widget()`, `dirty_widget_id()`, or `dirty_all()`. The dirty rect array (max 32 rects) enables partial redraws and partial blits. Adjacent dirty rects are merged when their union area is within 1.5x the sum of individual areas to avoid overdraw. If the array overflows (32+ rects), it falls back to full-screen redraw via `dirty_all()`.

Each dirty rect is processed independently in `render_widgets()`: the clip region is set to the rect bounds, the rect is cleared to the background color, all widgets overlapping that rect are rendered clipped to it, and the rect is blitted to screen via `platform_blit_region()`. This per-rect approach avoids stale pixels between non-adjacent dirty rects.

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

### Dispatch (mkgui_poll)

`mkgui_poll()` is the main event processing function. Each call:

1. Update animation time.
2. Fire expired timer callbacks.
3. Process focus/hover state changes (emit `MKGUI_EVENT_FOCUS`, `MKGUI_EVENT_UNFOCUS`, `MKGUI_EVENT_HOVER_ENTER`, `MKGUI_EVENT_HOVER_LEAVE`).
4. Dequeue one platform event and dispatch:
   - **Mouse motion**: hit-test to find widget under cursor, update `hover_id`, handle drag operations (splits, listview column resize, scrollbar thumb, etc.).
   - **Button press**: set `press_id`, delegate to widget-specific press handler.
   - **Button release**: generate click events, delegate to widget-specific release handler.
   - **Key press**: route to focused widget's key handler (text input, arrow navigation, etc.).
   - **Resize**: reallocate framebuffer, re-layout.
   - **Close**: set `close_requested`.
5. Return the resulting `struct mkgui_event` (type, id, value) to the application.

### Application event loop

```c
// Simple loop (mkgui_run does this internally):
while(!ctx->close_requested) {
    struct mkgui_event ev;
    while(mkgui_poll(ctx, &ev)) {
        // handle ev
    }
    mkgui_wait(ctx);  // flush + block until next event or timer
}
```

`mkgui_wait()` calls `mkgui_flush()` then blocks on `platform_wait_event()` (poll/select on Linux, MsgWaitForMultipleObjects on Windows).

## Platform abstraction

Both platforms implement the same set of `platform_*()` functions. The correct implementation is selected at compile time via `#ifdef _WIN32`.

| Function | Purpose |
|----------|---------|
| `platform_init()` | Create window, allocate framebuffer, load cursors |
| `platform_init_child()` | Create transient child window linked to parent |
| `platform_destroy()` | Close window, free resources |
| `platform_fb_create()` | Allocate shared-memory framebuffer (XShm / DIB) |
| `platform_fb_resize()` | Reallocate framebuffer on window resize |
| `platform_fb_destroy()` | Free framebuffer |
| `platform_blit()` | Copy full framebuffer to screen |
| `platform_blit_region()` | Copy a sub-rectangle to screen |
| `platform_popup_init()` | Create override-redirect popup window |
| `platform_popup_fini()` | Destroy popup window |
| `platform_popup_blit()` | Blit popup pixel buffer |
| `platform_wait_event()` | Block until event available (with timeout) |
| `platform_pending()` | Check if events are queued |
| `platform_next_event()` | Dequeue and translate one event |
| `platform_translate_coords()` | Local to screen coordinate conversion |
| `platform_screen_size()` | Get desktop dimensions |
| `platform_set_cursor()` | Switch cursor (default, h-resize, etc.) |
| `platform_font_init()` | Load TTF font, rasterize glyphs |
| `platform_font_fini()` | Free font resources |
| `platform_clipboard_set()` | Set clipboard content |
| `platform_clipboard_get_alloc()` | Get clipboard content (caller frees) |
| `platform_glview_create/destroy/show/reposition()` | GL viewport child window management |

## Icon system

Icons are stored in a global hash table (`icons[MKGUI_MAX_ICONS]`, max 8192) with a pre-allocated pixel pool. Each icon is `MKGUI_ICON_SIZE` x `MKGUI_ICON_SIZE` pixels (18x18 by default).

Icon packs are generated offline by `tools/gen_icons.c` from a TTF icon font (Material Design Icons). The binary `.dat` file is embedded into the application via `incbin.h` and loaded at startup with `mkgui_set_icon_data()`.

Lookup by name uses FNV-1a hashing with linear probing. Drawing is done by `draw_icon()` / `draw_icon_popup()` with per-pixel alpha blending and clip rect support.

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

`mkgui_create_child()` creates a second context with its own window, framebuffer, and widget tree. The child shares the parent's font data, icon packs, and display connection. It is marked as a transient window of the parent (window manager handles stacking/modality).

A deferred event queue handles events for non-focused windows. Each context is registered in a global `window_registry[]` (max 16 windows).

## Widget lifecycle

1. **Define**: create a `struct mkgui_widget` with type, id, parent_id, size, flags.
2. **Add**: `mkgui_add_widget(ctx, widget, after_id)` inserts into the flat array. If the widget type has aux data, `init_widget_aux()` allocates and initializes it.
3. **Layout**: next `mkgui_flush()` recomputes `ctx->rects[]` for all widgets.
4. **Render**: type-specific renderer draws the widget into the framebuffer.
5. **Interact**: platform events are dispatched; widget-specific handlers update state and emit `mkgui_event`s.
6. **Remove**: `mkgui_remove_widget(ctx, id)` removes the widget and all descendants, frees aux data, compacts the arrays.

## Performance notes

- **SSE2**: `draw_rect_fill` uses SSE2 128-bit stores for fast rectangle clearing.
- **Glyph cache**: all 224 Latin-1 glyphs (codepoints 32-255) are pre-rasterized at startup. No FreeType calls during rendering. Text input accepts UTF-8; codepoints within Latin-1 range are rendered from cache, others are skipped.
- **Virtual callbacks**: listview, gridview, and richlist fetch row data on demand via callbacks, supporting millions of rows without pre-allocating data.
- **Deferred text rendering**: text commands are batched and flushed once per frame (or per popup), not per-widget.
- **vm_arena**: layout and text command arrays never realloc during rendering. Pointer stability guaranteed.

## Limits

| Resource | Maximum | Defined by |
|----------|---------|------------|
| Widgets (layout) | 65,536 | `MKGUI_VM_MAX_WIDGETS` |
| Text commands/frame | 32,768 | `MKGUI_VM_MAX_TEXT_CMDS` |
| Icons | 8,192 | `MKGUI_MAX_ICONS` |
| Popups | 8 | `MKGUI_MAX_POPUPS` |
| Windows | 16 | `MKGUI_MAX_WINDOWS` |
| Timers | 16 | `MKGUI_MAX_TIMERS` |
| Dirty rects | 32 | Fixed array on ctx |
| Widget label length | 256 chars | `MKGUI_MAX_TEXT` |
| Icon name length | 64 chars | `MKGUI_ICON_NAME_LEN` |
| Context menu items | 64 | `MKGUI_MAX_CTXMENU` |
| Clipboard | 4,096 bytes | `MKGUI_CLIP_MAX` |
