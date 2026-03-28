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
	uint32_t lo = cb->sel_start < cb->sel_end ? cb->sel_start : cb->sel_end;
	uint32_t hi = cb->sel_start < cb->sel_end ? cb->sel_end : cb->sel_start;
	uint32_t tlen = (uint32_t)strlen(cb->text);
	memmove(&cb->text[lo], &cb->text[hi], tlen - hi + 1);
	cb->cursor = lo;
	cb->sel_start = lo;
	cb->sel_end = lo;
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
static uint32_t combobox_hit_cursor(struct mkgui_ctx *ctx, struct mkgui_combobox_data *cb, int32_t rx, int32_t mx) {
	int32_t base_x = rx + sc(ctx, 4) - cb->scroll_x;
	uint32_t len = (uint32_t)strlen(cb->text);
	char tmp[MKGUI_MAX_TEXT];
	for(uint32_t i = 0; i <= len; ++i) {
		memcpy(tmp, cb->text, i);
		tmp[i] = '\0';
		int32_t w = text_width(ctx, tmp);
		if(base_x + w >= mx) {
			if(i > 0) {
				tmp[i - 1] = '\0';
				int32_t prev_w = text_width(ctx, tmp);
				if(mx - (base_x + prev_w) < (base_x + w) - mx) {
					return i - 1;
				}
			}
			return i;
		}
	}
	return len;
}

// [=]===^=[ combobox_scroll_to_cursor ]============================[=]
static void combobox_scroll_to_cursor(struct mkgui_ctx *ctx, uint32_t widget_id) {
	struct mkgui_combobox_data *cb = find_combobox_data(ctx, widget_id);
	if(!cb) {
		return;
	}
	int32_t widx = find_widget_idx(ctx, widget_id);
	if(widx < 0) {
		return;
	}
	int32_t rw = ctx->rects[widx].w;
	int32_t pad = sc(ctx, 4);
	int32_t visible = rw - sc(ctx, 24) - pad * 2;
	if(visible < 1) {
		return;
	}

	char tmp[MKGUI_MAX_TEXT];
	uint32_t cpos = cb->cursor;
	uint32_t dlen = (uint32_t)strlen(cb->text);
	if(cpos > dlen) {
		cpos = dlen;
	}
	memcpy(tmp, cb->text, cpos);
	tmp[cpos] = '\0';
	int32_t cx = text_width(ctx, tmp);

	if(cx - cb->scroll_x > visible) {
		cb->scroll_x = cx - visible;
	}
	if(cx - cb->scroll_x < 0) {
		cb->scroll_x = cx;
	}

	int32_t total_w = text_width(ctx, cb->text);
	if(total_w - cb->scroll_x < visible && cb->scroll_x > 0) {
		cb->scroll_x = total_w - visible;
		if(cb->scroll_x < 0) {
			cb->scroll_x = 0;
		}
	}
}

