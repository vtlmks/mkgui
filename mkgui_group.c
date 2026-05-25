// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ render_group ]========================================[=]
static void render_group(struct mkgui_window *win, uint32_t idx) {
	struct mkgui_widget *w = &win->widgets[idx];
	int32_t rx = win->rects[idx].x;
	int32_t ry = win->rects[idx].y;
	int32_t rw = win->rects[idx].w;
	int32_t rh = win->rects[idx].h;

	int32_t group_pad = sc(win, 6);
	uint32_t collapsed = (w->style & MKGUI_GROUP_COLLAPSED);
	int32_t label_y = ry + sc(win, 2);
	int32_t label_x = rx + group_pad + sc(win, 4);
	int32_t tw = label_text_width(win, w);
	int32_t gap_left = label_x - sc(win, 4);
	int32_t gap_right = label_x + tw + sc(win, 4);

	int32_t frame_y = ry + win->font_height / 2 + sc(win, 2);
	int32_t frame_h = collapsed ? (rh - (frame_y - ry)) : (rh - (frame_y - ry));
	uint32_t bc = win->theme.widget_border;
	int32_t r = win->theme.corner_radius;

	uint32_t group_bg = shade_color(win->theme.bg, -10);
	draw_rounded_rect(win->pixels, win->win_w, win->win_h, rx, frame_y, rw, frame_h, group_bg, bc, r);

	if(gap_left > rx && gap_right <= rx + rw) {
		int32_t clear_y = frame_y;
		if(clear_y >= 0 && clear_y < win->win_h) {
			int32_t left = gap_left;
			int32_t right = gap_right;
			if(left < 0) {
				left = 0;
			}

			if(right > win->win_w) {
				right = win->win_w;
			}
			uint32_t *row = &win->pixels[clear_y * win->win_w];
			for(int32_t x = left; x < right; ++x) {
				row[x] = group_bg;
			}
		}
	}

	push_text_clip(label_x, label_y, w->label, win->theme.text, rx, ry, rx + rw, ry + rh);

	if(!(w->style & MKGUI_GROUP_COLLAPSIBLE)) {
		return;
	}

	uint32_t ac = win->theme.text;
	int32_t as = sc(win, 4);
	int32_t ax = rx + group_pad - sc(win, 3);
	int32_t ay = label_y + win->font_height / 2;
	if(collapsed) {
		draw_triangle_aa(win->pixels, win->win_w, win->win_h, ax, ay - as, ax + as, ay, ax, ay + as, ac);
	} else {
		draw_triangle_aa(win->pixels, win->win_w, win->win_h, ax - as, ay - as / 2, ax + as, ay - as / 2, ax, ay + as / 2, ac);
	}
}

// [=]===^=[ mkgui_group_set_collapsed ]============================[=]
MKGUI_API void mkgui_group_set_collapsed(struct mkgui_window *win, uint32_t id, uint32_t collapsed) {
	MKGUI_CHECK(win);
	struct mkgui_widget *w = find_widget(win, id);
	if(!w) {
		return;
	}

	if(collapsed) {
		w->style |= MKGUI_GROUP_COLLAPSED;
	} else {
		w->style &= ~MKGUI_GROUP_COLLAPSED;
	}
	dirty_all(win);
}

// [=]===^=[ mkgui_group_get_collapsed ]============================[=]
MKGUI_API uint32_t mkgui_group_get_collapsed(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, 0);
	struct mkgui_widget *w = find_widget(win, id);
	return (w && (w->style & MKGUI_GROUP_COLLAPSED)) ? 1 : 0;
}
