// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ render_toolbar ]=====================================[=]
static void render_toolbar(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;
	uint32_t mode = w->flags & MKGUI_TOOLBAR_MODE_MASK;
	uint32_t show_icons = (mode != MKGUI_TOOLBAR_TEXT_ONLY);
	uint32_t show_text = (mode != MKGUI_TOOLBAR_ICONS_ONLY);

	draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx, ry, rw, rh, ctx->theme.bg);

	int32_t bx = rx + 2;
	for(uint32_t i = 0; i < ctx->widget_count; ++i) {
		struct mkgui_widget *btn = &ctx->widgets[i];
		if(btn->type != MKGUI_BUTTON || btn->parent_id != w->id) {
			continue;
		}

		if(btn->flags & MKGUI_TOOLBAR_SEP) {
			int32_t sx = bx + MKGUI_TOOLBAR_SEP_W / 2;
			draw_vline(ctx->pixels, ctx->win_w, ctx->win_h, sx, ry + 4, rh - 8, ctx->theme.widget_border);
			bx += MKGUI_TOOLBAR_SEP_W;
		}

		int32_t ii = show_icons ? toolbar_icon_idx(btn) : -1;
		uint32_t has_icon = (ii >= 0);
		int32_t tw = show_text ? text_width(ctx, btn->label) : 0;
		int32_t icon_w = has_icon ? icons[ii].w + 4 : 0;
		int32_t content_w = icon_w + tw;
		int32_t bw = content_w + 12;
		if(bw < MKGUI_TOOLBAR_BTN_W) {
			bw = MKGUI_TOOLBAR_BTN_W;
		}

		int32_t by = ry + 2;
		int32_t bh = rh - 4;

		int32_t btn_idx = find_widget_idx(ctx, btn->id);
		if(btn_idx >= 0) {
			ctx->rects[btn_idx].x = bx;
			ctx->rects[btn_idx].y = by;
			ctx->rects[btn_idx].w = bw;
			ctx->rects[btn_idx].h = bh;
		}

		if(ctx->press_id == btn->id) {
			draw_patch(ctx, MKGUI_STYLE_RAISED, bx, by, bw, bh, ctx->theme.widget_press, ctx->theme.widget_border);
		} else if(ctx->hover_id == btn->id) {
			draw_patch(ctx, MKGUI_STYLE_RAISED, bx, by, bw, bh, ctx->theme.widget_hover, ctx->theme.widget_border);
		}

		int32_t cx = bx + (bw - content_w) / 2;
		if(has_icon) {
			int32_t iy = by + (bh - icons[ii].h) / 2;
			draw_icon(ctx->pixels, ctx->win_w, ctx->win_h, &icons[ii], cx, iy, bx, by, bx + bw, by + bh);
			cx += icons[ii].w + 4;
		}
		if(show_text && btn->label[0]) {
			int32_t ty = by + (bh - ctx->font_height) / 2;
			push_text_clip(cx, ty, btn->label, ctx->theme.text, bx, by, bx + bw, by + bh);
		}

		bx += bw + 2;
	}
}
