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

static void on_event(struct mkgui_ctx *ctx, struct mkgui_event *ev, void *userdata) {
    (void)userdata;
    switch(ev->type) {
        case MKGUI_EVENT_CLOSE: {
            mkgui_quit(ctx);
        } break;

        case MKGUI_EVENT_CLICK: {
            if(ev->id == ID_BTN) {
                mkgui_label_set(ctx, ID_LBL, "Clicked!");
            }
        } break;
    }
}

int main(void) {
    struct mkgui_widget widgets[] = {
        { MKGUI_WINDOW, ID_WIN, "Hello",  "",  0,      400, 300, 0, 0, 0 },
        { MKGUI_BUTTON, ID_BTN, "Click",  "",  ID_WIN, 100, 0, MKGUI_FIXED, 0, 0 },
        { MKGUI_LABEL,  ID_LBL, "Ready",  "",  ID_WIN, 0, 0, 0, 0, 0 },
    };

    struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
    if(!ctx) return 1;

    mkgui_run(ctx, on_event, NULL);
    mkgui_destroy(ctx);
    return 0;
}
```

## Architecture

Your application is a single `.c` file that includes `mkgui.c`. All functions are `static` (unity build). You define widgets as a flat array of `struct mkgui_widget`, create a context, provide an event callback, and call `mkgui_run`.

Widget hierarchy is expressed via `parent_id`. Every widget has a unique integer `id`. The first widget must be `MKGUI_WINDOW` with `parent_id = 0`.

Multiple windows are supported via `mkgui_create_child()`. See [Multi-window](#multi-window) below.

## Widget definition

```c
struct mkgui_widget {
    uint32_t type;        // MKGUI_BUTTON, MKGUI_INPUT, etc.
    uint32_t id;          // unique identifier
    char     label[256];  // display text
    char     icon[64];    // icon name (MDI name, e.g. "folder-open")
    uint32_t parent_id;   // parent widget id
    int32_t  w, h;        // size (0 = natural size derived from font height)
    uint32_t flags;       // universal layout flags (MKGUI_FIXED, MKGUI_HIDDEN, etc.)
    uint32_t style;       // per-widget-type flags (MKGUI_TRUNCATE, MKGUI_CHECKED, etc.)
    uint32_t weight;      // layout weight (0=default flex, >0=explicit proportional)
    int32_t  margin_l, margin_r, margin_t, margin_b; // layout margins
};
```

The `flags` field holds universal layout and state flags: `MKGUI_FIXED`, `MKGUI_HIDDEN`, `MKGUI_DISABLED`, `MKGUI_SCROLL`, `MKGUI_NO_PAD`, `MKGUI_VERTICAL`, `MKGUI_REGION_*`, `MKGUI_ALIGN_*`, `MKGUI_TOOLBAR_SEP`. The `style` field holds per-widget-type appearance flags: `MKGUI_CHECKED`, `MKGUI_PASSWORD`, `MKGUI_READONLY`, `MKGUI_SEPARATOR`, `MKGUI_MENU_CHECK`, `MKGUI_MENU_RADIO`, `MKGUI_PANEL_BORDER`, `MKGUI_PANEL_SUNKEN`, `MKGUI_SLIDER_MIXER`, `MKGUI_METER_TEXT`, `MKGUI_IMAGE_STRETCH`, `MKGUI_TAB_CLOSABLE`, `MKGUI_MULTI_SELECT`, `MKGUI_TRUNCATE`, `MKGUI_TOOLBAR_ICONS_ONLY`, `MKGUI_TOOLBAR_TEXT_ONLY`, `MKGUI_LINK`, `MKGUI_COLLAPSIBLE`, `MKGUI_COLLAPSED`, `MKGUI_WRAP`, `MKGUI_NUMERIC`.

There are no `x, y` fields. All positioning is handled by the container-based layout system. The `w` and `h` fields specify size; when `h` is 0, the widget uses its natural height (derived from the font height for most widget types).

## Widget types

| Type | Description |
|------|-------------|
| `MKGUI_WINDOW` | Root window. Must be first widget. |
| `MKGUI_BUTTON` | Push button. Emits `MKGUI_EVENT_CLICK`. Set `MKGUI_CHECKED` for toggle button (renders sunken when checked). |
| `MKGUI_LABEL` | Static text. |
| `MKGUI_INPUT` | Single-line text input. Emits `MKGUI_EVENT_INPUT_CHANGED`, `MKGUI_EVENT_INPUT_SUBMIT` on Enter. |
| `MKGUI_TEXTAREA` | Multi-line text editor. Emits `MKGUI_EVENT_TEXTAREA_CHANGED`, `MKGUI_EVENT_TEXTAREA_CURSOR` on cursor movement. |
| `MKGUI_CHECKBOX` | Toggle checkbox. Emits `MKGUI_EVENT_CHECKBOX_CHANGED`. |
| `MKGUI_RADIO` | Radio button (mutually exclusive within parent). Emits `MKGUI_EVENT_RADIO_CHANGED`. |
| `MKGUI_DROPDOWN` | Drop-down selector. Emits `MKGUI_EVENT_DROPDOWN_CHANGED`. |
| `MKGUI_SLIDER` | Horizontal slider. `MKGUI_VERTICAL` for vertical, `MKGUI_SLIDER_MIXER` for tapered volume style with meter. Emits `MKGUI_EVENT_SLIDER_START`, `MKGUI_EVENT_SLIDER_CHANGED`, `MKGUI_EVENT_SLIDER_END`. |
| `MKGUI_SPINBOX` | Numeric input with +/- buttons. Emits `MKGUI_EVENT_SPINBOX_CHANGED`. Click the text area to type directly, Enter to confirm (clears focus), Escape to cancel editing. |
| `MKGUI_PROGRESS` | Progress bar with animated shimmer effect. Per-widget color override via `mkgui_progress_set_color`. No events. |
| `MKGUI_METER` | Level meter with colored zones. `MKGUI_VERTICAL` for vertical, `MKGUI_METER_TEXT` for percentage text. No events. |
| `MKGUI_SPINNER` | Animated spinning arc indicator. No events, no setup needed. |
| `MKGUI_LISTVIEW` | Scrollable multi-column list with virtual rows and per-column cell types. Emits `MKGUI_EVENT_LISTVIEW_SELECT`, `_DBLCLICK`, `_SORT`, `_COL_REORDER`, `_REORDER`. |
| `MKGUI_GRIDVIEW` | Multi-column grid with per-cell checkboxes and resizable columns. Virtual data via callback. Emits `MKGUI_EVENT_GRID_CLICK`, `MKGUI_EVENT_GRID_CHECK`, `_GRIDVIEW_REORDER`. |
| `MKGUI_RICHLIST` | Rich list with thumbnail, title, subtitle, and right-aligned text per row. Virtual data via callback. Emits `MKGUI_EVENT_RICHLIST_SELECT`, `_RICHLIST_DBLCLICK`. |
| `MKGUI_ITEMVIEW` | Multi-mode item view (icon, thumbnail, compact, detail). Emits `MKGUI_EVENT_ITEMVIEW_SELECT`, `_DBLCLICK`. |
| `MKGUI_TREEVIEW` | Hierarchical tree. Emits `MKGUI_EVENT_TREEVIEW_SELECT`, `_DBLCLICK`, `_EXPAND`, `_COLLAPSE`, `_MOVE`. |
| `MKGUI_TABS` | Tab container. Children must be `MKGUI_TAB`. Emits `MKGUI_EVENT_TAB_CHANGED`. |
| `MKGUI_TAB` | Individual tab page (parent must be `MKGUI_TABS`). |
| `MKGUI_MENU` | Menu bar. Children are top-level `MKGUI_MENUITEM`. |
| `MKGUI_MENUITEM` | Menu entry. Parent is `MKGUI_MENU` or another `MKGUI_MENUITEM` (submenu). Leaf items emit `MKGUI_EVENT_MENU`. |
| `MKGUI_TOOLBAR` | Toolbar bar. Children are `MKGUI_BUTTON`. Supports display modes: icons+text (default), icons only, text only. Height adapts to content. |
| `MKGUI_STATUSBAR` | Status bar with sections. |
| `MKGUI_HSPLIT` | Horizontal splitter. Children use `MKGUI_REGION_TOP`/`MKGUI_REGION_BOTTOM`. Neither side can be smaller than `MKGUI_SPLIT_MIN_PX` (50px). |
| `MKGUI_VSPLIT` | Vertical splitter. Children use `MKGUI_REGION_LEFT`/`MKGUI_REGION_RIGHT`. Same minimum size enforcement. |
| `MKGUI_GROUP` | Group box container. Draws a thin rounded border with a label in the top edge. Children are inset inside the frame. |
| `MKGUI_PANEL` | Plain container that behaves as a VBOX. No border by default. Use `MKGUI_PANEL_BORDER` for a border, `MKGUI_PANEL_SUNKEN` for a recessed look. Children are stacked vertically like VBOX. |
| `MKGUI_SCROLLBAR` | Standalone scrollbar. Horizontal by default, `MKGUI_VERTICAL` for vertical. Emits `MKGUI_EVENT_SCROLL`. |
| `MKGUI_IMAGE` | Displays ARGB pixel data. Centered by default, `MKGUI_IMAGE_STRETCH` to fill. `MKGUI_PANEL_BORDER` for a border. |
| `MKGUI_GLVIEW` | OpenGL viewport. Creates a native child window for GL rendering. The user creates their own GL context. Automatically triggers 60fps redraws when visible. |
| `MKGUI_CANVAS` | Custom drawing area. Calls a user callback with clipping set to the widget rect. Supports `MKGUI_PANEL_BORDER`. |
| `MKGUI_VBOX` | Vertical box layout. Stacks children top-to-bottom. Uses weight-based distribution. |
| `MKGUI_HBOX` | Horizontal box layout. Stacks children left-to-right. Uses weight-based distribution. |
| `MKGUI_FORM` | Two-column form layout. Children are paired: odd=label, even=control. Label column auto-sizes to widest label. |

## Container-based layout

All layout is container-based. There are no `x, y` position fields on widgets. Every widget's position is determined by its parent container (VBOX, HBOX, FORM, PANEL, GROUP, TABS, or the WINDOW itself).

### How it works

- **WINDOW** acts as a VBOX for its non-chrome children. Menu, toolbar, statusbar, and pathbar are auto-positioned as window chrome (see [Window chrome auto-layout](#window-chrome-auto-layout)). All other children fill the remaining content area using VBOX stacking.
- **PANEL** behaves as a VBOX container. Children are stacked vertically.
- **VBOX** stacks children top-to-bottom.
- **HBOX** stacks children left-to-right.
- **FORM** pairs children into label+control rows.
- **GROUP** insets children inside a bordered frame, stacking like VBOX.
- **TABS/TAB** -- each TAB acts as a container for its children.
- **HSPLIT/VSPLIT** -- children use `MKGUI_REGION_*` flags to select their pane.

### Natural sizing

When `h` is 0, widgets use a natural height derived from the font height. Most leaf widgets (buttons, labels, inputs, checkboxes, radios, dropdowns, spinboxes, etc.) have sensible natural heights. This means you rarely need to specify `h` explicitly.

When `w` is 0 in a flex container, the widget stretches to fill available space along the layout axis.

### Margins

The `margin_l`, `margin_r`, `margin_t`, `margin_b` fields add spacing around a widget, inset from where the container would normally place it. Margins do not affect siblings -- they only shrink the widget's own allocated area.

### Common patterns

```c
// Menu, toolbar, and statusbar are auto-positioned by the layout engine.
// Just parent them to the window:
{ MKGUI_MENU,      ID_MENU, "",  "", ID_WINDOW, 0, 0, 0, 0, 0 },
{ MKGUI_TOOLBAR,   ID_TB,   "",  "", ID_WINDOW, 0, 0, 0, 0, 0 },
{ MKGUI_STATUSBAR, ID_SB,   "",  "", ID_WINDOW, 0, 0, 0, 0, 0 },

