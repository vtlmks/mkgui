// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <string.h>
#include "mkgui.h"
#include "mkguiicons_names.h"

static uint32_t checks;
static uint32_t failures;
static uint32_t canvas_calls;

// [=]===^=[ check ]============================================================================[=]
static void check(uint32_t condition, char *message) {
	++checks;

	if(!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		++failures;

	}
}

// [=]===^=[ draw_canvas ]======================================================================[=]
static void draw_canvas(struct mkgui_window *win, uint32_t id, uint32_t *pixels, int32_t x, int32_t y, int32_t w, int32_t h, void *userdata) {
	int32_t width, height;
	(void)id;
	mkgui_get_window_size(win, &width, &height);

	if(w > 0 && h > 0 && x >= 0 && y >= 0 && x < width && y < height) {
		pixels[(size_t)y * (size_t)width + (size_t)x] = 0xff123456;
		++*(uint32_t *)userdata;

	}
}

// [=]===^=[ stop_timer ]=======================================================================[=]
static void stop_timer(struct mkgui_window *win, uint32_t timer_id, void *userdata) {
	++*(uint32_t *)userdata;
	mkgui_window_remove_timer(win, timer_id);
	mkgui_ctx_quit(mkgui_window_get_ctx(win));

}

// [=]===^=[ main ]=============================================================================[=]
int main(void) {
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, 1, "mkgui shared consumer test", "", 0, 400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX, 2, "", "", 1, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, 3, "Save", MKICON_SAVE, 2, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_INPUT, 4, "", "", 2, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_CANVAS, 5, "", "", 2, 0, 0, 0, 0, 1),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	check(ctx != NULL, "context creation");

	if(ctx) {
		mkgui_ctx_set_scale(ctx, 1.0f);
		mkgui_ctx_set_theme(ctx, default_theme());
		struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, sizeof(widgets) / sizeof(widgets[0]), NULL, 0, 0);
		check(win != NULL, "window creation");

		if(win) {
			uint32_t timer_calls = 0;
			mkgui_input_set(win, 4, "Across the library boundary");
			check(strcmp(mkgui_input_get(win, 4), "Across the library boundary") == 0, "input storage ownership");
			check(mkgui_window_get_ctx(win) == ctx, "window context identity");
			check(mkgui_icon_at_size(win, MKICON_SAVE, 18) >= 0, "shared SVG icon rasterization");
			mkgui_canvas_set_callback(win, 5, draw_canvas, &canvas_calls);
			mkgui_window_invalidate(win);
			mkgui_ctx_wait_until(ctx, mkgui_now_ns());
			uint32_t timer_id = mkgui_window_add_timer(win, 10000000, stop_timer, &timer_calls);
			check(timer_id != 0, "timer creation");

			if(timer_id) {
				mkgui_ctx_run(ctx, NULL, NULL);
				check(timer_calls == 1, "timer callback into consumer");
				check(canvas_calls > 0, "canvas callback into consumer");

			}

			mkgui_ctx_set_scale(ctx, 2.13f);
			check(mkgui_ctx_get_scale(ctx) == 2.13f, "scale update");
			check(mkgui_icon_at_size(win, MKICON_SAVE, 18) >= 0, "SVG rerasterization after scaling");
			mkgui_ctx_set_theme(ctx, light_theme());
			check(mkgui_icon_at_size(win, MKICON_SAVE, 18) >= 0, "SVG rerasterization after theme change");
			struct mkgui_widget added = MKGUI_W(MKGUI_LABEL, 6, "Dynamic", "", 2, 0, 0, 0, 0, 0);
			check(mkgui_widget_add(win, added, 4) != 0, "dynamic widget insertion");
			check(mkgui_widget_remove(win, 6) != 0, "dynamic widget removal");
			mkgui_window_hide(win);
			check(!mkgui_window_is_visible(win), "hide window");
			mkgui_window_show(win);
			check(mkgui_window_is_visible(win), "show window");
			mkgui_window_destroy(win);

		}

		mkgui_ctx_destroy(ctx);

	}

	printf("Shared consumer: %u checks, %u failures\n", checks, failures);
	return failures ? 1 : 0;

}
