// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#include "mkgui.c"
#include <time.h>

// ---------------------------------------------------------------------------
// Widget layout mode
// ---------------------------------------------------------------------------

enum {
	BM_WINDOW = 0,
	BM_STATUSBAR = 1,
	BM_VBOX = 2,
	BM_FIRST_ROW = 3,
};

#define BM_COLS 5
#define BM_WIDGET_LIMIT 4096
#define BM_MAX_ROWS ((BM_WIDGET_LIMIT - BM_FIRST_ROW) / (BM_COLS + 1))

// ---------------------------------------------------------------------------
// Render stress mode
// ---------------------------------------------------------------------------

enum {
	ST_WINDOW = 0,
	ST_STATUSBAR,
	ST_HSPLIT,
	ST_LEFT_VBOX,
	ST_GRP_CONTROLS,
	ST_CTL_FORM,
	ST_LBL_R1, ST_RADIO1,
	ST_LBL_R2, ST_RADIO2,
	ST_LBL_R3, ST_RADIO3,
	ST_LBL_CHK1, ST_CHECK1,
	ST_LBL_CHK2, ST_CHECK2,
	ST_LBL_CHK3, ST_CHECK3,
	ST_LBL_PROG, ST_PROGRESS1,
	ST_GRP_BUTTONS,
	ST_BTN_VBOX,
	ST_BTN1, ST_BTN2, ST_BTN3, ST_BTN4, ST_BTN5, ST_BTN6,
	ST_GRP_TREE,
	ST_TREEVIEW,
	ST_RIGHT_VBOX,
	ST_LISTVIEW,
	ST_GRIDVIEW,
	ST_WIDGET_COUNT,
};

#define ST_LV_ROWS    200
#define ST_LV_COLS    8
#define ST_GV_ROWS    200
#define ST_GV_COLS    6
#define ST_TREE_NODES 50

// [=]===^=[ stress_lv_cb ]========================================[=]
static void stress_lv_cb(uint32_t row, uint32_t col, char *buf, uint32_t buf_size, void *userdata) {
	(void)userdata;
	switch(col) {
		case 0: { snprintf(buf, buf_size, "Item-%u", row); } break;
		case 1: { snprintf(buf, buf_size, "Description for row %u", row); } break;
		case 2: { snprintf(buf, buf_size, "%u.%02u", row * 13 % 1000, row * 7 % 100); } break;
		case 3: { snprintf(buf, buf_size, "Category %c", 'A' + (char)(row % 26)); } break;
		case 4: { snprintf(buf, buf_size, "%u", row * 42 + 100); } break;
		case 5: { snprintf(buf, buf_size, "user%u@host.net", row % 50); } break;
		case 6: { snprintf(buf, buf_size, "2026-%02u-%02u", row % 12 + 1, row % 28 + 1); } break;
		case 7: { snprintf(buf, buf_size, "%s", (row % 3 == 0) ? "Active" : (row % 3 == 1) ? "Pending" : "Done"); } break;
		default: { buf[0] = '\0'; } break;
	}
}

// [=]===^=[ stress_gv_cb ]========================================[=]
static void stress_gv_cb(uint32_t row, uint32_t col, char *buf, uint32_t buf_size, void *userdata) {
	(void)userdata;
	switch(col) {
		case 0: { snprintf(buf, buf_size, "Cell %u:%u", row, col); } break;
		case 1: { snprintf(buf, buf_size, "%u", row * col + 1); } break;
		default: { snprintf(buf, buf_size, "V%u", row + col); } break;
	}
}

// ---------------------------------------------------------------------------
// Common
// ---------------------------------------------------------------------------

static double bm_clock_us(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec * 1e-3;
}

