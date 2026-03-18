// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define MKGUI_SPINNER_SPEED 1.0
#define MKGUI_SPINNER_ARC   230

// [=]===^=[ iatan2_deg10 ]=========================================[=]
static int32_t iatan2_deg10(int32_t y, int32_t x) {
	if(x == 0 && y == 0) {
		return 0;
	}
	int32_t ax = x < 0 ? -x : x;
	int32_t ay = y < 0 ? -y : y;
	int32_t mn = ax < ay ? ax : ay;
	int32_t mx = ax > ay ? ax : ay;
	int32_t a = (int32_t)((int64_t)mn * 450 / (mx + 1));
	if(ay > ax) {
		a = 900 - a;
	}
	if(x < 0) {
		a = 1800 - a;
	}
	if(y > 0) {
		a = 3600 - a;
	}
	if(a < 0) {
		a += 3600;
	}
	if(a >= 3600) {
		a -= 3600;
	}
	return a;
}

// [=]===^=[ arc_coverage ]=========================================[=]
static uint32_t arc_coverage(int32_t dx, int32_t dy, int32_t outer_r, int32_t inner_r, int32_t ang_start, int32_t ang_end) {
	int32_t or8 = 8 * outer_r;
	int32_t ir8 = 8 * inner_r;
	int32_t or8sq = or8 * or8;
	int32_t ir8sq = ir8 * ir8;
	int32_t base_dx8 = 8 * dx;
	int32_t base_dy8 = 8 * dy;
	uint32_t count = 0;
	for(uint32_t sy = 0; sy < 4; ++sy) {
		int32_t sdy = base_dy8 + 2 * (int32_t)sy + 1;
		for(uint32_t sx = 0; sx < 4; ++sx) {
			int32_t sdx = base_dx8 + 2 * (int32_t)sx + 1;
			int32_t d2 = sdx * sdx + sdy * sdy;
			if(d2 > or8sq || d2 < ir8sq) {
				continue;
			}
			int32_t ang = iatan2_deg10(-sdy, sdx);
			if(ang_start <= ang_end) {
				if(ang >= ang_start && ang < ang_end) {
					++count;
				}
			} else {
				if(ang >= ang_start || ang < ang_end) {
					++count;
				}
			}
		}
	}
	return count;
}

// [=]===^=[ render_spinner ]=======================================[=]
static void render_spinner(struct mkgui_ctx *ctx, uint32_t idx) {
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	int32_t size = rw < rh ? rw : rh;
	int32_t outer_r = size / 2 - 1;
	if(outer_r < 4) {
		outer_r = 4;
	}
	int32_t inner_r = outer_r * 6 / 10;
	int32_t cx = rx + rw / 2;
	int32_t cy = ry + rh / 2;

	double phase = ctx->anim_time * MKGUI_SPINNER_SPEED;
	phase -= (double)(int32_t)phase;
	int32_t ang_start = (int32_t)(phase * 3600.0) % 3600;
	int32_t ang_end = (ang_start + MKGUI_SPINNER_ARC * 10) % 3600;

	int32_t or_inner = outer_r - 2;
	int32_t ir_outer = inner_r + 2;
	int32_t or_inner_sq = or_inner * or_inner;
	int32_t ir_outer_sq = ir_outer * ir_outer;

	for(int32_t dy = -outer_r; dy <= outer_r; ++dy) {
		int32_t py = cy + dy;
		if(py < 0 || py >= ctx->win_h) {
			continue;
		}
		for(int32_t dx = -outer_r; dx <= outer_r; ++dx) {
			int32_t px = cx + dx;
			if(px < 0 || px >= ctx->win_w) {
				continue;
			}
			int32_t d2 = dx * dx + dy * dy;
			if(d2 > (outer_r + 1) * (outer_r + 1) || d2 < (inner_r - 1) * (inner_r - 1)) {
				continue;
			}
			if(d2 <= or_inner_sq && d2 >= ir_outer_sq) {
				int32_t ang = iatan2_deg10(-dy, dx);
				uint32_t in_arc;
				if(ang_start <= ang_end) {
					in_arc = (ang >= ang_start && ang < ang_end);
				} else {
					in_arc = (ang >= ang_start || ang < ang_end);
				}
				if(in_arc) {
					ctx->pixels[py * ctx->win_w + px] = ctx->theme.accent;
				}
				continue;
			}
			uint32_t cov = arc_coverage(dx, dy, outer_r, inner_r, ang_start, ang_end);
			if(cov > 0) {
				uint8_t alpha = (uint8_t)(cov * 255 / 16);
				ctx->pixels[py * ctx->win_w + px] = blend_pixel(ctx->pixels[py * ctx->win_w + px], ctx->theme.accent, alpha);
			}
		}
	}
}
