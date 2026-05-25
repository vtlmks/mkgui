// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT
//
// Tests for hidden-window support: MKGUI_WINDOW_HIDDEN, MKGUI_WINDOW_HIDE_ON_CLOSE,
// mkgui_window_show / mkgui_window_hide / mkgui_window_is_visible, and the
// mkgui_ctx_run "exit when no window is visible" rule.

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
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, visible_widgets, sizeof(visible_widgets) / sizeof(visible_widgets[0]), NULL, 0, 0);
	CHECK(win != NULL, "default-flag create returned non-NULL win");
	CHECK(mkgui_window_is_visible(win) == 1, "default window is visible");
	mkgui_window_destroy(win);
	mkgui_ctx_destroy(ctx);
}

static void test_run_returns_with_no_visible_windows(void) {
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, hidden_widgets, sizeof(hidden_widgets) / sizeof(hidden_widgets[0]), NULL, 0, 0);
	CHECK(win != NULL, "create with HIDDEN flag returned non-NULL win");
	CHECK(mkgui_window_is_visible(win) == 0, "HIDDEN flag suppresses initial map");
	mkgui_ctx_run(mkgui_window_get_ctx(win), NULL, NULL);
	CHECK(win->should_close == 0, "mkgui_ctx_run returned without setting should_close");
	mkgui_window_destroy(win);
	mkgui_ctx_destroy(ctx);
}

static void test_show_hide_cycle(void) {
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, hidden_widgets, sizeof(hidden_widgets) / sizeof(hidden_widgets[0]), NULL, 0, 0);
	CHECK(mkgui_window_is_visible(win) == 0, "starts hidden");

	mkgui_window_show(win);
	CHECK(mkgui_window_is_visible(win) == 1, "show -> visible");

	mkgui_window_hide(win);
	CHECK(mkgui_window_is_visible(win) == 0, "hide -> hidden");

	mkgui_window_show(win);
	CHECK(mkgui_window_is_visible(win) == 1, "show again -> visible");

	mkgui_window_show(win);
	CHECK(mkgui_window_is_visible(win) == 1, "show on already-visible is idempotent");

	mkgui_window_hide(win);
	mkgui_window_hide(win);
	CHECK(mkgui_window_is_visible(win) == 0, "hide on already-hidden is idempotent");

	mkgui_window_destroy(win);
	mkgui_ctx_destroy(ctx);
}

// Inject a synthesized WM_DELETE close on the main window, then verify:
//  - HIDE_ON_CLOSE intercepts: window becomes hidden, should_close stays 0
//  - mkgui_ctx_run returns immediately afterward (no visible windows)
static void test_hide_on_close_intercept(void) {
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, hide_on_close_widgets, sizeof(hide_on_close_widgets) / sizeof(hide_on_close_widgets[0]), NULL, 0, 0);
	CHECK(mkgui_window_is_visible(win) == 0, "HIDDEN flag still applied");

	mkgui_window_show(win);
	CHECK(mkgui_window_is_visible(win) == 1, "show after create");

	XSync(win->plat.dpy, False);

	XClientMessageEvent xc;
	memset(&xc, 0, sizeof(xc));
	xc.type = ClientMessage;
	xc.display = win->plat.dpy;
	xc.window = win->plat.win;
	xc.message_type = XInternAtom(win->plat.dpy, "WM_PROTOCOLS", False);
	xc.format = 32;
	xc.data.l[0] = (long)win->plat.atoms.wm_delete;
	xc.data.l[1] = CurrentTime;
	XSendEvent(win->plat.dpy, win->plat.win, False, NoEventMask, (XEvent *)&xc);
	XFlush(win->plat.dpy);

	mkgui_ctx_run(mkgui_window_get_ctx(win), NULL, NULL);

	CHECK(win->should_close == 0, "HIDE_ON_CLOSE prevented should_close");
	CHECK(mkgui_window_is_visible(win) == 0, "WM close hid the window");
	mkgui_window_destroy(win);
	mkgui_ctx_destroy(ctx);
}

static struct mkgui_widget undecorated_widgets[] = {
	{ MKGUI_WINDOW, 1, 0, 320, 200, 0, MKGUI_WINDOW_UNDECORATED | MKGUI_WINDOW_HIDDEN, 0, 0, 0, 0, 0, 0, "test_undeco", "" },
};

static struct mkgui_widget canvas_window_widgets[] = {
	{ MKGUI_WINDOW, 1, 0, 275, 116, 0, MKGUI_WINDOW_UNDECORATED | MKGUI_WINDOW_CANVAS | MKGUI_WINDOW_HIDDEN, 0, 0, 0, 0, 0, 0, "test_canvas", "" },
	{ MKGUI_CANVAS, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "", "" },
};

static void test_undecorated_flag(void) {
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, undecorated_widgets, sizeof(undecorated_widgets) / sizeof(undecorated_widgets[0]), NULL, 0, 0);
	CHECK(win != NULL, "undecorated create returned non-NULL win");
	CHECK(win->undecorated == 1, "win->undecorated is set");
	CHECK(win->canvas_window == 0, "win->canvas_window is not set");
	mkgui_window_destroy(win);
	mkgui_ctx_destroy(ctx);
}

static void test_canvas_window_flag(void) {
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, canvas_window_widgets, sizeof(canvas_window_widgets) / sizeof(canvas_window_widgets[0]), NULL, 0, 0);
	CHECK(win != NULL, "canvas window create returned non-NULL win");
	CHECK(win->undecorated == 1, "win->undecorated is set");
	CHECK(win->canvas_window == 1, "win->canvas_window is set");
	mkgui_window_destroy(win);
	mkgui_ctx_destroy(ctx);
}

static void test_window_move_get_position(void) {
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, undecorated_widgets, sizeof(undecorated_widgets) / sizeof(undecorated_widgets[0]), NULL, 0, 0);
	CHECK(win != NULL, "create for move test");
	mkgui_window_show(win);
	XSync(win->plat.dpy, False);
	mkgui_window_move(win, 50, 75);
	XSync(win->plat.dpy, False);
	while(XPending(win->plat.dpy)) {
		XEvent xev;
		XNextEvent(win->plat.dpy, &xev);
	}
	int32_t x = -1, y = -1;
	mkgui_window_get_position(win, &x, &y);
	CHECK(x == 50, "window x after move");
	CHECK(y == 75, "window y after move");
	mkgui_window_destroy(win);
	mkgui_ctx_destroy(ctx);
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
