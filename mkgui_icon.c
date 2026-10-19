// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

struct mkgui_svg_source {
	char name[MKGUI_ICON_NAME_LEN];
	char *svg_data;
	uint32_t svg_len;
};

static struct vm_arena svg_source_arena;
static struct mkgui_svg_source *svg_sources;
static uint32_t svg_source_count;
static struct mkgui_window *icon_lazy_ctx;

// [=]===^=[ icon_find_idx_at ]=======================================[=]
static int32_t icon_find_idx_at(const char *name, int32_t size) {
	if(name[0] == '\0') {
		return -1;

	}

	return icon_hash_lookup_at(name, size);

}

// [=]===^=[ icon_find_idx_any ]======================================[=]
static int32_t icon_find_idx_any(const char *name) {
	if(name[0] == '\0') {
		return -1;

	}

	return icon_hash_lookup_any(name);

}

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

// [=]===^=[ svg_render_pixels ]==================================[=]
static uint32_t *svg_render_pixels(const char *data, uint32_t length, int32_t size, uint32_t color) {
	if(size <= 0 || (uint64_t)(uint32_t)size * (uint32_t)size > UINT32_MAX) {
		return NULL;

	}

	plutosvg_document_t *document = plutosvg_document_load_from_data(data, (int32_t)length, -1, -1, NULL, NULL);

	if(document) {
		plutovg_color_t current;
		current.r = (float)((color >> 16) & 0xff) / 255.0f;
		current.g = (float)((color >> 8) & 0xff) / 255.0f;
		current.b = (float)(color & 0xff) / 255.0f;
		current.a = 1.0f;
		plutovg_surface_t *surface = plutosvg_document_render_to_surface(document, NULL, size, size, &current, NULL, NULL);

		if(surface) {
			uint32_t *pixels = malloc((size_t)size * (size_t)size * sizeof(uint32_t));

			if(pixels) {
				uint8_t *source = plutovg_surface_get_data(surface);
				int32_t stride = plutovg_surface_get_stride(surface);

				for(int32_t row = 0; row < size; ++row) {
					memcpy(pixels + row * size, source + row * stride, (size_t)size * sizeof(uint32_t));

				}

				plutovg_surface_destroy(surface);
				plutosvg_document_destroy(document);
				return pixels;

			}

			plutovg_surface_destroy(surface);

		}

		plutosvg_document_destroy(document);

	}

	return NULL;

}

// [=]===^=[ icon_library_pixels ]================================[=]
static uint32_t *icon_library_pixels(const char *name, int32_t size, uint32_t color) {
	uint32_t length;
	const char *data = mkguiicons_svg(name, (uint32_t)size, &length);
	return data ? svg_render_pixels(data, length, size, color) : NULL;

}

// [=]===^=[ svg_rasterize_icon_ex ]=============================================================[=]
static int32_t svg_rasterize_icon_ex(const char *name, char *svg_data, uint32_t svg_len, int32_t target_size, uint32_t theme_text_color, uint32_t source_idx) {
	if(!svg_data || target_size <= 0) {
		return -1;

	}

	char *style = strstr(svg_data, "<style");

	while(style) {
		char *end = strstr(style, "</style>");

		if(!end) {
			break;

		}

		memset(style, ' ', (size_t)(end + 8 - style));
		style = strstr(end + 8, "<style");

	}

	uint32_t *pixels = svg_render_pixels(svg_data, svg_len, target_size, theme_text_color);
	return pixels ? icon_store(name, target_size, source_idx, pixels, target_size, target_size, 0) : -1;

}

// [=]===^=[ icon_library_rasterize ]============================================================[=]
static int32_t icon_library_rasterize(const char *name, const char *cache_name, int32_t size, uint32_t color) {
	uint32_t *pixels = icon_library_pixels(name, size, color);
	return pixels ? icon_store(cache_name, size, UINT32_MAX, pixels, size, size, 1) : -1;

}

// [=]===^=[ svg_find_source_idx ]====================================[=]
static uint32_t svg_find_source_idx(const char *name) {
	for(uint32_t i = 0; i < svg_source_count; ++i) {
		if(strcmp(svg_sources[i].name, name) == 0) {
			return i;

		}
	}

	return UINT32_MAX;

}

