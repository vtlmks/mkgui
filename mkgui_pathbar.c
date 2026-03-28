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
	draw_patch(ctx, MKGUI_STYLE_SUNKEN, rx, ry, rw, rh, ctx->theme.input_bg, focused ? ctx->theme.splitter : ctx->theme.widget_border);

	if(pb->editing) {
		char *display = pb->edit_buf;
		int32_t ty = ry + (rh - ctx->font_height) / 2;
		uint32_t tc = ctx->theme.text;

		uint32_t has_sel = (pb->edit_sel_start != pb->edit_sel_end);
		if(focused && has_sel) {
			uint32_t lo = pb->edit_sel_start < pb->edit_sel_end ? pb->edit_sel_start : pb->edit_sel_end;
			uint32_t hi = pb->edit_sel_start < pb->edit_sel_end ? pb->edit_sel_end : pb->edit_sel_start;
			char tmp[4096];

			memcpy(tmp, display, lo);
			tmp[lo] = '\0';
			int32_t sel_x1 = rx + text_pad + text_width(ctx, tmp);

			memcpy(tmp, display, hi);
			tmp[hi] = '\0';
			int32_t sel_x2 = rx + text_pad + text_width(ctx, tmp);

			int32_t cx1 = sel_x1 < rx + 1 ? rx + 1 : sel_x1;
			int32_t cx2 = sel_x2 > rx + rw - 1 ? rx + rw - 1 : sel_x2;
			if(cx2 > cx1) {
				draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, cx1, ry + inset2, cx2 - cx1, rh - inset2 * 2, ctx->theme.selection);
			}

			push_text_clip(rx + text_pad, ty, display, tc, rx + 1, ry + 1, rx + rw - 1, ry + rh - 1);

			uint32_t sel_len = hi - lo;
			memcpy(tmp, display + lo, sel_len);
			tmp[sel_len] = '\0';
			push_text_clip(sel_x1, ty, tmp, ctx->theme.sel_text, cx1, ry + 1, cx2, ry + rh - 1);

		} else {
			push_text_clip(rx + text_pad, ty, display, tc, rx + 1, ry + 1, rx + rw - 1, ry + rh - 1);
		}

		if(focused) {
			uint32_t cpos = pb->edit_cursor;
			uint32_t dlen = (uint32_t)strlen(display);
			if(cpos > dlen) {
				cpos = dlen;
			}
			char tmp[4096];
			memcpy(tmp, display, cpos);
			tmp[cpos] = '\0';
			int32_t cx = rx + text_pad + text_width(ctx, tmp);
			draw_vline(ctx->pixels, ctx->win_w, ctx->win_h, cx, ry + inset2, rh - inset2 * 2, ctx->theme.text);
		}
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
		int32_t base_x = rx + sc(ctx, 4);
		uint32_t len = (uint32_t)strlen(pb->edit_buf);
		char tmp[4096];
		uint32_t hit_pos = len;
		for(uint32_t i = 0; i <= len; ++i) {
			memcpy(tmp, pb->edit_buf, i);
			tmp[i] = '\0';
			int32_t tw = text_width(ctx, tmp);
			if(base_x + tw >= mx) {
				if(i > 0) {
					tmp[i - 1] = '\0';
					int32_t prev_w = text_width(ctx, tmp);
					if(mx - (base_x + prev_w) < (base_x + tw) - mx) {
						hit_pos = i - 1;
					} else {
						hit_pos = i;
					}
				} else {
					hit_pos = i;
				}
				break;
			}
		}
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
	uint32_t text_len = (uint32_t)strlen(pb->edit_buf);
	uint32_t has_sel = (pb->edit_sel_start != pb->edit_sel_end);

	if(ks == MKGUI_KEY_ESCAPE) {
		pathbar_exit_edit(pb);
		dirty_all(ctx);
		return 0;
	}

	if(ks == MKGUI_KEY_RETURN) {
		uint32_t elen = (uint32_t)strlen(pb->edit_buf);
		memcpy(pb->path, pb->edit_buf, elen + 1);
		pathbar_rebuild_segments(ctx, pb);
		pathbar_exit_edit(pb);
		ev->type = MKGUI_EVENT_PATHBAR_SUBMIT;
		ev->id = ctx->focus_id;
		dirty_all(ctx);
		return 1;
	}

	if(ks == MKGUI_KEY_LEFT) {
		if(pb->edit_cursor > 0) {
			--pb->edit_cursor;
		}
		if(shift) {
			pb->edit_sel_end = pb->edit_cursor;
		} else {
			pb->edit_sel_start = pb->edit_cursor;
			pb->edit_sel_end = pb->edit_cursor;
		}
		dirty_all(ctx);
		return 0;
	}

	if(ks == MKGUI_KEY_RIGHT) {
		if(pb->edit_cursor < text_len) {
			++pb->edit_cursor;
		}
		if(shift) {
			pb->edit_sel_end = pb->edit_cursor;
		} else {
			pb->edit_sel_start = pb->edit_cursor;
			pb->edit_sel_end = pb->edit_cursor;
		}
		dirty_all(ctx);
		return 0;
	}

	if(ks == MKGUI_KEY_HOME) {
		pb->edit_cursor = 0;
		if(shift) {
			pb->edit_sel_end = 0;
		} else {
			pb->edit_sel_start = 0;
			pb->edit_sel_end = 0;
		}
		dirty_all(ctx);
		return 0;
	}

	if(ks == MKGUI_KEY_END) {
		pb->edit_cursor = text_len;
		if(shift) {
			pb->edit_sel_end = text_len;
		} else {
			pb->edit_sel_start = text_len;
			pb->edit_sel_end = text_len;
		}
		dirty_all(ctx);
		return 0;
	}

	if(ks == MKGUI_KEY_BACKSPACE) {
		if(has_sel) {
			uint32_t lo = pb->edit_sel_start < pb->edit_sel_end ? pb->edit_sel_start : pb->edit_sel_end;
			uint32_t hi = pb->edit_sel_start < pb->edit_sel_end ? pb->edit_sel_end : pb->edit_sel_start;
			memmove(&pb->edit_buf[lo], &pb->edit_buf[hi], text_len - hi + 1);
			pb->edit_cursor = lo;
			pb->edit_sel_start = lo;
			pb->edit_sel_end = lo;
		} else if(pb->edit_cursor > 0) {
			memmove(&pb->edit_buf[pb->edit_cursor - 1], &pb->edit_buf[pb->edit_cursor], text_len - pb->edit_cursor + 1);
			--pb->edit_cursor;
			pb->edit_sel_start = pb->edit_cursor;
			pb->edit_sel_end = pb->edit_cursor;
		}
		dirty_all(ctx);
		return 0;
	}

	if(ks == MKGUI_KEY_DELETE) {
		if(has_sel) {
			uint32_t lo = pb->edit_sel_start < pb->edit_sel_end ? pb->edit_sel_start : pb->edit_sel_end;
			uint32_t hi = pb->edit_sel_start < pb->edit_sel_end ? pb->edit_sel_end : pb->edit_sel_start;
			memmove(&pb->edit_buf[lo], &pb->edit_buf[hi], text_len - hi + 1);
			pb->edit_cursor = lo;
			pb->edit_sel_start = lo;
			pb->edit_sel_end = lo;
		} else if(pb->edit_cursor < text_len) {
			memmove(&pb->edit_buf[pb->edit_cursor], &pb->edit_buf[pb->edit_cursor + 1], text_len - pb->edit_cursor);
		}
		dirty_all(ctx);
		return 0;
	}

	if(len > 0 && (uint8_t)buf[0] >= 32) {
		if(has_sel) {
			uint32_t lo = pb->edit_sel_start < pb->edit_sel_end ? pb->edit_sel_start : pb->edit_sel_end;
			uint32_t hi = pb->edit_sel_start < pb->edit_sel_end ? pb->edit_sel_end : pb->edit_sel_start;
			memmove(&pb->edit_buf[lo], &pb->edit_buf[hi], text_len - hi + 1);
			pb->edit_cursor = lo;
			text_len = (uint32_t)strlen(pb->edit_buf);
		}
		if(text_len + (uint32_t)len < sizeof(pb->edit_buf) - 1) {
			memmove(&pb->edit_buf[pb->edit_cursor + (uint32_t)len], &pb->edit_buf[pb->edit_cursor], text_len - pb->edit_cursor + 1);
			memcpy(&pb->edit_buf[pb->edit_cursor], buf, (uint32_t)len);
			pb->edit_cursor += (uint32_t)len;
			pb->edit_sel_start = pb->edit_cursor;
			pb->edit_sel_end = pb->edit_cursor;
		}
		dirty_all(ctx);
		return 0;
	}

	return 0;
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
MKGUI_API void mkgui_pathbar_set(struct mkgui_ctx *ctx, uint32_t id, char *path) {
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
MKGUI_API char *mkgui_pathbar_get(struct mkgui_ctx *ctx, uint32_t id) {
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
