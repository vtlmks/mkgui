// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define IB_ICON_NAME   64
#define IB_WIN_W       600
#define IB_WIN_H       500
#define IB_MAX_THEMES  32
#define IB_MAX_CATS    32

// ---------------------------------------------------------------------------
// SVG theme icon browser
// ---------------------------------------------------------------------------

#define IB_THEME_MAX_ICONS   32768
#define IBT_HASH_SIZE        65536
#define IBT_HASH_MASK        (IBT_HASH_SIZE - 1)

enum {
	IB_TH_WINDOW = 100,
	IB_TH_VBOX,
	IB_TH_HBOX_TOP,
	IB_TH_THEME_DROP,
	IB_TH_SEARCH,
	IB_TH_TABS,
	IB_TH_TAB_ALL,
	IB_TH_TAB_FIRST,
};

struct ib_theme_state {
	char names[IB_THEME_MAX_ICONS][IB_ICON_NAME];
	char paths[IB_THEME_MAX_ICONS][1024];
	uint32_t cat_mask[IB_THEME_MAX_ICONS];
	uint32_t total_count;

	uint32_t filtered_idx[IB_THEME_MAX_ICONS];
	uint32_t filtered_count;

	char cat_names[IB_MAX_CATS][64];
	uint32_t cat_count;

	uint32_t active_cat;
	uint32_t confirmed;
	char result_name[IB_ICON_NAME];
	char result_path[1024];
	int32_t prev_selected;
	uint32_t saved_icon_count;
	uint32_t saved_svg_source_count;

	char theme_dirs[IB_MAX_THEMES][1024];
	char theme_names[IB_MAX_THEMES][260];
	uint32_t theme_count;
	uint32_t active_theme;

	int32_t size;

	uint32_t hash_table[IBT_HASH_SIZE];
};

static struct ib_theme_state *ibt;

// [=]===^=[ ibt_try_add_theme ]=====================================[=]
static void ibt_try_add_theme(const char *dir_path, const char *name) {
	if(ibt->theme_count >= IB_MAX_THEMES) {
		return;
	}
	char idx_path[2048];
	snprintf(idx_path, sizeof(idx_path), "%s/index.theme", dir_path);
	if(access(idx_path, R_OK) == 0) {
		uint32_t ti = ibt->theme_count;
		snprintf(ibt->theme_dirs[ti], sizeof(ibt->theme_dirs[ti]), "%s", dir_path);
		snprintf(ibt->theme_names[ti], sizeof(ibt->theme_names[ti]), "%s", name);
		++ibt->theme_count;
		return;
	}
	// source repos (e.g. breeze) may have icons/ subdir with .theme.in files
	char icons_sub[2048];
	snprintf(icons_sub, sizeof(icons_sub), "%s/icons", dir_path);
	DIR *probe = opendir(icons_sub);
	if(!probe) {
		return;
	}
	uint32_t has_theme_in = 0;
	struct dirent *pe;
	while((pe = readdir(probe)) != NULL) {
		uint32_t plen = (uint32_t)strlen(pe->d_name);
		if(plen > 9 && strcmp(pe->d_name + plen - 9, ".theme.in") == 0) {
			has_theme_in = 1;
			break;
		}
	}
	closedir(probe);
	if(!has_theme_in) {
		return;
	}
	uint32_t ti = ibt->theme_count;
	snprintf(ibt->theme_dirs[ti], sizeof(ibt->theme_dirs[ti]), "%s/icons", dir_path);
	snprintf(ibt->theme_names[ti], sizeof(ibt->theme_names[ti]), "%s", name);
	++ibt->theme_count;
}

