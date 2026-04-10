// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT
//
// extract_icons -- copy SVG icons to a flat output directory
//
// Usage:
//   extract_icons <icons_list.txt> <output_dir> [theme_dir] [size]
//
// Reads the editor-generated icon list and an optional _extra.txt file
// for icons loaded dynamically in user code. The extra file is derived
// from the main list path: myapp_icons.txt -> myapp_icons_extra.txt
//
// The output directory is cleaned and rebuilt from scratch every run.
//
// Line formats in both files:
//   source_path<tab>icon_name   -- copy source_path directly
//   <tab>icon_name              -- resolve from theme_dir
//   icon_name                   -- resolve from theme_dir (plain name)
//
// mkgui's required internal icons are always included and resolved
// from theme_dir.

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

struct icon_entry {
	char name[MAX_NAME];
	char source[MAX_PATH];
};

static struct icon_entry icon_list[MAX_ICONS];
static uint32_t icon_count;

static const char *mkgui_required_icons[] = {
	"folder", "folder-open", "folder-new", "folder-download", "folder-documents",
	"text-x-generic", "text-x-csrc", "text-x-c++src", "text-x-script", "text-x-python",
	"text-html", "text-xml", "text-css",
	"image-x-generic",
	"application-pdf", "package-x-generic", "application-x-object",
	"dialog-information", "dialog-warning", "dialog-error", "help-browser",
	"user-home", "user-desktop", "drive-harddisk",
	"go-previous", "go-next", "go-up",
	"edit-find", "edit-copy", "edit-paste", "edit-cut", "edit-undo", "edit-redo",
	"document-save", "document-open", "document-new",
	NULL
};

// [=]===^=[ icon_name_exists ]=======================================[=]
static int icon_name_exists(const char *name) {
	for(uint32_t i = 0; i < icon_count; ++i) {
		if(strcmp(icon_list[i].name, name) == 0) {
			return 1;
		}
	}
	return 0;
}

// [=]===^=[ add_icon ]===============================================[=]
static void add_icon(const char *name, const char *source) {
	if(icon_count >= MAX_ICONS) {
		return;
	}

	if(icon_name_exists(name)) {
		return;
	}
	strncpy(icon_list[icon_count].name, name, MAX_NAME - 1);
	icon_list[icon_count].name[MAX_NAME - 1] = '\0';
	if(source) {
		strncpy(icon_list[icon_count].source, source, MAX_PATH - 1);
		icon_list[icon_count].source[MAX_PATH - 1] = '\0';
	} else {
		icon_list[icon_count].source[0] = '\0';
	}
	++icon_count;
}

// [=]===^=[ read_icon_list ]=========================================[=]
static int read_icon_list(const char *path, int required) {
	FILE *fp = fopen(path, "r");
	if(!fp) {
		if(required) {
			fprintf(stderr, "error: cannot open %s: %s\n", path, strerror(errno));
		}
		return !required;
	}
	uint32_t before = icon_count;
	char line[MAX_PATH + MAX_NAME + 8];
	while(fgets(line, sizeof(line), fp)) {
		uint32_t len = (uint32_t)strlen(line);
		while(len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' || line[len - 1] == ' ')) {
			line[--len] = '\0';
		}

		if(len == 0 || line[0] == '#') {
			continue;
		}
		char *tab = strchr(line, '\t');
		if(tab) {
			*tab = '\0';
			char *source = line;
			char *name = tab + 1;
			if(source[0]) {
				add_icon(name, source);
			} else {
				add_icon(name, NULL);
			}
		} else {
			add_icon(line, NULL);
		}
	}
	fclose(fp);
	fprintf(stderr, "extract_icons: read %u icons from %s\n", icon_count - before, path);
	return 1;
}

// [=]===^=[ copy_file ]==============================================[=]
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

// [=]===^=[ find_icon_in_theme ]=====================================[=]
static int find_icon_in_theme(const char *theme_dir, const char *name, int preferred_size, char *out_path) {
	static const int sizes[] = { 16, 22, 24, 32, 48, 64 };
	static const int num_sizes = 6;
	static const char *categories[] = {
		"actions", "places", "status", "devices", "mimetypes",
		"emblems", "apps", "panel", "emotes", "categories", NULL
	};

	for(const char **cat = categories; *cat; ++cat) {
		snprintf(out_path, MAX_PATH, "%s/scalable/%s/%s.svg", theme_dir, *cat, name);
		if(access(out_path, R_OK) == 0) {
			return 1;
		}
	}

	for(const char **cat = categories; *cat; ++cat) {
		snprintf(out_path, MAX_PATH, "%s/%dx%d/%s/%s.svg", theme_dir, preferred_size, preferred_size, *cat, name);
		if(access(out_path, R_OK) == 0) {
			return 1;
		}
	}

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

	for(const char **cat = categories; *cat; ++cat) {
		snprintf(out_path, MAX_PATH, "%s/%s/%d/%s.svg", theme_dir, *cat, preferred_size, name);
		if(access(out_path, R_OK) == 0) {
			return 1;
		}
	}

	return 0;
}

