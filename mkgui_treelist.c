// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// Columned tree (tree-table). A listview whose rows form a forest: the
// disclosure twisty and indentation live in logical column 0, every other
// column renders as a listview cell. Column/sort/resize/reorder machinery is
// shared with the listview header path in mkgui.c; the node/expand/collapse
// model is retained here with a rebuilt visible-row index that layout,
// hit-testing and the scrollbar all iterate.

#define MKGUI_TL_INDENT_PX 18
#define MKGUI_TL_ARROW_SIZE_PX 6
#define MKGUI_TL_NONE 0xffffffffu

// File-scope sort context: qsort comparators cannot carry a user pointer, and
// mkgui is single-threaded, so the active tree and key ride here for the
// duration of one sort.
static struct mkgui_treelist_data *tl_sort_tl;
static int32_t tl_sort_col;
static int32_t tl_sort_dir;

// [=]===^=[ tl_stricmp ]=========================================[=]
static int32_t tl_stricmp(char *a, char *b) {
	for(;; ++a, ++b) {
		int32_t ca = (uint8_t)*a;
		int32_t cb = (uint8_t)*b;
		if(ca >= 'A' && ca <= 'Z') {
			ca += 32;
		}

		if(cb >= 'A' && cb <= 'Z') {
			cb += 32;
		}

		if(ca != cb) {
			return ca - cb;
		}

		if(ca == 0) {
			return 0;
		}
	}
}

// [=]===^=[ treelist_cell ]======================================[=]
static char *treelist_cell(struct mkgui_treelist_data *tl, uint32_t node, uint32_t col) {
	if(col >= tl->col_count || !tl->nodes[node].cells) {
		return "";
	}
	return tl->nodes[node].cells + tl->nodes[node].cell_off[col];
}

// [=]===^=[ tl_multi_sel_find ]==================================[=]
// Selection is stored as stable node handles (1-based), sorted, so it survives
// expand/collapse of unrelated subtrees where visible offsets would not.
static int32_t tl_multi_sel_find(struct mkgui_treelist_data *tl, int32_t handle) {
	uint32_t lo = 0, hi = tl->multi_sel_count;
	while(lo < hi) {
		uint32_t mid = (lo + hi) / 2;
		if(tl->multi_sel[mid] < handle) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}
	return (lo < tl->multi_sel_count && tl->multi_sel[lo] == handle) ? (int32_t)lo : -1;
}

// [=]===^=[ tl_multi_sel_toggle ]================================[=]
static void tl_multi_sel_toggle(struct mkgui_treelist_data *tl, int32_t handle) {
	int32_t idx = tl_multi_sel_find(tl, handle);
	if(idx >= 0) {
		memmove(&tl->multi_sel[idx], &tl->multi_sel[idx + 1], (tl->multi_sel_count - (uint32_t)idx - 1) * sizeof(int32_t));
		--tl->multi_sel_count;
	} else {
		if(tl->multi_sel_count >= MKGUI_MAX_MULTI_SEL) {
			return;
		}
		uint32_t lo = 0, hi = tl->multi_sel_count;
		while(lo < hi) {
			uint32_t mid = (lo + hi) / 2;
			if(tl->multi_sel[mid] < handle) {
				lo = mid + 1;
			} else {
				hi = mid;
			}
		}
		memmove(&tl->multi_sel[lo + 1], &tl->multi_sel[lo], (tl->multi_sel_count - lo) * sizeof(int32_t));
		tl->multi_sel[lo] = handle;
		++tl->multi_sel_count;
	}
}

// [=]===^=[ tl_visible_visit ]===================================[=]
static void tl_visible_visit(struct mkgui_treelist_data *tl, uint32_t node) {
	while(node != MKGUI_TL_NONE) {
		if(tl->visible_count < tl->visible_cap) {
			tl->visible[tl->visible_count++] = node;
		}

		if(tl->nodes[node].has_children && tl->nodes[node].expanded) {
			tl_visible_visit(tl, tl->nodes[node].first_child);
		}
		node = tl->nodes[node].next_sibling;
	}
}

// [=]===^=[ treelist_rebuild_visible ]===========================[=]
static void treelist_rebuild_visible(struct mkgui_treelist_data *tl) {
	if(!tl->dirty_visible) {
		return;
	}

	if(tl->visible_cap < tl->node_count) {
		uint32_t *nv = (uint32_t *)realloc(tl->visible, tl->node_count * sizeof(uint32_t));
		if(!nv) {
			return;
		}
		tl->visible = nv;
		tl->visible_cap = tl->node_count;
	}
	tl->visible_count = 0;
	tl_visible_visit(tl, tl->root_first);
	tl->dirty_visible = 0;
}

// [=]===^=[ treelist_visible_row_of ]============================[=]
static int32_t treelist_visible_row_of(struct mkgui_treelist_data *tl, int32_t handle) {
	if(handle < 1) {
		return -1;
	}
	uint32_t node = (uint32_t)handle - 1;
	for(uint32_t i = 0; i < tl->visible_count; ++i) {
		if(tl->visible[i] == node) {
			return (int32_t)i;
		}
	}
	return -1;
}

// [=]===^=[ treelist_total_col_w ]===============================[=]
static int32_t treelist_total_col_w(struct mkgui_treelist_data *tl) {
	int32_t total = 0;
	for(uint32_t d = 0; d < tl->col_count; ++d) {
		total += tl->columns[tl->col_order[d]].width;
	}
	return total;
}

// [=]===^=[ treelist_col0_x ]====================================[=]
// Display x of logical column 0's left edge; the tree decoration always rides
// with logical column 0 regardless of where the user reordered it.
static int32_t treelist_col0_x(struct mkgui_window *win, uint32_t idx, struct mkgui_treelist_data *tl) {
	int32_t cx = win->rects[idx].x + 1 - tl->scroll_x;
	for(uint32_t d = 0; d < tl->col_count; ++d) {
		if(tl->col_order[d] == 0) {
			break;
		}
		cx += tl->columns[tl->col_order[d]].width;
	}
	return cx;
}

// [=]===^=[ treelist_col_insert_pos ]============================[=]
static int32_t treelist_col_insert_pos(struct mkgui_window *win, struct mkgui_treelist_data *tl, uint32_t idx) {
	int32_t rx = win->rects[idx].x;
	int32_t mx = win->mouse_x;
	int32_t cx = rx + 1 - tl->scroll_x;
	for(uint32_t d = 0; d < tl->col_count; ++d) {
		int32_t cw = tl->columns[tl->col_order[d]].width;
		if(mx < cx + cw / 2) {
			return (int32_t)d;
		}
		cx += cw;
	}
	return (int32_t)tl->col_count;
}

// [=]===^=[ tl_node_cmp ]========================================[=]
static int32_t tl_node_cmp(const void *pa, const void *pb) {
	uint32_t ia = *(const uint32_t *)pa;
	uint32_t ib = *(const uint32_t *)pb;
	char *sa = treelist_cell(tl_sort_tl, ia, (uint32_t)tl_sort_col);
	char *sb = treelist_cell(tl_sort_tl, ib, (uint32_t)tl_sort_col);
	int32_t r;
	switch(tl_sort_tl->columns[tl_sort_col].cell_type) {
		case MKGUI_CELL_SIZE:
		case MKGUI_CELL_DATE:
		case MKGUI_CELL_PROGRESS: {
			int64_t va = strtoll(sa, NULL, 10);
			int64_t vb = strtoll(sb, NULL, 10);
			r = (va < vb) ? -1 : (va > vb) ? 1 : 0;
		} break;

		case MKGUI_CELL_CHECKBOX: {
			r = (sa[0] == '1' ? 1 : 0) - (sb[0] == '1' ? 1 : 0);
		} break;

		default: {
			r = tl_stricmp(sa, sb);
		} break;
	}

	if(r == 0) {
		// Stable within a key: insertion order == node index, links reorder but
		// the array never moves, so ties keep their original relative order.
		r = (ia < ib) ? -1 : (ia > ib) ? 1 : 0;
	}
	return tl_sort_dir >= 0 ? r : -r;
}

