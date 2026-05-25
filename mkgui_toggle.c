// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ render_toggle ]=======================================[=]
static void render_toggle(struct mkgui_window *win, uint32_t idx) {
	struct mkgui_widget *w = &win->widgets[idx];
	int32_t rx = win->rects[idx].x;
	int32_t ry = win->rects[idx].y;
	int32_t rw = win->rects[idx].w;
	int32_t rh = win->rects[idx].h;

	struct mkgui_toggle_data *td = find_toggle_data(win, w->id);
	if(!td) {
		return;
	}

	int32_t track_w = sc(win, 38);
	int32_t track_h = sc(win, 20);
	int32_t knob_pad = sc(win, 3);
	int32_t knob_size = track_h - knob_pad * 2;

	uint32_t disabled = (w->flags & MKGUI_DISABLED);
	uint32_t focused = (win->focus_id == w->id);
	uint32_t hovered = (!disabled && win->hover_id == w->id);
	uint32_t on = td->state;

	int32_t ty = ry + (rh - track_h) / 2;
	int32_t track_r = track_h / 2;

	uint32_t track_fill;
	uint32_t track_border;
	if(on) {
		track_fill = win->theme.accent;
		track_border = track_fill;
	} else {
		track_fill = win->theme.bg;
		track_border = (focused || hovered) ? win->theme.highlight : win->theme.widget_border;
	}
	track_fill = disabled_blend(track_fill, win->theme.bg, disabled);
	track_border = disabled_blend(track_border, win->theme.bg, disabled);
	draw_rounded_rect(win->pixels, win->win_w, win->win_h, rx, ty, track_w, track_h, track_fill, track_border, track_r);

	int32_t knob_x;
	if(on) {
		knob_x = rx + track_w - knob_pad - knob_size;
	} else {
		knob_x = rx + knob_pad;
	}
	int32_t knob_y = ty + knob_pad;
	int32_t knob_r = knob_size / 2;
	uint32_t knob_color = on ? 0xffffffff : win->theme.widget_border;
	knob_color = disabled_blend(knob_color, win->theme.bg, disabled);
	draw_rounded_rect_fill(win->pixels, win->win_w, win->win_h, knob_x, knob_y, knob_size, knob_size, knob_color, knob_r);

	if(w->label[0]) {
		int32_t lx = rx + track_w + sc(win, 6);
		int32_t ly = ry + (rh - win->font_height) / 2;
		uint32_t tc = disabled ? win->theme.text_disabled : win->theme.text;
		push_text_clip(lx, ly, w->label, tc, rx, ry, rx + rw, ry + rh);
	}
}

// [=]===^=[ handle_toggle_click ]=================================[=]
static void handle_toggle_click(struct mkgui_window *win, struct mkgui_event *ev, uint32_t widget_id) {
	struct mkgui_toggle_data *td = find_toggle_data(win, widget_id);
	if(!td) {
		return;
	}
	td->state = td->state ? 0 : 1;
	ev->type = MKGUI_EVENT_TOGGLE_CHANGED;
	ev->id = widget_id;
	ev->value = (int32_t)td->state;
	dirty_all(win);
}

// [=]===^=[ handle_toggle_key ]===================================[=]
static uint32_t handle_toggle_key(struct mkgui_window *win, struct mkgui_event *ev, uint32_t ks) {
	if(ks == MKGUI_KEY_SPACE || ks == MKGUI_KEY_RETURN) {
		struct mkgui_toggle_data *td = find_toggle_data(win, win->focus_id);
		if(!td) {
			return 0;
		}
		td->state = td->state ? 0 : 1;
		ev->type = MKGUI_EVENT_TOGGLE_CHANGED;
		ev->id = win->focus_id;
		ev->value = (int32_t)td->state;
		dirty_all(win);
		return 1;
	}
	return 0;
}

// [=]===^=[ mkgui_toggle_set ]====================================[=]
MKGUI_API void mkgui_toggle_set(struct mkgui_window *win, uint32_t id, uint32_t state) {
	MKGUI_CHECK(win);
	struct mkgui_toggle_data *td = find_toggle_data(win, id);
	if(td) {
		td->state = state ? 1 : 0;
		dirty_all(win);
	}
}

// [=]===^=[ mkgui_toggle_get ]====================================[=]
MKGUI_API uint32_t mkgui_toggle_get(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, 0);
	struct mkgui_toggle_data *td = find_toggle_data(win, id);
	return td ? td->state : 0;
}
