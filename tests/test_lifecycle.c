// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT
//
// Lifecycle and host-loop tests for the v0.3.0-beta API:
//   - mkgui_ctx_create / mkgui_ctx_destroy (and auto-destroy of windows)
//   - mkgui_window_create with parent=NULL and parent=other-window
//   - mkgui_ctx_set_quit / mkgui_ctx_should_quit with cancellation
//   - mkgui_window_set_close / mkgui_window_should_close with cancellation
//   - mkgui_ctx_pump_others on a two-window ctx
//   - Host-owned main loop pattern (mkgui_now_ns + mkgui_ctx_wait_until)

#include "../mkgui.c"

static uint32_t fail_count;

#define CHECK(cond, fmt, ...) do { \
	if(!(cond)) { \
		fprintf(stderr, "FAIL line %d: " fmt "\n", __LINE__, ##__VA_ARGS__); \
		++fail_count; \
	} \
} while(0)

static uint32_t pump_others_secondary_calls;

// [=]===^=[ secondary_event_cb ]===================================[=]
static void secondary_event_cb(struct mkgui_window *win, struct mkgui_event *ev, void *ud) {
	(void)win; (void)ev; (void)ud;
	++pump_others_secondary_calls;
}

// [=]===^=[ test_ctx_lifecycle_basic ]=============================[=]
// ctx_create returns a valid pointer with sensible defaults, destroy
// is null-safe.
static void test_ctx_lifecycle_basic(void) {
	fprintf(stderr, "test_ctx_lifecycle_basic...\n");

	struct mkgui_ctx *ctx = mkgui_ctx_create();
	CHECK(ctx != NULL, "ctx_create returned NULL");
	CHECK(ctx->window_count == 0, "fresh ctx has %u windows, expected 0", ctx->window_count);
	CHECK(ctx->should_quit == 0, "fresh ctx has should_quit set");
	CHECK(ctx->primary == NULL, "fresh ctx has primary set");
	CHECK(ctx->scale > 0.99f && ctx->scale < 1.01f, "fresh ctx scale = %f, expected 1.0", (double)ctx->scale);
	mkgui_ctx_destroy(ctx);

	// null-safe
	mkgui_ctx_destroy(NULL);
}

// [=]===^=[ test_set_quit_cancel ]=================================[=]
// set_quit can both set and clear; ctx_quit is the shorthand for set(1).
static void test_set_quit_cancel(void) {
	fprintf(stderr, "test_set_quit_cancel...\n");

	struct mkgui_ctx *ctx = mkgui_ctx_create();
	CHECK(mkgui_ctx_should_quit(ctx) == 0, "fresh ctx should_quit nonzero");

	mkgui_ctx_quit(ctx);
	CHECK(mkgui_ctx_should_quit(ctx) == 1, "after quit, should_quit != 1");

	mkgui_ctx_set_quit(ctx, 0);
	CHECK(mkgui_ctx_should_quit(ctx) == 0, "after set_quit(0), should_quit != 0");

	mkgui_ctx_set_quit(ctx, 1);
	CHECK(mkgui_ctx_should_quit(ctx) == 1, "after set_quit(1), should_quit != 1");

	mkgui_ctx_destroy(ctx);
}

// [=]===^=[ test_window_create_top_level ]=========================[=]
// Top-level window creation registers in ctx->windows and sets primary.
static void test_window_create_top_level(void) {
	fprintf(stderr, "test_window_create_top_level...\n");

	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, 1, "Primary", "", 0, 320, 240, 0, 0, 0),
		MKGUI_W(MKGUI_LABEL,  2, "hi",      "", 1, 0,   0,   0, 0, 0),
	};

	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 2, NULL, 0, 0);
	CHECK(win != NULL, "window_create returned NULL");

	CHECK(ctx->window_count == 1, "ctx->window_count = %u, expected 1", ctx->window_count);
	CHECK(ctx->windows[0] == win, "ctx->windows[0] not equal to created window");
	CHECK(ctx->primary == win, "ctx->primary not equal to first window");
	CHECK(mkgui_window_get_ctx(win) == ctx, "window_get_ctx mismatch");

	mkgui_window_destroy(win);
	CHECK(ctx->window_count == 0, "after destroy, ctx->window_count = %u, expected 0", ctx->window_count);
	CHECK(ctx->primary == NULL, "after destroy, ctx->primary not NULL");

	mkgui_ctx_destroy(ctx);
}

