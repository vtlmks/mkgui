// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ tab_calc_width ]=====================================[=]
static int32_t tab_calc_width(struct mkgui_ctx *ctx, struct mkgui_widget *child) {
	int32_t tab_pad = sc(ctx, 10);
	int32_t icon_gap = sc(ctx, 4);
	int32_t close_w = sc(ctx, 16);
	int32_t tw = label_text_width(ctx, child) + tab_pad * 2;
	int32_t ii = widget_icon_idx(ctx, child);
	if(ii >= 0) {
		tw += icons[ii].w + icon_gap;
	}
	if(child->style & MKGUI_TAB_CLOSABLE) {
		tw += close_w;
	}
	return tw;
}

// [=]===^=[ tab_close_hit_test ]=================================[=]
static uint32_t tab_close_hit_test(struct mkgui_ctx *ctx, uint32_t tabs_idx, int32_t mx, int32_t my) {
	int32_t rx = ctx->rects[tabs_idx].x;
	int32_t ry = ctx->rects[tabs_idx].y;

	if(my < ry || my >= ry + ctx->tab_height) {
		return 0;
	}

	int32_t tx = rx;
	for(uint32_t c = layout_first_child[tabs_idx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
		struct mkgui_widget *child = &ctx->widgets[c];
		if(child->type != MKGUI_TAB) {
			continue;
		}
		int32_t tw = tab_calc_width(ctx, child);
		if(mx >= tx && mx < tx + tw && (child->style & MKGUI_TAB_CLOSABLE)) {
			int32_t close_w = sc(ctx, 16);
			int32_t close_sz = sc(ctx, 10);
			int32_t bx = tx + tw - close_w;
			int32_t by = ry + (ctx->tab_height - close_sz) / 2;
			if(mx >= bx && mx < bx + close_sz && my >= by && my < by + close_sz) {
				return child->id;
			}
		}
		tx += tw;
	}
	return 0;
}

// [=]===^=[ tab_hit_test ]======================================[=]
static uint32_t tab_hit_test(struct mkgui_ctx *ctx, uint32_t tabs_idx, int32_t mx, int32_t my) {
	int32_t rx = ctx->rects[tabs_idx].x;
	int32_t ry = ctx->rects[tabs_idx].y;

	if(my < ry || my >= ry + ctx->tab_height) {
		return 0;
	}

	int32_t tx = rx;
	for(uint32_t c = layout_first_child[tabs_idx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
		struct mkgui_widget *child = &ctx->widgets[c];
		if(child->type != MKGUI_TAB) {
			continue;
		}
		int32_t tw = tab_calc_width(ctx, child);
		if(mx >= tx && mx < tx + tw) {
			return child->id;
		}
		tx += tw;
	}
	return 0;
}

// [=]===^=[ render_tabs ]=======================================[=]
static void render_tabs(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;

	struct mkgui_tabs_data *td = find_tabs_data(ctx, w->id);

	draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx, ry, rw, ctx->tab_height, ctx->theme.tab_inactive);

	uint32_t hover_tab = (td && ctx->hover_id == w->id) ? td->hover_tab : 0;

	int32_t tab_pad = sc(ctx, 10);
	int32_t icon_gap = sc(ctx, 4);
	int32_t close_off = sc(ctx, 14);
	int32_t close_sz = sc(ctx, 10);
	int32_t close_glyph = sc(ctx, 8);

	int32_t tx = rx;
	for(uint32_t c = layout_first_child[idx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
		struct mkgui_widget *child = &ctx->widgets[c];
		if(child->type != MKGUI_TAB) {
			continue;
		}
		int32_t tw = tab_calc_width(ctx, child);
		uint32_t active = (td && td->active_tab == child->id);
		uint32_t focused_tab = (active && ctx->focus_id == w->id);
		uint32_t hovered = (!active && hover_tab == child->id);
		uint32_t bg = active ? ctx->theme.tab_active : (hovered ? ctx->theme.tab_hover : ctx->theme.tab_inactive);
		uint32_t bd = active ? (focused_tab ? ctx->theme.highlight : ctx->theme.widget_border) : (hovered ? ctx->theme.tab_hover : ctx->theme.tab_inactive);
		draw_rounded_rect(ctx->pixels, ctx->win_w, ctx->win_h, tx, ry, tw, ctx->tab_height, bg, bd, ctx->theme.corner_radius);
		if(active) {
			draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, tx, ry, tw, ctx->theme.highlight);
		}
		int32_t cx = tx + tab_pad;
		int32_t ii = widget_icon_idx(ctx, child);
		if(ii >= 0) {
			int32_t iy = ry + (ctx->tab_height - icons[ii].h) / 2;
			draw_icon(ctx->pixels, ctx->win_w, ctx->win_h, &icons[ii], cx, iy, tx + 1, ry + 1, tx + tw - 1, ry + ctx->tab_height - 1);
			cx += icons[ii].w + icon_gap;
		}
		int32_t tty = ry + (ctx->tab_height - ctx->font_height) / 2;
		push_text_clip(cx, tty, child->label, ctx->theme.text, tx, ry, tx + tw, ry + ctx->tab_height);
		if(child->style & MKGUI_TAB_CLOSABLE) {
			int32_t bx = tx + tw - close_off;
			int32_t by = ry + (ctx->tab_height - close_sz) / 2;
			uint32_t clr = ctx->theme.text_disabled;
			draw_aa_line(ctx->pixels, ctx->win_w, ctx->win_h, bx, by, bx + close_glyph, by + close_glyph, clr, 2);
			draw_aa_line(ctx->pixels, ctx->win_w, ctx->win_h, bx + close_glyph, by, bx, by + close_glyph, clr, 2);
		}
		tx += tw;
	}

	draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, rx, ry + ctx->tab_height - 1, rw, ctx->theme.widget_border);
}

