// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define MKGUI_DROPDOWN_MAX_VISIBLE 12

// [=]===^=[ render_dropdown ]===================================[=]
static void render_dropdown(struct mkgui_window *win, uint32_t idx) {
	struct mkgui_widget *w = &win->widgets[idx];
	int32_t rx = win->rects[idx].x;
	int32_t ry = win->rects[idx].y;
	int32_t rw = win->rects[idx].w;
	int32_t rh = win->rects[idx].h;

	uint32_t disabled = (w->flags & MKGUI_DISABLED);
	uint32_t bg = win->theme.widget_bg;
	if(!disabled && win->hover_id == w->id) {
		bg = win->theme.widget_hover;
	}
	uint32_t border = (win->focus_id == w->id) ? win->theme.highlight : win->theme.widget_border;
	bg = disabled_blend(bg, win->theme.bg, disabled);
	border = disabled_blend(border, win->theme.bg, disabled);
	draw_patch(win, MKGUI_STYLE_RAISED, rx, ry, rw, rh, bg, border);

	struct mkgui_dropdown_data *dd = find_dropdown_data(win, w->id);
	char *text = w->label;
	if(dd && dd->selected >= 0 && dd->selected < (int32_t)dd->item_count) {
		text = dd->items[dd->selected];
	}
	uint32_t tc = disabled ? win->theme.text_disabled : win->theme.text;
	int32_t ty = ry + (rh - win->font_height) / 2;
	int32_t text_pad = sc(win, 4);
	int32_t arrow_margin = sc(win, 14);
	push_text_clip(rx + text_pad, ty, text, tc, rx + 1, ry + 1, rx + rw - arrow_margin - sc(win, 2), ry + rh - 1);

	int32_t as = sc(win, 4);
	int32_t acx = rx + rw - arrow_margin + as;
	int32_t acy = ry + rh / 2;
	draw_triangle_aa(win->pixels, win->win_w, win->win_h, acx - as, acy - as / 2, acx + as, acy - as / 2, acx, acy + as / 2, tc);
}

// [=]===^=[ dropdown_clamp_scroll ]=============================[=]
static void dropdown_clamp_scroll(struct mkgui_window *win, struct mkgui_dropdown_data *dd, int32_t popup_h) {
	int32_t content_h = (int32_t)dd->item_count * win->row_height;
	int32_t view_h = popup_h - 2;
	int32_t max_scroll = content_h - view_h;
	if(max_scroll < 0) {
		max_scroll = 0;
	}

	if(dd->scroll_y < 0) {
		dd->scroll_y = 0;
	}

	if(dd->scroll_y > max_scroll) {
		dd->scroll_y = max_scroll;
	}
}

// [=]===^=[ render_dropdown_popup ]=============================[=]
static void render_dropdown_popup(struct mkgui_window *win, struct mkgui_popup *p, struct mkgui_dropdown_data *dd, int32_t hover_item) {
	int32_t saved_clip_x1 = render_clip_x1;
	int32_t saved_clip_y1 = render_clip_y1;
	int32_t saved_clip_x2 = render_clip_x2;
	int32_t saved_clip_y2 = render_clip_y2;
	render_clip_x1 = 0;
	render_clip_y1 = 0;
	render_clip_x2 = INT32_MAX;
	render_clip_y2 = INT32_MAX;

	draw_rounded_rect(p->pixels, p->w, p->h, 0, 0, p->w, p->h, win->theme.menu_bg, win->theme.widget_border, win->theme.corner_radius);

	int32_t view_h = p->h - 2;
	int32_t content_h = (int32_t)dd->item_count * win->row_height;
	int32_t sb_w = sc(win, 8);
	uint32_t needs_scroll = content_h > view_h;
	int32_t item_w = needs_scroll ? p->w - sb_w - 2 : p->w - 2;

	for(uint32_t i = 0; i < dd->item_count; ++i) {
		int32_t iy = 1 + (int32_t)i * win->row_height - dd->scroll_y;
		if(iy + win->row_height <= 0 || iy >= p->h) {
			continue;
		}
		uint32_t bg;
		if((int32_t)i == hover_item) {
			bg = win->theme.highlight;
		} else if((int32_t)i == dd->selected) {
			bg = win->theme.selection;
		} else {
			bg = win->theme.menu_bg;
		}
		draw_rect_fill(p->pixels, p->w, p->h, 1, iy, item_w, win->row_height, bg);
		int32_t ty = iy + (win->row_height - win->font_height) / 2;
		uint32_t tc = ((int32_t)i == dd->selected) ? win->theme.sel_text : win->theme.text;
		push_text_clip(p->x + sc(win, 5), ty + p->y, dd->items[i], tc, p->x + 1, p->y + 1, p->x + p->w - 1, p->y + p->h - 1);
	}

	if(needs_scroll) {
		int32_t min_thumb = sc(win, 16);
		int32_t sb_x = p->w - sb_w - 1;
		draw_rect_fill(p->pixels, p->w, p->h, sb_x, 1, sb_w, view_h, win->theme.scrollbar_bg);
		int32_t thumb_h = (view_h * view_h) / content_h;
		if(thumb_h < min_thumb) {
			thumb_h = min_thumb;
		}
		int32_t max_scroll = content_h - view_h;
		int32_t thumb_y = 1;
		if(max_scroll > 0) {
			thumb_y = 1 + (dd->scroll_y * (view_h - thumb_h)) / max_scroll;
		}
		draw_rounded_rect_fill(p->pixels, p->w, p->h, sb_x, thumb_y, sb_w, thumb_h, win->theme.scrollbar_thumb, win->theme.corner_radius);
	}

	render_clip_x1 = saved_clip_x1;
	render_clip_y1 = saved_clip_y1;
	render_clip_x2 = saved_clip_x2;
	render_clip_y2 = saved_clip_y2;
}

