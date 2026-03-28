// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// ---------------------------------------------------------------------------
// Dialog system - professional modal dialogs with icons and text wrapping
// ---------------------------------------------------------------------------

#define DLG_PAD           16
#define DLG_ICON_GAP      16
#define DLG_LINE_GAP      3
#define DLG_TEXT_BTN_GAP  16
#define DLG_BTN_H         28
#define DLG_BTN_W         80
#define DLG_BTN_GAP       8
#define DLG_BTN_PAD       12
#define DLG_MIN_W         300
#define DLG_MAX_W         500
#define DLG_MAX_LINES     32
#define DLG_LINE_LEN      256
#define DLG_INPUT_H       24
#define DLG_INPUT_GAP     8

enum {
	MKGUI_DLG_WINDOW     = 0xfe00,
	MKGUI_DLG_LABEL_BASE = 0xfe01,
	MKGUI_DLG_INPUT      = 0xfe30,
	MKGUI_DLG_VBOX       = 0xfe38,
	MKGUI_DLG_CONTENT_HBOX = 0xfe3c,
	MKGUI_DLG_ICON_PANEL = 0xfe3d,
	MKGUI_DLG_TEXT_VBOX  = 0xfe3e,
	MKGUI_DLG_DIVIDER    = 0xfe3f,
	MKGUI_DLG_BTN_HBOX   = 0xfe39,
	MKGUI_DLG_SPACER     = 0xfe3a,
	MKGUI_DLG_BTN_1      = 0xfe40,
	MKGUI_DLG_BTN_2      = 0xfe41,
	MKGUI_DLG_BTN_3      = 0xfe42,
};

struct dlg_icon_def {
	char *name;
	uint32_t color;
};

struct dlg_btn_def {
	char *label;
	uint32_t result;
};

struct dlg_render_state {
	int32_t icon_idx;
	int32_t icon_x;
	int32_t icon_y;
	int32_t sep_y;
};

static struct dlg_render_state dlg_rs;
static uint32_t dlg_btn_results[3];
static uint32_t dlg_enter_result;
static uint32_t dlg_escape_result;

static struct dlg_icon_def dlg_icon_defs[] = {
	{ NULL,                  0        },
	{ "dialog-information",  0x3daee9 },
	{ "dialog-warning",      0xf67400 },
	{ "dialog-error",        0xda4453 },
	{ "help-browser",        0x3daee9 },
};

static struct dlg_btn_def dlg_button_defs[5][3] = {
	{ {"OK",    MKGUI_DLG_RESULT_OK},    {NULL, 0},                           {NULL, 0}                            },
	{ {"OK",    MKGUI_DLG_RESULT_OK},    {"Cancel", MKGUI_DLG_RESULT_CANCEL}, {NULL, 0}                            },
	{ {"Yes",   MKGUI_DLG_RESULT_YES},   {"No",     MKGUI_DLG_RESULT_NO},    {NULL, 0}                            },
	{ {"Yes",   MKGUI_DLG_RESULT_YES},   {"No",     MKGUI_DLG_RESULT_NO},    {"Cancel", MKGUI_DLG_RESULT_CANCEL}  },
	{ {"Retry", MKGUI_DLG_RESULT_RETRY}, {"Cancel", MKGUI_DLG_RESULT_CANCEL},{NULL, 0}                            },
};

// [=]===^=[ dlg_render ]=============================================[=]
static void dlg_render(struct mkgui_ctx *ctx, void *userdata) {
	(void)userdata;
	if(dlg_rs.icon_idx >= 0) {
		int32_t ii = find_widget_idx(ctx, MKGUI_DLG_ICON_PANEL);
		if(ii >= 0) {
			int32_t rx = ctx->rects[ii].x;
			int32_t ry = ctx->rects[ii].y;
			int32_t rw = ctx->rects[ii].w;
			int32_t rh = ctx->rects[ii].h;
			int32_t ix = rx + (rw - icons[dlg_rs.icon_idx].w) / 2;
			int32_t iy = ry + (rh - icons[dlg_rs.icon_idx].h) / 2;
			draw_icon(ctx->pixels, ctx->win_w, ctx->win_h, &icons[dlg_rs.icon_idx],
				ix, iy, 0, 0, ctx->win_w, ctx->win_h);
		}
	}
}

