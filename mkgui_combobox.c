// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define MKGUI_COMBOBOX_MAX_VISIBLE 12

// [=]===^=[ combobox_filter ]=====================================[=]
static void combobox_filter(struct mkgui_combobox_data *cb) {
	cb->filter_count = 0;
	if(cb->text[0] == '\0') {
		for(uint32_t i = 0; i < cb->item_count && cb->filter_count < MKGUI_MAX_DROPDOWN; ++i) {
			cb->filter_map[cb->filter_count++] = i;
		}
		return;
	}
	for(uint32_t i = 0; i < cb->item_count; ++i) {
		uint32_t found = 0;
		for(uint32_t s = 0; cb->items[i][s] && !found; ++s) {
			uint32_t match = 1;
			for(uint32_t t = 0; cb->text[t] && match; ++t) {
				char a = cb->items[i][s + t];
				char b = cb->text[t];
				if(a >= 'A' && a <= 'Z') {
					a += 32;
				}

				if(b >= 'A' && b <= 'Z') {
					b += 32;
				}

				if(a != b) {
					match = 0;
				}
			}

			if(match) {
				found = 1;
			}
		}

		if(found && cb->filter_count < MKGUI_MAX_DROPDOWN) {
			cb->filter_map[cb->filter_count++] = i;
		}
	}
}

// [=]===^=[ combobox_has_selection ]===============================[=]
static uint32_t combobox_has_selection(struct mkgui_combobox_data *cb) {
	return cb->sel_start != cb->sel_end;
}

// [=]===^=[ combobox_clear_selection ]=============================[=]
static void combobox_clear_selection(struct mkgui_combobox_data *cb) {
	cb->sel_start = cb->cursor;
	cb->sel_end = cb->cursor;
}

// [=]===^=[ combobox_delete_selection ]============================[=]
static void combobox_delete_selection(struct mkgui_combobox_data *cb) {
	struct mkgui_text_edit te = {cb->text, MKGUI_MAX_TEXT, cb->cursor, cb->sel_start, cb->sel_end, cb->scroll_x};
	textedit_delete_selection(&te);
	cb->cursor = te.cursor;
	cb->sel_start = te.sel_start;
	cb->sel_end = te.sel_end;
}

// [=]===^=[ combobox_select_all ]=================================[=]
static void combobox_select_all(struct mkgui_combobox_data *cb) {
	uint32_t len = (uint32_t)strlen(cb->text);
	cb->sel_start = 0;
	cb->sel_end = len;
	cb->cursor = len;
	cb->scroll_x = 0;
}

// [=]===^=[ combobox_hit_cursor ]=================================[=]
static uint32_t combobox_hit_cursor(struct mkgui_window *win, struct mkgui_combobox_data *cb, int32_t rx, int32_t mx) {
	return textedit_hit_cursor(win, cb->text, rx + sc(win, 4) - cb->scroll_x, mx);
}

// [=]===^=[ combobox_scroll_to_cursor ]============================[=]
static void combobox_scroll_to_cursor(struct mkgui_window *win, uint32_t widget_id) {
	struct mkgui_combobox_data *cb = find_combobox_data(win, widget_id);
	if(!cb) {
		return;
	}
	int32_t widx = find_widget_idx(win, widget_id);
	if(widx < 0) {
		return;
	}
	int32_t pad = sc(win, 4);
	int32_t visible = win->rects[widx].w - sc(win, 24) - pad * 2;

	struct mkgui_text_edit te = {cb->text, MKGUI_MAX_TEXT, cb->cursor, cb->sel_start, cb->sel_end, cb->scroll_x};
	textedit_scroll_to_cursor(win, &te, cb->text, visible);
	cb->scroll_x = te.scroll_x;
}

