// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#include "mkgui.c"
#include <time.h>

enum {
	BM_WINDOW = 0,
	BM_STATUSBAR = 1,
	BM_VBOX = 2,
	BM_FIRST_ROW = 3,
};

#define BM_COLS 5
#define BM_MAX_ROWS ((MKGUI_MAX_WIDGETS - BM_FIRST_ROW) / (BM_COLS + 1))

static double bm_clock_us(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec * 1e-3;
}

int32_t main(int32_t argc, char **argv) {
	uint32_t rows = BM_MAX_ROWS;
	if(argc > 1) {
		int32_t r = atoi(argv[1]);
		if(r > 0 && (uint32_t)r < rows) {
			rows = (uint32_t)r;
		}
	}

	uint32_t widget_count = BM_FIRST_ROW + rows * (BM_COLS + 1);
	if(widget_count > MKGUI_MAX_WIDGETS) {
		widget_count = MKGUI_MAX_WIDGETS;
		rows = (widget_count - BM_FIRST_ROW) / (BM_COLS + 1);
	}

	struct mkgui_widget widgets[MKGUI_MAX_WIDGETS];
	memset(widgets, 0, sizeof(widgets));

	widgets[BM_WINDOW] = (struct mkgui_widget){ MKGUI_WINDOW, BM_WINDOW, "mkgui benchmark", "", 0, 0, 0, 900, 700, 0 };
	widgets[BM_STATUSBAR] = (struct mkgui_widget){ MKGUI_STATUSBAR, BM_STATUSBAR, "", "", BM_WINDOW, 0, 0, 0, 0, 0 };
	widgets[BM_VBOX] = (struct mkgui_widget){ MKGUI_VBOX, BM_VBOX, "", "", BM_WINDOW, 0, 0, 0, 0, MKGUI_ANCHOR_LEFT | MKGUI_ANCHOR_TOP | MKGUI_ANCHOR_RIGHT | MKGUI_ANCHOR_BOTTOM | MKGUI_SCROLL };

	uint32_t idx = BM_FIRST_ROW;
	for(uint32_t r = 0; r < rows; ++r) {
		uint32_t row_id = 1000 + r * 10;
		char label_buf[64];

		widgets[idx] = (struct mkgui_widget){ MKGUI_HBOX, row_id, "", "", BM_VBOX, 0, 0, 0, 24, 0 };
		++idx;

		snprintf(label_buf, sizeof(label_buf), "Row %u", r);
		widgets[idx] = (struct mkgui_widget){ MKGUI_LABEL, row_id + 1, "", "", row_id, 0, 0, 80, 0, 0 };
		snprintf(widgets[idx].label, sizeof(widgets[idx].label), "%s", label_buf);
		++idx;

		widgets[idx] = (struct mkgui_widget){ MKGUI_BUTTON, row_id + 2, "Action", "", row_id, 0, 0, 80, 0, 0 };
		++idx;

		widgets[idx] = (struct mkgui_widget){ MKGUI_CHECKBOX, row_id + 3, "Enable", "", row_id, 0, 0, 80, 0, 0 };
		++idx;

		snprintf(label_buf, sizeof(label_buf), "Status for item %u", r);
		widgets[idx] = (struct mkgui_widget){ MKGUI_LABEL, row_id + 4, "", "", row_id, 0, 0, 0, 0, 0 };
		snprintf(widgets[idx].label, sizeof(widgets[idx].label), "%s", label_buf);
		++idx;

		widgets[idx] = (struct mkgui_widget){ MKGUI_BUTTON, row_id + 5, "Delete", "delete", row_id, 0, 0, 80, 0, 0 };
		++idx;
	}

	printf("benchmark: %u rows, %u widgets (max %u)\n", rows, idx, MKGUI_MAX_WIDGETS);

	struct mkgui_ctx *ctx = mkgui_create(widgets, idx);
	mkgui_set_poll_timeout(ctx, 0);

	struct mkgui_event ev;
	uint32_t running = 1;
	uint32_t frame = 0;
	double total_us = 0.0;
	double worst_us = 0.0;
	double best_us = 1e12;
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
		if(frame_us > worst_us) {
			worst_us = frame_us;
		}
		if(frame_us < best_us) {
			best_us = frame_us;
		}
		++frame;

		if(frame % report_interval == 0) {
			double avg_us = total_us / (double)report_interval;
			double avg_ms = avg_us / 1000.0;
			double fps = avg_us > 0.0 ? 1e6 / avg_us : 0.0;
			double avg_layout = total_layout / (double)report_interval / 1000.0;
			double avg_render = total_render / (double)report_interval / 1000.0;
			double avg_blit = total_blit / (double)report_interval / 1000.0;
			char buf[256];
			snprintf(buf, sizeof(buf), "%u widgets | %.2f ms (%.0f fps) | layout %.2f | render %.2f | blit %.2f",
				idx, avg_ms, fps, avg_layout, avg_render, avg_blit);
			mkgui_statusbar_set(ctx, BM_STATUSBAR, 0, buf);
			printf("%s\n", buf);
			total_us = 0.0;
			worst_us = 0.0;
			best_us = 1e12;
			total_layout = 0.0;
			total_render = 0.0;
			total_blit = 0.0;
		}
		mkgui_wait(ctx);
	}

	mkgui_destroy(ctx);
	return 0;
}
