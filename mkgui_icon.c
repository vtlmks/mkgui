// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT


// [=]===^=[ icon_find_idx ]==========================================[=]
static int32_t icon_find_idx(const char *name) {
	if(name[0] == '\0') {
		return -1;
	}
	return icon_hash_lookup(name);
}

// ctx used by icon_resolve for system theme lazy-load (set once at init)
#ifndef _WIN32
static struct mkgui_ctx *icon_lazy_ctx;
#endif

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

// [=]===^=[ icon_dir_exists ]========================================[=]
static uint32_t icon_dir_exists(const char *path) {
	return mkgui_path_is_dir(path);
}

// ---------------------------------------------------------------------------
// System icon theme support (Linux only)
// ---------------------------------------------------------------------------

#ifndef _WIN32
// [=]===^=[ icon_detect_theme_name ]================================[=]
// Detect the user's icon theme name from config files.
// Checks: $MKGUI_ICON_THEME env, gtk-4.0/gtk-3.0 settings, kdeglobals.
static void icon_detect_theme_name(char *out, uint32_t out_size) {
	out[0] = '\0';

	// 1. explicit override
	{
		const char *env = getenv("MKGUI_ICON_THEME");
		if(env && env[0]) {
			snprintf(out, out_size, "%s", env);
			return;
		}
	}

	// 2. XFCE xsettings.xml (only if XFCE is the current desktop session)
	{
		const char *current_desktop = getenv("XDG_CURRENT_DESKTOP");
		uint32_t is_xfce = current_desktop && strcasestr(current_desktop, "XFCE") != NULL;
		char path[2048];
		path[0] = '\0';
		if(is_xfce) {
			const char *config_home = getenv("XDG_CONFIG_HOME");
			if(config_home && config_home[0]) {
				snprintf(path, sizeof(path), "%s/xfce4/xfconf/xfce-perchannel-xml/xsettings.xml", config_home);
			} else {
				const char *home = getenv("HOME");
				if(home && home[0]) {
					snprintf(path, sizeof(path), "%s/.config/xfce4/xfconf/xfce-perchannel-xml/xsettings.xml", home);
				}
			}
		}

		if(path[0]) {
			FILE *fp = fopen(path, "r");
			if(fp) {
				char line[1024];
				while(fgets(line, sizeof(line), fp)) {
					char *name_pos = strstr(line, "name=\"IconThemeName\"");
					if(!name_pos) {
						continue;
					}
					char *value_pos = strstr(name_pos, "value=\"");
					if(!value_pos) {
						continue;
					}
					value_pos += 7;
					char *end = strchr(value_pos, '"');
					if(!end) {
						continue;
					}
					uint32_t len = (uint32_t)(end - value_pos);
					if(len > 0 && len < out_size) {
						memcpy(out, value_pos, len);
						out[len] = '\0';
					}
					break;
				}
				fclose(fp);
			}
		}

		if(out[0]) {
			return;
		}
	}

	// 3. parse gtk settings files for gtk-icon-theme-name
	{
		const char *config_home = getenv("XDG_CONFIG_HOME");
		char path[2048];
		static const char *gtk_files[] = {
			"%s/gtk-4.0/settings.ini",
			"%s/gtk-3.0/settings.ini",
			NULL
		};
		char config_dir[1024];
		if(config_home && config_home[0]) {
			snprintf(config_dir, sizeof(config_dir), "%s", config_home);
		} else {
			const char *home = getenv("HOME");
			if(home && home[0]) {
				snprintf(config_dir, sizeof(config_dir), "%s/.config", home);
			} else {
				config_dir[0] = '\0';
			}
		}

		if(config_dir[0]) {
			for(const char **gf = gtk_files; *gf && !out[0]; ++gf) {
				snprintf(path, sizeof(path), *gf, config_dir);
				FILE *fp = fopen(path, "r");
				if(!fp) {
					continue;
				}
				char line[512];
				while(fgets(line, sizeof(line), fp)) {
					char *p = line;
					while(*p == ' ' || *p == '\t') {
						++p;
					}
					if(strncmp(p, "gtk-icon-theme-name", 19) != 0) {
						continue;
					}
					p += 19;
					while(*p == ' ' || *p == '\t') {
						++p;
					}
					if(*p != '=') {
						continue;
					}
					++p;
					while(*p == ' ' || *p == '\t') {
						++p;
					}
					uint32_t len = (uint32_t)strlen(p);
					while(len > 0 && (p[len - 1] == '\n' || p[len - 1] == '\r' || p[len - 1] == ' ')) {
						--len;
					}

					if(len > 0 && len < out_size) {
						memcpy(out, p, len);
						out[len] = '\0';
					}
					break;
				}
				fclose(fp);
			}
		}

		if(out[0]) {
			return;
		}
	}

	// 3. KDE kdeglobals
	{
		const char *config_home = getenv("XDG_CONFIG_HOME");
		char path[2048];
		if(config_home && config_home[0]) {
			snprintf(path, sizeof(path), "%s/kdeglobals", config_home);
		} else {
			const char *home = getenv("HOME");
			if(home && home[0]) {
				snprintf(path, sizeof(path), "%s/.config/kdeglobals", home);
			} else {
				path[0] = '\0';
			}
		}

		if(path[0]) {
			FILE *fp = fopen(path, "r");
			if(fp) {
				uint32_t in_icons = 0;
				char line[512];
				while(fgets(line, sizeof(line), fp)) {
					char *p = line;
					while(*p == ' ' || *p == '\t') {
						++p;
					}
					if(p[0] == '[') {
						in_icons = (strncmp(p, "[Icons]", 7) == 0);
						continue;
					}

					if(!in_icons) {
						continue;
					}

					if(strncmp(p, "Theme", 5) != 0) {
						continue;
					}
					p += 5;
					while(*p == ' ' || *p == '\t') {
						++p;
					}
					if(*p != '=') {
						continue;
					}
					++p;
					while(*p == ' ' || *p == '\t') {
						++p;
					}
					uint32_t len = (uint32_t)strlen(p);
					while(len > 0 && (p[len - 1] == '\n' || p[len - 1] == '\r' || p[len - 1] == ' ')) {
						--len;
					}

					if(len > 0 && len < out_size) {
						memcpy(out, p, len);
						out[len] = '\0';
					}
					break;
				}
				fclose(fp);
			}
		}
	}

	// 4. fallback
	if(!out[0]) {
		snprintf(out, out_size, "hicolor");
	}
}

