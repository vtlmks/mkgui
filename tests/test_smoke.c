// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT
//
// Smoke + coverage-gap tests for mkgui:
//   - instantiate every widget type and run a layout + render pass
//   - SPINNER / RICHLIST / GLVIEW setup and getters
//   - mkgui_is_shown parent-chain walk (hidden, collapsed group, inactive tab)
//   - category B getters (window size, anim time, col count/label, today)
//   - iconbrowser ibt_grid_metrics pure math

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
// Callback stubs for widgets that require one before render
// ---------------------------------------------------------------------------

// [=]===^=[ smoke_row_cb ]===========================================[=]
static void smoke_row_cb(uint32_t row, uint32_t col, char *buf, uint32_t buf_size, void *userdata) {
	(void)userdata;
	snprintf(buf, buf_size, "r%uc%u", row, col);
}

// [=]===^=[ smoke_grid_cell_cb ]=====================================[=]
static void smoke_grid_cell_cb(uint32_t row, uint32_t col, char *buf, uint32_t buf_size, void *userdata) {
	(void)userdata;
	snprintf(buf, buf_size, "g%u.%u", row, col);
}

// [=]===^=[ smoke_richlist_cb ]======================================[=]
static void smoke_richlist_cb(uint32_t row, struct mkgui_richlist_row *out, void *userdata) {
	(void)userdata;
	snprintf(out->title, sizeof(out->title), "title %u", row);
	snprintf(out->subtitle, sizeof(out->subtitle), "subtitle %u", row);
	snprintf(out->right_text, sizeof(out->right_text), "%u", row);
	out->thumbnail = NULL;
	out->thumb_w = 0;
	out->thumb_h = 0;
}

// [=]===^=[ smoke_itemview_label_cb ]================================[=]
static void smoke_itemview_label_cb(uint32_t item, char *buf, uint32_t buf_size, void *userdata) {
	(void)userdata;
	snprintf(buf, buf_size, "item %u", item);
}

// [=]===^=[ smoke_itemview_icon_cb ]=================================[=]
static void smoke_itemview_icon_cb(uint32_t item, char *buf, uint32_t buf_size, void *userdata) {
	(void)item;
	(void)userdata;
	snprintf(buf, buf_size, "folder");
}

// [=]===^=[ smoke_canvas_cb ]========================================[=]
static void smoke_canvas_cb(struct mkgui_ctx *ctx, uint32_t id, uint32_t *pixels, int32_t x, int32_t y, int32_t w, int32_t h, void *userdata) {
	(void)ctx;
	(void)id;
	(void)pixels;
	(void)x;
	(void)y;
	(void)w;
	(void)h;
	(void)userdata;
}

// ---------------------------------------------------------------------------
// Smoke: instantiate every widget type, run layout + render
// ---------------------------------------------------------------------------

enum {
	S_WIN = 1,
	S_MENU,
	S_MI_FILE,
	S_MI_NEW,
	S_MI_SEP,
	S_MI_QUIT,
	S_TOOLBAR,
	S_TB_BTN,
	S_VBOX_ROOT,
	S_HBOX_TOP,
	S_LABEL,
	S_BUTTON,
	S_CHECKBOX,
	S_RADIO1,
	S_RADIO2,
	S_TOGGLE,
	S_DIVIDER,
	S_SPACER,
	S_FORM,
	S_INPUT,
	S_IPINPUT,
	S_SPINBOX,
	S_DROPDOWN,
	S_COMBOBOX,
	S_SLIDER,
	S_PROGRESS,
	S_METER,
	S_SPINNER,
	S_DATEPICKER,
	S_GROUP,
	S_GROUP_LABEL,
	S_PANEL,
	S_PANEL_LABEL,
	S_TEXTAREA,
	S_PATHBAR,
	S_LISTVIEW,
	S_GRIDVIEW,
	S_TREEVIEW,
	S_ITEMVIEW,
	S_RICHLIST,
	S_CANVAS,
	S_IMAGE,
	S_GLVIEW,
	S_SCROLLBAR,
	S_TABS,
	S_TAB_A,
	S_TAB_B,
	S_TAB_A_LBL,
	S_TAB_B_LBL,
	S_HSPLIT,
	S_HSPLIT_L,
	S_HSPLIT_R,
	S_VSPLIT,
	S_VSPLIT_T,
	S_VSPLIT_B,
	S_STATUSBAR,
};

