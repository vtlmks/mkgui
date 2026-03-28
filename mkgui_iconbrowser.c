// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define IB_ICON_NAME   64
#define IB_WIN_W       500
#define IB_WIN_H       500

// ---------------------------------------------------------------------------
// SVG theme icon browser
// ---------------------------------------------------------------------------

#define IB_THEME_MAX_ICONS   8192
#define IB_THEME_MAX_CATS    16

enum {
	IB_TH_WINDOW = 100,
	IB_TH_VBOX,
	IB_TH_SEARCH,
	IB_TH_TABS,
	IB_TH_TAB_ALL,
	IB_TH_TAB_FIRST,
};

static char *ib_theme_cat_names[] = {
	"actions", "places", "status", "devices", "mimetypes",
	"emblems", "panel", "emotes", NULL
};

struct ib_theme_state {
	char names[IB_THEME_MAX_ICONS][IB_ICON_NAME];
	char paths[IB_THEME_MAX_ICONS][1024];
	uint32_t cats[IB_THEME_MAX_ICONS];
	uint32_t total_count;

	uint32_t filtered_idx[IB_THEME_MAX_ICONS];
	uint32_t filtered_count;

	uint32_t active_cat;
	uint32_t confirmed;
	char result[IB_ICON_NAME];
	int32_t prev_selected;
	uint32_t saved_icon_count;

	char theme_dir[1024];
	int32_t size;
};

static struct ib_theme_state ibt;

// [=]===^=[ ibt_scan_category ]======================================[=]
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
static void ibt_scan_category(char *theme_dir, int32_t size, char *category, uint32_t cat_idx) {
	char dir_path[2048];
	snprintf(dir_path, sizeof(dir_path), "%s/%dx%d/%s", theme_dir, size, size, category);

	DIR *d = opendir(dir_path);
	if(!d) {
		snprintf(dir_path, sizeof(dir_path), "%s/%s/%d", theme_dir, category, size);
		d = opendir(dir_path);
	}
	if(!d) {
		return;
	}

	struct dirent *ent;
	while((ent = readdir(d)) != NULL && ibt.total_count < IB_THEME_MAX_ICONS) {
		char *fname = ent->d_name;
		uint32_t len = (uint32_t)strlen(fname);
		if(len < 5 || strcmp(fname + len - 4, ".svg") != 0) {
			continue;
		}

		char full_path[2048];
		snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, fname);

#ifndef _WIN32
		struct stat st;
		if(lstat(full_path, &st) == 0 && S_ISLNK(st.st_mode)) {
			continue;
		}
#endif

		uint32_t name_len = len - 4;
		if(name_len >= IB_ICON_NAME) {
			name_len = IB_ICON_NAME - 1;
		}
		uint32_t idx = ibt.total_count;
		memcpy(ibt.names[idx], fname, name_len);
		ibt.names[idx][name_len] = '\0';
		strncpy(ibt.paths[idx], full_path, sizeof(ibt.paths[idx]) - 1);
		ibt.paths[idx][sizeof(ibt.paths[idx]) - 1] = '\0';
		ibt.cats[idx] = cat_idx;
		++ibt.total_count;
	}
	closedir(d);
}
#pragma GCC diagnostic pop

// [=]===^=[ ibt_sort_compare ]=======================================[=]
static int ibt_sort_compare(const void *a, const void *b) {
	uint32_t ia = *(uint32_t *)a;
	uint32_t ib = *(uint32_t *)b;
	return strcmp(ibt.names[ia], ibt.names[ib]);
}

// [=]===^=[ ibt_sort ]===============================================[=]
static void ibt_sort(void) {
	uint32_t *order = (uint32_t *)malloc(ibt.total_count * sizeof(uint32_t));
	if(!order) {
		return;
	}
	for(uint32_t i = 0; i < ibt.total_count; ++i) {
		order[i] = i;
	}
	qsort(order, ibt.total_count, sizeof(uint32_t), ibt_sort_compare);

	char (*tmp_names)[IB_ICON_NAME] = (char (*)[IB_ICON_NAME])malloc(ibt.total_count * IB_ICON_NAME);
	char (*tmp_paths)[1024] = (char (*)[1024])malloc(ibt.total_count * 1024);
	uint32_t *tmp_cats = (uint32_t *)malloc(ibt.total_count * sizeof(uint32_t));
	if(!tmp_names || !tmp_paths || !tmp_cats) {
		free(order);
		free(tmp_names);
		free(tmp_paths);
		free(tmp_cats);
		return;
	}

	for(uint32_t i = 0; i < ibt.total_count; ++i) {
		memcpy(tmp_names[i], ibt.names[order[i]], IB_ICON_NAME);
		memcpy(tmp_paths[i], ibt.paths[order[i]], 1024);
		tmp_cats[i] = ibt.cats[order[i]];
	}
	memcpy(ibt.names, tmp_names, ibt.total_count * IB_ICON_NAME);
	memcpy(ibt.paths, tmp_paths, ibt.total_count * 1024);
	memcpy(ibt.cats, tmp_cats, ibt.total_count * sizeof(uint32_t));

	free(order);
	free(tmp_names);
	free(tmp_paths);
	free(tmp_cats);
}

