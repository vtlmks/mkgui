// Copyright (c) 2026, mkgui contributors
// SPDX-License-Identifier: MIT

// [=]===^=[ render_input ]======================================[=]
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
	push_text_clip(rx + 4, ty, display, ctx->theme.text, rx + 1, ry + 1, rx + rw - 1, ry + rh - 1);

	if(ctx->focus_id == w->id) {
		char tmp[MKGUI_MAX_TEXT];
		uint32_t cpos = inp->cursor;
		if(cpos > strlen(display)) {
			cpos = (uint32_t)strlen(display);
		}
		memcpy(tmp, display, cpos);
		tmp[cpos] = '\0';
		int32_t cx = rx + 4 + text_width(ctx, tmp);
		draw_vline(ctx->pixels, ctx->win_w, ctx->win_h, cx, ry + 2, rh - 4, ctx->theme.text);
	}
}

// [=]===^=[ handle_input_key ]=================================[=]
static uint32_t handle_input_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks, const char *buf, int32_t len) {
	struct mkgui_input_data *inp = find_input_data(ctx, ctx->focus_id);
	if(!inp) {
		return 0;
	}

	struct mkgui_widget *w = find_widget(ctx, ctx->focus_id);
	if(w && (w->flags & MKGUI_READONLY)) {
		return 0;
	}

	uint32_t text_len = (uint32_t)strlen(inp->text);

	if(ks == MKGUI_KEY_LEFT) {
		if(inp->cursor > 0) {
			--inp->cursor;
		}
		dirty_all(ctx);
		return 0;

	} else if(ks == MKGUI_KEY_RIGHT) {
		if(inp->cursor < text_len) {
			++inp->cursor;
		}
		dirty_all(ctx);
		return 0;

	} else if(ks == MKGUI_KEY_HOME) {
		inp->cursor = 0;
		dirty_all(ctx);
		return 0;

	} else if(ks == MKGUI_KEY_END) {
		inp->cursor = text_len;
		dirty_all(ctx);
		return 0;

	} else if(ks == MKGUI_KEY_BACKSPACE) {
		if(inp->cursor > 0 && text_len > 0) {
			memmove(&inp->text[inp->cursor - 1], &inp->text[inp->cursor], text_len - inp->cursor + 1);
			--inp->cursor;
			dirty_all(ctx);
			ev->type = MKGUI_EVENT_INPUT_CHANGED;
			ev->id = ctx->focus_id;
			return 1;
		}
		return 0;

	} else if(ks == MKGUI_KEY_DELETE) {
		if(inp->cursor < text_len) {
			memmove(&inp->text[inp->cursor], &inp->text[inp->cursor + 1], text_len - inp->cursor);
			dirty_all(ctx);
			ev->type = MKGUI_EVENT_INPUT_CHANGED;
			ev->id = ctx->focus_id;
			return 1;
		}
		return 0;

	} else if(len > 0 && (uint8_t)buf[0] >= 32) {
		if(text_len + (uint32_t)len < MKGUI_MAX_TEXT - 1) {
			memmove(&inp->text[inp->cursor + (uint32_t)len], &inp->text[inp->cursor], text_len - inp->cursor + 1);
			memcpy(&inp->text[inp->cursor], buf, (uint32_t)len);
			inp->cursor += (uint32_t)len;
			dirty_all(ctx);
			ev->type = MKGUI_EVENT_INPUT_CHANGED;
			ev->id = ctx->focus_id;
			return 1;
		}
		return 0;
	}

	return 0;
}

// [=]===^=[ mkgui_input_set ]===================================[=]
static void mkgui_input_set(struct mkgui_ctx *ctx, uint32_t id, const char *text) {
	struct mkgui_input_data *inp = find_input_data(ctx, id);
	if(!inp) {
		return;
	}
	strncpy(inp->text, text, MKGUI_MAX_TEXT - 1);
	inp->text[MKGUI_MAX_TEXT - 1] = '\0';
	inp->cursor = (uint32_t)strlen(inp->text);
	dirty_all(ctx);
}

// [=]===^=[ mkgui_input_get ]===================================[=]
static const char *mkgui_input_get(struct mkgui_ctx *ctx, uint32_t id) {
	struct mkgui_input_data *inp = find_input_data(ctx, id);
	if(!inp) {
		return "";
	}
	return inp->text;
}
