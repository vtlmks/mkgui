// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ gridview_get_bit ]===================================[=]
static uint32_t gridview_get_bit(struct mkgui_gridview_data *gv, uint32_t row, uint32_t col) {
	uint32_t bit = row * gv->col_count + col;
	if(bit >= gv->checks_cap) {
		return 0;
	}
	return (gv->checks[bit / 8] >> (bit % 8)) & 1;
}

// [=]===^=[ gridview_set_bit ]===================================[=]
static void gridview_set_bit(struct mkgui_gridview_data *gv, uint32_t row, uint32_t col, uint32_t val) {
	uint32_t bit = row * gv->col_count + col;
	if(bit >= gv->checks_cap) {
		return;
	}
	if(val) {
		gv->checks[bit / 8] |= (uint8_t)(1u << (bit % 8));
	} else {
		gv->checks[bit / 8] &= (uint8_t)~(1u << (bit % 8));
	}
}

// [=]===^=[ gridview_total_col_w ]===============================[=]
static int32_t gridview_total_col_w(struct mkgui_gridview_data *gv) {
	int32_t total = 0;
	for(uint32_t c = 0; c < gv->col_count; ++c) {
		total += gv->columns[c].width;
	}
	return total;
}

// [=]===^=[ gridview_col_at_x ]==================================[=]
static int32_t gridview_col_at_x(struct mkgui_gridview_data *gv, int32_t x, int32_t base_x) {
	int32_t cx = base_x;
	for(uint32_t c = 0; c < gv->col_count; ++c) {
		if(x >= cx && x < cx + gv->columns[c].width) {
			return (int32_t)c;
		}
		cx += gv->columns[c].width;
	}
	return -1;
}

// [=]===^=[ gridview_clamp_scroll ]==============================[=]
static void gridview_clamp_scroll(struct mkgui_ctx *ctx, struct mkgui_gridview_data *gv, int32_t content_h) {
	int32_t total = (int32_t)gv->row_count * ctx->row_height;
	int32_t max_scroll = total - content_h;
	if(max_scroll < 0) {
		max_scroll = 0;
	}
	if(gv->scroll_y < 0) {
		gv->scroll_y = 0;
	}
	if(gv->scroll_y > max_scroll) {
		gv->scroll_y = max_scroll;
	}
}

// [=]===^=[ gridview_draw_checkbox ]=============================[=]
static void gridview_draw_checkbox(struct mkgui_ctx *ctx, int32_t bx, int32_t by, uint32_t checked,
	int32_t clip_left, int32_t clip_top, int32_t clip_right, int32_t clip_bottom) {
	int32_t box_size = sc(ctx, 14);
	if(bx + box_size <= clip_left || bx >= clip_right || by + box_size <= clip_top || by >= clip_bottom) {
		return;
	}
	uint32_t bg = checked ? ctx->theme.splitter : ctx->theme.input_bg;
	draw_patch(ctx, MKGUI_STYLE_SUNKEN, bx, by, box_size, box_size, bg, ctx->theme.widget_border);
	if(checked) {
		int32_t ccx = bx + box_size / 2;
		int32_t ccy = by + box_size / 2;
		int32_t ck1 = sc(ctx, 3);
		int32_t ck2 = sc(ctx, 2);
		int32_t ck3 = sc(ctx, 4);
		draw_aa_line(ctx->pixels, ctx->win_w, ctx->win_h, ccx - ck1, ccy - 1, ccx - 1, ccy + ck2, ctx->theme.sel_text, 2);
		draw_aa_line(ctx->pixels, ctx->win_w, ctx->win_h, ccx - 1, ccy + ck2, ccx + ck3, ccy - ck1, ctx->theme.sel_text, 2);
	}
}

