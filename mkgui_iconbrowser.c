// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT
#define IB_ICON_NAME 64
#define IB_WIN_W 850
#define IB_WIN_H 560
#define IB_GRID_ROW_H_PX 24
#define IB_GRID_COL_W_PX 250
#define IB_GRID_PAD_PX 4
enum {
	IB_TH_WINDOW = 100,
	IB_TH_VBOX,
	IB_TH_HBOX_TOP,
	IB_TH_STYLE,
	IB_TH_SEARCH,
	IB_TH_SPLIT,
	IB_TH_GROUPS,
	IB_TH_RIGHT,
	IB_TH_GRID,
	IB_TH_HSCROLL,
	IB_TH_STATUS,
};
struct ib_theme_state {
	uint32_t **thumb;
	uint32_t *filtered_idx;
	uint32_t total_count;
	uint32_t filtered_count;
	uint32_t active_cat;
	uint32_t active_style;
	uint32_t confirmed;
	uint32_t color;
	char result_name[IB_ICON_NAME];
	int32_t scroll_x;
	int32_t selected;
	int32_t last_click_item;
	uint32_t last_click_time;
	int32_t size;
	int32_t logical_size;
};

static struct ib_theme_state *ibt;

static struct mkgui_widget ibt_widgets[] = {
	MKGUI_W(MKGUI_WINDOW, IB_TH_WINDOW, "Icons", "", 0, IB_WIN_W, IB_WIN_H, 0, 0, 0),
	MKGUI_W(MKGUI_VBOX, IB_TH_VBOX, "", "", IB_TH_WINDOW, 0, 0, 0, 0, 0),
	MKGUI_W(MKGUI_HBOX, IB_TH_HBOX_TOP, "", "", IB_TH_VBOX, 0, 32, MKGUI_FIXED, 0, 0),
	MKGUI_W(MKGUI_DROPDOWN, IB_TH_STYLE, "", "", IB_TH_HBOX_TOP, 130, 28, MKGUI_FIXED, 0, 0),
	MKGUI_W(MKGUI_INPUT, IB_TH_SEARCH, "", MKICON_SEARCH, IB_TH_HBOX_TOP, 0, 0, 0, 0, 1),
	MKGUI_W(MKGUI_VSPLIT, IB_TH_SPLIT, "", "", IB_TH_VBOX, 0, 0, 0, 0, 1),
	MKGUI_W(MKGUI_LISTVIEW, IB_TH_GROUPS, "", "", IB_TH_SPLIT, 0, 0, MKGUI_REGION_LEFT, 0, 0),
	MKGUI_W(MKGUI_VBOX, IB_TH_RIGHT, "", "", IB_TH_SPLIT, 0, 0, MKGUI_REGION_RIGHT, 0, 0),
	MKGUI_W(MKGUI_CANVAS, IB_TH_GRID, "", "", IB_TH_RIGHT, 0, 0, 0, 0, 1),
	MKGUI_W(MKGUI_SCROLLBAR, IB_TH_HSCROLL, "", "", IB_TH_RIGHT, 0, 14, MKGUI_FIXED, 0, 0),
	MKGUI_W(MKGUI_LABEL, IB_TH_STATUS, "", "", IB_TH_VBOX, 0, 24, MKGUI_FIXED, 0, 0),
};

// [=]===^=[ ibt_matches ]========================================[=]
static uint32_t ibt_matches(const char *text, const char *search) {
	for(size_t start = 0; text[start]; ++start) {
		uint32_t match = 1;

		for(size_t i = 0; search[i]; ++i) {
			uint8_t left = (uint8_t)text[start + i];
			uint8_t right = (uint8_t)search[i];

			if(left >= 'A' && left <= 'Z') {
				left += 'a' - 'A';

			}

			if(right >= 'A' && right <= 'Z') {
				right += 'a' - 'A';

			}

			left = left == '-' || left == '_' ? ' ' : left;
			right = right == '-' || right == '_' ? ' ' : right;

			if(left != right || left == 0) {
				match = 0;
				break;

			}
		}

		if(match) {
			return 1;

		}
	}

	return search[0] == '\0';

}

// [=]===^=[ ibt_filter ]=========================================[=]
static void ibt_filter(const char *search) {
	static char *styles[] = {"", ":filled", ":light", ":color"};
	ibt->filtered_count = 0;

	for(uint32_t i = 0; i < ibt->total_count; ++i) {
		const char *name = mkguiicons_name(i);
		const char *style = strchr(name, ':');

		if(ibt->active_cat && mkguiicons_topic(i) != ibt->active_cat - 1) {
			continue;

		}

		if(ibt->active_style < 4 && ((ibt->active_style == 0 && style) || (ibt->active_style > 0 && (!style || strcmp(style, styles[ibt->active_style]) != 0)))) {
			continue;

		}

		if(search && search[0] && !ibt_matches(name, search) && !ibt_matches(mkguiicons_keywords(i), search)) {
			continue;

		}

		ibt->filtered_idx[ibt->filtered_count++] = i;

	}

	ibt->selected = -1;
	ibt->scroll_x = 0;
	ibt->last_click_item = -1;

}