// [=]===^=[ ibt_scan_icon_dir ]=====================================[=]
// Scan a directory containing icon themes (e.g. /usr/share/icons/).
// Each subdirectory with an index.theme is added as a browseable theme.
static void ibt_scan_icon_dir(const char *base_dir) {
	DIR *d = opendir(base_dir);
	if(!d) {
		return;
	}
	struct dirent *ent;
	while((ent = readdir(d)) != NULL && ibt->theme_count < IB_MAX_THEMES) {
		if(ent->d_name[0] == '.') {
			continue;
		}
		char sub_path[2048];
		snprintf(sub_path, sizeof(sub_path), "%s/%s", base_dir, ent->d_name);
		struct stat st;
		if(stat(sub_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
			continue;
		}
		ibt_try_add_theme(sub_path, ent->d_name);
	}
	closedir(d);
}

// [=]===^=[ ibt_scan_local_themes ]=================================[=]
static void ibt_scan_local_themes(void) {
	ibt->theme_count = 0;

#ifndef _WIN32
	// scan system icon theme directories on Linux
	{
		const char *data_home = getenv("XDG_DATA_HOME");
		char path[2048];
		if(data_home && data_home[0]) {
			snprintf(path, sizeof(path), "%s/icons", data_home);
			ibt_scan_icon_dir(path);
		} else {
			const char *home = getenv("HOME");
			if(home && home[0]) {
				snprintf(path, sizeof(path), "%s/.local/share/icons", home);
				ibt_scan_icon_dir(path);
			}
		}
	}
	{
		const char *xdg_dirs = getenv("XDG_DATA_DIRS");
		const char *defaults = "/usr/share:/usr/local/share";
		const char *dirs = (xdg_dirs && xdg_dirs[0]) ? xdg_dirs : defaults;
		const char *p = dirs;
		while(*p && ibt->theme_count < IB_MAX_THEMES) {
			const char *colon = p;
			while(*colon && *colon != ':') {
				++colon;
			}
			uint32_t seg_len = (uint32_t)(colon - p);
			if(seg_len > 0) {
				char path[2048];
				snprintf(path, sizeof(path), "%.*s/icons", (int)seg_len, p);
				ibt_scan_icon_dir(path);
			}
			p = *colon ? colon + 1 : colon;
		}
	}
#endif

	// also scan cwd and one level deeper (local/unpacked themes, Windows)
	DIR *d = opendir(".");
	if(!d) {
		return;
	}
	struct dirent *ent;
	while((ent = readdir(d)) != NULL && ibt->theme_count < IB_MAX_THEMES) {
		if(ent->d_name[0] == '.') {
			continue;
		}
		struct stat st;
		if(stat(ent->d_name, &st) != 0 || !S_ISDIR(st.st_mode)) {
			continue;
		}
		ibt_try_add_theme(ent->d_name, ent->d_name);
		// scan one level deeper for extracted archives
		DIR *sub = opendir(ent->d_name);
		if(sub) {
			struct dirent *sent;
			while((sent = readdir(sub)) != NULL && ibt->theme_count < IB_MAX_THEMES) {
				if(sent->d_name[0] == '.') {
					continue;
				}
				char sub_path[2048];
				snprintf(sub_path, sizeof(sub_path), "%s/%s", ent->d_name, sent->d_name);
				struct stat sst;
				if(stat(sub_path, &sst) == 0 && S_ISDIR(sst.st_mode)) {
					ibt_try_add_theme(sub_path, sent->d_name);
				}
			}
			closedir(sub);
		}
	}
	closedir(d);
}

// ---------------------------------------------------------------------------
// Hash table for fast icon name dedup during scanning
// ---------------------------------------------------------------------------

// [=]===^=[ ibt_hash_fn ]===========================================[=]
static uint32_t ibt_hash_fn(const char *name) {
	uint32_t h = 2166136261u;
	for(const char *p = name; *p; ++p) {
		h ^= (uint32_t)(uint8_t)*p;
		h *= 16777619u;
	}
	return h & IBT_HASH_MASK;
}

// [=]===^=[ ibt_hash_clear ]========================================[=]
static void ibt_hash_clear(void) {
	for(uint32_t i = 0; i < IBT_HASH_SIZE; ++i) {
		ibt->hash_table[i] = UINT32_MAX;
	}
}

// [=]===^=[ ibt_hash_insert ]=======================================[=]
static void ibt_hash_insert(uint32_t idx) {
	uint32_t h = ibt_hash_fn(ibt->names[idx]);
	while(ibt->hash_table[h] != UINT32_MAX) {
		h = (h + 1) & IBT_HASH_MASK;
	}
	ibt->hash_table[h] = idx;
}

// [=]===^=[ ibt_find_name ]==========================================[=]
static int32_t ibt_find_name(const char *name) {
	uint32_t h = ibt_hash_fn(name);
	for(;;) {
		uint32_t idx = ibt->hash_table[h];
		if(idx == UINT32_MAX) {
			return -1;
		}

		if(strcmp(ibt->names[idx], name) == 0) {
			return (int32_t)idx;
		}
		h = (h + 1) & IBT_HASH_MASK;
	}
}

// [=]===^=[ ibt_scan_one_dir ]=======================================[=]
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
static void ibt_scan_one_dir(const char *dir_path, uint32_t cat_idx) {
	DIR *d = opendir(dir_path);
	if(!d) {
		return;
	}
	struct dirent *ent;
	while((ent = readdir(d)) != NULL) {
		char *fname = ent->d_name;
		uint32_t len = (uint32_t)strlen(fname);
		if(len < 5 || strcmp(fname + len - 4, ".svg") != 0) {
			continue;
		}
		char name[IB_ICON_NAME];
		uint32_t name_len = len - 4;
		if(name_len >= IB_ICON_NAME) {
			name_len = IB_ICON_NAME - 1;
		}
		memcpy(name, fname, name_len);
		name[name_len] = '\0';
		int32_t existing = ibt_find_name(name);
		if(existing >= 0) {
			ibt->cat_mask[existing] |= (1u << cat_idx);
			continue;
		}

		if(ibt->total_count >= IB_THEME_MAX_ICONS) {
			continue;
		}
		char full_path[2048];
		snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, fname);
		uint32_t idx = ibt->total_count;
		memcpy(ibt->names[idx], name, name_len + 1);
		strncpy(ibt->paths[idx], full_path, sizeof(ibt->paths[idx]) - 1);
		ibt->paths[idx][sizeof(ibt->paths[idx]) - 1] = '\0';
		ibt->cat_mask[idx] = (1u << cat_idx);
		ibt_hash_insert(idx);
		++ibt->total_count;
	}
	closedir(d);
}

