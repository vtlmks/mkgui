// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ lv_multi_sel_find ]=================================[=]
static int32_t lv_multi_sel_find(struct mkgui_listview_data *lv, int32_t row) {
	uint32_t lo = 0, hi = lv->multi_sel_count;
	while(lo < hi) {
		uint32_t mid = (lo + hi) / 2;
		if(lv->multi_sel[mid] < row) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}
	return (lo < lv->multi_sel_count && lv->multi_sel[lo] == row) ? (int32_t)lo : -1;
}

// [=]===^=[ lv_multi_sel_toggle ]================================[=]
static void lv_multi_sel_toggle(struct mkgui_listview_data *lv, int32_t row) {
	int32_t idx = lv_multi_sel_find(lv, row);
	if(idx >= 0) {
		memmove(&lv->multi_sel[idx], &lv->multi_sel[idx + 1], (lv->multi_sel_count - (uint32_t)idx - 1) * sizeof(int32_t));
		--lv->multi_sel_count;
	} else {
		if(lv->multi_sel_count >= MKGUI_MAX_MULTI_SEL) {
			return;
		}
		uint32_t lo = 0, hi = lv->multi_sel_count;
		while(lo < hi) {
			uint32_t mid = (lo + hi) / 2;
			if(lv->multi_sel[mid] < row) {
				lo = mid + 1;
			} else {
				hi = mid;
			}
		}
		memmove(&lv->multi_sel[lo + 1], &lv->multi_sel[lo], (lv->multi_sel_count - lo) * sizeof(int32_t));
		lv->multi_sel[lo] = row;
		++lv->multi_sel_count;
	}
}

// [=]===^=[ listview_header_hit ]===============================[=]
static int32_t listview_header_hit(struct mkgui_ctx *ctx, uint32_t idx, int32_t mx, int32_t my) {
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;

	struct mkgui_listview_data *lv = find_listv_data(ctx, ctx->widgets[idx].id);
	if(!lv) {
		return -1;
	}

	int32_t hh = lv->header_height > 0 ? lv->header_height : ctx->row_height;
	if(my < ry || my >= ry + hh) {
		return -1;
	}

	int32_t cx = rx - lv->scroll_x;
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

	int32_t hh = lv->header_height > 0 ? lv->header_height : ctx->row_height;
	if(my < ry || my >= ry + hh) {
		return -1;
	}

	int32_t div_hit = sc(ctx, 3);
	int32_t cx = rx - lv->scroll_x;
	for(uint32_t d = 0; d < lv->col_count; ++d) {
		cx += lv->columns[lv->col_order[d]].width;
		if(mx >= cx - div_hit && mx <= cx + div_hit) {
			return (int32_t)d;
		}
	}
	return -1;
}

