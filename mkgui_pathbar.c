// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ pathbar_rebuild_segments ]=============================[=]
static void pathbar_rebuild_segments(struct mkgui_ctx *ctx, struct mkgui_pathbar_data *pb) {
	int32_t sep_w = sc(ctx, 8);
	int32_t seg_pad = sc(ctx, 6);
	pb->segment_count = 0;
	if(pb->path[0] == '\0') {
		return;
	}

	uint32_t len = (uint32_t)strlen(pb->path);
	uint32_t i = 0;

#ifdef _WIN32
	if(len >= 2 && pb->path[1] == ':') {
		pb->segments[0].offset = 0;
		pb->segments[0].len = (pb->path[2] == '\\') ? 3 : 2;
		pb->segments[0].x = 0;
		char tmp[4];
		memcpy(tmp, pb->path, pb->segments[0].len);
		tmp[pb->segments[0].len] = '\0';
		pb->segments[0].w = text_width(ctx, tmp) + seg_pad * 2;
		pb->segment_count = 1;
		i = pb->segments[0].len;
	}
#else
	if(pb->path[0] == '/') {
		pb->segments[0].offset = 0;
		pb->segments[0].len = 1;
		pb->segments[0].x = 0;
		pb->segments[0].w = sc(ctx, 18) + seg_pad * 2;
		pb->segment_count = 1;
		i = 1;
	}
#endif

	while(i < len && pb->segment_count < MKGUI_PATHBAR_MAX_SEGS) {
		while(i < len && (pb->path[i] == '/' || pb->path[i] == '\\')) {
			++i;
		}

		if(i >= len) {
			break;
		}
		uint32_t start = i;
		while(i < len && pb->path[i] != '/' && pb->path[i] != '\\') {
			++i;
		}
		uint32_t slen = i - start;
		uint32_t si = pb->segment_count++;
		pb->segments[si].offset = start;
		pb->segments[si].len = slen;

		char tmp[256];
		uint32_t cplen = slen < sizeof(tmp) - 1 ? slen : (uint32_t)(sizeof(tmp) - 1);
		memcpy(tmp, &pb->path[start], cplen);
		tmp[cplen] = '\0';
		pb->segments[si].w = text_width(ctx, tmp) + seg_pad * 2;
	}

	int32_t cx = 0;
	for(uint32_t s = 0; s < pb->segment_count; ++s) {
		pb->segments[s].x = cx;
		cx += pb->segments[s].w + sep_w;
	}
}

// [=]===^=[ pathbar_segment_hit ]=================================[=]
static int32_t pathbar_segment_hit(struct mkgui_ctx *ctx, struct mkgui_pathbar_data *pb, int32_t widget_x, int32_t mouse_x) {
	int32_t lx = mouse_x - widget_x - sc(ctx, 2);
	for(uint32_t i = 0; i < pb->segment_count; ++i) {
		if(lx >= pb->segments[i].x && lx < pb->segments[i].x + pb->segments[i].w) {
			return (int32_t)i;
		}
	}
	return -1;
}

// [=]===^=[ pathbar_build_segment_path ]==========================[=]
static void pathbar_build_segment_path(struct mkgui_pathbar_data *pb, uint32_t seg_idx, char *out, uint32_t out_size) {
	if(seg_idx >= pb->segment_count || out_size == 0) {
		out[0] = '\0';
		return;
	}
	uint32_t end = pb->segments[seg_idx].offset + pb->segments[seg_idx].len;
	uint32_t cplen = end < out_size - 1 ? end : out_size - 1;
	memcpy(out, pb->path, cplen);
	out[cplen] = '\0';

#ifdef _WIN32
	if(cplen == 2 && out[1] == ':') {
		out[2] = '\\';
		out[3] = '\0';
	}
#else
	if(cplen == 0 || (cplen == 1 && out[0] == '/')) {
		out[0] = '/';
		out[1] = '\0';
	}
#endif
}

// [=]===^=[ pathbar_enter_edit ]====================================[=]
static void pathbar_enter_edit(struct mkgui_pathbar_data *pb) {
	pb->editing = 1;
	uint32_t plen = (uint32_t)strlen(pb->path);
	memcpy(pb->edit_buf, pb->path, plen + 1);
	pb->edit_cursor = plen;
	pb->edit_sel_start = 0;
	pb->edit_sel_end = plen;
	pb->edit_scroll_x = 0;
}

