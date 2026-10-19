// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT
//
// Extended event tests: events, drag/reorder, cell-edit, canvas, timers, and
// window-level RESIZE/CLOSE/FILE_DROP not covered by test_events.c.

#include "../mkgui.c"

#include <signal.h>

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

// [=]===^=[ flush_x_events ]====================================[=]
static void flush_x_events(struct mkgui_window *win) {
	mkwin_sync(win->mkwin);
	XSync(win->mkwin->dpy, True);

}

static void run_layout(struct mkgui_window *win) {
	dirty_all(win);
	layout_widgets(win);
	win->dirty = 0;
	win->dirty_full = 0;
	win->dirty_count = 0;
	flush_x_events(win);
}

static void inject_press(struct mkgui_window *win, int32_t x, int32_t y, uint32_t button, uint32_t keymod) {
	struct mkwin_event pev;
	memset(&pev, 0, sizeof(pev));
	pev.type = MKWIN_EV_BUTTON_PRESS;
	pev.x = x;
	pev.y = y;
	pev.button = button;
	pev.keymod = keymod;
	pev.popup_idx = -1;
	mkwin_deferred_push(win->mkwin, &pev);
}

static void inject_release(struct mkgui_window *win, int32_t x, int32_t y, uint32_t button) {
	struct mkwin_event pev;
	memset(&pev, 0, sizeof(pev));
	pev.type = MKWIN_EV_BUTTON_RELEASE;
	pev.x = x;
	pev.y = y;
	pev.button = button;
	pev.popup_idx = -1;
	mkwin_deferred_push(win->mkwin, &pev);
}

static void inject_key(struct mkgui_window *win, uint32_t keysym, uint32_t keymod) {
	struct mkwin_event pev;
	memset(&pev, 0, sizeof(pev));
	pev.type = MKWIN_EV_KEY;
	pev.keysym = keysym;
	pev.keymod = keymod;
	pev.popup_idx = -1;
	if(keysym >= 0x20 && keysym <= 0x7e) {
		pev.text[0] = (char)keysym;
		pev.text[1] = '\0';
		pev.text_len = 1;
	}
	mkwin_deferred_push(win->mkwin, &pev);
}

static void inject_motion(struct mkgui_window *win, int32_t x, int32_t y) {
	struct mkwin_event pev;
	memset(&pev, 0, sizeof(pev));
	pev.type = MKWIN_EV_MOTION;
	pev.x = x;
	pev.y = y;
	pev.popup_idx = -1;
	mkwin_deferred_push(win->mkwin, &pev);
}

static void widget_center(struct mkgui_window *win, uint32_t id, int32_t *cx, int32_t *cy) {
	int32_t idx = find_widget_idx(win, id);
	if(idx < 0) {
		*cx = 0;
		*cy = 0;
		return;
	}
	*cx = win->rects[idx].x + win->rects[idx].w / 2;
	*cy = win->rects[idx].y + win->rects[idx].h / 2;
}

static void inject_click(struct mkgui_window *win, uint32_t id) {
	int32_t cx, cy;
	widget_center(win, id, &cx, &cy);
	inject_motion(win, cx, cy);
	inject_press(win, cx, cy, 1, 0);
	inject_release(win, cx, cy, 1);
}

static void inject_click_at(struct mkgui_window *win, int32_t x, int32_t y) {
	inject_motion(win, x, y);
	inject_press(win, x, y, 1, 0);
	inject_release(win, x, y, 1);
}

static void inject_right_click_at(struct mkgui_window *win, int32_t x, int32_t y) {
	inject_motion(win, x, y);
	inject_press(win, x, y, 3, 0);
	inject_release(win, x, y, 3);
}

static uint32_t poll_for_event(struct mkgui_window *win, uint32_t event_type, uint32_t target_id) {
	flush_x_events(win);
	uint32_t found = 0;
	struct mkgui_event ev;
	for(uint32_t i = 0; i < 50; ++i) {
		uint32_t got = mkgui_window_poll(win, &ev);
		if(got && ev.type == event_type && (target_id == 0 || ev.id == target_id)) {
			found = 1;
		}

		if(!got && win->mkwin->deferred_head == win->mkwin->deferred_tail) {
			mkgui_window_poll(win, &ev);
			if(ev.type == event_type && (target_id == 0 || ev.id == target_id)) {
				found = 1;
			}
			break;
		}
	}
	return found;
}

// Like poll_for_event but returns the matching event so callers can assert on its payload; type NONE if no match.
static struct mkgui_event poll_capture(struct mkgui_window *win, uint32_t event_type, uint32_t target_id) {
	flush_x_events(win);
	struct mkgui_event ev;
	struct mkgui_event hit;
	memset(&hit, 0, sizeof(hit));
	for(uint32_t i = 0; i < 50; ++i) {
		uint32_t got = mkgui_window_poll(win, &ev);
		if(got && ev.type == event_type && (target_id == 0 || ev.id == target_id)) {
			return ev;
		}

		if(!got && win->mkwin->deferred_head == win->mkwin->deferred_tail) {
			break;
		}
	}
	return hit;
}

static void drain_events(struct mkgui_window *win) {
	flush_x_events(win);
	struct mkgui_event ev;
	for(uint32_t i = 0; i < 50; ++i) {
		if(!mkgui_window_poll(win, &ev)) {
			break;
		}
	}
}

static void inject_resize(struct mkgui_window *win, int32_t w, int32_t h) {
	struct mkwin_event pev;
	memset(&pev, 0, sizeof(pev));
	pev.type = MKWIN_EV_RESIZE;
	pev.width = w;
	pev.height = h;
	pev.popup_idx = -1;
	mkwin_deferred_push(win->mkwin, &pev);
}

static void inject_close(struct mkgui_window *win) {
	struct mkwin_event pev;
	memset(&pev, 0, sizeof(pev));
	pev.type = MKWIN_EV_CLOSE;
	pev.popup_idx = -1;
	mkwin_deferred_push(win->mkwin, &pev);
}

