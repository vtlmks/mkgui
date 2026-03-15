// Copyright (c) 2026, mkgui contributors
// SPDX-License-Identifier: MIT

// [=]===^=[ mdi_dat_load ]===========================================[=]
static uint32_t mdi_dat_load(const char *path) {
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

	mdi_dat = (uint8_t *)malloc((size_t)sz);
	if(!mdi_dat) {
		fclose(fp);
		return 0;
	}
	if(fread(mdi_dat, 1, (size_t)sz, fp) != (size_t)sz) {
		free(mdi_dat);
		mdi_dat = NULL;
		fclose(fp);
		return 0;
	}
	fclose(fp);
	mdi_dat_size = (uint32_t)sz;

	if(memcmp(mdi_dat, "MKIC", 4) != 0) {
		free(mdi_dat);
		mdi_dat = NULL;
		return 0;
	}

	memcpy(&mdi_icon_size, mdi_dat + 4, 2);
	memcpy(&mdi_icon_count, mdi_dat + 6, 2);

	mdi_name_block = (const char *)(mdi_dat + 8);

	uint32_t name_block_size = 0;
	for(uint16_t i = 0; i < mdi_icon_count; ++i) {
		name_block_size += (uint32_t)strlen(mdi_name_block + name_block_size) + 1;
	}
	uint32_t name_block_padded = (name_block_size + 3) & ~3u;

	mdi_name_offsets = (const uint32_t *)(mdi_dat + 8 + name_block_padded);
	mdi_pixel_data = (const uint8_t *)(mdi_dat + 8 + name_block_padded + (uint32_t)mdi_icon_count * 4);

	return 1;
}

// [=]===^=[ mdi_dat_free ]===========================================[=]
static void mdi_dat_free(void) {
	for(uint32_t i = 0; i < icon_count; ++i) {
		if(icons[i].custom) {
			free(icons[i].pixels);
		}
	}
	if(mdi_dat) {
		free(mdi_dat);
		mdi_dat = NULL;
	}
	mdi_icon_count = 0;
}

// [=]===^=[ mdi_lookup ]=============================================[=]
static int32_t mdi_lookup(const char *name) {
	if(!mdi_dat || mdi_icon_count == 0) {
		return -1;
	}
	int32_t lo = 0;
	int32_t hi = (int32_t)mdi_icon_count - 1;
	while(lo <= hi) {
		int32_t mid = (lo + hi) / 2;
		const char *mid_name = mdi_name_block + mdi_name_offsets[mid];
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

// [=]===^=[ mdi_tint_icon ]==========================================[=]
static void mdi_tint_icon(int32_t mdi_idx, uint32_t color, uint32_t *dst, uint32_t size) {
	uint32_t pixel_count = size * size;
	const uint8_t *src = mdi_pixel_data + (uint32_t)mdi_idx * pixel_count;
	uint8_t cr = (color >> 16) & 0xff;
	uint8_t cg = (color >> 8) & 0xff;
	uint8_t cb = color & 0xff;
	for(uint32_t i = 0; i < pixel_count; ++i) {
		uint8_t a = src[i];
		if(a > 0) {
			dst[i] = ((uint32_t)a << 24) | ((uint32_t)cr << 16) | ((uint32_t)cg << 8) | cb;
		} else {
			dst[i] = 0;
		}
	}
}

// [=]===^=[ icon_load_one ]==========================================[=]
static int32_t icon_load_one(const char *name) {
	if(icon_count >= MKGUI_MAX_ICONS) {
		return -1;
	}
	if(name[0] == '\0') {
		return -1;
	}

	int32_t mi = mdi_lookup(name);
	if(mi < 0) {
		return -1;
	}

	uint32_t pixel_count = MKGUI_ICON_PIXELS;
	if(icon_pixels_used + pixel_count > MKGUI_ICON_PIXEL_POOL) {
		return -1;
	}

	uint32_t *dst = &icon_pixels[icon_pixels_used];
	mdi_tint_icon(mi, icon_text_color, dst, MKGUI_ICON_SIZE);
	icon_pixels_used += pixel_count;

	int32_t idx = (int32_t)icon_count;
	struct mkgui_icon *ic = &icons[icon_count++];
	strncpy(ic->name, name, MKGUI_ICON_NAME_LEN - 1);
	ic->name[MKGUI_ICON_NAME_LEN - 1] = '\0';
	ic->pixels = dst;
	ic->w = MKGUI_ICON_SIZE;
	ic->h = MKGUI_ICON_SIZE;
	ic->custom = 0;

	return idx;
}

// [=]===^=[ icon_find_idx ]==========================================[=]
static int32_t icon_find_idx(const char *name) {
	if(name[0] == '\0') {
		return -1;
	}
	for(uint32_t i = 0; i < icon_count; ++i) {
		if(strcmp(icons[i].name, name) == 0) {
			return (int32_t)i;
		}
	}
	return -1;
}

// [=]===^=[ icon_resolve ]===========================================[=]
static int32_t icon_resolve(const char *name) {
	int32_t idx = icon_find_idx(name);
	if(idx >= 0) {
		return idx;
	}
	return icon_load_one(name);
}

// [=]===^=[ widget_icon_idx ]========================================[=]
static int32_t widget_icon_idx(struct mkgui_widget *w) {
	if(w->icon[0] == '\0') {
		return -1;
	}
	return icon_resolve(w->icon);
}

// [=]===^=[ mkgui_icon_init ]========================================[=]
static void mkgui_icon_init(void) {
	icon_count = 0;
	icon_pixels_used = 0;

	const char *paths[] = {
		"mdi_icons.dat",
		"ext/mdi_icons.dat",
		NULL
	};

	for(uint32_t i = 0; paths[i]; ++i) {
		if(mdi_dat_load(paths[i])) {
			fprintf(stderr, "mkgui: icon pack loaded from %s (%u icons, %ux%u)\n", paths[i], mdi_icon_count, mdi_icon_size, mdi_icon_size);
			return;
		}
	}
	fprintf(stderr, "mkgui: warning: mdi_icons.dat not found, icons unavailable\n");
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
		int32_t mi = mdi_lookup(icons[i].name);
		if(mi >= 0) {
			mdi_tint_icon(mi, icon_text_color, icons[i].pixels, MKGUI_ICON_SIZE);
		}
	}
}

// [=]===^=[ mkgui_icon_add ]=========================================[=]
static int32_t mkgui_icon_add(const char *name, const uint32_t *pixels, int32_t w, int32_t h) {
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

	return idx;
}

// [=]===^=[ mkgui_set_icon ]=========================================[=]
static void mkgui_set_icon(struct mkgui_ctx *ctx, uint32_t widget_id, const char *icon_name) {
	struct mkgui_widget *w = find_widget(ctx, widget_id);
	if(!w) {
		return;
	}
	strncpy(w->icon, icon_name, MKGUI_ICON_NAME_LEN - 1);
	w->icon[MKGUI_ICON_NAME_LEN - 1] = '\0';
	icon_resolve(icon_name);
	dirty_all(ctx);
}

// [=]===^=[ mkgui_set_treenode_icon ]================================[=]
static void mkgui_set_treenode_icon(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id, const char *icon_name) {
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