// [=]===^=[ dlg_icon_resolve ]=======================================[=]
static int32_t dlg_icon_resolve(struct mkgui_ctx *ctx, uint32_t icon_type) {
	if(icon_type == 0 || icon_type >= sizeof(dlg_icon_defs) / sizeof(dlg_icon_defs[0])) {
		return -1;
	}
	char *name = dlg_icon_defs[icon_type].name;
	char cache_name[MKGUI_ICON_NAME_LEN];
	snprintf(cache_name, sizeof(cache_name), "dlg:%s", name);
	int32_t idx = icon_find_idx(cache_name);
	if(idx >= 0) {
		return idx;
	}
	struct mkgui_svg_source *src = svg_find_source(name);
	if(src) {
		return svg_rasterize_icon_ex(cache_name, src->svg_data, src->svg_len, ctx->dialog_icon_size, 0, 0);
	}
	return icon_find_idx(name);
}

// [=]===^=[ dlg_wrap_text ]==========================================[=]
static uint32_t dlg_wrap_text(struct mkgui_ctx *ctx, char *text, int32_t max_w, char lines[][DLG_LINE_LEN], uint32_t max_lines) {
	uint32_t lc = 0;
	char *p = text;

	while(lc < max_lines && *p) {
		uint32_t li = 0;
		int32_t w = 0;
		uint32_t last_break = 0;
		char *last_break_src = NULL;
		uint32_t broke_at_width = 0;

		char *s = p;
		while(*s && *s != '\n' && li < DLG_LINE_LEN - 1) {
			uint32_t ch = (uint8_t)*s;
			int32_t cw = 0;
			if(ch >= MKGUI_GLYPH_FIRST && ch <= MKGUI_GLYPH_LAST) {
				cw = ctx->glyphs[ch - MKGUI_GLYPH_FIRST].advance;
			}
			if(w + cw > max_w && li > 0) {
				broke_at_width = 1;
				break;
			}

			lines[lc][li++] = *s;
			w += cw;
			++s;

			if(*(s - 1) == ' ') {
				last_break = li;
				last_break_src = s;
			}
		}

		if(broke_at_width && last_break_src) {
			li = last_break;
			s = last_break_src;
		}

		while(li > 0 && lines[lc][li - 1] == ' ') {
			--li;
		}
		lines[lc][li] = '\0';
		++lc;

		p = s;
		if(broke_at_width) {
			while(*p == ' ') {
				++p;
			}

		} else if(*p == '\n') {
			++p;
		}
	}

	if(lc == 0) {
		lines[0][0] = '\0';
		lc = 1;
	}

	return lc;
}

// [=]===^=[ dlg_setup_buttons ]======================================[=]
static uint32_t dlg_setup_buttons(uint32_t buttons) {
	if(buttons > MKGUI_DLG_BUTTONS_RETRY_CANCEL) {
		buttons = MKGUI_DLG_BUTTONS_OK;
	}

	uint32_t btn_count = 0;
	for(uint32_t i = 0; i < 3; ++i) {
		dlg_btn_results[i] = MKGUI_DLG_RESULT_NONE;
		if(dlg_button_defs[buttons][i].label) {
			dlg_btn_results[i] = dlg_button_defs[buttons][i].result;
			++btn_count;
		}
	}

	dlg_enter_result = dlg_button_defs[buttons][0].result;
	dlg_escape_result = MKGUI_DLG_RESULT_NONE;
	for(uint32_t i = 0; i < 3; ++i) {
		if(!dlg_button_defs[buttons][i].label) {
			break;
		}
		uint32_t r = dlg_button_defs[buttons][i].result;
		if(r == MKGUI_DLG_RESULT_CANCEL || r == MKGUI_DLG_RESULT_NO) {
			dlg_escape_result = r;
		}
	}

	return btn_count;
}

