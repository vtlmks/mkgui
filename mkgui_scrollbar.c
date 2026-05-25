// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ find_scrollbar_data ]================================[=]
static struct mkgui_scrollbar_data *find_scrollbar_data(struct mkgui_window *win, uint32_t id) {
	for(uint32_t i = 0; i < win->scrollbar_count; ++i) {
		if(win->scrollbars[i].id == id) {
			return &win->scrollbars[i];
		}
	}
	return NULL;
}

// [=]===^=[ mkgui_scrollbar_setup ]==============================[=]
MKGUI_API void mkgui_scrollbar_setup(struct mkgui_window *win, uint32_t id, int32_t max_value, int32_t page_size) {
	MKGUI_CHECK(win);
	struct mkgui_scrollbar_data *sb = find_scrollbar_data(win, id);
	if(!sb) {
		MKGUI_AUX_GROW(&win->arenas.scrollbars, win->scrollbar_count, win->scrollbar_cap, struct mkgui_scrollbar_data);
		if(win->scrollbar_count >= win->scrollbar_cap) {
			return;
		}
		sb = &win->scrollbars[win->scrollbar_count++];
		sb->id = id;
	}
	sb->value = 0;
	sb->max_value = max_value;
	sb->page_size = page_size;
}

// [=]===^=[ mkgui_scrollbar_set ]================================[=]
MKGUI_API void mkgui_scrollbar_set(struct mkgui_window *win, uint32_t id, int32_t value) {
	MKGUI_CHECK(win);
	struct mkgui_scrollbar_data *sb = find_scrollbar_data(win, id);
	if(!sb) {
		return;
	}
	int32_t max_scroll = sb->max_value - sb->page_size;
	if(max_scroll < 0) {
		max_scroll = 0;
	}

	if(value < 0) {
		value = 0;
	}

	if(value > max_scroll) {
		value = max_scroll;
	}
	sb->value = value;
	dirty_all(win);
}

// [=]===^=[ mkgui_scrollbar_get ]================================[=]
MKGUI_API int32_t mkgui_scrollbar_get(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, 0);
	struct mkgui_scrollbar_data *sb = find_scrollbar_data(win, id);
	if(!sb) {
		return 0;
	}
	return sb->value;
}

// [=]===^=[ scrollbar_thumb_rect ]===============================[=]
static void scrollbar_thumb_rect(struct mkgui_window *win, uint32_t idx, struct mkgui_scrollbar_data *sb, int32_t *out_ty, int32_t *out_th) {
	struct mkgui_widget *w = &win->widgets[idx];
	int32_t rx = win->rects[idx].x;
	int32_t ry = win->rects[idx].y;
	int32_t rw = win->rects[idx].w;
	int32_t rh = win->rects[idx].h;
	uint32_t horizontal = (w->flags & MKGUI_VERTICAL) ? 0 : 1;

	int32_t track = horizontal ? rw : rh;
	if(track < 1) {
		track = 1;
	}

	int32_t total = sb->max_value;
	if(total < 1) {
		total = 1;
	}
	int32_t min_thumb = sc(win, 20);
	int32_t thumb = track * sb->page_size / total;
	if(thumb < min_thumb) {
		thumb = min_thumb;
	}

	if(thumb > track) {
		thumb = track;
	}

	int32_t max_scroll = sb->max_value - sb->page_size;
	if(max_scroll < 1) {
		max_scroll = 1;
	}
	int32_t pos = (int32_t)((int64_t)sb->value * (track - thumb) / max_scroll);
	if(pos < 0) {
		pos = 0;
	}

	if(pos > track - thumb) {
		pos = track - thumb;
	}

	*out_ty = (horizontal ? rx : ry) + pos;
	*out_th = thumb;
}

// [=]===^=[ render_scrollbar ]===================================[=]
static void render_scrollbar(struct mkgui_window *win, uint32_t idx) {
	struct mkgui_widget *w = &win->widgets[idx];
	struct mkgui_scrollbar_data *sb = find_scrollbar_data(win, w->id);
	if(!sb) {
		return;
	}

	int32_t rx = win->rects[idx].x;
	int32_t ry = win->rects[idx].y;
	int32_t rw = win->rects[idx].w;
	int32_t rh = win->rects[idx].h;
	uint32_t horizontal = (w->flags & MKGUI_VERTICAL) ? 0 : 1;

	draw_rect_fill(win->pixels, win->win_w, win->win_h, rx, ry, rw, rh, win->theme.scrollbar_bg);

	if(sb->max_value <= sb->page_size) {
		return;
	}

	int32_t thumb_pos, thumb_len;
	scrollbar_thumb_rect(win, idx, sb, &thumb_pos, &thumb_len);

	uint32_t thumb_color = (win->drag_scrollbar_id == w->id) ? win->theme.widget_hover : win->theme.scrollbar_thumb;
	if(win->hover_id == w->id && win->drag_scrollbar_id != w->id) {
		thumb_color = win->theme.scrollbar_thumb_hover;
	}

	int32_t r = win->theme.corner_radius;
	int32_t inset = sc(win, 2);
	if(horizontal) {
		draw_rounded_rect_fill(win->pixels, win->win_w, win->win_h, thumb_pos, ry + inset, thumb_len, rh - inset * 2, thumb_color, r);
	} else {
		draw_rounded_rect_fill(win->pixels, win->win_w, win->win_h, rx + inset, thumb_pos, rw - inset * 2, thumb_len, thumb_color, r);
	}
}

