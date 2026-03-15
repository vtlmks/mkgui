// Copyright (c) 2026, mkgui contributors
// SPDX-License-Identifier: MIT

// [=]===^=[ listview_header_hit ]===============================[=]
static int32_t listview_header_hit(struct mkgui_ctx *ctx, uint32_t idx, int32_t mx, int32_t my) {
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;

	struct mkgui_listview_data *lv = find_listv_data(ctx, ctx->widgets[idx].id);
	if(!lv) {
		return -1;
	}

	int32_t hh = lv->header_height > 0 ? lv->header_height : MKGUI_ROW_HEIGHT;
	if(my < ry || my >= ry + hh) {
		return -1;
	}

	int32_t cx = rx;
	for(uint32_t d = 0; d < lv->col_count; ++d) {
		uint32_t c = lv->col_order[d];
		if(mx >= cx && mx < cx + lv->columns[c].width) {
			return (int32_t)d;
		}
		cx += lv->columns[c].width;
	}
	return -1;
}

// [=]===^=[ listview_divider_hit ]================================[=]
static int32_t listview_divider_hit(struct mkgui_ctx *ctx, uint32_t idx, int32_t mx, int32_t my) {
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;

	struct mkgui_listview_data *lv = find_listv_data(ctx, ctx->widgets[idx].id);
	if(!lv) {
		return -1;
	}

	int32_t hh = lv->header_height > 0 ? lv->header_height : MKGUI_ROW_HEIGHT;
	if(my < ry || my >= ry + hh) {
		return -1;
	}

	int32_t cx = rx;
	for(uint32_t d = 0; d < lv->col_count; ++d) {
		cx += lv->columns[lv->col_order[d]].width;
		if(mx >= cx - 3 && mx <= cx + 3) {
			return (int32_t)d;
		}
	}
	return -1;
}

// [=]===^=[ listview_col_insert_pos ]============================[=]
static int32_t listview_col_insert_pos(struct mkgui_ctx *ctx, struct mkgui_listview_data *lv, uint32_t idx) {
	int32_t rx = ctx->rects[idx].x;
	int32_t mx = ctx->mouse_x;
	int32_t cx = rx + 1;
	for(uint32_t d = 0; d < lv->col_count; ++d) {
		int32_t cw = lv->columns[lv->col_order[d]].width;
		if(mx < cx + cw / 2) {
			return (int32_t)d;
		}
		cx += cw;
	}
	return (int32_t)lv->col_count;
}

