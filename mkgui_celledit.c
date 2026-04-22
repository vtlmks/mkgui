// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ celledit_cancel ]========================================[=]
static void celledit_cancel(struct mkgui_ctx *ctx) {
	if(!ctx->cell_edit.active) {
		return;
	}
	uint32_t wid = ctx->cell_edit.widget_id;
	ctx->cell_edit.active = 0;
	dirty_widget_id(ctx, wid);
}

// [=]===^=[ celledit_commit ]========================================[=]
static uint32_t celledit_commit(struct mkgui_ctx *ctx, struct mkgui_event *ev) {
	if(!ctx->cell_edit.active) {
		return 0;
	}
	ev->type = MKGUI_EVENT_CELL_EDIT_COMMIT;
	ev->id = ctx->cell_edit.widget_id;
	ev->value = ctx->cell_edit.row;
	ev->col = ctx->cell_edit.col;
	uint32_t wid = ctx->cell_edit.widget_id;
	ctx->cell_edit.active = 0;
	dirty_widget_id(ctx, wid);
	return 1;
}

// [=]===^=[ celledit_compute_rect_listview ]==========================[=]
static uint32_t celledit_compute_rect_listview(struct mkgui_ctx *ctx, int32_t widx) {
	struct mkgui_cell_edit *ce = &ctx->cell_edit;
	struct mkgui_listview_data *lv = find_listv_data(ctx, ce->widget_id);
	if(!lv) {
		return 0;
	}

	int32_t rx = ctx->rects[widx].x;
	int32_t ry = ctx->rects[widx].y;
	int32_t rh = ctx->rects[widx].h;

	int32_t hh = lv->header_height > 0 ? lv->header_height : ctx->row_height;
	int32_t content_y = ry + hh + 1;
	int32_t content_h = rh - hh - 2;
	int32_t first_row = lv->scroll_y / ctx->row_height;
	int32_t row_offset = ce->row - first_row;
	int32_t row_y = content_y + row_offset * ctx->row_height - (lv->scroll_y % ctx->row_height);

	if(row_y + ctx->row_height <= content_y || row_y >= content_y + content_h) {
		return 0;
	}

	int32_t cx = rx + 1 - lv->scroll_x;
	int32_t col_w = 0;
	for(uint32_t d = 0; d < lv->col_count; ++d) {
		uint32_t c = lv->col_order[d];
		if((int32_t)c == ce->col) {
			col_w = lv->columns[c].width;
			break;
		}
		cx += lv->columns[c].width;
	}

	if(col_w <= 0) {
		return 0;
	}

	ce->cell_x = cx;
	ce->cell_y = row_y;
	ce->cell_w = col_w;
	ce->cell_h = ctx->row_height;
	return 1;
}

// [=]===^=[ celledit_compute_rect_treeview ]==========================[=]
static uint32_t celledit_compute_rect_treeview(struct mkgui_ctx *ctx, int32_t widx) {
	struct mkgui_cell_edit *ce = &ctx->cell_edit;
	struct mkgui_treeview_data *tv = find_treeview_data(ctx, ce->widget_id);
	if(!tv) {
		return 0;
	}

	int32_t rx = ctx->rects[widx].x;
	int32_t ry = ctx->rects[widx].y;
	int32_t rw = ctx->rects[widx].w;
	int32_t rh = ctx->rects[widx].h;

	uint32_t node_idx = UINT32_MAX;
	for(uint32_t i = 0; i < tv->node_count; ++i) {
		if((int32_t)tv->nodes[i].id == ce->row) {
			node_idx = i;
			break;
		}
	}

	if(node_idx == UINT32_MAX) {
		return 0;
	}

	if(!treeview_node_visible(tv, node_idx)) {
		return 0;
	}

	int32_t draw_y = ry + 1 - tv->scroll_y;
	for(uint32_t i = 0; i < node_idx; ++i) {
		if(treeview_node_visible(tv, i)) {
			draw_y += ctx->row_height;
		}
	}
	int32_t row_y = draw_y;

	if(row_y + ctx->row_height <= ry + 1 || row_y >= ry + rh - 1) {
		return 0;
	}

	uint32_t depth = treeview_node_depth(tv, node_idx);
	int32_t tree_indent = sc(ctx, 18);
	int32_t tree_pad = sc(ctx, 4);
	int32_t indent = (int32_t)depth * tree_indent;
	int32_t text_x = rx + tree_pad + indent + tree_indent;

	int32_t ti = tv->nodes[node_idx].icon_idx;
	if(ti >= 0 && (uint32_t)ti < icon_count) {
		int32_t icon_gap = sc(ctx, 2);
		text_x += icons[ti].w + icon_gap;
	}

	ce->cell_x = text_x;
	ce->cell_y = row_y;
	ce->cell_w = rx + rw - 1 - text_x;
	ce->cell_h = ctx->row_height;
	return 1;
}