// [=]===^=[ render_combobox ]=====================================[=]
static void render_combobox(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	int32_t btn_w = sc(ctx, 24);
	int32_t text_pad = sc(ctx, 4);
	int32_t inset2 = sc(ctx, 2);

	uint32_t disabled = (w->flags & MKGUI_DISABLED);
	uint32_t focused = (ctx->focus_id == w->id);
	uint32_t border = focused ? ctx->theme.splitter : ctx->theme.widget_border;

	int32_t text_w = rw - btn_w;
	draw_patch(ctx, MKGUI_STYLE_SUNKEN, rx, ry, text_w, rh, ctx->theme.input_bg, border);

	uint32_t hovered = (!disabled && ctx->hover_id == w->id);
	uint32_t btn_bg = hovered ? ctx->theme.widget_hover : ctx->theme.widget_bg;
	draw_patch(ctx, MKGUI_STYLE_RAISED, rx + text_w, ry, btn_w, rh, btn_bg, border);

	uint32_t tc = disabled ? ctx->theme.text_disabled : ctx->theme.text;
	int32_t ax = rx + text_w + (btn_w - 9) / 2;
	int32_t ay = ry + rh / 2 - 2;
	for(uint32_t j = 0; j < 5; ++j) {
		draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, ax + (int32_t)j, ay + (int32_t)j, 9 - (int32_t)j * 2, tc);
	}

	struct mkgui_combobox_data *cb = find_combobox_data(ctx, w->id);
	if(!cb) {
		return;
	}

	char *display = cb->text;
	int32_t ty = ry + (rh - ctx->font_height) / 2;
	int32_t tx = rx + text_pad - cb->scroll_x;

	if(focused && cb->sel_start != cb->sel_end) {
		uint32_t lo = cb->sel_start < cb->sel_end ? cb->sel_start : cb->sel_end;
		uint32_t hi = cb->sel_start < cb->sel_end ? cb->sel_end : cb->sel_start;
		char tmp[MKGUI_MAX_TEXT];

		memcpy(tmp, display, lo);
		tmp[lo] = '\0';
		int32_t sel_x1 = tx + text_width(ctx, tmp);

		memcpy(tmp, display, hi);
		tmp[hi] = '\0';
		int32_t sel_x2 = tx + text_width(ctx, tmp);

		int32_t cx1 = sel_x1 < rx + 1 ? rx + 1 : sel_x1;
		int32_t cx2 = sel_x2 > rx + text_w - 1 ? rx + text_w - 1 : sel_x2;
		if(cx2 > cx1) {
			draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, cx1, ry + inset2, cx2 - cx1, rh - inset2 * 2, ctx->theme.selection);
		}

		push_text_clip(tx, ty, display, tc, rx + 1, ry + 1, rx + text_w - 1, ry + rh - 1);

		uint32_t sel_len = hi - lo;
		memcpy(tmp, display + lo, sel_len);
		tmp[sel_len] = '\0';
		push_text_clip(sel_x1, ty, tmp, ctx->theme.sel_text, cx1, ry + 1, cx2, ry + rh - 1);

	} else {
		push_text_clip(tx, ty, display, tc, rx + 1, ry + 1, rx + text_w - 1, ry + rh - 1);
	}

	if(focused) {
		char tmp[MKGUI_MAX_TEXT];
		uint32_t cpos = cb->cursor;
		uint32_t dlen = (uint32_t)strlen(display);
		if(cpos > dlen) {
			cpos = dlen;
		}
		memcpy(tmp, display, cpos);
		tmp[cpos] = '\0';
		int32_t cx = tx + text_width(ctx, tmp);
		if(cx >= rx + 1 && cx <= rx + text_w - 1) {
			draw_vline(ctx->pixels, ctx->win_w, ctx->win_h, cx, ry + inset2, rh - inset2 * 2, ctx->theme.text);
		}
	}
}

// [=]===^=[ render_combobox_popup ]===============================[=]
static void render_combobox_popup(struct mkgui_ctx *ctx, struct mkgui_popup *p, struct mkgui_combobox_data *cb, int32_t hover_item) {
	int32_t saved_clip_x1 = render_clip_x1;
	int32_t saved_clip_y1 = render_clip_y1;
	int32_t saved_clip_x2 = render_clip_x2;
	int32_t saved_clip_y2 = render_clip_y2;
	render_clip_x1 = 0;
	render_clip_y1 = 0;
	render_clip_x2 = INT32_MAX;
	render_clip_y2 = INT32_MAX;

	draw_rounded_rect(p->pixels, p->w, p->h, 0, 0, p->w, p->h, ctx->theme.menu_bg, ctx->theme.widget_border, ctx->theme.corner_radius);

	for(uint32_t i = 0; i < cb->filter_count; ++i) {
		int32_t iy = 1 + (int32_t)i * ctx->row_height - cb->scroll_y;
		if(iy + ctx->row_height <= 0 || iy >= p->h) {
			continue;
		}
		uint32_t bg;
		if((int32_t)i == hover_item) {
			bg = ctx->theme.menu_hover;
		} else if((int32_t)i == cb->highlight) {
			bg = ctx->theme.selection;
		} else {
			bg = ctx->theme.menu_bg;
		}
		draw_rect_fill(p->pixels, p->w, p->h, 1, iy, p->w - 2, ctx->row_height, bg);
		int32_t ty = iy + (ctx->row_height - ctx->font_height) / 2;
		uint32_t tc = ((int32_t)i == cb->highlight) ? ctx->theme.sel_text : ctx->theme.text;
		uint32_t real_idx = cb->filter_map[i];
		push_text_clip(p->x + sc(ctx, 5), ty + p->y, cb->items[real_idx], tc, p->x + 1, p->y + 1, p->x + p->w - 1, p->y + p->h - 1);
	}

	render_clip_x1 = saved_clip_x1;
	render_clip_y1 = saved_clip_y1;
	render_clip_x2 = saved_clip_x2;
	render_clip_y2 = saved_clip_y2;
}

