// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// Filesystem abstraction for directory enumeration and path queries.
// Windows uses FindFirstFileA / GetFileAttributesExA; POSIX uses opendir /
// readdir / stat / access. Callers in mkgui_icon.c, mkgui_iconbrowser.c and
// mkgui_filedialog.c reach file metadata exclusively through this layer so
// they build identically under MinGW and MSVC/clang-cl as well as POSIX.

#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

#define MKGUI_DIR_NAME_MAX 260

struct mkgui_dir_entry {
	char name[MKGUI_DIR_NAME_MAX];
};

struct mkgui_dir {
#ifdef _WIN32
	HANDLE handle;
	uint32_t pending;
	WIN32_FIND_DATAA fd;
#else
	DIR *dir;
#endif
	struct mkgui_dir_entry entry;
};

struct mkgui_path_info {
	uint32_t is_dir;
	int64_t size;
	int64_t mtime;
};

// ---------------------------------------------------------------------------
// Directory enumeration
// ---------------------------------------------------------------------------

// [=]===^=[ mkgui_dir_open ]=========================================[=]
// Open a directory for iteration. Returns 1 on success, 0 on failure.
static int32_t mkgui_dir_open(struct mkgui_dir *d, const char *path) {
#ifdef _WIN32
	char pattern[MKGUI_PATH_MAX + 4];
	int32_t n = snprintf(pattern, sizeof(pattern), "%s\\*", path);
	if(n <= 0 || (uint32_t)n >= sizeof(pattern)) {
		d->handle = INVALID_HANDLE_VALUE;
		d->pending = 0;
		return 0;
	}
	d->handle = FindFirstFileA(pattern, &d->fd);
	if(d->handle == INVALID_HANDLE_VALUE) {
		d->pending = 0;
		return 0;
	}
	d->pending = 1;
	return 1;
#else
	d->dir = opendir(path);
	return d->dir ? 1 : 0;
#endif
}

// [=]===^=[ mkgui_dir_next ]=========================================[=]
// Advance the iterator; returns the next entry or NULL when exhausted.
static struct mkgui_dir_entry *mkgui_dir_next(struct mkgui_dir *d) {
#ifdef _WIN32
	if(d->pending) {
		d->pending = 0;
	} else {
		if(!FindNextFileA(d->handle, &d->fd)) {
			return NULL;
		}
	}
	snprintf(d->entry.name, sizeof(d->entry.name), "%s", d->fd.cFileName);
	return &d->entry;
#else
	struct dirent *de = readdir(d->dir);
	if(!de) {
		return NULL;
	}
	snprintf(d->entry.name, sizeof(d->entry.name), "%s", de->d_name);
	return &d->entry;
#endif
}

// [=]===^=[ mkgui_dir_close ]========================================[=]
static void mkgui_dir_close(struct mkgui_dir *d) {
#ifdef _WIN32
	if(d->handle != INVALID_HANDLE_VALUE) {
		FindClose(d->handle);
		d->handle = INVALID_HANDLE_VALUE;
	}
	d->pending = 0;
#else
	if(d->dir) {
		closedir(d->dir);
		d->dir = NULL;
	}
#endif
}

// ---------------------------------------------------------------------------
// Path queries
// ---------------------------------------------------------------------------

// [=]===^=[ mkgui_path_stat ]========================================[=]
// Fill is_dir/size/mtime for a path. Returns 1 on success, 0 on failure.
static int32_t mkgui_path_stat(const char *path, struct mkgui_path_info *out) {
#ifdef _WIN32
	WIN32_FILE_ATTRIBUTE_DATA ad;
	if(!GetFileAttributesExA(path, GetFileExInfoStandard, &ad)) {
		return 0;
	}
	out->is_dir = (ad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
	ULARGE_INTEGER sz;
	sz.LowPart = ad.nFileSizeLow;
	sz.HighPart = ad.nFileSizeHigh;
	out->size = (int64_t)sz.QuadPart;
	ULARGE_INTEGER ft;
	ft.LowPart = ad.ftLastWriteTime.dwLowDateTime;
	ft.HighPart = ad.ftLastWriteTime.dwHighDateTime;
	// FILETIME is 100ns ticks since 1601-01-01 UTC; offset to 1970 epoch.
	out->mtime = (int64_t)((ft.QuadPart - 116444736000000000ULL) / 10000000ULL);
	return 1;
#else
	struct stat st;
	if(stat(path, &st) != 0) {
		return 0;
	}
	out->is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
	out->size = (int64_t)st.st_size;
	out->mtime = (int64_t)st.st_mtime;
	return 1;
#endif
}

// [=]===^=[ mkgui_path_is_dir ]======================================[=]
static uint32_t mkgui_path_is_dir(const char *path) {
#ifdef _WIN32
	DWORD a = GetFileAttributesA(path);
	return (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY)) ? 1 : 0;
#else
	struct stat st;
	return (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) ? 1 : 0;
#endif
}

// [=]===^=[ mkgui_path_exists ]======================================[=]
static uint32_t mkgui_path_exists(const char *path) {
#ifdef _WIN32
	return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES ? 1 : 0;
#else
	return access(path, F_OK) == 0 ? 1 : 0;
#endif
}

// [=]===^=[ mkgui_path_readable ]====================================[=]
// Win32 has no true R_OK without opening the file; presence is what the
// existing callers actually need (they attempt to read the file right after),
// so we report readability as existence there. POSIX uses access(R_OK).
static uint32_t mkgui_path_readable(const char *path) {
#ifdef _WIN32
	return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES ? 1 : 0;
#else
	return access(path, R_OK) == 0 ? 1 : 0;
#endif
}
