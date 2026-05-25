// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define MKGUI_TOOLTIP_DELAY_MS 500

// [=]===^=[ render_tooltip ]=====================================[=]
static void render_tooltip(struct mkgui_window *win) {
	if(!win->tooltip_id) {
		return;
	}
	uint32_t elapsed = (uint32_t)(mkgui_now_ns() / 1000000ull) - win->tooltip_start_ms;
	if(elapsed < MKGUI_TOOLTIP_DELAY_MS) {
		return;
	}

	int32_t idx = find_widget_idx(win, win->tooltip_id);
	if(idx < 0) {
		return;
	}

	char *text = win->tooltip_texts[idx];
	if(text[0] == '\0') {
		win->tooltip_shown = 1;
		return;
	}

	int32_t pad = sc(win, 4);
	int32_t tw = text_width(win, text) + pad * 2;
	int32_t th = win->font_height + pad * 2;
	int32_t tx = win->tooltip_x + sc(win, 12);
	int32_t ty = win->tooltip_y + sc(win, 16);

	if(tx + tw > win->win_w) {
		tx = win->win_w - tw;
	}

	if(ty + th > win->win_h) {
		ty = win->tooltip_y - th - sc(win, 4);
	}

	win->tooltip_shown = 1;
	draw_rounded_rect(win->pixels, win->win_w, win->win_h, tx, ty, tw, th, win->theme.widget_bg, win->theme.widget_border, win->theme.corner_radius);
	push_text_clip(tx + pad, ty + pad, text, win->theme.text, tx, ty, tx + tw, ty + th);
}

// [=]===^=[ tooltip_update ]=====================================[=]
static void tooltip_update(struct mkgui_window *win, uint32_t hover_id, int32_t mx, int32_t my) {
	if(hover_id != win->tooltip_id) {
		if(win->tooltip_shown) {
			dirty_all(win);
		}
		win->tooltip_id = hover_id;
		win->tooltip_start_ms = hover_id ? (uint32_t)(mkgui_now_ns() / 1000000ull) : 0;
		win->tooltip_shown = 0;
		win->tooltip_x = mx;
		win->tooltip_y = my;
	}
}

// [=]===^=[ mkgui_set_tooltip ]==================================[=]
MKGUI_API void mkgui_set_tooltip(struct mkgui_window *win, uint32_t id, const char *text) {
	MKGUI_CHECK(win);
	int32_t idx = find_widget_idx(win, id);
	if(idx < 0) {
		return;
	}

	if(text) {
		snprintf(win->tooltip_texts[idx], sizeof(win->tooltip_texts[idx]), "%s", text);
	} else {
		win->tooltip_texts[idx][0] = '\0';
	}
}
