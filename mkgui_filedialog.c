// Copyright (c) 2026, mkgui contributors
// SPDX-License-Identifier: MIT

#ifdef _WIN32
#include <shlobj.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define FD_MAX_FILES      4096
#define FD_PATH_SIZE      4096
#define FD_MAX_BOOKMARKS  32
#define FD_MAX_RESULTS    64
#define FD_INIT_W         720
#define FD_INIT_H         500
#define FD_DBLCLICK_MS    400
#define FD_BOTTOM_H       72

// ---------------------------------------------------------------------------
// Widget IDs
// ---------------------------------------------------------------------------

enum {
	FD_ID_WINDOW      = 1,
	FD_ID_PATH_LABEL  = 2,
	FD_ID_PATH_INPUT  = 3,
	FD_ID_SPLIT    = 4,
	FD_ID_BOOKMARKS   = 5,
	FD_ID_FILES       = 6,
	FD_ID_NAME_LABEL  = 7,
	FD_ID_NAME_INPUT  = 8,
	FD_ID_BTN_CONFIRM = 9,
	FD_ID_BTN_CANCEL  = 10,
};

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

struct fd_entry {
	char name[256];
	uint32_t is_dir;
	int64_t size;
};

struct fd_bookmark {
	char label[64];
	char path[1024];
};

struct fd_state {
	uint32_t mode;
	char path[FD_PATH_SIZE];

	struct fd_entry entries[FD_MAX_FILES];
	uint32_t entry_count;

	struct fd_bookmark bookmarks[FD_MAX_BOOKMARKS];
	uint32_t bookmark_count;

	uint32_t confirmed;

	uint32_t last_click_time;
	int32_t last_click_row;
};

static struct fd_state fd;
static char fd_results[FD_MAX_RESULTS][FD_PATH_SIZE];
static uint32_t fd_result_count;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// [=]===^=[ fd_get_time_ms ]=====================================[=]
static uint32_t fd_get_time_ms(void) {
	return mkgui_time_ms();
}

// [=]===^=[ fd_strcasecmp ]======================================[=]
static int32_t fd_strcasecmp(const char *a, const char *b) {
	while(*a && *b) {
		int32_t ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : *a;
		int32_t cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : *b;
		if(ca != cb) {
			return ca - cb;
		}
		++a;
		++b;
	}
	return (uint8_t)*a - (uint8_t)*b;
}

// [=]===^=[ fd_url_decode ]======================================[=]
static void fd_url_decode(char *dst, const char *src, uint32_t dst_size) {
	uint32_t di = 0;
	while(*src && di < dst_size - 1) {
		if(*src == '%' && src[1] && src[2]) {
			char hex[3] = { src[1], src[2], 0 };
			dst[di++] = (char)strtol(hex, NULL, 16);
			src += 3;
		} else {
			dst[di++] = *src++;
		}
	}
	dst[di] = '\0';
}

// [=]===^=[ fd_icon_for_name ]====================================[=]
static const char *fd_icon_for_name(const char *name, uint32_t is_dir) {
	if(is_dir) {
		return "folder";
	}
	const char *dot = strrchr(name, '.');
	if(!dot) {
		return "file-document-outline";
	}
	if(strcmp(dot, ".c") == 0 || strcmp(dot, ".cpp") == 0 || strcmp(dot, ".cc") == 0) {
		return "language-c";
	}
	if(strcmp(dot, ".h") == 0 || strcmp(dot, ".hpp") == 0) {
		return "language-c";
	}
	if(strcmp(dot, ".sh") == 0) {
		return "script-text";
	}
	if(strcmp(dot, ".py") == 0) {
		return "language-python";
	}
	if(strcmp(dot, ".md") == 0 || strcmp(dot, ".txt") == 0) {
		return "file-document-outline";
	}
	if(strcmp(dot, ".png") == 0 || strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0 || strcmp(dot, ".gif") == 0 || strcmp(dot, ".svg") == 0) {
		return "file-image";
	}
	return "file-document-outline";
}

