// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ input_has_selection ]====================================[=]
static uint32_t input_has_selection(struct mkgui_input_data *inp) {
	return inp->sel_start != inp->sel_end;
}

// [=]===^=[ input_sel_range ]========================================[=]
static void input_sel_range(struct mkgui_input_data *inp, uint32_t *lo, uint32_t *hi) {
	if(inp->sel_start < inp->sel_end) {
		*lo = inp->sel_start;
		*hi = inp->sel_end;
	} else {
		*lo = inp->sel_end;
		*hi = inp->sel_start;
	}
}

// [=]===^=[ input_clear_selection ]==================================[=]
static void input_clear_selection(struct mkgui_input_data *inp) {
	inp->sel_start = inp->cursor;
	inp->sel_end = inp->cursor;
}

// [=]===^=[ input_delete_selection ]=================================[=]
static void input_delete_selection(struct mkgui_input_data *inp) {
	uint32_t lo, hi;
	input_sel_range(inp, &lo, &hi);
	uint32_t text_len = (uint32_t)strlen(inp->text);
	memmove(&inp->text[lo], &inp->text[hi], text_len - hi + 1);
	inp->cursor = lo;
	inp->sel_start = lo;
	inp->sel_end = lo;
}

// [=]===^=[ input_scroll_to_cursor ]=================================[=]
static void input_scroll_to_cursor(struct mkgui_ctx *ctx, uint32_t widget_id) {
	struct mkgui_input_data *inp = find_input_data(ctx, widget_id);
	if(!inp) {
		return;
	}
	int32_t widx = find_widget_idx(ctx, widget_id);
	if(widx < 0) {
		return;
	}
	int32_t rw = ctx->rects[widx].w;
	int32_t pad = 4;
	int32_t visible = rw - pad * 2;
	if(visible < 1) {
		return;
	}

	const char *display = inp->text;
	char masked[MKGUI_MAX_TEXT];
	struct mkgui_widget *w = &ctx->widgets[widx];
	if(w->flags & MKGUI_PASSWORD) {
		uint32_t len = (uint32_t)strlen(inp->text);
		for(uint32_t j = 0; j < len && j < MKGUI_MAX_TEXT - 1; ++j) {
			masked[j] = '*';
		}
		masked[len < MKGUI_MAX_TEXT - 1 ? len : MKGUI_MAX_TEXT - 1] = '\0';
		display = masked;
	}

	char tmp[MKGUI_MAX_TEXT];
	uint32_t cpos = inp->cursor;
	uint32_t dlen = (uint32_t)strlen(display);
	if(cpos > dlen) {
		cpos = dlen;
	}
	memcpy(tmp, display, cpos);
	tmp[cpos] = '\0';
	int32_t cx = text_width(ctx, tmp);

	if(cx - inp->scroll_x > visible) {
		inp->scroll_x = cx - visible;
	}
	if(cx - inp->scroll_x < 0) {
		inp->scroll_x = cx;
	}

	int32_t total_w = text_width(ctx, display);
	if(total_w - inp->scroll_x < visible && inp->scroll_x > 0) {
		inp->scroll_x = total_w - visible;
		if(inp->scroll_x < 0) {
			inp->scroll_x = 0;
		}
	}
}