// [=]===^=[ icon_resolve_at ]=====================================[=]
static int32_t icon_resolve_at(struct mkgui_window *win, const char *name, int32_t size) {
	int32_t index = icon_find_idx_at(name, size);

	if(index >= 0) {
		return index;

	}

	struct mkgui_window *effective = win ? win : icon_lazy_ctx;

	if(!effective || size <= 0) {
		return -1;

	}

	uint32_t source = svg_find_source_idx(name);
	uint32_t color = effective->theme.text & 0x00ffffff;

	if(source < svg_source_count) {
		struct mkgui_svg_source *svg = &svg_sources[source];
		index = svg_rasterize_icon_ex(name, svg->svg_data, svg->svg_len, size, color, source);

	} else {
		index = icon_library_rasterize(name, name, size, color);

	}


	return index;

}

// [=]===^=[ icon_resolve ]===========================================[=]
static int32_t icon_resolve(struct mkgui_window *win, const char *name) {
	struct mkgui_window *effective = win;

	if(!effective) {
		effective = icon_lazy_ctx;

	}

	int32_t size = effective ? effective->icon_size : 0;
	int32_t idx = icon_resolve_at(effective, name, size);

	if(idx < 0) {
		idx = icon_find_idx_any(name);

	}

	return idx;

}

// [=]===^=[ widget_icon_idx ]========================================[=]
static int32_t widget_icon_idx(struct mkgui_window *win, struct mkgui_widget *w) {
	if(w->icon[0] == '\0') {
		return -1;

	}

	int32_t idx = icon_resolve(win, w->icon);

	if(idx < 0) {
		return icon_find_idx_any("_missing");

	}

	return idx;

}

// [=]===^=[ widget_icon_idx_at ]=====================================[=]
static int32_t widget_icon_idx_at(struct mkgui_window *win, struct mkgui_widget *w, int32_t size) {
	if(w->icon[0] == '\0') {
		return -1;

	}

	int32_t idx = icon_resolve_at(win, w->icon, size);

	if(idx < 0) {
		return icon_find_idx_any("_missing");

	}

	return idx;

}

// [=]===^=[ mkgui_icon_at_size ]=====================================[=]
MKGUI_API int32_t mkgui_icon_at_size(struct mkgui_window *win, const char *name, int32_t size) {
	MKGUI_CHECK_VAL(win, -1);

	if(!name || !name[0] || size <= 0) {
		return -1;

	}

	return icon_resolve_at(win, name, size);

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
	if(icon_find_idx_at("_missing", 0) < 0) {
		icon_generate_missing();

	}

}

// [=]===^=[ icon_load_from_widgets ]=================================[=]
static void icon_load_from_widgets(struct mkgui_window *win) {
	icon_lazy_ctx = win;

	for(uint32_t i = 0; i < win->widget_count; ++i) {
		if(win->widgets[i].icon[0] != '\0') {
			icon_resolve(win, win->widgets[i].icon);

		}
	}
}

// [=]===^=[ mkgui_icon_add ]====================================================================[=]
MKGUI_API int32_t mkgui_icon_add(const char *name, const uint32_t *pixels, int32_t w, int32_t h) {
	if(!name || !name[0] || strlen(name) >= MKGUI_ICON_NAME_LEN || !pixels || w <= 0 || h <= 0) {
		return -1;

	}

	uint64_t count = (uint64_t)(uint32_t)w * (uint32_t)h;

	if(count > UINT32_MAX || count > SIZE_MAX / sizeof(uint32_t)) {
		return -1;

	}

	uint32_t *copy = malloc((size_t)count * sizeof(*copy));

	if(copy) {
		memcpy(copy, pixels, (size_t)count * sizeof(*copy));
		return icon_store(name, 0, UINT32_MAX, copy, w, h, 0);

	}

	return -1;

}

// [=]===^=[ mkgui_set_icon ]=========================================[=]
MKGUI_API void mkgui_set_icon(struct mkgui_window *win, uint32_t widget_id, const char *icon_name) {
	MKGUI_CHECK(win);
	struct mkgui_widget *w = find_widget(win, widget_id);

	if(!w) {
		return;

	}

	if(!icon_name) {
		w->icon[0] = '\0';
		dirty_all(win);
		return;

	}

	snprintf(w->icon, MKGUI_ICON_NAME_LEN, "%s", icon_name);
	icon_resolve(win, icon_name);
	dirty_all(win);

}

// [=]===^=[ mkgui_set_treenode_icon ]================================[=]
MKGUI_API void mkgui_set_treenode_icon(struct mkgui_window *win, uint32_t widget_id, uint32_t node_id, const char *icon_name) {
	MKGUI_CHECK(win);
	struct mkgui_treeview_data *tv = find_treeview_data(win, widget_id);

	if(!tv) {
		return;

	}

	int32_t idx = icon_resolve(win, icon_name);

	for(uint32_t i = 0; i < tv->node_count; ++i) {
		if(tv->nodes[i].id == node_id) {
			tv->nodes[i].icon_idx = idx;
			dirty_all(win);
			return;

		}
	}
}