// [=]===^=[ render_gridview ]====================================[=]
static void render_gridview(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	struct mkgui_gridview_data *gv = find_gridv_data(ctx, w->id);
	if(!gv) {
		return;
	}

	uint32_t focused = (ctx->focus_id == w->id);
	draw_patch(ctx, MKGUI_STYLE_SUNKEN, rx, ry, rw, rh, ctx->theme.input_bg, focused ? ctx->theme.splitter : ctx->theme.widget_border);

	int32_t hh = gv->header_height > 0 ? gv->header_height : ctx->row_height;
	int32_t content_w = rw - 2 - ctx->scrollbar_w;
	int32_t clip_left = rx + 1;
	int32_t clip_right = rx + 1 + content_w;

	int32_t content_y = ry + hh + 1;
	int32_t content_h = rh - hh - 2;

	gridview_clamp_scroll(ctx, gv, content_h);

	draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx + 1, ry + 1, content_w, hh, ctx->theme.header_bg);

	uint32_t grid_line_color = blend_pixel(ctx->theme.input_bg, ctx->theme.widget_border, 80);

	int32_t cell_pad = sc(ctx, 4);
	int32_t cx = rx + 1;
	for(uint32_t c = 0; c < gv->col_count; ++c) {
		int32_t cw = gv->columns[c].width;
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
			push_text_clip(cx + cell_pad, ty, gv->columns[c].label, ctx->theme.text, col_left, ry, col_right, ry + hh);
		}
		cx += cw;
		if(cx > clip_left && cx < clip_right) {
			draw_vline(ctx->pixels, ctx->win_w, ctx->win_h, cx, ry + 1, hh, ctx->theme.widget_border);
		}
	}
	draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, rx + 1, ry + hh, content_w, ctx->theme.widget_border);

	int32_t visible_rows = content_h / ctx->row_height;
	int32_t first_row = gv->scroll_y / ctx->row_height;

	int32_t clip_top = content_y;
	int32_t clip_bottom = content_y + content_h;

	int32_t saved_clip_x1 = render_clip_x1;
	int32_t saved_clip_y1 = render_clip_y1;
	int32_t saved_clip_x2 = render_clip_x2;
	int32_t saved_clip_y2 = render_clip_y2;
	if(clip_left > render_clip_x1) {
		render_clip_x1 = clip_left;
	}
	if(clip_top > render_clip_y1) {
		render_clip_y1 = clip_top;
	}
	if(clip_right < render_clip_x2) {
		render_clip_x2 = clip_right;
	}
	if(clip_bottom < render_clip_y2) {
		render_clip_y2 = clip_bottom;
	}

	char cell_buf[MKGUI_MAX_TEXT];
	for(uint32_t r = 0; r <= (uint32_t)(visible_rows + 1) && (first_row + (int32_t)r) < (int32_t)gv->row_count; ++r) {
		int32_t row_idx = first_row + (int32_t)r;
		int32_t row_y = content_y + (int32_t)r * ctx->row_height - (gv->scroll_y % ctx->row_height);

		if(row_y >= clip_bottom || row_y + ctx->row_height <= clip_top) {
			continue;
		}

		int32_t draw_y = row_y < clip_top ? clip_top : row_y;
		int32_t draw_end = row_y + ctx->row_height;
		if(draw_end > clip_bottom) {
			draw_end = clip_bottom;
		}
		int32_t draw_h = draw_end - draw_y;

		uint32_t is_selected = (row_idx == gv->selected_row);
		uint32_t row_bg = (row_idx & 1) ? ctx->theme.listview_alt : ctx->theme.input_bg;
		if(is_selected) {
			row_bg = ctx->theme.selection;
		}
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx + 1, draw_y, content_w, draw_h, row_bg);

		int32_t ty = row_y + (ctx->row_height - ctx->font_height) / 2;
		cx = rx + 1;
		for(uint32_t c = 0; c < gv->col_count; ++c) {
			int32_t col_w = gv->columns[c].width;
			if(cx + col_w <= clip_left) {
				cx += col_w;
				continue;
			}
			if(cx >= clip_right) {
				break;
			}

			int32_t cell_cl = cx;
			int32_t cell_cr = cx + col_w;
			if(cell_cl < clip_left) {
				cell_cl = clip_left;
			}
			if(cell_cr > clip_right) {
				cell_cr = clip_right;
			}

			uint32_t tc = is_selected ? ctx->theme.sel_text : ((w->flags & MKGUI_DISABLED) ? ctx->theme.text_disabled : ctx->theme.text);

			int32_t box_size = sc(ctx, 14);
			switch(gv->columns[c].col_type) {
				case MKGUI_GRID_CHECK: {
					int32_t bx = cx + (col_w - box_size) / 2;
					int32_t by = row_y + (ctx->row_height - box_size) / 2;
					gridview_draw_checkbox(ctx, bx, by, gridview_get_bit(gv, (uint32_t)row_idx, c), cell_cl, clip_top, cell_cr, clip_bottom);
				} break;

				case MKGUI_GRID_CHECK_TEXT: {
					int32_t bx = cx + cell_pad;
					int32_t by = row_y + (ctx->row_height - box_size) / 2;
					gridview_draw_checkbox(ctx, bx, by, gridview_get_bit(gv, (uint32_t)row_idx, c), cell_cl, clip_top, cell_cr, clip_bottom);
					cell_buf[0] = '\0';
					if(gv->cell_cb) {
						gv->cell_cb((uint32_t)row_idx, c, cell_buf, sizeof(cell_buf), gv->userdata);
					}
					push_text_clip(bx + box_size + cell_pad, ty, cell_buf, tc, cell_cl, clip_top, cell_cr, clip_bottom);
				} break;

				default: {
					cell_buf[0] = '\0';
					if(gv->cell_cb) {
						gv->cell_cb((uint32_t)row_idx, c, cell_buf, sizeof(cell_buf), gv->userdata);
					}
					push_text_clip(cx + cell_pad, ty, cell_buf, tc, cell_cl, clip_top, cell_cr, clip_bottom);
				} break;
			}

			cx += col_w;
			if(cx > clip_left && cx < clip_right) {
				draw_vline(ctx->pixels, ctx->win_w, ctx->win_h, cx, draw_y, draw_h, grid_line_color);
			}
		}

		int32_t line_y = row_y + ctx->row_height - 1;
		if(line_y >= clip_top && line_y < clip_bottom) {
			draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, rx + 1, line_y, content_w, grid_line_color);
		}
	}

	render_clip_x1 = saved_clip_x1;
	render_clip_y1 = saved_clip_y1;
	render_clip_x2 = saved_clip_x2;
	render_clip_y2 = saved_clip_y2;

	if(gv->row_count > 0) {
		int32_t sb_x = rx + rw - ctx->scrollbar_w - 1;
		int32_t sb_y = content_y;
		int32_t sb_h = content_h;
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, sb_x, sb_y, ctx->scrollbar_w, sb_h, ctx->theme.scrollbar_bg);

		int32_t total_h = (int32_t)gv->row_count * ctx->row_height;
		if(total_h > sb_h) {
			int32_t min_thumb = sc(ctx, 20);
			int32_t thumb_h = sb_h * sb_h / total_h;
			if(thumb_h < min_thumb) {
				thumb_h = min_thumb;
			}
			int32_t thumb_y = sb_y + (int32_t)((int64_t)gv->scroll_y * (sb_h - thumb_h) / (total_h - sb_h));
			uint32_t thumb_color = (ctx->drag_scrollbar_id == w->id) ? ctx->theme.widget_hover : ctx->theme.scrollbar_thumb;
			int32_t sb_inset = sc(ctx, 2);
			draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, sb_x + sb_inset, thumb_y, ctx->scrollbar_w - sb_inset * 2, thumb_h, thumb_color, ctx->theme.corner_radius);
		}
	}

	if(gv->drag_active && gv->drag_target >= 0) {
		int32_t dhh = gv->header_height > 0 ? gv->header_height : ctx->row_height;
		int32_t dcy = ry + dhh + 1;
		int32_t tgt_y;
		if(gv->drag_target > gv->drag_source) {
			tgt_y = dcy + (gv->drag_target + 1) * ctx->row_height - gv->scroll_y;
		} else {
			tgt_y = dcy + gv->drag_target * ctx->row_height - gv->scroll_y;
		}
		if(tgt_y >= dcy && tgt_y <= ry + rh) {
			draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, rx + 1, tgt_y - 1, content_w, ctx->theme.accent);
			draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, rx + 1, tgt_y, content_w, ctx->theme.accent);
		}
	}
}

