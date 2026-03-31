// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// ---------------------------------------------------------------------------
// Skyline rect packer
// ---------------------------------------------------------------------------
//
// Packs N rectangles into a single atlas. Uses the skyline bottom-left
// algorithm: maintain a 1D height profile (skyline) across the atlas width,
// and for each rect choose the position that raises the skyline the least.
//
// Usage:
//   struct atlas_rect rects[N] = { {.w = 10, .h = 12}, ... };
//   struct atlas_result res;
//   atlas_pack(rects, N, &res);
//   // rects[i].x, rects[i].y now contain packed positions
//   // res.w, res.h contain the atlas dimensions (power of 2)
//
// The caller provides the rect array. The packer writes (x, y) into each
// element and returns the atlas dimensions. No heap allocations.

#define ATLAS_MAX_RECTS   16384
#define ATLAS_MAX_SKYLINE 4096
#define ATLAS_MIN_SIZE    64
#define ATLAS_MAX_SIZE    4096
#define ATLAS_PAD         1

struct atlas_rect {
	int32_t w, h;
	int32_t x, y;
	uint32_t idx;
};

struct atlas_result {
	int32_t w, h;
};

// [=]===^=[ atlas_next_pow2 ]======================================[=]
static uint32_t atlas_next_pow2(uint32_t v) {
	--v;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	return v + 1;
}

