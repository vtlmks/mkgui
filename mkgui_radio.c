// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ circle_coverage ]=====================================[=]
static uint32_t circle_coverage(int32_t dx, int32_t dy, int32_t radius) {
	int32_t r8 = 8 * radius;
	int32_t r8sq = r8 * r8;
	int32_t base_dx8 = 8 * dx;
	int32_t base_dy8 = 8 * dy;
	uint32_t count = 0;
	for(uint32_t sy = 0; sy < 4; ++sy) {
		int32_t sdy = base_dy8 + 2 * (int32_t)sy + 1;
		int32_t remain = r8sq - sdy * sdy;
		if(remain < 0) {
			continue;
		}
		for(uint32_t sx = 0; sx < 4; ++sx) {
			int32_t sdx = base_dx8 + 2 * (int32_t)sx + 1;
			if(sdx * sdx <= remain) {
				++count;
			}
		}
	}
	return count;
}

// [=]===^=[ draw_aa_circle_fill ]=================================[=]
static void draw_aa_circle_fill(uint32_t *buf, int32_t bw, int32_t bh, int32_t cx, int32_t cy, int32_t radius, uint32_t color) {
	for(int32_t dy = -radius; dy <= radius; ++dy) {
		int32_t py = cy + dy;
		if(py < 0 || py >= bh) {
			continue;
		}
		for(int32_t dx = -radius; dx <= radius; ++dx) {
			int32_t px = cx + dx;
			if(px < 0 || px >= bw) {
				continue;
			}
			int32_t ri = radius - 1;
			if(dx * dx + dy * dy <= ri * ri) {
				buf[py * bw + px] = color;
			} else if(dx * dx + dy * dy <= (radius + 1) * (radius + 1)) {
				uint32_t cov = circle_coverage(dx, dy, radius);
				if(cov > 0) {
					uint8_t alpha = (uint8_t)(cov * 255 / 16);
					buf[py * bw + px] = blend_pixel(buf[py * bw + px], color, alpha);
				}
			}
		}
	}
}

// [=]===^=[ draw_aa_circle_ring ]=================================[=]
static void draw_aa_circle_ring(uint32_t *buf, int32_t bw, int32_t bh, int32_t cx, int32_t cy, int32_t outer_r, int32_t inner_r, uint32_t fill, uint32_t border) {
	for(int32_t dy = -outer_r; dy <= outer_r; ++dy) {
		int32_t py = cy + dy;
		if(py < 0 || py >= bh) {
			continue;
		}
		for(int32_t dx = -outer_r; dx <= outer_r; ++dx) {
			int32_t px = cx + dx;
			if(px < 0 || px >= bw) {
				continue;
			}
			uint32_t outer_cov = circle_coverage(dx, dy, outer_r);
			if(outer_cov == 0) {
				continue;
			}
			uint32_t inner_cov = circle_coverage(dx, dy, inner_r);
			uint32_t border_cov = outer_cov - inner_cov;
			uint32_t *dst = &buf[py * bw + px];
			if(inner_cov == 16) {
				*dst = fill;
			} else if(border_cov == 16) {
				*dst = border;
			} else {
				uint32_t bg = *dst;
				if(inner_cov > 0) {
					uint8_t fill_a = (uint8_t)(inner_cov * 255 / 16);
					bg = blend_pixel(bg, fill, fill_a);
				}
				if(border_cov > 0) {
					uint8_t border_a = (uint8_t)(border_cov * 255 / 16);
					bg = blend_pixel(bg, border, border_a);
				}
				*dst = bg;
			}
		}
	}
}

// [=]===^=[ render_radio ]======================================[=]
static void render_radio(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rh = ctx->rects[idx].h;

	int32_t cy = ry + rh / 2;
	int32_t cx = rx + 9;
	int32_t outer_r = 7;
	int32_t inner_r = 6;

	uint32_t disabled = (w->flags & MKGUI_DISABLED);
	uint32_t hovered = (!disabled && ctx->hover_id == w->id);
	uint32_t pressed = (!disabled && ctx->press_id == w->id);
	uint32_t border = (ctx->focus_id == w->id || hovered) ? ctx->theme.splitter : ctx->theme.widget_border;
	uint32_t fill = pressed ? ctx->theme.widget_press : ctx->theme.widget_bg;
	draw_aa_circle_ring(ctx->pixels, ctx->win_w, ctx->win_h, cx, cy, outer_r, inner_r, fill, border);

	if(w->flags & MKGUI_CHECKED) {
		uint32_t dot_color = disabled ? ctx->theme.widget_border : ctx->theme.splitter;
		draw_aa_circle_fill(ctx->pixels, ctx->win_w, ctx->win_h, cx, cy, 4, dot_color);
	}

	int32_t rw = ctx->rects[idx].w;
	int32_t ty = ry + (rh - ctx->font_height) / 2;
	uint32_t tc = disabled ? ctx->theme.text_disabled : ctx->theme.text;
	push_text_clip(rx + 22, ty, w->label, tc, rx, ry, rx + rw, ry + rh);
}

// [=]===^=[ handle_radio_click ]=================================[=]
static uint32_t handle_radio_click(struct mkgui_ctx *ctx, struct mkgui_event *ev, struct mkgui_widget *w) {
	for(uint32_t i = 0; i < ctx->widget_count; ++i) {
		struct mkgui_widget *rw = &ctx->widgets[i];
		if(rw->type == MKGUI_RADIO && rw->parent_id == w->parent_id) {
			rw->flags &= ~MKGUI_CHECKED;
		}
	}
	w->flags |= MKGUI_CHECKED;
	dirty_all(ctx);
	ev->type = MKGUI_EVENT_RADIO_CHANGED;
	ev->id = w->id;
	ev->value = 1;
	return 1;
}

// [=]===^=[ handle_radio_key ]===================================[=]
static uint32_t handle_radio_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks) {
	if(ks == MKGUI_KEY_SPACE || ks == MKGUI_KEY_RETURN) {
		struct mkgui_widget *w = find_widget(ctx, ctx->focus_id);
		if(w) {
			return handle_radio_click(ctx, ev, w);
		}
	}
	return 0;
}

// [=]===^=[ mkgui_radio_get ]====================================[=]
static uint32_t mkgui_radio_get(struct mkgui_ctx *ctx, uint32_t id) {
	struct mkgui_widget *w = find_widget(ctx, id);
	return (w && (w->flags & MKGUI_CHECKED)) ? 1 : 0;
}

// [=]===^=[ mkgui_radio_get_selected ]============================[=]
static uint32_t mkgui_radio_get_selected(struct mkgui_ctx *ctx, uint32_t group_parent_id) {
	for(uint32_t i = 0; i < ctx->widget_count; ++i) {
		struct mkgui_widget *w = &ctx->widgets[i];
		if(w->type == MKGUI_RADIO && w->parent_id == group_parent_id && (w->flags & MKGUI_CHECKED)) {
			return w->id;
		}
	}
	return 0;
}
