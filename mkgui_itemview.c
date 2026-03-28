// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define MKGUI_ITEMVIEW_ICON_LINES    2
#define MKGUI_ITEMVIEW_ICON_CELL_W_PX   88
#define MKGUI_ITEMVIEW_ICON_CELL_H_PX   80
#define MKGUI_ITEMVIEW_THUMB_CELL_W_PX  120
#define MKGUI_ITEMVIEW_THUMB_CELL_H_PX  110
#define MKGUI_ITEMVIEW_COMPACT_ROW_H_PX 20
#define MKGUI_ITEMVIEW_DETAIL_ROW_H_PX  20
#define MKGUI_ITEMVIEW_THUMB_SIZE_PX    64
#define MKGUI_ITEMVIEW_HSCROLL_H_PX     14

// [=]===^=[ itemview_cell_size ]=================================[=]
static void itemview_cell_size(struct mkgui_ctx *ctx, struct mkgui_itemview_data *iv, int32_t *cw, int32_t *ch) {
	if(iv->cell_w > 0 && iv->cell_h > 0) {
		*cw = iv->cell_w;
		*ch = iv->cell_h;
		return;
	}
	switch(iv->view_mode) {
		case MKGUI_VIEW_ICON: {
			*cw = sc(ctx, MKGUI_ITEMVIEW_ICON_CELL_W_PX);
			*ch = sc(ctx, MKGUI_ITEMVIEW_ICON_CELL_H_PX);
		} break;

		case MKGUI_VIEW_THUMBNAIL: {
			*cw = sc(ctx, MKGUI_ITEMVIEW_THUMB_CELL_W_PX);
			*ch = sc(ctx, MKGUI_ITEMVIEW_THUMB_CELL_H_PX);
		} break;

		case MKGUI_VIEW_COMPACT: {
			*cw = 0;
			*ch = sc(ctx, MKGUI_ITEMVIEW_COMPACT_ROW_H_PX);
		} break;

		case MKGUI_VIEW_DETAIL: {
			*cw = 0;
			*ch = sc(ctx, MKGUI_ITEMVIEW_DETAIL_ROW_H_PX);
		} break;

		default: {
			*cw = sc(ctx, MKGUI_ITEMVIEW_ICON_CELL_W_PX);
			*ch = sc(ctx, MKGUI_ITEMVIEW_ICON_CELL_H_PX);
		} break;
	}
}

// [=]===^=[ itemview_content_area ]===============================[=]
static void itemview_content_area(struct mkgui_ctx *ctx, struct mkgui_itemview_data *iv, int32_t rx, int32_t ry, int32_t rw, int32_t rh, int32_t *cx, int32_t *cy, int32_t *cw, int32_t *ch) {
	*cx = rx + 1;
	*cy = ry + 1;
	if(iv->view_mode == MKGUI_VIEW_COMPACT) {
		*cw = rw - 2;
		*ch = rh - 2 - sc(ctx, MKGUI_ITEMVIEW_HSCROLL_H_PX);
	} else {
		*cw = rw - 2 - ctx->scrollbar_w;
		*ch = rh - 2;
	}
}

// [=]===^=[ itemview_grid_cols ]==================================[=]
static int32_t itemview_grid_cols(int32_t content_w, int32_t cell_w) {
	if(cell_w <= 0) {
		return 1;
	}
	int32_t cols = content_w / cell_w;
	return cols > 0 ? cols : 1;
}

// [=]===^=[ itemview_total_height ]===============================[=]
static int32_t itemview_total_height(struct mkgui_ctx *ctx, struct mkgui_itemview_data *iv, int32_t content_w) {
	int32_t cw, ch;
	itemview_cell_size(ctx, iv, &cw, &ch);

	if(iv->view_mode == MKGUI_VIEW_DETAIL) {
		return (int32_t)iv->item_count * ch;
	}

	if(iv->view_mode == MKGUI_VIEW_COMPACT) {
		return (int32_t)iv->item_count * ch;
	}

	int32_t cols = itemview_grid_cols(content_w, cw);
	int32_t rows = ((int32_t)iv->item_count + cols - 1) / cols;
	return rows * ch;
}

// [=]===^=[ itemview_compact_total_w ]============================[=]
static int32_t itemview_compact_total_w(struct mkgui_ctx *ctx, struct mkgui_itemview_data *iv, int32_t content_h) {
	int32_t row_h = sc(ctx, MKGUI_ITEMVIEW_COMPACT_ROW_H_PX);
	int32_t rows_per_col = content_h / row_h;
	if(rows_per_col < 1) {
		rows_per_col = 1;
	}
	int32_t col_w = iv->cell_w > 0 ? iv->cell_w : sc(ctx, 180);
	int32_t total_cols = ((int32_t)iv->item_count + rows_per_col - 1) / rows_per_col;
	return total_cols * col_w;
}

// [=]===^=[ itemview_clamp_scroll ]===============================[=]
static void itemview_clamp_scroll(struct mkgui_ctx *ctx, struct mkgui_itemview_data *iv, int32_t content_w, int32_t content_h) {
	int32_t max_scroll;
	if(iv->view_mode == MKGUI_VIEW_COMPACT) {
		max_scroll = itemview_compact_total_w(ctx, iv, content_h) - content_w;
	} else {
		max_scroll = itemview_total_height(ctx, iv, content_w) - content_h;
	}
	if(max_scroll < 0) {
		max_scroll = 0;
	}
	if(iv->scroll_y < 0) {
		iv->scroll_y = 0;
	}
	if(iv->scroll_y > max_scroll) {
		iv->scroll_y = max_scroll;
	}
}

// [=]===^=[ itemview_get_label ]==================================[=]
static void itemview_get_label(struct mkgui_itemview_data *iv, uint32_t item, char *buf, uint32_t buf_size) {
	buf[0] = '\0';
	if(iv->label_cb) {
		iv->label_cb(item, buf, buf_size, iv->userdata);
	}
}

// [=]===^=[ itemview_get_icon_idx ]===============================[=]
static int32_t itemview_get_icon_idx(struct mkgui_itemview_data *iv, uint32_t item) {
	if(!iv->icon_cb) {
		return -1;
	}
	char name[MKGUI_ICON_NAME_LEN];
	name[0] = '\0';
	iv->icon_cb(item, name, sizeof(name), iv->userdata);
	if(!name[0]) {
		return -1;
	}
	return icon_resolve(name);
}