// [=]===^=[ format_size ]========================================[=]
static void format_size(int64_t bytes, char *buf, uint32_t buf_size) {
	if(bytes < 1024) {
		snprintf(buf, buf_size, "%d B", (int32_t)bytes);
	} else if(bytes < 1024 * 1024) {
		snprintf(buf, buf_size, "%.1f KB", (double)bytes / 1024.0);
	} else if(bytes < 1024 * 1024 * 1024) {
		snprintf(buf, buf_size, "%.1f MB", (double)bytes / (1024.0 * 1024.0));
	} else {
		snprintf(buf, buf_size, "%.2f GB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
	}
}

// [=]===^=[ format_date ]========================================[=]
static void format_date(const char *timestamp_str, char *buf, uint32_t buf_size) {
	time_t ts = (time_t)strtoll(timestamp_str, NULL, 10);
	if(ts <= 0) {
		snprintf(buf, buf_size, "%s", timestamp_str);
		return;
	}
	struct tm *tm = localtime(&ts);
	if(!tm) {
		snprintf(buf, buf_size, "%s", timestamp_str);
		return;
	}
	strftime(buf, buf_size, "%Y-%m-%d %H:%M", tm);
}

// [=]===^=[ render_cell ]========================================[=]
static void render_cell(struct mkgui_ctx *ctx, struct mkgui_listview_data *lv, uint32_t col, const char *cell_buf, int32_t cx, int32_t ty, int32_t col_w, int32_t clip_top, int32_t clip_bottom, uint32_t tc, int32_t row_y) {
	switch(lv->columns[col].cell_type) {
		case MKGUI_CELL_PROGRESS: {
			int32_t val = 0;
			int32_t max = 100;
			const char *slash = strchr(cell_buf, '/');
			if(slash) {
				val = (int32_t)strtol(cell_buf, NULL, 10);
				max = (int32_t)strtol(slash + 1, NULL, 10);
			} else {
				val = (int32_t)strtol(cell_buf, NULL, 10);
			}
			if(max <= 0) {
				max = 100;
			}
			if(val < 0) {
				val = 0;
			}
			if(val > max) {
				val = max;
			}
			int32_t bar_x = cx + 4;
			int32_t bar_w = col_w - 8;
			int32_t bar_h = MKGUI_ROW_HEIGHT - 6;
			int32_t bar_y = row_y + 3;
			if(bar_w > 0 && bar_h > 0) {
				draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, bar_x, bar_y, bar_w, bar_h, ctx->theme.input_bg);
				int32_t fill_w = (int32_t)((int64_t)bar_w * val / max);
				if(fill_w > 0) {
					draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, bar_x, bar_y, fill_w, bar_h, ctx->theme.accent);
				}
				char pct[16];
				snprintf(pct, sizeof(pct), "%d%%", (int32_t)((int64_t)val * 100 / max));
				int32_t tw = text_width(ctx, pct);
				int32_t tx = bar_x + (bar_w - tw) / 2;
				push_text_clip(tx, ty, pct, tc, cx, clip_top, cx + col_w, clip_bottom);
			}
		} break;

		case MKGUI_CELL_ICON_TEXT: {
			const char *tab = strchr(cell_buf, '\t');
			if(tab) {
				char icon_name[64];
				uint32_t len = (uint32_t)(tab - cell_buf);
				if(len >= sizeof(icon_name)) {
					len = sizeof(icon_name) - 1;
				}
				memcpy(icon_name, cell_buf, len);
				icon_name[len] = '\0';
				int32_t ii = icon_resolve(icon_name);
				if(ii >= 0) {
					int32_t icon_y = row_y + (MKGUI_ROW_HEIGHT - icons[ii].h) / 2;
					draw_icon(ctx->pixels, ctx->win_w, ctx->win_h, &icons[ii], cx + 4, icon_y, cx, clip_top, cx + col_w, clip_bottom);
					push_text_clip(cx + 4 + icons[ii].w + 4, ty, tab + 1, tc, cx, clip_top, cx + col_w, clip_bottom);
				} else {
					push_text_clip(cx + 4, ty, tab + 1, tc, cx, clip_top, cx + col_w, clip_bottom);
				}
			} else {
				push_text_clip(cx + 4, ty, cell_buf, tc, cx, clip_top, cx + col_w, clip_bottom);
			}
		} break;

		case MKGUI_CELL_SIZE: {
			char fmt[32];
			int64_t bytes = strtoll(cell_buf, NULL, 10);
			format_size(bytes, fmt, sizeof(fmt));
			int32_t tw = text_width(ctx, fmt);
			int32_t tx = cx + col_w - tw - 4;
			push_text_clip(tx, ty, fmt, tc, cx, clip_top, cx + col_w, clip_bottom);
		} break;

		case MKGUI_CELL_DATE: {
			char fmt[32];
			format_date(cell_buf, fmt, sizeof(fmt));
			push_text_clip(cx + 4, ty, fmt, tc, cx, clip_top, cx + col_w, clip_bottom);
		} break;

		default: {
			push_text_clip(cx + 4, ty, cell_buf, tc, cx, clip_top, cx + col_w, clip_bottom);
		} break;
	}
}