// [=]===^=[ icon_resolve_theme_dirs ]===============================[=]
// Resolve a theme name to one or more directory paths. Per the XDG Icon
// Theme Spec, user ($XDG_DATA_HOME/icons) takes precedence over system
// ($XDG_DATA_DIRS/icons). Writes up to max_out full paths, returns count.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
static uint32_t icon_resolve_theme_dirs(const char *theme_name, char (*out)[4096], uint32_t max_out) {
	uint32_t count = 0;

	// user-installed themes first (per XDG spec)
	if(count < max_out) {
		const char *data_home = getenv("XDG_DATA_HOME");
		char home_icons[2048];
		home_icons[0] = '\0';
		if(data_home && data_home[0]) {
			snprintf(home_icons, sizeof(home_icons), "%s/icons/%s", data_home, theme_name);
		} else {
			const char *home = getenv("HOME");
			if(home && home[0]) {
				snprintf(home_icons, sizeof(home_icons), "%s/.local/share/icons/%s", home, theme_name);
			}
		}

		if(home_icons[0] && icon_dir_exists(home_icons)) {
			snprintf(out[count++], 4096, "%s", home_icons);
		}
	}

	// system-wide XDG data dirs
	{
		const char *xdg_dirs = getenv("XDG_DATA_DIRS");
		const char *defaults = "/usr/share:/usr/local/share";
		const char *dirs = (xdg_dirs && xdg_dirs[0]) ? xdg_dirs : defaults;
		const char *p = dirs;
		while(*p && count < max_out) {
			const char *colon = p;
			while(*colon && *colon != ':') {
				++colon;
			}
			uint32_t seg_len = (uint32_t)(colon - p);
			if(seg_len > 0) {
				char candidate[4096];
				snprintf(candidate, sizeof(candidate), "%.*s/icons/%s", (int)seg_len, p, theme_name);
				if(icon_dir_exists(candidate)) {
					snprintf(out[count++], 4096, "%s", candidate);
				}
			}
			p = *colon ? colon + 1 : colon;
		}
	}

	return count;
}
#pragma GCC diagnostic pop