// [=]===^=[ listview_col_insert_pos ]============================[=]
static int32_t listview_col_insert_pos(struct mkgui_ctx *ctx, struct mkgui_listview_data *lv, uint32_t idx) {
	int32_t rx = ctx->rects[idx].x;
	int32_t mx = ctx->mouse_x;
	int32_t cx = rx + 1 - lv->scroll_x;
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
// Formats a unix timestamp string as "YYYY-MM-DD HH:MM". The caller provides
// a small fixed buffer (typically 32 bytes) that fits the output. Invalid or
// out-of-range timestamps render as "-" rather than copying the raw input
// back verbatim, which would silently truncate if the row callback returned
// a long string for a CELL_DATE column.
static void format_date(char *timestamp_str, char *buf, uint32_t buf_size) {
	time_t ts = (time_t)strtoll(timestamp_str, NULL, 10);
	if(ts <= 0) {
		snprintf(buf, buf_size, "-");
		return;
	}
	struct tm *tm = localtime(&ts);
	if(!tm) {
		snprintf(buf, buf_size, "-");
		return;
	}
	strftime(buf, buf_size, "%Y-%m-%d %H:%M", tm);
}

// [=]===^=[ render_cell ]========================================[=]
static void render_cell(struct mkgui_ctx *ctx, struct mkgui_listview_data *lv, uint32_t col, char *cell_buf, int32_t cx, int32_t ty, int32_t col_w, int32_t clip_left, int32_t clip_right, int32_t clip_top, int32_t clip_bottom, uint32_t tc, int32_t row_y) {
	int32_t cell_pad = sc(ctx, 4);
	switch(lv->columns[col].cell_type) {
		case MKGUI_CELL_PROGRESS: {
			int32_t val = 0;
			int32_t max = 100;
			char *slash = strchr(cell_buf, '/');
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
			int32_t bar_pad = sc(ctx, 3);
			int32_t bar_x = cx + cell_pad;
			int32_t bar_w = col_w - cell_pad * 2;
			int32_t bar_h = ctx->row_height - bar_pad * 2;
			int32_t bar_y = row_y + bar_pad;
			if(bar_w > 0 && bar_h > 0) {
				int32_t dx = bar_x < clip_left ? clip_left : bar_x;
				int32_t dr = bar_x + bar_w > clip_right ? clip_right : bar_x + bar_w;
				int32_t dt = bar_y < clip_top ? clip_top : bar_y;
				int32_t db = bar_y + bar_h > clip_bottom ? clip_bottom : bar_y + bar_h;
				if(dx < dr && dt < db) {
					draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, dx, dt, dr - dx, db - dt, ctx->theme.input_bg);
					int32_t fill_w = (int32_t)((int64_t)bar_w * val / max);
					if(fill_w > 0) {
						int32_t fr = bar_x + fill_w > clip_right ? clip_right : bar_x + fill_w;
						if(dx < fr) {
							draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, dx, dt, fr - dx, db - dt, ctx->theme.accent);
						}
					}
				}
				char pct[16];
				snprintf(pct, sizeof(pct), "%d%%", (int32_t)((int64_t)val * 100 / max));
				int32_t tw = text_width(ctx, pct);
				int32_t tx = bar_x + (bar_w - tw) / 2;
				push_text_clip(tx, ty, pct, tc, clip_left, clip_top, clip_right, clip_bottom);
			}
		} break;

		case MKGUI_CELL_ICON_TEXT: {
			char *tab = strchr(cell_buf, '\t');
			if(tab) {
				char icon_name[64];
				uint32_t len = (uint32_t)(tab - cell_buf);
				if(len >= sizeof(icon_name)) {
					len = sizeof(icon_name) - 1;
				}
				memcpy(icon_name, cell_buf, len);
				icon_name[len] = '\0';
				int32_t ii = icon_resolve(ctx, icon_name);
				if(ii >= 0) {
					int32_t icon_y = row_y + (ctx->row_height - icons[ii].h) / 2;
					draw_icon(ctx->pixels, ctx->win_w, ctx->win_h, &icons[ii], cx + cell_pad, icon_y, clip_left, clip_top, clip_right, clip_bottom);
					push_text_clip(cx + cell_pad + icons[ii].w + cell_pad, ty, tab + 1, tc, clip_left, clip_top, clip_right, clip_bottom);
				} else {
					push_text_clip(cx + cell_pad, ty, tab + 1, tc, clip_left, clip_top, clip_right, clip_bottom);
				}
			} else {
				push_text_clip(cx + cell_pad, ty, cell_buf, tc, clip_left, clip_top, clip_right, clip_bottom);
			}
		} break;

		case MKGUI_CELL_SIZE: {
			char fmt[32];
			int64_t bytes = strtoll(cell_buf, NULL, 10);
			format_size(bytes, fmt, sizeof(fmt));
			int32_t tw = text_width(ctx, fmt);
			int32_t tx = cx + col_w - tw - cell_pad;
			push_text_clip(tx, ty, fmt, tc, clip_left, clip_top, clip_right, clip_bottom);
		} break;

		case MKGUI_CELL_DATE: {
			char fmt[32];
			format_date(cell_buf, fmt, sizeof(fmt));
			push_text_clip(cx + cell_pad, ty, fmt, tc, clip_left, clip_top, clip_right, clip_bottom);
		} break;

		case MKGUI_CELL_CHECKBOX: {
			uint32_t checked = (cell_buf[0] == '1');
			int32_t box_size = sc(ctx, 14);
			int32_t bx = cx + (col_w - box_size) / 2;
			int32_t by = row_y + (ctx->row_height - box_size) / 2;
			if(bx + box_size > clip_left && bx < clip_right && by + box_size > clip_top && by < clip_bottom) {
				uint32_t bg = checked ? ctx->theme.highlight : ctx->theme.input_bg;
				draw_patch(ctx, MKGUI_STYLE_SUNKEN, bx, by, box_size, box_size, bg, ctx->theme.widget_border);
				if(checked) {
					int32_t ccx = bx + box_size / 2;
					int32_t ccy = by + box_size / 2;
					int32_t ck1 = sc(ctx, 3);
					int32_t ck2 = sc(ctx, 2);
					draw_aa_line(ctx->pixels, ctx->win_w, ctx->win_h, ccx - ck1, ccy - 1, ccx - 1, ccy + ck2, ctx->theme.sel_text, 2);
					draw_aa_line(ctx->pixels, ctx->win_w, ctx->win_h, ccx - 1, ccy + ck2, ccx + cell_pad, ccy - ck1, ctx->theme.sel_text, 2);
				}
			}
		} break;

		default: {
			push_text_clip(cx + cell_pad, ty, cell_buf, tc, clip_left, clip_top, clip_right, clip_bottom);
		} break;
	}
}

// [=]===^=[ listview_autosize_col ]===============================[=]
static void listview_autosize_col(struct mkgui_ctx *ctx, struct mkgui_listview_data *lv, uint32_t col) {
	if(col >= lv->col_count || !lv->row_cb) {
		return;
	}

	int32_t max_w = text_width(ctx, lv->columns[col].label);
	int32_t cell_pad = sc(ctx, 4);

	char buf[MKGUI_MAX_TEXT];
	for(uint32_t row = 0; row < lv->row_count; ++row) {
		buf[0] = '\0';
		lv->row_cb(row, col, buf, sizeof(buf), lv->userdata);

		int32_t cell_w = 0;
		switch(lv->columns[col].cell_type) {
			case MKGUI_CELL_ICON_TEXT: {
				char *tab = strchr(buf, '\t');
				cell_w = text_width(ctx, tab ? tab + 1 : buf) + ctx->icon_size + cell_pad;
			} break;

			case MKGUI_CELL_SIZE: {
				char fmt[32];
				format_size(strtoll(buf, NULL, 10), fmt, sizeof(fmt));
				cell_w = text_width(ctx, fmt);
			} break;

			case MKGUI_CELL_DATE: {
				char fmt[32];
				format_date(buf, fmt, sizeof(fmt));
				cell_w = text_width(ctx, fmt);
			} break;

			case MKGUI_CELL_CHECKBOX: {
				cell_w = sc(ctx, 14);
			} break;

			case MKGUI_CELL_PROGRESS: {
				cell_w = 0;
			} break;

			default: {
				cell_w = text_width(ctx, buf);
			} break;
		}

		if(cell_w > max_w) {
			max_w = cell_w;
		}
	}

	lv->columns[col].width = max_w + sc(ctx, 12);
	dirty_all(ctx);
}

