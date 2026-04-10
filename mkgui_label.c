// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ render_label_wrap ]=================================[=]
static void render_label_wrap(struct mkgui_ctx *ctx, uint32_t idx, uint32_t tc) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;
	int32_t line_h = ctx->font_height + sc(ctx, 2);
	int32_t cy = ry;
	char *p = w->label;
	char line_buf[MKGUI_MAX_TEXT];

	while(*p && cy + line_h <= ry + rh) {
		uint32_t best = 0;
		uint32_t i = 0;
		while(p[i]) {
			char tmp[MKGUI_MAX_TEXT];
			uint32_t n = i + 1;
			if(n >= MKGUI_MAX_TEXT - 1) {
				n = MKGUI_MAX_TEXT - 1;
			}
			memcpy(tmp, p, n);
			tmp[n] = '\0';
			if(text_width(ctx, tmp) > rw && best > 0) {
				break;
			}

			if(p[i] == ' ' || p[i] == '\n') {
				best = i + 1;
				if(p[i] == '\n') {
					break;
				}
			}
			++i;
		}

		if(!p[i]) {
			best = i;
		}

		if(best == 0) {
			best = i > 0 ? i : 1;
		}
		uint32_t len = best;
		while(len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\n')) {
			--len;
		}
		memcpy(line_buf, p, len);
		line_buf[len] = '\0';
		push_text_clip(rx, cy, line_buf, tc, rx, ry, rx + rw, ry + rh);
		cy += line_h;
		p += best;
	}
}

// [=]===^=[ render_label ]======================================[=]
static void render_label(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	uint32_t is_link = (w->style & MKGUI_LABEL_LINK);
	uint32_t tc;
	if(w->flags & MKGUI_DISABLED) {
		tc = ctx->theme.text_disabled;
	} else if(is_link) {
		tc = (ctx->hover_id == w->id) ? ctx->theme.highlight : ctx->theme.selection;
	} else {
		tc = ctx->theme.text;
	}

	if(w->style & MKGUI_LABEL_WRAP) {
		render_label_wrap(ctx, idx, tc);
		return;
	}
	int32_t ty = ry + (rh - ctx->font_height) / 2;
	int32_t tx = rx;
	char *text = w->label;
	if(w->style & MKGUI_LABEL_TRUNCATE) {
		text = text_truncate(ctx, text, rw);
	}
	uint32_t align = w->flags & MKGUI_ALIGN_MASK;
	if(align == MKGUI_ALIGN_CENTER) {
		int32_t tw = text_width(ctx, text);
		tx = rx + (rw - tw) / 2;
	} else if(align == MKGUI_ALIGN_END) {
		int32_t tw = text_width(ctx, text);
		tx = rx + rw - tw;
	}
	push_text_clip(tx, ty, text, tc, rx, ry, rx + rw, ry + rh);
	if(is_link && !(w->flags & MKGUI_DISABLED)) {
		int32_t tw = text_width(ctx, text);
		int32_t uy = ty + ctx->font_height;
		int32_t ux2 = tx + tw;
		if(ux2 > rx + rw) {
			ux2 = rx + rw;
		}

		if(ux2 > tx) {
			draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, tx, uy, ux2 - tx, tc);
		}
	}
}

// [=]===^=[ mkgui_label_set ]===================================[=]
MKGUI_API void mkgui_label_set(struct mkgui_ctx *ctx, uint32_t id, const char *text) {
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
	dirty_widget_id(ctx, id);
}

// [=]===^=[ mkgui_label_get ]===================================[=]
MKGUI_API char *mkgui_label_get(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, "");
	struct mkgui_widget *w = find_widget(ctx, id);
	return w ? w->label : "";
}