// [=]===^=[ icon_read_inherits ]=====================================[=]
// Read the Inherits= line from a theme's index.theme file.
// Writes a comma-separated list of parent theme names to out.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
static void icon_read_inherits(const char *theme_dir, char *out, uint32_t out_size) {
	out[0] = '\0';
	char path[4096];
	snprintf(path, sizeof(path), "%s/index.theme", theme_dir);
	FILE *fp = fopen(path, "r");
	if(!fp) {
		return;
	}
	char line[1024];
	while(fgets(line, sizeof(line), fp)) {
		char *p = line;
		while(*p == ' ' || *p == '\t') {
			++p;
		}
		if(strncmp(p, "Inherits", 8) != 0) {
			continue;
		}
		p += 8;
		while(*p == ' ' || *p == '\t') {
			++p;
		}
		if(*p != '=') {
			continue;
		}
		++p;
		while(*p == ' ' || *p == '\t') {
			++p;
		}
		uint32_t len = (uint32_t)strlen(p);
		while(len > 0 && (p[len - 1] == '\n' || p[len - 1] == '\r' || p[len - 1] == ' ')) {
			--len;
		}

		if(len > 0 && len < out_size) {
			memcpy(out, p, len);
			out[len] = '\0';
		}
		break;
	}
	fclose(fp);
}
#pragma GCC diagnostic pop

// [=]===^=[ icon_build_theme_chain ]=================================[=]
// Build the full theme search chain: primary theme + all inherited themes.
// Follows the Inherits= chain recursively, avoids duplicates.
// Also adds the base variant for -Dark/-Light themes (e.g. Papirus for Papirus-Dark).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
static void icon_build_theme_chain(struct mkgui_ctx *ctx, const char *theme_name) {
	ctx->system_theme_count = 0;

	// queue for BFS through inheritance
	char queue[MKGUI_THEME_CHAIN_MAX][256];
	uint32_t qhead = 0;
	uint32_t qtail = 0;
	snprintf(queue[qtail++], sizeof(queue[0]), "%s", theme_name);

	// if theme has a -Dark or -Light suffix, also queue the base name
	{
		uint32_t nlen = (uint32_t)strlen(theme_name);
		if(nlen > 5 && strcmp(theme_name + nlen - 5, "-Dark") == 0) {
			char base[256];
			snprintf(base, sizeof(base), "%.*s", (int)(nlen - 5), theme_name);
			snprintf(queue[qtail++], sizeof(queue[0]), "%s", base);
		} else if(nlen > 6 && strcmp(theme_name + nlen - 6, "-Light") == 0) {
			char base[256];
			snprintf(base, sizeof(base), "%.*s", (int)(nlen - 6), theme_name);
			snprintf(queue[qtail++], sizeof(queue[0]), "%s", base);
		}
	}

	// hicolor is the universal fallback per Freedesktop spec
	if(qtail < MKGUI_THEME_CHAIN_MAX) {
		snprintf(queue[qtail++], sizeof(queue[0]), "hicolor");
	}

	while(qhead < qtail && ctx->system_theme_count < MKGUI_THEME_CHAIN_MAX) {
		char *name = queue[qhead++];

		// resolve all paths for this theme name (user + system)
		char resolved[4][4096];
		uint32_t resolved_count = icon_resolve_theme_dirs(name, resolved, 4);
		if(resolved_count == 0) {
			continue;
		}

		// skip paths already in the chain, add new ones
		uint32_t first_added = UINT32_MAX;
		for(uint32_t r = 0; r < resolved_count && ctx->system_theme_count < MKGUI_THEME_CHAIN_MAX; ++r) {
			uint32_t dup = 0;
			for(uint32_t i = 0; i < ctx->system_theme_count; ++i) {
				if(strcmp(ctx->system_theme_dirs[i], resolved[r]) == 0) {
					dup = 1;
					break;
				}
			}

			if(dup) {
				continue;
			}

			if(first_added == UINT32_MAX) {
				first_added = ctx->system_theme_count;
			}
			snprintf(ctx->system_theme_dirs[ctx->system_theme_count], sizeof(ctx->system_theme_dirs[0]), "%s", resolved[r]);
			++ctx->system_theme_count;
		}

		if(first_added == UINT32_MAX) {
			continue;
		}

		// read Inherits= from the first located path for parent queueing
		char inherits[1024];
		icon_read_inherits(ctx->system_theme_dirs[first_added], inherits, sizeof(inherits));
		if(inherits[0]) {
			char *p = inherits;
			while(*p && qtail < MKGUI_THEME_CHAIN_MAX) {
				char *comma = p;
				while(*comma && *comma != ',') {
					++comma;
				}
				uint32_t seg_len = (uint32_t)(comma - p);
				while(seg_len > 0 && (p[seg_len - 1] == ' ' || p[seg_len - 1] == '\t')) {
					--seg_len;
				}
				while(seg_len > 0 && (*p == ' ' || *p == '\t')) {
					++p;
					--seg_len;
				}

				if(seg_len > 0 && seg_len < sizeof(queue[0])) {
					memcpy(queue[qtail], p, seg_len);
					queue[qtail][seg_len] = '\0';
					++qtail;
				}
				p = *comma ? comma + 1 : comma;
			}
		}
	}
}
#pragma GCC diagnostic pop

