// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ tab_calc_width ]=====================================[=]
static int32_t tab_calc_width(struct mkgui_ctx *ctx, struct mkgui_widget *child) {
	int32_t tw = text_width(ctx, child->label) + 20;
	if(widget_icon_idx(child) >= 0) {
		tw += MKGUI_ICON_SIZE + 4;
	}
	return tw;
}

// [=]===^=[ tab_hit_test ]======================================[=]
static uint32_t tab_hit_test(struct mkgui_ctx *ctx, uint32_t tabs_idx, int32_t mx, int32_t my) {
	struct mkgui_widget *w = &ctx->widgets[tabs_idx];
	int32_t rx = ctx->rects[tabs_idx].x;
	int32_t ry = ctx->rects[tabs_idx].y;

	if(my < ry || my >= ry + MKGUI_TAB_HEIGHT) {
		return 0;
	}

	int32_t tx = rx;
	for(uint32_t i = 0; i < ctx->widget_count; ++i) {
		struct mkgui_widget *child = &ctx->widgets[i];
		if(child->type != MKGUI_TAB || child->parent_id != w->id) {
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

	draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx, ry, rw, MKGUI_TAB_HEIGHT, ctx->theme.tab_inactive);

	uint32_t hover_tab = (td && ctx->hover_id == w->id) ? td->hover_tab : 0;

	int32_t tx = rx;
	for(uint32_t i = 0; i < ctx->widget_count; ++i) {
		struct mkgui_widget *child = &ctx->widgets[i];
		if(child->type != MKGUI_TAB || child->parent_id != w->id) {
			continue;
		}
		int32_t tw = tab_calc_width(ctx, child);
		uint32_t active = (td && td->active_tab == child->id);
		uint32_t focused_tab = (active && ctx->focus_id == w->id);
		uint32_t hovered = (!active && hover_tab == child->id);
		uint32_t bg = active ? ctx->theme.tab_active : (hovered ? ctx->theme.tab_hover : ctx->theme.tab_inactive);
		uint32_t bd = active ? (focused_tab ? ctx->theme.splitter : ctx->theme.widget_border) : (hovered ? ctx->theme.tab_hover : ctx->theme.tab_inactive);
		draw_rounded_rect(ctx->pixels, ctx->win_w, ctx->win_h, tx, ry, tw, MKGUI_TAB_HEIGHT, bg, bd, ctx->theme.corner_radius);
		if(active) {
			draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, tx, ry, tw, ctx->theme.splitter);
		}
		int32_t cx = tx + 10;
		uint32_t has_icon = (widget_icon_idx(child) >= 0);
		if(has_icon) {
			int32_t iy = ry + (MKGUI_TAB_HEIGHT - MKGUI_ICON_SIZE) / 2;
			draw_icon(ctx->pixels, ctx->win_w, ctx->win_h, &icons[widget_icon_idx(child)], cx, iy, tx, ry, tx + tw, ry + MKGUI_TAB_HEIGHT);
			cx += MKGUI_ICON_SIZE + 4;
		}
		int32_t tty = ry + (MKGUI_TAB_HEIGHT - ctx->font_height) / 2;
		push_text_clip(cx, tty, child->label, ctx->theme.text, tx, ry, tx + tw, ry + MKGUI_TAB_HEIGHT);
		tx += tw;
	}

	draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, rx, ry + MKGUI_TAB_HEIGHT - 1, rw, ctx->theme.widget_border);
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

	int32_t cur = -1;
	int32_t count = 0;
	for(uint32_t i = 0; i < ctx->widget_count; ++i) {
		struct mkgui_widget *child = &ctx->widgets[i];
		if(child->type != MKGUI_TAB || child->parent_id != w->id) {
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
	for(uint32_t i = 0; i < ctx->widget_count; ++i) {
		struct mkgui_widget *child = &ctx->widgets[i];
		if(child->type == MKGUI_TAB && child->parent_id == w->id) {
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
