// Copyright (c) 2026, mkgui contributors
// SPDX-License-Identifier: MIT

// [=]===^=[ render_progress ]===================================[=]
static void render_progress(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	draw_patch(ctx, MKGUI_STYLE_SUNKEN, rx, ry, rw, rh, ctx->theme.input_bg, ctx->theme.widget_border);

	struct mkgui_progress_data *pd = find_progress_data(ctx, w->id);
	if(pd && pd->max_val > 0) {
		int32_t fill_w = (int32_t)((int64_t)(rw - 2) * pd->value / pd->max_val);
		if(fill_w < 0) {
			fill_w = 0;
		}
		if(fill_w > rw - 2) {
			fill_w = rw - 2;
		}
		int32_t fill_r = ctx->theme.corner_radius > 1 ? ctx->theme.corner_radius - 1 : 0;
		draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx + 1, ry + 1, fill_w, rh - 2, ctx->theme.accent, fill_r);

		if(pd->value > 0 && pd->value < pd->max_val) {
			int32_t shimmer_w = fill_w / 4;
			if(shimmer_w < 16) {
				shimmer_w = 16;
			}
			double phase = ctx->anim_time * 0.4;
			phase -= (double)(int32_t)phase;
			int32_t sweep_range = fill_w + shimmer_w;
			int32_t shimmer_x = (int32_t)(phase * sweep_range) - shimmer_w;

			int32_t fx = rx + 1;
			int32_t fy = ry + 1;
			int32_t fh = rh - 2;
			uint32_t highlight = 0xffffffff;
			int32_t half = shimmer_w / 2;

			int32_t sx_min = shimmer_x;
			int32_t sx_max = shimmer_x + shimmer_w + fh;
			if(sx_min < 0) {
				sx_min = 0;
			}
			if(sx_max > fill_w) {
				sx_max = fill_w;
			}

			for(int32_t sy = 0; sy < fh; ++sy) {
				int32_t py = fy + sy;
				if(py < 0 || py >= ctx->win_h) {
					continue;
				}
				int32_t row_min = shimmer_x + sy;
				int32_t row_max = row_min + shimmer_w;
				if(row_min < sx_min) {
					row_min = sx_min;
				}
				if(row_max > sx_max) {
					row_max = sx_max;
				}
				for(int32_t sx = row_min; sx < row_max; ++sx) {
					int32_t px = fx + sx;
					if(px < 0 || px >= ctx->win_w) {
						continue;
					}
					int32_t dist = sx - sy - shimmer_x;
					int32_t from_center = dist - half;
					if(from_center < 0) {
						from_center = -from_center;
					}
					int32_t alpha = 30 * (half - from_center) / (half + 1);
					if(alpha > 0) {
						ctx->pixels[py * ctx->win_w + px] = blend_pixel(ctx->pixels[py * ctx->win_w + px], highlight, (uint8_t)alpha);
					}
				}
			}
		}

		char buf[64];
		int32_t pct = (int32_t)((int64_t)pd->value * 100 / pd->max_val);
		snprintf(buf, sizeof(buf), "%d%%", pct);
		int32_t tw = text_width(ctx, buf);
		int32_t tx = rx + (rw - tw) / 2;
		int32_t ty = ry + (rh - ctx->font_height) / 2;
		push_text_clip(tx, ty, buf, ctx->theme.sel_text, rx + 1, ry + 1, rx + rw - 1, ry + rh - 1);
	}
}

// [=]===^=[ mkgui_progress_setup ]===============================[=]
static void mkgui_progress_setup(struct mkgui_ctx *ctx, uint32_t id, int32_t max_val) {
	struct mkgui_progress_data *pd = find_progress_data(ctx, id);
	if(pd) {
		pd->max_val = max_val;
		pd->value = 0;
		dirty_all(ctx);
	}
}

// [=]===^=[ mkgui_progress_set ]=================================[=]
static void mkgui_progress_set(struct mkgui_ctx *ctx, uint32_t id, int32_t value) {
	struct mkgui_progress_data *pd = find_progress_data(ctx, id);
	if(pd) {
		pd->value = value;
		if(pd->value < 0) {
			pd->value = 0;
		}
		if(pd->value > pd->max_val) {
			pd->value = pd->max_val;
		}
		dirty_all(ctx);
	}
}

// [=]===^=[ mkgui_progress_get ]=================================[=]
static int32_t mkgui_progress_get(struct mkgui_ctx *ctx, uint32_t id) {
	struct mkgui_progress_data *pd = find_progress_data(ctx, id);
	return pd ? pd->value : 0;
}
