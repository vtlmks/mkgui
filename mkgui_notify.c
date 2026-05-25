// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define MKGUI_TOAST_DEFAULT_MS   3000
#define MKGUI_TOAST_MARGIN       12
#define MKGUI_TOAST_GAP          8
#define MKGUI_TOAST_PAD_X        12
#define MKGUI_TOAST_PAD_Y        8
#define MKGUI_TOAST_WIDTH        320
#define MKGUI_TOAST_ICON_GAP     8
#define MKGUI_BANNER_PAD_X       12
#define MKGUI_BANNER_PAD_Y       6
#define MKGUI_BANNER_CLOSE_SIZE  18

struct notify_palette {
	uint32_t bg;
	uint32_t text;
	char *icon_name;
};

// Saturated severity palette. These are the same info/warning/error accent
// colors used by the dialog windows (mkgui_dialogs.c); success is green.
// White text reads cleanly on all four.
static struct notify_palette notify_pal[4] = {
	{ 0x3daee9, 0xffffff, "dialog-information" },  // INFO
	{ 0x27ae60, 0xffffff, "dialog-ok"          },  // SUCCESS
	{ 0xf67400, 0xffffff, "dialog-warning"     },  // WARNING
	{ 0xda4453, 0xffffff, "error"              },  // ERROR
};

// [=]===^=[ notify_palette_for ]======================================[=]
static struct notify_palette *notify_palette_for(uint32_t severity) {
	uint32_t s = severity < 4 ? severity : 0;
	return &notify_pal[s];
}

// ---------------------------------------------------------------------------
// Toast
// ---------------------------------------------------------------------------

// [=]===^=[ toast_text_max_width ]====================================[=]
static int32_t toast_text_max_width(struct mkgui_window *win, int32_t toast_w, int32_t icon_w) {
	int32_t avail = toast_w - sc(win, MKGUI_TOAST_PAD_X) * 2;
	if(icon_w > 0) {
		avail -= icon_w + sc(win, MKGUI_TOAST_ICON_GAP);
	}
	return avail;
}

// [=]===^=[ toast_height ]============================================[=]
static int32_t toast_height(struct mkgui_window *win, int32_t icon_h) {
	int32_t content_h = win->font_height;
	if(icon_h > content_h) {
		content_h = icon_h;
	}
	return content_h + sc(win, MKGUI_TOAST_PAD_Y) * 2;
}

// [=]===^=[ render_toasts ]===========================================[=]
static void render_toasts(struct mkgui_window *win) {
	int32_t margin = sc(win, MKGUI_TOAST_MARGIN);
	int32_t gap = sc(win, MKGUI_TOAST_GAP);
	int32_t desired_w = sc(win, MKGUI_TOAST_WIDTH);
	int32_t max_w = win->win_w - margin * 2;
	int32_t toast_w = desired_w < max_w ? desired_w : max_w;
	if(toast_w < sc(win, 80)) {
		return;
	}

	int32_t y_cursor = win->win_h - margin;
	for(uint32_t i = 0; i < MKGUI_MAX_TOASTS; ++i) {
		struct mkgui_toast_slot *t = &win->toasts[i];
		if(!t->active) {
			continue;
		}
		struct notify_palette *pal = notify_palette_for(t->severity);
		int32_t icon_idx = icon_resolve(win, pal->icon_name);
		int32_t icon_w = icon_idx >= 0 ? icons[icon_idx].w : 0;
		int32_t icon_h = icon_idx >= 0 ? icons[icon_idx].h : 0;
		int32_t th = toast_height(win, icon_h);
		int32_t tx = win->win_w - margin - toast_w;
		int32_t ty = y_cursor - th;
		if(ty < margin) {
			// ran out of vertical space; stop stacking
			t->x = t->y = t->w = t->h = 0;
			continue;
		}

		t->x = tx;
		t->y = ty;
		t->w = toast_w;
		t->h = th;

		int32_t radius = win->theme.corner_radius;
		draw_rounded_rect_fill(win->pixels, win->win_w, win->win_h, tx, ty, toast_w, th, pal->bg, radius);

		int32_t content_x = tx + sc(win, MKGUI_TOAST_PAD_X);
		int32_t content_y = ty + sc(win, MKGUI_TOAST_PAD_Y);
		if(icon_idx >= 0) {
			int32_t iy = content_y + (th - sc(win, MKGUI_TOAST_PAD_Y) * 2 - icon_h) / 2;
			draw_icon(win->pixels, win->win_w, win->win_h, &icons[icon_idx], content_x, iy, tx, ty, tx + toast_w, ty + th);
			content_x += icon_w + sc(win, MKGUI_TOAST_ICON_GAP);
		}

		int32_t text_max = toast_text_max_width(win, toast_w, icon_w);
		char display[MKGUI_MAX_TEXT];
		snprintf(display, sizeof(display), "%s", t->text);
		text_truncate(win, display, text_max);
		int32_t text_y = content_y + ((th - sc(win, MKGUI_TOAST_PAD_Y) * 2) - win->font_height) / 2;
		push_text_clip(content_x, text_y, display, pal->text, tx, ty, tx + toast_w, ty + th);

		y_cursor = ty - gap;
	}
}

