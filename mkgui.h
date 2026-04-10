// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT
//
// mkgui public API header -- single source of truth for all public types,
// constants, enums, and function declarations.
//
// Unity build: mkgui.c defines MKGUI_API as static before including this.
// Library build: mkgui.c defines MKGUI_API as empty (extern linkage).
// Consumer: MKGUI_API defaults to empty below.

#ifndef MKGUI_H
#define MKGUI_H

#include <stdint.h>

#ifndef MKGUI_API
#define MKGUI_API
#endif

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define MKGUI_MAX_COLS             32
#define MKGUI_MAX_TEXT             256
#define MKGUI_MAX_DROPDOWN         64
#define MKGUI_MAX_POPUPS           8
#define MKGUI_ROW_HEIGHT           20
#define MKGUI_TAB_HEIGHT           26
#define MKGUI_MENU_HEIGHT          22
#define MKGUI_SCROLLBAR_W          14
#define MKGUI_SPLIT_THICK          5
#define MKGUI_SPLIT_MIN_PX        50
#define MKGUI_BOX_GAP              6
#define MKGUI_BOX_PAD              6
#define MKGUI_MARGIN               3
#define MKGUI_ICON_SIZE            18
#define MKGUI_ICON_NAME_LEN        64
#define MKGUI_MAX_ICONS            32768
#define MKGUI_MAX_MULTI_SEL        4096
#define MKGUI_MAX_STATUSBAR_SECTIONS 8
#define MKGUI_MAX_CTXMENU          64
#define MKGUI_PATHBAR_MAX_SEGS     64
#define MKGUI_PATHBAR_HEIGHT       24
#define MKGUI_TOOLBAR_HEIGHT_DEFAULT 28
#define MKGUI_TOOLBAR_BTN_W        28
#define MKGUI_TOOLBAR_SEP_W        8
#define MKGUI_STATUSBAR_HEIGHT     22
#define MKGUI_MAX_TIMERS           8
#define MKGUI_MAX_ACCELS           64
#define MKGUI_DROP_MAX             256

// ---------------------------------------------------------------------------
// Key constants
// ---------------------------------------------------------------------------

#define MKGUI_KEY_BACKSPACE        0xff08
#define MKGUI_KEY_TAB              0xff09
#define MKGUI_KEY_ISO_LEFT_TAB     0xfe20
#define MKGUI_KEY_RETURN           0xff0d
#define MKGUI_KEY_ESCAPE           0xff1b
#define MKGUI_KEY_SPACE            0x0020
#define MKGUI_KEY_DELETE           0xffff
#define MKGUI_KEY_HOME             0xff50
#define MKGUI_KEY_LEFT             0xff51
#define MKGUI_KEY_UP               0xff52
#define MKGUI_KEY_RIGHT            0xff53
#define MKGUI_KEY_DOWN             0xff54
#define MKGUI_KEY_PAGE_UP          0xff55
#define MKGUI_KEY_PAGE_DOWN        0xff56
#define MKGUI_KEY_END              0xff57
#define MKGUI_KEY_F1               0xffbe
#define MKGUI_KEY_F2               0xffbf

#define MKGUI_MOD_SHIFT            (1 << 0)
#define MKGUI_MOD_CONTROL          (1 << 2)
#define MKGUI_MOD_ALT              (1 << 3)

// ---------------------------------------------------------------------------
// Widget types
// ---------------------------------------------------------------------------

enum {
	MKGUI_WINDOW,
	MKGUI_BUTTON,
	MKGUI_LABEL,
	MKGUI_INPUT,
	MKGUI_CHECKBOX,
	MKGUI_DROPDOWN,
	MKGUI_SLIDER,
	MKGUI_LISTVIEW,
	MKGUI_MENU,
	MKGUI_MENUITEM,
	MKGUI_TABS,
	MKGUI_TAB,
	MKGUI_HSPLIT,
	MKGUI_VSPLIT,
	MKGUI_TREEVIEW,
	MKGUI_STATUSBAR,
	MKGUI_TOOLBAR,
	MKGUI_SPINBOX,
	MKGUI_RADIO,
	MKGUI_PROGRESS,
	MKGUI_TEXTAREA,
	MKGUI_GROUP,
	MKGUI_SPINNER,
	MKGUI_ITEMVIEW,
	MKGUI_PANEL,
	MKGUI_SCROLLBAR,
	MKGUI_IMAGE,
	MKGUI_GLVIEW,
	MKGUI_CANVAS,
	MKGUI_VBOX,
	MKGUI_HBOX,
	MKGUI_FORM,
	MKGUI_SPACER,
	MKGUI_PATHBAR,
	MKGUI_IPINPUT,
	MKGUI_TOGGLE,
	MKGUI_COMBOBOX,
	MKGUI_DATEPICKER,
	MKGUI_GRIDVIEW,
	MKGUI_RICHLIST,
	MKGUI_METER,
	MKGUI_DIVIDER,
};

// ---------------------------------------------------------------------------
// Universal flags (stored in 'flags' field, apply to any widget)
// ---------------------------------------------------------------------------

#define MKGUI_REGION_TOP           (1u << 4)
#define MKGUI_REGION_BOTTOM        (1u << 5)
#define MKGUI_REGION_LEFT          (1u << 6)
#define MKGUI_REGION_RIGHT         (1u << 7)
#define MKGUI_HIDDEN               (1u << 8)
#define MKGUI_DISABLED             (1u << 9)
#define MKGUI_SCROLL               (1u << 10)
#define MKGUI_NO_PAD               (1u << 11)
#define MKGUI_ALIGN_START          (1u << 12)
#define MKGUI_ALIGN_CENTER         (2u << 12)
#define MKGUI_ALIGN_END            (3u << 12)
#define MKGUI_ALIGN_MASK           (3u << 12)
#define MKGUI_FIXED                (1u << 14)
#define MKGUI_VERTICAL             (1u << 15)

