// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define CP_WINDOW      0xffc000
#define CP_VBOX        0xffc001
#define CP_TABS        0xffc002
#define CP_TAB_SV      0xffc003
#define CP_TAB_WHEEL   0xffc004
#define CP_TAB_RGB     0xffc005
#define CP_SV_HBOX     0xffc010
#define CP_SV_CANVAS   0xffc011
#define CP_HUE_CANVAS  0xffc012
#define CP_WH_CANVAS   0xffc013
#define CP_RGB_CANVAS  0xffc014
#define CP_DIVIDER     0xffc015
#define CP_CTL_HBOX    0xffc020
#define CP_PREVIEW     0xffc021
#define CP_HEX_LBL     0xffc022
#define CP_HEX_INPUT   0xffc023
#define CP_R_LBL       0xffc024
#define CP_R_SPN       0xffc025
#define CP_G_LBL       0xffc026
#define CP_G_SPN       0xffc027
#define CP_B_LBL       0xffc028
#define CP_B_SPN       0xffc029
#define CP_BTN_HBOX    0xffc030
#define CP_BTN_SPACER  0xffc031
#define CP_BTN_OK      0xffc032
#define CP_BTN_CANCEL  0xffc033

static struct {
	float h, s, v;
	uint32_t drag_target;
	uint32_t *sv_cache;
	int32_t sv_w, sv_h;
	float sv_hue;
	uint32_t *hue_cache;
	int32_t hue_w, hue_h;
	uint32_t *wheel_cache;
	int32_t wheel_w, wheel_h;
	float wheel_hue;
} cp_state;

