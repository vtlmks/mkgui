// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT
//
// Layout test harness for mkgui.
// Creates widget trees, runs layout, and checks computed rects.
// Build: gcc -std=c99 -O2 test_layout.c -o test_layout $(pkg-config --cflags freetype2) -lX11 -lXext $(pkg-config --libs freetype2) -lm

#include "../mkgui.c"

static uint32_t tests_run;
static uint32_t tests_passed;
static uint32_t tests_failed;

#define TEST_BEGIN(name) \
	do { \
		++tests_run; \
		char *test_name__ = (name); \
		uint32_t fail__ = 0;

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
// Helpers
// ---------------------------------------------------------------------------

static struct mkgui_ctx *create_and_layout(struct mkgui_widget *widgets, uint32_t count) {
	struct mkgui_ctx *ctx = mkgui_create(widgets, count);
	if(!ctx) {
		fprintf(stderr, "FATAL: mkgui_create returned NULL\n");
		return NULL;
	}
	layout_widgets(ctx);
	return ctx;
}

static struct mkgui_rect get_rect(struct mkgui_ctx *ctx, uint32_t id) {
	int32_t idx = find_widget_idx(ctx, id);
	if(idx < 0) {
		struct mkgui_rect zero = {0, 0, 0, 0};
		return zero;
	}
	return ctx->rects[idx];
}

static void check_no_overlap_siblings(struct mkgui_ctx *ctx, uint32_t parent_id, char *test_name, uint32_t *fail) {
	uint32_t children[256];
	uint32_t child_count = 0;
	for(uint32_t i = 0; i < ctx->widget_count; ++i) {
		if(ctx->widgets[i].parent_id == parent_id && !(ctx->widgets[i].flags & MKGUI_HIDDEN)) {
			if(child_count < 256) {
				children[child_count++] = i;
			}
		}
	}
	for(uint32_t a = 0; a < child_count; ++a) {
		struct mkgui_rect ra = ctx->rects[children[a]];
		for(uint32_t b = a + 1; b < child_count; ++b) {
			struct mkgui_rect rb = ctx->rects[children[b]];
			uint32_t overlap_x = (ra.x < rb.x + rb.w) && (ra.x + ra.w > rb.x);
			uint32_t overlap_y = (ra.y < rb.y + rb.h) && (ra.y + ra.h > rb.y);
			if(overlap_x && overlap_y && ra.w > 0 && ra.h > 0 && rb.w > 0 && rb.h > 0) {
				fprintf(stderr, "  overlap: widget[%u] id=%u (%d,%d %dx%d) vs widget[%u] id=%u (%d,%d %dx%d)\n", children[a], ctx->widgets[children[a]].id, ra.x, ra.y, ra.w, ra.h, children[b], ctx->widgets[children[b]].id, rb.x, rb.y, rb.w, rb.h);
				*fail = 1;
			}
		}
	}
}

static void check_children_inside_parent(struct mkgui_ctx *ctx, uint32_t parent_id, char *test_name, uint32_t *fail) {
	int32_t pi = find_widget_idx(ctx, parent_id);
	if(pi < 0) {
		return;
	}
	struct mkgui_rect pr = ctx->rects[pi];
	for(uint32_t i = 0; i < ctx->widget_count; ++i) {
		if(ctx->widgets[i].parent_id == parent_id && !(ctx->widgets[i].flags & MKGUI_HIDDEN)) {
			struct mkgui_rect cr = ctx->rects[i];
			if(cr.w <= 0 || cr.h <= 0) {
				continue;
			}

			if(cr.x < pr.x || cr.y < pr.y || cr.x + cr.w > pr.x + pr.w || cr.y + cr.h > pr.y + pr.h) {
				fprintf(stderr, "  outside parent: widget[%u] id=%u (%d,%d %dx%d) parent id=%u (%d,%d %dx%d)\n", i, ctx->widgets[i].id, cr.x, cr.y, cr.w, cr.h, parent_id, pr.x, pr.y, pr.w, pr.h);
				*fail = 1;
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// [=]===^=[ test_vbox_basic ]=====================================[=]
static void test_vbox_basic(void) {
	TEST_BEGIN("vbox: children get positive size and don't overlap");
	enum { WIN = 1, VBOX1 = 2, BTN1 = 3, BTN2 = 4, BTN3 = 5 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,    600, 400, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,  0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN1,  "A",    "", VBOX1, 0,  0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN2,  "B",    "", VBOX1, 0,  0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN3,  "C",    "", VBOX1, 0,  0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 5);
	CHECK(ctx, "create failed");
	if(ctx) {
		for(uint32_t id = BTN1; id <= BTN3; ++id) {
			struct mkgui_rect r = get_rect(ctx, id);
			CHECK(r.w > 0, "id=%u w=%d", id, r.w);
			CHECK(r.h > 0, "id=%u h=%d", id, r.h);
		}
		check_no_overlap_siblings(ctx, VBOX1, test_name__, &fail__);
		check_children_inside_parent(ctx, VBOX1, test_name__, &fail__);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_hbox_basic ]=====================================[=]
static void test_hbox_basic(void) {
	TEST_BEGIN("hbox: children get positive size and don't overlap");
	enum { WIN = 1, HBOX1 = 2, BTN1 = 3, BTN2 = 4, BTN3 = 5 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     600, 400, 0, 0, 0),
		MKGUI_W(MKGUI_HBOX,   HBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN1,  "A",    "", HBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN2,  "B",    "", HBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN3,  "C",    "", HBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 5);
	CHECK(ctx, "create failed");
	if(ctx) {
		for(uint32_t id = BTN1; id <= BTN3; ++id) {
			struct mkgui_rect r = get_rect(ctx, id);
			CHECK(r.w > 0, "id=%u w=%d", id, r.w);
			CHECK(r.h > 0, "id=%u h=%d", id, r.h);
		}
		check_no_overlap_siblings(ctx, HBOX1, test_name__, &fail__);
		check_children_inside_parent(ctx, HBOX1, test_name__, &fail__);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_weights ]========================================[=]
static void test_weights(void) {
	TEST_BEGIN("vbox: weight=2 gets roughly double the extra space of weight=1");
	enum { WIN = 1, VBOX1 = 2, LBL1 = 3, LBL2 = 4 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     600, 400, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LABEL,  LBL1,  "A",    "", VBOX1, 0,   0,   0, 0, 1),
		MKGUI_W(MKGUI_LABEL,  LBL2,  "B",    "", VBOX1, 0,   0,   0, 0, 2),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 4);
	CHECK(ctx, "create failed");
	if(ctx) {
		struct mkgui_rect r1 = get_rect(ctx, LBL1);
		struct mkgui_rect r2 = get_rect(ctx, LBL2);
		CHECK(r1.h > 0, "lbl1 h=%d", r1.h);
		CHECK(r2.h > 0, "lbl2 h=%d", r2.h);
		CHECK(r2.h > r1.h, "weight=2 (%d) should be taller than weight=1 (%d)", r2.h, r1.h);
		int32_t nh = natural_height(ctx, MKGUI_LABEL);
		int32_t extra1 = r1.h - nh;
		int32_t extra2 = r2.h - nh;
		CHECK(extra1 > 0 && extra2 > 0, "both should get extra space: e1=%d e2=%d", extra1, extra2);
		int32_t tolerance = 3;
		CHECK(extra2 >= extra1 * 2 - tolerance && extra2 <= extra1 * 2 + tolerance, "weight=2 extra=%d, expected ~%d (2x of %d)", extra2, extra1 * 2, extra1);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_fixed_width ]====================================[=]
static void test_fixed_width(void) {
	TEST_BEGIN("hbox: FIXED widget gets its declared width");
	enum { WIN = 1, HBOX1 = 2, BTN1 = 3, BTN2 = 4 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     600, 400, 0, 0, 0),
		MKGUI_W(MKGUI_HBOX,   HBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN1,  "Fix",  "", HBOX1, 120, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN2,  "Flex", "", HBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 4);
	CHECK(ctx, "create failed");
	if(ctx) {
		struct mkgui_rect r1 = get_rect(ctx, BTN1);
		struct mkgui_rect r2 = get_rect(ctx, BTN2);
		CHECK(r1.w == 120, "fixed btn w=%d, expected 120", r1.w);
		CHECK(r2.w > r1.w, "flex btn w=%d should be wider than fixed %d", r2.w, r1.w);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_hidden_no_space ]=================================[=]
static void test_hidden_no_space(void) {
	TEST_BEGIN("vbox: hidden widgets get zero rect and don't affect siblings");
	enum { WIN = 1, VBOX1 = 2, BTN1 = 3, BTN2 = 4, BTN3 = 5 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     600, 400, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN1,  "A",    "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN2,  "Hid",  "", VBOX1, 0,   0,   MKGUI_HIDDEN, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN3,  "C",    "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 5);
	CHECK(ctx, "create failed");
	if(ctx) {
		struct mkgui_rect r1 = get_rect(ctx, BTN1);
		struct mkgui_rect r3 = get_rect(ctx, BTN3);
		CHECK(r1.w > 0 && r1.h > 0, "btn1 should be visible");
		CHECK(r3.w > 0 && r3.h > 0, "btn3 should be visible");
		int32_t gap_between = r3.y - (r1.y + r1.h);
		CHECK(gap_between < r1.h, "gap between btn1 and btn3 (%d) should be small (no hidden btn space)", gap_between);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_chrome_no_overlap ]===============================[=]
static void test_chrome_no_overlap(void) {
	TEST_BEGIN("window: chrome (menu, toolbar, statusbar) does not overlap content");
	enum { WIN = 1, MENU1 = 2, MI1 = 3, TB = 4, TB_BTN = 5, SB = 6, VBOX1 = 7, BTN1 = 8 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,    WIN,    "Test",  "", 0,   600, 400, 0, 0, 0),
		MKGUI_W(MKGUI_MENU,      MENU1,  "",      "", WIN, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_MENUITEM,  MI1,    "File",  "", MENU1, 0, 0,   0, 0, 0),
		MKGUI_W(MKGUI_TOOLBAR,   TB,     "",      "", WIN, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,    TB_BTN, "Save",  "", TB,  0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_STATUSBAR, SB,     "",      "", WIN, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_VBOX,      VBOX1,  "",      "", WIN, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,    BTN1,   "Click", "", VBOX1, 0, 0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 8);
	CHECK(ctx, "create failed");
	if(ctx) {
		mkgui_statusbar_setup(ctx, SB, 1, (int32_t[]){-1});
		layout_widgets(ctx);
		struct mkgui_rect menu_r = get_rect(ctx, MENU1);
		struct mkgui_rect tb_r   = get_rect(ctx, TB);
		struct mkgui_rect sb_r   = get_rect(ctx, SB);
		struct mkgui_rect vbox_r = get_rect(ctx, VBOX1);

		CHECK(menu_r.h > 0, "menu h=%d", menu_r.h);
		CHECK(tb_r.h > 0, "toolbar h=%d", tb_r.h);
		CHECK(sb_r.h > 0, "statusbar h=%d", sb_r.h);
		CHECK(vbox_r.h > 0, "content vbox h=%d", vbox_r.h);

		CHECK(tb_r.y >= menu_r.y + menu_r.h, "toolbar y=%d should be below menu bottom=%d", tb_r.y, menu_r.y + menu_r.h);
		CHECK(vbox_r.y >= tb_r.y + tb_r.h, "content y=%d should be below toolbar bottom=%d", vbox_r.y, tb_r.y + tb_r.h);
		CHECK(vbox_r.y + vbox_r.h <= sb_r.y, "content bottom=%d should be above statusbar y=%d", vbox_r.y + vbox_r.h, sb_r.y);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_nested_containers ]===============================[=]
static void test_nested_containers(void) {
	TEST_BEGIN("nested: vbox inside hbox inside tab inside window");
	enum { WIN = 1, TABS = 2, TAB1 = 3, HBOX1 = 4, VBOX1 = 5, BTN1 = 6, BTN2 = 7, BTN3 = 8 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     600, 400, 0, 0, 0),
		MKGUI_W(MKGUI_TABS,   TABS,  "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TAB,    TAB1,  "Tab1", "", TABS,  0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_HBOX,   HBOX1, "",     "", TAB1,  0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", HBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN1,  "A",    "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN2,  "B",    "", HBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN3,  "C",    "", HBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 8);
	CHECK(ctx, "create failed");
	if(ctx) {
		for(uint32_t id = BTN1; id <= BTN3; ++id) {
			struct mkgui_rect r = get_rect(ctx, id);
			CHECK(r.w > 0, "id=%u w=%d", id, r.w);
			CHECK(r.h > 0, "id=%u h=%d", id, r.h);
		}
		check_no_overlap_siblings(ctx, HBOX1, test_name__, &fail__);
		check_children_inside_parent(ctx, HBOX1, test_name__, &fail__);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_split_children_positive ]=========================[=]
static void test_split_children_positive(void) {
	TEST_BEGIN("vsplit: both panes get positive dimensions");
	enum { WIN = 1, SPLIT = 2, LEFT = 3, RIGHT = 4 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     600, 400, 0, 0, 0),
		MKGUI_W(MKGUI_VSPLIT, SPLIT, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   LEFT,  "",     "", SPLIT, 0,   0,   MKGUI_REGION_LEFT, 0, 0),
		MKGUI_W(MKGUI_VBOX,   RIGHT, "",     "", SPLIT, 0,   0,   MKGUI_REGION_RIGHT, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 4);
	CHECK(ctx, "create failed");
	if(ctx) {
		struct mkgui_rect lr = get_rect(ctx, LEFT);
		struct mkgui_rect rr = get_rect(ctx, RIGHT);
		CHECK(lr.w > 0, "left pane w=%d", lr.w);
		CHECK(lr.h > 0, "left pane h=%d", lr.h);
		CHECK(rr.w > 0, "right pane w=%d", rr.w);
		CHECK(rr.h > 0, "right pane h=%d", rr.h);
		CHECK(rr.x > lr.x, "right pane x=%d should be after left pane x=%d", rr.x, lr.x);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_hsplit_children_positive ]=========================[=]
static void test_hsplit_children_positive(void) {
	TEST_BEGIN("hsplit: both panes get positive dimensions");
	enum { WIN = 1, SPLIT = 2, TOP = 3, BOT = 4 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     600, 400, 0, 0, 0),
		MKGUI_W(MKGUI_HSPLIT, SPLIT, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   TOP,   "",     "", SPLIT, 0,   0,   MKGUI_REGION_TOP, 0, 0),
		MKGUI_W(MKGUI_VBOX,   BOT,   "",     "", SPLIT, 0,   0,   MKGUI_REGION_BOTTOM, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 4);
	CHECK(ctx, "create failed");
	if(ctx) {
		struct mkgui_rect tr = get_rect(ctx, TOP);
		struct mkgui_rect br = get_rect(ctx, BOT);
		CHECK(tr.w > 0, "top pane w=%d", tr.w);
		CHECK(tr.h > 0, "top pane h=%d", tr.h);
		CHECK(br.w > 0, "bottom pane w=%d", br.w);
		CHECK(br.h > 0, "bottom pane h=%d", br.h);
		CHECK(br.y > tr.y, "bottom pane y=%d should be below top pane y=%d", br.y, tr.y);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_many_children_no_zero ]===========================[=]
static void test_many_children_no_zero(void) {
	TEST_BEGIN("vbox: 20 flex children all get positive height");
	enum { WIN = 1, VBOX1 = 2 };
	struct mkgui_widget widgets[22];
	widgets[0] = (struct mkgui_widget)MKGUI_W(MKGUI_WINDOW, WIN, "Test", "", 0, 600, 400, 0, 0, 0);
	widgets[1] = (struct mkgui_widget)MKGUI_W(MKGUI_VBOX, VBOX1, "", "", WIN, 0, 0, 0, 0, 0);
	for(uint32_t i = 0; i < 20; ++i) {
		widgets[2 + i] = (struct mkgui_widget)MKGUI_W(MKGUI_BUTTON, 10 + i, "Btn", "", VBOX1, 0, 0, 0, 0, 0);
	}
	struct mkgui_ctx *ctx = create_and_layout(widgets, 22);
	CHECK(ctx, "create failed");
	if(ctx) {
		for(uint32_t i = 0; i < 20; ++i) {
			struct mkgui_rect r = get_rect(ctx, 10 + i);
			CHECK(r.h > 0, "id=%u h=%d (should be > 0)", 10 + i, r.h);
			CHECK(r.w > 0, "id=%u w=%d (should be > 0)", 10 + i, r.w);
		}
		check_no_overlap_siblings(ctx, VBOX1, test_name__, &fail__);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_small_window_no_zero ]=============================[=]
static void test_small_window_no_zero(void) {
	TEST_BEGIN("small window: widgets compress but stay positive");
	enum { WIN = 1, VBOX1 = 2, BTN1 = 3, BTN2 = 4, LV = 5, BTN3 = 6 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     200, 100, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   BTN1,  "A",    "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   BTN2,  "B",    "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LISTVIEW, LV,    "",     "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   BTN3,  "C",    "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 6);
	CHECK(ctx, "create failed");
	if(ctx) {
		for(uint32_t id = BTN1; id <= BTN3; ++id) {
			struct mkgui_rect r = get_rect(ctx, id);
			CHECK(r.w > 0, "id=%u w=%d", id, r.w);
			CHECK(r.h > 0, "id=%u h=%d", id, r.h);
		}
		struct mkgui_rect lv_r = get_rect(ctx, LV);
		CHECK(lv_r.w > 0, "listview w=%d", lv_r.w);
		CHECK(lv_r.h > 0, "listview h=%d", lv_r.h);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_form_layout ]=====================================[=]
static void test_form_layout(void) {
	TEST_BEGIN("form: label+control pairs don't overlap and stay inside form");
	enum { WIN = 1, FORM1 = 2, LBL1 = 3, INP1 = 4, LBL2 = 5, INP2 = 6 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test",  "", 0,     600, 400, 0, 0, 0),
		MKGUI_W(MKGUI_FORM,   FORM1, "",      "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LABEL,  LBL1,  "Name",  "", FORM1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_INPUT,  INP1,  "",       "", FORM1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LABEL,  LBL2,  "Email", "", FORM1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_INPUT,  INP2,  "",       "", FORM1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 6);
	CHECK(ctx, "create failed");
	if(ctx) {
		struct mkgui_rect l1 = get_rect(ctx, LBL1);
		struct mkgui_rect i1 = get_rect(ctx, INP1);
		struct mkgui_rect l2 = get_rect(ctx, LBL2);
		CHECK(l1.w > 0 && l1.h > 0, "label1 has size");
		CHECK(i1.w > 0 && i1.h > 0, "input1 has size");
		CHECK(l1.x + l1.w <= i1.x, "label1 right=%d should be left of input1 x=%d", l1.x + l1.w, i1.x);
		CHECK(l2.y > l1.y, "row2 y=%d should be below row1 y=%d", l2.y, l1.y);
		check_children_inside_parent(ctx, FORM1, test_name__, &fail__);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_group_collapsed ]=================================[=]
static void test_group_collapsed(void) {
	TEST_BEGIN("group: collapsed group in vbox uses only header height");
	enum { WIN = 1, VBOX1 = 2, GRP = 3, BTN1 = 4, BTN2 = 5 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     600, 400, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_GROUP,  GRP,   "Grp",  "", VBOX1, 0,   0,   MKGUI_FIXED, MKGUI_GROUP_COLLAPSIBLE | MKGUI_GROUP_COLLAPSED, 0),
		MKGUI_W(MKGUI_BUTTON, BTN1,  "A",    "", GRP,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN2,  "B",    "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 5);
	CHECK(ctx, "create failed");
	if(ctx) {
		struct mkgui_rect gr = get_rect(ctx, GRP);
		int32_t header_h = ctx->font_height + 4;
		CHECK(gr.h == header_h, "collapsed group h=%d, expected header_h=%d", gr.h, header_h);
		struct mkgui_rect br = get_rect(ctx, BTN2);
		CHECK(br.h > 0, "sibling button below collapsed group h=%d", br.h);
		CHECK(br.y >= gr.y + gr.h, "sibling y=%d should be below collapsed group bottom=%d", br.y, gr.y + gr.h);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_hbox_many_children_no_zero ]======================[=]
static void test_hbox_many_children_no_zero(void) {
	TEST_BEGIN("hbox: 15 flex children all get positive width");
	enum { WIN = 1, HBOX1 = 2 };
	struct mkgui_widget widgets[17];
	widgets[0] = (struct mkgui_widget)MKGUI_W(MKGUI_WINDOW, WIN, "Test", "", 0, 600, 400, 0, 0, 0);
	widgets[1] = (struct mkgui_widget)MKGUI_W(MKGUI_HBOX, HBOX1, "", "", WIN, 0, 0, 0, 0, 0);
	for(uint32_t i = 0; i < 15; ++i) {
		widgets[2 + i] = (struct mkgui_widget)MKGUI_W(MKGUI_BUTTON, 10 + i, "B", "", HBOX1, 0, 0, 0, 0, 0);
	}
	struct mkgui_ctx *ctx = create_and_layout(widgets, 17);
	CHECK(ctx, "create failed");
	if(ctx) {
		for(uint32_t i = 0; i < 15; ++i) {
			struct mkgui_rect r = get_rect(ctx, 10 + i);
			CHECK(r.w > 0, "id=%u w=%d (should be > 0)", 10 + i, r.w);
			CHECK(r.h > 0, "id=%u h=%d (should be > 0)", 10 + i, r.h);
		}
		check_no_overlap_siblings(ctx, HBOX1, test_name__, &fail__);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_mixed_fixed_flex ]================================[=]
static void test_mixed_fixed_flex(void) {
	TEST_BEGIN("vbox: mixed fixed + flex children, flex gets remaining space");
	enum { WIN = 1, VBOX1 = 2, BTN1 = 3, LV = 4, BTN2 = 5 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     600, 400, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   BTN1,  "Top",  "", VBOX1, 0,   40,  MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_LISTVIEW, LV,    "",     "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   BTN2,  "Bot",  "", VBOX1, 0,   40,  MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 5);
	CHECK(ctx, "create failed");
	if(ctx) {
		struct mkgui_rect b1 = get_rect(ctx, BTN1);
		struct mkgui_rect lv = get_rect(ctx, LV);
		struct mkgui_rect b2 = get_rect(ctx, BTN2);
		CHECK(b1.h == 40, "fixed top h=%d, expected 40", b1.h);
		CHECK(b2.h == 40, "fixed bot h=%d, expected 40", b2.h);
		CHECK(lv.h > 40, "flex listview h=%d should be larger than fixed buttons", lv.h);
		check_no_overlap_siblings(ctx, VBOX1, test_name__, &fail__);
		check_children_inside_parent(ctx, VBOX1, test_name__, &fail__);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_tabs_content_inside ]==============================[=]
static void test_tabs_content_inside(void) {
	TEST_BEGIN("tabs: tab content stays inside tab area");
	enum { WIN = 1, TABS1 = 2, TAB1 = 3, TAB2 = 4, BTN1 = 5, BTN2 = 6 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,    600, 400, 0, 0, 0),
		MKGUI_W(MKGUI_TABS,   TABS1, "",     "", WIN,  0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TAB,    TAB1,  "T1",   "", TABS1, 0,  0,   0, 0, 0),
		MKGUI_W(MKGUI_TAB,    TAB2,  "T2",   "", TABS1, 0,  0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN1,  "A",    "", TAB1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN2,  "B",    "", TAB2, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 6);
	CHECK(ctx, "create failed");
	if(ctx) {
		struct mkgui_rect tabs_r = get_rect(ctx, TABS1);
		struct mkgui_rect btn1_r = get_rect(ctx, BTN1);
		CHECK(tabs_r.w > 0 && tabs_r.h > 0, "tabs has size");
		CHECK(btn1_r.w > 0 && btn1_r.h > 0, "tab1 btn has size");
		CHECK(btn1_r.x >= tabs_r.x && btn1_r.y >= tabs_r.y && btn1_r.x + btn1_r.w <= tabs_r.x + tabs_r.w && btn1_r.y + btn1_r.h <= tabs_r.y + tabs_r.h, "btn (%d,%d %dx%d) outside tabs (%d,%d %dx%d)", btn1_r.x, btn1_r.y, btn1_r.w, btn1_r.h, tabs_r.x, tabs_r.y, tabs_r.w, tabs_r.h);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_resize_smaller ]=================================[=]
static void test_resize_smaller(void) {
	TEST_BEGIN("resize: shrinking window keeps all widgets positive");
	enum { WIN = 1, VBOX1 = 2, BTN1 = 3, LV = 4, BTN2 = 5 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     600, 400, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   BTN1,  "A",    "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LISTVIEW, LV,    "",     "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   BTN2,  "B",    "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 5);
	CHECK(ctx, "create failed");
	if(ctx) {
		int32_t sizes[] = { 300, 200, 150, 120, 100 };
		for(uint32_t s = 0; s < 5; ++s) {
			ctx->win_h = sizes[s];
			dirty_all(ctx);
			layout_widgets(ctx);
			for(uint32_t id = BTN1; id <= BTN2; ++id) {
				struct mkgui_rect r = get_rect(ctx, id);
				CHECK(r.w > 0, "h=%d id=%u w=%d", sizes[s], id, r.w);
				CHECK(r.h > 0, "h=%d id=%u h=%d", sizes[s], id, r.h);
			}
			struct mkgui_rect lv_r = get_rect(ctx, LV);
			CHECK(lv_r.w > 0, "h=%d listview w=%d", sizes[s], lv_r.w);
			CHECK(lv_r.h > 0, "h=%d listview h=%d", sizes[s], lv_r.h);
		}
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_split_resize_no_negative ]========================[=]
static void test_split_resize_no_negative(void) {
	TEST_BEGIN("vsplit: small window doesn't produce negative pane sizes");
	enum { WIN = 1, SPLIT = 2, LEFT = 3, RIGHT = 4, BTN1 = 5, BTN2 = 6 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     200, 150, 0, 0, 0),
		MKGUI_W(MKGUI_VSPLIT, SPLIT, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   LEFT,  "",     "", SPLIT, 0,   0,   MKGUI_REGION_LEFT, 0, 0),
		MKGUI_W(MKGUI_VBOX,   RIGHT, "",     "", SPLIT, 0,   0,   MKGUI_REGION_RIGHT, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN1,  "L",    "", LEFT,  0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN2,  "R",    "", RIGHT, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 6);
	CHECK(ctx, "create failed");
	if(ctx) {
		struct mkgui_rect lr = get_rect(ctx, LEFT);
		struct mkgui_rect rr = get_rect(ctx, RIGHT);
		CHECK(lr.w > 0, "left pane w=%d", lr.w);
		CHECK(rr.w > 0, "right pane w=%d", rr.w);
		struct mkgui_rect b1 = get_rect(ctx, BTN1);
		struct mkgui_rect b2 = get_rect(ctx, BTN2);
		CHECK(b1.w > 0, "left btn w=%d", b1.w);
		CHECK(b2.w > 0, "right btn w=%d", b2.w);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_full_chrome_stack ]===============================[=]
static void test_full_chrome_stack(void) {
	TEST_BEGIN("window: menu + toolbar + pathbar + statusbar leaves positive content area");
	enum { WIN = 1, MENU1 = 2, MI1 = 3, TB = 4, TB_BTN = 5, PB = 6, SB = 7, VBOX1 = 8, BTN1 = 9 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,    WIN,    "Test",  "", 0,   600, 400, 0, 0, 0),
		MKGUI_W(MKGUI_MENU,      MENU1,  "",      "", WIN, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_MENUITEM,  MI1,    "File",  "", MENU1, 0, 0,   0, 0, 0),
		MKGUI_W(MKGUI_TOOLBAR,   TB,     "",      "", WIN, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,    TB_BTN, "Save",  "", TB,  0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_PATHBAR,   PB,     "",      "", WIN, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_STATUSBAR, SB,     "",      "", WIN, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_VBOX,      VBOX1,  "",      "", WIN, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,    BTN1,   "Click", "", VBOX1, 0, 0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 9);
	CHECK(ctx, "create failed");
	if(ctx) {
		mkgui_statusbar_setup(ctx, SB, 1, (int32_t[]){-1});
		layout_widgets(ctx);
		struct mkgui_rect vbox_r = get_rect(ctx, VBOX1);
		CHECK(vbox_r.h > 0, "content area h=%d after full chrome stack", vbox_r.h);
		CHECK(vbox_r.w > 0, "content area w=%d after full chrome stack", vbox_r.w);
		struct mkgui_rect btn_r = get_rect(ctx, BTN1);
		CHECK(btn_r.h > 0, "button in content h=%d", btn_r.h);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_panel_border ]====================================[=]
static void test_panel_border(void) {
	TEST_BEGIN("panel: children stay inside bordered panel");
	enum { WIN = 1, PANEL1 = 2, BTN1 = 3, BTN2 = 4 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,    "Test", "", 0,      600, 400, 0, 0, 0),
		MKGUI_W(MKGUI_PANEL,  PANEL1, "",     "", WIN,    0,   0,   0, MKGUI_PANEL_BORDER, 0),
		MKGUI_W(MKGUI_BUTTON, BTN1,   "A",    "", PANEL1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN2,   "B",    "", PANEL1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 4);
	CHECK(ctx, "create failed");
	if(ctx) {
		check_children_inside_parent(ctx, PANEL1, test_name__, &fail__);
		check_no_overlap_siblings(ctx, PANEL1, test_name__, &fail__);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_no_widget_zero_size ]=============================[=]
static void test_no_widget_zero_size(void) {
	TEST_BEGIN("universal: no visible widget should have zero width or height");
	enum { WIN = 1, VBOX1 = 2, BTN1 = 3, INP1 = 4, CB = 5, DD = 6, SL = 7, PG = 8, LBL1 = 9 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test",   "", 0,     600, 400, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",        "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   BTN1,  "Button",  "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_INPUT,    INP1,  "",        "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_CHECKBOX, CB,    "Check",   "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_DROPDOWN, DD,    "",        "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_SLIDER,   SL,    "",        "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_PROGRESS, PG,    "",        "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LABEL,    LBL1,  "Label",   "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 9);
	CHECK(ctx, "create failed");
	if(ctx) {
		for(uint32_t i = 0; i < ctx->widget_count; ++i) {
			if(ctx->widgets[i].flags & MKGUI_HIDDEN) {
				continue;
			}

			if(ctx->widgets[i].type == MKGUI_WINDOW) {
				continue;
			}
			struct mkgui_rect r = ctx->rects[i];
			CHECK(r.w > 0, "widget[%u] id=%u type=%u w=%d", i, ctx->widgets[i].id, ctx->widgets[i].type, r.w);
			CHECK(r.h > 0, "widget[%u] id=%u type=%u h=%d", i, ctx->widgets[i].id, ctx->widgets[i].type, r.h);
		}
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_listview_min_height ]=============================[=]
static void test_listview_min_height(void) {
	TEST_BEGIN("vbox: listview under pressure keeps minimum height (3 rows)");
	enum { WIN = 1, VBOX1 = 2, BTN1 = 3, BTN2 = 4, BTN3 = 5, BTN4 = 6, BTN5 = 7, LV = 8 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     300, 200, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   BTN1,  "A",    "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   BTN2,  "B",    "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   BTN3,  "C",    "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   BTN4,  "D",    "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   BTN5,  "E",    "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LISTVIEW, LV,    "",     "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 8);
	CHECK(ctx, "create failed");
	if(ctx) {
		struct mkgui_rect lv_r = get_rect(ctx, LV);
		int32_t min_h = ctx->row_height * 3;
		CHECK(lv_r.h >= min_h, "listview h=%d, minimum should be %d (3 rows)", lv_r.h, min_h);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_textarea_min_height ]=============================[=]
static void test_textarea_min_height(void) {
	TEST_BEGIN("vbox: textarea under pressure keeps minimum height");
	enum { WIN = 1, VBOX1 = 2, BTN1 = 3, BTN2 = 4, BTN3 = 5, BTN4 = 6, TA = 7 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     300, 200, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   BTN1,  "A",    "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   BTN2,  "B",    "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   BTN3,  "C",    "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   BTN4,  "D",    "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TEXTAREA, TA,    "",     "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 7);
	CHECK(ctx, "create failed");
	if(ctx) {
		struct mkgui_rect ta_r = get_rect(ctx, TA);
		int32_t min_h = ctx->row_height * 3;
		CHECK(ta_r.h >= min_h, "textarea h=%d, minimum should be %d (3 rows)", ta_r.h, min_h);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_hbox_min_width ]==================================[=]
static void test_hbox_min_width(void) {
	TEST_BEGIN("hbox: flex children keep minimum width under pressure");
	enum { WIN = 1, HBOX1 = 2 };
	struct mkgui_widget widgets[22];
	widgets[0] = (struct mkgui_widget)MKGUI_W(MKGUI_WINDOW, WIN, "Test", "", 0, 200, 200, 0, 0, 0);
	widgets[1] = (struct mkgui_widget)MKGUI_W(MKGUI_HBOX, HBOX1, "", "", WIN, 0, 0, 0, 0, 0);
	for(uint32_t i = 0; i < 20; ++i) {
		widgets[2 + i] = (struct mkgui_widget)MKGUI_W(MKGUI_BUTTON, 10 + i, "B", "", HBOX1, 0, 0, 0, 0, 0);
	}
	struct mkgui_ctx *ctx = create_and_layout(widgets, 22);
	CHECK(ctx, "create failed");
	if(ctx) {
		int32_t min_w = natural_width(ctx, find_widget(ctx, 10));
		for(uint32_t i = 0; i < 20; ++i) {
			struct mkgui_rect r = get_rect(ctx, 10 + i);
			CHECK(r.w >= min_w, "id=%u w=%d, minimum should be %d", 10 + i, r.w, min_w);
		}
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_tabs_min_height ]=================================[=]
static void test_tabs_min_height(void) {
	TEST_BEGIN("vbox: tabs widget keeps minimum height under pressure");
	enum { WIN = 1, VBOX1 = 2, BTN1 = 3, BTN2 = 4, BTN3 = 5, TABS1 = 6, TAB1 = 7, BTN4 = 8 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     300, 180, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN1,  "A",    "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN2,  "B",    "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN3,  "C",    "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TABS,   TABS1, "",     "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TAB,    TAB1,  "Tab",  "", TABS1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN4,  "In",   "", TAB1,  0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 8);
	CHECK(ctx, "create failed");
	if(ctx) {
		struct mkgui_rect tabs_r = get_rect(ctx, TABS1);
		int32_t min_h = ctx->tab_height + ctx->row_height * 2;
		CHECK(tabs_r.h >= min_h, "tabs h=%d, minimum should be %d", tabs_r.h, min_h);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_split_pane_min_height ]==========================[=]
static void test_split_pane_min_height(void) {
	TEST_BEGIN("vbox: split widget keeps minimum height under pressure");
	enum { WIN = 1, VBOX1 = 2, BTN1 = 3, BTN2 = 4, BTN3 = 5, SPLIT = 6, TOP = 7, BOT = 8 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     300, 180, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN1,  "A",    "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN2,  "B",    "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN3,  "C",    "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_HSPLIT, SPLIT, "",     "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   TOP,   "",     "", SPLIT, 0,   0,   MKGUI_REGION_TOP, 0, 0),
		MKGUI_W(MKGUI_VBOX,   BOT,   "",     "", SPLIT, 0,   0,   MKGUI_REGION_BOTTOM, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 8);
	CHECK(ctx, "create failed");
	if(ctx) {
		struct mkgui_rect split_r = get_rect(ctx, SPLIT);
		int32_t min_h = ctx->split_min_px * 2 + ctx->split_thick;
		CHECK(split_r.h >= min_h, "split h=%d, minimum should be %d", split_r.h, min_h);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_hbox_label_progress_spinner ]=====================[=]
static void test_hbox_label_progress_spinner(void) {
	TEST_BEGIN("hbox: label(fixed) + progress(flex) + spinner(fixed) all fit");
	enum { WIN = 1, VBOX1 = 2, HBOX1 = 3, LBL = 4, PG = 5, SP = 6 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     600, 400, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_HBOX,     HBOX1, "",     "", VBOX1, 0,   28,  MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_LABEL,    LBL,   "Progress:", "", HBOX1, 80, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_PROGRESS, PG,    "",     "", HBOX1, 0,   0,   0, 0, 1),
		MKGUI_W(MKGUI_SPINNER,  SP,    "",     "", HBOX1, 28,  0,   MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 6);
	CHECK(ctx, "create failed");
	if(ctx) {
		struct mkgui_rect hbox_r = get_rect(ctx, HBOX1);
		struct mkgui_rect lbl_r = get_rect(ctx, LBL);
		struct mkgui_rect pg_r = get_rect(ctx, PG);
		struct mkgui_rect sp_r = get_rect(ctx, SP);
		CHECK(sp_r.w == 28, "spinner w=%d, expected 28", sp_r.w);
		CHECK(sp_r.x + sp_r.w <= hbox_r.x + hbox_r.w, "spinner right=%d overflows hbox right=%d", sp_r.x + sp_r.w, hbox_r.x + hbox_r.w);
		CHECK(lbl_r.x + lbl_r.w <= pg_r.x, "label right=%d overlaps progress x=%d", lbl_r.x + lbl_r.w, pg_r.x);
		CHECK(pg_r.x + pg_r.w <= sp_r.x, "progress right=%d overlaps spinner x=%d", pg_r.x + pg_r.w, sp_r.x);
		CHECK(pg_r.w > 0, "progress w=%d should be positive", pg_r.w);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_vbox_shrink_no_overlap ]==========================[=]
static void test_vbox_shrink_no_overlap(void) {
	TEST_BEGIN("vbox: children shrink proportionally and don't overlap");
	enum { WIN = 1, VBOX1 = 2, BTN1 = 3, LV = 4, BTN2 = 5 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     300, 200, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   BTN1,  "A",    "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LISTVIEW, LV,    "",     "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   BTN2,  "B",    "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 5);
	CHECK(ctx, "create failed");
	if(ctx) {
		check_no_overlap_siblings(ctx, VBOX1, test_name__, &fail__);
		struct mkgui_rect b1 = get_rect(ctx, BTN1);
		struct mkgui_rect lv_r = get_rect(ctx, LV);
		struct mkgui_rect b2 = get_rect(ctx, BTN2);
		CHECK(b1.h > 0, "btn1 h=%d", b1.h);
		CHECK(lv_r.h > 0, "listview h=%d", lv_r.h);
		CHECK(b2.h > 0, "btn2 h=%d", b2.h);
		CHECK(lv_r.y >= b1.y + b1.h, "listview y=%d should be below btn1 bottom=%d", lv_r.y, b1.y + b1.h);
		CHECK(b2.y >= lv_r.y + lv_r.h, "btn2 y=%d should be below listview bottom=%d", b2.y, lv_r.y + lv_r.h);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_hbox_shrink_stays_inside ]========================[=]
static void test_hbox_shrink_stays_inside(void) {
	TEST_BEGIN("hbox: children shrink proportionally and stay inside parent");
	enum { WIN = 1, HBOX1 = 2, LBL = 3, INP = 4, CB = 5 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     200, 200, 0, 0, 0),
		MKGUI_W(MKGUI_HBOX,   HBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LABEL,  LBL,   "Search:", "", HBOX1, 60, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_INPUT,  INP,   "",     "", HBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_COMBOBOX, CB,  "",     "", HBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 5);
	CHECK(ctx, "create failed");
	if(ctx) {
		check_children_inside_parent(ctx, HBOX1, test_name__, &fail__);
		check_no_overlap_siblings(ctx, HBOX1, test_name__, &fail__);
		struct mkgui_rect lbl_r = get_rect(ctx, LBL);
		struct mkgui_rect inp_r = get_rect(ctx, INP);
		struct mkgui_rect cb_r = get_rect(ctx, CB);
		CHECK(inp_r.w > 0, "input w=%d", inp_r.w);
		CHECK(cb_r.w > 0, "combobox w=%d", cb_r.w);
		CHECK(inp_r.x >= lbl_r.x + lbl_r.w, "input x=%d should be after label right=%d", inp_r.x, lbl_r.x + lbl_r.w);
		CHECK(cb_r.x >= inp_r.x + inp_r.w, "combobox x=%d should be after input right=%d", cb_r.x, inp_r.x + inp_r.w);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_form_shrink_no_overlap ]===========================[=]
static void test_form_shrink_no_overlap(void) {
	TEST_BEGIN("form: shrunk window keeps labels and controls from overlapping");
	enum { WIN = 1, FORM1 = 2, LBL1 = 3, INP1 = 4, LBL2 = 5, DD = 6, LBL3 = 7, CB = 8 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test",    "", 0,     200, 150, 0, 0, 0),
		MKGUI_W(MKGUI_FORM,     FORM1, "",        "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LABEL,    LBL1,  "Name",    "", FORM1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_INPUT,    INP1,  "",        "", FORM1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LABEL,    LBL2,  "Enable",  "", FORM1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_DROPDOWN, DD,    "",        "", FORM1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LABEL,    LBL3,  "Search",  "", FORM1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_COMBOBOX, CB,    "",        "", FORM1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 8);
	CHECK(ctx, "create failed");
	if(ctx) {
		struct mkgui_rect l1 = get_rect(ctx, LBL1);
		struct mkgui_rect i1 = get_rect(ctx, INP1);
		CHECK(l1.x + l1.w <= i1.x, "label1 right=%d should not overlap input1 x=%d", l1.x + l1.w, i1.x);
		struct mkgui_rect l3 = get_rect(ctx, LBL3);
		struct mkgui_rect cb_r = get_rect(ctx, CB);
		CHECK(l3.x + l3.w <= cb_r.x, "label3 right=%d should not overlap combobox x=%d", l3.x + l3.w, cb_r.x);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_hbox_fixed_zero_width ]==============================[=]
static void test_hbox_fixed_zero_width(void) {
	TEST_BEGIN("hbox: FIXED widget with w=0 gets natural width, not zero");
	enum { WIN = 1, HBOX1 = 2, BTN1 = 3, BTN2 = 4 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     600, 400, 0, 0, 0),
		MKGUI_W(MKGUI_HBOX,   HBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN1,  "Fix",  "", HBOX1, 0,   0,   MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN2,  "Flex", "", HBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 4);
	CHECK(ctx, "create failed");
	if(ctx) {
		struct mkgui_rect r1 = get_rect(ctx, BTN1);
		int32_t nw = natural_width(ctx, find_widget(ctx, BTN1));
		CHECK(r1.w == nw, "fixed w=0 btn w=%d, expected natural_width %d", r1.w, nw);
		CHECK(r1.w > 0, "fixed w=0 btn must not be zero width");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_generic_fixed_zero_size ]============================[=]
static void test_generic_fixed_zero_size(void) {
	TEST_BEGIN("generic container: FIXED widget with w=0 h=0 gets natural size, not parent fill");
	enum { WIN = 1, BTN1 = 2 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,  "Test", "", 0,   600, 400, 0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN1, "OK",   "", WIN, 0,   0,   MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = create_and_layout(widgets, 2);
	CHECK(ctx, "create failed");
	if(ctx) {
		struct mkgui_rect win_r = get_rect(ctx, WIN);
		struct mkgui_rect btn_r = get_rect(ctx, BTN1);
		struct mkgui_widget *bw = find_widget(ctx, BTN1);
		int32_t nw = natural_width(ctx, bw);
		int32_t nh = natural_height(ctx, MKGUI_BUTTON);
		CHECK(btn_r.w == nw, "fixed btn w=%d, expected natural_width %d (not parent %d)", btn_r.w, nw, win_r.w);
		CHECK(btn_r.h == nh, "fixed btn h=%d, expected natural_height %d (not parent %d)", btn_r.h, nh, win_r.h);
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(void) {
	fprintf(stderr, "mkgui layout test harness\n");
	fprintf(stderr, "========================\n\n");

	test_vbox_basic();
	test_hbox_basic();
	test_weights();
	test_fixed_width();
	test_hidden_no_space();
	test_chrome_no_overlap();
	test_nested_containers();
	test_split_children_positive();
	test_hsplit_children_positive();
	test_many_children_no_zero();
	test_small_window_no_zero();
	test_form_layout();
	test_group_collapsed();
	test_hbox_many_children_no_zero();
	test_mixed_fixed_flex();
	test_tabs_content_inside();
	test_resize_smaller();
	test_split_resize_no_negative();
	test_full_chrome_stack();
	test_panel_border();
	test_no_widget_zero_size();
	test_listview_min_height();
	test_textarea_min_height();
	test_hbox_min_width();
	test_tabs_min_height();
	test_split_pane_min_height();
	test_hbox_label_progress_spinner();
	test_vbox_shrink_no_overlap();
	test_hbox_shrink_stays_inside();
	test_form_shrink_no_overlap();
	test_hbox_fixed_zero_width();
	test_generic_fixed_zero_size();

	fprintf(stderr, "\n========================\n");
	fprintf(stderr, "%u tests: %u passed, %u failed\n", tests_run, tests_passed, tests_failed);
	return tests_failed > 0 ? 1 : 0;
}
