// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT
//
// extract_icons -- extract SVG icons from a Freedesktop icon theme
//
// Usage: extract_icons <theme_dir> <icons_used.txt> <output_dir> [size]
//
// Reads icon names from icons_used.txt (one per line), appends mkgui's
// internal required names, finds matching SVGs in the theme directory,
// resolves symlinks, and copies to a flat output directory.
//
// The optional size argument (default: 16) selects the preferred source
// size. Falls back to other sizes if not found at the preferred size.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#define MAX_ICONS 8192
#define MAX_NAME  64
#define MAX_PATH  4096

static char icon_names[MAX_ICONS][MAX_NAME];
static uint32_t icon_count;

static const char *mkgui_required_icons[] = {
	"folder", "folder-open", "folder-new", "folder-download", "folder-documents",
	"text-x-generic", "text-x-csrc", "text-x-script", "text-x-python",
	"image-x-generic",
	"dialog-information", "dialog-warning", "dialog-error", "help-browser",
	"user-home", "user-desktop", "drive-harddisk",
	"go-previous", "go-next", "go-up",
	"edit-find", "edit-copy", "edit-paste", "edit-cut", "edit-undo", "edit-redo",
	"document-save", "document-open", "document-new",
	NULL
};

static int icon_name_exists(const char *name) {
	for(uint32_t i = 0; i < icon_count; ++i) {
		if(strcmp(icon_names[i], name) == 0) {
			return 1;
		}
	}
	return 0;
}

static void add_icon_name(const char *name) {
	if(icon_count >= MAX_ICONS) {
		return;
	}
	if(icon_name_exists(name)) {
		return;
	}
	strncpy(icon_names[icon_count], name, MAX_NAME - 1);
	icon_names[icon_count][MAX_NAME - 1] = '\0';
	++icon_count;
}

static int read_icon_list(const char *path) {
	FILE *fp = fopen(path, "r");
	if(!fp) {
		fprintf(stderr, "error: cannot open %s: %s\n", path, strerror(errno));
		return 0;
	}
	char line[MAX_NAME];
	while(fgets(line, sizeof(line), fp)) {
		uint32_t len = (uint32_t)strlen(line);
		while(len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' || line[len - 1] == ' ')) {
			line[--len] = '\0';
		}
		if(len > 0 && line[0] != '#') {
			add_icon_name(line);
		}
	}
	fclose(fp);
	return 1;
}

static int copy_file(const char *src, const char *dst) {
	FILE *in = fopen(src, "rb");
	if(!in) {
		return 0;
	}
	FILE *out = fopen(dst, "wb");
	if(!out) {
		fclose(in);
		return 0;
	}
	char buf[8192];
	size_t n;
	while((n = fread(buf, 1, sizeof(buf), in)) > 0) {
		fwrite(buf, 1, n, out);
	}
	fclose(in);
	fclose(out);
	return 1;
}

static int find_icon_in_theme(const char *theme_dir, const char *name, int preferred_size, char *out_path) {
	static const int sizes[] = { 16, 22, 24, 32, 48, 64 };
	static const int num_sizes = 6;
	static const char *categories[] = {
		"actions", "places", "status", "devices", "mimetypes",
		"emblems", "apps", "panel", "emotes", "categories", NULL
	};

	// Try preferred size first
	for(const char **cat = categories; *cat; ++cat) {
		snprintf(out_path, MAX_PATH, "%s/%dx%d/%s/%s.svg", theme_dir, preferred_size, preferred_size, *cat, name);
		if(access(out_path, R_OK) == 0) {
			return 1;
		}
	}

	// Try scalable
	for(const char **cat = categories; *cat; ++cat) {
		snprintf(out_path, MAX_PATH, "%s/scalable/%s/%s.svg", theme_dir, *cat, name);
		if(access(out_path, R_OK) == 0) {
			return 1;
		}
	}

	// Try other sizes, closest first
	for(int si = 0; si < num_sizes; ++si) {
		if(sizes[si] == preferred_size) {
			continue;
		}
		for(const char **cat = categories; *cat; ++cat) {
			snprintf(out_path, MAX_PATH, "%s/%dx%d/%s/%s.svg", theme_dir, sizes[si], sizes[si], *cat, name);
			if(access(out_path, R_OK) == 0) {
				return 1;
			}
		}
	}

	// Try category-first layout (elementary style)
	for(const char **cat = categories; *cat; ++cat) {
		snprintf(out_path, MAX_PATH, "%s/%s/%d/%s.svg", theme_dir, *cat, preferred_size, name);
		if(access(out_path, R_OK) == 0) {
			return 1;
		}
	}

	return 0;
}