// [=]===^=[ cp_hsv_to_rgb ]========================================[=]
static uint32_t cp_hsv_to_rgb(float h, float s, float v) {
	int32_t hi = (int32_t)(h * (256.0f / 60.0f));
	if(hi < 0) { hi = 0; }
	if(hi >= 1536) { hi -= 1536; }
	int32_t sector = hi >> 8;
	int32_t frac = hi & 0xff;
	int32_t vi = (int32_t)(v * 255.0f + 0.5f);
	int32_t ci = vi * (int32_t)(s * 255.0f + 0.5f) / 255;
	int32_t xf = (sector & 1) ? (255 - frac) : frac;
	int32_t xi = ci * xf >> 8;
	int32_t mi = vi - ci;
	int32_t r, g, b;
	switch(sector) {
		case 0: {
			r = ci + mi; g = xi + mi; b = mi;
		} break;

		case 1: {
			r = xi + mi; g = ci + mi; b = mi;
		} break;

		case 2: {
			r = mi; g = ci + mi; b = xi + mi;
		} break;

		case 3: {
			r = mi; g = xi + mi; b = ci + mi;
		} break;

		case 4: {
			r = xi + mi; g = mi; b = ci + mi;
		} break;

		default: {
			r = ci + mi; g = mi; b = xi + mi;
		} break;

	}
	return 0xff000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

// [=]===^=[ cp_rgb_to_hsv ]========================================[=]
static void cp_rgb_to_hsv(uint32_t color, float *oh, float *os, float *ov) {
	float r = (float)((color >> 16) & 0xff) / 255.0f;
	float g = (float)((color >> 8) & 0xff) / 255.0f;
	float b = (float)(color & 0xff) / 255.0f;
	float mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
	float mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
	float d = mx - mn;
	*ov = mx;
	*os = (mx > 0.0f) ? d / mx : 0.0f;
	if(d < 0.0001f) {
		*oh = 0.0f;
	} else if(mx == r) {
		*oh = 60.0f * fmodf((g - b) / d + 6.0f, 6.0f);
	} else if(mx == g) {
		*oh = 60.0f * ((b - r) / d + 2.0f);
	} else {
		*oh = 60.0f * ((r - g) / d + 4.0f);
	}
}

// [=]===^=[ cp_current_rgb ]=======================================[=]
static uint32_t cp_current_rgb(void) {
	return cp_hsv_to_rgb(cp_state.h, cp_state.s, cp_state.v);
}

// [=]===^=[ cp_sync_controls ]=====================================[=]
static void cp_sync_controls(struct mkgui_ctx *ctx) {
	uint32_t rgb = cp_current_rgb();
	uint32_t r = (rgb >> 16) & 0xff;
	uint32_t g = (rgb >> 8) & 0xff;
	uint32_t b = rgb & 0xff;
	mkgui_spinbox_set(ctx, CP_R_SPN, (int32_t)r);
	mkgui_spinbox_set(ctx, CP_G_SPN, (int32_t)g);
	mkgui_spinbox_set(ctx, CP_B_SPN, (int32_t)b);
	char hex[8];
	snprintf(hex, sizeof(hex), "%02x%02x%02x", r, g, b);
	mkgui_input_set(ctx, CP_HEX_INPUT, hex);
}

// [=]===^=[ cp_cache_realloc ]=====================================[=]
static uint32_t *cp_cache_realloc(uint32_t *buf, int32_t *old_w, int32_t *old_h, int32_t new_w, int32_t new_h) {
	if(buf && *old_w == new_w && *old_h == new_h) {
		return buf;
	}
	free(buf);
	*old_w = new_w;
	*old_h = new_h;
	return (uint32_t *)malloc((size_t)new_w * (size_t)new_h * sizeof(uint32_t));
}

// [=]===^=[ cp_cache_blit ]========================================[=]
static void cp_cache_blit(uint32_t *dst, int32_t dw, int32_t dh, int32_t rx, int32_t ry, uint32_t *src, int32_t sw, int32_t sh) {
	for(int32_t y = 0; y < sh; ++y) {
		int32_t py = ry + y;
		if(py < 0 || py >= dh) { continue; }
		int32_t x0 = rx < 0 ? -rx : 0;
		int32_t x1 = rx + sw > dw ? dw - rx : sw;
		if(x1 > x0) {
			memcpy(dst + py * dw + rx + x0, src + y * sw + x0, (size_t)(x1 - x0) * sizeof(uint32_t));
		}
	}
}

// [=]===^=[ cp_render_sv_base ]====================================[=]
static void cp_render_sv_base(uint32_t *buf, int32_t rw, int32_t rh) {
	int32_t hi = (int32_t)(cp_state.h * (256.0f / 60.0f));
	if(hi < 0) { hi = 0; }
	if(hi >= 1536) { hi -= 1536; }
	int32_t sector = hi >> 8;
	int32_t frac = hi & 0xff;
	int32_t x_scale = (sector & 1) ? (255 - frac) : frac;
	uint32_t shift_a, shift_b, shift_z;
	switch(sector) {
		case 0:  { shift_a = 16; shift_b = 8;  shift_z = 0;  } break;
		case 1:  { shift_a = 8;  shift_b = 16; shift_z = 0;  } break;
		case 2:  { shift_a = 8;  shift_b = 0;  shift_z = 16; } break;
		case 3:  { shift_a = 0;  shift_b = 8;  shift_z = 16; } break;
		case 4:  { shift_a = 0;  shift_b = 16; shift_z = 8;  } break;
		default: { shift_a = 16; shift_b = 0;  shift_z = 8;  } break;
	}
	uint32_t rw1 = (uint32_t)(rw - 1);
	uint32_t rh1 = (uint32_t)(rh - 1);
	for(int32_t y = 0; y < rh; ++y) {
		int32_t vi = 255 - (int32_t)((uint32_t)y * 255 / rh1);
		uint32_t *row = buf + y * rw;
		for(int32_t x = 0; x < rw; ++x) {
			int32_t si = (int32_t)((uint32_t)x * 255 / rw1);
			int32_t ci = vi * si / 255;
			int32_t xi = ci * x_scale >> 8;
			int32_t mi = vi - ci;
			row[x] = 0xff000000 | ((uint32_t)(ci + mi) << shift_a) | ((uint32_t)(xi + mi) << shift_b) | ((uint32_t)mi << shift_z);
		}
	}
}

// [=]===^=[ cp_render_sv_cursor ]==================================[=]
static void cp_render_sv_cursor(uint32_t *pixels, int32_t bw, int32_t bh, int32_t rx, int32_t ry, int32_t rw, int32_t rh) {
	int32_t cx = rx + (int32_t)(cp_state.s * (float)(rw - 1));
	int32_t cy = ry + (int32_t)((1.0f - cp_state.v) * (float)(rh - 1));
	uint32_t mc = (cp_state.v > 0.5f) ? 0xff000000 : 0xffffffff;
	for(int32_t d = -4; d <= 4; ++d) {
		if(cy >= 0 && cy < bh && cx + d >= 0 && cx + d < bw) { pixels[cy * bw + cx + d] = mc; }
		if(cx >= 0 && cx < bw && cy + d >= 0 && cy + d < bh) { pixels[(cy + d) * bw + cx] = mc; }
	}
}

// [=]===^=[ cp_render_hue_base ]===================================[=]
static void cp_render_hue_base(uint32_t *buf, int32_t rw, int32_t rh) {
	for(int32_t y = 0; y < rh; ++y) {
		float hue = (float)y / (float)(rh - 1) * 360.0f;
		uint32_t c = cp_hsv_to_rgb(hue, 1.0f, 1.0f);
		uint32_t *row = buf + y * rw;
		for(int32_t x = 0; x < rw; ++x) {
			row[x] = c;
		}
	}
}

// [=]===^=[ cp_render_hue_cursor ]=================================[=]
static void cp_render_hue_cursor(uint32_t *pixels, int32_t bw, int32_t bh, int32_t rx, int32_t ry, int32_t rw, int32_t rh) {
	int32_t hy = ry + (int32_t)(cp_state.h / 360.0f * (float)(rh - 1));
	for(int32_t dy = -1; dy <= 1; ++dy) {
		int32_t py = hy + dy;
		if(py >= 0 && py < bh) {
			uint32_t lc = (dy == 0) ? 0xffffffff : 0xff000000;
			for(int32_t x = 0; x < rw; ++x) {
				int32_t px = rx + x;
				if(px >= 0 && px < bw) { pixels[py * bw + px] = lc; }
			}
		}
	}
}

// [=]===^=[ cp_render_wheel_base ]=================================[=]
static void cp_render_wheel_base(uint32_t *buf, int32_t rw, int32_t rh, uint32_t bg_color) {
	uint32_t hue_lut[3600];
	for(uint32_t i = 0; i < 3600; ++i) {
		hue_lut[i] = cp_hsv_to_rgb((float)i * 0.1f, 1.0f, 1.0f);
	}
	int32_t sz = rw < rh ? rw : rh;
	int32_t hcx = rw / 2;
	int32_t hcy = rh / 2;
	int32_t iouter = sz / 2;
	int32_t iinner = iouter * 3 / 4;
	int32_t isq_half = iinner * 6 / 10;
	int32_t outer_sq = iouter * iouter;
	int32_t inner_sq = iinner * iinner;
	for(int32_t i = 0; i < rw * rh; ++i) {
		buf[i] = bg_color;
	}
	for(int32_t y = 0; y < rh; ++y) {
		int32_t dy = y - hcy;
		uint32_t *row = buf + y * rw;
		for(int32_t x = 0; x < rw; ++x) {
			int32_t dx = x - hcx;
			int32_t d2 = dx * dx + dy * dy;
			if(d2 >= inner_sq && d2 <= outer_sq) {
				int32_t ang = iatan2_deg10(-dy, dx);
				row[x] = hue_lut[ang];
			} else if(dx >= -isq_half && dx <= isq_half && dy >= -isq_half && dy <= isq_half) {
				float s = (float)(dx + isq_half) / (float)(2 * isq_half);
				float v = 1.0f - (float)(dy + isq_half) / (float)(2 * isq_half);
				row[x] = cp_hsv_to_rgb(cp_state.h, s, v);
			}
		}
	}
}

// [=]===^=[ cp_render_wheel_cursor ]===============================[=]
static void cp_render_wheel_cursor(uint32_t *pixels, int32_t bw, int32_t bh, int32_t rx, int32_t ry, int32_t rw, int32_t rh) {
	int32_t sz = rw < rh ? rw : rh;
	int32_t icx = rx + rw / 2;
	int32_t icy = ry + rh / 2;
	int32_t iouter = sz / 2;
	int32_t iinner = iouter * 3 / 4;
	int32_t isq_half = iinner * 6 / 10;
	float sel_angle = cp_state.h * 3.14159265f / 180.0f;
	float ring_mid = ((float)iinner + (float)iouter) * 0.5f;
	int32_t hx = (int32_t)((float)icx + cosf(sel_angle) * ring_mid);
	int32_t hy = (int32_t)((float)icy + sinf(sel_angle) * ring_mid);
	for(int32_t d = -3; d <= 3; ++d) {
		if(hy >= 0 && hy < bh && hx + d >= 0 && hx + d < bw) { pixels[hy * bw + hx + d] = 0xffffffff; }
		if(hx >= 0 && hx < bw && hy + d >= 0 && hy + d < bh) { pixels[(hy + d) * bw + hx] = 0xffffffff; }
	}
	int32_t sv_cx = icx + (int32_t)((cp_state.s * 2.0f - 1.0f) * (float)isq_half);
	int32_t sv_cy = icy + (int32_t)((1.0f - cp_state.v * 2.0f) * (float)(isq_half - 1));
	uint32_t mc = (cp_state.v > 0.5f) ? 0xff000000 : 0xffffffff;
	for(int32_t d = -3; d <= 3; ++d) {
		if(sv_cy >= 0 && sv_cy < bh && sv_cx + d >= 0 && sv_cx + d < bw) { pixels[sv_cy * bw + sv_cx + d] = mc; }
		if(sv_cx >= 0 && sv_cx < bw && sv_cy + d >= 0 && sv_cy + d < bh) { pixels[(sv_cy + d) * bw + sv_cx] = mc; }
	}
}

// [=]===^=[ cp_render_rgb_bars ]===================================[=]
static void cp_render_rgb_bars(uint32_t *pixels, int32_t bw, int32_t bh, int32_t rx, int32_t ry, int32_t rw, int32_t rh) {
	uint32_t rgb = cp_current_rgb();
	uint32_t vals[3] = { (rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff };
	int32_t bar_h = 24, gap = 8;
	int32_t total = bar_h * 3 + gap * 2;
	int32_t y0 = ry + (rh - total) / 2;
	for(uint32_t ci = 0; ci < 3; ++ci) {
		int32_t by = y0 + (int32_t)ci * (bar_h + gap);
		for(int32_t y = 0; y < bar_h; ++y) {
			int32_t py = by + y;
			if(py < 0 || py >= bh) { continue; }
			for(int32_t x = 0; x < rw; ++x) {
				int32_t px = rx + x;
				if(px < 0 || px >= bw) { continue; }
				uint32_t v = (uint32_t)((float)x / (float)(rw - 1) * 255.0f);
				uint32_t c;
				if(ci == 0)      { c = 0xff000000 | (v << 16); }
				else if(ci == 1) { c = 0xff000000 | (v << 8); }
				else             { c = 0xff000000 | v; }
				pixels[py * bw + px] = c;
			}
		}
		int32_t mx = rx + (int32_t)((float)vals[ci] / 255.0f * (float)(rw - 1));
		for(int32_t y = 0; y < bar_h; ++y) {
			int32_t py = by + y;
			if(py >= 0 && py < bh && mx >= 0 && mx < bw) { pixels[py * bw + mx] = 0xffffffff; }
		}
	}
}

// [=]===^=[ cp_render_cb ]=========================================[=]
static void cp_render_cb(struct mkgui_ctx *ctx, void *userdata) {
	(void)userdata;
	struct mkgui_tabs_data *td = find_tabs_data(ctx, CP_TABS);
	uint32_t active = td ? td->active_tab : CP_TAB_SV;

	if(active == CP_TAB_SV) {
		int32_t si = find_widget_idx(ctx, CP_SV_CANVAS);
		int32_t hi = find_widget_idx(ctx, CP_HUE_CANVAS);
		if(si >= 0) {
			struct mkgui_rect *r = &ctx->rects[si];
			int32_t prev_sw = cp_state.sv_w;
			int32_t prev_sh = cp_state.sv_h;
			cp_state.sv_cache = cp_cache_realloc(cp_state.sv_cache, &cp_state.sv_w, &cp_state.sv_h, r->w, r->h);
			if(cp_state.sv_cache && (prev_sw != r->w || prev_sh != r->h || cp_state.sv_hue != cp_state.h)) {
				cp_render_sv_base(cp_state.sv_cache, r->w, r->h);
				cp_state.sv_hue = cp_state.h;
			}
			if(cp_state.sv_cache) {
				cp_cache_blit(ctx->pixels, ctx->win_w, ctx->win_h, r->x, r->y, cp_state.sv_cache, r->w, r->h);
			}
			cp_render_sv_cursor(ctx->pixels, ctx->win_w, ctx->win_h, r->x, r->y, r->w, r->h);
		}
		if(hi >= 0) {
			struct mkgui_rect *r = &ctx->rects[hi];
			int32_t prev_hw = cp_state.hue_w;
			int32_t prev_hh = cp_state.hue_h;
			cp_state.hue_cache = cp_cache_realloc(cp_state.hue_cache, &cp_state.hue_w, &cp_state.hue_h, r->w, r->h);
			if(cp_state.hue_cache && (prev_hw != r->w || prev_hh != r->h)) {
				cp_render_hue_base(cp_state.hue_cache, r->w, r->h);
			}
			if(cp_state.hue_cache) {
				cp_cache_blit(ctx->pixels, ctx->win_w, ctx->win_h, r->x, r->y, cp_state.hue_cache, r->w, r->h);
			}
			cp_render_hue_cursor(ctx->pixels, ctx->win_w, ctx->win_h, r->x, r->y, r->w, r->h);
		}
	} else if(active == CP_TAB_WHEEL) {
		int32_t wi = find_widget_idx(ctx, CP_WH_CANVAS);
		if(wi >= 0) {
			struct mkgui_rect *r = &ctx->rects[wi];
			int32_t ww = r->w - 2;
			int32_t wh = r->h - 2;
			int32_t prev_ww = cp_state.wheel_w;
			int32_t prev_wh = cp_state.wheel_h;
			cp_state.wheel_cache = cp_cache_realloc(cp_state.wheel_cache, &cp_state.wheel_w, &cp_state.wheel_h, ww, wh);
			if(cp_state.wheel_cache && (prev_ww != ww || prev_wh != wh || cp_state.wheel_hue != cp_state.h)) {
				cp_render_wheel_base(cp_state.wheel_cache, ww, wh, ctx->theme.bg);
				cp_state.wheel_hue = cp_state.h;
			}
			if(cp_state.wheel_cache) {
				cp_cache_blit(ctx->pixels, ctx->win_w, ctx->win_h, r->x + 1, r->y + 1, cp_state.wheel_cache, ww, wh);
			}
			cp_render_wheel_cursor(ctx->pixels, ctx->win_w, ctx->win_h, r->x + 1, r->y + 1, ww, wh);
		}
	} else if(active == CP_TAB_RGB) {
		int32_t ri = find_widget_idx(ctx, CP_RGB_CANVAS);
		if(ri >= 0) {
			struct mkgui_rect *r = &ctx->rects[ri];
			cp_render_rgb_bars(ctx->pixels, ctx->win_w, ctx->win_h, r->x + 1, r->y + 1, r->w - 2, r->h - 2);
		}
	}

	int32_t pi = find_widget_idx(ctx, CP_PREVIEW);
	if(pi >= 0) {
		struct mkgui_rect *r = &ctx->rects[pi];
		uint32_t c = cp_current_rgb();
		for(int32_t y = r->y + 1; y < r->y + r->h - 1 && y < ctx->win_h; ++y) {
			for(int32_t x = r->x + 1; x < r->x + r->w - 1 && x < ctx->win_w; ++x) {
				if(x >= 0 && y >= 0) { ctx->pixels[y * ctx->win_w + x] = c; }
			}
		}
	}
}

// [=]===^=[ cp_handle_mouse ]======================================[=]
static void cp_handle_mouse(struct mkgui_ctx *ctx) {
	int32_t mx = ctx->mouse_x;
	int32_t my = ctx->mouse_y;
	struct mkgui_tabs_data *td = find_tabs_data(ctx, CP_TABS);
	uint32_t active = td ? td->active_tab : CP_TAB_SV;

	if(cp_state.drag_target == CP_SV_CANVAS && active == CP_TAB_SV) {
		int32_t idx = find_widget_idx(ctx, CP_SV_CANVAS);
		if(idx >= 0) {
			struct mkgui_rect *r = &ctx->rects[idx];
			float s = (float)(mx - r->x) / (float)(r->w - 1);
			float v = 1.0f - (float)(my - r->y) / (float)(r->h - 1);
			if(s < 0.0f) { s = 0.0f; } if(s > 1.0f) { s = 1.0f; }
			if(v < 0.0f) { v = 0.0f; } if(v > 1.0f) { v = 1.0f; }
			cp_state.s = s;
			cp_state.v = v;
			cp_sync_controls(ctx);
			dirty_all(ctx);
		}
	} else if(cp_state.drag_target == CP_HUE_CANVAS && active == CP_TAB_SV) {
		int32_t idx = find_widget_idx(ctx, CP_HUE_CANVAS);
		if(idx >= 0) {
			struct mkgui_rect *r = &ctx->rects[idx];
			float h = (float)(my - r->y) / (float)(r->h - 1) * 360.0f;
			if(h < 0.0f) { h = 0.0f; } if(h > 359.99f) { h = 359.99f; }
			cp_state.h = h;
			cp_sync_controls(ctx);
			dirty_all(ctx);
		}
	} else if(cp_state.drag_target == CP_WH_CANVAS && active == CP_TAB_WHEEL) {
		int32_t idx = find_widget_idx(ctx, CP_WH_CANVAS);
		if(idx >= 0) {
			struct mkgui_rect *r = &ctx->rects[idx];
			int32_t sz = r->w < r->h ? r->w : r->h;
			float cx = (float)r->x + (float)r->w * 0.5f;
			float cy = (float)r->y + (float)r->h * 0.5f;
			float outer = (float)sz * 0.5f;
			float inner = outer * 0.75f;
			float sq_half = inner * 0.60f;
			float dx = (float)mx - cx;
			float dy = (float)my - cy;
			float dist = sqrtf(dx * dx + dy * dy);
			if(dist >= inner * 0.7f) {
				float angle = atan2f(dy, dx) * 180.0f / 3.14159265f;
				if(angle < 0.0f) { angle += 360.0f; }
				cp_state.h = angle;
			} else {
				float s = (dx + sq_half) / (2.0f * sq_half);
				float v = 1.0f - (dy + sq_half) / (2.0f * sq_half);
				if(s < 0.0f) { s = 0.0f; } if(s > 1.0f) { s = 1.0f; }
				if(v < 0.0f) { v = 0.0f; } if(v > 1.0f) { v = 1.0f; }
				cp_state.s = s;
				cp_state.v = v;
			}
			cp_sync_controls(ctx);
			dirty_all(ctx);
		}
	} else if(cp_state.drag_target == CP_RGB_CANVAS && active == CP_TAB_RGB) {
		int32_t idx = find_widget_idx(ctx, CP_RGB_CANVAS);
		if(idx >= 0) {
			struct mkgui_rect *r = &ctx->rects[idx];
			int32_t bar_h = 24, gap = 8;
			int32_t total = bar_h * 3 + gap * 2;
			int32_t y0 = r->y + (r->h - total) / 2;
			float frac = (float)(mx - r->x - 1) / (float)(r->w - 3);
			if(frac < 0.0f) { frac = 0.0f; } if(frac > 1.0f) { frac = 1.0f; }
			uint32_t val = (uint32_t)(frac * 255.0f + 0.5f);
			uint32_t rgb = cp_current_rgb();
			uint32_t cr = (rgb >> 16) & 0xff, cg = (rgb >> 8) & 0xff, cb = rgb & 0xff;
			if(my < y0 + bar_h + gap / 2) { cr = val; }
			else if(my < y0 + bar_h * 2 + gap + gap / 2) { cg = val; }
			else { cb = val; }
			cp_rgb_to_hsv((cr << 16) | (cg << 8) | cb, &cp_state.h, &cp_state.s, &cp_state.v);
			cp_sync_controls(ctx);
			dirty_all(ctx);
		}
	}
}

// [=]===^=[ cp_begin_drag ]========================================[=]
static void cp_begin_drag(struct mkgui_ctx *ctx) {
	uint32_t pid = ctx->press_id;
	if(pid == CP_SV_CANVAS || pid == CP_HUE_CANVAS || pid == CP_WH_CANVAS || pid == CP_RGB_CANVAS) {
		cp_state.drag_target = pid;
		cp_handle_mouse(ctx);
	}
}

// [=]===^=[ mkgui_color_dialog ]====================================[=]
MKGUI_API uint32_t mkgui_color_dialog(struct mkgui_ctx *ctx, uint32_t initial_color, uint32_t *out_color) {
	MKGUI_CHECK_VAL(ctx, 0);

	cp_rgb_to_hsv(initial_color, &cp_state.h, &cp_state.s, &cp_state.v);
	cp_state.drag_target = 0;

	int32_t dw = 420;
	int32_t dh = 440;

	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,  CP_WINDOW,     "Color Picker",  "", 0,            dw, dh, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,    CP_VBOX,       "",              "", CP_WINDOW,    0, 0, 0, 0, 0),

		MKGUI_W(MKGUI_TABS,    CP_TABS,       "",              "", CP_VBOX,      0, 0, 0, 0, 1),
		MKGUI_W(MKGUI_TAB,     CP_TAB_SV,     "Square",        "", CP_TABS,      0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_TAB,     CP_TAB_WHEEL,  "Wheel",         "", CP_TABS,      0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_TAB,     CP_TAB_RGB,    "Sliders",       "", CP_TABS,      0, 0, 0, 0, 0),

		MKGUI_W(MKGUI_HBOX,    CP_SV_HBOX,    "",              "", CP_TAB_SV,    0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_CANVAS,  CP_SV_CANVAS,  "",              "", CP_SV_HBOX,   0, 0, 0, MKGUI_CANVAS_BORDER, 1),
		MKGUI_W(MKGUI_CANVAS,  CP_HUE_CANVAS, "",              "", CP_SV_HBOX,   28, 0, MKGUI_FIXED, MKGUI_CANVAS_BORDER, 0),

		MKGUI_W(MKGUI_CANVAS,  CP_WH_CANVAS,  "",              "", CP_TAB_WHEEL,  0, 0, 0, MKGUI_CANVAS_BORDER, 0),
		MKGUI_W(MKGUI_CANVAS,  CP_RGB_CANVAS, "",              "", CP_TAB_RGB,    0, 0, 0, MKGUI_CANVAS_BORDER, 0),

		MKGUI_W(MKGUI_DIVIDER, CP_DIVIDER,    "",              "", CP_VBOX,      0, 0, MKGUI_FIXED, 0, 0),

		MKGUI_W(MKGUI_HBOX,    CP_CTL_HBOX,   "",              "", CP_VBOX,      0, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_CANVAS,  CP_PREVIEW,    "",              "", CP_CTL_HBOX,  28, 0, MKGUI_FIXED, MKGUI_CANVAS_BORDER, 0),
		MKGUI_W(MKGUI_LABEL,   CP_HEX_LBL,    "#",             "", CP_CTL_HBOX,  16, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_INPUT,   CP_HEX_INPUT,  "",              "", CP_CTL_HBOX,  68, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_LABEL,   CP_R_LBL,      "R:",            "", CP_CTL_HBOX,  20, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_SPINBOX, CP_R_SPN,      "",              "", CP_CTL_HBOX,  56, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_LABEL,   CP_G_LBL,      "G:",            "", CP_CTL_HBOX,  20, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_SPINBOX, CP_G_SPN,      "",              "", CP_CTL_HBOX,  56, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_LABEL,   CP_B_LBL,      "B:",            "", CP_CTL_HBOX,  20, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_SPINBOX, CP_B_SPN,      "",              "", CP_CTL_HBOX,  56, 0, MKGUI_FIXED, 0, 0),

		MKGUI_W(MKGUI_HBOX,    CP_BTN_HBOX,   "",              "", CP_VBOX,      0, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_SPACER,  CP_BTN_SPACER, "",              "", CP_BTN_HBOX,  0, 0, 0, 0, 1),
		MKGUI_W(MKGUI_BUTTON,  CP_BTN_OK,     "OK",            "", CP_BTN_HBOX,  80, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_BUTTON,  CP_BTN_CANCEL, "Cancel",        "", CP_BTN_HBOX,  80, 0, MKGUI_FIXED, 0, 0),
	};
	uint32_t wcount = sizeof(widgets) / sizeof(widgets[0]);

	struct mkgui_ctx *dlg = mkgui_create_child(ctx, widgets, wcount, "Color Picker", dw, dh);
	if(!dlg) {
		return 0;
	}
	mkgui_set_window_instance(dlg, "colorpicker");
	dlg->render_cb = cp_render_cb;

	mkgui_spinbox_setup(dlg, CP_R_SPN, 0, 255, 0, 1);
	mkgui_spinbox_setup(dlg, CP_G_SPN, 0, 255, 0, 1);
	mkgui_spinbox_setup(dlg, CP_B_SPN, 0, 255, 0, 1);
	cp_sync_controls(dlg);

	uint32_t result = 0;
	struct mkgui_event ev;
	uint32_t running = 1;
	uint32_t was_press = 0;

	while(running) {
		while(mkgui_poll(dlg, &ev)) {
			switch(ev.type) {
				case MKGUI_EVENT_CLOSE: {
					running = 0;
				} break;

				case MKGUI_EVENT_CLICK: {
					if(ev.id == CP_BTN_OK) {
						result = 1;
						running = 0;
					} else if(ev.id == CP_BTN_CANCEL) {
						running = 0;
					}
				} break;

				case MKGUI_EVENT_SPINBOX_CHANGED: {
					if(ev.id == CP_R_SPN || ev.id == CP_G_SPN || ev.id == CP_B_SPN) {
						uint32_t r = (uint32_t)mkgui_spinbox_get(dlg, CP_R_SPN);
						uint32_t g = (uint32_t)mkgui_spinbox_get(dlg, CP_G_SPN);
						uint32_t b = (uint32_t)mkgui_spinbox_get(dlg, CP_B_SPN);
						cp_rgb_to_hsv((r << 16) | (g << 8) | b, &cp_state.h, &cp_state.s, &cp_state.v);
						cp_sync_controls(dlg);
					}
				} break;

				case MKGUI_EVENT_INPUT_SUBMIT: {
					if(ev.id == CP_HEX_INPUT) {
						char *hex = mkgui_input_get(dlg, CP_HEX_INPUT);
						uint32_t hv = 0;
						if(hex && sscanf(hex, "%x", &hv) == 1) {
							cp_rgb_to_hsv(hv & 0xffffff, &cp_state.h, &cp_state.s, &cp_state.v);
							cp_sync_controls(dlg);
						}
					}
				} break;

				default: break;
			}
		}

		if(dlg->press_id != 0 && !was_press) {
			cp_begin_drag(dlg);
		}
		if(dlg->press_id != 0 && cp_state.drag_target) {
			cp_handle_mouse(dlg);
		}
		if(dlg->press_id == 0) {
			cp_state.drag_target = 0;
		}
		was_press = (dlg->press_id != 0);
		mkgui_wait(dlg);
	}

	if(result && out_color) {
		*out_color = cp_current_rgb() & 0x00ffffff;
	}

	mkgui_destroy_child(dlg);
	return result;
}
