// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define MKGUI_SLIDER_TRACK_SIZE       4
#define MKGUI_SLIDER_THUMB_SIZE       10
#define MKGUI_SLIDER_TAPER_MIN        2

// [=]===^=[ slider_draw_taper_v ]================================[=]
static void slider_draw_taper_v(uint32_t *buf, int32_t bw, int32_t bh,
	int32_t cx, int32_t y, int32_t h,
	int32_t row_start, int32_t row_count,
	int32_t min_w, int32_t max_w, uint32_t color) {
	if(h <= 1 || row_count <= 0) {
		return;
	}
	for(int32_t i = 0; i < row_count; ++i) {
		int32_t row = row_start + i;
		int32_t w_at_row = max_w - (max_w - min_w) * row / (h - 1);
		draw_rect_fill(buf, bw, bh, cx - w_at_row / 2, y + row, w_at_row, 1, color);
	}
}

// [=]===^=[ slider_draw_taper_h ]================================[=]
static void slider_draw_taper_h(uint32_t *buf, int32_t bw, int32_t bh,
	int32_t cy, int32_t x, int32_t w,
	int32_t col_start, int32_t col_count,
	int32_t min_h, int32_t max_h, uint32_t color) {
	if(w <= 1 || col_count <= 0) {
		return;
	}
	for(int32_t i = 0; i < col_count; ++i) {
		int32_t col = col_start + i;
		int32_t h_at_col = min_h + (max_h - min_h) * col / (w - 1);
		draw_rect_fill(buf, bw, bh, x + col, cy - h_at_col / 2, 1, h_at_col, color);
	}
}

// [=]===^=[ slider_draw_meter_v ]================================[=]
static void slider_draw_meter_v(uint32_t *buf, int32_t bw, int32_t bh,
	int32_t cx, int32_t y, int32_t h,
	int32_t min_w, int32_t max_w, struct mkgui_slider_data *sd) {
	if(sd->meter_pre_color != 0 && sd->meter_pre > 0.0f) {
		float pre = sd->meter_pre;
		if(pre > 1.0f) {
			pre = 1.0f;
		}
		int32_t ph = (int32_t)(pre * (float)h);
		if(ph > 0) {
			slider_draw_taper_v(buf, bw, bh, cx, y, h, h - ph, ph, min_w, max_w, sd->meter_pre_color);
		}
	}
	if(sd->meter_post_color != 0 && sd->meter_post > 0.0f) {
		float post = sd->meter_post;
		if(post > 1.0f) {
			post = 1.0f;
		}
		int32_t ph = (int32_t)(post * (float)h);
		if(ph > 0) {
			slider_draw_taper_v(buf, bw, bh, cx, y, h, h - ph, ph, min_w, max_w, sd->meter_post_color);
		}
	}
}

