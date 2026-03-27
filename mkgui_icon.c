// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define MKGUI_TOOLBAR_ICON_PREFIX "tb:"
#define MKGUI_TOOLBAR_ICON_PREFIX_LEN 3

// [=]===^=[ mdi_pack_load ]===========================================[=]
static uint32_t mdi_pack_load(struct mdi_pack *pack, const char *path) {
	FILE *fp = fopen(path, "rb");
	if(!fp) {
		return 0;
	}
	fseek(fp, 0, SEEK_END);
	long sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if(sz < 8) {
		fclose(fp);
		return 0;
	}

	pack->dat = (uint8_t *)malloc((size_t)sz);
	if(!pack->dat) {
		fclose(fp);
		return 0;
	}
	if(fread(pack->dat, 1, (size_t)sz, fp) != (size_t)sz) {
		free(pack->dat);
		pack->dat = NULL;
		fclose(fp);
		return 0;
	}
	fclose(fp);
	pack->dat_size = (uint32_t)sz;
	pack->dat_owned = 1;

	if(memcmp(pack->dat, "MKIC", 4) != 0) {
		free(pack->dat);
		pack->dat = NULL;
		return 0;
	}

	memcpy(&pack->icon_size, pack->dat + 4, 2);
	memcpy(&pack->icon_count, pack->dat + 6, 2);

	pack->name_block = (const char *)(pack->dat + 8);

	uint32_t name_block_size = 0;
	for(uint16_t i = 0; i < pack->icon_count; ++i) {
		name_block_size += (uint32_t)strlen(pack->name_block + name_block_size) + 1;
	}
	uint32_t name_block_padded = (name_block_size + 3) & ~3u;

	pack->name_offsets = (const uint32_t *)(pack->dat + 8 + name_block_padded);
	pack->pixel_data = (const uint8_t *)(pack->dat + 8 + name_block_padded + (uint32_t)pack->icon_count * 4);

	return 1;
}

// [=]===^=[ mdi_pack_load_mem ]=======================================[=]
static uint32_t mdi_pack_load_mem(struct mdi_pack *pack, const uint8_t *data, uint32_t size) {
	if(!data || size < 8) {
		return 0;
	}
	if(memcmp(data, "MKIC", 4) != 0) {
		return 0;
	}

	pack->dat = (uint8_t *)data;
	pack->dat_size = size;
	pack->dat_owned = 0;

	memcpy(&pack->icon_size, data + 4, 2);
	memcpy(&pack->icon_count, data + 6, 2);

	pack->name_block = (const char *)(data + 8);

	uint32_t name_block_size = 0;
	for(uint16_t i = 0; i < pack->icon_count; ++i) {
		name_block_size += (uint32_t)strlen(pack->name_block + name_block_size) + 1;
	}
	uint32_t name_block_padded = (name_block_size + 3) & ~3u;

	pack->name_offsets = (const uint32_t *)(data + 8 + name_block_padded);
	pack->pixel_data = (const uint8_t *)(data + 8 + name_block_padded + (uint32_t)pack->icon_count * 4);

	return 1;
}

// [=]===^=[ mdi_dat_free ]===========================================[=]
static void mdi_dat_free(void) {
	for(uint32_t i = 0; i < icon_count; ++i) {
		if(icons[i].custom) {
			free(icons[i].pixels);
		}
	}
	if(mdi_toolbar.dat && mdi_toolbar.dat != mdi.dat && mdi_toolbar.dat_owned) {
		free(mdi_toolbar.dat);
	}
	if(mdi.dat && mdi.dat_owned) {
		free(mdi.dat);
	}
	memset(&mdi, 0, sizeof(mdi));
	memset(&mdi_toolbar, 0, sizeof(mdi_toolbar));
}