// [=]===^=[ render_combobox ]=====================================[=]
static void render_combobox(struct mkgui_window *win, uint32_t idx) {
	struct mkgui_widget *w = &win->widgets[idx];
	int32_t rx = win->rects[idx].x;
	int32_t ry = win->rects[idx].y;
	int32_t rw = win->rects[idx].w;
	int32_t rh = win->rects[idx].h;

	int32_t btn_w = sc(win, 24);
	int32_t text_pad = sc(win, 4);
	int32_t inset2 = sc(win, 2);

	uint32_t disabled = (w->flags & MKGUI_DISABLED);
	uint32_t focused = (win->focus_id == w->id);
	uint32_t border = focused ? win->theme.highlight : win->theme.widget_border;
	uint32_t input_bg = disabled_blend(win->theme.input_bg, win->theme.bg, disabled);
	border = disabled_blend(border, win->theme.bg, disabled);

	int32_t text_w = rw - btn_w;
	draw_patch(win, MKGUI_STYLE_SUNKEN, rx, ry, text_w, rh, input_bg, border);

	uint32_t hovered = (!disabled && win->hover_id == w->id);
	uint32_t btn_bg = hovered ? win->theme.widget_hover : win->theme.widget_bg;
	btn_bg = disabled_blend(btn_bg, win->theme.bg, disabled);
	draw_patch(win, MKGUI_STYLE_RAISED, rx + text_w, ry, btn_w, rh, btn_bg, border);

	uint32_t tc = disabled ? win->theme.text_disabled : win->theme.text;
	int32_t as = sc(win, 4);
	int32_t acx = rx + text_w + btn_w / 2;
	int32_t acy = ry + rh / 2;
	draw_triangle_aa(win->pixels, win->win_w, win->win_h, acx - as, acy - as / 2, acx + as, acy - as / 2, acx, acy + as / 2, tc);

	struct mkgui_combobox_data *cb = find_combobox_data(win, w->id);
	if(!cb) {
		return;
	}

	int32_t ty = ry + (rh - win->font_height) / 2;
	int32_t tx = rx + text_pad - cb->scroll_x;

	struct mkgui_text_edit te = {cb->text, MKGUI_MAX_TEXT, cb->cursor, cb->sel_start, cb->sel_end, cb->scroll_x};
	textedit_render(win, &te, cb->text, tx, ty, ry + inset2, rh - inset2 * 2, rx + 1, ry + 1, rx + text_w - 1, ry + rh - 1, tc, focused);
}

// [=]===^=[ render_combobox_popup ]===============================[=]
static void render_combobox_popup(struct mkgui_window *win, struct mkgui_popup *p, struct mkgui_combobox_data *cb, int32_t hover_item) {
	int32_t saved_clip_x1 = render_clip_x1;
	int32_t saved_clip_y1 = render_clip_y1;
	int32_t saved_clip_x2 = render_clip_x2;
	int32_t saved_clip_y2 = render_clip_y2;
	render_clip_x1 = 0;
	render_clip_y1 = 0;
	render_clip_x2 = INT32_MAX;
	render_clip_y2 = INT32_MAX;

	draw_rounded_rect(p->pixels, p->w, p->h, 0, 0, p->w, p->h, win->theme.menu_bg, win->theme.widget_border, win->theme.corner_radius);

	for(uint32_t i = 0; i < cb->filter_count; ++i) {
		int32_t iy = 1 + (int32_t)i * win->row_height - cb->scroll_y;
		if(iy + win->row_height <= 0 || iy >= p->h) {
			continue;
		}
		uint32_t bg;
		if((int32_t)i == hover_item) {
			bg = win->theme.highlight;
		} else if((int32_t)i == cb->highlight) {
			bg = win->theme.selection;
		} else {
			bg = win->theme.menu_bg;
		}
		draw_rect_fill(p->pixels, p->w, p->h, 1, iy, p->w - 2, win->row_height, bg);
		int32_t ty = iy + (win->row_height - win->font_height) / 2;
		uint32_t tc = ((int32_t)i == cb->highlight) ? win->theme.sel_text : win->theme.text;
		uint32_t real_idx = cb->filter_map[i];
		push_text_clip(p->x + sc(win, 5), ty + p->y, cb->items[real_idx], tc, p->x + 1, p->y + 1, p->x + p->w - 1, p->y + p->h - 1);
	}

	render_clip_x1 = saved_clip_x1;
	render_clip_y1 = saved_clip_y1;
	render_clip_x2 = saved_clip_x2;
	render_clip_y2 = saved_clip_y2;
}

