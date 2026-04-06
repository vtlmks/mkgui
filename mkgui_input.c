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
	struct mkgui_text_edit te = {inp->text, MKGUI_MAX_TEXT, inp->cursor, inp->sel_start, inp->sel_end, inp->scroll_x};
	textedit_delete_selection(&te);
	inp->cursor = te.cursor;
	inp->sel_start = te.sel_start;
	inp->sel_end = te.sel_end;
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
	int32_t pad = sc(ctx, 4);
	int32_t visible = ctx->rects[widx].w - pad * 2;

	char *display = inp->text;
	char masked[MKGUI_MAX_TEXT];
	struct mkgui_widget *w = &ctx->widgets[widx];
	if(w->style & MKGUI_INPUT_PASSWORD) {
		uint32_t len = (uint32_t)strlen(inp->text);
		for(uint32_t j = 0; j < len && j < MKGUI_MAX_TEXT - 1; ++j) {
			masked[j] = '*';
		}
		masked[len < MKGUI_MAX_TEXT - 1 ? len : MKGUI_MAX_TEXT - 1] = '\0';
		display = masked;
	}

	struct mkgui_text_edit te = {inp->text, MKGUI_MAX_TEXT, inp->cursor, inp->sel_start, inp->sel_end, inp->scroll_x};
	textedit_scroll_to_cursor(ctx, &te, display, visible);
	inp->scroll_x = te.scroll_x;
}

// [=]===^=[ input_hit_cursor ]=======================================[=]
static uint32_t input_hit_cursor(struct mkgui_ctx *ctx, struct mkgui_input_data *inp, char *display, int32_t rx, int32_t mx) {
	return textedit_hit_cursor(ctx, display, rx + sc(ctx, 4) - inp->scroll_x, mx);
}

// [=]===^=[ render_input ]==========================================[=]
static void render_input(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	uint32_t focused = (ctx->focus_id == w->id);
	draw_patch(ctx, MKGUI_STYLE_SUNKEN, rx, ry, rw, rh, ctx->theme.input_bg, focused ? ctx->theme.highlight : ctx->theme.widget_border);

	struct mkgui_input_data *inp = find_input_data(ctx, w->id);
	if(!inp) {
		return;
	}

	char *display = inp->text;
	char masked[MKGUI_MAX_TEXT];
	if(w->style & MKGUI_INPUT_PASSWORD) {
		uint32_t len = (uint32_t)strlen(inp->text);
		for(uint32_t j = 0; j < len && j < MKGUI_MAX_TEXT - 1; ++j) {
			masked[j] = '*';
		}
		masked[len < MKGUI_MAX_TEXT - 1 ? len : MKGUI_MAX_TEXT - 1] = '\0';
		display = masked;
	}

	int32_t ty = ry + (rh - ctx->font_height) / 2;
	uint32_t tc = (w->flags & MKGUI_DISABLED) ? ctx->theme.text_disabled : ctx->theme.text;
	int32_t tx = rx + sc(ctx, 4) - inp->scroll_x;
	int32_t pad2 = sc(ctx, 2);

	struct mkgui_text_edit te = {inp->text, MKGUI_MAX_TEXT, inp->cursor, inp->sel_start, inp->sel_end, inp->scroll_x};
	textedit_render(ctx, &te, display, tx, ty, ry + pad2, rh - pad2 * 2, rx + 1, ry + 1, rx + rw - 1, ry + rh - 1, tc, focused);
}

