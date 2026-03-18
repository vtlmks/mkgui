// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ render_slider ]=====================================[=]
static void render_slider(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	struct mkgui_slider_data *sd = find_slider_data(ctx, w->id);
	if(!sd) {
		return;
	}

	uint32_t disabled = (w->flags & MKGUI_DISABLED);

	int32_t track_y = ry + rh / 2 - 2;
	draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx, track_y, rw, 4, ctx->theme.widget_border, 2);

	int32_t range = sd->max_val - sd->min_val;
	if(range <= 0) {
		range = 1;
	}
	int32_t thumb_x = rx + (int32_t)((int64_t)(sd->value - sd->min_val) * (rw - 10) / range);
	uint32_t thumb_color = disabled ? ctx->theme.widget_border : ((ctx->focus_id == w->id) ? ctx->theme.sel_text : ctx->theme.splitter);
	uint32_t thumb_border = disabled ? ctx->theme.widget_border : ctx->theme.splitter;
	draw_patch(ctx, MKGUI_STYLE_RAISED, thumb_x, ry + 2, 10, rh - 4, thumb_color, thumb_border);

}

// [=]===^=[ handle_slider_key ]=================================[=]
static uint32_t handle_slider_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks) {
	struct mkgui_slider_data *sd = find_slider_data(ctx, ctx->focus_id);
	if(!sd) {
		return 0;
	}

	int32_t range = sd->max_val - sd->min_val;
	int32_t step = range / 20;
	if(step < 1) {
		step = 1;
	}

	if(ks == MKGUI_KEY_LEFT) {
		sd->value -= step;
		if(sd->value < sd->min_val) {
			sd->value = sd->min_val;
		}

	} else if(ks == MKGUI_KEY_RIGHT) {
		sd->value += step;
		if(sd->value > sd->max_val) {
			sd->value = sd->max_val;
		}

	} else if(ks == MKGUI_KEY_HOME) {
		sd->value = sd->min_val;

	} else if(ks == MKGUI_KEY_END) {
		sd->value = sd->max_val;

	} else {
		return 0;
	}

	dirty_all(ctx);
	ev->type = MKGUI_EVENT_SLIDER_CHANGED;
	ev->id = ctx->focus_id;
	ev->value = sd->value;
	return 1;
}

// [=]===^=[ mkgui_slider_setup ]================================[=]
static void mkgui_slider_setup(struct mkgui_ctx *ctx, uint32_t id, int32_t min_val, int32_t max_val, int32_t value) {
	struct mkgui_slider_data *sd = find_slider_data(ctx, id);
	if(!sd) {
		return;
	}
	sd->min_val = min_val;
	sd->max_val = max_val;
	sd->value = value;
	if(sd->value < sd->min_val) {
		sd->value = sd->min_val;
	}
	if(sd->value > sd->max_val) {
		sd->value = sd->max_val;
	}
	dirty_all(ctx);
}

// [=]===^=[ mkgui_slider_get ]=================================[=]
static int32_t mkgui_slider_get(struct mkgui_ctx *ctx, uint32_t id) {
	struct mkgui_slider_data *sd = find_slider_data(ctx, id);
	return sd ? sd->value : 0;
}