// [=]===^=[ mdi_pack_lookup ]=========================================[=]
static int32_t mdi_pack_lookup(struct mdi_pack *pack, const char *name) {
	if(!pack->dat || pack->icon_count == 0) {
		return -1;
	}
	int32_t lo = 0;
	int32_t hi = (int32_t)pack->icon_count - 1;
	while(lo <= hi) {
		int32_t mid = (lo + hi) / 2;
		const char *mid_name = pack->name_block + pack->name_offsets[mid];
		int32_t cmp = strcmp(name, mid_name);
		if(cmp == 0) {
			return mid;
		} else if(cmp < 0) {
			hi = mid - 1;
		} else {
			lo = mid + 1;
		}
	}
	return -1;
}

// [=]===^=[ mdi_pack_tint ]===========================================[=]
static void mdi_pack_tint(struct mdi_pack *pack, int32_t mdi_idx, uint32_t color, uint32_t *dst, uint32_t size) {
	uint32_t pixel_count = size * size;
	const uint8_t *src = pack->pixel_data + (uint32_t)mdi_idx * pixel_count;
	uint8_t cr = (color >> 16) & 0xff;
	uint8_t cg = (color >> 8) & 0xff;
	uint8_t cb = color & 0xff;
	for(uint32_t i = 0; i < pixel_count; ++i) {
		uint8_t a = src[i];
		if(a > 0) {
			uint32_t pr = (uint32_t)cr * a / 255;
			uint32_t pg = (uint32_t)cg * a / 255;
			uint32_t pb = (uint32_t)cb * a / 255;
			dst[i] = ((uint32_t)a << 24) | (pr << 16) | (pg << 8) | pb;
		} else {
			dst[i] = 0;
		}
	}
}

// [=]===^=[ icon_load_from_pack ]=====================================[=]
static int32_t icon_load_from_pack(struct mdi_pack *pack, const char *name, const char *cache_name) {
	if(icon_count >= MKGUI_MAX_ICONS) {
		return -1;
	}

	int32_t mi = mdi_pack_lookup(pack, name);
	if(mi < 0) {
		return -1;
	}

	uint32_t sz = (uint32_t)pack->icon_size;
	uint32_t pixel_count = sz * sz;
	if(icon_pixels_used + pixel_count > MKGUI_ICON_PIXEL_POOL) {
		return -1;
	}

	uint32_t *dst = &icon_pixels[icon_pixels_used];
	mdi_pack_tint(pack, mi, icon_text_color, dst, sz);
	icon_pixels_used += pixel_count;

	int32_t idx = (int32_t)icon_count;
	struct mkgui_icon *ic = &icons[icon_count++];
	strncpy(ic->name, cache_name, MKGUI_ICON_NAME_LEN - 1);
	ic->name[MKGUI_ICON_NAME_LEN - 1] = '\0';
	ic->pixels = dst;
	ic->w = (int32_t)sz;
	ic->h = (int32_t)sz;
	ic->custom = 0;
	icon_hash_insert((uint32_t)idx);

	return idx;
}

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
static NSVGrasterizer *svg_rasterizer;

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