// [=]===^=[ celledit_compute_rect_gridview ]==========================[=]
static uint32_t celledit_compute_rect_gridview(struct mkgui_ctx *ctx, int32_t widx) {
	struct mkgui_cell_edit *ce = &ctx->cell_edit;
	struct mkgui_gridview_data *gv = find_gridv_data(ctx, ce->widget_id);
	if(!gv) {
		return 0;
	}

	int32_t rx = ctx->rects[widx].x;
	int32_t ry = ctx->rects[widx].y;
	int32_t rh = ctx->rects[widx].h;

	int32_t hh = gv->header_height > 0 ? gv->header_height : ctx->row_height;
	int32_t content_y = ry + hh + 1;
	int32_t content_h = rh - hh - 2;
	int32_t first_row = gv->scroll_y / ctx->row_height;
	int32_t row_offset = ce->row - first_row;
	int32_t row_y = content_y + row_offset * ctx->row_height - (gv->scroll_y % ctx->row_height);

	if(row_y + ctx->row_height <= content_y || row_y >= content_y + content_h) {
		return 0;
	}

	int32_t cx = rx + 1;
	int32_t col_w = 0;
	for(uint32_t c = 0; c < gv->col_count; ++c) {
		if((int32_t)c == ce->col) {
			col_w = gv->columns[c].width;
			break;
		}
		cx += gv->columns[c].width;
	}

	if(col_w <= 0) {
		return 0;
	}

	ce->cell_x = cx;
	ce->cell_y = row_y;
	ce->cell_w = col_w;
	ce->cell_h = ctx->row_height;
	return 1;
}

