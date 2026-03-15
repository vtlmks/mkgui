# mkgui

Minimal GUI toolkit for Linux (X11) and Windows (Win32) with software rendering. Single-file unity build, no third-party dependencies beyond FreeType2 and Xlib (Linux) or GDI (Windows).

## Building

mkgui uses a unity build. You compile a single `.c` file that `#include`s `mkgui.c`:

```bash
gcc -std=c99 -O2 -Wall -Wextra myapp.c -o myapp \
    $(pkg-config --cflags freetype2) -lX11 -lXext $(pkg-config --libs freetype2)
```

Or use the included `build.sh`:

```bash
./build.sh          # normal (debug symbols)
./build.sh release  # optimized, stripped
./build.sh debug    # -g -O0
```

Dependencies: FreeType2, X11, Xext (MIT-SHM) on Linux. GDI on Windows.

## Quick start

```c
#include "mkgui.c"

enum { ID_WIN = 0, ID_BTN = 1, ID_LBL = 2 };

int main(void) {
    struct mkgui_widget widgets[] = {
        { MKGUI_WINDOW, ID_WIN, "Hello",  "",  0,      0, 0, 400, 300, 0 },
        { MKGUI_BUTTON, ID_BTN, "Click",  "",  ID_WIN, 10, 10, 100, 28, 0 },
        { MKGUI_LABEL,  ID_LBL, "Ready",  "",  ID_WIN, 10, 50, 200, 24, 0 },
    };

    struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
    if(!ctx) return 1;

    struct mkgui_event ev;
    uint32_t running = 1;
    while(running) {
        while(mkgui_poll(ctx, &ev)) {
            if(ev.type == MKGUI_EVENT_CLOSE) running = 0;
            if(ev.type == MKGUI_EVENT_CLICK && ev.id == ID_BTN) {
                mkgui_label_set(ctx, ID_LBL, "Clicked!");
            }
        }
    }

    mkgui_destroy(ctx);
    return 0;
}
```

## Architecture

Your application is a single `.c` file that includes `mkgui.c`. All functions are `static` (unity build). You define widgets as a flat array of `struct mkgui_widget`, create a context, then poll for events in a loop.

Widget hierarchy is expressed via `parent_id`. Every widget has a unique integer `id`. The first widget must be `MKGUI_WINDOW` with `parent_id = 0`.

Multiple independent windows are supported by creating multiple `mkgui_ctx` instances.

## Widget definition

```c
struct mkgui_widget {
    uint32_t type;        // MKGUI_BUTTON, MKGUI_INPUT, etc.
    uint32_t id;          // unique identifier
    char     label[256];  // display text
    char     icon[64];    // icon name (MDI name, e.g. "folder-open")
    uint32_t parent_id;   // parent widget id
    int32_t  x, y, w, h; // position and size (see anchors)
    uint32_t flags;       // MKGUI_ANCHOR_*, MKGUI_CHECKED, etc.
};
```

## Widget types

| Type | Description |
|------|-------------|
| `MKGUI_WINDOW` | Root window. Must be first widget. |
| `MKGUI_BUTTON` | Push button. Emits `MKGUI_EVENT_CLICK`. |
| `MKGUI_LABEL` | Static text. |
| `MKGUI_INPUT` | Single-line text input. Emits `MKGUI_EVENT_INPUT_CHANGED`. |
| `MKGUI_TEXTAREA` | Multi-line text editor. Emits `MKGUI_EVENT_TEXTAREA_CHANGED`. |
| `MKGUI_CHECKBOX` | Toggle checkbox. Emits `MKGUI_EVENT_CHECKBOX_CHANGED`. |
| `MKGUI_RADIO` | Radio button (mutually exclusive within parent). Emits `MKGUI_EVENT_RADIO_CHANGED`. |
| `MKGUI_DROPDOWN` | Drop-down selector. Emits `MKGUI_EVENT_DROPDOWN_CHANGED`. |
| `MKGUI_SLIDER` | Horizontal slider. Emits `MKGUI_EVENT_SLIDER_CHANGED`. |
| `MKGUI_SPINBOX` | Numeric input with +/- buttons. Emits `MKGUI_EVENT_SPINBOX_CHANGED`. Click the text area to type directly, Enter to confirm (clears focus), Escape to cancel editing. |
| `MKGUI_PROGRESS` | Progress bar with animated shimmer effect. No events. |
| `MKGUI_SPINNER` | Animated spinning arc indicator. No events, no setup needed. |
| `MKGUI_LISTVIEW` | Scrollable multi-column list with virtual rows and per-column cell types. Emits `MKGUI_EVENT_LISTVIEW_SELECT`, `_DBLCLICK`, `_SORT`, `_COL_REORDER`, `_REORDER`. |
| `MKGUI_ITEMVIEW` | Multi-mode item view (icon, thumbnail, compact, detail). Emits `MKGUI_EVENT_ITEMVIEW_SELECT`, `_DBLCLICK`. |
| `MKGUI_TREEVIEW` | Hierarchical tree. Emits `MKGUI_EVENT_TREEVIEW_SELECT`, `_EXPAND`, `_COLLAPSE`. |
| `MKGUI_TABS` | Tab container. Children must be `MKGUI_TAB`. Emits `MKGUI_EVENT_TAB_CHANGED`. |
| `MKGUI_TAB` | Individual tab page (parent must be `MKGUI_TABS`). |
| `MKGUI_MENU` | Menu bar. Children are top-level `MKGUI_MENUITEM`. |
| `MKGUI_MENUITEM` | Menu entry. Parent is `MKGUI_MENU` or another `MKGUI_MENUITEM` (submenu). Leaf items emit `MKGUI_EVENT_MENU`. |
| `MKGUI_TOOLBAR` | Toolbar bar. Children are `MKGUI_BUTTON`. |
| `MKGUI_STATUSBAR` | Status bar with sections. |
| `MKGUI_HSPLIT` | Horizontal splitter. Children use `MKGUI_REGION_TOP`/`MKGUI_REGION_BOTTOM`. |
| `MKGUI_VSPLIT` | Vertical splitter. Children use `MKGUI_REGION_LEFT`/`MKGUI_REGION_RIGHT`. |
| `MKGUI_GROUP` | Group box container. Draws a thin rounded border with a label in the top edge. Children are inset inside the frame. |
| `MKGUI_PANEL` | Plain container. No border by default. Use `MKGUI_PANEL_BORDER` for a border, `MKGUI_PANEL_SUNKEN` for a recessed look. Children positioned relative to panel. |
| `MKGUI_SCROLLBAR` | Standalone scrollbar. Vertical by default, `MKGUI_SCROLLBAR_HORIZ` for horizontal. Emits `MKGUI_EVENT_SCROLL`. |
| `MKGUI_IMAGE` | Displays ARGB pixel data. Centered by default, `MKGUI_IMAGE_STRETCH` to fill. `MKGUI_PANEL_BORDER` for a border. |
| `MKGUI_GLVIEW` | OpenGL viewport. Creates a native child window for GL rendering. The user creates their own GL context. Automatically triggers 60fps redraws when visible. |
| `MKGUI_CANVAS` | Custom drawing area. Calls a user callback with clipping set to the widget rect. Supports `MKGUI_PANEL_BORDER`. |
| `MKGUI_VBOX` | Vertical box layout. Stacks children top-to-bottom. Children with `h=0` flex to fill remaining space. |
| `MKGUI_HBOX` | Horizontal box layout. Stacks children left-to-right. Children with `w=0` flex to fill remaining space. |
| `MKGUI_FORM` | Two-column form layout. Children are paired: odd=label, even=control. Label column auto-sizes to widest label. |

