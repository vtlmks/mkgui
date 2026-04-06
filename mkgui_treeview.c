// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define MKGUI_TREE_INDENT_PX 18
#define MKGUI_TREE_ARROW_SIZE_PX 6

#define TV_IDX_HASH_SIZE 1024
#define TV_IDX_HASH_MASK (TV_IDX_HASH_SIZE - 1)

static struct {
	uint32_t id;
	uint32_t idx;
} tv_idx_map[TV_IDX_HASH_SIZE];

// [=]===^=[ tv_dfs_visit ]========================================[=]
static void tv_dfs_visit(struct mkgui_treeview_data *tv, uint32_t parent_id, struct mkgui_treeview_node *out, uint32_t *pos) {
	for(uint32_t i = 0; i < tv->node_count; ++i) {
		if(tv->nodes[i].parent_node == parent_id) {
			out[(*pos)++] = tv->nodes[i];
			if(tv->nodes[i].has_children) {
				tv_dfs_visit(tv, tv->nodes[i].id, out, pos);
			}
		}
	}
}

// [=]===^=[ tv_reorder_dfs ]======================================[=]
static void tv_reorder_dfs(struct mkgui_treeview_data *tv) {
	if(!tv->order_dirty || tv->node_count == 0) {
		return;
	}
	tv->order_dirty = 0;
	struct mkgui_treeview_node *tmp = (struct mkgui_treeview_node *)malloc(tv->node_count * sizeof(struct mkgui_treeview_node));
	if(!tmp) {
		return;
	}
	uint32_t pos = 0;
	tv_dfs_visit(tv, 0, tmp, &pos);
	memcpy(tv->nodes, tmp, pos * sizeof(struct mkgui_treeview_node));
	tv->node_count = pos;
	free(tmp);
}

// [=]===^=[ tv_idx_build ]=======================================[=]
static void tv_idx_build(struct mkgui_treeview_data *tv) {
	tv_reorder_dfs(tv);
	for(uint32_t i = 0; i < TV_IDX_HASH_SIZE; ++i) {
		tv_idx_map[i].idx = UINT32_MAX;
	}
	for(uint32_t i = 0; i < tv->node_count; ++i) {
		uint32_t h = (tv->nodes[i].id * 2654435761u) & TV_IDX_HASH_MASK;
		while(tv_idx_map[h].idx != UINT32_MAX) {
			h = (h + 1) & TV_IDX_HASH_MASK;
		}
		tv_idx_map[h].id = tv->nodes[i].id;
		tv_idx_map[h].idx = i;
	}
}

// [=]===^=[ tv_idx_find ]========================================[=]
static uint32_t tv_idx_find(uint32_t id) {
	uint32_t h = (id * 2654435761u) & TV_IDX_HASH_MASK;
	for(;;) {
		if(tv_idx_map[h].idx == UINT32_MAX) {
			return UINT32_MAX;
		}
		if(tv_idx_map[h].id == id) {
			return tv_idx_map[h].idx;
		}
		h = (h + 1) & TV_IDX_HASH_MASK;
	}
}

// [=]===^=[ treeview_node_depth ]================================[=]
static uint32_t treeview_node_depth(struct mkgui_treeview_data *tv, uint32_t node_idx) {
	uint32_t depth = 0;
	uint32_t parent = tv->nodes[node_idx].parent_node;
	while(parent != 0) {
		uint32_t pi = tv_idx_find(parent);
		if(pi == UINT32_MAX) {
			break;
		}
		parent = tv->nodes[pi].parent_node;
		++depth;
	}
	return depth;
}

// [=]===^=[ treeview_node_visible ]==============================[=]
static uint32_t treeview_node_visible(struct mkgui_treeview_data *tv, uint32_t node_idx) {
	uint32_t parent = tv->nodes[node_idx].parent_node;
	while(parent != 0) {
		uint32_t pi = tv_idx_find(parent);
		if(pi == UINT32_MAX) {
			break;
		}
		if(!tv->nodes[pi].expanded) {
			return 0;
		}
		parent = tv->nodes[pi].parent_node;
	}
	return 1;
}