// [=]===^=[ celledit_compute_rect_itemview ]=========================[=]
static uint32_t celledit_compute_rect_itemview(struct mkgui_ctx *ctx, int32_t widx) {
	struct mkgui_cell_edit *ce = &ctx->cell_edit;
	struct mkgui_itemview_data *iv = find_itemview_data(ctx, ce->widget_id);
	if(!iv) {
		return 0;
	}

	int32_t rx = ctx->rects[widx].x;
	int32_t ry = ctx->rects[widx].y;
	int32_t rw = ctx->rects[widx].w;
	int32_t rh = ctx->rects[widx].h;

	int32_t ca_x, ca_y, ca_w, ca_h;
	itemview_content_area(ctx, iv, rx, ry, rw, rh, &ca_x, &ca_y, &ca_w, &ca_h);
	int32_t item = ce->row;

	if(iv->view_mode == MKGUI_VIEW_DETAIL) {
		int32_t row_h = sc(ctx, 20);
		int32_t cy = ca_y + item * row_h - iv->scroll_y;
		if(cy + row_h <= ca_y || cy >= ca_y + ca_h) {
			return 0;
		}
		int32_t tx = ca_x + sc(ctx, 4);
		int32_t icon_idx = itemview_get_icon_idx(iv, (uint32_t)item);
		if(icon_idx >= 0) {
			tx += icons[icon_idx].w + sc(ctx, 4);
		}
		ce->cell_x = tx;
		ce->cell_y = cy;
		ce->cell_w = ca_x + ca_w - tx;
		ce->cell_h = row_h;
		return 1;

	} else if(iv->view_mode == MKGUI_VIEW_COMPACT) {
		int32_t row_h = sc(ctx, 20);
		int32_t rows_per_col = ca_h / row_h;
		if(rows_per_col < 1) {
			rows_per_col = 1;
		}
		int32_t col_w = iv->cell_w > 0 ? iv->cell_w : sc(ctx, 180);
		int32_t c = item / rows_per_col;
		int32_t r = item % rows_per_col;
		int32_t col_x = ca_x + c * col_w - iv->scroll_y;
		int32_t cy = ca_y + r * row_h;
		if(col_x + col_w <= ca_x || col_x >= ca_x + ca_w) {
			return 0;
		}
		int32_t tx = col_x + sc(ctx, 4);
		int32_t icon_idx = itemview_get_icon_idx(iv, (uint32_t)item);
		if(icon_idx >= 0) {
			tx += icons[icon_idx].w + sc(ctx, 4);
		}
		ce->cell_x = tx;
		ce->cell_y = cy;
		ce->cell_w = col_x + col_w - tx;
		ce->cell_h = row_h;
		return 1;

	} else if(iv->view_mode == MKGUI_VIEW_ICON || iv->view_mode == MKGUI_VIEW_THUMBNAIL) {
		int32_t cw, ch;
		itemview_cell_size(ctx, iv, &cw, &ch);
		int32_t cols = itemview_grid_cols(ca_w, cw);
		int32_t pad_x = (ca_w - cols * cw) / 2;
		int32_t grid_col = item % cols;
		int32_t grid_row = item / cols;
		int32_t cx = ca_x + pad_x + grid_col * cw;
		int32_t cy = ca_y + grid_row * ch - iv->scroll_y;
		if(cy + ch <= ca_y || cy >= ca_y + ca_h) {
			return 0;
		}
		int32_t cell_pad = sc(ctx, 3);
		int32_t label_y;
		if(iv->view_mode == MKGUI_VIEW_ICON) {
			int32_t icon_idx = itemview_get_icon_idx(iv, (uint32_t)item);
			int32_t ic_h = (icon_idx >= 0) ? icons[icon_idx].h : 0;
			label_y = cy + ic_h + sc(ctx, 12);
		} else {
			int32_t ts = iv->thumb_size > 0 ? iv->thumb_size : sc(ctx, 64);
			int32_t thumb_pad = sc(ctx, 4);
			label_y = cy + ts + thumb_pad + thumb_pad;
		}
		ce->cell_x = cx + cell_pad;
		ce->cell_y = label_y;
		ce->cell_w = cw - cell_pad * 2;
		ce->cell_h = ch - (label_y - cy);
		if(ce->cell_h < ctx->font_height) {
			ce->cell_h = ctx->font_height;
		}
		return 1;
	}

	return 0;
}

// [=]===^=[ celledit_compute_rect ]==================================[=]
static uint32_t celledit_compute_rect(struct mkgui_ctx *ctx) {
	struct mkgui_cell_edit *ce = &ctx->cell_edit;
	int32_t widx = find_widget_idx(ctx, ce->widget_id);
	if(widx < 0) {
		return 0;
	}

	struct mkgui_widget *w = &ctx->widgets[widx];
	switch(w->type) {
	case MKGUI_LISTVIEW: {
		return celledit_compute_rect_listview(ctx, widx);
	}

	case MKGUI_TREEVIEW: {
		return celledit_compute_rect_treeview(ctx, widx);
	}

	case MKGUI_GRIDVIEW: {
		return celledit_compute_rect_gridview(ctx, widx);
	}

	case MKGUI_ITEMVIEW: {
		return celledit_compute_rect_itemview(ctx, widx);
	}

	default: {
		return 0;
	}
	}
}