// [=]===^=[ treelist_sort_chain ]================================[=]
// Sorts one sibling chain in place by relinking next_sibling, then recurses
// into each node's children. Parent links are never touched, so no child is
// ever promoted or moved across parents.
static void treelist_sort_chain(struct mkgui_treelist_data *tl, uint32_t head, uint32_t *out_first, uint32_t *out_last) {
	uint32_t count = 0;
	for(uint32_t n = head; n != MKGUI_TL_NONE; n = tl->nodes[n].next_sibling) {
		++count;
	}

	if(count == 0) {
		*out_first = MKGUI_TL_NONE;
		*out_last = MKGUI_TL_NONE;
		return;
	}

	uint32_t *arr = (uint32_t *)malloc(count * sizeof(uint32_t));
	if(!arr) {
		uint32_t last = head;
		while(tl->nodes[last].next_sibling != MKGUI_TL_NONE) {
			last = tl->nodes[last].next_sibling;
		}
		*out_first = head;
		*out_last = last;
		return;
	}

	uint32_t i = 0;
	for(uint32_t n = head; n != MKGUI_TL_NONE; n = tl->nodes[n].next_sibling) {
		arr[i++] = n;
	}
	qsort(arr, count, sizeof(uint32_t), tl_node_cmp);
	for(uint32_t k = 0; k < count; ++k) {
		tl->nodes[arr[k]].next_sibling = (k + 1 < count) ? arr[k + 1] : MKGUI_TL_NONE;
	}
	*out_first = arr[0];
	*out_last = arr[count - 1];
	free(arr);

	// Walk the freshly relinked list (not arr, which is gone) so the recursion
	// can reuse the same scratch pattern without aliasing.
	for(uint32_t n = *out_first; n != MKGUI_TL_NONE; n = tl->nodes[n].next_sibling) {
		if(tl->nodes[n].has_children) {
			uint32_t cf, cl;
			treelist_sort_chain(tl, tl->nodes[n].first_child, &cf, &cl);
			tl->nodes[n].first_child = cf;
			tl->nodes[n].last_child = cl;
		}
	}
}

// [=]===^=[ treelist_do_sort ]===================================[=]
static void treelist_do_sort(struct mkgui_treelist_data *tl) {
	if(tl->sort_col < 0 || (uint32_t)tl->sort_col >= tl->col_count || tl->node_count == 0) {
		return;
	}
	tl_sort_tl = tl;
	tl_sort_col = tl->sort_col;
	tl_sort_dir = tl->sort_dir;
	uint32_t f, l;
	treelist_sort_chain(tl, tl->root_first, &f, &l);
	tl->root_first = f;
	tl->root_last = l;
	tl->dirty_visible = 1;
}

// [=]===^=[ render_treelist ]====================================[=]
static void render_treelist(struct mkgui_window *win, uint32_t idx) {
	struct mkgui_widget *w = &win->widgets[idx];
	int32_t rx = win->rects[idx].x;
	int32_t ry = win->rects[idx].y;
	int32_t rw = win->rects[idx].w;
	int32_t rh = win->rects[idx].h;

	struct mkgui_treelist_data *tl = find_treelist_data(win, w->id);
	if(!tl) {
		return;
	}
	treelist_rebuild_visible(tl);

	uint32_t disabled = (w->flags & MKGUI_DISABLED);
	uint32_t focused = (win->focus_id == w->id);
	uint32_t frame_bg = disabled_blend(win->theme.input_bg, win->theme.bg, disabled);
	uint32_t frame_border = disabled_blend(focused ? win->theme.highlight : win->theme.widget_border, win->theme.bg, disabled);
	draw_patch(win, MKGUI_STYLE_SUNKEN, rx, ry, rw, rh, frame_bg, frame_border);

	int32_t hh = tl->header_height > 0 ? tl->header_height : win->row_height;
	int32_t content_w = rw - 2 - win->scrollbar_w;
	int32_t clip_left = rx + 1;
	int32_t clip_right = rx + 1 + content_w;
	int32_t total_col_w = treelist_total_col_w(tl);
	uint32_t needs_hscroll = (total_col_w > content_w);

	int32_t content_y = ry + hh + 1;
	int32_t content_h = rh - hh - 2;
	if(needs_hscroll) {
		content_h -= win->scrollbar_w;
	}

	int32_t max_sx = total_col_w - content_w;
	if(max_sx < 0) {
		max_sx = 0;
	}

	if(tl->scroll_x < 0) {
		tl->scroll_x = 0;
	}

	if(tl->scroll_x > max_sx) {
		tl->scroll_x = max_sx;
	}
	int32_t total_h = (int32_t)tl->visible_count * win->row_height;
	int32_t max_sy = total_h - content_h;
	if(max_sy < 0) {
		max_sy = 0;
	}

	if(tl->scroll_y < 0) {
		tl->scroll_y = 0;
	}

	if(tl->scroll_y > max_sy) {
		tl->scroll_y = max_sy;
	}
	int32_t sx = tl->scroll_x;

	uint32_t header_bg = disabled_blend(win->theme.header_bg, win->theme.bg, disabled);
	draw_rect_fill(win->pixels, win->win_w, win->win_h, rx + 1, ry + 1, content_w, hh, header_bg);

	int32_t hdr_pad = sc(win, 4);
	int32_t sort_icon_w = sc(win, 12);
	int32_t sort_arr_w = sc(win, 7);
	int32_t sort_arr_h = sc(win, 4);

	int32_t cx = rx + 1 - sx;
	for(uint32_t d = 0; d < tl->col_count; ++d) {
		uint32_t c = tl->col_order[d];
		int32_t cw = tl->columns[c].width;
		int32_t col_left = cx < clip_left ? clip_left : cx;
		int32_t col_right = cx + cw > clip_right ? clip_right : cx + cw;
		if(col_left < col_right) {
			int32_t ty = ry + (hh - win->font_height) / 2;
			push_text_clip(cx + hdr_pad, ty, tl->columns[c].label, win->theme.text, col_left, ry, col_right, ry + hh);

			if(tl->sort_col == (int32_t)c) {
				int32_t sacx = cx + cw - sort_icon_w + sort_arr_w / 2;
				int32_t say = ry + hh / 2;
				if(sacx - sort_arr_w / 2 >= clip_left && sacx + sort_arr_w / 2 <= clip_right) {
					if(tl->sort_dir > 0) {
						draw_triangle_aa(win->pixels, win->win_w, win->win_h, sacx, say - sort_arr_h / 2, sacx - sort_arr_w / 2, say + sort_arr_h / 2, sacx + sort_arr_w / 2, say + sort_arr_h / 2, win->theme.text);
					} else {
						draw_triangle_aa(win->pixels, win->win_w, win->win_h, sacx - sort_arr_w / 2, say - sort_arr_h / 2, sacx + sort_arr_w / 2, say - sort_arr_h / 2, sacx, say + sort_arr_h / 2, win->theme.text);
					}
				}
			}
		}

		cx += cw;
		if(cx > clip_left && cx < clip_right) {
			draw_vline(win->pixels, win->win_w, win->win_h, cx, ry + 1, hh, win->theme.widget_border);
		}
	}
	draw_hline(win->pixels, win->win_w, win->win_h, rx + 1, ry + hh, content_w, win->theme.widget_border);

	if(win->drag_col_id == w->id && win->drag_col_src >= 0) {
		int32_t insert_d = treelist_col_insert_pos(win, tl, idx);
		if(insert_d >= 0) {
			int32_t ix = rx + 1 - sx;
			for(uint32_t d = 0; d < (uint32_t)insert_d; ++d) {
				ix += tl->columns[tl->col_order[d]].width;
			}

			if(ix >= clip_left && ix <= clip_right) {
				int32_t ind_w = sc(win, 3);
				draw_rect_fill(win->pixels, win->win_w, win->win_h, ix - 1, ry + 1, ind_w, hh, win->theme.highlight);
			}
		}
	}

	int32_t clip_top = content_y;
	int32_t clip_bottom = content_y + content_h;
	int32_t first_row = tl->scroll_y / win->row_height;
	int32_t visible_rows = content_h / win->row_height;

	int32_t cell_pad = sc(win, 4);
	int32_t indent_step = sc(win, MKGUI_TL_INDENT_PX);
	int32_t arrow_size = sc(win, MKGUI_TL_ARROW_SIZE_PX);

	for(uint32_t r = 0; r <= (uint32_t)(visible_rows + 1) && (first_row + (int32_t)r) < (int32_t)tl->visible_count; ++r) {
		int32_t row_idx = first_row + (int32_t)r;
		uint32_t node = tl->visible[row_idx];
		int32_t row_y = content_y + (int32_t)r * win->row_height - (tl->scroll_y % win->row_height);
		if(row_y >= clip_bottom || row_y + win->row_height <= clip_top) {
			continue;
		}

		int32_t draw_y = row_y < clip_top ? clip_top : row_y;
		int32_t draw_end = row_y + win->row_height;
		if(draw_end > clip_bottom) {
			draw_end = clip_bottom;
		}
		int32_t draw_h = draw_end - draw_y;

		int32_t handle = (int32_t)node + 1;
		uint32_t is_selected;
		if(w->style & MKGUI_TREELIST_MULTI_SELECT) {
			is_selected = (tl_multi_sel_find(tl, handle) >= 0);
		} else {
			is_selected = (handle == tl->selected);
		}
		uint32_t row_bg = (row_idx & 1) ? win->theme.listview_alt : win->theme.input_bg;
		if(is_selected) {
			row_bg = win->theme.selection;
		}
		draw_rect_fill(win->pixels, win->win_w, win->win_h, rx + 1, draw_y, content_w, draw_h, row_bg);

		int32_t ty = row_y + (win->row_height - win->font_height) / 2;
		cx = rx + 1 - sx;
		for(uint32_t d = 0; d < tl->col_count; ++d) {
			uint32_t c = tl->col_order[d];
			int32_t col_w = tl->columns[c].width;
			if(cx + col_w <= clip_left) {
				cx += col_w;
				continue;
			}

			if(cx >= clip_right) {
				break;
			}
			int32_t cell_cl = cx < clip_left ? clip_left : cx;
			int32_t cell_cr = cx + col_w > clip_right ? clip_right : cx + col_w;
			uint32_t tc = is_selected ? win->theme.sel_text : ((w->flags & MKGUI_DISABLED) ? win->theme.text_disabled : win->theme.text);
			char *cbuf = treelist_cell(tl, node, c);

			if(c == 0) {
				uint32_t depth = tl->nodes[node].depth;
				int32_t indent_extra = (int32_t)depth * indent_step + indent_step;
				if(tl->nodes[node].has_children) {
					int32_t ax = cx + cell_pad + (int32_t)depth * indent_step;
					int32_t ay = row_y + win->row_height / 2;
					if(ay >= clip_top && ay < clip_bottom && ax + arrow_size > clip_left && ax < clip_right) {
						uint32_t acol = is_selected ? win->theme.sel_text : win->theme.text;
						int32_t as = arrow_size;
						if(tl->nodes[node].expanded) {
							draw_triangle_aa(win->pixels, win->win_w, win->win_h, ax, ay - as / 2, ax + as, ay - as / 2, ax + as / 2, ay + as / 2, acol);
						} else {
							draw_triangle_aa(win->pixels, win->win_w, win->win_h, ax, ay - as / 2, ax + as, ay, ax, ay + as / 2, acol);
						}
					}
				}
				render_cell(win, tl->columns[c].cell_type, cbuf, cx + indent_extra, ty, col_w - indent_extra, cell_cl, cell_cr, clip_top, clip_bottom, tc, row_y);
			} else {
				render_cell(win, tl->columns[c].cell_type, cbuf, cx, ty, col_w, cell_cl, cell_cr, clip_top, clip_bottom, tc, row_y);
			}
			cx += col_w;
		}
	}

	int32_t sb_inset = sc(win, 2);
	int32_t min_thumb = sc(win, 20);

	if(tl->visible_count > 0) {
		int32_t sb_x = rx + rw - win->scrollbar_w - 1;
		int32_t sb_y = content_y;
		int32_t sb_h = content_h;
		draw_rect_fill(win->pixels, win->win_w, win->win_h, sb_x, sb_y, win->scrollbar_w, sb_h, win->theme.scrollbar_bg);

		if(total_h > sb_h) {
			int32_t thumb_h = sb_h * sb_h / total_h;
			if(thumb_h < min_thumb) {
				thumb_h = min_thumb;
			}
			int32_t thumb_y = sb_y + (int32_t)((int64_t)tl->scroll_y * (sb_h - thumb_h) / (total_h - sb_h));
			uint32_t thumb_color = (win->drag_scrollbar_id == w->id && !win->drag_scrollbar_horiz) ? win->theme.widget_hover : win->theme.scrollbar_thumb;
			draw_rounded_rect_fill(win->pixels, win->win_w, win->win_h, sb_x + sb_inset, thumb_y, win->scrollbar_w - sb_inset * 2, thumb_h, thumb_color, win->theme.corner_radius);
		}
	}

	if(needs_hscroll) {
		int32_t hsb_x = rx + 1;
		int32_t hsb_y = ry + rh - win->scrollbar_w - 1;
		int32_t hsb_w = content_w;
		draw_rect_fill(win->pixels, win->win_w, win->win_h, hsb_x, hsb_y, hsb_w, win->scrollbar_w, win->theme.scrollbar_bg);
		int32_t thumb_w = (int32_t)((int64_t)hsb_w * content_w / total_col_w);
		if(thumb_w < min_thumb) {
			thumb_w = min_thumb;
		}
		int32_t thumb_x = hsb_x;
		if(max_sx > 0) {
			thumb_x = hsb_x + (int32_t)((int64_t)sx * (hsb_w - thumb_w) / max_sx);
		}
		uint32_t hthumb_color = (win->drag_scrollbar_id == w->id && win->drag_scrollbar_horiz) ? win->theme.widget_hover : win->theme.scrollbar_thumb;
		draw_rounded_rect_fill(win->pixels, win->win_w, win->win_h, thumb_x, hsb_y + sb_inset, thumb_w, win->scrollbar_w - sb_inset * 2, hthumb_color, win->theme.corner_radius);
	}
}