// [=]===^=[ treeview_is_last_visible_child ]======================[=]
static uint32_t treeview_is_last_visible_child(struct mkgui_treeview_data *tv, uint32_t node_idx) {
	uint32_t parent = tv->nodes[node_idx].parent_node;
	for(uint32_t i = node_idx + 1; i < tv->node_count; ++i) {
		if(tv->nodes[i].parent_node == parent && treeview_node_visible(tv, i)) {
			return 0;
		}
	}
	return 1;
}

// [=]===^=[ render_treeview ]====================================[=]
static void render_treeview(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	uint32_t focused = (ctx->focus_id == w->id);
	draw_patch(ctx, MKGUI_STYLE_SUNKEN, rx, ry, rw, rh, ctx->theme.input_bg, focused ? ctx->theme.highlight : ctx->theme.widget_border);

	struct mkgui_treeview_data *tv = find_treeview_data(ctx, w->id);
	if(!tv) {
		return;
	}
	tv_idx_build(tv);

	int32_t tree_indent = sc(ctx, MKGUI_TREE_INDENT_PX);
	int32_t arrow_size = sc(ctx, MKGUI_TREE_ARROW_SIZE_PX);
	int32_t tree_pad = sc(ctx, 4);
	int32_t icon_gap = sc(ctx, 2);

	int32_t clip_top = ry + 1;
	int32_t clip_bottom = ry + rh - 1;
	int32_t view_h = rh - 2;

	uint32_t vis_count = 0;
	for(uint32_t vi = 0; vi < tv->node_count; ++vi) {
		if(treeview_node_visible(tv, vi)) {
			++vis_count;
		}
	}
	int32_t max_scroll = (int32_t)vis_count * ctx->row_height - view_h;
	if(max_scroll < 0) {
		max_scroll = 0;
	}
	if(tv->scroll_y < 0) {
		tv->scroll_y = 0;
	}
	if(tv->scroll_y > max_scroll) {
		tv->scroll_y = max_scroll;
	}

	int32_t draw_y = ry + 1 - tv->scroll_y;

	uint32_t line_active[32] = {0};
	uint32_t line_color = shade_color(ctx->theme.widget_border, 20);

	for(uint32_t i = 0; i < tv->node_count; ++i) {
		if(!treeview_node_visible(tv, i)) {
			continue;
		}

		int32_t row_y = draw_y;
		int32_t row_end = draw_y + ctx->row_height;
		draw_y += ctx->row_height;

		uint32_t depth = treeview_node_depth(tv, i);
		uint32_t is_last = treeview_is_last_visible_child(tv, i);

		if(depth > 0) {
			if(is_last) {
				line_active[depth - 1] = 0;
			} else {
				line_active[depth - 1] = 1;
			}
		}
		if(tv->nodes[i].has_children && tv->nodes[i].expanded) {
			line_active[depth] = 1;
		}

		if(row_end <= clip_top || row_y >= clip_bottom) {
			continue;
		}

		int32_t indent = (int32_t)depth * tree_indent;

		if(depth > 0) {
			for(uint32_t d = 0; d + 1 < depth; ++d) {
				if(line_active[d]) {
					int32_t lx = rx + tree_pad + (int32_t)d * tree_indent + tree_indent / 2;
					draw_vline(ctx->pixels, ctx->win_w, ctx->win_h, lx, row_y, ctx->row_height, line_color);
				}
			}

			int32_t lx = rx + tree_pad + (int32_t)(depth - 1) * tree_indent + tree_indent / 2;
			int32_t cy = row_y + ctx->row_height / 2;
			if(is_last) {
				draw_vline(ctx->pixels, ctx->win_w, ctx->win_h, lx, row_y, ctx->row_height / 2 + 1, line_color);
			} else {
				draw_vline(ctx->pixels, ctx->win_w, ctx->win_h, lx, row_y, ctx->row_height, line_color);
			}
			int32_t hlen = tv->nodes[i].has_children ? tree_indent / 2 - 1 : tree_indent + tree_indent / 2 - sc(ctx, 4) - 1;
			draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, lx + 1, cy, hlen, line_color);
		}

		if((int32_t)tv->nodes[i].id == tv->selected_node) {
			int32_t dy = row_y < clip_top ? clip_top : row_y;
			int32_t de = row_end > clip_bottom ? clip_bottom : row_end;
			draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx + 1, dy, rw - 2, de - dy, ctx->theme.selection);
		}

		if(tv->nodes[i].has_children) {
			int32_t ax = rx + tree_pad + indent;
			int32_t ay = row_y + ctx->row_height / 2;

			if(ay >= clip_top && ay < clip_bottom) {
				int32_t as = arrow_size;
				if(tv->nodes[i].expanded) {
					draw_triangle_aa(ctx->pixels, ctx->win_w, ctx->win_h,
						ax, ay - as / 2, ax + as, ay - as / 2, ax + as / 2, ay + as / 2, ctx->theme.text);
				} else {
					draw_triangle_aa(ctx->pixels, ctx->win_w, ctx->win_h,
						ax, ay - as / 2, ax + as, ay, ax, ay + as / 2, ctx->theme.text);
				}
			}
		}

		int32_t text_x = rx + tree_pad + indent + tree_indent;
		if(tv->nodes[i].icon_idx >= 0 && tv->nodes[i].icon_idx < (int32_t)icon_count) {
			int32_t ti = tv->nodes[i].icon_idx;
			int32_t iy = row_y + (ctx->row_height - icons[ti].h) / 2;
			draw_icon(ctx->pixels, ctx->win_w, ctx->win_h, &icons[ti], text_x, iy, rx + 1, clip_top, rx + rw - 1, clip_bottom);
			text_x += icons[ti].w + icon_gap;
		}
		int32_t ty = row_y + (ctx->row_height - ctx->font_height) / 2;
		if(ty >= clip_top && ty + ctx->font_height <= clip_bottom) {
			uint32_t tc = ((int32_t)tv->nodes[i].id == tv->selected_node) ? ctx->theme.sel_text : ((w->flags & MKGUI_DISABLED) ? ctx->theme.text_disabled : ctx->theme.text);
			push_text_clip(text_x, ty, tv->nodes[i].label, tc, rx + 1, clip_top, rx + rw - 1, clip_bottom);
		}
	}

	if(tv->drag_active && tv->drag_target >= 0 && (uint32_t)tv->drag_target < tv->node_count) {
		uint32_t depth = treeview_node_depth(tv, (uint32_t)tv->drag_target);
		int32_t indent = (int32_t)depth * tree_indent + tree_indent + tree_pad;
		int32_t vis = 0;
		for(uint32_t i = 0; i < (uint32_t)tv->drag_target; ++i) {
			if(treeview_node_visible(tv, i)) {
				++vis;
			}
		}
		if(treeview_node_visible(tv, (uint32_t)tv->drag_target)) {
			int32_t dot_sz = sc(ctx, 5);
			int32_t line_y;
			if(tv->drag_pos == 0) {
				line_y = ry + 1 - tv->scroll_y + vis * ctx->row_height;
			} else if(tv->drag_pos == 2) {
				indent = (int32_t)(depth + 1) * tree_indent + tree_indent + tree_pad;
				line_y = ry + 1 - tv->scroll_y + vis * ctx->row_height + ctx->row_height / 2;
			} else {
				line_y = ry + 1 - tv->scroll_y + (vis + 1) * ctx->row_height;
			}
			if(line_y >= clip_top && line_y < clip_bottom) {
				draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, rx + indent, line_y, rw - indent - 2, ctx->theme.accent);
				draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, rx + indent, line_y - 1, rw - indent - 2, ctx->theme.accent);
				draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx + indent - 2, line_y - 2, dot_sz, dot_sz, ctx->theme.accent);
			}
		}
	}
}

