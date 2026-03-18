// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define MKGUI_TREE_INDENT 18
#define MKGUI_TREE_ARROW_SIZE 6

// [=]===^=[ treeview_node_depth ]================================[=]
static uint32_t treeview_node_depth(struct mkgui_treeview_data *tv, uint32_t node_idx) {
	uint32_t depth = 0;
	uint32_t parent = tv->nodes[node_idx].parent_node;
	while(parent != 0) {
		for(uint32_t i = 0; i < tv->node_count; ++i) {
			if(tv->nodes[i].id == parent) {
				parent = tv->nodes[i].parent_node;
				++depth;
				break;
			}
		}
	}
	return depth;
}

// [=]===^=[ treeview_node_visible ]==============================[=]
static uint32_t treeview_node_visible(struct mkgui_treeview_data *tv, uint32_t node_idx) {
	uint32_t parent = tv->nodes[node_idx].parent_node;
	while(parent != 0) {
		for(uint32_t i = 0; i < tv->node_count; ++i) {
			if(tv->nodes[i].id == parent) {
				if(!tv->nodes[i].expanded) {
					return 0;
				}
				parent = tv->nodes[i].parent_node;
				break;
			}
		}
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
	draw_patch(ctx, MKGUI_STYLE_SUNKEN, rx, ry, rw, rh, ctx->theme.input_bg, focused ? ctx->theme.splitter : ctx->theme.widget_border);

	struct mkgui_treeview_data *tv = find_treeview_data(ctx, w->id);
	if(!tv) {
		return;
	}

	int32_t clip_top = ry + 1;
	int32_t clip_bottom = ry + rh - 1;
	int32_t draw_y = ry + 1 - tv->scroll_y;

	uint32_t line_active[32] = {0};
	uint32_t line_color = shade_color(ctx->theme.widget_border, 20);

	for(uint32_t i = 0; i < tv->node_count; ++i) {
		if(!treeview_node_visible(tv, i)) {
			continue;
		}

		int32_t row_y = draw_y;
		int32_t row_end = draw_y + MKGUI_ROW_HEIGHT;
		draw_y += MKGUI_ROW_HEIGHT;

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

		int32_t indent = (int32_t)(depth * MKGUI_TREE_INDENT);

		if(depth > 0) {
			for(uint32_t d = 0; d + 1 < depth; ++d) {
				if(line_active[d]) {
					int32_t lx = rx + 4 + (int32_t)(d * MKGUI_TREE_INDENT) + MKGUI_TREE_INDENT / 2;
					draw_vline(ctx->pixels, ctx->win_w, ctx->win_h, lx, row_y, MKGUI_ROW_HEIGHT, line_color);
				}
			}

			int32_t lx = rx + 4 + (int32_t)((depth - 1) * MKGUI_TREE_INDENT) + MKGUI_TREE_INDENT / 2;
			int32_t cy = row_y + MKGUI_ROW_HEIGHT / 2;
			if(is_last) {
				draw_vline(ctx->pixels, ctx->win_w, ctx->win_h, lx, row_y, MKGUI_ROW_HEIGHT / 2 + 1, line_color);
			} else {
				draw_vline(ctx->pixels, ctx->win_w, ctx->win_h, lx, row_y, MKGUI_ROW_HEIGHT, line_color);
			}
			int32_t hlen = tv->nodes[i].has_children ? MKGUI_TREE_INDENT / 2 - 1 : MKGUI_TREE_INDENT + MKGUI_TREE_INDENT / 2 - 1;
			draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, lx + 1, cy, hlen, line_color);
		}

		if((int32_t)tv->nodes[i].id == tv->selected_node) {
			int32_t dy = row_y < clip_top ? clip_top : row_y;
			int32_t de = row_end > clip_bottom ? clip_bottom : row_end;
			draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx + 1, dy, rw - 2, de - dy, ctx->theme.selection);
		}

		if(tv->nodes[i].has_children) {
			int32_t ax = rx + 4 + indent;
			int32_t ay = row_y + MKGUI_ROW_HEIGHT / 2;

			if(ay >= clip_top && ay < clip_bottom) {
				if(tv->nodes[i].expanded) {
					for(uint32_t j = 0; j < MKGUI_TREE_ARROW_SIZE; ++j) {
						draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, ax + (int32_t)j, ay - MKGUI_TREE_ARROW_SIZE / 2 + (int32_t)j, MKGUI_TREE_ARROW_SIZE - (int32_t)j, ctx->theme.text);
					}

				} else {
					for(uint32_t j = 0; j < MKGUI_TREE_ARROW_SIZE; ++j) {
						draw_vline(ctx->pixels, ctx->win_w, ctx->win_h, ax + (int32_t)j, ay - MKGUI_TREE_ARROW_SIZE / 2 + (int32_t)j, MKGUI_TREE_ARROW_SIZE - (int32_t)j * 2, ctx->theme.text);
					}
				}
			}
		}

		int32_t text_x = rx + 4 + indent + MKGUI_TREE_INDENT;
		if(tv->nodes[i].icon_idx >= 0 && tv->nodes[i].icon_idx < (int32_t)icon_count) {
			int32_t ti = tv->nodes[i].icon_idx;
			int32_t iy = row_y + (MKGUI_ROW_HEIGHT - icons[ti].h) / 2;
			draw_icon(ctx->pixels, ctx->win_w, ctx->win_h, &icons[ti], text_x, iy, rx + 1, clip_top, rx + rw - 1, clip_bottom);
			text_x += icons[ti].w + 2;
		}
		int32_t ty = row_y + (MKGUI_ROW_HEIGHT - ctx->font_height) / 2;
		if(ty >= clip_top && ty + ctx->font_height <= clip_bottom) {
			uint32_t tc = ((int32_t)tv->nodes[i].id == tv->selected_node) ? ctx->theme.sel_text : ((w->flags & MKGUI_DISABLED) ? ctx->theme.text_disabled : ctx->theme.text);
			push_text_clip(text_x, ty, tv->nodes[i].label, tc, rx + 1, clip_top, rx + rw - 1, clip_bottom);
		}
	}

	if(tv->drag_active && tv->drag_target >= 0 && (uint32_t)tv->drag_target < tv->node_count) {
		uint32_t depth = treeview_node_depth(tv, (uint32_t)tv->drag_target);
		int32_t indent = (int32_t)(depth * MKGUI_TREE_INDENT) + MKGUI_TREE_INDENT + 4;
		int32_t vis = 0;
		for(uint32_t i = 0; i < (uint32_t)tv->drag_target; ++i) {
			if(treeview_node_visible(tv, i)) {
				++vis;
			}
		}
		if(treeview_node_visible(tv, (uint32_t)tv->drag_target)) {
			int32_t line_y;
			if(tv->drag_pos == 0) {
				line_y = ry + 1 - tv->scroll_y + vis * MKGUI_ROW_HEIGHT;
			} else if(tv->drag_pos == 2) {
				indent = (int32_t)((depth + 1) * MKGUI_TREE_INDENT) + MKGUI_TREE_INDENT + 4;
				line_y = ry + 1 - tv->scroll_y + vis * MKGUI_ROW_HEIGHT + MKGUI_ROW_HEIGHT / 2;
			} else {
				line_y = ry + 1 - tv->scroll_y + (vis + 1) * MKGUI_ROW_HEIGHT;
			}
			if(line_y >= clip_top && line_y < clip_bottom) {
				draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, rx + indent, line_y, rw - indent - 2, ctx->theme.accent);
				draw_hline(ctx->pixels, ctx->win_w, ctx->win_h, rx + indent, line_y - 1, rw - indent - 2, ctx->theme.accent);
				draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, rx + indent - 2, line_y - 2, 5, 5, ctx->theme.accent);
			}
		}
	}
}

