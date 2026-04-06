// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define MKGUI_TOOLBAR_ICON_PREFIX "tb:"
#define MKGUI_TOOLBAR_ICON_PREFIX_LEN 3

// [=]===^=[ icon_find_idx ]==========================================[=]
static int32_t icon_find_idx(const char *name) {
	if(name[0] == '\0') {
		return -1;
	}
	return icon_hash_lookup(name);
}

// ---------------------------------------------------------------------------
// SVG icon support (forward declarations for resolve functions)
// ---------------------------------------------------------------------------

#define MKGUI_SVG_ICON_MAX 4096

struct mkgui_svg_source {
	char name[MKGUI_ICON_NAME_LEN];
	char *svg_data;
	uint32_t svg_len;
};

static struct mkgui_svg_source svg_sources[MKGUI_SVG_ICON_MAX];
static uint32_t svg_source_count;

// [=]===^=[ svg_read_file ]===========================================[=]
static char *svg_read_file(const char *path, uint32_t *out_len) {
	FILE *fp = fopen(path, "rb");
	if(!fp) {
		return NULL;
	}
	fseek(fp, 0, SEEK_END);
	long sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if(sz <= 0 || sz > 1024 * 1024) {
		fclose(fp);
		return NULL;
	}
	char *buf = (char *)malloc((size_t)sz + 1);
	if(!buf) {
		fclose(fp);
		return NULL;
	}
	if(fread(buf, 1, (size_t)sz, fp) != (size_t)sz) {
		free(buf);
		fclose(fp);
		return NULL;
	}
	fclose(fp);
	buf[sz] = '\0';
	if(out_len) {
		*out_len = (uint32_t)sz;
	}
	return buf;
}

// [=]===^=[ svg_rasterize_icon_ex ]==================================[=]
static int32_t svg_rasterize_icon_ex(const char *name, char *svg_data, uint32_t svg_len, int32_t target_size, uint32_t theme_text_color, uint32_t snap_native) {
	if(!svg_data || target_size <= 0) {
		return -1;
	}

	plutosvg_document_t *doc = plutosvg_document_load_from_data(svg_data, (int)svg_len, -1, -1, NULL, NULL);
	if(!doc) {
		return -1;
	}

	int32_t render_size = target_size;
	if(snap_native) {
		int32_t native_w = (int32_t)(plutosvg_document_get_width(doc) + 0.5f);
		if(native_w > 0) {
			int32_t mult = target_size / native_w;
			if(mult < 1) {
				mult = 1;
			}
			render_size = native_w * mult;
		}
	}

	plutovg_color_t current_color;
	current_color.r = (float)((theme_text_color >> 16) & 0xff) / 255.0f;
	current_color.g = (float)((theme_text_color >> 8) & 0xff) / 255.0f;
	current_color.b = (float)(theme_text_color & 0xff) / 255.0f;
	current_color.a = 1.0f;

	plutovg_surface_t *surface = plutosvg_document_render_to_surface(doc, NULL, render_size, render_size, &current_color, NULL, NULL);
	plutosvg_document_destroy(doc);
	if(!surface) {
		return -1;
	}

	uint32_t pixel_count = (uint32_t)(render_size * render_size);
	uint32_t *argb = (uint32_t *)malloc(pixel_count * sizeof(uint32_t));
	if(!argb) {
		plutovg_surface_destroy(surface);
		return -1;
	}

	unsigned char *src = plutovg_surface_get_data(surface);
	int stride = plutovg_surface_get_stride(surface);
	for(int32_t row = 0; row < render_size; ++row) {
		uint32_t *src_row = (uint32_t *)(src + row * stride);
		uint32_t *dst_row = argb + row * render_size;
		memcpy(dst_row, src_row, (size_t)render_size * sizeof(uint32_t));
	}
	plutovg_surface_destroy(surface);

	int32_t existing = icon_find_idx(name);
	if(existing >= 0) {
		struct mkgui_icon *ic = &icons[existing];
		if(ic->custom) {
			free(ic->pixels);
		}
		ic->pixels = argb;
		ic->w = render_size;
		ic->h = render_size;
		ic->custom = 1;
		return existing;
	}

	if(icon_count >= MKGUI_MAX_ICONS) {
		free(argb);
		return -1;
	}

	int32_t idx = (int32_t)icon_count;
	struct mkgui_icon *ic = &icons[icon_count++];
	snprintf(ic->name, MKGUI_ICON_NAME_LEN, "%s", name);
	ic->pixels = argb;
	ic->w = render_size;
	ic->h = render_size;
	ic->custom = 1;
	icon_hash_insert((uint32_t)idx);

	return idx;
}