// [=]===^=[ treeview_visible_count ]=============================[=]
static uint32_t treeview_visible_count(struct mkgui_treeview_data *tv) {
	tv_idx_build(tv);
	uint32_t count = 0;
	for(uint32_t i = 0; i < tv->node_count; ++i) {
		if(treeview_node_visible(tv, i)) {
			++count;
		}
	}
	return count;
}

// [=]===^=[ treeview_clamp_scroll ]==============================[=]
static void treeview_clamp_scroll(struct mkgui_ctx *ctx, uint32_t widget_id) {
	struct mkgui_treeview_data *tv = find_treeview_data(ctx, widget_id);
	if(!tv) {
		return;
	}
	int32_t idx = find_widget_idx(ctx, widget_id);
	if(idx < 0) {
		return;
	}
	int32_t content_h = (int32_t)treeview_visible_count(tv) * ctx->row_height;
	int32_t view_h = ctx->rects[idx].h - 2;
	int32_t max_scroll = content_h - view_h;
	if(max_scroll < 0) {
		max_scroll = 0;
	}
	if(tv->scroll_y < 0) {
		tv->scroll_y = 0;
	}
	if(tv->scroll_y > max_scroll) {
		tv->scroll_y = max_scroll;
	}
}

// [=]===^=[ treeview_row_hit ]===================================[=]
static int32_t treeview_row_hit(struct mkgui_ctx *ctx, uint32_t widget_idx, int32_t my) {
	struct mkgui_widget *w = &ctx->widgets[widget_idx];
	struct mkgui_treeview_data *tv = find_treeview_data(ctx, w->id);
	if(!tv) {
		return -1;
	}

	tv_idx_build(tv);
	int32_t ry = ctx->rects[widget_idx].y;
	int32_t local_y = my - ry - 1 + tv->scroll_y;
	int32_t vis_row = local_y / ctx->row_height;

	int32_t count = 0;
	for(uint32_t i = 0; i < tv->node_count; ++i) {
		if(!treeview_node_visible(tv, i)) {
			continue;
		}
		if(count == vis_row) {
			return (int32_t)i;
		}
		++count;
	}
	return -1;
}