// [=]===^=[ combobox_open_popup ]=================================[=]
static void combobox_open_popup(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t filtered) {
	struct mkgui_combobox_data *cb = find_combobox_data(ctx, widget_id);
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
		popup_destroy_all(ctx);
		return;
	}

	cb->highlight = 0;
	cb->scroll_y = 0;
	cb->popup_open = 1;

	uint32_t widx = (uint32_t)find_widget_idx(ctx, widget_id);
	if(widx >= ctx->widget_count) {
		return;
	}

	int32_t abs_x, abs_y;
	platform_translate_coords(ctx, ctx->rects[widx].x, ctx->rects[widx].y + ctx->rects[widx].h, &abs_x, &abs_y);

	int32_t pw = ctx->rects[widx].w;
	int32_t ph = (int32_t)cb->filter_count * ctx->row_height + 2;
	int32_t max_ph = MKGUI_COMBOBOX_MAX_VISIBLE * ctx->row_height + 2;
	if(ph > max_ph) {
		ph = max_ph;
	}

	popup_destroy_all(ctx);
	struct mkgui_popup *p = popup_create(ctx, abs_x, abs_y, pw, ph, widget_id);
	if(p) {
		render_combobox_popup(ctx, p, cb, -1);
		flush_text_popup(ctx, p);
		platform_popup_blit(ctx, p);
		platform_flush(ctx);
	}
}

// [=]===^=[ combobox_close_popup ]================================[=]
static void combobox_close_popup(struct mkgui_ctx *ctx, struct mkgui_combobox_data *cb) {
	cb->popup_open = 0;
	popup_destroy_all(ctx);
}

// [=]===^=[ handle_combobox_click ]===============================[=]
static void handle_combobox_click(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t widget_id) {
	struct mkgui_combobox_data *cb = find_combobox_data(ctx, widget_id);
	if(!cb) {
		return;
	}

	uint32_t widx = (uint32_t)find_widget_idx(ctx, widget_id);
	if(widx >= ctx->widget_count) {
		return;
	}
	int32_t rx = ctx->rects[widx].x;
	int32_t rw = ctx->rects[widx].w;
	int32_t btn_x = rx + rw - sc(ctx, 24);

	if(ctx->mouse_x >= btn_x) {
		if(cb->popup_open) {
			combobox_close_popup(ctx, cb);
		} else {
			combobox_open_popup(ctx, widget_id, 0);
		}
	} else {
		if(ctx->focus_id != widget_id) {
			combobox_select_all(cb);
		} else {
			cb->cursor = combobox_hit_cursor(ctx, cb, rx, ctx->mouse_x);
			combobox_clear_selection(cb);
		}
	}
	dirty_all(ctx);
	combobox_scroll_to_cursor(ctx, widget_id);
	(void)ev;
}