// Other children of the window fill the remaining content area:
{ MKGUI_TABS, ID_TABS, "", "", ID_WINDOW, 0, 0, 0, 0, 0 },

// A simple form inside a tab:
{ MKGUI_FORM,  ID_FORM, "", "", ID_TAB1, 0, 0, 0, 0, 0 },
{ MKGUI_LABEL, ID_L1,   "Name:",  "", ID_FORM, 0, 0, 0, 0, 0 },
{ MKGUI_INPUT, ID_I1,   "",       "", ID_FORM, 0, 0, 0, 0, 0 },
```

## Flags

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
| `MKGUI_SLIDER_MIXER` | `1 << 19` | Tapered volume style with meter support for slider |
| `MKGUI_VERTICAL` | `1 << 30` | Vertical orientation for scrollbar, slider |
| `MKGUI_IMAGE_STRETCH` | `1 << 20` | Stretch image to fill widget area |
| `MKGUI_SCROLL` | `1 << 21` | Enable scrolling on VBOX (adds scrollbar when content overflows) |
| `MKGUI_NO_PAD` | `1 << 22` | Suppress automatic container padding |
| `MKGUI_TAB_CLOSABLE` | `1 << 23` | Show close button on tab (emits `MKGUI_EVENT_TAB_CLOSE`) |
| `MKGUI_MULTI_SELECT` | `1 << 24` | Enable multi-selection on listview |
| `MKGUI_TRUNCATE` | `1 << 31` | Truncate label text with "..." when it exceeds widget width |

### Cross-axis alignment flags

Used on children of HBOX/VBOX to control alignment perpendicular to the layout direction. Default is stretch (fill container).

| Flag | Value | Description |
|------|-------|-------------|
| `MKGUI_ALIGN_START` | `1 << 25` | Align to start (left in VBOX, top in HBOX) |
| `MKGUI_ALIGN_CENTER` | `2 << 25` | Center in cross-axis |
| `MKGUI_ALIGN_END` | `3 << 25` | Align to end (right in VBOX, bottom in HBOX) |

### Layout flag

| Flag | Value | Description |
|------|-------|-------------|
| `MKGUI_FIXED` | `1 << 27` | Fixed size in container layout. Widget keeps its declared dimensions; weight is ignored. |

### Toolbar display mode flags

Set on `MKGUI_TOOLBAR` widgets to control how buttons are rendered:

| Flag | Value | Description |
|------|-------|-------------|
| `MKGUI_TOOLBAR_ICONS_TEXT` | `0` | Show both icons and text (default) |
| `MKGUI_TOOLBAR_ICONS_ONLY` | `1 << 28` | Show only icons |
| `MKGUI_TOOLBAR_TEXT_ONLY` | `2 << 28` | Show only text |

Change at runtime with `mkgui_toolbar_set_mode()`. The toolbar height adapts automatically.

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
| `MKGUI_EVENT_LISTVIEW_DBLCLICK` | Row double-clicked | row index | column index |
| `MKGUI_EVENT_LISTVIEW_SORT` | Column header clicked | sort direction | column index |
| `MKGUI_EVENT_LISTVIEW_COL_REORDER` | Column dragged to new position | -- | -- |
| `MKGUI_EVENT_LISTVIEW_REORDER` | Row dragged to new position | source row | target row |
| `MKGUI_EVENT_TREEVIEW_SELECT` | Tree node selected | node id | -- |
| `MKGUI_EVENT_TREEVIEW_EXPAND` | Tree node expanded | node id | -- |
| `MKGUI_EVENT_TREEVIEW_COLLAPSE` | Tree node collapsed | node id | -- |
| `MKGUI_EVENT_SPLIT_MOVED` | Splitter dragged | -- | -- |
| `MKGUI_EVENT_KEY` | Key pressed | keysym, keymod | -- |
| `MKGUI_EVENT_ITEMVIEW_SELECT` | Item selected | item index | -- |
| `MKGUI_EVENT_ITEMVIEW_DBLCLICK` | Item double-clicked | item index | -- |
| `MKGUI_EVENT_SCROLL` | Scrollbar value changed | scroll position | -- |
| `MKGUI_EVENT_CONTEXT` | Right-click on widget | row/node/item (-1 if empty) for views, mouse x otherwise | column for views, mouse y otherwise |
| `MKGUI_EVENT_CONTEXT_HEADER` | Right-click on column header | mouse x | column index |
| `MKGUI_EVENT_CONTEXT_MENU` | Context menu item selected | item id | checked state (0/1) |
| `MKGUI_EVENT_GRIDVIEW_SELECT` | Grid cell selected | row | column |
| `MKGUI_EVENT_GRIDVIEW_REORDER` | Row drag-and-drop reorder | source row | target row |
| `MKGUI_EVENT_RICHLIST_SELECT` | Rich list row selected | row | -- |
| `MKGUI_EVENT_RICHLIST_DBLCLICK` | Rich list row double-clicked | row | -- |
| `MKGUI_EVENT_INPUT_SUBMIT` | Enter pressed in input | -- | -- |
| `MKGUI_EVENT_FOCUS` | Widget gained focus | -- | -- |
| `MKGUI_EVENT_UNFOCUS` | Widget lost focus | -- | -- |
| `MKGUI_EVENT_HOVER_ENTER` | Mouse entered widget | -- | -- |
| `MKGUI_EVENT_HOVER_LEAVE` | Mouse left widget | -- | -- |
| `MKGUI_EVENT_SLIDER_START` | Slider drag started | initial value | -- |
| `MKGUI_EVENT_SLIDER_END` | Slider drag ended | final value | -- |
| `MKGUI_EVENT_TREEVIEW_DBLCLICK` | Tree node double-clicked | node id | -- |
| `MKGUI_EVENT_TEXTAREA_CURSOR` | Textarea cursor moved | cursor position | -- |
| `MKGUI_EVENT_DRAG_START` | DnD drag started | source id/row | -- |
| `MKGUI_EVENT_DRAG_END` | DnD drag cancelled | -- | -- |
| `MKGUI_EVENT_TREEVIEW_MOVE` | Tree node moved via DnD | source node id | target node id |
| `MKGUI_EVENT_TAB_CLOSE` | Tab close button clicked | tab id | -- |
| `MKGUI_EVENT_TIMER` | Timer fired (when cb is NULL) | timer id | -- |

## API reference

### Lifecycle

```c
struct mkgui_ctx *mkgui_create(struct mkgui_widget *widgets, uint32_t count);
void mkgui_destroy(struct mkgui_ctx *ctx);
void mkgui_run(struct mkgui_ctx *ctx, mkgui_event_cb cb, void *userdata);
void mkgui_quit(struct mkgui_ctx *ctx);
uint32_t mkgui_add_timer(struct mkgui_ctx *ctx, uint64_t interval_ns, mkgui_timer_cb cb, void *userdata);
void mkgui_remove_timer(struct mkgui_ctx *ctx, uint32_t timer_id);
```

`mkgui_create` takes a widget array and returns a context.

`mkgui_run` runs the event loop. It drains all pending events, delivers each to `cb`, renders the frame, then blocks until the next event or timer. This repeats until `mkgui_quit` is called.

`mkgui_run` handles frame pacing automatically:
- When animated widgets are visible (spinners, progress bars, GL views), it runs at ~60fps
- When idle, it blocks indefinitely until an event arrives (zero CPU usage)
- It wakes immediately on platform events (mouse, keyboard, expose) or timer expiration

On Linux, blocking uses `poll()` on the X11 connection fd and timer fds. On Windows, it uses `MsgWaitForMultipleObjects`.

Mouse motion events are automatically compressed. When multiple consecutive motion events are queued, only the last position is delivered.

**Example:**

```c
static void on_event(struct mkgui_ctx *ctx, struct mkgui_event *ev, void *userdata) {
    (void)userdata;
    switch(ev->type) {
        case MKGUI_EVENT_CLOSE: {
            mkgui_quit(ctx);
        } break;

        case MKGUI_EVENT_CLICK: {
            // handle click
        } break;
    }
}

int main(void) {
    struct mkgui_ctx *ctx = mkgui_create(widgets, count);
    mkgui_run(ctx, on_event, NULL);
    mkgui_destroy(ctx);
}
```

Dialogs (`mkgui_message_box`, `mkgui_open_dialog`, etc.) work normally when called from the event callback -- they run their own nested event loop internally.

#### Timers

```c
typedef void (*mkgui_timer_cb)(struct mkgui_ctx *ctx, uint32_t timer_id, void *userdata);

uint32_t mkgui_add_timer(struct mkgui_ctx *ctx, uint64_t interval_ns, mkgui_timer_cb cb, void *userdata);
void mkgui_remove_timer(struct mkgui_ctx *ctx, uint32_t timer_id);
```

`mkgui_add_timer` creates a repeating timer with nanosecond-resolution interval. Returns a timer ID (0 on failure). Up to `MKGUI_MAX_TIMERS` (8) timers per context.

If `cb` is non-NULL, the callback fires during event processing. If `cb` is NULL, an `MKGUI_EVENT_TIMER` event is delivered to the event callback instead (with `ev->id` set to the timer ID).

Timers do not drift. On Linux, `timerfd_create(CLOCK_MONOTONIC)` provides kernel-managed absolute deadlines. On Windows, `SetWaitableTimer` with absolute re-arming after each firing prevents accumulation error.

Call `mkgui_remove_timer` inside the callback to make a one-shot timer.

**Example** (periodic data refresh at 30Hz):

```c
void refresh(struct mkgui_ctx *ctx, uint32_t timer_id, void *data) {
    update_data();
    update_widgets(ctx);
}

