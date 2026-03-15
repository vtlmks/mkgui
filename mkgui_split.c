// Copyright (c) 2026, mkgui contributors
// SPDX-License-Identifier: MIT

// [=]===^=[ render_split ]======================================[=]
static void render_split(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	struct mkgui_split_data *sd = find_split_data(ctx, w->id);
	float ratio = sd ? sd->ratio : 0.5f;

	uint32_t color = ctx->theme.splitter;
	if(ctx->hover_id == w->id || ctx->press_id == w->id) {
		color = ctx->theme.sel_text;
	}

	if(w->type == MKGUI_HSPLIT) {
		int32_t split_y = ry + (int32_t)(rh * ratio);
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx, split_y, rw, MKGUI_SPLIT_THICK, color);

	} else {
		int32_t split_x = rx + (int32_t)(rw * ratio);
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, split_x, ry, MKGUI_SPLIT_THICK, rh, color);
	}
}

// [=]===^=[ split_bar_hit ]=====================================[=]
static uint32_t split_bar_hit(struct mkgui_ctx *ctx, uint32_t idx, int32_t mx, int32_t my) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	struct mkgui_split_data *sd = find_split_data(ctx, w->id);
	float ratio = sd ? sd->ratio : 0.5f;

	if(w->type == MKGUI_HSPLIT) {
		int32_t split_y = ry + (int32_t)(rh * ratio);
		return (mx >= rx && mx < rx + rw && my >= split_y && my < split_y + MKGUI_SPLIT_THICK);

	} else {
		int32_t split_x = rx + (int32_t)(rw * ratio);
		return (mx >= split_x && mx < split_x + MKGUI_SPLIT_THICK && my >= ry && my < ry + rh);
	}
}
