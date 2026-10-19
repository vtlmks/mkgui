// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

#if defined(__SSE2__)
#include <emmintrin.h>
#endif
#if defined(__SSE4_1__)
#include <smmintrin.h>
#endif
#include <immintrin.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifndef MKGUI_LIBRARY
#define MKGUI_API static
#endif

// ---------------------------------------------------------------------------
// Public API (single source of truth for all public types, constants, enums)
// ---------------------------------------------------------------------------

#include "mkgui_features.h"
#include "mkgui.h"
#include "mkguiicons.h"
#include "mkguiicons_names.h"
#include "mkgui_internal.h"

// ---------------------------------------------------------------------------
// Platform layer (mkwin): window/input/blit, font rasterisation, glview.
// Enable MKWIN_FONT/MKWIN_GLVIEW here so consumers need not pass the flags.
// ---------------------------------------------------------------------------

#define MKWIN_FONT
#define MKWIN_GLVIEW

#include "mkwin.h"
#ifdef _WIN32
#include "mkwin_types_win32.h"
#else
#include "mkwin_types_linux.h"
#endif

// [=]===^=[ mkgui_sleep_ns ]======================================[=]
MKGUI_API void mkgui_sleep_ns(uint64_t ns) {
	mkwin_sleep_ns(ns);
}

// [=]===^=[ mkgui_now_ns ]========================================[=]
MKGUI_API uint64_t mkgui_now_ns(void) {
	return mkwin_now_ns();
}

static struct vm_arena glyph_arena;
static uint8_t *glyph_atlas;
static uint32_t *icon_atlas;
static struct vm_arena icon_arena;
static struct vm_arena icon_hash_arena;
static struct mkgui_icon *icons;
static uint32_t icon_count;
static uint32_t *icon_hash;
static size_t icon_hash_size;

// ---------------------------------------------------------------------------
// Virtual memory arena (reserve-commit, no realloc)
// ---------------------------------------------------------------------------

// Sized at 1M widgets: far past any real UI, still bounded so the per-win
// virtual reserve stays sane on 32-bit-virtual hosts.
#define MKGUI_VM_MAX_TEXT_CMDS   32768
#define MKGUI_VM_MAX_WIDGETS     (1u << 20)
#define MKGUI_VM_MAX_HASH_SLOTS  (MKGUI_VM_MAX_WIDGETS * 2)

#ifdef _WIN32

// [=]===^=[ vm_arena_create ]=====================================[=]
static uint32_t vm_arena_create(struct vm_arena *a, size_t reserve_bytes) {
	a->base = (uint8_t *)VirtualAlloc(NULL, reserve_bytes, MEM_RESERVE, PAGE_NOACCESS);
	if(!a->base) {
		return 0;
	}
	a->reserved = reserve_bytes;
	a->committed = 0;
	return 1;
}

// [=]===^=[ vm_arena_ensure ]=====================================[=]
static uint32_t vm_arena_ensure(struct vm_arena *a, size_t needed) {
	if(needed <= a->committed) {
		return 1;
	}

	if(needed > a->reserved) {
		return 0;
	}
	size_t page_size = 65536;
	size_t new_commit = (needed + page_size - 1) & ~(page_size - 1);
	if(new_commit > a->reserved) {
		new_commit = a->reserved;
	}

	if(!VirtualAlloc(a->base, new_commit, MEM_COMMIT, PAGE_READWRITE)) {
		return 0;
	}
	a->committed = new_commit;
	return 1;
}

// [=]===^=[ vm_arena_destroy ]====================================[=]
static void vm_arena_destroy(struct vm_arena *a) {
	if(a->base) {
		VirtualFree(a->base, 0, MEM_RELEASE);
		a->base = NULL;
		a->reserved = 0;
		a->committed = 0;
	}
}

#else

#include <sys/mman.h>

// [=]===^=[ vm_arena_create ]=====================================[=]
static uint32_t vm_arena_create(struct vm_arena *a, size_t reserve_bytes) {
	a->base = (uint8_t *)mmap(NULL, reserve_bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if(a->base == MAP_FAILED) {
		a->base = NULL;
		return 0;
	}
	a->reserved = reserve_bytes;
	a->committed = 0;
	return 1;
}

// [=]===^=[ vm_arena_ensure ]=====================================[=]
static uint32_t vm_arena_ensure(struct vm_arena *a, size_t needed) {
	if(needed <= a->committed) {
		return 1;
	}

	if(needed > a->reserved) {
		return 0;
	}
	size_t page_size = 4096;
	size_t new_commit = (needed + page_size - 1) & ~(page_size - 1);
	if(new_commit > a->reserved) {
		new_commit = a->reserved;
	}

	if(mprotect(a->base, new_commit, PROT_READ | PROT_WRITE) != 0) {
		return 0;
	}
	a->committed = new_commit;
	return 1;
}

// [=]===^=[ vm_arena_destroy ]====================================[=]
static void vm_arena_destroy(struct vm_arena *a) {
	if(a->base) {
		munmap(a->base, a->reserved);
		a->base = NULL;
		a->reserved = 0;
		a->committed = 0;
	}
}

#endif

// [=]===^=[ vm_arena_grow ]====================================================================[=]
static uint32_t vm_arena_grow(struct vm_arena *arena, size_t needed) {
	if(needed <= arena->reserved) {
		return vm_arena_ensure(arena, needed);

	}

	size_t reserved = arena->reserved ? arena->reserved : 65536;
	struct vm_arena replacement = {0};

	while(reserved < needed) {
		if(reserved > SIZE_MAX / 2) {
			reserved = needed;
			break;

		}

		reserved *= 2;

	}

	if(vm_arena_create(&replacement, reserved)) {
		if(vm_arena_ensure(&replacement, needed)) {
			if(arena->committed) {
				memcpy(replacement.base, arena->base, arena->committed);

			}

			vm_arena_destroy(arena);
			*arena = replacement;
			return 1;

		}

		vm_arena_destroy(&replacement);

	}

	return 0;

}

// [=]===^=[ icon_hash_fn ]=====================================================================[=]
static size_t icon_hash_fn(const char *name, int32_t size) {
	uint32_t hash = 2166136261u;

	for(size_t i = 0; name[i]; ++i) {
		hash ^= (uint32_t)(uint8_t)name[i];
		hash *= 16777619u;

	}

	hash ^= (uint32_t)size * 2654435761u;
	return (size_t)hash & (icon_hash_size - 1);

}

// [=]===^=[ icon_hash_clear ]==================================================================[=]
static void icon_hash_clear(void) {
	if(icon_hash_size) {
		memset(icon_hash, 0xff, icon_hash_size * sizeof(*icon_hash));

	}
}

// [=]===^=[ icon_hash_insert ]=================================================================[=]
static void icon_hash_insert(uint32_t index) {
	size_t slot = icon_hash_fn(icons[index].name, icons[index].requested_size);

	while(icon_hash[slot] != UINT32_MAX) {
		slot = (slot + 1) & (icon_hash_size - 1);

	}

	icon_hash[slot] = index;

}

// [=]===^=[ icon_hash_lookup_at ]==============================================================[=]
static int32_t icon_hash_lookup_at(const char *name, int32_t size) {
	if(!icon_hash_size) {
		return -1;

	}

	size_t slot = icon_hash_fn(name, size);

	for(;;) {
		uint32_t index = icon_hash[slot];

		if(index == UINT32_MAX) {
			return -1;

		}

		if(icons[index].requested_size == size && strcmp(icons[index].name, name) == 0) {
			return (int32_t)index;

		}

		slot = (slot + 1) & (icon_hash_size - 1);

	}
}

// [=]===^=[ icon_hash_lookup_any ]=============================================================[=]
static int32_t icon_hash_lookup_any(const char *name) {
	for(uint32_t i = 0; i < icon_count; ++i) {
		if(strcmp(icons[i].name, name) == 0) {
			return (int32_t)i;

		}
	}

	return -1;

}

// [=]===^=[ icon_storage_ensure ]==============================================================[=]
static uint32_t icon_storage_ensure(uint32_t count) {
	if(count > INT32_MAX || (uint64_t)count * sizeof(*icons) > SIZE_MAX) {
		return 0;

	}

	if(!vm_arena_grow(&icon_arena, (size_t)count * sizeof(*icons))) {
		return 0;

	}

	icons = (struct mkgui_icon *)icon_arena.base;
	size_t slots = icon_hash_size ? icon_hash_size : 128;

	while((size_t)count > slots / 2) {
		if(slots > SIZE_MAX / (2 * sizeof(*icon_hash))) {
			return 0;

		}

		slots *= 2;

	}

	if(slots != icon_hash_size) {
		if(!vm_arena_grow(&icon_hash_arena, slots * sizeof(*icon_hash))) {
			return 0;

		}

		icon_hash = (uint32_t *)icon_hash_arena.base;
		icon_hash_size = slots;
		icon_hash_clear();

		for(uint32_t i = 0; i < icon_count; ++i) {
			icon_hash_insert(i);

		}
	}

	return 1;

}

// ---------------------------------------------------------------------------
// Dynamic array growth
// ---------------------------------------------------------------------------

#define MKGUI_GROW_WIDGETS 256
#define MKGUI_GROW_AUX     16
#define STYLE_BORDER_BIT   (1u << 0)

// Grow the per-array arena by GROW_AUX. The array pointer never moves.
// Committed pages are kernel-zeroed, so the new tail needs no memset.
#define MKGUI_AUX_GROW(arena_ptr, count, cap, type) do { \
	if((count) >= (cap)) { \
		uint32_t _nc = (cap) + MKGUI_GROW_AUX; \
		if(vm_arena_ensure((arena_ptr), (size_t)_nc * sizeof(type))) { \
			(cap) = _nc; \
		} \
	} \
} while(0)

// ---------------------------------------------------------------------------
// Default theme
// ---------------------------------------------------------------------------

// [=]===^=[ default_theme ]=====================================[=]
MKGUI_API struct mkgui_theme default_theme(void) {
	struct mkgui_theme t;
	t.bg                  = 0xff232629;
	t.widget_bg           = 0xff31363b;
	t.widget_border       = 0xff5e5e5e;
	t.widget_hover        = 0xff3d4349;
	t.widget_press        = 0xff272b2f;
	t.text                = 0xffeff0f1;
	t.text_disabled       = 0xff6e7175;
	t.selection           = 0xff2980b9;
	t.sel_text            = 0xffffffff;
	t.input_bg            = 0xff1a1e21;
	t.tab_active          = 0xff31363b;
	t.tab_inactive        = 0xff2a2e32;
	t.tab_hover           = 0xff353a3f;
	t.menu_bg             = 0xff31363b;
	t.scrollbar_bg        = 0xff1d2023;
	t.scrollbar_thumb     = 0xff4d4d4d;
	t.scrollbar_thumb_hover = 0xff5a5a5a;
	t.highlight           = 0xff3daee9;
	t.header_bg           = 0xff2a2e32;
	t.listview_alt        = 0xff2a2e32;
	t.accent              = 0xff2a7ab5;
	t.corner_radius       = 3;
	return t;
}

// [=]===^=[ light_theme ]========================================[=]
MKGUI_API struct mkgui_theme light_theme(void) {
	struct mkgui_theme t;
	t.bg                    = 0xfff0f0f0;
	t.widget_bg             = 0xffffffff;
	t.widget_border         = 0xffbcbcbc;
	t.widget_hover          = 0xffe8e8e8;
	t.widget_press          = 0xffd0d0d0;
	t.text                  = 0xff1a1a1a;
	t.text_disabled         = 0xff999999;
	t.selection             = 0xff2980b9;
	t.sel_text              = 0xffffffff;
	t.input_bg              = 0xffffffff;
	t.tab_active            = 0xffffffff;
	t.tab_inactive          = 0xffe0e0e0;
	t.tab_hover             = 0xffebebeb;
	t.menu_bg               = 0xfffafafa;
	t.scrollbar_bg          = 0xffe0e0e0;
	t.scrollbar_thumb       = 0xffb0b0b0;
	t.scrollbar_thumb_hover = 0xff909090;
	t.highlight             = 0xff3daee9;
	t.header_bg             = 0xffe8e8e8;
	t.listview_alt          = 0xfff5f5f5;
	t.accent                = 0xff2a7ab5;
	t.corner_radius         = 3;
	return t;
}

// [=]===^=[ theme_prefers_dark ]=================================[=]
static uint32_t theme_prefers_dark(void) {
	char *override = getenv("MKGUI_THEME");
	if(override) {
		if(strcmp(override, "light") == 0) {
			return 0;
		}

		if(strcmp(override, "dark") == 0) {
			return 1;
		}
	}
	char *gtk = getenv("GTK_THEME");
	if(gtk && gtk[0]) {
		char lower[128];
		uint32_t i = 0;
		while(gtk[i] && i < sizeof(lower) - 1) {
			char c = gtk[i];
			if(c >= 'A' && c <= 'Z') {
				c = (char)(c + ('a' - 'A'));
			}
			lower[i] = c;
			++i;
		}
		lower[i] = 0;
		return strstr(lower, "dark") != NULL;
	}
#ifdef _WIN32
	DWORD value = 1;
	DWORD size = sizeof(value);
	LONG res = RegGetValueA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", "AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &value, &size);
	if(res == ERROR_SUCCESS) {
		return value == 0;
	}
#endif
	return 1;
}

// [=]===^=[ auto_theme ]=========================================[=]
static struct mkgui_theme auto_theme(void) {
	return theme_prefers_dark() ? default_theme() : light_theme();
}

// ---------------------------------------------------------------------------
// Public API validation macros
// ---------------------------------------------------------------------------

#define MKGUI_CHECK(win) do { if(!(win)) { return; } } while(0)
#define MKGUI_CHECK_VAL(win, val) do { if(!(win)) { return (val); } } while(0)

// ---------------------------------------------------------------------------
// Scale helpers
// ---------------------------------------------------------------------------

static inline int32_t sc(struct mkgui_window *win, int32_t px) {
	return (int32_t)(px * win->scale + 0.5f);
}

// [=]===^=[ mkgui_recompute_metrics ]==============================[=]
static void mkgui_recompute_metrics(struct mkgui_window *win) {
	win->row_height      = sc(win, MKGUI_ROW_HEIGHT);
	win->tab_height      = sc(win, MKGUI_TAB_HEIGHT);
	win->menu_height     = sc(win, MKGUI_MENU_HEIGHT);
	win->scrollbar_w     = sc(win, MKGUI_SCROLLBAR_W);
	win->split_thick     = sc(win, MKGUI_SPLIT_THICK);
	win->split_min_px    = sc(win, MKGUI_SPLIT_MIN_PX);
	win->box_gap         = sc(win, MKGUI_BOX_GAP);
	win->box_pad         = sc(win, MKGUI_BOX_PAD);
	win->icon_size           = sc(win, MKGUI_ICON_SIZE);
	win->dialog_icon_size    = sc(win, 32);
	win->pathbar_height  = sc(win, MKGUI_PATHBAR_HEIGHT);
	win->toolbar_height  = sc(win, MKGUI_TOOLBAR_HEIGHT_DEFAULT);
	win->toolbar_btn_w   = sc(win, MKGUI_TOOLBAR_BTN_W);
	win->toolbar_sep_w   = sc(win, MKGUI_TOOLBAR_SEP_W);
	win->statusbar_height = sc(win, MKGUI_STATUSBAR_HEIGHT);
	for(uint32_t i = 0; i < win->widget_count; ++i) {
		win->widgets[i].label_tw = -1;
	}
}

// [=]===^=[ icon_atlas_rebuild ]===============================================================[=]
static uint32_t icon_atlas_rebuild(struct mkgui_icon *entries, uint32_t count) {
	size_t total = 0;

	for(uint32_t i = 0; i < count; ++i) {
		size_t pixels = (size_t)entries[i].w * (size_t)entries[i].h;

		if(pixels > UINT32_MAX - total) {
			return 0;

		}

		total += pixels;

	}

	if(total > SIZE_MAX / sizeof(uint32_t)) {
		return 0;

	}

	uint32_t *replacement = calloc(total ? total : 1, sizeof(*replacement));

	if(replacement) {
		uint32_t offset = 0;

		for(uint32_t i = 0; i < count; ++i) {
			struct mkgui_icon *icon = &entries[i];
			uint32_t pixels = (uint32_t)((size_t)icon->w * (size_t)icon->h);

			if(icon->pixels) {
				memcpy(replacement + offset, icon->pixels, (size_t)pixels * sizeof(*replacement));
				free(icon->pixels);
				icon->pixels = NULL;

			} else if(pixels) {
				memcpy(replacement + offset, icon_atlas + icon->atlas_offset, (size_t)pixels * sizeof(*replacement));

			}

			icon->atlas_offset = offset;
			offset += pixels;

		}

		free(icon_atlas);
		icon_atlas = replacement;
		return 1;

	}

	return 0;

}

// [=]===^=[ icon_store ]=======================================================================[=]
static int32_t icon_store(const char *name, int32_t size, uint32_t source, uint32_t *pixels, int32_t width, int32_t height, uint32_t library) {
	int32_t existing = icon_hash_lookup_at(name, size);

	if(existing >= 0 || icon_storage_ensure(icon_count + 1)) {
		uint32_t index = existing >= 0 ? (uint32_t)existing : icon_count++;
		struct mkgui_icon previous = icons[index];
		struct mkgui_icon *icon = &icons[index];
		snprintf(icon->name, sizeof(icon->name), "%s", name);
		icon->requested_size = size;
		icon->source_idx = source;
		icon->pixels = pixels;
		icon->w = width;
		icon->h = height;
		icon->custom = 1;
		icon->library = library;

		if(icon_atlas_rebuild(icons, icon_count)) {
			if(existing < 0) {
				icon_hash_insert(index);

			}

			return (int32_t)index;

		}

		icons[index] = previous;

		if(existing < 0) {
			--icon_count;

		}
	}

	free(pixels);
	return -1;

}

#include "mkgui_render.c"

// ---------------------------------------------------------------------------
// Widget lookup helpers
// ---------------------------------------------------------------------------

// [=]===^=[ id_index_rebuild ]==================================[=]
// Rebuild the id->index hash from widgets[]. Keeps the FIRST widget for a
// duplicate id. id_index_cap >= 2 * widget_count, so probing terminates.
static void id_index_rebuild(struct mkgui_window *win) {
	uint32_t mask = win->id_index_cap - 1;
	for(uint32_t i = 0; i < win->id_index_cap; ++i) {
		win->id_index[i].idx = UINT32_MAX;
	}
	for(uint32_t i = 0; i < win->widget_count; ++i) {
		uint32_t id = win->widgets[i].id;
		uint32_t h = (id * 2654435761u) & mask;
		for(;;) {
			if(win->id_index[h].idx == UINT32_MAX) {
				win->id_index[h].id = id;
				win->id_index[h].idx = i;
				break;
			}

			if(win->id_index[h].id == id) {
				break;
			}
			h = (h + 1) & mask;
		}
	}
	win->id_index_dirty = 0;
}

// [=]===^=[ find_widget_idx ]===================================[=]
static int32_t find_widget_idx(struct mkgui_window *win, uint32_t id) {
	if(win->id_index_dirty) {
		id_index_rebuild(win);
	}
	uint32_t mask = win->id_index_cap - 1;
	uint32_t h = (id * 2654435761u) & mask;
	for(;;) {
		uint32_t idx = win->id_index[h].idx;
		if(idx == UINT32_MAX) {
			return -1;
		}

		if(win->id_index[h].id == id) {
			return (int32_t)idx;
		}
		h = (h + 1) & mask;
	}
}

// [=]===^=[ find_widget ]=======================================[=]
static struct mkgui_widget *find_widget(struct mkgui_window *win, uint32_t id) {
	int32_t idx = find_widget_idx(win, id);
	return idx >= 0 ? &win->widgets[idx] : NULL;
}

#define MKGUI_FIND_AUX(name, type, arr, cnt, enabled) \
static struct type *name(struct mkgui_window *win, uint32_t widget_id) { \
	for(uint32_t i = 0; (enabled) && i < win->cnt; ++i) { \
		if(win->arr[i].widget_id == widget_id) { \
			return &win->arr[i]; \
		} \
	} \
	return NULL; \
}

MKGUI_FIND_AUX(find_tabs_data,       mkgui_tabs_data,       tabs,        tab_count, (MKGUI_HAS_TABS))
MKGUI_FIND_AUX(find_listv_data,      mkgui_listview_data,   listvs,      listv_count, (MKGUI_HAS_LISTVIEW))
MKGUI_FIND_AUX(find_gridv_data,      mkgui_gridview_data,   gridviews,   gridview_count, (MKGUI_HAS_GRIDVIEW))
MKGUI_FIND_AUX(find_richlist_data,   mkgui_richlist_data,   richlists,   richlist_count, (MKGUI_HAS_RICHLIST))
MKGUI_FIND_AUX(find_input_data,      mkgui_input_data,      inputs,      input_count, (MKGUI_HAS_INPUT))
MKGUI_FIND_AUX(find_ipinput_data,    mkgui_ipinput_data,    ipinputs,    ipinput_count, (MKGUI_HAS_IPINPUT))
MKGUI_FIND_AUX(find_toggle_data,     mkgui_toggle_data,     toggles,     toggle_count, (MKGUI_HAS_TOGGLE))
MKGUI_FIND_AUX(find_combobox_data,   mkgui_combobox_data,   comboboxes,  combobox_count, (MKGUI_HAS_COMBOBOX))
MKGUI_FIND_AUX(find_datepicker_data, mkgui_datepicker_data, datepickers, datepicker_count, (MKGUI_HAS_DATEPICKER))
MKGUI_FIND_AUX(find_dropdown_data,   mkgui_dropdown_data,   dropdowns,   dropdown_count, (MKGUI_HAS_DROPDOWN))
MKGUI_FIND_AUX(find_slider_data,     mkgui_slider_data,     sliders,     slider_count, (MKGUI_HAS_SLIDER))
MKGUI_FIND_AUX(find_split_data,      mkgui_split_data,      splits,      split_count, (MKGUI_HAS_HSPLIT || MKGUI_HAS_VSPLIT))
MKGUI_FIND_AUX(find_treeview_data,   mkgui_treeview_data,   treeviews,   treeview_count, (MKGUI_HAS_TREEVIEW))
MKGUI_FIND_AUX(find_treelist_data,   mkgui_treelist_data,   treelists,   treelist_count, (MKGUI_HAS_TREELIST))
MKGUI_FIND_AUX(find_statusbar_data,  mkgui_statusbar_data,  statusbars,  statusbar_count, (MKGUI_HAS_STATUSBAR))
MKGUI_FIND_AUX(find_spinbox_data,    mkgui_spinbox_data,    spinboxes,   spinbox_count, (MKGUI_HAS_SPINBOX))
MKGUI_FIND_AUX(find_progress_data,   mkgui_progress_data,   progress,    progress_count, (MKGUI_HAS_PROGRESS))
MKGUI_FIND_AUX(find_meter_data,     mkgui_meter_data,      meters,      meter_count, (MKGUI_HAS_METER))
MKGUI_FIND_AUX(find_textarea_data,   mkgui_textarea_data,   textareas,   textarea_count, (MKGUI_HAS_TEXTAREA))
MKGUI_FIND_AUX(find_logview_data,    mkgui_logview_data,    logviews,    logview_count, (MKGUI_HAS_LOGVIEW))
MKGUI_FIND_AUX(find_itemview_data,   mkgui_itemview_data,   itemviews,   itemview_count, (MKGUI_HAS_ITEMVIEW))
MKGUI_FIND_AUX(find_canvas_data,     mkgui_canvas_data,     canvases,    canvas_count, (MKGUI_HAS_CANVAS))
MKGUI_FIND_AUX(find_pathbar_data,    mkgui_pathbar_data,    pathbars,    pathbar_count, (MKGUI_HAS_PATHBAR))

// ---------------------------------------------------------------------------
// Layout parent index (declared early for widget_visible)
// ---------------------------------------------------------------------------

static struct vm_arena layout_parent_arena;
static struct vm_arena layout_child_arena;
static struct vm_arena layout_sibling_arena;
static uint32_t *layout_parent;
static uint32_t *layout_first_child;
static uint32_t *layout_next_sibling;
static uint32_t layout_arr_cap;

static struct vm_arena layout_hash_arena;

// ---------------------------------------------------------------------------
// Widget visibility
// ---------------------------------------------------------------------------

// [=]===^=[ widget_visible ]====================================[=]
static uint32_t widget_visible(struct mkgui_window *win, uint32_t idx) {
	struct mkgui_widget *w = &win->widgets[idx];
	if(w->flags & MKGUI_HIDDEN) {
		return 0;
	}

	if((MKGUI_HAS_WINDOW && w->type == MKGUI_WINDOW)) {
		return 1;
	}

	if(layout_arr_cap > 0 && idx < layout_arr_cap) {
		uint32_t pidx = layout_parent[idx];
		while(pidx < win->widget_count) {
			struct mkgui_widget *parent = &win->widgets[pidx];
			if(parent->flags & MKGUI_HIDDEN) {
				return 0;
			}

			if((MKGUI_HAS_TAB && parent->type == MKGUI_TAB)) {
				uint32_t tabs_idx = layout_parent[pidx];
				if(tabs_idx < win->widget_count && (MKGUI_HAS_TABS && win->widgets[tabs_idx].type == MKGUI_TABS)) {
					struct mkgui_tabs_data *td = find_tabs_data(win, win->widgets[tabs_idx].id);
					if(td && td->active_tab != parent->id) {
						return 0;
					}
				}
			}

			if((MKGUI_HAS_GROUP && parent->type == MKGUI_GROUP) && (parent->style & MKGUI_GROUP_COLLAPSED)) {
				return 0;
			}

			if((MKGUI_HAS_WINDOW && parent->type == MKGUI_WINDOW)) {
				break;
			}
			pidx = layout_parent[pidx];
		}
	} else {
		uint32_t pid = w->parent_id;
		while(pid != 0) {
			struct mkgui_widget *parent = find_widget(win, pid);
			if(!parent) {
				break;
			}

			if(parent->flags & MKGUI_HIDDEN) {
				return 0;
			}

			if((MKGUI_HAS_TAB && parent->type == MKGUI_TAB)) {
				struct mkgui_widget *tabs_container = find_widget(win, parent->parent_id);
				if(tabs_container && (MKGUI_HAS_TABS && tabs_container->type == MKGUI_TABS)) {
					struct mkgui_tabs_data *td = find_tabs_data(win, tabs_container->id);
					if(td && td->active_tab != parent->id) {
						return 0;
					}
				}
			}

			if((MKGUI_HAS_GROUP && parent->type == MKGUI_GROUP) && (parent->style & MKGUI_GROUP_COLLAPSED)) {
				return 0;
			}
			pid = parent->parent_id;
		}
	}
	return 1;
}

// ---------------------------------------------------------------------------
// Per-type widget descriptor table
// ---------------------------------------------------------------------------

// One row per widget type, indexed by the MKGUI_* enum. Unlisted types fall
// through to the zero default: not focusable, flex height.
struct mkgui_widget_desc {
	uint8_t focusable;   // base per-type focusability; is_focusable also requires !DISABLED
	uint8_t fixed_h;     // 1 = holds natural height in a vbox; vflex_default = !fixed_h
};

static struct mkgui_widget_desc widget_desc[MKGUI_WIDGET_COUNT] = {
#if MKGUI_HAS_BUTTON
	[MKGUI_BUTTON]     = { .focusable = 1, .fixed_h = 1 },
#endif
#if MKGUI_HAS_LABEL
	[MKGUI_LABEL]      = { .fixed_h = 1 },
#endif
#if MKGUI_HAS_INPUT
	[MKGUI_INPUT]      = { .focusable = 1, .fixed_h = 1 },
#endif
#if MKGUI_HAS_CHECKBOX
	[MKGUI_CHECKBOX]   = { .focusable = 1, .fixed_h = 1 },
#endif
#if MKGUI_HAS_DROPDOWN
	[MKGUI_DROPDOWN]   = { .focusable = 1, .fixed_h = 1 },
#endif
#if MKGUI_HAS_SLIDER
	[MKGUI_SLIDER]     = { .focusable = 1, .fixed_h = 1 },
#endif
#if MKGUI_HAS_LISTVIEW
	[MKGUI_LISTVIEW]   = { .focusable = 1 },
#endif
#if MKGUI_HAS_TREEVIEW
	[MKGUI_TREEVIEW]   = { .focusable = 1 },
#endif
#if MKGUI_HAS_TREELIST
	[MKGUI_TREELIST]   = { .focusable = 1 },
#endif
#if MKGUI_HAS_SPINBOX
	[MKGUI_SPINBOX]    = { .focusable = 1, .fixed_h = 1 },
#endif
#if MKGUI_HAS_RADIO
	[MKGUI_RADIO]      = { .focusable = 1, .fixed_h = 1 },
#endif
#if MKGUI_HAS_TEXTAREA
	[MKGUI_TEXTAREA]   = { .focusable = 1 },
#endif
#if MKGUI_HAS_LOGVIEW
	[MKGUI_LOGVIEW]    = { .focusable = 1 },
#endif
#if MKGUI_HAS_ITEMVIEW
	[MKGUI_ITEMVIEW]   = { .focusable = 1 },
#endif
#if MKGUI_HAS_TABS
	[MKGUI_TABS]       = { .focusable = 1 },
#endif
#if MKGUI_HAS_PATHBAR
	[MKGUI_PATHBAR]    = { .focusable = 1, .fixed_h = 1 },
#endif
#if MKGUI_HAS_IPINPUT
	[MKGUI_IPINPUT]    = { .focusable = 1, .fixed_h = 1 },
#endif
#if MKGUI_HAS_TOGGLE
	[MKGUI_TOGGLE]     = { .focusable = 1, .fixed_h = 1 },
#endif
#if MKGUI_HAS_COMBOBOX
	[MKGUI_COMBOBOX]   = { .focusable = 1, .fixed_h = 1 },
#endif
#if MKGUI_HAS_DATEPICKER
	[MKGUI_DATEPICKER] = { .focusable = 1, .fixed_h = 1 },
#endif
#if MKGUI_HAS_GRIDVIEW
	[MKGUI_GRIDVIEW]   = { .focusable = 1 },
#endif
#if MKGUI_HAS_RICHLIST
	[MKGUI_RICHLIST]   = { .focusable = 1 },
#endif
#if MKGUI_HAS_SCROLLBAR
	[MKGUI_SCROLLBAR]  = { .fixed_h = 1 },
#endif
#if MKGUI_HAS_PROGRESS
	[MKGUI_PROGRESS]   = { .fixed_h = 1 },
#endif
#if MKGUI_HAS_METER
	[MKGUI_METER]      = { .fixed_h = 1 },
#endif
#if MKGUI_HAS_SPINNER
	[MKGUI_SPINNER]    = { .fixed_h = 1 },
#endif
#if MKGUI_HAS_DIVIDER
	[MKGUI_DIVIDER]    = { .fixed_h = 1 },
#endif
};

// ---------------------------------------------------------------------------
// Focusable check
// ---------------------------------------------------------------------------

// [=]===^=[ is_focusable ]======================================[=]
static uint32_t is_focusable(struct mkgui_widget *w) {
	if(w->flags & MKGUI_DISABLED) {
		return 0;
	}
	return widget_desc[w->type].focusable;
}

// ---------------------------------------------------------------------------
// Dirty rect tracking
// ---------------------------------------------------------------------------

// [=]===^=[ dirty_all ]==========================================[=]
static void dirty_all(struct mkgui_window *win) {
	win->dirty_full = 1;
	win->dirty = 1;
}

// [=]===^=[ dirty_add ]==========================================[=]
static void dirty_add(struct mkgui_window *win, int32_t x, int32_t y, int32_t w, int32_t h) {
	if(win->dirty_full) {
		return;
	}

	if(w <= 0 || h <= 0) {
		return;
	}

	for(uint32_t i = 0; i < win->dirty_count; ++i) {
		int32_t dx = win->dirty_rects[i].x;
		int32_t dy = win->dirty_rects[i].y;
		int32_t dw = win->dirty_rects[i].w;
		int32_t dh = win->dirty_rects[i].h;
		int32_t ux = dx < x ? dx : x;
		int32_t uy = dy < y ? dy : y;
		int32_t ur = (dx + dw) > (x + w) ? (dx + dw) : (x + w);
		int32_t ub = (dy + dh) > (y + h) ? (dy + dh) : (y + h);
		int32_t uw = ur - ux;
		int32_t uh = ub - uy;
		int64_t union_area = (int64_t)uw * uh;
		int64_t sum_area = (int64_t)dw * dh + (int64_t)w * h;
		if(union_area <= sum_area + sum_area / 2) {
			win->dirty_rects[i].x = ux;
			win->dirty_rects[i].y = uy;
			win->dirty_rects[i].w = uw;
			win->dirty_rects[i].h = uh;
			win->dirty = 1;
			return;
		}
	}

	if(win->dirty_count >= 32) {
		dirty_all(win);
		return;
	}

	win->dirty_rects[win->dirty_count].x = x;
	win->dirty_rects[win->dirty_count].y = y;
	win->dirty_rects[win->dirty_count].w = w;
	win->dirty_rects[win->dirty_count].h = h;
	++win->dirty_count;
	win->dirty = 1;
}

// [=]===^=[ dirty_widget ]========================================[=]
static void dirty_widget(struct mkgui_window *win, uint32_t idx) {
	dirty_add(win, win->rects[idx].x, win->rects[idx].y, win->rects[idx].w, win->rects[idx].h);
}

// [=]===^=[ dirty_widget_id ]=====================================[=]
static void dirty_widget_id(struct mkgui_window *win, uint32_t id) {
	int32_t idx = find_widget_idx(win, id);
	if(idx >= 0) {
		dirty_widget(win, (uint32_t)idx);
	}
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

// [=]===^=[ find_box_scroll ]==================================[=]
static struct mkgui_box_scroll *find_box_scroll(struct mkgui_window *win, uint32_t widget_id) {
	for(uint32_t i = 0; (MKGUI_HAS_VBOX || MKGUI_HAS_HBOX) && i < win->box_scroll_count; ++i) {
		if(win->box_scrolls[i].widget_id == widget_id) {
			return &win->box_scrolls[i];
		}
	}
	return NULL;
}

// ---------------------------------------------------------------------------
// Layout index (pre-built per layout pass for O(1) lookups)
// ---------------------------------------------------------------------------

static struct mkgui_layout_hash_entry *layout_id_map;
static uint32_t layout_hash_size;
static uint32_t layout_hash_mask;

// [=]===^=[ layout_find_idx ]====================================[=]
static uint32_t layout_find_idx(uint32_t id) {
	uint32_t h = (id * 2654435761u) & layout_hash_mask;
	for(;;) {
		if(layout_id_map[h].idx == UINT32_MAX) {
			return UINT32_MAX;
		}

		if(layout_id_map[h].id == id) {
			return layout_id_map[h].idx;
		}
		h = (h + 1) & layout_hash_mask;
	}
}

// [=]===^=[ layout_arena_init ]===================================[=]
static uint32_t layout_arena_init(void) {
	if(layout_parent_arena.base) {
		return 1;
	}
	size_t per_reserve = (size_t)MKGUI_VM_MAX_WIDGETS * sizeof(uint32_t);
	if(!vm_arena_create(&layout_parent_arena, per_reserve)) {
		return 0;
	}

	if(!vm_arena_create(&layout_child_arena, per_reserve)) {
		vm_arena_destroy(&layout_parent_arena);
		return 0;
	}

	if(!vm_arena_create(&layout_sibling_arena, per_reserve)) {
		vm_arena_destroy(&layout_parent_arena);
		vm_arena_destroy(&layout_child_arena);
		return 0;
	}
	layout_parent = (uint32_t *)layout_parent_arena.base;
	layout_first_child = (uint32_t *)layout_child_arena.base;
	layout_next_sibling = (uint32_t *)layout_sibling_arena.base;

	size_t hash_reserve = (size_t)MKGUI_VM_MAX_HASH_SLOTS * sizeof(struct mkgui_layout_hash_entry);
	if(!vm_arena_create(&layout_hash_arena, hash_reserve)) {
		vm_arena_destroy(&layout_parent_arena);
		vm_arena_destroy(&layout_child_arena);
		vm_arena_destroy(&layout_sibling_arena);
		layout_parent = NULL;
		layout_first_child = NULL;
		layout_next_sibling = NULL;
		return 0;
	}
	layout_id_map = (struct mkgui_layout_hash_entry *)layout_hash_arena.base;
	return 1;
}

// [=]===^=[ layout_arena_fini ]===================================[=]
static void layout_arena_fini(void) {
	vm_arena_destroy(&layout_parent_arena);
	vm_arena_destroy(&layout_child_arena);
	vm_arena_destroy(&layout_sibling_arena);
	vm_arena_destroy(&layout_hash_arena);
	layout_parent = NULL;
	layout_first_child = NULL;
	layout_next_sibling = NULL;
	layout_id_map = NULL;
	layout_arr_cap = 0;
	layout_hash_size = 0;
	layout_hash_mask = 0;
}

// ---------------------------------------------------------------------------
// Shared layout context (used by both runtime and editor)
// ---------------------------------------------------------------------------

// [=]===^=[ lw_get ]================================================[=]
static struct mkgui_widget *lw_get(struct layout_ctx *lc, uint32_t idx) {
	return (struct mkgui_widget *)((char *)lc->widgets + idx * lc->widget_stride);
}

// [=]===^=[ lw_sw ]=================================================[=]
static inline int32_t lw_sw(struct mkgui_window *win, int32_t val) {
	return val > 0 ? sc(win, val) : val;
}

// [=]===^=[ lc_find_idx ]===========================================[=]
static uint32_t lc_find_idx(struct layout_ctx *lc, uint32_t id) {
	// knuth multiplicative hash (0x9e3779b1) for good spread over power-of-two tables
	uint32_t h = (id * 2654435761u) & lc->hash_mask;
	for(;;) {
		if(lc->id_map[h].idx == UINT32_MAX) {
			return UINT32_MAX;
		}

		if(lc->id_map[h].id == id) {
			return lc->id_map[h].idx;
		}
		h = (h + 1) & lc->hash_mask;
	}
}

// [=]===^=[ lc_build_index ]========================================[=]
static void lc_build_index(struct layout_ctx *lc) {
	memset(lc->id_map, 0xff, (size_t)(lc->hash_mask + 1) * sizeof(*lc->id_map));
	for(uint32_t i = 0; i < lc->widget_count; ++i) {
		struct mkgui_widget *w = lw_get(lc, i);
		uint32_t h = (w->id * 2654435761u) & lc->hash_mask;
		while(lc->id_map[h].idx != UINT32_MAX) {
			h = (h + 1) & lc->hash_mask;
		}
		lc->id_map[h].id = w->id;
		lc->id_map[h].idx = i;
		lc->first_child[i] = UINT32_MAX;
		lc->next_sibling[i] = UINT32_MAX;
	}
	// build child linked lists by iterating in reverse: each child becomes
	// the new list head, so forward iteration later yields declaration order
	// without needing a tail pointer
	uint32_t i = lc->widget_count;
	while(i > 0) {
		--i;
		uint32_t pidx = lc_find_idx(lc, lw_get(lc, i)->parent_id);
		lc->parent[i] = pidx;
		if(pidx < lc->widget_count && pidx != i) {
			lc->next_sibling[i] = lc->first_child[pidx];
			lc->first_child[pidx] = i;
		}
	}
}

// [=]===^=[ layout_build_index ]==================================[=]
static void layout_build_index(struct mkgui_window *win) {
	if(win->widget_count > layout_arr_cap) {
		uint32_t nc = (win->widget_count + MKGUI_GROW_WIDGETS - 1) & ~(uint32_t)(MKGUI_GROW_WIDGETS - 1);
		if(nc > MKGUI_VM_MAX_WIDGETS) {
			return;
		}
		size_t needed = (size_t)nc * sizeof(uint32_t);
		if(!vm_arena_ensure(&layout_parent_arena, needed) || !vm_arena_ensure(&layout_child_arena, needed) || !vm_arena_ensure(&layout_sibling_arena, needed)) {
			return;
		}
		layout_arr_cap = nc;
	}
	uint32_t need = win->widget_count * 2;
	if(need < 256) {
		need = 256;
	}
	uint32_t hs = 256;
	while(hs < need) {
		hs <<= 1;
	}

	if(hs > layout_hash_size) {
		if(hs > MKGUI_VM_MAX_HASH_SLOTS) {
			return;
		}

		if(!vm_arena_ensure(&layout_hash_arena, (size_t)hs * sizeof(struct mkgui_layout_hash_entry))) {
			return;
		}
		layout_hash_size = hs;
		layout_hash_mask = hs - 1;
	}
	struct layout_ctx lc = {
		.widgets = win->widgets, .rects = win->rects,
		.first_child = layout_first_child, .next_sibling = layout_next_sibling,
		.parent = layout_parent, .id_map = layout_id_map, .hash_mask = layout_hash_mask,
		.widget_count = win->widget_count, .widget_stride = sizeof(struct mkgui_widget)
	};
	lc_build_index(&lc);
}

// Single-line controls hold their natural height in a VBOX instead of
// stretching; weight > 0 opts back into flex.
// [=]===^=[ widget_vflex_default ]==================================[=]
static uint32_t widget_vflex_default(uint32_t widget_type) {
	return !widget_desc[widget_type].fixed_h;
}

// [=]===^=[ vbox_child_is_fixed ]==================================[=]
static uint32_t vbox_child_is_fixed(struct mkgui_widget *jw) {
	if(jw->flags & MKGUI_FIXED) {
		return 1;
	}

	if((MKGUI_HAS_GROUP && jw->type == MKGUI_GROUP) && (jw->style & MKGUI_GROUP_COLLAPSIBLE)) {
		return 1;
	}

	if(jw->weight == 0 && !widget_vflex_default(jw->type)) {
		return 1;
	}
	return 0;
}

// [=]===^=[ natural_height ]=======================================[=]
static int32_t natural_height(struct mkgui_window *win, uint32_t widget_type) {
	switch(widget_type) {
#if MKGUI_HAS_BUTTON
		case MKGUI_BUTTON: {
			return win->font_height + sc(win, MKGUI_NAT_BUTTON_VPAD);
		}
#endif

#if MKGUI_HAS_INPUT || MKGUI_HAS_COMBOBOX || MKGUI_HAS_DROPDOWN || MKGUI_HAS_SPINBOX || MKGUI_HAS_DATEPICKER || MKGUI_HAS_IPINPUT || MKGUI_HAS_PATHBAR
		case MKGUI_INPUT:
		case MKGUI_COMBOBOX:
		case MKGUI_DROPDOWN:
		case MKGUI_SPINBOX:
		case MKGUI_DATEPICKER:
		case MKGUI_IPINPUT:
		case MKGUI_PATHBAR: {
			return win->font_height + sc(win, MKGUI_NAT_INPUT_VPAD);
		}
#endif

#if MKGUI_HAS_CHECKBOX || MKGUI_HAS_RADIO || MKGUI_HAS_TOGGLE
		case MKGUI_CHECKBOX:
		case MKGUI_RADIO:
		case MKGUI_TOGGLE: {
			return win->font_height + sc(win, MKGUI_NAT_CHECKBOX_VPAD);
		}
#endif

#if MKGUI_HAS_LABEL
		case MKGUI_LABEL: {
			return win->font_height + sc(win, MKGUI_NAT_LABEL_VPAD);
		}
#endif

#if MKGUI_HAS_SLIDER || MKGUI_HAS_SCROLLBAR
		case MKGUI_SLIDER:
		case MKGUI_SCROLLBAR: {
			return sc(win, MKGUI_NAT_SLIDER_H);
		}
#endif

#if MKGUI_HAS_PROGRESS || MKGUI_HAS_METER
		case MKGUI_PROGRESS:
		case MKGUI_METER: {
			return sc(win, MKGUI_NAT_PROGRESS_H);
		}
#endif

#if MKGUI_HAS_DIVIDER
		case MKGUI_DIVIDER: {
			return sc(win, MKGUI_NAT_DIVIDER_H);
		}
#endif

#if MKGUI_HAS_LISTVIEW || MKGUI_HAS_GRIDVIEW || MKGUI_HAS_RICHLIST || MKGUI_HAS_TREEVIEW || MKGUI_HAS_TREELIST || MKGUI_HAS_ITEMVIEW || MKGUI_HAS_TEXTAREA || MKGUI_HAS_LOGVIEW
		case MKGUI_LISTVIEW:
		case MKGUI_GRIDVIEW:
		case MKGUI_RICHLIST:
		case MKGUI_TREEVIEW:
		case MKGUI_TREELIST:
		case MKGUI_ITEMVIEW:
		case MKGUI_TEXTAREA:
		case MKGUI_LOGVIEW: {
			return win->row_height * 3;
		}
#endif

#if MKGUI_HAS_TABS
		case MKGUI_TABS: {
			return win->tab_height + win->row_height * 2;
		}
#endif

#if MKGUI_HAS_HSPLIT || MKGUI_HAS_VSPLIT
		case MKGUI_HSPLIT:
		case MKGUI_VSPLIT: {
			return win->split_min_px * 2 + win->split_thick;
		}
#endif

#if MKGUI_HAS_IMAGE || MKGUI_HAS_CANVAS || MKGUI_HAS_GLVIEW || MKGUI_HAS_SPINNER
		case MKGUI_IMAGE:
		case MKGUI_CANVAS:
		case MKGUI_GLVIEW:
		case MKGUI_SPINNER: {
			return sc(win, MKGUI_NAT_CANVAS_H);
		}
#endif

#if MKGUI_HAS_VBOX || MKGUI_HAS_HBOX || MKGUI_HAS_FORM || MKGUI_HAS_PANEL || MKGUI_HAS_GROUP
		case MKGUI_VBOX:
		case MKGUI_HBOX:
		case MKGUI_FORM:
		case MKGUI_PANEL:
		case MKGUI_GROUP: {
			return win->row_height;
		}
#endif

		default: {
			return 0;
		}
	}
}

// [=]===^=[ natural_width ]=======================================[=]
static int32_t natural_width(struct mkgui_window *win, struct mkgui_widget *w) {
	switch(w->type) {
#if MKGUI_HAS_BUTTON
		case MKGUI_BUTTON: {
			int32_t tw = w->label[0] ? label_text_width(win, w) : 0;
			int32_t iw = w->icon[0] ? (win->icon_size + (w->label[0] ? sc(win, MKGUI_NAT_BUTTON_ICON_GAP) : 0)) : 0;
			return tw + iw + sc(win, MKGUI_NAT_BUTTON_HPAD);
		}
#endif

#if MKGUI_HAS_LABEL
		case MKGUI_LABEL: {
			int32_t tw = label_text_width(win, w);
			return tw + sc(win, MKGUI_NAT_LABEL_HPAD);
		}
#endif

#if MKGUI_HAS_CHECKBOX || MKGUI_HAS_RADIO
		case MKGUI_CHECKBOX:
		case MKGUI_RADIO: {
			int32_t tw = label_text_width(win, w);
			return sc(win, MKGUI_NAT_CHECKBOX_BOX_W) + tw;
		}
#endif

#if MKGUI_HAS_TOGGLE
		case MKGUI_TOGGLE: {
			int32_t tw = label_text_width(win, w);
			return sc(win, MKGUI_NAT_TOGGLE_BOX_W) + tw;
		}
#endif

#if MKGUI_HAS_DIVIDER
		case MKGUI_DIVIDER: {
			return sc(win, MKGUI_NAT_DIVIDER_W);
		}
#endif

		default: {
			return sc(win, MKGUI_NAT_DEFAULT_W);
		}
	}
}

// [=]===^=[ lc_measure_container ]================================[=]
static int32_t lc_measure_container(struct mkgui_window *win, struct layout_ctx *lc, uint32_t idx, uint32_t axis) {
	struct mkgui_widget *w = lw_get(lc, idx);
	if((MKGUI_HAS_GROUP && w->type == MKGUI_GROUP)) {
		int32_t gtop = win->font_height + 4;
		if(w->style & MKGUI_GROUP_COLLAPSED) {
			return (axis == 1) ? gtop : 0;
		}
		int32_t gpad = win->box_pad;
		for(uint32_t j = lc->first_child[idx]; j < lc->widget_count; j = lc->next_sibling[j]) {
			uint32_t ct = lw_get(lc, j)->type;
			if((MKGUI_HAS_VBOX && ct == MKGUI_VBOX) || (MKGUI_HAS_HBOX && ct == MKGUI_HBOX) || (MKGUI_HAS_FORM && ct == MKGUI_FORM) || (MKGUI_HAS_PANEL && ct == MKGUI_PANEL)) {
				int32_t inner = lc_measure_container(win, lc, j, axis);
				if(axis == 1) {
					return inner + gtop + gpad;
				}
				return inner + gpad * 2;
			}
		}
		return 0;
	}

	uint32_t is_vbox = ((MKGUI_HAS_VBOX && w->type == MKGUI_VBOX) || (MKGUI_HAS_PANEL && w->type == MKGUI_PANEL));
	uint32_t is_hbox = ((MKGUI_HAS_HBOX && w->type == MKGUI_HBOX));
	uint32_t is_form = ((MKGUI_HAS_FORM && w->type == MKGUI_FORM));
	if(!is_vbox && !is_hbox && !is_form) {
		return 0;
	}

	if(is_form) {
		uint32_t pair_count = 0;
		for(uint32_t j = lc->first_child[idx]; j < lc->widget_count; j = lc->next_sibling[j]) {
			++pair_count;
		}
		uint32_t rows = (pair_count + 1) / 2;
		int32_t row_h = win->font_height + 10;
		int32_t h = (int32_t)rows * row_h + (rows > 1 ? (int32_t)(rows - 1) * win->box_gap : 0);
		return h;
	}

	int32_t main_total = 0;
	int32_t cross_max = 0;
	uint32_t visible = 0;
	for(uint32_t j = lc->first_child[idx]; j < lc->widget_count; j = lc->next_sibling[j]) {
		struct mkgui_widget *jw = lw_get(lc, j);
		if(jw->flags & MKGUI_HIDDEN) {
			continue;
		}
		++visible;
		int32_t child_main;
		int32_t child_cross;
		uint32_t ct = jw->type;
		if(is_vbox) {
			child_main = lw_sw(win, jw->h);
			child_cross = lw_sw(win, jw->w);
		} else {
			child_main = lw_sw(win, jw->w);
			child_cross = lw_sw(win, jw->h);
		}

		if(child_main == 0 && ((MKGUI_HAS_VBOX && ct == MKGUI_VBOX) || (MKGUI_HAS_HBOX && ct == MKGUI_HBOX) || (MKGUI_HAS_FORM && ct == MKGUI_FORM) || (MKGUI_HAS_GROUP && ct == MKGUI_GROUP) || (MKGUI_HAS_PANEL && ct == MKGUI_PANEL))) {
			child_main = lc_measure_container(win, lc, j, is_vbox ? 1 : 0);
		}

		if(child_main == 0) {
			if(is_vbox) {
				child_main = natural_height(win, ct);
			} else {
				child_main = natural_width(win, jw);
			}
		}

		if(child_cross == 0 && ((MKGUI_HAS_VBOX && ct == MKGUI_VBOX) || (MKGUI_HAS_HBOX && ct == MKGUI_HBOX) || (MKGUI_HAS_FORM && ct == MKGUI_FORM) || (MKGUI_HAS_GROUP && ct == MKGUI_GROUP) || (MKGUI_HAS_PANEL && ct == MKGUI_PANEL))) {
			child_cross = lc_measure_container(win, lc, j, is_vbox ? 0 : 1);
		}

		if(child_cross == 0) {
			if(is_vbox) {
				child_cross = natural_width(win, jw);
			} else {
				child_cross = natural_height(win, ct);
			}
		}
		main_total += child_main;
		if(child_cross > cross_max) {
			cross_max = child_cross;
		}
	}
	int32_t gap_total = visible > 1 ? (int32_t)(visible - 1) * win->box_gap : 0;
	main_total += gap_total;

	// only apply padding at outermost box level to avoid double-padding in
	// nested containers; bordered boxes always get padding for visual separation
	uint32_t has_pad = 0;
	if(!(w->flags & MKGUI_NO_PAD)) {
		uint32_t gpidx = lc->parent[idx];
		uint32_t nested = 0;
		if(gpidx < lc->widget_count) {
			uint32_t gpt = lw_get(lc, gpidx)->type;
			if((MKGUI_HAS_VBOX && gpt == MKGUI_VBOX) || (MKGUI_HAS_HBOX && gpt == MKGUI_HBOX) || (MKGUI_HAS_FORM && gpt == MKGUI_FORM) || (MKGUI_HAS_GROUP && gpt == MKGUI_GROUP) || (MKGUI_HAS_TABS && gpt == MKGUI_TABS)) {
				nested = 1;
			}
		}

		if(!nested || (w->style & STYLE_BORDER_BIT)) {
			has_pad = 1;
		}
	}

	if(has_pad) {
		main_total += win->box_pad * 2;
		cross_max += win->box_pad * 2;
	}

	if(axis == 0) {
		return is_vbox ? cross_max : main_total;
	}
	return is_vbox ? main_total : cross_max;
}

// [=]===^=[ lc_layout_node ]=====================================[=]
static void lc_layout_node(struct mkgui_window *win, struct layout_ctx *lc, uint32_t idx) {
	struct mkgui_widget *w = lw_get(lc, idx);

	int32_t px = lc->rects[idx].x;
	int32_t py = lc->rects[idx].y;
	int32_t pw = lc->rects[idx].w;
	int32_t ph = lc->rects[idx].h;

	if((MKGUI_HAS_WINDOW && w->type == MKGUI_WINDOW)) {
		int32_t top_y = py;
		int32_t bot_y = py + ph;
		uint32_t chrome[16];
		uint32_t chrome_count = 0;
		for(uint32_t c = lc->first_child[idx]; c < lc->widget_count; c = lc->next_sibling[c]) {
			struct mkgui_widget *cw = lw_get(lc, c);
			uint32_t ct = cw->type;
			if((MKGUI_HAS_MENU && ct == MKGUI_MENU) || (MKGUI_HAS_TOOLBAR && ct == MKGUI_TOOLBAR) || (MKGUI_HAS_STATUSBAR && ct == MKGUI_STATUSBAR) || ((MKGUI_HAS_PATHBAR && ct == MKGUI_PATHBAR) && lw_sw(win, cw->h) == 0)) {
				if(chrome_count < 16) {
					chrome[chrome_count++] = c;
				}
			}
		}
		for(uint32_t i = 0; i < chrome_count; ++i) {
			uint32_t c = chrome[i];
			if((MKGUI_HAS_MENU && lw_get(lc, c)->type == MKGUI_MENU)) {
				lc->rects[c].x = px;
				lc->rects[c].y = top_y;
				lc->rects[c].w = pw;
				lc->rects[c].h = win->menu_height;
				top_y += win->menu_height;
			}
		}
		for(uint32_t i = 0; i < chrome_count; ++i) {
			uint32_t c = chrome[i];
			struct mkgui_widget *cw = lw_get(lc, c);
			if((MKGUI_HAS_TOOLBAR && cw->type == MKGUI_TOOLBAR)) {
				uint32_t tb_mode = cw->style & MKGUI_TOOLBAR_MODE_MASK;
				int32_t th;
				if(tb_mode == MKGUI_TOOLBAR_TEXT_ONLY) {
					th = win->font_height + 10;
				} else {
					uint32_t tb_has_icons = 0;
					for(uint32_t j = lc->first_child[c]; j < lc->widget_count; j = lc->next_sibling[j]) {
						if(lw_get(lc, j)->icon[0]) {
							tb_has_icons = 1;
							break;
						}
					}

					if(tb_has_icons && win->icon_size > 0) {
						int32_t ih = win->icon_size;
						th = (tb_mode == MKGUI_TOOLBAR_ICONS_ONLY) ? ih + 10 : ((ih > win->font_height ? ih : win->font_height) + 10);
					} else {
						th = win->font_height + 10;
					}
				}
				lc->rects[c].x = px;
				lc->rects[c].y = top_y;
				lc->rects[c].w = pw;
				lc->rects[c].h = th;
				top_y += th;
			}
		}
		for(uint32_t i = 0; i < chrome_count; ++i) {
			uint32_t c = chrome[i];
			struct mkgui_widget *cw = lw_get(lc, c);
			if((MKGUI_HAS_PATHBAR && cw->type == MKGUI_PATHBAR)) {
				lc->rects[c].x = px + lw_sw(win, cw->margin_l);
				lc->rects[c].y = top_y;
				lc->rects[c].w = pw - lw_sw(win, cw->margin_l) - lw_sw(win, cw->margin_r);
				lc->rects[c].h = win->pathbar_height;
				top_y += win->pathbar_height;
			}
		}
		for(uint32_t i = 0; i < chrome_count; ++i) {
			uint32_t c = chrome[i];
			if((MKGUI_HAS_STATUSBAR && lw_get(lc, c)->type == MKGUI_STATUSBAR)) {
				bot_y -= win->statusbar_height;
				lc->rects[c].x = px;
				lc->rects[c].y = bot_y;
				lc->rects[c].w = pw;
				lc->rects[c].h = win->statusbar_height;
			}
		}
		py = top_y;
		ph = bot_y - top_y;
	}

	if((MKGUI_HAS_TAB && w->type == MKGUI_TAB)) {
		int32_t tab_inset = sc(win, MKGUI_TAB_INSET);
		px += tab_inset;
		py += win->tab_height + tab_inset;
		pw -= tab_inset * 2;
		ph -= win->tab_height + tab_inset * 2;
	}

	if((MKGUI_HAS_GROUP && w->type == MKGUI_GROUP)) {
		if(w->style & MKGUI_GROUP_COLLAPSED) {
			return;
		}
		int32_t gtop = win->font_height + 4;
		int32_t gpad = win->box_pad;
		px += gpad;
		py += gtop;
		pw -= gpad * 2;
		ph -= gtop + gpad;
	}

	// suppress padding for nested containers to avoid compounding insets;
	// bordered boxes override this to maintain visual separation
	uint32_t box_has_pad = 0;
	uint32_t is_panel_vbox = ((MKGUI_HAS_PANEL && w->type == MKGUI_PANEL));
	if((MKGUI_HAS_VBOX && w->type == MKGUI_VBOX) || (MKGUI_HAS_HBOX && w->type == MKGUI_HBOX) || (MKGUI_HAS_FORM && w->type == MKGUI_FORM) || is_panel_vbox) {
		if(!(w->flags & MKGUI_NO_PAD)) {
			uint32_t gpidx = lc->parent[idx];
			uint32_t nested = 0;
			if(gpidx < lc->widget_count) {
				uint32_t gpt = lw_get(lc, gpidx)->type;
				if((MKGUI_HAS_VBOX && gpt == MKGUI_VBOX) || (MKGUI_HAS_HBOX && gpt == MKGUI_HBOX) || (MKGUI_HAS_FORM && gpt == MKGUI_FORM) || (MKGUI_HAS_GROUP && gpt == MKGUI_GROUP) || (MKGUI_HAS_TABS && gpt == MKGUI_TABS) || (MKGUI_HAS_PANEL && gpt == MKGUI_PANEL)) {
					nested = 1;
				}
			}

			if(!nested || (w->style & STYLE_BORDER_BIT)) {
				px += win->box_pad;
				py += win->box_pad;
				pw -= win->box_pad * 2;
				ph -= win->box_pad * 2;
				box_has_pad = 1;
			}
		}

		if((MKGUI_HAS_VBOX && w->type == MKGUI_VBOX) || is_panel_vbox) {
			uint32_t scrollable = lc->has_scroll && (w->flags & MKGUI_SCROLL) ? 1 : 0;
			struct mkgui_box_scroll *bs = scrollable ? find_box_scroll(win, w->id) : NULL;
			int32_t fixed_total = 0;
			int32_t min_total = 0;
			uint32_t weight_total = 0;
			uint32_t child_count = 0;
			for(uint32_t j = lc->first_child[idx]; j < lc->widget_count; j = lc->next_sibling[j]) {
				struct mkgui_widget *jw = lw_get(lc, j);
				if(jw->flags & MKGUI_HIDDEN) {
					continue;
				}
				++child_count;
				// collapsible groups are deterministically sized; single-line
				// leaves with weight==0 hold their natural height instead of
				// eating leftover space (see widget_vflex_default comment)
				uint32_t treat_fixed = vbox_child_is_fixed(jw);
				if(treat_fixed) {
					int32_t fh = lw_sw(win, jw->h);
					if((MKGUI_HAS_GROUP && jw->type == MKGUI_GROUP) && (jw->style & MKGUI_GROUP_COLLAPSED)) {
						fh = win->font_height + 4;
					}
					uint32_t ct = jw->type;
					if(fh == 0 && ((MKGUI_HAS_VBOX && ct == MKGUI_VBOX) || (MKGUI_HAS_HBOX && ct == MKGUI_HBOX) || (MKGUI_HAS_FORM && ct == MKGUI_FORM) || (MKGUI_HAS_GROUP && ct == MKGUI_GROUP) || (MKGUI_HAS_PANEL && ct == MKGUI_PANEL))) {
						fh = lc_measure_container(win, lc, j, 1);
					}

					if(fh == 0) {
						fh = natural_height(win, ct);
					}
					lc->rects[j].h = fh;
					fixed_total += fh;
				} else {
					uint32_t ew = jw->weight > 0 ? jw->weight : 1;
					int32_t minh = lw_sw(win, jw->h);
					if(minh == 0) {
						uint32_t ct = jw->type;
						if((MKGUI_HAS_VBOX && ct == MKGUI_VBOX) || (MKGUI_HAS_HBOX && ct == MKGUI_HBOX) || (MKGUI_HAS_FORM && ct == MKGUI_FORM) || (MKGUI_HAS_GROUP && ct == MKGUI_GROUP) || (MKGUI_HAS_PANEL && ct == MKGUI_PANEL)) {
							minh = lc_measure_container(win, lc, j, 1);
						}

						if(minh == 0) {
							minh = natural_height(win, jw->type);
						}
					}
					lc->rects[j].h = minh;
					min_total += minh;
					weight_total += ew;
				}
			}
			int32_t gap_total = child_count > 1 ? (int32_t)(child_count - 1) * win->box_gap : 0;
			int32_t content_h = fixed_total + min_total + gap_total;
			uint32_t needs_scroll = scrollable && (content_h > ph);
			// scrollbar steals width from content area after initial measurement
			if(needs_scroll) {
				pw -= win->scrollbar_w;
			}

			if(bs) {
				bs->content_h = content_h;
				if(box_has_pad) {
					bs->content_h += win->box_pad * 2;
				}
			}
			int32_t remaining = ph - fixed_total - min_total - gap_total;
			// when scrolling, content already exceeds the viewport; clamp
			// remaining to zero so weight distribution doesn't go negative
			if(needs_scroll) {
				if(remaining < 0) {
					remaining = 0;
				}
			}
			int32_t scroll_off = (bs && needs_scroll) ? bs->scroll_y : 0;
			int32_t cy = py - scroll_off;
			for(uint32_t j = lc->first_child[idx]; j < lc->widget_count; j = lc->next_sibling[j]) {
				struct mkgui_widget *jw = lw_get(lc, j);
				if(jw->flags & MKGUI_HIDDEN) {
					continue;
				}
				int32_t ch;
				uint32_t child_fixed = vbox_child_is_fixed(jw);
				if(child_fixed) {
					ch = lc->rects[j].h;
				} else {
					uint32_t wt = jw->weight > 0 ? jw->weight : 1;
					int32_t base_h = lc->rects[j].h;
					// 64-bit intermediate prevents overflow when remaining * weight exceeds 2^31
					ch = base_h + (weight_total > 0 ? (int32_t)((int64_t)remaining * (int32_t)wt / (int32_t)weight_total) : 0);
				}
				// floor flexible children at content size so they never collapse
				// below usable dimensions; fixed children keep their declared size
				if(!child_fixed) {
					int32_t min_ch = lc->rects[j].h;
					if(min_ch < 1) {
						min_ch = 1;
					}
					if(ch < min_ch) {
						ch = min_ch;
					}
				}
				int32_t cx_child = px;
				int32_t cw_child = pw;
				uint32_t align = jw->flags & MKGUI_ALIGN_MASK;
				if(align && lw_sw(win, jw->w) > 0) {
					if(align == MKGUI_ALIGN_START) {
						cw_child = lw_sw(win, jw->w);
					} else if(align == MKGUI_ALIGN_CENTER) {
						cw_child = lw_sw(win, jw->w);
						cx_child = px + (pw - cw_child) / 2;
					} else if(align == MKGUI_ALIGN_END) {
						cw_child = lw_sw(win, jw->w);
						cx_child = px + pw - cw_child;
					}
				}
				lc->rects[j].x = cx_child;
				lc->rects[j].y = cy;
				lc->rects[j].w = cw_child;
				lc->rects[j].h = ch;
				cy += ch + win->box_gap;
			}

		} else if((MKGUI_HAS_HBOX && w->type == MKGUI_HBOX)) {
			uint32_t scrollable = lc->has_scroll && (w->flags & MKGUI_SCROLL) ? 1 : 0;
			struct mkgui_box_scroll *bs = scrollable ? find_box_scroll(win, w->id) : NULL;
			int32_t fixed_total = 0;
			int32_t min_total = 0;
			uint32_t weight_total = 0;
			uint32_t child_count = 0;
			for(uint32_t j = lc->first_child[idx]; j < lc->widget_count; j = lc->next_sibling[j]) {
				struct mkgui_widget *jw = lw_get(lc, j);
				if(jw->flags & MKGUI_HIDDEN) {
					continue;
				}
				++child_count;
				if(jw->flags & MKGUI_FIXED) {
					int32_t fw = lw_sw(win, jw->w);
					uint32_t ct = jw->type;
					if(fw == 0 && ((MKGUI_HAS_VBOX && ct == MKGUI_VBOX) || (MKGUI_HAS_HBOX && ct == MKGUI_HBOX) || (MKGUI_HAS_FORM && ct == MKGUI_FORM) || (MKGUI_HAS_GROUP && ct == MKGUI_GROUP) || (MKGUI_HAS_PANEL && ct == MKGUI_PANEL))) {
						fw = lc_measure_container(win, lc, j, 0);
					}

					if(fw == 0) {
						fw = natural_width(win, jw);
					}
					lc->rects[j].w = fw;
					fixed_total += fw;
				} else {
					uint32_t ew = jw->weight > 0 ? jw->weight : 1;
					int32_t minw = lw_sw(win, jw->w);
					if(minw == 0) {
						uint32_t ct = jw->type;
						if((MKGUI_HAS_VBOX && ct == MKGUI_VBOX) || (MKGUI_HAS_HBOX && ct == MKGUI_HBOX) || (MKGUI_HAS_FORM && ct == MKGUI_FORM) || (MKGUI_HAS_GROUP && ct == MKGUI_GROUP) || (MKGUI_HAS_PANEL && ct == MKGUI_PANEL)) {
							minw = lc_measure_container(win, lc, j, 0);
						}

						if(minw == 0) {
							minw = natural_width(win, jw);
						}
					}
					lc->rects[j].w = minw;
					min_total += minw;
					weight_total += ew;
				}
			}
			int32_t gap_total = child_count > 1 ? (int32_t)(child_count - 1) * win->box_gap : 0;
			int32_t content_w = fixed_total + min_total + gap_total;
			uint32_t needs_scroll = scrollable && (content_w > pw);
			if(needs_scroll) {
				ph -= win->scrollbar_w;
			}

			if(bs) {
				bs->content_w = content_w;
			}
			int32_t remaining = pw - fixed_total - min_total - gap_total;
			if(needs_scroll) {
				if(remaining < 0) {
					remaining = 0;
				}
			}
			int32_t scroll_off = (bs && needs_scroll) ? bs->scroll_x : 0;
			int32_t cx = px - scroll_off;
			for(uint32_t j = lc->first_child[idx]; j < lc->widget_count; j = lc->next_sibling[j]) {
				struct mkgui_widget *jw = lw_get(lc, j);
				if(jw->flags & MKGUI_HIDDEN) {
					continue;
				}
				int32_t cw;
				if(jw->flags & MKGUI_FIXED) {
					cw = lc->rects[j].w;
				} else {
					uint32_t wt = jw->weight > 0 ? jw->weight : 1;
					int32_t base_w = lc->rects[j].w;
					cw = base_w + (weight_total > 0 ? (int32_t)((int64_t)remaining * (int32_t)wt / (int32_t)weight_total) : 0);
				}

				if(!(jw->flags & MKGUI_FIXED)) {
					int32_t min_cw = lc->rects[j].w;
					if(min_cw < 1) {
						min_cw = 1;
					}
					if(cw < min_cw) {
						cw = min_cw;
					}
				}
				int32_t cy_child = py;
				int32_t ch_child = ph;
				uint32_t align = jw->flags & MKGUI_ALIGN_MASK;
				int32_t explicit_h = lw_sw(win, jw->h);
				if(align && explicit_h > 0) {
					ch_child = explicit_h;
					if(align == MKGUI_ALIGN_CENTER) {
						cy_child = py + (ph - ch_child) / 2;
					} else if(align == MKGUI_ALIGN_END) {
						cy_child = py + ph - ch_child;
					}
				} else if(explicit_h > 0 || ((jw->flags & MKGUI_FIXED) && !widget_vflex_default(jw->type))) {
					int32_t nat_h = explicit_h > 0 ? explicit_h : natural_height(win, jw->type);
					if(nat_h > 0 && nat_h < ph) {
						ch_child = nat_h;
						uint32_t eff_align = align ? align : MKGUI_ALIGN_CENTER;
						if(eff_align == MKGUI_ALIGN_CENTER) {
							cy_child = py + (ph - ch_child) / 2;
						} else if(eff_align == MKGUI_ALIGN_END) {
							cy_child = py + ph - ch_child;
						}
					}
				}
				lc->rects[j].x = cx;
				lc->rects[j].y = cy_child;
				lc->rects[j].w = cw;
				lc->rects[j].h = ch_child;
				cx += cw + win->box_gap;
			}

		} else {
			int32_t label_w = 0;
			uint32_t pair_idx = 0;
			for(uint32_t j = lc->first_child[idx]; j < lc->widget_count; j = lc->next_sibling[j]) {
				if((pair_idx & 1) == 0) {
					int32_t tw = label_text_width(win, lw_get(lc, j)) + 8;
					if(tw > label_w) {
						label_w = tw;
					}
				}
				++pair_idx;
			}
			pair_idx = 0;
			for(uint32_t j = lc->first_child[idx]; j < lc->widget_count; j = lc->next_sibling[j]) {
				uint32_t row = pair_idx / 2;
				uint32_t row_h = (uint32_t)(win->font_height + 10);
				if((pair_idx & 1) == 0) {
					lc->rects[j].x = px;
					lc->rects[j].y = py + (int32_t)(row * (row_h + (uint32_t)win->box_gap));
					lc->rects[j].w = label_w;
					lc->rects[j].h = (int32_t)row_h;
				} else {
					lc->rects[j].x = px + label_w;
					lc->rects[j].y = py + (int32_t)(row * (row_h + (uint32_t)win->box_gap));
					lc->rects[j].w = pw - label_w;
					lc->rects[j].h = (int32_t)row_h;
				}
				++pair_idx;
			}
		}

	} else if((MKGUI_HAS_TOOLBAR && w->type == MKGUI_TOOLBAR)) {
		// toolbar children are positioned by render_toolbar

	} else {
		if((MKGUI_HAS_HSPLIT && w->type == MKGUI_HSPLIT) || (MKGUI_HAS_VSPLIT && w->type == MKGUI_VSPLIT)) {
			struct mkgui_split_data *sd = find_split_data(win, w->id);
			float ratio = sd ? sd->ratio : 0.5f;
			if((MKGUI_HAS_HSPLIT && w->type == MKGUI_HSPLIT)) {
				int32_t split = (int32_t)(ph * ratio);
				for(uint32_t c = lc->first_child[idx]; c < lc->widget_count; c = lc->next_sibling[c]) {
					struct mkgui_widget *cw = lw_get(lc, c);
					if(cw->flags & MKGUI_HIDDEN) {
						continue;
					}

					if(cw->flags & MKGUI_REGION_TOP) {
						lc->rects[c].x = px;
						lc->rects[c].y = py;
						lc->rects[c].w = pw;
						lc->rects[c].h = split;
					} else if(cw->flags & MKGUI_REGION_BOTTOM) {
						lc->rects[c].x = px;
						lc->rects[c].y = py + split + win->split_thick;
						lc->rects[c].w = pw;
						lc->rects[c].h = ph - split - win->split_thick;
					}
				}
			} else {
				int32_t split = (int32_t)(pw * ratio);
				for(uint32_t c = lc->first_child[idx]; c < lc->widget_count; c = lc->next_sibling[c]) {
					struct mkgui_widget *cw = lw_get(lc, c);
					if(cw->flags & MKGUI_HIDDEN) {
						continue;
					}

					if(cw->flags & MKGUI_REGION_LEFT) {
						lc->rects[c].x = px;
						lc->rects[c].y = py;
						lc->rects[c].w = split;
						lc->rects[c].h = ph;
					} else if(cw->flags & MKGUI_REGION_RIGHT) {
						lc->rects[c].x = px + split + win->split_thick;
						lc->rects[c].y = py;
						lc->rects[c].w = pw - split - win->split_thick;
						lc->rects[c].h = ph;
					}
				}
			}
		} else {
			for(uint32_t c = lc->first_child[idx]; c < lc->widget_count; c = lc->next_sibling[c]) {
				struct mkgui_widget *cw = lw_get(lc, c);
				uint32_t ct = cw->type;
				if((MKGUI_HAS_MENUITEM && ct == MKGUI_MENUITEM) || (cw->flags & MKGUI_HIDDEN)) {
					continue;
				}

				if((MKGUI_HAS_WINDOW && w->type == MKGUI_WINDOW) && ((MKGUI_HAS_MENU && ct == MKGUI_MENU) || (MKGUI_HAS_TOOLBAR && ct == MKGUI_TOOLBAR) || (MKGUI_HAS_STATUSBAR && ct == MKGUI_STATUSBAR))) {
					continue;
				}

				if((MKGUI_HAS_WINDOW && w->type == MKGUI_WINDOW) && (MKGUI_HAS_PATHBAR && ct == MKGUI_PATHBAR) && lw_sw(win, cw->h) == 0) {
					continue;
				}
				int32_t ml = lw_sw(win, cw->margin_l);
				int32_t mr = lw_sw(win, cw->margin_r);
				int32_t mt = lw_sw(win, cw->margin_t);
				int32_t mb = lw_sw(win, cw->margin_b);
				lc->rects[c].x = px + ml;
				lc->rects[c].y = py + mt;
				lc->rects[c].w = pw - ml - mr;
				lc->rects[c].h = ph - mt - mb;
				if(cw->flags & MKGUI_FIXED) {
					int32_t fw = lw_sw(win, cw->w);
					if(fw == 0) {
						if((MKGUI_HAS_VBOX && ct == MKGUI_VBOX) || (MKGUI_HAS_HBOX && ct == MKGUI_HBOX) || (MKGUI_HAS_FORM && ct == MKGUI_FORM) || (MKGUI_HAS_GROUP && ct == MKGUI_GROUP) || (MKGUI_HAS_PANEL && ct == MKGUI_PANEL)) {
							fw = lc_measure_container(win, lc, c, 0);
						}

						if(fw == 0) {
							fw = natural_width(win, cw);
						}
					}
					lc->rects[c].w = fw;
					int32_t fh = lw_sw(win, cw->h);
					if(fh == 0) {
						if((MKGUI_HAS_VBOX && ct == MKGUI_VBOX) || (MKGUI_HAS_HBOX && ct == MKGUI_HBOX) || (MKGUI_HAS_FORM && ct == MKGUI_FORM) || (MKGUI_HAS_GROUP && ct == MKGUI_GROUP) || (MKGUI_HAS_PANEL && ct == MKGUI_PANEL)) {
							fh = lc_measure_container(win, lc, c, 1);
						}

						if(fh == 0) {
							fh = natural_height(win, ct);
						}
					}
					lc->rects[c].h = fh;
				}
			}
		}
	}

	for(uint32_t c = lc->first_child[idx]; c < lc->widget_count; c = lc->next_sibling[c]) {
		struct mkgui_widget *cw = lw_get(lc, c);
		if((!MKGUI_HAS_MENUITEM || cw->type != MKGUI_MENUITEM) && !(cw->flags & MKGUI_HIDDEN)) {
			lc_layout_node(win, lc, c);
		}
	}
}

// [=]===^=[ layout_node ]========================================[=]
static void layout_node(struct mkgui_window *win, uint32_t idx) {
	struct layout_ctx lc = {
		.widgets = win->widgets, .rects = win->rects,
		.first_child = layout_first_child, .next_sibling = layout_next_sibling,
		.parent = layout_parent, .widget_count = win->widget_count,
		.has_scroll = 1, .widget_stride = sizeof(struct mkgui_widget)
	};
	lc_layout_node(win, &lc, idx);
}

// [=]===^=[ lc_min_size ]======================================[=]
static void lc_min_size(struct mkgui_window *win, struct layout_ctx *lc, uint32_t idx, int32_t *out_w, int32_t *out_h) {
	struct mkgui_widget *w = lw_get(lc, idx);
	uint32_t t = w->type;

	if(((MKGUI_HAS_VBOX && t == MKGUI_VBOX) || (MKGUI_HAS_HBOX && t == MKGUI_HBOX) || (MKGUI_HAS_PANEL && t == MKGUI_PANEL)) && (w->flags & MKGUI_SCROLL)) {
		*out_w = 100;
		*out_h = win->row_height * 3;
		return;

	}

	if((MKGUI_HAS_VBOX && t == MKGUI_VBOX) || (MKGUI_HAS_HBOX && t == MKGUI_HBOX) || (MKGUI_HAS_FORM && t == MKGUI_FORM) || (MKGUI_HAS_GROUP && t == MKGUI_GROUP) || (MKGUI_HAS_PANEL && t == MKGUI_PANEL)) {
		*out_w = lc_measure_container(win, lc, idx, 0);
		*out_h = lc_measure_container(win, lc, idx, 1);
		return;

	}

	if((MKGUI_HAS_TABS && t == MKGUI_TABS)) {
		int32_t max_tw = 0;
		int32_t max_th = 0;

		for(uint32_t c = lc->first_child[idx]; c < lc->widget_count; c = lc->next_sibling[c]) {
			if((MKGUI_HAS_TAB && lw_get(lc, c)->type == MKGUI_TAB)) {
				int32_t tw = 0;
				int32_t th = 0;

				for(uint32_t gc = lc->first_child[c]; gc < lc->widget_count; gc = lc->next_sibling[gc]) {
					int32_t cw, ch;
					lc_min_size(win, lc, gc, &cw, &ch);

					if(cw > tw) {
						tw = cw;

					}

					th += ch;

				}

				if(tw > max_tw) {
					max_tw = tw;

				}

				if(th > max_th) {
					max_th = th;

				}
			}
		}

		int32_t tab_inset = sc(win, MKGUI_TAB_INSET);
		*out_w = max_tw + tab_inset * 2;
		*out_h = win->tab_height + tab_inset * 2 + max_th;
		return;

	}

	if((MKGUI_HAS_HSPLIT && t == MKGUI_HSPLIT)) {
		int32_t max_w = 0;
		int32_t total_h = win->split_thick;

		for(uint32_t c = lc->first_child[idx]; c < lc->widget_count; c = lc->next_sibling[c]) {
			int32_t cw, ch;
			lc_min_size(win, lc, c, &cw, &ch);

			if(cw > max_w) {
				max_w = cw;

			}

			total_h += ch;

		}

		*out_w = max_w;
		*out_h = total_h;
		return;

	}

	if((MKGUI_HAS_VSPLIT && t == MKGUI_VSPLIT)) {
		int32_t total_w = win->split_thick;
		int32_t max_h = 0;

		for(uint32_t c = lc->first_child[idx]; c < lc->widget_count; c = lc->next_sibling[c]) {
			int32_t cw, ch;
			lc_min_size(win, lc, c, &cw, &ch);
			total_w += cw;

			if(ch > max_h) {
				max_h = ch;

			}
		}

		*out_w = total_w;
		*out_h = max_h;
		return;

	}

	int32_t nh = natural_height(win, t);
	*out_w = lw_sw(win, w->w) > 0 ? lw_sw(win, w->w) : natural_width(win, w);
	*out_h = lw_sw(win, w->h) > 0 ? lw_sw(win, w->h) : (nh > 0 ? nh : 0);

}

// [=]===^=[ lc_window_min_size ]================================[=]
static void lc_window_min_size(struct mkgui_window *win, struct layout_ctx *lc, uint32_t win_idx, int32_t *out_w, int32_t *out_h) {
	int32_t min_w = 0;
	int32_t min_h = 0;

	for(uint32_t c = lc->first_child[win_idx]; c < lc->widget_count; c = lc->next_sibling[c]) {
		struct mkgui_widget *child = lw_get(lc, c);
		uint32_t type = child->type;

		if(MKGUI_HAS_MENU && type == MKGUI_MENU) {
			min_h += win->menu_height;

		} else if(MKGUI_HAS_TOOLBAR && type == MKGUI_TOOLBAR) {
			min_h += win->font_height + 10;

		} else if(MKGUI_HAS_STATUSBAR && type == MKGUI_STATUSBAR) {
			min_h += win->statusbar_height;

		} else if(MKGUI_HAS_PATHBAR && type == MKGUI_PATHBAR && child->h == 0) {
			min_h += win->pathbar_height;

		} else {
			int32_t child_w, child_h;
			lc_min_size(win, lc, c, &child_w, &child_h);
			min_h += child_h;

			if(child_w > min_w) {
				min_w = child_w;

			}
		}
	}

	*out_w = min_w + win->box_pad * 2;
	*out_h = min_h + win->box_pad * 2;

}

// [=]===^=[ layout_compute_min_size ]==============================[=]
static void layout_compute_min_size(struct mkgui_window *win, uint32_t win_idx) {
	struct layout_ctx lc = {
		.widgets = win->widgets, .first_child = layout_first_child, .next_sibling = layout_next_sibling,
		.parent = layout_parent, .widget_count = win->widget_count, .has_scroll = 1, .widget_stride = sizeof(struct mkgui_widget)
	};
	int32_t min_w, min_h;
	int32_t floor_w = sc(win, MKGUI_WIN_MIN_W) + win->box_pad * 2;
	int32_t floor_h = sc(win, MKGUI_WIN_MIN_H) + win->box_pad * 2;
	lc_window_min_size(win, &lc, win_idx, &min_w, &min_h);

	if(min_w < floor_w) {
		min_w = floor_w;

	}

	if(min_h < floor_h) {
		min_h = floor_h;

	}

	mkwin_set_min_size(win->mkwin, min_w, min_h);

}

// [=]===^=[ layout_widgets ]====================================[=]
static void layout_widgets(struct mkgui_window *win) {
	layout_build_index(win);
	for(uint32_t i = 0; i < win->widget_count; ++i) {
		if((MKGUI_HAS_WINDOW && win->widgets[i].type == MKGUI_WINDOW)) {
			win->rects[i].x = 0;
			win->rects[i].y = 0;
			win->rects[i].w = win->win_w;
			win->rects[i].h = win->win_h;
			layout_node(win, i);
			if(win->dirty_full && !win->canvas_window) {
				layout_compute_min_size(win, i);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Hit testing
// ---------------------------------------------------------------------------

// [=]===^=[ hit_test ]==========================================[=]
static int32_t hit_test(struct mkgui_window *win, int32_t mx, int32_t my) {
	for(int32_t i = (int32_t)win->widget_count - 1; i >= 0; --i) {
		if(!widget_visible(win, (uint32_t)i)) {
			continue;
		}
		struct mkgui_widget *w = &win->widgets[i];
		if((MKGUI_HAS_WINDOW && w->type == MKGUI_WINDOW) || (MKGUI_HAS_TAB && w->type == MKGUI_TAB) || (MKGUI_HAS_PANEL && w->type == MKGUI_PANEL) || (MKGUI_HAS_SPINNER && w->type == MKGUI_SPINNER) || (MKGUI_HAS_GLVIEW && w->type == MKGUI_GLVIEW) || (MKGUI_HAS_VBOX && w->type == MKGUI_VBOX) || (MKGUI_HAS_HBOX && w->type == MKGUI_HBOX) || (MKGUI_HAS_FORM && w->type == MKGUI_FORM) || (MKGUI_HAS_DIVIDER && w->type == MKGUI_DIVIDER) || (MKGUI_HAS_TOOLBAR && w->type == MKGUI_TOOLBAR)) {
			continue;
		}

		if((MKGUI_HAS_GROUP && w->type == MKGUI_GROUP)) {
			int32_t rx = win->rects[i].x;
			int32_t ry = win->rects[i].y;
			int32_t rw = win->rects[i].w;
			int32_t header_h = win->font_height + 4;
			if(mx >= rx && mx < rx + rw && my >= ry && my < ry + header_h) {
				return i;
			}
			continue;
		}

		if((MKGUI_HAS_LABEL && w->type == MKGUI_LABEL) && !(w->style & MKGUI_LABEL_LINK)) {
			continue;
		}

		if((MKGUI_HAS_HSPLIT && w->type == MKGUI_HSPLIT) || (MKGUI_HAS_VSPLIT && w->type == MKGUI_VSPLIT)) {
			int32_t rx = win->rects[i].x;
			int32_t ry = win->rects[i].y;
			int32_t rw = win->rects[i].w;
			int32_t rh = win->rects[i].h;
			struct mkgui_split_data *sd = find_split_data(win, w->id);
			float ratio = sd ? sd->ratio : 0.5f;
			if((MKGUI_HAS_HSPLIT && w->type == MKGUI_HSPLIT)) {
				int32_t split_y = ry + (int32_t)(rh * ratio);
				if(mx >= rx && mx < rx + rw && my >= split_y && my < split_y + win->split_thick) {
					return i;
				}
			} else {
				int32_t split_x = rx + (int32_t)(rw * ratio);
				if(mx >= split_x && mx < split_x + win->split_thick && my >= ry && my < ry + rh) {
					return i;
				}
			}
			continue;
		}
		int32_t rx = win->rects[i].x;
		int32_t ry = win->rects[i].y;
		int32_t rw = win->rects[i].w;
		int32_t rh = win->rects[i].h;
		if(mx >= rx && mx < rx + rw && my >= ry && my < ry + rh) {
			return i;
		}
	}
	return -1;
}

// ---------------------------------------------------------------------------
// Window registry
// ---------------------------------------------------------------------------

// Process-global ctx pointer so platform callbacks (X11 router, Win32 WndProc)
// reach the window registry. Points at ctx->windows[], not a duplicate.
static struct mkgui_ctx *app_ctx;

// ---------------------------------------------------------------------------
// Platform backend
// ---------------------------------------------------------------------------

static void mkgui_flush(struct mkgui_window *win);
static void mkgui_resize_render_impl(struct mkgui_window *win);
static void mkgui_resize_render(struct mkgui_window *win) {
	mkgui_resize_render_impl(win);
}

// celledit_begin lives in mkgui_celledit.c (included after the list/tree
// widgets it depends on), but those widgets call it: a genuine circular
// dependency, so it is forward-declared here.
static void celledit_begin(struct mkgui_window *win, uint32_t widget_id, int32_t row, int32_t col, const char *text);

#include "mkgui_drop.c"

#ifdef _WIN32
#include "mkwin_win32.c"
#else
#include "mkwin_linux.c"
#endif

// ---------------------------------------------------------------------------
// Font: mkwin rasterizes one glyph at a time; the atlas/staging policy is ours.
// ---------------------------------------------------------------------------

static struct mkwin_font *mkgui_font;

// [=]===^=[ mkgui_font_load_glyphs ]============================================================[=]
static void mkgui_font_load_glyphs(struct mkgui_window *win) {
	struct vm_arena replacement = {0};
	struct mkgui_glyph glyphs[MKGUI_GLYPH_COUNT] = {0};
	size_t total = 0;

	for(uint32_t character = MKGUI_GLYPH_FIRST; character <= MKGUI_GLYPH_LAST; ++character) {
		struct mkgui_glyph *glyph = &glyphs[character - MKGUI_GLYPH_FIRST];
		struct mkwin_glyph_bitmap bitmap;

		if(!mkwin_font_glyph(mkgui_font, character, &bitmap)) {
			glyph->advance = sc(win, 7);
			continue;

		}

		glyph->bearing_x = bitmap.bearing_x;
		glyph->bearing_y = bitmap.bearing_y;
		glyph->advance = bitmap.advance ? bitmap.advance : sc(win, 7);

		if(bitmap.coverage && bitmap.width > 0 && bitmap.height > 0) {
			size_t pixels = (size_t)bitmap.width * (size_t)bitmap.height;

			if(pixels > UINT32_MAX - total || !vm_arena_grow(&replacement, total + pixels)) {
				vm_arena_destroy(&replacement);
				return;

			}

			glyph->width = bitmap.width;
			glyph->height = bitmap.height;
			glyph->atlas_offset = (uint32_t)total;
			memcpy(replacement.base + total, bitmap.coverage, pixels);
			total += pixels;

		}
	}

	vm_arena_destroy(&glyph_arena);
	glyph_arena = replacement;
	glyph_atlas = glyph_arena.base;
	memcpy(win->glyphs, glyphs, sizeof(glyphs));
	win->char_width = win->glyphs['M' - MKGUI_GLYPH_FIRST].advance;

	if(win->char_width == 0) {
		win->char_width = sc(win, 7);

	}
}

// [=]===^=[ mkgui_font_fallback ]=================================[=]
static void mkgui_font_fallback(struct mkgui_window *win) {
	win->font_ascent = sc(win, 11);
	win->font_height = sc(win, 13);
	win->char_width = sc(win, 7);
}

// [=]===^=[ mkgui_font_init ]=====================================[=]
static void mkgui_font_init(struct mkgui_window *win) {
	int32_t ascent = 0, height = 0;
	mkgui_font = mkwin_font_open(NULL, (int32_t)(13.0f * win->scale + 0.5f), &ascent, &height);
	if(!mkgui_font) {
		mkgui_font_fallback(win);
		return;
	}
	win->font_ascent = ascent;
	win->font_height = height;
	mkgui_font_load_glyphs(win);
}

// [=]===^=[ mkgui_font_set_size ]=================================[=]
static void mkgui_font_set_size(struct mkgui_window *win, int32_t pixel_size) {
	if(!mkgui_font) {
		mkgui_font_fallback(win);
		return;
	}
	int32_t ascent = 0, height = 0;
	mkwin_font_set_size(mkgui_font, pixel_size, &ascent, &height);
	win->font_ascent = ascent;
	win->font_height = height;
	mkgui_font_load_glyphs(win);
}

// [=]===^=[ mkgui_font_fini ]=====================================[=]
static void mkgui_font_fini(struct mkgui_window *win) {
	(void)win;
	mkwin_font_close(mkgui_font);
	mkgui_font = NULL;
	vm_arena_destroy(&glyph_arena);
	glyph_atlas = NULL;

}

#ifdef _WIN32
// [=]===^=[ timer_due ]========================================================================[=]
static uint64_t timer_due(struct mkgui_timer *timer) {
	uint64_t ticks = timer->interval_ns / 100 + (timer->interval_ns % 100 != 0);
	return (uint64_t)timer->epoch.QuadPart + (timer->fire_count + 1) * ticks;

}
#endif

// [=]===^=[ mkgui_modal_frame ]===================================[=]
// mkwin calls this each frame of a Win32 modal move/resize loop, while our
// event loop is suspended: advance animation, service timers, and repaint.
static void mkgui_modal_frame(struct mkwin_window *mkwin_win, void *user) {
	(void)mkwin_win;
	struct mkgui_window *win = (struct mkgui_window *)user;
#ifdef _WIN32
	int64_t now;
	QueryPerformanceCounter((LARGE_INTEGER *)&now);
	double dt = (double)(now - win->anim_prev) / (double)win->perf_freq;
	win->anim_prev = now;
	win->anim_time += dt;
	for(uint32_t ti = 0; ti < win->timer_count; ++ti) {
		struct mkgui_timer *t = &win->timers[ti];
		if(t->active && t->cb && t->handle && WaitForSingleObject(t->handle, 0) == WAIT_OBJECT_0) {
			++t->fire_count;
			LARGE_INTEGER next;
			next.QuadPart = (int64_t)timer_due(t);
			SetWaitableTimer(t->handle, &next, 0, NULL, NULL, FALSE);
			t->cb(win, t->id, t->userdata);
		}
	}
#endif
	mkgui_resize_render(win);
}

// [=]===^=[ mkgui_drop_enable ]=================================[=]
MKGUI_API void mkgui_drop_enable(struct mkgui_window *win) {
	MKGUI_CHECK(win);
	if(win->drop_enabled) {
		return;
	}
	win->drop_enabled = 1;
	mkwin_drop_enable(win->mkwin);
}

// ---------------------------------------------------------------------------
// Popup windows (override-redirect)
// ---------------------------------------------------------------------------

// [=]===^=[ popup_create ]======================================[=]
static struct mkgui_popup *popup_create(struct mkgui_window *win, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t widget_id) {
	if(win->popup_count >= MKGUI_MAX_POPUPS) {
		return NULL;
	}
	struct mkgui_popup *p = &win->popups[win->popup_count];
	memset(p, 0, sizeof(*p));

	p->mkwin = mkwin_popup_open(win->mkwin, x, y, w, h);
	if(!p->mkwin) {
		return NULL;
	}
	p->pixels = mkwin_popup_framebuffer(p->mkwin);

	p->x = x;
	p->y = y;
	p->w = w;
	p->h = h;
	p->widget_id = widget_id;
	p->active = 1;
	p->dirty = 1;
	p->hover_item = -1;

	++win->popup_count;
	return p;
}

// [=]===^=[ popup_destroy ]=====================================[=]
static void popup_destroy(struct mkgui_window *win, uint32_t popup_idx) {
	if(popup_idx >= win->popup_count) {
		return;
	}
	struct mkgui_popup *p = &win->popups[popup_idx];
	mkwin_popup_close(p->mkwin);
	p->mkwin = NULL;
	p->active = 0;

	for(uint32_t i = popup_idx; i < win->popup_count - 1; ++i) {
		win->popups[i] = win->popups[i + 1];
	}
	--win->popup_count;
}

// [=]===^=[ popup_destroy_all ]=================================[=]
static void popup_destroy_all(struct mkgui_window *win) {
	while(win->popup_count > 0) {
		popup_destroy(win, 0);
	}
}

// [=]===^=[ popup_destroy_from ]================================[=]
static void popup_destroy_from(struct mkgui_window *win, uint32_t from_idx) {
	while(win->popup_count > from_idx) {
		popup_destroy(win, win->popup_count - 1);
	}
}

// ---------------------------------------------------------------------------
// Icon drawing
// ---------------------------------------------------------------------------

// [=]===^=[ draw_icon ]===========================================[=]
static void draw_icon(uint32_t *buf, int32_t bw, int32_t bh, struct mkgui_icon *icon, int32_t x, int32_t y, int32_t cx1, int32_t cy1, int32_t cx2, int32_t cy2) {
	int32_t row0 = 0;
	int32_t row1 = icon->h;
	int32_t col0 = 0;
	int32_t col1 = icon->w;
	if(y + row0 < cy1) {
		row0 = cy1 - y;
	}

	if(y + row0 < 0) {
		row0 = -y;
	}

	if(y + row1 > cy2) {
		row1 = cy2 - y;
	}

	if(y + row1 > bh) {
		row1 = bh - y;
	}

	if(x + col0 < cx1) {
		col0 = cx1 - x;
	}

	if(x + col0 < 0) {
		col0 = -x;
	}

	if(x + col1 > cx2) {
		col1 = cx2 - x;
	}

	if(x + col1 > bw) {
		col1 = bw - x;
	}
	for(int32_t row = row0; row < row1; ++row) {
		blend_icon_row(&buf[(y + row) * bw], &icon_atlas[icon->atlas_offset + (uint32_t)row * (uint32_t)icon->w], col0, col1, x);
	}
}

// [=]===^=[ draw_icon_popup ]=====================================[=]
static void draw_icon_popup(struct mkgui_popup *p, struct mkgui_icon *icon, int32_t x, int32_t y) {
	draw_icon(p->pixels, p->w, p->h, icon, x, y, 0, 0, p->w, p->h);
}

// ---------------------------------------------------------------------------
// PlutoVG + PlutoSVG (SVG parsing + rasterization)
// ---------------------------------------------------------------------------

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
#define PLUTOSVG_BUILD_STATIC
#define PLUTOSVG_BUILD
#define PLUTOVG_BUILD_STATIC
#define PLUTOVG_BUILD
#define PLUTOVG_NO_IMAGE_IO
#define PLUTOVG_NO_FONT
#include "plutovg/plutovg.h"
plutovg_surface_t *plutovg_surface_load_from_image_base64(const char *d, int l) { (void)d; (void)l; return NULL; }
plutovg_surface_t *plutovg_surface_load_from_image_file(const char *f) { (void)f; return NULL; }
plutovg_surface_t *plutovg_surface_load_from_image_data(const void *d, int l) { (void)d; (void)l; return NULL; }
#include "plutovg/plutovg-private.h"
#include "plutovg/plutovg-utils.h"
#include "plutovg/plutovg-ft-math.c"
#include "plutovg/plutovg-ft-raster.c"
#include "plutovg/plutovg-ft-stroker.c"
#include "plutovg/plutovg-blend.c"
#include "plutovg/plutovg-canvas.c"
#include "plutovg/plutovg-matrix.c"
#include "plutovg/plutovg-paint.c"
#include "plutovg/plutovg-path.c"
#include "plutovg/plutovg-rasterize.c"
#include "plutovg/plutovg-surface.c"
#undef MAX_NAME
#include "plutosvg/plutosvg.h"
#include "plutosvg/plutosvg.c"
#pragma GCC diagnostic pop

// ---------------------------------------------------------------------------
// Filesystem abstraction (used by icon, iconbrowser and filedialog)
// ---------------------------------------------------------------------------

#include "mkgui_fs.c"

// ---------------------------------------------------------------------------
// Widget implementations
// ---------------------------------------------------------------------------

#include "mkgui_icon.c"
#include "mkgui_button.c"
#include "mkgui_label.c"
#include "mkgui_textedit.c"
#include "mkgui_undo.c"
#include "mkgui_input.c"
#include "mkgui_checkbox.c"
#include "mkgui_dropdown.c"
#include "mkgui_slider.c"
#include "mkgui_listview.c"
#include "mkgui_tabs.c"
#include "mkgui_radio.c"
#include "mkgui_accel.c"
#include "mkgui_menu.c"
#include "mkgui_ctxmenu.c"
#include "mkgui_split.c"
#include "mkgui_treeview.c"
#include "mkgui_treelist.c"
#include "mkgui_statusbar.c"
#include "mkgui_toolbar.c"
#include "mkgui_pathbar.c"
#include "mkgui_spinbox.c"
#include "mkgui_progress.c"
#include "mkgui_meter.c"
#include "mkgui_textarea.c"
#include "mkgui_logview.c"
#include "mkgui_group.c"
#include "mkgui_panel.c"
#include "mkgui_spacer.c"
#include "mkgui_divider.c"
#include "mkgui_ipinput.c"
#include "mkgui_toggle.c"
#include "mkgui_combobox.c"
#include "mkgui_datepicker.c"
#include "mkgui_gridview.c"
#include "mkgui_richlist.c"
#include "mkgui_spinner.c"
#include "mkgui_itemview.c"
#include "mkgui_scrollbar.c"
#include "mkgui_image.c"
#include "mkgui_glview.c"
#include "mkgui_canvas.c"
#include "mkgui_celledit.c"
#include "mkgui_tooltip.c"
#include "mkgui_notify.c"

// ---------------------------------------------------------------------------
// Render dispatch
// ---------------------------------------------------------------------------

// [=]===^=[ render_widget ]=====================================[=]
// [=]===^=[ render_box ]========================================[=]
// Optional border (as a panel), plus a scrollbar on any overflowing axis when MKGUI_SCROLL is set.
static void render_box(struct mkgui_window *win, uint32_t idx) {
if(win->widgets[idx].style & STYLE_BORDER_BIT) {
	render_panel(win, idx);
}

if((win->widgets[idx].flags & MKGUI_SCROLL)) {
	struct mkgui_box_scroll *bs = find_box_scroll(win, win->widgets[idx].id);
	int32_t rx = win->rects[idx].x;
	int32_t ry = win->rects[idx].y;
	int32_t rw = win->rects[idx].w;
	int32_t rh = win->rects[idx].h;
	int32_t r = win->theme.corner_radius;
	if(bs && bs->content_h > rh) {
		int32_t sx = rx + rw - win->scrollbar_w;
		draw_rect_fill(win->pixels, win->win_w, win->win_h, sx, ry, win->scrollbar_w, rh, win->theme.scrollbar_bg);
		int32_t track = rh;
		int32_t total = bs->content_h;
		if(total < 1) {
			total = 1;
		}
		int32_t thumb = track * rh / total;
		if(thumb < sc(win, 20)) {
			thumb = sc(win, 20);
		}

		if(thumb > track) {
			thumb = track;
		}
		int32_t max_scroll = bs->content_h - rh;
		if(max_scroll < 1) {
			max_scroll = 1;
		}
		int32_t pos = (int32_t)((int64_t)bs->scroll_y * (track - thumb) / max_scroll);
		if(pos < 0) {
			pos = 0;
		}

		if(pos > track - thumb) {
			pos = track - thumb;
		}
		draw_rounded_rect_fill(win->pixels, win->win_w, win->win_h, sx + 2, ry + pos, win->scrollbar_w - 4, thumb, win->theme.scrollbar_thumb, r);
	}

	if(bs && bs->content_w > rw) {
		int32_t sy = ry + rh - win->scrollbar_w;
		draw_rect_fill(win->pixels, win->win_w, win->win_h, rx, sy, rw, win->scrollbar_w, win->theme.scrollbar_bg);
		int32_t track = rw;
		int32_t total = bs->content_w;
		if(total < 1) {
			total = 1;
		}
		int32_t thumb = track * rw / total;
		if(thumb < sc(win, 20)) {
			thumb = sc(win, 20);
		}

		if(thumb > track) {
			thumb = track;
		}
		int32_t max_scroll = bs->content_w - rw;
		if(max_scroll < 1) {
			max_scroll = 1;
		}
		int32_t pos = (int32_t)((int64_t)bs->scroll_x * (track - thumb) / max_scroll);
		if(pos < 0) {
			pos = 0;
		}

		if(pos > track - thumb) {
			pos = track - thumb;
		}
		draw_rounded_rect_fill(win->pixels, win->win_w, win->win_h, rx + pos, sy + 2, thumb, win->scrollbar_w - 4, win->theme.scrollbar_thumb, r);
	}
}
}

// Per-type render dispatch, indexed by the widget type enum. fn == NULL means
// the type paints nothing here (WINDOW/MENUITEM/TAB/SPACER). cell_overlay draws
// the active inline cell editor on top after the widget is drawn.
struct mkgui_render_desc {
	void (*fn)(struct mkgui_window *win, uint32_t idx);
	uint8_t cell_overlay;
};

static struct mkgui_render_desc render_desc[MKGUI_WIDGET_COUNT] = {
#if MKGUI_HAS_BUTTON
	[MKGUI_BUTTON] = { render_button },
#endif
#if MKGUI_HAS_LABEL
	[MKGUI_LABEL] = { render_label },
#endif
#if MKGUI_HAS_INPUT
	[MKGUI_INPUT] = { render_input },
#endif
#if MKGUI_HAS_CHECKBOX
	[MKGUI_CHECKBOX] = { render_checkbox },
#endif
#if MKGUI_HAS_SLIDER
	[MKGUI_SLIDER] = { render_slider },
#endif
#if MKGUI_HAS_DROPDOWN
	[MKGUI_DROPDOWN] = { render_dropdown },
#endif
#if MKGUI_HAS_TABS
	[MKGUI_TABS] = { render_tabs },
#endif
#if MKGUI_HAS_LISTVIEW
	[MKGUI_LISTVIEW] = { render_listview, 1 },
#endif
#if MKGUI_HAS_HSPLIT
	[MKGUI_HSPLIT] = { render_split },
#endif
#if MKGUI_HAS_VSPLIT
	[MKGUI_VSPLIT] = { render_split },
#endif
#if MKGUI_HAS_MENU
	[MKGUI_MENU] = { render_menu_bar },
#endif
#if MKGUI_HAS_TREEVIEW
	[MKGUI_TREEVIEW] = { render_treeview, 1 },
#endif
#if MKGUI_HAS_TREELIST
	[MKGUI_TREELIST] = { render_treelist },
#endif
#if MKGUI_HAS_STATUSBAR
	[MKGUI_STATUSBAR] = { render_statusbar },
#endif
#if MKGUI_HAS_TOOLBAR
	[MKGUI_TOOLBAR] = { render_toolbar },
#endif
#if MKGUI_HAS_SPINBOX
	[MKGUI_SPINBOX] = { render_spinbox },
#endif
#if MKGUI_HAS_RADIO
	[MKGUI_RADIO] = { render_radio },
#endif
#if MKGUI_HAS_PROGRESS
	[MKGUI_PROGRESS] = { render_progress },
#endif
#if MKGUI_HAS_METER
	[MKGUI_METER] = { render_meter },
#endif
#if MKGUI_HAS_TEXTAREA
	[MKGUI_TEXTAREA] = { render_textarea },
#endif
#if MKGUI_HAS_LOGVIEW
	[MKGUI_LOGVIEW] = { render_logview },
#endif
#if MKGUI_HAS_GROUP
	[MKGUI_GROUP] = { render_group },
#endif
#if MKGUI_HAS_SPINNER
	[MKGUI_SPINNER] = { render_spinner },
#endif
#if MKGUI_HAS_ITEMVIEW
	[MKGUI_ITEMVIEW] = { render_itemview, 1 },
#endif
#if MKGUI_HAS_PANEL
	[MKGUI_PANEL] = { render_panel },
#endif
#if MKGUI_HAS_SCROLLBAR
	[MKGUI_SCROLLBAR] = { render_scrollbar },
#endif
#if MKGUI_HAS_IMAGE
	[MKGUI_IMAGE] = { render_image },
#endif
#if MKGUI_HAS_GLVIEW
	[MKGUI_GLVIEW] = { render_glview },
#endif
#if MKGUI_HAS_CANVAS
	[MKGUI_CANVAS] = { render_canvas },
#endif
#if MKGUI_HAS_PATHBAR
	[MKGUI_PATHBAR] = { render_pathbar },
#endif
#if MKGUI_HAS_IPINPUT
	[MKGUI_IPINPUT] = { render_ipinput },
#endif
#if MKGUI_HAS_TOGGLE
	[MKGUI_TOGGLE] = { render_toggle },
#endif
#if MKGUI_HAS_COMBOBOX
	[MKGUI_COMBOBOX] = { render_combobox },
#endif
#if MKGUI_HAS_DATEPICKER
	[MKGUI_DATEPICKER] = { render_datepicker },
#endif
#if MKGUI_HAS_GRIDVIEW
	[MKGUI_GRIDVIEW] = { render_gridview, 1 },
#endif
#if MKGUI_HAS_RICHLIST
	[MKGUI_RICHLIST] = { render_richlist },
#endif
#if MKGUI_HAS_DIVIDER
	[MKGUI_DIVIDER] = { render_divider },
#endif
#if MKGUI_HAS_VBOX
	[MKGUI_VBOX] = { render_box },
#endif
#if MKGUI_HAS_HBOX
	[MKGUI_HBOX] = { render_box },
#endif
#if MKGUI_HAS_FORM
	[MKGUI_FORM] = { render_box },
#endif
};

// [=]===^=[ render_widget ]=====================================[=]
static void render_widget(struct mkgui_window *win, uint32_t idx) {
	uint32_t type = win->widgets[idx].type;
	// A button inside a toolbar is painted by the toolbar itself, not here.
	if((MKGUI_HAS_BUTTON && type == MKGUI_BUTTON)) {
		uint32_t pidx = layout_parent[idx];
		if(pidx < win->widget_count && (MKGUI_HAS_TOOLBAR && win->widgets[pidx].type == MKGUI_TOOLBAR)) {
			return;
		}
	}
	if(!render_desc[type].fn) {
		return;
	}
	render_desc[type].fn(win, idx);
	if(MKGUI_HAS_CELLEDIT && render_desc[type].cell_overlay) {
		celledit_render(win, idx);
	}
}

// [=]===^=[ set_parent_clip ]====================================[=]
static void set_parent_clip(struct mkgui_window *win, uint32_t idx) {
	render_clip_x1 = render_base_clip_x1;
	render_clip_y1 = render_base_clip_y1;
	render_clip_x2 = render_base_clip_x2;
	render_clip_y2 = render_base_clip_y2;
	uint32_t pidx = layout_parent[idx];
	while(pidx < win->widget_count && (!MKGUI_HAS_WINDOW || win->widgets[pidx].type != MKGUI_WINDOW)) {
		int32_t px = win->rects[pidx].x;
		int32_t py = win->rects[pidx].y;
		int32_t px2 = px + win->rects[pidx].w;
		int32_t py2 = py + win->rects[pidx].h;
		if(px > render_clip_x1) {
			render_clip_x1 = px;
		}

		if(py > render_clip_y1) {
			render_clip_y1 = py;
		}

		if(px2 < render_clip_x2) {
			render_clip_x2 = px2;
		}

		if(py2 < render_clip_y2) {
			render_clip_y2 = py2;
		}
		pidx = layout_parent[pidx];
	}
}

// [=]===^=[ render_widgets ]====================================[=]
static void render_widgets(struct mkgui_window *win) {
	text_cmd_count = 0;

	if(win->canvas_window) {
		render_clip_x1 = 0;
		render_clip_y1 = 0;
		render_clip_x2 = win->win_w;
		render_clip_y2 = win->win_h;
		for(uint32_t i = 0; (MKGUI_HAS_CANVAS) && i < win->canvas_count; ++i) {
			struct mkgui_canvas_data *cd = &win->canvases[i];
			if(cd->callback) {
				cd->callback(win, cd->widget_id, win->pixels, 0, 0, win->win_w, win->win_h, cd->userdata);
				break;
			}
		}
		win->dirty_full = 1;
		return;
	}

	if(win->dirty_full || win->render_cb) {
		render_base_clip_x1 = 0;
		render_base_clip_y1 = 0;
		render_base_clip_x2 = win->win_w;
		render_base_clip_y2 = win->win_h;
		render_clip_x1 = 0;
		render_clip_y1 = 0;
		render_clip_x2 = win->win_w;
		render_clip_y2 = win->win_h;
		draw_rect_fill(win->pixels, win->win_w, win->win_h, 0, 0, win->win_w, win->win_h, win->theme.bg);
		for(uint32_t i = 0; i < win->widget_count; ++i) {
			if(!widget_visible(win, i)) {
				continue;
			}
			set_parent_clip(win, i);
			render_widget(win, i);
		}
	} else {
		for(uint32_t d = 0; d < win->dirty_count; ++d) {
			int32_t dx = win->dirty_rects[d].x;
			int32_t dy = win->dirty_rects[d].y;
			int32_t dw = win->dirty_rects[d].w;
			int32_t dh = win->dirty_rects[d].h;
			render_base_clip_x1 = dx;
			render_base_clip_y1 = dy;
			render_base_clip_x2 = dx + dw;
			render_base_clip_y2 = dy + dh;
			render_clip_x1 = dx;
			render_clip_y1 = dy;
			render_clip_x2 = dx + dw;
			render_clip_y2 = dy + dh;
			draw_rect_fill(win->pixels, win->win_w, win->win_h, dx, dy, dw, dh, win->theme.bg);
			for(uint32_t i = 0; i < win->widget_count; ++i) {
				int32_t wx = win->rects[i].x;
				int32_t wy = win->rects[i].y;
				int32_t wr = wx + win->rects[i].w;
				int32_t wb = wy + win->rects[i].h;
				if(wx < dx + dw && wr > dx && wy < dy + dh && wb > dy && widget_visible(win, i)) {
					set_parent_clip(win, i);
					render_widget(win, i);
				}
			}

			flush_text(win);

		}
	}

	render_clip_x1 = 0;
	render_clip_y1 = 0;
	render_clip_x2 = win->win_w;
	render_clip_y2 = win->win_h;
}

// ---------------------------------------------------------------------------
// Init auxiliary data
// ---------------------------------------------------------------------------

// [=]===^=[ init_widget_aux ]====================================[=]
static void init_widget_aux(struct mkgui_window *win, struct mkgui_widget *w) {
	switch(w->type) {
#if MKGUI_HAS_INPUT
		case MKGUI_INPUT: {
			MKGUI_AUX_GROW(&win->arenas.inputs, win->input_count, win->input_cap, struct mkgui_input_data);
			if(win->input_count < win->input_cap) {
				struct mkgui_input_data *inp = &win->inputs[win->input_count++];
				memset(inp, 0, sizeof(*inp));
				inp->widget_id = w->id;
			}
		} break;
#endif

#if MKGUI_HAS_IPINPUT
		case MKGUI_IPINPUT: {
			MKGUI_AUX_GROW(&win->arenas.ipinputs, win->ipinput_count, win->ipinput_cap, struct mkgui_ipinput_data);
			if(win->ipinput_count < win->ipinput_cap) {
				struct mkgui_ipinput_data *ip = &win->ipinputs[win->ipinput_count++];
				memset(ip, 0, sizeof(*ip));
				ip->widget_id = w->id;
			}
		} break;
#endif

#if MKGUI_HAS_TOGGLE
		case MKGUI_TOGGLE: {
			MKGUI_AUX_GROW(&win->arenas.toggles, win->toggle_count, win->toggle_cap, struct mkgui_toggle_data);
			if(win->toggle_count < win->toggle_cap) {
				struct mkgui_toggle_data *td = &win->toggles[win->toggle_count++];
				memset(td, 0, sizeof(*td));
				td->widget_id = w->id;
			}
		} break;
#endif

#if MKGUI_HAS_COMBOBOX
		case MKGUI_COMBOBOX: {
			MKGUI_AUX_GROW(&win->arenas.comboboxes, win->combobox_count, win->combobox_cap, struct mkgui_combobox_data);
			if(win->combobox_count < win->combobox_cap) {
				struct mkgui_combobox_data *cb = &win->comboboxes[win->combobox_count++];
				memset(cb, 0, sizeof(*cb));
				cb->widget_id = w->id;
				cb->selected = -1;
			}
		} break;
#endif

#if MKGUI_HAS_DATEPICKER
		case MKGUI_DATEPICKER: {
			MKGUI_AUX_GROW(&win->arenas.datepickers, win->datepicker_count, win->datepicker_cap, struct mkgui_datepicker_data);
			if(win->datepicker_count < win->datepicker_cap) {
				struct mkgui_datepicker_data *dp = &win->datepickers[win->datepicker_count++];
				memset(dp, 0, sizeof(*dp));
				dp->widget_id = w->id;
				dp->year = 2026;
				dp->month = 1;
				dp->day = 1;
			}
		} break;
#endif

#if MKGUI_HAS_DROPDOWN
		case MKGUI_DROPDOWN: {
			MKGUI_AUX_GROW(&win->arenas.dropdowns, win->dropdown_count, win->dropdown_cap, struct mkgui_dropdown_data);
			if(win->dropdown_count < win->dropdown_cap) {
				struct mkgui_dropdown_data *dd = &win->dropdowns[win->dropdown_count++];
				memset(dd, 0, sizeof(*dd));
				dd->widget_id = w->id;
				dd->selected = -1;
			}
		} break;
#endif

#if MKGUI_HAS_SLIDER
		case MKGUI_SLIDER: {
			MKGUI_AUX_GROW(&win->arenas.sliders, win->slider_count, win->slider_cap, struct mkgui_slider_data);
			if(win->slider_count < win->slider_cap) {
				struct mkgui_slider_data *sd = &win->sliders[win->slider_count++];
				memset(sd, 0, sizeof(*sd));
				sd->widget_id = w->id;
				sd->min_val = 0;
				sd->max_val = 100;
				sd->value = 50;
			}
		} break;
#endif

#if MKGUI_HAS_TABS
		case MKGUI_TABS: {
			MKGUI_AUX_GROW(&win->arenas.tabs, win->tab_count, win->tab_cap, struct mkgui_tabs_data);
			if(win->tab_count < win->tab_cap) {
				struct mkgui_tabs_data *td = &win->tabs[win->tab_count++];
				td->widget_id = w->id;
				td->active_tab = 0;
				for(uint32_t j = 0; j < win->widget_count; ++j) {
					if((MKGUI_HAS_TAB && win->widgets[j].type == MKGUI_TAB) && win->widgets[j].parent_id == w->id) {
						td->active_tab = win->widgets[j].id;
						break;
					}
				}
			}
		} break;
#endif

#if MKGUI_HAS_VBOX || MKGUI_HAS_HBOX
		case MKGUI_VBOX:
		case MKGUI_HBOX: {
			MKGUI_AUX_GROW(&win->arenas.box_scrolls, win->box_scroll_count, win->box_scroll_cap, struct mkgui_box_scroll);
			if(win->box_scroll_count < win->box_scroll_cap) {
				struct mkgui_box_scroll *bs = &win->box_scrolls[win->box_scroll_count++];
				bs->widget_id = w->id;
				bs->scroll_y = 0;
				bs->content_h = 0;
				bs->scroll_x = 0;
				bs->content_w = 0;
			}
		} break;
#endif

#if MKGUI_HAS_HSPLIT || MKGUI_HAS_VSPLIT
		case MKGUI_HSPLIT:
		case MKGUI_VSPLIT: {
			MKGUI_AUX_GROW(&win->arenas.splits, win->split_count, win->split_cap, struct mkgui_split_data);
			if(win->split_count < win->split_cap) {
				struct mkgui_split_data *sd = &win->splits[win->split_count++];
				sd->widget_id = w->id;
				sd->ratio = 0.5f;
			}
		} break;
#endif

#if MKGUI_HAS_TREEVIEW
		case MKGUI_TREEVIEW: {
			MKGUI_AUX_GROW(&win->arenas.treeviews, win->treeview_count, win->treeview_cap, struct mkgui_treeview_data);
			if(win->treeview_count < win->treeview_cap) {
				struct mkgui_treeview_data *tv = &win->treeviews[win->treeview_count++];
				memset(tv, 0, sizeof(*tv));
				tv->widget_id = w->id;
				tv->selected_node = -1;
			}
		} break;
#endif

#if MKGUI_HAS_TREELIST
		case MKGUI_TREELIST: {
			MKGUI_AUX_GROW(&win->arenas.treelists, win->treelist_count, win->treelist_cap, struct mkgui_treelist_data);
			if(win->treelist_count < win->treelist_cap) {
				struct mkgui_treelist_data *tl = &win->treelists[win->treelist_count++];
				memset(tl, 0, sizeof(*tl));
				tl->widget_id = w->id;
				tl->selected = -1;
				tl->sort_col = -1;
				tl->sort_dir = 1;
				tl->root_first = 0xffffffffu;
				tl->root_last = 0xffffffffu;
				tl->dirty_visible = 1;
			}
		} break;
#endif

#if MKGUI_HAS_STATUSBAR
		case MKGUI_STATUSBAR: {
			MKGUI_AUX_GROW(&win->arenas.statusbars, win->statusbar_count, win->statusbar_cap, struct mkgui_statusbar_data);
			if(win->statusbar_count < win->statusbar_cap) {
				struct mkgui_statusbar_data *sb = &win->statusbars[win->statusbar_count++];
				memset(sb, 0, sizeof(*sb));
				sb->widget_id = w->id;
			}
		} break;
#endif

#if MKGUI_HAS_SPINBOX
		case MKGUI_SPINBOX: {
			MKGUI_AUX_GROW(&win->arenas.spinboxes, win->spinbox_count, win->spinbox_cap, struct mkgui_spinbox_data);
			if(win->spinbox_count < win->spinbox_cap) {
				struct mkgui_spinbox_data *sd = &win->spinboxes[win->spinbox_count++];
				memset(sd, 0, sizeof(*sd));
				sd->widget_id = w->id;
				sd->min_val = 0;
				sd->max_val = 100;
				sd->value = 0;
				sd->step = 1;
			}
		} break;
#endif

#if MKGUI_HAS_PROGRESS
		case MKGUI_PROGRESS: {
			MKGUI_AUX_GROW(&win->arenas.progress, win->progress_count, win->progress_cap, struct mkgui_progress_data);
			if(win->progress_count < win->progress_cap) {
				struct mkgui_progress_data *pd = &win->progress[win->progress_count++];
				memset(pd, 0, sizeof(*pd));
				pd->widget_id = w->id;
				pd->max_val = 100;
			}
		} break;
#endif

#if MKGUI_HAS_METER
		case MKGUI_METER: {
			MKGUI_AUX_GROW(&win->arenas.meters, win->meter_count, win->meter_cap, struct mkgui_meter_data);
			if(win->meter_count < win->meter_cap) {
				struct mkgui_meter_data *md = &win->meters[win->meter_count++];
				memset(md, 0, sizeof(*md));
				md->widget_id = w->id;
				md->max_val = 100;
				md->zone_t1 = 75;
				md->zone_t2 = 90;
				md->zone_c1 = 0xff44cc44;
				md->zone_c2 = 0xffffcc00;
				md->zone_c3 = 0xffff4444;
			}
		} break;
#endif

#if MKGUI_HAS_TEXTAREA
		case MKGUI_TEXTAREA: {
			MKGUI_AUX_GROW(&win->arenas.textareas, win->textarea_count, win->textarea_cap, struct mkgui_textarea_data);
			if(win->textarea_count < win->textarea_cap) {
				struct mkgui_textarea_data *ta = &win->textareas[win->textarea_count++];
				memset(ta, 0, sizeof(*ta));
				ta->widget_id = w->id;
				ta->text = (char *)calloc(1, MKGUI_TEXTAREA_INIT_CAP);
				if(!ta->text) {
					--win->textarea_count;
					break;
				}
				ta->text_cap = MKGUI_TEXTAREA_INIT_CAP;
			}
		} break;
#endif

#if MKGUI_HAS_LOGVIEW
		case MKGUI_LOGVIEW: {
			MKGUI_AUX_GROW(&win->arenas.logviews, win->logview_count, win->logview_cap, struct mkgui_logview_data);
			if(win->logview_count < win->logview_cap) {
				struct mkgui_logview_data *lv = &win->logviews[win->logview_count++];
				memset(lv, 0, sizeof(*lv));
				lv->widget_id = w->id;
				lv->cached_width = -1;
				lv->stuck_to_end = 1;
			}
		} break;
#endif

#if MKGUI_HAS_ITEMVIEW
		case MKGUI_ITEMVIEW: {
			MKGUI_AUX_GROW(&win->arenas.itemviews, win->itemview_count, win->itemview_cap, struct mkgui_itemview_data);
			if(win->itemview_count < win->itemview_cap) {
				struct mkgui_itemview_data *iv = &win->itemviews[win->itemview_count++];
				memset(iv, 0, sizeof(*iv));
				iv->widget_id = w->id;
				iv->selected = -1;
			}
		} break;
#endif

#if MKGUI_HAS_CANVAS
		case MKGUI_CANVAS: {
			MKGUI_AUX_GROW(&win->arenas.canvases, win->canvas_count, win->canvas_cap, struct mkgui_canvas_data);
			if(win->canvas_count < win->canvas_cap) {
				struct mkgui_canvas_data *cd = &win->canvases[win->canvas_count++];
				memset(cd, 0, sizeof(*cd));
				cd->widget_id = w->id;
			}
		} break;
#endif

#if MKGUI_HAS_PATHBAR
		case MKGUI_PATHBAR: {
			MKGUI_AUX_GROW(&win->arenas.pathbars, win->pathbar_count, win->pathbar_cap, struct mkgui_pathbar_data);
			if(win->pathbar_count < win->pathbar_cap) {
				struct mkgui_pathbar_data *pb = &win->pathbars[win->pathbar_count++];
				memset(pb, 0, sizeof(*pb));
				pb->widget_id = w->id;
				pb->hover_seg = -1;
			}
		} break;
#endif

		default: {
		} break;
	}
}

// [=]===^=[ init_aux_data ]=====================================[=]
static void init_aux_data(struct mkgui_window *win) {
	for(uint32_t i = 0; i < win->widget_count; ++i) {
		init_widget_aux(win, &win->widgets[i]);
	}
	for(uint32_t i = 0; i < win->widget_count; ++i) {
		struct mkgui_widget *w = &win->widgets[i];
		if((MKGUI_HAS_BUTTON && w->type == MKGUI_BUTTON) && w->label[0] != '\0') {
			struct mkgui_widget *parent = find_widget(win, w->parent_id);
			if(parent && (MKGUI_HAS_TOOLBAR && parent->type == MKGUI_TOOLBAR)) {
				snprintf(win->tooltip_texts[i], MKGUI_MAX_TEXT, "%s", w->label);
			}
		}
	}
}

// [=]===^=[ mkgui_grow_widgets ]=================================[=]
static uint32_t mkgui_grow_widgets(struct mkgui_window *win) {
	uint32_t new_cap = win->widget_cap + MKGUI_GROW_WIDGETS;
	if(!vm_arena_ensure(&win->arenas.widgets, (size_t)new_cap * sizeof(struct mkgui_widget))) {
		return 0;
	}
	if(!vm_arena_ensure(&win->arenas.rects, (size_t)new_cap * sizeof(struct mkgui_rect))) {
		return 0;
	}
	if(!vm_arena_ensure(&win->arenas.tooltip_texts, (size_t)new_cap * MKGUI_MAX_TEXT)) {
		return 0;
	}

	// Keep the id->index hash at >= 2 * widget_cap slots and force a rebuild:
	// growth only happens from an add, which shifts indices anyway.
	uint32_t id_slots = win->id_index_cap;
	while(id_slots < new_cap * 2) {
		id_slots <<= 1;
	}
	if(id_slots != win->id_index_cap) {
		if(!vm_arena_ensure(&win->arenas.id_index, (size_t)id_slots * sizeof(struct mkgui_id_slot))) {
			return 0;
		}
		win->id_index_cap = id_slots;
	}
	win->id_index_dirty = 1;
	win->widget_cap = new_cap;
	return 1;
}

// [=]===^=[ mkgui_widget_type_enabled ]==========================[=]
static uint32_t mkgui_widget_type_enabled(uint32_t type) {
	static uint8_t enabled[MKGUI_WIDGET_COUNT] = {
		[MKGUI_WINDOW] = MKGUI_HAS_WINDOW,
		[MKGUI_BUTTON] = MKGUI_HAS_BUTTON,
		[MKGUI_LABEL] = MKGUI_HAS_LABEL,
		[MKGUI_INPUT] = MKGUI_HAS_INPUT,
		[MKGUI_CHECKBOX] = MKGUI_HAS_CHECKBOX,
		[MKGUI_DROPDOWN] = MKGUI_HAS_DROPDOWN,
		[MKGUI_SLIDER] = MKGUI_HAS_SLIDER,
		[MKGUI_LISTVIEW] = MKGUI_HAS_LISTVIEW,
		[MKGUI_MENU] = MKGUI_HAS_MENU,
		[MKGUI_MENUITEM] = MKGUI_HAS_MENUITEM,
		[MKGUI_TABS] = MKGUI_HAS_TABS,
		[MKGUI_TAB] = MKGUI_HAS_TAB,
		[MKGUI_HSPLIT] = MKGUI_HAS_HSPLIT,
		[MKGUI_VSPLIT] = MKGUI_HAS_VSPLIT,
		[MKGUI_TREEVIEW] = MKGUI_HAS_TREEVIEW,
		[MKGUI_STATUSBAR] = MKGUI_HAS_STATUSBAR,
		[MKGUI_TOOLBAR] = MKGUI_HAS_TOOLBAR,
		[MKGUI_SPINBOX] = MKGUI_HAS_SPINBOX,
		[MKGUI_RADIO] = MKGUI_HAS_RADIO,
		[MKGUI_PROGRESS] = MKGUI_HAS_PROGRESS,
		[MKGUI_TEXTAREA] = MKGUI_HAS_TEXTAREA,
		[MKGUI_GROUP] = MKGUI_HAS_GROUP,
		[MKGUI_SPINNER] = MKGUI_HAS_SPINNER,
		[MKGUI_ITEMVIEW] = MKGUI_HAS_ITEMVIEW,
		[MKGUI_PANEL] = MKGUI_HAS_PANEL,
		[MKGUI_SCROLLBAR] = MKGUI_HAS_SCROLLBAR,
		[MKGUI_IMAGE] = MKGUI_HAS_IMAGE,
		[MKGUI_GLVIEW] = MKGUI_HAS_GLVIEW,
		[MKGUI_CANVAS] = MKGUI_HAS_CANVAS,
		[MKGUI_VBOX] = MKGUI_HAS_VBOX,
		[MKGUI_HBOX] = MKGUI_HAS_HBOX,
		[MKGUI_FORM] = MKGUI_HAS_FORM,
		[MKGUI_SPACER] = MKGUI_HAS_SPACER,
		[MKGUI_PATHBAR] = MKGUI_HAS_PATHBAR,
		[MKGUI_IPINPUT] = MKGUI_HAS_IPINPUT,
		[MKGUI_TOGGLE] = MKGUI_HAS_TOGGLE,
		[MKGUI_COMBOBOX] = MKGUI_HAS_COMBOBOX,
		[MKGUI_DATEPICKER] = MKGUI_HAS_DATEPICKER,
		[MKGUI_GRIDVIEW] = MKGUI_HAS_GRIDVIEW,
		[MKGUI_RICHLIST] = MKGUI_HAS_RICHLIST,
		[MKGUI_METER] = MKGUI_HAS_METER,
		[MKGUI_DIVIDER] = MKGUI_HAS_DIVIDER,
		[MKGUI_LOGVIEW] = MKGUI_HAS_LOGVIEW,
		[MKGUI_TREELIST] = MKGUI_HAS_TREELIST,
	};

	return type < MKGUI_WIDGET_COUNT && enabled[type];

}

// [=]===^=[ mkgui_widget_add ]===================================[=]
MKGUI_API uint32_t mkgui_widget_add(struct mkgui_window *win, struct mkgui_widget w, uint32_t after_id) {
	MKGUI_CHECK_VAL(win, 0);
	if(!mkgui_widget_type_enabled(w.type)) {
		return 0;
	}
	if(win->widget_count >= win->widget_cap) {
		if(!mkgui_grow_widgets(win)) {
			return 0;
		}
	}

	uint32_t pos = win->widget_count;
	if(after_id) {
		for(uint32_t i = 0; i < win->widget_count; ++i) {
			if(win->widgets[i].id == after_id) {
				pos = i + 1;
				break;
			}
		}
	}

	if(pos < win->widget_count) {
		uint32_t tail = win->widget_count - pos;
		memmove(&win->widgets[pos + 1], &win->widgets[pos], tail * sizeof(struct mkgui_widget));
		memmove(&win->rects[pos + 1], &win->rects[pos], tail * sizeof(struct mkgui_rect));
		memmove(&win->tooltip_texts[pos + 1], &win->tooltip_texts[pos], tail * MKGUI_MAX_TEXT);
	}

	win->widgets[pos] = w;
	win->widgets[pos].label_tw = -1;
	memset(&win->rects[pos], 0, sizeof(struct mkgui_rect));
	win->tooltip_texts[pos][0] = '\0';
	++win->widget_count;
	win->id_index_dirty = 1;

	init_widget_aux(win, &win->widgets[pos]);

	if((MKGUI_HAS_BUTTON && w.type == MKGUI_BUTTON) && w.label[0] != '\0') {
		struct mkgui_widget *parent = find_widget(win, w.parent_id);
		if(parent && (MKGUI_HAS_TOOLBAR && parent->type == MKGUI_TOOLBAR)) {
			snprintf(win->tooltip_texts[pos], MKGUI_MAX_TEXT, "%s", w.label);
		}
	}

	if(w.icon[0] != '\0') {
		icon_resolve(win, w.icon);
	}

	dirty_all(win);
	return 1;
}

// [=]===^=[ mkgui_remove_aux_ ]==================================[=]
static void mkgui_remove_aux_(struct mkgui_window *win, uint32_t id, uint32_t type) {
	switch(type) {
#if MKGUI_HAS_INPUT
		case MKGUI_INPUT: {
			for(uint32_t i = 0; (MKGUI_HAS_INPUT) && i < win->input_count; ++i) {
				if(win->inputs[i].widget_id == id) {
					if((MKGUI_HAS_INPUT) && i < win->input_count - 1) {
						win->inputs[i] = win->inputs[win->input_count - 1];
					}
					--win->input_count;
					break;
				}
			}
		} break;
#endif

#if MKGUI_HAS_IPINPUT
		case MKGUI_IPINPUT: {
			for(uint32_t i = 0; (MKGUI_HAS_IPINPUT) && i < win->ipinput_count; ++i) {
				if(win->ipinputs[i].widget_id == id) {
					if((MKGUI_HAS_IPINPUT) && i < win->ipinput_count - 1) {
						win->ipinputs[i] = win->ipinputs[win->ipinput_count - 1];
					}
					--win->ipinput_count;
					break;
				}
			}
		} break;
#endif

#if MKGUI_HAS_TOGGLE
		case MKGUI_TOGGLE: {
			for(uint32_t i = 0; (MKGUI_HAS_TOGGLE) && i < win->toggle_count; ++i) {
				if(win->toggles[i].widget_id == id) {
					if((MKGUI_HAS_TOGGLE) && i < win->toggle_count - 1) {
						win->toggles[i] = win->toggles[win->toggle_count - 1];
					}
					--win->toggle_count;
					break;
				}
			}
		} break;
#endif

#if MKGUI_HAS_COMBOBOX
		case MKGUI_COMBOBOX: {
			for(uint32_t i = 0; (MKGUI_HAS_COMBOBOX) && i < win->combobox_count; ++i) {
				if(win->comboboxes[i].widget_id == id) {
					if((MKGUI_HAS_COMBOBOX) && i < win->combobox_count - 1) {
						win->comboboxes[i] = win->comboboxes[win->combobox_count - 1];
					}
					--win->combobox_count;
					break;
				}
			}
		} break;
#endif

#if MKGUI_HAS_DATEPICKER
		case MKGUI_DATEPICKER: {
			for(uint32_t i = 0; (MKGUI_HAS_DATEPICKER) && i < win->datepicker_count; ++i) {
				if(win->datepickers[i].widget_id == id) {
					if((MKGUI_HAS_DATEPICKER) && i < win->datepicker_count - 1) {
						win->datepickers[i] = win->datepickers[win->datepicker_count - 1];
					}
					--win->datepicker_count;
					break;
				}
			}
		} break;
#endif

#if MKGUI_HAS_DROPDOWN
		case MKGUI_DROPDOWN: {
			for(uint32_t i = 0; (MKGUI_HAS_DROPDOWN) && i < win->dropdown_count; ++i) {
				if(win->dropdowns[i].widget_id == id) {
					if((MKGUI_HAS_DROPDOWN) && i < win->dropdown_count - 1) {
						win->dropdowns[i] = win->dropdowns[win->dropdown_count - 1];
					}
					--win->dropdown_count;
					break;
				}
			}
		} break;
#endif

#if MKGUI_HAS_SLIDER
		case MKGUI_SLIDER: {
			for(uint32_t i = 0; (MKGUI_HAS_SLIDER) && i < win->slider_count; ++i) {
				if(win->sliders[i].widget_id == id) {
					if((MKGUI_HAS_SLIDER) && i < win->slider_count - 1) {
						win->sliders[i] = win->sliders[win->slider_count - 1];
					}
					--win->slider_count;
					break;
				}
			}
		} break;
#endif

#if MKGUI_HAS_TABS
		case MKGUI_TABS: {
			for(uint32_t i = 0; (MKGUI_HAS_TABS) && i < win->tab_count; ++i) {
				if(win->tabs[i].widget_id == id) {
					if((MKGUI_HAS_TABS) && i < win->tab_count - 1) {
						win->tabs[i] = win->tabs[win->tab_count - 1];
					}
					--win->tab_count;
					break;
				}
			}
		} break;
#endif

#if MKGUI_HAS_VBOX || MKGUI_HAS_HBOX
		case MKGUI_VBOX:
		case MKGUI_HBOX: {
			for(uint32_t i = 0; (MKGUI_HAS_VBOX || MKGUI_HAS_HBOX) && i < win->box_scroll_count; ++i) {
				if(win->box_scrolls[i].widget_id == id) {
					if((MKGUI_HAS_VBOX || MKGUI_HAS_HBOX) && i < win->box_scroll_count - 1) {
						win->box_scrolls[i] = win->box_scrolls[win->box_scroll_count - 1];
					}
					--win->box_scroll_count;
					break;
				}
			}
		} break;
#endif

#if MKGUI_HAS_HSPLIT || MKGUI_HAS_VSPLIT
		case MKGUI_HSPLIT:
		case MKGUI_VSPLIT: {
			for(uint32_t i = 0; (MKGUI_HAS_HSPLIT || MKGUI_HAS_VSPLIT) && i < win->split_count; ++i) {
				if(win->splits[i].widget_id == id) {
					if((MKGUI_HAS_HSPLIT || MKGUI_HAS_VSPLIT) && i < win->split_count - 1) {
						win->splits[i] = win->splits[win->split_count - 1];
					}
					--win->split_count;
					break;
				}
			}
		} break;
#endif

#if MKGUI_HAS_TREEVIEW
		case MKGUI_TREEVIEW: {
			for(uint32_t i = 0; (MKGUI_HAS_TREEVIEW) && i < win->treeview_count; ++i) {
				if(win->treeviews[i].widget_id == id) {
					free(win->treeviews[i].nodes);
					if((MKGUI_HAS_TREEVIEW) && i < win->treeview_count - 1) {
						win->treeviews[i] = win->treeviews[win->treeview_count - 1];
					}
					--win->treeview_count;
					break;
				}
			}
		} break;
#endif

#if MKGUI_HAS_TREELIST
		case MKGUI_TREELIST: {
			for(uint32_t i = 0; (MKGUI_HAS_TREELIST) && i < win->treelist_count; ++i) {
				if(win->treelists[i].widget_id == id) {
					treelist_free(&win->treelists[i]);
					if((MKGUI_HAS_TREELIST) && i < win->treelist_count - 1) {
						win->treelists[i] = win->treelists[win->treelist_count - 1];
					}
					--win->treelist_count;
					break;
				}
			}
		} break;
#endif

#if MKGUI_HAS_STATUSBAR
		case MKGUI_STATUSBAR: {
			for(uint32_t i = 0; (MKGUI_HAS_STATUSBAR) && i < win->statusbar_count; ++i) {
				if(win->statusbars[i].widget_id == id) {
					if((MKGUI_HAS_STATUSBAR) && i < win->statusbar_count - 1) {
						win->statusbars[i] = win->statusbars[win->statusbar_count - 1];
					}
					--win->statusbar_count;
					break;
				}
			}
		} break;
#endif

#if MKGUI_HAS_SPINBOX
		case MKGUI_SPINBOX: {
			for(uint32_t i = 0; (MKGUI_HAS_SPINBOX) && i < win->spinbox_count; ++i) {
				if(win->spinboxes[i].widget_id == id) {
					if((MKGUI_HAS_SPINBOX) && i < win->spinbox_count - 1) {
						win->spinboxes[i] = win->spinboxes[win->spinbox_count - 1];
					}
					--win->spinbox_count;
					break;
				}
			}
		} break;
#endif

#if MKGUI_HAS_PROGRESS
		case MKGUI_PROGRESS: {
			for(uint32_t i = 0; (MKGUI_HAS_PROGRESS) && i < win->progress_count; ++i) {
				if(win->progress[i].widget_id == id) {
					if((MKGUI_HAS_PROGRESS) && i < win->progress_count - 1) {
						win->progress[i] = win->progress[win->progress_count - 1];
					}
					--win->progress_count;
					break;
				}
			}
		} break;
#endif

#if MKGUI_HAS_METER
		case MKGUI_METER: {
			for(uint32_t i = 0; (MKGUI_HAS_METER) && i < win->meter_count; ++i) {
				if(win->meters[i].widget_id == id) {
					if((MKGUI_HAS_METER) && i < win->meter_count - 1) {
						win->meters[i] = win->meters[win->meter_count - 1];
					}
					--win->meter_count;
					break;
				}
			}
		} break;
#endif

#if MKGUI_HAS_TEXTAREA
		case MKGUI_TEXTAREA: {
			for(uint32_t i = 0; (MKGUI_HAS_TEXTAREA) && i < win->textarea_count; ++i) {
				if(win->textareas[i].widget_id == id) {
					free(win->textareas[i].text);
					if((MKGUI_HAS_TEXTAREA) && i < win->textarea_count - 1) {
						win->textareas[i] = win->textareas[win->textarea_count - 1];
					}
					--win->textarea_count;
					break;
				}
			}
		} break;
#endif

#if MKGUI_HAS_LOGVIEW
		case MKGUI_LOGVIEW: {
			for(uint32_t i = 0; (MKGUI_HAS_LOGVIEW) && i < win->logview_count; ++i) {
				if(win->logviews[i].widget_id == id) {
					struct mkgui_logview_data *lv = &win->logviews[i];
					free(lv->text_arena);
					free(lv->lines);
					free(lv->runs);
					free(lv->pending_text);
					free(lv->pending_runs);
					free(lv->vlines);
					if((MKGUI_HAS_LOGVIEW) && i < win->logview_count - 1) {
						win->logviews[i] = win->logviews[win->logview_count - 1];
					}
					--win->logview_count;
					break;
				}
			}
		} break;
#endif

#if MKGUI_HAS_ITEMVIEW
		case MKGUI_ITEMVIEW: {
			for(uint32_t i = 0; (MKGUI_HAS_ITEMVIEW) && i < win->itemview_count; ++i) {
				if(win->itemviews[i].widget_id == id) {
					free(win->itemviews[i].thumb_buf);
					if((MKGUI_HAS_ITEMVIEW) && i < win->itemview_count - 1) {
						win->itemviews[i] = win->itemviews[win->itemview_count - 1];
					}
					--win->itemview_count;
					break;
				}
			}
		} break;
#endif

#if MKGUI_HAS_LISTVIEW
		case MKGUI_LISTVIEW: {
			for(uint32_t i = 0; (MKGUI_HAS_LISTVIEW) && i < win->listv_count; ++i) {
				if(win->listvs[i].widget_id == id) {
					if((MKGUI_HAS_LISTVIEW) && i < win->listv_count - 1) {
						win->listvs[i] = win->listvs[win->listv_count - 1];
					}
					--win->listv_count;
					break;
				}
			}
		} break;
#endif

#if MKGUI_HAS_GRIDVIEW
		case MKGUI_GRIDVIEW: {
			for(uint32_t i = 0; (MKGUI_HAS_GRIDVIEW) && i < win->gridview_count; ++i) {
				if(win->gridviews[i].widget_id == id) {
					free(win->gridviews[i].checks);
					if((MKGUI_HAS_GRIDVIEW) && i < win->gridview_count - 1) {
						win->gridviews[i] = win->gridviews[win->gridview_count - 1];
					}
					--win->gridview_count;
					break;
				}
			}
		} break;
#endif

#if MKGUI_HAS_RICHLIST
		case MKGUI_RICHLIST: {
			for(uint32_t i = 0; (MKGUI_HAS_RICHLIST) && i < win->richlist_count; ++i) {
				if(win->richlists[i].widget_id == id) {
					if((MKGUI_HAS_RICHLIST) && i < win->richlist_count - 1) {
						win->richlists[i] = win->richlists[win->richlist_count - 1];
					}
					--win->richlist_count;
					break;
				}
			}
		} break;
#endif

#if MKGUI_HAS_SCROLLBAR
		case MKGUI_SCROLLBAR: {
			for(uint32_t i = 0; (MKGUI_HAS_SCROLLBAR) && i < win->scrollbar_count; ++i) {
				if(win->scrollbars[i].id == id) {
					if((MKGUI_HAS_SCROLLBAR) && i < win->scrollbar_count - 1) {
						win->scrollbars[i] = win->scrollbars[win->scrollbar_count - 1];
					}
					--win->scrollbar_count;
					break;
				}
			}
		} break;
#endif

#if MKGUI_HAS_IMAGE
		case MKGUI_IMAGE: {
			for(uint32_t i = 0; (MKGUI_HAS_IMAGE) && i < win->image_count; ++i) {
				if(win->images[i].id == id) {
					free(win->images[i].pixels);
					if((MKGUI_HAS_IMAGE) && i < win->image_count - 1) {
						win->images[i] = win->images[win->image_count - 1];
					}
					--win->image_count;
					break;
				}
			}
		} break;
#endif

#if MKGUI_HAS_GLVIEW
		case MKGUI_GLVIEW: {
			for(uint32_t i = 0; (MKGUI_HAS_GLVIEW) && i < win->glview_count; ++i) {
				if(win->glviews[i].id == id) {
					if(win->glviews[i].created) {
						mkwin_glview_destroy(win->glviews[i].mkwin);
					}

					if((MKGUI_HAS_GLVIEW) && i < win->glview_count - 1) {
						win->glviews[i] = win->glviews[win->glview_count - 1];
					}
					--win->glview_count;
					break;
				}
			}
		} break;
#endif

#if MKGUI_HAS_CANVAS
		case MKGUI_CANVAS: {
			for(uint32_t i = 0; (MKGUI_HAS_CANVAS) && i < win->canvas_count; ++i) {
				if(win->canvases[i].widget_id == id) {
					if((MKGUI_HAS_CANVAS) && i < win->canvas_count - 1) {
						win->canvases[i] = win->canvases[win->canvas_count - 1];
					}
					--win->canvas_count;
					break;
				}
			}
		} break;
#endif

#if MKGUI_HAS_PATHBAR
		case MKGUI_PATHBAR: {
			for(uint32_t i = 0; (MKGUI_HAS_PATHBAR) && i < win->pathbar_count; ++i) {
				if(win->pathbars[i].widget_id == id) {
					if((MKGUI_HAS_PATHBAR) && i < win->pathbar_count - 1) {
						win->pathbars[i] = win->pathbars[win->pathbar_count - 1];
					}
					--win->pathbar_count;
					break;
				}
			}
		} break;
#endif

		default: {
		} break;
	}
}

// [=]===^=[ mkgui_widget_remove ]================================[=]
MKGUI_API uint32_t mkgui_widget_remove(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, 0);
	int32_t idx = find_widget_idx(win, id);
	if(idx < 0) {
		return 0;
	}

	if((MKGUI_HAS_WINDOW && win->widgets[idx].type == MKGUI_WINDOW)) {
		return 0;
	}

	uint8_t *marked = (uint8_t *)calloc(win->widget_count, 1);
	if(!marked) {
		return 0;
	}
	marked[idx] = 1;
	uint32_t changed = 1;
	while(changed) {
		changed = 0;
		for(uint32_t i = 0; i < win->widget_count; ++i) {
			if(!marked[i]) {
				int32_t pidx = find_widget_idx(win, win->widgets[i].parent_id);
				if(pidx >= 0 && marked[pidx]) {
					marked[i] = 1;
					changed = 1;
				}
			}
		}
	}

	for(uint32_t i = 0; i < win->widget_count; ++i) {
		if(marked[i]) {
			uint32_t wid = win->widgets[i].id;
			if(win->hover_id == wid) {
				win->hover_id = 0;
			}

			if(win->prev_hover_id == wid) {
				win->prev_hover_id = 0;
			}

			if(win->press_id == wid) {
				win->press_id = 0;
			}

			if(win->focus_id == wid) {
				win->focus_id = 0;
			}

			if(win->prev_focus_id == wid) {
				win->prev_focus_id = 0;
			}

			if(win->drag_scrollbar_id == wid) {
				win->drag_scrollbar_id = 0;
			}

			if(win->drag_col_id == wid) {
				win->drag_col_id = 0;
			}

			if(win->drag_col_resize_id == wid) {
				win->drag_col_resize_id = 0;
			}

			if(win->drag_select_id == wid) {
				win->drag_select_id = 0;
			}

			if(win->drag_text_id == wid) {
				win->drag_text_id = 0;
			}

			if(win->dblclick_id == wid) {
				win->dblclick_id = 0;
			}

			if(win->divider_dblclick_id == wid) {
				win->divider_dblclick_id = 0;
			}

			if(win->tooltip_id == wid) {
				win->tooltip_id = 0;
			}

			for(uint32_t p = 0; p < win->popup_count; ++p) {
				if(win->popups[p].widget_id == wid) {
					popup_destroy(win, p);
					break;
				}
			}

			mkgui_remove_aux_(win, wid, win->widgets[i].type);
		}
	}

	for(int32_t i = (int32_t)win->widget_count - 1; i >= 0; --i) {
		if(marked[i]) {
			uint32_t last = win->widget_count - 1;
			if((uint32_t)i < last) {
				uint32_t tail = last - (uint32_t)i;
				memmove(&win->widgets[i], &win->widgets[i + 1], tail * sizeof(struct mkgui_widget));
				memmove(&win->rects[i], &win->rects[i + 1], tail * sizeof(struct mkgui_rect));
				memmove(&win->tooltip_texts[i], &win->tooltip_texts[i + 1], tail * MKGUI_MAX_TEXT);
				for(uint32_t j = (uint32_t)i; j < last; ++j) {
					marked[j] = marked[j + 1];
				}
			}
			--win->widget_count;
			win->id_index_dirty = 1;
		}
	}

	free(marked);
	dirty_all(win);
	return 1;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// Reserve a per-win arena for the practical max of one aux array. The reserve
// is virtual-only, so the cost is address-space, not RAM; pages fault in
// zero-filled as MKGUI_AUX_GROW commits more.
// [=]===^=[ mkgui_aux_arena_create ]=============================[=]
static uint32_t mkgui_aux_arena_create(struct vm_arena *a, size_t elem_size, uint32_t initial_cap) {
	size_t reserve_bytes = (size_t)MKGUI_VM_MAX_WIDGETS * elem_size;
	if(!vm_arena_create(a, reserve_bytes)) {
		return 0;
	}
	if(initial_cap > 0) {
		if(!vm_arena_ensure(a, (size_t)initial_cap * elem_size)) {
			vm_arena_destroy(a);
			return 0;
		}
	}
	return 1;
}

// [=]===^=[ mkgui_free_arenas ]==================================[=]
static void mkgui_free_arenas(struct mkgui_window *win) {
	vm_arena_destroy(&win->arenas.widgets);
	vm_arena_destroy(&win->arenas.rects);
	vm_arena_destroy(&win->arenas.id_index);
	vm_arena_destroy(&win->arenas.tooltip_texts);
#if MKGUI_HAS_LISTVIEW
	vm_arena_destroy(&win->arenas.listvs);
#endif
#if MKGUI_HAS_INPUT
	vm_arena_destroy(&win->arenas.inputs);
#endif
#if MKGUI_HAS_DROPDOWN
	vm_arena_destroy(&win->arenas.dropdowns);
#endif
#if MKGUI_HAS_SLIDER
	vm_arena_destroy(&win->arenas.sliders);
#endif
#if MKGUI_HAS_TABS
	vm_arena_destroy(&win->arenas.tabs);
#endif
#if MKGUI_HAS_HSPLIT || MKGUI_HAS_VSPLIT
	vm_arena_destroy(&win->arenas.splits);
#endif
#if MKGUI_HAS_TREEVIEW
	vm_arena_destroy(&win->arenas.treeviews);
#endif
#if MKGUI_HAS_STATUSBAR
	vm_arena_destroy(&win->arenas.statusbars);
#endif
#if MKGUI_HAS_SPINBOX
	vm_arena_destroy(&win->arenas.spinboxes);
#endif
#if MKGUI_HAS_PROGRESS
	vm_arena_destroy(&win->arenas.progress);
#endif
#if MKGUI_HAS_METER
	vm_arena_destroy(&win->arenas.meters);
#endif
#if MKGUI_HAS_TEXTAREA
	vm_arena_destroy(&win->arenas.textareas);
#endif
#if MKGUI_HAS_LOGVIEW
	vm_arena_destroy(&win->arenas.logviews);
#endif
#if MKGUI_HAS_ITEMVIEW
	vm_arena_destroy(&win->arenas.itemviews);
#endif
#if MKGUI_HAS_SCROLLBAR
	vm_arena_destroy(&win->arenas.scrollbars);
#endif
#if MKGUI_HAS_VBOX || MKGUI_HAS_HBOX
	vm_arena_destroy(&win->arenas.box_scrolls);
#endif
#if MKGUI_HAS_IMAGE
	vm_arena_destroy(&win->arenas.images);
#endif
#if MKGUI_HAS_GLVIEW
	vm_arena_destroy(&win->arenas.glviews);
#endif
#if MKGUI_HAS_CANVAS
	vm_arena_destroy(&win->arenas.canvases);
#endif
#if MKGUI_HAS_PATHBAR
	vm_arena_destroy(&win->arenas.pathbars);
#endif
#if MKGUI_HAS_IPINPUT
	vm_arena_destroy(&win->arenas.ipinputs);
#endif
#if MKGUI_HAS_TOGGLE
	vm_arena_destroy(&win->arenas.toggles);
#endif
#if MKGUI_HAS_COMBOBOX
	vm_arena_destroy(&win->arenas.comboboxes);
#endif
#if MKGUI_HAS_DATEPICKER
	vm_arena_destroy(&win->arenas.datepickers);
#endif
#if MKGUI_HAS_GRIDVIEW
	vm_arena_destroy(&win->arenas.gridviews);
#endif
#if MKGUI_HAS_RICHLIST
	vm_arena_destroy(&win->arenas.richlists);
#endif
#if MKGUI_HAS_TREELIST
	vm_arena_destroy(&win->arenas.treelists);
#endif
}

// [=]===^=[ mkgui_alloc_arrays ]=================================[=]
static uint32_t mkgui_alloc_arrays(struct mkgui_window *win, uint32_t widget_cap) {
	if(!mkgui_aux_arena_create(&win->arenas.widgets, sizeof(struct mkgui_widget), widget_cap)) {
		return 0;
	}
	if(!mkgui_aux_arena_create(&win->arenas.rects, sizeof(struct mkgui_rect), widget_cap)) {
		mkgui_free_arenas(win);
		return 0;
	}
	if(!mkgui_aux_arena_create(&win->arenas.tooltip_texts, MKGUI_MAX_TEXT, widget_cap)) {
		mkgui_free_arenas(win);
		return 0;
	}
	win->widgets        = (struct mkgui_widget *)win->arenas.widgets.base;
	win->rects          = (struct mkgui_rect *)win->arenas.rects.base;
	win->tooltip_texts  = (char (*)[MKGUI_MAX_TEXT])win->arenas.tooltip_texts.base;
	win->widget_cap = widget_cap;

	// id->index hash: power-of-two slot count kept at >= 2 * widget_cap so the
	// open-addressed probe always finds an empty slot. Reserve the same ceiling
	// as the layout hash; commit only what the initial capacity needs.
	uint32_t id_slots = 512;
	while(id_slots < widget_cap * 2) {
		id_slots <<= 1;
	}
	if(!vm_arena_create(&win->arenas.id_index, (size_t)MKGUI_VM_MAX_HASH_SLOTS * sizeof(struct mkgui_id_slot))) {
		mkgui_free_arenas(win);
		return 0;
	}
	if(!vm_arena_ensure(&win->arenas.id_index, (size_t)id_slots * sizeof(struct mkgui_id_slot))) {
		mkgui_free_arenas(win);
		return 0;
	}
	win->id_index       = (struct mkgui_id_slot *)win->arenas.id_index.base;
	win->id_index_cap   = id_slots;
	win->id_index_dirty = 1;

	win->listv_cap      = 32;
	win->input_cap      = 64;
	win->dropdown_cap   = 32;
	win->slider_cap     = 32;
	win->tab_cap        = 32;
	win->split_cap      = 32;
	win->treeview_cap   = 8;
	win->statusbar_cap  = 8;
	win->spinbox_cap    = 32;
	win->progress_cap   = 32;
	win->meter_cap      = 32;
	win->textarea_cap   = 16;
	win->logview_cap    = 8;
	win->itemview_cap   = 16;
	win->scrollbar_cap  = 32;
	win->box_scroll_cap = 32;
	win->image_cap      = 32;
	win->glview_cap     = 8;
	win->canvas_cap     = 16;
	win->pathbar_cap    = 8;
	win->ipinput_cap    = 16;
	win->toggle_cap     = 32;
	win->combobox_cap   = 16;
	win->datepicker_cap = 16;
	win->gridview_cap   = 16;
	win->richlist_cap   = 16;
	win->treelist_cap   = 16;

	uint32_t success = mkgui_aux_arena_create(&win->arenas.listvs, sizeof(struct mkgui_listview_data), win->listv_cap);
#if MKGUI_HAS_INPUT
	success = success && mkgui_aux_arena_create(&win->arenas.inputs, sizeof(struct mkgui_input_data), win->input_cap);
#endif
#if MKGUI_HAS_DROPDOWN
	success = success && mkgui_aux_arena_create(&win->arenas.dropdowns, sizeof(struct mkgui_dropdown_data), win->dropdown_cap);
#endif
#if MKGUI_HAS_SLIDER
	success = success && mkgui_aux_arena_create(&win->arenas.sliders, sizeof(struct mkgui_slider_data), win->slider_cap);
#endif
#if MKGUI_HAS_TABS
	success = success && mkgui_aux_arena_create(&win->arenas.tabs, sizeof(struct mkgui_tabs_data), win->tab_cap);
#endif
#if MKGUI_HAS_HSPLIT || MKGUI_HAS_VSPLIT
	success = success && mkgui_aux_arena_create(&win->arenas.splits, sizeof(struct mkgui_split_data), win->split_cap);
#endif
#if MKGUI_HAS_TREEVIEW
	success = success && mkgui_aux_arena_create(&win->arenas.treeviews, sizeof(struct mkgui_treeview_data), win->treeview_cap);
#endif
#if MKGUI_HAS_STATUSBAR
	success = success && mkgui_aux_arena_create(&win->arenas.statusbars, sizeof(struct mkgui_statusbar_data), win->statusbar_cap);
#endif
#if MKGUI_HAS_SPINBOX
	success = success && mkgui_aux_arena_create(&win->arenas.spinboxes, sizeof(struct mkgui_spinbox_data), win->spinbox_cap);
#endif
#if MKGUI_HAS_PROGRESS
	success = success && mkgui_aux_arena_create(&win->arenas.progress, sizeof(struct mkgui_progress_data), win->progress_cap);
#endif
#if MKGUI_HAS_METER
	success = success && mkgui_aux_arena_create(&win->arenas.meters, sizeof(struct mkgui_meter_data), win->meter_cap);
#endif
#if MKGUI_HAS_TEXTAREA
	success = success && mkgui_aux_arena_create(&win->arenas.textareas, sizeof(struct mkgui_textarea_data), win->textarea_cap);
#endif
#if MKGUI_HAS_LOGVIEW
	success = success && mkgui_aux_arena_create(&win->arenas.logviews, sizeof(struct mkgui_logview_data), win->logview_cap);
#endif
#if MKGUI_HAS_ITEMVIEW
	success = success && mkgui_aux_arena_create(&win->arenas.itemviews, sizeof(struct mkgui_itemview_data), win->itemview_cap);
#endif
#if MKGUI_HAS_SCROLLBAR
	success = success && mkgui_aux_arena_create(&win->arenas.scrollbars, sizeof(struct mkgui_scrollbar_data), win->scrollbar_cap);
#endif
#if MKGUI_HAS_VBOX || MKGUI_HAS_HBOX
	success = success && mkgui_aux_arena_create(&win->arenas.box_scrolls, sizeof(struct mkgui_box_scroll), win->box_scroll_cap);
#endif
#if MKGUI_HAS_IMAGE
	success = success && mkgui_aux_arena_create(&win->arenas.images, sizeof(struct mkgui_image_data), win->image_cap);
#endif
#if MKGUI_HAS_GLVIEW
	success = success && mkgui_aux_arena_create(&win->arenas.glviews, sizeof(struct mkgui_glview_data), win->glview_cap);
#endif
#if MKGUI_HAS_CANVAS
	success = success && mkgui_aux_arena_create(&win->arenas.canvases, sizeof(struct mkgui_canvas_data), win->canvas_cap);
#endif
#if MKGUI_HAS_PATHBAR
	success = success && mkgui_aux_arena_create(&win->arenas.pathbars, sizeof(struct mkgui_pathbar_data), win->pathbar_cap);
#endif
#if MKGUI_HAS_IPINPUT
	success = success && mkgui_aux_arena_create(&win->arenas.ipinputs, sizeof(struct mkgui_ipinput_data), win->ipinput_cap);
#endif
#if MKGUI_HAS_TOGGLE
	success = success && mkgui_aux_arena_create(&win->arenas.toggles, sizeof(struct mkgui_toggle_data), win->toggle_cap);
#endif
#if MKGUI_HAS_COMBOBOX
	success = success && mkgui_aux_arena_create(&win->arenas.comboboxes, sizeof(struct mkgui_combobox_data), win->combobox_cap);
#endif
#if MKGUI_HAS_DATEPICKER
	success = success && mkgui_aux_arena_create(&win->arenas.datepickers, sizeof(struct mkgui_datepicker_data), win->datepicker_cap);
#endif
#if MKGUI_HAS_GRIDVIEW
	success = success && mkgui_aux_arena_create(&win->arenas.gridviews, sizeof(struct mkgui_gridview_data), win->gridview_cap);
#endif
#if MKGUI_HAS_RICHLIST
	success = success && mkgui_aux_arena_create(&win->arenas.richlists, sizeof(struct mkgui_richlist_data), win->richlist_cap);
#endif
#if MKGUI_HAS_TREELIST
	success = success && mkgui_aux_arena_create(&win->arenas.treelists, sizeof(struct mkgui_treelist_data), win->treelist_cap);
#endif
	if(!success) {
		mkgui_free_arenas(win);
		return 0;
	}

#if MKGUI_HAS_LISTVIEW
	win->listvs      = (struct mkgui_listview_data *)win->arenas.listvs.base;
#endif
#if MKGUI_HAS_INPUT
	win->inputs      = (struct mkgui_input_data *)win->arenas.inputs.base;
#endif
#if MKGUI_HAS_DROPDOWN
	win->dropdowns   = (struct mkgui_dropdown_data *)win->arenas.dropdowns.base;
#endif
#if MKGUI_HAS_SLIDER
	win->sliders     = (struct mkgui_slider_data *)win->arenas.sliders.base;
#endif
#if MKGUI_HAS_TABS
	win->tabs        = (struct mkgui_tabs_data *)win->arenas.tabs.base;
#endif
#if MKGUI_HAS_HSPLIT || MKGUI_HAS_VSPLIT
	win->splits      = (struct mkgui_split_data *)win->arenas.splits.base;
#endif
#if MKGUI_HAS_TREEVIEW
	win->treeviews   = (struct mkgui_treeview_data *)win->arenas.treeviews.base;
#endif
#if MKGUI_HAS_STATUSBAR
	win->statusbars  = (struct mkgui_statusbar_data *)win->arenas.statusbars.base;
#endif
#if MKGUI_HAS_SPINBOX
	win->spinboxes   = (struct mkgui_spinbox_data *)win->arenas.spinboxes.base;
#endif
#if MKGUI_HAS_PROGRESS
	win->progress    = (struct mkgui_progress_data *)win->arenas.progress.base;
#endif
#if MKGUI_HAS_METER
	win->meters      = (struct mkgui_meter_data *)win->arenas.meters.base;
#endif
#if MKGUI_HAS_TEXTAREA
	win->textareas   = (struct mkgui_textarea_data *)win->arenas.textareas.base;
#endif
#if MKGUI_HAS_LOGVIEW
	win->logviews    = (struct mkgui_logview_data *)win->arenas.logviews.base;
#endif
#if MKGUI_HAS_ITEMVIEW
	win->itemviews   = (struct mkgui_itemview_data *)win->arenas.itemviews.base;
#endif
#if MKGUI_HAS_SCROLLBAR
	win->scrollbars  = (struct mkgui_scrollbar_data *)win->arenas.scrollbars.base;
#endif
#if MKGUI_HAS_VBOX || MKGUI_HAS_HBOX
	win->box_scrolls = (struct mkgui_box_scroll *)win->arenas.box_scrolls.base;
#endif
#if MKGUI_HAS_IMAGE
	win->images      = (struct mkgui_image_data *)win->arenas.images.base;
#endif
#if MKGUI_HAS_GLVIEW
	win->glviews     = (struct mkgui_glview_data *)win->arenas.glviews.base;
#endif
#if MKGUI_HAS_CANVAS
	win->canvases    = (struct mkgui_canvas_data *)win->arenas.canvases.base;
#endif
#if MKGUI_HAS_PATHBAR
	win->pathbars    = (struct mkgui_pathbar_data *)win->arenas.pathbars.base;
#endif
#if MKGUI_HAS_IPINPUT
	win->ipinputs    = (struct mkgui_ipinput_data *)win->arenas.ipinputs.base;
#endif
#if MKGUI_HAS_TOGGLE
	win->toggles     = (struct mkgui_toggle_data *)win->arenas.toggles.base;
#endif
#if MKGUI_HAS_COMBOBOX
	win->comboboxes  = (struct mkgui_combobox_data *)win->arenas.comboboxes.base;
#endif
#if MKGUI_HAS_DATEPICKER
	win->datepickers = (struct mkgui_datepicker_data *)win->arenas.datepickers.base;
#endif
#if MKGUI_HAS_GRIDVIEW
	win->gridviews   = (struct mkgui_gridview_data *)win->arenas.gridviews.base;
#endif
#if MKGUI_HAS_RICHLIST
	win->richlists   = (struct mkgui_richlist_data *)win->arenas.richlists.base;
#endif
#if MKGUI_HAS_TREELIST
	win->treelists   = (struct mkgui_treelist_data *)win->arenas.treelists.base;
#endif

	return 1;
}

// [=]===^=[ mkgui_free_arrays ]==================================[=]
static void mkgui_free_arrays(struct mkgui_window *win) {
	for(uint32_t i = 0; (MKGUI_HAS_GRIDVIEW) && i < win->gridview_count; ++i) {
		free(win->gridviews[i].checks);
	}
	mkgui_free_arenas(win);
	win->widgets = NULL;
	win->rects = NULL;
	win->tooltip_texts = NULL;
}

// [=]===^=[ mkgui_ctx_create ]===================================[=]
// No display is opened here; the first mkgui_window_create opens it.
MKGUI_API struct mkgui_ctx *mkgui_ctx_create(void) {
	if(app_ctx) {
		return NULL;
	}
	struct mkgui_ctx *ctx = (struct mkgui_ctx *)calloc(1, sizeof(struct mkgui_ctx));
	if(!ctx) {
		return NULL;
	}
	ctx->theme = auto_theme();
	ctx->scale = 1.0f;
	app_ctx = ctx;
	return ctx;
}

// [=]===^=[ mkgui_window_create ]================================[=]
// Create a window on the given ctx. parent==NULL: top-level window. parent
// non-NULL: transient/dialog window of that parent (shares display, set as
// transient_for). title/w/h override the MKGUI_WINDOW widget in the array;
// pass NULL/0 to inherit from the widget.
MKGUI_API struct mkgui_window *mkgui_window_create(struct mkgui_ctx *ctx, struct mkgui_window *parent, const struct mkgui_widget *widgets, uint32_t count, const char *title, int32_t w, int32_t h) {
	if(!ctx || (!widgets && count > 0) || count > MKGUI_VM_MAX_WIDGETS || (parent && parent->ctx != ctx)) {
		return NULL;
	}
	for(uint32_t i = 0; i < count; ++i) {
		if(!mkgui_widget_type_enabled(widgets[i].type)) {
			return NULL;
		}
	}
	if(ctx->window_count >= MKGUI_MAX_WINDOWS) {
		return NULL;
	}
	struct mkgui_window *win = (struct mkgui_window *)calloc(1, sizeof(struct mkgui_window));
	if(!win) {
		return NULL;
	}

	uint32_t cap = (count + MKGUI_GROW_WIDGETS - 1) & ~(uint32_t)(MKGUI_GROW_WIDGETS - 1);
	if(cap < MKGUI_GROW_WIDGETS) {
		cap = MKGUI_GROW_WIDGETS;
	}

	if(!mkgui_alloc_arrays(win, cap)) {
		free(win);
		return NULL;
	}
	memcpy(win->widgets, widgets, count * sizeof(struct mkgui_widget));
	win->widget_count = count;
	win->ctx = ctx;

	int32_t init_w = w > 0 ? w : 800;
	int32_t init_h = h > 0 ? h : 600;
	const char *use_title = title ? title : (parent ? "mkgui" : "mkgui");
	uint32_t window_style = 0;
	for(uint32_t i = 0; i < count; ++i) {
		if((MKGUI_HAS_WINDOW && widgets[i].type == MKGUI_WINDOW)) {
			if(w <= 0 && widgets[i].w > 0) {
				init_w = widgets[i].w;
			}

			if(h <= 0 && widgets[i].h > 0) {
				init_h = widgets[i].h;
			}

			if(!title && widgets[i].label[0]) {
				use_title = win->widgets[i].label;
			}
			window_style = widgets[i].style;
			break;
		}
	}

	// Effective parent for display sharing: explicit parent, else any existing
	// window on this ctx. NULL means this is the first window; the display opens now.
	struct mkgui_window *share_from = parent;
	if(!share_from && ctx->window_count > 0) {
		share_from = ctx->windows[0];
	}

	win->scale = share_from ? share_from->scale : ctx->scale;
	mkgui_recompute_metrics(win);

	// Child windows use the parent's DPI-scaled pixel size; a top-level uses
	// the requested size and lets scale detection resize it.
	snprintf(win->app_class, sizeof(win->app_class), "%s", ctx->app_class);
	int32_t init_ww = parent ? sc(win, init_w) : init_w;
	int32_t init_hh = parent ? sc(win, init_h) : init_h;
	// mkwin owns pixels/win_w/win_h and publishes them into these fields; scale
	// stays ours (out_scale NULL) so child windows keep the parent's scale.
	struct mkwin_host host;
	host.out_fb = &win->pixels;
	host.out_w = &win->win_w;
	host.out_h = &win->win_h;
	host.out_scale = NULL;
	win->mkwin = mkwin_open(parent ? parent->mkwin : NULL, use_title, init_ww, init_hh, window_style, win->app_class, &host);
	if(!win->mkwin) {
		mkgui_free_arrays(win);
		free(win);
		return NULL;
	}
	mkwin_set_modal_frame_cb(win->mkwin, mkgui_modal_frame, win);

	win->hide_on_close = (window_style & MKGUI_WINDOW_HIDE_ON_CLOSE) ? 1 : 0;
	win->undecorated   = (window_style & MKGUI_WINDOW_UNDECORATED) ? 1 : 0;
	win->canvas_window = (window_style & MKGUI_WINDOW_CANVAS) ? 1 : 0;

	if(!share_from) {
		float detected = mkwin_scale(win->mkwin);
		if(detected > 1.01f) {
			win->scale = detected;
			mkgui_recompute_metrics(win);
			int32_t new_w = (int32_t)(init_w * detected + 0.5f);
			int32_t new_h = (int32_t)(init_h * detected + 0.5f);
			mkwin_resize(win->mkwin, new_w, new_h);
		}
	}

	render_clip_x1 = 0;
	render_clip_y1 = 0;
	render_clip_x2 = win->win_w;
	render_clip_y2 = win->win_h;

	if(!share_from) {
		if(!text_cmd_arena.base) {
			if(!text_cmd_init()) {
				mkgui_free_arrays(win);
				mkwin_destroy(win->mkwin);
				free(win);
				return NULL;
			}
		}

		if(!layout_arena_init()) {
			mkgui_free_arrays(win);
			mkwin_destroy(win->mkwin);
			free(win);
			return NULL;
		}
		mkgui_font_init(win);
		mkgui_icon_init();
	} else {
		memcpy(win->glyphs, share_from->glyphs, sizeof(win->glyphs));
		win->font_ascent = share_from->font_ascent;
		win->font_height = share_from->font_height;
		win->char_width  = share_from->char_width;
	}

	win->theme = share_from ? share_from->theme : ctx->theme;
	icon_load_from_widgets(win);
	win->parent = parent;
	dirty_all(win);
#ifdef _WIN32
	if(share_from) {
		win->perf_freq = share_from->perf_freq;
	} else {
		QueryPerformanceFrequency((LARGE_INTEGER *)&win->perf_freq);
	}
	QueryPerformanceCounter((LARGE_INTEGER *)&win->anim_prev);
#else
	clock_gettime(CLOCK_MONOTONIC, &win->anim_prev);
#endif
	win->anim_time = 0.0;

	init_aux_data(win);
	ctx->windows[ctx->window_count++] = win;
	if(!ctx->primary) {
		ctx->primary = win;
	}

	if(!(window_style & MKGUI_WINDOW_HIDDEN)) {
		mkwin_map(win->mkwin);
		win->window_visible = 1;
	}

	return win;
}

// [=]===^=[ mkgui_ctx_set_app_class ]================================[=]
// Also updates every existing window on this ctx so the WM picks it up now.
MKGUI_API void mkgui_ctx_set_app_class(struct mkgui_ctx *ctx, const char *app_class) {
	MKGUI_CHECK(ctx);
	MKGUI_CHECK(app_class);
	snprintf(ctx->app_class, sizeof(ctx->app_class), "%s", app_class);
	for(uint32_t i = 0; i < ctx->window_count; ++i) {
		struct mkgui_window *win = ctx->windows[i];
		snprintf(win->app_class, sizeof(win->app_class), "%s", app_class);
		mkwin_set_class_hint(win->mkwin, mkwin_is_child(win->mkwin) ? "dialog" : "main", win->app_class);
	}
}

// [=]===^=[ mkgui_window_set_title ]======================================[=]
MKGUI_API void mkgui_window_set_title(struct mkgui_window *win, const char *title) {
	MKGUI_CHECK(win);
	if(!title) {
		title = "";
	}
	mkwin_set_title(win->mkwin, title);
}

// [=]===^=[ mkgui_window_set_instance ]============================[=]
MKGUI_API void mkgui_window_set_instance(struct mkgui_window *win, const char *instance) {
	MKGUI_CHECK(win);
	MKGUI_CHECK(instance);
	char *cls = "mkgui";
	if(win->app_class[0]) {
		cls = win->app_class;
	} else if(win->parent && win->parent->app_class[0]) {
		cls = win->parent->app_class;
	}
	mkwin_set_class_hint(win->mkwin, instance, cls);
}

// [=]===^=[ mkgui_window_set_icon ]================================[=]
MKGUI_API void mkgui_window_set_icon(struct mkgui_window *win, const struct mkgui_icon_size *sizes, uint32_t count) {
	MKGUI_CHECK(win);
	MKGUI_CHECK(sizes);
	if(count == 0) {
		return;
	}
	// struct mkgui_icon_size and struct mkwin_icon_size are layout-identical
	// ({pixels, w, h}); the public mkgui type can't name the mkwin one.
	mkwin_set_icon(win->mkwin, (const struct mkwin_icon_size *)sizes, count);
}

// [=]===^=[ mkgui_ctx_set_theme ]====================================[=]
MKGUI_API void mkgui_ctx_set_theme(struct mkgui_ctx *ctx, struct mkgui_theme theme) {
	MKGUI_CHECK(ctx);
	ctx->theme = theme;

	for(uint32_t i = 0; i < ctx->window_count; ++i) {
		struct mkgui_window *win = ctx->windows[i];
		win->theme = theme;
		dirty_all(win);

	}

	if(ctx->window_count) {
		svg_rerasterize_all(ctx->windows[0], 1.0f);

	}
}

// [=]===^=[ mkgui_ctx_set_scale ]=====================================[=]
MKGUI_API void mkgui_ctx_set_scale(struct mkgui_ctx *ctx, float scale) {
	MKGUI_CHECK(ctx);

	if(scale < 0.5f) {
		scale = 0.5f;

	}

	if(scale > 4.0f) {
		scale = 4.0f;

	}

	float old_scale = ctx->window_count ? ctx->windows[0]->scale : ctx->scale;
	old_scale = old_scale > 0.0f ? old_scale : 1.0f;
	ctx->scale = scale;

	for(uint32_t i = 0; i < ctx->window_count; ++i) {
		struct mkgui_window *win = ctx->windows[i];
		win->scale = scale;
		mkgui_recompute_metrics(win);
		mkgui_font_set_size(win, (int32_t)(13.0f * scale + 0.5f));
		dirty_all(win);

	}

	if(ctx->window_count) {
		svg_rerasterize_all(ctx->windows[0], scale / old_scale);

	}
}

// [=]===^=[ mkgui_ctx_get_scale ]=====================================[=]
MKGUI_API float mkgui_ctx_get_scale(struct mkgui_ctx *ctx) {
	MKGUI_CHECK_VAL(ctx, 1.0f);
	return ctx->scale;
}

// [=]===^=[ mkgui_set_weight ]===================================[=]
MKGUI_API void mkgui_set_weight(struct mkgui_window *win, uint32_t id, uint32_t weight) {
	MKGUI_CHECK(win);
	struct mkgui_widget *w = find_widget(win, id);
	if(w) {
		w->weight = weight;
		dirty_all(win);
	}
}

// [=]===^=[ mkgui_toolbar_set_mode ]==============================[=]
MKGUI_API void mkgui_toolbar_set_mode(struct mkgui_window *win, uint32_t toolbar_id, uint32_t mode) {
	MKGUI_CHECK(win);
	struct mkgui_widget *w = find_widget(win, toolbar_id);
	if(w && (MKGUI_HAS_TOOLBAR && w->type == MKGUI_TOOLBAR)) {
		w->style = (w->style & ~MKGUI_TOOLBAR_MODE_MASK) | (mode & MKGUI_TOOLBAR_MODE_MASK);
		dirty_all(win);
	}
}

// [=]===^=[ mkgui_set_enabled ]====================================[=]
MKGUI_API void mkgui_set_enabled(struct mkgui_window *win, uint32_t id, uint32_t enabled) {
	MKGUI_CHECK(win);
	struct mkgui_widget *w = find_widget(win, id);
	if(!w) {
		return;
	}

	if(enabled) {
		w->flags &= ~MKGUI_DISABLED;

	} else {
		w->flags |= MKGUI_DISABLED;
		if(win->focus_id == id) {
			win->focus_id = 0;
		}
	}
	dirty_all(win);
}

// [=]===^=[ mkgui_get_enabled ]====================================[=]
MKGUI_API uint32_t mkgui_get_enabled(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, 0);
	struct mkgui_widget *w = find_widget(win, id);
	if(!w) {
		return 0;
	}
	return (w->flags & MKGUI_DISABLED) ? 0 : 1;
}

// [=]===^=[ mkgui_set_visible ]====================================[=]
MKGUI_API void mkgui_set_visible(struct mkgui_window *win, uint32_t id, uint32_t visible) {
	MKGUI_CHECK(win);
	struct mkgui_widget *w = find_widget(win, id);
	if(!w) {
		return;
	}

	if(visible) {
		w->flags &= ~MKGUI_HIDDEN;

	} else {
		w->flags |= MKGUI_HIDDEN;
		if(win->focus_id == id) {
			win->focus_id = 0;
		}
	}
	dirty_all(win);
}

// [=]===^=[ mkgui_get_visible ]====================================[=]
MKGUI_API uint32_t mkgui_get_visible(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, 0);
	struct mkgui_widget *w = find_widget(win, id);
	if(!w) {
		return 0;
	}
	return (w->flags & MKGUI_HIDDEN) ? 0 : 1;
}

// Effective-visibility check: walks the parent chain (hidden ancestors,
// non-active tabs). Use to gate per-frame work; mkgui_get_visible reports
// only the widget's own HIDDEN flag.

// [=]===^=[ mkgui_is_shown ]========================================[=]
MKGUI_API uint32_t mkgui_is_shown(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, 0);
	int32_t idx = find_widget_idx(win, id);
	if(idx < 0) {
		return 0;
	}
	return widget_visible(win, (uint32_t)idx);
}

// [=]===^=[ mkgui_set_focus ]======================================[=]
MKGUI_API void mkgui_set_focus(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK(win);
	struct mkgui_widget *w = find_widget(win, id);
	if(!w || !is_focusable(w)) {
		return;
	}
	win->focus_id = id;
	dirty_all(win);
}

// [=]===^=[ mkgui_has_focus ]======================================[=]
MKGUI_API uint32_t mkgui_has_focus(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, 0);
	return win->focus_id == id ? 1 : 0;
}

// [=]===^=[ mkgui_get_tooltip ]====================================[=]
MKGUI_API const char *mkgui_get_tooltip(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, "");
	int32_t idx = find_widget_idx(win, id);
	if(idx < 0) {
		return "";
	}
	return win->tooltip_texts[idx];
}

// [=]===^=[ mkgui_get_geometry ]====================================[=]
MKGUI_API void mkgui_get_geometry(struct mkgui_window *win, uint32_t id, int32_t *x, int32_t *y, int32_t *w, int32_t *h) {
	MKGUI_CHECK(win);
	int32_t idx = find_widget_idx(win, id);
	if(idx < 0) {
		if(x) { *x = 0; }
		if(y) { *y = 0; }
		if(w) { *w = 0; }
		if(h) { *h = 0; }
		return;
	}

	if(x) { *x = win->rects[idx].x; }
	if(y) { *y = win->rects[idx].y; }
	if(w) { *w = win->rects[idx].w; }
	if(h) { *h = win->rects[idx].h; }
}

// [=]===^=[ mkgui_get_min_size ]=================================[=]
MKGUI_API void mkgui_get_min_size(struct mkgui_window *win, int32_t *out_w, int32_t *out_h) {
	MKGUI_CHECK(win);
	layout_build_index(win);
	struct layout_ctx lc = {
		.widgets = win->widgets, .first_child = layout_first_child, .next_sibling = layout_next_sibling,
		.parent = layout_parent, .widget_count = win->widget_count, .has_scroll = 1, .widget_stride = sizeof(struct mkgui_widget)
	};
	int32_t min_w = win->box_pad * 2;
	int32_t min_h = win->box_pad * 2;

	for(uint32_t i = 0; i < win->widget_count; ++i) {
		if(MKGUI_HAS_WINDOW && win->widgets[i].type == MKGUI_WINDOW) {
			lc_window_min_size(win, &lc, i, &min_w, &min_h);
			break;

		}
	}

	if(out_w) {
		*out_w = min_w;

	}

	if(out_h) {
		*out_h = min_h;

	}
}

// [=]===^=[ mkgui_get_window_size ]==================================[=]
MKGUI_API void mkgui_get_window_size(struct mkgui_window *win, int32_t *out_w, int32_t *out_h) {
	MKGUI_CHECK(win);
	if(out_w) { *out_w = win->win_w; }
	if(out_h) { *out_h = win->win_h; }
}

// [=]===^=[ mkgui_get_anim_time ]===================================[=]
MKGUI_API double mkgui_get_anim_time(struct mkgui_window *win) {
	MKGUI_CHECK_VAL(win, 0.0);
	return win->anim_time;
}

// [=]===^=[ mkgui_set_flags ]======================================[=]
MKGUI_API void mkgui_set_flags(struct mkgui_window *win, uint32_t id, uint32_t flags) {
	MKGUI_CHECK(win);
	struct mkgui_widget *w = find_widget(win, id);
	if(!w) {
		return;
	}
	w->flags = flags;
	dirty_all(win);
}

// [=]===^=[ mkgui_get_flags ]======================================[=]
MKGUI_API uint32_t mkgui_get_flags(struct mkgui_window *win, uint32_t id) {
	MKGUI_CHECK_VAL(win, 0);
	struct mkgui_widget *w = find_widget(win, id);
	return w ? w->flags : 0;
}

// [=]===^=[ mkgui_window_destroy ]================================[=]
// Destroy a window. Frees all per-window state. If this was the last
// window on its ctx, also tears down process-level globals (text-cmd
// arena, layout arena). The ctx itself remains valid; call
// mkgui_ctx_destroy when done.
MKGUI_API void mkgui_window_destroy(struct mkgui_window *win) {
	if(!win) {
		return;
	}
	struct mkgui_ctx *ctx = win->ctx;
	for(uint32_t i = 0; i < ctx->window_count; ++i) {
		if(ctx->windows[i] == win) {
			ctx->windows[i] = ctx->windows[--ctx->window_count];
			break;
		}
	}
	if(ctx->primary == win) {
		ctx->primary = ctx->window_count > 0 ? ctx->windows[0] : NULL;
	}
	if(icon_lazy_ctx == win) {
		icon_lazy_ctx = ctx->primary;

	}

	popup_destroy_all(win);
	for(uint32_t i = 0; (MKGUI_HAS_TREEVIEW) && i < win->treeview_count; ++i) {
		free(win->treeviews[i].nodes);
	}
	for(uint32_t i = 0; (MKGUI_HAS_TREELIST) && i < win->treelist_count; ++i) {
		treelist_free(&win->treelists[i]);
	}
	for(uint32_t i = 0; (MKGUI_HAS_TEXTAREA) && i < win->textarea_count; ++i) {
		textarea_undo_free(&win->textareas[i]);
		free(win->textareas[i].text);
	}
	for(uint32_t i = 0; (MKGUI_HAS_LOGVIEW) && i < win->logview_count; ++i) {
		free(win->logviews[i].text_arena);
		free(win->logviews[i].lines);
		free(win->logviews[i].runs);
		free(win->logviews[i].pending_text);
		free(win->logviews[i].pending_runs);
		free(win->logviews[i].vlines);
	}
	for(uint32_t i = 0; (MKGUI_HAS_ITEMVIEW) && i < win->itemview_count; ++i) {
		free(win->itemviews[i].thumb_buf);
	}
	for(uint32_t i = 0; (MKGUI_HAS_IMAGE) && i < win->image_count; ++i) {
		free(win->images[i].pixels);
	}
	free(win->clip_text);
	for(uint32_t i = 0; (MKGUI_HAS_GLVIEW) && i < win->glview_count; ++i) {
		if(win->glviews[i].created) {
			mkwin_glview_destroy(win->glviews[i].mkwin);
		}
	}
	for(uint32_t i = 0; i < win->timer_count; ++i) {
#ifdef _WIN32
		if(win->timers[i].handle) {
			CancelWaitableTimer(win->timers[i].handle);
			CloseHandle(win->timers[i].handle);
		}
#else
		if(win->timers[i].fd >= 0) {
			mkwin_wait_fd_remove(win->timers[i].fd);
			close(win->timers[i].fd);
		}
#endif
	}
	win->timer_count = 0;
	mkgui_free_arrays(win);
	// Last window on this ctx: tear down ctx-shared resources (font, SVG,
	// text-cmd, layout arenas). mkwin_destroy then handles per-window teardown.
	if(ctx->window_count == 0) {
		mkgui_font_fini(win);
		svg_cleanup();
		text_cmd_fini();
		layout_arena_fini();
	}
	mkwin_destroy(win->mkwin);
	free(win);
}

// [=]===^=[ mkgui_ctx_destroy ]================================================================[=]
MKGUI_API void mkgui_ctx_destroy(struct mkgui_ctx *ctx) {
	if(!ctx) {
		return;

	}

	while(ctx->window_count > 0) {
		mkgui_window_destroy(ctx->windows[ctx->window_count - 1]);

	}

	if(app_ctx == ctx) {
		svg_cleanup();
		app_ctx = NULL;

	}

	free(ctx);

}

// ---------------------------------------------------------------------------
// Event processing
// ---------------------------------------------------------------------------

// [=]===^=[ mkgui_resize_render_impl ]=============================[=]
static void mkgui_resize_render_impl(struct mkgui_window *win) {
	// mkwin already resized its framebuffer and published win_w/win_h on WM_SIZE;
	// we just repaint at the current size for live-resize feedback.
	dirty_all(win);
	mkgui_flush(win);
}

// Per-type press dispatch for widgets whose handler has the (win, ev, idx)
// shape. Widgets with special press behaviour are handled before this table.
// discrete: 1 = handler sets an event when it returns nonzero, so poll returns
// 1; 0 = handler may consume without an event, so poll returns ev->type != NONE.
struct mkgui_press_desc {
	uint32_t (*fn)(struct mkgui_window *win, struct mkgui_event *ev, uint32_t idx);
	uint8_t discrete;
};

static struct mkgui_press_desc press_desc[MKGUI_WIDGET_COUNT] = {
#if MKGUI_HAS_TEXTAREA
	[MKGUI_TEXTAREA]  = { handle_textarea_click, 1 },
#endif
#if MKGUI_HAS_LOGVIEW
	[MKGUI_LOGVIEW]   = { handle_logview_click, 1 },
#endif
#if MKGUI_HAS_TABS
	[MKGUI_TABS]      = { handle_tabs_click, 1 },
#endif
#if MKGUI_HAS_LISTVIEW
	[MKGUI_LISTVIEW]  = { handle_listview_click, 0 },
#endif
#if MKGUI_HAS_GRIDVIEW
	[MKGUI_GRIDVIEW]  = { handle_gridview_click, 1 },
#endif
#if MKGUI_HAS_RICHLIST
	[MKGUI_RICHLIST]  = { handle_richlist_click, 1 },
#endif
#if MKGUI_HAS_ITEMVIEW
	[MKGUI_ITEMVIEW]  = { handle_itemview_click, 0 },
#endif
#if MKGUI_HAS_SCROLLBAR
	[MKGUI_SCROLLBAR] = { handle_scrollbar_click, 1 },
#endif
#if MKGUI_HAS_PATHBAR
	[MKGUI_PATHBAR]   = { handle_pathbar_click, 1 },
#endif
#if MKGUI_HAS_SLIDER
	[MKGUI_SLIDER]    = { handle_slider_click, 1 },
#endif
#if MKGUI_HAS_TREEVIEW
	[MKGUI_TREEVIEW]  = { handle_treeview_click, 0 },
#endif
#if MKGUI_HAS_TREELIST
	[MKGUI_TREELIST]  = { handle_treelist_click, 1 },
#endif
#if MKGUI_HAS_SPINBOX
	[MKGUI_SPINBOX]   = { handle_spinbox_click, 0 },
#endif
};

// Per-type key dispatch for the focused widget. All handlers share the superset
// signature (win, ev, ks, keymod, buf, len) and return nonzero for "consumed",
// so no discrete flag is needed (unlike press_desc).
static uint32_t (*key_desc[MKGUI_WIDGET_COUNT])(struct mkgui_window *win, struct mkgui_event *ev, uint32_t ks, uint32_t keymod, char *buf, int32_t len) = {
#if MKGUI_HAS_INPUT
	[MKGUI_INPUT]      = handle_input_key,
#endif
#if MKGUI_HAS_BUTTON
	[MKGUI_BUTTON]     = handle_button_key,
#endif
#if MKGUI_HAS_CHECKBOX
	[MKGUI_CHECKBOX]   = handle_checkbox_key,
#endif
#if MKGUI_HAS_DROPDOWN
	[MKGUI_DROPDOWN]   = handle_dropdown_key,
#endif
#if MKGUI_HAS_SLIDER
	[MKGUI_SLIDER]     = handle_slider_key,
#endif
#if MKGUI_HAS_LISTVIEW
	[MKGUI_LISTVIEW]   = handle_listview_key,
#endif
#if MKGUI_HAS_GRIDVIEW
	[MKGUI_GRIDVIEW]   = handle_gridview_key,
#endif
#if MKGUI_HAS_RICHLIST
	[MKGUI_RICHLIST]   = handle_richlist_key,
#endif
#if MKGUI_HAS_TREEVIEW
	[MKGUI_TREEVIEW]   = handle_treeview_key,
#endif
#if MKGUI_HAS_TREELIST
	[MKGUI_TREELIST]   = handle_treelist_key,
#endif
#if MKGUI_HAS_SPINBOX
	[MKGUI_SPINBOX]    = handle_spinbox_key,
#endif
#if MKGUI_HAS_RADIO
	[MKGUI_RADIO]      = handle_radio_key,
#endif
#if MKGUI_HAS_TEXTAREA
	[MKGUI_TEXTAREA]   = handle_textarea_key,
#endif
#if MKGUI_HAS_ITEMVIEW
	[MKGUI_ITEMVIEW]   = handle_itemview_key,
#endif
#if MKGUI_HAS_TABS
	[MKGUI_TABS]       = handle_tabs_key,
#endif
#if MKGUI_HAS_PATHBAR
	[MKGUI_PATHBAR]    = handle_pathbar_key,
#endif
#if MKGUI_HAS_IPINPUT
	[MKGUI_IPINPUT]    = handle_ipinput_key,
#endif
#if MKGUI_HAS_TOGGLE
	[MKGUI_TOGGLE]     = handle_toggle_key,
#endif
#if MKGUI_HAS_COMBOBOX
	[MKGUI_COMBOBOX]   = handle_combobox_key,
#endif
#if MKGUI_HAS_DATEPICKER
	[MKGUI_DATEPICKER] = handle_datepicker_key,
#endif
};

// Per-type release dispatch for widgets that finish a drag on button-release
// (slider thumb, split handles).
static uint32_t (*release_desc[MKGUI_WIDGET_COUNT])(struct mkgui_window *win, struct mkgui_event *ev, uint32_t id) = {
#if MKGUI_HAS_SLIDER
	[MKGUI_SLIDER] = handle_slider_release,
#endif
#if MKGUI_HAS_HSPLIT
	[MKGUI_HSPLIT] = handle_split_release,
#endif
#if MKGUI_HAS_VSPLIT
	[MKGUI_VSPLIT] = handle_split_release,
#endif
};

// Per-type click dispatch: fires when release lands on the press widget.
// direct: 1 = poll returns the handler result even when 0 (radio); else
// poll returns only when the handler reports an event.
struct mkgui_click_desc {
	uint32_t (*fn)(struct mkgui_window *win, struct mkgui_event *ev, struct mkgui_widget *w);
	uint8_t direct;
};

static struct mkgui_click_desc click_desc[MKGUI_WIDGET_COUNT] = {
#if MKGUI_HAS_BUTTON
	[MKGUI_BUTTON]   = { handle_button_click, 0 },
#endif
#if MKGUI_HAS_LABEL
	[MKGUI_LABEL]    = { handle_label_click, 0 },
#endif
#if MKGUI_HAS_GROUP
	[MKGUI_GROUP]    = { handle_group_click, 0 },
#endif
#if MKGUI_HAS_CHECKBOX
	[MKGUI_CHECKBOX] = { handle_checkbox_click, 0 },
#endif
#if MKGUI_HAS_RADIO
	[MKGUI_RADIO]    = { handle_radio_click, 1 },
#endif
};

// [=]===^=[ mkgui_window_poll ]========================================[=]
MKGUI_API uint32_t mkgui_window_poll(struct mkgui_window *win, struct mkgui_event *ev) {
	MKGUI_CHECK_VAL(win, 0);
	if(!ev) {
		return 0;
	}
	ev->type = MKGUI_EVENT_NONE;
	ev->id = 0;
	ev->value = 0;
	ev->col = 0;
	ev->keysym = 0;
	ev->keymod = 0;

	if(toasts_expire(win)) {
		dirty_all(win);
	}

#ifdef _WIN32
	int64_t now;
	QueryPerformanceCounter((LARGE_INTEGER *)&now);
	double dt = (double)(now - win->anim_prev) / (double)win->perf_freq;
	win->anim_prev = now;
#else
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	double dt = (double)(now.tv_sec - win->anim_prev.tv_sec) + (double)(now.tv_nsec - win->anim_prev.tv_nsec) * 1e-9;
	win->anim_prev = now;
#endif
	win->anim_time += dt;

	for(uint32_t ti = 0; ti < win->timer_count; ++ti) {
		struct mkgui_timer *t = &win->timers[ti];
		if(!t->active) {
			continue;
		}
#ifdef _WIN32
		if(WaitForSingleObject(t->handle, 0) == WAIT_OBJECT_0) {
			++t->fire_count;
			LARGE_INTEGER next;
			next.QuadPart = (int64_t)timer_due(t);
			SetWaitableTimer(t->handle, &next, 0, NULL, NULL, FALSE);
			if(t->cb) {
				t->cb(win, t->id, t->userdata);
			} else {
				ev->type = MKGUI_EVENT_TIMER;
				ev->id = t->id;
				return 1;
			}
		}
#else
		uint64_t expirations;
		if(read(t->fd, &expirations, sizeof(expirations)) == (int64_t)sizeof(expirations)) {
			if(t->cb) {
				t->cb(win, t->id, t->userdata);
			} else {
				ev->type = MKGUI_EVENT_TIMER;
				ev->id = t->id;
				return 1;
			}
		}
#endif
	}

	if(win->focus_id != win->prev_focus_id) {
		if(win->prev_focus_id) {
			ev->type = MKGUI_EVENT_UNFOCUS;
			ev->id = win->prev_focus_id;
			win->prev_focus_id = 0;
			return 1;
		}
		ev->type = MKGUI_EVENT_FOCUS;
		ev->id = win->focus_id;
		win->prev_focus_id = win->focus_id;
		return 1;
	}

	if(win->hover_id != win->prev_hover_id) {
		if(win->prev_hover_id) {
			ev->type = MKGUI_EVENT_HOVER_LEAVE;
			ev->id = win->prev_hover_id;
			win->prev_hover_id = 0;
			return 1;
		}
		ev->type = MKGUI_EVENT_HOVER_ENTER;
		ev->id = win->hover_id;
		win->prev_hover_id = win->hover_id;
		return 1;
	}

	for(uint32_t si = 0; (MKGUI_HAS_SPINBOX) && si < win->spinbox_count; ++si) {
		struct mkgui_spinbox_data *sd = &win->spinboxes[si];
		if(sd->repeat_dir && win->press_id) {
			uint32_t now_ms = (uint32_t)(mkgui_now_ns() / 1000000ull);
			if(now_ms >= sd->repeat_next_ms) {
				int32_t delta = (sd->repeat_dir > 0) ? sd->step : -sd->step;
				if(spinbox_adjust(win, ev, sd->widget_id, delta)) {
					sd->repeat_next_ms = now_ms + 80;
					return 1;
				}
			}
		}
	}

	// widget_visible walks layout_parent[], a file-scope scratch buffer rebuilt
	// per layout pass. If the last flush was for a different window, those indices
	// refer to it and the visibility walk is garbage. Rebuild for THIS window first.
	layout_build_index(win);

	win->anim_active = 0;
	for(uint32_t i = 0; i < win->widget_count; ++i) {
		struct mkgui_widget *w = &win->widgets[i];
		if(!widget_visible(win, i)) {
			continue;
		}

		if((MKGUI_HAS_SPINNER && w->type == MKGUI_SPINNER) || (MKGUI_HAS_GLVIEW && w->type == MKGUI_GLVIEW)) {
			win->anim_active = 1;
		}

		if((MKGUI_HAS_PROGRESS && w->type == MKGUI_PROGRESS) && (w->style & MKGUI_PROGRESS_SHIMMER)) {
			struct mkgui_progress_data *pd = find_progress_data(win, w->id);
			if(pd && pd->value > 0 && pd->value < pd->max_val) {
				win->anim_active = 1;
			}
		}
	}
	for(uint32_t si = 0; (MKGUI_HAS_SPINBOX) && si < win->spinbox_count; ++si) {
		if(win->spinboxes[si].repeat_dir) {
			win->anim_active = 1;
		}
	}

	struct mkwin_event pev;
	while(mkwin_poll(win->mkwin, &pev)) {
		int32_t popup_idx = pev.popup_idx;

		switch(pev.type) {
			// -=[ EXPOSE ]=-
			case MKWIN_EV_EXPOSE: {
				dirty_all(win);
			} break;

			// -=[ RESIZE ]=-
			// mkwin already resized its framebuffer and published win_w/win_h.
			case MKWIN_EV_RESIZE: {
				dirty_all(win);
				ev->type = MKGUI_EVENT_RESIZE;
			} break;

			// -=[ MOUSE MOVE ]=-
			case MKWIN_EV_MOTION: {
				// popups are separate platform windows with independent hover
				// tracking; a motion event targets either a popup or the main
				// window, never both, so handle popup motion and break early
				if(popup_idx >= 0) {
					struct mkgui_popup *mp = &win->popups[popup_idx];
					struct mkgui_widget *mpw = find_widget(win, mp->widget_id);

					if(mp->widget_id == MKGUI_CTXMENU_POPUP_ID) {
						int32_t new_hover = ctxmenu_hit_item(win, pev.y);
						if(new_hover != mp->hover_item) {
							mp->hover_item = new_hover;
							mp->dirty = 1;
						}

					} else if(mpw && (MKGUI_HAS_MENUITEM && mpw->type == MKGUI_MENUITEM)) {
						int32_t hit_idx;
						struct mkgui_widget *hovered = menu_popup_hit_item(win, mpw->id, pev.y, &hit_idx);
						if(hit_idx != mp->hover_item) {
							mp->dirty = 1;
						}
						mp->hover_item = hit_idx;
						if(hovered && !(hovered->style & MKGUI_MENUITEM_SEPARATOR) && menu_item_has_children(win, hovered->id)) {
							uint32_t already_open = 0;
							if((uint32_t)popup_idx + 1 < win->popup_count) {
								if(win->popups[popup_idx + 1].widget_id == hovered->id) {
									already_open = 1;
								}
							}

							if(!already_open) {
								menu_open_submenu(win, (uint32_t)popup_idx, hovered->id);
							}

						} else {
							popup_destroy_from(win, (uint32_t)popup_idx + 1);
						}

					} else if(mpw && (MKGUI_HAS_DATEPICKER && mpw->type == MKGUI_DATEPICKER)) {
						struct mkgui_datepicker_data *mdp = find_datepicker_data(win, mpw->id);
						if(mdp) {
							int32_t new_hover = datepicker_cal_hit(win, mdp, pev.x, pev.y);
							if(new_hover != mdp->cal_hover) {
								mdp->cal_hover = new_hover;
								mp->dirty = 1;
							}
						}

					} else {
						int32_t scroll_off = 0;
						if(mpw && (MKGUI_HAS_DROPDOWN && mpw->type == MKGUI_DROPDOWN)) {
							struct mkgui_dropdown_data *mdd = find_dropdown_data(win, mpw->id);
							if(mdd) {
								scroll_off = mdd->scroll_y;
							}
						} else if(mpw && (MKGUI_HAS_COMBOBOX && mpw->type == MKGUI_COMBOBOX)) {
							struct mkgui_combobox_data *mcb = find_combobox_data(win, mpw->id);
							if(mcb) {
								scroll_off = mcb->scroll_y;
							}
						}
						int32_t new_item = (pev.y - 1 + scroll_off) / win->row_height;
						if(new_item != mp->hover_item) {
							mp->dirty = 1;
						}
						mp->hover_item = new_item;
					}

					dirty_all(win);
					break;
				}

				win->mouse_x = pev.x;
				win->mouse_y = pev.y;

				if(win->canvas_window) {
					for(uint32_t ci = 0; (MKGUI_HAS_CANVAS) && ci < win->canvas_count; ++ci) {
						ev->type = MKGUI_EVENT_CANVAS_MOTION;
						ev->id = win->canvases[ci].widget_id;
						ev->value = pev.x;
						ev->col = pev.y;
						ev->keysym = 0;
						ev->keymod = pev.keymod;
						dirty_all(win);
						return 1;
					}
					break;
				}

				if(win->drag_text_id) {
					int32_t dsi = find_widget_idx(win, win->drag_text_id);
					if(dsi >= 0) {
						struct mkgui_textarea_data *ta = find_textarea_data(win, win->drag_text_id);
						if(ta) {
							win->drag_text_drop_pos = textarea_hit_pos(win, ta, win->rects[dsi].x, win->rects[dsi].y, win->rects[dsi].h, win->mouse_x, win->mouse_y);
							dirty_widget(win, (uint32_t)dsi);
						}
					}
					break;
				}

				if(MKGUI_HAS_CELLEDIT && win->cell_edit.active && win->cell_edit.dragging) {
					struct mkgui_cell_edit *ce = &win->cell_edit;
					if(celledit_compute_rect(win)) {
						int32_t base_x = ce->cell_x + sc(win, 4) - ce->te.scroll_x;
						uint32_t hit_pos = textedit_hit_cursor(win, ce->text, base_x, win->mouse_x);
						ce->te.cursor = hit_pos;
						ce->te.sel_end = hit_pos;
						dirty_widget_id(win, ce->widget_id);
					}
					break;
				}

				if(win->drag_select_id) {
					int32_t dsi = find_widget_idx(win, win->drag_select_id);
					if(dsi >= 0 && (MKGUI_HAS_INPUT && win->widgets[dsi].type == MKGUI_INPUT)) {
						struct mkgui_input_data *inp = find_input_data(win, win->drag_select_id);
						if(inp) {
							char *display = inp->text;
							char masked_buf[MKGUI_MAX_TEXT];
							if(win->widgets[dsi].style & MKGUI_INPUT_PASSWORD) {
								uint32_t mlen = (uint32_t)strlen(inp->text);
								for(uint32_t mi = 0; mi < mlen && mi < MKGUI_MAX_TEXT - 1; ++mi) {
									masked_buf[mi] = '*';
								}
								masked_buf[mlen < MKGUI_MAX_TEXT - 1 ? mlen : MKGUI_MAX_TEXT - 1] = '\0';
								display = masked_buf;
							}
							inp->cursor = input_hit_cursor(win, inp, display, win->rects[dsi].x, win->mouse_x);
							inp->sel_end = inp->cursor;
							dirty_widget(win, (uint32_t)dsi);
							input_scroll_to_cursor(win, win->drag_select_id);
						}

					} else if(dsi >= 0 && (MKGUI_HAS_TEXTAREA && win->widgets[dsi].type == MKGUI_TEXTAREA)) {
						struct mkgui_textarea_data *ta = find_textarea_data(win, win->drag_select_id);
						if(ta) {
							int32_t ry = win->rects[dsi].y;
							int32_t rh = win->rects[dsi].h;
							if(win->mouse_y < ry + 1) {
								ta->scroll_y -= win->row_height;
								if(ta->scroll_y < 0) {
									ta->scroll_y = 0;
								}

							} else if(win->mouse_y > ry + rh - 1) {
								int32_t content_h = (int32_t)textarea_line_count(ta) * win->row_height;
								int32_t view_h = rh - 2;
								int32_t max_scroll = content_h - view_h;
								if(max_scroll > 0) {
									ta->scroll_y += win->row_height;
									if(ta->scroll_y > max_scroll) {
										ta->scroll_y = max_scroll;
									}
								}
							}
							ta->cursor = textarea_hit_pos(win, ta, win->rects[dsi].x, win->rects[dsi].y, win->rects[dsi].h, win->mouse_x, win->mouse_y);
							ta->sel_end = ta->cursor;
							dirty_widget(win, (uint32_t)dsi);
						}

					} else if(dsi >= 0 && (MKGUI_HAS_LOGVIEW && win->widgets[dsi].type == MKGUI_LOGVIEW)) {
						struct mkgui_logview_data *lv = find_logview_data(win, win->drag_select_id);
						if(lv) {
							int32_t ry = win->rects[dsi].y;
							int32_t rh = win->rects[dsi].h;
							int32_t view_h = rh - 2;
							int32_t content_h = logview_total_content_h(win, lv);
							if(win->mouse_y < ry + 1) {
								lv->scroll_y -= win->row_height;
								logview_clamp_scroll(lv, content_h, view_h);

							} else if(win->mouse_y > ry + rh - 1) {
								lv->scroll_y += win->row_height;
								logview_clamp_scroll(lv, content_h, view_h);
							}
							uint32_t vline, off;
							logview_hit_vline(win, lv, win->rects[dsi].x, win->rects[dsi].y, win->mouse_y, win->mouse_x, &vline, &off);
							uint64_t seq;
							uint32_t soff;
							logview_vline_to_pos(lv, vline, off, &seq, &soff);
							lv->sel_cursor_line_seq = seq;
							lv->sel_cursor_off = soff;
							lv->has_selection = (lv->sel_anchor_line_seq != lv->sel_cursor_line_seq || lv->sel_anchor_off != lv->sel_cursor_off);
							int32_t max_scroll = content_h - view_h;
							if(max_scroll < 0) {
								max_scroll = 0;
							}
							lv->stuck_to_end = (lv->scroll_y >= max_scroll) ? 1 : 0;
							dirty_widget(win, (uint32_t)dsi);
						}

					} else if(dsi >= 0 && (MKGUI_HAS_PATHBAR && win->widgets[dsi].type == MKGUI_PATHBAR)) {
						struct mkgui_pathbar_data *pb = find_pathbar_data(win, win->drag_select_id);
						if(pb && pb->editing) {
							int32_t base_x = win->rects[dsi].x + sc(win, 4) - pb->edit_scroll_x;
							uint32_t hit_pos = textedit_hit_cursor(win, pb->edit_buf, base_x, win->mouse_x);
							pb->edit_cursor = hit_pos;
							pb->edit_sel_end = hit_pos;
							dirty_widget(win, (uint32_t)dsi);
							pathbar_scroll_to_cursor(win, pb);
						}

					} else if(dsi >= 0 && (MKGUI_HAS_COMBOBOX && win->widgets[dsi].type == MKGUI_COMBOBOX)) {
						struct mkgui_combobox_data *cb = find_combobox_data(win, win->drag_select_id);
						if(cb) {
							cb->cursor = combobox_hit_cursor(win, cb, win->rects[dsi].x, win->mouse_x);
							cb->sel_end = cb->cursor;
							dirty_widget(win, (uint32_t)dsi);
							combobox_scroll_to_cursor(win, win->drag_select_id);
						}
					}
					break;
				}

				if(win->drag_scrollbar_id) {
					int32_t dsi = find_widget_idx(win, win->drag_scrollbar_id);
					if(dsi >= 0 && (MKGUI_HAS_TEXTAREA && win->widgets[dsi].type == MKGUI_TEXTAREA)) {
						struct mkgui_textarea_data *ta = find_textarea_data(win, win->drag_scrollbar_id);
						if(ta) {
							textarea_scroll_drag(win, (uint32_t)dsi, ta, win->mouse_y, win->drag_scrollbar_offset);
						}
					} else if(dsi >= 0 && (MKGUI_HAS_LOGVIEW && win->widgets[dsi].type == MKGUI_LOGVIEW)) {
						struct mkgui_logview_data *lv = find_logview_data(win, win->drag_scrollbar_id);
						if(lv) {
							logview_sb_drag(win, (uint32_t)dsi, lv, win->mouse_y, win->drag_scrollbar_offset);
						}
					} else if(dsi >= 0 && (MKGUI_HAS_SCROLLBAR && win->widgets[dsi].type == MKGUI_SCROLLBAR)) {
						scrollbar_drag_to(win, win->drag_scrollbar_id, win->mouse_x, win->mouse_y);
					} else if(dsi >= 0 && (MKGUI_HAS_ITEMVIEW && win->widgets[dsi].type == MKGUI_ITEMVIEW)) {
						itemview_scroll_to_mouse(win, win->drag_scrollbar_id, win->mouse_x, win->mouse_y);
					} else if(dsi >= 0 && ((MKGUI_HAS_VBOX && win->widgets[dsi].type == MKGUI_VBOX) || (MKGUI_HAS_HBOX && win->widgets[dsi].type == MKGUI_HBOX))) {
						struct mkgui_box_scroll *bs = find_box_scroll(win, win->drag_scrollbar_id);
						if(bs) {
							if((MKGUI_HAS_HBOX && win->widgets[dsi].type == MKGUI_HBOX) && bs->content_w > win->rects[dsi].w) {
								int32_t sw = win->rects[dsi].w;
								int32_t sx = win->rects[dsi].x;
								int32_t track = sw;
								int32_t total = bs->content_w;
								if(total < 1) {
									total = 1;
								}
								int32_t thumb = track * sw / total;
								if(thumb < sc(win, 20)) {
									thumb = sc(win, 20);
								}

								if(thumb > track) {
									thumb = track;
								}
								int32_t max_scroll = bs->content_w - sw;
								if(max_scroll < 1) {
									max_scroll = 1;
								}
								int32_t new_pos = win->mouse_x - sx - win->drag_scrollbar_offset;
								int32_t scrollable_range = track - thumb;
								if(scrollable_range < 1) {
									scrollable_range = 1;
								}
								bs->scroll_x = (int32_t)((int64_t)new_pos * max_scroll / scrollable_range);
								if(bs->scroll_x < 0) {
									bs->scroll_x = 0;
								}

								if(bs->scroll_x > max_scroll) {
									bs->scroll_x = max_scroll;
								}
							} else {
								int32_t sh = win->rects[dsi].h;
								int32_t sy = win->rects[dsi].y;
								int32_t track = sh;
								int32_t total = bs->content_h;
								if(total < 1) {
									total = 1;
								}
								int32_t thumb = track * sh / total;
								if(thumb < sc(win, 20)) {
									thumb = sc(win, 20);
								}

								if(thumb > track) {
									thumb = track;
								}
								int32_t max_scroll = bs->content_h - sh;
								if(max_scroll < 1) {
									max_scroll = 1;
								}
								int32_t new_pos = win->mouse_y - sy - win->drag_scrollbar_offset;
								int32_t scrollable_range = track - thumb;
								if(scrollable_range < 1) {
									scrollable_range = 1;
								}
								bs->scroll_y = (int32_t)((int64_t)new_pos * max_scroll / scrollable_range);
								if(bs->scroll_y < 0) {
									bs->scroll_y = 0;
								}

								if(bs->scroll_y > max_scroll) {
									bs->scroll_y = max_scroll;
								}
							}
							dirty_widget(win, (uint32_t)dsi);
						}
					} else if(dsi >= 0 && (MKGUI_HAS_GRIDVIEW && win->widgets[dsi].type == MKGUI_GRIDVIEW)) {
						gridview_scroll_to_y(win, win->drag_scrollbar_id, win->mouse_y);
					} else if(dsi >= 0 && (MKGUI_HAS_RICHLIST && win->widgets[dsi].type == MKGUI_RICHLIST)) {
						richlist_scroll_to_y(win, win->drag_scrollbar_id, win->mouse_y);
					} else if(win->drag_scrollbar_horiz) {
						listview_scroll_to_x(win, win->drag_scrollbar_id, win->mouse_x);
					} else {
						listview_scroll_to_y(win, win->drag_scrollbar_id, win->mouse_y);
					}
					break;
				}

				if(win->drag_col_resize_id) {
					int32_t nw = win->drag_col_resize_start_w + (win->mouse_x - win->drag_col_resize_start_x);
					if(nw < sc(win, 40)) {
						nw = sc(win, 40);
					}
					struct mkgui_listview_data *lv = find_listv_data(win, win->drag_col_resize_id);
					if(lv) {
						lv->columns[win->drag_col_resize_col].width = nw;
						dirty_widget_id(win, win->drag_col_resize_id);

					} else {
						struct mkgui_gridview_data *gv = find_gridv_data(win, win->drag_col_resize_id);
						if(gv) {
							gv->columns[win->drag_col_resize_col].width = nw;
							dirty_widget_id(win, win->drag_col_resize_id);

						} else {
							struct mkgui_treelist_data *tl = find_treelist_data(win, win->drag_col_resize_id);
							if(tl) {
								tl->columns[win->drag_col_resize_col].width = nw;
								dirty_widget_id(win, win->drag_col_resize_id);
							}
						}
					}
					break;
				}

				if(win->drag_col_id) {
					int32_t dx = win->mouse_x - win->drag_col_start_x;
					if(dx < -sc(win, 3) || dx > sc(win, 3)) {
						dirty_widget_id(win, win->drag_col_id);
					}
					int32_t lv_idx = find_widget_idx(win, win->drag_col_id);
					if(lv_idx >= 0) {
						struct mkgui_listview_data *lv = find_listv_data(win, win->drag_col_id);
						if(lv) {
							int32_t lx = win->rects[lv_idx].x;
							int32_t lw = win->rects[lv_idx].w;
							int32_t edge = sc(win, 30);
							int32_t scroll_step = win->row_height * 2;
							int32_t content_w = lw - 2 - win->scrollbar_w;
							if(win->mouse_x < lx + edge) {
								lv->scroll_x -= scroll_step;
								dirty_widget(win, (uint32_t)lv_idx);
							} else if(win->mouse_x > lx + lw - edge) {
								lv->scroll_x += scroll_step;
								dirty_widget(win, (uint32_t)lv_idx);
							}
							listview_clamp_scroll_x(lv, content_w);

						} else {
							struct mkgui_treelist_data *tl = find_treelist_data(win, win->drag_col_id);
							if(tl) {
								int32_t lx = win->rects[lv_idx].x;
								int32_t lw = win->rects[lv_idx].w;
								int32_t edge = sc(win, 30);
								int32_t scroll_step = win->row_height * 2;
								int32_t content_w = lw - 2 - win->scrollbar_w;
								if(win->mouse_x < lx + edge) {
									tl->scroll_x -= scroll_step;
									dirty_widget(win, (uint32_t)lv_idx);
								} else if(win->mouse_x > lx + lw - edge) {
									tl->scroll_x += scroll_step;
									dirty_widget(win, (uint32_t)lv_idx);
								}
								treelist_clamp_scroll_x(tl, content_w);
							}
						}
					}
					break;
				}

				if(win->press_id) {
					struct mkgui_widget *pw = find_widget(win, win->press_id);

					if(pw && ((MKGUI_HAS_HSPLIT && pw->type == MKGUI_HSPLIT) || (MKGUI_HAS_VSPLIT && pw->type == MKGUI_VSPLIT))) {
						int32_t pi = find_widget_idx(win, pw->id);
						if(pi >= 0) {
							struct mkgui_split_data *sd = find_split_data(win, pw->id);
							if(sd) {
								if((MKGUI_HAS_HSPLIT && pw->type == MKGUI_HSPLIT)) {
									int32_t rh = win->rects[pi].h;
									if(rh > 0) {
										sd->ratio = (float)(win->mouse_y - win->rects[pi].y) / (float)rh;
										float min_ratio = (float)win->split_min_px / (float)rh;
										float max_ratio = 1.0f - (float)(win->split_min_px + win->split_thick) / (float)rh;
										if(min_ratio > 0.45f) {
											min_ratio = 0.45f;
										}

										if(max_ratio < 0.55f) {
											max_ratio = 0.55f;
										}

										if(sd->ratio < min_ratio) {
											sd->ratio = min_ratio;
										}

										if(sd->ratio > max_ratio) {
											sd->ratio = max_ratio;
										}
									}

								} else {
									int32_t rw = win->rects[pi].w;
									if(rw > 0) {
										sd->ratio = (float)(win->mouse_x - win->rects[pi].x) / (float)rw;
										float min_ratio = (float)win->split_min_px / (float)rw;
										float max_ratio = 1.0f - (float)(win->split_min_px + win->split_thick) / (float)rw;
										if(min_ratio > 0.45f) {
											min_ratio = 0.45f;
										}

										if(max_ratio < 0.55f) {
											max_ratio = 0.55f;
										}

										if(sd->ratio < min_ratio) {
											sd->ratio = min_ratio;
										}

										if(sd->ratio > max_ratio) {
											sd->ratio = max_ratio;
										}
									}
								}
								dirty_widget(win, (uint32_t)pi);
							}
						}
					}

					if(pw && (MKGUI_HAS_SLIDER && pw->type == MKGUI_SLIDER)) {
						struct mkgui_slider_data *sd = find_slider_data(win, pw->id);
						int32_t pi = find_widget_idx(win, pw->id);
						if(sd && pi >= 0) {
							int32_t range = sd->max_val - sd->min_val;
							int32_t thumb_sz = sc(win, 10);
							int32_t new_val;
							if(pw->flags & MKGUI_VERTICAL) {
								int32_t ry = win->rects[pi].y;
								int32_t rh = win->rects[pi].h;
								int32_t track = rh - thumb_sz;
								int32_t pos = (win->mouse_y - win->drag_slider_offset) - ry - thumb_sz / 2;
								new_val = sd->max_val - (track > 0 ? (int32_t)((int64_t)pos * range / track) : 0);
							} else {
								int32_t rx = win->rects[pi].x;
								int32_t rw = win->rects[pi].w;
								int32_t track = rw - thumb_sz;
								int32_t pos = (win->mouse_x - win->drag_slider_offset) - rx - thumb_sz / 2;
								new_val = sd->min_val + (track > 0 ? (int32_t)((int64_t)pos * range / track) : 0);
							}

							if(new_val < sd->min_val) {
								new_val = sd->min_val;
							}

							if(new_val > sd->max_val) {
								new_val = sd->max_val;
							}

							if(new_val != sd->value) {
								sd->value = new_val;
								dirty_widget(win, (uint32_t)pi);
								ev->type = MKGUI_EVENT_SLIDER_CHANGED;
								ev->id = pw->id;
								ev->value = new_val;
								return 1;
							}
						}
					}
				}

				int32_t hi = hit_test(win, win->mouse_x, win->mouse_y);
				uint32_t new_hover = (hi >= 0) ? win->widgets[hi].id : 0;
				if(new_hover != win->hover_id) {
					if(win->hover_id) {
						dirty_widget_id(win, win->hover_id);
					}
					win->hover_id = new_hover;
					if(win->hover_id) {
						dirty_widget_id(win, win->hover_id);
					}
				}

				if(hi >= 0 && (MKGUI_HAS_TABS && win->widgets[hi].type == MKGUI_TABS)) {
					struct mkgui_tabs_data *td = find_tabs_data(win, win->widgets[hi].id);
					if(td) {
						uint32_t ht = tab_hit_test(win, (uint32_t)hi, win->mouse_x, win->mouse_y);
						if(ht != td->hover_tab) {
							td->hover_tab = ht;
							dirty_widget(win, (uint32_t)hi);
						}
					}
				}

				if(hi >= 0 && (MKGUI_HAS_PATHBAR && win->widgets[hi].type == MKGUI_PATHBAR)) {
					struct mkgui_pathbar_data *pb = find_pathbar_data(win, win->widgets[hi].id);
					if(pb && !pb->editing) {
						int32_t seg = pathbar_segment_hit(win, pb, win->rects[hi].x, win->mouse_x);
						if(seg != pb->hover_seg) {
							pb->hover_seg = seg;
							dirty_widget(win, (uint32_t)hi);
						}
					}
				}
				for(uint32_t tvi = 0; (MKGUI_HAS_TREEVIEW) && tvi < win->treeview_count; ++tvi) {
					struct mkgui_treeview_data *tv = &win->treeviews[tvi];
					if(tv->drag_source >= 0 && win->press_id == tv->widget_id) {
						int32_t dy = win->mouse_y - tv->drag_start_y;
						if(dy < 0) {
							dy = -dy;
						}

						if(!tv->drag_active && dy > sc(win, 4)) {
							tv->drag_active = 1;
							ev->type = MKGUI_EVENT_DRAG_START;
							ev->id = tv->widget_id;
							ev->value = (int32_t)tv->nodes[tv->drag_source].id;
							dirty_widget_id(win, tv->widget_id);
							return 1;
						}

						if(tv->drag_active) {
							int32_t tidx = find_widget_idx(win, tv->widget_id);
							if(tidx >= 0) {
								int32_t node_idx = treeview_row_hit(win, (uint32_t)tidx, win->mouse_y);
								if(node_idx >= 0 && node_idx != tv->drag_source) {
									int32_t ry = win->rects[tidx].y;
									int32_t local_y = win->mouse_y - ry - 1 + tv->scroll_y;
									int32_t row_local = local_y % win->row_height;
									int32_t third = win->row_height / 3;
									if(row_local < third) {
										tv->drag_pos = 0;
									} else if(row_local > win->row_height - third) {
										tv->drag_pos = 1;
									} else {
										tv->drag_pos = 2;
									}
									tv->drag_target = node_idx;
								} else {
									tv->drag_target = -1;
								}
								dirty_widget(win, (uint32_t)tidx);
							}
						}
					}
				}
				for(uint32_t lvi = 0; (MKGUI_HAS_LISTVIEW) && lvi < win->listv_count; ++lvi) {
					struct mkgui_listview_data *lv = &win->listvs[lvi];
					if(lv->drag_source >= 0 && win->press_id == lv->widget_id) {
						int32_t dy = win->mouse_y - lv->drag_start_y;
						if(dy < 0) {
							dy = -dy;
						}

						if(!lv->drag_active && dy > sc(win, 4)) {
							lv->drag_active = 1;
							ev->type = MKGUI_EVENT_DRAG_START;
							ev->id = lv->widget_id;
							ev->value = lv->drag_source;
							dirty_widget_id(win, lv->widget_id);
							return 1;
						}

						if(lv->drag_active) {
							int32_t lidx = find_widget_idx(win, lv->widget_id);
							if(lidx >= 0) {
								int32_t tgt = listview_row_hit(win, (uint32_t)lidx, win->mouse_x, win->mouse_y);
								if(tgt >= 0 && tgt != lv->drag_source) {
									lv->drag_target = tgt;
								} else {
									lv->drag_target = -1;
								}
								dirty_widget(win, (uint32_t)lidx);
							}
						}
					}
				}
				for(uint32_t gvi = 0; (MKGUI_HAS_GRIDVIEW) && gvi < win->gridview_count; ++gvi) {
					struct mkgui_gridview_data *gv = &win->gridviews[gvi];
					if(gv->drag_source >= 0 && win->press_id == gv->widget_id) {
						int32_t dy = win->mouse_y - gv->drag_start_y;
						if(dy < 0) {
							dy = -dy;
						}

						if(!gv->drag_active && dy > sc(win, 4)) {
							gv->drag_active = 1;
							ev->type = MKGUI_EVENT_DRAG_START;
							ev->id = gv->widget_id;
							ev->value = gv->drag_source;
							dirty_widget_id(win, gv->widget_id);
							return 1;
						}

						if(gv->drag_active) {
							int32_t gidx = find_widget_idx(win, gv->widget_id);
							if(gidx >= 0) {
								int32_t tgt = gridview_row_hit(win, (uint32_t)gidx, win->mouse_y);
								if(tgt >= 0 && tgt != gv->drag_source) {
									gv->drag_target = tgt;
								} else {
									gv->drag_target = -1;
								}
								dirty_widget(win, (uint32_t)gidx);
							}
						}
					}
				}

				if(hi >= 0 && (MKGUI_HAS_SPINBOX && win->widgets[hi].type == MKGUI_SPINBOX)) {
					struct mkgui_spinbox_data *sd = find_spinbox_data(win, win->widgets[hi].id);
					if(sd) {
						int32_t hb = spinbox_btn_hit(win, (uint32_t)hi, win->mouse_x, win->mouse_y);
						if(hb != sd->hover_btn) {
							sd->hover_btn = hb;
							dirty_widget(win, (uint32_t)hi);
						}
					}
				}

				uint32_t cursor_type = MKWIN_CURSOR_DEFAULT;
				if(win->press_id) {
					struct mkgui_widget *pw = find_widget(win, win->press_id);
					if(pw) {
						if((MKGUI_HAS_HSPLIT && pw->type == MKGUI_HSPLIT)) {
							cursor_type = MKWIN_CURSOR_V_RESIZE;
						} else if((MKGUI_HAS_VSPLIT && pw->type == MKGUI_VSPLIT)) {
							cursor_type = MKWIN_CURSOR_H_RESIZE;
						}
					}
				}

				if(cursor_type == MKWIN_CURSOR_DEFAULT && hi >= 0) {
					uint32_t htype = win->widgets[hi].type;
					if((MKGUI_HAS_HSPLIT && htype == MKGUI_HSPLIT)) {
						cursor_type = MKWIN_CURSOR_V_RESIZE;
					} else if((MKGUI_HAS_VSPLIT && htype == MKGUI_VSPLIT)) {
						cursor_type = MKWIN_CURSOR_H_RESIZE;
					} else if((MKGUI_HAS_LISTVIEW && htype == MKGUI_LISTVIEW)) {
						if(listview_divider_hit(win, (uint32_t)hi, win->mouse_x, win->mouse_y) >= 0) {
							cursor_type = MKWIN_CURSOR_H_RESIZE;
						}
					} else if((MKGUI_HAS_TREELIST && htype == MKGUI_TREELIST)) {
						if(treelist_divider_hit(win, (uint32_t)hi, win->mouse_x, win->mouse_y) >= 0) {
							cursor_type = MKWIN_CURSOR_H_RESIZE;
						}
					} else if((MKGUI_HAS_GRIDVIEW && htype == MKGUI_GRIDVIEW)) {
						if(gridview_divider_hit(win, (uint32_t)hi, win->mouse_x, win->mouse_y) >= 0) {
							cursor_type = MKWIN_CURSOR_H_RESIZE;
						}
					}
				}
				mkwin_set_cursor(win->mkwin, cursor_type);

				if(win->popup_count > 0 && hi >= 0 && (MKGUI_HAS_MENUITEM && win->widgets[hi].type == MKGUI_MENUITEM)) {
					struct mkgui_widget *mw = &win->widgets[hi];
					struct mkgui_widget *mp = find_widget(win, mw->parent_id);
					if(mp && (MKGUI_HAS_MENU && mp->type == MKGUI_MENU) && win->popups[0].widget_id != mw->id) {
						menu_open_popup(win, mw->id);
					}
				}

				tooltip_update(win, new_hover, win->mouse_x, win->mouse_y);
			} break;

			// -=[ BUTTON PRESS ]=-
			case MKWIN_EV_BUTTON_PRESS: {
				// Toast and banner clicks dismiss the notification without
				// reaching the underlying widget. Only left-click.
				if(pev.button == 1 && popup_idx < 0 && notify_handle_click(win, win->mouse_x, win->mouse_y)) {
					break;
				}

				if(win->canvas_window && popup_idx < 0) {
					for(uint32_t ci = 0; (MKGUI_HAS_CANVAS) && ci < win->canvas_count; ++ci) {
						ev->type = MKGUI_EVENT_CANVAS_PRESS;
						ev->id = win->canvases[ci].widget_id;
						ev->value = pev.x;
						ev->col = pev.y;
						ev->keysym = pev.button;
						ev->keymod = pev.keymod;
						return 1;
					}
					break;
				}

				if(MKGUI_HAS_CELLEDIT && win->cell_edit.active && pev.button == 1) {
					struct mkgui_cell_edit *ce = &win->cell_edit;
					if(!celledit_compute_rect(win) || win->mouse_x < ce->cell_x || win->mouse_x >= ce->cell_x + ce->cell_w || win->mouse_y < ce->cell_y || win->mouse_y >= ce->cell_y + ce->cell_h) {
						ce->dragging = 0;
						celledit_commit(win, ev);
						if(ev->type != MKGUI_EVENT_NONE) {
							return 1;
						}
					} else {
						int32_t base_x = ce->cell_x + sc(win, 4) - ce->te.scroll_x;
						uint32_t hit_pos = textedit_hit_cursor(win, ce->text, base_x, win->mouse_x);
						ce->te.cursor = hit_pos;
						ce->te.sel_start = hit_pos;
						ce->te.sel_end = hit_pos;
						ce->dragging = 1;
						dirty_widget_id(win, ce->widget_id);
						break;
					}
				}

				if(popup_idx >= 0) {
					struct mkgui_popup *p = &win->popups[popup_idx];
					struct mkgui_widget *pw = find_widget(win, p->widget_id);

					if(p->widget_id == MKGUI_CTXMENU_POPUP_ID && pev.button == 1) {
						int32_t hit = ctxmenu_hit_item(win, pev.y);
						if(hit >= 0) {
							struct mkgui_ctxmenu_item *it = ctxmenu_item_at(win, hit);
							if(it && !(it->flags & MKGUI_DISABLED)) {
								if(it->flags & MKGUI_MENUITEM_CHECK) {
									it->flags ^= MKGUI_MENUITEM_CHECKED;
								} else if(it->flags & MKGUI_MENUITEM_RADIO) {
									for(uint32_t ri = 0; ri < win->ctxmenu_count; ++ri) {
										win->ctxmenu_items[ri].flags &= ~MKGUI_MENUITEM_CHECKED;
									}
									it->flags |= MKGUI_MENUITEM_CHECKED;
								}
								ev->type = MKGUI_EVENT_CONTEXT_MENU;
								ev->id = it->id;
								ev->value = (it->flags & MKGUI_MENUITEM_CHECKED) ? 1 : 0;
								popup_destroy_all(win);
								dirty_all(win);
								return 1;
							}
						}
						// break (not return): dismiss the menu, then let the click
						// fall through to the main window dispatcher below
						popup_destroy_all(win);
						dirty_all(win);
						break;
					}

					if((pev.button == 4 || pev.button == 5) && pw && (MKGUI_HAS_DATEPICKER && pw->type == MKGUI_DATEPICKER)) {
						struct mkgui_datepicker_data *dp = find_datepicker_data(win, pw->id);
						if(dp) {
							int32_t hit = datepicker_cal_hit(win, dp, pev.x, pev.y);
							if(hit == -5 || hit == -6 || hit == -7) {
								dp->view_year += (pev.button == 4) ? 1 : -1;
							} else {
								if(pev.button == 4) {
									--dp->view_month;
									if(dp->view_month < 1) {
										dp->view_month = 12; --dp->view_year;
									}
								} else {
									++dp->view_month;
									if(dp->view_month > 12) {
										dp->view_month = 1; ++dp->view_year;
									}
								}
							}
							p->dirty = 1;
							dirty_all(win);
						}
						break;
					}

					if((pev.button == 4 || pev.button == 5) && pw && (MKGUI_HAS_DROPDOWN && pw->type == MKGUI_DROPDOWN)) {
						struct mkgui_dropdown_data *dd = find_dropdown_data(win, pw->id);
						if(dd) {
							int32_t delta = (pev.button == 4) ? -win->row_height * 3 : win->row_height * 3;
							dd->scroll_y += delta;
							dropdown_clamp_scroll(win, dd, p->h);
							p->hover_item = (pev.y - 1 + dd->scroll_y) / win->row_height;
							p->dirty = 1;
						}
						break;
					}

					if(pw && (MKGUI_HAS_DROPDOWN && pw->type == MKGUI_DROPDOWN)) {
						int32_t scroll_off = 0;
						struct mkgui_dropdown_data *sdd = find_dropdown_data(win, pw->id);
						if(sdd) {
							scroll_off = sdd->scroll_y;
						}
						int32_t item = (pev.y - 1 + scroll_off) / win->row_height;
						struct mkgui_dropdown_data *dd = find_dropdown_data(win, pw->id);
						if(dd && item >= 0 && item < (int32_t)dd->item_count) {
							dd->selected = item;
							ev->type = MKGUI_EVENT_DROPDOWN_CHANGED;
							ev->id = pw->id;
							ev->value = item;
							popup_destroy(win, (uint32_t)popup_idx);
							dirty_all(win);
							return 1;
						}

					} else if(pw && (MKGUI_HAS_COMBOBOX && pw->type == MKGUI_COMBOBOX)) {
						struct mkgui_combobox_data *cb = find_combobox_data(win, pw->id);
						if(cb) {
							int32_t item = (pev.y - 1 + cb->scroll_y) / win->row_height;
							if(item >= 0 && item < (int32_t)cb->filter_count) {
								uint32_t real_idx = cb->filter_map[item];
								cb->selected = (int32_t)real_idx;
								{ size_t _l = strlen(cb->items[real_idx]); if(_l >= MKGUI_MAX_TEXT) { _l = MKGUI_MAX_TEXT - 1; } memcpy(cb->text, cb->items[real_idx], _l); cb->text[_l] = '\0'; }
								cb->cursor = (uint32_t)strlen(cb->text);
								combobox_clear_selection(cb);
								combobox_close_popup(win, cb);
								ev->type = MKGUI_EVENT_COMBOBOX_CHANGED;
								ev->id = pw->id;
								ev->value = cb->selected;
								dirty_all(win);
								return 1;
							}
						}

					} else if(pw && (MKGUI_HAS_DATEPICKER && pw->type == MKGUI_DATEPICKER)) {
						struct mkgui_datepicker_data *dp = find_datepicker_data(win, pw->id);
						if(dp) {
							int32_t hit = datepicker_cal_hit(win, dp, pev.x, pev.y);
							if(hit == -2) {
								--dp->view_month;
								if(dp->view_month < 1) {
									dp->view_month = 12;
									--dp->view_year;
								}
								p->dirty = 1;
								dirty_all(win);
								break;
							} else if(hit == -3) {
								++dp->view_month;
								if(dp->view_month > 12) {
									dp->view_month = 1;
									++dp->view_year;
								}
								p->dirty = 1;
								dirty_all(win);
								break;
							} else if(hit == -4) {
								dp->month_select = dp->month_select ? 0 : 1;
								dp->cal_hover = -1;
								p->dirty = 1;
								dirty_all(win);
								break;
							} else if(hit == -5) {
								++dp->view_year;
								p->dirty = 1;
								dirty_all(win);
								break;
							} else if(hit == -6) {
								--dp->view_year;
								p->dirty = 1;
								dirty_all(win);
								break;
							} else if(hit >= 100 && hit < 112) {
								dp->view_month = hit - 100 + 1;
								dp->month_select = 0;
								dp->cal_hover = -1;
								p->dirty = 1;
								dirty_all(win);
								break;
							} else if(hit == -7 || hit == -1) {
								break;
							} else if(hit > 0) {
								dp->day = hit;
								dp->month = dp->view_month;
								dp->year = dp->view_year;
								dp->popup_open = 0;
								dp->editing = 0;
								popup_destroy(win, (uint32_t)popup_idx);
								ev->type = MKGUI_EVENT_DATEPICKER_CHANGED;
								ev->id = pw->id;
								dirty_all(win);
								return 1;
							}
						}

					} else if(pw && (MKGUI_HAS_MENUITEM && pw->type == MKGUI_MENUITEM)) {
						int32_t hit_idx;
						struct mkgui_widget *clicked = menu_popup_hit_item(win, pw->id, pev.y, &hit_idx);
						if(clicked && !(clicked->style & MKGUI_MENUITEM_SEPARATOR)) {
							if(menu_item_has_children(win, clicked->id)) {
								menu_open_submenu(win, (uint32_t)popup_idx, clicked->id);

							} else {
								if(clicked->style & MKGUI_MENUITEM_CHECK) {
									clicked->style ^= MKGUI_MENUITEM_CHECKED;

								} else if(clicked->style & MKGUI_MENUITEM_RADIO) {
									for(uint32_t ri = 0; ri < win->widget_count; ++ri) {
										struct mkgui_widget *rw = &win->widgets[ri];
										if((MKGUI_HAS_MENUITEM && rw->type == MKGUI_MENUITEM) && rw->parent_id == clicked->parent_id && (rw->style & MKGUI_MENUITEM_RADIO)) {
											rw->style &= ~MKGUI_MENUITEM_CHECKED;
										}
									}
									clicked->style |= MKGUI_MENUITEM_CHECKED;
								}
								ev->type = MKGUI_EVENT_MENU;
								ev->id = clicked->id;
								ev->value = (clicked->style & MKGUI_MENUITEM_CHECKED) ? 1 : 0;
								popup_destroy_all(win);
								dirty_all(win);
								return 1;
							}
						}
					}

					popup_destroy(win, (uint32_t)popup_idx);
					dirty_all(win);
					break;
				}

				uint32_t closed_dropdown_id = 0;
				uint32_t closed_combobox_id = 0;
				if(win->popup_count > 0) {
					for(uint32_t pi = 0; pi < win->popup_count; ++pi) {
						struct mkgui_widget *pw = find_widget(win, win->popups[pi].widget_id);
						if(pw && (MKGUI_HAS_DROPDOWN && pw->type == MKGUI_DROPDOWN)) {
							closed_dropdown_id = win->popups[pi].widget_id;
						}

						if(pw && (MKGUI_HAS_COMBOBOX && pw->type == MKGUI_COMBOBOX)) {
							closed_combobox_id = win->popups[pi].widget_id;
							struct mkgui_combobox_data *cb = find_combobox_data(win, pw->id);
							if(cb) {
								cb->popup_open = 0;
							}
						}
					}
					popup_destroy_all(win);
					dirty_all(win);
				}

				win->mouse_x = pev.x;
				win->mouse_y = pev.y;
				win->mouse_btn = pev.button;

				if(pev.button == 3) {
					int32_t hi = hit_test(win, win->mouse_x, win->mouse_y);
					if(hi >= 0) {
						struct mkgui_widget *hw = &win->widgets[hi];
						win->ctxmenu_x = win->mouse_x;
						win->ctxmenu_y = win->mouse_y;

						if((MKGUI_HAS_LISTVIEW && hw->type == MKGUI_LISTVIEW)) {
							struct mkgui_listview_data *lv = find_listv_data(win, hw->id);
							if(lv) {
								int32_t hdr = listview_header_hit(win, (uint32_t)hi, win->mouse_x, win->mouse_y);
								if(hdr >= 0) {
									ev->type = MKGUI_EVENT_CONTEXT_HEADER;
									ev->id = hw->id;
									ev->col = (int32_t)lv->col_order[hdr];
									ev->value = win->mouse_x;
									return 1;
								}
								int32_t hh = lv->header_height > 0 ? lv->header_height : win->row_height;
								int32_t body_y = win->rects[hi].y + hh + 1;
								int32_t row = (win->mouse_y - body_y + lv->scroll_y) / win->row_height;
								int32_t col = -1;
								int32_t cx = win->rects[hi].x - lv->scroll_x;
								for(uint32_t d = 0; d < lv->col_count; ++d) {
									uint32_t c = lv->col_order[d];
									if(win->mouse_x >= cx && win->mouse_x < cx + lv->columns[c].width) {
										col = (int32_t)c;
										break;
									}
									cx += lv->columns[c].width;
								}

								if(row < 0 || row >= (int32_t)lv->row_count) {
									row = -1;
								} else {
									if(hw->style & MKGUI_LISTVIEW_MULTI_SELECT) {
										if(lv_multi_sel_find(lv, row) < 0) {
											lv->multi_sel_count = 0;
											lv->multi_sel[0] = row;
											lv->multi_sel_count = 1;
											lv->selected_row = row;
											dirty_widget(win, (uint32_t)hi);
										}
									} else {
										if(lv->selected_row != row) {
											lv->selected_row = row;
											dirty_widget(win, (uint32_t)hi);
										}
									}
								}
								ev->type = MKGUI_EVENT_CONTEXT;
								ev->id = hw->id;
								ev->value = row;
								ev->col = col;
								return 1;
							}

						} else if((MKGUI_HAS_GRIDVIEW && hw->type == MKGUI_GRIDVIEW)) {
							struct mkgui_gridview_data *gv = find_gridv_data(win, hw->id);
							if(gv) {
								int32_t ghh = gv->header_height > 0 ? gv->header_height : win->row_height;
								if(win->mouse_y < win->rects[hi].y + ghh) {
									int32_t col = gridview_col_at_x(gv, win->mouse_x, win->rects[hi].x + 1);
									ev->type = MKGUI_EVENT_CONTEXT_HEADER;
									ev->id = hw->id;
									ev->col = col;
									ev->value = win->mouse_x;
									return 1;
								}
								int32_t body_y = win->rects[hi].y + ghh + 1;
								int32_t row = (win->mouse_y - body_y + gv->scroll_y) / win->row_height;
								int32_t col = gridview_col_at_x(gv, win->mouse_x, win->rects[hi].x + 1);
								if(row < 0 || row >= (int32_t)gv->row_count) {
									row = -1;
								} else if(gv->selected_row != row) {
									gv->selected_row = row;
									gv->selected_col = col;
									dirty_widget(win, (uint32_t)hi);
								}
								ev->type = MKGUI_EVENT_CONTEXT;
								ev->id = hw->id;
								ev->value = row;
								ev->col = col;
								return 1;
							}

						} else if((MKGUI_HAS_RICHLIST && hw->type == MKGUI_RICHLIST)) {
							struct mkgui_richlist_data *rl = find_richlist_data(win, hw->id);
							if(rl) {
								int32_t row = richlist_row_hit(win, (uint32_t)hi, win->mouse_y);
								if(row >= 0 && rl->selected_row != row) {
									rl->selected_row = row;
									dirty_widget(win, (uint32_t)hi);
								}
								ev->type = MKGUI_EVENT_CONTEXT;
								ev->id = hw->id;
								ev->value = row;
								return 1;
							}

						} else if((MKGUI_HAS_TREEVIEW && hw->type == MKGUI_TREEVIEW)) {
							struct mkgui_treeview_data *tv = find_treeview_data(win, hw->id);
							if(tv) {
								int32_t body_y = win->rects[hi].y + 1;
								tv_idx_build(tv);
								int32_t vis_row = (win->mouse_y - body_y + tv->scroll_y) / win->row_height;
								int32_t node_id = -1;
								int32_t vis = 0;
								for(uint32_t n = 0; n < tv->node_count; ++n) {
									if(!treeview_node_visible(tv, n)) {
										continue;
									}

									if(vis == vis_row) {
										node_id = (int32_t)tv->nodes[n].id;
										break;
									}
									++vis;
								}

								if(node_id >= 0 && tv->selected_node != node_id) {
									tv->selected_node = node_id;
									dirty_widget(win, (uint32_t)hi);
								}
								ev->type = MKGUI_EVENT_CONTEXT;
								ev->id = hw->id;
								ev->value = node_id;
								ev->col = 0;
								return 1;
							}

						} else if((MKGUI_HAS_TREELIST && hw->type == MKGUI_TREELIST)) {
							struct mkgui_treelist_data *tl = find_treelist_data(win, hw->id);
							if(tl) {
								int32_t hdr = treelist_header_hit(win, (uint32_t)hi, win->mouse_x, win->mouse_y);
								if(hdr >= 0) {
									ev->type = MKGUI_EVENT_CONTEXT_HEADER;
									ev->id = hw->id;
									ev->col = (int32_t)tl->col_order[hdr];
									ev->value = win->mouse_x;
									return 1;
								}
								int32_t vis_row = treelist_row_hit(win, (uint32_t)hi, win->mouse_x, win->mouse_y);
								int32_t node_handle = -1;
								if(vis_row >= 0 && (uint32_t)vis_row < tl->visible_count) {
									node_handle = (int32_t)tl->visible[vis_row] + 1;
									if(tl->selected != node_handle) {
										tl->selected = node_handle;
										tl->multi_sel[0] = node_handle;
										tl->multi_sel_count = 1;
										dirty_widget(win, (uint32_t)hi);
									}
								}
								ev->type = MKGUI_EVENT_CONTEXT;
								ev->id = hw->id;
								ev->value = node_handle;
								ev->col = treelist_col_hit(win, (uint32_t)hi, win->mouse_x);
								return 1;
							}

						} else if((MKGUI_HAS_ITEMVIEW && hw->type == MKGUI_ITEMVIEW)) {
							struct mkgui_itemview_data *iv = find_itemview_data(win, hw->id);
							if(iv) {
								int32_t item = itemview_hit_item(win, (uint32_t)hi, iv, win->mouse_x, win->mouse_y);
								if(item >= 0 && iv->selected != item) {
									iv->selected = item;
									dirty_widget(win, (uint32_t)hi);
								}
								ev->type = MKGUI_EVENT_CONTEXT;
								ev->id = hw->id;
								ev->value = item;
								ev->col = 0;
								return 1;
							}
						}

						ev->type = MKGUI_EVENT_CONTEXT;
						ev->id = hw->id;
						ev->value = win->mouse_x;
						ev->col = win->mouse_y;
						return 1;
					}
					break;
				}

				if(pev.button == 4 || pev.button == 5) {
					if(MKGUI_HAS_CELLEDIT && win->cell_edit.active) {
						celledit_cancel(win);
					}
					int32_t hi = hit_test(win, win->mouse_x, win->mouse_y);
					int32_t delta = (pev.button == 4) ? -win->row_height * 3 : win->row_height * 3;
					if(hi >= 0 && (MKGUI_HAS_LISTVIEW && win->widgets[hi].type == MKGUI_LISTVIEW)) {
						struct mkgui_listview_data *lv = find_listv_data(win, win->widgets[hi].id);
						if(lv) {
							if((pev.keymod & MKGUI_MOD_SHIFT) || listview_hscrollbar_hit(win, (uint32_t)hi, win->mouse_x, win->mouse_y)) {
								lv->scroll_x += delta;
								int32_t content_w = win->rects[hi].w - 2 - win->scrollbar_w;
								listview_clamp_scroll_x(lv, content_w);
							} else {
								lv->scroll_y += delta;
								int32_t content_h = win->rects[hi].h - win->row_height - 2;
								int32_t content_w = win->rects[hi].w - 2 - win->scrollbar_w;
								if(listview_total_col_w(lv) > content_w) {
									content_h -= win->scrollbar_w;
								}
								int32_t max_scroll = (int32_t)lv->row_count * win->row_height - content_h;
								if(max_scroll < 0) {
									max_scroll = 0;
								}

								if(lv->scroll_y < 0) {
									lv->scroll_y = 0;
								}

								if(lv->scroll_y > max_scroll) {
									lv->scroll_y = max_scroll;
								}
							}
							dirty_widget(win, (uint32_t)hi);
						}

					} else if(hi >= 0 && (MKGUI_HAS_GRIDVIEW && win->widgets[hi].type == MKGUI_GRIDVIEW)) {
						struct mkgui_gridview_data *gv = find_gridv_data(win, win->widgets[hi].id);
						if(gv) {
							gv->scroll_y += delta;
							int32_t hh = gv->header_height > 0 ? gv->header_height : win->row_height;
							int32_t content_h = win->rects[hi].h - hh - 2;
							gridview_clamp_scroll(win, gv, content_h);
							dirty_widget(win, (uint32_t)hi);
						}

					} else if(hi >= 0 && (MKGUI_HAS_RICHLIST && win->widgets[hi].type == MKGUI_RICHLIST)) {
						struct mkgui_richlist_data *rl = find_richlist_data(win, win->widgets[hi].id);
						if(rl) {
							rl->scroll_y += delta;
							richlist_clamp_scroll(rl, win->rects[hi].h - 2);
							dirty_widget(win, (uint32_t)hi);
						}

					} else if(hi >= 0 && (MKGUI_HAS_TREEVIEW && win->widgets[hi].type == MKGUI_TREEVIEW)) {
						struct mkgui_treeview_data *tv = find_treeview_data(win, win->widgets[hi].id);
						if(tv) {
							tv->scroll_y += delta;
							treeview_clamp_scroll(win, win->widgets[hi].id);
							dirty_widget(win, (uint32_t)hi);
						}

					} else if(hi >= 0 && (MKGUI_HAS_TREELIST && win->widgets[hi].type == MKGUI_TREELIST)) {
						struct mkgui_treelist_data *tl = find_treelist_data(win, win->widgets[hi].id);
						if(tl) {
							if((pev.keymod & MKGUI_MOD_SHIFT) || treelist_hscrollbar_hit(win, (uint32_t)hi, win->mouse_x, win->mouse_y)) {
								tl->scroll_x += delta;
								int32_t content_w = win->rects[hi].w - 2 - win->scrollbar_w;
								treelist_clamp_scroll_x(tl, content_w);
							} else {
								treelist_rebuild_visible(tl);
								tl->scroll_y += delta;
								int32_t hh = tl->header_height > 0 ? tl->header_height : win->row_height;
								int32_t content_h = win->rects[hi].h - hh - 2;
								int32_t content_w = win->rects[hi].w - 2 - win->scrollbar_w;
								if(treelist_total_col_w(tl) > content_w) {
									content_h -= win->scrollbar_w;
								}
								int32_t max_scroll = (int32_t)tl->visible_count * win->row_height - content_h;
								if(max_scroll < 0) {
									max_scroll = 0;
								}

								if(tl->scroll_y < 0) {
									tl->scroll_y = 0;
								}

								if(tl->scroll_y > max_scroll) {
									tl->scroll_y = max_scroll;
								}
							}
							dirty_widget(win, (uint32_t)hi);
						}

					} else if(hi >= 0 && (MKGUI_HAS_SPINBOX && win->widgets[hi].type == MKGUI_SPINBOX)) {
						struct mkgui_spinbox_data *sd = find_spinbox_data(win, win->widgets[hi].id);
						if(sd) {
							if(sd->editing) {
								sd->editing = 0;
							}
							int32_t step = (pev.button == 4) ? sd->step : -sd->step;
							if(spinbox_adjust(win, ev, win->widgets[hi].id, step)) {
								return 1;
							}
						}

					} else if(hi >= 0 && (MKGUI_HAS_DROPDOWN && win->widgets[hi].type == MKGUI_DROPDOWN)) {
						struct mkgui_dropdown_data *dd = find_dropdown_data(win, win->widgets[hi].id);
						if(dd && dd->item_count > 0) {
							int32_t sel = dd->selected + (pev.button == 4 ? -1 : 1);
							if(sel < 0) {
								sel = 0;
							}

							if(sel >= (int32_t)dd->item_count) {
								sel = (int32_t)dd->item_count - 1;
							}

							if(sel != dd->selected) {
								dd->selected = sel;
								dirty_widget(win, (uint32_t)hi);
								ev->type = MKGUI_EVENT_DROPDOWN_CHANGED;
								ev->id = win->widgets[hi].id;
								ev->value = sel;
								return 1;
							}
						}

					} else if(hi >= 0 && (MKGUI_HAS_SLIDER && win->widgets[hi].type == MKGUI_SLIDER)) {
						struct mkgui_slider_data *sd = find_slider_data(win, win->widgets[hi].id);
						if(sd) {
							int32_t range = sd->max_val - sd->min_val;
							int32_t step = range / 20;
							if(step < 1) {
								step = 1;
							}
							uint32_t vert = (win->widgets[hi].flags & MKGUI_VERTICAL);
							sd->value += (pev.button == 4) ? (vert ? step : -step) : (vert ? -step : step);
							if(sd->value < sd->min_val) {
								sd->value = sd->min_val;
							}

							if(sd->value > sd->max_val) {
								sd->value = sd->max_val;
							}
							dirty_widget(win, (uint32_t)hi);
							ev->type = MKGUI_EVENT_SLIDER_CHANGED;
							ev->id = win->widgets[hi].id;
							ev->value = sd->value;
							return 1;
						}

					} else if(hi >= 0 && (MKGUI_HAS_TEXTAREA && win->widgets[hi].type == MKGUI_TEXTAREA)) {
						struct mkgui_textarea_data *ta = find_textarea_data(win, win->widgets[hi].id);
						if(ta) {
							ta->scroll_y += delta;
							int32_t content_h = (int32_t)textarea_line_count(ta) * win->row_height;
							int32_t view_h = win->rects[hi].h - 2;
							int32_t max_scroll = content_h - view_h;
							if(max_scroll < 0) {
								max_scroll = 0;
							}

							if(ta->scroll_y < 0) {
								ta->scroll_y = 0;
							}

							if(ta->scroll_y > max_scroll) {
								ta->scroll_y = max_scroll;
							}
							dirty_widget(win, (uint32_t)hi);
						}

					} else if(hi >= 0 && (MKGUI_HAS_LOGVIEW && win->widgets[hi].type == MKGUI_LOGVIEW)) {
						struct mkgui_logview_data *lv = find_logview_data(win, win->widgets[hi].id);
						if(lv) {
							lv->scroll_y += delta;
							int32_t content_h = logview_total_content_h(win, lv);
							int32_t view_h = win->rects[hi].h - 2;
							int32_t max_scroll = content_h - view_h;
							if(max_scroll < 0) {
								max_scroll = 0;
							}
							lv->stuck_to_end = (lv->scroll_y >= max_scroll) ? 1 : 0;
							logview_recompute(win, lv, hi);
							dirty_widget(win, (uint32_t)hi);
						}

					} else if(hi >= 0 && (MKGUI_HAS_ITEMVIEW && win->widgets[hi].type == MKGUI_ITEMVIEW)) {
						struct mkgui_itemview_data *iv = find_itemview_data(win, win->widgets[hi].id);
						if(iv) {
							iv->scroll_y += delta;
							int32_t ca_x, ca_y, ca_w, ca_h;
							itemview_content_area(win, iv, win->rects[hi].x, win->rects[hi].y, win->rects[hi].w, win->rects[hi].h, &ca_x, &ca_y, &ca_w, &ca_h);
							itemview_clamp_scroll(win, iv, ca_w, ca_h);
							dirty_widget(win, (uint32_t)hi);
						}

					} else if(hi >= 0 && (MKGUI_HAS_SCROLLBAR && win->widgets[hi].type == MKGUI_SCROLLBAR)) {
						scrollbar_scroll(win, win->widgets[hi].id, (pev.button == 4) ? -1 : 1);
						ev->type = MKGUI_EVENT_SCROLL;
						ev->id = win->widgets[hi].id;
						struct mkgui_scrollbar_data *sbd = find_scrollbar_data(win, win->widgets[hi].id);
						ev->value = sbd ? sbd->value : 0;
						return 1;

					} else if(hi >= 0 && (MKGUI_HAS_CANVAS && win->widgets[hi].type == MKGUI_CANVAS)) {
						ev->type = MKGUI_EVENT_SCROLL;
						ev->id = win->widgets[hi].id;
						ev->value = delta;
						ev->col = 0;
						return 1;

					} else {
						uint32_t sid = hi >= 0 ? win->widgets[hi].parent_id : 0;
						if(!sid) {
							for(int32_t k = (int32_t)win->widget_count - 1; k >= 0; --k) {
								if(((MKGUI_HAS_VBOX && win->widgets[k].type == MKGUI_VBOX) || (MKGUI_HAS_HBOX && win->widgets[k].type == MKGUI_HBOX)) && (win->widgets[k].flags & MKGUI_SCROLL)) {
									struct mkgui_box_scroll *bsk = find_box_scroll(win, win->widgets[k].id);
									if(bsk && (bsk->content_h > win->rects[k].h || bsk->content_w > win->rects[k].w)) {
										int32_t rx = win->rects[k].x;
										int32_t ry = win->rects[k].y;
										if(win->mouse_x >= rx && win->mouse_x < rx + win->rects[k].w && win->mouse_y >= ry && win->mouse_y < ry + win->rects[k].h) {
											sid = win->widgets[k].id;
											break;
										}
									}
								}
							}
						}
						while(sid) {
							int32_t si = find_widget_idx(win, sid);
							if(si < 0) {
								break;
							}

							if(((MKGUI_HAS_VBOX && win->widgets[si].type == MKGUI_VBOX) || (MKGUI_HAS_HBOX && win->widgets[si].type == MKGUI_HBOX)) && (win->widgets[si].flags & MKGUI_SCROLL)) {
								struct mkgui_box_scroll *bs = find_box_scroll(win, sid);
								if(bs) {
									if((MKGUI_HAS_HBOX && win->widgets[si].type == MKGUI_HBOX) && bs->content_w > win->rects[si].w) {
										int32_t view_w = win->rects[si].w;
										int32_t max_scroll = bs->content_w - view_w;
										if(max_scroll < 0) {
											max_scroll = 0;
										}
										bs->scroll_x += delta;
										if(bs->scroll_x < 0) {
											bs->scroll_x = 0;
										}

										if(bs->scroll_x > max_scroll) {
											bs->scroll_x = max_scroll;
										}
										dirty_widget(win, (uint32_t)si);
										break;
									}

									if(bs->content_h > win->rects[si].h) {
										int32_t view_h = win->rects[si].h;
										int32_t max_scroll = bs->content_h - view_h;
										if(max_scroll < 0) {
											max_scroll = 0;
										}
										bs->scroll_y += delta;
										if(bs->scroll_y < 0) {
											bs->scroll_y = 0;
										}

										if(bs->scroll_y > max_scroll) {
											bs->scroll_y = max_scroll;
										}
										dirty_widget(win, (uint32_t)si);
										break;
									}
								}
							}
							sid = win->widgets[si].parent_id;
						}
					}
					break;
				}

				if(pev.button == 6 || pev.button == 7) {
					int32_t hi = hit_test(win, win->mouse_x, win->mouse_y);
					int32_t delta = (pev.button == 6) ? -win->row_height * 3 : win->row_height * 3;
					if(hi >= 0 && (MKGUI_HAS_LISTVIEW && win->widgets[hi].type == MKGUI_LISTVIEW)) {
						struct mkgui_listview_data *lv = find_listv_data(win, win->widgets[hi].id);
						if(lv) {
							lv->scroll_x += delta;
							int32_t content_w = win->rects[hi].w - 2 - win->scrollbar_w;
							listview_clamp_scroll_x(lv, content_w);
							dirty_widget(win, (uint32_t)hi);
						}

					} else if(hi >= 0 && (MKGUI_HAS_CANVAS && win->widgets[hi].type == MKGUI_CANVAS)) {
						ev->type = MKGUI_EVENT_SCROLL;
						ev->id = win->widgets[hi].id;
						ev->value = delta;
						ev->col = 1;
						return 1;

					} else {
						uint32_t sid = hi >= 0 ? win->widgets[hi].parent_id : 0;
						while(sid) {
							int32_t si = find_widget_idx(win, sid);
							if(si < 0) {
								break;
							}

							if(((MKGUI_HAS_VBOX && win->widgets[si].type == MKGUI_VBOX) || (MKGUI_HAS_HBOX && win->widgets[si].type == MKGUI_HBOX)) && (win->widgets[si].flags & MKGUI_SCROLL)) {
								struct mkgui_box_scroll *bs = find_box_scroll(win, sid);
								if(bs && bs->content_w > win->rects[si].w) {
									int32_t max_scroll = bs->content_w - win->rects[si].w;
									if(max_scroll < 0) {
										max_scroll = 0;
									}
									bs->scroll_x += delta;
									if(bs->scroll_x < 0) {
										bs->scroll_x = 0;
									}

									if(bs->scroll_x > max_scroll) {
										bs->scroll_x = max_scroll;
									}
									dirty_widget(win, (uint32_t)si);
									break;
								}
							}
							sid = win->widgets[si].parent_id;
						}
					}
					break;
				}

				for(int32_t bi = (int32_t)win->widget_count - 1; bi >= 0; --bi) {
					struct mkgui_widget *bw = &win->widgets[bi];
					if(((MKGUI_HAS_VBOX && bw->type == MKGUI_VBOX) || (MKGUI_HAS_HBOX && bw->type == MKGUI_HBOX)) && (bw->flags & MKGUI_SCROLL)) {
						struct mkgui_box_scroll *bs = find_box_scroll(win, bw->id);
						if(!bs) {
							continue;
						}

						if((MKGUI_HAS_HBOX && bw->type == MKGUI_HBOX) && bs->content_w > win->rects[bi].w) {
							int32_t sy = win->rects[bi].y + win->rects[bi].h - win->scrollbar_w;
							int32_t sx = win->rects[bi].x;
							int32_t sw = win->rects[bi].w;
							if(win->mouse_x >= sx && win->mouse_x < sx + sw && win->mouse_y >= sy && win->mouse_y < sy + win->scrollbar_w) {
								int32_t track = sw;
								int32_t total = bs->content_w;
								if(total < 1) {
									total = 1;
								}
								int32_t thumb = track * sw / total;
								if(thumb < sc(win, 20)) {
									thumb = sc(win, 20);
								}

								if(thumb > track) {
									thumb = track;
								}
								int32_t max_scroll = bs->content_w - sw;
								if(max_scroll < 1) {
									max_scroll = 1;
								}
								int32_t pos = (int32_t)((int64_t)bs->scroll_x * (track - thumb) / max_scroll);
								if(win->mouse_x >= sx + pos && win->mouse_x < sx + pos + thumb) {
									win->drag_scrollbar_id = bw->id;
									win->drag_scrollbar_offset = win->mouse_x - (sx + pos);
								}
								break;
							}
						}

						if(bs->content_h <= win->rects[bi].h) {
							continue;
						}
						int32_t sx = win->rects[bi].x + win->rects[bi].w - win->scrollbar_w;
						int32_t sy = win->rects[bi].y;
						int32_t sh = win->rects[bi].h;
						if(win->mouse_x >= sx && win->mouse_x < sx + win->scrollbar_w && win->mouse_y >= sy && win->mouse_y < sy + sh) {
							int32_t track = sh;
							int32_t total = bs->content_h;
							if(total < 1) {
								total = 1;
							}
							int32_t thumb = track * sh / total;
							if(thumb < sc(win, 20)) {
								thumb = sc(win, 20);
							}

							if(thumb > track) {
								thumb = track;
							}
							int32_t max_scroll = bs->content_h - sh;
							if(max_scroll < 1) {
								max_scroll = 1;
							}
							int32_t pos = (int32_t)((int64_t)bs->scroll_y * (track - thumb) / max_scroll);
							if(pos < 0) {
								pos = 0;
							}

							if(pos > track - thumb) {
								pos = track - thumb;
							}
							int32_t thumb_y = sy + pos;
							if(win->mouse_y >= thumb_y && win->mouse_y < thumb_y + thumb) {
								win->drag_scrollbar_id = bw->id;
								win->drag_scrollbar_offset = win->mouse_y - thumb_y;
							} else {
								int32_t dir = win->mouse_y < thumb_y ? -1 : 1;
								int32_t step = sh / 2;
								bs->scroll_y += dir * step;
								if(bs->scroll_y < 0) {
									bs->scroll_y = 0;
								}

								if(bs->scroll_y > max_scroll) {
									bs->scroll_y = max_scroll;
								}
							}
							dirty_widget(win, (uint32_t)bi);
							break;
						}
					}
				}

				if(win->drag_scrollbar_id) {
					break;
				}

				int32_t hi = hit_test(win, win->mouse_x, win->mouse_y);
				if(hi >= 0) {
					struct mkgui_widget *hw = &win->widgets[hi];

					if(hw->flags & MKGUI_DISABLED) {
						break;
					}

					if(is_focusable(hw)) {
						spinbox_focus_lost(win);
						if(win->focus_id != hw->id) {
							pathbar_focus_lost(win);
						}
						win->focus_id = hw->id;

					} else {
						spinbox_focus_lost(win);
						pathbar_focus_lost(win);
						win->focus_id = 0;
					}

					win->press_id = hw->id;
					win->press_mod = pev.keymod;
					dirty_all(win);

					if(MKGUI_HAS_CANVAS && hw->type == MKGUI_CANVAS) {
						ev->type = MKGUI_EVENT_CANVAS_PRESS;
						ev->id = hw->id;
						ev->value = pev.x;
						ev->col = pev.y;
						ev->keysym = pev.button;
						ev->keymod = pev.keymod;
						return 1;

					}

					if((MKGUI_HAS_INPUT && hw->type == MKGUI_INPUT)) {
						handle_input_click(win, (uint32_t)hi);
					}

					if((MKGUI_HAS_IPINPUT && hw->type == MKGUI_IPINPUT)) {
						handle_ipinput_click(win, hw->id, win->mouse_x);
					}

					if((MKGUI_HAS_TOGGLE && hw->type == MKGUI_TOGGLE)) {
						handle_toggle_click(win, ev, hw->id);
						return ev->type != MKGUI_EVENT_NONE;
					}

					if((MKGUI_HAS_COMBOBOX && hw->type == MKGUI_COMBOBOX)) {
						if(closed_combobox_id == hw->id) {
							int32_t btn_x = win->rects[hi].x + win->rects[hi].w - sc(win, 24);
							if(win->mouse_x < btn_x) {
								handle_combobox_click(win, ev, hw->id);
							}
						} else {
							handle_combobox_click(win, ev, hw->id);
						}
					}

					if((MKGUI_HAS_DATEPICKER && hw->type == MKGUI_DATEPICKER)) {
						handle_datepicker_click(win, hw->id);
					}

					if((MKGUI_HAS_MENUITEM && hw->type == MKGUI_MENUITEM)) {
						struct mkgui_widget *parent = find_widget(win, hw->parent_id);
						if(parent && (MKGUI_HAS_MENU && parent->type == MKGUI_MENU)) {
							menu_open_popup(win, hw->id);
						}
					}

					if((MKGUI_HAS_DROPDOWN && hw->type == MKGUI_DROPDOWN)) {
						if(closed_dropdown_id != hw->id) {
							dropdown_open_popup(win, hw->id);
						}
					}

					// Uniform (win, ev, idx) press handlers dispatch through the table.
					if(press_desc[hw->type].fn) {
						if(press_desc[hw->type].fn(win, ev, (uint32_t)hi)) {
							return press_desc[hw->type].discrete ? 1 : (ev->type != MKGUI_EVENT_NONE);
						}
					}

				} else {
					spinbox_focus_lost(win);
					pathbar_focus_lost(win);
					win->focus_id = 0;
				}
			} break;

			// -=[ BUTTON RELEASE ]=-
			case MKWIN_EV_BUTTON_RELEASE: {
				// buttons 4+ are scroll wheel; their "release" is meaningless
				// and must not trigger drag completion or click logic
				if(pev.button >= 4) {
					break;
				}

				if(win->canvas_window && popup_idx < 0) {
					for(uint32_t ci = 0; (MKGUI_HAS_CANVAS) && ci < win->canvas_count; ++ci) {
						ev->type = MKGUI_EVENT_CANVAS_RELEASE;
						ev->id = win->canvases[ci].widget_id;
						ev->value = pev.x;
						ev->col = pev.y;
						ev->keysym = pev.button;
						ev->keymod = pev.keymod;
						return 1;
					}
					break;
				}

				win->cell_edit.dragging = 0;

				for(uint32_t si = 0; (MKGUI_HAS_SPINBOX) && si < win->spinbox_count; ++si) {
					win->spinboxes[si].repeat_dir = 0;
				}

				for(uint32_t tvi = 0; (MKGUI_HAS_TREEVIEW) && tvi < win->treeview_count; ++tvi) {
					struct mkgui_treeview_data *tv = &win->treeviews[tvi];
					if(tv->drag_active && tv->drag_target >= 0) {
						int32_t src_id = (int32_t)tv->nodes[tv->drag_source].id;
						int32_t tgt_id = (int32_t)tv->nodes[tv->drag_target].id;
						int32_t pos = tv->drag_pos;
						tv->drag_active = 0;
						tv->drag_source = -1;
						tv->drag_target = -1;
						dirty_all(win);
						ev->type = MKGUI_EVENT_TREEVIEW_MOVE;
						ev->id = tv->widget_id;
						ev->value = src_id;
						ev->col = tgt_id;
						ev->keysym = (uint32_t)pos;
						return 1;
					}

					if(tv->drag_active) {
						ev->type = MKGUI_EVENT_DRAG_END;
						ev->id = tv->widget_id;
						dirty_widget_id(win, tv->widget_id);
					}
					tv->drag_active = 0;
					tv->drag_source = -1;
					tv->drag_target = -1;
				}
				for(uint32_t lvi = 0; (MKGUI_HAS_LISTVIEW) && lvi < win->listv_count; ++lvi) {
					struct mkgui_listview_data *lv = &win->listvs[lvi];
					if(lv->drag_active && lv->drag_target >= 0) {
						int32_t src = lv->drag_source;
						int32_t tgt = lv->drag_target;
						lv->drag_active = 0;
						lv->drag_source = -1;
						lv->drag_target = -1;
						dirty_all(win);
						ev->type = MKGUI_EVENT_LISTVIEW_REORDER;
						ev->id = lv->widget_id;
						ev->value = src;
						ev->col = tgt;
						return 1;
					}

					if(lv->drag_active) {
						ev->type = MKGUI_EVENT_DRAG_END;
						ev->id = lv->widget_id;
						dirty_widget_id(win, lv->widget_id);
					}
					lv->drag_active = 0;
					lv->drag_source = -1;
					lv->drag_target = -1;
				}
				for(uint32_t gvi = 0; (MKGUI_HAS_GRIDVIEW) && gvi < win->gridview_count; ++gvi) {
					struct mkgui_gridview_data *gv = &win->gridviews[gvi];
					if(gv->drag_active && gv->drag_target >= 0) {
						int32_t src = gv->drag_source;
						int32_t tgt = gv->drag_target;
						gv->drag_active = 0;
						gv->drag_source = -1;
						gv->drag_target = -1;
						dirty_all(win);
						ev->type = MKGUI_EVENT_GRIDVIEW_REORDER;
						ev->id = gv->widget_id;
						ev->value = src;
						ev->col = tgt;
						return 1;
					}

					if(gv->drag_active) {
						ev->type = MKGUI_EVENT_DRAG_END;
						ev->id = gv->widget_id;
						dirty_widget_id(win, gv->widget_id);
					}
					gv->drag_active = 0;
					gv->drag_source = -1;
					gv->drag_target = -1;
				}

				win->drag_select_id = 0;

				if(win->drag_text_id) {
					uint32_t dtid = win->drag_text_id;
					uint32_t drop = win->drag_text_drop_pos;
					win->drag_text_id = 0;
					struct mkgui_textarea_data *ta = find_textarea_data(win, dtid);
					if(ta && textarea_has_selection(ta)) {
						uint32_t lo, hi2;
						textarea_sel_range(ta, &lo, &hi2);
						if(drop < lo || drop >= hi2) {
							textarea_undo_push_force(ta);
							uint32_t sel_len = hi2 - lo;
							char *tmp = (char *)malloc(sel_len);
							if(tmp) {
								memcpy(tmp, &ta->text[lo], sel_len);
								if(drop > hi2) {
									memmove(&ta->text[lo], &ta->text[hi2], drop - hi2);
									memcpy(&ta->text[drop - sel_len], tmp, sel_len);
									ta->cursor = drop;
									ta->sel_start = drop - sel_len;
									ta->sel_end = drop;
								} else {
									memmove(&ta->text[drop + sel_len], &ta->text[drop], lo - drop);
									memcpy(&ta->text[drop], tmp, sel_len);
									ta->cursor = drop + sel_len;
									ta->sel_start = drop;
									ta->sel_end = drop + sel_len;
								}
								free(tmp);
								ev->type = MKGUI_EVENT_TEXTAREA_CHANGED;
								ev->id = dtid;
								dirty_widget_id(win, dtid);
								return 1;
							}
						} else {
							ta->cursor = drop;
							textarea_clear_selection(ta);
						}
					}
					dirty_widget_id(win, dtid);
					break;
				}

				if(win->drag_col_resize_id) {
					uint32_t dcr_id = win->drag_col_resize_id;
					win->drag_col_resize_id = 0;
					dirty_widget_id(win, dcr_id);
					break;
				}

				if(win->drag_col_id) {
					uint32_t dcid = win->drag_col_id;
					int32_t src = win->drag_col_src;
					int32_t dx = pev.x - win->drag_col_start_x;
					win->drag_col_id = 0;
					win->drag_col_src = -1;
					dirty_widget_id(win, dcid);

					struct mkgui_listview_data *lv = find_listv_data(win, dcid);
					if(lv) {
						if(dx > -4 && dx < 4) {
							uint32_t logical = lv->col_order[src];
							if(lv->sort_col == (int32_t)logical) {
								lv->sort_dir = -lv->sort_dir;

							} else {
								lv->sort_col = (int32_t)logical;
								lv->sort_dir = 1;
							}
							ev->type = MKGUI_EVENT_LISTVIEW_SORT;
							ev->id = dcid;
							ev->col = (int32_t)logical;
							ev->value = lv->sort_dir;
							return 1;

						} else {
							int32_t widx = find_widget_idx(win, dcid);
							if(widx >= 0) {
								int32_t dst = listview_col_insert_pos(win, lv, (uint32_t)widx);
								if(dst != src && dst != src + 1) {
									uint32_t grabbed = lv->col_order[src];
									if(dst > src) {
										memmove(&lv->col_order[src], &lv->col_order[src + 1], (uint32_t)(dst - src - 1) * sizeof(uint32_t));
										lv->col_order[dst - 1] = grabbed;
									} else {
										memmove(&lv->col_order[dst + 1], &lv->col_order[dst], (uint32_t)(src - dst) * sizeof(uint32_t));
										lv->col_order[dst] = grabbed;
									}
									ev->type = MKGUI_EVENT_LISTVIEW_COL_REORDER;
									ev->id = dcid;
									return 1;
								}
							}
						}
					}

					struct mkgui_treelist_data *tld = lv ? NULL : find_treelist_data(win, dcid);
					if(tld) {
						if(dx > -4 && dx < 4) {
							uint32_t logical = tld->col_order[src];
							if(tld->sort_col == (int32_t)logical) {
								tld->sort_dir = -tld->sort_dir;

							} else {
								tld->sort_col = (int32_t)logical;
								tld->sort_dir = 1;
							}
							treelist_do_sort(tld);
							ev->type = MKGUI_EVENT_TREELIST_SORT;
							ev->id = dcid;
							ev->col = (int32_t)logical;
							ev->value = tld->sort_dir;
							return 1;

						} else {
							int32_t widx = find_widget_idx(win, dcid);
							if(widx >= 0) {
								int32_t dst = treelist_col_insert_pos(win, tld, (uint32_t)widx);
								if(dst != src && dst != src + 1) {
									uint32_t grabbed = tld->col_order[src];
									if(dst > src) {
										memmove(&tld->col_order[src], &tld->col_order[src + 1], (uint32_t)(dst - src - 1) * sizeof(uint32_t));
										tld->col_order[dst - 1] = grabbed;
									} else {
										memmove(&tld->col_order[dst + 1], &tld->col_order[dst], (uint32_t)(src - dst) * sizeof(uint32_t));
										tld->col_order[dst] = grabbed;
									}
									ev->type = MKGUI_EVENT_TREELIST_COL_REORDER;
									ev->id = dcid;
									return 1;
								}
							}
						}
					}
					break;
				}

				uint32_t was_press = win->press_id;
				win->press_id = 0;
				if(win->drag_scrollbar_id) {
					int32_t dsi = find_widget_idx(win, win->drag_scrollbar_id);
					if(dsi >= 0 && (MKGUI_HAS_SCROLLBAR && win->widgets[dsi].type == MKGUI_SCROLLBAR)) {
						struct mkgui_scrollbar_data *sbd = find_scrollbar_data(win, win->drag_scrollbar_id);
						ev->type = MKGUI_EVENT_SCROLL;
						ev->id = win->drag_scrollbar_id;
						ev->value = sbd ? sbd->value : 0;
						win->drag_scrollbar_id = 0;
						dirty_widget(win, (uint32_t)dsi);
						return 1;
					}
				}
				win->drag_scrollbar_id = 0;
				if(was_press) {
					dirty_widget_id(win, was_press);
				}

				if(was_press) {
					int32_t pi = find_widget_idx(win, was_press);
					if(pi >= 0) {
						if(release_desc[win->widgets[pi].type]) {
							return release_desc[win->widgets[pi].type](win, ev, was_press);
						}
					}

					// click fires only when release lands on the same widget
					// that received the press; dragging off and releasing
					// elsewhere cancels the interaction
					int32_t hi = hit_test(win, pev.x, pev.y);
					if(hi >= 0 && win->widgets[hi].id == was_press) {
						struct mkgui_widget *hw = &win->widgets[hi];

						if(click_desc[hw->type].fn) {
							uint32_t r = click_desc[hw->type].fn(win, ev, hw);
							if(click_desc[hw->type].direct) {
								return r;
							}
							if(r) {
								return 1;
							}
						}
					}
				}
			} break;

			// -=[ KEY ]=-
			case MKWIN_EV_KEY: {
				uint32_t ks = pev.keysym;

				if(win->popup_count > 0 && win->popups[win->popup_count - 1].widget_id == MKGUI_CTXMENU_POPUP_ID) {
					struct mkgui_popup *cp = &win->popups[win->popup_count - 1];
					if(ks == MKGUI_KEY_ESCAPE) {
						popup_destroy_all(win);
						dirty_all(win);
						break;
					}

					if(ks == MKGUI_KEY_UP) {
						int32_t next = ctxmenu_next_item(win, cp->hover_item >= 0 ? cp->hover_item : (int32_t)win->ctxmenu_count, -1);
						if(next != cp->hover_item) {
							cp->hover_item = next;
							cp->dirty = 1;
							dirty_all(win);
						}
						break;
					}

					if(ks == MKGUI_KEY_DOWN) {
						int32_t next = ctxmenu_next_item(win, cp->hover_item, 1);
						if(next != cp->hover_item) {
							cp->hover_item = next;
							cp->dirty = 1;
							dirty_all(win);
						}
						break;
					}

					if(ks == MKGUI_KEY_RETURN || ks == MKGUI_KEY_SPACE) {
						if(cp->hover_item >= 0) {
							struct mkgui_ctxmenu_item *it = ctxmenu_item_at(win, cp->hover_item);
							if(it && !(it->flags & MKGUI_DISABLED)) {
								if(it->flags & MKGUI_MENUITEM_CHECK) {
									it->flags ^= MKGUI_MENUITEM_CHECKED;
								} else if(it->flags & MKGUI_MENUITEM_RADIO) {
									for(uint32_t ri = 0; ri < win->ctxmenu_count; ++ri) {
										win->ctxmenu_items[ri].flags &= ~MKGUI_MENUITEM_CHECKED;
									}
									it->flags |= MKGUI_MENUITEM_CHECKED;
								}
								ev->type = MKGUI_EVENT_CONTEXT_MENU;
								ev->id = it->id;
								ev->value = (it->flags & MKGUI_MENUITEM_CHECKED) ? 1 : 0;
								popup_destroy_all(win);
								dirty_all(win);
								return 1;
							}
						}
						break;
					}
					break;
				}

				if(ks == MKGUI_KEY_ESCAPE && win->popup_count > 0) {
					popup_destroy(win, win->popup_count - 1);
					dirty_all(win);
					break;
				}

				// accelerators run before the focused widget so global
				// shortcuts (ctrl+s, ctrl+q, etc.) are never eaten by
				// a text input that would otherwise consume the keypress
				if(accel_dispatch(win, ks, pev.keymod, ev)) {
					return 1;
				}

				if(ks == MKGUI_KEY_TAB || ks == MKGUI_KEY_ISO_LEFT_TAB) {
					uint32_t reverse = (pev.keymod & MKGUI_MOD_SHIFT) || (ks == MKGUI_KEY_ISO_LEFT_TAB);
					uint32_t start_idx = 0;
					if(win->focus_id) {
						for(uint32_t i = 0; i < win->widget_count; ++i) {
							if(win->widgets[i].id == win->focus_id) {
								start_idx = reverse ? (i + win->widget_count - 1) : (i + 1);
								break;
							}
						}
					}
					for(uint32_t i = 0; i < win->widget_count; ++i) {
						uint32_t idx;
						if(reverse) {
							idx = (start_idx + win->widget_count - i) % win->widget_count;
						} else {
							idx = (start_idx + i) % win->widget_count;
						}

						if(widget_visible(win, idx) && is_focusable(&win->widgets[idx])) {
							spinbox_focus_lost(win);
					pathbar_focus_lost(win);
							win->focus_id = win->widgets[idx].id;
							dirty_all(win);
							break;
						}
					}
					break;
				}

				if(MKGUI_HAS_CELLEDIT && win->cell_edit.active) {
					if(celledit_key(win, ev, ks, pev.keymod, pev.text, pev.text_len)) {
						return ev->type != MKGUI_EVENT_NONE;
					}
				}

				if((pev.keymod & 4) && win->focus_id) {
					struct mkgui_widget *fw = find_widget(win, win->focus_id);
					if(fw && (ks == 'z' || ks == 'Z') && !(pev.keymod & MKGUI_MOD_SHIFT)) {
						if((MKGUI_HAS_INPUT && fw->type == MKGUI_INPUT)) {
							struct mkgui_input_data *inp = find_input_data(win, win->focus_id);
							if(inp && input_undo(inp)) {
								dirty_widget_id(win, win->focus_id);
								input_scroll_to_cursor(win, win->focus_id);
								ev->type = MKGUI_EVENT_INPUT_CHANGED;
								ev->id = win->focus_id;
								return 1;
							}

						} else if((MKGUI_HAS_TEXTAREA && fw->type == MKGUI_TEXTAREA)) {
							struct mkgui_textarea_data *ta = find_textarea_data(win, win->focus_id);
							if(ta && textarea_undo(ta)) {
								dirty_widget_id(win, win->focus_id);
								textarea_scroll_to_cursor(win, win->focus_id);
								ev->type = MKGUI_EVENT_TEXTAREA_CHANGED;
								ev->id = win->focus_id;
								return 1;
							}
						}
						break;
					}

					if(fw && (ks == 'y' || ks == 'Y' || ((ks == 'z' || ks == 'Z') && (pev.keymod & MKGUI_MOD_SHIFT)))) {
						if((MKGUI_HAS_INPUT && fw->type == MKGUI_INPUT)) {
							struct mkgui_input_data *inp = find_input_data(win, win->focus_id);
							if(inp && input_redo(inp)) {
								dirty_widget_id(win, win->focus_id);
								input_scroll_to_cursor(win, win->focus_id);
								ev->type = MKGUI_EVENT_INPUT_CHANGED;
								ev->id = win->focus_id;
								return 1;
							}

						} else if((MKGUI_HAS_TEXTAREA && fw->type == MKGUI_TEXTAREA)) {
							struct mkgui_textarea_data *ta = find_textarea_data(win, win->focus_id);
							if(ta && textarea_redo(ta)) {
								dirty_widget_id(win, win->focus_id);
								textarea_scroll_to_cursor(win, win->focus_id);
								ev->type = MKGUI_EVENT_TEXTAREA_CHANGED;
								ev->id = win->focus_id;
								return 1;
							}
						}
						break;
					}

					if(fw && (ks == 'a' || ks == 'A')) {
						if((MKGUI_HAS_INPUT && fw->type == MKGUI_INPUT)) {
							struct mkgui_input_data *inp = find_input_data(win, win->focus_id);
							if(inp) {
								inp->sel_start = 0;
								inp->sel_end = (uint32_t)strlen(inp->text);
								inp->cursor = inp->sel_end;
								dirty_widget_id(win, win->focus_id);
								input_scroll_to_cursor(win, win->focus_id);
							}

						} else if((MKGUI_HAS_COMBOBOX && fw->type == MKGUI_COMBOBOX)) {
							struct mkgui_combobox_data *cb = find_combobox_data(win, win->focus_id);
							if(cb) {
								combobox_select_all(cb);
								dirty_widget_id(win, win->focus_id);
							}

						} else if((MKGUI_HAS_PATHBAR && fw->type == MKGUI_PATHBAR)) {
							struct mkgui_pathbar_data *pb = find_pathbar_data(win, win->focus_id);
							if(pb && pb->editing) {
								struct mkgui_text_edit te = {pb->edit_buf, (uint32_t)sizeof(pb->edit_buf), pb->edit_cursor, pb->edit_sel_start, pb->edit_sel_end, pb->edit_scroll_x};
								textedit_select_all(&te);
								pb->edit_cursor = te.cursor;
								pb->edit_sel_start = te.sel_start;
								pb->edit_sel_end = te.sel_end;
								dirty_widget_id(win, win->focus_id);
							}

						} else if((MKGUI_HAS_TEXTAREA && fw->type == MKGUI_TEXTAREA)) {
							struct mkgui_textarea_data *ta = find_textarea_data(win, win->focus_id);
							if(ta) {
								ta->sel_start = 0;
								ta->sel_end = ta->text_len;
								ta->cursor = ta->text_len;
								dirty_widget_id(win, win->focus_id);
							}

						} else if((MKGUI_HAS_LOGVIEW && fw->type == MKGUI_LOGVIEW)) {
							struct mkgui_logview_data *lv = find_logview_data(win, win->focus_id);
							if(lv && lv->line_seq_head > lv->line_seq_tail) {
								struct mkgui_logview_line *last = logview_get_line(lv, lv->line_seq_head - 1);
								lv->sel_anchor_line_seq = lv->line_seq_tail;
								lv->sel_anchor_off = 0;
								lv->sel_cursor_line_seq = lv->line_seq_head - 1;
								lv->sel_cursor_off = last ? last->len : 0;
								lv->has_selection = 1;
								dirty_widget_id(win, win->focus_id);
							}
						}
						break;
					}

					if(fw && (ks == 'c' || ks == 'C')) {
						if((MKGUI_HAS_INPUT && fw->type == MKGUI_INPUT) && !(fw->style & MKGUI_INPUT_PASSWORD)) {
							struct mkgui_input_data *inp = find_input_data(win, win->focus_id);
							if(inp) {
								if(input_has_selection(inp)) {
									uint32_t lo, hi;
									input_sel_range(inp, &lo, &hi);
									mkwin_clipboard_set(win->mkwin, inp->text + lo, hi - lo);
								} else {
									mkwin_clipboard_set(win->mkwin, inp->text, (uint32_t)strlen(inp->text));
								}
							}

						} else if((MKGUI_HAS_TEXTAREA && fw->type == MKGUI_TEXTAREA)) {
							struct mkgui_textarea_data *ta = find_textarea_data(win, win->focus_id);
							if(ta) {
								if(textarea_has_selection(ta)) {
									uint32_t lo, hi;
									textarea_sel_range(ta, &lo, &hi);
									mkwin_clipboard_set(win->mkwin, ta->text + lo, hi - lo);
								} else {
									mkwin_clipboard_set(win->mkwin, ta->text, ta->text_len);
								}
							}

						} else if((MKGUI_HAS_LOGVIEW && fw->type == MKGUI_LOGVIEW)) {
							struct mkgui_logview_data *lv = find_logview_data(win, win->focus_id);
							if(lv && lv->has_selection) {
								uint64_t lo_seq, hi_seq;
								uint32_t lo_off, hi_off;
								if(logview_sel_range(lv, &lo_seq, &lo_off, &hi_seq, &hi_off)) {
									uint32_t total = 0;
									for(uint64_t s = lo_seq; s <= hi_seq; ++s) {
										struct mkgui_logview_line *line = logview_get_line(lv, s);
										if(line) {
											uint32_t a = (s == lo_seq) ? lo_off : 0;
											uint32_t b = (s == hi_seq) ? hi_off : line->len;
											total += (b > a ? b - a : 0) + (s < hi_seq ? 1 : 0);
										}
									}
									char *buf = (char *)malloc(total + 1);
									if(buf) {
										uint32_t written = mkgui_logview_get_selection_text(win, win->focus_id, buf, total + 1);
										mkwin_clipboard_set(win->mkwin, buf, written);
										free(buf);
									}
								}
							}

						} else if((MKGUI_HAS_PATHBAR && fw->type == MKGUI_PATHBAR)) {
							struct mkgui_pathbar_data *pb = find_pathbar_data(win, win->focus_id);
							if(pb && pb->editing) {
								if(pb->edit_sel_start != pb->edit_sel_end) {
									uint32_t lo = pb->edit_sel_start < pb->edit_sel_end ? pb->edit_sel_start : pb->edit_sel_end;
									uint32_t hi = pb->edit_sel_start < pb->edit_sel_end ? pb->edit_sel_end : pb->edit_sel_start;
									mkwin_clipboard_set(win->mkwin, pb->edit_buf + lo, hi - lo);
								} else {
									mkwin_clipboard_set(win->mkwin, pb->edit_buf, (uint32_t)strlen(pb->edit_buf));
								}
							}

						} else if((MKGUI_HAS_COMBOBOX && fw->type == MKGUI_COMBOBOX)) {
							struct mkgui_combobox_data *cb = find_combobox_data(win, win->focus_id);
							if(cb) {
								if(combobox_has_selection(cb)) {
									uint32_t lo = cb->sel_start < cb->sel_end ? cb->sel_start : cb->sel_end;
									uint32_t hi = cb->sel_start < cb->sel_end ? cb->sel_end : cb->sel_start;
									mkwin_clipboard_set(win->mkwin, cb->text + lo, hi - lo);
								} else {
									mkwin_clipboard_set(win->mkwin, cb->text, (uint32_t)strlen(cb->text));
								}
							}
						}
						break;
					}

					if(fw && (ks == 'x' || ks == 'X')) {
						if((MKGUI_HAS_INPUT && fw->type == MKGUI_INPUT) && !(fw->style & MKGUI_INPUT_READONLY) && !(fw->style & MKGUI_INPUT_PASSWORD)) {
							struct mkgui_input_data *inp = find_input_data(win, win->focus_id);
							if(inp && input_has_selection(inp)) {
								input_undo_push_force(inp);
								uint32_t lo, hi;
								input_sel_range(inp, &lo, &hi);
								mkwin_clipboard_set(win->mkwin, inp->text + lo, hi - lo);
								input_delete_selection(inp);
								dirty_widget_id(win, win->focus_id);
								input_scroll_to_cursor(win, win->focus_id);
								ev->type = MKGUI_EVENT_INPUT_CHANGED;
								ev->id = win->focus_id;
								return 1;
							}

						} else if((MKGUI_HAS_TEXTAREA && fw->type == MKGUI_TEXTAREA) && !(fw->style & MKGUI_TEXTAREA_READONLY)) {
							struct mkgui_textarea_data *ta = find_textarea_data(win, win->focus_id);
							if(ta && textarea_has_selection(ta)) {
								textarea_undo_push_force(ta);
								uint32_t lo, hi;
								textarea_sel_range(ta, &lo, &hi);
								mkwin_clipboard_set(win->mkwin, ta->text + lo, hi - lo);
								textarea_delete_selection(ta);
								dirty_widget_id(win, win->focus_id);
								textarea_scroll_to_cursor(win, win->focus_id);
								ev->type = MKGUI_EVENT_TEXTAREA_CHANGED;
								ev->id = win->focus_id;
								return 1;
							}

						} else if((MKGUI_HAS_PATHBAR && fw->type == MKGUI_PATHBAR)) {
							struct mkgui_pathbar_data *pb = find_pathbar_data(win, win->focus_id);
							if(pb && pb->editing && pb->edit_sel_start != pb->edit_sel_end) {
								uint32_t lo = pb->edit_sel_start < pb->edit_sel_end ? pb->edit_sel_start : pb->edit_sel_end;
								uint32_t hi = pb->edit_sel_start < pb->edit_sel_end ? pb->edit_sel_end : pb->edit_sel_start;
								mkwin_clipboard_set(win->mkwin, pb->edit_buf + lo, hi - lo);
								struct mkgui_text_edit te = {pb->edit_buf, (uint32_t)sizeof(pb->edit_buf), pb->edit_cursor, pb->edit_sel_start, pb->edit_sel_end, pb->edit_scroll_x};
								textedit_delete_selection(&te);
								pb->edit_cursor = te.cursor;
								pb->edit_sel_start = te.sel_start;
								pb->edit_sel_end = te.sel_end;
								dirty_widget_id(win, win->focus_id);
								pathbar_scroll_to_cursor(win, pb);
							}

						} else if((MKGUI_HAS_COMBOBOX && fw->type == MKGUI_COMBOBOX)) {
							struct mkgui_combobox_data *cb = find_combobox_data(win, win->focus_id);
							if(cb && combobox_has_selection(cb)) {
								uint32_t lo = cb->sel_start < cb->sel_end ? cb->sel_start : cb->sel_end;
								uint32_t hi = cb->sel_start < cb->sel_end ? cb->sel_end : cb->sel_start;
								mkwin_clipboard_set(win->mkwin, cb->text + lo, hi - lo);
								combobox_delete_selection(cb);
								cb->selected = -1;
								{ size_t _l = strlen(cb->text); if(_l >= MKGUI_MAX_TEXT) { _l = MKGUI_MAX_TEXT - 1; } memcpy(cb->prev_text, cb->text, _l); cb->prev_text[_l] = '\0'; }
								combobox_open_popup(win, win->focus_id, 1);
								ev->type = MKGUI_EVENT_COMBOBOX_CHANGED;
								ev->id = win->focus_id;
								dirty_widget_id(win, win->focus_id);
								combobox_scroll_to_cursor(win, win->focus_id);
								return 1;
							}
						}
						break;
					}

					if(fw && (ks == 'v' || ks == 'V')) {
						if((MKGUI_HAS_INPUT && fw->type == MKGUI_INPUT) && !(fw->style & MKGUI_INPUT_READONLY)) {
							uint32_t clip_len = 0;
							char *clip_buf = mkwin_clipboard_get(win->mkwin, &clip_len);
							if(clip_buf && clip_len > 0) {
								struct mkgui_input_data *inp = find_input_data(win, win->focus_id);
								if(inp) {
									input_undo_push_force(inp);
									if(input_has_selection(inp)) {
										input_delete_selection(inp);
									}
									for(uint32_t ci = 0; ci < clip_len; ++ci) {
										if(clip_buf[ci] == '\n' || clip_buf[ci] == '\r') {
											clip_buf[ci] = ' ';
										}
									}
									uint32_t text_len = (uint32_t)strlen(inp->text);
									if(text_len + clip_len < MKGUI_MAX_TEXT - 1) {
										memmove(&inp->text[inp->cursor + clip_len], &inp->text[inp->cursor], text_len - inp->cursor + 1);
										memcpy(&inp->text[inp->cursor], clip_buf, clip_len);
										inp->cursor += clip_len;
										input_clear_selection(inp);
										dirty_widget_id(win, win->focus_id);
										input_scroll_to_cursor(win, win->focus_id);
										ev->type = MKGUI_EVENT_INPUT_CHANGED;
										ev->id = win->focus_id;
										free(clip_buf);
										return 1;
									}
								}
							}
							free(clip_buf);

						} else if((MKGUI_HAS_TEXTAREA && fw->type == MKGUI_TEXTAREA) && !(fw->style & MKGUI_TEXTAREA_READONLY)) {
							uint32_t clip_len = 0;
							char *clip_buf = mkwin_clipboard_get(win->mkwin, &clip_len);
							if(clip_buf && clip_len > 0) {
								struct mkgui_textarea_data *ta = find_textarea_data(win, win->focus_id);
								if(ta) {
									for(uint32_t ci = 0; ci < clip_len; ++ci) {
										if(clip_buf[ci] == '\r') {
											memmove(&clip_buf[ci], &clip_buf[ci + 1], clip_len - ci - 1);
											--clip_len;
											--ci;
										}
									}
									if(!textarea_ensure_cap(ta, (size_t)ta->text_len + clip_len + 1)) {
										free(clip_buf);
										return 0;
									}
									textarea_undo_push_force(ta);
									if(textarea_has_selection(ta)) {
										textarea_delete_selection(ta);
									}
									memmove(&ta->text[ta->cursor + clip_len], &ta->text[ta->cursor], ta->text_len - ta->cursor);
									memcpy(&ta->text[ta->cursor], clip_buf, clip_len);
									ta->cursor += clip_len;
									ta->text_len += clip_len;
									ta->text[ta->text_len] = '\0';
									textarea_clear_selection(ta);
									dirty_widget_id(win, win->focus_id);
									textarea_scroll_to_cursor(win, win->focus_id);
									ev->type = MKGUI_EVENT_TEXTAREA_CHANGED;
									ev->id = win->focus_id;
									free(clip_buf);
									return 1;
								}
							}
							free(clip_buf);

						} else if((MKGUI_HAS_PATHBAR && fw->type == MKGUI_PATHBAR)) {
							struct mkgui_pathbar_data *pb = find_pathbar_data(win, win->focus_id);
							if(pb && pb->editing) {
								uint32_t clip_len = 0;
								char *clip_buf = mkwin_clipboard_get(win->mkwin, &clip_len);
								if(clip_buf && clip_len > 0) {
									for(uint32_t ci = 0; ci < clip_len; ++ci) {
										if(clip_buf[ci] == '\n' || clip_buf[ci] == '\r') {
											clip_buf[ci] = ' ';
										}
									}
									struct mkgui_text_edit te = {pb->edit_buf, (uint32_t)sizeof(pb->edit_buf), pb->edit_cursor, pb->edit_sel_start, pb->edit_sel_end, pb->edit_scroll_x};
									if(textedit_insert(&te, clip_buf, clip_len)) {
										pb->edit_cursor = te.cursor;
										pb->edit_sel_start = te.sel_start;
										pb->edit_sel_end = te.sel_end;
										dirty_widget_id(win, win->focus_id);
										pathbar_scroll_to_cursor(win, pb);
									}
								}
								free(clip_buf);
							}

						} else if((MKGUI_HAS_COMBOBOX && fw->type == MKGUI_COMBOBOX)) {
							uint32_t clip_len = 0;
							char *clip_buf = mkwin_clipboard_get(win->mkwin, &clip_len);
							if(clip_buf && clip_len > 0) {
								struct mkgui_combobox_data *cb = find_combobox_data(win, win->focus_id);
								if(cb) {
									for(uint32_t ci = 0; ci < clip_len; ++ci) {
										if(clip_buf[ci] == '\n' || clip_buf[ci] == '\r') {
											clip_buf[ci] = ' ';
										}
									}
									struct mkgui_text_edit te = {cb->text, MKGUI_MAX_TEXT, cb->cursor, cb->sel_start, cb->sel_end, cb->scroll_x};
									if(textedit_insert(&te, clip_buf, clip_len)) {
										cb->cursor = te.cursor;
										cb->sel_start = te.sel_start;
										cb->sel_end = te.sel_end;
										cb->selected = -1;
										{ size_t _l = strlen(cb->text); if(_l >= MKGUI_MAX_TEXT) { _l = MKGUI_MAX_TEXT - 1; } memcpy(cb->prev_text, cb->text, _l); cb->prev_text[_l] = '\0'; }
										combobox_open_popup(win, win->focus_id, 1);
										ev->type = MKGUI_EVENT_COMBOBOX_CHANGED;
										ev->id = win->focus_id;
										dirty_widget_id(win, win->focus_id);
										combobox_scroll_to_cursor(win, win->focus_id);
										free(clip_buf);
										return 1;
									}
								}
							}
							free(clip_buf);
						}
						break;
					}
				}

				if(win->focus_id) {
					struct mkgui_widget *fw = find_widget(win, win->focus_id);
					if(fw) {
						if(ks == MKGUI_KEY_F2 && ((MKGUI_HAS_TREEVIEW && fw->type == MKGUI_TREEVIEW) || (MKGUI_HAS_LISTVIEW && fw->type == MKGUI_LISTVIEW) || (MKGUI_HAS_GRIDVIEW && fw->type == MKGUI_GRIDVIEW) || (MKGUI_HAS_ITEMVIEW && fw->type == MKGUI_ITEMVIEW))) {
							if((MKGUI_HAS_TREEVIEW && fw->type == MKGUI_TREEVIEW)) {
								struct mkgui_treeview_data *tv = find_treeview_data(win, win->focus_id);
								if(tv && tv->selected_node >= 0) {
									for(uint32_t ni = 0; ni < tv->node_count; ++ni) {
										if((int32_t)tv->nodes[ni].id == tv->selected_node) {
											celledit_begin(win, win->focus_id, tv->selected_node, 0, tv->nodes[ni].label);
											break;
										}
									}
									return 0;
								}
							} else if((MKGUI_HAS_LISTVIEW && fw->type == MKGUI_LISTVIEW)) {
								struct mkgui_listview_data *lv = find_listv_data(win, win->focus_id);
								if(lv && lv->selected_row >= 0) {
									char cell_buf[MKGUI_MAX_TEXT] = {0};
									if(lv->row_cb) {
										lv->row_cb((uint32_t)lv->selected_row, 0, cell_buf, MKGUI_MAX_TEXT, lv->userdata);
									}
									celledit_begin(win, win->focus_id, lv->selected_row, 0, cell_buf);
									return 0;
								}
							} else if((MKGUI_HAS_GRIDVIEW && fw->type == MKGUI_GRIDVIEW)) {
								struct mkgui_gridview_data *gv = find_gridv_data(win, win->focus_id);
								if(gv && gv->selected_row >= 0 && gv->selected_col >= 0) {
									char cell_buf[MKGUI_MAX_TEXT] = {0};
									if(gv->cell_cb) {
										gv->cell_cb((uint32_t)gv->selected_row, (uint32_t)gv->selected_col, cell_buf, MKGUI_MAX_TEXT, gv->userdata);
									}
									celledit_begin(win, win->focus_id, gv->selected_row, gv->selected_col, cell_buf);
									return 0;
								}
							} else if((MKGUI_HAS_ITEMVIEW && fw->type == MKGUI_ITEMVIEW)) {
								struct mkgui_itemview_data *iv = find_itemview_data(win, win->focus_id);
								if(iv && iv->selected >= 0) {
									char label[MKGUI_MAX_TEXT] = {0};
									if(iv->label_cb) {
										iv->label_cb((uint32_t)iv->selected, label, MKGUI_MAX_TEXT, iv->userdata);
									}
									celledit_begin(win, win->focus_id, iv->selected, 0, label);
									return 0;
								}
							}
						}
						if(key_desc[fw->type]) {
							if(key_desc[fw->type](win, ev, ks, pev.keymod, pev.text, pev.text_len)) {
								return 1;
							}
						}
						}
					}

				ev->type = MKGUI_EVENT_KEY;
				ev->keysym = ks;
				ev->keymod = pev.keymod;
				return 1;
			} break;

			// -=[ CLOSE ]=-
			case MKWIN_EV_CLOSE: {
				ev->type = MKGUI_EVENT_CLOSE;
				if(win->hide_on_close) {
					if(win->window_visible) {
						mkwin_unmap(win->mkwin);
						win->window_visible = 0;
					}
				} else {
					win->should_close = 1;
				}
				return 1;
			} break;

			// -=[ FOCUS OUT ]=-
			case MKWIN_EV_FOCUS_OUT: {
				if(win->popup_count > 0) {
					popup_destroy_all(win);
					dirty_all(win);
				}
			} break;

			// -=[ LEAVE ]=-
			case MKWIN_EV_LEAVE: {
				if(win->hover_id) {
					dirty_widget_id(win, win->hover_id);
					win->hover_id = 0;
				}
				tooltip_update(win, 0, 0, 0);
				if(popup_idx >= 0) {
					uint32_t has_child_popup = (uint32_t)popup_idx + 1 < win->popup_count;
					if(!has_child_popup) {
						win->popups[popup_idx].hover_item = -1;
						win->popups[popup_idx].dirty = 1;
					}
				}
				for(uint32_t pbi = 0; (MKGUI_HAS_PATHBAR) && pbi < win->pathbar_count; ++pbi) {
					if(win->pathbars[pbi].hover_seg >= 0) {
						win->pathbars[pbi].hover_seg = -1;
						dirty_widget_id(win, win->pathbars[pbi].widget_id);
					}
				}
			} break;

			// -=[ DROP ]=-
			case MKWIN_EV_DROP: {
				uint32_t dc = mkwin_drop_count(win->mkwin);
				if(dc > 0) {
					ev->type = MKGUI_EVENT_FILE_DROP;
					ev->id = 0;
					ev->value = (int32_t)dc;
					return 1;
				}
			} break;

			default: {
			} break;
		}
	}

	if(win->anim_active) {
		for(uint32_t i = 0; i < win->widget_count; ++i) {
			struct mkgui_widget *w = &win->widgets[i];
			if(!widget_visible(win, i)) {
				continue;
			}

			if((MKGUI_HAS_SPINNER && w->type == MKGUI_SPINNER) || (MKGUI_HAS_GLVIEW && w->type == MKGUI_GLVIEW)) {
				dirty_widget(win, i);
			}

			if((MKGUI_HAS_PROGRESS && w->type == MKGUI_PROGRESS) && (w->style & MKGUI_PROGRESS_SHIMMER)) {
				struct mkgui_progress_data *pd = find_progress_data(win, w->id);
				if(pd && pd->value > 0 && pd->value < pd->max_val) {
					dirty_widget(win, i);
				}
			}
		}
	}

	if(win->tooltip_id && !win->tooltip_shown) {
		uint32_t elapsed = (uint32_t)(mkgui_now_ns() / 1000000ull) - win->tooltip_start_ms;
		if(elapsed >= MKGUI_TOOLTIP_DELAY_MS) {
			dirty_all(win);
		}
	}

	return ev->type != MKGUI_EVENT_NONE;
}

// [=]===^=[ mkgui_frame_begin ]===================================[=]
static void mkgui_frame_begin(struct mkgui_window *win) {
	struct mkgui_rect current_rects[32];
	uint32_t current_count = win->dirty_count;
	uint32_t current_full = win->dirty_full || win->render_cb || win->canvas_window;
	uint32_t age = mkwin_begin_frame(win->mkwin);
	memcpy(current_rects, win->dirty_rects, current_count * sizeof(*current_rects));

	if(age == 0 || age > 2 || (age == 2 && win->previous_dirty_full)) {
		dirty_all(win);

	} else if(age == 2) {
		for(uint32_t i = 0; i < win->previous_dirty_count; ++i) {
			struct mkgui_rect *rect = &win->previous_dirty_rects[i];
			dirty_add(win, rect->x, rect->y, rect->w, rect->h);

		}
	}

	memcpy(win->previous_dirty_rects, current_rects, current_count * sizeof(*current_rects));
	win->previous_dirty_count = current_count;
	win->previous_dirty_full = current_full;

}

// [=]===^=[ mkgui_frame_present ]=================================[=]
static void mkgui_frame_present(struct mkgui_window *win) {
	if(win->previous_dirty_full) {
		mkwin_present(win->mkwin);

	} else {
		for(uint32_t i = 0; i < win->previous_dirty_count; ++i) {
			struct mkgui_rect *rect = &win->previous_dirty_rects[i];
			mkwin_present_region(win->mkwin, rect->x, rect->y, rect->w, rect->h);

		}
	}
}

// [=]===^=[ mkgui_flush ]========================================[=]
static void mkgui_flush(struct mkgui_window *win) {
	if(win->dirty) {
		double t0 = (double)mkgui_now_ns() * 1e-3;
		layout_widgets(win);
		if(MKGUI_HAS_GLVIEW) {
			glview_sync_all(win);

		}
		double t1 = (double)mkgui_now_ns() * 1e-3;
		mkgui_frame_begin(win);
		double render_start = (double)mkgui_now_ns() * 1e-3;
		render_widgets(win);

		if(win->render_cb) {
			flush_text(win);
			win->render_cb(win, win->render_cb_data);

		}

		flush_text(win);
		render_notify(win);
		flush_text(win);
		render_tooltip(win);
		flush_text(win);
		double t2 = ((double)mkgui_now_ns() * 1e-3);

		mkgui_frame_present(win);
		double t3 = ((double)mkgui_now_ns() * 1e-3);
		win->perf_layout_us = t1 - t0;
		win->perf_acquire_us = render_start - t1;
		win->perf_render_us = t2 - render_start;
		win->perf_blit_us = t3 - t2;

		for(uint32_t pi = 0; pi < win->popup_count; ++pi) {
			struct mkgui_popup *p = &win->popups[pi];

			if(!p->active || !p->dirty) {
				continue;

			}

			if(p->widget_id == MKGUI_CTXMENU_POPUP_ID) {
				render_ctxmenu_popup(win, p, p->hover_item);
				flush_text_popup(win, p);
				mkwin_popup_present(p->mkwin);

			} else {
				struct mkgui_widget *pw = find_widget(win, p->widget_id);

				if(pw && (MKGUI_HAS_DROPDOWN && pw->type == MKGUI_DROPDOWN)) {
					struct mkgui_dropdown_data *dd = find_dropdown_data(win, pw->id);

					if(dd) {
						render_dropdown_popup(win, p, dd, p->hover_item);
						flush_text_popup(win, p);
						mkwin_popup_present(p->mkwin);

					}
				} else if(pw && (MKGUI_HAS_COMBOBOX && pw->type == MKGUI_COMBOBOX)) {
					struct mkgui_combobox_data *cb = find_combobox_data(win, pw->id);

					if(cb) {
						render_combobox_popup(win, p, cb, p->hover_item);
						flush_text_popup(win, p);
						mkwin_popup_present(p->mkwin);

					}
				} else if(pw && (MKGUI_HAS_DATEPICKER && pw->type == MKGUI_DATEPICKER)) {
					struct mkgui_datepicker_data *dp = find_datepicker_data(win, pw->id);

					if(dp) {
						int32_t hover_day = dp->cal_hover > 0 ? dp->cal_hover : -1;
						render_datepicker_popup(win, p, dp, hover_day);
						flush_text_popup(win, p);
						mkwin_popup_present(p->mkwin);

					}
				} else if(pw && (MKGUI_HAS_MENUITEM && pw->type == MKGUI_MENUITEM)) {
					render_menu_popup(win, p, pw->id, p->hover_item);
					flush_text_popup(win, p);
					mkwin_popup_present(p->mkwin);

				}
			}

			p->dirty = 0;

		}

		mkwin_flush(win->mkwin);
		win->dirty = 0;
		win->dirty_full = 0;
		win->dirty_count = 0;

	}

	if(win->parent) {
		struct mkgui_window *p = win->parent;
#ifdef _WIN32
		int64_t pnow;
		QueryPerformanceCounter((LARGE_INTEGER *)&pnow);
		double p_dt = (double)(pnow - p->anim_prev) / (double)p->perf_freq;
		p->anim_prev = pnow;
#else
		struct timespec pnow;
		clock_gettime(CLOCK_MONOTONIC, &pnow);
		double p_dt = (double)(pnow.tv_sec - p->anim_prev.tv_sec) + (double)(pnow.tv_nsec - p->anim_prev.tv_nsec) * 1e-9;
		p->anim_prev = pnow;
#endif
		p->anim_time += p_dt;
		p->anim_active = 0;

		for(uint32_t i = 0; i < p->widget_count; ++i) {
			struct mkgui_widget *w = &p->widgets[i];

			if(!widget_visible(p, i)) {
				continue;

			}

			if((MKGUI_HAS_SPINNER && w->type == MKGUI_SPINNER) || (MKGUI_HAS_GLVIEW && w->type == MKGUI_GLVIEW)) {
				p->anim_active = 1;
				dirty_widget(p, i);

			}

			if((MKGUI_HAS_PROGRESS && w->type == MKGUI_PROGRESS) && (w->style & MKGUI_PROGRESS_SHIMMER)) {
				struct mkgui_progress_data *pd = find_progress_data(p, w->id);

				if(pd && pd->value > 0 && pd->value < pd->max_val) {
					p->anim_active = 1;
					dirty_widget(p, i);

				}
			}
		}

		if(p->anim_active && !win->anim_active) {
			win->anim_active = 1;

		}

		if(p->dirty) {
			layout_widgets(p);
			if(MKGUI_HAS_GLVIEW) {
				glview_sync_all(p);

			}
			mkgui_frame_begin(p);
			render_widgets(p);

			if(p->render_cb) {
				flush_text(p);
				p->render_cb(p, p->render_cb_data);

			}

			flush_text(p);
			render_notify(p);
			flush_text(p);
			render_tooltip(p);
			flush_text(p);
			mkgui_frame_present(p);
			mkwin_flush(p->mkwin);
			p->dirty = 0;
			p->dirty_full = 0;
			p->dirty_count = 0;

		}
	}
}

// [=]===^=[ mkgui_ctx_wait_until ]===================================[=]
// Block until: an event arrives, an animation/tooltip/toast wakes up, OR
// the caller's deadline (absolute ns from mkgui_now_ns) is reached -
// whichever fires first. deadline_ns == 0 means no caller deadline; the
// library's own deadlines still apply.
MKGUI_API void mkgui_ctx_wait_until(struct mkgui_ctx *ctx, uint64_t deadline_ns) {
	MKGUI_CHECK(ctx);
	for(uint32_t i = 0; i < app_ctx->window_count; ++i) {
		mkgui_flush(app_ctx->windows[i]);
	}

	if(ctx->should_quit || ctx->window_count == 0) {
		return;
	}
	uint32_t any_anim = 0;
	for(uint32_t i = 0; i < app_ctx->window_count; ++i) {
		struct mkgui_window *c = app_ctx->windows[i];
		if(c->anim_active || c->dirty) {
			any_anim = 1;
			break;
		}
	}

	uint64_t now = mkgui_now_ns();
	uint64_t target = 0;
	if(any_anim) {
		target = now + 16000000ull;
	}
	if(deadline_ns) {
		if(!target || deadline_ns < target) {
			target = deadline_ns;
		}
	}
	for(uint32_t i = 0; i < app_ctx->window_count; ++i) {
		struct mkgui_window *c = app_ctx->windows[i];
		if(c->tooltip_id && !c->tooltip_shown) {
			uint32_t elapsed = (uint32_t)(mkgui_now_ns() / 1000000ull) - c->tooltip_start_ms;
			if(elapsed < MKGUI_TOOLTIP_DELAY_MS) {
				uint64_t tt = now + (uint64_t)(MKGUI_TOOLTIP_DELAY_MS - elapsed) * 1000000ull;
				if(!target || tt < target) {
					target = tt;
				}
			}
		}
#ifdef _WIN32
		FILETIME file_time;
		ULARGE_INTEGER wall_time;
		GetSystemTimeAsFileTime(&file_time);
		wall_time.LowPart = file_time.dwLowDateTime;
		wall_time.HighPart = file_time.dwHighDateTime;

		for(uint32_t ti = 0; ti < c->timer_count; ++ti) {
			struct mkgui_timer *timer = &c->timers[ti];

			if(timer->active) {
				uint64_t due = timer_due(timer);
				uint64_t ticks = due > wall_time.QuadPart ? due - wall_time.QuadPart : 0;
				uint64_t deadline = ticks > (UINT64_MAX - now) / 100 ? UINT64_MAX : now + ticks * 100;

				if(!target || deadline < target) {
					target = deadline;

				}
			}
		}
#endif

		int32_t next_toast = toasts_next_expiry_ms(c);
		if(next_toast >= 0) {
			uint64_t tt = now + (uint64_t)(uint32_t)next_toast * 1000000ull;
			if(!target || tt < target) {
				target = tt;
			}
		}
	}

	int64_t timeout_ns;
	if(!target) {
		timeout_ns = -1;
	} else if(target <= now) {
		timeout_ns = 0;
	} else {
		timeout_ns = (int64_t)(target - now);
	}
	// platform_wait_event blocks on the shared X fd / Win32 queue. Any
	// window works as the anchor; use the primary.
	struct mkgui_window *anchor = ctx->primary ? ctx->primary : ctx->windows[0];
	mkwin_wait(anchor->mkwin, timeout_ns);
}

// [=]===^=[ mkgui_ctx_wait ]=========================================[=]
MKGUI_API void mkgui_ctx_wait(struct mkgui_ctx *ctx) {
	mkgui_ctx_wait_until(ctx, 0);
}

// [=]===^=[ mkgui_ctx_should_quit ]==================================[=]
MKGUI_API uint32_t mkgui_ctx_should_quit(struct mkgui_ctx *ctx) {
	MKGUI_CHECK_VAL(ctx, 1);
	return ctx->should_quit;
}

// [=]===^=[ mkgui_window_add_timer ]=====================================[=]
MKGUI_API uint32_t mkgui_window_add_timer(struct mkgui_window *win, uint64_t interval_ns, mkgui_timer_cb cb, void *userdata) {
	MKGUI_CHECK_VAL(win, 0);

	if(interval_ns == 0 || win->timer_count >= MKGUI_MAX_TIMERS) {
		return 0;

	}

	struct mkgui_timer *timer = &win->timers[win->timer_count];
	memset(timer, 0, sizeof(*timer));
	timer->id = ++win->timer_next_id;
	timer->interval_ns = interval_ns;
	timer->cb = cb;
	timer->userdata = userdata;
	timer->active = 1;
#ifdef _WIN32
	timer->handle = CreateWaitableTimerW(NULL, FALSE, NULL);

	if(timer->handle) {
		FILETIME file_time;
		GetSystemTimeAsFileTime(&file_time);
		timer->epoch.LowPart = file_time.dwLowDateTime;
		timer->epoch.HighPart = (LONG)file_time.dwHighDateTime;
		LARGE_INTEGER due;
		due.QuadPart = -(int64_t)(interval_ns / 100 + (interval_ns % 100 != 0));

		if(SetWaitableTimer(timer->handle, &due, 0, NULL, NULL, FALSE)) {
			++win->timer_count;
			return timer->id;

		}

		CloseHandle(timer->handle);

	}
#else
	timer->fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

	if(timer->fd >= 0) {
		struct itimerspec interval = {0};
		interval.it_value.tv_sec = (time_t)(interval_ns / 1000000000);
		interval.it_value.tv_nsec = (long)(interval_ns % 1000000000);
		interval.it_interval = interval.it_value;

		if(timerfd_settime(timer->fd, 0, &interval, NULL) == 0) {
			mkwin_wait_fd_add(timer->fd);
			++win->timer_count;
			return timer->id;

		}

		close(timer->fd);

	}
#endif

	memset(timer, 0, sizeof(*timer));
	return 0;

}

// [=]===^=[ mkgui_window_remove_timer ]==================================[=]
MKGUI_API void mkgui_window_remove_timer(struct mkgui_window *win, uint32_t timer_id) {
	MKGUI_CHECK(win);
	for(uint32_t i = 0; i < win->timer_count; ++i) {
		if(win->timers[i].id == timer_id) {
#ifdef _WIN32
			if(win->timers[i].handle) {
				CancelWaitableTimer(win->timers[i].handle);
				CloseHandle(win->timers[i].handle);
			}
#else
			if(win->timers[i].fd >= 0) {
				mkwin_wait_fd_remove(win->timers[i].fd);
				close(win->timers[i].fd);
			}
#endif
			win->timers[i] = win->timers[--win->timer_count];
			return;
		}
	}
}

// [=]===^=[ mkgui_window_set_callback ]=================================[=]
MKGUI_API void mkgui_window_set_callback(struct mkgui_window *win, mkgui_event_cb cb, void *userdata) {
	MKGUI_CHECK(win);
	win->event_cb = cb;
	win->event_cb_data = userdata;
}

// [=]===^=[ mkgui_window_get_ctx ]=================================[=]
MKGUI_API struct mkgui_ctx *mkgui_window_get_ctx(struct mkgui_window *win) {
	MKGUI_CHECK_VAL(win, NULL);
	return win->ctx;
}

// [=]===^=[ mkgui_ctx_run ]===========================================[=]
// Run the event loop until: mkgui_ctx_quit is called, the ctx's primary
// window has should_close set, OR no registered window is currently
// visible. cb is invoked for the primary window's events; other windows
// dispatch through their own per-window callbacks (mkgui_window_set_callback).
MKGUI_API void mkgui_ctx_run(struct mkgui_ctx *ctx, mkgui_event_cb cb, void *userdata) {
	MKGUI_CHECK(ctx);
	struct mkgui_window *primary = ctx->primary;
	for(;;) {
		// Reap windows that closed themselves last iteration, before any poll
		// walks the registry -- this is the only safe point to free a window
		// that self-destroyed from its own callback.
		mkgui_ctx_reap_closed(ctx);

		uint32_t any_visible = 0;
		for(uint32_t i = 0; i < app_ctx->window_count; ++i) {
			if(app_ctx->windows[i]->window_visible) {
				any_visible = 1;
				break;
			}
		}
		if(ctx->should_quit || (primary && (primary->should_close || primary->destroy_pending)) || !any_visible) {
			break;
		}

		struct mkgui_event ev;
		if(primary) {
			while(mkgui_window_poll(primary, &ev)) {
				if(cb) {
					cb(primary, &ev, userdata);
				}
			}
		}
		for(uint32_t i = 0; i < app_ctx->window_count; ++i) {
			struct mkgui_window *c = app_ctx->windows[i];
			if(c == primary || c->should_close || c->destroy_pending || !c->event_cb) {
				continue;
			}
			while(mkgui_window_poll(c, &ev)) {
				c->event_cb(c, &ev, c->event_cb_data);
			}
		}

		any_visible = 0;
		for(uint32_t i = 0; i < app_ctx->window_count; ++i) {
			if(app_ctx->windows[i]->window_visible) {
				any_visible = 1;
				break;
			}
		}
		if(ctx->should_quit || (primary && (primary->should_close || primary->destroy_pending)) || !any_visible) {
			break;
		}
		mkgui_ctx_wait(ctx);
	}
}

// [=]===^=[ mkgui_ctx_quit ]==========================================[=]
MKGUI_API void mkgui_ctx_quit(struct mkgui_ctx *ctx) {
	MKGUI_CHECK(ctx);
	ctx->should_quit = 1;
}

// [=]===^=[ mkgui_ctx_set_quit ]======================================[=]
MKGUI_API void mkgui_ctx_set_quit(struct mkgui_ctx *ctx, uint32_t value) {
	MKGUI_CHECK(ctx);
	ctx->should_quit = value ? 1 : 0;
}

// [=]===^=[ mkgui_ctx_pump_others ]==================================[=]
// Drain queued events from every window on this ctx other than the primary,
// dispatching each through the window's registered callback. Use this in a
// host-owned loop after pumping the primary window. Windows without a
// callback (mkgui_window_set_callback) are skipped silently.
MKGUI_API void mkgui_ctx_pump_others(struct mkgui_ctx *ctx) {
	MKGUI_CHECK(ctx);
	struct mkgui_event ev;
	for(uint32_t i = 0; i < ctx->window_count; ++i) {
		struct mkgui_window *c = ctx->windows[i];
		if(c == ctx->primary || c->should_close || c->destroy_pending || !c->event_cb) {
			continue;
		}
		while(mkgui_window_poll(c, &ev)) {
			c->event_cb(c, &ev, c->event_cb_data);
		}
	}
}

// [=]===^=[ mkgui_ctx_reap_closed ]==================================[=]
// Destroy every non-primary window flagged by mkgui_window_close. Call it
// once per iteration at a point where nothing is polling a window (the top of
// the loop, outside every mkgui_window_poll). mkgui_ctx_run does this itself;
// host-owned loops must call it themselves. The primary is never reaped here --
// closing it ends the loop and mkgui_ctx_destroy frees it.
MKGUI_API void mkgui_ctx_reap_closed(struct mkgui_ctx *ctx) {
	MKGUI_CHECK(ctx);
	uint32_t i = 0;
	while(i < ctx->window_count) {
		struct mkgui_window *c = ctx->windows[i];
		if(c->destroy_pending && c != ctx->primary) {
			// mkgui_window_destroy swaps the last window into slot i and
			// decrements the count, so re-examine slot i without advancing.
			mkgui_window_destroy(c);
		} else {
			++i;
		}
	}
}

// [=]===^=[ mkgui_window_set_close ]==================================[=]
MKGUI_API void mkgui_window_set_close(struct mkgui_window *win, uint32_t value) {
	MKGUI_CHECK(win);
	win->should_close = value ? 1 : 0;
}

// [=]===^=[ mkgui_window_should_close ]===============================[=]
MKGUI_API uint32_t mkgui_window_should_close(struct mkgui_window *win) {
	MKGUI_CHECK_VAL(win, 1);
	return win->should_close;
}

// [=]===^=[ mkgui_window_close ]======================================[=]
// Request deferred destruction. Only flags it; the actual destroy happens
// later at a point where no poll is walking the window (mkgui_ctx_reap_closed).
// This makes it safe for a window to close itself from its own callback, where
// an immediate destroy would free the window mid-iteration. Flagging the
// primary ends mkgui_ctx_run; the primary is freed by mkgui_ctx_destroy.
MKGUI_API void mkgui_window_close(struct mkgui_window *win) {
	MKGUI_CHECK(win);
	win->destroy_pending = 1;
}

// [=]===^=[ mkgui_window_show ]===================================[=]
MKGUI_API void mkgui_window_show(struct mkgui_window *win) {
	MKGUI_CHECK(win);
	if(win->window_visible) {
		return;
	}
	mkwin_map(win->mkwin);
	win->window_visible = 1;
	dirty_all(win);
}

// [=]===^=[ mkgui_window_hide ]===================================[=]
MKGUI_API void mkgui_window_hide(struct mkgui_window *win) {
	MKGUI_CHECK(win);
	if(!win->window_visible) {
		return;
	}
	mkwin_unmap(win->mkwin);
	win->window_visible = 0;
}

// [=]===^=[ mkgui_window_is_visible ]=============================[=]
MKGUI_API uint32_t mkgui_window_is_visible(struct mkgui_window *win) {
	MKGUI_CHECK_VAL(win, 0);
	return win->window_visible;
}

// [=]===^=[ mkgui_window_move ]====================================[=]
MKGUI_API void mkgui_window_move(struct mkgui_window *win, int32_t x, int32_t y) {
	MKGUI_CHECK(win);
	mkwin_move(win->mkwin, x, y);
}

// [=]===^=[ mkgui_window_get_position ]============================[=]
MKGUI_API void mkgui_window_get_position(struct mkgui_window *win, int32_t *x, int32_t *y) {
	MKGUI_CHECK(win);
	MKGUI_CHECK(x);
	MKGUI_CHECK(y);
	mkwin_position(win->mkwin, x, y);
}

// [=]===^=[ mkgui_window_begin_drag ]==============================[=]
MKGUI_API void mkgui_window_begin_drag(struct mkgui_window *win) {
	MKGUI_CHECK(win);
	mkwin_begin_drag(win->mkwin);
}

// [=]===^=[ mkgui_window_set_shape ]===============================[=]
MKGUI_API void mkgui_window_set_shape(struct mkgui_window *win, const int32_t *xy_pairs, uint32_t point_count) {
	MKGUI_CHECK(win);
	MKGUI_CHECK(xy_pairs);
	if(point_count < 3) {
		return;
	}
	mkwin_set_shape(win->mkwin, xy_pairs, point_count);
}

// [=]===^=[ mkgui_window_set_shape_mask ]=========================[=]
MKGUI_API void mkgui_window_set_shape_mask(struct mkgui_window *win, const uint32_t *argb, int32_t w, int32_t h, uint32_t alpha_threshold) {
	MKGUI_CHECK(win);
	MKGUI_CHECK(argb);
	if(w <= 0 || h <= 0) {
		return;
	}
	mkwin_set_shape_mask(win->mkwin, argb, w, h, alpha_threshold);
}

// [=]===^=[ mkgui_window_clear_shape ]=============================[=]
MKGUI_API void mkgui_window_clear_shape(struct mkgui_window *win) {
	MKGUI_CHECK(win);
	mkwin_clear_shape(win->mkwin);
}

// [=]===^=[ mkgui_window_resize ]==================================[=]
MKGUI_API void mkgui_window_resize(struct mkgui_window *win, int32_t w, int32_t h) {
	MKGUI_CHECK(win);
	if(w <= 0 || h <= 0) {
		return;
	}
	mkwin_resize(win->mkwin, w, h);
}

// [=]===^=[ mkgui_window_invalidate ]=====================================[=]
MKGUI_API void mkgui_window_invalidate(struct mkgui_window *win) {
	MKGUI_CHECK(win);
	dirty_all(win);
}

// ---------------------------------------------------------------------------
// Dialog windows
// ---------------------------------------------------------------------------

#if MKGUI_HAS_DIALOGS
#include "mkgui_dialogs.c"
#endif
#if MKGUI_HAS_FILEDIALOG
#include "mkgui_filedialog.c"
#endif
#if MKGUI_HAS_ICONBROWSER
#include "mkgui_iconbrowser.c"
#endif
#if MKGUI_HAS_COLORPICKER
#include "mkgui_colorpicker.c"
#endif

#if defined(MKGUI_LIBRARY) && !defined(MKGUI_STATIC)
#include "mkgui_editor_api.c"
#endif
