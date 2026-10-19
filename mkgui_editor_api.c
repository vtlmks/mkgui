// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT

static struct mkgui_editor_api editor_api = {
	.accel_format_text = accel_format_text,
	.blend_pixel = blend_pixel,
	.dirty_all = dirty_all,
	.draw_aa_circle_fill = draw_aa_circle_fill,
	.draw_aa_circle_ring = draw_aa_circle_ring,
	.draw_aa_line = draw_aa_line,
	.draw_hline = draw_hline,
	.draw_icon = draw_icon,
	.draw_patch = draw_patch,
	.draw_rect_border = draw_rect_border,
	.draw_rect_dashed = draw_rect_dashed,
	.draw_rect_fill = draw_rect_fill,
	.draw_rounded_rect = draw_rounded_rect,
	.draw_rounded_rect_fill = draw_rounded_rect_fill,
	.draw_triangle_aa = draw_triangle_aa,
	.draw_vline = draw_vline,
	.find_widget = find_widget,
	.find_widget_idx = find_widget_idx,
	.flush_text = flush_text,
	.icon_resolve = icon_resolve,
	.lc_build_index = lc_build_index,
	.lc_layout_node = lc_layout_node,
	.lc_window_min_size = lc_window_min_size,
	.push_text = push_text,
	.push_text_clip = push_text_clip,
	.render_widget = render_widget,
	.sc = sc,
	.slider_draw_wedge_h = slider_draw_wedge_h,
	.slider_draw_wedge_v = slider_draw_wedge_v,
	.text_width = text_width,
	.find_dropdown_data = find_dropdown_data,
	.find_treeview_data = find_treeview_data,
	.find_split_data = find_split_data,
	.find_listv_data = find_listv_data,
	.icons = &icons,
	.render_clip_x1 = &render_clip_x1,
	.render_clip_y1 = &render_clip_y1,
	.render_clip_x2 = &render_clip_x2,
	.render_clip_y2 = &render_clip_y2,
};

// [=]===^=[ mkgui_editor_api_v2 ]===============================================================[=]
MKGUI_API struct mkgui_editor_api *mkgui_editor_api_v2(void) {
	return &editor_api;

}