// [=]===^=[ listview_total_col_w ]===============================[=]
static int32_t listview_total_col_w(struct mkgui_listview_data *lv) {
	int32_t total = 0;
	for(uint32_t d = 0; d < lv->col_count; ++d) {
		total += lv->columns[lv->col_order[d]].width;
	}
	return total;
}

// [=]===^=[ listview_clamp_scroll_y ]=============================[=]
static void listview_clamp_scroll_y(struct mkgui_ctx *ctx, struct mkgui_listview_data *lv, int32_t content_h) {
	int32_t total = (int32_t)lv->row_count * ctx->row_height;
	int32_t max_scroll = total - content_h;
	if(max_scroll < 0) {
		max_scroll = 0;
	}

	if(lv->scroll_y < 0) {
		lv->scroll_y = 0;
	}

	if(lv->scroll_y > max_scroll) {
		lv->scroll_y = max_scroll;
	}
}

// [=]===^=[ listview_clamp_scroll_x ]=============================[=]
static void listview_clamp_scroll_x(struct mkgui_listview_data *lv, int32_t content_w) {
	int32_t total = listview_total_col_w(lv);
	int32_t max_scroll = total - content_w;
	if(max_scroll < 0) {
		max_scroll = 0;
	}

	if(lv->scroll_x < 0) {
		lv->scroll_x = 0;
	}

	if(lv->scroll_x > max_scroll) {
		lv->scroll_x = max_scroll;
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
	draw_patch(ctx, MKGUI_STYLE_SUNKEN, rx, ry, rw, rh, ctx->theme.input_bg, focused ? ctx->theme.highlight : ctx->theme.widget_border);

	int32_t hh = lv->header_height > 0 ? lv->header_height : ctx->row_height;
	int32_t content_w = rw - 2 - ctx->scrollbar_w;
	int32_t clip_left = rx + 1;
	int32_t clip_right = rx + 1 + content_w;
	int32_t total_col_w = listview_total_col_w(lv);
	uint32_t needs_hscroll = (total_col_w > content_w);

	int32_t content_y = ry + hh + 1;
	int32_t content_h = rh - hh - 2;
	if(needs_hscroll) {
		content_h -= ctx->scrollbar_w;
	}

	listview_clamp_scroll_x(lv, content_w);
	listview_clamp_scroll_y(ctx, lv, content_h);
	int32_t sx = lv->scroll_x;

	draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx + 1, ry + 1, content_w, hh, ctx->theme.header_bg);

	int32_t hdr_pad = sc(ctx, 4);
	int32_t sort_icon_w = sc(ctx, 12);
	int32_t sort_arr_w = sc(ctx, 7);
	int32_t sort_arr_h = sc(ctx, 4);

	int32_t cx = rx + 1 - sx;
	for(uint32_t d = 0; d < lv->col_count; ++d) {
		uint32_t c = lv->col_order[d];
		int32_t cw = lv->columns[c].width;
		int32_t col_left = cx;
		int32_t col_right = cx + cw;
		if(col_left < clip_left) {
			col_left = clip_left;
		}

		if(col_right > clip_right) {
			col_right = clip_right;
		}

		if(col_left < col_right) {
			int32_t ty = ry + (hh - ctx->font_height) / 2;
			push_text_clip(cx + hdr_pad, ty, lv->columns[c].label, ctx->theme.text, col_left, ry, col_right, ry + hh);

			if(lv->sort_col == (int32_t)c) {
				int32_t sacx = cx + cw - sort_icon_w + sort_arr_w / 2;
				int32_t say = ry + hh / 2;
				if(sacx - sort_arr_w / 2 >= clip_left && sacx + sort_arr_w / 2 <= clip_right) {
					if(lv->sort_dir > 0) {
						draw_triangle_aa(ctx->pixels, ctx->win_w, ctx->win_h, sacx, say - sort_arr_h / 2, sacx - sort_arr_w / 2, say + sort_arr_h / 2, sacx + sort_arr_w / 2, say + sort_arr_h / 2, ctx->theme.text);
					} else {
						draw_triangle_aa(ctx->pixels, ctx->win_w, ctx->win_h, sacx - sort_arr_w / 2, say - sort_arr_h / 2, sacx + sort_arr_w / 2, say - sort_arr_h / 2, sacx, say + sort_arr_h / 2, ctx->theme.text);
					}
				}
			}
		}

		cx += cw;
		if(cx > clip_left && cx < clip_right) {
			draw_vline(ctx->pixels, ctx->win_w, ctx->win_h, cx, ry + 1, hh, ctx->theme.widget_border);
		}
	}
	draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, rx + 1, ry + hh, content_w, ctx->theme.widget_border);

	if(ctx->drag_col_id == w->id && ctx->drag_col_src >= 0) {
		int32_t insert_d = listview_col_insert_pos(ctx, lv, idx);
		if(insert_d >= 0) {
			int32_t ix = rx + 1 - sx;
			for(uint32_t d = 0; d < (uint32_t)insert_d; ++d) {
				ix += lv->columns[lv->col_order[d]].width;
			}

			if(ix >= clip_left && ix <= clip_right) {
				int32_t ind_w = sc(ctx, 3);
				draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ix - 1, ry + 1, ind_w, hh, ctx->theme.highlight);
			}
		}
	}

	int32_t visible_rows = content_h / ctx->row_height;
	int32_t first_row = lv->scroll_y / ctx->row_height;

	int32_t clip_top = content_y;
	int32_t clip_bottom = content_y + content_h;

	char cell_buf[MKGUI_MAX_TEXT];
	for(uint32_t r = 0; r <= (uint32_t)(visible_rows + 1) && (first_row + (int32_t)r) < (int32_t)lv->row_count; ++r) {
		int32_t row_idx = first_row + (int32_t)r;
		int32_t row_y = content_y + (int32_t)r * ctx->row_height - (lv->scroll_y % ctx->row_height);

		if(row_y >= clip_bottom || row_y + ctx->row_height <= clip_top) {
			continue;
		}

		int32_t draw_y = row_y < clip_top ? clip_top : row_y;
		int32_t draw_end = row_y + ctx->row_height;
		if(draw_end > clip_bottom) {
			draw_end = clip_bottom;
		}
		int32_t draw_h = draw_end - draw_y;

		uint32_t is_selected;
		if(w->style & MKGUI_LISTVIEW_MULTI_SELECT) {
			is_selected = (lv_multi_sel_find(lv, row_idx) >= 0);
		} else {
			is_selected = ((uint32_t)row_idx == (uint32_t)lv->selected_row);
		}
		uint32_t row_bg = (row_idx & 1) ? ctx->theme.listview_alt : ctx->theme.input_bg;
		if(is_selected) {
			row_bg = ctx->theme.selection;
		}
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx + 1, draw_y, content_w, draw_h, row_bg);

		int32_t ty = row_y + (ctx->row_height - ctx->font_height) / 2;
		cx = rx + 1 - sx;
		for(uint32_t d = 0; d < lv->col_count; ++d) {
			uint32_t c = lv->col_order[d];
			int32_t col_w = lv->columns[c].width;
			if(cx + col_w <= clip_left) {
				cx += col_w;
				continue;
			}

			if(cx >= clip_right) {
				break;
			}
			cell_buf[0] = '\0';
			if(lv->row_cb) {
				lv->row_cb((uint32_t)row_idx, c, cell_buf, sizeof(cell_buf), lv->userdata);
			}
			uint32_t tc = is_selected ? ctx->theme.sel_text : ((w->flags & MKGUI_DISABLED) ? ctx->theme.text_disabled : ctx->theme.text);
			int32_t cell_cl = cx;
			int32_t cell_cr = cx + col_w;
			if(cell_cl < clip_left) {
				cell_cl = clip_left;
			}

			if(cell_cr > clip_right) {
				cell_cr = clip_right;
			}

			if(!(ctx->cell_edit.active && ctx->cell_edit.widget_id == w->id && ctx->cell_edit.row == row_idx && ctx->cell_edit.col == (int32_t)c)) {
				render_cell(ctx, lv, c, cell_buf, cx, ty, col_w, cell_cl, cell_cr, clip_top, clip_bottom, tc, row_y);
			}
			cx += col_w;
		}
	}

	int32_t sb_inset = sc(ctx, 2);
	int32_t min_thumb = sc(ctx, 20);

	if(lv->row_count > 0) {
		int32_t sb_x = rx + rw - ctx->scrollbar_w - 1;
		int32_t sb_y = content_y;
		int32_t sb_h = content_h;
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, sb_x, sb_y, ctx->scrollbar_w, sb_h, ctx->theme.scrollbar_bg);

		int32_t total_h = (int32_t)lv->row_count * ctx->row_height;
		if(total_h > sb_h) {
			int32_t thumb_h = sb_h * sb_h / total_h;
			if(thumb_h < min_thumb) {
				thumb_h = min_thumb;
			}
			int32_t thumb_y = sb_y + (int32_t)((int64_t)lv->scroll_y * (sb_h - thumb_h) / (total_h - sb_h));
			uint32_t thumb_color = (ctx->drag_scrollbar_id == w->id && !ctx->drag_scrollbar_horiz) ? ctx->theme.widget_hover : ctx->theme.scrollbar_thumb;
			draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, sb_x + sb_inset, thumb_y, ctx->scrollbar_w - sb_inset * 2, thumb_h, thumb_color, ctx->theme.corner_radius);
		}
	}

	if(needs_hscroll) {
		int32_t hsb_x = rx + 1;
		int32_t hsb_y = ry + rh - ctx->scrollbar_w - 1;
		int32_t hsb_w = content_w;
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, hsb_x, hsb_y, hsb_w, ctx->scrollbar_w, ctx->theme.scrollbar_bg);
		int32_t thumb_w = (int32_t)((int64_t)hsb_w * content_w / total_col_w);
		if(thumb_w < min_thumb) {
			thumb_w = min_thumb;
		}
		int32_t max_sx = total_col_w - content_w;
		int32_t thumb_x = hsb_x;
		if(max_sx > 0) {
			thumb_x = hsb_x + (int32_t)((int64_t)sx * (hsb_w - thumb_w) / max_sx);
		}
		uint32_t hthumb_color = (ctx->drag_scrollbar_id == w->id && ctx->drag_scrollbar_horiz) ? ctx->theme.widget_hover : ctx->theme.scrollbar_thumb;
		draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, thumb_x, hsb_y + sb_inset, thumb_w, ctx->scrollbar_w - sb_inset * 2, hthumb_color, ctx->theme.corner_radius);
	}

	if(lv->drag_active && lv->drag_target >= 0) {
		int32_t dhh = lv->header_height > 0 ? lv->header_height : ctx->row_height;
		int32_t dcy = ry + dhh + 1;
		int32_t tgt_y;
		if(lv->drag_target > lv->drag_source) {
			tgt_y = dcy + (lv->drag_target + 1) * ctx->row_height - lv->scroll_y;
		} else {
			tgt_y = dcy + lv->drag_target * ctx->row_height - lv->scroll_y;
		}

		if(tgt_y >= dcy && tgt_y <= ry + rh) {
			draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, rx + 1, tgt_y - 1, content_w, ctx->theme.accent);
			draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, rx + 1, tgt_y, content_w, ctx->theme.accent);
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

	int32_t hh = lv->header_height > 0 ? lv->header_height : ctx->row_height;
	int32_t content_y = ry + hh + 1;
	int32_t sb_x = rx + rw - ctx->scrollbar_w - 1;

	if(mx >= sb_x) {
		return -1;
	}
	int32_t content_h_bottom = ry + rh;
	int32_t tcw = rw - 2 - ctx->scrollbar_w;
	if(listview_total_col_w(lv) > tcw) {
		content_h_bottom -= ctx->scrollbar_w;
	}

	if(my < content_y || my >= content_h_bottom) {
		return -1;
	}

	int32_t row = (my - content_y + lv->scroll_y) / ctx->row_height;
	if(row < 0 || row >= (int32_t)lv->row_count) {
		return -1;
	}
	return row;
}

