// Copyright (c) 2026, Peter Fors
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

#define FD_MAX_FILES        4096
#define FD_PATH_SIZE        4096
#define FD_MAX_BOOKMARKS    32
#define FD_MAX_RESULTS      64
#define FD_MAX_FILTERS      32
#define FD_HISTORY_SIZE     32
#define FD_INIT_W           780
#define FD_INIT_H           520
#define FD_DBLCLICK_MS      400
#define FD_TOOLBAR_H        28
#define FD_BOTTOM_H         72
#define FD_FILTER_MAX 64

// ---------------------------------------------------------------------------
// Widget IDs
// ---------------------------------------------------------------------------

enum {
	FD_ID_WINDOW = 1,
	FD_ID_TOOLBAR,
	FD_ID_BTN_BACK,
	FD_ID_BTN_FWD,
	FD_ID_BTN_UP,
	FD_ID_TB_SEP1,
	FD_ID_PATHBAR,
	FD_ID_SPLIT,
	FD_ID_BOOKMARKS,
	FD_ID_FILES,
	FD_ID_NAME_LABEL,
	FD_ID_NAME_INPUT,
	FD_ID_FILTER_LABEL,
	FD_ID_FILTER_DROP,
	FD_ID_CHK_HIDDEN,
	FD_ID_BTN_NEWFOLDER,
	FD_ID_BTN_CONFIRM,
	FD_ID_BTN_CANCEL,
	FD_ID_SEARCH_LABEL,
	FD_ID_CONTENT_VBOX,
	FD_ID_NAME_HBOX,
	FD_ID_BTN_HBOX,
	FD_ID_SPACER,
};

// ---------------------------------------------------------------------------
// Internal types
// ---------------------------------------------------------------------------

struct fd_entry {
	char name[260];
	uint32_t is_dir;
	int64_t size;
	int64_t mtime;
};

struct fd_bookmark {
	char label[64];
	char path[FD_PATH_SIZE];
};

struct fd_state {
	uint32_t mode;
	char path[FD_PATH_SIZE];

	struct fd_entry entries[FD_MAX_FILES];
	uint32_t entry_count;

	struct fd_bookmark bookmarks[FD_MAX_BOOKMARKS];
	uint32_t bookmark_count;

	uint32_t confirmed;

	uint32_t show_hidden;

	char history[FD_HISTORY_SIZE][FD_PATH_SIZE];
	uint32_t history_pos;
	uint32_t history_count;

	int32_t sort_col;
	int32_t sort_dir;

	struct mkgui_file_filter *filters;
	uint32_t filter_count;
	uint32_t active_filter;

	char filter[FD_FILTER_MAX];
	uint32_t filter_len;
	uint32_t filtered[FD_MAX_FILES];
	uint32_t filtered_count;
};

