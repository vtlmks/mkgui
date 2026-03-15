// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#include <wingdi.h>

struct mkgui_platform {
	HWND hwnd;
	HDC hdc_mem;
	HBITMAP hbmp;
	HBITMAP hbmp_old;
	HCURSOR cursor_default;
	HCURSOR cursor_h_resize;
	uint32_t cursor_active;
	uint32_t is_child;
	HWND parent_hwnd;
};

struct mkgui_popup_platform {
	HWND hwnd;
	HDC hdc_mem;
	HBITMAP hbmp;
	HBITMAP hbmp_old;
};

struct mkgui_glview_platform {
	HWND hwnd;
};
