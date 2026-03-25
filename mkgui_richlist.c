// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define MKGUI_RICHLIST_THUMB_PAD   4
#define MKGUI_RICHLIST_TEXT_PAD    6
#define MKGUI_RICHLIST_RIGHT_PAD   8
#define MKGUI_RICHLIST_DEFAULT_ROW_HEIGHT 56

// [=]===^=[ richlist_clamp_scroll ]===============================[=]
static void richlist_clamp_scroll(struct mkgui_richlist_data *rl, int32_t content_h) {
	int32_t total = (int32_t)rl->row_count * rl->row_height;
	int32_t max_scroll = total - content_h;
	if(max_scroll < 0) {
		max_scroll = 0;
	}
	if(rl->scroll_y < 0) {
		rl->scroll_y = 0;
	}
	if(rl->scroll_y > max_scroll) {
		rl->scroll_y = max_scroll;
	}
}

// [=]===^=[ richlist_scrollbar_hit ]==============================[=]
static uint32_t richlist_scrollbar_hit(struct mkgui_ctx *ctx, uint32_t idx, int32_t mx, int32_t my) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	struct mkgui_richlist_data *rl = find_richlist_data(ctx, w->id);
	if(!rl || rl->row_count == 0) {
		return 0;
	}

	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	int32_t sb_x = rx + rw - MKGUI_SCROLLBAR_W - 1;
	int32_t sb_y = ry + 1;
	int32_t sb_h = rh - 2;

	if(mx < sb_x || mx >= sb_x + MKGUI_SCROLLBAR_W || my < sb_y || my >= sb_y + sb_h) {
		return 0;
	}

	int32_t total_h = (int32_t)rl->row_count * rl->row_height;
	if(total_h <= sb_h) {
		return 0;
	}

	int32_t thumb_h = sb_h * sb_h / total_h;
	if(thumb_h < 20) {
		thumb_h = 20;
	}
	int32_t thumb_y = sb_y + (int32_t)((int64_t)rl->scroll_y * (sb_h - thumb_h) / (total_h - sb_h));

	if(my >= thumb_y && my < thumb_y + thumb_h) {
		return 1;
	}
	return (my < thumb_y) ? 2 : 3;
}

// [=]===^=[ richlist_thumb_offset ]===============================[=]
static int32_t richlist_thumb_offset(struct mkgui_ctx *ctx, uint32_t idx, int32_t my) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	struct mkgui_richlist_data *rl = find_richlist_data(ctx, w->id);
	if(!rl) {
		return 0;
	}

	int32_t ry = ctx->rects[idx].y;
	int32_t rh = ctx->rects[idx].h;
	int32_t sb_y = ry + 1;
	int32_t sb_h = rh - 2;

	int32_t total_h = (int32_t)rl->row_count * rl->row_height;
	int32_t thumb_h = sb_h * sb_h / total_h;
	if(thumb_h < 20) {
		thumb_h = 20;
	}
	int32_t thumb_y = sb_y + (int32_t)((int64_t)rl->scroll_y * (sb_h - thumb_h) / (total_h - sb_h));
	return my - thumb_y;
}

