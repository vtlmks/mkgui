// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>

#if defined(__SSE2__)
#include <emmintrin.h>
#endif
#if defined(__SSE4_1__)
#include <smmintrin.h>
#endif

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
MKGUI_API void mkgui_sleep_ms(uint32_t ms) {
#ifdef _WIN32
	Sleep(ms);
#else
	struct timespec ts = { 0, (long)ms * 1000000L };
	nanosleep(&ts, NULL);
#endif
}

// [=]===^=[ mkgui_time_ms ]=======================================[=]
MKGUI_API uint32_t mkgui_time_ms(void) {
#ifdef _WIN32
	return (uint32_t)GetTickCount();
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#endif
}

// [=]===^=[ mkgui_time_us ]=======================================[=]
MKGUI_API double mkgui_time_us(void) {
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

struct mkgui_input_data {
	uint32_t widget_id;
	char text[MKGUI_MAX_TEXT];
	uint32_t cursor;
	uint32_t sel_start;
	uint32_t sel_end;
	int32_t scroll_x;
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

// ---------------------------------------------------------------------------
// Icon system
// ---------------------------------------------------------------------------

struct mkgui_icon {
	char name[MKGUI_ICON_NAME_LEN];
	uint32_t *pixels;
	int32_t w, h;
	uint32_t custom;
};

static struct mkgui_icon icons[MKGUI_MAX_ICONS];
static uint32_t icon_count;

#define MKGUI_ICON_HASH_SIZE 16384
#define MKGUI_ICON_HASH_MASK (MKGUI_ICON_HASH_SIZE - 1)

static uint32_t icon_hash[MKGUI_ICON_HASH_SIZE];

// [=]===^=[ icon_hash_fn ]===========================================[=]
static uint32_t icon_hash_fn(char *name) {
	uint32_t h = 2166136261u;
	for(char *p = name; *p; ++p) {
		h ^= (uint32_t)(uint8_t)*p;
		h *= 16777619u;
	}
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
	uint32_t h = icon_hash_fn(icons[idx].name);
	while(icon_hash[h] != UINT32_MAX) {
		h = (h + 1) & MKGUI_ICON_HASH_MASK;
	}
	icon_hash[h] = idx;
}

// [=]===^=[ icon_hash_lookup ]=======================================[=]
static int32_t icon_hash_lookup(char *name) {
	uint32_t h = icon_hash_fn(name);
	for(;;) {
		uint32_t idx = icon_hash[h];
		if(idx == UINT32_MAX) {
			return -1;
		}
		if(strcmp(icons[idx].name, name) == 0) {
			return (int32_t)idx;
		}
		h = (h + 1) & MKGUI_ICON_HASH_MASK;
	}
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

struct mkgui_ctx {
	struct mkgui_platform plat;

	uint32_t *pixels;
	int32_t win_w, win_h;

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
	uint32_t close_requested;

	float scale;
	int32_t row_height;
	int32_t tab_height;
	int32_t menu_height;
	int32_t scrollbar_w;
	int32_t split_thick;
	int32_t split_min_px;
	int32_t box_gap;
	int32_t box_pad;
	int32_t ui_margin;
	int32_t icon_size;
	int32_t toolbar_icon_size;
	int32_t dialog_icon_size;
	int32_t pathbar_height;
	int32_t toolbar_height;
	int32_t toolbar_btn_w;
	int32_t toolbar_sep_w;
	int32_t statusbar_height;

	struct mkgui_timer timers[MKGUI_MAX_TIMERS];
	uint32_t timer_count;
	uint32_t timer_next_id;

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

	struct mkgui_ctxmenu_item ctxmenu_items[MKGUI_MAX_CTXMENU];
	uint32_t ctxmenu_count;
	int32_t ctxmenu_x, ctxmenu_y;

	char app_class[64];
};

// ---------------------------------------------------------------------------
// Virtual memory arena (reserve-commit, no realloc)
// ---------------------------------------------------------------------------

#define MKGUI_VM_MAX_TEXT_CMDS   32768
#define MKGUI_VM_MAX_WIDGETS     65536
#define MKGUI_VM_MAX_HASH_SLOTS  (MKGUI_VM_MAX_WIDGETS * 2)

struct vm_arena {
	uint8_t *base;
	size_t reserved;
	size_t committed;
};

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

#define MKGUI_AUX_GROW(arr, count, cap, type) do { \
	if((count) >= (cap)) { \
		uint32_t _nc = (cap) + MKGUI_GROW_AUX; \
		type *_tmp = (type *)realloc((arr), (size_t)_nc * sizeof(type)); \
		if(_tmp) { \
			memset(&_tmp[(cap)], 0, (size_t)MKGUI_GROW_AUX * sizeof(type)); \
			(arr) = _tmp; \
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
// Public API validation macros
// ---------------------------------------------------------------------------

#define MKGUI_CHECK(ctx) do { if(!(ctx)) { return; } } while(0)
#define MKGUI_CHECK_VAL(ctx, val) do { if(!(ctx)) { return (val); } } while(0)

// ---------------------------------------------------------------------------
// Scale helpers
// ---------------------------------------------------------------------------

static inline int32_t sc(struct mkgui_ctx *ctx, int32_t px) {
	return (int32_t)(px * ctx->scale + 0.5f);
}

// [=]===^=[ mkgui_recompute_metrics ]==============================[=]
static void mkgui_recompute_metrics(struct mkgui_ctx *ctx) {
	ctx->row_height      = sc(ctx, MKGUI_ROW_HEIGHT);
	ctx->tab_height      = sc(ctx, MKGUI_TAB_HEIGHT);
	ctx->menu_height     = sc(ctx, MKGUI_MENU_HEIGHT);
	ctx->scrollbar_w     = sc(ctx, MKGUI_SCROLLBAR_W);
	ctx->split_thick     = sc(ctx, MKGUI_SPLIT_THICK);
	ctx->split_min_px    = sc(ctx, MKGUI_SPLIT_MIN_PX);
	ctx->box_gap         = sc(ctx, MKGUI_BOX_GAP);
	ctx->box_pad         = sc(ctx, MKGUI_BOX_PAD);
	ctx->ui_margin       = sc(ctx, MKGUI_MARGIN);
	ctx->icon_size           = sc(ctx, MKGUI_ICON_SIZE);
	ctx->toolbar_icon_size   = sc(ctx, 22);
	ctx->dialog_icon_size    = sc(ctx, 32);
	ctx->pathbar_height  = sc(ctx, MKGUI_PATHBAR_HEIGHT);
	ctx->toolbar_height  = sc(ctx, MKGUI_TOOLBAR_HEIGHT_DEFAULT);
	ctx->toolbar_btn_w   = sc(ctx, MKGUI_TOOLBAR_BTN_W);
	ctx->toolbar_sep_w   = sc(ctx, MKGUI_TOOLBAR_SEP_W);
	ctx->statusbar_height = sc(ctx, MKGUI_STATUSBAR_HEIGHT);
}

#include "mkgui_render.c"

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

#define MKGUI_FIND_AUX(name, type, arr, cnt) \
static struct type *name(struct mkgui_ctx *ctx, uint32_t widget_id) { \
	for(uint32_t i = 0; i < ctx->cnt; ++i) { \
		if(ctx->arr[i].widget_id == widget_id) { \
			return &ctx->arr[i]; \
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
static uint32_t widget_visible(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	if(w->flags & MKGUI_HIDDEN) {
		return 0;
	}
	if(w->type == MKGUI_WINDOW) {
		return 1;
	}
	if(layout_arr_cap > 0 && idx < layout_arr_cap) {
		uint32_t pidx = layout_parent[idx];
		while(pidx < ctx->widget_count) {
			struct mkgui_widget *parent = &ctx->widgets[pidx];
			if(parent->flags & MKGUI_HIDDEN) {
				return 0;
			}
			if(parent->type == MKGUI_TAB) {
				uint32_t tabs_idx = layout_parent[pidx];
				if(tabs_idx < ctx->widget_count && ctx->widgets[tabs_idx].type == MKGUI_TABS) {
					struct mkgui_tabs_data *td = find_tabs_data(ctx, ctx->widgets[tabs_idx].id);
					if(td && td->active_tab != parent->id) {
						return 0;
					}
				}
			}
			if(parent->type == MKGUI_GROUP && (parent->style & MKGUI_COLLAPSED)) {
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
			if(parent->type == MKGUI_GROUP && (parent->style & MKGUI_COLLAPSED)) {
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
static inline int32_t lw_sw(struct mkgui_ctx *ctx, int32_t val) {
	return val > 0 ? sc(ctx, val) : val;
}

// [=]===^=[ lc_find_idx ]===========================================[=]
static uint32_t lc_find_idx(struct layout_ctx *lc, uint32_t id) {
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
static void layout_build_index(struct mkgui_ctx *ctx) {
	if(ctx->widget_count > layout_arr_cap) {
		uint32_t nc = (ctx->widget_count + MKGUI_GROW_WIDGETS - 1) & ~(uint32_t)(MKGUI_GROW_WIDGETS - 1);
		if(nc > MKGUI_VM_MAX_WIDGETS) {
			return;
		}
		size_t needed = (size_t)nc * sizeof(uint32_t);
		if(!vm_arena_ensure(&layout_parent_arena, needed) ||
		   !vm_arena_ensure(&layout_child_arena, needed) ||
		   !vm_arena_ensure(&layout_sibling_arena, needed)) {
			return;
		}
		layout_arr_cap = nc;
	}
	uint32_t need = ctx->widget_count * 2;
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
		.widgets = ctx->widgets, .rects = (void *)ctx->rects,
		.first_child = layout_first_child, .next_sibling = layout_next_sibling,
		.parent = layout_parent, .id_map = layout_id_map, .hash_mask = layout_hash_mask,
		.widget_count = ctx->widget_count, .widget_stride = sizeof(struct mkgui_widget)
	};
	lc_build_index(&lc);
}


// [=]===^=[ natural_height ]=======================================[=]
static int32_t natural_height(struct mkgui_ctx *ctx, uint32_t widget_type) {
	switch(widget_type) {
		case MKGUI_BUTTON: {
			return ctx->font_height + sc(ctx, 12);
		}

		case MKGUI_INPUT:
		case MKGUI_COMBOBOX:
		case MKGUI_DROPDOWN:
		case MKGUI_SPINBOX:
		case MKGUI_DATEPICKER:
		case MKGUI_IPINPUT:
		case MKGUI_PATHBAR: {
			return ctx->font_height + sc(ctx, 10);
		}

		case MKGUI_CHECKBOX:
		case MKGUI_RADIO:
		case MKGUI_TOGGLE: {
			return ctx->font_height + sc(ctx, 6);
		}

		case MKGUI_LABEL: {
			return ctx->font_height + sc(ctx, 4);
		}

		case MKGUI_SLIDER:
		case MKGUI_SCROLLBAR: {
			return sc(ctx, 24);
		}

		case MKGUI_PROGRESS:
		case MKGUI_METER: {
			return sc(ctx, 20);
		}

		case MKGUI_DIVIDER: {
			return sc(ctx, 6);
		}

		case MKGUI_LISTVIEW:
		case MKGUI_GRIDVIEW:
		case MKGUI_RICHLIST:
		case MKGUI_TREEVIEW:
		case MKGUI_ITEMVIEW:
		case MKGUI_TEXTAREA: {
			return ctx->row_height * 3;
		}

		case MKGUI_TABS: {
			return ctx->tab_height + ctx->row_height * 2;
		}

		case MKGUI_HSPLIT:
		case MKGUI_VSPLIT: {
			return ctx->split_min_px * 2 + ctx->split_thick;
		}

		case MKGUI_IMAGE:
		case MKGUI_CANVAS:
		case MKGUI_GLVIEW: {
			return sc(ctx, 24);
		}

		case MKGUI_VBOX:
		case MKGUI_HBOX:
		case MKGUI_FORM:
		case MKGUI_PANEL:
		case MKGUI_GROUP: {
			return ctx->row_height;
		}

		default: {
			return 0;
		}
	}
}

// [=]===^=[ natural_width ]=======================================[=]
static int32_t natural_width(struct mkgui_ctx *ctx, uint32_t widget_type) {
	switch(widget_type) {
		case MKGUI_CHECKBOX:
		case MKGUI_RADIO:
		case MKGUI_TOGGLE: {
			return sc(ctx, 24);
		}

		case MKGUI_DIVIDER: {
			return sc(ctx, 4);
		}

		default: {
			return sc(ctx, 40);
		}
	}
}

// [=]===^=[ lc_measure_container ]================================[=]
static int32_t lc_measure_container(struct mkgui_ctx *ctx, struct layout_ctx *lc, uint32_t idx, uint32_t axis) {
	struct mkgui_widget *w = lw_get(lc, idx);
	if(w->type == MKGUI_GROUP) {
		int32_t gtop = ctx->font_height + 4;
		if(w->style & MKGUI_COLLAPSED) {
			return (axis == 1) ? gtop : 0;
		}
		int32_t gpad = 6;
		for(uint32_t j = lc->first_child[idx]; j < lc->widget_count; j = lc->next_sibling[j]) {
			uint32_t ct = lw_get(lc, j)->type;
			if(ct == MKGUI_VBOX || ct == MKGUI_HBOX || ct == MKGUI_FORM || ct == MKGUI_PANEL) {
				int32_t inner = lc_measure_container(ctx, lc, j, axis);
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
		int32_t row_h = ctx->font_height + 10;
		int32_t h = (int32_t)rows * row_h + (rows > 1 ? (int32_t)(rows - 1) * ctx->box_gap : 0);
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
			child_main = lw_sw(ctx, jw->h);
			child_cross = lw_sw(ctx, jw->w);
		} else {
			child_main = lw_sw(ctx, jw->w);
			child_cross = lw_sw(ctx, jw->h);
		}
		if(child_main == 0 && (jw->flags & MKGUI_FIXED) && (ct == MKGUI_VBOX || ct == MKGUI_HBOX || ct == MKGUI_FORM || ct == MKGUI_GROUP || ct == MKGUI_PANEL)) {
			child_main = lc_measure_container(ctx, lc, j, is_vbox ? 1 : 0);
		}
		if(child_main == 0) {
			child_main = natural_height(ctx, ct);
		}
		if(child_cross == 0) {
			int32_t nh = natural_height(ctx, ct);
			if(nh > 0) {
				child_cross = nh;
			}
		}
		main_total += child_main;
		if(child_cross > cross_max) {
			cross_max = child_cross;
		}
	}
	int32_t gap_total = visible > 1 ? (int32_t)(visible - 1) * ctx->box_gap : 0;
	main_total += gap_total;

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
		if(!nested || (w->style & MKGUI_PANEL_BORDER)) {
			has_pad = 1;
		}
	}
	if(has_pad) {
		main_total += ctx->box_pad * 2;
		cross_max += ctx->box_pad * 2;
	}

	if(axis == 0) {
		return is_vbox ? cross_max : main_total;
	}
	return is_vbox ? main_total : cross_max;
}

// [=]===^=[ measure_container ]===================================[=]
static int32_t measure_container(struct mkgui_ctx *ctx, uint32_t idx, uint32_t axis) {
	struct layout_ctx lc = {
		.widgets = ctx->widgets, .rects = (void *)ctx->rects,
		.first_child = layout_first_child, .next_sibling = layout_next_sibling,
		.parent = layout_parent, .widget_count = ctx->widget_count,
		.has_scroll = 1, .widget_stride = sizeof(struct mkgui_widget)
	};
	return lc_measure_container(ctx, &lc, idx, axis);
}

// [=]===^=[ lc_layout_node ]=====================================[=]
static void lc_layout_node(struct mkgui_ctx *ctx, struct layout_ctx *lc, uint32_t idx) {
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
			if(ct == MKGUI_MENU || ct == MKGUI_TOOLBAR || ct == MKGUI_STATUSBAR || (ct == MKGUI_PATHBAR && lw_sw(ctx, cw->h) == 0)) {
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
				lc->rects[c].h = ctx->menu_height;
				top_y += ctx->menu_height;
			}
		}
		for(uint32_t i = 0; i < chrome_count; ++i) {
			uint32_t c = chrome[i];
			struct mkgui_widget *cw = lw_get(lc, c);
			if(cw->type == MKGUI_TOOLBAR) {
				uint32_t tb_mode = cw->style & MKGUI_TOOLBAR_MODE_MASK;
				int32_t th;
				if(tb_mode == MKGUI_TOOLBAR_TEXT_ONLY) {
					th = ctx->font_height + 10;
				} else {
					uint32_t tb_has_icons = 0;
					for(uint32_t j = lc->first_child[c]; j < lc->widget_count; j = lc->next_sibling[j]) {
						if(lw_get(lc, j)->icon[0]) {
							tb_has_icons = 1;
							break;
						}
					}
					if(tb_has_icons && ctx->toolbar_icon_size > 0) {
						int32_t ih = ctx->toolbar_icon_size;
						th = (tb_mode == MKGUI_TOOLBAR_ICONS_ONLY) ? ih + 10 : ((ih > ctx->font_height ? ih : ctx->font_height) + 10);
					} else {
						th = ctx->font_height + 10;
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
				lc->rects[c].x = px + lw_sw(ctx, cw->margin_l);
				lc->rects[c].y = top_y;
				lc->rects[c].w = pw - lw_sw(ctx, cw->margin_l) - lw_sw(ctx, cw->margin_r);
				lc->rects[c].h = ctx->pathbar_height;
				top_y += ctx->pathbar_height;
			}
		}
		for(uint32_t i = 0; i < chrome_count; ++i) {
			uint32_t c = chrome[i];
			if(lw_get(lc, c)->type == MKGUI_STATUSBAR) {
				bot_y -= ctx->statusbar_height;
				lc->rects[c].x = px;
				lc->rects[c].y = bot_y;
				lc->rects[c].w = pw;
				lc->rects[c].h = ctx->statusbar_height;
			}
		}
		py = top_y;
		ph = bot_y - top_y;
	}

	if(w->type == MKGUI_TAB) {
		px += 2;
		py += ctx->tab_height + 2;
		pw -= 4;
		ph -= ctx->tab_height + 4;
	}
	if(w->type == MKGUI_GROUP) {
		if(w->style & MKGUI_COLLAPSED) {
			return;
		}
		int32_t gtop = ctx->font_height + 4;
		int32_t gpad = 6;
		px += gpad;
		py += gtop;
		pw -= gpad * 2;
		ph -= gtop + gpad;
	}

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
			if(!nested || (w->style & MKGUI_PANEL_BORDER)) {
				px += ctx->box_pad;
				py += ctx->box_pad;
				pw -= ctx->box_pad * 2;
				ph -= ctx->box_pad * 2;
				box_has_pad = 1;
			}
		}

		if(w->type == MKGUI_VBOX || is_panel_vbox) {
			uint32_t scrollable = lc->has_scroll && (w->flags & MKGUI_SCROLL) ? 1 : 0;
			struct mkgui_box_scroll *bs = scrollable ? find_box_scroll(ctx, w->id) : NULL;
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
				uint32_t treat_fixed = (jw->flags & MKGUI_FIXED) || (jw->type == MKGUI_GROUP && (jw->style & MKGUI_COLLAPSIBLE));
				if(treat_fixed) {
					int32_t fh = lw_sw(ctx, jw->h);
					if(jw->type == MKGUI_GROUP && (jw->style & MKGUI_COLLAPSED)) {
						fh = ctx->font_height + 4;
					}
					uint32_t ct = jw->type;
					if(fh == 0 && (ct == MKGUI_VBOX || ct == MKGUI_HBOX || ct == MKGUI_FORM || ct == MKGUI_GROUP || ct == MKGUI_PANEL)) {
						fh = lc_measure_container(ctx, lc, j, 1);
					}
					if(fh == 0) {
						fh = natural_height(ctx, ct);
					}
					lc->rects[j].h = fh;
					fixed_total += fh;
				} else {
					uint32_t ew = jw->weight > 0 ? jw->weight : 1;
					int32_t minh = lw_sw(ctx, jw->h);
					if(minh == 0) {
						minh = natural_height(ctx, jw->type);
					}
					min_total += minh;
					weight_total += ew;
				}
			}
			int32_t gap_total = child_count > 1 ? (int32_t)(child_count - 1) * ctx->box_gap : 0;
			int32_t content_h = fixed_total + min_total + gap_total;
			uint32_t needs_scroll = scrollable && (content_h > ph);
			if(needs_scroll) {
				pw -= ctx->scrollbar_w;
			}
			if(bs) {
				bs->content_h = content_h;
				if(box_has_pad) {
					bs->content_h += ctx->box_pad * 2;
				}
			}
			int32_t remaining = ph - fixed_total - min_total - gap_total;
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
				if((jw->flags & MKGUI_FIXED) || (jw->type == MKGUI_GROUP && (jw->style & MKGUI_COLLAPSIBLE))) {
					ch = lc->rects[j].h;
				} else {
					uint32_t wt = jw->weight > 0 ? jw->weight : 1;
					int32_t base_h = lw_sw(ctx, jw->h);
					if(base_h == 0) {
						base_h = natural_height(ctx, jw->type);
					}
					ch = base_h + (weight_total > 0 ? (int32_t)((int64_t)remaining * (int32_t)wt / (int32_t)weight_total) : 0);
				}
				if(!((jw->flags & MKGUI_FIXED) || (jw->type == MKGUI_GROUP && (jw->style & MKGUI_COLLAPSIBLE)))) {
					int32_t min_ch = natural_height(ctx, jw->type);
					if(min_ch < 1) { min_ch = 1; }
					if(ch < min_ch) { ch = min_ch; }
				}
				int32_t cx_child = px;
				int32_t cw_child = pw;
				uint32_t align = jw->flags & MKGUI_ALIGN_MASK;
				if(align && lw_sw(ctx, jw->w) > 0) {
					if(align == MKGUI_ALIGN_START) {
						cw_child = lw_sw(ctx, jw->w);
					} else if(align == MKGUI_ALIGN_CENTER) {
						cw_child = lw_sw(ctx, jw->w);
						cx_child = px + (pw - cw_child) / 2;
					} else if(align == MKGUI_ALIGN_END) {
						cw_child = lw_sw(ctx, jw->w);
						cx_child = px + pw - cw_child;
					}
				}
				lc->rects[j].x = cx_child;
				lc->rects[j].y = cy;
				lc->rects[j].w = cw_child;
				lc->rects[j].h = ch;
				cy += ch + ctx->box_gap;
			}

		} else if(w->type == MKGUI_HBOX) {
			uint32_t scrollable = lc->has_scroll && (w->flags & MKGUI_SCROLL) ? 1 : 0;
			struct mkgui_box_scroll *bs = scrollable ? find_box_scroll(ctx, w->id) : NULL;
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
					int32_t fw = lw_sw(ctx, jw->w);
					uint32_t ct = jw->type;
					if(fw == 0 && (ct == MKGUI_VBOX || ct == MKGUI_HBOX || ct == MKGUI_FORM || ct == MKGUI_GROUP || ct == MKGUI_PANEL)) {
						fw = lc_measure_container(ctx, lc, j, 0);
					}
					lc->rects[j].w = fw;
					fixed_total += fw;
				} else {
					uint32_t ew = jw->weight > 0 ? jw->weight : 1;
					int32_t minw = lw_sw(ctx, jw->w);
					if(minw == 0) {
						minw = natural_width(ctx, jw->type);
					}
					min_total += minw;
					weight_total += ew;
				}
			}
			int32_t gap_total = child_count > 1 ? (int32_t)(child_count - 1) * ctx->box_gap : 0;
			int32_t content_w = fixed_total + min_total + gap_total;
			uint32_t needs_scroll = scrollable && (content_w > pw);
			if(needs_scroll) {
				ph -= ctx->scrollbar_w;
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
					int32_t base_w = lw_sw(ctx, jw->w);
					if(base_w == 0) {
						base_w = natural_width(ctx, jw->type);
					}
					cw = base_w + (weight_total > 0 ? (int32_t)((int64_t)remaining * (int32_t)wt / (int32_t)weight_total) : 0);
				}
				if(!(jw->flags & MKGUI_FIXED)) {
					int32_t min_cw = natural_width(ctx, jw->type);
					if(cw < min_cw) { cw = min_cw; }
				}
				int32_t cy_child = py;
				int32_t ch_child = ph;
				uint32_t align = jw->flags & MKGUI_ALIGN_MASK;
				if(align && lw_sw(ctx, jw->h) > 0) {
					if(align == MKGUI_ALIGN_START) {
						ch_child = lw_sw(ctx, jw->h);
					} else if(align == MKGUI_ALIGN_CENTER) {
						ch_child = lw_sw(ctx, jw->h);
						cy_child = py + (ph - ch_child) / 2;
					} else if(align == MKGUI_ALIGN_END) {
						ch_child = lw_sw(ctx, jw->h);
						cy_child = py + ph - ch_child;
					}
				}
				lc->rects[j].x = cx;
				lc->rects[j].y = cy_child;
				lc->rects[j].w = cw;
				lc->rects[j].h = ch_child;
				cx += cw + ctx->box_gap;
			}

		} else {
			int32_t label_w = 0;
			uint32_t pair_idx = 0;
			for(uint32_t j = lc->first_child[idx]; j < lc->widget_count; j = lc->next_sibling[j]) {
				if((pair_idx & 1) == 0) {
					int32_t tw = text_width(ctx, lw_get(lc, j)->label) + 8;
					if(tw > label_w) {
						label_w = tw;
					}
				}
				++pair_idx;
			}
			pair_idx = 0;
			for(uint32_t j = lc->first_child[idx]; j < lc->widget_count; j = lc->next_sibling[j]) {
				uint32_t row = pair_idx / 2;
				uint32_t row_h = (uint32_t)(ctx->font_height + 10);
				if((pair_idx & 1) == 0) {
					lc->rects[j].x = px;
					lc->rects[j].y = py + (int32_t)(row * (row_h + (uint32_t)ctx->box_gap));
					lc->rects[j].w = label_w;
					lc->rects[j].h = (int32_t)row_h;
				} else {
					lc->rects[j].x = px + label_w;
					lc->rects[j].y = py + (int32_t)(row * (row_h + (uint32_t)ctx->box_gap));
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
			struct mkgui_split_data *sd = find_split_data(ctx, w->id);
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
						lc->rects[c].y = py + split + ctx->split_thick;
						lc->rects[c].w = pw;
						lc->rects[c].h = ph - split - ctx->split_thick;
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
						lc->rects[c].x = px + split + ctx->split_thick;
						lc->rects[c].y = py;
						lc->rects[c].w = pw - split - ctx->split_thick;
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
				if(w->type == MKGUI_WINDOW && ct == MKGUI_PATHBAR && lw_sw(ctx, cw->h) == 0) {
					continue;
				}
				int32_t ml = lw_sw(ctx, cw->margin_l);
				int32_t mr = lw_sw(ctx, cw->margin_r);
				int32_t mt = lw_sw(ctx, cw->margin_t);
				int32_t mb = lw_sw(ctx, cw->margin_b);
				lc->rects[c].x = px + ml;
				lc->rects[c].y = py + mt;
				lc->rects[c].w = pw - ml - mr;
				lc->rects[c].h = ph - mt - mb;
				if((cw->flags & MKGUI_FIXED) && lw_sw(ctx, cw->w) > 0) {
					lc->rects[c].w = lw_sw(ctx, cw->w);
				}
				if((cw->flags & MKGUI_FIXED) && lw_sw(ctx, cw->h) > 0) {
					lc->rects[c].h = lw_sw(ctx, cw->h);
				}
			}
		}
	}

	for(uint32_t c = lc->first_child[idx]; c < lc->widget_count; c = lc->next_sibling[c]) {
		struct mkgui_widget *cw = lw_get(lc, c);
		if(cw->type != MKGUI_MENUITEM && !(cw->flags & MKGUI_HIDDEN)) {
			lc_layout_node(ctx, lc, c);
		}
	}
}

// [=]===^=[ layout_node ]========================================[=]
static void layout_node(struct mkgui_ctx *ctx, uint32_t idx) {
	struct layout_ctx lc = {
		.widgets = ctx->widgets, .rects = (void *)ctx->rects,
		.first_child = layout_first_child, .next_sibling = layout_next_sibling,
		.parent = layout_parent, .widget_count = ctx->widget_count,
		.has_scroll = 1, .widget_stride = sizeof(struct mkgui_widget)
	};
	lc_layout_node(ctx, &lc, idx);
}

static void platform_set_min_size(struct mkgui_ctx *ctx, int32_t min_w, int32_t min_h);
static void platform_font_set_size(struct mkgui_ctx *ctx, int32_t pixel_size);

// [=]===^=[ layout_min_size ]======================================[=]
static void layout_min_size(struct mkgui_ctx *ctx, uint32_t idx, int32_t *out_w, int32_t *out_h) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	uint32_t t = w->type;

	if(w->flags & MKGUI_SCROLL) {
		*out_w = 100;
		*out_h = ctx->row_height * 3;
		return;
	}

	int32_t nh = natural_height(ctx, t);
	if(nh > 0) {
		*out_w = lw_sw(ctx, w->w) > 0 ? lw_sw(ctx, w->w) : 40;
		*out_h = lw_sw(ctx, w->h) > 0 ? lw_sw(ctx, w->h) : nh;
		return;
	}

	if(t == MKGUI_TABS) {
		int32_t max_tw = 0;
		int32_t max_th = 0;
		for(uint32_t c = layout_first_child[idx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
			if(ctx->widgets[c].type == MKGUI_TAB) {
				int32_t tw = 0;
				int32_t th = 0;
				for(uint32_t gc = layout_first_child[c]; gc < ctx->widget_count; gc = layout_next_sibling[gc]) {
					int32_t cw, ch;
					layout_min_size(ctx, gc, &cw, &ch);
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
		*out_w = max_tw + 4;
		*out_h = ctx->tab_height + 4 + max_th;
		return;
	}

	if(t == MKGUI_HSPLIT) {
		int32_t max_w = 0;
		int32_t total_h = ctx->split_thick;
		for(uint32_t c = layout_first_child[idx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
			int32_t cw, ch;
			layout_min_size(ctx, c, &cw, &ch);
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
		int32_t total_w = ctx->split_thick;
		int32_t max_h = 0;
		for(uint32_t c = layout_first_child[idx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
			int32_t cw, ch;
			layout_min_size(ctx, c, &cw, &ch);
			total_w += cw;
			if(ch > max_h) {
				max_h = ch;
			}
		}
		*out_w = total_w;
		*out_h = max_h;
		return;
	}

	if(t == MKGUI_VBOX || t == MKGUI_PANEL) {
		int32_t max_w = 0;
		int32_t total_h = 0;
		uint32_t count = 0;
		for(uint32_t c = layout_first_child[idx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
			if(ctx->widgets[c].flags & MKGUI_HIDDEN) {
				continue;
			}
			int32_t cw, ch;
			layout_min_size(ctx, c, &cw, &ch);
			if(cw > max_w) {
				max_w = cw;
			}
			total_h += ch;
			++count;
		}
		if(count > 1) {
			total_h += (int32_t)(count - 1) * ctx->box_gap;
		}
		*out_w = max_w;
		*out_h = total_h;
		return;
	}

	if(t == MKGUI_HBOX) {
		int32_t total_w = 0;
		int32_t max_h = 0;
		uint32_t count = 0;
		for(uint32_t c = layout_first_child[idx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
			if(ctx->widgets[c].flags & MKGUI_HIDDEN) {
				continue;
			}
			int32_t cw, ch;
			layout_min_size(ctx, c, &cw, &ch);
			if(ctx->widgets[c].w > cw) {
				cw = ctx->widgets[c].w;
			}
			total_w += cw;
			if(ch > max_h) {
				max_h = ch;
			}
			++count;
		}
		if(count > 1) {
			total_w += (int32_t)(count - 1) * ctx->box_gap;
		}
		*out_w = total_w;
		*out_h = max_h;
		return;
	}

	if(t == MKGUI_FORM) {
		uint32_t pair_count = 0;
		for(uint32_t c = layout_first_child[idx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
			++pair_count;
		}
		uint32_t rows = (pair_count + 1) / 2;
		int32_t row_h = ctx->font_height + 10;
		*out_w = 120;
		*out_h = (int32_t)rows * row_h + (rows > 1 ? (int32_t)(rows - 1) * ctx->box_gap : 0);
		return;
	}

	if(t == MKGUI_GROUP) {
		int32_t gtop = ctx->font_height + 4;
		if(w->style & MKGUI_COLLAPSED) {
			*out_w = 40;
			*out_h = gtop;
			return;
		}
		int32_t gpad = 6;
		int32_t inner_w = 0;
		int32_t inner_h = 0;
		for(uint32_t c = layout_first_child[idx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
			int32_t cw, ch;
			layout_min_size(ctx, c, &cw, &ch);
			if(cw > inner_w) {
				inner_w = cw;
			}
			inner_h += ch;
		}
		*out_w = inner_w + 12;
		*out_h = gtop + gpad + inner_h;
		return;
	}

	*out_w = lw_sw(ctx, w->w) > 0 ? lw_sw(ctx, w->w) : 0;
	*out_h = lw_sw(ctx, w->h) > 0 ? lw_sw(ctx, w->h) : 0;
}

// [=]===^=[ layout_compute_min_size ]==============================[=]
static void layout_compute_min_size(struct mkgui_ctx *ctx, uint32_t win_idx) {
	int32_t min_h = 0;
	int32_t min_w = 0;
	for(uint32_t c = layout_first_child[win_idx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
		uint32_t ct = ctx->widgets[c].type;
		if(ct == MKGUI_MENU) {
			min_h += ctx->menu_height;
		} else if(ct == MKGUI_TOOLBAR) {
			min_h += ctx->font_height + 10;
		} else if(ct == MKGUI_STATUSBAR) {
			min_h += ctx->statusbar_height;
		} else if(ct == MKGUI_PATHBAR && ctx->widgets[c].h == 0) {
			min_h += ctx->pathbar_height;
		} else {
			int32_t cw, ch;
			layout_min_size(ctx, c, &cw, &ch);
			min_h += ch;
			if(cw > min_w) {
				min_w = cw;
			}
		}
	}
	if(min_h < 100) {
		min_h = 100;
	}
	if(min_w < 200) {
		min_w = 200;
	}
	min_w += ctx->box_pad * 2;
	min_h += ctx->box_pad * 2;
	platform_set_min_size(ctx, min_w, min_h);
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
			if(ctx->dirty_full) {
				layout_compute_min_size(ctx, i);
			}
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
		if(w->type == MKGUI_WINDOW || w->type == MKGUI_TAB || w->type == MKGUI_PANEL || w->type == MKGUI_SPINNER || w->type == MKGUI_GLVIEW || w->type == MKGUI_VBOX || w->type == MKGUI_HBOX || w->type == MKGUI_FORM || w->type == MKGUI_DIVIDER || w->type == MKGUI_TOOLBAR) {
			continue;
		}
		if(w->type == MKGUI_GROUP) {
			int32_t rx = ctx->rects[i].x;
			int32_t ry = ctx->rects[i].y;
			int32_t rw = ctx->rects[i].w;
			int32_t header_h = ctx->font_height + 4;
			if(mx >= rx && mx < rx + rw && my >= ry && my < ry + header_h) {
				return i;
			}
			continue;
		}
		if(w->type == MKGUI_LABEL && !(w->style & MKGUI_LINK)) {
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
				if(mx >= rx && mx < rx + rw && my >= split_y && my < split_y + ctx->split_thick) {
					return i;
				}
			} else {
				int32_t split_x = rx + (int32_t)(rw * ratio);
				if(mx >= split_x && mx < split_x + ctx->split_thick && my >= ry && my < ry + rh) {
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

static void mkgui_flush(struct mkgui_ctx *ctx);
static void mkgui_resize_render_impl(struct mkgui_ctx *ctx);
static void mkgui_resize_render(struct mkgui_ctx *ctx) {
	mkgui_resize_render_impl(ctx);
}

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
		uint32_t *rowp = &buf[(y + row) * bw];
		uint32_t *src = &icon->pixels[row * icon->w];
		int32_t col = col0;
#if defined(__SSE2__)
		__m128i round_rb = _mm_set1_epi32(0x00800080);
		__m128i round_g = _mm_set1_epi32(0x00000080);
		__m128i mask_rb = _mm_set1_epi32(0x00ff00ff);
		__m128i mask_byte = _mm_set1_epi32(0x000000ff);
		__m128i mask_a = _mm_set1_epi32((int32_t)0xff000000u);
		__m128i val_255 = _mm_set1_epi32(255);
		__m128i zero = _mm_setzero_si128();
		for(; col + 4 <= col1; col += 4) {
			__m128i spx = _mm_loadu_si128((__m128i *)&src[col]);
			__m128i alphas = _mm_srli_epi32(spx, 24);
			__m128i any_zero = _mm_cmpeq_epi32(alphas, zero);
			if(_mm_movemask_epi8(any_zero) == 0xffff) {
				continue;
			}
			int32_t px = x + col;
			__m128i dst = _mm_loadu_si128((__m128i *)&rowp[px]);
			__m128i ia = _mm_sub_epi32(val_255, alphas);
			__m128i ia16 = _mm_or_si128(ia, _mm_slli_epi32(ia, 16));
			__m128i src_rb = _mm_and_si128(spx, mask_rb);
			__m128i src_g8 = _mm_and_si128(_mm_srli_epi32(spx, 8), mask_byte);
			__m128i dst_rb = _mm_and_si128(dst, mask_rb);
			__m128i dst_g8 = _mm_and_si128(_mm_srli_epi32(dst, 8), mask_byte);
			__m128i d_rb = _mm_add_epi32(_mm_mullo_epi16(dst_rb, ia16), round_rb);
			d_rb = _mm_and_si128(_mm_srli_epi32(_mm_add_epi32(d_rb, _mm_and_si128(_mm_srli_epi32(d_rb, 8), mask_rb)), 8), mask_rb);
			__m128i d_gv = _mm_add_epi32(_mm_mullo_epi16(dst_g8, ia), round_g);
			d_gv = _mm_slli_epi32(_mm_and_si128(_mm_srli_epi32(_mm_add_epi32(d_gv, _mm_and_si128(_mm_srli_epi32(d_gv, 8), mask_byte)), 8), mask_byte), 8);
			__m128i rb = _mm_add_epi32(src_rb, d_rb);
			__m128i gv = _mm_add_epi32(_mm_slli_epi32(src_g8, 8), d_gv);
			__m128i result = _mm_or_si128(_mm_and_si128(_mm_or_si128(rb, gv), _mm_set1_epi32(0x00ffffff)), mask_a);
			__m128i full = _mm_cmpeq_epi32(alphas, val_255);
			result = _mm_or_si128(_mm_and_si128(full, spx), _mm_andnot_si128(full, result));
			__m128i skip = _mm_cmpeq_epi32(alphas, zero);
			result = _mm_or_si128(_mm_andnot_si128(skip, result), _mm_and_si128(skip, dst));
			_mm_storeu_si128((__m128i *)&rowp[px], result);
		}
#endif
		for(; col < col1; ++col) {
			uint32_t spx = src[col];
			uint32_t a = spx >> 24;
			int32_t px = x + col;
			if(a == 255) {
				rowp[px] = spx;
			} else if(a > 0) {
				uint32_t ia = 255 - a;
				uint32_t d = rowp[px];
				uint32_t d_rb = (d & 0x00ff00ff) * ia + 0x00800080;
				d_rb = ((d_rb + ((d_rb >> 8) & 0x00ff00ff)) >> 8) & 0x00ff00ff;
				uint32_t d_g = (d & 0x0000ff00) * ia + 0x00008000;
				d_g = ((d_g + ((d_g >> 8) & 0x0000ff00)) >> 8) & 0x0000ff00;
				uint32_t s_rb = spx & 0x00ff00ff;
				uint32_t s_g = spx & 0x0000ff00;
				rowp[px] = 0xff000000 | ((s_rb + d_rb) & 0x00ff00ff) | ((s_g + d_g) & 0x0000ff00);
			}
		}
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
#pragma GCC diagnostic ignored "-Wsign-compare"
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
#include "mkgui_tooltip.c"

// ---------------------------------------------------------------------------
// Render dispatch
// ---------------------------------------------------------------------------

// [=]===^=[ render_widget ]=====================================[=]
static void render_widget(struct mkgui_ctx *ctx, uint32_t idx) {
	switch(ctx->widgets[idx].type) {
		case MKGUI_BUTTON: {
			uint32_t pidx = layout_parent[idx];
			if(pidx < ctx->widget_count && ctx->widgets[pidx].type == MKGUI_TOOLBAR) {
				break;
			}
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

		case MKGUI_METER: {
			render_meter(ctx, idx);
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

		case MKGUI_PATHBAR: {
			render_pathbar(ctx, idx);
		} break;

		case MKGUI_IPINPUT: {
			render_ipinput(ctx, idx);
		} break;

		case MKGUI_TOGGLE: {
			render_toggle(ctx, idx);
		} break;

		case MKGUI_COMBOBOX: {
			render_combobox(ctx, idx);
		} break;

		case MKGUI_DATEPICKER: {
			render_datepicker(ctx, idx);
		} break;

		case MKGUI_GRIDVIEW: {
			render_gridview(ctx, idx);
		} break;

		case MKGUI_RICHLIST: {
			render_richlist(ctx, idx);
		} break;

		case MKGUI_DIVIDER: {
			render_divider(ctx, idx);
		} break;

		case MKGUI_VBOX:
		case MKGUI_HBOX:
		case MKGUI_FORM: {
			if(ctx->widgets[idx].style & MKGUI_PANEL_BORDER) {
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
					int32_t sx = rx + rw - ctx->scrollbar_w;
					draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, sx, ry, ctx->scrollbar_w, rh, ctx->theme.scrollbar_bg);
					int32_t track = rh;
					int32_t total = bs->content_h;
					if(total < 1) {
						total = 1;
					}
					int32_t thumb = track * rh / total;
					if(thumb < sc(ctx, 20)) {
						thumb = sc(ctx, 20);
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
					draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, sx + 2, ry + pos, ctx->scrollbar_w - 4, thumb, ctx->theme.scrollbar_thumb, r);
				}
				if(bs && bs->content_w > rw) {
					int32_t sy = ry + rh - ctx->scrollbar_w;
					draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx, sy, rw, ctx->scrollbar_w, ctx->theme.scrollbar_bg);
					int32_t track = rw;
					int32_t total = bs->content_w;
					if(total < 1) {
						total = 1;
					}
					int32_t thumb = track * rw / total;
					if(thumb < sc(ctx, 20)) {
						thumb = sc(ctx, 20);
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
					draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx + pos, sy + 2, thumb, ctx->scrollbar_w - 4, ctx->theme.scrollbar_thumb, r);
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
	uint32_t pidx = layout_parent[idx];
	while(pidx < ctx->widget_count && ctx->widgets[pidx].type != MKGUI_WINDOW) {
		int32_t px = ctx->rects[pidx].x;
		int32_t py = ctx->rects[pidx].y;
		int32_t px2 = px + ctx->rects[pidx].w;
		int32_t py2 = py + ctx->rects[pidx].h;
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
		for(uint32_t d = 0; d < ctx->dirty_count; ++d) {
			int32_t dx = ctx->dirty_rects[d].x;
			int32_t dy = ctx->dirty_rects[d].y;
			int32_t dw = ctx->dirty_rects[d].w;
			int32_t dh = ctx->dirty_rects[d].h;
			render_base_clip_x1 = dx;
			render_base_clip_y1 = dy;
			render_base_clip_x2 = dx + dw;
			render_base_clip_y2 = dy + dh;
			render_clip_x1 = dx;
			render_clip_y1 = dy;
			render_clip_x2 = dx + dw;
			render_clip_y2 = dy + dh;
			draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, dx, dy, dw, dh, ctx->theme.bg);
			for(uint32_t i = 0; i < ctx->widget_count; ++i) {
				if(!widget_visible(ctx, i)) {
					continue;
				}
				int32_t wx = ctx->rects[i].x;
				int32_t wy = ctx->rects[i].y;
				int32_t wr = wx + ctx->rects[i].w;
				int32_t wb = wy + ctx->rects[i].h;
				if(wx < dx + dw && wr > dx && wy < dy + dh && wb > dy) {
					set_parent_clip(ctx, i);
					render_widget(ctx, i);
				}
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

// [=]===^=[ init_widget_aux ]====================================[=]
static void init_widget_aux(struct mkgui_ctx *ctx, struct mkgui_widget *w) {
	switch(w->type) {
		case MKGUI_INPUT: {
			MKGUI_AUX_GROW(ctx->inputs, ctx->input_count, ctx->input_cap, struct mkgui_input_data);
			if(ctx->input_count < ctx->input_cap) {
				struct mkgui_input_data *inp = &ctx->inputs[ctx->input_count++];
				memset(inp, 0, sizeof(*inp));
				inp->widget_id = w->id;
			}
		} break;

		case MKGUI_IPINPUT: {
			MKGUI_AUX_GROW(ctx->ipinputs, ctx->ipinput_count, ctx->ipinput_cap, struct mkgui_ipinput_data);
			if(ctx->ipinput_count < ctx->ipinput_cap) {
				struct mkgui_ipinput_data *ip = &ctx->ipinputs[ctx->ipinput_count++];
				memset(ip, 0, sizeof(*ip));
				ip->widget_id = w->id;
			}
		} break;

		case MKGUI_TOGGLE: {
			MKGUI_AUX_GROW(ctx->toggles, ctx->toggle_count, ctx->toggle_cap, struct mkgui_toggle_data);
			if(ctx->toggle_count < ctx->toggle_cap) {
				struct mkgui_toggle_data *td = &ctx->toggles[ctx->toggle_count++];
				memset(td, 0, sizeof(*td));
				td->widget_id = w->id;
			}
		} break;

		case MKGUI_COMBOBOX: {
			MKGUI_AUX_GROW(ctx->comboboxes, ctx->combobox_count, ctx->combobox_cap, struct mkgui_combobox_data);
			if(ctx->combobox_count < ctx->combobox_cap) {
				struct mkgui_combobox_data *cb = &ctx->comboboxes[ctx->combobox_count++];
				memset(cb, 0, sizeof(*cb));
				cb->widget_id = w->id;
				cb->selected = -1;
			}
		} break;

		case MKGUI_DATEPICKER: {
			MKGUI_AUX_GROW(ctx->datepickers, ctx->datepicker_count, ctx->datepicker_cap, struct mkgui_datepicker_data);
			if(ctx->datepicker_count < ctx->datepicker_cap) {
				struct mkgui_datepicker_data *dp = &ctx->datepickers[ctx->datepicker_count++];
				memset(dp, 0, sizeof(*dp));
				dp->widget_id = w->id;
				dp->year = 2026;
				dp->month = 1;
				dp->day = 1;
			}
		} break;

		case MKGUI_DROPDOWN: {
			MKGUI_AUX_GROW(ctx->dropdowns, ctx->dropdown_count, ctx->dropdown_cap, struct mkgui_dropdown_data);
			if(ctx->dropdown_count < ctx->dropdown_cap) {
				struct mkgui_dropdown_data *dd = &ctx->dropdowns[ctx->dropdown_count++];
				memset(dd, 0, sizeof(*dd));
				dd->widget_id = w->id;
				dd->selected = -1;
			}
		} break;

		case MKGUI_SLIDER: {
			MKGUI_AUX_GROW(ctx->sliders, ctx->slider_count, ctx->slider_cap, struct mkgui_slider_data);
			if(ctx->slider_count < ctx->slider_cap) {
				struct mkgui_slider_data *sd = &ctx->sliders[ctx->slider_count++];
				memset(sd, 0, sizeof(*sd));
				sd->widget_id = w->id;
				sd->min_val = 0;
				sd->max_val = 100;
				sd->value = 50;
			}
		} break;

		case MKGUI_TABS: {
			MKGUI_AUX_GROW(ctx->tabs, ctx->tab_count, ctx->tab_cap, struct mkgui_tabs_data);
			if(ctx->tab_count < ctx->tab_cap) {
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
			if(w->flags & MKGUI_SCROLL) {
				MKGUI_AUX_GROW(ctx->box_scrolls, ctx->box_scroll_count, ctx->box_scroll_cap, struct mkgui_box_scroll);
				if(ctx->box_scroll_count < ctx->box_scroll_cap) {
					struct mkgui_box_scroll *bs = &ctx->box_scrolls[ctx->box_scroll_count++];
					bs->widget_id = w->id;
					bs->scroll_y = 0;
					bs->content_h = 0;
				}
			}
		} break;

		case MKGUI_HSPLIT:
		case MKGUI_VSPLIT: {
			MKGUI_AUX_GROW(ctx->splits, ctx->split_count, ctx->split_cap, struct mkgui_split_data);
			if(ctx->split_count < ctx->split_cap) {
				struct mkgui_split_data *sd = &ctx->splits[ctx->split_count++];
				sd->widget_id = w->id;
				sd->ratio = 0.5f;
			}
		} break;

		case MKGUI_TREEVIEW: {
			MKGUI_AUX_GROW(ctx->treeviews, ctx->treeview_count, ctx->treeview_cap, struct mkgui_treeview_data);
			if(ctx->treeview_count < ctx->treeview_cap) {
				struct mkgui_treeview_data *tv = &ctx->treeviews[ctx->treeview_count++];
				memset(tv, 0, sizeof(*tv));
				tv->widget_id = w->id;
				tv->selected_node = -1;
			}
		} break;

		case MKGUI_STATUSBAR: {
			MKGUI_AUX_GROW(ctx->statusbars, ctx->statusbar_count, ctx->statusbar_cap, struct mkgui_statusbar_data);
			if(ctx->statusbar_count < ctx->statusbar_cap) {
				struct mkgui_statusbar_data *sb = &ctx->statusbars[ctx->statusbar_count++];
				memset(sb, 0, sizeof(*sb));
				sb->widget_id = w->id;
			}
		} break;

		case MKGUI_SPINBOX: {
			MKGUI_AUX_GROW(ctx->spinboxes, ctx->spinbox_count, ctx->spinbox_cap, struct mkgui_spinbox_data);
			if(ctx->spinbox_count < ctx->spinbox_cap) {
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
			MKGUI_AUX_GROW(ctx->progress, ctx->progress_count, ctx->progress_cap, struct mkgui_progress_data);
			if(ctx->progress_count < ctx->progress_cap) {
				struct mkgui_progress_data *pd = &ctx->progress[ctx->progress_count++];
				memset(pd, 0, sizeof(*pd));
				pd->widget_id = w->id;
				pd->max_val = 100;
			}
		} break;

		case MKGUI_METER: {
			MKGUI_AUX_GROW(ctx->meters, ctx->meter_count, ctx->meter_cap, struct mkgui_meter_data);
			if(ctx->meter_count < ctx->meter_cap) {
				struct mkgui_meter_data *md = &ctx->meters[ctx->meter_count++];
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
			MKGUI_AUX_GROW(ctx->textareas, ctx->textarea_count, ctx->textarea_cap, struct mkgui_textarea_data);
			if(ctx->textarea_count < ctx->textarea_cap) {
				struct mkgui_textarea_data *ta = &ctx->textareas[ctx->textarea_count++];
				memset(ta, 0, sizeof(*ta));
				ta->widget_id = w->id;
				ta->text = (char *)calloc(1, MKGUI_TEXTAREA_INIT_CAP);
				if(!ta->text) {
					--ctx->textarea_count;
					break;
				}
				ta->text_cap = MKGUI_TEXTAREA_INIT_CAP;
			}
		} break;

		case MKGUI_ITEMVIEW: {
			MKGUI_AUX_GROW(ctx->itemviews, ctx->itemview_count, ctx->itemview_cap, struct mkgui_itemview_data);
			if(ctx->itemview_count < ctx->itemview_cap) {
				struct mkgui_itemview_data *iv = &ctx->itemviews[ctx->itemview_count++];
				memset(iv, 0, sizeof(*iv));
				iv->widget_id = w->id;
				iv->selected = -1;
			}
		} break;

		case MKGUI_CANVAS: {
			MKGUI_AUX_GROW(ctx->canvases, ctx->canvas_count, ctx->canvas_cap, struct mkgui_canvas_data);
			if(ctx->canvas_count < ctx->canvas_cap) {
				struct mkgui_canvas_data *cd = &ctx->canvases[ctx->canvas_count++];
				memset(cd, 0, sizeof(*cd));
				cd->widget_id = w->id;
			}
		} break;

		case MKGUI_PATHBAR: {
			MKGUI_AUX_GROW(ctx->pathbars, ctx->pathbar_count, ctx->pathbar_cap, struct mkgui_pathbar_data);
			if(ctx->pathbar_count < ctx->pathbar_cap) {
				struct mkgui_pathbar_data *pb = &ctx->pathbars[ctx->pathbar_count++];
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
static void init_aux_data(struct mkgui_ctx *ctx) {
	for(uint32_t i = 0; i < ctx->widget_count; ++i) {
		init_widget_aux(ctx, &ctx->widgets[i]);
	}
	for(uint32_t i = 0; i < ctx->widget_count; ++i) {
		struct mkgui_widget *w = &ctx->widgets[i];
		if(w->type == MKGUI_BUTTON && w->label[0] != '\0') {
			struct mkgui_widget *parent = find_widget(ctx, w->parent_id);
			if(parent && parent->type == MKGUI_TOOLBAR) {
				snprintf(ctx->tooltip_texts[i], MKGUI_MAX_TEXT, "%s", w->label);
			}
		}
	}
}

// [=]===^=[ mkgui_grow_widgets ]=================================[=]
static uint32_t mkgui_grow_widgets(struct mkgui_ctx *ctx) {
	uint32_t new_cap = ctx->widget_cap + MKGUI_GROW_WIDGETS;
	struct mkgui_widget *nw = (struct mkgui_widget *)realloc(ctx->widgets, (size_t)new_cap * sizeof(struct mkgui_widget));
	struct mkgui_rect *nr = (struct mkgui_rect *)realloc(ctx->rects, (size_t)new_cap * sizeof(struct mkgui_rect));
	char (*nt)[MKGUI_MAX_TEXT] = (char (*)[MKGUI_MAX_TEXT])realloc(ctx->tooltip_texts, (size_t)new_cap * MKGUI_MAX_TEXT);
	if(!nw || !nr || !nt) {
		if(nw) {
			ctx->widgets = nw;
		}
		if(nr) {
			ctx->rects = nr;
		}
		if(nt) {
			ctx->tooltip_texts = nt;
		}
		return 0;
	}
	memset(&nw[ctx->widget_cap], 0, (size_t)MKGUI_GROW_WIDGETS * sizeof(struct mkgui_widget));
	memset(&nr[ctx->widget_cap], 0, (size_t)MKGUI_GROW_WIDGETS * sizeof(struct mkgui_rect));
	memset(&nt[ctx->widget_cap], 0, (size_t)MKGUI_GROW_WIDGETS * MKGUI_MAX_TEXT);
	ctx->widgets = nw;
	ctx->rects = nr;
	ctx->tooltip_texts = nt;
	ctx->widget_cap = new_cap;
	return 1;
}

// [=]===^=[ mkgui_add_widget ]===================================[=]
MKGUI_API uint32_t mkgui_add_widget(struct mkgui_ctx *ctx, struct mkgui_widget w, uint32_t after_id) {
	MKGUI_CHECK_VAL(ctx, 0);
	if(ctx->widget_count >= ctx->widget_cap) {
		if(!mkgui_grow_widgets(ctx)) {
			return 0;
		}
	}

	uint32_t pos = ctx->widget_count;
	if(after_id) {
		for(uint32_t i = 0; i < ctx->widget_count; ++i) {
			if(ctx->widgets[i].id == after_id) {
				pos = i + 1;
				break;
			}
		}
	}

	if(pos < ctx->widget_count) {
		uint32_t tail = ctx->widget_count - pos;
		memmove(&ctx->widgets[pos + 1], &ctx->widgets[pos], tail * sizeof(struct mkgui_widget));
		memmove(&ctx->rects[pos + 1], &ctx->rects[pos], tail * sizeof(struct mkgui_rect));
		memmove(&ctx->tooltip_texts[pos + 1], &ctx->tooltip_texts[pos], tail * MKGUI_MAX_TEXT);
	}

	ctx->widgets[pos] = w;
	memset(&ctx->rects[pos], 0, sizeof(struct mkgui_rect));
	ctx->tooltip_texts[pos][0] = '\0';
	++ctx->widget_count;

	init_widget_aux(ctx, &ctx->widgets[pos]);

	if(w.type == MKGUI_BUTTON && w.label[0] != '\0') {
		struct mkgui_widget *parent = find_widget(ctx, w.parent_id);
		if(parent && parent->type == MKGUI_TOOLBAR) {
			snprintf(ctx->tooltip_texts[pos], MKGUI_MAX_TEXT, "%s", w.label);
		}
	}

	if(w.icon[0] != '\0') {
		icon_resolve(w.icon);
	}

	dirty_all(ctx);
	return 1;
}

// [=]===^=[ mkgui_remove_aux_ ]==================================[=]
static void mkgui_remove_aux_(struct mkgui_ctx *ctx, uint32_t id, uint32_t type) {
	switch(type) {
		case MKGUI_INPUT: {
			for(uint32_t i = 0; i < ctx->input_count; ++i) {
				if(ctx->inputs[i].widget_id == id) {
					if(i < ctx->input_count - 1) {
						ctx->inputs[i] = ctx->inputs[ctx->input_count - 1];
					}
					--ctx->input_count;
					break;
				}
			}
		} break;

		case MKGUI_IPINPUT: {
			for(uint32_t i = 0; i < ctx->ipinput_count; ++i) {
				if(ctx->ipinputs[i].widget_id == id) {
					if(i < ctx->ipinput_count - 1) {
						ctx->ipinputs[i] = ctx->ipinputs[ctx->ipinput_count - 1];
					}
					--ctx->ipinput_count;
					break;
				}
			}
		} break;

		case MKGUI_TOGGLE: {
			for(uint32_t i = 0; i < ctx->toggle_count; ++i) {
				if(ctx->toggles[i].widget_id == id) {
					if(i < ctx->toggle_count - 1) {
						ctx->toggles[i] = ctx->toggles[ctx->toggle_count - 1];
					}
					--ctx->toggle_count;
					break;
				}
			}
		} break;

		case MKGUI_COMBOBOX: {
			for(uint32_t i = 0; i < ctx->combobox_count; ++i) {
				if(ctx->comboboxes[i].widget_id == id) {
					if(i < ctx->combobox_count - 1) {
						ctx->comboboxes[i] = ctx->comboboxes[ctx->combobox_count - 1];
					}
					--ctx->combobox_count;
					break;
				}
			}
		} break;

		case MKGUI_DATEPICKER: {
			for(uint32_t i = 0; i < ctx->datepicker_count; ++i) {
				if(ctx->datepickers[i].widget_id == id) {
					if(i < ctx->datepicker_count - 1) {
						ctx->datepickers[i] = ctx->datepickers[ctx->datepicker_count - 1];
					}
					--ctx->datepicker_count;
					break;
				}
			}
		} break;

		case MKGUI_DROPDOWN: {
			for(uint32_t i = 0; i < ctx->dropdown_count; ++i) {
				if(ctx->dropdowns[i].widget_id == id) {
					if(i < ctx->dropdown_count - 1) {
						ctx->dropdowns[i] = ctx->dropdowns[ctx->dropdown_count - 1];
					}
					--ctx->dropdown_count;
					break;
				}
			}
		} break;

		case MKGUI_SLIDER: {
			for(uint32_t i = 0; i < ctx->slider_count; ++i) {
				if(ctx->sliders[i].widget_id == id) {
					if(i < ctx->slider_count - 1) {
						ctx->sliders[i] = ctx->sliders[ctx->slider_count - 1];
					}
					--ctx->slider_count;
					break;
				}
			}
		} break;

		case MKGUI_TABS: {
			for(uint32_t i = 0; i < ctx->tab_count; ++i) {
				if(ctx->tabs[i].widget_id == id) {
					if(i < ctx->tab_count - 1) {
						ctx->tabs[i] = ctx->tabs[ctx->tab_count - 1];
					}
					--ctx->tab_count;
					break;
				}
			}
		} break;

		case MKGUI_VBOX:
		case MKGUI_HBOX: {
			for(uint32_t i = 0; i < ctx->box_scroll_count; ++i) {
				if(ctx->box_scrolls[i].widget_id == id) {
					if(i < ctx->box_scroll_count - 1) {
						ctx->box_scrolls[i] = ctx->box_scrolls[ctx->box_scroll_count - 1];
					}
					--ctx->box_scroll_count;
					break;
				}
			}
		} break;

		case MKGUI_HSPLIT:
		case MKGUI_VSPLIT: {
			for(uint32_t i = 0; i < ctx->split_count; ++i) {
				if(ctx->splits[i].widget_id == id) {
					if(i < ctx->split_count - 1) {
						ctx->splits[i] = ctx->splits[ctx->split_count - 1];
					}
					--ctx->split_count;
					break;
				}
			}
		} break;

		case MKGUI_TREEVIEW: {
			for(uint32_t i = 0; i < ctx->treeview_count; ++i) {
				if(ctx->treeviews[i].widget_id == id) {
					free(ctx->treeviews[i].nodes);
					if(i < ctx->treeview_count - 1) {
						ctx->treeviews[i] = ctx->treeviews[ctx->treeview_count - 1];
					}
					--ctx->treeview_count;
					break;
				}
			}
		} break;

		case MKGUI_STATUSBAR: {
			for(uint32_t i = 0; i < ctx->statusbar_count; ++i) {
				if(ctx->statusbars[i].widget_id == id) {
					if(i < ctx->statusbar_count - 1) {
						ctx->statusbars[i] = ctx->statusbars[ctx->statusbar_count - 1];
					}
					--ctx->statusbar_count;
					break;
				}
			}
		} break;

		case MKGUI_SPINBOX: {
			for(uint32_t i = 0; i < ctx->spinbox_count; ++i) {
				if(ctx->spinboxes[i].widget_id == id) {
					if(i < ctx->spinbox_count - 1) {
						ctx->spinboxes[i] = ctx->spinboxes[ctx->spinbox_count - 1];
					}
					--ctx->spinbox_count;
					break;
				}
			}
		} break;

		case MKGUI_PROGRESS: {
			for(uint32_t i = 0; i < ctx->progress_count; ++i) {
				if(ctx->progress[i].widget_id == id) {
					if(i < ctx->progress_count - 1) {
						ctx->progress[i] = ctx->progress[ctx->progress_count - 1];
					}
					--ctx->progress_count;
					break;
				}
			}
		} break;

		case MKGUI_METER: {
			for(uint32_t i = 0; i < ctx->meter_count; ++i) {
				if(ctx->meters[i].widget_id == id) {
					if(i < ctx->meter_count - 1) {
						ctx->meters[i] = ctx->meters[ctx->meter_count - 1];
					}
					--ctx->meter_count;
					break;
				}
			}
		} break;

		case MKGUI_TEXTAREA: {
			for(uint32_t i = 0; i < ctx->textarea_count; ++i) {
				if(ctx->textareas[i].widget_id == id) {
					free(ctx->textareas[i].text);
					if(i < ctx->textarea_count - 1) {
						ctx->textareas[i] = ctx->textareas[ctx->textarea_count - 1];
					}
					--ctx->textarea_count;
					break;
				}
			}
		} break;

		case MKGUI_ITEMVIEW: {
			for(uint32_t i = 0; i < ctx->itemview_count; ++i) {
				if(ctx->itemviews[i].widget_id == id) {
					free(ctx->itemviews[i].thumb_buf);
					if(i < ctx->itemview_count - 1) {
						ctx->itemviews[i] = ctx->itemviews[ctx->itemview_count - 1];
					}
					--ctx->itemview_count;
					break;
				}
			}
		} break;

		case MKGUI_LISTVIEW: {
			for(uint32_t i = 0; i < ctx->listv_count; ++i) {
				if(ctx->listvs[i].widget_id == id) {
					if(i < ctx->listv_count - 1) {
						ctx->listvs[i] = ctx->listvs[ctx->listv_count - 1];
					}
					--ctx->listv_count;
					break;
				}
			}
		} break;

		case MKGUI_GRIDVIEW: {
			for(uint32_t i = 0; i < ctx->gridview_count; ++i) {
				if(ctx->gridviews[i].widget_id == id) {
					free(ctx->gridviews[i].checks);
					if(i < ctx->gridview_count - 1) {
						ctx->gridviews[i] = ctx->gridviews[ctx->gridview_count - 1];
					}
					--ctx->gridview_count;
					break;
				}
			}
		} break;

		case MKGUI_RICHLIST: {
			for(uint32_t i = 0; i < ctx->richlist_count; ++i) {
				if(ctx->richlists[i].widget_id == id) {
					if(i < ctx->richlist_count - 1) {
						ctx->richlists[i] = ctx->richlists[ctx->richlist_count - 1];
					}
					--ctx->richlist_count;
					break;
				}
			}
		} break;

		case MKGUI_SCROLLBAR: {
			for(uint32_t i = 0; i < ctx->scrollbar_count; ++i) {
				if(ctx->scrollbars[i].id == id) {
					if(i < ctx->scrollbar_count - 1) {
						ctx->scrollbars[i] = ctx->scrollbars[ctx->scrollbar_count - 1];
					}
					--ctx->scrollbar_count;
					break;
				}
			}
		} break;

		case MKGUI_IMAGE: {
			for(uint32_t i = 0; i < ctx->image_count; ++i) {
				if(ctx->images[i].id == id) {
					free(ctx->images[i].pixels);
					if(i < ctx->image_count - 1) {
						ctx->images[i] = ctx->images[ctx->image_count - 1];
					}
					--ctx->image_count;
					break;
				}
			}
		} break;

		case MKGUI_GLVIEW: {
			for(uint32_t i = 0; i < ctx->glview_count; ++i) {
				if(ctx->glviews[i].id == id) {
					if(ctx->glviews[i].created) {
						platform_glview_destroy(ctx, &ctx->glviews[i]);
					}
					if(i < ctx->glview_count - 1) {
						ctx->glviews[i] = ctx->glviews[ctx->glview_count - 1];
					}
					--ctx->glview_count;
					break;
				}
			}
		} break;

		case MKGUI_CANVAS: {
			for(uint32_t i = 0; i < ctx->canvas_count; ++i) {
				if(ctx->canvases[i].widget_id == id) {
					if(i < ctx->canvas_count - 1) {
						ctx->canvases[i] = ctx->canvases[ctx->canvas_count - 1];
					}
					--ctx->canvas_count;
					break;
				}
			}
		} break;

		case MKGUI_PATHBAR: {
			for(uint32_t i = 0; i < ctx->pathbar_count; ++i) {
				if(ctx->pathbars[i].widget_id == id) {
					if(i < ctx->pathbar_count - 1) {
						ctx->pathbars[i] = ctx->pathbars[ctx->pathbar_count - 1];
					}
					--ctx->pathbar_count;
					break;
				}
			}
		} break;

		default: {
		} break;
	}
}

// [=]===^=[ mkgui_remove_widget ]================================[=]
MKGUI_API uint32_t mkgui_remove_widget(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, 0);
	int32_t idx = find_widget_idx(ctx, id);
	if(idx < 0) {
		return 0;
	}
	if(ctx->widgets[idx].type == MKGUI_WINDOW) {
		return 0;
	}

	uint8_t *marked = (uint8_t *)calloc(ctx->widget_count, 1);
	if(!marked) {
		return 0;
	}
	marked[idx] = 1;
	uint32_t changed = 1;
	while(changed) {
		changed = 0;
		for(uint32_t i = 0; i < ctx->widget_count; ++i) {
			if(!marked[i]) {
				int32_t pidx = find_widget_idx(ctx, ctx->widgets[i].parent_id);
				if(pidx >= 0 && marked[pidx]) {
					marked[i] = 1;
					changed = 1;
				}
			}
		}
	}

	for(uint32_t i = 0; i < ctx->widget_count; ++i) {
		if(marked[i]) {
			uint32_t wid = ctx->widgets[i].id;
			if(ctx->hover_id == wid) {
				ctx->hover_id = 0;
			}
			if(ctx->prev_hover_id == wid) {
				ctx->prev_hover_id = 0;
			}
			if(ctx->press_id == wid) {
				ctx->press_id = 0;
			}
			if(ctx->focus_id == wid) {
				ctx->focus_id = 0;
			}
			if(ctx->prev_focus_id == wid) {
				ctx->prev_focus_id = 0;
			}
			if(ctx->drag_scrollbar_id == wid) {
				ctx->drag_scrollbar_id = 0;
			}
			if(ctx->drag_col_id == wid) {
				ctx->drag_col_id = 0;
			}
			if(ctx->drag_col_resize_id == wid) {
				ctx->drag_col_resize_id = 0;
			}
			if(ctx->drag_select_id == wid) {
				ctx->drag_select_id = 0;
			}
			if(ctx->drag_text_id == wid) {
				ctx->drag_text_id = 0;
			}
			if(ctx->dblclick_id == wid) {
				ctx->dblclick_id = 0;
			}
			if(ctx->divider_dblclick_id == wid) {
				ctx->divider_dblclick_id = 0;
			}
			if(ctx->tooltip_id == wid) {
				ctx->tooltip_id = 0;
			}

			for(uint32_t p = 0; p < ctx->popup_count; ++p) {
				if(ctx->popups[p].widget_id == wid) {
					popup_destroy(ctx, p);
					break;
				}
			}

			mkgui_remove_aux_(ctx, wid, ctx->widgets[i].type);
		}
	}

	for(int32_t i = (int32_t)ctx->widget_count - 1; i >= 0; --i) {
		if(marked[i]) {
			uint32_t last = ctx->widget_count - 1;
			if((uint32_t)i < last) {
				uint32_t tail = last - (uint32_t)i;
				memmove(&ctx->widgets[i], &ctx->widgets[i + 1], tail * sizeof(struct mkgui_widget));
				memmove(&ctx->rects[i], &ctx->rects[i + 1], tail * sizeof(struct mkgui_rect));
				memmove(&ctx->tooltip_texts[i], &ctx->tooltip_texts[i + 1], tail * MKGUI_MAX_TEXT);
				for(uint32_t j = (uint32_t)i; j < last; ++j) {
					marked[j] = marked[j + 1];
				}
			}
			--ctx->widget_count;
		}
	}

	free(marked);
	dirty_all(ctx);
	return 1;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// [=]===^=[ mkgui_alloc_arrays ]=================================[=]
static uint32_t mkgui_alloc_arrays(struct mkgui_ctx *ctx, uint32_t widget_cap) {
	ctx->widget_cap = widget_cap;
	ctx->widgets = (struct mkgui_widget *)calloc(widget_cap, sizeof(struct mkgui_widget));
	ctx->rects = (struct mkgui_rect *)calloc(widget_cap, sizeof(struct mkgui_rect));
	ctx->tooltip_texts = (char (*)[MKGUI_MAX_TEXT])calloc(widget_cap, MKGUI_MAX_TEXT);
	if(!ctx->widgets || !ctx->rects || !ctx->tooltip_texts) {
		free(ctx->widgets);
		free(ctx->rects);
		free(ctx->tooltip_texts);
		return 0;
	}

	ctx->listv_cap = 32;
	ctx->listvs = (struct mkgui_listview_data *)calloc(ctx->listv_cap, sizeof(struct mkgui_listview_data));
	ctx->input_cap = 64;
	ctx->inputs = (struct mkgui_input_data *)calloc(ctx->input_cap, sizeof(struct mkgui_input_data));
	ctx->dropdown_cap = 32;
	ctx->dropdowns = (struct mkgui_dropdown_data *)calloc(ctx->dropdown_cap, sizeof(struct mkgui_dropdown_data));
	ctx->slider_cap = 32;
	ctx->sliders = (struct mkgui_slider_data *)calloc(ctx->slider_cap, sizeof(struct mkgui_slider_data));
	ctx->tab_cap = 32;
	ctx->tabs = (struct mkgui_tabs_data *)calloc(ctx->tab_cap, sizeof(struct mkgui_tabs_data));
	ctx->split_cap = 32;
	ctx->splits = (struct mkgui_split_data *)calloc(ctx->split_cap, sizeof(struct mkgui_split_data));
	ctx->treeview_cap = 8;
	ctx->treeviews = (struct mkgui_treeview_data *)calloc(ctx->treeview_cap, sizeof(struct mkgui_treeview_data));
	ctx->statusbar_cap = 8;
	ctx->statusbars = (struct mkgui_statusbar_data *)calloc(ctx->statusbar_cap, sizeof(struct mkgui_statusbar_data));
	ctx->spinbox_cap = 32;
	ctx->spinboxes = (struct mkgui_spinbox_data *)calloc(ctx->spinbox_cap, sizeof(struct mkgui_spinbox_data));
	ctx->progress_cap = 32;
	ctx->progress = (struct mkgui_progress_data *)calloc(ctx->progress_cap, sizeof(struct mkgui_progress_data));
	ctx->meter_cap = 32;
	ctx->meters = (struct mkgui_meter_data *)calloc(ctx->meter_cap, sizeof(struct mkgui_meter_data));
	ctx->textarea_cap = 16;
	ctx->textareas = (struct mkgui_textarea_data *)calloc(ctx->textarea_cap, sizeof(struct mkgui_textarea_data));
	ctx->itemview_cap = 16;
	ctx->itemviews = (struct mkgui_itemview_data *)calloc(ctx->itemview_cap, sizeof(struct mkgui_itemview_data));
	ctx->scrollbar_cap = 32;
	ctx->scrollbars = (struct mkgui_scrollbar_data *)calloc(ctx->scrollbar_cap, sizeof(struct mkgui_scrollbar_data));
	ctx->box_scroll_cap = 32;
	ctx->box_scrolls = (struct mkgui_box_scroll *)calloc(ctx->box_scroll_cap, sizeof(struct mkgui_box_scroll));
	ctx->image_cap = 32;
	ctx->images = (struct mkgui_image_data *)calloc(ctx->image_cap, sizeof(struct mkgui_image_data));
	ctx->glview_cap = 8;
	ctx->glviews = (struct mkgui_glview_data *)calloc(ctx->glview_cap, sizeof(struct mkgui_glview_data));
	ctx->canvas_cap = 16;
	ctx->canvases = (struct mkgui_canvas_data *)calloc(ctx->canvas_cap, sizeof(struct mkgui_canvas_data));
	ctx->pathbar_cap = 8;
	ctx->pathbars = (struct mkgui_pathbar_data *)calloc(ctx->pathbar_cap, sizeof(struct mkgui_pathbar_data));
	ctx->ipinput_cap = 16;
	ctx->ipinputs = (struct mkgui_ipinput_data *)calloc(ctx->ipinput_cap, sizeof(struct mkgui_ipinput_data));
	ctx->toggle_cap = 32;
	ctx->toggles = (struct mkgui_toggle_data *)calloc(ctx->toggle_cap, sizeof(struct mkgui_toggle_data));
	ctx->combobox_cap = 16;
	ctx->comboboxes = (struct mkgui_combobox_data *)calloc(ctx->combobox_cap, sizeof(struct mkgui_combobox_data));
	ctx->datepicker_cap = 16;
	ctx->datepickers = (struct mkgui_datepicker_data *)calloc(ctx->datepicker_cap, sizeof(struct mkgui_datepicker_data));
	ctx->gridview_cap = 16;
	ctx->gridviews = (struct mkgui_gridview_data *)calloc(ctx->gridview_cap, sizeof(struct mkgui_gridview_data));
	ctx->richlist_cap = 16;
	ctx->richlists = (struct mkgui_richlist_data *)calloc(ctx->richlist_cap, sizeof(struct mkgui_richlist_data));

	return 1;
}

// [=]===^=[ mkgui_free_arrays ]==================================[=]
static void mkgui_free_arrays(struct mkgui_ctx *ctx) {
	free(ctx->widgets);
	free(ctx->rects);
	free(ctx->tooltip_texts);
	free(ctx->listvs);
	free(ctx->inputs);
	free(ctx->dropdowns);
	free(ctx->sliders);
	free(ctx->tabs);
	free(ctx->splits);
	free(ctx->treeviews);
	free(ctx->statusbars);
	free(ctx->spinboxes);
	free(ctx->progress);
	free(ctx->meters);
	free(ctx->textareas);
	free(ctx->itemviews);
	free(ctx->scrollbars);
	free(ctx->box_scrolls);
	free(ctx->images);
	free(ctx->glviews);
	free(ctx->canvases);
	free(ctx->pathbars);
	free(ctx->ipinputs);
	free(ctx->toggles);
	free(ctx->comboboxes);
	free(ctx->datepickers);
	for(uint32_t i = 0; i < ctx->gridview_count; ++i) {
		free(ctx->gridviews[i].checks);
	}
	free(ctx->gridviews);
	free(ctx->richlists);
}

// [=]===^=[ mkgui_create ]======================================[=]
MKGUI_API struct mkgui_ctx *mkgui_create(struct mkgui_widget *widgets, uint32_t count) {
	if(!widgets && count > 0) {
		return NULL;
	}
	struct mkgui_ctx *ctx = (struct mkgui_ctx *)calloc(1, sizeof(struct mkgui_ctx));

	uint32_t cap = (count + MKGUI_GROW_WIDGETS - 1) & ~(uint32_t)(MKGUI_GROW_WIDGETS - 1);
	if(cap < MKGUI_GROW_WIDGETS) {
		cap = MKGUI_GROW_WIDGETS;
	}
	if(!mkgui_alloc_arrays(ctx, cap)) {
		free(ctx);
		return NULL;
	}
	memcpy(ctx->widgets, widgets, count * sizeof(struct mkgui_widget));
	ctx->widget_count = count;

	int32_t init_w = 800, init_h = 600;
	char *title = "mkgui";
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

	ctx->scale = 1.0f;
	mkgui_recompute_metrics(ctx);

	if(!platform_init(ctx, title, init_w, init_h)) {
		mkgui_free_arrays(ctx);
		free(ctx);
		return NULL;
	}

	float detected = platform_detect_scale(ctx);
	if(detected > 1.01f) {
		ctx->scale = detected;
		mkgui_recompute_metrics(ctx);
		int32_t new_w = (int32_t)(init_w * detected + 0.5f);
		int32_t new_h = (int32_t)(init_h * detected + 0.5f);
		platform_resize_window(ctx, new_w, new_h);
	}

	render_clip_x1 = 0;
	render_clip_y1 = 0;
	render_clip_x2 = ctx->win_w;
	render_clip_y2 = ctx->win_h;

	if(!text_cmd_arena.base) {
		if(!text_cmd_init()) {
			mkgui_free_arrays(ctx);
			platform_destroy(ctx);
			free(ctx);
			return NULL;
		}
	}
	if(!layout_arena_init()) {
		mkgui_free_arrays(ctx);
		platform_destroy(ctx);
		free(ctx);
		return NULL;
	}

	platform_font_init(ctx);
	ctx->theme = default_theme();
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


	init_aux_data(ctx);
	window_register(ctx);

	return ctx;
}

// [=]===^=[ mkgui_set_app_class ]================================[=]
MKGUI_API void mkgui_set_app_class(struct mkgui_ctx *ctx, char *app_class) {
	MKGUI_CHECK(ctx);
	MKGUI_CHECK(app_class);
	snprintf(ctx->app_class, sizeof(ctx->app_class), "%s", app_class);
	platform_set_class_hint(&ctx->plat, "main", ctx->app_class);
}

// [=]===^=[ mkgui_set_window_instance ]============================[=]
MKGUI_API void mkgui_set_window_instance(struct mkgui_ctx *ctx, char *instance) {
	MKGUI_CHECK(ctx);
	MKGUI_CHECK(instance);
	char *cls = "mkgui";
	if(ctx->app_class[0]) {
		cls = ctx->app_class;
	} else if(ctx->parent && ctx->parent->app_class[0]) {
		cls = ctx->parent->app_class;
	}
	platform_set_class_hint(&ctx->plat, instance, cls);
}

// [=]===^=[ mkgui_set_window_icon ]================================[=]
MKGUI_API void mkgui_set_window_icon(struct mkgui_ctx *ctx, struct mkgui_icon_size *sizes, uint32_t count) {
	MKGUI_CHECK(ctx);
	MKGUI_CHECK(sizes);
	if(count == 0) {
		return;
	}
	platform_set_window_icon(&ctx->plat, sizes, count);
}

// [=]===^=[ mkgui_set_theme ]====================================[=]
MKGUI_API void mkgui_set_theme(struct mkgui_ctx *ctx, struct mkgui_theme theme) {
	MKGUI_CHECK(ctx);
	ctx->theme = theme;
	svg_rerasterize_all(ctx);
	dirty_all(ctx);
}

// [=]===^=[ mkgui_set_scale ]=====================================[=]
MKGUI_API void mkgui_set_scale(struct mkgui_ctx *ctx, float scale) {
	MKGUI_CHECK(ctx);
	if(scale < 0.5f) { scale = 0.5f; }
	if(scale > 4.0f) { scale = 4.0f; }
	ctx->scale = scale;
	mkgui_recompute_metrics(ctx);
	platform_font_set_size(ctx, (int32_t)(13.0f * scale + 0.5f));
	svg_rerasterize_all(ctx);
	dirty_all(ctx);
}

// [=]===^=[ mkgui_get_scale ]=====================================[=]
MKGUI_API float mkgui_get_scale(struct mkgui_ctx *ctx) {
	MKGUI_CHECK_VAL(ctx, 1.0f);
	return ctx->scale;
}

// [=]===^=[ mkgui_set_weight ]===================================[=]
MKGUI_API void mkgui_set_weight(struct mkgui_ctx *ctx, uint32_t id, uint32_t weight) {
	MKGUI_CHECK(ctx);
	struct mkgui_widget *w = find_widget(ctx, id);
	if(w) {
		w->weight = weight;
		dirty_all(ctx);
	}
}

// [=]===^=[ mkgui_toolbar_set_mode ]==============================[=]
MKGUI_API void mkgui_toolbar_set_mode(struct mkgui_ctx *ctx, uint32_t toolbar_id, uint32_t mode) {
	MKGUI_CHECK(ctx);
	struct mkgui_widget *w = find_widget(ctx, toolbar_id);
	if(w && w->type == MKGUI_TOOLBAR) {
		w->style = (w->style & ~MKGUI_TOOLBAR_MODE_MASK) | (mode & MKGUI_TOOLBAR_MODE_MASK);
		dirty_all(ctx);
	}
}

// [=]===^=[ mkgui_set_enabled ]====================================[=]
MKGUI_API void mkgui_set_enabled(struct mkgui_ctx *ctx, uint32_t id, uint32_t enabled) {
	MKGUI_CHECK(ctx);
	struct mkgui_widget *w = find_widget(ctx, id);
	if(!w) {
		return;
	}
	if(enabled) {
		w->flags &= ~MKGUI_DISABLED;

	} else {
		w->flags |= MKGUI_DISABLED;
		if(ctx->focus_id == id) {
			ctx->focus_id = 0;
		}
	}
	dirty_all(ctx);
}

// [=]===^=[ mkgui_get_enabled ]====================================[=]
MKGUI_API uint32_t mkgui_get_enabled(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_widget *w = find_widget(ctx, id);
	if(!w) {
		return 0;
	}
	return (w->flags & MKGUI_DISABLED) ? 0 : 1;
}

// [=]===^=[ mkgui_set_visible ]====================================[=]
MKGUI_API void mkgui_set_visible(struct mkgui_ctx *ctx, uint32_t id, uint32_t visible) {
	MKGUI_CHECK(ctx);
	struct mkgui_widget *w = find_widget(ctx, id);
	if(!w) {
		return;
	}
	if(visible) {
		w->flags &= ~MKGUI_HIDDEN;

	} else {
		w->flags |= MKGUI_HIDDEN;
		if(ctx->focus_id == id) {
			ctx->focus_id = 0;
		}
	}
	dirty_all(ctx);
}

// [=]===^=[ mkgui_get_visible ]====================================[=]
MKGUI_API uint32_t mkgui_get_visible(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_widget *w = find_widget(ctx, id);
	if(!w) {
		return 0;
	}
	return (w->flags & MKGUI_HIDDEN) ? 0 : 1;
}

// [=]===^=[ mkgui_set_focus ]======================================[=]
MKGUI_API void mkgui_set_focus(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK(ctx);
	struct mkgui_widget *w = find_widget(ctx, id);
	if(!w || !is_focusable(w)) {
		return;
	}
	ctx->focus_id = id;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_has_focus ]======================================[=]
MKGUI_API uint32_t mkgui_has_focus(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, 0);
	return ctx->focus_id == id ? 1 : 0;
}

// [=]===^=[ mkgui_get_tooltip ]====================================[=]
MKGUI_API char *mkgui_get_tooltip(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, "");
	int32_t idx = find_widget_idx(ctx, id);
	if(idx < 0) {
		return "";
	}
	return ctx->tooltip_texts[idx];
}

// [=]===^=[ mkgui_get_geometry ]====================================[=]
MKGUI_API void mkgui_get_geometry(struct mkgui_ctx *ctx, uint32_t id, int32_t *x, int32_t *y, int32_t *w, int32_t *h) {
	MKGUI_CHECK(ctx);
	int32_t idx = find_widget_idx(ctx, id);
	if(idx < 0) {
		if(x) { *x = 0; }
		if(y) { *y = 0; }
		if(w) { *w = 0; }
		if(h) { *h = 0; }
		return;
	}
	if(x) { *x = ctx->rects[idx].x; }
	if(y) { *y = ctx->rects[idx].y; }
	if(w) { *w = ctx->rects[idx].w; }
	if(h) { *h = ctx->rects[idx].h; }
}

// [=]===^=[ mkgui_set_flags ]======================================[=]
MKGUI_API void mkgui_set_flags(struct mkgui_ctx *ctx, uint32_t id, uint32_t flags) {
	MKGUI_CHECK(ctx);
	struct mkgui_widget *w = find_widget(ctx, id);
	if(!w) {
		return;
	}
	w->flags = flags;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_get_flags ]======================================[=]
MKGUI_API uint32_t mkgui_get_flags(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_widget *w = find_widget(ctx, id);
	return w ? w->flags : 0;
}

// [=]===^=[ mkgui_destroy ]=====================================[=]
MKGUI_API void mkgui_destroy(struct mkgui_ctx *ctx) {
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
	for(uint32_t i = 0; i < ctx->timer_count; ++i) {
#ifdef _WIN32
		if(ctx->timers[i].handle) {
			CancelWaitableTimer(ctx->timers[i].handle);
			CloseHandle(ctx->timers[i].handle);
		}
#else
		if(ctx->timers[i].fd >= 0) {
			close(ctx->timers[i].fd);
		}
#endif
	}
	ctx->timer_count = 0;
	mkgui_free_arrays(ctx);
	platform_font_fini(ctx);
	platform_destroy(ctx);
	svg_cleanup();
	if(window_registry_count == 0) {
		text_cmd_fini();
		layout_arena_fini();
	}
	free(ctx);
}

// [=]===^=[ mkgui_create_child ]=================================[=]
MKGUI_API struct mkgui_ctx *mkgui_create_child(struct mkgui_ctx *parent, struct mkgui_widget *widgets, uint32_t count, char *title, int32_t w, int32_t h) {
	if(!parent || (!widgets && count > 0)) {
		return NULL;
	}
	struct mkgui_ctx *ctx = (struct mkgui_ctx *)calloc(1, sizeof(struct mkgui_ctx));

	uint32_t cap = (count + MKGUI_GROW_WIDGETS - 1) & ~(uint32_t)(MKGUI_GROW_WIDGETS - 1);
	if(cap < MKGUI_GROW_WIDGETS) {
		cap = MKGUI_GROW_WIDGETS;
	}
	if(!mkgui_alloc_arrays(ctx, cap)) {
		free(ctx);
		return NULL;
	}
	memcpy(ctx->widgets, widgets, count * sizeof(struct mkgui_widget));
	ctx->widget_count = count;

	ctx->scale = parent->scale;
	mkgui_recompute_metrics(ctx);

	if(!platform_init_child(ctx, parent, title, w, h)) {
		mkgui_free_arrays(ctx);
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


	init_aux_data(ctx);
	window_register(ctx);

	return ctx;
}

// [=]===^=[ mkgui_destroy_child ]================================[=]
MKGUI_API void mkgui_destroy_child(struct mkgui_ctx *ctx) {
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
	mkgui_free_arrays(ctx);
	platform_destroy(ctx);
	free(ctx);
}

// ---------------------------------------------------------------------------
// Event processing
// ---------------------------------------------------------------------------

// [=]===^=[ mkgui_resize_render_impl ]=============================[=]
static void mkgui_resize_render_impl(struct mkgui_ctx *ctx) {
#ifdef _WIN32
	RECT rc;
	GetClientRect(ctx->plat.hwnd, &rc);
	int32_t nw = rc.right - rc.left;
	int32_t nh = rc.bottom - rc.top;
	if(nw > 0 && nh > 0 && (nw != ctx->win_w || nh != ctx->win_h)) {
		ctx->win_w = nw;
		ctx->win_h = nh;
		platform_fb_resize(ctx);
		dirty_all(ctx);
	}
#endif
	mkgui_flush(ctx);
}

// [=]===^=[ mkgui_poll ]========================================[=]
MKGUI_API uint32_t mkgui_poll(struct mkgui_ctx *ctx, struct mkgui_event *ev) {
	MKGUI_CHECK_VAL(ctx, 0);
	if(!ev) {
		return 0;
	}
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

	for(uint32_t ti = 0; ti < ctx->timer_count; ++ti) {
		struct mkgui_timer *t = &ctx->timers[ti];
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
				t->cb(ctx, t->id, t->userdata);
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
				t->cb(ctx, t->id, t->userdata);
			} else {
				ev->type = MKGUI_EVENT_TIMER;
				ev->id = t->id;
				return 1;
			}
		}
#endif
	}

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
		if(w->type == MKGUI_PROGRESS && (w->style & MKGUI_SHIMMER)) {
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

	if(ctx->parent) {
		layout_build_index(ctx);
	}

	while(platform_pending(ctx)) {
		struct mkgui_plat_event pev;
		platform_next_event(ctx, &pev);

		int32_t popup_idx = pev.popup_idx;

		switch(pev.type) {
			// -=[ EXPOSE ]=-
			case MKGUI_PLAT_EXPOSE: {
				dirty_all(ctx);
			} break;

			// -=[ RESIZE ]=-
			case MKGUI_PLAT_RESIZE: {
				if(pev.width != ctx->win_w || pev.height != ctx->win_h) {
					ctx->win_w = pev.width;
					ctx->win_h = pev.height;
					platform_fb_resize(ctx);
					dirty_all(ctx);
					ev->type = MKGUI_EVENT_RESIZE;
				}
			} break;

			// -=[ MOUSE MOVE ]=-
			case MKGUI_PLAT_MOTION: {
				if(popup_idx >= 0) {
					struct mkgui_popup *mp = &ctx->popups[popup_idx];
					struct mkgui_widget *mpw = find_widget(ctx, mp->widget_id);

					if(mp->widget_id == MKGUI_CTXMENU_POPUP_ID) {
						int32_t new_hover = ctxmenu_hit_item(ctx, pev.y);
						if(new_hover != mp->hover_item) {
							mp->hover_item = new_hover;
							mp->dirty = 1;
						}

					} else if(mpw && mpw->type == MKGUI_MENUITEM) {
						int32_t hit_idx;
						struct mkgui_widget *hovered = menu_popup_hit_item(ctx, mpw->id, pev.y, &hit_idx);
						if(hit_idx != mp->hover_item) {
							mp->dirty = 1;
						}
						mp->hover_item = hit_idx;
						if(hovered && !(hovered->style & MKGUI_SEPARATOR) && menu_item_has_children(ctx, hovered->id)) {
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

					} else if(mpw && mpw->type == MKGUI_DATEPICKER) {
						struct mkgui_datepicker_data *mdp = find_datepicker_data(ctx, mpw->id);
						if(mdp) {
							int32_t new_hover = datepicker_cal_hit(ctx, mdp, pev.x, pev.y);
							if(new_hover != mdp->cal_hover) {
								mdp->cal_hover = new_hover;
								mp->dirty = 1;
							}
						}

					} else {
						int32_t scroll_off = 0;
						if(mpw && mpw->type == MKGUI_DROPDOWN) {
							struct mkgui_dropdown_data *mdd = find_dropdown_data(ctx, mpw->id);
							if(mdd) {
								scroll_off = mdd->scroll_y;
							}
						} else if(mpw && mpw->type == MKGUI_COMBOBOX) {
							struct mkgui_combobox_data *mcb = find_combobox_data(ctx, mpw->id);
							if(mcb) {
								scroll_off = mcb->scroll_y;
							}
						}
						int32_t new_item = (pev.y - 1 + scroll_off) / ctx->row_height;
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

				if(ctx->drag_text_id) {
					int32_t dsi = find_widget_idx(ctx, ctx->drag_text_id);
					if(dsi >= 0) {
						struct mkgui_textarea_data *ta = find_textarea_data(ctx, ctx->drag_text_id);
						if(ta) {
							ctx->drag_text_drop_pos = textarea_hit_pos(ctx, ta, ctx->rects[dsi].x, ctx->rects[dsi].y, ctx->rects[dsi].h, ctx->mouse_x, ctx->mouse_y);
							dirty_widget(ctx, (uint32_t)dsi);
						}
					}
					break;
				}

				if(ctx->drag_select_id) {
					int32_t dsi = find_widget_idx(ctx, ctx->drag_select_id);
					if(dsi >= 0 && ctx->widgets[dsi].type == MKGUI_INPUT) {
						struct mkgui_input_data *inp = find_input_data(ctx, ctx->drag_select_id);
						if(inp) {
							char *display = inp->text;
							char masked_buf[MKGUI_MAX_TEXT];
							if(ctx->widgets[dsi].style & MKGUI_PASSWORD) {
								uint32_t mlen = (uint32_t)strlen(inp->text);
								for(uint32_t mi = 0; mi < mlen && mi < MKGUI_MAX_TEXT - 1; ++mi) {
									masked_buf[mi] = '*';
								}
								masked_buf[mlen < MKGUI_MAX_TEXT - 1 ? mlen : MKGUI_MAX_TEXT - 1] = '\0';
								display = masked_buf;
							}
							inp->cursor = input_hit_cursor(ctx, inp, display, ctx->rects[dsi].x, ctx->mouse_x);
							inp->sel_end = inp->cursor;
							dirty_widget(ctx, (uint32_t)dsi);
							input_scroll_to_cursor(ctx, ctx->drag_select_id);
						}

					} else if(dsi >= 0 && ctx->widgets[dsi].type == MKGUI_TEXTAREA) {
						struct mkgui_textarea_data *ta = find_textarea_data(ctx, ctx->drag_select_id);
						if(ta) {
							int32_t ry = ctx->rects[dsi].y;
							int32_t rh = ctx->rects[dsi].h;
							if(ctx->mouse_y < ry + 1) {
								ta->scroll_y -= ctx->row_height;
								if(ta->scroll_y < 0) {
									ta->scroll_y = 0;
								}

							} else if(ctx->mouse_y > ry + rh - 1) {
								int32_t content_h = (int32_t)textarea_line_count(ta) * ctx->row_height;
								int32_t view_h = rh - 2;
								int32_t max_scroll = content_h - view_h;
								if(max_scroll > 0) {
									ta->scroll_y += ctx->row_height;
									if(ta->scroll_y > max_scroll) {
										ta->scroll_y = max_scroll;
									}
								}
							}
							ta->cursor = textarea_hit_pos(ctx, ta, ctx->rects[dsi].x, ctx->rects[dsi].y, ctx->rects[dsi].h, ctx->mouse_x, ctx->mouse_y);
							ta->sel_end = ta->cursor;
							dirty_widget(ctx, (uint32_t)dsi);
						}

					} else if(dsi >= 0 && ctx->widgets[dsi].type == MKGUI_PATHBAR) {
						struct mkgui_pathbar_data *pb = find_pathbar_data(ctx, ctx->drag_select_id);
						if(pb && pb->editing) {
							int32_t rx = ctx->rects[dsi].x;
							int32_t base_x = rx + sc(ctx, 4);
							uint32_t len = (uint32_t)strlen(pb->edit_buf);
							char tmp[4096];
							uint32_t hit_pos = len;
							for(uint32_t i = 0; i <= len; ++i) {
								memcpy(tmp, pb->edit_buf, i);
								tmp[i] = '\0';
								int32_t tw = text_width(ctx, tmp);
								if(base_x + tw >= ctx->mouse_x) {
									if(i > 0) {
										tmp[i - 1] = '\0';
										int32_t prev_w = text_width(ctx, tmp);
										if(ctx->mouse_x - (base_x + prev_w) < (base_x + tw) - ctx->mouse_x) {
											hit_pos = i - 1;
										} else {
											hit_pos = i;
										}
									} else {
										hit_pos = i;
									}
									break;
								}
							}
							pb->edit_cursor = hit_pos;
							pb->edit_sel_end = hit_pos;
							dirty_widget(ctx, (uint32_t)dsi);
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
								if(total < 1) {
									total = 1;
								}
								int32_t thumb = track * sw / total;
								if(thumb < sc(ctx, 20)) {
									thumb = sc(ctx, 20);
								}
								if(thumb > track) {
									thumb = track;
								}
								int32_t max_scroll = bs->content_w - sw;
								if(max_scroll < 1) {
									max_scroll = 1;
								}
								int32_t new_pos = ctx->mouse_x - sx - ctx->drag_scrollbar_offset;
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
								int32_t sh = ctx->rects[dsi].h;
								int32_t sy = ctx->rects[dsi].y;
								int32_t track = sh;
								int32_t total = bs->content_h;
								if(total < 1) {
									total = 1;
								}
								int32_t thumb = track * sh / total;
								if(thumb < sc(ctx, 20)) {
									thumb = sc(ctx, 20);
								}
								if(thumb > track) {
									thumb = track;
								}
								int32_t max_scroll = bs->content_h - sh;
								if(max_scroll < 1) {
									max_scroll = 1;
								}
								int32_t new_pos = ctx->mouse_y - sy - ctx->drag_scrollbar_offset;
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
							dirty_widget(ctx, (uint32_t)dsi);
						}
					} else if(dsi >= 0 && ctx->widgets[dsi].type == MKGUI_GRIDVIEW) {
						gridview_scroll_to_y(ctx, ctx->drag_scrollbar_id, ctx->mouse_y);
					} else if(dsi >= 0 && ctx->widgets[dsi].type == MKGUI_RICHLIST) {
						richlist_scroll_to_y(ctx, ctx->drag_scrollbar_id, ctx->mouse_y);
					} else if(ctx->drag_scrollbar_horiz) {
						listview_scroll_to_x(ctx, ctx->drag_scrollbar_id, ctx->mouse_x);
					} else {
						listview_scroll_to_y(ctx, ctx->drag_scrollbar_id, ctx->mouse_y);
					}
					break;
				}

				if(ctx->drag_col_resize_id) {
					int32_t nw = ctx->drag_col_resize_start_w + (ctx->mouse_x - ctx->drag_col_resize_start_x);
					if(nw < sc(ctx, 40)) {
						nw = sc(ctx, 40);
					}
					struct mkgui_listview_data *lv = find_listv_data(ctx, ctx->drag_col_resize_id);
					if(lv) {
						lv->columns[ctx->drag_col_resize_col].width = nw;
						dirty_widget_id(ctx, ctx->drag_col_resize_id);

					} else {
						struct mkgui_gridview_data *gv = find_gridv_data(ctx, ctx->drag_col_resize_id);
						if(gv) {
							gv->columns[ctx->drag_col_resize_col].width = nw;
							dirty_widget_id(ctx, ctx->drag_col_resize_id);
						}
					}
					break;
				}

				if(ctx->drag_col_id) {
					int32_t dx = ctx->mouse_x - ctx->drag_col_start_x;
					if(dx < -sc(ctx, 3) || dx > sc(ctx, 3)) {
						dirty_widget_id(ctx, ctx->drag_col_id);
					}
					int32_t lv_idx = find_widget_idx(ctx, ctx->drag_col_id);
					if(lv_idx >= 0) {
						struct mkgui_listview_data *lv = find_listv_data(ctx, ctx->drag_col_id);
						if(lv) {
							int32_t lx = ctx->rects[lv_idx].x;
							int32_t lw = ctx->rects[lv_idx].w;
							int32_t edge = sc(ctx, 30);
							int32_t scroll_step = ctx->row_height * 2;
							int32_t content_w = lw - 2 - ctx->scrollbar_w;
							if(ctx->mouse_x < lx + edge) {
								lv->scroll_x -= scroll_step;
								dirty_widget(ctx, (uint32_t)lv_idx);
							} else if(ctx->mouse_x > lx + lw - edge) {
								lv->scroll_x += scroll_step;
								dirty_widget(ctx, (uint32_t)lv_idx);
							}
							listview_clamp_scroll_x(lv, content_w);
						}
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
										float min_ratio = (float)ctx->split_min_px / (float)rh;
										float max_ratio = 1.0f - (float)(ctx->split_min_px + ctx->split_thick) / (float)rh;
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
									int32_t rw = ctx->rects[pi].w;
									if(rw > 0) {
										sd->ratio = (float)(ctx->mouse_x - ctx->rects[pi].x) / (float)rw;
										float min_ratio = (float)ctx->split_min_px / (float)rw;
										float max_ratio = 1.0f - (float)(ctx->split_min_px + ctx->split_thick) / (float)rw;
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
								dirty_widget(ctx, (uint32_t)pi);
							}
						}
					}

					if(pw && pw->type == MKGUI_SLIDER) {
						struct mkgui_slider_data *sd = find_slider_data(ctx, pw->id);
						int32_t pi = find_widget_idx(ctx, pw->id);
						if(sd && pi >= 0) {
							int32_t range = sd->max_val - sd->min_val;
							int32_t thumb_sz = sc(ctx, 10);
							int32_t new_val;
							if(pw->flags & MKGUI_VERTICAL) {
								int32_t ry = ctx->rects[pi].y;
								int32_t rh = ctx->rects[pi].h;
								int32_t track = rh - thumb_sz;
								int32_t pos = (ctx->mouse_y - ctx->drag_slider_offset) - ry - thumb_sz / 2;
								new_val = sd->max_val - (track > 0 ? (int32_t)((int64_t)pos * range / track) : 0);
							} else {
								int32_t rx = ctx->rects[pi].x;
								int32_t rw = ctx->rects[pi].w;
								int32_t track = rw - thumb_sz;
								int32_t pos = (ctx->mouse_x - ctx->drag_slider_offset) - rx - thumb_sz / 2;
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
				if(hi >= 0 && ctx->widgets[hi].type == MKGUI_PATHBAR) {
					struct mkgui_pathbar_data *pb = find_pathbar_data(ctx, ctx->widgets[hi].id);
					if(pb && !pb->editing) {
						int32_t seg = pathbar_segment_hit(ctx, pb, ctx->rects[hi].x, ctx->mouse_x);
						if(seg != pb->hover_seg) {
							pb->hover_seg = seg;
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
						if(!tv->drag_active && dy > sc(ctx, 4)) {
							tv->drag_active = 1;
							ev->type = MKGUI_EVENT_DRAG_START;
							ev->id = tv->widget_id;
							ev->value = (int32_t)tv->nodes[tv->drag_source].id;
							dirty_widget_id(ctx, tv->widget_id);
							return 1;
						}
						if(tv->drag_active) {
							int32_t tidx = find_widget_idx(ctx, tv->widget_id);
							if(tidx >= 0) {
								int32_t node_idx = treeview_row_hit(ctx, (uint32_t)tidx, ctx->mouse_y);
								if(node_idx >= 0 && node_idx != tv->drag_source) {
									int32_t ry = ctx->rects[tidx].y;
									int32_t local_y = ctx->mouse_y - ry - 1 + tv->scroll_y;
									int32_t row_local = local_y % ctx->row_height;
									int32_t third = ctx->row_height / 3;
									if(row_local < third) {
										tv->drag_pos = 0;
									} else if(row_local > ctx->row_height - third) {
										tv->drag_pos = 1;
									} else {
										tv->drag_pos = 2;
									}
									tv->drag_target = node_idx;
								} else {
									tv->drag_target = -1;
								}
								dirty_widget(ctx, (uint32_t)tidx);
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
						if(!lv->drag_active && dy > sc(ctx, 4)) {
							lv->drag_active = 1;
							ev->type = MKGUI_EVENT_DRAG_START;
							ev->id = lv->widget_id;
							ev->value = lv->drag_source;
							dirty_widget_id(ctx, lv->widget_id);
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
								dirty_widget(ctx, (uint32_t)lidx);
							}
						}
					}
				}
				for(uint32_t gvi = 0; gvi < ctx->gridview_count; ++gvi) {
					struct mkgui_gridview_data *gv = &ctx->gridviews[gvi];
					if(gv->drag_source >= 0 && ctx->press_id == gv->widget_id) {
						int32_t dy = ctx->mouse_y - gv->drag_start_y;
						if(dy < 0) {
							dy = -dy;
						}
						if(!gv->drag_active && dy > sc(ctx, 4)) {
							gv->drag_active = 1;
							ev->type = MKGUI_EVENT_DRAG_START;
							ev->id = gv->widget_id;
							ev->value = gv->drag_source;
							dirty_widget_id(ctx, gv->widget_id);
							return 1;
						}
						if(gv->drag_active) {
							int32_t gidx = find_widget_idx(ctx, gv->widget_id);
							if(gidx >= 0) {
								int32_t tgt = gridview_row_hit(ctx, (uint32_t)gidx, ctx->mouse_y);
								if(tgt >= 0 && tgt != gv->drag_source) {
									gv->drag_target = tgt;
								} else {
									gv->drag_target = -1;
								}
								dirty_widget(ctx, (uint32_t)gidx);
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
							dirty_widget(ctx, (uint32_t)hi);
						}
					}
				}

				uint32_t want_resize_cursor = 0;
				if(hi >= 0 && ctx->widgets[hi].type == MKGUI_LISTVIEW) {
					if(listview_divider_hit(ctx, (uint32_t)hi, ctx->mouse_x, ctx->mouse_y) >= 0) {
						want_resize_cursor = 1;
					}
				}
				if(hi >= 0 && ctx->widgets[hi].type == MKGUI_GRIDVIEW) {
					if(gridview_divider_hit(ctx, (uint32_t)hi, ctx->mouse_x, ctx->mouse_y) >= 0) {
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

			// -=[ BUTTON PRESS ]=-
			case MKGUI_PLAT_BUTTON_PRESS: {
				if(popup_idx >= 0) {
					struct mkgui_popup *p = &ctx->popups[popup_idx];
					struct mkgui_widget *pw = find_widget(ctx, p->widget_id);

					if(p->widget_id == MKGUI_CTXMENU_POPUP_ID && pev.button == 1) {
						int32_t hit = ctxmenu_hit_item(ctx, pev.y);
						if(hit >= 0) {
							struct mkgui_ctxmenu_item *it = ctxmenu_item_at(ctx, hit);
							if(it && !(it->flags & MKGUI_DISABLED)) {
								if(it->flags & MKGUI_MENU_CHECK) {
									it->flags ^= MKGUI_CHECKED;
								} else if(it->flags & MKGUI_MENU_RADIO) {
									for(uint32_t ri = 0; ri < ctx->ctxmenu_count; ++ri) {
										ctx->ctxmenu_items[ri].flags &= ~MKGUI_CHECKED;
									}
									it->flags |= MKGUI_CHECKED;
								}
								ev->type = MKGUI_EVENT_CONTEXT_MENU;
								ev->id = it->id;
								ev->value = (it->flags & MKGUI_CHECKED) ? 1 : 0;
								popup_destroy_all(ctx);
								dirty_all(ctx);
								return 1;
							}
						}
						popup_destroy_all(ctx);
						dirty_all(ctx);
						break;
					}

					if((pev.button == 4 || pev.button == 5) && pw && pw->type == MKGUI_DATEPICKER) {
						struct mkgui_datepicker_data *dp = find_datepicker_data(ctx, pw->id);
						if(dp) {
							int32_t hit = datepicker_cal_hit(ctx, dp, pev.x, pev.y);
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
							dirty_all(ctx);
						}
						break;
					}

					if((pev.button == 4 || pev.button == 5) && pw && pw->type == MKGUI_DROPDOWN) {
						struct mkgui_dropdown_data *dd = find_dropdown_data(ctx, pw->id);
						if(dd) {
							int32_t delta = (pev.button == 4) ? -ctx->row_height * 3 : ctx->row_height * 3;
							dd->scroll_y += delta;
							dropdown_clamp_scroll(ctx, dd, p->h);
							p->hover_item = (pev.y - 1 + dd->scroll_y) / ctx->row_height;
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
						int32_t item = (pev.y - 1 + scroll_off) / ctx->row_height;
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

					} else if(pw && pw->type == MKGUI_COMBOBOX) {
						struct mkgui_combobox_data *cb = find_combobox_data(ctx, pw->id);
						if(cb) {
							int32_t item = (pev.y - 1 + cb->scroll_y) / ctx->row_height;
							if(item >= 0 && item < (int32_t)cb->filter_count) {
								uint32_t real_idx = cb->filter_map[item];
								cb->selected = (int32_t)real_idx;
								{ size_t _l = strlen(cb->items[real_idx]); if(_l >= MKGUI_MAX_TEXT) { _l = MKGUI_MAX_TEXT - 1; } memcpy(cb->text, cb->items[real_idx], _l); cb->text[_l] = '\0'; }
								cb->cursor = (uint32_t)strlen(cb->text);
								combobox_clear_selection(cb);
								combobox_close_popup(ctx, cb);
								ev->type = MKGUI_EVENT_COMBOBOX_CHANGED;
								ev->id = pw->id;
								ev->value = cb->selected;
								dirty_all(ctx);
								return 1;
							}
						}

					} else if(pw && pw->type == MKGUI_DATEPICKER) {
						struct mkgui_datepicker_data *dp = find_datepicker_data(ctx, pw->id);
						if(dp) {
							int32_t hit = datepicker_cal_hit(ctx, dp, pev.x, pev.y);
							if(hit == -2) {
								--dp->view_month;
								if(dp->view_month < 1) {
									dp->view_month = 12;
									--dp->view_year;
								}
								p->dirty = 1;
								dirty_all(ctx);
								break;
							} else if(hit == -3) {
								++dp->view_month;
								if(dp->view_month > 12) {
									dp->view_month = 1;
									++dp->view_year;
								}
								p->dirty = 1;
								dirty_all(ctx);
								break;
							} else if(hit == -4) {
								dp->month_select = dp->month_select ? 0 : 1;
								dp->cal_hover = -1;
								p->dirty = 1;
								dirty_all(ctx);
								break;
							} else if(hit == -5) {
								++dp->view_year;
								p->dirty = 1;
								dirty_all(ctx);
								break;
							} else if(hit == -6) {
								--dp->view_year;
								p->dirty = 1;
								dirty_all(ctx);
								break;
							} else if(hit >= 100 && hit < 112) {
								dp->view_month = hit - 100 + 1;
								dp->month_select = 0;
								dp->cal_hover = -1;
								p->dirty = 1;
								dirty_all(ctx);
								break;
							} else if(hit == -7 || hit == -1) {
								break;
							} else if(hit > 0) {
								dp->day = hit;
								dp->month = dp->view_month;
								dp->year = dp->view_year;
								dp->popup_open = 0;
								dp->editing = 0;
								popup_destroy(ctx, (uint32_t)popup_idx);
								ev->type = MKGUI_EVENT_DATEPICKER_CHANGED;
								ev->id = pw->id;
								dirty_all(ctx);
								return 1;
							}
						}

					} else if(pw && pw->type == MKGUI_MENUITEM) {
						int32_t hit_idx;
						struct mkgui_widget *clicked = menu_popup_hit_item(ctx, pw->id, pev.y, &hit_idx);
						if(clicked && !(clicked->style & MKGUI_SEPARATOR)) {
							if(menu_item_has_children(ctx, clicked->id)) {
								menu_open_submenu(ctx, (uint32_t)popup_idx, clicked->id);

							} else {
								if(clicked->style & MKGUI_MENU_CHECK) {
									clicked->style ^= MKGUI_CHECKED;

								} else if(clicked->style & MKGUI_MENU_RADIO) {
									for(uint32_t ri = 0; ri < ctx->widget_count; ++ri) {
										struct mkgui_widget *rw = &ctx->widgets[ri];
										if(rw->type == MKGUI_MENUITEM && rw->parent_id == clicked->parent_id && (rw->style & MKGUI_MENU_RADIO)) {
											rw->style &= ~MKGUI_CHECKED;
										}
									}
									clicked->style |= MKGUI_CHECKED;
								}
								ev->type = MKGUI_EVENT_MENU;
								ev->id = clicked->id;
								ev->value = (clicked->style & MKGUI_CHECKED) ? 1 : 0;
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
				uint32_t closed_combobox_id = 0;
				if(ctx->popup_count > 0) {
					for(uint32_t pi = 0; pi < ctx->popup_count; ++pi) {
						struct mkgui_widget *pw = find_widget(ctx, ctx->popups[pi].widget_id);
						if(pw && pw->type == MKGUI_DROPDOWN) {
							closed_dropdown_id = ctx->popups[pi].widget_id;
						}
						if(pw && pw->type == MKGUI_COMBOBOX) {
							closed_combobox_id = ctx->popups[pi].widget_id;
							struct mkgui_combobox_data *cb = find_combobox_data(ctx, pw->id);
							if(cb) {
								cb->popup_open = 0;
							}
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
						struct mkgui_widget *hw = &ctx->widgets[hi];
						ctx->ctxmenu_x = ctx->mouse_x;
						ctx->ctxmenu_y = ctx->mouse_y;

						if(hw->type == MKGUI_LISTVIEW) {
							struct mkgui_listview_data *lv = find_listv_data(ctx, hw->id);
							if(lv) {
								int32_t hdr = listview_header_hit(ctx, (uint32_t)hi, ctx->mouse_x, ctx->mouse_y);
								if(hdr >= 0) {
									ev->type = MKGUI_EVENT_CONTEXT_HEADER;
									ev->id = hw->id;
									ev->col = (int32_t)lv->col_order[hdr];
									ev->value = ctx->mouse_x;
									return 1;
								}
								int32_t hh = lv->header_height > 0 ? lv->header_height : ctx->row_height;
								int32_t body_y = ctx->rects[hi].y + hh + 1;
								int32_t row = (ctx->mouse_y - body_y + lv->scroll_y) / ctx->row_height;
								int32_t col = -1;
								int32_t cx = ctx->rects[hi].x - lv->scroll_x;
								for(uint32_t d = 0; d < lv->col_count; ++d) {
									uint32_t c = lv->col_order[d];
									if(ctx->mouse_x >= cx && ctx->mouse_x < cx + lv->columns[c].width) {
										col = (int32_t)c;
										break;
									}
									cx += lv->columns[c].width;
								}
								if(row < 0 || row >= (int32_t)lv->row_count) {
									row = -1;
								} else {
									if(hw->style & MKGUI_MULTI_SELECT) {
										if(lv_multi_sel_find(lv, row) < 0) {
											lv->multi_sel_count = 0;
											lv->multi_sel[0] = row;
											lv->multi_sel_count = 1;
											lv->selected_row = row;
											dirty_widget(ctx, (uint32_t)hi);
										}
									} else {
										if(lv->selected_row != row) {
											lv->selected_row = row;
											dirty_widget(ctx, (uint32_t)hi);
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
							struct mkgui_gridview_data *gv = find_gridv_data(ctx, hw->id);
							if(gv) {
								int32_t ghh = gv->header_height > 0 ? gv->header_height : ctx->row_height;
								if(ctx->mouse_y < ctx->rects[hi].y + ghh) {
									int32_t col = gridview_col_at_x(gv, ctx->mouse_x, ctx->rects[hi].x + 1);
									ev->type = MKGUI_EVENT_CONTEXT_HEADER;
									ev->id = hw->id;
									ev->col = col;
									ev->value = ctx->mouse_x;
									return 1;
								}
								int32_t body_y = ctx->rects[hi].y + ghh + 1;
								int32_t row = (ctx->mouse_y - body_y + gv->scroll_y) / ctx->row_height;
								int32_t col = gridview_col_at_x(gv, ctx->mouse_x, ctx->rects[hi].x + 1);
								if(row < 0 || row >= (int32_t)gv->row_count) {
									row = -1;
								} else if(gv->selected_row != row) {
									gv->selected_row = row;
									gv->selected_col = col;
									dirty_widget(ctx, (uint32_t)hi);
								}
								ev->type = MKGUI_EVENT_CONTEXT;
								ev->id = hw->id;
								ev->value = row;
								ev->col = col;
								return 1;
							}

						} else if(hw->type == MKGUI_RICHLIST) {
							struct mkgui_richlist_data *rl = find_richlist_data(ctx, hw->id);
							if(rl) {
								int32_t row = richlist_row_hit(ctx, (uint32_t)hi, ctx->mouse_y);
								if(row >= 0 && rl->selected_row != row) {
									rl->selected_row = row;
									dirty_widget(ctx, (uint32_t)hi);
								}
								ev->type = MKGUI_EVENT_CONTEXT;
								ev->id = hw->id;
								ev->value = row;
								return 1;
							}

						} else if(hw->type == MKGUI_TREEVIEW) {
							struct mkgui_treeview_data *tv = find_treeview_data(ctx, hw->id);
							if(tv) {
								int32_t body_y = ctx->rects[hi].y + 1;
								tv_idx_build(tv);
								int32_t vis_row = (ctx->mouse_y - body_y + tv->scroll_y) / ctx->row_height;
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
									dirty_widget(ctx, (uint32_t)hi);
								}
								ev->type = MKGUI_EVENT_CONTEXT;
								ev->id = hw->id;
								ev->value = node_id;
								ev->col = 0;
								return 1;
							}

						} else if(hw->type == MKGUI_ITEMVIEW) {
							struct mkgui_itemview_data *iv = find_itemview_data(ctx, hw->id);
							if(iv) {
								int32_t item = itemview_hit_item(ctx, (uint32_t)hi, iv, ctx->mouse_x, ctx->mouse_y);
								if(item >= 0 && iv->selected != item) {
									iv->selected = item;
									dirty_widget(ctx, (uint32_t)hi);
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
						ev->value = ctx->mouse_x;
						ev->col = ctx->mouse_y;
						return 1;
					}
					break;
				}

				if(pev.button == 4 || pev.button == 5) {
					int32_t hi = hit_test(ctx, ctx->mouse_x, ctx->mouse_y);
					int32_t delta = (pev.button == 4) ? -ctx->row_height * 3 : ctx->row_height * 3;
					if(hi >= 0 && ctx->widgets[hi].type == MKGUI_LISTVIEW) {
						struct mkgui_listview_data *lv = find_listv_data(ctx, ctx->widgets[hi].id);
						if(lv) {
							if((pev.keymod & MKGUI_MOD_SHIFT) || listview_hscrollbar_hit(ctx, (uint32_t)hi, ctx->mouse_x, ctx->mouse_y)) {
								lv->scroll_x += delta;
								int32_t content_w = ctx->rects[hi].w - 2 - ctx->scrollbar_w;
								listview_clamp_scroll_x(lv, content_w);
							} else {
								lv->scroll_y += delta;
								int32_t content_h = ctx->rects[hi].h - ctx->row_height - 2;
								int32_t content_w = ctx->rects[hi].w - 2 - ctx->scrollbar_w;
								if(listview_total_col_w(lv) > content_w) {
									content_h -= ctx->scrollbar_w;
								}
								int32_t max_scroll = (int32_t)lv->row_count * ctx->row_height - content_h;
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
							dirty_widget(ctx, (uint32_t)hi);
						}

					} else if(hi >= 0 && ctx->widgets[hi].type == MKGUI_GRIDVIEW) {
						struct mkgui_gridview_data *gv = find_gridv_data(ctx, ctx->widgets[hi].id);
						if(gv) {
							gv->scroll_y += delta;
							int32_t hh = gv->header_height > 0 ? gv->header_height : ctx->row_height;
							int32_t content_h = ctx->rects[hi].h - hh - 2;
							gridview_clamp_scroll(ctx, gv, content_h);
							dirty_widget(ctx, (uint32_t)hi);
						}

					} else if(hi >= 0 && ctx->widgets[hi].type == MKGUI_RICHLIST) {
						struct mkgui_richlist_data *rl = find_richlist_data(ctx, ctx->widgets[hi].id);
						if(rl) {
							rl->scroll_y += delta;
							richlist_clamp_scroll(rl, ctx->rects[hi].h - 2);
							dirty_widget(ctx, (uint32_t)hi);
						}

					} else if(hi >= 0 && ctx->widgets[hi].type == MKGUI_TREEVIEW) {
						struct mkgui_treeview_data *tv = find_treeview_data(ctx, ctx->widgets[hi].id);
						if(tv) {
							tv->scroll_y += delta;
							treeview_clamp_scroll(ctx, ctx->widgets[hi].id);
							dirty_widget(ctx, (uint32_t)hi);
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

					} else if(hi >= 0 && ctx->widgets[hi].type == MKGUI_DROPDOWN) {
						struct mkgui_dropdown_data *dd = find_dropdown_data(ctx, ctx->widgets[hi].id);
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
								dirty_widget(ctx, (uint32_t)hi);
								ev->type = MKGUI_EVENT_DROPDOWN_CHANGED;
								ev->id = ctx->widgets[hi].id;
								ev->value = sel;
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
							uint32_t vert = (ctx->widgets[hi].flags & MKGUI_VERTICAL);
							sd->value += (pev.button == 4) ? (vert ? step : -step) : (vert ? -step : step);
							if(sd->value < sd->min_val) {
								sd->value = sd->min_val;
							}
							if(sd->value > sd->max_val) {
								sd->value = sd->max_val;
							}
							dirty_widget(ctx, (uint32_t)hi);
							ev->type = MKGUI_EVENT_SLIDER_CHANGED;
							ev->id = ctx->widgets[hi].id;
							ev->value = sd->value;
							return 1;
						}

					} else if(hi >= 0 && ctx->widgets[hi].type == MKGUI_TEXTAREA) {
						struct mkgui_textarea_data *ta = find_textarea_data(ctx, ctx->widgets[hi].id);
						if(ta) {
							ta->scroll_y += delta;
							int32_t content_h = (int32_t)textarea_line_count(ta) * ctx->row_height;
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
							dirty_widget(ctx, (uint32_t)hi);
						}

					} else if(hi >= 0 && ctx->widgets[hi].type == MKGUI_ITEMVIEW) {
						struct mkgui_itemview_data *iv = find_itemview_data(ctx, ctx->widgets[hi].id);
						if(iv) {
							iv->scroll_y += delta;
							int32_t ca_x, ca_y, ca_w, ca_h;
							itemview_content_area(ctx, iv, ctx->rects[hi].x, ctx->rects[hi].y, ctx->rects[hi].w, ctx->rects[hi].h, &ca_x, &ca_y, &ca_w, &ca_h);
							itemview_clamp_scroll(ctx, iv, ca_w, ca_h);
							dirty_widget(ctx, (uint32_t)hi);
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
									} else {
										int32_t view_h = ctx->rects[si].h;
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
									}
									dirty_widget(ctx, (uint32_t)si);
								}
								break;
							}
							sid = ctx->widgets[si].parent_id;
						}
					}
					break;
				}

				if(pev.button == 6 || pev.button == 7) {
					int32_t hi = hit_test(ctx, ctx->mouse_x, ctx->mouse_y);
					int32_t delta = (pev.button == 6) ? -ctx->row_height * 3 : ctx->row_height * 3;
					if(hi >= 0 && ctx->widgets[hi].type == MKGUI_LISTVIEW) {
						struct mkgui_listview_data *lv = find_listv_data(ctx, ctx->widgets[hi].id);
						if(lv) {
							lv->scroll_x += delta;
							int32_t content_w = ctx->rects[hi].w - 2 - ctx->scrollbar_w;
							listview_clamp_scroll_x(lv, content_w);
							dirty_widget(ctx, (uint32_t)hi);
						}

					} else {
						uint32_t sid = hi >= 0 ? ctx->widgets[hi].parent_id : 0;
						while(sid) {
							int32_t si = find_widget_idx(ctx, sid);
							if(si < 0) {
								break;
							}
							if((ctx->widgets[si].type == MKGUI_VBOX || ctx->widgets[si].type == MKGUI_HBOX) && (ctx->widgets[si].flags & MKGUI_SCROLL)) {
								struct mkgui_box_scroll *bs = find_box_scroll(ctx, sid);
								if(bs && bs->content_w > ctx->rects[si].w) {
									int32_t max_scroll = bs->content_w - ctx->rects[si].w;
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
									dirty_widget(ctx, (uint32_t)si);
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
							int32_t sy = ctx->rects[bi].y + ctx->rects[bi].h - ctx->scrollbar_w;
							int32_t sx = ctx->rects[bi].x;
							int32_t sw = ctx->rects[bi].w;
							if(ctx->mouse_x >= sx && ctx->mouse_x < sx + sw && ctx->mouse_y >= sy && ctx->mouse_y < sy + ctx->scrollbar_w) {
								int32_t track = sw;
								int32_t total = bs->content_w;
								if(total < 1) {
									total = 1;
								}
								int32_t thumb = track * sw / total;
								if(thumb < sc(ctx, 20)) {
									thumb = sc(ctx, 20);
								}
								if(thumb > track) {
									thumb = track;
								}
								int32_t max_scroll = bs->content_w - sw;
								if(max_scroll < 1) {
									max_scroll = 1;
								}
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
						int32_t sx = ctx->rects[bi].x + ctx->rects[bi].w - ctx->scrollbar_w;
						int32_t sy = ctx->rects[bi].y;
						int32_t sh = ctx->rects[bi].h;
						if(ctx->mouse_x >= sx && ctx->mouse_x < sx + ctx->scrollbar_w && ctx->mouse_y >= sy && ctx->mouse_y < sy + sh) {
							int32_t track = sh;
							int32_t total = bs->content_h;
							if(total < 1) {
								total = 1;
							}
							int32_t thumb = track * sh / total;
							if(thumb < sc(ctx, 20)) {
								thumb = sc(ctx, 20);
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
							if(ctx->mouse_y >= thumb_y && ctx->mouse_y < thumb_y + thumb) {
								ctx->drag_scrollbar_id = bw->id;
								ctx->drag_scrollbar_offset = ctx->mouse_y - thumb_y;
							} else {
								int32_t dir = ctx->mouse_y < thumb_y ? -1 : 1;
								int32_t step = sh / 2;
								bs->scroll_y += dir * step;
								if(bs->scroll_y < 0) {
									bs->scroll_y = 0;
								}
								if(bs->scroll_y > max_scroll) {
									bs->scroll_y = max_scroll;
								}
							}
							dirty_widget(ctx, (uint32_t)bi);
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
						if(ctx->focus_id != hw->id) {
							pathbar_focus_lost(ctx);
						}
						ctx->focus_id = hw->id;

					} else {
						spinbox_focus_lost(ctx);
						pathbar_focus_lost(ctx);
						ctx->focus_id = 0;
					}

					ctx->press_id = hw->id;
					ctx->press_mod = pev.keymod;
					dirty_all(ctx);

					if(hw->type == MKGUI_INPUT) {
						struct mkgui_input_data *inp = find_input_data(ctx, hw->id);
						if(inp) {
							uint32_t now = mkgui_time_ms();
							uint32_t is_dblclick = (ctx->dblclick_id == hw->id && (now - ctx->dblclick_time) < 400);
							ctx->dblclick_id = hw->id;
							ctx->dblclick_time = now;
							if(is_dblclick) {
								uint32_t len = (uint32_t)strlen(inp->text);
								inp->sel_start = 0;
								inp->sel_end = len;
								inp->cursor = len;
								ctx->dblclick_id = 0;
							} else {
								char *display = inp->text;
								char masked_buf[MKGUI_MAX_TEXT];
								if(hw->style & MKGUI_PASSWORD) {
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
							}
							dirty_widget(ctx, (uint32_t)hi);
							input_scroll_to_cursor(ctx, hw->id);
						}
					}

					if(hw->type == MKGUI_IPINPUT) {
						handle_ipinput_click(ctx, hw->id, ctx->mouse_x);
					}

					if(hw->type == MKGUI_TOGGLE) {
						handle_toggle_click(ctx, ev, hw->id);
						return ev->type != MKGUI_EVENT_NONE;
					}

					if(hw->type == MKGUI_COMBOBOX) {
						if(closed_combobox_id == hw->id) {
							int32_t btn_x = ctx->rects[hi].x + ctx->rects[hi].w - sc(ctx, 24);
							if(ctx->mouse_x < btn_x) {
								handle_combobox_click(ctx, ev, hw->id);
							}
						} else {
							handle_combobox_click(ctx, ev, hw->id);
						}
					}

					if(hw->type == MKGUI_DATEPICKER) {
						handle_datepicker_click(ctx, hw->id);
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
							uint32_t hit = textarea_hit_pos(ctx, ta, ctx->rects[hi].x, ctx->rects[hi].y, ctx->rects[hi].h, ctx->mouse_x, ctx->mouse_y);
							if(textarea_has_selection(ta) && !(hw->style & MKGUI_READONLY)) {
								uint32_t lo, hi2;
								textarea_sel_range(ta, &lo, &hi2);
								if(hit >= lo && hit < hi2) {
									ctx->drag_text_id = hw->id;
									ctx->drag_text_drop_pos = hit;
									break;
								}
							}
							ta->cursor = hit;
							textarea_clear_selection(ta);
							ctx->drag_select_id = hw->id;
							dirty_widget(ctx, (uint32_t)hi);
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
						uint32_t hsb_hit = listview_hscrollbar_hit(ctx, (uint32_t)hi, ctx->mouse_x, ctx->mouse_y);
						if(hsb_hit == 1) {
							ctx->drag_scrollbar_id = hw->id;
							ctx->drag_scrollbar_offset = listview_hthumb_offset(ctx, (uint32_t)hi, ctx->mouse_x);
							ctx->drag_scrollbar_horiz = 1;

						} else if(hsb_hit == 2 || hsb_hit == 3) {
							struct mkgui_listview_data *hlv = find_listv_data(ctx, hw->id);
							if(hlv) {
								int32_t cw = ctx->rects[hi].w - 2 - ctx->scrollbar_w;
								hlv->scroll_x += (hsb_hit == 2 ? -1 : 1) * (cw / 2);
								listview_clamp_scroll_x(hlv, cw);
								dirty_widget(ctx, (uint32_t)hi);
							}

						} else {
						uint32_t sb_hit = listview_scrollbar_hit(ctx, (uint32_t)hi, ctx->mouse_x, ctx->mouse_y);
						if(sb_hit == 1) {
							ctx->drag_scrollbar_id = hw->id;
							ctx->drag_scrollbar_offset = listview_thumb_offset(ctx, (uint32_t)hi, ctx->mouse_y);
							ctx->drag_scrollbar_horiz = 0;

						} else if(sb_hit == 2 || sb_hit == 3) {
							listview_page_scroll(ctx, (uint32_t)hi, sb_hit == 2 ? -1 : 1);

						} else {
							int32_t div = listview_divider_hit(ctx, (uint32_t)hi, ctx->mouse_x, ctx->mouse_y);
							if(div >= 0) {
								struct mkgui_listview_data *lv = find_listv_data(ctx, hw->id);
								if(lv) {
									uint32_t logical = lv->col_order[div];
									uint32_t now_ms = mkgui_time_ms();
									if(ctx->divider_dblclick_id == hw->id && ctx->divider_dblclick_col == (int32_t)logical && (now_ms - ctx->divider_dblclick_time) < 400) {
										listview_autosize_col(ctx, lv, logical);
										ctx->divider_dblclick_id = 0;

									} else {
										ctx->divider_dblclick_id = hw->id;
										ctx->divider_dblclick_col = (int32_t)logical;
										ctx->divider_dblclick_time = now_ms;
										ctx->drag_col_resize_id = hw->id;
										ctx->drag_col_resize_col = (int32_t)logical;
										ctx->drag_col_resize_start_x = ctx->mouse_x;
										ctx->drag_col_resize_start_w = lv->columns[logical].width;
									}
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
											if(hw->style & MKGUI_MULTI_SELECT) {
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
												dirty_widget(ctx, (uint32_t)hi);
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
											dirty_widget(ctx, (uint32_t)hi);
											return 1;
										}
									}
								}
							}
						}
					}
					}

					if(hw->type == MKGUI_GRIDVIEW) {
						uint32_t gv_sb_hit = gridview_scrollbar_hit(ctx, (uint32_t)hi, ctx->mouse_x, ctx->mouse_y);
						if(gv_sb_hit == 1) {
							ctx->drag_scrollbar_id = hw->id;
							ctx->drag_scrollbar_offset = gridview_thumb_offset(ctx, (uint32_t)hi, ctx->mouse_y);

						} else if(gv_sb_hit == 2 || gv_sb_hit == 3) {
							struct mkgui_gridview_data *gv = find_gridv_data(ctx, hw->id);
							if(gv) {
								int32_t ghh = gv->header_height > 0 ? gv->header_height : ctx->row_height;
								int32_t gcontent_h = ctx->rects[hi].h - ghh - 2;
								gv->scroll_y += (gv_sb_hit == 2 ? -1 : 1) * gcontent_h;
								gridview_clamp_scroll(ctx, gv, gcontent_h);
								dirty_widget(ctx, (uint32_t)hi);
							}

						} else {
							int32_t gv_div = gridview_divider_hit(ctx, (uint32_t)hi, ctx->mouse_x, ctx->mouse_y);
							if(gv_div >= 0) {
								struct mkgui_gridview_data *gv = find_gridv_data(ctx, hw->id);
								if(gv) {
									uint32_t now_ms = mkgui_time_ms();
									if(ctx->divider_dblclick_id == hw->id && ctx->divider_dblclick_col == gv_div && (now_ms - ctx->divider_dblclick_time) < 400) {
										gridview_autosize_col(ctx, gv, (uint32_t)gv_div);
										ctx->divider_dblclick_id = 0;

									} else {
										ctx->divider_dblclick_id = hw->id;
										ctx->divider_dblclick_col = gv_div;
										ctx->divider_dblclick_time = now_ms;
										ctx->drag_col_resize_id = hw->id;
										ctx->drag_col_resize_col = gv_div;
										ctx->drag_col_resize_start_x = ctx->mouse_x;
										ctx->drag_col_resize_start_w = gv->columns[gv_div].width;
									}
								}

							} else {
								struct mkgui_gridview_data *gv = find_gridv_data(ctx, hw->id);
								if(gv) {
									int32_t ghh = gv->header_height > 0 ? gv->header_height : ctx->row_height;
									int32_t gcontent_y = ctx->rects[hi].y + ghh + 1;
									int32_t grow = (ctx->mouse_y - gcontent_y + gv->scroll_y) / ctx->row_height;
									int32_t gcol = gridview_col_at_x(gv, ctx->mouse_x, ctx->rects[hi].x + 1);
									if(grow >= 0 && grow < (int32_t)gv->row_count) {
										gv->selected_row = grow;
										gv->selected_col = gcol;
										gv->drag_source = grow;
										gv->drag_target = -1;
										gv->drag_start_y = ctx->mouse_y;
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
												dirty_widget(ctx, (uint32_t)hi);
												return 1;
											}
										}
										ev->type = MKGUI_EVENT_GRID_CLICK;
										ev->id = hw->id;
										ev->value = grow;
										ev->col = gcol;
										dirty_widget(ctx, (uint32_t)hi);
										return 1;
									}
								}
							}
						}
					}

					if(hw->type == MKGUI_RICHLIST) {
						uint32_t rl_sb_hit = richlist_scrollbar_hit(ctx, (uint32_t)hi, ctx->mouse_x, ctx->mouse_y);
						if(rl_sb_hit == 1) {
							ctx->drag_scrollbar_id = hw->id;
							ctx->drag_scrollbar_offset = richlist_thumb_offset(ctx, (uint32_t)hi, ctx->mouse_y);

						} else if(rl_sb_hit == 2 || rl_sb_hit == 3) {
							struct mkgui_richlist_data *rl = find_richlist_data(ctx, hw->id);
							if(rl) {
								int32_t content_h = ctx->rects[hi].h - 2;
								rl->scroll_y += (rl_sb_hit == 2 ? -1 : 1) * content_h;
								richlist_clamp_scroll(rl, content_h);
								dirty_widget(ctx, (uint32_t)hi);
							}

						} else {
							struct mkgui_richlist_data *rl = find_richlist_data(ctx, hw->id);
							if(rl) {
								int32_t row = richlist_row_hit(ctx, (uint32_t)hi, ctx->mouse_y);
								if(row >= 0) {
									uint32_t now = mkgui_time_ms();
									uint32_t is_dblclick = (ctx->dblclick_id == hw->id && ctx->dblclick_row == row && (now - ctx->dblclick_time) < 400);
									ctx->dblclick_id = hw->id;
									ctx->dblclick_row = row;
									ctx->dblclick_time = now;
									rl->selected_row = row;
									dirty_widget(ctx, (uint32_t)hi);
									if(is_dblclick) {
										ctx->dblclick_id = 0;
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
									dirty_widget(ctx, (uint32_t)hi);
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

					if(hw->type == MKGUI_PATHBAR) {
						if(handle_pathbar_click(ctx, ev, (uint32_t)hi, ctx->mouse_x, ctx->mouse_y)) {
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
							int32_t range = sd->max_val - sd->min_val;
							int32_t thumb_sz = sc(ctx, 10);
							int32_t new_val;
							ctx->drag_slider_offset = 0;
							if(hw->flags & MKGUI_VERTICAL) {
								int32_t ry = ctx->rects[hi].y;
								int32_t rh = ctx->rects[hi].h;
								int32_t track = rh - thumb_sz;
								int32_t thumb_y = ry + rh - thumb_sz - (range > 0 ? (int32_t)((int64_t)(sd->value - sd->min_val) * track / range) : 0);
								if(ctx->mouse_y >= thumb_y && ctx->mouse_y < thumb_y + thumb_sz) {
									ctx->drag_slider_offset = ctx->mouse_y - (thumb_y + thumb_sz / 2);
									new_val = sd->value;
								} else {
									int32_t pos = ctx->mouse_y - ry - thumb_sz / 2;
									new_val = sd->max_val - (track > 0 ? (int32_t)((int64_t)pos * range / track) : 0);
								}
							} else {
								int32_t rx = ctx->rects[hi].x;
								int32_t rw = ctx->rects[hi].w;
								int32_t track = rw - thumb_sz;
								int32_t thumb_x = rx + (range > 0 ? (int32_t)((int64_t)(sd->value - sd->min_val) * track / range) : 0);
								if(ctx->mouse_x >= thumb_x && ctx->mouse_x < thumb_x + thumb_sz) {
									ctx->drag_slider_offset = ctx->mouse_x - (thumb_x + thumb_sz / 2);
									new_val = sd->value;
								} else {
									int32_t pos = ctx->mouse_x - rx - thumb_sz / 2;
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
									dirty_widget(ctx, (uint32_t)hi);
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
								dirty_widget(ctx, (uint32_t)hi);
							}
						}
					}

				} else {
					spinbox_focus_lost(ctx);
					pathbar_focus_lost(ctx);
					ctx->focus_id = 0;
				}
			} break;

			// -=[ BUTTON RELEASE ]=-
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
						dirty_widget_id(ctx, tv->widget_id);
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
						dirty_widget_id(ctx, lv->widget_id);
					}
					lv->drag_active = 0;
					lv->drag_source = -1;
					lv->drag_target = -1;
				}
				for(uint32_t gvi = 0; gvi < ctx->gridview_count; ++gvi) {
					struct mkgui_gridview_data *gv = &ctx->gridviews[gvi];
					if(gv->drag_active && gv->drag_target >= 0) {
						int32_t src = gv->drag_source;
						int32_t tgt = gv->drag_target;
						gv->drag_active = 0;
						gv->drag_source = -1;
						gv->drag_target = -1;
						dirty_all(ctx);
						ev->type = MKGUI_EVENT_GRIDVIEW_REORDER;
						ev->id = gv->widget_id;
						ev->value = src;
						ev->col = tgt;
						return 1;
					}
					if(gv->drag_active) {
						ev->type = MKGUI_EVENT_DRAG_END;
						ev->id = gv->widget_id;
						dirty_widget_id(ctx, gv->widget_id);
					}
					gv->drag_active = 0;
					gv->drag_source = -1;
					gv->drag_target = -1;
				}

				ctx->drag_select_id = 0;

				if(ctx->drag_text_id) {
					uint32_t dtid = ctx->drag_text_id;
					uint32_t drop = ctx->drag_text_drop_pos;
					ctx->drag_text_id = 0;
					struct mkgui_textarea_data *ta = find_textarea_data(ctx, dtid);
					if(ta && textarea_has_selection(ta)) {
						uint32_t lo, hi2;
						textarea_sel_range(ta, &lo, &hi2);
						if(drop < lo || drop >= hi2) {
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
								dirty_widget_id(ctx, dtid);
								return 1;
							}
						} else {
							ta->cursor = drop;
							textarea_clear_selection(ta);
						}
					}
					dirty_widget_id(ctx, dtid);
					break;
				}

				if(ctx->drag_col_resize_id) {
					uint32_t dcr_id = ctx->drag_col_resize_id;
					ctx->drag_col_resize_id = 0;
					dirty_widget_id(ctx, dcr_id);
					break;
				}

				if(ctx->drag_col_id) {
					uint32_t dcid = ctx->drag_col_id;
					int32_t src = ctx->drag_col_src;
					int32_t dx = pev.x - ctx->drag_col_start_x;
					ctx->drag_col_id = 0;
					ctx->drag_col_src = -1;
					dirty_widget_id(ctx, dcid);

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
						dirty_widget(ctx, (uint32_t)dsi);
						return 1;
					}
				}
				ctx->drag_scrollbar_id = 0;
				if(was_press) {
					dirty_widget_id(ctx, was_press);
				}

				if(was_press) {
					int32_t pi = find_widget_idx(ctx, was_press);
					if(pi >= 0) {
						uint32_t pt = ctx->widgets[pi].type;
						if(pt == MKGUI_SLIDER) {
							struct mkgui_slider_data *sd = find_slider_data(ctx, was_press);
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

					int32_t hi = hit_test(ctx, pev.x, pev.y);
					if(hi >= 0 && ctx->widgets[hi].id == was_press) {
						struct mkgui_widget *hw = &ctx->widgets[hi];

						if(hw->type == MKGUI_BUTTON) {
							ev->type = MKGUI_EVENT_CLICK;
							ev->id = hw->id;
							return 1;
						}

						if(hw->type == MKGUI_LABEL && (hw->style & MKGUI_LINK)) {
							ev->type = MKGUI_EVENT_CLICK;
							ev->id = hw->id;
							return 1;
						}

						if(hw->type == MKGUI_GROUP && (hw->style & MKGUI_COLLAPSIBLE)) {
							hw->style ^= MKGUI_COLLAPSED;
							dirty_all(ctx);
							ev->type = MKGUI_EVENT_CLICK;
							ev->id = hw->id;
							ev->value = (hw->style & MKGUI_COLLAPSED) ? 1 : 0;
							return 1;
						}

						if(hw->type == MKGUI_CHECKBOX) {
							hw->style ^= MKGUI_CHECKED;
							ev->type = MKGUI_EVENT_CHECKBOX_CHANGED;
							ev->id = hw->id;
							ev->value = (hw->style & MKGUI_CHECKED) ? 1 : 0;
							return 1;
						}

						if(hw->type == MKGUI_RADIO) {
							return handle_radio_click(ctx, ev, hw);
						}
					}
				}
			} break;

			// -=[ KEY ]=-
			case MKGUI_PLAT_KEY: {
				uint32_t ks = pev.keysym;

				if(ctx->popup_count > 0 && ctx->popups[ctx->popup_count - 1].widget_id == MKGUI_CTXMENU_POPUP_ID) {
					struct mkgui_popup *cp = &ctx->popups[ctx->popup_count - 1];
					if(ks == MKGUI_KEY_ESCAPE) {
						popup_destroy_all(ctx);
						dirty_all(ctx);
						break;
					}
					if(ks == MKGUI_KEY_UP) {
						int32_t next = ctxmenu_next_item(ctx, cp->hover_item >= 0 ? cp->hover_item : (int32_t)ctx->ctxmenu_count, -1);
						if(next != cp->hover_item) {
							cp->hover_item = next;
							cp->dirty = 1;
							dirty_all(ctx);
						}
						break;
					}
					if(ks == MKGUI_KEY_DOWN) {
						int32_t next = ctxmenu_next_item(ctx, cp->hover_item, 1);
						if(next != cp->hover_item) {
							cp->hover_item = next;
							cp->dirty = 1;
							dirty_all(ctx);
						}
						break;
					}
					if(ks == MKGUI_KEY_RETURN || ks == MKGUI_KEY_SPACE) {
						if(cp->hover_item >= 0) {
							struct mkgui_ctxmenu_item *it = ctxmenu_item_at(ctx, cp->hover_item);
							if(it && !(it->flags & MKGUI_DISABLED)) {
								if(it->flags & MKGUI_MENU_CHECK) {
									it->flags ^= MKGUI_CHECKED;
								} else if(it->flags & MKGUI_MENU_RADIO) {
									for(uint32_t ri = 0; ri < ctx->ctxmenu_count; ++ri) {
										ctx->ctxmenu_items[ri].flags &= ~MKGUI_CHECKED;
									}
									it->flags |= MKGUI_CHECKED;
								}
								ev->type = MKGUI_EVENT_CONTEXT_MENU;
								ev->id = it->id;
								ev->value = (it->flags & MKGUI_CHECKED) ? 1 : 0;
								popup_destroy_all(ctx);
								dirty_all(ctx);
								return 1;
							}
						}
						break;
					}
					break;
				}

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
					pathbar_focus_lost(ctx);
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
								dirty_widget_id(ctx, ctx->focus_id);
								input_scroll_to_cursor(ctx, ctx->focus_id);
							}

						} else if(fw->type == MKGUI_COMBOBOX) {
							struct mkgui_combobox_data *cb = find_combobox_data(ctx, ctx->focus_id);
							if(cb) {
								combobox_select_all(cb);
								dirty_widget_id(ctx, ctx->focus_id);
							}

						} else if(fw->type == MKGUI_TEXTAREA) {
							struct mkgui_textarea_data *ta = find_textarea_data(ctx, ctx->focus_id);
							if(ta) {
								ta->sel_start = 0;
								ta->sel_end = ta->text_len;
								ta->cursor = ta->text_len;
								dirty_widget_id(ctx, ctx->focus_id);
							}
						}
						break;
					}

					if(fw && (ks == 'c' || ks == 'C')) {
						if(fw->type == MKGUI_INPUT && !(fw->style & MKGUI_PASSWORD)) {
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

					if(fw && (ks == 'x' || ks == 'X') && !(fw->style & MKGUI_READONLY)) {
						if(fw->type == MKGUI_INPUT && !(fw->style & MKGUI_PASSWORD)) {
							struct mkgui_input_data *inp = find_input_data(ctx, ctx->focus_id);
							if(inp && input_has_selection(inp)) {
								uint32_t lo, hi;
								input_sel_range(inp, &lo, &hi);
								platform_clipboard_set(ctx, inp->text + lo, hi - lo);
								input_delete_selection(inp);
								dirty_widget_id(ctx, ctx->focus_id);
								input_scroll_to_cursor(ctx, ctx->focus_id);
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
								dirty_widget_id(ctx, ctx->focus_id);
								textarea_scroll_to_cursor(ctx, ctx->focus_id);
								ev->type = MKGUI_EVENT_TEXTAREA_CHANGED;
								ev->id = ctx->focus_id;
								return 1;
							}
						}
						break;
					}

					if(fw && (ks == 'v' || ks == 'V') && !(fw->style & MKGUI_READONLY)) {
						if(fw->type == MKGUI_INPUT) {
							char clip_buf[MKGUI_CLIP_MAX];
							uint32_t clip_len = platform_clipboard_get(ctx, clip_buf, sizeof(clip_buf));
							if(clip_len > 0) {
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
										dirty_widget_id(ctx, ctx->focus_id);
										input_scroll_to_cursor(ctx, ctx->focus_id);
										ev->type = MKGUI_EVENT_INPUT_CHANGED;
										ev->id = ctx->focus_id;
										return 1;
									}
								}
							}

						} else if(fw->type == MKGUI_TEXTAREA) {
							uint32_t clip_len = 0;
							char *clip_buf = platform_clipboard_get_alloc(ctx, &clip_len);
							if(clip_buf && clip_len > 0) {
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
									dirty_widget_id(ctx, ctx->focus_id);
									textarea_scroll_to_cursor(ctx, ctx->focus_id);
									ev->type = MKGUI_EVENT_TEXTAREA_CHANGED;
									ev->id = ctx->focus_id;
									free(clip_buf);
									return 1;
								}
							}
							free(clip_buf);
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

							case MKGUI_GRIDVIEW: {
								if(handle_gridview_key(ctx, ev, ks)) {
									return 1;
								}
							} break;

							case MKGUI_RICHLIST: {
								if(handle_richlist_key(ctx, ev, ks)) {
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

							case MKGUI_PATHBAR: {
								if(handle_pathbar_key(ctx, ev, ks, pev.keymod, pev.text, pev.text_len)) {
									return 1;
								}
							} break;

							case MKGUI_IPINPUT: {
								if(handle_ipinput_key(ctx, ev, ks, pev.keymod, pev.text, pev.text_len)) {
									return 1;
								}
							} break;

							case MKGUI_TOGGLE: {
								if(handle_toggle_key(ctx, ev, ks)) {
									return 1;
								}
							} break;

							case MKGUI_COMBOBOX: {
								if(handle_combobox_key(ctx, ev, ks, pev.keymod, pev.text, pev.text_len)) {
									return 1;
								}
							} break;

							case MKGUI_DATEPICKER: {
								if(handle_datepicker_key(ctx, ev, ks, pev.keymod, pev.text, pev.text_len)) {
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
				ctx->close_requested = 1;
				return 1;
			} break;

			// -=[ FOCUS OUT ]=-
			case MKGUI_PLAT_FOCUS_OUT: {
				if(ctx->popup_count > 0) {
					popup_destroy_all(ctx);
					dirty_all(ctx);
				}
			} break;

			// -=[ LEAVE ]=-
			case MKGUI_PLAT_LEAVE: {
				if(popup_idx >= 0) {
					uint32_t has_child_popup = (uint32_t)popup_idx + 1 < ctx->popup_count;
					if(!has_child_popup) {
						ctx->popups[popup_idx].hover_item = -1;
						ctx->popups[popup_idx].dirty = 1;
					}
				}
				for(uint32_t pbi = 0; pbi < ctx->pathbar_count; ++pbi) {
					if(ctx->pathbars[pbi].hover_seg >= 0) {
						ctx->pathbars[pbi].hover_seg = -1;
						dirty_widget_id(ctx, ctx->pathbars[pbi].widget_id);
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
			if(w->type == MKGUI_PROGRESS && (w->style & MKGUI_SHIMMER)) {
				struct mkgui_progress_data *pd = find_progress_data(ctx, w->id);
				if(pd && pd->value > 0 && pd->value < pd->max_val) {
					dirty_widget(ctx, i);
				}
			}
		}
	}

	if(ctx->tooltip_id && !ctx->tooltip_shown) {
		uint32_t elapsed = mkgui_time_ms() - ctx->tooltip_start_ms;
		if(elapsed >= MKGUI_TOOLTIP_DELAY_MS) {
			dirty_all(ctx);
		}
	}

	return ev->type != MKGUI_EVENT_NONE;
}

// [=]===^=[ mkgui_flush ]========================================[=]
static void mkgui_flush(struct mkgui_ctx *ctx) {
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
			for(uint32_t d = 0; d < ctx->dirty_count; ++d) {
				platform_blit_region(ctx,
					ctx->dirty_rects[d].x, ctx->dirty_rects[d].y,
					ctx->dirty_rects[d].w, ctx->dirty_rects[d].h);
			}
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
			if(p->widget_id == MKGUI_CTXMENU_POPUP_ID) {
				render_ctxmenu_popup(ctx, p, p->hover_item);
				flush_text_popup(ctx, p);
				platform_popup_blit(ctx, p);

			} else {
				struct mkgui_widget *pw = find_widget(ctx, p->widget_id);
				if(pw && pw->type == MKGUI_DROPDOWN) {
					struct mkgui_dropdown_data *dd = find_dropdown_data(ctx, pw->id);
					if(dd) {
						render_dropdown_popup(ctx, p, dd, p->hover_item);
						flush_text_popup(ctx, p);
						platform_popup_blit(ctx, p);
					}

				} else if(pw && pw->type == MKGUI_COMBOBOX) {
					struct mkgui_combobox_data *cb = find_combobox_data(ctx, pw->id);
					if(cb) {
						render_combobox_popup(ctx, p, cb, p->hover_item);
						flush_text_popup(ctx, p);
						platform_popup_blit(ctx, p);
					}

				} else if(pw && pw->type == MKGUI_DATEPICKER) {
					struct mkgui_datepicker_data *dp = find_datepicker_data(ctx, pw->id);
					if(dp) {
						int32_t hover_day = dp->cal_hover > 0 ? dp->cal_hover : -1;
						render_datepicker_popup(ctx, p, dp, hover_day);
						flush_text_popup(ctx, p);
						platform_popup_blit(ctx, p);
					}

				} else if(pw && pw->type == MKGUI_MENUITEM) {
					render_menu_popup(ctx, p, pw->id, p->hover_item);
					flush_text_popup(ctx, p);
					platform_popup_blit(ctx, p);
				}
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
			if(w->type == MKGUI_PROGRESS && (w->style & MKGUI_SHIMMER)) {
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
}

// [=]===^=[ mkgui_wait ]=========================================[=]
MKGUI_API void mkgui_wait(struct mkgui_ctx *ctx) {
	MKGUI_CHECK(ctx);
	mkgui_flush(ctx);

	if(ctx->close_requested) {
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
	int32_t timeout = any_anim ? 16 : -1;
	if(ctx->tooltip_id && !ctx->tooltip_shown) {
		uint32_t elapsed = mkgui_time_ms() - ctx->tooltip_start_ms;
		if(elapsed < MKGUI_TOOLTIP_DELAY_MS) {
			int32_t remaining = (int32_t)(MKGUI_TOOLTIP_DELAY_MS - elapsed);
			if(timeout < 0 || remaining < timeout) {
				timeout = remaining;
			}
		}
	}
	platform_wait_event(ctx, timeout);
}

// [=]===^=[ mkgui_add_timer ]=====================================[=]
MKGUI_API uint32_t mkgui_add_timer(struct mkgui_ctx *ctx, uint64_t interval_ns, mkgui_timer_cb cb, void *userdata) {
	MKGUI_CHECK_VAL(ctx, 0);
	if(!cb) {
		return 0;
	}
	if(ctx->timer_count >= MKGUI_MAX_TIMERS) {
		return 0;
	}
	struct mkgui_timer *t = &ctx->timers[ctx->timer_count];
	memset(t, 0, sizeof(*t));
	t->id = ++ctx->timer_next_id;
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
	++ctx->timer_count;
	return t->id;
}

// [=]===^=[ mkgui_remove_timer ]==================================[=]
MKGUI_API void mkgui_remove_timer(struct mkgui_ctx *ctx, uint32_t timer_id) {
	MKGUI_CHECK(ctx);
	for(uint32_t i = 0; i < ctx->timer_count; ++i) {
		if(ctx->timers[i].id == timer_id) {
#ifdef _WIN32
			if(ctx->timers[i].handle) {
				CancelWaitableTimer(ctx->timers[i].handle);
				CloseHandle(ctx->timers[i].handle);
			}
#else
			if(ctx->timers[i].fd >= 0) {
				close(ctx->timers[i].fd);
			}
#endif
			ctx->timers[i] = ctx->timers[--ctx->timer_count];
			return;
		}
	}
}

// [=]===^=[ mkgui_run ]===========================================[=]
MKGUI_API void mkgui_run(struct mkgui_ctx *ctx, mkgui_event_cb cb, void *userdata) {
	MKGUI_CHECK(ctx);
	while(!ctx->close_requested) {
		struct mkgui_event ev;
		while(mkgui_poll(ctx, &ev)) {
			if(cb) {
				cb(ctx, &ev, userdata);
			}
		}
		mkgui_wait(ctx);
	}
}

// [=]===^=[ mkgui_quit ]==========================================[=]
MKGUI_API void mkgui_quit(struct mkgui_ctx *ctx) {
	MKGUI_CHECK(ctx);
	ctx->close_requested = 1;
}

// ---------------------------------------------------------------------------
// Dialog windows
// ---------------------------------------------------------------------------

#include "mkgui_dialogs.c"
#include "mkgui_filedialog.c"
#include "mkgui_iconbrowser.c"
#include "mkgui_colorpicker.c"
