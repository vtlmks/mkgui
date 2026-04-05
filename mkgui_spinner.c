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

// [=]===^=[ isqrt_u32 ]============================================[=]
static uint32_t isqrt_u32(uint32_t n) {
	if(n == 0) {
		return 0;
	}
	uint32_t x = n;
	uint32_t y = (x + 1) / 2;
	while(y < x) {
		x = y;
		y = (x + n / x) / 2;
	}
	return x;
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
	int32_t arc_len = MKGUI_SPINNER_ARC * 10;

	int32_t outer_sq = outer_r * outer_r;
	int32_t inner_sq = inner_r * inner_r;
	int32_t or_inner_sq = (outer_r - 1) * (outer_r - 1);
	int32_t ir_outer_sq = (inner_r + 1) * (inner_r + 1);
	int32_t outer_aa = outer_sq - or_inner_sq;
	int32_t inner_aa = ir_outer_sq - inner_sq;
	if(outer_aa < 1) {
		outer_aa = 1;
	}
	if(inner_aa < 1) {
		inner_aa = 1;
	}

	int32_t dy0 = -outer_r;
	int32_t dy1 = outer_r;
	int32_t dx0 = -outer_r;
	int32_t dx1 = outer_r;
	if(cy + dy0 < 0) {
		dy0 = -cy;
	}
	if(cy + dy1 >= ctx->win_h) {
		dy1 = ctx->win_h - 1 - cy;
	}
	if(cx + dx0 < 0) {
		dx0 = -cx;
	}
	if(cx + dx1 >= ctx->win_w) {
		dx1 = ctx->win_w - 1 - cx;
	}
	if(cy + dy0 < render_clip_y1) {
		dy0 = render_clip_y1 - cy;
	}
	if(cy + dy1 >= render_clip_y2) {
		dy1 = render_clip_y2 - 1 - cy;
	}
	if(cx + dx0 < render_clip_x1) {
		dx0 = render_clip_x1 - cx;
	}
	if(cx + dx1 >= render_clip_x2) {
		dx1 = render_clip_x2 - 1 - cx;
	}

	uint32_t accent = ctx->theme.accent;
	for(int32_t dy = dy0; dy <= dy1; ++dy) {
		int32_t dy2 = dy * dy;
		int32_t rem_out = outer_sq - dy2;
		if(rem_out < 0) {
			continue;
		}
		int32_t x_out = (int32_t)isqrt_u32((uint32_t)rem_out);
		int32_t rem_in = inner_sq - dy2;
		int32_t py = cy + dy;
		uint32_t *row = &ctx->pixels[py * ctx->win_w];

		int32_t ra0 = -x_out;
		int32_t ra1 = x_out;
		int32_t rb0 = 1;
		int32_t rb1 = 0;
		if(rem_in > 0) {
			int32_t s = (int32_t)isqrt_u32((uint32_t)rem_in);
			int32_t x_in = (s * s < rem_in) ? s + 1 : s;
			ra1 = -x_in;
			rb0 = x_in;
			rb1 = x_out;
		}
		if(ra0 < dx0) {
			ra0 = dx0;
		}
		if(ra1 > dx1) {
			ra1 = dx1;
		}
		if(rb0 < dx0) {
			rb0 = dx0;
		}
		if(rb1 > dx1) {
			rb1 = dx1;
		}

		for(uint32_t pass = 0; pass < 2; ++pass) {
			int32_t dxa = pass == 0 ? ra0 : rb0;
			int32_t dxb = pass == 0 ? ra1 : rb1;
			if(dxa > dxb) {
				continue;
			}
			int32_t d2 = dxa * dxa + dy2;
			int32_t d2_step = 2 * dxa + 1;
			for(int32_t dx = dxa; dx <= dxb; ++dx) {
				int32_t ang = iatan2_deg10(-dy, dx);
				int32_t diff = ang - ang_start;
				if(diff < 0) {
					diff += 3600;
				}
				if(diff < arc_len) {
					uint32_t alpha = 255;
					if(d2 > or_inner_sq) {
						alpha = (uint32_t)((outer_sq - d2) * 255 / outer_aa);
					} else if(d2 < ir_outer_sq) {
						alpha = (uint32_t)((d2 - inner_sq) * 255 / inner_aa);
					}
					int32_t px = cx + dx;
					if(alpha >= 255) {
						row[px] = accent;
					} else if(alpha > 0) {
						row[px] = blend_pixel(row[px], accent, (uint8_t)alpha);
					}
				}
				d2 += d2_step;
				d2_step += 2;
			}
		}
	}
}
