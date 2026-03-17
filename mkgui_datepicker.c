// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define DATEPICKER_BTN_W     24
#define DATEPICKER_CAL_W     224
#define DATEPICKER_HDR_H     28
#define DATEPICKER_DOW_H     20
#define DATEPICKER_CELL_SIZE 32
#define DATEPICKER_CAL_H     (DATEPICKER_HDR_H + 12 * MKGUI_ROW_HEIGHT + 4)

// [=]===^=[ datepicker_days_in_month ]============================[=]
static uint32_t datepicker_days_in_month(int32_t year, int32_t month) {
	static const uint32_t days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
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
static uint32_t datepicker_parse(const char *text, int32_t *year, int32_t *month, int32_t *day) {
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
	uint32_t border = focused ? ctx->theme.splitter : ctx->theme.widget_border;

	int32_t text_w = rw - DATEPICKER_BTN_W;
	draw_patch(ctx, MKGUI_STYLE_SUNKEN, rx, ry, text_w, rh, ctx->theme.input_bg, border);

	uint32_t hovered = (!disabled && ctx->hover_id == w->id);
	uint32_t btn_bg = hovered ? ctx->theme.widget_hover : ctx->theme.widget_bg;
	draw_patch(ctx, MKGUI_STYLE_RAISED, rx + text_w, ry, DATEPICKER_BTN_W, rh, btn_bg, border);

	struct mkgui_datepicker_data *dp = find_datepicker_data(ctx, w->id);
	if(!dp) {
		return;
	}

	uint32_t tc = disabled ? ctx->theme.text_disabled : ctx->theme.text;
	int32_t ty = ry + (rh - ctx->font_height) / 2;

	if(focused && dp->editing) {
		push_text_clip(rx + 4, ty, dp->edit_buf, tc, rx + 1, ry + 1, rx + text_w - 1, ry + rh - 1);
		char tmp[32];
		uint32_t cpos = dp->edit_cursor;
		uint32_t dlen = (uint32_t)strlen(dp->edit_buf);
		if(cpos > dlen) {
			cpos = dlen;
		}
		memcpy(tmp, dp->edit_buf, cpos);
		tmp[cpos] = '\0';
		int32_t cx = rx + 4 + text_width(ctx, tmp);
		draw_vline(ctx->pixels, ctx->win_w, ctx->win_h, cx, ry + 2, rh - 4, ctx->theme.text);
	} else {
		char buf[16];
		datepicker_format(dp, buf, sizeof(buf));
		push_text_clip(rx + 4, ty, buf, tc, rx + 1, ry + 1, rx + text_w - 1, ry + rh - 1);
	}

	int32_t icon_x = rx + text_w + (DATEPICKER_BTN_W - 12) / 2;
	int32_t icon_y = ry + (rh - 10) / 2;
	draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, icon_x, icon_y, 12, 10, tc);
	draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, icon_x + 1, icon_y + 3, 10, 6, ctx->theme.input_bg);
	draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, icon_x + 2, icon_y + 4, 2, 2, tc);
	draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, icon_x + 5, icon_y + 4, 2, 2, tc);
	draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, icon_x + 8, icon_y + 4, 2, 2, tc);
	draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, icon_x + 2, icon_y + 7, 2, 2, tc);
	draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, icon_x + 5, icon_y + 7, 2, 2, tc);
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

	int32_t cy = 4;
	int32_t arrow_w = 20;
	int32_t hdr_y = cy + (DATEPICKER_HDR_H - ctx->font_height) / 2;

	static const char *month_names[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
	const char *mname = (dp->view_month >= 1 && dp->view_month <= 12) ? month_names[dp->view_month - 1] : "?";

	int32_t left_x = 4;
	int32_t right_x = p->w - 4 - arrow_w;
	int32_t btn_h = DATEPICKER_HDR_H - 4;
	int32_t btn_y = cy + 2;

	int32_t inner_x = left_x + arrow_w + 2;
	int32_t inner_w = right_x - 2 - inner_x;
	int32_t year_spin_w = 56;
	int32_t year_btn_w = 14;
	int32_t month_w = inner_w - year_spin_w - 4;
	int32_t year_x = inner_x + month_w + 4;

	uint32_t left_hover = (dp->cal_hover == -2);
	uint32_t right_hover = (dp->cal_hover == -3);
	uint32_t month_hover = (dp->cal_hover == -4);
	uint32_t year_up_hover = (dp->cal_hover == -5);
	uint32_t year_dn_hover = (dp->cal_hover == -6);

	int32_t acy = cy + DATEPICKER_HDR_H / 2;

	uint32_t lbg = left_hover ? ctx->theme.widget_hover : ctx->theme.menu_bg;
	draw_rounded_rect_fill(p->pixels, p->w, p->h, left_x, btn_y, arrow_w, btn_h, lbg, ctx->theme.corner_radius);
	int32_t acx_l = left_x + arrow_w / 2;
	for(int32_t j = 0; j < 4; ++j) {
		draw_vline(p->pixels, p->w, p->h, acx_l + 2 - j, acy - 3 + j, 1, ctx->theme.text);
		draw_vline(p->pixels, p->w, p->h, acx_l + 2 - j, acy + 3 - j, 1, ctx->theme.text);
	}

	uint32_t rbg = right_hover ? ctx->theme.widget_hover : ctx->theme.menu_bg;
	draw_rounded_rect_fill(p->pixels, p->w, p->h, right_x, btn_y, arrow_w, btn_h, rbg, ctx->theme.corner_radius);
	int32_t acx_r = right_x + arrow_w / 2;
	for(int32_t j = 0; j < 4; ++j) {
		draw_vline(p->pixels, p->w, p->h, acx_r - 2 + j, acy - 3 + j, 1, ctx->theme.text);
		draw_vline(p->pixels, p->w, p->h, acx_r - 2 + j, acy + 3 - j, 1, ctx->theme.text);
	}

	uint32_t mbg = month_hover ? ctx->theme.widget_hover : ctx->theme.widget_bg;
	draw_rounded_rect(p->pixels, p->w, p->h, inner_x, btn_y, month_w, btn_h, mbg, ctx->theme.widget_border, ctx->theme.corner_radius);
	int32_t mname_tw = text_width(ctx, mname);
	push_text_clip(p->x + inner_x + 4, hdr_y + p->y, mname, ctx->theme.text, p->x + inner_x, p->y, p->x + inner_x + month_w - 12, p->y + p->h);
	int32_t drop_ax = inner_x + month_w - 12;
	int32_t drop_ay = acy - 2;
	for(int32_t j = 0; j < 4; ++j) {
		draw_hline(p->pixels, p->w, p->h, drop_ax + j, drop_ay + j, 7 - j * 2, ctx->theme.text);
	}
	(void)mname_tw;

	int32_t yr = ctx->theme.corner_radius;
	int32_t year_half = btn_h / 2;
	draw_rounded_rect(p->pixels, p->w, p->h, year_x, btn_y, year_spin_w, btn_h, ctx->theme.input_bg, ctx->theme.widget_border, yr);
	char year_buf[8];
	snprintf(year_buf, sizeof(year_buf), "%d", dp->view_year);
	int32_t year_tw = text_width(ctx, year_buf);
	push_text_clip(p->x + year_x + (year_spin_w - year_tw) / 2, hdr_y + p->y, year_buf, ctx->theme.text, p->x + year_x + year_btn_w, p->y, p->x + year_x + year_spin_w - year_btn_w, p->y + p->h);

	uint32_t up_bg = year_up_hover ? ctx->theme.widget_hover : ctx->theme.widget_bg;
	int32_t sbx = year_x + year_spin_w - year_btn_w;
	draw_rounded_rect(p->pixels, p->w, p->h, sbx, btn_y, year_btn_w, year_half, up_bg, ctx->theme.widget_border, yr);
	int32_t sacx = sbx + year_btn_w / 2;
	int32_t sacy_up = btn_y + year_half / 2 - 2;
	for(int32_t j = 0; j < 4; ++j) {
		draw_hline(p->pixels, p->w, p->h, sacx - j, sacy_up + j, 1 + j * 2, ctx->theme.text);
	}

	uint32_t dn_bg = year_dn_hover ? ctx->theme.widget_hover : ctx->theme.widget_bg;
	draw_rounded_rect(p->pixels, p->w, p->h, sbx, btn_y + year_half, year_btn_w, btn_h - year_half, dn_bg, ctx->theme.widget_border, yr);
	int32_t sacy_dn = btn_y + year_half + (btn_h - year_half) / 2 - 2;
	for(int32_t j = 0; j < 4; ++j) {
		draw_hline(p->pixels, p->w, p->h, sacx - (3 - j), sacy_dn + j, 1 + (3 - j) * 2, ctx->theme.text);
	}

	cy += DATEPICKER_HDR_H;

	if(dp->month_select) {
		for(uint32_t mi = 0; mi < 12; ++mi) {
			int32_t iy = cy + 1 + (int32_t)mi * MKGUI_ROW_HEIGHT;
			if(iy + MKGUI_ROW_HEIGHT <= cy || iy >= p->h) {
				continue;
			}
			uint32_t is_sel = ((int32_t)(mi + 1) == dp->view_month);
			uint32_t is_hover_m = (hover_day == (int32_t)(mi + 100));
			uint32_t bg;
			if(is_hover_m) {
				bg = ctx->theme.menu_hover;
			} else if(is_sel) {
				bg = ctx->theme.selection;
			} else {
				bg = ctx->theme.menu_bg;
			}
			draw_rect_fill(p->pixels, p->w, p->h, 1, iy, p->w - 2, MKGUI_ROW_HEIGHT, bg);
			int32_t ty = iy + (MKGUI_ROW_HEIGHT - ctx->font_height) / 2;
			uint32_t tc = is_sel ? ctx->theme.sel_text : ctx->theme.text;
			push_text_clip(p->x + 5, ty + p->y, month_names[mi], tc, p->x + 1, p->y + cy, p->x + p->w - 1, p->y + p->h - 1);
		}
	} else {
		static const char *dow[] = { "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su" };
		int32_t cw = DATEPICKER_CELL_SIZE;
		for(uint32_t d = 0; d < 7; ++d) {
			int32_t dx = (int32_t)d * cw;
			int32_t dw = text_width(ctx, dow[d]);
			push_text_clip(p->x + dx + (cw - dw) / 2, cy + 2 + p->y, dow[d], ctx->theme.text_disabled, p->x, p->y, p->x + p->w, p->y + p->h);
		}
		cy += DATEPICKER_DOW_H;

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
				int32_t circ_r = cw / 2 - 2;

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

	uint32_t widx = find_widget_idx(ctx, widget_id);
	if(widx >= ctx->widget_count) {
		return;
	}

	int32_t abs_x, abs_y;
	platform_translate_coords(ctx, ctx->rects[widx].x, ctx->rects[widx].y + ctx->rects[widx].h, &abs_x, &abs_y);

	popup_destroy_all(ctx);
	struct mkgui_popup *p = popup_create(ctx, abs_x, abs_y, DATEPICKER_CAL_W, DATEPICKER_CAL_H, widget_id);
	if(p) {
		render_datepicker_popup(ctx, p, dp, -1);
		flush_text_popup(ctx, p);
		platform_popup_blit(ctx, p);
		platform_flush(ctx);
	}
}

// [=]===^=[ datepicker_cal_hit ]=================================[=]
static int32_t datepicker_cal_hit(struct mkgui_datepicker_data *dp, int32_t mx, int32_t my) {
	int32_t arrow_w = 20;
	if(my >= 4 && my < 4 + DATEPICKER_HDR_H) {
		int32_t left_x = 4;
		int32_t right_x = DATEPICKER_CAL_W - 4 - arrow_w;
		if(mx >= left_x && mx < left_x + arrow_w) {
			return -2;
		}
		if(mx >= right_x && mx < right_x + arrow_w) {
			return -3;
		}
		int32_t inner_x = left_x + arrow_w + 2;
		int32_t inner_w = right_x - 2 - inner_x;
		int32_t year_spin_w = 56;
		int32_t year_btn_w = 14;
		int32_t month_w = inner_w - year_spin_w - 4;
		int32_t year_x = inner_x + month_w + 4;
		if(mx >= inner_x && mx < inner_x + month_w) {
			return -4;
		}
		if(mx >= year_x && mx < year_x + year_spin_w) {
			int32_t sb_x = year_x + year_spin_w - year_btn_w;
			if(mx >= sb_x) {
				int32_t btn_mid = 4 + 2 + (DATEPICKER_HDR_H - 4) / 2;
				if(my < btn_mid) {
					return -5;
				}
				return -6;
			}
			return -7;
		}
		return -1;
	}

	int32_t body_y = 4 + DATEPICKER_HDR_H;

	if(dp->month_select) {
		int32_t list_y = body_y + 1;
		int32_t item = (my - list_y) / MKGUI_ROW_HEIGHT;
		if(item >= 0 && item < 12) {
			return item + 100;
		}
		return -1;
	}

	int32_t grid_y = body_y + DATEPICKER_DOW_H;
	if(my < grid_y) {
		return -1;
	}
	int32_t row = (my - grid_y) / DATEPICKER_CELL_SIZE;
	int32_t col = mx / DATEPICKER_CELL_SIZE;
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

	uint32_t widx = find_widget_idx(ctx, widget_id);
	if(widx >= ctx->widget_count) {
		return;
	}
	int32_t rx = ctx->rects[widx].x;
	int32_t rw = ctx->rects[widx].w;
	int32_t btn_x = rx + rw - DATEPICKER_BTN_W;

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
static uint32_t handle_datepicker_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks, uint32_t keymod, const char *buf, int32_t len) {
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
static void mkgui_datepicker_set(struct mkgui_ctx *ctx, uint32_t id, int32_t year, int32_t month, int32_t day) {
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
static void mkgui_datepicker_get(struct mkgui_ctx *ctx, uint32_t id, int32_t *year, int32_t *month, int32_t *day) {
	struct mkgui_datepicker_data *dp = find_datepicker_data(ctx, id);
	if(!dp) {
		*year = 2026;
		*month = 1;
		*day = 1;
		return;
	}
	*year = dp->year;
	*month = dp->month;
	*day = dp->day;
}

// [=]===^=[ mkgui_datepicker_get_text ]===========================[=]
static const char *mkgui_datepicker_get_text(struct mkgui_ctx *ctx, uint32_t id) {
	static char datepicker_buf[16];
	struct mkgui_datepicker_data *dp = find_datepicker_data(ctx, id);
	if(!dp) {
		snprintf(datepicker_buf, sizeof(datepicker_buf), "2026-01-01");
		return datepicker_buf;
	}
	datepicker_format(dp, datepicker_buf, sizeof(datepicker_buf));
	return datepicker_buf;
}