// [=]===^=[ test_two_windows_with_parent ]=========================[=]
// Create primary + secondary (transient on primary). Both end up in
// ctx->windows[] in creation order. Primary stays primary.
static void test_two_windows_with_parent(void) {
	fprintf(stderr, "test_two_windows_with_parent...\n");

	struct mkgui_widget primary_widgets[] = {
		MKGUI_W(MKGUI_WINDOW, 1, "Primary",  "", 0, 320, 240, 0, 0, 0),
		MKGUI_W(MKGUI_LABEL,  2, "main",     "", 1, 0,   0,   0, 0, 0),
	};
	struct mkgui_widget secondary_widgets[] = {
		MKGUI_W(MKGUI_WINDOW, 10, "Secondary", "", 0, 240, 160, 0, 0, 0),
		MKGUI_W(MKGUI_LABEL,  11, "second",    "", 10, 0,  0,   0, 0, 0),
	};

	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *primary = mkgui_window_create(ctx, NULL, primary_widgets, 2, NULL, 0, 0);
	CHECK(primary != NULL, "primary create failed");

	struct mkgui_window *secondary = mkgui_window_create(ctx, primary, secondary_widgets, 2, "Secondary", 240, 160);
	CHECK(secondary != NULL, "secondary create failed");

	CHECK(ctx->window_count == 2, "expected 2 windows, have %u", ctx->window_count);
	CHECK(ctx->primary == primary, "primary should be the first window created");
	CHECK(mkgui_window_get_ctx(secondary) == ctx, "secondary's ctx mismatch");
	CHECK(secondary->parent == primary, "secondary's parent != primary");
	CHECK(secondary->plat.is_child == 1, "secondary should be marked as child");
	CHECK(primary->plat.is_child == 0, "primary should NOT be marked as child");

	// destroy secondary first; primary unaffected
	mkgui_window_destroy(secondary);
	CHECK(ctx->window_count == 1, "after secondary destroy, have %u windows", ctx->window_count);
	CHECK(ctx->primary == primary, "primary should remain primary after secondary destroy");

	mkgui_window_destroy(primary);
	mkgui_ctx_destroy(ctx);
}

// [=]===^=[ test_window_should_close_cancel ]=====================[=]
// set_close can both set and clear (mirrors set_quit for windows).
static void test_window_should_close_cancel(void) {
	fprintf(stderr, "test_window_should_close_cancel...\n");

	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, 1, "T", "", 0, 200, 150, 0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 1, NULL, 0, 0);

	CHECK(mkgui_window_should_close(win) == 0, "fresh window should_close != 0");

	mkgui_window_set_close(win, 1);
	CHECK(mkgui_window_should_close(win) == 1, "set_close(1) didn't take");

	mkgui_window_set_close(win, 0);
	CHECK(mkgui_window_should_close(win) == 0, "set_close(0) didn't clear");

	mkgui_window_destroy(win);
	mkgui_ctx_destroy(ctx);
}

// [=]===^=[ test_pump_others ]=====================================[=]
// pump_others drains the secondary's deferred queue through its
// registered callback, without touching the primary.
static void test_pump_others(void) {
	fprintf(stderr, "test_pump_others...\n");

	struct mkgui_widget pw[] = { MKGUI_W(MKGUI_WINDOW, 1,  "P", "", 0,  200, 150, 0, 0, 0) };
	struct mkgui_widget sw[] = { MKGUI_W(MKGUI_WINDOW, 10, "S", "", 0,  200, 150, 0, 0, 0) };

	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *p = mkgui_window_create(ctx, NULL, pw, 1, NULL, 0, 0);
	struct mkgui_window *s = mkgui_window_create(ctx, p, sw, 1, "S", 200, 150);

	mkgui_window_set_callback(s, secondary_event_cb, NULL);

	// Inject a synthetic key event into the secondary's deferred queue so
	// pump_others has a user-facing event to deliver to the callback.
	// (EXPOSE only marks the window dirty internally and never reaches
	// the user cb, so we use a key event which is always delivered.)
	pump_others_secondary_calls = 0;
	struct mkgui_plat_event pev = {0};
	pev.type = MKGUI_PLAT_KEY;
	pev.keysym = 'a';
	pev.popup_idx = -1;
#ifdef _WIN32
	evq_push_ctx(&s->plat, &pev);
#else
	platform_deferred_push(s, &pev);
#endif

	mkgui_ctx_pump_others(ctx);
	CHECK(pump_others_secondary_calls >= 1, "secondary callback should have run at least once, got %u", pump_others_secondary_calls);

	// Same window with no callback: pump_others must silently skip it.
	mkgui_window_set_callback(s, NULL, NULL);
#ifdef _WIN32
	evq_push_ctx(&s->plat, &pev);
#else
	platform_deferred_push(s, &pev);
#endif
	uint32_t before = pump_others_secondary_calls;
	mkgui_ctx_pump_others(ctx);
	CHECK(pump_others_secondary_calls == before, "pump_others should skip windows without a callback");

	mkgui_window_destroy(s);
	mkgui_window_destroy(p);
	mkgui_ctx_destroy(ctx);
}

