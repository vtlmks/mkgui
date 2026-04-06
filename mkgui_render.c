// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// ---------------------------------------------------------------------------
// SIMD runtime dispatch (SSE2 baseline, AVX2 optional)
// ---------------------------------------------------------------------------

static uint32_t cpu_has_avx2;

// [=]===^=[ detect_cpu ]===========================================[=]
static void detect_cpu(void) {
	uint32_t eax, ebx, ecx, edx;
	__asm__ __volatile__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(7), "c"(0));
	cpu_has_avx2 = (ebx >> 5) & 1;
}

// [=]===^=[ fill_pixels_sse2 ]=====================================[=]
static void fill_pixels_sse2(uint32_t *dst, uint32_t count, uint32_t color) {
	uint32_t i = 0;
	__m128i cv = _mm_set1_epi32((int32_t)color);
	for(; i + 4 <= count; i += 4) {
		_mm_storeu_si128((__m128i *)&dst[i], cv);
	}
	for(; i < count; ++i) {
		dst[i] = color;
	}
}

// [=]===^=[ fill_pixels_avx2 ]=====================================[=]
__attribute__((target("avx2")))
static void fill_pixels_avx2(uint32_t *dst, uint32_t count, uint32_t color) {
	uint32_t i = 0;
	__m256i cv = _mm256_set1_epi32((int32_t)color);
	for(; i + 8 <= count; i += 8) {
		_mm256_storeu_si256((__m256i *)&dst[i], cv);
	}
	__m128i cv4 = _mm_set1_epi32((int32_t)color);
	for(; i + 4 <= count; i += 4) {
		_mm_storeu_si128((__m128i *)&dst[i], cv4);
	}
	for(; i < count; ++i) {
		dst[i] = color;
	}
}

// [=]===^=[ blend_glyph_row_sse2 ]=================================[=]
static void blend_glyph_row_sse2(uint32_t *rowp, uint8_t *bmp, int32_t col0, int32_t col1, int32_t gx, uint32_t rb_src, uint32_t g_src) {
	// color is pre-split into red|blue and green channels so we can use
	// 16-bit multiplies to blend two channels per 32-bit lane simultaneously
	int32_t col = col0;
	__m128i src_rb = _mm_set1_epi32((int32_t)rb_src);
	__m128i src_g8 = _mm_set1_epi32((int32_t)(g_src >> 8));
	// 0x80 bias per channel implements the "divide by 255" approximation:
	// (x + 0x80 + (x >> 8)) >> 8 is equivalent to x/255 with correct rounding
	__m128i round_rb = _mm_set1_epi32(0x00800080);
	__m128i round_g = _mm_set1_epi32(0x00000080);
	__m128i mask_rb = _mm_set1_epi32(0x00ff00ff);
	__m128i mask_byte = _mm_set1_epi32(0x000000ff);
	__m128i mask_a = _mm_set1_epi32((int32_t)0xff000000u);
	__m128i val_255 = _mm_set1_epi32(255);
	__m128i zero = _mm_setzero_si128();
	for(; col + 4 <= col1; col += 4) {
		__m128i alphas = _mm_cvtsi32_si128(*(int32_t *)&bmp[col]);
		alphas = _mm_unpacklo_epi8(alphas, zero);
		alphas = _mm_unpacklo_epi16(alphas, zero);
		__m128i any = _mm_cmpeq_epi32(alphas, zero);
		if(_mm_movemask_epi8(any) == 0xffff) {
			continue;
		}
		int32_t px = gx + col;
		__m128i dst = _mm_loadu_si128((__m128i *)&rowp[px]);
		__m128i ia = _mm_sub_epi32(val_255, alphas);
		__m128i a16 = _mm_or_si128(alphas, _mm_slli_epi32(alphas, 16));
		__m128i ia16 = _mm_or_si128(ia, _mm_slli_epi32(ia, 16));
		__m128i dst_rb = _mm_and_si128(dst, mask_rb);
		__m128i dst_g8 = _mm_and_si128(_mm_srli_epi32(dst, 8), mask_byte);
		__m128i rb = _mm_add_epi32(_mm_add_epi32(_mm_mullo_epi16(src_rb, a16), _mm_mullo_epi16(dst_rb, ia16)), round_rb);
		rb = _mm_and_si128(_mm_srli_epi32(_mm_add_epi32(rb, _mm_and_si128(_mm_srli_epi32(rb, 8), mask_rb)), 8), mask_rb);
		__m128i gv = _mm_add_epi32(_mm_add_epi32(_mm_mullo_epi16(src_g8, alphas), _mm_mullo_epi16(dst_g8, ia)), round_g);
		gv = _mm_slli_epi32(_mm_and_si128(_mm_srli_epi32(_mm_add_epi32(gv, _mm_and_si128(_mm_srli_epi32(gv, 8), mask_byte)), 8), mask_byte), 8);
		__m128i result = _mm_or_si128(_mm_or_si128(rb, gv), mask_a);
		__m128i skip = _mm_cmpeq_epi32(alphas, zero);
		result = _mm_or_si128(_mm_andnot_si128(skip, result), _mm_and_si128(skip, dst));
		_mm_storeu_si128((__m128i *)&rowp[px], result);
	}
	for(; col < col1; ++col) {
		uint32_t a = bmp[col];
		if(a == 0) {
			continue;
		}
		int32_t px = gx + col;
		uint32_t ia = 255 - a;
		uint32_t d = rowp[px];
		uint32_t rb = (rb_src * a + (d & 0x00ff00ff) * ia + 0x00800080);
		rb = ((rb + ((rb >> 8) & 0x00ff00ff)) >> 8) & 0x00ff00ff;
		uint32_t gv = (g_src * a + (d & 0x0000ff00) * ia + 0x00008000);
		gv = ((gv + ((gv >> 8) & 0x0000ff00)) >> 8) & 0x0000ff00;
		rowp[px] = 0xff000000 | rb | gv;
	}
}

