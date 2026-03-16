// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT
//
// Generates mdi_icons.dat from the MDI TTF font.
// Usage: gen_icons <ttf_path> <icon_size> <output.dat> [subset.txt]

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "../ext/mdi_icons.h"

#define MAX_SUBSET 4096

// [=]===^=[ render_glyph ]===========================================[=]
static uint32_t render_glyph(FT_Face face, uint32_t codepoint, uint8_t *dst, uint32_t size) {
	FT_UInt glyph_idx = FT_Get_Char_Index(face, codepoint);
	if(!glyph_idx) {
		return 0;
	}
	if(FT_Load_Glyph(face, glyph_idx, FT_LOAD_RENDER)) {
		return 0;
	}

	FT_GlyphSlot slot = face->glyph;
	FT_Bitmap *bmp = &slot->bitmap;

	uint32_t pixels = size * size;
	memset(dst, 0, pixels);

	int32_t gw = (int32_t)bmp->width;
	int32_t gh = (int32_t)bmp->rows;
	int32_t ox = ((int32_t)size - gw) / 2;
	int32_t oy = ((int32_t)size - gh) / 2;

	for(int32_t y = 0; y < gh; ++y) {
		int32_t dy = y + oy;
		if(dy < 0 || dy >= (int32_t)size) {
			continue;
		}
		for(int32_t x = 0; x < gw; ++x) {
			int32_t dx = x + ox;
			if(dx < 0 || dx >= (int32_t)size) {
				continue;
			}
			dst[dy * (int32_t)size + dx] = bmp->buffer[y * bmp->pitch + x];
		}
	}

	return 1;
}

// [=]===^=[ load_subset ]===============================================[=]
static uint32_t load_subset(const char *path, uint32_t *indices, uint32_t max_count) {
	FILE *fp = fopen(path, "r");
	if(!fp) {
		fprintf(stderr, "cannot open subset file: %s\n", path);
		return 0;
	}

	uint32_t count = 0;
	char line[256];
	while(fgets(line, sizeof(line), fp)) {
		char *p = line;
		while(*p == ' ' || *p == '\t') {
			++p;
		}
		if(*p == '#' || *p == '\n' || *p == '\0') {
			continue;
		}
		char *end = p + strlen(p) - 1;
		while(end > p && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t')) {
			*end-- = '\0';
		}
		if(*p == '\0') {
			continue;
		}
		uint32_t found = 0;
		for(uint32_t i = 0; i < MDI_ICON_COUNT; ++i) {
			if(strcmp(mdi_icons[i].name, p) == 0) {
				if(count < max_count) {
					indices[count++] = i;
				}
				found = 1;
				break;
			}
		}
		if(!found) {
			fprintf(stderr, "warning: icon not found: %s\n", p);
		}
	}

	fclose(fp);

	for(uint32_t i = 1; i < count; ++i) {
		uint32_t key = indices[i];
		uint32_t j = i;
		while(j > 0 && indices[j - 1] > key) {
			indices[j] = indices[j - 1];
			--j;
		}
		indices[j] = key;
	}

	uint32_t deduped = 0;
	for(uint32_t i = 0; i < count; ++i) {
		if(deduped == 0 || indices[i] != indices[deduped - 1]) {
			indices[deduped++] = indices[i];
		}
	}

	return deduped;
}

// [=]===^=[ main ]====================================================[=]
int main(int argc, char **argv) {
	if(argc < 4 || argc > 5) {
		fprintf(stderr, "usage: gen_icons <ttf_path> <icon_size> <output.dat> [subset.txt]\n");
		return 1;
	}

	const char *ttf_path = argv[1];
	uint32_t icon_size = (uint32_t)atoi(argv[2]);
	const char *out_path = argv[3];
	const char *subset_path = argc > 4 ? argv[4] : NULL;

	if(icon_size < 8 || icon_size > 64) {
		fprintf(stderr, "icon_size must be 8-64\n");
		return 1;
	}

	uint32_t subset_indices[MAX_SUBSET];
	uint32_t subset_count = 0;
	if(subset_path) {
		subset_count = load_subset(subset_path, subset_indices, MAX_SUBSET);
		if(subset_count == 0) {
			fprintf(stderr, "subset file empty or failed to load\n");
			return 1;
		}
	}

	uint32_t icon_count = subset_count > 0 ? subset_count : MDI_ICON_COUNT;

	FT_Library ft;
	FT_Face face;
	if(FT_Init_FreeType(&ft)) {
		fprintf(stderr, "cannot init freetype\n");
		return 1;
	}
	if(FT_New_Face(ft, ttf_path, 0, &face)) {
		fprintf(stderr, "cannot load font: %s\n", ttf_path);
		return 1;
	}
	FT_Set_Pixel_Sizes(face, 0, icon_size);

	uint32_t name_block_size = 0;
	for(uint32_t i = 0; i < icon_count; ++i) {
		uint32_t idx = subset_count > 0 ? subset_indices[i] : i;
		name_block_size += (uint32_t)strlen(mdi_icons[idx].name) + 1;
	}
	uint32_t name_block_padded = (name_block_size + 3) & ~3u;

	FILE *fp = fopen(out_path, "wb");
	if(!fp) {
		fprintf(stderr, "cannot open: %s\n", out_path);
		return 1;
	}

	// header: magic + icon_size + icon_count
	fwrite("MKIC", 1, 4, fp);
	uint16_t sz = (uint16_t)icon_size;
	uint16_t cnt = (uint16_t)icon_count;
	fwrite(&sz, 2, 1, fp);
	fwrite(&cnt, 2, 1, fp);

	// name block (packed null-terminated strings, sorted)
	uint32_t *name_offsets = calloc(icon_count, sizeof(uint32_t));
	uint32_t off = 0;
	for(uint32_t i = 0; i < icon_count; ++i) {
		uint32_t idx = subset_count > 0 ? subset_indices[i] : i;
		name_offsets[i] = off;
		uint32_t len = (uint32_t)strlen(mdi_icons[idx].name) + 1;
		fwrite(mdi_icons[idx].name, 1, len, fp);
		off += len;
	}
	// pad to 4-byte boundary
	uint8_t zero = 0;
	for(uint32_t i = name_block_size; i < name_block_padded; ++i) {
		fwrite(&zero, 1, 1, fp);
	}

	// name offset table
	fwrite(name_offsets, sizeof(uint32_t), icon_count, fp);

	// pixel data: 8bpp alpha bitmaps
	uint32_t pixel_count = icon_size * icon_size;
	uint8_t *buf = calloc(pixel_count, 1);
	uint32_t rendered = 0;
	uint32_t failed = 0;

	for(uint32_t i = 0; i < icon_count; ++i) {
		uint32_t idx = subset_count > 0 ? subset_indices[i] : i;
		memset(buf, 0, pixel_count);
		if(render_glyph(face, mdi_icons[idx].codepoint, buf, icon_size)) {
			++rendered;
		} else {
			++failed;
		}
		fwrite(buf, 1, pixel_count, fp);
	}

	long file_size = ftell(fp);
	fclose(fp);
	free(buf);
	free(name_offsets);
	FT_Done_Face(face);
	FT_Done_FreeType(ft);

	fprintf(stderr, "generated %s: %u icons (%u failed), %ux%u, %ld bytes (%.1f KB)\n",
		out_path, rendered, failed, icon_size, icon_size, file_size, file_size / 1024.0);

	return 0;
}
