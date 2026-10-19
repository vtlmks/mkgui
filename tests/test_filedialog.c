// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT

#include "../mkgui.c"

static struct fd_state state;
static char results[FD_MAX_RESULTS][FD_PATH_SIZE + 260];
static uint32_t checks;
static uint32_t failures;

// [=]===^=[ check ]============================================================================[=]
static void check(uint32_t condition, char *message) {
	++checks;

	if(!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		++failures;

	}
}

// [=]===^=[ main ]=============================================================================[=]
int main(void) {
	struct mkgui_input_data input = {.widget_id = FD_ID_NAME_INPUT};
	struct mkgui_listview_data list = {.widget_id = FD_ID_FILES, .selected_row = -1};
	struct mkgui_id_slot empty = {.idx = UINT32_MAX};
	struct mkgui_window window = {.inputs = &input, .input_count = 1, .listvs = &list, .listv_count = 1, .id_index = &empty, .id_index_cap = 1};
	char base[FD_PATH_SIZE];
	char expected[FD_PATH_SIZE + 260];
	fd = &state;
	fd_results = results;

#ifdef _WIN32
	check(GetCurrentDirectoryA(sizeof(base), base) > 0, "working directory");
#else
	check(getcwd(base, sizeof(base)) != NULL, "working directory");
#endif

	snprintf(fd->path, sizeof(fd->path), "%s", base);
	mkgui_input_set(&window, FD_ID_NAME_INPUT, "README.md");
	check(fd_confirm(&window), "typed relative filename opens without list selection");
	snprintf(expected, sizeof(expected), "%s/README.md", base);
	check(fd_result_count == 1 && strcmp(results[0], expected) == 0, "relative filename resolves against current directory");
	fd->confirmed = 0;
	mkgui_input_set(&window, FD_ID_NAME_INPUT, expected);
	check(fd_confirm(&window), "typed absolute filename opens");
	check(strcmp(results[0], expected) == 0, "absolute path does not gain a directory prefix");
	mkgui_input_set(&window, FD_ID_NAME_INPUT, "file with spaces.txt");
	fd_build_results(&window);
	snprintf(expected, sizeof(expected), "%s/file with spaces.txt", base);
	check(fd_result_count == 1 && strcmp(results[0], expected) == 0, "unquoted spaces belong to one filename");
	fd->entry_count = 2;
	fd->filtered_count = 2;
	fd->filtered[0] = 0;
	fd->filtered[1] = 1;
	snprintf(fd->entries[0].name, sizeof(fd->entries[0].name), "README.md");
	snprintf(fd->entries[1].name, sizeof(fd->entries[1].name), "CHANGELOG.md");
	list.multi_sel[0] = 0;
	list.multi_sel[1] = 1;
	list.multi_sel_count = 2;
	check(fd_confirm(&window) && fd_result_count == 2, "multiple selected files remain supported");
	list.multi_sel_count = 0;
	list.selected_row = 0;
	mkgui_input_set(&window, FD_ID_NAME_INPUT, "CHANGELOG.md");
	check(fd_confirm(&window), "typed filename overrides stale single selection");
	snprintf(expected, sizeof(expected), "%s/CHANGELOG.md", base);
	check(strcmp(results[0], expected) == 0, "typed target wins over selected row");
	list.selected_row = -1;
	fd->confirmed = 0;
	mkgui_input_set(&window, FD_ID_NAME_INPUT, "documentation");
	check(!fd_confirm(&window) && !fd->confirmed, "typed directory navigates instead of closing");
	check(mkgui_path_is_dir(fd->path) && strcmp(fd->path, base) != 0, "directory navigation changes current folder");
	check(mkgui_input_get(&window, FD_ID_NAME_INPUT)[0] == '\0', "directory navigation clears filename");
	struct mkgui_file_filter filter = {"mkgui", "*.mkgui"};
	fd->mode = 1;
	fd->filters = &filter;
	fd->filter_count = 1;
	snprintf(fd->path, sizeof(fd->path), "%s", "parent.with.dots");
	mkgui_input_set(&window, FD_ID_NAME_INPUT, "new-project");
	check(fd_confirm(&window), "new save target is accepted");
	check(strcmp(results[0], "parent.with.dots/new-project.mkgui") == 0, "parent-directory dots do not suppress filename extension");
	mkgui_input_set(&window, FD_ID_NAME_INPUT, "new-project.txt");
	check(fd_confirm(&window) && strcmp(results[0], "parent.with.dots/new-project.txt") == 0, "explicit extension is preserved");

#ifdef _WIN32
	fd->mode = 0;
	mkgui_input_set(&window, FD_ID_NAME_INPUT, "\\\\server\\share\\file.txt");
	fd_build_results(&window);
	check(strcmp(results[0], "\\\\server\\share\\file.txt") == 0, "UNC path is absolute");
	mkgui_input_set(&window, FD_ID_NAME_INPUT, "C:\\folder\\file.txt");
	fd_build_results(&window);
	check(strcmp(results[0], "C:\\folder\\file.txt") == 0, "drive path is absolute");
#endif

	fd->mode = 2;
	list.selected_row = -1;
	fd_build_results(&window);
	check(fd_result_count == 1 && strcmp(results[0], fd->path) == 0, "folder picker retains its current directory");
	fd = NULL;
	fd_results = NULL;
	printf("File dialog: %u checks, %u failures\n", checks, failures);
	return failures ? 1 : 0;

}