// [=]===^=[ blend_glyph_row_avx2 ]=================================[=]
__attribute__((target("avx2")))
static void blend_glyph_row_avx2(uint32_t *rowp, uint8_t *bmp, int32_t col0, int32_t col1, int32_t gx, uint32_t rb_src, uint32_t g_src) {
	int32_t col = col0;
	__m256i src_rb = _mm256_set1_epi32((int32_t)rb_src);
	__m256i src_g8 = _mm256_set1_epi32((int32_t)(g_src >> 8));
	__m256i round_rb = _mm256_set1_epi32(0x00800080);
	__m256i round_g = _mm256_set1_epi32(0x00000080);
	__m256i mask_rb = _mm256_set1_epi32(0x00ff00ff);
	__m256i mask_byte = _mm256_set1_epi32(0x000000ff);
	__m256i mask_a = _mm256_set1_epi32((int32_t)0xff000000u);
	__m256i val_255 = _mm256_set1_epi32(255);
	__m256i zero = _mm256_setzero_si256();
	for(; col + 8 <= col1; col += 8) {
		__m128i alpha8 = _mm_loadl_epi64((__m128i *)&bmp[col]);
		__m128i alpha16 = _mm_unpacklo_epi8(alpha8, _mm_setzero_si128());
		__m256i alphas = _mm256_cvtepu16_epi32(alpha16);
		__m256i any = _mm256_cmpeq_epi32(alphas, zero);
		if(_mm256_movemask_epi8(any) == -1) {
			continue;
		}
		int32_t px = gx + col;
		__m256i dst = _mm256_loadu_si256((__m256i *)&rowp[px]);
		__m256i ia = _mm256_sub_epi32(val_255, alphas);
		__m256i a16 = _mm256_or_si256(alphas, _mm256_slli_epi32(alphas, 16));
		__m256i ia16 = _mm256_or_si256(ia, _mm256_slli_epi32(ia, 16));
		__m256i dst_rb = _mm256_and_si256(dst, mask_rb);
		__m256i dst_g8 = _mm256_and_si256(_mm256_srli_epi32(dst, 8), mask_byte);
		__m256i rb = _mm256_add_epi32(_mm256_add_epi32(_mm256_mullo_epi16(src_rb, a16), _mm256_mullo_epi16(dst_rb, ia16)), round_rb);
		rb = _mm256_and_si256(_mm256_srli_epi32(_mm256_add_epi32(rb, _mm256_and_si256(_mm256_srli_epi32(rb, 8), mask_rb)), 8), mask_rb);
		__m256i gv = _mm256_add_epi32(_mm256_add_epi32(_mm256_mullo_epi16(src_g8, alphas), _mm256_mullo_epi16(dst_g8, ia)), round_g);
		gv = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(_mm256_add_epi32(gv, _mm256_and_si256(_mm256_srli_epi32(gv, 8), mask_byte)), 8), mask_byte), 8);
		__m256i result = _mm256_or_si256(_mm256_or_si256(rb, gv), mask_a);
		__m256i skip = _mm256_cmpeq_epi32(alphas, zero);
		result = _mm256_or_si256(_mm256_andnot_si256(skip, result), _mm256_and_si256(skip, dst));
		_mm256_storeu_si256((__m256i *)&rowp[px], result);
	}
	// SSE2 tail for remaining 4+ pixels
	__m128i src_rb4 = _mm_set1_epi32((int32_t)rb_src);
	__m128i src_g84 = _mm_set1_epi32((int32_t)(g_src >> 8));
	__m128i round_rb4 = _mm_set1_epi32(0x00800080);
	__m128i round_g4 = _mm_set1_epi32(0x00000080);
	__m128i mask_rb4 = _mm_set1_epi32(0x00ff00ff);
	__m128i mask_byte4 = _mm_set1_epi32(0x000000ff);
	__m128i mask_a4 = _mm_set1_epi32((int32_t)0xff000000u);
	__m128i val_2554 = _mm_set1_epi32(255);
	__m128i zero4 = _mm_setzero_si128();
	for(; col + 4 <= col1; col += 4) {
		int32_t bmp4; memcpy(&bmp4, &bmp[col], 4);
		__m128i alphas4 = _mm_cvtsi32_si128(bmp4);
		alphas4 = _mm_unpacklo_epi8(alphas4, zero4);
		alphas4 = _mm_unpacklo_epi16(alphas4, zero4);
		__m128i any4 = _mm_cmpeq_epi32(alphas4, zero4);
		if(_mm_movemask_epi8(any4) == 0xffff) {
			continue;
		}
		int32_t px = gx + col;
		__m128i dst4 = _mm_loadu_si128((__m128i *)&rowp[px]);
		__m128i ia4 = _mm_sub_epi32(val_2554, alphas4);
		__m128i a164 = _mm_or_si128(alphas4, _mm_slli_epi32(alphas4, 16));
		__m128i ia164 = _mm_or_si128(ia4, _mm_slli_epi32(ia4, 16));
		__m128i dst_rb4 = _mm_and_si128(dst4, mask_rb4);
		__m128i dst_g84 = _mm_and_si128(_mm_srli_epi32(dst4, 8), mask_byte4);
		__m128i rb4 = _mm_add_epi32(_mm_add_epi32(_mm_mullo_epi16(src_rb4, a164), _mm_mullo_epi16(dst_rb4, ia164)), round_rb4);
		rb4 = _mm_and_si128(_mm_srli_epi32(_mm_add_epi32(rb4, _mm_and_si128(_mm_srli_epi32(rb4, 8), mask_rb4)), 8), mask_rb4);
		__m128i gv4 = _mm_add_epi32(_mm_add_epi32(_mm_mullo_epi16(src_g84, alphas4), _mm_mullo_epi16(dst_g84, ia4)), round_g4);
		gv4 = _mm_slli_epi32(_mm_and_si128(_mm_srli_epi32(_mm_add_epi32(gv4, _mm_and_si128(_mm_srli_epi32(gv4, 8), mask_byte4)), 8), mask_byte4), 8);
		__m128i result4 = _mm_or_si128(_mm_or_si128(rb4, gv4), mask_a4);
		__m128i skip4 = _mm_cmpeq_epi32(alphas4, zero4);
		result4 = _mm_or_si128(_mm_andnot_si128(skip4, result4), _mm_and_si128(skip4, dst4));
		_mm_storeu_si128((__m128i *)&rowp[px], result4);
	}
	for(; col < col1; ++col) {
		uint32_t a = bmp[col];
		if(a == 0) {
			continue;
		}
		int32_t px = gx + col;
		uint32_t ia = 255 - a;
		uint32_t d = rowp[px];
		uint32_t rb = (rb_src * a + (d & 0x00ff00ff) * ia + 0x00800080);
		rb = ((rb + ((rb >> 8) & 0x00ff00ff)) >> 8) & 0x00ff00ff;
		uint32_t gv = (g_src * a + (d & 0x0000ff00) * ia + 0x00008000);
		gv = ((gv + ((gv >> 8) & 0x0000ff00)) >> 8) & 0x0000ff00;
		rowp[px] = 0xff000000 | rb | gv;
	}
}