// [=]===^=[ gridview_row_hit ]====================================[=]
static int32_t gridview_row_hit(struct mkgui_ctx *ctx, uint32_t idx, int32_t my) {
	int32_t ry = ctx->rects[idx].y;
	int32_t rh = ctx->rects[idx].h;

	struct mkgui_gridview_data *gv = find_gridv_data(ctx, ctx->widgets[idx].id);
	if(!gv) {
		return -1;
	}

	int32_t hh = gv->header_height > 0 ? gv->header_height : ctx->row_height;
	int32_t content_y = ry + hh + 1;
	int32_t content_bottom = ry + rh;

	if(my < content_y || my >= content_bottom) {
		return -1;
	}

	int32_t row = (my - content_y + gv->scroll_y) / ctx->row_height;
	if(row < 0 || row >= (int32_t)gv->row_count) {
		return -1;
	}
	return row;
}

// [=]===^=[ gridview_autosize_col ]===============================[=]
static void gridview_autosize_col(struct mkgui_ctx *ctx, struct mkgui_gridview_data *gv, uint32_t col) {
	if(col >= gv->col_count) {
		return;
	}

	int32_t max_w = text_width(ctx, gv->columns[col].label);
	int32_t box_sz = sc(ctx, 14);
	int32_t pad = sc(ctx, 4);

	char buf[MKGUI_MAX_TEXT];
	for(uint32_t row = 0; row < gv->row_count; ++row) {
		int32_t cell_w = 0;
		switch(gv->columns[col].col_type) {
			case MKGUI_GRID_CHECK: {
				cell_w = box_sz;
			} break;

			case MKGUI_GRID_CHECK_TEXT: {
				buf[0] = '\0';
				if(gv->cell_cb) {
					gv->cell_cb(row, col, buf, sizeof(buf), gv->userdata);
				}
				cell_w = box_sz + pad + text_width(ctx, buf);
			} break;

			default: {
				buf[0] = '\0';
				if(gv->cell_cb) {
					gv->cell_cb(row, col, buf, sizeof(buf), gv->userdata);
				}
				cell_w = text_width(ctx, buf);
			} break;
		}
		if(cell_w > max_w) {
			max_w = cell_w;
		}
	}

	gv->columns[col].width = max_w + sc(ctx, 12);
	dirty_all(ctx);
}