// [=]===^=[ listview_col_hit ]=================================[=]
static int32_t listview_col_hit(struct mkgui_ctx *ctx, uint32_t idx, int32_t mx) {
	int32_t rx = ctx->rects[idx].x;
	struct mkgui_listview_data *lv = find_listv_data(ctx, ctx->widgets[idx].id);
	if(!lv) {
		return -1;
	}
	int32_t cx = rx - lv->scroll_x;
	for(uint32_t d = 0; d < lv->col_count; ++d) {
		uint32_t logical = lv->col_order[d];
		int32_t cw = lv->columns[logical].width;
		if(mx >= cx && mx < cx + cw) {
			return (int32_t)logical;
		}
		cx += cw;
	}
	return -1;
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

	int32_t hh = lv->header_height > 0 ? lv->header_height : ctx->row_height;
	int32_t sb_x = rx + rw - ctx->scrollbar_w - 1;
	int32_t sb_y = ry + hh + 1;
	int32_t sb_h = rh - hh - 2;
	int32_t content_w = rw - 2 - ctx->scrollbar_w;
	if(listview_total_col_w(lv) > content_w) {
		sb_h -= ctx->scrollbar_w;
	}

	if(mx < sb_x || mx >= sb_x + ctx->scrollbar_w || my < sb_y || my >= sb_y + sb_h) {
		return 0;
	}

	int32_t total_h = (int32_t)lv->row_count * ctx->row_height;
	if(total_h <= sb_h) {
		return 1;
	}

	int32_t min_thumb = sc(ctx, 20);
	int32_t thumb_h = sb_h * sb_h / total_h;
	if(thumb_h < min_thumb) {
		thumb_h = min_thumb;
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

	int32_t rw = ctx->rects[idx].w;
	int32_t ry = ctx->rects[idx].y;
	int32_t rh = ctx->rects[idx].h;
	int32_t hh = lv->header_height > 0 ? lv->header_height : ctx->row_height;
	int32_t sb_y = ry + hh + 1;
	int32_t sb_h = rh - hh - 2;
	int32_t cw = rw - 2 - ctx->scrollbar_w;
	if(listview_total_col_w(lv) > cw) {
		sb_h -= ctx->scrollbar_w;
	}

	int32_t total_h = (int32_t)lv->row_count * ctx->row_height;
	if(total_h <= sb_h) {
		return 0;
	}

	int32_t min_thumb = sc(ctx, 20);
	int32_t thumb_h = sb_h * sb_h / total_h;
	if(thumb_h < min_thumb) {
		thumb_h = min_thumb;
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

	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;
	int32_t hh = lv->header_height > 0 ? lv->header_height : ctx->row_height;
	int32_t content_h = rh - hh - 2;
	int32_t cw = rw - 2 - ctx->scrollbar_w;
	if(listview_total_col_w(lv) > cw) {
		content_h -= ctx->scrollbar_w;
	}

	int32_t total_h = (int32_t)lv->row_count * ctx->row_height;
	int32_t max_scroll = total_h - content_h;
	if(max_scroll < 0) {
		max_scroll = 0;
	}

	int32_t page = content_h;
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

	int32_t rw = ctx->rects[idx].w;
	int32_t ry = ctx->rects[idx].y;
	int32_t rh = ctx->rects[idx].h;
	int32_t hh = lv->header_height > 0 ? lv->header_height : ctx->row_height;
	int32_t content_y = ry + hh + 1;
	int32_t content_h = rh - hh - 2;
	int32_t cw = rw - 2 - ctx->scrollbar_w;
	if(listview_total_col_w(lv) > cw) {
		content_h -= ctx->scrollbar_w;
	}

	int32_t total_h = (int32_t)lv->row_count * ctx->row_height;
	int32_t max_scroll = total_h - content_h;
	if(max_scroll < 0) {
		max_scroll = 0;
	}

	if(content_h <= 0) {
		return;
	}

	int32_t min_thumb = sc(ctx, 20);
	int32_t thumb_h = content_h * content_h / total_h;
	if(thumb_h < min_thumb) {
		thumb_h = min_thumb;
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
	dirty_all(ctx);
}

// [=]===^=[ listview_hscrollbar_hit ]=============================[=]
static uint32_t listview_hscrollbar_hit(struct mkgui_ctx *ctx, uint32_t idx, int32_t mx, int32_t my) {
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	struct mkgui_listview_data *lv = find_listv_data(ctx, ctx->widgets[idx].id);
	if(!lv) {
		return 0;
	}

	int32_t content_w = rw - 2 - ctx->scrollbar_w;
	int32_t total_col_w = listview_total_col_w(lv);
	if(total_col_w <= content_w) {
		return 0;
	}

	int32_t hsb_x = rx + 1;
	int32_t hsb_y = ry + rh - ctx->scrollbar_w - 1;
	int32_t hsb_w = content_w;

	if(mx < hsb_x || mx >= hsb_x + hsb_w || my < hsb_y || my >= hsb_y + ctx->scrollbar_w) {
		return 0;
	}

	int32_t min_thumb = sc(ctx, 20);
	int32_t thumb_w = (int32_t)((int64_t)hsb_w * content_w / total_col_w);
	if(thumb_w < min_thumb) {
		thumb_w = min_thumb;
	}
	int32_t max_sx = total_col_w - content_w;
	int32_t thumb_x = hsb_x;
	if(max_sx > 0) {
		thumb_x = hsb_x + (int32_t)((int64_t)lv->scroll_x * (hsb_w - thumb_w) / max_sx);
	}

	if(mx < thumb_x) {
		return 2;
	}

	if(mx >= thumb_x + thumb_w) {
		return 3;
	}
	return 1;
}

// [=]===^=[ listview_hthumb_offset ]==============================[=]
static int32_t listview_hthumb_offset(struct mkgui_ctx *ctx, uint32_t idx, int32_t mouse_x) {
	struct mkgui_listview_data *lv = find_listv_data(ctx, ctx->widgets[idx].id);
	if(!lv) {
		return 0;
	}

	int32_t rx = ctx->rects[idx].x;
	int32_t rw = ctx->rects[idx].w;
	int32_t content_w = rw - 2 - ctx->scrollbar_w;
	int32_t total_col_w = listview_total_col_w(lv);
	if(total_col_w <= content_w) {
		return 0;
	}

	int32_t min_thumb = sc(ctx, 20);
	int32_t hsb_x = rx + 1;
	int32_t hsb_w = content_w;
	int32_t thumb_w = (int32_t)((int64_t)hsb_w * content_w / total_col_w);
	if(thumb_w < min_thumb) {
		thumb_w = min_thumb;
	}
	int32_t max_sx = total_col_w - content_w;
	int32_t thumb_x = hsb_x + (int32_t)((int64_t)lv->scroll_x * (hsb_w - thumb_w) / max_sx);
	return mouse_x - thumb_x;
}

// [=]===^=[ listview_scroll_to_x ]==============================[=]
static void listview_scroll_to_x(struct mkgui_ctx *ctx, uint32_t widget_id, int32_t mouse_x) {
	struct mkgui_listview_data *lv = find_listv_data(ctx, widget_id);
	if(!lv) {
		return;
	}

	int32_t idx = find_widget_idx(ctx, widget_id);
	if(idx < 0) {
		return;
	}

	int32_t rx = ctx->rects[idx].x;
	int32_t rw = ctx->rects[idx].w;
	int32_t content_w = rw - 2 - ctx->scrollbar_w;
	int32_t total_col_w = listview_total_col_w(lv);
	int32_t max_sx = total_col_w - content_w;
	if(max_sx <= 0) {
		return;
	}

	int32_t min_thumb = sc(ctx, 20);
	int32_t hsb_x = rx + 1;
	int32_t hsb_w = content_w;
	int32_t thumb_w = (int32_t)((int64_t)hsb_w * content_w / total_col_w);
	if(thumb_w < min_thumb) {
		thumb_w = min_thumb;
	}
	int32_t track = hsb_w - thumb_w;
	if(track <= 0) {
		return;
	}
	float frac = (float)(mouse_x - hsb_x - ctx->drag_scrollbar_offset) / (float)track;
	if(frac < 0.0f) {
		frac = 0.0f;
	}

	if(frac > 1.0f) {
		frac = 1.0f;
	}
	lv->scroll_x = (int32_t)(frac * (float)max_sx);
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
	int32_t hh = lv->header_height > 0 ? lv->header_height : ctx->row_height;
	int32_t content_h = rh - hh - 2;
	int32_t visible_rows = content_h / ctx->row_height;
	int32_t total_h = (int32_t)lv->row_count * ctx->row_height;
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

	switch(ks) {
	case MKGUI_KEY_UP: {
		if(lv->selected_row > 0) {
			--lv->selected_row;
			int32_t row_y = lv->selected_row * ctx->row_height;
			if(row_y < lv->scroll_y) {
				lv->scroll_y = row_y;
			}
			dirty_all(ctx);
			ev->type = MKGUI_EVENT_LISTVIEW_SELECT;
			ev->id = ctx->focus_id;
			ev->value = lv->selected_row;
			return 1;
		}
	} break;

	case MKGUI_KEY_DOWN: {
		if(lv->selected_row < (int32_t)lv->row_count - 1) {
			++lv->selected_row;
			int32_t row_bottom = (lv->selected_row + 1) * ctx->row_height;
			if(row_bottom > lv->scroll_y + content_h) {
				lv->scroll_y = row_bottom - content_h;
			}
			dirty_all(ctx);
			ev->type = MKGUI_EVENT_LISTVIEW_SELECT;
			ev->id = ctx->focus_id;
			ev->value = lv->selected_row;
			return 1;
		}
	} break;

	case MKGUI_KEY_PAGE_UP: {
		lv->selected_row -= visible_rows;
		if(lv->selected_row < 0) {
			lv->selected_row = 0;
		}
		lv->scroll_y = lv->selected_row * ctx->row_height;
		if(lv->scroll_y > max_scroll) {
			lv->scroll_y = max_scroll;
		}
		dirty_all(ctx);
		ev->type = MKGUI_EVENT_LISTVIEW_SELECT;
		ev->id = ctx->focus_id;
		ev->value = lv->selected_row;
		return 1;
	} break;

	case MKGUI_KEY_PAGE_DOWN: {
		lv->selected_row += visible_rows;
		if(lv->selected_row >= (int32_t)lv->row_count) {
			lv->selected_row = (int32_t)lv->row_count - 1;
		}
		int32_t row_bottom = (lv->selected_row + 1) * ctx->row_height;
		if(row_bottom > lv->scroll_y + content_h) {
			lv->scroll_y = row_bottom - content_h;
		}

		if(lv->scroll_y > max_scroll) {
			lv->scroll_y = max_scroll;
		}
		dirty_all(ctx);
		ev->type = MKGUI_EVENT_LISTVIEW_SELECT;
		ev->id = ctx->focus_id;
		ev->value = lv->selected_row;
		return 1;
	} break;

	case MKGUI_KEY_HOME: {
		lv->selected_row = 0;
		lv->scroll_y = 0;
		dirty_all(ctx);
		ev->type = MKGUI_EVENT_LISTVIEW_SELECT;
		ev->id = ctx->focus_id;
		ev->value = 0;
		return 1;
	} break;

	case MKGUI_KEY_END: {
		lv->selected_row = (int32_t)lv->row_count - 1;
		lv->scroll_y = max_scroll;
		dirty_all(ctx);
		ev->type = MKGUI_EVENT_LISTVIEW_SELECT;
		ev->id = ctx->focus_id;
		ev->value = lv->selected_row;
		return 1;
	} break;

	default: {
	} break;
	}

	return 0;
}

// [=]===^=[ mkgui_listview_setup ]==============================[=]
MKGUI_API void mkgui_listview_setup(struct mkgui_ctx *ctx, uint32_t id, uint32_t row_count, uint32_t col_count, const struct mkgui_column *columns, mkgui_row_cb cb, void *userdata) {
	MKGUI_CHECK(ctx);
	if(!columns) {
		return;
	}

	if(!cb) {
		return;
	}
	struct mkgui_listview_data *lv = find_listv_data(ctx, id);
	if(!lv) {
		MKGUI_AUX_GROW(ctx->listvs, ctx->listv_count, ctx->listv_cap, struct mkgui_listview_data);
		if(ctx->listv_count >= ctx->listv_cap) {
			return;
		}
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
	for(uint32_t ci = 0; ci < lv->col_count; ++ci) {
		lv->columns[ci].width = sc(ctx, lv->columns[ci].width);
	}
	lv->row_cb = cb;
	lv->userdata = userdata;
	lv->selected_row = -1;
	lv->sort_col = -1;
	lv->sort_dir = 1;
	lv->drag_source = -1;
	lv->drag_target = -1;
	lv->header_height = ctx->row_height;
	for(uint32_t i = 0; i < lv->col_count; ++i) {
		lv->col_order[i] = i;
	}
	dirty_all(ctx);
}

// [=]===^=[ mkgui_listview_set_rows ]===========================[=]
MKGUI_API void mkgui_listview_set_rows(struct mkgui_ctx *ctx, uint32_t id, uint32_t row_count) {
	MKGUI_CHECK(ctx);
	struct mkgui_listview_data *lv = find_listv_data(ctx, id);
	if(lv) {
		lv->row_count = row_count;
		dirty_all(ctx);
	}
}

// [=]===^=[ mkgui_listview_get_selected ]=======================[=]
MKGUI_API int32_t mkgui_listview_get_selected(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, -1);
	struct mkgui_listview_data *lv = find_listv_data(ctx, id);
	return lv ? lv->selected_row : -1;
}

// [=]===^=[ mkgui_listview_get_multi_sel ]======================[=]
MKGUI_API uint32_t mkgui_listview_get_multi_sel(struct mkgui_ctx *ctx, uint32_t id, int32_t **out) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_listview_data *lv = find_listv_data(ctx, id);
	if(!lv) {
		if(out) {
			*out = NULL;
		}
		return 0;
	}

	if(out) {
		*out = lv->multi_sel;
	}
	return lv->multi_sel_count;
}

// [=]===^=[ mkgui_listview_is_selected ]========================[=]
MKGUI_API uint32_t mkgui_listview_is_selected(struct mkgui_ctx *ctx, uint32_t id, int32_t row) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_listview_data *lv = find_listv_data(ctx, id);
	if(!lv) {
		return 0;
	}
	return (lv_multi_sel_find(lv, row) >= 0) ? 1 : 0;
}

// [=]===^=[ mkgui_listview_clear_selection ]=====================[=]
MKGUI_API void mkgui_listview_clear_selection(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK(ctx);
	struct mkgui_listview_data *lv = find_listv_data(ctx, id);
	if(lv) {
		lv->multi_sel_count = 0;
		lv->selected_row = -1;
	}
}

// [=]===^=[ mkgui_listview_get_col_order ]======================[=]
MKGUI_API const uint32_t *mkgui_listview_get_col_order(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, NULL);
	struct mkgui_listview_data *lv = find_listv_data(ctx, id);
	return lv ? lv->col_order : NULL;
}

