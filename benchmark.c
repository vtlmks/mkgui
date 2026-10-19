// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT

#include "mkgui.c"
#include <errno.h>

enum bench_mode {
	BENCH_LAYOUT,
	BENCH_RENDER,
	BENCH_DIRTY,
	BENCH_FRAME,
	BENCH_PRESENT,
};

enum bench_workload {
	BENCH_FILL,
	BENCH_BUTTONS,
	BENCH_TEXT,
	BENCH_MIXED,
};

struct bench_state {
	struct mkgui_window *win;
	uint32_t mode;
	uint32_t target_idx;
	double acquire_us;
	double layout_us;
	double render_us;
	double blit_us;
};

// [=]===^=[ bench_parse_u32 ]====================================[=]
static uint32_t bench_parse_u32(char *text, uint32_t min_value, uint32_t max_value, uint32_t *out) {
	char *end = NULL;
	errno = 0;
	unsigned long value = strtoul(text, &end, 10);
	if(errno != 0 || !end || *end != '\0' || value < min_value || value > max_value) {
		return 0;
	}
	*out = (uint32_t)value;
	return 1;
}

// [=]===^=[ bench_widgets_create ]===============================[=]
static struct mkgui_widget *bench_widgets_create(uint32_t workload, uint32_t item_count, uint32_t width, uint32_t height, uint32_t *widget_count, uint32_t *target_idx) {
	uint32_t row_count = workload == BENCH_FILL ? 0 : (item_count + 7) / 8;
	size_t total = 1 + (workload == BENCH_FILL ? 0 : 1 + (size_t)row_count + item_count);
	if(total > MKGUI_VM_MAX_WIDGETS) {
		return NULL;
	}
	struct mkgui_widget *widgets = (struct mkgui_widget *)calloc(total, sizeof(struct mkgui_widget));
	if(!widgets) {
		return NULL;
	}
	widgets[0].type = MKGUI_WINDOW;
	widgets[0].id = 1;
	widgets[0].w = (int32_t)width;
	widgets[0].h = (int32_t)height;
	memcpy(widgets[0].label, "mkgui benchmark", 16);
	uint32_t idx = 1;
	if(workload != BENCH_FILL) {
		widgets[idx].type = MKGUI_VBOX;
		widgets[idx].id = idx + 1;
		widgets[idx].parent_id = 1;
		++idx;
		uint32_t item = 0;
		for(uint32_t row = 0; row < row_count; ++row) {
			uint32_t row_id = idx + 1;
			widgets[idx].type = MKGUI_HBOX;
			widgets[idx].id = row_id;
			widgets[idx].parent_id = 2;
			++idx;
			for(uint32_t col = 0; col < 8 && item < item_count; ++col) {
				struct mkgui_widget *widget = &widgets[idx];
				widget->id = idx + 1;
				widget->parent_id = row_id;
				if(workload == BENCH_BUTTONS) {
					widget->type = MKGUI_BUTTON;
				} else if(workload == BENCH_TEXT) {
					widget->type = MKGUI_LABEL;
				} else {
					switch(item % 6) {
						case 0: { widget->type = MKGUI_BUTTON; } break;

						case 1: { widget->type = MKGUI_LABEL; } break;

						case 2: { widget->type = MKGUI_CHECKBOX; } break;

						case 3: { widget->type = MKGUI_PROGRESS; } break;

						case 4: { widget->type = MKGUI_INPUT; } break;

						case 5: { widget->type = MKGUI_SLIDER; } break;
					}
				}
				snprintf(widget->label, sizeof(widget->label), "item %u", item);
				if(item == 0) {
					*target_idx = idx;
				}
				++idx;
				++item;
			}
		}
	}
	*widget_count = idx;
	return widgets;
}

// [=]===^=[ bench_run_iteration ]================================[=]
static void bench_run_iteration(struct bench_state *state) {
	struct mkgui_window *win = state->win;

	switch(state->mode) {
		case BENCH_LAYOUT: {
			layout_widgets(win);

		} break;

		case BENCH_RENDER: {
			win->dirty_full = 1;
			render_widgets(win);
			flush_text(win);
			win->dirty_full = 0;

		} break;

		case BENCH_DIRTY: {
			win->dirty = 0;
			win->dirty_full = 0;
			win->dirty_count = 0;
			dirty_widget(win, state->target_idx);
			render_widgets(win);
			flush_text(win);
			win->dirty = 0;
			win->dirty_count = 0;

		} break;

		case BENCH_FRAME: {
			dirty_all(win);
			mkgui_flush(win);
			state->acquire_us += win->perf_acquire_us;
			state->layout_us += win->perf_layout_us;
			state->render_us += win->perf_render_us;
			state->blit_us += win->perf_blit_us;

		} break;

		case BENCH_PRESENT: {
			mkwin_present(win->mkwin);

		} break;
	}
}