// [=]===^=[ slider_draw_meter_h ]================================[=]
static void slider_draw_meter_h(uint32_t *buf, int32_t bw, int32_t bh,
	int32_t cy, int32_t x, int32_t w,
	int32_t min_h, int32_t max_h, struct mkgui_slider_data *sd) {
	if(sd->meter_pre_color != 0 && sd->meter_pre > 0.0f) {
		float pre = sd->meter_pre;
		if(pre > 1.0f) {
			pre = 1.0f;
		}
		int32_t pw = (int32_t)(pre * (float)w);
		if(pw > 0) {
			slider_draw_taper_h(buf, bw, bh, cy, x, w, 0, pw, min_h, max_h, sd->meter_pre_color);
		}
	}
	if(sd->meter_post_color != 0 && sd->meter_post > 0.0f) {
		float post = sd->meter_post;
		if(post > 1.0f) {
			post = 1.0f;
		}
		int32_t pw = (int32_t)(post * (float)w);
		if(pw > 0) {
			slider_draw_taper_h(buf, bw, bh, cy, x, w, 0, pw, min_h, max_h, sd->meter_post_color);
		}
	}
}

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
	uint32_t vertical = (w->flags & MKGUI_VERTICAL);
	uint32_t mixer = (w->flags & MKGUI_SLIDER_MIXER);

	int32_t range = sd->max_val - sd->min_val;
	if(range <= 0) {
		range = 1;
	}

	uint32_t thumb_color = disabled ? ctx->theme.widget_border : ((ctx->focus_id == w->id) ? ctx->theme.sel_text : ctx->theme.splitter);
	uint32_t thumb_border = disabled ? ctx->theme.widget_border : ctx->theme.splitter;

	if(vertical) {
		if(mixer) {
			int32_t cx = rx + rw / 2;
			int32_t taper_max = rw - 4;
			slider_draw_taper_v(ctx->pixels, ctx->win_w, ctx->win_h, cx, ry, rh, 0, rh, MKGUI_SLIDER_TAPER_MIN, taper_max, ctx->theme.widget_border);
			slider_draw_meter_v(ctx->pixels, ctx->win_w, ctx->win_h, cx, ry, rh, MKGUI_SLIDER_TAPER_MIN, taper_max, sd);

		} else {
			int32_t track_w = MKGUI_SLIDER_TRACK_SIZE;
			int32_t track_x = rx + rw / 2 - track_w / 2;
			draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, track_x, ry, track_w, rh, ctx->theme.widget_border, 2);
		}

		int32_t thumb_y = ry + rh - MKGUI_SLIDER_THUMB_SIZE - (int32_t)((int64_t)(sd->value - sd->min_val) * (rh - MKGUI_SLIDER_THUMB_SIZE) / range);
		draw_patch(ctx, MKGUI_STYLE_RAISED, rx + 2, thumb_y, rw - 4, MKGUI_SLIDER_THUMB_SIZE, thumb_color, thumb_border);

	} else {
		if(mixer) {
			int32_t cy = ry + rh / 2;
			int32_t taper_max = rh - 4;
			slider_draw_taper_h(ctx->pixels, ctx->win_w, ctx->win_h, cy, rx, rw, 0, rw, MKGUI_SLIDER_TAPER_MIN, taper_max, ctx->theme.widget_border);
			slider_draw_meter_h(ctx->pixels, ctx->win_w, ctx->win_h, cy, rx, rw, MKGUI_SLIDER_TAPER_MIN, taper_max, sd);

		} else {
			int32_t track_y = ry + rh / 2 - MKGUI_SLIDER_TRACK_SIZE / 2;
			draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx, track_y, rw, MKGUI_SLIDER_TRACK_SIZE, ctx->theme.widget_border, 2);
		}

		int32_t thumb_x = rx + (int32_t)((int64_t)(sd->value - sd->min_val) * (rw - MKGUI_SLIDER_THUMB_SIZE) / range);
		draw_patch(ctx, MKGUI_STYLE_RAISED, thumb_x, ry + 2, MKGUI_SLIDER_THUMB_SIZE, rh - 4, thumb_color, thumb_border);
	}
}

// [=]===^=[ handle_slider_key ]=================================[=]
static uint32_t handle_slider_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks) {
	struct mkgui_slider_data *sd = find_slider_data(ctx, ctx->focus_id);
	if(!sd) {
		return 0;
	}

	struct mkgui_widget *fw = find_widget(ctx, ctx->focus_id);
	uint32_t vertical = (fw && (fw->flags & MKGUI_VERTICAL));

	int32_t range = sd->max_val - sd->min_val;
	int32_t step = range / 20;
	if(step < 1) {
		step = 1;
	}

	if(ks == MKGUI_KEY_LEFT || (vertical && ks == MKGUI_KEY_DOWN)) {
		sd->value -= step;
		if(sd->value < sd->min_val) {
			sd->value = sd->min_val;
		}

	} else if(ks == MKGUI_KEY_RIGHT || (vertical && ks == MKGUI_KEY_UP)) {
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

// [=]===^=[ mkgui_slider_set_meter ]=============================[=]
static void mkgui_slider_set_meter(struct mkgui_ctx *ctx, uint32_t id, float pre, float post, uint32_t pre_color, uint32_t post_color) {
	struct mkgui_slider_data *sd = find_slider_data(ctx, id);
	if(!sd) {
		return;
	}
	sd->meter_pre = pre;
	sd->meter_post = post;
	sd->meter_pre_color = pre_color;
	sd->meter_post_color = post_color;
	dirty_all(ctx);
}
