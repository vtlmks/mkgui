// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define IB_MAX_ICONS   8192
#define IB_ICON_NAME   64
#define IB_WIN_W       500
#define IB_WIN_H       500

enum {
	IB_ID_WINDOW   = 1,
	IB_ID_VBOX,
	IB_ID_SEARCH,
	IB_ID_ITEMVIEW,
};

struct ib_state {
	char names[IB_MAX_ICONS][IB_ICON_NAME];
	uint32_t count;

	uint32_t confirmed;
	char result[IB_ICON_NAME];
	int32_t prev_selected;
	uint32_t saved_icon_count;
	uint32_t saved_pixels_used;
	struct mdi_pack *pack;
};

static struct ib_state ib;

// [=]===^=[ ib_scan ]==============================================[=]
static void ib_scan(const char *filter) {
	ib.count = 0;
	icon_count = ib.saved_icon_count;
	icon_pixels_used = ib.saved_pixels_used;

	uint32_t filter_len = filter ? (uint32_t)strlen(filter) : 0;

	for(uint32_t i = 0; i < ib.pack->icon_count && ib.count < IB_MAX_ICONS; ++i) {
		const char *name = ib.pack->name_block + ib.pack->name_offsets[i];
		if(filter_len > 0) {
			const char *p = name;
			uint32_t found = 0;
			while(*p) {
				uint32_t match = 1;
				for(uint32_t fi = 0; fi < filter_len; ++fi) {
					char fc = filter[fi];
					char nc = p[fi];
					if(nc == '\0') {
						match = 0;
						break;
					}
					if(fc >= 'A' && fc <= 'Z') {
						fc = (char)(fc + 32);
					}
					if(nc >= 'A' && nc <= 'Z') {
						nc = (char)(nc + 32);
					}
					if(fc != nc) {
						match = 0;
						break;
					}
				}
				if(match) {
					found = 1;
					break;
				}
				++p;
			}
			if(!found) {
				continue;
			}
		}
		strncpy(ib.names[ib.count], name, IB_ICON_NAME - 1);
		ib.names[ib.count][IB_ICON_NAME - 1] = '\0';
		++ib.count;
	}
}

// [=]===^=[ ib_label_cb ]==========================================[=]
static void ib_label_cb(uint32_t item, char *buf, uint32_t buf_size, void *userdata) {
	(void)userdata;
	if(item < ib.count) {
		strncpy(buf, ib.names[item], buf_size - 1);
		buf[buf_size - 1] = '\0';
	} else {
		buf[0] = '\0';
	}
}

// [=]===^=[ ib_icon_cb ]===========================================[=]
static void ib_icon_cb(uint32_t item, char *buf, uint32_t buf_size, void *userdata) {
	(void)userdata;
	if(item < ib.count) {
		strncpy(buf, ib.names[item], buf_size - 1);
		buf[buf_size - 1] = '\0';
	} else {
		buf[0] = '\0';
	}
}

