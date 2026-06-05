// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/shape.h>
#include <sys/shm.h>
#include <unistd.h>
#include <poll.h>
#include <sys/timerfd.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <fontconfig/fontconfig.h>

#define MKGUI_DEFERRED_SIZE 64

// Concurrent outgoing INCR clipboard transfers we can serve at once. One per
// requestor that pulls a selection too large for a single X request.
#define MKGUI_CLIP_INCR_MAX 8

// One in-flight INCR send: a snapshot of the selection data being streamed to
// a single requestor, chunk by chunk, as it deletes the property between
// chunks. data is owned (malloc'd) so a later clipboard change can't pull the
// rug out from under an active transfer.
struct mkgui_clip_incr {
	Window requestor;
	Atom property;
	Atom type;
	char *data;
	uint32_t len;
	uint32_t offset;
};

struct mkgui_platform_atoms {
	Atom wm_delete;
	Atom clipboard;
	Atom utf8_string;
	Atom targets;
	Atom incr;
	Atom mkgui_clip_prop;
	Atom net_wm_pid;
	Atom xdnd_aware;
	Atom xdnd_enter;
	Atom xdnd_position;
	Atom xdnd_status;
	Atom xdnd_drop;
	Atom xdnd_finished;
	Atom xdnd_leave;
	Atom xdnd_action_copy;
	Atom xdnd_selection;
	Atom text_uri_list;
};

struct mkgui_platform {
	Display *dpy;
	Window root;
	Window win;
	int32_t screen;
	GC gc;
	Visual *visual;
	Colormap colormap;
	uint32_t depth;
	struct mkgui_platform_atoms atoms;
	Window xdnd_source;
	uint32_t xdnd_uri_ok;
	XShmSegmentInfo shm;
	XImage *img;
	Cursor cursor_default;
	Cursor cursor_h_resize;
	Cursor cursor_v_resize;
	uint32_t cursor_active;
	uint32_t is_child;
	XIM xim;
	XIC xic;
	struct mkgui_plat_event deferred[MKGUI_DEFERRED_SIZE];
	uint32_t deferred_head;
	uint32_t deferred_tail;
	struct mkgui_clip_incr incr_send[MKGUI_CLIP_INCR_MAX];
	uint32_t incr_send_count;
};

struct mkgui_popup_platform {
	Window xwin;
	XShmSegmentInfo shm;
	XImage *img;
};

struct mkgui_glview_platform {
	Window xwin;
};