// [=]===^=[ render_listview ]===================================[=]
static void render_listview(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	struct mkgui_listview_data *lv = find_listv_data(ctx, w->id);
	if(!lv) {
		return;
	}

	uint32_t focused = (ctx->focus_id == w->id);
	draw_patch(ctx, MKGUI_STYLE_SUNKEN, rx, ry, rw, rh, ctx->theme.input_bg, focused ? ctx->theme.splitter : ctx->theme.widget_border);

	int32_t hh = lv->header_height > 0 ? lv->header_height : MKGUI_ROW_HEIGHT;
	draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx + 1, ry + 1, rw - 2, hh, ctx->theme.header_bg);

	int32_t cx = rx + 1;
	for(uint32_t d = 0; d < lv->col_count; ++d) {
		uint32_t c = lv->col_order[d];
		int32_t cw = lv->columns[c].width;
		int32_t ty = ry + (hh - ctx->font_height) / 2;
		push_text_clip(cx + 4, ty, lv->columns[c].label, ctx->theme.text, cx, ry, cx + cw, ry + hh);

		if(lv->sort_col == (int32_t)c) {
			int32_t ax = cx + cw - 12;
			int32_t ay = ry + hh / 2;
			if(lv->sort_dir > 0) {
				for(int32_t j = 0; j < 4; ++j) {
					draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, ax + j, ay - j + 2, 7 - j * 2, ctx->theme.text);
				}

			} else {
				for(int32_t j = 0; j < 4; ++j) {
					draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, ax + j, ay + j - 2, 7 - j * 2, ctx->theme.text);
				}
			}
		}

		cx += cw;
		draw_vline(ctx->pixels, ctx->win_w, ctx->win_h, cx, ry + 1, hh, ctx->theme.widget_border);
	}
	draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, rx + 1, ry + hh, rw - 2, ctx->theme.widget_border);

	// Column drag insertion indicator
	if(ctx->drag_col_id == w->id && ctx->drag_col_src >= 0) {
		int32_t insert_d = listview_col_insert_pos(ctx, lv, idx);
		if(insert_d >= 0) {
			int32_t ix = rx + 1;
			for(int32_t d = 0; d < insert_d; ++d) {
				ix += lv->columns[lv->col_order[d]].width;
			}
			draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ix - 1, ry + 1, 3, hh, ctx->theme.splitter);
		}
	}

	int32_t content_y = ry + hh + 1;
	int32_t content_h = rh - hh - 2;
	int32_t visible_rows = content_h / MKGUI_ROW_HEIGHT;
	int32_t first_row = lv->scroll_y / MKGUI_ROW_HEIGHT;

	int32_t clip_top = content_y;
	int32_t clip_bottom = content_y + content_h;

	char cell_buf[MKGUI_MAX_TEXT];
	for(int32_t r = 0; r <= visible_rows + 1 && (first_row + r) < (int32_t)lv->row_count; ++r) {
		int32_t row_idx = first_row + r;
		int32_t row_y = content_y + r * MKGUI_ROW_HEIGHT - (lv->scroll_y % MKGUI_ROW_HEIGHT);

		if(row_y >= clip_bottom || row_y + MKGUI_ROW_HEIGHT <= clip_top) {
			continue;
		}

		int32_t draw_y = row_y < clip_top ? clip_top : row_y;
		int32_t draw_end = row_y + MKGUI_ROW_HEIGHT;
		if(draw_end > clip_bottom) {
			draw_end = clip_bottom;
		}
		int32_t draw_h = draw_end - draw_y;

		uint32_t row_bg = (row_idx & 1) ? ctx->theme.listview_alt : ctx->theme.input_bg;
		if(row_idx == lv->selected_row) {
			row_bg = ctx->theme.selection;
		}
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx + 1, draw_y, rw - 2 - MKGUI_SCROLLBAR_W, draw_h, row_bg);

		int32_t ty = row_y + (MKGUI_ROW_HEIGHT - ctx->font_height) / 2;
		if(ty >= clip_top && ty + ctx->font_height <= clip_bottom) {
			cx = rx + 1;
			for(uint32_t d = 0; d < lv->col_count; ++d) {
				uint32_t c = lv->col_order[d];
				cell_buf[0] = '\0';
				if(lv->row_cb) {
					lv->row_cb((uint32_t)row_idx, c, cell_buf, sizeof(cell_buf), lv->userdata);
				}
				uint32_t tc = (row_idx == lv->selected_row) ? ctx->theme.sel_text : ctx->theme.text;
				int32_t col_w = lv->columns[c].width;
				render_cell(ctx, lv, c, cell_buf, cx, ty, col_w, clip_top, clip_bottom, tc, row_y);
				cx += col_w;
			}
		}
	}

	if(lv->row_count > 0) {
		int32_t sb_x = rx + rw - MKGUI_SCROLLBAR_W - 1;
		int32_t sb_y = content_y;
		int32_t sb_h = content_h;
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, sb_x, sb_y, MKGUI_SCROLLBAR_W, sb_h, ctx->theme.scrollbar_bg);

		int32_t total_h = (int32_t)lv->row_count * MKGUI_ROW_HEIGHT;
		if(total_h > sb_h) {
			int32_t thumb_h = sb_h * sb_h / total_h;
			if(thumb_h < 20) {
				thumb_h = 20;
			}
			int32_t thumb_y = sb_y + (int32_t)((int64_t)lv->scroll_y * (sb_h - thumb_h) / (total_h - sb_h));
			uint32_t thumb_color = (ctx->drag_scrollbar_id == w->id) ? ctx->theme.widget_hover : ctx->theme.scrollbar_thumb;
			draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, sb_x + 2, thumb_y, MKGUI_SCROLLBAR_W - 4, thumb_h, thumb_color, ctx->theme.corner_radius);
		}
	}

}