// [=]===^=[ run_widget_mode ]=====================================[=]
static void run_widget_mode(uint32_t rows) {
	uint32_t widget_count = BM_FIRST_ROW + rows * (BM_COLS + 1);
	if(widget_count > BM_WIDGET_LIMIT) {
		widget_count = BM_WIDGET_LIMIT;
		rows = (widget_count - BM_FIRST_ROW) / (BM_COLS + 1);
	}

	struct mkgui_widget widgets[BM_WIDGET_LIMIT];
	memset(widgets, 0, sizeof(widgets));

	widgets[BM_WINDOW] = (struct mkgui_widget){ MKGUI_WINDOW, BM_WINDOW, "mkgui benchmark", "", 0, 0, 0, 900, 700, 0, 0 };
	widgets[BM_STATUSBAR] = (struct mkgui_widget){ MKGUI_STATUSBAR, BM_STATUSBAR, "", "", BM_WINDOW, 0, 0, 0, 0, 0, 0 };
	widgets[BM_VBOX] = (struct mkgui_widget){ MKGUI_VBOX, BM_VBOX, "", "", BM_WINDOW, 0, 0, 0, 0, MKGUI_ANCHOR_LEFT | MKGUI_ANCHOR_TOP | MKGUI_ANCHOR_RIGHT | MKGUI_ANCHOR_BOTTOM | MKGUI_SCROLL, 0 };

	uint32_t idx = BM_FIRST_ROW;
	for(uint32_t r = 0; r < rows; ++r) {
		uint32_t row_id = 1000 + r * 10;
		char label_buf[64];

		widgets[idx] = (struct mkgui_widget){ MKGUI_HBOX, row_id, "", "", BM_VBOX, 0, 0, 0, 24, MKGUI_FIXED, 0 };
		++idx;

		snprintf(label_buf, sizeof(label_buf), "Row %u", r);
		widgets[idx] = (struct mkgui_widget){ MKGUI_LABEL, row_id + 1, "", "", row_id, 0, 0, 80, 0, MKGUI_FIXED, 0 };
		snprintf(widgets[idx].label, sizeof(widgets[idx].label), "%s", label_buf);
		++idx;

		widgets[idx] = (struct mkgui_widget){ MKGUI_BUTTON, row_id + 2, "Action", "", row_id, 0, 0, 80, 0, MKGUI_FIXED, 0 };
		++idx;

		widgets[idx] = (struct mkgui_widget){ MKGUI_CHECKBOX, row_id + 3, "Enable", "", row_id, 0, 0, 80, 0, MKGUI_FIXED, 0 };
		++idx;

		snprintf(label_buf, sizeof(label_buf), "Status for item %u", r);
		widgets[idx] = (struct mkgui_widget){ MKGUI_LABEL, row_id + 4, "", "", row_id, 0, 0, 0, 0, 0, 1 };
		snprintf(widgets[idx].label, sizeof(widgets[idx].label), "%s", label_buf);
		++idx;

		widgets[idx] = (struct mkgui_widget){ MKGUI_BUTTON, row_id + 5, "Delete", "delete", row_id, 0, 0, 80, 0, MKGUI_FIXED, 0 };
		++idx;
	}

	printf("widget mode: %u rows, %u widgets (max %u)\n", rows, idx, BM_WIDGET_LIMIT);

	struct mkgui_ctx *ctx = mkgui_create(widgets, idx);
	mkgui_set_poll_timeout(ctx, 0);

	struct mkgui_event ev;
	uint32_t running = 1;
	uint32_t frame = 0;
	double total_us = 0.0;
	double total_layout = 0.0;
	double total_render = 0.0;
	double total_blit = 0.0;
	uint32_t report_interval = 60;

	while(running) {
		double t0 = bm_clock_us();
		dirty_all(ctx);

		while(mkgui_poll(ctx, &ev)) {
			if(ev.type == MKGUI_EVENT_CLOSE) {
				running = 0;
			}
		}

		double t1 = bm_clock_us();
		double frame_us = t1 - t0;
		total_us += frame_us;
		total_layout += ctx->perf_layout_us;
		total_render += ctx->perf_render_us;
		total_blit += ctx->perf_blit_us;
		++frame;

		if(frame % report_interval == 0) {
			double avg_us = total_us / (double)report_interval;
			double avg_ms = avg_us / 1000.0;
			double fps = avg_us > 0.0 ? 1e6 / avg_us : 0.0;
			double avg_layout = total_layout / (double)report_interval / 1000.0;
			double avg_render = total_render / (double)report_interval / 1000.0;
			double avg_blit = total_blit / (double)report_interval / 1000.0;
			char buf[256];
			snprintf(buf, sizeof(buf), "%u widgets | %.3f ms (%.0f fps) | layout %.4f | render %.3f | blit %.3f",
				idx, avg_ms, fps, avg_layout, avg_render, avg_blit);
			mkgui_statusbar_set(ctx, BM_STATUSBAR, 0, buf);
			printf("%s\n", buf);
			total_us = 0.0;
			total_layout = 0.0;
			total_render = 0.0;
			total_blit = 0.0;
		}
		mkgui_wait(ctx);
	}

	mkgui_destroy(ctx);
}