// [=]===^=[ mkgui_listview_set_col_order ]======================[=]
MKGUI_API void mkgui_listview_set_col_order(struct mkgui_ctx *ctx, uint32_t id, const uint32_t *order, uint32_t count) {
	MKGUI_CHECK(ctx);
	MKGUI_CHECK(order);
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
MKGUI_API int32_t mkgui_listview_get_col_width(struct mkgui_ctx *ctx, uint32_t id, uint32_t col) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_listview_data *lv = find_listv_data(ctx, id);
	if(!lv || col >= lv->col_count) {
		return 0;
	}
	return lv->columns[col].width;
}

// [=]===^=[ mkgui_listview_get_col_count ]======================[=]
MKGUI_API uint32_t mkgui_listview_get_col_count(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_listview_data *lv = find_listv_data(ctx, id);
	return lv ? lv->col_count : 0;
}

// [=]===^=[ mkgui_listview_get_col_label ]======================[=]
MKGUI_API const char *mkgui_listview_get_col_label(struct mkgui_ctx *ctx, uint32_t id, uint32_t col) {
	MKGUI_CHECK_VAL(ctx, "");
	struct mkgui_listview_data *lv = find_listv_data(ctx, id);
	if(!lv || col >= lv->col_count) {
		return "";
	}
	return lv->columns[col].label;
}

