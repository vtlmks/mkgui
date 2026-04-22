// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ find_image_data ]====================================[=]
static struct mkgui_image_data *find_image_data(struct mkgui_ctx *ctx, uint32_t id) {
	for(uint32_t i = 0; i < ctx->image_count; ++i) {
		if(ctx->images[i].id == id) {
			return &ctx->images[i];
		}
	}
	return NULL;
}

// [=]===^=[ mkgui_image_set ]====================================[=]
MKGUI_API void mkgui_image_set(struct mkgui_ctx *ctx, uint32_t id, const uint32_t *pixels, int32_t w, int32_t h) {
	MKGUI_CHECK(ctx);
	if(!pixels) {
		return;
	}
	struct mkgui_image_data *img = find_image_data(ctx, id);
	if(!img) {
		MKGUI_AUX_GROW(ctx->images, ctx->image_count, ctx->image_cap, struct mkgui_image_data);
		if(ctx->image_count >= ctx->image_cap) {
			return;
		}
		img = &ctx->images[ctx->image_count++];
		img->id = id;
		img->pixels = NULL;
	}

	free(img->pixels);
	img->pixels = NULL;
	img->img_w = 0;
	img->img_h = 0;

	if(pixels && w > 0 && h > 0) {
		img->pixels = (uint32_t *)malloc((size_t)w * (size_t)h * sizeof(uint32_t));
		if(img->pixels) {
			memcpy(img->pixels, pixels, (size_t)w * (size_t)h * sizeof(uint32_t));
			img->img_w = w;
			img->img_h = h;
		}
	}
	dirty_all(ctx);
}

// [=]===^=[ mkgui_image_clear ]==================================[=]
MKGUI_API void mkgui_image_clear(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK(ctx);
	struct mkgui_image_data *img = find_image_data(ctx, id);
	if(!img) {
		return;
	}
	free(img->pixels);
	img->pixels = NULL;
	img->img_w = 0;
	img->img_h = 0;
	dirty_all(ctx);
}

// [=]===^=[ render_image ]=======================================[=]
static void render_image(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	struct mkgui_image_data *img = find_image_data(ctx, w->id);

	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	if(w->style & MKGUI_IMAGE_BORDER) {
		draw_rounded_rect(ctx->pixels, ctx->win_w, ctx->win_h, rx, ry, rw, rh, ctx->theme.bg, ctx->theme.widget_border, ctx->theme.corner_radius);
		rx += 1;
		ry += 1;
		rw -= 2;
		rh -= 2;
	}

	if(!img || !img->pixels || img->img_w <= 0 || img->img_h <= 0) {
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx, ry, rw, rh, shade_color(ctx->theme.bg, -10));
		return;
	}

	int32_t iw = img->img_w;
	int32_t ih = img->img_h;

	int32_t dx, dy, dw, dh;
	if(w->style & MKGUI_IMAGE_STRETCH) {
		dx = rx;
		dy = ry;
		dw = rw;
		dh = rh;
	} else {
		float scale_x = (float)rw / (float)iw;
		float scale_y = (float)rh / (float)ih;
		float scale = scale_x < scale_y ? scale_x : scale_y;
		if(scale > 1.0f && !(w->style & MKGUI_IMAGE_STRETCH)) {
			scale = 1.0f;
		}
		dw = (int32_t)(iw * scale);
		dh = (int32_t)(ih * scale);
		dx = rx + (rw - dw) / 2;
		dy = ry + (rh - dh) / 2;
	}

	draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx, ry, rw, rh, shade_color(ctx->theme.bg, -10));

	int32_t cx1 = rx > 0 ? rx : 0;
	int32_t cy1 = ry > 0 ? ry : 0;
	int32_t cx2 = rx + rw < ctx->win_w ? rx + rw : ctx->win_w;
	int32_t cy2 = ry + rh < ctx->win_h ? ry + rh : ctx->win_h;

	for(int32_t py = cy1; py < cy2; ++py) {
		int32_t sy = (int32_t)((int64_t)(py - dy) * ih / dh);
		if(sy < 0 || sy >= ih) {
			continue;
		}
		uint32_t *src_row = &img->pixels[sy * iw];
		uint32_t *dst_row = &ctx->pixels[py * ctx->win_w];
		for(int32_t px = cx1; px < cx2; ++px) {
			int32_t sx = (int32_t)((int64_t)(px - dx) * iw / dw);
			if(sx < 0 || sx >= iw) {
				continue;
			}
			uint32_t src = src_row[sx];
			uint8_t a = (uint8_t)(src >> 24);
			if(a == 255) {
				dst_row[px] = src;
			} else if(a > 0) {
				dst_row[px] = blend_pixel(dst_row[px], src, a);
			}
		}
	}
}