// [=]===^=[ richlist_scroll_to_y ]================================[=]
static void richlist_scroll_to_y(struct mkgui_ctx *ctx, uint32_t id, int32_t my) {
	int32_t idx = find_widget_idx(ctx, id);
	if(idx < 0) {
		return;
	}
	struct mkgui_richlist_data *rl = find_richlist_data(ctx, id);
	if(!rl) {
		return;
	}

	int32_t ry = ctx->rects[idx].y;
	int32_t rh = ctx->rects[idx].h;
	int32_t sb_y = ry + 1;
	int32_t sb_h = rh - 2;

	int32_t total_h = (int32_t)rl->row_count * rl->row_height;
	if(total_h <= sb_h) {
		return;
	}
	int32_t thumb_h = sb_h * sb_h / total_h;
	if(thumb_h < 20) {
		thumb_h = 20;
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

	rl->scroll_y = (int32_t)(frac * (float)(total_h - sb_h));
	dirty_all(ctx);
}

// [=]===^=[ richlist_row_hit ]====================================[=]
static int32_t richlist_row_hit(struct mkgui_ctx *ctx, uint32_t idx, int32_t my) {
	int32_t ry = ctx->rects[idx].y;
	int32_t rh = ctx->rects[idx].h;

	struct mkgui_richlist_data *rl = find_richlist_data(ctx, ctx->widgets[idx].id);
	if(!rl) {
		return -1;
	}

	int32_t content_y = ry + 1;
	int32_t content_bottom = ry + rh - 1;

	if(my < content_y || my >= content_bottom) {
		return -1;
	}

	int32_t sb_x = ctx->rects[idx].x + ctx->rects[idx].w - MKGUI_SCROLLBAR_W - 1;
	if(ctx->mouse_x >= sb_x) {
		return -1;
	}

	int32_t row = (my - content_y + rl->scroll_y) / rl->row_height;
	if(row < 0 || row >= (int32_t)rl->row_count) {
		return -1;
	}
	return row;
}

// [=]===^=[ render_richlist ]=====================================[=]
static void render_richlist(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	struct mkgui_richlist_data *rl = find_richlist_data(ctx, w->id);
	if(!rl) {
		return;
	}

	uint32_t focused = (ctx->focus_id == w->id);
	draw_patch(ctx, MKGUI_STYLE_SUNKEN, rx, ry, rw, rh, ctx->theme.input_bg, focused ? ctx->theme.splitter : ctx->theme.widget_border);

	int32_t content_w = rw - 2 - MKGUI_SCROLLBAR_W;
	int32_t content_y = ry + 1;
	int32_t content_h = rh - 2;

	richlist_clamp_scroll(rl, content_h);

	int32_t clip_left = rx + 1;
	int32_t clip_right = rx + 1 + content_w;
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

	int32_t visible_rows = content_h / rl->row_height + 2;
	int32_t first_row = rl->scroll_y / rl->row_height;

	int32_t thumb_size = rl->row_height - MKGUI_RICHLIST_THUMB_PAD * 2;
	if(thumb_size < 4) {
		thumb_size = 4;
	}

	struct mkgui_richlist_row row_data;
	uint32_t sep_color = blend_pixel(ctx->theme.input_bg, ctx->theme.widget_border, 60);

	for(int32_t r = 0; r < visible_rows && (first_row + r) < (int32_t)rl->row_count; ++r) {
		int32_t row_idx = first_row + r;
		int32_t row_y = content_y + r * rl->row_height - (rl->scroll_y % rl->row_height);

		if(row_y >= clip_bottom || row_y + rl->row_height <= clip_top) {
			continue;
		}

		int32_t draw_y = row_y < clip_top ? clip_top : row_y;
		int32_t draw_end = row_y + rl->row_height;
		if(draw_end > clip_bottom) {
			draw_end = clip_bottom;
		}
		int32_t draw_h = draw_end - draw_y;

		uint32_t is_selected = (row_idx == rl->selected_row);
		uint32_t row_bg = (row_idx & 1) ? ctx->theme.listview_alt : ctx->theme.input_bg;
		if(is_selected) {
			row_bg = ctx->theme.selection;
		}
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx + 1, draw_y, content_w, draw_h, row_bg);

		memset(&row_data, 0, sizeof(row_data));
		if(rl->row_cb) {
			rl->row_cb((uint32_t)row_idx, &row_data, rl->userdata);
		}

		int32_t text_x = clip_left + MKGUI_RICHLIST_TEXT_PAD;

		if(row_data.thumbnail && row_data.thumb_w > 0 && row_data.thumb_h > 0) {
			int32_t tx = clip_left + MKGUI_RICHLIST_THUMB_PAD;
			int32_t ty = row_y + MKGUI_RICHLIST_THUMB_PAD;
			int32_t tw = thumb_size;
			int32_t th = thumb_size;
			if(row_data.thumb_w != tw || row_data.thumb_h != th) {
				int32_t dw = tw < row_data.thumb_w ? tw : row_data.thumb_w;
				int32_t dh = th < row_data.thumb_h ? th : row_data.thumb_h;
				for(int32_t py = 0; py < dh; ++py) {
					int32_t dy = ty + py;
					if(dy < clip_top || dy >= clip_bottom) {
						continue;
					}
					for(int32_t px = 0; px < dw; ++px) {
						int32_t dx = tx + px;
						if(dx < clip_left || dx >= clip_right) {
							continue;
						}
						int32_t sx = px * row_data.thumb_w / tw;
						int32_t sy = py * row_data.thumb_h / th;
						uint32_t src = row_data.thumbnail[sy * row_data.thumb_w + sx];
						uint8_t sa = (uint8_t)(src >> 24);
						if(sa == 255) {
							ctx->pixels[dy * ctx->win_w + dx] = src;
						} else if(sa > 0) {
							ctx->pixels[dy * ctx->win_w + dx] = blend_pixel(ctx->pixels[dy * ctx->win_w + dx], src, sa);
						}
					}
				}
			} else {
				for(int32_t py = 0; py < th; ++py) {
					int32_t dy = ty + py;
					if(dy < clip_top || dy >= clip_bottom) {
						continue;
					}
					for(int32_t px = 0; px < tw; ++px) {
						int32_t dx = tx + px;
						if(dx < clip_left || dx >= clip_right) {
							continue;
						}
						uint32_t src = row_data.thumbnail[py * tw + px];
						uint8_t sa = (uint8_t)(src >> 24);
						if(sa == 255) {
							ctx->pixels[dy * ctx->win_w + dx] = src;
						} else if(sa > 0) {
							ctx->pixels[dy * ctx->win_w + dx] = blend_pixel(ctx->pixels[dy * ctx->win_w + dx], src, sa);
						}
					}
				}
			}
			text_x = clip_left + MKGUI_RICHLIST_THUMB_PAD + thumb_size + MKGUI_RICHLIST_TEXT_PAD;
		}

		uint32_t tc_title = is_selected ? ctx->theme.sel_text : ctx->theme.text;
		uint32_t tc_sub = is_selected ? ctx->theme.sel_text : ctx->theme.text_disabled;

		int32_t right_w = 0;
		if(row_data.right_text[0]) {
			right_w = text_width(ctx, row_data.right_text) + MKGUI_RICHLIST_RIGHT_PAD;
		}

		int32_t text_right = clip_right - right_w;

		if(row_data.title[0]) {
			int32_t ty;
			if(row_data.subtitle[0]) {
				ty = row_y + rl->row_height / 2 - ctx->font_height - 1;
			} else {
				ty = row_y + (rl->row_height - ctx->font_height) / 2;
			}
			push_text_clip(text_x, ty, row_data.title, tc_title, text_x, clip_top, text_right, clip_bottom);
		}

		if(row_data.subtitle[0]) {
			int32_t ty = row_y + rl->row_height / 2 + 2;
			push_text_clip(text_x, ty, row_data.subtitle, tc_sub, text_x, clip_top, text_right, clip_bottom);
		}

		if(row_data.right_text[0]) {
			int32_t rtw = text_width(ctx, row_data.right_text);
			int32_t rtx = clip_right - rtw - MKGUI_RICHLIST_RIGHT_PAD;
			int32_t rty = row_y + (rl->row_height - ctx->font_height) / 2;
			push_text_clip(rtx, rty, row_data.right_text, tc_sub, rtx, clip_top, clip_right, clip_bottom);
		}

		int32_t sep_y = row_y + rl->row_height - 1;
		if(sep_y >= clip_top && sep_y < clip_bottom) {
			draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, rx + 1, sep_y, content_w, sep_color);
		}
	}

	render_clip_x1 = saved_clip_x1;
	render_clip_y1 = saved_clip_y1;
	render_clip_x2 = saved_clip_x2;
	render_clip_y2 = saved_clip_y2;

	if(rl->row_count > 0) {
		int32_t sb_x = rx + rw - MKGUI_SCROLLBAR_W - 1;
		int32_t sb_y = content_y;
		int32_t sb_h = content_h;
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, sb_x, sb_y, MKGUI_SCROLLBAR_W, sb_h, ctx->theme.scrollbar_bg);

		int32_t total_h = (int32_t)rl->row_count * rl->row_height;
		if(total_h > sb_h) {
			int32_t thumb_h = sb_h * sb_h / total_h;
			if(thumb_h < 20) {
				thumb_h = 20;
			}
			int32_t thumb_y = sb_y + (int32_t)((int64_t)rl->scroll_y * (sb_h - thumb_h) / (total_h - sb_h));
			uint32_t thumb_color = (ctx->drag_scrollbar_id == w->id) ? ctx->theme.widget_hover : ctx->theme.scrollbar_thumb;
			draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, sb_x + 2, thumb_y, MKGUI_SCROLLBAR_W - 4, thumb_h, thumb_color, ctx->theme.corner_radius);
		}
	}
}

