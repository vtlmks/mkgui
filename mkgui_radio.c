// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ circle_coverage ]=====================================[=]
static uint32_t circle_coverage(int32_t dx, int32_t dy, int32_t radius) {
	int32_t r8 = 8 * radius;
	int32_t r8sq = r8 * r8;
	int32_t base_dx8 = 8 * dx;
	int32_t base_dy8 = 8 * dy;
	uint32_t count = 0;
#if defined(__SSE2__)
	__m128i dx_vals = _mm_set_epi32(base_dx8 + 7, base_dx8 + 5, base_dx8 + 3, base_dx8 + 1);
	for(uint32_t sy = 0; sy < 4; ++sy) {
		int32_t sdy = base_dy8 + 2 * (int32_t)sy + 1;
		int32_t remain = r8sq - sdy * sdy;
		if(remain < 0) {
			continue;
		}
		__m128i rem = _mm_set1_epi32(remain);
		__m128i dx_lo = _mm_and_si128(dx_vals, _mm_set1_epi32(0x0000ffff));
		__m128i dx2 = _mm_madd_epi16(dx_lo, dx_lo);
		__m128i cmp = _mm_or_si128(_mm_cmplt_epi32(dx2, rem), _mm_cmpeq_epi32(dx2, rem));
		count += (uint32_t)__builtin_popcount((uint32_t)_mm_movemask_epi8(cmp)) / 4;
	}
#else
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
#endif
	return count;
}

