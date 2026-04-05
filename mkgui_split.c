// Copyright (c) 2026, Peter Fors
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

	uint32_t color = ctx->theme.widget_border;

	if(w->type == MKGUI_HSPLIT) {
		int32_t split_y = ry + (int32_t)(rh * ratio);
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx, split_y, rw, ctx->split_thick, color);

	} else {
		int32_t split_x = rx + (int32_t)(rw * ratio);
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, split_x, ry, ctx->split_thick, rh, color);
	}
}

// [=]===^=[ mkgui_split_get_ratio ]================================[=]
MKGUI_API float mkgui_split_get_ratio(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, 0.0f);
	struct mkgui_split_data *sd = find_split_data(ctx, id);
	return sd ? sd->ratio : 0.5f;
}

// [=]===^=[ mkgui_split_set_ratio ]================================[=]
MKGUI_API void mkgui_split_set_ratio(struct mkgui_ctx *ctx, uint32_t id, float ratio) {
	MKGUI_CHECK(ctx);
	struct mkgui_split_data *sd = find_split_data(ctx, id);
	if(!sd) {
		return;
	}
	if(ratio < 0.0f) {
		ratio = 0.0f;
	}
	if(ratio > 1.0f) {
		ratio = 1.0f;
	}
	sd->ratio = ratio;
	dirty_all(ctx);
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
		return (mx >= rx && mx < rx + rw && my >= split_y && my < split_y + ctx->split_thick);

	} else {
		int32_t split_x = rx + (int32_t)(rw * ratio);
		return (mx >= split_x && mx < split_x + ctx->split_thick && my >= ry && my < ry + rh);
	}
}
