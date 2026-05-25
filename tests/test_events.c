// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT
//
// Widget event test harness for mkgui.
// Tests that all widget types fire the events they are supposed to fire.
// Uses platform deferred event queue to inject synthetic input.

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
// Event injection helpers
// ---------------------------------------------------------------------------

static void flush_x_events(struct mkgui_window *win) {
	XSync(win->plat.dpy, True);
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
	struct mkgui_plat_event pev;
	memset(&pev, 0, sizeof(pev));
	pev.type = MKGUI_PLAT_BUTTON_PRESS;
	pev.x = x;
	pev.y = y;
	pev.button = button;
	pev.keymod = keymod;
	pev.popup_idx = -1;
	platform_deferred_push(win, &pev);
}

static void inject_release(struct mkgui_window *win, int32_t x, int32_t y, uint32_t button) {
	struct mkgui_plat_event pev;
	memset(&pev, 0, sizeof(pev));
	pev.type = MKGUI_PLAT_BUTTON_RELEASE;
	pev.x = x;
	pev.y = y;
	pev.button = button;
	pev.popup_idx = -1;
	platform_deferred_push(win, &pev);
}

static void inject_key(struct mkgui_window *win, uint32_t keysym, uint32_t keymod) {
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
	platform_deferred_push(win, &pev);
}