// [=]===^=[ handle_input_key ]======================================[=]
static uint32_t handle_input_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks, uint32_t keymod, char *buf, int32_t len) {
	struct mkgui_input_data *inp = find_input_data(ctx, ctx->focus_id);
	if(!inp) {
		return 0;
	}

	struct mkgui_widget *w = find_widget(ctx, ctx->focus_id);
	uint32_t readonly = (w && (w->style & MKGUI_INPUT_READONLY));
	uint32_t shift = (keymod & MKGUI_MOD_SHIFT);

	switch(ks) {
	case MKGUI_KEY_RETURN: {
		ctx->focus_id = 0;
		dirty_all(ctx);
		ev->type = MKGUI_EVENT_INPUT_SUBMIT;
		ev->id = w ? w->id : 0;
		return 1;
	}

	case MKGUI_KEY_LEFT: {
		struct mkgui_text_edit te = {inp->text, MKGUI_MAX_TEXT, inp->cursor, inp->sel_start, inp->sel_end, inp->scroll_x};
		textedit_move_left(&te, shift);
		inp->cursor = te.cursor;
		inp->sel_start = te.sel_start;
		inp->sel_end = te.sel_end;
		dirty_all(ctx);
		input_scroll_to_cursor(ctx, ctx->focus_id);
		return 0;
	}

	case MKGUI_KEY_RIGHT: {
		struct mkgui_text_edit te = {inp->text, MKGUI_MAX_TEXT, inp->cursor, inp->sel_start, inp->sel_end, inp->scroll_x};
		textedit_move_right(&te, shift);
		inp->cursor = te.cursor;
		inp->sel_start = te.sel_start;
		inp->sel_end = te.sel_end;
		dirty_all(ctx);
		input_scroll_to_cursor(ctx, ctx->focus_id);
		return 0;
	}

	case MKGUI_KEY_HOME: {
		struct mkgui_text_edit te = {inp->text, MKGUI_MAX_TEXT, inp->cursor, inp->sel_start, inp->sel_end, inp->scroll_x};
		textedit_move_home(&te, shift);
		inp->cursor = te.cursor;
		inp->sel_start = te.sel_start;
		inp->sel_end = te.sel_end;
		dirty_all(ctx);
		input_scroll_to_cursor(ctx, ctx->focus_id);
		return 0;
	}

	case MKGUI_KEY_END: {
		struct mkgui_text_edit te = {inp->text, MKGUI_MAX_TEXT, inp->cursor, inp->sel_start, inp->sel_end, inp->scroll_x};
		textedit_move_end(&te, shift);
		inp->cursor = te.cursor;
		inp->sel_start = te.sel_start;
		inp->sel_end = te.sel_end;
		dirty_all(ctx);
		input_scroll_to_cursor(ctx, ctx->focus_id);
		return 0;
	}

	case MKGUI_KEY_BACKSPACE: {
		if(readonly) {
			return 0;
		}
		input_undo_push_force(inp);
		struct mkgui_text_edit te = {inp->text, MKGUI_MAX_TEXT, inp->cursor, inp->sel_start, inp->sel_end, inp->scroll_x};
		if(textedit_backspace(&te)) {
			inp->cursor = te.cursor;
			inp->sel_start = te.sel_start;
			inp->sel_end = te.sel_end;
			dirty_all(ctx);
			input_scroll_to_cursor(ctx, ctx->focus_id);
			ev->type = MKGUI_EVENT_INPUT_CHANGED;
			ev->id = ctx->focus_id;
			return 1;
		}
		return 0;
	}

	case MKGUI_KEY_DELETE: {
		if(readonly) {
			return 0;
		}
		input_undo_push_force(inp);
		struct mkgui_text_edit te = {inp->text, MKGUI_MAX_TEXT, inp->cursor, inp->sel_start, inp->sel_end, inp->scroll_x};
		if(textedit_delete(&te)) {
			inp->cursor = te.cursor;
			inp->sel_start = te.sel_start;
			inp->sel_end = te.sel_end;
			dirty_all(ctx);
			input_scroll_to_cursor(ctx, ctx->focus_id);
			ev->type = MKGUI_EVENT_INPUT_CHANGED;
			ev->id = ctx->focus_id;
			return 1;
		}
		return 0;
	}

	default: {
		if(len > 0 && (uint8_t)buf[0] >= 32) {
			if(readonly) {
				return 0;
			}
			if(w && (w->style & MKGUI_INPUT_NUMERIC)) {
				for(int32_t ci = 0; ci < len; ++ci) {
					char ch = buf[ci];
					if(!((ch >= '0' && ch <= '9') || ch == '.' || ch == '-' || ch == '+' || ch == 'e' || ch == 'E')) {
						return 0;
					}
				}
			}
			input_undo_push(inp);
			struct mkgui_text_edit te = {inp->text, MKGUI_MAX_TEXT, inp->cursor, inp->sel_start, inp->sel_end, inp->scroll_x};
			if(textedit_insert(&te, buf, (uint32_t)len)) {
				inp->cursor = te.cursor;
				inp->sel_start = te.sel_start;
				inp->sel_end = te.sel_end;
				dirty_all(ctx);
				input_scroll_to_cursor(ctx, ctx->focus_id);
				ev->type = MKGUI_EVENT_INPUT_CHANGED;
				ev->id = ctx->focus_id;
				return 1;
			}
		}
	} break;
	}

	return 0;
}

// [=]===^=[ mkgui_input_set ]=======================================[=]
MKGUI_API void mkgui_input_set(struct mkgui_ctx *ctx, uint32_t id, const char *text) {
	MKGUI_CHECK(ctx);
	if(!text) {
		text = "";
	}
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
MKGUI_API char *mkgui_input_get(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, "");
	struct mkgui_input_data *inp = find_input_data(ctx, id);
	if(!inp) {
		return "";
	}
	return inp->text;
}

// [=]===^=[ mkgui_input_clear ]=====================================[=]
MKGUI_API void mkgui_input_clear(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK(ctx);
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
	MKGUI_CHECK(ctx);
	struct mkgui_widget *w = find_widget(ctx, id);
	if(!w) {
		return;
	}
	if(readonly) {
		w->style |= MKGUI_INPUT_READONLY;

	} else {
		w->style &= ~MKGUI_INPUT_READONLY;
	}
	dirty_all(ctx);
}

// [=]===^=[ mkgui_input_get_readonly ]===============================[=]
MKGUI_API uint32_t mkgui_input_get_readonly(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_widget *w = find_widget(ctx, id);
	return (w && (w->style & MKGUI_INPUT_READONLY)) ? 1 : 0;
}

// [=]===^=[ mkgui_input_get_cursor ]=================================[=]
MKGUI_API uint32_t mkgui_input_get_cursor(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_input_data *inp = find_input_data(ctx, id);
	return inp ? inp->cursor : 0;
}

// [=]===^=[ mkgui_input_set_cursor ]=================================[=]
MKGUI_API void mkgui_input_set_cursor(struct mkgui_ctx *ctx, uint32_t id, uint32_t pos) {
	MKGUI_CHECK(ctx);
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
	MKGUI_CHECK(ctx);
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
	MKGUI_CHECK(ctx);
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
	MKGUI_CHECK(ctx);
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