// [=]===^=[ treeview_arrow_hit ]=================================[=]
static uint32_t treeview_arrow_hit(struct mkgui_ctx *ctx, uint32_t widget_idx, int32_t mx, int32_t node_idx) {
	struct mkgui_widget *w = &ctx->widgets[widget_idx];
	struct mkgui_treeview_data *tv = find_treeview_data(ctx, w->id);
	if(!tv || node_idx < 0 || (uint32_t)node_idx >= tv->node_count) {
		return 0;
	}
	if(!tv->nodes[node_idx].has_children) {
		return 0;
	}
	tv_idx_build(tv);
	int32_t tree_indent = sc(ctx, MKGUI_TREE_INDENT_PX);
	int32_t tree_pad = sc(ctx, 4);
	uint32_t depth = treeview_node_depth(tv, (uint32_t)node_idx);
	int32_t rx = ctx->rects[widget_idx].x;
	int32_t arrow_x = rx + tree_pad + (int32_t)depth * tree_indent;
	return (mx >= arrow_x && mx < arrow_x + tree_indent);
}

// [=]===^=[ handle_treeview_key ]================================[=]
static uint32_t handle_treeview_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks) {
	struct mkgui_treeview_data *tv = find_treeview_data(ctx, ctx->focus_id);
	if(!tv || tv->node_count == 0) {
		return 0;
	}
	tv_idx_build(tv);

	switch(ks) {
	case MKGUI_KEY_UP: {
		int32_t prev = -1;
		for(uint32_t i = 0; i < tv->node_count; ++i) {
			if(!treeview_node_visible(tv, i)) {
				continue;
			}
			if((int32_t)tv->nodes[i].id == tv->selected_node) {
				break;
			}
			prev = (int32_t)i;
		}
		if(prev >= 0) {
			tv->selected_node = (int32_t)tv->nodes[prev].id;
			dirty_all(ctx);
			treeview_clamp_scroll(ctx, ctx->focus_id);
			ev->type = MKGUI_EVENT_TREEVIEW_SELECT;
			ev->id = ctx->focus_id;
			ev->value = tv->selected_node;
			return 1;
		}
		break;
	}

	case MKGUI_KEY_DOWN: {
		uint32_t found = 0;
		for(uint32_t i = 0; i < tv->node_count; ++i) {
			if(!treeview_node_visible(tv, i)) {
				continue;
			}
			if(found) {
				tv->selected_node = (int32_t)tv->nodes[i].id;
				dirty_all(ctx);
				treeview_clamp_scroll(ctx, ctx->focus_id);
				ev->type = MKGUI_EVENT_TREEVIEW_SELECT;
				ev->id = ctx->focus_id;
				ev->value = tv->selected_node;
				return 1;
			}
			if((int32_t)tv->nodes[i].id == tv->selected_node) {
				found = 1;
			}
		}
		break;
	}

	case MKGUI_KEY_RIGHT: {
		for(uint32_t i = 0; i < tv->node_count; ++i) {
			if((int32_t)tv->nodes[i].id == tv->selected_node && tv->nodes[i].has_children && !tv->nodes[i].expanded) {
				tv->nodes[i].expanded = 1;
				dirty_all(ctx);
				ev->type = MKGUI_EVENT_TREEVIEW_EXPAND;
				ev->id = ctx->focus_id;
				ev->value = tv->selected_node;
				return 1;
			}
		}
		break;
	}

	case MKGUI_KEY_PAGE_UP:
	case MKGUI_KEY_PAGE_DOWN:
	case MKGUI_KEY_HOME:
	case MKGUI_KEY_END: {
		uint32_t vis_idx = 0;
		uint32_t vis_total = 0;
		for(uint32_t i = 0; i < tv->node_count; ++i) {
			if(!treeview_node_visible(tv, i)) {
				continue;
			}
			if((int32_t)tv->nodes[i].id == tv->selected_node) {
				vis_idx = vis_total;
			}
			++vis_total;
		}
		if(vis_total == 0) {
			return 0;
		}
		int32_t idx = find_widget_idx(ctx, ctx->focus_id);
		int32_t page = idx >= 0 ? (ctx->rects[idx].h - 2) / ctx->row_height : 10;
		if(page < 1) {
			page = 1;
		}
		int32_t target = (int32_t)vis_idx;
		if(ks == MKGUI_KEY_PAGE_UP) {
			target -= page;
		} else if(ks == MKGUI_KEY_PAGE_DOWN) {
			target += page;
		} else if(ks == MKGUI_KEY_HOME) {
			target = 0;
		} else {
			target = (int32_t)vis_total - 1;
		}
		if(target < 0) {
			target = 0;
		}
		if(target >= (int32_t)vis_total) {
			target = (int32_t)vis_total - 1;
		}
		uint32_t vis_cur = 0;
		for(uint32_t i = 0; i < tv->node_count; ++i) {
			if(!treeview_node_visible(tv, i)) {
				continue;
			}
			if((int32_t)vis_cur == target) {
				tv->selected_node = (int32_t)tv->nodes[i].id;
				dirty_all(ctx);
				treeview_clamp_scroll(ctx, ctx->focus_id);
				ev->type = MKGUI_EVENT_TREEVIEW_SELECT;
				ev->id = ctx->focus_id;
				ev->value = tv->selected_node;
				return 1;
			}
			++vis_cur;
		}
		break;
	}

	case MKGUI_KEY_LEFT: {
		for(uint32_t i = 0; i < tv->node_count; ++i) {
			if((int32_t)tv->nodes[i].id == tv->selected_node) {
				if(tv->nodes[i].has_children && tv->nodes[i].expanded) {
					tv->nodes[i].expanded = 0;
					dirty_all(ctx);
					ev->type = MKGUI_EVENT_TREEVIEW_COLLAPSE;
					ev->id = ctx->focus_id;
					ev->value = tv->selected_node;
					return 1;

				} else if(tv->nodes[i].parent_node != 0) {
					tv->selected_node = (int32_t)tv->nodes[i].parent_node;
					dirty_all(ctx);
					treeview_clamp_scroll(ctx, ctx->focus_id);
					ev->type = MKGUI_EVENT_TREEVIEW_SELECT;
					ev->id = ctx->focus_id;
					ev->value = tv->selected_node;
					return 1;
				}
				break;
			}
		}
		break;
	}

	default: {
		break;
	}
	}

	return 0;
}