// [=]===^=[ gridview_divider_hit ]================================[=]
static int32_t gridview_divider_hit(struct mkgui_ctx *ctx, uint32_t idx, int32_t mx, int32_t my) {
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;

	struct mkgui_gridview_data *gv = find_gridv_data(ctx, ctx->widgets[idx].id);
	if(!gv) {
		return -1;
	}

	int32_t hh = gv->header_height > 0 ? gv->header_height : ctx->row_height;
	if(my < ry || my >= ry + hh) {
		return -1;
	}

	int32_t div_hit = sc(ctx, 3);
	int32_t cx = rx + 1;
	for(uint32_t c = 0; c < gv->col_count; ++c) {
		cx += gv->columns[c].width;
		if(mx >= cx - div_hit && mx <= cx + div_hit) {
			return (int32_t)c;
		}
	}
	return -1;
}

// [=]===^=[ gridview_scrollbar_hit ]=============================[=]
static uint32_t gridview_scrollbar_hit(struct mkgui_ctx *ctx, uint32_t idx, int32_t mx, int32_t my) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	struct mkgui_gridview_data *gv = find_gridv_data(ctx, w->id);
	if(!gv || gv->row_count == 0) {
		return 0;
	}

	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	int32_t hh = gv->header_height > 0 ? gv->header_height : ctx->row_height;
	int32_t sb_x = rx + rw - ctx->scrollbar_w - 1;
	int32_t sb_y = ry + hh + 1;
	int32_t sb_h = rh - hh - 2;

	if(mx < sb_x || mx >= sb_x + ctx->scrollbar_w || my < sb_y || my >= sb_y + sb_h) {
		return 0;
	}

	int32_t total_h = (int32_t)gv->row_count * ctx->row_height;
	if(total_h <= sb_h) {
		return 0;
	}

	int32_t min_thumb = sc(ctx, 20);
	int32_t thumb_h = sb_h * sb_h / total_h;
	if(thumb_h < min_thumb) {
		thumb_h = min_thumb;
	}
	int32_t thumb_y = sb_y + (int32_t)((int64_t)gv->scroll_y * (sb_h - thumb_h) / (total_h - sb_h));

	if(my >= thumb_y && my < thumb_y + thumb_h) {
		return 1;
	}
	return (my < thumb_y) ? 2 : 3;
}

