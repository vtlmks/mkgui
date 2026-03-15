// Copyright (c) 2026, mkgui contributors
// SPDX-License-Identifier: MIT

#define MKGUI_GROUP_PAD 6

// [=]===^=[ render_group ]========================================[=]
static void render_group(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	int32_t label_y = ry + 2;
	int32_t label_x = rx + MKGUI_GROUP_PAD + 4;
	int32_t tw = text_width(ctx, w->label);
	int32_t gap_left = label_x - 4;
	int32_t gap_right = label_x + tw + 4;

	int32_t frame_y = ry + ctx->font_height / 2 + 2;
	int32_t frame_h = rh - (frame_y - ry);
	uint32_t bc = ctx->theme.widget_border;
	int32_t r = ctx->theme.corner_radius;

	uint32_t group_bg = shade_color(ctx->theme.bg, -10);
	draw_rounded_rect(ctx->pixels, ctx->win_w, ctx->win_h, rx, frame_y, rw, frame_h, group_bg, bc, r);

	if(gap_left > rx && gap_right <= rx + rw) {
		int32_t clear_y = frame_y;
		if(clear_y >= 0 && clear_y < ctx->win_h) {
			int32_t left = gap_left;
			int32_t right = gap_right;
			if(left < 0) {
				left = 0;
			}
			if(right > ctx->win_w) {
				right = ctx->win_w;
			}
			uint32_t *row = &ctx->pixels[clear_y * ctx->win_w];
			for(int32_t x = left; x < right; ++x) {
				row[x] = group_bg;
			}
		}
	}

	push_text_clip(label_x, label_y, w->label, ctx->theme.text, rx, ry, rx + rw, ry + rh);
}
