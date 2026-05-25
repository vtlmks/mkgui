// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ render_divider ]=====================================[=]
static void render_divider(struct mkgui_window *win, uint32_t idx) {
	struct mkgui_widget *w = &win->widgets[idx];
	int32_t rx = win->rects[idx].x;
	int32_t ry = win->rects[idx].y;
	int32_t rw = win->rects[idx].w;
	int32_t rh = win->rects[idx].h;

	uint32_t color = win->theme.widget_border;
	if(w->flags & MKGUI_VERTICAL) {
		int32_t cx = rx + rw / 2;
		draw_vline(win->pixels, win->win_w, win->win_h, cx, ry + sc(win, 2), rh - sc(win, 4), color);
	} else {
		int32_t cy = ry + rh / 2;
		draw_hline(win->pixels, win->win_w, win->win_h, rx + sc(win, 2), cy, rw - sc(win, 4), color);
	}
}
