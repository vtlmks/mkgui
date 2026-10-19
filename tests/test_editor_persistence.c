// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../mkgui_editor.h"

static char *selected_path;
static uint32_t failure_mode;
static uint32_t checks;
static uint32_t failures;

// [=]===^=[ persistence_flush ]===============================================================[=]
static int persistence_flush(FILE *file) {
	if(failure_mode == 1) {
		failure_mode = 0;
		errno = ENOSPC;
		return EOF;

	}

	return fflush(file);

}

// [=]===^=[ persistence_close ]===============================================================[=]
static int persistence_close(FILE *file) {
	int closed = fclose(file);

	if(failure_mode == 2) {
		failure_mode = 0;
		errno = EIO;
		return EOF;

	}

	return closed;

}

#ifdef _WIN32
// [=]===^=[ persistence_move ]================================================================[=]
static BOOL WINAPI persistence_move(LPCSTR source, LPCSTR destination, DWORD flags) {
	if(failure_mode == 3) {
		failure_mode = 0;
		SetLastError(ERROR_ACCESS_DENIED);
		return FALSE;

	}

	return MoveFileExA(source, destination, flags);

}
#else
// [=]===^=[ persistence_rename ]==============================================================[=]
static int persistence_rename(const char *source, const char *destination) {
	if(failure_mode == 3) {
		failure_mode = 0;
		errno = EACCES;
		return -1;

	}

	return rename(source, destination);

}
#endif

// [=]===^=[ persistence_dialog ]==============================================================[=]
static uint32_t persistence_dialog(struct mkgui_window *win, const struct mkgui_file_dialog_opts *options) {
	return 1;

}

// [=]===^=[ persistence_path ]================================================================[=]
static const char *persistence_path(struct mkgui_window *win, uint32_t index) {
	return selected_path;

}

#define fflush persistence_flush
#define fclose persistence_close
#define rename persistence_rename
#define MoveFileExA persistence_move
#define mkgui_dialog_save persistence_dialog
#define mkgui_dialog_get_path persistence_path
#define main editor_main
#include "../editor.c"
#undef main
#undef mkgui_dialog_get_path
#undef mkgui_dialog_save
#undef MoveFileExA
#undef rename
#undef fclose
#undef fflush

// [=]===^=[ check ]============================================================================[=]
static void check(uint32_t condition, char *message) {
	++checks;

	if(!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		++failures;

	}
}

// [=]===^=[ read_file ]========================================================================[=]
static char *read_file(char *path) {
	FILE *file = fopen(path, "rb");

	if(file) {
		if(fseek(file, 0, SEEK_END) == 0) {
			int64_t length = ftell(file);

			if(length >= 0 && fseek(file, 0, SEEK_SET) == 0) {
				char *text = malloc((size_t)length + 1);

				if(text) {
					if(fread(text, 1, (size_t)length, file) == (size_t)length) {
						text[length] = '\0';
						fclose(file);
						return text;

					}

					free(text);

				}
			}
		}

		fclose(file);

	}

	return NULL;

}