// [=]===^=[ treelist_header_hit ]================================[=]
static int32_t treelist_header_hit(struct mkgui_window *win, uint32_t idx, int32_t mx, int32_t my) {
	int32_t rx = win->rects[idx].x;
	int32_t ry = win->rects[idx].y;

	struct mkgui_treelist_data *tl = find_treelist_data(win, win->widgets[idx].id);
	if(!tl) {
		return -1;
	}

	int32_t hh = tl->header_height > 0 ? tl->header_height : win->row_height;
	if(my < ry || my >= ry + hh) {
		return -1;
	}

	int32_t cx = rx + 1 - tl->scroll_x;
	for(uint32_t d = 0; d < tl->col_count; ++d) {
		uint32_t c = tl->col_order[d];
		if(mx >= cx && mx < cx + tl->columns[c].width) {
			return (int32_t)d;
		}
		cx += tl->columns[c].width;
	}
	return -1;
}

// [=]===^=[ treelist_divider_hit ]===============================[=]
static int32_t treelist_divider_hit(struct mkgui_window *win, uint32_t idx, int32_t mx, int32_t my) {
	int32_t rx = win->rects[idx].x;
	int32_t ry = win->rects[idx].y;

	struct mkgui_treelist_data *tl = find_treelist_data(win, win->widgets[idx].id);
	if(!tl) {
		return -1;
	}

	int32_t hh = tl->header_height > 0 ? tl->header_height : win->row_height;
	if(my < ry || my >= ry + hh) {
		return -1;
	}

	int32_t div_hit = sc(win, 3);
	int32_t cx = rx + 1 - tl->scroll_x;
	for(uint32_t d = 0; d < tl->col_count; ++d) {
		cx += tl->columns[tl->col_order[d]].width;
		if(mx >= cx - div_hit && mx <= cx + div_hit) {
			return (int32_t)d;
		}
	}
	return -1;
}

// [=]===^=[ treelist_col_hit ]===================================[=]
static int32_t treelist_col_hit(struct mkgui_window *win, uint32_t idx, int32_t mx) {
	int32_t rx = win->rects[idx].x;
	struct mkgui_treelist_data *tl = find_treelist_data(win, win->widgets[idx].id);
	if(!tl) {
		return -1;
	}
	int32_t cx = rx + 1 - tl->scroll_x;
	for(uint32_t d = 0; d < tl->col_count; ++d) {
		uint32_t logical = tl->col_order[d];
		int32_t cw = tl->columns[logical].width;
		if(mx >= cx && mx < cx + cw) {
			return (int32_t)logical;
		}
		cx += cw;
	}
	return -1;
}