// [=]===^=[ combobox_open_popup ]=================================[=]
static void combobox_open_popup(struct mkgui_window *win, uint32_t widget_id, uint32_t filtered) {
	struct mkgui_combobox_data *cb = find_combobox_data(win, widget_id);
	if(!cb) {
		return;
	}

	if(filtered) {
		combobox_filter(cb);
	} else {
		cb->filter_count = 0;
		for(uint32_t i = 0; i < cb->item_count && cb->filter_count < MKGUI_MAX_DROPDOWN; ++i) {
			cb->filter_map[cb->filter_count++] = i;
		}
	}

	if(cb->filter_count == 0) {
		popup_destroy_all(win);
		return;
	}

	cb->highlight = 0;
	cb->scroll_y = 0;
	cb->popup_open = 1;

	uint32_t widx = (uint32_t)find_widget_idx(win, widget_id);
	if(widx >= win->widget_count) {
		return;
	}

	int32_t abs_x, abs_y;
	platform_translate_coords(win, win->rects[widx].x, win->rects[widx].y + win->rects[widx].h, &abs_x, &abs_y);

	int32_t pw = win->rects[widx].w;
	int32_t ph = (int32_t)cb->filter_count * win->row_height + 2;
	int32_t max_ph = MKGUI_COMBOBOX_MAX_VISIBLE * win->row_height + 2;
	if(ph > max_ph) {
		ph = max_ph;
	}

	popup_destroy_all(win);
	struct mkgui_popup *p = popup_create(win, abs_x, abs_y, pw, ph, widget_id);
	if(p) {
		render_combobox_popup(win, p, cb, -1);
		flush_text_popup(win, p);
		platform_popup_blit(win, p);
		platform_flush(win);
	}
}

// [=]===^=[ combobox_close_popup ]================================[=]
static void combobox_close_popup(struct mkgui_window *win, struct mkgui_combobox_data *cb) {
	cb->popup_open = 0;
	popup_destroy_all(win);
}

// [=]===^=[ handle_combobox_click ]===============================[=]
static void handle_combobox_click(struct mkgui_window *win, struct mkgui_event *ev, uint32_t widget_id) {
	struct mkgui_combobox_data *cb = find_combobox_data(win, widget_id);
	if(!cb) {
		return;
	}

	uint32_t widx = (uint32_t)find_widget_idx(win, widget_id);
	if(widx >= win->widget_count) {
		return;
	}
	int32_t rx = win->rects[widx].x;
	int32_t rw = win->rects[widx].w;
	int32_t btn_x = rx + rw - sc(win, 24);

	if(win->mouse_x >= btn_x) {
		if(cb->popup_open) {
			combobox_close_popup(win, cb);
		} else {
			combobox_open_popup(win, widget_id, 0);
		}
	} else {
		if(win->focus_id != widget_id) {
			combobox_select_all(cb);
		} else {
			cb->cursor = combobox_hit_cursor(win, cb, rx, win->mouse_x);
			combobox_clear_selection(cb);
			win->drag_select_id = widget_id;
		}
	}
	dirty_all(win);
	combobox_scroll_to_cursor(win, widget_id);
	(void)ev;
}