// [=]===^=[ mkgui_treeview_setup ]===============================[=]
MKGUI_API void mkgui_treeview_setup(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK(ctx);
	struct mkgui_treeview_data *tv = find_treeview_data(ctx, id);
	if(tv) {
		tv->node_count = 0;
		tv->selected_node = -1;
		tv->scroll_y = 0;
		if(tv->nodes) {
			free(tv->nodes);
		}
		tv->nodes = (struct mkgui_treeview_node *)calloc(256, sizeof(struct mkgui_treeview_node));
		tv->node_cap = 256;
		dirty_all(ctx);
	}
}

// [=]===^=[ mkgui_treeview_add ]=================================[=]
MKGUI_API uint32_t mkgui_treeview_add(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id, uint32_t parent_node, const char *label) {
	MKGUI_CHECK_VAL(ctx, 0);
	if(!label) {
		label = "";
	}
	struct mkgui_treeview_data *tv = find_treeview_data(ctx, widget_id);
	if(!tv || !tv->nodes) {
		return 0;
	}
	if(tv->node_count >= tv->node_cap) {
		uint32_t new_cap = tv->node_cap * 2;
		struct mkgui_treeview_node *new_nodes = (struct mkgui_treeview_node *)realloc(tv->nodes, new_cap * sizeof(struct mkgui_treeview_node));
		if(!new_nodes) {
			return 0;
		}
		tv->nodes = new_nodes;
		tv->node_cap = new_cap;
	}

	struct mkgui_treeview_node *n = &tv->nodes[tv->node_count++];
	memset(n, 0, sizeof(*n));
	n->id = node_id;
	n->parent_node = parent_node;
	n->icon_idx = -1;

	size_t slen = strlen(label);
	if(slen >= MKGUI_MAX_TEXT) {
		slen = MKGUI_MAX_TEXT - 1;
	}
	memcpy(n->label, label, slen);
	n->label[slen] = '\0';

	if(parent_node != 0) {
		for(uint32_t i = 0; i < tv->node_count - 1; ++i) {
			if(tv->nodes[i].id == parent_node) {
				tv->nodes[i].has_children = 1;
				break;
			}
		}
	}
	tv->order_dirty = 1;

	dirty_all(ctx);
	return node_id;
}

