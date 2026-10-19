// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT

#ifndef MKGUI_INTERNAL_H
#define MKGUI_INTERNAL_H

#include <stddef.h>
#include <time.h>
#include "mkgui.h"

#ifdef _WIN32
#include <windows.h>
#endif

#define MKGUI_GLYPH_FIRST     32
#define MKGUI_GLYPH_LAST      255
#define MKGUI_GLYPH_COUNT     (MKGUI_GLYPH_LAST - MKGUI_GLYPH_FIRST + 1)
#define MKGUI_UNDO_MAX 32
#define MKGUI_MAX_TEXTAREA_LINES 4096
#define MKGUI_MAX_TEXTAREA_LINE  1024
#define MKGUI_CTXMENU_POPUP_ID   UINT32_MAX
#define MKGUI_MAX_WINDOWS 16
#define MKGUI_SLIDER_WEDGE_MAX_W 10

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
	uint32_t undo_start;
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

struct mkgui_treelist_node {
	uint32_t parent;         // node index, or 0xffffffff for a root
	uint32_t first_child;
	uint32_t last_child;
	uint32_t next_sibling;
	uint32_t depth;
	uint32_t expanded;
	uint32_t has_children;
	void *userdata;
	char *cells;                        // blob: col_count NUL-terminated strings
	uint16_t cell_off[MKGUI_MAX_COLS];  // offset of each column string in cells
};

struct mkgui_treelist_data {
	uint32_t widget_id;
	uint32_t col_count;
	struct mkgui_column columns[MKGUI_MAX_COLS];
	uint32_t col_order[MKGUI_MAX_COLS];
	struct mkgui_treelist_node *nodes;
	uint32_t node_count;
	uint32_t node_cap;
	uint32_t root_first;
	uint32_t root_last;
	uint32_t *visible;       // node indices in display order
	uint32_t visible_count;
	uint32_t visible_cap;
	uint32_t dirty_visible;
	int32_t selected;        // node handle (1-based), -1 for none
	int32_t multi_sel[MKGUI_MAX_MULTI_SEL];  // node handles, sorted ascending
	uint32_t multi_sel_count;
	int32_t scroll_x;
	int32_t scroll_y;
	int32_t sort_col;
	int32_t sort_dir;
	int32_t header_height;
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
	uint32_t undo_start;
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
	struct mkwin_glview *mkwin;
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

struct mkgui_popup {
	struct mkwin_popup *mkwin;
	uint32_t *pixels;
	int32_t x, y, w, h;
	uint32_t widget_id;
	uint32_t active;
	uint32_t dirty;
	int32_t hover_item;
};

struct mkgui_icon {
	char name[MKGUI_ICON_NAME_LEN];
	// Logical size for svg_rerasterize_all on scale/theme change. 0 for raster icons.
	int32_t requested_size;
	// svg_sources[] index backing this icon, or UINT32_MAX for raster/uncached.
	uint32_t source_idx;
	uint32_t *pixels;
	int32_t w, h;
	uint32_t atlas_offset;
	uint32_t custom;
	uint32_t library;
};

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

struct vm_arena {
	uint8_t *base;
	size_t reserved;
	size_t committed;
};

struct mkgui_id_slot {
	uint32_t id;
	uint32_t idx;
};

struct mkgui_window_arenas {
	struct vm_arena widgets;
	struct vm_arena rects;
	struct vm_arena id_index;
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
	struct vm_arena treelists;
};

struct mkgui_window {
	struct mkgui_ctx *ctx;        // back-pointer to the owning application ctx
	struct mkwin_window *mkwin;   // window/input/blit handle (mkwin library)

	uint32_t *pixels;             // published by mkwin (see struct mkwin_host at create)
	int32_t win_w, win_h;         // published by mkwin

	struct mkgui_window_arenas arenas;

	struct mkgui_widget *widgets;
	uint32_t widget_count;
	uint32_t widget_cap;

	// O(1) id->index map over widgets[]. id_index_cap is a power-of-two slot count;
	// id_index_dirty forces a rebuild after add/remove (indices shift).
	struct mkgui_id_slot *id_index;
	uint32_t id_index_cap;
	uint32_t id_index_dirty;

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
	struct mkgui_treelist_data *treelists;
	uint32_t treelist_count, treelist_cap;

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
	struct mkgui_rect previous_dirty_rects[32];
	uint32_t previous_dirty_count;
	uint32_t previous_dirty_full;
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
	uint32_t destroy_pending;
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

	uint32_t drop_enabled;        // drop file storage is owned by mkwin

	double perf_acquire_us;
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

	char *clip_text;
	uint32_t clip_len;
	uint32_t clip_cap;

	struct mkgui_ctxmenu_item ctxmenu_items[MKGUI_MAX_CTXMENU];
	uint32_t ctxmenu_count;
	int32_t ctxmenu_x, ctxmenu_y;