// [=]===^=[ listview_row_hit ]=================================[=]
static int32_t listview_row_hit(struct mkgui_ctx *ctx, uint32_t idx, int32_t mx, int32_t my) {
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	struct mkgui_listview_data *lv = find_listv_data(ctx, ctx->widgets[idx].id);
	if(!lv) {
		return -1;
	}

	int32_t hh = lv->header_height > 0 ? lv->header_height : MKGUI_ROW_HEIGHT;
	int32_t content_y = ry + hh + 1;
	int32_t sb_x = rx + rw - MKGUI_SCROLLBAR_W - 1;

	if(mx >= sb_x) {
		return -1;
	}
	if(my < content_y || my >= ry + rh) {
		return -1;
	}

	int32_t row = (my - content_y + lv->scroll_y) / MKGUI_ROW_HEIGHT;
	if(row < 0 || row >= (int32_t)lv->row_count) {
		return -1;
	}
	return row;
}

// [=]===^=[ listview_scrollbar_hit ]============================[=]
// returns: 0=miss, 1=on thumb, 2=above thumb, 3=below thumb
static uint32_t listview_scrollbar_hit(struct mkgui_ctx *ctx, uint32_t idx, int32_t mx, int32_t my) {
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	struct mkgui_listview_data *lv = find_listv_data(ctx, ctx->widgets[idx].id);
	if(!lv) {
		return 0;
	}

	int32_t hh = lv->header_height > 0 ? lv->header_height : MKGUI_ROW_HEIGHT;
	int32_t sb_x = rx + rw - MKGUI_SCROLLBAR_W - 1;
	int32_t sb_y = ry + hh + 1;
	int32_t sb_h = rh - hh - 2;

	if(mx < sb_x || mx >= sb_x + MKGUI_SCROLLBAR_W || my < sb_y || my >= sb_y + sb_h) {
		return 0;
	}

	int32_t total_h = (int32_t)lv->row_count * MKGUI_ROW_HEIGHT;
	if(total_h <= sb_h) {
		return 1;
	}

	int32_t thumb_h = sb_h * sb_h / total_h;
	if(thumb_h < 20) {
		thumb_h = 20;
	}
	int32_t max_scroll = total_h - sb_h;
	int32_t thumb_y = sb_y + (int32_t)((int64_t)lv->scroll_y * (sb_h - thumb_h) / max_scroll);

	if(my < thumb_y) {
		return 2;
	}
	if(my >= thumb_y + thumb_h) {
		return 3;
	}
	return 1;
}

// [=]===^=[ listview_thumb_offset ]==============================[=]
static int32_t listview_thumb_offset(struct mkgui_ctx *ctx, uint32_t idx, int32_t mouse_y) {
	struct mkgui_listview_data *lv = find_listv_data(ctx, ctx->widgets[idx].id);
	if(!lv) {
		return 0;
	}

	int32_t ry = ctx->rects[idx].y;
	int32_t rh = ctx->rects[idx].h;
	int32_t hh = lv->header_height > 0 ? lv->header_height : MKGUI_ROW_HEIGHT;
	int32_t sb_y = ry + hh + 1;
	int32_t sb_h = rh - hh - 2;

	int32_t total_h = (int32_t)lv->row_count * MKGUI_ROW_HEIGHT;
	if(total_h <= sb_h) {
		return 0;
	}

	int32_t thumb_h = sb_h * sb_h / total_h;
	if(thumb_h < 20) {
		thumb_h = 20;
	}
	int32_t max_scroll = total_h - sb_h;
	int32_t thumb_y = sb_y + (int32_t)((int64_t)lv->scroll_y * (sb_h - thumb_h) / max_scroll);
	return mouse_y - thumb_y;
}