// [=]===^=[ handle_combobox_key ]=================================[=]
static uint32_t handle_combobox_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks, uint32_t keymod, char *buf, int32_t len) {
	struct mkgui_combobox_data *cb = find_combobox_data(ctx, ctx->focus_id);
	if(!cb) {
		return 0;
	}
	uint32_t shift = (keymod & MKGUI_MOD_SHIFT);
	uint32_t text_len = (uint32_t)strlen(cb->text);

	if(ks == MKGUI_KEY_ESCAPE) {
		if(cb->popup_open) {
			{ size_t _l = strlen(cb->prev_text); if(_l >= MKGUI_MAX_TEXT) { _l = MKGUI_MAX_TEXT - 1; } memcpy(cb->text, cb->prev_text, _l); cb->text[_l] = '\0'; }
			cb->cursor = (uint32_t)strlen(cb->text);
			combobox_clear_selection(cb);
			combobox_close_popup(ctx, cb);
			dirty_all(ctx);
			return 1;
		}
		return 0;
	}

	if(ks == MKGUI_KEY_RETURN) {
		if(cb->popup_open && cb->highlight >= 0 && cb->highlight < (int32_t)cb->filter_count) {
			uint32_t real_idx = cb->filter_map[cb->highlight];
			{ size_t _l = strlen(cb->items[real_idx]); if(_l >= MKGUI_MAX_TEXT) { _l = MKGUI_MAX_TEXT - 1; } memcpy(cb->text, cb->items[real_idx], _l); cb->text[_l] = '\0'; }
			cb->selected = (int32_t)real_idx;
			cb->cursor = (uint32_t)strlen(cb->text);
			combobox_clear_selection(cb);
			combobox_close_popup(ctx, cb);
			ev->type = MKGUI_EVENT_COMBOBOX_CHANGED;
			ev->id = ctx->focus_id;
			ev->value = cb->selected;
			dirty_all(ctx);
			return 1;
		}
		combobox_close_popup(ctx, cb);
		ev->type = MKGUI_EVENT_COMBOBOX_SUBMIT;
		ev->id = ctx->focus_id;
		dirty_all(ctx);
		return 1;
	}

	if(ks == MKGUI_KEY_UP) {
		if(cb->popup_open) {
			if(cb->highlight > 0) {
				--cb->highlight;
			}
			for(uint32_t pi = 0; pi < ctx->popup_count; ++pi) {
				ctx->popups[pi].dirty = 1;
			}
			dirty_all(ctx);
			return 1;
		}
		if(cb->selected > 0) {
			--cb->selected;
			{ size_t _l = strlen(cb->items[cb->selected]); if(_l >= MKGUI_MAX_TEXT) { _l = MKGUI_MAX_TEXT - 1; } memcpy(cb->text, cb->items[cb->selected], _l); cb->text[_l] = '\0'; }
			combobox_select_all(cb);
			ev->type = MKGUI_EVENT_COMBOBOX_CHANGED;
			ev->id = ctx->focus_id;
			ev->value = cb->selected;
			dirty_all(ctx);
			return 1;
		}
		return 1;
	}

	if(ks == MKGUI_KEY_DOWN) {
		if(cb->popup_open) {
			if(cb->highlight < (int32_t)cb->filter_count - 1) {
				++cb->highlight;
			}
			for(uint32_t pi = 0; pi < ctx->popup_count; ++pi) {
				ctx->popups[pi].dirty = 1;
			}
			dirty_all(ctx);
			return 1;
		}
		if(cb->selected < (int32_t)cb->item_count - 1) {
			++cb->selected;
			{ size_t _l = strlen(cb->items[cb->selected]); if(_l >= MKGUI_MAX_TEXT) { _l = MKGUI_MAX_TEXT - 1; } memcpy(cb->text, cb->items[cb->selected], _l); cb->text[_l] = '\0'; }
			combobox_select_all(cb);
			ev->type = MKGUI_EVENT_COMBOBOX_CHANGED;
			ev->id = ctx->focus_id;
			ev->value = cb->selected;
			dirty_all(ctx);
			return 1;
		}
		return 1;
	}

	if(ks == MKGUI_KEY_LEFT) {
		if(combobox_has_selection(cb) && !shift) {
			uint32_t lo = cb->sel_start < cb->sel_end ? cb->sel_start : cb->sel_end;
			cb->cursor = lo;
			combobox_clear_selection(cb);
		} else {
			if(cb->cursor > 0) {
				--cb->cursor;
			}
			if(shift) {
				cb->sel_end = cb->cursor;
			} else {
				combobox_clear_selection(cb);
			}
		}
		dirty_all(ctx);
		combobox_scroll_to_cursor(ctx, ctx->focus_id);
		return 1;
	}

	if(ks == MKGUI_KEY_RIGHT) {
		if(combobox_has_selection(cb) && !shift) {
			uint32_t hi = cb->sel_start > cb->sel_end ? cb->sel_start : cb->sel_end;
			cb->cursor = hi;
			combobox_clear_selection(cb);
		} else {
			if(cb->cursor < text_len) {
				++cb->cursor;
			}
			if(shift) {
				cb->sel_end = cb->cursor;
			} else {
				combobox_clear_selection(cb);
			}
		}
		dirty_all(ctx);
		combobox_scroll_to_cursor(ctx, ctx->focus_id);
		return 1;
	}

	if(ks == MKGUI_KEY_HOME) {
		cb->cursor = 0;
		if(shift) {
			cb->sel_end = cb->cursor;
		} else {
			combobox_clear_selection(cb);
		}
		dirty_all(ctx);
		combobox_scroll_to_cursor(ctx, ctx->focus_id);
		return 1;
	}

	if(ks == MKGUI_KEY_END) {
		cb->cursor = text_len;
		if(shift) {
			cb->sel_end = cb->cursor;
		} else {
			combobox_clear_selection(cb);
		}
		dirty_all(ctx);
		combobox_scroll_to_cursor(ctx, ctx->focus_id);
		return 1;
	}

	if(ks == MKGUI_KEY_BACKSPACE) {
		if(combobox_has_selection(cb)) {
			combobox_delete_selection(cb);
			cb->selected = -1;
			{ size_t _l = strlen(cb->text); if(_l >= MKGUI_MAX_TEXT) { _l = MKGUI_MAX_TEXT - 1; } memcpy(cb->prev_text, cb->text, _l); cb->prev_text[_l] = '\0'; }
			combobox_open_popup(ctx, ctx->focus_id, 1);
			ev->type = MKGUI_EVENT_COMBOBOX_CHANGED;
			ev->id = ctx->focus_id;
			dirty_all(ctx);
			combobox_scroll_to_cursor(ctx, ctx->focus_id);
			return 1;
		}
		if(cb->cursor > 0) {
			uint32_t tlen = (uint32_t)strlen(cb->text);
			memmove(&cb->text[cb->cursor - 1], &cb->text[cb->cursor], tlen - cb->cursor + 1);
			--cb->cursor;
			cb->selected = -1;
			combobox_open_popup(ctx, ctx->focus_id, 1);
			ev->type = MKGUI_EVENT_COMBOBOX_CHANGED;
			ev->id = ctx->focus_id;
			dirty_all(ctx);
			combobox_scroll_to_cursor(ctx, ctx->focus_id);
			return 1;
		}
		return 1;
	}

	if(ks == MKGUI_KEY_DELETE) {
		if(combobox_has_selection(cb)) {
			combobox_delete_selection(cb);
			cb->selected = -1;
			{ size_t _l = strlen(cb->text); if(_l >= MKGUI_MAX_TEXT) { _l = MKGUI_MAX_TEXT - 1; } memcpy(cb->prev_text, cb->text, _l); cb->prev_text[_l] = '\0'; }
			combobox_open_popup(ctx, ctx->focus_id, 1);
			ev->type = MKGUI_EVENT_COMBOBOX_CHANGED;
			ev->id = ctx->focus_id;
			dirty_all(ctx);
			combobox_scroll_to_cursor(ctx, ctx->focus_id);
			return 1;
		}
		if(cb->cursor < text_len) {
			memmove(&cb->text[cb->cursor], &cb->text[cb->cursor + 1], text_len - cb->cursor);
			cb->selected = -1;
			combobox_open_popup(ctx, ctx->focus_id, 1);
			ev->type = MKGUI_EVENT_COMBOBOX_CHANGED;
			ev->id = ctx->focus_id;
			dirty_all(ctx);
			combobox_scroll_to_cursor(ctx, ctx->focus_id);
			return 1;
		}
		return 1;
	}

	if(len == 1 && (uint8_t)buf[0] >= 0x20) {
		if(combobox_has_selection(cb)) {
			combobox_delete_selection(cb);
		}
		uint32_t tlen = (uint32_t)strlen(cb->text);
		if(tlen < MKGUI_MAX_TEXT - 2) {
			memmove(&cb->text[cb->cursor + 1], &cb->text[cb->cursor], tlen - cb->cursor + 1);
			cb->text[cb->cursor] = buf[0];
			++cb->cursor;
			combobox_clear_selection(cb);
			cb->selected = -1;
			{ size_t _l = strlen(cb->text); if(_l >= MKGUI_MAX_TEXT) { _l = MKGUI_MAX_TEXT - 1; } memcpy(cb->prev_text, cb->text, _l); cb->prev_text[_l] = '\0'; }
			combobox_open_popup(ctx, ctx->focus_id, 1);
			ev->type = MKGUI_EVENT_COMBOBOX_CHANGED;
			ev->id = ctx->focus_id;
			dirty_all(ctx);
			combobox_scroll_to_cursor(ctx, ctx->focus_id);
			return 1;
		}
		return 1;
	}

	return 0;
}

