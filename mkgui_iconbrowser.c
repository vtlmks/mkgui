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
	IB_TH_GRID,
	IB_TH_HSCROLL,
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

	// Browser's own thumbnail storage: one ARGB pixel buffer per theme icon,
	// sized size*size. NULL until rasterized. Indexed by theme icon index
	// (the index into names[]/paths[]), not by filtered index, so narrowing
	// the filter and widening it again doesn't force re-rasterization.
	uint32_t *thumb[IB_THEME_MAX_ICONS];

	// Grid state
	int32_t scroll_x;
	int32_t selected;
	int32_t last_click_item;
	uint32_t last_click_time;

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
	if(mkgui_path_readable(idx_path)) {
		uint32_t ti = ibt->theme_count;
		snprintf(ibt->theme_dirs[ti], sizeof(ibt->theme_dirs[ti]), "%s", dir_path);
		snprintf(ibt->theme_names[ti], sizeof(ibt->theme_names[ti]), "%s", name);
		++ibt->theme_count;
		return;
	}
	// source repos (e.g. breeze) may have icons/ subdir with .theme.in files
	char icons_sub[2048];
	snprintf(icons_sub, sizeof(icons_sub), "%s/icons", dir_path);
	struct mkgui_dir probe;
	if(!mkgui_dir_open(&probe, icons_sub)) {
		return;
	}
	uint32_t has_theme_in = 0;
	struct mkgui_dir_entry *pe;
	while((pe = mkgui_dir_next(&probe)) != NULL) {
		uint32_t plen = (uint32_t)strlen(pe->name);
		if(plen > 9 && strcmp(pe->name + plen - 9, ".theme.in") == 0) {
			has_theme_in = 1;
			break;
		}
	}
	mkgui_dir_close(&probe);
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
	struct mkgui_dir d;
	if(!mkgui_dir_open(&d, base_dir)) {
		return;
	}
	struct mkgui_dir_entry *ent;
	while((ent = mkgui_dir_next(&d)) != NULL && ibt->theme_count < IB_MAX_THEMES) {
		if(ent->name[0] == '.') {
			continue;
		}
		char sub_path[2048];
		snprintf(sub_path, sizeof(sub_path), "%s/%s", base_dir, ent->name);
		if(!mkgui_path_is_dir(sub_path)) {
			continue;
		}
		ibt_try_add_theme(sub_path, ent->name);
	}
	mkgui_dir_close(&d);
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
	struct mkgui_dir d;
	if(!mkgui_dir_open(&d, ".")) {
		return;
	}
	struct mkgui_dir_entry *ent;
	while((ent = mkgui_dir_next(&d)) != NULL && ibt->theme_count < IB_MAX_THEMES) {
		if(ent->name[0] == '.') {
			continue;
		}
		if(!mkgui_path_is_dir(ent->name)) {
			continue;
		}
		ibt_try_add_theme(ent->name, ent->name);
		// scan one level deeper for extracted archives
		struct mkgui_dir sub;
		if(mkgui_dir_open(&sub, ent->name)) {
			struct mkgui_dir_entry *sent;
			while((sent = mkgui_dir_next(&sub)) != NULL && ibt->theme_count < IB_MAX_THEMES) {
				if(sent->name[0] == '.') {
					continue;
				}
				char sub_path[2048];
				snprintf(sub_path, sizeof(sub_path), "%s/%s", ent->name, sent->name);
				if(mkgui_path_is_dir(sub_path)) {
					ibt_try_add_theme(sub_path, sent->name);
				}
			}
			mkgui_dir_close(&sub);
		}
	}
	mkgui_dir_close(&d);
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
	struct mkgui_dir d;
	if(!mkgui_dir_open(&d, dir_path)) {
		return;
	}
	struct mkgui_dir_entry *ent;
	while((ent = mkgui_dir_next(&d)) != NULL) {
		char *fname = ent->name;
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
	mkgui_dir_close(&d);
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
		struct mkgui_dir d;
		if(!mkgui_dir_open(&d, size_path)) {
			continue;
		}
		struct mkgui_dir_entry *ent;
		while((ent = mkgui_dir_next(&d)) != NULL && ibt->cat_count < IB_MAX_CATS) {
			if(ent->name[0] == '.') {
				continue;
			}
			char sub[2048];
			snprintf(sub, sizeof(sub), "%s/%s", size_path, ent->name);
			if(!mkgui_path_is_dir(sub)) {
				continue;
			}
			uint32_t exists = 0;
			for(uint32_t i = 0; i < ibt->cat_count; ++i) {
				if(strcmp(ibt->cat_names[i], ent->name) == 0) {
					exists = 1;
					break;
				}
			}

			if(!exists) {
				strncpy(ibt->cat_names[ibt->cat_count], ent->name, sizeof(ibt->cat_names[0]) - 1);
				ibt->cat_names[ibt->cat_count][sizeof(ibt->cat_names[0]) - 1] = '\0';
				++ibt->cat_count;
			}
		}
		mkgui_dir_close(&d);
	}

	// scan {category}/{size} layout (breeze)
	struct mkgui_dir root;
	if(mkgui_dir_open(&root, theme_dir)) {
		struct mkgui_dir_entry *ent;
		while((ent = mkgui_dir_next(&root)) != NULL && ibt->cat_count < IB_MAX_CATS) {
			if(ent->name[0] == '.') {
				continue;
			}
			char sub[2048];
			snprintf(sub, sizeof(sub), "%s/%s", theme_dir, ent->name);
			if(!mkgui_path_is_dir(sub)) {
				continue;
			}
			// check if this dir contains numbered subdirs (size dirs)
			for(uint32_t si = 0; si < 6; ++si) {
				char probe[2048];
				snprintf(probe, sizeof(probe), "%s/%d", sub, cat_first_sizes[si]);
				if(mkgui_path_is_dir(probe)) {
					uint32_t exists = 0;
					for(uint32_t i = 0; i < ibt->cat_count; ++i) {
						if(strcmp(ibt->cat_names[i], ent->name) == 0) {
							exists = 1;
							break;
						}
					}

					if(!exists && ibt->cat_count < IB_MAX_CATS) {
						strncpy(ibt->cat_names[ibt->cat_count], ent->name, sizeof(ibt->cat_names[0]) - 1);
						ibt->cat_names[ibt->cat_count][sizeof(ibt->cat_names[0]) - 1] = '\0';
						++ibt->cat_count;
					}
					break;
				}
			}
		}
		mkgui_dir_close(&root);
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

// Rasterize one SVG into a freshly-malloc'd ARGB buffer of size*size pixels.
// Caller owns the returned buffer. Never touches the main icon system.

// [=]===^=[ ibt_rasterize ]==========================================[=]
static uint32_t *ibt_rasterize(struct mkgui_ctx *ctx, const char *path, int32_t size) {
	uint32_t svg_len = 0;
	char *svg_data = svg_read_file(path, &svg_len);
	if(!svg_data) {
		return NULL;
	}
	svg_strip_style_blocks(svg_data, svg_len);

	plutosvg_document_t *doc = plutosvg_document_load_from_data(svg_data, (int)svg_len, -1, -1, NULL, NULL);
	if(!doc) {
		free(svg_data);
		return NULL;
	}

	plutovg_color_t color;
	uint32_t text = ctx->theme.text & 0x00ffffff;
	color.r = (float)((text >> 16) & 0xff) / 255.0f;
	color.g = (float)((text >> 8) & 0xff) / 255.0f;
	color.b = (float)(text & 0xff) / 255.0f;
	color.a = 1.0f;

	plutovg_surface_t *surface = plutosvg_document_render_to_surface(doc, NULL, size, size, &color, NULL, NULL);
	plutosvg_document_destroy(doc);
	free(svg_data);
	if(!surface) {
		return NULL;
	}

	uint32_t pixel_count = (uint32_t)(size * size);
	uint32_t *argb = (uint32_t *)malloc(pixel_count * sizeof(uint32_t));
	if(!argb) {
		plutovg_surface_destroy(surface);
		return NULL;
	}
	unsigned char *src = plutovg_surface_get_data(surface);
	int stride = plutovg_surface_get_stride(surface);
	for(int32_t row = 0; row < size; ++row) {
		uint32_t *src_row = (uint32_t *)(src + row * stride);
		uint32_t *dst_row = argb + row * size;
		memcpy(dst_row, src_row, (size_t)size * sizeof(uint32_t));
	}
	plutovg_surface_destroy(surface);
	return argb;
}

// Rasterize every icon in the current filtered set that doesn't already have
// a thumbnail cached. All storage is local to the browser; the main icon
// system is never touched.

// [=]===^=[ ibt_preload_filtered ]===================================[=]
static void ibt_preload_filtered(struct mkgui_ctx *ctx) {
	for(uint32_t i = 0; i < ibt->filtered_count; ++i) {
		uint32_t idx = ibt->filtered_idx[i];
		if(ibt->thumb[idx]) {
			continue;
		}
		ibt->thumb[idx] = ibt_rasterize(ctx, ibt->paths[idx], ibt->size);
	}
}

// Grid layout: items flow top-to-bottom in columns, columns flow left-to-right,
// horizontally scrollable. Each column has rows_per_col items stacked vertically.

#define IB_GRID_ROW_H_PX 22
#define IB_GRID_COL_W_PX 180
#define IB_GRID_PAD_PX   4

// [=]===^=[ ibt_grid_metrics ]=======================================[=]
static void ibt_grid_metrics(struct mkgui_ctx *ctx, int32_t ca_w, int32_t ca_h, int32_t *out_row_h, int32_t *out_col_w, int32_t *out_rows_per_col, int32_t *out_total_cols, int32_t *out_total_w) {
	int32_t row_h = sc(ctx, IB_GRID_ROW_H_PX);
	int32_t col_w = sc(ctx, IB_GRID_COL_W_PX);
	int32_t rows_per_col = ca_h / row_h;
	if(rows_per_col < 1) {
		rows_per_col = 1;
	}
	int32_t total_cols = ((int32_t)ibt->filtered_count + rows_per_col - 1) / rows_per_col;
	int32_t total_w = total_cols * col_w;
	if(total_w < ca_w) {
		total_w = ca_w;
	}
	*out_row_h = row_h;
	*out_col_w = col_w;
	*out_rows_per_col = rows_per_col;
	*out_total_cols = total_cols;
	*out_total_w = total_w;
}

// [=]===^=[ ibt_canvas_cb ]==========================================[=]
static void ibt_canvas_cb(struct mkgui_ctx *ctx, uint32_t id, uint32_t *pixels, int32_t x, int32_t y, int32_t w, int32_t h, void *userdata) {
	(void)id;
	(void)pixels;
	(void)userdata;

	int32_t row_h, col_w, rows_per_col, total_cols, total_w;
	ibt_grid_metrics(ctx, w, h, &row_h, &col_w, &rows_per_col, &total_cols, &total_w);
	(void)total_cols;
	(void)total_w;

	int32_t pad = sc(ctx, IB_GRID_PAD_PX);
	int32_t clip_x2 = x + w;
	int32_t clip_y2 = y + h;

	int32_t first_col = ibt->scroll_x / col_w;
	int32_t visible_cols = w / col_w + 2;
	int32_t ts = ibt->size;

	for(int32_t vc = 0; vc < visible_cols; ++vc) {
		int32_t c = first_col + vc;
		if(c < 0) {
			continue;
		}
		int32_t col_x = x + c * col_w - ibt->scroll_x;
		if(col_x >= clip_x2) {
			break;
		}

		if(col_x + col_w <= x) {
			continue;
		}

		for(int32_t r = 0; r < rows_per_col; ++r) {
			int32_t item = c * rows_per_col + r;
			if(item < 0 || item >= (int32_t)ibt->filtered_count) {
				break;
			}
			int32_t cy = y + r * row_h;

			if(item == ibt->selected) {
				draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, col_x, cy, col_w, row_h, ctx->theme.selection);
			}

			uint32_t theme_idx = ibt->filtered_idx[item];
			uint32_t *src = ibt->thumb[theme_idx];
			int32_t ix = col_x + pad;
			int32_t iy = cy + (row_h - ts) / 2;
			if(src) {
				for(int32_t py = 0; py < ts; ++py) {
					int32_t dy = iy + py;
					if(dy < y || dy >= clip_y2 || dy < 0 || dy >= ctx->win_h) {
						continue;
					}
					for(int32_t px = 0; px < ts; ++px) {
						int32_t dx = ix + px;
						if(dx < x || dx >= clip_x2 || dx < 0 || dx >= ctx->win_w) {
							continue;
						}
						uint32_t spx = src[py * ts + px];
						uint32_t alpha = (spx >> 24) & 0xff;
						if(alpha == 255) {
							ctx->pixels[(uint32_t)dy * (uint32_t)ctx->win_w + (uint32_t)dx] = spx;
						} else if(alpha > 0) {
							ctx->pixels[(uint32_t)dy * (uint32_t)ctx->win_w + (uint32_t)dx] = blend_pixel(ctx->pixels[(uint32_t)dy * (uint32_t)ctx->win_w + (uint32_t)dx], spx, (uint8_t)alpha);
						}
					}
				}
			}

			int32_t tx = ix + ts + pad;
			int32_t ty = cy + (row_h - ctx->font_height) / 2;
			uint32_t tc = (item == ibt->selected) ? ctx->theme.sel_text : ctx->theme.text;
			int32_t col_clip_r = col_x + col_w < clip_x2 ? col_x + col_w : clip_x2;
			int32_t col_clip_l = col_x > x ? col_x : x;
			push_text_clip(tx, ty, ibt->names[theme_idx], tc, col_clip_l, y, col_clip_r, clip_y2);
		}
	}
}

