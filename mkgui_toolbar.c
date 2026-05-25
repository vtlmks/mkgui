// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ render_toolbar ]=====================================[=]
static void render_toolbar(struct mkgui_window *win, uint32_t idx) {
	struct mkgui_widget *w = &win->widgets[idx];
	int32_t rx = win->rects[idx].x;
	int32_t ry = win->rects[idx].y;
	int32_t rw = win->rects[idx].w;
	int32_t rh = win->rects[idx].h;
	uint32_t mode = w->style & MKGUI_TOOLBAR_MODE_MASK;
	uint32_t show_icons = (mode != MKGUI_TOOLBAR_TEXT_ONLY);
	uint32_t show_text = (mode != MKGUI_TOOLBAR_ICONS_ONLY);

	draw_rect_fill(win->pixels, win->win_w, win->win_h, rx, ry, rw, rh, win->theme.bg);

	int32_t bx = rx + 2;
	for(uint32_t i = 0; i < win->widget_count; ++i) {
		struct mkgui_widget *btn = &win->widgets[i];
		if(btn->type != MKGUI_BUTTON || btn->parent_id != w->id) {
			continue;
		}

		if(btn->style & MKGUI_BUTTON_SEPARATOR) {
			int32_t sx = bx + win->toolbar_sep_w / 2;
			int32_t sep_margin = rh / 5;
			if(sep_margin < 4) {
				sep_margin = 4;
			} else if(sep_margin > 8) {
				sep_margin = 8;
			}
			draw_vline(win->pixels, win->win_w, win->win_h, sx, ry + sep_margin, rh - sep_margin * 2, win->theme.widget_border);
			bx += win->toolbar_sep_w;
			int32_t sep_idx = find_widget_idx(win, btn->id);
			if(sep_idx >= 0) {
				win->rects[sep_idx].x = 0;
				win->rects[sep_idx].y = 0;
				win->rects[sep_idx].w = 0;
				win->rects[sep_idx].h = 0;
			}
			continue;
		}

		int32_t ii = show_icons ? widget_icon_idx(win, btn) : -1;
		uint32_t has_icon = (ii >= 0);
		int32_t tw = show_text ? label_text_width(win, btn) : 0;
		int32_t icon_w = has_icon ? icons[ii].w + 4 : 0;
		int32_t content_w = icon_w + tw;
		int32_t bw = content_w + 12;
		if(bw < win->toolbar_btn_w) {
			bw = win->toolbar_btn_w;
		}

		int32_t by = ry + 2;
		int32_t bh = rh - 4;

		int32_t btn_idx = find_widget_idx(win, btn->id);
		if(btn_idx >= 0) {
			win->rects[btn_idx].x = bx;
			win->rects[btn_idx].y = by;
			win->rects[btn_idx].w = bw;
			win->rects[btn_idx].h = bh;
		}

		if(win->press_id == btn->id) {
			draw_patch(win, MKGUI_STYLE_RAISED, bx, by, bw, bh, win->theme.widget_press, win->theme.widget_border);
		} else if(win->hover_id == btn->id) {
			draw_patch(win, MKGUI_STYLE_RAISED, bx, by, bw, bh, win->theme.widget_hover, win->theme.widget_border);
		}

		int32_t cx = bx + (bw - content_w) / 2;
		if(has_icon) {
			int32_t iy = by + (bh - icons[ii].h) / 2;
			draw_icon(win->pixels, win->win_w, win->win_h, &icons[ii], cx, iy, bx, by, bx + bw, by + bh);
			cx += icons[ii].w + 4;
		}

		if(show_text && btn->label[0]) {
			int32_t ty = by + (bh - win->font_height) / 2;
			push_text_clip(cx, ty, btn->label, win->theme.text, bx, by, bx + bw, by + bh);
		}

		bx += bw + 2;
	}
}