// [=]===^=[ ibt_group_row ]======================================[=]
static void ibt_group_row(uint32_t row, uint32_t column, char *buffer, uint32_t size, void *userdata) {
	(void)column;
	(void)userdata;
	snprintf(buffer, size, "%s", row ? mkguiicons_topic_name(row - 1) : "All icons");

}

// [=]===^=[ ibt_free_thumbs ]=====================================[=]
static void ibt_free_thumbs(void) {
	for(uint32_t i = 0; i < ibt->total_count; ++i) {
		free(ibt->thumb[i]);
		ibt->thumb[i] = NULL;

	}
}

// [=]===^=[ ibt_grid_metrics ]=======================================[=]
static void ibt_grid_metrics(struct mkgui_window *win, int32_t ca_w, int32_t ca_h, int32_t *out_row_h, int32_t *out_col_w, int32_t *out_rows_per_col, int32_t *out_total_cols, int32_t *out_total_w) {
	int32_t row_h = sc(win, IB_GRID_ROW_H_PX);
	int32_t icon_row_h = sc(win, ibt->logical_size + 2 * IB_GRID_PAD_PX);

	if(row_h < icon_row_h) {
		row_h = icon_row_h;

	}

	int32_t col_w = sc(win, IB_GRID_COL_W_PX);
	int32_t rows_per_col = ca_h / row_h;

	if(rows_per_col < 1) {
		rows_per_col = 1;

	}

	int32_t total_cols = ((int32_t)ibt->filtered_count + rows_per_col - 1) / rows_per_col;
	int32_t total_w = total_cols * col_w;

	if(total_w < ca_w) {
		total_w = ca_w;

	}

	*out_row_h = row_h;
	*out_col_w = col_w;
	*out_rows_per_col = rows_per_col;
	*out_total_cols = total_cols;
	*out_total_w = total_w;

}

// [=]===^=[ ibt_canvas_cb ]=======================================[=]
static void ibt_canvas_cb(struct mkgui_window *win, uint32_t id, uint32_t *pixels, int32_t x, int32_t y, int32_t w, int32_t h, void *userdata) {
	(void)id;
	(void)pixels;
	(void)userdata;
	int32_t row_h, col_w, rows_per_col, total_cols, total_w;
	ibt_grid_metrics(win, w, h, &row_h, &col_w, &rows_per_col, &total_cols, &total_w);
	int32_t size = sc(win, ibt->logical_size);

	if(size != ibt->size || ibt->color != win->theme.text) {
		ibt_free_thumbs();
		ibt->size = size;
		ibt->color = win->theme.text;

	}

	int32_t pad = sc(win, IB_GRID_PAD_PX);
	int32_t first_col = ibt->scroll_x / col_w;
	int32_t last_col = first_col + w / col_w + 2;

	for(int32_t column = first_col; column < last_col && column < total_cols; ++column) {
		int32_t left = x + column * col_w - ibt->scroll_x;
		int32_t clip_left = left > x ? left : x;
		int32_t clip_right = left + col_w < x + w ? left + col_w : x + w;
		clip_left = clip_left < 0 ? 0 : clip_left;
		clip_right = clip_right > win->win_w ? win->win_w : clip_right;

		for(int32_t row = 0; row < rows_per_col; ++row) {
			uint32_t item = (uint32_t)(column * rows_per_col + row);

			if(item >= ibt->filtered_count) {
				break;

			}

			uint32_t index = ibt->filtered_idx[item];
			int32_t top = y + row * row_h;
			uint32_t selected = (int32_t)item == ibt->selected;

			if(selected) {
				draw_rect_fill(win->pixels, win->win_w, win->win_h, clip_left, top, clip_right - clip_left, row_h, win->theme.selection);

			}

			if(!ibt->thumb[index]) {
				ibt->thumb[index] = icon_library_pixels(mkguiicons_name(index), size, win->theme.text);

			}

			if(ibt->thumb[index]) {
				int32_t ix = left + pad;
				int32_t iy = top + (row_h - size) / 2;
				int32_t start = ix < clip_left ? clip_left - ix : 0;
				int32_t end = ix + size > clip_right ? clip_right - ix : size;

				for(int32_t py = 0; py < size; ++py) {
					int32_t dy = iy + py;

					if(dy >= y && dy < y + h && dy >= 0 && dy < win->win_h && start < end) {
						blend_icon_row(win->pixels + dy * win->win_w, ibt->thumb[index] + py * size, start, end, ix);

					}
				}
			}

			uint32_t color = selected ? win->theme.sel_text : win->theme.text;
			char name[IB_ICON_NAME];
			snprintf(name, sizeof(name), "%s", mkguiicons_name(index));
			push_text_clip(left + size + 2 * pad, top + (row_h - win->font_height) / 2, name, color, clip_left, y, clip_right, y + h);

		}
	}
}

