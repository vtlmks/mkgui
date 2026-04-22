# mkgui

Minimal GUI toolkit for Linux (X11) and Windows (Win32) with software rendering. Single-file unity build, no third-party dependencies beyond FreeType2, Fontconfig, and Xlib (Linux) or GDI (Windows). PlutoSVG/PlutoVG are embedded in the source tree, not external dependencies.

## Building

mkgui uses a unity build. You compile a single `.c` file that `#include`s `mkgui.c`:

```bash
gcc -std=c99 -O2 -Wall -Wextra myapp.c -o myapp \
    $(pkg-config --cflags freetype2 fontconfig) -lX11 -lXext \
    $(pkg-config --libs freetype2 fontconfig) -lm
```

Or use the included `build.sh`:

```bash
./build.sh          # normal (-O2, debug symbols)
./build.sh release  # -O2 stripped
./build.sh debug    # -g -O0
./build.sh size     # -Os stripped (~26% smaller than release, also catches more -Wformat-truncation warnings)
./build.sh asan     # -O0 -fsanitize=address,undefined (Linux only)
./build.sh clean    # remove out/
```

`build.sh` produces `out/demo`, `out/editor`, `out/libmkgui.a`, `out/extract_icons`, and the test binaries. It also cross-compiles `out/demo.exe` and `out/editor.exe` if `x86_64-w64-mingw32-gcc` is available.

### Linux dependencies

- **FreeType2** -- glyph rasterization
- **Fontconfig** -- font file lookup
- **Xlib** (`-lX11`) -- windowing
- **Xext** (`-lXext`) -- MIT-SHM framebuffer blit
- **-lm** -- used by PlutoVG path math

### Windows dependencies

- **GDI** (`-lgdi32`) -- windowing, font rendering, framebuffer blit
- `-mwindows` to mark the executable as a GUI app (no console)

### OpenGL is optional

OpenGL (`-lGL` / `-lopengl32`) is only required if your application instantiates a `MKGUI_GLVIEW` widget. The core library, static library, editor, and all other widgets have no GPU dependency. The bundled `demo` binary uses `MKGUI_GLVIEW`, so building `demo.c` pulls in OpenGL; `libmkgui.a`, the editor, and user applications that do not use GL views do not.

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
        MKGUI_W(MKGUI_WINDOW, ID_WIN, "Hello",  "",  0,      400, 300, 0, 0, 0),
        MKGUI_W(MKGUI_BUTTON, ID_BTN, "Click",  "",  ID_WIN, 100, 0, MKGUI_FIXED, 0, 0),
        MKGUI_W(MKGUI_LABEL,  ID_LBL, "Ready",  "",  ID_WIN, 0, 0, 0, 0, 0),
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

### Parameter ownership

mkgui never modifies data passed as input parameters. Strings, widget arrays, column definitions, file dialog options, dropdown items, pixel buffers, and all other input data are either copied into internal storage or read in place. Your original data is never touched.

A small number of functions take explicit output parameters that the caller allocates and mkgui fills in:

| Function | Output parameter | Description |
|----------|-----------------|-------------|
| `mkgui_input_dialog` | `char *out, uint32_t out_size` | User-entered text |
| `mkgui_color_dialog` | `uint32_t *out_color` | Chosen color |
| `mkgui_icon_browser_theme` | `char *out, uint32_t out_size` | Selected icon name |
| `mkgui_listview_get_multi_sel` | `int32_t **out` | Selection index array (library-owned) |
| `mkgui_pathbar_get_segment_path` | `char *out, uint32_t out_size` | Composed path string |

These are the only parameters mkgui writes to.

## Widget definition

```c
struct mkgui_widget {
    uint32_t type;        // MKGUI_BUTTON, MKGUI_INPUT, etc.
    uint32_t id;          // unique identifier
    uint32_t parent_id;   // parent widget id
    int32_t  w, h;        // size (0 = natural size derived from font height)
    uint32_t flags;       // universal layout flags (MKGUI_FIXED, MKGUI_HIDDEN, etc.)
    uint32_t style;       // per-widget-type flags (MKGUI_LABEL_TRUNCATE, MKGUI_CHECKBOX_CHECKED, etc.)
    uint32_t weight;      // layout weight (0=default flex, >0=explicit proportional)
    int32_t  margin_l, margin_r, margin_t, margin_b; // layout margins
    char     label[256];  // display text
    char     icon[64];    // Freedesktop icon name (e.g. "folder-open")
};
```

The `MKGUI_W` macro provides a shorthand for widget initializers using the original logical field order:

```c
MKGUI_W(type, id, label, icon, parent_id, w, h, flags, style, weight)
```

The `flags` field holds universal layout and state flags: `MKGUI_FIXED`, `MKGUI_HIDDEN`, `MKGUI_DISABLED`, `MKGUI_SCROLL`, `MKGUI_NO_PAD`, `MKGUI_VERTICAL`, `MKGUI_REGION_*`, `MKGUI_ALIGN_*`. The `style` field holds per-widget-type appearance flags, named with the widget type prefix: `MKGUI_CHECKBOX_CHECKED`, `MKGUI_INPUT_PASSWORD`, `MKGUI_INPUT_READONLY`, `MKGUI_MENUITEM_SEPARATOR`, `MKGUI_MENUITEM_CHECK`, `MKGUI_MENUITEM_RADIO`, `MKGUI_PANEL_BORDER`, `MKGUI_PANEL_SUNKEN`, `MKGUI_SLIDER_MIXER`, `MKGUI_METER_TEXT`, `MKGUI_IMAGE_STRETCH`, `MKGUI_TAB_CLOSABLE`, `MKGUI_LISTVIEW_MULTI_SELECT`, `MKGUI_LABEL_TRUNCATE`, `MKGUI_TOOLBAR_ICONS_ONLY`, `MKGUI_TOOLBAR_TEXT_ONLY`, `MKGUI_LABEL_LINK`, `MKGUI_GROUP_COLLAPSIBLE`, `MKGUI_GROUP_COLLAPSED`, `MKGUI_LABEL_WRAP`, `MKGUI_INPUT_NUMERIC`.

There are no `x, y` fields. All positioning is handled by the container-based layout system. The `w` and `h` fields specify size; when `h` is 0, the widget uses its natural height (derived from the font height for most widget types).

## Widget types

| Type | Description |
|------|-------------|
| `MKGUI_WINDOW` | Root window. Must be first widget. |
| `MKGUI_BUTTON` | Push button. Emits `MKGUI_EVENT_CLICK`. Set `MKGUI_BUTTON_CHECKED` for toggle button (renders sunken when checked). |
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
| `MKGUI_IMAGE` | Displays ARGB pixel data. Centered by default, `MKGUI_IMAGE_STRETCH` to fill. `MKGUI_IMAGE_BORDER` for a border. |
| `MKGUI_GLVIEW` | OpenGL viewport. Creates a native child window for GL rendering. The user creates their own GL context. Automatically triggers 60fps redraws when visible. |
| `MKGUI_CANVAS` | Custom drawing area. Calls a user callback with clipping set to the widget rect. Supports `MKGUI_CANVAS_BORDER`. |
| `MKGUI_TOGGLE` | On/off toggle switch. Emits `MKGUI_EVENT_TOGGLE_CHANGED`. |
| `MKGUI_COMBOBOX` | Editable dropdown with filtering. Emits `MKGUI_EVENT_COMBOBOX_CHANGED`, `MKGUI_EVENT_COMBOBOX_SUBMIT`. |
| `MKGUI_DATEPICKER` | Date picker with calendar popup. Emits `MKGUI_EVENT_DATEPICKER_CHANGED`. |
| `MKGUI_IPINPUT` | IP address input (four octets). Emits `MKGUI_EVENT_IPINPUT_CHANGED`. |
| `MKGUI_PATHBAR` | Breadcrumb path bar. Emits `MKGUI_EVENT_PATHBAR_NAV`, `MKGUI_EVENT_PATHBAR_SUBMIT`. |
| `MKGUI_DIVIDER` | Horizontal or vertical etched separator line. No events. |
| `MKGUI_SPACER` | Invisible spacer for layout padding. No events. |
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
MKGUI_W(MKGUI_MENU,      ID_MENU, "",  "", ID_WINDOW, 0, 0, 0, 0, 0),
MKGUI_W(MKGUI_TOOLBAR,   ID_TB,   "",  "", ID_WINDOW, 0, 0, 0, 0, 0),
MKGUI_W(MKGUI_STATUSBAR, ID_SB,   "",  "", ID_WINDOW, 0, 0, 0, 0, 0),