// [=]===^=[ mkgui_icon_load_svg ]===============================================================[=]
MKGUI_API int32_t mkgui_icon_load_svg(struct mkgui_window *win, const char *name, const char *path) {
	MKGUI_CHECK_VAL(win, -1);

	if(!name || !name[0] || strlen(name) >= MKGUI_ICON_NAME_LEN || !path) {
		return -1;

	}

	uint32_t length = 0;
	char *data = svg_read_file(path, &length);

	if(data) {
		uint32_t source = svg_find_source_idx(name);

		if(source == UINT32_MAX) {
			if(svg_source_count == UINT32_MAX || (uint64_t)(svg_source_count + 1) * sizeof(*svg_sources) > SIZE_MAX || !vm_arena_grow(&svg_source_arena, (size_t)(svg_source_count + 1) * sizeof(*svg_sources))) {
				free(data);
				return -1;

			}

			svg_sources = (struct mkgui_svg_source *)svg_source_arena.base;
			source = svg_source_count;

		}

		int32_t index = svg_rasterize_icon_ex(name, data, length, win->icon_size, win->theme.text & 0x00ffffff, source);

		if(index >= 0) {
			struct mkgui_svg_source *entry = &svg_sources[source];
			free(entry->svg_data);
			snprintf(entry->name, sizeof(entry->name), "%s", name);
			entry->svg_data = data;
			entry->svg_len = length;

			if(source == svg_source_count) {
				++svg_source_count;

			}

			return index;

		}

		free(data);

	}

	return -1;

}

// [=]===^=[ svg_rerasterize_all ]===============================================================[=]
static void svg_rerasterize_all(struct mkgui_window *win, float size_ratio) {
	if(!icon_count) {
		return;

	}

	struct vm_arena staging = {0};

	if(vm_arena_grow(&staging, (size_t)icon_count * sizeof(*icons))) {
		struct mkgui_icon *updated = (struct mkgui_icon *)staging.base;
		uint32_t color = win->theme.text & 0x00ffffff;
		memcpy(updated, icons, (size_t)icon_count * sizeof(*icons));

		for(uint32_t i = 0; i < icon_count; ++i) {
			struct mkgui_icon *icon = &updated[i];

			if(icon->requested_size <= 0 || strncmp(icon->name, "dlg:", 4) == 0) {
				continue;

			}

			int32_t size = (int32_t)((float)icon->requested_size * size_ratio + 0.5f);
			size = size > 0 ? size : 1;
			uint32_t *pixels = NULL;

			if(icon->library) {
				pixels = icon_library_pixels(icon->name, size, color);

			} else if(icon->source_idx < svg_source_count) {
				struct mkgui_svg_source *source = &svg_sources[icon->source_idx];
				pixels = svg_render_pixels(source->svg_data, source->svg_len, size, color);

			}

			if(pixels) {
				icon->pixels = pixels;
				icon->w = size;
				icon->h = size;
				icon->requested_size = size;

			}
		}

		if(icon_atlas_rebuild(updated, icon_count)) {
			memcpy(icons, updated, (size_t)icon_count * sizeof(*icons));
			icon_hash_clear();

			for(uint32_t i = 0; i < icon_count; ++i) {
				icon_hash_insert(i);

			}

		} else {
			for(uint32_t i = 0; i < icon_count; ++i) {
				free(updated[i].pixels);

			}
		}

		vm_arena_destroy(&staging);

	}
}

// [=]===^=[ svg_cleanup ]======================================================================[=]
static void svg_cleanup(void) {
	for(uint32_t i = 0; i < svg_source_count; ++i) {
		free(svg_sources[i].svg_data);

	}

	for(uint32_t i = 0; i < icon_count; ++i) {
		free(icons[i].pixels);

	}

	free(icon_atlas);
	vm_arena_destroy(&svg_source_arena);
	vm_arena_destroy(&icon_hash_arena);
	vm_arena_destroy(&icon_arena);
	icon_atlas = NULL;
	icons = NULL;
	icon_hash = NULL;
	svg_sources = NULL;
	icon_count = 0;
	icon_hash_size = 0;
	svg_source_count = 0;
	icon_lazy_ctx = NULL;

}
