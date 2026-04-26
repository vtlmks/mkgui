// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT
//
// Tests for hidden-window support: MKGUI_WINDOW_HIDDEN, MKGUI_WINDOW_HIDE_ON_CLOSE,
// mkgui_window_show / mkgui_window_hide / mkgui_window_is_visible, and the
// mkgui_run "exit when no window is visible" rule.

#include "../mkgui.c"

static uint32_t tests_run;
static uint32_t tests_failed;

#define CHECK(cond, msg) \
	do { \
		++tests_run; \
		if(!(cond)) { \
			fprintf(stderr, "FAIL: %s (line %d)\n", (msg), __LINE__); \
			++tests_failed; \
		} \
	} while(0)

static struct mkgui_widget visible_widgets[] = {
	{ MKGUI_WINDOW, 1, 0, 320, 200, 0, 0, 0, 0, 0, 0, 0, 0, "test", "" },
};

static struct mkgui_widget hidden_widgets[] = {
	{ MKGUI_WINDOW, 1, 0, 320, 200, 0, MKGUI_WINDOW_HIDDEN, 0, 0, 0, 0, 0, 0, "test_hidden", "" },
};

static struct mkgui_widget hide_on_close_widgets[] = {
	{ MKGUI_WINDOW, 1, 0, 320, 200, 0, MKGUI_WINDOW_HIDDEN | MKGUI_WINDOW_HIDE_ON_CLOSE, 0, 0, 0, 0, 0, 0, "test_hoc", "" },
};

static void test_default_is_visible(void) {
	struct mkgui_ctx *ctx = mkgui_create(visible_widgets, sizeof(visible_widgets) / sizeof(visible_widgets[0]));
	CHECK(ctx != NULL, "default-flag create returned non-NULL ctx");
	CHECK(mkgui_window_is_visible(ctx) == 1, "default window is visible");
	mkgui_destroy(ctx);
}

static void test_run_returns_with_no_visible_windows(void) {
	struct mkgui_ctx *ctx = mkgui_create(hidden_widgets, sizeof(hidden_widgets) / sizeof(hidden_widgets[0]));
	CHECK(ctx != NULL, "create with HIDDEN flag returned non-NULL ctx");
	CHECK(mkgui_window_is_visible(ctx) == 0, "HIDDEN flag suppresses initial map");
	mkgui_run(ctx, NULL, NULL);
	CHECK(ctx->close_requested == 0, "mkgui_run returned without setting close_requested");
	mkgui_destroy(ctx);
}

static void test_show_hide_cycle(void) {
	struct mkgui_ctx *ctx = mkgui_create(hidden_widgets, sizeof(hidden_widgets) / sizeof(hidden_widgets[0]));
	CHECK(mkgui_window_is_visible(ctx) == 0, "starts hidden");

	mkgui_window_show(ctx);
	CHECK(mkgui_window_is_visible(ctx) == 1, "show -> visible");

	mkgui_window_hide(ctx);
	CHECK(mkgui_window_is_visible(ctx) == 0, "hide -> hidden");

	mkgui_window_show(ctx);
	CHECK(mkgui_window_is_visible(ctx) == 1, "show again -> visible");

	mkgui_window_show(ctx);
	CHECK(mkgui_window_is_visible(ctx) == 1, "show on already-visible is idempotent");

	mkgui_window_hide(ctx);
	mkgui_window_hide(ctx);
	CHECK(mkgui_window_is_visible(ctx) == 0, "hide on already-hidden is idempotent");

	mkgui_destroy(ctx);
}

// Inject a synthesized WM_DELETE close on the main window, then verify:
//  - HIDE_ON_CLOSE intercepts: window becomes hidden, close_requested stays 0
//  - mkgui_run returns immediately afterward (no visible windows)
static void test_hide_on_close_intercept(void) {
	struct mkgui_ctx *ctx = mkgui_create(hide_on_close_widgets, sizeof(hide_on_close_widgets) / sizeof(hide_on_close_widgets[0]));
	CHECK(mkgui_window_is_visible(ctx) == 0, "HIDDEN flag still applied");

	mkgui_window_show(ctx);
	CHECK(mkgui_window_is_visible(ctx) == 1, "show after create");

	XSync(ctx->plat.dpy, False);

	XClientMessageEvent xc;
	memset(&xc, 0, sizeof(xc));
	xc.type = ClientMessage;
	xc.display = ctx->plat.dpy;
	xc.window = ctx->plat.win;
	xc.message_type = XInternAtom(ctx->plat.dpy, "WM_PROTOCOLS", False);
	xc.format = 32;
	xc.data.l[0] = (long)ctx->plat.wm_delete;
	xc.data.l[1] = CurrentTime;
	XSendEvent(ctx->plat.dpy, ctx->plat.win, False, NoEventMask, (XEvent *)&xc);
	XFlush(ctx->plat.dpy);

	mkgui_run(ctx, NULL, NULL);

	CHECK(ctx->close_requested == 0, "HIDE_ON_CLOSE prevented close_requested");
	CHECK(mkgui_window_is_visible(ctx) == 0, "WM close hid the window");
	mkgui_destroy(ctx);
}

int32_t main(void) {
	test_default_is_visible();
	test_run_returns_with_no_visible_windows();
	test_show_hide_cycle();
	test_hide_on_close_intercept();

	fprintf(stderr, "\n%u tests, %u failed\n", tests_run, tests_failed);
	return tests_failed > 0 ? 1 : 0;
}