// [=]===^=[ toasts_expire ]===========================================[=]
// Returns 1 if any toast was removed (caller should dirty_all).
static uint32_t toasts_expire(struct mkgui_window *win) {
	uint32_t now = (uint32_t)(mkgui_now_ns() / 1000000ull);
	uint32_t any = 0;
	for(uint32_t i = 0; i < MKGUI_MAX_TOASTS; ++i) {
		struct mkgui_toast_slot *t = &win->toasts[i];
		if(!t->active || t->expire_ms == 0) {
			continue;
		}

		if((int32_t)(now - t->expire_ms) >= 0) {
			t->active = 0;
			t->text[0] = '\0';
			any = 1;
		}
	}
	return any;
}

// [=]===^=[ toasts_next_expiry_ms ]===================================[=]
// Returns milliseconds until the nearest toast expiry, or -1 if none.
static int32_t toasts_next_expiry_ms(struct mkgui_window *win) {
	uint32_t now = (uint32_t)(mkgui_now_ns() / 1000000ull);
	int32_t best = -1;
	for(uint32_t i = 0; i < MKGUI_MAX_TOASTS; ++i) {
		struct mkgui_toast_slot *t = &win->toasts[i];
		if(!t->active || t->expire_ms == 0) {
			continue;
		}
		int32_t remaining = (int32_t)(t->expire_ms - now);
		if(remaining < 0) {
			remaining = 0;
		}

		if(best < 0 || remaining < best) {
			best = remaining;
		}
	}
	return best;
}

// [=]===^=[ toasts_hit ]==============================================[=]
// Returns slot index (0..MKGUI_MAX_TOASTS-1) if the point hits an active toast,
// -1 otherwise.
static int32_t toasts_hit(struct mkgui_window *win, int32_t mx, int32_t my) {
	for(uint32_t i = 0; i < MKGUI_MAX_TOASTS; ++i) {
		struct mkgui_toast_slot *t = &win->toasts[i];
		if(!t->active || t->w == 0) {
			continue;
		}

		if(mx >= t->x && mx < t->x + t->w && my >= t->y && my < t->y + t->h) {
			return (int32_t)i;
		}
	}
	return -1;
}

// [=]===^=[ toast_dismiss_slot ]======================================[=]
static void toast_dismiss_slot(struct mkgui_window *win, uint32_t slot) {
	struct mkgui_toast_slot *t = &win->toasts[slot];
	t->active = 0;
	t->text[0] = '\0';
	dirty_all(win);
}

// [=]===^=[ toast_alloc_slot ]========================================[=]
// Returns a free slot, or evicts the oldest (first) slot if all are full.
static uint32_t toast_alloc_slot(struct mkgui_window *win) {
	for(uint32_t i = 0; i < MKGUI_MAX_TOASTS; ++i) {
		if(!win->toasts[i].active) {
			return i;
		}
	}
	// All slots in use: shift everything down, freeing the last slot.
	// Oldest toast (slot 0) is dropped. This keeps newest-at-bottom order.
	for(uint32_t i = 0; i < MKGUI_MAX_TOASTS - 1; ++i) {
		win->toasts[i] = win->toasts[i + 1];
	}
	return MKGUI_MAX_TOASTS - 1;
}

