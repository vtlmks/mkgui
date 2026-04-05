// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ menu_item_has_children ]=============================[=]
static uint32_t menu_item_has_children(struct mkgui_ctx *ctx, uint32_t item_id) {
	uint32_t pidx = layout_find_idx(item_id);
	if(pidx == UINT32_MAX) {
		return 0;
	}
	for(uint32_t c = layout_first_child[pidx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
		if(ctx->widgets[c].type == MKGUI_MENUITEM) {
			return 1;
		}
	}
	return 0;
}

// [=]===^=[ menu_popup_nth_item ]================================[=]
static struct mkgui_widget *menu_popup_nth_item(struct mkgui_ctx *ctx, uint32_t parent_id, int32_t n) {
	uint32_t pidx = layout_find_idx(parent_id);
	if(pidx == UINT32_MAX) {
		return NULL;
	}
	int32_t count = 0;
	for(uint32_t c = layout_first_child[pidx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
		struct mkgui_widget *mi = &ctx->widgets[c];
		if(mi->type != MKGUI_MENUITEM) {
			continue;
		}
		if(count == n) {
			return mi;
		}
		++count;
	}
	return NULL;
}

// [=]===^=[ menu_popup_hit_item ]================================[=]
static struct mkgui_widget *menu_popup_hit_item(struct mkgui_ctx *ctx, uint32_t parent_id, int32_t local_y, int32_t *out_idx) {
	int32_t sep_h = sc(ctx, 10);
	uint32_t pidx = layout_find_idx(parent_id);
	if(pidx == UINT32_MAX) {
		*out_idx = -1;
		return NULL;
	}
	int32_t iy = 1;
	int32_t idx = 0;
	for(uint32_t c = layout_first_child[pidx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
		struct mkgui_widget *mi = &ctx->widgets[c];
		if(mi->type != MKGUI_MENUITEM) {
			continue;
		}
		int32_t rh = (mi->style & MKGUI_MENUITEM_SEPARATOR) ? sep_h : ctx->row_height;
		if(local_y >= iy && local_y < iy + rh) {
			*out_idx = idx;
			return mi;
		}
		iy += rh;
		++idx;
	}
	*out_idx = -1;
	return NULL;
}

// [=]===^=[ menu_popup_child_metrics ]============================[=]
static int32_t menu_popup_child_metrics(struct mkgui_ctx *ctx, uint32_t parent_id, int32_t *max_w, int32_t *content_h) {
	int32_t sep_h = sc(ctx, 10);
	int32_t icon_col = sc(ctx, 20);
	int32_t text_rpad = sc(ctx, 20);
	int32_t submenu_w = sc(ctx, 12);
	int32_t min_w = sc(ctx, 120);
	uint32_t pidx = layout_find_idx(parent_id);
	if(pidx == UINT32_MAX) {
		*max_w = min_w;
		*content_h = 0;
		return 0;
	}
	int32_t count = 0;
	*max_w = min_w;
	*content_h = 0;
	for(uint32_t c = layout_first_child[pidx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
		struct mkgui_widget *sub = &ctx->widgets[c];
		if(sub->type != MKGUI_MENUITEM) {
			continue;
		}
		++count;
		if(sub->style & MKGUI_MENUITEM_SEPARATOR) {
			*content_h += sep_h;
		} else {
			*content_h += ctx->row_height;
			int32_t tw = label_text_width(ctx, sub) + icon_col + text_rpad;
			if(menu_item_has_children(ctx, sub->id)) {
				tw += submenu_w;
			}
			struct mkgui_accel *acl = accel_find_for_widget(ctx, sub->id);
			if(acl) {
				char accel_buf[64];
				accel_format_text(acl->keymod, acl->keysym, accel_buf, sizeof(accel_buf));
				tw += text_width(ctx, accel_buf) + sc(ctx, 16);
			}
			if(tw > *max_w) {
				*max_w = tw;
			}
		}
	}
	return count;
}

// [=]===^=[ menu_popup_item_y ]===================================[=]
static int32_t menu_popup_item_y(struct mkgui_ctx *ctx, uint32_t parent_id, int32_t target_idx) {
	int32_t sep_h = sc(ctx, 10);
	uint32_t pidx = layout_find_idx(parent_id);
	if(pidx == UINT32_MAX) {
		return 1;
	}
	int32_t iy = 1;
	int32_t idx = 0;
	for(uint32_t c = layout_first_child[pidx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
		struct mkgui_widget *mi = &ctx->widgets[c];
		if(mi->type != MKGUI_MENUITEM) {
			continue;
		}
		if(idx == target_idx) {
			return iy;
		}
		iy += (mi->style & MKGUI_MENUITEM_SEPARATOR) ? sep_h : ctx->row_height;
		++idx;
	}
	return iy;
}

// [=]===^=[ render_menu_bar ]===================================[=]
static void render_menu_bar(struct mkgui_ctx *ctx, uint32_t idx) {
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx, ry, rw, rh, ctx->theme.menu_bg);

	int32_t menu_pad = sc(ctx, 8);
	int32_t mx = rx;
	for(uint32_t c = layout_first_child[idx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
		struct mkgui_widget *mi = &ctx->widgets[c];
		if(mi->type != MKGUI_MENUITEM) {
			continue;
		}
		int32_t iw = label_text_width(ctx, mi) + menu_pad * 2;
		uint32_t bg = (ctx->hover_id == mi->id) ? ctx->theme.highlight : ctx->theme.menu_bg;
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, mx, ry, iw, rh, bg);
		int32_t ty = ry + (rh - ctx->font_height) / 2;
		push_text_clip(mx + menu_pad, ty, mi->label, ctx->theme.text, mx, ry, mx + iw, ry + rh);

		ctx->rects[c].x = mx;
		ctx->rects[c].y = ry;
		ctx->rects[c].w = iw;
		ctx->rects[c].h = rh;

		mx += iw;
	}
}

// [=]===^=[ render_menu_popup ]=================================[=]
static void render_menu_popup(struct mkgui_ctx *ctx, struct mkgui_popup *p, uint32_t menu_id, int32_t hover_item) {
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
	int32_t icon_col = sc(ctx, 20);
	int32_t icon_off = sc(ctx, 2);
	int32_t check_x = sc(ctx, 4);
	int32_t check_sz = sc(ctx, 9);
	int32_t check_half = sc(ctx, 4);
	int32_t radio_cx = sc(ctx, 8);
	int32_t radio_r_out = sc(ctx, 5);
	int32_t radio_r_mid = sc(ctx, 4);
	int32_t radio_r_in = sc(ctx, 3);
	int32_t submenu_off = sc(ctx, 12);

	uint32_t pidx = layout_find_idx(menu_id);
	int32_t iy = 1;
	int32_t item_idx = 0;
	if(pidx == UINT32_MAX) {
		goto menu_popup_done;
	}
	for(uint32_t c = layout_first_child[pidx]; c < ctx->widget_count; c = layout_next_sibling[c]) {
		struct mkgui_widget *mi = &ctx->widgets[c];
		if(mi->type != MKGUI_MENUITEM) {
			continue;
		}

		if(mi->style & MKGUI_MENUITEM_SEPARATOR) {
			draw_hline(p->pixels, p->w, p->h, 1, iy + sep_h / 2, p->w - 2, ctx->theme.widget_border);
			iy += sep_h;

		} else {
			uint32_t bg = (item_idx == hover_item) ? ctx->theme.highlight : ctx->theme.menu_bg;
			draw_rect_fill(p->pixels, p->w, p->h, 1, iy, p->w - 2, ctx->row_height, bg);
			int32_t ty = iy + (ctx->row_height - ctx->font_height) / 2;
			push_text_clip(p->x + icon_col, ty + p->y, mi->label, ctx->theme.text, p->x + 1, p->y + 1, p->x + p->w - 1, p->y + p->h - 1);

			int32_t mi_icon = widget_icon_idx(mi);
			if(mi_icon >= 0 && !(mi->style & (MKGUI_MENUITEM_CHECK | MKGUI_MENUITEM_RADIO))) {
				int32_t iy2 = iy + (ctx->row_height - icons[mi_icon].h) / 2;
				draw_icon_popup(p, &icons[mi_icon], icon_off, iy2);

			} else if(mi->style & MKGUI_MENUITEM_CHECK) {
				int32_t bx = check_x;
				int32_t by = iy + ctx->row_height / 2 - check_half;
				draw_rect_border(p->pixels, p->w, p->h, bx, by, check_sz, check_sz, ctx->theme.text);
				if(mi->style & MKGUI_MENUITEM_CHECKED) {
					int32_t m = check_sz / 5;
					int32_t lw = m > 0 ? m : 1;
					int32_t x0 = bx + m;
					int32_t y0 = by + check_sz / 2;
					int32_t x1 = bx + check_sz * 2 / 5;
					int32_t y1 = by + check_sz - m - 1;
					int32_t x2 = bx + check_sz - m;
					int32_t y2 = by + m;
					draw_aa_line(p->pixels, p->w, p->h, x0, y0, x1, y1, ctx->theme.text, lw);
					draw_aa_line(p->pixels, p->w, p->h, x1, y1, x2, y2, ctx->theme.text, lw);
				}

			} else if(mi->style & MKGUI_MENUITEM_RADIO) {
				int32_t cx = radio_cx;
				int32_t cy = iy + ctx->row_height / 2;
				draw_aa_circle_ring(p->pixels, p->w, p->h, cx, cy, radio_r_out, radio_r_mid, bg, ctx->theme.text);
				if(mi->style & MKGUI_MENUITEM_CHECKED) {
					draw_aa_circle_fill(p->pixels, p->w, p->h, cx, cy, radio_r_in, ctx->theme.text);
				}
			}

			struct mkgui_accel *acl = accel_find_for_widget(ctx, mi->id);
			if(acl) {
				char accel_buf[64];
				accel_format_text(acl->keymod, acl->keysym, accel_buf, sizeof(accel_buf));
				int32_t atw = text_width(ctx, accel_buf);
				int32_t ax = p->w - atw - sc(ctx, 8);
				push_text_clip(p->x + ax, ty + p->y, accel_buf, ctx->theme.text_disabled, p->x + 1, p->y + 1, p->x + p->w - 1, p->y + p->h - 1);
			}

			if(menu_item_has_children(ctx, mi->id)) {
				int32_t ax = p->w - submenu_off;
				int32_t ay = iy + ctx->row_height / 2;
				for(uint32_t j = 0; j < 4; ++j) {
					draw_vline(p->pixels, p->w, p->h, ax + (int32_t)j, ay - 3 + (int32_t)j, 7 - (int32_t)j * 2, ctx->theme.text);
				}
			}

			iy += ctx->row_height;
		}

		++item_idx;
	}

menu_popup_done:
	render_clip_x1 = saved_clip_x1;
	render_clip_y1 = saved_clip_y1;
	render_clip_x2 = saved_clip_x2;
	render_clip_y2 = saved_clip_y2;
}

// [=]===^=[ menu_open_popup ]===================================[=]
static void menu_open_popup(struct mkgui_ctx *ctx, uint32_t menuitem_id) {
	int32_t max_w, content_h;
	int32_t sub_count = menu_popup_child_metrics(ctx, menuitem_id, &max_w, &content_h);
	if(sub_count == 0) {
		return;
	}

	int32_t mx_idx = find_widget_idx(ctx, menuitem_id);
	if(mx_idx < 0) {
		return;
	}

	int32_t abs_x, abs_y;
	platform_translate_coords(ctx, ctx->rects[mx_idx].x, ctx->rects[mx_idx].y + ctx->rects[mx_idx].h, &abs_x, &abs_y);

	int32_t ph = content_h + 2;

	int32_t scr_w, scr_h;
	platform_screen_size(ctx, &scr_w, &scr_h);
	if(abs_x + max_w > scr_w) {
		abs_x = scr_w - max_w;
	}
	if(abs_y + ph > scr_h) {
		abs_y = scr_h - ph;
	}

	popup_destroy_all(ctx);
	struct mkgui_popup *p = popup_create(ctx, abs_x, abs_y, max_w, ph, menuitem_id);
	if(p) {
		render_menu_popup(ctx, p, menuitem_id, -1);
		flush_text_popup(ctx, p);
		platform_popup_blit(ctx, p);
		platform_flush(ctx);
	}
}

// [=]===^=[ menu_open_submenu ]=================================[=]
static void menu_open_submenu(struct mkgui_ctx *ctx, uint32_t popup_idx, uint32_t submenu_item_id) {
	int32_t max_w, content_h;
	int32_t sub_count = menu_popup_child_metrics(ctx, submenu_item_id, &max_w, &content_h);
	if(sub_count == 0) {
		return;
	}

	popup_destroy_from(ctx, popup_idx + 1);

	struct mkgui_popup *parent = &ctx->popups[popup_idx];
	int32_t hover = parent->hover_item;
	int32_t ph = content_h + 2;

	int32_t item_y = menu_popup_item_y(ctx, parent->widget_id, hover);
	int32_t sub_x = parent->x + parent->w - 2;
	int32_t sub_y = parent->y + item_y;

	int32_t scr_w, scr_h;
	platform_screen_size(ctx, &scr_w, &scr_h);

	if(sub_x + max_w > scr_w) {
		sub_x = parent->x - max_w + 2;
	}
	if(sub_y + ph > scr_h) {
		sub_y = scr_h - ph;
	}
	if(sub_y < 0) {
		sub_y = 0;
	}

	struct mkgui_popup *p = popup_create(ctx, sub_x, sub_y, max_w, ph, submenu_item_id);
	if(p) {
		render_menu_popup(ctx, p, submenu_item_id, -1);
		flush_text_popup(ctx, p);
		platform_popup_blit(ctx, p);
		platform_flush(ctx);
	}
}