// [=]===^=[ pathbar_exit_edit ]=====================================[=]
static void pathbar_exit_edit(struct mkgui_pathbar_data *pb) {
	pb->editing = 0;
}

// [=]===^=[ render_pathbar ]========================================[=]
static void render_pathbar(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	int32_t text_pad = sc(ctx, 4);
	int32_t inset2 = sc(ctx, 2);
	int32_t seg_pad = sc(ctx, 6);
	int32_t sep_w = sc(ctx, 8);

	struct mkgui_pathbar_data *pb = find_pathbar_data(ctx, w->id);
	if(!pb) {
		return;
	}

	uint32_t focused = (ctx->focus_id == w->id);
	draw_patch(ctx, MKGUI_STYLE_SUNKEN, rx, ry, rw, rh, ctx->theme.input_bg, focused ? ctx->theme.highlight : ctx->theme.widget_border);

	if(pb->editing) {
		int32_t ty = ry + (rh - ctx->font_height) / 2;
		int32_t tx = rx + text_pad - pb->edit_scroll_x;

		struct mkgui_text_edit te = {pb->edit_buf, (uint32_t)sizeof(pb->edit_buf), pb->edit_cursor, pb->edit_sel_start, pb->edit_sel_end, pb->edit_scroll_x};
		textedit_render(ctx, &te, pb->edit_buf, tx, ty, ry + inset2, rh - inset2 * 2, rx + 1, ry + 1, rx + rw - 1, ry + rh - 1, ctx->theme.text, focused);
		return;
	}

	int32_t ty = ry + (rh - ctx->font_height) / 2;

	for(uint32_t i = 0; i < pb->segment_count; ++i) {
		int32_t sx = rx + inset2 + pb->segments[i].x;
		int32_t sw = pb->segments[i].w;

		if(sx + sw < rx || sx > rx + rw) {
			continue;
		}

		if((int32_t)i == pb->hover_seg) {
			int32_t r = ctx->theme.corner_radius;
			draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, sx, ry + inset2, sw, rh - inset2 * 2, ctx->theme.widget_hover, r);
		}

		uint32_t is_root = 0;
#ifdef _WIN32
		is_root = (i == 0 && pb->segments[0].len <= 3 && pb->path[1] == ':');
#else
		is_root = (i == 0 && pb->segments[0].len == 1 && pb->path[0] == '/');
#endif

		if(is_root) {
			int32_t ii = icon_resolve("folder");
			if(ii >= 0) {
				draw_icon(ctx->pixels, ctx->win_w, ctx->win_h, &icons[ii], sx + seg_pad, ry + (rh - icons[ii].h) / 2, rx + 1, ry + 1, rx + rw - 1, ry + rh - 1);
			}
		} else {
			char tmp[256];
			uint32_t cplen = pb->segments[i].len < sizeof(tmp) - 1 ? pb->segments[i].len : (uint32_t)(sizeof(tmp) - 1);
			memcpy(tmp, &pb->path[pb->segments[i].offset], cplen);
			tmp[cplen] = '\0';
			push_text_clip(sx + seg_pad, ty, tmp, ctx->theme.text, rx + 1, ry + 1, rx + rw - 1, ry + rh - 1);
		}

		if(i + 1 < pb->segment_count) {
			int32_t sep_x = sx + sw + sep_w / 2 - inset2;
			push_text_clip(sep_x, ty, ">", ctx->theme.text_disabled, rx + 1, ry + 1, rx + rw - 1, ry + rh - 1);
		}
	}
}

// [=]===^=[ handle_pathbar_click ]=================================[=]
static uint32_t handle_pathbar_click(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t idx, int32_t mx, int32_t my) {
	(void)my;
	struct mkgui_widget *w = &ctx->widgets[idx];
	struct mkgui_pathbar_data *pb = find_pathbar_data(ctx, w->id);
	if(!pb) {
		return 0;
	}

	if(pb->editing) {
		int32_t rx = ctx->rects[idx].x;
		int32_t base_x = rx + sc(ctx, 4) - pb->edit_scroll_x;
		uint32_t hit_pos = textedit_hit_cursor(ctx, pb->edit_buf, base_x, mx);
		pb->edit_cursor = hit_pos;
		pb->edit_sel_start = hit_pos;
		pb->edit_sel_end = hit_pos;
		ctx->drag_select_id = w->id;
		dirty_all(ctx);
		return 0;
	}

	int32_t seg = pathbar_segment_hit(ctx, pb, ctx->rects[idx].x, mx);
	if(seg >= 0) {
		ev->type = MKGUI_EVENT_PATHBAR_NAV;
		ev->id = w->id;
		ev->value = seg;
		dirty_all(ctx);
		return 1;
	}

	pathbar_enter_edit(pb);
	dirty_all(ctx);
	return 0;
}

