// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define MKGUI_SPINBOX_BTN_W 20

// [=]===^=[ spinbox_btn_hit ]====================================[=]
static int32_t spinbox_btn_hit(struct mkgui_ctx *ctx, uint32_t idx, int32_t mx, int32_t my) {
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;
	int32_t bx = rx + rw - MKGUI_SPINBOX_BTN_W;

	if(mx < bx || mx >= rx + rw) {
		return 0;
	}
	if(my < ry + rh / 2) {
		return 1;
	}
	return -1;
}

// [=]===^=[ spinbox_commit_edit ]=================================[=]
static void spinbox_commit_edit(struct mkgui_spinbox_data *sd) {
	if(!sd->editing) {
		return;
	}
	sd->editing = 0;
	sd->edit_buf[sd->edit_len] = '\0';
	if(sd->edit_len == 0 || (sd->edit_len == 1 && sd->edit_buf[0] == '-')) {
		return;
	}
	int32_t v = 0;
	uint32_t neg = 0;
	uint32_t i = 0;
	if(sd->edit_buf[0] == '-') {
		neg = 1;
		i = 1;
	}
	for(; i < sd->edit_len; ++i) {
		v = v * 10 + (sd->edit_buf[i] - '0');
	}
	if(neg) {
		v = -v;
	}
	if(v < sd->min_val) {
		v = sd->min_val;
	}
	if(v > sd->max_val) {
		v = sd->max_val;
	}
	sd->value = v;
}

// [=]===^=[ spinbox_focus_lost ]===================================[=]
static void spinbox_focus_lost(struct mkgui_ctx *ctx) {
	if(!ctx->focus_id) {
		return;
	}
	struct mkgui_spinbox_data *sd = find_spinbox_data(ctx, ctx->focus_id);
	if(sd && sd->editing) {
		spinbox_commit_edit(sd);
	}
}

// [=]===^=[ render_spinbox ]=====================================[=]
static void render_spinbox(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	uint32_t disabled = (w->flags & MKGUI_DISABLED);
	uint32_t focused = (ctx->focus_id == w->id);
	draw_patch(ctx, MKGUI_STYLE_SUNKEN, rx, ry, rw, rh, ctx->theme.input_bg, focused ? ctx->theme.splitter : ctx->theme.widget_border);

	struct mkgui_spinbox_data *sd = find_spinbox_data(ctx, w->id);
	uint32_t tc = disabled ? ctx->theme.text_disabled : ctx->theme.text;
	if(sd) {
		const char *display;
		char val_buf[64];
		if(sd->editing && focused) {
			sd->edit_buf[sd->edit_len] = '\0';
			display = sd->edit_buf;
		} else {
			snprintf(val_buf, sizeof(val_buf), "%d", sd->value);
			display = val_buf;
		}
		int32_t ty = ry + (rh - ctx->font_height) / 2;
		int32_t text_right = rx + rw - MKGUI_SPINBOX_BTN_W;
		if(sd->editing && focused && sd->select_all && sd->edit_len > 0) {
			int32_t sel_w = text_width(ctx, display);
			if(rx + 4 + sel_w > text_right) {
				sel_w = text_right - rx - 4;
			}
			if(sel_w > 0) {
				draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx + 4, ry + 2, sel_w, rh - 4, ctx->theme.selection);
				push_text_clip(rx + 4, ty, display, ctx->theme.sel_text, rx + 1, ry + 1, text_right, ry + rh - 1);
			}
		} else {
			push_text_clip(rx + 4, ty, display, tc, rx + 1, ry + 1, text_right, ry + rh - 1);
		}
		if(sd->editing && focused && !sd->select_all) {
			int32_t cur_x = rx + 4 + text_width(ctx, display);
			if(cur_x < text_right) {
				draw_vline(ctx->pixels, ctx->win_w, ctx->win_h, cur_x, ry + 3, rh - 6, tc);
			}
		}
	}

	int32_t bx = rx + rw - MKGUI_SPINBOX_BTN_W;
	int32_t half = rh / 2;
	int32_t hover_btn = (!disabled && ctx->hover_id == w->id && sd) ? sd->hover_btn : 0;

	uint32_t up_bg = (hover_btn == 1) ? ctx->theme.widget_hover : ctx->theme.widget_bg;
	draw_patch(ctx, MKGUI_STYLE_RAISED, bx, ry, MKGUI_SPINBOX_BTN_W, half, up_bg, ctx->theme.widget_border);

	int32_t acx = bx + MKGUI_SPINBOX_BTN_W / 2;
	int32_t acy = ry + half / 2 - 2;
	for(uint32_t j = 0; j < 4; ++j) {
		draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, acx - (int32_t)j, acy + (int32_t)j, 1 + (int32_t)j * 2, tc);
	}

	uint32_t dn_bg = (hover_btn == -1) ? ctx->theme.widget_hover : ctx->theme.widget_bg;
	draw_patch(ctx, MKGUI_STYLE_RAISED, bx, ry + half, MKGUI_SPINBOX_BTN_W, rh - half, dn_bg, ctx->theme.widget_border);

	int32_t acy2 = ry + half + (rh - half) / 2 - 2;
	for(uint32_t j = 0; j < 4; ++j) {
		draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, acx - (int32_t)(3 - j), acy2 + (int32_t)j, 1 + (int32_t)(3 - j) * 2, tc);
	}
}