// [=]===^=[ treelist_row_hit ]===================================[=]
// Returns the visible-row index (index into visible[]), or -1.
static int32_t treelist_row_hit(struct mkgui_window *win, uint32_t idx, int32_t mx, int32_t my) {
	int32_t rx = win->rects[idx].x;
	int32_t ry = win->rects[idx].y;
	int32_t rw = win->rects[idx].w;
	int32_t rh = win->rects[idx].h;

	struct mkgui_treelist_data *tl = find_treelist_data(win, win->widgets[idx].id);
	if(!tl) {
		return -1;
	}
	treelist_rebuild_visible(tl);

	int32_t hh = tl->header_height > 0 ? tl->header_height : win->row_height;
	int32_t content_y = ry + hh + 1;
	int32_t sb_x = rx + rw - win->scrollbar_w - 1;
	if(mx >= sb_x) {
		return -1;
	}
	int32_t content_bottom = ry + rh;
	if(treelist_total_col_w(tl) > rw - 2 - win->scrollbar_w) {
		content_bottom -= win->scrollbar_w;
	}

	if(my < content_y || my >= content_bottom) {
		return -1;
	}

	int32_t row = (my - content_y + tl->scroll_y) / win->row_height;
	if(row < 0 || row >= (int32_t)tl->visible_count) {
		return -1;
	}
	return row;
}

// [=]===^=[ treelist_twisty_hit ]================================[=]
static uint32_t treelist_twisty_hit(struct mkgui_window *win, uint32_t idx, int32_t mx, uint32_t node) {
	struct mkgui_treelist_data *tl = find_treelist_data(win, win->widgets[idx].id);
	if(!tl || !tl->nodes[node].has_children) {
		return 0;
	}
	int32_t c0 = treelist_col0_x(win, idx, tl);
	int32_t cell_pad = sc(win, 4);
	int32_t indent_step = sc(win, MKGUI_TL_INDENT_PX);
	int32_t tx = c0 + cell_pad + (int32_t)tl->nodes[node].depth * indent_step;
	return (mx >= tx && mx < tx + indent_step);
}

// [=]===^=[ treelist_scrollbar_hit ]=============================[=]
// returns: 0=miss, 1=on thumb, 2=above thumb, 3=below thumb
static uint32_t treelist_scrollbar_hit(struct mkgui_window *win, uint32_t idx, int32_t mx, int32_t my) {
	int32_t rx = win->rects[idx].x;
	int32_t ry = win->rects[idx].y;
	int32_t rw = win->rects[idx].w;
	int32_t rh = win->rects[idx].h;

	struct mkgui_treelist_data *tl = find_treelist_data(win, win->widgets[idx].id);
	if(!tl) {
		return 0;
	}
	treelist_rebuild_visible(tl);

	int32_t hh = tl->header_height > 0 ? tl->header_height : win->row_height;
	int32_t sb_x = rx + rw - win->scrollbar_w - 1;
	int32_t sb_y = ry + hh + 1;
	int32_t sb_h = rh - hh - 2;
	if(treelist_total_col_w(tl) > rw - 2 - win->scrollbar_w) {
		sb_h -= win->scrollbar_w;
	}

	if(mx < sb_x || mx >= sb_x + win->scrollbar_w || my < sb_y || my >= sb_y + sb_h) {
		return 0;
	}

	int32_t total_h = (int32_t)tl->visible_count * win->row_height;
	if(total_h <= sb_h) {
		return 1;
	}

	int32_t min_thumb = sc(win, 20);
	int32_t thumb_h = sb_h * sb_h / total_h;
	if(thumb_h < min_thumb) {
		thumb_h = min_thumb;
	}
	int32_t thumb_y = sb_y + (int32_t)((int64_t)tl->scroll_y * (sb_h - thumb_h) / (total_h - sb_h));

	if(my < thumb_y) {
		return 2;
	}

	if(my >= thumb_y + thumb_h) {
		return 3;
	}
	return 1;
}

// [=]===^=[ treelist_thumb_offset ]==============================[=]
static int32_t treelist_thumb_offset(struct mkgui_window *win, uint32_t idx, int32_t mouse_y) {
	struct mkgui_treelist_data *tl = find_treelist_data(win, win->widgets[idx].id);
	if(!tl) {
		return 0;
	}

	treelist_rebuild_visible(tl);
	int32_t ry = win->rects[idx].y;
	int32_t rh = win->rects[idx].h;
	int32_t rw = win->rects[idx].w;
	int32_t hh = tl->header_height > 0 ? tl->header_height : win->row_height;
	int32_t sb_y = ry + hh + 1;
	int32_t sb_h = rh - hh - 2;
	if(treelist_total_col_w(tl) > rw - 2 - win->scrollbar_w) {
		sb_h -= win->scrollbar_w;
	}

	int32_t total_h = (int32_t)tl->visible_count * win->row_height;
	if(total_h <= sb_h) {
		return 0;
	}

	int32_t min_thumb = sc(win, 20);
	int32_t thumb_h = sb_h * sb_h / total_h;
	if(thumb_h < min_thumb) {
		thumb_h = min_thumb;
	}
	int32_t thumb_y = sb_y + (int32_t)((int64_t)tl->scroll_y * (sb_h - thumb_h) / (total_h - sb_h));
	return mouse_y - thumb_y;
}

// [=]===^=[ treelist_scroll_to_y ]===============================[=]
static void treelist_scroll_to_y(struct mkgui_window *win, uint32_t widget_id, int32_t mouse_y) {
	struct mkgui_treelist_data *tl = find_treelist_data(win, widget_id);
	if(!tl) {
		return;
	}
	int32_t idx = find_widget_idx(win, widget_id);
	if(idx < 0) {
		return;
	}
	treelist_rebuild_visible(tl);

	int32_t ry = win->rects[idx].y;
	int32_t rh = win->rects[idx].h;
	int32_t rw = win->rects[idx].w;
	int32_t hh = tl->header_height > 0 ? tl->header_height : win->row_height;
	int32_t content_y = ry + hh + 1;
	int32_t content_h = rh - hh - 2;
	if(treelist_total_col_w(tl) > rw - 2 - win->scrollbar_w) {
		content_h -= win->scrollbar_w;
	}

	int32_t total_h = (int32_t)tl->visible_count * win->row_height;
	int32_t max_scroll = total_h - content_h;
	if(max_scroll < 0) {
		max_scroll = 0;
	}

	if(content_h <= 0) {
		return;
	}

	int32_t min_thumb = sc(win, 20);
	int32_t thumb_h = content_h * content_h / total_h;
	if(thumb_h < min_thumb) {
		thumb_h = min_thumb;
	}
	int32_t track = content_h - thumb_h;
	if(track <= 0) {
		return;
	}
	float frac = (float)(mouse_y - content_y - win->drag_scrollbar_offset) / (float)track;
	if(frac < 0.0f) {
		frac = 0.0f;
	}

	if(frac > 1.0f) {
		frac = 1.0f;
	}

	tl->scroll_y = (int32_t)(frac * (float)max_scroll);
	dirty_all(win);
}

// [=]===^=[ treelist_page_scroll ]===============================[=]
static void treelist_page_scroll(struct mkgui_window *win, uint32_t idx, int32_t direction) {
	struct mkgui_treelist_data *tl = find_treelist_data(win, win->widgets[idx].id);
	if(!tl) {
		return;
	}

	treelist_rebuild_visible(tl);
	int32_t rh = win->rects[idx].h;
	int32_t rw = win->rects[idx].w;
	int32_t hh = tl->header_height > 0 ? tl->header_height : win->row_height;
	int32_t content_h = rh - hh - 2;
	if(treelist_total_col_w(tl) > rw - 2 - win->scrollbar_w) {
		content_h -= win->scrollbar_w;
	}

	int32_t total_h = (int32_t)tl->visible_count * win->row_height;
	int32_t max_scroll = total_h - content_h;
	if(max_scroll < 0) {
		max_scroll = 0;
	}

	tl->scroll_y += direction * content_h;
	if(tl->scroll_y < 0) {
		tl->scroll_y = 0;
	}

	if(tl->scroll_y > max_scroll) {
		tl->scroll_y = max_scroll;
	}
	dirty_all(win);
}

