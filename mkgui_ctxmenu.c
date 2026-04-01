// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ ctxmenu_item_count_nonsep ]============================[=]
static uint32_t ctxmenu_item_count_nonsep(struct mkgui_ctx *ctx) {
	uint32_t count = 0;
	for(uint32_t i = 0; i < ctx->ctxmenu_count; ++i) {
		if(!(ctx->ctxmenu_items[i].flags & MKGUI_MENUITEM_SEPARATOR)) {
			++count;
		}
	}
	return count;
}

// [=]===^=[ ctxmenu_metrics ]=====================================[=]
static void ctxmenu_metrics(struct mkgui_ctx *ctx, int32_t *out_w, int32_t *out_h) {
	int32_t sep_h = sc(ctx, 10);
	int32_t pad = sc(ctx, 4);
	int32_t icon_w = sc(ctx, 20);
	int32_t rpad = sc(ctx, 8);
	int32_t min_w = sc(ctx, 120);
	int32_t max_w = min_w;
	int32_t total_h = 0;
	uint32_t has_icon = 0;

	for(uint32_t i = 0; i < ctx->ctxmenu_count; ++i) {
		struct mkgui_ctxmenu_item *it = &ctx->ctxmenu_items[i];
		if(it->flags & MKGUI_MENUITEM_SEPARATOR) {
			total_h += sep_h;
		} else {
			total_h += ctx->row_height;
			int32_t tw = text_width(ctx, it->label) + icon_w + pad * 2 + rpad;
			if(tw > max_w) {
				max_w = tw;
			}
			if(it->icon[0]) {
				has_icon = 1;
			}
		}
	}
	(void)has_icon;
	*out_w = max_w;
	*out_h = total_h + 2;
}

// [=]===^=[ ctxmenu_hit_item ]====================================[=]
static int32_t ctxmenu_hit_item(struct mkgui_ctx *ctx, int32_t local_y) {
	int32_t sep_h = sc(ctx, 10);
	int32_t iy = 1;
	int32_t idx = 0;
	for(uint32_t i = 0; i < ctx->ctxmenu_count; ++i) {
		struct mkgui_ctxmenu_item *it = &ctx->ctxmenu_items[i];
		int32_t rh = (it->flags & MKGUI_MENUITEM_SEPARATOR) ? sep_h : ctx->row_height;
		if(local_y >= iy && local_y < iy + rh) {
			if(it->flags & MKGUI_MENUITEM_SEPARATOR) {
				return -1;
			}
			return idx;
		}
		iy += rh;
		++idx;
	}
	return -1;
}

// [=]===^=[ ctxmenu_item_at ]=====================================[=]
static struct mkgui_ctxmenu_item *ctxmenu_item_at(struct mkgui_ctx *ctx, int32_t idx) {
	if(idx < 0 || idx >= (int32_t)ctx->ctxmenu_count) {
		return NULL;
	}
	return &ctx->ctxmenu_items[idx];
}

// [=]===^=[ ctxmenu_next_item ]===================================[=]
static int32_t ctxmenu_next_item(struct mkgui_ctx *ctx, int32_t current, int32_t dir) {
	if(ctx->ctxmenu_count == 0) {
		return -1;
	}
	int32_t n = (int32_t)ctx->ctxmenu_count;
	int32_t start = current + dir;
	for(int32_t i = 0; i < n; ++i) {
		int32_t idx = ((start + i * dir) % n + n) % n;
		if(!(ctx->ctxmenu_items[idx].flags & MKGUI_MENUITEM_SEPARATOR) && !(ctx->ctxmenu_items[idx].flags & MKGUI_DISABLED)) {
			return idx;
		}
	}
	return -1;
}