// [=]===^=[ test_smoke_all_widget_types ]===========================[=]
static void test_smoke_all_widget_types(void) {
	TEST_BEGIN("smoke: every widget type lays out and renders");
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,     S_WIN,         "Smoke", "", 0,              1600, 1400, 0, 0, 0),
		MKGUI_W(MKGUI_MENU,       S_MENU,        "",      "", S_WIN,          0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_MENUITEM,   S_MI_FILE,     "File",  "", S_MENU,         0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_MENUITEM,   S_MI_NEW,      "New",   "", S_MI_FILE,      0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_MENUITEM,   S_MI_SEP,      "",      "", S_MI_FILE,      0,   0,   0, MKGUI_MENUITEM_SEPARATOR, 0),
		MKGUI_W(MKGUI_MENUITEM,   S_MI_QUIT,     "Quit",  "", S_MI_FILE,      0,   0,   0, 0, 0),

		MKGUI_W(MKGUI_TOOLBAR,    S_TOOLBAR,     "",      "", S_WIN,          0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,     S_TB_BTN,      "Run",   "", S_TOOLBAR,      0,   0,   0, 0, 0),

		MKGUI_W(MKGUI_VBOX,       S_VBOX_ROOT,   "",      "", S_WIN,          0,   0,   0, 0, 0),

		MKGUI_W(MKGUI_HBOX,       S_HBOX_TOP,    "",      "", S_VBOX_ROOT,    0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LABEL,      S_LABEL,       "Label", "", S_HBOX_TOP,     0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,     S_BUTTON,      "OK",    "", S_HBOX_TOP,     0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_CHECKBOX,   S_CHECKBOX,    "Chk",   "", S_HBOX_TOP,     0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_RADIO,      S_RADIO1,      "A",     "", S_HBOX_TOP,     0,   0,   0, MKGUI_RADIO_CHECKED, 0),
		MKGUI_W(MKGUI_RADIO,      S_RADIO2,      "B",     "", S_HBOX_TOP,     0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TOGGLE,     S_TOGGLE,      "Tog",   "", S_HBOX_TOP,     0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_DIVIDER,    S_DIVIDER,     "",      "", S_HBOX_TOP,     0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_SPACER,     S_SPACER,      "",      "", S_HBOX_TOP,     0,   0,   0, 0, 1),

		MKGUI_W(MKGUI_FORM,       S_FORM,        "",      "", S_VBOX_ROOT,    0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_INPUT,      S_INPUT,       "",      "", S_FORM,         0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_IPINPUT,    S_IPINPUT,     "",      "", S_FORM,         0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_SPINBOX,    S_SPINBOX,     "",      "", S_FORM,         0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_DROPDOWN,   S_DROPDOWN,    "",      "", S_FORM,         0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_COMBOBOX,   S_COMBOBOX,    "",      "", S_FORM,         0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_SLIDER,     S_SLIDER,      "",      "", S_FORM,         0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_PROGRESS,   S_PROGRESS,    "",      "", S_FORM,         0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_METER,      S_METER,       "",      "", S_FORM,         0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_SPINNER,    S_SPINNER,     "",      "", S_FORM,         32,  32,  MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_DATEPICKER, S_DATEPICKER,  "",      "", S_FORM,         0,   0,   0, 0, 0),

		MKGUI_W(MKGUI_GROUP,      S_GROUP,       "Group", "", S_VBOX_ROOT,    0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LABEL,      S_GROUP_LABEL, "inside","", S_GROUP,        0,   0,   0, 0, 0),

		MKGUI_W(MKGUI_PANEL,      S_PANEL,       "",      "", S_VBOX_ROOT,    0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LABEL,      S_PANEL_LABEL, "panel", "", S_PANEL,        0,   0,   0, 0, 0),

		MKGUI_W(MKGUI_TEXTAREA,   S_TEXTAREA,    "",      "", S_VBOX_ROOT,    0,   80,  MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_PATHBAR,    S_PATHBAR,     "",      "", S_VBOX_ROOT,    0,   0,   0, 0, 0),

		MKGUI_W(MKGUI_LISTVIEW,   S_LISTVIEW,    "",      "", S_VBOX_ROOT,    0,   80,  MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_GRIDVIEW,   S_GRIDVIEW,    "",      "", S_VBOX_ROOT,    0,   80,  MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_TREEVIEW,   S_TREEVIEW,    "",      "", S_VBOX_ROOT,    0,   80,  MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_ITEMVIEW,   S_ITEMVIEW,    "",      "", S_VBOX_ROOT,    0,   80,  MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_RICHLIST,   S_RICHLIST,    "",      "", S_VBOX_ROOT,    0,   80,  MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_CANVAS,     S_CANVAS,      "",      "", S_VBOX_ROOT,    0,   60,  MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_IMAGE,      S_IMAGE,       "",      "", S_VBOX_ROOT,    0,   40,  MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_GLVIEW,     S_GLVIEW,      "",      "", S_VBOX_ROOT,    0,   60,  MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_SCROLLBAR,  S_SCROLLBAR,   "",      "", S_VBOX_ROOT,    0,   14,  MKGUI_FIXED, 0, 0),

		MKGUI_W(MKGUI_TABS,       S_TABS,        "",      "", S_VBOX_ROOT,    0,   120, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_TAB,        S_TAB_A,       "A",     "", S_TABS,         0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LABEL,      S_TAB_A_LBL,   "tab A", "", S_TAB_A,        0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TAB,        S_TAB_B,       "B",     "", S_TABS,         0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LABEL,      S_TAB_B_LBL,   "tab B", "", S_TAB_B,        0,   0,   0, 0, 0),

		MKGUI_W(MKGUI_HSPLIT,     S_HSPLIT,      "",      "", S_VBOX_ROOT,    0,   80,  MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_PANEL,      S_HSPLIT_L,    "",      "", S_HSPLIT,       0,   0,   MKGUI_REGION_LEFT, 0, 0),
		MKGUI_W(MKGUI_PANEL,      S_HSPLIT_R,    "",      "", S_HSPLIT,       0,   0,   MKGUI_REGION_RIGHT, 0, 0),

		MKGUI_W(MKGUI_VSPLIT,     S_VSPLIT,      "",      "", S_VBOX_ROOT,    0,   80,  MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_PANEL,      S_VSPLIT_T,    "",      "", S_VSPLIT,       0,   0,   MKGUI_REGION_TOP, 0, 0),
		MKGUI_W(MKGUI_PANEL,      S_VSPLIT_B,    "",      "", S_VSPLIT,       0,   0,   MKGUI_REGION_BOTTOM, 0, 0),

		MKGUI_W(MKGUI_STATUSBAR,  S_STATUSBAR,   "ready", "", S_WIN,          0,   0,   0, 0, 0),
	};

	uint32_t count = sizeof(widgets) / sizeof(widgets[0]);
	struct mkgui_ctx *ctx = mkgui_create(widgets, count);
	CHECK(ctx, "mkgui_create failed");
	if(ctx) {
	// Minimum setup so the view widgets have something to render
	struct mkgui_column lv_cols[2] = {
		{ "Col A", 100, 0 },
		{ "Col B", 100, 0 },
	};
	struct mkgui_grid_column gv_cols[2] = {
		{ "G1", 80, 0 },
		{ "G2", 80, 0 },
	};
	const char *cb_items[] = { "one", "two", "three" };

	mkgui_listview_setup(ctx, S_LISTVIEW, 5, 2, lv_cols, smoke_row_cb, NULL);
	mkgui_gridview_setup(ctx, S_GRIDVIEW, 5, 2, gv_cols, smoke_grid_cell_cb, NULL);
	mkgui_treeview_setup(ctx, S_TREEVIEW);
	mkgui_treeview_add(ctx, S_TREEVIEW, 1, 0, "root");
	mkgui_treeview_add(ctx, S_TREEVIEW, 2, 1, "child");
	mkgui_itemview_setup(ctx, S_ITEMVIEW, 4, MKGUI_VIEW_DETAIL, smoke_itemview_label_cb, smoke_itemview_icon_cb, NULL);
	mkgui_richlist_setup(ctx, S_RICHLIST, 5, 48, smoke_richlist_cb, NULL);
	mkgui_canvas_set_callback(ctx, S_CANVAS, smoke_canvas_cb, NULL);
	mkgui_combobox_setup(ctx, S_COMBOBOX, cb_items, 3);
	mkgui_pathbar_set(ctx, S_PATHBAR, "/tmp/smoke");
	mkgui_datepicker_set(ctx, S_DATEPICKER, 2026, 4, 22);

	layout_widgets(ctx);

	// All expected widgets should have indexed rects.
	CHECK(ctx->widget_count == count, "widget_count mismatch: got %u want %u", ctx->widget_count, count);
	// Layout is allowed to produce negative dims under extreme overflow; only
	// assert that each widget's id resolves back to a valid index.
	for(uint32_t i = 0; i < ctx->widget_count; ++i) {
		int32_t idx = find_widget_idx(ctx, ctx->widgets[i].id);
		CHECK(idx == (int32_t)i, "find_widget_idx(%u) = %d want %u", ctx->widgets[i].id, idx, i);
	}

	// Render pass: force full redraw and render all widgets
	ctx->dirty_full = 1;
	render_widgets(ctx);

	// A second render pass with dirty_full cleared exercises the incremental path
	ctx->dirty_full = 0;
	ctx->dirty_count = 0;
	render_widgets(ctx);

	mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// mkgui_is_shown parent-chain walk
// ---------------------------------------------------------------------------

// [=]===^=[ test_is_shown_basic ]===================================[=]
static void test_is_shown_basic(void) {
	TEST_BEGIN("is_shown: visible widget returns 1");
	enum { WIN = 1, VBOX1 = 2, LBL = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "T", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",  "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LABEL,  LBL,   "x", "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		layout_widgets(ctx);
		CHECK(mkgui_is_shown(ctx, LBL) == 1, "label should be shown");
		CHECK(mkgui_is_shown(ctx, WIN) == 1, "window should be shown");
		CHECK(mkgui_is_shown(ctx, 9999) == 0, "unknown id returns 0");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_is_shown_hidden_self ]=============================[=]
static void test_is_shown_hidden_self(void) {
	TEST_BEGIN("is_shown: HIDDEN flag on self returns 0");
	enum { WIN = 1, VBOX1 = 2, LBL = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "T", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",  "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LABEL,  LBL,   "x", "", VBOX1, 0,   0,   MKGUI_HIDDEN, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		layout_widgets(ctx);
		CHECK(mkgui_is_shown(ctx, LBL) == 0, "hidden label should not be shown");
		CHECK(mkgui_is_shown(ctx, VBOX1) == 1, "parent still visible");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_is_shown_hidden_ancestor ]=========================[=]
static void test_is_shown_hidden_ancestor(void) {
	TEST_BEGIN("is_shown: HIDDEN ancestor hides descendants");
	enum { WIN = 1, OUTER = 2, INNER = 3, LBL = 4 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "T", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   OUTER, "",  "", WIN,   0,   0,   MKGUI_HIDDEN, 0, 0),
		MKGUI_W(MKGUI_HBOX,   INNER, "",  "", OUTER, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LABEL,  LBL,   "x", "", INNER, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 4);
	CHECK(ctx, "create failed");
	if(ctx) {
		layout_widgets(ctx);
		CHECK(mkgui_is_shown(ctx, LBL) == 0, "label hidden via ancestor");
		CHECK(mkgui_is_shown(ctx, INNER) == 0, "inner hidden via ancestor");
		CHECK(mkgui_is_shown(ctx, OUTER) == 0, "hidden outer itself");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_is_shown_collapsed_group ]=========================[=]
static void test_is_shown_collapsed_group(void) {
	TEST_BEGIN("is_shown: collapsed GROUP hides children");
	enum { WIN = 1, VBOX1 = 2, GRP = 3, LBL = 4 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "T",     "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",      "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_GROUP,  GRP,   "Group", "", VBOX1, 0,   0,   0, MKGUI_GROUP_COLLAPSED, 0),
		MKGUI_W(MKGUI_LABEL,  LBL,   "x",     "", GRP,   0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 4);
	CHECK(ctx, "create failed");
	if(ctx) {
		layout_widgets(ctx);
		CHECK(mkgui_is_shown(ctx, LBL) == 0, "child of collapsed group is hidden");
		CHECK(mkgui_is_shown(ctx, GRP) == 1, "the group header itself still shows");
		mkgui_group_set_collapsed(ctx, GRP, 0);
		layout_widgets(ctx);
		CHECK(mkgui_is_shown(ctx, LBL) == 1, "expanding the group reveals children");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_is_shown_inactive_tab ]============================[=]
static void test_is_shown_inactive_tab(void) {
	TEST_BEGIN("is_shown: inactive TAB hides its descendants");
	enum { WIN = 1, TABS = 2, TA = 3, TB = 4, LA = 5, LB = 6 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,  "T", "", 0,    400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_TABS,   TABS, "",  "", WIN,  0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TAB,    TA,   "A", "", TABS, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LABEL,  LA,   "a", "", TA,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TAB,    TB,   "B", "", TABS, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LABEL,  LB,   "b", "", TB,   0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 6);
	CHECK(ctx, "create failed");
	if(ctx) {
		layout_widgets(ctx);
		uint32_t current = mkgui_tabs_get_current(ctx, TABS);
		CHECK(current == TA, "first tab should be active by default (got %u)", current);
		CHECK(mkgui_is_shown(ctx, LA) == 1, "active tab child shows");
		CHECK(mkgui_is_shown(ctx, LB) == 0, "inactive tab child hidden");
		mkgui_tabs_set_current(ctx, TABS, TB);
		layout_widgets(ctx);
		CHECK(mkgui_is_shown(ctx, LA) == 0, "old active tab child now hidden");
		CHECK(mkgui_is_shown(ctx, LB) == 1, "newly active tab child shows");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Category B getters
// ---------------------------------------------------------------------------

// [=]===^=[ test_get_window_size ]==================================[=]
static void test_get_window_size(void) {
	TEST_BEGIN("get_window_size: returns configured w/h");
	enum { WIN = 1 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN, "T", "", 0, 640, 480, 0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 1);
	CHECK(ctx, "create failed");
	if(ctx) {
		int32_t w = 0, h = 0;
		mkgui_get_window_size(ctx, &w, &h);
		CHECK(w == 640, "w expected 640 got %d", w);
		CHECK(h == 480, "h expected 480 got %d", h);
		// NULL-out parameters are allowed
		mkgui_get_window_size(ctx, NULL, NULL);
		mkgui_get_window_size(ctx, &w, NULL);
		CHECK(w == 640, "w-only readback still works");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_get_anim_time ]====================================[=]
static void test_get_anim_time(void) {
	TEST_BEGIN("get_anim_time: returns non-negative time");
	enum { WIN = 1 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN, "T", "", 0, 200, 100, 0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 1);
	CHECK(ctx, "create failed");
	if(ctx) {
		double t = mkgui_get_anim_time(ctx);
		CHECK(t >= 0.0, "anim_time should be non-negative, got %f", t);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_today ]============================================[=]
static void test_today(void) {
	TEST_BEGIN("mkgui_today: returns a plausible date");
	int32_t y = 0, m = 0, d = 0;
	mkgui_today(&y, &m, &d);
	CHECK(y >= 2024 && y <= 2100, "year out of range: %d", y);
	CHECK(m >= 1 && m <= 12, "month out of range: %d", m);
	CHECK(d >= 1 && d <= 31, "day out of range: %d", d);
	// NULL-out parameters are allowed
	mkgui_today(NULL, NULL, NULL);
	mkgui_today(&y, NULL, NULL);
	CHECK(y >= 2024, "year-only readback still works");
	TEST_END();
}

// [=]===^=[ test_listview_col_getters ]=============================[=]
static void test_listview_col_getters(void) {
	TEST_BEGIN("listview: col count/label getters");
	enum { WIN = 1, VBOX1 = 2, LV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "T", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",  "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LISTVIEW, LV,    "",  "", VBOX1, 0,   100, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		struct mkgui_column cols[3] = {
			{ "Name",  100, 0 },
			{ "Size",  80,  0 },
			{ "Date",  120, 0 },
		};
		// Before setup: count is 0, label is ""
		CHECK(mkgui_listview_get_col_count(ctx, LV) == 0, "count is 0 pre-setup");
		CHECK(mkgui_listview_get_col_label(ctx, LV, 0)[0] == '\0', "label empty pre-setup");

		mkgui_listview_setup(ctx, LV, 10, 3, cols, smoke_row_cb, NULL);
		CHECK(mkgui_listview_get_col_count(ctx, LV) == 3, "count should be 3");
		CHECK(strcmp(mkgui_listview_get_col_label(ctx, LV, 0), "Name") == 0, "col 0 label");
		CHECK(strcmp(mkgui_listview_get_col_label(ctx, LV, 1), "Size") == 0, "col 1 label");
		CHECK(strcmp(mkgui_listview_get_col_label(ctx, LV, 2), "Date") == 0, "col 2 label");
		CHECK(mkgui_listview_get_col_label(ctx, LV, 3)[0] == '\0', "out-of-range label is empty");
		CHECK(mkgui_listview_get_col_count(ctx, 9999) == 0, "unknown id returns 0");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_gridview_col_getters ]=============================[=]
static void test_gridview_col_getters(void) {
	TEST_BEGIN("gridview: col count/label getters");
	enum { WIN = 1, VBOX1 = 2, GV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "T", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",  "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_GRIDVIEW, GV,    "",  "", VBOX1, 0,   100, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		struct mkgui_grid_column cols[2] = {
			{ "Key",   80, 0 },
			{ "Value", 80, 0 },
		};
		CHECK(mkgui_gridview_get_col_count(ctx, GV) == 0, "count is 0 pre-setup");
		CHECK(mkgui_gridview_get_col_label(ctx, GV, 0)[0] == '\0', "label empty pre-setup");

		mkgui_gridview_setup(ctx, GV, 4, 2, cols, smoke_grid_cell_cb, NULL);
		CHECK(mkgui_gridview_get_col_count(ctx, GV) == 2, "count should be 2");
		CHECK(strcmp(mkgui_gridview_get_col_label(ctx, GV, 0), "Key") == 0, "col 0 label");
		CHECK(strcmp(mkgui_gridview_get_col_label(ctx, GV, 1), "Value") == 0, "col 1 label");
		CHECK(mkgui_gridview_get_col_label(ctx, GV, 7)[0] == '\0', "out-of-range label is empty");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// SPINNER / RICHLIST / GLVIEW setup
// ---------------------------------------------------------------------------

// [=]===^=[ test_spinner_instantiate ]==============================[=]
static void test_spinner_instantiate(void) {
	TEST_BEGIN("spinner: instantiates, lays out with a rect, renders");
	enum { WIN = 1, VBOX1 = 2, SP = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,  WIN,   "T", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,    VBOX1, "",  "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_SPINNER, SP,    "",  "", VBOX1, 48,  48,  MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		layout_widgets(ctx);
		int32_t idx = find_widget_idx(ctx, SP);
		CHECK(idx >= 0, "spinner must be findable");
		if(idx >= 0) {
			CHECK(ctx->rects[idx].w > 0 && ctx->rects[idx].h > 0, "spinner rect must be non-empty");
		}
		// render pass should not crash and should advance via anim_time
		ctx->anim_time = 0.25;
		ctx->dirty_full = 1;
		render_widgets(ctx);
		ctx->anim_time = 0.75;
		ctx->dirty_full = 1;
		render_widgets(ctx);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_richlist_setup_get_set ]===========================[=]
static void test_richlist_setup_get_set(void) {
	TEST_BEGIN("richlist: setup/get_selected/set_selected/set_rows");
	enum { WIN = 1, VBOX1 = 2, RL = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "T", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",  "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_RICHLIST, RL,    "",  "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		// Before setup: -1 selected, set_selected is a no-op (no richlist_data yet)
		CHECK(mkgui_richlist_get_selected(ctx, RL) == -1, "unset richlist reports -1 selected");

		mkgui_richlist_setup(ctx, RL, 20, 56, smoke_richlist_cb, NULL);
		CHECK(mkgui_richlist_get_selected(ctx, RL) == -1, "fresh richlist has no selection");

		mkgui_richlist_set_selected(ctx, RL, 7);
		CHECK(mkgui_richlist_get_selected(ctx, RL) == 7, "set/get roundtrip");

		mkgui_richlist_set_rows(ctx, RL, 5);
		// Selection is intentionally not auto-clamped; just check API doesn't crash.
		mkgui_richlist_scroll_to(ctx, RL, 3);

		// NULL callback to setup is a no-op
		mkgui_richlist_setup(ctx, RL, 10, 56, NULL, NULL);
		// The still-valid richlist_data should retain its selection
		CHECK(mkgui_richlist_get_selected(ctx, RL) == 7, "NULL-cb setup must not clobber state");

		// Rendering must still work after API churn
		layout_widgets(ctx);
		ctx->dirty_full = 1;
		render_widgets(ctx);

		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_glview_setup ]=====================================[=]
static void test_glview_setup(void) {
	TEST_BEGIN("glview: init/get_size/destroy");
	enum { WIN = 1, VBOX1 = 2, GV = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "T", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",  "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_GLVIEW, GV,    "",  "", VBOX1, 0,   120, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		// get_size before init: both zero
		int32_t w = -1, h = -1;
		mkgui_glview_get_size(ctx, GV, &w, &h);
		CHECK(w == 0 && h == 0, "pre-init size should be zero (got %dx%d)", w, h);

		uint32_t ok = mkgui_glview_init(ctx, GV);
		CHECK(ok == 1, "glview_init should succeed on valid widget");

		// Second init returns the existing 'created' flag, not a fresh one
		uint32_t ok2 = mkgui_glview_init(ctx, GV);
		CHECK(ok2 == 1, "glview_init is idempotent");

		mkgui_glview_get_size(ctx, GV, &w, &h);
		CHECK(w > 0 && h > 0, "post-init size should be positive (got %dx%d)", w, h);

		// get_size on unknown id returns zero
		int32_t uw = -1, uh = -1;
		mkgui_glview_get_size(ctx, 9999, &uw, &uh);
		CHECK(uw == 0 && uh == 0, "unknown id returns zero size");

		// get_size tolerates NULL out pointers
		mkgui_glview_get_size(ctx, GV, NULL, NULL);

		mkgui_glview_destroy(ctx, GV);
		mkgui_glview_get_size(ctx, GV, &w, &h);
		CHECK(w == 0 && h == 0, "post-destroy size should be zero");

		// destroy is idempotent (no-op if already destroyed)
		mkgui_glview_destroy(ctx, GV);

		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Iconbrowser grid metrics (pure math, no dialog needed)
// ---------------------------------------------------------------------------

// [=]===^=[ test_iconbrowser_grid_metrics ]=========================[=]
static void test_iconbrowser_grid_metrics(void) {
	TEST_BEGIN("iconbrowser: ibt_grid_metrics math across sizes");
	enum { WIN = 1 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN, "T", "", 0, 400, 300, 0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 1);
	CHECK(ctx, "create failed");
	if(ctx) {
		// The function uses the iconbrowser global. Allocate a temporary state
		// matching what the real dialog uses.
		struct ib_theme_state *saved = ibt;
		ibt = (struct ib_theme_state *)calloc(1, sizeof(struct ib_theme_state));
		CHECK(ibt != NULL, "calloc ib_theme_state");
		if(ibt) {
			int32_t row_h, col_w, rows_per_col, total_cols, total_w;
			int32_t expect_row_h = sc(ctx, IB_GRID_ROW_H_PX);
			int32_t expect_col_w = sc(ctx, IB_GRID_COL_W_PX);

			// Case 1: empty filter -> 0 columns, total_w clamps to canvas width
			ibt->filtered_count = 0;
			ibt_grid_metrics(ctx, 500, 220, &row_h, &col_w, &rows_per_col, &total_cols, &total_w);
			CHECK(row_h == expect_row_h, "row_h scaled (got %d, want %d)", row_h, expect_row_h);
			CHECK(col_w == expect_col_w, "col_w scaled (got %d, want %d)", col_w, expect_col_w);
			CHECK(rows_per_col == 220 / expect_row_h, "rows_per_col = ca_h/row_h (got %d)", rows_per_col);
			CHECK(total_cols == 0, "empty filter yields 0 cols");
			CHECK(total_w == 500, "total_w clamped up to canvas width");

			// Case 2: skinny canvas forces rows_per_col >= 1 even when h < row_h
			ibt->filtered_count = 10;
			ibt_grid_metrics(ctx, 300, 5, &row_h, &col_w, &rows_per_col, &total_cols, &total_w);
			CHECK(rows_per_col == 1, "rows_per_col floor is 1, got %d", rows_per_col);
			CHECK(total_cols == 10, "10 items at 1 row/col -> 10 cols");
			int32_t expect_total_w_2 = 10 * expect_col_w;
			if(expect_total_w_2 < 300) {
				expect_total_w_2 = 300;
			}
			CHECK(total_w == expect_total_w_2, "total_w got %d want %d", total_w, expect_total_w_2);

			// Case 3: ceiling division when items don't fill last column
			ibt->filtered_count = 11;
			ibt_grid_metrics(ctx, 200, expect_row_h * 4, &row_h, &col_w, &rows_per_col, &total_cols, &total_w);
			CHECK(rows_per_col == 4, "rows_per_col = 4");
			CHECK(total_cols == 3, "ceil(11 / 4) = 3 cols, got %d", total_cols);

			// Case 4: items fit in a single partial column
			ibt->filtered_count = 3;
			ibt_grid_metrics(ctx, 100, expect_row_h * 10, &row_h, &col_w, &rows_per_col, &total_cols, &total_w);
			CHECK(total_cols == 1, "3 items fit in 1 col when rows_per_col=10");

			free(ibt);
		}
		ibt = saved;
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Toast and banner API
// ---------------------------------------------------------------------------

// [=]===^=[ test_toast_basic ]=======================================[=]
static void test_toast_basic(void) {
	TEST_BEGIN("toast: add one, verify state, clear");
	enum { WIN = 1 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN, "T", "", 0, 400, 300, 0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 1);
	CHECK(ctx, "create failed");
	if(ctx) {
		mkgui_toast(ctx, "hello");
		uint32_t active = 0;
		for(uint32_t i = 0; i < MKGUI_MAX_TOASTS; ++i) {
			if(ctx->toasts[i].active) {
				++active;
				CHECK(ctx->toasts[i].severity == MKGUI_SEVERITY_INFO, "default severity should be INFO");
				CHECK(ctx->toasts[i].expire_ms != 0, "default duration should not be persistent");
				CHECK(strcmp(ctx->toasts[i].text, "hello") == 0, "text mismatch: %s", ctx->toasts[i].text);
			}
		}

		CHECK(active == 1, "expected 1 active toast, got %u", active);

		mkgui_toast_clear(ctx);
		active = 0;
		for(uint32_t i = 0; i < MKGUI_MAX_TOASTS; ++i) {
			if(ctx->toasts[i].active) {
				++active;
			}
		}

		CHECK(active == 0, "toast_clear should remove all toasts, got %u active", active);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_toast_severity_and_persistent ]=====================[=]
static void test_toast_severity_and_persistent(void) {
	TEST_BEGIN("toast: severity + duration=0 (persistent)");
	enum { WIN = 1 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN, "T", "", 0, 400, 300, 0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 1);
	if(ctx) {
		mkgui_toast_ex(ctx, MKGUI_SEVERITY_ERROR, "boom", 0);
		uint32_t found = 0;
		for(uint32_t i = 0; i < MKGUI_MAX_TOASTS; ++i) {
			struct mkgui_toast_slot *t = &ctx->toasts[i];
			if(t->active) {
				CHECK(t->severity == MKGUI_SEVERITY_ERROR, "severity should be ERROR, got %u", t->severity);
				CHECK(t->expire_ms == 0, "duration=0 should yield persistent toast");
				CHECK(strcmp(t->text, "boom") == 0, "text mismatch");
				++found;
			}
		}

		CHECK(found == 1, "expected 1 active toast, got %u", found);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_toast_overflow_evicts_oldest ]======================[=]
static void test_toast_overflow_evicts_oldest(void) {
	TEST_BEGIN("toast: overflow beyond MKGUI_MAX_TOASTS evicts oldest");
	enum { WIN = 1 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN, "T", "", 0, 400, 300, 0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 1);
	if(ctx) {
		char buf[32];
		for(uint32_t i = 0; i < MKGUI_MAX_TOASTS; ++i) {
			snprintf(buf, sizeof(buf), "t%u", i);
			mkgui_toast(ctx, buf);
		}
		uint32_t active = 0;
		for(uint32_t i = 0; i < MKGUI_MAX_TOASTS; ++i) {
			if(ctx->toasts[i].active) {
				++active;
			}
		}

		CHECK(active == MKGUI_MAX_TOASTS, "expected all %u slots filled, got %u", MKGUI_MAX_TOASTS, active);

		// One more: oldest (t0) should be dropped, newest ("overflow") should be in last slot
		mkgui_toast(ctx, "overflow");
		active = 0;
		uint32_t has_t0 = 0;
		uint32_t has_overflow = 0;
		for(uint32_t i = 0; i < MKGUI_MAX_TOASTS; ++i) {
			if(ctx->toasts[i].active) {
				++active;
				if(strcmp(ctx->toasts[i].text, "t0") == 0) {
					has_t0 = 1;
				}
				if(strcmp(ctx->toasts[i].text, "overflow") == 0) {
					has_overflow = 1;
				}
			}
		}

		CHECK(active == MKGUI_MAX_TOASTS, "still expected %u active after overflow, got %u", MKGUI_MAX_TOASTS, active);
		CHECK(!has_t0, "oldest toast t0 should have been evicted");
		CHECK(has_overflow, "newest toast 'overflow' should be present");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_toast_expiry ]======================================[=]
static void test_toast_expiry(void) {
	TEST_BEGIN("toast: toasts_expire removes timed-out toasts");
	enum { WIN = 1 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN, "T", "", 0, 400, 300, 0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 1);
	if(ctx) {
		// Short-lived toast: 1ms
		mkgui_toast_ex(ctx, MKGUI_SEVERITY_INFO, "short", 1);
		// Persistent toast: 0ms
		mkgui_toast_ex(ctx, MKGUI_SEVERITY_INFO, "persist", 0);

		// Ensure the 1ms one has already expired before calling toasts_expire
		struct timespec ts;
		ts.tv_sec = 0;
		ts.tv_nsec = 5 * 1000 * 1000; // 5ms
		nanosleep(&ts, NULL);
		uint32_t removed = toasts_expire(ctx);
		CHECK(removed, "toasts_expire should have removed the short-lived toast");

		uint32_t active = 0;
		uint32_t has_persist = 0;
		for(uint32_t i = 0; i < MKGUI_MAX_TOASTS; ++i) {
			if(ctx->toasts[i].active) {
				++active;
				if(strcmp(ctx->toasts[i].text, "persist") == 0) {
					has_persist = 1;
				}
			}
		}

		CHECK(active == 1, "expected 1 surviving toast, got %u", active);
		CHECK(has_persist, "persistent toast should still be active");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_banner_set_clear ]==================================[=]
static void test_banner_set_clear(void) {
	TEST_BEGIN("banner: set, active, replace, clear");
	enum { WIN = 1 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN, "T", "", 0, 400, 300, 0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 1);
	if(ctx) {
		CHECK(!mkgui_banner_active(ctx), "banner should start inactive");

		mkgui_banner_set(ctx, MKGUI_SEVERITY_WARNING, "unsaved");
		CHECK(mkgui_banner_active(ctx), "banner should be active after set");
		CHECK(ctx->banner.severity == MKGUI_SEVERITY_WARNING, "severity mismatch: %u", ctx->banner.severity);
		CHECK(strcmp(ctx->banner.text, "unsaved") == 0, "text mismatch: %s", ctx->banner.text);

		// Replace
		mkgui_banner_set(ctx, MKGUI_SEVERITY_ERROR, "offline");
		CHECK(mkgui_banner_active(ctx), "banner should still be active after replace");
		CHECK(ctx->banner.severity == MKGUI_SEVERITY_ERROR, "severity after replace mismatch");
		CHECK(strcmp(ctx->banner.text, "offline") == 0, "text after replace mismatch");

		mkgui_banner_clear(ctx);
		CHECK(!mkgui_banner_active(ctx), "banner should be inactive after clear");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Canvas-window render path
// ---------------------------------------------------------------------------

static int32_t cw_cb_x, cw_cb_y, cw_cb_w, cw_cb_h;
static uint32_t cw_cb_called;

// [=]===^=[ canvas_window_draw_cb ]==================================[=]
static void canvas_window_draw_cb(struct mkgui_ctx *ctx, uint32_t id, uint32_t *pixels, int32_t x, int32_t y, int32_t w, int32_t h, void *userdata) {
	(void)ctx; (void)id; (void)pixels; (void)userdata;
	cw_cb_x = x;
	cw_cb_y = y;
	cw_cb_w = w;
	cw_cb_h = h;
	cw_cb_called = 1;
}

// [=]===^=[ test_canvas_window_render ]===============================[=]
static void test_canvas_window_render(void) {
	TEST_BEGIN("canvas-window: callback receives full window dimensions");
	enum { WIN = 1, CANVAS = 2 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN, "T", "", 0, 275, 116, 0, MKGUI_WINDOW_UNDECORATED | MKGUI_WINDOW_CANVAS | MKGUI_WINDOW_HIDDEN, 0),
		MKGUI_W(MKGUI_CANVAS, CANVAS, "", "", WIN, 0, 0, 0, 0, 1),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 2);
	CHECK(ctx, "mkgui_create failed");
	if(ctx) {
		CHECK(ctx->canvas_window == 1, "canvas_window flag set");
		CHECK(ctx->undecorated == 1, "undecorated flag set");

		mkgui_canvas_set_callback(ctx, CANVAS, canvas_window_draw_cb, NULL);

		cw_cb_called = 0;
		layout_widgets(ctx);
		ctx->dirty_full = 1;
		render_widgets(ctx);

		CHECK(cw_cb_called == 1, "canvas callback was invoked");
		CHECK(cw_cb_x == 0, "callback x == 0, got %d", cw_cb_x);
		CHECK(cw_cb_y == 0, "callback y == 0, got %d", cw_cb_y);
		CHECK(cw_cb_w == ctx->win_w, "callback w == win_w (%d), got %d", ctx->win_w, cw_cb_w);
		CHECK(cw_cb_h == ctx->win_h, "callback h == win_h (%d), got %d", ctx->win_h, cw_cb_h);

		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(void) {
	printf("mkgui smoke + coverage-gap tests\n");
	printf("=================================\n\n");

	test_smoke_all_widget_types();

	test_is_shown_basic();
	test_is_shown_hidden_self();
	test_is_shown_hidden_ancestor();
	test_is_shown_collapsed_group();
	test_is_shown_inactive_tab();

	test_get_window_size();
	test_get_anim_time();
	test_today();
	test_listview_col_getters();
	test_gridview_col_getters();

	test_spinner_instantiate();
	test_richlist_setup_get_set();
	test_glview_setup();

	test_iconbrowser_grid_metrics();

	test_toast_basic();
	test_toast_severity_and_persistent();
	test_toast_overflow_evicts_oldest();
	test_toast_expiry();
	test_banner_set_clear();

	test_canvas_window_render();

	printf("\n=================================\n");
	printf("%u tests: %u passed, %u failed\n", tests_run, tests_passed, tests_failed);
	return tests_failed ? 1 : 0;
}