// [=]===^=[ listview_page_scroll ]================================[=]
static void listview_page_scroll(struct mkgui_ctx *ctx, uint32_t idx, int32_t direction) {
	struct mkgui_listview_data *lv = find_listv_data(ctx, ctx->widgets[idx].id);
	if(!lv) {
		return;
	}

	int32_t rh = ctx->rects[idx].h;
	int32_t hh = lv->header_height > 0 ? lv->header_height : MKGUI_ROW_HEIGHT;
	int32_t content_h = rh - hh - 2;

	int32_t total_h = (int32_t)lv->row_count * MKGUI_ROW_HEIGHT;
	int32_t max_scroll = total_h - content_h;
	if(max_scroll < 0) {
		max_scroll = 0;
	}

	int32_t page = (content_h / MKGUI_ROW_HEIGHT) * MKGUI_ROW_HEIGHT;
	lv->scroll_y += direction * page;
	if(lv->scroll_y < 0) {
		lv->scroll_y = 0;
	}
	if(lv->scroll_y > max_scroll) {
		lv->scroll_y = max_scroll;
	}
	dirty_all(ctx);
}

// [=]===^=[ listview_scroll_to_y ]==============================[=]
static void listview_scroll_to_y(struct mkgui_ctx *ctx, uint32_t widget_id, int32_t mouse_y) {
	struct mkgui_listview_data *lv = find_listv_data(ctx, widget_id);
	if(!lv) {
		return;
	}

	int32_t idx = find_widget_idx(ctx, widget_id);
	if(idx < 0) {
		return;
	}

	int32_t ry = ctx->rects[idx].y;
	int32_t rh = ctx->rects[idx].h;
	int32_t hh = lv->header_height > 0 ? lv->header_height : MKGUI_ROW_HEIGHT;
	int32_t content_y = ry + hh + 1;
	int32_t content_h = rh - hh - 2;

	int32_t total_h = (int32_t)lv->row_count * MKGUI_ROW_HEIGHT;
	int32_t max_scroll = total_h - content_h;
	if(max_scroll < 0) {
		max_scroll = 0;
	}
	if(content_h <= 0) {
		return;
	}

	int32_t thumb_h = content_h * content_h / total_h;
	if(thumb_h < 20) {
		thumb_h = 20;
	}
	int32_t track = content_h - thumb_h;
	if(track <= 0) {
		return;
	}
	float frac = (float)(mouse_y - content_y - ctx->drag_scrollbar_offset) / (float)track;
	if(frac < 0.0f) {
		frac = 0.0f;
	}
	if(frac > 1.0f) {
		frac = 1.0f;
	}

	lv->scroll_y = (int32_t)(frac * (float)max_scroll);
	lv->scroll_y = (lv->scroll_y / MKGUI_ROW_HEIGHT) * MKGUI_ROW_HEIGHT;
	dirty_all(ctx);
}