// [=]===^=[ handle_tabs_key ]======================================[=]
static uint32_t handle_tabs_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks) {
	struct mkgui_widget *w = find_widget(ctx, ctx->focus_id);
	if(!w) {
		return 0;
	}
	if(ks != MKGUI_KEY_LEFT && ks != MKGUI_KEY_RIGHT) {
		return 0;
	}

	struct mkgui_tabs_data *td = find_tabs_data(ctx, w->id);
	if(!td) {
		return 0;
	}

	int32_t widx = find_widget_idx(ctx, w->id);
	if(widx < 0) {
		return 0;
	}

	int32_t cur = -1;
	int32_t count = 0;
	for(uint32_t c = layout_first_child[widx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
		struct mkgui_widget *child = &ctx->widgets[c];
		if(child->type != MKGUI_TAB) {
			continue;
		}
		if(child->id == td->active_tab) {
			cur = count;
		}
		++count;
	}
	if(count <= 1 || cur < 0) {
		return 0;
	}

	int32_t next = cur + (ks == MKGUI_KEY_LEFT ? -1 : 1);
	if(next < 0) {
		next = count - 1;
	}
	if(next >= count) {
		next = 0;
	}

	struct mkgui_widget *tab = NULL;
	int32_t n = 0;
	for(uint32_t c = layout_first_child[widx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
		struct mkgui_widget *child = &ctx->widgets[c];
		if(child->type == MKGUI_TAB) {
			if(n == next) {
				tab = child;
				break;
			}
			++n;
		}
	}
	if(tab && tab->id != td->active_tab) {
		td->active_tab = tab->id;
		dirty_all(ctx);
		ev->type = MKGUI_EVENT_TAB_CHANGED;
		ev->id = w->id;
		ev->value = (int32_t)tab->id;
		return 1;
	}
	return 0;
}

// [=]===^=[ mkgui_tabs_get_current ]================================[=]
MKGUI_API uint32_t mkgui_tabs_get_current(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_tabs_data *td = find_tabs_data(ctx, id);
	return td ? td->active_tab : 0;
}

// [=]===^=[ mkgui_tabs_set_current ]================================[=]
MKGUI_API void mkgui_tabs_set_current(struct mkgui_ctx *ctx, uint32_t id, uint32_t tab_id) {
	MKGUI_CHECK(ctx);
	struct mkgui_tabs_data *td = find_tabs_data(ctx, id);
	if(!td) {
		return;
	}
	td->active_tab = tab_id;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_tabs_get_count ]==================================[=]
MKGUI_API uint32_t mkgui_tabs_get_count(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, 0);
	int32_t widx = find_widget_idx(ctx, id);
	if(widx < 0) {
		return 0;
	}
	uint32_t count = 0;
	for(uint32_t c = layout_first_child[widx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
		if(ctx->widgets[c].type == MKGUI_TAB) {
			++count;
		}
	}
	return count;
}

// [=]===^=[ mkgui_tabs_set_text ]==================================[=]
MKGUI_API void mkgui_tabs_set_text(struct mkgui_ctx *ctx, uint32_t tabs_id, uint32_t tab_id, const char *text) {
	MKGUI_CHECK(ctx);
	if(!text) {
		text = "";
	}
	(void)tabs_id;
	struct mkgui_widget *w = find_widget(ctx, tab_id);
	if(!w || w->type != MKGUI_TAB) {
		return;
	}
	strncpy(w->label, text, MKGUI_MAX_TEXT - 1);
	w->label[MKGUI_MAX_TEXT - 1] = '\0';
	w->label_tw = -1;
	dirty_widget_id(ctx, tabs_id);
}

// [=]===^=[ mkgui_tabs_get_text ]==================================[=]
MKGUI_API char *mkgui_tabs_get_text(struct mkgui_ctx *ctx, uint32_t tabs_id, uint32_t tab_id) {
	MKGUI_CHECK_VAL(ctx, "");
	(void)tabs_id;
	struct mkgui_widget *w = find_widget(ctx, tab_id);
	return (w && w->type == MKGUI_TAB) ? w->label : "";
}
