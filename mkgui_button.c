// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ render_button ]=====================================[=]
static void render_button(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	uint32_t checked = (w->style & MKGUI_BUTTON_CHECKED);
	uint32_t bg = checked ? ctx->theme.widget_press : ctx->theme.widget_bg;
	uint32_t style = checked ? MKGUI_STYLE_SUNKEN : MKGUI_STYLE_RAISED;
	if(ctx->press_id == w->id) {
		bg = ctx->theme.widget_press;

	} else if(ctx->hover_id == w->id) {
		bg = ctx->theme.widget_hover;
	}
	uint32_t border = (ctx->focus_id == w->id) ? ctx->theme.highlight : ctx->theme.widget_border;
	draw_patch(ctx, style, rx, ry, rw, rh, bg, border);

	uint32_t tc = (w->flags & MKGUI_DISABLED) ? ctx->theme.text_disabled : ctx->theme.text;
	int32_t ii = widget_icon_idx(w);
	uint32_t has_icon = (ii >= 0);
	int32_t tw = label_text_width(ctx, w);
	int32_t icon_w = has_icon ? (icons[ii].w + (w->label[0] ? sc(ctx, 4) : 0)) : 0;
	int32_t content_w = icon_w + (w->label[0] ? tw : 0);
	int32_t cx = rx + (rw - content_w) / 2;
	if(has_icon) {
		int32_t iy = ry + (rh - icons[ii].h) / 2;
		draw_icon(ctx->pixels, ctx->win_w, ctx->win_h, &icons[ii], cx, iy, rx + 1, ry + 1, rx + rw - 1, ry + rh - 1);
		cx += icons[ii].w + sc(ctx, 4);
	}
	if(w->label[0]) {
		int32_t ty = ry + (rh - ctx->font_height) / 2;
		push_text_clip(cx, ty, w->label, tc, rx, ry, rx + rw, ry + rh);
	}
}

// [=]===^=[ mkgui_button_set_text ]================================[=]
MKGUI_API void mkgui_button_set_text(struct mkgui_ctx *ctx, uint32_t id, char *text) {
	MKGUI_CHECK(ctx);
	struct mkgui_widget *w = find_widget(ctx, id);
	if(!w) {
		return;
	}
	if(!text) {
		text = "";
	}
	strncpy(w->label, text, MKGUI_MAX_TEXT - 1);
	w->label[MKGUI_MAX_TEXT - 1] = '\0';
	w->label_tw = -1;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_button_get_text ]================================[=]
MKGUI_API char *mkgui_button_get_text(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, "");
	struct mkgui_widget *w = find_widget(ctx, id);
	return w ? w->label : "";
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