// [=]===^=[ svg_find_source ]=========================================[=]
static struct mkgui_svg_source *svg_find_source(char *name) {
	for(uint32_t i = 0; i < svg_source_count; ++i) {
		if(strcmp(svg_sources[i].name, name) == 0) {
			return &svg_sources[i];
		}
	}
	return NULL;
}

// [=]===^=[ icon_resolve ]===========================================[=]
static int32_t icon_resolve(const char *name) {
	return icon_find_idx(name);
}

// [=]===^=[ toolbar_icon_resolve_ctx ]================================[=]
static int32_t toolbar_icon_resolve_ctx(struct mkgui_ctx *ctx, char *name) {
	char tb_name[MKGUI_ICON_NAME_LEN + MKGUI_TOOLBAR_ICON_PREFIX_LEN];
	snprintf(tb_name, sizeof(tb_name), "%s%s", MKGUI_TOOLBAR_ICON_PREFIX, name);
	int32_t idx = icon_find_idx(tb_name);
	if(idx >= 0) {
		return idx;
	}
	struct mkgui_svg_source *src = svg_find_source(name);
	if(src) {
		int32_t tb_icon_sz = ctx->toolbar_icon_size;
		return svg_rasterize_icon_ex(tb_name, src->svg_data, src->svg_len, tb_icon_sz, ctx->theme.text & 0x00ffffff, 0);
	}
	return -1;
}

// [=]===^=[ widget_icon_idx ]========================================[=]
static int32_t widget_icon_idx(struct mkgui_widget *w) {
	if(w->icon[0] == '\0') {
		return -1;
	}
	return icon_resolve(w->icon);
}

// [=]===^=[ toolbar_icon_idx ]========================================[=]
static int32_t toolbar_icon_idx(struct mkgui_ctx *ctx, struct mkgui_widget *w) {
	if(w->icon[0] == '\0') {
		return -1;
	}
	return toolbar_icon_resolve_ctx(ctx, w->icon);
}

// [=]===^=[ mkgui_icon_init ]========================================[=]
static void mkgui_icon_init(void) {
	icon_count = 0;
	icon_hash_clear();
}

// [=]===^=[ icon_load_from_widgets ]=================================[=]
static void icon_load_from_widgets(struct mkgui_ctx *ctx) {
	for(uint32_t i = 0; i < ctx->widget_count; ++i) {
		if(ctx->widgets[i].icon[0] != '\0') {
			icon_resolve(ctx->widgets[i].icon);
		}
	}
}

// [=]===^=[ mkgui_icon_add ]=========================================[=]
MKGUI_API int32_t mkgui_icon_add(const char *name, uint32_t *pixels, int32_t w, int32_t h) {
	if(icon_count >= MKGUI_MAX_ICONS) {
		return -1;
	}
	if(!name || name[0] == '\0' || !pixels || w <= 0 || h <= 0) {
		return -1;
	}

	int32_t existing = icon_find_idx(name);
	if(existing >= 0) {
		struct mkgui_icon *ic = &icons[existing];
		if(ic->custom) {
			free(ic->pixels);
		}
		uint32_t pixel_count = (uint32_t)(w * h);
		ic->pixels = (uint32_t *)malloc(pixel_count * sizeof(uint32_t));
		if(!ic->pixels) {
			return -1;
		}
		memcpy(ic->pixels, pixels, pixel_count * sizeof(uint32_t));
		ic->w = w;
		ic->h = h;
		ic->custom = 1;
		icon_atlas_rebuild();
		return existing;
	}

	uint32_t pixel_count = (uint32_t)(w * h);
	uint32_t *dst = (uint32_t *)malloc(pixel_count * sizeof(uint32_t));
	if(!dst) {
		return -1;
	}
	memcpy(dst, pixels, pixel_count * sizeof(uint32_t));

	int32_t idx = (int32_t)icon_count;
	struct mkgui_icon *ic = &icons[icon_count++];
	snprintf(ic->name, MKGUI_ICON_NAME_LEN, "%s", name);
	ic->pixels = dst;
	ic->w = w;
	ic->h = h;
	ic->custom = 1;
	icon_hash_insert((uint32_t)idx);
	icon_atlas_rebuild();

	return idx;
}