// [=]===^=[ handle_listview_key ]===============================[=]
static uint32_t handle_listview_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks) {
	struct mkgui_listview_data *lv = find_listv_data(ctx, ctx->focus_id);
	if(!lv || lv->row_count == 0) {
		return 0;
	}

	int32_t idx = find_widget_idx(ctx, ctx->focus_id);
	if(idx < 0) {
		return 0;
	}

	int32_t rh = ctx->rects[idx].h;
	int32_t hh = lv->header_height > 0 ? lv->header_height : MKGUI_ROW_HEIGHT;
	int32_t content_h = rh - hh - 2;
	int32_t visible_rows = content_h / MKGUI_ROW_HEIGHT;
	int32_t total_h = (int32_t)lv->row_count * MKGUI_ROW_HEIGHT;
	int32_t max_scroll = total_h - content_h;
	if(max_scroll < 0) {
		max_scroll = 0;
	}

	if(lv->selected_row < 0 && (ks == MKGUI_KEY_UP || ks == MKGUI_KEY_DOWN || ks == MKGUI_KEY_PAGE_UP || ks == MKGUI_KEY_PAGE_DOWN || ks == MKGUI_KEY_HOME || ks == MKGUI_KEY_END)) {
		lv->selected_row = 0;
		lv->scroll_y = 0;
		dirty_all(ctx);
		ev->type = MKGUI_EVENT_LISTVIEW_SELECT;
		ev->id = ctx->focus_id;
		ev->value = 0;
		return 1;
	}

	if(ks == MKGUI_KEY_UP) {
		if(lv->selected_row > 0) {
			--lv->selected_row;
			int32_t row_y = lv->selected_row * MKGUI_ROW_HEIGHT;
			if(row_y < lv->scroll_y) {
				lv->scroll_y = row_y;
			}
			dirty_all(ctx);
			ev->type = MKGUI_EVENT_LISTVIEW_SELECT;
			ev->id = ctx->focus_id;
			ev->value = lv->selected_row;
			return 1;
		}

	} else if(ks == MKGUI_KEY_DOWN) {
		if(lv->selected_row < (int32_t)lv->row_count - 1) {
			++lv->selected_row;
			int32_t row_bottom = (lv->selected_row + 1) * MKGUI_ROW_HEIGHT;
			if(row_bottom > lv->scroll_y + content_h) {
				lv->scroll_y = ((row_bottom - content_h + MKGUI_ROW_HEIGHT - 1) / MKGUI_ROW_HEIGHT) * MKGUI_ROW_HEIGHT;
			}
			dirty_all(ctx);
			ev->type = MKGUI_EVENT_LISTVIEW_SELECT;
			ev->id = ctx->focus_id;
			ev->value = lv->selected_row;
			return 1;
		}

	} else if(ks == MKGUI_KEY_PAGE_UP) {
		lv->selected_row -= visible_rows;
		if(lv->selected_row < 0) {
			lv->selected_row = 0;
		}
		lv->scroll_y = lv->selected_row * MKGUI_ROW_HEIGHT;
		if(lv->scroll_y > max_scroll) {
			lv->scroll_y = max_scroll;
		}
		dirty_all(ctx);
		ev->type = MKGUI_EVENT_LISTVIEW_SELECT;
		ev->id = ctx->focus_id;
		ev->value = lv->selected_row;
		return 1;

	} else if(ks == MKGUI_KEY_PAGE_DOWN) {
		lv->selected_row += visible_rows;
		if(lv->selected_row >= (int32_t)lv->row_count) {
			lv->selected_row = (int32_t)lv->row_count - 1;
		}
		int32_t row_bottom = (lv->selected_row + 1) * MKGUI_ROW_HEIGHT;
		if(row_bottom > lv->scroll_y + content_h) {
			lv->scroll_y = ((row_bottom - content_h + MKGUI_ROW_HEIGHT - 1) / MKGUI_ROW_HEIGHT) * MKGUI_ROW_HEIGHT;
		}
		if(lv->scroll_y > max_scroll) {
			lv->scroll_y = max_scroll;
		}
		dirty_all(ctx);
		ev->type = MKGUI_EVENT_LISTVIEW_SELECT;
		ev->id = ctx->focus_id;
		ev->value = lv->selected_row;
		return 1;

	} else if(ks == MKGUI_KEY_HOME) {
		lv->selected_row = 0;
		lv->scroll_y = 0;
		dirty_all(ctx);
		ev->type = MKGUI_EVENT_LISTVIEW_SELECT;
		ev->id = ctx->focus_id;
		ev->value = 0;
		return 1;

	} else if(ks == MKGUI_KEY_END) {
		lv->selected_row = (int32_t)lv->row_count - 1;
		lv->scroll_y = max_scroll;
		dirty_all(ctx);
		ev->type = MKGUI_EVENT_LISTVIEW_SELECT;
		ev->id = ctx->focus_id;
		ev->value = lv->selected_row;
		return 1;
	}

	return 0;
}