## Anchor / layout system

Widget `x, y, w, h` are interpreted differently depending on anchor flags:

| Flags | x | y | w | h |
|-------|---|---|---|---|
| None | absolute x | absolute y | width | height |
| `LEFT + RIGHT` | left margin | -- | right margin | -- |
| `RIGHT` only | right margin | -- | width | -- |
| `TOP + BOTTOM` | -- | top margin | -- | bottom margin |
| `BOTTOM` only | -- | bottom margin | -- | height |

Combine horizontal and vertical anchors freely. Common patterns:

```c
// Menu bar: fill width, fixed height at top
{ MKGUI_MENU, ..., "", "", 0, 0, 0, 22, MKGUI_ANCHOR_LEFT | MKGUI_ANCHOR_TOP | MKGUI_ANCHOR_RIGHT }

// Status bar: fill width, fixed height at bottom
{ MKGUI_STATUSBAR, ..., "", "", 0, 0, 0, 22, MKGUI_ANCHOR_LEFT | MKGUI_ANCHOR_BOTTOM | MKGUI_ANCHOR_RIGHT }

// Fill remaining area (below menu+toolbar, above statusbar)
{ MKGUI_TABS, ..., "", "", 0, 50, 0, 22, MKGUI_ANCHOR_LEFT | MKGUI_ANCHOR_TOP | MKGUI_ANCHOR_RIGHT | MKGUI_ANCHOR_BOTTOM }
```

## Flags

### Anchor flags

| Flag | Value | Description |
|------|-------|-------------|
| `MKGUI_ANCHOR_LEFT` | `1 << 0` | Pin/stretch to left edge |
| `MKGUI_ANCHOR_TOP` | `1 << 1` | Pin/stretch to top edge |
| `MKGUI_ANCHOR_RIGHT` | `1 << 2` | Pin/stretch to right edge |
| `MKGUI_ANCHOR_BOTTOM` | `1 << 3` | Pin/stretch to bottom edge |

### Splitter region flags

| Flag | Value | Description |
|------|-------|-------------|
| `MKGUI_REGION_TOP` | `1 << 4` | Top region of horizontal splitter |
| `MKGUI_REGION_BOTTOM` | `1 << 5` | Bottom region |
| `MKGUI_REGION_LEFT` | `1 << 6` | Left region of vertical splitter |
| `MKGUI_REGION_RIGHT` | `1 << 7` | Right region |

### State flags

| Flag | Value | Description |
|------|-------|-------------|
| `MKGUI_HIDDEN` | `1 << 8` | Widget not visible |
| `MKGUI_DISABLED` | `1 << 9` | Widget not interactive |
| `MKGUI_CHECKED` | `1 << 10` | Checked state (checkbox, radio, menu check/radio) |
| `MKGUI_PASSWORD` | `1 << 11` | Input shows dots |
| `MKGUI_READONLY` | `1 << 12` | Input not editable |

### Special flags

| Flag | Value | Description |
|------|-------|-------------|
| `MKGUI_SEPARATOR` | `1 << 13` | Menu item is a separator line |
| `MKGUI_TOOLBAR_SEP` | `1 << 14` | Toolbar separator (before this button) |
| `MKGUI_MENU_CHECK` | `1 << 15` | Menu item with checkbox indicator |
| `MKGUI_MENU_RADIO` | `1 << 16` | Menu item with radio indicator |
| `MKGUI_PANEL_BORDER` | `1 << 17` | Draw border on panel, image, canvas, or glview |
| `MKGUI_PANEL_SUNKEN` | `1 << 18` | Recessed background on panel |
| `MKGUI_SCROLLBAR_HORIZ` | `1 << 19` | Horizontal scrollbar (default is vertical) |
| `MKGUI_IMAGE_STRETCH` | `1 << 20` | Stretch image to fill widget area |
| `MKGUI_SCROLL` | `1 << 21` | Enable scrolling on VBOX (adds scrollbar when content overflows) |
| `MKGUI_NO_PAD` | `1 << 22` | Suppress automatic container padding |

## Events

```c
struct mkgui_event {
    uint32_t type;    // MKGUI_EVENT_*
    uint32_t id;      // widget that triggered the event
    int32_t  value;   // event-specific value
    int32_t  col;     // column (listview events)
    uint32_t keysym;  // key code (key events)
    uint32_t keymod;  // modifier mask (key events)
};
```

| Event | Trigger | value | col |
|-------|---------|-------|-----|
| `MKGUI_EVENT_CLOSE` | Window close | -- | -- |
| `MKGUI_EVENT_RESIZE` | Window resized | -- | -- |
| `MKGUI_EVENT_CLICK` | Button pressed | -- | -- |
| `MKGUI_EVENT_MENU` | Menu item selected | -- | -- |
| `MKGUI_EVENT_TAB_CHANGED` | Tab switched | tab id | -- |
| `MKGUI_EVENT_CHECKBOX_CHANGED` | Checkbox toggled | 0 or 1 | -- |
| `MKGUI_EVENT_RADIO_CHANGED` | Radio selected | -- | -- |
| `MKGUI_EVENT_DROPDOWN_CHANGED` | Dropdown changed | selected index | -- |
| `MKGUI_EVENT_SLIDER_CHANGED` | Slider moved | new value | -- |
| `MKGUI_EVENT_SPINBOX_CHANGED` | Spinbox changed | new value | -- |
| `MKGUI_EVENT_INPUT_CHANGED` | Input text changed | -- | -- |
| `MKGUI_EVENT_TEXTAREA_CHANGED` | Textarea changed | -- | -- |
| `MKGUI_EVENT_LISTVIEW_SELECT` | Row clicked | row index | column index |
| `MKGUI_EVENT_LISTVIEW_DBLCLICK` | Row double-clicked | row index | -- |
| `MKGUI_EVENT_LISTVIEW_SORT` | Column header clicked | sort direction | column index |
| `MKGUI_EVENT_LISTVIEW_COL_REORDER` | Column dragged to new position | -- | -- |
| `MKGUI_EVENT_LISTVIEW_REORDER` | Row dragged to new position | source row | target row |
| `MKGUI_EVENT_TREEVIEW_SELECT` | Tree node selected | node id | -- |
| `MKGUI_EVENT_TREEVIEW_EXPAND` | Tree node expanded | node id | -- |
| `MKGUI_EVENT_TREEVIEW_COLLAPSE` | Tree node collapsed | node id | -- |
| `MKGUI_EVENT_SPLITTER_MOVED` | Splitter dragged | -- | -- |
| `MKGUI_EVENT_KEY` | Key pressed | keysym, keymod | -- |
| `MKGUI_EVENT_ITEMVIEW_SELECT` | Item selected | item index | -- |
| `MKGUI_EVENT_ITEMVIEW_DBLCLICK` | Item double-clicked | item index | -- |
| `MKGUI_EVENT_SCROLL` | Scrollbar value changed | scroll position | -- |