// [=]===^=[ ibt_hit_test ]===========================================[=]
static int32_t ibt_hit_test(struct mkgui_window *dlg, int32_t mx, int32_t my, uint32_t grid_id) {
	int32_t gi = find_widget_idx(dlg, grid_id);

	if(gi < 0) {
		return -1;

	}

	int32_t rx = dlg->rects[gi].x;
	int32_t ry = dlg->rects[gi].y;
	int32_t rw = dlg->rects[gi].w;
	int32_t rh = dlg->rects[gi].h;

	if(mx < rx || mx >= rx + rw || my < ry || my >= ry + rh) {
		return -1;

	}

	int32_t row_h, col_w, rows_per_col, total_cols, total_w;
	ibt_grid_metrics(dlg, rw, rh, &row_h, &col_w, &rows_per_col, &total_cols, &total_w);
	(void)total_cols;
	(void)total_w;
	int32_t local_x = (mx - rx) + ibt->scroll_x;
	int32_t local_y = my - ry;
	int32_t c = local_x / col_w;
	int32_t r = local_y / row_h;

	if(c < 0 || r < 0 || r >= rows_per_col) {
		return -1;

	}

	int32_t item = c * rows_per_col + r;

	if(item < 0 || item >= (int32_t)ibt->filtered_count) {
		return -1;

	}

	return item;

}

// [=]===^=[ ibt_update_scroll_range ]================================[=]
static void ibt_update_scroll_range(struct mkgui_window *dlg, uint32_t grid_id, uint32_t scroll_id) {
	int32_t gi = find_widget_idx(dlg, grid_id);

	if(gi < 0) {
		return;

	}

	int32_t rw = dlg->rects[gi].w;
	int32_t rh = dlg->rects[gi].h;

	if(rw <= 0 || rh <= 0) {
		return;

	}

	struct mkgui_scrollbar_data *sb = find_scrollbar_data(dlg, scroll_id);

	if(!sb) {
		return;

	}

	int32_t row_h, col_w, rows_per_col, total_cols, total_w;
	ibt_grid_metrics(dlg, rw, rh, &row_h, &col_w, &rows_per_col, &total_cols, &total_w);
	sb->max_value = total_w;
	sb->page_size = rw;
	int32_t max_scroll = total_w > rw ? total_w - rw : 0;

	if(ibt->scroll_x > max_scroll) {
		ibt->scroll_x = max_scroll;

	}

	if(ibt->scroll_x < 0) {
		ibt->scroll_x = 0;

	}

	sb->value = ibt->scroll_x;

}

// [=]===^=[ ibt_confirm_selection ]==============================[=]
static void ibt_confirm_selection(uint32_t item) {
	if(item < ibt->filtered_count) {
		snprintf(ibt->result_name, sizeof(ibt->result_name), "%s", mkguiicons_name(ibt->filtered_idx[item]));
		ibt->confirmed = 1;

	}
}

// [=]===^=[ ibt_cleanup ]========================================[=]
static void ibt_cleanup(struct mkgui_window *win) {
	ibt_free_thumbs();
	free(ibt);
	ibt = NULL;
	dirty_all(win);

}