// [=]===^=[ mkgui_listview_setup ]==============================[=]
static void mkgui_listview_setup(struct mkgui_ctx *ctx, uint32_t id, uint32_t row_count, uint32_t col_count, struct mkgui_column *columns, mkgui_row_cb cb, void *userdata) {
	struct mkgui_listview_data *lv = find_listv_data(ctx, id);
	if(!lv && ctx->listv_count < 32) {
		lv = &ctx->listvs[ctx->listv_count++];
		memset(lv, 0, sizeof(*lv));
		lv->widget_id = id;
	}
	if(!lv) {
		return;
	}
	lv->row_count = row_count;
	lv->col_count = col_count > MKGUI_MAX_COLS ? MKGUI_MAX_COLS : col_count;
	memcpy(lv->columns, columns, lv->col_count * sizeof(struct mkgui_column));
	lv->row_cb = cb;
	lv->userdata = userdata;
	lv->selected_row = -1;
	lv->sort_col = -1;
	lv->sort_dir = 1;
	lv->header_height = MKGUI_ROW_HEIGHT;
	for(uint32_t i = 0; i < lv->col_count; ++i) {
		lv->col_order[i] = i;
	}
	dirty_all(ctx);
}

// [=]===^=[ mkgui_listview_set_rows ]===========================[=]
static void mkgui_listview_set_rows(struct mkgui_ctx *ctx, uint32_t id, uint32_t row_count) {
	struct mkgui_listview_data *lv = find_listv_data(ctx, id);
	if(lv) {
		lv->row_count = row_count;
		dirty_all(ctx);
	}
}

// [=]===^=[ mkgui_listview_get_selected ]=======================[=]
static int32_t mkgui_listview_get_selected(struct mkgui_ctx *ctx, uint32_t id) {
	struct mkgui_listview_data *lv = find_listv_data(ctx, id);
	return lv ? lv->selected_row : -1;
}

// [=]===^=[ mkgui_listview_get_col_order ]======================[=]
static const uint32_t *mkgui_listview_get_col_order(struct mkgui_ctx *ctx, uint32_t id) {
	struct mkgui_listview_data *lv = find_listv_data(ctx, id);
	return lv ? lv->col_order : NULL;
}

// [=]===^=[ mkgui_listview_set_col_order ]======================[=]
static void mkgui_listview_set_col_order(struct mkgui_ctx *ctx, uint32_t id, const uint32_t *order, uint32_t count) {
	struct mkgui_listview_data *lv = find_listv_data(ctx, id);
	if(!lv) {
		return;
	}
	if(count > lv->col_count) {
		count = lv->col_count;
	}
	memcpy(lv->col_order, order, count * sizeof(uint32_t));
	dirty_all(ctx);
}

// [=]===^=[ mkgui_listview_get_col_width ]======================[=]
static int32_t mkgui_listview_get_col_width(struct mkgui_ctx *ctx, uint32_t id, uint32_t col) {
	struct mkgui_listview_data *lv = find_listv_data(ctx, id);
	if(!lv || col >= lv->col_count) {
		return 0;
	}
	return lv->columns[col].width;
}

// [=]===^=[ mkgui_listview_set_col_width ]======================[=]
static void mkgui_listview_set_col_width(struct mkgui_ctx *ctx, uint32_t id, uint32_t col, int32_t width) {
	struct mkgui_listview_data *lv = find_listv_data(ctx, id);
	if(!lv || col >= lv->col_count) {
		return;
	}
	if(width < 20) {
		width = 20;
	}
	lv->columns[col].width = width;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_listview_set_cell_type ]======================[=]
static void mkgui_listview_set_cell_type(struct mkgui_ctx *ctx, uint32_t id, uint32_t col, uint32_t cell_type) {
	struct mkgui_listview_data *lv = find_listv_data(ctx, id);
	if(!lv || col >= lv->col_count) {
		return;
	}
	lv->columns[col].cell_type = cell_type;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_listview_visible_range ]======================[=]
static void mkgui_listview_visible_range(struct mkgui_ctx *ctx, uint32_t id, int32_t *first, int32_t *last) {
	struct mkgui_listview_data *lv = find_listv_data(ctx, id);
	if(!lv) {
		*first = -1;
		*last = -1;
		return;
	}
	int32_t idx = find_widget_idx(ctx, id);
	if(idx < 0) {
		*first = -1;
		*last = -1;
		return;
	}
	int32_t rh = ctx->rects[idx].h;
	int32_t hh = lv->header_height > 0 ? lv->header_height : MKGUI_ROW_HEIGHT;
	int32_t content_h = rh - hh - 2;
	*first = lv->scroll_y / MKGUI_ROW_HEIGHT;
	*last = *first + content_h / MKGUI_ROW_HEIGHT;
	if(*last >= (int32_t)lv->row_count) {
		*last = (int32_t)lv->row_count - 1;
	}
}
