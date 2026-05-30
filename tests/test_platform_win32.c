// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT
//
// Win32 platform-layer test harness for mkgui (built with mingw, run under
// wine). The GUI core (plat_event -> mkgui_event) is platform-agnostic and
// fully covered by the Linux suites; the software renderer and widget data
// are identical on both platforms. This test exercises only the divergent
// Win32 surface: windowing lifecycle and the native-message -> plat_event
// translation in the real WndProc (mouse, keyboard, resize, close), plus the
// VK -> keysym mapping that must agree with the X11 keysym space the core
// compares against.

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

static void run_layout(struct mkgui_window *win) {
	dirty_all(win);
	layout_widgets(win);
	win->dirty = 0;
	win->dirty_full = 0;
	win->dirty_count = 0;
}

// Drain whatever the window manager queued at creation (paint, initial size)
// so later assertions only see the messages this test posts.
static void settle(struct mkgui_window *win) {
	struct mkgui_event ev;
	for(uint32_t i = 0; i < 200; ++i) {
		if(!mkgui_window_poll(win, &ev)) {
			break;
		}
	}
}

static void post_msg(struct mkgui_window *win, UINT msg, WPARAM wp, LPARAM lp) {
	PostMessageA(win->plat.hwnd, msg, wp, lp);
}

// mkgui_window_poll pumps the Win32 message queue internally, so looping it
// dispatches posted messages through the WndProc and drains the resulting
// plat_events. Returns the first matching event (type NONE if not seen).
static struct mkgui_event poll_capture(struct mkgui_window *win, uint32_t event_type, uint32_t target_id) {
	struct mkgui_event ev;
	struct mkgui_event hit;
	memset(&hit, 0, sizeof(hit));
	for(uint32_t i = 0; i < 400; ++i) {
		uint32_t got = mkgui_window_poll(win, &ev);
		if(got && ev.type == event_type && (target_id == 0 || ev.id == target_id)) {
			hit = ev;
			return hit;
		}

		if(!got) {
			// queue drained and no native messages left to pump
			if(i > 4) {
				break;
			}
		}
	}
	return hit;
}

static uint32_t poll_for_event(struct mkgui_window *win, uint32_t event_type, uint32_t target_id) {
	return poll_capture(win, event_type, target_id).type == event_type;
}

// ---------------------------------------------------------------------------
// Windowing lifecycle
// ---------------------------------------------------------------------------