// [=]===^=[ celledit_render ]========================================[=]
static void celledit_render(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_cell_edit *ce = &ctx->cell_edit;
	if(!ce->active || ctx->widgets[idx].id != ce->widget_id) {
		return;
	}

	if(!celledit_compute_rect(ctx)) {
		celledit_cancel(ctx);
		return;
	}

	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	int32_t cx1 = ce->cell_x > rx + 1 ? ce->cell_x : rx + 1;
	int32_t cy1 = ce->cell_y > ry + 1 ? ce->cell_y : ry + 1;
	int32_t cx2 = ce->cell_x + ce->cell_w < rx + rw - 1 ? ce->cell_x + ce->cell_w : rx + rw - 1;
	int32_t cy2 = ce->cell_y + ce->cell_h < ry + rh - 1 ? ce->cell_y + ce->cell_h : ry + rh - 1;

	if(cx2 <= cx1 || cy2 <= cy1) {
		return;
	}

	draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, cx1, cy1, cx2 - cx1, cy2 - cy1, ctx->theme.input_bg);

	int32_t pad = sc(ctx, 4);
	int32_t tx = ce->cell_x + pad - ce->te.scroll_x;
	int32_t ty = ce->cell_y + (ce->cell_h - ctx->font_height) / 2;

	textedit_render(ctx, &ce->te, ce->text, tx, ty, cy1, cy2 - cy1, cx1, cy1, cx2, cy2, ctx->theme.text, 1);
}

// [=]===^=[ celledit_key ]===========================================[=]
static uint32_t celledit_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks, uint32_t keymod, char *buf, int32_t len) {
	struct mkgui_cell_edit *ce = &ctx->cell_edit;
	if(!ce->active) {
		return 0;
	}

	uint32_t shift = (keymod & MKGUI_MOD_SHIFT);
	uint32_t ctrl = (keymod & MKGUI_MOD_CONTROL);

	if(ks == MKGUI_KEY_ESCAPE) {
		celledit_cancel(ctx);
		return 1;
	}

	if(ks == MKGUI_KEY_RETURN) {
		return celledit_commit(ctx, ev);
	}

	if(ks == MKGUI_KEY_TAB) {
		return celledit_commit(ctx, ev);
	}

	if(ctrl) {
		if(ks == 'a' || ks == 'A') {
			textedit_select_all(&ce->te);
			dirty_widget_id(ctx, ce->widget_id);
			return 1;
		}

		if(ks == 'c' || ks == 'C') {
			if(textedit_has_selection(&ce->te)) {
				uint32_t lo, hi;
				textedit_sel_range(&ce->te, &lo, &hi);
				platform_clipboard_set(ctx, ce->text + lo, hi - lo);
			} else {
				platform_clipboard_set(ctx, ce->text, (uint32_t)strlen(ce->text));
			}
			return 1;
		}

		if(ks == 'x' || ks == 'X') {
			if(textedit_has_selection(&ce->te)) {
				uint32_t lo, hi;
				textedit_sel_range(&ce->te, &lo, &hi);
				platform_clipboard_set(ctx, ce->text + lo, hi - lo);
				textedit_delete_selection(&ce->te);
				dirty_widget_id(ctx, ce->widget_id);
			}
			return 1;
		}

		if(ks == 'v' || ks == 'V') {
			char clip_buf[MKGUI_CLIP_MAX];
			uint32_t clip_len = platform_clipboard_get(ctx, clip_buf, sizeof(clip_buf));
			if(clip_len > 0) {
				for(uint32_t ci = 0; ci < clip_len; ++ci) {
					if(clip_buf[ci] == '\n' || clip_buf[ci] == '\r') {
						clip_buf[ci] = ' ';
					}
				}
				textedit_insert(&ce->te, clip_buf, clip_len);
				int32_t visible_w = ce->cell_w - sc(ctx, 4) * 2;
				textedit_scroll_to_cursor(ctx, &ce->te, ce->text, visible_w);
				dirty_widget_id(ctx, ce->widget_id);
			}
			return 1;
		}
		return 1;
	}

	int32_t visible_w = ce->cell_w - sc(ctx, 4) * 2;

	switch(ks) {
	case MKGUI_KEY_LEFT: {
		textedit_move_left(&ce->te, shift);
	} break;

	case MKGUI_KEY_RIGHT: {
		textedit_move_right(&ce->te, shift);
	} break;

	case MKGUI_KEY_HOME: {
		textedit_move_home(&ce->te, shift);
	} break;

	case MKGUI_KEY_END: {
		textedit_move_end(&ce->te, shift);
	} break;

	case MKGUI_KEY_BACKSPACE: {
		textedit_backspace(&ce->te);
	} break;

	case MKGUI_KEY_DELETE: {
		textedit_delete(&ce->te);
	} break;

	default: {
		if(len > 0 && (uint8_t)buf[0] >= 32) {
			textedit_insert(&ce->te, buf, (uint32_t)len);
		} else {
			return 1;
		}
	} break;
	}

	textedit_scroll_to_cursor(ctx, &ce->te, ce->text, visible_w);
	dirty_widget_id(ctx, ce->widget_id);
	return 1;
}

