// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define MKGUI_DROPDOWN_MAX_VISIBLE 12

// [=]===^=[ render_dropdown ]===================================[=]
static void render_dropdown(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	uint32_t disabled = (w->flags & MKGUI_DISABLED);
	uint32_t bg = ctx->theme.widget_bg;
	if(!disabled && ctx->hover_id == w->id) {
		bg = ctx->theme.widget_hover;
	}
	uint32_t border = (ctx->focus_id == w->id) ? ctx->theme.splitter : ctx->theme.widget_border;
	draw_patch(ctx, MKGUI_STYLE_RAISED, rx, ry, rw, rh, bg, border);

	struct mkgui_dropdown_data *dd = find_dropdown_data(ctx, w->id);
	const char *text = w->label;
	if(dd && dd->selected >= 0 && dd->selected < (int32_t)dd->item_count) {
		text = dd->items[dd->selected];
	}
	uint32_t tc = disabled ? ctx->theme.text_disabled : ctx->theme.text;
	int32_t ty = ry + (rh - ctx->font_height) / 2;
	push_text_clip(rx + 4, ty, text, tc, rx + 1, ry + 1, rx + rw - 16, ry + rh - 1);

	int32_t ax = rx + rw - 14;
	int32_t ay = ry + rh / 2 - 2;
	for(uint32_t j = 0; j < 5; ++j) {
		draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, ax + (int32_t)j, ay + (int32_t)j, 9 - (int32_t)j * 2, tc);
	}
}

// [=]===^=[ dropdown_clamp_scroll ]=============================[=]
static void dropdown_clamp_scroll(struct mkgui_dropdown_data *dd, int32_t popup_h) {
	int32_t content_h = (int32_t)dd->item_count * MKGUI_ROW_HEIGHT;
	int32_t view_h = popup_h - 2;
	int32_t max_scroll = content_h - view_h;
	if(max_scroll < 0) {
		max_scroll = 0;
	}
	if(dd->scroll_y < 0) {
		dd->scroll_y = 0;
	}
	if(dd->scroll_y > max_scroll) {
		dd->scroll_y = max_scroll;
	}
}

// [=]===^=[ render_dropdown_popup ]=============================[=]
static void render_dropdown_popup(struct mkgui_ctx *ctx, struct mkgui_popup *p, struct mkgui_dropdown_data *dd, int32_t hover_item) {
	int32_t saved_clip_x1 = render_clip_x1;
	int32_t saved_clip_y1 = render_clip_y1;
	int32_t saved_clip_x2 = render_clip_x2;
	int32_t saved_clip_y2 = render_clip_y2;
	render_clip_x1 = 0;
	render_clip_y1 = 0;
	render_clip_x2 = INT32_MAX;
	render_clip_y2 = INT32_MAX;

	draw_rounded_rect(p->pixels, p->w, p->h, 0, 0, p->w, p->h, ctx->theme.menu_bg, ctx->theme.widget_border, ctx->theme.corner_radius);

	int32_t view_h = p->h - 2;
	int32_t content_h = (int32_t)dd->item_count * MKGUI_ROW_HEIGHT;
	uint32_t needs_scroll = content_h > view_h;
	int32_t item_w = needs_scroll ? p->w - 10 : p->w - 2;

	for(uint32_t i = 0; i < dd->item_count; ++i) {
		int32_t iy = 1 + (int32_t)i * MKGUI_ROW_HEIGHT - dd->scroll_y;
		if(iy + MKGUI_ROW_HEIGHT <= 0 || iy >= p->h) {
			continue;
		}
		uint32_t bg;
		if((int32_t)i == hover_item) {
			bg = ctx->theme.menu_hover;
		} else if((int32_t)i == dd->selected) {
			bg = ctx->theme.selection;
		} else {
			bg = ctx->theme.menu_bg;
		}
		draw_rect_fill(p->pixels, p->w, p->h, 1, iy, item_w, MKGUI_ROW_HEIGHT, bg);
		int32_t ty = iy + (MKGUI_ROW_HEIGHT - ctx->font_height) / 2;
		uint32_t tc = ((int32_t)i == dd->selected) ? ctx->theme.sel_text : ctx->theme.text;
		push_text_clip(p->x + 5, ty + p->y, dd->items[i], tc, p->x + 1, p->y + 1, p->x + p->w - 1, p->y + p->h - 1);
	}

	if(needs_scroll) {
		int32_t sb_x = p->w - 9;
		draw_rect_fill(p->pixels, p->w, p->h, sb_x, 1, 8, view_h, ctx->theme.scrollbar_bg);
		int32_t thumb_h = (view_h * view_h) / content_h;
		if(thumb_h < 16) {
			thumb_h = 16;
		}
		int32_t max_scroll = content_h - view_h;
		int32_t thumb_y = 1;
		if(max_scroll > 0) {
			thumb_y = 1 + (dd->scroll_y * (view_h - thumb_h)) / max_scroll;
		}
		draw_rounded_rect_fill(p->pixels, p->w, p->h, sb_x, thumb_y, 8, thumb_h, ctx->theme.scrollbar_thumb, ctx->theme.corner_radius);
	}

	render_clip_x1 = saved_clip_x1;
	render_clip_y1 = saved_clip_y1;
	render_clip_x2 = saved_clip_x2;
	render_clip_y2 = saved_clip_y2;
}

