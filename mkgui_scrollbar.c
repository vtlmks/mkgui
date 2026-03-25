// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ find_scrollbar_data ]================================[=]
static struct mkgui_scrollbar_data *find_scrollbar_data(struct mkgui_ctx *ctx, uint32_t id) {
	for(uint32_t i = 0; i < ctx->scrollbar_count; ++i) {
		if(ctx->scrollbars[i].id == id) {
			return &ctx->scrollbars[i];
		}
	}
	return NULL;
}

// [=]===^=[ mkgui_scrollbar_setup ]==============================[=]
MKGUI_API void mkgui_scrollbar_setup(struct mkgui_ctx *ctx, uint32_t id, int32_t max_value, int32_t page_size) {
	MKGUI_CHECK(ctx);
	struct mkgui_scrollbar_data *sb = find_scrollbar_data(ctx, id);
	if(!sb) {
		MKGUI_AUX_GROW(ctx->scrollbars, ctx->scrollbar_count, ctx->scrollbar_cap, struct mkgui_scrollbar_data);
		if(ctx->scrollbar_count >= ctx->scrollbar_cap) {
			return;
		}
		sb = &ctx->scrollbars[ctx->scrollbar_count++];
		sb->id = id;
	}
	sb->value = 0;
	sb->max_value = max_value;
	sb->page_size = page_size;
}

// [=]===^=[ mkgui_scrollbar_set ]================================[=]
MKGUI_API void mkgui_scrollbar_set(struct mkgui_ctx *ctx, uint32_t id, int32_t value) {
	MKGUI_CHECK(ctx);
	struct mkgui_scrollbar_data *sb = find_scrollbar_data(ctx, id);
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
	dirty_all(ctx);
}

// [=]===^=[ mkgui_scrollbar_get ]================================[=]
MKGUI_API int32_t mkgui_scrollbar_get(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_scrollbar_data *sb = find_scrollbar_data(ctx, id);
	if(!sb) {
		return 0;
	}
	return sb->value;
}

// [=]===^=[ scrollbar_thumb_rect ]===============================[=]
static void scrollbar_thumb_rect(struct mkgui_ctx *ctx, uint32_t idx, struct mkgui_scrollbar_data *sb, int32_t *out_ty, int32_t *out_th) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;
	uint32_t horizontal = (w->flags & MKGUI_VERTICAL) ? 0 : 1;

	int32_t track = horizontal ? rw : rh;
	if(track < 1) {
		track = 1;
	}

	int32_t total = sb->max_value;
	if(total < 1) {
		total = 1;
	}
	int32_t thumb = track * sb->page_size / total;
	if(thumb < 20) {
		thumb = 20;
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
static void render_scrollbar(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	struct mkgui_scrollbar_data *sb = find_scrollbar_data(ctx, w->id);
	if(!sb) {
		return;
	}

	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;
	uint32_t horizontal = (w->flags & MKGUI_VERTICAL) ? 0 : 1;

	draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx, ry, rw, rh, ctx->theme.scrollbar_bg);

	if(sb->max_value <= sb->page_size) {
		return;
	}

	int32_t thumb_pos, thumb_len;
	scrollbar_thumb_rect(ctx, idx, sb, &thumb_pos, &thumb_len);

	uint32_t thumb_color = (ctx->drag_scrollbar_id == w->id) ? ctx->theme.widget_hover : ctx->theme.scrollbar_thumb;
	if(ctx->hover_id == w->id && ctx->drag_scrollbar_id != w->id) {
		thumb_color = ctx->theme.scrollbar_thumb_hover;
	}

	int32_t r = ctx->theme.corner_radius;
	if(horizontal) {
		draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, thumb_pos, ry + 2, thumb_len, rh - 4, thumb_color, r);
	} else {
		draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx + 2, thumb_pos, rw - 4, thumb_len, thumb_color, r);
	}
}

// [=]===^=[ scrollbar_hit_thumb ]================================[=]
static uint32_t scrollbar_hit_thumb(struct mkgui_ctx *ctx, uint32_t idx, int32_t mx, int32_t my) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	struct mkgui_scrollbar_data *sb = find_scrollbar_data(ctx, w->id);
	if(!sb || sb->max_value <= sb->page_size) {
		return 0;
	}

	int32_t thumb_pos, thumb_len;
	scrollbar_thumb_rect(ctx, idx, sb, &thumb_pos, &thumb_len);

	uint32_t horizontal = (w->flags & MKGUI_VERTICAL) ? 0 : 1;
	int32_t coord = horizontal ? mx : my;
	return (coord >= thumb_pos && coord < thumb_pos + thumb_len) ? 1 : 0;
}

// [=]===^=[ scrollbar_thumb_drag_offset ]========================[=]
static int32_t scrollbar_thumb_drag_offset(struct mkgui_ctx *ctx, uint32_t idx, int32_t mx, int32_t my) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	struct mkgui_scrollbar_data *sb = find_scrollbar_data(ctx, w->id);
	if(!sb) {
		return 0;
	}

	int32_t thumb_pos, thumb_len;
	scrollbar_thumb_rect(ctx, idx, sb, &thumb_pos, &thumb_len);

	uint32_t horizontal = (w->flags & MKGUI_VERTICAL) ? 0 : 1;
	int32_t coord = horizontal ? mx : my;
	return coord - thumb_pos;
}

// [=]===^=[ scrollbar_drag_to ]==================================[=]
static void scrollbar_drag_to(struct mkgui_ctx *ctx, uint32_t id, int32_t mx, int32_t my) {
	int32_t idx = find_widget_idx(ctx, id);
	if(idx < 0) {
		return;
	}
	struct mkgui_widget *w = &ctx->widgets[idx];
	struct mkgui_scrollbar_data *sb = find_scrollbar_data(ctx, w->id);
	if(!sb) {
		return;
	}

	uint32_t horizontal = (w->flags & MKGUI_VERTICAL) ? 0 : 1;
	int32_t track = horizontal ? ctx->rects[idx].w : ctx->rects[idx].h;
	int32_t origin = horizontal ? ctx->rects[idx].x : ctx->rects[idx].y;
	int32_t coord = horizontal ? mx : my;

	int32_t total = sb->max_value;
	if(total < 1) {
		total = 1;
	}
	int32_t thumb = track * sb->page_size / total;
	if(thumb < 20) {
		thumb = 20;
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

	float frac = (float)(coord - origin - ctx->drag_scrollbar_offset) / (float)usable;
	if(frac < 0.0f) {
		frac = 0.0f;
	}
	if(frac > 1.0f) {
		frac = 1.0f;
	}

	sb->value = (int32_t)(frac * (float)max_scroll);
	dirty_all(ctx);
}

// [=]===^=[ scrollbar_scroll ]=================================[=]
static void scrollbar_scroll(struct mkgui_ctx *ctx, uint32_t id, int32_t direction) {
	struct mkgui_scrollbar_data *sb = find_scrollbar_data(ctx, id);
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
	dirty_all(ctx);
}