// [=]===^=[ blend_icon_row_sse2 ]===================================[=]
// unlike glyph blending (alpha * src + (1-alpha) * dst), icons use
// premultiplied alpha: src + (1-alpha) * dst. the source pixels already
// have their RGB values scaled by alpha in the icon rasterizer
static void blend_icon_row_sse2(uint32_t *rowp, uint32_t *src, int32_t col0, int32_t col1, int32_t x) {
	int32_t col = col0;
	__m128i round_rb = _mm_set1_epi32(0x00800080);
	__m128i round_g = _mm_set1_epi32(0x00000080);
	__m128i mask_rb = _mm_set1_epi32(0x00ff00ff);
	__m128i mask_byte = _mm_set1_epi32(0x000000ff);
	__m128i mask_a = _mm_set1_epi32((int32_t)0xff000000u);
	__m128i val_255 = _mm_set1_epi32(255);
	__m128i zero = _mm_setzero_si128();
	for(; col + 4 <= col1; col += 4) {
		__m128i spx = _mm_loadu_si128((__m128i *)&src[col]);
		__m128i alphas = _mm_srli_epi32(spx, 24);
		__m128i any_zero = _mm_cmpeq_epi32(alphas, zero);
		if(_mm_movemask_epi8(any_zero) == 0xffff) {
			continue;
		}
		int32_t px = x + col;
		__m128i dst = _mm_loadu_si128((__m128i *)&rowp[px]);
		__m128i ia = _mm_sub_epi32(val_255, alphas);
		__m128i ia16 = _mm_or_si128(ia, _mm_slli_epi32(ia, 16));
		__m128i src_rb = _mm_and_si128(spx, mask_rb);
		__m128i src_g8 = _mm_and_si128(_mm_srli_epi32(spx, 8), mask_byte);
		__m128i dst_rb = _mm_and_si128(dst, mask_rb);
		__m128i dst_g8 = _mm_and_si128(_mm_srli_epi32(dst, 8), mask_byte);
		__m128i d_rb = _mm_add_epi32(_mm_mullo_epi16(dst_rb, ia16), round_rb);
		d_rb = _mm_and_si128(_mm_srli_epi32(_mm_add_epi32(d_rb, _mm_and_si128(_mm_srli_epi32(d_rb, 8), mask_rb)), 8), mask_rb);
		__m128i d_gv = _mm_add_epi32(_mm_mullo_epi16(dst_g8, ia), round_g);
		d_gv = _mm_slli_epi32(_mm_and_si128(_mm_srli_epi32(_mm_add_epi32(d_gv, _mm_and_si128(_mm_srli_epi32(d_gv, 8), mask_byte)), 8), mask_byte), 8);
		__m128i rb = _mm_add_epi32(src_rb, d_rb);
		__m128i gv = _mm_add_epi32(_mm_slli_epi32(src_g8, 8), d_gv);
		__m128i result = _mm_or_si128(_mm_and_si128(_mm_or_si128(rb, gv), _mm_set1_epi32(0x00ffffff)), mask_a);
		__m128i full = _mm_cmpeq_epi32(alphas, val_255);
		result = _mm_or_si128(_mm_and_si128(full, spx), _mm_andnot_si128(full, result));
		__m128i skip = _mm_cmpeq_epi32(alphas, zero);
		result = _mm_or_si128(_mm_andnot_si128(skip, result), _mm_and_si128(skip, dst));
		_mm_storeu_si128((__m128i *)&rowp[px], result);
	}
	for(; col < col1; ++col) {
		uint32_t spx = src[col];
		uint32_t a = spx >> 24;
		int32_t px = x + col;
		if(a == 255) {
			rowp[px] = spx;
		} else if(a > 0) {
			uint32_t ia = 255 - a;
			uint32_t d = rowp[px];
			uint32_t d_rb = (d & 0x00ff00ff) * ia + 0x00800080;
			d_rb = ((d_rb + ((d_rb >> 8) & 0x00ff00ff)) >> 8) & 0x00ff00ff;
			uint32_t d_g = (d & 0x0000ff00) * ia + 0x00008000;
			d_g = ((d_g + ((d_g >> 8) & 0x0000ff00)) >> 8) & 0x0000ff00;
			uint32_t s_rb = spx & 0x00ff00ff;
			uint32_t s_g = spx & 0x0000ff00;
			rowp[px] = 0xff000000 | ((s_rb + d_rb) & 0x00ff00ff) | ((s_g + d_g) & 0x0000ff00);
		}
	}
}