// [=]===^=[ handle_richlist_key ]=================================[=]
static uint32_t handle_richlist_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks) {
	struct mkgui_richlist_data *rl = find_richlist_data(ctx, ctx->focus_id);
	if(!rl || rl->row_count == 0) {
		return 0;
	}

	int32_t idx = find_widget_idx(ctx, ctx->focus_id);
	if(idx < 0) {
		return 0;
	}
	int32_t rh = ctx->rects[idx].h;
	int32_t content_h = rh - 2;
	int32_t page = content_h / rl->row_height;
	if(page < 1) {
		page = 1;
	}

	if(ks == MKGUI_KEY_UP) {
		if(rl->selected_row > 0) {
			--rl->selected_row;
		}
	} else if(ks == MKGUI_KEY_DOWN) {
		if(rl->selected_row < (int32_t)rl->row_count - 1) {
			++rl->selected_row;
		}
	} else if(ks == MKGUI_KEY_PAGE_UP) {
		rl->selected_row -= page;
		if(rl->selected_row < 0) {
			rl->selected_row = 0;
		}
	} else if(ks == MKGUI_KEY_PAGE_DOWN) {
		rl->selected_row += page;
		if(rl->selected_row >= (int32_t)rl->row_count) {
			rl->selected_row = (int32_t)rl->row_count - 1;
		}
	} else if(ks == MKGUI_KEY_HOME) {
		rl->selected_row = 0;
	} else if(ks == MKGUI_KEY_END) {
		rl->selected_row = (int32_t)rl->row_count - 1;
	} else {
		return 0;
	}

	int32_t row_y = rl->selected_row * rl->row_height;
	if(row_y < rl->scroll_y) {
		rl->scroll_y = row_y;
	}
	if(row_y + rl->row_height > rl->scroll_y + content_h) {
		rl->scroll_y = row_y + rl->row_height - content_h;
	}
	richlist_clamp_scroll(rl, content_h);
	dirty_all(ctx);
	ev->type = MKGUI_EVENT_RICHLIST_SELECT;
	ev->id = ctx->focus_id;
	ev->value = rl->selected_row;
	return 1;
}