// [=]===^=[ gridview_thumb_offset ]==============================[=]
static int32_t gridview_thumb_offset(struct mkgui_ctx *ctx, uint32_t idx, int32_t my) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	struct mkgui_gridview_data *gv = find_gridv_data(ctx, w->id);
	if(!gv) {
		return 0;
	}

	int32_t ry = ctx->rects[idx].y;
	int32_t rh = ctx->rects[idx].h;
	int32_t hh = gv->header_height > 0 ? gv->header_height : ctx->row_height;
	int32_t sb_y = ry + hh + 1;
	int32_t sb_h = rh - hh - 2;

	int32_t total_h = (int32_t)gv->row_count * ctx->row_height;
	int32_t min_thumb = sc(ctx, 20);
	int32_t thumb_h = sb_h * sb_h / total_h;
	if(thumb_h < min_thumb) {
		thumb_h = min_thumb;
	}
	int32_t thumb_y = sb_y + (int32_t)((int64_t)gv->scroll_y * (sb_h - thumb_h) / (total_h - sb_h));
	return my - thumb_y;
}

// [=]===^=[ gridview_scroll_to_y ]===============================[=]
static void gridview_scroll_to_y(struct mkgui_ctx *ctx, uint32_t id, int32_t my) {
	int32_t idx = find_widget_idx(ctx, id);
	if(idx < 0) {
		return;
	}
	struct mkgui_gridview_data *gv = find_gridv_data(ctx, id);
	if(!gv) {
		return;
	}

	int32_t ry = ctx->rects[idx].y;
	int32_t rh = ctx->rects[idx].h;
	int32_t hh = gv->header_height > 0 ? gv->header_height : ctx->row_height;
	int32_t sb_y = ry + hh + 1;
	int32_t sb_h = rh - hh - 2;

	int32_t total_h = (int32_t)gv->row_count * ctx->row_height;
	if(total_h <= sb_h) {
		return;
	}
	int32_t min_thumb = sc(ctx, 20);
	int32_t thumb_h = sb_h * sb_h / total_h;
	if(thumb_h < min_thumb) {
		thumb_h = min_thumb;
	}

	int32_t usable = sb_h - thumb_h;
	if(usable < 1) {
		return;
	}

	float frac = (float)(my - sb_y - ctx->drag_scrollbar_offset) / (float)usable;
	if(frac < 0.0f) {
		frac = 0.0f;
	}
	if(frac > 1.0f) {
		frac = 1.0f;
	}

	gv->scroll_y = (int32_t)(frac * (float)(total_h - sb_h));
	dirty_all(ctx);
}

