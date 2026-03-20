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

// [=]===^=[ mdi_dat_free ]===========================================[=]
static void mdi_dat_free(void) {
	for(uint32_t i = 0; i < icon_count; ++i) {
		if(icons[i].custom) {
			free(icons[i].pixels);
		}
	}
	if(mdi_toolbar.dat && mdi_toolbar.dat != mdi.dat) {
		free(mdi_toolbar.dat);
	}
	if(mdi.dat) {
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
			dst[i] = ((uint32_t)a << 24) | ((uint32_t)cr << 16) | ((uint32_t)cg << 8) | cb;
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

// [=]===^=[ icon_resolve ]===========================================[=]
static int32_t icon_resolve(const char *name) {
	int32_t idx = icon_find_idx(name);
	if(idx >= 0) {
		return idx;
	}
	return icon_load_from_pack(&mdi, name, name);
}

// [=]===^=[ toolbar_icon_resolve ]====================================[=]
static int32_t toolbar_icon_resolve(const char *name) {
	char tb_name[MKGUI_ICON_NAME_LEN + MKGUI_TOOLBAR_ICON_PREFIX_LEN];
	snprintf(tb_name, sizeof(tb_name), "%s%s", MKGUI_TOOLBAR_ICON_PREFIX, name);
	int32_t idx = icon_find_idx(tb_name);
	if(idx >= 0) {
		return idx;
	}
	return icon_load_from_pack(&mdi_toolbar, name, tb_name);
}

// [=]===^=[ widget_icon_idx ]========================================[=]
static int32_t widget_icon_idx(struct mkgui_widget *w) {
	if(w->icon[0] == '\0') {
		return -1;
	}
	return icon_resolve(w->icon);
}

// [=]===^=[ toolbar_icon_idx ]========================================[=]
static int32_t toolbar_icon_idx(struct mkgui_widget *w) {
	if(w->icon[0] == '\0') {
		return -1;
	}
	return toolbar_icon_resolve(w->icon);
}

// [=]===^=[ mkgui_icon_init ]========================================[=]
static void mkgui_icon_init(void) {
	icon_count = 0;
	icon_pixels_used = 0;
	icon_hash_clear();

	const char *paths[] = {
		"mdi_icons.dat",
		"ext/mdi_icons.dat",
		NULL
	};

	for(uint32_t i = 0; paths[i]; ++i) {
		if(mdi_pack_load(&mdi, paths[i])) {
			fprintf(stderr, "mkgui: icon pack loaded from %s (%u icons, %ux%u)\n", paths[i], mdi.icon_count, mdi.icon_size, mdi.icon_size);
			break;
		}
	}
	if(!mdi.dat) {
		fprintf(stderr, "mkgui: warning: mdi_icons.dat not found, icons unavailable\n");
	}

	const char *tb_paths[] = {
		"mdi_icons_toolbar.dat",
		"ext/mdi_icons_toolbar.dat",
		NULL
	};

	uint32_t tb_loaded = 0;
	for(uint32_t i = 0; tb_paths[i]; ++i) {
		if(mdi_pack_load(&mdi_toolbar, tb_paths[i])) {
			fprintf(stderr, "mkgui: toolbar icon pack loaded from %s (%u icons, %ux%u)\n", tb_paths[i], mdi_toolbar.icon_count, mdi_toolbar.icon_size, mdi_toolbar.icon_size);
			tb_loaded = 1;
			break;
		}
	}
	if(!tb_loaded) {
		mdi_toolbar = mdi;
	}
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
	icon_hash_insert((uint32_t)idx);

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