static struct fd_state *fd;
static char (*fd_results)[FD_PATH_SIZE + 260];
static uint32_t fd_result_count;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// [=]===^=[ fd_strcasecmp ]======================================[=]
static int32_t fd_strcasecmp(char *a, char *b) {
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
static void fd_url_decode(char *dst, char *src, uint32_t dst_size) {
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

// [=]===^=[ fd_icon_for_ext ]=====================================[=]
static char *fd_icon_for_ext(char *dot) {
	if(strcmp(dot, ".c") == 0 || strcmp(dot, ".h") == 0) {
		return "text-x-csrc";
	}
	if(strcmp(dot, ".cpp") == 0 || strcmp(dot, ".cc") == 0 || strcmp(dot, ".cxx") == 0 || strcmp(dot, ".hpp") == 0) {
		return "text-x-c++src";
	}
	if(strcmp(dot, ".sh") == 0 || strcmp(dot, ".bash") == 0 || strcmp(dot, ".zsh") == 0) {
		return "text-x-script";
	}
	if(strcmp(dot, ".py") == 0) {
		return "text-x-python";
	}
	if(strcmp(dot, ".js") == 0 || strcmp(dot, ".ts") == 0 || strcmp(dot, ".json") == 0) {
		return "text-x-script";
	}
	if(strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0) {
		return "text-html";
	}
	if(strcmp(dot, ".xml") == 0 || strcmp(dot, ".svg") == 0) {
		return "text-xml";
	}
	if(strcmp(dot, ".css") == 0) {
		return "text-css";
	}
	if(strcmp(dot, ".png") == 0 || strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0 || strcmp(dot, ".gif") == 0 || strcmp(dot, ".bmp") == 0 || strcmp(dot, ".webp") == 0 || strcmp(dot, ".ico") == 0) {
		return "image-x-generic";
	}
	if(strcmp(dot, ".pdf") == 0) {
		return "application-pdf";
	}
	if(strcmp(dot, ".zip") == 0 || strcmp(dot, ".tar") == 0 || strcmp(dot, ".gz") == 0 || strcmp(dot, ".bz2") == 0 || strcmp(dot, ".xz") == 0 || strcmp(dot, ".7z") == 0 || strcmp(dot, ".rar") == 0) {
		return "package-x-generic";
	}
	if(strcmp(dot, ".o") == 0 || strcmp(dot, ".so") == 0 || strcmp(dot, ".a") == 0 || strcmp(dot, ".dll") == 0) {
		return "application-x-object";
	}
	return NULL;
}

// [=]===^=[ fd_icon_for_name ]====================================[=]
static char *fd_icon_for_name(char *name, uint32_t is_dir) {
	if(is_dir) {
		return "folder";
	}
	char *dot = strrchr(name, '.');
	if(dot) {
		char *specific = fd_icon_for_ext(dot);
		if(specific && icon_find_idx(specific) >= 0) {
			return specific;
		}
	}
	return "text-x-generic";
}

// [=]===^=[ fd_path_exists ]======================================[=]
static uint32_t fd_path_exists(char *path) {
#ifdef _WIN32
	return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
	return access(path, F_OK) == 0;
#endif
}

// [=]===^=[ fd_get_home ]=========================================[=]
static char *fd_get_home(void) {
#ifdef _WIN32
	char *h = getenv("USERPROFILE");
	if(h) {
		return h;
	}
	return "C:\\";
#else
	char *h = getenv("HOME");
	if(h) {
		return h;
	}
	return "/";
#endif
}

// ---------------------------------------------------------------------------
// Filter matching
// ---------------------------------------------------------------------------

// [=]===^=[ fd_match_pattern ]====================================[=]
static uint32_t fd_match_pattern(char *name, char *pattern) {
	if(!pattern || pattern[0] == '\0' || strcmp(pattern, "*") == 0 || strcmp(pattern, "*.*") == 0) {
		return 1;
	}

	char pat_buf[1024];
	snprintf(pat_buf, sizeof(pat_buf), "%s", pattern);

	char *tok = pat_buf;
	while(*tok) {
		while(*tok == ' ' || *tok == ';') {
			++tok;
		}
		if(*tok == '\0') {
			break;
		}

		char *end = tok;
		while(*end && *end != ';' && *end != ' ') {
			++end;
		}
		char saved = *end;
		*end = '\0';

		char *pat = tok;
		if(pat[0] == '*' && pat[1] == '.') {
			char *ext = pat + 1;
			char *dot = strrchr(name, '.');
			if(dot && fd_strcasecmp(dot, ext) == 0) {
				return 1;
			}
		} else if(strcmp(pat, "*") == 0) {
			return 1;
		}

		*end = saved;
		tok = end;
	}
	return 0;
}

// ---------------------------------------------------------------------------
// Directory scanning
// ---------------------------------------------------------------------------

// [=]===^=[ fd_compare_entries ]=================================[=]
static int fd_compare_entries(const void *a, const void *b) {
	struct fd_entry *ea = (struct fd_entry *)a;
	struct fd_entry *eb = (struct fd_entry *)b;
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

	int32_t result = 0;
	switch(fd->sort_col) {
		case 0: {
			result = fd_strcasecmp(ea->name, eb->name);
		} break;

		case 1: {
			if(ea->size < eb->size) {
				result = -1;
			} else if(ea->size > eb->size) {
				result = 1;
			} else {
				result = fd_strcasecmp(ea->name, eb->name);
			}
		} break;

		case 2: {
			if(ea->mtime < eb->mtime) {
				result = -1;
			} else if(ea->mtime > eb->mtime) {
				result = 1;
			} else {
				result = fd_strcasecmp(ea->name, eb->name);
			}
		} break;

		default: {
			result = fd_strcasecmp(ea->name, eb->name);
		} break;
	}

	return result * fd->sort_dir;
}

// [=]===^=[ fd_scan_dir ]========================================[=]
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
static void fd_scan_dir(void) {
	fd->entry_count = 0;

	char *filter_pat = NULL;
	if(fd->filters && fd->active_filter < fd->filter_count) {
		filter_pat = fd->filters[fd->active_filter].pattern;
	}

#ifdef _WIN32
	uint32_t plen = (uint32_t)strlen(fd->path);
	if(plen == 0) {
		return;
	}

	if(!(plen == 3 && fd->path[1] == ':' && fd->path[2] == '\\')) {
		struct fd_entry *e = &fd->entries[fd->entry_count++];
		snprintf(e->name, sizeof(e->name), "..");
		e->is_dir = 1;
		e->size = 0;
		e->mtime = 0;
	}

	char pattern[FD_PATH_SIZE];
	snprintf(pattern, sizeof(pattern), "%s\\*", fd->path);
	WIN32_FIND_DATAA wfd;
	HANDLE hf = FindFirstFileA(pattern, &wfd);
	if(hf == INVALID_HANDLE_VALUE) {
		return;
	}
	do {
		if(fd->entry_count >= FD_MAX_FILES) {
			break;
		}
		if(wfd.cFileName[0] == '.') {
			if(!fd->show_hidden) {
				continue;
			}
			if(strcmp(wfd.cFileName, ".") == 0 || strcmp(wfd.cFileName, "..") == 0) {
				continue;
			}
		}
		uint32_t is_dir = (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
		if(!is_dir && filter_pat && !fd_match_pattern(wfd.cFileName, filter_pat)) {
			continue;
		}
		struct fd_entry *e = &fd->entries[fd->entry_count];
		snprintf(e->name, sizeof(e->name), "%s", wfd.cFileName);
		e->is_dir = is_dir;
		e->size = ((int64_t)wfd.nFileSizeHigh << 32) | wfd.nFileSizeLow;
		ULARGE_INTEGER ftime;
		ftime.LowPart = wfd.ftLastWriteTime.dwLowDateTime;
		ftime.HighPart = wfd.ftLastWriteTime.dwHighDateTime;
		e->mtime = (int64_t)((ftime.QuadPart - 116444736000000000ULL) / 10000000ULL);
		++fd->entry_count;
	} while(FindNextFileA(hf, &wfd));
	FindClose(hf);
#else
	DIR *dir = opendir(fd->path);
	if(!dir) {
		return;
	}

	if(strcmp(fd->path, "/") != 0) {
		struct fd_entry *e = &fd->entries[fd->entry_count++];
		snprintf(e->name, sizeof(e->name), "..");
		e->is_dir = 1;
		e->size = 0;
		e->mtime = 0;
	}

	struct dirent *de;
	while((de = readdir(dir)) != NULL && fd->entry_count < FD_MAX_FILES) {
		if(de->d_name[0] == '.') {
			if(!fd->show_hidden) {
				continue;
			}
			if(strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
				continue;
			}
		}

		char fullpath[FD_PATH_SIZE + 260];
		snprintf(fullpath, sizeof(fullpath), "%s/%s", fd->path, de->d_name);

		struct stat st;
		uint32_t is_dir = 0;
		int64_t fsize = 0;
		int64_t fmtime = 0;
		if(stat(fullpath, &st) == 0) {
			is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
			fsize = st.st_size;
			fmtime = (int64_t)st.st_mtime;
		}

		if(!is_dir && filter_pat && !fd_match_pattern(de->d_name, filter_pat)) {
			continue;
		}

		struct fd_entry *e = &fd->entries[fd->entry_count];
		snprintf(e->name, sizeof(e->name), "%s", de->d_name);
		e->is_dir = is_dir;
		e->size = fsize;
		e->mtime = fmtime;
		++fd->entry_count;
	}
	closedir(dir);
#endif

	if(fd->entry_count > 1) {
		qsort(fd->entries, fd->entry_count, sizeof(fd->entries[0]), fd_compare_entries);
	}
}
#pragma GCC diagnostic pop

// ---------------------------------------------------------------------------
// Bookmarks
// ---------------------------------------------------------------------------

// [=]===^=[ fd_add_bookmark ]====================================[=]
static void fd_add_bookmark(char *label, char *path) {
	if(fd->bookmark_count >= FD_MAX_BOOKMARKS) {
		return;
	}
	struct fd_bookmark *b = &fd->bookmarks[fd->bookmark_count++];
	snprintf(b->label, sizeof(b->label), "%.63s", label);
	snprintf(b->path, sizeof(b->path), "%s", path);
}

// [=]===^=[ fd_load_bookmarks ]==================================[=]
static void fd_load_bookmarks(void) {
	fd->bookmark_count = 0;

	char *home = fd_get_home();
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

#ifndef _WIN32
	snprintf(buf, sizeof(buf), "%s/.config/gtk-3.0/bookmarks", home);
	FILE *f = fopen(buf, "r");
	if(f) {
		char line[4096];
		while(fgets(line, sizeof(line), f) != NULL && fd->bookmark_count < FD_MAX_BOOKMARKS) {
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

	FILE *mounts = fopen("/proc/mounts", "r");
	if(mounts) {
		char mline[4096];
		while(fgets(mline, sizeof(mline), mounts) != NULL && fd->bookmark_count < FD_MAX_BOOKMARKS) {
			char *nl = strchr(mline, '\n');
			if(nl) {
				*nl = '\0';
			}

			char dev[512], mp[512], fs[128];
			if(sscanf(mline, "%511s %511s %127s", dev, mp, fs) < 3) {
				continue;
			}

			if(strcmp(fs, "proc") == 0 || strcmp(fs, "sysfs") == 0 || strcmp(fs, "tmpfs") == 0 ||
			   strcmp(fs, "devtmpfs") == 0 || strcmp(fs, "devpts") == 0 || strcmp(fs, "cgroup") == 0 ||
			   strcmp(fs, "cgroup2") == 0 || strcmp(fs, "securityfs") == 0 || strcmp(fs, "debugfs") == 0 ||
			   strcmp(fs, "configfs") == 0 || strcmp(fs, "fusectl") == 0 || strcmp(fs, "hugetlbfs") == 0 ||
			   strcmp(fs, "mqueue") == 0 || strcmp(fs, "pstore") == 0 || strcmp(fs, "binfmt_misc") == 0 ||
			   strcmp(fs, "autofs") == 0 || strcmp(fs, "efivarfs") == 0 || strcmp(fs, "tracefs") == 0 ||
			   strcmp(fs, "bpf") == 0 || strcmp(fs, "nsfs") == 0 || strcmp(fs, "overlay") == 0 ||
			   strcmp(fs, "squashfs") == 0 || strcmp(fs, "fuse.portal") == 0) {
				continue;
			}

			uint32_t is_media = (strncmp(mp, "/media/", 7) == 0);
			uint32_t is_mnt = (strncmp(mp, "/mnt/", 5) == 0);
			if(!is_media && !is_mnt) {
				continue;
			}

			uint32_t already = 0;
			for(uint32_t bi = 0; bi < fd->bookmark_count; ++bi) {
				if(strcmp(fd->bookmarks[bi].path, mp) == 0) {
					already = 1;
					break;
				}
			}
			if(already) {
				continue;
			}

			char *label = strrchr(mp, '/');
			label = label ? label + 1 : mp;
			fd_add_bookmark(label, mp);
		}
		fclose(mounts);
	}
#endif
}

// ---------------------------------------------------------------------------
// Navigation history
// ---------------------------------------------------------------------------

// [=]===^=[ fd_history_push ]=====================================[=]
static void fd_history_push(char *path) {
	if(fd->history_count > 0 && fd->history_pos > 0 && strcmp(fd->history[fd->history_pos - 1], path) == 0) {
		return;
	}

	if(fd->history_pos < fd->history_count) {
		fd->history_count = fd->history_pos;
	}

	if(fd->history_count >= FD_HISTORY_SIZE) {
		memmove(&fd->history[0], &fd->history[1], (FD_HISTORY_SIZE - 1) * FD_PATH_SIZE);
		--fd->history_count;
		if(fd->history_pos > 0) {
			--fd->history_pos;
		}
	}

	snprintf(fd->history[fd->history_count], FD_PATH_SIZE, "%s", path);
	++fd->history_count;
	fd->history_pos = fd->history_count;
}

// [=]===^=[ fd_history_can_back ]=================================[=]
static uint32_t fd_history_can_back(void) {
	return fd->history_pos > 1;
}

// [=]===^=[ fd_history_can_fwd ]=================================[=]
static uint32_t fd_history_can_fwd(void) {
	return fd->history_pos < fd->history_count;
}

// ---------------------------------------------------------------------------
// Filter-as-you-type
// ---------------------------------------------------------------------------

// [=]===^=[ fd_strcasestr ]=======================================[=]
static uint32_t fd_strcasestr(char *haystack, char *needle, uint32_t needle_len) {
	if(needle_len == 0) {
		return 1;
	}
	uint32_t hlen = (uint32_t)strlen(haystack);
	if(needle_len > hlen) {
		return 0;
	}
	for(uint32_t i = 0; i <= hlen - needle_len; ++i) {
		uint32_t match = 1;
		for(uint32_t j = 0; j < needle_len; ++j) {
			int32_t ca = haystack[i + j];
			int32_t cb = needle[j];
			if(ca >= 'A' && ca <= 'Z') {
				ca += 32;
			}
			if(cb >= 'A' && cb <= 'Z') {
				cb += 32;
			}
			if(ca != cb) {
				match = 0;
				break;
			}
		}
		if(match) {
			return 1;
		}
	}
	return 0;
}

// [=]===^=[ fd_apply_filter ]=====================================[=]
static void fd_apply_filter(struct mkgui_ctx *dlg) {
	fd->filtered_count = 0;
	for(uint32_t i = 0; i < fd->entry_count; ++i) {
		if(fd->entries[i].is_dir) {
			fd->filtered[fd->filtered_count++] = i;
			continue;
		}
		if(fd->filter_len == 0 || fd_strcasestr(fd->entries[i].name, fd->filter, fd->filter_len)) {
			fd->filtered[fd->filtered_count++] = i;
		}
	}
	struct mkgui_listview_data *lv = find_listv_data(dlg, FD_ID_FILES);
	if(lv) {
		lv->row_count = fd->filtered_count;
		lv->selected_row = -1;
		lv->scroll_y = 0;
		memset(lv->multi_sel, 0, sizeof(lv->multi_sel));
		lv->multi_sel_count = 0;
	}
	if(fd->filter_len > 0) {
		char label[80];
		snprintf(label, sizeof(label), "Filter: %s", fd->filter);
		mkgui_label_set(dlg, FD_ID_SEARCH_LABEL, label);
		int32_t widx = find_widget_idx(dlg, FD_ID_SEARCH_LABEL);
		if(widx >= 0) {
			dlg->widgets[widx].flags &= ~MKGUI_HIDDEN;
		}
	} else {
		int32_t widx = find_widget_idx(dlg, FD_ID_SEARCH_LABEL);
		if(widx >= 0) {
			dlg->widgets[widx].flags |= MKGUI_HIDDEN;
		}
	}
	dirty_all(dlg);
}

// [=]===^=[ fd_clear_filter ]=====================================[=]
static void fd_clear_filter(struct mkgui_ctx *dlg) {
	if(fd->filter_len == 0) {
		return;
	}
	fd->filter_len = 0;
	fd->filter[0] = '\0';
	fd_apply_filter(dlg);
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

// [=]===^=[ fd_navigate ]========================================[=]
static void fd_navigate(struct mkgui_ctx *dlg, char *newpath) {
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

	snprintf(fd->path, sizeof(fd->path), "%s", resolved);
	fd_history_push(fd->path);
	fd_scan_dir();

	mkgui_pathbar_set(dlg, FD_ID_PATHBAR, fd->path);

	fd->filter_len = 0;
	fd->filter[0] = '\0';
	fd_apply_filter(dlg);
}

// [=]===^=[ fd_navigate_entry ]==================================[=]
static void fd_navigate_entry(struct mkgui_ctx *dlg, int32_t idx) {
	if(idx < 0 || idx >= (int32_t)fd->entry_count) {
		return;
	}
	struct fd_entry *e = &fd->entries[idx];
	if(!e->is_dir) {
		return;
	}

	char newpath[FD_PATH_SIZE + 260];
	if(strcmp(e->name, "..") == 0) {
		char *last = strrchr(fd->path, '/');
#ifdef _WIN32
		char *last_bs = strrchr(fd->path, '\\');
		if(last_bs && (!last || last_bs > last)) {
			last = last_bs;
		}
#endif
		if(last && last != fd->path) {
			snprintf(newpath, sizeof(newpath), "%.*s", (int)(last - fd->path), fd->path);
		} else {
			snprintf(newpath, sizeof(newpath), "/");
		}
	} else {
#ifdef _WIN32
		snprintf(newpath, sizeof(newpath), "%s\\%s", fd->path, e->name);
#else
		if(strcmp(fd->path, "/") == 0) {
			snprintf(newpath, sizeof(newpath), "/%s", e->name);
		} else {
			snprintf(newpath, sizeof(newpath), "%s/%s", fd->path, e->name);
		}
#endif
	}
	fd_navigate(dlg, newpath);
}

// [=]===^=[ fd_navigate_up ]=====================================[=]
static void fd_navigate_up(struct mkgui_ctx *dlg) {
	char *last = strrchr(fd->path, '/');
#ifdef _WIN32
	char *last_bs = strrchr(fd->path, '\\');
	if(last_bs && (!last || last_bs > last)) {
		last = last_bs;
	}
	if(!last) {
		return;
	}
	if(last == fd->path + 2 && fd->path[1] == ':') {
		char newpath[4] = { fd->path[0], ':', '\\', '\0' };
		fd_navigate(dlg, newpath);
		return;
	}
#else
	if(!last) {
		return;
	}
	if(last == fd->path) {
		if(strcmp(fd->path, "/") != 0) {
			fd_navigate(dlg, "/");
		}
		return;
	}
#endif
	char newpath[FD_PATH_SIZE + 260];
	snprintf(newpath, sizeof(newpath), "%.*s", (int)(last - fd->path), fd->path);
	fd_navigate(dlg, newpath);
}

// [=]===^=[ fd_navigate_back ]====================================[=]
static void fd_navigate_back(struct mkgui_ctx *dlg) {
	if(!fd_history_can_back()) {
		return;
	}
	--fd->history_pos;
	char *target = fd->history[fd->history_pos - 1];
	snprintf(fd->path, sizeof(fd->path), "%s", target);
	fd_scan_dir();
	mkgui_pathbar_set(dlg, FD_ID_PATHBAR, fd->path);
	fd->filter_len = 0;
	fd->filter[0] = '\0';
	fd_apply_filter(dlg);
}

// [=]===^=[ fd_navigate_fwd ]=====================================[=]
static void fd_navigate_fwd(struct mkgui_ctx *dlg) {
	if(!fd_history_can_fwd()) {
		return;
	}
	char *target = fd->history[fd->history_pos];
	++fd->history_pos;
	snprintf(fd->path, sizeof(fd->path), "%s", target);
	fd_scan_dir();
	mkgui_pathbar_set(dlg, FD_ID_PATHBAR, fd->path);
	fd->filter_len = 0;
	fd->filter[0] = '\0';
	fd_apply_filter(dlg);
}

// ---------------------------------------------------------------------------
// Filter extension
// ---------------------------------------------------------------------------

// [=]===^=[ fd_append_filter_ext ]================================[=]
static void fd_append_filter_ext(char *name, uint32_t name_size) {
	if(!fd->filters || fd->active_filter >= fd->filter_count) {
		return;
	}
	if(strchr(name, '.')) {
		return;
	}
	char *pat = fd->filters[fd->active_filter].pattern;
	if(!pat) {
		return;
	}

	while(*pat == ' ' || *pat == ';') {
		++pat;
	}
	if(pat[0] == '*' && pat[1] == '.') {
		uint32_t nlen = (uint32_t)strlen(name);
		char *ext = pat + 1;
		uint32_t elen = 0;
		while(ext[elen] && ext[elen] != ';' && ext[elen] != ' ') {
			++elen;
		}
		if(nlen + elen < name_size - 1) {
			memcpy(&name[nlen], ext, elen);
			name[nlen + elen] = '\0';
		}
	}
}

// [=]===^=[ fd_filtered_entry ]===================================[=]
static struct fd_entry *fd_filtered_entry(uint32_t row) {
	if(row >= fd->filtered_count) {
		return NULL;
	}
	return &fd->entries[fd->filtered[row]];
}

// ---------------------------------------------------------------------------
// Results
// ---------------------------------------------------------------------------

// [=]===^=[ fd_build_results ]===================================[=]
static void fd_build_results(struct mkgui_ctx *dlg) {
	fd_result_count = 0;

	if(fd->mode == 1) {
		char *name = mkgui_input_get(dlg, FD_ID_NAME_INPUT);
		if(name && name[0] != '\0') {
			char fname[260];
			snprintf(fname, sizeof(fname), "%s", name);
			fd_append_filter_ext(fname, sizeof(fname));
			if(strcmp(fd->path, "/") == 0) {
				snprintf(fd_results[0], sizeof(fd_results[0]), "/%s", fname);
			} else {
				snprintf(fd_results[0], sizeof(fd_results[0]), "%s/%s", fd->path, fname);
			}
			fd_result_count = 1;
		}

	} else {
		struct mkgui_listview_data *lv = find_listv_data(dlg, FD_ID_FILES);
		if(!lv) {
			return;
		}

		if(lv->multi_sel_count > 0) {
			for(uint32_t i = 0; i < lv->multi_sel_count && fd_result_count < FD_MAX_RESULTS; ++i) {
				int32_t row = lv->multi_sel[i];
				struct fd_entry *e = fd_filtered_entry((uint32_t)row);
				if(e && !e->is_dir) {
					if(strcmp(fd->path, "/") == 0) {
						snprintf(fd_results[fd_result_count], sizeof(fd_results[0]), "/%s", e->name);
					} else {
						snprintf(fd_results[fd_result_count], sizeof(fd_results[0]), "%s/%s", fd->path, e->name);
					}
					++fd_result_count;
				}
			}

		} else {
			struct fd_entry *e = fd_filtered_entry((uint32_t)lv->selected_row);
			if(e && !e->is_dir) {
				if(strcmp(fd->path, "/") == 0) {
					snprintf(fd_results[0], sizeof(fd_results[0]), "/%s", e->name);
				} else {
					snprintf(fd_results[0], sizeof(fd_results[0]), "%s/%s", fd->path, e->name);
				}
				fd_result_count = 1;
			}
		}
	}
}

// [=]===^=[ fd_confirm ]=========================================[=]
static uint32_t fd_confirm(struct mkgui_ctx *dlg) {
	fd_build_results(dlg);
	if(fd_result_count == 0) {
		return 0;
	}

	if(fd->mode == 1 && fd_path_exists(fd_results[0])) {
		char *fname = strrchr(fd_results[0], '/');
		if(!fname) {
			fname = fd_results[0];
		} else {
			++fname;
		}
		char msg[FD_PATH_SIZE + 320];
		snprintf(msg, sizeof(msg), "Overwrite existing file %s?", fname);
		if(!mkgui_confirm_dialog(dlg, "Confirm Save", msg)) {
			return 0;
		}
	}

	fd->confirmed = 1;
	return 1;
}

// ---------------------------------------------------------------------------
// Listview callbacks
// ---------------------------------------------------------------------------

// [=]===^=[ fd_bookmark_row_cb ]==================================[=]
static void fd_bookmark_row_cb(uint32_t row, uint32_t col, char *buf, uint32_t buf_size, void *userdata) {
	(void)userdata;
	(void)col;
	if(row < fd->bookmark_count) {
		char *icon = "folder";
		if(strcmp(fd->bookmarks[row].label, "Home") == 0) {
			icon = "user-home";
		} else if(strcmp(fd->bookmarks[row].label, "Desktop") == 0) {
			icon = "user-desktop";
		} else if(strcmp(fd->bookmarks[row].label, "Downloads") == 0) {
			icon = "folder-download";
		} else if(strcmp(fd->bookmarks[row].label, "Documents") == 0) {
			icon = "folder-documents";
		} else {
			uint32_t is_mount = 0;
			if(strncmp(fd->bookmarks[row].path, "/media/", 7) == 0 || strncmp(fd->bookmarks[row].path, "/mnt/", 5) == 0) {
				is_mount = 1;
			}
			if(is_mount) {
				icon = "drive-harddisk";
			}
		}
		snprintf(buf, buf_size, "%s\t%s", icon, fd->bookmarks[row].label);
	} else {
		buf[0] = '\0';
	}
}

// [=]===^=[ fd_file_row_cb ]=====================================[=]
static void fd_file_row_cb(uint32_t row, uint32_t col, char *buf, uint32_t buf_size, void *userdata) {
	(void)userdata;
	struct fd_entry *e = fd_filtered_entry(row);
	if(!e) {
		buf[0] = '\0';
		return;
	}

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

		case 2: {
			if(e->mtime > 0) {
				snprintf(buf, buf_size, "%lld", (long long)e->mtime);
			} else {
				buf[0] = '\0';
			}
		} break;

		default: {
			buf[0] = '\0';
		} break;
	}
}

// [=]===^=[ fd_update_name_from_selection ]=======================[=]
static void fd_update_name_from_selection(struct mkgui_ctx *dlg) {
	struct mkgui_listview_data *lv = find_listv_data(dlg, FD_ID_FILES);
	if(!lv) {
		return;
	}

	if(lv->multi_sel_count > 0) {
		char combined[2048] = {0};
		uint32_t pos = 0;
		for(uint32_t i = 0; i < lv->multi_sel_count; ++i) {
			int32_t row = lv->multi_sel[i];
			struct fd_entry *e = fd_filtered_entry((uint32_t)row);
			if(e && !e->is_dir) {
				uint32_t has_space = (strchr(e->name, ' ') != NULL);
				if(pos > 0 && pos < sizeof(combined) - 1) {
					combined[pos++] = ' ';
				}
				if(has_space && pos < sizeof(combined) - 2) {
					combined[pos++] = '"';
				}
				uint32_t nlen = (uint32_t)strlen(e->name);
				for(uint32_t j = 0; j < nlen && pos < sizeof(combined) - 2; ++j) {
					combined[pos++] = e->name[j];
				}
				if(has_space && pos < sizeof(combined) - 1) {
					combined[pos++] = '"';
				}
			}
		}
		combined[pos] = '\0';
		mkgui_input_set(dlg, FD_ID_NAME_INPUT, combined);

	} else {
		struct fd_entry *e = fd_filtered_entry((uint32_t)lv->selected_row);
		if(e && !e->is_dir) {
			mkgui_input_set(dlg, FD_ID_NAME_INPUT, e->name);
		}
	}
}

// [=]===^=[ fd_new_folder ]=======================================[=]
static void fd_new_folder(struct mkgui_ctx *dlg) {
	char name[256] = {0};
	if(!mkgui_input_dialog(dlg, "New Folder", "Name:", "", name, sizeof(name))) {
		return;
	}
	if(name[0] == '\0') {
		return;
	}

	char fullpath[FD_PATH_SIZE + 260];
	snprintf(fullpath, sizeof(fullpath), "%s/%s", fd->path, name);

#ifdef _WIN32
	CreateDirectoryA(fullpath, NULL);
#else
	mkdir(fullpath, 0755);
#endif

	fd_scan_dir();
	fd_apply_filter(dlg);
}

// ---------------------------------------------------------------------------
// Dialog runner
// ---------------------------------------------------------------------------

// [=]===^=[ fd_run_dialog ]=======================================[=]
static uint32_t fd_run_dialog(struct mkgui_ctx *ctx, uint32_t mode, struct mkgui_file_dialog_opts *opts) {
	fd = (struct fd_state *)calloc(1, sizeof(struct fd_state));
	if(!fd) {
		return 0;
	}
	if(!fd_results) {
		fd_results = (char (*)[FD_PATH_SIZE + 260])calloc(FD_MAX_RESULTS, FD_PATH_SIZE + 260);
		if(!fd_results) {
			free(fd);
			fd = NULL;
			return 0;
		}
	}
	fd_result_count = 0;
	fd->mode = mode;
	fd->sort_col = 0;
	fd->sort_dir = 1;

	if(opts && opts->start_path && opts->start_path[0] != '\0') {
		snprintf(fd->path, sizeof(fd->path), "%s", opts->start_path);
	} else {
		snprintf(fd->path, sizeof(fd->path), "%s", fd_get_home());
	}

	if(opts && opts->filters && opts->filter_count > 0) {
		fd->filters = opts->filters;
		fd->filter_count = opts->filter_count;
	}

	fd_load_bookmarks();
	fd_history_push(fd->path);
	fd_scan_dir();

	popup_destroy_all(ctx);

	uint32_t multi = (mode == 0 && opts && opts->multi_select) ? MKGUI_LISTVIEW_MULTI_SELECT : 0;
	char *confirm_label = (mode == 0) ? "Open" : "Save";
	char *title = (mode == 0) ? "Open File" : "Save File";

	uint32_t has_filters = (fd->filter_count > 0) ? 0 : MKGUI_HIDDEN;

	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   FD_ID_WINDOW,        "",              "",             0,                  FD_INIT_W, FD_INIT_H, 0, 0, 0),

		MKGUI_W(MKGUI_TOOLBAR,  FD_ID_TOOLBAR,        "",              "",             FD_ID_WINDOW,       0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   FD_ID_BTN_BACK,       "",              "go-previous",  FD_ID_TOOLBAR,      0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   FD_ID_BTN_FWD,        "",              "go-next",      FD_ID_TOOLBAR,      0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   FD_ID_BTN_UP,         "",              "go-up",        FD_ID_TOOLBAR,      0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   FD_ID_TB_SEP1,        "",              "",             FD_ID_TOOLBAR,      0, 0, 0, MKGUI_BUTTON_SEPARATOR, 0),
		MKGUI_W(MKGUI_BUTTON,   FD_ID_BTN_NEWFOLDER,  "",              "folder-new",   FD_ID_TOOLBAR,      0, 0, 0, 0, 0),

		MKGUI_W(MKGUI_PATHBAR,  FD_ID_PATHBAR,        "",              "",             FD_ID_WINDOW,       0, 0, 0, 0, 0),

		MKGUI_W(MKGUI_VBOX,     FD_ID_CONTENT_VBOX,   "",              "",             FD_ID_WINDOW,       0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_VSPLIT,   FD_ID_SPLIT,          "",              "",             FD_ID_CONTENT_VBOX, 0, 0, 0, 0, 1),
		MKGUI_W(MKGUI_LISTVIEW, FD_ID_BOOKMARKS,      "",              "",             FD_ID_SPLIT,        0, 0, MKGUI_REGION_LEFT, 0, 0),
		MKGUI_W(MKGUI_LISTVIEW, FD_ID_FILES,          "",              "",             FD_ID_SPLIT,        0, 0, MKGUI_REGION_RIGHT, multi | MKGUI_LISTVIEW_EDITABLE, 0),

		MKGUI_W(MKGUI_HBOX,     FD_ID_NAME_HBOX,      "",              "",             FD_ID_CONTENT_VBOX, 0, 24, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_LABEL,    FD_ID_NAME_LABEL,     "File name:",    "",             FD_ID_NAME_HBOX,    80, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_INPUT,    FD_ID_NAME_INPUT,     "",              "",             FD_ID_NAME_HBOX,    0, 0, 0, 0, 1),
		MKGUI_W(MKGUI_LABEL,    FD_ID_FILTER_LABEL,   "Filter:",       "",             FD_ID_NAME_HBOX,    50, 0, MKGUI_FIXED | has_filters, 0, 0),
		MKGUI_W(MKGUI_DROPDOWN, FD_ID_FILTER_DROP,    "",              "",             FD_ID_NAME_HBOX,    170, 0, MKGUI_FIXED | has_filters, 0, 0),

		MKGUI_W(MKGUI_HBOX,     FD_ID_BTN_HBOX,       "",              "",             FD_ID_CONTENT_VBOX, 0, 28, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_CHECKBOX, FD_ID_CHK_HIDDEN,     "Show hidden",   "",             FD_ID_BTN_HBOX,     110, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_LABEL,    FD_ID_SEARCH_LABEL,   "",              "edit-find",    FD_ID_BTN_HBOX,     200, 0, MKGUI_FIXED | MKGUI_HIDDEN, 0, 0),
		MKGUI_W(MKGUI_LABEL,    FD_ID_SPACER,         "",              "",             FD_ID_BTN_HBOX,     0, 0, 0, 0, 1),
		MKGUI_W(MKGUI_BUTTON,   FD_ID_BTN_CONFIRM,    "",              "",             FD_ID_BTN_HBOX,     80, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   FD_ID_BTN_CANCEL,     "Cancel",        "",             FD_ID_BTN_HBOX,     80, 0, MKGUI_FIXED, 0, 0),
	};

	uint32_t confirm_idx = 0;
	for(uint32_t i = 0; i < sizeof(widgets) / sizeof(widgets[0]); ++i) {
		if(widgets[i].id == FD_ID_BTN_CONFIRM) {
			confirm_idx = i;
			break;
		}
	}
	strncpy(widgets[0].label, title, MKGUI_MAX_TEXT - 1);
	strncpy(widgets[confirm_idx].label, confirm_label, MKGUI_MAX_TEXT - 1);

	uint32_t wcount = sizeof(widgets) / sizeof(widgets[0]);
	struct mkgui_ctx *dlg = mkgui_create_child(ctx, widgets, wcount, title, FD_INIT_W, FD_INIT_H);
	if(!dlg) {
		return 0;
	}
	mkgui_set_window_instance(dlg, "filedialog");

	mkgui_pathbar_set(dlg, FD_ID_PATHBAR, fd->path);

	if(mode == 1 && opts && opts->default_name) {
		mkgui_input_set(dlg, FD_ID_NAME_INPUT, opts->default_name);
	}

	struct mkgui_column bm_cols[] = {
		{ "Places", 160, MKGUI_CELL_ICON_TEXT },
	};
	mkgui_listview_setup(dlg, FD_ID_BOOKMARKS, fd->bookmark_count, 1, bm_cols, fd_bookmark_row_cb, NULL);

	struct mkgui_column file_cols[] = {
		{ "Name",     300, MKGUI_CELL_ICON_TEXT },
		{ "Size",     100, MKGUI_CELL_SIZE },
		{ "Modified", 150, MKGUI_CELL_DATE },
	};
	mkgui_listview_setup(dlg, FD_ID_FILES, fd->entry_count, 3, file_cols, fd_file_row_cb, NULL);
	fd_apply_filter(dlg);

	struct mkgui_split_data *sd = find_split_data(dlg, FD_ID_SPLIT);
	if(sd) {
		sd->ratio = 0.22f;
	}

	if(fd->filter_count > 0) {
		char *filter_labels[FD_MAX_FILTERS];
		uint32_t fc = fd->filter_count < FD_MAX_FILTERS ? fd->filter_count : FD_MAX_FILTERS;
		for(uint32_t i = 0; i < fc; ++i) {
			filter_labels[i] = fd->filters[i].label;
		}
		mkgui_dropdown_setup(dlg, FD_ID_FILTER_DROP, filter_labels, fc);
	}

	mkgui_set_tooltip(dlg, FD_ID_BTN_BACK, "Back");
	mkgui_set_tooltip(dlg, FD_ID_BTN_FWD, "Forward");
	mkgui_set_tooltip(dlg, FD_ID_BTN_UP, "Parent directory");
	mkgui_set_tooltip(dlg, FD_ID_BTN_NEWFOLDER, "New folder");

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
						if(fd_confirm(dlg)) {
							running = 0;
						}

					} else if(ev.id == FD_ID_BTN_BACK) {
						fd_navigate_back(dlg);

					} else if(ev.id == FD_ID_BTN_FWD) {
						fd_navigate_fwd(dlg);

					} else if(ev.id == FD_ID_BTN_UP) {
						fd_navigate_up(dlg);

					} else if(ev.id == FD_ID_BTN_NEWFOLDER) {
						fd_new_folder(dlg);
					}
				} break;

				case MKGUI_EVENT_CHECKBOX_CHANGED: {
					if(ev.id == FD_ID_CHK_HIDDEN) {
						fd->show_hidden = mkgui_checkbox_get(dlg, FD_ID_CHK_HIDDEN);
						fd_scan_dir();
						fd_apply_filter(dlg);
					}
				} break;

				case MKGUI_EVENT_DROPDOWN_CHANGED: {
					if(ev.id == FD_ID_FILTER_DROP) {
						fd->active_filter = (uint32_t)ev.value;
						fd_scan_dir();
						fd_apply_filter(dlg);
					}
				} break;

				case MKGUI_EVENT_PATHBAR_NAV: {
					if(ev.id == FD_ID_PATHBAR) {
						char seg_path[FD_PATH_SIZE];
						mkgui_pathbar_get_segment_path(dlg, FD_ID_PATHBAR, (uint32_t)ev.value, seg_path, sizeof(seg_path));
						if(seg_path[0] != '\0') {
							fd_navigate(dlg, seg_path);
						}
					}
				} break;

				case MKGUI_EVENT_PATHBAR_SUBMIT: {
					if(ev.id == FD_ID_PATHBAR) {
						char *new_path = mkgui_pathbar_get(dlg, FD_ID_PATHBAR);
						fd_navigate(dlg, new_path);
					}
				} break;

				case MKGUI_EVENT_LISTVIEW_SORT: {
					if(ev.id == FD_ID_FILES) {
						fd->sort_col = ev.col;
						fd->sort_dir = ev.value;
						fd_scan_dir();
						fd_apply_filter(dlg);
					}
				} break;

				case MKGUI_EVENT_KEY: {
					if(ev.keysym == MKGUI_KEY_ESCAPE) {
						if(fd->filter_len > 0) {
							fd_clear_filter(dlg);
						} else {
							running = 0;
						}

					} else if(ev.keysym == MKGUI_KEY_RETURN) {
						if(dlg->focus_id == FD_ID_NAME_INPUT) {
							if(fd_confirm(dlg)) {
								running = 0;
							}
						} else if(dlg->focus_id == FD_ID_FILES) {
							struct mkgui_listview_data *lv = find_listv_data(dlg, FD_ID_FILES);
							if(lv && lv->selected_row >= 0) {
								struct fd_entry *e = fd_filtered_entry((uint32_t)lv->selected_row);
								if(e && e->is_dir) {
									uint32_t real_idx = fd->filtered[(uint32_t)lv->selected_row];
									fd_navigate_entry(dlg, (int32_t)real_idx);
								} else if(e) {
									if(fd_confirm(dlg)) {
										running = 0;
									}
								}
							}
						} else {
							if(fd_confirm(dlg)) {
								running = 0;
							}
						}

					} else if(ev.keysym == MKGUI_KEY_DELETE && dlg->focus_id == FD_ID_FILES) {
						fd_clear_filter(dlg);

					} else if(ev.keysym == MKGUI_KEY_BACKSPACE && dlg->focus_id == FD_ID_FILES) {
						if(fd->filter_len > 0) {
							--fd->filter_len;
							fd->filter[fd->filter_len] = '\0';
							fd_apply_filter(dlg);
						} else {
							fd_navigate_up(dlg);
						}

					} else if(ev.keysym == MKGUI_KEY_F2 && dlg->focus_id == FD_ID_FILES) {
						struct mkgui_listview_data *lv = find_listv_data(dlg, FD_ID_FILES);
						if(lv && lv->selected_row >= 0) {
							struct fd_entry *e = fd_filtered_entry((uint32_t)lv->selected_row);
							if(e && strcmp(e->name, "..") != 0) {
								celledit_begin(dlg, FD_ID_FILES, lv->selected_row, 0, e->name);
							}
						}

					} else if(ev.keysym == 'l' && (ev.keymod & MKGUI_MOD_CONTROL)) {
						struct mkgui_pathbar_data *pb = find_pathbar_data(dlg, FD_ID_PATHBAR);
						if(pb && !pb->editing) {
							dlg->focus_id = FD_ID_PATHBAR;
							pathbar_enter_edit(pb);
							dirty_all(dlg);
						}

					} else if(dlg->focus_id == FD_ID_FILES && ev.keysym >= 32 && ev.keysym < 127 && !(ev.keymod & MKGUI_MOD_CONTROL)) {
						if(fd->filter_len < FD_FILTER_MAX - 1) {
							fd->filter[fd->filter_len++] = (char)ev.keysym;
							fd->filter[fd->filter_len] = '\0';
							fd_apply_filter(dlg);
						}
					}
				} break;

				case MKGUI_EVENT_LISTVIEW_SELECT: {
					if(ev.id == FD_ID_BOOKMARKS) {
						if(ev.value >= 0 && ev.value < (int32_t)fd->bookmark_count) {
							fd_navigate(dlg, fd->bookmarks[ev.value].path);
						}

					} else if(ev.id == FD_ID_FILES) {
						struct fd_entry *e = fd_filtered_entry((uint32_t)ev.value);
						if(e) {
							if(fd->mode == 0) {
								fd_update_name_from_selection(dlg);
							} else if(!e->is_dir) {
								mkgui_input_set(dlg, FD_ID_NAME_INPUT, e->name);
							}
						}
					}
				} break;

				case MKGUI_EVENT_LISTVIEW_DBLCLICK: {
					if(ev.id == FD_ID_FILES) {
						struct fd_entry *e = fd_filtered_entry((uint32_t)ev.value);
						if(e) {
							if(e->is_dir) {
								uint32_t real_idx = fd->filtered[(uint32_t)ev.value];
								fd_navigate_entry(dlg, (int32_t)real_idx);
							} else {
								if(fd->mode == 0) {
									fd_update_name_from_selection(dlg);
								} else {
									mkgui_input_set(dlg, FD_ID_NAME_INPUT, e->name);
								}
								if(fd_confirm(dlg)) {
									running = 0;
								}
							}
						}
					}
				} break;

				case MKGUI_EVENT_CELL_EDIT_COMMIT: {
					if(ev.id == FD_ID_FILES && ev.value >= 0) {
						struct fd_entry *e = fd_filtered_entry((uint32_t)ev.value);
						char *new_name = mkgui_cell_edit_get_text(dlg);
						if(e && new_name && new_name[0] && strcmp(e->name, new_name) != 0) {
							char old_path[FD_PATH_SIZE + 260];
							char new_path[FD_PATH_SIZE + 260];
							snprintf(old_path, sizeof(old_path), "%s/%s", fd->path, e->name);
							snprintf(new_path, sizeof(new_path), "%s/%s", fd->path, new_name);
							if(rename(old_path, new_path) == 0) {
								snprintf(e->name, sizeof(e->name), "%s", new_name);
								fd_apply_filter(dlg);
								dirty_all(dlg);
							}
						}
					}
				} break;

				case MKGUI_EVENT_INPUT_SUBMIT: {
					if(ev.id == FD_ID_NAME_INPUT) {
						if(fd_confirm(dlg)) {
							running = 0;
						}
					}
				} break;

				default: break;
			}
		}
		mkgui_wait(dlg);
	}

	mkgui_destroy_child(dlg);
	dirty_all(ctx);

	uint32_t result = fd->confirmed ? fd_result_count : 0;
	free(fd);
	fd = NULL;
	return result;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// [=]===^=[ mkgui_open_dialog ]==================================[=]
MKGUI_API uint32_t mkgui_open_dialog(struct mkgui_ctx *ctx, struct mkgui_file_dialog_opts *opts) {
	MKGUI_CHECK_VAL(ctx, 0);
	return fd_run_dialog(ctx, 0, opts);
}

// [=]===^=[ mkgui_save_dialog ]==================================[=]
MKGUI_API uint32_t mkgui_save_dialog(struct mkgui_ctx *ctx, struct mkgui_file_dialog_opts *opts) {
	MKGUI_CHECK_VAL(ctx, 0);
	return fd_run_dialog(ctx, 1, opts);
}

// [=]===^=[ mkgui_dialog_path ]==================================[=]
MKGUI_API char *mkgui_dialog_path(struct mkgui_ctx *ctx, uint32_t index) {
	MKGUI_CHECK_VAL(ctx, "");
	(void)ctx;
	if(index >= fd_result_count) {
		return "";
	}
	return fd_results[index];
}