// [=]===^=[ render_itemview_icon ]================================[=]
static void render_itemview_icon(struct mkgui_ctx *ctx, uint32_t idx, struct mkgui_itemview_data *iv) {
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	int32_t cw, ch;
	itemview_cell_size(ctx, iv, &cw, &ch);
	int32_t ca_x, ca_y, ca_w, ca_h;
	itemview_content_area(ctx, iv, rx, ry, rw, rh, &ca_x, &ca_y, &ca_w, &ca_h);
	int32_t cols = itemview_grid_cols(ca_w, cw);
	int32_t pad_x = (ca_w - cols * cw) / 2;

	int32_t clip_x2 = ca_x + ca_w;
	int32_t clip_y2 = ca_y + ca_h;

	int32_t first_row = iv->scroll_y / ch;
	int32_t last_row = (iv->scroll_y + ca_h + ch - 1) / ch;

	char label[MKGUI_MAX_TEXT];

	for(int32_t row = first_row; row <= last_row; ++row) {
		for(uint32_t col = 0; col < (uint32_t)cols; ++col) {
			int32_t item = row * cols + (int32_t)col;
			if(item < 0 || item >= (int32_t)iv->item_count) {
				continue;
			}

			int32_t cx = ca_x + pad_x + (int32_t)col * cw;
			int32_t cy = ca_y + row * ch - iv->scroll_y;

			if(cy + ch <= ca_y || cy >= clip_y2) {
				continue;
			}

			int32_t sel_inset = sc(ctx, 2);
			int32_t icon_top_pad = sc(ctx, 8);
			int32_t cell_pad = sc(ctx, 3);
			int32_t label_gap = sc(ctx, 12);

			if(item == iv->selected) {
				draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, cx + sel_inset, cy + sel_inset, cw - sel_inset * 2, ch - sel_inset * 2, ctx->theme.selection, ctx->theme.corner_radius);
			}

			int32_t icon_idx = itemview_get_icon_idx(iv, (uint32_t)item);
			int32_t ic_h = (icon_idx >= 0) ? icons[icon_idx].h : 0;
			if(icon_idx >= 0) {
				int32_t ix = cx + (cw - icons[icon_idx].w) / 2;
				int32_t iy = cy + icon_top_pad;
				draw_icon(ctx->pixels, ctx->win_w, ctx->win_h, &icons[icon_idx], ix, iy, ca_x, ca_y, clip_x2, clip_y2);
			}

			itemview_get_label(iv, (uint32_t)item, label, sizeof(label));
			if(label[0]) {
				int32_t max_tw = cw - cell_pad * 2;
				int32_t cell_cx1 = cx + cell_pad > ca_x ? cx + cell_pad : ca_x;
				int32_t cell_cx2 = cx + cw - cell_pad < clip_x2 ? cx + cw - cell_pad : clip_x2;
				uint32_t tc = (item == iv->selected) ? ctx->theme.sel_text : ((ctx->widgets[idx].flags & MKGUI_DISABLED) ? ctx->theme.text_disabled : ctx->theme.text);
				int32_t ty = cy + ic_h + label_gap;
				char *p = label;
				for(uint32_t line = 0; line < MKGUI_ITEMVIEW_ICON_LINES && *p; ++line) {
					int32_t w = 0;
					int32_t brk = 0;
					int32_t last_sep = -1;
					for(uint32_t k = 0; p[k]; ++k) {
						uint32_t ch = (uint8_t)p[k];
						int32_t gw = (ch >= MKGUI_GLYPH_FIRST && ch <= MKGUI_GLYPH_LAST) ? ctx->glyphs[ch - MKGUI_GLYPH_FIRST].advance : ctx->char_width;
						if(w + gw > max_tw && k > 0) {
							brk = (last_sep >= 0) ? last_sep + 1 : (int32_t)k;
							break;
						}
						w += gw;
						if(p[k] == '-' || p[k] == '_' || p[k] == ' ') {
							last_sep = (int32_t)k;
						}
					}
					char tmp[MKGUI_MAX_TEXT];
					if(brk > 0) {
						if(brk < (int32_t)(sizeof(tmp) - 1)) {
							memcpy(tmp, p, (uint32_t)brk);
							tmp[brk] = '\0';
						} else {
							strncpy(tmp, p, sizeof(tmp) - 1);
							tmp[sizeof(tmp) - 1] = '\0';
						}
					} else {
						strncpy(tmp, p, sizeof(tmp) - 1);
						tmp[sizeof(tmp) - 1] = '\0';
					}
					int32_t tw = text_width(ctx, tmp);
					int32_t tx = cx + (cw - tw) / 2;
					push_text_clip(tx, ty, tmp, tc, cell_cx1, ca_y, cell_cx2, clip_y2);
					ty += ctx->font_height + 1;
					if(brk > 0) {
						p += brk;
					} else {
						break;
					}
				}
			}
		}
	}
}