// [=]===^=[ ibt_hit_test ]===========================================[=]
static int32_t ibt_hit_test(struct mkgui_ctx *dlg, int32_t mx, int32_t my, uint32_t grid_id) {
	int32_t gi = find_widget_idx(dlg, grid_id);
	if(gi < 0) {
		return -1;
	}
	int32_t rx = dlg->rects[gi].x;
	int32_t ry = dlg->rects[gi].y;
	int32_t rw = dlg->rects[gi].w;
	int32_t rh = dlg->rects[gi].h;
	if(mx < rx || mx >= rx + rw || my < ry || my >= ry + rh) {
		return -1;
	}

	int32_t row_h, col_w, rows_per_col, total_cols, total_w;
	ibt_grid_metrics(dlg, rw, rh, &row_h, &col_w, &rows_per_col, &total_cols, &total_w);
	(void)total_cols;
	(void)total_w;

	int32_t local_x = (mx - rx) + ibt->scroll_x;
	int32_t local_y = my - ry;
	int32_t c = local_x / col_w;
	int32_t r = local_y / row_h;
	if(c < 0 || r < 0 || r >= rows_per_col) {
		return -1;
	}
	int32_t item = c * rows_per_col + r;
	if(item < 0 || item >= (int32_t)ibt->filtered_count) {
		return -1;
	}
	return item;
}