// [=]===^=[ input_hit_cursor ]=======================================[=]
static uint32_t input_hit_cursor(struct mkgui_ctx *ctx, struct mkgui_input_data *inp, const char *display, int32_t rx, int32_t mx) {
	int32_t base_x = rx + 4 - inp->scroll_x;
	uint32_t len = (uint32_t)strlen(display);
	char tmp[MKGUI_MAX_TEXT];
	for(uint32_t i = 0; i <= len; ++i) {
		memcpy(tmp, display, i);
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

// [=]===^=[ render_input ]==========================================[=]
static void render_input(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	uint32_t focused = (ctx->focus_id == w->id);
	draw_patch(ctx, MKGUI_STYLE_SUNKEN, rx, ry, rw, rh, ctx->theme.input_bg, focused ? ctx->theme.splitter : ctx->theme.widget_border);

	struct mkgui_input_data *inp = find_input_data(ctx, w->id);
	if(!inp) {
		return;
	}

	const char *display = inp->text;
	char masked[MKGUI_MAX_TEXT];
	if(w->flags & MKGUI_PASSWORD) {
		uint32_t len = (uint32_t)strlen(inp->text);
		for(uint32_t j = 0; j < len && j < MKGUI_MAX_TEXT - 1; ++j) {
			masked[j] = '*';
		}
		masked[len < MKGUI_MAX_TEXT - 1 ? len : MKGUI_MAX_TEXT - 1] = '\0';
		display = masked;
	}
	int32_t ty = ry + (rh - ctx->font_height) / 2;
	uint32_t tc = (w->flags & MKGUI_DISABLED) ? ctx->theme.text_disabled : ctx->theme.text;
	int32_t tx = rx + 4 - inp->scroll_x;

	if(focused && input_has_selection(inp)) {
		uint32_t lo, hi;
		input_sel_range(inp, &lo, &hi);
		char tmp[MKGUI_MAX_TEXT];

		memcpy(tmp, display, lo);
		tmp[lo] = '\0';
		int32_t sel_x1 = tx + text_width(ctx, tmp);

		memcpy(tmp, display, hi);
		tmp[hi] = '\0';
		int32_t sel_x2 = tx + text_width(ctx, tmp);

		int32_t cx1 = sel_x1 < rx + 1 ? rx + 1 : sel_x1;
		int32_t cx2 = sel_x2 > rx + rw - 1 ? rx + rw - 1 : sel_x2;
		if(cx2 > cx1) {
			draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, cx1, ry + 2, cx2 - cx1, rh - 4, ctx->theme.selection);
		}

		push_text_clip(tx, ty, display, tc, rx + 1, ry + 1, rx + rw - 1, ry + rh - 1);

		if(lo > 0) {
			memcpy(tmp, display, lo);
			tmp[lo] = '\0';
		}
		uint32_t sel_len = hi - lo;
		memcpy(tmp, display + lo, sel_len);
		tmp[sel_len] = '\0';
		push_text_clip(sel_x1, ty, tmp, ctx->theme.sel_text, cx1, ry + 1, cx2, ry + rh - 1);

	} else {
		push_text_clip(tx, ty, display, tc, rx + 1, ry + 1, rx + rw - 1, ry + rh - 1);
	}

	if(focused) {
		char tmp[MKGUI_MAX_TEXT];
		uint32_t cpos = inp->cursor;
		uint32_t dlen = (uint32_t)strlen(display);
		if(cpos > dlen) {
			cpos = dlen;
		}
		memcpy(tmp, display, cpos);
		tmp[cpos] = '\0';
		int32_t cx = tx + text_width(ctx, tmp);
		if(cx >= rx + 1 && cx <= rx + rw - 1) {
			draw_vline(ctx->pixels, ctx->win_w, ctx->win_h, cx, ry + 2, rh - 4, ctx->theme.text);
		}
	}
}

// [=]===^=[ handle_input_key ]======================================[=]
static uint32_t handle_input_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks, uint32_t keymod, const char *buf, int32_t len) {
	struct mkgui_input_data *inp = find_input_data(ctx, ctx->focus_id);
	if(!inp) {
		return 0;
	}

	struct mkgui_widget *w = find_widget(ctx, ctx->focus_id);
	uint32_t readonly = (w && (w->flags & MKGUI_READONLY));
	uint32_t shift = (keymod & MKGUI_MOD_SHIFT);
	uint32_t text_len = (uint32_t)strlen(inp->text);

	if(ks == MKGUI_KEY_RETURN) {
		ctx->focus_id = 0;
		dirty_all(ctx);
		ev->type = MKGUI_EVENT_INPUT_SUBMIT;
		ev->id = w ? w->id : 0;
		return 1;
	}

	if(ks == MKGUI_KEY_LEFT) {
		if(inp->cursor > 0) {
			--inp->cursor;
		}
		if(shift) {
			inp->sel_end = inp->cursor;
		} else {
			input_clear_selection(inp);
		}
		dirty_all(ctx);
		input_scroll_to_cursor(ctx, ctx->focus_id);
		return 0;

	} else if(ks == MKGUI_KEY_RIGHT) {
		if(inp->cursor < text_len) {
			++inp->cursor;
		}
		if(shift) {
			inp->sel_end = inp->cursor;
		} else {
			input_clear_selection(inp);
		}
		dirty_all(ctx);
		input_scroll_to_cursor(ctx, ctx->focus_id);
		return 0;

	} else if(ks == MKGUI_KEY_HOME) {
		inp->cursor = 0;
		if(shift) {
			inp->sel_end = inp->cursor;
		} else {
			input_clear_selection(inp);
		}
		dirty_all(ctx);
		input_scroll_to_cursor(ctx, ctx->focus_id);
		return 0;

	} else if(ks == MKGUI_KEY_END) {
		inp->cursor = text_len;
		if(shift) {
			inp->sel_end = inp->cursor;
		} else {
			input_clear_selection(inp);
		}
		dirty_all(ctx);
		input_scroll_to_cursor(ctx, ctx->focus_id);
		return 0;

	} else if(ks == MKGUI_KEY_BACKSPACE) {
		if(readonly) {
			return 0;
		}
		if(input_has_selection(inp)) {
			input_delete_selection(inp);
			dirty_all(ctx);
			input_scroll_to_cursor(ctx, ctx->focus_id);
			ev->type = MKGUI_EVENT_INPUT_CHANGED;
			ev->id = ctx->focus_id;
			return 1;
		}
		if(inp->cursor > 0 && text_len > 0) {
			memmove(&inp->text[inp->cursor - 1], &inp->text[inp->cursor], text_len - inp->cursor + 1);
			--inp->cursor;
			input_clear_selection(inp);
			dirty_all(ctx);
			input_scroll_to_cursor(ctx, ctx->focus_id);
			ev->type = MKGUI_EVENT_INPUT_CHANGED;
			ev->id = ctx->focus_id;
			return 1;
		}
		return 0;

	} else if(ks == MKGUI_KEY_DELETE) {
		if(readonly) {
			return 0;
		}
		if(input_has_selection(inp)) {
			input_delete_selection(inp);
			dirty_all(ctx);
			input_scroll_to_cursor(ctx, ctx->focus_id);
			ev->type = MKGUI_EVENT_INPUT_CHANGED;
			ev->id = ctx->focus_id;
			return 1;
		}
		if(inp->cursor < text_len) {
			memmove(&inp->text[inp->cursor], &inp->text[inp->cursor + 1], text_len - inp->cursor);
			dirty_all(ctx);
			input_scroll_to_cursor(ctx, ctx->focus_id);
			ev->type = MKGUI_EVENT_INPUT_CHANGED;
			ev->id = ctx->focus_id;
			return 1;
		}
		return 0;

	} else if(len > 0 && (uint8_t)buf[0] >= 32) {
		if(readonly) {
			return 0;
		}
		if(input_has_selection(inp)) {
			input_delete_selection(inp);
			text_len = (uint32_t)strlen(inp->text);
		}
		if(text_len + (uint32_t)len < MKGUI_MAX_TEXT - 1) {
			memmove(&inp->text[inp->cursor + (uint32_t)len], &inp->text[inp->cursor], text_len - inp->cursor + 1);
			memcpy(&inp->text[inp->cursor], buf, (uint32_t)len);
			inp->cursor += (uint32_t)len;
			input_clear_selection(inp);
			dirty_all(ctx);
			input_scroll_to_cursor(ctx, ctx->focus_id);
			ev->type = MKGUI_EVENT_INPUT_CHANGED;
			ev->id = ctx->focus_id;
			return 1;
		}
		return 0;
	}

	return 0;
}