// [=]===^=[ treelist_hscrollbar_hit ]============================[=]
static uint32_t treelist_hscrollbar_hit(struct mkgui_window *win, uint32_t idx, int32_t mx, int32_t my) {
	int32_t rx = win->rects[idx].x;
	int32_t ry = win->rects[idx].y;
	int32_t rw = win->rects[idx].w;
	int32_t rh = win->rects[idx].h;

	struct mkgui_treelist_data *tl = find_treelist_data(win, win->widgets[idx].id);
	if(!tl) {
		return 0;
	}

	int32_t content_w = rw - 2 - win->scrollbar_w;
	int32_t total_col_w = treelist_total_col_w(tl);
	if(total_col_w <= content_w) {
		return 0;
	}

	int32_t hsb_x = rx + 1;
	int32_t hsb_y = ry + rh - win->scrollbar_w - 1;
	if(mx < hsb_x || mx >= hsb_x + content_w || my < hsb_y || my >= hsb_y + win->scrollbar_w) {
		return 0;
	}

	int32_t min_thumb = sc(win, 20);
	int32_t thumb_w = (int32_t)((int64_t)content_w * content_w / total_col_w);
	if(thumb_w < min_thumb) {
		thumb_w = min_thumb;
	}
	int32_t max_sx = total_col_w - content_w;
	int32_t thumb_x = hsb_x;
	if(max_sx > 0) {
		thumb_x = hsb_x + (int32_t)((int64_t)tl->scroll_x * (content_w - thumb_w) / max_sx);
	}

	if(mx < thumb_x) {
		return 2;
	}

	if(mx >= thumb_x + thumb_w) {
		return 3;
	}
	return 1;
}

// [=]===^=[ treelist_hthumb_offset ]=============================[=]
static int32_t treelist_hthumb_offset(struct mkgui_window *win, uint32_t idx, int32_t mouse_x) {
	struct mkgui_treelist_data *tl = find_treelist_data(win, win->widgets[idx].id);
	if(!tl) {
		return 0;
	}

	int32_t rx = win->rects[idx].x;
	int32_t rw = win->rects[idx].w;
	int32_t content_w = rw - 2 - win->scrollbar_w;
	int32_t total_col_w = treelist_total_col_w(tl);
	if(total_col_w <= content_w) {
		return 0;
	}

	int32_t min_thumb = sc(win, 20);
	int32_t hsb_x = rx + 1;
	int32_t thumb_w = (int32_t)((int64_t)content_w * content_w / total_col_w);
	if(thumb_w < min_thumb) {
		thumb_w = min_thumb;
	}
	int32_t max_sx = total_col_w - content_w;
	int32_t thumb_x = hsb_x + (int32_t)((int64_t)tl->scroll_x * (content_w - thumb_w) / max_sx);
	return mouse_x - thumb_x;
}

// [=]===^=[ treelist_scroll_to_x ]===============================[=]
static void treelist_scroll_to_x(struct mkgui_window *win, uint32_t widget_id, int32_t mouse_x) {
	struct mkgui_treelist_data *tl = find_treelist_data(win, widget_id);
	if(!tl) {
		return;
	}
	int32_t idx = find_widget_idx(win, widget_id);
	if(idx < 0) {
		return;
	}

	int32_t rx = win->rects[idx].x;
	int32_t rw = win->rects[idx].w;
	int32_t content_w = rw - 2 - win->scrollbar_w;
	int32_t total_col_w = treelist_total_col_w(tl);
	int32_t max_sx = total_col_w - content_w;
	if(max_sx <= 0) {
		return;
	}

	int32_t min_thumb = sc(win, 20);
	int32_t hsb_x = rx + 1;
	int32_t thumb_w = (int32_t)((int64_t)content_w * content_w / total_col_w);
	if(thumb_w < min_thumb) {
		thumb_w = min_thumb;
	}
	int32_t track = content_w - thumb_w;
	if(track <= 0) {
		return;
	}
	float frac = (float)(mouse_x - hsb_x - win->drag_scrollbar_offset) / (float)track;
	if(frac < 0.0f) {
		frac = 0.0f;
	}

	if(frac > 1.0f) {
		frac = 1.0f;
	}
	tl->scroll_x = (int32_t)(frac * (float)max_sx);
	dirty_all(win);
}

// [=]===^=[ treelist_clamp_scroll_x ]============================[=]
static void treelist_clamp_scroll_x(struct mkgui_treelist_data *tl, int32_t content_w) {
	int32_t max_scroll = treelist_total_col_w(tl) - content_w;
	if(max_scroll < 0) {
		max_scroll = 0;
	}

	if(tl->scroll_x < 0) {
		tl->scroll_x = 0;
	}

	if(tl->scroll_x > max_scroll) {
		tl->scroll_x = max_scroll;
	}
}

// [=]===^=[ treelist_autosize_col ]==============================[=]
static void treelist_autosize_col(struct mkgui_window *win, struct mkgui_treelist_data *tl, uint32_t col) {
	if(col >= tl->col_count) {
		return;
	}

	int32_t max_w = text_width(win, tl->columns[col].label);
	int32_t cell_pad = sc(win, 4);
	int32_t indent_step = sc(win, MKGUI_TL_INDENT_PX);

	for(uint32_t node = 0; node < tl->node_count; ++node) {
		char *buf = treelist_cell(tl, node, col);
		int32_t cell_w = 0;
		switch(tl->columns[col].cell_type) {
			case MKGUI_CELL_ICON_TEXT: {
				char *tab = strchr(buf, '\t');
				cell_w = text_width(win, tab ? tab + 1 : buf) + win->icon_size + cell_pad;
			} break;

			case MKGUI_CELL_SIZE: {
				char fmt[32];
				format_size(strtoll(buf, NULL, 10), fmt, sizeof(fmt));
				cell_w = text_width(win, fmt);
			} break;

			case MKGUI_CELL_DATE: {
				char fmt[32];
				format_date(buf, fmt, sizeof(fmt));
				cell_w = text_width(win, fmt);
			} break;

			case MKGUI_CELL_CHECKBOX: {
				cell_w = sc(win, 14);
			} break;

			case MKGUI_CELL_PROGRESS: {
				cell_w = 0;
			} break;

			default: {
				cell_w = text_width(win, buf);
			} break;
		}

		if(col == 0) {
			cell_w += (int32_t)tl->nodes[node].depth * indent_step + indent_step;
		}

		if(cell_w > max_w) {
			max_w = cell_w;
		}
	}

	tl->columns[col].width = max_w + sc(win, 12);
	dirty_all(win);
}

