// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define MKGUI_MAX_WIDGETS    1024
#define MKGUI_MAX_COLS       32
#define MKGUI_MAX_TEXT       256
#define MKGUI_MAX_DROPDOWN   64
#define MKGUI_MAX_POPUPS     8
#define MKGUI_ROW_HEIGHT     20
#define MKGUI_TAB_HEIGHT     26
#define MKGUI_MENU_HEIGHT    22
#define MKGUI_SCROLLBAR_W    14
#define MKGUI_SPLIT_THICK    5
#define MKGUI_BOX_GAP        6
#define MKGUI_BOX_PAD        6
#define MKGUI_MARGIN         3
#define MKGUI_GLYPH_FIRST    32
#define MKGUI_GLYPH_LAST     126
#define MKGUI_GLYPH_COUNT    (MKGUI_GLYPH_LAST - MKGUI_GLYPH_FIRST + 1)
#define MKGUI_GLYPH_MAX_BMP  1024

#define MKGUI_TOOLBAR_HEIGHT  28
#define MKGUI_STATUSBAR_HEIGHT 22

#define MKGUI_ICON_SIZE       18
#define MKGUI_ICON_PIXELS     (MKGUI_ICON_SIZE * MKGUI_ICON_SIZE)
#define MKGUI_MAX_ICONS       2048
#define MKGUI_ICON_NAME_LEN   64
#define MKGUI_ICON_PIXEL_POOL (MKGUI_ICON_PIXELS * MKGUI_MAX_ICONS)

// ---------------------------------------------------------------------------
// Platform-neutral key constants (values match X11 keysyms for convenience)
// ---------------------------------------------------------------------------

#define MKGUI_KEY_BACKSPACE  0xff08
#define MKGUI_KEY_TAB        0xff09
#define MKGUI_KEY_ISO_LEFT_TAB 0xfe20
#define MKGUI_KEY_RETURN     0xff0d
#define MKGUI_KEY_ESCAPE     0xff1b
#define MKGUI_KEY_SPACE      0x0020
#define MKGUI_KEY_DELETE     0xffff
#define MKGUI_KEY_HOME       0xff50
#define MKGUI_KEY_LEFT       0xff51
#define MKGUI_KEY_UP         0xff52
#define MKGUI_KEY_RIGHT      0xff53
#define MKGUI_KEY_DOWN       0xff54
#define MKGUI_KEY_PAGE_UP    0xff55
#define MKGUI_KEY_PAGE_DOWN  0xff56
#define MKGUI_KEY_END        0xff57
#define MKGUI_KEY_F1         0xffbe

// ---------------------------------------------------------------------------
// Platform-neutral modifier masks
// ---------------------------------------------------------------------------

#define MKGUI_MOD_SHIFT      (1 << 0)
#define MKGUI_MOD_CONTROL    (1 << 2)

// ---------------------------------------------------------------------------
// Platform event types
// ---------------------------------------------------------------------------

enum {
	MKGUI_PLAT_NONE,
	MKGUI_PLAT_EXPOSE,
	MKGUI_PLAT_RESIZE,
	MKGUI_PLAT_MOTION,
	MKGUI_PLAT_BUTTON_PRESS,
	MKGUI_PLAT_BUTTON_RELEASE,
	MKGUI_PLAT_KEY,
	MKGUI_PLAT_CLOSE,
	MKGUI_PLAT_LEAVE,
};

struct mkgui_plat_event {
	uint32_t type;
	int32_t x, y;
	int32_t width, height;
	uint32_t button;
	uint32_t keysym;
	uint32_t keymod;
	char text[32];
	int32_t text_len;
	int32_t popup_idx;
};

// ---------------------------------------------------------------------------
// Platform types
// ---------------------------------------------------------------------------

#ifdef _WIN32
#include "platform_win32_types.h"
#else
#include "platform_linux_types.h"
#endif

// [=]===^=[ mkgui_sleep_ms ]======================================[=]
static void mkgui_sleep_ms(uint32_t ms) {
#ifdef _WIN32
	Sleep(ms);
#else
	struct timespec ts = { 0, (long)ms * 1000000L };
	nanosleep(&ts, NULL);
#endif
}

// [=]===^=[ mkgui_time_ms ]=======================================[=]
static uint32_t mkgui_time_ms(void) {
#ifdef _WIN32
	return (uint32_t)GetTickCount();
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#endif
}

// [=]===^=[ mkgui_time_us ]=======================================[=]
static double mkgui_time_us(void) {
#ifdef _WIN32
	LARGE_INTEGER freq, now;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&now);
	return (double)now.QuadPart / (double)freq.QuadPart * 1e6;
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec * 1e-3;
#endif
}

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
};

// ---------------------------------------------------------------------------
// Anchor / layout flags
// ---------------------------------------------------------------------------

enum {
	MKGUI_ANCHOR_LEFT   = (1 << 0),
	MKGUI_ANCHOR_TOP    = (1 << 1),
	MKGUI_ANCHOR_RIGHT  = (1 << 2),
	MKGUI_ANCHOR_BOTTOM = (1 << 3),
	MKGUI_REGION_TOP    = (1 << 4),
	MKGUI_REGION_BOTTOM = (1 << 5),
	MKGUI_REGION_LEFT   = (1 << 6),
	MKGUI_REGION_RIGHT  = (1 << 7),
	MKGUI_HIDDEN        = (1 << 8),
	MKGUI_DISABLED      = (1 << 9),
	MKGUI_CHECKED       = (1 << 10),
	MKGUI_PASSWORD      = (1 << 11),
	MKGUI_READONLY      = (1 << 12),
	MKGUI_SEPARATOR     = (1 << 13),
	MKGUI_TOOLBAR_SEP   = (1 << 14),
	MKGUI_MENU_CHECK    = (1 << 15),
	MKGUI_MENU_RADIO    = (1 << 16),
	MKGUI_PANEL_BORDER  = (1 << 17),
	MKGUI_PANEL_SUNKEN  = (1 << 18),
	MKGUI_SCROLLBAR_HORIZ = (1 << 19),
	MKGUI_IMAGE_STRETCH = (1 << 20),
	MKGUI_SCROLL        = (1 << 21),
	MKGUI_NO_PAD        = (1 << 22),
	MKGUI_TAB_CLOSABLE  = (1 << 23),
	MKGUI_MULTI_SELECT  = (1 << 24),
};

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
	MKGUI_EVENT_BUTTON_DBLCLICK,
	MKGUI_EVENT_TAB_CLOSE,
};

// ---------------------------------------------------------------------------
// Itemview modes
// ---------------------------------------------------------------------------

enum {
	MKGUI_VIEW_ICON,
	MKGUI_VIEW_THUMBNAIL,
	MKGUI_VIEW_COMPACT,
	MKGUI_VIEW_DETAIL,
};

// ---------------------------------------------------------------------------
// Structs
// ---------------------------------------------------------------------------

struct mkgui_widget {
	uint32_t type;
	uint32_t id;
	char label[MKGUI_MAX_TEXT];
	char icon[MKGUI_ICON_NAME_LEN];
	uint32_t parent_id;
	int32_t x, y, w, h;
	uint32_t flags;
};

struct mkgui_ctx;
typedef void (*mkgui_render_cb)(struct mkgui_ctx *ctx, void *userdata);

struct mkgui_event {
	uint32_t type;
	uint32_t id;
	int32_t value;
	int32_t col;
	uint32_t keysym;
	uint32_t keymod;
};

enum {
	MKGUI_CELL_TEXT,
	MKGUI_CELL_PROGRESS,
	MKGUI_CELL_ICON_TEXT,
	MKGUI_CELL_SIZE,
	MKGUI_CELL_DATE,
	MKGUI_CELL_CHECKBOX,
};

struct mkgui_column {
	char label[MKGUI_MAX_TEXT];
	int32_t width;
	uint32_t cell_type;
};

typedef void (*mkgui_row_cb)(uint32_t row, uint32_t col, char *buf, uint32_t buf_size, void *userdata);

#define MKGUI_MAX_MULTI_SEL 4096

struct mkgui_listview_data {
	uint32_t widget_id;
	uint32_t row_count;
	uint32_t col_count;
	struct mkgui_column columns[MKGUI_MAX_COLS];
	mkgui_row_cb row_cb;
	void *userdata;
	int32_t scroll_y;
	int32_t selected_row;
	int32_t sort_col;
	int32_t sort_dir;
	int32_t header_height;
	uint32_t col_order[MKGUI_MAX_COLS];
	int32_t drag_source;
	int32_t drag_target;
	int32_t drag_start_y;
	uint32_t drag_active;
	int32_t multi_sel[MKGUI_MAX_MULTI_SEL];
	uint32_t multi_sel_count;
};

struct mkgui_input_data {
	uint32_t widget_id;
	char text[MKGUI_MAX_TEXT];
	uint32_t cursor;
	uint32_t sel_start;
	uint32_t sel_end;
};

struct mkgui_dropdown_data {
	uint32_t widget_id;
	char items[MKGUI_MAX_DROPDOWN][MKGUI_MAX_TEXT];
	uint32_t item_count;
	int32_t selected;
	int32_t scroll_y;
};

struct mkgui_slider_data {
	uint32_t widget_id;
	int32_t min_val;
	int32_t max_val;
	int32_t value;
};

struct mkgui_tabs_data {
	uint32_t widget_id;
	uint32_t active_tab;
	uint32_t hover_tab;
};

struct mkgui_split_data {
	uint32_t widget_id;
	float ratio;
};

#define MKGUI_MAX_STATUSBAR_SECTIONS 8
#define MKGUI_MAX_TEXTAREA_LINES 4096
#define MKGUI_MAX_TEXTAREA_LINE  1024
#define MKGUI_CLIP_MAX           4096

struct mkgui_treeview_node {
	uint32_t id;
	uint32_t parent_node;
	char label[MKGUI_MAX_TEXT];
	uint32_t expanded;
	uint32_t has_children;
	int32_t icon_idx;
};

struct mkgui_treeview_data {
	uint32_t widget_id;
	struct mkgui_treeview_node *nodes;
	uint32_t node_count;
	uint32_t node_cap;
	int32_t selected_node;
	int32_t scroll_y;
	int32_t drag_source;
	int32_t drag_target;
	int32_t drag_pos;
	uint32_t drag_active;
	int32_t drag_start_y;
};

struct mkgui_statusbar_section {
	char text[MKGUI_MAX_TEXT];
	int32_t width;
};

struct mkgui_statusbar_data {
	uint32_t widget_id;
	struct mkgui_statusbar_section sections[MKGUI_MAX_STATUSBAR_SECTIONS];
	uint32_t section_count;
};

struct mkgui_spinbox_data {
	uint32_t widget_id;
	int32_t min_val;
	int32_t max_val;
	int32_t value;
	int32_t step;
	int32_t hover_btn;
	uint32_t editing;
	uint32_t select_all;
	char edit_buf[32];
	uint32_t edit_len;
	int32_t repeat_dir;
	uint32_t repeat_next_ms;
};

struct mkgui_progress_data {
	uint32_t widget_id;
	int32_t value;
	int32_t max_val;
};

struct mkgui_textarea_data {
	uint32_t widget_id;
	char *text;
	uint32_t text_len;
	uint32_t text_cap;
	uint32_t cursor;
	uint32_t sel_start;
	uint32_t sel_end;
	int32_t scroll_y;
	int32_t scroll_x;
};


typedef void (*mkgui_itemview_label_cb)(uint32_t item, char *buf, uint32_t buf_size, void *userdata);
typedef void (*mkgui_itemview_icon_cb)(uint32_t item, char *buf, uint32_t buf_size, void *userdata);
typedef void (*mkgui_thumbnail_cb)(uint32_t item, uint32_t *pixels, int32_t w, int32_t h, void *userdata);

struct mkgui_itemview_data {
	uint32_t widget_id;
	uint32_t item_count;
	uint32_t view_mode;
	int32_t selected;
	int32_t scroll_y;
	int32_t cell_w;
	int32_t cell_h;
	mkgui_itemview_label_cb label_cb;
	mkgui_itemview_icon_cb icon_cb;
	mkgui_thumbnail_cb thumbnail_cb;
	void *userdata;
	uint32_t *thumb_buf;
	int32_t thumb_size;
};

struct mkgui_scrollbar_data {
	uint32_t id;
	int32_t value;
	int32_t max_value;
	int32_t page_size;
};

struct mkgui_box_scroll {
	uint32_t widget_id;
	int32_t scroll_y;
	int32_t content_h;
	int32_t scroll_x;
	int32_t content_w;
};

struct mkgui_image_data {
	uint32_t id;
	uint32_t *pixels;
	int32_t img_w, img_h;
};

struct mkgui_glview_data {
	uint32_t id;
	uint32_t created;
	uint32_t visible;
	int32_t last_x, last_y, last_w, last_h;
	struct mkgui_glview_platform plat;
};

typedef void (*mkgui_canvas_cb)(struct mkgui_ctx *ctx, uint32_t id, uint32_t *pixels, int32_t x, int32_t y, int32_t w, int32_t h, void *userdata);

struct mkgui_canvas_data {
	uint32_t widget_id;
	mkgui_canvas_cb callback;
	void *userdata;
};

struct mkgui_glyph {
	int32_t width;
	int32_t height;
	int32_t bearing_x;
	int32_t bearing_y;
	int32_t advance;
	uint8_t bitmap[MKGUI_GLYPH_MAX_BMP];
};

struct mkgui_popup {
	struct mkgui_popup_platform plat;
	uint32_t *pixels;
	int32_t x, y, w, h;
	uint32_t widget_id;
	uint32_t active;
	uint32_t dirty;
	int32_t hover_item;
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
	uint32_t menu_hover;
	uint32_t scrollbar_bg;
	uint32_t scrollbar_thumb;
	uint32_t scrollbar_thumb_hover;
	uint32_t splitter;
	uint32_t header_bg;
	uint32_t listview_alt;
	uint32_t accent;
	int32_t corner_radius;
};

// ---------------------------------------------------------------------------
// Icon system
// ---------------------------------------------------------------------------

struct mkgui_icon {
	char name[MKGUI_ICON_NAME_LEN];
	uint32_t *pixels;
	int32_t w, h;
	uint32_t custom;
};

static uint32_t icon_pixels[MKGUI_ICON_PIXEL_POOL];
static uint32_t icon_pixels_used;
static struct mkgui_icon icons[MKGUI_MAX_ICONS];
static uint32_t icon_count;

static uint32_t icon_text_color;

static uint8_t *mdi_dat;
static uint32_t mdi_dat_size;
static uint16_t mdi_icon_size;
static uint16_t mdi_icon_count;
static const char *mdi_name_block;
static const uint32_t *mdi_name_offsets;
static const uint8_t *mdi_pixel_data;

struct mkgui_ctx {
	struct mkgui_platform plat;

	uint32_t *pixels;
	int32_t win_w, win_h;

	struct mkgui_widget widgets[MKGUI_MAX_WIDGETS];
	uint32_t widget_count;

	struct { int32_t x, y, w, h; } rects[MKGUI_MAX_WIDGETS];

	struct mkgui_listview_data listvs[32];
	uint32_t listv_count;
	struct mkgui_input_data inputs[64];
	uint32_t input_count;
	struct mkgui_dropdown_data dropdowns[32];
	uint32_t dropdown_count;
	struct mkgui_slider_data sliders[32];
	uint32_t slider_count;
	struct mkgui_tabs_data tabs[32];
	uint32_t tab_count;
	struct mkgui_split_data splits[32];
	uint32_t split_count;
	struct mkgui_treeview_data treeviews[8];
	uint32_t treeview_count;
	struct mkgui_statusbar_data statusbars[8];
	uint32_t statusbar_count;
	struct mkgui_spinbox_data spinboxes[32];
	uint32_t spinbox_count;
	struct mkgui_progress_data progress[32];
	uint32_t progress_count;
	struct mkgui_textarea_data textareas[16];
	uint32_t textarea_count;
	struct mkgui_itemview_data itemviews[16];
	uint32_t itemview_count;
	struct mkgui_scrollbar_data scrollbars[32];
	uint32_t scrollbar_count;
	struct mkgui_box_scroll box_scrolls[32];
	uint32_t box_scroll_count;
	struct mkgui_image_data images[32];
	uint32_t image_count;
	struct mkgui_glview_data glviews[8];
	uint32_t glview_count;
	struct mkgui_canvas_data canvases[16];
	uint32_t canvas_count;

	struct mkgui_popup popups[MKGUI_MAX_POPUPS];
	uint32_t popup_count;

	uint32_t hover_id;
	uint32_t prev_hover_id;
	uint32_t press_id;
	uint32_t press_mod;
	uint32_t focus_id;
	uint32_t prev_focus_id;
	uint32_t drag_scrollbar_id;
	int32_t drag_scrollbar_offset;
	uint32_t drag_col_id;
	int32_t drag_col_src;
	int32_t drag_col_start_x;
	uint32_t drag_col_resize_id;
	int32_t drag_col_resize_col;
	int32_t drag_col_resize_start_x;
	int32_t drag_col_resize_start_w;
	uint32_t drag_select_id;
	int32_t mouse_x, mouse_y;
	uint32_t mouse_btn;
	uint32_t dirty;

	uint32_t dblclick_id;
	int32_t dblclick_row;
	uint32_t dblclick_time;

	struct { int32_t x, y, w, h; } dirty_rects[32];
	uint32_t dirty_count;
	uint32_t dirty_full;

	uint32_t tooltip_id;
	uint32_t tooltip_timer;
	int32_t tooltip_x, tooltip_y;

	char tooltip_texts[MKGUI_MAX_WIDGETS][128];

	struct mkgui_theme theme;

	double anim_time;
#ifdef _WIN32
	int64_t anim_prev;
	int64_t perf_freq;
#else
	struct timespec anim_prev;
#endif
	uint32_t anim_active;
	uint32_t close_requested;
	int32_t poll_timeout_ms;

	double perf_layout_us;
	double perf_render_us;
	double perf_blit_us;

	struct mkgui_glyph glyphs[MKGUI_GLYPH_COUNT];
	int32_t font_ascent;
	int32_t font_height;
	int32_t char_width;

	mkgui_render_cb render_cb;
	void *render_cb_data;

	struct mkgui_ctx *parent;

	char clip_text[MKGUI_CLIP_MAX];
	uint32_t clip_len;
};

// ---------------------------------------------------------------------------
// Default theme
// ---------------------------------------------------------------------------

// [=]===^=[ default_theme ]=====================================[=]
static struct mkgui_theme default_theme(void) {
	struct mkgui_theme t;
	t.bg                  = 0xff232629;
	t.widget_bg           = 0xff31363b;
	t.widget_border       = 0xff5e5e5e;
	t.widget_hover        = 0xff3d4349;
	t.widget_press        = 0xff272b2f;
	t.text                = 0xffeff0f1;
	t.text_disabled       = 0xff6e7175;
	t.selection           = 0xff2980b9;
	t.sel_text            = 0xffffffff;
	t.input_bg            = 0xff1a1e21;
	t.tab_active          = 0xff31363b;
	t.tab_inactive        = 0xff2a2e32;
	t.tab_hover           = 0xff353a3f;
	t.menu_bg             = 0xff31363b;
	t.menu_hover          = 0xff3daee9;
	t.scrollbar_bg        = 0xff1d2023;
	t.scrollbar_thumb     = 0xff4d4d4d;
	t.scrollbar_thumb_hover = 0xff5a5a5a;
	t.splitter            = 0xff3daee9;
	t.header_bg           = 0xff2a2e32;
	t.listview_alt        = 0xff2a2e32;
	t.accent              = 0xff2a7ab5;
	t.corner_radius       = 3;
	return t;
}

// [=]===^=[ light_theme ]========================================[=]
static struct mkgui_theme light_theme(void) {
	struct mkgui_theme t;
	t.bg                    = 0xfff0f0f0;
	t.widget_bg             = 0xffffffff;
	t.widget_border         = 0xffbcbcbc;
	t.widget_hover          = 0xffe8e8e8;
	t.widget_press          = 0xffd0d0d0;
	t.text                  = 0xff1a1a1a;
	t.text_disabled         = 0xff999999;
	t.selection             = 0xff2980b9;
	t.sel_text              = 0xffffffff;
	t.input_bg              = 0xffffffff;
	t.tab_active            = 0xffffffff;
	t.tab_inactive          = 0xffe0e0e0;
	t.tab_hover             = 0xffebebeb;
	t.menu_bg               = 0xfffafafa;
	t.menu_hover            = 0xff3daee9;
	t.scrollbar_bg          = 0xffe0e0e0;
	t.scrollbar_thumb       = 0xffb0b0b0;
	t.scrollbar_thumb_hover = 0xff909090;
	t.splitter              = 0xff3daee9;
	t.header_bg             = 0xffe8e8e8;
	t.listview_alt          = 0xfff5f5f5;
	t.accent                = 0xff2a7ab5;
	t.corner_radius         = 3;
	return t;
}