## API reference

### Lifecycle

```c
struct mkgui_ctx *mkgui_create(struct mkgui_widget *widgets, uint32_t count);
void mkgui_destroy(struct mkgui_ctx *ctx);
uint32_t mkgui_poll(struct mkgui_ctx *ctx, struct mkgui_event *ev);
void mkgui_set_poll_timeout(struct mkgui_ctx *ctx, int32_t ms);
```

`mkgui_create` takes a widget array and returns a context. `mkgui_poll` processes platform events and renders when dirty. Returns 1 if an event was written to `ev`, 0 otherwise.

When the event queue is empty and the widget tree is not dirty, `mkgui_poll` blocks internally instead of returning immediately. This means no external sleep is needed -- the standard mainloop is simply:

```c
while(running) {
    while(mkgui_poll(ctx, &ev)) {
        // handle ev
    }
    // timeout expired, do per-frame work here if needed
}
```

`mkgui_poll` never blocks when:
- Platform events are pending
- The widget tree is dirty (needs rendering)
- A close event has been delivered

#### Poll timeout modes

`mkgui_set_poll_timeout` controls how long `mkgui_poll` blocks when no events are pending:

| Value | Behavior |
|-------|----------|
| `-1` (default) | **Auto mode.** Blocks for 16ms (~60fps) when animated widgets (spinners, progress bars, glviews) are visible. Blocks indefinitely when idle -- zero CPU usage. |
| `> 0` | **Fixed timeout.** Always blocks for the given number of milliseconds. |
| `0` | **No blocking.** Returns immediately even with no events. The caller owns all timing. |

On Linux, blocking uses `poll()` on the X11 connection fd. On Windows, it uses `MsgWaitForMultipleObjects`. Both wake immediately when a platform event arrives.

**Emulator / real-time example** (caller drives timing):

```c
mkgui_set_poll_timeout(ctx, 0);
while(running) {
    while(mkgui_poll(ctx, &ev)) {
        // handle ev
    }
    emulator_tick();
    precision_sleep_until_next_frame();
}
```

### Performance timing

After each `mkgui_poll` call that renders, the context contains timing data for the last frame:

```c
ctx->perf_layout_us   // layout calculation time in microseconds
ctx->perf_render_us   // widget rendering time in microseconds
ctx->perf_blit_us     // framebuffer blit time in microseconds
```

### Sleep

```c
void mkgui_sleep_ms(uint32_t ms);
```

Cross-platform sleep helper. Not needed for normal mainloops (mkgui_poll handles timing internally), but available for custom timing code.

### Label

```c
void mkgui_label_set(struct mkgui_ctx *ctx, uint32_t id, const char *text);
```

### Input

```c
void mkgui_input_set(struct mkgui_ctx *ctx, uint32_t id, const char *text);
const char *mkgui_input_get(struct mkgui_ctx *ctx, uint32_t id);
```

Supports Ctrl+C (copy) and Ctrl+V (paste). Newlines in pasted text are replaced with spaces. Copy is disabled when `MKGUI_PASSWORD` is set. Paste is disabled when `MKGUI_READONLY` is set.

### Checkbox

```c
uint32_t mkgui_checkbox_get(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_checkbox_set(struct mkgui_ctx *ctx, uint32_t id, uint32_t checked);
```

### Radio

```c
uint32_t mkgui_radio_get(struct mkgui_ctx *ctx, uint32_t id);
uint32_t mkgui_radio_get_selected(struct mkgui_ctx *ctx, uint32_t group_parent_id);
```

Radio buttons are mutually exclusive within the same parent. Clicking one unchecks siblings.

### Dropdown

```c
void mkgui_dropdown_setup(struct mkgui_ctx *ctx, uint32_t id, const char **items, uint32_t count);
int32_t mkgui_dropdown_get(struct mkgui_ctx *ctx, uint32_t id);
```

### Slider

```c
void mkgui_slider_setup(struct mkgui_ctx *ctx, uint32_t id, int32_t min_val, int32_t max_val, int32_t value);
int32_t mkgui_slider_get(struct mkgui_ctx *ctx, uint32_t id);
```

### Spinbox

```c
void mkgui_spinbox_setup(struct mkgui_ctx *ctx, uint32_t id, int32_t min_val, int32_t max_val, int32_t value, int32_t step);
int32_t mkgui_spinbox_get(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_spinbox_set(struct mkgui_ctx *ctx, uint32_t id, int32_t value);
```

Click the text area to enter edit mode (cursor appears, existing value is pre-filled). Type a new value and press Enter to confirm (clears focus) or Escape to cancel. Use up/down arrow keys or the +/- buttons to step.

### Progress

```c
void mkgui_progress_setup(struct mkgui_ctx *ctx, uint32_t id, int32_t max_val);
void mkgui_progress_set(struct mkgui_ctx *ctx, uint32_t id, int32_t value);
int32_t mkgui_progress_get(struct mkgui_ctx *ctx, uint32_t id);
```

Renders a filled bar with an animated diagonal shimmer sweep while in progress. The fill color uses `theme.accent`. The percentage text is centered on the bar.

### Spinner

```c
{ MKGUI_SPINNER, ID_SPIN, "", "", ID_PARENT, 10, 10, 24, 24, 0 }
```

No setup or API functions needed. The spinner is a rotating arc that animates automatically. Size is determined by the widget's `w` and `h` (uses the smaller of the two as diameter). Uses `theme.accent` for the arc color. Place it anywhere a loading indicator is needed.

The spinner only consumes CPU while visible. Widgets on inactive tabs or hidden parents do not trigger redraws.

