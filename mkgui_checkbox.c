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
	for(uint32_t i = 0; i <= (uint32_t)(steps * 8); ++i) {
		int32_t cx8 = x0 * 8 + dx * (int32_t)i / steps;
		int32_t cy8 = y0 * 8 + dy * (int32_t)i / steps;
		int32_t px_lo = (cx8 - half) >> 3;
		int32_t px_hi = (cx8 + half) >> 3;
		int32_t py_lo = (cy8 - half) >> 3;
		int32_t py_hi = (cy8 + half) >> 3;
		if(py_lo < 0) {
			py_lo = 0;
		}

		if(py_hi >= bh) {
			py_hi = bh - 1;
		}

		if(px_lo < 0) {
			px_lo = 0;
		}

		if(px_hi >= bw) {
			px_hi = bw - 1;
		}
		for(int32_t py = py_lo; py <= py_hi; ++py) {
			for(int32_t px = px_lo; px <= px_hi; ++px) {
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
static void render_checkbox(struct mkgui_window *win, uint32_t idx) {
	struct mkgui_widget *w = &win->widgets[idx];
	int32_t rx = win->rects[idx].x;
	int32_t ry = win->rects[idx].y;
	int32_t rh = win->rects[idx].h;

	int32_t box_size = sc(win, 16);
	int32_t by = ry + (rh - box_size) / 2;

	uint32_t disabled = (w->flags & MKGUI_DISABLED);
	uint32_t focused = (win->focus_id == w->id);
	uint32_t hovered = (!disabled && win->hover_id == w->id);
	uint32_t pressed = (!disabled && win->press_id == w->id);
	uint32_t bg = (w->style & MKGUI_CHECKBOX_CHECKED) ? win->theme.highlight : (pressed ? win->theme.widget_press : win->theme.input_bg);
	uint32_t border = (focused || hovered) ? win->theme.highlight : win->theme.widget_border;
	bg = disabled_blend(bg, win->theme.bg, disabled);
	border = disabled_blend(border, win->theme.bg, disabled);
	draw_patch(win, MKGUI_STYLE_SUNKEN, rx, by, box_size, box_size, bg, border);

	if(w->style & MKGUI_CHECKBOX_CHECKED) {
		int32_t cx = rx + box_size / 2;
		int32_t cy = by + box_size / 2;
		uint32_t check_color = disabled ? win->theme.text_disabled : win->theme.sel_text;
		draw_aa_line(win->pixels, win->win_w, win->win_h, cx - sc(win, 4), cy - sc(win, 1), cx - sc(win, 1), cy + sc(win, 3), check_color, sc(win, 2));
		draw_aa_line(win->pixels, win->win_w, win->win_h, cx - sc(win, 1), cy + sc(win, 3), cx + sc(win, 5), cy - sc(win, 4), check_color, sc(win, 2));
	}

	int32_t rw = win->rects[idx].w;
	int32_t ty = ry + (rh - win->font_height) / 2;
	uint32_t tc = disabled ? win->theme.text_disabled : win->theme.text;
	push_text_clip(rx + box_size + sc(win, 6), ty, w->label, tc, rx, ry, rx + rw, ry + rh);
}

// [=]===^=[ handle_checkbox_key ]===============================[=]
static uint32_t handle_checkbox_key(struct mkgui_window *win, struct mkgui_event *ev, uint32_t ks) {
	if(ks == MKGUI_KEY_SPACE) {
		struct mkgui_widget *w = find_widget(win, win->focus_id);
		if(!w) {
			return 0;
		}
		w->style ^= MKGUI_CHECKBOX_CHECKED;
		ev->type = MKGUI_EVENT_CHECKBOX_CHANGED;
		ev->id = win->focus_id;
		ev->value = (w->style & MKGUI_CHECKBOX_CHECKED) ? 1 : 0;
		dirty_all(win);
		return 1;
	}
	return 0;
}

// [=]===^=[ mkgui_checkbox_get ]================================[=]
MKGUI_API uint32_t mkgui_checkbox_get(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, 0);
	struct mkgui_widget *w = find_widget(win, id);
	if(!w) {
		return 0;
	}
	return (w->style & MKGUI_CHECKBOX_CHECKED) ? 1 : 0;
}

// [=]===^=[ mkgui_checkbox_set ]================================[=]
MKGUI_API void mkgui_checkbox_set(struct mkgui_window *win, uint32_t id, uint32_t checked) {
	MKGUI_CHECK(win);
	struct mkgui_widget *w = find_widget(win, id);
	if(!w) {
		return;
	}

	if(checked) {
		w->style |= MKGUI_CHECKBOX_CHECKED;

	} else {
		w->style &= ~MKGUI_CHECKBOX_CHECKED;
	}
	dirty_all(win);
}
