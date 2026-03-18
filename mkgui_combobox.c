// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define MKGUI_COMBOBOX_BTN_W 20
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
				if(a >= 'A' && a <= 'Z') { a += 32; }
				if(b >= 'A' && b <= 'Z') { b += 32; }
				if(a != b) { match = 0; }
			}
			if(match) { found = 1; }
		}
		if(found && cb->filter_count < MKGUI_MAX_DROPDOWN) {
			cb->filter_map[cb->filter_count++] = i;
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

	uint32_t disabled = (w->flags & MKGUI_DISABLED);
	uint32_t focused = (ctx->focus_id == w->id);
	uint32_t border = focused ? ctx->theme.splitter : ctx->theme.widget_border;

	int32_t text_w = rw - MKGUI_COMBOBOX_BTN_W;
	draw_patch(ctx, MKGUI_STYLE_SUNKEN, rx, ry, text_w, rh, ctx->theme.input_bg, border);

	uint32_t hovered = (!disabled && ctx->hover_id == w->id);
	uint32_t btn_bg = hovered ? ctx->theme.widget_hover : ctx->theme.widget_bg;
	draw_patch(ctx, MKGUI_STYLE_RAISED, rx + text_w, ry, MKGUI_COMBOBOX_BTN_W, rh, btn_bg, border);

	uint32_t tc = disabled ? ctx->theme.text_disabled : ctx->theme.text;
	int32_t ax = rx + text_w + 3;
	int32_t ay = ry + rh / 2 - 2;
	for(uint32_t j = 0; j < 5; ++j) {
		draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, ax + (int32_t)j, ay + (int32_t)j, 9 - (int32_t)j * 2, tc);
	}

	struct mkgui_combobox_data *cb = find_combobox_data(ctx, w->id);
	if(!cb) {
		return;
	}

	const char *display = cb->text;
	int32_t ty = ry + (rh - ctx->font_height) / 2;

	if(focused && cb->sel_all) {
		int32_t tw = text_width(ctx, display);
		int32_t sel_x = rx + 4;
		int32_t sel_w = tw < text_w - 8 ? tw : text_w - 8;
		if(sel_w > 0) {
			draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, sel_x, ry + 2, sel_w, rh - 4, ctx->theme.selection);
		}
		push_text_clip(rx + 4, ty, display, ctx->theme.sel_text, rx + 1, ry + 1, rx + text_w - 1, ry + rh - 1);
	} else {
		push_text_clip(rx + 4, ty, display, tc, rx + 1, ry + 1, rx + text_w - 1, ry + rh - 1);
	}

	if(focused && !cb->sel_all) {
		char tmp[MKGUI_MAX_TEXT];
		uint32_t cpos = cb->cursor;
		uint32_t dlen = (uint32_t)strlen(display);
		if(cpos > dlen) {
			cpos = dlen;
		}
		memcpy(tmp, display, cpos);
		tmp[cpos] = '\0';
		int32_t cx = rx + 4 + text_width(ctx, tmp);
		draw_vline(ctx->pixels, ctx->win_w, ctx->win_h, cx, ry + 2, rh - 4, ctx->theme.text);
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
		int32_t iy = 1 + (int32_t)i * MKGUI_ROW_HEIGHT - cb->scroll_y;
		if(iy + MKGUI_ROW_HEIGHT <= 0 || iy >= p->h) {
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
		draw_rect_fill(p->pixels, p->w, p->h, 1, iy, p->w - 2, MKGUI_ROW_HEIGHT, bg);
		int32_t ty = iy + (MKGUI_ROW_HEIGHT - ctx->font_height) / 2;
		uint32_t tc = ((int32_t)i == cb->highlight) ? ctx->theme.sel_text : ctx->theme.text;
		uint32_t real_idx = cb->filter_map[i];
		push_text_clip(p->x + 5, ty + p->y, cb->items[real_idx], tc, p->x + 1, p->y + 1, p->x + p->w - 1, p->y + p->h - 1);
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
	int32_t ph = (int32_t)cb->filter_count * MKGUI_ROW_HEIGHT + 2;
	int32_t max_ph = MKGUI_COMBOBOX_MAX_VISIBLE * MKGUI_ROW_HEIGHT + 2;
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
	int32_t btn_x = rx + rw - MKGUI_COMBOBOX_BTN_W;

	if(ctx->mouse_x >= btn_x) {
		combobox_open_popup(ctx, widget_id, 0);
	} else {
		cb->sel_all = 1;
		cb->cursor = (uint32_t)strlen(cb->text);
	}
	dirty_all(ctx);
	(void)ev;
}

// [=]===^=[ handle_combobox_key ]=================================[=]
static uint32_t handle_combobox_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks, uint32_t keymod, const char *buf, int32_t len) {
	struct mkgui_combobox_data *cb = find_combobox_data(ctx, ctx->focus_id);
	if(!cb) {
		return 0;
	}
	(void)keymod;

	if(ks == MKGUI_KEY_ESCAPE) {
		if(cb->popup_open) {
			{ size_t _l = strlen(cb->prev_text); if(_l >= MKGUI_MAX_TEXT) { _l = MKGUI_MAX_TEXT - 1; } memcpy(cb->text, cb->prev_text, _l); cb->text[_l] = '\0'; }
			cb->cursor = (uint32_t)strlen(cb->text);
			cb->sel_all = 0;
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
			cb->sel_all = 0;
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
			cb->cursor = (uint32_t)strlen(cb->text);
			cb->sel_all = 1;
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
			cb->cursor = (uint32_t)strlen(cb->text);
			cb->sel_all = 1;
			ev->type = MKGUI_EVENT_COMBOBOX_CHANGED;
			ev->id = ctx->focus_id;
			ev->value = cb->selected;
			dirty_all(ctx);
			return 1;
		}
		return 1;
	}

	if(ks == MKGUI_KEY_BACKSPACE) {
		if(cb->sel_all) {
			cb->text[0] = '\0';
			cb->cursor = 0;
			cb->sel_all = 0;
			cb->selected = -1;
			{ size_t _l = strlen(cb->text); if(_l >= MKGUI_MAX_TEXT) { _l = MKGUI_MAX_TEXT - 1; } memcpy(cb->prev_text, cb->text, _l); cb->prev_text[_l] = '\0'; }
			combobox_open_popup(ctx, ctx->focus_id, 1);
			ev->type = MKGUI_EVENT_COMBOBOX_CHANGED;
			ev->id = ctx->focus_id;
			dirty_all(ctx);
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
			return 1;
		}
		return 1;
	}

	if(ks == MKGUI_KEY_LEFT) {
		if(cb->sel_all) {
			cb->sel_all = 0;
			cb->cursor = 0;
		} else if(cb->cursor > 0) {
			--cb->cursor;
		}
		dirty_all(ctx);
		return 1;
	}

	if(ks == MKGUI_KEY_RIGHT) {
		if(cb->sel_all) {
			cb->sel_all = 0;
		} else if(cb->cursor < (uint32_t)strlen(cb->text)) {
			++cb->cursor;
		}
		dirty_all(ctx);
		return 1;
	}

	if(len == 1 && (uint8_t)buf[0] >= 0x20) {
		if(cb->sel_all) {
			cb->text[0] = '\0';
			cb->cursor = 0;
			cb->sel_all = 0;
		}
		uint32_t tlen = (uint32_t)strlen(cb->text);
		if(tlen < MKGUI_MAX_TEXT - 2) {
			memmove(&cb->text[cb->cursor + 1], &cb->text[cb->cursor], tlen - cb->cursor + 1);
			cb->text[cb->cursor] = buf[0];
			++cb->cursor;
			cb->selected = -1;
			{ size_t _l = strlen(cb->text); if(_l >= MKGUI_MAX_TEXT) { _l = MKGUI_MAX_TEXT - 1; } memcpy(cb->prev_text, cb->text, _l); cb->prev_text[_l] = '\0'; }
			combobox_open_popup(ctx, ctx->focus_id, 1);
			ev->type = MKGUI_EVENT_COMBOBOX_CHANGED;
			ev->id = ctx->focus_id;
			dirty_all(ctx);
			return 1;
		}
		return 1;
	}

	return 0;
}

// [=]===^=[ mkgui_combobox_setup ]================================[=]
static void mkgui_combobox_setup(struct mkgui_ctx *ctx, uint32_t id, const char **items, uint32_t count) {
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
static int32_t mkgui_combobox_get(struct mkgui_ctx *ctx, uint32_t id) {
	struct mkgui_combobox_data *cb = find_combobox_data(ctx, id);
	return cb ? cb->selected : -1;
}

// [=]===^=[ mkgui_combobox_get_text ]=============================[=]
static const char *mkgui_combobox_get_text(struct mkgui_ctx *ctx, uint32_t id) {
	struct mkgui_combobox_data *cb = find_combobox_data(ctx, id);
	return cb ? cb->text : "";
}
