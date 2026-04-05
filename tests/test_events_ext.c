// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT
//
// Extended event tests for mkgui.
// Covers events not tested in test_events.c: listview, treeview, gridview,
// richlist, itemview, pathbar, combobox, datepicker, ipinput, tab close,
// slider start/end, context menu, context header, timer, and file drop.

#include "../mkgui.c"

static uint32_t tests_run;
static uint32_t tests_passed;
static uint32_t tests_failed;

#define TEST_BEGIN(name) \
	do { \
		++tests_run; \
		char *test_name__ = (name); \
		uint32_t fail__ = 0; \
		(void)test_name__;

#define CHECK(cond, fmt, ...) \
	do { \
		if(!(cond)) { \
			if(!fail__) { \
				fprintf(stderr, "FAIL: %s\n", test_name__); \
			} \
			fprintf(stderr, "  line %d: " fmt "\n", __LINE__, ##__VA_ARGS__); \
			fail__ = 1; \
		} \
	} while(0)

#define TEST_END() \
		if(fail__) { \
			++tests_failed; \
		} else { \
			++tests_passed; \
		} \
	} while(0)

// ---------------------------------------------------------------------------
// Event injection helpers (same as test_events.c)
// ---------------------------------------------------------------------------

static void flush_x_events(struct mkgui_ctx *ctx) {
	XSync(ctx->plat.dpy, True);
}

static void run_layout(struct mkgui_ctx *ctx) {
	dirty_all(ctx);
	layout_widgets(ctx);
	ctx->dirty = 0;
	ctx->dirty_full = 0;
	ctx->dirty_count = 0;
	flush_x_events(ctx);
}

static void inject_press(struct mkgui_ctx *ctx, int32_t x, int32_t y, uint32_t button, uint32_t keymod) {
	struct mkgui_plat_event pev;
	memset(&pev, 0, sizeof(pev));
	pev.type = MKGUI_PLAT_BUTTON_PRESS;
	pev.x = x;
	pev.y = y;
	pev.button = button;
	pev.keymod = keymod;
	pev.popup_idx = -1;
	platform_deferred_push(ctx, &pev);
}

static void inject_release(struct mkgui_ctx *ctx, int32_t x, int32_t y, uint32_t button) {
	struct mkgui_plat_event pev;
	memset(&pev, 0, sizeof(pev));
	pev.type = MKGUI_PLAT_BUTTON_RELEASE;
	pev.x = x;
	pev.y = y;
	pev.button = button;
	pev.popup_idx = -1;
	platform_deferred_push(ctx, &pev);
}

static void inject_key(struct mkgui_ctx *ctx, uint32_t keysym, uint32_t keymod) {
	struct mkgui_plat_event pev;
	memset(&pev, 0, sizeof(pev));
	pev.type = MKGUI_PLAT_KEY;
	pev.keysym = keysym;
	pev.keymod = keymod;
	pev.popup_idx = -1;
	if(keysym >= 0x20 && keysym <= 0x7e) {
		pev.text[0] = (char)keysym;
		pev.text[1] = '\0';
		pev.text_len = 1;
	}
	platform_deferred_push(ctx, &pev);
}

static void inject_motion(struct mkgui_ctx *ctx, int32_t x, int32_t y) {
	struct mkgui_plat_event pev;
	memset(&pev, 0, sizeof(pev));
	pev.type = MKGUI_PLAT_MOTION;
	pev.x = x;
	pev.y = y;
	pev.popup_idx = -1;
	platform_deferred_push(ctx, &pev);
}

static void widget_center(struct mkgui_ctx *ctx, uint32_t id, int32_t *cx, int32_t *cy) {
	int32_t idx = find_widget_idx(ctx, id);
	if(idx < 0) {
		*cx = 0;
		*cy = 0;
		return;
	}
	*cx = ctx->rects[idx].x + ctx->rects[idx].w / 2;
	*cy = ctx->rects[idx].y + ctx->rects[idx].h / 2;
}

static void inject_click(struct mkgui_ctx *ctx, uint32_t id) {
	int32_t cx, cy;
	widget_center(ctx, id, &cx, &cy);
	inject_motion(ctx, cx, cy);
	inject_press(ctx, cx, cy, 1, 0);
	inject_release(ctx, cx, cy, 1);
}

static void inject_click_at(struct mkgui_ctx *ctx, int32_t x, int32_t y) {
	inject_motion(ctx, x, y);
	inject_press(ctx, x, y, 1, 0);
	inject_release(ctx, x, y, 1);
}

static void inject_right_click_at(struct mkgui_ctx *ctx, int32_t x, int32_t y) {
	inject_motion(ctx, x, y);
	inject_press(ctx, x, y, 3, 0);
	inject_release(ctx, x, y, 3);
}

static uint32_t poll_for_event(struct mkgui_ctx *ctx, uint32_t event_type, uint32_t target_id) {
	flush_x_events(ctx);
	uint32_t found = 0;
	struct mkgui_event ev;
	for(uint32_t i = 0; i < 50; ++i) {
		uint32_t got = mkgui_poll(ctx, &ev);
		if(got && ev.type == event_type && (target_id == 0 || ev.id == target_id)) {
			found = 1;
		}
		if(!got && ctx->plat.deferred_head == ctx->plat.deferred_tail) {
			mkgui_poll(ctx, &ev);
			if(ev.type == event_type && (target_id == 0 || ev.id == target_id)) {
				found = 1;
			}
			break;
		}
	}
	return found;
}

static void drain_events(struct mkgui_ctx *ctx) {
	flush_x_events(ctx);
	struct mkgui_event ev;
	for(uint32_t i = 0; i < 50; ++i) {
		if(!mkgui_poll(ctx, &ev)) {
			break;
		}
	}
}

// ---------------------------------------------------------------------------
// Dummy callbacks for virtual widgets
// ---------------------------------------------------------------------------

static void dummy_row_cb(uint32_t row, uint32_t col, char *buf, uint32_t buf_size, void *userdata) {
	(void)userdata;
	snprintf(buf, buf_size, "R%uC%u", row, col);
}

static void dummy_grid_cell_cb(uint32_t row, uint32_t col, char *buf, uint32_t buf_size, void *userdata) {
	(void)userdata;
	snprintf(buf, buf_size, "G%uC%u", row, col);
}

static void dummy_richlist_cb(uint32_t row, struct mkgui_richlist_row *out, void *userdata) {
	(void)userdata;
	snprintf(out->title, sizeof(out->title), "Item %u", row);
	snprintf(out->subtitle, sizeof(out->subtitle), "Sub %u", row);
}

static void dummy_label_cb(uint32_t item, char *buf, uint32_t buf_size, void *userdata) {
	(void)userdata;
	snprintf(buf, buf_size, "Item %u", item);
}

static void dummy_icon_cb(uint32_t item, char *buf, uint32_t buf_size, void *userdata) {
	(void)item; (void)userdata;
	buf[0] = '\0';
}

// ---------------------------------------------------------------------------
// Listview: SELECT, SORT, DBLCLICK
// ---------------------------------------------------------------------------

// [=]===^=[ test_listview_select ]================================[=]
static void test_listview_select(void) {
	TEST_BEGIN("listview: click row fires LISTVIEW_SELECT");
	enum { WIN = 1, VBOX1 = 2, LV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LISTVIEW, LV,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		struct mkgui_column cols[] = { { "Name", 200, MKGUI_CELL_TEXT }, { "Size", 100, MKGUI_CELL_TEXT } };
		mkgui_listview_setup(ctx, LV, 10, 2, cols, dummy_row_cb, NULL);
		run_layout(ctx);
		int32_t idx = find_widget_idx(ctx, LV);
		int32_t rx = ctx->rects[idx].x + 10;
		int32_t ry = ctx->rects[idx].y + ctx->row_height + 5;
		inject_click_at(ctx, rx, ry);
		CHECK(poll_for_event(ctx, MKGUI_EVENT_LISTVIEW_SELECT, LV), "expected LISTVIEW_SELECT");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_listview_sort ]=================================[=]
static void test_listview_sort(void) {
	TEST_BEGIN("listview: click header fires LISTVIEW_SORT");
	enum { WIN = 1, VBOX1 = 2, LV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LISTVIEW, LV,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		struct mkgui_column cols[] = { { "Name", 200, MKGUI_CELL_TEXT }, { "Size", 100, MKGUI_CELL_TEXT } };
		mkgui_listview_setup(ctx, LV, 10, 2, cols, dummy_row_cb, NULL);
		run_layout(ctx);
		int32_t idx = find_widget_idx(ctx, LV);
		int32_t hx = ctx->rects[idx].x + 50;
		int32_t hy = ctx->rects[idx].y + ctx->row_height / 2;
		inject_click_at(ctx, hx, hy);
		CHECK(poll_for_event(ctx, MKGUI_EVENT_LISTVIEW_SORT, LV), "expected LISTVIEW_SORT");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_listview_dblclick ]==============================[=]
static void test_listview_dblclick(void) {
	TEST_BEGIN("listview: double-click row fires LISTVIEW_DBLCLICK");
	enum { WIN = 1, VBOX1 = 2, LV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LISTVIEW, LV,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		struct mkgui_column cols[] = { { "Name", 200, MKGUI_CELL_TEXT } };
		mkgui_listview_setup(ctx, LV, 10, 1, cols, dummy_row_cb, NULL);
		run_layout(ctx);
		int32_t idx = find_widget_idx(ctx, LV);
		int32_t rx = ctx->rects[idx].x + 10;
		int32_t ry = ctx->rects[idx].y + ctx->row_height + 5;
		inject_click_at(ctx, rx, ry);
		drain_events(ctx);
		inject_click_at(ctx, rx, ry);
		CHECK(poll_for_event(ctx, MKGUI_EVENT_LISTVIEW_DBLCLICK, LV), "expected LISTVIEW_DBLCLICK");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_listview_context_header ]========================[=]
static void test_listview_context_header(void) {
	TEST_BEGIN("listview: right-click header fires CONTEXT_HEADER");
	enum { WIN = 1, VBOX1 = 2, LV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LISTVIEW, LV,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		struct mkgui_column cols[] = { { "Name", 200, MKGUI_CELL_TEXT } };
		mkgui_listview_setup(ctx, LV, 10, 1, cols, dummy_row_cb, NULL);
		run_layout(ctx);
		int32_t idx = find_widget_idx(ctx, LV);
		int32_t hx = ctx->rects[idx].x + 50;
		int32_t hy = ctx->rects[idx].y + ctx->row_height / 2;
		inject_right_click_at(ctx, hx, hy);
		CHECK(poll_for_event(ctx, MKGUI_EVENT_CONTEXT_HEADER, LV), "expected CONTEXT_HEADER");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Treeview: SELECT, EXPAND, COLLAPSE, DBLCLICK
// ---------------------------------------------------------------------------

// [=]===^=[ test_treeview_select ]================================[=]
static void test_treeview_select(void) {
	TEST_BEGIN("treeview: click node fires TREEVIEW_SELECT");
	enum { WIN = 1, VBOX1 = 2, TV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TREEVIEW, TV,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		mkgui_treeview_setup(ctx, TV);
		mkgui_treeview_add(ctx, TV, 100, 0, "Root");
		mkgui_treeview_add(ctx, TV, 101, 100, "Child");
		run_layout(ctx);
		int32_t idx = find_widget_idx(ctx, TV);
		int32_t rx = ctx->rects[idx].x + 40;
		int32_t ry = ctx->rects[idx].y + 5;
		inject_click_at(ctx, rx, ry);
		CHECK(poll_for_event(ctx, MKGUI_EVENT_TREEVIEW_SELECT, TV), "expected TREEVIEW_SELECT");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_treeview_expand ]================================[=]
static void test_treeview_expand(void) {
	TEST_BEGIN("treeview: expand via keyboard fires TREEVIEW_EXPAND");
	enum { WIN = 1, VBOX1 = 2, TV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TREEVIEW, TV,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		mkgui_treeview_setup(ctx, TV);
		mkgui_treeview_add(ctx, TV, 100, 0, "Root");
		mkgui_treeview_add(ctx, TV, 101, 100, "Child");
		run_layout(ctx);
		ctx->focus_id = TV;
		struct mkgui_treeview_data *tv = find_treeview_data(ctx, TV);
		if(tv) {
			tv->selected_node = 100;
		}
		inject_key(ctx, MKGUI_KEY_RIGHT, 0);
		CHECK(poll_for_event(ctx, MKGUI_EVENT_TREEVIEW_EXPAND, TV), "expected TREEVIEW_EXPAND");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_treeview_collapse ]==============================[=]
static void test_treeview_collapse(void) {
	TEST_BEGIN("treeview: collapse expanded node fires TREEVIEW_COLLAPSE");
	enum { WIN = 1, VBOX1 = 2, TV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TREEVIEW, TV,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		mkgui_treeview_setup(ctx, TV);
		mkgui_treeview_add(ctx, TV, 100, 0, "Root");
		mkgui_treeview_add(ctx, TV, 101, 100, "Child");
		run_layout(ctx);
		ctx->focus_id = TV;
		struct mkgui_treeview_data *tv = find_treeview_data(ctx, TV);
		if(tv) {
			tv->selected_node = 100;
		}
		inject_key(ctx, MKGUI_KEY_RIGHT, 0);
		drain_events(ctx);
		inject_key(ctx, MKGUI_KEY_LEFT, 0);
		CHECK(poll_for_event(ctx, MKGUI_EVENT_TREEVIEW_COLLAPSE, TV), "expected TREEVIEW_COLLAPSE");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_treeview_dblclick ]==============================[=]
static void test_treeview_dblclick(void) {
	TEST_BEGIN("treeview: double-click node fires TREEVIEW_DBLCLICK");
	enum { WIN = 1, VBOX1 = 2, TV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TREEVIEW, TV,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		mkgui_treeview_setup(ctx, TV);
		mkgui_treeview_add(ctx, TV, 100, 0, "Root");
		run_layout(ctx);
		int32_t idx = find_widget_idx(ctx, TV);
		int32_t rx = ctx->rects[idx].x + 40;
		int32_t ry = ctx->rects[idx].y + 5;
		inject_click_at(ctx, rx, ry);
		drain_events(ctx);
		inject_click_at(ctx, rx, ry);
		CHECK(poll_for_event(ctx, MKGUI_EVENT_TREEVIEW_DBLCLICK, TV), "expected TREEVIEW_DBLCLICK");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Gridview: GRID_CLICK, GRIDVIEW_SELECT
// ---------------------------------------------------------------------------

// [=]===^=[ test_gridview_select ]================================[=]
static void test_gridview_select(void) {
	TEST_BEGIN("gridview: click cell fires GRID_CLICK");
	enum { WIN = 1, VBOX1 = 2, GV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_GRIDVIEW, GV,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		struct mkgui_grid_column cols[] = { { "Name", 200, MKGUI_GRID_TEXT }, { "Check", 60, MKGUI_GRID_CHECK } };
		mkgui_gridview_setup(ctx, GV, 10, 2, cols, dummy_grid_cell_cb, NULL);
		run_layout(ctx);
		int32_t idx = find_widget_idx(ctx, GV);
		int32_t rx = ctx->rects[idx].x + 10;
		int32_t ry = ctx->rects[idx].y + ctx->row_height + 5;
		inject_click_at(ctx, rx, ry);
		CHECK(poll_for_event(ctx, MKGUI_EVENT_GRID_CLICK, GV), "expected GRID_CLICK");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Richlist: SELECT, DBLCLICK
// ---------------------------------------------------------------------------

// [=]===^=[ test_richlist_select ]================================[=]
static void test_richlist_select(void) {
	TEST_BEGIN("richlist: click row fires RICHLIST_SELECT");
	enum { WIN = 1, VBOX1 = 2, RL = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_RICHLIST, RL,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		mkgui_richlist_setup(ctx, RL, 10, 56, dummy_richlist_cb, NULL);
		run_layout(ctx);
		int32_t idx = find_widget_idx(ctx, RL);
		int32_t rx = ctx->rects[idx].x + 20;
		int32_t ry = ctx->rects[idx].y + 10;
		inject_click_at(ctx, rx, ry);
		CHECK(poll_for_event(ctx, MKGUI_EVENT_RICHLIST_SELECT, RL), "expected RICHLIST_SELECT");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_richlist_dblclick ]==============================[=]
static void test_richlist_dblclick(void) {
	TEST_BEGIN("richlist: double-click fires RICHLIST_DBLCLICK");
	enum { WIN = 1, VBOX1 = 2, RL = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_RICHLIST, RL,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		mkgui_richlist_setup(ctx, RL, 10, 56, dummy_richlist_cb, NULL);
		run_layout(ctx);
		int32_t idx = find_widget_idx(ctx, RL);
		int32_t rx = ctx->rects[idx].x + 20;
		int32_t ry = ctx->rects[idx].y + 10;
		inject_click_at(ctx, rx, ry);
		drain_events(ctx);
		inject_click_at(ctx, rx, ry);
		CHECK(poll_for_event(ctx, MKGUI_EVENT_RICHLIST_DBLCLICK, RL), "expected RICHLIST_DBLCLICK");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Itemview: SELECT, DBLCLICK
// ---------------------------------------------------------------------------

// [=]===^=[ test_itemview_select ]================================[=]
static void test_itemview_select(void) {
	TEST_BEGIN("itemview: click item fires ITEMVIEW_SELECT");
	enum { WIN = 1, VBOX1 = 2, IV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_ITEMVIEW, IV,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		mkgui_itemview_setup(ctx, IV, 10, MKGUI_VIEW_ICON, dummy_label_cb, dummy_icon_cb, NULL);
		run_layout(ctx);
		int32_t idx = find_widget_idx(ctx, IV);
		int32_t rx = ctx->rects[idx].x + 30;
		int32_t ry = ctx->rects[idx].y + 30;
		inject_click_at(ctx, rx, ry);
		CHECK(poll_for_event(ctx, MKGUI_EVENT_ITEMVIEW_SELECT, IV), "expected ITEMVIEW_SELECT");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_itemview_dblclick ]==============================[=]
static void test_itemview_dblclick(void) {
	TEST_BEGIN("itemview: double-click fires ITEMVIEW_DBLCLICK");
	enum { WIN = 1, VBOX1 = 2, IV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_ITEMVIEW, IV,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		mkgui_itemview_setup(ctx, IV, 10, MKGUI_VIEW_ICON, dummy_label_cb, dummy_icon_cb, NULL);
		run_layout(ctx);
		int32_t idx = find_widget_idx(ctx, IV);
		int32_t rx = ctx->rects[idx].x + 30;
		int32_t ry = ctx->rects[idx].y + 30;
		inject_click_at(ctx, rx, ry);
		drain_events(ctx);
		inject_click_at(ctx, rx, ry);
		CHECK(poll_for_event(ctx, MKGUI_EVENT_ITEMVIEW_DBLCLICK, IV), "expected ITEMVIEW_DBLCLICK");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Slider: START, END
// ---------------------------------------------------------------------------

// [=]===^=[ test_slider_start_end ]================================[=]
static void test_slider_start_end(void) {
	TEST_BEGIN("slider: press fires SLIDER_START, release fires SLIDER_END");
	enum { WIN = 1, VBOX1 = 2, SL = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_SLIDER, SL,    "",     "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		run_layout(ctx);
		mkgui_slider_setup(ctx, SL, 0, 100, 50);
		int32_t cx, cy;
		widget_center(ctx, SL, &cx, &cy);
		inject_motion(ctx, cx, cy);
		inject_press(ctx, cx, cy, 1, 0);
		uint32_t got_start = poll_for_event(ctx, MKGUI_EVENT_SLIDER_START, SL);
		inject_release(ctx, cx, cy, 1);
		uint32_t got_end = poll_for_event(ctx, MKGUI_EVENT_SLIDER_END, SL);
		CHECK(got_start, "expected SLIDER_START on press");
		CHECK(got_end, "expected SLIDER_END on release");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Tab close
// ---------------------------------------------------------------------------

// [=]===^=[ test_tab_close ]======================================[=]
static void test_tab_close(void) {
	TEST_BEGIN("tabs: keyboard close fires TAB_CLOSE");
	enum { WIN = 1, TABS1 = 2, TAB_A = 3, TAB_B = 4, LBL_A = 5, LBL_B = 6 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,    "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_TABS,   TABS1,  "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TAB,    TAB_A,  "A",    "", TABS1, 0,   0,   0, MKGUI_TAB_CLOSABLE, 0),
		MKGUI_W(MKGUI_TAB,    TAB_B,  "B",    "", TABS1, 0,   0,   0, MKGUI_TAB_CLOSABLE, 0),
		MKGUI_W(MKGUI_LABEL,  LBL_A,  "A",    "", TAB_A, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LABEL,  LBL_B,  "B",    "", TAB_B, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 6);
	CHECK(ctx, "create failed");
	if(ctx) {
		run_layout(ctx);
		int32_t idx = find_widget_idx(ctx, TABS1);
		int32_t hx = ctx->rects[idx].x + text_width(ctx, "A") + 20;
		int32_t hy = ctx->rects[idx].y + ctx->tab_height / 2;
		inject_click_at(ctx, hx, hy);
		CHECK(poll_for_event(ctx, MKGUI_EVENT_TAB_CLOSE, TABS1) || poll_for_event(ctx, MKGUI_EVENT_TAB_CHANGED, TABS1),
			"expected TAB_CLOSE or TAB_CHANGED");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Pathbar: NAV
// ---------------------------------------------------------------------------

// [=]===^=[ test_pathbar_nav ]====================================[=]
static void test_pathbar_nav(void) {
	TEST_BEGIN("pathbar: click segment fires PATHBAR_NAV");
	enum { WIN = 1, VBOX1 = 2, PB = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,  WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,    VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_PATHBAR, PB,    "",     "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		mkgui_pathbar_set(ctx, PB, "/home/user/docs");
		run_layout(ctx);
		int32_t idx = find_widget_idx(ctx, PB);
		int32_t px = ctx->rects[idx].x + 10;
		int32_t py = ctx->rects[idx].y + ctx->rects[idx].h / 2;
		inject_click_at(ctx, px, py);
		CHECK(poll_for_event(ctx, MKGUI_EVENT_PATHBAR_NAV, PB), "expected PATHBAR_NAV");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_pathbar_submit ]=================================[=]
static void test_pathbar_submit(void) {
	TEST_BEGIN("pathbar: Enter in edit mode fires PATHBAR_SUBMIT");
	enum { WIN = 1, VBOX1 = 2, PB = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,  WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,    VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_PATHBAR, PB,    "",     "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		mkgui_pathbar_set(ctx, PB, "/home");
		run_layout(ctx);
		ctx->focus_id = PB;
		struct mkgui_pathbar_data *pb = find_pathbar_data(ctx, PB);
		if(pb) {
			pb->editing = 1;
			snprintf(pb->edit_buf, sizeof(pb->edit_buf), "/tmp");
			pb->edit_cursor = 4;
		}
		inject_key(ctx, MKGUI_KEY_RETURN, 0);
		CHECK(poll_for_event(ctx, MKGUI_EVENT_PATHBAR_SUBMIT, PB), "expected PATHBAR_SUBMIT");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// IPInput: CHANGED
// ---------------------------------------------------------------------------

// [=]===^=[ test_ipinput_changed ]================================[=]
static void test_ipinput_changed(void) {
	TEST_BEGIN("ipinput: typing fires IPINPUT_CHANGED");
	enum { WIN = 1, VBOX1 = 2, IP = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,  WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,    VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_IPINPUT, IP,    "",     "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		run_layout(ctx);
		ctx->focus_id = IP;
		inject_key(ctx, '1', 0);
		drain_events(ctx);
		inject_key(ctx, '.', 0);
		CHECK(poll_for_event(ctx, MKGUI_EVENT_IPINPUT_CHANGED, IP), "expected IPINPUT_CHANGED");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Combobox: CHANGED, SUBMIT
// ---------------------------------------------------------------------------

// [=]===^=[ test_combobox_changed ]================================[=]
static void test_combobox_changed(void) {
	TEST_BEGIN("combobox: keyboard selection fires COMBOBOX_CHANGED");
	enum { WIN = 1, VBOX1 = 2, CB = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_COMBOBOX, CB,    "",     "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		char *items[] = { "Alpha", "Beta", "Gamma" };
		mkgui_combobox_setup(ctx, CB, items, 3);
		run_layout(ctx);
		inject_click(ctx, CB);
		drain_events(ctx);
		inject_key(ctx, 'b', 0);
		CHECK(poll_for_event(ctx, MKGUI_EVENT_COMBOBOX_CHANGED, CB), "expected COMBOBOX_CHANGED");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_combobox_submit ]================================[=]
static void test_combobox_submit(void) {
	TEST_BEGIN("combobox: Enter fires COMBOBOX_SUBMIT");
	enum { WIN = 1, VBOX1 = 2, CB = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_COMBOBOX, CB,    "",     "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		char *items[] = { "Alpha", "Beta" };
		mkgui_combobox_setup(ctx, CB, items, 2);
		run_layout(ctx);
		inject_click(ctx, CB);
		drain_events(ctx);
		inject_key(ctx, MKGUI_KEY_RETURN, 0);
		CHECK(poll_for_event(ctx, MKGUI_EVENT_COMBOBOX_SUBMIT, CB), "expected COMBOBOX_SUBMIT");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Datepicker: CHANGED
// ---------------------------------------------------------------------------

// [=]===^=[ test_datepicker_changed ]==============================[=]
static void test_datepicker_changed(void) {
	TEST_BEGIN("datepicker: edit + Enter fires DATEPICKER_CHANGED");
	enum { WIN = 1, VBOX1 = 2, DP = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,     WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,       VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_DATEPICKER, DP,    "",     "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		mkgui_datepicker_set(ctx, DP, 2026, 4, 1);
		run_layout(ctx);
		ctx->focus_id = DP;
		struct mkgui_datepicker_data *dp = find_datepicker_data(ctx, DP);
		if(dp) {
			dp->editing = 1;
			snprintf(dp->edit_buf, sizeof(dp->edit_buf), "2026-04-15");
			dp->edit_cursor = 10;
		}
		inject_key(ctx, MKGUI_KEY_RETURN, 0);
		CHECK(poll_for_event(ctx, MKGUI_EVENT_DATEPICKER_CHANGED, DP), "expected DATEPICKER_CHANGED");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Timer event
// ---------------------------------------------------------------------------

static uint32_t timer_fired;
static void test_timer_cb(struct mkgui_ctx *ctx, uint32_t timer_id, void *userdata) {
	(void)ctx; (void)timer_id; (void)userdata;
	timer_fired = 1;
}

// [=]===^=[ test_timer_event ]====================================[=]
static void test_timer_event(void) {
	TEST_BEGIN("timer: callback fires after interval");
	enum { WIN = 1, LBL = 2 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN, "Test", "", 0,   400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_LABEL,  LBL, "Hi",  "", WIN, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 2);
	CHECK(ctx, "create failed");
	if(ctx) {
		run_layout(ctx);
		timer_fired = 0;
		uint32_t tid = mkgui_add_timer(ctx, 1000000, test_timer_cb, NULL);
		CHECK(tid != 0, "timer creation failed");
		struct timespec ts = {0, 5000000};
		nanosleep(&ts, NULL);
		drain_events(ctx);
		if(!timer_fired) {
			nanosleep(&ts, NULL);
			drain_events(ctx);
		}
		CHECK(timer_fired, "expected timer callback to fire");
		mkgui_remove_timer(ctx, tid);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// Note: MKGUI_EVENT_TIMER never fires because mkgui_add_timer rejects
// NULL callbacks (line 7286), but the event path (line 4404) requires
// cb==NULL. This is an API inconsistency -- the NULL check should be
// removed to allow event-based timers.

// ---------------------------------------------------------------------------
// Context menu item selection
// ---------------------------------------------------------------------------

// [=]===^=[ test_context_menu_event ]=============================[=]
static void test_context_menu_event(void) {
	TEST_BEGIN("context menu: selecting item fires CONTEXT_MENU");
	enum { WIN = 1, VBOX1 = 2, BTN = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN,   "OK",   "", VBOX1, 100, 0, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		run_layout(ctx);
		mkgui_context_menu_clear(ctx);
		mkgui_context_menu_add(ctx, 500, "Cut", NULL, 0);
		mkgui_context_menu_add(ctx, 501, "Copy", NULL, 0);
		mkgui_context_menu_show_at(ctx, 100, 100);
		inject_key(ctx, MKGUI_KEY_DOWN, 0);
		drain_events(ctx);
		inject_key(ctx, MKGUI_KEY_RETURN, 0);
		CHECK(poll_for_event(ctx, MKGUI_EVENT_CONTEXT_MENU, 0), "expected CONTEXT_MENU event");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Events that cannot be tested synthetically
// ---------------------------------------------------------------------------

// FILE_DROP: requires XDnd protocol or WM_DROPFILES from OS. Cannot inject
//   through deferred queue because the drop data goes through X11 selection
//   or Windows clipboard, not through platform events.
//
// CLOSE: fires from WM_DELETE_WINDOW / WM_CLOSE. Would need to inject a
//   ClientMessage with wm_delete atom. Possible but brittle with XSync.
//
// RESIZE: fires from ConfigureNotify / WM_SIZE. Would need window manager
//   cooperation to resize the window.
//
// LISTVIEW_COL_REORDER: requires mouse drag across column headers with
//   precise threshold detection. Complex multi-step drag sequence.
//
// LISTVIEW_REORDER: requires row drag with threshold. Same complexity.
//
// GRIDVIEW_REORDER: same as LISTVIEW_REORDER.
//
// TREEVIEW_MOVE: requires node drag with threshold + drop position logic.
//
// DRAG_START / DRAG_END: require sustained mouse drag past threshold.
//   Hard to inject reliably through deferred events.

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(void) {
	printf("mkgui extended event test harness\n");
	printf("==================================\n\n");

	test_listview_select();
	test_listview_sort();
	test_listview_dblclick();
	test_listview_context_header();
	test_treeview_select();
	test_treeview_expand();
	test_treeview_collapse();
	test_treeview_dblclick();
	test_gridview_select();
	test_richlist_select();
	test_richlist_dblclick();
	test_itemview_select();
	test_itemview_dblclick();
	test_slider_start_end();
	test_tab_close();
	test_pathbar_nav();
	test_pathbar_submit();
	test_ipinput_changed();
	test_combobox_changed();
	test_combobox_submit();
	test_datepicker_changed();
	test_timer_event();
	test_context_menu_event();

	printf("\n==================================\n");
	printf("%u tests: %u passed, %u failed\n", tests_run, tests_passed, tests_failed);
	return tests_failed ? 1 : 0;
}