// [=]===^=[ mkgui_richlist_setup ]================================[=]
MKGUI_API void mkgui_richlist_setup(struct mkgui_ctx *ctx, uint32_t id, uint32_t row_count, int32_t row_height, mkgui_richlist_cb cb, void *userdata) {
	MKGUI_CHECK(ctx);
	if(!cb) {
		return;
	}
	struct mkgui_richlist_data *rl = find_richlist_data(ctx, id);
	if(!rl) {
		MKGUI_AUX_GROW(ctx->richlists, ctx->richlist_count, ctx->richlist_cap, struct mkgui_richlist_data);
		if(ctx->richlist_count >= ctx->richlist_cap) {
			return;
		}
		rl = &ctx->richlists[ctx->richlist_count++];
		memset(rl, 0, sizeof(*rl));
		rl->widget_id = id;
		rl->selected_row = -1;
	}
	rl->row_count = row_count;
	rl->row_height = row_height > 0 ? row_height : MKGUI_RICHLIST_DEFAULT_ROW_HEIGHT;
	rl->row_cb = cb;
	rl->userdata = userdata;
	rl->scroll_y = 0;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_richlist_set_rows ]=============================[=]
MKGUI_API void mkgui_richlist_set_rows(struct mkgui_ctx *ctx, uint32_t id, uint32_t row_count) {
	MKGUI_CHECK(ctx);
	struct mkgui_richlist_data *rl = find_richlist_data(ctx, id);
	if(!rl) {
		return;
	}
	rl->row_count = row_count;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_richlist_get_selected ]==========================[=]
MKGUI_API int32_t mkgui_richlist_get_selected(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, -1);
	struct mkgui_richlist_data *rl = find_richlist_data(ctx, id);
	return rl ? rl->selected_row : -1;
}

// [=]===^=[ mkgui_richlist_set_selected ]==========================[=]
MKGUI_API void mkgui_richlist_set_selected(struct mkgui_ctx *ctx, uint32_t id, int32_t row) {
	MKGUI_CHECK(ctx);
	struct mkgui_richlist_data *rl = find_richlist_data(ctx, id);
	if(rl) {
		rl->selected_row = row;
		dirty_all(ctx);
	}
}

// [=]===^=[ mkgui_richlist_scroll_to ]=============================[=]
MKGUI_API void mkgui_richlist_scroll_to(struct mkgui_ctx *ctx, uint32_t id, int32_t row) {
	MKGUI_CHECK(ctx);
	struct mkgui_richlist_data *rl = find_richlist_data(ctx, id);
	if(!rl) {
		return;
	}
	int32_t idx = find_widget_idx(ctx, id);
	if(idx < 0) {
		return;
	}
	int32_t rh = ctx->rects[idx].h;
	int32_t content_h = rh - 2;
	int32_t row_y = row * rl->row_height;
	if(row_y < rl->scroll_y) {
		rl->scroll_y = row_y;
	}
	if(row_y + rl->row_height > rl->scroll_y + content_h) {
		rl->scroll_y = row_y + rl->row_height - content_h;
	}
	if(rl->scroll_y < 0) {
		rl->scroll_y = 0;
	}
	dirty_all(ctx);
}