// [=]===^=[ handle_treelist_click ]=============================[=]
// Press on a treelist (columned tree). In priority order: horizontal then
// vertical scrollbar (thumb arms drag, trough pages), a column divider
// (double-click autosizes else arms resize), a column header (arms reorder),
// then a visible row: a twisty hit toggles expand/collapse (EXPAND/COLLAPSE),
// otherwise select honoring Ctrl/Shift multi-select via win->press_mod
// (double-click emits DBLCLICK). Returns 1 only when it emits an event; all
// scrollbar/column paths return 0 to keep draining. idx == hit result.
static uint32_t handle_treelist_click(struct mkgui_window *win, struct mkgui_event *ev, uint32_t idx) {
	struct mkgui_widget *hw = &win->widgets[idx];
	uint32_t hsb_hit = treelist_hscrollbar_hit(win, idx, win->mouse_x, win->mouse_y);
	if(hsb_hit == 1) {
		win->drag_scrollbar_id = hw->id;
		win->drag_scrollbar_offset = treelist_hthumb_offset(win, idx, win->mouse_x);
		win->drag_scrollbar_horiz = 1;

	} else if(hsb_hit == 2 || hsb_hit == 3) {
		struct mkgui_treelist_data *htl = find_treelist_data(win, hw->id);
		if(htl) {
			int32_t cw = win->rects[idx].w - 2 - win->scrollbar_w;
			htl->scroll_x += (hsb_hit == 2 ? -1 : 1) * (cw / 2);
			treelist_clamp_scroll_x(htl, cw);
			dirty_widget(win, idx);
		}

	} else {
		uint32_t sb_hit = treelist_scrollbar_hit(win, idx, win->mouse_x, win->mouse_y);
		if(sb_hit == 1) {
			win->drag_scrollbar_id = hw->id;
			win->drag_scrollbar_offset = treelist_thumb_offset(win, idx, win->mouse_y);
			win->drag_scrollbar_horiz = 0;

		} else if(sb_hit == 2 || sb_hit == 3) {
			treelist_page_scroll(win, idx, sb_hit == 2 ? -1 : 1);

		} else {
			int32_t div = treelist_divider_hit(win, idx, win->mouse_x, win->mouse_y);
			if(div >= 0) {
				struct mkgui_treelist_data *tl = find_treelist_data(win, hw->id);
				if(tl) {
					uint32_t logical = tl->col_order[div];
					uint32_t now_ms = (uint32_t)(mkgui_now_ns() / 1000000ull);
					if(win->divider_dblclick_id == hw->id && win->divider_dblclick_col == (int32_t)logical && (now_ms - win->divider_dblclick_time) < 400) {
						treelist_autosize_col(win, tl, logical);
						win->divider_dblclick_id = 0;

					} else {
						win->divider_dblclick_id = hw->id;
						win->divider_dblclick_col = (int32_t)logical;
						win->divider_dblclick_time = now_ms;
						win->drag_col_resize_id = hw->id;
						win->drag_col_resize_col = (int32_t)logical;
						win->drag_col_resize_start_x = win->mouse_x;
						win->drag_col_resize_start_w = tl->columns[logical].width;
					}
				}

			} else {
				int32_t dcol = treelist_header_hit(win, idx, win->mouse_x, win->mouse_y);
				if(dcol >= 0) {
					win->drag_col_id = hw->id;
					win->drag_col_src = dcol;
					win->drag_col_start_x = win->mouse_x;

				} else {
					int32_t vis_row = treelist_row_hit(win, idx, win->mouse_x, win->mouse_y);
					struct mkgui_treelist_data *tl = find_treelist_data(win, hw->id);
					if(tl && vis_row >= 0 && (uint32_t)vis_row < tl->visible_count) {
						uint32_t node = tl->visible[vis_row];
						if(treelist_twisty_hit(win, idx, win->mouse_x, node) && tl->nodes[node].has_children) {
							tl->nodes[node].expanded = !tl->nodes[node].expanded;
							tl->dirty_visible = 1;
							dirty_all(win);
							ev->type = tl->nodes[node].expanded ? MKGUI_EVENT_TREELIST_EXPAND : MKGUI_EVENT_TREELIST_COLLAPSE;
							ev->id = hw->id;
							ev->value = (int32_t)node + 1;
							return 1;
						}

						int32_t handle = (int32_t)node + 1;
						uint32_t now = (uint32_t)(mkgui_now_ns() / 1000000ull);
						uint32_t elapsed = now - win->dblclick_time;
						uint32_t is_dblclick = (win->dblclick_id == hw->id && win->dblclick_row == handle && elapsed < 400);
						win->dblclick_id = hw->id;
						win->dblclick_row = handle;
						win->dblclick_time = now;
						if(hw->style & MKGUI_TREELIST_MULTI_SELECT) {
							uint32_t mod_ctrl = (win->press_mod & MKGUI_MOD_CONTROL) != 0;
							uint32_t mod_shift = (win->press_mod & MKGUI_MOD_SHIFT) != 0;
							if(mod_ctrl) {
								tl_multi_sel_toggle(tl, handle);
								tl->selected = handle;
							} else if(mod_shift) {
								int32_t anchor_row = treelist_visible_row_of(tl, tl->selected);
								if(anchor_row < 0) {
									anchor_row = vis_row;
								}
								int32_t lo = anchor_row < vis_row ? anchor_row : vis_row;
								int32_t rhi = anchor_row < vis_row ? vis_row : anchor_row;
								tl->multi_sel_count = 0;
								for(int32_t r = lo; r <= rhi && tl->multi_sel_count < MKGUI_MAX_MULTI_SEL; ++r) {
									tl_multi_sel_toggle(tl, (int32_t)tl->visible[r] + 1);
								}
								tl->selected = handle;
							} else {
								tl->multi_sel[0] = handle;
								tl->multi_sel_count = 1;
								tl->selected = handle;
							}
						} else {
							tl->selected = handle;
						}
						int32_t col = treelist_col_hit(win, idx, win->mouse_x);
						dirty_widget(win, idx);
						if(is_dblclick) {
							ev->type = MKGUI_EVENT_TREELIST_DBLCLICK;
							ev->id = hw->id;
							ev->value = handle;
							ev->col = col;
							win->dblclick_id = 0;
							return 1;
						}
						ev->type = MKGUI_EVENT_TREELIST_SELECT;
						ev->id = hw->id;
						ev->value = handle;
						ev->col = col;
						return 1;
					}
				}
			}
		}
	}
	return 0;
}

// [=]===^=[ handle_treelist_key ]================================[=]
static uint32_t handle_treelist_key(struct mkgui_window *win, struct mkgui_event *ev, uint32_t ks, uint32_t keymod, char *buf, int32_t len) {
	struct mkgui_treelist_data *tl = find_treelist_data(win, win->focus_id);
	if(!tl) {
		return 0;
	}
	treelist_rebuild_visible(tl);
	if(tl->visible_count == 0) {
		return 0;
	}

	int32_t idx = find_widget_idx(win, win->focus_id);
	int32_t rh = idx >= 0 ? win->rects[idx].h : 0;
	int32_t hh = tl->header_height > 0 ? tl->header_height : win->row_height;
	int32_t content_h = rh - hh - 2;
	int32_t page = content_h / win->row_height;
	if(page < 1) {
		page = 1;
	}

	int32_t cur = treelist_visible_row_of(tl, tl->selected);

	switch(ks) {
	case MKGUI_KEY_RIGHT: {
		if(cur >= 0) {
			uint32_t node = tl->visible[cur];
			if(tl->nodes[node].has_children && !tl->nodes[node].expanded) {
				tl->nodes[node].expanded = 1;
				tl->dirty_visible = 1;
				dirty_all(win);
				ev->type = MKGUI_EVENT_TREELIST_EXPAND;
				ev->id = win->focus_id;
				ev->value = (int32_t)node + 1;
				return 1;
			}

			if(tl->nodes[node].has_children && tl->nodes[node].expanded && tl->nodes[node].first_child != MKGUI_TL_NONE) {
				tl->selected = (int32_t)tl->nodes[node].first_child + 1;
				dirty_all(win);
				ev->type = MKGUI_EVENT_TREELIST_SELECT;
				ev->id = win->focus_id;
				ev->value = tl->selected;
				return 1;
			}
		}
		return 0;
	} break;

	case MKGUI_KEY_LEFT: {
		if(cur >= 0) {
			uint32_t node = tl->visible[cur];
			if(tl->nodes[node].has_children && tl->nodes[node].expanded) {
				tl->nodes[node].expanded = 0;
				tl->dirty_visible = 1;
				dirty_all(win);
				ev->type = MKGUI_EVENT_TREELIST_COLLAPSE;
				ev->id = win->focus_id;
				ev->value = (int32_t)node + 1;
				return 1;
			}

			if(tl->nodes[node].parent != MKGUI_TL_NONE) {
				tl->selected = (int32_t)tl->nodes[node].parent + 1;
				dirty_all(win);
				ev->type = MKGUI_EVENT_TREELIST_SELECT;
				ev->id = win->focus_id;
				ev->value = tl->selected;
				return 1;
			}
		}
		return 0;
	} break;

	case '+':
	case 0xffab: {
		if(cur >= 0) {
			uint32_t node = tl->visible[cur];
			if(tl->nodes[node].has_children && !tl->nodes[node].expanded) {
				tl->nodes[node].expanded = 1;
				tl->dirty_visible = 1;
				dirty_all(win);
				ev->type = MKGUI_EVENT_TREELIST_EXPAND;
				ev->id = win->focus_id;
				ev->value = (int32_t)node + 1;
				return 1;
			}
		}
		return 0;
	} break;

	case '-':
	case 0xffad: {
		if(cur >= 0) {
			uint32_t node = tl->visible[cur];
			if(tl->nodes[node].has_children && tl->nodes[node].expanded) {
				tl->nodes[node].expanded = 0;
				tl->dirty_visible = 1;
				dirty_all(win);
				ev->type = MKGUI_EVENT_TREELIST_COLLAPSE;
				ev->id = win->focus_id;
				ev->value = (int32_t)node + 1;
				return 1;
			}
		}
		return 0;
	} break;

	case MKGUI_KEY_RETURN: {
		if(cur >= 0) {
			ev->type = MKGUI_EVENT_TREELIST_DBLCLICK;
			ev->id = win->focus_id;
			ev->value = (int32_t)tl->visible[cur] + 1;
			return 1;
		}
		return 0;
	} break;

	default: {
	} break;
	}

	int32_t target = cur;
	switch(ks) {
	case MKGUI_KEY_UP: {
		target = cur < 0 ? 0 : cur - 1;
	} break;

	case MKGUI_KEY_DOWN: {
		target = cur < 0 ? 0 : cur + 1;
	} break;

	case MKGUI_KEY_PAGE_UP: {
		target = (cur < 0 ? 0 : cur) - page;
	} break;

	case MKGUI_KEY_PAGE_DOWN: {
		target = (cur < 0 ? 0 : cur) + page;
	} break;

	case MKGUI_KEY_HOME: {
		target = 0;
	} break;

	case MKGUI_KEY_END: {
		target = (int32_t)tl->visible_count - 1;
	} break;

	default: {
		return 0;
	} break;
	}

	if(target < 0) {
		target = 0;
	}

	if(target >= (int32_t)tl->visible_count) {
		target = (int32_t)tl->visible_count - 1;
	}
	tl->selected = (int32_t)tl->visible[target] + 1;

	int32_t row_top = target * win->row_height;
	if(row_top < tl->scroll_y) {
		tl->scroll_y = row_top;
	}

	if(row_top + win->row_height > tl->scroll_y + content_h) {
		tl->scroll_y = row_top + win->row_height - content_h;
	}

	if(tl->scroll_y < 0) {
		tl->scroll_y = 0;
	}
	dirty_all(win);
	ev->type = MKGUI_EVENT_TREELIST_SELECT;
	ev->id = win->focus_id;
	ev->value = tl->selected;
	return 1;
}

