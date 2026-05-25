// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ render_button ]=====================================[=]
static void render_button(struct mkgui_window *win, uint32_t idx) {
	struct mkgui_widget *w = &win->widgets[idx];
	int32_t rx = win->rects[idx].x;
	int32_t ry = win->rects[idx].y;
	int32_t rw = win->rects[idx].w;
	int32_t rh = win->rects[idx].h;

	uint32_t disabled = (w->flags & MKGUI_DISABLED);
	uint32_t checked = (w->style & MKGUI_BUTTON_CHECKED);
	uint32_t bg = checked ? win->theme.widget_press : win->theme.widget_bg;
	uint32_t style = checked ? MKGUI_STYLE_SUNKEN : MKGUI_STYLE_RAISED;
	if(!disabled && win->press_id == w->id) {
		bg = win->theme.widget_press;

	} else if(!disabled && win->hover_id == w->id) {
		bg = win->theme.widget_hover;
	}
	uint32_t border = (win->focus_id == w->id) ? win->theme.highlight : win->theme.widget_border;
	bg = disabled_blend(bg, win->theme.bg, disabled);
	border = disabled_blend(border, win->theme.bg, disabled);
	draw_patch(win, style, rx, ry, rw, rh, bg, border);

	uint32_t tc = disabled ? win->theme.text_disabled : win->theme.text;
	int32_t ii = widget_icon_idx(win, w);
	uint32_t has_icon = (ii >= 0);
	int32_t tw = label_text_width(win, w);
	int32_t icon_w = has_icon ? (icons[ii].w + (w->label[0] ? sc(win, 4) : 0)) : 0;
	int32_t content_w = icon_w + (w->label[0] ? tw : 0);
	int32_t cx = rx + (rw - content_w) / 2;
	if(has_icon) {
		int32_t iy = ry + (rh - icons[ii].h) / 2;
		draw_icon(win->pixels, win->win_w, win->win_h, &icons[ii], cx, iy, rx + 1, ry + 1, rx + rw - 1, ry + rh - 1);
		cx += icons[ii].w + sc(win, 4);
	}

	if(w->label[0]) {
		int32_t ty = ry + (rh - win->font_height) / 2;
		push_text_clip(cx, ty, w->label, tc, rx, ry, rx + rw, ry + rh);
	}
}

// [=]===^=[ mkgui_button_set_text ]================================[=]
MKGUI_API void mkgui_button_set_text(struct mkgui_window *win, uint32_t id, const char *text) {
	MKGUI_CHECK(win);
	struct mkgui_widget *w = find_widget(win, id);
	if(!w) {
		return;
	}

	if(!text) {
		text = "";
	}
	strncpy(w->label, text, MKGUI_MAX_TEXT - 1);
	w->label[MKGUI_MAX_TEXT - 1] = '\0';
	w->label_tw = -1;
	dirty_all(win);
}

// [=]===^=[ mkgui_button_get_text ]================================[=]
MKGUI_API const char *mkgui_button_get_text(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, "");
	struct mkgui_widget *w = find_widget(win, id);
	return w ? w->label : "";
}

// [=]===^=[ handle_button_key ]=================================[=]
static uint32_t handle_button_key(struct mkgui_window *win, struct mkgui_event *ev, uint32_t ks) {
	if(ks == MKGUI_KEY_RETURN || ks == MKGUI_KEY_SPACE) {
		ev->type = MKGUI_EVENT_CLICK;
		ev->id = win->focus_id;
		dirty_all(win);
		return 1;
	}
	return 0;
}
