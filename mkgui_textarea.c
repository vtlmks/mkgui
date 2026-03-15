// Copyright (c) 2026, mkgui contributors
// SPDX-License-Identifier: MIT

#define MKGUI_TEXTAREA_INIT_CAP 4096

// [=]===^=[ textarea_line_start ]================================[=]
static uint32_t textarea_line_start(struct mkgui_textarea_data *ta, uint32_t pos) {
	while(pos > 0 && ta->text[pos - 1] != '\n') {
		--pos;
	}
	return pos;
}

// [=]===^=[ textarea_line_end ]=================================[=]
static uint32_t textarea_line_end(struct mkgui_textarea_data *ta, uint32_t pos) {
	while(pos < ta->text_len && ta->text[pos] != '\n') {
		++pos;
	}
	return pos;
}

// [=]===^=[ textarea_cursor_line ]===============================[=]
static uint32_t textarea_cursor_line(struct mkgui_textarea_data *ta) {
	uint32_t line = 0;
	for(uint32_t i = 0; i < ta->cursor && i < ta->text_len; ++i) {
		if(ta->text[i] == '\n') {
			++line;
		}
	}
	return line;
}

// [=]===^=[ textarea_line_count ]================================[=]
static uint32_t textarea_line_count(struct mkgui_textarea_data *ta) {
	if(ta->text_len == 0) {
		return 1;
	}
	uint32_t count = 1;
	for(uint32_t i = 0; i < ta->text_len; ++i) {
		if(ta->text[i] == '\n') {
			++count;
		}
	}
	return count;
}

// [=]===^=[ textarea_ensure_cap ]================================[=]
static void textarea_ensure_cap(struct mkgui_textarea_data *ta, uint32_t needed) {
	if(needed <= ta->text_cap) {
		return;
	}
	uint32_t new_cap = ta->text_cap * 2;
	if(new_cap < needed) {
		new_cap = needed;
	}
	ta->text = (char *)realloc(ta->text, new_cap);
	ta->text_cap = new_cap;
}

// [=]===^=[ textarea_insert_char ]===============================[=]
static void textarea_insert_char(struct mkgui_textarea_data *ta, char ch) {
	textarea_ensure_cap(ta, ta->text_len + 2);
	memmove(&ta->text[ta->cursor + 1], &ta->text[ta->cursor], ta->text_len - ta->cursor);
	ta->text[ta->cursor] = ch;
	++ta->cursor;
	++ta->text_len;
	ta->text[ta->text_len] = '\0';
}

// [=]===^=[ textarea_delete_char ]===============================[=]
static void textarea_delete_char(struct mkgui_textarea_data *ta, uint32_t pos) {
	if(pos >= ta->text_len) {
		return;
	}
	memmove(&ta->text[pos], &ta->text[pos + 1], ta->text_len - pos - 1);
	--ta->text_len;
	ta->text[ta->text_len] = '\0';
}