// [=]===^=[ ibt_filter ]=============================================[=]
static void ibt_filter(char *search) {
	ibt.filtered_count = 0;
	uint32_t search_len = search ? (uint32_t)strlen(search) : 0;

	for(uint32_t i = 0; i < ibt.total_count && ibt.filtered_count < IB_THEME_MAX_ICONS; ++i) {
		if(ibt.active_cat > 0 && ibt.cats[i] != ibt.active_cat - 1) {
			continue;
		}
		if(search_len > 0) {
			char *p = ibt.names[i];
			uint32_t found = 0;
			while(*p) {
				uint32_t match = 1;
				for(uint32_t fi = 0; fi < search_len; ++fi) {
					char fc = search[fi];
					char nc = p[fi];
					if(nc == '\0') { match = 0; break; }
					if(fc >= 'A' && fc <= 'Z') { fc = (char)(fc + 32); }
					if(nc >= 'A' && nc <= 'Z') { nc = (char)(nc + 32); }
					if(fc != nc) { match = 0; break; }
				}
				if(match) { found = 1; break; }
				++p;
			}
			if(!found) {
				continue;
			}
		}
		ibt.filtered_idx[ibt.filtered_count++] = i;
	}
}

// [=]===^=[ ibt_label_cb ]===========================================[=]
static void ibt_label_cb(uint32_t item, char *buf, uint32_t buf_size, void *userdata) {
	(void)userdata;
	if(item < ibt.filtered_count) {
		strncpy(buf, ibt.names[ibt.filtered_idx[item]], buf_size - 1);
		buf[buf_size - 1] = '\0';
	} else {
		buf[0] = '\0';
	}
}

// [=]===^=[ ibt_icon_cb ]============================================[=]
static void ibt_icon_cb(uint32_t item, char *buf, uint32_t buf_size, void *userdata) {
	struct mkgui_ctx *ctx = (struct mkgui_ctx *)userdata;
	if(item < ibt.filtered_count) {
		uint32_t idx = ibt.filtered_idx[item];
		char *name = ibt.names[idx];
		strncpy(buf, name, buf_size - 1);
		buf[buf_size - 1] = '\0';

		if(icon_find_idx(name) < 0) {
			mkgui_icon_load_svg(ctx, name, ibt.paths[idx]);
		}
	} else {
		buf[0] = '\0';
	}
}