### Listview

```c
typedef void (*mkgui_row_cb)(uint32_t row, uint32_t col, char *buf, uint32_t buf_size, void *userdata);

void mkgui_listview_setup(struct mkgui_ctx *ctx, uint32_t id, uint32_t row_count,
                          uint32_t col_count, struct mkgui_column *columns,
                          mkgui_row_cb cb, void *userdata);
void mkgui_listview_set_rows(struct mkgui_ctx *ctx, uint32_t id, uint32_t row_count);
int32_t mkgui_listview_get_selected(struct mkgui_ctx *ctx, uint32_t id);
const uint32_t *mkgui_listview_get_col_order(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_listview_set_col_order(struct mkgui_ctx *ctx, uint32_t id, const uint32_t *order, uint32_t count);
int32_t mkgui_listview_get_col_width(struct mkgui_ctx *ctx, uint32_t id, uint32_t col);
void mkgui_listview_set_col_width(struct mkgui_ctx *ctx, uint32_t id, uint32_t col, int32_t width);
void mkgui_listview_set_cell_type(struct mkgui_ctx *ctx, uint32_t id, uint32_t col, uint32_t cell_type);
void mkgui_listview_visible_range(struct mkgui_ctx *ctx, uint32_t id, int32_t *first, int32_t *last);
```

The listview is fully virtual -- it never owns data. The row callback is invoked only for visible rows during rendering. For datasets with frequently changing data (e.g. download progress), use `mkgui_listview_visible_range` to check if the changed row is currently visible before dirtying the widget.

#### Column definition

```c
struct mkgui_column {
    char label[256];   // header text
    int32_t width;     // column width in pixels
    uint32_t cell_type; // MKGUI_CELL_* (default: MKGUI_CELL_TEXT)
};
```

Cell type can be set in the column struct at setup time, or changed later via `mkgui_listview_set_cell_type`.

#### Cell types

| Type | Description | Callback format |
|------|-------------|-----------------|
| `MKGUI_CELL_TEXT` | Left-aligned text (default) | Any string |
| `MKGUI_CELL_PROGRESS` | Simple filled progress bar with percentage text | `"value/max"` (e.g. `"75/100"`) or just `"75"` (max defaults to 100) |
| `MKGUI_CELL_ICON_TEXT` | Icon followed by text | `"icon-name\tLabel"` (tab-separated; icon name is an MDI icon name) |
| `MKGUI_CELL_SIZE` | Right-aligned formatted file size | Byte count as string (e.g. `"1048576"` renders as `"1.0 MB"`) |
| `MKGUI_CELL_DATE` | Formatted date/time | Unix timestamp as string (e.g. `"1741900800"` renders as `"2025-03-14 00:00"`) |
| `MKGUI_CELL_CHECKBOX` | Centered checkbox | `"1"` for checked, `"0"` for unchecked |

Cell progress bars are non-animated (no shimmer), using `theme.accent` for the fill. They are purely data-driven via the row callback.

#### Row drag-and-drop reordering

Rows can be reordered by dragging. Click and drag a row 4+ pixels vertically to start a drag. A horizontal accent-colored line shows the drop position. On release, `MKGUI_EVENT_LISTVIEW_REORDER` is emitted with `ev->value = source_row` and `ev->col = target_row`. The application is responsible for reordering its data array.

#### Column interaction

Column headers support drag-and-drop reordering and resize. Drag a header to a new position to rearrange columns. A short click (< 4px movement) sorts by that column instead. Drag the divider between columns to resize (minimum 20px, cursor changes to a resize arrow on hover). The column order maps display positions to logical column indices -- the row callback always receives the logical column index regardless of display order. Use `mkgui_listview_get_col_order` / `mkgui_listview_set_col_order` and `mkgui_listview_get_col_width` / `mkgui_listview_set_col_width` to save and restore layouts.

Sorting is application-side. When a header is clicked, `MKGUI_EVENT_LISTVIEW_SORT` is emitted with `ev->col` set to the column and `ev->value` set to the sort direction (1 ascending, -1 descending). The listview tracks the sort arrow visual internally but the application must sort its data and call `dirty_all`.

### Itemview

A multi-mode view widget for displaying items as icons, thumbnails, a compact list, or a detailed list. Similar to the view modes in file managers like pcmanfm-qt.

```c
typedef void (*mkgui_itemview_label_cb)(uint32_t item, char *buf, uint32_t buf_size, void *userdata);
typedef void (*mkgui_itemview_icon_cb)(uint32_t item, char *buf, uint32_t buf_size, void *userdata);
typedef void (*mkgui_thumbnail_cb)(uint32_t item, uint32_t *pixels, int32_t w, int32_t h, void *userdata);

void mkgui_itemview_setup(struct mkgui_ctx *ctx, uint32_t id, uint32_t item_count,
                          uint32_t view_mode, mkgui_itemview_label_cb label_cb,
                          mkgui_itemview_icon_cb icon_cb, void *userdata);
void mkgui_itemview_set_thumbnail(struct mkgui_ctx *ctx, uint32_t id, mkgui_thumbnail_cb cb, int32_t thumb_size);
void mkgui_itemview_set_view(struct mkgui_ctx *ctx, uint32_t id, uint32_t view_mode);
void mkgui_itemview_set_items(struct mkgui_ctx *ctx, uint32_t id, uint32_t count);
void mkgui_itemview_set_cell_size(struct mkgui_ctx *ctx, uint32_t id, int32_t w, int32_t h);
int32_t mkgui_itemview_get_selected(struct mkgui_ctx *ctx, uint32_t id);
uint32_t mkgui_itemview_get_view(struct mkgui_ctx *ctx, uint32_t id);
```

#### View modes

| Mode | Description |
|------|-------------|
| `MKGUI_VIEW_ICON` | Grid of icons with labels below. Vertical scrollbar. |
| `MKGUI_VIEW_THUMBNAIL` | Grid of large thumbnails with labels below. Vertical scrollbar. |
| `MKGUI_VIEW_COMPACT` | Single-row-height items in columns, filling top-to-bottom then left-to-right. Horizontal scrollbar at bottom. |
| `MKGUI_VIEW_DETAIL` | One item per row with icon and label. Vertical scrollbar. |

#### Callbacks

The itemview does not own data. It uses callbacks to request labels, icons, and thumbnails on demand:

- **`label_cb(item, buf, buf_size, userdata)`** -- fill `buf` with the display name for `item`. Required.
- **`icon_cb(item, buf, buf_size, userdata)`** -- fill `buf` with the MDI icon name for `item` (e.g. `"folder"`, `"file-document"`). Used by icon, compact, and detail modes. Optional (no icon drawn if NULL).
- **`thumbnail_cb(item, pixels, w, h, userdata)`** -- fill the `pixels` buffer (w*h uint32_t ARGB) with a thumbnail image. Used only in thumbnail mode. Optional; set via `mkgui_itemview_set_thumbnail`. The `thumb_size` parameter controls the thumbnail resolution (default 96px).

