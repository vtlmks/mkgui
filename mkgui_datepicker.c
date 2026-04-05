// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define DATEPICKER_BTN_W_PX     24
#define DATEPICKER_CAL_W_PX     224
#define DATEPICKER_HDR_H_PX     28
#define DATEPICKER_DOW_H_PX     20
#define DATEPICKER_CELL_SIZE_PX 32

// [=]===^=[ datepicker_days_in_month ]============================[=]
static uint32_t datepicker_days_in_month(int32_t year, int32_t month) {
	static uint32_t days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	if(month < 1 || month > 12) {
		return 30;
	}
	uint32_t d = days[month - 1];
	if(month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
		d = 29;
	}
	return d;
}

// [=]===^=[ datepicker_weekday ]==================================[=]
static uint32_t datepicker_weekday(int32_t year, int32_t month, int32_t day) {
	if(month < 3) {
		month += 12;
		--year;
	}
	int32_t h = (day + (13 * (month + 1)) / 5 + year + year / 4 - year / 100 + year / 400) % 7;
	return (uint32_t)((h + 6) % 7);
}

// [=]===^=[ datepicker_format ]===================================[=]
static void datepicker_format(struct mkgui_datepicker_data *dp, char *buf, uint32_t buf_size) {
	snprintf(buf, buf_size, "%04d-%02d-%02d", dp->year, dp->month, dp->day);
}

// [=]===^=[ datepicker_parse ]====================================[=]
static uint32_t datepicker_parse(char *text, int32_t *year, int32_t *month, int32_t *day) {
	int32_t y = 0, m = 0, d = 0;
	if(sscanf(text, "%d-%d-%d", &y, &m, &d) != 3) {
		return 0;
	}
	if(m < 1 || m > 12 || d < 1 || d > (int32_t)datepicker_days_in_month(y, m)) {
		return 0;
	}
	*year = y;
	*month = m;
	*day = d;
	return 1;
}

// [=]===^=[ datepicker_today ]====================================[=]
static void datepicker_today(int32_t *year, int32_t *month, int32_t *day) {
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	*year = tm->tm_year + 1900;
	*month = tm->tm_mon + 1;
	*day = tm->tm_mday;
}

// [=]===^=[ render_datepicker ]===================================[=]
static void render_datepicker(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	uint32_t disabled = (w->flags & MKGUI_DISABLED);
	uint32_t focused = (ctx->focus_id == w->id);
	uint32_t border = focused ? ctx->theme.highlight : ctx->theme.widget_border;

	int32_t btn_w = sc(ctx, DATEPICKER_BTN_W_PX);
	int32_t text_w = rw - btn_w;
	draw_patch(ctx, MKGUI_STYLE_SUNKEN, rx, ry, text_w, rh, ctx->theme.input_bg, border);

	uint32_t hovered = (!disabled && ctx->hover_id == w->id);
	uint32_t btn_bg = hovered ? ctx->theme.widget_hover : ctx->theme.widget_bg;
	draw_patch(ctx, MKGUI_STYLE_RAISED, rx + text_w, ry, btn_w, rh, btn_bg, border);

	struct mkgui_datepicker_data *dp = find_datepicker_data(ctx, w->id);
	if(!dp) {
		return;
	}

	uint32_t tc = disabled ? ctx->theme.text_disabled : ctx->theme.text;
	int32_t ty = ry + (rh - ctx->font_height) / 2;

	int32_t text_pad = sc(ctx, 4);
	int32_t cursor_inset = sc(ctx, 2);
	if(focused && dp->editing) {
		push_text_clip(rx + text_pad, ty, dp->edit_buf, tc, rx + 1, ry + 1, rx + text_w - 1, ry + rh - 1);
		char tmp[32];
		uint32_t cpos = dp->edit_cursor;
		uint32_t dlen = (uint32_t)strlen(dp->edit_buf);
		if(cpos > dlen) {
			cpos = dlen;
		}
		memcpy(tmp, dp->edit_buf, cpos);
		tmp[cpos] = '\0';
		int32_t cx = rx + text_pad + text_width(ctx, tmp);
		draw_vline(ctx->pixels, ctx->win_w, ctx->win_h, cx, ry + cursor_inset, rh - cursor_inset * 2, ctx->theme.text);
	} else {
		char buf[16];
		datepicker_format(dp, buf, sizeof(buf));
		push_text_clip(rx + text_pad, ty, buf, tc, rx + 1, ry + 1, rx + text_w - 1, ry + rh - 1);
	}

	int32_t ic_w = sc(ctx, 12);
	int32_t ic_h = sc(ctx, 10);
	int32_t icon_x = rx + text_w + (btn_w - ic_w) / 2;
	int32_t icon_y = ry + (rh - ic_h) / 2;
	int32_t ic_dot = sc(ctx, 2);
	int32_t ic_hdr = sc(ctx, 3);
	draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, icon_x, icon_y, ic_w, ic_h, tc);
	draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, icon_x + 1, icon_y + ic_hdr, ic_w - 2, ic_h - ic_hdr - 1, ctx->theme.input_bg);
	int32_t ic_dx = sc(ctx, 3);
	int32_t ic_dy = sc(ctx, 1);
	draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, icon_x + ic_dot, icon_y + ic_hdr + ic_dy, ic_dot, ic_dot, tc);
	draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, icon_x + ic_dot + ic_dx, icon_y + ic_hdr + ic_dy, ic_dot, ic_dot, tc);
	draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, icon_x + ic_dot + ic_dx * 2, icon_y + ic_hdr + ic_dy, ic_dot, ic_dot, tc);
	draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, icon_x + ic_dot, icon_y + ic_hdr + ic_dy + ic_dx, ic_dot, ic_dot, tc);
	draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, icon_x + ic_dot + ic_dx, icon_y + ic_hdr + ic_dy + ic_dx, ic_dot, ic_dot, tc);
}