// [=]===^=[ handle_gridview_key ]================================[=]
static uint32_t handle_gridview_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks) {
	struct mkgui_gridview_data *gv = find_gridv_data(ctx, ctx->focus_id);
	if(!gv || gv->row_count == 0) {
		return 0;
	}

	int32_t idx = find_widget_idx(ctx, ctx->focus_id);
	if(idx < 0) {
		return 0;
	}
	int32_t rh = ctx->rects[idx].h;
	int32_t hh = gv->header_height > 0 ? gv->header_height : ctx->row_height;
	int32_t content_h = rh - hh - 2;
	int32_t page = content_h / ctx->row_height;
	if(page < 1) {
		page = 1;
	}

	if(ks == MKGUI_KEY_UP) {
		if(gv->selected_row > 0) {
			--gv->selected_row;
		}
	} else if(ks == MKGUI_KEY_DOWN) {
		if(gv->selected_row < (int32_t)gv->row_count - 1) {
			++gv->selected_row;
		}
	} else if(ks == MKGUI_KEY_PAGE_UP) {
		gv->selected_row -= page;
		if(gv->selected_row < 0) {
			gv->selected_row = 0;
		}
	} else if(ks == MKGUI_KEY_PAGE_DOWN) {
		gv->selected_row += page;
		if(gv->selected_row >= (int32_t)gv->row_count) {
			gv->selected_row = (int32_t)gv->row_count - 1;
		}
	} else if(ks == MKGUI_KEY_HOME) {
		gv->selected_row = 0;
	} else if(ks == MKGUI_KEY_END) {
		gv->selected_row = (int32_t)gv->row_count - 1;
	} else if(ks == MKGUI_KEY_SPACE) {
		if(gv->selected_col >= 0 && gv->selected_col < (int32_t)gv->col_count) {
			uint32_t ct = gv->columns[gv->selected_col].col_type;
			if(ct == MKGUI_GRID_CHECK || ct == MKGUI_GRID_CHECK_TEXT) {
				uint32_t cur = gridview_get_bit(gv, (uint32_t)gv->selected_row, (uint32_t)gv->selected_col);
				gridview_set_bit(gv, (uint32_t)gv->selected_row, (uint32_t)gv->selected_col, !cur);
				dirty_all(ctx);
				ev->type = MKGUI_EVENT_GRID_CHECK;
				ev->id = ctx->focus_id;
				ev->value = gv->selected_row;
				ev->col = gv->selected_col;
				return 1;
			}
		}
		return 0;
	} else if(ks == MKGUI_KEY_LEFT) {
		if(gv->selected_col > 0) {
			--gv->selected_col;
		}
		dirty_all(ctx);
		ev->type = MKGUI_EVENT_GRID_CLICK;
		ev->id = ctx->focus_id;
		ev->value = gv->selected_row;
		ev->col = gv->selected_col;
		return 1;
	} else if(ks == MKGUI_KEY_RIGHT) {
		if(gv->selected_col < (int32_t)gv->col_count - 1) {
			++gv->selected_col;
		}
		dirty_all(ctx);
		ev->type = MKGUI_EVENT_GRID_CLICK;
		ev->id = ctx->focus_id;
		ev->value = gv->selected_row;
		ev->col = gv->selected_col;
		return 1;
	} else {
		return 0;
	}

	int32_t row_y = gv->selected_row * ctx->row_height;
	if(row_y < gv->scroll_y) {
		gv->scroll_y = row_y;
	}
	if(row_y + ctx->row_height > gv->scroll_y + content_h) {
		gv->scroll_y = row_y + ctx->row_height - content_h;
	}
	gridview_clamp_scroll(ctx, gv, content_h);
	dirty_all(ctx);
	ev->type = MKGUI_EVENT_GRID_CLICK;
	ev->id = ctx->focus_id;
	ev->value = gv->selected_row;
	ev->col = gv->selected_col;
	return 1;
}

// [=]===^=[ mkgui_gridview_setup ]===============================[=]
MKGUI_API void mkgui_gridview_setup(struct mkgui_ctx *ctx, uint32_t id, uint32_t row_count, uint32_t col_count,
	struct mkgui_grid_column *columns, mkgui_grid_cell_cb cell_cb, void *userdata) {
	MKGUI_CHECK(ctx);
	if(!columns) {
		return;
	}
	if(!cell_cb) {
		return;
	}
	struct mkgui_gridview_data *gv = find_gridv_data(ctx, id);
	if(!gv) {
		MKGUI_AUX_GROW(ctx->gridviews, ctx->gridview_count, ctx->gridview_cap, struct mkgui_gridview_data);
		if(ctx->gridview_count >= ctx->gridview_cap) {
			return;
		}
		gv = &ctx->gridviews[ctx->gridview_count++];
		memset(gv, 0, sizeof(*gv));
		gv->widget_id = id;
		gv->selected_row = -1;
		gv->selected_col = -1;
		gv->drag_source = -1;
		gv->drag_target = -1;
	}
	gv->row_count = row_count;
	gv->col_count = col_count > MKGUI_MAX_COLS ? MKGUI_MAX_COLS : col_count;
	for(uint32_t c = 0; c < gv->col_count; ++c) {
		gv->columns[c] = columns[c];
	}
	gv->cell_cb = cell_cb;
	gv->userdata = userdata;
	gv->scroll_y = 0;

	uint32_t total_bits = row_count * gv->col_count;
	uint32_t total_bytes = (total_bits + 7) / 8;
	if(total_bits > gv->checks_cap) {
		free(gv->checks);
		gv->checks = (uint8_t *)calloc(1, total_bytes);
		gv->checks_cap = total_bits;
	} else {
		memset(gv->checks, 0, total_bytes);
	}

	dirty_all(ctx);
}