#### Setup example

```c
void my_label(uint32_t item, char *buf, uint32_t buf_size, void *ud) {
    snprintf(buf, buf_size, "File %u.txt", item + 1);
}

void my_icon(uint32_t item, char *buf, uint32_t buf_size, void *ud) {
    snprintf(buf, buf_size, "text-x-generic");
}

mkgui_itemview_setup(ctx, ID_ITEMVIEW, 200, MKGUI_VIEW_ICON, my_label, my_icon, NULL);
```

Switch view modes at runtime:

```c
mkgui_itemview_set_view(ctx, ID_ITEMVIEW, MKGUI_VIEW_COMPACT);
```

### Treeview

```c
void mkgui_treeview_setup(struct mkgui_ctx *ctx, uint32_t id);
uint32_t mkgui_treeview_add(struct mkgui_ctx *ctx, uint32_t widget_id,
                            uint32_t node_id, uint32_t parent_node, const char *label);
int32_t mkgui_treeview_get_selected(struct mkgui_ctx *ctx, uint32_t id);
```

`parent_node = 0` for root-level nodes.

### Textarea

```c
void mkgui_textarea_set(struct mkgui_ctx *ctx, uint32_t id, const char *text);
const char *mkgui_textarea_get(struct mkgui_ctx *ctx, uint32_t id);
```

Supports Ctrl+C (copy) and Ctrl+V (paste). Uses the system clipboard (X11 CLIPBOARD selection / Win32 clipboard).

### Statusbar

```c
void mkgui_statusbar_setup(struct mkgui_ctx *ctx, uint32_t id, uint32_t section_count, const int32_t *widths);
void mkgui_statusbar_set(struct mkgui_ctx *ctx, uint32_t id, uint32_t section, const char *text);
```

Section widths: positive = fixed pixels, negative = fill remaining space.

```c
int32_t widths[] = { -1, 150, 100 };  // 3 sections: flexible, 150px, 100px
mkgui_statusbar_setup(ctx, ID_SB, 3, widths);
```

### Canvas

Custom drawing widget with automatic clipping.

```c
typedef void (*mkgui_canvas_cb)(struct mkgui_ctx *ctx, uint32_t id, uint32_t *pixels,
                                 int32_t x, int32_t y, int32_t w, int32_t h, void *userdata);

void mkgui_canvas_set_callback(struct mkgui_ctx *ctx, uint32_t id, mkgui_canvas_cb callback, void *userdata);
```

```c
{ MKGUI_CANVAS, ID_CANVAS, "", "", ID_PARENT, 10, 10, 400, 300, MKGUI_PANEL_BORDER }
```

The callback receives the pixel buffer and the widget's rect. All `draw_*` functions respect the clip rect, which is automatically set to the widget bounds before the callback and restored after. Use `MKGUI_PANEL_BORDER` to draw a border before the callback runs.

```c
void my_draw(struct mkgui_ctx *ctx, uint32_t id, uint32_t *pixels,
             int32_t x, int32_t y, int32_t w, int32_t h, void *userdata) {
    draw_rect_fill(pixels, ctx->win_w, ctx->win_h, x, y, w, h, 0xFF2A2A2A);
    // ... custom drawing ...
}

mkgui_canvas_set_callback(ctx, ID_CANVAS, my_draw, NULL);
```

## Menus

Menus are built from a `MKGUI_MENU` (the bar) containing `MKGUI_MENUITEM` entries. Submenus are menu items whose parent is another menu item:

```c
{ MKGUI_MENU,     ID_MENU,  "",     "",             ID_WINDOW, 0, 0, 0, 22, MKGUI_ANCHOR_LEFT | MKGUI_ANCHOR_TOP | MKGUI_ANCHOR_RIGHT },
{ MKGUI_MENUITEM, ID_FILE,  "File", "",             ID_MENU,   0, 0, 0, 0, 0 },
{ MKGUI_MENUITEM, ID_OPEN,  "Open", "folder-open",  ID_FILE,   0, 0, 0, 0, 0 },
{ MKGUI_MENUITEM, ID_SAVE,  "Save", "content-save", ID_FILE,   0, 0, 0, 0, 0 },
{ MKGUI_MENUITEM, ID_SEP1,  "",     "",             ID_FILE,   0, 0, 0, 0, MKGUI_SEPARATOR },
{ MKGUI_MENUITEM, ID_EXIT,  "Exit", "",             ID_FILE,   0, 0, 0, 0, 0 },
```

`MKGUI_SEPARATOR` makes the item render as a horizontal separator line. Use a dedicated menu item with an empty label and the `MKGUI_SEPARATOR` flag to create visual grouping between menu entries.

Check/radio menu items:

```c
{ MKGUI_MENUITEM, ID_SHOW_TB, "Show Toolbar", "", ID_VIEW, 0, 0, 0, 0, MKGUI_MENU_CHECK | MKGUI_CHECKED },
{ MKGUI_MENUITEM, ID_SORT_NAME, "By Name",    "", ID_SORT, 0, 0, 0, 0, MKGUI_MENU_RADIO | MKGUI_CHECKED },
{ MKGUI_MENUITEM, ID_SORT_SIZE, "By Size",    "", ID_SORT, 0, 0, 0, 0, MKGUI_MENU_RADIO },
```

Check items toggle on click. Radio items auto-uncheck siblings. Only leaf items (no children) emit `MKGUI_EVENT_MENU`.

## Toolbar

Toolbar children are `MKGUI_BUTTON` widgets. Use `MKGUI_TOOLBAR_SEP` on a button to draw a separator before it. Toolbar buttons support tooltips via the widget label:

```c
{ MKGUI_TOOLBAR, ID_TB,      "",     "",             ID_WINDOW, 0, 22, 0, 28, MKGUI_ANCHOR_LEFT | MKGUI_ANCHOR_TOP | MKGUI_ANCHOR_RIGHT },
{ MKGUI_BUTTON,  ID_TB_NEW,  "New",  "file-plus",    ID_TB,     0, 0, 0, 0, 0 },
{ MKGUI_BUTTON,  ID_TB_OPEN, "Open", "folder-open",  ID_TB,     0, 0, 0, 0, MKGUI_TOOLBAR_SEP },
{ MKGUI_BUTTON,  ID_TB_SAVE, "Save", "content-save", ID_TB,     0, 0, 0, 0, 0 },
```

Toolbar buttons emit `MKGUI_EVENT_CLICK`. When a toolbar button has a label but shows only an icon, the label appears as a tooltip after a short hover delay.

