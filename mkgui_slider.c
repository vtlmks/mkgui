// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define MKGUI_SLIDER_TRACK_SIZE       4
#define MKGUI_SLIDER_METER_TRACK_SIZE 8
#define MKGUI_SLIDER_THUMB_SIZE       10

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
	uint32_t has_meter = (sd->meter_pre_color != 0 || sd->meter_post_color != 0);

	int32_t range = sd->max_val - sd->min_val;
	if(range <= 0) {
		range = 1;
	}

	uint32_t thumb_color = disabled ? ctx->theme.widget_border : ((ctx->focus_id == w->id) ? ctx->theme.sel_text : ctx->theme.splitter);
	uint32_t thumb_border = disabled ? ctx->theme.widget_border : ctx->theme.splitter;

	if(vertical) {
		int32_t track_w = MKGUI_SLIDER_TRACK_SIZE;
		int32_t track_x = rx + rw / 2 - track_w / 2;

		if(has_meter) {
			track_w = MKGUI_SLIDER_METER_TRACK_SIZE;
			track_x = rx + rw / 2 - track_w / 2;
			draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, track_x, ry, track_w, rh, ctx->theme.widget_border, 2);

			int32_t meter_x = track_x + 1;
			int32_t meter_y = ry + 1;
			int32_t meter_w = track_w - 2;
			int32_t meter_h = rh - 2;

			if(sd->meter_pre_color != 0 && sd->meter_pre > 0.0f) {
				float pre = sd->meter_pre;
				if(pre > 1.0f) {
					pre = 1.0f;
				}
				int32_t ph = (int32_t)(pre * (float)meter_h);
				if(ph > 0) {
					draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, meter_x, meter_y + meter_h - ph, meter_w, ph, sd->meter_pre_color);
				}
			}

			if(sd->meter_post_color != 0 && sd->meter_post > 0.0f) {
				float post = sd->meter_post;
				if(post > 1.0f) {
					post = 1.0f;
				}
				int32_t ph = (int32_t)(post * (float)meter_h);
				if(ph > 0) {
					draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, meter_x, meter_y + meter_h - ph, meter_w, ph, sd->meter_post_color);
				}
			}

		} else {
			draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, track_x, ry, track_w, rh, ctx->theme.widget_border, 2);
		}

		int32_t thumb_y = ry + rh - MKGUI_SLIDER_THUMB_SIZE - (int32_t)((int64_t)(sd->value - sd->min_val) * (rh - MKGUI_SLIDER_THUMB_SIZE) / range);
		draw_patch(ctx, MKGUI_STYLE_RAISED, rx + 2, thumb_y, rw - 4, MKGUI_SLIDER_THUMB_SIZE, thumb_color, thumb_border);

	} else {
		int32_t track_y = ry + rh / 2 - MKGUI_SLIDER_TRACK_SIZE / 2;
		int32_t track_h = MKGUI_SLIDER_TRACK_SIZE;

		if(has_meter) {
			track_h = MKGUI_SLIDER_METER_TRACK_SIZE;
			track_y = ry + rh / 2 - track_h / 2;
			draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx, track_y, rw, track_h, ctx->theme.widget_border, 2);

			int32_t meter_w = rw - 2;
			int32_t meter_x = rx + 1;
			int32_t meter_y = track_y + 1;
			int32_t meter_h = track_h - 2;

			if(sd->meter_pre_color != 0 && sd->meter_pre > 0.0f) {
				float pre = sd->meter_pre;
				if(pre > 1.0f) {
					pre = 1.0f;
				}
				int32_t pw = (int32_t)(pre * (float)meter_w);
				if(pw > 0) {
					draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, meter_x, meter_y, pw, meter_h, sd->meter_pre_color);
				}
			}

			if(sd->meter_post_color != 0 && sd->meter_post > 0.0f) {
				float post = sd->meter_post;
				if(post > 1.0f) {
					post = 1.0f;
				}
				int32_t pw = (int32_t)(post * (float)meter_w);
				if(pw > 0) {
					draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, meter_x, meter_y, pw, meter_h, sd->meter_post_color);
				}
			}

		} else {
			draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx, track_y, rw, track_h, ctx->theme.widget_border, 2);
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
