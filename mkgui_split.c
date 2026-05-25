// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ render_split ]======================================[=]
static void render_split(struct mkgui_window *win, uint32_t idx) {
	struct mkgui_widget *w = &win->widgets[idx];
	int32_t rx = win->rects[idx].x;
	int32_t ry = win->rects[idx].y;
	int32_t rw = win->rects[idx].w;
	int32_t rh = win->rects[idx].h;

	struct mkgui_split_data *sd = find_split_data(win, w->id);
	float ratio = sd ? sd->ratio : 0.5f;

	uint32_t color = win->theme.widget_border;

	if(w->type == MKGUI_HSPLIT) {
		int32_t split_y = ry + (int32_t)(rh * ratio);
		draw_rect_fill(win->pixels, win->win_w, win->win_h, rx, split_y, rw, win->split_thick, color);

	} else {
		int32_t split_x = rx + (int32_t)(rw * ratio);
		draw_rect_fill(win->pixels, win->win_w, win->win_h, split_x, ry, win->split_thick, rh, color);
	}
}

// [=]===^=[ mkgui_split_get_ratio ]================================[=]
MKGUI_API float mkgui_split_get_ratio(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, 0.0f);
	struct mkgui_split_data *sd = find_split_data(win, id);
	return sd ? sd->ratio : 0.5f;
}

// [=]===^=[ mkgui_split_set_ratio ]================================[=]
MKGUI_API void mkgui_split_set_ratio(struct mkgui_window *win, uint32_t id, float ratio) {
	MKGUI_CHECK(win);
	struct mkgui_split_data *sd = find_split_data(win, id);
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
	dirty_all(win);
}

// [=]===^=[ split_bar_hit ]=====================================[=]
static uint32_t split_bar_hit(struct mkgui_window *win, uint32_t idx, int32_t mx, int32_t my) {
	struct mkgui_widget *w = &win->widgets[idx];
	int32_t rx = win->rects[idx].x;
	int32_t ry = win->rects[idx].y;
	int32_t rw = win->rects[idx].w;
	int32_t rh = win->rects[idx].h;

	struct mkgui_split_data *sd = find_split_data(win, w->id);
	float ratio = sd ? sd->ratio : 0.5f;

	if(w->type == MKGUI_HSPLIT) {
		int32_t split_y = ry + (int32_t)(rh * ratio);
		return (mx >= rx && mx < rx + rw && my >= split_y && my < split_y + win->split_thick);

	} else {
		int32_t split_x = rx + (int32_t)(rw * ratio);
		return (mx >= split_x && mx < split_x + win->split_thick && my >= ry && my < ry + rh);
	}
}