## Splitters

```c
{ MKGUI_VSPLIT,   ID_SPLIT, "", "", ID_WIN, 0, 0, 500, 400, 0 },
{ MKGUI_TREEVIEW, ID_TREE,  "", "", ID_SPLIT, 0, 0, 200, 400, MKGUI_REGION_LEFT },
{ MKGUI_LISTVIEW, ID_LIST,  "", "", ID_SPLIT, 0, 0, 300, 400, MKGUI_REGION_RIGHT },
```

The divider is draggable. Children sized automatically based on their region flag.

## Group box

A container that draws a thin rounded border with a title label breaking the top edge. Children are automatically inset inside the frame (top padding accounts for the label height, sides and bottom have a small margin).

```c
{ MKGUI_GROUP,    ID_GRP, "Settings",     "", ID_TAB1, 10, 10, 400, 170, 0 },
{ MKGUI_INPUT,    ID_INP, "",             "", ID_GRP,   0,  0, 250,  24, 0 },
{ MKGUI_CHECKBOX, ID_CHK, "Enable",       "", ID_GRP,   0, 34, 200,  24, 0 },
```

Children use coordinates relative to the group's content area (inside the border). The group is not focusable and does not intercept mouse events -- clicks pass through to children.

## Panel

A plain container without the group box border/title. Useful for layout grouping. Children are positioned relative to the panel.

```c
{ MKGUI_PANEL, ID_PANEL, "", "", ID_TAB1, 10, 10, 300, 200, MKGUI_PANEL_BORDER | MKGUI_PANEL_SUNKEN },
{ MKGUI_BUTTON, ID_BTN,  "OK", "", ID_PANEL, 10, 10, 80, 28, 0 },
```

Flags: `MKGUI_PANEL_BORDER` draws a rounded border, `MKGUI_PANEL_SUNKEN` darkens the background.

## Scrollbar

Standalone scrollbar widget. Vertical by default.

```c
{ MKGUI_SCROLLBAR, ID_SB, "", "", ID_TAB1, 10, 10, MKGUI_SCROLLBAR_W, 200, 0 },
```

```c
void mkgui_scrollbar_setup(struct mkgui_ctx *ctx, uint32_t id, int32_t max_value, int32_t page_size);
void mkgui_scrollbar_set(struct mkgui_ctx *ctx, uint32_t id, int32_t value);
int32_t mkgui_scrollbar_get(struct mkgui_ctx *ctx, uint32_t id);
```

- `max_value`: total content size
- `page_size`: visible area size (determines thumb size)
- `value`: scroll position (0 to `max_value - page_size`)

Use `MKGUI_SCROLLBAR_HORIZ` flag for a horizontal scrollbar. Emits `MKGUI_EVENT_SCROLL` on interaction (thumb drag, click, scroll wheel).

## Image

Displays ARGB pixel data. The image is centered in the widget and scaled down to fit (never scaled up) by default.

```c
{ MKGUI_IMAGE, ID_IMG, "", "", ID_TAB1, 10, 10, 200, 200, MKGUI_PANEL_BORDER },
```

```c
void mkgui_image_set(struct mkgui_ctx *ctx, uint32_t id, const uint32_t *pixels, int32_t w, int32_t h);
void mkgui_image_clear(struct mkgui_ctx *ctx, uint32_t id);
```

Pixels are ARGB format (alpha in bits 31-24). The widget copies the pixel data internally. Use `MKGUI_IMAGE_STRETCH` to stretch the image to fill the widget area. Use `MKGUI_PANEL_BORDER` for a border frame.

## GL View

Embeds an OpenGL viewport. mkgui creates a native child window; the user creates their own GL context and renders to it. The widget automatically triggers 60fps redraws when visible on the active tab.

```c
{ MKGUI_GLVIEW, ID_GL, "", "", ID_TAB1, 10, 10, 400, 300, MKGUI_PANEL_BORDER },
```

```c
uint32_t mkgui_glview_init(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_glview_destroy(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_glview_get_size(struct mkgui_ctx *ctx, uint32_t id, int32_t *w, int32_t *h);

// Linux (X11):
Window mkgui_glview_get_x11_window(struct mkgui_ctx *ctx, uint32_t id);
Display *mkgui_glview_get_x11_display(struct mkgui_ctx *ctx);

// Windows:
HWND mkgui_glview_get_hwnd(struct mkgui_ctx *ctx, uint32_t id);
```

### Usage

1. Define the widget in your widget array
2. After `mkgui_create`, call `mkgui_glview_init` to create the native child window
3. Get the native handle and create your GL context (GLX, WGL, EGL, etc.)
4. In your render loop, make the context current, draw, and swap buffers

```c
mkgui_glview_init(ctx, ID_GL);

// Linux example with GLX:
Display *dpy = mkgui_glview_get_x11_display(ctx);
Window win = mkgui_glview_get_x11_window(ctx, ID_GL);
// ... create GLXContext with glXCreateContext / glXCreateNewContext ...

// In render loop:
glXMakeCurrent(dpy, win, glctx);
int32_t w, h;
mkgui_glview_get_size(ctx, ID_GL, &w, &h);
glViewport(0, 0, w, h);
// ... draw ...
glXSwapBuffers(dpy, win);
```

mkgui does not link against OpenGL. The user brings their own GL loader and context creation. The widget automatically repositions/resizes the child window when the layout changes (e.g. window resize, tab switch). GL animations should use `ctx->anim_time` (wall-clock seconds) for time-based motion rather than per-frame increments.

## Auto-layout containers

Three container types provide automatic child positioning. Children's `x, y` values are ignored -- the container controls placement.

### Container padding

