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
static int32_t toast_text_max_width(struct mkgui_ctx *ctx, int32_t toast_w, int32_t icon_w) {
	int32_t avail = toast_w - sc(ctx, MKGUI_TOAST_PAD_X) * 2;
	if(icon_w > 0) {
		avail -= icon_w + sc(ctx, MKGUI_TOAST_ICON_GAP);
	}
	return avail;
}

// [=]===^=[ toast_height ]============================================[=]
static int32_t toast_height(struct mkgui_ctx *ctx, int32_t icon_h) {
	int32_t content_h = ctx->font_height;
	if(icon_h > content_h) {
		content_h = icon_h;
	}
	return content_h + sc(ctx, MKGUI_TOAST_PAD_Y) * 2;
}

// [=]===^=[ render_toasts ]===========================================[=]
static void render_toasts(struct mkgui_ctx *ctx) {
	int32_t margin = sc(ctx, MKGUI_TOAST_MARGIN);
	int32_t gap = sc(ctx, MKGUI_TOAST_GAP);
	int32_t desired_w = sc(ctx, MKGUI_TOAST_WIDTH);
	int32_t max_w = ctx->win_w - margin * 2;
	int32_t toast_w = desired_w < max_w ? desired_w : max_w;
	if(toast_w < sc(ctx, 80)) {
		return;
	}

	int32_t y_cursor = ctx->win_h - margin;
	for(uint32_t i = 0; i < MKGUI_MAX_TOASTS; ++i) {
		struct mkgui_toast_slot *t = &ctx->toasts[i];
		if(!t->active) {
			continue;
		}
		struct notify_palette *pal = notify_palette_for(t->severity);
		int32_t icon_idx = icon_resolve(ctx, pal->icon_name);
		int32_t icon_w = icon_idx >= 0 ? icons[icon_idx].w : 0;
		int32_t icon_h = icon_idx >= 0 ? icons[icon_idx].h : 0;
		int32_t th = toast_height(ctx, icon_h);
		int32_t tx = ctx->win_w - margin - toast_w;
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

		int32_t radius = ctx->theme.corner_radius;
		draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, tx, ty, toast_w, th, pal->bg, radius);

		int32_t content_x = tx + sc(ctx, MKGUI_TOAST_PAD_X);
		int32_t content_y = ty + sc(ctx, MKGUI_TOAST_PAD_Y);
		if(icon_idx >= 0) {
			int32_t iy = content_y + (th - sc(ctx, MKGUI_TOAST_PAD_Y) * 2 - icon_h) / 2;
			draw_icon(ctx->pixels, ctx->win_w, ctx->win_h, &icons[icon_idx], content_x, iy, tx, ty, tx + toast_w, ty + th);
			content_x += icon_w + sc(ctx, MKGUI_TOAST_ICON_GAP);
		}

		int32_t text_max = toast_text_max_width(ctx, toast_w, icon_w);
		char display[MKGUI_MAX_TEXT];
		snprintf(display, sizeof(display), "%s", t->text);
		text_truncate(ctx, display, text_max);
		int32_t text_y = content_y + ((th - sc(ctx, MKGUI_TOAST_PAD_Y) * 2) - ctx->font_height) / 2;
		push_text_clip(content_x, text_y, display, pal->text, tx, ty, tx + toast_w, ty + th);

		y_cursor = ty - gap;
	}
}