// [=]===^=[ pathbar_scroll_to_cursor ]==============================[=]
static void pathbar_scroll_to_cursor(struct mkgui_ctx *ctx, struct mkgui_pathbar_data *pb) {
	int32_t widx = find_widget_idx(ctx, pb->widget_id);
	if(widx < 0) {
		return;
	}
	int32_t pad = sc(ctx, 4);
	int32_t visible = ctx->rects[widx].w - pad * 2;

	struct mkgui_text_edit te = {pb->edit_buf, (uint32_t)sizeof(pb->edit_buf), pb->edit_cursor, pb->edit_sel_start, pb->edit_sel_end, pb->edit_scroll_x};
	textedit_scroll_to_cursor(ctx, &te, pb->edit_buf, visible);
	pb->edit_scroll_x = te.scroll_x;
}

// [=]===^=[ handle_pathbar_key ]===================================[=]
static uint32_t handle_pathbar_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks, uint32_t keymod, char *buf, int32_t len) {
	struct mkgui_pathbar_data *pb = find_pathbar_data(ctx, ctx->focus_id);
	if(!pb) {
		return 0;
	}

	if(!pb->editing) {
		if(ks == MKGUI_KEY_RETURN || ks == ' ') {
			pathbar_enter_edit(pb);
			dirty_all(ctx);
			return 0;
		}
		return 0;
	}

	uint32_t shift = (keymod & MKGUI_MOD_SHIFT);

	switch(ks) {
	case MKGUI_KEY_ESCAPE: {
		pathbar_exit_edit(pb);
		dirty_all(ctx);
		return 0;
	}

	case MKGUI_KEY_RETURN: {
		uint32_t elen = (uint32_t)strlen(pb->edit_buf);
		memcpy(pb->path, pb->edit_buf, elen + 1);
		pathbar_rebuild_segments(ctx, pb);
		pathbar_exit_edit(pb);
		ev->type = MKGUI_EVENT_PATHBAR_SUBMIT;
		ev->id = ctx->focus_id;
		dirty_all(ctx);
		return 1;
	}

	case MKGUI_KEY_LEFT: {
		struct mkgui_text_edit te = {pb->edit_buf, (uint32_t)sizeof(pb->edit_buf), pb->edit_cursor, pb->edit_sel_start, pb->edit_sel_end, pb->edit_scroll_x};
		textedit_move_left(&te, shift);
		pb->edit_cursor = te.cursor;
		pb->edit_sel_start = te.sel_start;
		pb->edit_sel_end = te.sel_end;
		dirty_all(ctx);
		pathbar_scroll_to_cursor(ctx, pb);
		return 0;
	}

	case MKGUI_KEY_RIGHT: {
		struct mkgui_text_edit te = {pb->edit_buf, (uint32_t)sizeof(pb->edit_buf), pb->edit_cursor, pb->edit_sel_start, pb->edit_sel_end, pb->edit_scroll_x};
		textedit_move_right(&te, shift);
		pb->edit_cursor = te.cursor;
		pb->edit_sel_start = te.sel_start;
		pb->edit_sel_end = te.sel_end;
		dirty_all(ctx);
		pathbar_scroll_to_cursor(ctx, pb);
		return 0;
	}

	case MKGUI_KEY_HOME: {
		struct mkgui_text_edit te = {pb->edit_buf, (uint32_t)sizeof(pb->edit_buf), pb->edit_cursor, pb->edit_sel_start, pb->edit_sel_end, pb->edit_scroll_x};
		textedit_move_home(&te, shift);
		pb->edit_cursor = te.cursor;
		pb->edit_sel_start = te.sel_start;
		pb->edit_sel_end = te.sel_end;
		dirty_all(ctx);
		pathbar_scroll_to_cursor(ctx, pb);
		return 0;
	}

	case MKGUI_KEY_END: {
		struct mkgui_text_edit te = {pb->edit_buf, (uint32_t)sizeof(pb->edit_buf), pb->edit_cursor, pb->edit_sel_start, pb->edit_sel_end, pb->edit_scroll_x};
		textedit_move_end(&te, shift);
		pb->edit_cursor = te.cursor;
		pb->edit_sel_start = te.sel_start;
		pb->edit_sel_end = te.sel_end;
		dirty_all(ctx);
		pathbar_scroll_to_cursor(ctx, pb);
		return 0;
	}

	case MKGUI_KEY_BACKSPACE: {
		struct mkgui_text_edit te = {pb->edit_buf, (uint32_t)sizeof(pb->edit_buf), pb->edit_cursor, pb->edit_sel_start, pb->edit_sel_end, pb->edit_scroll_x};
		textedit_backspace(&te);
		pb->edit_cursor = te.cursor;
		pb->edit_sel_start = te.sel_start;
		pb->edit_sel_end = te.sel_end;
		dirty_all(ctx);
		pathbar_scroll_to_cursor(ctx, pb);
		return 0;
	}

	case MKGUI_KEY_DELETE: {
		struct mkgui_text_edit te = {pb->edit_buf, (uint32_t)sizeof(pb->edit_buf), pb->edit_cursor, pb->edit_sel_start, pb->edit_sel_end, pb->edit_scroll_x};
		textedit_delete(&te);
		pb->edit_cursor = te.cursor;
		pb->edit_sel_start = te.sel_start;
		pb->edit_sel_end = te.sel_end;
		dirty_all(ctx);
		pathbar_scroll_to_cursor(ctx, pb);
		return 0;
	}

	default: {
		if(len > 0 && (uint8_t)buf[0] >= 32) {
			struct mkgui_text_edit te = {pb->edit_buf, (uint32_t)sizeof(pb->edit_buf), pb->edit_cursor, pb->edit_sel_start, pb->edit_sel_end, pb->edit_scroll_x};
			textedit_insert(&te, buf, (uint32_t)len);
			pb->edit_cursor = te.cursor;
			pb->edit_sel_start = te.sel_start;
			pb->edit_sel_end = te.sel_end;
			dirty_all(ctx);
			pathbar_scroll_to_cursor(ctx, pb);
		}
		return 0;
	}
	}
}

