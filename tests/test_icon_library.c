// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT

#include "../mkgui.c"

static uint32_t failures;
static uint32_t checks;

// [=]===^=[ icon_check ]========================================[=]
static void icon_check(uint32_t condition, const char *message) {
	++checks;

	if(!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		++failures;

	}
}

// [=]===^=[ test_catalogue ]====================================[=]
static void test_catalogue(void) {
	uint32_t count = mkguiicons_count();
	uint32_t rendered = 0;
	icon_check(count > 6000, "catalogue incomplete");
	icon_check(mkguiicons_name(count) == NULL && mkguiicons_keywords(count) == NULL, "invalid name indices");
	icon_check(mkguiicons_topic(count) == UINT32_MAX && mkguiicons_topic_name(mkguiicons_topic_count()) == NULL, "invalid topic indices");
	icon_check(mkguiicons_size_count(count) == 0 && mkguiicons_size(count, 0) == 0, "invalid size indices");

	for(uint32_t i = 0; i < count; ++i) {
		const char *name = mkguiicons_name(i);
		uint32_t variants = mkguiicons_size_count(i);
		icon_check(strlen(name) < MKGUI_ICON_NAME_LEN, name);
		icon_check(i == 0 || strcmp(mkguiicons_name(i - 1), name) < 0, name);
		icon_check(mkguiicons_topic(i) < mkguiicons_topic_count(), name);
		icon_check(mkguiicons_keywords(i) != NULL && variants > 0, name);

		for(uint32_t j = 0; j < variants; ++j) {
			uint32_t size = mkguiicons_size(i, j);
			uint32_t length;
			const char *svg = mkguiicons_svg(name, size, &length);
			icon_check(svg && length && strlen(svg) == length, name);
			uint32_t *pixels = icon_library_pixels(name, (int32_t)size, 0xffabcdef);
			icon_check(pixels != NULL, name);
			uint32_t coverage = 0;

			if(pixels) {
				for(uint32_t pixel = 0; pixel < size * size; ++pixel) {
					coverage |= pixels[pixel];

				}
			}

			icon_check(coverage != 0, name);
			free(pixels);
			++rendered;

		}
	}

	uint32_t length = 123;
	icon_check(mkguiicons_svg("does-not-exist", 16, &length) == NULL && length == 0, "missing name");
	icon_check(mkguiicons_svg(NULL, 16, &length) == NULL && length == 0, "null name");
	const char *save = mkguiicons_svg(MKICON_SAVE, 24, &length);
	icon_check(mkguiicons_svg("content-save", 24, &length) == NULL && length == 0, "legacy name is not accepted");
	icon_check(save == mkguiicons_svg(MKICON_SAVE, 0, &length), "default source size");
	icon_check(mkguiicons_svg(MKICON_SAVE, UINT32_MAX, &length) != NULL, "large source size");
	printf("icon catalogue: %u names, %u SVG lookups rendered\n", count, rendered);

}

// [=]===^=[ test_cache ]========================================[=]
static void test_cache(void) {
	static struct mkgui_window win;
	win.scale = 1.0f;
	win.icon_size = 16;
	win.theme.text = 0xff112233;
	mkgui_icon_init();
	int32_t index = icon_resolve_at(&win, MKICON_SAVE, 16);
	icon_check(index >= 0 && icons[index].library, "library cache entry");
	uint32_t count = icon_count;
	icon_check(icon_resolve_at(&win, MKICON_SAVE, 16) == index && icon_count == count, "cache hit");
	float scales[] = {2.13f, 0.57f, 1.0f};
	float previous = 1.0f;

	for(uint32_t i = 0; i < sizeof(scales) / sizeof(scales[0]); ++i) {
		win.scale = scales[i];
		svg_rerasterize_all(&win, scales[i] / previous);
		int32_t size = (int32_t)(16.0f * scales[i] + 0.5f);
		icon_check(icons[index].w == size && icons[index].h == size && icon_count == count, "scaled cache dimensions");
		uint32_t *reference = icon_library_pixels(MKICON_SAVE, size, win.theme.text);
		icon_check(reference && memcmp(reference, icon_atlas + icons[index].atlas_offset, (size_t)size * (size_t)size * sizeof(uint32_t)) == 0, "scaled vector pixels");
		free(reference);
		previous = scales[i];

	}

	win.theme.text = 0xfffedcba;
	svg_rerasterize_all(&win, 1.0f);
	uint32_t *reference = icon_library_pixels(MKICON_SAVE, 16, win.theme.text);
	icon_check(reference && memcmp(reference, icon_atlas + icons[index].atlas_offset, 16 * 16 * sizeof(uint32_t)) == 0, "theme rerasterization");
	free(reference);
	icon_check(icon_resolve_at(&win, "does-not-exist", 16) < 0 && icon_count == count, "missing icon preserves cache");
	svg_cleanup();
	icon_check(icon_count == 0 && svg_source_count == 0 && icon_atlas == NULL, "cache cleanup");

}

// [=]===^=[ test_browser_filter ]===============================[=]
static void test_browser_filter(void) {
	uint32_t count = mkguiicons_count();
	ibt = calloc(1, sizeof(*ibt) + (size_t)count * (sizeof(uint32_t *) + sizeof(uint32_t)));

	if(ibt) {
		ibt->thumb = (uint32_t **)(ibt + 1);
		ibt->filtered_idx = (uint32_t *)(ibt->thumb + count);
		ibt->total_count = count;
		ibt_filter("SAVE");
		icon_check(ibt->filtered_count > 0, "browser name search");

		for(uint32_t i = 0; i < ibt->filtered_count; ++i) {
			icon_check(strchr(mkguiicons_name(ibt->filtered_idx[i]), ':') == NULL, "default browser style");

		}

		ibt->active_style = 1;
		ibt_filter("save");
		icon_check(ibt->filtered_count > 0, "filled search");

		for(uint32_t i = 0; i < ibt->filtered_count; ++i) {
			icon_check(strstr(mkguiicons_name(ibt->filtered_idx[i]), ":filled") != NULL, "filled browser style");

		}

		ibt->active_cat = mkguiicons_topic(ibt->filtered_idx[0]) + 1;
		ibt->active_style = 4;
		ibt_filter(NULL);

		for(uint32_t i = 0; i < ibt->filtered_count; ++i) {
			icon_check(mkguiicons_topic(ibt->filtered_idx[i]) == ibt->active_cat - 1, "browser group");

		}

		ibt_confirm_selection(0);
		icon_check(ibt->confirmed && strcmp(ibt->result_name, mkguiicons_name(ibt->filtered_idx[0])) == 0, "browser selection returns name");
		ibt_free_thumbs();
		free(ibt);
		ibt = NULL;

	} else {
		icon_check(0, "browser allocation");

	}
}

// [=]===^=[ main ]==============================================[=]
int main(void) {
	test_catalogue();
	test_cache();
	test_browser_filter();
	printf("icon library: %u checks, %u failures\n", checks, failures);
	return failures ? 1 : 0;

}
