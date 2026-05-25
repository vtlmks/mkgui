// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ celledit_cancel ]========================================[=]
static void celledit_cancel(struct mkgui_window *win) {
	if(!win->cell_edit.active) {
		return;
	}
	uint32_t wid = win->cell_edit.widget_id;
	win->cell_edit.active = 0;
	dirty_widget_id(win, wid);
}

// [=]===^=[ celledit_commit ]========================================[=]
static uint32_t celledit_commit(struct mkgui_window *win, struct mkgui_event *ev) {
	if(!win->cell_edit.active) {
		return 0;
	}
	ev->type = MKGUI_EVENT_CELL_EDIT_COMMIT;
	ev->id = win->cell_edit.widget_id;
	ev->value = win->cell_edit.row;
	ev->col = win->cell_edit.col;
	uint32_t wid = win->cell_edit.widget_id;
	win->cell_edit.active = 0;
	dirty_widget_id(win, wid);
	return 1;
}

// [=]===^=[ celledit_compute_rect_listview ]==========================[=]
static uint32_t celledit_compute_rect_listview(struct mkgui_window *win, int32_t widx) {
	struct mkgui_cell_edit *ce = &win->cell_edit;
	struct mkgui_listview_data *lv = find_listv_data(win, ce->widget_id);
	if(!lv) {
		return 0;
	}

	int32_t rx = win->rects[widx].x;
	int32_t ry = win->rects[widx].y;
	int32_t rh = win->rects[widx].h;

	int32_t hh = lv->header_height > 0 ? lv->header_height : win->row_height;
	int32_t content_y = ry + hh + 1;
	int32_t content_h = rh - hh - 2;
	int32_t first_row = lv->scroll_y / win->row_height;
	int32_t row_offset = ce->row - first_row;
	int32_t row_y = content_y + row_offset * win->row_height - (lv->scroll_y % win->row_height);

	if(row_y + win->row_height <= content_y || row_y >= content_y + content_h) {
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
	ce->cell_h = win->row_height;
	return 1;
}

// [=]===^=[ celledit_compute_rect_treeview ]==========================[=]
static uint32_t celledit_compute_rect_treeview(struct mkgui_window *win, int32_t widx) {
	struct mkgui_cell_edit *ce = &win->cell_edit;
	struct mkgui_treeview_data *tv = find_treeview_data(win, ce->widget_id);
	if(!tv) {
		return 0;
	}

	int32_t rx = win->rects[widx].x;
	int32_t ry = win->rects[widx].y;
	int32_t rw = win->rects[widx].w;
	int32_t rh = win->rects[widx].h;

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
			draw_y += win->row_height;
		}
	}
	int32_t row_y = draw_y;

	if(row_y + win->row_height <= ry + 1 || row_y >= ry + rh - 1) {
		return 0;
	}

	uint32_t depth = treeview_node_depth(tv, node_idx);
	int32_t tree_indent = sc(win, MKGUI_TREE_INDENT_PX);
	int32_t tree_pad = sc(win, MKGUI_TREE_PAD_PX);
	int32_t indent = (int32_t)depth * tree_indent;
	int32_t text_x = rx + tree_pad + indent + tree_indent;

	int32_t ti = tv->nodes[node_idx].icon_idx;
	if(ti >= 0 && (uint32_t)ti < icon_count) {
		int32_t icon_gap = sc(win, MKGUI_TREE_ICON_GAP_PX);
		text_x += icons[ti].w + icon_gap;
	}

	ce->cell_x = text_x;
	ce->cell_y = row_y;
	ce->cell_w = rx + rw - 1 - text_x;
	ce->cell_h = win->row_height;
	return 1;
}

