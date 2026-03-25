// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define MKGUI_TOGGLE_TRACK_W 38
#define MKGUI_TOGGLE_TRACK_H 20
#define MKGUI_TOGGLE_KNOB_PAD 3
#define MKGUI_TOGGLE_KNOB_SIZE (MKGUI_TOGGLE_TRACK_H - MKGUI_TOGGLE_KNOB_PAD * 2)

// [=]===^=[ render_toggle ]=======================================[=]
static void render_toggle(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	struct mkgui_toggle_data *td = find_toggle_data(ctx, w->id);
	if(!td) {
		return;
	}

	uint32_t disabled = (w->flags & MKGUI_DISABLED);
	uint32_t focused = (ctx->focus_id == w->id);
	uint32_t hovered = (!disabled && ctx->hover_id == w->id);
	uint32_t on = td->state;

	int32_t ty = ry + (rh - MKGUI_TOGGLE_TRACK_H) / 2;
	int32_t track_r = MKGUI_TOGGLE_TRACK_H / 2;

	uint32_t track_fill;
	uint32_t track_border;
	if(on) {
		track_fill = disabled ? ctx->theme.widget_border : ctx->theme.accent;
		track_border = track_fill;
	} else {
		track_fill = ctx->theme.bg;
		track_border = (focused || hovered) ? ctx->theme.splitter : ctx->theme.widget_border;
	}
	draw_rounded_rect(ctx->pixels, ctx->win_w, ctx->win_h, rx, ty, MKGUI_TOGGLE_TRACK_W, MKGUI_TOGGLE_TRACK_H, track_fill, track_border, track_r);

	int32_t knob_x;
	if(on) {
		knob_x = rx + MKGUI_TOGGLE_TRACK_W - MKGUI_TOGGLE_KNOB_PAD - MKGUI_TOGGLE_KNOB_SIZE;
	} else {
		knob_x = rx + MKGUI_TOGGLE_KNOB_PAD;
	}
	int32_t knob_y = ty + MKGUI_TOGGLE_KNOB_PAD;
	int32_t knob_r = MKGUI_TOGGLE_KNOB_SIZE / 2;
	uint32_t knob_color = on ? 0xffffffff : (disabled ? ctx->theme.text_disabled : ctx->theme.widget_border);
	draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, knob_x, knob_y, MKGUI_TOGGLE_KNOB_SIZE, MKGUI_TOGGLE_KNOB_SIZE, knob_color, knob_r);

	if(w->label[0]) {
		int32_t lx = rx + MKGUI_TOGGLE_TRACK_W + 6;
		int32_t ly = ry + (rh - ctx->font_height) / 2;
		uint32_t tc = disabled ? ctx->theme.text_disabled : ctx->theme.text;
		push_text_clip(lx, ly, w->label, tc, rx, ry, rx + rw, ry + rh);
	}
}

// [=]===^=[ handle_toggle_click ]=================================[=]
static void handle_toggle_click(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t widget_id) {
	struct mkgui_toggle_data *td = find_toggle_data(ctx, widget_id);
	if(!td) {
		return;
	}
	td->state = td->state ? 0 : 1;
	ev->type = MKGUI_EVENT_TOGGLE_CHANGED;
	ev->id = widget_id;
	ev->value = (int32_t)td->state;
	dirty_all(ctx);
}

// [=]===^=[ handle_toggle_key ]===================================[=]
static uint32_t handle_toggle_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks) {
	if(ks == MKGUI_KEY_SPACE || ks == MKGUI_KEY_RETURN) {
		struct mkgui_toggle_data *td = find_toggle_data(ctx, ctx->focus_id);
		if(!td) {
			return 0;
		}
		td->state = td->state ? 0 : 1;
		ev->type = MKGUI_EVENT_TOGGLE_CHANGED;
		ev->id = ctx->focus_id;
		ev->value = (int32_t)td->state;
		dirty_all(ctx);
		return 1;
	}
	return 0;
}

// [=]===^=[ mkgui_toggle_set ]====================================[=]
MKGUI_API void mkgui_toggle_set(struct mkgui_ctx *ctx, uint32_t id, uint32_t state) {
	MKGUI_CHECK(ctx);
	struct mkgui_toggle_data *td = find_toggle_data(ctx, id);
	if(td) {
		td->state = state ? 1 : 0;
		dirty_all(ctx);
	}
}

// [=]===^=[ mkgui_toggle_get ]====================================[=]
MKGUI_API uint32_t mkgui_toggle_get(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_toggle_data *td = find_toggle_data(ctx, id);
	return td ? td->state : 0;
}