static void inject_motion(struct mkgui_window *win, int32_t x, int32_t y) {
	struct mkgui_plat_event pev;
	memset(&pev, 0, sizeof(pev));
	pev.type = MKGUI_PLAT_MOTION;
	pev.x = x;
	pev.y = y;
	pev.popup_idx = -1;
	platform_deferred_push(win, &pev);
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

static void inject_right_click(struct mkgui_window *win, uint32_t id) {
	int32_t cx, cy;
	widget_center(win, id, &cx, &cy);
	inject_motion(win, cx, cy);
	inject_press(win, cx, cy, 3, 0);
	inject_release(win, cx, cy, 3);
}

static void inject_dblclick(struct mkgui_window *win, uint32_t id) {
	int32_t cx, cy;
	widget_center(win, id, &cx, &cy);
	inject_motion(win, cx, cy);
	inject_press(win, cx, cy, 1, 0);
	inject_release(win, cx, cy, 1);
	inject_press(win, cx, cy, 1, 0);
	inject_release(win, cx, cy, 1);
}

// Pump all deferred events through mkgui_window_poll, return 1 if target event found
static uint32_t poll_for_event(struct mkgui_window *win, uint32_t event_type, uint32_t target_id) {
	flush_x_events(win);
	uint32_t found = 0;
	struct mkgui_event ev;
	// Call mkgui_window_poll repeatedly: it returns 1 if a user event was produced,
	// 0 if no events remain. Internal state (hover, focus) produces events
	// on subsequent calls even after platform events are drained.
	for(uint32_t i = 0; i < 50; ++i) {
		uint32_t got = mkgui_window_poll(win, &ev);
		if(got && ev.type == event_type && (target_id == 0 || ev.id == target_id)) {
			found = 1;
		}

		if(!got && win->plat.deferred_head == win->plat.deferred_tail) {
			// One more poll to catch hover/focus state changes
			mkgui_window_poll(win, &ev);
			if(ev.type == event_type && (target_id == 0 || ev.id == target_id)) {
				found = 1;
			}
			break;
		}
	}
	return found;
}

// Drain all events, return the event type and id of the first non-hover/focus event
static struct mkgui_event poll_first_widget_event(struct mkgui_window *win) {
	struct mkgui_event result;
	memset(&result, 0, sizeof(result));
	struct mkgui_event ev;
	for(uint32_t safety = 0; safety < 200; ++safety) {
		if(!mkgui_window_poll(win, &ev)) {
			break;
		}

		if(ev.type == MKGUI_EVENT_HOVER_ENTER || ev.type == MKGUI_EVENT_HOVER_LEAVE || ev.type == MKGUI_EVENT_FOCUS || ev.type == MKGUI_EVENT_UNFOCUS || ev.type == MKGUI_EVENT_RESIZE) {
			continue;
		}

		if(result.type == MKGUI_EVENT_NONE) {
			result = ev;
		}
	}
	return result;
}

// ---------------------------------------------------------------------------
// Button
// ---------------------------------------------------------------------------

// [=]===^=[ test_button_click ]====================================[=]
static void test_button_click(void) {
	TEST_BEGIN("button: click fires MKGUI_EVENT_CLICK");
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
		inject_click(win, BTN);
		CHECK(poll_for_event(win, MKGUI_EVENT_CLICK, BTN), "expected CLICK event");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Checkbox
// ---------------------------------------------------------------------------

// [=]===^=[ test_checkbox_click_event ]=============================[=]
static void test_checkbox_click_event(void) {
	TEST_BEGIN("checkbox: click fires MKGUI_EVENT_CHECKBOX_CHANGED");
	enum { WIN = 1, VBOX1 = 2, CB = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_CHECKBOX, CB,    "Test", "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		inject_click(win, CB);
		CHECK(poll_for_event(win, MKGUI_EVENT_CHECKBOX_CHANGED, CB), "expected CHECKBOX_CHANGED event");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Radio
// ---------------------------------------------------------------------------

// [=]===^=[ test_radio_click_event ]================================[=]
static void test_radio_click_event(void) {
	TEST_BEGIN("radio: click fires MKGUI_EVENT_RADIO_CHANGED");
	enum { WIN = 1, VBOX1 = 2, R1 = 3, R2 = 4 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_RADIO,  R1,    "A",    "", VBOX1, 0,   0,   0, MKGUI_RADIO_CHECKED, 0),
		MKGUI_W(MKGUI_RADIO,  R2,    "B",    "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 4, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		inject_click(win, R2);
		CHECK(poll_for_event(win, MKGUI_EVENT_RADIO_CHANGED, R2), "expected RADIO_CHANGED event");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Toggle
// ---------------------------------------------------------------------------

// [=]===^=[ test_toggle_click_event ]===============================[=]
static void test_toggle_click_event(void) {
	TEST_BEGIN("toggle: click fires MKGUI_EVENT_TOGGLE_CHANGED");
	enum { WIN = 1, VBOX1 = 2, TGL = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TOGGLE, TGL,   "Test", "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		inject_click(win, TGL);
		CHECK(poll_for_event(win, MKGUI_EVENT_TOGGLE_CHANGED, TGL), "expected TOGGLE_CHANGED event");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

// [=]===^=[ test_input_type_event ]=================================[=]
static void test_input_type_event(void) {
	TEST_BEGIN("input: typing fires MKGUI_EVENT_INPUT_CHANGED");
	enum { WIN = 1, VBOX1 = 2, INP = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_INPUT,  INP,   "",     "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		inject_click(win, INP);
		poll_for_event(win, MKGUI_EVENT_NONE, 0);
		inject_key(win, 'a', 0);
		CHECK(poll_for_event(win, MKGUI_EVENT_INPUT_CHANGED, INP), "expected INPUT_CHANGED event");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_input_submit_event ]===============================[=]
static void test_input_submit_event(void) {
	TEST_BEGIN("input: Enter fires MKGUI_EVENT_INPUT_SUBMIT");
	enum { WIN = 1, VBOX1 = 2, INP = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_INPUT,  INP,   "",     "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		inject_click(win, INP);
		poll_for_event(win, MKGUI_EVENT_NONE, 0);
		inject_key(win, MKGUI_KEY_RETURN, 0);
		CHECK(poll_for_event(win, MKGUI_EVENT_INPUT_SUBMIT, INP), "expected INPUT_SUBMIT event");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Textarea
// ---------------------------------------------------------------------------

// [=]===^=[ test_textarea_type_event ]==============================[=]
static void test_textarea_type_event(void) {
	TEST_BEGIN("textarea: typing fires MKGUI_EVENT_TEXTAREA_CHANGED");
	enum { WIN = 1, VBOX1 = 2, TA = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TEXTAREA, TA,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		inject_click(win, TA);
		poll_for_event(win, MKGUI_EVENT_NONE, 0);
		inject_key(win, 'x', 0);
		CHECK(poll_for_event(win, MKGUI_EVENT_TEXTAREA_CHANGED, TA), "expected TEXTAREA_CHANGED event");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_textarea_cursor_event ]============================[=]
static void test_textarea_cursor_event(void) {
	TEST_BEGIN("textarea: arrow key fires MKGUI_EVENT_TEXTAREA_CURSOR");
	enum { WIN = 1, VBOX1 = 2, TA = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TEXTAREA, TA,    "",     "", VBOX1, 0,   200, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		inject_click(win, TA);
		poll_for_event(win, MKGUI_EVENT_NONE, 0);
		inject_key(win, 'a', 0);
		poll_for_event(win, MKGUI_EVENT_NONE, 0);
		inject_key(win, MKGUI_KEY_LEFT, 0);
		CHECK(poll_for_event(win, MKGUI_EVENT_TEXTAREA_CURSOR, TA), "expected TEXTAREA_CURSOR event");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Slider
// ---------------------------------------------------------------------------

// [=]===^=[ test_slider_key_event ]=================================[=]
static void test_slider_key_event(void) {
	TEST_BEGIN("slider: arrow key fires MKGUI_EVENT_SLIDER_CHANGED");
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
		inject_click(win, SL);
		poll_for_event(win, MKGUI_EVENT_NONE, 0);
		inject_key(win, MKGUI_KEY_RIGHT, 0);
		CHECK(poll_for_event(win, MKGUI_EVENT_SLIDER_CHANGED, SL), "expected SLIDER_CHANGED event");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Spinbox
// ---------------------------------------------------------------------------

// [=]===^=[ test_spinbox_click_event ]==============================[=]
static void test_spinbox_click_event(void) {
	TEST_BEGIN("spinbox: click + button fires MKGUI_EVENT_SPINBOX_CHANGED");
	enum { WIN = 1, VBOX1 = 2, SPN = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,  WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,    VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_SPINBOX, SPN,   "",     "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		mkgui_spinbox_setup(win, SPN, 0, 100, 50, 1);
		int32_t idx = find_widget_idx(win, SPN);
		int32_t rx = win->rects[idx].x + win->rects[idx].w - 5;
		int32_t ry = win->rects[idx].y + 3;
		inject_motion(win, rx, ry);
		inject_press(win, rx, ry, 1, 0);
		inject_release(win, rx, ry, 1);
		CHECK(poll_for_event(win, MKGUI_EVENT_SPINBOX_CHANGED, SPN), "expected SPINBOX_CHANGED event");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Dropdown
// ---------------------------------------------------------------------------

// [=]===^=[ test_dropdown_select_event ]=============================[=]
static void test_dropdown_select_event(void) {
	TEST_BEGIN("dropdown: keyboard selection fires MKGUI_EVENT_DROPDOWN_CHANGED");
	enum { WIN = 1, VBOX1 = 2, DRP = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_DROPDOWN, DRP,   "",     "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		const char *items[] = { "One", "Two", "Three" };
		mkgui_dropdown_setup(win, DRP, items, 3);
		win->focus_id = DRP;
		inject_key(win, MKGUI_KEY_DOWN, 0);
		CHECK(poll_for_event(win, MKGUI_EVENT_DROPDOWN_CHANGED, DRP), "expected DROPDOWN_CHANGED event");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Tabs
// ---------------------------------------------------------------------------

// [=]===^=[ test_tabs_click_event ]=================================[=]
static void test_tabs_click_event(void) {
	TEST_BEGIN("tabs: keyboard switches tab fires MKGUI_EVENT_TAB_CHANGED");
	enum { WIN = 1, TABS1 = 2, TAB_A = 3, TAB_B = 4, LBL_A = 5, LBL_B = 6 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,    "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_TABS,   TABS1,  "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TAB,    TAB_A,  "Tab A","", TABS1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_TAB,    TAB_B,  "Tab B","", TABS1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LABEL,  LBL_A,  "A",    "", TAB_A, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LABEL,  LBL_B,  "B",    "", TAB_B, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 6, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		win->focus_id = TABS1;
		inject_key(win, MKGUI_KEY_RIGHT, 0);
		CHECK(poll_for_event(win, MKGUI_EVENT_TAB_CHANGED, TABS1), "expected TAB_CHANGED event");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Scrollbar
// ---------------------------------------------------------------------------

// [=]===^=[ test_scrollbar_key_event ]==============================[=]
static void test_scrollbar_wheel_event(void) {
	TEST_BEGIN("scrollbar: mouse wheel fires MKGUI_EVENT_SCROLL");
	enum { WIN = 1, VBOX1 = 2, SB = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,    WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,      VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_SCROLLBAR, SB,    "",     "", VBOX1, 0,   0, MKGUI_VERTICAL, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		mkgui_scrollbar_setup(win, SB, 100, 10);
		int32_t cx, cy;
		widget_center(win, SB, &cx, &cy);
		inject_motion(win, cx, cy);
		inject_press(win, cx, cy, 5, 0);
		CHECK(poll_for_event(win, MKGUI_EVENT_SCROLL, SB), "expected SCROLL event");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Context menu (right-click)
// ---------------------------------------------------------------------------

// [=]===^=[ test_button_context_event ]=============================[=]
static void test_button_context_event(void) {
	TEST_BEGIN("button: right-click fires MKGUI_EVENT_CONTEXT");
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
		inject_right_click(win, BTN);
		CHECK(poll_for_event(win, MKGUI_EVENT_CONTEXT, BTN), "expected CONTEXT event");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Hover enter/leave
// ---------------------------------------------------------------------------

// [=]===^=[ test_hover_events ]=====================================[=]
static void test_hover_events(void) {
	TEST_BEGIN("button: hover fires HOVER_ENTER and HOVER_LEAVE");
	enum { WIN = 1, VBOX1 = 2, BTN = 3, LBL = 4 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN,   "OK",   "", VBOX1, 100, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_LABEL,  LBL,   "Test", "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 4, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		int32_t cx, cy;
		widget_center(win, BTN, &cx, &cy);
		inject_motion(win, cx, cy);
		CHECK(poll_for_event(win, MKGUI_EVENT_HOVER_ENTER, BTN), "expected HOVER_ENTER");
		int32_t lx, ly;
		widget_center(win, LBL, &lx, &ly);
		inject_motion(win, lx, ly);
		CHECK(poll_for_event(win, MKGUI_EVENT_HOVER_LEAVE, BTN), "expected HOVER_LEAVE");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Focus / Unfocus
// ---------------------------------------------------------------------------

// [=]===^=[ test_focus_events ]=====================================[=]
static void test_focus_events(void) {
	TEST_BEGIN("input: click fires FOCUS, click elsewhere fires UNFOCUS");
	enum { WIN = 1, VBOX1 = 2, INP = 3, BTN = 4 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_INPUT,  INP,   "",     "", VBOX1, 0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN,   "OK",   "", VBOX1, 100, 0, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 4, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		inject_click(win, INP);
		CHECK(poll_for_event(win, MKGUI_EVENT_FOCUS, INP), "expected FOCUS on input");
		inject_click(win, BTN);
		CHECK(poll_for_event(win, MKGUI_EVENT_UNFOCUS, INP), "expected UNFOCUS on input");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// KEY event (passthrough)
// ---------------------------------------------------------------------------

// [=]===^=[ test_key_event ]========================================[=]
static void test_key_event(void) {
	TEST_BEGIN("window: unhandled key fires MKGUI_EVENT_KEY");
	enum { WIN = 1, VBOX1 = 2, LBL = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_LABEL,  LBL,   "Hi",   "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		inject_key(win, MKGUI_KEY_F1 + 4, 0);
		CHECK(poll_for_event(win, MKGUI_EVENT_KEY, 0), "expected KEY event for F5");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Accelerator
// ---------------------------------------------------------------------------

// [=]===^=[ test_accel_event ]======================================[=]
static void test_accel_event(void) {
	TEST_BEGIN("accelerator: Ctrl+S fires MKGUI_EVENT_ACCEL");
	enum { WIN = 1, VBOX1 = 2, BTN = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN,   "Save", "", VBOX1, 100, 0, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		mkgui_accel_add(win, BTN, MKGUI_MOD_CONTROL, 's');
		inject_key(win, 's', MKGUI_MOD_CONTROL);
		CHECK(poll_for_event(win, MKGUI_EVENT_ACCEL, BTN), "expected ACCEL event for Ctrl+S");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_accel_menu_event ]=================================[=]
static void test_accel_menu_event(void) {
	TEST_BEGIN("accelerator: Ctrl+S on menuitem fires MKGUI_EVENT_MENU");
	enum { WIN = 1, MENU1 = 2, MI_FILE = 3, MI_SAVE = 4 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   WIN,     "Test", "", 0,       400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_MENU,     MENU1,   "",     "", WIN,     0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_MENUITEM, MI_FILE, "File", "", MENU1,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_MENUITEM, MI_SAVE, "Save", "", MI_FILE, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 4, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		mkgui_accel_add(win, MI_SAVE, MKGUI_MOD_CONTROL, 's');
		inject_key(win, 's', MKGUI_MOD_CONTROL);
		CHECK(poll_for_event(win, MKGUI_EVENT_MENU, MI_SAVE), "expected MENU event for Ctrl+S on menuitem");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Split
// ---------------------------------------------------------------------------

// [=]===^=[ test_split_drag_event ]=================================[=]
static void test_split_drag_event(void) {
	TEST_BEGIN("hsplit: dragging divider fires MKGUI_EVENT_SPLIT_MOVED");
	enum { WIN = 1, SPLIT = 2, TOP = 3, BOT = 4 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_HSPLIT, SPLIT, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_PANEL,  TOP,   "",     "", SPLIT, 0,   0,   MKGUI_REGION_TOP, 0, 0),
		MKGUI_W(MKGUI_PANEL,  BOT,   "",     "", SPLIT, 0,   0,   MKGUI_REGION_BOTTOM, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 4, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		int32_t idx = find_widget_idx(win, SPLIT);
		int32_t sx = win->rects[idx].x + win->rects[idx].w / 2;
		int32_t sy = win->rects[idx].y + win->rects[idx].h / 2;
		inject_motion(win, sx, sy);
		inject_press(win, sx, sy, 1, 0);
		poll_for_event(win, MKGUI_EVENT_NONE, 0);
		struct mkgui_plat_event pev;
		memset(&pev, 0, sizeof(pev));
		pev.type = MKGUI_PLAT_MOTION;
		pev.x = sx;
		pev.y = sy + 20;
		platform_deferred_push(win, &pev);
		inject_release(win, sx, sy + 20, 1);
		CHECK(poll_for_event(win, MKGUI_EVENT_SPLIT_MOVED, SPLIT), "expected SPLIT_MOVED event");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Undo/Redo
// ---------------------------------------------------------------------------

// [=]===^=[ test_input_undo ]=======================================[=]
static void test_input_undo(void) {
	TEST_BEGIN("input: Ctrl+Z undoes last typed character");
	enum { WIN = 1, VBOX1 = 2, INP = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_INPUT,  INP,   "",     "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		run_layout(win);
		inject_click(win, INP);
		poll_for_event(win, MKGUI_EVENT_NONE, 0);
		inject_key(win, 'h', 0);
		poll_for_event(win, MKGUI_EVENT_NONE, 0);
		struct mkgui_input_data *inp = find_input_data(win, INP);
		CHECK(inp && strcmp(inp->text, "h") == 0, "should have 'h' after typing");
		inp->undo_last_ms = 0;
		inject_key(win, 'i', 0);
		poll_for_event(win, MKGUI_EVENT_NONE, 0);
		CHECK(inp && strcmp(inp->text, "hi") == 0, "should have 'hi' after typing");
		inject_key(win, 'z', MKGUI_MOD_CONTROL);
		poll_for_event(win, MKGUI_EVENT_NONE, 0);
		CHECK(inp && strcmp(inp->text, "h") == 0, "should have 'h' after Ctrl+Z (got '%s')", inp->text);
		inject_key(win, 'y', MKGUI_MOD_CONTROL);
		poll_for_event(win, MKGUI_EVENT_NONE, 0);
		CHECK(inp && strcmp(inp->text, "hi") == 0, "should have 'hi' after Ctrl+Y (got '%s')", inp->text);
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(void) {
	printf("mkgui event test harness\n");
	printf("========================\n\n");

	test_button_click();
	test_checkbox_click_event();
	test_radio_click_event();
	test_toggle_click_event();
	test_input_type_event();
	test_input_submit_event();
	test_textarea_type_event();
	test_textarea_cursor_event();
	test_slider_key_event();
	test_spinbox_click_event();
	test_dropdown_select_event();
	test_tabs_click_event();
	test_scrollbar_wheel_event();
	test_button_context_event();
	test_hover_events();
	test_focus_events();
	test_key_event();
	test_accel_event();
	test_accel_menu_event();
	test_split_drag_event();
	test_input_undo();

	printf("\n========================\n");
	printf("%u tests: %u passed, %u failed\n", tests_run, tests_passed, tests_failed);
	return tests_failed ? 1 : 0;
}