// ---------------------------------------------------------------------------
// Drawing primitives
// ---------------------------------------------------------------------------

// [=]===^=[ blend_pixel ]========================================[=]
static inline uint32_t blend_pixel(uint32_t dst, uint32_t color, uint8_t alpha) {
	if(alpha == 255) {
		return color;
	}
	uint32_t a = alpha;
	uint32_t ia = 255 - a;
	uint32_t rb_src = color & 0x00ff00ff;
	uint32_t g_src = color & 0x0000ff00;
	uint32_t rb_dst = dst & 0x00ff00ff;
	uint32_t g_dst = dst & 0x0000ff00;
	uint32_t rb = (rb_src * a + rb_dst * ia + 0x00800080);
	rb = ((rb + ((rb >> 8) & 0x00ff00ff)) >> 8) & 0x00ff00ff;
	uint32_t g = (g_src * a + g_dst * ia + 0x00008000);
	g = ((g + ((g >> 8) & 0x0000ff00)) >> 8) & 0x0000ff00;
	return 0xff000000 | rb | g;
}

static int32_t render_clip_x1, render_clip_y1, render_clip_x2, render_clip_y2;
static int32_t render_base_clip_x1, render_base_clip_y1, render_base_clip_x2, render_base_clip_y2;

// [=]===^=[ draw_pixel ]========================================[=]
static void draw_pixel(uint32_t *buf, int32_t bw, int32_t bh, int32_t x, int32_t y, uint32_t color) {
	if(x >= 0 && x < bw && y >= 0 && y < bh && x >= render_clip_x1 && x < render_clip_x2 && y >= render_clip_y1 && y < render_clip_y2) {
		buf[y * bw + x] = color;
	}
}

// [=]===^=[ draw_rect_fill ]====================================[=]
static void draw_rect_fill(uint32_t *buf, int32_t bw, int32_t bh, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
	int32_t x0 = x < 0 ? 0 : x;
	int32_t y0 = y < 0 ? 0 : y;
	int32_t x1 = (x + w) > bw ? bw : (x + w);
	int32_t y1 = (y + h) > bh ? bh : (y + h);
	if(x0 < render_clip_x1) { x0 = render_clip_x1; }
	if(y0 < render_clip_y1) { y0 = render_clip_y1; }
	if(x1 > render_clip_x2) { x1 = render_clip_x2; }
	if(y1 > render_clip_y2) { y1 = render_clip_y2; }
	int32_t span = x1 - x0;
	if(span <= 0) {
		return;
	}
	for(int32_t row = y0; row < y1; ++row) {
		uint32_t *dst = &buf[row * bw + x0];
		for(int32_t i = 0; i < span; ++i) {
			dst[i] = color;
		}
	}
}

#define MKGUI_MAX_CORNER_RADIUS 16

// [=]===^=[ rounded_rect_insets ]=================================[=]
static void rounded_rect_insets(int32_t radius, int32_t *out) {
	for(int32_t row = 0; row < radius; ++row) {
		int32_t dy = 2 * (radius - row) - 1;
		int32_t r2x4 = 4 * radius * radius;
		int32_t inset = 0;
		for(int32_t col = 0; col < radius; ++col) {
			int32_t dx = 2 * (radius - col) - 1;
			if(dx * dx + dy * dy > r2x4) {
				inset = col + 1;
			}
		}
		out[row] = inset;
	}
}

// [=]===^=[ corner_coverage ]=====================================[=]
static uint32_t corner_coverage(int32_t col, int32_t crow, int32_t radius) {
	int32_t r8 = 8 * radius;
	int32_t r8sq = r8 * r8;
	int32_t base_dx8 = 8 * radius - 8 * col;
	int32_t base_dy8 = 8 * radius - 8 * crow;
	uint32_t count = 0;
	for(int32_t sy = 0; sy < 4; ++sy) {
		int32_t dy8 = base_dy8 - 2 * sy - 1;
		int32_t remain = r8sq - dy8 * dy8;
		if(remain < 0) {
			continue;
		}
		for(int32_t sx = 0; sx < 4; ++sx) {
			int32_t dx8 = base_dx8 - 2 * sx - 1;
			if(dx8 * dx8 <= remain) {
				++count;
			}
		}
	}
	return count;
}

// [=]===^=[ fill_span_clipped ]===================================[=]
static void fill_span_clipped(uint32_t *buf, int32_t bw, int32_t py, int32_t left, int32_t right, uint32_t color) {
	if(left < 0) { left = 0; }
	if(left < render_clip_x1) { left = render_clip_x1; }
	if(right > bw) { right = bw; }
	if(right > render_clip_x2) { right = render_clip_x2; }
	if(left < right) {
		uint32_t *dst = &buf[py * bw + left];
		int32_t cnt = right - left;
		for(int32_t i = 0; i < cnt; ++i) {
			dst[i] = color;
		}
	}
}

// [=]===^=[ draw_rounded_rect_fill ]==============================[=]
static void draw_rounded_rect_fill(uint32_t *buf, int32_t bw, int32_t bh, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color, int32_t radius) {
	if(w <= 0 || h <= 0) {
		return;
	}
	if(radius > w / 2) {
		radius = w / 2;
	}
	if(radius > h / 2) {
		radius = h / 2;
	}
	if(radius > MKGUI_MAX_CORNER_RADIUS) {
		radius = MKGUI_MAX_CORNER_RADIUS;
	}

	for(int32_t row = 0; row < h; ++row) {
		int32_t py = y + row;
		if(py < 0 || py >= bh || py < render_clip_y1 || py >= render_clip_y2) {
			continue;
		}

		int32_t crow = -1;
		if(row < radius) {
			crow = row;
		} else if(row >= h - radius) {
			crow = h - 1 - row;
		}

		if(crow >= 0 && radius > 0) {
			uint32_t *rowp = &buf[py * bw];
			for(int32_t col = 0; col < radius; ++col) {
				uint32_t cov = corner_coverage(col, crow, radius);
				if(cov == 0) {
					continue;
				}
				int32_t pl = x + col;
				int32_t pr = x + w - 1 - col;
				if(cov >= 16) {
					if(pl >= 0 && pl < bw && pl >= render_clip_x1 && pl < render_clip_x2) {
						rowp[pl] = color;
					}
					if(pr >= 0 && pr < bw && pr != pl && pr >= render_clip_x1 && pr < render_clip_x2) {
						rowp[pr] = color;
					}
				} else {
					uint8_t alpha = (uint8_t)(cov * 255 / 16);
					if(pl >= 0 && pl < bw && pl >= render_clip_x1 && pl < render_clip_x2) {
						rowp[pl] = blend_pixel(rowp[pl], color, alpha);
					}
					if(pr >= 0 && pr < bw && pr != pl && pr >= render_clip_x1 && pr < render_clip_x2) {
						rowp[pr] = blend_pixel(rowp[pr], color, alpha);
					}
				}
			}
			fill_span_clipped(buf, bw, py, x + radius, x + w - radius, color);
		} else {
			fill_span_clipped(buf, bw, py, x, x + w, color);
		}
	}
}

// [=]===^=[ draw_rounded_rect ]===================================[=]
static void draw_rounded_rect(uint32_t *buf, int32_t bw, int32_t bh, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t fill, uint32_t border, int32_t radius) {
	draw_rounded_rect_fill(buf, bw, bh, x, y, w, h, border, radius);
	if(w > 2 && h > 2) {
		int32_t inner_r = radius > 1 ? radius - 1 : 0;
		draw_rounded_rect_fill(buf, bw, bh, x + 1, y + 1, w - 2, h - 2, fill, inner_r);
	}
}