// [=]===^=[ mkgui_set_icon ]=========================================[=]
MKGUI_API void mkgui_set_icon(struct mkgui_ctx *ctx, uint32_t widget_id, const char *icon_name) {
	MKGUI_CHECK(ctx);
	struct mkgui_widget *w = find_widget(ctx, widget_id);
	if(!w) {
		return;
	}
	if(!icon_name) {
		w->icon[0] = '\0';
		dirty_all(ctx);
		return;
	}
	snprintf(w->icon, MKGUI_ICON_NAME_LEN, "%s", icon_name);
	icon_resolve(icon_name);
	dirty_all(ctx);
}

// [=]===^=[ mkgui_set_treenode_icon ]================================[=]
MKGUI_API void mkgui_set_treenode_icon(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id, const char *icon_name) {
	MKGUI_CHECK(ctx);
	struct mkgui_treeview_data *tv = find_treeview_data(ctx, widget_id);
	if(!tv) {
		return;
	}
	int32_t idx = icon_resolve(icon_name);
	for(uint32_t i = 0; i < tv->node_count; ++i) {
		if(tv->nodes[i].id == node_id) {
			tv->nodes[i].icon_idx = idx;
			dirty_all(ctx);
			return;
		}
	}
}



// [=]===^=[ mkgui_icon_load_svg ]====================================[=]
MKGUI_API int32_t mkgui_icon_load_svg(struct mkgui_ctx *ctx, const char *name, const char *path) {
	MKGUI_CHECK_VAL(ctx, -1);
	if(!name || !path || name[0] == '\0') {
		return -1;
	}



	uint32_t svg_len = 0;
	char *svg_data = svg_read_file(path, &svg_len);
	if(!svg_data) {
		return -1;
	}

	int32_t target_size = ctx->icon_size;
	uint32_t theme_color = ctx->theme.text & 0x00ffffff;
	int32_t idx = svg_rasterize_icon_ex(name, svg_data, svg_len, target_size, theme_color, 1);

	if(idx >= 0 && svg_source_count < MKGUI_SVG_ICON_MAX) {
		struct mkgui_svg_source *src = &svg_sources[svg_source_count++];
		snprintf(src->name, MKGUI_ICON_NAME_LEN, "%s", name);
		src->svg_data = svg_data;
		src->svg_len = svg_len;
	} else {
		free(svg_data);
	}

	return idx;
}