// [=]===^=[ dlg_run ]================================================[=]
static uint32_t dlg_run(struct mkgui_ctx *parent, struct mkgui_widget *widgets, uint32_t count, char *title, int32_t w, int32_t h) {
	struct mkgui_ctx *dlg = mkgui_create_child(parent, widgets, count, title, w, h);
	if(!dlg) {
		return MKGUI_DLG_RESULT_NONE;
	}

	dlg->render_cb = dlg_render;

	uint32_t result = MKGUI_DLG_RESULT_NONE;
	struct mkgui_event ev;
	uint32_t running = 1;
	while(running) {
		while(mkgui_poll(dlg, &ev)) {
			switch(ev.type) {
				case MKGUI_EVENT_CLOSE: {
					running = 0;
				} break;

				case MKGUI_EVENT_CLICK: {
					if(ev.id == MKGUI_DLG_BTN_1) {
						result = dlg_btn_results[0];
						running = 0;

					} else if(ev.id == MKGUI_DLG_BTN_2) {
						result = dlg_btn_results[1];
						running = 0;

					} else if(ev.id == MKGUI_DLG_BTN_3) {
						result = dlg_btn_results[2];
						running = 0;
					}
				} break;

				case MKGUI_EVENT_KEY: {
					if(ev.keysym == MKGUI_KEY_ESCAPE) {
						result = dlg_escape_result;
						running = 0;

					} else if(ev.keysym == MKGUI_KEY_RETURN) {
						result = dlg_enter_result;
						running = 0;
					}
				} break;

				default: break;
			}
		}
		mkgui_wait(dlg);
	}

	mkgui_destroy_child(dlg);
	dirty_all(parent);
	return result;
}