// ---------------------------------------------------------------------------
// Widget style flags (stored in 'style' field, per-widget-type)
// Each widget type has its own flag namespace starting at bit 0.
// ---------------------------------------------------------------------------

// -- Checkbox --
#define MKGUI_CHECKBOX_CHECKED         (1u << 0)

// -- Radio --
#define MKGUI_RADIO_CHECKED            (1u << 0)

// -- Toggle --
#define MKGUI_TOGGLE_CHECKED           (1u << 0)

// -- Button --
#define MKGUI_BUTTON_CHECKED           (1u << 0)
#define MKGUI_BUTTON_SEPARATOR         (1u << 1)

// -- Input --
#define MKGUI_INPUT_PASSWORD           (1u << 0)
#define MKGUI_INPUT_READONLY           (1u << 1)
#define MKGUI_INPUT_NUMERIC            (1u << 2)

// -- Textarea --
#define MKGUI_TEXTAREA_READONLY        (1u << 0)

// -- Datepicker --
#define MKGUI_DATEPICKER_READONLY      (1u << 0)

// -- Menu item / context menu --
#define MKGUI_MENUITEM_SEPARATOR       (1u << 0)
#define MKGUI_MENUITEM_CHECK           (1u << 1)
#define MKGUI_MENUITEM_RADIO           (1u << 2)
#define MKGUI_MENUITEM_CHECKED         (1u << 3)

// -- Panel --
#define MKGUI_PANEL_BORDER             (1u << 0)
#define MKGUI_PANEL_SUNKEN             (1u << 1)

// -- VBox / HBox / Form --
#define MKGUI_VBOX_BORDER              (1u << 0)
#define MKGUI_HBOX_BORDER              (1u << 0)
#define MKGUI_FORM_BORDER              (1u << 0)

// -- Canvas --
#define MKGUI_CANVAS_BORDER            (1u << 0)
#define MKGUI_CANVAS_SUNKEN            (1u << 1)

// -- GLView --
#define MKGUI_GLVIEW_BORDER            (1u << 0)

// -- Image --
#define MKGUI_IMAGE_BORDER             (1u << 0)
#define MKGUI_IMAGE_STRETCH            (1u << 1)

// -- Slider --
#define MKGUI_SLIDER_MIXER             (1u << 0)

// -- Meter --
#define MKGUI_METER_TEXT               (1u << 0)

// -- Tab --
#define MKGUI_TAB_CLOSABLE             (1u << 0)

// -- Listview / Gridview / Treeview --
#define MKGUI_LISTVIEW_MULTI_SELECT    (1u << 0)
#define MKGUI_LISTVIEW_EDITABLE        (1u << 1)
#define MKGUI_GRIDVIEW_MULTI_SELECT    (1u << 0)
#define MKGUI_TREEVIEW_MULTI_SELECT    (1u << 0)
#define MKGUI_TREEVIEW_EDITABLE        (1u << 1)
#define MKGUI_ITEMVIEW_EDITABLE        (1u << 0)

// -- Label --
#define MKGUI_LABEL_TRUNCATE           (1u << 0)
#define MKGUI_LABEL_LINK               (1u << 1)
#define MKGUI_LABEL_WRAP               (1u << 2)

// -- Toolbar --
#define MKGUI_TOOLBAR_ICONS_TEXT       0
#define MKGUI_TOOLBAR_ICONS_ONLY       (1u << 0)
#define MKGUI_TOOLBAR_TEXT_ONLY        (2u << 0)
#define MKGUI_TOOLBAR_MODE_MASK        (3u << 0)

// -- Group --
#define MKGUI_GROUP_COLLAPSIBLE        (1u << 0)
#define MKGUI_GROUP_COLLAPSED          (1u << 1)

// -- Progress --
#define MKGUI_PROGRESS_SHIMMER         (1u << 0)

// ---------------------------------------------------------------------------
// Event types
// ---------------------------------------------------------------------------

enum {
	MKGUI_EVENT_NONE,
	MKGUI_EVENT_CLICK,
	MKGUI_EVENT_MENU,
	MKGUI_EVENT_TAB_CHANGED,
	MKGUI_EVENT_LISTVIEW_SORT,
	MKGUI_EVENT_LISTVIEW_COL_REORDER,
	MKGUI_EVENT_LISTVIEW_SELECT,
	MKGUI_EVENT_LISTVIEW_DBLCLICK,
	MKGUI_EVENT_LISTVIEW_REORDER,
	MKGUI_EVENT_INPUT_CHANGED,
	MKGUI_EVENT_CHECKBOX_CHANGED,
	MKGUI_EVENT_DROPDOWN_CHANGED,
	MKGUI_EVENT_SLIDER_CHANGED,
	MKGUI_EVENT_SPLIT_MOVED,
	MKGUI_EVENT_TREEVIEW_SELECT,
	MKGUI_EVENT_TREEVIEW_EXPAND,
	MKGUI_EVENT_TREEVIEW_COLLAPSE,
	MKGUI_EVENT_TREEVIEW_MOVE,
	MKGUI_EVENT_SPINBOX_CHANGED,
	MKGUI_EVENT_RADIO_CHANGED,
	MKGUI_EVENT_TEXTAREA_CHANGED,
	MKGUI_EVENT_KEY,
	MKGUI_EVENT_CLOSE,
	MKGUI_EVENT_RESIZE,
	MKGUI_EVENT_ITEMVIEW_SELECT,
	MKGUI_EVENT_ITEMVIEW_DBLCLICK,
	MKGUI_EVENT_SCROLL,
	MKGUI_EVENT_CONTEXT,
	MKGUI_EVENT_INPUT_SUBMIT,
	MKGUI_EVENT_FOCUS,
	MKGUI_EVENT_UNFOCUS,
	MKGUI_EVENT_HOVER_ENTER,
	MKGUI_EVENT_HOVER_LEAVE,
	MKGUI_EVENT_SLIDER_START,
	MKGUI_EVENT_SLIDER_END,
	MKGUI_EVENT_TREEVIEW_DBLCLICK,
	MKGUI_EVENT_TEXTAREA_CURSOR,
	MKGUI_EVENT_DRAG_START,
	MKGUI_EVENT_DRAG_END,
	MKGUI_EVENT_TAB_CLOSE,
	MKGUI_EVENT_PATHBAR_NAV,
	MKGUI_EVENT_PATHBAR_SUBMIT,
	MKGUI_EVENT_IPINPUT_CHANGED,
	MKGUI_EVENT_TOGGLE_CHANGED,
	MKGUI_EVENT_COMBOBOX_CHANGED,
	MKGUI_EVENT_COMBOBOX_SUBMIT,
	MKGUI_EVENT_DATEPICKER_CHANGED,
	MKGUI_EVENT_GRID_CLICK,
	MKGUI_EVENT_GRID_CHECK,
	MKGUI_EVENT_GRIDVIEW_SELECT,
	MKGUI_EVENT_GRIDVIEW_REORDER,
	MKGUI_EVENT_RICHLIST_SELECT,
	MKGUI_EVENT_RICHLIST_DBLCLICK,
	MKGUI_EVENT_CONTEXT_HEADER,
	MKGUI_EVENT_CONTEXT_MENU,
	MKGUI_EVENT_TIMER,
	MKGUI_EVENT_ACCEL,
	MKGUI_EVENT_FILE_DROP,
	MKGUI_EVENT_CELL_EDIT_COMMIT,
};

