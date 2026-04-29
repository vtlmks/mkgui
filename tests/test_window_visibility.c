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

static struct mkgui_widget undecorated_widgets[] = {
	{ MKGUI_WINDOW, 1, 0, 320, 200, 0, MKGUI_WINDOW_UNDECORATED | MKGUI_WINDOW_HIDDEN, 0, 0, 0, 0, 0, 0, "test_undeco", "" },
};

static struct mkgui_widget canvas_window_widgets[] = {
	{ MKGUI_WINDOW, 1, 0, 275, 116, 0, MKGUI_WINDOW_UNDECORATED | MKGUI_WINDOW_CANVAS | MKGUI_WINDOW_HIDDEN, 0, 0, 0, 0, 0, 0, "test_canvas", "" },
	{ MKGUI_CANVAS, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "", "" },
};

static void test_undecorated_flag(void) {
	struct mkgui_ctx *ctx = mkgui_create(undecorated_widgets, sizeof(undecorated_widgets) / sizeof(undecorated_widgets[0]));
	CHECK(ctx != NULL, "undecorated create returned non-NULL ctx");
	CHECK(ctx->undecorated == 1, "ctx->undecorated is set");
	CHECK(ctx->canvas_window == 0, "ctx->canvas_window is not set");
	mkgui_destroy(ctx);
}

static void test_canvas_window_flag(void) {
	struct mkgui_ctx *ctx = mkgui_create(canvas_window_widgets, sizeof(canvas_window_widgets) / sizeof(canvas_window_widgets[0]));
	CHECK(ctx != NULL, "canvas window create returned non-NULL ctx");
	CHECK(ctx->undecorated == 1, "ctx->undecorated is set");
	CHECK(ctx->canvas_window == 1, "ctx->canvas_window is set");
	mkgui_destroy(ctx);
}

static void test_window_move_get_position(void) {
	struct mkgui_ctx *ctx = mkgui_create(undecorated_widgets, sizeof(undecorated_widgets) / sizeof(undecorated_widgets[0]));
	CHECK(ctx != NULL, "create for move test");
	mkgui_window_show(ctx);
	XSync(ctx->plat.dpy, False);
	mkgui_window_move(ctx, 50, 75);
	XSync(ctx->plat.dpy, False);
	while(XPending(ctx->plat.dpy)) {
		XEvent xev;
		XNextEvent(ctx->plat.dpy, &xev);
	}
	int32_t x = -1, y = -1;
	mkgui_window_get_position(ctx, &x, &y);
	CHECK(x == 50, "window x after move");
	CHECK(y == 75, "window y after move");
	mkgui_destroy(ctx);
}

int32_t main(void) {
	test_default_is_visible();
	test_run_returns_with_no_visible_windows();
	test_show_hide_cycle();
	test_hide_on_close_intercept();
	test_undecorated_flag();
	test_canvas_window_flag();
	test_window_move_get_position();

	fprintf(stderr, "\n%u tests, %u failed\n", tests_run, tests_failed);
	return tests_failed > 0 ? 1 : 0;
}