// [=]===^=[ ibt_scan_category ]======================================[=]
static void ibt_scan_category(const char *theme_dir, int32_t size, const char *category, uint32_t cat_idx) {
	static const int sizes[] = { 16, 22, 24, 32, 48, 64 };
	char dir_path[2048];

	// scalable first (preferred for SVG)
	snprintf(dir_path, sizeof(dir_path), "%s/scalable/%s", theme_dir, category);
	ibt_scan_one_dir(dir_path, cat_idx);

	// preferred size
	snprintf(dir_path, sizeof(dir_path), "%s/%dx%d/%s", theme_dir, size, size, category);
	ibt_scan_one_dir(dir_path, cat_idx);

	// all other sizes (picks up icons that only exist at certain sizes)
	for(uint32_t si = 0; si < 6; ++si) {
		if(sizes[si] == size) {
			continue;
		}
		snprintf(dir_path, sizeof(dir_path), "%s/%dx%d/%s", theme_dir, sizes[si], sizes[si], category);
		ibt_scan_one_dir(dir_path, cat_idx);
	}

	// category-first layout (breeze, elementary)
	snprintf(dir_path, sizeof(dir_path), "%s/%s/%d", theme_dir, category, size);
	ibt_scan_one_dir(dir_path, cat_idx);
	for(uint32_t si = 0; si < 6; ++si) {
		if(sizes[si] == size) {
			continue;
		}
		snprintf(dir_path, sizeof(dir_path), "%s/%s/%d", theme_dir, category, sizes[si]);
		ibt_scan_one_dir(dir_path, cat_idx);
	}
	snprintf(dir_path, sizeof(dir_path), "%s/%s/scalable", theme_dir, category);
	ibt_scan_one_dir(dir_path, cat_idx);
}
#pragma GCC diagnostic pop