// [=]===^=[ celledit_compute_rect_gridview ]==========================[=]
static uint32_t celledit_compute_rect_gridview(struct mkgui_window *win, int32_t widx) {
	struct mkgui_cell_edit *ce = &win->cell_edit;
	struct mkgui_gridview_data *gv = find_gridv_data(win, ce->widget_id);
	if(!gv) {
		return 0;
	}

	int32_t rx = win->rects[widx].x;
	int32_t ry = win->rects[widx].y;
	int32_t rh = win->rects[widx].h;

	int32_t hh = gv->header_height > 0 ? gv->header_height : win->row_height;
	int32_t content_y = ry + hh + 1;
	int32_t content_h = rh - hh - 2;
	int32_t first_row = gv->scroll_y / win->row_height;
	int32_t row_offset = ce->row - first_row;
	int32_t row_y = content_y + row_offset * win->row_height - (gv->scroll_y % win->row_height);

	if(row_y + win->row_height <= content_y || row_y >= content_y + content_h) {
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
	ce->cell_h = win->row_height;
	return 1;
}

// [=]===^=[ celledit_compute_rect_itemview ]=========================[=]
static uint32_t celledit_compute_rect_itemview(struct mkgui_window *win, int32_t widx) {
	struct mkgui_cell_edit *ce = &win->cell_edit;
	struct mkgui_itemview_data *iv = find_itemview_data(win, ce->widget_id);
	if(!iv) {
		return 0;
	}

	int32_t rx = win->rects[widx].x;
	int32_t ry = win->rects[widx].y;
	int32_t rw = win->rects[widx].w;
	int32_t rh = win->rects[widx].h;

	int32_t ca_x, ca_y, ca_w, ca_h;
	itemview_content_area(win, iv, rx, ry, rw, rh, &ca_x, &ca_y, &ca_w, &ca_h);
	int32_t item = ce->row;

	if(iv->view_mode == MKGUI_VIEW_DETAIL) {
		int32_t row_h = sc(win, MKGUI_ITEMVIEW_DETAIL_ROW_H_PX);
		int32_t cy = ca_y + item * row_h - iv->scroll_y;
		if(cy + row_h <= ca_y || cy >= ca_y + ca_h) {
			return 0;
		}
		int32_t item_pad = sc(win, MKGUI_ITEMVIEW_ITEM_PAD_PX);
		int32_t tx = ca_x + item_pad;
		int32_t icon_idx = itemview_get_icon_idx(win, iv, (uint32_t)item);
		if(icon_idx >= 0) {
			tx += icons[icon_idx].w + item_pad;
		}
		ce->cell_x = tx;
		ce->cell_y = cy;
		ce->cell_w = ca_x + ca_w - tx;
		ce->cell_h = row_h;
		return 1;

	} else if(iv->view_mode == MKGUI_VIEW_COMPACT) {
		int32_t row_h = sc(win, MKGUI_ITEMVIEW_COMPACT_ROW_H_PX);
		int32_t rows_per_col = ca_h / row_h;
		if(rows_per_col < 1) {
			rows_per_col = 1;
		}
		int32_t col_w = iv->cell_w > 0 ? iv->cell_w : sc(win, MKGUI_ITEMVIEW_COMPACT_COL_W_PX);
		int32_t c = item / rows_per_col;
		int32_t r = item % rows_per_col;
		int32_t col_x = ca_x + c * col_w - iv->scroll_y;
		int32_t cy = ca_y + r * row_h;
		if(col_x + col_w <= ca_x || col_x >= ca_x + ca_w) {
			return 0;
		}
		int32_t item_pad = sc(win, MKGUI_ITEMVIEW_ITEM_PAD_PX);
		int32_t tx = col_x + item_pad;
		int32_t icon_idx = itemview_get_icon_idx(win, iv, (uint32_t)item);
		if(icon_idx >= 0) {
			tx += icons[icon_idx].w + item_pad;
		}
		ce->cell_x = tx;
		ce->cell_y = cy;
		ce->cell_w = col_x + col_w - tx;
		ce->cell_h = row_h;
		return 1;

	} else if(iv->view_mode == MKGUI_VIEW_ICON || iv->view_mode == MKGUI_VIEW_THUMBNAIL) {
		int32_t cw, ch;
		itemview_cell_size(win, iv, &cw, &ch);
		int32_t cols = itemview_grid_cols(ca_w, cw);
		int32_t pad_x = (ca_w - cols * cw) / 2;
		int32_t grid_col = item % cols;
		int32_t grid_row = item / cols;
		int32_t cx = ca_x + pad_x + grid_col * cw;
		int32_t cy = ca_y + grid_row * ch - iv->scroll_y;
		if(cy + ch <= ca_y || cy >= ca_y + ca_h) {
			return 0;
		}
		int32_t cell_pad = sc(win, MKGUI_ITEMVIEW_CELL_PAD_PX);
		int32_t label_y;
		if(iv->view_mode == MKGUI_VIEW_ICON) {
			int32_t icon_idx = itemview_get_icon_idx(win, iv, (uint32_t)item);
			int32_t ic_h = (icon_idx >= 0) ? icons[icon_idx].h : 0;
			label_y = cy + ic_h + sc(win, MKGUI_ITEMVIEW_LABEL_GAP_PX);
		} else {
			int32_t ts = iv->thumb_size > 0 ? iv->thumb_size : sc(win, MKGUI_ITEMVIEW_THUMB_SIZE_PX);
			int32_t thumb_pad = sc(win, MKGUI_ITEMVIEW_THUMB_PAD_PX);
			label_y = cy + ts + thumb_pad + thumb_pad;
		}
		ce->cell_x = cx + cell_pad;
		ce->cell_y = label_y;
		ce->cell_w = cw - cell_pad * 2;
		ce->cell_h = ch - (label_y - cy);
		if(ce->cell_h < win->font_height) {
			ce->cell_h = win->font_height;
		}
		return 1;
	}

	return 0;
}

// [=]===^=[ celledit_compute_rect ]==================================[=]
static uint32_t celledit_compute_rect(struct mkgui_window *win) {
	struct mkgui_cell_edit *ce = &win->cell_edit;
	int32_t widx = find_widget_idx(win, ce->widget_id);
	if(widx < 0) {
		return 0;
	}

	struct mkgui_widget *w = &win->widgets[widx];
	switch(w->type) {
	case MKGUI_LISTVIEW: {
		return celledit_compute_rect_listview(win, widx);
	}

	case MKGUI_TREEVIEW: {
		return celledit_compute_rect_treeview(win, widx);
	}

	case MKGUI_GRIDVIEW: {
		return celledit_compute_rect_gridview(win, widx);
	}

	case MKGUI_ITEMVIEW: {
		return celledit_compute_rect_itemview(win, widx);
	}

	default: {
		return 0;
	}
	}
}

// [=]===^=[ celledit_render ]========================================[=]
static void celledit_render(struct mkgui_window *win, uint32_t idx) {
	struct mkgui_cell_edit *ce = &win->cell_edit;
	if(!ce->active || win->widgets[idx].id != ce->widget_id) {
		return;
	}

	if(!celledit_compute_rect(win)) {
		celledit_cancel(win);
		return;
	}

	int32_t rx = win->rects[idx].x;
	int32_t ry = win->rects[idx].y;
	int32_t rw = win->rects[idx].w;
	int32_t rh = win->rects[idx].h;

	int32_t cx1 = ce->cell_x > rx + 1 ? ce->cell_x : rx + 1;
	int32_t cy1 = ce->cell_y > ry + 1 ? ce->cell_y : ry + 1;
	int32_t cx2 = ce->cell_x + ce->cell_w < rx + rw - 1 ? ce->cell_x + ce->cell_w : rx + rw - 1;
	int32_t cy2 = ce->cell_y + ce->cell_h < ry + rh - 1 ? ce->cell_y + ce->cell_h : ry + rh - 1;

	if(cx2 <= cx1 || cy2 <= cy1) {
		return;
	}

	draw_rect_fill(win->pixels, win->win_w, win->win_h, cx1, cy1, cx2 - cx1, cy2 - cy1, win->theme.input_bg);

	int32_t pad = sc(win, 4);
	int32_t tx = ce->cell_x + pad - ce->te.scroll_x;
	int32_t ty = ce->cell_y + (ce->cell_h - win->font_height) / 2;

	textedit_render(win, &ce->te, ce->text, tx, ty, cy1, cy2 - cy1, cx1, cy1, cx2, cy2, win->theme.text, 1);
}

// [=]===^=[ celledit_key ]===========================================[=]
static uint32_t celledit_key(struct mkgui_window *win, struct mkgui_event *ev, uint32_t ks, uint32_t keymod, char *buf, int32_t len) {
	struct mkgui_cell_edit *ce = &win->cell_edit;
	if(!ce->active) {
		return 0;
	}

	uint32_t shift = (keymod & MKGUI_MOD_SHIFT);
	uint32_t ctrl = (keymod & MKGUI_MOD_CONTROL);

	if(ks == MKGUI_KEY_ESCAPE) {
		celledit_cancel(win);
		return 1;
	}

	if(ks == MKGUI_KEY_RETURN) {
		return celledit_commit(win, ev);
	}

	if(ks == MKGUI_KEY_TAB) {
		return celledit_commit(win, ev);
	}

	if(ctrl) {
		if(ks == 'a' || ks == 'A') {
			textedit_select_all(&ce->te);
			dirty_widget_id(win, ce->widget_id);
			return 1;
		}

		if(ks == 'c' || ks == 'C') {
			if(textedit_has_selection(&ce->te)) {
				uint32_t lo, hi;
				textedit_sel_range(&ce->te, &lo, &hi);
				platform_clipboard_set(win, ce->text + lo, hi - lo);
			} else {
				platform_clipboard_set(win, ce->text, (uint32_t)strlen(ce->text));
			}
			return 1;
		}

		if(ks == 'x' || ks == 'X') {
			if(textedit_has_selection(&ce->te)) {
				uint32_t lo, hi;
				textedit_sel_range(&ce->te, &lo, &hi);
				platform_clipboard_set(win, ce->text + lo, hi - lo);
				textedit_delete_selection(&ce->te);
				dirty_widget_id(win, ce->widget_id);
			}
			return 1;
		}

		if(ks == 'v' || ks == 'V') {
			char clip_buf[MKGUI_CLIP_MAX];
			uint32_t clip_len = platform_clipboard_get(win, clip_buf, sizeof(clip_buf));
			if(clip_len > 0) {
				for(uint32_t ci = 0; ci < clip_len; ++ci) {
					if(clip_buf[ci] == '\n' || clip_buf[ci] == '\r') {
						clip_buf[ci] = ' ';
					}
				}
				textedit_insert(&ce->te, clip_buf, clip_len);
				int32_t visible_w = ce->cell_w - sc(win, 4) * 2;
				textedit_scroll_to_cursor(win, &ce->te, ce->text, visible_w);
				dirty_widget_id(win, ce->widget_id);
			}
			return 1;
		}
		return 1;
	}

	int32_t visible_w = ce->cell_w - sc(win, 4) * 2;

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

	textedit_scroll_to_cursor(win, &ce->te, ce->text, visible_w);
	dirty_widget_id(win, ce->widget_id);
	return 1;
}

// [=]===^=[ celledit_extract_text ]=================================[=]
// strips icon prefix from MKGUI_CELL_ICON_TEXT format ("icon\tText" -> "Text")
static const char *celledit_extract_text(struct mkgui_window *win, uint32_t widget_id, int32_t col, const char *raw) {
	if(!raw) {
		return raw;
	}
	struct mkgui_widget *w = find_widget(win, widget_id);
	if(!w) {
		return raw;
	}

	if(w->type == MKGUI_LISTVIEW) {
		struct mkgui_listview_data *lv = find_listv_data(win, widget_id);
		if(lv && col >= 0 && (uint32_t)col < lv->col_count && lv->columns[col].cell_type == MKGUI_CELL_ICON_TEXT) {
			const char *tab = strchr(raw, '\t');
			if(tab) {
				return tab + 1;
			}
		}
	} else if(w->type == MKGUI_GRIDVIEW) {
		struct mkgui_gridview_data *gv = find_gridv_data(win, widget_id);
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
static void celledit_begin(struct mkgui_window *win, uint32_t widget_id, int32_t row, int32_t col, const char *text) {
	struct mkgui_cell_edit *ce = &win->cell_edit;

	if(ce->active) {
		celledit_cancel(win);
	}

	ce->widget_id = widget_id;
	ce->row = row;
	ce->col = col;
	const char *display = celledit_extract_text(win, widget_id, col, text);
	strncpy(ce->text, display ? display : "", MKGUI_MAX_TEXT - 1);
	ce->text[MKGUI_MAX_TEXT - 1] = '\0';

	uint32_t text_len = (uint32_t)strlen(ce->text);
	ce->te.text = ce->text;
	ce->te.buf_size = MKGUI_MAX_TEXT;
	ce->te.cursor = text_len;
	ce->te.sel_start = 0;
	ce->te.sel_end = text_len;
	ce->te.scroll_x = 0;

	if(!celledit_compute_rect(win)) {
		return;
	}

	ce->active = 1;
	win->focus_id = widget_id;
	dirty_widget_id(win, widget_id);
}

// [=]===^=[ mkgui_cell_edit_begin ]==================================[=]
MKGUI_API void mkgui_cell_edit_begin(struct mkgui_window *win, uint32_t widget_id, int32_t row, int32_t col, const char *text) {
	MKGUI_CHECK(win);
	celledit_begin(win, widget_id, row, col, text);
}

// [=]===^=[ mkgui_cell_edit_cancel ]=================================[=]
MKGUI_API void mkgui_cell_edit_cancel(struct mkgui_window *win) {
	MKGUI_CHECK(win);
	celledit_cancel(win);
}

// [=]===^=[ mkgui_cell_edit_get_text ]================================[=]
MKGUI_API const char *mkgui_cell_edit_get_text(struct mkgui_window *win) {
	MKGUI_CHECK_VAL(win, "");
	return win->cell_edit.text;
}

// [=]===^=[ mkgui_cell_edit_active ]=================================[=]
MKGUI_API uint32_t mkgui_cell_edit_active(struct mkgui_window *win) {
	MKGUI_CHECK_VAL(win, 0);
	return win->cell_edit.active;
}