// [=]===^=[ render_datepicker_popup ]=============================[=]
static void render_datepicker_popup(struct mkgui_ctx *ctx, struct mkgui_popup *p, struct mkgui_datepicker_data *dp, int32_t hover_day) {
	int32_t saved_clip_x1 = render_clip_x1;
	int32_t saved_clip_y1 = render_clip_y1;
	int32_t saved_clip_x2 = render_clip_x2;
	int32_t saved_clip_y2 = render_clip_y2;
	render_clip_x1 = 0;
	render_clip_y1 = 0;
	render_clip_x2 = INT32_MAX;
	render_clip_y2 = INT32_MAX;

	draw_rounded_rect(p->pixels, p->w, p->h, 0, 0, p->w, p->h, ctx->theme.menu_bg, ctx->theme.widget_border, ctx->theme.corner_radius);

	int32_t hdr_h = sc(ctx, DATEPICKER_HDR_H_PX);
	int32_t dow_h = sc(ctx, DATEPICKER_DOW_H_PX);
	int32_t cell_size = sc(ctx, DATEPICKER_CELL_SIZE_PX);
	int32_t popup_pad = sc(ctx, 4);
	int32_t gap2 = sc(ctx, 2);
	int32_t text_pad = sc(ctx, 4);

	int32_t cy = popup_pad;
	int32_t arrow_w = sc(ctx, 20);
	int32_t hdr_y = cy + (hdr_h - ctx->font_height) / 2;

	static char *month_names[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
	char *mname = (dp->view_month >= 1 && dp->view_month <= 12) ? month_names[dp->view_month - 1] : "?";

	int32_t left_x = popup_pad;
	int32_t right_x = p->w - popup_pad - arrow_w;
	int32_t btn_h = hdr_h - popup_pad;
	int32_t btn_y = cy + gap2;

	int32_t inner_x = left_x + arrow_w + gap2;
	int32_t inner_w = right_x - gap2 - inner_x;
	int32_t year_spin_w = sc(ctx, 56);
	int32_t year_btn_w = sc(ctx, 14);
	int32_t month_w = inner_w - year_spin_w - popup_pad;
	int32_t year_x = inner_x + month_w + popup_pad;

	uint32_t left_hover = (dp->cal_hover == -2);
	uint32_t right_hover = (dp->cal_hover == -3);
	uint32_t month_hover = (dp->cal_hover == -4);
	uint32_t year_up_hover = (dp->cal_hover == -5);
	uint32_t year_dn_hover = (dp->cal_hover == -6);

	int32_t acy = cy + hdr_h / 2;

	uint32_t lbg = left_hover ? ctx->theme.widget_hover : ctx->theme.menu_bg;
	draw_rounded_rect_fill(p->pixels, p->w, p->h, left_x, btn_y, arrow_w, btn_h, lbg, ctx->theme.corner_radius);
	int32_t acx_l = left_x + arrow_w / 2;
	int32_t arr_half = sc(ctx, 3);
	int32_t arr_off = sc(ctx, 2);
	for(uint32_t j = 0; j < (uint32_t)(arr_half + 1); ++j) {
		draw_vline(p->pixels, p->w, p->h, acx_l + arr_off - (int32_t)j, acy - arr_half + (int32_t)j, 1, ctx->theme.text);
		draw_vline(p->pixels, p->w, p->h, acx_l + arr_off - (int32_t)j, acy + arr_half - (int32_t)j, 1, ctx->theme.text);
	}

	uint32_t rbg = right_hover ? ctx->theme.widget_hover : ctx->theme.menu_bg;
	draw_rounded_rect_fill(p->pixels, p->w, p->h, right_x, btn_y, arrow_w, btn_h, rbg, ctx->theme.corner_radius);
	int32_t acx_r = right_x + arrow_w / 2;
	for(uint32_t j = 0; j < (uint32_t)(arr_half + 1); ++j) {
		draw_vline(p->pixels, p->w, p->h, acx_r - arr_off + (int32_t)j, acy - arr_half + (int32_t)j, 1, ctx->theme.text);
		draw_vline(p->pixels, p->w, p->h, acx_r - arr_off + (int32_t)j, acy + arr_half - (int32_t)j, 1, ctx->theme.text);
	}

	int32_t drop_icon_w = sc(ctx, 12);
	uint32_t mbg = month_hover ? ctx->theme.widget_hover : ctx->theme.widget_bg;
	draw_rounded_rect(p->pixels, p->w, p->h, inner_x, btn_y, month_w, btn_h, mbg, ctx->theme.widget_border, ctx->theme.corner_radius);
	int32_t mname_tw = text_width(ctx, mname);
	push_text_clip(p->x + inner_x + text_pad, hdr_y + p->y, mname, ctx->theme.text, p->x + inner_x, p->y, p->x + inner_x + month_w - drop_icon_w, p->y + p->h);
	int32_t drop_acx = inner_x + month_w - drop_icon_w + arr_half;
	int32_t drop_acy = acy - gap2 + arr_half / 2;
	draw_triangle_aa(p->pixels, p->w, p->h,
		drop_acx - arr_half, drop_acy - arr_half / 2, drop_acx + arr_half, drop_acy - arr_half / 2, drop_acx, drop_acy + arr_half / 2, ctx->theme.text);
	(void)mname_tw;

	int32_t yr = ctx->theme.corner_radius;
	int32_t year_half = btn_h / 2;
	draw_rounded_rect(p->pixels, p->w, p->h, year_x, btn_y, year_spin_w, btn_h, ctx->theme.input_bg, ctx->theme.widget_border, yr);
	char year_buf[8];
	snprintf(year_buf, sizeof(year_buf), "%d", dp->view_year);
	int32_t year_tw = text_width(ctx, year_buf);
	int32_t year_text_w = year_spin_w - year_btn_w;
	push_text_clip(p->x + year_x + (year_text_w - year_tw) / 2, hdr_y + p->y, year_buf, ctx->theme.text, p->x + year_x + 1, p->y, p->x + year_x + year_text_w, p->y + p->h);

	uint32_t up_bg = year_up_hover ? ctx->theme.widget_hover : ctx->theme.widget_bg;
	int32_t sbx = year_x + year_spin_w - year_btn_w;
	draw_rounded_rect(p->pixels, p->w, p->h, sbx, btn_y, year_btn_w, year_half, up_bg, ctx->theme.widget_border, yr);
	int32_t sacx = sbx + year_btn_w / 2;
	int32_t sacy_up = btn_y + year_half / 2 - gap2;
	draw_triangle_aa(p->pixels, p->w, p->h,
		sacx, sacy_up, sacx - arr_half, sacy_up + arr_half, sacx + arr_half, sacy_up + arr_half, ctx->theme.text);

	uint32_t dn_bg = year_dn_hover ? ctx->theme.widget_hover : ctx->theme.widget_bg;
	draw_rounded_rect(p->pixels, p->w, p->h, sbx, btn_y + year_half, year_btn_w, btn_h - year_half, dn_bg, ctx->theme.widget_border, yr);
	int32_t sacy_dn = btn_y + year_half + (btn_h - year_half) / 2 - gap2;
	draw_triangle_aa(p->pixels, p->w, p->h,
		sacx - arr_half, sacy_dn, sacx + arr_half, sacy_dn, sacx, sacy_dn + arr_half, ctx->theme.text);

	cy += hdr_h;

	if(dp->month_select) {
		for(uint32_t mi = 0; mi < 12; ++mi) {
			int32_t iy = cy + 1 + (int32_t)mi * ctx->row_height;
			if(iy + ctx->row_height <= cy || iy >= p->h) {
				continue;
			}
			uint32_t is_sel = ((int32_t)(mi + 1) == dp->view_month);
			uint32_t is_hover_m = (hover_day == (int32_t)(mi + 100));
			uint32_t bg;
			if(is_hover_m) {
				bg = ctx->theme.highlight;
			} else if(is_sel) {
				bg = ctx->theme.selection;
			} else {
				bg = ctx->theme.menu_bg;
			}
			draw_rect_fill(p->pixels, p->w, p->h, 1, iy, p->w - 2, ctx->row_height, bg);
			int32_t ty = iy + (ctx->row_height - ctx->font_height) / 2;
			uint32_t tc = is_sel ? ctx->theme.sel_text : ctx->theme.text;
			int32_t list_pad = sc(ctx, 5);
			push_text_clip(p->x + list_pad, ty + p->y, month_names[mi], tc, p->x + 1, p->y + cy, p->x + p->w - 1, p->y + p->h - 1);
		}
	} else {
		static char *dow[] = { "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su" };
		int32_t cw = cell_size;
		for(uint32_t d = 0; d < 7; ++d) {
			int32_t dx = (int32_t)d * cw;
			int32_t dw = text_width(ctx, dow[d]);
			push_text_clip(p->x + dx + (cw - dw) / 2, cy + gap2 + p->y, dow[d], ctx->theme.text_disabled, p->x, p->y, p->x + p->w, p->y + p->h);
		}
		cy += dow_h;

		uint32_t first_dow = datepicker_weekday(dp->view_year, dp->view_month, 1);
		uint32_t dim = datepicker_days_in_month(dp->view_year, dp->view_month);

		int32_t today_y, today_m, today_d;
		datepicker_today(&today_y, &today_m, &today_d);

		int32_t prev_month = dp->view_month - 1;
		int32_t prev_year = dp->view_year;
		if(prev_month < 1) {
			prev_month = 12;
			--prev_year;
		}
		uint32_t prev_dim = datepicker_days_in_month(prev_year, prev_month);

		for(uint32_t row = 0; row < 6; ++row) {
			for(uint32_t col = 0; col < 7; ++col) {
				int32_t cell_idx = (int32_t)(row * 7 + col) - (int32_t)first_dow;
				int32_t day_num;
				uint32_t is_cur_month = 0;
				if(cell_idx < 0) {
					day_num = (int32_t)prev_dim + cell_idx + 1;
				} else if(cell_idx >= (int32_t)dim) {
					day_num = cell_idx - (int32_t)dim + 1;
				} else {
					day_num = cell_idx + 1;
					is_cur_month = 1;
				}

				int32_t cx = (int32_t)col * cw;
				int32_t cell_y = cy + (int32_t)row * cw;
				int32_t center_x = cx + cw / 2;
				int32_t center_y = cell_y + cw / 2;
				int32_t circ_r = cw / 2 - gap2;

				uint32_t is_selected = (is_cur_month && day_num == dp->day && dp->view_month == dp->month && dp->view_year == dp->year);
				uint32_t is_today = (is_cur_month && day_num == today_d && dp->view_month == today_m && dp->view_year == today_y);
				uint32_t is_hover = (is_cur_month && day_num == hover_day);

				if(is_selected) {
					draw_rounded_rect_fill(p->pixels, p->w, p->h, center_x - circ_r, center_y - circ_r, circ_r * 2, circ_r * 2, ctx->theme.accent, circ_r);
				} else if(is_hover) {
					draw_rounded_rect_fill(p->pixels, p->w, p->h, center_x - circ_r, center_y - circ_r, circ_r * 2, circ_r * 2, ctx->theme.widget_hover, circ_r);
				} else if(is_today) {
					draw_rounded_rect(p->pixels, p->w, p->h, center_x - circ_r, center_y - circ_r, circ_r * 2, circ_r * 2, ctx->theme.menu_bg, ctx->theme.accent, circ_r);
				}

				char dbuf[4];
				snprintf(dbuf, sizeof(dbuf), "%d", day_num);
				int32_t dw = text_width(ctx, dbuf);
				int32_t tx = cx + (cw - dw) / 2;
				int32_t text_y = cell_y + (cw - ctx->font_height) / 2;
				uint32_t tc;
				if(is_selected) {
					tc = 0xffffffff;
				} else if(!is_cur_month) {
					tc = ctx->theme.text_disabled;
				} else {
					tc = ctx->theme.text;
				}
				push_text_clip(p->x + tx, text_y + p->y, dbuf, tc, p->x, p->y, p->x + p->w, p->y + p->h);
			}
		}
	}

	render_clip_x1 = saved_clip_x1;
	render_clip_y1 = saved_clip_y1;
	render_clip_x2 = saved_clip_x2;
	render_clip_y2 = saved_clip_y2;
}

// [=]===^=[ datepicker_open_popup ]===============================[=]
static void datepicker_open_popup(struct mkgui_ctx *ctx, uint32_t widget_id) {
	struct mkgui_datepicker_data *dp = find_datepicker_data(ctx, widget_id);
	if(!dp) {
		return;
	}

	dp->view_year = dp->year;
	dp->view_month = dp->month;
	dp->popup_open = 1;
	dp->cal_hover = -1;
	dp->month_select = 0;

	uint32_t widx = (uint32_t)find_widget_idx(ctx, widget_id);
	if(widx >= ctx->widget_count) {
		return;
	}

	int32_t abs_x, abs_y;
	platform_translate_coords(ctx, ctx->rects[widx].x, ctx->rects[widx].y + ctx->rects[widx].h, &abs_x, &abs_y);

	int32_t cal_w = sc(ctx, DATEPICKER_CAL_W_PX);
	int32_t cal_h = sc(ctx, DATEPICKER_HDR_H_PX) + 12 * ctx->row_height + sc(ctx, 4);
	popup_destroy_all(ctx);
	struct mkgui_popup *p = popup_create(ctx, abs_x, abs_y, cal_w, cal_h, widget_id);
	if(p) {
		render_datepicker_popup(ctx, p, dp, -1);
		flush_text_popup(ctx, p);
		platform_popup_blit(ctx, p);
		platform_flush(ctx);
	}
}

// [=]===^=[ datepicker_cal_hit ]=================================[=]
static int32_t datepicker_cal_hit(struct mkgui_ctx *ctx, struct mkgui_datepicker_data *dp, int32_t mx, int32_t my) {
	int32_t hdr_h = sc(ctx, DATEPICKER_HDR_H_PX);
	int32_t dow_h = sc(ctx, DATEPICKER_DOW_H_PX);
	int32_t cell_sz = sc(ctx, DATEPICKER_CELL_SIZE_PX);
	int32_t cal_w = sc(ctx, DATEPICKER_CAL_W_PX);
	int32_t popup_pad = sc(ctx, 4);
	int32_t gap2 = sc(ctx, 2);
	int32_t arrow_w = sc(ctx, 20);
	if(my >= popup_pad && my < popup_pad + hdr_h) {
		int32_t left_x = popup_pad;
		int32_t right_x = cal_w - popup_pad - arrow_w;
		if(mx >= left_x && mx < left_x + arrow_w) {
			return -2;
		}
		if(mx >= right_x && mx < right_x + arrow_w) {
			return -3;
		}
		int32_t inner_x = left_x + arrow_w + gap2;
		int32_t inner_w = right_x - gap2 - inner_x;
		int32_t year_spin_w = sc(ctx, 56);
		int32_t year_btn_w = sc(ctx, 14);
		int32_t month_w = inner_w - year_spin_w - popup_pad;
		int32_t year_x = inner_x + month_w + popup_pad;
		if(mx >= inner_x && mx < inner_x + month_w) {
			return -4;
		}
		if(mx >= year_x && mx < year_x + year_spin_w) {
			int32_t sb_x = year_x + year_spin_w - year_btn_w;
			if(mx >= sb_x) {
				int32_t btn_mid = popup_pad + gap2 + (hdr_h - popup_pad) / 2;
				if(my < btn_mid) {
					return -5;
				}
				return -6;
			}
			return -7;
		}
		return -1;
	}

	int32_t body_y = popup_pad + hdr_h;

	if(dp->month_select) {
		int32_t list_y = body_y + 1;
		int32_t item = (my - list_y) / ctx->row_height;
		if(item >= 0 && item < 12) {
			return item + 100;
		}
		return -1;
	}

	int32_t grid_y = body_y + dow_h;
	if(my < grid_y) {
		return -1;
	}
	int32_t row = (my - grid_y) / cell_sz;
	int32_t col = mx / cell_sz;
	if(row < 0 || row >= 6 || col < 0 || col >= 7) {
		return -1;
	}
	uint32_t first_dow = datepicker_weekday(dp->view_year, dp->view_month, 1);
	int32_t cell_idx = row * 7 + col - (int32_t)first_dow;
	int32_t dim = (int32_t)datepicker_days_in_month(dp->view_year, dp->view_month);
	if(cell_idx >= 0 && cell_idx < dim) {
		return cell_idx + 1;
	}
	return -1;
}

// [=]===^=[ handle_datepicker_click ]=============================[=]
static void handle_datepicker_click(struct mkgui_ctx *ctx, uint32_t widget_id) {
	struct mkgui_datepicker_data *dp = find_datepicker_data(ctx, widget_id);
	if(!dp) {
		return;
	}

	uint32_t widx = (uint32_t)find_widget_idx(ctx, widget_id);
	if(widx >= ctx->widget_count) {
		return;
	}
	int32_t rx = ctx->rects[widx].x;
	int32_t rw = ctx->rects[widx].w;
	int32_t btn_x = rx + rw - sc(ctx, DATEPICKER_BTN_W_PX);

	if(ctx->mouse_x >= btn_x) {
		datepicker_open_popup(ctx, widget_id);
	} else {
		dp->editing = 1;
		datepicker_format(dp, dp->edit_buf, sizeof(dp->edit_buf));
		dp->edit_cursor = (uint32_t)strlen(dp->edit_buf);
	}
	dirty_all(ctx);
}

// [=]===^=[ handle_datepicker_key ]===============================[=]
static uint32_t handle_datepicker_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks, uint32_t keymod, char *buf, int32_t len) {
	struct mkgui_datepicker_data *dp = find_datepicker_data(ctx, ctx->focus_id);
	if(!dp) {
		return 0;
	}
	(void)keymod;

	if(ks == MKGUI_KEY_RETURN) {
		if(dp->editing) {
			int32_t y, m, d;
			if(datepicker_parse(dp->edit_buf, &y, &m, &d)) {
				dp->year = y;
				dp->month = m;
				dp->day = d;
				ev->type = MKGUI_EVENT_DATEPICKER_CHANGED;
				ev->id = ctx->focus_id;
			}
			dp->editing = 0;
			dirty_all(ctx);
			return 1;
		}
		datepicker_open_popup(ctx, ctx->focus_id);
		return 1;
	}

	if(ks == MKGUI_KEY_ESCAPE) {
		if(dp->editing) {
			dp->editing = 0;
			dirty_all(ctx);
			return 1;
		}
		return 0;
	}

	if(dp->editing) {
		if(ks == MKGUI_KEY_BACKSPACE) {
			if(dp->edit_cursor > 0) {
				uint32_t elen = (uint32_t)strlen(dp->edit_buf);
				memmove(&dp->edit_buf[dp->edit_cursor - 1], &dp->edit_buf[dp->edit_cursor], elen - dp->edit_cursor + 1);
				--dp->edit_cursor;
				dirty_all(ctx);
			}
			return 1;
		}
		if(ks == MKGUI_KEY_LEFT && dp->edit_cursor > 0) {
			--dp->edit_cursor;
			dirty_all(ctx);
			return 1;
		}
		if(ks == MKGUI_KEY_RIGHT && dp->edit_cursor < (uint32_t)strlen(dp->edit_buf)) {
			++dp->edit_cursor;
			dirty_all(ctx);
			return 1;
		}
		if(len == 1 && ((buf[0] >= '0' && buf[0] <= '9') || buf[0] == '-')) {
			uint32_t elen = (uint32_t)strlen(dp->edit_buf);
			if(elen < 10) {
				memmove(&dp->edit_buf[dp->edit_cursor + 1], &dp->edit_buf[dp->edit_cursor], elen - dp->edit_cursor + 1);
				dp->edit_buf[dp->edit_cursor] = buf[0];
				++dp->edit_cursor;
				dirty_all(ctx);
			}
			return 1;
		}
		return 1;
	}

	return 0;
}