// [=]===^=[ render_textarea ]====================================[=]
static void render_textarea(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	uint32_t focused = (ctx->focus_id == w->id);
	draw_patch(ctx, MKGUI_STYLE_SUNKEN, rx, ry, rw, rh, ctx->theme.input_bg, focused ? ctx->theme.splitter : ctx->theme.widget_border);

	struct mkgui_textarea_data *ta = find_textarea_data(ctx, w->id);
	if(!ta) {
		return;
	}

	int32_t clip_top = ry + 1;
	int32_t clip_bottom = ry + rh - 1;
	int32_t clip_left = rx + 1;
	int32_t clip_right = rx + rw - 1;

	uint32_t line = 0;
	uint32_t line_start = 0;
	uint32_t cursor_line = textarea_cursor_line(ta);
	uint32_t cursor_col = ta->cursor - textarea_line_start(ta, ta->cursor);

	for(uint32_t i = 0; i <= ta->text_len; ++i) {
		if(i == ta->text_len || ta->text[i] == '\n') {
			int32_t draw_y = ry + 1 + (int32_t)line * MKGUI_ROW_HEIGHT - ta->scroll_y;

			if(draw_y + MKGUI_ROW_HEIGHT > clip_top && draw_y < clip_bottom) {
				int32_t ty = draw_y + (MKGUI_ROW_HEIGHT - ctx->font_height) / 2;
				int32_t tx = rx + 4 - ta->scroll_x;

				if(ty >= clip_top && ty + ctx->font_height <= clip_bottom) {
					uint32_t len = i - line_start;
					char line_buf[MKGUI_MAX_TEXTAREA_LINE];
					if(len >= MKGUI_MAX_TEXTAREA_LINE) {
						len = MKGUI_MAX_TEXTAREA_LINE - 1;
					}
					memcpy(line_buf, &ta->text[line_start], len);
					line_buf[len] = '\0';
					push_text_clip(tx, ty, line_buf, ctx->theme.text, clip_left, clip_top, clip_right, clip_bottom);
				}

				if(ctx->focus_id == w->id && line == cursor_line) {
					uint32_t line_len = i - line_start;
					uint32_t clen = cursor_col;
					if(clen > line_len) {
						clen = line_len;
					}
					char cursor_buf[512];
					if(clen > sizeof(cursor_buf) - 1) {
						clen = sizeof(cursor_buf) - 1;
					}
					memcpy(cursor_buf, &ta->text[line_start], clen);
					cursor_buf[clen] = '\0';
					int32_t cx = tx + text_width(ctx, cursor_buf);
					if(cx >= clip_left && cx < clip_right) {
						draw_vline(ctx->pixels, ctx->win_w, ctx->win_h, cx, draw_y + 2, MKGUI_ROW_HEIGHT - 4, ctx->theme.sel_text);
					}
				}
			}

			line_start = i + 1;
			++line;
		}
	}

	int32_t total_lines = (int32_t)textarea_line_count(ta);
	int32_t content_h = total_lines * MKGUI_ROW_HEIGHT;
	int32_t view_h = rh - 2;
	if(content_h > view_h) {
		int32_t sb_x = rx + rw - MKGUI_SCROLLBAR_W;
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, sb_x, ry + 1, MKGUI_SCROLLBAR_W - 1, rh - 2, ctx->theme.scrollbar_bg);

		int32_t thumb_h = (int32_t)((int64_t)view_h * view_h / content_h);
		if(thumb_h < 20) {
			thumb_h = 20;
		}
		int32_t thumb_y = ry + 1 + (int32_t)((int64_t)ta->scroll_y * (view_h - thumb_h) / (content_h - view_h));
		draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, sb_x + 2, thumb_y, MKGUI_SCROLLBAR_W - 5, thumb_h, ctx->theme.scrollbar_thumb, ctx->theme.corner_radius);
	}
}

// [=]===^=[ textarea_scroll_to_cursor ]=========================[=]
static void textarea_scroll_to_cursor(struct mkgui_ctx *ctx, uint32_t widget_id) {
	struct mkgui_textarea_data *ta = find_textarea_data(ctx, widget_id);
	if(!ta) {
		return;
	}
	int32_t idx = find_widget_idx(ctx, widget_id);
	if(idx < 0) {
		return;
	}
	int32_t rh = ctx->rects[idx].h;
	int32_t view_h = rh - 2;
	int32_t cursor_line = (int32_t)textarea_cursor_line(ta);
	int32_t cursor_y = cursor_line * MKGUI_ROW_HEIGHT;

	if(cursor_y < ta->scroll_y) {
		ta->scroll_y = cursor_y;
	}
	if(cursor_y + MKGUI_ROW_HEIGHT > ta->scroll_y + view_h) {
		ta->scroll_y = cursor_y + MKGUI_ROW_HEIGHT - view_h;
	}
	if(ta->scroll_y < 0) {
		ta->scroll_y = 0;
	}
}