static void inject_drop(struct mkgui_window *win) {
	struct mkwin_event pev;
	memset(&pev, 0, sizeof(pev));
	pev.type = MKWIN_EV_DROP;
	pev.popup_idx = -1;
	mkwin_deferred_push(win->mkwin, &pev);
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
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		struct mkgui_column cols[] = { { "Name", 200, MKGUI_CELL_TEXT }, { "Size", 100, MKGUI_CELL_TEXT } };
		mkgui_listview_setup(win, LV, 10, 2, cols, dummy_row_cb, NULL);
		run_layout(win);
		int32_t idx = find_widget_idx(win, LV);
		int32_t rx = win->rects[idx].x + 10;
		int32_t ry = win->rects[idx].y + win->row_height + 5;
		inject_click_at(win, rx, ry);
		CHECK(poll_for_event(win, MKGUI_EVENT_LISTVIEW_SELECT, LV), "expected LISTVIEW_SELECT");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
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
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		struct mkgui_column cols[] = { { "Name", 200, MKGUI_CELL_TEXT }, { "Size", 100, MKGUI_CELL_TEXT } };
		mkgui_listview_setup(win, LV, 10, 2, cols, dummy_row_cb, NULL);
		run_layout(win);
		int32_t idx = find_widget_idx(win, LV);
		int32_t hx = win->rects[idx].x + 50;
		int32_t hy = win->rects[idx].y + win->row_height / 2;
		inject_click_at(win, hx, hy);
		CHECK(poll_for_event(win, MKGUI_EVENT_LISTVIEW_SORT, LV), "expected LISTVIEW_SORT");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
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
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		struct mkgui_column cols[] = { { "Name", 200, MKGUI_CELL_TEXT } };
		mkgui_listview_setup(win, LV, 10, 1, cols, dummy_row_cb, NULL);
		run_layout(win);
		int32_t idx = find_widget_idx(win, LV);
		int32_t rx = win->rects[idx].x + 10;
		int32_t ry = win->rects[idx].y + win->row_height + 5;
		inject_click_at(win, rx, ry);
		drain_events(win);
		inject_click_at(win, rx, ry);
		CHECK(poll_for_event(win, MKGUI_EVENT_LISTVIEW_DBLCLICK, LV), "expected LISTVIEW_DBLCLICK");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
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
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		struct mkgui_column cols[] = { { "Name", 200, MKGUI_CELL_TEXT } };
		mkgui_listview_setup(win, LV, 10, 1, cols, dummy_row_cb, NULL);
		run_layout(win);
		int32_t idx = find_widget_idx(win, LV);
		int32_t hx = win->rects[idx].x + 50;
		int32_t hy = win->rects[idx].y + win->row_height / 2;
		inject_right_click_at(win, hx, hy);
		CHECK(poll_for_event(win, MKGUI_EVENT_CONTEXT_HEADER, LV), "expected CONTEXT_HEADER");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
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
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		mkgui_treeview_setup(win, TV);
		mkgui_treeview_add(win, TV, 100, 0, "Root");
		mkgui_treeview_add(win, TV, 101, 100, "Child");
		run_layout(win);
		int32_t idx = find_widget_idx(win, TV);
		int32_t rx = win->rects[idx].x + 40;
		int32_t ry = win->rects[idx].y + 5;
		inject_click_at(win, rx, ry);
		CHECK(poll_for_event(win, MKGUI_EVENT_TREEVIEW_SELECT, TV), "expected TREEVIEW_SELECT");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
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
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		mkgui_treeview_setup(win, TV);
		mkgui_treeview_add(win, TV, 100, 0, "Root");
		mkgui_treeview_add(win, TV, 101, 100, "Child");
		run_layout(win);
		win->focus_id = TV;
		struct mkgui_treeview_data *tv = find_treeview_data(win, TV);
		if(tv) {
			tv->selected_node = 100;
		}
		inject_key(win, MKGUI_KEY_RIGHT, 0);
		CHECK(poll_for_event(win, MKGUI_EVENT_TREEVIEW_EXPAND, TV), "expected TREEVIEW_EXPAND");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
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
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		mkgui_treeview_setup(win, TV);
		mkgui_treeview_add(win, TV, 100, 0, "Root");
		mkgui_treeview_add(win, TV, 101, 100, "Child");
		run_layout(win);
		win->focus_id = TV;
		struct mkgui_treeview_data *tv = find_treeview_data(win, TV);
		if(tv) {
			tv->selected_node = 100;
		}
		inject_key(win, MKGUI_KEY_RIGHT, 0);
		drain_events(win);
		inject_key(win, MKGUI_KEY_LEFT, 0);
		CHECK(poll_for_event(win, MKGUI_EVENT_TREEVIEW_COLLAPSE, TV), "expected TREEVIEW_COLLAPSE");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
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
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		mkgui_treeview_setup(win, TV);
		mkgui_treeview_add(win, TV, 100, 0, "Root");
		run_layout(win);
		int32_t idx = find_widget_idx(win, TV);
		int32_t rx = win->rects[idx].x + 40;
		int32_t ry = win->rects[idx].y + 5;
		inject_click_at(win, rx, ry);
		drain_events(win);
		inject_click_at(win, rx, ry);
		CHECK(poll_for_event(win, MKGUI_EVENT_TREEVIEW_DBLCLICK, TV), "expected TREEVIEW_DBLCLICK");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// TreeList: SELECT, DBLCLICK, EXPAND, COLLAPSE, SORT, COL_REORDER, CONTEXT
// plus behaviour invariants (hierarchy-preserving sort, handle-stable
// selection across expand/collapse).
// ---------------------------------------------------------------------------

// [=]===^=[ test_treelist_select ]===============================[=]
static void test_treelist_select(void) {
	TEST_BEGIN("treelist: click row fires TREELIST_SELECT");
	enum { WIN = 1, VBOX1 = 2, TL = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TREELIST, TL,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		struct mkgui_column cols[] = { { "Name", 200, MKGUI_CELL_TEXT }, { "Size", 100, MKGUI_CELL_TEXT } };
		mkgui_treelist_setup(win, TL, 2, cols);
		const char *r0[] = { "Alpha", "1" };
		mkgui_treelist_add(win, TL, MKGUI_TREELIST_ROOT, r0, NULL);
		run_layout(win);
		int32_t idx = find_widget_idx(win, TL);
		int32_t rx = win->rects[idx].x + 80;
		int32_t ry = win->rects[idx].y + win->row_height + 5;
		inject_click_at(win, rx, ry);
		CHECK(poll_for_event(win, MKGUI_EVENT_TREELIST_SELECT, TL), "expected TREELIST_SELECT");
		CHECK(mkgui_treelist_get_selected(win, TL) == 1, "selection should be handle 1");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_treelist_dblclick ]=============================[=]
static void test_treelist_dblclick(void) {
	TEST_BEGIN("treelist: double-click row fires TREELIST_DBLCLICK");
	enum { WIN = 1, VBOX1 = 2, TL = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TREELIST, TL,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		struct mkgui_column cols[] = { { "Name", 200, MKGUI_CELL_TEXT } };
		mkgui_treelist_setup(win, TL, 1, cols);
		const char *r0[] = { "Alpha" };
		mkgui_treelist_add(win, TL, MKGUI_TREELIST_ROOT, r0, NULL);
		run_layout(win);
		int32_t idx = find_widget_idx(win, TL);
		int32_t rx = win->rects[idx].x + 80;
		int32_t ry = win->rects[idx].y + win->row_height + 5;
		inject_click_at(win, rx, ry);
		drain_events(win);
		inject_click_at(win, rx, ry);
		CHECK(poll_for_event(win, MKGUI_EVENT_TREELIST_DBLCLICK, TL), "expected TREELIST_DBLCLICK");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_treelist_twisty_expand ]========================[=]
static void test_treelist_twisty_expand(void) {
	TEST_BEGIN("treelist: click twisty fires TREELIST_EXPAND");
	enum { WIN = 1, VBOX1 = 2, TL = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TREELIST, TL,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		struct mkgui_column cols[] = { { "Name", 200, MKGUI_CELL_TEXT } };
		mkgui_treelist_setup(win, TL, 1, cols);
		const char *rp[] = { "Parent" };
		const char *rc[] = { "Child" };
		uint32_t p = mkgui_treelist_add(win, TL, MKGUI_TREELIST_ROOT, rp, NULL);
		mkgui_treelist_add(win, TL, p, rc, NULL);
		run_layout(win);
		int32_t idx = find_widget_idx(win, TL);
		int32_t tx = win->rects[idx].x + 1 + sc(win, 4) + sc(win, MKGUI_TL_INDENT_PX) / 2;
		int32_t ty = win->rects[idx].y + win->row_height + win->row_height / 2;
		inject_click_at(win, tx, ty);
		CHECK(poll_for_event(win, MKGUI_EVENT_TREELIST_EXPAND, TL), "expected TREELIST_EXPAND");
		CHECK(mkgui_treelist_is_expanded(win, TL, p), "parent should be expanded");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_treelist_expand_key ]===========================[=]
static void test_treelist_expand_key(void) {
	TEST_BEGIN("treelist: Right on parent fires TREELIST_EXPAND");
	enum { WIN = 1, VBOX1 = 2, TL = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TREELIST, TL,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		struct mkgui_column cols[] = { { "Name", 200, MKGUI_CELL_TEXT } };
		mkgui_treelist_setup(win, TL, 1, cols);
		const char *rp[] = { "Parent" };
		const char *rc[] = { "Child" };
		uint32_t p = mkgui_treelist_add(win, TL, MKGUI_TREELIST_ROOT, rp, NULL);
		mkgui_treelist_add(win, TL, p, rc, NULL);
		run_layout(win);
		win->focus_id = TL;
		mkgui_treelist_set_selected(win, TL, (int32_t)p);
		inject_key(win, MKGUI_KEY_RIGHT, 0);
		CHECK(poll_for_event(win, MKGUI_EVENT_TREELIST_EXPAND, TL), "expected TREELIST_EXPAND");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_treelist_collapse_key ]=========================[=]
static void test_treelist_collapse_key(void) {
	TEST_BEGIN("treelist: Left on expanded parent fires TREELIST_COLLAPSE");
	enum { WIN = 1, VBOX1 = 2, TL = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TREELIST, TL,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		struct mkgui_column cols[] = { { "Name", 200, MKGUI_CELL_TEXT } };
		mkgui_treelist_setup(win, TL, 1, cols);
		const char *rp[] = { "Parent" };
		const char *rc[] = { "Child" };
		uint32_t p = mkgui_treelist_add(win, TL, MKGUI_TREELIST_ROOT, rp, NULL);
		mkgui_treelist_add(win, TL, p, rc, NULL);
		mkgui_treelist_expand(win, TL, p);
		run_layout(win);
		win->focus_id = TL;
		mkgui_treelist_set_selected(win, TL, (int32_t)p);
		inject_key(win, MKGUI_KEY_LEFT, 0);
		CHECK(poll_for_event(win, MKGUI_EVENT_TREELIST_COLLAPSE, TL), "expected TREELIST_COLLAPSE");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_treelist_sort ]=================================[=]
static void test_treelist_sort(void) {
	TEST_BEGIN("treelist: click header fires TREELIST_SORT");
	enum { WIN = 1, VBOX1 = 2, TL = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TREELIST, TL,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		struct mkgui_column cols[] = { { "Name", 200, MKGUI_CELL_TEXT }, { "Size", 100, MKGUI_CELL_TEXT } };
		mkgui_treelist_setup(win, TL, 2, cols);
		const char *r0[] = { "Alpha", "1" };
		mkgui_treelist_add(win, TL, MKGUI_TREELIST_ROOT, r0, NULL);
		run_layout(win);
		int32_t idx = find_widget_idx(win, TL);
		int32_t hx = win->rects[idx].x + 50;
		int32_t hy = win->rects[idx].y + win->row_height / 2;
		inject_click_at(win, hx, hy);
		CHECK(poll_for_event(win, MKGUI_EVENT_TREELIST_SORT, TL), "expected TREELIST_SORT");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_treelist_col_reorder ]==========================[=]
static void test_treelist_col_reorder(void) {
	TEST_BEGIN("treelist: drag column header fires TREELIST_COL_REORDER");
	enum { WIN = 1, VBOX1 = 2, TL = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TREELIST, TL,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		struct mkgui_column cols[] = { { "Name", 200, MKGUI_CELL_TEXT }, { "Size", 100, MKGUI_CELL_TEXT } };
		mkgui_treelist_setup(win, TL, 2, cols);
		run_layout(win);
		int32_t idx = find_widget_idx(win, TL);
		int32_t hy = win->rects[idx].y + win->row_height / 2;
		int32_t col0 = win->rects[idx].x + 100;
		int32_t drop_x = win->rects[idx].x + 290;
		inject_press(win, col0, hy, 1, 0);
		inject_motion(win, drop_x, hy);
		inject_release(win, drop_x, hy, 1);
		CHECK(poll_for_event(win, MKGUI_EVENT_TREELIST_COL_REORDER, TL), "expected TREELIST_COL_REORDER");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_treelist_context_header ]=======================[=]
static void test_treelist_context_header(void) {
	TEST_BEGIN("treelist: right-click header fires CONTEXT_HEADER");
	enum { WIN = 1, VBOX1 = 2, TL = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TREELIST, TL,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		struct mkgui_column cols[] = { { "Name", 200, MKGUI_CELL_TEXT } };
		mkgui_treelist_setup(win, TL, 1, cols);
		run_layout(win);
		int32_t idx = find_widget_idx(win, TL);
		int32_t hx = win->rects[idx].x + 50;
		int32_t hy = win->rects[idx].y + win->row_height / 2;
		inject_right_click_at(win, hx, hy);
		CHECK(poll_for_event(win, MKGUI_EVENT_CONTEXT_HEADER, TL), "expected CONTEXT_HEADER");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_treelist_sort_keeps_hierarchy ]=================[=]
// Sorting reorders parents at the top level and children within each parent,
// but never reparents or promotes a child. Handles: p1=1, p2=2, c1=3, c2=4.
static void test_treelist_sort_keeps_hierarchy(void) {
	TEST_BEGIN("treelist: sort reorders per-parent without promoting children");
	enum { WIN = 1, VBOX1 = 2, TL = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TREELIST, TL,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		struct mkgui_column cols[] = { { "Name", 200, MKGUI_CELL_TEXT } };
		mkgui_treelist_setup(win, TL, 1, cols);
		const char *rb[] = { "Beta" };
		const char *ra[] = { "Alpha" };
		const char *rz[] = { "Zeta" };
		const char *rd[] = { "Aardvark" };
		uint32_t p1 = mkgui_treelist_add(win, TL, MKGUI_TREELIST_ROOT, rb, NULL);
		uint32_t p2 = mkgui_treelist_add(win, TL, MKGUI_TREELIST_ROOT, ra, NULL);
		uint32_t c1 = mkgui_treelist_add(win, TL, p1, rz, NULL);
		uint32_t c2 = mkgui_treelist_add(win, TL, p1, rd, NULL);
		CHECK(p1 == 1 && p2 == 2 && c1 == 3 && c2 == 4, "handles are 1-based insertion order");
		mkgui_treelist_expand_all(win, TL);
		mkgui_treelist_set_sort(win, TL, 0, 1);

		CHECK(mkgui_treelist_node_count(win, TL) == 4, "node count unchanged by sort");
		CHECK(mkgui_treelist_get_parent(win, TL, c1) == p1, "c1 stays under p1");
		CHECK(mkgui_treelist_get_parent(win, TL, c2) == p1, "c2 stays under p1");
		CHECK(mkgui_treelist_get_parent(win, TL, p1) == 0, "p1 stays a root");
		CHECK(mkgui_treelist_get_parent(win, TL, p2) == 0, "p2 stays a root");

		struct mkgui_treelist_data *tl = find_treelist_data(win, TL);
		if(tl) {
			treelist_rebuild_visible(tl);
			// Expected display order: Alpha(p2), then p1's children Aardvark(c2),
			// Zeta(c1) under Beta(p1). Node indices are handle - 1.
			CHECK(tl->visible_count == 4, "all four nodes visible");
			CHECK(tl->visible[0] == p2 - 1, "Alpha sorts first at root");
			CHECK(tl->visible[1] == p1 - 1, "Beta second at root");
			CHECK(tl->visible[2] == c2 - 1, "Aardvark first among Beta's children");
			CHECK(tl->visible[3] == c1 - 1, "Zeta second among Beta's children");
		}
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_treelist_selection_survives_expand ]============[=]
static void test_treelist_selection_survives_expand(void) {
	TEST_BEGIN("treelist: selection survives expand/collapse of another subtree");
	enum { WIN = 1, VBOX1 = 2, TL = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TREELIST, TL,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		struct mkgui_column cols[] = { { "Name", 200, MKGUI_CELL_TEXT } };
		mkgui_treelist_setup(win, TL, 1, cols);
		const char *r[] = { "n" };
		uint32_t p1 = mkgui_treelist_add(win, TL, MKGUI_TREELIST_ROOT, r, NULL);
		uint32_t p2 = mkgui_treelist_add(win, TL, MKGUI_TREELIST_ROOT, r, NULL);
		uint32_t c1 = mkgui_treelist_add(win, TL, p1, r, NULL);
		mkgui_treelist_add(win, TL, p2, r, NULL);
		mkgui_treelist_expand_all(win, TL);
		run_layout(win);
		mkgui_treelist_set_selected(win, TL, (int32_t)c1);
		mkgui_treelist_collapse(win, TL, p2);
		mkgui_treelist_expand(win, TL, p2);
		CHECK(mkgui_treelist_get_selected(win, TL) == (int32_t)c1, "selection handle unchanged");
		CHECK(mkgui_treelist_is_selected(win, TL, c1), "c1 still selected");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
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
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		struct mkgui_grid_column cols[] = { { "Name", 200, MKGUI_GRID_TEXT }, { "Check", 60, MKGUI_GRID_CHECK } };
		mkgui_gridview_setup(win, GV, 10, 2, cols, dummy_grid_cell_cb, NULL);
		run_layout(win);
		int32_t idx = find_widget_idx(win, GV);
		int32_t rx = win->rects[idx].x + 10;
		int32_t ry = win->rects[idx].y + win->row_height + 5;
		inject_click_at(win, rx, ry);
		CHECK(poll_for_event(win, MKGUI_EVENT_GRID_CLICK, GV), "expected GRID_CLICK");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
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
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		mkgui_richlist_setup(win, RL, 10, 56, dummy_richlist_cb, NULL);
		run_layout(win);
		int32_t idx = find_widget_idx(win, RL);
		int32_t rx = win->rects[idx].x + 20;
		int32_t ry = win->rects[idx].y + 10;
		inject_click_at(win, rx, ry);
		CHECK(poll_for_event(win, MKGUI_EVENT_RICHLIST_SELECT, RL), "expected RICHLIST_SELECT");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
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
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		mkgui_richlist_setup(win, RL, 10, 56, dummy_richlist_cb, NULL);
		run_layout(win);
		int32_t idx = find_widget_idx(win, RL);
		int32_t rx = win->rects[idx].x + 20;
		int32_t ry = win->rects[idx].y + 10;
		inject_click_at(win, rx, ry);
		drain_events(win);
		inject_click_at(win, rx, ry);
		CHECK(poll_for_event(win, MKGUI_EVENT_RICHLIST_DBLCLICK, RL), "expected RICHLIST_DBLCLICK");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
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
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		mkgui_itemview_setup(win, IV, 10, MKGUI_VIEW_ICON, dummy_label_cb, dummy_icon_cb, NULL);
		run_layout(win);
		int32_t idx = find_widget_idx(win, IV);
		int32_t rx = win->rects[idx].x + 30;
		int32_t ry = win->rects[idx].y + 30;
		inject_click_at(win, rx, ry);
		CHECK(poll_for_event(win, MKGUI_EVENT_ITEMVIEW_SELECT, IV), "expected ITEMVIEW_SELECT");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
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
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		mkgui_itemview_setup(win, IV, 10, MKGUI_VIEW_ICON, dummy_label_cb, dummy_icon_cb, NULL);
		run_layout(win);
		int32_t idx = find_widget_idx(win, IV);
		int32_t rx = win->rects[idx].x + 30;
		int32_t ry = win->rects[idx].y + 30;
		inject_click_at(win, rx, ry);
		drain_events(win);
		inject_click_at(win, rx, ry);
		CHECK(poll_for_event(win, MKGUI_EVENT_ITEMVIEW_DBLCLICK, IV), "expected ITEMVIEW_DBLCLICK");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
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
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		mkgui_slider_setup(win, SL, 0, 100, 50);
		int32_t cx, cy;
		widget_center(win, SL, &cx, &cy);
		inject_motion(win, cx, cy);
		inject_press(win, cx, cy, 1, 0);
		uint32_t got_start = poll_for_event(win, MKGUI_EVENT_SLIDER_START, SL);
		inject_release(win, cx, cy, 1);
		uint32_t got_end = poll_for_event(win, MKGUI_EVENT_SLIDER_END, SL);
		CHECK(got_start, "expected SLIDER_START on press");
		CHECK(got_end, "expected SLIDER_END on release");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
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
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 6, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		int32_t idx = find_widget_idx(win, TABS1);
		int32_t hx = win->rects[idx].x + text_width(win, "A") + 20;
		int32_t hy = win->rects[idx].y + win->tab_height / 2;
		inject_click_at(win, hx, hy);
		CHECK(poll_for_event(win, MKGUI_EVENT_TAB_CLOSE, TABS1) || poll_for_event(win, MKGUI_EVENT_TAB_CHANGED, TABS1), "expected TAB_CLOSE or TAB_CHANGED");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
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
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		mkgui_pathbar_set(win, PB, "/home/user/docs");
		run_layout(win);
		int32_t idx = find_widget_idx(win, PB);
		int32_t px = win->rects[idx].x + 10;
		int32_t py = win->rects[idx].y + win->rects[idx].h / 2;
		inject_click_at(win, px, py);
		CHECK(poll_for_event(win, MKGUI_EVENT_PATHBAR_NAV, PB), "expected PATHBAR_NAV");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
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
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		mkgui_pathbar_set(win, PB, "/home");
		run_layout(win);
		win->focus_id = PB;
		struct mkgui_pathbar_data *pb = find_pathbar_data(win, PB);
		if(pb) {
			pb->editing = 1;
			snprintf(pb->edit_buf, sizeof(pb->edit_buf), "/tmp");
			pb->edit_cursor = 4;
		}
		inject_key(win, MKGUI_KEY_RETURN, 0);
		CHECK(poll_for_event(win, MKGUI_EVENT_PATHBAR_SUBMIT, PB), "expected PATHBAR_SUBMIT");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
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
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		win->focus_id = IP;
		inject_key(win, '1', 0);
		drain_events(win);
		inject_key(win, '.', 0);
		CHECK(poll_for_event(win, MKGUI_EVENT_IPINPUT_CHANGED, IP), "expected IPINPUT_CHANGED");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
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
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		const char *items[] = { "Alpha", "Beta", "Gamma" };
		mkgui_combobox_setup(win, CB, items, 3);
		run_layout(win);
		inject_click(win, CB);
		drain_events(win);
		inject_key(win, 'b', 0);
		CHECK(poll_for_event(win, MKGUI_EVENT_COMBOBOX_CHANGED, CB), "expected COMBOBOX_CHANGED");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
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
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		const char *items[] = { "Alpha", "Beta" };
		mkgui_combobox_setup(win, CB, items, 2);
		run_layout(win);
		inject_click(win, CB);
		drain_events(win);
		inject_key(win, MKGUI_KEY_RETURN, 0);
		CHECK(poll_for_event(win, MKGUI_EVENT_COMBOBOX_SUBMIT, CB), "expected COMBOBOX_SUBMIT");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
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
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		mkgui_datepicker_set(win, DP, 2026, 4, 1);
		run_layout(win);
		win->focus_id = DP;
		struct mkgui_datepicker_data *dp = find_datepicker_data(win, DP);
		if(dp) {
			dp->editing = 1;
			snprintf(dp->edit_buf, sizeof(dp->edit_buf), "2026-04-15");
			dp->edit_cursor = 10;
		}
		inject_key(win, MKGUI_KEY_RETURN, 0);
		CHECK(poll_for_event(win, MKGUI_EVENT_DATEPICKER_CHANGED, DP), "expected DATEPICKER_CHANGED");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Timer event
// ---------------------------------------------------------------------------

static uint32_t timer_fired;
static void test_timer_cb(struct mkgui_window *win, uint32_t timer_id, void *userdata) {
	(void)win; (void)timer_id; (void)userdata;
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
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 2, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		timer_fired = 0;
		uint32_t tid = mkgui_window_add_timer(win, 1000000, test_timer_cb, NULL);
		CHECK(tid != 0, "timer creation failed");
		struct timespec ts = {0, 5000000};
		nanosleep(&ts, NULL);
		drain_events(win);
		if(!timer_fired) {
			nanosleep(&ts, NULL);
			drain_events(win);
		}
		CHECK(timer_fired, "expected timer callback to fire");
		mkgui_window_remove_timer(win, tid);
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// Note: MKGUI_EVENT_TIMER never fires because mkgui_window_add_timer rejects
// NULL callbacks (line 7286), but the event path (line 4404) requires
// cb==NULL. This is an API inconsistency -- the NULL check should be
// removed to allow event-based timers.

static volatile sig_atomic_t idle_wd_fired;
static void idle_wd_handler(int sig) {
	(void)sig;
	idle_wd_fired = 1;
}

static uint32_t transient_timer_fired;
static void transient_timer_cb(struct mkgui_window *win, uint32_t timer_id, void *userdata) {
	(void)win; (void)timer_id; (void)userdata;
	transient_timer_fired = 1;
}

// [=]===^=[ test_timer_wakes_idle_on_transient_window ]===========[=]
// Regression: a timer on a non-primary window must wake an idle wait. Only the
// primary's timer fds used to be folded into ppoll. SIGALRM watchdog turns a hang into a failure.
static void test_timer_wakes_idle_on_transient_window(void) {
	TEST_BEGIN("timer: transient-window timer wakes an idle wait");
	enum { PWIN = 1, PLBL = 2, TWIN = 3, TLBL = 4 };
	struct mkgui_widget pw[] = {
		MKGUI_W(MKGUI_WINDOW, PWIN, "Primary",   "", 0,    300, 200, 0, 0, 0),
		MKGUI_W(MKGUI_LABEL,  PLBL, "P",         "", PWIN, 0, 0, 0, 0, 0),
	};
	struct mkgui_widget tw[] = {
		MKGUI_W(MKGUI_WINDOW, TWIN, "Transient", "", 0,    200, 120, 0, 0, 0),
		MKGUI_W(MKGUI_LABEL,  TLBL, "T",         "", TWIN, 0, 0, 0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *primary = mkgui_window_create(ctx, NULL, pw, 2, NULL, 0, 0);
	struct mkgui_window *transient = mkgui_window_create(ctx, primary, tw, 2, NULL, 0, 0);
	CHECK(primary && transient, "window creation failed");
	if(primary && transient) {
		// Drain all startup mapping/expose traffic so the next wait genuinely
		// blocks on ppoll rather than returning on already-queued events.
		for(uint32_t i = 0; i < 20; ++i) {
			drain_events(primary);
			drain_events(transient);
			struct timespec s = {0, 5000000};
			nanosleep(&s, NULL);
		}

		transient_timer_fired = 0;
		uint32_t tid = mkgui_window_add_timer(transient, 30000000, transient_timer_cb, NULL);
		CHECK(tid != 0, "timer creation failed");

		idle_wd_fired = 0;
		struct sigaction sa;
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = idle_wd_handler;
		sigaction(SIGALRM, &sa, NULL);
		alarm(2);

		while(!transient_timer_fired && !idle_wd_fired) {
			mkgui_ctx_wait(ctx);
			drain_events(primary);
			drain_events(transient);
		}
		alarm(0);

		CHECK(transient_timer_fired, "transient-window timer callback never fired");
		CHECK(!idle_wd_fired, "wait blocked; transient-window timer did not wake the idle loop");

		mkgui_window_remove_timer(transient, tid);
	}
	mkgui_ctx_destroy(ctx);
	TEST_END();
}

static uint32_t self_close_fired;
static void self_close_timer_cb(struct mkgui_window *win, uint32_t timer_id, void *userdata) {
	(void)timer_id; (void)userdata;
	self_close_fired = 1;
	// Self-close from inside the timer callback: deferred-destroy only flags it; an inline free would leave the timer loop on freed memory.
	mkgui_window_close(win);
}

// [=]===^=[ test_window_self_close_from_timer ]===================[=]
// Regression: a window must destroy itself from its own callback via deferred
// close, reaped outside any poll. Under ASan an inline destroy would be a use-after-free.
static void test_window_self_close_from_timer(void) {
	TEST_BEGIN("window: self-close from own timer callback is safe (deferred)");
	enum { PWIN = 1, PLBL = 2, TWIN = 3, TLBL = 4 };
	struct mkgui_widget pw[] = {
		MKGUI_W(MKGUI_WINDOW, PWIN, "Primary",   "", 0,    300, 200, 0, 0, 0),
		MKGUI_W(MKGUI_LABEL,  PLBL, "P",         "", PWIN, 0, 0, 0, 0, 0),
	};
	struct mkgui_widget tw[] = {
		MKGUI_W(MKGUI_WINDOW, TWIN, "Transient", "", 0,    200, 120, 0, 0, 0),
		MKGUI_W(MKGUI_LABEL,  TLBL, "T",         "", TWIN, 0, 0, 0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *primary = mkgui_window_create(ctx, NULL, pw, 2, NULL, 0, 0);
	struct mkgui_window *transient = mkgui_window_create(ctx, primary, tw, 2, NULL, 0, 0);
	CHECK(primary && transient, "window creation failed");
	if(primary && transient) {
		CHECK(ctx->window_count == 2, "expected two windows on the ctx");
		self_close_fired = 0;
		uint32_t tid = mkgui_window_add_timer(transient, 5000000, self_close_timer_cb, NULL);
		CHECK(tid != 0, "timer creation failed");

		// Host-owned loop: poll every live window (which fires the transient's
		// timer -> it closes itself), then reap outside the poll.
		struct mkgui_event ev;
		uint32_t iter = 0;
		while(ctx->window_count > 1 && iter < 400) {
			for(uint32_t i = 0; i < ctx->window_count; ++i) {
				while(mkgui_window_poll(ctx->windows[i], &ev)) {
				}
			}
			mkgui_ctx_reap_closed(ctx);
			struct timespec s = {0, 2000000};
			nanosleep(&s, NULL);
			++iter;
		}

		CHECK(self_close_fired, "timer callback never fired");
		CHECK(ctx->window_count == 1, "self-closed window was not reaped");
		CHECK(ctx->windows[0] == primary, "primary window did not survive the reap");
	}
	mkgui_ctx_destroy(ctx);
	TEST_END();
}

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
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		mkgui_context_menu_clear(win);
		mkgui_context_menu_add(win, 500, "Cut", NULL, 0);
		mkgui_context_menu_add(win, 501, "Copy", NULL, 0);
		mkgui_context_menu_show_at(win, 100, 100);
		inject_key(win, MKGUI_KEY_DOWN, 0);
		drain_events(win);
		inject_key(win, MKGUI_KEY_RETURN, 0);
		CHECK(poll_for_event(win, MKGUI_EVENT_CONTEXT_MENU, 0), "expected CONTEXT_MENU event");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
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

// ---------------------------------------------------------------------------
// Gridview: keyboard SELECT and checkbox CHECK
// ---------------------------------------------------------------------------

// [=]===^=[ test_gridview_keyboard_select ]=======================[=]
static void test_gridview_keyboard_select(void) {
	TEST_BEGIN("gridview: keyboard row nav fires GRIDVIEW_SELECT");
	enum { WIN = 1, VBOX1 = 2, GV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_GRIDVIEW, GV,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		struct mkgui_grid_column cols[] = { { "A", 80, MKGUI_GRID_TEXT }, { "B", 80, MKGUI_GRID_TEXT } };
		mkgui_gridview_setup(win, GV, 10, 2, cols, dummy_grid_cell_cb, NULL);
		run_layout(win);
		win->focus_id = GV;
		struct mkgui_gridview_data *gv = find_gridv_data(win, GV);
		if(gv) {
			gv->selected_row = 0;
			gv->selected_col = 0;
		}
		inject_key(win, MKGUI_KEY_DOWN, 0);
		CHECK(poll_for_event(win, MKGUI_EVENT_GRIDVIEW_SELECT, GV), "expected GRIDVIEW_SELECT");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_gridview_check ]=================================[=]
static void test_gridview_check(void) {
	TEST_BEGIN("gridview: click checkbox cell fires GRID_CHECK");
	enum { WIN = 1, VBOX1 = 2, GV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_GRIDVIEW, GV,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		struct mkgui_grid_column cols[] = { { "On", 40, MKGUI_GRID_CHECK }, { "Name", 120, MKGUI_GRID_TEXT } };
		mkgui_gridview_setup(win, GV, 10, 2, cols, dummy_grid_cell_cb, NULL);
		run_layout(win);
		int32_t idx = find_widget_idx(win, GV);
		int32_t rx = win->rects[idx].x + 20;
		int32_t ry = win->rects[idx].y + win->row_height + win->row_height / 2;
		inject_click_at(win, rx, ry);
		CHECK(poll_for_event(win, MKGUI_EVENT_GRID_CHECK, GV), "expected GRID_CHECK");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Drag/reorder: DRAG_START, DRAG_END, LISTVIEW_REORDER, LISTVIEW_COL_REORDER,
// TREEVIEW_MOVE, GRIDVIEW_REORDER
// ---------------------------------------------------------------------------

// [=]===^=[ test_listview_drag_start_end ]========================[=]
static void test_listview_drag_start_end(void) {
	TEST_BEGIN("listview: drag past threshold fires DRAG_START then DRAG_END");
	enum { WIN = 1, VBOX1 = 2, LV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LISTVIEW, LV,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		struct mkgui_column cols[] = { { "Name", 200, MKGUI_CELL_TEXT } };
		mkgui_listview_setup(win, LV, 10, 1, cols, dummy_row_cb, NULL);
		run_layout(win);
		int32_t idx = find_widget_idx(win, LV);
		int32_t rx = win->rects[idx].x + 10;
		int32_t row0 = win->rects[idx].y + win->row_height + 3;
		// press row 0, drag down past threshold, release back over the source
		// row so there is no valid reorder target -> DRAG_END (not REORDER).
		inject_press(win, rx, row0, 1, 0);
		inject_motion(win, rx, row0 + win->row_height * 2);
		CHECK(poll_for_event(win, MKGUI_EVENT_DRAG_START, LV), "expected DRAG_START");
		inject_motion(win, rx, row0);
		inject_release(win, rx, row0, 1);
		CHECK(poll_for_event(win, MKGUI_EVENT_DRAG_END, LV), "expected DRAG_END");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_listview_reorder ]===============================[=]
static void test_listview_reorder(void) {
	TEST_BEGIN("listview: drag row onto another fires LISTVIEW_REORDER");
	enum { WIN = 1, VBOX1 = 2, LV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LISTVIEW, LV,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		struct mkgui_column cols[] = { { "Name", 200, MKGUI_CELL_TEXT } };
		mkgui_listview_setup(win, LV, 10, 1, cols, dummy_row_cb, NULL);
		run_layout(win);
		int32_t idx = find_widget_idx(win, LV);
		int32_t rx = win->rects[idx].x + 10;
		int32_t row0 = win->rects[idx].y + win->row_height + 3;
		int32_t row3 = win->rects[idx].y + win->row_height * 4 + 3;
		inject_press(win, rx, row0, 1, 0);
		inject_motion(win, rx, row0 + 10);
		inject_motion(win, rx, row3);
		inject_release(win, rx, row3, 1);
		CHECK(poll_for_event(win, MKGUI_EVENT_LISTVIEW_REORDER, LV), "expected LISTVIEW_REORDER");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_listview_col_reorder ]===========================[=]
static void test_listview_col_reorder(void) {
	TEST_BEGIN("listview: drag column header fires LISTVIEW_COL_REORDER");
	enum { WIN = 1, VBOX1 = 2, LV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LISTVIEW, LV,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		struct mkgui_column cols[] = { { "Name", 200, MKGUI_CELL_TEXT }, { "Size", 100, MKGUI_CELL_TEXT } };
		mkgui_listview_setup(win, LV, 10, 2, cols, dummy_row_cb, NULL);
		run_layout(win);
		int32_t idx = find_widget_idx(win, LV);
		int32_t hy = win->rects[idx].y + win->row_height / 2;
		int32_t col0 = win->rects[idx].x + 100;
		// drop past the midpoint of column 1 so the insert position lands
		// after it (dst == 2); dropping inside col1 (dst == src+1) is a no-op.
		int32_t drop_x = win->rects[idx].x + 290;
		inject_press(win, col0, hy, 1, 0);
		inject_motion(win, drop_x, hy);
		inject_release(win, drop_x, hy, 1);
		CHECK(poll_for_event(win, MKGUI_EVENT_LISTVIEW_COL_REORDER, LV), "expected LISTVIEW_COL_REORDER");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_treeview_move ]==================================[=]
static void test_treeview_move(void) {
	TEST_BEGIN("treeview: drag node onto another fires TREEVIEW_MOVE");
	enum { WIN = 1, VBOX1 = 2, TV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TREEVIEW, TV,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		mkgui_treeview_setup(win, TV);
		mkgui_treeview_add(win, TV, 100, 0, "One");
		mkgui_treeview_add(win, TV, 101, 0, "Two");
		mkgui_treeview_add(win, TV, 102, 0, "Three");
		run_layout(win);
		int32_t idx = find_widget_idx(win, TV);
		int32_t rx = win->rects[idx].x + 40;
		int32_t row0 = win->rects[idx].y + win->row_height / 2 + 1;
		int32_t row2 = win->rects[idx].y + win->row_height * 2 + win->row_height / 2 + 1;
		inject_press(win, rx, row0, 1, 0);
		inject_motion(win, rx, row0 + 10);
		inject_motion(win, rx, row2);
		inject_release(win, rx, row2, 1);
		CHECK(poll_for_event(win, MKGUI_EVENT_TREEVIEW_MOVE, TV), "expected TREEVIEW_MOVE");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_gridview_reorder ]===============================[=]
static void test_gridview_reorder(void) {
	TEST_BEGIN("gridview: drag row onto another fires GRIDVIEW_REORDER");
	enum { WIN = 1, VBOX1 = 2, GV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_GRIDVIEW, GV,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		struct mkgui_grid_column cols[] = { { "A", 80, MKGUI_GRID_TEXT }, { "B", 80, MKGUI_GRID_TEXT } };
		mkgui_gridview_setup(win, GV, 10, 2, cols, dummy_grid_cell_cb, NULL);
		run_layout(win);
		int32_t idx = find_widget_idx(win, GV);
		int32_t rx = win->rects[idx].x + 10;
		int32_t row0 = win->rects[idx].y + win->row_height + win->row_height / 2;
		int32_t row3 = win->rects[idx].y + win->row_height * 4 + win->row_height / 2;
		inject_press(win, rx, row0, 1, 0);
		inject_motion(win, rx, row0 + 10);
		inject_motion(win, rx, row3);
		inject_release(win, rx, row3, 1);
		CHECK(poll_for_event(win, MKGUI_EVENT_GRIDVIEW_REORDER, GV), "expected GRIDVIEW_REORDER");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Cell edit commit
// ---------------------------------------------------------------------------

// [=]===^=[ test_cell_edit_commit ]===============================[=]
static void test_cell_edit_commit(void) {
	TEST_BEGIN("listview: editing a cell and pressing Enter fires CELL_EDIT_COMMIT");
	enum { WIN = 1, VBOX1 = 2, LV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LISTVIEW, LV,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, MKGUI_LISTVIEW_EDITABLE, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		struct mkgui_column cols[] = { { "Name", 200, MKGUI_CELL_TEXT } };
		mkgui_listview_setup(win, LV, 10, 1, cols, dummy_row_cb, NULL);
		run_layout(win);
		win->focus_id = LV;
		celledit_begin(win, LV, 0, 0, "edit me");
		CHECK(win->cell_edit.active, "cell edit did not start");
		inject_key(win, MKGUI_KEY_RETURN, 0);
		CHECK(poll_for_event(win, MKGUI_EVENT_CELL_EDIT_COMMIT, LV), "expected CELL_EDIT_COMMIT");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Canvas: PRESS, MOTION, RELEASE
// ---------------------------------------------------------------------------

// [=]===^=[ test_canvas_events ]==================================[=]
static void test_canvas_events(void) {
	TEST_BEGIN("canvas: press/motion/release fire CANVAS_PRESS/MOTION/RELEASE");
	enum { WIN = 1, CV = 2 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN, "Test", "", 0,   400, 300, 0, MKGUI_WINDOW_CANVAS, 0),
		MKGUI_W(MKGUI_CANVAS, CV,  "",     "", WIN, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 2, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		inject_press(win, 50, 50, 1, 0);
		CHECK(poll_for_event(win, MKGUI_EVENT_CANVAS_PRESS, CV), "expected CANVAS_PRESS");
		inject_motion(win, 60, 60);
		CHECK(poll_for_event(win, MKGUI_EVENT_CANVAS_MOTION, CV), "expected CANVAS_MOTION");
		inject_release(win, 60, 60, 1);
		CHECK(poll_for_event(win, MKGUI_EVENT_CANVAS_RELEASE, CV), "expected CANVAS_RELEASE");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Window-level events: RESIZE, CLOSE, FILE_DROP
// ---------------------------------------------------------------------------

// [=]===^=[ test_window_resize ]==================================[=]
static void test_window_resize(void) {
	TEST_BEGIN("window: resize fires RESIZE");
	enum { WIN = 1 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN, "Test", "", 0, 400, 300, 0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 1, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		inject_resize(win, win->win_w + 64, win->win_h + 48);
		CHECK(poll_for_event(win, MKGUI_EVENT_RESIZE, 0), "expected RESIZE");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_window_close ]===================================[=]
static void test_window_close(void) {
	TEST_BEGIN("window: close request fires CLOSE");
	enum { WIN = 1 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN, "Test", "", 0, 400, 300, 0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 1, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		inject_close(win);
		CHECK(poll_for_event(win, MKGUI_EVENT_CLOSE, 0), "expected CLOSE");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_file_drop ]======================================[=]
static void test_file_drop(void) {
	TEST_BEGIN("window: dropping files fires FILE_DROP");
	enum { WIN = 1 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN, "Test", "", 0, 400, 300, 0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 1, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		drop_add_path(win->mkwin,"/tmp/dropped_a.txt");
		drop_add_path(win->mkwin,"/tmp/dropped_b.txt");
		inject_drop(win);
		CHECK(poll_for_event(win, MKGUI_EVENT_FILE_DROP, 0), "expected FILE_DROP");
		CHECK(mkgui_drop_count(win) == 2, "expected drop_count == 2");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Logview: LINE_CLICKED
// ---------------------------------------------------------------------------

// [=]===^=[ test_logview_line_clicked ]===========================[=]
static void test_logview_line_clicked(void) {
	TEST_BEGIN("logview: clicking a line fires LOGVIEW_LINE_CLICKED");
	enum { WIN = 1, VBOX1 = 2, LV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,  WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,    VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LOGVIEW, LV,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		mkgui_logview_setup(win, LV, 256, 64 * 1024);
		mkgui_logview_append(win, LV, "first line\n");
		mkgui_logview_append(win, LV, "second line\n");
		mkgui_logview_append(win, LV, "third line\n");
		run_layout(win);
		int32_t idx = find_widget_idx(win, LV);
		int32_t rx = win->rects[idx].x + 10;
		int32_t ry = win->rects[idx].y + win->row_height / 2 + 1;
		inject_click_at(win, rx, ry);
		CHECK(poll_for_event(win, MKGUI_EVENT_LOGVIEW_LINE_CLICKED, LV), "expected LOGVIEW_LINE_CLICKED");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Payload assertions: events must carry the correct row/col/state
// ---------------------------------------------------------------------------

// [=]===^=[ test_listview_reorder_payload ]=======================[=]
static void test_listview_reorder_payload(void) {
	TEST_BEGIN("listview: REORDER carries source row in value, target in col");
	enum { WIN = 1, VBOX1 = 2, LV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LISTVIEW, LV,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		struct mkgui_column cols[] = { { "Name", 200, MKGUI_CELL_TEXT } };
		mkgui_listview_setup(win, LV, 10, 1, cols, dummy_row_cb, NULL);
		run_layout(win);
		int32_t idx = find_widget_idx(win, LV);
		int32_t rx = win->rects[idx].x + 10;
		int32_t row1 = win->rects[idx].y + win->row_height * 2 + 3;
		int32_t row4 = win->rects[idx].y + win->row_height * 5 + 3;
		inject_press(win, rx, row1, 1, 0);
		inject_motion(win, rx, row1 + 10);
		inject_motion(win, rx, row4);
		inject_release(win, rx, row4, 1);
		struct mkgui_event ev = poll_capture(win, MKGUI_EVENT_LISTVIEW_REORDER, LV);
		CHECK(ev.type == MKGUI_EVENT_LISTVIEW_REORDER, "expected REORDER");
		CHECK(ev.value == 1, "source row %d != 1", ev.value);
		CHECK(ev.col == 4, "target row %d != 4", ev.col);
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_gridview_check_payload ]=========================[=]
static void test_gridview_check_payload(void) {
	TEST_BEGIN("gridview: GRID_CHECK carries row/col and toggles state");
	enum { WIN = 1, VBOX1 = 2, GV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_GRIDVIEW, GV,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		struct mkgui_grid_column cols[] = { { "On", 40, MKGUI_GRID_CHECK }, { "Name", 120, MKGUI_GRID_TEXT } };
		mkgui_gridview_setup(win, GV, 10, 2, cols, dummy_grid_cell_cb, NULL);
		run_layout(win);
		CHECK(mkgui_gridview_get_check(win, GV, 2, 0) == 0, "cell should start unchecked");
		int32_t idx = find_widget_idx(win, GV);
		int32_t rx = win->rects[idx].x + 20;
		int32_t ry = win->rects[idx].y + win->row_height * 3 + win->row_height / 2;
		inject_click_at(win, rx, ry);
		struct mkgui_event ev = poll_capture(win, MKGUI_EVENT_GRID_CHECK, GV);
		CHECK(ev.type == MKGUI_EVENT_GRID_CHECK, "expected GRID_CHECK");
		CHECK(ev.value == 2, "row %d != 2", ev.value);
		CHECK(ev.col == 0, "col %d != 0", ev.col);
		CHECK(mkgui_gridview_get_check(win, GV, 2, 0) == 1, "cell should be checked after click");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_treeview_move_payload ]==========================[=]
static void test_treeview_move_payload(void) {
	TEST_BEGIN("treeview: MOVE carries source and target node ids");
	enum { WIN = 1, VBOX1 = 2, TV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TREEVIEW, TV,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		mkgui_treeview_setup(win, TV);
		mkgui_treeview_add(win, TV, 100, 0, "One");
		mkgui_treeview_add(win, TV, 101, 0, "Two");
		mkgui_treeview_add(win, TV, 102, 0, "Three");
		run_layout(win);
		int32_t idx = find_widget_idx(win, TV);
		int32_t rx = win->rects[idx].x + 40;
		int32_t row0 = win->rects[idx].y + win->row_height / 2 + 1;
		int32_t row2 = win->rects[idx].y + win->row_height * 2 + win->row_height / 2 + 1;
		inject_press(win, rx, row0, 1, 0);
		inject_motion(win, rx, row0 + 10);
		inject_motion(win, rx, row2);
		inject_release(win, rx, row2, 1);
		struct mkgui_event ev = poll_capture(win, MKGUI_EVENT_TREEVIEW_MOVE, TV);
		CHECK(ev.type == MKGUI_EVENT_TREEVIEW_MOVE, "expected TREEVIEW_MOVE");
		CHECK(ev.value == 100, "source node %d != 100", ev.value);
		CHECK(ev.col == 102, "target node %d != 102", ev.col);
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Negative cases: events must NOT fire when they shouldn't
// ---------------------------------------------------------------------------

// [=]===^=[ test_disabled_listview_no_select ]====================[=]
static void test_disabled_listview_no_select(void) {
	TEST_BEGIN("listview: click on a DISABLED listview fires no SELECT");
	enum { WIN = 1, VBOX1 = 2, LV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LISTVIEW, LV,    "",     "", VBOX1, 0,   200, MKGUI_FIXED | MKGUI_DISABLED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		struct mkgui_column cols[] = { { "Name", 200, MKGUI_CELL_TEXT } };
		mkgui_listview_setup(win, LV, 10, 1, cols, dummy_row_cb, NULL);
		run_layout(win);
		int32_t idx = find_widget_idx(win, LV);
		int32_t rx = win->rects[idx].x + 10;
		int32_t ry = win->rects[idx].y + win->row_height + 5;
		inject_click_at(win, rx, ry);
		CHECK(!poll_for_event(win, MKGUI_EVENT_LISTVIEW_SELECT, LV), "disabled listview must not fire SELECT");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_listview_below_threshold_no_reorder ]============[=]
static void test_listview_below_threshold_no_reorder(void) {
	TEST_BEGIN("listview: a sub-threshold drag fires neither DRAG_START nor REORDER");
	enum { WIN = 1, VBOX1 = 2, LV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LISTVIEW, LV,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		struct mkgui_column cols[] = { { "Name", 200, MKGUI_CELL_TEXT } };
		mkgui_listview_setup(win, LV, 10, 1, cols, dummy_row_cb, NULL);
		run_layout(win);
		int32_t idx = find_widget_idx(win, LV);
		int32_t rx = win->rects[idx].x + 10;
		int32_t row0 = win->rects[idx].y + win->row_height + 3;
		// move only 2px: below the 4px drag activation threshold
		inject_press(win, rx, row0, 1, 0);
		inject_motion(win, rx, row0 + 2);
		inject_release(win, rx, row0 + 2, 1);
		CHECK(!poll_for_event(win, MKGUI_EVENT_DRAG_START, LV), "must not fire DRAG_START below threshold");
		CHECK(!poll_for_event(win, MKGUI_EVENT_LISTVIEW_REORDER, LV), "must not fire REORDER below threshold");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_listview_click_empty_no_select ]=================[=]
static void test_listview_click_empty_no_select(void) {
	TEST_BEGIN("listview: click below the last row fires no SELECT");
	enum { WIN = 1, VBOX1 = 2, LV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LISTVIEW, LV,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		// only 2 rows; click well below them but still inside the widget
		struct mkgui_column cols[] = { { "Name", 200, MKGUI_CELL_TEXT } };
		mkgui_listview_setup(win, LV, 2, 1, cols, dummy_row_cb, NULL);
		run_layout(win);
		int32_t idx = find_widget_idx(win, LV);
		int32_t rx = win->rects[idx].x + 10;
		int32_t ry = win->rects[idx].y + win->rects[idx].h - 4;
		inject_click_at(win, rx, ry);
		CHECK(!poll_for_event(win, MKGUI_EVENT_LISTVIEW_SELECT, LV), "empty-area click must not fire SELECT");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

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
	test_treelist_select();
	test_treelist_dblclick();
	test_treelist_twisty_expand();
	test_treelist_expand_key();
	test_treelist_collapse_key();
	test_treelist_sort();
	test_treelist_col_reorder();
	test_treelist_context_header();
	test_treelist_sort_keeps_hierarchy();
	test_treelist_selection_survives_expand();
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
	test_timer_wakes_idle_on_transient_window();
	test_window_self_close_from_timer();
	test_context_menu_event();
	test_gridview_keyboard_select();
	test_gridview_check();
	test_listview_drag_start_end();
	test_listview_reorder();
	test_listview_col_reorder();
	test_treeview_move();
	test_gridview_reorder();
	test_cell_edit_commit();
	test_canvas_events();
	test_window_resize();
	test_window_close();
	test_file_drop();
	test_logview_line_clicked();
	test_listview_reorder_payload();
	test_gridview_check_payload();
	test_treeview_move_payload();
	test_disabled_listview_no_select();
	test_listview_below_threshold_no_reorder();
	test_listview_click_empty_no_select();

	printf("\n==================================\n");
	printf("%u tests: %u passed, %u failed\n", tests_run, tests_passed, tests_failed);
	return tests_failed ? 1 : 0;
}