// [=]===^=[ mkgui_listview_set_col_width ]======================[=]
MKGUI_API void mkgui_listview_set_col_width(struct mkgui_ctx *ctx, uint32_t id, uint32_t col, int32_t width) {
	MKGUI_CHECK(ctx);
	struct mkgui_listview_data *lv = find_listv_data(ctx, id);
	if(!lv || col >= lv->col_count) {
		return;
	}
	int32_t min_col_w = sc(ctx, 20);
	if(width < min_col_w) {
		width = min_col_w;
	}
	lv->columns[col].width = sc(ctx, width);
	dirty_all(ctx);
}

// [=]===^=[ mkgui_listview_set_cell_type ]======================[=]
MKGUI_API void mkgui_listview_set_cell_type(struct mkgui_ctx *ctx, uint32_t id, uint32_t col, uint32_t cell_type) {
	MKGUI_CHECK(ctx);
	struct mkgui_listview_data *lv = find_listv_data(ctx, id);
	if(!lv || col >= lv->col_count) {
		return;
	}
	lv->columns[col].cell_type = cell_type;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_listview_visible_range ]======================[=]
MKGUI_API void mkgui_listview_visible_range(struct mkgui_ctx *ctx, uint32_t id, int32_t *first, int32_t *last) {
	MKGUI_CHECK(ctx);
	MKGUI_CHECK(first);
	MKGUI_CHECK(last);
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
	int32_t hh = lv->header_height > 0 ? lv->header_height : ctx->row_height;
	int32_t content_h = rh - hh - 2;
	*first = lv->scroll_y / ctx->row_height;
	*last = (lv->scroll_y + content_h - 1) / ctx->row_height;
	if(*last >= (int32_t)lv->row_count) {
		*last = (int32_t)lv->row_count - 1;
	}
}