// [=]===^=[ resolve_symlink ]========================================[=]
static void resolve_symlink(char *path) {
	char resolved[MAX_PATH];
	char *r = realpath(path, resolved);
	if(r) {
		strncpy(path, resolved, MAX_PATH - 1);
		path[MAX_PATH - 1] = '\0';
	}
}

// [=]===^=[ clean_dir ]==============================================[=]
static void clean_dir(const char *path) {
	DIR *d = opendir(path);
	if(!d) {
		return;
	}
	struct dirent *ent;
	while((ent = readdir(d)) != NULL) {
		if(ent->d_name[0] == '.') {
			continue;
		}
		uint32_t len = (uint32_t)strlen(ent->d_name);
		if(len < 5 || strcmp(ent->d_name + len - 4, ".svg") != 0) {
			continue;
		}
		char full[MAX_PATH];
		snprintf(full, sizeof(full), "%s/%s", path, ent->d_name);
		unlink(full);
	}
	closedir(d);
}

// [=]===^=[ ensure_dir ]=============================================[=]
static void ensure_dir(const char *path) {
	struct stat st;
	if(stat(path, &st) != 0) {
		mkdir(path, 0755);
	}
}

// [=]===^=[ derive_extra_path ]======================================[=]
static void derive_extra_path(const char *list_path, char *extra_path, uint32_t extra_size) {
	const char *dot = strrchr(list_path, '.');
	size_t base_len = dot ? (size_t)(dot - list_path) : strlen(list_path);
	snprintf(extra_path, extra_size, "%.*s_extra.txt", (int)base_len, list_path);
}

// [=]===^=[ main ]===================================================[=]
int main(int argc, char **argv) {
	if(argc < 3) {
		fprintf(stderr, "usage: extract_icons <icons_list.txt> <output_dir> [theme_dir] [size]\n\n");
		fprintf(stderr, "  icons_list.txt - icon list from mkgui editor (source_path<tab>name)\n");
		fprintf(stderr, "  output_dir     - flat output directory for SVG icons (cleaned each run)\n");
		fprintf(stderr, "  theme_dir      - icon theme for resolving missing sources and required icons\n");
		fprintf(stderr, "  size           - preferred icon size for theme lookup (default: 16)\n\n");
		fprintf(stderr, "Also reads <base>_extra.txt if it exists, for icons loaded dynamically\n");
		fprintf(stderr, "in user code. The extra file is never touched by the editor.\n");
		return 1;
	}

	const char *list_path = argv[1];
	const char *output_dir = argv[2];
	const char *theme_dir = argc > 3 ? argv[3] : NULL;
	int preferred_size = argc > 4 ? atoi(argv[4]) : 16;

	if(preferred_size <= 0) {
		preferred_size = 16;
	}

	if(!read_icon_list(list_path, 1)) {
		return 1;
	}

	char extra_path[MAX_PATH];
	derive_extra_path(list_path, extra_path, sizeof(extra_path));
	read_icon_list(extra_path, 0);

	uint32_t user_count = icon_count;
	for(const char **req = mkgui_required_icons; *req; ++req) {
		add_icon(*req, NULL);
	}

	fprintf(stderr, "extract_icons: %u user + %u required = %u total\n", user_count, icon_count - user_count, icon_count);

	ensure_dir(output_dir);
	clean_dir(output_dir);

	uint32_t copied = 0;
	uint32_t resolved = 0;
	uint32_t missing = 0;

	for(uint32_t i = 0; i < icon_count; ++i) {
		char dst_path[MAX_PATH];
		snprintf(dst_path, sizeof(dst_path), "%s/%s.svg", output_dir, icon_list[i].name);

		if(icon_list[i].source[0]) {
			char src[MAX_PATH];
			strncpy(src, icon_list[i].source, MAX_PATH - 1);
			src[MAX_PATH - 1] = '\0';
			resolve_symlink(src);
			if(copy_file(src, dst_path)) {
				++copied;
			} else {
				fprintf(stderr, "  error copying %s from %s\n", icon_list[i].name, icon_list[i].source);
				++missing;
			}
		} else if(theme_dir) {
			char src_path[MAX_PATH];
			if(find_icon_in_theme(theme_dir, icon_list[i].name, preferred_size, src_path)) {
				resolve_symlink(src_path);
				if(copy_file(src_path, dst_path)) {
					++resolved;
				} else {
					fprintf(stderr, "  error copying %s\n", icon_list[i].name);
					++missing;
				}
			} else {
				fprintf(stderr, "  missing: %s\n", icon_list[i].name);
				++missing;
			}
		} else {
			fprintf(stderr, "  missing: %s (no source path, no theme_dir)\n", icon_list[i].name);
			++missing;
		}
	}

	fprintf(stderr, "extract_icons: %u copied (source), %u resolved (theme), %u missing\n", copied, resolved, missing);
	fprintf(stderr, "extract_icons: output: %s\n", output_dir);
	return 0;
}