// [=]===^=[ render_itemview_thumbnail ]===========================[=]
static void render_itemview_thumbnail(struct mkgui_ctx *ctx, uint32_t idx, struct mkgui_itemview_data *iv) {
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	int32_t cw, ch;
	itemview_cell_size(ctx, iv, &cw, &ch);
	int32_t ca_x, ca_y, ca_w, ca_h;
	itemview_content_area(ctx, iv, rx, ry, rw, rh, &ca_x, &ca_y, &ca_w, &ca_h);
	int32_t cols = itemview_grid_cols(ca_w, cw);
	int32_t pad_x = (ca_w - cols * cw) / 2;

	int32_t clip_x2 = ca_x + ca_w;
	int32_t clip_y2 = ca_y + ca_h;

	int32_t first_row = iv->scroll_y / ch;
	int32_t last_row = (iv->scroll_y + ca_h + ch - 1) / ch;

	int32_t ts = iv->thumb_size > 0 ? iv->thumb_size : sc(ctx, MKGUI_ITEMVIEW_THUMB_SIZE_PX);

	char label[MKGUI_MAX_TEXT];

	for(int32_t row = first_row; row <= last_row; ++row) {
		for(uint32_t col = 0; col < (uint32_t)cols; ++col) {
			int32_t item = row * cols + (int32_t)col;
			if(item < 0 || item >= (int32_t)iv->item_count) {
				continue;
			}

			int32_t cx = ca_x + pad_x + (int32_t)col * cw;
			int32_t cy = ca_y + row * ch - iv->scroll_y;

			if(cy + ch <= ca_y || cy >= clip_y2) {
				continue;
			}

			int32_t sel_inset = sc(ctx, 2);
			int32_t thumb_pad = sc(ctx, 4);

			if(item == iv->selected) {
				draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, cx + sel_inset, cy + sel_inset, cw - sel_inset * 2, ch - sel_inset * 2, ctx->theme.selection, ctx->theme.corner_radius);
			}

			int32_t tx_off = (cw - ts) / 2;
			int32_t ty_off = thumb_pad;
			int32_t thumb_x = cx + tx_off;
			int32_t thumb_y = cy + ty_off;

			draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, thumb_x, thumb_y, ts, ts, ctx->theme.input_bg);

			if(iv->thumbnail_cb && iv->thumb_buf) {
				uint32_t uts = (uint32_t)ts;
				iv->thumbnail_cb((uint32_t)item, iv->thumb_buf, ts, ts, iv->userdata);
				for(uint32_t py = 0; py < uts; ++py) {
					int32_t dy = thumb_y + (int32_t)py;
					if(dy < ca_y || dy >= clip_y2 || dy < 0 || dy >= ctx->win_h) {
						continue;
					}
					for(uint32_t px = 0; px < uts; ++px) {
						int32_t dx = thumb_x + (int32_t)px;
						if(dx < ca_x || dx >= clip_x2 || dx < 0 || dx >= ctx->win_w) {
							continue;
						}
						uint32_t spx = iv->thumb_buf[py * uts + px];
						uint32_t alpha = (spx >> 24) & 0xff;
						if(alpha == 255) {
							ctx->pixels[(uint32_t)dy * (uint32_t)ctx->win_w + (uint32_t)dx] = spx;
						} else if(alpha > 0) {
							ctx->pixels[(uint32_t)dy * (uint32_t)ctx->win_w + (uint32_t)dx] = blend_pixel(ctx->pixels[(uint32_t)dy * (uint32_t)ctx->win_w + (uint32_t)dx], spx, (uint8_t)alpha);
						}
					}
				}
			} else {
				int32_t icon_idx = itemview_get_icon_idx(iv, (uint32_t)item);
				if(icon_idx >= 0) {
					int32_t ix = cx + (cw - icons[icon_idx].w) / 2;
					int32_t iy = cy + (ts - icons[icon_idx].h) / 2 + ty_off;
					draw_icon(ctx->pixels, ctx->win_w, ctx->win_h, &icons[icon_idx], ix, iy, ca_x, ca_y, clip_x2, clip_y2);
				}
			}

			draw_rect_border(ctx->pixels, ctx->win_w, ctx->win_h, thumb_x, thumb_y, ts, ts, ctx->theme.widget_border);

			itemview_get_label(iv, (uint32_t)item, label, sizeof(label));
			if(label[0]) {
				int32_t tw = text_width(ctx, label);
				int32_t max_tw = cw - thumb_pad;
				int32_t label_x = cx + (cw - (tw < max_tw ? tw : max_tw)) / 2;
				int32_t label_y = cy + ts + ty_off + thumb_pad;
				int32_t cell_clip_r = cx + cw < clip_x2 ? cx + cw : clip_x2;
				int32_t cell_clip_l = cx > ca_x ? cx : ca_x;
				uint32_t tc = (item == iv->selected) ? ctx->theme.sel_text : ((ctx->widgets[idx].flags & MKGUI_DISABLED) ? ctx->theme.text_disabled : ctx->theme.text);
				push_text_clip(label_x, label_y, label, tc, cell_clip_l, ca_y, cell_clip_r, clip_y2);
			}
		}
	}
}

// [=]===^=[ render_itemview_compact ]=============================[=]
static void render_itemview_compact(struct mkgui_ctx *ctx, uint32_t idx, struct mkgui_itemview_data *iv) {
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	int32_t ca_x, ca_y, ca_w, ca_h;
	itemview_content_area(ctx, iv, rx, ry, rw, rh, &ca_x, &ca_y, &ca_w, &ca_h);
	int32_t clip_x2 = ca_x + ca_w;
	int32_t clip_y2 = ca_y + ca_h;

	int32_t row_h = sc(ctx, MKGUI_ITEMVIEW_COMPACT_ROW_H_PX);
	int32_t rows_per_col = ca_h / row_h;
	if(rows_per_col < 1) {
		rows_per_col = 1;
	}

	int32_t col_w = iv->cell_w > 0 ? iv->cell_w : sc(ctx, 180);
	int32_t visible_cols = ca_w / col_w + 2;
	int32_t total_cols = ((int32_t)iv->item_count + rows_per_col - 1) / rows_per_col;
	int32_t first_col_off = iv->scroll_y / col_w;

	char label[MKGUI_MAX_TEXT];

	for(uint32_t vc = 0; vc < (uint32_t)visible_cols; ++vc) {
		int32_t c = first_col_off + (int32_t)vc;
		if(c >= total_cols) {
			break;
		}
		int32_t col_x = ca_x + c * col_w - iv->scroll_y;
		if(col_x >= clip_x2 || col_x + col_w <= ca_x) {
			continue;
		}
		for(uint32_t r = 0; r < (uint32_t)rows_per_col; ++r) {
			int32_t item = c * rows_per_col + (int32_t)r;
			if(item >= (int32_t)iv->item_count) {
				break;
			}

			int32_t cy = ca_y + (int32_t)r * row_h;

			if(item == iv->selected) {
				draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, col_x, cy, col_w, row_h, ctx->theme.selection);
			}

			int32_t item_pad = sc(ctx, 4);
			int32_t tx = col_x + item_pad;
			int32_t icon_idx = itemview_get_icon_idx(iv, (uint32_t)item);
			if(icon_idx >= 0) {
				int32_t iy = cy + (row_h - icons[icon_idx].h) / 2;
				draw_icon(ctx->pixels, ctx->win_w, ctx->win_h, &icons[icon_idx], tx, iy, ca_x, ca_y, clip_x2, clip_y2);
				tx += icons[icon_idx].w + item_pad;
			}

			itemview_get_label(iv, (uint32_t)item, label, sizeof(label));
			if(label[0]) {
				int32_t ty = cy + (row_h - ctx->font_height) / 2;
				uint32_t tc = (item == iv->selected) ? ctx->theme.sel_text : ((ctx->widgets[idx].flags & MKGUI_DISABLED) ? ctx->theme.text_disabled : ctx->theme.text);
				int32_t col_clip_r = col_x + col_w;
				if(col_clip_r > clip_x2) {
					col_clip_r = clip_x2;
				}
				push_text_clip(tx, ty, label, tc, ca_x, ca_y, col_clip_r, clip_y2);
			}
		}
	}
}