// [=]===^=[ run_stress_mode ]=====================================[=]
static void run_stress_mode(void) {
	struct mkgui_widget widgets[ST_WIDGET_COUNT];
	memset(widgets, 0, sizeof(widgets));

	uint32_t a4 = MKGUI_ANCHOR_LEFT | MKGUI_ANCHOR_TOP | MKGUI_ANCHOR_RIGHT | MKGUI_ANCHOR_BOTTOM;

	widgets[ST_WINDOW]     = (struct mkgui_widget){ MKGUI_WINDOW,    ST_WINDOW,     "mkgui stress",  "",                  0,              0, 0, 1200, 800, 0, 0 };
	widgets[ST_STATUSBAR]  = (struct mkgui_widget){ MKGUI_STATUSBAR, ST_STATUSBAR,  "",              "",                  ST_WINDOW,      0, 0, 0, 0, 0, 0 };
	widgets[ST_HSPLIT]     = (struct mkgui_widget){ MKGUI_VSPLIT,    ST_HSPLIT,      "",              "",                  ST_WINDOW,      0, 0, 0, 0, a4, 0 };
	widgets[ST_LEFT_VBOX]  = (struct mkgui_widget){ MKGUI_VBOX,      ST_LEFT_VBOX,  "",              "",                  ST_HSPLIT,      0, 0, 0, 0, MKGUI_REGION_LEFT, 0 };

	widgets[ST_GRP_CONTROLS] = (struct mkgui_widget){ MKGUI_GROUP,   ST_GRP_CONTROLS, "Controls",    "",                  ST_LEFT_VBOX,   0, 0, 0, 210, MKGUI_FIXED, 0 };
	widgets[ST_CTL_FORM]   = (struct mkgui_widget){ MKGUI_FORM,      ST_CTL_FORM,   "",              "",                  ST_GRP_CONTROLS, 0, 0, 0, 0, a4, 0 };
	widgets[ST_LBL_R1]     = (struct mkgui_widget){ MKGUI_LABEL,     ST_LBL_R1,     "Option A:",     "",                  ST_CTL_FORM,    0, 0, 0, 0, 0, 0 };
	widgets[ST_RADIO1]     = (struct mkgui_widget){ MKGUI_RADIO,     ST_RADIO1,     "Enabled",       "",                  ST_CTL_FORM,    0, 0, 0, 0, MKGUI_CHECKED, 0 };
	widgets[ST_LBL_R2]     = (struct mkgui_widget){ MKGUI_LABEL,     ST_LBL_R2,     "Option B:",     "",                  ST_CTL_FORM,    0, 0, 0, 0, 0, 0 };
	widgets[ST_RADIO2]     = (struct mkgui_widget){ MKGUI_RADIO,     ST_RADIO2,     "Disabled",      "",                  ST_CTL_FORM,    0, 0, 0, 0, 0, 0 };
	widgets[ST_LBL_R3]     = (struct mkgui_widget){ MKGUI_LABEL,     ST_LBL_R3,     "Option C:",     "",                  ST_CTL_FORM,    0, 0, 0, 0, 0, 0 };
	widgets[ST_RADIO3]     = (struct mkgui_widget){ MKGUI_RADIO,     ST_RADIO3,     "Default",       "",                  ST_CTL_FORM,    0, 0, 0, 0, 0, 0 };
	widgets[ST_LBL_CHK1]   = (struct mkgui_widget){ MKGUI_LABEL,     ST_LBL_CHK1,   "Logs:",         "",                  ST_CTL_FORM,    0, 0, 0, 0, 0, 0 };
	widgets[ST_CHECK1]     = (struct mkgui_widget){ MKGUI_CHECKBOX,  ST_CHECK1,     "Enable logs",   "",                  ST_CTL_FORM,    0, 0, 0, 0, MKGUI_CHECKED, 0 };
	widgets[ST_LBL_CHK2]   = (struct mkgui_widget){ MKGUI_LABEL,     ST_LBL_CHK2,   "Refresh:",      "",                  ST_CTL_FORM,    0, 0, 0, 0, 0, 0 };
	widgets[ST_CHECK2]     = (struct mkgui_widget){ MKGUI_CHECKBOX,  ST_CHECK2,     "Auto refresh",  "",                  ST_CTL_FORM,    0, 0, 0, 0, MKGUI_CHECKED, 0 };
	widgets[ST_LBL_CHK3]   = (struct mkgui_widget){ MKGUI_LABEL,     ST_LBL_CHK3,   "Hidden:",       "",                  ST_CTL_FORM,    0, 0, 0, 0, 0, 0 };
	widgets[ST_CHECK3]     = (struct mkgui_widget){ MKGUI_CHECKBOX,  ST_CHECK3,     "Show hidden",   "",                  ST_CTL_FORM,    0, 0, 0, 0, 0, 0 };
	widgets[ST_LBL_PROG]   = (struct mkgui_widget){ MKGUI_LABEL,     ST_LBL_PROG,   "Progress:",     "",                  ST_CTL_FORM,    0, 0, 0, 0, 0, 0 };
	widgets[ST_PROGRESS1]  = (struct mkgui_widget){ MKGUI_PROGRESS,  ST_PROGRESS1,  "",              "",                  ST_CTL_FORM,    0, 0, 0, 0, 0, 0 };

	widgets[ST_GRP_BUTTONS] = (struct mkgui_widget){ MKGUI_GROUP,    ST_GRP_BUTTONS, "Actions",      "",                  ST_LEFT_VBOX,   0, 0, 0, 190, MKGUI_FIXED, 0 };
	widgets[ST_BTN_VBOX]   = (struct mkgui_widget){ MKGUI_VBOX,      ST_BTN_VBOX,   "",              "",                  ST_GRP_BUTTONS, 0, 0, 0, 0, a4, 0 };
	widgets[ST_BTN1]       = (struct mkgui_widget){ MKGUI_BUTTON,    ST_BTN1,       "New",           "file-plus",         ST_BTN_VBOX,    0, 0, 0, 0, 0, 0 };
	widgets[ST_BTN2]       = (struct mkgui_widget){ MKGUI_BUTTON,    ST_BTN2,       "Open",          "folder-open",       ST_BTN_VBOX,    0, 0, 0, 0, 0, 0 };
	widgets[ST_BTN3]       = (struct mkgui_widget){ MKGUI_BUTTON,    ST_BTN3,       "Save",          "content-save",      ST_BTN_VBOX,    0, 0, 0, 0, 0, 0 };
	widgets[ST_BTN4]       = (struct mkgui_widget){ MKGUI_BUTTON,    ST_BTN4,       "Delete",        "delete",            ST_BTN_VBOX,    0, 0, 0, 0, 0, 0 };
	widgets[ST_BTN5]       = (struct mkgui_widget){ MKGUI_BUTTON,    ST_BTN5,       "Refresh",       "refresh",           ST_BTN_VBOX,    0, 0, 0, 0, 0, 0 };
	widgets[ST_BTN6]       = (struct mkgui_widget){ MKGUI_BUTTON,    ST_BTN6,       "Settings",      "cog",              ST_BTN_VBOX,    0, 0, 0, 0, 0, 0 };

	widgets[ST_GRP_TREE]   = (struct mkgui_widget){ MKGUI_GROUP,     ST_GRP_TREE,   "Tree",          "",                  ST_LEFT_VBOX,   0, 0, 0, 0, 0, 1 };
	widgets[ST_TREEVIEW]   = (struct mkgui_widget){ MKGUI_TREEVIEW,  ST_TREEVIEW,   "",              "",                  ST_GRP_TREE,    0, 0, 0, 0, a4, 0 };

	widgets[ST_RIGHT_VBOX] = (struct mkgui_widget){ MKGUI_VBOX,      ST_RIGHT_VBOX, "",              "",                  ST_HSPLIT,      0, 0, 0, 0, MKGUI_REGION_RIGHT, 0 };
	widgets[ST_LISTVIEW]   = (struct mkgui_widget){ MKGUI_LISTVIEW,  ST_LISTVIEW,   "",              "",                  ST_RIGHT_VBOX,  0, 0, 0, 0, 0, 2 };
	widgets[ST_GRIDVIEW]   = (struct mkgui_widget){ MKGUI_GRIDVIEW,  ST_GRIDVIEW,   "",              "",                  ST_RIGHT_VBOX,  0, 0, 0, 0, 0, 1 };

	printf("stress mode: %u widgets, %u lv_rows x %u cols, %u gv_rows x %u cols, %u tree nodes\n",
		ST_WIDGET_COUNT, ST_LV_ROWS, ST_LV_COLS, ST_GV_ROWS, ST_GV_COLS, ST_TREE_NODES);

	struct mkgui_ctx *ctx = mkgui_create(widgets, ST_WIDGET_COUNT);
	mkgui_set_poll_timeout(ctx, 0);

	struct mkgui_column lv_cols[ST_LV_COLS] = {
		{ "Name",        120, MKGUI_CELL_TEXT },
		{ "Description", 180, MKGUI_CELL_TEXT },
		{ "Value",        80, MKGUI_CELL_TEXT },
		{ "Category",    100, MKGUI_CELL_TEXT },
		{ "Count",        70, MKGUI_CELL_TEXT },
		{ "Owner",       140, MKGUI_CELL_TEXT },
		{ "Date",        100, MKGUI_CELL_DATE },
		{ "Status",       80, MKGUI_CELL_TEXT },
	};
	mkgui_listview_setup(ctx, ST_LISTVIEW, ST_LV_ROWS, ST_LV_COLS, lv_cols, stress_lv_cb, NULL);

	struct mkgui_grid_column gv_cols[ST_GV_COLS] = {
		{ "Name",   140, MKGUI_GRID_TEXT },
		{ "Value",   80, MKGUI_GRID_TEXT },
		{ "Enable", 60, MKGUI_GRID_CHECK },
		{ "Flag",   60, MKGUI_GRID_CHECK },
		{ "Extra",  100, MKGUI_GRID_CHECK_TEXT },
		{ "Notes",  120, MKGUI_GRID_TEXT },
	};
	mkgui_gridview_setup(ctx, ST_GRIDVIEW, ST_GV_ROWS, ST_GV_COLS, gv_cols, stress_gv_cb, NULL);

	mkgui_treeview_setup(ctx, ST_TREEVIEW);
	{
		uint32_t nid = 1;
		uint32_t roots = 5;
		uint32_t children_per = (ST_TREE_NODES - roots) / roots;
		for(uint32_t r = 0; r < roots; ++r) {
			uint32_t root_id = nid++;
			char label[64];
			snprintf(label, sizeof(label), "Category %u", r + 1);
			mkgui_treeview_add(ctx, ST_TREEVIEW, root_id, 0, label);
			mkgui_set_treenode_icon(ctx, ST_TREEVIEW, root_id, "folder");
			for(uint32_t c = 0; c < children_per && nid <= ST_TREE_NODES; ++c) {
				uint32_t child_id = nid++;
				snprintf(label, sizeof(label), "Item %u-%u", r + 1, c + 1);
				mkgui_treeview_add(ctx, ST_TREEVIEW, child_id, root_id, label);
				mkgui_set_treenode_icon(ctx, ST_TREEVIEW, child_id, "file");
			}
		}
		struct mkgui_treeview_data *tv = find_treeview_data(ctx, ST_TREEVIEW);
		if(tv) {
			for(uint32_t i = 0; i < tv->node_count; ++i) {
				if(tv->nodes[i].parent_node == 0) {
					tv->nodes[i].expanded = 1;
				}
			}
		}
	}

	struct mkgui_progress_data *pd = find_progress_data(ctx, ST_PROGRESS1);
	if(pd) {
		pd->value = 65;
		pd->max_val = 100;
	}

	struct mkgui_split_data *sd = find_split_data(ctx, ST_HSPLIT);
	if(sd) {
		sd->ratio = 0.22f;
	}

	struct mkgui_event ev;
	uint32_t running = 1;
	uint32_t frame = 0;
	double total_us = 0.0;
	double total_layout = 0.0;
	double total_render = 0.0;
	double total_blit = 0.0;
	uint32_t report_interval = 60;

	while(running) {
		double t0 = bm_clock_us();
		dirty_all(ctx);

		while(mkgui_poll(ctx, &ev)) {
			if(ev.type == MKGUI_EVENT_CLOSE) {
				running = 0;
			}
		}

		double t1 = bm_clock_us();
		double frame_us = t1 - t0;
		total_us += frame_us;
		total_layout += ctx->perf_layout_us;
		total_render += ctx->perf_render_us;
		total_blit += ctx->perf_blit_us;
		++frame;

		if(frame % report_interval == 0) {
			double avg_us = total_us / (double)report_interval;
			double avg_ms = avg_us / 1000.0;
			double fps = avg_us > 0.0 ? 1e6 / avg_us : 0.0;
			double avg_layout = total_layout / (double)report_interval / 1000.0;
			double avg_render = total_render / (double)report_interval / 1000.0;
			double avg_blit = total_blit / (double)report_interval / 1000.0;
			char buf[256];
			snprintf(buf, sizeof(buf), "stress | %.3f ms (%.0f fps) | layout %.4f | render %.3f | blit %.3f",
				avg_ms, fps, avg_layout, avg_render, avg_blit);
			mkgui_statusbar_set(ctx, ST_STATUSBAR, 0, buf);
			printf("%s\n", buf);
			total_us = 0.0;
			total_layout = 0.0;
			total_render = 0.0;
			total_blit = 0.0;
		}
		mkgui_wait(ctx);
	}

	mkgui_destroy(ctx);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int32_t main(int32_t argc, char **argv) {
	if(argc > 1 && strcmp(argv[1], "stress") == 0) {
		run_stress_mode();
		return 0;
	}

	uint32_t rows = BM_MAX_ROWS;
	if(argc > 1) {
		int32_t r = atoi(argv[1]);
		if(r > 0 && (uint32_t)r < rows) {
			rows = (uint32_t)r;
		}
	}

	run_widget_mode(rows);
	return 0;
}
