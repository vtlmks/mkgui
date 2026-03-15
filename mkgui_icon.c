// Copyright (c) 2026, mkgui contributors
// SPDX-License-Identifier: MIT

#include "incbin.h"
#include "ext/mdi_icons.h"

INCBIN(mdi_font, "ext/materialdesignicons-webfont.ttf");

// [=]===^=[ mdi_lookup ]===========================================[=]
static int32_t mdi_lookup(const char *name) {
	int32_t lo = 0;
	int32_t hi = MDI_ICON_COUNT - 1;
	while(lo <= hi) {
		int32_t mid = (lo + hi) / 2;
		int32_t cmp = strcmp(name, mdi_icons[mid].name);
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

// [=]===^=[ icon_load_one ]========================================[=]
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
	if(!platform_icon_render_glyph(mdi_icons[mi].codepoint, icon_text_color, dst, MKGUI_ICON_SIZE)) {
		return -1;
	}
	icon_pixels_used += pixel_count;

	int32_t idx = (int32_t)icon_count;
	struct mkgui_icon *ic = &icons[icon_count++];
	strncpy(ic->name, name, MKGUI_ICON_NAME_LEN - 1);
	ic->name[MKGUI_ICON_NAME_LEN - 1] = '\0';
	ic->pixels = dst;
	ic->w = MKGUI_ICON_SIZE;
	ic->h = MKGUI_ICON_SIZE;

	return idx;
}

// [=]===^=[ icon_find_idx ]========================================[=]
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

// [=]===^=[ icon_resolve ]=========================================[=]
static int32_t icon_resolve(const char *name) {
	int32_t idx = icon_find_idx(name);
	if(idx >= 0) {
		return idx;
	}
	return icon_load_one(name);
}

// [=]===^=[ widget_icon_idx ]======================================[=]
static int32_t widget_icon_idx(struct mkgui_widget *w) {
	if(w->icon[0] == '\0') {
		return -1;
	}
	return icon_resolve(w->icon);
}

// [=]===^=[ mkgui_icon_init ]======================================[=]
static void mkgui_icon_init(void) {
	icon_count = 0;
	icon_pixels_used = 0;

	size_t font_size = INCBIN_SIZE(mdi_font);
	if(!platform_icon_font_init(mdi_font_data, font_size, MKGUI_ICON_SIZE)) {
		return;
	}
	fprintf(stderr, "mkgui: MDI font loaded (%u icons available)\n", MDI_ICON_COUNT);
}

// [=]===^=[ icon_load_from_widgets ]===============================[=]
static void icon_load_from_widgets(struct mkgui_ctx *ctx) {
	for(uint32_t i = 0; i < ctx->widget_count; ++i) {
		if(ctx->widgets[i].icon[0] != '\0') {
			icon_resolve(ctx->widgets[i].icon);
		}
	}
}

// [=]===^=[ icon_reload_all ]=======================================[=]
static void icon_reload_all(void) {
	for(uint32_t i = 0; i < icon_count; ++i) {
		int32_t mi = mdi_lookup(icons[i].name);
		if(mi >= 0) {
			platform_icon_render_glyph(mdi_icons[mi].codepoint, icon_text_color, icons[i].pixels, MKGUI_ICON_SIZE);
		}
	}
}

// [=]===^=[ mkgui_set_icon ]=======================================[=]
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

// [=]===^=[ mkgui_set_treenode_icon ]==============================[=]
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