	char app_class[64];
};

struct mkgui_ctx {
	struct mkgui_window *windows[MKGUI_MAX_WINDOWS];
	uint32_t window_count;
	uint32_t should_quit;          // app-wide quit flag
	char app_class[64];            // default WM_CLASS for new windows
	struct mkgui_theme theme;      // default theme for new windows
	float scale;                   // default scale for new windows
	struct mkgui_window *primary;  // window whose close ends mkgui_ctx_run
};

struct mkgui_layout_hash_entry {
	uint32_t id;
	uint32_t idx;
};

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

enum {
	MKGUI_STYLE_RAISED,
	MKGUI_STYLE_SUNKEN,
};

// NOTE(peter): Bump the private bridge revision when a shared internal type or table layout changes.
struct mkgui_editor_api {
	void (*accel_format_text)(uint32_t keymod, uint32_t keysym, char *buf, uint32_t buf_size);
	uint32_t (*blend_pixel)(uint32_t dst, uint32_t color, uint8_t alpha);
	void (*dirty_all)(struct mkgui_window *win);
	void (*draw_aa_circle_fill)(uint32_t *buf, int32_t bw, int32_t bh, int32_t cx, int32_t cy, int32_t radius, uint32_t color);
	void (*draw_aa_circle_ring)(uint32_t *buf, int32_t bw, int32_t bh, int32_t cx, int32_t cy, int32_t outer_r, int32_t inner_r, uint32_t fill, uint32_t border);
	void (*draw_aa_line)(uint32_t *buf, int32_t bw, int32_t bh, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color, int32_t thickness);
	void (*draw_hline)(uint32_t *buf, int32_t bw, int32_t bh, int32_t x, int32_t y, int32_t w, uint32_t color);
	void (*draw_icon)(uint32_t *buf, int32_t bw, int32_t bh, struct mkgui_icon *icon, int32_t x, int32_t y, int32_t cx1, int32_t cy1, int32_t cx2, int32_t cy2);
	void (*draw_patch)(struct mkgui_window *win, uint32_t style, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t fill, uint32_t border);
	void (*draw_rect_border)(uint32_t *buf, int32_t bw, int32_t bh, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
	void (*draw_rect_dashed)(uint32_t *buf, int32_t bw, int32_t bh, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color, int32_t dash);
	void (*draw_rect_fill)(uint32_t *buf, int32_t bw, int32_t bh, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
	void (*draw_rounded_rect)(uint32_t *buf, int32_t bw, int32_t bh, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t fill, uint32_t border, int32_t radius);
	void (*draw_rounded_rect_fill)(uint32_t *buf, int32_t bw, int32_t bh, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color, int32_t radius);
	void (*draw_triangle_aa)(uint32_t *buf, int32_t bw, int32_t bh, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color);
	void (*draw_vline)(uint32_t *buf, int32_t bw, int32_t bh, int32_t x, int32_t y, int32_t h, uint32_t color);
	struct mkgui_widget *(*find_widget)(struct mkgui_window *win, uint32_t id);
	int32_t (*find_widget_idx)(struct mkgui_window *win, uint32_t id);
	void (*flush_text)(struct mkgui_window *win);
	int32_t (*icon_resolve)(struct mkgui_window *win, const char *name);
	void (*lc_build_index)(struct layout_ctx *lc);
	void (*lc_layout_node)(struct mkgui_window *win, struct layout_ctx *lc, uint32_t idx);
	void (*lc_window_min_size)(struct mkgui_window *win, struct layout_ctx *lc, uint32_t win_idx, int32_t *out_w, int32_t *out_h);
	void (*push_text)(int32_t x, int32_t y, char *text, uint32_t color);
	void (*push_text_clip)(int32_t x, int32_t y, char *text, uint32_t color, int32_t cx1, int32_t cy1, int32_t cx2, int32_t cy2);
	void (*render_widget)(struct mkgui_window *win, uint32_t idx);
	int32_t (*sc)(struct mkgui_window *win, int32_t px);
	void (*slider_draw_wedge_h)(uint32_t *buf, int32_t bw, int32_t bh, int32_t x, int32_t y, int32_t w, int32_t col_start, int32_t col_count, int32_t max_h, uint32_t color);
	void (*slider_draw_wedge_v)(uint32_t *buf, int32_t bw, int32_t bh, int32_t x, int32_t y, int32_t h, int32_t row_start, int32_t row_count, int32_t max_w, uint32_t color);
	int32_t (*text_width)(struct mkgui_window *win, char *text);
	struct mkgui_dropdown_data *(*find_dropdown_data)(struct mkgui_window *win, uint32_t id);
	struct mkgui_treeview_data *(*find_treeview_data)(struct mkgui_window *win, uint32_t id);
	struct mkgui_split_data *(*find_split_data)(struct mkgui_window *win, uint32_t id);
	struct mkgui_listview_data *(*find_listv_data)(struct mkgui_window *win, uint32_t id);
	struct mkgui_icon **icons;
	int32_t *render_clip_x1;
	int32_t *render_clip_y1;
	int32_t *render_clip_x2;
	int32_t *render_clip_y2;
};

#ifdef MKGUI_LIBRARY
MKGUI_API struct mkgui_editor_api *mkgui_editor_api_v2(void);
#endif

#endif