// ---------------------------------------------------------------------------
// Cell / view mode enums
// ---------------------------------------------------------------------------

enum {
	MKGUI_CELL_TEXT,
	MKGUI_CELL_PROGRESS,
	MKGUI_CELL_ICON_TEXT,
	MKGUI_CELL_SIZE,
	MKGUI_CELL_DATE,
	MKGUI_CELL_CHECKBOX,
};

enum {
	MKGUI_GRID_TEXT,
	MKGUI_GRID_CHECK,
	MKGUI_GRID_CHECK_TEXT,
};

enum {
	MKGUI_VIEW_ICON,
	MKGUI_VIEW_THUMBNAIL,
	MKGUI_VIEW_COMPACT,
	MKGUI_VIEW_DETAIL,
};

// ---------------------------------------------------------------------------
// Dialog constants
// ---------------------------------------------------------------------------

#define MKGUI_DLG_ICON_NONE              0
#define MKGUI_DLG_ICON_INFO              1
#define MKGUI_DLG_ICON_WARNING           2
#define MKGUI_DLG_ICON_ERROR             3
#define MKGUI_DLG_ICON_QUESTION          4

#define MKGUI_DLG_BUTTONS_OK             0
#define MKGUI_DLG_BUTTONS_OK_CANCEL      1
#define MKGUI_DLG_BUTTONS_YES_NO         2
#define MKGUI_DLG_BUTTONS_YES_NO_CANCEL  3
#define MKGUI_DLG_BUTTONS_RETRY_CANCEL   4

#define MKGUI_DLG_RESULT_NONE            0
#define MKGUI_DLG_RESULT_OK              1
#define MKGUI_DLG_RESULT_CANCEL          2
#define MKGUI_DLG_RESULT_YES             3
#define MKGUI_DLG_RESULT_NO              4
#define MKGUI_DLG_RESULT_RETRY           5

// ---------------------------------------------------------------------------
// Public types
// ---------------------------------------------------------------------------

struct mkgui_widget {
	uint32_t type;
	uint32_t id;
	uint32_t parent_id;
	int32_t w, h;
	uint32_t flags;
	uint32_t style;
	uint32_t weight;
	int32_t margin_l, margin_r, margin_t, margin_b;
	int32_t label_tw;
	char label[MKGUI_MAX_TEXT];
	char icon[MKGUI_ICON_NAME_LEN];
};

// Positional initializer: type, id, label, icon, parent_id, w, h, flags, style, weight
#define MKGUI_W(t, i, lbl, ico, pid, ww, hh, fl, st, wt) \
	{ (t), (i), (pid), (ww), (hh), (fl), (st), (wt), 0, 0, 0, 0, 0, lbl, ico }

struct mkgui_event {
	uint32_t type;
	uint32_t id;
	int32_t value;
	int32_t col;
	uint32_t keysym;
	uint32_t keymod;
};

struct mkgui_column {
	char label[MKGUI_MAX_TEXT];
	int32_t width;
	uint32_t cell_type;
};

struct mkgui_grid_column {
	char label[MKGUI_MAX_TEXT];
	int32_t width;
	uint32_t col_type;
};

struct mkgui_theme {
	uint32_t bg;
	uint32_t widget_bg;
	uint32_t widget_border;
	uint32_t widget_hover;
	uint32_t widget_press;
	uint32_t text;
	uint32_t text_disabled;
	uint32_t selection;
	uint32_t sel_text;
	uint32_t input_bg;
	uint32_t tab_active;
	uint32_t tab_inactive;
	uint32_t tab_hover;
	uint32_t menu_bg;
	uint32_t scrollbar_bg;
	uint32_t scrollbar_thumb;
	uint32_t scrollbar_thumb_hover;
	uint32_t highlight;
	uint32_t header_bg;
	uint32_t listview_alt;
	uint32_t accent;
	int32_t corner_radius;
};

struct mkgui_file_filter {
	char *label;
	char *pattern;
};

struct mkgui_file_dialog_opts {
	char *start_path;
	struct mkgui_file_filter *filters;
	uint32_t filter_count;
	char *default_name;
	uint32_t multi_select;
};