// [=]===^=[ render_itemview_detail ]==============================[=]
static void render_itemview_detail(struct mkgui_ctx *ctx, uint32_t idx, struct mkgui_itemview_data *iv) {
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	int32_t ca_x, ca_y, ca_w, ca_h;
	itemview_content_area(ctx, iv, rx, ry, rw, rh, &ca_x, &ca_y, &ca_w, &ca_h);
	int32_t clip_y2 = ca_y + ca_h;

	int32_t row_h = sc(ctx, MKGUI_ITEMVIEW_DETAIL_ROW_H_PX);
	int32_t first_row = iv->scroll_y / row_h;
	int32_t visible = ca_h / row_h + 2;
	int32_t item_pad = sc(ctx, 4);

	char label[MKGUI_MAX_TEXT];

	for(uint32_t r = 0; r < (uint32_t)visible; ++r) {
		int32_t item = first_row + (int32_t)r;
		if(item >= (int32_t)iv->item_count) {
			break;
		}

		int32_t cy = ca_y + item * row_h - iv->scroll_y;
		if(cy + row_h <= ca_y || cy >= clip_y2) {
			continue;
		}

		uint32_t row_bg = (item & 1) ? ctx->theme.listview_alt : ctx->theme.input_bg;
		if(item == iv->selected) {
			row_bg = ctx->theme.selection;
		}
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ca_x, cy, ca_w, row_h, row_bg);

		int32_t tx = ca_x + item_pad;
		int32_t icon_idx = itemview_get_icon_idx(iv, (uint32_t)item);
		if(icon_idx >= 0) {
			int32_t iy = cy + (row_h - icons[icon_idx].h) / 2;
			draw_icon(ctx->pixels, ctx->win_w, ctx->win_h, &icons[icon_idx], tx, iy, ca_x, ca_y, ca_x + ca_w, clip_y2);
			tx += icons[icon_idx].w + item_pad;
		}

		itemview_get_label(iv, (uint32_t)item, label, sizeof(label));
		if(label[0]) {
			int32_t ty = cy + (row_h - ctx->font_height) / 2;
			uint32_t tc = (item == iv->selected) ? ctx->theme.sel_text : ((ctx->widgets[idx].flags & MKGUI_DISABLED) ? ctx->theme.text_disabled : ctx->theme.text);
			push_text_clip(tx, ty, label, tc, ca_x, ca_y, ca_x + ca_w, clip_y2);
		}
	}
}

// [=]===^=[ render_itemview_scrollbar ]===========================[=]
static void render_itemview_scrollbar(struct mkgui_ctx *ctx, uint32_t idx, struct mkgui_itemview_data *iv) {
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	if(iv->view_mode == MKGUI_VIEW_COMPACT) {
		int32_t ca_x, ca_y, ca_w, ca_h;
		itemview_content_area(ctx, iv, rx, ry, rw, rh, &ca_x, &ca_y, &ca_w, &ca_h);
		int32_t sb_x = ca_x;
		int32_t sb_y = ca_y + ca_h;
		int32_t sb_w = ca_w;
		int32_t sb_h = sc(ctx, MKGUI_ITEMVIEW_HSCROLL_H_PX);
		int32_t sb_inset = sc(ctx, 2);
		int32_t min_thumb = sc(ctx, 20);
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, sb_x, sb_y, sb_w, sb_h, ctx->theme.scrollbar_bg);

		int32_t total_w = itemview_compact_total_w(ctx, iv, ca_h);
		if(total_w > ca_w) {
			int32_t thumb_w = sb_w * ca_w / total_w;
			if(thumb_w < min_thumb) {
				thumb_w = min_thumb;
			}
			int32_t max_scroll = total_w - ca_w;
			if(max_scroll < 1) {
				max_scroll = 1;
			}
			int32_t thumb_x = sb_x + (int32_t)((int64_t)iv->scroll_y * (sb_w - thumb_w) / max_scroll);
			uint32_t thumb_color = (ctx->drag_scrollbar_id == ctx->widgets[idx].id) ? ctx->theme.widget_hover : ctx->theme.scrollbar_thumb;
			draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, thumb_x, sb_y + sb_inset, thumb_w, sb_h - sb_inset * 2, thumb_color, ctx->theme.corner_radius);
		}
	} else {
		int32_t ca_x, ca_y, ca_w, ca_h;
		itemview_content_area(ctx, iv, rx, ry, rw, rh, &ca_x, &ca_y, &ca_w, &ca_h);
		int32_t sb_x = ca_x + ca_w;
		int32_t sb_y = ca_y;
		int32_t sb_h = ca_h;
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, sb_x, sb_y, ctx->scrollbar_w, sb_h, ctx->theme.scrollbar_bg);

		int32_t sb_inset = sc(ctx, 2);
		int32_t min_thumb = sc(ctx, 20);
		int32_t total = itemview_total_height(ctx, iv, ca_w);
		if(total > ca_h) {
			int32_t thumb_h = sb_h * ca_h / total;
			if(thumb_h < min_thumb) {
				thumb_h = min_thumb;
			}
			int32_t max_scroll = total - ca_h;
			if(max_scroll < 1) {
				max_scroll = 1;
			}
			int32_t thumb_y = sb_y + (int32_t)((int64_t)iv->scroll_y * (sb_h - thumb_h) / max_scroll);
			uint32_t thumb_color = (ctx->drag_scrollbar_id == ctx->widgets[idx].id) ? ctx->theme.widget_hover : ctx->theme.scrollbar_thumb;
			draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, sb_x + sb_inset, thumb_y, ctx->scrollbar_w - sb_inset * 2, thumb_h, thumb_color, ctx->theme.corner_radius);
		}
	}
}