// [=]===^=[ mkgui_toast ]=============================================[=]
MKGUI_API void mkgui_toast(struct mkgui_window *win, const char *text) {
	MKGUI_CHECK(win);
	if(!text) {
		return;
	}
	mkgui_toast_ex(win, MKGUI_SEVERITY_INFO, text, MKGUI_TOAST_DEFAULT_MS);
}

// [=]===^=[ mkgui_toast_ex ]==========================================[=]
MKGUI_API void mkgui_toast_ex(struct mkgui_window *win, uint32_t severity, const char *text, uint32_t duration_ms) {
	MKGUI_CHECK(win);
	if(!text) {
		return;
	}
	uint32_t slot = toast_alloc_slot(win);
	struct mkgui_toast_slot *t = &win->toasts[slot];
	t->active = 1;
	t->severity = severity < 4 ? severity : MKGUI_SEVERITY_INFO;
	t->expire_ms = duration_ms ? (uint32_t)(mkgui_now_ns() / 1000000ull) + duration_ms : 0;
	t->x = t->y = t->w = t->h = 0;
	snprintf(t->text, sizeof(t->text), "%s", text);
	// Pre-resolve the severity icon so the system-theme lazy-load happens
	// once here rather than on every render pass.
	icon_resolve(win, notify_palette_for(t->severity)->icon_name);
	dirty_all(win);
}

// [=]===^=[ mkgui_toast_clear ]=======================================[=]
MKGUI_API void mkgui_toast_clear(struct mkgui_window *win) {
	MKGUI_CHECK(win);
	uint32_t any = 0;
	for(uint32_t i = 0; i < MKGUI_MAX_TOASTS; ++i) {
		if(win->toasts[i].active) {
			win->toasts[i].active = 0;
			win->toasts[i].text[0] = '\0';
			any = 1;
		}
	}

	if(any) {
		dirty_all(win);
	}
}

// ---------------------------------------------------------------------------
// Banner
// ---------------------------------------------------------------------------

// [=]===^=[ banner_height ]===========================================[=]
static int32_t banner_height(struct mkgui_window *win, int32_t icon_h) {
	int32_t content_h = win->font_height;
	if(icon_h > content_h) {
		content_h = icon_h;
	}
	int32_t close_h = sc(win, MKGUI_BANNER_CLOSE_SIZE);
	if(close_h > content_h) {
		content_h = close_h;
	}
	return content_h + sc(win, MKGUI_BANNER_PAD_Y) * 2;
}

// [=]===^=[ render_banner ]===========================================[=]
static void render_banner(struct mkgui_window *win) {
	if(!win->banner.active) {
		win->banner.close_x = win->banner.close_y = 0;
		win->banner.close_w = win->banner.close_h = 0;
		return;
	}
	if(win->win_w <= 0 || win->win_h <= 0) {
		return;
	}

	struct notify_palette *pal = notify_palette_for(win->banner.severity);
	int32_t icon_idx = icon_resolve(win, pal->icon_name);
	int32_t icon_w = icon_idx >= 0 ? icons[icon_idx].w : 0;
	int32_t icon_h = icon_idx >= 0 ? icons[icon_idx].h : 0;
	int32_t bh = banner_height(win, icon_h);
	int32_t bx = 0;
	int32_t by = 0;
	int32_t bw = win->win_w;

	draw_rect_fill(win->pixels, win->win_w, win->win_h, bx, by, bw, bh, pal->bg);

	int32_t pad_x = sc(win, MKGUI_BANNER_PAD_X);
	int32_t pad_y = sc(win, MKGUI_BANNER_PAD_Y);
	int32_t content_x = bx + pad_x;
	int32_t close_size = sc(win, MKGUI_BANNER_CLOSE_SIZE);
	int32_t cx_x = bx + bw - pad_x - close_size;
	int32_t cx_y = by + (bh - close_size) / 2;
	win->banner.close_x = cx_x;
	win->banner.close_y = cx_y;
	win->banner.close_w = close_size;
	win->banner.close_h = close_size;

	if(icon_idx >= 0) {
		int32_t iy = by + (bh - icon_h) / 2;
		draw_icon(win->pixels, win->win_w, win->win_h, &icons[icon_idx], content_x, iy, bx, by, bx + bw, by + bh);
		content_x += icon_w + sc(win, MKGUI_TOAST_ICON_GAP);
	}

	int32_t text_max = cx_x - content_x - pad_x;
	if(text_max > 0) {
		char display[MKGUI_MAX_TEXT];
		snprintf(display, sizeof(display), "%s", win->banner.text);
		text_truncate(win, display, text_max);
		int32_t text_y = by + pad_y + ((bh - pad_y * 2) - win->font_height) / 2;
		push_text_clip(content_x, text_y, display, pal->text, bx, by, cx_x, by + bh);
	}

	// Close button: a pair of crossed 2px lines inside close_size.
	int32_t inset = sc(win, 4);
	int32_t x0 = cx_x + inset;
	int32_t y0 = cx_y + inset;
	int32_t x1 = cx_x + close_size - inset - 1;
	int32_t y1 = cx_y + close_size - inset - 1;
	uint32_t line = pal->text;
	for(int32_t step = 0; step <= x1 - x0; ++step) {
		int32_t px = x0 + step;
		int32_t py = y0 + step;
		if(px >= 0 && px < win->win_w && py >= 0 && py < win->win_h) {
			win->pixels[py * win->win_w + px] = line;
		}

		if(px + 1 < win->win_w && py >= 0 && py < win->win_h) {
			win->pixels[py * win->win_w + (px + 1)] = line;
		}
	}
	for(int32_t step = 0; step <= x1 - x0; ++step) {
		int32_t px = x0 + step;
		int32_t py = y1 - step;
		if(px >= 0 && px < win->win_w && py >= 0 && py < win->win_h) {
			win->pixels[py * win->win_w + px] = line;
		}

		if(px + 1 < win->win_w && py >= 0 && py < win->win_h) {
			win->pixels[py * win->win_w + (px + 1)] = line;
		}
	}
}

