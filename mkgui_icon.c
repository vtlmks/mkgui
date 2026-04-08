// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT


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
// strip <style>...</style> blocks that plutosvg cannot parse (breeze icons)
static void svg_strip_style_blocks(char *data, uint32_t len) {
	char *p = data;
	char *end = data + len;
	while(p < end) {
		char *open = strstr(p, "<style");
		if(!open || open >= end) {
			break;
		}
		char *close = strstr(open, "</style>");
		if(!close || close >= end) {
			break;
		}
		close += 8;
		memset(open, ' ', (size_t)(close - open));
		p = close;
	}
}

static int32_t svg_rasterize_icon_ex(const char *name, char *svg_data, uint32_t svg_len, int32_t target_size, uint32_t theme_text_color, uint32_t snap_native) {
	if(!svg_data || target_size <= 0) {
		return -1;
	}

	svg_strip_style_blocks(svg_data, svg_len);

	plutosvg_document_t *doc = plutosvg_document_load_from_data(svg_data, (int)svg_len, -1, -1, NULL, NULL);
	if(!doc) {
		return -1;
	}

	int32_t render_size = target_size;
	if(snap_native) {
		int32_t native_w = (int32_t)(plutosvg_document_get_width(doc) + 0.5f);
		if(native_w > 0 && native_w <= target_size) {
			int32_t mult = target_size / native_w;
			if(mult >= 1) {
				render_size = native_w * mult;
			}
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
	int32_t idx = icon_find_idx(name);
	if(idx >= 0 && (uint32_t)idx >= icon_count) {
		return -1;
	}
	return idx;
}

// [=]===^=[ widget_icon_idx ]========================================[=]
static int32_t widget_icon_idx(struct mkgui_widget *w) {
	if(w->icon[0] == '\0') {
		return -1;
	}
	int32_t idx = icon_resolve(w->icon);
	if(idx < 0) {
		return icon_find_idx("_missing");
	}
	return idx;
}

// [=]===^=[ icon_generate_missing ]=================================[=]
static void icon_generate_missing(void) {
	uint32_t sz = 16;
	uint32_t pixels[16 * 16];
	memset(pixels, 0, sizeof(pixels));
	uint32_t color = 0xffff00ff;
	int32_t cx = (int32_t)sz / 2;
	int32_t cy = (int32_t)sz / 2;
	int32_t r = (int32_t)sz / 2 - 2;
	for(uint32_t y = 0; y < sz; ++y) {
		for(uint32_t x = 0; x < sz; ++x) {
			int32_t dx = (int32_t)x - cx;
			int32_t dy = (int32_t)y - cy;
			if(dx < 0) { dx = -dx; }
			if(dy < 0) { dy = -dy; }
			if(dx + dy <= r) {
				pixels[y * sz + x] = color;
			}
		}
	}
	mkgui_icon_add("_missing", pixels, (int32_t)sz, (int32_t)sz);
}

// [=]===^=[ mkgui_icon_init ]========================================[=]
static void mkgui_icon_init(void) {
	icon_count = 0;
	icon_hash_clear();
	icon_generate_missing();
	icon_atlas_rebuild();
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

	if(idx >= 0) {
		icon_atlas_rebuild();
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

	icon_atlas_rebuild();
	return loaded;
}

// [=]===^=[ svg_rerasterize_all ]====================================[=]
static void svg_rerasterize_all(struct mkgui_ctx *ctx) {
	if(svg_source_count == 0) {
		return;
	}
	int32_t target_size = ctx->icon_size;
	uint32_t theme_color = ctx->theme.text & 0x00ffffff;
	for(uint32_t i = 0; i < svg_source_count; ++i) {
		svg_rasterize_icon_ex(svg_sources[i].name, svg_sources[i].svg_data, svg_sources[i].svg_len, target_size, theme_color, 1);
	}
	icon_atlas_rebuild();
}

// [=]===^=[ svg_cleanup ]============================================[=]
static void svg_cleanup(void) {
	for(uint32_t i = 0; i < svg_source_count; ++i) {
		free(svg_sources[i].svg_data);
		svg_sources[i].svg_data = NULL;
	}
	svg_source_count = 0;
}

// [=]===^=[ icon_dir_exists ]========================================[=]
static uint32_t icon_dir_exists(const char *path) {
	struct stat st;
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

// [=]===^=[ icon_resolve_dir ]=======================================[=]
// Search order:
// 1. $<APPNAME_UPPER>_ICON_DIR env override
// 2. <exe_dir>/../share/<app_name>/icons/  (relative to /proc/self/exe)
// 3. $XDG_DATA_HOME/<app_name>/icons/  (default: ~/.local/share/)
// 4. each $XDG_DATA_DIRS entry/<app_name>/icons/  (default: /usr/share:/usr/local/share)
// 5. ./icons/  (development fallback)
static uint32_t icon_resolve_dir(const char *app_name, char *out, uint32_t out_size) {
	if(!app_name || !app_name[0] || !out || out_size < 2) {
		return 0;
	}

	// 1. env override: <APPNAME>_ICON_DIR (uppercase, hyphens become underscores)
	{
		char env_name[128];
		uint32_t i = 0;
		for(const char *p = app_name; *p && i < sizeof(env_name) - 10; ++p, ++i) {
			if(*p >= 'a' && *p <= 'z') {
				env_name[i] = *p - 32;
			} else if(*p == '-') {
				env_name[i] = '_';
			} else {
				env_name[i] = *p;
			}
		}
		env_name[i] = '\0';
		strcat(env_name, "_ICON_DIR");
		const char *env = getenv(env_name);
		if(env && env[0] && icon_dir_exists(env)) {
			snprintf(out, out_size, "%s", env);
			return 1;
		}
	}

#ifndef _WIN32
	// 2. relative to executable via /proc/self/exe
	{
		char exe_path[2048];
		ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
		if(len > 0) {
			exe_path[len] = '\0';
			// strip executable name to get directory
			char *slash = strrchr(exe_path, '/');
			if(slash) {
				*slash = '\0';
				// strip one more level (bin/) to get prefix
				slash = strrchr(exe_path, '/');
				if(slash) {
					*slash = '\0';
					snprintf(out, out_size, "%s/share/%s/icons", exe_path, app_name);
					if(icon_dir_exists(out)) {
						return 1;
					}
				}
			}
		}
	}

	// 3. XDG_DATA_HOME/<app_name>/icons/ (default: ~/.local/share/)
	{
		const char *xdg_home = getenv("XDG_DATA_HOME");
		if(xdg_home && xdg_home[0]) {
			snprintf(out, out_size, "%s/%s/icons", xdg_home, app_name);
		} else {
			const char *home = getenv("HOME");
			if(home && home[0]) {
				snprintf(out, out_size, "%s/.local/share/%s/icons", home, app_name);
			} else {
				out[0] = '\0';
			}
		}
		if(out[0] && icon_dir_exists(out)) {
			return 1;
		}
	}

	// 4. each entry in XDG_DATA_DIRS/<app_name>/icons/ (default: /usr/share:/usr/local/share)
	{
		const char *xdg_dirs = getenv("XDG_DATA_DIRS");
		const char *defaults = "/usr/share:/usr/local/share";
		const char *dirs = (xdg_dirs && xdg_dirs[0]) ? xdg_dirs : defaults;
		const char *p = dirs;
		while(*p) {
			const char *colon = p;
			while(*colon && *colon != ':') {
				++colon;
			}
			uint32_t seg_len = (uint32_t)(colon - p);
			if(seg_len > 0) {
				snprintf(out, out_size, "%.*s/%s/icons", (int)seg_len, p, app_name);
				if(icon_dir_exists(out)) {
					return 1;
				}
			}
			p = *colon ? colon + 1 : colon;
		}
	}
#else
	// Windows: same directory as executable
	{
		char exe_path[2048];
		DWORD len = GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
		if(len > 0 && len < sizeof(exe_path)) {
			char *slash = strrchr(exe_path, '\\');
			if(slash) {
				*slash = '\0';
			}
			snprintf(out, out_size, "%s\\icons", exe_path);
			if(icon_dir_exists(out)) {
				return 1;
			}
		}
	}
#endif

	// 5. ./icons/ development fallback
	snprintf(out, out_size, "icons");
	if(icon_dir_exists(out)) {
		return 1;
	}

	out[0] = '\0';
	return 0;
}

// [=]===^=[ mkgui_icon_load_app_icons ]==============================[=]
MKGUI_API uint32_t mkgui_icon_load_app_icons(struct mkgui_ctx *ctx, const char *app_name) {
	MKGUI_CHECK_VAL(ctx, 0);
	if(!app_name || !app_name[0]) {
		return 0;
	}

	char resolved[4096];
	if(!icon_resolve_dir(app_name, resolved, sizeof(resolved))) {
		fprintf(stderr, "mkgui: could not find icon directory for '%s'\n", app_name);
		return 0;
	}

	snprintf(ctx->icon_dir, sizeof(ctx->icon_dir), "%s", resolved);
	fprintf(stderr, "mkgui: resolved icon directory: %s\n", resolved);
	return mkgui_icon_load_svg_dir(ctx, resolved);
}

// [=]===^=[ mkgui_icon_get_dir ]=====================================[=]
MKGUI_API const char *mkgui_icon_get_dir(struct mkgui_ctx *ctx) {
	MKGUI_CHECK_VAL(ctx, NULL);
	return ctx->icon_dir[0] ? ctx->icon_dir : NULL;
}
