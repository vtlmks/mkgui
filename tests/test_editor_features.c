// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT

#define MKGUI_EDITOR_UNITY
#define main mkgui_editor_main
#include "../editor.c"
#undef main

static uint32_t feature_checks;
static uint32_t feature_failures;

// [=]===^=[ feature_check ]======================================[=]
static void feature_check(uint32_t condition, char *message) {
	++feature_checks;

	if(!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		++feature_failures;

	}
}

// [=]===^=[ test_editor_canvas_minimum ]========================[=]
static void test_editor_canvas_minimum(void) {
	static struct mkgui_window preview;
	memset(&ed, 0, sizeof(ed));
	ed.selected = -1;
	ed.canvas_w = 640;
	ed.canvas_h = 480;
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

	preview.scale = 1.0f;
	preview.font_height = 16;
	mkgui_recompute_metrics(&preview);
	ed.widget_count = 13;
	ed_fit_canvas_to_content(&preview);
	feature_check(ed.canvas_w == 640 && ed.canvas_h == 480, "fitting content changed the chosen canvas size");
	ed_push_undo();
	ed.widget_count = 20;
	ed_fit_canvas_to_content(&preview);
	feature_check(ed.canvas_w > 640 && ed.canvas_h == 480, "17-spinner row did not grow the canvas width");
	feature_check(ed.widgets[0].w == ed.canvas_w && ed.widgets[0].h == ed.canvas_h, "canvas growth did not update the project window");
	int32_t grown_w = ed.canvas_w;
	int32_t grown_h = ed.canvas_h;
	feature_check(layout_arena_init(), "cannot initialize minimum-size test arena");

	if(layout_parent_arena.base) {
		preview.widgets = ed_scratch.test_widgets;
		preview.widget_count = ed.widget_count;
		preview.rects = ed_rects;
		preview.win_w = ed.canvas_w;
		preview.win_h = ed.canvas_h;
		int32_t min_w, min_h;
		mkgui_get_min_size(&preview, &min_w, &min_h);
		feature_check(ed.canvas_w == min_w, "canvas and runtime minimum widths differ");
		layout_widgets(&preview);

		for(uint32_t i = 3; i < ed.widget_count; ++i) {
			feature_check(ed_rects[i].x >= ed_rects[2].x && ed_rects[i].x + ed_rects[i].w <= ed_rects[2].x + ed_rects[2].w, "spinner escapes its grown row");

		}

		layout_arena_fini();

	}

	ed.canvas_w = 100;
	ed.canvas_h = 100;
	ed_fit_canvas_to_content(&preview);
	feature_check(ed.canvas_w == grown_w, "manual resize bypassed the content minimum");
	ed.canvas_w = grown_w + 100;
	ed.canvas_h = grown_h + 100;
	ed_fit_canvas_to_content(&preview);
	feature_check(ed.canvas_w == grown_w + 100 && ed.canvas_h == grown_h + 100, "content fitting shrank a larger chosen canvas");
	ed.widgets[3].flags = MKGUI_FIXED;
	ed.widgets[3].h = 700;
	ed_fit_canvas_to_content(&preview);
	feature_check(ed.canvas_h > 700, "canvas height did not grow for tall content");
	ed_undo();
	feature_check(ed.widget_count == 13 && ed.canvas_w == 640 && ed.canvas_h == 480, "undo did not restore canvas dimensions");
	ed_fit_canvas_to_content(&preview);
	feature_check(ed.canvas_w == 640 && ed.canvas_h == 480, "undo restored a canvas that still needs growth");

	for(uint32_t i = 0; i < ED_MAX_UNDO; ++i) {
		free(ed.undo[i].widgets);
		ed.undo[i].widgets = NULL;
		ed.undo[i].widget_cap = 0;

	}
}

// [=]===^=[ editor_history_value ]============================================================[=]
static void editor_history_value(uint32_t value) {
	ed.widget_count = value % 3 + 1;
	ed.widgets[0].id = value;
	ed.canvas_w = 600 + (int32_t)value;
	ed.canvas_h = 400 + (int32_t)value;

}

// [=]===^=[ check_editor_history ]============================================================[=]
static void check_editor_history(uint32_t value) {
	feature_check(ed.widgets[0].id == value && ed.widget_count == value % 3 + 1 && ed.canvas_w == 600 + (int32_t)value && ed.canvas_h == 400 + (int32_t)value, "editor history restores widgets and canvas");

}

// [=]===^=[ test_editor_history ]=============================================================[=]
static void test_editor_history(void) {
	memset(&ed, 0, sizeof(ed));

	for(uint32_t i = 0; i < ED_MAX_UNDO * 2; ++i) {
		editor_history_value(i);
		ed_push_undo();

	}

	editor_history_value(ED_MAX_UNDO * 2);

	for(uint32_t i = 0; i < ED_MAX_UNDO; ++i) {
		ed_undo();
		check_editor_history(ED_MAX_UNDO * 2 - i - 1);

	}

	ed_undo();
	check_editor_history(ED_MAX_UNDO);

	for(uint32_t i = 0; i < ED_MAX_UNDO; ++i) {
		ed_redo();
		check_editor_history(ED_MAX_UNDO + i + 1);

	}

	ed_redo();
	check_editor_history(ED_MAX_UNDO * 2);
	ed_undo();
	ed_push_undo();
	editor_history_value(9000);
	ed_redo();
	check_editor_history(9000);
	ed_undo();
	check_editor_history(ED_MAX_UNDO * 2 - 1);
	ed_redo();
	check_editor_history(9000);

	for(uint32_t i = 0; i < ED_MAX_UNDO; ++i) {
		free(ed.undo[i].widgets);

	}

	memset(&ed, 0, sizeof(ed));

}