// [=]===^=[ render_itemview ]=====================================[=]
static void render_itemview(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	struct mkgui_itemview_data *iv = find_itemview_data(ctx, w->id);
	if(!iv) {
		return;
	}

	uint32_t focused = (ctx->focus_id == w->id);
	draw_patch(ctx, MKGUI_STYLE_SUNKEN, rx, ry, rw, rh, ctx->theme.input_bg, focused ? ctx->theme.splitter : ctx->theme.widget_border);

	int32_t save_cx1 = render_clip_x1, save_cy1 = render_clip_y1;
	int32_t save_cx2 = render_clip_x2, save_cy2 = render_clip_y2;

	int32_t ca_x, ca_y, ca_w, ca_h;
	itemview_content_area(ctx, iv, rx, ry, rw, rh, &ca_x, &ca_y, &ca_w, &ca_h);
	itemview_clamp_scroll(ctx, iv, ca_w, ca_h);
	if(ca_x > render_clip_x1) {
		render_clip_x1 = ca_x;
	}
	if(ca_y > render_clip_y1) {
		render_clip_y1 = ca_y;
	}
	if(ca_x + ca_w < render_clip_x2) {
		render_clip_x2 = ca_x + ca_w;
	}
	if(ca_y + ca_h < render_clip_y2) {
		render_clip_y2 = ca_y + ca_h;
	}

	switch(iv->view_mode) {
		case MKGUI_VIEW_ICON: {
			render_itemview_icon(ctx, idx, iv);
		} break;

		case MKGUI_VIEW_THUMBNAIL: {
			render_itemview_thumbnail(ctx, idx, iv);
		} break;

		case MKGUI_VIEW_COMPACT: {
			render_itemview_compact(ctx, idx, iv);
		} break;

		case MKGUI_VIEW_DETAIL: {
			render_itemview_detail(ctx, idx, iv);
		} break;

		default: {
		} break;
	}

	render_clip_x1 = save_cx1;
	render_clip_y1 = save_cy1;
	render_clip_x2 = save_cx2;
	render_clip_y2 = save_cy2;

	render_itemview_scrollbar(ctx, idx, iv);
}

// [=]===^=[ itemview_scrollbar_hit ]==============================[=]
// returns: 0=miss, 1=on thumb, 2=before thumb, 3=after thumb
static uint32_t itemview_scrollbar_hit(struct mkgui_ctx *ctx, uint32_t idx, int32_t mx, int32_t my) {
	struct mkgui_itemview_data *iv = find_itemview_data(ctx, ctx->widgets[idx].id);
	if(!iv) {
		return 0;
	}

	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;
	int32_t ca_x, ca_y, ca_w, ca_h;
	itemview_content_area(ctx, iv, rx, ry, rw, rh, &ca_x, &ca_y, &ca_w, &ca_h);

	int32_t min_thumb = sc(ctx, 20);
	if(iv->view_mode == MKGUI_VIEW_COMPACT) {
		int32_t sb_y = ca_y + ca_h;
		if(mx < ca_x || mx >= ca_x + ca_w || my < sb_y || my >= sb_y + sc(ctx, MKGUI_ITEMVIEW_HSCROLL_H_PX)) {
			return 0;
		}
		int32_t total_w = itemview_compact_total_w(ctx, iv, ca_h);
		if(total_w <= ca_w) {
			return 1;
		}
		int32_t thumb_w = ca_w * ca_w / total_w;
		if(thumb_w < min_thumb) {
			thumb_w = min_thumb;
		}
		int32_t max_scroll = total_w - ca_w;
		int32_t thumb_x = ca_x + (int32_t)((int64_t)iv->scroll_y * (ca_w - thumb_w) / max_scroll);
		if(mx < thumb_x) {
			return 2;
		}
		if(mx >= thumb_x + thumb_w) {
			return 3;
		}
		return 1;
	} else {
		int32_t sb_x = ca_x + ca_w;
		if(mx < sb_x || mx >= sb_x + ctx->scrollbar_w || my < ca_y || my >= ca_y + ca_h) {
			return 0;
		}
		int32_t total = itemview_total_height(ctx, iv, ca_w);
		if(total <= ca_h) {
			return 1;
		}
		int32_t thumb_h = ca_h * ca_h / total;
		if(thumb_h < min_thumb) {
			thumb_h = min_thumb;
		}
		int32_t max_scroll = total - ca_h;
		int32_t thumb_y = ca_y + (int32_t)((int64_t)iv->scroll_y * (ca_h - thumb_h) / max_scroll);
		if(my < thumb_y) {
			return 2;
		}
		if(my >= thumb_y + thumb_h) {
			return 3;
		}
		return 1;
	}
}

// [=]===^=[ itemview_thumb_offset ]===============================[=]
static int32_t itemview_thumb_offset(struct mkgui_ctx *ctx, uint32_t idx, int32_t mx, int32_t my) {
	struct mkgui_itemview_data *iv = find_itemview_data(ctx, ctx->widgets[idx].id);
	if(!iv) {
		return 0;
	}

	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;
	int32_t ca_x, ca_y, ca_w, ca_h;
	itemview_content_area(ctx, iv, rx, ry, rw, rh, &ca_x, &ca_y, &ca_w, &ca_h);

	int32_t min_thumb = sc(ctx, 20);
	if(iv->view_mode == MKGUI_VIEW_COMPACT) {
		int32_t total_w = itemview_compact_total_w(ctx, iv, ca_h);
		if(total_w <= ca_w) {
			return 0;
		}
		int32_t thumb_w = ca_w * ca_w / total_w;
		if(thumb_w < min_thumb) {
			thumb_w = min_thumb;
		}
		int32_t max_scroll = total_w - ca_w;
		int32_t thumb_x = ca_x + (int32_t)((int64_t)iv->scroll_y * (ca_w - thumb_w) / max_scroll);
		return mx - thumb_x;
	} else {
		int32_t total = itemview_total_height(ctx, iv, ca_w);
		if(total <= ca_h) {
			return 0;
		}
		int32_t thumb_h = ca_h * ca_h / total;
		if(thumb_h < min_thumb) {
			thumb_h = min_thumb;
		}
		int32_t max_scroll = total - ca_h;
		int32_t thumb_y = ca_y + (int32_t)((int64_t)iv->scroll_y * (ca_h - thumb_h) / max_scroll);
		return my - thumb_y;
	}
}