// [=]===^=[ dropdown_open_popup ]===============================[=]
static void dropdown_open_popup(struct mkgui_window *win, uint32_t widget_id) {
	struct mkgui_dropdown_data *dd = find_dropdown_data(win, widget_id);
	if(!dd || dd->item_count == 0) {
		return;
	}

	int32_t idx = find_widget_idx(win, widget_id);
	if(idx < 0) {
		return;
	}

	int32_t abs_x, abs_y;
	platform_translate_coords(win, win->rects[idx].x, win->rects[idx].y + win->rects[idx].h, &abs_x, &abs_y);

	int32_t pw = win->rects[idx].w;
	int32_t ph = (int32_t)dd->item_count * win->row_height + 2;
	int32_t max_ph = MKGUI_DROPDOWN_MAX_VISIBLE * win->row_height + 2;
	if(ph > max_ph) {
		ph = max_ph;
	}

	if(dd->selected > 0) {
		dd->scroll_y = dd->selected * win->row_height - (ph - 2) / 2;
	} else {
		dd->scroll_y = 0;
	}
	dropdown_clamp_scroll(win, dd, ph);

	popup_destroy_all(win);
	struct mkgui_popup *p = popup_create(win, abs_x, abs_y, pw, ph, widget_id);
	if(p) {
		render_dropdown_popup(win, p, dd, -1);
		flush_text_popup(win, p);
		platform_popup_blit(win, p);
		platform_flush(win);
	}
}

// [=]===^=[ handle_dropdown_key ]===============================[=]
static uint32_t handle_dropdown_key(struct mkgui_window *win, struct mkgui_event *ev, uint32_t ks) {
	struct mkgui_dropdown_data *dd;

	switch(ks) {
	case MKGUI_KEY_RETURN:
	case MKGUI_KEY_SPACE: {
		dropdown_open_popup(win, win->focus_id);
		return 0;
	}

	case MKGUI_KEY_UP: {
		dd = find_dropdown_data(win, win->focus_id);
		if(!dd) {
			return 0;
		}

		if(dd->selected > 0) {
			--dd->selected;
			dirty_all(win);
			ev->type = MKGUI_EVENT_DROPDOWN_CHANGED;
			ev->id = win->focus_id;
			ev->value = dd->selected;
			return 1;
		}
	} break;

	case MKGUI_KEY_DOWN: {
		dd = find_dropdown_data(win, win->focus_id);
		if(!dd) {
			return 0;
		}

		if(dd->selected < (int32_t)dd->item_count - 1) {
			++dd->selected;
			dirty_all(win);
			ev->type = MKGUI_EVENT_DROPDOWN_CHANGED;
			ev->id = win->focus_id;
			ev->value = dd->selected;
			return 1;
		}
	} break;

	default: {
	} break;
	}

	return 0;
}

