// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define IB_MAX_ICONS   1280
#define IB_ICON_NAME   64
#define IB_WIN_W       640
#define IB_WIN_H       480

enum {
	IB_ID_WINDOW   = 1,
	IB_ID_TABS     = 2,
	IB_ID_ITEMVIEW = 3,
	IB_ID_TAB_BASE = 10,
};

static const char *ib_categories[] = {
	"A", "B", "C", "D-E", "F", "G-L", "M-N", "O-P", "Q-S", "T-Z",
};
#define IB_CAT_COUNT (sizeof(ib_categories) / sizeof(ib_categories[0]))

static const char ib_cat_start[] = { 'a', 'b', 'c', 'd', 'f', 'g', 'm', 'o', 'q', 't' };
static const char ib_cat_end[]   = { 'a', 'b', 'c', 'e', 'f', 'l', 'n', 'p', 's', 'z' };

struct ib_state {
	char names[IB_MAX_ICONS][IB_ICON_NAME];
	uint32_t count;

	uint32_t confirmed;
	char result[IB_ICON_NAME];
	int32_t prev_selected;
	uint32_t saved_icon_count;
	uint32_t saved_pixels_used;
};

static struct ib_state ib;

// [=]===^=[ ib_scan_category ]=====================================[=]
static void ib_scan_category(uint32_t cat) {
	ib.count = 0;
	icon_count = ib.saved_icon_count;
	icon_pixels_used = ib.saved_pixels_used;

	if(cat >= IB_CAT_COUNT) {
		return;
	}

	char lo = ib_cat_start[cat];
	char hi = ib_cat_end[cat];

	for(uint32_t i = 0; i < mdi_icon_count && ib.count < IB_MAX_ICONS; ++i) {
		const char *name = mdi_name_block + mdi_name_offsets[i];
		char first = name[0];
		if(first < lo) {
			continue;
		}
		if(first > hi) {
			break;
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

// [=]===^=[ mkgui_icon_browser ]===================================[=]
static uint32_t mkgui_icon_browser(struct mkgui_ctx *ctx, char *out, uint32_t out_size) {
	memset(&ib, 0, sizeof(ib));
	ib.result[0] = '\0';
	ib.prev_selected = -1;
	ib.saved_icon_count = icon_count;
	ib.saved_pixels_used = icon_pixels_used;

	popup_destroy_all(ctx);

	uint32_t tab_count = IB_CAT_COUNT;
	uint32_t wcount = 3 + tab_count;
	struct mkgui_widget widgets[3 + IB_CAT_COUNT];

	widgets[0] = (struct mkgui_widget){ MKGUI_WINDOW, IB_ID_WINDOW, "Icon Browser", "", 0, 0, 0, IB_WIN_W, IB_WIN_H, 0 };
	widgets[1] = (struct mkgui_widget){ MKGUI_TABS, IB_ID_TABS, "", "", IB_ID_WINDOW, 0, 0, 0, 0, MKGUI_ANCHOR_LEFT | MKGUI_ANCHOR_TOP | MKGUI_ANCHOR_RIGHT | MKGUI_ANCHOR_BOTTOM };
	for(uint32_t i = 0; i < tab_count; ++i) {
		widgets[2 + i] = (struct mkgui_widget){ MKGUI_TAB, IB_ID_TAB_BASE + i, "", "", IB_ID_TABS, 0, 0, 0, 0, 0 };
		strncpy(widgets[2 + i].label, ib_categories[i], MKGUI_MAX_TEXT - 1);
	}
	widgets[2 + tab_count] = (struct mkgui_widget){ MKGUI_ITEMVIEW, IB_ID_ITEMVIEW, "", "", IB_ID_TAB_BASE, 0, 0, 0, 0, MKGUI_ANCHOR_LEFT | MKGUI_ANCHOR_TOP | MKGUI_ANCHOR_RIGHT | MKGUI_ANCHOR_BOTTOM };

	struct mkgui_ctx *dlg = mkgui_create_child(ctx, widgets, wcount, "Icon Browser", IB_WIN_W, IB_WIN_H);
	if(!dlg) {
		return 0;
	}

	ib_scan_category(0);
	mkgui_itemview_setup(dlg, IB_ID_ITEMVIEW, ib.count, MKGUI_VIEW_ICON, ib_label_cb, ib_icon_cb, NULL);

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

				case MKGUI_EVENT_TAB_CHANGED: {
					uint32_t cat = (uint32_t)ev.value - IB_ID_TAB_BASE;
					if(cat < IB_CAT_COUNT) {
						int32_t iv_idx = find_widget_idx(dlg, IB_ID_ITEMVIEW);
						if(iv_idx >= 0) {
							dlg->widgets[iv_idx].parent_id = (uint32_t)ev.value;
						}
						ib_scan_category(cat);
						mkgui_itemview_setup(dlg, IB_ID_ITEMVIEW, ib.count, MKGUI_VIEW_ICON, ib_label_cb, ib_icon_cb, NULL);
						ib.prev_selected = -1;
					}
				} break;

				case MKGUI_EVENT_ITEMVIEW_SELECT: {
					if(ev.value == ib.prev_selected && ev.value >= 0 && ev.value < (int32_t)ib.count) {
						strncpy(ib.result, ib.names[ev.value], IB_ICON_NAME - 1);
						ib.result[IB_ICON_NAME - 1] = '\0';
						ib.confirmed = 1;
						running = 0;
					}
					ib.prev_selected = ev.value;
				} break;

				default: break;
			}
		}
		mkgui_wait(dlg);
	}

	mkgui_destroy_child(dlg);

	icon_count = ib.saved_icon_count;
	icon_pixels_used = ib.saved_pixels_used;

	if(ib.confirmed && ib.result[0] != '\0') {
		icon_resolve(ib.result);
	}

	dirty_all(ctx);

	if(ib.confirmed && ib.result[0] != '\0') {
		strncpy(out, ib.result, out_size - 1);
		out[out_size - 1] = '\0';
		return 1;
	}
	return 0;
}
