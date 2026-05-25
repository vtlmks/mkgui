// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ drop_free ]==========================================[=]
static void drop_free(struct mkgui_window *win) {
	for(uint32_t i = 0; i < win->drop_count; ++i) {
		free(win->drop_files[i]);
		win->drop_files[i] = NULL;
	}
	win->drop_count = 0;
}

// [=]===^=[ drop_add_path ]======================================[=]
static void drop_add_path(struct mkgui_window *win, char *path) {
	if(win->drop_count >= MKGUI_DROP_MAX) {
		return;
	}
	uint32_t len = (uint32_t)strlen(path);
	char *copy = (char *)malloc(len + 1);
	if(!copy) {
		return;
	}
	memcpy(copy, path, len + 1);
	win->drop_files[win->drop_count++] = copy;
}

// [=]===^=[ drop_uri_decode ]====================================[=]
static void drop_uri_decode(char *dst, char *src, uint32_t dst_size) {
	uint32_t di = 0;
	for(uint32_t si = 0; src[si] && di + 1 < dst_size; ++si) {
		if(src[si] == '%' && src[si + 1] && src[si + 2]) {
			char hex[3] = { src[si + 1], src[si + 2], 0 };
			dst[di++] = (char)strtoul(hex, NULL, 16);
			si += 2;
		} else {
			dst[di++] = src[si];
		}
	}
	dst[di] = '\0';
}

// [=]===^=[ drop_parse_uri_list ]================================[=]
static void drop_parse_uri_list(struct mkgui_window *win, char *data, uint32_t len) {
	drop_free(win);
	char *p = data;
	char *end = data + len;
	while(p < end) {
		char *eol = p;
		while(eol < end && *eol != '\n' && *eol != '\r') {
			++eol;
		}
		uint32_t line_len = (uint32_t)(eol - p);
		if(line_len > 0 && p[0] != '#') {
			char line[4096];
			if(line_len >= sizeof(line)) {
				line_len = sizeof(line) - 1;
			}
			memcpy(line, p, line_len);
			line[line_len] = '\0';
			if(line_len > 7 && strncmp(line, "file://", 7) == 0) {
				char decoded[4096];
				drop_uri_decode(decoded, line + 7, sizeof(decoded));
				drop_add_path(win, decoded);
			} else {
				drop_add_path(win, line);
			}
		}
		p = eol;
		while(p < end && (*p == '\n' || *p == '\r')) {
			++p;
		}
	}
}

// mkgui_drop_enable is implemented in mkgui.c after the platform include

// [=]===^=[ mkgui_drop_count ]==================================[=]
MKGUI_API uint32_t mkgui_drop_count(struct mkgui_window *win) {
	MKGUI_CHECK_VAL(win, 0);
	return win->drop_count;
}

// [=]===^=[ mkgui_drop_file ]===================================[=]
MKGUI_API const char *mkgui_drop_file(struct mkgui_window *win, uint32_t index) {
	MKGUI_CHECK_VAL(win, NULL);
	if(index >= win->drop_count) {
		return NULL;
	}
	return win->drop_files[index];
}