// [=]===^=[ mkgui_datepicker_set ]================================[=]
MKGUI_API void mkgui_datepicker_set(struct mkgui_ctx *ctx, uint32_t id, int32_t year, int32_t month, int32_t day) {
	MKGUI_CHECK(ctx);
	struct mkgui_datepicker_data *dp = find_datepicker_data(ctx, id);
	if(!dp) {
		return;
	}
	dp->year = year;
	dp->month = month;
	dp->day = day;
	dp->editing = 0;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_datepicker_get ]================================[=]
MKGUI_API void mkgui_datepicker_get(struct mkgui_ctx *ctx, uint32_t id, int32_t *year, int32_t *month, int32_t *day) {
	MKGUI_CHECK(ctx);
	struct mkgui_datepicker_data *dp = find_datepicker_data(ctx, id);
	int32_t y = dp ? dp->year : 2026;
	int32_t m = dp ? dp->month : 1;
	int32_t d = dp ? dp->day : 1;
	if(year) { *year = y; }
	if(month) { *month = m; }
	if(day) { *day = d; }
}

// [=]===^=[ mkgui_datepicker_get_text ]===========================[=]
MKGUI_API char *mkgui_datepicker_get_text(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, "");
	static char datepicker_buf[16];
	struct mkgui_datepicker_data *dp = find_datepicker_data(ctx, id);
	if(!dp) {
		snprintf(datepicker_buf, sizeof(datepicker_buf), "2026-01-01");
		return datepicker_buf;
	}
	datepicker_format(dp, datepicker_buf, sizeof(datepicker_buf));
	return datepicker_buf;
}

// [=]===^=[ mkgui_datepicker_set_readonly ]=========================[=]
MKGUI_API void mkgui_datepicker_set_readonly(struct mkgui_ctx *ctx, uint32_t id, uint32_t readonly) {
	MKGUI_CHECK(ctx);
	struct mkgui_widget *w = find_widget(ctx, id);
	if(!w) {
		return;
	}
	if(readonly) {
		w->style |= MKGUI_DATEPICKER_READONLY;

	} else {
		w->style &= ~MKGUI_DATEPICKER_READONLY;
	}
	dirty_all(ctx);
}

// [=]===^=[ mkgui_datepicker_get_readonly ]=========================[=]
MKGUI_API uint32_t mkgui_datepicker_get_readonly(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_widget *w = find_widget(ctx, id);
	return (w && (w->style & MKGUI_DATEPICKER_READONLY)) ? 1 : 0;
}