// [=]===^=[ svg_inline_css ]==========================================[=]
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
static void svg_inline_css(char *svg, uint32_t len) {
	char classes[16][48];
	char colors[16][32];
	uint32_t class_count = 0;

	char *style_start = strstr(svg, "<style");
	if(!style_start) {
		return;
	}
	char *style_body = strchr(style_start, '>');
	if(!style_body) {
		return;
	}
	++style_body;
	char *style_end = strstr(style_body, "</style>");
	if(!style_end) {
		return;
	}

	char *p = style_body;
	while(p < style_end && class_count < 16) {
		char *dot = strchr(p, '.');
		if(!dot || dot >= style_end) {
			break;
		}
		char *brace = strchr(dot, '{');
		if(!brace || brace >= style_end) {
			break;
		}
		char *close = strchr(brace, '}');
		if(!close || close >= style_end) {
			break;
		}

		uint32_t name_len = (uint32_t)(brace - dot - 1);
		while(name_len > 0 && (dot[1 + name_len - 1] == ' ' || dot[1 + name_len - 1] == '\t')) {
			--name_len;
		}
		if(name_len >= 48) {
			name_len = 47;
		}
		memcpy(classes[class_count], dot + 1, name_len);
		classes[class_count][name_len] = '\0';

		char *color = strstr(brace, "color:");
		if(color && color < close) {
			color += 6;
			while(*color == ' ') {
				++color;
			}
			char *cend = color;
			while(cend < close && *cend != ';' && *cend != '}' && *cend != ' ') {
				++cend;
			}
			uint32_t clen = (uint32_t)(cend - color);
			if(clen >= 32) {
				clen = 31;
			}
			memcpy(colors[class_count], color, clen);
			colors[class_count][clen] = '\0';
			++class_count;
		}
		p = close + 1;
	}

	if(class_count == 0) {
		return;
	}

	for(uint32_t ci = 0; ci < class_count; ++ci) {
		char search[256];
		snprintf(search, sizeof(search), "class=\"%s\"", classes[ci]);
		uint32_t search_len = (uint32_t)strlen(search);

		char *pos = svg;
		while((pos = strstr(pos, search)) != NULL) {
			char *elem_start = pos;
			while(elem_start > svg && *elem_start != '<') {
				--elem_start;
			}

			char *fill_cc = strstr(elem_start, "currentColor");
			if(fill_cc && fill_cc < pos + search_len + 200) {
				uint32_t color_len = (uint32_t)strlen(colors[ci]);
				uint32_t cc_len = 12;
				if(color_len <= cc_len) {
					memcpy(fill_cc, colors[ci], color_len);
					for(uint32_t pad = color_len; pad < cc_len; ++pad) {
						fill_cc[pad] = ' ';
					}
				}
			}
			pos += search_len;
		}
	}
	(void)len;
}
#pragma GCC diagnostic pop

// [=]===^=[ svg_apply_theme_color ]==================================[=]
static char *svg_apply_theme_color(const char *svg_data, uint32_t svg_len, uint32_t theme_text_color) {
	char *buf = (char *)malloc(svg_len + 1);
	if(!buf) {
		return NULL;
	}
	memcpy(buf, svg_data, svg_len + 1);

	char hex[8];
	snprintf(hex, sizeof(hex), "#%02x%02x%02x",
		(theme_text_color >> 16) & 0xff,
		(theme_text_color >> 8) & 0xff,
		theme_text_color & 0xff);

	char *pos = buf;
	while((pos = strstr(pos, "currentColor")) != NULL) {
		memcpy(pos, hex, 7);
		for(uint32_t pad = 7; pad < 12; ++pad) {
			pos[pad] = ' ';
		}
		pos += 12;
	}
	return buf;
}

// [=]===^=[ svg_rasterize_icon ]=====================================[=]
static int32_t svg_rasterize_icon(const char *name, const char *svg_data, uint32_t svg_len, int32_t target_size, uint32_t theme_text_color) {
	if(!svg_data || !svg_rasterizer || target_size <= 0) {
		return -1;
	}

	char *colored = svg_apply_theme_color(svg_data, svg_len, theme_text_color);
	if(!colored) {
		return -1;
	}

	NSVGimage *image = nsvgParse(colored, "px", 96.0f);
	free(colored);
	if(!image) {
		return -1;
	}

	float svg_w = image->width;
	float svg_h = image->height;
	if(svg_w <= 0.0f || svg_h <= 0.0f) {
		nsvgDelete(image);
		return -1;
	}

	int32_t native_w = (int32_t)(svg_w + 0.5f);
	int32_t native_h = (int32_t)(svg_h + 0.5f);
	int32_t render_size = target_size;
	if(native_w > 0 && native_h > 0) {
		int32_t mult = target_size / native_w;
		if(mult < 1) {
			mult = 1;
		}
		render_size = native_w * mult;
	}

	float svg_scale = (float)render_size / svg_w;

	uint32_t pixel_count = (uint32_t)(render_size * render_size);
	uint8_t *rgba = (uint8_t *)calloc(pixel_count * 4, 1);
	if(!rgba) {
		nsvgDelete(image);
		return -1;
	}

	nsvgRasterize(svg_rasterizer, image, 0, 0, svg_scale, rgba, render_size, render_size, render_size * 4);
	nsvgDelete(image);

	uint32_t *argb = (uint32_t *)malloc(pixel_count * sizeof(uint32_t));
	if(!argb) {
		free(rgba);
		return -1;
	}
	for(uint32_t i = 0; i < pixel_count; ++i) {
		uint32_t r = rgba[i * 4 + 0];
		uint32_t g = rgba[i * 4 + 1];
		uint32_t b = rgba[i * 4 + 2];
		uint32_t a = rgba[i * 4 + 3];
		argb[i] = (a << 24) | (r << 16) | (g << 8) | b;
	}
	free(rgba);

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
	strncpy(ic->name, name, MKGUI_ICON_NAME_LEN - 1);
	ic->name[MKGUI_ICON_NAME_LEN - 1] = '\0';
	ic->pixels = argb;
	ic->w = render_size;
	ic->h = render_size;
	ic->custom = 1;
	icon_hash_insert((uint32_t)idx);

	return idx;
}