// [=]===^=[ blend_icon_row_avx2 ]===================================[=]
__attribute__((target("avx2")))
static void blend_icon_row_avx2(uint32_t *rowp, uint32_t *src, int32_t col0, int32_t col1, int32_t x) {
	int32_t col = col0;
	__m256i round_rb = _mm256_set1_epi32(0x00800080);
	__m256i round_g = _mm256_set1_epi32(0x00000080);
	__m256i mask_rb = _mm256_set1_epi32(0x00ff00ff);
	__m256i mask_byte = _mm256_set1_epi32(0x000000ff);
	__m256i mask_a = _mm256_set1_epi32((int32_t)0xff000000u);
	__m256i mask_rgb = _mm256_set1_epi32(0x00ffffff);
	__m256i val_255 = _mm256_set1_epi32(255);
	__m256i zero = _mm256_setzero_si256();
	for(; col + 8 <= col1; col += 8) {
		__m256i spx = _mm256_loadu_si256((__m256i *)&src[col]);
		__m256i alphas = _mm256_srli_epi32(spx, 24);
		__m256i any_zero = _mm256_cmpeq_epi32(alphas, zero);
		if(_mm256_movemask_epi8(any_zero) == -1) {
			continue;
		}
		int32_t px = x + col;
		__m256i dst = _mm256_loadu_si256((__m256i *)&rowp[px]);
		__m256i ia = _mm256_sub_epi32(val_255, alphas);
		__m256i ia16 = _mm256_or_si256(ia, _mm256_slli_epi32(ia, 16));
		__m256i src_rb = _mm256_and_si256(spx, mask_rb);
		__m256i src_g8 = _mm256_and_si256(_mm256_srli_epi32(spx, 8), mask_byte);
		__m256i dst_rb = _mm256_and_si256(dst, mask_rb);
		__m256i dst_g8 = _mm256_and_si256(_mm256_srli_epi32(dst, 8), mask_byte);
		__m256i d_rb = _mm256_add_epi32(_mm256_mullo_epi16(dst_rb, ia16), round_rb);
		d_rb = _mm256_and_si256(_mm256_srli_epi32(_mm256_add_epi32(d_rb, _mm256_and_si256(_mm256_srli_epi32(d_rb, 8), mask_rb)), 8), mask_rb);
		__m256i d_gv = _mm256_add_epi32(_mm256_mullo_epi16(dst_g8, ia), round_g);
		d_gv = _mm256_slli_epi32(_mm256_and_si256(_mm256_srli_epi32(_mm256_add_epi32(d_gv, _mm256_and_si256(_mm256_srli_epi32(d_gv, 8), mask_byte)), 8), mask_byte), 8);
		__m256i rb = _mm256_add_epi32(src_rb, d_rb);
		__m256i gv = _mm256_add_epi32(_mm256_slli_epi32(src_g8, 8), d_gv);
		__m256i result = _mm256_or_si256(_mm256_and_si256(_mm256_or_si256(rb, gv), mask_rgb), mask_a);
		__m256i full = _mm256_cmpeq_epi32(alphas, val_255);
		result = _mm256_or_si256(_mm256_and_si256(full, spx), _mm256_andnot_si256(full, result));
		__m256i skip = _mm256_cmpeq_epi32(alphas, zero);
		result = _mm256_or_si256(_mm256_andnot_si256(skip, result), _mm256_and_si256(skip, dst));
		_mm256_storeu_si256((__m256i *)&rowp[px], result);
	}
	// SSE2 tail
	__m128i round_rb4 = _mm_set1_epi32(0x00800080);
	__m128i round_g4 = _mm_set1_epi32(0x00000080);
	__m128i mask_rb4 = _mm_set1_epi32(0x00ff00ff);
	__m128i mask_byte4 = _mm_set1_epi32(0x000000ff);
	__m128i mask_a4 = _mm_set1_epi32((int32_t)0xff000000u);
	__m128i val_2554 = _mm_set1_epi32(255);
	__m128i zero4 = _mm_setzero_si128();
	for(; col + 4 <= col1; col += 4) {
		__m128i spx = _mm_loadu_si128((__m128i *)&src[col]);
		__m128i alphas = _mm_srli_epi32(spx, 24);
		__m128i any_zero = _mm_cmpeq_epi32(alphas, zero4);
		if(_mm_movemask_epi8(any_zero) == 0xffff) {
			continue;
		}
		int32_t px = x + col;
		__m128i dst = _mm_loadu_si128((__m128i *)&rowp[px]);
		__m128i ia = _mm_sub_epi32(val_2554, alphas);
		__m128i ia16 = _mm_or_si128(ia, _mm_slli_epi32(ia, 16));
		__m128i s_rb = _mm_and_si128(spx, mask_rb4);
		__m128i s_g8 = _mm_and_si128(_mm_srli_epi32(spx, 8), mask_byte4);
		__m128i dst_rb = _mm_and_si128(dst, mask_rb4);
		__m128i dst_g8 = _mm_and_si128(_mm_srli_epi32(dst, 8), mask_byte4);
		__m128i d_rb = _mm_add_epi32(_mm_mullo_epi16(dst_rb, ia16), round_rb4);
		d_rb = _mm_and_si128(_mm_srli_epi32(_mm_add_epi32(d_rb, _mm_and_si128(_mm_srli_epi32(d_rb, 8), mask_rb4)), 8), mask_rb4);
		__m128i d_gv = _mm_add_epi32(_mm_mullo_epi16(dst_g8, ia), round_g4);
		d_gv = _mm_slli_epi32(_mm_and_si128(_mm_srli_epi32(_mm_add_epi32(d_gv, _mm_and_si128(_mm_srli_epi32(d_gv, 8), mask_byte4)), 8), mask_byte4), 8);
		__m128i rb = _mm_add_epi32(s_rb, d_rb);
		__m128i gv = _mm_add_epi32(_mm_slli_epi32(s_g8, 8), d_gv);
		__m128i result = _mm_or_si128(_mm_and_si128(_mm_or_si128(rb, gv), _mm_set1_epi32(0x00ffffff)), mask_a4);
		__m128i full = _mm_cmpeq_epi32(alphas, val_2554);
		result = _mm_or_si128(_mm_and_si128(full, spx), _mm_andnot_si128(full, result));
		__m128i skip = _mm_cmpeq_epi32(alphas, zero4);
		result = _mm_or_si128(_mm_andnot_si128(skip, result), _mm_and_si128(skip, dst));
		_mm_storeu_si128((__m128i *)&rowp[px], result);
	}
	for(; col < col1; ++col) {
		uint32_t spx = src[col];
		uint32_t a = spx >> 24;
		int32_t px = x + col;
		if(a == 255) {
			rowp[px] = spx;
		} else if(a > 0) {
			uint32_t ia = 255 - a;
			uint32_t d = rowp[px];
			uint32_t d_rb = (d & 0x00ff00ff) * ia + 0x00800080;
			d_rb = ((d_rb + ((d_rb >> 8) & 0x00ff00ff)) >> 8) & 0x00ff00ff;
			uint32_t d_g = (d & 0x0000ff00) * ia + 0x00008000;
			d_g = ((d_g + ((d_g >> 8) & 0x0000ff00)) >> 8) & 0x0000ff00;
			uint32_t s_rb = spx & 0x00ff00ff;
			uint32_t s_g = spx & 0x0000ff00;
			rowp[px] = 0xff000000 | ((s_rb + d_rb) & 0x00ff00ff) | ((s_g + d_g) & 0x0000ff00);
		}
	}
}

// Function pointer types
typedef void (*fn_fill_pixels)(uint32_t *, uint32_t, uint32_t);
typedef void (*fn_blend_glyph_row)(uint32_t *, uint8_t *, int32_t, int32_t, int32_t, uint32_t, uint32_t);
typedef void (*fn_blend_icon_row)(uint32_t *, uint32_t *, int32_t, int32_t, int32_t);

// Forward declaration
static void fill_pixels_resolve(uint32_t *dst, uint32_t count, uint32_t color);

// Dispatch pointers
static fn_fill_pixels fill_pixels = fill_pixels_resolve;
static fn_blend_glyph_row blend_glyph_row = blend_glyph_row_sse2;
static fn_blend_icon_row blend_icon_row = blend_icon_row_sse2;

// [=]===^=[ fill_pixels_resolve ]===================================[=]
static void fill_pixels_resolve(uint32_t *dst, uint32_t count, uint32_t color) {
	detect_cpu();
	if(cpu_has_avx2) {
		fill_pixels = fill_pixels_avx2;
		blend_glyph_row = blend_glyph_row_avx2;
		blend_icon_row = blend_icon_row_avx2;
	} else {
		fill_pixels = fill_pixels_sse2;
	}
	fill_pixels(dst, count, color);
}

// ---------------------------------------------------------------------------
// Drawing primitives
// ---------------------------------------------------------------------------

// [=]===^=[ blend_pixel ]========================================[=]
static inline uint32_t blend_pixel(uint32_t dst, uint32_t color, uint8_t alpha) {
	if(alpha == 255) {
		return color;
	}
	uint32_t a = alpha;
	uint32_t ia = 255 - a;
	uint32_t rb_src = color & 0x00ff00ff;
	uint32_t g_src = color & 0x0000ff00;
	uint32_t rb_dst = dst & 0x00ff00ff;
	uint32_t g_dst = dst & 0x0000ff00;
	uint32_t rb = (rb_src * a + rb_dst * ia + 0x00800080);
	rb = ((rb + ((rb >> 8) & 0x00ff00ff)) >> 8) & 0x00ff00ff;
	uint32_t g = (g_src * a + g_dst * ia + 0x00008000);
	g = ((g + ((g >> 8) & 0x0000ff00)) >> 8) & 0x0000ff00;
	return 0xff000000 | rb | g;
}

static int32_t render_clip_x1, render_clip_y1, render_clip_x2, render_clip_y2;
static int32_t render_base_clip_x1, render_base_clip_y1, render_base_clip_x2, render_base_clip_y2;

// [=]===^=[ draw_pixel ]========================================[=]
static void draw_pixel(uint32_t *buf, int32_t bw, int32_t bh, int32_t x, int32_t y, uint32_t color) {
	if(x >= 0 && x < bw && y >= 0 && y < bh && x >= render_clip_x1 && x < render_clip_x2 && y >= render_clip_y1 && y < render_clip_y2) {
		buf[y * bw + x] = color;
	}
}