// ---------------------------------------------------------------------------
// Directory scanning
// ---------------------------------------------------------------------------

// [=]===^=[ fd_compare_entries ]=================================[=]
static int fd_compare_entries(const void *a, const void *b) {
	const struct fd_entry *ea = (const struct fd_entry *)a;
	const struct fd_entry *eb = (const struct fd_entry *)b;
	if(strcmp(ea->name, "..") == 0) {
		return -1;
	}
	if(strcmp(eb->name, "..") == 0) {
		return 1;
	}
	if(ea->is_dir && !eb->is_dir) {
		return -1;
	}
	if(!ea->is_dir && eb->is_dir) {
		return 1;
	}
	return fd_strcasecmp(ea->name, eb->name);
}

// [=]===^=[ fd_scan_dir ]========================================[=]
static void fd_scan_dir(void) {
	fd.entry_count = 0;

#ifdef _WIN32
	uint32_t plen = (uint32_t)strlen(fd.path);
	if(plen == 0) {
		return;
	}

	if(!(plen == 3 && fd.path[1] == ':' && fd.path[2] == '\\')) {
		struct fd_entry *e = &fd.entries[fd.entry_count++];
		snprintf(e->name, sizeof(e->name), "..");
		e->is_dir = 1;
		e->size = 0;
	}

	char pattern[FD_PATH_SIZE];
	snprintf(pattern, sizeof(pattern), "%s\\*", fd.path);
	WIN32_FIND_DATAA wfd;
	HANDLE hf = FindFirstFileA(pattern, &wfd);
	if(hf == INVALID_HANDLE_VALUE) {
		return;
	}
	do {
		if(fd.entry_count >= FD_MAX_FILES) {
			break;
		}
		if(wfd.cFileName[0] == '.') {
			continue;
		}
		struct fd_entry *e = &fd.entries[fd.entry_count];
		snprintf(e->name, sizeof(e->name), "%s", wfd.cFileName);
		e->is_dir = (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
		e->size = ((int64_t)wfd.nFileSizeHigh << 32) | wfd.nFileSizeLow;
		++fd.entry_count;
	} while(FindNextFileA(hf, &wfd));
	FindClose(hf);
#else
	DIR *dir = opendir(fd.path);
	if(!dir) {
		return;
	}

	if(strcmp(fd.path, "/") != 0) {
		struct fd_entry *e = &fd.entries[fd.entry_count++];
		snprintf(e->name, sizeof(e->name), "..");
		e->is_dir = 1;
		e->size = 0;
	}

	struct dirent *de;
	while((de = readdir(dir)) != NULL && fd.entry_count < FD_MAX_FILES) {
		if(de->d_name[0] == '.') {
			continue;
		}
		struct fd_entry *e = &fd.entries[fd.entry_count];
		snprintf(e->name, sizeof(e->name), "%s", de->d_name);

		char fullpath[FD_PATH_SIZE];
		snprintf(fullpath, sizeof(fullpath), "%s/%s", fd.path, de->d_name);

		struct stat st;
		if(stat(fullpath, &st) == 0) {
			e->is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
			e->size = st.st_size;
		} else {
			e->is_dir = 0;
			e->size = 0;
		}
		++fd.entry_count;
	}
	closedir(dir);
#endif

	if(fd.entry_count > 1) {
		qsort(fd.entries, fd.entry_count, sizeof(fd.entries[0]), fd_compare_entries);
	}
}

// ---------------------------------------------------------------------------
// Bookmarks
// ---------------------------------------------------------------------------

// [=]===^=[ fd_add_bookmark ]====================================[=]
static void fd_add_bookmark(const char *label, const char *path) {
	if(fd.bookmark_count >= FD_MAX_BOOKMARKS) {
		return;
	}
	struct fd_bookmark *b = &fd.bookmarks[fd.bookmark_count++];
	snprintf(b->label, sizeof(b->label), "%s", label);
	snprintf(b->path, sizeof(b->path), "%s", path);
}

// [=]===^=[ fd_path_exists ]======================================[=]
static uint32_t fd_path_exists(const char *path) {
#ifdef _WIN32
	return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
	return access(path, F_OK) == 0;
#endif
}

// [=]===^=[ fd_get_home ]=========================================[=]
static const char *fd_get_home(void) {
#ifdef _WIN32
	const char *h = getenv("USERPROFILE");
	if(h) {
		return h;
	}
	return "C:\\";
#else
	const char *h = getenv("HOME");
	if(h) {
		return h;
	}
	return "/";
#endif
}

// [=]===^=[ fd_load_bookmarks ]==================================[=]
static void fd_load_bookmarks(void) {
	fd.bookmark_count = 0;

	const char *home = fd_get_home();
	fd_add_bookmark("Home", home);

	char buf[FD_PATH_SIZE];

	snprintf(buf, sizeof(buf), "%s/Desktop", home);
	if(fd_path_exists(buf)) {
		fd_add_bookmark("Desktop", buf);
	}

	snprintf(buf, sizeof(buf), "%s/Documents", home);
	if(fd_path_exists(buf)) {
		fd_add_bookmark("Documents", buf);
	}

	snprintf(buf, sizeof(buf), "%s/Downloads", home);
	if(fd_path_exists(buf)) {
		fd_add_bookmark("Downloads", buf);
	}

#ifdef _WIN32
	FILE *f = NULL;
#else
	snprintf(buf, sizeof(buf), "%s/.config/gtk-3.0/bookmarks", home);
	FILE *f = fopen(buf, "r");
#endif
	if(!f) {
		return;
	}

	char line[4096];
	while(fgets(line, sizeof(line), f) != NULL && fd.bookmark_count < FD_MAX_BOOKMARKS) {
		char *nl = strchr(line, '\n');
		if(nl) {
			*nl = '\0';
		}
		if(strncmp(line, "file://", 7) != 0) {
			continue;
		}

		char *raw_path = line + 7;
		char *space = strchr(raw_path, ' ');
		char *label = NULL;
		if(space) {
			*space = '\0';
			label = space + 1;
		}

		char decoded[1024];
		fd_url_decode(decoded, raw_path, sizeof(decoded));

		if(!label || !*label) {
			label = strrchr(decoded, '/');
			label = label ? label + 1 : decoded;
		}

		if(fd_path_exists(decoded)) {
			fd_add_bookmark(label, decoded);
		}
	}
	fclose(f);
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

// [=]===^=[ fd_navigate ]========================================[=]
static void fd_navigate(struct mkgui_ctx *dlg, const char *newpath) {
#ifdef _WIN32
	DWORD attr = GetFileAttributesA(newpath);
	if(attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
		return;
	}
	char resolved[FD_PATH_SIZE];
	DWORD rlen = GetFullPathNameA(newpath, sizeof(resolved), resolved, NULL);
	if(rlen == 0 || rlen >= sizeof(resolved)) {
		return;
	}
#else
	struct stat st;
	if(stat(newpath, &st) != 0 || !S_ISDIR(st.st_mode)) {
		return;
	}
	char resolved[FD_PATH_SIZE];
	if(realpath(newpath, resolved) == NULL) {
		return;
	}
#endif

	snprintf(fd.path, sizeof(fd.path), "%s", resolved);
	fd_scan_dir();

	mkgui_input_set(dlg, FD_ID_PATH_INPUT, fd.path);

	struct mkgui_listview_data *lv = find_listv_data(dlg, FD_ID_FILES);
	if(lv) {
		lv->row_count = fd.entry_count;
		lv->selected_row = -1;
		lv->scroll_y = 0;
	}
	dirty_all(dlg);
}

// [=]===^=[ fd_navigate_entry ]==================================[=]
static void fd_navigate_entry(struct mkgui_ctx *dlg, int32_t idx) {
	if(idx < 0 || idx >= (int32_t)fd.entry_count) {
		return;
	}
	struct fd_entry *e = &fd.entries[idx];
	if(!e->is_dir) {
		return;
	}

	char newpath[FD_PATH_SIZE];
	if(strcmp(e->name, "..") == 0) {
		char *last = strrchr(fd.path, '/');
		if(last && last != fd.path) {
			snprintf(newpath, sizeof(newpath), "%.*s", (int)(last - fd.path), fd.path);
		} else {
			snprintf(newpath, sizeof(newpath), "/");
		}
	} else {
		if(strcmp(fd.path, "/") == 0) {
			snprintf(newpath, sizeof(newpath), "/%s", e->name);
		} else {
			snprintf(newpath, sizeof(newpath), "%s/%s", fd.path, e->name);
		}
	}
	fd_navigate(dlg, newpath);
}

// ---------------------------------------------------------------------------
// Results
// ---------------------------------------------------------------------------

// [=]===^=[ fd_build_results ]===================================[=]
static void fd_build_results(struct mkgui_ctx *dlg) {
	fd_result_count = 0;

	if(fd.mode == 1) {
		const char *name = mkgui_input_get(dlg, FD_ID_NAME_INPUT);
		if(name && name[0] != '\0') {
			if(strcmp(fd.path, "/") == 0) {
				snprintf(fd_results[0], FD_PATH_SIZE, "/%s", name);
			} else {
				snprintf(fd_results[0], FD_PATH_SIZE, "%s/%s", fd.path, name);
			}
			fd_result_count = 1;
		}
	} else {
		struct mkgui_listview_data *lv = find_listv_data(dlg, FD_ID_FILES);
		if(lv && lv->selected_row >= 0 && lv->selected_row < (int32_t)fd.entry_count) {
			struct fd_entry *e = &fd.entries[lv->selected_row];
			if(!e->is_dir) {
				if(strcmp(fd.path, "/") == 0) {
					snprintf(fd_results[0], FD_PATH_SIZE, "/%s", e->name);
				} else {
					snprintf(fd_results[0], FD_PATH_SIZE, "%s/%s", fd.path, e->name);
				}
				fd_result_count = 1;
			}
		}
	}
}

// [=]===^=[ fd_confirm ]=========================================[=]
static void fd_confirm(struct mkgui_ctx *dlg) {
	fd_build_results(dlg);
	if(fd_result_count > 0) {
		fd.confirmed = 1;
	}
}

// ---------------------------------------------------------------------------
// Listview callbacks
// ---------------------------------------------------------------------------

// [=]===^=[ fd_bookmark_row_cb ]==================================[=]
static void fd_bookmark_row_cb(uint32_t row, uint32_t col, char *buf, uint32_t buf_size, void *userdata) {
	(void)userdata;
	(void)col;
	if(row < fd.bookmark_count) {
		const char *icon = "folder";
		if(strcmp(fd.bookmarks[row].label, "Home") == 0) {
			icon = "home";
		} else if(strcmp(fd.bookmarks[row].label, "Desktop") == 0) {
			icon = "monitor";
		} else if(strcmp(fd.bookmarks[row].label, "Downloads") == 0) {
			icon = "download";
		} else if(strcmp(fd.bookmarks[row].label, "Documents") == 0) {
			icon = "file-document-multiple";
		}
		snprintf(buf, buf_size, "%s\t%s", icon, fd.bookmarks[row].label);
	} else {
		buf[0] = '\0';
	}
}

// [=]===^=[ fd_file_row_cb ]=====================================[=]
static void fd_file_row_cb(uint32_t row, uint32_t col, char *buf, uint32_t buf_size, void *userdata) {
	(void)userdata;
	if(row >= fd.entry_count) {
		buf[0] = '\0';
		return;
	}
	struct fd_entry *e = &fd.entries[row];

	switch(col) {
		case 0: {
			snprintf(buf, buf_size, "%s\t%s", fd_icon_for_name(e->name, e->is_dir), e->name);
		} break;

		case 1: {
			if(e->is_dir) {
				buf[0] = '\0';
			} else {
				snprintf(buf, buf_size, "%lld", (long long)e->size);
			}
		} break;

		default: {
			buf[0] = '\0';
		} break;
	}
}

// ---------------------------------------------------------------------------
// Dialog runner
// ---------------------------------------------------------------------------

// [=]===^=[ fd_run_dialog ]=======================================[=]
static uint32_t fd_run_dialog(struct mkgui_ctx *ctx, uint32_t mode, const char *default_name) {
	memset(&fd, 0, sizeof(fd));
	fd.mode = mode;
	fd.last_click_row = -1;

	snprintf(fd.path, sizeof(fd.path), "%s", fd_get_home());

	fd_load_bookmarks();
	fd_scan_dir();

	popup_destroy_all(ctx);

	uint32_t name_flags = (mode == 0) ? MKGUI_HIDDEN : 0;
	const char *confirm_label = (mode == 0) ? "Open" : "Save";
	const char *title = (mode == 0) ? "Open File" : "Save File";

	struct mkgui_widget widgets[] = {
		{ MKGUI_WINDOW,     FD_ID_WINDOW,      "",         "",             0,              0,  0, FD_INIT_W, FD_INIT_H, 0 },
		{ MKGUI_LABEL,      FD_ID_PATH_LABEL,  "Path:",    "",             FD_ID_WINDOW,   8,  6,  40, 24, 0 },
		{ MKGUI_INPUT,      FD_ID_PATH_INPUT,   "",         "",             FD_ID_WINDOW,  50,  4,  20, 24, MKGUI_ANCHOR_LEFT | MKGUI_ANCHOR_TOP | MKGUI_ANCHOR_RIGHT },
		{ MKGUI_VSPLIT, FD_ID_SPLIT,     "",         "",             FD_ID_WINDOW,   0, 32,   0, FD_BOTTOM_H, MKGUI_ANCHOR_LEFT | MKGUI_ANCHOR_TOP | MKGUI_ANCHOR_RIGHT | MKGUI_ANCHOR_BOTTOM },
		{ MKGUI_LISTVIEW,   FD_ID_BOOKMARKS,    "",         "",             FD_ID_SPLIT, 0,  0,   0,  0, MKGUI_REGION_LEFT },
		{ MKGUI_LISTVIEW,   FD_ID_FILES,        "",         "",             FD_ID_SPLIT, 0,  0,   0,  0, MKGUI_REGION_RIGHT | MKGUI_VIRTUAL },
		{ MKGUI_LABEL,      FD_ID_NAME_LABEL,  "File name:", "",           FD_ID_WINDOW,   8,  40, 70, 24, MKGUI_ANCHOR_BOTTOM | name_flags },
		{ MKGUI_INPUT,      FD_ID_NAME_INPUT,   "",         "",             FD_ID_WINDOW,  80,  40, 110, 24, MKGUI_ANCHOR_LEFT | MKGUI_ANCHOR_BOTTOM | MKGUI_ANCHOR_RIGHT | name_flags },
		{ MKGUI_BUTTON,     FD_ID_BTN_CONFIRM,  "",         "",             FD_ID_WINDOW, 100,   8, 80, 28, MKGUI_ANCHOR_BOTTOM | MKGUI_ANCHOR_RIGHT },
		{ MKGUI_BUTTON,     FD_ID_BTN_CANCEL,  "Cancel",   "",             FD_ID_WINDOW,  12,   8, 80, 28, MKGUI_ANCHOR_BOTTOM | MKGUI_ANCHOR_RIGHT },
	};

	strncpy(widgets[0].label, title, MKGUI_MAX_TEXT - 1);
	strncpy(widgets[8].label, confirm_label, MKGUI_MAX_TEXT - 1);

	uint32_t wcount = sizeof(widgets) / sizeof(widgets[0]);
	struct mkgui_ctx *dlg = mkgui_create_child(ctx, widgets, wcount, title, FD_INIT_W, FD_INIT_H);
	if(!dlg) {
		return 0;
	}

	mkgui_input_set(dlg, FD_ID_PATH_INPUT, fd.path);
	if(mode == 1 && default_name) {
		mkgui_input_set(dlg, FD_ID_NAME_INPUT, default_name);
	}

	struct mkgui_column bm_cols[] = {
		{ "Places", 160, MKGUI_CELL_ICON_TEXT },
	};
	mkgui_listview_setup(dlg, FD_ID_BOOKMARKS, fd.bookmark_count, 1, bm_cols, fd_bookmark_row_cb, NULL);

	struct mkgui_column file_cols[] = {
		{ "Name", 350, MKGUI_CELL_ICON_TEXT },
		{ "Size", 100, MKGUI_CELL_SIZE },
	};
	mkgui_listview_setup(dlg, FD_ID_FILES, fd.entry_count, 2, file_cols, fd_file_row_cb, NULL);

	struct mkgui_split_data *sd = find_split_data(dlg, FD_ID_SPLIT);
	if(sd) {
		sd->ratio = 0.22f;
	}

	uint32_t running = 1;
	struct mkgui_event ev;
	while(running) {
		while(mkgui_poll(dlg, &ev)) {
			switch(ev.type) {
				case MKGUI_EVENT_CLOSE: {
					running = 0;
				} break;

				case MKGUI_EVENT_CLICK: {
					if(ev.id == FD_ID_BTN_CANCEL) {
						running = 0;

					} else if(ev.id == FD_ID_BTN_CONFIRM) {
						fd_confirm(dlg);
						if(fd.confirmed) {
							running = 0;
						}
					}
				} break;

				case MKGUI_EVENT_KEY: {
					if(ev.keysym == MKGUI_KEY_ESCAPE) {
						running = 0;

					} else if(ev.keysym == MKGUI_KEY_RETURN) {
						if(dlg->focus_id == FD_ID_PATH_INPUT) {
							fd_navigate(dlg, mkgui_input_get(dlg, FD_ID_PATH_INPUT));
						} else {
							fd_confirm(dlg);
							if(fd.confirmed) {
								running = 0;
							}
						}
					}
				} break;

				case MKGUI_EVENT_LISTVIEW_SELECT: {
					if(ev.id == FD_ID_BOOKMARKS) {
						if(ev.value >= 0 && ev.value < (int32_t)fd.bookmark_count) {
							fd_navigate(dlg, fd.bookmarks[ev.value].path);
						}

					} else if(ev.id == FD_ID_FILES) {
						uint32_t now = fd_get_time_ms();
						uint32_t is_dblclick = (ev.value == fd.last_click_row && (now - fd.last_click_time) < FD_DBLCLICK_MS);
						fd.last_click_row = ev.value;
						fd.last_click_time = now;

						if(is_dblclick && ev.value >= 0 && ev.value < (int32_t)fd.entry_count) {
							if(fd.entries[ev.value].is_dir) {
								fd_navigate_entry(dlg, ev.value);
							} else {
								fd_confirm(dlg);
								if(fd.confirmed) {
									running = 0;
								}
							}
						} else if(fd.mode == 1 && ev.value >= 0 && ev.value < (int32_t)fd.entry_count && !fd.entries[ev.value].is_dir) {
							mkgui_input_set(dlg, FD_ID_NAME_INPUT, fd.entries[ev.value].name);
						}
					}
				} break;

				default: break;
			}
		}

	}

	mkgui_destroy_child(dlg);
	dirty_all(ctx);

	return fd.confirmed ? fd_result_count : 0;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// [=]===^=[ mkgui_open_dialog ]==================================[=]
static uint32_t mkgui_open_dialog(struct mkgui_ctx *ctx) {
	return fd_run_dialog(ctx, 0, NULL);
}

// [=]===^=[ mkgui_save_dialog ]==================================[=]
static uint32_t mkgui_save_dialog(struct mkgui_ctx *ctx, const char *default_name) {
	return fd_run_dialog(ctx, 1, default_name);
}

// [=]===^=[ mkgui_dialog_path ]==================================[=]
static const char *mkgui_dialog_path(struct mkgui_ctx *ctx, uint32_t index) {
	(void)ctx;
	if(index >= fd_result_count) {
		return "";
	}
	return fd_results[index];
}
