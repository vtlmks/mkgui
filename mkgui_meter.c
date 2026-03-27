// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ meter_zone_color ]=====================================[=]
static uint32_t meter_zone_color(struct mkgui_meter_data *md, int32_t pos) {
	if(pos < md->zone_t1) {
		return md->zone_c1;
	} else if(pos < md->zone_t2) {
		return md->zone_c2;
	}
	return md->zone_c3;
}

// [=]===^=[ meter_dim_color ]======================================[=]
static uint32_t meter_dim_color(uint32_t color) {
	uint32_t a = (color >> 24) & 0xff;
	uint32_t r = ((color >> 16) & 0xff) / 4;
	uint32_t g = ((color >> 8) & 0xff) / 4;
	uint32_t b = (color & 0xff) / 4;
	return (a << 24) | (r << 16) | (g << 8) | b;
}

// [=]===^=[ render_meter ]=========================================[=]
static void render_meter(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;
	uint32_t vertical = w->flags & MKGUI_VERTICAL;

	draw_patch(ctx, MKGUI_STYLE_SUNKEN, rx, ry, rw, rh, ctx->theme.input_bg, ctx->theme.widget_border);

	struct mkgui_meter_data *md = find_meter_data(ctx, w->id);
	if(!md || md->max_val <= 0) {
		return;
	}

	int32_t ix = rx + 1;
	int32_t iy = ry + 1;
	int32_t iw = rw - 2;
	int32_t ih = rh - 2;

	if(vertical) {
		// zone boundaries in pixels from bottom
		int32_t z1_px = (int32_t)((int64_t)ih * md->zone_t1 / md->max_val);
		int32_t z2_px = (int32_t)((int64_t)ih * md->zone_t2 / md->max_val);
		if(z1_px > ih) {
			z1_px = ih;
		}
		if(z2_px > ih) {
			z2_px = ih;
		}

		// draw zones bottom-to-top (zone 1 at bottom, zone 3 at top)
		int32_t fill_h = (int32_t)((int64_t)ih * md->value / md->max_val);
		if(fill_h < 0) {
			fill_h = 0;
		}
		if(fill_h > ih) {
			fill_h = ih;
		}

		// zone 1: bottom to z1
		if(z1_px > 0) {
			int32_t zy = iy + ih - z1_px;
			int32_t zh = z1_px;
			int32_t lit = fill_h;
			if(lit > z1_px) {
				lit = z1_px;
			}
			int32_t lit_y = iy + ih - lit;
			int32_t dim_y = zy;
			int32_t dim_h = lit_y - zy;
			if(dim_h > 0) {
				draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ix, dim_y, iw, dim_h, meter_dim_color(md->zone_c1));
			}
			int32_t lit_h = zy + zh - lit_y;
			if(lit_h > 0) {
				draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ix, lit_y, iw, lit_h, md->zone_c1);
			}
		}

		// zone 2: z1 to z2
		if(z2_px > z1_px) {
			int32_t zy = iy + ih - z2_px;
			int32_t zh = z2_px - z1_px;
			int32_t lit = fill_h - z1_px;
			if(lit < 0) {
				lit = 0;
			}
			if(lit > zh) {
				lit = zh;
			}
			int32_t lit_y = zy + zh - lit;
			int32_t dim_y = zy;
			int32_t dim_h = lit_y - zy;
			if(dim_h > 0) {
				draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ix, dim_y, iw, dim_h, meter_dim_color(md->zone_c2));
			}
			if(lit > 0) {
				draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ix, lit_y, iw, lit, md->zone_c2);
			}
		}

		// zone 3: z2 to top
		if(ih > z2_px) {
			int32_t zy = iy;
			int32_t zh = ih - z2_px;
			int32_t lit = fill_h - z2_px;
			if(lit < 0) {
				lit = 0;
			}
			if(lit > zh) {
				lit = zh;
			}
			int32_t lit_y = zy + zh - lit;
			int32_t dim_y = zy;
			int32_t dim_h = lit_y - zy;
			if(dim_h > 0) {
				draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ix, dim_y, iw, dim_h, meter_dim_color(md->zone_c3));
			}
			if(lit > 0) {
				draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ix, lit_y, iw, lit, md->zone_c3);
			}
		}

		// thin fill bar (60% width, centered)
		int32_t bar_w = iw * 6 / 10;
		if(bar_w < 3) {
			bar_w = 3;
		}
		int32_t bar_x = ix + (iw - bar_w) / 2;
		if(fill_h > 0) {
			int32_t bar_y = iy + ih - fill_h;
			uint32_t bar_color = meter_zone_color(md, md->value);
			draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, bar_x, bar_y, bar_w, fill_h, bar_color);
		}

		// percentage text
		if(w->style & MKGUI_METER_TEXT) {
			char buf[16];
			int32_t pct = (int32_t)((int64_t)md->value * 100 / md->max_val);
			snprintf(buf, sizeof(buf), "%d%%", pct);
			int32_t tw = text_width(ctx, buf);
			int32_t tx = rx + (rw - tw) / 2;
			int32_t ty = ry + (rh - ctx->font_height) / 2;
			push_text_clip(tx, ty, buf, ctx->theme.text, rx + 1, ry + 1, rx + rw - 1, ry + rh - 1);
		}
	} else {
		// horizontal: zones left-to-right
		int32_t z1_px = (int32_t)((int64_t)iw * md->zone_t1 / md->max_val);
		int32_t z2_px = (int32_t)((int64_t)iw * md->zone_t2 / md->max_val);
		if(z1_px > iw) {
			z1_px = iw;
		}
		if(z2_px > iw) {
			z2_px = iw;
		}

		int32_t fill_w = (int32_t)((int64_t)iw * md->value / md->max_val);
		if(fill_w < 0) {
			fill_w = 0;
		}
		if(fill_w > iw) {
			fill_w = iw;
		}

		// zone 1: 0 to z1
		if(z1_px > 0) {
			int32_t lit = fill_w;
			if(lit > z1_px) {
				lit = z1_px;
			}
			if(lit > 0) {
				draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ix, iy, lit, ih, md->zone_c1);
			}
			int32_t dim = z1_px - lit;
			if(dim > 0) {
				draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ix + lit, iy, dim, ih, meter_dim_color(md->zone_c1));
			}
		}

		// zone 2: z1 to z2
		if(z2_px > z1_px) {
			int32_t zh = z2_px - z1_px;
			int32_t lit = fill_w - z1_px;
			if(lit < 0) {
				lit = 0;
			}
			if(lit > zh) {
				lit = zh;
			}
			if(lit > 0) {
				draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ix + z1_px, iy, lit, ih, md->zone_c2);
			}
			int32_t dim = zh - lit;
			if(dim > 0) {
				draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ix + z1_px + lit, iy, dim, ih, meter_dim_color(md->zone_c2));
			}
		}

		// zone 3: z2 to end
		if(iw > z2_px) {
			int32_t zh = iw - z2_px;
			int32_t lit = fill_w - z2_px;
			if(lit < 0) {
				lit = 0;
			}
			if(lit > zh) {
				lit = zh;
			}
			if(lit > 0) {
				draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ix + z2_px, iy, lit, ih, md->zone_c3);
			}
			int32_t dim = zh - lit;
			if(dim > 0) {
				draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ix + z2_px + lit, iy, dim, ih, meter_dim_color(md->zone_c3));
			}
		}

		// thin fill bar (60% height, centered)
		int32_t bar_h = ih * 6 / 10;
		if(bar_h < 3) {
			bar_h = 3;
		}
		int32_t bar_y = iy + (ih - bar_h) / 2;
		if(fill_w > 0) {
			uint32_t bar_color = meter_zone_color(md, md->value);
			draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, ix, bar_y, fill_w, bar_h, bar_color);
		}

		// percentage text
		if(w->style & MKGUI_METER_TEXT) {
			char buf[16];
			int32_t pct = (int32_t)((int64_t)md->value * 100 / md->max_val);
			snprintf(buf, sizeof(buf), "%d%%", pct);
			int32_t tw = text_width(ctx, buf);
			int32_t tx = rx + (rw - tw) / 2;
			int32_t ty = ry + (rh - ctx->font_height) / 2;
			push_text_clip(tx, ty, buf, ctx->theme.text, rx + 1, ry + 1, rx + rw - 1, ry + rh - 1);
		}
	}
}

