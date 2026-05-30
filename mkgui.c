// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

#if defined(__SSE2__)
#include <emmintrin.h>
#endif
#if defined(__SSE4_1__)
#include <smmintrin.h>
#endif
#include <immintrin.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifdef MKGUI_LIBRARY
#define MKGUI_API
#else
#define MKGUI_API static
#endif

// ---------------------------------------------------------------------------
// Public API (single source of truth for all public types, constants, enums)
// ---------------------------------------------------------------------------

#include "mkgui.h"

// ---------------------------------------------------------------------------
// Internal constants (not in public header)
// ---------------------------------------------------------------------------

#define MKGUI_GLYPH_FIRST     32
#define MKGUI_GLYPH_LAST      255
#define MKGUI_GLYPH_COUNT     (MKGUI_GLYPH_LAST - MKGUI_GLYPH_FIRST + 1)
#define MKGUI_GLYPH_MAX_BMP   4096

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
	MKGUI_PLAT_FOCUS_OUT,
	MKGUI_PLAT_DROP,
};

enum {
	MKGUI_CURSOR_DEFAULT,
	MKGUI_CURSOR_H_RESIZE,
	MKGUI_CURSOR_V_RESIZE,
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

// [=]===^=[ mkgui_sleep_ns ]======================================[=]
// Sleep for the given number of nanoseconds. On Windows, ms-granular at
// the kernel level (Sleep ceils to ms; sub-ms requires a waitable timer).
MKGUI_API void mkgui_sleep_ns(uint64_t ns) {
#ifdef _WIN32
	DWORD ms = (DWORD)((ns + 999999ull) / 1000000ull);
	Sleep(ms);
#else
	struct timespec ts;
	ts.tv_sec  = (time_t)(ns / 1000000000ull);
	ts.tv_nsec = (long)(ns % 1000000000ull);
	nanosleep(&ts, NULL);
#endif
}

// [=]===^=[ mkgui_now_ns ]========================================[=]
// Monotonic clock in nanoseconds. Same epoch the library uses internally,
// so host deadlines and library deadlines are commensurable.
MKGUI_API uint64_t mkgui_now_ns(void) {
#ifdef _WIN32
	static int64_t freq;
	if(!freq) {
		QueryPerformanceFrequency((LARGE_INTEGER *)&freq);
	}
	int64_t now;
	QueryPerformanceCounter((LARGE_INTEGER *)&now);
	uint64_t q = (uint64_t)now / (uint64_t)freq;
	uint64_t r = (uint64_t)now % (uint64_t)freq;
	return q * 1000000000ull + (r * 1000000000ull) / (uint64_t)freq;
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
#endif
}

// ---------------------------------------------------------------------------
// Internal data structures
// ---------------------------------------------------------------------------

struct mkgui_listview_data {
	uint32_t widget_id;
	uint32_t row_count;
	uint32_t col_count;
	struct mkgui_column columns[MKGUI_MAX_COLS];
	mkgui_row_cb row_cb;
	void *userdata;
	int32_t scroll_y;
	int32_t scroll_x;
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

struct mkgui_gridview_data {
	uint32_t widget_id;
	uint32_t row_count;
	uint32_t col_count;
	struct mkgui_grid_column columns[MKGUI_MAX_COLS];
	mkgui_grid_cell_cb cell_cb;
	void *userdata;
	int32_t scroll_y;
	int32_t selected_row;
	int32_t selected_col;
	int32_t header_height;
	uint8_t *checks;
	uint32_t checks_cap;
	int32_t drag_source;
	int32_t drag_target;
	int32_t drag_start_y;
	uint32_t drag_active;
};

struct mkgui_richlist_data {
	uint32_t widget_id;
	uint32_t row_count;
	int32_t row_height;
	mkgui_richlist_cb row_cb;
	void *userdata;
	int32_t scroll_y;
	int32_t selected_row;
};

#define MKGUI_UNDO_MAX 32

struct mkgui_input_snap {
	char text[MKGUI_MAX_TEXT];
	uint32_t cursor;
	uint32_t sel_start;
	uint32_t sel_end;
};

struct mkgui_input_data {
	uint32_t widget_id;
	char text[MKGUI_MAX_TEXT];
	uint32_t cursor;
	uint32_t sel_start;
	uint32_t sel_end;
	int32_t scroll_x;
	struct mkgui_input_snap undo[MKGUI_UNDO_MAX];
	uint32_t undo_pos;
	uint32_t undo_count;
	uint32_t undo_saved;
	uint32_t undo_last_ms;
};

struct mkgui_ipinput_data {
	uint32_t widget_id;
	uint8_t octets[4];
	uint32_t active_field;
	uint32_t sel_all;
	uint32_t editing;
	char edit_buf[4];
	uint32_t edit_len;
};

struct mkgui_toggle_data {
	uint32_t widget_id;
	uint32_t state;
};

struct mkgui_combobox_data {
	uint32_t widget_id;
	char items[MKGUI_MAX_DROPDOWN][MKGUI_MAX_TEXT];
	uint32_t item_count;
	int32_t selected;
	char text[MKGUI_MAX_TEXT];
	char prev_text[MKGUI_MAX_TEXT];
	uint32_t cursor;
	uint32_t sel_start;
	uint32_t sel_end;
	uint32_t popup_open;
	int32_t highlight;
	uint32_t filter_map[MKGUI_MAX_DROPDOWN];
	uint32_t filter_count;
	int32_t scroll_y;
	int32_t scroll_x;
};

struct mkgui_datepicker_data {
	uint32_t widget_id;
	int32_t year;
	int32_t month;
	int32_t day;
	int32_t view_year;
	int32_t view_month;
	uint32_t popup_open;
	int32_t cal_hover;
	uint32_t editing;
	char edit_buf[16];
	uint32_t edit_cursor;
	uint32_t month_select;
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
	float meter_pre;
	float meter_post;
	uint32_t meter_pre_color;
	uint32_t meter_post_color;
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
	uint32_t order_dirty;
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
	uint32_t color;
};

struct mkgui_meter_data {
	uint32_t widget_id;
	int32_t value;
	int32_t max_val;
	int32_t zone_t1;
	int32_t zone_t2;
	uint32_t zone_c1;
	uint32_t zone_c2;
	uint32_t zone_c3;
};

struct mkgui_textarea_snap {
	char *text;
	uint32_t text_len;
	uint32_t cursor;
	uint32_t sel_start;
	uint32_t sel_end;
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
	struct mkgui_textarea_snap undo[MKGUI_UNDO_MAX];
	uint32_t undo_pos;
	uint32_t undo_count;
	uint32_t undo_saved;
	uint32_t undo_last_ms;
};


struct mkgui_logview_line {
	uint64_t arena_off;
	uint32_t len;
	uint64_t runs_off;
	uint32_t runs_count;
};

struct mkgui_logview_run {
	uint32_t start;
	uint32_t fg;
	uint32_t bg;
};

struct mkgui_logview_vline {
	uint64_t line_seq;
	uint32_t src_off;
	uint32_t len;
};

struct mkgui_logview_data {
	uint32_t widget_id;

	uint32_t max_lines;
	uint32_t arena_bytes;
	uint32_t max_runs;

	char *text_arena;
	uint64_t arena_head;

	struct mkgui_logview_line *lines;
	uint64_t line_seq_head;
	uint64_t line_seq_tail;

	struct mkgui_logview_run *runs;
	uint64_t runs_head;

	uint32_t cur_fg;
	uint32_t cur_bg;
	uint32_t esc_state;
	char esc_buf[64];
	uint32_t esc_len;

	char *pending_text;
	uint32_t pending_len;
	uint32_t pending_cap;
	struct mkgui_logview_run *pending_runs;
	uint32_t pending_runs_count;
	uint32_t pending_runs_cap;

	struct mkgui_logview_vline *vlines;
	uint32_t vline_count;
	uint32_t vline_cap;
	uint64_t vline_seq_tail;
	int32_t cached_width;
	int32_t cached_row_h;

	int32_t scroll_y;
	int32_t scroll_x;
	uint32_t stuck_to_end;

	uint32_t has_selection;
	uint64_t sel_anchor_line_seq;
	uint32_t sel_anchor_off;
	uint64_t sel_cursor_line_seq;
	uint32_t sel_cursor_off;
};

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

struct mkgui_canvas_data {
	uint32_t widget_id;
	mkgui_canvas_cb callback;
	void *userdata;
};

struct mkgui_pathbar_data {
	uint32_t widget_id;
	char path[4096];
	uint32_t editing;
	uint32_t segment_count;
	struct mkgui_pathbar_seg {
		uint32_t offset;
		uint32_t len;
		int32_t x;
		int32_t w;
	} segments[MKGUI_PATHBAR_MAX_SEGS];
	int32_t hover_seg;

	char edit_buf[4096];
	uint32_t edit_cursor;
	uint32_t edit_sel_start;
	uint32_t edit_sel_end;
	int32_t edit_scroll_x;
};

struct mkgui_text_edit {
	char *text;
	uint32_t buf_size;
	uint32_t cursor;
	uint32_t sel_start;
	uint32_t sel_end;
	int32_t scroll_x;
};

#define MKGUI_CTXMENU_POPUP_ID   UINT32_MAX

struct mkgui_ctxmenu_item {
	uint32_t id;
	char label[MKGUI_MAX_TEXT];
	char icon[MKGUI_ICON_NAME_LEN];
	uint32_t flags;
};

struct mkgui_glyph {
	int32_t width;
	int32_t height;
	int32_t bearing_x;
	int32_t bearing_y;
	int32_t advance;
	uint32_t atlas_offset;
};

// Glyph atlas: linear uint8_t alpha bitmaps for all 224 glyphs (contiguous per glyph)
static uint8_t *glyph_atlas;
static uint8_t glyph_staging[MKGUI_GLYPH_COUNT][MKGUI_GLYPH_MAX_BMP];

struct mkgui_popup {
	struct mkgui_popup_platform plat;
	uint32_t *pixels;
	int32_t x, y, w, h;
	uint32_t widget_id;
	uint32_t active;
	uint32_t dirty;
	int32_t hover_item;
};

// ---------------------------------------------------------------------------
// Icon system
// ---------------------------------------------------------------------------

struct mkgui_icon {
	char name[MKGUI_ICON_NAME_LEN];
	// Logical size requested by the caller; stored so svg_rerasterize_all
	// can re-render at the correct size on scale/theme change. 0 for raster
	// icons added via mkgui_icon_add where size is implicit in w/h.
	int32_t requested_size;
	// Index into svg_sources[] for the SVG data that backs this icon, or
	// UINT32_MAX for raster icons and icons whose source was never cached.
	// Lets one SVG source back many icon variants (same name at different
	// sizes, or coloured variants under dlg: prefix).
	uint32_t source_idx;
	uint32_t *pixels;
	int32_t w, h;
	uint32_t atlas_offset;
	uint32_t custom;
};

// Icon atlas: linear uint32_t ARGB pixels for all loaded icons (contiguous per icon)
static uint32_t *icon_atlas;

static struct mkgui_icon icons[MKGUI_MAX_ICONS];
static uint32_t icon_count;

#define MKGUI_ICON_HASH_SIZE (MKGUI_MAX_ICONS * 2)
#define MKGUI_ICON_HASH_MASK (MKGUI_ICON_HASH_SIZE - 1)

static uint32_t icon_hash[MKGUI_ICON_HASH_SIZE];

// [=]===^=[ icon_hash_fn ]===========================================[=]
// FNV-1a over the name, then mixes in the requested size so the same
// icon at different sizes lives in separate hash slots.
static uint32_t icon_hash_fn(const char *name, int32_t size) {
	uint32_t h = 2166136261u;
	for(const char *p = name; *p; ++p) {
		h ^= (uint32_t)(uint8_t)*p;
		h *= 16777619u;
	}
	h ^= (uint32_t)size * 2654435761u;
	return h & MKGUI_ICON_HASH_MASK;
}

// [=]===^=[ icon_hash_clear ]========================================[=]
static void icon_hash_clear(void) {
	for(uint32_t i = 0; i < MKGUI_ICON_HASH_SIZE; ++i) {
		icon_hash[i] = UINT32_MAX;
	}
}

// [=]===^=[ icon_hash_insert ]=======================================[=]
static void icon_hash_insert(uint32_t idx) {
	uint32_t h = icon_hash_fn(icons[idx].name, icons[idx].requested_size);
	while(icon_hash[h] != UINT32_MAX) {
		h = (h + 1) & MKGUI_ICON_HASH_MASK;
	}
	icon_hash[h] = idx;
}

// [=]===^=[ icon_hash_lookup_at ]====================================[=]
// Exact lookup on (name, size). Misses if the icon exists at other sizes.
static int32_t icon_hash_lookup_at(const char *name, int32_t size) {
	uint32_t h = icon_hash_fn(name, size);
	for(;;) {
		uint32_t idx = icon_hash[h];
		if(idx == UINT32_MAX) {
			return -1;
		}

		if(icons[idx].requested_size == size && strcmp(icons[idx].name, name) == 0) {
			return (int32_t)idx;
		}
		h = (h + 1) & MKGUI_ICON_HASH_MASK;
	}
}

// [=]===^=[ icon_hash_lookup_any ]===================================[=]
// Linear scan for the first entry with this name, regardless of size.
// Used for existence checks (does any variant of this icon exist?).
static int32_t icon_hash_lookup_any(const char *name) {
	for(uint32_t i = 0; i < icon_count; ++i) {
		if(strcmp(icons[i].name, name) == 0) {
			return (int32_t)i;
		}
	}
	return -1;
}

struct mkgui_rect {
	int32_t x, y, w, h;
};

struct mkgui_timer {
	uint32_t id;
	uint64_t interval_ns;
	mkgui_timer_cb cb;
	void *userdata;
	uint32_t active;
#ifdef _WIN32
	HANDLE handle;
	LARGE_INTEGER epoch;
	uint64_t fire_count;
#else
	int32_t fd;
#endif
};

struct mkgui_accel {
	uint32_t keysym;
	uint32_t keymod;
	uint32_t id;
};

// Reserve-commit virtual memory arena. Pages are mapped PROT_NONE up front,
// committed (read/write) on demand in fixed-size bumps. Pointers handed out
// from .base never move, so callers can cache them across grow operations.
// The implementation (vm_arena_create/ensure/destroy) is defined below.
struct vm_arena {
	uint8_t *base;
	size_t reserved;
	size_t committed;
};

// Per-window virtual reserve. One arena per array; the array pointer in
// mkgui_window is set once to arena.base and stays valid forever. Cap and
// count fields advance as vm_arena_ensure commits more pages. The reserve
// is sized so the practical maximum (well past any real-world UI count)
// fits in committed pages without ever hitting the ceiling.
struct mkgui_window_arenas {
	struct vm_arena widgets;
	struct vm_arena rects;
	struct vm_arena tooltip_texts;
	struct vm_arena listvs;
	struct vm_arena inputs;
	struct vm_arena dropdowns;
	struct vm_arena sliders;
	struct vm_arena tabs;
	struct vm_arena splits;
	struct vm_arena treeviews;
	struct vm_arena statusbars;
	struct vm_arena spinboxes;
	struct vm_arena progress;
	struct vm_arena meters;
	struct vm_arena textareas;
	struct vm_arena logviews;
	struct vm_arena itemviews;
	struct vm_arena scrollbars;
	struct vm_arena box_scrolls;
	struct vm_arena images;
	struct vm_arena glviews;
	struct vm_arena canvases;
	struct vm_arena pathbars;
	struct vm_arena ipinputs;
	struct vm_arena toggles;
	struct vm_arena comboboxes;
	struct vm_arena datepickers;
	struct vm_arena gridviews;
	struct vm_arena richlists;
};

// Forward decl: mkgui_window and mkgui_ctx hold pointers to each other.
struct mkgui_ctx;

struct mkgui_window {
	struct mkgui_ctx *ctx;        // back-pointer to the owning application ctx
	struct mkgui_platform plat;

	uint32_t *pixels;
	int32_t win_w, win_h;

	struct mkgui_window_arenas arenas;

	struct mkgui_widget *widgets;
	uint32_t widget_count;
	uint32_t widget_cap;

	struct mkgui_rect *rects;

	char (*tooltip_texts)[MKGUI_MAX_TEXT];

	struct mkgui_listview_data *listvs;
	uint32_t listv_count, listv_cap;
	struct mkgui_input_data *inputs;
	uint32_t input_count, input_cap;
	struct mkgui_dropdown_data *dropdowns;
	uint32_t dropdown_count, dropdown_cap;
	struct mkgui_slider_data *sliders;
	uint32_t slider_count, slider_cap;
	struct mkgui_tabs_data *tabs;
	uint32_t tab_count, tab_cap;
	struct mkgui_split_data *splits;
	uint32_t split_count, split_cap;
	struct mkgui_treeview_data *treeviews;
	uint32_t treeview_count, treeview_cap;
	struct mkgui_statusbar_data *statusbars;
	uint32_t statusbar_count, statusbar_cap;
	struct mkgui_spinbox_data *spinboxes;
	uint32_t spinbox_count, spinbox_cap;
	struct mkgui_progress_data *progress;
	uint32_t progress_count, progress_cap;
	struct mkgui_meter_data *meters;
	uint32_t meter_count, meter_cap;
	struct mkgui_textarea_data *textareas;
	uint32_t textarea_count, textarea_cap;
	struct mkgui_logview_data *logviews;
	uint32_t logview_count, logview_cap;
	struct mkgui_itemview_data *itemviews;
	uint32_t itemview_count, itemview_cap;
	struct mkgui_scrollbar_data *scrollbars;
	uint32_t scrollbar_count, scrollbar_cap;
	struct mkgui_box_scroll *box_scrolls;
	uint32_t box_scroll_count, box_scroll_cap;
	struct mkgui_image_data *images;
	uint32_t image_count, image_cap;
	struct mkgui_glview_data *glviews;
	uint32_t glview_count, glview_cap;
	struct mkgui_canvas_data *canvases;
	uint32_t canvas_count, canvas_cap;
	struct mkgui_pathbar_data *pathbars;
	uint32_t pathbar_count, pathbar_cap;
	struct mkgui_ipinput_data *ipinputs;
	uint32_t ipinput_count, ipinput_cap;
	struct mkgui_toggle_data *toggles;
	uint32_t toggle_count, toggle_cap;
	struct mkgui_combobox_data *comboboxes;
	uint32_t combobox_count, combobox_cap;
	struct mkgui_datepicker_data *datepickers;
	uint32_t datepicker_count, datepicker_cap;
	struct mkgui_gridview_data *gridviews;
	uint32_t gridview_count, gridview_cap;
	struct mkgui_richlist_data *richlists;
	uint32_t richlist_count, richlist_cap;

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
	uint32_t drag_scrollbar_horiz;
	uint32_t drag_col_id;
	int32_t drag_col_src;
	int32_t drag_col_start_x;
	uint32_t drag_col_resize_id;
	int32_t drag_col_resize_col;
	int32_t drag_col_resize_start_x;
	int32_t drag_col_resize_start_w;
	uint32_t drag_select_id;
	uint32_t drag_text_id;
	uint32_t drag_text_drop_pos;
	int32_t drag_slider_offset;
	int32_t mouse_x, mouse_y;
	uint32_t mouse_btn;
	uint32_t dirty;

	uint32_t dblclick_id;
	int32_t dblclick_row;
	uint32_t dblclick_time;

	uint32_t divider_dblclick_id;
	int32_t divider_dblclick_col;
	uint32_t divider_dblclick_time;

	struct mkgui_cell_edit {
		uint32_t active;
		uint32_t widget_id;
		int32_t row;
		int32_t col;
		char text[MKGUI_MAX_TEXT];
		struct mkgui_text_edit te;
		int32_t cell_x, cell_y, cell_w, cell_h;
		uint32_t dragging;
	} cell_edit;

	struct mkgui_toast_slot {
		uint32_t active;
		uint32_t severity;
		uint32_t expire_ms;
		int32_t x, y, w, h;
		char text[MKGUI_MAX_TEXT];
	} toasts[MKGUI_MAX_TOASTS];

	struct mkgui_banner_state {
		uint32_t active;
		uint32_t severity;
		int32_t close_x, close_y, close_w, close_h;
		char text[MKGUI_MAX_TEXT];
	} banner;

	struct mkgui_rect dirty_rects[32];
	uint32_t dirty_count;
	uint32_t dirty_full;

	uint32_t tooltip_id;
	uint32_t tooltip_start_ms;
	uint32_t tooltip_shown;
	int32_t tooltip_x, tooltip_y;

	struct mkgui_theme theme;

	double anim_time;
#ifdef _WIN32
	int64_t anim_prev;
	int64_t perf_freq;
#else
	struct timespec anim_prev;
#endif
	uint32_t anim_active;
	uint32_t should_close;
	uint32_t window_visible;
	uint32_t hide_on_close;
	uint32_t undecorated;
	uint32_t canvas_window;

	float scale;
	int32_t row_height;
	int32_t tab_height;
	int32_t menu_height;
	int32_t scrollbar_w;
	int32_t split_thick;
	int32_t split_min_px;
	int32_t box_gap;
	int32_t box_pad;
	int32_t icon_size;
	int32_t dialog_icon_size;
	int32_t pathbar_height;
	int32_t toolbar_height;
	int32_t toolbar_btn_w;
	int32_t toolbar_sep_w;
	int32_t statusbar_height;

	struct mkgui_timer timers[MKGUI_MAX_TIMERS];
	uint32_t timer_count;
	uint32_t timer_next_id;

	struct mkgui_accel accels[MKGUI_MAX_ACCELS];
	uint32_t accel_count;

	char *drop_files[MKGUI_DROP_MAX];
	uint32_t drop_count;
	uint32_t drop_enabled;

	double perf_layout_us;
	double perf_render_us;
	double perf_blit_us;

	struct mkgui_glyph glyphs[MKGUI_GLYPH_COUNT];
	int32_t font_ascent;
	int32_t font_height;
	int32_t char_width;

	mkgui_render_cb render_cb;
	void *render_cb_data;

	mkgui_event_cb event_cb;
	void *event_cb_data;

	struct mkgui_window *parent;

	char clip_text[MKGUI_CLIP_MAX];
	uint32_t clip_len;

	struct mkgui_ctxmenu_item ctxmenu_items[MKGUI_MAX_CTXMENU];
	uint32_t ctxmenu_count;
	int32_t ctxmenu_x, ctxmenu_y;

