// Copyright (c) 2026, mkgui contributors
// SPDX-License-Identifier: MIT

#define MKGUI_TOOLTIP_DELAY 30
#define MKGUI_TOOLTIP_PAD   4

// [=]===^=[ render_tooltip ]=====================================[=]
static void render_tooltip(struct mkgui_ctx *ctx) {
	if(!ctx->tooltip_id || ctx->tooltip_timer < MKGUI_TOOLTIP_DELAY) {
		return;
	}

	struct mkgui_widget *w = find_widget(ctx, ctx->tooltip_id);
	if(!w || w->label[0] == '\0') {
		return;
	}

	struct mkgui_widget *parent = find_widget(ctx, w->parent_id);
	if(!parent || parent->type != MKGUI_TOOLBAR) {
		return;
	}

	int32_t tw = text_width(ctx, w->label) + MKGUI_TOOLTIP_PAD * 2;
	int32_t th = ctx->font_height + MKGUI_TOOLTIP_PAD * 2;
	int32_t tx = ctx->tooltip_x + 12;
	int32_t ty = ctx->tooltip_y + 16;

	if(tx + tw > ctx->win_w) {
		tx = ctx->win_w - tw;
	}
	if(ty + th > ctx->win_h) {
		ty = ctx->tooltip_y - th - 4;
	}

	draw_rounded_rect(ctx->pixels, ctx->win_w, ctx->win_h, tx, ty, tw, th, ctx->theme.widget_bg, ctx->theme.widget_border, ctx->theme.corner_radius);
	push_text_clip(tx + MKGUI_TOOLTIP_PAD, ty + MKGUI_TOOLTIP_PAD, w->label, ctx->theme.text, tx, ty, tx + tw, ty + th);
}

// [=]===^=[ tooltip_update ]=====================================[=]
static void tooltip_update(struct mkgui_ctx *ctx, uint32_t hover_id, int32_t mx, int32_t my) {
	if(hover_id != ctx->tooltip_id) {
		ctx->tooltip_id = hover_id;
		ctx->tooltip_timer = 0;
		ctx->tooltip_x = mx;
		ctx->tooltip_y = my;

	} else if(ctx->tooltip_id) {
		if(ctx->tooltip_timer < MKGUI_TOOLTIP_DELAY) {
			++ctx->tooltip_timer;
			if(ctx->tooltip_timer == MKGUI_TOOLTIP_DELAY) {
				dirty_all(ctx);
			}
		}
	}
}