// [=]===^=[ draw_rect_fill ]====================================[=]
static void draw_rect_fill(uint32_t *buf, int32_t bw, int32_t bh, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
	int32_t x0 = x < 0 ? 0 : x;
	int32_t y0 = y < 0 ? 0 : y;
	int32_t x1 = (x + w) > bw ? bw : (x + w);
	int32_t y1 = (y + h) > bh ? bh : (y + h);
	if(x0 < render_clip_x1) {
		x0 = render_clip_x1;
	}
	if(y0 < render_clip_y1) {
		y0 = render_clip_y1;
	}
	if(x1 > render_clip_x2) {
		x1 = render_clip_x2;
	}
	if(y1 > render_clip_y2) {
		y1 = render_clip_y2;
	}
	int32_t span = x1 - x0;
	if(span <= 0) {
		return;
	}
	for(int32_t row = y0; row < y1; ++row) {
		fill_pixels(&buf[row * bw + x0], (uint32_t)span, color);
	}
}

#define MKGUI_MAX_CORNER_RADIUS 48

// [=]===^=[ rounded_rect_insets ]=================================[=]
static void rounded_rect_insets(int32_t radius, int32_t *out) {
	for(uint32_t row = 0; row < (uint32_t)radius; ++row) {
		int32_t dy = 2 * (radius - (int32_t)row) - 1;
		int32_t r2x4 = 4 * radius * radius;
		int32_t inset = 0;
		for(uint32_t col = 0; col < (uint32_t)radius; ++col) {
			int32_t dx = 2 * (radius - (int32_t)col) - 1;
			if(dx * dx + dy * dy > r2x4) {
				inset = (int32_t)(col + 1);
			}
		}
		out[row] = inset;
	}
}

// [=]===^=[ corner_coverage ]=====================================[=]
// computes antialiased coverage for a pixel in a rounded corner by testing
// a 4x4 subpixel grid against the circle equation (dx^2 + dy^2 <= r^2).
// coordinates are scaled 8x for integer subpixel precision; the SIMD path
// evaluates all 4 x-samples per row in one compare via madd + popcount
static uint32_t corner_coverage(int32_t col, int32_t crow, int32_t radius) {
	int32_t r8 = 8 * radius;
	int32_t r8sq = r8 * r8;
	int32_t base_dx8 = 8 * radius - 8 * col;
	int32_t base_dy8 = 8 * radius - 8 * crow;
	uint32_t count = 0;
#if defined(__SSE2__)
	__m128i dx_base = _mm_set_epi32(base_dx8 - 7, base_dx8 - 5, base_dx8 - 3, base_dx8 - 1);
	for(uint32_t sy = 0; sy < 4; ++sy) {
		int32_t dy8 = base_dy8 - 2 * (int32_t)sy - 1;
		int32_t remain = r8sq - dy8 * dy8;
		if(remain < 0) {
			continue;
		}
		__m128i rem = _mm_set1_epi32(remain);
		__m128i dx_lo = _mm_and_si128(dx_base, _mm_set1_epi32(0x0000ffff));
		__m128i dx2 = _mm_madd_epi16(dx_lo, dx_lo);
		__m128i cmp = _mm_or_si128(_mm_cmplt_epi32(dx2, rem), _mm_cmpeq_epi32(dx2, rem));
		count += (uint32_t)__builtin_popcount((uint32_t)_mm_movemask_epi8(cmp)) / 4;
	}
#else
	for(uint32_t sy = 0; sy < 4; ++sy) {
		int32_t dy8 = base_dy8 - 2 * (int32_t)sy - 1;
		int32_t remain = r8sq - dy8 * dy8;
		if(remain < 0) {
			continue;
		}
		for(uint32_t sx = 0; sx < 4; ++sx) {
			int32_t dx8 = base_dx8 - 2 * (int32_t)sx - 1;
			if(dx8 * dx8 <= remain) {
				++count;
			}
		}
	}
#endif
	return count;
}

// [=]===^=[ fill_span_clipped ]===================================[=]
static void fill_span_clipped(uint32_t *buf, int32_t bw, int32_t py, int32_t left, int32_t right, uint32_t color) {
	if(left < 0) {
		left = 0;
	}
	if(left < render_clip_x1) {
		left = render_clip_x1;
	}
	if(right > bw) {
		right = bw;
	}
	if(right > render_clip_x2) {
		right = render_clip_x2;
	}
	if(left < right) {
		fill_pixels(&buf[py * bw + left], (uint32_t)(right - left), color);
	}
}

// [=]===^=[ draw_rounded_rect_fill ]==============================[=]
static void draw_rounded_rect_fill(uint32_t *buf, int32_t bw, int32_t bh, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color, int32_t radius) {
	if(w <= 0 || h <= 0) {
		return;
	}
	if(radius > w / 2) {
		radius = w / 2;
	}
	if(radius > h / 2) {
		radius = h / 2;
	}
	if(radius > MKGUI_MAX_CORNER_RADIUS) {
		radius = MKGUI_MAX_CORNER_RADIUS;
	}

	for(uint32_t row = 0; row < (uint32_t)h; ++row) {
		int32_t py = y + (int32_t)row;
		if(py < 0 || py >= bh || py < render_clip_y1 || py >= render_clip_y2) {
			continue;
		}

		int32_t crow = -1;
		if((int32_t)row < radius) {
			crow = (int32_t)row;
		} else if((int32_t)row >= h - radius) {
			crow = h - 1 - (int32_t)row;
		}

		if(crow >= 0 && radius > 0) {
			uint32_t *rowp = &buf[py * bw];
			for(uint32_t col = 0; col < (uint32_t)radius; ++col) {
				uint32_t cov = corner_coverage((int32_t)col, crow, radius);
				if(cov == 0) {
					continue;
				}
				int32_t pl = x + (int32_t)col;
				int32_t pr = x + w - 1 - (int32_t)col;
				if(cov >= 16) {
					if(pl >= 0 && pl < bw && pl >= render_clip_x1 && pl < render_clip_x2) {
						rowp[pl] = color;
					}
					if(pr >= 0 && pr < bw && pr != pl && pr >= render_clip_x1 && pr < render_clip_x2) {
						rowp[pr] = color;
					}
				} else {
					uint8_t alpha = (uint8_t)(cov * 255 / 16);
					if(pl >= 0 && pl < bw && pl >= render_clip_x1 && pl < render_clip_x2) {
						rowp[pl] = blend_pixel(rowp[pl], color, alpha);
					}
					if(pr >= 0 && pr < bw && pr != pl && pr >= render_clip_x1 && pr < render_clip_x2) {
						rowp[pr] = blend_pixel(rowp[pr], color, alpha);
					}
				}
			}
			fill_span_clipped(buf, bw, py, x + radius, x + w - radius, color);
		} else {
			fill_span_clipped(buf, bw, py, x, x + w, color);
		}
	}
}