// [=]===^=[ mkgui_icon_browser_theme ]===============================[=]
MKGUI_API uint32_t mkgui_icon_browser_theme(struct mkgui_ctx *ctx, char *theme_dir, int32_t size, char *out, uint32_t out_size) {
	MKGUI_CHECK_VAL(ctx, 0);
	if(!out || out_size == 0 || !theme_dir) {
		return 0;
	}

	memset(&ibt, 0, sizeof(ibt));
	ibt.prev_selected = -1;
	ibt.saved_icon_count = icon_count;
	strncpy(ibt.theme_dir, theme_dir, sizeof(ibt.theme_dir) - 1);
	ibt.size = size > 0 ? size : 16;

	for(uint32_t ci = 0; ib_theme_cat_names[ci]; ++ci) {
		ibt_scan_category(theme_dir, ibt.size, ib_theme_cat_names[ci], ci);
	}
	ibt_sort();

	if(ibt.total_count == 0) {
		return 0;
	}

	popup_destroy_all(ctx);

	uint32_t cat_count = 0;
	for(uint32_t ci = 0; ib_theme_cat_names[ci]; ++ci) {
		++cat_count;
	}

	uint32_t iv_id = IB_TH_TAB_ALL + 100;
	uint32_t wcount = 6 + cat_count;
	struct mkgui_widget *widgets = (struct mkgui_widget *)calloc(wcount, sizeof(struct mkgui_widget));
	if(!widgets) {
		return 0;
	}

	uint32_t wi = 0;
	widgets[wi++] = (struct mkgui_widget){ MKGUI_WINDOW, IB_TH_WINDOW, "Icon Browser", "", 0, IB_WIN_W, IB_WIN_H, 0, 0, 0 };
	widgets[wi++] = (struct mkgui_widget){ MKGUI_VBOX, IB_TH_VBOX, "", "", IB_TH_WINDOW, 0, 0, 0, 0, 0 };
	widgets[wi++] = (struct mkgui_widget){ MKGUI_INPUT, IB_TH_SEARCH, "", "edit-find", IB_TH_VBOX, 0, 24, MKGUI_FIXED, 0, 0 };
	widgets[wi++] = (struct mkgui_widget){ MKGUI_TABS, IB_TH_TABS, "", "", IB_TH_VBOX, 0, 28, MKGUI_FIXED, 0, 0 };
	widgets[wi++] = (struct mkgui_widget){ MKGUI_TAB, IB_TH_TAB_ALL, "All", "", IB_TH_TABS, 0, 0, 0, 0, 0 };

	for(uint32_t ci = 0; ci < cat_count; ++ci) {
		char label[64];
		strncpy(label, ib_theme_cat_names[ci], sizeof(label) - 1);
		label[sizeof(label) - 1] = '\0';
		if(label[0] >= 'a' && label[0] <= 'z') {
			label[0] = (char)(label[0] - 32);
		}
		widgets[wi] = (struct mkgui_widget){ MKGUI_TAB, (uint32_t)(IB_TH_TAB_FIRST + ci), "", "", IB_TH_TABS, 0, 0, 0, 0, 0 };
		strncpy(widgets[wi].label, label, MKGUI_MAX_TEXT - 1);
		++wi;
	}
	widgets[wi++] = (struct mkgui_widget){ MKGUI_ITEMVIEW, iv_id, "", "", IB_TH_VBOX, 0, 0, 0, 0, 1 };

	struct mkgui_ctx *dlg = mkgui_create_child(ctx, widgets, wcount, "Icon Browser", IB_WIN_W, IB_WIN_H);
	free(widgets);
	if(!dlg) {
		return 0;
	}
	mkgui_set_window_instance(dlg, "iconbrowser");

	ibt.active_cat = 0;
	ibt_filter(NULL);
	mkgui_itemview_setup(dlg, iv_id, ibt.filtered_count, MKGUI_VIEW_COMPACT, ibt_label_cb, ibt_icon_cb, dlg);

	mkgui_set_focus(dlg, IB_TH_SEARCH);

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
						struct mkgui_itemview_data *iv = find_itemview_data(dlg, iv_id);
						if(iv && iv->selected >= 0 && iv->selected < (int32_t)ibt.filtered_count) {
							uint32_t idx = ibt.filtered_idx[iv->selected];
							strncpy(ibt.result, ibt.names[idx], IB_ICON_NAME - 1);
							ibt.result[IB_ICON_NAME - 1] = '\0';
							ibt.confirmed = 1;
							running = 0;
						}
					}
				} break;

				case MKGUI_EVENT_INPUT_CHANGED: {
					if(ev.id == IB_TH_SEARCH) {
						char *filter = mkgui_input_get(dlg, IB_TH_SEARCH);
						ibt_filter(filter);
						mkgui_itemview_set_items(dlg, iv_id, ibt.filtered_count);
						mkgui_itemview_set_selected(dlg, iv_id, -1);
					}
				} break;

				case MKGUI_EVENT_TAB_CHANGED: {
					if(ev.id == IB_TH_TABS) {
						uint32_t tab_id = (uint32_t)ev.value;
						if(tab_id == IB_TH_TAB_ALL) {
							ibt.active_cat = 0;
						} else {
							ibt.active_cat = tab_id - IB_TH_TAB_FIRST + 1;
						}
						char *filter = mkgui_input_get(dlg, IB_TH_SEARCH);
						ibt_filter(filter);
						mkgui_itemview_set_items(dlg, iv_id, ibt.filtered_count);
						mkgui_itemview_set_selected(dlg, iv_id, -1);
					}
				} break;

				case MKGUI_EVENT_ITEMVIEW_DBLCLICK: {
					if(ev.id == iv_id && ev.value >= 0 && ev.value < (int32_t)ibt.filtered_count) {
						uint32_t idx = ibt.filtered_idx[ev.value];
						strncpy(ibt.result, ibt.names[idx], IB_ICON_NAME - 1);
						ibt.result[IB_ICON_NAME - 1] = '\0';
						ibt.confirmed = 1;
						running = 0;
					}
				} break;

				case MKGUI_EVENT_ITEMVIEW_SELECT: {
					ibt.prev_selected = ev.value;
				} break;

				default: break;
			}
		}
		mkgui_wait(dlg);
	}

	mkgui_destroy_child(dlg);

	icon_count = ibt.saved_icon_count;
	dirty_all(ctx);

	if(ibt.confirmed && ibt.result[0] != '\0') {
		strncpy(out, ibt.result, out_size - 1);
		out[out_size - 1] = '\0';
		return 1;
	}
	return 0;
}