// [=]===^=[ treeview_visible_count ]=============================[=]
static uint32_t treeview_visible_count(struct mkgui_treeview_data *tv) {
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
	int32_t content_h = (int32_t)treeview_visible_count(tv) * MKGUI_ROW_HEIGHT;
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

	int32_t ry = ctx->rects[widget_idx].y;
	int32_t local_y = my - ry - 1 + tv->scroll_y;
	int32_t vis_row = local_y / MKGUI_ROW_HEIGHT;

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
	uint32_t depth = treeview_node_depth(tv, (uint32_t)node_idx);
	int32_t rx = ctx->rects[widget_idx].x;
	int32_t arrow_x = rx + 4 + (int32_t)(depth * MKGUI_TREE_INDENT);
	return (mx >= arrow_x && mx < arrow_x + MKGUI_TREE_INDENT);
}

// [=]===^=[ handle_treeview_key ]================================[=]
static uint32_t handle_treeview_key(struct mkgui_ctx *ctx, struct mkgui_event *ev, uint32_t ks) {
	struct mkgui_treeview_data *tv = find_treeview_data(ctx, ctx->focus_id);
	if(!tv || tv->node_count == 0) {
		return 0;
	}

	if(ks == MKGUI_KEY_UP) {
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

	} else if(ks == MKGUI_KEY_DOWN) {
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

	} else if(ks == MKGUI_KEY_RIGHT) {
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

	} else if(ks == MKGUI_KEY_LEFT) {
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
	}

	return 0;
}

// [=]===^=[ mkgui_treeview_setup ]===============================[=]
static void mkgui_treeview_setup(struct mkgui_ctx *ctx, uint32_t id) {
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
static uint32_t mkgui_treeview_add(struct mkgui_ctx *ctx, uint32_t widget_id, uint32_t node_id, uint32_t parent_node, const char *label) {
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
	n->id = node_id;
	n->parent_node = parent_node;
	n->expanded = 0;
	n->has_children = 0;
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

	dirty_all(ctx);
	return node_id;
}

// [=]===^=[ mkgui_treeview_get_selected ]========================[=]
static int32_t mkgui_treeview_get_selected(struct mkgui_ctx *ctx, uint32_t id) {
	struct mkgui_treeview_data *tv = find_treeview_data(ctx, id);
	return tv ? tv->selected_node : -1;
}

// [=]===^=[ mkgui_treeview_select ]==============================[=]
static void mkgui_treeview_select(struct mkgui_ctx *ctx, uint32_t widget_id, int32_t node_id) {
	struct mkgui_treeview_data *tv = find_treeview_data(ctx, widget_id);
	if(!tv) {
		return;
	}
	tv->selected_node = node_id;
	dirty_all(ctx);
}
