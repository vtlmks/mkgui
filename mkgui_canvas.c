// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ render_canvas ]======================================[=]
static void render_canvas(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	if(w->flags & MKGUI_PANEL_BORDER) {
		uint32_t bg = (w->flags & MKGUI_PANEL_SUNKEN) ? shade_color(ctx->theme.bg, -15) : ctx->theme.bg;
		draw_rounded_rect(ctx->pixels, ctx->win_w, ctx->win_h, rx, ry, rw, rh, bg, ctx->theme.widget_border, ctx->theme.corner_radius);
	}

	struct mkgui_canvas_data *cd = find_canvas_data(ctx, w->id);
	if(cd && cd->callback && rw > 0 && rh > 0) {
		int32_t old_x1 = render_clip_x1;
		int32_t old_y1 = render_clip_y1;
		int32_t old_x2 = render_clip_x2;
		int32_t old_y2 = render_clip_y2;
		render_clip_x1 = rx;
		render_clip_y1 = ry;
		render_clip_x2 = rx + rw;
		render_clip_y2 = ry + rh;
		cd->callback(ctx, w->id, ctx->pixels, rx, ry, rw, rh, cd->userdata);
		render_clip_x1 = old_x1;
		render_clip_y1 = old_y1;
		render_clip_x2 = old_x2;
		render_clip_y2 = old_y2;
	}
}

// [=]===^=[ mkgui_canvas_set_callback ]============================[=]
static void mkgui_canvas_set_callback(struct mkgui_ctx *ctx, uint32_t id, mkgui_canvas_cb callback, void *userdata) {
	struct mkgui_canvas_data *cd = find_canvas_data(ctx, id);
	if(cd) {
		cd->callback = callback;
		cd->userdata = userdata;
	}
}
