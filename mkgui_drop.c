// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// Storage and URI parsing live in mkwin; these forward to it.
// mkgui_drop_enable is in mkgui.c, after the platform include.

// [=]===^=[ mkgui_drop_count ]==================================[=]
MKGUI_API uint32_t mkgui_drop_count(struct mkgui_window *win) {
	MKGUI_CHECK_VAL(win, 0);
	return mkwin_drop_count(win->mkwin);
}

// [=]===^=[ mkgui_drop_file ]===================================[=]
MKGUI_API const char *mkgui_drop_file(struct mkgui_window *win, uint32_t index) {
	MKGUI_CHECK_VAL(win, NULL);
	return mkwin_drop_path(win->mkwin, index);
}