// [=]===^=[ mkgui_combobox_setup ]================================[=]
MKGUI_API void mkgui_combobox_setup(struct mkgui_ctx *ctx, uint32_t id, char **items, uint32_t count) {
	MKGUI_CHECK(ctx);
	if(!items && count > 0) {
		return;
	}
	struct mkgui_combobox_data *cb = find_combobox_data(ctx, id);
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
	dirty_all(ctx);
}

// [=]===^=[ mkgui_combobox_get ]=================================[=]
MKGUI_API int32_t mkgui_combobox_get(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, -1);
	struct mkgui_combobox_data *cb = find_combobox_data(ctx, id);
	return cb ? cb->selected : -1;
}

// [=]===^=[ mkgui_combobox_get_text ]=============================[=]
MKGUI_API char *mkgui_combobox_get_text(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, "");
	struct mkgui_combobox_data *cb = find_combobox_data(ctx, id);
	return cb ? cb->text : "";
}

// [=]===^=[ mkgui_combobox_set ]=================================[=]
MKGUI_API void mkgui_combobox_set(struct mkgui_ctx *ctx, uint32_t id, int32_t index) {
	MKGUI_CHECK(ctx);
	struct mkgui_combobox_data *cb = find_combobox_data(ctx, id);
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
	dirty_all(ctx);
}