// [=]===^=[ mkgui_icon_load_svg_dir ]================================[=]
MKGUI_API uint32_t mkgui_icon_load_svg_dir(struct mkgui_ctx *ctx, const char *dir_path) {
	MKGUI_CHECK_VAL(ctx, 0);
	if(!dir_path) {
		return 0;
	}

	DIR *d = opendir(dir_path);
	if(!d) {
		return 0;
	}

	uint32_t loaded = 0;
	struct dirent *ent;
	while((ent = readdir(d)) != NULL) {
		char *fname = ent->d_name;
		uint32_t len = (uint32_t)strlen(fname);
		if(len < 5 || strcmp(fname + len - 4, ".svg") != 0) {
			continue;
		}

		char name[MKGUI_ICON_NAME_LEN];
		uint32_t name_len = len - 4;
		if(name_len >= MKGUI_ICON_NAME_LEN) {
			name_len = MKGUI_ICON_NAME_LEN - 1;
		}
		memcpy(name, fname, name_len);
		name[name_len] = '\0';

		if(icon_find_idx(name) >= 0) {
			continue;
		}

		char full_path[4096];
		uint32_t dir_len = (uint32_t)strlen(dir_path);
		if(dir_len + 1 + len >= sizeof(full_path)) {
			continue;
		}
		memcpy(full_path, dir_path, dir_len);
		full_path[dir_len] = '/';
		memcpy(full_path + dir_len + 1, fname, len + 1);

		uint32_t svg_len = 0;
		char *svg_data = svg_read_file(full_path, &svg_len);
		if(!svg_data) {
			continue;
		}

		int32_t target_size = ctx->icon_size;
		uint32_t theme_color = ctx->theme.text & 0x00ffffff;
		int32_t idx = svg_rasterize_icon_ex(name, svg_data, svg_len, target_size, theme_color, 1);
		if(idx >= 0) {
			++loaded;
			if(svg_source_count < MKGUI_SVG_ICON_MAX) {
				struct mkgui_svg_source *src = &svg_sources[svg_source_count++];
				snprintf(src->name, MKGUI_ICON_NAME_LEN, "%s", name);
				src->svg_data = svg_data;
				src->svg_len = svg_len;
			} else {
				free(svg_data);
			}
		} else {
			free(svg_data);
		}
	}
	closedir(d);

	if(loaded > 0) {
		fprintf(stderr, "mkgui: loaded %u SVG icons from %s\n", loaded, dir_path);
	}

	char tb_dir[4096];
	snprintf(tb_dir, sizeof(tb_dir), "%s/toolbar", dir_path);
	DIR *td = opendir(tb_dir);
	if(td) {
		uint32_t tb_loaded = 0;
		while((ent = readdir(td)) != NULL) {
			char *fname = ent->d_name;
			uint32_t len = (uint32_t)strlen(fname);
			if(len < 5 || strcmp(fname + len - 4, ".svg") != 0) {
				continue;
			}

			char name[MKGUI_ICON_NAME_LEN];
			uint32_t name_len = len - 4;
			if(name_len >= MKGUI_ICON_NAME_LEN) {
				name_len = MKGUI_ICON_NAME_LEN - 1;
			}
			memcpy(name, fname, name_len);
			name[name_len] = '\0';

			char tb_name[MKGUI_ICON_NAME_LEN + MKGUI_TOOLBAR_ICON_PREFIX_LEN];
			snprintf(tb_name, sizeof(tb_name), "%s%s", MKGUI_TOOLBAR_ICON_PREFIX, name);

			if(icon_find_idx(tb_name) >= 0) {
				continue;
			}

			char full_path[4096];
			uint32_t td_len = (uint32_t)strlen(tb_dir);
			if(td_len + 1 + len >= sizeof(full_path)) {
				continue;
			}
			memcpy(full_path, tb_dir, td_len);
			full_path[td_len] = '/';
			memcpy(full_path + td_len + 1, fname, len + 1);

			uint32_t svg_len = 0;
			char *svg_data = svg_read_file(full_path, &svg_len);
			if(!svg_data) {
				continue;
			}

			int32_t target_size = ctx->toolbar_icon_size;
			uint32_t theme_color = ctx->theme.text & 0x00ffffff;
			int32_t idx = svg_rasterize_icon_ex(tb_name, svg_data, svg_len, target_size, theme_color, 0);
			if(idx >= 0) {
				++tb_loaded;
				if(svg_source_count < MKGUI_SVG_ICON_MAX) {
					struct mkgui_svg_source *src = &svg_sources[svg_source_count++];
					snprintf(src->name, MKGUI_ICON_NAME_LEN, "%s", name);
					src->svg_data = svg_data;
					src->svg_len = svg_len;
				} else {
					free(svg_data);
				}
			} else {
				free(svg_data);
			}
		}
		closedir(td);
		if(tb_loaded > 0) {
			fprintf(stderr, "mkgui: loaded %u toolbar SVG icons from %s\n", tb_loaded, tb_dir);
		}
	}

	icon_atlas_rebuild();
	return loaded;
}

// [=]===^=[ svg_rerasterize_all ]====================================[=]
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
static void svg_rerasterize_all(struct mkgui_ctx *ctx) {
	if(svg_source_count == 0) {
		return;
	}
	int32_t target_size = ctx->icon_size;
	int32_t tb_size = ctx->toolbar_icon_size;
	uint32_t theme_color = ctx->theme.text & 0x00ffffff;
	for(uint32_t i = 0; i < svg_source_count; ++i) {
		svg_rasterize_icon_ex(svg_sources[i].name, svg_sources[i].svg_data, svg_sources[i].svg_len, target_size, theme_color, 1);
		char tb_name[MKGUI_ICON_NAME_LEN + MKGUI_TOOLBAR_ICON_PREFIX_LEN];
		snprintf(tb_name, sizeof(tb_name), "%s%s", MKGUI_TOOLBAR_ICON_PREFIX, svg_sources[i].name);
		if(icon_find_idx(tb_name) >= 0) {
			svg_rasterize_icon_ex(tb_name, svg_sources[i].svg_data, svg_sources[i].svg_len, tb_size, theme_color, 0);
		}
	}
	icon_atlas_rebuild();
}
#pragma GCC diagnostic pop

// [=]===^=[ svg_cleanup ]============================================[=]
static void svg_cleanup(void) {
	for(uint32_t i = 0; i < svg_source_count; ++i) {
		free(svg_sources[i].svg_data);
		svg_sources[i].svg_data = NULL;
	}
	svg_source_count = 0;
}