// [=]===^=[ handle_textarea_key ]================================[=]
static uint32_t handle_textarea_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks, const char *buf, int32_t len) {
	struct mkgui_textarea_data *ta = find_textarea_data(ctx, ctx->focus_id);
	if(!ta) {
		return 0;
	}

	if(ks == MKGUI_KEY_RETURN) {
		textarea_insert_char(ta, '\n');
		dirty_all(ctx);
		textarea_scroll_to_cursor(ctx, ctx->focus_id);
		ev->type = MKGUI_EVENT_TEXTAREA_CHANGED;
		ev->id = ctx->focus_id;
		return 1;
	}

	if(ks == MKGUI_KEY_BACKSPACE) {
		if(ta->cursor > 0) {
			--ta->cursor;
			textarea_delete_char(ta, ta->cursor);
			dirty_all(ctx);
			textarea_scroll_to_cursor(ctx, ctx->focus_id);
			ev->type = MKGUI_EVENT_TEXTAREA_CHANGED;
			ev->id = ctx->focus_id;
			return 1;
		}
		return 0;
	}

	if(ks == MKGUI_KEY_DELETE) {
		if(ta->cursor < ta->text_len) {
			textarea_delete_char(ta, ta->cursor);
			dirty_all(ctx);
			ev->type = MKGUI_EVENT_TEXTAREA_CHANGED;
			ev->id = ctx->focus_id;
			return 1;
		}
		return 0;
	}

	if(ks == MKGUI_KEY_LEFT) {
		if(ta->cursor > 0) {
			--ta->cursor;
			dirty_all(ctx);
			textarea_scroll_to_cursor(ctx, ctx->focus_id);
		}
		return 0;
	}

	if(ks == MKGUI_KEY_RIGHT) {
		if(ta->cursor < ta->text_len) {
			++ta->cursor;
			dirty_all(ctx);
			textarea_scroll_to_cursor(ctx, ctx->focus_id);
		}
		return 0;
	}

	if(ks == MKGUI_KEY_UP) {
		uint32_t ls = textarea_line_start(ta, ta->cursor);
		uint32_t col = ta->cursor - ls;
		if(ls > 0) {
			uint32_t prev_ls = textarea_line_start(ta, ls - 1);
			uint32_t prev_len = ls - 1 - prev_ls;
			ta->cursor = prev_ls + (col < prev_len ? col : prev_len);
			dirty_all(ctx);
			textarea_scroll_to_cursor(ctx, ctx->focus_id);
		}
		return 0;
	}

	if(ks == MKGUI_KEY_DOWN) {
		uint32_t le = textarea_line_end(ta, ta->cursor);
		uint32_t ls = textarea_line_start(ta, ta->cursor);
		uint32_t col = ta->cursor - ls;
		if(le < ta->text_len) {
			uint32_t next_ls = le + 1;
			uint32_t next_le = textarea_line_end(ta, next_ls);
			uint32_t next_len = next_le - next_ls;
			ta->cursor = next_ls + (col < next_len ? col : next_len);
			dirty_all(ctx);
			textarea_scroll_to_cursor(ctx, ctx->focus_id);
		}
		return 0;
	}

	if(ks == MKGUI_KEY_HOME) {
		ta->cursor = textarea_line_start(ta, ta->cursor);
		dirty_all(ctx);
		return 0;
	}

	if(ks == MKGUI_KEY_END) {
		ta->cursor = textarea_line_end(ta, ta->cursor);
		dirty_all(ctx);
		return 0;
	}

	if(len > 0 && (uint8_t)buf[0] >= 32) {
		for(int32_t i = 0; i < len; ++i) {
			textarea_insert_char(ta, buf[i]);
		}
		dirty_all(ctx);
		textarea_scroll_to_cursor(ctx, ctx->focus_id);
		ev->type = MKGUI_EVENT_TEXTAREA_CHANGED;
		ev->id = ctx->focus_id;
		return 1;
	}

	return 0;
}

// [=]===^=[ mkgui_textarea_set ]=================================[=]
static void mkgui_textarea_set(struct mkgui_ctx *ctx, uint32_t id, const char *text) {
	struct mkgui_textarea_data *ta = find_textarea_data(ctx, id);
	if(!ta) {
		return;
	}
	uint32_t slen = (uint32_t)strlen(text);
	textarea_ensure_cap(ta, slen + 1);
	memcpy(ta->text, text, slen);
	ta->text[slen] = '\0';
	ta->text_len = slen;
	ta->cursor = 0;
	ta->scroll_y = 0;
	ta->scroll_x = 0;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_textarea_get ]=================================[=]
static const char *mkgui_textarea_get(struct mkgui_ctx *ctx, uint32_t id) {
	struct mkgui_textarea_data *ta = find_textarea_data(ctx, id);
	return ta ? ta->text : "";
}
