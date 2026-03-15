// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// ---------------------------------------------------------------------------
// Simple dialog helpers
// ---------------------------------------------------------------------------

enum {
	MKGUI_DLG_WINDOW = 0xfe00,
	MKGUI_DLG_LABEL,
	MKGUI_DLG_ICON,
	MKGUI_DLG_INPUT,
	MKGUI_DLG_BTN_OK,
	MKGUI_DLG_BTN_CANCEL,
};

// [=]===^=[ dlg_run ]=============================================[=]
static int32_t dlg_run(struct mkgui_ctx *parent, struct mkgui_widget *widgets, uint32_t count, const char *title, int32_t w, int32_t h) {
	struct mkgui_ctx *dlg = mkgui_create_child(parent, widgets, count, title, w, h);
	if(!dlg) {
		return 0;
	}

	int32_t result = 0;
	struct mkgui_event ev;
	uint32_t running = 1;
	while(running) {
		while(mkgui_poll(dlg, &ev)) {
			switch(ev.type) {
				case MKGUI_EVENT_CLOSE: {
					running = 0;
				} break;

				case MKGUI_EVENT_CLICK: {
					if(ev.id == MKGUI_DLG_BTN_OK) {
						result = 1;
						running = 0;

					} else if(ev.id == MKGUI_DLG_BTN_CANCEL) {
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

				default: break;
			}
		}
	}

	mkgui_destroy_child(dlg);
	dirty_all(parent);
	return result;
}

// [=]===^=[ mkgui_message_box ]===================================[=]
static void mkgui_message_box(struct mkgui_ctx *ctx, const char *title, const char *message) {
	int32_t tw = text_width(ctx, message);
	int32_t dw = tw + 60;
	if(dw < 250) { dw = 250; }
	if(dw > 600) { dw = 600; }
	int32_t dh = 110;

	struct mkgui_widget widgets[] = {
		{ MKGUI_WINDOW, MKGUI_DLG_WINDOW, "", "", 0, 0, 0, dw, dh, 0 },
		{ MKGUI_LABEL, MKGUI_DLG_LABEL, "", "", MKGUI_DLG_WINDOW, 20, 16, dw - 40, 24, 0 },
		{ MKGUI_BUTTON, MKGUI_DLG_BTN_OK, "OK", "", MKGUI_DLG_WINDOW, 40, 12, 80, 28, MKGUI_ANCHOR_BOTTOM | MKGUI_ANCHOR_RIGHT },
	};

	strncpy(widgets[0].label, title, MKGUI_MAX_TEXT - 1);
	strncpy(widgets[1].label, message, MKGUI_MAX_TEXT - 1);

	dlg_run(ctx, widgets, sizeof(widgets) / sizeof(widgets[0]), title, dw, dh);
}

// [=]===^=[ mkgui_confirm_dialog ]================================[=]
static uint32_t mkgui_confirm_dialog(struct mkgui_ctx *ctx, const char *title, const char *message) {
	int32_t tw = text_width(ctx, message);
	int32_t dw = tw + 60;
	if(dw < 300) { dw = 300; }
	if(dw > 600) { dw = 600; }
	int32_t dh = 110;

	struct mkgui_widget widgets[] = {
		{ MKGUI_WINDOW, MKGUI_DLG_WINDOW, "", "", 0, 0, 0, dw, dh, 0 },
		{ MKGUI_LABEL, MKGUI_DLG_LABEL, "", "", MKGUI_DLG_WINDOW, 20, 16, dw - 40, 24, 0 },
		{ MKGUI_BUTTON, MKGUI_DLG_BTN_OK, "OK", "", MKGUI_DLG_WINDOW, 100, 12, 80, 28, MKGUI_ANCHOR_BOTTOM | MKGUI_ANCHOR_RIGHT },
		{ MKGUI_BUTTON, MKGUI_DLG_BTN_CANCEL, "Cancel", "", MKGUI_DLG_WINDOW, 12, 12, 80, 28, MKGUI_ANCHOR_BOTTOM | MKGUI_ANCHOR_RIGHT },
	};

	strncpy(widgets[0].label, title, MKGUI_MAX_TEXT - 1);
	strncpy(widgets[1].label, message, MKGUI_MAX_TEXT - 1);

	return (uint32_t)dlg_run(ctx, widgets, sizeof(widgets) / sizeof(widgets[0]), title, dw, dh);
}

// [=]===^=[ mkgui_input_dialog ]=================================[=]
static uint32_t mkgui_input_dialog(struct mkgui_ctx *ctx, const char *title, const char *prompt, const char *default_text, char *out, uint32_t out_size) {
	int32_t tw = text_width(ctx, prompt);
	int32_t dw = tw + 250;
	if(dw < 350) { dw = 350; }
	if(dw > 600) { dw = 600; }
	int32_t dh = 110;
	int32_t lbl_w = tw + 10;

	struct mkgui_widget widgets[] = {
		{ MKGUI_WINDOW, MKGUI_DLG_WINDOW, "", "", 0, 0, 0, dw, dh, 0 },
		{ MKGUI_LABEL, MKGUI_DLG_LABEL, "", "", MKGUI_DLG_WINDOW, 12, 18, lbl_w, 24, 0 },
		{ MKGUI_INPUT, MKGUI_DLG_INPUT, "", "", MKGUI_DLG_WINDOW, 12 + lbl_w, 16, 20, 24, MKGUI_ANCHOR_LEFT | MKGUI_ANCHOR_RIGHT },
		{ MKGUI_BUTTON, MKGUI_DLG_BTN_OK, "OK", "", MKGUI_DLG_WINDOW, 100, 12, 80, 28, MKGUI_ANCHOR_BOTTOM | MKGUI_ANCHOR_RIGHT },
		{ MKGUI_BUTTON, MKGUI_DLG_BTN_CANCEL, "Cancel", "", MKGUI_DLG_WINDOW, 12, 12, 80, 28, MKGUI_ANCHOR_BOTTOM | MKGUI_ANCHOR_RIGHT },
	};

	strncpy(widgets[0].label, title, MKGUI_MAX_TEXT - 1);
	strncpy(widgets[1].label, prompt, MKGUI_MAX_TEXT - 1);

	struct mkgui_ctx *dlg = mkgui_create_child(ctx, widgets, sizeof(widgets) / sizeof(widgets[0]), title, dw, dh);
	if(!dlg) {
		return 0;
	}

	if(default_text) {
		mkgui_input_set(dlg, MKGUI_DLG_INPUT, default_text);
	}
	dlg->focus_id = MKGUI_DLG_INPUT;

	int32_t result = 0;
	struct mkgui_event ev;
	uint32_t running = 1;
	while(running) {
		while(mkgui_poll(dlg, &ev)) {
			switch(ev.type) {
				case MKGUI_EVENT_CLOSE: {
					running = 0;
				} break;

				case MKGUI_EVENT_CLICK: {
					if(ev.id == MKGUI_DLG_BTN_OK) {
						result = 1;
						running = 0;

					} else if(ev.id == MKGUI_DLG_BTN_CANCEL) {
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

				default: break;
			}
		}
	}

	if(result) {
		const char *val = mkgui_input_get(dlg, MKGUI_DLG_INPUT);
		if(val && out && out_size > 0) {
			strncpy(out, val, out_size - 1);
			out[out_size - 1] = '\0';
		}
	}

	mkgui_destroy_child(dlg);
	dirty_all(ctx);
	return (uint32_t)result;
}
