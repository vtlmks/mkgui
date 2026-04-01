// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT
//
// Widget behavior test harness for mkgui.
// Tests style flags, get/set APIs, and widget state logic.

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
// Helpers
// ---------------------------------------------------------------------------

static struct mkgui_ctx *make_ctx(struct mkgui_widget *widgets, uint32_t count) {
	struct mkgui_ctx *ctx = mkgui_create(widgets, count);
	if(!ctx) {
		fprintf(stderr, "FATAL: mkgui_create returned NULL\n");
	}
	return ctx;
}

static uint32_t get_style(struct mkgui_ctx *ctx, uint32_t id) {
	int32_t idx = find_widget_idx(ctx, id);
	if(idx < 0) {
		return 0;
	}
	return ctx->widgets[idx].style;
}

// ---------------------------------------------------------------------------
// Checkbox tests
// ---------------------------------------------------------------------------

// [=]===^=[ test_checkbox_get_set ]================================[=]
static void test_checkbox_get_set(void) {
	TEST_BEGIN("checkbox: get/set checked state");
	enum { WIN = 1, VBOX1 = 2, CB1 = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_CHECKBOX, CB1,   "Test", "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = make_ctx(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		CHECK(mkgui_checkbox_get(ctx, CB1) == 0, "should start unchecked");
		mkgui_checkbox_set(ctx, CB1, 1);
		CHECK(mkgui_checkbox_get(ctx, CB1) == 1, "should be checked after set(1)");
		CHECK(get_style(ctx, CB1) & MKGUI_CHECKBOX_CHECKED, "style bit should be set");
		mkgui_checkbox_set(ctx, CB1, 0);
		CHECK(mkgui_checkbox_get(ctx, CB1) == 0, "should be unchecked after set(0)");
		CHECK(!(get_style(ctx, CB1) & MKGUI_CHECKBOX_CHECKED), "style bit should be clear");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_checkbox_initial_checked ]========================[=]
static void test_checkbox_initial_checked(void) {
	TEST_BEGIN("checkbox: initial checked via style flag");
	enum { WIN = 1, VBOX1 = 2, CB1 = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_CHECKBOX, CB1,   "Test", "", VBOX1, 0,   0,   0, MKGUI_CHECKBOX_CHECKED, 0),
	};
	struct mkgui_ctx *ctx = make_ctx(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		CHECK(mkgui_checkbox_get(ctx, CB1) == 1, "should start checked");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Radio tests
// ---------------------------------------------------------------------------

// [=]===^=[ test_radio_mutual_exclusion ]=========================[=]
static void test_radio_mutual_exclusion(void) {
	TEST_BEGIN("radio: setting one unchecks siblings");
	enum { WIN = 1, VBOX1 = 2, R1 = 3, R2 = 4, R3 = 5 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_RADIO,  R1,    "A",    "", VBOX1, 0,   0,   0, MKGUI_RADIO_CHECKED, 0),
		MKGUI_W(MKGUI_RADIO,  R2,    "B",    "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_RADIO,  R3,    "C",    "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = make_ctx(widgets, 5);
	CHECK(ctx, "create failed");
	if(ctx) {
		CHECK(mkgui_radio_get(ctx, R1) == 1, "R1 should start checked");
		CHECK(mkgui_radio_get(ctx, R2) == 0, "R2 should start unchecked");
		CHECK(mkgui_radio_get(ctx, R3) == 0, "R3 should start unchecked");
		mkgui_radio_set(ctx, R2, 1);
		CHECK(mkgui_radio_get(ctx, R1) == 0, "R1 should be unchecked after selecting R2");
		CHECK(mkgui_radio_get(ctx, R2) == 1, "R2 should be checked");
		CHECK(mkgui_radio_get(ctx, R3) == 0, "R3 should remain unchecked");
		mkgui_radio_set(ctx, R3, 1);
		CHECK(mkgui_radio_get(ctx, R1) == 0, "R1 unchecked");
		CHECK(mkgui_radio_get(ctx, R2) == 0, "R2 unchecked after selecting R3");
		CHECK(mkgui_radio_get(ctx, R3) == 1, "R3 checked");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Toggle tests
// ---------------------------------------------------------------------------

// [=]===^=[ test_toggle_get_set ]=================================[=]
static void test_toggle_get_set(void) {
	TEST_BEGIN("toggle: get/set state");
	enum { WIN = 1, VBOX1 = 2, T1 = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TOGGLE,  T1,   "Test", "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = make_ctx(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		CHECK(mkgui_toggle_get(ctx, T1) == 0, "should start off");
		mkgui_toggle_set(ctx, T1, 1);
		CHECK(mkgui_toggle_get(ctx, T1) == 1, "should be on after set(1)");
		mkgui_toggle_set(ctx, T1, 0);
		CHECK(mkgui_toggle_get(ctx, T1) == 0, "should be off after set(0)");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Input tests
// ---------------------------------------------------------------------------

// [=]===^=[ test_input_get_set ]=================================[=]
static void test_input_get_set(void) {
	TEST_BEGIN("input: get/set text");
	enum { WIN = 1, VBOX1 = 2, INP = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_INPUT,  INP,   "",     "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = make_ctx(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		mkgui_input_set(ctx, INP, "hello");
		char *txt = mkgui_input_get(ctx, INP);
		CHECK(txt && strcmp(txt, "hello") == 0, "text should be 'hello', got '%s'", txt ? txt : "(null)");
		mkgui_input_set(ctx, INP, "");
		txt = mkgui_input_get(ctx, INP);
		CHECK(txt && strcmp(txt, "") == 0, "text should be empty");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_input_readonly_flag ]============================[=]
static void test_input_readonly_flag(void) {
	TEST_BEGIN("input: readonly flag via style");
	enum { WIN = 1, VBOX1 = 2, INP = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_INPUT,  INP,   "",     "", VBOX1, 0,   0,   0, MKGUI_INPUT_READONLY, 0),
	};
	struct mkgui_ctx *ctx = make_ctx(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		CHECK(mkgui_input_get_readonly(ctx, INP) == 1, "should be readonly from style");
		mkgui_input_set_readonly(ctx, INP, 0);
		CHECK(mkgui_input_get_readonly(ctx, INP) == 0, "should be writable after set_readonly(0)");
		mkgui_input_set_readonly(ctx, INP, 1);
		CHECK(mkgui_input_get_readonly(ctx, INP) == 1, "should be readonly after set_readonly(1)");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_input_password_flag ]============================[=]
static void test_input_password_flag(void) {
	TEST_BEGIN("input: password style flag bit");
	enum { WIN = 1, VBOX1 = 2, INP = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_INPUT,  INP,   "",     "", VBOX1, 0,   0,   0, MKGUI_INPUT_PASSWORD, 0),
	};
	struct mkgui_ctx *ctx = make_ctx(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		CHECK(get_style(ctx, INP) & MKGUI_INPUT_PASSWORD, "password bit should be set");
		CHECK(!(get_style(ctx, INP) & MKGUI_INPUT_READONLY), "readonly bit should not be set");
		CHECK(!(get_style(ctx, INP) & MKGUI_INPUT_NUMERIC), "numeric bit should not be set");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_input_numeric_flag ]=============================[=]
static void test_input_numeric_flag(void) {
	TEST_BEGIN("input: numeric style flag bit");
	enum { WIN = 1, VBOX1 = 2, INP = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_INPUT,  INP,   "",     "", VBOX1, 0,   0,   0, MKGUI_INPUT_NUMERIC, 0),
	};
	struct mkgui_ctx *ctx = make_ctx(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		CHECK(get_style(ctx, INP) & MKGUI_INPUT_NUMERIC, "numeric bit should be set");
		CHECK(!(get_style(ctx, INP) & MKGUI_INPUT_PASSWORD), "password bit should not be set");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Textarea tests
// ---------------------------------------------------------------------------

// [=]===^=[ test_textarea_get_set ]===============================[=]
static void test_textarea_get_set(void) {
	TEST_BEGIN("textarea: get/set text");
	enum { WIN = 1, VBOX1 = 2, TA = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TEXTAREA, TA,    "",     "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = make_ctx(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		mkgui_textarea_set(ctx, TA, "line one\nline two");
		char *txt = mkgui_textarea_get(ctx, TA);
		CHECK(txt && strcmp(txt, "line one\nline two") == 0, "text mismatch");
		CHECK(mkgui_textarea_get_line_count(ctx, TA) == 2, "should have 2 lines");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_textarea_readonly_flag ]=========================[=]
static void test_textarea_readonly_flag(void) {
	TEST_BEGIN("textarea: readonly flag uses correct bit");
	enum { WIN = 1, VBOX1 = 2, TA = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TEXTAREA, TA,    "",     "", VBOX1, 0,   0,   0, MKGUI_TEXTAREA_READONLY, 0),
	};
	struct mkgui_ctx *ctx = make_ctx(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		CHECK(mkgui_textarea_get_readonly(ctx, TA) == 1, "should be readonly from style");
		CHECK(get_style(ctx, TA) & MKGUI_TEXTAREA_READONLY, "readonly bit should be set");
		mkgui_textarea_set_readonly(ctx, TA, 0);
		CHECK(mkgui_textarea_get_readonly(ctx, TA) == 0, "should be writable");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Label tests
// ---------------------------------------------------------------------------

// [=]===^=[ test_label_get_set ]=================================[=]
static void test_label_get_set(void) {
	TEST_BEGIN("label: get/set text");
	enum { WIN = 1, VBOX1 = 2, LBL = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LABEL,  LBL,   "init", "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = make_ctx(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		char *txt = mkgui_label_get(ctx, LBL);
		CHECK(txt && strcmp(txt, "init") == 0, "initial text should be 'init'");
		mkgui_label_set(ctx, LBL, "changed");
		txt = mkgui_label_get(ctx, LBL);
		CHECK(txt && strcmp(txt, "changed") == 0, "text should be 'changed'");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_label_style_flags ]=============================[=]
static void test_label_style_flags(void) {
	TEST_BEGIN("label: style flags are distinct bits");
	enum { WIN = 1, VBOX1 = 2, L1 = 3, L2 = 4, L3 = 5 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LABEL,  L1,    "trunc","", VBOX1, 0,   0,   0, MKGUI_LABEL_TRUNCATE, 0),
		MKGUI_W(MKGUI_LABEL,  L2,    "link", "", VBOX1, 0,   0,   0, MKGUI_LABEL_LINK, 0),
		MKGUI_W(MKGUI_LABEL,  L3,    "wrap", "", VBOX1, 0,   0,   0, MKGUI_LABEL_WRAP, 0),
	};
	struct mkgui_ctx *ctx = make_ctx(widgets, 5);
	CHECK(ctx, "create failed");
	if(ctx) {
		CHECK((get_style(ctx, L1) & MKGUI_LABEL_TRUNCATE) && !(get_style(ctx, L1) & MKGUI_LABEL_LINK), "L1: truncate only");
		CHECK((get_style(ctx, L2) & MKGUI_LABEL_LINK) && !(get_style(ctx, L2) & MKGUI_LABEL_TRUNCATE), "L2: link only");
		CHECK((get_style(ctx, L3) & MKGUI_LABEL_WRAP) && !(get_style(ctx, L3) & MKGUI_LABEL_LINK), "L3: wrap only");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Group tests
// ---------------------------------------------------------------------------

// [=]===^=[ test_group_collapse ]=================================[=]
static void test_group_collapse(void) {
	TEST_BEGIN("group: collapse/expand via API");
	enum { WIN = 1, VBOX1 = 2, GRP = 3, BTN = 4 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test",  "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",      "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_GROUP,  GRP,   "Group", "", VBOX1, 0,   0,   0, MKGUI_GROUP_COLLAPSIBLE, 0),
		MKGUI_W(MKGUI_BUTTON, BTN,   "Child", "", GRP,   0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = make_ctx(widgets, 4);
	CHECK(ctx, "create failed");
	if(ctx) {
		CHECK(mkgui_group_get_collapsed(ctx, GRP) == 0, "should start expanded");
		mkgui_group_set_collapsed(ctx, GRP, 1);
		CHECK(mkgui_group_get_collapsed(ctx, GRP) == 1, "should be collapsed");
		CHECK(get_style(ctx, GRP) & MKGUI_GROUP_COLLAPSED, "collapsed bit set");
		mkgui_group_set_collapsed(ctx, GRP, 0);
		CHECK(mkgui_group_get_collapsed(ctx, GRP) == 0, "should be expanded");
		CHECK(!(get_style(ctx, GRP) & MKGUI_GROUP_COLLAPSED), "collapsed bit clear");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Slider tests
// ---------------------------------------------------------------------------

// [=]===^=[ test_slider_range ]====================================[=]
static void test_slider_range(void) {
	TEST_BEGIN("slider: get/set value clamped to range");
	enum { WIN = 1, VBOX1 = 2, SL = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_SLIDER,  SL,   "",     "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = make_ctx(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		mkgui_slider_setup(ctx, SL, 0, 100, 50);
		CHECK(mkgui_slider_get(ctx, SL) == 50, "initial value should be 50");
		mkgui_slider_set(ctx, SL, 200);
		CHECK(mkgui_slider_get(ctx, SL) <= 100, "value should be clamped to max");
		mkgui_slider_set(ctx, SL, -10);
		CHECK(mkgui_slider_get(ctx, SL) >= 0, "value should be clamped to min");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Progress tests
// ---------------------------------------------------------------------------

// [=]===^=[ test_progress_get_set ]================================[=]
static void test_progress_get_set(void) {
	TEST_BEGIN("progress: get/set value clamped to range");
	enum { WIN = 1, VBOX1 = 2, PB = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_PROGRESS, PB,    "",     "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = make_ctx(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		mkgui_progress_setup(ctx, PB, 100);
		mkgui_progress_set(ctx, PB, 65);
		CHECK(mkgui_progress_get(ctx, PB) == 65, "value should be 65");
		mkgui_progress_set(ctx, PB, 200);
		CHECK(mkgui_progress_get(ctx, PB) == 100, "value should be clamped to 100");
		mkgui_progress_set(ctx, PB, -5);
		CHECK(mkgui_progress_get(ctx, PB) == 0, "value should be clamped to 0");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_progress_shimmer_flag ]============================[=]
static void test_progress_shimmer_flag(void) {
	TEST_BEGIN("progress: shimmer style flag bit");
	enum { WIN = 1, VBOX1 = 2, PB = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_PROGRESS, PB,    "",     "", VBOX1, 0,   0,   0, MKGUI_PROGRESS_SHIMMER, 0),
	};
	struct mkgui_ctx *ctx = make_ctx(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		CHECK(get_style(ctx, PB) & MKGUI_PROGRESS_SHIMMER, "shimmer bit should be set");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Spinbox tests
// ---------------------------------------------------------------------------

// [=]===^=[ test_spinbox_range ]====================================[=]
static void test_spinbox_range(void) {
	TEST_BEGIN("spinbox: get/set value clamped to range");
	enum { WIN = 1, VBOX1 = 2, SP = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,  WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,    VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_SPINBOX, SP,    "",     "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = make_ctx(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		mkgui_spinbox_setup(ctx, SP, -10, 10, 0, 1);
		CHECK(mkgui_spinbox_get(ctx, SP) == 0, "initial value should be 0");
		mkgui_spinbox_set(ctx, SP, 5);
		CHECK(mkgui_spinbox_get(ctx, SP) == 5, "value should be 5");
		mkgui_spinbox_set(ctx, SP, 99);
		CHECK(mkgui_spinbox_get(ctx, SP) == 10, "value should be clamped to 10");
		mkgui_spinbox_set(ctx, SP, -99);
		CHECK(mkgui_spinbox_get(ctx, SP) == -10, "value should be clamped to -10");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Dropdown tests
// ---------------------------------------------------------------------------

// [=]===^=[ test_dropdown_get_set ]=================================[=]
static void test_dropdown_get_set(void) {
	TEST_BEGIN("dropdown: setup and get/set index");
	enum { WIN = 1, VBOX1 = 2, DD = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_DROPDOWN, DD,    "",     "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = make_ctx(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		char *items[] = { "Alpha", "Beta", "Gamma" };
		mkgui_dropdown_setup(ctx, DD, items, 3);
		CHECK(mkgui_dropdown_get_count(ctx, DD) == 3, "should have 3 items");
		mkgui_dropdown_set(ctx, DD, 1);
		CHECK(mkgui_dropdown_get(ctx, DD) == 1, "selected index should be 1");
		char *txt = mkgui_dropdown_get_text(ctx, DD);
		CHECK(txt && strcmp(txt, "Beta") == 0, "selected text should be 'Beta'");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Visible / enabled tests
// ---------------------------------------------------------------------------

// [=]===^=[ test_visible_enabled ]=================================[=]
static void test_visible_enabled(void) {
	TEST_BEGIN("widget: visible/enabled toggle");
	enum { WIN = 1, VBOX1 = 2, BTN = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN,   "Btn",  "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = make_ctx(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		CHECK(mkgui_get_visible(ctx, BTN) == 1, "should start visible");
		CHECK(mkgui_get_enabled(ctx, BTN) == 1, "should start enabled");
		mkgui_set_visible(ctx, BTN, 0);
		CHECK(mkgui_get_visible(ctx, BTN) == 0, "should be hidden");
		mkgui_set_visible(ctx, BTN, 1);
		CHECK(mkgui_get_visible(ctx, BTN) == 1, "should be visible again");
		mkgui_set_enabled(ctx, BTN, 0);
		CHECK(mkgui_get_enabled(ctx, BTN) == 0, "should be disabled");
		mkgui_set_enabled(ctx, BTN, 1);
		CHECK(mkgui_get_enabled(ctx, BTN) == 1, "should be enabled again");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Border style flag tests
// ---------------------------------------------------------------------------

// [=]===^=[ test_border_flags_per_widget ]=========================[=]
static void test_border_flags_per_widget(void) {
	TEST_BEGIN("border: per-widget-type border flags at bit 0");
	enum { WIN = 1, VBOX1 = 2, P1 = 3, V1 = 4, C1 = 5, I1 = 6 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_PANEL,  P1,    "",     "", VBOX1, 0,   0,   MKGUI_FIXED, MKGUI_PANEL_BORDER, 0),
		MKGUI_W(MKGUI_VBOX,   V1,    "",     "", VBOX1, 0,   0,   MKGUI_FIXED, MKGUI_VBOX_BORDER, 0),
		MKGUI_W(MKGUI_CANVAS, C1,    "",     "", VBOX1, 0,   0,   MKGUI_FIXED, MKGUI_CANVAS_BORDER, 0),
		MKGUI_W(MKGUI_IMAGE,  I1,    "",     "", VBOX1, 0,   0,   MKGUI_FIXED, MKGUI_IMAGE_BORDER, 0),
	};
	struct mkgui_ctx *ctx = make_ctx(widgets, 6);
	CHECK(ctx, "create failed");
	if(ctx) {
		CHECK(get_style(ctx, P1) == MKGUI_PANEL_BORDER, "panel border = bit 0");
		CHECK(get_style(ctx, V1) == MKGUI_VBOX_BORDER, "vbox border = bit 0");
		CHECK(get_style(ctx, C1) == MKGUI_CANVAS_BORDER, "canvas border = bit 0");
		CHECK(get_style(ctx, I1) == MKGUI_IMAGE_BORDER, "image border = bit 0");
		CHECK(MKGUI_PANEL_BORDER == MKGUI_VBOX_BORDER, "all border flags are same bit");
		CHECK(MKGUI_VBOX_BORDER == MKGUI_CANVAS_BORDER, "all border flags are same bit");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Menu item style flag tests
// ---------------------------------------------------------------------------

// [=]===^=[ test_menuitem_flags_distinct ]=========================[=]
static void test_menuitem_flags_distinct(void) {
	TEST_BEGIN("menuitem: separator/check/radio/checked are distinct bits");
	CHECK(MKGUI_MENUITEM_SEPARATOR != MKGUI_MENUITEM_CHECK, "separator != check");
	CHECK(MKGUI_MENUITEM_CHECK != MKGUI_MENUITEM_RADIO, "check != radio");
	CHECK(MKGUI_MENUITEM_RADIO != MKGUI_MENUITEM_CHECKED, "radio != checked");
	CHECK(MKGUI_MENUITEM_SEPARATOR != MKGUI_MENUITEM_CHECKED, "separator != checked");
	CHECK((MKGUI_MENUITEM_SEPARATOR & MKGUI_MENUITEM_CHECK) == 0, "no overlap sep/check");
	CHECK((MKGUI_MENUITEM_CHECK & MKGUI_MENUITEM_RADIO) == 0, "no overlap check/radio");
	CHECK((MKGUI_MENUITEM_RADIO & MKGUI_MENUITEM_CHECKED) == 0, "no overlap radio/checked");
	TEST_END();
}

// ---------------------------------------------------------------------------
// Input flag orthogonality
// ---------------------------------------------------------------------------

// [=]===^=[ test_input_flags_distinct ]============================[=]
static void test_input_flags_distinct(void) {
	TEST_BEGIN("input: password/readonly/numeric are distinct bits");
	CHECK((MKGUI_INPUT_PASSWORD & MKGUI_INPUT_READONLY) == 0, "password != readonly");
	CHECK((MKGUI_INPUT_READONLY & MKGUI_INPUT_NUMERIC) == 0, "readonly != numeric");
	CHECK((MKGUI_INPUT_PASSWORD & MKGUI_INPUT_NUMERIC) == 0, "password != numeric");
	TEST_END();
}

// ---------------------------------------------------------------------------
// Meter tests
// ---------------------------------------------------------------------------

// [=]===^=[ test_meter_get_set ]===================================[=]
static void test_meter_get_set(void) {
	TEST_BEGIN("meter: get/set value");
	enum { WIN = 1, VBOX1 = 2, MT = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_METER,  MT,    "",     "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = make_ctx(widgets, 3);
	CHECK(ctx, "create failed");
	if(ctx) {
		mkgui_meter_setup(ctx, MT, 100);
		mkgui_meter_set(ctx, MT, 42);
		CHECK(mkgui_meter_get(ctx, MT) == 42, "value should be 42");
		mkgui_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(void) {
	printf("mkgui widget behavior test harness\n");
	printf("===================================\n\n");

	test_checkbox_get_set();
	test_checkbox_initial_checked();
	test_radio_mutual_exclusion();
	test_toggle_get_set();
	test_input_get_set();
	test_input_readonly_flag();
	test_input_password_flag();
	test_input_numeric_flag();
	test_textarea_get_set();
	test_textarea_readonly_flag();
	test_label_get_set();
	test_label_style_flags();
	test_group_collapse();
	test_slider_range();
	test_progress_get_set();
	test_progress_shimmer_flag();
	test_spinbox_range();
	test_dropdown_get_set();
	test_visible_enabled();
	test_border_flags_per_widget();
	test_menuitem_flags_distinct();
	test_input_flags_distinct();
	test_meter_get_set();

	printf("\n===================================\n");
	printf("%u tests: %u passed, %u failed\n", tests_run, tests_passed, tests_failed);
	return tests_failed ? 1 : 0;
}