// [=]===^=[ draw_aa_circle_fill ]=================================[=]
static void draw_aa_circle_fill(uint32_t *buf, int32_t bw, int32_t bh, int32_t cx, int32_t cy, int32_t radius, uint32_t color) {
	int32_t dy0 = -radius;
	int32_t dy1 = radius;
	int32_t dx0 = -radius;
	int32_t dx1 = radius;
	if(cy + dy0 < 0) {
		dy0 = -cy;
	}

	if(cy + dy1 >= bh) {
		dy1 = bh - 1 - cy;
	}

	if(cx + dx0 < 0) {
		dx0 = -cx;
	}

	if(cx + dx1 >= bw) {
		dx1 = bw - 1 - cx;
	}
	for(int32_t dy = dy0; dy <= dy1; ++dy) {
		int32_t py = cy + dy;
		for(int32_t dx = dx0; dx <= dx1; ++dx) {
			int32_t px = cx + dx;
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
	int32_t dy0 = -outer_r;
	int32_t dy1 = outer_r;
	int32_t dx0 = -outer_r;
	int32_t dx1 = outer_r;
	if(cy + dy0 < 0) {
		dy0 = -cy;
	}

	if(cy + dy1 >= bh) {
		dy1 = bh - 1 - cy;
	}

	if(cx + dx0 < 0) {
		dx0 = -cx;
	}

	if(cx + dx1 >= bw) {
		dx1 = bw - 1 - cx;
	}
	for(int32_t dy = dy0; dy <= dy1; ++dy) {
		int32_t py = cy + dy;
		for(int32_t dx = dx0; dx <= dx1; ++dx) {
			int32_t px = cx + dx;
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
static void render_radio(struct mkgui_window *win, uint32_t idx) {
	struct mkgui_widget *w = &win->widgets[idx];
	int32_t rx = win->rects[idx].x;
	int32_t ry = win->rects[idx].y;
	int32_t rh = win->rects[idx].h;

	int32_t cy = ry + rh / 2;
	int32_t cx = rx + sc(win, 9);
	int32_t outer_r = sc(win, 7);
	int32_t inner_r = sc(win, 6);

	uint32_t disabled = (w->flags & MKGUI_DISABLED);
	uint32_t hovered = (!disabled && win->hover_id == w->id);
	uint32_t pressed = (!disabled && win->press_id == w->id);
	uint32_t border = (win->focus_id == w->id || hovered) ? win->theme.highlight : win->theme.widget_border;
	uint32_t fill = pressed ? win->theme.widget_press : win->theme.widget_bg;
	border = disabled_blend(border, win->theme.bg, disabled);
	fill = disabled_blend(fill, win->theme.bg, disabled);
	draw_aa_circle_ring(win->pixels, win->win_w, win->win_h, cx, cy, outer_r, inner_r, fill, border);

	if(w->style & MKGUI_RADIO_CHECKED) {
		uint32_t dot_color = disabled_blend(win->theme.highlight, win->theme.bg, disabled);
		draw_aa_circle_fill(win->pixels, win->win_w, win->win_h, cx, cy, sc(win, 4), dot_color);
	}

	int32_t rw = win->rects[idx].w;
	int32_t ty = ry + (rh - win->font_height) / 2;
	uint32_t tc = disabled ? win->theme.text_disabled : win->theme.text;
	push_text_clip(rx + sc(win, 22), ty, w->label, tc, rx, ry, rx + rw, ry + rh);
}

// [=]===^=[ handle_radio_click ]=================================[=]
static uint32_t handle_radio_click(struct mkgui_window *win, struct mkgui_event *ev, struct mkgui_widget *w) {
	for(uint32_t i = 0; i < win->widget_count; ++i) {
		struct mkgui_widget *rw = &win->widgets[i];
		if(rw->type == MKGUI_RADIO && rw->parent_id == w->parent_id) {
			rw->style &= ~MKGUI_RADIO_CHECKED;
		}
	}
	w->style |= MKGUI_RADIO_CHECKED;
	dirty_all(win);
	ev->type = MKGUI_EVENT_RADIO_CHANGED;
	ev->id = w->id;
	ev->value = 1;
	return 1;
}

// [=]===^=[ handle_radio_key ]===================================[=]
static uint32_t handle_radio_key(struct mkgui_window *win, struct mkgui_event *ev, uint32_t ks) {
	if(ks == MKGUI_KEY_SPACE || ks == MKGUI_KEY_RETURN) {
		struct mkgui_widget *w = find_widget(win, win->focus_id);
		if(w) {
			return handle_radio_click(win, ev, w);
		}
	}
	return 0;
}

// [=]===^=[ mkgui_radio_get ]====================================[=]
MKGUI_API uint32_t mkgui_radio_get(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, 0);
	struct mkgui_widget *w = find_widget(win, id);
	return (w && (w->style & MKGUI_RADIO_CHECKED)) ? 1 : 0;
}

// [=]===^=[ mkgui_radio_set ]======================================[=]
MKGUI_API void mkgui_radio_set(struct mkgui_window *win, uint32_t id, uint32_t checked) {
	MKGUI_CHECK(win);
	struct mkgui_widget *w = find_widget(win, id);
	if(!w || w->type != MKGUI_RADIO) {
		return;
	}

	if(checked) {
		for(uint32_t i = 0; i < win->widget_count; ++i) {
			struct mkgui_widget *s = &win->widgets[i];
			if(s->type == MKGUI_RADIO && s->parent_id == w->parent_id && s->id != id) {
				s->style &= ~MKGUI_RADIO_CHECKED;
			}
		}
		w->style |= MKGUI_RADIO_CHECKED;

	} else {
		w->style &= ~MKGUI_RADIO_CHECKED;
	}
	dirty_all(win);
}

// [=]===^=[ mkgui_radio_get_selected ]============================[=]
static uint32_t mkgui_radio_get_selected(struct mkgui_window *win, uint32_t group_parent_id) {
	for(uint32_t i = 0; i < win->widget_count; ++i) {
		struct mkgui_widget *w = &win->widgets[i];
		if(w->type == MKGUI_RADIO && w->parent_id == group_parent_id && (w->style & MKGUI_RADIO_CHECKED)) {
			return w->id;
		}
	}
	return 0;
}