// [=]===^=[ test_ctx_destroy_auto_destroys_windows ]==============[=]
// User forgets to destroy windows: ctx_destroy must clean them up so no
// platform resources leak.
static void test_ctx_destroy_auto_destroys_windows(void) {
	fprintf(stderr, "test_ctx_destroy_auto_destroys_windows...\n");

	struct mkgui_widget pw[] = { MKGUI_W(MKGUI_WINDOW, 1,  "P", "", 0,  200, 150, 0, 0, 0) };
	struct mkgui_widget sw[] = { MKGUI_W(MKGUI_WINDOW, 10, "S", "", 0,  200, 150, 0, 0, 0) };

	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *p = mkgui_window_create(ctx, NULL, pw, 1, NULL, 0, 0);
	(void)mkgui_window_create(ctx, p, sw, 1, "S", 200, 150);
	CHECK(ctx->window_count == 2, "expected 2 windows pre-destroy");

	// Intentionally skip window_destroy calls -- auto-destroy is the
	// contract. After ctx_destroy, no windows should be tracked anywhere.
	mkgui_ctx_destroy(ctx);
	CHECK(g_ctx == NULL, "g_ctx should be NULL after ctx_destroy");
}

// [=]===^=[ test_host_loop_two_windows ]==========================[=]
// Exercise the host-owned mainloop pattern with two windows. Loop
// terminates after a fixed number of host ticks (we can't inject real X11
// close events from a unit test, so we self-terminate via set_quit).
// Verifies that mkgui_now_ns advances, mkgui_ctx_wait_until honors a
// near-term deadline, and the loop drains both windows' events without
// blocking forever.
static void test_host_loop_two_windows(void) {
	fprintf(stderr, "test_host_loop_two_windows...\n");

	struct mkgui_widget pw[] = { MKGUI_W(MKGUI_WINDOW, 1,  "P", "", 0,  200, 150, 0, 0, 0) };
	struct mkgui_widget sw[] = { MKGUI_W(MKGUI_WINDOW, 10, "S", "", 0,  200, 150, 0, 0, 0) };

	struct mkgui_ctx *ctx = mkgui_ctx_create();
	struct mkgui_window *p = mkgui_window_create(ctx, NULL, pw, 1, NULL, 0, 0);
	struct mkgui_window *s = mkgui_window_create(ctx, p, sw, 1, "S", 200, 150);

	mkgui_window_set_callback(s, secondary_event_cb, NULL);
	pump_others_secondary_calls = 0;

	uint64_t t0 = mkgui_now_ns();
	uint64_t interval_ns = 10000000ull;  // 10 ms host tick
	uint64_t next_tick = mkgui_now_ns() + interval_ns;
	uint32_t host_ticks = 0;
	uint32_t target_ticks = 5;

	while(!mkgui_ctx_should_quit(ctx)) {
		struct mkgui_event ev;
		while(mkgui_window_poll(p, &ev)) {
			// drain primary - tests don't generate any real events
		}
		mkgui_ctx_pump_others(ctx);

		uint64_t now = mkgui_now_ns();
		if(now >= next_tick) {
			++host_ticks;
			if(host_ticks >= target_ticks) {
				mkgui_ctx_quit(ctx);
				break;
			}
			next_tick += interval_ns;
			if(next_tick <= now) {
				next_tick = now + interval_ns;
			}
		}
		mkgui_ctx_wait_until(ctx, next_tick);
	}

	uint64_t elapsed = mkgui_now_ns() - t0;
	CHECK(host_ticks == target_ticks, "expected %u host ticks, got %u", target_ticks, host_ticks);
	CHECK(elapsed >= (target_ticks - 1) * interval_ns,
	      "elapsed %llu ns < expected min %llu ns (loop terminated too early)",
	      (unsigned long long)elapsed,
	      (unsigned long long)((target_ticks - 1) * interval_ns));

	mkgui_window_destroy(s);
	mkgui_window_destroy(p);
	mkgui_ctx_destroy(ctx);
}

// [=]===^=[ main ]================================================[=]
int main(void) {
	test_ctx_lifecycle_basic();
	test_set_quit_cancel();
	test_window_create_top_level();
	test_two_windows_with_parent();
	test_window_should_close_cancel();
	test_pump_others();
	test_ctx_destroy_auto_destroys_windows();
	test_host_loop_two_windows();

	if(fail_count == 0) {
		fprintf(stderr, "all tests passed\n");
		return 0;
	}
	fprintf(stderr, "%u test(s) failed\n", fail_count);
	return 1;
}