// [=]===^=[ treelist_free ]======================================[=]
static void treelist_free(struct mkgui_treelist_data *tl) {
	for(uint32_t i = 0; i < tl->node_count; ++i) {
		free(tl->nodes[i].cells);
	}
	free(tl->nodes);
	free(tl->visible);
	tl->nodes = NULL;
	tl->visible = NULL;
	tl->node_count = 0;
	tl->node_cap = 0;
	tl->visible_count = 0;
	tl->visible_cap = 0;
}

// [=]===^=[ mkgui_treelist_setup ]===============================[=]
MKGUI_API void mkgui_treelist_setup(struct mkgui_window *win, uint32_t id, uint32_t col_count, const struct mkgui_column *columns) {
	MKGUI_CHECK(win);
	MKGUI_CHECK(columns);
	struct mkgui_treelist_data *tl = find_treelist_data(win, id);
	if(!tl) {
		return;
	}

	treelist_free(tl);
	tl->col_count = col_count > MKGUI_MAX_COLS ? MKGUI_MAX_COLS : col_count;
	memcpy(tl->columns, columns, tl->col_count * sizeof(struct mkgui_column));
	for(uint32_t c = 0; c < tl->col_count; ++c) {
		tl->columns[c].width = sc(win, tl->columns[c].width);
		tl->col_order[c] = c;
	}
	tl->root_first = MKGUI_TL_NONE;
	tl->root_last = MKGUI_TL_NONE;
	tl->selected = -1;
	tl->multi_sel_count = 0;
	tl->scroll_x = 0;
	tl->scroll_y = 0;
	tl->sort_col = -1;
	tl->sort_dir = 1;
	tl->header_height = win->row_height;
	tl->dirty_visible = 1;
	dirty_all(win);
}

// [=]===^=[ mkgui_treelist_add ]=================================[=]
MKGUI_API uint32_t mkgui_treelist_add(struct mkgui_window *win, uint32_t id, uint32_t parent_node, const char *const *cells, void *row_userdata) {
	MKGUI_CHECK_VAL(win, 0);
	struct mkgui_treelist_data *tl = find_treelist_data(win, id);
	if(!tl) {
		return 0;
	}

	if(tl->node_count >= tl->node_cap) {
		uint32_t nc = tl->node_cap ? tl->node_cap * 2 : 256;
		struct mkgui_treelist_node *nn = (struct mkgui_treelist_node *)realloc(tl->nodes, nc * sizeof(struct mkgui_treelist_node));
		if(!nn) {
			return 0;
		}
		tl->nodes = nn;
		tl->node_cap = nc;
	}

	uint32_t idxn = tl->node_count;
	struct mkgui_treelist_node *n = &tl->nodes[idxn];
	memset(n, 0, sizeof(*n));
	n->first_child = MKGUI_TL_NONE;
	n->last_child = MKGUI_TL_NONE;
	n->next_sibling = MKGUI_TL_NONE;
	n->userdata = row_userdata;

	uint32_t parent_idx = MKGUI_TL_NONE;
	if(parent_node >= 1 && parent_node <= tl->node_count) {
		parent_idx = parent_node - 1;
	}
	n->parent = parent_idx;
	n->depth = (parent_idx != MKGUI_TL_NONE) ? tl->nodes[parent_idx].depth + 1 : 0;

	uint32_t total = 0;
	uint32_t lens[MKGUI_MAX_COLS];
	for(uint32_t c = 0; c < tl->col_count; ++c) {
		char *s = (cells && cells[c]) ? (char *)cells[c] : "";
		size_t sl = strlen(s);
		if(sl >= MKGUI_MAX_TEXT) {
			sl = MKGUI_MAX_TEXT - 1;
		}
		lens[c] = (uint32_t)sl;
		total += (uint32_t)sl + 1;
	}
	n->cells = (char *)malloc(total ? total : 1);
	if(n->cells) {
		uint32_t off = 0;
		for(uint32_t c = 0; c < tl->col_count; ++c) {
			char *s = (cells && cells[c]) ? (char *)cells[c] : "";
			n->cell_off[c] = (uint16_t)off;
			memcpy(n->cells + off, s, lens[c]);
			n->cells[off + lens[c]] = '\0';
			off += lens[c] + 1;
		}
	}

	tl->node_count++;

	if(parent_idx != MKGUI_TL_NONE) {
		struct mkgui_treelist_node *p = &tl->nodes[parent_idx];
		if(p->last_child != MKGUI_TL_NONE) {
			tl->nodes[p->last_child].next_sibling = idxn;
		} else {
			p->first_child = idxn;
		}
		p->last_child = idxn;
		p->has_children = 1;
	} else {
		if(tl->root_last != MKGUI_TL_NONE) {
			tl->nodes[tl->root_last].next_sibling = idxn;
		} else {
			tl->root_first = idxn;
		}
		tl->root_last = idxn;
	}

	tl->dirty_visible = 1;
	dirty_all(win);
	return idxn + 1;
}

// [=]===^=[ mkgui_treelist_clear ]===============================[=]
MKGUI_API void mkgui_treelist_clear(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK(win);
	struct mkgui_treelist_data *tl = find_treelist_data(win, id);
	if(!tl) {
		return;
	}
	treelist_free(tl);
	tl->root_first = MKGUI_TL_NONE;
	tl->root_last = MKGUI_TL_NONE;
	tl->selected = -1;
	tl->multi_sel_count = 0;
	tl->scroll_x = 0;
	tl->scroll_y = 0;
	tl->dirty_visible = 1;
	dirty_all(win);
}

// [=]===^=[ mkgui_treelist_expand ]==============================[=]
MKGUI_API void mkgui_treelist_expand(struct mkgui_window *win, uint32_t id, uint32_t node) {
	MKGUI_CHECK(win);
	struct mkgui_treelist_data *tl = find_treelist_data(win, id);
	if(!tl || node < 1 || node > tl->node_count) {
		return;
	}
	tl->nodes[node - 1].expanded = 1;
	tl->dirty_visible = 1;
	dirty_all(win);
}

// [=]===^=[ mkgui_treelist_collapse ]============================[=]
MKGUI_API void mkgui_treelist_collapse(struct mkgui_window *win, uint32_t id, uint32_t node) {
	MKGUI_CHECK(win);
	struct mkgui_treelist_data *tl = find_treelist_data(win, id);
	if(!tl || node < 1 || node > tl->node_count) {
		return;
	}
	tl->nodes[node - 1].expanded = 0;
	tl->dirty_visible = 1;
	dirty_all(win);
}