// [=]===^=[ banner_close_hit ]========================================[=]
static uint32_t banner_close_hit(struct mkgui_window *win, int32_t mx, int32_t my) {
	if(!win->banner.active || win->banner.close_w == 0) {
		return 0;
	}
	return mx >= win->banner.close_x && mx < win->banner.close_x + win->banner.close_w &&
	       my >= win->banner.close_y && my < win->banner.close_y + win->banner.close_h;
}

// [=]===^=[ mkgui_banner_set ]========================================[=]
MKGUI_API void mkgui_banner_set(struct mkgui_window *win, uint32_t severity, const char *text) {
	MKGUI_CHECK(win);
	if(!text) {
		return;
	}
	win->banner.active = 1;
	win->banner.severity = severity < 4 ? severity : MKGUI_SEVERITY_INFO;
	snprintf(win->banner.text, sizeof(win->banner.text), "%s", text);
	icon_resolve(win, notify_palette_for(win->banner.severity)->icon_name);
	dirty_all(win);
}

// [=]===^=[ mkgui_banner_clear ]======================================[=]
MKGUI_API void mkgui_banner_clear(struct mkgui_window *win) {
	MKGUI_CHECK(win);
	if(!win->banner.active) {
		return;
	}
	win->banner.active = 0;
	win->banner.text[0] = '\0';
	dirty_all(win);
}

// [=]===^=[ mkgui_banner_active ]=====================================[=]
MKGUI_API uint32_t mkgui_banner_active(struct mkgui_window *win) {
	MKGUI_CHECK_VAL(win, 0);
	return win->banner.active;
}

// ---------------------------------------------------------------------------
// Shared render hook
// ---------------------------------------------------------------------------

// [=]===^=[ render_notify ]===========================================[=]
// Called from mkgui_flush after widget rendering. Banner first (underneath
// any overlapping toasts), then toasts (always on top).
static void render_notify(struct mkgui_window *win) {
	render_banner(win);
	render_toasts(win);
}

// [=]===^=[ notify_handle_click ]=====================================[=]
// Returns 1 if the click was handled (a toast or banner close button),
// 0 otherwise.
static uint32_t notify_handle_click(struct mkgui_window *win, int32_t mx, int32_t my) {
	if(banner_close_hit(win, mx, my)) {
		mkgui_banner_clear(win);
		return 1;
	}
	int32_t slot = toasts_hit(win, mx, my);
	if(slot >= 0) {
		toast_dismiss_slot(win, (uint32_t)slot);
		return 1;
	}
	return 0;
}