// [=]===^=[ mkgui_treeview_get_selected ]========================[=]
MKGUI_API int32_t mkgui_treeview_get_selected(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, -1);
	struct mkgui_treeview_data *tv = find_treeview_data(ctx, id);
	return tv ? tv->selected_node : -1;
}

// [=]===^=[ mkgui_treeview_select ]==============================[=]
MKGUI_API void mkgui_treeview_select(struct mkgui_ctx *ctx, uint32_t widget_id, int32_t node_id) {
	MKGUI_CHECK(ctx);
	struct mkgui_treeview_data *tv = find_treeview_data(ctx, widget_id);
	if(!tv) {
		return;
	}
	tv->selected_node = node_id;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_treeview_remove ]================================[=]
MKGUI_API void mkgui_treeview_remove(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id) {
	MKGUI_CHECK(ctx);
	struct mkgui_treeview_data *tv = find_treeview_data(ctx, widget_id);
	if(!tv) {
		return;
	}
	uint32_t stack[256];
	uint32_t stack_count = 0;
	for(uint32_t i = 0; i < tv->node_count; ++i) {
		if(tv->nodes[i].id == node_id) {
			stack[stack_count++] = i;
			break;
		}
	}
	uint32_t mark[4096];
	uint32_t mark_count = 0;
	while(stack_count > 0) {
		uint32_t idx = stack[--stack_count];
		if(mark_count < 4096) {
			mark[mark_count++] = idx;
		}
		for(uint32_t i = 0; i < tv->node_count; ++i) {
			if(tv->nodes[i].parent_node == tv->nodes[idx].id && stack_count < 256) {
				stack[stack_count++] = i;
			}
		}
	}
	for(uint32_t i = mark_count; i > 0; --i) {
		uint32_t idx = mark[i - 1];
		if(idx < tv->node_count - 1) {
			memmove(&tv->nodes[idx], &tv->nodes[idx + 1], (tv->node_count - idx - 1) * sizeof(struct mkgui_treeview_node));
		}
		--tv->node_count;
		for(uint32_t j = i; j < mark_count; ++j) {
			if(mark[j] > idx) {
				--mark[j];
			}
		}
	}
	if(tv->selected_node == (int32_t)node_id) {
		tv->selected_node = -1;
	}
	tv_idx_build(tv);
	dirty_all(ctx);
}

