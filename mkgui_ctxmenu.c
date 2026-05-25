// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ ctxmenu_item_count_nonsep ]============================[=]
static uint32_t ctxmenu_item_count_nonsep(struct mkgui_window *win) {
	uint32_t count = 0;
	for(uint32_t i = 0; i < win->ctxmenu_count; ++i) {
		if(!(win->ctxmenu_items[i].flags & MKGUI_MENUITEM_SEPARATOR)) {
			++count;
		}
	}
	return count;
}

// [=]===^=[ ctxmenu_metrics ]=====================================[=]
static void ctxmenu_metrics(struct mkgui_window *win, int32_t *out_w, int32_t *out_h) {
	int32_t sep_h = sc(win, 10);
	int32_t pad = sc(win, 4);
	int32_t icon_w = sc(win, 20);
	int32_t rpad = sc(win, 8);
	int32_t min_w = sc(win, 120);
	int32_t max_w = min_w;
	int32_t total_h = 0;
	uint32_t has_icon = 0;

	for(uint32_t i = 0; i < win->ctxmenu_count; ++i) {
		struct mkgui_ctxmenu_item *it = &win->ctxmenu_items[i];
		if(it->flags & MKGUI_MENUITEM_SEPARATOR) {
			total_h += sep_h;
		} else {
			total_h += win->row_height;
			int32_t tw = text_width(win, it->label) + icon_w + pad * 2 + rpad;
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
static int32_t ctxmenu_hit_item(struct mkgui_window *win, int32_t local_y) {
	int32_t sep_h = sc(win, 10);
	int32_t iy = 1;
	int32_t idx = 0;
	for(uint32_t i = 0; i < win->ctxmenu_count; ++i) {
		struct mkgui_ctxmenu_item *it = &win->ctxmenu_items[i];
		int32_t rh = (it->flags & MKGUI_MENUITEM_SEPARATOR) ? sep_h : win->row_height;
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
static struct mkgui_ctxmenu_item *ctxmenu_item_at(struct mkgui_window *win, int32_t idx) {
	if(idx < 0 || idx >= (int32_t)win->ctxmenu_count) {
		return NULL;
	}
	return &win->ctxmenu_items[idx];
}

// [=]===^=[ ctxmenu_next_item ]===================================[=]
static int32_t ctxmenu_next_item(struct mkgui_window *win, int32_t current, int32_t dir) {
	if(win->ctxmenu_count == 0) {
		return -1;
	}
	int32_t n = (int32_t)win->ctxmenu_count;
	int32_t start = current + dir;
	for(int32_t i = 0; i < n; ++i) {
		int32_t idx = ((start + i * dir) % n + n) % n;
		if(!(win->ctxmenu_items[idx].flags & MKGUI_MENUITEM_SEPARATOR) && !(win->ctxmenu_items[idx].flags & MKGUI_DISABLED)) {
			return idx;
		}
	}
	return -1;
}

// [=]===^=[ render_ctxmenu_popup ]================================[=]
static void render_ctxmenu_popup(struct mkgui_window *win, struct mkgui_popup *p, int32_t hover_item) {
	int32_t saved_clip_x1 = render_clip_x1;
	int32_t saved_clip_y1 = render_clip_y1;
	int32_t saved_clip_x2 = render_clip_x2;
	int32_t saved_clip_y2 = render_clip_y2;
	render_clip_x1 = 0;
	render_clip_y1 = 0;
	render_clip_x2 = INT32_MAX;
	render_clip_y2 = INT32_MAX;

	draw_rounded_rect(p->pixels, p->w, p->h, 0, 0, p->w, p->h, win->theme.menu_bg, win->theme.widget_border, win->theme.corner_radius);

	int32_t sep_h = sc(win, 10);
	int32_t pad = sc(win, 4);
	int32_t icon_w = sc(win, 20);
	int32_t check_off = sc(win, 2);
	int32_t check_sz = sc(win, 9);
	int32_t check_half = sc(win, 4);
	int32_t radio_off = sc(win, 6);
	int32_t radio_r_out = sc(win, 5);
	int32_t radio_r_mid = sc(win, 4);
	int32_t radio_r_in = sc(win, 3);

	int32_t iy = 1;
	for(uint32_t i = 0; i < win->ctxmenu_count; ++i) {
		struct mkgui_ctxmenu_item *it = &win->ctxmenu_items[i];

		if(it->flags & MKGUI_MENUITEM_SEPARATOR) {
			draw_hline(p->pixels, p->w, p->h, pad + 1, iy + sep_h / 2, p->w - pad * 2 - 2, win->theme.widget_border);
			iy += sep_h;

		} else {
			uint32_t bg = ((int32_t)i == hover_item) ? win->theme.highlight : win->theme.menu_bg;
			uint32_t tc = (it->flags & MKGUI_DISABLED) ? win->theme.text_disabled : win->theme.text;

			if((int32_t)i == hover_item && !(it->flags & MKGUI_DISABLED)) {
				draw_rect_fill(p->pixels, p->w, p->h, pad, iy, p->w - pad * 2, win->row_height, bg);
			}

			int32_t ty = iy + (win->row_height - win->font_height) / 2;
			int32_t tx = icon_w + pad;
			push_text_clip(p->x + tx, ty + p->y, it->label, tc, p->x + 1, p->y + 1, p->x + p->w - 1, p->y + p->h - 1);

			int32_t icon_idx = -1;
			if(it->icon[0] && !(it->flags & (MKGUI_MENUITEM_CHECK | MKGUI_MENUITEM_RADIO))) {
				icon_idx = icon_resolve(win, it->icon);
			}

			if(icon_idx >= 0) {
				int32_t iy2 = iy + (win->row_height - icons[icon_idx].h) / 2;
				draw_icon_popup(p, &icons[icon_idx], pad, iy2);

			} else if(it->flags & MKGUI_MENUITEM_CHECK) {
				int32_t bx = pad + check_off;
				int32_t by = iy + win->row_height / 2 - check_half;
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
				int32_t cy = iy + win->row_height / 2;
				uint32_t rbg = ((int32_t)i == hover_item && !(it->flags & MKGUI_DISABLED)) ? bg : win->theme.menu_bg;
				draw_aa_circle_ring(p->pixels, p->w, p->h, cx, cy, radio_r_out, radio_r_mid, rbg, tc);
				if(it->flags & MKGUI_MENUITEM_CHECKED) {
					draw_aa_circle_fill(p->pixels, p->w, p->h, cx, cy, radio_r_in, tc);
				}
			}

			iy += win->row_height;
		}
	}

	render_clip_x1 = saved_clip_x1;
	render_clip_y1 = saved_clip_y1;
	render_clip_x2 = saved_clip_x2;
	render_clip_y2 = saved_clip_y2;
}

// [=]===^=[ mkgui_context_menu_clear ]=============================[=]
MKGUI_API void mkgui_context_menu_clear(struct mkgui_window *win) {
	MKGUI_CHECK(win);
	win->ctxmenu_count = 0;
}

// [=]===^=[ mkgui_context_menu_add ]===============================[=]
MKGUI_API void mkgui_context_menu_add(struct mkgui_window *win, uint32_t id, const char *label, const char *icon, uint32_t flags) {
	MKGUI_CHECK(win);
	if(!label && !(flags & MKGUI_MENUITEM_SEPARATOR)) {
		return;
	}

	if(win->ctxmenu_count >= MKGUI_MAX_CTXMENU) {
		return;
	}
	struct mkgui_ctxmenu_item *it = &win->ctxmenu_items[win->ctxmenu_count];
	memset(it, 0, sizeof(*it));
	it->id = id;
	it->flags = flags;
	if(label) {
		snprintf(it->label, MKGUI_MAX_TEXT, "%s", label);
	}

	if(icon) {
		snprintf(it->icon, MKGUI_ICON_NAME_LEN, "%s", icon);
	}
	++win->ctxmenu_count;
}

// [=]===^=[ mkgui_context_menu_add_separator ]=====================[=]
MKGUI_API void mkgui_context_menu_add_separator(struct mkgui_window *win) {
	MKGUI_CHECK(win);
	mkgui_context_menu_add(win, 0, NULL, NULL, MKGUI_MENUITEM_SEPARATOR);
}

// [=]===^=[ mkgui_context_menu_show ]==============================[=]
MKGUI_API void mkgui_context_menu_show(struct mkgui_window *win) {
	MKGUI_CHECK(win);
	if(win->ctxmenu_count == 0) {
		return;
	}

	popup_destroy_all(win);

	int32_t pw, ph;
	ctxmenu_metrics(win, &pw, &ph);

	int32_t abs_x = win->ctxmenu_x;
	int32_t abs_y = win->ctxmenu_y;

	platform_translate_coords(win, win->mouse_x, win->mouse_y, &abs_x, &abs_y);

	int32_t scr_w, scr_h;
	platform_screen_size(win, &scr_w, &scr_h);
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

	struct mkgui_popup *p = popup_create(win, abs_x, abs_y, pw, ph, MKGUI_CTXMENU_POPUP_ID);
	if(p) {
		render_ctxmenu_popup(win, p, -1);
		flush_text_popup(win, p);
		platform_popup_blit(win, p);
		platform_flush(win);
	}
}

// [=]===^=[ mkgui_context_menu_show_at ]============================[=]
MKGUI_API void mkgui_context_menu_show_at(struct mkgui_window *win, int32_t x, int32_t y) {
	MKGUI_CHECK(win);
	if(win->ctxmenu_count == 0) {
		return;
	}

	popup_destroy_all(win);

	int32_t pw, ph;
	ctxmenu_metrics(win, &pw, &ph);

	int32_t abs_x, abs_y;
	platform_translate_coords(win, x, y, &abs_x, &abs_y);

	int32_t scr_w, scr_h;
	platform_screen_size(win, &scr_w, &scr_h);
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

	struct mkgui_popup *p = popup_create(win, abs_x, abs_y, pw, ph, MKGUI_CTXMENU_POPUP_ID);
	if(p) {
		render_ctxmenu_popup(win, p, -1);
		flush_text_popup(win, p);
		platform_popup_blit(win, p);
		platform_flush(win);
	}
}