// [=]===^=[ ibt_discover_categories ]================================[=]
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
static void ibt_discover_categories(const char *theme_dir) {
	ibt->cat_count = 0;
	static const char *size_dirs[] = { "scalable", "16x16", "22x22", "24x24", "32x32", "48x48", "64x64", NULL };
	static const int cat_first_sizes[] = { 16, 22, 24, 32, 48, 64 };

	// scan {size}/{category} layout
	for(const char **sd = size_dirs; *sd; ++sd) {
		char size_path[2048];
		snprintf(size_path, sizeof(size_path), "%s/%s", theme_dir, *sd);
		DIR *d = opendir(size_path);
		if(!d) {
			continue;
		}
		struct dirent *ent;
		while((ent = readdir(d)) != NULL && ibt->cat_count < IB_MAX_CATS) {
			if(ent->d_name[0] == '.') {
				continue;
			}
			char sub[2048];
			snprintf(sub, sizeof(sub), "%s/%s", size_path, ent->d_name);
			struct stat st;
			if(stat(sub, &st) != 0 || !S_ISDIR(st.st_mode)) {
				continue;
			}
			uint32_t exists = 0;
			for(uint32_t i = 0; i < ibt->cat_count; ++i) {
				if(strcmp(ibt->cat_names[i], ent->d_name) == 0) {
					exists = 1;
					break;
				}
			}

			if(!exists) {
				strncpy(ibt->cat_names[ibt->cat_count], ent->d_name, sizeof(ibt->cat_names[0]) - 1);
				ibt->cat_names[ibt->cat_count][sizeof(ibt->cat_names[0]) - 1] = '\0';
				++ibt->cat_count;
			}
		}
		closedir(d);
	}

	// scan {category}/{size} layout (breeze)
	DIR *root = opendir(theme_dir);
	if(root) {
		struct dirent *ent;
		while((ent = readdir(root)) != NULL && ibt->cat_count < IB_MAX_CATS) {
			if(ent->d_name[0] == '.') {
				continue;
			}
			char sub[2048];
			snprintf(sub, sizeof(sub), "%s/%s", theme_dir, ent->d_name);
			struct stat st;
			if(stat(sub, &st) != 0 || !S_ISDIR(st.st_mode)) {
				continue;
			}
			// check if this dir contains numbered subdirs (size dirs)
			for(uint32_t si = 0; si < 6; ++si) {
				char probe[2048];
				snprintf(probe, sizeof(probe), "%s/%d", sub, cat_first_sizes[si]);
				struct stat pst;
				if(stat(probe, &pst) == 0 && S_ISDIR(pst.st_mode)) {
					uint32_t exists = 0;
					for(uint32_t i = 0; i < ibt->cat_count; ++i) {
						if(strcmp(ibt->cat_names[i], ent->d_name) == 0) {
							exists = 1;
							break;
						}
					}

					if(!exists && ibt->cat_count < IB_MAX_CATS) {
						strncpy(ibt->cat_names[ibt->cat_count], ent->d_name, sizeof(ibt->cat_names[0]) - 1);
						ibt->cat_names[ibt->cat_count][sizeof(ibt->cat_names[0]) - 1] = '\0';
						++ibt->cat_count;
					}
					break;
				}
			}
		}
		closedir(root);
	}

	// sort categories alphabetically
	for(uint32_t i = 1; i < ibt->cat_count; ++i) {
		for(uint32_t j = i; j > 0 && strcmp(ibt->cat_names[j - 1], ibt->cat_names[j]) > 0; --j) {
			char tmp[64];
			memcpy(tmp, ibt->cat_names[j], 64);
			memcpy(ibt->cat_names[j], ibt->cat_names[j - 1], 64);
			memcpy(ibt->cat_names[j - 1], tmp, 64);
		}
	}
}

#pragma GCC diagnostic pop

// [=]===^=[ ibt_sort_compare ]=======================================[=]
static int ibt_sort_compare(const void *a, const void *b) {
	uint32_t ia = *(uint32_t *)a;
	uint32_t ib = *(uint32_t *)b;
	return strcmp(ibt->names[ia], ibt->names[ib]);
}

// [=]===^=[ ibt_sort ]===============================================[=]
static void ibt_sort(void) {
	if(ibt->total_count == 0) {
		return;
	}
	uint32_t *order = (uint32_t *)malloc(ibt->total_count * sizeof(uint32_t));
	if(!order) {
		return;
	}
	for(uint32_t i = 0; i < ibt->total_count; ++i) {
		order[i] = i;
	}
	qsort(order, ibt->total_count, sizeof(uint32_t), ibt_sort_compare);

	char (*tmp_names)[IB_ICON_NAME] = (char (*)[IB_ICON_NAME])malloc(ibt->total_count * IB_ICON_NAME);
	char (*tmp_paths)[1024] = (char (*)[1024])malloc(ibt->total_count * 1024);
	uint32_t *tmp_masks = (uint32_t *)malloc(ibt->total_count * sizeof(uint32_t));
	if(!tmp_names || !tmp_paths || !tmp_masks) {
		free(order);
		free(tmp_names);
		free(tmp_paths);
		free(tmp_masks);
		return;
	}

	for(uint32_t i = 0; i < ibt->total_count; ++i) {
		memcpy(tmp_names[i], ibt->names[order[i]], IB_ICON_NAME);
		memcpy(tmp_paths[i], ibt->paths[order[i]], 1024);
		tmp_masks[i] = ibt->cat_mask[order[i]];
	}
	memcpy(ibt->names, tmp_names, ibt->total_count * IB_ICON_NAME);
	memcpy(ibt->paths, tmp_paths, ibt->total_count * 1024);
	memcpy(ibt->cat_mask, tmp_masks, ibt->total_count * sizeof(uint32_t));

	free(order);
	free(tmp_names);
	free(tmp_paths);
	free(tmp_masks);
}