// [=]===^=[ mkgui_listview_set_selected ]============================[=]
MKGUI_API void mkgui_listview_set_selected(struct mkgui_ctx *ctx, uint32_t id, int32_t row) {
	MKGUI_CHECK(ctx);
	struct mkgui_listview_data *lv = find_listv_data(ctx, id);
	if(!lv) {
		return;
	}
	lv->selected_row = row;
	lv->multi_sel_count = 0;
	if(row >= 0 && (uint32_t)row < lv->row_count) {
		lv->multi_sel[0] = row;
		lv->multi_sel_count = 1;
	}
	dirty_all(ctx);
}

// [=]===^=[ mkgui_listview_scroll_to ]===============================[=]
MKGUI_API void mkgui_listview_scroll_to(struct mkgui_ctx *ctx, uint32_t id, int32_t row) {
	MKGUI_CHECK(ctx);
	struct mkgui_listview_data *lv = find_listv_data(ctx, id);
	if(!lv) {
		return;
	}
	int32_t idx = find_widget_idx(ctx, id);
	if(idx < 0) {
		return;
	}
	int32_t rh = ctx->rects[idx].h;
	int32_t hh = lv->header_height > 0 ? lv->header_height : ctx->row_height;
	int32_t content_h = rh - hh - 2;
	int32_t row_y = row * ctx->row_height;
	if(row_y < lv->scroll_y) {
		lv->scroll_y = row_y;
	}

	if(row_y + ctx->row_height > lv->scroll_y + content_h) {
		lv->scroll_y = row_y + ctx->row_height - content_h;
	}

	if(lv->scroll_y < 0) {
		lv->scroll_y = 0;
	}
	dirty_all(ctx);
}

// [=]===^=[ mkgui_listview_get_sort ]================================[=]
MKGUI_API void mkgui_listview_get_sort(struct mkgui_ctx *ctx, uint32_t id, int32_t *col, int32_t *dir) {
	MKGUI_CHECK(ctx);
	struct mkgui_listview_data *lv = find_listv_data(ctx, id);
	if(!lv) {
		if(col) { *col = -1; }
		if(dir) { *dir = 0; }
		return;
	}

	if(col) { *col = lv->sort_col; }
	if(dir) { *dir = lv->sort_dir; }
}

// [=]===^=[ mkgui_listview_set_sort ]================================[=]
MKGUI_API void mkgui_listview_set_sort(struct mkgui_ctx *ctx, uint32_t id, int32_t col, int32_t dir) {
	MKGUI_CHECK(ctx);
	struct mkgui_listview_data *lv = find_listv_data(ctx, id);
	if(!lv) {
		return;
	}
	lv->sort_col = col;
	lv->sort_dir = dir;
	dirty_all(ctx);
}