// [=]===^=[ mkgui_gridview_set_rows ]============================[=]
MKGUI_API void mkgui_gridview_set_rows(struct mkgui_ctx *ctx, uint32_t id, uint32_t row_count) {
	MKGUI_CHECK(ctx);
	struct mkgui_gridview_data *gv = find_gridv_data(ctx, id);
	if(!gv) {
		return;
	}
	uint32_t total_bits = row_count * gv->col_count;
	uint32_t total_bytes = (total_bits + 7) / 8;
	if(total_bits > gv->checks_cap) {
		uint8_t *new_checks = (uint8_t *)calloc(1, total_bytes);
		if(gv->checks) {
			uint32_t old_bytes = (gv->checks_cap + 7) / 8;
			memcpy(new_checks, gv->checks, old_bytes);
			free(gv->checks);
		}
		gv->checks = new_checks;
		gv->checks_cap = total_bits;
	}
	gv->row_count = row_count;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_gridview_get_selected ]========================[=]
MKGUI_API int32_t mkgui_gridview_get_selected(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, -1);
	struct mkgui_gridview_data *gv = find_gridv_data(ctx, id);
	return gv ? gv->selected_row : -1;
}

// [=]===^=[ mkgui_gridview_set_selected ]=========================[=]
MKGUI_API void mkgui_gridview_set_selected(struct mkgui_ctx *ctx, uint32_t id, int32_t row) {
	MKGUI_CHECK(ctx);
	struct mkgui_gridview_data *gv = find_gridv_data(ctx, id);
	if(gv) {
		gv->selected_row = row;
	}
}

// [=]===^=[ mkgui_gridview_get_check ]============================[=]
MKGUI_API uint32_t mkgui_gridview_get_check(struct mkgui_ctx *ctx, uint32_t id, uint32_t row, uint32_t col) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_gridview_data *gv = find_gridv_data(ctx, id);
	if(!gv) {
		return 0;
	}
	return gridview_get_bit(gv, row, col);
}

// [=]===^=[ mkgui_gridview_set_check ]============================[=]
MKGUI_API void mkgui_gridview_set_check(struct mkgui_ctx *ctx, uint32_t id, uint32_t row, uint32_t col, uint32_t checked) {
	MKGUI_CHECK(ctx);
	struct mkgui_gridview_data *gv = find_gridv_data(ctx, id);
	if(!gv) {
		return;
	}
	gridview_set_bit(gv, row, col, checked);
	dirty_all(ctx);
}

// [=]===^=[ mkgui_gridview_get_col_width ]==========================[=]
MKGUI_API int32_t mkgui_gridview_get_col_width(struct mkgui_ctx *ctx, uint32_t id, uint32_t col) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_gridview_data *gv = find_gridv_data(ctx, id);
	if(!gv || col >= gv->col_count) {
		return 0;
	}
	return gv->columns[col].width;
}

// [=]===^=[ mkgui_gridview_set_col_width ]==========================[=]
MKGUI_API void mkgui_gridview_set_col_width(struct mkgui_ctx *ctx, uint32_t id, uint32_t col, int32_t width) {
	MKGUI_CHECK(ctx);
	struct mkgui_gridview_data *gv = find_gridv_data(ctx, id);
	if(!gv || col >= gv->col_count) {
		return;
	}
	gv->columns[col].width = width;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_gridview_scroll_to ]==============================[=]
MKGUI_API void mkgui_gridview_scroll_to(struct mkgui_ctx *ctx, uint32_t id, int32_t row) {
	MKGUI_CHECK(ctx);
	struct mkgui_gridview_data *gv = find_gridv_data(ctx, id);
	if(!gv) {
		return;
	}
	int32_t idx = find_widget_idx(ctx, id);
	if(idx < 0) {
		return;
	}
	int32_t rh = ctx->rects[idx].h;
	int32_t hh = gv->header_height > 0 ? gv->header_height : ctx->row_height;
	int32_t content_h = rh - hh - 2;
	int32_t row_y = row * ctx->row_height;
	if(row_y < gv->scroll_y) {
		gv->scroll_y = row_y;
	}
	if(row_y + ctx->row_height > gv->scroll_y + content_h) {
		gv->scroll_y = row_y + ctx->row_height - content_h;
	}
	if(gv->scroll_y < 0) {
		gv->scroll_y = 0;
	}
	dirty_all(ctx);
}
