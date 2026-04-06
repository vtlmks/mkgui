// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// ---------------------------------------------------------------------------
// Shared inline text editing primitives
// ---------------------------------------------------------------------------

// [=]===^=[ utf8_prev ]==============================================[=]
static uint32_t utf8_prev(char *text, uint32_t pos) {
	if(pos == 0) {
		return 0;
	}
	--pos;
	while(pos > 0 && ((uint8_t)text[pos] & 0xc0) == 0x80) {
		--pos;
	}
	return pos;
}

// [=]===^=[ utf8_next ]==============================================[=]
static uint32_t utf8_next(char *text, uint32_t pos) {
	if(!text[pos]) {
		return pos;
	}
	++pos;
	while(text[pos] && ((uint8_t)text[pos] & 0xc0) == 0x80) {
		++pos;
	}
	return pos;
}

// [=]===^=[ textedit_has_selection ]=================================[=]
static uint32_t textedit_has_selection(struct mkgui_text_edit *te) {
	return te->sel_start != te->sel_end;
}

// [=]===^=[ textedit_sel_range ]=====================================[=]
static void textedit_sel_range(struct mkgui_text_edit *te, uint32_t *lo, uint32_t *hi) {
	if(te->sel_start < te->sel_end) {
		*lo = te->sel_start;
		*hi = te->sel_end;
	} else {
		*lo = te->sel_end;
		*hi = te->sel_start;
	}
}

// [=]===^=[ textedit_clear_selection ]===============================[=]
static void textedit_clear_selection(struct mkgui_text_edit *te) {
	te->sel_start = te->cursor;
	te->sel_end = te->cursor;
}

// [=]===^=[ textedit_select_all ]====================================[=]
static void textedit_select_all(struct mkgui_text_edit *te) {
	uint32_t len = (uint32_t)strlen(te->text);
	te->sel_start = 0;
	te->sel_end = len;
	te->cursor = len;
}

// [=]===^=[ textedit_delete_selection ]==============================[=]
static uint32_t textedit_delete_selection(struct mkgui_text_edit *te) {
	if(!textedit_has_selection(te)) {
		return 0;
	}
	uint32_t lo, hi;
	textedit_sel_range(te, &lo, &hi);
	uint32_t text_len = (uint32_t)strlen(te->text);
	memmove(&te->text[lo], &te->text[hi], text_len - hi + 1);
	te->cursor = lo;
	te->sel_start = lo;
	te->sel_end = lo;
	return 1;
}

// [=]===^=[ textedit_move_left ]=====================================[=]
static void textedit_move_left(struct mkgui_text_edit *te, uint32_t shift) {
	if(te->cursor > 0) {
		te->cursor = utf8_prev(te->text, te->cursor);
	}
	if(shift) {
		te->sel_end = te->cursor;
	} else {
		textedit_clear_selection(te);
	}
}

// [=]===^=[ textedit_move_right ]====================================[=]
static void textedit_move_right(struct mkgui_text_edit *te, uint32_t shift) {
	uint32_t len = (uint32_t)strlen(te->text);
	if(te->cursor < len) {
		te->cursor = utf8_next(te->text, te->cursor);
	}
	if(shift) {
		te->sel_end = te->cursor;
	} else {
		textedit_clear_selection(te);
	}
}

// [=]===^=[ textedit_move_home ]=====================================[=]
static void textedit_move_home(struct mkgui_text_edit *te, uint32_t shift) {
	te->cursor = 0;
	if(shift) {
		te->sel_end = te->cursor;
	} else {
		textedit_clear_selection(te);
	}
}

// [=]===^=[ textedit_move_end ]======================================[=]
static void textedit_move_end(struct mkgui_text_edit *te, uint32_t shift) {
	te->cursor = (uint32_t)strlen(te->text);
	if(shift) {
		te->sel_end = te->cursor;
	} else {
		textedit_clear_selection(te);
	}
}

// [=]===^=[ textedit_backspace ]=====================================[=]
static uint32_t textedit_backspace(struct mkgui_text_edit *te) {
	if(textedit_has_selection(te)) {
		textedit_delete_selection(te);
		return 1;
	}
	if(te->cursor > 0) {
		uint32_t prev = utf8_prev(te->text, te->cursor);
		uint32_t text_len = (uint32_t)strlen(te->text);
		memmove(&te->text[prev], &te->text[te->cursor], text_len - te->cursor + 1);
		te->cursor = prev;
		textedit_clear_selection(te);
		return 1;
	}
	return 0;
}

// [=]===^=[ textedit_delete ]========================================[=]
static uint32_t textedit_delete(struct mkgui_text_edit *te) {
	if(textedit_has_selection(te)) {
		textedit_delete_selection(te);
		return 1;
	}
	uint32_t text_len = (uint32_t)strlen(te->text);
	if(te->cursor < text_len) {
		uint32_t next = utf8_next(te->text, te->cursor);
		memmove(&te->text[te->cursor], &te->text[next], text_len - next + 1);
		return 1;
	}
	return 0;
}