// Other children of the window fill the remaining content area:
MKGUI_W(MKGUI_TABS, ID_TABS, "", "", ID_WINDOW, 0, 0, 0, 0, 0),

// A simple form inside a tab:
MKGUI_W(MKGUI_FORM,  ID_FORM, "", "", ID_TAB1, 0, 0, 0, 0, 0),
MKGUI_W(MKGUI_LABEL, ID_L1,   "Name:",  "", ID_FORM, 0, 0, 0, 0, 0),
MKGUI_W(MKGUI_INPUT, ID_I1,   "",       "", ID_FORM, 0, 0, 0, 0, 0),
```

## Flags

### Universal flags (`flags` field)

| Flag | Value | Description |
|------|-------|-------------|
| `MKGUI_REGION_TOP` | `1 << 4` | Top region of horizontal splitter |
| `MKGUI_REGION_BOTTOM` | `1 << 5` | Bottom region |
| `MKGUI_REGION_LEFT` | `1 << 6` | Left region of vertical splitter |
| `MKGUI_REGION_RIGHT` | `1 << 7` | Right region |
| `MKGUI_HIDDEN` | `1 << 8` | Widget not visible |
| `MKGUI_DISABLED` | `1 << 9` | Widget not interactive |
| `MKGUI_SCROLL` | `1 << 10` | Enable scrollbar on VBOX/HBOX when content exceeds bounds. Without this flag, containers enforce their content's minimum size. |
| `MKGUI_NO_PAD` | `1 << 11` | Suppress automatic container padding |
| `MKGUI_ALIGN_START` | `1 << 12` | Align to start (left in VBOX, top in HBOX) |
| `MKGUI_ALIGN_CENTER` | `2 << 12` | Center in cross-axis |
| `MKGUI_ALIGN_END` | `3 << 12` | Align to end (right in VBOX, bottom in HBOX) |
| `MKGUI_FIXED` | `1 << 14` | Fixed size; removed from flex distribution. With `w`/`h` > 0: use that exact size. With `w`/`h` = 0: use natural size. Weight is ignored. |
| `MKGUI_VERTICAL` | `1 << 15` | Vertical orientation for scrollbar, slider, meter, divider |

Cross-axis alignment flags are used on children of HBOX/VBOX to control alignment perpendicular to the layout direction. Default is stretch (fill container).

Change toolbar display mode at runtime with `mkgui_toolbar_set_mode()`. The toolbar height adapts automatically.

### Style flags (`style` field)

Each widget type has its own private flag namespace inside the `style` field, starting at bit 0. Different widget types reuse the same bit positions -- for example `MKGUI_CHECKBOX_CHECKED`, `MKGUI_BUTTON_CHECKED`, and `MKGUI_INPUT_PASSWORD` all live at bit 0, but that is harmless because the `style` field is interpreted relative to the widget's `type`. Always reference flags by name; the numeric bit layout is an internal detail and may change between releases.

**Button:**

| Flag | Description |
|------|-------------|
| `MKGUI_BUTTON_CHECKED` | Toggle button pressed state (renders sunken) |
| `MKGUI_BUTTON_SEPARATOR` | Toolbar button separator (draws a vertical divider, no click) |

**Checkbox / Radio / Toggle:**

| Flag | Description |
|------|-------------|
| `MKGUI_CHECKBOX_CHECKED` | Initial checked state |
| `MKGUI_RADIO_CHECKED` | Initial selected state |
| `MKGUI_TOGGLE_CHECKED` | Initial on state |

**Input:**

| Flag | Description |
|------|-------------|
| `MKGUI_INPUT_PASSWORD` | Render dots instead of text, disables copy |
| `MKGUI_INPUT_READONLY` | Not editable, disables paste |
| `MKGUI_INPUT_NUMERIC` | Filter keyboard input to digits, decimal point, sign, and scientific notation |

**Textarea / DatePicker:**

| Flag | Description |
|------|-------------|
| `MKGUI_TEXTAREA_READONLY` | Not editable |
| `MKGUI_DATEPICKER_READONLY` | Not editable |

**Label:**

| Flag | Description |
|------|-------------|
| `MKGUI_LABEL_TRUNCATE` | Clip overflow with "..." |
| `MKGUI_LABEL_LINK` | Clickable hyperlink style, emits `MKGUI_EVENT_CLICK` |
| `MKGUI_LABEL_WRAP` | Word-wrap multi-line text |

**Menu item / context menu item:**

| Flag | Description |
|------|-------------|
| `MKGUI_MENUITEM_SEPARATOR` | Render as separator line |
| `MKGUI_MENUITEM_CHECK` | Checkbox indicator |
| `MKGUI_MENUITEM_RADIO` | Radio indicator (auto-unchecks siblings) |
| `MKGUI_MENUITEM_CHECKED` | Initial check/radio state |

**Panel / VBox / HBox / Form / Canvas / Image / GLView:**

| Flag | Widget(s) | Description |
|------|-----------|-------------|
| `MKGUI_PANEL_BORDER` | Panel | Draw rounded border |
| `MKGUI_PANEL_SUNKEN` | Panel | Recessed background |
| `MKGUI_VBOX_BORDER` | VBox | Draw border |
| `MKGUI_HBOX_BORDER` | HBox | Draw border |
| `MKGUI_FORM_BORDER` | Form | Draw border |
| `MKGUI_CANVAS_BORDER` | Canvas | Draw border |
| `MKGUI_CANVAS_SUNKEN` | Canvas | Recessed background |
| `MKGUI_IMAGE_BORDER` | Image | Draw border |
| `MKGUI_IMAGE_STRETCH` | Image | Stretch image to fill widget |
| `MKGUI_GLVIEW_BORDER` | GL View | Draw border |

**Slider / Meter / Progress:**

| Flag | Description |
|------|-------------|
| `MKGUI_SLIDER_MIXER` | Tapered volume style with optional meter overlay |
| `MKGUI_METER_TEXT` | Display centered percentage |
| `MKGUI_PROGRESS_SHIMMER` | Animated shimmer sweep (default on) |

**Toolbar (display mode):**

| Flag | Description |
|------|-------------|
| `MKGUI_TOOLBAR_ICONS_TEXT` | Icons and text (default, value 0) |
| `MKGUI_TOOLBAR_ICONS_ONLY` | Icons only |
| `MKGUI_TOOLBAR_TEXT_ONLY` | Text only |

The toolbar mode is selected by masking `MKGUI_TOOLBAR_MODE_MASK`. Change at runtime with `mkgui_toolbar_set_mode()`; the toolbar height adapts automatically.

**Tab:**

| Flag | Description |
|------|-------------|
| `MKGUI_TAB_CLOSABLE` | Show close button (emits `MKGUI_EVENT_TAB_CLOSE`) |

**Listview / Treeview / Gridview / Itemview:**

| Flag | Description |
|------|-------------|
| `MKGUI_LISTVIEW_MULTI_SELECT` | Enable multi-selection |
| `MKGUI_LISTVIEW_EDITABLE` | Enable F2 / slow-double-click cell editing |
| `MKGUI_TREEVIEW_MULTI_SELECT` | Enable multi-selection |
| `MKGUI_TREEVIEW_EDITABLE` | Enable F2 / slow-double-click cell editing |
| `MKGUI_GRIDVIEW_MULTI_SELECT` | Enable multi-selection |
| `MKGUI_ITEMVIEW_EDITABLE` | Enable F2 / slow-double-click cell editing |

**Group:**

| Flag | Description |
|------|-------------|
| `MKGUI_GROUP_COLLAPSIBLE` | Header click toggles collapse state; adds arrow indicator |
| `MKGUI_GROUP_COLLAPSED` | Start collapsed (requires `MKGUI_GROUP_COLLAPSIBLE`) |

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
| `MKGUI_EVENT_GRIDVIEW_SELECT` | Grid row selected (keyboard navigation / single click) | row | column |
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
| `MKGUI_EVENT_ACCEL` | Keyboard accelerator matched (non-menu target) | -- | -- |
| `MKGUI_EVENT_FILE_DROP` | Files dropped on window from OS | file count | -- |
| `MKGUI_EVENT_PATHBAR_NAV` | Pathbar segment clicked | segment index | -- |
| `MKGUI_EVENT_PATHBAR_SUBMIT` | Pathbar edit-mode Enter pressed | -- | -- |
| `MKGUI_EVENT_IPINPUT_CHANGED` | IPv4 address octet changed | -- | -- |
| `MKGUI_EVENT_TOGGLE_CHANGED` | Toggle switched | 0 or 1 | -- |
| `MKGUI_EVENT_COMBOBOX_CHANGED` | Combobox selection or text changed | selected index (-1 if custom text) | -- |
| `MKGUI_EVENT_COMBOBOX_SUBMIT` | Enter pressed in combobox | -- | -- |
| `MKGUI_EVENT_DATEPICKER_CHANGED` | Date picker value changed | -- | -- |
| `MKGUI_EVENT_CELL_EDIT_COMMIT` | Cell edit confirmed (Enter or focus out) | row | column |

## API reference

### Lifecycle

```c
struct mkgui_ctx *mkgui_create(const struct mkgui_widget *widgets, uint32_t count);
void mkgui_destroy(struct mkgui_ctx *ctx);
void mkgui_set_callback(struct mkgui_ctx *ctx, mkgui_event_cb cb, void *userdata);
void mkgui_run(struct mkgui_ctx *ctx, mkgui_event_cb cb, void *userdata);
void mkgui_quit(struct mkgui_ctx *ctx);
void mkgui_set_title(struct mkgui_ctx *ctx, const char *title);
void mkgui_set_app_class(struct mkgui_ctx *ctx, const char *app_class);
void mkgui_set_window_instance(struct mkgui_ctx *ctx, const char *instance);
uint32_t mkgui_add_timer(struct mkgui_ctx *ctx, uint64_t interval_ns, mkgui_timer_cb cb, void *userdata);
void mkgui_remove_timer(struct mkgui_ctx *ctx, uint32_t timer_id);
```

`mkgui_set_title` changes the window title bar text at runtime. Does not trigger a widget redraw.

`mkgui_create` takes a widget array and returns a context.

`mkgui_set_app_class` sets the application class name used by window managers to group windows, match `.desktop` files, and select taskbar icons. On X11 this sets the `WM_CLASS` class field and the `_NET_WM_PID`; on Windows it maps to the window class. Call before showing the window (i.e. right after `mkgui_create`) for best results. The string is copied internally.

`mkgui_set_window_instance` sets the per-window instance name in `WM_CLASS`. Useful when one application spawns several windows that window managers should treat as distinct (e.g. "editor", "preview", "icon-browser"). The default instance is the class name. Has no effect on Windows.

`mkgui_icon_load_app_icons` takes its own `app_name` argument for the icon directory lookup. It is independent from the WM class, but in practice most applications pass the same string to both calls.

`mkgui_run` runs the event loop. It drains all pending events, delivers each to `cb`, renders the frame, then blocks until the next event or timer. This repeats until `mkgui_quit` is called. It also pumps events for all other registered contexts that have a callback set via `mkgui_set_callback`, rendering and dispatching their events automatically.

`mkgui_set_callback` registers a per-context event callback without starting an event loop. Use this for secondary windows that should be pumped by an existing `mkgui_run` loop. The context is automatically picked up on the next iteration.

`mkgui_run` handles frame pacing automatically:
- When animated widgets are visible (spinners, progress bars, GL views), it runs at ~60fps
- When idle, it blocks indefinitely until an event arrives (zero CPU usage)
- It wakes immediately on platform events (mouse, keyboard, expose) or timer expiration

On Linux, blocking uses `poll()` on the X11 connection fd and timer fds. On Windows, it uses `MsgWaitForMultipleObjects`.

Mouse motion events are automatically compressed. When multiple consecutive motion events are queued, only the last position is delivered.

**Single-window example:**

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

**Multi-window example (e.g. plugin config):**

```c
static struct mkgui_ctx *plugin_ctx;

