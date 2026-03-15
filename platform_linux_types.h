// Copyright (c) 2026, mkgui contributors
// SPDX-License-Identifier: MIT

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <X11/extensions/XShm.h>
#include <sys/shm.h>
#include <poll.h>
#include <ft2build.h>
#include FT_FREETYPE_H

struct mkgui_platform {
	Display *dpy;
	Window root;
	Window win;
	int32_t screen;
	GC gc;
	Visual *visual;
	uint32_t depth;
	Atom wm_delete;
	Atom clipboard;
	Atom utf8_string;
	Atom targets;
	Atom mkgui_clip_prop;
	XShmSegmentInfo shm;
	XImage *img;
	Cursor cursor_default;
	Cursor cursor_h_resize;
	uint32_t cursor_active;
	uint32_t is_child;
};

struct mkgui_popup_platform {
	Window xwin;
	XShmSegmentInfo shm;
	XImage *img;
};

struct mkgui_glview_platform {
	Window xwin;
};