// [=]===^=[ draw_rounded_rect ]===================================[=]
// 1px border via overdraw: fill the full rect with border color, then fill
// an inset rect with the background color. simpler than edge detection and
// naturally handles rounded corners
static void draw_rounded_rect(uint32_t *buf, int32_t bw, int32_t bh, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t fill, uint32_t border, int32_t radius) {
	draw_rounded_rect_fill(buf, bw, bh, x, y, w, h, border, radius);
	if(w > 2 && h > 2) {
		int32_t inner_r = radius > 1 ? radius - 1 : 0;
		draw_rounded_rect_fill(buf, bw, bh, x + 1, y + 1, w - 2, h - 2, fill, inner_r);
	}
}

// [=]===^=[ shade_color ]=========================================[=]
static inline uint32_t shade_color(uint32_t color, int32_t amount) {
	int32_t r = (int32_t)((color >> 16) & 0xff) + amount;
	int32_t g = (int32_t)((color >> 8) & 0xff) + amount;
	int32_t b = (int32_t)(color & 0xff) + amount;
	if(r < 0) {
		r = 0;

	} else if(r > 255) {
		r = 255;
	}
	if(g < 0) {
		g = 0;

	} else if(g > 255) {
		g = 255;
	}
	if(b < 0) {
		b = 0;

	} else if(b > 255) {
		b = 255;
	}
	return 0xff000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

// [=]===^=[ draw_hline ]========================================[=]
static inline void draw_hline(uint32_t *buf, int32_t bw, int32_t bh, int32_t x, int32_t y, int32_t w, uint32_t color) {
	if(y < 0 || y >= bh || y < render_clip_y1 || y >= render_clip_y2) {
		return;
	}
	int32_t x0 = x < 0 ? 0 : x;
	int32_t x1 = (x + w) > bw ? bw : (x + w);
	if(x0 < render_clip_x1) {
		x0 = render_clip_x1;
	}
	if(x1 > render_clip_x2) {
		x1 = render_clip_x2;
	}
	int32_t cnt = x1 - x0;
	if(cnt > 0) {
		fill_pixels(&buf[y * bw + x0], (uint32_t)cnt, color);
	}
}

// [=]===^=[ draw_vline ]========================================[=]
static inline void draw_vline(uint32_t *buf, int32_t bw, int32_t bh, int32_t x, int32_t y, int32_t h, uint32_t color) {
	if(x < 0 || x >= bw || x < render_clip_x1 || x >= render_clip_x2) {
		return;
	}
	int32_t y0 = y < 0 ? 0 : y;
	int32_t y1 = (y + h) > bh ? bh : (y + h);
	if(y0 < render_clip_y1) {
		y0 = render_clip_y1;
	}
	if(y1 > render_clip_y2) {
		y1 = render_clip_y2;
	}
	for(int32_t i = y0; i < y1; ++i) {
		buf[i * bw + x] = color;
	}
}

// [=]===^=[ draw_triangle_aa ]====================================[=]
static void draw_triangle_aa(uint32_t *buf, int32_t bw, int32_t bh,
	int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color) {
	int32_t tmp;
	if(y0 > y1) { tmp = x0; x0 = x1; x1 = tmp; tmp = y0; y0 = y1; y1 = tmp; }
	if(y0 > y2) { tmp = x0; x0 = x2; x2 = tmp; tmp = y0; y0 = y2; y2 = tmp; }
	if(y1 > y2) { tmp = x1; x1 = x2; x2 = tmp; tmp = y1; y1 = y2; y2 = tmp; }
	if(y0 == y2) {
		return;
	}
	int32_t min_x = x0 < x1 ? x0 : x1;
	if(x2 < min_x) { min_x = x2; }
	int32_t max_x = x0 > x1 ? x0 : x1;
	if(x2 > max_x) { max_x = x2; }
	int32_t span = max_x - min_x + 2;
	if(span > 256) { span = 256; }
	int32_t min_y = y0 < render_clip_y1 ? render_clip_y1 : y0;
	int32_t max_y = (y2 + 1) > render_clip_y2 ? render_clip_y2 : (y2 + 1);
	if(min_y < 0) { min_y = 0; }
	if(max_y > bh) { max_y = bh; }
	// 4x supersampling in Y (4 subrows per pixel), 8x in X (subpixel edge
	// precision). coverage per pixel accumulates over all 4 subrows; max
	// coverage is 32 (4 rows * 8 subpixel units width)
	int32_t y0_4 = y0 * 4;
	int32_t y1_4 = y1 * 4;
	int32_t y2_4 = y2 * 4;
	int32_t x0_8 = x0 * 8;
	int32_t x1_8 = x1 * 8;
	int32_t x2_8 = x2 * 8;
	for(int32_t sy = min_y; sy < max_y; ++sy) {
		int32_t cov[256];
		memset(cov, 0, (size_t)span * sizeof(int32_t));
		for(uint32_t sub = 0; sub < 4; ++sub) {
			int32_t sy4 = sy * 4 + (int32_t)sub;
			if(sy4 < y0_4 || sy4 >= y2_4) {
				continue;
			}
			int32_t el, er;
			int32_t la = (x2_8 - x0_8) * (sy4 - y0_4) / (y2_4 - y0_4) + x0_8;
			if(sy4 < y1_4) {
				if(y1_4 == y0_4) { continue; }
				int32_t s = (x1_8 - x0_8) * (sy4 - y0_4) / (y1_4 - y0_4) + x0_8;
				if(s < la) { el = s; er = la; } else { el = la; er = s; }
			} else {
				if(y2_4 == y1_4) { continue; }
				int32_t s = (x2_8 - x1_8) * (sy4 - y1_4) / (y2_4 - y1_4) + x1_8;
				if(s < la) { el = s; er = la; } else { el = la; er = s; }
			}
			int32_t px_l = el / 8;
			int32_t px_r = (er + 7) / 8;
			if(px_l < min_x) { px_l = min_x; }
			if(px_r > max_x + 2) { px_r = max_x + 2; }
			for(int32_t px = px_l; px < px_r; ++px) {
				int32_t ci = px - min_x;
				if(ci < 0 || ci >= span) { continue; }
				int32_t pl = px * 8;
				int32_t pr = pl + 8;
				int32_t cl = el > pl ? el : pl;
				int32_t cr = er < pr ? er : pr;
				if(cr > cl) {
					cov[ci] += cr - cl;
				}
			}
		}
		for(int32_t ci = 0; ci < span; ++ci) {
			if(cov[ci] <= 0) { continue; }
			int32_t px = min_x + ci;
			if(px < 0 || px >= bw || px < render_clip_x1 || px >= render_clip_x2) { continue; }
			uint32_t idx = (uint32_t)(sy * bw + px);
			int32_t alpha = cov[ci] * 255 / 32;
			if(alpha >= 255) {
				buf[idx] = color;
			} else {
				buf[idx] = blend_pixel(buf[idx], color, (uint8_t)alpha);
			}
		}
	}
}

// [=]===^=[ draw_rect_border ]==================================[=]
static void draw_rect_border(uint32_t *buf, int32_t bw, int32_t bh, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
	draw_hline(buf, bw, bh, x, y, w, color);
	draw_hline(buf, bw, bh, x, y + h - 1, w, color);
	draw_vline(buf, bw, bh, x, y, h, color);
	draw_vline(buf, bw, bh, x + w - 1, y, h, color);
}

// [=]===^=[ draw_rect_dashed ]===================================[=]
static void draw_rect_dashed(uint32_t *buf, int32_t bw, int32_t bh, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color, int32_t dash) {
	for(uint32_t i = 0; i < (uint32_t)w; ++i) {
		if(((int32_t)i / dash) & 1) {
			continue;
		}
		int32_t px = x + (int32_t)i;
		if(px >= 0 && px < bw && px >= render_clip_x1 && px < render_clip_x2) {
			if(y >= 0 && y < bh && y >= render_clip_y1 && y < render_clip_y2) {
				buf[y * bw + px] = color;
			}
			int32_t by = y + h - 1;
			if(by >= 0 && by < bh && by >= render_clip_y1 && by < render_clip_y2) {
				buf[by * bw + px] = color;
			}
		}
	}
	for(uint32_t i = 0; i < (uint32_t)h; ++i) {
		if(((int32_t)i / dash) & 1) {
			continue;
		}
		int32_t py = y + (int32_t)i;
		if(py >= 0 && py < bh && py >= render_clip_y1 && py < render_clip_y2) {
			if(x >= 0 && x < bw && x >= render_clip_x1 && x < render_clip_x2) {
				buf[py * bw + x] = color;
			}
			int32_t rx = x + w - 1;
			if(rx >= 0 && rx < bw && rx >= render_clip_x1 && rx < render_clip_x2) {
				buf[py * bw + rx] = color;
			}
		}
	}
}

enum {
	MKGUI_STYLE_RAISED,
	MKGUI_STYLE_SUNKEN,
};

// [=]===^=[ draw_patch ]==========================================[=]
static void draw_patch(struct mkgui_ctx *ctx, uint32_t style, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t fill, uint32_t border) {
	int32_t r = ctx->theme.corner_radius;
	draw_rounded_rect(ctx->pixels, ctx->win_w, ctx->win_h, x, y, w, h, fill, border, r);

	if(w > 2 && h > 2) {
		int32_t inner_r = r > 1 ? r - 1 : 0;
		int32_t insets[MKGUI_MAX_CORNER_RADIUS];
		int32_t hl_inset = 0;
		if(inner_r > 0) {
			rounded_rect_insets(inner_r, insets);
			hl_inset = insets[0];
		}
		int32_t hl_y = y + 1;
		if(hl_y >= 0 && hl_y < ctx->win_h && hl_y >= render_clip_y1 && hl_y < render_clip_y2) {
			uint32_t hl;
			if(style == MKGUI_STYLE_SUNKEN) {
				hl = shade_color(fill, -6);
			} else {
				hl = shade_color(fill, 8);
			}
			fill_span_clipped(ctx->pixels, ctx->win_w, hl_y, x + 1 + hl_inset, x + w - 1 - hl_inset, hl);
		}
	}
}

// ---------------------------------------------------------------------------
// Software text renderer
// ---------------------------------------------------------------------------

// [=]===^=[ utf8_decode ]========================================[=]
static uint32_t utf8_decode(char *p, uint32_t *out_cp) {
	uint8_t b = (uint8_t)p[0];
	if(b < 0x80) {
		*out_cp = b;
		return 1;
	}
	if((b & 0xe0) == 0xc0 && (p[1] & 0xc0) == 0x80) {
		*out_cp = ((uint32_t)(b & 0x1f) << 6) | (uint32_t)(p[1] & 0x3f);
		return 2;
	}
	if((b & 0xf0) == 0xe0 && (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80) {
		*out_cp = ((uint32_t)(b & 0x0f) << 12) | ((uint32_t)(p[1] & 0x3f) << 6) | (uint32_t)(p[2] & 0x3f);
		return 3;
	}
	if((b & 0xf8) == 0xf0 && (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80 && (p[3] & 0xc0) == 0x80) {
		*out_cp = ((uint32_t)(b & 0x07) << 18) | ((uint32_t)(p[1] & 0x3f) << 12) | ((uint32_t)(p[2] & 0x3f) << 6) | (uint32_t)(p[3] & 0x3f);
		return 4;
	}
	*out_cp = 0xfffd;
	return 1;
}

// [=]===^=[ draw_text_sw ]=======================================[=]
static void draw_text_sw(struct mkgui_ctx *ctx, uint32_t *buf, int32_t bw, int32_t x, int32_t y, char *text, uint32_t color, int32_t cx1, int32_t cy1, int32_t cx2, int32_t cy2) {
	int32_t cx = x;
	for(char *p = text; *p; ) {
		uint32_t ch;
		uint32_t bytes = utf8_decode(p, &ch);
		p += bytes;
		if(ch < MKGUI_GLYPH_FIRST || ch > MKGUI_GLYPH_LAST) {
			cx += ctx->char_width;
			continue;
		}
		struct mkgui_glyph *g = &ctx->glyphs[ch - MKGUI_GLYPH_FIRST];

		int32_t gx = cx + g->bearing_x;
		if(gx >= cx2) {
			break;
		}
		if(gx + g->width <= cx1) {
			cx += g->advance;
			continue;
		}

		int32_t gy = y + ctx->font_ascent - g->bearing_y;
		int32_t col0 = cx1 - gx;
		int32_t col1 = cx2 - gx;
		if(col0 < 0) {
			col0 = 0;
		}
		if(col1 > g->width) {
			col1 = g->width;
		}
		int32_t row0 = cy1 - gy;
		int32_t row1 = cy2 - gy;
		if(row0 < 0) {
			row0 = 0;
		}
		if(row1 > g->height) {
			row1 = g->height;
		}

		uint32_t rb_src = color & 0x00ff00ff;
		uint32_t g_src = color & 0x0000ff00;
		for(int32_t row = row0; row < row1; ++row) {
			int32_t py = gy + row;
			blend_glyph_row(&buf[py * bw], &glyph_atlas[(g->atlas_y + row) * glyph_atlas_w + g->atlas_x], col0, col1, gx, rb_src, g_src);
		}

		cx += g->advance;
	}
}

// ---------------------------------------------------------------------------
// Text command buffer
// ---------------------------------------------------------------------------

struct mkgui_text_cmd {
	int32_t x, y;
	int32_t clip_x1, clip_y1, clip_x2, clip_y2;
	char text[MKGUI_MAX_TEXT];
	uint32_t color;
};

static struct vm_arena text_cmd_arena;
static struct mkgui_text_cmd *text_cmds;
static uint32_t text_cmd_count;

// [=]===^=[ text_cmd_init ]======================================[=]
static uint32_t text_cmd_init(void) {
	if(!vm_arena_create(&text_cmd_arena, (size_t)MKGUI_VM_MAX_TEXT_CMDS * sizeof(struct mkgui_text_cmd))) {
		return 0;
	}
	text_cmds = (struct mkgui_text_cmd *)text_cmd_arena.base;
	return 1;
}

// [=]===^=[ text_cmd_fini ]======================================[=]
static void text_cmd_fini(void) {
	vm_arena_destroy(&text_cmd_arena);
	text_cmds = NULL;
	text_cmd_count = 0;
}

// [=]===^=[ push_text_clip ]=====================================[=]
static void push_text_clip(int32_t x, int32_t y, char *text, uint32_t color, int32_t cx1, int32_t cy1, int32_t cx2, int32_t cy2) {
	if(text_cmd_count >= MKGUI_VM_MAX_TEXT_CMDS) {
		return;
	}
	if(!vm_arena_ensure(&text_cmd_arena, (size_t)(text_cmd_count + 1) * sizeof(struct mkgui_text_cmd))) {
		return;
	}
	if(cx1 < render_clip_x1) {
		cx1 = render_clip_x1;
	}
	if(cy1 < render_clip_y1) {
		cy1 = render_clip_y1;
	}
	if(cx2 > render_clip_x2) {
		cx2 = render_clip_x2;
	}
	if(cy2 > render_clip_y2) {
		cy2 = render_clip_y2;
	}
	struct mkgui_text_cmd *cmd = &text_cmds[text_cmd_count++];
	cmd->x = x;
	cmd->y = y;
	cmd->clip_x1 = cx1;
	cmd->clip_y1 = cy1;
	cmd->clip_x2 = cx2;
	cmd->clip_y2 = cy2;
	cmd->color = color;
	size_t slen = strlen(text);
	if(slen >= MKGUI_MAX_TEXT) {
		slen = MKGUI_MAX_TEXT - 1;
	}
	memcpy(cmd->text, text, slen);
	cmd->text[slen] = '\0';
}

// [=]===^=[ push_text ]=========================================[=]
static void push_text(int32_t x, int32_t y, char *text, uint32_t color) {
	push_text_clip(x, y, text, color, render_clip_x1, render_clip_y1, render_clip_x2, render_clip_y2);
}

// [=]===^=[ clip_intersect ]=====================================[=]
static void clip_intersect(int32_t *x1, int32_t *y1, int32_t *x2, int32_t *y2, int32_t bx1, int32_t by1, int32_t bx2, int32_t by2) {
	if(*x1 < bx1) {
		*x1 = bx1;
	}
	if(*y1 < by1) {
		*y1 = by1;
	}
	if(*x2 > bx2) {
		*x2 = bx2;
	}
	if(*y2 > by2) {
		*y2 = by2;
	}
}

// [=]===^=[ flush_text_to ]======================================[=]
// text commands are queued in screen coordinates; ox/oy translate them into
// the target buffer's local space so the same queue can flush to either the
// main framebuffer (ox=0,oy=0) or a popup window at its own origin
static void flush_text_to(struct mkgui_ctx *ctx, uint32_t *buf, int32_t bw, int32_t bh, int32_t ox, int32_t oy) {
	for(uint32_t i = 0; i < text_cmd_count; ++i) {
		struct mkgui_text_cmd *cmd = &text_cmds[i];
		int32_t cx1 = cmd->clip_x1 - ox, cy1 = cmd->clip_y1 - oy;
		int32_t cx2 = cmd->clip_x2 - ox, cy2 = cmd->clip_y2 - oy;
		clip_intersect(&cx1, &cy1, &cx2, &cy2, 0, 0, bw, bh);
		if(cx1 >= cx2 || cy1 >= cy2) {
			continue;
		}
		draw_text_sw(ctx, buf, bw, cmd->x - ox, cmd->y - oy, cmd->text, cmd->color, cx1, cy1, cx2, cy2);
	}
	text_cmd_count = 0;
}

// [=]===^=[ flush_text ]========================================[=]
static void flush_text(struct mkgui_ctx *ctx) {
	flush_text_to(ctx, ctx->pixels, ctx->win_w, ctx->win_h, 0, 0);
}

// [=]===^=[ flush_text_popup ]==================================[=]
static void flush_text_popup(struct mkgui_ctx *ctx, struct mkgui_popup *p) {
	flush_text_to(ctx, p->pixels, p->w, p->h, p->x, p->y);
}

// [=]===^=[ text_width ]========================================[=]
static int32_t text_width(struct mkgui_ctx *ctx, char *text) {
	int32_t w = 0;
	for(char *p = text; *p; ) {
		uint32_t ch;
		uint32_t bytes = utf8_decode(p, &ch);
		p += bytes;
		if(ch >= MKGUI_GLYPH_FIRST && ch <= MKGUI_GLYPH_LAST) {
			w += ctx->glyphs[ch - MKGUI_GLYPH_FIRST].advance;
		} else {
			w += ctx->char_width;
		}
	}
	return w;
}

// [=]===^=[ label_text_width ]=====================================[=]
static int32_t label_text_width(struct mkgui_ctx *ctx, struct mkgui_widget *w) {
	if(w->label_tw >= 0) {
		return w->label_tw;
	}
	w->label_tw = text_width(ctx, w->label);
	return w->label_tw;
}

// [=]===^=[ text_truncate ]=====================================[=]
static char *text_truncate(struct mkgui_ctx *ctx, char *text, int32_t max_w) {
	static char buf[MKGUI_MAX_TEXT];
	int32_t tw = text_width(ctx, text);
	if(tw <= max_w) {
		return text;
	}
	int32_t dots_w = 0;
	for(uint32_t i = 0; i < 3; ++i) {
		dots_w += ctx->glyphs['.' - MKGUI_GLYPH_FIRST].advance;
	}
	int32_t budget = max_w - dots_w;
	if(budget <= 0) {
		buf[0] = '\0';
		return buf;
	}
	int32_t w = 0;
	uint32_t n = 0;
	for(char *p = text; *p && n < MKGUI_MAX_TEXT - 4; ) {
		uint32_t ch;
		uint32_t bytes = utf8_decode(p, &ch);
		int32_t adv;
		if(ch >= MKGUI_GLYPH_FIRST && ch <= MKGUI_GLYPH_LAST) {
			adv = ctx->glyphs[ch - MKGUI_GLYPH_FIRST].advance;
		} else {
			adv = ctx->char_width;
		}
		if(w + adv > budget) {
			break;
		}
		for(uint32_t bi = 0; bi < bytes && n < MKGUI_MAX_TEXT - 4; ++bi) {
			buf[n++] = p[bi];
		}
		p += bytes;
		w += adv;
	}
	buf[n++] = '.';
	buf[n++] = '.';
	buf[n++] = '.';
	buf[n] = '\0';
	return buf;
}
