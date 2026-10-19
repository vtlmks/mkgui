// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT

#ifndef MKGUI_EDITOR_H
#define MKGUI_EDITOR_H

#include "mkgui_internal.h"
#include "mkguiicons.h"
#include "mkguiicons_names.h"

MKGUI_API struct mkgui_editor_api *mkgui_editor_api_v2(void);

static struct mkgui_editor_api *editor_api;

#define accel_format_text(...) (editor_api->accel_format_text)(__VA_ARGS__)
#define blend_pixel(...) (editor_api->blend_pixel)(__VA_ARGS__)
#define dirty_all(...) (editor_api->dirty_all)(__VA_ARGS__)
#define draw_aa_circle_fill(...) (editor_api->draw_aa_circle_fill)(__VA_ARGS__)
#define draw_aa_circle_ring(...) (editor_api->draw_aa_circle_ring)(__VA_ARGS__)
#define draw_aa_line(...) (editor_api->draw_aa_line)(__VA_ARGS__)
#define draw_hline(...) (editor_api->draw_hline)(__VA_ARGS__)
#define draw_icon(...) (editor_api->draw_icon)(__VA_ARGS__)
#define draw_patch(...) (editor_api->draw_patch)(__VA_ARGS__)
#define draw_rect_border(...) (editor_api->draw_rect_border)(__VA_ARGS__)
#define draw_rect_dashed(...) (editor_api->draw_rect_dashed)(__VA_ARGS__)
#define draw_rect_fill(...) (editor_api->draw_rect_fill)(__VA_ARGS__)
#define draw_rounded_rect(...) (editor_api->draw_rounded_rect)(__VA_ARGS__)
#define draw_rounded_rect_fill(...) (editor_api->draw_rounded_rect_fill)(__VA_ARGS__)
#define draw_triangle_aa(...) (editor_api->draw_triangle_aa)(__VA_ARGS__)
#define draw_vline(...) (editor_api->draw_vline)(__VA_ARGS__)
#define find_widget(...) (editor_api->find_widget)(__VA_ARGS__)
#define find_widget_idx(...) (editor_api->find_widget_idx)(__VA_ARGS__)
#define flush_text(...) (editor_api->flush_text)(__VA_ARGS__)
#define icon_resolve(...) (editor_api->icon_resolve)(__VA_ARGS__)
#define lc_build_index(...) (editor_api->lc_build_index)(__VA_ARGS__)
#define lc_layout_node(...) (editor_api->lc_layout_node)(__VA_ARGS__)
#define lc_window_min_size(...) (editor_api->lc_window_min_size)(__VA_ARGS__)
#define push_text(...) (editor_api->push_text)(__VA_ARGS__)
#define push_text_clip(...) (editor_api->push_text_clip)(__VA_ARGS__)
#define render_widget(...) (editor_api->render_widget)(__VA_ARGS__)
#define sc(...) (editor_api->sc)(__VA_ARGS__)
#define slider_draw_wedge_h(...) (editor_api->slider_draw_wedge_h)(__VA_ARGS__)
#define slider_draw_wedge_v(...) (editor_api->slider_draw_wedge_v)(__VA_ARGS__)
#define text_width(...) (editor_api->text_width)(__VA_ARGS__)
#define find_dropdown_data(...) (editor_api->find_dropdown_data)(__VA_ARGS__)
#define find_treeview_data(...) (editor_api->find_treeview_data)(__VA_ARGS__)
#define find_split_data(...) (editor_api->find_split_data)(__VA_ARGS__)
#define find_listv_data(...) (editor_api->find_listv_data)(__VA_ARGS__)
#define icons (*editor_api->icons)
#define render_clip_x1 (*editor_api->render_clip_x1)
#define render_clip_y1 (*editor_api->render_clip_y1)
#define render_clip_x2 (*editor_api->render_clip_x2)
#define render_clip_y2 (*editor_api->render_clip_y2)

#endif