// [=]===^=[ itemview_page_scroll ]================================[=]
static void itemview_page_scroll(struct mkgui_ctx *ctx, uint32_t idx, int32_t direction) {
	struct mkgui_itemview_data *iv = find_itemview_data(ctx, ctx->widgets[idx].id);
	if(!iv) {
		return;
	}

	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;
	int32_t ca_x, ca_y, ca_w, ca_h;
	itemview_content_area(ctx, iv, rx, ry, rw, rh, &ca_x, &ca_y, &ca_w, &ca_h);

	if(iv->view_mode == MKGUI_VIEW_COMPACT) {
		iv->scroll_y += direction * ca_w;
	} else {
		iv->scroll_y += direction * ca_h;
	}
	itemview_clamp_scroll(ctx, iv, ca_w, ca_h);
	dirty_all(ctx);
}

// [=]===^=[ itemview_scroll_to_mouse ]============================[=]
static void itemview_scroll_to_mouse(struct mkgui_ctx *ctx, uint32_t widget_id, int32_t mouse_x, int32_t mouse_y) {
	struct mkgui_itemview_data *iv = find_itemview_data(ctx, widget_id);
	if(!iv) {
		return;
	}

	int32_t idx = find_widget_idx(ctx, widget_id);
	if(idx < 0) {
		return;
	}

	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	int32_t ca_x, ca_y, ca_w, ca_h;
	itemview_content_area(ctx, iv, rx, ry, rw, rh, &ca_x, &ca_y, &ca_w, &ca_h);

	int32_t min_thumb = sc(ctx, 20);
	if(iv->view_mode == MKGUI_VIEW_COMPACT) {
		int32_t total_w = itemview_compact_total_w(ctx, iv, ca_h);
		int32_t max_scroll = total_w - ca_w;
		if(max_scroll <= 0) {
			return;
		}
		int32_t sb_w = ca_w;
		int32_t thumb_w = sb_w * ca_w / total_w;
		if(thumb_w < min_thumb) {
			thumb_w = min_thumb;
		}
		int32_t track = sb_w - thumb_w;
		if(track <= 0) {
			return;
		}
		float frac = (float)(mouse_x - ca_x - ctx->drag_scrollbar_offset) / (float)track;
		if(frac < 0.0f) {
			frac = 0.0f;
		}
		if(frac > 1.0f) {
			frac = 1.0f;
		}
		iv->scroll_y = (int32_t)(frac * (float)max_scroll);
	} else {
		int32_t total = itemview_total_height(ctx, iv, ca_w);
		int32_t max_scroll = total - ca_h;
		if(max_scroll <= 0) {
			return;
		}
		int32_t thumb_h = ca_h * ca_h / total;
		if(thumb_h < min_thumb) {
			thumb_h = min_thumb;
		}
		int32_t track = ca_h - thumb_h;
		if(track <= 0) {
			return;
		}
		float frac = (float)(mouse_y - ca_y - ctx->drag_scrollbar_offset) / (float)track;
		if(frac < 0.0f) {
			frac = 0.0f;
		}
		if(frac > 1.0f) {
			frac = 1.0f;
		}
		iv->scroll_y = (int32_t)(frac * (float)max_scroll);
	}

	itemview_clamp_scroll(ctx, iv, ca_w, ca_h);
	dirty_all(ctx);
}

// [=]===^=[ itemview_hit_item ]===================================[=]
static int32_t itemview_hit_item(struct mkgui_ctx *ctx, uint32_t idx, struct mkgui_itemview_data *iv, int32_t mx, int32_t my) {
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	int32_t ca_x, ca_y, ca_w, ca_h;
	itemview_content_area(ctx, iv, rx, ry, rw, rh, &ca_x, &ca_y, &ca_w, &ca_h);

	if(mx < ca_x || mx >= ca_x + ca_w || my < ca_y || my >= ca_y + ca_h) {
		return -1;
	}

	int32_t cw, ch;
	itemview_cell_size(ctx, iv, &cw, &ch);

	if(iv->view_mode == MKGUI_VIEW_DETAIL) {
		int32_t row = (my - ca_y + iv->scroll_y) / ch;
		if(row >= 0 && row < (int32_t)iv->item_count) {
			return row;
		}
		return -1;
	}

	if(iv->view_mode == MKGUI_VIEW_COMPACT) {
		int32_t row_h = sc(ctx, MKGUI_ITEMVIEW_COMPACT_ROW_H_PX);
		int32_t rows_per_col = ca_h / row_h;
		if(rows_per_col < 1) {
			rows_per_col = 1;
		}
		int32_t col_w = iv->cell_w > 0 ? iv->cell_w : sc(ctx, 180);
		int32_t c = (mx - ca_x + iv->scroll_y) / col_w;
		int32_t r = (my - ca_y) / row_h;
		int32_t item = c * rows_per_col + r;
		if(item >= 0 && item < (int32_t)iv->item_count) {
			return item;
		}
		return -1;
	}

	int32_t cols = itemview_grid_cols(ca_w, cw);
	int32_t pad_x = (ca_w - cols * cw) / 2;
	int32_t lx = mx - ca_x - pad_x;
	int32_t ly = my - ca_y + iv->scroll_y;
	if(lx < 0) {
		return -1;
	}
	int32_t col = lx / cw;
	int32_t row = ly / ch;
	if(col >= cols) {
		return -1;
	}
	int32_t item = row * cols + col;
	if(item >= 0 && item < (int32_t)iv->item_count) {
		return item;
	}
	return -1;
}