mkgui_add_timer(ctx, 33333333, refresh, NULL);  // 33.3ms = 30Hz
```


### Runtime widget management

```c
uint32_t mkgui_add_widget(struct mkgui_ctx *ctx, struct mkgui_widget w, uint32_t after_id);
uint32_t mkgui_remove_widget(struct mkgui_ctx *ctx, uint32_t id);
```

`mkgui_add_widget` inserts a widget into the context at runtime. The widget's `parent_id` determines which container it belongs to. The `after_id` parameter controls where it appears among its siblings:

| `after_id` | Behavior |
|------------|----------|
| `0` | Append as the last widget in the array |
| parent's ID | Insert as the first child of that container |
| sibling's ID | Insert immediately after that sibling |

Returns 1 on success, 0 on allocation failure. The widget's aux data (slider state, input buffer, etc.) is automatically initialized. All arrays grow dynamically as needed.

`mkgui_remove_widget` removes a widget and all its descendants. It cleans up aux data (frees textarea buffers, treeview nodes, image pixels, GL views), clears any references in focus/hover/drag state, and destroys open popups for the removed widgets. Returns 1 on success, 0 if the widget was not found. Removing a MKGUI_WINDOW widget is not allowed.

**Important:** Pointers returned by `find_widget()` or any `find_*_data()` function are invalidated after calling `mkgui_add_widget` or `mkgui_remove_widget`, because the underlying arrays may be reallocated. Always re-query after adding or removing widgets.

```c
// add a slider as the first child of a vbox
struct mkgui_widget slider = { MKGUI_SLIDER, 500, "Volume", "", VBOX_ID, 0, 0, 0, 0, 0 };
mkgui_add_widget(ctx, slider, VBOX_ID);

// add a button after an existing sibling
struct mkgui_widget btn = { MKGUI_BUTTON, 501, "Mute", "", VBOX_ID, 80, 0, MKGUI_FIXED, 0, 0 };
mkgui_add_widget(ctx, btn, 500);  // after the slider