// [=]===^=[ atlas_pack ]===========================================[=]
static uint32_t atlas_pack(struct atlas_rect *rects, uint32_t count, struct atlas_result *out) {
	if(count == 0) {
		out->w = 0;
		out->h = 0;
		return 1;
	}

	// Sort rects by height descending (tall first packs better)
	for(uint32_t i = 1; i < count; ++i) {
		struct atlas_rect key = rects[i];
		uint32_t j = i;
		while(j > 0 && rects[j - 1].h < key.h) {
			rects[j] = rects[j - 1];
			--j;
		}
		rects[j] = key;
	}

	// Compute total area and max dimensions for initial size estimate
	uint32_t total_area = 0;
	int32_t max_w = 0;
	int32_t max_h = 0;
	for(uint32_t i = 0; i < count; ++i) {
		int32_t pw = rects[i].w + ATLAS_PAD;
		int32_t ph = rects[i].h + ATLAS_PAD;
		total_area += (uint32_t)(pw * ph);
		if(pw > max_w) {
			max_w = pw;
		}
		if(ph > max_h) {
			max_h = ph;
		}
	}

	// Start with smallest power-of-2 that fits the area
	uint32_t side = atlas_next_pow2((uint32_t)sqrtf((float)total_area));
	if(side < (uint32_t)max_w) {
		side = atlas_next_pow2((uint32_t)max_w);
	}
	if(side < ATLAS_MIN_SIZE) {
		side = ATLAS_MIN_SIZE;
	}

	// Try increasing atlas sizes until everything fits
	for(; side <= ATLAS_MAX_SIZE; side *= 2) {
		int32_t aw = (int32_t)side;
		int32_t ah = (int32_t)side;

		// Skyline: array of (x, y, width) spans
		int32_t sky_x[ATLAS_MAX_SKYLINE];
		int32_t sky_y[ATLAS_MAX_SKYLINE];
		int32_t sky_w[ATLAS_MAX_SKYLINE];
		uint32_t sky_count = 1;
		sky_x[0] = 0;
		sky_y[0] = 0;
		sky_w[0] = aw;

		uint32_t packed = 0;
		for(uint32_t i = 0; i < count; ++i) {
			int32_t rw = rects[i].w + ATLAS_PAD;
			int32_t rh = rects[i].h + ATLAS_PAD;

			// Find best position: span with lowest y where rect fits
			int32_t best_y = ah + 1;
			uint32_t best_idx = 0;
			int32_t best_x = 0;

			for(uint32_t s = 0; s < sky_count; ++s) {
				if(sky_x[s] + rw > aw) {
					continue;
				}
				// Check max height across all spans this rect would cover
				int32_t max_sky_y = sky_y[s];
				int32_t covered = sky_w[s];
				uint32_t end = s;
				while(covered < rw && end + 1 < sky_count) {
					++end;
					if(sky_y[end] > max_sky_y) {
						max_sky_y = sky_y[end];
					}
					covered += sky_w[end];
				}
				if(covered < rw) {
					continue;
				}
				if(max_sky_y + rh <= ah && max_sky_y < best_y) {
					best_y = max_sky_y;
					best_idx = s;
					best_x = sky_x[s];
				}
			}

			if(best_y > ah) {
				break;
			}

			rects[i].x = best_x;
			rects[i].y = best_y;
			++packed;

			// Update skyline: replace covered spans with new span at (best_x, best_y + rh, rw)
			// then trim/split partially covered spans
			int32_t new_right = best_x + rw;
			uint32_t first = best_idx;
			uint32_t last = first;
			while(last + 1 < sky_count && sky_x[last] + sky_w[last] <= new_right) {
				++last;
			}
			// last is now the first span NOT fully covered (or == sky_count)
			// Check if last span is partially covered
			if(last < sky_count && sky_x[last] < new_right) {
				// Split: trim left edge of last span
				int32_t trim = new_right - sky_x[last];
				sky_x[last] += trim;
				sky_w[last] -= trim;
				if(sky_w[last] <= 0) {
					++last;
				}
			}

			// Build new skyline: [0..first) + new_span + [last..sky_count)
			int32_t new_sky_x[ATLAS_MAX_SKYLINE];
			int32_t new_sky_y[ATLAS_MAX_SKYLINE];
			int32_t new_sky_w[ATLAS_MAX_SKYLINE];
			uint32_t nc = 0;

			for(uint32_t s = 0; s < first; ++s) {
				new_sky_x[nc] = sky_x[s];
				new_sky_y[nc] = sky_y[s];
				new_sky_w[nc] = sky_w[s];
				++nc;
			}

			// Check if left neighbor has same height — merge
			int32_t new_span_y = best_y + rh;
			if(nc > 0 && new_sky_y[nc - 1] == new_span_y) {
				new_sky_w[nc - 1] += rw;
			} else {
				new_sky_x[nc] = best_x;
				new_sky_y[nc] = new_span_y;
				new_sky_w[nc] = rw;
				++nc;
			}

			// Right remainder of the first partially covered span
			if(first < last) {
				int32_t orig_right = sky_x[first] + sky_w[first];
				if(orig_right > new_right) {
					int32_t rem_w = orig_right - new_right;
					if(nc > 0 && new_sky_y[nc - 1] == sky_y[first]) {
						new_sky_w[nc - 1] += rem_w;
					} else {
						new_sky_x[nc] = new_right;
						new_sky_y[nc] = sky_y[first];
						new_sky_w[nc] = rem_w;
						++nc;
					}
				}
			}

			for(uint32_t s = last; s < sky_count; ++s) {
				// Merge with previous if same height
				if(nc > 0 && new_sky_y[nc - 1] == sky_y[s] && new_sky_x[nc - 1] + new_sky_w[nc - 1] == sky_x[s]) {
					new_sky_w[nc - 1] += sky_w[s];
				} else {
					new_sky_x[nc] = sky_x[s];
					new_sky_y[nc] = sky_y[s];
					new_sky_w[nc] = sky_w[s];
					++nc;
				}
			}

			memcpy(sky_x, new_sky_x, nc * sizeof(int32_t));
			memcpy(sky_y, new_sky_y, nc * sizeof(int32_t));
			memcpy(sky_w, new_sky_w, nc * sizeof(int32_t));
			sky_count = nc;
		}

		if(packed == count) {
			// Find actual height used
			int32_t used_h = 0;
			for(uint32_t i = 0; i < count; ++i) {
				int32_t bot = rects[i].y + rects[i].h;
				if(bot > used_h) {
					used_h = bot;
				}
			}
			out->w = aw;
			out->h = (int32_t)atlas_next_pow2((uint32_t)used_h);
			if(out->h < ATLAS_MIN_SIZE) {
				out->h = ATLAS_MIN_SIZE;
			}
			if(out->h > ah) {
				out->h = ah;
			}
			return 1;
		}
	}

	out->w = 0;
	out->h = 0;
	return 0;
}