Layout containers (VBOX, HBOX, FORM) automatically apply 6px internal padding following these rules:
- **Top-level containers** (parent is a window, tab, panel, etc.): 6px padding on all sides
- **Nested containers** (parent is another VBOX/HBOX/FORM/GROUP/TABS): no padding (parent's gap provides spacing)
- **Containers with `MKGUI_PANEL_BORDER`**: always have padding regardless of nesting (the border creates a visual boundary)
- Use `MKGUI_NO_PAD` to explicitly suppress padding on any container

### VBox

Stacks children vertically with 6px gap. Each child gets the full container width. Children with `h > 0` get a fixed height. Children with `h = 0` flex to share remaining vertical space equally. Use `MKGUI_SCROLL` to enable vertical scrolling when content overflows.

```c
{ MKGUI_VBOX,   ID_VBOX,  "", "", ID_TAB1, 10, 10, 300, 400, 0 },
{ MKGUI_BUTTON, ID_BTN1,  "Fixed (28px)", "", ID_VBOX, 0, 0, 0, 28, 0 },
{ MKGUI_INPUT,  ID_INP1,  "",             "", ID_VBOX, 0, 0, 0, 24, 0 },
{ MKGUI_BUTTON, ID_BTN2,  "Flex (fills)", "", ID_VBOX, 0, 0, 0,  0, 0 },
```

### HBox

Stacks children horizontally with 6px gap. Each child gets the full container height. Children with `w > 0` get a fixed width. Children with `w = 0` flex to share remaining horizontal space equally.

```c
{ MKGUI_HBOX,   ID_HBOX, "", "", ID_TAB1, 10, 10, 600, 300, 0 },
{ MKGUI_PANEL,  ID_LEFT, "", "", ID_HBOX, 0, 0, 200, 0, MKGUI_PANEL_BORDER },
{ MKGUI_PANEL,  ID_MID,  "", "", ID_HBOX, 0, 0,   0, 0, 0 },  // flex
{ MKGUI_PANEL,  ID_RIGHT,"", "", ID_HBOX, 0, 0, 150, 0, MKGUI_PANEL_BORDER },
```

### Form

Two-column form layout for label+control pairs. Children are paired in order: 1st is label, 2nd is control, 3rd is label, 4th is control, etc. The label column auto-sizes to the widest label. Each row is 24px tall with 6px gap.

```c
{ MKGUI_FORM,     ID_FORM,  "", "", ID_TAB1, 10, 10, 400, 200, 0 },
{ MKGUI_LABEL,    ID_LBL1,  "Name:",    "", ID_FORM, 0, 0, 0, 0, 0 },
{ MKGUI_INPUT,    ID_INP1,  "",          "", ID_FORM, 0, 0, 0, 0, 0 },
{ MKGUI_LABEL,    ID_LBL2,  "Email:",   "", ID_FORM, 0, 0, 0, 0, 0 },
{ MKGUI_INPUT,    ID_INP2,  "",          "", ID_FORM, 0, 0, 0, 0, 0 },
{ MKGUI_LABEL,    ID_LBL3,  "Category:","", ID_FORM, 0, 0, 0, 0, 0 },
{ MKGUI_DROPDOWN, ID_DRP1,  "",          "", ID_FORM, 0, 0, 0, 0, 0 },
```

Containers can be nested. For example, an HBOX containing a VBOX and a FORM side by side, where the VBOX has a fixed width and the FORM flexes to fill remaining space.

## Tabs

```c
{ MKGUI_TABS, ID_TABS, "",         "", ID_WINDOW, 0, 50, 0, 22,
  MKGUI_ANCHOR_LEFT | MKGUI_ANCHOR_TOP | MKGUI_ANCHOR_RIGHT | MKGUI_ANCHOR_BOTTOM },
{ MKGUI_TAB,  ID_TAB1, "General",  "", ID_TABS, 0, 0, 0, 0, 0 },
{ MKGUI_TAB,  ID_TAB2, "Settings", "", ID_TABS, 0, 0, 0, 0, 0 },

// Widgets inside tabs
{ MKGUI_BUTTON, ID_BTN, "OK", "", ID_TAB1, 10, 10, 80, 28, 0 },
```

Only widgets parented to the active tab are visible. Animated widgets (spinners, progress bars, glviews) on inactive tabs do not consume CPU.

## Key constants

```c
MKGUI_KEY_BACKSPACE  0xFF08    MKGUI_KEY_TAB     0xFF09
MKGUI_KEY_RETURN     0xFF0D    MKGUI_KEY_ESCAPE  0xFF1B
MKGUI_KEY_DELETE     0xFFFF    MKGUI_KEY_HOME    0xFF50
MKGUI_KEY_LEFT       0xFF51    MKGUI_KEY_UP      0xFF52
MKGUI_KEY_RIGHT      0xFF53    MKGUI_KEY_DOWN    0xFF54
MKGUI_KEY_PAGE_UP    0xFF55    MKGUI_KEY_PAGE_DOWN 0xFF56
MKGUI_KEY_END        0xFF57

MKGUI_MOD_SHIFT      (1 << 0)
MKGUI_MOD_CONTROL    (1 << 2)
```

## Size constants

```c
MKGUI_ROW_HEIGHT       20    // default row height (listview, treeview, dropdown)
MKGUI_TAB_HEIGHT       26    // tab bar height
MKGUI_MENU_HEIGHT      22    // menu bar height
MKGUI_TOOLBAR_HEIGHT   28    // toolbar height
MKGUI_STATUSBAR_HEIGHT 22    // statusbar height
MKGUI_SCROLLBAR_W      14    // scrollbar width
MKGUI_SPLIT_THICK       5    // splitter handle thickness
MKGUI_BOX_GAP           6    // gap between children in layout containers
MKGUI_BOX_PAD           6    // internal padding in layout containers
```

## Icons

mkgui uses pre-rasterized icons from the Material Design Icons (MDI) set, stored in an external `mdi_icons.dat` file. This file must be placed next to the executable (or in `ext/` during development). At startup, mkgui looks for it at `mdi_icons.dat` then `ext/mdi_icons.dat`.

Icons are looked up by name (e.g. `"folder-open"`, `"content-save"`) and rendered at the size baked into the `.dat` file (default 18x18). Browse available icons at https://pictogrammers.com/library/mdi/.

```c
// Set icon on a widget (toolbar button, menu item, button, tab)
void mkgui_set_icon(struct mkgui_ctx *ctx, uint32_t widget_id, const char *icon_name);

// Set icon on a treeview node
void mkgui_set_treenode_icon(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id, const char *icon_name);
```

### Generating the icon pack

The icon pack is generated from the MDI TTF font using the `gen_icons` tool:

```bash
./tools/gen_icons ext/materialdesignicons-webfont.ttf 18 mdi_icons.dat
```

The second argument is the icon size in pixels. Regenerate with a different size at any time.

### Custom icons

Register custom ARGB icons at any size using `mkgui_icon_add`. Custom icons override `.dat` icons with the same name and are not affected by theme changes (the caller owns the pixel data).

```c
int32_t mkgui_icon_add(const char *name, const uint32_t *pixels, int32_t w, int32_t h);
```

- `name` -- icon name (same namespace as MDI names)
- `pixels` -- ARGB pixel data (alpha in bits 31-24), copied internally
- `w`, `h` -- icon dimensions (any size)
- Returns the icon index, or -1 on failure

This is useful for app-specific icons or for providing larger icons (e.g. 24x24) for icon-only toolbars:

```c
// Load or generate a 24x24 ARGB icon
uint32_t my_icon[24 * 24];
load_png("my_save_icon.png", my_icon, 24, 24);
mkgui_icon_add("content-save", my_icon, 24, 24);
```

If an icon with the same name already exists, it is replaced.

Override the font with the `MKGUI_FONT` environment variable:

```bash
MKGUI_FONT=/path/to/font.ttf ./myapp
```

The default font search order prefers proportional sans-serif fonts: Noto Sans, IBM Plex Sans, DejaVu Sans.

## Simple dialogs

Convenience functions for common dialog patterns. All are blocking (modal).

### Message box

```c
void mkgui_message_box(struct mkgui_ctx *ctx, const char *title, const char *message);
```

Displays a message with an OK button. Closes on OK, Enter, Escape, or window close.

### Confirm dialog

```c
uint32_t mkgui_confirm_dialog(struct mkgui_ctx *ctx, const char *title, const char *message);
```

Displays a message with OK and Cancel buttons. Returns 1 if OK, 0 if cancelled.

### Input dialog

```c
uint32_t mkgui_input_dialog(struct mkgui_ctx *ctx, const char *title, const char *prompt,
                             const char *default_text, char *out, uint32_t out_size);
```

Displays a prompt label with a text input field and OK/Cancel buttons. Returns 1 if OK (text written to `out`), 0 if cancelled. `default_text` can be NULL.

```c
char name[256];
if(mkgui_input_dialog(ctx, "New File", "File name:", "untitled.txt", name, sizeof(name))) {
    printf("Created: %s\n", name);
}
```

## File dialogs

mkgui includes built-in open and save file dialogs. Both are blocking (modal) -- they run a nested event loop and return when the user confirms or cancels.

### Open dialog

```c
uint32_t count = mkgui_open_dialog(ctx);
for(uint32_t i = 0; i < count; ++i) {
    printf("Selected: %s\n", mkgui_dialog_path(ctx, i));
}
```

Returns the number of selected files (0 = cancelled). Supports multi-select with Ctrl+Click and Shift+Click.

### Save dialog

```c
if(mkgui_save_dialog(ctx, "untitled.txt")) {
    printf("Save to: %s\n", mkgui_dialog_path(ctx, 0));
}
```

Returns 1 if the user confirmed, 0 if cancelled. The second argument is the default filename (pre-selected in the input field).

### Dialog window

Dialogs open as separate, WM-decorated X11 windows (not overlays). They are:
- **Resizable** -- drag the window border to see more sidebar entries or file list rows (minimum size 400x300, initial 720x500)
- **Movable** -- drag the title bar to reposition, e.g. to see the main window underneath when naming a file
- **Modal** -- set as transient-for the main window; closing the main window also closes the dialog

The dialog window is centered on the parent window when opened.

### Dialog layout

Both dialogs feature:
- Path bar at top (editable, press Enter to navigate)
- Left sidebar with shortcuts: Home, Desktop, Documents, Downloads, plus bookmarks from `~/.config/gtk-3.0/bookmarks`
- File list on the right with Name and Size columns
- Scrollable file list with mouse wheel
- Double-click directories to enter them, double-click files to select and confirm
- Escape to cancel, Enter to confirm
- Tab to cycle focus between path bar, filename input (save), and file list
- Ctrl+A to select all files (open dialog)

The save dialog adds a "File name:" input row at the bottom. Clicking a file in the list fills the filename field automatically.

### API reference

```c
uint32_t mkgui_open_dialog(struct mkgui_ctx *ctx);
uint32_t mkgui_save_dialog(struct mkgui_ctx *ctx, const char *default_name);
const char *mkgui_dialog_path(struct mkgui_ctx *ctx, uint32_t index);
```

## Rendering

All widget rendering is software-based (no GPU). The framebuffer is blitted to the X11 window via MIT-SHM (`XShmPutImage`).

### Dirty rectangle tracking

mkgui uses dirty rectangle tracking to minimize CPU usage. Only regions that have changed are re-rendered and blitted. When two dirty rects are close together, they are merged if the union area is less than 1.5x the sum of individual areas. Animated widgets (spinners, progress bars, glviews) only dirty their own rect. Widgets on inactive tabs or hidden parents do not trigger redraws.

### Antialiased drawing

Rounded rectangles and circles use 4x4 subpixel sampling for antialiased edges. All math is integer-only (no sqrt or floating point). This is used for:

- Widget borders (buttons, inputs, dropdowns, checkboxes, spinboxes, progress bars)
- Radio button circles (outer ring and inner dot)
- Checkbox checkmark (AA thick line segments)
- Group box borders
- Tooltip backgrounds
- Scrollbar thumbs
- Spinner arc

### Theming

The default theme uses Breeze dark colors. A built-in light theme is also available. Switch themes at runtime with `mkgui_set_theme`:

```c
// Built-in themes
struct mkgui_theme default_theme(void);  // dark (Breeze)
struct mkgui_theme light_theme(void);    // light

// Apply a theme (re-renders icons automatically)
void mkgui_set_theme(struct mkgui_ctx *ctx, struct mkgui_theme theme);
```

Custom themes can be created by filling in a `struct mkgui_theme` manually.

Key theme colors (dark defaults shown):

| Field | Default | Description |
|-------|---------|-------------|
| `bg` | `#232629` | Window background |
| `widget_bg` | `#31363b` | Widget fill |
| `widget_border` | `#5e5e5e` | Widget border |
| `widget_hover` | `#3d4349` | Hover state |
| `widget_press` | `#272b2f` | Pressed state |
| `text` | `#eff0f1` | Primary text |
| `text_disabled` | `#6e7175` | Disabled text |
| `selection` | `#2980b9` | Selection highlight (listview, treeview) |
| `sel_text` | `#ffffff` | Selected item text |
| `input_bg` | `#1a1e21` | Input/textarea/listview background |
| `tab_active` | `#31363b` | Active tab background |
| `tab_inactive` | `#2a2e32` | Inactive tab background |
| `tab_hover` | `#353a3f` | Tab hover highlight |
| `menu_bg` | `#31363b` | Menu/popup background |
| `menu_hover` | `#3daee9` | Menu hover highlight |
| `scrollbar_bg` | `#1d2023` | Scrollbar track |
| `scrollbar_thumb` | `#4d4d4d` | Scrollbar thumb |
| `scrollbar_thumb_hover` | `#5a5a5a` | Scrollbar thumb on hover/drag |
| `splitter` | `#3daee9` | Splitter handle, focus border, column drag indicator |
| `header_bg` | `#2a2e32` | Listview column header background |
| `listview_alt` | `#2a2e32` | Alternating row background |
| `accent` | `#2a7ab5` | Accent color (spinner arc, progress bar fill, cell progress) |
| `corner_radius` | `3` | Rounded corner radius in pixels |

## Complete example

See [demo.c](../demo.c) for a full working application demonstrating all widget types.

## License

MIT