// [=]===^=[ ibt_load_theme ]=========================================[=]
static void ibt_load_theme(uint32_t theme_idx) {
	ibt->total_count = 0;
	ibt->filtered_count = 0;
	ibt_hash_clear();
	if(theme_idx >= ibt->theme_count) {
		return;
	}
	ibt->active_theme = theme_idx;
	ibt_discover_categories(ibt->theme_dirs[theme_idx]);
	for(uint32_t ci = 0; ci < ibt->cat_count; ++ci) {
		ibt_scan_category(ibt->theme_dirs[theme_idx], ibt->size, ibt->cat_names[ci], ci);
	}
	ibt_sort();
	// rebuild hash after sort (indices changed)
	ibt_hash_clear();
	for(uint32_t i = 0; i < ibt->total_count; ++i) {
		ibt_hash_insert(i);
	}
}

// [=]===^=[ ibt_rebuild_tabs ]=======================================[=]
static void ibt_rebuild_tabs(struct mkgui_ctx *dlg) {
	// remove old category tabs (not the "All" tab)
	for(uint32_t i = 0; i < IB_MAX_CATS; ++i) {
		mkgui_remove_widget(dlg, IB_TH_TAB_FIRST + i);
	}
	// add new tabs for discovered categories
	for(uint32_t ci = 0; ci < ibt->cat_count; ++ci) {
		char label[64];
		strncpy(label, ibt->cat_names[ci], sizeof(label) - 1);
		label[sizeof(label) - 1] = '\0';
		if(label[0] >= 'a' && label[0] <= 'z') {
			label[0] = (char)(label[0] - 32);
		}
		struct mkgui_widget tw = MKGUI_W(MKGUI_TAB, IB_TH_TAB_FIRST + ci, "", "", IB_TH_TABS, 0, 0, 0, 0, 0);
		strncpy(tw.label, label, MKGUI_MAX_TEXT - 1);
		mkgui_add_widget(dlg, tw, 0);
	}
}