// [=]===^=[ textedit_insert ]========================================[=]
static uint32_t textedit_insert(struct mkgui_text_edit *te, char *buf, uint32_t len) {
	if(textedit_has_selection(te)) {
		textedit_delete_selection(te);
	}
	uint32_t text_len = (uint32_t)strlen(te->text);
	if(text_len + len >= te->buf_size) {
		return 0;
	}
	memmove(&te->text[te->cursor + len], &te->text[te->cursor], text_len - te->cursor + 1);
	memcpy(&te->text[te->cursor], buf, len);
	te->cursor += len;
	textedit_clear_selection(te);
	return 1;
}

// [=]===^=[ textedit_hit_cursor ]====================================[=]
static uint32_t textedit_hit_cursor(struct mkgui_ctx *ctx, char *display, int32_t base_x, int32_t mx) {
	uint32_t len = (uint32_t)strlen(display);
	char tmp[MKGUI_MAX_TEXT];
	if(len >= MKGUI_MAX_TEXT) {
		len = MKGUI_MAX_TEXT - 1;
	}
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

// [=]===^=[ textedit_scroll_to_cursor ]==============================[=]
static void textedit_scroll_to_cursor(struct mkgui_ctx *ctx, struct mkgui_text_edit *te, char *display, int32_t visible_w) {
	if(visible_w < 1) {
		return;
	}
	uint32_t dlen = (uint32_t)strlen(display);
	uint32_t cpos = te->cursor;
	if(cpos > dlen) {
		cpos = dlen;
	}
	char tmp[MKGUI_MAX_TEXT];
	if(cpos >= MKGUI_MAX_TEXT) {
		cpos = MKGUI_MAX_TEXT - 1;
	}
	memcpy(tmp, display, cpos);
	tmp[cpos] = '\0';
	int32_t cx = text_width(ctx, tmp);

	if(cx - te->scroll_x > visible_w) {
		te->scroll_x = cx - visible_w;
	}
	if(cx - te->scroll_x < 0) {
		te->scroll_x = cx;
	}

	int32_t total_w = text_width(ctx, display);
	if(total_w - te->scroll_x < visible_w && te->scroll_x > 0) {
		te->scroll_x = total_w - visible_w;
		if(te->scroll_x < 0) {
			te->scroll_x = 0;
		}
	}
}

// [=]===^=[ textedit_render ]========================================[=]
// renders selection highlight, clipped text, and cursor for inline editing.
// display may differ from te->text (e.g. password masking)
static void textedit_render(struct mkgui_ctx *ctx, struct mkgui_text_edit *te, char *display, int32_t tx, int32_t ty, int32_t sel_y, int32_t sel_h, int32_t clip_x1, int32_t clip_y1, int32_t clip_x2, int32_t clip_y2, uint32_t text_color, uint32_t focused) {
	if(focused && textedit_has_selection(te)) {
		uint32_t lo, hi;
		textedit_sel_range(te, &lo, &hi);
		char tmp[MKGUI_MAX_TEXT];
		uint32_t dlen = (uint32_t)strlen(display);
		if(lo > dlen) {
			lo = dlen;
		}
		if(hi > dlen) {
			hi = dlen;
		}

		memcpy(tmp, display, lo);
		tmp[lo] = '\0';
		int32_t sel_x1 = tx + text_width(ctx, tmp);

		memcpy(tmp, display, hi);
		tmp[hi] = '\0';
		int32_t sel_x2 = tx + text_width(ctx, tmp);

		int32_t cx1 = sel_x1 < clip_x1 ? clip_x1 : sel_x1;
		int32_t cx2 = sel_x2 > clip_x2 ? clip_x2 : sel_x2;
		if(cx2 > cx1) {
			draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, cx1, sel_y, cx2 - cx1, sel_h, ctx->theme.selection);
		}

		push_text_clip(tx, ty, display, text_color, clip_x1, clip_y1, clip_x2, clip_y2);

		uint32_t sel_len = hi - lo;
		memcpy(tmp, display + lo, sel_len);
		tmp[sel_len] = '\0';
		push_text_clip(sel_x1, ty, tmp, ctx->theme.sel_text, cx1, clip_y1, cx2, clip_y2);

	} else {
		push_text_clip(tx, ty, display, text_color, clip_x1, clip_y1, clip_x2, clip_y2);
	}

	if(focused) {
		uint32_t dlen = (uint32_t)strlen(display);
		uint32_t cpos = te->cursor;
		if(cpos > dlen) {
			cpos = dlen;
		}
		char tmp[MKGUI_MAX_TEXT];
		memcpy(tmp, display, cpos);
		tmp[cpos] = '\0';
		int32_t cx = tx + text_width(ctx, tmp);
		if(cx >= clip_x1 && cx <= clip_x2) {
			draw_vline(ctx->pixels, ctx->win_w, ctx->win_h, cx, sel_y, sel_h, ctx->theme.text);
		}
	}
}