	char app_class[64];
	char icon_dir[4096];
#define MKGUI_THEME_CHAIN_MAX 8
	char system_theme_dirs[MKGUI_THEME_CHAIN_MAX][4096];
	uint32_t system_theme_count;
};

// Application-level context. Created once per app via mkgui_ctx_create();
// owns app-wide state and the window registry. Each window holds a back-
// pointer via win->ctx. New windows inherit the ctx's theme/scale/app_class
// defaults at creation time and can be re-themed in bulk via the ctx
// setters.
#define MKGUI_MAX_WINDOWS 16
struct mkgui_ctx {
	struct mkgui_window *windows[MKGUI_MAX_WINDOWS];
	uint32_t window_count;
	uint32_t should_quit;          // app-wide quit flag
	char app_class[64];            // default WM_CLASS for new windows
	struct mkgui_theme theme;      // default theme for new windows
	float scale;                   // default scale for new windows
	struct mkgui_window *primary;  // window whose close ends mkgui_ctx_run
};

// ---------------------------------------------------------------------------
// Virtual memory arena (reserve-commit, no realloc)
// ---------------------------------------------------------------------------

// Sized at 1M widgets: an order of magnitude past any conceivable real UI
// (a desktop full of 100 windows with 100 widgets each is only 10k), while
// still bounded so the per-win virtual reserve stays sane on 32-bit-virtual
// hosts. Bumping later is a constant change and a recompile.
#define MKGUI_VM_MAX_TEXT_CMDS   32768
#define MKGUI_VM_MAX_WIDGETS     (1u << 20)
#define MKGUI_VM_MAX_HASH_SLOTS  (MKGUI_VM_MAX_WIDGETS * 2)

#ifdef _WIN32

// [=]===^=[ vm_arena_create ]=====================================[=]
static uint32_t vm_arena_create(struct vm_arena *a, size_t reserve_bytes) {
	a->base = (uint8_t *)VirtualAlloc(NULL, reserve_bytes, MEM_RESERVE, PAGE_NOACCESS);
	if(!a->base) {
		return 0;
	}
	a->reserved = reserve_bytes;
	a->committed = 0;
	return 1;
}

// [=]===^=[ vm_arena_ensure ]=====================================[=]
static uint32_t vm_arena_ensure(struct vm_arena *a, size_t needed) {
	if(needed <= a->committed) {
		return 1;
	}

	if(needed > a->reserved) {
		return 0;
	}
	size_t page_size = 65536;
	size_t new_commit = (needed + page_size - 1) & ~(page_size - 1);
	if(new_commit > a->reserved) {
		new_commit = a->reserved;
	}

	if(!VirtualAlloc(a->base, new_commit, MEM_COMMIT, PAGE_READWRITE)) {
		return 0;
	}
	a->committed = new_commit;
	return 1;
}

// [=]===^=[ vm_arena_destroy ]====================================[=]
static void vm_arena_destroy(struct vm_arena *a) {
	if(a->base) {
		VirtualFree(a->base, 0, MEM_RELEASE);
		a->base = NULL;
		a->reserved = 0;
		a->committed = 0;
	}
}

#else

#include <sys/mman.h>

// [=]===^=[ vm_arena_create ]=====================================[=]
static uint32_t vm_arena_create(struct vm_arena *a, size_t reserve_bytes) {
	a->base = (uint8_t *)mmap(NULL, reserve_bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if(a->base == MAP_FAILED) {
		a->base = NULL;
		return 0;
	}
	a->reserved = reserve_bytes;
	a->committed = 0;
	return 1;
}

// [=]===^=[ vm_arena_ensure ]=====================================[=]
static uint32_t vm_arena_ensure(struct vm_arena *a, size_t needed) {
	if(needed <= a->committed) {
		return 1;
	}

	if(needed > a->reserved) {
		return 0;
	}
	size_t page_size = 4096;
	size_t new_commit = (needed + page_size - 1) & ~(page_size - 1);
	if(new_commit > a->reserved) {
		new_commit = a->reserved;
	}

	if(mprotect(a->base, new_commit, PROT_READ | PROT_WRITE) != 0) {
		return 0;
	}
	a->committed = new_commit;
	return 1;
}

// [=]===^=[ vm_arena_destroy ]====================================[=]
static void vm_arena_destroy(struct vm_arena *a) {
	if(a->base) {
		munmap(a->base, a->reserved);
		a->base = NULL;
		a->reserved = 0;
		a->committed = 0;
	}
}

#endif

// ---------------------------------------------------------------------------
// Dynamic array growth
// ---------------------------------------------------------------------------

#define MKGUI_GROW_WIDGETS 256
#define MKGUI_GROW_AUX     16
#define STYLE_BORDER_BIT   (1u << 0)

// Bump the per-array vm_arena to fit at least cap+GROW_AUX elements. The
// array pointer never moves; only the committed footprint grows. Pages
// committed by mprotect/VirtualAlloc(COMMIT) are zero-initialised by the
// kernel, so no memset is required for the new tail.
#define MKGUI_AUX_GROW(arena_ptr, count, cap, type) do { \
	if((count) >= (cap)) { \
		uint32_t _nc = (cap) + MKGUI_GROW_AUX; \
		if(vm_arena_ensure((arena_ptr), (size_t)_nc * sizeof(type))) { \
			(cap) = _nc; \
		} \
	} \
} while(0)

// ---------------------------------------------------------------------------
// Default theme
// ---------------------------------------------------------------------------

// [=]===^=[ default_theme ]=====================================[=]
MKGUI_API struct mkgui_theme default_theme(void) {
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
	t.scrollbar_bg        = 0xff1d2023;
	t.scrollbar_thumb     = 0xff4d4d4d;
	t.scrollbar_thumb_hover = 0xff5a5a5a;
	t.highlight           = 0xff3daee9;
	t.header_bg           = 0xff2a2e32;
	t.listview_alt        = 0xff2a2e32;
	t.accent              = 0xff2a7ab5;
	t.corner_radius       = 3;
	return t;
}

// [=]===^=[ light_theme ]========================================[=]
MKGUI_API struct mkgui_theme light_theme(void) {
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
	t.scrollbar_bg          = 0xffe0e0e0;
	t.scrollbar_thumb       = 0xffb0b0b0;
	t.scrollbar_thumb_hover = 0xff909090;
	t.highlight             = 0xff3daee9;
	t.header_bg             = 0xffe8e8e8;
	t.listview_alt          = 0xfff5f5f5;
	t.accent                = 0xff2a7ab5;
	t.corner_radius         = 3;
	return t;
}

// [=]===^=[ theme_prefers_dark ]=================================[=]
static uint32_t theme_prefers_dark(void) {
	char *override = getenv("MKGUI_THEME");
	if(override) {
		if(strcmp(override, "light") == 0) {
			return 0;
		}

		if(strcmp(override, "dark") == 0) {
			return 1;
		}
	}
	char *gtk = getenv("GTK_THEME");
	if(gtk && gtk[0]) {
		char lower[128];
		uint32_t i = 0;
		while(gtk[i] && i < sizeof(lower) - 1) {
			char c = gtk[i];
			if(c >= 'A' && c <= 'Z') {
				c = (char)(c + ('a' - 'A'));
			}
			lower[i] = c;
			++i;
		}
		lower[i] = 0;
		return strstr(lower, "dark") != NULL;
	}
#ifdef _WIN32
	DWORD value = 1;
	DWORD size = sizeof(value);
	LONG res = RegGetValueA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", "AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &value, &size);
	if(res == ERROR_SUCCESS) {
		return value == 0;
	}
#endif
	return 1;
}

// [=]===^=[ auto_theme ]=========================================[=]
static struct mkgui_theme auto_theme(void) {
	return theme_prefers_dark() ? default_theme() : light_theme();
}

// ---------------------------------------------------------------------------
// Public API validation macros
// ---------------------------------------------------------------------------

#define MKGUI_CHECK(win) do { if(!(win)) { return; } } while(0)
#define MKGUI_CHECK_VAL(win, val) do { if(!(win)) { return (val); } } while(0)

// ---------------------------------------------------------------------------
// Scale helpers
// ---------------------------------------------------------------------------

static inline int32_t sc(struct mkgui_window *win, int32_t px) {
	return (int32_t)(px * win->scale + 0.5f);
}

// [=]===^=[ mkgui_recompute_metrics ]==============================[=]
static void mkgui_recompute_metrics(struct mkgui_window *win) {
	win->row_height      = sc(win, MKGUI_ROW_HEIGHT);
	win->tab_height      = sc(win, MKGUI_TAB_HEIGHT);
	win->menu_height     = sc(win, MKGUI_MENU_HEIGHT);
	win->scrollbar_w     = sc(win, MKGUI_SCROLLBAR_W);
	win->split_thick     = sc(win, MKGUI_SPLIT_THICK);
	win->split_min_px    = sc(win, MKGUI_SPLIT_MIN_PX);
	win->box_gap         = sc(win, MKGUI_BOX_GAP);
	win->box_pad         = sc(win, MKGUI_BOX_PAD);
	win->icon_size           = sc(win, MKGUI_ICON_SIZE);
	win->dialog_icon_size    = sc(win, 32);
	win->pathbar_height  = sc(win, MKGUI_PATHBAR_HEIGHT);
	win->toolbar_height  = sc(win, MKGUI_TOOLBAR_HEIGHT_DEFAULT);
	win->toolbar_btn_w   = sc(win, MKGUI_TOOLBAR_BTN_W);
	win->toolbar_sep_w   = sc(win, MKGUI_TOOLBAR_SEP_W);
	win->statusbar_height = sc(win, MKGUI_STATUSBAR_HEIGHT);
	for(uint32_t i = 0; i < win->widget_count; ++i) {
		win->widgets[i].label_tw = -1;
	}
}

// [=]===^=[ glyph_atlas_build ]====================================[=]
// Linear layout: each glyph's bitmap is stored contiguously (row-major,
// no stride padding). Better cache locality than 2D packing since the
// entire glyph fits in a few cache lines with no gaps.
static void glyph_atlas_build(struct mkgui_window *win) {
	uint32_t total = 0;
	for(uint32_t i = 0; i < MKGUI_GLYPH_COUNT; ++i) {
		struct mkgui_glyph *g = &win->glyphs[i];
		total += (uint32_t)g->width * (uint32_t)g->height;
	}

	free(glyph_atlas);
	glyph_atlas = (uint8_t *)calloc(total > 0 ? total : 1, 1);
	if(!glyph_atlas) {
		return;
	}

	uint32_t offset = 0;
	for(uint32_t i = 0; i < MKGUI_GLYPH_COUNT; ++i) {
		struct mkgui_glyph *g = &win->glyphs[i];
		g->atlas_offset = offset;
		uint32_t size = (uint32_t)g->width * (uint32_t)g->height;
		memcpy(&glyph_atlas[offset], glyph_staging[i], size);
		offset += size;
	}
}

// [=]===^=[ icon_atlas_rebuild ]====================================[=]
static void icon_atlas_rebuild(void) {
	if(icon_count == 0) {
		return;
	}

	uint32_t total = 0;
	for(uint32_t i = 0; i < icon_count; ++i) {
		total += (uint32_t)icons[i].w * (uint32_t)icons[i].h;
	}

	uint32_t *old_atlas = icon_atlas;
	icon_atlas = (uint32_t *)calloc(total > 0 ? total : 1, sizeof(uint32_t));
	if(!icon_atlas) {
		icon_atlas = old_atlas;
		return;
	}

	uint32_t offset = 0;
	for(uint32_t i = 0; i < icon_count; ++i) {
		struct mkgui_icon *ic = &icons[i];
		uint32_t size = (uint32_t)ic->w * (uint32_t)ic->h;
		uint32_t old_offset = ic->atlas_offset;
		ic->atlas_offset = offset;
		if(ic->pixels) {
			memcpy(&icon_atlas[offset], ic->pixels, size * sizeof(uint32_t));
			free(ic->pixels);
			ic->pixels = NULL;
		} else if(old_atlas && size > 0) {
			memcpy(&icon_atlas[offset], &old_atlas[old_offset], size * sizeof(uint32_t));
		}
		offset += size;
	}
	free(old_atlas);
}

#include "mkgui_render.c"

// ---------------------------------------------------------------------------
// Widget lookup helpers
// ---------------------------------------------------------------------------

// [=]===^=[ find_widget ]=======================================[=]
static struct mkgui_widget *find_widget(struct mkgui_window *win, uint32_t id) {
	for(uint32_t i = 0; i < win->widget_count; ++i) {
		if(win->widgets[i].id == id) {
			return &win->widgets[i];
		}
	}
	return NULL;
}

// [=]===^=[ find_widget_idx ]===================================[=]
static int32_t find_widget_idx(struct mkgui_window *win, uint32_t id) {
	for(uint32_t i = 0; i < win->widget_count; ++i) {
		if(win->widgets[i].id == id) {
			return (int32_t)i;
		}
	}
	return -1;
}

#define MKGUI_FIND_AUX(name, type, arr, cnt) \
static struct type *name(struct mkgui_window *win, uint32_t widget_id) { \
	for(uint32_t i = 0; i < win->cnt; ++i) { \
		if(win->arr[i].widget_id == widget_id) { \
			return &win->arr[i]; \
		} \
	} \
	return NULL; \
}

MKGUI_FIND_AUX(find_tabs_data,       mkgui_tabs_data,       tabs,        tab_count)
MKGUI_FIND_AUX(find_listv_data,      mkgui_listview_data,   listvs,      listv_count)
MKGUI_FIND_AUX(find_gridv_data,      mkgui_gridview_data,   gridviews,   gridview_count)
MKGUI_FIND_AUX(find_richlist_data,   mkgui_richlist_data,   richlists,   richlist_count)
MKGUI_FIND_AUX(find_input_data,      mkgui_input_data,      inputs,      input_count)
MKGUI_FIND_AUX(find_ipinput_data,    mkgui_ipinput_data,    ipinputs,    ipinput_count)
MKGUI_FIND_AUX(find_toggle_data,     mkgui_toggle_data,     toggles,     toggle_count)
MKGUI_FIND_AUX(find_combobox_data,   mkgui_combobox_data,   comboboxes,  combobox_count)
MKGUI_FIND_AUX(find_datepicker_data, mkgui_datepicker_data, datepickers, datepicker_count)
MKGUI_FIND_AUX(find_dropdown_data,   mkgui_dropdown_data,   dropdowns,   dropdown_count)
MKGUI_FIND_AUX(find_slider_data,     mkgui_slider_data,     sliders,     slider_count)
MKGUI_FIND_AUX(find_split_data,      mkgui_split_data,      splits,      split_count)
MKGUI_FIND_AUX(find_treeview_data,   mkgui_treeview_data,   treeviews,   treeview_count)
MKGUI_FIND_AUX(find_statusbar_data,  mkgui_statusbar_data,  statusbars,  statusbar_count)
MKGUI_FIND_AUX(find_spinbox_data,    mkgui_spinbox_data,    spinboxes,   spinbox_count)
MKGUI_FIND_AUX(find_progress_data,   mkgui_progress_data,   progress,    progress_count)
MKGUI_FIND_AUX(find_meter_data,     mkgui_meter_data,      meters,      meter_count)
MKGUI_FIND_AUX(find_textarea_data,   mkgui_textarea_data,   textareas,   textarea_count)
MKGUI_FIND_AUX(find_logview_data,    mkgui_logview_data,    logviews,    logview_count)
MKGUI_FIND_AUX(find_itemview_data,   mkgui_itemview_data,   itemviews,   itemview_count)
MKGUI_FIND_AUX(find_canvas_data,     mkgui_canvas_data,     canvases,    canvas_count)
MKGUI_FIND_AUX(find_pathbar_data,    mkgui_pathbar_data,    pathbars,    pathbar_count)

// ---------------------------------------------------------------------------
// Layout parent index (declared early for widget_visible)
// ---------------------------------------------------------------------------

static struct vm_arena layout_parent_arena;
static struct vm_arena layout_child_arena;
static struct vm_arena layout_sibling_arena;
static uint32_t *layout_parent;
static uint32_t *layout_first_child;
static uint32_t *layout_next_sibling;
static uint32_t layout_arr_cap;

static struct vm_arena layout_hash_arena;

// ---------------------------------------------------------------------------
// Widget visibility
// ---------------------------------------------------------------------------

// [=]===^=[ widget_visible ]====================================[=]
static uint32_t widget_visible(struct mkgui_window *win, uint32_t idx) {
	struct mkgui_widget *w = &win->widgets[idx];
	if(w->flags & MKGUI_HIDDEN) {
		return 0;
	}

	if(w->type == MKGUI_WINDOW) {
		return 1;
	}

	if(layout_arr_cap > 0 && idx < layout_arr_cap) {
		uint32_t pidx = layout_parent[idx];
		while(pidx < win->widget_count) {
			struct mkgui_widget *parent = &win->widgets[pidx];
			if(parent->flags & MKGUI_HIDDEN) {
				return 0;
			}

			if(parent->type == MKGUI_TAB) {
				uint32_t tabs_idx = layout_parent[pidx];
				if(tabs_idx < win->widget_count && win->widgets[tabs_idx].type == MKGUI_TABS) {
					struct mkgui_tabs_data *td = find_tabs_data(win, win->widgets[tabs_idx].id);
					if(td && td->active_tab != parent->id) {
						return 0;
					}
				}
			}

			if(parent->type == MKGUI_GROUP && (parent->style & MKGUI_GROUP_COLLAPSED)) {
				return 0;
			}

			if(parent->type == MKGUI_WINDOW) {
				break;
			}
			pidx = layout_parent[pidx];
		}
	} else {
		uint32_t pid = w->parent_id;
		while(pid != 0) {
			struct mkgui_widget *parent = find_widget(win, pid);
			if(!parent) {
				break;
			}

			if(parent->flags & MKGUI_HIDDEN) {
				return 0;
			}

			if(parent->type == MKGUI_TAB) {
				struct mkgui_widget *tabs_container = find_widget(win, parent->parent_id);
				if(tabs_container && tabs_container->type == MKGUI_TABS) {
					struct mkgui_tabs_data *td = find_tabs_data(win, tabs_container->id);
					if(td && td->active_tab != parent->id) {
						return 0;
					}
				}
			}

			if(parent->type == MKGUI_GROUP && (parent->style & MKGUI_GROUP_COLLAPSED)) {
				return 0;
			}
			pid = parent->parent_id;
		}
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
		case MKGUI_LOGVIEW:
		case MKGUI_ITEMVIEW:
		case MKGUI_TABS:
		case MKGUI_PATHBAR:
		case MKGUI_IPINPUT:
		case MKGUI_TOGGLE:
		case MKGUI_COMBOBOX:
		case MKGUI_DATEPICKER:
		case MKGUI_GRIDVIEW: {
			return 1;
		} break;

		case MKGUI_RICHLIST: {
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
static void dirty_all(struct mkgui_window *win) {
	win->dirty_full = 1;
	win->dirty = 1;
}

// [=]===^=[ dirty_add ]==========================================[=]
static void dirty_add(struct mkgui_window *win, int32_t x, int32_t y, int32_t w, int32_t h) {
	if(win->dirty_full) {
		return;
	}

	if(w <= 0 || h <= 0) {
		return;
	}

	for(uint32_t i = 0; i < win->dirty_count; ++i) {
		int32_t dx = win->dirty_rects[i].x;
		int32_t dy = win->dirty_rects[i].y;
		int32_t dw = win->dirty_rects[i].w;
		int32_t dh = win->dirty_rects[i].h;
		int32_t ux = dx < x ? dx : x;
		int32_t uy = dy < y ? dy : y;
		int32_t ur = (dx + dw) > (x + w) ? (dx + dw) : (x + w);
		int32_t ub = (dy + dh) > (y + h) ? (dy + dh) : (y + h);
		int32_t uw = ur - ux;
		int32_t uh = ub - uy;
		int64_t union_area = (int64_t)uw * uh;
		int64_t sum_area = (int64_t)dw * dh + (int64_t)w * h;
		if(union_area <= sum_area + sum_area / 2) {
			win->dirty_rects[i].x = ux;
			win->dirty_rects[i].y = uy;
			win->dirty_rects[i].w = uw;
			win->dirty_rects[i].h = uh;
			win->dirty = 1;
			return;
		}
	}

	if(win->dirty_count >= 32) {
		dirty_all(win);
		return;
	}

	win->dirty_rects[win->dirty_count].x = x;
	win->dirty_rects[win->dirty_count].y = y;
	win->dirty_rects[win->dirty_count].w = w;
	win->dirty_rects[win->dirty_count].h = h;
	++win->dirty_count;
	win->dirty = 1;
}

// [=]===^=[ dirty_widget ]========================================[=]
static void dirty_widget(struct mkgui_window *win, uint32_t idx) {
	dirty_add(win, win->rects[idx].x, win->rects[idx].y, win->rects[idx].w, win->rects[idx].h);
}

// [=]===^=[ dirty_widget_id ]=====================================[=]
static void dirty_widget_id(struct mkgui_window *win, uint32_t id) {
	int32_t idx = find_widget_idx(win, id);
	if(idx >= 0) {
		dirty_widget(win, (uint32_t)idx);
	}
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

// [=]===^=[ find_box_scroll ]==================================[=]
static struct mkgui_box_scroll *find_box_scroll(struct mkgui_window *win, uint32_t widget_id) {
	for(uint32_t i = 0; i < win->box_scroll_count; ++i) {
		if(win->box_scrolls[i].widget_id == widget_id) {
			return &win->box_scrolls[i];
		}
	}
	return NULL;
}

// ---------------------------------------------------------------------------
// Layout index (pre-built per layout pass for O(1) lookups)
// ---------------------------------------------------------------------------

static struct mkgui_layout_hash_entry {
	uint32_t id;
	uint32_t idx;
} *layout_id_map;
static uint32_t layout_hash_size;
static uint32_t layout_hash_mask;

// [=]===^=[ layout_find_idx ]====================================[=]
static uint32_t layout_find_idx(uint32_t id) {
	uint32_t h = (id * 2654435761u) & layout_hash_mask;
	for(;;) {
		if(layout_id_map[h].idx == UINT32_MAX) {
			return UINT32_MAX;
		}

		if(layout_id_map[h].id == id) {
			return layout_id_map[h].idx;
		}
		h = (h + 1) & layout_hash_mask;
	}
}

// [=]===^=[ layout_arena_init ]===================================[=]
static uint32_t layout_arena_init(void) {
	if(layout_parent_arena.base) {
		return 1;
	}
	size_t per_reserve = (size_t)MKGUI_VM_MAX_WIDGETS * sizeof(uint32_t);
	if(!vm_arena_create(&layout_parent_arena, per_reserve)) {
		return 0;
	}

	if(!vm_arena_create(&layout_child_arena, per_reserve)) {
		vm_arena_destroy(&layout_parent_arena);
		return 0;
	}

	if(!vm_arena_create(&layout_sibling_arena, per_reserve)) {
		vm_arena_destroy(&layout_parent_arena);
		vm_arena_destroy(&layout_child_arena);
		return 0;
	}
	layout_parent = (uint32_t *)layout_parent_arena.base;
	layout_first_child = (uint32_t *)layout_child_arena.base;
	layout_next_sibling = (uint32_t *)layout_sibling_arena.base;

	size_t hash_reserve = (size_t)MKGUI_VM_MAX_HASH_SLOTS * sizeof(struct mkgui_layout_hash_entry);
	if(!vm_arena_create(&layout_hash_arena, hash_reserve)) {
		vm_arena_destroy(&layout_parent_arena);
		vm_arena_destroy(&layout_child_arena);
		vm_arena_destroy(&layout_sibling_arena);
		layout_parent = NULL;
		layout_first_child = NULL;
		layout_next_sibling = NULL;
		return 0;
	}
	layout_id_map = (struct mkgui_layout_hash_entry *)layout_hash_arena.base;
	return 1;
}

// [=]===^=[ layout_arena_fini ]===================================[=]
static void layout_arena_fini(void) {
	vm_arena_destroy(&layout_parent_arena);
	vm_arena_destroy(&layout_child_arena);
	vm_arena_destroy(&layout_sibling_arena);
	vm_arena_destroy(&layout_hash_arena);
	layout_parent = NULL;
	layout_first_child = NULL;
	layout_next_sibling = NULL;
	layout_id_map = NULL;
	layout_arr_cap = 0;
	layout_hash_size = 0;
	layout_hash_mask = 0;
}

// ---------------------------------------------------------------------------
// Shared layout context (used by both runtime and editor)
// ---------------------------------------------------------------------------

struct layout_ctx {
	struct mkgui_widget *widgets;
	struct { int32_t x, y, w, h; } *rects;
	uint32_t *first_child;
	uint32_t *next_sibling;
	uint32_t *parent;
	struct mkgui_layout_hash_entry *id_map;
	uint32_t hash_mask;
	uint32_t widget_count;
	uint32_t has_scroll;
	size_t widget_stride;
};

// [=]===^=[ lw_get ]================================================[=]
static struct mkgui_widget *lw_get(struct layout_ctx *lc, uint32_t idx) {
	return (struct mkgui_widget *)((char *)lc->widgets + idx * lc->widget_stride);
}

// [=]===^=[ lw_sw ]=================================================[=]
static inline int32_t lw_sw(struct mkgui_window *win, int32_t val) {
	return val > 0 ? sc(win, val) : val;
}

// [=]===^=[ lc_find_idx ]===========================================[=]
static uint32_t lc_find_idx(struct layout_ctx *lc, uint32_t id) {
	// knuth multiplicative hash (golden ratio constant 0x9e3779b1) for good
	// distribution across power-of-two table sizes
	uint32_t h = (id * 2654435761u) & lc->hash_mask;
	for(;;) {
		if(lc->id_map[h].idx == UINT32_MAX) {
			return UINT32_MAX;
		}

		if(lc->id_map[h].id == id) {
			return lc->id_map[h].idx;
		}
		h = (h + 1) & lc->hash_mask;
	}
}

// [=]===^=[ lc_build_index ]========================================[=]
static void lc_build_index(struct layout_ctx *lc) {
	memset(lc->id_map, 0xff, (size_t)(lc->hash_mask + 1) * sizeof(*lc->id_map));
	for(uint32_t i = 0; i < lc->widget_count; ++i) {
		struct mkgui_widget *w = lw_get(lc, i);
		uint32_t h = (w->id * 2654435761u) & lc->hash_mask;
		while(lc->id_map[h].idx != UINT32_MAX) {
			h = (h + 1) & lc->hash_mask;
		}
		lc->id_map[h].id = w->id;
		lc->id_map[h].idx = i;
		lc->first_child[i] = UINT32_MAX;
		lc->next_sibling[i] = UINT32_MAX;
	}
	// build child linked lists by iterating in reverse: each child becomes
	// the new list head, so forward iteration later yields declaration order
	// without needing a tail pointer
	uint32_t i = lc->widget_count;
	while(i > 0) {
		--i;
		uint32_t pidx = lc_find_idx(lc, lw_get(lc, i)->parent_id);
		lc->parent[i] = pidx;
		if(pidx < lc->widget_count && pidx != i) {
			lc->next_sibling[i] = lc->first_child[pidx];
			lc->first_child[pidx] = i;
		}
	}
}

// [=]===^=[ layout_build_index ]==================================[=]
static void layout_build_index(struct mkgui_window *win) {
	if(win->widget_count > layout_arr_cap) {
		uint32_t nc = (win->widget_count + MKGUI_GROW_WIDGETS - 1) & ~(uint32_t)(MKGUI_GROW_WIDGETS - 1);
		if(nc > MKGUI_VM_MAX_WIDGETS) {
			return;
		}
		size_t needed = (size_t)nc * sizeof(uint32_t);
		if(!vm_arena_ensure(&layout_parent_arena, needed) || !vm_arena_ensure(&layout_child_arena, needed) || !vm_arena_ensure(&layout_sibling_arena, needed)) {
			return;
		}
		layout_arr_cap = nc;
	}
	uint32_t need = win->widget_count * 2;
	if(need < 256) {
		need = 256;
	}
	uint32_t hs = 256;
	while(hs < need) {
		hs <<= 1;
	}

	if(hs > layout_hash_size) {
		if(hs > MKGUI_VM_MAX_HASH_SLOTS) {
			return;
		}

		if(!vm_arena_ensure(&layout_hash_arena, (size_t)hs * sizeof(struct mkgui_layout_hash_entry))) {
			return;
		}
		layout_hash_size = hs;
		layout_hash_mask = hs - 1;
	}
	struct layout_ctx lc = {
		.widgets = win->widgets, .rects = (void *)win->rects,
		.first_child = layout_first_child, .next_sibling = layout_next_sibling,
		.parent = layout_parent, .id_map = layout_id_map, .hash_mask = layout_hash_mask,
		.widget_count = win->widget_count, .widget_stride = sizeof(struct mkgui_widget)
	};
	lc_build_index(&lc);
}


// Single-line controls hold their natural height in a VBOX rather than
// stretching to share leftover space. Setting weight > 0 explicitly opts
// the widget back into flex behaviour. This matches Qt/GTK size-policy
// conventions and prevents text carets from rendering at half-window
// height when a user drops an input next to a weighted sibling.
// [=]===^=[ widget_vflex_default ]==================================[=]
static uint32_t widget_vflex_default(uint32_t widget_type) {
	switch(widget_type) {
		case MKGUI_BUTTON:
		case MKGUI_LABEL:
		case MKGUI_INPUT:
		case MKGUI_CHECKBOX:
		case MKGUI_DROPDOWN:
		case MKGUI_SLIDER:
		case MKGUI_SCROLLBAR:
		case MKGUI_SPINBOX:
		case MKGUI_RADIO:
		case MKGUI_PROGRESS:
		case MKGUI_METER:
		case MKGUI_SPINNER:
		case MKGUI_PATHBAR:
		case MKGUI_IPINPUT:
		case MKGUI_TOGGLE:
		case MKGUI_COMBOBOX:
		case MKGUI_DATEPICKER:
		case MKGUI_DIVIDER: {
			return 0;
		}

		default: {
			return 1;
		}
	}
}

// [=]===^=[ vbox_child_is_fixed ]==================================[=]
static uint32_t vbox_child_is_fixed(struct mkgui_widget *jw) {
	if(jw->flags & MKGUI_FIXED) {
		return 1;
	}

	if(jw->type == MKGUI_GROUP && (jw->style & MKGUI_GROUP_COLLAPSIBLE)) {
		return 1;
	}

	if(jw->weight == 0 && !widget_vflex_default(jw->type)) {
		return 1;
	}
	return 0;
}

// [=]===^=[ natural_height ]=======================================[=]
static int32_t natural_height(struct mkgui_window *win, uint32_t widget_type) {
	switch(widget_type) {
		case MKGUI_BUTTON: {
			return win->font_height + sc(win, MKGUI_NAT_BUTTON_VPAD);
		}

		case MKGUI_INPUT:
		case MKGUI_COMBOBOX:
		case MKGUI_DROPDOWN:
		case MKGUI_SPINBOX:
		case MKGUI_DATEPICKER:
		case MKGUI_IPINPUT:
		case MKGUI_PATHBAR: {
			return win->font_height + sc(win, MKGUI_NAT_INPUT_VPAD);
		}

		case MKGUI_CHECKBOX:
		case MKGUI_RADIO:
		case MKGUI_TOGGLE: {
			return win->font_height + sc(win, MKGUI_NAT_CHECKBOX_VPAD);
		}

		case MKGUI_LABEL: {
			return win->font_height + sc(win, MKGUI_NAT_LABEL_VPAD);
		}

		case MKGUI_SLIDER:
		case MKGUI_SCROLLBAR: {
			return sc(win, MKGUI_NAT_SLIDER_H);
		}

		case MKGUI_PROGRESS:
		case MKGUI_METER: {
			return sc(win, MKGUI_NAT_PROGRESS_H);
		}

		case MKGUI_DIVIDER: {
			return sc(win, MKGUI_NAT_DIVIDER_H);
		}

		case MKGUI_LISTVIEW:
		case MKGUI_GRIDVIEW:
		case MKGUI_RICHLIST:
		case MKGUI_TREEVIEW:
		case MKGUI_ITEMVIEW:
		case MKGUI_TEXTAREA:
		case MKGUI_LOGVIEW: {
			return win->row_height * 3;
		}

		case MKGUI_TABS: {
			return win->tab_height + win->row_height * 2;
		}

		case MKGUI_HSPLIT:
		case MKGUI_VSPLIT: {
			return win->split_min_px * 2 + win->split_thick;
		}

		case MKGUI_IMAGE:
		case MKGUI_CANVAS:
		case MKGUI_GLVIEW:
		case MKGUI_SPINNER: {
			return sc(win, MKGUI_NAT_CANVAS_H);
		}

		case MKGUI_VBOX:
		case MKGUI_HBOX:
		case MKGUI_FORM:
		case MKGUI_PANEL:
		case MKGUI_GROUP: {
			return win->row_height;
		}

		default: {
			return 0;
		}
	}
}

// [=]===^=[ natural_width ]=======================================[=]
static int32_t natural_width(struct mkgui_window *win, struct mkgui_widget *w) {
	int32_t tw;
	switch(w->type) {
		case MKGUI_BUTTON: {
			tw = w->label[0] ? label_text_width(win, w) : 0;
			int32_t iw = w->icon[0] ? (win->icon_size + (w->label[0] ? sc(win, MKGUI_NAT_BUTTON_ICON_GAP) : 0)) : 0;
			return tw + iw + sc(win, MKGUI_NAT_BUTTON_HPAD);
		}

		case MKGUI_LABEL: {
			tw = label_text_width(win, w);
			return tw + sc(win, MKGUI_NAT_LABEL_HPAD);
		}

		case MKGUI_CHECKBOX:
		case MKGUI_RADIO: {
			tw = label_text_width(win, w);
			return sc(win, MKGUI_NAT_CHECKBOX_BOX_W) + tw;
		}

		case MKGUI_TOGGLE: {
			tw = label_text_width(win, w);
			return sc(win, MKGUI_NAT_TOGGLE_BOX_W) + tw;
		}

		case MKGUI_DIVIDER: {
			return sc(win, MKGUI_NAT_DIVIDER_W);
		}

		default: {
			return sc(win, MKGUI_NAT_DEFAULT_W);
		}
	}
}

// [=]===^=[ lc_measure_container ]================================[=]
static int32_t lc_measure_container(struct mkgui_window *win, struct layout_ctx *lc, uint32_t idx, uint32_t axis) {
	struct mkgui_widget *w = lw_get(lc, idx);
	if(w->type == MKGUI_GROUP) {
		int32_t gtop = win->font_height + 4;
		if(w->style & MKGUI_GROUP_COLLAPSED) {
			return (axis == 1) ? gtop : 0;
		}
		int32_t gpad = win->box_pad;
		for(uint32_t j = lc->first_child[idx]; j < lc->widget_count; j = lc->next_sibling[j]) {
			uint32_t ct = lw_get(lc, j)->type;
			if(ct == MKGUI_VBOX || ct == MKGUI_HBOX || ct == MKGUI_FORM || ct == MKGUI_PANEL) {
				int32_t inner = lc_measure_container(win, lc, j, axis);
				if(axis == 1) {
					return inner + gtop + gpad;
				}
				return inner + gpad * 2;
			}
		}
		return 0;
	}

	uint32_t is_vbox = (w->type == MKGUI_VBOX || w->type == MKGUI_PANEL);
	uint32_t is_hbox = (w->type == MKGUI_HBOX);
	uint32_t is_form = (w->type == MKGUI_FORM);
	if(!is_vbox && !is_hbox && !is_form) {
		return 0;
	}

	if(is_form) {
		uint32_t pair_count = 0;
		for(uint32_t j = lc->first_child[idx]; j < lc->widget_count; j = lc->next_sibling[j]) {
			++pair_count;
		}
		uint32_t rows = (pair_count + 1) / 2;
		int32_t row_h = win->font_height + 10;
		int32_t h = (int32_t)rows * row_h + (rows > 1 ? (int32_t)(rows - 1) * win->box_gap : 0);
		return h;
	}

	int32_t main_total = 0;
	int32_t cross_max = 0;
	uint32_t visible = 0;
	for(uint32_t j = lc->first_child[idx]; j < lc->widget_count; j = lc->next_sibling[j]) {
		struct mkgui_widget *jw = lw_get(lc, j);
		if(jw->flags & MKGUI_HIDDEN) {
			continue;
		}
		++visible;
		int32_t child_main;
		int32_t child_cross;
		uint32_t ct = jw->type;
		if(is_vbox) {
			child_main = lw_sw(win, jw->h);
			child_cross = lw_sw(win, jw->w);
		} else {
			child_main = lw_sw(win, jw->w);
			child_cross = lw_sw(win, jw->h);
		}

		if(child_main == 0 && (ct == MKGUI_VBOX || ct == MKGUI_HBOX || ct == MKGUI_FORM || ct == MKGUI_GROUP || ct == MKGUI_PANEL)) {
			child_main = lc_measure_container(win, lc, j, is_vbox ? 1 : 0);
		}

		if(child_main == 0) {
			if(is_vbox) {
				child_main = natural_height(win, ct);
			} else {
				child_main = natural_width(win, jw);
			}
		}

		if(child_cross == 0 && (ct == MKGUI_VBOX || ct == MKGUI_HBOX || ct == MKGUI_FORM || ct == MKGUI_GROUP || ct == MKGUI_PANEL)) {
			child_cross = lc_measure_container(win, lc, j, is_vbox ? 0 : 1);
		}

		if(child_cross == 0) {
			if(is_vbox) {
				child_cross = natural_width(win, jw);
			} else {
				child_cross = natural_height(win, ct);
			}
		}
		main_total += child_main;
		if(child_cross > cross_max) {
			cross_max = child_cross;
		}
	}
	int32_t gap_total = visible > 1 ? (int32_t)(visible - 1) * win->box_gap : 0;
	main_total += gap_total;

	// only apply padding at outermost box level to avoid double-padding in
	// nested containers; bordered boxes always get padding for visual separation
	uint32_t has_pad = 0;
	if(!(w->flags & MKGUI_NO_PAD)) {
		uint32_t gpidx = lc->parent[idx];
		uint32_t nested = 0;
		if(gpidx < lc->widget_count) {
			uint32_t gpt = lw_get(lc, gpidx)->type;
			if(gpt == MKGUI_VBOX || gpt == MKGUI_HBOX || gpt == MKGUI_FORM || gpt == MKGUI_GROUP || gpt == MKGUI_TABS) {
				nested = 1;
			}
		}

		if(!nested || (w->style & STYLE_BORDER_BIT)) {
			has_pad = 1;
		}
	}

	if(has_pad) {
		main_total += win->box_pad * 2;
		cross_max += win->box_pad * 2;
	}

	if(axis == 0) {
		return is_vbox ? cross_max : main_total;
	}
	return is_vbox ? main_total : cross_max;
}

// [=]===^=[ measure_container ]===================================[=]
static int32_t measure_container(struct mkgui_window *win, uint32_t idx, uint32_t axis) {
	struct layout_ctx lc = {
		.widgets = win->widgets, .rects = (void *)win->rects,
		.first_child = layout_first_child, .next_sibling = layout_next_sibling,
		.parent = layout_parent, .widget_count = win->widget_count,
		.has_scroll = 1, .widget_stride = sizeof(struct mkgui_widget)
	};
	return lc_measure_container(win, &lc, idx, axis);
}

// [=]===^=[ lc_layout_node ]=====================================[=]
static void lc_layout_node(struct mkgui_window *win, struct layout_ctx *lc, uint32_t idx) {
	struct mkgui_widget *w = lw_get(lc, idx);

	int32_t px = lc->rects[idx].x;
	int32_t py = lc->rects[idx].y;
	int32_t pw = lc->rects[idx].w;
	int32_t ph = lc->rects[idx].h;

	if(w->type == MKGUI_WINDOW) {
		int32_t top_y = py;
		int32_t bot_y = py + ph;
		uint32_t chrome[16];
		uint32_t chrome_count = 0;
		for(uint32_t c = lc->first_child[idx]; c < lc->widget_count; c = lc->next_sibling[c]) {
			struct mkgui_widget *cw = lw_get(lc, c);
			uint32_t ct = cw->type;
			if(ct == MKGUI_MENU || ct == MKGUI_TOOLBAR || ct == MKGUI_STATUSBAR || (ct == MKGUI_PATHBAR && lw_sw(win, cw->h) == 0)) {
				if(chrome_count < 16) {
					chrome[chrome_count++] = c;
				}
			}
		}
		for(uint32_t i = 0; i < chrome_count; ++i) {
			uint32_t c = chrome[i];
			if(lw_get(lc, c)->type == MKGUI_MENU) {
				lc->rects[c].x = px;
				lc->rects[c].y = top_y;
				lc->rects[c].w = pw;
				lc->rects[c].h = win->menu_height;
				top_y += win->menu_height;
			}
		}
		for(uint32_t i = 0; i < chrome_count; ++i) {
			uint32_t c = chrome[i];
			struct mkgui_widget *cw = lw_get(lc, c);
			if(cw->type == MKGUI_TOOLBAR) {
				uint32_t tb_mode = cw->style & MKGUI_TOOLBAR_MODE_MASK;
				int32_t th;
				if(tb_mode == MKGUI_TOOLBAR_TEXT_ONLY) {
					th = win->font_height + 10;
				} else {
					uint32_t tb_has_icons = 0;
					for(uint32_t j = lc->first_child[c]; j < lc->widget_count; j = lc->next_sibling[j]) {
						if(lw_get(lc, j)->icon[0]) {
							tb_has_icons = 1;
							break;
						}
					}

					if(tb_has_icons && win->icon_size > 0) {
						int32_t ih = win->icon_size;
						th = (tb_mode == MKGUI_TOOLBAR_ICONS_ONLY) ? ih + 10 : ((ih > win->font_height ? ih : win->font_height) + 10);
					} else {
						th = win->font_height + 10;
					}
				}
				lc->rects[c].x = px;
				lc->rects[c].y = top_y;
				lc->rects[c].w = pw;
				lc->rects[c].h = th;
				top_y += th;
			}
		}
		for(uint32_t i = 0; i < chrome_count; ++i) {
			uint32_t c = chrome[i];
			struct mkgui_widget *cw = lw_get(lc, c);
			if(cw->type == MKGUI_PATHBAR) {
				lc->rects[c].x = px + lw_sw(win, cw->margin_l);
				lc->rects[c].y = top_y;
				lc->rects[c].w = pw - lw_sw(win, cw->margin_l) - lw_sw(win, cw->margin_r);
				lc->rects[c].h = win->pathbar_height;
				top_y += win->pathbar_height;
			}
		}
		for(uint32_t i = 0; i < chrome_count; ++i) {
			uint32_t c = chrome[i];
			if(lw_get(lc, c)->type == MKGUI_STATUSBAR) {
				bot_y -= win->statusbar_height;
				lc->rects[c].x = px;
				lc->rects[c].y = bot_y;
				lc->rects[c].w = pw;
				lc->rects[c].h = win->statusbar_height;
			}
		}
		py = top_y;
		ph = bot_y - top_y;
	}

	if(w->type == MKGUI_TAB) {
		int32_t tab_inset = sc(win, MKGUI_TAB_INSET);
		px += tab_inset;
		py += win->tab_height + tab_inset;
		pw -= tab_inset * 2;
		ph -= win->tab_height + tab_inset * 2;
	}

	if(w->type == MKGUI_GROUP) {
		if(w->style & MKGUI_GROUP_COLLAPSED) {
			return;
		}
		int32_t gtop = win->font_height + 4;
		int32_t gpad = win->box_pad;
		px += gpad;
		py += gtop;
		pw -= gpad * 2;
		ph -= gtop + gpad;
	}

	// suppress padding for nested containers to avoid compounding insets;
	// bordered boxes override this to maintain visual separation
	uint32_t box_has_pad = 0;
	uint32_t is_panel_vbox = (w->type == MKGUI_PANEL);
	if(w->type == MKGUI_VBOX || w->type == MKGUI_HBOX || w->type == MKGUI_FORM || is_panel_vbox) {
		if(!(w->flags & MKGUI_NO_PAD)) {
			uint32_t gpidx = lc->parent[idx];
			uint32_t nested = 0;
			if(gpidx < lc->widget_count) {
				uint32_t gpt = lw_get(lc, gpidx)->type;
				if(gpt == MKGUI_VBOX || gpt == MKGUI_HBOX || gpt == MKGUI_FORM || gpt == MKGUI_GROUP || gpt == MKGUI_TABS || gpt == MKGUI_PANEL) {
					nested = 1;
				}
			}

			if(!nested || (w->style & STYLE_BORDER_BIT)) {
				px += win->box_pad;
				py += win->box_pad;
				pw -= win->box_pad * 2;
				ph -= win->box_pad * 2;
				box_has_pad = 1;
			}
		}

		if(w->type == MKGUI_VBOX || is_panel_vbox) {
			uint32_t scrollable = lc->has_scroll && (w->flags & MKGUI_SCROLL) ? 1 : 0;
			struct mkgui_box_scroll *bs = scrollable ? find_box_scroll(win, w->id) : NULL;
			int32_t fixed_total = 0;
			int32_t min_total = 0;
			uint32_t weight_total = 0;
			uint32_t child_count = 0;
			for(uint32_t j = lc->first_child[idx]; j < lc->widget_count; j = lc->next_sibling[j]) {
				struct mkgui_widget *jw = lw_get(lc, j);
				if(jw->flags & MKGUI_HIDDEN) {
					continue;
				}
				++child_count;
				// collapsible groups are deterministically sized; single-line
				// leaves with weight==0 hold their natural height instead of
				// eating leftover space (see widget_vflex_default comment)
				uint32_t treat_fixed = vbox_child_is_fixed(jw);
				if(treat_fixed) {
					int32_t fh = lw_sw(win, jw->h);
					if(jw->type == MKGUI_GROUP && (jw->style & MKGUI_GROUP_COLLAPSED)) {
						fh = win->font_height + 4;
					}
					uint32_t ct = jw->type;
					if(fh == 0 && (ct == MKGUI_VBOX || ct == MKGUI_HBOX || ct == MKGUI_FORM || ct == MKGUI_GROUP || ct == MKGUI_PANEL)) {
						fh = lc_measure_container(win, lc, j, 1);
					}

					if(fh == 0) {
						fh = natural_height(win, ct);
					}
					lc->rects[j].h = fh;
					fixed_total += fh;
				} else {
					uint32_t ew = jw->weight > 0 ? jw->weight : 1;
					int32_t minh = lw_sw(win, jw->h);
					if(minh == 0) {
						uint32_t ct = jw->type;
						if(ct == MKGUI_VBOX || ct == MKGUI_HBOX || ct == MKGUI_FORM || ct == MKGUI_GROUP || ct == MKGUI_PANEL) {
							minh = lc_measure_container(win, lc, j, 1);
						}

						if(minh == 0) {
							minh = natural_height(win, jw->type);
						}
					}
					min_total += minh;
					weight_total += ew;
				}
			}
			int32_t gap_total = child_count > 1 ? (int32_t)(child_count - 1) * win->box_gap : 0;
			int32_t content_h = fixed_total + min_total + gap_total;
			uint32_t needs_scroll = scrollable && (content_h > ph);
			// scrollbar steals width from content area after initial measurement
			if(needs_scroll) {
				pw -= win->scrollbar_w;
			}

			if(bs) {
				bs->content_h = content_h;
				if(box_has_pad) {
					bs->content_h += win->box_pad * 2;
				}
			}
			int32_t remaining = ph - fixed_total - min_total - gap_total;
			// when scrolling, content already exceeds the viewport; clamp
			// remaining to zero so weight distribution doesn't go negative
			if(needs_scroll) {
				if(remaining < 0) {
					remaining = 0;
				}
			}
			int32_t scroll_off = (bs && needs_scroll) ? bs->scroll_y : 0;
			int32_t cy = py - scroll_off;
			for(uint32_t j = lc->first_child[idx]; j < lc->widget_count; j = lc->next_sibling[j]) {
				struct mkgui_widget *jw = lw_get(lc, j);
				if(jw->flags & MKGUI_HIDDEN) {
					continue;
				}
				int32_t ch;
				uint32_t child_fixed = vbox_child_is_fixed(jw);
				if(child_fixed) {
					ch = lc->rects[j].h;
				} else {
					uint32_t wt = jw->weight > 0 ? jw->weight : 1;
					int32_t base_h = lw_sw(win, jw->h);
					if(base_h == 0) {
						uint32_t ct = jw->type;
						if(ct == MKGUI_VBOX || ct == MKGUI_HBOX || ct == MKGUI_FORM || ct == MKGUI_GROUP || ct == MKGUI_PANEL) {
							base_h = lc_measure_container(win, lc, j, 1);
						}

						if(base_h == 0) {
							base_h = natural_height(win, jw->type);
						}
					}
					// 64-bit intermediate prevents overflow when remaining * weight exceeds 2^31
					ch = base_h + (weight_total > 0 ? (int32_t)((int64_t)remaining * (int32_t)wt / (int32_t)weight_total) : 0);
				}
				// floor flexible children at content size so they never collapse
				// below usable dimensions; fixed children keep their declared size
				if(!child_fixed) {
					uint32_t ct = jw->type;
					int32_t min_ch;
					if(ct == MKGUI_VBOX || ct == MKGUI_HBOX || ct == MKGUI_FORM || ct == MKGUI_GROUP || ct == MKGUI_PANEL) {
						min_ch = lc_measure_container(win, lc, j, 1);
					} else {
						min_ch = natural_height(win, ct);
					}

					if(min_ch < 1) {
						min_ch = 1;
					}
					if(ch < min_ch) {
						ch = min_ch;
					}
				}
				int32_t cx_child = px;
				int32_t cw_child = pw;
				uint32_t align = jw->flags & MKGUI_ALIGN_MASK;
				if(align && lw_sw(win, jw->w) > 0) {
					if(align == MKGUI_ALIGN_START) {
						cw_child = lw_sw(win, jw->w);
					} else if(align == MKGUI_ALIGN_CENTER) {
						cw_child = lw_sw(win, jw->w);
						cx_child = px + (pw - cw_child) / 2;
					} else if(align == MKGUI_ALIGN_END) {
						cw_child = lw_sw(win, jw->w);
						cx_child = px + pw - cw_child;
					}
				}
				lc->rects[j].x = cx_child;
				lc->rects[j].y = cy;
				lc->rects[j].w = cw_child;
				lc->rects[j].h = ch;
				cy += ch + win->box_gap;
			}

		} else if(w->type == MKGUI_HBOX) {
			uint32_t scrollable = lc->has_scroll && (w->flags & MKGUI_SCROLL) ? 1 : 0;
			struct mkgui_box_scroll *bs = scrollable ? find_box_scroll(win, w->id) : NULL;
			int32_t fixed_total = 0;
			int32_t min_total = 0;
			uint32_t weight_total = 0;
			uint32_t child_count = 0;
			for(uint32_t j = lc->first_child[idx]; j < lc->widget_count; j = lc->next_sibling[j]) {
				struct mkgui_widget *jw = lw_get(lc, j);
				if(jw->flags & MKGUI_HIDDEN) {
					continue;
				}
				++child_count;
				if(jw->flags & MKGUI_FIXED) {
					int32_t fw = lw_sw(win, jw->w);
					uint32_t ct = jw->type;
					if(fw == 0 && (ct == MKGUI_VBOX || ct == MKGUI_HBOX || ct == MKGUI_FORM || ct == MKGUI_GROUP || ct == MKGUI_PANEL)) {
						fw = lc_measure_container(win, lc, j, 0);
					}

					if(fw == 0) {
						fw = natural_width(win, jw);
					}
					lc->rects[j].w = fw;
					fixed_total += fw;
				} else {
					uint32_t ew = jw->weight > 0 ? jw->weight : 1;
					int32_t minw = lw_sw(win, jw->w);
					if(minw == 0) {
						uint32_t ct = jw->type;
						if(ct == MKGUI_VBOX || ct == MKGUI_HBOX || ct == MKGUI_FORM || ct == MKGUI_GROUP || ct == MKGUI_PANEL) {
							minw = lc_measure_container(win, lc, j, 0);
						}

						if(minw == 0) {
							minw = natural_width(win, jw);
						}
					}
					min_total += minw;
					weight_total += ew;
				}
			}
			int32_t gap_total = child_count > 1 ? (int32_t)(child_count - 1) * win->box_gap : 0;
			int32_t content_w = fixed_total + min_total + gap_total;
			uint32_t needs_scroll = scrollable && (content_w > pw);
			if(needs_scroll) {
				ph -= win->scrollbar_w;
			}

			if(bs) {
				bs->content_w = content_w;
			}
			int32_t remaining = pw - fixed_total - min_total - gap_total;
			if(needs_scroll) {
				if(remaining < 0) {
					remaining = 0;
				}
			}
			int32_t scroll_off = (bs && needs_scroll) ? bs->scroll_x : 0;
			int32_t cx = px - scroll_off;
			for(uint32_t j = lc->first_child[idx]; j < lc->widget_count; j = lc->next_sibling[j]) {
				struct mkgui_widget *jw = lw_get(lc, j);
				if(jw->flags & MKGUI_HIDDEN) {
					continue;
				}
				int32_t cw;
				if(jw->flags & MKGUI_FIXED) {
					cw = lc->rects[j].w;
				} else {
					uint32_t wt = jw->weight > 0 ? jw->weight : 1;
					int32_t base_w = lw_sw(win, jw->w);
					if(base_w == 0) {
						uint32_t ct = jw->type;
						if(ct == MKGUI_VBOX || ct == MKGUI_HBOX || ct == MKGUI_FORM || ct == MKGUI_GROUP || ct == MKGUI_PANEL) {
							base_w = lc_measure_container(win, lc, j, 0);
						}

						if(base_w == 0) {
							base_w = natural_width(win, jw);
						}
					}
					cw = base_w + (weight_total > 0 ? (int32_t)((int64_t)remaining * (int32_t)wt / (int32_t)weight_total) : 0);
				}

				if(!(jw->flags & MKGUI_FIXED)) {
					uint32_t ct = jw->type;
					int32_t min_cw;
					if(ct == MKGUI_VBOX || ct == MKGUI_HBOX || ct == MKGUI_FORM || ct == MKGUI_GROUP || ct == MKGUI_PANEL) {
						min_cw = lc_measure_container(win, lc, j, 0);
					} else {
						min_cw = natural_width(win, jw);
					}

					if(min_cw < 1) {
						min_cw = 1;
					}
					if(cw < min_cw) {
						cw = min_cw;
					}
				}
				// Cross-axis (height) placement. Single-line leaves hold their
				// natural height and center vertically so e.g. an OK/Cancel row
				// doesn't stretch its buttons to fill the HBOX. Multi-line leaves
				// and containers fill the full height (existing behaviour).
				int32_t cy_child = py;
				int32_t ch_child = ph;
				uint32_t align = jw->flags & MKGUI_ALIGN_MASK;
				int32_t explicit_h = lw_sw(win, jw->h);
				if(align && explicit_h > 0) {
					ch_child = explicit_h;
					if(align == MKGUI_ALIGN_CENTER) {
						cy_child = py + (ph - ch_child) / 2;
					} else if(align == MKGUI_ALIGN_END) {
						cy_child = py + ph - ch_child;
					}
				} else if(!widget_vflex_default(jw->type)) {
					int32_t nat_h = explicit_h > 0 ? explicit_h : natural_height(win, jw->type);
					if(nat_h > 0 && nat_h < ph) {
						ch_child = nat_h;
						uint32_t eff_align = align ? align : MKGUI_ALIGN_CENTER;
						if(eff_align == MKGUI_ALIGN_CENTER) {
							cy_child = py + (ph - ch_child) / 2;
						} else if(eff_align == MKGUI_ALIGN_END) {
							cy_child = py + ph - ch_child;
						}
					}
				}
				lc->rects[j].x = cx;
				lc->rects[j].y = cy_child;
				lc->rects[j].w = cw;
				lc->rects[j].h = ch_child;
				cx += cw + win->box_gap;
			}

		} else {
			int32_t label_w = 0;
			uint32_t pair_idx = 0;
			for(uint32_t j = lc->first_child[idx]; j < lc->widget_count; j = lc->next_sibling[j]) {
				if((pair_idx & 1) == 0) {
					int32_t tw = label_text_width(win, lw_get(lc, j)) + 8;
					if(tw > label_w) {
						label_w = tw;
					}
				}
				++pair_idx;
			}
			pair_idx = 0;
			for(uint32_t j = lc->first_child[idx]; j < lc->widget_count; j = lc->next_sibling[j]) {
				uint32_t row = pair_idx / 2;
				uint32_t row_h = (uint32_t)(win->font_height + 10);
				if((pair_idx & 1) == 0) {
					lc->rects[j].x = px;
					lc->rects[j].y = py + (int32_t)(row * (row_h + (uint32_t)win->box_gap));
					lc->rects[j].w = label_w;
					lc->rects[j].h = (int32_t)row_h;
				} else {
					lc->rects[j].x = px + label_w;
					lc->rects[j].y = py + (int32_t)(row * (row_h + (uint32_t)win->box_gap));
					lc->rects[j].w = pw - label_w;
					lc->rects[j].h = (int32_t)row_h;
				}
				++pair_idx;
			}
		}

	} else if(w->type == MKGUI_TOOLBAR) {
		// toolbar children are positioned by render_toolbar

	} else {
		if(w->type == MKGUI_HSPLIT || w->type == MKGUI_VSPLIT) {
			struct mkgui_split_data *sd = find_split_data(win, w->id);
			float ratio = sd ? sd->ratio : 0.5f;
			if(w->type == MKGUI_HSPLIT) {
				int32_t split = (int32_t)(ph * ratio);
				for(uint32_t c = lc->first_child[idx]; c < lc->widget_count; c = lc->next_sibling[c]) {
					struct mkgui_widget *cw = lw_get(lc, c);
					if(cw->flags & MKGUI_HIDDEN) {
						continue;
					}

					if(cw->flags & MKGUI_REGION_TOP) {
						lc->rects[c].x = px;
						lc->rects[c].y = py;
						lc->rects[c].w = pw;
						lc->rects[c].h = split;
					} else if(cw->flags & MKGUI_REGION_BOTTOM) {
						lc->rects[c].x = px;
						lc->rects[c].y = py + split + win->split_thick;
						lc->rects[c].w = pw;
						lc->rects[c].h = ph - split - win->split_thick;
					}
				}
			} else {
				int32_t split = (int32_t)(pw * ratio);
				for(uint32_t c = lc->first_child[idx]; c < lc->widget_count; c = lc->next_sibling[c]) {
					struct mkgui_widget *cw = lw_get(lc, c);
					if(cw->flags & MKGUI_HIDDEN) {
						continue;
					}

					if(cw->flags & MKGUI_REGION_LEFT) {
						lc->rects[c].x = px;
						lc->rects[c].y = py;
						lc->rects[c].w = split;
						lc->rects[c].h = ph;
					} else if(cw->flags & MKGUI_REGION_RIGHT) {
						lc->rects[c].x = px + split + win->split_thick;
						lc->rects[c].y = py;
						lc->rects[c].w = pw - split - win->split_thick;
						lc->rects[c].h = ph;
					}
				}
			}
		} else {
			for(uint32_t c = lc->first_child[idx]; c < lc->widget_count; c = lc->next_sibling[c]) {
				struct mkgui_widget *cw = lw_get(lc, c);
				uint32_t ct = cw->type;
				if(ct == MKGUI_MENUITEM || (cw->flags & MKGUI_HIDDEN)) {
					continue;
				}

				if(w->type == MKGUI_WINDOW && (ct == MKGUI_MENU || ct == MKGUI_TOOLBAR || ct == MKGUI_STATUSBAR)) {
					continue;
				}

				if(w->type == MKGUI_WINDOW && ct == MKGUI_PATHBAR && lw_sw(win, cw->h) == 0) {
					continue;
				}
				int32_t ml = lw_sw(win, cw->margin_l);
				int32_t mr = lw_sw(win, cw->margin_r);
				int32_t mt = lw_sw(win, cw->margin_t);
				int32_t mb = lw_sw(win, cw->margin_b);
				lc->rects[c].x = px + ml;
				lc->rects[c].y = py + mt;
				lc->rects[c].w = pw - ml - mr;
				lc->rects[c].h = ph - mt - mb;
				if(cw->flags & MKGUI_FIXED) {
					int32_t fw = lw_sw(win, cw->w);
					if(fw == 0) {
						if(ct == MKGUI_VBOX || ct == MKGUI_HBOX || ct == MKGUI_FORM || ct == MKGUI_GROUP || ct == MKGUI_PANEL) {
							fw = lc_measure_container(win, lc, c, 0);
						}

						if(fw == 0) {
							fw = natural_width(win, cw);
						}
					}
					lc->rects[c].w = fw;
					int32_t fh = lw_sw(win, cw->h);
					if(fh == 0) {
						if(ct == MKGUI_VBOX || ct == MKGUI_HBOX || ct == MKGUI_FORM || ct == MKGUI_GROUP || ct == MKGUI_PANEL) {
							fh = lc_measure_container(win, lc, c, 1);
						}

						if(fh == 0) {
							fh = natural_height(win, ct);
						}
					}
					lc->rects[c].h = fh;
				}
			}
		}
	}

	for(uint32_t c = lc->first_child[idx]; c < lc->widget_count; c = lc->next_sibling[c]) {
		struct mkgui_widget *cw = lw_get(lc, c);
		if(cw->type != MKGUI_MENUITEM && !(cw->flags & MKGUI_HIDDEN)) {
			lc_layout_node(win, lc, c);
		}
	}
}

// [=]===^=[ layout_node ]========================================[=]
static void layout_node(struct mkgui_window *win, uint32_t idx) {
	struct layout_ctx lc = {
		.widgets = win->widgets, .rects = (void *)win->rects,
		.first_child = layout_first_child, .next_sibling = layout_next_sibling,
		.parent = layout_parent, .widget_count = win->widget_count,
		.has_scroll = 1, .widget_stride = sizeof(struct mkgui_widget)
	};
	lc_layout_node(win, &lc, idx);
}

static void platform_set_min_size(struct mkgui_window *win, int32_t min_w, int32_t min_h);
static void platform_font_set_size(struct mkgui_window *win, int32_t pixel_size);

// [=]===^=[ layout_min_size ]======================================[=]
// Uses the same measure_container path the layout engine uses for
// VBOX/HBOX/FORM/GROUP/PANEL so there is one source of truth.
static void layout_min_size(struct mkgui_window *win, uint32_t idx, int32_t *out_w, int32_t *out_h) {
	struct mkgui_widget *w = &win->widgets[idx];
	uint32_t t = w->type;

	// scrollable containers use a small fixed minimum; they scroll instead of
	// forcing all content to fit
	if((t == MKGUI_VBOX || t == MKGUI_HBOX || t == MKGUI_PANEL) && (w->flags & MKGUI_SCROLL)) {
		*out_w = 100;
		*out_h = win->row_height * 3;
		return;
	}

	// containers: delegate to the layout engine's own measurement
	if(t == MKGUI_VBOX || t == MKGUI_HBOX || t == MKGUI_FORM || t == MKGUI_GROUP || t == MKGUI_PANEL) {
		*out_w = measure_container(win, idx, 0);
		*out_h = measure_container(win, idx, 1);
		return;
	}

	// tabs: max across all tab pages
	if(t == MKGUI_TABS) {
		int32_t max_tw = 0;
		int32_t max_th = 0;
		for(uint32_t c = layout_first_child[idx]; c < win->widget_count; c = layout_next_sibling[c]) {
			if(win->widgets[c].type == MKGUI_TAB) {
				int32_t tw = 0;
				int32_t th = 0;
				for(uint32_t gc = layout_first_child[c]; gc < win->widget_count; gc = layout_next_sibling[gc]) {
					int32_t cw, ch;
					layout_min_size(win, gc, &cw, &ch);
					if(cw > tw) {
						tw = cw;
					}
					th += ch;
				}

				if(tw > max_tw) {
					max_tw = tw;
				}

				if(th > max_th) {
					max_th = th;
				}
			}
		}
		int32_t tab_inset = sc(win, MKGUI_TAB_INSET);
		*out_w = max_tw + tab_inset * 2;
		*out_h = win->tab_height + tab_inset * 2 + max_th;
		return;
	}

	if(t == MKGUI_HSPLIT) {
		int32_t max_w = 0;
		int32_t total_h = win->split_thick;
		for(uint32_t c = layout_first_child[idx]; c < win->widget_count; c = layout_next_sibling[c]) {
			int32_t cw, ch;
			layout_min_size(win, c, &cw, &ch);
			if(cw > max_w) {
				max_w = cw;
			}
			total_h += ch;
		}
		*out_w = max_w;
		*out_h = total_h;
		return;
	}

	if(t == MKGUI_VSPLIT) {
		int32_t total_w = win->split_thick;
		int32_t max_h = 0;
		for(uint32_t c = layout_first_child[idx]; c < win->widget_count; c = layout_next_sibling[c]) {
			int32_t cw, ch;
			layout_min_size(win, c, &cw, &ch);
			total_w += cw;
			if(ch > max_h) {
				max_h = ch;
			}
		}
		*out_w = total_w;
		*out_h = max_h;
		return;
	}

	// leaf widgets
	int32_t nh = natural_height(win, t);
	*out_w = lw_sw(win, w->w) > 0 ? lw_sw(win, w->w) : natural_width(win, w);
	*out_h = lw_sw(win, w->h) > 0 ? lw_sw(win, w->h) : (nh > 0 ? nh : 0);
}

// [=]===^=[ layout_compute_min_size ]==============================[=]
static void layout_compute_min_size(struct mkgui_window *win, uint32_t win_idx) {
	int32_t min_h = 0;
	int32_t min_w = 0;
	for(uint32_t c = layout_first_child[win_idx]; c < win->widget_count; c = layout_next_sibling[c]) {
		uint32_t ct = win->widgets[c].type;
		if(ct == MKGUI_MENU) {
			min_h += win->menu_height;
		} else if(ct == MKGUI_TOOLBAR) {
			min_h += win->font_height + 10;
		} else if(ct == MKGUI_STATUSBAR) {
			min_h += win->statusbar_height;
		} else if(ct == MKGUI_PATHBAR && win->widgets[c].h == 0) {
			min_h += win->pathbar_height;
		} else {
			int32_t cw, ch;
			layout_min_size(win, c, &cw, &ch);
			min_h += ch;
			if(cw > min_w) {
				min_w = cw;
			}
		}
	}

	int32_t floor_w = sc(win, MKGUI_WIN_MIN_W);
	int32_t floor_h = sc(win, MKGUI_WIN_MIN_H);
	if(min_h < floor_h) {
		min_h = floor_h;
	}

	if(min_w < floor_w) {
		min_w = floor_w;
	}
	min_w += win->box_pad * 2;
	min_h += win->box_pad * 2;
	platform_set_min_size(win, min_w, min_h);
}

// [=]===^=[ layout_widgets ]====================================[=]
static void layout_widgets(struct mkgui_window *win) {
	layout_build_index(win);
	for(uint32_t i = 0; i < win->widget_count; ++i) {
		if(win->widgets[i].type == MKGUI_WINDOW) {
			win->rects[i].x = 0;
			win->rects[i].y = 0;
			win->rects[i].w = win->win_w;
			win->rects[i].h = win->win_h;
			layout_node(win, i);
			if(win->dirty_full && !win->canvas_window) {
				layout_compute_min_size(win, i);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Hit testing
// ---------------------------------------------------------------------------

// [=]===^=[ hit_test ]==========================================[=]
static int32_t hit_test(struct mkgui_window *win, int32_t mx, int32_t my) {
	for(int32_t i = (int32_t)win->widget_count - 1; i >= 0; --i) {
		if(!widget_visible(win, (uint32_t)i)) {
			continue;
		}
		struct mkgui_widget *w = &win->widgets[i];
		if(w->type == MKGUI_WINDOW || w->type == MKGUI_TAB || w->type == MKGUI_PANEL || w->type == MKGUI_SPINNER || w->type == MKGUI_GLVIEW || w->type == MKGUI_VBOX || w->type == MKGUI_HBOX || w->type == MKGUI_FORM || w->type == MKGUI_DIVIDER || w->type == MKGUI_TOOLBAR) {
			continue;
		}

		if(w->type == MKGUI_GROUP) {
			int32_t rx = win->rects[i].x;
			int32_t ry = win->rects[i].y;
			int32_t rw = win->rects[i].w;
			int32_t header_h = win->font_height + 4;
			if(mx >= rx && mx < rx + rw && my >= ry && my < ry + header_h) {
				return i;
			}
			continue;
		}

		if(w->type == MKGUI_LABEL && !(w->style & MKGUI_LABEL_LINK)) {
			continue;
		}

		if(w->type == MKGUI_HSPLIT || w->type == MKGUI_VSPLIT) {
			int32_t rx = win->rects[i].x;
			int32_t ry = win->rects[i].y;
			int32_t rw = win->rects[i].w;
			int32_t rh = win->rects[i].h;
			struct mkgui_split_data *sd = find_split_data(win, w->id);
			float ratio = sd ? sd->ratio : 0.5f;
			if(w->type == MKGUI_HSPLIT) {
				int32_t split_y = ry + (int32_t)(rh * ratio);
				if(mx >= rx && mx < rx + rw && my >= split_y && my < split_y + win->split_thick) {
					return i;
				}
			} else {
				int32_t split_x = rx + (int32_t)(rw * ratio);
				if(mx >= split_x && mx < split_x + win->split_thick && my >= ry && my < ry + rh) {
					return i;
				}
			}
			continue;
		}
		int32_t rx = win->rects[i].x;
		int32_t ry = win->rects[i].y;
		int32_t rw = win->rects[i].w;
		int32_t rh = win->rects[i].h;
		if(mx >= rx && mx < rx + rw && my >= ry && my < ry + rh) {
			return i;
		}
	}
	return -1;
}

// ---------------------------------------------------------------------------
// Window registry
// ---------------------------------------------------------------------------

// Single process-global ctx pointer. Set by mkgui_ctx_create, cleared by
// mkgui_ctx_destroy. Lets platform-level callbacks (X11 event router,
// Win32 WndProc) find the window registry without threading ctx through
// every layer. The canonical window list lives on ctx->windows[]; this is
// only a pointer to it, not a duplicate.
static struct mkgui_ctx *g_ctx;

// ---------------------------------------------------------------------------
// Platform backend
// ---------------------------------------------------------------------------

static void mkgui_flush(struct mkgui_window *win);
static void mkgui_resize_render_impl(struct mkgui_window *win);
static void mkgui_resize_render(struct mkgui_window *win) {
	mkgui_resize_render_impl(win);
}

#include "mkgui_drop.c"

#ifdef _WIN32
#include "platform_win32.c"
#else
#include "platform_linux.c"
#endif

// [=]===^=[ mkgui_drop_enable ]=================================[=]
MKGUI_API void mkgui_drop_enable(struct mkgui_window *win) {
	MKGUI_CHECK(win);
	if(win->drop_enabled) {
		return;
	}
	win->drop_enabled = 1;
	platform_drop_enable(win);
}

// ---------------------------------------------------------------------------
// Popup windows (override-redirect)
// ---------------------------------------------------------------------------

// [=]===^=[ popup_create ]======================================[=]
static struct mkgui_popup *popup_create(struct mkgui_window *win, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t widget_id) {
	if(win->popup_count >= MKGUI_MAX_POPUPS) {
		return NULL;
	}
	struct mkgui_popup *p = &win->popups[win->popup_count];
	memset(p, 0, sizeof(*p));

	if(!platform_popup_init(win, p, x, y, w, h)) {
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

	++win->popup_count;
	return p;
}

// [=]===^=[ popup_destroy ]=====================================[=]
static void popup_destroy(struct mkgui_window *win, uint32_t popup_idx) {
	if(popup_idx >= win->popup_count) {
		return;
	}
	struct mkgui_popup *p = &win->popups[popup_idx];
	platform_popup_fini(win, p);
	p->active = 0;

	for(uint32_t i = popup_idx; i < win->popup_count - 1; ++i) {
		win->popups[i] = win->popups[i + 1];
	}
	--win->popup_count;
}

// [=]===^=[ popup_destroy_all ]=================================[=]
static void popup_destroy_all(struct mkgui_window *win) {
	while(win->popup_count > 0) {
		popup_destroy(win, 0);
	}
}

// [=]===^=[ popup_destroy_from ]================================[=]
static void popup_destroy_from(struct mkgui_window *win, uint32_t from_idx) {
	while(win->popup_count > from_idx) {
		popup_destroy(win, win->popup_count - 1);
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
	if(y + row0 < cy1) {
		row0 = cy1 - y;
	}

	if(y + row0 < 0) {
		row0 = -y;
	}

	if(y + row1 > cy2) {
		row1 = cy2 - y;
	}

	if(y + row1 > bh) {
		row1 = bh - y;
	}

	if(x + col0 < cx1) {
		col0 = cx1 - x;
	}

	if(x + col0 < 0) {
		col0 = -x;
	}

	if(x + col1 > cx2) {
		col1 = cx2 - x;
	}

	if(x + col1 > bw) {
		col1 = bw - x;
	}
	for(int32_t row = row0; row < row1; ++row) {
		blend_icon_row(&buf[(y + row) * bw], &icon_atlas[icon->atlas_offset + (uint32_t)row * (uint32_t)icon->w], col0, col1, x);
	}
}

// [=]===^=[ draw_icon_popup ]=====================================[=]
static void draw_icon_popup(struct mkgui_popup *p, struct mkgui_icon *icon, int32_t x, int32_t y) {
	draw_icon(p->pixels, p->w, p->h, icon, x, y, 0, 0, p->w, p->h);
}

// ---------------------------------------------------------------------------
// PlutoVG + PlutoSVG (SVG parsing + rasterization)
// ---------------------------------------------------------------------------

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
#define PLUTOSVG_BUILD_STATIC
#define PLUTOSVG_BUILD
#define PLUTOVG_BUILD_STATIC
#define PLUTOVG_BUILD
#define PLUTOVG_NO_IMAGE_IO
#include "plutovg/plutovg.h"
plutovg_surface_t *plutovg_surface_load_from_image_base64(const char *d, int l) { (void)d; (void)l; return NULL; }
plutovg_surface_t *plutovg_surface_load_from_image_file(const char *f) { (void)f; return NULL; }
plutovg_surface_t *plutovg_surface_load_from_image_data(const void *d, int l) { (void)d; (void)l; return NULL; }
#include "plutovg/plutovg-private.h"
#include "plutovg/plutovg-utils.h"
#include "plutovg/plutovg-ft-math.c"
#include "plutovg/plutovg-ft-raster.c"
#include "plutovg/plutovg-ft-stroker.c"
#include "plutovg/plutovg-blend.c"
#include "plutovg/plutovg-canvas.c"
#include "plutovg/plutovg-font.c"
#include "plutovg/plutovg-matrix.c"
#include "plutovg/plutovg-paint.c"
#include "plutovg/plutovg-path.c"
#include "plutovg/plutovg-rasterize.c"
#include "plutovg/plutovg-surface.c"
#undef MAX_NAME
#include "plutosvg/plutosvg.h"
#include "plutosvg/plutosvg.c"
#pragma GCC diagnostic pop

// ---------------------------------------------------------------------------
// Filesystem abstraction (used by icon, iconbrowser and filedialog)
// ---------------------------------------------------------------------------

#include "mkgui_fs.c"

// ---------------------------------------------------------------------------
// Widget implementations
// ---------------------------------------------------------------------------

#include "mkgui_icon.c"
#include "mkgui_button.c"
#include "mkgui_label.c"
#include "mkgui_textedit.c"
#include "mkgui_undo.c"
#include "mkgui_input.c"
#include "mkgui_checkbox.c"
#include "mkgui_dropdown.c"
#include "mkgui_slider.c"
#include "mkgui_listview.c"
#include "mkgui_tabs.c"
#include "mkgui_radio.c"
#include "mkgui_accel.c"
#include "mkgui_menu.c"
#include "mkgui_ctxmenu.c"
#include "mkgui_split.c"
#include "mkgui_treeview.c"
#include "mkgui_statusbar.c"
#include "mkgui_toolbar.c"
#include "mkgui_pathbar.c"
#include "mkgui_spinbox.c"
#include "mkgui_progress.c"
#include "mkgui_meter.c"
#include "mkgui_textarea.c"
#include "mkgui_logview.c"
#include "mkgui_group.c"
#include "mkgui_panel.c"
#include "mkgui_spacer.c"
#include "mkgui_divider.c"
#include "mkgui_ipinput.c"
#include "mkgui_toggle.c"
#include "mkgui_combobox.c"
#include "mkgui_datepicker.c"
#include "mkgui_gridview.c"
#include "mkgui_richlist.c"
#include "mkgui_spinner.c"
#include "mkgui_itemview.c"
#include "mkgui_scrollbar.c"
#include "mkgui_image.c"
#include "mkgui_glview.c"
#include "mkgui_canvas.c"
#include "mkgui_celledit.c"
#include "mkgui_tooltip.c"
#include "mkgui_notify.c"

// ---------------------------------------------------------------------------
// Render dispatch
// ---------------------------------------------------------------------------

// [=]===^=[ render_widget ]=====================================[=]
static void render_widget(struct mkgui_window *win, uint32_t idx) {
	switch(win->widgets[idx].type) {
		case MKGUI_BUTTON: {
			uint32_t pidx = layout_parent[idx];
			if(pidx < win->widget_count && win->widgets[pidx].type == MKGUI_TOOLBAR) {
				break;
			}
			render_button(win, idx);
		} break;

		case MKGUI_LABEL: {
			render_label(win, idx);
		} break;

		case MKGUI_INPUT: {
			render_input(win, idx);
		} break;

		case MKGUI_CHECKBOX: {
			render_checkbox(win, idx);
		} break;

		case MKGUI_SLIDER: {
			render_slider(win, idx);
		} break;

		case MKGUI_DROPDOWN: {
			render_dropdown(win, idx);
		} break;

		case MKGUI_TABS: {
			render_tabs(win, idx);
		} break;

		case MKGUI_LISTVIEW: {
			render_listview(win, idx);
			celledit_render(win, idx);
		} break;

		case MKGUI_HSPLIT:
		case MKGUI_VSPLIT: {
			render_split(win, idx);
		} break;

		case MKGUI_MENU: {
			render_menu_bar(win, idx);
		} break;

		case MKGUI_TREEVIEW: {
			render_treeview(win, idx);
			celledit_render(win, idx);
		} break;

		case MKGUI_STATUSBAR: {
			render_statusbar(win, idx);
		} break;

		case MKGUI_TOOLBAR: {
			render_toolbar(win, idx);
		} break;

		case MKGUI_SPINBOX: {
			render_spinbox(win, idx);
		} break;

		case MKGUI_RADIO: {
			render_radio(win, idx);
		} break;

		case MKGUI_PROGRESS: {
			render_progress(win, idx);
		} break;

		case MKGUI_METER: {
			render_meter(win, idx);
		} break;

		case MKGUI_TEXTAREA: {
			render_textarea(win, idx);
		} break;

		case MKGUI_LOGVIEW: {
			render_logview(win, idx);
		} break;

		case MKGUI_GROUP: {
			render_group(win, idx);
		} break;

		case MKGUI_SPINNER: {
			render_spinner(win, idx);
		} break;

		case MKGUI_ITEMVIEW: {
			render_itemview(win, idx);
			celledit_render(win, idx);
		} break;

		case MKGUI_PANEL: {
			render_panel(win, idx);
		} break;

		case MKGUI_SCROLLBAR: {
			render_scrollbar(win, idx);
		} break;

		case MKGUI_IMAGE: {
			render_image(win, idx);
		} break;

		case MKGUI_GLVIEW: {
			render_glview(win, idx);
		} break;

		case MKGUI_CANVAS: {
			render_canvas(win, idx);
		} break;

		case MKGUI_PATHBAR: {
			render_pathbar(win, idx);
		} break;

		case MKGUI_IPINPUT: {
			render_ipinput(win, idx);
		} break;

		case MKGUI_TOGGLE: {
			render_toggle(win, idx);
		} break;

		case MKGUI_COMBOBOX: {
			render_combobox(win, idx);
		} break;

		case MKGUI_DATEPICKER: {
			render_datepicker(win, idx);
		} break;

		case MKGUI_GRIDVIEW: {
			render_gridview(win, idx);
			celledit_render(win, idx);
		} break;

		case MKGUI_RICHLIST: {
			render_richlist(win, idx);
		} break;

		case MKGUI_DIVIDER: {
			render_divider(win, idx);
		} break;

		case MKGUI_VBOX:
		case MKGUI_HBOX:
		case MKGUI_FORM: {
			if(win->widgets[idx].style & STYLE_BORDER_BIT) {
				render_panel(win, idx);
			}

			if((win->widgets[idx].flags & MKGUI_SCROLL)) {
				struct mkgui_box_scroll *bs = find_box_scroll(win, win->widgets[idx].id);
				int32_t rx = win->rects[idx].x;
				int32_t ry = win->rects[idx].y;
				int32_t rw = win->rects[idx].w;
				int32_t rh = win->rects[idx].h;
				int32_t r = win->theme.corner_radius;
				if(bs && bs->content_h > rh) {
					int32_t sx = rx + rw - win->scrollbar_w;
					draw_rect_fill(win->pixels, win->win_w, win->win_h, sx, ry, win->scrollbar_w, rh, win->theme.scrollbar_bg);
					int32_t track = rh;
					int32_t total = bs->content_h;
					if(total < 1) {
						total = 1;
					}
					int32_t thumb = track * rh / total;
					if(thumb < sc(win, 20)) {
						thumb = sc(win, 20);
					}

					if(thumb > track) {
						thumb = track;
					}
					int32_t max_scroll = bs->content_h - rh;
					if(max_scroll < 1) {
						max_scroll = 1;
					}
					int32_t pos = (int32_t)((int64_t)bs->scroll_y * (track - thumb) / max_scroll);
					if(pos < 0) {
						pos = 0;
					}

					if(pos > track - thumb) {
						pos = track - thumb;
					}
					draw_rounded_rect_fill(win->pixels, win->win_w, win->win_h, sx + 2, ry + pos, win->scrollbar_w - 4, thumb, win->theme.scrollbar_thumb, r);
				}

				if(bs && bs->content_w > rw) {
					int32_t sy = ry + rh - win->scrollbar_w;
					draw_rect_fill(win->pixels, win->win_w, win->win_h, rx, sy, rw, win->scrollbar_w, win->theme.scrollbar_bg);
					int32_t track = rw;
					int32_t total = bs->content_w;
					if(total < 1) {
						total = 1;
					}
					int32_t thumb = track * rw / total;
					if(thumb < sc(win, 20)) {
						thumb = sc(win, 20);
					}

					if(thumb > track) {
						thumb = track;
					}
					int32_t max_scroll = bs->content_w - rw;
					if(max_scroll < 1) {
						max_scroll = 1;
					}
					int32_t pos = (int32_t)((int64_t)bs->scroll_x * (track - thumb) / max_scroll);
					if(pos < 0) {
						pos = 0;
					}

					if(pos > track - thumb) {
						pos = track - thumb;
					}
					draw_rounded_rect_fill(win->pixels, win->win_w, win->win_h, rx + pos, sy + 2, thumb, win->scrollbar_w - 4, win->theme.scrollbar_thumb, r);
				}
			}
		} break;

		default: {
		} break;
	}
}

// [=]===^=[ set_parent_clip ]====================================[=]
static void set_parent_clip(struct mkgui_window *win, uint32_t idx) {
	render_clip_x1 = render_base_clip_x1;
	render_clip_y1 = render_base_clip_y1;
	render_clip_x2 = render_base_clip_x2;
	render_clip_y2 = render_base_clip_y2;
	uint32_t pidx = layout_parent[idx];
	while(pidx < win->widget_count && win->widgets[pidx].type != MKGUI_WINDOW) {
		int32_t px = win->rects[pidx].x;
		int32_t py = win->rects[pidx].y;
		int32_t px2 = px + win->rects[pidx].w;
		int32_t py2 = py + win->rects[pidx].h;
		if(px > render_clip_x1) {
			render_clip_x1 = px;
		}

		if(py > render_clip_y1) {
			render_clip_y1 = py;
		}

		if(px2 < render_clip_x2) {
			render_clip_x2 = px2;
		}

		if(py2 < render_clip_y2) {
			render_clip_y2 = py2;
		}
		pidx = layout_parent[pidx];
	}
}

// [=]===^=[ render_widgets ]====================================[=]
static void render_widgets(struct mkgui_window *win) {
	text_cmd_count = 0;

	if(win->canvas_window) {
		render_clip_x1 = 0;
		render_clip_y1 = 0;
		render_clip_x2 = win->win_w;
		render_clip_y2 = win->win_h;
		for(uint32_t i = 0; i < win->canvas_count; ++i) {
			struct mkgui_canvas_data *cd = &win->canvases[i];
			if(cd->callback) {
				cd->callback(win, cd->widget_id, win->pixels, 0, 0, win->win_w, win->win_h, cd->userdata);
				break;
			}
		}
		win->dirty_full = 1;
		return;
	}

	if(win->dirty_full || win->render_cb) {
		render_base_clip_x1 = 0;
		render_base_clip_y1 = 0;
		render_base_clip_x2 = win->win_w;
		render_base_clip_y2 = win->win_h;
		render_clip_x1 = 0;
		render_clip_y1 = 0;
		render_clip_x2 = win->win_w;
		render_clip_y2 = win->win_h;
		draw_rect_fill(win->pixels, win->win_w, win->win_h, 0, 0, win->win_w, win->win_h, win->theme.bg);
		for(uint32_t i = 0; i < win->widget_count; ++i) {
			if(!widget_visible(win, i)) {
				continue;
			}
			set_parent_clip(win, i);
			render_widget(win, i);
		}
	} else {
		for(uint32_t d = 0; d < win->dirty_count; ++d) {
			int32_t dx = win->dirty_rects[d].x;
			int32_t dy = win->dirty_rects[d].y;
			int32_t dw = win->dirty_rects[d].w;
			int32_t dh = win->dirty_rects[d].h;
			render_base_clip_x1 = dx;
			render_base_clip_y1 = dy;
			render_base_clip_x2 = dx + dw;
			render_base_clip_y2 = dy + dh;
			render_clip_x1 = dx;
			render_clip_y1 = dy;
			render_clip_x2 = dx + dw;
			render_clip_y2 = dy + dh;
			draw_rect_fill(win->pixels, win->win_w, win->win_h, dx, dy, dw, dh, win->theme.bg);
			for(uint32_t i = 0; i < win->widget_count; ++i) {
				if(!widget_visible(win, i)) {
					continue;
				}
				int32_t wx = win->rects[i].x;
				int32_t wy = win->rects[i].y;
				int32_t wr = wx + win->rects[i].w;
				int32_t wb = wy + win->rects[i].h;
				if(wx < dx + dw && wr > dx && wy < dy + dh && wb > dy) {
					set_parent_clip(win, i);
					render_widget(win, i);
				}
			}
		}
	}

	render_clip_x1 = 0;
	render_clip_y1 = 0;
	render_clip_x2 = win->win_w;
	render_clip_y2 = win->win_h;
}

// ---------------------------------------------------------------------------
// Init auxiliary data
// ---------------------------------------------------------------------------

// [=]===^=[ init_widget_aux ]====================================[=]
static void init_widget_aux(struct mkgui_window *win, struct mkgui_widget *w) {
	switch(w->type) {
		case MKGUI_INPUT: {
			MKGUI_AUX_GROW(&win->arenas.inputs, win->input_count, win->input_cap, struct mkgui_input_data);
			if(win->input_count < win->input_cap) {
				struct mkgui_input_data *inp = &win->inputs[win->input_count++];
				memset(inp, 0, sizeof(*inp));
				inp->widget_id = w->id;
			}
		} break;

		case MKGUI_IPINPUT: {
			MKGUI_AUX_GROW(&win->arenas.ipinputs, win->ipinput_count, win->ipinput_cap, struct mkgui_ipinput_data);
			if(win->ipinput_count < win->ipinput_cap) {
				struct mkgui_ipinput_data *ip = &win->ipinputs[win->ipinput_count++];
				memset(ip, 0, sizeof(*ip));
				ip->widget_id = w->id;
			}
		} break;

		case MKGUI_TOGGLE: {
			MKGUI_AUX_GROW(&win->arenas.toggles, win->toggle_count, win->toggle_cap, struct mkgui_toggle_data);
			if(win->toggle_count < win->toggle_cap) {
				struct mkgui_toggle_data *td = &win->toggles[win->toggle_count++];
				memset(td, 0, sizeof(*td));
				td->widget_id = w->id;
			}
		} break;

		case MKGUI_COMBOBOX: {
			MKGUI_AUX_GROW(&win->arenas.comboboxes, win->combobox_count, win->combobox_cap, struct mkgui_combobox_data);
			if(win->combobox_count < win->combobox_cap) {
				struct mkgui_combobox_data *cb = &win->comboboxes[win->combobox_count++];
				memset(cb, 0, sizeof(*cb));
				cb->widget_id = w->id;
				cb->selected = -1;
			}
		} break;

		case MKGUI_DATEPICKER: {
			MKGUI_AUX_GROW(&win->arenas.datepickers, win->datepicker_count, win->datepicker_cap, struct mkgui_datepicker_data);
			if(win->datepicker_count < win->datepicker_cap) {
				struct mkgui_datepicker_data *dp = &win->datepickers[win->datepicker_count++];
				memset(dp, 0, sizeof(*dp));
				dp->widget_id = w->id;
				dp->year = 2026;
				dp->month = 1;
				dp->day = 1;
			}
		} break;

		case MKGUI_DROPDOWN: {
			MKGUI_AUX_GROW(&win->arenas.dropdowns, win->dropdown_count, win->dropdown_cap, struct mkgui_dropdown_data);
			if(win->dropdown_count < win->dropdown_cap) {
				struct mkgui_dropdown_data *dd = &win->dropdowns[win->dropdown_count++];
				memset(dd, 0, sizeof(*dd));
				dd->widget_id = w->id;
				dd->selected = -1;
			}
		} break;

		case MKGUI_SLIDER: {
			MKGUI_AUX_GROW(&win->arenas.sliders, win->slider_count, win->slider_cap, struct mkgui_slider_data);
			if(win->slider_count < win->slider_cap) {
				struct mkgui_slider_data *sd = &win->sliders[win->slider_count++];
				memset(sd, 0, sizeof(*sd));
				sd->widget_id = w->id;
				sd->min_val = 0;
				sd->max_val = 100;
				sd->value = 50;
			}
		} break;

		case MKGUI_TABS: {
			MKGUI_AUX_GROW(&win->arenas.tabs, win->tab_count, win->tab_cap, struct mkgui_tabs_data);
			if(win->tab_count < win->tab_cap) {
				struct mkgui_tabs_data *td = &win->tabs[win->tab_count++];
				td->widget_id = w->id;
				td->active_tab = 0;
				for(uint32_t j = 0; j < win->widget_count; ++j) {
					if(win->widgets[j].type == MKGUI_TAB && win->widgets[j].parent_id == w->id) {
						td->active_tab = win->widgets[j].id;
						break;
					}
				}
			}
		} break;

		case MKGUI_VBOX:
		case MKGUI_HBOX: {
			MKGUI_AUX_GROW(&win->arenas.box_scrolls, win->box_scroll_count, win->box_scroll_cap, struct mkgui_box_scroll);
			if(win->box_scroll_count < win->box_scroll_cap) {
				struct mkgui_box_scroll *bs = &win->box_scrolls[win->box_scroll_count++];
				bs->widget_id = w->id;
				bs->scroll_y = 0;
				bs->content_h = 0;
				bs->scroll_x = 0;
				bs->content_w = 0;
			}
		} break;

		case MKGUI_HSPLIT:
		case MKGUI_VSPLIT: {
			MKGUI_AUX_GROW(&win->arenas.splits, win->split_count, win->split_cap, struct mkgui_split_data);
			if(win->split_count < win->split_cap) {
				struct mkgui_split_data *sd = &win->splits[win->split_count++];
				sd->widget_id = w->id;
				sd->ratio = 0.5f;
			}
		} break;

		case MKGUI_TREEVIEW: {
			MKGUI_AUX_GROW(&win->arenas.treeviews, win->treeview_count, win->treeview_cap, struct mkgui_treeview_data);
			if(win->treeview_count < win->treeview_cap) {
				struct mkgui_treeview_data *tv = &win->treeviews[win->treeview_count++];
				memset(tv, 0, sizeof(*tv));
				tv->widget_id = w->id;
				tv->selected_node = -1;
			}
		} break;

		case MKGUI_STATUSBAR: {
			MKGUI_AUX_GROW(&win->arenas.statusbars, win->statusbar_count, win->statusbar_cap, struct mkgui_statusbar_data);
			if(win->statusbar_count < win->statusbar_cap) {
				struct mkgui_statusbar_data *sb = &win->statusbars[win->statusbar_count++];
				memset(sb, 0, sizeof(*sb));
				sb->widget_id = w->id;
			}
		} break;

		case MKGUI_SPINBOX: {
			MKGUI_AUX_GROW(&win->arenas.spinboxes, win->spinbox_count, win->spinbox_cap, struct mkgui_spinbox_data);
			if(win->spinbox_count < win->spinbox_cap) {
				struct mkgui_spinbox_data *sd = &win->spinboxes[win->spinbox_count++];
				memset(sd, 0, sizeof(*sd));
				sd->widget_id = w->id;
				sd->min_val = 0;
				sd->max_val = 100;
				sd->value = 0;
				sd->step = 1;
			}
		} break;

		case MKGUI_PROGRESS: {
			MKGUI_AUX_GROW(&win->arenas.progress, win->progress_count, win->progress_cap, struct mkgui_progress_data);
			if(win->progress_count < win->progress_cap) {
				struct mkgui_progress_data *pd = &win->progress[win->progress_count++];
				memset(pd, 0, sizeof(*pd));
				pd->widget_id = w->id;
				pd->max_val = 100;
			}
		} break;

		case MKGUI_METER: {
			MKGUI_AUX_GROW(&win->arenas.meters, win->meter_count, win->meter_cap, struct mkgui_meter_data);
			if(win->meter_count < win->meter_cap) {
				struct mkgui_meter_data *md = &win->meters[win->meter_count++];
				memset(md, 0, sizeof(*md));
				md->widget_id = w->id;
				md->max_val = 100;
				md->zone_t1 = 75;
				md->zone_t2 = 90;
				md->zone_c1 = 0xff44cc44;
				md->zone_c2 = 0xffffcc00;
				md->zone_c3 = 0xffff4444;
			}
		} break;

		case MKGUI_TEXTAREA: {
			MKGUI_AUX_GROW(&win->arenas.textareas, win->textarea_count, win->textarea_cap, struct mkgui_textarea_data);
			if(win->textarea_count < win->textarea_cap) {
				struct mkgui_textarea_data *ta = &win->textareas[win->textarea_count++];
				memset(ta, 0, sizeof(*ta));
				ta->widget_id = w->id;
				ta->text = (char *)calloc(1, MKGUI_TEXTAREA_INIT_CAP);
				if(!ta->text) {
					--win->textarea_count;
					break;
				}
				ta->text_cap = MKGUI_TEXTAREA_INIT_CAP;
			}
		} break;

		case MKGUI_LOGVIEW: {
			MKGUI_AUX_GROW(&win->arenas.logviews, win->logview_count, win->logview_cap, struct mkgui_logview_data);
			if(win->logview_count < win->logview_cap) {
				struct mkgui_logview_data *lv = &win->logviews[win->logview_count++];
				memset(lv, 0, sizeof(*lv));
				lv->widget_id = w->id;
				lv->cached_width = -1;
				lv->stuck_to_end = 1;
			}
		} break;

		case MKGUI_ITEMVIEW: {
			MKGUI_AUX_GROW(&win->arenas.itemviews, win->itemview_count, win->itemview_cap, struct mkgui_itemview_data);
			if(win->itemview_count < win->itemview_cap) {
				struct mkgui_itemview_data *iv = &win->itemviews[win->itemview_count++];
				memset(iv, 0, sizeof(*iv));
				iv->widget_id = w->id;
				iv->selected = -1;
			}
		} break;

		case MKGUI_CANVAS: {
			MKGUI_AUX_GROW(&win->arenas.canvases, win->canvas_count, win->canvas_cap, struct mkgui_canvas_data);
			if(win->canvas_count < win->canvas_cap) {
				struct mkgui_canvas_data *cd = &win->canvases[win->canvas_count++];
				memset(cd, 0, sizeof(*cd));
				cd->widget_id = w->id;
			}
		} break;

		case MKGUI_PATHBAR: {
			MKGUI_AUX_GROW(&win->arenas.pathbars, win->pathbar_count, win->pathbar_cap, struct mkgui_pathbar_data);
			if(win->pathbar_count < win->pathbar_cap) {
				struct mkgui_pathbar_data *pb = &win->pathbars[win->pathbar_count++];
				memset(pb, 0, sizeof(*pb));
				pb->widget_id = w->id;
				pb->hover_seg = -1;
			}
		} break;

		default: {
		} break;
	}
}

// [=]===^=[ init_aux_data ]=====================================[=]
static void init_aux_data(struct mkgui_window *win) {
	for(uint32_t i = 0; i < win->widget_count; ++i) {
		init_widget_aux(win, &win->widgets[i]);
	}
	for(uint32_t i = 0; i < win->widget_count; ++i) {
		struct mkgui_widget *w = &win->widgets[i];
		if(w->type == MKGUI_BUTTON && w->label[0] != '\0') {
			struct mkgui_widget *parent = find_widget(win, w->parent_id);
			if(parent && parent->type == MKGUI_TOOLBAR) {
				snprintf(win->tooltip_texts[i], MKGUI_MAX_TEXT, "%s", w->label);
			}
		}
	}
}

// [=]===^=[ mkgui_grow_widgets ]=================================[=]
static uint32_t mkgui_grow_widgets(struct mkgui_window *win) {
	uint32_t new_cap = win->widget_cap + MKGUI_GROW_WIDGETS;
	if(!vm_arena_ensure(&win->arenas.widgets, (size_t)new_cap * sizeof(struct mkgui_widget))) {
		return 0;
	}
	if(!vm_arena_ensure(&win->arenas.rects, (size_t)new_cap * sizeof(struct mkgui_rect))) {
		return 0;
	}
	if(!vm_arena_ensure(&win->arenas.tooltip_texts, (size_t)new_cap * MKGUI_MAX_TEXT)) {
		return 0;
	}
	win->widget_cap = new_cap;
	return 1;
}

// [=]===^=[ mkgui_widget_add ]===================================[=]
MKGUI_API uint32_t mkgui_widget_add(struct mkgui_window *win, struct mkgui_widget w, uint32_t after_id) {
	MKGUI_CHECK_VAL(win, 0);
	if(win->widget_count >= win->widget_cap) {
		if(!mkgui_grow_widgets(win)) {
			return 0;
		}
	}

	uint32_t pos = win->widget_count;
	if(after_id) {
		for(uint32_t i = 0; i < win->widget_count; ++i) {
			if(win->widgets[i].id == after_id) {
				pos = i + 1;
				break;
			}
		}
	}

	if(pos < win->widget_count) {
		uint32_t tail = win->widget_count - pos;
		memmove(&win->widgets[pos + 1], &win->widgets[pos], tail * sizeof(struct mkgui_widget));
		memmove(&win->rects[pos + 1], &win->rects[pos], tail * sizeof(struct mkgui_rect));
		memmove(&win->tooltip_texts[pos + 1], &win->tooltip_texts[pos], tail * MKGUI_MAX_TEXT);
	}

	win->widgets[pos] = w;
	win->widgets[pos].label_tw = -1;
	memset(&win->rects[pos], 0, sizeof(struct mkgui_rect));
	win->tooltip_texts[pos][0] = '\0';
	++win->widget_count;

	init_widget_aux(win, &win->widgets[pos]);

	if(w.type == MKGUI_BUTTON && w.label[0] != '\0') {
		struct mkgui_widget *parent = find_widget(win, w.parent_id);
		if(parent && parent->type == MKGUI_TOOLBAR) {
			snprintf(win->tooltip_texts[pos], MKGUI_MAX_TEXT, "%s", w.label);
		}
	}

	if(w.icon[0] != '\0') {
		icon_resolve(win, w.icon);
	}

	dirty_all(win);
	return 1;
}

// [=]===^=[ mkgui_remove_aux_ ]==================================[=]
static void mkgui_remove_aux_(struct mkgui_window *win, uint32_t id, uint32_t type) {
	switch(type) {
		case MKGUI_INPUT: {
			for(uint32_t i = 0; i < win->input_count; ++i) {
				if(win->inputs[i].widget_id == id) {
					if(i < win->input_count - 1) {
						win->inputs[i] = win->inputs[win->input_count - 1];
					}
					--win->input_count;
					break;
				}
			}
		} break;

		case MKGUI_IPINPUT: {
			for(uint32_t i = 0; i < win->ipinput_count; ++i) {
				if(win->ipinputs[i].widget_id == id) {
					if(i < win->ipinput_count - 1) {
						win->ipinputs[i] = win->ipinputs[win->ipinput_count - 1];
					}
					--win->ipinput_count;
					break;
				}
			}
		} break;

		case MKGUI_TOGGLE: {
			for(uint32_t i = 0; i < win->toggle_count; ++i) {
				if(win->toggles[i].widget_id == id) {
					if(i < win->toggle_count - 1) {
						win->toggles[i] = win->toggles[win->toggle_count - 1];
					}
					--win->toggle_count;
					break;
				}
			}
		} break;

		case MKGUI_COMBOBOX: {
			for(uint32_t i = 0; i < win->combobox_count; ++i) {
				if(win->comboboxes[i].widget_id == id) {
					if(i < win->combobox_count - 1) {
						win->comboboxes[i] = win->comboboxes[win->combobox_count - 1];
					}
					--win->combobox_count;
					break;
				}
			}
		} break;

		case MKGUI_DATEPICKER: {
			for(uint32_t i = 0; i < win->datepicker_count; ++i) {
				if(win->datepickers[i].widget_id == id) {
					if(i < win->datepicker_count - 1) {
						win->datepickers[i] = win->datepickers[win->datepicker_count - 1];
					}
					--win->datepicker_count;
					break;
				}
			}
		} break;

		case MKGUI_DROPDOWN: {
			for(uint32_t i = 0; i < win->dropdown_count; ++i) {
				if(win->dropdowns[i].widget_id == id) {
					if(i < win->dropdown_count - 1) {
						win->dropdowns[i] = win->dropdowns[win->dropdown_count - 1];
					}
					--win->dropdown_count;
					break;
				}
			}
		} break;

		case MKGUI_SLIDER: {
			for(uint32_t i = 0; i < win->slider_count; ++i) {
				if(win->sliders[i].widget_id == id) {
					if(i < win->slider_count - 1) {
						win->sliders[i] = win->sliders[win->slider_count - 1];
					}
					--win->slider_count;
					break;
				}
			}
		} break;

		case MKGUI_TABS: {
			for(uint32_t i = 0; i < win->tab_count; ++i) {
				if(win->tabs[i].widget_id == id) {
					if(i < win->tab_count - 1) {
						win->tabs[i] = win->tabs[win->tab_count - 1];
					}
					--win->tab_count;
					break;
				}
			}
		} break;

		case MKGUI_VBOX:
		case MKGUI_HBOX: {
			for(uint32_t i = 0; i < win->box_scroll_count; ++i) {
				if(win->box_scrolls[i].widget_id == id) {
					if(i < win->box_scroll_count - 1) {
						win->box_scrolls[i] = win->box_scrolls[win->box_scroll_count - 1];
					}
					--win->box_scroll_count;
					break;
				}
			}
		} break;

		case MKGUI_HSPLIT:
		case MKGUI_VSPLIT: {
			for(uint32_t i = 0; i < win->split_count; ++i) {
				if(win->splits[i].widget_id == id) {
					if(i < win->split_count - 1) {
						win->splits[i] = win->splits[win->split_count - 1];
					}
					--win->split_count;
					break;
				}
			}
		} break;

		case MKGUI_TREEVIEW: {
			for(uint32_t i = 0; i < win->treeview_count; ++i) {
				if(win->treeviews[i].widget_id == id) {
					free(win->treeviews[i].nodes);
					if(i < win->treeview_count - 1) {
						win->treeviews[i] = win->treeviews[win->treeview_count - 1];
					}
					--win->treeview_count;
					break;
				}
			}
		} break;

		case MKGUI_STATUSBAR: {
			for(uint32_t i = 0; i < win->statusbar_count; ++i) {
				if(win->statusbars[i].widget_id == id) {
					if(i < win->statusbar_count - 1) {
						win->statusbars[i] = win->statusbars[win->statusbar_count - 1];
					}
					--win->statusbar_count;
					break;
				}
			}
		} break;

		case MKGUI_SPINBOX: {
			for(uint32_t i = 0; i < win->spinbox_count; ++i) {
				if(win->spinboxes[i].widget_id == id) {
					if(i < win->spinbox_count - 1) {
						win->spinboxes[i] = win->spinboxes[win->spinbox_count - 1];
					}
					--win->spinbox_count;
					break;
				}
			}
		} break;

		case MKGUI_PROGRESS: {
			for(uint32_t i = 0; i < win->progress_count; ++i) {
				if(win->progress[i].widget_id == id) {
					if(i < win->progress_count - 1) {
						win->progress[i] = win->progress[win->progress_count - 1];
					}
					--win->progress_count;
					break;
				}
			}
		} break;

		case MKGUI_METER: {
			for(uint32_t i = 0; i < win->meter_count; ++i) {
				if(win->meters[i].widget_id == id) {
					if(i < win->meter_count - 1) {
						win->meters[i] = win->meters[win->meter_count - 1];
					}
					--win->meter_count;
					break;
				}
			}
		} break;

		case MKGUI_TEXTAREA: {
			for(uint32_t i = 0; i < win->textarea_count; ++i) {
				if(win->textareas[i].widget_id == id) {
					free(win->textareas[i].text);
					if(i < win->textarea_count - 1) {
						win->textareas[i] = win->textareas[win->textarea_count - 1];
					}
					--win->textarea_count;
					break;
				}
			}
		} break;

		case MKGUI_LOGVIEW: {
			for(uint32_t i = 0; i < win->logview_count; ++i) {
				if(win->logviews[i].widget_id == id) {
					struct mkgui_logview_data *lv = &win->logviews[i];
					free(lv->text_arena);
					free(lv->lines);
					free(lv->runs);
					free(lv->pending_text);
					free(lv->pending_runs);
					free(lv->vlines);
					if(i < win->logview_count - 1) {
						win->logviews[i] = win->logviews[win->logview_count - 1];
					}
					--win->logview_count;
					break;
				}
			}
		} break;

		case MKGUI_ITEMVIEW: {
			for(uint32_t i = 0; i < win->itemview_count; ++i) {
				if(win->itemviews[i].widget_id == id) {
					free(win->itemviews[i].thumb_buf);
					if(i < win->itemview_count - 1) {
						win->itemviews[i] = win->itemviews[win->itemview_count - 1];
					}
					--win->itemview_count;
					break;
				}
			}
		} break;

		case MKGUI_LISTVIEW: {
			for(uint32_t i = 0; i < win->listv_count; ++i) {
				if(win->listvs[i].widget_id == id) {
					if(i < win->listv_count - 1) {
						win->listvs[i] = win->listvs[win->listv_count - 1];
					}
					--win->listv_count;
					break;
				}
			}
		} break;

		case MKGUI_GRIDVIEW: {
			for(uint32_t i = 0; i < win->gridview_count; ++i) {
				if(win->gridviews[i].widget_id == id) {
					free(win->gridviews[i].checks);
					if(i < win->gridview_count - 1) {
						win->gridviews[i] = win->gridviews[win->gridview_count - 1];
					}
					--win->gridview_count;
					break;
				}
			}
		} break;

		case MKGUI_RICHLIST: {
			for(uint32_t i = 0; i < win->richlist_count; ++i) {
				if(win->richlists[i].widget_id == id) {
					if(i < win->richlist_count - 1) {
						win->richlists[i] = win->richlists[win->richlist_count - 1];
					}
					--win->richlist_count;
					break;
				}
			}
		} break;

		case MKGUI_SCROLLBAR: {
			for(uint32_t i = 0; i < win->scrollbar_count; ++i) {
				if(win->scrollbars[i].id == id) {
					if(i < win->scrollbar_count - 1) {
						win->scrollbars[i] = win->scrollbars[win->scrollbar_count - 1];
					}
					--win->scrollbar_count;
					break;
				}
			}
		} break;

		case MKGUI_IMAGE: {
			for(uint32_t i = 0; i < win->image_count; ++i) {
				if(win->images[i].id == id) {
					free(win->images[i].pixels);
					if(i < win->image_count - 1) {
						win->images[i] = win->images[win->image_count - 1];
					}
					--win->image_count;
					break;
				}
			}
		} break;

		case MKGUI_GLVIEW: {
			for(uint32_t i = 0; i < win->glview_count; ++i) {
				if(win->glviews[i].id == id) {
					if(win->glviews[i].created) {
						platform_glview_destroy(win, &win->glviews[i]);
					}

					if(i < win->glview_count - 1) {
						win->glviews[i] = win->glviews[win->glview_count - 1];
					}
					--win->glview_count;
					break;
				}
			}
		} break;

		case MKGUI_CANVAS: {
			for(uint32_t i = 0; i < win->canvas_count; ++i) {
				if(win->canvases[i].widget_id == id) {
					if(i < win->canvas_count - 1) {
						win->canvases[i] = win->canvases[win->canvas_count - 1];
					}
					--win->canvas_count;
					break;
				}
			}
		} break;

		case MKGUI_PATHBAR: {
			for(uint32_t i = 0; i < win->pathbar_count; ++i) {
				if(win->pathbars[i].widget_id == id) {
					if(i < win->pathbar_count - 1) {
						win->pathbars[i] = win->pathbars[win->pathbar_count - 1];
					}
					--win->pathbar_count;
					break;
				}
			}
		} break;

		default: {
		} break;
	}
}

// [=]===^=[ mkgui_widget_remove ]================================[=]
MKGUI_API uint32_t mkgui_widget_remove(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, 0);
	int32_t idx = find_widget_idx(win, id);
	if(idx < 0) {
		return 0;
	}

	if(win->widgets[idx].type == MKGUI_WINDOW) {
		return 0;
	}

	uint8_t *marked = (uint8_t *)calloc(win->widget_count, 1);
	if(!marked) {
		return 0;
	}
	marked[idx] = 1;
	uint32_t changed = 1;
	while(changed) {
		changed = 0;
		for(uint32_t i = 0; i < win->widget_count; ++i) {
			if(!marked[i]) {
				int32_t pidx = find_widget_idx(win, win->widgets[i].parent_id);
				if(pidx >= 0 && marked[pidx]) {
					marked[i] = 1;
					changed = 1;
				}
			}
		}
	}

	for(uint32_t i = 0; i < win->widget_count; ++i) {
		if(marked[i]) {
			uint32_t wid = win->widgets[i].id;
			if(win->hover_id == wid) {
				win->hover_id = 0;
			}

			if(win->prev_hover_id == wid) {
				win->prev_hover_id = 0;
			}

			if(win->press_id == wid) {
				win->press_id = 0;
			}

			if(win->focus_id == wid) {
				win->focus_id = 0;
			}

			if(win->prev_focus_id == wid) {
				win->prev_focus_id = 0;
			}

			if(win->drag_scrollbar_id == wid) {
				win->drag_scrollbar_id = 0;
			}

			if(win->drag_col_id == wid) {
				win->drag_col_id = 0;
			}

			if(win->drag_col_resize_id == wid) {
				win->drag_col_resize_id = 0;
			}

			if(win->drag_select_id == wid) {
				win->drag_select_id = 0;
			}

			if(win->drag_text_id == wid) {
				win->drag_text_id = 0;
			}

			if(win->dblclick_id == wid) {
				win->dblclick_id = 0;
			}

			if(win->divider_dblclick_id == wid) {
				win->divider_dblclick_id = 0;
			}

			if(win->tooltip_id == wid) {
				win->tooltip_id = 0;
			}

			for(uint32_t p = 0; p < win->popup_count; ++p) {
				if(win->popups[p].widget_id == wid) {
					popup_destroy(win, p);
					break;
				}
			}

			mkgui_remove_aux_(win, wid, win->widgets[i].type);
		}
	}

	for(int32_t i = (int32_t)win->widget_count - 1; i >= 0; --i) {
		if(marked[i]) {
			uint32_t last = win->widget_count - 1;
			if((uint32_t)i < last) {
				uint32_t tail = last - (uint32_t)i;
				memmove(&win->widgets[i], &win->widgets[i + 1], tail * sizeof(struct mkgui_widget));
				memmove(&win->rects[i], &win->rects[i + 1], tail * sizeof(struct mkgui_rect));
				memmove(&win->tooltip_texts[i], &win->tooltip_texts[i + 1], tail * MKGUI_MAX_TEXT);
				for(uint32_t j = (uint32_t)i; j < last; ++j) {
					marked[j] = marked[j + 1];
				}
			}
			--win->widget_count;
		}
	}

	free(marked);
	dirty_all(win);
	return 1;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// Reserve a per-win vm_arena large enough for the practical maximum element
// count of one aux array. The reserve is virtual-only (PROT_NONE on Linux,
// MEM_RESERVE on Windows) so the cost is address-space, not RAM. Pages get
// faulted in zero-filled as MKGUI_AUX_GROW bumps the committed footprint.
// [=]===^=[ mkgui_aux_arena_create ]=============================[=]
static uint32_t mkgui_aux_arena_create(struct vm_arena *a, size_t elem_size, uint32_t initial_cap) {
	size_t reserve_bytes = (size_t)MKGUI_VM_MAX_WIDGETS * elem_size;
	if(!vm_arena_create(a, reserve_bytes)) {
		return 0;
	}
	if(initial_cap > 0) {
		if(!vm_arena_ensure(a, (size_t)initial_cap * elem_size)) {
			vm_arena_destroy(a);
			return 0;
		}
	}
	return 1;
}

// [=]===^=[ mkgui_alloc_arrays ]=================================[=]
static uint32_t mkgui_alloc_arrays(struct mkgui_window *win, uint32_t widget_cap) {
	if(!mkgui_aux_arena_create(&win->arenas.widgets,       sizeof(struct mkgui_widget),        widget_cap)) return 0;
	if(!mkgui_aux_arena_create(&win->arenas.rects,         sizeof(struct mkgui_rect),          widget_cap)) return 0;
	if(!mkgui_aux_arena_create(&win->arenas.tooltip_texts, MKGUI_MAX_TEXT,                     widget_cap)) return 0;
	win->widgets        = (struct mkgui_widget *)win->arenas.widgets.base;
	win->rects          = (struct mkgui_rect *)win->arenas.rects.base;
	win->tooltip_texts  = (char (*)[MKGUI_MAX_TEXT])win->arenas.tooltip_texts.base;
	win->widget_cap = widget_cap;

	win->listv_cap      = 32;
	win->input_cap      = 64;
	win->dropdown_cap   = 32;
	win->slider_cap     = 32;
	win->tab_cap        = 32;
	win->split_cap      = 32;
	win->treeview_cap   = 8;
	win->statusbar_cap  = 8;
	win->spinbox_cap    = 32;
	win->progress_cap   = 32;
	win->meter_cap      = 32;
	win->textarea_cap   = 16;
	win->logview_cap    = 8;
	win->itemview_cap   = 16;
	win->scrollbar_cap  = 32;
	win->box_scroll_cap = 32;
	win->image_cap      = 32;
	win->glview_cap     = 8;
	win->canvas_cap     = 16;
	win->pathbar_cap    = 8;
	win->ipinput_cap    = 16;
	win->toggle_cap     = 32;
	win->combobox_cap   = 16;
	win->datepicker_cap = 16;
	win->gridview_cap   = 16;
	win->richlist_cap   = 16;

	if(!mkgui_aux_arena_create(&win->arenas.listvs,      sizeof(struct mkgui_listview_data),   win->listv_cap))      return 0;
	if(!mkgui_aux_arena_create(&win->arenas.inputs,      sizeof(struct mkgui_input_data),      win->input_cap))      return 0;
	if(!mkgui_aux_arena_create(&win->arenas.dropdowns,   sizeof(struct mkgui_dropdown_data),   win->dropdown_cap))   return 0;
	if(!mkgui_aux_arena_create(&win->arenas.sliders,     sizeof(struct mkgui_slider_data),     win->slider_cap))     return 0;
	if(!mkgui_aux_arena_create(&win->arenas.tabs,        sizeof(struct mkgui_tabs_data),       win->tab_cap))        return 0;
	if(!mkgui_aux_arena_create(&win->arenas.splits,      sizeof(struct mkgui_split_data),      win->split_cap))      return 0;
	if(!mkgui_aux_arena_create(&win->arenas.treeviews,   sizeof(struct mkgui_treeview_data),   win->treeview_cap))   return 0;
	if(!mkgui_aux_arena_create(&win->arenas.statusbars,  sizeof(struct mkgui_statusbar_data),  win->statusbar_cap))  return 0;
	if(!mkgui_aux_arena_create(&win->arenas.spinboxes,   sizeof(struct mkgui_spinbox_data),    win->spinbox_cap))    return 0;
	if(!mkgui_aux_arena_create(&win->arenas.progress,    sizeof(struct mkgui_progress_data),   win->progress_cap))   return 0;
	if(!mkgui_aux_arena_create(&win->arenas.meters,      sizeof(struct mkgui_meter_data),      win->meter_cap))      return 0;
	if(!mkgui_aux_arena_create(&win->arenas.textareas,   sizeof(struct mkgui_textarea_data),   win->textarea_cap))   return 0;
	if(!mkgui_aux_arena_create(&win->arenas.logviews,    sizeof(struct mkgui_logview_data),    win->logview_cap))    return 0;
	if(!mkgui_aux_arena_create(&win->arenas.itemviews,   sizeof(struct mkgui_itemview_data),   win->itemview_cap))   return 0;
	if(!mkgui_aux_arena_create(&win->arenas.scrollbars,  sizeof(struct mkgui_scrollbar_data),  win->scrollbar_cap))  return 0;
	if(!mkgui_aux_arena_create(&win->arenas.box_scrolls, sizeof(struct mkgui_box_scroll),      win->box_scroll_cap)) return 0;
	if(!mkgui_aux_arena_create(&win->arenas.images,      sizeof(struct mkgui_image_data),      win->image_cap))      return 0;
	if(!mkgui_aux_arena_create(&win->arenas.glviews,     sizeof(struct mkgui_glview_data),     win->glview_cap))     return 0;
	if(!mkgui_aux_arena_create(&win->arenas.canvases,    sizeof(struct mkgui_canvas_data),     win->canvas_cap))     return 0;
	if(!mkgui_aux_arena_create(&win->arenas.pathbars,    sizeof(struct mkgui_pathbar_data),    win->pathbar_cap))    return 0;
	if(!mkgui_aux_arena_create(&win->arenas.ipinputs,    sizeof(struct mkgui_ipinput_data),    win->ipinput_cap))    return 0;
	if(!mkgui_aux_arena_create(&win->arenas.toggles,     sizeof(struct mkgui_toggle_data),     win->toggle_cap))     return 0;
	if(!mkgui_aux_arena_create(&win->arenas.comboboxes,  sizeof(struct mkgui_combobox_data),   win->combobox_cap))   return 0;
	if(!mkgui_aux_arena_create(&win->arenas.datepickers, sizeof(struct mkgui_datepicker_data), win->datepicker_cap)) return 0;
	if(!mkgui_aux_arena_create(&win->arenas.gridviews,   sizeof(struct mkgui_gridview_data),   win->gridview_cap))   return 0;
	if(!mkgui_aux_arena_create(&win->arenas.richlists,   sizeof(struct mkgui_richlist_data),   win->richlist_cap))   return 0;

	win->listvs      = (struct mkgui_listview_data *)win->arenas.listvs.base;
	win->inputs      = (struct mkgui_input_data *)win->arenas.inputs.base;
	win->dropdowns   = (struct mkgui_dropdown_data *)win->arenas.dropdowns.base;
	win->sliders     = (struct mkgui_slider_data *)win->arenas.sliders.base;
	win->tabs        = (struct mkgui_tabs_data *)win->arenas.tabs.base;
	win->splits      = (struct mkgui_split_data *)win->arenas.splits.base;
	win->treeviews   = (struct mkgui_treeview_data *)win->arenas.treeviews.base;
	win->statusbars  = (struct mkgui_statusbar_data *)win->arenas.statusbars.base;
	win->spinboxes   = (struct mkgui_spinbox_data *)win->arenas.spinboxes.base;
	win->progress    = (struct mkgui_progress_data *)win->arenas.progress.base;
	win->meters      = (struct mkgui_meter_data *)win->arenas.meters.base;
	win->textareas   = (struct mkgui_textarea_data *)win->arenas.textareas.base;
	win->logviews    = (struct mkgui_logview_data *)win->arenas.logviews.base;
	win->itemviews   = (struct mkgui_itemview_data *)win->arenas.itemviews.base;
	win->scrollbars  = (struct mkgui_scrollbar_data *)win->arenas.scrollbars.base;
	win->box_scrolls = (struct mkgui_box_scroll *)win->arenas.box_scrolls.base;
	win->images      = (struct mkgui_image_data *)win->arenas.images.base;
	win->glviews     = (struct mkgui_glview_data *)win->arenas.glviews.base;
	win->canvases    = (struct mkgui_canvas_data *)win->arenas.canvases.base;
	win->pathbars    = (struct mkgui_pathbar_data *)win->arenas.pathbars.base;
	win->ipinputs    = (struct mkgui_ipinput_data *)win->arenas.ipinputs.base;
	win->toggles     = (struct mkgui_toggle_data *)win->arenas.toggles.base;
	win->comboboxes  = (struct mkgui_combobox_data *)win->arenas.comboboxes.base;
	win->datepickers = (struct mkgui_datepicker_data *)win->arenas.datepickers.base;
	win->gridviews   = (struct mkgui_gridview_data *)win->arenas.gridviews.base;
	win->richlists   = (struct mkgui_richlist_data *)win->arenas.richlists.base;

	return 1;
}

// [=]===^=[ mkgui_free_arrays ]==================================[=]
static void mkgui_free_arrays(struct mkgui_window *win) {
	for(uint32_t i = 0; i < win->gridview_count; ++i) {
		free(win->gridviews[i].checks);
	}
	vm_arena_destroy(&win->arenas.widgets);
	vm_arena_destroy(&win->arenas.rects);
	vm_arena_destroy(&win->arenas.tooltip_texts);
	vm_arena_destroy(&win->arenas.listvs);
	vm_arena_destroy(&win->arenas.inputs);
	vm_arena_destroy(&win->arenas.dropdowns);
	vm_arena_destroy(&win->arenas.sliders);
	vm_arena_destroy(&win->arenas.tabs);
	vm_arena_destroy(&win->arenas.splits);
	vm_arena_destroy(&win->arenas.treeviews);
	vm_arena_destroy(&win->arenas.statusbars);
	vm_arena_destroy(&win->arenas.spinboxes);
	vm_arena_destroy(&win->arenas.progress);
	vm_arena_destroy(&win->arenas.meters);
	vm_arena_destroy(&win->arenas.textareas);
	vm_arena_destroy(&win->arenas.logviews);
	vm_arena_destroy(&win->arenas.itemviews);
	vm_arena_destroy(&win->arenas.scrollbars);
	vm_arena_destroy(&win->arenas.box_scrolls);
	vm_arena_destroy(&win->arenas.images);
	vm_arena_destroy(&win->arenas.glviews);
	vm_arena_destroy(&win->arenas.canvases);
	vm_arena_destroy(&win->arenas.pathbars);
	vm_arena_destroy(&win->arenas.ipinputs);
	vm_arena_destroy(&win->arenas.toggles);
	vm_arena_destroy(&win->arenas.comboboxes);
	vm_arena_destroy(&win->arenas.datepickers);
	vm_arena_destroy(&win->arenas.gridviews);
	vm_arena_destroy(&win->arenas.richlists);
	win->widgets = NULL;
	win->rects = NULL;
	win->tooltip_texts = NULL;
}

// [=]===^=[ mkgui_ctx_create ]===================================[=]
// Allocate the application-level context. No display is opened here; the
// first mkgui_window_create on this ctx is what opens it.
MKGUI_API struct mkgui_ctx *mkgui_ctx_create(void) {
	struct mkgui_ctx *ctx = (struct mkgui_ctx *)calloc(1, sizeof(struct mkgui_ctx));
	if(!ctx) {
		return NULL;
	}
	ctx->theme = auto_theme();
	ctx->scale = 1.0f;
	g_ctx = ctx;
	return ctx;
}

// [=]===^=[ mkgui_window_create ]================================[=]
// Create a window on the given ctx. parent==NULL: top-level window. parent
// non-NULL: transient/dialog window of that parent (shares display, set as
// transient_for). title/w/h override the MKGUI_WINDOW widget in the array;
// pass NULL/0 to inherit from the widget.
MKGUI_API struct mkgui_window *mkgui_window_create(struct mkgui_ctx *ctx, struct mkgui_window *parent, const struct mkgui_widget *widgets, uint32_t count, const char *title, int32_t w, int32_t h) {
	if(!ctx || (!widgets && count > 0)) {
		return NULL;
	}
	if(ctx->window_count >= MKGUI_MAX_WINDOWS) {
		return NULL;
	}
	struct mkgui_window *win = (struct mkgui_window *)calloc(1, sizeof(struct mkgui_window));

	uint32_t cap = (count + MKGUI_GROW_WIDGETS - 1) & ~(uint32_t)(MKGUI_GROW_WIDGETS - 1);
	if(cap < MKGUI_GROW_WIDGETS) {
		cap = MKGUI_GROW_WIDGETS;
	}

	if(!mkgui_alloc_arrays(win, cap)) {
		free(win);
		return NULL;
	}
	memcpy(win->widgets, widgets, count * sizeof(struct mkgui_widget));
	win->widget_count = count;
	win->ctx = ctx;

	int32_t init_w = w > 0 ? w : 800;
	int32_t init_h = h > 0 ? h : 600;
	const char *use_title = title ? title : (parent ? "mkgui" : "mkgui");
	uint32_t window_style = 0;
	for(uint32_t i = 0; i < count; ++i) {
		if(widgets[i].type == MKGUI_WINDOW) {
			if(w <= 0 && widgets[i].w > 0) {
				init_w = widgets[i].w;
			}

			if(h <= 0 && widgets[i].h > 0) {
				init_h = widgets[i].h;
			}

			if(!title && widgets[i].label[0]) {
				use_title = win->widgets[i].label;
			}
			window_style = widgets[i].style;
			break;
		}
	}

	// Effective parent for display sharing: explicit parent if given, else
	// the first existing window on this ctx (any will do since they share
	// the X display). NULL means this is the first window and platform_init
	// must open the display.
	struct mkgui_window *share_from = parent;
	if(!share_from && ctx->window_count > 0) {
		share_from = ctx->windows[0];
	}

	win->scale = share_from ? share_from->scale : ctx->scale;
	mkgui_recompute_metrics(win);

	uint32_t plat_ok;
	if(share_from) {
		int32_t sw = parent ? sc(win, init_w) : init_w;
		int32_t sh = parent ? sc(win, init_h) : init_h;
		plat_ok = platform_init_child(win, share_from, parent, use_title, sw, sh, window_style);
	} else {
		// Copy ctx's app_class onto the window so platform_init can read it
		// from the conventional place; mkgui_ctx_set_app_class keeps the two
		// in sync.
		snprintf(win->app_class, sizeof(win->app_class), "%s", ctx->app_class);
		plat_ok = platform_init(win, use_title, init_w, init_h, window_style);
	}
	if(!plat_ok) {
		mkgui_free_arrays(win);
		free(win);
		return NULL;
	}

	win->hide_on_close = (window_style & MKGUI_WINDOW_HIDE_ON_CLOSE) ? 1 : 0;
	win->undecorated   = (window_style & MKGUI_WINDOW_UNDECORATED) ? 1 : 0;
	win->canvas_window = (window_style & MKGUI_WINDOW_CANVAS) ? 1 : 0;

	if(!share_from) {
		float detected = platform_detect_scale(win);
		if(detected > 1.01f) {
			win->scale = detected;
			mkgui_recompute_metrics(win);
			int32_t new_w = (int32_t)(init_w * detected + 0.5f);
			int32_t new_h = (int32_t)(init_h * detected + 0.5f);
			platform_resize_window(win, new_w, new_h);
		}
	}

	render_clip_x1 = 0;
	render_clip_y1 = 0;
	render_clip_x2 = win->win_w;
	render_clip_y2 = win->win_h;

	if(!share_from) {
		if(!text_cmd_arena.base) {
			if(!text_cmd_init()) {
				mkgui_free_arrays(win);
				platform_window_destroy(win);
				platform_ctx_destroy(win);
				free(win);
				return NULL;
			}
		}

		if(!layout_arena_init()) {
			mkgui_free_arrays(win);
			platform_window_destroy(win);
			platform_ctx_destroy(win);
			free(win);
			return NULL;
		}
		platform_font_init(win);
		mkgui_icon_init();
	} else {
		memcpy(win->glyphs, share_from->glyphs, sizeof(win->glyphs));
		win->font_ascent = share_from->font_ascent;
		win->font_height = share_from->font_height;
		win->char_width  = share_from->char_width;
	}

	win->theme = share_from ? share_from->theme : ctx->theme;
	icon_load_from_widgets(win);
	win->parent = parent;
	dirty_all(win);
#ifdef _WIN32
	if(share_from) {
		win->perf_freq = share_from->perf_freq;
	} else {
		QueryPerformanceFrequency((LARGE_INTEGER *)&win->perf_freq);
	}
	QueryPerformanceCounter((LARGE_INTEGER *)&win->anim_prev);
#else
	clock_gettime(CLOCK_MONOTONIC, &win->anim_prev);
#endif
	win->anim_time = 0.0;

	init_aux_data(win);
	ctx->windows[ctx->window_count++] = win;
	if(!ctx->primary) {
		ctx->primary = win;
	}

	if(!(window_style & MKGUI_WINDOW_HIDDEN)) {
		platform_window_map(win);
		win->window_visible = 1;
	}

	return win;
}

// [=]===^=[ mkgui_ctx_set_app_class ]================================[=]
// Sets the WM_CLASS / Win32 class name used by future windows. Also
// updates every existing window on this ctx so the WM picks up the
// change immediately.
MKGUI_API void mkgui_ctx_set_app_class(struct mkgui_ctx *ctx, const char *app_class) {
	MKGUI_CHECK(ctx);
	MKGUI_CHECK(app_class);
	snprintf(ctx->app_class, sizeof(ctx->app_class), "%s", app_class);
	for(uint32_t i = 0; i < ctx->window_count; ++i) {
		struct mkgui_window *win = ctx->windows[i];
		snprintf(win->app_class, sizeof(win->app_class), "%s", app_class);
		platform_set_class_hint(&win->plat, win->plat.is_child ? "dialog" : "main", win->app_class);
	}
}

// [=]===^=[ mkgui_window_set_title ]======================================[=]
MKGUI_API void mkgui_window_set_title(struct mkgui_window *win, const char *title) {
	MKGUI_CHECK(win);
	if(!title) {
		title = "";
	}
#ifdef _WIN32
	SetWindowTextA(win->plat.hwnd, title);
#else
	XStoreName(win->plat.dpy, win->plat.win, title);
#endif
}

// [=]===^=[ mkgui_window_set_instance ]============================[=]
MKGUI_API void mkgui_window_set_instance(struct mkgui_window *win, const char *instance) {
	MKGUI_CHECK(win);
	MKGUI_CHECK(instance);
	char *cls = "mkgui";
	if(win->app_class[0]) {
		cls = win->app_class;
	} else if(win->parent && win->parent->app_class[0]) {
		cls = win->parent->app_class;
	}
	platform_set_class_hint(&win->plat, instance, cls);
}

// [=]===^=[ mkgui_window_set_icon ]================================[=]
MKGUI_API void mkgui_window_set_icon(struct mkgui_window *win, const struct mkgui_icon_size *sizes, uint32_t count) {
	MKGUI_CHECK(win);
	MKGUI_CHECK(sizes);
	if(count == 0) {
		return;
	}
	platform_set_window_icon(&win->plat, sizes, count);
}

// [=]===^=[ mkgui_ctx_set_theme ]====================================[=]
// Sets the default theme for new windows and applies to every existing
// window on this ctx.
MKGUI_API void mkgui_ctx_set_theme(struct mkgui_ctx *ctx, struct mkgui_theme theme) {
	MKGUI_CHECK(ctx);
	ctx->theme = theme;
	for(uint32_t i = 0; i < ctx->window_count; ++i) {
		struct mkgui_window *win = ctx->windows[i];
		win->theme = theme;
		svg_rerasterize_all(win, 1.0f);
		dirty_all(win);
	}
}

// [=]===^=[ mkgui_ctx_set_scale ]=====================================[=]
// Sets the default scale for new windows and applies to every existing
// window on this ctx.
MKGUI_API void mkgui_ctx_set_scale(struct mkgui_ctx *ctx, float scale) {
	MKGUI_CHECK(ctx);
	if(scale < 0.5f) {
		scale = 0.5f;
	}
	if(scale > 4.0f) {
		scale = 4.0f;
	}
	ctx->scale = scale;
	for(uint32_t i = 0; i < ctx->window_count; ++i) {
		struct mkgui_window *win = ctx->windows[i];
		float old_scale = win->scale > 0.0f ? win->scale : 1.0f;
		win->scale = scale;
		mkgui_recompute_metrics(win);
		platform_font_set_size(win, (int32_t)(13.0f * scale + 0.5f));
		svg_rerasterize_all(win, scale / old_scale);
		dirty_all(win);
	}
}

// [=]===^=[ mkgui_ctx_get_scale ]=====================================[=]
MKGUI_API float mkgui_ctx_get_scale(struct mkgui_ctx *ctx) {
	MKGUI_CHECK_VAL(ctx, 1.0f);
	return ctx->scale;
}

// [=]===^=[ mkgui_set_weight ]===================================[=]
MKGUI_API void mkgui_set_weight(struct mkgui_window *win, uint32_t id, uint32_t weight) {
	MKGUI_CHECK(win);
	struct mkgui_widget *w = find_widget(win, id);
	if(w) {
		w->weight = weight;
		dirty_all(win);
	}
}

// [=]===^=[ mkgui_toolbar_set_mode ]==============================[=]
MKGUI_API void mkgui_toolbar_set_mode(struct mkgui_window *win, uint32_t toolbar_id, uint32_t mode) {
	MKGUI_CHECK(win);
	struct mkgui_widget *w = find_widget(win, toolbar_id);
	if(w && w->type == MKGUI_TOOLBAR) {
		w->style = (w->style & ~MKGUI_TOOLBAR_MODE_MASK) | (mode & MKGUI_TOOLBAR_MODE_MASK);
		dirty_all(win);
	}
}

// [=]===^=[ mkgui_set_enabled ]====================================[=]
MKGUI_API void mkgui_set_enabled(struct mkgui_window *win, uint32_t id, uint32_t enabled) {
	MKGUI_CHECK(win);
	struct mkgui_widget *w = find_widget(win, id);
	if(!w) {
		return;
	}

	if(enabled) {
		w->flags &= ~MKGUI_DISABLED;

	} else {
		w->flags |= MKGUI_DISABLED;
		if(win->focus_id == id) {
			win->focus_id = 0;
		}
	}
	dirty_all(win);
}

// [=]===^=[ mkgui_get_enabled ]====================================[=]
MKGUI_API uint32_t mkgui_get_enabled(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, 0);
	struct mkgui_widget *w = find_widget(win, id);
	if(!w) {
		return 0;
	}
	return (w->flags & MKGUI_DISABLED) ? 0 : 1;
}

// [=]===^=[ mkgui_set_visible ]====================================[=]
MKGUI_API void mkgui_set_visible(struct mkgui_window *win, uint32_t id, uint32_t visible) {
	MKGUI_CHECK(win);
	struct mkgui_widget *w = find_widget(win, id);
	if(!w) {
		return;
	}

	if(visible) {
		w->flags &= ~MKGUI_HIDDEN;

	} else {
		w->flags |= MKGUI_HIDDEN;
		if(win->focus_id == id) {
			win->focus_id = 0;
		}
	}
	dirty_all(win);
}

// [=]===^=[ mkgui_get_visible ]====================================[=]
MKGUI_API uint32_t mkgui_get_visible(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, 0);
	struct mkgui_widget *w = find_widget(win, id);
	if(!w) {
		return 0;
	}
	return (w->flags & MKGUI_HIDDEN) ? 0 : 1;
}

// Effective-visibility check: walks the parent chain, accounts for hidden
// ancestors and non-active tabs. Use this to gate expensive per-frame work
// (GL rendering, etc.) that only needs to happen when the widget is actually
// on screen. mkgui_get_visible only reports the widget's own HIDDEN flag.

// [=]===^=[ mkgui_is_shown ]========================================[=]
MKGUI_API uint32_t mkgui_is_shown(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, 0);
	int32_t idx = find_widget_idx(win, id);
	if(idx < 0) {
		return 0;
	}
	return widget_visible(win, (uint32_t)idx);
}

// [=]===^=[ mkgui_set_focus ]======================================[=]
MKGUI_API void mkgui_set_focus(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK(win);
	struct mkgui_widget *w = find_widget(win, id);
	if(!w || !is_focusable(w)) {
		return;
	}
	win->focus_id = id;
	dirty_all(win);
}

// [=]===^=[ mkgui_has_focus ]======================================[=]
MKGUI_API uint32_t mkgui_has_focus(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, 0);
	return win->focus_id == id ? 1 : 0;
}

// [=]===^=[ mkgui_get_tooltip ]====================================[=]
MKGUI_API const char *mkgui_get_tooltip(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, "");
	int32_t idx = find_widget_idx(win, id);
	if(idx < 0) {
		return "";
	}
	return win->tooltip_texts[idx];
}

// [=]===^=[ mkgui_get_geometry ]====================================[=]
MKGUI_API void mkgui_get_geometry(struct mkgui_window *win, uint32_t id, int32_t *x, int32_t *y, int32_t *w, int32_t *h) {
	MKGUI_CHECK(win);
	int32_t idx = find_widget_idx(win, id);
	if(idx < 0) {
		if(x) { *x = 0; }
		if(y) { *y = 0; }
		if(w) { *w = 0; }
		if(h) { *h = 0; }
		return;
	}

	if(x) { *x = win->rects[idx].x; }
	if(y) { *y = win->rects[idx].y; }
	if(w) { *w = win->rects[idx].w; }
	if(h) { *h = win->rects[idx].h; }
}

// [=]===^=[ mkgui_get_min_size ]====================================[=]
MKGUI_API void mkgui_get_min_size(struct mkgui_window *win, int32_t *out_w, int32_t *out_h) {
	MKGUI_CHECK(win);
	if(out_w) { *out_w = 0; }
	if(out_h) { *out_h = 0; }
	layout_build_index(win);
	int32_t min_w = 0;
	int32_t min_h = 0;
	for(uint32_t i = 0; i < win->widget_count; ++i) {
		if(win->widgets[i].type != MKGUI_WINDOW) {
			continue;
		}
		for(uint32_t c = layout_first_child[i]; c < win->widget_count; c = layout_next_sibling[c]) {
			uint32_t ct = win->widgets[c].type;
			if(ct == MKGUI_MENU) {
				min_h += win->menu_height;
			} else if(ct == MKGUI_TOOLBAR) {
				min_h += win->font_height + 10;
			} else if(ct == MKGUI_STATUSBAR) {
				min_h += win->statusbar_height;
			} else if(ct == MKGUI_PATHBAR && win->widgets[c].h == 0) {
				min_h += win->pathbar_height;
			} else {
				int32_t cw, ch;
				layout_min_size(win, c, &cw, &ch);
				min_h += ch;
				if(cw > min_w) {
					min_w = cw;
				}
			}
		}
		break;
	}
	min_w += win->box_pad * 2;
	min_h += win->box_pad * 2;
	if(out_w) { *out_w = min_w; }
	if(out_h) { *out_h = min_h; }
}

// [=]===^=[ mkgui_get_window_size ]==================================[=]
MKGUI_API void mkgui_get_window_size(struct mkgui_window *win, int32_t *out_w, int32_t *out_h) {
	MKGUI_CHECK(win);
	if(out_w) { *out_w = win->win_w; }
	if(out_h) { *out_h = win->win_h; }
}

// [=]===^=[ mkgui_get_anim_time ]===================================[=]
MKGUI_API double mkgui_get_anim_time(struct mkgui_window *win) {
	MKGUI_CHECK_VAL(win, 0.0);
	return win->anim_time;
}

// [=]===^=[ mkgui_set_flags ]======================================[=]
MKGUI_API void mkgui_set_flags(struct mkgui_window *win, uint32_t id, uint32_t flags) {
	MKGUI_CHECK(win);
	struct mkgui_widget *w = find_widget(win, id);
	if(!w) {
		return;
	}
	w->flags = flags;
	dirty_all(win);
}

// [=]===^=[ mkgui_get_flags ]======================================[=]
MKGUI_API uint32_t mkgui_get_flags(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, 0);
	struct mkgui_widget *w = find_widget(win, id);
	return w ? w->flags : 0;
}

// [=]===^=[ mkgui_window_destroy ]================================[=]
// Destroy a window. Frees all per-window state. If this was the last
// window on its ctx, also tears down process-level globals (text-cmd
// arena, layout arena). The ctx itself remains valid; call
// mkgui_ctx_destroy when done.
MKGUI_API void mkgui_window_destroy(struct mkgui_window *win) {
	if(!win) {
		return;
	}
	struct mkgui_ctx *ctx = win->ctx;
	drop_free(win);
	for(uint32_t i = 0; i < ctx->window_count; ++i) {
		if(ctx->windows[i] == win) {
			ctx->windows[i] = ctx->windows[--ctx->window_count];
			break;
		}
	}
	if(ctx->primary == win) {
		ctx->primary = ctx->window_count > 0 ? ctx->windows[0] : NULL;
	}
	popup_destroy_all(win);
	for(uint32_t i = 0; i < win->treeview_count; ++i) {
		free(win->treeviews[i].nodes);
	}
	for(uint32_t i = 0; i < win->textarea_count; ++i) {
		textarea_undo_free(&win->textareas[i]);
		free(win->textareas[i].text);
	}
	for(uint32_t i = 0; i < win->logview_count; ++i) {
		free(win->logviews[i].text_arena);
		free(win->logviews[i].lines);
		free(win->logviews[i].runs);
		free(win->logviews[i].pending_text);
		free(win->logviews[i].pending_runs);
		free(win->logviews[i].vlines);
	}
	for(uint32_t i = 0; i < win->itemview_count; ++i) {
		free(win->itemviews[i].thumb_buf);
	}
	for(uint32_t i = 0; i < win->image_count; ++i) {
		free(win->images[i].pixels);
	}
	for(uint32_t i = 0; i < win->glview_count; ++i) {
		if(win->glviews[i].created) {
			platform_glview_destroy(win, &win->glviews[i]);
		}
	}
	for(uint32_t i = 0; i < win->timer_count; ++i) {
#ifdef _WIN32
		if(win->timers[i].handle) {
			CancelWaitableTimer(win->timers[i].handle);
			CloseHandle(win->timers[i].handle);
		}
#else
		if(win->timers[i].fd >= 0) {
			close(win->timers[i].fd);
		}
#endif
	}
	win->timer_count = 0;
	mkgui_free_arrays(win);
	platform_window_destroy(win);
	// Last window on this ctx: tear down ctx-shared resources (font lib,
	// SVG arenas, text-cmd arena, layout arenas, platform display/IM).
	// Order independent of which window was created first.
	if(ctx->window_count == 0) {
		platform_font_fini(win);
		svg_cleanup();
		text_cmd_fini();
		layout_arena_fini();
		platform_ctx_destroy(win);
	}
	free(win);
}

// [=]===^=[ mkgui_ctx_destroy ]==================================[=]
// Free the context. Auto-destroys any windows still alive on this ctx so
// callers that forget to destroy windows first don't leak platform
// resources.
MKGUI_API void mkgui_ctx_destroy(struct mkgui_ctx *ctx) {
	if(!ctx) {
		return;
	}
	while(ctx->window_count > 0) {
		mkgui_window_destroy(ctx->windows[ctx->window_count - 1]);
	}
	if(g_ctx == ctx) {
		g_ctx = NULL;
	}
	free(ctx);
}

// ---------------------------------------------------------------------------
// Event processing
// ---------------------------------------------------------------------------

// [=]===^=[ mkgui_resize_render_impl ]=============================[=]
static void mkgui_resize_render_impl(struct mkgui_window *win) {
#ifdef _WIN32
	RECT rc;
	GetClientRect(win->plat.hwnd, &rc);
	int32_t nw = rc.right - rc.left;
	int32_t nh = rc.bottom - rc.top;
	if(nw > 0 && nh > 0 && (nw != win->win_w || nh != win->win_h)) {
		win->win_w = nw;
		win->win_h = nh;
		platform_fb_resize(win);
		dirty_all(win);
	}
#endif
	mkgui_flush(win);
}

// [=]===^=[ mkgui_window_poll ]========================================[=]
MKGUI_API uint32_t mkgui_window_poll(struct mkgui_window *win, struct mkgui_event *ev) {
	MKGUI_CHECK_VAL(win, 0);
	if(!ev) {
		return 0;
	}
	ev->type = MKGUI_EVENT_NONE;
	ev->id = 0;
	ev->value = 0;
	ev->col = 0;
	ev->keysym = 0;
	ev->keymod = 0;

	if(toasts_expire(win)) {
		dirty_all(win);
	}

#ifdef _WIN32
	int64_t now;
	QueryPerformanceCounter((LARGE_INTEGER *)&now);
	double dt = (double)(now - win->anim_prev) / (double)win->perf_freq;
	win->anim_prev = now;
#else
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	double dt = (double)(now.tv_sec - win->anim_prev.tv_sec) + (double)(now.tv_nsec - win->anim_prev.tv_nsec) * 1e-9;
	win->anim_prev = now;
#endif
	win->anim_time += dt;

	for(uint32_t ti = 0; ti < win->timer_count; ++ti) {
		struct mkgui_timer *t = &win->timers[ti];
		if(!t->active) {
			continue;
		}
#ifdef _WIN32
		if(WaitForSingleObject(t->handle, 0) == WAIT_OBJECT_0) {
			++t->fire_count;
			LARGE_INTEGER next;
			next.QuadPart = t->epoch.QuadPart + (int64_t)(t->fire_count * (t->interval_ns / 100));
			SetWaitableTimer(t->handle, &next, 0, NULL, NULL, FALSE);
			if(t->cb) {
				t->cb(win, t->id, t->userdata);
			} else {
				ev->type = MKGUI_EVENT_TIMER;
				ev->id = t->id;
				return 1;
			}
		}
#else
		uint64_t expirations;
		if(read(t->fd, &expirations, sizeof(expirations)) == (int64_t)sizeof(expirations)) {
			if(t->cb) {
				t->cb(win, t->id, t->userdata);
			} else {
				ev->type = MKGUI_EVENT_TIMER;
				ev->id = t->id;
				return 1;
			}
		}
#endif
	}

	if(win->focus_id != win->prev_focus_id) {
		if(win->prev_focus_id) {
			ev->type = MKGUI_EVENT_UNFOCUS;
			ev->id = win->prev_focus_id;
			win->prev_focus_id = 0;
			return 1;
		}
		ev->type = MKGUI_EVENT_FOCUS;
		ev->id = win->focus_id;
		win->prev_focus_id = win->focus_id;
		return 1;
	}

	if(win->hover_id != win->prev_hover_id) {
		if(win->prev_hover_id) {
			ev->type = MKGUI_EVENT_HOVER_LEAVE;
			ev->id = win->prev_hover_id;
			win->prev_hover_id = 0;
			return 1;
		}
		ev->type = MKGUI_EVENT_HOVER_ENTER;
		ev->id = win->hover_id;
		win->prev_hover_id = win->hover_id;
		return 1;
	}

	for(uint32_t si = 0; si < win->spinbox_count; ++si) {
		struct mkgui_spinbox_data *sd = &win->spinboxes[si];
		if(sd->repeat_dir && win->press_id) {
			uint32_t now_ms = (uint32_t)(mkgui_now_ns() / 1000000ull);
			if(now_ms >= sd->repeat_next_ms) {
				int32_t delta = (sd->repeat_dir > 0) ? sd->step : -sd->step;
				if(spinbox_adjust(win, ev, sd->widget_id, delta)) {
					sd->repeat_next_ms = now_ms + 80;
					return 1;
				}
			}
		}
	}

	// widget_visible walks layout_parent[], which is a file-scope scratch
	// buffer rebuilt per layout pass. If the most recent flush was for a
	// different window (e.g. parent flushed before we get here, or this is
	// our first poll), layout_parent indexes refer to that other window
	// and the visibility walk produces garbage. Rebuild for THIS window
	// before any widget-visibility check.
	layout_build_index(win);

	win->anim_active = 0;
	for(uint32_t i = 0; i < win->widget_count; ++i) {
		struct mkgui_widget *w = &win->widgets[i];
		if(!widget_visible(win, i)) {
			continue;
		}

		if(w->type == MKGUI_SPINNER || w->type == MKGUI_GLVIEW) {
			win->anim_active = 1;
		}

		if(w->type == MKGUI_PROGRESS && (w->style & MKGUI_PROGRESS_SHIMMER)) {
			struct mkgui_progress_data *pd = find_progress_data(win, w->id);
			if(pd && pd->value > 0 && pd->value < pd->max_val) {
				win->anim_active = 1;
			}
		}
	}
	for(uint32_t si = 0; si < win->spinbox_count; ++si) {
		if(win->spinboxes[si].repeat_dir) {
			win->anim_active = 1;
		}
	}

	while(platform_pending(win)) {
		struct mkgui_plat_event pev;
		platform_next_event(win, &pev);

		int32_t popup_idx = pev.popup_idx;

		switch(pev.type) {
			// -=[ EXPOSE ]=-
			case MKGUI_PLAT_EXPOSE: {
				dirty_all(win);
			} break;

			// -=[ RESIZE ]=-
			case MKGUI_PLAT_RESIZE: {
				if(pev.width != win->win_w || pev.height != win->win_h) {
					win->win_w = pev.width;
					win->win_h = pev.height;
					platform_fb_resize(win);
					dirty_all(win);
					ev->type = MKGUI_EVENT_RESIZE;
				}
			} break;

			// -=[ MOUSE MOVE ]=-
			case MKGUI_PLAT_MOTION: {
				// popups are separate platform windows with independent hover
				// tracking; a motion event targets either a popup or the main
				// window, never both, so handle popup motion and break early
				if(popup_idx >= 0) {
					struct mkgui_popup *mp = &win->popups[popup_idx];
					struct mkgui_widget *mpw = find_widget(win, mp->widget_id);

					if(mp->widget_id == MKGUI_CTXMENU_POPUP_ID) {
						int32_t new_hover = ctxmenu_hit_item(win, pev.y);
						if(new_hover != mp->hover_item) {
							mp->hover_item = new_hover;
							mp->dirty = 1;
						}

					} else if(mpw && mpw->type == MKGUI_MENUITEM) {
						int32_t hit_idx;
						struct mkgui_widget *hovered = menu_popup_hit_item(win, mpw->id, pev.y, &hit_idx);
						if(hit_idx != mp->hover_item) {
							mp->dirty = 1;
						}
						mp->hover_item = hit_idx;
						if(hovered && !(hovered->style & MKGUI_MENUITEM_SEPARATOR) && menu_item_has_children(win, hovered->id)) {
							uint32_t already_open = 0;
							if((uint32_t)popup_idx + 1 < win->popup_count) {
								if(win->popups[popup_idx + 1].widget_id == hovered->id) {
									already_open = 1;
								}
							}

							if(!already_open) {
								menu_open_submenu(win, (uint32_t)popup_idx, hovered->id);
							}

						} else {
							popup_destroy_from(win, (uint32_t)popup_idx + 1);
						}

					} else if(mpw && mpw->type == MKGUI_DATEPICKER) {
						struct mkgui_datepicker_data *mdp = find_datepicker_data(win, mpw->id);
						if(mdp) {
							int32_t new_hover = datepicker_cal_hit(win, mdp, pev.x, pev.y);
							if(new_hover != mdp->cal_hover) {
								mdp->cal_hover = new_hover;
								mp->dirty = 1;
							}
						}

					} else {
						int32_t scroll_off = 0;
						if(mpw && mpw->type == MKGUI_DROPDOWN) {
							struct mkgui_dropdown_data *mdd = find_dropdown_data(win, mpw->id);
							if(mdd) {
								scroll_off = mdd->scroll_y;
							}
						} else if(mpw && mpw->type == MKGUI_COMBOBOX) {
							struct mkgui_combobox_data *mcb = find_combobox_data(win, mpw->id);
							if(mcb) {
								scroll_off = mcb->scroll_y;
							}
						}
						int32_t new_item = (pev.y - 1 + scroll_off) / win->row_height;
						if(new_item != mp->hover_item) {
							mp->dirty = 1;
						}
						mp->hover_item = new_item;
					}

					dirty_all(win);
					break;
				}

				win->mouse_x = pev.x;
				win->mouse_y = pev.y;

				if(win->canvas_window) {
					for(uint32_t ci = 0; ci < win->canvas_count; ++ci) {
						ev->type = MKGUI_EVENT_CANVAS_MOTION;
						ev->id = win->canvases[ci].widget_id;
						ev->value = pev.x;
						ev->col = pev.y;
						ev->keysym = 0;
						ev->keymod = pev.keymod;
						dirty_all(win);
						return 1;
					}
					break;
				}

				if(win->drag_text_id) {
					int32_t dsi = find_widget_idx(win, win->drag_text_id);
					if(dsi >= 0) {
						struct mkgui_textarea_data *ta = find_textarea_data(win, win->drag_text_id);
						if(ta) {
							win->drag_text_drop_pos = textarea_hit_pos(win, ta, win->rects[dsi].x, win->rects[dsi].y, win->rects[dsi].h, win->mouse_x, win->mouse_y);
							dirty_widget(win, (uint32_t)dsi);
						}
					}
					break;
				}

				if(win->cell_edit.active && win->cell_edit.dragging) {
					struct mkgui_cell_edit *ce = &win->cell_edit;
					if(celledit_compute_rect(win)) {
						int32_t base_x = ce->cell_x + sc(win, 4) - ce->te.scroll_x;
						uint32_t hit_pos = textedit_hit_cursor(win, ce->text, base_x, win->mouse_x);
						ce->te.cursor = hit_pos;
						ce->te.sel_end = hit_pos;
						dirty_widget_id(win, ce->widget_id);
					}
					break;
				}

				if(win->drag_select_id) {
					int32_t dsi = find_widget_idx(win, win->drag_select_id);
					if(dsi >= 0 && win->widgets[dsi].type == MKGUI_INPUT) {
						struct mkgui_input_data *inp = find_input_data(win, win->drag_select_id);
						if(inp) {
							char *display = inp->text;
							char masked_buf[MKGUI_MAX_TEXT];
							if(win->widgets[dsi].style & MKGUI_INPUT_PASSWORD) {
								uint32_t mlen = (uint32_t)strlen(inp->text);
								for(uint32_t mi = 0; mi < mlen && mi < MKGUI_MAX_TEXT - 1; ++mi) {
									masked_buf[mi] = '*';
								}
								masked_buf[mlen < MKGUI_MAX_TEXT - 1 ? mlen : MKGUI_MAX_TEXT - 1] = '\0';
								display = masked_buf;
							}
							inp->cursor = input_hit_cursor(win, inp, display, win->rects[dsi].x, win->mouse_x);
							inp->sel_end = inp->cursor;
							dirty_widget(win, (uint32_t)dsi);
							input_scroll_to_cursor(win, win->drag_select_id);
						}

					} else if(dsi >= 0 && win->widgets[dsi].type == MKGUI_TEXTAREA) {
						struct mkgui_textarea_data *ta = find_textarea_data(win, win->drag_select_id);
						if(ta) {
							int32_t ry = win->rects[dsi].y;
							int32_t rh = win->rects[dsi].h;
							if(win->mouse_y < ry + 1) {
								ta->scroll_y -= win->row_height;
								if(ta->scroll_y < 0) {
									ta->scroll_y = 0;
								}

							} else if(win->mouse_y > ry + rh - 1) {
								int32_t content_h = (int32_t)textarea_line_count(ta) * win->row_height;
								int32_t view_h = rh - 2;
								int32_t max_scroll = content_h - view_h;
								if(max_scroll > 0) {
									ta->scroll_y += win->row_height;
									if(ta->scroll_y > max_scroll) {
										ta->scroll_y = max_scroll;
									}
								}
							}
							ta->cursor = textarea_hit_pos(win, ta, win->rects[dsi].x, win->rects[dsi].y, win->rects[dsi].h, win->mouse_x, win->mouse_y);
							ta->sel_end = ta->cursor;
							dirty_widget(win, (uint32_t)dsi);
						}

					} else if(dsi >= 0 && win->widgets[dsi].type == MKGUI_LOGVIEW) {
						struct mkgui_logview_data *lv = find_logview_data(win, win->drag_select_id);
						if(lv) {
							int32_t ry = win->rects[dsi].y;
							int32_t rh = win->rects[dsi].h;
							int32_t view_h = rh - 2;
							int32_t content_h = logview_total_content_h(win, lv);
							if(win->mouse_y < ry + 1) {
								lv->scroll_y -= win->row_height;
								logview_clamp_scroll(lv, content_h, view_h);

							} else if(win->mouse_y > ry + rh - 1) {
								lv->scroll_y += win->row_height;
								logview_clamp_scroll(lv, content_h, view_h);
							}
							uint32_t vline, off;
							logview_hit_vline(win, lv, win->rects[dsi].x, win->rects[dsi].y, win->mouse_y, win->mouse_x, &vline, &off);
							uint64_t seq;
							uint32_t soff;
							logview_vline_to_pos(lv, vline, off, &seq, &soff);
							lv->sel_cursor_line_seq = seq;
							lv->sel_cursor_off = soff;
							lv->has_selection = (lv->sel_anchor_line_seq != lv->sel_cursor_line_seq || lv->sel_anchor_off != lv->sel_cursor_off);
							int32_t max_scroll = content_h - view_h;
							if(max_scroll < 0) {
								max_scroll = 0;
							}
							lv->stuck_to_end = (lv->scroll_y >= max_scroll) ? 1 : 0;
							dirty_widget(win, (uint32_t)dsi);
						}

					} else if(dsi >= 0 && win->widgets[dsi].type == MKGUI_PATHBAR) {
						struct mkgui_pathbar_data *pb = find_pathbar_data(win, win->drag_select_id);
						if(pb && pb->editing) {
							int32_t base_x = win->rects[dsi].x + sc(win, 4) - pb->edit_scroll_x;
							uint32_t hit_pos = textedit_hit_cursor(win, pb->edit_buf, base_x, win->mouse_x);
							pb->edit_cursor = hit_pos;
							pb->edit_sel_end = hit_pos;
							dirty_widget(win, (uint32_t)dsi);
							pathbar_scroll_to_cursor(win, pb);
						}

					} else if(dsi >= 0 && win->widgets[dsi].type == MKGUI_COMBOBOX) {
						struct mkgui_combobox_data *cb = find_combobox_data(win, win->drag_select_id);
						if(cb) {
							cb->cursor = combobox_hit_cursor(win, cb, win->rects[dsi].x, win->mouse_x);
							cb->sel_end = cb->cursor;
							dirty_widget(win, (uint32_t)dsi);
							combobox_scroll_to_cursor(win, win->drag_select_id);
						}
					}
					break;
				}

				if(win->drag_scrollbar_id) {
					int32_t dsi = find_widget_idx(win, win->drag_scrollbar_id);
					if(dsi >= 0 && win->widgets[dsi].type == MKGUI_TEXTAREA) {
						struct mkgui_textarea_data *ta = find_textarea_data(win, win->drag_scrollbar_id);
						if(ta) {
							textarea_scroll_drag(win, (uint32_t)dsi, ta, win->mouse_y, win->drag_scrollbar_offset);
						}
					} else if(dsi >= 0 && win->widgets[dsi].type == MKGUI_LOGVIEW) {
						struct mkgui_logview_data *lv = find_logview_data(win, win->drag_scrollbar_id);
						if(lv) {
							logview_sb_drag(win, (uint32_t)dsi, lv, win->mouse_y, win->drag_scrollbar_offset);
						}
					} else if(dsi >= 0 && win->widgets[dsi].type == MKGUI_SCROLLBAR) {
						scrollbar_drag_to(win, win->drag_scrollbar_id, win->mouse_x, win->mouse_y);
					} else if(dsi >= 0 && win->widgets[dsi].type == MKGUI_ITEMVIEW) {
						itemview_scroll_to_mouse(win, win->drag_scrollbar_id, win->mouse_x, win->mouse_y);
					} else if(dsi >= 0 && (win->widgets[dsi].type == MKGUI_VBOX || win->widgets[dsi].type == MKGUI_HBOX)) {
						struct mkgui_box_scroll *bs = find_box_scroll(win, win->drag_scrollbar_id);
						if(bs) {
							if(win->widgets[dsi].type == MKGUI_HBOX && bs->content_w > win->rects[dsi].w) {
								int32_t sw = win->rects[dsi].w;
								int32_t sx = win->rects[dsi].x;
								int32_t track = sw;
								int32_t total = bs->content_w;
								if(total < 1) {
									total = 1;
								}
								int32_t thumb = track * sw / total;
								if(thumb < sc(win, 20)) {
									thumb = sc(win, 20);
								}

								if(thumb > track) {
									thumb = track;
								}
								int32_t max_scroll = bs->content_w - sw;
								if(max_scroll < 1) {
									max_scroll = 1;
								}
								int32_t new_pos = win->mouse_x - sx - win->drag_scrollbar_offset;
								int32_t scrollable_range = track - thumb;
								if(scrollable_range < 1) {
									scrollable_range = 1;
								}
								bs->scroll_x = (int32_t)((int64_t)new_pos * max_scroll / scrollable_range);
								if(bs->scroll_x < 0) {
									bs->scroll_x = 0;
								}

								if(bs->scroll_x > max_scroll) {
									bs->scroll_x = max_scroll;
								}
							} else {
								int32_t sh = win->rects[dsi].h;
								int32_t sy = win->rects[dsi].y;
								int32_t track = sh;
								int32_t total = bs->content_h;
								if(total < 1) {
									total = 1;
								}
								int32_t thumb = track * sh / total;
								if(thumb < sc(win, 20)) {
									thumb = sc(win, 20);
								}

								if(thumb > track) {
									thumb = track;
								}
								int32_t max_scroll = bs->content_h - sh;
								if(max_scroll < 1) {
									max_scroll = 1;
								}
								int32_t new_pos = win->mouse_y - sy - win->drag_scrollbar_offset;
								int32_t scrollable_range = track - thumb;
								if(scrollable_range < 1) {
									scrollable_range = 1;
								}
								bs->scroll_y = (int32_t)((int64_t)new_pos * max_scroll / scrollable_range);
								if(bs->scroll_y < 0) {
									bs->scroll_y = 0;
								}

								if(bs->scroll_y > max_scroll) {
									bs->scroll_y = max_scroll;
								}
							}
							dirty_widget(win, (uint32_t)dsi);
						}
					} else if(dsi >= 0 && win->widgets[dsi].type == MKGUI_GRIDVIEW) {
						gridview_scroll_to_y(win, win->drag_scrollbar_id, win->mouse_y);
					} else if(dsi >= 0 && win->widgets[dsi].type == MKGUI_RICHLIST) {
						richlist_scroll_to_y(win, win->drag_scrollbar_id, win->mouse_y);
					} else if(win->drag_scrollbar_horiz) {
						listview_scroll_to_x(win, win->drag_scrollbar_id, win->mouse_x);
					} else {
						listview_scroll_to_y(win, win->drag_scrollbar_id, win->mouse_y);
					}
					break;
				}

				if(win->drag_col_resize_id) {
					int32_t nw = win->drag_col_resize_start_w + (win->mouse_x - win->drag_col_resize_start_x);
					if(nw < sc(win, 40)) {
						nw = sc(win, 40);
					}
					struct mkgui_listview_data *lv = find_listv_data(win, win->drag_col_resize_id);
					if(lv) {
						lv->columns[win->drag_col_resize_col].width = nw;
						dirty_widget_id(win, win->drag_col_resize_id);

					} else {
						struct mkgui_gridview_data *gv = find_gridv_data(win, win->drag_col_resize_id);
						if(gv) {
							gv->columns[win->drag_col_resize_col].width = nw;
							dirty_widget_id(win, win->drag_col_resize_id);
						}
					}
					break;
				}

				if(win->drag_col_id) {
					int32_t dx = win->mouse_x - win->drag_col_start_x;
					if(dx < -sc(win, 3) || dx > sc(win, 3)) {
						dirty_widget_id(win, win->drag_col_id);
					}
					int32_t lv_idx = find_widget_idx(win, win->drag_col_id);
					if(lv_idx >= 0) {
						struct mkgui_listview_data *lv = find_listv_data(win, win->drag_col_id);
						if(lv) {
							int32_t lx = win->rects[lv_idx].x;
							int32_t lw = win->rects[lv_idx].w;
							int32_t edge = sc(win, 30);
							int32_t scroll_step = win->row_height * 2;
							int32_t content_w = lw - 2 - win->scrollbar_w;
							if(win->mouse_x < lx + edge) {
								lv->scroll_x -= scroll_step;
								dirty_widget(win, (uint32_t)lv_idx);
							} else if(win->mouse_x > lx + lw - edge) {
								lv->scroll_x += scroll_step;
								dirty_widget(win, (uint32_t)lv_idx);
							}
							listview_clamp_scroll_x(lv, content_w);
						}
					}
					break;
				}

				if(win->press_id) {
					struct mkgui_widget *pw = find_widget(win, win->press_id);

					if(pw && (pw->type == MKGUI_HSPLIT || pw->type == MKGUI_VSPLIT)) {
						int32_t pi = find_widget_idx(win, pw->id);
						if(pi >= 0) {
							struct mkgui_split_data *sd = find_split_data(win, pw->id);
							if(sd) {
								if(pw->type == MKGUI_HSPLIT) {
									int32_t rh = win->rects[pi].h;
									if(rh > 0) {
										sd->ratio = (float)(win->mouse_y - win->rects[pi].y) / (float)rh;
										float min_ratio = (float)win->split_min_px / (float)rh;
										float max_ratio = 1.0f - (float)(win->split_min_px + win->split_thick) / (float)rh;
										if(min_ratio > 0.45f) {
											min_ratio = 0.45f;
										}

										if(max_ratio < 0.55f) {
											max_ratio = 0.55f;
										}

										if(sd->ratio < min_ratio) {
											sd->ratio = min_ratio;
										}

										if(sd->ratio > max_ratio) {
											sd->ratio = max_ratio;
										}
									}

								} else {
									int32_t rw = win->rects[pi].w;
									if(rw > 0) {
										sd->ratio = (float)(win->mouse_x - win->rects[pi].x) / (float)rw;
										float min_ratio = (float)win->split_min_px / (float)rw;
										float max_ratio = 1.0f - (float)(win->split_min_px + win->split_thick) / (float)rw;
										if(min_ratio > 0.45f) {
											min_ratio = 0.45f;
										}

										if(max_ratio < 0.55f) {
											max_ratio = 0.55f;
										}

										if(sd->ratio < min_ratio) {
											sd->ratio = min_ratio;
										}

										if(sd->ratio > max_ratio) {
											sd->ratio = max_ratio;
										}
									}
								}
								dirty_widget(win, (uint32_t)pi);
							}
						}
					}

					if(pw && pw->type == MKGUI_SLIDER) {
						struct mkgui_slider_data *sd = find_slider_data(win, pw->id);
						int32_t pi = find_widget_idx(win, pw->id);
						if(sd && pi >= 0) {
							int32_t range = sd->max_val - sd->min_val;
							int32_t thumb_sz = sc(win, 10);
							int32_t new_val;
							if(pw->flags & MKGUI_VERTICAL) {
								int32_t ry = win->rects[pi].y;
								int32_t rh = win->rects[pi].h;
								int32_t track = rh - thumb_sz;
								int32_t pos = (win->mouse_y - win->drag_slider_offset) - ry - thumb_sz / 2;
								new_val = sd->max_val - (track > 0 ? (int32_t)((int64_t)pos * range / track) : 0);
							} else {
								int32_t rx = win->rects[pi].x;
								int32_t rw = win->rects[pi].w;
								int32_t track = rw - thumb_sz;
								int32_t pos = (win->mouse_x - win->drag_slider_offset) - rx - thumb_sz / 2;
								new_val = sd->min_val + (track > 0 ? (int32_t)((int64_t)pos * range / track) : 0);
							}

							if(new_val < sd->min_val) {
								new_val = sd->min_val;
							}

							if(new_val > sd->max_val) {
								new_val = sd->max_val;
							}

							if(new_val != sd->value) {
								sd->value = new_val;
								dirty_widget(win, (uint32_t)pi);
								ev->type = MKGUI_EVENT_SLIDER_CHANGED;
								ev->id = pw->id;
								ev->value = new_val;
								return 1;
							}
						}
					}
				}

				int32_t hi = hit_test(win, win->mouse_x, win->mouse_y);
				uint32_t new_hover = (hi >= 0) ? win->widgets[hi].id : 0;
				if(new_hover != win->hover_id) {
					if(win->hover_id) {
						dirty_widget_id(win, win->hover_id);
					}
					win->hover_id = new_hover;
					if(win->hover_id) {
						dirty_widget_id(win, win->hover_id);
					}
				}

				if(hi >= 0 && win->widgets[hi].type == MKGUI_TABS) {
					struct mkgui_tabs_data *td = find_tabs_data(win, win->widgets[hi].id);
					if(td) {
						uint32_t ht = tab_hit_test(win, (uint32_t)hi, win->mouse_x, win->mouse_y);
						if(ht != td->hover_tab) {
							td->hover_tab = ht;
							dirty_widget(win, (uint32_t)hi);
						}
					}
				}

				if(hi >= 0 && win->widgets[hi].type == MKGUI_PATHBAR) {
					struct mkgui_pathbar_data *pb = find_pathbar_data(win, win->widgets[hi].id);
					if(pb && !pb->editing) {
						int32_t seg = pathbar_segment_hit(win, pb, win->rects[hi].x, win->mouse_x);
						if(seg != pb->hover_seg) {
							pb->hover_seg = seg;
							dirty_widget(win, (uint32_t)hi);
						}
					}
				}
				for(uint32_t tvi = 0; tvi < win->treeview_count; ++tvi) {
					struct mkgui_treeview_data *tv = &win->treeviews[tvi];
					if(tv->drag_source >= 0 && win->press_id == tv->widget_id) {
						int32_t dy = win->mouse_y - tv->drag_start_y;
						if(dy < 0) {
							dy = -dy;
						}

						if(!tv->drag_active && dy > sc(win, 4)) {
							tv->drag_active = 1;
							ev->type = MKGUI_EVENT_DRAG_START;
							ev->id = tv->widget_id;
							ev->value = (int32_t)tv->nodes[tv->drag_source].id;
							dirty_widget_id(win, tv->widget_id);
							return 1;
						}

						if(tv->drag_active) {
							int32_t tidx = find_widget_idx(win, tv->widget_id);
							if(tidx >= 0) {
								int32_t node_idx = treeview_row_hit(win, (uint32_t)tidx, win->mouse_y);
								if(node_idx >= 0 && node_idx != tv->drag_source) {
									int32_t ry = win->rects[tidx].y;
									int32_t local_y = win->mouse_y - ry - 1 + tv->scroll_y;
									int32_t row_local = local_y % win->row_height;
									int32_t third = win->row_height / 3;
									if(row_local < third) {
										tv->drag_pos = 0;
									} else if(row_local > win->row_height - third) {
										tv->drag_pos = 1;
									} else {
										tv->drag_pos = 2;
									}
									tv->drag_target = node_idx;
								} else {
									tv->drag_target = -1;
								}
								dirty_widget(win, (uint32_t)tidx);
							}
						}
					}
				}
				for(uint32_t lvi = 0; lvi < win->listv_count; ++lvi) {
					struct mkgui_listview_data *lv = &win->listvs[lvi];
					if(lv->drag_source >= 0 && win->press_id == lv->widget_id) {
						int32_t dy = win->mouse_y - lv->drag_start_y;
						if(dy < 0) {
							dy = -dy;
						}

						if(!lv->drag_active && dy > sc(win, 4)) {
							lv->drag_active = 1;
							ev->type = MKGUI_EVENT_DRAG_START;
							ev->id = lv->widget_id;
							ev->value = lv->drag_source;
							dirty_widget_id(win, lv->widget_id);
							return 1;
						}

						if(lv->drag_active) {
							int32_t lidx = find_widget_idx(win, lv->widget_id);
							if(lidx >= 0) {
								int32_t tgt = listview_row_hit(win, (uint32_t)lidx, win->mouse_x, win->mouse_y);
								if(tgt >= 0 && tgt != lv->drag_source) {
									lv->drag_target = tgt;
								} else {
									lv->drag_target = -1;
								}
								dirty_widget(win, (uint32_t)lidx);
							}
						}
					}
				}
				for(uint32_t gvi = 0; gvi < win->gridview_count; ++gvi) {
					struct mkgui_gridview_data *gv = &win->gridviews[gvi];
					if(gv->drag_source >= 0 && win->press_id == gv->widget_id) {
						int32_t dy = win->mouse_y - gv->drag_start_y;
						if(dy < 0) {
							dy = -dy;
						}

						if(!gv->drag_active && dy > sc(win, 4)) {
							gv->drag_active = 1;
							ev->type = MKGUI_EVENT_DRAG_START;
							ev->id = gv->widget_id;
							ev->value = gv->drag_source;
							dirty_widget_id(win, gv->widget_id);
							return 1;
						}

						if(gv->drag_active) {
							int32_t gidx = find_widget_idx(win, gv->widget_id);
							if(gidx >= 0) {
								int32_t tgt = gridview_row_hit(win, (uint32_t)gidx, win->mouse_y);
								if(tgt >= 0 && tgt != gv->drag_source) {
									gv->drag_target = tgt;
								} else {
									gv->drag_target = -1;
								}
								dirty_widget(win, (uint32_t)gidx);
							}
						}
					}
				}

				if(hi >= 0 && win->widgets[hi].type == MKGUI_SPINBOX) {
					struct mkgui_spinbox_data *sd = find_spinbox_data(win, win->widgets[hi].id);
					if(sd) {
						int32_t hb = spinbox_btn_hit(win, (uint32_t)hi, win->mouse_x, win->mouse_y);
						if(hb != sd->hover_btn) {
							sd->hover_btn = hb;
							dirty_widget(win, (uint32_t)hi);
						}
					}
				}

				uint32_t cursor_type = MKGUI_CURSOR_DEFAULT;
				if(win->press_id) {
					struct mkgui_widget *pw = find_widget(win, win->press_id);
					if(pw) {
						if(pw->type == MKGUI_HSPLIT) {
							cursor_type = MKGUI_CURSOR_V_RESIZE;
						} else if(pw->type == MKGUI_VSPLIT) {
							cursor_type = MKGUI_CURSOR_H_RESIZE;
						}
					}
				}

				if(cursor_type == MKGUI_CURSOR_DEFAULT && hi >= 0) {
					uint32_t htype = win->widgets[hi].type;
					if(htype == MKGUI_HSPLIT) {
						cursor_type = MKGUI_CURSOR_V_RESIZE;
					} else if(htype == MKGUI_VSPLIT) {
						cursor_type = MKGUI_CURSOR_H_RESIZE;
					} else if(htype == MKGUI_LISTVIEW) {
						if(listview_divider_hit(win, (uint32_t)hi, win->mouse_x, win->mouse_y) >= 0) {
							cursor_type = MKGUI_CURSOR_H_RESIZE;
						}
					} else if(htype == MKGUI_GRIDVIEW) {
						if(gridview_divider_hit(win, (uint32_t)hi, win->mouse_x, win->mouse_y) >= 0) {
							cursor_type = MKGUI_CURSOR_H_RESIZE;
						}
					}
				}
				platform_set_cursor(win, cursor_type);

				if(win->popup_count > 0 && hi >= 0 && win->widgets[hi].type == MKGUI_MENUITEM) {
					struct mkgui_widget *mw = &win->widgets[hi];
					struct mkgui_widget *mp = find_widget(win, mw->parent_id);
					if(mp && mp->type == MKGUI_MENU && win->popups[0].widget_id != mw->id) {
						menu_open_popup(win, mw->id);
					}
				}

				tooltip_update(win, new_hover, win->mouse_x, win->mouse_y);
			} break;

			// -=[ BUTTON PRESS ]=-
			case MKGUI_PLAT_BUTTON_PRESS: {
				// Toast and banner clicks dismiss the notification without
				// reaching the underlying widget. Only left-click.
				if(pev.button == 1 && popup_idx < 0 && notify_handle_click(win, win->mouse_x, win->mouse_y)) {
					break;
				}

				if(win->canvas_window && popup_idx < 0) {
					for(uint32_t ci = 0; ci < win->canvas_count; ++ci) {
						ev->type = MKGUI_EVENT_CANVAS_PRESS;
						ev->id = win->canvases[ci].widget_id;
						ev->value = pev.x;
						ev->col = pev.y;
						ev->keysym = pev.button;
						ev->keymod = pev.keymod;
						return 1;
					}
					break;
				}

				if(win->cell_edit.active && pev.button == 1) {
					struct mkgui_cell_edit *ce = &win->cell_edit;
					if(!celledit_compute_rect(win) || win->mouse_x < ce->cell_x || win->mouse_x >= ce->cell_x + ce->cell_w || win->mouse_y < ce->cell_y || win->mouse_y >= ce->cell_y + ce->cell_h) {
						ce->dragging = 0;
						celledit_commit(win, ev);
						if(ev->type != MKGUI_EVENT_NONE) {
							return 1;
						}
					} else {
						int32_t base_x = ce->cell_x + sc(win, 4) - ce->te.scroll_x;
						uint32_t hit_pos = textedit_hit_cursor(win, ce->text, base_x, win->mouse_x);
						ce->te.cursor = hit_pos;
						ce->te.sel_start = hit_pos;
						ce->te.sel_end = hit_pos;
						ce->dragging = 1;
						dirty_widget_id(win, ce->widget_id);
						break;
					}
				}

				if(popup_idx >= 0) {
					struct mkgui_popup *p = &win->popups[popup_idx];
					struct mkgui_widget *pw = find_widget(win, p->widget_id);

					if(p->widget_id == MKGUI_CTXMENU_POPUP_ID && pev.button == 1) {
						int32_t hit = ctxmenu_hit_item(win, pev.y);
						if(hit >= 0) {
							struct mkgui_ctxmenu_item *it = ctxmenu_item_at(win, hit);
							if(it && !(it->flags & MKGUI_DISABLED)) {
								if(it->flags & MKGUI_MENUITEM_CHECK) {
									it->flags ^= MKGUI_MENUITEM_CHECKED;
								} else if(it->flags & MKGUI_MENUITEM_RADIO) {
									for(uint32_t ri = 0; ri < win->ctxmenu_count; ++ri) {
										win->ctxmenu_items[ri].flags &= ~MKGUI_MENUITEM_CHECKED;
									}
									it->flags |= MKGUI_MENUITEM_CHECKED;
								}
								ev->type = MKGUI_EVENT_CONTEXT_MENU;
								ev->id = it->id;
								ev->value = (it->flags & MKGUI_MENUITEM_CHECKED) ? 1 : 0;
								popup_destroy_all(win);
								dirty_all(win);
								return 1;
							}
						}
						// break (not return): dismiss the menu, then let the click
						// fall through to the main window dispatcher below
						popup_destroy_all(win);
						dirty_all(win);
						break;
					}

					if((pev.button == 4 || pev.button == 5) && pw && pw->type == MKGUI_DATEPICKER) {
						struct mkgui_datepicker_data *dp = find_datepicker_data(win, pw->id);
						if(dp) {
							int32_t hit = datepicker_cal_hit(win, dp, pev.x, pev.y);
							if(hit == -5 || hit == -6 || hit == -7) {
								dp->view_year += (pev.button == 4) ? 1 : -1;
							} else {
								if(pev.button == 4) {
									--dp->view_month;
									if(dp->view_month < 1) {
										dp->view_month = 12; --dp->view_year;
									}
								} else {
									++dp->view_month;
									if(dp->view_month > 12) {
										dp->view_month = 1; ++dp->view_year;
									}
								}
							}
							p->dirty = 1;
							dirty_all(win);
						}
						break;
					}

					if((pev.button == 4 || pev.button == 5) && pw && pw->type == MKGUI_DROPDOWN) {
						struct mkgui_dropdown_data *dd = find_dropdown_data(win, pw->id);
						if(dd) {
							int32_t delta = (pev.button == 4) ? -win->row_height * 3 : win->row_height * 3;
							dd->scroll_y += delta;
							dropdown_clamp_scroll(win, dd, p->h);
							p->hover_item = (pev.y - 1 + dd->scroll_y) / win->row_height;
							p->dirty = 1;
						}
						break;
					}

					if(pw && pw->type == MKGUI_DROPDOWN) {
						int32_t scroll_off = 0;
						struct mkgui_dropdown_data *sdd = find_dropdown_data(win, pw->id);
						if(sdd) {
							scroll_off = sdd->scroll_y;
						}
						int32_t item = (pev.y - 1 + scroll_off) / win->row_height;
						struct mkgui_dropdown_data *dd = find_dropdown_data(win, pw->id);
						if(dd && item >= 0 && item < (int32_t)dd->item_count) {
							dd->selected = item;
							ev->type = MKGUI_EVENT_DROPDOWN_CHANGED;
							ev->id = pw->id;
							ev->value = item;
							popup_destroy(win, (uint32_t)popup_idx);
							dirty_all(win);
							return 1;
						}

					} else if(pw && pw->type == MKGUI_COMBOBOX) {
						struct mkgui_combobox_data *cb = find_combobox_data(win, pw->id);
						if(cb) {
							int32_t item = (pev.y - 1 + cb->scroll_y) / win->row_height;
							if(item >= 0 && item < (int32_t)cb->filter_count) {
								uint32_t real_idx = cb->filter_map[item];
								cb->selected = (int32_t)real_idx;
								{ size_t _l = strlen(cb->items[real_idx]); if(_l >= MKGUI_MAX_TEXT) { _l = MKGUI_MAX_TEXT - 1; } memcpy(cb->text, cb->items[real_idx], _l); cb->text[_l] = '\0'; }
								cb->cursor = (uint32_t)strlen(cb->text);
								combobox_clear_selection(cb);
								combobox_close_popup(win, cb);
								ev->type = MKGUI_EVENT_COMBOBOX_CHANGED;
								ev->id = pw->id;
								ev->value = cb->selected;
								dirty_all(win);
								return 1;
							}
						}

					} else if(pw && pw->type == MKGUI_DATEPICKER) {
						struct mkgui_datepicker_data *dp = find_datepicker_data(win, pw->id);
						if(dp) {
							int32_t hit = datepicker_cal_hit(win, dp, pev.x, pev.y);
							if(hit == -2) {
								--dp->view_month;
								if(dp->view_month < 1) {
									dp->view_month = 12;
									--dp->view_year;
								}
								p->dirty = 1;
								dirty_all(win);
								break;
							} else if(hit == -3) {
								++dp->view_month;
								if(dp->view_month > 12) {
									dp->view_month = 1;
									++dp->view_year;
								}
								p->dirty = 1;
								dirty_all(win);
								break;
							} else if(hit == -4) {
								dp->month_select = dp->month_select ? 0 : 1;
								dp->cal_hover = -1;
								p->dirty = 1;
								dirty_all(win);
								break;
							} else if(hit == -5) {
								++dp->view_year;
								p->dirty = 1;
								dirty_all(win);
								break;
							} else if(hit == -6) {
								--dp->view_year;
								p->dirty = 1;
								dirty_all(win);
								break;
							} else if(hit >= 100 && hit < 112) {
								dp->view_month = hit - 100 + 1;
								dp->month_select = 0;
								dp->cal_hover = -1;
								p->dirty = 1;
								dirty_all(win);
								break;
							} else if(hit == -7 || hit == -1) {
								break;
							} else if(hit > 0) {
								dp->day = hit;
								dp->month = dp->view_month;
								dp->year = dp->view_year;
								dp->popup_open = 0;
								dp->editing = 0;
								popup_destroy(win, (uint32_t)popup_idx);
								ev->type = MKGUI_EVENT_DATEPICKER_CHANGED;
								ev->id = pw->id;
								dirty_all(win);
								return 1;
							}
						}

					} else if(pw && pw->type == MKGUI_MENUITEM) {
						int32_t hit_idx;
						struct mkgui_widget *clicked = menu_popup_hit_item(win, pw->id, pev.y, &hit_idx);
						if(clicked && !(clicked->style & MKGUI_MENUITEM_SEPARATOR)) {
							if(menu_item_has_children(win, clicked->id)) {
								menu_open_submenu(win, (uint32_t)popup_idx, clicked->id);

							} else {
								if(clicked->style & MKGUI_MENUITEM_CHECK) {
									clicked->style ^= MKGUI_MENUITEM_CHECKED;

								} else if(clicked->style & MKGUI_MENUITEM_RADIO) {
									for(uint32_t ri = 0; ri < win->widget_count; ++ri) {
										struct mkgui_widget *rw = &win->widgets[ri];
										if(rw->type == MKGUI_MENUITEM && rw->parent_id == clicked->parent_id && (rw->style & MKGUI_MENUITEM_RADIO)) {
											rw->style &= ~MKGUI_MENUITEM_CHECKED;
										}
									}
									clicked->style |= MKGUI_MENUITEM_CHECKED;
								}
								ev->type = MKGUI_EVENT_MENU;
								ev->id = clicked->id;
								ev->value = (clicked->style & MKGUI_MENUITEM_CHECKED) ? 1 : 0;
								popup_destroy_all(win);
								dirty_all(win);
								return 1;
							}
						}
					}

					popup_destroy(win, (uint32_t)popup_idx);
					dirty_all(win);
					break;
				}

				uint32_t closed_dropdown_id = 0;
				uint32_t closed_combobox_id = 0;
				if(win->popup_count > 0) {
					for(uint32_t pi = 0; pi < win->popup_count; ++pi) {
						struct mkgui_widget *pw = find_widget(win, win->popups[pi].widget_id);
						if(pw && pw->type == MKGUI_DROPDOWN) {
							closed_dropdown_id = win->popups[pi].widget_id;
						}

						if(pw && pw->type == MKGUI_COMBOBOX) {
							closed_combobox_id = win->popups[pi].widget_id;
							struct mkgui_combobox_data *cb = find_combobox_data(win, pw->id);
							if(cb) {
								cb->popup_open = 0;
							}
						}
					}
					popup_destroy_all(win);
					dirty_all(win);
				}

				win->mouse_x = pev.x;
				win->mouse_y = pev.y;
				win->mouse_btn = pev.button;

				if(pev.button == 3) {
					int32_t hi = hit_test(win, win->mouse_x, win->mouse_y);
					if(hi >= 0) {
						struct mkgui_widget *hw = &win->widgets[hi];
						win->ctxmenu_x = win->mouse_x;
						win->ctxmenu_y = win->mouse_y;

						if(hw->type == MKGUI_LISTVIEW) {
							struct mkgui_listview_data *lv = find_listv_data(win, hw->id);
							if(lv) {
								int32_t hdr = listview_header_hit(win, (uint32_t)hi, win->mouse_x, win->mouse_y);
								if(hdr >= 0) {
									ev->type = MKGUI_EVENT_CONTEXT_HEADER;
									ev->id = hw->id;
									ev->col = (int32_t)lv->col_order[hdr];
									ev->value = win->mouse_x;
									return 1;
								}
								int32_t hh = lv->header_height > 0 ? lv->header_height : win->row_height;
								int32_t body_y = win->rects[hi].y + hh + 1;
								int32_t row = (win->mouse_y - body_y + lv->scroll_y) / win->row_height;
								int32_t col = -1;
								int32_t cx = win->rects[hi].x - lv->scroll_x;
								for(uint32_t d = 0; d < lv->col_count; ++d) {
									uint32_t c = lv->col_order[d];
									if(win->mouse_x >= cx && win->mouse_x < cx + lv->columns[c].width) {
										col = (int32_t)c;
										break;
									}
									cx += lv->columns[c].width;
								}

								if(row < 0 || row >= (int32_t)lv->row_count) {
									row = -1;
								} else {
									if(hw->style & MKGUI_LISTVIEW_MULTI_SELECT) {
										if(lv_multi_sel_find(lv, row) < 0) {
											lv->multi_sel_count = 0;
											lv->multi_sel[0] = row;
											lv->multi_sel_count = 1;
											lv->selected_row = row;
											dirty_widget(win, (uint32_t)hi);
										}
									} else {
										if(lv->selected_row != row) {
											lv->selected_row = row;
											dirty_widget(win, (uint32_t)hi);
										}
									}
								}
								ev->type = MKGUI_EVENT_CONTEXT;
								ev->id = hw->id;
								ev->value = row;
								ev->col = col;
								return 1;
							}

						} else if(hw->type == MKGUI_GRIDVIEW) {
							struct mkgui_gridview_data *gv = find_gridv_data(win, hw->id);
							if(gv) {
								int32_t ghh = gv->header_height > 0 ? gv->header_height : win->row_height;
								if(win->mouse_y < win->rects[hi].y + ghh) {
									int32_t col = gridview_col_at_x(gv, win->mouse_x, win->rects[hi].x + 1);
									ev->type = MKGUI_EVENT_CONTEXT_HEADER;
									ev->id = hw->id;
									ev->col = col;
									ev->value = win->mouse_x;
									return 1;
								}
								int32_t body_y = win->rects[hi].y + ghh + 1;
								int32_t row = (win->mouse_y - body_y + gv->scroll_y) / win->row_height;
								int32_t col = gridview_col_at_x(gv, win->mouse_x, win->rects[hi].x + 1);
								if(row < 0 || row >= (int32_t)gv->row_count) {
									row = -1;
								} else if(gv->selected_row != row) {
									gv->selected_row = row;
									gv->selected_col = col;
									dirty_widget(win, (uint32_t)hi);
								}
								ev->type = MKGUI_EVENT_CONTEXT;
								ev->id = hw->id;
								ev->value = row;
								ev->col = col;
								return 1;
							}

						} else if(hw->type == MKGUI_RICHLIST) {
							struct mkgui_richlist_data *rl = find_richlist_data(win, hw->id);
							if(rl) {
								int32_t row = richlist_row_hit(win, (uint32_t)hi, win->mouse_y);
								if(row >= 0 && rl->selected_row != row) {
									rl->selected_row = row;
									dirty_widget(win, (uint32_t)hi);
								}
								ev->type = MKGUI_EVENT_CONTEXT;
								ev->id = hw->id;
								ev->value = row;
								return 1;
							}

						} else if(hw->type == MKGUI_TREEVIEW) {
							struct mkgui_treeview_data *tv = find_treeview_data(win, hw->id);
							if(tv) {
								int32_t body_y = win->rects[hi].y + 1;
								tv_idx_build(tv);
								int32_t vis_row = (win->mouse_y - body_y + tv->scroll_y) / win->row_height;
								int32_t node_id = -1;
								int32_t vis = 0;
								for(uint32_t n = 0; n < tv->node_count; ++n) {
									if(!treeview_node_visible(tv, n)) {
										continue;
									}

									if(vis == vis_row) {
										node_id = (int32_t)tv->nodes[n].id;
										break;
									}
									++vis;
								}

								if(node_id >= 0 && tv->selected_node != node_id) {
									tv->selected_node = node_id;
									dirty_widget(win, (uint32_t)hi);
								}
								ev->type = MKGUI_EVENT_CONTEXT;
								ev->id = hw->id;
								ev->value = node_id;
								ev->col = 0;
								return 1;
							}

						} else if(hw->type == MKGUI_ITEMVIEW) {
							struct mkgui_itemview_data *iv = find_itemview_data(win, hw->id);
							if(iv) {
								int32_t item = itemview_hit_item(win, (uint32_t)hi, iv, win->mouse_x, win->mouse_y);
								if(item >= 0 && iv->selected != item) {
									iv->selected = item;
									dirty_widget(win, (uint32_t)hi);
								}
								ev->type = MKGUI_EVENT_CONTEXT;
								ev->id = hw->id;
								ev->value = item;
								ev->col = 0;
								return 1;
							}
						}

						ev->type = MKGUI_EVENT_CONTEXT;
						ev->id = hw->id;
						ev->value = win->mouse_x;
						ev->col = win->mouse_y;
						return 1;
					}
					break;
				}

				if(pev.button == 4 || pev.button == 5) {
					if(win->cell_edit.active) {
						celledit_cancel(win);
					}
					int32_t hi = hit_test(win, win->mouse_x, win->mouse_y);
					int32_t delta = (pev.button == 4) ? -win->row_height * 3 : win->row_height * 3;
					if(hi >= 0 && win->widgets[hi].type == MKGUI_LISTVIEW) {
						struct mkgui_listview_data *lv = find_listv_data(win, win->widgets[hi].id);
						if(lv) {
							if((pev.keymod & MKGUI_MOD_SHIFT) || listview_hscrollbar_hit(win, (uint32_t)hi, win->mouse_x, win->mouse_y)) {
								lv->scroll_x += delta;
								int32_t content_w = win->rects[hi].w - 2 - win->scrollbar_w;
								listview_clamp_scroll_x(lv, content_w);
							} else {
								lv->scroll_y += delta;
								int32_t content_h = win->rects[hi].h - win->row_height - 2;
								int32_t content_w = win->rects[hi].w - 2 - win->scrollbar_w;
								if(listview_total_col_w(lv) > content_w) {
									content_h -= win->scrollbar_w;
								}
								int32_t max_scroll = (int32_t)lv->row_count * win->row_height - content_h;
								if(max_scroll < 0) {
									max_scroll = 0;
								}

								if(lv->scroll_y < 0) {
									lv->scroll_y = 0;
								}

								if(lv->scroll_y > max_scroll) {
									lv->scroll_y = max_scroll;
								}
							}
							dirty_widget(win, (uint32_t)hi);
						}

					} else if(hi >= 0 && win->widgets[hi].type == MKGUI_GRIDVIEW) {
						struct mkgui_gridview_data *gv = find_gridv_data(win, win->widgets[hi].id);
						if(gv) {
							gv->scroll_y += delta;
							int32_t hh = gv->header_height > 0 ? gv->header_height : win->row_height;
							int32_t content_h = win->rects[hi].h - hh - 2;
							gridview_clamp_scroll(win, gv, content_h);
							dirty_widget(win, (uint32_t)hi);
						}

					} else if(hi >= 0 && win->widgets[hi].type == MKGUI_RICHLIST) {
						struct mkgui_richlist_data *rl = find_richlist_data(win, win->widgets[hi].id);
						if(rl) {
							rl->scroll_y += delta;
							richlist_clamp_scroll(rl, win->rects[hi].h - 2);
							dirty_widget(win, (uint32_t)hi);
						}

					} else if(hi >= 0 && win->widgets[hi].type == MKGUI_TREEVIEW) {
						struct mkgui_treeview_data *tv = find_treeview_data(win, win->widgets[hi].id);
						if(tv) {
							tv->scroll_y += delta;
							treeview_clamp_scroll(win, win->widgets[hi].id);
							dirty_widget(win, (uint32_t)hi);
						}

					} else if(hi >= 0 && win->widgets[hi].type == MKGUI_SPINBOX) {
						struct mkgui_spinbox_data *sd = find_spinbox_data(win, win->widgets[hi].id);
						if(sd) {
							if(sd->editing) {
								sd->editing = 0;
							}
							int32_t step = (pev.button == 4) ? sd->step : -sd->step;
							if(spinbox_adjust(win, ev, win->widgets[hi].id, step)) {
								return 1;
							}
						}

					} else if(hi >= 0 && win->widgets[hi].type == MKGUI_DROPDOWN) {
						struct mkgui_dropdown_data *dd = find_dropdown_data(win, win->widgets[hi].id);
						if(dd && dd->item_count > 0) {
							int32_t sel = dd->selected + (pev.button == 4 ? -1 : 1);
							if(sel < 0) {
								sel = 0;
							}

							if(sel >= (int32_t)dd->item_count) {
								sel = (int32_t)dd->item_count - 1;
							}

							if(sel != dd->selected) {
								dd->selected = sel;
								dirty_widget(win, (uint32_t)hi);
								ev->type = MKGUI_EVENT_DROPDOWN_CHANGED;
								ev->id = win->widgets[hi].id;
								ev->value = sel;
								return 1;
							}
						}

					} else if(hi >= 0 && win->widgets[hi].type == MKGUI_SLIDER) {
						struct mkgui_slider_data *sd = find_slider_data(win, win->widgets[hi].id);
						if(sd) {
							int32_t range = sd->max_val - sd->min_val;
							int32_t step = range / 20;
							if(step < 1) {
								step = 1;
							}
							uint32_t vert = (win->widgets[hi].flags & MKGUI_VERTICAL);
							sd->value += (pev.button == 4) ? (vert ? step : -step) : (vert ? -step : step);
							if(sd->value < sd->min_val) {
								sd->value = sd->min_val;
							}

							if(sd->value > sd->max_val) {
								sd->value = sd->max_val;
							}
							dirty_widget(win, (uint32_t)hi);
							ev->type = MKGUI_EVENT_SLIDER_CHANGED;
							ev->id = win->widgets[hi].id;
							ev->value = sd->value;
							return 1;
						}

					} else if(hi >= 0 && win->widgets[hi].type == MKGUI_TEXTAREA) {
						struct mkgui_textarea_data *ta = find_textarea_data(win, win->widgets[hi].id);
						if(ta) {
							ta->scroll_y += delta;
							int32_t content_h = (int32_t)textarea_line_count(ta) * win->row_height;
							int32_t view_h = win->rects[hi].h - 2;
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
							dirty_widget(win, (uint32_t)hi);
						}

					} else if(hi >= 0 && win->widgets[hi].type == MKGUI_LOGVIEW) {
						struct mkgui_logview_data *lv = find_logview_data(win, win->widgets[hi].id);
						if(lv) {
							lv->scroll_y += delta;
							int32_t content_h = logview_total_content_h(win, lv);
							int32_t view_h = win->rects[hi].h - 2;
							int32_t max_scroll = content_h - view_h;
							if(max_scroll < 0) {
								max_scroll = 0;
							}
							lv->stuck_to_end = (lv->scroll_y >= max_scroll) ? 1 : 0;
							logview_recompute(win, lv, hi);
							dirty_widget(win, (uint32_t)hi);
						}

					} else if(hi >= 0 && win->widgets[hi].type == MKGUI_ITEMVIEW) {
						struct mkgui_itemview_data *iv = find_itemview_data(win, win->widgets[hi].id);
						if(iv) {
							iv->scroll_y += delta;
							int32_t ca_x, ca_y, ca_w, ca_h;
							itemview_content_area(win, iv, win->rects[hi].x, win->rects[hi].y, win->rects[hi].w, win->rects[hi].h, &ca_x, &ca_y, &ca_w, &ca_h);
							itemview_clamp_scroll(win, iv, ca_w, ca_h);
							dirty_widget(win, (uint32_t)hi);
						}

					} else if(hi >= 0 && win->widgets[hi].type == MKGUI_SCROLLBAR) {
						scrollbar_scroll(win, win->widgets[hi].id, (pev.button == 4) ? -1 : 1);
						ev->type = MKGUI_EVENT_SCROLL;
						ev->id = win->widgets[hi].id;
						struct mkgui_scrollbar_data *sbd = find_scrollbar_data(win, win->widgets[hi].id);
						ev->value = sbd ? sbd->value : 0;
						return 1;

					} else if(hi >= 0 && win->widgets[hi].type == MKGUI_CANVAS) {
						ev->type = MKGUI_EVENT_SCROLL;
						ev->id = win->widgets[hi].id;
						ev->value = delta;
						ev->col = 0;
						return 1;

					} else {
						uint32_t sid = hi >= 0 ? win->widgets[hi].parent_id : 0;
						if(!sid) {
							for(int32_t k = (int32_t)win->widget_count - 1; k >= 0; --k) {
								if((win->widgets[k].type == MKGUI_VBOX || win->widgets[k].type == MKGUI_HBOX) && (win->widgets[k].flags & MKGUI_SCROLL)) {
									struct mkgui_box_scroll *bsk = find_box_scroll(win, win->widgets[k].id);
									if(bsk && (bsk->content_h > win->rects[k].h || bsk->content_w > win->rects[k].w)) {
										int32_t rx = win->rects[k].x;
										int32_t ry = win->rects[k].y;
										if(win->mouse_x >= rx && win->mouse_x < rx + win->rects[k].w && win->mouse_y >= ry && win->mouse_y < ry + win->rects[k].h) {
											sid = win->widgets[k].id;
											break;
										}
									}
								}
							}
						}
						while(sid) {
							int32_t si = find_widget_idx(win, sid);
							if(si < 0) {
								break;
							}

							if((win->widgets[si].type == MKGUI_VBOX || win->widgets[si].type == MKGUI_HBOX) && (win->widgets[si].flags & MKGUI_SCROLL)) {
								struct mkgui_box_scroll *bs = find_box_scroll(win, sid);
								if(bs) {
									if(win->widgets[si].type == MKGUI_HBOX && bs->content_w > win->rects[si].w) {
										int32_t view_w = win->rects[si].w;
										int32_t max_scroll = bs->content_w - view_w;
										if(max_scroll < 0) {
											max_scroll = 0;
										}
										bs->scroll_x += delta;
										if(bs->scroll_x < 0) {
											bs->scroll_x = 0;
										}

										if(bs->scroll_x > max_scroll) {
											bs->scroll_x = max_scroll;
										}
										dirty_widget(win, (uint32_t)si);
										break;
									}

									if(bs->content_h > win->rects[si].h) {
										int32_t view_h = win->rects[si].h;
										int32_t max_scroll = bs->content_h - view_h;
										if(max_scroll < 0) {
											max_scroll = 0;
										}
										bs->scroll_y += delta;
										if(bs->scroll_y < 0) {
											bs->scroll_y = 0;
										}

										if(bs->scroll_y > max_scroll) {
											bs->scroll_y = max_scroll;
										}
										dirty_widget(win, (uint32_t)si);
										break;
									}
								}
							}
							sid = win->widgets[si].parent_id;
						}
					}
					break;
				}

				if(pev.button == 6 || pev.button == 7) {
					int32_t hi = hit_test(win, win->mouse_x, win->mouse_y);
					int32_t delta = (pev.button == 6) ? -win->row_height * 3 : win->row_height * 3;
					if(hi >= 0 && win->widgets[hi].type == MKGUI_LISTVIEW) {
						struct mkgui_listview_data *lv = find_listv_data(win, win->widgets[hi].id);
						if(lv) {
							lv->scroll_x += delta;
							int32_t content_w = win->rects[hi].w - 2 - win->scrollbar_w;
							listview_clamp_scroll_x(lv, content_w);
							dirty_widget(win, (uint32_t)hi);
						}

					} else if(hi >= 0 && win->widgets[hi].type == MKGUI_CANVAS) {
						ev->type = MKGUI_EVENT_SCROLL;
						ev->id = win->widgets[hi].id;
						ev->value = delta;
						ev->col = 1;
						return 1;

					} else {
						uint32_t sid = hi >= 0 ? win->widgets[hi].parent_id : 0;
						while(sid) {
							int32_t si = find_widget_idx(win, sid);
							if(si < 0) {
								break;
							}

							if((win->widgets[si].type == MKGUI_VBOX || win->widgets[si].type == MKGUI_HBOX) && (win->widgets[si].flags & MKGUI_SCROLL)) {
								struct mkgui_box_scroll *bs = find_box_scroll(win, sid);
								if(bs && bs->content_w > win->rects[si].w) {
									int32_t max_scroll = bs->content_w - win->rects[si].w;
									if(max_scroll < 0) {
										max_scroll = 0;
									}
									bs->scroll_x += delta;
									if(bs->scroll_x < 0) {
										bs->scroll_x = 0;
									}

									if(bs->scroll_x > max_scroll) {
										bs->scroll_x = max_scroll;
									}
									dirty_widget(win, (uint32_t)si);
									break;
								}
							}
							sid = win->widgets[si].parent_id;
						}
					}
					break;
				}

				for(int32_t bi = (int32_t)win->widget_count - 1; bi >= 0; --bi) {
					struct mkgui_widget *bw = &win->widgets[bi];
					if((bw->type == MKGUI_VBOX || bw->type == MKGUI_HBOX) && (bw->flags & MKGUI_SCROLL)) {
						struct mkgui_box_scroll *bs = find_box_scroll(win, bw->id);
						if(!bs) {
							continue;
						}

						if(bw->type == MKGUI_HBOX && bs->content_w > win->rects[bi].w) {
							int32_t sy = win->rects[bi].y + win->rects[bi].h - win->scrollbar_w;
							int32_t sx = win->rects[bi].x;
							int32_t sw = win->rects[bi].w;
							if(win->mouse_x >= sx && win->mouse_x < sx + sw && win->mouse_y >= sy && win->mouse_y < sy + win->scrollbar_w) {
								int32_t track = sw;
								int32_t total = bs->content_w;
								if(total < 1) {
									total = 1;
								}
								int32_t thumb = track * sw / total;
								if(thumb < sc(win, 20)) {
									thumb = sc(win, 20);
								}

								if(thumb > track) {
									thumb = track;
								}
								int32_t max_scroll = bs->content_w - sw;
								if(max_scroll < 1) {
									max_scroll = 1;
								}
								int32_t pos = (int32_t)((int64_t)bs->scroll_x * (track - thumb) / max_scroll);
								if(win->mouse_x >= sx + pos && win->mouse_x < sx + pos + thumb) {
									win->drag_scrollbar_id = bw->id;
									win->drag_scrollbar_offset = win->mouse_x - (sx + pos);
								}
								break;
							}
						}

						if(bs->content_h <= win->rects[bi].h) {
							continue;
						}
						int32_t sx = win->rects[bi].x + win->rects[bi].w - win->scrollbar_w;
						int32_t sy = win->rects[bi].y;
						int32_t sh = win->rects[bi].h;
						if(win->mouse_x >= sx && win->mouse_x < sx + win->scrollbar_w && win->mouse_y >= sy && win->mouse_y < sy + sh) {
							int32_t track = sh;
							int32_t total = bs->content_h;
							if(total < 1) {
								total = 1;
							}
							int32_t thumb = track * sh / total;
							if(thumb < sc(win, 20)) {
								thumb = sc(win, 20);
							}

							if(thumb > track) {
								thumb = track;
							}
							int32_t max_scroll = bs->content_h - sh;
							if(max_scroll < 1) {
								max_scroll = 1;
							}
							int32_t pos = (int32_t)((int64_t)bs->scroll_y * (track - thumb) / max_scroll);
							if(pos < 0) {
								pos = 0;
							}

							if(pos > track - thumb) {
								pos = track - thumb;
							}
							int32_t thumb_y = sy + pos;
							if(win->mouse_y >= thumb_y && win->mouse_y < thumb_y + thumb) {
								win->drag_scrollbar_id = bw->id;
								win->drag_scrollbar_offset = win->mouse_y - thumb_y;
							} else {
								int32_t dir = win->mouse_y < thumb_y ? -1 : 1;
								int32_t step = sh / 2;
								bs->scroll_y += dir * step;
								if(bs->scroll_y < 0) {
									bs->scroll_y = 0;
								}

								if(bs->scroll_y > max_scroll) {
									bs->scroll_y = max_scroll;
								}
							}
							dirty_widget(win, (uint32_t)bi);
							break;
						}
					}
				}

				if(win->drag_scrollbar_id) {
					break;
				}

				int32_t hi = hit_test(win, win->mouse_x, win->mouse_y);
				if(hi >= 0) {
					struct mkgui_widget *hw = &win->widgets[hi];

					if(hw->flags & MKGUI_DISABLED) {
						break;
					}

					if(is_focusable(hw)) {
						spinbox_focus_lost(win);
						if(win->focus_id != hw->id) {
							pathbar_focus_lost(win);
						}
						win->focus_id = hw->id;

					} else {
						spinbox_focus_lost(win);
						pathbar_focus_lost(win);
						win->focus_id = 0;
					}

					win->press_id = hw->id;
					win->press_mod = pev.keymod;
					dirty_all(win);

					if(hw->type == MKGUI_INPUT) {
						struct mkgui_input_data *inp = find_input_data(win, hw->id);
						if(inp) {
							uint32_t now = (uint32_t)(mkgui_now_ns() / 1000000ull);
							uint32_t is_dblclick = (win->dblclick_id == hw->id && (now - win->dblclick_time) < 400);
							win->dblclick_id = hw->id;
							win->dblclick_time = now;
							if(is_dblclick) {
								uint32_t len = (uint32_t)strlen(inp->text);
								inp->sel_start = 0;
								inp->sel_end = len;
								inp->cursor = len;
								win->dblclick_id = 0;
							} else {
								char *display = inp->text;
								char masked_buf[MKGUI_MAX_TEXT];
								if(hw->style & MKGUI_INPUT_PASSWORD) {
									uint32_t mlen = (uint32_t)strlen(inp->text);
									for(uint32_t mi = 0; mi < mlen && mi < MKGUI_MAX_TEXT - 1; ++mi) {
										masked_buf[mi] = '*';
									}
									masked_buf[mlen < MKGUI_MAX_TEXT - 1 ? mlen : MKGUI_MAX_TEXT - 1] = '\0';
									display = masked_buf;
								}
								inp->cursor = input_hit_cursor(win, inp, display, win->rects[hi].x, win->mouse_x);
								input_clear_selection(inp);
								win->drag_select_id = hw->id;
							}
							dirty_widget(win, (uint32_t)hi);
							input_scroll_to_cursor(win, hw->id);
						}
					}

					if(hw->type == MKGUI_IPINPUT) {
						handle_ipinput_click(win, hw->id, win->mouse_x);
					}

					if(hw->type == MKGUI_TOGGLE) {
						handle_toggle_click(win, ev, hw->id);
						return ev->type != MKGUI_EVENT_NONE;
					}

					if(hw->type == MKGUI_COMBOBOX) {
						if(closed_combobox_id == hw->id) {
							int32_t btn_x = win->rects[hi].x + win->rects[hi].w - sc(win, 24);
							if(win->mouse_x < btn_x) {
								handle_combobox_click(win, ev, hw->id);
							}
						} else {
							handle_combobox_click(win, ev, hw->id);
						}
					}

					if(hw->type == MKGUI_DATEPICKER) {
						handle_datepicker_click(win, hw->id);
					}

					if(hw->type == MKGUI_TEXTAREA) {
						struct mkgui_textarea_data *ta = find_textarea_data(win, hw->id);
						if(ta && textarea_has_scrollbar(win, (uint32_t)hi, ta)) {
							int32_t sb_off = textarea_sb_hit(win, (uint32_t)hi, ta, win->mouse_x, win->mouse_y);
							if(sb_off >= 0) {
								win->drag_scrollbar_id = hw->id;
								win->drag_scrollbar_offset = sb_off;
								break;
							}
						}

						if(ta) {
							uint32_t hit = textarea_hit_pos(win, ta, win->rects[hi].x, win->rects[hi].y, win->rects[hi].h, win->mouse_x, win->mouse_y);
							if(textarea_has_selection(ta) && !(hw->style & MKGUI_TEXTAREA_READONLY)) {
								uint32_t lo, hi2;
								textarea_sel_range(ta, &lo, &hi2);
								if(hit >= lo && hit < hi2) {
									win->drag_text_id = hw->id;
									win->drag_text_drop_pos = hit;
									break;
								}
							}
							ta->cursor = hit;
							textarea_clear_selection(ta);
							win->drag_select_id = hw->id;
							dirty_widget(win, (uint32_t)hi);
						}
					}

					if(hw->type == MKGUI_LOGVIEW) {
						struct mkgui_logview_data *lv = find_logview_data(win, hw->id);
						if(lv) {
							logview_recompute(win, lv, hi);
							if(logview_has_scrollbar(win, (uint32_t)hi, lv)) {
								int32_t sb_off = logview_sb_hit(win, (uint32_t)hi, lv, win->mouse_x, win->mouse_y);
								if(sb_off >= 0) {
									win->drag_scrollbar_id = hw->id;
									win->drag_scrollbar_offset = sb_off;
									break;
								}
							}
							uint32_t vline, off;
							logview_hit_vline(win, lv, win->rects[hi].x, win->rects[hi].y, win->mouse_y, win->mouse_x, &vline, &off);
							uint64_t seq;
							uint32_t soff;
							logview_vline_to_pos(lv, vline, off, &seq, &soff);
							lv->sel_anchor_line_seq = seq;
							lv->sel_anchor_off = soff;
							lv->sel_cursor_line_seq = seq;
							lv->sel_cursor_off = soff;
							lv->has_selection = 0;
							win->drag_select_id = hw->id;
							dirty_widget(win, (uint32_t)hi);
							// Deliver the clicked line as a 0-based index into the
							// retained buffer; a drag-select may still follow via
							// drag_select_id, mirroring listview select-on-press.
							if(seq >= lv->line_seq_tail && seq < lv->line_seq_head) {
								ev->type = MKGUI_EVENT_LOGVIEW_LINE_CLICKED;
								ev->id = hw->id;
								ev->value = (int32_t)(seq - lv->line_seq_tail);
								return 1;
							}
						}
					}

					if(hw->type == MKGUI_TABS) {
						uint32_t close_id = tab_close_hit_test(win, (uint32_t)hi, win->mouse_x, win->mouse_y);
						if(close_id) {
							ev->type = MKGUI_EVENT_TAB_CLOSE;
							ev->id = hw->id;
							ev->value = (int32_t)close_id;
							return 1;
						}
						uint32_t tab_id = tab_hit_test(win, (uint32_t)hi, win->mouse_x, win->mouse_y);
						if(tab_id) {
							struct mkgui_tabs_data *td = find_tabs_data(win, hw->id);
							if(td && td->active_tab != tab_id) {
								td->active_tab = tab_id;
								dirty_all(win);
								ev->type = MKGUI_EVENT_TAB_CHANGED;
								ev->id = hw->id;
								ev->value = (int32_t)tab_id;
								return 1;
							}
						}
					}

					if(hw->type == MKGUI_LISTVIEW) {
						uint32_t hsb_hit = listview_hscrollbar_hit(win, (uint32_t)hi, win->mouse_x, win->mouse_y);
						if(hsb_hit == 1) {
							win->drag_scrollbar_id = hw->id;
							win->drag_scrollbar_offset = listview_hthumb_offset(win, (uint32_t)hi, win->mouse_x);
							win->drag_scrollbar_horiz = 1;

						} else if(hsb_hit == 2 || hsb_hit == 3) {
							struct mkgui_listview_data *hlv = find_listv_data(win, hw->id);
							if(hlv) {
								int32_t cw = win->rects[hi].w - 2 - win->scrollbar_w;
								hlv->scroll_x += (hsb_hit == 2 ? -1 : 1) * (cw / 2);
								listview_clamp_scroll_x(hlv, cw);
								dirty_widget(win, (uint32_t)hi);
							}

						} else {
						uint32_t sb_hit = listview_scrollbar_hit(win, (uint32_t)hi, win->mouse_x, win->mouse_y);
						if(sb_hit == 1) {
							win->drag_scrollbar_id = hw->id;
							win->drag_scrollbar_offset = listview_thumb_offset(win, (uint32_t)hi, win->mouse_y);
							win->drag_scrollbar_horiz = 0;

						} else if(sb_hit == 2 || sb_hit == 3) {
							listview_page_scroll(win, (uint32_t)hi, sb_hit == 2 ? -1 : 1);

						} else {
							int32_t div = listview_divider_hit(win, (uint32_t)hi, win->mouse_x, win->mouse_y);
							if(div >= 0) {
								struct mkgui_listview_data *lv = find_listv_data(win, hw->id);
								if(lv) {
									uint32_t logical = lv->col_order[div];
									uint32_t now_ms = (uint32_t)(mkgui_now_ns() / 1000000ull);
									if(win->divider_dblclick_id == hw->id && win->divider_dblclick_col == (int32_t)logical && (now_ms - win->divider_dblclick_time) < 400) {
										listview_autosize_col(win, lv, logical);
										win->divider_dblclick_id = 0;

									} else {
										win->divider_dblclick_id = hw->id;
										win->divider_dblclick_col = (int32_t)logical;
										win->divider_dblclick_time = now_ms;
										win->drag_col_resize_id = hw->id;
										win->drag_col_resize_col = (int32_t)logical;
										win->drag_col_resize_start_x = win->mouse_x;
										win->drag_col_resize_start_w = lv->columns[logical].width;
									}
								}

							} else {
								int32_t dcol = listview_header_hit(win, (uint32_t)hi, win->mouse_x, win->mouse_y);
								if(dcol >= 0) {
									win->drag_col_id = hw->id;
									win->drag_col_src = dcol;
									win->drag_col_start_x = win->mouse_x;

								} else {
									int32_t row = listview_row_hit(win, (uint32_t)hi, win->mouse_x, win->mouse_y);
									if(row >= 0) {
										struct mkgui_listview_data *lv = find_listv_data(win, hw->id);
										if(lv) {
											uint32_t now = (uint32_t)(mkgui_now_ns() / 1000000ull);
											uint32_t elapsed = now - win->dblclick_time;
											uint32_t is_dblclick = (win->dblclick_id == hw->id && win->dblclick_row == row && elapsed < 400);
											uint32_t slow_click = (win->dblclick_id == hw->id && win->dblclick_row == row && elapsed >= 400 && elapsed < 1500);
											uint32_t was_selected = (lv->selected_row == row);
											win->dblclick_id = hw->id;
											win->dblclick_row = row;
											win->dblclick_time = now;
											if(hw->style & MKGUI_LISTVIEW_MULTI_SELECT) {
												uint32_t mod_ctrl = (pev.keymod & MKGUI_MOD_CONTROL) != 0;
												uint32_t mod_shift = (pev.keymod & MKGUI_MOD_SHIFT) != 0;
												if(mod_ctrl) {
													lv_multi_sel_toggle(lv, row);
													lv->selected_row = row;
												} else if(mod_shift) {
													int32_t anchor = lv->selected_row;
													if(anchor < 0) {
														anchor = row;
													}
													int32_t lo = anchor < row ? anchor : row;
													int32_t hi2 = anchor < row ? row : anchor;
													lv->multi_sel_count = 0;
													for(int32_t r = lo; r <= hi2 && lv->multi_sel_count < MKGUI_MAX_MULTI_SEL; ++r) {
														lv->multi_sel[lv->multi_sel_count++] = r;
													}
													lv->selected_row = row;
												} else {
													lv->multi_sel[0] = row;
													lv->multi_sel_count = 1;
													lv->selected_row = row;
												}
											} else {
												lv->selected_row = row;
											}
											int32_t col = listview_col_hit(win, (uint32_t)hi, win->mouse_x);
											if(is_dblclick) {
												ev->type = MKGUI_EVENT_LISTVIEW_DBLCLICK;
												ev->id = hw->id;
												ev->value = row;
												ev->col = col;
												win->dblclick_id = 0;
												dirty_widget(win, (uint32_t)hi);
												return 1;
											}

											if(was_selected && slow_click && (hw->style & MKGUI_LISTVIEW_EDITABLE)) {
												char cell_buf[MKGUI_MAX_TEXT] = {0};
												if(lv->row_cb) {
													lv->row_cb((uint32_t)row, 0, cell_buf, MKGUI_MAX_TEXT, lv->userdata);
												}
												celledit_begin(win, hw->id, row, 0, cell_buf);
												dirty_widget(win, (uint32_t)hi);
												return 0;
											}
											lv->drag_source = row;
											lv->drag_target = -1;
											lv->drag_start_y = win->mouse_y;
											lv->drag_active = 0;
											ev->type = MKGUI_EVENT_LISTVIEW_SELECT;
											ev->id = hw->id;
											ev->value = row;
											ev->col = col;
											dirty_widget(win, (uint32_t)hi);
											return 1;
										}
									}
								}
							}
						}
					}
					}

					if(hw->type == MKGUI_GRIDVIEW) {
						uint32_t gv_sb_hit = gridview_scrollbar_hit(win, (uint32_t)hi, win->mouse_x, win->mouse_y);
						if(gv_sb_hit == 1) {
							win->drag_scrollbar_id = hw->id;
							win->drag_scrollbar_offset = gridview_thumb_offset(win, (uint32_t)hi, win->mouse_y);

						} else if(gv_sb_hit == 2 || gv_sb_hit == 3) {
							struct mkgui_gridview_data *gv = find_gridv_data(win, hw->id);
							if(gv) {
								int32_t ghh = gv->header_height > 0 ? gv->header_height : win->row_height;
								int32_t gcontent_h = win->rects[hi].h - ghh - 2;
								gv->scroll_y += (gv_sb_hit == 2 ? -1 : 1) * gcontent_h;
								gridview_clamp_scroll(win, gv, gcontent_h);
								dirty_widget(win, (uint32_t)hi);
							}

						} else {
							int32_t gv_div = gridview_divider_hit(win, (uint32_t)hi, win->mouse_x, win->mouse_y);
							if(gv_div >= 0) {
								struct mkgui_gridview_data *gv = find_gridv_data(win, hw->id);
								if(gv) {
									uint32_t now_ms = (uint32_t)(mkgui_now_ns() / 1000000ull);
									if(win->divider_dblclick_id == hw->id && win->divider_dblclick_col == gv_div && (now_ms - win->divider_dblclick_time) < 400) {
										gridview_autosize_col(win, gv, (uint32_t)gv_div);
										win->divider_dblclick_id = 0;

									} else {
										win->divider_dblclick_id = hw->id;
										win->divider_dblclick_col = gv_div;
										win->divider_dblclick_time = now_ms;
										win->drag_col_resize_id = hw->id;
										win->drag_col_resize_col = gv_div;
										win->drag_col_resize_start_x = win->mouse_x;
										win->drag_col_resize_start_w = gv->columns[gv_div].width;
									}
								}

							} else {
								struct mkgui_gridview_data *gv = find_gridv_data(win, hw->id);
								if(gv) {
									int32_t ghh = gv->header_height > 0 ? gv->header_height : win->row_height;
									int32_t gcontent_y = win->rects[hi].y + ghh + 1;
									int32_t grow = (win->mouse_y - gcontent_y + gv->scroll_y) / win->row_height;
									int32_t gcol = gridview_col_at_x(gv, win->mouse_x, win->rects[hi].x + 1);
									if(grow >= 0 && grow < (int32_t)gv->row_count) {
										gv->selected_row = grow;
										gv->selected_col = gcol;
										gv->drag_source = grow;
										gv->drag_target = -1;
										gv->drag_start_y = win->mouse_y;
										gv->drag_active = 0;
										if(gcol >= 0 && gcol < (int32_t)gv->col_count) {
											uint32_t ct = gv->columns[gcol].col_type;
											if(ct == MKGUI_GRID_CHECK || ct == MKGUI_GRID_CHECK_TEXT) {
												uint32_t cur = gridview_get_bit(gv, (uint32_t)grow, (uint32_t)gcol);
												gridview_set_bit(gv, (uint32_t)grow, (uint32_t)gcol, !cur);
												ev->type = MKGUI_EVENT_GRID_CHECK;
												ev->id = hw->id;
												ev->value = grow;
												ev->col = gcol;
												dirty_widget(win, (uint32_t)hi);
												return 1;
											}
										}
										ev->type = MKGUI_EVENT_GRID_CLICK;
										ev->id = hw->id;
										ev->value = grow;
										ev->col = gcol;
										dirty_widget(win, (uint32_t)hi);
										return 1;
									}
								}
							}
						}
					}

					if(hw->type == MKGUI_RICHLIST) {
						uint32_t rl_sb_hit = richlist_scrollbar_hit(win, (uint32_t)hi, win->mouse_x, win->mouse_y);
						if(rl_sb_hit == 1) {
							win->drag_scrollbar_id = hw->id;
							win->drag_scrollbar_offset = richlist_thumb_offset(win, (uint32_t)hi, win->mouse_y);

						} else if(rl_sb_hit == 2 || rl_sb_hit == 3) {
							struct mkgui_richlist_data *rl = find_richlist_data(win, hw->id);
							if(rl) {
								int32_t content_h = win->rects[hi].h - 2;
								rl->scroll_y += (rl_sb_hit == 2 ? -1 : 1) * content_h;
								richlist_clamp_scroll(rl, content_h);
								dirty_widget(win, (uint32_t)hi);
							}

						} else {
							struct mkgui_richlist_data *rl = find_richlist_data(win, hw->id);
							if(rl) {
								int32_t row = richlist_row_hit(win, (uint32_t)hi, win->mouse_y);
								if(row >= 0) {
									uint32_t now = (uint32_t)(mkgui_now_ns() / 1000000ull);
									uint32_t is_dblclick = (win->dblclick_id == hw->id && win->dblclick_row == row && (now - win->dblclick_time) < 400);
									win->dblclick_id = hw->id;
									win->dblclick_row = row;
									win->dblclick_time = now;
									rl->selected_row = row;
									dirty_widget(win, (uint32_t)hi);
									if(is_dblclick) {
										win->dblclick_id = 0;
										ev->type = MKGUI_EVENT_RICHLIST_DBLCLICK;
										ev->id = hw->id;
										ev->value = row;
										return 1;
									}
									ev->type = MKGUI_EVENT_RICHLIST_SELECT;
									ev->id = hw->id;
									ev->value = row;
									return 1;
								}
							}
						}
					}

					if(hw->type == MKGUI_ITEMVIEW) {
						uint32_t iv_sb_hit = itemview_scrollbar_hit(win, (uint32_t)hi, win->mouse_x, win->mouse_y);
						if(iv_sb_hit == 1) {
							win->drag_scrollbar_id = hw->id;
							win->drag_scrollbar_offset = itemview_thumb_offset(win, (uint32_t)hi, win->mouse_x, win->mouse_y);

						} else if(iv_sb_hit == 2 || iv_sb_hit == 3) {
							itemview_page_scroll(win, (uint32_t)hi, iv_sb_hit == 2 ? -1 : 1);

						} else {
							struct mkgui_itemview_data *iv = find_itemview_data(win, hw->id);
							if(iv) {
								int32_t item = itemview_hit_item(win, (uint32_t)hi, iv, win->mouse_x, win->mouse_y);
								if(item >= 0) {
									uint32_t now = (uint32_t)(mkgui_now_ns() / 1000000ull);
									uint32_t elapsed = now - win->dblclick_time;
									uint32_t is_dblclick = (win->dblclick_id == hw->id && win->dblclick_row == item && elapsed < 400);
									uint32_t slow_click = (win->dblclick_id == hw->id && win->dblclick_row == item && elapsed >= 400 && elapsed < 1500);
									uint32_t was_selected = (iv->selected == item);
									win->dblclick_id = hw->id;
									win->dblclick_row = item;
									win->dblclick_time = now;
									iv->selected = item;
									if(is_dblclick) {
										ev->type = MKGUI_EVENT_ITEMVIEW_DBLCLICK;
										win->dblclick_id = 0;
									} else if(was_selected && slow_click && (hw->style & MKGUI_ITEMVIEW_EDITABLE)) {
										char label[MKGUI_MAX_TEXT] = {0};
										if(iv->label_cb) {
											iv->label_cb((uint32_t)item, label, MKGUI_MAX_TEXT, iv->userdata);
										}
										celledit_begin(win, hw->id, item, 0, label);
										dirty_widget(win, (uint32_t)hi);
										return 0;
									} else {
										ev->type = MKGUI_EVENT_ITEMVIEW_SELECT;
									}
									ev->id = hw->id;
									ev->value = item;
									dirty_widget(win, (uint32_t)hi);
									return 1;
								}
							}
						}
					}

					if(hw->type == MKGUI_SCROLLBAR) {
						if(scrollbar_hit_thumb(win, (uint32_t)hi, win->mouse_x, win->mouse_y)) {
							win->drag_scrollbar_id = hw->id;
							win->drag_scrollbar_offset = scrollbar_thumb_drag_offset(win, (uint32_t)hi, win->mouse_x, win->mouse_y);
						} else {
							scrollbar_scroll(win, hw->id, win->mouse_y > win->rects[hi].y + win->rects[hi].h / 2 ? 1 : -1);
							ev->type = MKGUI_EVENT_SCROLL;
							ev->id = hw->id;
							struct mkgui_scrollbar_data *sbd = find_scrollbar_data(win, hw->id);
							ev->value = sbd ? sbd->value : 0;
							return 1;
						}
					}

					if(hw->type == MKGUI_PATHBAR) {
						if(handle_pathbar_click(win, ev, (uint32_t)hi, win->mouse_x, win->mouse_y)) {
							return 1;
						}
					}

					if(hw->type == MKGUI_MENUITEM) {
						struct mkgui_widget *parent = find_widget(win, hw->parent_id);
						if(parent && parent->type == MKGUI_MENU) {
							menu_open_popup(win, hw->id);
						}
					}

					if(hw->type == MKGUI_SLIDER) {
						struct mkgui_slider_data *sd = find_slider_data(win, hw->id);
						if(sd) {
							int32_t range = sd->max_val - sd->min_val;
							int32_t thumb_sz = sc(win, 10);
							int32_t new_val;
							win->drag_slider_offset = 0;
							if(hw->flags & MKGUI_VERTICAL) {
								int32_t ry = win->rects[hi].y;
								int32_t rh = win->rects[hi].h;
								int32_t track = rh - thumb_sz;
								int32_t thumb_y = ry + rh - thumb_sz - (range > 0 ? (int32_t)((int64_t)(sd->value - sd->min_val) * track / range) : 0);
								if(win->mouse_y >= thumb_y && win->mouse_y < thumb_y + thumb_sz) {
									win->drag_slider_offset = win->mouse_y - (thumb_y + thumb_sz / 2);
									new_val = sd->value;
								} else {
									int32_t pos = win->mouse_y - ry - thumb_sz / 2;
									new_val = sd->max_val - (track > 0 ? (int32_t)((int64_t)pos * range / track) : 0);
								}
							} else {
								int32_t rx = win->rects[hi].x;
								int32_t rw = win->rects[hi].w;
								int32_t track = rw - thumb_sz;
								int32_t thumb_x = rx + (range > 0 ? (int32_t)((int64_t)(sd->value - sd->min_val) * track / range) : 0);
								if(win->mouse_x >= thumb_x && win->mouse_x < thumb_x + thumb_sz) {
									win->drag_slider_offset = win->mouse_x - (thumb_x + thumb_sz / 2);
									new_val = sd->value;
								} else {
									int32_t pos = win->mouse_x - rx - thumb_sz / 2;
									new_val = sd->min_val + (track > 0 ? (int32_t)((int64_t)pos * range / track) : 0);
								}
							}

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
							dirty_widget(win, (uint32_t)hi);
							return 1;
						}
					}

					if(hw->type == MKGUI_DROPDOWN) {
						if(closed_dropdown_id != hw->id) {
							dropdown_open_popup(win, hw->id);
						}
					}

					if(hw->type == MKGUI_TREEVIEW) {
						int32_t node_idx = treeview_row_hit(win, (uint32_t)hi, win->mouse_y);
						if(node_idx >= 0) {
							struct mkgui_treeview_data *tv = find_treeview_data(win, hw->id);
							if(tv) {
								if(treeview_arrow_hit(win, (uint32_t)hi, win->mouse_x, node_idx) && tv->nodes[node_idx].has_children) {
									tv->nodes[node_idx].expanded = !tv->nodes[node_idx].expanded;
									dirty_all(win);
									ev->type = tv->nodes[node_idx].expanded ? MKGUI_EVENT_TREEVIEW_EXPAND : MKGUI_EVENT_TREEVIEW_COLLAPSE;
									ev->id = hw->id;
									ev->value = (int32_t)tv->nodes[node_idx].id;
									return 1;

								} else {
									uint32_t now_ms = (uint32_t)(mkgui_now_ns() / 1000000ull);
									int32_t node_id = (int32_t)tv->nodes[node_idx].id;
									uint32_t elapsed = now_ms - win->dblclick_time;
									uint32_t is_dblclick = (win->dblclick_id == hw->id && win->dblclick_row == node_id && elapsed < 400);
									uint32_t slow_click = (win->dblclick_id == hw->id && win->dblclick_row == node_id && elapsed >= 400 && elapsed < 1500);
									uint32_t was_selected = (tv->selected_node == node_id);
									win->dblclick_id = hw->id;
									win->dblclick_row = node_id;
									win->dblclick_time = now_ms;
									tv->selected_node = node_id;
									tv->drag_source = node_idx;
									tv->drag_target = -1;
									tv->drag_pos = 0;
									tv->drag_active = 0;
									tv->drag_start_y = win->mouse_y;
									dirty_widget(win, (uint32_t)hi);
									if(is_dblclick) {
										ev->type = MKGUI_EVENT_TREEVIEW_DBLCLICK;
										win->dblclick_id = 0;
									} else if(was_selected && slow_click && (hw->style & MKGUI_TREEVIEW_EDITABLE)) {
										celledit_begin(win, hw->id, node_id, 0, tv->nodes[node_idx].label);
										return 0;
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
						int32_t dir = spinbox_btn_hit(win, (uint32_t)hi, win->mouse_x, win->mouse_y);
						struct mkgui_spinbox_data *sd = find_spinbox_data(win, hw->id);
						if(sd) {
							if(dir != 0) {
								sd->repeat_dir = dir;
								sd->repeat_next_ms = (uint32_t)(mkgui_now_ns() / 1000000ull) + 400;
								int32_t delta = (dir > 0) ? sd->step : -sd->step;
								spinbox_adjust(win, ev, hw->id, delta);
								return ev->type != MKGUI_EVENT_NONE;

							} else {
								sd->editing = 1;
								sd->select_all = 1;
								snprintf(sd->edit_buf, sizeof(sd->edit_buf), "%d", sd->value);
								sd->edit_len = (uint32_t)strlen(sd->edit_buf);
								dirty_widget(win, (uint32_t)hi);
							}
						}
					}

				} else {
					spinbox_focus_lost(win);
					pathbar_focus_lost(win);
					win->focus_id = 0;
				}
			} break;

			// -=[ BUTTON RELEASE ]=-
			case MKGUI_PLAT_BUTTON_RELEASE: {
				// buttons 4+ are scroll wheel; their "release" is meaningless
				// and must not trigger drag completion or click logic
				if(pev.button >= 4) {
					break;
				}

				if(win->canvas_window && popup_idx < 0) {
					for(uint32_t ci = 0; ci < win->canvas_count; ++ci) {
						ev->type = MKGUI_EVENT_CANVAS_RELEASE;
						ev->id = win->canvases[ci].widget_id;
						ev->value = pev.x;
						ev->col = pev.y;
						ev->keysym = pev.button;
						ev->keymod = pev.keymod;
						return 1;
					}
					break;
				}

				win->cell_edit.dragging = 0;

				for(uint32_t si = 0; si < win->spinbox_count; ++si) {
					win->spinboxes[si].repeat_dir = 0;
				}

				for(uint32_t tvi = 0; tvi < win->treeview_count; ++tvi) {
					struct mkgui_treeview_data *tv = &win->treeviews[tvi];
					if(tv->drag_active && tv->drag_target >= 0) {
						int32_t src_id = (int32_t)tv->nodes[tv->drag_source].id;
						int32_t tgt_id = (int32_t)tv->nodes[tv->drag_target].id;
						int32_t pos = tv->drag_pos;
						tv->drag_active = 0;
						tv->drag_source = -1;
						tv->drag_target = -1;
						dirty_all(win);
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
						dirty_widget_id(win, tv->widget_id);
					}
					tv->drag_active = 0;
					tv->drag_source = -1;
					tv->drag_target = -1;
				}
				for(uint32_t lvi = 0; lvi < win->listv_count; ++lvi) {
					struct mkgui_listview_data *lv = &win->listvs[lvi];
					if(lv->drag_active && lv->drag_target >= 0) {
						int32_t src = lv->drag_source;
						int32_t tgt = lv->drag_target;
						lv->drag_active = 0;
						lv->drag_source = -1;
						lv->drag_target = -1;
						dirty_all(win);
						ev->type = MKGUI_EVENT_LISTVIEW_REORDER;
						ev->id = lv->widget_id;
						ev->value = src;
						ev->col = tgt;
						return 1;
					}

					if(lv->drag_active) {
						ev->type = MKGUI_EVENT_DRAG_END;
						ev->id = lv->widget_id;
						dirty_widget_id(win, lv->widget_id);
					}
					lv->drag_active = 0;
					lv->drag_source = -1;
					lv->drag_target = -1;
				}
				for(uint32_t gvi = 0; gvi < win->gridview_count; ++gvi) {
					struct mkgui_gridview_data *gv = &win->gridviews[gvi];
					if(gv->drag_active && gv->drag_target >= 0) {
						int32_t src = gv->drag_source;
						int32_t tgt = gv->drag_target;
						gv->drag_active = 0;
						gv->drag_source = -1;
						gv->drag_target = -1;
						dirty_all(win);
						ev->type = MKGUI_EVENT_GRIDVIEW_REORDER;
						ev->id = gv->widget_id;
						ev->value = src;
						ev->col = tgt;
						return 1;
					}

					if(gv->drag_active) {
						ev->type = MKGUI_EVENT_DRAG_END;
						ev->id = gv->widget_id;
						dirty_widget_id(win, gv->widget_id);
					}
					gv->drag_active = 0;
					gv->drag_source = -1;
					gv->drag_target = -1;
				}

				win->drag_select_id = 0;

				if(win->drag_text_id) {
					uint32_t dtid = win->drag_text_id;
					uint32_t drop = win->drag_text_drop_pos;
					win->drag_text_id = 0;
					struct mkgui_textarea_data *ta = find_textarea_data(win, dtid);
					if(ta && textarea_has_selection(ta)) {
						uint32_t lo, hi2;
						textarea_sel_range(ta, &lo, &hi2);
						if(drop < lo || drop >= hi2) {
							textarea_undo_push_force(ta);
							uint32_t sel_len = hi2 - lo;
							char *tmp = (char *)malloc(sel_len);
							if(tmp) {
								memcpy(tmp, &ta->text[lo], sel_len);
								if(drop > hi2) {
									memmove(&ta->text[lo], &ta->text[hi2], drop - hi2);
									memcpy(&ta->text[drop - sel_len], tmp, sel_len);
									ta->cursor = drop;
									ta->sel_start = drop - sel_len;
									ta->sel_end = drop;
								} else {
									memmove(&ta->text[drop + sel_len], &ta->text[drop], lo - drop);
									memcpy(&ta->text[drop], tmp, sel_len);
									ta->cursor = drop + sel_len;
									ta->sel_start = drop;
									ta->sel_end = drop + sel_len;
								}
								free(tmp);
								ev->type = MKGUI_EVENT_TEXTAREA_CHANGED;
								ev->id = dtid;
								dirty_widget_id(win, dtid);
								return 1;
							}
						} else {
							ta->cursor = drop;
							textarea_clear_selection(ta);
						}
					}
					dirty_widget_id(win, dtid);
					break;
				}

				if(win->drag_col_resize_id) {
					uint32_t dcr_id = win->drag_col_resize_id;
					win->drag_col_resize_id = 0;
					dirty_widget_id(win, dcr_id);
					break;
				}

				if(win->drag_col_id) {
					uint32_t dcid = win->drag_col_id;
					int32_t src = win->drag_col_src;
					int32_t dx = pev.x - win->drag_col_start_x;
					win->drag_col_id = 0;
					win->drag_col_src = -1;
					dirty_widget_id(win, dcid);

					struct mkgui_listview_data *lv = find_listv_data(win, dcid);
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
							int32_t widx = find_widget_idx(win, dcid);
							if(widx >= 0) {
								int32_t dst = listview_col_insert_pos(win, lv, (uint32_t)widx);
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

				uint32_t was_press = win->press_id;
				win->press_id = 0;
				if(win->drag_scrollbar_id) {
					int32_t dsi = find_widget_idx(win, win->drag_scrollbar_id);
					if(dsi >= 0 && win->widgets[dsi].type == MKGUI_SCROLLBAR) {
						struct mkgui_scrollbar_data *sbd = find_scrollbar_data(win, win->drag_scrollbar_id);
						ev->type = MKGUI_EVENT_SCROLL;
						ev->id = win->drag_scrollbar_id;
						ev->value = sbd ? sbd->value : 0;
						win->drag_scrollbar_id = 0;
						dirty_widget(win, (uint32_t)dsi);
						return 1;
					}
				}
				win->drag_scrollbar_id = 0;
				if(was_press) {
					dirty_widget_id(win, was_press);
				}

				if(was_press) {
					int32_t pi = find_widget_idx(win, was_press);
					if(pi >= 0) {
						uint32_t pt = win->widgets[pi].type;
						if(pt == MKGUI_SLIDER) {
							struct mkgui_slider_data *sd = find_slider_data(win, was_press);
							ev->type = MKGUI_EVENT_SLIDER_END;
							ev->id = was_press;
							ev->value = sd ? sd->value : 0;
							return 1;
						}

						if(pt == MKGUI_HSPLIT || pt == MKGUI_VSPLIT) {
							ev->type = MKGUI_EVENT_SPLIT_MOVED;
							ev->id = was_press;
							return 1;
						}
					}

					// click fires only when release lands on the same widget
					// that received the press; dragging off and releasing
					// elsewhere cancels the interaction
					int32_t hi = hit_test(win, pev.x, pev.y);
					if(hi >= 0 && win->widgets[hi].id == was_press) {
						struct mkgui_widget *hw = &win->widgets[hi];

						if(hw->type == MKGUI_BUTTON) {
							ev->type = MKGUI_EVENT_CLICK;
							ev->id = hw->id;
							return 1;
						}

						if(hw->type == MKGUI_LABEL && (hw->style & MKGUI_LABEL_LINK)) {
							ev->type = MKGUI_EVENT_CLICK;
							ev->id = hw->id;
							return 1;
						}

						if(hw->type == MKGUI_GROUP && (hw->style & MKGUI_GROUP_COLLAPSIBLE)) {
							hw->style ^= MKGUI_GROUP_COLLAPSED;
							dirty_all(win);
							ev->type = MKGUI_EVENT_CLICK;
							ev->id = hw->id;
							ev->value = (hw->style & MKGUI_GROUP_COLLAPSED) ? 1 : 0;
							return 1;
						}

						if(hw->type == MKGUI_CHECKBOX) {
							hw->style ^= MKGUI_CHECKBOX_CHECKED;
							ev->type = MKGUI_EVENT_CHECKBOX_CHANGED;
							ev->id = hw->id;
							ev->value = (hw->style & MKGUI_CHECKBOX_CHECKED) ? 1 : 0;
							return 1;
						}

						if(hw->type == MKGUI_RADIO) {
							return handle_radio_click(win, ev, hw);
						}
					}
				}
			} break;

			// -=[ KEY ]=-
			case MKGUI_PLAT_KEY: {
				uint32_t ks = pev.keysym;

				if(win->popup_count > 0 && win->popups[win->popup_count - 1].widget_id == MKGUI_CTXMENU_POPUP_ID) {
					struct mkgui_popup *cp = &win->popups[win->popup_count - 1];
					if(ks == MKGUI_KEY_ESCAPE) {
						popup_destroy_all(win);
						dirty_all(win);
						break;
					}

					if(ks == MKGUI_KEY_UP) {
						int32_t next = ctxmenu_next_item(win, cp->hover_item >= 0 ? cp->hover_item : (int32_t)win->ctxmenu_count, -1);
						if(next != cp->hover_item) {
							cp->hover_item = next;
							cp->dirty = 1;
							dirty_all(win);
						}
						break;
					}

					if(ks == MKGUI_KEY_DOWN) {
						int32_t next = ctxmenu_next_item(win, cp->hover_item, 1);
						if(next != cp->hover_item) {
							cp->hover_item = next;
							cp->dirty = 1;
							dirty_all(win);
						}
						break;
					}

					if(ks == MKGUI_KEY_RETURN || ks == MKGUI_KEY_SPACE) {
						if(cp->hover_item >= 0) {
							struct mkgui_ctxmenu_item *it = ctxmenu_item_at(win, cp->hover_item);
							if(it && !(it->flags & MKGUI_DISABLED)) {
								if(it->flags & MKGUI_MENUITEM_CHECK) {
									it->flags ^= MKGUI_MENUITEM_CHECKED;
								} else if(it->flags & MKGUI_MENUITEM_RADIO) {
									for(uint32_t ri = 0; ri < win->ctxmenu_count; ++ri) {
										win->ctxmenu_items[ri].flags &= ~MKGUI_MENUITEM_CHECKED;
									}
									it->flags |= MKGUI_MENUITEM_CHECKED;
								}
								ev->type = MKGUI_EVENT_CONTEXT_MENU;
								ev->id = it->id;
								ev->value = (it->flags & MKGUI_MENUITEM_CHECKED) ? 1 : 0;
								popup_destroy_all(win);
								dirty_all(win);
								return 1;
							}
						}
						break;
					}
					break;
				}

				if(ks == MKGUI_KEY_ESCAPE && win->popup_count > 0) {
					popup_destroy(win, win->popup_count - 1);
					dirty_all(win);
					break;
				}

				// accelerators run before the focused widget so global
				// shortcuts (ctrl+s, ctrl+q, etc.) are never eaten by
				// a text input that would otherwise consume the keypress
				if(accel_dispatch(win, ks, pev.keymod, ev)) {
					return 1;
				}

				if(ks == MKGUI_KEY_TAB || ks == MKGUI_KEY_ISO_LEFT_TAB) {
					uint32_t reverse = (pev.keymod & MKGUI_MOD_SHIFT) || (ks == MKGUI_KEY_ISO_LEFT_TAB);
					uint32_t start_idx = 0;
					if(win->focus_id) {
						for(uint32_t i = 0; i < win->widget_count; ++i) {
							if(win->widgets[i].id == win->focus_id) {
								start_idx = reverse ? (i + win->widget_count - 1) : (i + 1);
								break;
							}
						}
					}
					for(uint32_t i = 0; i < win->widget_count; ++i) {
						uint32_t idx;
						if(reverse) {
							idx = (start_idx + win->widget_count - i) % win->widget_count;
						} else {
							idx = (start_idx + i) % win->widget_count;
						}

						if(widget_visible(win, idx) && is_focusable(&win->widgets[idx])) {
							spinbox_focus_lost(win);
					pathbar_focus_lost(win);
							win->focus_id = win->widgets[idx].id;
							dirty_all(win);
							break;
						}
					}
					break;
				}

				if(win->cell_edit.active) {
					if(celledit_key(win, ev, ks, pev.keymod, pev.text, pev.text_len)) {
						return ev->type != MKGUI_EVENT_NONE;
					}
				}

				if((pev.keymod & 4) && win->focus_id) {
					struct mkgui_widget *fw = find_widget(win, win->focus_id);
					if(fw && (ks == 'z' || ks == 'Z') && !(pev.keymod & MKGUI_MOD_SHIFT)) {
						if(fw->type == MKGUI_INPUT) {
							struct mkgui_input_data *inp = find_input_data(win, win->focus_id);
							if(inp && input_undo(inp)) {
								dirty_widget_id(win, win->focus_id);
								input_scroll_to_cursor(win, win->focus_id);
								ev->type = MKGUI_EVENT_INPUT_CHANGED;
								ev->id = win->focus_id;
								return 1;
							}

						} else if(fw->type == MKGUI_TEXTAREA) {
							struct mkgui_textarea_data *ta = find_textarea_data(win, win->focus_id);
							if(ta && textarea_undo(ta)) {
								dirty_widget_id(win, win->focus_id);
								textarea_scroll_to_cursor(win, win->focus_id);
								ev->type = MKGUI_EVENT_TEXTAREA_CHANGED;
								ev->id = win->focus_id;
								return 1;
							}
						}
						break;
					}

					if(fw && (ks == 'y' || ks == 'Y' || ((ks == 'z' || ks == 'Z') && (pev.keymod & MKGUI_MOD_SHIFT)))) {
						if(fw->type == MKGUI_INPUT) {
							struct mkgui_input_data *inp = find_input_data(win, win->focus_id);
							if(inp && input_redo(inp)) {
								dirty_widget_id(win, win->focus_id);
								input_scroll_to_cursor(win, win->focus_id);
								ev->type = MKGUI_EVENT_INPUT_CHANGED;
								ev->id = win->focus_id;
								return 1;
							}

						} else if(fw->type == MKGUI_TEXTAREA) {
							struct mkgui_textarea_data *ta = find_textarea_data(win, win->focus_id);
							if(ta && textarea_redo(ta)) {
								dirty_widget_id(win, win->focus_id);
								textarea_scroll_to_cursor(win, win->focus_id);
								ev->type = MKGUI_EVENT_TEXTAREA_CHANGED;
								ev->id = win->focus_id;
								return 1;
							}
						}
						break;
					}

					if(fw && (ks == 'a' || ks == 'A')) {
						if(fw->type == MKGUI_INPUT) {
							struct mkgui_input_data *inp = find_input_data(win, win->focus_id);
							if(inp) {
								inp->sel_start = 0;
								inp->sel_end = (uint32_t)strlen(inp->text);
								inp->cursor = inp->sel_end;
								dirty_widget_id(win, win->focus_id);
								input_scroll_to_cursor(win, win->focus_id);
							}

						} else if(fw->type == MKGUI_COMBOBOX) {
							struct mkgui_combobox_data *cb = find_combobox_data(win, win->focus_id);
							if(cb) {
								combobox_select_all(cb);
								dirty_widget_id(win, win->focus_id);
							}

						} else if(fw->type == MKGUI_PATHBAR) {
							struct mkgui_pathbar_data *pb = find_pathbar_data(win, win->focus_id);
							if(pb && pb->editing) {
								struct mkgui_text_edit te = {pb->edit_buf, (uint32_t)sizeof(pb->edit_buf), pb->edit_cursor, pb->edit_sel_start, pb->edit_sel_end, pb->edit_scroll_x};
								textedit_select_all(&te);
								pb->edit_cursor = te.cursor;
								pb->edit_sel_start = te.sel_start;
								pb->edit_sel_end = te.sel_end;
								dirty_widget_id(win, win->focus_id);
							}

						} else if(fw->type == MKGUI_TEXTAREA) {
							struct mkgui_textarea_data *ta = find_textarea_data(win, win->focus_id);
							if(ta) {
								ta->sel_start = 0;
								ta->sel_end = ta->text_len;
								ta->cursor = ta->text_len;
								dirty_widget_id(win, win->focus_id);
							}

						} else if(fw->type == MKGUI_LOGVIEW) {
							struct mkgui_logview_data *lv = find_logview_data(win, win->focus_id);
							if(lv && lv->line_seq_head > lv->line_seq_tail) {
								struct mkgui_logview_line *last = logview_get_line(lv, lv->line_seq_head - 1);
								lv->sel_anchor_line_seq = lv->line_seq_tail;
								lv->sel_anchor_off = 0;
								lv->sel_cursor_line_seq = lv->line_seq_head - 1;
								lv->sel_cursor_off = last ? last->len : 0;
								lv->has_selection = 1;
								dirty_widget_id(win, win->focus_id);
							}
						}
						break;
					}

					if(fw && (ks == 'c' || ks == 'C')) {
						if(fw->type == MKGUI_INPUT && !(fw->style & MKGUI_INPUT_PASSWORD)) {
							struct mkgui_input_data *inp = find_input_data(win, win->focus_id);
							if(inp) {
								if(input_has_selection(inp)) {
									uint32_t lo, hi;
									input_sel_range(inp, &lo, &hi);
									platform_clipboard_set(win, inp->text + lo, hi - lo);
								} else {
									platform_clipboard_set(win, inp->text, (uint32_t)strlen(inp->text));
								}
							}

						} else if(fw->type == MKGUI_TEXTAREA) {
							struct mkgui_textarea_data *ta = find_textarea_data(win, win->focus_id);
							if(ta) {
								if(textarea_has_selection(ta)) {
									uint32_t lo, hi;
									textarea_sel_range(ta, &lo, &hi);
									platform_clipboard_set(win, ta->text + lo, hi - lo);
								} else {
									platform_clipboard_set(win, ta->text, ta->text_len);
								}
							}

						} else if(fw->type == MKGUI_LOGVIEW) {
							struct mkgui_logview_data *lv = find_logview_data(win, win->focus_id);
							if(lv && lv->has_selection) {
								uint64_t lo_seq, hi_seq;
								uint32_t lo_off, hi_off;
								if(logview_sel_range(lv, &lo_seq, &lo_off, &hi_seq, &hi_off)) {
									uint32_t total = 0;
									for(uint64_t s = lo_seq; s <= hi_seq; ++s) {
										struct mkgui_logview_line *line = logview_get_line(lv, s);
										if(line) {
											uint32_t a = (s == lo_seq) ? lo_off : 0;
											uint32_t b = (s == hi_seq) ? hi_off : line->len;
											total += (b > a ? b - a : 0) + (s < hi_seq ? 1 : 0);
										}
									}
									char *buf = (char *)malloc(total + 1);
									if(buf) {
										uint32_t written = mkgui_logview_get_selection_text(win, win->focus_id, buf, total + 1);
										platform_clipboard_set(win, buf, written);
										free(buf);
									}
								}
							}

						} else if(fw->type == MKGUI_PATHBAR) {
							struct mkgui_pathbar_data *pb = find_pathbar_data(win, win->focus_id);
							if(pb && pb->editing) {
								if(pb->edit_sel_start != pb->edit_sel_end) {
									uint32_t lo = pb->edit_sel_start < pb->edit_sel_end ? pb->edit_sel_start : pb->edit_sel_end;
									uint32_t hi = pb->edit_sel_start < pb->edit_sel_end ? pb->edit_sel_end : pb->edit_sel_start;
									platform_clipboard_set(win, pb->edit_buf + lo, hi - lo);
								} else {
									platform_clipboard_set(win, pb->edit_buf, (uint32_t)strlen(pb->edit_buf));
								}
							}

						} else if(fw->type == MKGUI_COMBOBOX) {
							struct mkgui_combobox_data *cb = find_combobox_data(win, win->focus_id);
							if(cb) {
								if(combobox_has_selection(cb)) {
									uint32_t lo = cb->sel_start < cb->sel_end ? cb->sel_start : cb->sel_end;
									uint32_t hi = cb->sel_start < cb->sel_end ? cb->sel_end : cb->sel_start;
									platform_clipboard_set(win, cb->text + lo, hi - lo);
								} else {
									platform_clipboard_set(win, cb->text, (uint32_t)strlen(cb->text));
								}
							}
						}
						break;
					}

					if(fw && (ks == 'x' || ks == 'X')) {
						if(fw->type == MKGUI_INPUT && !(fw->style & MKGUI_INPUT_READONLY) && !(fw->style & MKGUI_INPUT_PASSWORD)) {
							struct mkgui_input_data *inp = find_input_data(win, win->focus_id);
							if(inp && input_has_selection(inp)) {
								input_undo_push_force(inp);
								uint32_t lo, hi;
								input_sel_range(inp, &lo, &hi);
								platform_clipboard_set(win, inp->text + lo, hi - lo);
								input_delete_selection(inp);
								dirty_widget_id(win, win->focus_id);
								input_scroll_to_cursor(win, win->focus_id);
								ev->type = MKGUI_EVENT_INPUT_CHANGED;
								ev->id = win->focus_id;
								return 1;
							}

						} else if(fw->type == MKGUI_TEXTAREA && !(fw->style & MKGUI_TEXTAREA_READONLY)) {
							struct mkgui_textarea_data *ta = find_textarea_data(win, win->focus_id);
							if(ta && textarea_has_selection(ta)) {
								textarea_undo_push_force(ta);
								uint32_t lo, hi;
								textarea_sel_range(ta, &lo, &hi);
								platform_clipboard_set(win, ta->text + lo, hi - lo);
								textarea_delete_selection(ta);
								dirty_widget_id(win, win->focus_id);
								textarea_scroll_to_cursor(win, win->focus_id);
								ev->type = MKGUI_EVENT_TEXTAREA_CHANGED;
								ev->id = win->focus_id;
								return 1;
							}

						} else if(fw->type == MKGUI_PATHBAR) {
							struct mkgui_pathbar_data *pb = find_pathbar_data(win, win->focus_id);
							if(pb && pb->editing && pb->edit_sel_start != pb->edit_sel_end) {
								uint32_t lo = pb->edit_sel_start < pb->edit_sel_end ? pb->edit_sel_start : pb->edit_sel_end;
								uint32_t hi = pb->edit_sel_start < pb->edit_sel_end ? pb->edit_sel_end : pb->edit_sel_start;
								platform_clipboard_set(win, pb->edit_buf + lo, hi - lo);
								struct mkgui_text_edit te = {pb->edit_buf, (uint32_t)sizeof(pb->edit_buf), pb->edit_cursor, pb->edit_sel_start, pb->edit_sel_end, pb->edit_scroll_x};
								textedit_delete_selection(&te);
								pb->edit_cursor = te.cursor;
								pb->edit_sel_start = te.sel_start;
								pb->edit_sel_end = te.sel_end;
								dirty_widget_id(win, win->focus_id);
								pathbar_scroll_to_cursor(win, pb);
							}

						} else if(fw->type == MKGUI_COMBOBOX) {
							struct mkgui_combobox_data *cb = find_combobox_data(win, win->focus_id);
							if(cb && combobox_has_selection(cb)) {
								uint32_t lo = cb->sel_start < cb->sel_end ? cb->sel_start : cb->sel_end;
								uint32_t hi = cb->sel_start < cb->sel_end ? cb->sel_end : cb->sel_start;
								platform_clipboard_set(win, cb->text + lo, hi - lo);
								combobox_delete_selection(cb);
								cb->selected = -1;
								{ size_t _l = strlen(cb->text); if(_l >= MKGUI_MAX_TEXT) { _l = MKGUI_MAX_TEXT - 1; } memcpy(cb->prev_text, cb->text, _l); cb->prev_text[_l] = '\0'; }
								combobox_open_popup(win, win->focus_id, 1);
								ev->type = MKGUI_EVENT_COMBOBOX_CHANGED;
								ev->id = win->focus_id;
								dirty_widget_id(win, win->focus_id);
								combobox_scroll_to_cursor(win, win->focus_id);
								return 1;
							}
						}
						break;
					}

					if(fw && (ks == 'v' || ks == 'V')) {
						if(fw->type == MKGUI_INPUT && !(fw->style & MKGUI_INPUT_READONLY)) {
							char clip_buf[MKGUI_CLIP_MAX];
							uint32_t clip_len = platform_clipboard_get(win, clip_buf, sizeof(clip_buf));
							if(clip_len > 0) {
								struct mkgui_input_data *inp = find_input_data(win, win->focus_id);
								if(inp) {
									input_undo_push_force(inp);
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
										dirty_widget_id(win, win->focus_id);
										input_scroll_to_cursor(win, win->focus_id);
										ev->type = MKGUI_EVENT_INPUT_CHANGED;
										ev->id = win->focus_id;
										return 1;
									}
								}
							}

						} else if(fw->type == MKGUI_TEXTAREA && !(fw->style & MKGUI_TEXTAREA_READONLY)) {
							uint32_t clip_len = 0;
							char *clip_buf = platform_clipboard_get_alloc(win, &clip_len);
							if(clip_buf && clip_len > 0) {
								struct mkgui_textarea_data *ta = find_textarea_data(win, win->focus_id);
								if(ta) {
									textarea_undo_push_force(ta);
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
									dirty_widget_id(win, win->focus_id);
									textarea_scroll_to_cursor(win, win->focus_id);
									ev->type = MKGUI_EVENT_TEXTAREA_CHANGED;
									ev->id = win->focus_id;
									free(clip_buf);
									return 1;
								}
							}
							free(clip_buf);

						} else if(fw->type == MKGUI_PATHBAR) {
							struct mkgui_pathbar_data *pb = find_pathbar_data(win, win->focus_id);
							if(pb && pb->editing) {
								char clip_buf[MKGUI_CLIP_MAX];
								uint32_t clip_len = platform_clipboard_get(win, clip_buf, sizeof(clip_buf));
								if(clip_len > 0) {
									for(uint32_t ci = 0; ci < clip_len; ++ci) {
										if(clip_buf[ci] == '\n' || clip_buf[ci] == '\r') {
											clip_buf[ci] = ' ';
										}
									}
									struct mkgui_text_edit te = {pb->edit_buf, (uint32_t)sizeof(pb->edit_buf), pb->edit_cursor, pb->edit_sel_start, pb->edit_sel_end, pb->edit_scroll_x};
									if(textedit_insert(&te, clip_buf, clip_len)) {
										pb->edit_cursor = te.cursor;
										pb->edit_sel_start = te.sel_start;
										pb->edit_sel_end = te.sel_end;
										dirty_widget_id(win, win->focus_id);
										pathbar_scroll_to_cursor(win, pb);
									}
								}
							}

						} else if(fw->type == MKGUI_COMBOBOX) {
							char clip_buf[MKGUI_CLIP_MAX];
							uint32_t clip_len = platform_clipboard_get(win, clip_buf, sizeof(clip_buf));
							if(clip_len > 0) {
								struct mkgui_combobox_data *cb = find_combobox_data(win, win->focus_id);
								if(cb) {
									for(uint32_t ci = 0; ci < clip_len; ++ci) {
										if(clip_buf[ci] == '\n' || clip_buf[ci] == '\r') {
											clip_buf[ci] = ' ';
										}
									}
									struct mkgui_text_edit te = {cb->text, MKGUI_MAX_TEXT, cb->cursor, cb->sel_start, cb->sel_end, cb->scroll_x};
									if(textedit_insert(&te, clip_buf, clip_len)) {
										cb->cursor = te.cursor;
										cb->sel_start = te.sel_start;
										cb->sel_end = te.sel_end;
										cb->selected = -1;
										{ size_t _l = strlen(cb->text); if(_l >= MKGUI_MAX_TEXT) { _l = MKGUI_MAX_TEXT - 1; } memcpy(cb->prev_text, cb->text, _l); cb->prev_text[_l] = '\0'; }
										combobox_open_popup(win, win->focus_id, 1);
										ev->type = MKGUI_EVENT_COMBOBOX_CHANGED;
										ev->id = win->focus_id;
										dirty_widget_id(win, win->focus_id);
										combobox_scroll_to_cursor(win, win->focus_id);
										return 1;
									}
								}
							}
						}
						break;
					}
				}

				if(win->focus_id) {
					struct mkgui_widget *fw = find_widget(win, win->focus_id);
					if(fw) {
						if(ks == MKGUI_KEY_F2 && (fw->type == MKGUI_TREEVIEW || fw->type == MKGUI_LISTVIEW || fw->type == MKGUI_GRIDVIEW || fw->type == MKGUI_ITEMVIEW)) {
							if(fw->type == MKGUI_TREEVIEW) {
								struct mkgui_treeview_data *tv = find_treeview_data(win, win->focus_id);
								if(tv && tv->selected_node >= 0) {
									for(uint32_t ni = 0; ni < tv->node_count; ++ni) {
										if((int32_t)tv->nodes[ni].id == tv->selected_node) {
											celledit_begin(win, win->focus_id, tv->selected_node, 0, tv->nodes[ni].label);
											break;
										}
									}
									return 0;
								}
							} else if(fw->type == MKGUI_LISTVIEW) {
								struct mkgui_listview_data *lv = find_listv_data(win, win->focus_id);
								if(lv && lv->selected_row >= 0) {
									char cell_buf[MKGUI_MAX_TEXT] = {0};
									if(lv->row_cb) {
										lv->row_cb((uint32_t)lv->selected_row, 0, cell_buf, MKGUI_MAX_TEXT, lv->userdata);
									}
									celledit_begin(win, win->focus_id, lv->selected_row, 0, cell_buf);
									return 0;
								}
							} else if(fw->type == MKGUI_GRIDVIEW) {
								struct mkgui_gridview_data *gv = find_gridv_data(win, win->focus_id);
								if(gv && gv->selected_row >= 0 && gv->selected_col >= 0) {
									char cell_buf[MKGUI_MAX_TEXT] = {0};
									if(gv->cell_cb) {
										gv->cell_cb((uint32_t)gv->selected_row, (uint32_t)gv->selected_col, cell_buf, MKGUI_MAX_TEXT, gv->userdata);
									}
									celledit_begin(win, win->focus_id, gv->selected_row, gv->selected_col, cell_buf);
									return 0;
								}
							} else if(fw->type == MKGUI_ITEMVIEW) {
								struct mkgui_itemview_data *iv = find_itemview_data(win, win->focus_id);
								if(iv && iv->selected >= 0) {
									char label[MKGUI_MAX_TEXT] = {0};
									if(iv->label_cb) {
										iv->label_cb((uint32_t)iv->selected, label, MKGUI_MAX_TEXT, iv->userdata);
									}
									celledit_begin(win, win->focus_id, iv->selected, 0, label);
									return 0;
								}
							}
						}
						switch(fw->type) {
							case MKGUI_INPUT: {
								if(handle_input_key(win, ev, ks, pev.keymod, pev.text, pev.text_len)) {
									return 1;
								}
							} break;

							case MKGUI_BUTTON: {
								if(handle_button_key(win, ev, ks)) {
									return 1;
								}
							} break;

							case MKGUI_CHECKBOX: {
								if(handle_checkbox_key(win, ev, ks)) {
									return 1;
								}
							} break;

							case MKGUI_DROPDOWN: {
								if(handle_dropdown_key(win, ev, ks)) {
									return 1;
								}
							} break;

							case MKGUI_SLIDER: {
								if(handle_slider_key(win, ev, ks)) {
									return 1;
								}
							} break;

							case MKGUI_LISTVIEW: {
								if(handle_listview_key(win, ev, ks)) {
									return 1;
								}
							} break;

							case MKGUI_GRIDVIEW: {
								if(handle_gridview_key(win, ev, ks)) {
									return 1;
								}
							} break;

							case MKGUI_RICHLIST: {
								if(handle_richlist_key(win, ev, ks)) {
									return 1;
								}
							} break;

							case MKGUI_TREEVIEW: {
								if(handle_treeview_key(win, ev, ks)) {
									return 1;
								}
							} break;

							case MKGUI_SPINBOX: {
								if(handle_spinbox_key(win, ev, ks, pev.text, pev.text_len)) {
									return 1;
								}
							} break;

							case MKGUI_RADIO: {
								if(handle_radio_key(win, ev, ks)) {
									return 1;
								}
							} break;

							case MKGUI_TEXTAREA: {
								if(handle_textarea_key(win, ev, ks, pev.keymod, pev.text, pev.text_len)) {
									return 1;
								}
							} break;

							case MKGUI_ITEMVIEW: {
								struct mkgui_itemview_data *iv = find_itemview_data(win, fw->id);
								if(iv) {
									int32_t fi = find_widget_idx(win, fw->id);
									if(fi >= 0 && handle_itemview_key(win, iv, (uint32_t)fi, ks, ev)) {
										return 1;
									}
								}
							} break;

							case MKGUI_TABS: {
								if(handle_tabs_key(win, ev, ks)) {
									return 1;
								}
							} break;

							case MKGUI_PATHBAR: {
								if(handle_pathbar_key(win, ev, ks, pev.keymod, pev.text, pev.text_len)) {
									return 1;
								}
							} break;

							case MKGUI_IPINPUT: {
								if(handle_ipinput_key(win, ev, ks, pev.keymod, pev.text, pev.text_len)) {
									return 1;
								}
							} break;

							case MKGUI_TOGGLE: {
								if(handle_toggle_key(win, ev, ks)) {
									return 1;
								}
							} break;

							case MKGUI_COMBOBOX: {
								if(handle_combobox_key(win, ev, ks, pev.keymod, pev.text, pev.text_len)) {
									return 1;
								}
							} break;

							case MKGUI_DATEPICKER: {
								if(handle_datepicker_key(win, ev, ks, pev.keymod, pev.text, pev.text_len)) {
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

			// -=[ CLOSE ]=-
			case MKGUI_PLAT_CLOSE: {
				ev->type = MKGUI_EVENT_CLOSE;
				if(win->hide_on_close) {
					if(win->window_visible) {
						platform_window_unmap(win);
						win->window_visible = 0;
					}
				} else {
					win->should_close = 1;
				}
				return 1;
			} break;

			// -=[ FOCUS OUT ]=-
			case MKGUI_PLAT_FOCUS_OUT: {
				if(win->popup_count > 0) {
					popup_destroy_all(win);
					dirty_all(win);
				}
			} break;

			// -=[ LEAVE ]=-
			case MKGUI_PLAT_LEAVE: {
				if(win->hover_id) {
					dirty_widget_id(win, win->hover_id);
					win->hover_id = 0;
				}
				tooltip_update(win, 0, 0, 0);
				if(popup_idx >= 0) {
					uint32_t has_child_popup = (uint32_t)popup_idx + 1 < win->popup_count;
					if(!has_child_popup) {
						win->popups[popup_idx].hover_item = -1;
						win->popups[popup_idx].dirty = 1;
					}
				}
				for(uint32_t pbi = 0; pbi < win->pathbar_count; ++pbi) {
					if(win->pathbars[pbi].hover_seg >= 0) {
						win->pathbars[pbi].hover_seg = -1;
						dirty_widget_id(win, win->pathbars[pbi].widget_id);
					}
				}
			} break;

			// -=[ DROP ]=-
			case MKGUI_PLAT_DROP: {
				if(win->drop_count > 0) {
					ev->type = MKGUI_EVENT_FILE_DROP;
					ev->id = 0;
					ev->value = (int32_t)win->drop_count;
					return 1;
				}
			} break;

			default: {
			} break;
		}
	}

	if(win->anim_active) {
		for(uint32_t i = 0; i < win->widget_count; ++i) {
			struct mkgui_widget *w = &win->widgets[i];
			if(!widget_visible(win, i)) {
				continue;
			}

			if(w->type == MKGUI_SPINNER || w->type == MKGUI_GLVIEW) {
				dirty_widget(win, i);
			}

			if(w->type == MKGUI_PROGRESS && (w->style & MKGUI_PROGRESS_SHIMMER)) {
				struct mkgui_progress_data *pd = find_progress_data(win, w->id);
				if(pd && pd->value > 0 && pd->value < pd->max_val) {
					dirty_widget(win, i);
				}
			}
		}
	}

	if(win->tooltip_id && !win->tooltip_shown) {
		uint32_t elapsed = (uint32_t)(mkgui_now_ns() / 1000000ull) - win->tooltip_start_ms;
		if(elapsed >= MKGUI_TOOLTIP_DELAY_MS) {
			dirty_all(win);
		}
	}

	return ev->type != MKGUI_EVENT_NONE;
}

// [=]===^=[ mkgui_flush ]========================================[=]
static void mkgui_flush(struct mkgui_window *win) {
	if(win->dirty) {
#ifndef _WIN32
		XSync(win->plat.dpy, False);
#endif
		double t0 = ((double)mkgui_now_ns() * 1e-3);
		layout_widgets(win);
		glview_sync_all(win);
		double t1 = ((double)mkgui_now_ns() * 1e-3);
		render_widgets(win);
		if(win->render_cb) {
			flush_text(win);
			win->render_cb(win, win->render_cb_data);
		}
		flush_text(win);
		render_notify(win);
		flush_text(win);
		render_tooltip(win);
		flush_text(win);
		double t2 = ((double)mkgui_now_ns() * 1e-3);

		if(win->dirty_full || win->render_cb) {
			platform_blit(win);
		} else {
			for(uint32_t d = 0; d < win->dirty_count; ++d) {
				platform_blit_region(win, win->dirty_rects[d].x, win->dirty_rects[d].y, win->dirty_rects[d].w, win->dirty_rects[d].h);
			}
		}
		double t3 = ((double)mkgui_now_ns() * 1e-3);
		win->perf_layout_us = t1 - t0;
		win->perf_render_us = t2 - t1;
		win->perf_blit_us = t3 - t2;

		for(uint32_t pi = 0; pi < win->popup_count; ++pi) {
			struct mkgui_popup *p = &win->popups[pi];
			if(!p->active || !p->dirty) {
				continue;
			}

			if(p->widget_id == MKGUI_CTXMENU_POPUP_ID) {
				render_ctxmenu_popup(win, p, p->hover_item);
				flush_text_popup(win, p);
				platform_popup_blit(win, p);

			} else {
				struct mkgui_widget *pw = find_widget(win, p->widget_id);
				if(pw && pw->type == MKGUI_DROPDOWN) {
					struct mkgui_dropdown_data *dd = find_dropdown_data(win, pw->id);
					if(dd) {
						render_dropdown_popup(win, p, dd, p->hover_item);
						flush_text_popup(win, p);
						platform_popup_blit(win, p);
					}

				} else if(pw && pw->type == MKGUI_COMBOBOX) {
					struct mkgui_combobox_data *cb = find_combobox_data(win, pw->id);
					if(cb) {
						render_combobox_popup(win, p, cb, p->hover_item);
						flush_text_popup(win, p);
						platform_popup_blit(win, p);
					}

				} else if(pw && pw->type == MKGUI_DATEPICKER) {
					struct mkgui_datepicker_data *dp = find_datepicker_data(win, pw->id);
					if(dp) {
						int32_t hover_day = dp->cal_hover > 0 ? dp->cal_hover : -1;
						render_datepicker_popup(win, p, dp, hover_day);
						flush_text_popup(win, p);
						platform_popup_blit(win, p);
					}

				} else if(pw && pw->type == MKGUI_MENUITEM) {
					render_menu_popup(win, p, pw->id, p->hover_item);
					flush_text_popup(win, p);
					platform_popup_blit(win, p);
				}
			}
			p->dirty = 0;
		}

		platform_flush(win);
		win->dirty = 0;
		win->dirty_full = 0;
		win->dirty_count = 0;
	}

	if(win->parent) {
		struct mkgui_window *p = win->parent;
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

			if(w->type == MKGUI_PROGRESS && (w->style & MKGUI_PROGRESS_SHIMMER)) {
				struct mkgui_progress_data *pd = find_progress_data(p, w->id);
				if(pd && pd->value > 0 && pd->value < pd->max_val) {
					p->anim_active = 1;
					dirty_widget(p, i);
				}
			}
		}

		if(p->anim_active && !win->anim_active) {
			win->anim_active = 1;
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
			render_notify(p);
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
}

// [=]===^=[ mkgui_ctx_wait_until ]===================================[=]
// Block until: an event arrives, an animation/tooltip/toast wakes up, OR
// the caller's deadline (absolute ns from mkgui_now_ns) is reached -
// whichever fires first. deadline_ns == 0 means no caller deadline; the
// library's own deadlines still apply.
MKGUI_API void mkgui_ctx_wait_until(struct mkgui_ctx *ctx, uint64_t deadline_ns) {
	MKGUI_CHECK(ctx);
	for(uint32_t i = 0; i < g_ctx->window_count; ++i) {
		mkgui_flush(g_ctx->windows[i]);
	}

	if(ctx->should_quit || ctx->window_count == 0) {
		return;
	}
	uint32_t any_anim = 0;
	for(uint32_t i = 0; i < g_ctx->window_count; ++i) {
		struct mkgui_window *c = g_ctx->windows[i];
		if(c->anim_active || c->dirty) {
			any_anim = 1;
			break;
		}
	}

	uint64_t now = mkgui_now_ns();
	uint64_t target = 0;
	if(any_anim) {
		target = now + 16000000ull;
	}
	if(deadline_ns) {
		if(!target || deadline_ns < target) {
			target = deadline_ns;
		}
	}
	for(uint32_t i = 0; i < g_ctx->window_count; ++i) {
		struct mkgui_window *c = g_ctx->windows[i];
		if(c->tooltip_id && !c->tooltip_shown) {
			uint32_t elapsed = (uint32_t)(mkgui_now_ns() / 1000000ull) - c->tooltip_start_ms;
			if(elapsed < MKGUI_TOOLTIP_DELAY_MS) {
				uint64_t tt = now + (uint64_t)(MKGUI_TOOLTIP_DELAY_MS - elapsed) * 1000000ull;
				if(!target || tt < target) {
					target = tt;
				}
			}
		}
		int32_t next_toast = toasts_next_expiry_ms(c);
		if(next_toast >= 0) {
			uint64_t tt = now + (uint64_t)(uint32_t)next_toast * 1000000ull;
			if(!target || tt < target) {
				target = tt;
			}
		}
	}

	int64_t timeout_ns;
	if(!target) {
		timeout_ns = -1;
	} else if(target <= now) {
		timeout_ns = 0;
	} else {
		timeout_ns = (int64_t)(target - now);
	}
	// platform_wait_event blocks on the shared X fd / Win32 queue. Any
	// window works as the anchor; use the primary.
	struct mkgui_window *anchor = ctx->primary ? ctx->primary : ctx->windows[0];
	platform_wait_event(anchor, timeout_ns);
}

// [=]===^=[ mkgui_ctx_wait ]=========================================[=]
MKGUI_API void mkgui_ctx_wait(struct mkgui_ctx *ctx) {
	mkgui_ctx_wait_until(ctx, 0);
}

// [=]===^=[ mkgui_ctx_should_quit ]==================================[=]
MKGUI_API uint32_t mkgui_ctx_should_quit(struct mkgui_ctx *ctx) {
	MKGUI_CHECK_VAL(ctx, 1);
	return ctx->should_quit;
}

// [=]===^=[ mkgui_window_add_timer ]=====================================[=]
MKGUI_API uint32_t mkgui_window_add_timer(struct mkgui_window *win, uint64_t interval_ns, mkgui_timer_cb cb, void *userdata) {
	MKGUI_CHECK_VAL(win, 0);
	if(!cb) {
		return 0;
	}

	if(win->timer_count >= MKGUI_MAX_TIMERS) {
		return 0;
	}
	struct mkgui_timer *t = &win->timers[win->timer_count];
	memset(t, 0, sizeof(*t));
	t->id = ++win->timer_next_id;
	t->interval_ns = interval_ns;
	t->cb = cb;
	t->userdata = userdata;
	t->active = 1;
#ifdef _WIN32
	t->handle = CreateWaitableTimerW(NULL, FALSE, NULL);
	if(!t->handle) {
		return 0;
	}
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	t->epoch.LowPart = ft.dwLowDateTime;
	t->epoch.HighPart = (LONG)ft.dwHighDateTime;
	t->fire_count = 0;
	LARGE_INTEGER due;
	due.QuadPart = -(int64_t)(interval_ns / 100);
	SetWaitableTimer(t->handle, &due, 0, NULL, NULL, FALSE);
#else
	t->fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	if(t->fd < 0) {
		return 0;
	}
	struct itimerspec its = {0};
	its.it_value.tv_sec = (time_t)(interval_ns / 1000000000);
	its.it_value.tv_nsec = (long)(interval_ns % 1000000000);
	its.it_interval = its.it_value;
	timerfd_settime(t->fd, 0, &its, NULL);
#endif
	++win->timer_count;
	return t->id;
}

// [=]===^=[ mkgui_window_remove_timer ]==================================[=]
MKGUI_API void mkgui_window_remove_timer(struct mkgui_window *win, uint32_t timer_id) {
	MKGUI_CHECK(win);
	for(uint32_t i = 0; i < win->timer_count; ++i) {
		if(win->timers[i].id == timer_id) {
#ifdef _WIN32
			if(win->timers[i].handle) {
				CancelWaitableTimer(win->timers[i].handle);
				CloseHandle(win->timers[i].handle);
			}
#else
			if(win->timers[i].fd >= 0) {
				close(win->timers[i].fd);
			}
#endif
			win->timers[i] = win->timers[--win->timer_count];
			return;
		}
	}
}

// [=]===^=[ mkgui_window_set_callback ]=================================[=]
MKGUI_API void mkgui_window_set_callback(struct mkgui_window *win, mkgui_event_cb cb, void *userdata) {
	MKGUI_CHECK(win);
	win->event_cb = cb;
	win->event_cb_data = userdata;
}

// [=]===^=[ mkgui_window_get_ctx ]=================================[=]
MKGUI_API struct mkgui_ctx *mkgui_window_get_ctx(struct mkgui_window *win) {
	MKGUI_CHECK_VAL(win, NULL);
	return win->ctx;
}

// [=]===^=[ mkgui_ctx_run ]===========================================[=]
// Run the event loop until: mkgui_ctx_quit is called, the ctx's primary
// window has should_close set, OR no registered window is currently
// visible. cb is invoked for the primary window's events; other windows
// dispatch through their own per-window callbacks (mkgui_window_set_callback).
MKGUI_API void mkgui_ctx_run(struct mkgui_ctx *ctx, mkgui_event_cb cb, void *userdata) {
	MKGUI_CHECK(ctx);
	struct mkgui_window *primary = ctx->primary;
	for(;;) {
		uint32_t any_visible = 0;
		for(uint32_t i = 0; i < g_ctx->window_count; ++i) {
			if(g_ctx->windows[i]->window_visible) {
				any_visible = 1;
				break;
			}
		}
		if(ctx->should_quit || (primary && primary->should_close) || !any_visible) {
			break;
		}

		struct mkgui_event ev;
		if(primary) {
			while(mkgui_window_poll(primary, &ev)) {
				if(cb) {
					cb(primary, &ev, userdata);
				}
			}
		}
		for(uint32_t i = 0; i < g_ctx->window_count; ++i) {
			struct mkgui_window *c = g_ctx->windows[i];
			if(c == primary || c->should_close || !c->event_cb) {
				continue;
			}
			while(mkgui_window_poll(c, &ev)) {
				c->event_cb(c, &ev, c->event_cb_data);
			}
		}

		any_visible = 0;
		for(uint32_t i = 0; i < g_ctx->window_count; ++i) {
			if(g_ctx->windows[i]->window_visible) {
				any_visible = 1;
				break;
			}
		}
		if(ctx->should_quit || (primary && primary->should_close) || !any_visible) {
			break;
		}
		mkgui_ctx_wait(ctx);
	}
}

// [=]===^=[ mkgui_ctx_quit ]==========================================[=]
MKGUI_API void mkgui_ctx_quit(struct mkgui_ctx *ctx) {
	MKGUI_CHECK(ctx);
	ctx->should_quit = 1;
}

// [=]===^=[ mkgui_ctx_set_quit ]======================================[=]
MKGUI_API void mkgui_ctx_set_quit(struct mkgui_ctx *ctx, uint32_t value) {
	MKGUI_CHECK(ctx);
	ctx->should_quit = value ? 1 : 0;
}

// [=]===^=[ mkgui_ctx_pump_others ]==================================[=]
// Drain queued events from every window on this ctx other than the primary,
// dispatching each through the window's registered callback. Use this in a
// host-owned loop after pumping the primary window. Windows without a
// callback (mkgui_window_set_callback) are skipped silently.
MKGUI_API void mkgui_ctx_pump_others(struct mkgui_ctx *ctx) {
	MKGUI_CHECK(ctx);
	struct mkgui_event ev;
	for(uint32_t i = 0; i < ctx->window_count; ++i) {
		struct mkgui_window *c = ctx->windows[i];
		if(c == ctx->primary || c->should_close || !c->event_cb) {
			continue;
		}
		while(mkgui_window_poll(c, &ev)) {
			c->event_cb(c, &ev, c->event_cb_data);
		}
	}
}

// [=]===^=[ mkgui_window_set_close ]==================================[=]
MKGUI_API void mkgui_window_set_close(struct mkgui_window *win, uint32_t value) {
	MKGUI_CHECK(win);
	win->should_close = value ? 1 : 0;
}

// [=]===^=[ mkgui_window_should_close ]===============================[=]
MKGUI_API uint32_t mkgui_window_should_close(struct mkgui_window *win) {
	MKGUI_CHECK_VAL(win, 1);
	return win->should_close;
}

// [=]===^=[ mkgui_window_show ]===================================[=]
MKGUI_API void mkgui_window_show(struct mkgui_window *win) {
	MKGUI_CHECK(win);
	if(win->window_visible) {
		return;
	}
	platform_window_map(win);
	win->window_visible = 1;
	dirty_all(win);
}

// [=]===^=[ mkgui_window_hide ]===================================[=]
MKGUI_API void mkgui_window_hide(struct mkgui_window *win) {
	MKGUI_CHECK(win);
	if(!win->window_visible) {
		return;
	}
	platform_window_unmap(win);
	win->window_visible = 0;
}

// [=]===^=[ mkgui_window_is_visible ]=============================[=]
MKGUI_API uint32_t mkgui_window_is_visible(struct mkgui_window *win) {
	MKGUI_CHECK_VAL(win, 0);
	return win->window_visible;
}

// [=]===^=[ mkgui_window_move ]====================================[=]
MKGUI_API void mkgui_window_move(struct mkgui_window *win, int32_t x, int32_t y) {
	MKGUI_CHECK(win);
	platform_move_window(win, x, y);
}

// [=]===^=[ mkgui_window_get_position ]============================[=]
MKGUI_API void mkgui_window_get_position(struct mkgui_window *win, int32_t *x, int32_t *y) {
	MKGUI_CHECK(win);
	MKGUI_CHECK(x);
	MKGUI_CHECK(y);
	platform_get_window_position(win, x, y);
}

// [=]===^=[ mkgui_window_begin_drag ]==============================[=]
MKGUI_API void mkgui_window_begin_drag(struct mkgui_window *win) {
	MKGUI_CHECK(win);
	platform_begin_drag(win);
}

// [=]===^=[ mkgui_window_set_shape ]===============================[=]
MKGUI_API void mkgui_window_set_shape(struct mkgui_window *win, const int32_t *xy_pairs, uint32_t point_count) {
	MKGUI_CHECK(win);
	MKGUI_CHECK(xy_pairs);
	if(point_count < 3) {
		return;
	}
#ifdef _WIN32
	POINT *pts = (POINT *)malloc(point_count * sizeof(POINT));
	if(!pts) {
		return;
	}
	for(uint32_t i = 0; i < point_count; ++i) {
		pts[i].x = xy_pairs[i * 2];
		pts[i].y = xy_pairs[i * 2 + 1];
	}
	platform_set_shape(win, pts, (int32_t)point_count);
	free(pts);
#else
	XPoint *pts = (XPoint *)malloc(point_count * sizeof(XPoint));
	if(!pts) {
		return;
	}
	for(uint32_t i = 0; i < point_count; ++i) {
		pts[i].x = (short)xy_pairs[i * 2];
		pts[i].y = (short)xy_pairs[i * 2 + 1];
	}
	platform_set_shape(win, pts, (int32_t)point_count);
	free(pts);
#endif
}

// [=]===^=[ mkgui_window_set_shape_mask ]=========================[=]
MKGUI_API void mkgui_window_set_shape_mask(struct mkgui_window *win, const uint32_t *argb, int32_t w, int32_t h, uint32_t alpha_threshold) {
	MKGUI_CHECK(win);
	MKGUI_CHECK(argb);
	if(w <= 0 || h <= 0) {
		return;
	}
#ifdef _WIN32
	HRGN combined = CreateRectRgn(0, 0, 0, 0);
	for(int32_t y = 0; y < h; ++y) {
		int32_t x = 0;
		while(x < w) {
			while(x < w && (argb[y * w + x] >> 24) < alpha_threshold) {
				++x;
			}
			int32_t x0 = x;
			while(x < w && (argb[y * w + x] >> 24) >= alpha_threshold) {
				++x;
			}
			if(x > x0) {
				HRGN span = CreateRectRgn(x0, y, x, y + 1);
				CombineRgn(combined, combined, span, RGN_OR);
				DeleteObject(span);
			}
		}
	}
	SetWindowRgn(win->plat.hwnd, combined, TRUE);
#else
	Pixmap mask = XCreatePixmap(win->plat.dpy, win->plat.root, (uint32_t)w, (uint32_t)h, 1);
	GC gc = XCreateGC(win->plat.dpy, mask, 0, NULL);
	XSetForeground(win->plat.dpy, gc, 0);
	XFillRectangle(win->plat.dpy, mask, gc, 0, 0, (uint32_t)w, (uint32_t)h);
	XSetForeground(win->plat.dpy, gc, 1);
	for(int32_t y = 0; y < h; ++y) {
		int32_t x = 0;
		while(x < w) {
			while(x < w && (argb[y * w + x] >> 24) < alpha_threshold) {
				++x;
			}
			int32_t x0 = x;
			while(x < w && (argb[y * w + x] >> 24) >= alpha_threshold) {
				++x;
			}
			if(x > x0) {
				XFillRectangle(win->plat.dpy, mask, gc, x0, y, (uint32_t)(x - x0), 1);
			}
		}
	}
	XShapeCombineMask(win->plat.dpy, win->plat.win, ShapeBounding, 0, 0, mask, ShapeSet);
	XFreeGC(win->plat.dpy, gc);
	XFreePixmap(win->plat.dpy, mask);
	XFlush(win->plat.dpy);
#endif
}

// [=]===^=[ mkgui_window_clear_shape ]=============================[=]
MKGUI_API void mkgui_window_clear_shape(struct mkgui_window *win) {
	MKGUI_CHECK(win);
	platform_clear_shape(win);
}

// [=]===^=[ mkgui_window_resize ]==================================[=]
MKGUI_API void mkgui_window_resize(struct mkgui_window *win, int32_t w, int32_t h) {
	MKGUI_CHECK(win);
	if(w <= 0 || h <= 0) {
		return;
	}
	platform_resize_window(win, w, h);
}

// [=]===^=[ mkgui_window_invalidate ]=====================================[=]
MKGUI_API void mkgui_window_invalidate(struct mkgui_window *win) {
	MKGUI_CHECK(win);
	dirty_all(win);
}

// ---------------------------------------------------------------------------
// Dialog windows
// ---------------------------------------------------------------------------

#include "mkgui_dialogs.c"
#include "mkgui_filedialog.c"
#include "mkgui_iconbrowser.c"
#include "mkgui_colorpicker.c"