// [=]===^=[ ibt_filter ]=============================================[=]
static void ibt_filter(const char *search) {
	ibt->filtered_count = 0;
	uint32_t search_len = search ? (uint32_t)strlen(search) : 0;

	for(uint32_t i = 0; i < ibt->total_count && ibt->filtered_count < IB_THEME_MAX_ICONS; ++i) {
		if(ibt->active_cat > 0 && !(ibt->cat_mask[i] & (1u << (ibt->active_cat - 1)))) {
			continue;
		}

		if(search_len > 0) {
			char *p = ibt->names[i];
			uint32_t found = 0;
			while(*p) {
				uint32_t match = 1;
				for(uint32_t fi = 0; fi < search_len; ++fi) {
					char fc = search[fi];
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
		ibt->filtered_idx[ibt->filtered_count++] = i;
	}
}

// [=]===^=[ ibt_label_cb ]===========================================[=]
static void ibt_label_cb(uint32_t item, char *buf, uint32_t buf_size, void *userdata) {
	(void)userdata;
	if(item < ibt->filtered_count) {
		strncpy(buf, ibt->names[ibt->filtered_idx[item]], buf_size - 1);
		buf[buf_size - 1] = '\0';
	} else {
		buf[0] = '\0';
	}
}

// [=]===^=[ ibt_icon_cb ]============================================[=]
static void ibt_icon_cb(uint32_t item, char *buf, uint32_t buf_size, void *userdata) {
	struct mkgui_ctx *ctx = (struct mkgui_ctx *)userdata;
	if(item < ibt->filtered_count) {
		uint32_t idx = ibt->filtered_idx[item];
		char *name = ibt->names[idx];
		strncpy(buf, name, buf_size - 1);
		buf[buf_size - 1] = '\0';

		if(icon_find_idx(name) < 0) {
			mkgui_icon_load_svg(ctx, name, ibt->paths[idx]);
		}
	} else {
		buf[0] = '\0';
	}
}

// [=]===^=[ ibt_confirm_selection ]==================================[=]
static void ibt_confirm_selection(uint32_t filtered_item) {
	if(filtered_item >= ibt->filtered_count) {
		return;
	}
	uint32_t idx = ibt->filtered_idx[filtered_item];
	strncpy(ibt->result_name, ibt->names[idx], IB_ICON_NAME - 1);
	ibt->result_name[IB_ICON_NAME - 1] = '\0';
	strncpy(ibt->result_path, ibt->paths[idx], sizeof(ibt->result_path) - 1);
	ibt->result_path[sizeof(ibt->result_path) - 1] = '\0';
	ibt->confirmed = 1;
}

// [=]===^=[ ibt_reset_icons ]========================================[=]
// Reset icon array back to pre-browser state, freeing any icons loaded
// during browsing. Called on theme switch and browser close.
static void ibt_reset_icons(void) {
	for(uint32_t i = ibt->saved_svg_source_count; i < svg_source_count; ++i) {
		free(svg_sources[i].svg_data);
		svg_sources[i].svg_data = NULL;
	}
	icon_count = ibt->saved_icon_count;
	svg_source_count = ibt->saved_svg_source_count;
	icon_hash_clear();
	for(uint32_t i = 0; i < icon_count; ++i) {
		icon_hash_insert(i);
	}
	icon_atlas_rebuild();
}

// [=]===^=[ ibt_cleanup ]============================================[=]
static void ibt_cleanup(struct mkgui_ctx *ctx) {
	ibt_reset_icons();
	free(ibt);
	ibt = NULL;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_icon_browser_theme ]===============================[=]
MKGUI_API uint32_t mkgui_icon_browser_theme(struct mkgui_ctx *ctx, const char *theme_dir, int32_t size, char *out, uint32_t out_size) {
	MKGUI_CHECK_VAL(ctx, 0);
	if(!out || out_size == 0 || !theme_dir) {
		return 0;
	}
	char path_buf[1024];
	uint32_t result = mkgui_icon_browser(ctx, size, out, out_size, path_buf, sizeof(path_buf));
	return result;
}

// [=]===^=[ mkgui_icon_browser ]=====================================[=]
MKGUI_API uint32_t mkgui_icon_browser(struct mkgui_ctx *ctx, int32_t size, char *out_name, uint32_t name_size, char *out_path, uint32_t path_size) {
	MKGUI_CHECK_VAL(ctx, 0);
	if(!out_name || name_size == 0) {
		return 0;
	}

	ibt = (struct ib_theme_state *)calloc(1, sizeof(struct ib_theme_state));
	if(!ibt) {
		return 0;
	}
	ibt->prev_selected = -1;
	ibt->saved_icon_count = icon_count;
	ibt->saved_svg_source_count = svg_source_count;
	ibt->size = size > 0 ? size : 16;

	ibt_scan_local_themes();
	if(ibt->theme_count == 0) {
		fprintf(stderr, "mkgui: no icon themes found (place a Freedesktop icon theme directory next to the editor)\n");
		free(ibt);
		ibt = NULL;
		return 0;
	}

	ibt_load_theme(0);

	popup_destroy_all(ctx);

	uint32_t iv_id = IB_TH_TAB_ALL + 100;
	uint32_t wcount = 7;
	struct mkgui_widget widgets[7] = {
		MKGUI_W(MKGUI_WINDOW, IB_TH_WINDOW, "Icon Browser", "", 0, IB_WIN_W, IB_WIN_H, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX, IB_TH_VBOX, "", "", IB_TH_WINDOW, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_HBOX, IB_TH_HBOX_TOP, "", "", IB_TH_VBOX, 0, 24, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_DROPDOWN, IB_TH_THEME_DROP, "", "", IB_TH_HBOX_TOP, 180, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_INPUT, IB_TH_SEARCH, "", "edit-find", IB_TH_HBOX_TOP, 0, 0, 0, 0, 1),
		MKGUI_W(MKGUI_TABS, IB_TH_TABS, "", "", IB_TH_VBOX, 0, 28, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_TAB, IB_TH_TAB_ALL, "All", "", IB_TH_TABS, 0, 0, 0, 0, 0),
	};

	struct mkgui_ctx *dlg = mkgui_create_child(ctx, widgets, wcount, "Icon Browser", IB_WIN_W, IB_WIN_H);
	if(!dlg) {
		return 0;
	}
	mkgui_set_window_instance(dlg, "iconbrowser");

	// add itemview after tabs so it appears below
	struct mkgui_widget ivw = MKGUI_W(MKGUI_ITEMVIEW, iv_id, "", "", IB_TH_VBOX, 0, 0, 0, 0, 1);
	mkgui_add_widget(dlg, ivw, 0);

	for(uint32_t i = 0; i < ibt->theme_count; ++i) {
		mkgui_dropdown_add(dlg, IB_TH_THEME_DROP, ibt->theme_names[i]);
	}
	mkgui_dropdown_set(dlg, IB_TH_THEME_DROP, 0);

	ibt_rebuild_tabs(dlg);

	ibt->active_cat = 0;
	ibt_filter(NULL);
	mkgui_itemview_setup(dlg, iv_id, ibt->filtered_count, MKGUI_VIEW_COMPACT, ibt_label_cb, ibt_icon_cb, dlg);

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
						if(iv && iv->selected >= 0 && iv->selected < (int32_t)ibt->filtered_count) {
							ibt_confirm_selection((uint32_t)iv->selected);
							running = 0;
						}
					}
				} break;

				case MKGUI_EVENT_DROPDOWN_CHANGED: {
					if(ev.id == IB_TH_THEME_DROP && ev.value >= 0 && ev.value < (int32_t)ibt->theme_count) {
						ibt_reset_icons();
						ibt_load_theme((uint32_t)ev.value);
						ibt_rebuild_tabs(dlg);
						ibt->active_cat = 0;
						mkgui_tabs_set_current(dlg, IB_TH_TABS, IB_TH_TAB_ALL);
						const char *filter = mkgui_input_get(dlg, IB_TH_SEARCH);
						ibt_filter(filter);
						mkgui_itemview_set_items(dlg, iv_id, ibt->filtered_count);
						mkgui_itemview_set_selected(dlg, iv_id, -1);
					}
				} break;

				case MKGUI_EVENT_INPUT_CHANGED: {
					if(ev.id == IB_TH_SEARCH) {
						const char *filter = mkgui_input_get(dlg, IB_TH_SEARCH);
						ibt_filter(filter);
						mkgui_itemview_set_items(dlg, iv_id, ibt->filtered_count);
						mkgui_itemview_set_selected(dlg, iv_id, -1);
					}
				} break;

				case MKGUI_EVENT_TAB_CHANGED: {
					if(ev.id == IB_TH_TABS) {
						uint32_t tab_id = (uint32_t)ev.value;
						if(tab_id == IB_TH_TAB_ALL) {
							ibt->active_cat = 0;
						} else if(tab_id >= IB_TH_TAB_FIRST && tab_id < IB_TH_TAB_FIRST + ibt->cat_count) {
							ibt->active_cat = tab_id - IB_TH_TAB_FIRST + 1;
						}
						const char *filter = mkgui_input_get(dlg, IB_TH_SEARCH);
						ibt_filter(filter);
						mkgui_itemview_set_items(dlg, iv_id, ibt->filtered_count);
						mkgui_itemview_set_selected(dlg, iv_id, -1);
					}
				} break;

				case MKGUI_EVENT_ITEMVIEW_DBLCLICK: {
					if(ev.id == iv_id && ev.value >= 0 && ev.value < (int32_t)ibt->filtered_count) {
						ibt_confirm_selection((uint32_t)ev.value);
						running = 0;
					}
				} break;

				case MKGUI_EVENT_ITEMVIEW_SELECT: {
					ibt->prev_selected = ev.value;
				} break;

				default: break;
			}
		}
		mkgui_wait(dlg);
	}

	mkgui_destroy_child(dlg);

	char confirmed_name[IB_ICON_NAME] = {0};
	char confirmed_path[1024] = {0};
	if(ibt->confirmed && ibt->result_name[0] != '\0') {
		strncpy(confirmed_name, ibt->result_name, IB_ICON_NAME - 1);
		strncpy(confirmed_path, ibt->result_path, sizeof(confirmed_path) - 1);
	}

	ibt_cleanup(ctx);

	if(confirmed_name[0] != '\0') {
		mkgui_icon_load_svg(ctx, confirmed_name, confirmed_path);
		strncpy(out_name, confirmed_name, name_size - 1);
		out_name[name_size - 1] = '\0';
		if(out_path && path_size > 0) {
			strncpy(out_path, confirmed_path, path_size - 1);
			out_path[path_size - 1] = '\0';
		}
		return 1;
	}
	return 0;
}