// [=]===^=[ handle_combobox_key ]=================================[=]
static uint32_t handle_combobox_key(struct mkgui_window *win, struct mkgui_event *ev, uint32_t ks, uint32_t keymod, char *buf, int32_t len) {
	struct mkgui_combobox_data *cb = find_combobox_data(win, win->focus_id);
	if(!cb) {
		return 0;
	}
	uint32_t shift = (keymod & MKGUI_MOD_SHIFT);

	switch(ks) {
	case MKGUI_KEY_ESCAPE: {
		if(cb->popup_open) {
			{ size_t _l = strlen(cb->prev_text); if(_l >= MKGUI_MAX_TEXT) { _l = MKGUI_MAX_TEXT - 1; } memcpy(cb->text, cb->prev_text, _l); cb->text[_l] = '\0'; }
			cb->cursor = (uint32_t)strlen(cb->text);
			combobox_clear_selection(cb);
			combobox_close_popup(win, cb);
			dirty_all(win);
			return 1;
		}
		return 0;
	}

	case MKGUI_KEY_RETURN: {
		if(cb->popup_open && cb->highlight >= 0 && cb->highlight < (int32_t)cb->filter_count) {
			uint32_t real_idx = cb->filter_map[cb->highlight];
			{ size_t _l = strlen(cb->items[real_idx]); if(_l >= MKGUI_MAX_TEXT) { _l = MKGUI_MAX_TEXT - 1; } memcpy(cb->text, cb->items[real_idx], _l); cb->text[_l] = '\0'; }
			cb->selected = (int32_t)real_idx;
			cb->cursor = (uint32_t)strlen(cb->text);
			combobox_clear_selection(cb);
			combobox_close_popup(win, cb);
			ev->type = MKGUI_EVENT_COMBOBOX_CHANGED;
			ev->id = win->focus_id;
			ev->value = cb->selected;
			dirty_all(win);
			return 1;
		}
		combobox_close_popup(win, cb);
		ev->type = MKGUI_EVENT_COMBOBOX_SUBMIT;
		ev->id = win->focus_id;
		dirty_all(win);
		return 1;
	}

	case MKGUI_KEY_UP: {
		if(cb->popup_open) {
			if(cb->highlight > 0) {
				--cb->highlight;
			}
			for(uint32_t pi = 0; pi < win->popup_count; ++pi) {
				win->popups[pi].dirty = 1;
			}
			dirty_all(win);
			return 1;
		}

		if(cb->selected > 0) {
			--cb->selected;
			{ size_t _l = strlen(cb->items[cb->selected]); if(_l >= MKGUI_MAX_TEXT) { _l = MKGUI_MAX_TEXT - 1; } memcpy(cb->text, cb->items[cb->selected], _l); cb->text[_l] = '\0'; }
			combobox_select_all(cb);
			ev->type = MKGUI_EVENT_COMBOBOX_CHANGED;
			ev->id = win->focus_id;
			ev->value = cb->selected;
			dirty_all(win);
			return 1;
		}
		return 1;
	}

	case MKGUI_KEY_DOWN: {
		if(cb->popup_open) {
			if(cb->highlight < (int32_t)cb->filter_count - 1) {
				++cb->highlight;
			}
			for(uint32_t pi = 0; pi < win->popup_count; ++pi) {
				win->popups[pi].dirty = 1;
			}
			dirty_all(win);
			return 1;
		}

		if(cb->selected < (int32_t)cb->item_count - 1) {
			++cb->selected;
			{ size_t _l = strlen(cb->items[cb->selected]); if(_l >= MKGUI_MAX_TEXT) { _l = MKGUI_MAX_TEXT - 1; } memcpy(cb->text, cb->items[cb->selected], _l); cb->text[_l] = '\0'; }
			combobox_select_all(cb);
			ev->type = MKGUI_EVENT_COMBOBOX_CHANGED;
			ev->id = win->focus_id;
			ev->value = cb->selected;
			dirty_all(win);
			return 1;
		}
		return 1;
	}

	case MKGUI_KEY_LEFT: {
		struct mkgui_text_edit te = {cb->text, MKGUI_MAX_TEXT, cb->cursor, cb->sel_start, cb->sel_end, cb->scroll_x};
		textedit_move_left(&te, shift);
		cb->cursor = te.cursor;
		cb->sel_start = te.sel_start;
		cb->sel_end = te.sel_end;
		dirty_all(win);
		combobox_scroll_to_cursor(win, win->focus_id);
		return 1;
	}

	case MKGUI_KEY_RIGHT: {
		struct mkgui_text_edit te = {cb->text, MKGUI_MAX_TEXT, cb->cursor, cb->sel_start, cb->sel_end, cb->scroll_x};
		textedit_move_right(&te, shift);
		cb->cursor = te.cursor;
		cb->sel_start = te.sel_start;
		cb->sel_end = te.sel_end;
		dirty_all(win);
		combobox_scroll_to_cursor(win, win->focus_id);
		return 1;
	}

	case MKGUI_KEY_HOME: {
		struct mkgui_text_edit te = {cb->text, MKGUI_MAX_TEXT, cb->cursor, cb->sel_start, cb->sel_end, cb->scroll_x};
		textedit_move_home(&te, shift);
		cb->cursor = te.cursor;
		cb->sel_start = te.sel_start;
		cb->sel_end = te.sel_end;
		dirty_all(win);
		combobox_scroll_to_cursor(win, win->focus_id);
		return 1;
	}

	case MKGUI_KEY_END: {
		struct mkgui_text_edit te = {cb->text, MKGUI_MAX_TEXT, cb->cursor, cb->sel_start, cb->sel_end, cb->scroll_x};
		textedit_move_end(&te, shift);
		cb->cursor = te.cursor;
		cb->sel_start = te.sel_start;
		cb->sel_end = te.sel_end;
		dirty_all(win);
		combobox_scroll_to_cursor(win, win->focus_id);
		return 1;
	}

	case MKGUI_KEY_BACKSPACE: {
		struct mkgui_text_edit te = {cb->text, MKGUI_MAX_TEXT, cb->cursor, cb->sel_start, cb->sel_end, cb->scroll_x};
		if(textedit_backspace(&te)) {
			cb->cursor = te.cursor;
			cb->sel_start = te.sel_start;
			cb->sel_end = te.sel_end;
			cb->selected = -1;
			{ size_t _l = strlen(cb->text); if(_l >= MKGUI_MAX_TEXT) { _l = MKGUI_MAX_TEXT - 1; } memcpy(cb->prev_text, cb->text, _l); cb->prev_text[_l] = '\0'; }
			combobox_open_popup(win, win->focus_id, 1);
			ev->type = MKGUI_EVENT_COMBOBOX_CHANGED;
			ev->id = win->focus_id;
			dirty_all(win);
			combobox_scroll_to_cursor(win, win->focus_id);
			return 1;
		}
		return 1;
	}

	case MKGUI_KEY_DELETE: {
		struct mkgui_text_edit te = {cb->text, MKGUI_MAX_TEXT, cb->cursor, cb->sel_start, cb->sel_end, cb->scroll_x};
		if(textedit_delete(&te)) {
			cb->cursor = te.cursor;
			cb->sel_start = te.sel_start;
			cb->sel_end = te.sel_end;
			cb->selected = -1;
			{ size_t _l = strlen(cb->text); if(_l >= MKGUI_MAX_TEXT) { _l = MKGUI_MAX_TEXT - 1; } memcpy(cb->prev_text, cb->text, _l); cb->prev_text[_l] = '\0'; }
			combobox_open_popup(win, win->focus_id, 1);
			ev->type = MKGUI_EVENT_COMBOBOX_CHANGED;
			ev->id = win->focus_id;
			dirty_all(win);
			combobox_scroll_to_cursor(win, win->focus_id);
			return 1;
		}
		return 1;
	}

	default: {
		if(len > 0 && (uint8_t)buf[0] >= 0x20) {
			struct mkgui_text_edit te = {cb->text, MKGUI_MAX_TEXT, cb->cursor, cb->sel_start, cb->sel_end, cb->scroll_x};
			if(textedit_insert(&te, buf, (uint32_t)len)) {
				cb->cursor = te.cursor;
				cb->sel_start = te.sel_start;
				cb->sel_end = te.sel_end;
				cb->selected = -1;
				{ size_t _l = strlen(cb->text); if(_l >= MKGUI_MAX_TEXT) { _l = MKGUI_MAX_TEXT - 1; } memcpy(cb->prev_text, cb->text, _l); cb->prev_text[_l] = '\0'; }
				combobox_open_popup(win, win->focus_id, 1);
				ev->type = MKGUI_EVENT_COMBOBOX_CHANGED;
				ev->id = win->focus_id;
				dirty_all(win);
				combobox_scroll_to_cursor(win, win->focus_id);
				return 1;
			}
		}
	} break;
	}

	return 0;
}