// [=]===^=[ mkgui_input_set ]=======================================[=]
MKGUI_API void mkgui_input_set(struct mkgui_ctx *ctx, uint32_t id, const char *text) {
	struct mkgui_input_data *inp = find_input_data(ctx, id);
	if(!inp) {
		return;
	}
	strncpy(inp->text, text, MKGUI_MAX_TEXT - 1);
	inp->text[MKGUI_MAX_TEXT - 1] = '\0';
	inp->cursor = (uint32_t)strlen(inp->text);
	inp->sel_start = inp->cursor;
	inp->sel_end = inp->cursor;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_input_get ]=======================================[=]
MKGUI_API const char *mkgui_input_get(struct mkgui_ctx *ctx, uint32_t id) {
	struct mkgui_input_data *inp = find_input_data(ctx, id);
	if(!inp) {
		return "";
	}
	return inp->text;
}

// [=]===^=[ mkgui_input_clear ]=====================================[=]
MKGUI_API void mkgui_input_clear(struct mkgui_ctx *ctx, uint32_t id) {
	struct mkgui_input_data *inp = find_input_data(ctx, id);
	if(!inp) {
		return;
	}
	inp->text[0] = '\0';
	inp->cursor = 0;
	inp->sel_start = 0;
	inp->sel_end = 0;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_input_set_readonly ]===============================[=]
