// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ render_statusbar ]===================================[=]
static void render_statusbar(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx, ry, rw, rh, ctx->theme.header_bg);
	draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, rx, ry, rw, ctx->theme.widget_border);

	int32_t text_pad = sc(ctx, 4);
	int32_t inset2 = sc(ctx, 2);

	struct mkgui_statusbar_data *sb = find_statusbar_data(ctx, w->id);
	if(!sb || sb->section_count == 0) {
		int32_t ty = ry + (rh - ctx->font_height) / 2;
		push_text_clip(rx + text_pad, ty, w->label, ctx->theme.text, rx, ry, rx + rw, ry + rh);
		return;
	}

	int32_t sx = rx + 1;
	for(uint32_t i = 0; i < sb->section_count; ++i) {
		int32_t sw = sb->sections[i].width;
		if(sw <= 0) {
			int32_t used = 0;
			for(uint32_t j = 0; j < sb->section_count; ++j) {
				if(sb->sections[j].width > 0) {
					used += sb->sections[j].width;
				}
			}
			sw = rw - used - 2;
			if(sw < 0) {
				sw = 0;
			}
		}

		int32_t ty = ry + (rh - ctx->font_height) / 2;
		push_text_clip(sx + text_pad, ty, sb->sections[i].text, ctx->theme.text, sx, ry, sx + sw, ry + rh);

		sx += sw;
		if(i < sb->section_count - 1) {
			draw_vline(ctx->pixels, ctx->win_w, ctx->win_h, sx, ry + inset2, rh - inset2 * 2, ctx->theme.widget_border);
			sx += 1;
		}
	}
}

// [=]===^=[ mkgui_statusbar_setup ]==============================[=]
MKGUI_API void mkgui_statusbar_setup(struct mkgui_ctx *ctx, uint32_t id, uint32_t section_count, const int32_t *widths) {
	MKGUI_CHECK(ctx);
	struct mkgui_statusbar_data *sb = find_statusbar_data(ctx, id);
	if(!sb) {
		return;
	}
	sb->section_count = section_count > MKGUI_MAX_STATUSBAR_SECTIONS ? MKGUI_MAX_STATUSBAR_SECTIONS : section_count;
	for(uint32_t i = 0; i < sb->section_count; ++i) {
		sb->sections[i].width = widths[i] > 0 ? sc(ctx, widths[i]) : widths[i];
		sb->sections[i].text[0] = '\0';
	}
	dirty_all(ctx);
}

// [=]===^=[ mkgui_statusbar_set ]================================[=]
MKGUI_API void mkgui_statusbar_set(struct mkgui_ctx *ctx, uint32_t id, uint32_t section, const char *text) {
	MKGUI_CHECK(ctx);
	if(!text) {
		text = "";
	}
	struct mkgui_statusbar_data *sb = find_statusbar_data(ctx, id);
	if(!sb || section >= sb->section_count) {
		return;
	}
	size_t slen = strlen(text);
	if(slen >= MKGUI_MAX_TEXT) {
		slen = MKGUI_MAX_TEXT - 1;
	}
	memcpy(sb->sections[section].text, text, slen);
	sb->sections[section].text[slen] = '\0';
	dirty_all(ctx);
}

// [=]===^=[ mkgui_statusbar_get ]=================================[=]
MKGUI_API const char *mkgui_statusbar_get(struct mkgui_ctx *ctx, uint32_t id, uint32_t section) {
	MKGUI_CHECK_VAL(ctx, "");
	struct mkgui_statusbar_data *sb = find_statusbar_data(ctx, id);
	if(!sb || section >= sb->section_count) {
		return "";
	}
	return sb->sections[section].text;
}

// [=]===^=[ mkgui_statusbar_clear ]================================[=]
MKGUI_API void mkgui_statusbar_clear(struct mkgui_ctx *ctx, uint32_t id, uint32_t section) {
	MKGUI_CHECK(ctx);
	struct mkgui_statusbar_data *sb = find_statusbar_data(ctx, id);
	if(!sb || section >= sb->section_count) {
		return;
	}
	sb->sections[section].text[0] = '\0';
	dirty_all(ctx);
}