// [=]===^=[ render_ctxmenu_popup ]================================[=]
static void render_ctxmenu_popup(struct mkgui_ctx *ctx, struct mkgui_popup *p, int32_t hover_item) {
	int32_t saved_clip_x1 = render_clip_x1;
	int32_t saved_clip_y1 = render_clip_y1;
	int32_t saved_clip_x2 = render_clip_x2;
	int32_t saved_clip_y2 = render_clip_y2;
	render_clip_x1 = 0;
	render_clip_y1 = 0;
	render_clip_x2 = INT32_MAX;
	render_clip_y2 = INT32_MAX;

	draw_rounded_rect(p->pixels, p->w, p->h, 0, 0, p->w, p->h, ctx->theme.menu_bg, ctx->theme.widget_border, ctx->theme.corner_radius);

	int32_t sep_h = sc(ctx, 10);
	int32_t pad = sc(ctx, 4);
	int32_t icon_w = sc(ctx, 20);
	int32_t check_off = sc(ctx, 2);
	int32_t check_sz = sc(ctx, 9);
	int32_t check_half = sc(ctx, 4);
	int32_t radio_off = sc(ctx, 6);
	int32_t radio_r_out = sc(ctx, 5);
	int32_t radio_r_mid = sc(ctx, 4);
	int32_t radio_r_in = sc(ctx, 3);

	int32_t iy = 1;
	for(uint32_t i = 0; i < ctx->ctxmenu_count; ++i) {
		struct mkgui_ctxmenu_item *it = &ctx->ctxmenu_items[i];

		if(it->flags & MKGUI_MENUITEM_SEPARATOR) {
			draw_hline(p->pixels, p->w, p->h, pad + 1, iy + sep_h / 2, p->w - pad * 2 - 2, ctx->theme.widget_border);
			iy += sep_h;

		} else {
			uint32_t bg = ((int32_t)i == hover_item) ? ctx->theme.menu_hover : ctx->theme.menu_bg;
			uint32_t tc = (it->flags & MKGUI_DISABLED) ? ctx->theme.text_disabled : ctx->theme.text;

			if((int32_t)i == hover_item && !(it->flags & MKGUI_DISABLED)) {
				draw_rect_fill(p->pixels, p->w, p->h, pad, iy, p->w - pad * 2, ctx->row_height, bg);
			}

			int32_t ty = iy + (ctx->row_height - ctx->font_height) / 2;
			int32_t tx = icon_w + pad;
			push_text_clip(p->x + tx, ty + p->y, it->label, tc, p->x + 1, p->y + 1, p->x + p->w - 1, p->y + p->h - 1);

			int32_t icon_idx = -1;
			if(it->icon[0] && !(it->flags & (MKGUI_MENUITEM_CHECK | MKGUI_MENUITEM_RADIO))) {
				icon_idx = icon_resolve(it->icon);
			}

			if(icon_idx >= 0) {
				int32_t iy2 = iy + (ctx->row_height - icons[icon_idx].h) / 2;
				draw_icon_popup(p, &icons[icon_idx], pad, iy2);

			} else if(it->flags & MKGUI_MENUITEM_CHECK) {
				int32_t bx = pad + check_off;
				int32_t by = iy + ctx->row_height / 2 - check_half;
				draw_rect_border(p->pixels, p->w, p->h, bx, by, check_sz, check_sz, tc);
				if(it->flags & MKGUI_MENUITEM_CHECKED) {
					draw_pixel(p->pixels, p->w, p->h, bx + 2, by + 4, tc);
					draw_pixel(p->pixels, p->w, p->h, bx + 3, by + 5, tc);
					draw_pixel(p->pixels, p->w, p->h, bx + 4, by + 6, tc);
					draw_pixel(p->pixels, p->w, p->h, bx + 5, by + 5, tc);
					draw_pixel(p->pixels, p->w, p->h, bx + 6, by + 4, tc);
					draw_pixel(p->pixels, p->w, p->h, bx + 7, by + 3, tc);
					draw_pixel(p->pixels, p->w, p->h, bx + 3, by + 4, tc);
					draw_pixel(p->pixels, p->w, p->h, bx + 4, by + 5, tc);
					draw_pixel(p->pixels, p->w, p->h, bx + 5, by + 4, tc);
					draw_pixel(p->pixels, p->w, p->h, bx + 6, by + 3, tc);
					draw_pixel(p->pixels, p->w, p->h, bx + 7, by + 2, tc);
				}

			} else if(it->flags & MKGUI_MENUITEM_RADIO) {
				int32_t cx = pad + radio_off;
				int32_t cy = iy + ctx->row_height / 2;
				uint32_t rbg = ((int32_t)i == hover_item && !(it->flags & MKGUI_DISABLED)) ? bg : ctx->theme.menu_bg;
				draw_aa_circle_ring(p->pixels, p->w, p->h, cx, cy, radio_r_out, radio_r_mid, rbg, tc);
				if(it->flags & MKGUI_MENUITEM_CHECKED) {
					draw_aa_circle_fill(p->pixels, p->w, p->h, cx, cy, radio_r_in, tc);
				}
			}

			iy += ctx->row_height;
		}
	}

	render_clip_x1 = saved_clip_x1;
	render_clip_y1 = saved_clip_y1;
	render_clip_x2 = saved_clip_x2;
	render_clip_y2 = saved_clip_y2;
}