static void plugin_event(struct mkgui_ctx *ctx, struct mkgui_event *ev, void *ud) {
    (void)ud;
    switch(ev->type) {
        case MKGUI_EVENT_CLOSE: {
            mkgui_destroy(ctx);
            plugin_ctx = NULL;
        } break;
    }
}

// Called from the main window's event callback
void open_plugin_config(void) {
    plugin_ctx = mkgui_create(plugin_widgets, plugin_count);
    mkgui_set_callback(plugin_ctx, plugin_event, NULL);
    // Returns immediately; the main mkgui_run() loop pumps this context
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


### Window icon

```c
struct mkgui_icon_size {
    uint32_t *pixels;
    int32_t w, h;
};

void mkgui_set_window_icon(struct mkgui_ctx *ctx, const struct mkgui_icon_size *sizes, uint32_t count);
```

Sets the application icon displayed in the window title bar, taskbar, and task switcher. Call after `mkgui_create` (or `mkgui_create_child`), before or after `mkgui_run`. Pass one or more ARGB32 pixel buffers at different resolutions so the window manager can pick the best match for each context.

**Recommended sizes:**

| Size | Used for |
|---|---|
| 16x16 | Window title bar, system tray (Win32 `ICON_SMALL`) |
| 24x24 | Title bar on HiDPI / some Linux WMs |
| 32x32 | Taskbar, alt-tab (Win32 `ICON_BIG` at 100% scale) |
| 48x48 | Taskbar at 150% scale, KDE/GNOME app grid |
| 64x64 | Alt-tab on HiDPI, large taskbar icons |
| 256x256 | Windows Explorer details view, GNOME Activities |

For best cross-platform results, provide at least **16x16, 32x32, and 48x48**. Adding 64x64 and 256x256 covers HiDPI and high-res contexts.

On X11, all sizes are packed into the `_NET_WM_ICON` property and the window manager selects the closest match. On Win32, the two closest matches to `SM_CXSMICON` and `SM_CXICON` are used for `ICON_SMALL` and `ICON_BIG`.

Pixel format is ARGB32 (same as the rest of mkgui): `0xAARRGGBB`.

**Example:**

```c
// icon_16, icon_32, icon_48 are uint32_t arrays with premade pixel data
struct mkgui_icon_size app_icons[] = {
    { icon_16, 16, 16 },
    { icon_32, 32, 32 },
    { icon_48, 48, 48 },
};
mkgui_set_window_icon(ctx, app_icons, 3);
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
struct mkgui_widget slider = MKGUI_W(MKGUI_SLIDER, 500, "Volume", "", VBOX_ID, 0, 0, 0, 0, 0);
mkgui_add_widget(ctx, slider, VBOX_ID);

// add a button after an existing sibling
struct mkgui_widget btn = MKGUI_W(MKGUI_BUTTON, 501, "Mute", "", VBOX_ID, 80, 0, MKGUI_FIXED, 0, 0);
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
void mkgui_get_min_size(struct mkgui_ctx *ctx, int32_t *out_w, int32_t *out_h);
void mkgui_set_flags(struct mkgui_ctx *ctx, uint32_t id, uint32_t flags);
uint32_t mkgui_get_flags(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_set_tooltip(struct mkgui_ctx *ctx, uint32_t id, const char *text);
const char *mkgui_get_tooltip(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_set_weight(struct mkgui_ctx *ctx, uint32_t id, uint32_t weight);
void mkgui_set_icon(struct mkgui_ctx *ctx, uint32_t widget_id, const char *icon_name);
uint32_t mkgui_is_shown(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_get_window_size(struct mkgui_ctx *ctx, int32_t *out_w, int32_t *out_h);
double mkgui_get_anim_time(struct mkgui_ctx *ctx);
```

- `set_enabled` / `get_enabled` -- toggle `MKGUI_DISABLED` flag. Disabling clears focus.
- `set_visible` / `get_visible` -- toggle `MKGUI_HIDDEN` flag. Hiding clears focus.
- `set_focus` -- programmatic focus. Only works on focusable, enabled widgets.
- `get_geometry` -- returns the widget's computed layout rect (after layout). Any pointer may be NULL.
- `get_min_size` -- returns the minimum window size needed to display all content without scrolling. Useful after dynamically adding widgets.
- `set_flags` / `get_flags` -- raw flag access. Use for toggling multiple flags at once.
- `is_shown` -- returns 1 if the widget is currently rendered: not flagged `MKGUI_HIDDEN`, no ancestor is hidden, and if nested inside a `MKGUI_TABS`, its tab is active. Accounts for collapsed groups. Returns 0 for unknown ids. Requires at least one layout pass.
- `get_window_size` -- returns the current window dimensions. Either out pointer may be NULL.
- `get_anim_time` -- monotonic time in seconds since `mkgui_create`, used internally by animated widgets (spinner, progress shimmer). Useful for driving custom animations from your render callback.

### Button

```c
void mkgui_button_set_text(struct mkgui_ctx *ctx, uint32_t id, const char *text);
const char *mkgui_button_get_text(struct mkgui_ctx *ctx, uint32_t id);
```

**Toggle button**: Set `MKGUI_BUTTON_CHECKED` on a button to render it sunken (pressed state). Toggle the flag in your click handler to create an on/off button:

```c
// Icon-only buttons center the icon automatically when label is empty
MKGUI_W(MKGUI_BUTTON, ID_MUTE, "", "volume-high", ID_VBOX, 28, 28, MKGUI_FIXED | MKGUI_ALIGN_CENTER, 0, 0),

// In event callback:
case MKGUI_EVENT_CLICK: {
    if(ev->id == ID_MUTE) {
        uint32_t f = mkgui_get_flags(ctx, ID_MUTE);
        f ^= MKGUI_BUTTON_CHECKED;
        mkgui_set_flags(ctx, ID_MUTE, f);
        mkgui_set_icon(ctx, ID_MUTE, (f & MKGUI_BUTTON_CHECKED) ? "volume-off" : "volume-high");
    }
} break;
```

### Label

```c
void mkgui_label_set(struct mkgui_ctx *ctx, uint32_t id, const char *text);
const char *mkgui_label_get(struct mkgui_ctx *ctx, uint32_t id);
```

Set `MKGUI_LABEL_TRUNCATE` to clip text that overflows the widget width, replacing the tail with "...". Without this flag, text is hard-clipped at the widget boundary. The full text is preserved in the widget -- only the rendered output is affected.

```c
MKGUI_W(MKGUI_LABEL, ID_NAME, "Very Long Channel Name", "", ID_PARENT, 80, 0,
  MKGUI_FIXED, MKGUI_LABEL_TRUNCATE, 0)
```

#### Link label

Set `MKGUI_LABEL_LINK` to make a label clickable. The text renders in the selection color with an underline. Hover changes color. Generates `MKGUI_EVENT_CLICK` when clicked.

```c
MKGUI_W(MKGUI_LABEL, ID_LINK, "Visit website", "", ID_PARENT, 0, 0, MKGUI_FIXED, MKGUI_LABEL_LINK, 0)
```

#### Wrapping label

Set `MKGUI_LABEL_WRAP` to word-wrap text across multiple lines. The label renders from the top and wraps at word boundaries to fit the widget width. Set an explicit height or use flex fill.

```c
MKGUI_W(MKGUI_LABEL, ID_DESC, "Long description text here...", "", ID_PARENT, 0, 80, MKGUI_FIXED, MKGUI_LABEL_WRAP, 0)
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

Supports Ctrl+C (copy) and Ctrl+V (paste). Newlines in pasted text are replaced with spaces. Copy is disabled when `MKGUI_INPUT_PASSWORD` is set. Paste is disabled when `MKGUI_INPUT_READONLY` is set.

`input_clear` resets text, cursor, and selection. `input_set_readonly` / `input_get_readonly` toggle the `MKGUI_INPUT_READONLY` flag at runtime. Cursor and selection positions are byte offsets into the text. Text is stored as UTF-8. Cursor navigation (left/right, backspace, delete) is UTF-8 aware -- multi-byte characters are treated as single units. The glyph range covers Latin-1 (codepoints 32-255), supporting Western European languages.

#### Numeric input

Set `MKGUI_INPUT_NUMERIC` to filter keyboard input to digits, decimal point, sign, and scientific notation (e/E). Non-numeric characters are rejected.

```c
MKGUI_W(MKGUI_INPUT, ID_AMOUNT, "", "", ID_FORM, 0, 0, 0, MKGUI_INPUT_NUMERIC, 0)
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
void mkgui_dropdown_setup(struct mkgui_ctx *ctx, uint32_t id, const char *const *items, uint32_t count);
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
void mkgui_combobox_setup(struct mkgui_ctx *ctx, uint32_t id, const char *const *items, uint32_t count);
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
MKGUI_W(MKGUI_SPINNER, ID_SPIN, "", "", ID_PARENT, 24, 24, MKGUI_FIXED, 0, 0)
```

No setup or API functions needed. The spinner is a rotating arc that animates automatically. Size is determined by the widget's `w` and `h` (uses the smaller of the two as diameter). Uses `theme.accent` for the arc color. Place it anywhere a loading indicator is needed.

The spinner only consumes CPU while visible. Widgets on inactive tabs or hidden parents do not trigger redraws.

### Listview

```c
typedef void (*mkgui_row_cb)(uint32_t row, uint32_t col, char *buf, uint32_t buf_size, void *userdata);

void mkgui_listview_setup(struct mkgui_ctx *ctx, uint32_t id, uint32_t row_count,
                          uint32_t col_count, const struct mkgui_column *columns,
                          mkgui_row_cb cb, void *userdata);
void mkgui_listview_set_rows(struct mkgui_ctx *ctx, uint32_t id, uint32_t row_count);
int32_t mkgui_listview_get_selected(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_listview_set_selected(struct mkgui_ctx *ctx, uint32_t id, int32_t row);
uint32_t mkgui_listview_get_multi_sel(struct mkgui_ctx *ctx, uint32_t id, int32_t **out);
uint32_t mkgui_listview_is_selected(struct mkgui_ctx *ctx, uint32_t id, int32_t row);
void mkgui_listview_clear_selection(struct mkgui_ctx *ctx, uint32_t id);
const uint32_t *mkgui_listview_get_col_order(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_listview_set_col_order(struct mkgui_ctx *ctx, uint32_t id, const uint32_t *order, uint32_t count);
int32_t mkgui_listview_get_col_width(struct mkgui_ctx *ctx, uint32_t id, uint32_t col);
uint32_t mkgui_listview_get_col_count(struct mkgui_ctx *ctx, uint32_t id);
const char *mkgui_listview_get_col_label(struct mkgui_ctx *ctx, uint32_t id, uint32_t col);
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
| `MKGUI_CELL_ICON_TEXT` | Icon followed by text | `"icon-name\tLabel"` (tab-separated; icon name is a Freedesktop icon name) |
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
    const struct mkgui_grid_column *columns, mkgui_grid_cell_cb cell_cb, void *userdata);
void mkgui_gridview_set_rows(struct mkgui_ctx *ctx, uint32_t id, uint32_t row_count);
int32_t mkgui_gridview_get_selected(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_gridview_set_selected(struct mkgui_ctx *ctx, uint32_t id, int32_t row);
uint32_t mkgui_gridview_get_check(struct mkgui_ctx *ctx, uint32_t id, uint32_t row, uint32_t col);
void mkgui_gridview_set_check(struct mkgui_ctx *ctx, uint32_t id, uint32_t row, uint32_t col, uint32_t checked);
int32_t mkgui_gridview_get_col_width(struct mkgui_ctx *ctx, uint32_t id, uint32_t col);
uint32_t mkgui_gridview_get_col_count(struct mkgui_ctx *ctx, uint32_t id);
const char *mkgui_gridview_get_col_label(struct mkgui_ctx *ctx, uint32_t id, uint32_t col);
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
- **`icon_cb(item, buf, buf_size, userdata)`** -- fill `buf` with the Freedesktop icon name for `item` (e.g. `"folder"`, `"text-x-generic"`). Used by icon, compact, and detail modes. Optional (no icon drawn if NULL).
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

`textarea_set_readonly` / `textarea_get_readonly` toggle the `MKGUI_TEXTAREA_READONLY` flag at runtime. Cursor position is returned as line/col (0-based). Selection start/end are byte offsets. `textarea_insert` inserts text at the cursor (replacing any selection). `textarea_append` appends to the end without moving the cursor. `textarea_scroll_to_end` scrolls to the bottom -- useful for log views. Text is stored as UTF-8. Cursor navigation (left/right, backspace, delete) is UTF-8 aware -- multi-byte characters are treated as single units.

### Statusbar

```c
void mkgui_statusbar_setup(struct mkgui_ctx *ctx, uint32_t id, uint32_t section_count, int32_t *widths);
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
MKGUI_W(MKGUI_CANVAS, ID_CANVAS, "", "", ID_PARENT, 0, 0, 0, MKGUI_CANVAS_BORDER, 0)
```

The callback receives the pixel buffer and the widget's rect. All `draw_*` functions respect the clip rect, which is automatically set to the widget bounds before the callback and restored after. Use `MKGUI_CANVAS_BORDER` to draw a border before the callback runs.

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
MKGUI_W(MKGUI_MENU,     ID_MENU,  "",     "",             ID_WINDOW, 0, 0, 0, 0, 0),
MKGUI_W(MKGUI_MENUITEM, ID_FILE,  "File", "",             ID_MENU,   0, 0, 0, 0, 0),
MKGUI_W(MKGUI_MENUITEM, ID_OPEN,  "Open", "folder-open",  ID_FILE,   0, 0, 0, 0, 0),
MKGUI_W(MKGUI_MENUITEM, ID_SAVE,  "Save", "content-save", ID_FILE,   0, 0, 0, 0, 0),
MKGUI_W(MKGUI_MENUITEM, ID_SEP1,  "",     "",             ID_FILE,   0, 0, 0, MKGUI_MENUITEM_SEPARATOR, 0),
MKGUI_W(MKGUI_MENUITEM, ID_EXIT,  "Exit", "",             ID_FILE,   0, 0, 0, 0, 0),
```

`MKGUI_MENUITEM_SEPARATOR` makes the item render as a horizontal separator line. Use a dedicated menu item with an empty label and the `MKGUI_MENUITEM_SEPARATOR` flag to create visual grouping between menu entries.

Check/radio menu items:

```c
MKGUI_W(MKGUI_MENUITEM, ID_SHOW_TB, "Show Toolbar", "", ID_VIEW, 0, 0, 0, MKGUI_MENUITEM_CHECK | MKGUI_MENUITEM_CHECKED, 0),
MKGUI_W(MKGUI_MENUITEM, ID_SORT_NAME, "By Name",    "", ID_SORT, 0, 0, 0, MKGUI_MENUITEM_RADIO | MKGUI_MENUITEM_CHECKED, 0),
MKGUI_W(MKGUI_MENUITEM, ID_SORT_SIZE, "By Size",    "", ID_SORT, 0, 0, 0, MKGUI_MENUITEM_RADIO, 0),
```

Check items toggle on click. Radio items auto-uncheck siblings. Only leaf items (no children) emit `MKGUI_EVENT_MENU`.

## Toolbar

Toolbar children are `MKGUI_BUTTON` widgets:

```c
MKGUI_W(MKGUI_TOOLBAR, ID_TB,      "",     "",             ID_WINDOW, 0, 0, 0, 0, 0),
MKGUI_W(MKGUI_BUTTON,  ID_TB_NEW,  "New",  "file-plus",    ID_TB,     0, 0, 0, 0, 0),
MKGUI_W(MKGUI_BUTTON,  ID_TB_OPEN, "Open", "folder-open",  ID_TB,     0, 0, 0, 0, 0),
MKGUI_W(MKGUI_BUTTON,  ID_TB_SAVE, "Save", "content-save", ID_TB,     0, 0, 0, 0, 0),
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

Or set the mode in the widget style at init time:

```c
MKGUI_W(MKGUI_TOOLBAR, ID_TB, "", "", ID_WINDOW, 0, 0, 0, MKGUI_TOOLBAR_ICONS_ONLY, 0),
```

### DatePicker

```c
void mkgui_datepicker_set(struct mkgui_ctx *ctx, uint32_t id, int32_t year, int32_t month, int32_t day);
void mkgui_datepicker_get(struct mkgui_ctx *ctx, uint32_t id, int32_t *year, int32_t *month, int32_t *day);
const char *mkgui_datepicker_get_text(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_datepicker_set_readonly(struct mkgui_ctx *ctx, uint32_t id, uint32_t readonly);
uint32_t mkgui_datepicker_get_readonly(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_today(int32_t *year, int32_t *month, int32_t *day);
```

Date picker with calendar popup. Click the dropdown arrow to open the calendar. Type directly in the text field to edit the date (YYYY-MM-DD format). Emits `MKGUI_EVENT_DATEPICKER_CHANGED`.

`mkgui_today` is a free-standing helper that fills the current local date. Any out pointer may be NULL. Useful to seed a datepicker at creation time.

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
MKGUI_W(MKGUI_VSPLIT,   ID_SPLIT, "", "", ID_WIN, 0, 0, 0, 0, 0),
MKGUI_W(MKGUI_TREEVIEW, ID_TREE,  "", "", ID_SPLIT, 0, 0, MKGUI_REGION_LEFT, 0, 0),
MKGUI_W(MKGUI_LISTVIEW, ID_LIST,  "", "", ID_SPLIT, 0, 0, MKGUI_REGION_RIGHT, 0, 0),
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
MKGUI_W(MKGUI_GROUP,    ID_GRP, "Settings",     "", ID_TAB1, 0, 0, MKGUI_FIXED, 0, 0),
MKGUI_W(MKGUI_INPUT,    ID_INP, "",             "", ID_GRP,  0, 0, MKGUI_FIXED, 0, 0),
MKGUI_W(MKGUI_CHECKBOX, ID_CHK, "Enable",       "", ID_GRP,  0, 0, 0, 0, 0),
```

Children are laid out inside the group's content area (inside the border), stacking vertically like VBOX. The group is not focusable and does not intercept mouse events -- clicks pass through to children.

### Collapsible group

Set `MKGUI_GROUP_COLLAPSIBLE` to allow a group to be collapsed/expanded by clicking the header. A collapsible group is always content-sized (it does not stretch to fill available space). Add `MKGUI_GROUP_COLLAPSED` to start collapsed. When collapsed, children are hidden and the group shrinks to header height. Generates `MKGUI_EVENT_CLICK` with `value=1` when collapsed, `value=0` when expanded. Groups without `MKGUI_GROUP_COLLAPSIBLE` ignore header clicks and have no arrow indicator.

```c
MKGUI_W(MKGUI_GROUP, ID_GRP, "Advanced", "", ID_PARENT, 0, 0, 0, MKGUI_GROUP_COLLAPSIBLE, 1)
MKGUI_W(MKGUI_GROUP, ID_GRP, "Advanced", "", ID_PARENT, 0, 0, 0, MKGUI_GROUP_COLLAPSIBLE | MKGUI_GROUP_COLLAPSED, 1)
```

```c
void mkgui_group_set_collapsed(struct mkgui_ctx *ctx, uint32_t id, uint32_t collapsed);
uint32_t mkgui_group_get_collapsed(struct mkgui_ctx *ctx, uint32_t id);
```

## Panel

A plain container that behaves as a VBOX. Useful for layout grouping. Children are stacked vertically.

```c
MKGUI_W(MKGUI_PANEL, ID_PANEL, "", "", ID_TAB1, 0, 0, 0, MKGUI_PANEL_BORDER | MKGUI_PANEL_SUNKEN, 0),
MKGUI_W(MKGUI_BUTTON, ID_BTN,  "OK", "", ID_PANEL, 80, 0, MKGUI_FIXED, 0, 0),
```

Flags: `MKGUI_PANEL_BORDER` draws a rounded border, `MKGUI_PANEL_SUNKEN` darkens the background.

## Scrollbar

Standalone scrollbar widget. Horizontal by default, set `MKGUI_VERTICAL` for vertical orientation.

```c
MKGUI_W(MKGUI_SCROLLBAR, ID_SB, "", "", ID_TAB1, MKGUI_SCROLLBAR_W, 200, MKGUI_VERTICAL | MKGUI_FIXED, 0, 0),
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
MKGUI_W(MKGUI_DIVIDER, ID_DIV, "", "", ID_VBOX, 0, 0, MKGUI_FIXED, 0, 0)
```

## Image

Displays ARGB pixel data. The image is centered in the widget and scaled down to fit (never scaled up) by default.

```c
MKGUI_W(MKGUI_IMAGE, ID_IMG, "", "", ID_TAB1, 200, 200, MKGUI_FIXED, MKGUI_IMAGE_BORDER, 0),
```

```c
void mkgui_image_set(struct mkgui_ctx *ctx, uint32_t id, uint32_t *pixels, int32_t w, int32_t h);
void mkgui_image_clear(struct mkgui_ctx *ctx, uint32_t id);
```

Pixels are ARGB format (alpha in bits 31-24). The widget copies the pixel data internally. Use `MKGUI_IMAGE_STRETCH` to stretch the image to fill the widget area. Use `MKGUI_IMAGE_BORDER` for a border frame.

## GL View

Embeds an OpenGL viewport. mkgui creates a native child window; the user creates their own GL context and renders to it. The widget automatically triggers 60fps redraws when visible on the active tab.

```c
MKGUI_W(MKGUI_GLVIEW, ID_GL, "", "", ID_TAB1, 0, 0, 0, MKGUI_GLVIEW_BORDER, 0),
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
- **Containers with a border flag** (`MKGUI_PANEL_BORDER`, `MKGUI_VBOX_BORDER`, `MKGUI_HBOX_BORDER`, `MKGUI_FORM_BORDER`): always have padding regardless of nesting (the border creates a visual boundary)
- Use `MKGUI_NO_PAD` to explicitly suppress padding on any container

### Weight-based layout

Children in HBOX/VBOX use a weight-based distribution system. The `weight` field controls how space is allocated:

- **`MKGUI_FIXED`** flag -- Removed from flex distribution. With `w`/`h` > 0: use that exact pixel size. With `w`/`h` = 0: use natural size (font-derived for leaf widgets, content-measured for containers). Weight is ignored.
- **`weight = 0`** (default) -- Flexible with effective weight 1. Zero-initialized widgets are flexible by default.
- **`weight > 0`** -- Flexible with explicit weight. Remaining space is distributed proportionally. A weight-2 child gets twice the extra space of a weight-1 child.

For flexible children, `w`/`h` is the minimum size in the layout direction. The child gets at least that much, plus its proportional share of remaining space. When `h` is 0, the natural height (derived from font height) is used as the minimum.

Use `mkgui_set_weight(ctx, id, weight)` to change weight at runtime.

### VBox

Stacks children vertically with 6px gap. By default children stretch to the full container width (override with `MKGUI_ALIGN_START/CENTER/END`). Content clips when it exceeds the container bounds. Set `MKGUI_SCROLL` to enable automatic scrollbars instead.

```c
MKGUI_W(MKGUI_VBOX,   ID_VBOX,  "", "", ID_TAB1, 0, 0, 0, 0, 0),
MKGUI_W(MKGUI_BUTTON, ID_BTN1,  "Fixed (28px)", "", ID_VBOX, 0, 28, MKGUI_FIXED, 0, 0),
MKGUI_W(MKGUI_INPUT,  ID_INP1,  "",             "", ID_VBOX, 0, 0, MKGUI_FIXED, 0, 0),
MKGUI_W(MKGUI_BUTTON, ID_BTN2,  "Flex (fills)", "", ID_VBOX, 0, 0, 0, 0, 0),
```

### HBox

Stacks children horizontally with 6px gap. By default children stretch to the full container height (override with `MKGUI_ALIGN_START/CENTER/END`). Content clips when it exceeds the container bounds. Set `MKGUI_SCROLL` to enable automatic scrollbars instead.

```c
MKGUI_W(MKGUI_HBOX,   ID_HBOX, "", "", ID_TAB1, 0, 0, 0, 0, 0),
MKGUI_W(MKGUI_PANEL,  ID_LEFT, "", "", ID_HBOX, 200, 0, MKGUI_FIXED, MKGUI_PANEL_BORDER, 0),
MKGUI_W(MKGUI_PANEL,  ID_MID,  "", "", ID_HBOX, 0, 0, 0, 0, 0),
MKGUI_W(MKGUI_PANEL,  ID_RIGHT,"", "", ID_HBOX, 150, 0, MKGUI_FIXED, MKGUI_PANEL_BORDER, 0),
```

### Form

Two-column form layout for label+control pairs. Children are paired in order: 1st is label, 2nd is control, 3rd is label, 4th is control, etc. The label column auto-sizes to the widest label. Each row is 24px tall with 6px gap.

```c
MKGUI_W(MKGUI_FORM,     ID_FORM,  "", "", ID_TAB1, 0, 0, 0, 0, 0),
MKGUI_W(MKGUI_LABEL,    ID_LBL1,  "Name:",    "", ID_FORM, 0, 0, 0, 0, 0),
MKGUI_W(MKGUI_INPUT,    ID_INP1,  "",          "", ID_FORM, 0, 0, 0, 0, 0),
MKGUI_W(MKGUI_LABEL,    ID_LBL2,  "Email:",   "", ID_FORM, 0, 0, 0, 0, 0),
MKGUI_W(MKGUI_INPUT,    ID_INP2,  "",          "", ID_FORM, 0, 0, 0, 0, 0),
MKGUI_W(MKGUI_LABEL,    ID_LBL3,  "Category:","", ID_FORM, 0, 0, 0, 0, 0),
MKGUI_W(MKGUI_DROPDOWN, ID_DRP1,  "",          "", ID_FORM, 0, 0, 0, 0, 0),
```

The widget editor automatically inserts a label when dropping a non-label widget into a FORM, ensuring the pairing is always correct. Dropping a label by itself places it in the label column.

Containers can be nested. For example, an HBOX containing a VBOX and a FORM side by side, where the VBOX has a fixed width and the FORM flexes to fill remaining space.

## Tabs

```c
MKGUI_W(MKGUI_TABS, ID_TABS, "",         "", ID_WINDOW, 0, 0, 0, 0, 0),
MKGUI_W(MKGUI_TAB,  ID_TAB1, "General",  "", ID_TABS, 0, 0, 0, 0, 0),
MKGUI_W(MKGUI_TAB,  ID_TAB2, "Settings", "", ID_TABS, 0, 0, 0, 0, 0),

// Widgets inside tabs
MKGUI_W(MKGUI_BUTTON, ID_BTN, "OK", "", ID_TAB1, 80, 0, MKGUI_FIXED, 0, 0),
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

mkgui uses SVG icons rendered via PlutoSVG/PlutoVG (embedded, MIT licensed). Icons follow Freedesktop standard naming (e.g. `"document-save"`, `"folder"`, `"dialog-warning"`).

```c
void mkgui_set_icon(struct mkgui_ctx *ctx, uint32_t widget_id, const char *icon_name);
void mkgui_set_treenode_icon(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id, const char *icon_name);
```

### Loading SVG icons

```c
int32_t mkgui_icon_load_svg(struct mkgui_ctx *ctx, const char *name, const char *path);
uint32_t mkgui_icon_load_svg_dir(struct mkgui_ctx *ctx, const char *dir_path);
uint32_t mkgui_icon_load_app_icons(struct mkgui_ctx *ctx, const char *app_name);
const char *mkgui_icon_get_dir(struct mkgui_ctx *ctx);
```

`mkgui_icon_load_svg` loads a single SVG file and registers it under `name`. `mkgui_icon_load_svg_dir` batch-loads all `.svg` files from a flat directory (filename minus `.svg` extension becomes the icon name). All icons (including toolbar icons) are loaded at the same size.

`mkgui_icon_load_app_icons` resolves the icon directory automatically using standard platform paths, then loads all SVGs from it. The search order is:

1. `$<APPNAME>_ICON_DIR` environment variable (override for development/packaging)
2. `../share/<app_name>/icons/` relative to the executable (covers FHS installs)
3. `$XDG_DATA_HOME/<app_name>/icons/` (default: `~/.local/share/`)
4. Each `$XDG_DATA_DIRS` entry + `/<app_name>/icons/` (default: `/usr/share:/usr/local/share`)
5. `./icons/` (development fallback)

On Windows, the executable's own directory is checked instead of the XDG paths.

`mkgui_icon_get_dir` returns the resolved icon directory path, or NULL if no icons were loaded.

```c
mkgui_set_app_class(ctx, "myapp");
mkgui_icon_load_app_icons(ctx, "myapp");
// Loads /usr/share/myapp/icons/document-save.svg as "document-save"
```

SVG sources are cached in memory. Icons are automatically re-rasterized on scale or theme changes. Icons use `currentColor` from the SVG spec, so they follow the theme text color.

The `extract_icons` build tool (built to `out/extract_icons`) reads the editor-generated icon list (`myapp_icons.txt`) and an optional user-managed file (`myapp_icons_extra.txt`) for dynamically loaded icons. The output directory is cleaned and rebuilt from scratch each run. Icons with source paths are copied directly; icons without are resolved from a theme directory. The extra file is never touched by the editor.

### System icon theme fallback (Linux)

On Linux, if an icon name is not found in the bundled `icons/` directory, mkgui automatically searches the user's installed system icon theme. This means applications can run without a bundled icon directory on Linux and still display correct icons.

The system theme is detected once at init from (in order): `$MKGUI_ICON_THEME` environment variable, `gtk-icon-theme-name` in GTK 3/4 settings, KDE `kdeglobals` `[Icons] Theme=`, or `hicolor` as final fallback. The full Freedesktop `Inherits=` chain is followed (e.g. Papirus-Dark -> Papirus -> breeze-dark -> hicolor). For `-Dark`/`-Light` theme variants, the base theme is automatically inserted into the chain.

Icons are loaded lazily on first use and cached. A negative cache prevents repeated filesystem lookups for names that don't exist in any theme. Bundled icons always take priority over the system theme.

On Windows, there is no system theme support. The bundled `icons/` directory is required.

### Missing icons

If a widget references an icon name that isn't loaded and it cannot be resolved from the system theme, a built-in placeholder (magenta diamond) is shown instead. This makes unresolved icons immediately visible during development.

### Custom icons

```c
int32_t mkgui_icon_add(const char *name, const uint32_t *pixels, int32_t w, int32_t h);
```

Register custom ARGB icons at any size. Custom icons override SVG icons with the same name. Pixels are copied internally. Returns the icon index, or -1 on failure.

### Icon browser

```c
uint32_t mkgui_icon_browser(struct mkgui_ctx *ctx, int32_t size,
                            char *out_name, uint32_t name_size,
                            char *out_path, uint32_t path_size);

uint32_t mkgui_icon_browser_theme(struct mkgui_ctx *ctx, const char *theme_dir,
                                  int32_t size, char *out, uint32_t out_size);
```

`mkgui_icon_browser` opens a modal browser showing icons from available Freedesktop icon themes. On Linux, it scans system icon directories (`/usr/share/icons/`, `$XDG_DATA_HOME/icons/`, etc.) in addition to local directories. On Windows, only local theme directories are scanned. A theme dropdown allows switching between themes; the icon array is reset on each switch to avoid stale icons. Returns 1 and writes the selected icon name to `out_name` and the full source path of the selected SVG to `out_path` if the user picked an icon, 0 on cancel. Multiple themes can coexist, allowing users to mix icons from different icon sets.

`mkgui_icon_browser_theme` is a narrower variant that browses only the icons in a single explicit theme directory. It writes the chosen icon name (no path) to `out`. Useful when an application bundles its own custom theme and wants a browser scoped to it.

### Font

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
- `mkgui_context_menu_add()` -- Add an item. `id` is returned in `MKGUI_EVENT_CONTEXT_MENU`. `icon` is a Freedesktop icon name (or NULL). `flags` can include `MKGUI_DISABLED`, `MKGUI_MENUITEM_SEPARATOR`, `MKGUI_MENUITEM_CHECK`, `MKGUI_MENUITEM_RADIO`, `MKGUI_MENUITEM_CHECKED`
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
    mkgui_context_menu_add(ctx, 200, "Name", NULL, MKGUI_MENUITEM_CHECK | MKGUI_MENUITEM_CHECKED);
    mkgui_context_menu_add(ctx, 201, "Size", NULL, MKGUI_MENUITEM_CHECK | MKGUI_MENUITEM_CHECKED);
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
uint32_t mkgui_message_box(struct mkgui_ctx *ctx, const char *title, const char *message,
                           uint32_t icon_type, uint32_t buttons);
```

Displays a message with a stock icon and a configurable button set. Blocks until dismissed. Returns one of the `MKGUI_DLG_RESULT_*` constants indicating which button was pressed (or `MKGUI_DLG_RESULT_CANCEL` if the window was closed).

**Icon constants:**

| Constant | Icon |
|----------|------|
| `MKGUI_DLG_ICON_NONE` | No icon |
| `MKGUI_DLG_ICON_INFO` | `dialog-information` |
| `MKGUI_DLG_ICON_WARNING` | `dialog-warning` |
| `MKGUI_DLG_ICON_ERROR` | `dialog-error` |
| `MKGUI_DLG_ICON_QUESTION` | `dialog-question` |

**Button set constants:**

| Constant | Buttons |
|----------|---------|
| `MKGUI_DLG_BUTTONS_OK` | OK |
| `MKGUI_DLG_BUTTONS_OK_CANCEL` | OK, Cancel |
| `MKGUI_DLG_BUTTONS_YES_NO` | Yes, No |
| `MKGUI_DLG_BUTTONS_YES_NO_CANCEL` | Yes, No, Cancel |
| `MKGUI_DLG_BUTTONS_RETRY_CANCEL` | Retry, Cancel |

**Result constants:**

| Constant | Meaning |
|----------|---------|
| `MKGUI_DLG_RESULT_OK` | OK pressed |
| `MKGUI_DLG_RESULT_CANCEL` | Cancel pressed or window closed |
| `MKGUI_DLG_RESULT_YES` | Yes pressed |
| `MKGUI_DLG_RESULT_NO` | No pressed |
| `MKGUI_DLG_RESULT_RETRY` | Retry pressed |

```c
uint32_t r = mkgui_message_box(ctx, "Save?", "Save changes before exit?",
                               MKGUI_DLG_ICON_QUESTION, MKGUI_DLG_BUTTONS_YES_NO_CANCEL);
if(r == MKGUI_DLG_RESULT_YES)    { save_and_exit(); }
if(r == MKGUI_DLG_RESULT_NO)     { exit_no_save(); }
if(r == MKGUI_DLG_RESULT_CANCEL) { /* keep editing */ }
```

### Confirm dialog

```c
uint32_t mkgui_confirm_dialog(struct mkgui_ctx *ctx, const char *title, const char *message);
```

Shorthand for `mkgui_message_box` with `MKGUI_DLG_ICON_QUESTION` and `MKGUI_DLG_BUTTONS_OK_CANCEL`. Returns 1 if OK, 0 if cancelled.

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
    const char *label;                              // "Images", "Source Files"
    const char *pattern;                            // "*.png;*.jpg;*.jpeg", "*.c;*.h"
};

struct mkgui_file_dialog_opts {
    const char *start_path;                         // NULL = home directory
    const struct mkgui_file_filter *filters;        // NULL = no filters
    uint32_t filter_count;
    const char *default_name;                       // save mode only, NULL = empty
    uint32_t multi_select;                          // open mode only, 0 or 1
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
                                      const struct mkgui_widget *widgets,
                                      uint32_t count,
                                      const char *title,
                                      int32_t w, int32_t h);

// Destroy a child window (do not use mkgui_destroy for children)
void mkgui_destroy_child(struct mkgui_ctx *ctx);
```

## Rendering

All widget rendering is software-based (no GPU). The framebuffer is blitted to the X11 window via MIT-SHM (`XShmPutImage`).

### Rendering pipeline

mkgui uses partial rendering via dirty rectangles. Widgets mark themselves dirty on state changes, dirty rects are merged when spatially close, and only affected regions are cleared, re-rendered, and blitted. Each dirty rect is processed individually: clip bounds are set, the background is cleared, overlapping widgets are rendered, and `platform_blit_region` blits just that region. Hot-path operations (hover, scroll, slider drag, text input, click selection) dirty only the affected widget. Full redraws (`dirty_all`) are reserved for structural changes: window resize/expose, tab switch, focus change, widget add/remove, theme change, and scale change. All rendering goes to a client-side framebuffer, then `XShmPutImage` (Linux) or `BitBlt` (Windows) blits the result to the window.

### Glyph and icon storage

Glyph and icon bitmaps are stored in linear memory for cache-friendly rendering. Each glyph or icon is a contiguous block of pixels (row-major, no stride padding), and each entry stores a byte offset into the flat buffer. This means the entire bitmap for a glyph or icon fits in a few cache lines with no gaps, which is optimal for the CPU-side alpha blending path. The buffers are rebuilt automatically on scale or theme changes.

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

`mkgui_init()` auto-detects light vs dark from the environment. Apps can override at runtime with `mkgui_set_theme`:

```c
// Built-in themes
struct mkgui_theme default_theme(void);  // dark (Breeze)
struct mkgui_theme light_theme(void);    // light

// Apply a theme (re-renders icons automatically)
void mkgui_set_theme(struct mkgui_ctx *ctx, struct mkgui_theme theme);
```

`mkgui_init()` detects dark vs light in this order:

1. `MKGUI_THEME=light` or `MKGUI_THEME=dark` environment variable (explicit override)
2. `GTK_THEME` contains `:dark` or `-dark` (GNOME/XFCE convention)
3. Windows registry `HKCU\...\Themes\Personalize\AppsUseLightTheme` (Win32 only)
4. Falls back to dark

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
| `scrollbar_bg` | `#1d2023` | Scrollbar track |
| `scrollbar_thumb` | `#4d4d4d` | Scrollbar thumb |
| `scrollbar_thumb_hover` | `#5a5a5a` | Scrollbar thumb on hover/drag |
| `highlight` | `#3daee9` | Focus border, menu hover, selection accents (KDE Highlight color) |
| `header_bg` | `#2a2e32` | Listview column header background |
| `listview_alt` | `#2a2e32` | Alternating row background |
| `accent` | `#2a7ab5` | Accent color (spinner arc, progress bar fill, cell progress) |
| `corner_radius` | `3` | Rounded corner radius in pixels |

## Keyboard accelerators

Bind keyboard shortcuts to widgets. When a shortcut matches, the bound widget receives an event -- `MKGUI_EVENT_MENU` for menu items, `MKGUI_EVENT_ACCEL` for everything else.

```c
void mkgui_accel_add(struct mkgui_ctx *ctx, uint32_t id, uint32_t keymod, uint32_t keysym);
void mkgui_accel_remove(struct mkgui_ctx *ctx, uint32_t id);
void mkgui_accel_clear(struct mkgui_ctx *ctx);
```

Modifier flags: `MKGUI_MOD_CONTROL`, `MKGUI_MOD_SHIFT`, `MKGUI_MOD_ALT`. Combine with bitwise OR.

```c
mkgui_accel_add(ctx, ID_SAVE, MKGUI_MOD_CONTROL, 's');
mkgui_accel_add(ctx, ID_REDO, MKGUI_MOD_CONTROL | MKGUI_MOD_SHIFT, 'z');
mkgui_accel_add(ctx, ID_HELP, MKGUI_MOD_CONTROL, MKGUI_KEY_F1);
```

Menu items with accelerators automatically show the shortcut text right-aligned in the popup (e.g. "Ctrl+S"). No manual text formatting needed.

Accelerators dispatch after context menu keys but before Tab and Ctrl+A/C/V/X. This means app shortcuts take priority over default clipboard bindings when a text widget is focused. Key matching is case-insensitive.

Maximum 64 accelerators per context.

## Undo/redo

Input and textarea widgets support undo/redo via Ctrl+Z and Ctrl+Y (or Ctrl+Shift+Z). This is built-in and requires no setup.

Each widget maintains its own undo ring buffer (32 snapshots). All text mutations are covered: typing, backspace, delete, cut, paste, Enter (textarea), and text drag-and-drop. Consecutive single-character inserts within 500ms are coalesced into one undo step to avoid excessive snapshots during normal typing.

## Inline cell editing

Listview, treeview, gridview, and itemview support inline cell editing. The widget must opt in with the appropriate style flag:

- `MKGUI_LISTVIEW_EDITABLE` -- first column of listview
- `MKGUI_TREEVIEW_EDITABLE` -- node label in treeview
- `MKGUI_GRIDVIEW_MULTI_SELECT` enables selection, editing uses `MKGUI_EVENT_GRID_CLICK` plus programmatic `mkgui_cell_edit_begin`
- `MKGUI_ITEMVIEW_EDITABLE` -- label of itemview item

Once enabled, cell editing starts in two ways:

1. **F2** on the selected row/node/item begins editing.
2. **Slow double-click**: clicking an already-selected item a second time within a 400ms-1500ms window (longer than a normal double-click) begins editing, matching file manager rename behavior.

While editing, the cell shows a text input with cursor and selection. Click inside the cell to position the cursor, drag to select. Click outside the cell commits. Scrolling cancels. Enter commits, Escape cancels. Clipboard (Ctrl+C/X/V), Ctrl+A, and undo/redo all work inside the edit overlay.

On commit, `MKGUI_EVENT_CELL_EDIT_COMMIT` fires with `ev->id` set to the widget, `ev->value` set to the row (or node id), and `ev->col` set to the column. Retrieve the new text with `mkgui_cell_edit_get_text` while the event is being handled. The application is responsible for applying the change to its backing data.

```c
void mkgui_cell_edit_begin(struct mkgui_ctx *ctx, uint32_t widget_id,
                           int32_t row, int32_t col, const char *text);
void mkgui_cell_edit_cancel(struct mkgui_ctx *ctx);
const char *mkgui_cell_edit_get_text(struct mkgui_ctx *ctx);
uint32_t mkgui_cell_edit_active(struct mkgui_ctx *ctx);
```

- `mkgui_cell_edit_begin` programmatically starts editing a specific cell with a pre-filled initial text. Useful for "Rename" context menu actions.
- `mkgui_cell_edit_cancel` dismisses the edit overlay without committing.
- `mkgui_cell_edit_get_text` returns the current edit buffer (valid during `MKGUI_EVENT_CELL_EDIT_COMMIT`).
- `mkgui_cell_edit_active` returns non-zero while an edit is in progress.

Only one cell edit can be active per context at a time (similar to focus).

```c
case MKGUI_EVENT_CELL_EDIT_COMMIT: {
    if(ev->id == ID_FILE_LIST) {
        const char *new_name = mkgui_cell_edit_get_text(ctx);
        rename_file(ev->value, new_name);
        refresh_listview();
    }
} break;
```

## File drag-and-drop

Accept file drops from the OS file manager. Opt-in per context.

```c
void mkgui_drop_enable(struct mkgui_ctx *ctx);
uint32_t mkgui_drop_count(struct mkgui_ctx *ctx);
const char *mkgui_drop_file(struct mkgui_ctx *ctx, uint32_t index);
```

Call `mkgui_drop_enable(ctx)` after creation. When files are dropped, `MKGUI_EVENT_FILE_DROP` fires with `ev->value` set to the file count. Retrieve paths with `mkgui_drop_file()`:

```c
mkgui_drop_enable(ctx);

// in event handler:
case MKGUI_EVENT_FILE_DROP: {
    for(uint32_t i = 0; i < mkgui_drop_count(ctx); ++i) {
        printf("Dropped: %s\n", mkgui_drop_file(ctx, i));
    }
} break;
```

Paths are valid until the next drop event or context destruction. Uses XDnd v5 protocol on Linux and WM_DROPFILES on Windows. Maximum 256 files per drop.

## DPI scaling

mkgui auto-detects the display scale factor on startup:

- **Linux**: reads `Xft.dpi` from X resources, falls back to physical display DPI
- **Windows**: uses `GetDpiForWindow()` or `GetDeviceCaps(LOGPIXELSX)`

Override auto-detection with `mkgui_set_scale()` or the `MKGUI_SCALE` environment variable:

```c
void mkgui_set_scale(struct mkgui_ctx *ctx, float scale);
float mkgui_get_scale(struct mkgui_ctx *ctx);
```

The `MKGUI_SCALE` environment variable accepts three formats:

| Format | Example | Result |
|--------|---------|--------|
| Float | `MKGUI_SCALE=1.5` | 1.5x scale |
| Percentage | `MKGUI_SCALE=150%` | 1.5x scale |
| DPI | `MKGUI_SCALE=144dpi` | 1.5x scale (144/96) |

The environment variable takes precedence over auto-detection. Scale range is 0.5 to 4.0.

All widgets, fonts, spacing, icons, and dialog windows scale automatically. User-provided pixel values in widget definitions are scaled by the layout engine.

## Complete example

See [demo.c](../demo.c) for a full working application demonstrating all widget types.

## License

MIT