// remove the slider and anything nested inside it
mkgui_remove_widget(ctx, 500);
```

### Performance timing

After each frame, the context contains timing data:

```c
ctx->perf_layout_us   // layout calculation time in microseconds
ctx->perf_render_us   // widget rendering time in microseconds
ctx->perf_blit_us     // framebuffer blit time in microseconds
```

### Utilities

```c
void mkgui_sleep_ms(uint32_t ms);
uint32_t mkgui_time_ms(void);
double mkgui_time_us(void);
```

`mkgui_sleep_ms` -- cross-platform sleep helper. `mkgui_time_ms` -- monotonic clock in milliseconds (wraps at ~49 days). `mkgui_time_us` -- monotonic clock in microseconds (double precision). Not needed for normal mainloops but available for custom timing.

### Base widget properties

These functions work on any widget type:

```c
void mkgui_set_enabled(struct mkgui_ctx *ctx, uint32_t id, uint32_t enabled);
uint32_t mkgui_get_enabled(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_set_visible(struct mkgui_ctx *ctx, uint32_t id, uint32_t visible);
uint32_t mkgui_get_visible(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_set_focus(struct mkgui_ctx *ctx, uint32_t id);
uint32_t mkgui_has_focus(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_get_geometry(struct mkgui_ctx *ctx, uint32_t id, int32_t *x, int32_t *y, int32_t *w, int32_t *h);
void mkgui_set_flags(struct mkgui_ctx *ctx, uint32_t id, uint32_t flags);
uint32_t mkgui_get_flags(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_set_tooltip(struct mkgui_ctx *ctx, uint32_t id, const char *text);
const char *mkgui_get_tooltip(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_set_weight(struct mkgui_ctx *ctx, uint32_t id, uint32_t weight);
void mkgui_set_icon(struct mkgui_ctx *ctx, uint32_t widget_id, const char *icon_name);
```

- `set_enabled` / `get_enabled` -- toggle `MKGUI_DISABLED` flag. Disabling clears focus.
- `set_visible` / `get_visible` -- toggle `MKGUI_HIDDEN` flag. Hiding clears focus.
- `set_focus` -- programmatic focus. Only works on focusable, enabled widgets.
- `get_geometry` -- returns the widget's computed layout rect (after layout). Any pointer may be NULL.
- `set_flags` / `get_flags` -- raw flag access. Use for toggling multiple flags at once.

### Button

```c
void mkgui_button_set_text(struct mkgui_ctx *ctx, uint32_t id, const char *text);
const char *mkgui_button_get_text(struct mkgui_ctx *ctx, uint32_t id);
```

**Toggle button**: Set `MKGUI_CHECKED` on a button to render it sunken (pressed state). Toggle the flag in your click handler to create an on/off button:

```c
// Icon-only buttons center the icon automatically when label is empty
{ MKGUI_BUTTON, ID_MUTE, "", "volume-high", ID_VBOX, 28, 28, MKGUI_FIXED | MKGUI_ALIGN_CENTER, 0, 0 },

// In event callback:
case MKGUI_EVENT_CLICK: {
    if(ev->id == ID_MUTE) {
        uint32_t f = mkgui_get_flags(ctx, ID_MUTE);
        f ^= MKGUI_CHECKED;
        mkgui_set_flags(ctx, ID_MUTE, f);
        mkgui_set_icon(ctx, ID_MUTE, (f & MKGUI_CHECKED) ? "volume-off" : "volume-high");
    }
} break;
```

### Label

```c
void mkgui_label_set(struct mkgui_ctx *ctx, uint32_t id, const char *text);
const char *mkgui_label_get(struct mkgui_ctx *ctx, uint32_t id);
```

Set `MKGUI_TRUNCATE` to clip text that overflows the widget width, replacing the tail with "...". Without this flag, text is hard-clipped at the widget boundary. The full text is preserved in the widget -- only the rendered output is affected.

```c
{ MKGUI_LABEL, ID_NAME, "Very Long Channel Name", "", ID_PARENT, 80, 0,
  MKGUI_FIXED, MKGUI_TRUNCATE, 0 }
```

#### Link label

Set `MKGUI_LINK` to make a label clickable. The text renders in the selection color with an underline. Hover changes color. Generates `MKGUI_EVENT_CLICK` when clicked.

```c
{ MKGUI_LABEL, ID_LINK, "Visit website", "", ID_PARENT, 0, 0, MKGUI_FIXED, MKGUI_LINK, 0 }
```

#### Wrapping label

Set `MKGUI_WRAP` to word-wrap text across multiple lines. The label renders from the top and wraps at word boundaries to fit the widget width. Set an explicit height or use flex fill.

```c
{ MKGUI_LABEL, ID_DESC, "Long description text here...", "", ID_PARENT, 0, 80, MKGUI_FIXED, MKGUI_WRAP, 0 }
```

### Input

```c
void mkgui_input_set(struct mkgui_ctx *ctx, uint32_t id, const char *text);
const char *mkgui_input_get(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_input_clear(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_input_set_readonly(struct mkgui_ctx *ctx, uint32_t id, uint32_t readonly);
uint32_t mkgui_input_get_readonly(struct mkgui_ctx *ctx, uint32_t id);
uint32_t mkgui_input_get_cursor(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_input_set_cursor(struct mkgui_ctx *ctx, uint32_t id, uint32_t pos);
void mkgui_input_select_all(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_input_get_selection(struct mkgui_ctx *ctx, uint32_t id, uint32_t *start, uint32_t *end);
void mkgui_input_set_selection(struct mkgui_ctx *ctx, uint32_t id, uint32_t start, uint32_t end);
```

Double-click on an input widget selects all text. Single click positions the cursor. Click-drag selects a range.

Supports Ctrl+C (copy) and Ctrl+V (paste). Newlines in pasted text are replaced with spaces. Copy is disabled when `MKGUI_PASSWORD` is set. Paste is disabled when `MKGUI_READONLY` is set.

`input_clear` resets text, cursor, and selection. `input_set_readonly` / `input_get_readonly` toggle the `MKGUI_READONLY` flag at runtime. Cursor and selection positions are byte offsets into the text. Text is stored as UTF-8. Cursor navigation (left/right, backspace, delete) is UTF-8 aware -- multi-byte characters are treated as single units. The glyph range covers Latin-1 (codepoints 32-255), supporting Western European languages.

#### Numeric input

Set `MKGUI_NUMERIC` to filter keyboard input to digits, decimal point, sign, and scientific notation (e/E). Non-numeric characters are rejected.

```c
{ MKGUI_INPUT, ID_AMOUNT, "", "", ID_FORM, 0, 0, 0, MKGUI_NUMERIC, 0 }
```

### Checkbox

```c
uint32_t mkgui_checkbox_get(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_checkbox_set(struct mkgui_ctx *ctx, uint32_t id, uint32_t checked);
```

### Radio

```c
uint32_t mkgui_radio_get(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_radio_set(struct mkgui_ctx *ctx, uint32_t id, uint32_t checked);
```

Radio buttons are mutually exclusive within the same parent. Clicking one unchecks siblings. `radio_set` with `checked=1` automatically unchecks all other radios in the same parent group.

### Toggle

```c
void mkgui_toggle_set(struct mkgui_ctx *ctx, uint32_t id, uint32_t state);
uint32_t mkgui_toggle_get(struct mkgui_ctx *ctx, uint32_t id);
```

On/off toggle switch. Emits `MKGUI_EVENT_TOGGLE_CHANGED`.

### Dropdown

```c
void mkgui_dropdown_setup(struct mkgui_ctx *ctx, uint32_t id, const char **items, uint32_t count);
int32_t mkgui_dropdown_get(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_dropdown_set(struct mkgui_ctx *ctx, uint32_t id, int32_t index);
const char *mkgui_dropdown_get_text(struct mkgui_ctx *ctx, uint32_t id);
uint32_t mkgui_dropdown_get_count(struct mkgui_ctx *ctx, uint32_t id);
const char *mkgui_dropdown_get_item_text(struct mkgui_ctx *ctx, uint32_t id, uint32_t index);
void mkgui_dropdown_add(struct mkgui_ctx *ctx, uint32_t id, const char *text);
void mkgui_dropdown_remove(struct mkgui_ctx *ctx, uint32_t id, uint32_t index);
void mkgui_dropdown_clear(struct mkgui_ctx *ctx, uint32_t id);
```

`dropdown_set` selects an item by index (-1 to deselect). `dropdown_add` / `dropdown_remove` / `dropdown_clear` modify items at runtime without a full re-setup. Maximum 64 items.

### ComboBox

```c
void mkgui_combobox_setup(struct mkgui_ctx *ctx, uint32_t id, const char **items, uint32_t count);
int32_t mkgui_combobox_get(struct mkgui_ctx *ctx, uint32_t id);
const char *mkgui_combobox_get_text(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_combobox_set(struct mkgui_ctx *ctx, uint32_t id, int32_t index);
void mkgui_combobox_set_text(struct mkgui_ctx *ctx, uint32_t id, const char *text);
uint32_t mkgui_combobox_get_count(struct mkgui_ctx *ctx, uint32_t id);
const char *mkgui_combobox_get_item_text(struct mkgui_ctx *ctx, uint32_t id, uint32_t index);
void mkgui_combobox_add(struct mkgui_ctx *ctx, uint32_t id, const char *text);
void mkgui_combobox_remove(struct mkgui_ctx *ctx, uint32_t id, uint32_t index);
void mkgui_combobox_clear(struct mkgui_ctx *ctx, uint32_t id);
```

Editable dropdown with text input and filtering. The user can type to filter the item list or enter custom text. `combobox_get` returns the selected item index (-1 if custom text). `combobox_get_text` returns the current text (whether from the list or typed). Emits `MKGUI_EVENT_COMBOBOX_CHANGED` on selection and `MKGUI_EVENT_COMBOBOX_SUBMIT` on Enter.

### Slider

```c
void mkgui_slider_setup(struct mkgui_ctx *ctx, uint32_t id, int32_t min_val, int32_t max_val, int32_t value);
int32_t mkgui_slider_get(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_slider_set(struct mkgui_ctx *ctx, uint32_t id, int32_t value);
void mkgui_slider_get_range(struct mkgui_ctx *ctx, uint32_t id, int32_t *min_val, int32_t *max_val);
void mkgui_slider_set_range(struct mkgui_ctx *ctx, uint32_t id, int32_t min_val, int32_t max_val);
void mkgui_slider_set_meter(struct mkgui_ctx *ctx, uint32_t id, float pre, float post, uint32_t pre_color, uint32_t post_color);
```

The slider renders a horizontal track with a draggable thumb. Set `MKGUI_VERTICAL` for vertical orientation. It does not display any text -- add your own label and/or value display if needed (e.g. using a FORM row or an HBOX with a label widget).

`slider_set` changes the value without a full re-setup. `slider_set_range` changes min/max at runtime (clamps value). `slider_get_range` reads the current min/max. Any pointer may be NULL.

Set `MKGUI_SLIDER_MIXER` for a tapered volume-style slider (like the Windows volume mixer). The track becomes a triangle shape, narrow at the quiet end and wide at the loud end. Use `mkgui_slider_set_meter()` to overlay RMS/peak meter fill colors on the mixer track.

Meter values are 0.0--1.0 (clamped). The `pre` layer is drawn first, then `post` on top. Only active when `MKGUI_SLIDER_MIXER` is set.

### Spinbox

```c
void mkgui_spinbox_setup(struct mkgui_ctx *ctx, uint32_t id, int32_t min_val, int32_t max_val, int32_t value, int32_t step);
int32_t mkgui_spinbox_get(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_spinbox_set(struct mkgui_ctx *ctx, uint32_t id, int32_t value);
void mkgui_spinbox_set_range(struct mkgui_ctx *ctx, uint32_t id, int32_t min_val, int32_t max_val);
void mkgui_spinbox_get_range(struct mkgui_ctx *ctx, uint32_t id, int32_t *min_val, int32_t *max_val);
void mkgui_spinbox_set_step(struct mkgui_ctx *ctx, uint32_t id, int32_t step);
```

Click the text area to enter edit mode (cursor appears, existing value is pre-filled). Type a new value and press Enter to confirm (clears focus) or Escape to cancel. Use up/down arrow keys or the +/- buttons to step.

`spinbox_set_range` changes min/max at runtime (clamps value). `spinbox_get_range` reads current min/max (any pointer may be NULL). `spinbox_set_step` changes the step size.

### Progress

```c
void mkgui_progress_setup(struct mkgui_ctx *ctx, uint32_t id, int32_t max_val);
void mkgui_progress_set(struct mkgui_ctx *ctx, uint32_t id, int32_t value);
int32_t mkgui_progress_get(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_progress_set_range(struct mkgui_ctx *ctx, uint32_t id, int32_t max_val);
void mkgui_progress_get_range(struct mkgui_ctx *ctx, uint32_t id, int32_t *max_val);
void mkgui_progress_set_color(struct mkgui_ctx *ctx, uint32_t id, uint32_t color);
```

Renders a filled bar with an animated diagonal shimmer sweep while in progress. The fill color defaults to `theme.accent`. Use `progress_set_color` to override the bar color per-widget; pass `0` to revert to the theme default. The percentage text is centered on the bar. `progress_set_range` changes max at runtime (clamps value).

### Meter

```c
void mkgui_meter_setup(struct mkgui_ctx *ctx, uint32_t id, int32_t max_val);
void mkgui_meter_set(struct mkgui_ctx *ctx, uint32_t id, int32_t value);
int32_t mkgui_meter_get(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_meter_set_range(struct mkgui_ctx *ctx, uint32_t id, int32_t max_val);
void mkgui_meter_get_range(struct mkgui_ctx *ctx, uint32_t id, int32_t *max_val);
void mkgui_meter_set_zones(struct mkgui_ctx *ctx, uint32_t id, int32_t t1, int32_t t2, uint32_t c1, uint32_t c2, uint32_t c3);
```

Level meter with colored zones showing capacity/usage. The background displays three color zones at fixed thresholds, and a thinner bar on top fills from the start to the current value. The thin bar takes the color of whichever zone the current value falls in. Set `MKGUI_VERTICAL` for a vertical meter (zones bottom-to-top). Set `MKGUI_METER_TEXT` to display a centered percentage.

`meter_setup` initializes with default zone thresholds at 75% and 90% of max, with green/yellow/red colors. `meter_set_zones` overrides thresholds (`t1`, `t2` as absolute values, not percentages) and colors (`c1`, `c2`, `c3` as ARGB). `meter_set_range` changes max at runtime (clamps value).

### Spinner

```c
{ MKGUI_SPINNER, ID_SPIN, "", "", ID_PARENT, 24, 24, MKGUI_FIXED, 0, 0 }
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
void mkgui_listview_set_selected(struct mkgui_ctx *ctx, uint32_t id, int32_t row);
uint32_t mkgui_listview_get_multi_sel(struct mkgui_ctx *ctx, uint32_t id, const int32_t **out);
uint32_t mkgui_listview_is_selected(struct mkgui_ctx *ctx, uint32_t id, int32_t row);
void mkgui_listview_clear_selection(struct mkgui_ctx *ctx, uint32_t id);
const uint32_t *mkgui_listview_get_col_order(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_listview_set_col_order(struct mkgui_ctx *ctx, uint32_t id, const uint32_t *order, uint32_t count);
int32_t mkgui_listview_get_col_width(struct mkgui_ctx *ctx, uint32_t id, uint32_t col);
void mkgui_listview_set_col_width(struct mkgui_ctx *ctx, uint32_t id, uint32_t col, int32_t width);
void mkgui_listview_set_cell_type(struct mkgui_ctx *ctx, uint32_t id, uint32_t col, uint32_t cell_type);
void mkgui_listview_visible_range(struct mkgui_ctx *ctx, uint32_t id, int32_t *first, int32_t *last);
void mkgui_listview_scroll_to(struct mkgui_ctx *ctx, uint32_t id, int32_t row);
void mkgui_listview_get_sort(struct mkgui_ctx *ctx, uint32_t id, int32_t *col, int32_t *dir);
void mkgui_listview_set_sort(struct mkgui_ctx *ctx, uint32_t id, int32_t col, int32_t dir);
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

Column headers support drag-and-drop reordering and resize. Drag a header to a new position to rearrange columns. A short click (< 4px movement) sorts by that column instead. Drag the divider between columns to resize (minimum 40px, cursor changes to a resize arrow on hover). Double-click a column divider to auto-fit the column width to its contents (measures the header label and all rows via the row callback). The column order maps display positions to logical column indices -- the row callback always receives the logical column index regardless of display order. Use `mkgui_listview_get_col_order` / `mkgui_listview_set_col_order` and `mkgui_listview_get_col_width` / `mkgui_listview_set_col_width` to save and restore layouts.

Sorting is application-side. When a header is clicked, `MKGUI_EVENT_LISTVIEW_SORT` is emitted with `ev->col` set to the column and `ev->value` set to the sort direction (1 ascending, -1 descending). The listview tracks the sort arrow visual internally. The application should sort its data in response -- the row callback will be called again on the next frame.

### Gridview

Multi-column grid with per-cell checkbox support. Ideal for patchbays, routing matrices, and tabular data with toggles.

```c
typedef void (*mkgui_grid_cell_cb)(uint32_t row, uint32_t col, char *buf, uint32_t buf_size, void *userdata);

void mkgui_gridview_setup(struct mkgui_ctx *ctx, uint32_t id, uint32_t row_count, uint32_t col_count,
    struct mkgui_grid_column *columns, mkgui_grid_cell_cb cell_cb, void *userdata);
void mkgui_gridview_set_rows(struct mkgui_ctx *ctx, uint32_t id, uint32_t row_count);
int32_t mkgui_gridview_get_selected(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_gridview_set_selected(struct mkgui_ctx *ctx, uint32_t id, int32_t row);
uint32_t mkgui_gridview_get_check(struct mkgui_ctx *ctx, uint32_t id, uint32_t row, uint32_t col);
void mkgui_gridview_set_check(struct mkgui_ctx *ctx, uint32_t id, uint32_t row, uint32_t col, uint32_t checked);
int32_t mkgui_gridview_get_col_width(struct mkgui_ctx *ctx, uint32_t id, uint32_t col);
void mkgui_gridview_set_col_width(struct mkgui_ctx *ctx, uint32_t id, uint32_t col, int32_t width);
void mkgui_gridview_scroll_to(struct mkgui_ctx *ctx, uint32_t id, int32_t row);
```

Column types (set in `struct mkgui_grid_column`):

| Type | Description |
|------|-------------|
| `MKGUI_GRID_TEXT` | Plain text cell (data from callback) |
| `MKGUI_GRID_CHECK` | Checkbox-only cell (state owned by gridview) |
| `MKGUI_GRID_CHECK_TEXT` | Checkbox + text (checkbox state owned by gridview, text from callback) |

The cell callback is called for `MKGUI_GRID_TEXT` and `MKGUI_GRID_CHECK_TEXT` columns only. Checkbox state is managed internally by the widget and accessed via `mkgui_gridview_get_check()` / `mkgui_gridview_set_check()`.

Events:

- `MKGUI_EVENT_GRID_CLICK` -- cell selected. `ev->value` = row, `ev->col` = column.
- `MKGUI_EVENT_GRID_CHECK` -- checkbox toggled. `ev->value` = row, `ev->col` = column. Read new state with `mkgui_gridview_get_check()`.
- `MKGUI_EVENT_GRIDVIEW_REORDER` -- row drag-and-drop reorder. `ev->value` = source row, `ev->col` = target row. Drag activates after 4px vertical mouse movement. A drop indicator line is drawn at the target position during drag.
- `MKGUI_EVENT_DRAG_START` -- drag started. `ev->value` = source row.
- `MKGUI_EVENT_DRAG_END` -- drag cancelled (no valid drop target).

Grid lines are drawn between cells. Keyboard navigation: arrow keys to move selection, Space to toggle checkbox in the selected cell.

#### Column resize

Column headers support manual resize and auto-fit. Drag the divider between column headers to resize (minimum 40px, cursor changes to a resize arrow on hover). Double-click a column divider to auto-fit the column width to its contents (measures the header label and all rows via the cell callback).

### RichList

A list widget where each row displays a thumbnail, title, subtitle, and right-aligned text. Suitable for media players, email clients, and similar applications with rich row content.

```c
struct mkgui_richlist_row {
    char title[MKGUI_MAX_TEXT];
    char subtitle[MKGUI_MAX_TEXT];
    char right_text[64];
    uint32_t *thumbnail;      // ARGB pixels, NULL for no thumbnail
    int32_t thumb_w, thumb_h; // source thumbnail dimensions
};

typedef void (*mkgui_richlist_cb)(uint32_t row, struct mkgui_richlist_row *out, void *userdata);

void mkgui_richlist_setup(struct mkgui_ctx *ctx, uint32_t id, uint32_t row_count, int32_t row_height, mkgui_richlist_cb cb, void *userdata);
void mkgui_richlist_set_rows(struct mkgui_ctx *ctx, uint32_t id, uint32_t row_count);
int32_t mkgui_richlist_get_selected(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_richlist_set_selected(struct mkgui_ctx *ctx, uint32_t id, int32_t row);
void mkgui_richlist_scroll_to(struct mkgui_ctx *ctx, uint32_t id, int32_t row);
```

The callback fills a `struct mkgui_richlist_row` for each visible row. The widget renders: thumbnail on the left (scaled to fit row height minus padding), title and subtitle stacked in the center, right-aligned text on the right. Virtual scrolling -- only visible rows are queried.

Row height is configurable (default 56px). Pass 0 for `row_height` to use the default.

Events:

- `MKGUI_EVENT_RICHLIST_SELECT` -- row selected. `ev->value` = row index.
- `MKGUI_EVENT_RICHLIST_DBLCLICK` -- row double-clicked. `ev->value` = row index.

Keyboard navigation: arrow keys, Page Up/Down, Home/End. Right-click emits `MKGUI_EVENT_CONTEXT`.

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
void mkgui_itemview_set_selected(struct mkgui_ctx *ctx, uint32_t id, int32_t item);
void mkgui_itemview_clear_selection(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_itemview_scroll_to(struct mkgui_ctx *ctx, uint32_t id, int32_t item);
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
void mkgui_treeview_select(struct mkgui_ctx *ctx, uint32_t widget_id, int32_t node_id);
int32_t mkgui_treeview_get_selected(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_treeview_remove(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id);
void mkgui_treeview_clear(struct mkgui_ctx *ctx, uint32_t widget_id);
void mkgui_treeview_set_label(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id, const char *label);
const char *mkgui_treeview_get_label(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id);
void mkgui_treeview_expand(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id);
void mkgui_treeview_collapse(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id);
uint32_t mkgui_treeview_is_expanded(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id);
void mkgui_treeview_expand_all(struct mkgui_ctx *ctx, uint32_t widget_id);
void mkgui_treeview_collapse_all(struct mkgui_ctx *ctx, uint32_t widget_id);
uint32_t mkgui_treeview_get_parent(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id);
uint32_t mkgui_treeview_node_count(struct mkgui_ctx *ctx, uint32_t widget_id);
void mkgui_treeview_scroll_to(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id);
void mkgui_set_treenode_icon(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id, const char *icon_name);
```

`parent_node = 0` for root-level nodes. `treeview_remove` removes a node and all its descendants. `treeview_clear` removes all nodes. `expand` / `collapse` / `expand_all` / `collapse_all` control node visibility. `get_parent` returns the parent node ID (0 for root nodes). `scroll_to` ensures a node is visible in the scrollable area.

### Textarea

```c
void mkgui_textarea_set(struct mkgui_ctx *ctx, uint32_t id, const char *text);
const char *mkgui_textarea_get(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_textarea_set_readonly(struct mkgui_ctx *ctx, uint32_t id, uint32_t readonly);
uint32_t mkgui_textarea_get_readonly(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_textarea_get_cursor(struct mkgui_ctx *ctx, uint32_t id, uint32_t *line, uint32_t *col);
void mkgui_textarea_set_cursor(struct mkgui_ctx *ctx, uint32_t id, uint32_t line, uint32_t col);
uint32_t mkgui_textarea_get_line_count(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_textarea_get_selection(struct mkgui_ctx *ctx, uint32_t id, uint32_t *start, uint32_t *end);
void mkgui_textarea_insert(struct mkgui_ctx *ctx, uint32_t id, const char *text);
void mkgui_textarea_append(struct mkgui_ctx *ctx, uint32_t id, const char *text);
void mkgui_textarea_scroll_to_end(struct mkgui_ctx *ctx, uint32_t id);
```

Supports Ctrl+C (copy) and Ctrl+V (paste). Uses the system clipboard (X11 CLIPBOARD selection / Win32 clipboard).

`textarea_set_readonly` / `textarea_get_readonly` toggle the `MKGUI_READONLY` flag at runtime. Cursor position is returned as line/col (0-based). Selection start/end are byte offsets. `textarea_insert` inserts text at the cursor (replacing any selection). `textarea_append` appends to the end without moving the cursor. `textarea_scroll_to_end` scrolls to the bottom -- useful for log views. Text is stored as UTF-8. Cursor navigation (left/right, backspace, delete) is UTF-8 aware -- multi-byte characters are treated as single units.

### Statusbar

```c
void mkgui_statusbar_setup(struct mkgui_ctx *ctx, uint32_t id, uint32_t section_count, const int32_t *widths);
void mkgui_statusbar_set(struct mkgui_ctx *ctx, uint32_t id, uint32_t section, const char *text);
const char *mkgui_statusbar_get(struct mkgui_ctx *ctx, uint32_t id, uint32_t section);
void mkgui_statusbar_clear(struct mkgui_ctx *ctx, uint32_t id, uint32_t section);
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
{ MKGUI_CANVAS, ID_CANVAS, "", "", ID_PARENT, 0, 0, 0, MKGUI_PANEL_BORDER, 0 }
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
{ MKGUI_MENU,     ID_MENU,  "",     "",             ID_WINDOW, 0, 0, 0, 0, 0 },
{ MKGUI_MENUITEM, ID_FILE,  "File", "",             ID_MENU,   0, 0, 0, 0, 0 },
{ MKGUI_MENUITEM, ID_OPEN,  "Open", "folder-open",  ID_FILE,   0, 0, 0, 0, 0 },
{ MKGUI_MENUITEM, ID_SAVE,  "Save", "content-save", ID_FILE,   0, 0, 0, 0, 0 },
{ MKGUI_MENUITEM, ID_SEP1,  "",     "",             ID_FILE,   0, 0, 0, MKGUI_SEPARATOR, 0 },
{ MKGUI_MENUITEM, ID_EXIT,  "Exit", "",             ID_FILE,   0, 0, 0, 0, 0 },
```

`MKGUI_SEPARATOR` makes the item render as a horizontal separator line. Use a dedicated menu item with an empty label and the `MKGUI_SEPARATOR` flag to create visual grouping between menu entries.

Check/radio menu items:

```c
{ MKGUI_MENUITEM, ID_SHOW_TB, "Show Toolbar", "", ID_VIEW, 0, 0, 0, MKGUI_MENU_CHECK | MKGUI_CHECKED, 0 },
{ MKGUI_MENUITEM, ID_SORT_NAME, "By Name",    "", ID_SORT, 0, 0, 0, MKGUI_MENU_RADIO | MKGUI_CHECKED, 0 },
{ MKGUI_MENUITEM, ID_SORT_SIZE, "By Size",    "", ID_SORT, 0, 0, 0, MKGUI_MENU_RADIO, 0 },
```

Check items toggle on click. Radio items auto-uncheck siblings. Only leaf items (no children) emit `MKGUI_EVENT_MENU`.

## Toolbar

Toolbar children are `MKGUI_BUTTON` widgets. Use `MKGUI_TOOLBAR_SEP` on a button to draw a separator before it:

```c
{ MKGUI_TOOLBAR, ID_TB,      "",     "",             ID_WINDOW, 0, 0, 0, 0, 0 },
{ MKGUI_BUTTON,  ID_TB_NEW,  "New",  "file-plus",    ID_TB,     0, 0, 0, 0, 0 },
{ MKGUI_BUTTON,  ID_TB_OPEN, "Open", "folder-open",  ID_TB,     0, 0, MKGUI_TOOLBAR_SEP, 0, 0 },
{ MKGUI_BUTTON,  ID_TB_SAVE, "Save", "content-save", ID_TB,     0, 0, 0, 0, 0 },
```

Toolbar buttons render flat (Breeze style) -- no border when idle, highlighted on hover, pressed on click. They emit `MKGUI_EVENT_CLICK`.

### Display modes

```c
void mkgui_toolbar_set_mode(struct mkgui_ctx *ctx, uint32_t toolbar_id, uint32_t mode);
```

Switch between icons+text, icons only, or text only at runtime. The toolbar height adapts automatically -- text-only toolbars are shorter, icon toolbars size to the loaded icon dimensions.

```c
mkgui_toolbar_set_mode(ctx, ID_TB, MKGUI_TOOLBAR_ICONS_ONLY);
```

Or set the mode in the widget flags at init time:

```c
{ MKGUI_TOOLBAR, ID_TB, "", "", ID_WINDOW, 0, 0, 0, MKGUI_TOOLBAR_ICONS_ONLY, 0 },
```

### Toolbar icon pack

mkgui supports a separate icon pack for toolbar icons. If `mdi_icons_toolbar.dat` (or `ext/mdi_icons_toolbar.dat`) exists, toolbar buttons use icons from that pack. Otherwise they fall back to the regular `mdi_icons.dat`.

This allows larger icons for icon-only toolbars while keeping the standard 18x18 icons for menus, tabs, and treeviews:

```bash
# Generate 32x32 toolbar icons (only the ones you need)
./tools/gen_icons ext/materialdesignicons-webfont.ttf 32 mdi_icons_toolbar.dat toolbar_icons.txt
```

### DatePicker

```c
void mkgui_datepicker_set(struct mkgui_ctx *ctx, uint32_t id, int32_t year, int32_t month, int32_t day);
void mkgui_datepicker_get(struct mkgui_ctx *ctx, uint32_t id, int32_t *year, int32_t *month, int32_t *day);
const char *mkgui_datepicker_get_text(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_datepicker_set_readonly(struct mkgui_ctx *ctx, uint32_t id, uint32_t readonly);
uint32_t mkgui_datepicker_get_readonly(struct mkgui_ctx *ctx, uint32_t id);
```

Date picker with calendar popup. Click the dropdown arrow to open the calendar. Type directly in the text field to edit the date (YYYY-MM-DD format). Emits `MKGUI_EVENT_DATEPICKER_CHANGED`.

### IP Input

```c
void mkgui_ipinput_set(struct mkgui_ctx *ctx, uint32_t id, const char *ip_string);
const char *mkgui_ipinput_get(struct mkgui_ctx *ctx, uint32_t id);
uint32_t mkgui_ipinput_get_u32(struct mkgui_ctx *ctx, uint32_t id);
```

Four-field IPv4 address input (like Windows IP input). Each octet is a separate editable field with 0--255 validation. Tab/dot moves to the next field. `ipinput_get` returns a dotted-decimal string. `ipinput_get_u32` returns the address as a 32-bit integer (network byte order). Emits `MKGUI_EVENT_IPINPUT_CHANGED`.

### Pathbar

```c
void mkgui_pathbar_set(struct mkgui_ctx *ctx, uint32_t id, const char *path);
const char *mkgui_pathbar_get(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_pathbar_get_segment_path(struct mkgui_ctx *ctx, uint32_t id, uint32_t seg_idx, char *out, uint32_t out_size);
```

Breadcrumb-style path bar. Click a segment to navigate to that directory (emits `MKGUI_EVENT_PATHBAR_NAV` with `ev->value` = segment index). Click the background to enter edit mode (type a path directly, Enter emits `MKGUI_EVENT_PATHBAR_SUBMIT`). `pathbar_get_segment_path` builds the full path up to and including segment `seg_idx`.

### Tabs API

```c
uint32_t mkgui_tabs_get_current(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_tabs_set_current(struct mkgui_ctx *ctx, uint32_t id, uint32_t tab_id);
uint32_t mkgui_tabs_get_count(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_tabs_set_text(struct mkgui_ctx *ctx, uint32_t tabs_id, uint32_t tab_id, const char *text);
const char *mkgui_tabs_get_text(struct mkgui_ctx *ctx, uint32_t tabs_id, uint32_t tab_id);
```

`tabs_get_current` returns the active tab's widget ID. `tabs_set_current` switches to a tab by its widget ID. `tabs_get_count` returns the number of MKGUI_TAB children. `tabs_set_text` / `tabs_get_text` change tab labels at runtime.

## Splitters

```c
{ MKGUI_VSPLIT,   ID_SPLIT, "", "", ID_WIN, 0, 0, 0, 0, 0 },
{ MKGUI_TREEVIEW, ID_TREE,  "", "", ID_SPLIT, 0, 0, MKGUI_REGION_LEFT, 0, 0 },
{ MKGUI_LISTVIEW, ID_LIST,  "", "", ID_SPLIT, 0, 0, MKGUI_REGION_RIGHT, 0, 0 },
```

The divider is draggable. Children sized automatically based on their region flag.

```c
float mkgui_split_get_ratio(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_split_set_ratio(struct mkgui_ctx *ctx, uint32_t id, float ratio);
```

`split_get_ratio` returns the current split position as a 0.0--1.0 ratio. `split_set_ratio` sets it programmatically (clamped to 0.0--1.0).

## Group box

A container that draws a thin rounded border with a title label breaking the top edge. Children are automatically inset inside the frame (top padding accounts for the label height, sides and bottom have a small margin).

```c
{ MKGUI_GROUP,    ID_GRP, "Settings",     "", ID_TAB1, 0, 0, MKGUI_FIXED, 0, 0 },
{ MKGUI_INPUT,    ID_INP, "",             "", ID_GRP,  0, 0, MKGUI_FIXED, 0, 0 },
{ MKGUI_CHECKBOX, ID_CHK, "Enable",       "", ID_GRP,  0, 0, 0, 0, 0 },
```

Children are laid out inside the group's content area (inside the border), stacking vertically like VBOX. The group is not focusable and does not intercept mouse events -- clicks pass through to children.

### Collapsible group

Set `MKGUI_COLLAPSIBLE` to allow a group to be collapsed/expanded by clicking the header. A collapsible group is always content-sized (it does not stretch to fill available space). Add `MKGUI_COLLAPSED` to start collapsed. When collapsed, children are hidden and the group shrinks to header height. Generates `MKGUI_EVENT_CLICK` with `value=1` when collapsed, `value=0` when expanded. Groups without `MKGUI_COLLAPSIBLE` ignore header clicks and have no arrow indicator.

```c
{ MKGUI_GROUP, ID_GRP, "Advanced", "", ID_PARENT, 0, 0, 0, MKGUI_COLLAPSIBLE, 1 }
{ MKGUI_GROUP, ID_GRP, "Advanced", "", ID_PARENT, 0, 0, 0, MKGUI_COLLAPSIBLE | MKGUI_COLLAPSED, 1 }
```

```c
void mkgui_group_set_collapsed(struct mkgui_ctx *ctx, uint32_t id, uint32_t collapsed);
uint32_t mkgui_group_get_collapsed(struct mkgui_ctx *ctx, uint32_t id);
```

## Panel

A plain container that behaves as a VBOX. Useful for layout grouping. Children are stacked vertically.

```c
{ MKGUI_PANEL, ID_PANEL, "", "", ID_TAB1, 0, 0, 0, MKGUI_PANEL_BORDER | MKGUI_PANEL_SUNKEN, 0 },
{ MKGUI_BUTTON, ID_BTN,  "OK", "", ID_PANEL, 80, 0, MKGUI_FIXED, 0, 0 },
```

Flags: `MKGUI_PANEL_BORDER` draws a rounded border, `MKGUI_PANEL_SUNKEN` darkens the background.

## Scrollbar

Standalone scrollbar widget. Horizontal by default, set `MKGUI_VERTICAL` for vertical orientation.

```c
{ MKGUI_SCROLLBAR, ID_SB, "", "", ID_TAB1, MKGUI_SCROLLBAR_W, 200, MKGUI_VERTICAL | MKGUI_FIXED, 0, 0 },
```

```c
void mkgui_scrollbar_setup(struct mkgui_ctx *ctx, uint32_t id, int32_t max_value, int32_t page_size);
void mkgui_scrollbar_set(struct mkgui_ctx *ctx, uint32_t id, int32_t value);
int32_t mkgui_scrollbar_get(struct mkgui_ctx *ctx, uint32_t id);
```

- `max_value`: total content size
- `page_size`: visible area size (determines thumb size)
- `value`: scroll position (0 to `max_value - page_size`)

Use `MKGUI_VERTICAL` flag for a vertical scrollbar. Emits `MKGUI_EVENT_SCROLL` on interaction (thumb drag, click, scroll wheel).

## Divider

A visual separator line for VBOX/HBOX containers. Draws an etched line. Horizontal by default; set `MKGUI_VERTICAL` for vertical.

```c
{ MKGUI_DIVIDER, ID_DIV, "", "", ID_VBOX, 0, 0, MKGUI_FIXED, 0, 0 }
```

## Image

Displays ARGB pixel data. The image is centered in the widget and scaled down to fit (never scaled up) by default.

```c
{ MKGUI_IMAGE, ID_IMG, "", "", ID_TAB1, 200, 200, MKGUI_FIXED, MKGUI_PANEL_BORDER, 0 },
```

```c
void mkgui_image_set(struct mkgui_ctx *ctx, uint32_t id, const uint32_t *pixels, int32_t w, int32_t h);
void mkgui_image_clear(struct mkgui_ctx *ctx, uint32_t id);
```

Pixels are ARGB format (alpha in bits 31-24). The widget copies the pixel data internally. Use `MKGUI_IMAGE_STRETCH` to stretch the image to fill the widget area. Use `MKGUI_PANEL_BORDER` for a border frame.

## GL View

Embeds an OpenGL viewport. mkgui creates a native child window; the user creates their own GL context and renders to it. The widget automatically triggers 60fps redraws when visible on the active tab.

```c
{ MKGUI_GLVIEW, ID_GL, "", "", ID_TAB1, 0, 0, 0, MKGUI_PANEL_BORDER, 0 },
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

## Window chrome auto-layout

When a `MKGUI_WINDOW` has menu, toolbar, statusbar, or pathbar children, the layout engine automatically positions them and reserves their space:

- **Menu** -- placed at the very top, full window width
- **Toolbar** -- placed below the menu, full window width
- **Pathbar** -- placed below the toolbar, full window width
- **Statusbar** -- placed at the very bottom, full window width
- **All other children** -- positioned in the remaining content area between the chrome widgets, using VBOX stacking

No manual offsets are needed on chrome widgets. Simply parent them to the window with zero size/flags. The content area is computed automatically.

## Auto-layout containers

Three container types provide automatic child positioning. The container controls all placement.

### Container padding

Layout containers (VBOX, HBOX, FORM) automatically apply 6px internal padding following these rules:
- **Top-level containers** (parent is a window, tab, panel, etc.): 6px padding on all sides
- **Nested containers** (parent is another VBOX/HBOX/FORM/GROUP/TABS): no padding (parent's gap provides spacing)
- **Containers with `MKGUI_PANEL_BORDER`**: always have padding regardless of nesting (the border creates a visual boundary)
- Use `MKGUI_NO_PAD` to explicitly suppress padding on any container

### Weight-based layout

Children in HBOX/VBOX use a weight-based distribution system. The `weight` field controls how space is allocated:

- **`MKGUI_FIXED`** flag -- Fixed size. The child uses its declared `w`/`h` as exact size. Weight is ignored.
- **`weight = 0`** (default) -- Flexible with effective weight 1. Zero-initialized widgets are flexible by default.
- **`weight > 0`** -- Flexible with explicit weight. Remaining space is distributed proportionally. A weight-2 child gets twice the extra space of a weight-1 child.

For flexible children, `w`/`h` is the minimum size in the layout direction. The child gets at least that much, plus its proportional share of remaining space. When `h` is 0, the natural height (derived from font height) is used as the minimum.

A container with `MKGUI_FIXED` and size 0 in the layout direction measures its children and sizes to fit them (size-to-content).

Use `mkgui_set_weight(ctx, id, weight)` to change weight at runtime.

### VBox

Stacks children vertically with 6px gap. By default children stretch to the full container width (override with `MKGUI_ALIGN_START/CENTER/END`). Use `MKGUI_SCROLL` to enable vertical scrolling when content overflows.

```c
{ MKGUI_VBOX,   ID_VBOX,  "", "", ID_TAB1, 0, 0, 0, 0, 0 },
{ MKGUI_BUTTON, ID_BTN1,  "Fixed (28px)", "", ID_VBOX, 0, 28, MKGUI_FIXED, 0, 0 },
{ MKGUI_INPUT,  ID_INP1,  "",             "", ID_VBOX, 0, 0, MKGUI_FIXED, 0, 0 },
{ MKGUI_BUTTON, ID_BTN2,  "Flex (fills)", "", ID_VBOX, 0, 0, 0, 0, 0 },
```

### HBox

Stacks children horizontally with 6px gap. By default children stretch to the full container height (override with `MKGUI_ALIGN_START/CENTER/END`). Use `MKGUI_SCROLL` to enable horizontal scrolling when content overflows.

```c
{ MKGUI_HBOX,   ID_HBOX, "", "", ID_TAB1, 0, 0, 0, 0, 0 },
{ MKGUI_PANEL,  ID_LEFT, "", "", ID_HBOX, 200, 0, MKGUI_FIXED, MKGUI_PANEL_BORDER, 0 },
{ MKGUI_PANEL,  ID_MID,  "", "", ID_HBOX, 0, 0, 0, 0, 0 },
{ MKGUI_PANEL,  ID_RIGHT,"", "", ID_HBOX, 150, 0, MKGUI_FIXED, MKGUI_PANEL_BORDER, 0 },
```

### Form

Two-column form layout for label+control pairs. Children are paired in order: 1st is label, 2nd is control, 3rd is label, 4th is control, etc. The label column auto-sizes to the widest label. Each row is 24px tall with 6px gap.

```c
{ MKGUI_FORM,     ID_FORM,  "", "", ID_TAB1, 0, 0, 0, 0, 0 },
{ MKGUI_LABEL,    ID_LBL1,  "Name:",    "", ID_FORM, 0, 0, 0, 0, 0 },
{ MKGUI_INPUT,    ID_INP1,  "",          "", ID_FORM, 0, 0, 0, 0, 0 },
{ MKGUI_LABEL,    ID_LBL2,  "Email:",   "", ID_FORM, 0, 0, 0, 0, 0 },
{ MKGUI_INPUT,    ID_INP2,  "",          "", ID_FORM, 0, 0, 0, 0, 0 },
{ MKGUI_LABEL,    ID_LBL3,  "Category:","", ID_FORM, 0, 0, 0, 0, 0 },
{ MKGUI_DROPDOWN, ID_DRP1,  "",          "", ID_FORM, 0, 0, 0, 0, 0 },
```

The widget editor automatically inserts a label when dropping a non-label widget into a FORM, ensuring the pairing is always correct. Dropping a label by itself places it in the label column.

Containers can be nested. For example, an HBOX containing a VBOX and a FORM side by side, where the VBOX has a fixed width and the FORM flexes to fill remaining space.

## Tabs

```c
{ MKGUI_TABS, ID_TABS, "",         "", ID_WINDOW, 0, 0, 0, 0, 0 },
{ MKGUI_TAB,  ID_TAB1, "General",  "", ID_TABS, 0, 0, 0, 0, 0 },
{ MKGUI_TAB,  ID_TAB2, "Settings", "", ID_TABS, 0, 0, 0, 0, 0 },

// Widgets inside tabs
{ MKGUI_BUTTON, ID_BTN, "OK", "", ID_TAB1, 80, 0, MKGUI_FIXED, 0, 0 },
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
MKGUI_TOOLBAR_HEIGHT_DEFAULT 28  // toolbar height (adapts to icon size)
MKGUI_STATUSBAR_HEIGHT 22    // statusbar height
MKGUI_SCROLLBAR_W      14    // scrollbar width
MKGUI_SPLIT_THICK       5    // splitter handle thickness
MKGUI_BOX_GAP           6    // gap between children in layout containers
MKGUI_BOX_PAD           6    // internal padding in layout containers
```

## Icons

mkgui uses pre-rasterized icons from the Material Design Icons (MDI) set. Icons are looked up by name (e.g. `"folder-open"`, `"content-save"`). Browse available icons at https://pictogrammers.com/library/mdi/.

```c
// Set icon on a widget (toolbar button, menu item, button, tab)
void mkgui_set_icon(struct mkgui_ctx *ctx, uint32_t widget_id, const char *icon_name);

// Set icon on a treeview node
void mkgui_set_treenode_icon(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id, const char *icon_name);
```

### Icon loading

Icons are **not loaded automatically**. The application must explicitly load icons before calling `mkgui_create`. There are three ways to provide icon data:

**Explicit file paths:**

```c
void mkgui_icons_load(const char *path, const char *toolbar_path);
```

Load icon packs from specific file paths. Pass NULL for `toolbar_path` to reuse the main pack for toolbars.

```c
mkgui_icons_load("mdi_icons.dat", "mdi_icons_toolbar.dat");
struct mkgui_ctx *ctx = mkgui_create(widgets, count);
```

**Auto-search by app name:**

```c
void mkgui_icons_search(const char *app_name);
```

Searches for `<name>_icons.dat` and `<name>_icons_toolbar.dat` in standard locations:

| Search order | Path |
|---|---|
| 1 | `./<name>_icons.dat` |
| 2 | `/usr/share/<name>/<name>_icons.dat` |
| 3 | `/usr/local/share/<name>/<name>_icons.dat` |
| 4 | `$XDG_DATA_DIRS/<name>/<name>_icons.dat` |

```c
mkgui_icons_search("myapp");
struct mkgui_ctx *ctx = mkgui_create(widgets, count);
```

This is what the editor emits in generated code. For distribution, place the `.dat` files in `/usr/share/<appname>/`.

**Embedded data (incbin):**

```c
void mkgui_set_icon_data(const uint8_t *icons_dat, uint32_t icons_size,
                         const uint8_t *toolbar_dat, uint32_t toolbar_size);
```

Provide icon data directly from memory, ideal for single-binary distribution using incbin. Pass NULL/0 for toolbar to reuse the main pack.

```c
#include "incbin.h"
INCBIN(app_icons, "myapp_icons.dat");
INCBIN(app_icons_tb, "myapp_icons_toolbar.dat");

int main(void) {
    mkgui_set_icon_data(app_icons_data, (uint32_t)INCBIN_SIZE(app_icons),
                        app_icons_tb_data, (uint32_t)INCBIN_SIZE(app_icons_tb));
    struct mkgui_ctx *ctx = mkgui_create(widgets, count);
}
```

The data is used in-place (not copied), so the pointers must remain valid for the lifetime of the application.

### Generating the icon pack

The icon pack is generated from the MDI TTF font using the `gen_icons` tool:

```bash
./tools/gen_icons ext/materialdesignicons-webfont.ttf 16 mdi_icons.dat
./tools/gen_icons ext/materialdesignicons-webfont.ttf 40 mdi_icons_toolbar.dat
```

The second argument is the icon size in pixels. The full MDI set has ~7400 icons (2 MB at 16px). To generate a subset, pass a text file listing the icon names to include:

```bash
./tools/gen_icons ext/materialdesignicons-webfont.ttf 16 mdi_icons.dat tools/subset.txt
```

The subset file has one kebab-case icon name per line. Comments start with `#`. The editor generates subset files automatically during code generation -- see below.

### Editor icon list generation

When the editor generates code ("File > Generate Code"), it also writes icon subset files alongside the `.c` file:

- `<name>_icons.txt` -- all icons used by non-toolbar widgets
- `<name>_icons_toolbar.txt` -- all icons used by toolbar buttons plus menu/widget icons

Add any icons you load dynamically in code (e.g. file type icons for a treeview) to these files, then pass them to `gen_icons` to build minimal `.dat` files for your application.

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

### Icon browser

```c
uint32_t mkgui_icon_browser(struct mkgui_ctx *ctx, char *out, uint32_t out_size);
```

Opens a modal icon browser dialog showing all loaded icons in a searchable grid. Returns 1 if the user selected an icon (name written to `out`), 0 if cancelled.

Override the font with the `MKGUI_FONT` environment variable:

```bash
MKGUI_FONT=/path/to/font.ttf ./myapp
```

The default font search order prefers proportional sans-serif fonts: Noto Sans, IBM Plex Sans, DejaVu Sans.

## Tooltips

```c
void mkgui_set_tooltip(struct mkgui_ctx *ctx, uint32_t id, const char *text);
```

Sets a tooltip on any widget. The tooltip appears after a short hover delay, positioned near the cursor. Pass `NULL` to clear.

```c
mkgui_set_tooltip(ctx, ID_BTN_SAVE, "Save the current document");
mkgui_set_tooltip(ctx, ID_CANVAS, "Click to draw, right-click to erase");
```

Toolbar buttons automatically get their `label` set as tooltip text (since toolbars only show icons). For other widgets, tooltips must be set explicitly -- no tooltip is shown by default.

Maximum tooltip text length is 127 characters.

## Context menus

Build and show context menus in response to right-click events. Context menus are Breeze-styled popup menus that support icons, separators, checkable and radio items, and keyboard navigation.

### API reference

```c
void mkgui_context_menu_clear(struct mkgui_ctx *ctx);
void mkgui_context_menu_add(struct mkgui_ctx *ctx, uint32_t id, const char *label, const char *icon, uint32_t flags);
void mkgui_context_menu_add_separator(struct mkgui_ctx *ctx);
void mkgui_context_menu_show(struct mkgui_ctx *ctx);
void mkgui_context_menu_show_at(struct mkgui_ctx *ctx, int32_t x, int32_t y);
```

- `mkgui_context_menu_clear()` -- Clear all items from the context menu
- `mkgui_context_menu_add()` -- Add an item. `id` is returned in `MKGUI_EVENT_CONTEXT_MENU`. `icon` is an MDI icon name (or NULL). `flags` can include `MKGUI_DISABLED`, `MKGUI_SEPARATOR`, `MKGUI_MENU_CHECK`, `MKGUI_MENU_RADIO`, `MKGUI_CHECKED`
- `mkgui_context_menu_add_separator()` -- Shorthand for adding a separator line
- `mkgui_context_menu_show()` -- Show the menu at the last right-click position
- `mkgui_context_menu_show_at()` -- Show the menu at a specific window position

Maximum 64 items per context menu.

### Usage pattern

Context menus are built and shown in response to `MKGUI_EVENT_CONTEXT` or `MKGUI_EVENT_CONTEXT_HEADER` events:

```c
case MKGUI_EVENT_CONTEXT: {
    if(ev->id == ID_LISTVIEW1) {
        mkgui_context_menu_clear(ctx);
        mkgui_context_menu_add(ctx, 100, "Cut", "content-cut", 0);
        mkgui_context_menu_add(ctx, 101, "Copy", "content-copy", 0);
        mkgui_context_menu_add(ctx, 102, "Paste", "content-paste", 0);
        mkgui_context_menu_add_separator(ctx);
        mkgui_context_menu_add(ctx, 103, "Delete", "delete", (ev->value < 0) ? MKGUI_DISABLED : 0);
        mkgui_context_menu_show(ctx);
    }
} break;

case MKGUI_EVENT_CONTEXT_HEADER: {
    // Right-click on column header -- ev->col has the column index
    mkgui_context_menu_clear(ctx);
    mkgui_context_menu_add(ctx, 200, "Name", NULL, MKGUI_MENU_CHECK | MKGUI_CHECKED);
    mkgui_context_menu_add(ctx, 201, "Size", NULL, MKGUI_MENU_CHECK | MKGUI_CHECKED);
    mkgui_context_menu_show(ctx);
} break;

case MKGUI_EVENT_CONTEXT_MENU: {
    // ev->id is the item id passed to mkgui_context_menu_add()
    printf("Selected context menu item: %u\n", ev->id);
} break;
```

### Enhanced CONTEXT event for views

For `MKGUI_LISTVIEW`, `MKGUI_GRIDVIEW`, `MKGUI_TREEVIEW`, and `MKGUI_ITEMVIEW`, the `MKGUI_EVENT_CONTEXT` event provides additional information:

| Widget type | `ev->value` | `ev->col` |
|---|---|---|
| `MKGUI_LISTVIEW` | row index (-1 if empty area) | column index |
| `MKGUI_GRIDVIEW` | row index (-1 if empty area) | column index |
| `MKGUI_TREEVIEW` | node id (-1 if empty area) | 0 |
| `MKGUI_ITEMVIEW` | item index (-1 if empty area) | 0 |
| Other widgets | mouse x | mouse y |

For listview and gridview, right-clicking on an unselected item selects it (matching Qt/KDE behavior). Right-clicking on an already-selected item preserves the selection.

### Keyboard navigation

When a context menu is open:
- **Up/Down** arrows move between items (skipping separators and disabled items)
- **Enter/Space** activates the highlighted item
- **Escape** closes the menu

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

### Color picker

```c
uint32_t mkgui_color_dialog(struct mkgui_ctx *ctx, uint32_t initial_color, uint32_t *out_color);
```

Opens a modal color picker dialog with three modes selectable via tabs: SV square with hue bar, color wheel, and RGB sliders. Includes hex input, R/G/B spinboxes, and a preview swatch. Returns 1 if OK (selected color written to `out_color` as 0x00RRGGBB), 0 if cancelled.

```c
uint32_t color = 0;
if(mkgui_color_dialog(ctx, 0x3366cc, &color)) {
    printf("Selected: #%06x\n", color);
}
```

## File dialogs

mkgui includes built-in open and save file dialogs. Both are blocking (modal) -- they run a nested event loop and return when the user confirms or cancels.

### Options struct

```c
struct mkgui_file_filter {
    const char *label;        // "Images", "Source Files"
    const char *pattern;      // "*.png;*.jpg;*.jpeg", "*.c;*.h"
};

struct mkgui_file_dialog_opts {
    const char *start_path;                   // NULL = home directory
    const struct mkgui_file_filter *filters;  // NULL = no filters
    uint32_t filter_count;
    const char *default_name;                 // save mode only, NULL = empty
    uint32_t multi_select;                    // open mode only, 0 or 1
};
```

Zero-init opts and only set the fields you need.

### Open dialog

```c
struct mkgui_file_dialog_opts opts = {0};
opts.multi_select = 1;
uint32_t count = mkgui_open_dialog(ctx, &opts);
for(uint32_t i = 0; i < count; ++i) {
    printf("Selected: %s\n", mkgui_dialog_path(ctx, i));
}
```

Returns the number of selected files (0 = cancelled). Set `opts.multi_select = 1` to enable Ctrl+Click and Shift+Click selection.

### Save dialog

```c
struct mkgui_file_dialog_opts opts = {0};
opts.default_name = "untitled.txt";
struct mkgui_file_filter filters[] = {
    { "Text files", "*.txt" },
    { "All files",  "*" },
};
opts.filters = filters;
opts.filter_count = 2;
if(mkgui_save_dialog(ctx, &opts)) {
    printf("Save to: %s\n", mkgui_dialog_path(ctx, 0));
}
```

Returns 1 if the user confirmed, 0 if cancelled. If the user types a filename without an extension and a filter is active, the first extension from the active filter is appended automatically. If the target file already exists, an overwrite confirmation dialog is shown.

### Dialog window

Dialogs open as separate, WM-decorated X11 windows (not overlays). They are:
- **Resizable** -- drag the window border to see more sidebar entries or file list rows (minimum size 400x300, initial 780x520)
- **Movable** -- drag the title bar to reposition, e.g. to see the main window underneath when naming a file
- **Modal** -- set as transient-for the main window; the main window does not accept input while the dialog is open
- **Parent stays alive** -- the parent window continues to repaint and animate (spinners, progress bars, GL views) while the dialog is open

The dialog window is centered on the parent window when opened.

### Dialog layout

Both dialogs feature:
- Toolbar with back/forward/up navigation buttons
- Breadcrumb path bar (MKGUI_PATHBAR) showing clickable directory segments; click to navigate, click background to type a path directly
- Left sidebar with shortcuts: Home, Desktop, Documents, Downloads, GTK bookmarks, and mounted volumes from `/media/` and `/mnt/`
- File list on the right with Name, Size, and Date Modified columns (sortable by clicking headers)
- File type filter dropdown (when filters are provided)
- Show hidden files checkbox
- Scrollable file list with mouse wheel
- Double-click directories to enter them, double-click files to select and confirm
- Type-ahead search: typing while the file list is focused jumps to the first matching file (accumulating prefix, backspace to correct, escape to reset)
- Keyboard: Escape to cancel, Enter to confirm, Ctrl+L to focus path bar, Backspace to go up a directory
- Tab to cycle focus between path bar, filename input (save), filter dropdown, and file list

The save dialog adds a "File name:" input row and a "New Folder" button at the bottom. Clicking a file in the list fills the filename field automatically.

### API reference

```c
uint32_t mkgui_open_dialog(struct mkgui_ctx *ctx, const struct mkgui_file_dialog_opts *opts);
uint32_t mkgui_save_dialog(struct mkgui_ctx *ctx, const struct mkgui_file_dialog_opts *opts);
const char *mkgui_dialog_path(struct mkgui_ctx *ctx, uint32_t index);
```

## Multi-window

mkgui supports multiple windows through child contexts. All child windows share the parent's platform connection (X11 Display / Win32 message loop), so events are routed correctly without any manual pump logic. The parent window stays visually updated (including animations) while child windows are open.

### Creating a child window

```c
struct mkgui_ctx *child = mkgui_create_child(parent_ctx, widgets, count, "Title", 640, 480);
```

`mkgui_create_child` creates a new window that shares the parent's display connection, font data, and theme. The child window is set as transient-for the parent (window managers typically keep it above the parent).

Destroy child windows with `mkgui_destroy_child(child)` instead of `mkgui_destroy()`. The parent must outlive all its children.

### Modal windows

Modal dialogs are handled internally by the built-in dialog functions (`mkgui_message_box`, `mkgui_confirm_dialog`, `mkgui_input_dialog`, `mkgui_open_dialog`, `mkgui_save_dialog`). They run their own nested event loop, blocking the caller. The parent window does not receive input during a modal dialog, but continues to repaint and animate (spinners, progress bars, GL views).

### How multi-window works

All windows created with `mkgui_create_child` share the parent's X11 Display connection (or Win32 message thread). A global window registry maps platform window handles to their owning `mkgui_ctx`. Events are automatically routed to the correct context:

1. Events for the running context or its popups are processed normally
2. Events for the parent trigger expose/resize handling (keeping it visually fresh during modal dialogs)
3. Events for other registered contexts are pushed to that context's deferred event queue

### API reference

```c
// Create a child window sharing the parent's platform connection
struct mkgui_ctx *mkgui_create_child(struct mkgui_ctx *parent,
                                      struct mkgui_widget *widgets,
                                      uint32_t count,
                                      const char *title,
                                      int32_t w, int32_t h);

// Destroy a child window (do not use mkgui_destroy for children)
void mkgui_destroy_child(struct mkgui_ctx *ctx);
```

## Rendering

All widget rendering is software-based (no GPU). The framebuffer is blitted to the X11 window via MIT-SHM (`XShmPutImage`).

### Rendering pipeline

mkgui currently performs a full redraw every frame. The dirty rectangle tracking infrastructure is in place (widgets mark themselves dirty, rects are merged when close together) but partial rendering is temporarily disabled due to a clipping bug. All rendering goes to a client-side framebuffer, then a single `XShmPutImage` (Linux) or `BitBlt` (Windows) blits the result to the window.

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