// [=]===^=[ mkgui_context_menu_clear ]=============================[=]
MKGUI_API void mkgui_context_menu_clear(struct mkgui_ctx *ctx) {
	MKGUI_CHECK(ctx);
	ctx->ctxmenu_count = 0;
}

// [=]===^=[ mkgui_context_menu_add ]===============================[=]
MKGUI_API void mkgui_context_menu_add(struct mkgui_ctx *ctx, uint32_t id, char *label, char *icon, uint32_t flags) {
	MKGUI_CHECK(ctx);
	if(!label && !(flags & MKGUI_MENUITEM_SEPARATOR)) {
		return;
	}
	if(ctx->ctxmenu_count >= MKGUI_MAX_CTXMENU) {
		return;
	}
	struct mkgui_ctxmenu_item *it = &ctx->ctxmenu_items[ctx->ctxmenu_count];
	memset(it, 0, sizeof(*it));
	it->id = id;
	it->flags = flags;
	if(label) {
		snprintf(it->label, MKGUI_MAX_TEXT, "%s", label);
	}
	if(icon) {
		snprintf(it->icon, MKGUI_ICON_NAME_LEN, "%s", icon);
	}
	++ctx->ctxmenu_count;
}

// [=]===^=[ mkgui_context_menu_add_separator ]=====================[=]
MKGUI_API void mkgui_context_menu_add_separator(struct mkgui_ctx *ctx) {
	MKGUI_CHECK(ctx);
	mkgui_context_menu_add(ctx, 0, NULL, NULL, MKGUI_MENUITEM_SEPARATOR);
}

// [=]===^=[ mkgui_context_menu_show ]==============================[=]
MKGUI_API void mkgui_context_menu_show(struct mkgui_ctx *ctx) {
	MKGUI_CHECK(ctx);
	if(ctx->ctxmenu_count == 0) {
		return;
	}

	popup_destroy_all(ctx);

	int32_t pw, ph;
	ctxmenu_metrics(ctx, &pw, &ph);

	int32_t abs_x = ctx->ctxmenu_x;
	int32_t abs_y = ctx->ctxmenu_y;

	platform_translate_coords(ctx, ctx->mouse_x, ctx->mouse_y, &abs_x, &abs_y);

	int32_t scr_w, scr_h;
	platform_screen_size(ctx, &scr_w, &scr_h);
	if(abs_x + pw > scr_w) {
		abs_x = scr_w - pw;
	}
	if(abs_y + ph > scr_h) {
		abs_y = abs_y - ph;
	}
	if(abs_x < 0) {
		abs_x = 0;
	}
	if(abs_y < 0) {
		abs_y = 0;
	}

	struct mkgui_popup *p = popup_create(ctx, abs_x, abs_y, pw, ph, MKGUI_CTXMENU_POPUP_ID);
	if(p) {
		render_ctxmenu_popup(ctx, p, -1);
		flush_text_popup(ctx, p);
		platform_popup_blit(ctx, p);
		platform_flush(ctx);
	}
}

// [=]===^=[ mkgui_context_menu_show_at ]============================[=]
MKGUI_API void mkgui_context_menu_show_at(struct mkgui_ctx *ctx, int32_t x, int32_t y) {
	MKGUI_CHECK(ctx);
	if(ctx->ctxmenu_count == 0) {
		return;
	}

	popup_destroy_all(ctx);

	int32_t pw, ph;
	ctxmenu_metrics(ctx, &pw, &ph);

	int32_t abs_x, abs_y;
	platform_translate_coords(ctx, x, y, &abs_x, &abs_y);

	int32_t scr_w, scr_h;
	platform_screen_size(ctx, &scr_w, &scr_h);
	if(abs_x + pw > scr_w) {
		abs_x = scr_w - pw;
	}
	if(abs_y + ph > scr_h) {
		abs_y = abs_y - ph;
	}
	if(abs_x < 0) {
		abs_x = 0;
	}
	if(abs_y < 0) {
		abs_y = 0;
	}

	struct mkgui_popup *p = popup_create(ctx, abs_x, abs_y, pw, ph, MKGUI_CTXMENU_POPUP_ID);
	if(p) {
		render_ctxmenu_popup(ctx, p, -1);
		flush_text_popup(ctx, p);
		platform_popup_blit(ctx, p);
		platform_flush(ctx);
	}
}
