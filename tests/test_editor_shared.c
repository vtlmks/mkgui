// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdio.h>
#include <string.h>
#include "../mkgui.h"

static char *output_path;
static uint32_t checks;
static uint32_t failures;

// [=]===^=[ save_dialog ]======================================================================[=]
static uint32_t save_dialog(struct mkgui_window *win, const struct mkgui_file_dialog_opts *opts) {
	return 1;

}

// [=]===^=[ saved_path ]=======================================================================[=]
static const char *saved_path(struct mkgui_window *win, uint32_t index) {
	return output_path;

}

#define mkgui_dialog_save save_dialog
#define mkgui_dialog_get_path saved_path
#define main editor_main
#include "../editor.c"
#undef main
#undef mkgui_dialog_save
#undef mkgui_dialog_get_path

// [=]===^=[ check ]============================================================================[=]
static void check(uint32_t condition, char *message) {
	++checks;

	if(!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		++failures;

	}
}

// [=]===^=[ main ]=============================================================================[=]
int main(int argc, char **argv) {
	editor_api = mkgui_editor_api_v2();
	ed.selected = -1;
	ed.canvas_w = 640;
	ed.canvas_h = 480;
	ed.widget_count = 21;
	ed.widgets[0].type = MKGUI_WINDOW;
	ed.widgets[0].id = 1;
	ed.widgets[1].type = MKGUI_VBOX;
	ed.widgets[1].id = 2;
	ed.widgets[1].parent_id = 1;
	ed.widgets[2].type = MKGUI_HBOX;
	ed.widgets[2].id = 3;
	ed.widgets[2].parent_id = 2;

	for(uint32_t i = 3; i < 20; ++i) {
		ed.widgets[i].type = MKGUI_SPINNER;
		ed.widgets[i].id = i + 1;
		ed.widgets[i].parent_id = 3;

	}

	ed.widgets[20].type = MKGUI_BUTTON;
	ed.widgets[20].id = 21;
	ed.widgets[20].parent_id = 2;
	snprintf(ed.widgets[20].label, sizeof(ed.widgets[20].label), "Shared preview");
	snprintf(ed.widgets[20].icon, sizeof(ed.widgets[20].icon), "%s", MKICON_SAVE);

	for(uint32_t i = 0; i < ed.widget_count; ++i) {
		snprintf(ed.widgets[i].id_name, sizeof(ed.widgets[i].id_name), "ID_WIDGET_%u", i + 1);

	}

	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, ED_WINDOW, "mkgui shared editor test", "", 0, 1200, 800, 0, 0, 0),
		MKGUI_W(MKGUI_CANVAS, ED_CANVAS_LBL, "", "", ED_WINDOW, 0, 0, 0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	check(ctx != NULL, "context creation");

	if(ctx) {
		mkgui_ctx_set_scale(ctx, 1.0f);
		struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, sizeof(widgets) / sizeof(widgets[0]), NULL, 0, 0);
		check(win != NULL, "window creation");

		if(win) {
			uint32_t pixel = 0xff123456;
			uint32_t added = 0;
			char name[64];

			for(uint32_t i = 0; i < 1000; ++i) {
				snprintf(name, sizeof(name), "editor-custom-%u", i);

				if(mkgui_icon_add(name, &pixel, 1, 1) >= 0) {
					++added;

				}
			}

			check(added == 1000, "private bridge icon cache growth");
			mkgui_canvas_set_callback(win, ED_CANVAS_LBL, ed_render_canvas, NULL);
			mkgui_window_invalidate(win);
			mkgui_ctx_wait_until(ctx, mkgui_now_ns());
			check(ed.canvas_w > 640, "shared layout did not grow the editor canvas");
			check(win->widgets[0].type == MKGUI_WINDOW && win->widgets[0].id == ED_WINDOW, "preview did not restore the runtime widget");
			check(ed_rects[20].w > 0 && ed_rects[20].h > 0, "preview button was not laid out");

			for(uint32_t i = 3; i < 20; ++i) {
				check(ed_rects[i].w > 0 && ed_rects[i].x >= ed_rects[2].x && ed_rects[i].x + ed_rects[i].w <= ed_rects[2].x + ed_rects[2].w, "spinner escapes shared preview layout");

			}

			if(argc > 1) {
				output_path = argv[1];
				ed_generate_code(win);
				FILE *file = fopen(output_path, "r");
				check(file != NULL, "editor did not generate C source");

				if(file) {
					fclose(file);

				}
			}

			mkgui_window_destroy(win);

		}

		mkgui_ctx_destroy(ctx);

	}

	printf("Shared editor: %u checks, %u failures\n", checks, failures);
	return failures ? 1 : 0;

}