// [=]===^=[ handle_itemview_key ]=================================[=]
static uint32_t handle_itemview_key(struct mkgui_ctx *ctx, struct mkgui_itemview_data *iv, uint32_t widget_idx, uint32_t keysym, struct mkgui_event *ev) {
	int32_t rx = ctx->rects[widget_idx].x;
	int32_t ry = ctx->rects[widget_idx].y;
	int32_t rw = ctx->rects[widget_idx].w;
	int32_t rh = ctx->rects[widget_idx].h;

	int32_t ca_x, ca_y, ca_w, ca_h;
	itemview_content_area(ctx, iv, rx, ry, rw, rh, &ca_x, &ca_y, &ca_w, &ca_h);
	(void)ca_x;
	(void)ca_y;

	int32_t cw, ch;
	itemview_cell_size(ctx, iv, &cw, &ch);

	int32_t old_sel = iv->selected;

	if(iv->view_mode == MKGUI_VIEW_COMPACT) {
		int32_t row_h = sc(ctx, MKGUI_ITEMVIEW_COMPACT_ROW_H_PX);
		int32_t rows_per_col = ca_h / row_h;
		if(rows_per_col < 1) {
			rows_per_col = 1;
		}
		if(keysym == MKGUI_KEY_UP) {
			if(iv->selected > 0) {
				--iv->selected;
			}
		} else if(keysym == MKGUI_KEY_DOWN) {
			if(iv->selected < (int32_t)iv->item_count - 1) {
				++iv->selected;
			}
		} else if(keysym == MKGUI_KEY_LEFT) {
			if(iv->selected >= rows_per_col) {
				iv->selected -= rows_per_col;
			}
		} else if(keysym == MKGUI_KEY_RIGHT) {
			if(iv->selected + rows_per_col < (int32_t)iv->item_count) {
				iv->selected += rows_per_col;
			}
		} else if(keysym == MKGUI_KEY_PAGE_UP) {
			int32_t page = ca_h / ch;
			iv->selected -= page;
			if(iv->selected < 0) {
				iv->selected = 0;
			}
		} else if(keysym == MKGUI_KEY_PAGE_DOWN) {
			int32_t page = ca_h / ch;
			iv->selected += page;
			if(iv->selected >= (int32_t)iv->item_count) {
				iv->selected = (int32_t)iv->item_count - 1;
			}
		} else if(keysym == MKGUI_KEY_HOME) {
			iv->selected = 0;
		} else if(keysym == MKGUI_KEY_END) {
			iv->selected = (int32_t)iv->item_count - 1;
		}

	} else if(iv->view_mode == MKGUI_VIEW_DETAIL) {
		if(keysym == MKGUI_KEY_UP) {
			if(iv->selected > 0) {
				--iv->selected;
			}
		} else if(keysym == MKGUI_KEY_DOWN) {
			if(iv->selected < (int32_t)iv->item_count - 1) {
				++iv->selected;
			}
		} else if(keysym == MKGUI_KEY_PAGE_UP) {
			int32_t page = ca_h / ch;
			iv->selected -= page;
			if(iv->selected < 0) {
				iv->selected = 0;
			}
		} else if(keysym == MKGUI_KEY_PAGE_DOWN) {
			int32_t page = ca_h / ch;
			iv->selected += page;
			if(iv->selected >= (int32_t)iv->item_count) {
				iv->selected = (int32_t)iv->item_count - 1;
			}
		} else if(keysym == MKGUI_KEY_HOME) {
			iv->selected = 0;
		} else if(keysym == MKGUI_KEY_END) {
			iv->selected = (int32_t)iv->item_count - 1;
		}

	} else {
		int32_t cols = itemview_grid_cols(ca_w, cw);
		if(keysym == MKGUI_KEY_LEFT) {
			if(iv->selected > 0) {
				--iv->selected;
			}
		} else if(keysym == MKGUI_KEY_RIGHT) {
			if(iv->selected < (int32_t)iv->item_count - 1) {
				++iv->selected;
			}
		} else if(keysym == MKGUI_KEY_UP) {
			if(iv->selected >= cols) {
				iv->selected -= cols;
			}
		} else if(keysym == MKGUI_KEY_DOWN) {
			if(iv->selected + cols < (int32_t)iv->item_count) {
				iv->selected += cols;
			}
		} else if(keysym == MKGUI_KEY_PAGE_UP) {
			int32_t page_rows = ca_h / ch;
			if(page_rows < 1) {
				page_rows = 1;
			}
			iv->selected -= page_rows * cols;
			if(iv->selected < 0) {
				iv->selected = 0;
			}
		} else if(keysym == MKGUI_KEY_PAGE_DOWN) {
			int32_t page_rows = ca_h / ch;
			if(page_rows < 1) {
				page_rows = 1;
			}
			iv->selected += page_rows * cols;
			if(iv->selected >= (int32_t)iv->item_count) {
				iv->selected = (int32_t)iv->item_count - 1;
			}
		} else if(keysym == MKGUI_KEY_HOME) {
			iv->selected = 0;
		} else if(keysym == MKGUI_KEY_END) {
			iv->selected = (int32_t)iv->item_count - 1;
		}
	}

	if(iv->selected != old_sel) {
		if(iv->view_mode == MKGUI_VIEW_COMPACT) {
			int32_t row_h = sc(ctx, MKGUI_ITEMVIEW_COMPACT_ROW_H_PX);
			int32_t rows_per_col = ca_h / row_h;
			if(rows_per_col < 1) {
				rows_per_col = 1;
			}
			int32_t col_w = iv->cell_w > 0 ? iv->cell_w : sc(ctx, 180);
			int32_t item_col = iv->selected / rows_per_col;
			int32_t item_x = item_col * col_w;
			if(item_x < iv->scroll_y) {
				iv->scroll_y = item_x;
			}
			if(item_x + col_w > iv->scroll_y + ca_w) {
				iv->scroll_y = item_x + col_w - ca_w;
			}
		} else {
			int32_t item_y;
			if(iv->view_mode == MKGUI_VIEW_DETAIL) {
				item_y = iv->selected * ch;
			} else {
				int32_t cols = itemview_grid_cols(ca_w, cw);
				int32_t row = iv->selected / cols;
				item_y = row * ch;
			}
			if(item_y < iv->scroll_y) {
				iv->scroll_y = item_y;
			}
			if(item_y + ch > iv->scroll_y + ca_h) {
				iv->scroll_y = item_y + ch - ca_h;
			}
		}

		itemview_clamp_scroll(ctx, iv, ca_w, ca_h);
		dirty_all(ctx);
		ev->type = MKGUI_EVENT_ITEMVIEW_SELECT;
		ev->id = ctx->widgets[widget_idx].id;
		ev->value = iv->selected;
		return 1;
	}

	return 0;
}