// [=]===^=[ scrollbar_hit_thumb ]================================[=]
static uint32_t scrollbar_hit_thumb(struct mkgui_window *win, uint32_t idx, int32_t mx, int32_t my) {
	struct mkgui_widget *w = &win->widgets[idx];
	struct mkgui_scrollbar_data *sb = find_scrollbar_data(win, w->id);
	if(!sb || sb->max_value <= sb->page_size) {
		return 0;
	}

	int32_t thumb_pos, thumb_len;
	scrollbar_thumb_rect(win, idx, sb, &thumb_pos, &thumb_len);

	uint32_t horizontal = (w->flags & MKGUI_VERTICAL) ? 0 : 1;
	int32_t coord = horizontal ? mx : my;
	return (coord >= thumb_pos && coord < thumb_pos + thumb_len) ? 1 : 0;
}

// [=]===^=[ scrollbar_thumb_drag_offset ]========================[=]
static int32_t scrollbar_thumb_drag_offset(struct mkgui_window *win, uint32_t idx, int32_t mx, int32_t my) {
	struct mkgui_widget *w = &win->widgets[idx];
	struct mkgui_scrollbar_data *sb = find_scrollbar_data(win, w->id);
	if(!sb) {
		return 0;
	}

	int32_t thumb_pos, thumb_len;
	scrollbar_thumb_rect(win, idx, sb, &thumb_pos, &thumb_len);

	uint32_t horizontal = (w->flags & MKGUI_VERTICAL) ? 0 : 1;
	int32_t coord = horizontal ? mx : my;
	return coord - thumb_pos;
}

// [=]===^=[ scrollbar_drag_to ]==================================[=]
static void scrollbar_drag_to(struct mkgui_window *win, uint32_t id, int32_t mx, int32_t my) {
	int32_t idx = find_widget_idx(win, id);
	if(idx < 0) {
		return;
	}
	struct mkgui_widget *w = &win->widgets[idx];
	struct mkgui_scrollbar_data *sb = find_scrollbar_data(win, w->id);
	if(!sb) {
		return;
	}

	uint32_t horizontal = (w->flags & MKGUI_VERTICAL) ? 0 : 1;
	int32_t track = horizontal ? win->rects[idx].w : win->rects[idx].h;
	int32_t origin = horizontal ? win->rects[idx].x : win->rects[idx].y;
	int32_t coord = horizontal ? mx : my;

	int32_t total = sb->max_value;
	if(total < 1) {
		total = 1;
	}
	int32_t min_thumb = sc(win, 20);
	int32_t thumb = track * sb->page_size / total;
	if(thumb < min_thumb) {
		thumb = min_thumb;
	}

	if(thumb > track) {
		thumb = track;
	}

	int32_t usable = track - thumb;
	if(usable < 1) {
		return;
	}

	int32_t max_scroll = sb->max_value - sb->page_size;
	if(max_scroll < 0) {
		max_scroll = 0;
	}

	float frac = (float)(coord - origin - win->drag_scrollbar_offset) / (float)usable;
	if(frac < 0.0f) {
		frac = 0.0f;
	}

	if(frac > 1.0f) {
		frac = 1.0f;
	}

	sb->value = (int32_t)(frac * (float)max_scroll);
	dirty_all(win);
}

// [=]===^=[ scrollbar_scroll ]=================================[=]
static void scrollbar_scroll(struct mkgui_window *win, uint32_t id, int32_t direction) {
	struct mkgui_scrollbar_data *sb = find_scrollbar_data(win, id);
	if(!sb) {
		return;
	}
	int32_t step = sb->page_size / 4;
	if(step < 1) {
		step = 1;
	}
	int32_t max_scroll = sb->max_value - sb->page_size;
	if(max_scroll < 0) {
		max_scroll = 0;
	}
	sb->value += direction * step;
	if(sb->value < 0) {
		sb->value = 0;
	}

	if(sb->value > max_scroll) {
		sb->value = max_scroll;
	}
	dirty_all(win);
}