// [=]===^=[ mkgui_message_box ]======================================[=]
MKGUI_API uint32_t mkgui_message_box(struct mkgui_ctx *ctx, char *title, char *message, uint32_t icon_type, uint32_t buttons) {
	MKGUI_CHECK_VAL(ctx, 0);
	if(!title) {
		title = "";
	}
	if(!message) {
		message = "";
	}
	int32_t icon_idx = dlg_icon_resolve(ctx, icon_type);
	int32_t icon_w = 0;
	int32_t icon_h = 0;
	if(icon_idx >= 0) {
		icon_w = icons[icon_idx].w;
		icon_h = icons[icon_idx].h;
	}

	int32_t icon_area = (icon_idx >= 0) ? icon_w + DLG_ICON_GAP : 0;
	int32_t text_max_w = DLG_MAX_W - DLG_PAD * 2 - icon_area;

	char lines[DLG_MAX_LINES][DLG_LINE_LEN];
	uint32_t line_count = dlg_wrap_text(ctx, message, text_max_w, lines, DLG_MAX_LINES);

	int32_t max_line_w = 0;
	for(uint32_t i = 0; i < line_count; ++i) {
		int32_t lw = text_width(ctx, lines[i]);
		if(lw > max_line_w) {
			max_line_w = lw;
		}
	}

	uint32_t btn_count = dlg_setup_buttons(buttons);
	int32_t btn_area_w = (int32_t)btn_count * DLG_BTN_W + ((int32_t)btn_count - 1) * DLG_BTN_GAP + DLG_BTN_PAD * 2;

	int32_t content_w = icon_area + max_line_w;
	int32_t dw = content_w + DLG_PAD * 2;
	if(dw < btn_area_w) {
		dw = btn_area_w;
	}
	if(dw < DLG_MIN_W) {
		dw = DLG_MIN_W;
	}
	if(dw > DLG_MAX_W) {
		dw = DLG_MAX_W;
	}

	int32_t text_h = (int32_t)line_count * (ctx->font_height + DLG_LINE_GAP) - DLG_LINE_GAP;
	int32_t content_h = (icon_h > text_h) ? icon_h : text_h;
	int32_t dh = DLG_PAD + content_h + DLG_TEXT_BTN_GAP + DLG_BTN_H + DLG_PAD;

	struct mkgui_widget widgets[DLG_MAX_LINES + 12];
	uint32_t wi = 0;

	widgets[wi] = (struct mkgui_widget){ MKGUI_WINDOW, MKGUI_DLG_WINDOW, "", "", 0, dw, dh, 0, 0, 0 };
	strncpy(widgets[wi].label, title, MKGUI_MAX_TEXT - 1);
	++wi;

	widgets[wi] = (struct mkgui_widget){ MKGUI_VBOX, MKGUI_DLG_VBOX, "", "", MKGUI_DLG_WINDOW, 0, 0, 0, 0, 0 };
	++wi;

	widgets[wi] = (struct mkgui_widget){ MKGUI_HBOX, MKGUI_DLG_CONTENT_HBOX, "", "", MKGUI_DLG_VBOX, 0, 0, 0, 0, 1 };
	++wi;

	if(icon_idx >= 0) {
		widgets[wi] = (struct mkgui_widget){ MKGUI_PANEL, MKGUI_DLG_ICON_PANEL, "", "", MKGUI_DLG_CONTENT_HBOX,
			icon_w + DLG_ICON_GAP, 0, MKGUI_FIXED, 0, 0 };
		++wi;
	}

	widgets[wi] = (struct mkgui_widget){ MKGUI_VBOX, MKGUI_DLG_TEXT_VBOX, "", "", MKGUI_DLG_CONTENT_HBOX, 0, 0, MKGUI_NO_PAD, 0, 1 };
	++wi;

	for(uint32_t i = 0; i < line_count; ++i) {
		widgets[wi] = (struct mkgui_widget){ MKGUI_LABEL, MKGUI_DLG_LABEL_BASE + i, "", "", MKGUI_DLG_TEXT_VBOX,
			0, ctx->font_height, MKGUI_FIXED, 0, 0 };
		strncpy(widgets[wi].label, lines[i], MKGUI_MAX_TEXT - 1);
		++wi;
	}

	widgets[wi] = (struct mkgui_widget){ MKGUI_DIVIDER, MKGUI_DLG_DIVIDER, "", "", MKGUI_DLG_VBOX, 0, 0, MKGUI_FIXED, 0, 0 };
	++wi;

	widgets[wi] = (struct mkgui_widget){ MKGUI_HBOX, MKGUI_DLG_BTN_HBOX, "", "", MKGUI_DLG_VBOX, 0, DLG_BTN_H, MKGUI_FIXED, 0, 0 };
	++wi;

	widgets[wi] = (struct mkgui_widget){ MKGUI_LABEL, MKGUI_DLG_SPACER, "", "", MKGUI_DLG_BTN_HBOX, 0, 0, 0, 0, 1 };
	++wi;

	for(uint32_t i = 0; i < btn_count; ++i) {
		widgets[wi] = (struct mkgui_widget){ MKGUI_BUTTON, MKGUI_DLG_BTN_1 + i, "", "", MKGUI_DLG_BTN_HBOX,
			DLG_BTN_W, 0, MKGUI_FIXED, 0, 0 };
		strncpy(widgets[wi].label, dlg_button_defs[buttons][i].label, MKGUI_MAX_TEXT - 1);
		++wi;
	}

	dlg_rs.icon_idx = icon_idx;

	return dlg_run(ctx, widgets, wi, title, dw, dh);
}

// [=]===^=[ mkgui_confirm_dialog ]==================================[=]
MKGUI_API uint32_t mkgui_confirm_dialog(struct mkgui_ctx *ctx, char *title, char *message) {
	MKGUI_CHECK_VAL(ctx, 0);
	if(!title) {
		title = "";
	}
	if(!message) {
		message = "";
	}
	uint32_t result = mkgui_message_box(ctx, title, message, MKGUI_DLG_ICON_QUESTION, MKGUI_DLG_BUTTONS_YES_NO);
	return (result == MKGUI_DLG_RESULT_YES) ? 1 : 0;
}