// [=]===^=[ mkgui_combobox_setup ]================================[=]
MKGUI_API void mkgui_combobox_setup(struct mkgui_window *win, uint32_t id, const char *const *items, uint32_t count) {
	MKGUI_CHECK(win);
	if(!items && count > 0) {
		return;
	}
	struct mkgui_combobox_data *cb = find_combobox_data(win, id);
	if(!cb) {
		return;
	}
	cb->item_count = count > MKGUI_MAX_DROPDOWN ? MKGUI_MAX_DROPDOWN : count;
	for(uint32_t i = 0; i < cb->item_count; ++i) {
		strncpy(cb->items[i], items[i], MKGUI_MAX_TEXT - 1);
		cb->items[i][MKGUI_MAX_TEXT - 1] = '\0';
	}
	cb->selected = 0;
	if(cb->item_count > 0) {
		{ size_t _l = strlen(cb->items[0]); if(_l >= MKGUI_MAX_TEXT) { _l = MKGUI_MAX_TEXT - 1; } memcpy(cb->text, cb->items[0], _l); cb->text[_l] = '\0'; }
	}
	cb->cursor = (uint32_t)strlen(cb->text);
	cb->scroll_y = 0;
	dirty_all(win);
}

// [=]===^=[ mkgui_combobox_get ]=================================[=]
MKGUI_API int32_t mkgui_combobox_get(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, -1);
	struct mkgui_combobox_data *cb = find_combobox_data(win, id);
	return cb ? cb->selected : -1;
}

// [=]===^=[ mkgui_combobox_get_text ]=============================[=]
MKGUI_API const char *mkgui_combobox_get_text(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, "");
	struct mkgui_combobox_data *cb = find_combobox_data(win, id);
	return cb ? cb->text : "";
}