// [=]===^=[ mkgui_icon_browser ]=================================[=]
MKGUI_API uint32_t mkgui_icon_browser(struct mkgui_window *win, int32_t size, char *out_name, uint32_t name_size) {
	MKGUI_CHECK_VAL(win, 0);
	uint32_t count = mkguiicons_count();

	if(!out_name || name_size == 0 || count == 0) {
		return 0;

	}

	ibt = calloc(1, sizeof(*ibt) + (size_t)count * (sizeof(uint32_t *) + sizeof(uint32_t)));

	if(ibt) {
		ibt->thumb = (uint32_t **)(ibt + 1);
		ibt->filtered_idx = (uint32_t *)(ibt->thumb + count);
		ibt->total_count = count;
		ibt->logical_size = size > 0 ? size : 16;
		ibt->selected = -1;
		ibt->last_click_item = -1;
		ibt_filter(NULL);

		struct mkgui_window *dialog = mkgui_window_create(win->ctx, win, ibt_widgets, sizeof(ibt_widgets) / sizeof(ibt_widgets[0]), NULL, 0, 0);

		if(dialog) {
			struct mkgui_column columns[] = {{"Name groups", 160, MKGUI_CELL_TEXT}};
			const char *styles[] = {"Default", "Filled", "Light", "Colour", "All styles"};
			mkgui_dropdown_setup(dialog, IB_TH_STYLE, styles, 5);
			mkgui_listview_setup(dialog, IB_TH_GROUPS, mkguiicons_topic_count() + 1, 1, columns, ibt_group_row, NULL);
			mkgui_listview_set_selected(dialog, IB_TH_GROUPS, 0);
			mkgui_split_set_ratio(dialog, IB_TH_SPLIT, 0.22f);
			mkgui_canvas_set_callback(dialog, IB_TH_GRID, ibt_canvas_cb, NULL);
			mkgui_scrollbar_setup(dialog, IB_TH_HSCROLL, 1, 1);
			mkgui_set_focus(dialog, IB_TH_SEARCH);
			uint32_t running = 1;
			int32_t last_scroll = 0;
			struct mkgui_event event;

			while(running) {
				while(running && mkgui_window_poll(dialog, &event)) {
					if(event.type == MKGUI_EVENT_CLOSE || (event.type == MKGUI_EVENT_KEY && event.keysym == MKGUI_KEY_ESCAPE)) {
						running = 0;

					} else if(event.type == MKGUI_EVENT_KEY && event.keysym == MKGUI_KEY_RETURN && ibt->selected >= 0) {
						ibt_confirm_selection((uint32_t)ibt->selected);
						running = 0;

					} else if(event.type == MKGUI_EVENT_INPUT_CHANGED && event.id == IB_TH_SEARCH) {
						ibt_filter(mkgui_input_get(dialog, IB_TH_SEARCH));
						dirty_all(dialog);

					} else if(event.type == MKGUI_EVENT_DROPDOWN_CHANGED && event.id == IB_TH_STYLE) {
						ibt->active_style = (uint32_t)event.value;
						ibt_filter(mkgui_input_get(dialog, IB_TH_SEARCH));
						dirty_all(dialog);

					} else if(event.type == MKGUI_EVENT_LISTVIEW_SELECT && event.id == IB_TH_GROUPS) {
						int32_t selected = mkgui_listview_get_selected(dialog, IB_TH_GROUPS);
						ibt->active_cat = selected >= 0 ? (uint32_t)selected : 0;
						ibt_filter(mkgui_input_get(dialog, IB_TH_SEARCH));
						dirty_all(dialog);

					} else if(event.type == MKGUI_EVENT_SCROLL && event.id == IB_TH_GRID) {
						ibt->scroll_x += event.value;
						ibt->scroll_x = ibt->scroll_x > 0 ? ibt->scroll_x : 0;
						dirty_all(dialog);

					} else if(event.type == MKGUI_EVENT_CANVAS_PRESS && event.id == IB_TH_GRID && event.keysym == 1) {
						int32_t item = ibt_hit_test(dialog, event.value, event.col, IB_TH_GRID);

						if(item >= 0) {
							uint32_t now = (uint32_t)(mkgui_now_ns() / 1000000ull);

							if(ibt->last_click_item == item && now - ibt->last_click_time < 400) {
								ibt_confirm_selection((uint32_t)item);
								running = 0;

							} else {
								ibt->selected = item;
								ibt->last_click_item = item;
								ibt->last_click_time = now;
								dirty_all(dialog);

							}
						}
					}
				}

				int32_t scroll = mkgui_scrollbar_get(dialog, IB_TH_HSCROLL);

				if(scroll != last_scroll) {
					ibt->scroll_x = scroll;
					dirty_all(dialog);

				}

				ibt_update_scroll_range(dialog, IB_TH_GRID, IB_TH_HSCROLL);
				last_scroll = ibt->scroll_x;
				char status[128];
				snprintf(status, sizeof(status), "%u icons | Fluent %s", ibt->filtered_count, mkguiicons_release());

				if(strcmp(mkgui_label_get(dialog, IB_TH_STATUS), status) != 0) {
					mkgui_label_set(dialog, IB_TH_STATUS, status);

				}

				if(running) {
					mkgui_ctx_wait(dialog->ctx);

				}
			}

			mkgui_window_destroy(dialog);

		} else {
			fprintf(stderr, "mkgui: cannot create icon browser\n");

		}

		uint32_t confirmed = ibt->confirmed;

		if(confirmed) {
			snprintf(out_name, name_size, "%s", ibt->result_name);
			icon_resolve(win, out_name);

		}

		ibt_cleanup(win);
		return confirmed;

	} else {
		fprintf(stderr, "mkgui: cannot allocate icon browser\n");

	}

	return 0;

}
