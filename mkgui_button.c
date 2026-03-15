// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ render_button ]=====================================[=]
static void render_button(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	uint32_t bg = ctx->theme.widget_bg;
	if(ctx->press_id == w->id) {
		bg = ctx->theme.widget_press;

	} else if(ctx->hover_id == w->id) {
		bg = ctx->theme.widget_hover;
	}
	uint32_t border = (ctx->focus_id == w->id) ? ctx->theme.splitter : ctx->theme.widget_border;
	draw_patch(ctx, MKGUI_STYLE_RAISED, rx, ry, rw, rh, bg, border);

	uint32_t tc = (w->flags & MKGUI_DISABLED) ? ctx->theme.text_disabled : ctx->theme.text;
	uint32_t has_icon = (widget_icon_idx(w) >= 0);
	int32_t tw = text_width(ctx, w->label);
	int32_t icon_w = has_icon ? MKGUI_ICON_SIZE + 4 : 0;
	int32_t content_w = icon_w + (w->label[0] ? tw : 0);
	int32_t cx = rx + (rw - content_w) / 2;
	if(has_icon) {
		int32_t iy = ry + (rh - MKGUI_ICON_SIZE) / 2;
		draw_icon(ctx->pixels, ctx->win_w, ctx->win_h, &icons[widget_icon_idx(w)], cx, iy, rx, ry, rx + rw, ry + rh);
		cx += MKGUI_ICON_SIZE + 4;
	}
	if(w->label[0]) {
		int32_t ty = ry + (rh - ctx->font_height) / 2;
		push_text_clip(cx, ty, w->label, tc, rx, ry, rx + rw, ry + rh);
	}
}

// [=]===^=[ handle_button_key ]=================================[=]
static uint32_t handle_button_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks) {
	if(ks == MKGUI_KEY_RETURN || ks == MKGUI_KEY_SPACE) {
		ev->type = MKGUI_EVENT_CLICK;
		ev->id = ctx->focus_id;
		dirty_all(ctx);
		return 1;
	}
	return 0;
}
