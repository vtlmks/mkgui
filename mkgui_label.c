// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ render_label ]======================================[=]
static void render_label(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	uint32_t tc = (w->flags & MKGUI_DISABLED) ? ctx->theme.text_disabled : ctx->theme.text;
	int32_t ty = ry + (rh - ctx->font_height) / 2;
	int32_t tx = rx;
	uint32_t align = w->flags & MKGUI_ALIGN_MASK;
	if(align == MKGUI_ALIGN_CENTER) {
		int32_t tw = text_width(ctx, w->label);
		tx = rx + (rw - tw) / 2;
	} else if(align == MKGUI_ALIGN_END) {
		int32_t tw = text_width(ctx, w->label);
		tx = rx + rw - tw;
	}
	push_text_clip(tx, ty, w->label, tc, rx, ry, rx + rw, ry + rh);
}

// [=]===^=[ mkgui_label_set ]===================================[=]
MKGUI_API void mkgui_label_set(struct mkgui_ctx *ctx, uint32_t id, const char *text) {
	struct mkgui_widget *w = find_widget(ctx, id);
	if(!w) {
		return;
	}
	strncpy(w->label, text, MKGUI_MAX_TEXT - 1);
	w->label[MKGUI_MAX_TEXT - 1] = '\0';
	dirty_all(ctx);
}

// [=]===^=[ mkgui_label_get ]===================================[=]
MKGUI_API const char *mkgui_label_get(struct mkgui_ctx *ctx, uint32_t id) {
	struct mkgui_widget *w = find_widget(ctx, id);
	return w ? w->label : "";
}
