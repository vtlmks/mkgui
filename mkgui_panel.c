// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ render_panel ]======================================[=]
static void render_panel(struct mkgui_window *win, uint32_t idx) {
	int32_t rx = win->rects[idx].x;
	int32_t ry = win->rects[idx].y;
	int32_t rw = win->rects[idx].w;
	int32_t rh = win->rects[idx].h;

	struct mkgui_widget *w = &win->widgets[idx];
	uint32_t bg = (w->style & MKGUI_PANEL_SUNKEN) ? shade_color(win->theme.bg, -15) : win->theme.bg;
	uint32_t bc = (w->style & MKGUI_PANEL_BORDER) ? win->theme.widget_border : 0;

	if(bc) {
		draw_rounded_rect(win->pixels, win->win_w, win->win_h, rx, ry, rw, rh, bg, bc, win->theme.corner_radius);
	} else {
		draw_rect_fill(win->pixels, win->win_w, win->win_h, rx, ry, rw, rh, bg);
	}
}