// [=]===^=[ svg_find_source ]=========================================[=]
static struct mkgui_svg_source *svg_find_source(const char *name) {
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
	if(idx >= 0) {
		return idx;
	}
	return icon_load_from_pack(&mdi, name, name);
}

// [=]===^=[ toolbar_icon_resolve_ctx ]================================[=]
static int32_t toolbar_icon_resolve_ctx(struct mkgui_ctx *ctx, const char *name) {
	char tb_name[MKGUI_ICON_NAME_LEN + MKGUI_TOOLBAR_ICON_PREFIX_LEN];
	snprintf(tb_name, sizeof(tb_name), "%s%s", MKGUI_TOOLBAR_ICON_PREFIX, name);
	int32_t idx = icon_find_idx(tb_name);
	if(idx >= 0) {
		return idx;
	}
	idx = icon_load_from_pack(&mdi_toolbar, name, tb_name);
	if(idx >= 0) {
		return idx;
	}
	struct mkgui_svg_source *src = svg_find_source(name);
	if(src && svg_rasterizer) {
		int32_t tb_icon_sz = ctx->toolbar_icon_size;
		return svg_rasterize_icon(tb_name, src->svg_data, src->svg_len, tb_icon_sz, ctx->theme.text & 0x00ffffff);
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

// [=]===^=[ mkgui_icons_load_both ]====================================[=]
static void mkgui_icons_load_both(const char *path, const char *toolbar_path) {
	if(path) {
		if(mdi_pack_load(&mdi, path)) {
			fprintf(stderr, "mkgui: icon pack loaded from %s (%u icons, %ux%u)\n", path, mdi.icon_count, mdi.icon_size, mdi.icon_size);
		}
	}
	if(toolbar_path) {
		if(mdi_pack_load(&mdi_toolbar, toolbar_path)) {
			fprintf(stderr, "mkgui: toolbar icon pack loaded from %s (%u icons, %ux%u)\n", toolbar_path, mdi_toolbar.icon_count, mdi_toolbar.icon_size, mdi_toolbar.icon_size);
		}
	}
	if(mdi.dat && !mdi_toolbar.dat) {
		mdi_toolbar = mdi;
	}
}

// [=]===^=[ mkgui_set_icon_data ]=====================================[=]
MKGUI_API void mkgui_set_icon_data(const uint8_t *icons_dat, uint32_t icons_size, const uint8_t *toolbar_dat, uint32_t toolbar_size) {
	if(icons_dat && icons_size > 0) {
		mdi_pack_load_mem(&mdi, icons_dat, icons_size);
	}
	if(toolbar_dat && toolbar_size > 0) {
		mdi_pack_load_mem(&mdi_toolbar, toolbar_dat, toolbar_size);
	}
	if(mdi.dat && !mdi_toolbar.dat) {
		mdi_toolbar = mdi;
	}
}

// [=]===^=[ mkgui_icons_load ]========================================[=]
MKGUI_API void mkgui_icons_load(const char *path, const char *toolbar_path) {
	mkgui_icons_load_both(path, toolbar_path);
}

// [=]===^=[ mkgui_icons_search ]======================================[=]
MKGUI_API void mkgui_icons_search(const char *app_name) {
	if(!app_name || !app_name[0]) {
		return;
	}
	char path[512];
	char tb_path[512];

	const char *search_dirs[] = {
		".",
#ifndef _WIN32
		"/usr/share",
		"/usr/local/share",
#endif
		NULL
	};

	for(uint32_t i = 0; search_dirs[i]; ++i) {
		if(strcmp(search_dirs[i], ".") == 0) {
			snprintf(path, sizeof(path), "%s_icons.dat", app_name);
			snprintf(tb_path, sizeof(tb_path), "%s_icons_toolbar.dat", app_name);
		} else {
			snprintf(path, sizeof(path), "%s/%s/%s_icons.dat", search_dirs[i], app_name, app_name);
			snprintf(tb_path, sizeof(tb_path), "%s/%s/%s_icons_toolbar.dat", search_dirs[i], app_name, app_name);
		}
		if(!mdi.dat) {
			mdi_pack_load(&mdi, path);
		}
		if(!mdi_toolbar.dat) {
			mdi_pack_load(&mdi_toolbar, tb_path);
		}
		if(mdi.dat) {
			break;
		}
	}

#ifndef _WIN32
	const char *xdg = getenv("XDG_DATA_DIRS");
	if(!mdi.dat && xdg) {
		char xdg_buf[1024];
		snprintf(xdg_buf, sizeof(xdg_buf), "%s", xdg);
		char *tok = strtok(xdg_buf, ":");
		while(tok && !mdi.dat) {
			snprintf(path, sizeof(path), "%s/%s/%s_icons.dat", tok, app_name, app_name);
			snprintf(tb_path, sizeof(tb_path), "%s/%s/%s_icons_toolbar.dat", tok, app_name, app_name);
			if(!mdi.dat) {
				mdi_pack_load(&mdi, path);
			}
			if(!mdi_toolbar.dat) {
				mdi_pack_load(&mdi_toolbar, tb_path);
			}
			tok = strtok(NULL, ":");
		}
	}
#endif

	if(mdi.dat) {
		fprintf(stderr, "mkgui: icon pack loaded (%u icons, %ux%u)\n", mdi.icon_count, mdi.icon_size, mdi.icon_size);
	}
	if(mdi_toolbar.dat && mdi_toolbar.dat != mdi.dat) {
		fprintf(stderr, "mkgui: toolbar icon pack loaded (%u icons, %ux%u)\n", mdi_toolbar.icon_count, mdi_toolbar.icon_size, mdi_toolbar.icon_size);
	}
	if(mdi.dat && !mdi_toolbar.dat) {
		mdi_toolbar = mdi;
	}
}

// [=]===^=[ mkgui_icon_init ]========================================[=]
static void mkgui_icon_init(void) {
	icon_count = 0;
	icon_pixels_used = 0;
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

// [=]===^=[ icon_reload_all ]========================================[=]
static void icon_reload_all(void) {
	for(uint32_t i = 0; i < icon_count; ++i) {
		if(icons[i].custom) {
			continue;
		}
		uint32_t is_toolbar = (strncmp(icons[i].name, MKGUI_TOOLBAR_ICON_PREFIX, MKGUI_TOOLBAR_ICON_PREFIX_LEN) == 0);
		struct mdi_pack *pack = is_toolbar ? &mdi_toolbar : &mdi;
		const char *lookup_name = is_toolbar ? icons[i].name + MKGUI_TOOLBAR_ICON_PREFIX_LEN : icons[i].name;
		int32_t mi = mdi_pack_lookup(pack, lookup_name);
		if(mi >= 0) {
			mdi_pack_tint(pack, mi, icon_text_color, icons[i].pixels, (uint32_t)icons[i].w);
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
	strncpy(ic->name, name, MKGUI_ICON_NAME_LEN - 1);
	ic->name[MKGUI_ICON_NAME_LEN - 1] = '\0';
	ic->pixels = dst;
	ic->w = w;
	ic->h = h;
	ic->custom = 1;
	icon_hash_insert((uint32_t)idx);

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
	strncpy(w->icon, icon_name, MKGUI_ICON_NAME_LEN - 1);
	w->icon[MKGUI_ICON_NAME_LEN - 1] = '\0';
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

	if(!svg_rasterizer) {
		svg_rasterizer = nsvgCreateRasterizer();
		if(!svg_rasterizer) {
			return -1;
		}
	}

	uint32_t svg_len = 0;
	char *svg_data = svg_read_file(path, &svg_len);
	if(!svg_data) {
		return -1;
	}

	int32_t target_size = ctx->icon_size;
	uint32_t theme_color = ctx->theme.text & 0x00ffffff;
	int32_t idx = svg_rasterize_icon(name, svg_data, svg_len, target_size, theme_color);

	if(idx >= 0 && svg_source_count < MKGUI_SVG_ICON_MAX) {
		struct mkgui_svg_source *src = &svg_sources[svg_source_count++];
		strncpy(src->name, name, MKGUI_ICON_NAME_LEN - 1);
		src->name[MKGUI_ICON_NAME_LEN - 1] = '\0';
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

	if(!svg_rasterizer) {
		svg_rasterizer = nsvgCreateRasterizer();
		if(!svg_rasterizer) {
			return 0;
		}
	}

	DIR *d = opendir(dir_path);
	if(!d) {
		return 0;
	}

	uint32_t loaded = 0;
	struct dirent *ent;
	while((ent = readdir(d)) != NULL) {
		const char *fname = ent->d_name;
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
		int32_t idx = svg_rasterize_icon(name, svg_data, svg_len, target_size, theme_color);
		if(idx >= 0) {
			++loaded;
			if(svg_source_count < MKGUI_SVG_ICON_MAX) {
				struct mkgui_svg_source *src = &svg_sources[svg_source_count++];
				strncpy(src->name, name, MKGUI_ICON_NAME_LEN - 1);
				src->name[MKGUI_ICON_NAME_LEN - 1] = '\0';
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
	return loaded;
}

// [=]===^=[ svg_rerasterize_all ]====================================[=]
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
static void svg_rerasterize_all(struct mkgui_ctx *ctx) {
	if(!svg_rasterizer || svg_source_count == 0) {
		return;
	}
	int32_t target_size = ctx->icon_size;
	int32_t tb_size = ctx->toolbar_icon_size;
	uint32_t theme_color = ctx->theme.text & 0x00ffffff;
	for(uint32_t i = 0; i < svg_source_count; ++i) {
		svg_rasterize_icon(svg_sources[i].name, svg_sources[i].svg_data, svg_sources[i].svg_len, target_size, theme_color);
		char tb_name[MKGUI_ICON_NAME_LEN + MKGUI_TOOLBAR_ICON_PREFIX_LEN];
		snprintf(tb_name, sizeof(tb_name), "%s%s", MKGUI_TOOLBAR_ICON_PREFIX, svg_sources[i].name);
		if(icon_find_idx(tb_name) >= 0) {
			svg_rasterize_icon(tb_name, svg_sources[i].svg_data, svg_sources[i].svg_len, tb_size, theme_color);
		}
	}
}
#pragma GCC diagnostic pop

// [=]===^=[ svg_cleanup ]============================================[=]
static void svg_cleanup(void) {
	for(uint32_t i = 0; i < svg_source_count; ++i) {
		free(svg_sources[i].svg_data);
		svg_sources[i].svg_data = NULL;
	}
	svg_source_count = 0;
	if(svg_rasterizer) {
		nsvgDeleteRasterizer(svg_rasterizer);
		svg_rasterizer = NULL;
	}
}