// Sync the scrollbar's range and value to the current layout state. Writes
// directly to the scrollbar's fields to preserve the current value across
// calls (mkgui_scrollbar_setup would reset it to 0 every time). Safe to call
// every frame and no-op before layout has produced a valid canvas rect.
// The scrollbar API treats max_value as total content size and page_size as
// visible portion; max scroll position is max_value - page_size.

// [=]===^=[ ibt_update_scroll_range ]================================[=]
static void ibt_update_scroll_range(struct mkgui_ctx *dlg, uint32_t grid_id, uint32_t scroll_id) {
	int32_t gi = find_widget_idx(dlg, grid_id);
	if(gi < 0) {
		return;
	}
	int32_t rw = dlg->rects[gi].w;
	int32_t rh = dlg->rects[gi].h;
	if(rw <= 0 || rh <= 0) {
		return;
	}

	struct mkgui_scrollbar_data *sb = find_scrollbar_data(dlg, scroll_id);
	if(!sb) {
		return;
	}

	int32_t row_h, col_w, rows_per_col, total_cols, total_w;
	ibt_grid_metrics(dlg, rw, rh, &row_h, &col_w, &rows_per_col, &total_cols, &total_w);

	sb->max_value = total_w;
	sb->page_size = rw;

	int32_t max_scroll = total_w > rw ? total_w - rw : 0;
	if(ibt->scroll_x > max_scroll) {
		ibt->scroll_x = max_scroll;
	}

	if(ibt->scroll_x < 0) {
		ibt->scroll_x = 0;
	}
	sb->value = ibt->scroll_x;
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

// Free every thumbnail buffer the browser rasterized. Called on theme switch
// (all thumbs are invalid) and on browser close.

// [=]===^=[ ibt_free_thumbs ]========================================[=]
static void ibt_free_thumbs(void) {
	for(uint32_t i = 0; i < ibt->total_count; ++i) {
		if(ibt->thumb[i]) {
			free(ibt->thumb[i]);
			ibt->thumb[i] = NULL;
		}
	}
}

// [=]===^=[ ibt_cleanup ]============================================[=]
static void ibt_cleanup(struct mkgui_ctx *ctx) {
	ibt_free_thumbs();
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
	ibt->selected = -1;
	ibt->last_click_item = -1;
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

	uint32_t wcount = 9;
	struct mkgui_widget widgets[9] = {
		MKGUI_W(MKGUI_WINDOW,    IB_TH_WINDOW,     "Icon Browser", "",          0,              IB_WIN_W, IB_WIN_H, 0,           0, 0),
		MKGUI_W(MKGUI_VBOX,      IB_TH_VBOX,       "",             "",          IB_TH_WINDOW,   0,        0,        0,           0, 0),
		MKGUI_W(MKGUI_HBOX,      IB_TH_HBOX_TOP,   "",             "",          IB_TH_VBOX,     0,        24,       MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_DROPDOWN,  IB_TH_THEME_DROP, "",             "",          IB_TH_HBOX_TOP, 180,      0,        MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_INPUT,     IB_TH_SEARCH,     "",             "edit-find", IB_TH_HBOX_TOP, 0,        0,        0,           0, 1),
		MKGUI_W(MKGUI_TABS,      IB_TH_TABS,       "",             "",          IB_TH_VBOX,     0,        28,       MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_TAB,       IB_TH_TAB_ALL,    "All",          "",          IB_TH_TABS,     0,        0,        0,           0, 0),
		MKGUI_W(MKGUI_CANVAS,    IB_TH_GRID,       "",             "",          IB_TH_VBOX,     0,        0,        0,           0, 1),
		MKGUI_W(MKGUI_SCROLLBAR, IB_TH_HSCROLL,    "",             "",          IB_TH_VBOX,     0,        14,       MKGUI_FIXED, 0, 0),
	};

	struct mkgui_ctx *dlg = mkgui_create_child(ctx, widgets, wcount, "Icon Browser", IB_WIN_W, IB_WIN_H);
	if(!dlg) {
		return 0;
	}
	mkgui_set_window_instance(dlg, "iconbrowser");
	mkgui_canvas_set_callback(dlg, IB_TH_GRID, ibt_canvas_cb, NULL);

	for(uint32_t i = 0; i < ibt->theme_count; ++i) {
		mkgui_dropdown_add(dlg, IB_TH_THEME_DROP, ibt->theme_names[i]);
	}
	mkgui_dropdown_set(dlg, IB_TH_THEME_DROP, 0);

	ibt_rebuild_tabs(dlg);

	ibt->active_cat = 0;
	ibt_filter(NULL);
	ibt_preload_filtered(dlg);

	// Bootstrap the scrollbar's aux data so find_scrollbar_data returns
	// non-NULL. Values are placeholders - the per-frame update installs the
	// real range once layout has produced a valid canvas rect.
	mkgui_scrollbar_setup(dlg, IB_TH_HSCROLL, 1, 1);

	mkgui_set_focus(dlg, IB_TH_SEARCH);

	uint32_t running = 1;
	int32_t last_sb_value = 0;
	uint32_t was_press = 0;
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
						if(ibt->selected >= 0 && ibt->selected < (int32_t)ibt->filtered_count) {
							ibt_confirm_selection((uint32_t)ibt->selected);
							running = 0;
						}
					}
				} break;

				case MKGUI_EVENT_DROPDOWN_CHANGED: {
					if(ev.id == IB_TH_THEME_DROP && ev.value >= 0 && ev.value < (int32_t)ibt->theme_count) {
						ibt_free_thumbs();
						ibt_load_theme((uint32_t)ev.value);
						ibt_rebuild_tabs(dlg);
						ibt->active_cat = 0;
						mkgui_tabs_set_current(dlg, IB_TH_TABS, IB_TH_TAB_ALL);
						const char *filter = mkgui_input_get(dlg, IB_TH_SEARCH);
						ibt_filter(filter);
						ibt_preload_filtered(dlg);
						ibt->selected = -1;
						ibt->scroll_x = 0;
						dirty_widget_id(dlg, IB_TH_GRID);
					}
				} break;

				case MKGUI_EVENT_INPUT_CHANGED: {
					if(ev.id == IB_TH_SEARCH) {
						const char *filter = mkgui_input_get(dlg, IB_TH_SEARCH);
						ibt_filter(filter);
						ibt_preload_filtered(dlg);
						ibt->selected = -1;
						ibt->scroll_x = 0;
						dirty_widget_id(dlg, IB_TH_GRID);
					}
				} break;

				case MKGUI_EVENT_SCROLL: {
					if(ev.id == IB_TH_GRID) {
						ibt->scroll_x += ev.value;
						dirty_widget_id(dlg, IB_TH_GRID);
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
						ibt_preload_filtered(dlg);
						ibt->selected = -1;
						ibt->scroll_x = 0;
						dirty_widget_id(dlg, IB_TH_GRID);
					}
				} break;

				default: break;
			}
		}

		// Detect press on the grid canvas: select on first click, confirm on
		// double-click. ctx->press_id is set by mkgui_poll; we only act on
		// the rising edge (new press this frame).
		if(dlg->press_id == IB_TH_GRID && !was_press) {
			int32_t item = ibt_hit_test(dlg, dlg->mouse_x, dlg->mouse_y, IB_TH_GRID);
			if(item >= 0) {
				uint32_t now = mkgui_time_ms();
				if(ibt->last_click_item == item && (now - ibt->last_click_time) < 400) {
					ibt_confirm_selection((uint32_t)item);
					running = 0;
				} else {
					ibt->selected = item;
					ibt->last_click_item = item;
					ibt->last_click_time = now;
					dirty_widget_id(dlg, IB_TH_GRID);
				}
			}
		}
		was_press = (dlg->press_id != 0);

		// Reconcile scrollbar and our scroll_x each frame. This is where
		// layout-driven updates happen (initial size, resize, filter change
		// all manifest as changes to the canvas rect).
		//
		// 1. Read the scrollbar. If it changed since we last wrote to it,
		//    the user moved it (drag, wheel, click) - copy into scroll_x.
		// 2. Re-run the range update: reads current canvas rect, rewrites
		//    max_value/page_size from current filtered_count, clamps
		//    scroll_x, writes scroll_x back to the scrollbar.
		// 3. Remember what we wrote so the next frame can detect user input.
		int32_t cur_sb = mkgui_scrollbar_get(dlg, IB_TH_HSCROLL);
		if(cur_sb != last_sb_value) {
			ibt->scroll_x = cur_sb;
			dirty_widget_id(dlg, IB_TH_GRID);
		}

		int32_t prev_scroll_x = ibt->scroll_x;
		ibt_update_scroll_range(dlg, IB_TH_GRID, IB_TH_HSCROLL);
		if(ibt->scroll_x != prev_scroll_x) {
			dirty_widget_id(dlg, IB_TH_GRID);
		}
		last_sb_value = ibt->scroll_x;

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