// [=]===^=[ mkgui_treeview_clear ]=================================[=]
MKGUI_API void mkgui_treeview_clear(struct mkgui_ctx *ctx, uint32_t widget_id) {
	MKGUI_CHECK(ctx);
	struct mkgui_treeview_data *tv = find_treeview_data(ctx, widget_id);
	if(!tv) {
		return;
	}
	tv->node_count = 0;
	tv->selected_node = -1;
	tv->scroll_y = 0;
	dirty_all(ctx);
}

// [=]===^=[ mkgui_treeview_set_label ]=============================[=]
MKGUI_API void mkgui_treeview_set_label(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id, const char *label) {
	MKGUI_CHECK(ctx);
	if(!label) {
		label = "";
	}
	struct mkgui_treeview_data *tv = find_treeview_data(ctx, widget_id);
	if(!tv) {
		return;
	}
	for(uint32_t i = 0; i < tv->node_count; ++i) {
		if(tv->nodes[i].id == node_id) {
			strncpy(tv->nodes[i].label, label, MKGUI_MAX_TEXT - 1);
			tv->nodes[i].label[MKGUI_MAX_TEXT - 1] = '\0';
			dirty_all(ctx);
			return;
		}
	}
}

// [=]===^=[ mkgui_treeview_get_label ]=============================[=]
MKGUI_API char *mkgui_treeview_get_label(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id) {
	MKGUI_CHECK_VAL(ctx, "");
	struct mkgui_treeview_data *tv = find_treeview_data(ctx, widget_id);
	if(!tv) {
		return "";
	}
	for(uint32_t i = 0; i < tv->node_count; ++i) {
		if(tv->nodes[i].id == node_id) {
			return tv->nodes[i].label;
		}
	}
	return "";
}

// [=]===^=[ mkgui_treeview_expand ]================================[=]
MKGUI_API void mkgui_treeview_expand(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id) {
	MKGUI_CHECK(ctx);
	struct mkgui_treeview_data *tv = find_treeview_data(ctx, widget_id);
	if(!tv) {
		return;
	}
	for(uint32_t i = 0; i < tv->node_count; ++i) {
		if(tv->nodes[i].id == node_id) {
			tv->nodes[i].expanded = 1;
			dirty_all(ctx);
			return;
		}
	}
}