// [=]===^=[ icon_find_in_system_theme ]==============================[=]
// Search a Freedesktop icon theme hierarchy for a single icon by name.
// Returns 1 and writes the full SVG path to out, or 0 if not found.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
static uint32_t icon_find_in_system_theme(const char *theme_dir, const char *name, char *out, uint32_t out_size) {
	if(!theme_dir || !theme_dir[0] || !name || !name[0]) {
		return 0;
	}

	static const char *categories[] = {
		"actions", "places", "status", "devices", "mimetypes",
		"emblems", "apps", "panel", "emotes", "categories", NULL
	};
	static const int32_t sizes[] = { 16, 22, 24, 32, 48, 64 };

	// scalable first (best for SVG)
	for(const char **cat = categories; *cat; ++cat) {
		snprintf(out, out_size, "%s/scalable/%s/%s.svg", theme_dir, *cat, name);
		if(mkgui_path_readable(out)) {
			return 1;
		}
	}

	// fixed sizes
	for(uint32_t si = 0; si < 6; ++si) {
		for(const char **cat = categories; *cat; ++cat) {
			snprintf(out, out_size, "%s/%dx%d/%s/%s.svg", theme_dir, sizes[si], sizes[si], *cat, name);
			if(mkgui_path_readable(out)) {
				return 1;
			}
		}
	}

	// category-first layout (breeze, elementary)
	for(const char **cat = categories; *cat; ++cat) {
		snprintf(out, out_size, "%s/%s/scalable/%s.svg", theme_dir, *cat, name);
		if(mkgui_path_readable(out)) {
			return 1;
		}
		for(uint32_t si = 0; si < 6; ++si) {
			snprintf(out, out_size, "%s/%s/%d/%s.svg", theme_dir, *cat, sizes[si], name);
			if(mkgui_path_readable(out)) {
				return 1;
			}
		}
	}

	out[0] = '\0';
	return 0;
}
#pragma GCC diagnostic pop

// negative cache for icon names not found in the system theme
#define ICON_NEG_CACHE_SIZE 256
static char icon_neg_cache[ICON_NEG_CACHE_SIZE][MKGUI_ICON_NAME_LEN];
static uint32_t icon_neg_cache_count;

// [=]===^=[ icon_neg_cache_has ]=====================================[=]
static uint32_t icon_neg_cache_has(const char *name) {
	for(uint32_t i = 0; i < icon_neg_cache_count; ++i) {
		if(strcmp(icon_neg_cache[i], name) == 0) {
			return 1;
		}
	}
	return 0;
}

// [=]===^=[ icon_neg_cache_add ]=====================================[=]
static void icon_neg_cache_add(const char *name) {
	if(icon_neg_cache_count >= ICON_NEG_CACHE_SIZE) {
		return;
	}
	snprintf(icon_neg_cache[icon_neg_cache_count], MKGUI_ICON_NAME_LEN, "%s", name);
	++icon_neg_cache_count;
}

