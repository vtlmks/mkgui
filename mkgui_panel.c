// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ render_panel ]======================================[=]
static void render_panel(struct mkgui_ctx *ctx, uint32_t idx) {
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	struct mkgui_widget *w = &ctx->widgets[idx];
	uint32_t bg = (w->style & MKGUI_PANEL_SUNKEN) ? shade_color(ctx->theme.bg, -15) : ctx->theme.bg;
	uint32_t bc = (w->style & MKGUI_PANEL_BORDER) ? ctx->theme.widget_border : 0;

	if(bc) {
		draw_rounded_rect(ctx->pixels, ctx->win_w, ctx->win_h, rx, ry, rw, rh, bg, bc, ctx->theme.corner_radius);
	} else {
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx, ry, rw, rh, bg);
	}
}
