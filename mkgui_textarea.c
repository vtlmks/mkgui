// Copyright (c) 2026, Peter Fors
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

// [=]===^=[ textarea_has_selection ]==============================[=]
static uint32_t textarea_has_selection(struct mkgui_textarea_data *ta) {
	return ta->sel_start != ta->sel_end;
}

// [=]===^=[ textarea_sel_range ]=================================[=]
static void textarea_sel_range(struct mkgui_textarea_data *ta, uint32_t *lo, uint32_t *hi) {
	if(ta->sel_start < ta->sel_end) {
		*lo = ta->sel_start;
		*hi = ta->sel_end;
	} else {
		*lo = ta->sel_end;
		*hi = ta->sel_start;
	}
}

// [=]===^=[ textarea_clear_selection ]============================[=]
static void textarea_clear_selection(struct mkgui_textarea_data *ta) {
	ta->sel_start = ta->cursor;
	ta->sel_end = ta->cursor;
}

// [=]===^=[ textarea_delete_selection ]===========================[=]
static void textarea_delete_selection(struct mkgui_textarea_data *ta) {
	uint32_t lo, hi;
	textarea_sel_range(ta, &lo, &hi);
	memmove(&ta->text[lo], &ta->text[hi], ta->text_len - hi);
	ta->text_len -= (hi - lo);
	ta->text[ta->text_len] = '\0';
	ta->cursor = lo;
	ta->sel_start = lo;
	ta->sel_end = lo;
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

// [=]===^=[ textarea_pos_to_line ]================================[=]
static uint32_t textarea_pos_to_line(struct mkgui_textarea_data *ta, uint32_t pos) {
	uint32_t line = 0;
	for(uint32_t i = 0; i < pos && i < ta->text_len; ++i) {
		if(ta->text[i] == '\n') {
			++line;
		}
	}
	return line;
}

// [=]===^=[ textarea_hit_pos ]====================================[=]
static uint32_t textarea_hit_pos(struct mkgui_ctx *ctx, struct mkgui_textarea_data *ta, int32_t rx, int32_t ry, int32_t rh, int32_t mx, int32_t my) {
	int32_t local_y = my - (ry + 1) + ta->scroll_y;
	int32_t hit_line = local_y / MKGUI_ROW_HEIGHT;
	if(hit_line < 0) {
		hit_line = 0;
	}

	uint32_t line = 0;
	uint32_t line_start = 0;
	for(uint32_t i = 0; i <= ta->text_len; ++i) {
		if(i == ta->text_len || ta->text[i] == '\n') {
			if((int32_t)line == hit_line) {
				uint32_t len = i - line_start;
				int32_t base_x = rx + 4 - ta->scroll_x;
				char tmp[MKGUI_MAX_TEXTAREA_LINE];
				if(len >= MKGUI_MAX_TEXTAREA_LINE) {
					len = MKGUI_MAX_TEXTAREA_LINE - 1;
				}
				for(uint32_t j = 0; j <= len; ++j) {
					memcpy(tmp, &ta->text[line_start], j);
					tmp[j] = '\0';
					int32_t w = text_width(ctx, tmp);
					if(base_x + w >= mx) {
						if(j > 0) {
							tmp[j - 1] = '\0';
							int32_t prev_w = text_width(ctx, tmp);
							if(mx - (base_x + prev_w) < (base_x + w) - mx) {
								return line_start + j - 1;
							}
						}
						return line_start + j;
					}
				}
				return line_start + len;
			}
			line_start = i + 1;
			++line;
		}
	}
	return ta->text_len;
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

	uint32_t has_sel = (focused && textarea_has_selection(ta));
	uint32_t sel_lo = 0, sel_hi = 0;
	if(has_sel) {
		textarea_sel_range(ta, &sel_lo, &sel_hi);
	}

	uint32_t line = 0;
	uint32_t line_start = 0;
	uint32_t cursor_line = textarea_cursor_line(ta);
	uint32_t cursor_col = ta->cursor - textarea_line_start(ta, ta->cursor);

	uint32_t tc = (w->flags & MKGUI_DISABLED) ? ctx->theme.text_disabled : ctx->theme.text;

	for(uint32_t i = 0; i <= ta->text_len; ++i) {
		if(i == ta->text_len || ta->text[i] == '\n') {
			int32_t draw_y = ry + 1 + (int32_t)line * MKGUI_ROW_HEIGHT - ta->scroll_y;

			if(draw_y + MKGUI_ROW_HEIGHT > clip_top && draw_y < clip_bottom) {
				int32_t ty = draw_y + (MKGUI_ROW_HEIGHT - ctx->font_height) / 2;
				int32_t tx = rx + 4 - ta->scroll_x;

				uint32_t len = i - line_start;
				char line_buf[MKGUI_MAX_TEXTAREA_LINE];
				if(len >= MKGUI_MAX_TEXTAREA_LINE) {
					len = MKGUI_MAX_TEXTAREA_LINE - 1;
				}
				memcpy(line_buf, &ta->text[line_start], len);
				line_buf[len] = '\0';

				if(has_sel && sel_lo < i && sel_hi > line_start) {
					uint32_t s0 = sel_lo > line_start ? sel_lo - line_start : 0;
					uint32_t s1 = sel_hi < i ? sel_hi - line_start : len;

					char tmp[MKGUI_MAX_TEXTAREA_LINE];
					memcpy(tmp, line_buf, s0);
					tmp[s0] = '\0';
					int32_t sx1 = tx + text_width(ctx, tmp);

					memcpy(tmp, line_buf, s1);
					tmp[s1] = '\0';
					int32_t sx2 = tx + text_width(ctx, tmp);

					int32_t cx1 = sx1 < clip_left ? clip_left : sx1;
					int32_t cx2 = sx2 > clip_right ? clip_right : sx2;
					if(cx2 > cx1) {
						draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, cx1, draw_y, cx2 - cx1, MKGUI_ROW_HEIGHT, ctx->theme.selection);
					}

					if(ty >= clip_top && ty + ctx->font_height <= clip_bottom) {
						push_text_clip(tx, ty, line_buf, tc, clip_left, clip_top, clip_right, clip_bottom);

						char sel_buf[MKGUI_MAX_TEXTAREA_LINE];
						uint32_t sel_len = s1 - s0;
						memcpy(sel_buf, &line_buf[s0], sel_len);
						sel_buf[sel_len] = '\0';
						push_text_clip(sx1, ty, sel_buf, ctx->theme.sel_text, cx1, clip_top, cx2, clip_bottom);
					}

				} else {
					if(ty >= clip_top && ty + ctx->font_height <= clip_bottom) {
						push_text_clip(tx, ty, line_buf, tc, clip_left, clip_top, clip_right, clip_bottom);
					}
				}

				if(focused && line == cursor_line) {
					uint32_t clen = cursor_col;
					if(clen > len) {
						clen = len;
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

// [=]===^=[ textarea_has_scrollbar ]==============================[=]
static uint32_t textarea_has_scrollbar(struct mkgui_ctx *ctx, uint32_t idx, struct mkgui_textarea_data *ta) {
	int32_t rh = ctx->rects[idx].h;
	int32_t content_h = (int32_t)textarea_line_count(ta) * MKGUI_ROW_HEIGHT;
	return content_h > rh - 2;
}

// [=]===^=[ textarea_sb_hit ]========================================[=]
static int32_t textarea_sb_hit(struct mkgui_ctx *ctx, uint32_t idx, struct mkgui_textarea_data *ta, int32_t mx, int32_t my) {
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;
	int32_t sb_x = rx + rw - MKGUI_SCROLLBAR_W;
	if(mx < sb_x || mx >= rx + rw) {
		return -1;
	}
	int32_t content_h = (int32_t)textarea_line_count(ta) * MKGUI_ROW_HEIGHT;
	int32_t view_h = rh - 2;
	int32_t thumb_h = (int32_t)((int64_t)view_h * view_h / content_h);
	if(thumb_h < 20) {
		thumb_h = 20;
	}
	int32_t max_scroll = content_h - view_h;
	if(max_scroll <= 0) {
		return -1;
	}
	int32_t thumb_y = ry + 1 + (int32_t)((int64_t)ta->scroll_y * (view_h - thumb_h) / max_scroll);
	return my - thumb_y;
}

// [=]===^=[ textarea_scroll_drag ]=================================[=]
static void textarea_scroll_drag(struct mkgui_ctx *ctx, uint32_t idx, struct mkgui_textarea_data *ta, int32_t my, int32_t offset) {
	int32_t ry = ctx->rects[idx].y;
	int32_t rh = ctx->rects[idx].h;
	int32_t content_h = (int32_t)textarea_line_count(ta) * MKGUI_ROW_HEIGHT;
	int32_t view_h = rh - 2;
	int32_t thumb_h = (int32_t)((int64_t)view_h * view_h / content_h);
	if(thumb_h < 20) {
		thumb_h = 20;
	}
	int32_t max_scroll = content_h - view_h;
	if(max_scroll <= 0) {
		return;
	}
	int32_t track = view_h - thumb_h;
	if(track <= 0) {
		return;
	}
	int32_t thumb_pos = my - offset - (ry + 1);
	ta->scroll_y = (int32_t)((int64_t)thumb_pos * max_scroll / track);
	if(ta->scroll_y < 0) {
		ta->scroll_y = 0;
	}
	if(ta->scroll_y > max_scroll) {
		ta->scroll_y = max_scroll;
	}
	dirty_widget(ctx, idx);
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
static uint32_t handle_textarea_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks, uint32_t keymod, const char *buf, int32_t len) {
	struct mkgui_textarea_data *ta = find_textarea_data(ctx, ctx->focus_id);
	if(!ta) {
		return 0;
	}

	struct mkgui_widget *w = find_widget(ctx, ctx->focus_id);
	uint32_t readonly = (w && (w->style & MKGUI_READONLY));
	uint32_t shift = (keymod & MKGUI_MOD_SHIFT);

	if(ks == MKGUI_KEY_RETURN) {
		if(readonly) {
			return 0;
		}
		if(textarea_has_selection(ta)) {
			textarea_delete_selection(ta);
		}
		textarea_insert_char(ta, '\n');
		textarea_clear_selection(ta);
		dirty_widget_id(ctx, ctx->focus_id);
		textarea_scroll_to_cursor(ctx, ctx->focus_id);
		ev->type = MKGUI_EVENT_TEXTAREA_CHANGED;
		ev->id = ctx->focus_id;
		return 1;
	}

	if(ks == MKGUI_KEY_BACKSPACE) {
		if(readonly) {
			return 0;
		}
		if(textarea_has_selection(ta)) {
			textarea_delete_selection(ta);
			dirty_widget_id(ctx, ctx->focus_id);
			textarea_scroll_to_cursor(ctx, ctx->focus_id);
			ev->type = MKGUI_EVENT_TEXTAREA_CHANGED;
			ev->id = ctx->focus_id;
			return 1;
		}
		if(ta->cursor > 0) {
			uint32_t prev = utf8_prev(ta->text, ta->cursor);
			memmove(&ta->text[prev], &ta->text[ta->cursor], ta->text_len - ta->cursor + 1);
			ta->text_len -= (ta->cursor - prev);
			ta->cursor = prev;
			textarea_clear_selection(ta);
			dirty_widget_id(ctx, ctx->focus_id);
			textarea_scroll_to_cursor(ctx, ctx->focus_id);
			ev->type = MKGUI_EVENT_TEXTAREA_CHANGED;
			ev->id = ctx->focus_id;
			return 1;
		}
		return 0;
	}

	if(ks == MKGUI_KEY_DELETE) {
		if(readonly) {
			return 0;
		}
		if(textarea_has_selection(ta)) {
			textarea_delete_selection(ta);
			dirty_widget_id(ctx, ctx->focus_id);
			textarea_scroll_to_cursor(ctx, ctx->focus_id);
			ev->type = MKGUI_EVENT_TEXTAREA_CHANGED;
			ev->id = ctx->focus_id;
			return 1;
		}
		if(ta->cursor < ta->text_len) {
			uint32_t next = utf8_next(ta->text, ta->cursor);
			memmove(&ta->text[ta->cursor], &ta->text[next], ta->text_len - next + 1);
			ta->text_len -= (next - ta->cursor);
			dirty_widget_id(ctx, ctx->focus_id);
			ev->type = MKGUI_EVENT_TEXTAREA_CHANGED;
			ev->id = ctx->focus_id;
			return 1;
		}
		return 0;
	}

	if(ks == MKGUI_KEY_LEFT) {
		if(ta->cursor > 0) {
			ta->cursor = utf8_prev(ta->text, ta->cursor);
		}
		if(shift) {
			ta->sel_end = ta->cursor;
		} else {
			textarea_clear_selection(ta);
		}
		dirty_widget_id(ctx, ctx->focus_id);
		textarea_scroll_to_cursor(ctx, ctx->focus_id);
		ev->type = MKGUI_EVENT_TEXTAREA_CURSOR;
		ev->id = ctx->focus_id;
		ev->value = (int32_t)ta->cursor;
		return 1;
	}

	if(ks == MKGUI_KEY_RIGHT) {
		if(ta->cursor < ta->text_len) {
			ta->cursor = utf8_next(ta->text, ta->cursor);
		}
		if(shift) {
			ta->sel_end = ta->cursor;
		} else {
			textarea_clear_selection(ta);
		}
		dirty_widget_id(ctx, ctx->focus_id);
		textarea_scroll_to_cursor(ctx, ctx->focus_id);
		ev->type = MKGUI_EVENT_TEXTAREA_CURSOR;
		ev->id = ctx->focus_id;
		ev->value = (int32_t)ta->cursor;
		return 1;
	}

	if(ks == MKGUI_KEY_UP) {
		uint32_t ls = textarea_line_start(ta, ta->cursor);
		uint32_t col = ta->cursor - ls;
		if(ls > 0) {
			uint32_t prev_ls = textarea_line_start(ta, ls - 1);
			uint32_t prev_len = ls - 1 - prev_ls;
			ta->cursor = prev_ls + (col < prev_len ? col : prev_len);
		}
		if(shift) {
			ta->sel_end = ta->cursor;
		} else {
			textarea_clear_selection(ta);
		}
		dirty_widget_id(ctx, ctx->focus_id);
		textarea_scroll_to_cursor(ctx, ctx->focus_id);
		ev->type = MKGUI_EVENT_TEXTAREA_CURSOR;
		ev->id = ctx->focus_id;
		ev->value = (int32_t)ta->cursor;
		return 1;
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
		}
		if(shift) {
			ta->sel_end = ta->cursor;
		} else {
			textarea_clear_selection(ta);
		}
		dirty_widget_id(ctx, ctx->focus_id);
		textarea_scroll_to_cursor(ctx, ctx->focus_id);
		ev->type = MKGUI_EVENT_TEXTAREA_CURSOR;
		ev->id = ctx->focus_id;
		ev->value = (int32_t)ta->cursor;
		return 1;
	}

	if(ks == MKGUI_KEY_HOME) {
		ta->cursor = textarea_line_start(ta, ta->cursor);
		if(shift) {
			ta->sel_end = ta->cursor;
		} else {
			textarea_clear_selection(ta);
		}
		dirty_widget_id(ctx, ctx->focus_id);
		ev->type = MKGUI_EVENT_TEXTAREA_CURSOR;
		ev->id = ctx->focus_id;
		ev->value = (int32_t)ta->cursor;
		return 1;
	}

	if(ks == MKGUI_KEY_END) {
		ta->cursor = textarea_line_end(ta, ta->cursor);
		if(shift) {
			ta->sel_end = ta->cursor;
		} else {
			textarea_clear_selection(ta);
		}
		dirty_widget_id(ctx, ctx->focus_id);
		ev->type = MKGUI_EVENT_TEXTAREA_CURSOR;
		ev->id = ctx->focus_id;
		ev->value = (int32_t)ta->cursor;
		return 1;
	}

	if(len > 0 && (uint8_t)buf[0] >= 32) {
		if(readonly) {
			return 0;
		}
		if(textarea_has_selection(ta)) {
			textarea_delete_selection(ta);
		}
		uint32_t slen = (uint32_t)len;
		textarea_ensure_cap(ta, ta->text_len + slen + 1);
		memmove(&ta->text[ta->cursor + slen], &ta->text[ta->cursor], ta->text_len - ta->cursor);
		memcpy(&ta->text[ta->cursor], buf, slen);
		ta->text_len += slen;
		ta->cursor += slen;
		ta->text[ta->text_len] = '\0';
		textarea_clear_selection(ta);
		dirty_widget_id(ctx, ctx->focus_id);
		textarea_scroll_to_cursor(ctx, ctx->focus_id);
		ev->type = MKGUI_EVENT_TEXTAREA_CHANGED;
		ev->id = ctx->focus_id;
		return 1;
	}

	return 0;
}

// [=]===^=[ mkgui_textarea_set ]=================================[=]
MKGUI_API void mkgui_textarea_set(struct mkgui_ctx *ctx, uint32_t id, const char *text) {
	MKGUI_CHECK(ctx);
	if(!text) { text = ""; }
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
	ta->sel_start = 0;
	ta->sel_end = 0;
	ta->scroll_y = 0;
	ta->scroll_x = 0;
	dirty_widget_id(ctx, id);
}

// [=]===^=[ mkgui_textarea_get ]=================================[=]
MKGUI_API const char *mkgui_textarea_get(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, "");
	struct mkgui_textarea_data *ta = find_textarea_data(ctx, id);
	return ta ? ta->text : "";
}

// [=]===^=[ mkgui_textarea_set_readonly ]============================[=]
MKGUI_API void mkgui_textarea_set_readonly(struct mkgui_ctx *ctx, uint32_t id, uint32_t readonly) {
	MKGUI_CHECK(ctx);
	struct mkgui_widget *w = find_widget(ctx, id);
	if(!w) {
		return;
	}
	if(readonly) {
		w->style |= MKGUI_READONLY;

	} else {
		w->style &= ~MKGUI_READONLY;
	}
	dirty_widget_id(ctx, id);
}

// [=]===^=[ mkgui_textarea_get_readonly ]============================[=]
MKGUI_API uint32_t mkgui_textarea_get_readonly(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_widget *w = find_widget(ctx, id);
	return (w && (w->style & MKGUI_READONLY)) ? 1 : 0;
}

// [=]===^=[ mkgui_textarea_get_cursor ]==============================[=]
MKGUI_API void mkgui_textarea_get_cursor(struct mkgui_ctx *ctx, uint32_t id, uint32_t *line, uint32_t *col) {
	MKGUI_CHECK(ctx);
	struct mkgui_textarea_data *ta = find_textarea_data(ctx, id);
	if(!ta) {
		if(line) { *line = 0; }
		if(col) { *col = 0; }
		return;
	}
	if(line) { *line = textarea_cursor_line(ta); }
	if(col) { *col = ta->cursor - textarea_line_start(ta, ta->cursor); }
}

// [=]===^=[ mkgui_textarea_set_cursor ]==============================[=]
MKGUI_API void mkgui_textarea_set_cursor(struct mkgui_ctx *ctx, uint32_t id, uint32_t line, uint32_t col) {
	MKGUI_CHECK(ctx);
	struct mkgui_textarea_data *ta = find_textarea_data(ctx, id);
	if(!ta) {
		return;
	}
	uint32_t cur_line = 0;
	uint32_t pos = 0;
	while(pos < ta->text_len && cur_line < line) {
		if(ta->text[pos] == '\n') {
			++cur_line;
		}
		++pos;
	}
	uint32_t line_end = textarea_line_end(ta, pos);
	uint32_t max_col = line_end - pos;
	if(col > max_col) {
		col = max_col;
	}
	ta->cursor = pos + col;
	ta->sel_start = ta->cursor;
	ta->sel_end = ta->cursor;
	textarea_scroll_to_cursor(ctx, id);
	dirty_widget_id(ctx, id);
}

// [=]===^=[ mkgui_textarea_get_line_count ]==========================[=]
MKGUI_API uint32_t mkgui_textarea_get_line_count(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_textarea_data *ta = find_textarea_data(ctx, id);
	return ta ? textarea_line_count(ta) : 0;
}

// [=]===^=[ mkgui_textarea_get_selection ]============================[=]
MKGUI_API void mkgui_textarea_get_selection(struct mkgui_ctx *ctx, uint32_t id, uint32_t *start, uint32_t *end) {
	MKGUI_CHECK(ctx);
	struct mkgui_textarea_data *ta = find_textarea_data(ctx, id);
	if(!ta) {
		if(start) { *start = 0; }
		if(end) { *end = 0; }
		return;
	}
	uint32_t lo, hi;
	textarea_sel_range(ta, &lo, &hi);
	if(start) { *start = lo; }
	if(end) { *end = hi; }
}

// [=]===^=[ mkgui_textarea_insert ]=================================[=]
MKGUI_API void mkgui_textarea_insert(struct mkgui_ctx *ctx, uint32_t id, const char *text) {
	MKGUI_CHECK(ctx);
	if(!text) { return; }
	struct mkgui_textarea_data *ta = find_textarea_data(ctx, id);
	if(!ta) {
		return;
	}
	if(textarea_has_selection(ta)) {
		textarea_delete_selection(ta);
	}
	uint32_t slen = (uint32_t)strlen(text);
	textarea_ensure_cap(ta, ta->text_len + slen + 1);
	memmove(&ta->text[ta->cursor + slen], &ta->text[ta->cursor], ta->text_len - ta->cursor);
	memcpy(&ta->text[ta->cursor], text, slen);
	ta->text_len += slen;
	ta->cursor += slen;
	ta->text[ta->text_len] = '\0';
	ta->sel_start = ta->cursor;
	ta->sel_end = ta->cursor;
	textarea_scroll_to_cursor(ctx, id);
	dirty_widget_id(ctx, id);
}

// [=]===^=[ mkgui_textarea_append ]=================================[=]
MKGUI_API void mkgui_textarea_append(struct mkgui_ctx *ctx, uint32_t id, const char *text) {
	MKGUI_CHECK(ctx);
	if(!text) { return; }
	struct mkgui_textarea_data *ta = find_textarea_data(ctx, id);
	if(!ta) {
		return;
	}
	uint32_t slen = (uint32_t)strlen(text);
	textarea_ensure_cap(ta, ta->text_len + slen + 1);
	memcpy(&ta->text[ta->text_len], text, slen);
	ta->text_len += slen;
	ta->text[ta->text_len] = '\0';
	dirty_widget_id(ctx, id);
}

// [=]===^=[ mkgui_textarea_scroll_to_end ]==========================[=]
MKGUI_API void mkgui_textarea_scroll_to_end(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK(ctx);
	struct mkgui_textarea_data *ta = find_textarea_data(ctx, id);
	if(!ta) {
		return;
	}
	int32_t idx = find_widget_idx(ctx, id);
	if(idx < 0) {
		return;
	}
	int32_t rh = ctx->rects[idx].h;
	int32_t view_h = rh - 2;
	int32_t content_h = (int32_t)textarea_line_count(ta) * MKGUI_ROW_HEIGHT;
	int32_t max_scroll = content_h - view_h;
	if(max_scroll < 0) {
		max_scroll = 0;
	}
	ta->scroll_y = max_scroll;
	dirty_widget_id(ctx, id);
}