// [=]===^=[ icon_lazy_load_system ]=================================[=]
// Attempt to load an icon from the system theme chain. Searches primary
// theme first, then inherited themes in order.
static int32_t icon_lazy_load_system(struct mkgui_ctx *ctx, const char *name) {
	if(!ctx || ctx->system_theme_count == 0) {
		return -1;
	}

	if(icon_neg_cache_has(name)) {
		return -1;
	}

	char svg_path[4096];
	uint32_t found = 0;
	for(uint32_t ti = 0; ti < ctx->system_theme_count; ++ti) {
		if(icon_find_in_system_theme(ctx->system_theme_dirs[ti], name, svg_path, sizeof(svg_path))) {
			found = 1;
			break;
		}
	}

	if(!found) {
		icon_neg_cache_add(name);
		return -1;
	}

	uint32_t svg_len = 0;
	char *svg_data = svg_read_file(svg_path, &svg_len);
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
#endif

// [=]===^=[ icon_resolve ]===========================================[=]
static int32_t icon_resolve(const char *name) {
	int32_t idx = icon_find_idx(name);
	if(idx >= 0 && (uint32_t)idx >= icon_count) {
		return -1;
	}
#ifndef _WIN32
	if(idx < 0 && name[0] != '\0' && icon_lazy_ctx) {
		idx = icon_lazy_load_system(icon_lazy_ctx, name);
	}
#endif
	return idx;
}

// [=]===^=[ widget_icon_idx ]========================================[=]
static int32_t widget_icon_idx(struct mkgui_ctx *ctx, struct mkgui_widget *w) {
	(void)ctx;
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
			if(dx < 0) {
				dx = -dx;
			}
			if(dy < 0) {
				dy = -dy;
			}
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
MKGUI_API int32_t mkgui_icon_add(const char *name, const uint32_t *pixels, int32_t w, int32_t h) {
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
#ifndef _WIN32
	if(ctx->system_theme_count == 0) {
		char theme_name[256];
		icon_detect_theme_name(theme_name, sizeof(theme_name));
		icon_build_theme_chain(ctx, theme_name);
		icon_lazy_ctx = ctx;
		if(ctx->system_theme_count > 0) {
			fprintf(stderr, "mkgui: system icon theme chain:");
			for(uint32_t i = 0; i < ctx->system_theme_count; ++i) {
				fprintf(stderr, " %s", ctx->system_theme_dirs[i]);
			}
			fprintf(stderr, "\n");
		}
	}
#endif
	if(!dir_path) {
		return 0;
	}

	struct mkgui_dir d;
	if(!mkgui_dir_open(&d, dir_path)) {
		return 0;
	}

	uint32_t loaded = 0;
	struct mkgui_dir_entry *ent;
	while((ent = mkgui_dir_next(&d)) != NULL) {
		char *fname = ent->name;
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
	mkgui_dir_close(&d);

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

// [=]===^=[ icon_resolve_dir ]=======================================[=]
// Search order -- Linux:
// 1. $<APPNAME_UPPER>_ICON_DIR env override
// 2. <exe_dir>/../share/<app_name>/icons/  (relative to /proc/self/exe)
// 3. $XDG_DATA_HOME/<app_name>/icons/  (default: ~/.local/share/)
// 4. each $XDG_DATA_DIRS entry/<app_name>/icons/  (default: /usr/share:/usr/local/share)
// 5. ./icons/  (development fallback)
//
// Search order -- Windows:
// 1. %<APPNAME_UPPER>_ICON_DIR% env override
// 2. <exe_dir>/icons
// 3. %LOCALAPPDATA%/<app_name>/icons
// 4. %APPDATA%/<app_name>/icons
// 5. %ProgramFiles%/<app_name>/icons
// 6. %ProgramFiles(x86)%/<app_name>/icons
// 7. ./icons  (development fallback)
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
	// 2. <exe_dir>\icons
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

	// 3-6. per-user and system-wide install roots, in order of specificity
	{
		static const char *env_vars[] = {
			"LOCALAPPDATA",
			"APPDATA",
			"ProgramFiles",
			"ProgramFiles(x86)",
			NULL
		};
		for(uint32_t i = 0; env_vars[i]; ++i) {
			const char *base = getenv(env_vars[i]);
			if(!base || !base[0]) {
				continue;
			}
			snprintf(out, out_size, "%s\\%s\\icons", base, app_name);
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

#ifndef _WIN32
	if(ctx->system_theme_count == 0) {
		char theme_name[256];
		icon_detect_theme_name(theme_name, sizeof(theme_name));
		icon_build_theme_chain(ctx, theme_name);
		icon_lazy_ctx = ctx;
	}
#endif

	char resolved[4096];
	if(!icon_resolve_dir(app_name, resolved, sizeof(resolved))) {
#ifndef _WIN32
		if(ctx->system_theme_count > 0) {
			fprintf(stderr, "mkgui: no bundled icon directory for '%s', using system theme fallback\n", app_name);
			return 0;
		}
		fprintf(stderr,
			"mkgui: could not find icon directory for '%s'. Set $%s_ICON_DIR, "
			"install icons under $XDG_DATA_HOME/%s/icons/ or /usr/share/%s/icons/, "
			"or place an icons/ directory in the working directory.\n",
			app_name, app_name, app_name, app_name);
#else
		fprintf(stderr,
			"mkgui: could not find icon directory for '%s'. Place icons\\ next "
			"to the executable, install under %%LOCALAPPDATA%%\\%s\\icons\\ or "
			"%%ProgramFiles%%\\%s\\icons\\, or set %%%s_ICON_DIR%%.\n",
			app_name, app_name, app_name, app_name);
#endif
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