static void resolve_symlink(char *path) {
	char resolved[MAX_PATH];
	char *r = realpath(path, resolved);
	if(r) {
		strncpy(path, resolved, MAX_PATH - 1);
		path[MAX_PATH - 1] = '\0';
	}
}

static void ensure_dir(const char *path) {
	struct stat st;
	if(stat(path, &st) != 0) {
		mkdir(path, 0755);
	}
}

int main(int argc, char **argv) {
	if(argc < 4) {
		fprintf(stderr, "usage: extract_icons <theme_dir> <icons_used.txt> <output_dir> [size] [toolbar_size]\n");
		fprintf(stderr, "  theme_dir      - icon theme root (e.g. /usr/share/icons/Papirus)\n");
		fprintf(stderr, "  icons_used.txt - list of icon names, one per line\n");
		fprintf(stderr, "  output_dir     - output directory (creates toolbar/ subdirectory)\n");
		fprintf(stderr, "  size           - preferred small icon size (default: 16)\n");
		fprintf(stderr, "  toolbar_size   - preferred toolbar icon size (default: 22)\n");
		return 1;
	}

	const char *theme_dir = argv[1];
	const char *list_path = argv[2];
	const char *output_dir = argv[3];
	int preferred_size = argc > 4 ? atoi(argv[4]) : 16;
	int toolbar_size = argc > 5 ? atoi(argv[5]) : 22;

	if(preferred_size <= 0) {
		preferred_size = 16;
	}
	if(toolbar_size <= 0) {
		toolbar_size = 22;
	}

	if(!read_icon_list(list_path)) {
		return 1;
	}

	uint32_t user_count = icon_count;
	for(const char **req = mkgui_required_icons; *req; ++req) {
		add_icon_name(*req);
	}

	fprintf(stderr, "extract_icons: %u user icons + %u mkgui required = %u total\n",
		user_count, icon_count - user_count, icon_count);

	ensure_dir(output_dir);

	uint32_t found = 0;
	uint32_t missing = 0;
	for(uint32_t i = 0; i < icon_count; ++i) {
		char src_path[MAX_PATH];
		if(find_icon_in_theme(theme_dir, icon_names[i], preferred_size, src_path)) {
			resolve_symlink(src_path);
			char dst_path[MAX_PATH];
			snprintf(dst_path, sizeof(dst_path), "%s/%s.svg", output_dir, icon_names[i]);
			if(copy_file(src_path, dst_path)) {
				++found;
			} else {
				fprintf(stderr, "  error copying %s\n", icon_names[i]);
			}
		} else {
			fprintf(stderr, "  missing: %s\n", icon_names[i]);
			++missing;
		}
	}

	fprintf(stderr, "extract_icons: %u found, %u missing (small %dx%d)\n", found, missing, preferred_size, preferred_size);

	char toolbar_dir[MAX_PATH];
	snprintf(toolbar_dir, sizeof(toolbar_dir), "%s/toolbar", output_dir);
	ensure_dir(toolbar_dir);

	uint32_t tb_found = 0;
	uint32_t tb_missing = 0;
	for(uint32_t i = 0; i < icon_count; ++i) {
		char src_path[MAX_PATH];
		if(find_icon_in_theme(theme_dir, icon_names[i], toolbar_size, src_path)) {
			resolve_symlink(src_path);
			char dst_path[MAX_PATH];
			snprintf(dst_path, sizeof(dst_path), "%s/%s.svg", toolbar_dir, icon_names[i]);
			if(copy_file(src_path, dst_path)) {
				++tb_found;
			}
		} else {
			++tb_missing;
		}
	}

	fprintf(stderr, "extract_icons: %u found, %u missing (toolbar %dx%d)\n", tb_found, tb_missing, toolbar_size, toolbar_size);
	fprintf(stderr, "extract_icons: output: %s\n", output_dir);
	return missing > 0 ? 1 : 0;
}