// [=]===^=[ main ]================================================[=]
int main(int argc, char **argv) {
	if(argc != 7) {
		fprintf(stderr, "usage: %s <layout|render|dirty|frame|present> <fill|buttons|text|mixed> <items> <width> <height> <iterations>\n", argv[0]);
		return 1;

	}

	uint32_t mode;

	if(strcmp(argv[1], "layout") == 0) {
		mode = BENCH_LAYOUT;

	} else if(strcmp(argv[1], "render") == 0) {
		mode = BENCH_RENDER;

	} else if(strcmp(argv[1], "dirty") == 0) {
		mode = BENCH_DIRTY;

	} else if(strcmp(argv[1], "frame") == 0) {
		mode = BENCH_FRAME;

	} else if(strcmp(argv[1], "present") == 0) {
		mode = BENCH_PRESENT;

	} else {
		fprintf(stderr, "invalid mode: %s\n", argv[1]);
		return 1;

	}

	uint32_t workload;

	if(strcmp(argv[2], "fill") == 0) {
		workload = BENCH_FILL;

	} else if(strcmp(argv[2], "buttons") == 0) {
		workload = BENCH_BUTTONS;

	} else if(strcmp(argv[2], "text") == 0) {
		workload = BENCH_TEXT;

	} else if(strcmp(argv[2], "mixed") == 0) {
		workload = BENCH_MIXED;

	} else {
		fprintf(stderr, "invalid workload: %s\n", argv[2]);
		return 1;

	}

	uint32_t item_count;
	uint32_t width;
	uint32_t height;
	uint32_t iterations;

	if(!bench_parse_u32(argv[3], 0, MKGUI_VM_MAX_WIDGETS - 1, &item_count) || !bench_parse_u32(argv[4], 64, 16384, &width) || !bench_parse_u32(argv[5], 64, 16384, &height) || !bench_parse_u32(argv[6], 1, UINT32_MAX, &iterations)) {
		fprintf(stderr, "invalid numeric argument\n");
		return 1;

	}

	if(workload != BENCH_FILL && item_count == 0) {
		fprintf(stderr, "non-fill workloads require at least one item\n");
		return 1;

	}

	uint32_t widget_count = 0;
	uint32_t target_idx = 0;
	uint32_t result = 1;
	struct mkgui_widget *widgets = bench_widgets_create(workload, item_count, width, height, &widget_count, &target_idx);

	if(widgets) {
		struct mkgui_ctx *ctx = mkgui_ctx_create();

		if(ctx) {
			struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, widget_count, NULL, (int32_t)width, (int32_t)height);

			if(win) {
				struct bench_state state;
				state.win = win;
				state.mode = mode;
				state.target_idx = target_idx;
				state.acquire_us = 0.0;
				state.layout_us = 0.0;
				state.render_us = 0.0;
				state.blit_us = 0.0;
				dirty_all(win);
				mkgui_flush(win);
				layout_widgets(win);
				mkwin_sync(win->mkwin);

				for(uint32_t i = 0; i < 32; ++i) {
					bench_run_iteration(&state);

				}

				state.acquire_us = 0.0;
				state.layout_us = 0.0;
				state.render_us = 0.0;
				state.blit_us = 0.0;

				if(mode == BENCH_PRESENT || mode == BENCH_FRAME) {
					mkwin_sync(win->mkwin);

				}

				uint64_t start_ns = mkgui_now_ns();

				for(uint32_t i = 0; i < iterations; ++i) {
					bench_run_iteration(&state);

				}

				uint64_t drain_start_ns = mkgui_now_ns();

				if(mode == BENCH_PRESENT || mode == BENCH_FRAME) {
					mkwin_sync(win->mkwin);

				}

				uint64_t end_ns = mkgui_now_ns();
				uint64_t drain_ns = mode == BENCH_PRESENT || mode == BENCH_FRAME ? end_ns - drain_start_ns : 0;
				uint64_t elapsed_ns = end_ns - start_ns;
				uint64_t checksum = 1469598103934665603ull;
				size_t pixel_count = (size_t)win->win_w * (size_t)win->win_h;

				for(size_t i = 0; i < pixel_count; i += 64) {
					checksum ^= win->pixels[i];
					checksum *= 1099511628211ull;

				}

				uint64_t units;
				char *unit_name;

				if(mode == BENCH_LAYOUT) {
					units = widget_count;
					unit_name = "widget";

				} else if(mode == BENCH_DIRTY) {
					units = (uint64_t)(uint32_t)win->rects[target_idx].w * (uint64_t)(uint32_t)win->rects[target_idx].h;
					unit_name = "dirty_pixel";

				} else {
					units = pixel_count;
					unit_name = "pixel";

				}

				double ns_iteration = (double)elapsed_ns / iterations;
				double ns_unit = units > 0 ? ns_iteration / (double)units : 0.0;
				double minimum_gbps = mode == BENCH_LAYOUT ? 0.0 : (double)units * 4.0 / ns_iteration;
				double buffer_acquire_us = mode == BENCH_FRAME ? state.acquire_us / iterations : 0.0;
				double drain_us = (double)drain_ns / iterations / 1000.0;
				double layout_us = mode == BENCH_FRAME ? state.layout_us / iterations : 0.0;
				double render_us = mode == BENCH_FRAME ? state.render_us / iterations : 0.0;
				double blit_submit_us = mode == BENCH_FRAME ? state.blit_us / iterations : 0.0;
				printf("mode=%s workload=%s items=%u widgets=%u width=%d height=%d iterations=%u total_ns=%llu ns_iteration=%.3f unit=%s ns_unit=%.6f minimum_gbps=%.3f buffer_acquire_us=%.3f drain_us=%.3f layout_us=%.3f render_us=%.3f blit_submit_us=%.3f checksum=%016llx\n", argv[1], argv[2], item_count, widget_count, win->win_w, win->win_h, iterations, (unsigned long long)elapsed_ns, ns_iteration, unit_name, ns_unit, minimum_gbps, buffer_acquire_us, drain_us, layout_us, render_us, blit_submit_us, (unsigned long long)checksum);
				result = 0;
				mkgui_window_destroy(win);

			} else {
				fprintf(stderr, "window creation failed\n");

			}

			mkgui_ctx_destroy(ctx);

		} else {
			fprintf(stderr, "context creation failed\n");

		}

		free(widgets);

	} else {
		fprintf(stderr, "widget allocation failed\n");

	}

	return (int)result;

}