// [=]===^=[ mkgui_combobox_set_text ]==============================[=]
MKGUI_API void mkgui_combobox_set_text(struct mkgui_ctx *ctx, uint32_t id, char *text) {
	MKGUI_CHECK(ctx);
	if(!text) {
		text = "";
	}
	struct mkgui_combobox_data *cb = find_combobox_data(ctx, id);
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
	dirty_all(ctx);
}

// [=]===^=[ mkgui_combobox_get_count ]=============================[=]
MKGUI_API uint32_t mkgui_combobox_get_count(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_combobox_data *cb = find_combobox_data(ctx, id);
	return cb ? cb->item_count : 0;
}

// [=]===^=[ mkgui_combobox_get_item_text ]=========================[=]
MKGUI_API char *mkgui_combobox_get_item_text(struct mkgui_ctx *ctx, uint32_t id, uint32_t index) {
	MKGUI_CHECK_VAL(ctx, "");
	struct mkgui_combobox_data *cb = find_combobox_data(ctx, id);
	if(!cb || index >= cb->item_count) {
		return "";
	}
	return cb->items[index];
}

// [=]===^=[ mkgui_combobox_add ]=================================[=]
MKGUI_API void mkgui_combobox_add(struct mkgui_ctx *ctx, uint32_t id, char *text) {
	MKGUI_CHECK(ctx);
	if(!text) {
		return;
	}
	struct mkgui_combobox_data *cb = find_combobox_data(ctx, id);
	if(!cb || cb->item_count >= MKGUI_MAX_DROPDOWN) {
		return;
	}
	strncpy(cb->items[cb->item_count], text, MKGUI_MAX_TEXT - 1);
	cb->items[cb->item_count][MKGUI_MAX_TEXT - 1] = '\0';
	++cb->item_count;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_combobox_remove ]================================[=]
MKGUI_API void mkgui_combobox_remove(struct mkgui_ctx *ctx, uint32_t id, uint32_t index) {
	MKGUI_CHECK(ctx);
	struct mkgui_combobox_data *cb = find_combobox_data(ctx, id);
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
	dirty_all(ctx);
}

// [=]===^=[ mkgui_combobox_clear ]=================================[=]
MKGUI_API void mkgui_combobox_clear(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK(ctx);
	struct mkgui_combobox_data *cb = find_combobox_data(ctx, id);
	if(!cb) {
		return;
	}
	cb->item_count = 0;
	cb->selected = -1;
	cb->text[0] = '\0';
	cb->cursor = 0;
	cb->scroll_y = 0;
	dirty_all(ctx);
}
