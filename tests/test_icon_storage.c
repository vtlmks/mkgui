// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static uint32_t fail_heap;
static uint32_t fail_map;
static uint32_t fail_commit;

// [=]===^=[ storage_malloc ]===================================================================[=]
static void *storage_malloc(size_t size) {
	if(fail_heap && --fail_heap == 0) {
		return NULL;

	}

	return malloc(size);

}

// [=]===^=[ storage_calloc ]===================================================================[=]
static void *storage_calloc(size_t count, size_t size) {
	if(fail_heap && --fail_heap == 0) {
		return NULL;

	}

	return calloc(count, size);

}

#ifdef _WIN32
// [=]===^=[ storage_virtual_alloc ]============================================================[=]
static LPVOID WINAPI storage_virtual_alloc(LPVOID address, SIZE_T size, DWORD allocation_type, DWORD protection) {
	if(((allocation_type & MEM_RESERVE) && fail_map && --fail_map == 0) || ((allocation_type & MEM_COMMIT) && fail_commit && --fail_commit == 0)) {
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return NULL;

	}

	return VirtualAlloc(address, size, allocation_type, protection);

}
#define VirtualAlloc storage_virtual_alloc
#else
// [=]===^=[ storage_mmap ]=====================================================================[=]
static void *storage_mmap(void *address, size_t length, int protection, int flags, int descriptor, off_t offset) {
	if(fail_map && --fail_map == 0) {
		errno = ENOMEM;
		return MAP_FAILED;

	}

	return mmap(address, length, protection, flags, descriptor, offset);

}

// [=]===^=[ storage_mprotect ]=================================================================[=]
static int storage_mprotect(void *address, size_t length, int protection) {
	if(fail_commit && --fail_commit == 0) {
		errno = ENOMEM;
		return -1;

	}

	return mprotect(address, length, protection);

}
#define mmap storage_mmap
#define mprotect storage_mprotect
#endif

#define malloc storage_malloc
#define calloc storage_calloc
#include "../mkgui.c"
#undef malloc
#undef calloc
#undef mmap
#undef mprotect
#undef VirtualAlloc

static uint32_t checks;
static uint32_t failures;

// [=]===^=[ check ]============================================================================[=]
static void check(uint32_t condition, char *message) {
	++checks;

	if(!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		++failures;

	}
}

// [=]===^=[ test_raster_growth ]===============================================================[=]
static void test_raster_growth(void) {
	uint32_t pixel = 0xff123456;
	char name[64];
	mkgui_icon_init();

	for(uint32_t i = 0; i < 33000; ++i) {
		snprintf(name, sizeof(name), "custom-%u", i);
		int32_t index = mkgui_icon_add(name, &pixel, 1, 1);
		check(index == (int32_t)(i + 1), "raster icon index changed during growth");

		if(index < 0) {
			break;

		}
	}

	check(icon_count == 33001, "cache still has the old 32768-icon limit");

	for(uint32_t i = 0; i < 33000 && failures == 0; ++i) {
		snprintf(name, sizeof(name), "custom-%u", i);
		int32_t index = icon_find_idx_at(name, 0);
		check(index == (int32_t)(i + 1), "rehashing lost a raster icon");
		check(index >= 0 && icon_atlas[icons[index].atlas_offset] == pixel, "atlas growth lost pixels");

	}

	uint32_t count = icon_count;
	pixel = 0xffabcdef;
	check(mkgui_icon_add("custom-0", &pixel, 1, 1) == 1 && icon_count == count, "replacement changed the icon index");
	check(icon_atlas[icons[1].atlas_offset] == pixel, "replacement did not update pixels");
	svg_cleanup();
	check(!icons && !icon_hash && !icon_arena.base && !icon_hash_arena.base && !icon_atlas, "raster arenas survived cleanup");

}

// [=]===^=[ test_svg_growth ]==================================================================[=]
static void test_svg_growth(char *path) {
	static struct mkgui_window win;
	char name[64];
	FILE *file = fopen(path, "w");
	check(file != NULL, "cannot create custom SVG fixture");

	if(file) {
		fputs("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1\" height=\"1\"><path fill=\"#123456\" d=\"M0 0H1V1H0Z\"/></svg>", file);
		fclose(file);
		win.icon_size = 1;
		win.theme.text = 0xffffffff;
		mkgui_icon_init();

		for(uint32_t i = 0; i < 4200; ++i) {
			snprintf(name, sizeof(name), "svg-%u", i);
			check(mkgui_icon_load_svg(&win, name, path) >= 0, "custom SVG source limit remains");

		}

		check(svg_source_count == 4200, "SVG records were lost during growth");
		int32_t previous = icon_find_idx_at("svg-0", 1);
		uint32_t count = icon_count;
		check(mkgui_icon_load_svg(&win, "svg-0", path) == previous, "SVG replacement changed the icon index");
		check(svg_source_count == 4200 && icon_count == count, "SVG replacement added duplicate records");
		check(icon_resolve_at(&win, "svg-4199", 2) >= 0, "grown SVG source cannot render at another size");
		svg_rerasterize_all(&win, 2.0f);
		check(icon_find_idx_at("svg-0", 2) == previous, "SVG scale change lost the stable index");
		svg_cleanup();
		check(!svg_sources && !svg_source_arena.base && !svg_source_count, "SVG source arena survived cleanup");
		remove(path);

	}
}