// [=]===^=[ mkgui_input_dialog ]====================================[=]
MKGUI_API uint32_t mkgui_input_dialog(struct mkgui_ctx *ctx, char *title, char *prompt, char *default_text, char *out, uint32_t out_size) {
	MKGUI_CHECK_VAL(ctx, 0);
	if(!title) {
		title = "";
	}
	if(!prompt) {
		prompt = "";
	}
	if(!out || out_size == 0) {
		return 0;
	}
	int32_t icon_idx = dlg_icon_resolve(ctx, MKGUI_DLG_ICON_QUESTION);
	int32_t icon_w = 0;
	int32_t icon_h = 0;
	if(icon_idx >= 0) {
		icon_w = icons[icon_idx].w;
		icon_h = icons[icon_idx].h;
	}

	int32_t icon_area = (icon_idx >= 0) ? icon_w + DLG_ICON_GAP : 0;
	int32_t text_max_w = DLG_MAX_W - DLG_PAD * 2 - icon_area;

	char lines[DLG_MAX_LINES][DLG_LINE_LEN];
	uint32_t line_count = dlg_wrap_text(ctx, prompt, text_max_w, lines, DLG_MAX_LINES);

	int32_t max_line_w = 0;
	for(uint32_t i = 0; i < line_count; ++i) {
		int32_t lw = text_width(ctx, lines[i]);
		if(lw > max_line_w) {
			max_line_w = lw;
		}
	}

	int32_t text_h = (int32_t)line_count * (ctx->font_height + DLG_LINE_GAP) - DLG_LINE_GAP;
	int32_t text_icon_h = (icon_h > text_h) ? icon_h : text_h;
	int32_t content_h = text_icon_h + DLG_INPUT_GAP + DLG_INPUT_H;

	int32_t content_w = icon_area + max_line_w;
	int32_t dw = content_w + DLG_PAD * 2;
	int32_t btn_area_w = 2 * DLG_BTN_W + DLG_BTN_GAP + DLG_BTN_PAD * 2;
	if(dw < btn_area_w) {
		dw = btn_area_w;
	}
	if(dw < DLG_MIN_W) {
		dw = DLG_MIN_W;
	}
	if(dw > DLG_MAX_W) {
		dw = DLG_MAX_W;
	}

	int32_t dh = DLG_PAD + content_h + DLG_TEXT_BTN_GAP + DLG_BTN_H + DLG_PAD;

	struct mkgui_widget widgets[DLG_MAX_LINES + 10];
	uint32_t wi = 0;

	widgets[wi] = (struct mkgui_widget){ MKGUI_WINDOW, MKGUI_DLG_WINDOW, "", "", 0, dw, dh, 0, 0, 0 };
	strncpy(widgets[wi].label, title, MKGUI_MAX_TEXT - 1);
	++wi;

	widgets[wi] = (struct mkgui_widget){ MKGUI_VBOX, MKGUI_DLG_VBOX, "", "", MKGUI_DLG_WINDOW, 0, 0, 0, 0, 0 };
	++wi;

	widgets[wi] = (struct mkgui_widget){ MKGUI_HBOX, MKGUI_DLG_CONTENT_HBOX, "", "", MKGUI_DLG_VBOX, 0, 0, 0, 0, 1 };
	++wi;

	if(icon_idx >= 0) {
		widgets[wi] = (struct mkgui_widget){ MKGUI_PANEL, MKGUI_DLG_ICON_PANEL, "", "", MKGUI_DLG_CONTENT_HBOX,
			icon_w + DLG_ICON_GAP, 0, MKGUI_FIXED, 0, 0 };
		++wi;
	}

	widgets[wi] = (struct mkgui_widget){ MKGUI_VBOX, MKGUI_DLG_TEXT_VBOX, "", "", MKGUI_DLG_CONTENT_HBOX, 0, 0, MKGUI_NO_PAD, 0, 1 };
	++wi;

	for(uint32_t i = 0; i < line_count; ++i) {
		widgets[wi] = (struct mkgui_widget){ MKGUI_LABEL, MKGUI_DLG_LABEL_BASE + i, "", "", MKGUI_DLG_TEXT_VBOX,
			0, ctx->font_height, MKGUI_FIXED, 0, 0 };
		strncpy(widgets[wi].label, lines[i], MKGUI_MAX_TEXT - 1);
		++wi;
	}

	widgets[wi] = (struct mkgui_widget){ MKGUI_INPUT, MKGUI_DLG_INPUT, "", "", MKGUI_DLG_TEXT_VBOX,
		0, DLG_INPUT_H, MKGUI_FIXED, 0, 0 };
	++wi;

	widgets[wi] = (struct mkgui_widget){ MKGUI_DIVIDER, MKGUI_DLG_DIVIDER, "", "", MKGUI_DLG_VBOX, 0, 0, MKGUI_FIXED, 0, 0 };
	++wi;

	widgets[wi] = (struct mkgui_widget){ MKGUI_HBOX, MKGUI_DLG_BTN_HBOX, "", "", MKGUI_DLG_VBOX, 0, DLG_BTN_H, MKGUI_FIXED, 0, 0 };
	++wi;

	widgets[wi] = (struct mkgui_widget){ MKGUI_LABEL, MKGUI_DLG_SPACER, "", "", MKGUI_DLG_BTN_HBOX, 0, 0, 0, 0, 1 };
	++wi;

	widgets[wi] = (struct mkgui_widget){ MKGUI_BUTTON, MKGUI_DLG_BTN_1, "OK", "", MKGUI_DLG_BTN_HBOX,
		DLG_BTN_W, 0, MKGUI_FIXED, 0, 0 };
	++wi;

	widgets[wi] = (struct mkgui_widget){ MKGUI_BUTTON, MKGUI_DLG_BTN_2, "Cancel", "", MKGUI_DLG_BTN_HBOX,
		DLG_BTN_W, 0, MKGUI_FIXED, 0, 0 };
	++wi;

	dlg_rs.icon_idx = icon_idx;

	struct mkgui_ctx *dlg = mkgui_create_child(ctx, widgets, wi, title, dw, dh);
	if(!dlg) {
		return 0;
	}

	dlg->render_cb = dlg_render;

	if(default_text) {
		mkgui_input_set(dlg, MKGUI_DLG_INPUT, default_text);
	}
	dlg->focus_id = MKGUI_DLG_INPUT;

	uint32_t result = 0;
	struct mkgui_event ev;
	uint32_t running = 1;
	while(running) {
		while(mkgui_poll(dlg, &ev)) {
			switch(ev.type) {
				case MKGUI_EVENT_CLOSE: {
					running = 0;
				} break;

				case MKGUI_EVENT_CLICK: {
					if(ev.id == MKGUI_DLG_BTN_1) {
						result = 1;
						running = 0;

					} else if(ev.id == MKGUI_DLG_BTN_2) {
						running = 0;
					}
				} break;

				case MKGUI_EVENT_KEY: {
					if(ev.keysym == MKGUI_KEY_ESCAPE) {
						running = 0;

					} else if(ev.keysym == MKGUI_KEY_RETURN) {
						result = 1;
						running = 0;
					}
				} break;

				case MKGUI_EVENT_INPUT_SUBMIT: {
					result = 1;
					running = 0;
				} break;

				default: break;
			}
		}
		mkgui_wait(dlg);
	}

	if(result) {
		char *val = mkgui_input_get(dlg, MKGUI_DLG_INPUT);
		if(val && out && out_size > 0) {
			strncpy(out, val, out_size - 1);
			out[out_size - 1] = '\0';
		}
	}

	mkgui_destroy_child(dlg);
	dirty_all(ctx);
	return result;
}