// [=]===^=[ mkgui_treeview_collapse ]==============================[=]
MKGUI_API void mkgui_treeview_collapse(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id) {
	MKGUI_CHECK(ctx);
	struct mkgui_treeview_data *tv = find_treeview_data(ctx, widget_id);
	if(!tv) {
		return;
	}
	for(uint32_t i = 0; i < tv->node_count; ++i) {
		if(tv->nodes[i].id == node_id) {
			tv->nodes[i].expanded = 0;
			dirty_all(ctx);
			return;
		}
	}
}

// [=]===^=[ mkgui_treeview_is_expanded ]============================[=]
MKGUI_API uint32_t mkgui_treeview_is_expanded(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_treeview_data *tv = find_treeview_data(ctx, widget_id);
	if(!tv) {
		return 0;
	}
	for(uint32_t i = 0; i < tv->node_count; ++i) {
		if(tv->nodes[i].id == node_id) {
			return tv->nodes[i].expanded;
		}
	}
	return 0;
}

// [=]===^=[ mkgui_treeview_expand_all ]=============================[=]
MKGUI_API void mkgui_treeview_expand_all(struct mkgui_ctx *ctx, uint32_t widget_id) {
	MKGUI_CHECK(ctx);
	struct mkgui_treeview_data *tv = find_treeview_data(ctx, widget_id);
	if(!tv) {
		return;
	}
	for(uint32_t i = 0; i < tv->node_count; ++i) {
		if(tv->nodes[i].has_children) {
			tv->nodes[i].expanded = 1;
		}
	}
	dirty_all(ctx);
}

// [=]===^=[ mkgui_treeview_collapse_all ]============================[=]
MKGUI_API void mkgui_treeview_collapse_all(struct mkgui_ctx *ctx, uint32_t widget_id) {
	MKGUI_CHECK(ctx);
	struct mkgui_treeview_data *tv = find_treeview_data(ctx, widget_id);
	if(!tv) {
		return;
	}
	for(uint32_t i = 0; i < tv->node_count; ++i) {
		tv->nodes[i].expanded = 0;
	}
	dirty_all(ctx);
}

// [=]===^=[ mkgui_treeview_get_parent ]=============================[=]
MKGUI_API uint32_t mkgui_treeview_get_parent(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_treeview_data *tv = find_treeview_data(ctx, widget_id);
	if(!tv) {
		return 0;
	}
	for(uint32_t i = 0; i < tv->node_count; ++i) {
		if(tv->nodes[i].id == node_id) {
			return tv->nodes[i].parent_node;
		}
	}
	return 0;
}

// [=]===^=[ mkgui_treeview_node_count ]=============================[=]
MKGUI_API uint32_t mkgui_treeview_node_count(struct mkgui_ctx *ctx, uint32_t widget_id) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_treeview_data *tv = find_treeview_data(ctx, widget_id);
	return tv ? tv->node_count : 0;
}

// [=]===^=[ mkgui_treeview_scroll_to ]==============================[=]
MKGUI_API void mkgui_treeview_scroll_to(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id) {
	MKGUI_CHECK(ctx);
	struct mkgui_treeview_data *tv = find_treeview_data(ctx, widget_id);
	if(!tv) {
		return;
	}
	int32_t widx = find_widget_idx(ctx, widget_id);
	if(widx < 0) {
		return;
	}
	uint32_t vis_row = 0;
	for(uint32_t i = 0; i < tv->node_count; ++i) {
		if(!treeview_node_visible(tv, i)) {
			continue;
		}
		if(tv->nodes[i].id == node_id) {
			int32_t rh = ctx->rects[widx].h;
			int32_t node_y = (int32_t)vis_row * ctx->row_height;
			if(node_y < tv->scroll_y) {
				tv->scroll_y = node_y;
			}
			if(node_y + ctx->row_height > tv->scroll_y + rh) {
				tv->scroll_y = node_y + ctx->row_height - rh;
			}
			dirty_all(ctx);
			return;
		}
		++vis_row;
	}
}