struct mkgui_richlist_row {
	char title[MKGUI_MAX_TEXT];
	char subtitle[MKGUI_MAX_TEXT];
	char right_text[64];
	uint32_t *thumbnail;
	int32_t thumb_w, thumb_h;
};

struct mkgui_icon_size {
	uint32_t *pixels;
	int32_t w, h;
};

// Opaque context
struct mkgui_ctx;

// ---------------------------------------------------------------------------
// Callback typedefs
// ---------------------------------------------------------------------------

typedef void (*mkgui_render_cb)(struct mkgui_ctx *ctx, void *userdata);
typedef void (*mkgui_row_cb)(uint32_t row, uint32_t col, char *buf, uint32_t buf_size, void *userdata);
typedef void (*mkgui_grid_cell_cb)(uint32_t row, uint32_t col, char *buf, uint32_t buf_size, void *userdata);
typedef void (*mkgui_richlist_cb)(uint32_t row, struct mkgui_richlist_row *out, void *userdata);
typedef void (*mkgui_itemview_label_cb)(uint32_t item, char *buf, uint32_t buf_size, void *userdata);
typedef void (*mkgui_itemview_icon_cb)(uint32_t item, char *buf, uint32_t buf_size, void *userdata);
typedef void (*mkgui_thumbnail_cb)(uint32_t item, uint32_t *pixels, int32_t w, int32_t h, void *userdata);
typedef void (*mkgui_canvas_cb)(struct mkgui_ctx *ctx, uint32_t id, uint32_t *pixels, int32_t x, int32_t y, int32_t w, int32_t h, void *userdata);
typedef void (*mkgui_timer_cb)(struct mkgui_ctx *ctx, uint32_t timer_id, void *userdata);
typedef void (*mkgui_event_cb)(struct mkgui_ctx *ctx, struct mkgui_event *ev, void *userdata);

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

MKGUI_API struct mkgui_ctx *mkgui_create(struct mkgui_widget *widgets, uint32_t count);
MKGUI_API void mkgui_destroy(struct mkgui_ctx *ctx);
MKGUI_API struct mkgui_ctx *mkgui_create_child(struct mkgui_ctx *parent, struct mkgui_widget *widgets, uint32_t count, const char *title, int32_t w, int32_t h);
MKGUI_API void mkgui_destroy_child(struct mkgui_ctx *ctx);
MKGUI_API void mkgui_set_app_class(struct mkgui_ctx *ctx, const char *app_class);
MKGUI_API void mkgui_set_window_instance(struct mkgui_ctx *ctx, const char *instance);
MKGUI_API void mkgui_set_window_icon(struct mkgui_ctx *ctx, struct mkgui_icon_size *sizes, uint32_t count);
MKGUI_API void mkgui_set_title(struct mkgui_ctx *ctx, const char *title);
MKGUI_API void mkgui_set_callback(struct mkgui_ctx *ctx, mkgui_event_cb cb, void *userdata);
MKGUI_API void mkgui_run(struct mkgui_ctx *ctx, mkgui_event_cb cb, void *userdata);
MKGUI_API void mkgui_quit(struct mkgui_ctx *ctx);
MKGUI_API uint32_t mkgui_add_timer(struct mkgui_ctx *ctx, uint64_t interval_ns, mkgui_timer_cb cb, void *userdata);
MKGUI_API void mkgui_remove_timer(struct mkgui_ctx *ctx, uint32_t timer_id);

// ---------------------------------------------------------------------------
// Widget management
// ---------------------------------------------------------------------------

MKGUI_API uint32_t mkgui_add_widget(struct mkgui_ctx *ctx, struct mkgui_widget w, uint32_t after_id);
MKGUI_API uint32_t mkgui_remove_widget(struct mkgui_ctx *ctx, uint32_t id);