// [=]===^=[ dropdown_open_popup ]===============================[=]
static void dropdown_open_popup(struct mkgui_ctx *ctx, uint32_t widget_id) {
	struct mkgui_dropdown_data *dd = find_dropdown_data(ctx, widget_id);
	if(!dd || dd->item_count == 0) {
		return;
	}

	int32_t idx = find_widget_idx(ctx, widget_id);
	if(idx < 0) {
		return;
	}

	int32_t abs_x, abs_y;
	platform_translate_coords(ctx, ctx->rects[idx].x, ctx->rects[idx].y + ctx->rects[idx].h, &abs_x, &abs_y);

	int32_t pw = ctx->rects[idx].w;
	int32_t ph = (int32_t)dd->item_count * MKGUI_ROW_HEIGHT + 2;
	int32_t max_ph = MKGUI_DROPDOWN_MAX_VISIBLE * MKGUI_ROW_HEIGHT + 2;
	if(ph > max_ph) {
		ph = max_ph;
	}

	if(dd->selected > 0) {
		dd->scroll_y = dd->selected * MKGUI_ROW_HEIGHT - (ph - 2) / 2;
	} else {
		dd->scroll_y = 0;
	}
	dropdown_clamp_scroll(dd, ph);

	popup_destroy_all(ctx);
	struct mkgui_popup *p = popup_create(ctx, abs_x, abs_y, pw, ph, widget_id);
	if(p) {
		render_dropdown_popup(ctx, p, dd, -1);
		flush_text_popup(ctx, p);
		platform_popup_blit(ctx, p);
		platform_flush(ctx);
	}
}

// [=]===^=[ handle_dropdown_key ]===============================[=]
static uint32_t handle_dropdown_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks) {
	if(ks == MKGUI_KEY_RETURN || ks == MKGUI_KEY_SPACE) {
		dropdown_open_popup(ctx, ctx->focus_id);
		return 0;
	}

	struct mkgui_dropdown_data *dd = find_dropdown_data(ctx, ctx->focus_id);
	if(!dd) {
		return 0;
	}

	if(ks == MKGUI_KEY_UP && dd->selected > 0) {
		--dd->selected;
		dirty_all(ctx);
		ev->type = MKGUI_EVENT_DROPDOWN_CHANGED;
		ev->id = ctx->focus_id;
		ev->value = dd->selected;
		return 1;
	}

	if(ks == MKGUI_KEY_DOWN && dd->selected < (int32_t)dd->item_count - 1) {
		++dd->selected;
		dirty_all(ctx);
		ev->type = MKGUI_EVENT_DROPDOWN_CHANGED;
		ev->id = ctx->focus_id;
		ev->value = dd->selected;
		return 1;
	}

	return 0;
}

// [=]===^=[ mkgui_dropdown_setup ]==============================[=]
static void mkgui_dropdown_setup(struct mkgui_ctx *ctx, uint32_t id, const char **items, uint32_t count) {
	struct mkgui_dropdown_data *dd = find_dropdown_data(ctx, id);
	if(!dd) {
		return;
	}
	dd->item_count = count > MKGUI_MAX_DROPDOWN ? MKGUI_MAX_DROPDOWN : count;
	for(uint32_t i = 0; i < dd->item_count; ++i) {
		strncpy(dd->items[i], items[i], MKGUI_MAX_TEXT - 1);
		dd->items[i][MKGUI_MAX_TEXT - 1] = '\0';
	}
	dd->selected = 0;
	dd->scroll_y = 0;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_dropdown_get ]================================[=]
static int32_t mkgui_dropdown_get(struct mkgui_ctx *ctx, uint32_t id) {
	struct mkgui_dropdown_data *dd = find_dropdown_data(ctx, id);
	return dd ? dd->selected : -1;
}