// [=]===^=[ main ]=============================================================================[=]
int main(int argc, char **argv) {
	if(argc != 2) {
		fprintf(stderr, "Usage: %s temporary-directory\n", argv[0]);
		return 1;

	}

#ifdef _WIN32
	check(_putenv_s("XDG_CONFIG_HOME", argv[1]) == 0, "temporary editor configuration");
#else
	check(setenv("XDG_CONFIG_HOME", argv[1], 1) == 0, "temporary editor configuration");
#endif

	char project[MKGUI_PATH_MAX];
	char bad[MKGUI_PATH_MAX];
	char generated[MKGUI_PATH_MAX];
	char alternate[MKGUI_PATH_MAX];
	snprintf(project, sizeof(project), "%s/project.mkgui", argv[1]);
	snprintf(bad, sizeof(bad), "%s/invalid.mkgui", argv[1]);
	snprintf(generated, sizeof(generated), "%s/generated.c", argv[1]);
	snprintf(alternate, sizeof(alternate), "%s/alternate.mkgui", argv[1]);
	selected_path = project;
	char style[512];
	check(strcmp(ed_style_to_str(MKGUI_BUTTON_SEPARATOR, MKGUI_BUTTON, style, sizeof(style)), "MKGUI_BUTTON_SEPARATOR") == 0, "button style export uses its own namespace");
	check(strcmp(ed_style_to_str(MKGUI_CHECKBOX_CHECKED, MKGUI_CHECKBOX, style, sizeof(style)), "MKGUI_CHECKBOX_CHECKED") == 0, "checkbox style export uses its own namespace");

	editor_api = mkgui_editor_api_v2();
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, ED_WINDOW, "Editor persistence test", "", 0, 640, 480, 0, MKGUI_WINDOW_HIDDEN, 0),
		MKGUI_W(MKGUI_STATUSBAR, ED_STATUSBAR, "", "", ED_WINDOW, 0, 0, 0, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_ctx_create();
	check(ctx != NULL, "context creation");

	if(ctx) {
		struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, sizeof(widgets) / sizeof(widgets[0]), NULL, 0, 0);
		check(win != NULL, "window creation");

		if(win) {
			int32_t width = -1;
			mkgui_statusbar_setup(win, ED_STATUSBAR, 1, &width);
			ed.canvas_w = 640;
			ed.canvas_h = 480;
			ed.grid_size = 8;
			ed.selected = -1;
			ed.next_id = 1;
			ed_add_widget(MKGUI_WINDOW, 0, 0);
			ed.selected = 0;
			ed_add_widget(MKGUI_VBOX, 0, 0);
			ed.selected = 1;
			ed_add_widget(MKGUI_BUTTON, 0, 0);
			ed.selected = 1;
			ed_add_widget(MKGUI_DROPDOWN, 0, 0);
			ed.widgets[2].parent_id = ed.widgets[1].id;
			ed.widgets[3].parent_id = ed.widgets[1].id;
			snprintf(ed.widgets[2].label, sizeof(ed.widgets[2].label), "Say \"hello\" at C:\\tools\\mkgui");
			snprintf(ed.widgets[2].icon, sizeof(ed.widgets[2].icon), "%s", MKICON_SAVE);
			ed.widgets[2].flags = MKGUI_FIXED;
			ed.widgets[2].style = MKGUI_BUTTON_SEPARATOR;
			ed.widgets[2].margin_l = 7;
			ed.widgets[2].weight = 3;
			ed.widgets[2].accel_keysym = 's';
			ed.widgets[2].accel_keymod = MKGUI_MOD_CONTROL;
			ed.widgets[2].event_mask = UINT64_C(1) << MKGUI_EVENT_CLICK;
			struct ed_widget_data *data = ed_get_or_create_widget_data(ed.widgets[3].id);
			data->item_count = 2;
			snprintf(data->items[0], sizeof(data->items[0]), "A \"quoted\" choice");
			snprintf(data->items[1], sizeof(data->items[1]), "C:\\data\\files");
			ed.required_widgets = UINT64_C(1) << MKGUI_TEXTAREA;
			ed.required_dialogs = 1;
			ed_save_project(win, 1);
			char *original = read_file(project);
			check(original != NULL && strcmp(ed.project_path, project) == 0, "project saved and path adopted");

			if(original) {
				snprintf(ed.widgets[2].label, sizeof(ed.widgets[2].label), "unsaved edit");
				ed_load_file(win, project);
				ed_load_file(win, ed.project_path);
				check(ed.widget_count == 4 && strcmp(ed.project_path, project) == 0, "loading the current document path preserves path storage");
				check(strcmp(ed.widgets[2].label, "Say \"hello\" at C:\\tools\\mkgui") == 0, "quoted label and backslashes load intact");
				check(ed.required_widgets == (UINT64_C(1) << MKGUI_TEXTAREA) && ed.required_dialogs == 1, "feature settings round trip");
				check(ed.widgets[2].parent_id == ed.widgets[1].id && ed.widgets[3].parent_id == ed.widgets[1].id, "nested hierarchy round trip");
				data = ed_find_widget_data(ed.widgets[3].id);
				check(data && data->item_count == 2 && strcmp(data->items[0], "A \"quoted\" choice") == 0 && strcmp(data->items[1], "C:\\data\\files") == 0, "item data round trip");
				ed_save_project(win, 0);
				char *again = read_file(project);
				check(again && strcmp(original, again) == 0, "save/load/save preserves serialized project");
				free(again);

				for(uint32_t mode = 1; mode <= 3; ++mode) {
					snprintf(ed.widgets[2].label, sizeof(ed.widgets[2].label), "failed save %u", mode);
					failure_mode = mode;
					ed_save_project(win, 0);
					again = read_file(project);
					check(again && strcmp(original, again) == 0, "failed write/close/replace preserves previous project");
					check(strstr(mkgui_statusbar_get(win, ED_STATUSBAR, 0), "Cannot save") != NULL, "failed save reports failure");
					free(again);

				}

				selected_path = alternate;
				failure_mode = 3;
				ed_save_project(win, 1);
				check(strcmp(ed.project_path, project) == 0, "failed Save As preserves the document path");
				again = read_file(alternate);
				check(again == NULL, "failed Save As leaves no destination file");
				free(again);

				ed_load_file(win, project);
				char *invalid[] = {
					"[mkgui 1]\n[widget 0]\ntype 999999\nid 1\nparent 0\n",
					"[mkgui 1]\n[widget 0]\ntype 0\nid 1\n",
					"[mkgui 1]\ncanvas broken\n[widget 0]\ntype 0\nid 1\nparent 0\n",
					"[mkgui 1]\n[widget 0]\ntype 0\nid 1\nparent 0\n[widget 1]\ntype 1\nid 2\nparent 99\n",
					"[mkgui 1]\n[widget 0]\ntype 0\nid 1\nparent 0\n[widget 1]\ntype 1\nid 1\nparent 1\n",
					"[mkgui 1]\n[widget 0]\ntype 0\nid 1\nparent 0\n[widget 1]\ntype 1\nid 2\nparent 3\n[widget 2]\ntype 1\nid 3\nparent 2\n",
				};

				for(uint32_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
					FILE *file = fopen(bad, "w");
					check(file != NULL, "invalid fixture creation");

					if(file) {
						fputs(invalid[i], file);
						fclose(file);
						ed_load_file(win, bad);
						check(ed.widget_count == 4 && ed.widgets[2].icon[0] && strcmp(ed.project_path, project) == 0 && strstr(mkgui_statusbar_get(win, ED_STATUSBAR, 0), "Cannot load"), "invalid load preserves current project");

					}
				}

				selected_path = generated;
				ed_generate_code(win);
				again = read_file(generated);
				check(again && strstr(again, "\\\"hello\\\"") && strstr(again, "C:\\\\tools\\\\mkgui"), "generated literals escape quotes and backslashes");

				if(again) {
					failure_mode = 1;
					ed_generate_code(win);
					char *after_failure = read_file(generated);
					check(after_failure && strcmp(again, after_failure) == 0, "failed code export preserves the previous source");
					check(strstr(mkgui_statusbar_get(win, ED_STATUSBAR, 0), "Cannot write") != NULL, "failed code export reports failure");
					free(after_failure);

				}

				free(again);
				snprintf(generated, sizeof(generated), "%s/snippet.h", argv[1]);
				ed_generate_snippet(win);
				again = read_file(generated);
				check(again && strstr(again, "\\\"hello\\\""), "snippet literals escape quotes");
				free(again);
				free(original);

			}

			ed_clear_undo();
			check(ed.undo_count == 0 && ed.undo_pos == 0 && ed.undo_start == 0, "new-document history reset");
			mkgui_window_destroy(win);

		}

		mkgui_ctx_destroy(ctx);

	}

	printf("Editor persistence: %u checks, %u failures\n", checks, failures);
	return failures ? 1 : 0;

}