// [=]===^=[ pathbar_focus_lost ]====================================[=]
static void pathbar_focus_lost(struct mkgui_ctx *ctx) {
	if(!ctx->focus_id) {
		return;
	}
	struct mkgui_pathbar_data *pb = find_pathbar_data(ctx, ctx->focus_id);
	if(pb && pb->editing) {
		pathbar_exit_edit(pb);
		dirty_all(ctx);
	}
}

// [=]===^=[ mkgui_pathbar_set ]=====================================[=]
MKGUI_API void mkgui_pathbar_set(struct mkgui_ctx *ctx, uint32_t id, const char *path) {
	MKGUI_CHECK(ctx);
	if(!path) {
		path = "";
	}
	struct mkgui_pathbar_data *pb = find_pathbar_data(ctx, id);
	if(!pb) {
		return;
	}
	snprintf(pb->path, sizeof(pb->path), "%s", path);
	pathbar_rebuild_segments(ctx, pb);
	if(pb->editing) {
		pathbar_exit_edit(pb);
	}
	dirty_all(ctx);
}

// [=]===^=[ mkgui_pathbar_get ]=====================================[=]
MKGUI_API const char *mkgui_pathbar_get(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, "");
	struct mkgui_pathbar_data *pb = find_pathbar_data(ctx, id);
	if(!pb) {
		return "";
	}
	return pb->path;
}

// [=]===^=[ mkgui_pathbar_get_segment_path ]========================[=]
MKGUI_API void mkgui_pathbar_get_segment_path(struct mkgui_ctx *ctx, uint32_t id, uint32_t seg_idx, char *out, uint32_t out_size) {
	MKGUI_CHECK(ctx);
	if(!out || out_size == 0) {
		return;
	}
	struct mkgui_pathbar_data *pb = find_pathbar_data(ctx, id);
	if(!pb) {
		if(out_size > 0) {
			out[0] = '\0';
		}
		return;
	}
	pathbar_build_segment_path(pb, seg_idx, out, out_size);
}