// [=]===^=[ mkgui_itemview_setup ]================================[=]
MKGUI_API void mkgui_itemview_setup(struct mkgui_ctx *ctx, uint32_t id, uint32_t item_count, uint32_t view_mode, mkgui_itemview_label_cb label_cb, mkgui_itemview_icon_cb icon_cb, void *userdata) {
	MKGUI_CHECK(ctx);
	struct mkgui_itemview_data *iv = find_itemview_data(ctx, id);
	if(!iv) {
		MKGUI_AUX_GROW(ctx->itemviews, ctx->itemview_count, ctx->itemview_cap, struct mkgui_itemview_data);
		if(ctx->itemview_count >= ctx->itemview_cap) {
			return;
		}
		iv = &ctx->itemviews[ctx->itemview_count++];
		memset(iv, 0, sizeof(*iv));
		iv->widget_id = id;
	}
	if(!iv) {
		return;
	}
	iv->item_count = item_count;
	iv->view_mode = view_mode;
	iv->label_cb = label_cb;
	iv->icon_cb = icon_cb;
	iv->userdata = userdata;
	iv->selected = -1;
	iv->scroll_y = 0;
	iv->cell_w = 0;
	iv->cell_h = 0;
	iv->thumbnail_cb = NULL;
	iv->thumb_buf = NULL;
	iv->thumb_size = 0;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_itemview_set_thumbnail ]========================[=]
MKGUI_API void mkgui_itemview_set_thumbnail(struct mkgui_ctx *ctx, uint32_t id, mkgui_thumbnail_cb cb, int32_t thumb_size) {
	MKGUI_CHECK(ctx);
	struct mkgui_itemview_data *iv = find_itemview_data(ctx, id);
	if(!iv) {
		return;
	}
	iv->thumbnail_cb = cb;
	iv->thumb_size = thumb_size > 0 ? thumb_size : sc(ctx, MKGUI_ITEMVIEW_THUMB_SIZE_PX);
	if(iv->thumb_buf) {
		free(iv->thumb_buf);
	}
	iv->thumb_buf = (uint32_t *)calloc((uint32_t)(iv->thumb_size * iv->thumb_size), sizeof(uint32_t));
	dirty_all(ctx);
}

// [=]===^=[ mkgui_itemview_set_view ]=============================[=]
MKGUI_API void mkgui_itemview_set_view(struct mkgui_ctx *ctx, uint32_t id, uint32_t view_mode) {
	MKGUI_CHECK(ctx);
	struct mkgui_itemview_data *iv = find_itemview_data(ctx, id);
	if(!iv) {
		return;
	}
	iv->view_mode = view_mode;
	iv->scroll_y = 0;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_itemview_set_items ]============================[=]
MKGUI_API void mkgui_itemview_set_items(struct mkgui_ctx *ctx, uint32_t id, uint32_t count) {
	MKGUI_CHECK(ctx);
	struct mkgui_itemview_data *iv = find_itemview_data(ctx, id);
	if(!iv) {
		return;
	}
	iv->item_count = count;
	if(iv->selected >= (int32_t)count) {
		iv->selected = (int32_t)count - 1;
	}
	dirty_all(ctx);
}

// [=]===^=[ mkgui_itemview_set_cell_size ]========================[=]
MKGUI_API void mkgui_itemview_set_cell_size(struct mkgui_ctx *ctx, uint32_t id, int32_t w, int32_t h) {
	MKGUI_CHECK(ctx);
	struct mkgui_itemview_data *iv = find_itemview_data(ctx, id);
	if(!iv) {
		return;
	}
	iv->cell_w = sc(ctx, w);
	iv->cell_h = sc(ctx, h);
	dirty_all(ctx);
}

// [=]===^=[ mkgui_itemview_get_selected ]=========================[=]
MKGUI_API int32_t mkgui_itemview_get_selected(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, -1);
	struct mkgui_itemview_data *iv = find_itemview_data(ctx, id);
	return iv ? iv->selected : -1;
}

// [=]===^=[ mkgui_itemview_get_view ]=============================[=]
MKGUI_API uint32_t mkgui_itemview_get_view(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_itemview_data *iv = find_itemview_data(ctx, id);
	return iv ? iv->view_mode : MKGUI_VIEW_ICON;
}

// [=]===^=[ mkgui_itemview_set_selected ]============================[=]
MKGUI_API void mkgui_itemview_set_selected(struct mkgui_ctx *ctx, uint32_t id, int32_t item) {
	MKGUI_CHECK(ctx);
	struct mkgui_itemview_data *iv = find_itemview_data(ctx, id);
	if(!iv) {
		return;
	}
	iv->selected = item;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_itemview_clear_selection ]=========================[=]
MKGUI_API void mkgui_itemview_clear_selection(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK(ctx);
	struct mkgui_itemview_data *iv = find_itemview_data(ctx, id);
	if(!iv) {
		return;
	}
	iv->selected = -1;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_itemview_scroll_to ]===============================[=]
MKGUI_API void mkgui_itemview_scroll_to(struct mkgui_ctx *ctx, uint32_t id, int32_t item) {
	MKGUI_CHECK(ctx);
	struct mkgui_itemview_data *iv = find_itemview_data(ctx, id);
	if(!iv || item < 0 || (uint32_t)item >= iv->item_count) {
		return;
	}
	int32_t idx = find_widget_idx(ctx, id);
	if(idx < 0) {
		return;
	}
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;
	int32_t cx, cy, cw, ch;
	itemview_content_area(ctx, iv, rx, ry, rw, rh, &cx, &cy, &cw, &ch);
	int32_t cell_w, cell_h;
	itemview_cell_size(ctx, iv, &cell_w, &cell_h);
	if(iv->view_mode == MKGUI_VIEW_COMPACT || iv->view_mode == MKGUI_VIEW_DETAIL) {
		int32_t row_y = item * cell_h;
		if(row_y < iv->scroll_y) {
			iv->scroll_y = row_y;
		}
		if(row_y + cell_h > iv->scroll_y + ch) {
			iv->scroll_y = row_y + cell_h - ch;
		}

	} else {
		int32_t cols = cw / cell_w;
		if(cols < 1) {
			cols = 1;
		}
		int32_t row = item / cols;
		int32_t row_y = row * cell_h;
		if(row_y < iv->scroll_y) {
			iv->scroll_y = row_y;
		}
		if(row_y + cell_h > iv->scroll_y + ch) {
			iv->scroll_y = row_y + cell_h - ch;
		}
	}
	if(iv->scroll_y < 0) {
		iv->scroll_y = 0;
	}
	dirty_all(ctx);
}
