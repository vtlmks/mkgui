// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ draw_aa_line ]========================================[=]
static void draw_aa_line(uint32_t *buf, int32_t bw, int32_t bh, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color, int32_t thickness) {
	int32_t dx = x1 - x0;
	int32_t dy = y1 - y0;
	int32_t steps = dx > 0 ? dx : -dx;
	int32_t ady = dy > 0 ? dy : -dy;
	if(ady > steps) {
		steps = ady;
	}
	if(steps == 0) {
		return;
	}
	int32_t half = thickness * 4;
	for(int32_t i = 0; i <= steps * 8; ++i) {
		int32_t cx8 = x0 * 8 + dx * i / steps;
		int32_t cy8 = y0 * 8 + dy * i / steps;
		int32_t px_lo = (cx8 - half) >> 3;
		int32_t px_hi = (cx8 + half) >> 3;
		int32_t py_lo = (cy8 - half) >> 3;
		int32_t py_hi = (cy8 + half) >> 3;
		for(int32_t py = py_lo; py <= py_hi; ++py) {
			if(py < 0 || py >= bh) {
				continue;
			}
			for(int32_t px = px_lo; px <= px_hi; ++px) {
				if(px < 0 || px >= bw) {
					continue;
				}
				int32_t ddx = px * 8 + 4 - cx8;
				int32_t ddy = py * 8 + 4 - cy8;
				int32_t dist2 = ddx * ddx + ddy * ddy;
				int32_t r8 = half;
				if(dist2 >= r8 * r8) {
					continue;
				}
				int32_t ri = r8 - 8;
				if(ri < 0) {
					ri = 0;
				}
				uint32_t a;
				if(dist2 <= ri * ri) {
					a = 255;
				} else {
					int32_t edge = r8 * r8 - ri * ri;
					int32_t d = r8 * r8 - dist2;
					a = (uint32_t)(d * 255 / edge);
					if(a > 255) {
						a = 255;
					}
				}
				uint32_t *dst = &buf[py * bw + px];
				uint32_t existing_a = (*dst >> 24) & 0xff;
				if(a > existing_a || *dst == buf[py * bw + px]) {
					*dst = blend_pixel(*dst, color, (uint8_t)a);
				}
			}
		}
	}
}

// [=]===^=[ render_checkbox ]===================================[=]
static void render_checkbox(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rh = ctx->rects[idx].h;

	int32_t box_size = 16;
	int32_t by = ry + (rh - box_size) / 2;

	uint32_t disabled = (w->flags & MKGUI_DISABLED);
	uint32_t focused = (ctx->focus_id == w->id);
	uint32_t hovered = (!disabled && ctx->hover_id == w->id);
	uint32_t pressed = (!disabled && ctx->press_id == w->id);
	uint32_t bg = (w->flags & MKGUI_CHECKED) ? (disabled ? ctx->theme.widget_border : ctx->theme.splitter) : (pressed ? ctx->theme.widget_press : ctx->theme.input_bg);
	uint32_t border = (focused || hovered) ? ctx->theme.splitter : ctx->theme.widget_border;
	draw_patch(ctx, MKGUI_STYLE_SUNKEN, rx, by, box_size, box_size, bg, border);

	if(w->flags & MKGUI_CHECKED) {
		int32_t cx = rx + box_size / 2;
		int32_t cy = by + box_size / 2;
		uint32_t check_color = disabled ? ctx->theme.text_disabled : ctx->theme.sel_text;
		draw_aa_line(ctx->pixels, ctx->win_w, ctx->win_h, cx - 4, cy - 1, cx - 1, cy + 3, check_color, 2);
		draw_aa_line(ctx->pixels, ctx->win_w, ctx->win_h, cx - 1, cy + 3, cx + 5, cy - 4, check_color, 2);
	}

	int32_t rw = ctx->rects[idx].w;
	int32_t ty = ry + (rh - ctx->font_height) / 2;
	uint32_t tc = disabled ? ctx->theme.text_disabled : ctx->theme.text;
	push_text_clip(rx + box_size + 6, ty, w->label, tc, rx, ry, rx + rw, ry + rh);
}

// [=]===^=[ handle_checkbox_key ]===============================[=]
static uint32_t handle_checkbox_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks) {
	if(ks == MKGUI_KEY_SPACE) {
		struct mkgui_widget *w = find_widget(ctx, ctx->focus_id);
		if(!w) {
			return 0;
		}
		w->flags ^= MKGUI_CHECKED;
		ev->type = MKGUI_EVENT_CHECKBOX_CHANGED;
		ev->id = ctx->focus_id;
		ev->value = (w->flags & MKGUI_CHECKED) ? 1 : 0;
		dirty_all(ctx);
		return 1;
	}
	return 0;
}

// [=]===^=[ mkgui_checkbox_get ]================================[=]
static uint32_t mkgui_checkbox_get(struct mkgui_ctx *ctx, uint32_t id) {
	struct mkgui_widget *w = find_widget(ctx, id);
	if(!w) {
		return 0;
	}
	return (w->flags & MKGUI_CHECKED) ? 1 : 0;
}

// [=]===^=[ mkgui_checkbox_set ]================================[=]
static void mkgui_checkbox_set(struct mkgui_ctx *ctx, uint32_t id, uint32_t checked) {
	struct mkgui_widget *w = find_widget(ctx, id);
	if(!w) {
		return;
	}
	if(checked) {
		w->flags |= MKGUI_CHECKED;

	} else {
		w->flags &= ~MKGUI_CHECKED;
	}
	dirty_all(ctx);
}