// [=]===^=[ shade_color ]=========================================[=]
static inline uint32_t shade_color(uint32_t color, int32_t amount) {
	int32_t r = (int32_t)((color >> 16) & 0xff) + amount;
	int32_t g = (int32_t)((color >> 8) & 0xff) + amount;
	int32_t b = (int32_t)(color & 0xff) + amount;
	if(r < 0) { r = 0; } else if(r > 255) { r = 255; }
	if(g < 0) { g = 0; } else if(g > 255) { g = 255; }
	if(b < 0) { b = 0; } else if(b > 255) { b = 255; }
	return 0xff000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

// [=]===^=[ draw_hline ]========================================[=]
static inline void draw_hline(uint32_t *buf, int32_t bw, int32_t bh, int32_t x, int32_t y, int32_t w, uint32_t color) {
	if(y < 0 || y >= bh || y < render_clip_y1 || y >= render_clip_y2) {
		return;
	}
	int32_t x0 = x < 0 ? 0 : x;
	int32_t x1 = (x + w) > bw ? bw : (x + w);
	if(x0 < render_clip_x1) { x0 = render_clip_x1; }
	if(x1 > render_clip_x2) { x1 = render_clip_x2; }
	for(int32_t i = x0; i < x1; ++i) {
		buf[y * bw + i] = color;
	}
}

// [=]===^=[ draw_vline ]========================================[=]
static inline void draw_vline(uint32_t *buf, int32_t bw, int32_t bh, int32_t x, int32_t y, int32_t h, uint32_t color) {
	if(x < 0 || x >= bw || x < render_clip_x1 || x >= render_clip_x2) {
		return;
	}
	int32_t y0 = y < 0 ? 0 : y;
	int32_t y1 = (y + h) > bh ? bh : (y + h);
	if(y0 < render_clip_y1) { y0 = render_clip_y1; }
	if(y1 > render_clip_y2) { y1 = render_clip_y2; }
	for(int32_t i = y0; i < y1; ++i) {
		buf[i * bw + x] = color;
	}
}

// [=]===^=[ draw_rect_border ]==================================[=]
static void draw_rect_border(uint32_t *buf, int32_t bw, int32_t bh, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
	draw_hline(buf, bw, bh, x, y, w, color);
	draw_hline(buf, bw, bh, x, y + h - 1, w, color);
	draw_vline(buf, bw, bh, x, y, h, color);
	draw_vline(buf, bw, bh, x + w - 1, y, h, color);
}

// [=]===^=[ draw_rect_dashed ]===================================[=]
static void draw_rect_dashed(uint32_t *buf, int32_t bw, int32_t bh, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color, int32_t dash) {
	for(int32_t i = 0; i < w; ++i) {
		if((i / dash) & 1) { continue; }
		int32_t px = x + i;
		if(px >= 0 && px < bw && px >= render_clip_x1 && px < render_clip_x2) {
			if(y >= 0 && y < bh && y >= render_clip_y1 && y < render_clip_y2) {
				buf[y * bw + px] = color;
			}
			int32_t by = y + h - 1;
			if(by >= 0 && by < bh && by >= render_clip_y1 && by < render_clip_y2) {
				buf[by * bw + px] = color;
			}
		}
	}
	for(int32_t i = 0; i < h; ++i) {
		if((i / dash) & 1) { continue; }
		int32_t py = y + i;
		if(py >= 0 && py < bh && py >= render_clip_y1 && py < render_clip_y2) {
			if(x >= 0 && x < bw && x >= render_clip_x1 && x < render_clip_x2) {
				buf[py * bw + x] = color;
			}
			int32_t rx = x + w - 1;
			if(rx >= 0 && rx < bw && rx >= render_clip_x1 && rx < render_clip_x2) {
				buf[py * bw + rx] = color;
			}
		}
	}
}

enum {
	MKGUI_STYLE_RAISED,
	MKGUI_STYLE_SUNKEN,
};

// [=]===^=[ draw_patch ]==========================================[=]
static void draw_patch(struct mkgui_ctx *ctx, uint32_t style, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t fill, uint32_t border) {
	int32_t r = ctx->theme.corner_radius;
	draw_rounded_rect(ctx->pixels, ctx->win_w, ctx->win_h, x, y, w, h, fill, border, r);

	if(w > 2 && h > 2) {
		int32_t inner_r = r > 1 ? r - 1 : 0;
		int32_t insets[MKGUI_MAX_CORNER_RADIUS];
		int32_t hl_inset = 0;
		if(inner_r > 0) {
			rounded_rect_insets(inner_r, insets);
			hl_inset = insets[0];
		}
		int32_t hl_y = y + 1;
		if(hl_y >= 0 && hl_y < ctx->win_h && hl_y >= render_clip_y1 && hl_y < render_clip_y2) {
			uint32_t hl;
			if(style == MKGUI_STYLE_SUNKEN) {
				hl = shade_color(fill, -6);
			} else {
				hl = shade_color(fill, 8);
			}
			fill_span_clipped(ctx->pixels, ctx->win_w, hl_y, x + 1 + hl_inset, x + w - 1 - hl_inset, hl);
		}
	}
}

// ---------------------------------------------------------------------------
// Font init (platform-specific, see platform_linux.c / platform_win32.c)
// ---------------------------------------------------------------------------

// [=]===^=[ draw_text_sw ]=======================================[=]
static void draw_text_sw(struct mkgui_ctx *ctx, uint32_t *buf, int32_t bw, int32_t x, int32_t y, const char *text, uint32_t color, int32_t cx1, int32_t cy1, int32_t cx2, int32_t cy2) {
	int32_t cx = x;
	for(const char *p = text; *p; ++p) {
		uint32_t ch = (uint8_t)*p;
		if(ch < MKGUI_GLYPH_FIRST || ch > MKGUI_GLYPH_LAST) {
			cx += ctx->char_width;
			continue;
		}
		struct mkgui_glyph *g = &ctx->glyphs[ch - MKGUI_GLYPH_FIRST];

		int32_t gx = cx + g->bearing_x;
		if(gx >= cx2) {
			break;
		}
		if(gx + g->width <= cx1) {
			cx += g->advance;
			continue;
		}

		int32_t gy = y + ctx->font_ascent - g->bearing_y;
		int32_t col0 = cx1 - gx;
		int32_t col1 = cx2 - gx;
		if(col0 < 0) { col0 = 0; }
		if(col1 > g->width) { col1 = g->width; }
		int32_t row0 = cy1 - gy;
		int32_t row1 = cy2 - gy;
		if(row0 < 0) { row0 = 0; }
		if(row1 > g->height) { row1 = g->height; }

		for(int32_t row = row0; row < row1; ++row) {
			int32_t py = gy + row;
			uint32_t *rowp = &buf[py * bw];
			uint8_t *bmp = &g->bitmap[row * g->width];
			for(int32_t col = col0; col < col1; ++col) {
				uint8_t alpha = bmp[col];
				if(alpha == 0) {
					continue;
				}
				int32_t px = gx + col;
				rowp[px] = blend_pixel(rowp[px], color, alpha);
			}
		}

		cx += g->advance;
	}
}

// ---------------------------------------------------------------------------
// Text command buffer
// ---------------------------------------------------------------------------

struct mkgui_text_cmd {
	int32_t x, y;
	int32_t clip_x1, clip_y1, clip_x2, clip_y2;
	char text[MKGUI_MAX_TEXT];
	uint32_t color;
};

#define MKGUI_MAX_TEXT_CMDS 2048

static struct mkgui_text_cmd text_cmds[MKGUI_MAX_TEXT_CMDS];
static uint32_t text_cmd_count;

// [=]===^=[ push_text_clip ]=====================================[=]
static void push_text_clip(int32_t x, int32_t y, const char *text, uint32_t color, int32_t cx1, int32_t cy1, int32_t cx2, int32_t cy2) {
	if(text_cmd_count >= MKGUI_MAX_TEXT_CMDS) {
		return;
	}
	if(cx1 < render_clip_x1) { cx1 = render_clip_x1; }
	if(cy1 < render_clip_y1) { cy1 = render_clip_y1; }
	if(cx2 > render_clip_x2) { cx2 = render_clip_x2; }
	if(cy2 > render_clip_y2) { cy2 = render_clip_y2; }
	struct mkgui_text_cmd *cmd = &text_cmds[text_cmd_count++];
	cmd->x = x;
	cmd->y = y;
	cmd->clip_x1 = cx1;
	cmd->clip_y1 = cy1;
	cmd->clip_x2 = cx2;
	cmd->clip_y2 = cy2;
	cmd->color = color;
	size_t slen = strlen(text);
	if(slen >= MKGUI_MAX_TEXT) {
		slen = MKGUI_MAX_TEXT - 1;
	}
	memcpy(cmd->text, text, slen);
	cmd->text[slen] = '\0';
}

// [=]===^=[ push_text ]=========================================[=]
static void push_text(int32_t x, int32_t y, const char *text, uint32_t color) {
	push_text_clip(x, y, text, color, render_clip_x1, render_clip_y1, render_clip_x2, render_clip_y2);
}

// [=]===^=[ clip_intersect ]=====================================[=]
static void clip_intersect(int32_t *x1, int32_t *y1, int32_t *x2, int32_t *y2, int32_t bx1, int32_t by1, int32_t bx2, int32_t by2) {
	if(*x1 < bx1) { *x1 = bx1; }
	if(*y1 < by1) { *y1 = by1; }
	if(*x2 > bx2) { *x2 = bx2; }
	if(*y2 > by2) { *y2 = by2; }
}

// [=]===^=[ flush_text_to ]======================================[=]
static void flush_text_to(struct mkgui_ctx *ctx, uint32_t *buf, int32_t bw, int32_t bh, int32_t ox, int32_t oy) {
	for(uint32_t i = 0; i < text_cmd_count; ++i) {
		struct mkgui_text_cmd *cmd = &text_cmds[i];
		int32_t cx1 = cmd->clip_x1 - ox, cy1 = cmd->clip_y1 - oy;
		int32_t cx2 = cmd->clip_x2 - ox, cy2 = cmd->clip_y2 - oy;
		clip_intersect(&cx1, &cy1, &cx2, &cy2, 0, 0, bw, bh);
		if(cx1 >= cx2 || cy1 >= cy2) {
			continue;
		}
		draw_text_sw(ctx, buf, bw, cmd->x - ox, cmd->y - oy, cmd->text, cmd->color, cx1, cy1, cx2, cy2);
	}
	text_cmd_count = 0;
}

// [=]===^=[ flush_text ]========================================[=]
static void flush_text(struct mkgui_ctx *ctx) {
	flush_text_to(ctx, ctx->pixels, ctx->win_w, ctx->win_h, 0, 0);
}

// [=]===^=[ flush_text_popup ]==================================[=]
static void flush_text_popup(struct mkgui_ctx *ctx, struct mkgui_popup *p) {
	flush_text_to(ctx, p->pixels, p->w, p->h, p->x, p->y);
}

// [=]===^=[ text_width ]========================================[=]
static int32_t text_width(struct mkgui_ctx *ctx, const char *text) {
	int32_t w = 0;
	for(const char *p = text; *p; ++p) {
		uint32_t ch = (uint8_t)*p;
		if(ch >= MKGUI_GLYPH_FIRST && ch <= MKGUI_GLYPH_LAST) {
			w += ctx->glyphs[ch - MKGUI_GLYPH_FIRST].advance;
		} else {
			w += ctx->char_width;
		}
	}
	return w;
}

// ---------------------------------------------------------------------------
// Widget lookup helpers
// ---------------------------------------------------------------------------

// [=]===^=[ find_widget ]=======================================[=]
static struct mkgui_widget *find_widget(struct mkgui_ctx *ctx, uint32_t id) {
	for(uint32_t i = 0; i < ctx->widget_count; ++i) {
		if(ctx->widgets[i].id == id) {
			return &ctx->widgets[i];
		}
	}
	return NULL;
}

// [=]===^=[ find_widget_idx ]===================================[=]
static int32_t find_widget_idx(struct mkgui_ctx *ctx, uint32_t id) {
	for(uint32_t i = 0; i < ctx->widget_count; ++i) {
		if(ctx->widgets[i].id == id) {
			return (int32_t)i;
		}
	}
	return -1;
}

// [=]===^=[ find_tabs_data ]====================================[=]
static struct mkgui_tabs_data *find_tabs_data(struct mkgui_ctx *ctx, uint32_t widget_id) {
	for(uint32_t i = 0; i < ctx->tab_count; ++i) {
		if(ctx->tabs[i].widget_id == widget_id) {
			return &ctx->tabs[i];
		}
	}
	return NULL;
}

// [=]===^=[ find_listv_data ]==================================[=]
static struct mkgui_listview_data *find_listv_data(struct mkgui_ctx *ctx, uint32_t widget_id) {
	for(uint32_t i = 0; i < ctx->listv_count; ++i) {
		if(ctx->listvs[i].widget_id == widget_id) {
			return &ctx->listvs[i];
		}
	}
	return NULL;
}

// [=]===^=[ find_input_data ]===================================[=]
static struct mkgui_input_data *find_input_data(struct mkgui_ctx *ctx, uint32_t widget_id) {
	for(uint32_t i = 0; i < ctx->input_count; ++i) {
		if(ctx->inputs[i].widget_id == widget_id) {
			return &ctx->inputs[i];
		}
	}
	return NULL;
}

// [=]===^=[ find_dropdown_data ]================================[=]
static struct mkgui_dropdown_data *find_dropdown_data(struct mkgui_ctx *ctx, uint32_t widget_id) {
	for(uint32_t i = 0; i < ctx->dropdown_count; ++i) {
		if(ctx->dropdowns[i].widget_id == widget_id) {
			return &ctx->dropdowns[i];
		}
	}
	return NULL;
}

// [=]===^=[ find_slider_data ]=================================[=]
static struct mkgui_slider_data *find_slider_data(struct mkgui_ctx *ctx, uint32_t widget_id) {
	for(uint32_t i = 0; i < ctx->slider_count; ++i) {
		if(ctx->sliders[i].widget_id == widget_id) {
			return &ctx->sliders[i];
		}
	}
	return NULL;
}

// [=]===^=[ find_split_data ]================================[=]
static struct mkgui_split_data *find_split_data(struct mkgui_ctx *ctx, uint32_t widget_id) {
	for(uint32_t i = 0; i < ctx->split_count; ++i) {
		if(ctx->splits[i].widget_id == widget_id) {
			return &ctx->splits[i];
		}
	}
	return NULL;
}

// [=]===^=[ find_treeview_data ]=================================[=]
static struct mkgui_treeview_data *find_treeview_data(struct mkgui_ctx *ctx, uint32_t widget_id) {
	for(uint32_t i = 0; i < ctx->treeview_count; ++i) {
		if(ctx->treeviews[i].widget_id == widget_id) {
			return &ctx->treeviews[i];
		}
	}
	return NULL;
}

// [=]===^=[ find_statusbar_data ]================================[=]
static struct mkgui_statusbar_data *find_statusbar_data(struct mkgui_ctx *ctx, uint32_t widget_id) {
	for(uint32_t i = 0; i < ctx->statusbar_count; ++i) {
		if(ctx->statusbars[i].widget_id == widget_id) {
			return &ctx->statusbars[i];
		}
	}
	return NULL;
}

// [=]===^=[ find_spinbox_data ]=================================[=]
static struct mkgui_spinbox_data *find_spinbox_data(struct mkgui_ctx *ctx, uint32_t widget_id) {
	for(uint32_t i = 0; i < ctx->spinbox_count; ++i) {
		if(ctx->spinboxes[i].widget_id == widget_id) {
			return &ctx->spinboxes[i];
		}
	}
	return NULL;
}

// [=]===^=[ find_progress_data ]=================================[=]
static struct mkgui_progress_data *find_progress_data(struct mkgui_ctx *ctx, uint32_t widget_id) {
	for(uint32_t i = 0; i < ctx->progress_count; ++i) {
		if(ctx->progress[i].widget_id == widget_id) {
			return &ctx->progress[i];
		}
	}
	return NULL;
}

// [=]===^=[ find_textarea_data ]=================================[=]
static struct mkgui_textarea_data *find_textarea_data(struct mkgui_ctx *ctx, uint32_t widget_id) {
	for(uint32_t i = 0; i < ctx->textarea_count; ++i) {
		if(ctx->textareas[i].widget_id == widget_id) {
			return &ctx->textareas[i];
		}
	}
	return NULL;
}

// [=]===^=[ find_itemview_data ]=================================[=]
static struct mkgui_itemview_data *find_itemview_data(struct mkgui_ctx *ctx, uint32_t widget_id) {
	for(uint32_t i = 0; i < ctx->itemview_count; ++i) {
		if(ctx->itemviews[i].widget_id == widget_id) {
			return &ctx->itemviews[i];
		}
	}
	return NULL;
}

// [=]===^=[ find_canvas_data ]===================================[=]
static struct mkgui_canvas_data *find_canvas_data(struct mkgui_ctx *ctx, uint32_t widget_id) {
	for(uint32_t i = 0; i < ctx->canvas_count; ++i) {
		if(ctx->canvases[i].widget_id == widget_id) {
			return &ctx->canvases[i];
		}
	}
	return NULL;
}

// ---------------------------------------------------------------------------
// Widget visibility
// ---------------------------------------------------------------------------

// [=]===^=[ widget_visible ]====================================[=]
static uint32_t widget_visible(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	if(w->flags & MKGUI_HIDDEN) {
		return 0;
	}
	if(w->type == MKGUI_WINDOW) {
		return 1;
	}
	uint32_t pid = w->parent_id;
	while(pid != 0) {
		struct mkgui_widget *parent = find_widget(ctx, pid);
		if(!parent) {
			break;
		}
		if(parent->flags & MKGUI_HIDDEN) {
			return 0;
		}
		if(parent->type == MKGUI_TAB) {
			struct mkgui_widget *tabs_container = find_widget(ctx, parent->parent_id);
			if(tabs_container && tabs_container->type == MKGUI_TABS) {
				struct mkgui_tabs_data *td = find_tabs_data(ctx, tabs_container->id);
				if(td && td->active_tab != parent->id) {
					return 0;
				}
			}
		}
		pid = parent->parent_id;
	}
	return 1;
}

// ---------------------------------------------------------------------------
// Focusable check
// ---------------------------------------------------------------------------

// [=]===^=[ is_focusable ]======================================[=]
static uint32_t is_focusable(struct mkgui_widget *w) {
	if(w->flags & MKGUI_DISABLED) {
		return 0;
	}
	switch(w->type) {
		case MKGUI_BUTTON:
		case MKGUI_INPUT:
		case MKGUI_CHECKBOX:
		case MKGUI_DROPDOWN:
		case MKGUI_SLIDER:
		case MKGUI_LISTVIEW:
		case MKGUI_TREEVIEW:
		case MKGUI_SPINBOX:
		case MKGUI_RADIO:
		case MKGUI_TEXTAREA:
		case MKGUI_ITEMVIEW:
		case MKGUI_TABS: {
			return 1;
		} break;

		default: {
			return 0;
		} break;
	}
}

// ---------------------------------------------------------------------------
// Dirty rect tracking
// ---------------------------------------------------------------------------

// [=]===^=[ dirty_all ]==========================================[=]
static void dirty_all(struct mkgui_ctx *ctx) {
	ctx->dirty_full = 1;
	ctx->dirty = 1;
}

// [=]===^=[ dirty_add ]==========================================[=]
static void dirty_add(struct mkgui_ctx *ctx, int32_t x, int32_t y, int32_t w, int32_t h) {
	if(ctx->dirty_full) {
		return;
	}
	if(w <= 0 || h <= 0) {
		return;
	}

	for(uint32_t i = 0; i < ctx->dirty_count; ++i) {
		int32_t dx = ctx->dirty_rects[i].x;
		int32_t dy = ctx->dirty_rects[i].y;
		int32_t dw = ctx->dirty_rects[i].w;
		int32_t dh = ctx->dirty_rects[i].h;
		int32_t ux = dx < x ? dx : x;
		int32_t uy = dy < y ? dy : y;
		int32_t ur = (dx + dw) > (x + w) ? (dx + dw) : (x + w);
		int32_t ub = (dy + dh) > (y + h) ? (dy + dh) : (y + h);
		int32_t uw = ur - ux;
		int32_t uh = ub - uy;
		int64_t union_area = (int64_t)uw * uh;
		int64_t sum_area = (int64_t)dw * dh + (int64_t)w * h;
		if(union_area <= sum_area + sum_area / 2) {
			ctx->dirty_rects[i].x = ux;
			ctx->dirty_rects[i].y = uy;
			ctx->dirty_rects[i].w = uw;
			ctx->dirty_rects[i].h = uh;
			ctx->dirty = 1;
			return;
		}
	}

	if(ctx->dirty_count >= 32) {
		dirty_all(ctx);
		return;
	}

	ctx->dirty_rects[ctx->dirty_count].x = x;
	ctx->dirty_rects[ctx->dirty_count].y = y;
	ctx->dirty_rects[ctx->dirty_count].w = w;
	ctx->dirty_rects[ctx->dirty_count].h = h;
	++ctx->dirty_count;
	ctx->dirty = 1;
}

// [=]===^=[ dirty_widget ]========================================[=]
static void dirty_widget(struct mkgui_ctx *ctx, uint32_t idx) {
	dirty_add(ctx, ctx->rects[idx].x, ctx->rects[idx].y, ctx->rects[idx].w, ctx->rects[idx].h);
}

// [=]===^=[ dirty_widget_id ]=====================================[=]
static void dirty_widget_id(struct mkgui_ctx *ctx, uint32_t id) {
	int32_t idx = find_widget_idx(ctx, id);
	if(idx >= 0) {
		dirty_widget(ctx, (uint32_t)idx);
	}
}

// [=]===^=[ dirty_intersects ]====================================[=]
static uint32_t dirty_intersects(struct mkgui_ctx *ctx, int32_t x, int32_t y, int32_t w, int32_t h) {
	if(ctx->dirty_full) {
		return 1;
	}
	for(uint32_t i = 0; i < ctx->dirty_count; ++i) {
		int32_t dx = ctx->dirty_rects[i].x;
		int32_t dy = ctx->dirty_rects[i].y;
		int32_t dw = ctx->dirty_rects[i].w;
		int32_t dh = ctx->dirty_rects[i].h;
		if(x < dx + dw && x + w > dx && y < dy + dh && y + h > dy) {
			return 1;
		}
	}
	return 0;
}

// [=]===^=[ dirty_bounds ]========================================[=]
static void dirty_bounds(struct mkgui_ctx *ctx, int32_t *bx, int32_t *by, int32_t *bw, int32_t *bh) {
	if(ctx->dirty_full || ctx->dirty_count == 0) {
		*bx = 0;
		*by = 0;
		*bw = ctx->win_w;
		*bh = ctx->win_h;
		return;
	}
	int32_t x0 = ctx->dirty_rects[0].x;
	int32_t y0 = ctx->dirty_rects[0].y;
	int32_t x1 = x0 + ctx->dirty_rects[0].w;
	int32_t y1 = y0 + ctx->dirty_rects[0].h;
	for(uint32_t i = 1; i < ctx->dirty_count; ++i) {
		int32_t dx = ctx->dirty_rects[i].x;
		int32_t dy = ctx->dirty_rects[i].y;
		if(dx < x0) { x0 = dx; }
		if(dy < y0) { y0 = dy; }
		int32_t dr = dx + ctx->dirty_rects[i].w;
		int32_t db = dy + ctx->dirty_rects[i].h;
		if(dr > x1) { x1 = dr; }
		if(db > y1) { y1 = db; }
	}
	if(x0 < 0) { x0 = 0; }
	if(y0 < 0) { y0 = 0; }
	if(x1 > ctx->win_w) { x1 = ctx->win_w; }
	if(y1 > ctx->win_h) { y1 = ctx->win_h; }
	*bx = x0;
	*by = y0;
	*bw = x1 - x0;
	*bh = y1 - y0;
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

// [=]===^=[ find_box_scroll ]==================================[=]
static struct mkgui_box_scroll *find_box_scroll(struct mkgui_ctx *ctx, uint32_t widget_id) {
	for(uint32_t i = 0; i < ctx->box_scroll_count; ++i) {
		if(ctx->box_scrolls[i].widget_id == widget_id) {
			return &ctx->box_scrolls[i];
		}
	}
	return NULL;
}

// ---------------------------------------------------------------------------
// Layout index (pre-built per layout pass for O(1) lookups)
// ---------------------------------------------------------------------------

#define LAYOUT_HASH_SIZE 2048
#define LAYOUT_HASH_MASK (LAYOUT_HASH_SIZE - 1)

static struct {
	uint32_t id;
	uint32_t idx;
} layout_id_map[LAYOUT_HASH_SIZE];

static uint32_t layout_parent[MKGUI_MAX_WIDGETS];
static uint32_t layout_first_child[MKGUI_MAX_WIDGETS];
static uint32_t layout_next_sibling[MKGUI_MAX_WIDGETS];

// [=]===^=[ layout_find_idx ]====================================[=]
static uint32_t layout_find_idx(uint32_t id) {
	uint32_t h = (id * 2654435761u) & LAYOUT_HASH_MASK;
	for(;;) {
		if(layout_id_map[h].idx == UINT32_MAX) {
			return UINT32_MAX;
		}
		if(layout_id_map[h].id == id) {
			return layout_id_map[h].idx;
		}
		h = (h + 1) & LAYOUT_HASH_MASK;
	}
}

// [=]===^=[ layout_build_index ]==================================[=]
static void layout_build_index(struct mkgui_ctx *ctx) {
	for(uint32_t i = 0; i < LAYOUT_HASH_SIZE; ++i) {
		layout_id_map[i].idx = UINT32_MAX;
	}
	for(uint32_t i = 0; i < ctx->widget_count; ++i) {
		uint32_t h = (ctx->widgets[i].id * 2654435761u) & LAYOUT_HASH_MASK;
		while(layout_id_map[h].idx != UINT32_MAX) {
			h = (h + 1) & LAYOUT_HASH_MASK;
		}
		layout_id_map[h].id = ctx->widgets[i].id;
		layout_id_map[h].idx = i;
		layout_first_child[i] = UINT32_MAX;
		layout_next_sibling[i] = UINT32_MAX;
	}
	uint32_t i = ctx->widget_count;
	while(i > 0) {
		--i;
		uint32_t pidx = layout_find_idx(ctx->widgets[i].parent_id);
		layout_parent[i] = pidx;
		if(pidx < ctx->widget_count && pidx != i) {
			layout_next_sibling[i] = layout_first_child[pidx];
			layout_first_child[pidx] = i;
		}
	}
}

// [=]===^=[ layout_child_area ]=================================[=]
static void layout_child_area(struct mkgui_ctx *ctx, uint32_t pidx, uint32_t cidx, int32_t px, int32_t py, int32_t pw, int32_t ph) {
	struct mkgui_widget *w = &ctx->widgets[cidx];
	struct mkgui_widget *parent = &ctx->widgets[pidx];

	if(parent->type == MKGUI_HSPLIT || parent->type == MKGUI_VSPLIT) {
		struct mkgui_split_data *sd = find_split_data(ctx, parent->id);
		float ratio = sd ? sd->ratio : 0.5f;
		if(parent->type == MKGUI_HSPLIT) {
			int32_t split = (int32_t)(ph * ratio);
			if(w->flags & MKGUI_REGION_TOP) {
				ph = split;
			} else if(w->flags & MKGUI_REGION_BOTTOM) {
				py += split + MKGUI_SPLIT_THICK;
				ph = ph - split - MKGUI_SPLIT_THICK;
			}
		} else {
			int32_t split = (int32_t)(pw * ratio);
			if(w->flags & MKGUI_REGION_LEFT) {
				pw = split;
			} else if(w->flags & MKGUI_REGION_RIGHT) {
				px += split + MKGUI_SPLIT_THICK;
				pw = pw - split - MKGUI_SPLIT_THICK;
			}
		}
	}

	uint32_t flags = w->flags;
	int32_t ox = w->x;
	int32_t oy = w->y;
	int32_t ow = w->w;
	int32_t oh = w->h;
	int32_t rx, ry, rw, rh;
	uint32_t has_anchor = flags & (MKGUI_ANCHOR_LEFT | MKGUI_ANCHOR_TOP | MKGUI_ANCHOR_RIGHT | MKGUI_ANCHOR_BOTTOM);

	if(!has_anchor) {
		rx = px + ox;
		ry = py + oy;
		rw = ow;
		rh = oh;
	} else {
		if((flags & MKGUI_ANCHOR_LEFT) && (flags & MKGUI_ANCHOR_RIGHT)) {
			rx = px + ox;
			rw = pw - ox - (ow > 0 ? ow : 0);
		} else if(flags & MKGUI_ANCHOR_RIGHT) {
			rx = px + pw - ow - ox;
			rw = ow;
		} else {
			rx = px + ox;
			rw = ow;
		}
		if((flags & MKGUI_ANCHOR_TOP) && (flags & MKGUI_ANCHOR_BOTTOM)) {
			ry = py + oy;
			rh = ph - oy - (oh > 0 ? oh : 0);
		} else if(flags & MKGUI_ANCHOR_BOTTOM) {
			ry = py + ph - oh - oy;
			rh = oh;
		} else {
			ry = py + oy;
			rh = oh;
		}
	}

	if(w->type == MKGUI_TABS && ow == 0 && oh == 0 && !has_anchor) {
		rx = px; ry = py; rw = pw; rh = ph;
	}
	if(w->type == MKGUI_TAB) {
		rx = px; ry = py; rw = pw; rh = ph;
	}
	if((parent->type == MKGUI_HSPLIT || parent->type == MKGUI_VSPLIT) && (flags & (MKGUI_REGION_TOP | MKGUI_REGION_BOTTOM | MKGUI_REGION_LEFT | MKGUI_REGION_RIGHT))) {
		rx = px; ry = py; rw = pw; rh = ph;
	}

	ctx->rects[cidx].x = rx;
	ctx->rects[cidx].y = ry;
	ctx->rects[cidx].w = rw;
	ctx->rects[cidx].h = rh;

	if(w->type == MKGUI_GROUP || w->type == MKGUI_TABS) {
		ctx->rects[cidx].x += MKGUI_MARGIN;
		ctx->rects[cidx].y += MKGUI_MARGIN;
		ctx->rects[cidx].w -= MKGUI_MARGIN * 2;
		ctx->rects[cidx].h -= MKGUI_MARGIN * 2;
	}
}

// [=]===^=[ layout_node ]========================================[=]
static void layout_node(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];

	int32_t px = ctx->rects[idx].x;
	int32_t py = ctx->rects[idx].y;
	int32_t pw = ctx->rects[idx].w;
	int32_t ph = ctx->rects[idx].h;

	if(w->type == MKGUI_WINDOW) {
		int32_t top_y = py;
		int32_t bot_y = py + ph;
		for(uint32_t c = layout_first_child[idx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
			if(ctx->widgets[c].type == MKGUI_MENU) {
				ctx->rects[c].x = px;
				ctx->rects[c].y = top_y;
				ctx->rects[c].w = pw;
				ctx->rects[c].h = MKGUI_MENU_HEIGHT;
				top_y += MKGUI_MENU_HEIGHT;
			}
		}
		for(uint32_t c = layout_first_child[idx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
			if(ctx->widgets[c].type == MKGUI_TOOLBAR) {
				ctx->rects[c].x = px;
				ctx->rects[c].y = top_y;
				ctx->rects[c].w = pw;
				ctx->rects[c].h = MKGUI_TOOLBAR_HEIGHT;
				top_y += MKGUI_TOOLBAR_HEIGHT;
			}
		}
		for(uint32_t c = layout_first_child[idx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
			if(ctx->widgets[c].type == MKGUI_STATUSBAR) {
				bot_y -= MKGUI_STATUSBAR_HEIGHT;
				ctx->rects[c].x = px;
				ctx->rects[c].y = bot_y;
				ctx->rects[c].w = pw;
				ctx->rects[c].h = MKGUI_STATUSBAR_HEIGHT;
			}
		}
		py = top_y;
		ph = bot_y - top_y;
	}

	if(w->type == MKGUI_TAB) {
		px += 2;
		py += MKGUI_TAB_HEIGHT + 2;
		pw -= 4;
		ph -= MKGUI_TAB_HEIGHT + 4;
	}
	if(w->type == MKGUI_GROUP) {
		int32_t gtop = ctx->font_height + 4;
		int32_t gpad = 6;
		px += gpad;
		py += gtop;
		pw -= gpad * 2;
		ph -= gtop + gpad;
	}

	uint32_t box_has_pad = 0;
	if(w->type == MKGUI_VBOX || w->type == MKGUI_HBOX || w->type == MKGUI_FORM) {
		if(!(w->flags & MKGUI_NO_PAD)) {
			uint32_t gpidx = layout_parent[idx];
			uint32_t nested = 0;
			if(gpidx < ctx->widget_count) {
				uint32_t gpt = ctx->widgets[gpidx].type;
				if(gpt == MKGUI_VBOX || gpt == MKGUI_HBOX || gpt == MKGUI_FORM || gpt == MKGUI_GROUP || gpt == MKGUI_TABS) {
					nested = 1;
				}
			}
			if(!nested || (w->flags & MKGUI_PANEL_BORDER)) {
				px += MKGUI_BOX_PAD;
				py += MKGUI_BOX_PAD;
				pw -= MKGUI_BOX_PAD * 2;
				ph -= MKGUI_BOX_PAD * 2;
				box_has_pad = 1;
			}
		}

		uint32_t child_count = 0;
		for(uint32_t j = layout_first_child[idx]; j < ctx->widget_count; j = layout_next_sibling[j]) {
			if(!(ctx->widgets[j].flags & MKGUI_HIDDEN)) {
				++child_count;
			}
		}

		if(w->type == MKGUI_VBOX) {
			uint32_t scrollable = (w->flags & MKGUI_SCROLL) ? 1 : 0;
			struct mkgui_box_scroll *bs = scrollable ? find_box_scroll(ctx, w->id) : NULL;
			int32_t fixed_total = 0;
			uint32_t flex_count = 0;
			for(uint32_t j = layout_first_child[idx]; j < ctx->widget_count; j = layout_next_sibling[j]) {
				if(!(ctx->widgets[j].flags & MKGUI_HIDDEN)) {
					if(ctx->widgets[j].h > 0) {
						fixed_total += ctx->widgets[j].h;
					} else {
						++flex_count;
					}
				}
			}
			int32_t gap_total = child_count > 1 ? (int32_t)(child_count - 1) * MKGUI_BOX_GAP : 0;
			uint32_t needs_scroll = scrollable && (fixed_total + gap_total > ph);
			if(needs_scroll) {
				pw -= MKGUI_SCROLLBAR_W;
			}
			if(bs) {
				bs->content_h = fixed_total + gap_total;
				if(box_has_pad) {
					bs->content_h += MKGUI_BOX_PAD * 2;
				}
			}
			int32_t remaining = ph - fixed_total - gap_total;
			if(remaining < 0) {
				remaining = 0;
			}
			int32_t flex_h = needs_scroll ? 0 : (flex_count > 0 ? remaining / (int32_t)flex_count : 0);
			int32_t vflex_rem = (!needs_scroll && flex_count > 0) ? (remaining - flex_h * (int32_t)flex_count) : 0;
			int32_t scroll_off = (bs && needs_scroll) ? bs->scroll_y : 0;
			uint32_t vflex_idx = 0;
			int32_t cy = py - scroll_off;
			for(uint32_t j = layout_first_child[idx]; j < ctx->widget_count; j = layout_next_sibling[j]) {
				if(!(ctx->widgets[j].flags & MKGUI_HIDDEN)) {
					int32_t ch;
					if(ctx->widgets[j].h > 0) {
						ch = ctx->widgets[j].h;
					} else {
						ch = flex_h + ((int32_t)vflex_idx < vflex_rem ? 1 : 0);
						++vflex_idx;
					}
					ctx->rects[j].x = px;
					ctx->rects[j].y = cy;
					ctx->rects[j].w = pw;
					ctx->rects[j].h = ch;
					cy += ch + MKGUI_BOX_GAP;
				}
			}

		} else if(w->type == MKGUI_HBOX) {
			uint32_t scrollable = (w->flags & MKGUI_SCROLL) ? 1 : 0;
			struct mkgui_box_scroll *bs = scrollable ? find_box_scroll(ctx, w->id) : NULL;
			int32_t fixed_total = 0;
			uint32_t flex_count = 0;
			for(uint32_t j = layout_first_child[idx]; j < ctx->widget_count; j = layout_next_sibling[j]) {
				if(!(ctx->widgets[j].flags & MKGUI_HIDDEN)) {
					if(ctx->widgets[j].w > 0) {
						fixed_total += ctx->widgets[j].w;
					} else {
						++flex_count;
					}
				}
			}
			int32_t gap_total = child_count > 1 ? (int32_t)(child_count - 1) * MKGUI_BOX_GAP : 0;
			uint32_t needs_scroll = scrollable && (fixed_total + gap_total > pw);
			if(needs_scroll) {
				ph -= MKGUI_SCROLLBAR_W;
			}
			if(bs) {
				bs->content_w = fixed_total + gap_total;
			}
			int32_t remaining = pw - fixed_total - gap_total;
			if(remaining < 0) {
				remaining = 0;
			}
			int32_t flex_w = needs_scroll ? 0 : (flex_count > 0 ? remaining / (int32_t)flex_count : 0);
			int32_t flex_rem = (!needs_scroll && flex_count > 0) ? (remaining - flex_w * (int32_t)flex_count) : 0;
			int32_t scroll_off = (bs && needs_scroll) ? bs->scroll_x : 0;
			uint32_t flex_idx = 0;
			int32_t cx = px - scroll_off;
			for(uint32_t j = layout_first_child[idx]; j < ctx->widget_count; j = layout_next_sibling[j]) {
				if(!(ctx->widgets[j].flags & MKGUI_HIDDEN)) {
					int32_t cw;
					if(ctx->widgets[j].w > 0) {
						cw = ctx->widgets[j].w;
					} else {
						cw = flex_w + ((int32_t)flex_idx < flex_rem ? 1 : 0);
						++flex_idx;
					}
					ctx->rects[j].x = cx;
					ctx->rects[j].y = py;
					ctx->rects[j].w = cw;
					ctx->rects[j].h = ph;
					cx += cw + MKGUI_BOX_GAP;
				}
			}

		} else {
			int32_t label_w = 0;
			uint32_t pair_idx = 0;
			for(uint32_t j = layout_first_child[idx]; j < ctx->widget_count; j = layout_next_sibling[j]) {
				if(!(ctx->widgets[j].flags & MKGUI_HIDDEN)) {
					if((pair_idx & 1) == 0) {
						int32_t tw = text_width(ctx, ctx->widgets[j].label) + 8;
						if(tw > label_w) {
							label_w = tw;
						}
					}
					++pair_idx;
				}
			}
			pair_idx = 0;
			for(uint32_t j = layout_first_child[idx]; j < ctx->widget_count; j = layout_next_sibling[j]) {
				if(!(ctx->widgets[j].flags & MKGUI_HIDDEN)) {
					uint32_t row = pair_idx / 2;
					uint32_t row_h = 24;
					if((pair_idx & 1) == 0) {
						ctx->rects[j].x = px;
						ctx->rects[j].y = py + (int32_t)(row * (row_h + MKGUI_BOX_GAP));
						ctx->rects[j].w = label_w;
						ctx->rects[j].h = (int32_t)row_h;
					} else {
						ctx->rects[j].x = px + label_w;
						ctx->rects[j].y = py + (int32_t)(row * (row_h + MKGUI_BOX_GAP));
						ctx->rects[j].w = pw - label_w;
						ctx->rects[j].h = (int32_t)row_h;
					}
					++pair_idx;
				}
			}
		}

	} else if(w->type == MKGUI_TOOLBAR) {
		int32_t bx = px + 2;
		for(uint32_t c = layout_first_child[idx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
			struct mkgui_widget *btn = &ctx->widgets[c];
			if(btn->type != MKGUI_BUTTON) {
				continue;
			}
			if(btn->flags & MKGUI_TOOLBAR_SEP) {
				bx += 8;
			}
			int32_t tw = text_width(ctx, btn->label);
			int32_t icon_w = btn->icon[0] ? 22 : 0;
			int32_t btn_w = icon_w + tw + 12;
			if(btn_w < 28) {
				btn_w = 28;
			}
			ctx->rects[c].x = bx;
			ctx->rects[c].y = py + 2;
			ctx->rects[c].w = btn_w;
			ctx->rects[c].h = ph - 4;
			bx += btn_w + 2;
		}

	} else {
		for(uint32_t c = layout_first_child[idx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
			uint32_t ct = ctx->widgets[c].type;
			if(ct == MKGUI_MENUITEM || (ctx->widgets[c].flags & MKGUI_HIDDEN)) {
				continue;
			}
			if(w->type == MKGUI_WINDOW && (ct == MKGUI_MENU || ct == MKGUI_TOOLBAR || ct == MKGUI_STATUSBAR)) {
				continue;
			}
			layout_child_area(ctx, idx, c, px, py, pw, ph);
		}
	}

	for(uint32_t c = layout_first_child[idx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
		if(ctx->widgets[c].type != MKGUI_MENUITEM && !(ctx->widgets[c].flags & MKGUI_HIDDEN)) {
			layout_node(ctx, c);
		}
	}
}

// [=]===^=[ layout_widgets ]====================================[=]
static void layout_widgets(struct mkgui_ctx *ctx) {
	layout_build_index(ctx);
	for(uint32_t i = 0; i < ctx->widget_count; ++i) {
		if(ctx->widgets[i].type == MKGUI_WINDOW) {
			ctx->rects[i].x = 0;
			ctx->rects[i].y = 0;
			ctx->rects[i].w = ctx->win_w;
			ctx->rects[i].h = ctx->win_h;
			layout_node(ctx, i);
		}
	}
}

// ---------------------------------------------------------------------------
// Hit testing
// ---------------------------------------------------------------------------

// [=]===^=[ hit_test ]==========================================[=]
static int32_t hit_test(struct mkgui_ctx *ctx, int32_t mx, int32_t my) {
	for(int32_t i = (int32_t)ctx->widget_count - 1; i >= 0; --i) {
		if(!widget_visible(ctx, (uint32_t)i)) {
			continue;
		}
		struct mkgui_widget *w = &ctx->widgets[i];
		if(w->type == MKGUI_WINDOW || w->type == MKGUI_TAB || w->type == MKGUI_GROUP || w->type == MKGUI_PANEL || w->type == MKGUI_SPINNER || w->type == MKGUI_GLVIEW || w->type == MKGUI_VBOX || w->type == MKGUI_HBOX || w->type == MKGUI_FORM) {
			continue;
		}
		if(w->type == MKGUI_HSPLIT || w->type == MKGUI_VSPLIT) {
			int32_t rx = ctx->rects[i].x;
			int32_t ry = ctx->rects[i].y;
			int32_t rw = ctx->rects[i].w;
			int32_t rh = ctx->rects[i].h;
			struct mkgui_split_data *sd = find_split_data(ctx, w->id);
			float ratio = sd ? sd->ratio : 0.5f;
			if(w->type == MKGUI_HSPLIT) {
				int32_t split_y = ry + (int32_t)(rh * ratio);
				if(mx >= rx && mx < rx + rw && my >= split_y && my < split_y + MKGUI_SPLIT_THICK) {
					return i;
				}
			} else {
				int32_t split_x = rx + (int32_t)(rw * ratio);
				if(mx >= split_x && mx < split_x + MKGUI_SPLIT_THICK && my >= ry && my < ry + rh) {
					return i;
				}
			}
			continue;
		}
		int32_t rx = ctx->rects[i].x;
		int32_t ry = ctx->rects[i].y;
		int32_t rw = ctx->rects[i].w;
		int32_t rh = ctx->rects[i].h;
		if(mx >= rx && mx < rx + rw && my >= ry && my < ry + rh) {
			return i;
		}
	}
	return -1;
}

// ---------------------------------------------------------------------------
// Window registry
// ---------------------------------------------------------------------------

#define MKGUI_MAX_WINDOWS 16

static struct mkgui_ctx *window_registry[MKGUI_MAX_WINDOWS];
static uint32_t window_registry_count;

// [=]===^=[ window_register ]====================================[=]
static void window_register(struct mkgui_ctx *ctx) {
	if(window_registry_count < MKGUI_MAX_WINDOWS) {
		window_registry[window_registry_count++] = ctx;
	}
}

// [=]===^=[ window_unregister ]==================================[=]
static void window_unregister(struct mkgui_ctx *ctx) {
	for(uint32_t i = 0; i < window_registry_count; ++i) {
		if(window_registry[i] == ctx) {
			window_registry[i] = window_registry[--window_registry_count];
			return;
		}
	}
}

// ---------------------------------------------------------------------------
// Platform backend
// ---------------------------------------------------------------------------

#ifdef _WIN32
#include "platform_win32.c"
#else
#include "platform_linux.c"
#endif

// ---------------------------------------------------------------------------
// Popup windows (override-redirect)
// ---------------------------------------------------------------------------

// [=]===^=[ popup_create ]======================================[=]
static struct mkgui_popup *popup_create(struct mkgui_ctx *ctx, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t widget_id) {
	if(ctx->popup_count >= MKGUI_MAX_POPUPS) {
		return NULL;
	}
	struct mkgui_popup *p = &ctx->popups[ctx->popup_count];
	memset(p, 0, sizeof(*p));

	if(!platform_popup_init(ctx, p, x, y, w, h)) {
		return NULL;
	}

	p->x = x;
	p->y = y;
	p->w = w;
	p->h = h;
	p->widget_id = widget_id;
	p->active = 1;
	p->dirty = 1;
	p->hover_item = -1;

	++ctx->popup_count;
	return p;
}

// [=]===^=[ popup_destroy ]=====================================[=]
static void popup_destroy(struct mkgui_ctx *ctx, uint32_t popup_idx) {
	if(popup_idx >= ctx->popup_count) {
		return;
	}
	struct mkgui_popup *p = &ctx->popups[popup_idx];
	platform_popup_fini(ctx, p);
	p->active = 0;

	for(uint32_t i = popup_idx; i < ctx->popup_count - 1; ++i) {
		ctx->popups[i] = ctx->popups[i + 1];
	}
	--ctx->popup_count;
}

// [=]===^=[ popup_destroy_all ]=================================[=]
static void popup_destroy_all(struct mkgui_ctx *ctx) {
	while(ctx->popup_count > 0) {
		popup_destroy(ctx, 0);
	}
}

// [=]===^=[ popup_destroy_from ]================================[=]
static void popup_destroy_from(struct mkgui_ctx *ctx, uint32_t from_idx) {
	while(ctx->popup_count > from_idx) {
		popup_destroy(ctx, ctx->popup_count - 1);
	}
}

// ---------------------------------------------------------------------------
// Icon drawing
// ---------------------------------------------------------------------------

// [=]===^=[ draw_icon ]===========================================[=]
static void draw_icon(uint32_t *buf, int32_t bw, int32_t bh, struct mkgui_icon *icon, int32_t x, int32_t y, int32_t cx1, int32_t cy1, int32_t cx2, int32_t cy2) {
	int32_t row0 = 0;
	int32_t row1 = icon->h;
	int32_t col0 = 0;
	int32_t col1 = icon->w;
	if(y + row0 < cy1) { row0 = cy1 - y; }
	if(y + row0 < 0) { row0 = -y; }
	if(y + row1 > cy2) { row1 = cy2 - y; }
	if(y + row1 > bh) { row1 = bh - y; }
	if(x + col0 < cx1) { col0 = cx1 - x; }
	if(x + col0 < 0) { col0 = -x; }
	if(x + col1 > cx2) { col1 = cx2 - x; }
	if(x + col1 > bw) { col1 = bw - x; }
	for(int32_t row = row0; row < row1; ++row) {
		uint32_t *rowp = &buf[(y + row) * bw];
		uint32_t *src = &icon->pixels[row * icon->w];
		for(int32_t col = col0; col < col1; ++col) {
			uint32_t spx = src[col];
			uint32_t alpha = spx >> 24;
			if(alpha == 0) {
				continue;
			}
			int32_t px = x + col;
			if(alpha == 255) {
				rowp[px] = spx;
			} else {
				rowp[px] = blend_pixel(rowp[px], spx, (uint8_t)alpha);
			}
		}
	}
}

// [=]===^=[ draw_icon_popup ]=====================================[=]
static void draw_icon_popup(struct mkgui_popup *p, struct mkgui_icon *icon, int32_t x, int32_t y) {
	draw_icon(p->pixels, p->w, p->h, icon, x, y, 0, 0, p->w, p->h);
}

// ---------------------------------------------------------------------------
// Widget implementations
// ---------------------------------------------------------------------------

#include "mkgui_icon.c"
#include "mkgui_button.c"
#include "mkgui_label.c"
#include "mkgui_input.c"
#include "mkgui_checkbox.c"
#include "mkgui_dropdown.c"
#include "mkgui_slider.c"
#include "mkgui_listview.c"
#include "mkgui_tabs.c"
#include "mkgui_radio.c"
#include "mkgui_menu.c"
#include "mkgui_split.c"
#include "mkgui_treeview.c"
#include "mkgui_statusbar.c"
#include "mkgui_toolbar.c"
#include "mkgui_spinbox.c"
#include "mkgui_progress.c"
#include "mkgui_textarea.c"
#include "mkgui_group.c"
#include "mkgui_panel.c"
#include "mkgui_spinner.c"
#include "mkgui_itemview.c"
#include "mkgui_scrollbar.c"
#include "mkgui_image.c"
#include "mkgui_glview.c"
#include "mkgui_canvas.c"
#include "mkgui_tooltip.c"

// ---------------------------------------------------------------------------
// Render dispatch
// ---------------------------------------------------------------------------

// [=]===^=[ render_widget ]=====================================[=]
static void render_widget(struct mkgui_ctx *ctx, uint32_t idx) {
	switch(ctx->widgets[idx].type) {
		case MKGUI_BUTTON: {
			render_button(ctx, idx);
		} break;

		case MKGUI_LABEL: {
			render_label(ctx, idx);
		} break;

		case MKGUI_INPUT: {
			render_input(ctx, idx);
		} break;

		case MKGUI_CHECKBOX: {
			render_checkbox(ctx, idx);
		} break;

		case MKGUI_SLIDER: {
			render_slider(ctx, idx);
		} break;

		case MKGUI_DROPDOWN: {
			render_dropdown(ctx, idx);
		} break;

		case MKGUI_TABS: {
			render_tabs(ctx, idx);
		} break;

		case MKGUI_LISTVIEW: {
			render_listview(ctx, idx);
		} break;

		case MKGUI_HSPLIT:
		case MKGUI_VSPLIT: {
			render_split(ctx, idx);
		} break;

		case MKGUI_MENU: {
			render_menu_bar(ctx, idx);
		} break;

		case MKGUI_TREEVIEW: {
			render_treeview(ctx, idx);
		} break;

		case MKGUI_STATUSBAR: {
			render_statusbar(ctx, idx);
		} break;

		case MKGUI_TOOLBAR: {
			render_toolbar(ctx, idx);
		} break;

		case MKGUI_SPINBOX: {
			render_spinbox(ctx, idx);
		} break;

		case MKGUI_RADIO: {
			render_radio(ctx, idx);
		} break;

		case MKGUI_PROGRESS: {
			render_progress(ctx, idx);
		} break;

		case MKGUI_TEXTAREA: {
			render_textarea(ctx, idx);
		} break;

		case MKGUI_GROUP: {
			render_group(ctx, idx);
		} break;

		case MKGUI_SPINNER: {
			render_spinner(ctx, idx);
		} break;

		case MKGUI_ITEMVIEW: {
			render_itemview(ctx, idx);
		} break;

		case MKGUI_PANEL: {
			render_panel(ctx, idx);
		} break;

		case MKGUI_SCROLLBAR: {
			render_scrollbar(ctx, idx);
		} break;

		case MKGUI_IMAGE: {
			render_image(ctx, idx);
		} break;

		case MKGUI_GLVIEW: {
			render_glview(ctx, idx);
		} break;

		case MKGUI_CANVAS: {
			render_canvas(ctx, idx);
		} break;

		case MKGUI_VBOX:
		case MKGUI_HBOX:
		case MKGUI_FORM: {
			if(ctx->widgets[idx].flags & MKGUI_PANEL_BORDER) {
				render_panel(ctx, idx);
			}
			if(ctx->widgets[idx].flags & MKGUI_SCROLL) {
				struct mkgui_box_scroll *bs = find_box_scroll(ctx, ctx->widgets[idx].id);
				int32_t rx = ctx->rects[idx].x;
				int32_t ry = ctx->rects[idx].y;
				int32_t rw = ctx->rects[idx].w;
				int32_t rh = ctx->rects[idx].h;
				int32_t r = ctx->theme.corner_radius;
				if(bs && bs->content_h > rh) {
					int32_t sx = rx + rw - MKGUI_SCROLLBAR_W;
					draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, sx, ry, MKGUI_SCROLLBAR_W, rh, ctx->theme.scrollbar_bg);
					int32_t track = rh;
					int32_t total = bs->content_h;
					if(total < 1) { total = 1; }
					int32_t thumb = track * rh / total;
					if(thumb < 20) { thumb = 20; }
					if(thumb > track) { thumb = track; }
					int32_t max_scroll = bs->content_h - rh;
					if(max_scroll < 1) { max_scroll = 1; }
					int32_t pos = (int32_t)((int64_t)bs->scroll_y * (track - thumb) / max_scroll);
					if(pos < 0) { pos = 0; }
					if(pos > track - thumb) { pos = track - thumb; }
					draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, sx + 2, ry + pos, MKGUI_SCROLLBAR_W - 4, thumb, ctx->theme.scrollbar_thumb, r);
				}
				if(bs && bs->content_w > rw) {
					int32_t sy = ry + rh - MKGUI_SCROLLBAR_W;
					draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx, sy, rw, MKGUI_SCROLLBAR_W, ctx->theme.scrollbar_bg);
					int32_t track = rw;
					int32_t total = bs->content_w;
					if(total < 1) { total = 1; }
					int32_t thumb = track * rw / total;
					if(thumb < 20) { thumb = 20; }
					if(thumb > track) { thumb = track; }
					int32_t max_scroll = bs->content_w - rw;
					if(max_scroll < 1) { max_scroll = 1; }
					int32_t pos = (int32_t)((int64_t)bs->scroll_x * (track - thumb) / max_scroll);
					if(pos < 0) { pos = 0; }
					if(pos > track - thumb) { pos = track - thumb; }
					draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx + pos, sy + 2, thumb, MKGUI_SCROLLBAR_W - 4, ctx->theme.scrollbar_thumb, r);
				}
			}
		} break;

		default: {
		} break;
	}
}

// [=]===^=[ set_parent_clip ]====================================[=]
static void set_parent_clip(struct mkgui_ctx *ctx, uint32_t idx) {
	render_clip_x1 = render_base_clip_x1;
	render_clip_y1 = render_base_clip_y1;
	render_clip_x2 = render_base_clip_x2;
	render_clip_y2 = render_base_clip_y2;
	int32_t pidx = find_widget_idx(ctx, ctx->widgets[idx].parent_id);
	while(pidx >= 0 && ctx->widgets[pidx].type != MKGUI_WINDOW) {
		int32_t px = ctx->rects[pidx].x;
		int32_t py = ctx->rects[pidx].y;
		int32_t px2 = px + ctx->rects[pidx].w;
		int32_t py2 = py + ctx->rects[pidx].h;
		if(px > render_clip_x1) { render_clip_x1 = px; }
		if(py > render_clip_y1) { render_clip_y1 = py; }
		if(px2 < render_clip_x2) { render_clip_x2 = px2; }
		if(py2 < render_clip_y2) { render_clip_y2 = py2; }
		pidx = find_widget_idx(ctx, ctx->widgets[pidx].parent_id);
	}
}

// [=]===^=[ render_widgets ]====================================[=]
static void render_widgets(struct mkgui_ctx *ctx) {
	text_cmd_count = 0;

	if(ctx->dirty_full || ctx->render_cb) {
		render_base_clip_x1 = 0;
		render_base_clip_y1 = 0;
		render_base_clip_x2 = ctx->win_w;
		render_base_clip_y2 = ctx->win_h;
		render_clip_x1 = 0;
		render_clip_y1 = 0;
		render_clip_x2 = ctx->win_w;
		render_clip_y2 = ctx->win_h;
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, 0, 0, ctx->win_w, ctx->win_h, ctx->theme.bg);
		for(uint32_t i = 0; i < ctx->widget_count; ++i) {
			if(!widget_visible(ctx, i)) {
				continue;
			}
			set_parent_clip(ctx, i);
			render_widget(ctx, i);
		}
	} else {
		int32_t bx, by, bw, bh;
		dirty_bounds(ctx, &bx, &by, &bw, &bh);
		render_base_clip_x1 = bx;
		render_base_clip_y1 = by;
		render_base_clip_x2 = bx + bw;
		render_base_clip_y2 = by + bh;
		render_clip_x1 = bx;
		render_clip_y1 = by;
		render_clip_x2 = bx + bw;
		render_clip_y2 = by + bh;
		for(uint32_t d = 0; d < ctx->dirty_count; ++d) {
			draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h,
				ctx->dirty_rects[d].x, ctx->dirty_rects[d].y,
				ctx->dirty_rects[d].w, ctx->dirty_rects[d].h, ctx->theme.bg);
		}
		for(uint32_t i = 0; i < ctx->widget_count; ++i) {
			if(!widget_visible(ctx, i)) {
				continue;
			}
			if(dirty_intersects(ctx, ctx->rects[i].x, ctx->rects[i].y, ctx->rects[i].w, ctx->rects[i].h)) {
				set_parent_clip(ctx, i);
				render_widget(ctx, i);
			}
		}
	}

	render_clip_x1 = 0;
	render_clip_y1 = 0;
	render_clip_x2 = ctx->win_w;
	render_clip_y2 = ctx->win_h;
}

// ---------------------------------------------------------------------------
// Init auxiliary data
// ---------------------------------------------------------------------------

// [=]===^=[ init_aux_data ]=====================================[=]
static void init_aux_data(struct mkgui_ctx *ctx) {
	for(uint32_t i = 0; i < ctx->widget_count; ++i) {
		struct mkgui_widget *w = &ctx->widgets[i];
		switch(w->type) {
			case MKGUI_INPUT: {
				if(ctx->input_count < 64) {
					struct mkgui_input_data *inp = &ctx->inputs[ctx->input_count++];
					memset(inp, 0, sizeof(*inp));
					inp->widget_id = w->id;
				}
			} break;

			case MKGUI_DROPDOWN: {
				if(ctx->dropdown_count < 32) {
					struct mkgui_dropdown_data *dd = &ctx->dropdowns[ctx->dropdown_count++];
					memset(dd, 0, sizeof(*dd));
					dd->widget_id = w->id;
					dd->selected = -1;
				}
			} break;

			case MKGUI_SLIDER: {
				if(ctx->slider_count < 32) {
					struct mkgui_slider_data *sd = &ctx->sliders[ctx->slider_count++];
					memset(sd, 0, sizeof(*sd));
					sd->widget_id = w->id;
					sd->min_val = 0;
					sd->max_val = 100;
					sd->value = 50;
				}
			} break;

			case MKGUI_TABS: {
				if(ctx->tab_count < 32) {
					struct mkgui_tabs_data *td = &ctx->tabs[ctx->tab_count++];
					td->widget_id = w->id;
					td->active_tab = 0;
					for(uint32_t j = 0; j < ctx->widget_count; ++j) {
						if(ctx->widgets[j].type == MKGUI_TAB && ctx->widgets[j].parent_id == w->id) {
							td->active_tab = ctx->widgets[j].id;
							break;
						}
					}
				}
			} break;

			case MKGUI_VBOX:
			case MKGUI_HBOX: {
				if((w->flags & MKGUI_SCROLL) && ctx->box_scroll_count < 32) {
					struct mkgui_box_scroll *bs = &ctx->box_scrolls[ctx->box_scroll_count++];
					bs->widget_id = w->id;
					bs->scroll_y = 0;
					bs->content_h = 0;
				}
			} break;

			case MKGUI_HSPLIT:
			case MKGUI_VSPLIT: {
				if(ctx->split_count < 32) {
					struct mkgui_split_data *sd = &ctx->splits[ctx->split_count++];
					sd->widget_id = w->id;
					sd->ratio = 0.5f;
				}
			} break;

			case MKGUI_TREEVIEW: {
				if(ctx->treeview_count < 8) {
					struct mkgui_treeview_data *tv = &ctx->treeviews[ctx->treeview_count++];
					memset(tv, 0, sizeof(*tv));
					tv->widget_id = w->id;
					tv->selected_node = -1;
				}
			} break;

			case MKGUI_STATUSBAR: {
				if(ctx->statusbar_count < 8) {
					struct mkgui_statusbar_data *sb = &ctx->statusbars[ctx->statusbar_count++];
					memset(sb, 0, sizeof(*sb));
					sb->widget_id = w->id;
				}
			} break;

			case MKGUI_SPINBOX: {
				if(ctx->spinbox_count < 32) {
					struct mkgui_spinbox_data *sd = &ctx->spinboxes[ctx->spinbox_count++];
					memset(sd, 0, sizeof(*sd));
					sd->widget_id = w->id;
					sd->min_val = 0;
					sd->max_val = 100;
					sd->value = 0;
					sd->step = 1;
				}
			} break;

			case MKGUI_PROGRESS: {
				if(ctx->progress_count < 32) {
					struct mkgui_progress_data *pd = &ctx->progress[ctx->progress_count++];
					memset(pd, 0, sizeof(*pd));
					pd->widget_id = w->id;
					pd->max_val = 100;
				}
			} break;

			case MKGUI_TEXTAREA: {
				if(ctx->textarea_count < 16) {
					struct mkgui_textarea_data *ta = &ctx->textareas[ctx->textarea_count++];
					memset(ta, 0, sizeof(*ta));
					ta->widget_id = w->id;
					ta->text = (char *)calloc(1, MKGUI_TEXTAREA_INIT_CAP);
					ta->text_cap = MKGUI_TEXTAREA_INIT_CAP;
				}
			} break;

			case MKGUI_ITEMVIEW: {
				if(ctx->itemview_count < 16) {
					struct mkgui_itemview_data *iv = &ctx->itemviews[ctx->itemview_count++];
					memset(iv, 0, sizeof(*iv));
					iv->widget_id = w->id;
					iv->selected = -1;
				}
			} break;

			case MKGUI_CANVAS: {
				if(ctx->canvas_count < 16) {
					struct mkgui_canvas_data *cd = &ctx->canvases[ctx->canvas_count++];
					memset(cd, 0, sizeof(*cd));
					cd->widget_id = w->id;
				}
			} break;

			default: {
			} break;
		}
	}
	for(uint32_t i = 0; i < ctx->widget_count; ++i) {
		struct mkgui_widget *w = &ctx->widgets[i];
		if(w->type == MKGUI_BUTTON && w->label[0] != '\0') {
			struct mkgui_widget *parent = find_widget(ctx, w->parent_id);
			if(parent && parent->type == MKGUI_TOOLBAR) {
				snprintf(ctx->tooltip_texts[i], sizeof(ctx->tooltip_texts[i]), "%s", w->label);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// [=]===^=[ mkgui_create ]======================================[=]
static struct mkgui_ctx *mkgui_create(struct mkgui_widget *widgets, uint32_t count) {
	struct mkgui_ctx *ctx = (struct mkgui_ctx *)calloc(1, sizeof(struct mkgui_ctx));

	if(count > MKGUI_MAX_WIDGETS) {
		count = MKGUI_MAX_WIDGETS;
	}
	memcpy(ctx->widgets, widgets, count * sizeof(struct mkgui_widget));
	ctx->widget_count = count;

	int32_t init_w = 800, init_h = 600;
	const char *title = "mkgui";
	for(uint32_t i = 0; i < count; ++i) {
		if(widgets[i].type == MKGUI_WINDOW) {
			if(widgets[i].w > 0) {
				init_w = widgets[i].w;
			}
			if(widgets[i].h > 0) {
				init_h = widgets[i].h;
			}
			if(widgets[i].label[0]) {
				title = ctx->widgets[i].label;
			}
			break;
		}
	}

	if(!platform_init(ctx, title, init_w, init_h)) {
		free(ctx);
		return NULL;
	}

	render_clip_x1 = 0;
	render_clip_y1 = 0;
	render_clip_x2 = ctx->win_w;
	render_clip_y2 = ctx->win_h;

	platform_font_init(ctx);
	ctx->theme = default_theme();
	icon_text_color = ctx->theme.text & 0x00ffffff;
	mkgui_icon_init();
	icon_load_from_widgets(ctx);
	dirty_all(ctx);
#ifdef _WIN32
	QueryPerformanceFrequency((LARGE_INTEGER *)&ctx->perf_freq);
	QueryPerformanceCounter((LARGE_INTEGER *)&ctx->anim_prev);
#else
	clock_gettime(CLOCK_MONOTONIC, &ctx->anim_prev);
#endif
	ctx->anim_time = 0.0;
	ctx->poll_timeout_ms = -1;

	init_aux_data(ctx);
	window_register(ctx);

	return ctx;
}

// [=]===^=[ mkgui_set_theme ]====================================[=]
static void mkgui_set_theme(struct mkgui_ctx *ctx, struct mkgui_theme theme) {
	ctx->theme = theme;
	icon_text_color = theme.text & 0x00ffffff;
	icon_reload_all();
	dirty_all(ctx);
}

// [=]===^=[ mkgui_destroy ]=====================================[=]
static void mkgui_destroy(struct mkgui_ctx *ctx) {
	if(!ctx) {
		return;
	}
	window_unregister(ctx);
	popup_destroy_all(ctx);
	for(uint32_t i = 0; i < ctx->treeview_count; ++i) {
		free(ctx->treeviews[i].nodes);
	}
	for(uint32_t i = 0; i < ctx->textarea_count; ++i) {
		free(ctx->textareas[i].text);
	}
	for(uint32_t i = 0; i < ctx->itemview_count; ++i) {
		free(ctx->itemviews[i].thumb_buf);
	}
	for(uint32_t i = 0; i < ctx->image_count; ++i) {
		free(ctx->images[i].pixels);
	}
	for(uint32_t i = 0; i < ctx->glview_count; ++i) {
		if(ctx->glviews[i].created) {
			platform_glview_destroy(ctx, &ctx->glviews[i]);
		}
	}
	platform_font_fini(ctx);
	platform_destroy(ctx);
	mdi_dat_free();
	free(ctx);
}

// [=]===^=[ mkgui_create_child ]=================================[=]
static struct mkgui_ctx *mkgui_create_child(struct mkgui_ctx *parent, struct mkgui_widget *widgets, uint32_t count, const char *title, int32_t w, int32_t h) {
	struct mkgui_ctx *ctx = (struct mkgui_ctx *)calloc(1, sizeof(struct mkgui_ctx));

	if(count > MKGUI_MAX_WIDGETS) {
		count = MKGUI_MAX_WIDGETS;
	}
	memcpy(ctx->widgets, widgets, count * sizeof(struct mkgui_widget));
	ctx->widget_count = count;

	if(!platform_init_child(ctx, parent, title, w, h)) {
		free(ctx);
		return NULL;
	}

	render_clip_x1 = 0;
	render_clip_y1 = 0;
	render_clip_x2 = ctx->win_w;
	render_clip_y2 = ctx->win_h;

	memcpy(ctx->glyphs, parent->glyphs, sizeof(ctx->glyphs));
	ctx->font_ascent = parent->font_ascent;
	ctx->font_height = parent->font_height;
	ctx->char_width = parent->char_width;

	icon_load_from_widgets(ctx);

	ctx->theme = parent->theme;
	ctx->parent = parent;
	dirty_all(ctx);
#ifdef _WIN32
	ctx->perf_freq = parent->perf_freq;
	QueryPerformanceCounter((LARGE_INTEGER *)&ctx->anim_prev);
#else
	clock_gettime(CLOCK_MONOTONIC, &ctx->anim_prev);
#endif
	ctx->anim_time = 0.0;
	ctx->poll_timeout_ms = -1;

	init_aux_data(ctx);
	window_register(ctx);

	return ctx;
}

// [=]===^=[ mkgui_destroy_child ]================================[=]
static void mkgui_destroy_child(struct mkgui_ctx *ctx) {
	if(!ctx) {
		return;
	}
	window_unregister(ctx);
	popup_destroy_all(ctx);
	for(uint32_t i = 0; i < ctx->treeview_count; ++i) {
		free(ctx->treeviews[i].nodes);
	}
	for(uint32_t i = 0; i < ctx->textarea_count; ++i) {
		free(ctx->textareas[i].text);
	}
	for(uint32_t i = 0; i < ctx->itemview_count; ++i) {
		free(ctx->itemviews[i].thumb_buf);
	}
	platform_destroy(ctx);
	free(ctx);
}

// ---------------------------------------------------------------------------
// Event processing
// ---------------------------------------------------------------------------

// [=]===^=[ mkgui_poll ]========================================[=]
static uint32_t mkgui_poll(struct mkgui_ctx *ctx, struct mkgui_event *ev) {
	ev->type = MKGUI_EVENT_NONE;
	ev->id = 0;
	ev->value = 0;
	ev->col = 0;
	ev->keysym = 0;
	ev->keymod = 0;

#ifdef _WIN32
	int64_t now;
	QueryPerformanceCounter((LARGE_INTEGER *)&now);
	double dt = (double)(now - ctx->anim_prev) / (double)ctx->perf_freq;
	ctx->anim_prev = now;
#else
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	double dt = (double)(now.tv_sec - ctx->anim_prev.tv_sec) + (double)(now.tv_nsec - ctx->anim_prev.tv_nsec) * 1e-9;
	ctx->anim_prev = now;
#endif
	ctx->anim_time += dt;

	if(ctx->focus_id != ctx->prev_focus_id) {
		if(ctx->prev_focus_id) {
			ev->type = MKGUI_EVENT_UNFOCUS;
			ev->id = ctx->prev_focus_id;
			ctx->prev_focus_id = 0;
			return 1;
		}
		ev->type = MKGUI_EVENT_FOCUS;
		ev->id = ctx->focus_id;
		ctx->prev_focus_id = ctx->focus_id;
		return 1;
	}

	if(ctx->hover_id != ctx->prev_hover_id) {
		if(ctx->prev_hover_id) {
			ev->type = MKGUI_EVENT_HOVER_LEAVE;
			ev->id = ctx->prev_hover_id;
			ctx->prev_hover_id = 0;
			return 1;
		}
		ev->type = MKGUI_EVENT_HOVER_ENTER;
		ev->id = ctx->hover_id;
		ctx->prev_hover_id = ctx->hover_id;
		return 1;
	}

	for(uint32_t si = 0; si < ctx->spinbox_count; ++si) {
		struct mkgui_spinbox_data *sd = &ctx->spinboxes[si];
		if(sd->repeat_dir && ctx->press_id) {
			uint32_t now_ms = mkgui_time_ms();
			if(now_ms >= sd->repeat_next_ms) {
				int32_t delta = (sd->repeat_dir > 0) ? sd->step : -sd->step;
				if(spinbox_adjust(ctx, ev, sd->widget_id, delta)) {
					sd->repeat_next_ms = now_ms + 80;
					return 1;
				}
			}
		}
	}

	ctx->anim_active = 0;
	for(uint32_t i = 0; i < ctx->widget_count; ++i) {
		struct mkgui_widget *w = &ctx->widgets[i];
		if(!widget_visible(ctx, i)) {
			continue;
		}
		if(w->type == MKGUI_SPINNER || w->type == MKGUI_GLVIEW) {
			ctx->anim_active = 1;
		}
		if(w->type == MKGUI_PROGRESS) {
			struct mkgui_progress_data *pd = find_progress_data(ctx, w->id);
			if(pd && pd->value > 0 && pd->value < pd->max_val) {
				ctx->anim_active = 1;
			}
		}
	}
	for(uint32_t si = 0; si < ctx->spinbox_count; ++si) {
		if(ctx->spinboxes[si].repeat_dir) {
			ctx->anim_active = 1;
		}
	}

	while(platform_pending(ctx)) {
		struct mkgui_plat_event pev;
		platform_next_event(ctx, &pev);

		int32_t popup_idx = pev.popup_idx;

		switch(pev.type) {
			case MKGUI_PLAT_EXPOSE: {
				dirty_all(ctx);
			} break;

			case MKGUI_PLAT_RESIZE: {
				if(pev.width != ctx->win_w || pev.height != ctx->win_h) {
					ctx->win_w = pev.width;
					ctx->win_h = pev.height;
					platform_fb_resize(ctx);
					dirty_all(ctx);
					ev->type = MKGUI_EVENT_RESIZE;
				}
			} break;

			case MKGUI_PLAT_MOTION: {
				if(popup_idx >= 0) {
					struct mkgui_popup *mp = &ctx->popups[popup_idx];
					struct mkgui_widget *mpw = find_widget(ctx, mp->widget_id);

					if(mpw && mpw->type == MKGUI_MENUITEM) {
						int32_t hit_idx;
						struct mkgui_widget *hovered = menu_popup_hit_item(ctx, mpw->id, pev.y, &hit_idx);
						if(hit_idx != mp->hover_item) {
							mp->dirty = 1;
						}
						mp->hover_item = hit_idx;
						if(hovered && !(hovered->flags & MKGUI_SEPARATOR) && menu_item_has_children(ctx, hovered->id)) {
							uint32_t already_open = 0;
							if((uint32_t)popup_idx + 1 < ctx->popup_count) {
								if(ctx->popups[popup_idx + 1].widget_id == hovered->id) {
									already_open = 1;
								}
							}
							if(!already_open) {
								menu_open_submenu(ctx, (uint32_t)popup_idx, hovered->id);
							}

						} else {
							popup_destroy_from(ctx, (uint32_t)popup_idx + 1);
						}

					} else {
						int32_t scroll_off = 0;
						if(mpw && mpw->type == MKGUI_DROPDOWN) {
							struct mkgui_dropdown_data *mdd = find_dropdown_data(ctx, mpw->id);
							if(mdd) {
								scroll_off = mdd->scroll_y;
							}
						}
						int32_t new_item = (pev.y - 1 + scroll_off) / MKGUI_ROW_HEIGHT;
						if(new_item != mp->hover_item) {
							mp->dirty = 1;
						}
						mp->hover_item = new_item;
					}

					dirty_all(ctx);
					break;
				}

				ctx->mouse_x = pev.x;
				ctx->mouse_y = pev.y;

				if(ctx->drag_select_id) {
					int32_t dsi = find_widget_idx(ctx, ctx->drag_select_id);
					if(dsi >= 0 && ctx->widgets[dsi].type == MKGUI_INPUT) {
						struct mkgui_input_data *inp = find_input_data(ctx, ctx->drag_select_id);
						if(inp) {
							const char *display = inp->text;
							char masked_buf[MKGUI_MAX_TEXT];
							if(ctx->widgets[dsi].flags & MKGUI_PASSWORD) {
								uint32_t mlen = (uint32_t)strlen(inp->text);
								for(uint32_t mi = 0; mi < mlen && mi < MKGUI_MAX_TEXT - 1; ++mi) {
									masked_buf[mi] = '*';
								}
								masked_buf[mlen < MKGUI_MAX_TEXT - 1 ? mlen : MKGUI_MAX_TEXT - 1] = '\0';
								display = masked_buf;
							}
							inp->cursor = input_hit_cursor(ctx, inp, display, ctx->rects[dsi].x, ctx->mouse_x);
							inp->sel_end = inp->cursor;
							dirty_all(ctx);
						}

					} else if(dsi >= 0 && ctx->widgets[dsi].type == MKGUI_TEXTAREA) {
						struct mkgui_textarea_data *ta = find_textarea_data(ctx, ctx->drag_select_id);
						if(ta) {
							int32_t ry = ctx->rects[dsi].y;
							int32_t rh = ctx->rects[dsi].h;
							if(ctx->mouse_y < ry + 1) {
								ta->scroll_y -= MKGUI_ROW_HEIGHT;
								if(ta->scroll_y < 0) {
									ta->scroll_y = 0;
								}

							} else if(ctx->mouse_y > ry + rh - 1) {
								int32_t content_h = (int32_t)textarea_line_count(ta) * MKGUI_ROW_HEIGHT;
								int32_t view_h = rh - 2;
								int32_t max_scroll = content_h - view_h;
								if(max_scroll > 0) {
									ta->scroll_y += MKGUI_ROW_HEIGHT;
									if(ta->scroll_y > max_scroll) {
										ta->scroll_y = max_scroll;
									}
								}
							}
							ta->cursor = textarea_hit_pos(ctx, ta, ctx->rects[dsi].x, ctx->rects[dsi].y, ctx->rects[dsi].h, ctx->mouse_x, ctx->mouse_y);
							ta->sel_end = ta->cursor;
							dirty_all(ctx);
						}
					}
					break;
				}

				if(ctx->drag_scrollbar_id) {
					int32_t dsi = find_widget_idx(ctx, ctx->drag_scrollbar_id);
					if(dsi >= 0 && ctx->widgets[dsi].type == MKGUI_TEXTAREA) {
						struct mkgui_textarea_data *ta = find_textarea_data(ctx, ctx->drag_scrollbar_id);
						if(ta) {
							textarea_scroll_drag(ctx, (uint32_t)dsi, ta, ctx->mouse_y, ctx->drag_scrollbar_offset);
						}
					} else if(dsi >= 0 && ctx->widgets[dsi].type == MKGUI_SCROLLBAR) {
						scrollbar_drag_to(ctx, ctx->drag_scrollbar_id, ctx->mouse_x, ctx->mouse_y);
					} else if(dsi >= 0 && ctx->widgets[dsi].type == MKGUI_ITEMVIEW) {
						itemview_scroll_to_mouse(ctx, ctx->drag_scrollbar_id, ctx->mouse_x, ctx->mouse_y);
					} else if(dsi >= 0 && (ctx->widgets[dsi].type == MKGUI_VBOX || ctx->widgets[dsi].type == MKGUI_HBOX)) {
						struct mkgui_box_scroll *bs = find_box_scroll(ctx, ctx->drag_scrollbar_id);
						if(bs) {
							if(ctx->widgets[dsi].type == MKGUI_HBOX && bs->content_w > ctx->rects[dsi].w) {
								int32_t sw = ctx->rects[dsi].w;
								int32_t sx = ctx->rects[dsi].x;
								int32_t track = sw;
								int32_t total = bs->content_w;
								if(total < 1) { total = 1; }
								int32_t thumb = track * sw / total;
								if(thumb < 20) { thumb = 20; }
								if(thumb > track) { thumb = track; }
								int32_t max_scroll = bs->content_w - sw;
								if(max_scroll < 1) { max_scroll = 1; }
								int32_t new_pos = ctx->mouse_x - sx - ctx->drag_scrollbar_offset;
								int32_t scrollable_range = track - thumb;
								if(scrollable_range < 1) { scrollable_range = 1; }
								bs->scroll_x = (int32_t)((int64_t)new_pos * max_scroll / scrollable_range);
								if(bs->scroll_x < 0) { bs->scroll_x = 0; }
								if(bs->scroll_x > max_scroll) { bs->scroll_x = max_scroll; }
							} else {
								int32_t sh = ctx->rects[dsi].h;
								int32_t sy = ctx->rects[dsi].y;
								int32_t track = sh;
								int32_t total = bs->content_h;
								if(total < 1) { total = 1; }
								int32_t thumb = track * sh / total;
								if(thumb < 20) { thumb = 20; }
								if(thumb > track) { thumb = track; }
								int32_t max_scroll = bs->content_h - sh;
								if(max_scroll < 1) { max_scroll = 1; }
								int32_t new_pos = ctx->mouse_y - sy - ctx->drag_scrollbar_offset;
								int32_t scrollable_range = track - thumb;
								if(scrollable_range < 1) { scrollable_range = 1; }
								bs->scroll_y = (int32_t)((int64_t)new_pos * max_scroll / scrollable_range);
								if(bs->scroll_y < 0) { bs->scroll_y = 0; }
								if(bs->scroll_y > max_scroll) { bs->scroll_y = max_scroll; }
							}
							dirty_all(ctx);
						}
					} else {
						listview_scroll_to_y(ctx, ctx->drag_scrollbar_id, ctx->mouse_y);
					}
					break;
				}

				if(ctx->drag_col_resize_id) {
					struct mkgui_listview_data *lv = find_listv_data(ctx, ctx->drag_col_resize_id);
					if(lv) {
						int32_t nw = ctx->drag_col_resize_start_w + (ctx->mouse_x - ctx->drag_col_resize_start_x);
						if(nw < 20) {
							nw = 20;
						}
						lv->columns[ctx->drag_col_resize_col].width = nw;
						dirty_all(ctx);
					}
					break;
				}

				if(ctx->drag_col_id) {
					int32_t dx = ctx->mouse_x - ctx->drag_col_start_x;
					if(dx < -3 || dx > 3) {
						dirty_all(ctx);
					}
					break;
				}

				if(ctx->press_id) {
					struct mkgui_widget *pw = find_widget(ctx, ctx->press_id);

					if(pw && (pw->type == MKGUI_HSPLIT || pw->type == MKGUI_VSPLIT)) {
						int32_t pi = find_widget_idx(ctx, pw->id);
						if(pi >= 0) {
							struct mkgui_split_data *sd = find_split_data(ctx, pw->id);
							if(sd) {
								if(pw->type == MKGUI_HSPLIT) {
									int32_t rh = ctx->rects[pi].h;
									if(rh > 0) {
										sd->ratio = (float)(ctx->mouse_y - ctx->rects[pi].y) / (float)rh;
									}

								} else {
									int32_t rw = ctx->rects[pi].w;
									if(rw > 0) {
										sd->ratio = (float)(ctx->mouse_x - ctx->rects[pi].x) / (float)rw;
									}
								}
								if(sd->ratio < 0.05f) {
									sd->ratio = 0.05f;
								}
								if(sd->ratio > 0.95f) {
									sd->ratio = 0.95f;
								}
								dirty_all(ctx);
							}
						}
					}

					if(pw && pw->type == MKGUI_SLIDER) {
						struct mkgui_slider_data *sd = find_slider_data(ctx, pw->id);
						int32_t pi = find_widget_idx(ctx, pw->id);
						if(sd && pi >= 0) {
							int32_t rx = ctx->rects[pi].x;
							int32_t rw = ctx->rects[pi].w;
							int32_t range = sd->max_val - sd->min_val;
							int32_t new_val = sd->min_val + (int32_t)((int64_t)(ctx->mouse_x - rx) * range / (rw > 0 ? rw : 1));
							if(new_val < sd->min_val) {
								new_val = sd->min_val;
							}
							if(new_val > sd->max_val) {
								new_val = sd->max_val;
							}
							if(new_val != sd->value) {
								sd->value = new_val;
								dirty_widget(ctx, (uint32_t)pi);
								ev->type = MKGUI_EVENT_SLIDER_CHANGED;
								ev->id = pw->id;
								ev->value = new_val;
								return 1;
							}
						}
					}
				}

				int32_t hi = hit_test(ctx, ctx->mouse_x, ctx->mouse_y);
				uint32_t new_hover = (hi >= 0) ? ctx->widgets[hi].id : 0;
				if(new_hover != ctx->hover_id) {
					if(ctx->hover_id) {
						dirty_widget_id(ctx, ctx->hover_id);
					}
					ctx->hover_id = new_hover;
					if(ctx->hover_id) {
						dirty_widget_id(ctx, ctx->hover_id);
					}
				}
				if(hi >= 0 && ctx->widgets[hi].type == MKGUI_TABS) {
					struct mkgui_tabs_data *td = find_tabs_data(ctx, ctx->widgets[hi].id);
					if(td) {
						uint32_t ht = tab_hit_test(ctx, (uint32_t)hi, ctx->mouse_x, ctx->mouse_y);
						if(ht != td->hover_tab) {
							td->hover_tab = ht;
							dirty_widget(ctx, (uint32_t)hi);
						}
					}
				}
				for(uint32_t tvi = 0; tvi < ctx->treeview_count; ++tvi) {
					struct mkgui_treeview_data *tv = &ctx->treeviews[tvi];
					if(tv->drag_source >= 0 && ctx->press_id == tv->widget_id) {
						int32_t dy = ctx->mouse_y - tv->drag_start_y;
						if(dy < 0) {
							dy = -dy;
						}
						if(!tv->drag_active && dy > 4) {
							tv->drag_active = 1;
							ev->type = MKGUI_EVENT_DRAG_START;
							ev->id = tv->widget_id;
							ev->value = (int32_t)tv->nodes[tv->drag_source].id;
							dirty_all(ctx);
							return 1;
						}
						if(tv->drag_active) {
							int32_t tidx = find_widget_idx(ctx, tv->widget_id);
							if(tidx >= 0) {
								int32_t node_idx = treeview_row_hit(ctx, (uint32_t)tidx, ctx->mouse_y);
								if(node_idx >= 0 && node_idx != tv->drag_source) {
									int32_t ry = ctx->rects[tidx].y;
									int32_t local_y = ctx->mouse_y - ry - 1 + tv->scroll_y;
									int32_t row_local = local_y % MKGUI_ROW_HEIGHT;
									int32_t third = MKGUI_ROW_HEIGHT / 3;
									if(row_local < third) {
										tv->drag_pos = 0;
									} else if(row_local > MKGUI_ROW_HEIGHT - third) {
										tv->drag_pos = 1;
									} else {
										tv->drag_pos = 2;
									}
									tv->drag_target = node_idx;
								} else {
									tv->drag_target = -1;
								}
								dirty_all(ctx);
							}
						}
					}
				}
				for(uint32_t lvi = 0; lvi < ctx->listv_count; ++lvi) {
					struct mkgui_listview_data *lv = &ctx->listvs[lvi];
					if(lv->drag_source >= 0 && ctx->press_id == lv->widget_id) {
						int32_t dy = ctx->mouse_y - lv->drag_start_y;
						if(dy < 0) {
							dy = -dy;
						}
						if(!lv->drag_active && dy > 4) {
							lv->drag_active = 1;
							ev->type = MKGUI_EVENT_DRAG_START;
							ev->id = lv->widget_id;
							ev->value = lv->drag_source;
							dirty_all(ctx);
							return 1;
						}
						if(lv->drag_active) {
							int32_t lidx = find_widget_idx(ctx, lv->widget_id);
							if(lidx >= 0) {
								int32_t tgt = listview_row_hit(ctx, (uint32_t)lidx, ctx->mouse_x, ctx->mouse_y);
								if(tgt >= 0 && tgt != lv->drag_source) {
									lv->drag_target = tgt;
								} else {
									lv->drag_target = -1;
								}
								dirty_all(ctx);
							}
						}
					}
				}

				if(hi >= 0 && ctx->widgets[hi].type == MKGUI_SPINBOX) {
					struct mkgui_spinbox_data *sd = find_spinbox_data(ctx, ctx->widgets[hi].id);
					if(sd) {
						int32_t hb = spinbox_btn_hit(ctx, (uint32_t)hi, ctx->mouse_x, ctx->mouse_y);
						if(hb != sd->hover_btn) {
							sd->hover_btn = hb;
							dirty_all(ctx);
						}
					}
				}

				uint32_t want_resize_cursor = 0;
				if(hi >= 0 && ctx->widgets[hi].type == MKGUI_LISTVIEW) {
					if(listview_divider_hit(ctx, (uint32_t)hi, ctx->mouse_x, ctx->mouse_y) >= 0) {
						want_resize_cursor = 1;
					}
				}
				platform_set_cursor(ctx, want_resize_cursor);

				if(ctx->popup_count > 0 && hi >= 0 && ctx->widgets[hi].type == MKGUI_MENUITEM) {
					struct mkgui_widget *mw = &ctx->widgets[hi];
					struct mkgui_widget *mp = find_widget(ctx, mw->parent_id);
					if(mp && mp->type == MKGUI_MENU && ctx->popups[0].widget_id != mw->id) {
						menu_open_popup(ctx, mw->id);
					}
				}

				tooltip_update(ctx, new_hover, ctx->mouse_x, ctx->mouse_y);
			} break;

			case MKGUI_PLAT_BUTTON_PRESS: {
				if(popup_idx >= 0) {
					struct mkgui_popup *p = &ctx->popups[popup_idx];
					struct mkgui_widget *pw = find_widget(ctx, p->widget_id);

					if((pev.button == 4 || pev.button == 5) && pw && pw->type == MKGUI_DROPDOWN) {
						struct mkgui_dropdown_data *dd = find_dropdown_data(ctx, pw->id);
						if(dd) {
							int32_t delta = (pev.button == 4) ? -MKGUI_ROW_HEIGHT * 3 : MKGUI_ROW_HEIGHT * 3;
							dd->scroll_y += delta;
							dropdown_clamp_scroll(dd, p->h);
							p->hover_item = (pev.y - 1 + dd->scroll_y) / MKGUI_ROW_HEIGHT;
							p->dirty = 1;
						}
						break;
					}

					if(pw && pw->type == MKGUI_DROPDOWN) {
						int32_t scroll_off = 0;
						struct mkgui_dropdown_data *sdd = find_dropdown_data(ctx, pw->id);
						if(sdd) {
							scroll_off = sdd->scroll_y;
						}
						int32_t item = (pev.y - 1 + scroll_off) / MKGUI_ROW_HEIGHT;
						struct mkgui_dropdown_data *dd = find_dropdown_data(ctx, pw->id);
						if(dd && item >= 0 && item < (int32_t)dd->item_count) {
							dd->selected = item;
							ev->type = MKGUI_EVENT_DROPDOWN_CHANGED;
							ev->id = pw->id;
							ev->value = item;
							popup_destroy(ctx, (uint32_t)popup_idx);
							dirty_all(ctx);
							return 1;
						}

					} else if(pw && pw->type == MKGUI_MENUITEM) {
						int32_t hit_idx;
						struct mkgui_widget *clicked = menu_popup_hit_item(ctx, pw->id, pev.y, &hit_idx);
						if(clicked && !(clicked->flags & MKGUI_SEPARATOR)) {
							if(menu_item_has_children(ctx, clicked->id)) {
								menu_open_submenu(ctx, (uint32_t)popup_idx, clicked->id);

							} else {
								if(clicked->flags & MKGUI_MENU_CHECK) {
									clicked->flags ^= MKGUI_CHECKED;

								} else if(clicked->flags & MKGUI_MENU_RADIO) {
									for(uint32_t ri = 0; ri < ctx->widget_count; ++ri) {
										struct mkgui_widget *rw = &ctx->widgets[ri];
										if(rw->type == MKGUI_MENUITEM && rw->parent_id == clicked->parent_id && (rw->flags & MKGUI_MENU_RADIO)) {
											rw->flags &= ~MKGUI_CHECKED;
										}
									}
									clicked->flags |= MKGUI_CHECKED;
								}
								ev->type = MKGUI_EVENT_MENU;
								ev->id = clicked->id;
								ev->value = (clicked->flags & MKGUI_CHECKED) ? 1 : 0;
								popup_destroy_all(ctx);
								dirty_all(ctx);
								return 1;
							}
						}
					}

					popup_destroy(ctx, (uint32_t)popup_idx);
					dirty_all(ctx);
					break;
				}

				uint32_t closed_dropdown_id = 0;
				if(ctx->popup_count > 0) {
					for(uint32_t pi = 0; pi < ctx->popup_count; ++pi) {
						struct mkgui_widget *pw = find_widget(ctx, ctx->popups[pi].widget_id);
						if(pw && pw->type == MKGUI_DROPDOWN) {
							closed_dropdown_id = ctx->popups[pi].widget_id;
						}
					}
					popup_destroy_all(ctx);
					dirty_all(ctx);
				}

				ctx->mouse_x = pev.x;
				ctx->mouse_y = pev.y;
				ctx->mouse_btn = pev.button;

				if(pev.button == 3) {
					int32_t hi = hit_test(ctx, ctx->mouse_x, ctx->mouse_y);
					if(hi >= 0) {
						ev->type = MKGUI_EVENT_CONTEXT;
						ev->id = ctx->widgets[hi].id;
						ev->value = ctx->mouse_x;
						ev->col = ctx->mouse_y;
						return 1;
					}
					break;
				}

				if(pev.button == 4 || pev.button == 5) {
					int32_t hi = hit_test(ctx, ctx->mouse_x, ctx->mouse_y);
					int32_t delta = (pev.button == 4) ? -MKGUI_ROW_HEIGHT * 3 : MKGUI_ROW_HEIGHT * 3;
					if(hi >= 0 && ctx->widgets[hi].type == MKGUI_LISTVIEW) {
						struct mkgui_listview_data *lv = find_listv_data(ctx, ctx->widgets[hi].id);
						if(lv) {
							lv->scroll_y += delta;
							int32_t max_scroll = (int32_t)lv->row_count * MKGUI_ROW_HEIGHT - (ctx->rects[hi].h - MKGUI_ROW_HEIGHT - 2);
							if(max_scroll < 0) {
								max_scroll = 0;
							}
							if(lv->scroll_y < 0) {
								lv->scroll_y = 0;
							}
							if(lv->scroll_y > max_scroll) {
								lv->scroll_y = max_scroll;
							}
							lv->scroll_y = (lv->scroll_y / MKGUI_ROW_HEIGHT) * MKGUI_ROW_HEIGHT;
							dirty_all(ctx);
						}

					} else if(hi >= 0 && ctx->widgets[hi].type == MKGUI_TREEVIEW) {
						struct mkgui_treeview_data *tv = find_treeview_data(ctx, ctx->widgets[hi].id);
						if(tv) {
							tv->scroll_y += delta;
							treeview_clamp_scroll(ctx, ctx->widgets[hi].id);
							dirty_all(ctx);
						}

					} else if(hi >= 0 && ctx->widgets[hi].type == MKGUI_SPINBOX) {
						struct mkgui_spinbox_data *sd = find_spinbox_data(ctx, ctx->widgets[hi].id);
						if(sd) {
							if(sd->editing) {
								sd->editing = 0;
							}
							int32_t step = (pev.button == 4) ? sd->step : -sd->step;
							if(spinbox_adjust(ctx, ev, ctx->widgets[hi].id, step)) {
								return 1;
							}
						}

					} else if(hi >= 0 && ctx->widgets[hi].type == MKGUI_SLIDER) {
						struct mkgui_slider_data *sd = find_slider_data(ctx, ctx->widgets[hi].id);
						if(sd) {
							int32_t range = sd->max_val - sd->min_val;
							int32_t step = range / 20;
							if(step < 1) {
								step = 1;
							}
							sd->value += (pev.button == 4) ? -step : step;
							if(sd->value < sd->min_val) {
								sd->value = sd->min_val;
							}
							if(sd->value > sd->max_val) {
								sd->value = sd->max_val;
							}
							dirty_all(ctx);
							ev->type = MKGUI_EVENT_SLIDER_CHANGED;
							ev->id = ctx->widgets[hi].id;
							ev->value = sd->value;
							return 1;
						}

					} else if(hi >= 0 && ctx->widgets[hi].type == MKGUI_TEXTAREA) {
						struct mkgui_textarea_data *ta = find_textarea_data(ctx, ctx->widgets[hi].id);
						if(ta) {
							ta->scroll_y += delta;
							int32_t content_h = (int32_t)textarea_line_count(ta) * MKGUI_ROW_HEIGHT;
							int32_t view_h = ctx->rects[hi].h - 2;
							int32_t max_scroll = content_h - view_h;
							if(max_scroll < 0) {
								max_scroll = 0;
							}
							if(ta->scroll_y < 0) {
								ta->scroll_y = 0;
							}
							if(ta->scroll_y > max_scroll) {
								ta->scroll_y = max_scroll;
							}
							dirty_all(ctx);
						}

					} else if(hi >= 0 && ctx->widgets[hi].type == MKGUI_ITEMVIEW) {
						struct mkgui_itemview_data *iv = find_itemview_data(ctx, ctx->widgets[hi].id);
						if(iv) {
							iv->scroll_y += delta;
							int32_t ca_x, ca_y, ca_w, ca_h;
							itemview_content_area(iv, ctx->rects[hi].x, ctx->rects[hi].y, ctx->rects[hi].w, ctx->rects[hi].h, &ca_x, &ca_y, &ca_w, &ca_h);
							itemview_clamp_scroll(iv, ca_w, ca_h);
							dirty_all(ctx);
						}

					} else if(hi >= 0 && ctx->widgets[hi].type == MKGUI_SCROLLBAR) {
						scrollbar_scroll(ctx, ctx->widgets[hi].id, (pev.button == 4) ? -1 : 1);
						ev->type = MKGUI_EVENT_SCROLL;
						ev->id = ctx->widgets[hi].id;
						struct mkgui_scrollbar_data *sbd = find_scrollbar_data(ctx, ctx->widgets[hi].id);
						ev->value = sbd ? sbd->value : 0;
						return 1;

					} else {
						uint32_t sid = hi >= 0 ? ctx->widgets[hi].parent_id : 0;
						if(!sid) {
							for(int32_t k = (int32_t)ctx->widget_count - 1; k >= 0; --k) {
								if((ctx->widgets[k].type == MKGUI_VBOX || ctx->widgets[k].type == MKGUI_HBOX) && (ctx->widgets[k].flags & MKGUI_SCROLL)) {
									int32_t rx = ctx->rects[k].x;
									int32_t ry = ctx->rects[k].y;
									if(ctx->mouse_x >= rx && ctx->mouse_x < rx + ctx->rects[k].w && ctx->mouse_y >= ry && ctx->mouse_y < ry + ctx->rects[k].h) {
										sid = ctx->widgets[k].id;
										break;
									}
								}
							}
						}
						while(sid) {
							int32_t si = find_widget_idx(ctx, sid);
							if(si < 0) {
								break;
							}
							if((ctx->widgets[si].type == MKGUI_VBOX || ctx->widgets[si].type == MKGUI_HBOX) && (ctx->widgets[si].flags & MKGUI_SCROLL)) {
								struct mkgui_box_scroll *bs = find_box_scroll(ctx, sid);
								if(bs) {
									if(ctx->widgets[si].type == MKGUI_HBOX && bs->content_w > ctx->rects[si].w) {
										int32_t view_w = ctx->rects[si].w;
										int32_t max_scroll = bs->content_w - view_w;
										if(max_scroll < 0) { max_scroll = 0; }
										bs->scroll_x += delta;
										if(bs->scroll_x < 0) { bs->scroll_x = 0; }
										if(bs->scroll_x > max_scroll) { bs->scroll_x = max_scroll; }
									} else {
										int32_t view_h = ctx->rects[si].h;
										int32_t max_scroll = bs->content_h - view_h;
										if(max_scroll < 0) { max_scroll = 0; }
										bs->scroll_y += delta;
										if(bs->scroll_y < 0) { bs->scroll_y = 0; }
										if(bs->scroll_y > max_scroll) { bs->scroll_y = max_scroll; }
									}
									dirty_all(ctx);
								}
								break;
							}
							sid = ctx->widgets[si].parent_id;
						}
					}
					break;
				}

				for(int32_t bi = (int32_t)ctx->widget_count - 1; bi >= 0; --bi) {
					struct mkgui_widget *bw = &ctx->widgets[bi];
					if((bw->type == MKGUI_VBOX || bw->type == MKGUI_HBOX) && (bw->flags & MKGUI_SCROLL)) {
						struct mkgui_box_scroll *bs = find_box_scroll(ctx, bw->id);
						if(!bs) {
							continue;
						}
						if(bw->type == MKGUI_HBOX && bs->content_w > ctx->rects[bi].w) {
							int32_t sy = ctx->rects[bi].y + ctx->rects[bi].h - MKGUI_SCROLLBAR_W;
							int32_t sx = ctx->rects[bi].x;
							int32_t sw = ctx->rects[bi].w;
							if(ctx->mouse_x >= sx && ctx->mouse_x < sx + sw && ctx->mouse_y >= sy && ctx->mouse_y < sy + MKGUI_SCROLLBAR_W) {
								int32_t track = sw;
								int32_t total = bs->content_w;
								if(total < 1) { total = 1; }
								int32_t thumb = track * sw / total;
								if(thumb < 20) { thumb = 20; }
								if(thumb > track) { thumb = track; }
								int32_t max_scroll = bs->content_w - sw;
								if(max_scroll < 1) { max_scroll = 1; }
								int32_t pos = (int32_t)((int64_t)bs->scroll_x * (track - thumb) / max_scroll);
								if(ctx->mouse_x >= sx + pos && ctx->mouse_x < sx + pos + thumb) {
									ctx->drag_scrollbar_id = bw->id;
									ctx->drag_scrollbar_offset = ctx->mouse_x - (sx + pos);
								}
								break;
							}
						}
						if(bs->content_h <= ctx->rects[bi].h) {
							continue;
						}
						int32_t sx = ctx->rects[bi].x + ctx->rects[bi].w - MKGUI_SCROLLBAR_W;
						int32_t sy = ctx->rects[bi].y;
						int32_t sh = ctx->rects[bi].h;
						if(ctx->mouse_x >= sx && ctx->mouse_x < sx + MKGUI_SCROLLBAR_W && ctx->mouse_y >= sy && ctx->mouse_y < sy + sh) {
							int32_t track = sh;
							int32_t total = bs->content_h;
							if(total < 1) { total = 1; }
							int32_t thumb = track * sh / total;
							if(thumb < 20) { thumb = 20; }
							if(thumb > track) { thumb = track; }
							int32_t max_scroll = bs->content_h - sh;
							if(max_scroll < 1) { max_scroll = 1; }
							int32_t pos = (int32_t)((int64_t)bs->scroll_y * (track - thumb) / max_scroll);
							if(pos < 0) { pos = 0; }
							if(pos > track - thumb) { pos = track - thumb; }
							int32_t thumb_y = sy + pos;
							if(ctx->mouse_y >= thumb_y && ctx->mouse_y < thumb_y + thumb) {
								ctx->drag_scrollbar_id = bw->id;
								ctx->drag_scrollbar_offset = ctx->mouse_y - thumb_y;
							} else {
								int32_t dir = ctx->mouse_y < thumb_y ? -1 : 1;
								int32_t step = sh / 2;
								bs->scroll_y += dir * step;
								if(bs->scroll_y < 0) { bs->scroll_y = 0; }
								if(bs->scroll_y > max_scroll) { bs->scroll_y = max_scroll; }
							}
							dirty_all(ctx);
							break;
						}
					}
				}
				if(ctx->drag_scrollbar_id) {
					break;
				}

				int32_t hi = hit_test(ctx, ctx->mouse_x, ctx->mouse_y);
				if(hi >= 0) {
					struct mkgui_widget *hw = &ctx->widgets[hi];

					if(hw->flags & MKGUI_DISABLED) {
						break;
					}

					if(is_focusable(hw)) {
						spinbox_focus_lost(ctx);
						ctx->focus_id = hw->id;

					} else {
						spinbox_focus_lost(ctx);
						ctx->focus_id = 0;
					}

					ctx->press_id = hw->id;
					ctx->press_mod = pev.keymod;
					dirty_all(ctx);

					if(hw->type == MKGUI_INPUT) {
						struct mkgui_input_data *inp = find_input_data(ctx, hw->id);
						if(inp) {
							const char *display = inp->text;
							char masked_buf[MKGUI_MAX_TEXT];
							if(hw->flags & MKGUI_PASSWORD) {
								uint32_t mlen = (uint32_t)strlen(inp->text);
								for(uint32_t mi = 0; mi < mlen && mi < MKGUI_MAX_TEXT - 1; ++mi) {
									masked_buf[mi] = '*';
								}
								masked_buf[mlen < MKGUI_MAX_TEXT - 1 ? mlen : MKGUI_MAX_TEXT - 1] = '\0';
								display = masked_buf;
							}
							inp->cursor = input_hit_cursor(ctx, inp, display, ctx->rects[hi].x, ctx->mouse_x);
							input_clear_selection(inp);
							ctx->drag_select_id = hw->id;
							dirty_all(ctx);
						}
					}

					if(hw->type == MKGUI_TEXTAREA) {
						struct mkgui_textarea_data *ta = find_textarea_data(ctx, hw->id);
						if(ta && textarea_has_scrollbar(ctx, (uint32_t)hi, ta)) {
							int32_t sb_off = textarea_sb_hit(ctx, (uint32_t)hi, ta, ctx->mouse_x, ctx->mouse_y);
							if(sb_off >= 0) {
								ctx->drag_scrollbar_id = hw->id;
								ctx->drag_scrollbar_offset = sb_off;
								break;
							}
						}
						if(ta) {
							ta->cursor = textarea_hit_pos(ctx, ta, ctx->rects[hi].x, ctx->rects[hi].y, ctx->rects[hi].h, ctx->mouse_x, ctx->mouse_y);
							textarea_clear_selection(ta);
							ctx->drag_select_id = hw->id;
							dirty_all(ctx);
						}
					}

					if(hw->type == MKGUI_TABS) {
						uint32_t close_id = tab_close_hit_test(ctx, (uint32_t)hi, ctx->mouse_x, ctx->mouse_y);
						if(close_id) {
							ev->type = MKGUI_EVENT_TAB_CLOSE;
							ev->id = hw->id;
							ev->value = (int32_t)close_id;
							return 1;
						}
						uint32_t tab_id = tab_hit_test(ctx, (uint32_t)hi, ctx->mouse_x, ctx->mouse_y);
						if(tab_id) {
							struct mkgui_tabs_data *td = find_tabs_data(ctx, hw->id);
							if(td && td->active_tab != tab_id) {
								td->active_tab = tab_id;
								dirty_all(ctx);
								ev->type = MKGUI_EVENT_TAB_CHANGED;
								ev->id = hw->id;
								ev->value = (int32_t)tab_id;
								return 1;
							}
						}
					}

					if(hw->type == MKGUI_LISTVIEW) {
						uint32_t sb_hit = listview_scrollbar_hit(ctx, (uint32_t)hi, ctx->mouse_x, ctx->mouse_y);
						if(sb_hit == 1) {
							ctx->drag_scrollbar_id = hw->id;
							ctx->drag_scrollbar_offset = listview_thumb_offset(ctx, (uint32_t)hi, ctx->mouse_y);

						} else if(sb_hit == 2 || sb_hit == 3) {
							listview_page_scroll(ctx, (uint32_t)hi, sb_hit == 2 ? -1 : 1);

						} else {
							int32_t div = listview_divider_hit(ctx, (uint32_t)hi, ctx->mouse_x, ctx->mouse_y);
							if(div >= 0) {
								struct mkgui_listview_data *lv = find_listv_data(ctx, hw->id);
								if(lv) {
									uint32_t logical = lv->col_order[div];
									ctx->drag_col_resize_id = hw->id;
									ctx->drag_col_resize_col = (int32_t)logical;
									ctx->drag_col_resize_start_x = ctx->mouse_x;
									ctx->drag_col_resize_start_w = lv->columns[logical].width;
								}

							} else {
								int32_t dcol = listview_header_hit(ctx, (uint32_t)hi, ctx->mouse_x, ctx->mouse_y);
								if(dcol >= 0) {
									ctx->drag_col_id = hw->id;
									ctx->drag_col_src = dcol;
									ctx->drag_col_start_x = ctx->mouse_x;

								} else {
									int32_t row = listview_row_hit(ctx, (uint32_t)hi, ctx->mouse_x, ctx->mouse_y);
									if(row >= 0) {
										struct mkgui_listview_data *lv = find_listv_data(ctx, hw->id);
										if(lv) {
											uint32_t now = mkgui_time_ms();
											uint32_t is_dblclick = (ctx->dblclick_id == hw->id && ctx->dblclick_row == row && (now - ctx->dblclick_time) < 400);
											ctx->dblclick_id = hw->id;
											ctx->dblclick_row = row;
											ctx->dblclick_time = now;
											if(hw->flags & MKGUI_MULTI_SELECT) {
												lv_multi_sel_toggle(lv, row);
												lv->selected_row = row;
											} else {
												lv->selected_row = row;
											}
											int32_t col = listview_col_hit(ctx, (uint32_t)hi, ctx->mouse_x);
											if(is_dblclick) {
												ev->type = MKGUI_EVENT_LISTVIEW_DBLCLICK;
												ev->id = hw->id;
												ev->value = row;
												ev->col = col;
												ctx->dblclick_id = 0;
												dirty_all(ctx);
												return 1;
											}
											lv->drag_source = row;
											lv->drag_target = -1;
											lv->drag_start_y = ctx->mouse_y;
											lv->drag_active = 0;
											ev->type = MKGUI_EVENT_LISTVIEW_SELECT;
											ev->id = hw->id;
											ev->value = row;
											ev->col = col;
											dirty_all(ctx);
											return 1;
										}
									}
								}
							}
						}
					}

					if(hw->type == MKGUI_ITEMVIEW) {
						uint32_t iv_sb_hit = itemview_scrollbar_hit(ctx, (uint32_t)hi, ctx->mouse_x, ctx->mouse_y);
						if(iv_sb_hit == 1) {
							ctx->drag_scrollbar_id = hw->id;
							ctx->drag_scrollbar_offset = itemview_thumb_offset(ctx, (uint32_t)hi, ctx->mouse_x, ctx->mouse_y);

						} else if(iv_sb_hit == 2 || iv_sb_hit == 3) {
							itemview_page_scroll(ctx, (uint32_t)hi, iv_sb_hit == 2 ? -1 : 1);

						} else {
							struct mkgui_itemview_data *iv = find_itemview_data(ctx, hw->id);
							if(iv) {
								int32_t item = itemview_hit_item(ctx, (uint32_t)hi, iv, ctx->mouse_x, ctx->mouse_y);
								if(item >= 0) {
									uint32_t now = mkgui_time_ms();
									uint32_t is_dblclick = (ctx->dblclick_id == hw->id && ctx->dblclick_row == item && (now - ctx->dblclick_time) < 400);
									ctx->dblclick_id = hw->id;
									ctx->dblclick_row = item;
									ctx->dblclick_time = now;
									iv->selected = item;
									if(is_dblclick) {
										ev->type = MKGUI_EVENT_ITEMVIEW_DBLCLICK;
										ctx->dblclick_id = 0;
									} else {
										ev->type = MKGUI_EVENT_ITEMVIEW_SELECT;
									}
									ev->id = hw->id;
									ev->value = item;
									dirty_all(ctx);
									return 1;
								}
							}
						}
					}

					if(hw->type == MKGUI_SCROLLBAR) {
						if(scrollbar_hit_thumb(ctx, (uint32_t)hi, ctx->mouse_x, ctx->mouse_y)) {
							ctx->drag_scrollbar_id = hw->id;
							ctx->drag_scrollbar_offset = scrollbar_thumb_drag_offset(ctx, (uint32_t)hi, ctx->mouse_x, ctx->mouse_y);
						} else {
							scrollbar_scroll(ctx, hw->id, ctx->mouse_y > ctx->rects[hi].y + ctx->rects[hi].h / 2 ? 1 : -1);
							ev->type = MKGUI_EVENT_SCROLL;
							ev->id = hw->id;
							struct mkgui_scrollbar_data *sbd = find_scrollbar_data(ctx, hw->id);
							ev->value = sbd ? sbd->value : 0;
							return 1;
						}
					}

					if(hw->type == MKGUI_MENUITEM) {
						struct mkgui_widget *parent = find_widget(ctx, hw->parent_id);
						if(parent && parent->type == MKGUI_MENU) {
							menu_open_popup(ctx, hw->id);
						}
					}

					if(hw->type == MKGUI_SLIDER) {
						struct mkgui_slider_data *sd = find_slider_data(ctx, hw->id);
						if(sd) {
							int32_t rx = ctx->rects[hi].x;
							int32_t rw = ctx->rects[hi].w;
							int32_t range = sd->max_val - sd->min_val;
							int32_t new_val = sd->min_val + (int32_t)((int64_t)(ctx->mouse_x - rx) * range / (rw > 0 ? rw : 1));
							if(new_val < sd->min_val) {
								new_val = sd->min_val;
							}
							if(new_val > sd->max_val) {
								new_val = sd->max_val;
							}
							sd->value = new_val;
							ev->type = MKGUI_EVENT_SLIDER_START;
							ev->id = hw->id;
							ev->value = new_val;
							dirty_widget(ctx, (uint32_t)hi);
							return 1;
						}
					}

					if(hw->type == MKGUI_DROPDOWN) {
						if(closed_dropdown_id != hw->id) {
							dropdown_open_popup(ctx, hw->id);
						}
					}

					if(hw->type == MKGUI_TREEVIEW) {
						int32_t node_idx = treeview_row_hit(ctx, (uint32_t)hi, ctx->mouse_y);
						if(node_idx >= 0) {
							struct mkgui_treeview_data *tv = find_treeview_data(ctx, hw->id);
							if(tv) {
								if(treeview_arrow_hit(ctx, (uint32_t)hi, ctx->mouse_x, node_idx) && tv->nodes[node_idx].has_children) {
									tv->nodes[node_idx].expanded = !tv->nodes[node_idx].expanded;
									dirty_all(ctx);
									ev->type = tv->nodes[node_idx].expanded ? MKGUI_EVENT_TREEVIEW_EXPAND : MKGUI_EVENT_TREEVIEW_COLLAPSE;
									ev->id = hw->id;
									ev->value = (int32_t)tv->nodes[node_idx].id;
									return 1;

								} else {
									uint32_t now_ms = mkgui_time_ms();
									uint32_t is_dblclick = (ctx->dblclick_id == hw->id && ctx->dblclick_row == (int32_t)tv->nodes[node_idx].id && (now_ms - ctx->dblclick_time) < 400);
									ctx->dblclick_id = hw->id;
									ctx->dblclick_row = (int32_t)tv->nodes[node_idx].id;
									ctx->dblclick_time = now_ms;
									tv->selected_node = (int32_t)tv->nodes[node_idx].id;
									tv->drag_source = node_idx;
									tv->drag_target = -1;
									tv->drag_pos = 0;
									tv->drag_active = 0;
									tv->drag_start_y = ctx->mouse_y;
									dirty_all(ctx);
									if(is_dblclick) {
										ev->type = MKGUI_EVENT_TREEVIEW_DBLCLICK;
										ctx->dblclick_id = 0;
									} else {
										ev->type = MKGUI_EVENT_TREEVIEW_SELECT;
									}
									ev->id = hw->id;
									ev->value = tv->selected_node;
									return 1;
								}
							}
						}
					}

					if(hw->type == MKGUI_SPINBOX) {
						int32_t dir = spinbox_btn_hit(ctx, (uint32_t)hi, ctx->mouse_x, ctx->mouse_y);
						struct mkgui_spinbox_data *sd = find_spinbox_data(ctx, hw->id);
						if(sd) {
							if(dir != 0) {
								sd->repeat_dir = dir;
								sd->repeat_next_ms = mkgui_time_ms() + 400;
								int32_t delta = (dir > 0) ? sd->step : -sd->step;
								spinbox_adjust(ctx, ev, hw->id, delta);
								return ev->type != MKGUI_EVENT_NONE;

							} else {
								sd->editing = 1;
								sd->select_all = 1;
								snprintf(sd->edit_buf, sizeof(sd->edit_buf), "%d", sd->value);
								sd->edit_len = (uint32_t)strlen(sd->edit_buf);
								dirty_all(ctx);
							}
						}
					}

				} else {
					spinbox_focus_lost(ctx);
					ctx->focus_id = 0;
				}
			} break;

			case MKGUI_PLAT_BUTTON_RELEASE: {
				if(pev.button >= 4) {
					break;
				}

				for(uint32_t si = 0; si < ctx->spinbox_count; ++si) {
					ctx->spinboxes[si].repeat_dir = 0;
				}

				for(uint32_t tvi = 0; tvi < ctx->treeview_count; ++tvi) {
					struct mkgui_treeview_data *tv = &ctx->treeviews[tvi];
					if(tv->drag_active && tv->drag_target >= 0) {
						int32_t src_id = (int32_t)tv->nodes[tv->drag_source].id;
						int32_t tgt_id = (int32_t)tv->nodes[tv->drag_target].id;
						int32_t pos = tv->drag_pos;
						tv->drag_active = 0;
						tv->drag_source = -1;
						tv->drag_target = -1;
						dirty_all(ctx);
						ev->type = MKGUI_EVENT_TREEVIEW_MOVE;
						ev->id = tv->widget_id;
						ev->value = src_id;
						ev->col = tgt_id;
						ev->keysym = (uint32_t)pos;
						return 1;
					}
					if(tv->drag_active) {
						ev->type = MKGUI_EVENT_DRAG_END;
						ev->id = tv->widget_id;
						dirty_all(ctx);
					}
					tv->drag_active = 0;
					tv->drag_source = -1;
					tv->drag_target = -1;
				}
				for(uint32_t lvi = 0; lvi < ctx->listv_count; ++lvi) {
					struct mkgui_listview_data *lv = &ctx->listvs[lvi];
					if(lv->drag_active && lv->drag_target >= 0) {
						int32_t src = lv->drag_source;
						int32_t tgt = lv->drag_target;
						lv->drag_active = 0;
						lv->drag_source = -1;
						lv->drag_target = -1;
						dirty_all(ctx);
						ev->type = MKGUI_EVENT_LISTVIEW_REORDER;
						ev->id = lv->widget_id;
						ev->value = src;
						ev->col = tgt;
						return 1;
					}
					if(lv->drag_active) {
						ev->type = MKGUI_EVENT_DRAG_END;
						ev->id = lv->widget_id;
						dirty_all(ctx);
					}
					lv->drag_active = 0;
					lv->drag_source = -1;
					lv->drag_target = -1;
				}

				ctx->drag_select_id = 0;

				if(ctx->drag_col_resize_id) {
					ctx->drag_col_resize_id = 0;
					dirty_all(ctx);
					break;
				}

				if(ctx->drag_col_id) {
					uint32_t dcid = ctx->drag_col_id;
					int32_t src = ctx->drag_col_src;
					int32_t dx = pev.x - ctx->drag_col_start_x;
					ctx->drag_col_id = 0;
					ctx->drag_col_src = -1;
					dirty_all(ctx);

					struct mkgui_listview_data *lv = find_listv_data(ctx, dcid);
					if(lv) {
						if(dx > -4 && dx < 4) {
							uint32_t logical = lv->col_order[src];
							if(lv->sort_col == (int32_t)logical) {
								lv->sort_dir = -lv->sort_dir;

							} else {
								lv->sort_col = (int32_t)logical;
								lv->sort_dir = 1;
							}
							ev->type = MKGUI_EVENT_LISTVIEW_SORT;
							ev->id = dcid;
							ev->col = (int32_t)logical;
							ev->value = lv->sort_dir;
							return 1;

						} else {
							int32_t widx = find_widget_idx(ctx, dcid);
							if(widx >= 0) {
								int32_t dst = listview_col_insert_pos(ctx, lv, (uint32_t)widx);
								if(dst != src && dst != src + 1) {
									uint32_t grabbed = lv->col_order[src];
									if(dst > src) {
										memmove(&lv->col_order[src], &lv->col_order[src + 1], (uint32_t)(dst - src - 1) * sizeof(uint32_t));
										lv->col_order[dst - 1] = grabbed;
									} else {
										memmove(&lv->col_order[dst + 1], &lv->col_order[dst], (uint32_t)(src - dst) * sizeof(uint32_t));
										lv->col_order[dst] = grabbed;
									}
									ev->type = MKGUI_EVENT_LISTVIEW_COL_REORDER;
									ev->id = dcid;
									return 1;
								}
							}
						}
					}
					break;
				}

				uint32_t was_press = ctx->press_id;
				ctx->press_id = 0;
				if(ctx->drag_scrollbar_id) {
					int32_t dsi = find_widget_idx(ctx, ctx->drag_scrollbar_id);
					if(dsi >= 0 && ctx->widgets[dsi].type == MKGUI_SCROLLBAR) {
						struct mkgui_scrollbar_data *sbd = find_scrollbar_data(ctx, ctx->drag_scrollbar_id);
						ev->type = MKGUI_EVENT_SCROLL;
						ev->id = ctx->drag_scrollbar_id;
						ev->value = sbd ? sbd->value : 0;
						ctx->drag_scrollbar_id = 0;
						dirty_all(ctx);
						return 1;
					}
				}
				ctx->drag_scrollbar_id = 0;
				if(was_press) {
					dirty_widget_id(ctx, was_press);
				}

				if(was_press) {
					int32_t hi = hit_test(ctx, pev.x, pev.y);
					if(hi >= 0 && ctx->widgets[hi].id == was_press) {
						struct mkgui_widget *hw = &ctx->widgets[hi];

						if(hw->type == MKGUI_BUTTON) {
							uint32_t now_ms = mkgui_time_ms();
							uint32_t is_dblclick = (ctx->dblclick_id == hw->id && (now_ms - ctx->dblclick_time) < 400);
							ctx->dblclick_id = hw->id;
							ctx->dblclick_row = 0;
							ctx->dblclick_time = now_ms;
							if(is_dblclick) {
								ev->type = MKGUI_EVENT_BUTTON_DBLCLICK;
								ctx->dblclick_id = 0;
							} else {
								ev->type = MKGUI_EVENT_CLICK;
							}
							ev->id = hw->id;
							return 1;
						}

						if(hw->type == MKGUI_CHECKBOX) {
							hw->flags ^= MKGUI_CHECKED;
							ev->type = MKGUI_EVENT_CHECKBOX_CHANGED;
							ev->id = hw->id;
							ev->value = (hw->flags & MKGUI_CHECKED) ? 1 : 0;
							return 1;
						}

						if(hw->type == MKGUI_RADIO) {
							return handle_radio_click(ctx, ev, hw);
						}

						if(hw->type == MKGUI_SLIDER) {
							struct mkgui_slider_data *sd = find_slider_data(ctx, hw->id);
							ev->type = MKGUI_EVENT_SLIDER_END;
							ev->id = hw->id;
							ev->value = sd ? sd->value : 0;
							return 1;
						}

						if(hw->type == MKGUI_HSPLIT || hw->type == MKGUI_VSPLIT) {
							ev->type = MKGUI_EVENT_SPLIT_MOVED;
							ev->id = hw->id;
							return 1;
						}
					}
				}
			} break;

			case MKGUI_PLAT_KEY: {
				uint32_t ks = pev.keysym;

				if(ks == MKGUI_KEY_ESCAPE && ctx->popup_count > 0) {
					popup_destroy(ctx, ctx->popup_count - 1);
					dirty_all(ctx);
					break;
				}

				if(ks == MKGUI_KEY_TAB || ks == MKGUI_KEY_ISO_LEFT_TAB) {
					uint32_t reverse = (pev.keymod & MKGUI_MOD_SHIFT) || (ks == MKGUI_KEY_ISO_LEFT_TAB);
					uint32_t start_idx = 0;
					if(ctx->focus_id) {
						for(uint32_t i = 0; i < ctx->widget_count; ++i) {
							if(ctx->widgets[i].id == ctx->focus_id) {
								start_idx = reverse ? (i + ctx->widget_count - 1) : (i + 1);
								break;
							}
						}
					}
					for(uint32_t i = 0; i < ctx->widget_count; ++i) {
						uint32_t idx;
						if(reverse) {
							idx = (start_idx + ctx->widget_count - i) % ctx->widget_count;
						} else {
							idx = (start_idx + i) % ctx->widget_count;
						}
						if(widget_visible(ctx, idx) && is_focusable(&ctx->widgets[idx])) {
							spinbox_focus_lost(ctx);
							ctx->focus_id = ctx->widgets[idx].id;
							dirty_all(ctx);
							break;
						}
					}
					break;
				}

				if((pev.keymod & 4) && ctx->focus_id) {
					struct mkgui_widget *fw = find_widget(ctx, ctx->focus_id);
					if(fw && (ks == 'a' || ks == 'A')) {
						if(fw->type == MKGUI_INPUT) {
							struct mkgui_input_data *inp = find_input_data(ctx, ctx->focus_id);
							if(inp) {
								inp->sel_start = 0;
								inp->sel_end = (uint32_t)strlen(inp->text);
								inp->cursor = inp->sel_end;
								dirty_all(ctx);
							}

						} else if(fw->type == MKGUI_TEXTAREA) {
							struct mkgui_textarea_data *ta = find_textarea_data(ctx, ctx->focus_id);
							if(ta) {
								ta->sel_start = 0;
								ta->sel_end = ta->text_len;
								ta->cursor = ta->text_len;
								dirty_all(ctx);
							}
						}
						break;
					}

					if(fw && (ks == 'c' || ks == 'C')) {
						if(fw->type == MKGUI_INPUT && !(fw->flags & MKGUI_PASSWORD)) {
							struct mkgui_input_data *inp = find_input_data(ctx, ctx->focus_id);
							if(inp) {
								if(input_has_selection(inp)) {
									uint32_t lo, hi;
									input_sel_range(inp, &lo, &hi);
									platform_clipboard_set(ctx, inp->text + lo, hi - lo);
								} else {
									platform_clipboard_set(ctx, inp->text, (uint32_t)strlen(inp->text));
								}
							}

						} else if(fw->type == MKGUI_TEXTAREA) {
							struct mkgui_textarea_data *ta = find_textarea_data(ctx, ctx->focus_id);
							if(ta) {
								if(textarea_has_selection(ta)) {
									uint32_t lo, hi;
									textarea_sel_range(ta, &lo, &hi);
									platform_clipboard_set(ctx, ta->text + lo, hi - lo);
								} else {
									platform_clipboard_set(ctx, ta->text, ta->text_len);
								}
							}
						}
						break;
					}

					if(fw && (ks == 'x' || ks == 'X') && !(fw->flags & MKGUI_READONLY)) {
						if(fw->type == MKGUI_INPUT && !(fw->flags & MKGUI_PASSWORD)) {
							struct mkgui_input_data *inp = find_input_data(ctx, ctx->focus_id);
							if(inp && input_has_selection(inp)) {
								uint32_t lo, hi;
								input_sel_range(inp, &lo, &hi);
								platform_clipboard_set(ctx, inp->text + lo, hi - lo);
								input_delete_selection(inp);
								dirty_all(ctx);
								ev->type = MKGUI_EVENT_INPUT_CHANGED;
								ev->id = ctx->focus_id;
								return 1;
							}

						} else if(fw->type == MKGUI_TEXTAREA) {
							struct mkgui_textarea_data *ta = find_textarea_data(ctx, ctx->focus_id);
							if(ta && textarea_has_selection(ta)) {
								uint32_t lo, hi;
								textarea_sel_range(ta, &lo, &hi);
								platform_clipboard_set(ctx, ta->text + lo, hi - lo);
								textarea_delete_selection(ta);
								dirty_all(ctx);
								textarea_scroll_to_cursor(ctx, ctx->focus_id);
								ev->type = MKGUI_EVENT_TEXTAREA_CHANGED;
								ev->id = ctx->focus_id;
								return 1;
							}
						}
						break;
					}

					if(fw && (ks == 'v' || ks == 'V') && !(fw->flags & MKGUI_READONLY)) {
						char clip_buf[MKGUI_CLIP_MAX];
						uint32_t clip_len = platform_clipboard_get(ctx, clip_buf, sizeof(clip_buf));
						if(clip_len > 0) {
							if(fw->type == MKGUI_INPUT) {
								struct mkgui_input_data *inp = find_input_data(ctx, ctx->focus_id);
								if(inp) {
									if(input_has_selection(inp)) {
										input_delete_selection(inp);
									}
									for(uint32_t ci = 0; ci < clip_len; ++ci) {
										if(clip_buf[ci] == '\n' || clip_buf[ci] == '\r') {
											clip_buf[ci] = ' ';
										}
									}
									uint32_t text_len = (uint32_t)strlen(inp->text);
									if(text_len + clip_len < MKGUI_MAX_TEXT - 1) {
										memmove(&inp->text[inp->cursor + clip_len], &inp->text[inp->cursor], text_len - inp->cursor + 1);
										memcpy(&inp->text[inp->cursor], clip_buf, clip_len);
										inp->cursor += clip_len;
										input_clear_selection(inp);
										dirty_all(ctx);
										ev->type = MKGUI_EVENT_INPUT_CHANGED;
										ev->id = ctx->focus_id;
										return 1;
									}
								}

							} else if(fw->type == MKGUI_TEXTAREA) {
								struct mkgui_textarea_data *ta = find_textarea_data(ctx, ctx->focus_id);
								if(ta) {
									if(textarea_has_selection(ta)) {
										textarea_delete_selection(ta);
									}
									for(uint32_t ci = 0; ci < clip_len; ++ci) {
										if(clip_buf[ci] == '\r') {
											memmove(&clip_buf[ci], &clip_buf[ci + 1], clip_len - ci - 1);
											--clip_len;
											--ci;
										}
									}
									textarea_ensure_cap(ta, ta->text_len + clip_len + 1);
									memmove(&ta->text[ta->cursor + clip_len], &ta->text[ta->cursor], ta->text_len - ta->cursor);
									memcpy(&ta->text[ta->cursor], clip_buf, clip_len);
									ta->cursor += clip_len;
									ta->text_len += clip_len;
									ta->text[ta->text_len] = '\0';
									textarea_clear_selection(ta);
									dirty_all(ctx);
									textarea_scroll_to_cursor(ctx, ctx->focus_id);
									ev->type = MKGUI_EVENT_TEXTAREA_CHANGED;
									ev->id = ctx->focus_id;
									return 1;
								}
							}
						}
						break;
					}
				}

				if(ctx->focus_id) {
					struct mkgui_widget *fw = find_widget(ctx, ctx->focus_id);
					if(fw) {
						switch(fw->type) {
							case MKGUI_INPUT: {
								if(handle_input_key(ctx, ev, ks, pev.keymod, pev.text, pev.text_len)) {
									return 1;
								}
							} break;

							case MKGUI_BUTTON: {
								if(handle_button_key(ctx, ev, ks)) {
									return 1;
								}
							} break;

							case MKGUI_CHECKBOX: {
								if(handle_checkbox_key(ctx, ev, ks)) {
									return 1;
								}
							} break;

							case MKGUI_DROPDOWN: {
								if(handle_dropdown_key(ctx, ev, ks)) {
									return 1;
								}
							} break;

							case MKGUI_SLIDER: {
								if(handle_slider_key(ctx, ev, ks)) {
									return 1;
								}
							} break;

							case MKGUI_LISTVIEW: {
								if(handle_listview_key(ctx, ev, ks)) {
									return 1;
								}
							} break;

							case MKGUI_TREEVIEW: {
								if(handle_treeview_key(ctx, ev, ks)) {
									return 1;
								}
							} break;

							case MKGUI_SPINBOX: {
								if(handle_spinbox_key(ctx, ev, ks, pev.text, pev.text_len)) {
									return 1;
								}
							} break;

							case MKGUI_RADIO: {
								if(handle_radio_key(ctx, ev, ks)) {
									return 1;
								}
							} break;

							case MKGUI_TEXTAREA: {
								if(handle_textarea_key(ctx, ev, ks, pev.keymod, pev.text, pev.text_len)) {
									return 1;
								}
							} break;

							case MKGUI_ITEMVIEW: {
								struct mkgui_itemview_data *iv = find_itemview_data(ctx, fw->id);
								if(iv) {
									int32_t fi = find_widget_idx(ctx, fw->id);
									if(fi >= 0 && handle_itemview_key(ctx, iv, (uint32_t)fi, ks, ev)) {
										return 1;
									}
								}
							} break;

							case MKGUI_TABS: {
								if(handle_tabs_key(ctx, ev, ks)) {
									return 1;
								}
							} break;

							default: {
							} break;
						}
					}
				}

				ev->type = MKGUI_EVENT_KEY;
				ev->keysym = ks;
				ev->keymod = pev.keymod;
				return 1;
			} break;

			case MKGUI_PLAT_CLOSE: {
				ev->type = MKGUI_EVENT_CLOSE;
				ctx->close_requested = 1;
				return 1;
			} break;

			case MKGUI_PLAT_LEAVE: {
				if(popup_idx >= 0) {
					uint32_t has_child_popup = (uint32_t)popup_idx + 1 < ctx->popup_count;
					if(!has_child_popup) {
						ctx->popups[popup_idx].hover_item = -1;
						ctx->popups[popup_idx].dirty = 1;
					}
				}
			} break;

			default: {
			} break;
		}
	}

	if(ctx->anim_active) {
		for(uint32_t i = 0; i < ctx->widget_count; ++i) {
			struct mkgui_widget *w = &ctx->widgets[i];
			if(!widget_visible(ctx, i)) {
				continue;
			}
			if(w->type == MKGUI_SPINNER || w->type == MKGUI_GLVIEW) {
				dirty_widget(ctx, i);
			}
			if(w->type == MKGUI_PROGRESS) {
				struct mkgui_progress_data *pd = find_progress_data(ctx, w->id);
				if(pd && pd->value > 0 && pd->value < pd->max_val) {
					dirty_widget(ctx, i);
				}
			}
		}
	}

	if(ctx->dirty) {
		double t0 = mkgui_time_us();
		layout_widgets(ctx);
		glview_sync_all(ctx);
		double t1 = mkgui_time_us();
		render_widgets(ctx);
		if(ctx->render_cb) {
			flush_text(ctx);
			ctx->render_cb(ctx, ctx->render_cb_data);
		}
		flush_text(ctx);
		render_tooltip(ctx);
		flush_text(ctx);
		double t2 = mkgui_time_us();

		if(ctx->dirty_full || ctx->render_cb) {
			platform_blit(ctx);
		} else {
			int32_t bx, by, bw, bh;
			dirty_bounds(ctx, &bx, &by, &bw, &bh);
			platform_blit_region(ctx, bx, by, bw, bh);
		}
		double t3 = mkgui_time_us();
		ctx->perf_layout_us = t1 - t0;
		ctx->perf_render_us = t2 - t1;
		ctx->perf_blit_us = t3 - t2;

		for(uint32_t pi = 0; pi < ctx->popup_count; ++pi) {
			struct mkgui_popup *p = &ctx->popups[pi];
			if(!p->active || !p->dirty) {
				continue;
			}
			struct mkgui_widget *pw = find_widget(ctx, p->widget_id);
			if(pw && pw->type == MKGUI_DROPDOWN) {
				struct mkgui_dropdown_data *dd = find_dropdown_data(ctx, pw->id);
				if(dd) {
					render_dropdown_popup(ctx, p, dd, p->hover_item);
					flush_text_popup(ctx, p);
					platform_popup_blit(ctx, p);
				}

			} else if(pw && pw->type == MKGUI_MENUITEM) {
				render_menu_popup(ctx, p, pw->id, p->hover_item);
				flush_text_popup(ctx, p);
				platform_popup_blit(ctx, p);
			}
			p->dirty = 0;
		}

		platform_flush(ctx);
		ctx->dirty = 0;
		ctx->dirty_full = 0;
		ctx->dirty_count = 0;
	}

	if(ctx->parent) {
		struct mkgui_ctx *p = ctx->parent;
#ifdef _WIN32
		int64_t pnow;
		QueryPerformanceCounter((LARGE_INTEGER *)&pnow);
		double p_dt = (double)(pnow - p->anim_prev) / (double)p->perf_freq;
		p->anim_prev = pnow;
#else
		struct timespec pnow;
		clock_gettime(CLOCK_MONOTONIC, &pnow);
		double p_dt = (double)(pnow.tv_sec - p->anim_prev.tv_sec) + (double)(pnow.tv_nsec - p->anim_prev.tv_nsec) * 1e-9;
		p->anim_prev = pnow;
#endif
		p->anim_time += p_dt;
		p->anim_active = 0;
		for(uint32_t i = 0; i < p->widget_count; ++i) {
			struct mkgui_widget *w = &p->widgets[i];
			if(!widget_visible(p, i)) {
				continue;
			}
			if(w->type == MKGUI_SPINNER || w->type == MKGUI_GLVIEW) {
				p->anim_active = 1;
				dirty_widget(p, i);
			}
			if(w->type == MKGUI_PROGRESS) {
				struct mkgui_progress_data *pd = find_progress_data(p, w->id);
				if(pd && pd->value > 0 && pd->value < pd->max_val) {
					p->anim_active = 1;
					dirty_widget(p, i);
				}
			}
		}
		if(p->anim_active && !ctx->anim_active) {
			ctx->anim_active = 1;
		}
		if(p->dirty) {
			layout_widgets(p);
			glview_sync_all(p);
			render_widgets(p);
			if(p->render_cb) {
				flush_text(p);
				p->render_cb(p, p->render_cb_data);
			}
			flush_text(p);
			render_tooltip(p);
			flush_text(p);
			platform_blit(p);
			platform_flush(p);
			p->dirty = 0;
			p->dirty_full = 0;
			p->dirty_count = 0;
		}
	}

	return ev->type != MKGUI_EVENT_NONE;
}

// [=]===^=[ mkgui_wait ]=========================================[=]
static void mkgui_wait(struct mkgui_ctx *ctx) {
	if(platform_pending(ctx) || ctx->close_requested || ctx->dirty) {
		return;
	}
	if(ctx->poll_timeout_ms == 0) {
		return;
	}
	if(ctx->poll_timeout_ms > 0) {
		platform_wait_event(ctx, ctx->poll_timeout_ms);
		return;
	}
	uint32_t any_anim = ctx->anim_active;
	if(!any_anim) {
		for(uint32_t i = 0; i < window_registry_count; ++i) {
			struct mkgui_ctx *c = window_registry[i];
			if(c->anim_active || c->dirty) {
				any_anim = 1;
				break;
			}
		}
	}
	platform_wait_event(ctx, any_anim ? 16 : -1);
}

// [=]===^=[ mkgui_set_poll_timeout ]==============================[=]
static void mkgui_set_poll_timeout(struct mkgui_ctx *ctx, int32_t ms) {
	ctx->poll_timeout_ms = ms;
}

// ---------------------------------------------------------------------------
// Dialog windows
// ---------------------------------------------------------------------------

#include "mkgui_dialogs.c"
#include "mkgui_filedialog.c"
#include "mkgui_iconbrowser.c"