// [=]===^=[ main ]================================================[=]
int main(int argc, char **argv) {
	ed.widget_count = 4;
	ed.widgets[0].type = MKGUI_WINDOW;
	ed.widgets[1].type = MKGUI_BUTTON;
	ed.widgets[2].type = MKGUI_WINDOW;
	ed.widgets[3].type = MKGUI_VBOX;
	ed.canvas_w = 640;
	ed.canvas_h = 480;
	ed.widgets[1].w = 80;
	ed.widgets[1].h = 28;
	ed_prepare_widgets(ed_scratch.test_widgets);
	feature_check(ed_scratch.test_widgets[0].w == 640 && ed_scratch.test_widgets[0].h == 480, "project window dimensions lost during export conversion");
	feature_check(ed_scratch.test_widgets[1].w == 0 && ed_scratch.test_widgets[1].h == 0, "flexible preview control retains placement size");
	ed.widgets[1].flags = MKGUI_FIXED;
	ed_prepare_widgets(ed_scratch.test_widgets);
	feature_check(ed_scratch.test_widgets[1].w == 80 && ed_scratch.test_widgets[1].h == 28, "fixed preview control loses explicit size");
	ed.widgets[1].flags = 0;
	uint64_t automatic = ed_collect_features(0, 0);
	feature_check((automatic & (UINT64_C(1) << MKGUI_BUTTON)) != 0, "project button omitted");
	feature_check((automatic & (UINT64_C(1) << MKGUI_VBOX)) != 0, "second window's container omitted");
	feature_check((automatic & (UINT64_C(1) << MKGUI_DATEPICKER)) == 0, "unused widget retained");
	uint64_t dynamic = ed_collect_features((UINT64_C(1) << MKGUI_DATEPICKER) | (UINT64_C(1) << MKGUI_TAB) | (UINT64_C(1) << MKGUI_MENUITEM), 0);
	feature_check((dynamic & (UINT64_C(1) << MKGUI_DATEPICKER)) != 0, "dynamic widget omitted");
	feature_check((dynamic & (UINT64_C(1) << MKGUI_TABS)) != 0, "tab parent dependency omitted");
	feature_check((dynamic & (UINT64_C(1) << MKGUI_MENU)) != 0, "menu parent dependency omitted");

	for(uint32_t dialogs = 0; dialogs < 16; ++dialogs) {
		uint64_t used = ed_collect_features(0, dialogs);

		for(uint32_t i = 0; i < sizeof(ed_dialog_features) / sizeof(ed_dialog_features[0]); ++i) {
			if(dialogs & (1u << i)) {
				feature_check((used & ed_dialog_features[i].widgets) == ed_dialog_features[i].widgets, "dialog widget dependency omitted");

			}
		}

		if(argc > 1) {
			static char path[4096];
			int32_t length = snprintf(path, sizeof(path), "%s/features_%u.h", argv[1], dialogs);

			if(length > 0 && (size_t)length < sizeof(path)) {
				FILE *file = fopen(path, "w");

				if(file) {
					ed.required_widgets = UINT64_C(1) << MKGUI_LABEL;
					ed.required_dialogs = dialogs;
					ed_write_features(file);
					feature_check(fclose(file) == 0, "cannot finish generated configuration");

				} else {
					feature_check(0, "cannot create generated configuration");

				}

			} else {
				feature_check(0, "generated configuration path too long");

			}
		}
	}

	FILE *file = tmpfile();

	if(file) {
		static char header[4096];
		ed.required_widgets = UINT64_C(1) << MKGUI_DATEPICKER;
		ed.required_dialogs = 0;
		ed_write_features(file);
		rewind(file);
		size_t length = fread(header, 1, sizeof(header) - 1, file);
		header[length] = '\0';
		feature_check(strstr(header, "#define MKGUI_NO_BUTTON\n") == NULL, "generated configuration excludes project widget");
		feature_check(strstr(header, "#define MKGUI_NO_DATEPICKER\n") == NULL, "generated configuration excludes dynamic widget");
		feature_check(strstr(header, "#define MKGUI_NO_LISTVIEW\n") != NULL, "generated configuration retains unused widget");
		feature_check(strstr(header, "#define MKGUI_NO_FILEDIALOG\n") != NULL, "generated configuration retains unselected dialog");
		fclose(file);

	} else {
		feature_check(0, "cannot create configuration stream");

	}

	test_editor_history();
	test_editor_canvas_minimum();
	fprintf(stderr, "editor features: %u checks, %u failures\n", feature_checks, feature_failures);
	return feature_failures ? 1 : 0;

}