// [=]===^=[ mkgui_meter_setup ]===================================[=]
MKGUI_API void mkgui_meter_setup(struct mkgui_ctx *ctx, uint32_t id, int32_t max_val) {
	MKGUI_CHECK(ctx);
	struct mkgui_meter_data *md = find_meter_data(ctx, id);
	if(md) {
		md->max_val = max_val;
		md->value = 0;
		md->zone_t1 = max_val * 75 / 100;
		md->zone_t2 = max_val * 90 / 100;
		dirty_widget_id(ctx, id);
	}
}

// [=]===^=[ mkgui_meter_set ]=====================================[=]
MKGUI_API void mkgui_meter_set(struct mkgui_ctx *ctx, uint32_t id, int32_t value) {
	MKGUI_CHECK(ctx);
	struct mkgui_meter_data *md = find_meter_data(ctx, id);
	if(md) {
		md->value = value;
		if(md->value < 0) {
			md->value = 0;
		}
		if(md->value > md->max_val) {
			md->value = md->max_val;
		}
		dirty_widget_id(ctx, id);
	}
}

// [=]===^=[ mkgui_meter_get ]=====================================[=]
MKGUI_API int32_t mkgui_meter_get(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_meter_data *md = find_meter_data(ctx, id);
	return md ? md->value : 0;
}

// [=]===^=[ mkgui_meter_set_range ]================================[=]
MKGUI_API void mkgui_meter_set_range(struct mkgui_ctx *ctx, uint32_t id, int32_t max_val) {
	MKGUI_CHECK(ctx);
	struct mkgui_meter_data *md = find_meter_data(ctx, id);
	if(!md) {
		return;
	}
	md->max_val = max_val;
	if(md->value > md->max_val) {
		md->value = md->max_val;
	}
	dirty_widget_id(ctx, id);
}

// [=]===^=[ mkgui_meter_get_range ]================================[=]
MKGUI_API void mkgui_meter_get_range(struct mkgui_ctx *ctx, uint32_t id, int32_t *max_val) {
	MKGUI_CHECK(ctx);
	struct mkgui_meter_data *md = find_meter_data(ctx, id);
	if(max_val) {
		*max_val = md ? md->max_val : 0;
	}
}

// [=]===^=[ mkgui_meter_set_zones ]================================[=]
MKGUI_API void mkgui_meter_set_zones(struct mkgui_ctx *ctx, uint32_t id, int32_t t1, int32_t t2, uint32_t c1, uint32_t c2, uint32_t c3) {
	MKGUI_CHECK(ctx);
	struct mkgui_meter_data *md = find_meter_data(ctx, id);
	if(!md) {
		return;
	}
	md->zone_t1 = t1;
	md->zone_t2 = t2;
	md->zone_c1 = c1;
	md->zone_c2 = c2;
	md->zone_c3 = c3;
	dirty_widget_id(ctx, id);
}
