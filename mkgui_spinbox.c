// Copyright (c) 2026, mkgui contributors
// SPDX-License-Identifier: MIT

#define MKGUI_SPINBOX_BTN_W 20

// [=]===^=[ spinbox_btn_hit ]====================================[=]
static int32_t spinbox_btn_hit(struct mkgui_ctx *ctx, uint32_t idx, int32_t mx, int32_t my) {
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;
	int32_t bx = rx + rw - MKGUI_SPINBOX_BTN_W;

	if(mx < bx || mx >= rx + rw) {
		return 0;
	}
	if(my < ry + rh / 2) {
		return 1;
	}
	return -1;
}

// [=]===^=[ render_spinbox ]=====================================[=]
static void render_spinbox(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	uint32_t focused = (ctx->focus_id == w->id);
	draw_patch(ctx, MKGUI_STYLE_SUNKEN, rx, ry, rw, rh, ctx->theme.input_bg, focused ? ctx->theme.splitter : ctx->theme.widget_border);

	struct mkgui_spinbox_data *sd = find_spinbox_data(ctx, w->id);
	if(sd) {
		char buf[64];
		snprintf(buf, sizeof(buf), "%d", sd->value);
		int32_t ty = ry + (rh - ctx->font_height) / 2;
		push_text_clip(rx + 4, ty, buf, ctx->theme.text, rx + 1, ry + 1, rx + rw - MKGUI_SPINBOX_BTN_W, ry + rh - 1);
	}

	int32_t bx = rx + rw - MKGUI_SPINBOX_BTN_W;
	int32_t half = rh / 2;
	int32_t hover_btn = (ctx->hover_id == w->id && sd) ? sd->hover_btn : 0;

	uint32_t up_bg = (hover_btn == 1) ? ctx->theme.widget_hover : ctx->theme.widget_bg;
	draw_patch(ctx, MKGUI_STYLE_RAISED, bx, ry, MKGUI_SPINBOX_BTN_W, half, up_bg, ctx->theme.widget_border);

	int32_t acx = bx + MKGUI_SPINBOX_BTN_W / 2;
	int32_t acy = ry + half / 2 - 2;
	for(int32_t j = 0; j < 4; ++j) {
		draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, acx - j, acy + j, 1 + j * 2, ctx->theme.text);
	}

	uint32_t dn_bg = (hover_btn == -1) ? ctx->theme.widget_hover : ctx->theme.widget_bg;
	draw_patch(ctx, MKGUI_STYLE_RAISED, bx, ry + half, MKGUI_SPINBOX_BTN_W, rh - half, dn_bg, ctx->theme.widget_border);

	int32_t acy2 = ry + half + (rh - half) / 2 - 2;
	for(int32_t j = 0; j < 4; ++j) {
		draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, acx - (3 - j), acy2 + j, 1 + (3 - j) * 2, ctx->theme.text);
	}
}

// [=]===^=[ spinbox_adjust ]=====================================[=]
static uint32_t spinbox_adjust(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t widget_id, int32_t delta) {
	struct mkgui_spinbox_data *sd = find_spinbox_data(ctx, widget_id);
	if(!sd) {
		return 0;
	}
	int32_t new_val = sd->value + delta;
	if(new_val < sd->min_val) {
		new_val = sd->min_val;
	}
	if(new_val > sd->max_val) {
		new_val = sd->max_val;
	}
	if(new_val != sd->value) {
		sd->value = new_val;
		dirty_all(ctx);
		ev->type = MKGUI_EVENT_SPINBOX_CHANGED;
		ev->id = widget_id;
		ev->value = new_val;
		return 1;
	}
	return 0;
}

// [=]===^=[ handle_spinbox_key ]=================================[=]
static uint32_t handle_spinbox_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks) {
	struct mkgui_spinbox_data *sd = find_spinbox_data(ctx, ctx->focus_id);
	if(!sd) {
		return 0;
	}
	if(ks == MKGUI_KEY_UP) {
		return spinbox_adjust(ctx, ev, ctx->focus_id, sd->step);
	}
	if(ks == MKGUI_KEY_DOWN) {
		return spinbox_adjust(ctx, ev, ctx->focus_id, -sd->step);
	}
	if(ks == MKGUI_KEY_HOME) {
		return spinbox_adjust(ctx, ev, ctx->focus_id, sd->min_val - sd->value);
	}
	if(ks == MKGUI_KEY_END) {
		return spinbox_adjust(ctx, ev, ctx->focus_id, sd->max_val - sd->value);
	}
	return 0;
}

// [=]===^=[ mkgui_spinbox_setup ]================================[=]
static void mkgui_spinbox_setup(struct mkgui_ctx *ctx, uint32_t id, int32_t min_val, int32_t max_val, int32_t value, int32_t step) {
	struct mkgui_spinbox_data *sd = find_spinbox_data(ctx, id);
	if(sd) {
		sd->min_val = min_val;
		sd->max_val = max_val;
		sd->value = value;
		sd->step = step > 0 ? step : 1;
		dirty_all(ctx);
	}
}

// [=]===^=[ mkgui_spinbox_get ]=================================[=]
static int32_t mkgui_spinbox_get(struct mkgui_ctx *ctx, uint32_t id) {
	struct mkgui_spinbox_data *sd = find_spinbox_data(ctx, id);
	return sd ? sd->value : 0;
}

// [=]===^=[ mkgui_spinbox_set ]=================================[=]
static void mkgui_spinbox_set(struct mkgui_ctx *ctx, uint32_t id, int32_t value) {
	struct mkgui_spinbox_data *sd = find_spinbox_data(ctx, id);
	if(sd) {
		sd->value = value;
		if(sd->value < sd->min_val) {
			sd->value = sd->min_val;
		}
		if(sd->value > sd->max_val) {
			sd->value = sd->max_val;
		}
		dirty_all(ctx);
	}
}
