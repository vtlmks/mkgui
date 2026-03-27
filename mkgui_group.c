// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ render_group ]========================================[=]
static void render_group(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	int32_t group_pad = sc(ctx, 6);
	uint32_t collapsed = (w->style & MKGUI_COLLAPSED);
	int32_t label_y = ry + sc(ctx, 2);
	int32_t label_x = rx + group_pad + sc(ctx, 4);
	int32_t tw = text_width(ctx, w->label);
	int32_t gap_left = label_x - sc(ctx, 4);
	int32_t gap_right = label_x + tw + sc(ctx, 4);

	int32_t frame_y = ry + ctx->font_height / 2 + sc(ctx, 2);
	int32_t frame_h = collapsed ? (rh - (frame_y - ry)) : (rh - (frame_y - ry));
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

	if(!(w->style & MKGUI_COLLAPSIBLE)) {
		return;
	}

	uint32_t ac = ctx->theme.text;
	int32_t ax = rx + group_pad - sc(ctx, 4);
	int32_t ay = label_y + ctx->font_height / 2;
	if(collapsed) {
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ax, ay - sc(ctx, 3), sc(ctx, 1), sc(ctx, 1), ac);
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ax, ay - sc(ctx, 2), sc(ctx, 2), sc(ctx, 1), ac);
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ax, ay - sc(ctx, 1), sc(ctx, 3), sc(ctx, 1), ac);
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ax, ay, sc(ctx, 4), sc(ctx, 1), ac);
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ax, ay + sc(ctx, 1), sc(ctx, 3), sc(ctx, 1), ac);
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ax, ay + sc(ctx, 2), sc(ctx, 2), sc(ctx, 1), ac);
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ax, ay + sc(ctx, 3), sc(ctx, 1), sc(ctx, 1), ac);
	} else {
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ax - sc(ctx, 2), ay - sc(ctx, 1), sc(ctx, 7), sc(ctx, 1), ac);
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ax - sc(ctx, 1), ay, sc(ctx, 5), sc(ctx, 1), ac);
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ax, ay + sc(ctx, 1), sc(ctx, 3), sc(ctx, 1), ac);
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ax + sc(ctx, 1), ay + sc(ctx, 2), sc(ctx, 1), sc(ctx, 1), ac);
	}
}

// [=]===^=[ mkgui_group_set_collapsed ]============================[=]
MKGUI_API void mkgui_group_set_collapsed(struct mkgui_ctx *ctx, uint32_t id, uint32_t collapsed) {
	MKGUI_CHECK(ctx);
	struct mkgui_widget *w = find_widget(ctx, id);
	if(!w) {
		return;
	}
	if(collapsed) {
		w->style |= MKGUI_COLLAPSED;
	} else {
		w->style &= ~MKGUI_COLLAPSED;
	}
	dirty_all(ctx);
}

// [=]===^=[ mkgui_group_get_collapsed ]============================[=]
MKGUI_API uint32_t mkgui_group_get_collapsed(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_widget *w = find_widget(ctx, id);
	return (w && (w->style & MKGUI_COLLAPSED)) ? 1 : 0;
}