// [=]===^=[ mkgui_dropdown_setup ]==============================[=]
MKGUI_API void mkgui_dropdown_setup(struct mkgui_window *win, uint32_t id, const char *const *items, uint32_t count) {
	MKGUI_CHECK(win);
	if(!items && count > 0) {
		return;
	}
	struct mkgui_dropdown_data *dd = find_dropdown_data(win, id);
	if(!dd) {
		return;
	}
	dd->item_count = count > MKGUI_MAX_DROPDOWN ? MKGUI_MAX_DROPDOWN : count;
	for(uint32_t i = 0; i < dd->item_count; ++i) {
		strncpy(dd->items[i], items[i], MKGUI_MAX_TEXT - 1);
		dd->items[i][MKGUI_MAX_TEXT - 1] = '\0';
	}
	dd->selected = 0;
	dd->scroll_y = 0;
	dirty_all(win);
}

// [=]===^=[ mkgui_dropdown_get ]================================[=]
MKGUI_API int32_t mkgui_dropdown_get(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, -1);
	struct mkgui_dropdown_data *dd = find_dropdown_data(win, id);
	return dd ? dd->selected : -1;
}

// [=]===^=[ mkgui_dropdown_set ]================================[=]
MKGUI_API void mkgui_dropdown_set(struct mkgui_window *win, uint32_t id, int32_t index) {
	MKGUI_CHECK(win);
	struct mkgui_dropdown_data *dd = find_dropdown_data(win, id);
	if(!dd) {
		return;
	}

	if(index < 0 || (uint32_t)index >= dd->item_count) {
		dd->selected = -1;

	} else {
		dd->selected = index;
	}
	dirty_all(win);
}

// [=]===^=[ mkgui_dropdown_get_text ]==============================[=]
MKGUI_API const char *mkgui_dropdown_get_text(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, "");
	struct mkgui_dropdown_data *dd = find_dropdown_data(win, id);
	if(!dd || dd->selected < 0 || (uint32_t)dd->selected >= dd->item_count) {
		return "";
	}
	return dd->items[dd->selected];
}

// [=]===^=[ mkgui_dropdown_get_count ]=============================[=]
MKGUI_API uint32_t mkgui_dropdown_get_count(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, 0);
	struct mkgui_dropdown_data *dd = find_dropdown_data(win, id);
	return dd ? dd->item_count : 0;
}

// [=]===^=[ mkgui_dropdown_get_item_text ]=========================[=]
MKGUI_API const char *mkgui_dropdown_get_item_text(struct mkgui_window *win, uint32_t id, uint32_t index) {
	MKGUI_CHECK_VAL(win, "");
	struct mkgui_dropdown_data *dd = find_dropdown_data(win, id);
	if(!dd || index >= dd->item_count) {
		return "";
	}
	return dd->items[index];
}

// [=]===^=[ mkgui_dropdown_add ]=================================[=]
MKGUI_API void mkgui_dropdown_add(struct mkgui_window *win, uint32_t id, const char *text) {
	MKGUI_CHECK(win);
	if(!text) {
		return;
	}
	struct mkgui_dropdown_data *dd = find_dropdown_data(win, id);
	if(!dd || dd->item_count >= MKGUI_MAX_DROPDOWN) {
		return;
	}
	strncpy(dd->items[dd->item_count], text, MKGUI_MAX_TEXT - 1);
	dd->items[dd->item_count][MKGUI_MAX_TEXT - 1] = '\0';
	++dd->item_count;
	dirty_all(win);
}

// [=]===^=[ mkgui_dropdown_remove ]===============================[=]
MKGUI_API void mkgui_dropdown_remove(struct mkgui_window *win, uint32_t id, uint32_t index) {
	MKGUI_CHECK(win);
	struct mkgui_dropdown_data *dd = find_dropdown_data(win, id);
	if(!dd || index >= dd->item_count) {
		return;
	}
	for(uint32_t i = index; i < dd->item_count - 1; ++i) {
		memcpy(dd->items[i], dd->items[i + 1], MKGUI_MAX_TEXT);
	}
	--dd->item_count;
	if(dd->selected >= (int32_t)dd->item_count) {
		dd->selected = (int32_t)dd->item_count - 1;
	}
	dirty_all(win);
}

// [=]===^=[ mkgui_dropdown_clear ]================================[=]
MKGUI_API void mkgui_dropdown_clear(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK(win);
	struct mkgui_dropdown_data *dd = find_dropdown_data(win, id);
	if(!dd) {
		return;
	}
	dd->item_count = 0;
	dd->selected = -1;
	dd->scroll_y = 0;
	dirty_all(win);
}
