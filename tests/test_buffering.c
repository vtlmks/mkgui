// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT

#include "../mkgui.c"

static uint32_t reference_pixels[512 * 512];
static uint32_t failures;
static uint32_t comparisons;

// [=]===^=[ buffering_check ]====================================[=]
static void buffering_check(struct mkgui_window *win) {
	uint32_t *pixels = win->pixels;
	uint32_t dirty_full = win->dirty_full;
	size_t count = (size_t)win->win_w * (size_t)win->win_h;
	mkwin_sync(win->mkwin);
	win->pixels = reference_pixels;
	win->dirty_full = 1;
	render_widgets(win);
	flush_text(win);
	render_notify(win);
	flush_text(win);
	render_tooltip(win);
	flush_text(win);
	win->pixels = pixels;
	win->dirty_full = dirty_full;
	++comparisons;

	for(size_t i = 0; i < count; ++i) {
		if(pixels[i] != reference_pixels[i]) {
			fprintf(stderr, "FAIL: comparison %u pixel %zu: %08x != %08x\n", comparisons, i, pixels[i], reference_pixels[i]);
			++failures;
			break;

		}
	}
}

// [=]===^=[ buffering_exercise ]=================================[=]
static void buffering_exercise(struct mkgui_window *first, struct mkgui_window *second) {
	struct mkgui_window *views[2] = {first, second};

	for(uint32_t frame = 0; frame < 80; ++frame) {
		for(uint32_t i = 0; i < 2; ++i) {
			struct mkgui_window *win = views[i];
			uint32_t index = 2 + (frame + i) % 5;
			snprintf(win->widgets[index].label, sizeof(win->widgets[index].label), "frame %03u window %u", frame, i);
			dirty_widget(win, index);
			dirty_widget(win, 2 + (frame + i + 2) % 5);

			if(frame % 13 == 0) {
				dirty_all(win);

			}

			mkgui_flush(win);

			if(frame % 7 == 0) {
				buffering_check(win);

			}
		}

		struct mkwin_event event;
		mkwin_poll(first->mkwin, &event);

	}

	for(uint32_t i = 0; i < 2; ++i) {
		buffering_check(views[i]);

	}

	for(uint32_t step = 0; step < 6; ++step) {
		int32_t width = step % 2 ? 280 : 480;
		int32_t height = step % 2 ? 200 : 360;
		first->mkwin->win_w = width;
		first->mkwin->win_h = height;
		mkwin_fb_resize(first->mkwin);
		mkwin_publish(first->mkwin);
		dirty_all(first);

		if(first->win_w != width || first->win_h != height) {
			fprintf(stderr, "FAIL: framebuffer resize to %dx%d\n", width, height);
			++failures;

		}

		for(uint32_t frame = 0; frame < 5; ++frame) {
			snprintf(first->widgets[2].label, sizeof(first->widgets[2].label), "resize %u frame %u", step, frame);
			dirty_widget(first, 2);
			mkgui_flush(first);
			buffering_check(first);

		}
	}

	struct mkwin_popup *popup = mkwin_popup_open(first->mkwin, 0, 0, 64, 48);

	if(popup) {
		for(uint32_t frame = 0; frame < 8; ++frame) {
			uint32_t *pixels = mkwin_popup_framebuffer(popup);

			for(uint32_t i = 0; i < 64 * 48; ++i) {
				pixels[i] = frame & 1 ? 0xff123456 : 0xffabcdef;

			}

			mkwin_popup_present(popup);

		}

		mkwin_popup_close(popup);

	} else {
		fprintf(stderr, "FAIL: popup creation\n");
		++failures;
	}

	struct mkwin_window *platform = first->mkwin;
	Window native_window = platform->win;
	Pixmap target = XCreatePixmap(platform->dpy, native_window, (uint32_t)first->win_w, (uint32_t)first->win_h, platform->depth);
	platform->win = target;

	for(uint32_t frame = 0; frame < 8; ++frame) {
		snprintf(first->widgets[2 + frame % 5].label, sizeof(first->widgets[2].label), "server frame %u", frame);
		dirty_widget(first, 2 + frame % 5);

		if(frame == 0) {
			dirty_all(first);

		} else if(frame == 2 || frame == 3) {
			dirty_add(first, first->win_w / 2 - 8, 0, 16, first->win_h);

		}

		mkgui_frame_begin(first);
		render_widgets(first);
		flush_text(first);
		mkgui_frame_present(first);
		mkwin_sync(platform);
		buffering_check(first);
		XImage *image = XGetImage(platform->dpy, target, 0, 0, (uint32_t)first->win_w, (uint32_t)first->win_h, AllPlanes, ZPixmap);

		if(image) {
			uint32_t mismatches = 0;

			for(int32_t y = 0; y < first->win_h; ++y) {
				for(int32_t x = 0; x < first->win_w; ++x) {
					if((XGetPixel(image, x, y) & 0x00ffffffu) != (first->pixels[(size_t)y * (size_t)first->win_w + (size_t)x] & 0x00ffffffu)) {
						++mismatches;

					}
				}
			}

			++comparisons;

			if(mismatches) {
				fprintf(stderr, "FAIL: server frame %u has %u stale pixels\n", frame, mismatches);
				++failures;

			}

			XDestroyImage(image);

		} else {
			fprintf(stderr, "FAIL: server image readback\n");
			++failures;
		}

		first->dirty = 0;
		first->dirty_full = 0;
		first->dirty_count = 0;

	}

	platform->win = native_window;
	XFreePixmap(platform->dpy, target);
	mkwin_sync(first->mkwin);
	XSync(first->mkwin->dpy, True);
	dirty_all(first);
	dirty_all(second);
	mkgui_flush(first);
	mkgui_flush(second);

}

// [=]===^=[ main ]===============================================[=]
int main(void) {
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, 1, "buffering", "", 0, 320, 240, 0, MKGUI_WINDOW_HIDDEN | MKGUI_WINDOW_UNDECORATED, 0),
		MKGUI_W(MKGUI_VBOX, 2, "", "", 1, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, 3, "first", "", 2, 0, 28, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_BUTTON, 4, "second", "", 2, 0, 28, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_LABEL, 5, "label", "", 2, 0, 28, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_CHECKBOX, 6, "check", "", 2, 0, 28, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_BUTTON, 7, "third", "", 2, 0, 28, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();

	if(ctx) {
		struct mkgui_window *first = mkgui_window_create(ctx, NULL, widgets, sizeof(widgets) / sizeof(*widgets), NULL, 320, 240);

		if(first) {
			struct mkgui_window *second = mkgui_window_create(ctx, NULL, widgets, sizeof(widgets) / sizeof(*widgets), NULL, 320, 240);

			if(second) {
				buffering_exercise(first, second);
				mkgui_window_destroy(second);

			} else {
				fprintf(stderr, "FAIL: second window creation\n");
				++failures;
			}

			mkgui_window_destroy(first);

		} else {
			fprintf(stderr, "FAIL: first window creation\n");
			++failures;
		}

		mkgui_ctx_destroy(ctx);

	} else {
		fprintf(stderr, "FAIL: context creation\n");
		++failures;
	}

	fprintf(stderr, "buffering: %u framebuffer comparisons, %u failures\n", comparisons, failures);
	return failures ? 1 : 0;

}