// [=]===^=[ test_window_lifecycle ]===============================[=]
static void test_window_lifecycle(void) {
	TEST_BEGIN("win32: window create/destroy yields a valid HWND");
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, 1, "Test", "", 0, 320, 240, 0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	CHECK(ctx, "ctx_create failed");
	if(ctx) {
		struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 1, NULL, 0, 0);
		CHECK(win, "window_create failed");
		if(win) {
			CHECK(win->plat.hwnd != NULL, "HWND is NULL after create");
			settle(win);
			mkgui_window_destroy(win);
		}
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// VK -> keysym parity: Win32 must map native keys to the same keysym space
// the X11 backend uses (the core compares against MKGUI_KEY_* directly).
// ---------------------------------------------------------------------------

// [=]===^=[ test_vk_keysym_parity ]===============================[=]
static void test_vk_keysym_parity(void) {
	TEST_BEGIN("win32: VK codes translate to canonical MKGUI_KEY_* values");
	CHECK(platform_translate_vk(VK_BACK)   == MKGUI_KEY_BACKSPACE, "VK_BACK");
	CHECK(platform_translate_vk(VK_TAB)    == MKGUI_KEY_TAB,       "VK_TAB");
	CHECK(platform_translate_vk(VK_RETURN) == MKGUI_KEY_RETURN,    "VK_RETURN");
	CHECK(platform_translate_vk(VK_ESCAPE) == MKGUI_KEY_ESCAPE,    "VK_ESCAPE");
	CHECK(platform_translate_vk(VK_DELETE) == MKGUI_KEY_DELETE,    "VK_DELETE");
	CHECK(platform_translate_vk(VK_HOME)   == MKGUI_KEY_HOME,      "VK_HOME");
	CHECK(platform_translate_vk(VK_LEFT)   == MKGUI_KEY_LEFT,      "VK_LEFT");
	CHECK(platform_translate_vk(VK_UP)     == MKGUI_KEY_UP,        "VK_UP");
	CHECK(platform_translate_vk(VK_RIGHT)  == MKGUI_KEY_RIGHT,     "VK_RIGHT");
	CHECK(platform_translate_vk(VK_DOWN)   == MKGUI_KEY_DOWN,      "VK_DOWN");
	CHECK(platform_translate_vk(VK_PRIOR)  == MKGUI_KEY_PAGE_UP,   "VK_PRIOR");
	CHECK(platform_translate_vk(VK_NEXT)   == MKGUI_KEY_PAGE_DOWN, "VK_NEXT");
	CHECK(platform_translate_vk(VK_END)    == MKGUI_KEY_END,       "VK_END");
	TEST_END();
}

// ---------------------------------------------------------------------------
// Mouse: WM_LBUTTONDOWN/UP through the WndProc fires CLICK
// ---------------------------------------------------------------------------

// [=]===^=[ test_mouse_click ]====================================[=]
static void test_mouse_click(void) {
	TEST_BEGIN("win32: posted left button down/up fires CLICK");
	enum { WIN = 1, VBOX1 = 2, BTN = 3 };
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, WIN,   "Test", "", 0,     320, 240, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   VBOX1, "",     "", WIN,   0,   0,   0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, BTN,   "OK",   "", VBOX1, 0,   0,   0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		settle(win);
		run_layout(win);
		int32_t idx = find_widget_idx(win, BTN);
		int32_t cx = win->rects[idx].x + win->rects[idx].w / 2;
		int32_t cy = win->rects[idx].y + win->rects[idx].h / 2;
		LPARAM lp = (LPARAM)((cy << 16) | (cx & 0xffff));
		post_msg(win, WM_LBUTTONDOWN, 0, lp);
		post_msg(win, WM_LBUTTONUP, 0, lp);
		CHECK(poll_for_event(win, MKGUI_EVENT_CLICK, BTN), "expected CLICK on button");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Keyboard: WM_KEYDOWN through the WndProc fires KEY with the mapped keysym
// ---------------------------------------------------------------------------

// [=]===^=[ test_key_through ]====================================[=]
static void test_key_through(void) {
	TEST_BEGIN("win32: posted WM_KEYDOWN fires KEY with translated keysym");
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, 1, "Test", "", 0, 320, 240, 0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 1, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		settle(win);
		post_msg(win, WM_KEYDOWN, VK_DOWN, 0);
		struct mkgui_event ev = poll_capture(win, MKGUI_EVENT_KEY, 0);
		CHECK(ev.type == MKGUI_EVENT_KEY, "expected KEY event");
		CHECK(ev.keysym == MKGUI_KEY_DOWN, "keysym %#x != MKGUI_KEY_DOWN", ev.keysym);
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// ---------------------------------------------------------------------------
// Resize and close
// ---------------------------------------------------------------------------

// [=]===^=[ test_resize_through ]=================================[=]
static void test_resize_through(void) {
	TEST_BEGIN("win32: posted WM_SIZE fires RESIZE with new dimensions");
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, 1, "Test", "", 0, 320, 240, 0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 1, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		settle(win);
		int32_t nw = win->win_w + 80;
		int32_t nh = win->win_h + 60;
		LPARAM lp = (LPARAM)((nh << 16) | (nw & 0xffff));
		post_msg(win, WM_SIZE, SIZE_RESTORED, lp);
		struct mkgui_event ev = poll_capture(win, MKGUI_EVENT_RESIZE, 0);
		CHECK(ev.type == MKGUI_EVENT_RESIZE, "expected RESIZE event");
		CHECK(win->win_w == nw && win->win_h == nh, "window size not updated to %dx%d", nw, nh);
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

// [=]===^=[ test_close_through ]==================================[=]
static void test_close_through(void) {
	TEST_BEGIN("win32: posted WM_CLOSE fires CLOSE");
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, 1, "Test", "", 0, 320, 240, 0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 1, NULL, 0, 0);
	CHECK(win, "create failed");
	if(win) {
		settle(win);
		post_msg(win, WM_CLOSE, 0, 0);
		CHECK(poll_for_event(win, MKGUI_EVENT_CLOSE, 0), "expected CLOSE event");
		mkgui_window_destroy(win);
		mkgui_ctx_destroy(ctx);
	}
	TEST_END();
}

int main(void) {
	printf("mkgui win32 platform test harness\n");
	printf("=================================\n\n");

	test_window_lifecycle();
	test_vk_keysym_parity();
	test_mouse_click();
	test_key_through();
	test_resize_through();
	test_close_through();

	printf("\n=================================\n");
	printf("%u tests: %u passed, %u failed\n", tests_run, tests_passed, tests_failed);
	return tests_failed ? 1 : 0;
}