// [=]===^=[ test_allocation_failures ]=========================================================[=]
static void test_allocation_failures(void) {
	uint32_t pixel = 0xff123456;
	fail_map = 2;
	check(mkgui_icon_add("reserve-fails", &pixel, 1, 1) == -1 && icon_count == 0, "hash reserve failure published an icon");
	svg_cleanup();
	fail_commit = 1;
	check(mkgui_icon_add("commit-fails", &pixel, 1, 1) == -1 && icon_count == 0, "commit failure published an icon");
	svg_cleanup();
	mkgui_icon_init();
	int32_t index = mkgui_icon_add("existing", &pixel, 1, 1);
	check(index >= 0, "cannot create allocation-failure fixture");
	uint32_t count = icon_count;
	uint32_t replacement = 0xffabcdef;
	uint32_t *atlas = icon_atlas;
	fail_heap = 2;
	check(mkgui_icon_add("existing", &replacement, 1, 1) == -1, "atlas replacement failure was not reported");
	check(icon_count == count && icon_atlas == atlas && icon_atlas[icons[index].atlas_offset] == pixel, "failed replacement damaged cached pixels");
	fail_heap = 2;
	check(mkgui_icon_add("new", &replacement, 1, 1) == -1, "atlas insertion failure was not reported");
	check(icon_count == count && icon_find_idx_at("new", 0) == -1, "failed insertion left a cache entry");
	check(mkgui_icon_add("oversized", &pixel, INT32_MAX, INT32_MAX) == -1 && icon_count == count, "oversized raster was accepted");
	svg_cleanup();

}

// [=]===^=[ test_large_glyphs ]================================================================[=]
static void test_large_glyphs(void) {
	struct mkgui_widget widget = MKGUI_W(MKGUI_WINDOW, 1, "Glyph storage test", "", 0, 300, 200, 0, 0, 0);
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	check(ctx != NULL, "glyph test context");

	if(ctx) {
		uint32_t pixel = 0xff123456;
		int32_t preloaded = mkgui_icon_add("preloaded", &pixel, 1, 1);
		check(preloaded >= 0, "cannot preload an icon before the first window");
		struct mkgui_window *win = mkgui_window_create(ctx, NULL, &widget, 1, NULL, 0, 0);
		check(win != NULL, "glyph test window");

		if(win) {
			check(icon_find_idx_at("preloaded", 0) == preloaded, "first window discarded a preloaded icon");
			mkgui_font_set_size(win, 160);
			struct mkgui_glyph glyph = win->glyphs['W' - MKGUI_GLYPH_FIRST];
			struct mkwin_glyph_bitmap bitmap;
			check((size_t)glyph.width * (size_t)glyph.height > 4096, "glyph fixture does not exceed the old staging slot");
			uint32_t rendered = mkwin_font_glyph(mkgui_font, 'W', &bitmap);
			check(rendered, "cannot rasterize reference glyph");

			if(rendered) {
				check(memcmp(glyph_atlas + glyph.atlas_offset, bitmap.coverage, (size_t)glyph.width * (size_t)glyph.height) == 0, "large glyph bitmap was truncated");

			}
			uint8_t *previous = glyph_atlas;
			fail_map = 1;
			mkgui_font_load_glyphs(win);
			check(glyph_atlas == previous && memcmp(&win->glyphs['W' - MKGUI_GLYPH_FIRST], &glyph, sizeof(glyph)) == 0, "failed glyph reserve damaged the previous atlas");
			mkgui_window_destroy(win);
			check(!glyph_atlas && !glyph_arena.base, "font atlas survived last-window cleanup");

		}

		mkgui_ctx_destroy(ctx);

	}
}

// [=]===^=[ test_unused_context ]==============================================================[=]
static void test_unused_context(void) {
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	check(ctx != NULL, "unused context creation");

	if(ctx) {
		uint32_t pixel = 0xff123456;
		check(mkgui_icon_add("unused", &pixel, 1, 1) >= 0, "unused context icon insertion");
		mkgui_ctx_destroy(ctx);
		check(!icons && !icon_hash && !icon_atlas && !icon_arena.base, "context without a window leaked preloaded icons");

	}
}

// [=]===^=[ main ]=============================================================================[=]
int main(void) {
	test_raster_growth();
#ifdef _WIN32
	char directory[MAX_PATH];
	char path[MAX_PATH];

	if(GetTempPathA(sizeof(directory), directory) && GetTempFileNameA(directory, "mkg", 0, path)) {
		test_svg_growth(path);

	} else {
		check(0, "cannot create SVG fixture path");

	}
#else
	char path[] = "/tmp/mkgui-icon-storage-XXXXXX";
	int descriptor = mkstemp(path);

	if(descriptor >= 0) {
		close(descriptor);
		test_svg_growth(path);

	} else {
		check(0, "cannot create SVG fixture path");

	}
#endif
	test_allocation_failures();
	test_large_glyphs();
	test_unused_context();
	printf("Icon storage: %u checks, %u failures\n", checks, failures);
	return failures ? 1 : 0;

}
