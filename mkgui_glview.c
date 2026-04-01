// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ find_glview_data ]==================================[=]
static struct mkgui_glview_data *find_glview_data(struct mkgui_ctx *ctx, uint32_t id) {
	for(uint32_t i = 0; i < ctx->glview_count; ++i) {
		if(ctx->glviews[i].id == id) {
			return &ctx->glviews[i];
		}
	}
	return NULL;
}

// [=]===^=[ glview_sync_all ]====================================[=]
static void glview_sync_all(struct mkgui_ctx *ctx) {
	for(uint32_t i = 0; i < ctx->glview_count; ++i) {
		struct mkgui_glview_data *gv = &ctx->glviews[i];
		if(!gv->created) {
			continue;
		}

		int32_t idx = find_widget_idx(ctx, gv->id);
		if(idx < 0) {
			continue;
		}

		uint32_t vis = widget_visible(ctx, (uint32_t)idx);
		if(vis != gv->visible) {
			gv->visible = vis;
			platform_glview_show(ctx, gv, vis);
		}

		if(!vis) {
			continue;
		}

		int32_t rx = ctx->rects[idx].x;
		int32_t ry = ctx->rects[idx].y;
		int32_t rw = ctx->rects[idx].w;
		int32_t rh = ctx->rects[idx].h;

		struct mkgui_widget *w = &ctx->widgets[idx];
		if(w->style & MKGUI_GLVIEW_BORDER) {
			rx += 1;
			ry += 1;
			rw -= 2;
			rh -= 2;
		}

		if(rw < 1) {
			rw = 1;
		}
		if(rh < 1) {
			rh = 1;
		}

		if(rx != gv->last_x || ry != gv->last_y || rw != gv->last_w || rh != gv->last_h) {
			gv->last_x = rx;
			gv->last_y = ry;
			gv->last_w = rw;
			gv->last_h = rh;
			platform_glview_reposition(ctx, gv, rx, ry, rw, rh);
		}
	}
}

// [=]===^=[ render_glview ]======================================[=]
static void render_glview(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];

	if(w->style & MKGUI_GLVIEW_BORDER) {
		int32_t rx = ctx->rects[idx].x;
		int32_t ry = ctx->rects[idx].y;
		int32_t rw = ctx->rects[idx].w;
		int32_t rh = ctx->rects[idx].h;
		draw_rect_border(ctx->pixels, ctx->win_w, ctx->win_h, rx, ry, rw, rh, ctx->theme.widget_border);
	}
}

// [=]===^=[ mkgui_glview_init ]==================================[=]
MKGUI_API uint32_t mkgui_glview_init(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_glview_data *gv = find_glview_data(ctx, id);
	if(gv) {
		return gv->created;
	}

	MKGUI_AUX_GROW(ctx->glviews, ctx->glview_count, ctx->glview_cap, struct mkgui_glview_data);
	if(ctx->glview_count >= ctx->glview_cap) {
		return 0;
	}
	gv = &ctx->glviews[ctx->glview_count++];
	gv->id = id;
	gv->created = 0;
	gv->last_x = 0;
	gv->last_y = 0;
	gv->last_w = 0;
	gv->last_h = 0;

	int32_t idx = find_widget_idx(ctx, id);
	if(idx < 0) {
		return 0;
	}

	layout_widgets(ctx);

	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	struct mkgui_widget *w = &ctx->widgets[idx];
	if(w->style & MKGUI_GLVIEW_BORDER) {
		rx += 1;
		ry += 1;
		rw -= 2;
		rh -= 2;
	}

	if(rw < 1) {
		rw = 1;
	}
	if(rh < 1) {
		rh = 1;
	}

	if(!platform_glview_create(ctx, gv, rx, ry, rw, rh)) {
		return 0;
	}

	gv->created = 1;
	gv->visible = widget_visible(ctx, (uint32_t)idx);
	gv->last_x = rx;
	gv->last_y = ry;
	gv->last_w = rw;
	gv->last_h = rh;

	if(!gv->visible) {
		platform_glview_show(ctx, gv, 0);
	}

	return 1;
}

// [=]===^=[ mkgui_glview_destroy ]================================[=]
MKGUI_API void mkgui_glview_destroy(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK(ctx);
	struct mkgui_glview_data *gv = find_glview_data(ctx, id);
	if(!gv || !gv->created) {
		return;
	}
	platform_glview_destroy(ctx, gv);
	gv->created = 0;
}

// [=]===^=[ mkgui_glview_get_size ]===============================[=]
MKGUI_API void mkgui_glview_get_size(struct mkgui_ctx *ctx, uint32_t id, int32_t *w, int32_t *h) {
	MKGUI_CHECK(ctx);
	struct mkgui_glview_data *gv = find_glview_data(ctx, id);
	if(!gv || !gv->created) {
		if(w) {
			*w = 0;
		}
		if(h) {
			*h = 0;
		}
		return;
	}
	if(w) {
		*w = gv->last_w;
	}
	if(h) {
		*h = gv->last_h;
	}
}

// [=]===^=[ mkgui_glview_get_native_window ]======================[=]
#ifdef _WIN32
MKGUI_API HWND mkgui_glview_get_hwnd(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, NULL);
	struct mkgui_glview_data *gv = find_glview_data(ctx, id);
	if(!gv || !gv->created) {
		return NULL;
	}
	return gv->plat.hwnd;
}
#else
MKGUI_API Window mkgui_glview_get_x11_window(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_glview_data *gv = find_glview_data(ctx, id);
	if(!gv || !gv->created) {
		return 0;
	}
	return gv->plat.xwin;
}

// [=]===^=[ mkgui_glview_get_x11_display ]========================[=]
MKGUI_API Display *mkgui_glview_get_x11_display(struct mkgui_ctx *ctx) {
	MKGUI_CHECK_VAL(ctx, NULL);
	return ctx->plat.dpy;
}
#endif