// [=]===^=[ mkgui_combobox_set ]=================================[=]
MKGUI_API void mkgui_combobox_set(struct mkgui_window *win, uint32_t id, int32_t index) {
	MKGUI_CHECK(win);
	struct mkgui_combobox_data *cb = find_combobox_data(win, id);
	if(!cb) {
		return;
	}

	if(index < 0 || (uint32_t)index >= cb->item_count) {
		cb->selected = -1;
		cb->text[0] = '\0';

	} else {
		cb->selected = index;
		strncpy(cb->text, cb->items[index], MKGUI_MAX_TEXT - 1);
		cb->text[MKGUI_MAX_TEXT - 1] = '\0';
	}
	cb->cursor = (uint32_t)strlen(cb->text);
	combobox_clear_selection(cb);
	dirty_all(win);
}

// [=]===^=[ mkgui_combobox_set_text ]==============================[=]
MKGUI_API void mkgui_combobox_set_text(struct mkgui_window *win, uint32_t id, const char *text) {
	MKGUI_CHECK(win);
	if(!text) {
		text = "";
	}
	struct mkgui_combobox_data *cb = find_combobox_data(win, id);
	if(!cb) {
		return;
	}
	strncpy(cb->text, text, MKGUI_MAX_TEXT - 1);
	cb->text[MKGUI_MAX_TEXT - 1] = '\0';
	cb->cursor = (uint32_t)strlen(cb->text);
	cb->selected = -1;
	for(uint32_t i = 0; i < cb->item_count; ++i) {
		if(strcmp(cb->items[i], cb->text) == 0) {
			cb->selected = (int32_t)i;
			break;
		}
	}
	combobox_clear_selection(cb);
	dirty_all(win);
}

// [=]===^=[ mkgui_combobox_get_count ]=============================[=]
MKGUI_API uint32_t mkgui_combobox_get_count(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, 0);
	struct mkgui_combobox_data *cb = find_combobox_data(win, id);
	return cb ? cb->item_count : 0;
}

// [=]===^=[ mkgui_combobox_get_item_text ]=========================[=]
MKGUI_API const char *mkgui_combobox_get_item_text(struct mkgui_window *win, uint32_t id, uint32_t index) {
	MKGUI_CHECK_VAL(win, "");
	struct mkgui_combobox_data *cb = find_combobox_data(win, id);
	if(!cb || index >= cb->item_count) {
		return "";
	}
	return cb->items[index];
}

// [=]===^=[ mkgui_combobox_add ]=================================[=]
MKGUI_API void mkgui_combobox_add(struct mkgui_window *win, uint32_t id, const char *text) {
	MKGUI_CHECK(win);
	if(!text) {
		return;
	}
	struct mkgui_combobox_data *cb = find_combobox_data(win, id);
	if(!cb || cb->item_count >= MKGUI_MAX_DROPDOWN) {
		return;
	}
	strncpy(cb->items[cb->item_count], text, MKGUI_MAX_TEXT - 1);
	cb->items[cb->item_count][MKGUI_MAX_TEXT - 1] = '\0';
	++cb->item_count;
	dirty_all(win);
}

// [=]===^=[ mkgui_combobox_remove ]================================[=]
MKGUI_API void mkgui_combobox_remove(struct mkgui_window *win, uint32_t id, uint32_t index) {
	MKGUI_CHECK(win);
	struct mkgui_combobox_data *cb = find_combobox_data(win, id);
	if(!cb || index >= cb->item_count) {
		return;
	}
	for(uint32_t i = index; i < cb->item_count - 1; ++i) {
		memcpy(cb->items[i], cb->items[i + 1], MKGUI_MAX_TEXT);
	}
	--cb->item_count;
	if(cb->selected >= (int32_t)cb->item_count) {
		cb->selected = (int32_t)cb->item_count - 1;
	}
	dirty_all(win);
}

// [=]===^=[ mkgui_combobox_clear ]=================================[=]
MKGUI_API void mkgui_combobox_clear(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK(win);
	struct mkgui_combobox_data *cb = find_combobox_data(win, id);
	if(!cb) {
		return;
	}
	cb->item_count = 0;
	cb->selected = -1;
	cb->text[0] = '\0';
	cb->cursor = 0;
	cb->scroll_y = 0;
	dirty_all(win);
}