// [=]===^=[ spinbox_adjust ]=====================================[=]
static uint32_t spinbox_adjust(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t widget_id, int32_t delta) {
	struct mkgui_spinbox_data *sd = find_spinbox_data(ctx, widget_id);
	if(!sd) {
		return 0;
	}
	int32_t new_val = sd->value + delta;
	if(new_val < sd->min_val) {
		new_val = sd->min_val;
	}
	if(new_val > sd->max_val) {
		new_val = sd->max_val;
	}
	if(new_val != sd->value) {
		sd->value = new_val;
		dirty_all(ctx);
		ev->type = MKGUI_EVENT_SPINBOX_CHANGED;
		ev->id = widget_id;
		ev->value = new_val;
		return 1;
	}
	return 0;
}

// [=]===^=[ handle_spinbox_key ]=================================[=]
static uint32_t handle_spinbox_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks, const char *text, int32_t text_len) {
	struct mkgui_spinbox_data *sd = find_spinbox_data(ctx, ctx->focus_id);
	if(!sd) {
		return 0;
	}

	if(ks == MKGUI_KEY_ESCAPE) {
		sd->editing = 0;
		dirty_all(ctx);
		return 1;
	}

	if(ks == MKGUI_KEY_RETURN || ks == MKGUI_KEY_TAB) {
		if(sd->editing) {
			spinbox_commit_edit(sd);
			sd->select_all = 0;
			ev->type = MKGUI_EVENT_SPINBOX_CHANGED;
			ev->id = ctx->focus_id;
			ev->value = sd->value;
		}
		ctx->focus_id = 0;
		dirty_all(ctx);
		return 1;
	}

	if(ks == MKGUI_KEY_LEFT || ks == MKGUI_KEY_RIGHT) {
		if(sd->select_all) {
			sd->select_all = 0;
			dirty_all(ctx);
			return 1;
		}
	}

	if(!sd->editing) {
		if(ks == MKGUI_KEY_UP) {
			return spinbox_adjust(ctx, ev, ctx->focus_id, sd->step);
		}
		if(ks == MKGUI_KEY_DOWN) {
			return spinbox_adjust(ctx, ev, ctx->focus_id, -sd->step);
		}
		if(ks == MKGUI_KEY_HOME) {
			return spinbox_adjust(ctx, ev, ctx->focus_id, sd->min_val - sd->value);
		}
		if(ks == MKGUI_KEY_END) {
			return spinbox_adjust(ctx, ev, ctx->focus_id, sd->max_val - sd->value);
		}
	}

	if(ks == MKGUI_KEY_BACKSPACE) {
		if(sd->editing) {
			if(sd->select_all) {
				sd->edit_len = 0;
				sd->select_all = 0;
			} else if(sd->edit_len > 0) {
				--sd->edit_len;
			}
			dirty_all(ctx);
			return 1;
		}
		return 0;
	}

	if(text_len > 0) {
		for(uint32_t i = 0; i < (uint32_t)text_len; ++i) {
			char c = text[i];
			uint32_t is_digit = (c >= '0' && c <= '9');
			uint32_t is_minus = (c == '-' && sd->min_val < 0);
			if(!is_digit && !is_minus) {
				continue;
			}
			if(!sd->editing) {
				sd->editing = 1;
				sd->edit_len = 0;
				sd->select_all = 0;
			}
			if(sd->select_all) {
				sd->edit_len = 0;
				sd->select_all = 0;
			}
			if(is_minus) {
				if(sd->edit_len == 0) {
					sd->edit_buf[sd->edit_len++] = '-';
				}
			} else {
				if(sd->edit_len < sizeof(sd->edit_buf) - 1) {
					sd->edit_buf[sd->edit_len++] = c;
				}
			}
		}
		dirty_all(ctx);
		return 1;
	}

	return 0;
}

// [=]===^=[ mkgui_spinbox_setup ]================================[=]
static void mkgui_spinbox_setup(struct mkgui_ctx *ctx, uint32_t id, int32_t min_val, int32_t max_val, int32_t value, int32_t step) {
	struct mkgui_spinbox_data *sd = find_spinbox_data(ctx, id);
	if(sd) {
		sd->min_val = min_val;
		sd->max_val = max_val;
		sd->value = value;
		sd->step = step > 0 ? step : 1;
		dirty_all(ctx);
	}
}

// [=]===^=[ mkgui_spinbox_get ]=================================[=]
static int32_t mkgui_spinbox_get(struct mkgui_ctx *ctx, uint32_t id) {
	struct mkgui_spinbox_data *sd = find_spinbox_data(ctx, id);
	return sd ? sd->value : 0;
}

// [=]===^=[ mkgui_spinbox_set ]=================================[=]
static void mkgui_spinbox_set(struct mkgui_ctx *ctx, uint32_t id, int32_t value) {
	struct mkgui_spinbox_data *sd = find_spinbox_data(ctx, id);
	if(sd) {
		sd->value = value;
		if(sd->value < sd->min_val) {
			sd->value = sd->min_val;
		}
		if(sd->value > sd->max_val) {
			sd->value = sd->max_val;
		}
		dirty_all(ctx);
	}
}