// [=]===^=[ mkgui_treelist_expand_all ]==========================[=]
MKGUI_API void mkgui_treelist_expand_all(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK(win);
	struct mkgui_treelist_data *tl = find_treelist_data(win, id);
	if(!tl) {
		return;
	}
	for(uint32_t i = 0; i < tl->node_count; ++i) {
		if(tl->nodes[i].has_children) {
			tl->nodes[i].expanded = 1;
		}
	}
	tl->dirty_visible = 1;
	dirty_all(win);
}

// [=]===^=[ mkgui_treelist_collapse_all ]========================[=]
MKGUI_API void mkgui_treelist_collapse_all(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK(win);
	struct mkgui_treelist_data *tl = find_treelist_data(win, id);
	if(!tl) {
		return;
	}
	for(uint32_t i = 0; i < tl->node_count; ++i) {
		tl->nodes[i].expanded = 0;
	}
	tl->dirty_visible = 1;
	dirty_all(win);
}

// [=]===^=[ mkgui_treelist_is_expanded ]=========================[=]
MKGUI_API uint32_t mkgui_treelist_is_expanded(struct mkgui_window *win, uint32_t id, uint32_t node) {
	MKGUI_CHECK_VAL(win, 0);
	struct mkgui_treelist_data *tl = find_treelist_data(win, id);
	if(!tl || node < 1 || node > tl->node_count) {
		return 0;
	}
	return tl->nodes[node - 1].expanded;
}

// [=]===^=[ mkgui_treelist_get_selected ]========================[=]
MKGUI_API int32_t mkgui_treelist_get_selected(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, -1);
	struct mkgui_treelist_data *tl = find_treelist_data(win, id);
	return tl ? tl->selected : -1;
}

// [=]===^=[ mkgui_treelist_set_selected ]========================[=]
MKGUI_API void mkgui_treelist_set_selected(struct mkgui_window *win, uint32_t id, int32_t node) {
	MKGUI_CHECK(win);
	struct mkgui_treelist_data *tl = find_treelist_data(win, id);
	if(!tl) {
		return;
	}
	tl->selected = node;
	tl->multi_sel_count = 0;
	if(node >= 1 && node <= (int32_t)tl->node_count) {
		tl->multi_sel[0] = node;
		tl->multi_sel_count = 1;
	}
	dirty_all(win);
}

// [=]===^=[ mkgui_treelist_is_selected ]=========================[=]
MKGUI_API uint32_t mkgui_treelist_is_selected(struct mkgui_window *win, uint32_t id, uint32_t node) {
	MKGUI_CHECK_VAL(win, 0);
	struct mkgui_treelist_data *tl = find_treelist_data(win, id);
	if(!tl) {
		return 0;
	}
	return (tl_multi_sel_find(tl, (int32_t)node) >= 0) ? 1 : 0;
}

// [=]===^=[ mkgui_treelist_clear_selection ]=====================[=]
MKGUI_API void mkgui_treelist_clear_selection(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK(win);
	struct mkgui_treelist_data *tl = find_treelist_data(win, id);
	if(tl) {
		tl->multi_sel_count = 0;
		tl->selected = -1;
		dirty_all(win);
	}
}

// [=]===^=[ mkgui_treelist_get_multi_sel ]=======================[=]
MKGUI_API uint32_t mkgui_treelist_get_multi_sel(struct mkgui_window *win, uint32_t id, int32_t **out) {
	MKGUI_CHECK_VAL(win, 0);
	struct mkgui_treelist_data *tl = find_treelist_data(win, id);
	if(!tl) {
		if(out) {
			*out = NULL;
		}
		return 0;
	}

	if(out) {
		*out = tl->multi_sel;
	}
	return tl->multi_sel_count;
}

// [=]===^=[ mkgui_treelist_get_userdata ]========================[=]
MKGUI_API void *mkgui_treelist_get_userdata(struct mkgui_window *win, uint32_t id, uint32_t node) {
	MKGUI_CHECK_VAL(win, NULL);
	struct mkgui_treelist_data *tl = find_treelist_data(win, id);
	if(!tl || node < 1 || node > tl->node_count) {
		return NULL;
	}
	return tl->nodes[node - 1].userdata;
}

// [=]===^=[ mkgui_treelist_get_parent ]==========================[=]
MKGUI_API uint32_t mkgui_treelist_get_parent(struct mkgui_window *win, uint32_t id, uint32_t node) {
	MKGUI_CHECK_VAL(win, 0);
	struct mkgui_treelist_data *tl = find_treelist_data(win, id);
	if(!tl || node < 1 || node > tl->node_count) {
		return 0;
	}
	uint32_t parent = tl->nodes[node - 1].parent;
	return parent == MKGUI_TL_NONE ? 0 : parent + 1;
}

// [=]===^=[ mkgui_treelist_node_count ]==========================[=]
MKGUI_API uint32_t mkgui_treelist_node_count(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, 0);
	struct mkgui_treelist_data *tl = find_treelist_data(win, id);
	return tl ? tl->node_count : 0;
}

// [=]===^=[ mkgui_treelist_set_col_width ]========================[=]
MKGUI_API void mkgui_treelist_set_col_width(struct mkgui_window *win, uint32_t id, uint32_t col, int32_t width) {
	MKGUI_CHECK(win);
	struct mkgui_treelist_data *tl = find_treelist_data(win, id);
	if(!tl || col >= tl->col_count) {
		return;
	}
	int32_t min_col_w = sc(win, 20);
	if(width < min_col_w) {
		width = min_col_w;
	}
	tl->columns[col].width = sc(win, width);
	dirty_all(win);
}

// [=]===^=[ mkgui_treelist_set_cell_type ]========================[=]
MKGUI_API void mkgui_treelist_set_cell_type(struct mkgui_window *win, uint32_t id, uint32_t col, uint32_t cell_type) {
	MKGUI_CHECK(win);
	struct mkgui_treelist_data *tl = find_treelist_data(win, id);
	if(!tl || col >= tl->col_count) {
		return;
	}
	tl->columns[col].cell_type = cell_type;
	dirty_all(win);
}

// [=]===^=[ mkgui_treelist_get_sort ]============================[=]
MKGUI_API void mkgui_treelist_get_sort(struct mkgui_window *win, uint32_t id, int32_t *col, int32_t *dir) {
	MKGUI_CHECK(win);
	struct mkgui_treelist_data *tl = find_treelist_data(win, id);
	if(!tl) {
		if(col) { *col = -1; }
		if(dir) { *dir = 0; }
		return;
	}

	if(col) { *col = tl->sort_col; }
	if(dir) { *dir = tl->sort_dir; }
}

// [=]===^=[ mkgui_treelist_set_sort ]============================[=]
MKGUI_API void mkgui_treelist_set_sort(struct mkgui_window *win, uint32_t id, int32_t col, int32_t dir) {
	MKGUI_CHECK(win);
	struct mkgui_treelist_data *tl = find_treelist_data(win, id);
	if(!tl) {
		return;
	}
	tl->sort_col = col;
	tl->sort_dir = dir;
	treelist_do_sort(tl);
	dirty_all(win);
}

// [=]===^=[ mkgui_treelist_scroll_to ]===========================[=]
MKGUI_API void mkgui_treelist_scroll_to(struct mkgui_window *win, uint32_t id, uint32_t node) {
	MKGUI_CHECK(win);
	struct mkgui_treelist_data *tl = find_treelist_data(win, id);
	if(!tl || node < 1 || node > tl->node_count) {
		return;
	}
	int32_t idx = find_widget_idx(win, id);
	if(idx < 0) {
		return;
	}

	// Ensure every ancestor is expanded so the node is actually reachable.
	uint32_t p = tl->nodes[node - 1].parent;
	while(p != MKGUI_TL_NONE) {
		tl->nodes[p].expanded = 1;
		p = tl->nodes[p].parent;
	}
	tl->dirty_visible = 1;
	treelist_rebuild_visible(tl);

	int32_t row = treelist_visible_row_of(tl, (int32_t)node);
	if(row < 0) {
		return;
	}
	int32_t hh = tl->header_height > 0 ? tl->header_height : win->row_height;
	int32_t content_h = win->rects[idx].h - hh - 2;
	int32_t row_y = row * win->row_height;
	if(row_y < tl->scroll_y) {
		tl->scroll_y = row_y;
	}

	if(row_y + win->row_height > tl->scroll_y + content_h) {
		tl->scroll_y = row_y + win->row_height - content_h;
	}

	if(tl->scroll_y < 0) {
		tl->scroll_y = 0;
	}
	dirty_all(win);
}
