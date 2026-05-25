// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ render_canvas ]======================================[=]
static void render_canvas(struct mkgui_window *win, uint32_t idx) {
	struct mkgui_widget *w = &win->widgets[idx];
	int32_t rx = win->rects[idx].x;
	int32_t ry = win->rects[idx].y;
	int32_t rw = win->rects[idx].w;
	int32_t rh = win->rects[idx].h;

	if(w->style & MKGUI_CANVAS_BORDER) {
		uint32_t bg = (w->style & MKGUI_CANVAS_SUNKEN) ? shade_color(win->theme.bg, -15) : win->theme.bg;
		draw_rounded_rect(win->pixels, win->win_w, win->win_h, rx, ry, rw, rh, bg, win->theme.widget_border, win->theme.corner_radius);
	}

	struct mkgui_canvas_data *cd = find_canvas_data(win, w->id);
	if(cd && cd->callback && rw > 0 && rh > 0) {
		int32_t old_x1 = render_clip_x1;
		int32_t old_y1 = render_clip_y1;
		int32_t old_x2 = render_clip_x2;
		int32_t old_y2 = render_clip_y2;
		render_clip_x1 = rx;
		render_clip_y1 = ry;
		render_clip_x2 = rx + rw;
		render_clip_y2 = ry + rh;
		cd->callback(win, w->id, win->pixels, rx, ry, rw, rh, cd->userdata);
		render_clip_x1 = old_x1;
		render_clip_y1 = old_y1;
		render_clip_x2 = old_x2;
		render_clip_y2 = old_y2;
	}
}

// [=]===^=[ mkgui_canvas_set_callback ]============================[=]
MKGUI_API void mkgui_canvas_set_callback(struct mkgui_window *win, uint32_t id, mkgui_canvas_cb callback, void *userdata) {
	MKGUI_CHECK(win);
	struct mkgui_canvas_data *cd = find_canvas_data(win, id);
	if(cd) {
		cd->callback = callback;
		cd->userdata = userdata;
	}
}