// [=]===^=[ mkgui_icon_browser_pack ]===============================[=]
static uint32_t mkgui_icon_browser_pack(struct mkgui_ctx *ctx, struct mdi_pack *pack, char *out, uint32_t out_size) {
	memset(&ib, 0, sizeof(ib));
	ib.result[0] = '\0';
	ib.prev_selected = -1;
	ib.saved_icon_count = icon_count;
	ib.saved_pixels_used = icon_pixels_used;
	ib.pack = pack;

	struct mdi_pack saved_mdi = mdi;
	mdi = *pack;

	popup_destroy_all(ctx);

	struct mkgui_widget widgets[] = {
		{ MKGUI_WINDOW, IB_ID_WINDOW, "Icon Browser", "", 0, IB_WIN_W, IB_WIN_H, 0, 0, 0 },
		{ MKGUI_VBOX, IB_ID_VBOX, "", "", IB_ID_WINDOW, 0, 0, 0, 0, 0 },
		{ MKGUI_INPUT, IB_ID_SEARCH, "", "magnify", IB_ID_VBOX, 0, 24, MKGUI_FIXED, 0, 0 },
		{ MKGUI_ITEMVIEW, IB_ID_ITEMVIEW, "", "", IB_ID_VBOX, 0, 0, 0, 0, 1 },
	};
	uint32_t wcount = sizeof(widgets) / sizeof(widgets[0]);

	struct mkgui_ctx *dlg = mkgui_create_child(ctx, widgets, wcount, "Icon Browser", IB_WIN_W, IB_WIN_H);
	if(!dlg) {
		return 0;
	}

	ib_scan(NULL);
	mkgui_itemview_setup(dlg, IB_ID_ITEMVIEW, ib.count, MKGUI_VIEW_COMPACT, ib_label_cb, ib_icon_cb, NULL);
	mkgui_set_focus(dlg, IB_ID_SEARCH);

	uint32_t running = 1;
	struct mkgui_event ev;
	while(running) {
		while(mkgui_poll(dlg, &ev)) {
			switch(ev.type) {
				case MKGUI_EVENT_CLOSE: {
					running = 0;
				} break;

				case MKGUI_EVENT_KEY: {
					if(ev.keysym == MKGUI_KEY_ESCAPE) {
						running = 0;
					} else if(ev.keysym == MKGUI_KEY_RETURN) {
						struct mkgui_itemview_data *iv = find_itemview_data(dlg, IB_ID_ITEMVIEW);
						if(iv && iv->selected >= 0 && iv->selected < (int32_t)ib.count) {
							strncpy(ib.result, ib.names[iv->selected], IB_ICON_NAME - 1);
							ib.result[IB_ICON_NAME - 1] = '\0';
							ib.confirmed = 1;
							running = 0;
						}
					}
				} break;

				case MKGUI_EVENT_INPUT_CHANGED: {
					if(ev.id == IB_ID_SEARCH) {
						const char *filter = mkgui_input_get(dlg, IB_ID_SEARCH);
						ib_scan(filter);
						mkgui_itemview_set_items(dlg, IB_ID_ITEMVIEW, ib.count);
						mkgui_itemview_set_selected(dlg, IB_ID_ITEMVIEW, -1);
						ib.prev_selected = -1;
					}
				} break;

				case MKGUI_EVENT_ITEMVIEW_DBLCLICK: {
					if(ev.id == IB_ID_ITEMVIEW && ev.value >= 0 && ev.value < (int32_t)ib.count) {
						strncpy(ib.result, ib.names[ev.value], IB_ICON_NAME - 1);
						ib.result[IB_ICON_NAME - 1] = '\0';
						ib.confirmed = 1;
						running = 0;
					}
				} break;

				case MKGUI_EVENT_ITEMVIEW_SELECT: {
					ib.prev_selected = ev.value;
				} break;

				default: break;
			}
		}
		mkgui_wait(dlg);
	}

	mkgui_destroy_child(dlg);

	mdi = saved_mdi;
	icon_count = ib.saved_icon_count;
	icon_pixels_used = ib.saved_pixels_used;

	dirty_all(ctx);

	if(ib.confirmed && ib.result[0] != '\0') {
		strncpy(out, ib.result, out_size - 1);
		out[out_size - 1] = '\0';
		return 1;
	}
	return 0;
}

// [=]===^=[ mkgui_icon_browser ]===================================[=]
MKGUI_API uint32_t mkgui_icon_browser(struct mkgui_ctx *ctx, char *out, uint32_t out_size) {
	MKGUI_CHECK_VAL(ctx, 0);
	if(!out || out_size == 0) {
		return 0;
	}
	uint32_t r = mkgui_icon_browser_pack(ctx, &mdi, out, out_size);
	if(r) {
		icon_resolve(out);
	}
	return r;
}
