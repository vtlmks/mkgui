// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ render_divider ]=====================================[=]
static void render_divider(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	uint32_t color = ctx->theme.widget_border;
	if(w->flags & MKGUI_VERTICAL) {
		int32_t cx = rx + rw / 2;
		draw_vline(ctx->pixels, ctx->win_w, ctx->win_h, cx, ry + sc(ctx, 2), rh - sc(ctx, 4), color);
	} else {
		int32_t cy = ry + rh / 2;
		draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, rx + sc(ctx, 2), cy, rw - sc(ctx, 4), color);
	}
}