MKGUI_API void mkgui_input_set_readonly(struct mkgui_ctx *ctx, uint32_t id, uint32_t readonly) {
	struct mkgui_widget *w = find_widget(ctx, id);
	if(!w) {
		return;
	}
	if(readonly) {
		w->flags |= MKGUI_READONLY;

	} else {
		w->flags &= ~MKGUI_READONLY;
	}
	dirty_all(ctx);
}

// [=]===^=[ mkgui_input_get_readonly ]===============================[=]
MKGUI_API uint32_t mkgui_input_get_readonly(struct mkgui_ctx *ctx, uint32_t id) {
	struct mkgui_widget *w = find_widget(ctx, id);
	return (w && (w->flags & MKGUI_READONLY)) ? 1 : 0;
}

// [=]===^=[ mkgui_input_get_cursor ]=================================[=]
MKGUI_API uint32_t mkgui_input_get_cursor(struct mkgui_ctx *ctx, uint32_t id) {
	struct mkgui_input_data *inp = find_input_data(ctx, id);
	return inp ? inp->cursor : 0;
}

// [=]===^=[ mkgui_input_set_cursor ]=================================[=]
MKGUI_API void mkgui_input_set_cursor(struct mkgui_ctx *ctx, uint32_t id, uint32_t pos) {
	struct mkgui_input_data *inp = find_input_data(ctx, id);
	if(!inp) {
		return;
	}
	uint32_t len = (uint32_t)strlen(inp->text);
	if(pos > len) {
		pos = len;
	}
	inp->cursor = pos;
	inp->sel_start = pos;
	inp->sel_end = pos;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_input_select_all ]=================================[=]
MKGUI_API void mkgui_input_select_all(struct mkgui_ctx *ctx, uint32_t id) {
	struct mkgui_input_data *inp = find_input_data(ctx, id);
	if(!inp) {
		return;
	}
	uint32_t len = (uint32_t)strlen(inp->text);
	inp->sel_start = 0;
	inp->sel_end = len;
	inp->cursor = len;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_input_get_selection ]==============================[=]
MKGUI_API void mkgui_input_get_selection(struct mkgui_ctx *ctx, uint32_t id, uint32_t *start, uint32_t *end) {
	struct mkgui_input_data *inp = find_input_data(ctx, id);
	if(!inp) {
		if(start) { *start = 0; }
		if(end) { *end = 0; }
		return;
	}
	uint32_t lo = inp->sel_start < inp->sel_end ? inp->sel_start : inp->sel_end;
	uint32_t hi = inp->sel_start < inp->sel_end ? inp->sel_end : inp->sel_start;
	if(start) { *start = lo; }
	if(end) { *end = hi; }
}

// [=]===^=[ mkgui_input_set_selection ]==============================[=]
MKGUI_API void mkgui_input_set_selection(struct mkgui_ctx *ctx, uint32_t id, uint32_t start, uint32_t end) {
	struct mkgui_input_data *inp = find_input_data(ctx, id);
	if(!inp) {
		return;
	}
	uint32_t len = (uint32_t)strlen(inp->text);
	if(start > len) { start = len; }
	if(end > len) { end = len; }
	inp->sel_start = start;
	inp->sel_end = end;
	inp->cursor = end;
	dirty_all(ctx);
}
