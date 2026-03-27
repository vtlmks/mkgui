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
} cp_state;

// [=]===^=[ cp_hsv_to_rgb ]========================================[=]
static uint32_t cp_hsv_to_rgb(float h, float s, float v) {
	float c = v * s;
	float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
	float m = v - c;
	float r, g, b;
	if(h < 60.0f)       { r = c; g = x; b = 0; }
	else if(h < 120.0f) { r = x; g = c; b = 0; }
	else if(h < 180.0f) { r = 0; g = c; b = x; }
	else if(h < 240.0f) { r = 0; g = x; b = c; }
	else if(h < 300.0f) { r = x; g = 0; b = c; }
	else                { r = c; g = 0; b = x; }
	uint32_t ri = (uint32_t)((r + m) * 255.0f + 0.5f);
	uint32_t gi = (uint32_t)((g + m) * 255.0f + 0.5f);
	uint32_t bi = (uint32_t)((b + m) * 255.0f + 0.5f);
	return 0xff000000 | (ri << 16) | (gi << 8) | bi;
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

// [=]===^=[ cp_render_sv_square ]==================================[=]
static void cp_render_sv_square(uint32_t *pixels, int32_t bw, int32_t bh, int32_t rx, int32_t ry, int32_t rw, int32_t rh) {
	for(int32_t y = 0; y < rh; ++y) {
		int32_t py = ry + y;
		if(py < 0 || py >= bh) { continue; }
		float v = 1.0f - (float)y / (float)(rh - 1);
		for(int32_t x = 0; x < rw; ++x) {
			int32_t px = rx + x;
			if(px < 0 || px >= bw) { continue; }
			float s = (float)x / (float)(rw - 1);
			pixels[py * bw + px] = cp_hsv_to_rgb(cp_state.h, s, v);
		}
	}
	int32_t cx = rx + (int32_t)(cp_state.s * (float)(rw - 1));
	int32_t cy = ry + (int32_t)((1.0f - cp_state.v) * (float)(rh - 1));
	uint32_t mc = (cp_state.v > 0.5f) ? 0xff000000 : 0xffffffff;
	for(int32_t d = -4; d <= 4; ++d) {
		if(cy >= 0 && cy < bh && cx + d >= 0 && cx + d < bw) { pixels[cy * bw + cx + d] = mc; }
		if(cx >= 0 && cx < bw && cy + d >= 0 && cy + d < bh) { pixels[(cy + d) * bw + cx] = mc; }
	}
}

// [=]===^=[ cp_render_hue_bar ]====================================[=]
static void cp_render_hue_bar(uint32_t *pixels, int32_t bw, int32_t bh, int32_t rx, int32_t ry, int32_t rw, int32_t rh) {
	for(int32_t y = 0; y < rh; ++y) {
		int32_t py = ry + y;
		if(py < 0 || py >= bh) { continue; }
		float hue = (float)y / (float)(rh - 1) * 360.0f;
		uint32_t c = cp_hsv_to_rgb(hue, 1.0f, 1.0f);
		for(int32_t x = 0; x < rw; ++x) {
			int32_t px = rx + x;
			if(px >= 0 && px < bw) { pixels[py * bw + px] = c; }
		}
	}
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

// [=]===^=[ cp_render_wheel ]======================================[=]
static void cp_render_wheel(uint32_t *pixels, int32_t bw, int32_t bh, int32_t rx, int32_t ry, int32_t rw, int32_t rh) {
	int32_t sz = rw < rh ? rw : rh;
	float cx = (float)rx + (float)rw * 0.5f;
	float cy = (float)ry + (float)rh * 0.5f;
	float outer = (float)sz * 0.5f;
	float inner = outer * 0.75f;
	float sq_half = inner * 0.60f;
	for(int32_t y = 0; y < rh; ++y) {
		int32_t py = ry + y;
		if(py < 0 || py >= bh) { continue; }
		for(int32_t x = 0; x < rw; ++x) {
			int32_t px = rx + x;
			if(px < 0 || px >= bw) { continue; }
			float dx = (float)px - cx + 0.5f;
			float dy = (float)py - cy + 0.5f;
			float dist = sqrtf(dx * dx + dy * dy);
			if(dist >= inner && dist <= outer) {
				float angle = atan2f(dy, dx) * 180.0f / 3.14159265f;
				if(angle < 0.0f) { angle += 360.0f; }
				pixels[py * bw + px] = cp_hsv_to_rgb(angle, 1.0f, 1.0f);
			} else if(fabsf(dx) <= sq_half && fabsf(dy) <= sq_half) {
				float s = (dx + sq_half) / (2.0f * sq_half);
				float v = 1.0f - (dy + sq_half) / (2.0f * sq_half);
				pixels[py * bw + px] = cp_hsv_to_rgb(cp_state.h, s, v);
			}
		}
	}
	float sel_angle = cp_state.h * 3.14159265f / 180.0f;
	float ring_mid = (inner + outer) * 0.5f;
	int32_t hx = (int32_t)(cx + cosf(sel_angle) * ring_mid);
	int32_t hy = (int32_t)(cy + sinf(sel_angle) * ring_mid);
	for(int32_t d = -3; d <= 3; ++d) {
		if(hy >= 0 && hy < bh && hx + d >= 0 && hx + d < bw) { pixels[hy * bw + hx + d] = 0xffffffff; }
		if(hx >= 0 && hx < bw && hy + d >= 0 && hy + d < bh) { pixels[(hy + d) * bw + hx] = 0xffffffff; }
	}
	int32_t sv_cx = (int32_t)(cx + (cp_state.s * 2.0f - 1.0f) * sq_half);
	int32_t sv_cy = (int32_t)(cy + (1.0f - cp_state.v * 2.0f) * (sq_half - 1.0f));
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
			cp_render_sv_square(ctx->pixels, ctx->win_w, ctx->win_h, r->x, r->y, r->w, r->h);
		}
		if(hi >= 0) {
			struct mkgui_rect *r = &ctx->rects[hi];
			cp_render_hue_bar(ctx->pixels, ctx->win_w, ctx->win_h, r->x, r->y, r->w, r->h);
		}
	} else if(active == CP_TAB_WHEEL) {
		int32_t wi = find_widget_idx(ctx, CP_WH_CANVAS);
		if(wi >= 0) {
			struct mkgui_rect *r = &ctx->rects[wi];
			cp_render_wheel(ctx->pixels, ctx->win_w, ctx->win_h, r->x + 1, r->y + 1, r->w - 2, r->h - 2);
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
		{ MKGUI_WINDOW,  CP_WINDOW,     "Color Picker",  "", 0,            dw, dh, 0, 0, 0 },
		{ MKGUI_VBOX,    CP_VBOX,       "",              "", CP_WINDOW,    0, 0, 0, 0, 0 },

		{ MKGUI_TABS,    CP_TABS,       "",              "", CP_VBOX,      0, 0, 0, 0, 1 },
		{ MKGUI_TAB,     CP_TAB_SV,     "Square",        "", CP_TABS,      0, 0, 0, 0, 0 },
		{ MKGUI_TAB,     CP_TAB_WHEEL,  "Wheel",         "", CP_TABS,      0, 0, 0, 0, 0 },
		{ MKGUI_TAB,     CP_TAB_RGB,    "Sliders",       "", CP_TABS,      0, 0, 0, 0, 0 },

		{ MKGUI_HBOX,    CP_SV_HBOX,    "",              "", CP_TAB_SV,    0, 0, 0, 0, 0 },
		{ MKGUI_CANVAS,  CP_SV_CANVAS,  "",              "", CP_SV_HBOX,   0, 0, 0, MKGUI_PANEL_BORDER, 1 },
		{ MKGUI_CANVAS,  CP_HUE_CANVAS, "",              "", CP_SV_HBOX,   28, 0, MKGUI_FIXED, MKGUI_PANEL_BORDER, 0 },

		{ MKGUI_CANVAS,  CP_WH_CANVAS,  "",              "", CP_TAB_WHEEL,  0, 0, 0, MKGUI_PANEL_BORDER, 0 },
		{ MKGUI_CANVAS,  CP_RGB_CANVAS, "",              "", CP_TAB_RGB,    0, 0, 0, MKGUI_PANEL_BORDER, 0 },

		{ MKGUI_DIVIDER, CP_DIVIDER,    "",              "", CP_VBOX,      0, 0, MKGUI_FIXED, 0, 0 },

		{ MKGUI_HBOX,    CP_CTL_HBOX,   "",              "", CP_VBOX,      0, 0, MKGUI_FIXED, 0, 0 },
		{ MKGUI_CANVAS,  CP_PREVIEW,    "",              "", CP_CTL_HBOX,  28, 0, MKGUI_FIXED, MKGUI_PANEL_BORDER, 0 },
		{ MKGUI_LABEL,   CP_HEX_LBL,    "#",             "", CP_CTL_HBOX,  16, 0, MKGUI_FIXED, 0, 0 },
		{ MKGUI_INPUT,   CP_HEX_INPUT,  "",              "", CP_CTL_HBOX,  68, 0, MKGUI_FIXED, 0, 0 },
		{ MKGUI_LABEL,   CP_R_LBL,      "R:",            "", CP_CTL_HBOX,  20, 0, MKGUI_FIXED, 0, 0 },
		{ MKGUI_SPINBOX, CP_R_SPN,      "",              "", CP_CTL_HBOX,  56, 0, MKGUI_FIXED, 0, 0 },
		{ MKGUI_LABEL,   CP_G_LBL,      "G:",            "", CP_CTL_HBOX,  20, 0, MKGUI_FIXED, 0, 0 },
		{ MKGUI_SPINBOX, CP_G_SPN,      "",              "", CP_CTL_HBOX,  56, 0, MKGUI_FIXED, 0, 0 },
		{ MKGUI_LABEL,   CP_B_LBL,      "B:",            "", CP_CTL_HBOX,  20, 0, MKGUI_FIXED, 0, 0 },
		{ MKGUI_SPINBOX, CP_B_SPN,      "",              "", CP_CTL_HBOX,  56, 0, MKGUI_FIXED, 0, 0 },

		{ MKGUI_HBOX,    CP_BTN_HBOX,   "",              "", CP_VBOX,      0, 0, MKGUI_FIXED, 0, 0 },
		{ MKGUI_SPACER,  CP_BTN_SPACER, "",              "", CP_BTN_HBOX,  0, 0, 0, 0, 1 },
		{ MKGUI_BUTTON,  CP_BTN_OK,     "OK",            "", CP_BTN_HBOX,  80, 0, MKGUI_FIXED, 0, 0 },
		{ MKGUI_BUTTON,  CP_BTN_CANCEL, "Cancel",        "", CP_BTN_HBOX,  80, 0, MKGUI_FIXED, 0, 0 },
	};
	uint32_t wcount = sizeof(widgets) / sizeof(widgets[0]);

	struct mkgui_ctx *dlg = mkgui_create_child(ctx, widgets, wcount, "Color Picker", dw, dh);
	if(!dlg) {
		return 0;
	}
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
						const char *hex = mkgui_input_get(dlg, CP_HEX_INPUT);
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