// [=]===^=[ celledit_extract_text ]=================================[=]
// strips icon prefix from MKGUI_CELL_ICON_TEXT format ("icon\tText" -> "Text")
static const char *celledit_extract_text(struct mkgui_ctx *ctx, uint32_t widget_id, int32_t col, const char *raw) {
	if(!raw) {
		return raw;
	}
	struct mkgui_widget *w = find_widget(ctx, widget_id);
	if(!w) {
		return raw;
	}

	if(w->type == MKGUI_LISTVIEW) {
		struct mkgui_listview_data *lv = find_listv_data(ctx, widget_id);
		if(lv && col >= 0 && (uint32_t)col < lv->col_count && lv->columns[col].cell_type == MKGUI_CELL_ICON_TEXT) {
			const char *tab = strchr(raw, '\t');
			if(tab) {
				return tab + 1;
			}
		}
	} else if(w->type == MKGUI_GRIDVIEW) {
		struct mkgui_gridview_data *gv = find_gridv_data(ctx, widget_id);
		if(gv && col >= 0 && (uint32_t)col < gv->col_count && gv->columns[col].col_type == MKGUI_CELL_ICON_TEXT) {
			const char *tab = strchr(raw, '\t');
			if(tab) {
				return tab + 1;
			}
		}
	}
	return raw;
}

// [=]===^=[ celledit_begin ]=========================================[=]
static void celledit_begin(struct mkgui_ctx *ctx, uint32_t widget_id, int32_t row, int32_t col, const char *text) {
	struct mkgui_cell_edit *ce = &ctx->cell_edit;

	if(ce->active) {
		celledit_cancel(ctx);
	}

	ce->widget_id = widget_id;
	ce->row = row;
	ce->col = col;
	const char *display = celledit_extract_text(ctx, widget_id, col, text);
	strncpy(ce->text, display ? display : "", MKGUI_MAX_TEXT - 1);
	ce->text[MKGUI_MAX_TEXT - 1] = '\0';

	uint32_t text_len = (uint32_t)strlen(ce->text);
	ce->te.text = ce->text;
	ce->te.buf_size = MKGUI_MAX_TEXT;
	ce->te.cursor = text_len;
	ce->te.sel_start = 0;
	ce->te.sel_end = text_len;
	ce->te.scroll_x = 0;

	if(!celledit_compute_rect(ctx)) {
		return;
	}

	ce->active = 1;
	ctx->focus_id = widget_id;
	dirty_widget_id(ctx, widget_id);
}

// [=]===^=[ mkgui_cell_edit_begin ]==================================[=]
MKGUI_API void mkgui_cell_edit_begin(struct mkgui_ctx *ctx, uint32_t widget_id, int32_t row, int32_t col, const char *text) {
	MKGUI_CHECK(ctx);
	celledit_begin(ctx, widget_id, row, col, text);
}

// [=]===^=[ mkgui_cell_edit_cancel ]=================================[=]
MKGUI_API void mkgui_cell_edit_cancel(struct mkgui_ctx *ctx) {
	MKGUI_CHECK(ctx);
	celledit_cancel(ctx);
}

// [=]===^=[ mkgui_cell_edit_get_text ]================================[=]
MKGUI_API const char *mkgui_cell_edit_get_text(struct mkgui_ctx *ctx) {
	MKGUI_CHECK_VAL(ctx, "");
	return ctx->cell_edit.text;
}

// [=]===^=[ mkgui_cell_edit_active ]=================================[=]
MKGUI_API uint32_t mkgui_cell_edit_active(struct mkgui_ctx *ctx) {
	MKGUI_CHECK_VAL(ctx, 0);
	return ctx->cell_edit.active;
}