// [=]===^=[ toasts_expire ]===========================================[=]
// Returns 1 if any toast was removed (caller should dirty_all).
static uint32_t toasts_expire(struct mkgui_ctx *ctx) {
	uint32_t now = mkgui_time_ms();
	uint32_t any = 0;
	for(uint32_t i = 0; i < MKGUI_MAX_TOASTS; ++i) {
		struct mkgui_toast_slot *t = &ctx->toasts[i];
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
static int32_t toasts_next_expiry_ms(struct mkgui_ctx *ctx) {
	uint32_t now = mkgui_time_ms();
	int32_t best = -1;
	for(uint32_t i = 0; i < MKGUI_MAX_TOASTS; ++i) {
		struct mkgui_toast_slot *t = &ctx->toasts[i];
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
static int32_t toasts_hit(struct mkgui_ctx *ctx, int32_t mx, int32_t my) {
	for(uint32_t i = 0; i < MKGUI_MAX_TOASTS; ++i) {
		struct mkgui_toast_slot *t = &ctx->toasts[i];
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
static void toast_dismiss_slot(struct mkgui_ctx *ctx, uint32_t slot) {
	struct mkgui_toast_slot *t = &ctx->toasts[slot];
	t->active = 0;
	t->text[0] = '\0';
	dirty_all(ctx);
}

// [=]===^=[ toast_alloc_slot ]========================================[=]
// Returns a free slot, or evicts the oldest (first) slot if all are full.
static uint32_t toast_alloc_slot(struct mkgui_ctx *ctx) {
	for(uint32_t i = 0; i < MKGUI_MAX_TOASTS; ++i) {
		if(!ctx->toasts[i].active) {
			return i;
		}
	}
	// All slots in use: shift everything down, freeing the last slot.
	// Oldest toast (slot 0) is dropped. This keeps newest-at-bottom order.
	for(uint32_t i = 0; i < MKGUI_MAX_TOASTS - 1; ++i) {
		ctx->toasts[i] = ctx->toasts[i + 1];
	}
	return MKGUI_MAX_TOASTS - 1;
}

// [=]===^=[ mkgui_toast ]=============================================[=]
MKGUI_API void mkgui_toast(struct mkgui_ctx *ctx, const char *text) {
	MKGUI_CHECK(ctx);
	if(!text) {
		return;
	}
	mkgui_toast_ex(ctx, MKGUI_SEVERITY_INFO, text, MKGUI_TOAST_DEFAULT_MS);
}

// [=]===^=[ mkgui_toast_ex ]==========================================[=]
MKGUI_API void mkgui_toast_ex(struct mkgui_ctx *ctx, uint32_t severity, const char *text, uint32_t duration_ms) {
	MKGUI_CHECK(ctx);
	if(!text) {
		return;
	}
	uint32_t slot = toast_alloc_slot(ctx);
	struct mkgui_toast_slot *t = &ctx->toasts[slot];
	t->active = 1;
	t->severity = severity < 4 ? severity : MKGUI_SEVERITY_INFO;
	t->expire_ms = duration_ms ? mkgui_time_ms() + duration_ms : 0;
	t->x = t->y = t->w = t->h = 0;
	snprintf(t->text, sizeof(t->text), "%s", text);
	// Pre-resolve the severity icon so the system-theme lazy-load happens
	// once here rather than on every render pass.
	icon_resolve(ctx, notify_palette_for(t->severity)->icon_name);
	dirty_all(ctx);
}

// [=]===^=[ mkgui_toast_clear ]=======================================[=]
MKGUI_API void mkgui_toast_clear(struct mkgui_ctx *ctx) {
	MKGUI_CHECK(ctx);
	uint32_t any = 0;
	for(uint32_t i = 0; i < MKGUI_MAX_TOASTS; ++i) {
		if(ctx->toasts[i].active) {
			ctx->toasts[i].active = 0;
			ctx->toasts[i].text[0] = '\0';
			any = 1;
		}
	}

	if(any) {
		dirty_all(ctx);
	}
}

// ---------------------------------------------------------------------------
// Banner
// ---------------------------------------------------------------------------

// [=]===^=[ banner_height ]===========================================[=]
static int32_t banner_height(struct mkgui_ctx *ctx, int32_t icon_h) {
	int32_t content_h = ctx->font_height;
	if(icon_h > content_h) {
		content_h = icon_h;
	}
	int32_t close_h = sc(ctx, MKGUI_BANNER_CLOSE_SIZE);
	if(close_h > content_h) {
		content_h = close_h;
	}
	return content_h + sc(ctx, MKGUI_BANNER_PAD_Y) * 2;
}

// [=]===^=[ render_banner ]===========================================[=]
static void render_banner(struct mkgui_ctx *ctx) {
	if(!ctx->banner.active) {
		ctx->banner.close_x = ctx->banner.close_y = 0;
		ctx->banner.close_w = ctx->banner.close_h = 0;
		return;
	}
	if(ctx->win_w <= 0 || ctx->win_h <= 0) {
		return;
	}

	struct notify_palette *pal = notify_palette_for(ctx->banner.severity);
	int32_t icon_idx = icon_resolve(ctx, pal->icon_name);
	int32_t icon_w = icon_idx >= 0 ? icons[icon_idx].w : 0;
	int32_t icon_h = icon_idx >= 0 ? icons[icon_idx].h : 0;
	int32_t bh = banner_height(ctx, icon_h);
	int32_t bx = 0;
	int32_t by = 0;
	int32_t bw = ctx->win_w;

	draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, bx, by, bw, bh, pal->bg);

	int32_t pad_x = sc(ctx, MKGUI_BANNER_PAD_X);
	int32_t pad_y = sc(ctx, MKGUI_BANNER_PAD_Y);
	int32_t content_x = bx + pad_x;
	int32_t close_size = sc(ctx, MKGUI_BANNER_CLOSE_SIZE);
	int32_t cx_x = bx + bw - pad_x - close_size;
	int32_t cx_y = by + (bh - close_size) / 2;
	ctx->banner.close_x = cx_x;
	ctx->banner.close_y = cx_y;
	ctx->banner.close_w = close_size;
	ctx->banner.close_h = close_size;

	if(icon_idx >= 0) {
		int32_t iy = by + (bh - icon_h) / 2;
		draw_icon(ctx->pixels, ctx->win_w, ctx->win_h, &icons[icon_idx], content_x, iy, bx, by, bx + bw, by + bh);
		content_x += icon_w + sc(ctx, MKGUI_TOAST_ICON_GAP);
	}

	int32_t text_max = cx_x - content_x - pad_x;
	if(text_max > 0) {
		char display[MKGUI_MAX_TEXT];
		snprintf(display, sizeof(display), "%s", ctx->banner.text);
		text_truncate(ctx, display, text_max);
		int32_t text_y = by + pad_y + ((bh - pad_y * 2) - ctx->font_height) / 2;
		push_text_clip(content_x, text_y, display, pal->text, bx, by, cx_x, by + bh);
	}

	// Close button: a pair of crossed 2px lines inside close_size.
	int32_t inset = sc(ctx, 4);
	int32_t x0 = cx_x + inset;
	int32_t y0 = cx_y + inset;
	int32_t x1 = cx_x + close_size - inset - 1;
	int32_t y1 = cx_y + close_size - inset - 1;
	uint32_t line = pal->text;
	for(int32_t step = 0; step <= x1 - x0; ++step) {
		int32_t px = x0 + step;
		int32_t py = y0 + step;
		if(px >= 0 && px < ctx->win_w && py >= 0 && py < ctx->win_h) {
			ctx->pixels[py * ctx->win_w + px] = line;
		}

		if(px + 1 < ctx->win_w && py >= 0 && py < ctx->win_h) {
			ctx->pixels[py * ctx->win_w + (px + 1)] = line;
		}
	}
	for(int32_t step = 0; step <= x1 - x0; ++step) {
		int32_t px = x0 + step;
		int32_t py = y1 - step;
		if(px >= 0 && px < ctx->win_w && py >= 0 && py < ctx->win_h) {
			ctx->pixels[py * ctx->win_w + px] = line;
		}

		if(px + 1 < ctx->win_w && py >= 0 && py < ctx->win_h) {
			ctx->pixels[py * ctx->win_w + (px + 1)] = line;
		}
	}
}

// [=]===^=[ banner_close_hit ]========================================[=]
static uint32_t banner_close_hit(struct mkgui_ctx *ctx, int32_t mx, int32_t my) {
	if(!ctx->banner.active || ctx->banner.close_w == 0) {
		return 0;
	}
	return mx >= ctx->banner.close_x && mx < ctx->banner.close_x + ctx->banner.close_w &&
	       my >= ctx->banner.close_y && my < ctx->banner.close_y + ctx->banner.close_h;
}

// [=]===^=[ mkgui_banner_set ]========================================[=]
MKGUI_API void mkgui_banner_set(struct mkgui_ctx *ctx, uint32_t severity, const char *text) {
	MKGUI_CHECK(ctx);
	if(!text) {
		return;
	}
	ctx->banner.active = 1;
	ctx->banner.severity = severity < 4 ? severity : MKGUI_SEVERITY_INFO;
	snprintf(ctx->banner.text, sizeof(ctx->banner.text), "%s", text);
	icon_resolve(ctx, notify_palette_for(ctx->banner.severity)->icon_name);
	dirty_all(ctx);
}

// [=]===^=[ mkgui_banner_clear ]======================================[=]
MKGUI_API void mkgui_banner_clear(struct mkgui_ctx *ctx) {
	MKGUI_CHECK(ctx);
	if(!ctx->banner.active) {
		return;
	}
	ctx->banner.active = 0;
	ctx->banner.text[0] = '\0';
	dirty_all(ctx);
}

// [=]===^=[ mkgui_banner_active ]=====================================[=]
MKGUI_API uint32_t mkgui_banner_active(struct mkgui_ctx *ctx) {
	MKGUI_CHECK_VAL(ctx, 0);
	return ctx->banner.active;
}

// ---------------------------------------------------------------------------
// Shared render hook
// ---------------------------------------------------------------------------

// [=]===^=[ render_notify ]===========================================[=]
// Called from mkgui_flush after widget rendering. Banner first (underneath
// any overlapping toasts), then toasts (always on top).
static void render_notify(struct mkgui_ctx *ctx) {
	render_banner(ctx);
	render_toasts(ctx);
}

// [=]===^=[ notify_handle_click ]=====================================[=]
// Returns 1 if the click was handled (a toast or banner close button),
// 0 otherwise.
static uint32_t notify_handle_click(struct mkgui_ctx *ctx, int32_t mx, int32_t my) {
	if(banner_close_hit(ctx, mx, my)) {
		mkgui_banner_clear(ctx);
		return 1;
	}
	int32_t slot = toasts_hit(ctx, mx, my);
	if(slot >= 0) {
		toast_dismiss_slot(ctx, (uint32_t)slot);
		return 1;
	}
	return 0;
}