// ---------------------------------------------------------------------------
// Base widget properties
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_set_enabled(struct mkgui_ctx *ctx, uint32_t id, uint32_t enabled);
MKGUI_API uint32_t mkgui_get_enabled(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_set_visible(struct mkgui_ctx *ctx, uint32_t id, uint32_t visible);
MKGUI_API uint32_t mkgui_get_visible(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_set_focus(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API uint32_t mkgui_has_focus(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_get_geometry(struct mkgui_ctx *ctx, uint32_t id, int32_t *x, int32_t *y, int32_t *w, int32_t *h);
MKGUI_API void mkgui_get_min_size(struct mkgui_ctx *ctx, int32_t *out_w, int32_t *out_h);
MKGUI_API void mkgui_set_flags(struct mkgui_ctx *ctx, uint32_t id, uint32_t flags);
MKGUI_API uint32_t mkgui_get_flags(struct mkgui_ctx *ctx, uint32_t id);

// ---------------------------------------------------------------------------
// Theme
// ---------------------------------------------------------------------------

MKGUI_API struct mkgui_theme default_theme(void);
MKGUI_API struct mkgui_theme light_theme(void);
MKGUI_API void mkgui_set_theme(struct mkgui_ctx *ctx, struct mkgui_theme theme);
MKGUI_API void mkgui_set_scale(struct mkgui_ctx *ctx, float scale);
MKGUI_API float mkgui_get_scale(struct mkgui_ctx *ctx);
MKGUI_API void mkgui_set_weight(struct mkgui_ctx *ctx, uint32_t id, uint32_t weight);
MKGUI_API void mkgui_toolbar_set_mode(struct mkgui_ctx *ctx, uint32_t toolbar_id, uint32_t mode);

// ---------------------------------------------------------------------------
// Tooltip
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_set_tooltip(struct mkgui_ctx *ctx, uint32_t id, const char *text);
MKGUI_API char *mkgui_get_tooltip(struct mkgui_ctx *ctx, uint32_t id);

// ---------------------------------------------------------------------------
// Icon
// ---------------------------------------------------------------------------

MKGUI_API int32_t mkgui_icon_add(const char *name, uint32_t *pixels, int32_t w, int32_t h);
MKGUI_API int32_t mkgui_icon_load_svg(struct mkgui_ctx *ctx, const char *name, const char *path);
MKGUI_API uint32_t mkgui_icon_load_svg_dir(struct mkgui_ctx *ctx, const char *dir_path);
MKGUI_API uint32_t mkgui_icon_load_app_icons(struct mkgui_ctx *ctx, const char *app_name);
MKGUI_API const char *mkgui_icon_get_dir(struct mkgui_ctx *ctx);
MKGUI_API void mkgui_set_icon(struct mkgui_ctx *ctx, uint32_t widget_id, const char *icon_name);
MKGUI_API void mkgui_set_treenode_icon(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id, const char *icon_name);
MKGUI_API uint32_t mkgui_icon_browser_theme(struct mkgui_ctx *ctx, const char *theme_dir, int32_t size, char *out, uint32_t out_size);
MKGUI_API uint32_t mkgui_icon_browser(struct mkgui_ctx *ctx, int32_t size, char *out_name, uint32_t name_size, char *out_path, uint32_t path_size);

// ---------------------------------------------------------------------------
// Button
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_button_set_text(struct mkgui_ctx *ctx, uint32_t id, const char *text);
MKGUI_API char *mkgui_button_get_text(struct mkgui_ctx *ctx, uint32_t id);

// ---------------------------------------------------------------------------
// Label
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_label_set(struct mkgui_ctx *ctx, uint32_t id, const char *text);
MKGUI_API char *mkgui_label_get(struct mkgui_ctx *ctx, uint32_t id);

MKGUI_API void mkgui_group_set_collapsed(struct mkgui_ctx *ctx, uint32_t id, uint32_t collapsed);
MKGUI_API uint32_t mkgui_group_get_collapsed(struct mkgui_ctx *ctx, uint32_t id);

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_input_set(struct mkgui_ctx *ctx, uint32_t id, const char *text);
MKGUI_API char *mkgui_input_get(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_input_clear(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_input_set_readonly(struct mkgui_ctx *ctx, uint32_t id, uint32_t readonly);
MKGUI_API uint32_t mkgui_input_get_readonly(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API uint32_t mkgui_input_get_cursor(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_input_set_cursor(struct mkgui_ctx *ctx, uint32_t id, uint32_t pos);
MKGUI_API void mkgui_input_select_all(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_input_get_selection(struct mkgui_ctx *ctx, uint32_t id, uint32_t *start, uint32_t *end);
MKGUI_API void mkgui_input_set_selection(struct mkgui_ctx *ctx, uint32_t id, uint32_t start, uint32_t end);

// ---------------------------------------------------------------------------
// Checkbox
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_checkbox_set(struct mkgui_ctx *ctx, uint32_t id, uint32_t checked);
MKGUI_API uint32_t mkgui_checkbox_get(struct mkgui_ctx *ctx, uint32_t id);

// ---------------------------------------------------------------------------
// Radio
// ---------------------------------------------------------------------------

MKGUI_API uint32_t mkgui_radio_get(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_radio_set(struct mkgui_ctx *ctx, uint32_t id, uint32_t checked);

// ---------------------------------------------------------------------------
// Dropdown
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_dropdown_setup(struct mkgui_ctx *ctx, uint32_t id, char **items, uint32_t count);
MKGUI_API int32_t mkgui_dropdown_get(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_dropdown_set(struct mkgui_ctx *ctx, uint32_t id, int32_t index);
MKGUI_API char *mkgui_dropdown_get_text(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API uint32_t mkgui_dropdown_get_count(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API char *mkgui_dropdown_get_item_text(struct mkgui_ctx *ctx, uint32_t id, uint32_t index);
MKGUI_API void mkgui_dropdown_add(struct mkgui_ctx *ctx, uint32_t id, const char *text);
MKGUI_API void mkgui_dropdown_remove(struct mkgui_ctx *ctx, uint32_t id, uint32_t index);
MKGUI_API void mkgui_dropdown_clear(struct mkgui_ctx *ctx, uint32_t id);

// ---------------------------------------------------------------------------
// ComboBox
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_combobox_setup(struct mkgui_ctx *ctx, uint32_t id, char **items, uint32_t count);
MKGUI_API int32_t mkgui_combobox_get(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API char *mkgui_combobox_get_text(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_combobox_set(struct mkgui_ctx *ctx, uint32_t id, int32_t index);
MKGUI_API void mkgui_combobox_set_text(struct mkgui_ctx *ctx, uint32_t id, const char *text);
MKGUI_API uint32_t mkgui_combobox_get_count(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API char *mkgui_combobox_get_item_text(struct mkgui_ctx *ctx, uint32_t id, uint32_t index);
MKGUI_API void mkgui_combobox_add(struct mkgui_ctx *ctx, uint32_t id, const char *text);
MKGUI_API void mkgui_combobox_remove(struct mkgui_ctx *ctx, uint32_t id, uint32_t index);
MKGUI_API void mkgui_combobox_clear(struct mkgui_ctx *ctx, uint32_t id);

// ---------------------------------------------------------------------------
// Slider
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_slider_setup(struct mkgui_ctx *ctx, uint32_t id, int32_t min_val, int32_t max_val, int32_t value);
MKGUI_API int32_t mkgui_slider_get(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_slider_set(struct mkgui_ctx *ctx, uint32_t id, int32_t value);
MKGUI_API void mkgui_slider_get_range(struct mkgui_ctx *ctx, uint32_t id, int32_t *min_val, int32_t *max_val);
MKGUI_API void mkgui_slider_set_range(struct mkgui_ctx *ctx, uint32_t id, int32_t min_val, int32_t max_val);
MKGUI_API void mkgui_slider_set_meter(struct mkgui_ctx *ctx, uint32_t id, float pre, float post, uint32_t pre_color, uint32_t post_color);

// ---------------------------------------------------------------------------
// Spinbox
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_spinbox_setup(struct mkgui_ctx *ctx, uint32_t id, int32_t min_val, int32_t max_val, int32_t value, int32_t step);
MKGUI_API void mkgui_spinbox_set(struct mkgui_ctx *ctx, uint32_t id, int32_t value);
MKGUI_API int32_t mkgui_spinbox_get(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_spinbox_set_range(struct mkgui_ctx *ctx, uint32_t id, int32_t min_val, int32_t max_val);
MKGUI_API void mkgui_spinbox_get_range(struct mkgui_ctx *ctx, uint32_t id, int32_t *min_val, int32_t *max_val);
MKGUI_API void mkgui_spinbox_set_step(struct mkgui_ctx *ctx, uint32_t id, int32_t step);

// ---------------------------------------------------------------------------
// Scrollbar
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_scrollbar_setup(struct mkgui_ctx *ctx, uint32_t id, int32_t max_value, int32_t page_size);
MKGUI_API void mkgui_scrollbar_set(struct mkgui_ctx *ctx, uint32_t id, int32_t value);
MKGUI_API int32_t mkgui_scrollbar_get(struct mkgui_ctx *ctx, uint32_t id);

// ---------------------------------------------------------------------------
// Progress
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_progress_setup(struct mkgui_ctx *ctx, uint32_t id, int32_t max_val);
MKGUI_API void mkgui_progress_set(struct mkgui_ctx *ctx, uint32_t id, int32_t value);
MKGUI_API int32_t mkgui_progress_get(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_progress_set_range(struct mkgui_ctx *ctx, uint32_t id, int32_t max_val);
MKGUI_API void mkgui_progress_get_range(struct mkgui_ctx *ctx, uint32_t id, int32_t *max_val);
MKGUI_API void mkgui_progress_set_color(struct mkgui_ctx *ctx, uint32_t id, uint32_t color);

// ---------------------------------------------------------------------------
// Meter
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_meter_setup(struct mkgui_ctx *ctx, uint32_t id, int32_t max_val);
MKGUI_API void mkgui_meter_set(struct mkgui_ctx *ctx, uint32_t id, int32_t value);
MKGUI_API int32_t mkgui_meter_get(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_meter_set_range(struct mkgui_ctx *ctx, uint32_t id, int32_t max_val);
MKGUI_API void mkgui_meter_get_range(struct mkgui_ctx *ctx, uint32_t id, int32_t *max_val);
MKGUI_API void mkgui_meter_set_zones(struct mkgui_ctx *ctx, uint32_t id, int32_t t1, int32_t t2, uint32_t c1, uint32_t c2, uint32_t c3);

// ---------------------------------------------------------------------------
// Toggle
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_toggle_set(struct mkgui_ctx *ctx, uint32_t id, uint32_t state);
MKGUI_API uint32_t mkgui_toggle_get(struct mkgui_ctx *ctx, uint32_t id);

// ---------------------------------------------------------------------------
// Textarea
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_textarea_set(struct mkgui_ctx *ctx, uint32_t id, const char *text);
MKGUI_API char *mkgui_textarea_get(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_textarea_set_readonly(struct mkgui_ctx *ctx, uint32_t id, uint32_t readonly);
MKGUI_API uint32_t mkgui_textarea_get_readonly(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_textarea_get_cursor(struct mkgui_ctx *ctx, uint32_t id, uint32_t *line, uint32_t *col);
MKGUI_API void mkgui_textarea_set_cursor(struct mkgui_ctx *ctx, uint32_t id, uint32_t line, uint32_t col);
MKGUI_API uint32_t mkgui_textarea_get_line_count(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_textarea_get_selection(struct mkgui_ctx *ctx, uint32_t id, uint32_t *start, uint32_t *end);
MKGUI_API void mkgui_textarea_insert(struct mkgui_ctx *ctx, uint32_t id, const char *text);
MKGUI_API void mkgui_textarea_append(struct mkgui_ctx *ctx, uint32_t id, const char *text);
MKGUI_API void mkgui_textarea_scroll_to_end(struct mkgui_ctx *ctx, uint32_t id);

// ---------------------------------------------------------------------------
// DatePicker
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_datepicker_set(struct mkgui_ctx *ctx, uint32_t id, int32_t year, int32_t month, int32_t day);
MKGUI_API void mkgui_datepicker_get(struct mkgui_ctx *ctx, uint32_t id, int32_t *year, int32_t *month, int32_t *day);
MKGUI_API char *mkgui_datepicker_get_text(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_datepicker_set_readonly(struct mkgui_ctx *ctx, uint32_t id, uint32_t readonly);
MKGUI_API uint32_t mkgui_datepicker_get_readonly(struct mkgui_ctx *ctx, uint32_t id);

// ---------------------------------------------------------------------------
// IP Input
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_ipinput_set(struct mkgui_ctx *ctx, uint32_t id, const char *ip_string);
MKGUI_API char *mkgui_ipinput_get(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API uint32_t mkgui_ipinput_get_u32(struct mkgui_ctx *ctx, uint32_t id);

// ---------------------------------------------------------------------------
// Tabs
// ---------------------------------------------------------------------------

MKGUI_API uint32_t mkgui_tabs_get_current(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_tabs_set_current(struct mkgui_ctx *ctx, uint32_t id, uint32_t tab_id);
MKGUI_API uint32_t mkgui_tabs_get_count(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_tabs_set_text(struct mkgui_ctx *ctx, uint32_t tabs_id, uint32_t tab_id, const char *text);
MKGUI_API char *mkgui_tabs_get_text(struct mkgui_ctx *ctx, uint32_t tabs_id, uint32_t tab_id);

// ---------------------------------------------------------------------------
// Split
// ---------------------------------------------------------------------------

MKGUI_API float mkgui_split_get_ratio(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_split_set_ratio(struct mkgui_ctx *ctx, uint32_t id, float ratio);

// ---------------------------------------------------------------------------
// ListView
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_listview_setup(struct mkgui_ctx *ctx, uint32_t id, uint32_t row_count, uint32_t col_count, struct mkgui_column *columns, mkgui_row_cb cb, void *userdata);
MKGUI_API void mkgui_listview_set_rows(struct mkgui_ctx *ctx, uint32_t id, uint32_t row_count);
MKGUI_API int32_t mkgui_listview_get_selected(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_listview_set_selected(struct mkgui_ctx *ctx, uint32_t id, int32_t row);
MKGUI_API uint32_t mkgui_listview_get_multi_sel(struct mkgui_ctx *ctx, uint32_t id, int32_t **out);
MKGUI_API uint32_t mkgui_listview_is_selected(struct mkgui_ctx *ctx, uint32_t id, int32_t row);
MKGUI_API void mkgui_listview_clear_selection(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API uint32_t *mkgui_listview_get_col_order(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_listview_set_col_order(struct mkgui_ctx *ctx, uint32_t id, uint32_t *order, uint32_t count);
MKGUI_API int32_t mkgui_listview_get_col_width(struct mkgui_ctx *ctx, uint32_t id, uint32_t col);
MKGUI_API void mkgui_listview_set_col_width(struct mkgui_ctx *ctx, uint32_t id, uint32_t col, int32_t width);
MKGUI_API void mkgui_listview_set_cell_type(struct mkgui_ctx *ctx, uint32_t id, uint32_t col, uint32_t cell_type);
MKGUI_API void mkgui_listview_visible_range(struct mkgui_ctx *ctx, uint32_t id, int32_t *first, int32_t *last);
MKGUI_API void mkgui_listview_scroll_to(struct mkgui_ctx *ctx, uint32_t id, int32_t row);
MKGUI_API void mkgui_listview_get_sort(struct mkgui_ctx *ctx, uint32_t id, int32_t *col, int32_t *dir);
MKGUI_API void mkgui_listview_set_sort(struct mkgui_ctx *ctx, uint32_t id, int32_t col, int32_t dir);

// ---------------------------------------------------------------------------
// GridView
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_gridview_setup(struct mkgui_ctx *ctx, uint32_t id, uint32_t row_count, uint32_t col_count, struct mkgui_grid_column *columns, mkgui_grid_cell_cb cb, void *userdata);
MKGUI_API void mkgui_gridview_set_rows(struct mkgui_ctx *ctx, uint32_t id, uint32_t row_count);
MKGUI_API int32_t mkgui_gridview_get_selected(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_gridview_set_selected(struct mkgui_ctx *ctx, uint32_t id, int32_t row);
MKGUI_API uint32_t mkgui_gridview_get_check(struct mkgui_ctx *ctx, uint32_t id, uint32_t row, uint32_t col);
MKGUI_API void mkgui_gridview_set_check(struct mkgui_ctx *ctx, uint32_t id, uint32_t row, uint32_t col, uint32_t checked);
MKGUI_API int32_t mkgui_gridview_get_col_width(struct mkgui_ctx *ctx, uint32_t id, uint32_t col);
MKGUI_API void mkgui_gridview_set_col_width(struct mkgui_ctx *ctx, uint32_t id, uint32_t col, int32_t width);
MKGUI_API void mkgui_gridview_scroll_to(struct mkgui_ctx *ctx, uint32_t id, int32_t row);

// ---------------------------------------------------------------------------
// RichList
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_richlist_setup(struct mkgui_ctx *ctx, uint32_t id, uint32_t row_count, int32_t row_height, mkgui_richlist_cb cb, void *userdata);
MKGUI_API void mkgui_richlist_set_rows(struct mkgui_ctx *ctx, uint32_t id, uint32_t row_count);
MKGUI_API int32_t mkgui_richlist_get_selected(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_richlist_set_selected(struct mkgui_ctx *ctx, uint32_t id, int32_t row);
MKGUI_API void mkgui_richlist_scroll_to(struct mkgui_ctx *ctx, uint32_t id, int32_t row);

// ---------------------------------------------------------------------------
// TreeView
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_treeview_setup(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API uint32_t mkgui_treeview_add(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id, uint32_t parent_node, const char *label);
MKGUI_API void mkgui_treeview_select(struct mkgui_ctx *ctx, uint32_t widget_id, int32_t node_id);
MKGUI_API int32_t mkgui_treeview_get_selected(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_treeview_remove(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id);
MKGUI_API void mkgui_treeview_clear(struct mkgui_ctx *ctx, uint32_t widget_id);
MKGUI_API void mkgui_treeview_set_label(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id, const char *label);
MKGUI_API char *mkgui_treeview_get_label(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id);
MKGUI_API void mkgui_treeview_expand(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id);
MKGUI_API void mkgui_treeview_collapse(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id);
MKGUI_API uint32_t mkgui_treeview_is_expanded(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id);
MKGUI_API void mkgui_treeview_expand_all(struct mkgui_ctx *ctx, uint32_t widget_id);
MKGUI_API void mkgui_treeview_collapse_all(struct mkgui_ctx *ctx, uint32_t widget_id);
MKGUI_API uint32_t mkgui_treeview_get_parent(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id);
MKGUI_API uint32_t mkgui_treeview_node_count(struct mkgui_ctx *ctx, uint32_t widget_id);
MKGUI_API void mkgui_treeview_scroll_to(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id);

// ---------------------------------------------------------------------------
// ItemView
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_itemview_setup(struct mkgui_ctx *ctx, uint32_t id, uint32_t item_count, uint32_t view_mode, mkgui_itemview_label_cb label_cb, mkgui_itemview_icon_cb icon_cb, void *userdata);
MKGUI_API void mkgui_itemview_set_items(struct mkgui_ctx *ctx, uint32_t id, uint32_t count);
MKGUI_API void mkgui_itemview_set_view(struct mkgui_ctx *ctx, uint32_t id, uint32_t view_mode);
MKGUI_API uint32_t mkgui_itemview_get_view(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_itemview_set_cell_size(struct mkgui_ctx *ctx, uint32_t id, int32_t w, int32_t h);
MKGUI_API int32_t mkgui_itemview_get_selected(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_itemview_set_selected(struct mkgui_ctx *ctx, uint32_t id, int32_t item);
MKGUI_API void mkgui_itemview_clear_selection(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_itemview_scroll_to(struct mkgui_ctx *ctx, uint32_t id, int32_t item);
MKGUI_API void mkgui_itemview_set_thumbnail(struct mkgui_ctx *ctx, uint32_t id, mkgui_thumbnail_cb cb, int32_t thumb_size);

// ---------------------------------------------------------------------------
// Pathbar
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_pathbar_set(struct mkgui_ctx *ctx, uint32_t id, const char *path);
MKGUI_API char *mkgui_pathbar_get(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_pathbar_get_segment_path(struct mkgui_ctx *ctx, uint32_t id, uint32_t seg_idx, char *out, uint32_t out_size);

// ---------------------------------------------------------------------------
// Statusbar
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_statusbar_setup(struct mkgui_ctx *ctx, uint32_t id, uint32_t section_count, int32_t *widths);
MKGUI_API void mkgui_statusbar_set(struct mkgui_ctx *ctx, uint32_t id, uint32_t section, const char *text);
MKGUI_API char *mkgui_statusbar_get(struct mkgui_ctx *ctx, uint32_t id, uint32_t section);
MKGUI_API void mkgui_statusbar_clear(struct mkgui_ctx *ctx, uint32_t id, uint32_t section);

// ---------------------------------------------------------------------------
// Canvas
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_canvas_set_callback(struct mkgui_ctx *ctx, uint32_t id, mkgui_canvas_cb callback, void *userdata);

// ---------------------------------------------------------------------------
// Image
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_image_set(struct mkgui_ctx *ctx, uint32_t id, uint32_t *pixels, int32_t w, int32_t h);
MKGUI_API void mkgui_image_clear(struct mkgui_ctx *ctx, uint32_t id);

// ---------------------------------------------------------------------------
// GLView
// ---------------------------------------------------------------------------

MKGUI_API uint32_t mkgui_glview_init(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_glview_destroy(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_glview_get_size(struct mkgui_ctx *ctx, uint32_t id, int32_t *w, int32_t *h);

// ---------------------------------------------------------------------------
// Context menu
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_context_menu_clear(struct mkgui_ctx *ctx);
MKGUI_API void mkgui_context_menu_add(struct mkgui_ctx *ctx, uint32_t id, const char *label, const char *icon, uint32_t flags);
MKGUI_API void mkgui_context_menu_add_separator(struct mkgui_ctx *ctx);
MKGUI_API void mkgui_context_menu_show(struct mkgui_ctx *ctx);
MKGUI_API void mkgui_context_menu_show_at(struct mkgui_ctx *ctx, int32_t x, int32_t y);

// ---------------------------------------------------------------------------
// Dialogs
// ---------------------------------------------------------------------------

MKGUI_API uint32_t mkgui_color_dialog(struct mkgui_ctx *ctx, uint32_t initial_color, uint32_t *out_color);
MKGUI_API uint32_t mkgui_message_box(struct mkgui_ctx *ctx, const char *title, const char *message, uint32_t icon_type, uint32_t buttons);
MKGUI_API uint32_t mkgui_confirm_dialog(struct mkgui_ctx *ctx, const char *title, const char *message);
MKGUI_API uint32_t mkgui_input_dialog(struct mkgui_ctx *ctx, const char *title, const char *prompt, const char *default_text, char *out, uint32_t out_size);
MKGUI_API uint32_t mkgui_open_dialog(struct mkgui_ctx *ctx, struct mkgui_file_dialog_opts *opts);
MKGUI_API uint32_t mkgui_save_dialog(struct mkgui_ctx *ctx, struct mkgui_file_dialog_opts *opts);
MKGUI_API char *mkgui_dialog_path(struct mkgui_ctx *ctx, uint32_t index);

// ---------------------------------------------------------------------------
// Accelerators
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_accel_add(struct mkgui_ctx *ctx, uint32_t id, uint32_t keymod, uint32_t keysym);
MKGUI_API void mkgui_accel_remove(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API void mkgui_accel_clear(struct mkgui_ctx *ctx);

// ---------------------------------------------------------------------------
// File drag-and-drop
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_drop_enable(struct mkgui_ctx *ctx);
MKGUI_API uint32_t mkgui_drop_count(struct mkgui_ctx *ctx);
MKGUI_API char *mkgui_drop_file(struct mkgui_ctx *ctx, uint32_t index);

// ---------------------------------------------------------------------------
// Cell editing (inline rename in treeview/listview/gridview)
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_cell_edit_begin(struct mkgui_ctx *ctx, uint32_t widget_id, int32_t row, int32_t col, char *text);
MKGUI_API void mkgui_cell_edit_cancel(struct mkgui_ctx *ctx);
MKGUI_API char *mkgui_cell_edit_get_text(struct mkgui_ctx *ctx);
MKGUI_API uint32_t mkgui_cell_edit_active(struct mkgui_ctx *ctx);

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------

MKGUI_API void mkgui_sleep_ms(uint32_t ms);
MKGUI_API uint32_t mkgui_time_ms(void);
MKGUI_API double mkgui_time_us(void);

#endif // MKGUI_H
