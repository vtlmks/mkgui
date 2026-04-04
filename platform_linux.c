// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// ---------------------------------------------------------------------------
// Framebuffer helpers
// ---------------------------------------------------------------------------

// [=]===^=[ platform_fb_create ]=================================[=]
static uint32_t platform_fb_create(struct mkgui_platform *plat, XShmSegmentInfo *shm, XImage **img, uint32_t **pixels, int32_t w, int32_t h) {
	*img = XShmCreateImage(plat->dpy, plat->visual, plat->depth, ZPixmap, NULL, shm, (uint32_t)w, (uint32_t)h);
	if(!*img) {
		return 0;
	}
	shm->shmid = shmget(IPC_PRIVATE, (size_t)((*img)->bytes_per_line * h), IPC_CREAT | 0600);
	if(shm->shmid < 0) {
		XDestroyImage(*img);
		*img = NULL;
		return 0;
	}
	shm->shmaddr = (char *)shmat(shm->shmid, NULL, 0);
	(*img)->data = shm->shmaddr;
	shm->readOnly = False;
	XShmAttach(plat->dpy, shm);
	XSync(plat->dpy, False);
	*pixels = (uint32_t *)shm->shmaddr;
	return 1;
}

// [=]===^=[ platform_fb_destroy ]================================[=]
static void platform_fb_destroy(struct mkgui_platform *plat, XShmSegmentInfo *shm, XImage *img) {
	if(!img) {
		return;
	}
	XShmDetach(plat->dpy, shm);
	XSync(plat->dpy, False);
	shmdt(shm->shmaddr);
	shmctl(shm->shmid, IPC_RMID, NULL);
	img->data = NULL;
	XDestroyImage(img);
}

// ---------------------------------------------------------------------------
// WM_CLASS
// ---------------------------------------------------------------------------

// [=]===^=[ platform_set_class_hint ]==============================[=]
static void platform_set_class_hint(struct mkgui_platform *plat, char *instance, char *cls) {
	XClassHint hint;
	hint.res_name = instance;
	hint.res_class = cls;
	XSetClassHint(plat->dpy, plat->win, &hint);
}

// ---------------------------------------------------------------------------
// Window icon
// ---------------------------------------------------------------------------

// [=]===^=[ platform_set_window_icon ]=============================[=]
static void platform_set_window_icon(struct mkgui_platform *plat, struct mkgui_icon_size *sizes, uint32_t count) {
	Atom net_wm_icon = XInternAtom(plat->dpy, "_NET_WM_ICON", False);
	size_t total = 0;
	for(uint32_t s = 0; s < count; ++s) {
		total += 2 + (size_t)(sizes[s].w * sizes[s].h);
	}
	unsigned long *buf = (unsigned long *)malloc(total * sizeof(unsigned long));
	if(!buf) {
		return;
	}
	size_t off = 0;
	for(uint32_t s = 0; s < count; ++s) {
		uint32_t npx = (uint32_t)(sizes[s].w * sizes[s].h);
		buf[off++] = (unsigned long)sizes[s].w;
		buf[off++] = (unsigned long)sizes[s].h;
		for(uint32_t i = 0; i < npx; ++i) {
			buf[off++] = (unsigned long)sizes[s].pixels[i];
		}
	}
	XChangeProperty(plat->dpy, plat->win, net_wm_icon, XA_CARDINAL, 32,
		PropModeReplace, (unsigned char *)buf, (int)total);
	XFlush(plat->dpy);
	free(buf);
}

// ---------------------------------------------------------------------------
// Platform init / destroy
// ---------------------------------------------------------------------------

// [=]===^=[ platform_init ]======================================[=]
static uint32_t platform_init(struct mkgui_ctx *ctx, const char *title, int32_t w, int32_t h) {
	struct mkgui_platform *plat = &ctx->plat;

	plat->dpy = XOpenDisplay(NULL);
	if(!plat->dpy) {
		fprintf(stderr, "mkgui: cannot open display\n");
		return 0;
	}

	plat->screen = DefaultScreen(plat->dpy);
	plat->root = RootWindow(plat->dpy, plat->screen);
	plat->visual = DefaultVisual(plat->dpy, plat->screen);
	plat->depth = (uint32_t)DefaultDepth(plat->dpy, plat->screen);

	XSetWindowAttributes wa;
	wa.background_pixmap = None;
	wa.bit_gravity = NorthWestGravity;
	plat->win = XCreateWindow(plat->dpy, plat->root, 0, 0, (uint32_t)w, (uint32_t)h, 0,
		(int)plat->depth, InputOutput, plat->visual, CWBackPixmap | CWBitGravity, &wa);
	XStoreName(plat->dpy, plat->win, title);
	platform_set_class_hint(plat, "main", ctx->app_class[0] ? ctx->app_class : "mkgui");

	plat->wm_delete = XInternAtom(plat->dpy, "WM_DELETE_WINDOW", False);
	plat->clipboard = XInternAtom(plat->dpy, "CLIPBOARD", False);
	plat->utf8_string = XInternAtom(plat->dpy, "UTF8_STRING", False);
	plat->targets = XInternAtom(plat->dpy, "TARGETS", False);
	plat->mkgui_clip_prop = XInternAtom(plat->dpy, "MKGUI_CLIP", False);
	plat->net_wm_pid = XInternAtom(plat->dpy, "_NET_WM_PID", False);
	plat->xdnd_aware = XInternAtom(plat->dpy, "XdndAware", False);
	plat->xdnd_enter = XInternAtom(plat->dpy, "XdndEnter", False);
	plat->xdnd_position = XInternAtom(plat->dpy, "XdndPosition", False);
	plat->xdnd_status = XInternAtom(plat->dpy, "XdndStatus", False);
	plat->xdnd_drop = XInternAtom(plat->dpy, "XdndDrop", False);
	plat->xdnd_finished = XInternAtom(plat->dpy, "XdndFinished", False);
	plat->xdnd_leave = XInternAtom(plat->dpy, "XdndLeave", False);
	plat->xdnd_action_copy = XInternAtom(plat->dpy, "XdndActionCopy", False);
	plat->xdnd_selection = XInternAtom(plat->dpy, "XdndSelection", False);
	plat->text_uri_list = XInternAtom(plat->dpy, "text/uri-list", False);
	XSetWMProtocols(plat->dpy, plat->win, &plat->wm_delete, 1);

	pid_t pid = getpid();
	XChangeProperty(plat->dpy, plat->win, plat->net_wm_pid, XA_CARDINAL, 32,
		PropModeReplace, (unsigned char *)&pid, 1);
	XSetWMClientMachine(plat->dpy, plat->win, &(XTextProperty){
		.value = (unsigned char *)"localhost", .encoding = XA_STRING,
		.format = 8, .nitems = 9
	});

	XSelectInput(plat->dpy, plat->win,
		ExposureMask | StructureNotifyMask |
		ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
		KeyPressMask | KeyReleaseMask | FocusChangeMask);

	plat->gc = XCreateGC(plat->dpy, plat->win, 0, NULL);

	ctx->win_w = w;
	ctx->win_h = h;

	platform_fb_create(plat, &plat->shm, &plat->img, &ctx->pixels, w, h);

	XSizeHints hints = {0};
	hints.flags = PMinSize;
	hints.min_width = 200;
	hints.min_height = 100;
	XSetWMNormalHints(plat->dpy, plat->win, &hints);

	plat->cursor_default = XCreateFontCursor(plat->dpy, XC_left_ptr);
	plat->cursor_h_resize = XCreateFontCursor(plat->dpy, XC_sb_h_double_arrow);
	plat->cursor_active = 0;

	plat->xim = XOpenIM(plat->dpy, NULL, NULL, NULL);
	if(plat->xim) {
		plat->xic = XCreateIC(plat->xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
			XNClientWindow, plat->win, XNFocusWindow, plat->win, NULL);
	}

	XMapWindow(plat->dpy, plat->win);
	XFlush(plat->dpy);

	return 1;
}

// [=]===^=[ platform_init_child ]=================================[=]
static uint32_t platform_init_child(struct mkgui_ctx *ctx, struct mkgui_ctx *parent, const char *title, int32_t w, int32_t h) {
	struct mkgui_platform *plat = &ctx->plat;
	struct mkgui_platform *pplat = &parent->plat;

	plat->dpy = pplat->dpy;
	plat->screen = pplat->screen;
	plat->root = pplat->root;
	plat->visual = pplat->visual;
	plat->depth = pplat->depth;
	plat->clipboard = pplat->clipboard;
	plat->utf8_string = pplat->utf8_string;
	plat->targets = pplat->targets;
	plat->mkgui_clip_prop = pplat->mkgui_clip_prop;
	plat->net_wm_pid = pplat->net_wm_pid;
	plat->is_child = 1;

	XSetWindowAttributes wa;
	wa.background_pixmap = None;
	wa.bit_gravity = NorthWestGravity;
	plat->win = XCreateWindow(plat->dpy, plat->root, 0, 0, (uint32_t)w, (uint32_t)h, 0,
		(int)plat->depth, InputOutput, plat->visual, CWBackPixmap | CWBitGravity, &wa);
	XStoreName(plat->dpy, plat->win, title);
	platform_set_class_hint(plat, "dialog", parent->app_class[0] ? parent->app_class : "mkgui");

	XSetTransientForHint(plat->dpy, plat->win, pplat->win);

	plat->wm_delete = XInternAtom(plat->dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(plat->dpy, plat->win, &plat->wm_delete, 1);

	pid_t pid = getpid();
	XChangeProperty(plat->dpy, plat->win, plat->net_wm_pid, XA_CARDINAL, 32,
		PropModeReplace, (unsigned char *)&pid, 1);
	XSetWMClientMachine(plat->dpy, plat->win, &(XTextProperty){
		.value = (unsigned char *)"localhost", .encoding = XA_STRING,
		.format = 8, .nitems = 9
	});

	XSelectInput(plat->dpy, plat->win,
		ExposureMask | StructureNotifyMask |
		ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
		KeyPressMask | KeyReleaseMask | FocusChangeMask);

	plat->gc = XCreateGC(plat->dpy, plat->win, 0, NULL);

	ctx->win_w = w;
	ctx->win_h = h;

	platform_fb_create(plat, &plat->shm, &plat->img, &ctx->pixels, w, h);

	plat->cursor_default = XCreateFontCursor(plat->dpy, XC_left_ptr);
	plat->cursor_h_resize = XCreateFontCursor(plat->dpy, XC_sb_h_double_arrow);
	plat->cursor_active = 0;

	plat->xim = pplat->xim;
	plat->xic = NULL;
	if(plat->xim) {
		plat->xic = XCreateIC(plat->xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
			XNClientWindow, plat->win, XNFocusWindow, plat->win, NULL);
	}

	XMapWindow(plat->dpy, plat->win);
	XFlush(plat->dpy);

	return 1;
}

// [=]===^=[ platform_destroy ]====================================[=]
static void platform_destroy(struct mkgui_ctx *ctx) {
	struct mkgui_platform *plat = &ctx->plat;
	platform_fb_destroy(plat, &plat->shm, plat->img);
	plat->img = NULL;
	XFreeCursor(plat->dpy, plat->cursor_default);
	XFreeCursor(plat->dpy, plat->cursor_h_resize);
	if(plat->xic) {
		XDestroyIC(plat->xic);
	}
	if(plat->xim && !plat->is_child) {
		XCloseIM(plat->xim);
	}
	XFreeGC(plat->dpy, plat->gc);
	XDestroyWindow(plat->dpy, plat->win);
	if(!plat->is_child) {
		XCloseDisplay(plat->dpy);
	}
}

// [=]===^=[ platform_set_cursor ]==================================[=]
static void platform_set_cursor(struct mkgui_ctx *ctx, uint32_t cursor_type) {
	struct mkgui_platform *plat = &ctx->plat;
	if(plat->cursor_active == cursor_type) {
		return;
	}
	plat->cursor_active = cursor_type;
	Cursor c = cursor_type ? plat->cursor_h_resize : plat->cursor_default;
	XDefineCursor(plat->dpy, plat->win, c);
}

// ---------------------------------------------------------------------------
// Framebuffer resize
// ---------------------------------------------------------------------------

// [=]===^=[ platform_fb_resize ]==================================[=]
static void platform_fb_resize(struct mkgui_ctx *ctx) {
	if(ctx->win_w <= 0 || ctx->win_h <= 0) {
		return;
	}
	struct mkgui_platform *plat = &ctx->plat;
	if(plat->img) {
		platform_fb_destroy(plat, &plat->shm, plat->img);
		plat->img = NULL;
	}
	platform_fb_create(plat, &plat->shm, &plat->img, &ctx->pixels, ctx->win_w, ctx->win_h);
}

// [=]===^=[ platform_detect_scale ]================================[=]
static float platform_detect_scale(struct mkgui_ctx *ctx) {
	char *env = getenv("MKGUI_SCALE");
	if(env) {
		char *end = NULL;
		double val = strtod(env, &end);
		if(end && end != env && val > 0.0) {
			if(end[0] == '%') {
				val /= 100.0;
			} else if((end[0] == 'd' || end[0] == 'D') && (end[1] == 'p' || end[1] == 'P') && (end[2] == 'i' || end[2] == 'I')) {
				val /= 96.0;
			}
			if(val >= 0.5 && val <= 4.0) {
				return (float)val;
			}
		}
	}

	Display *dpy = ctx->plat.dpy;

	XrmInitialize();
	char *rms = XResourceManagerString(dpy);
	if(rms) {
		XrmDatabase db = XrmGetStringDatabase(rms);
		if(db) {
			char *type = NULL;
			XrmValue val;
			if(XrmGetResource(db, "Xft.dpi", "Xft.Dpi", &type, &val)) {
				if(type && strcmp(type, "String") == 0 && val.addr) {
					double dpi = atof(val.addr);
					if(dpi > 48.0 && dpi < 960.0) {
						XrmDestroyDatabase(db);
						return (float)(dpi / 96.0);
					}
				}
			}
			XrmDestroyDatabase(db);
		}
	}

	int32_t width_px = DisplayWidth(dpy, ctx->plat.screen);
	int32_t width_mm = DisplayWidthMM(dpy, ctx->plat.screen);
	if(width_mm > 0) {
		float dpi = (float)width_px * 25.4f / (float)width_mm;
		if(dpi > 48.0f && dpi < 960.0f) {
			float scale = dpi / 96.0f;
			if(scale >= 1.2f) {
				return scale;
			}
		}
	}

	return 1.0f;
}

// [=]===^=[ platform_resize_window ]===============================[=]
static void platform_resize_window(struct mkgui_ctx *ctx, int32_t w, int32_t h) {
	XResizeWindow(ctx->plat.dpy, ctx->plat.win, (uint32_t)w, (uint32_t)h);
	ctx->win_w = w;
	ctx->win_h = h;
	platform_fb_resize(ctx);
}

// ---------------------------------------------------------------------------
// Blit / flush
// ---------------------------------------------------------------------------

// [=]===^=[ platform_blit ]=======================================[=]
static void platform_blit(struct mkgui_ctx *ctx) {
	struct mkgui_platform *plat = &ctx->plat;
	XShmPutImage(plat->dpy, plat->win, plat->gc, plat->img, 0, 0, 0, 0, (uint32_t)ctx->win_w, (uint32_t)ctx->win_h, False);
	XFlush(plat->dpy);
}

// [=]===^=[ platform_blit_region ]================================[=]
static void platform_blit_region(struct mkgui_ctx *ctx, int32_t x, int32_t y, int32_t w, int32_t h) {
	if(x < 0) { w += x; x = 0; }
	if(y < 0) { h += y; y = 0; }
	if(x + w > ctx->win_w) { w = ctx->win_w - x; }
	if(y + h > ctx->win_h) { h = ctx->win_h - y; }
	if(w <= 0 || h <= 0) {
		return;
	}
	struct mkgui_platform *plat = &ctx->plat;
	XShmPutImage(plat->dpy, plat->win, plat->gc, plat->img, x, y, x, y, (uint32_t)w, (uint32_t)h, False);
	XFlush(plat->dpy);
}

// [=]===^=[ platform_popup_blit ]=================================[=]
static void platform_popup_blit(struct mkgui_ctx *ctx, struct mkgui_popup *p) {
	struct mkgui_platform *plat = &ctx->plat;
	XShmPutImage(plat->dpy, p->plat.xwin, plat->gc, p->plat.img, 0, 0, 0, 0, (uint32_t)p->w, (uint32_t)p->h, False);
	XFlush(plat->dpy);
}

// [=]===^=[ platform_flush ]======================================[=]
static void platform_flush(struct mkgui_ctx *ctx) {
	XFlush(ctx->plat.dpy);
}

// ---------------------------------------------------------------------------
// Popup windows
// ---------------------------------------------------------------------------

// [=]===^=[ platform_popup_init ]=================================[=]
static uint32_t platform_popup_init(struct mkgui_ctx *ctx, struct mkgui_popup *p, int32_t x, int32_t y, int32_t w, int32_t h) {
	struct mkgui_platform *plat = &ctx->plat;

	XSetWindowAttributes attrs;
	attrs.override_redirect = True;
	attrs.save_under = True;
	attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | LeaveWindowMask;

	p->plat.xwin = XCreateWindow(plat->dpy, plat->root, x, y, (uint32_t)w, (uint32_t)h, 0,
		(int)plat->depth, InputOutput, plat->visual,
		CWOverrideRedirect | CWSaveUnder | CWEventMask, &attrs);

	if(!p->plat.xwin) {
		return 0;
	}

	platform_fb_create(plat, &p->plat.shm, &p->plat.img, &p->pixels, w, h);

	XMapRaised(plat->dpy, p->plat.xwin);
	XFlush(plat->dpy);

	return 1;
}

// [=]===^=[ platform_popup_fini ]=================================[=]
static void platform_popup_fini(struct mkgui_ctx *ctx, struct mkgui_popup *p) {
	struct mkgui_platform *plat = &ctx->plat;
	if(p->plat.img) {
		platform_fb_destroy(plat, &p->plat.shm, p->plat.img);
		p->plat.img = NULL;
	}
	if(p->plat.xwin) {
		XDestroyWindow(plat->dpy, p->plat.xwin);
		p->plat.xwin = 0;
	}
}

// ---------------------------------------------------------------------------
// Screen size
// ---------------------------------------------------------------------------

// [=]===^=[ platform_screen_size ]================================[=]
static void platform_screen_size(struct mkgui_ctx *ctx, int32_t *sw, int32_t *sh) {
	*sw = DisplayWidth(ctx->plat.dpy, ctx->plat.screen);
	*sh = DisplayHeight(ctx->plat.dpy, ctx->plat.screen);
}

// [=]===^=[ platform_set_min_size ]================================[=]
static void platform_set_min_size(struct mkgui_ctx *ctx, int32_t min_w, int32_t min_h) {
	XSizeHints hints = {0};
	hints.flags = PMinSize;
	hints.min_width = min_w;
	hints.min_height = min_h;
	XSetWMNormalHints(ctx->plat.dpy, ctx->plat.win, &hints);
}

// ---------------------------------------------------------------------------
// Coordinate translation
// ---------------------------------------------------------------------------

// [=]===^=[ platform_translate_coords ]===========================[=]
static void platform_translate_coords(struct mkgui_ctx *ctx, int32_t lx, int32_t ly, int32_t *sx, int32_t *sy) {
	Window child;
	XTranslateCoordinates(ctx->plat.dpy, ctx->plat.win, ctx->plat.root, lx, ly, sx, sy, &child);
}

// ---------------------------------------------------------------------------
// Event translation
// ---------------------------------------------------------------------------

// [=]===^=[ platform_wait_event ]=================================[=]
static void platform_wait_event(struct mkgui_ctx *ctx, int32_t timeout_ms) {
	if(timeout_ms == 0) {
		return;
	}
	if(XEventsQueued(ctx->plat.dpy, QueuedAlready) > 0) {
		return;
	}
	XFlush(ctx->plat.dpy);
	struct pollfd pfds[1 + MKGUI_MAX_TIMERS];
	uint32_t nfds = 0;
	pfds[nfds].fd = ConnectionNumber(ctx->plat.dpy);
	pfds[nfds].events = POLLIN;
	++nfds;
	for(uint32_t i = 0; i < ctx->timer_count; ++i) {
		if(ctx->timers[i].active && ctx->timers[i].fd >= 0) {
			pfds[nfds].fd = ctx->timers[i].fd;
			pfds[nfds].events = POLLIN;
			++nfds;
		}
	}
	poll(pfds, nfds, timeout_ms);
	if(pfds[0].revents & POLLIN) {
		XEventsQueued(ctx->plat.dpy, QueuedAfterReading);
	}
}

// [=]===^=[ platform_deferred_push ]==============================[=]
static void platform_deferred_push(struct mkgui_ctx *ctx, struct mkgui_plat_event *pev) {
	uint32_t next = (ctx->plat.deferred_head + 1) % MKGUI_DEFERRED_SIZE;
	if(next == ctx->plat.deferred_tail) {
		return;
	}
	ctx->plat.deferred[ctx->plat.deferred_head] = *pev;
	ctx->plat.deferred_head = next;
}

// [=]===^=[ platform_deferred_pop ]===============================[=]
static uint32_t platform_deferred_pop(struct mkgui_ctx *ctx, struct mkgui_plat_event *pev) {
	if(ctx->plat.deferred_head == ctx->plat.deferred_tail) {
		return 0;
	}
	*pev = ctx->plat.deferred[ctx->plat.deferred_tail];
	ctx->plat.deferred_tail = (ctx->plat.deferred_tail + 1) % MKGUI_DEFERRED_SIZE;
	return 1;
}

// [=]===^=[ platform_pending ]====================================[=]
static uint32_t platform_pending(struct mkgui_ctx *ctx) {
	if(ctx->plat.deferred_head != ctx->plat.deferred_tail) {
		return 1;
	}
	if(XEventsQueued(ctx->plat.dpy, QueuedAlready) > 0) {
		return 1;
	}
	return 0;
}

// [=]===^=[ platform_translate_keysym ]===========================[=]
static uint32_t platform_translate_keysym(KeySym ks) {
	switch(ks) {
		case 0xff8d: { return 0xff0d; } break; // KP_Enter  -> Return
		case 0xff89: { return 0xff09; } break; // KP_Tab    -> Tab
		case 0xff9f: { return 0xffff; } break; // KP_Delete -> Delete
		case 0xff95: { return 0xff50; } break; // KP_Home   -> Home
		case 0xff96: { return 0xff51; } break; // KP_Left   -> Left
		case 0xff97: { return 0xff52; } break; // KP_Up     -> Up
		case 0xff98: { return 0xff53; } break; // KP_Right  -> Right
		case 0xff99: { return 0xff54; } break; // KP_Down   -> Down
		case 0xff9a: { return 0xff55; } break; // KP_Prior  -> Prior
		case 0xff9b: { return 0xff56; } break; // KP_Next   -> Next
		case 0xff9c: { return 0xff57; } break; // KP_End    -> End
		default: break;
	}
	return (uint32_t)ks;
}

// ---------------------------------------------------------------------------
// File drag-and-drop (XDnd v5)
// ---------------------------------------------------------------------------

// [=]===^=[ platform_drop_enable ]================================[=]
static void platform_drop_enable(struct mkgui_ctx *ctx) {
	struct mkgui_platform *plat = &ctx->plat;
	Atom version = 5;
	XChangeProperty(plat->dpy, plat->win, plat->xdnd_aware, XA_ATOM, 32,
		PropModeReplace, (unsigned char *)&version, 1);
}

// [=]===^=[ platform_xdnd_handle ]================================[=]
static uint32_t platform_xdnd_handle(struct mkgui_ctx *ctx, XClientMessageEvent *cm, struct mkgui_plat_event *pev) {
	struct mkgui_platform *plat = &ctx->plat;

	if(cm->message_type == plat->xdnd_enter) {
		plat->xdnd_source = (Window)cm->data.l[0];
		plat->xdnd_uri_ok = 0;
		uint32_t has_type_list = (uint32_t)cm->data.l[1] & 1;
		if(has_type_list) {
			Atom type_ret;
			int32_t format;
			unsigned long count, remaining;
			unsigned char *data = NULL;
			XGetWindowProperty(plat->dpy, plat->xdnd_source, plat->text_uri_list,
				0, 256, False, XA_ATOM, &type_ret, &format, &count, &remaining, &data);
			if(!data) {
				Atom type_list_atom = XInternAtom(plat->dpy, "XdndTypeList", False);
				XGetWindowProperty(plat->dpy, plat->xdnd_source, type_list_atom,
					0, 256, False, XA_ATOM, &type_ret, &format, &count, &remaining, &data);
			}
			if(data) {
				Atom *types = (Atom *)data;
				for(unsigned long i = 0; i < count; ++i) {
					if(types[i] == plat->text_uri_list) {
						plat->xdnd_uri_ok = 1;
						break;
					}
				}
				XFree(data);
			}
		} else {
			for(uint32_t i = 0; i < 3; ++i) {
				if((Atom)cm->data.l[2 + i] == plat->text_uri_list) {
					plat->xdnd_uri_ok = 1;
					break;
				}
			}
		}
		return 0;
	}

	if(cm->message_type == plat->xdnd_position) {
		XClientMessageEvent reply;
		memset(&reply, 0, sizeof(reply));
		reply.type = ClientMessage;
		reply.window = plat->xdnd_source;
		reply.message_type = plat->xdnd_status;
		reply.format = 32;
		reply.data.l[0] = (long)plat->win;
		if(plat->xdnd_uri_ok && ctx->drop_enabled) {
			reply.data.l[1] = 1;
			reply.data.l[4] = (long)plat->xdnd_action_copy;
		}
		XSendEvent(plat->dpy, plat->xdnd_source, False, NoEventMask, (XEvent *)&reply);
		XFlush(plat->dpy);
		return 0;
	}

	if(cm->message_type == plat->xdnd_drop) {
		if(plat->xdnd_uri_ok && ctx->drop_enabled) {
			XConvertSelection(plat->dpy, plat->xdnd_selection, plat->text_uri_list,
				plat->mkgui_clip_prop, plat->win, (Time)cm->data.l[2]);
		} else {
			XClientMessageEvent fin;
			memset(&fin, 0, sizeof(fin));
			fin.type = ClientMessage;
			fin.window = plat->xdnd_source;
			fin.message_type = plat->xdnd_finished;
			fin.format = 32;
			fin.data.l[0] = (long)plat->win;
			XSendEvent(plat->dpy, plat->xdnd_source, False, NoEventMask, (XEvent *)&fin);
			XFlush(plat->dpy);
		}
		return 0;
	}

	if(cm->message_type == plat->xdnd_leave) {
		plat->xdnd_source = 0;
		plat->xdnd_uri_ok = 0;
		return 0;
	}

	(void)pev;
	return 0;
}

// [=]===^=[ platform_xdnd_selection ]=============================[=]
static uint32_t platform_xdnd_selection(struct mkgui_ctx *ctx, XSelectionEvent *se, struct mkgui_plat_event *pev) {
	struct mkgui_platform *plat = &ctx->plat;

	if(se->selection != plat->xdnd_selection) {
		return 0;
	}

	uint32_t got_data = 0;
	if(se->property != None) {
		Atom type_ret;
		int32_t format;
		unsigned long count, remaining;
		unsigned char *data = NULL;
		XGetWindowProperty(plat->dpy, plat->win, se->property,
			0, 65536, True, AnyPropertyType, &type_ret, &format, &count, &remaining, &data);
		if(data && count > 0) {
			drop_parse_uri_list(ctx, (char *)data, (uint32_t)count);
			got_data = (ctx->drop_count > 0);
		}
		if(data) {
			XFree(data);
		}
	}

	XClientMessageEvent fin;
	memset(&fin, 0, sizeof(fin));
	fin.type = ClientMessage;
	fin.window = plat->xdnd_source;
	fin.message_type = plat->xdnd_finished;
	fin.format = 32;
	fin.data.l[0] = (long)plat->win;
	if(got_data) {
		fin.data.l[1] = 1;
		fin.data.l[2] = (long)plat->xdnd_action_copy;
	}
	XSendEvent(plat->dpy, plat->xdnd_source, False, NoEventMask, (XEvent *)&fin);
	XFlush(plat->dpy);

	plat->xdnd_source = 0;
	plat->xdnd_uri_ok = 0;

	if(got_data) {
		pev->type = MKGUI_PLAT_DROP;
		return 1;
	}
	return 0;
}

// [=]===^=[ platform_translate_xevent ]===========================[=]
static void platform_translate_xevent(struct mkgui_ctx *owner, XEvent *xev, struct mkgui_plat_event *pev) {
	switch(xev->type) {
		case Expose: {
			pev->type = MKGUI_PLAT_EXPOSE;
		} break;

		case ConfigureNotify: {
			if(xev->xconfigure.window == owner->plat.win) {
				while(XEventsQueued(owner->plat.dpy, QueuedAlready) > 0) {
					XEvent peek;
					XPeekEvent(owner->plat.dpy, &peek);
					if(peek.type != ConfigureNotify || peek.xconfigure.window != xev->xconfigure.window) {
						break;
					}
					XNextEvent(owner->plat.dpy, xev);
				}
				pev->type = MKGUI_PLAT_RESIZE;
				pev->width = xev->xconfigure.width;
				pev->height = xev->xconfigure.height;
			}
		} break;

		case MotionNotify: {
			while(XEventsQueued(owner->plat.dpy, QueuedAlready) > 0) {
				XEvent peek;
				XPeekEvent(owner->plat.dpy, &peek);
				if(peek.type != MotionNotify || peek.xmotion.window != xev->xmotion.window) {
					break;
				}
				XNextEvent(owner->plat.dpy, xev);
			}
			pev->type = MKGUI_PLAT_MOTION;
			pev->x = xev->xmotion.x;
			pev->y = xev->xmotion.y;
		} break;

		case ButtonPress: {
			pev->type = MKGUI_PLAT_BUTTON_PRESS;
			pev->x = xev->xbutton.x;
			pev->y = xev->xbutton.y;
			pev->button = xev->xbutton.button;
			pev->keymod = xev->xbutton.state;
		} break;

		case ButtonRelease: {
			pev->type = MKGUI_PLAT_BUTTON_RELEASE;
			pev->x = xev->xbutton.x;
			pev->y = xev->xbutton.y;
			pev->button = xev->xbutton.button;
		} break;

		case KeyPress: {
			char buf[32];
			KeySym ks;
			int32_t len;
			if(owner->plat.xic) {
				Status status;
				len = Xutf8LookupString(owner->plat.xic, &xev->xkey, buf, sizeof(buf) - 1, &ks, &status);
			} else {
				len = XLookupString(&xev->xkey, buf, sizeof(buf) - 1, &ks, NULL);
			}
			buf[len] = '\0';

			pev->type = MKGUI_PLAT_KEY;
			pev->keysym = platform_translate_keysym(ks);
			pev->keymod = xev->xkey.state;
			memcpy(pev->text, buf, (uint32_t)(len + 1));
			pev->text_len = len;
		} break;

		case ClientMessage: {
			if((Atom)xev->xclient.data.l[0] == owner->plat.wm_delete) {
				pev->type = MKGUI_PLAT_CLOSE;
			} else {
				platform_xdnd_handle(owner, &xev->xclient, pev);
			}
		} break;

		case LeaveNotify: {
			pev->type = MKGUI_PLAT_LEAVE;
		} break;

		case FocusOut: {
			pev->type = MKGUI_PLAT_FOCUS_OUT;
		} break;

		case SelectionRequest: {
			XSelectionRequestEvent *req = &xev->xselectionrequest;
			XSelectionEvent resp;
			memset(&resp, 0, sizeof(resp));
			resp.type = SelectionNotify;
			resp.requestor = req->requestor;
			resp.selection = req->selection;
			resp.target = req->target;
			resp.time = req->time;
			resp.property = None;

			if(req->target == owner->plat.targets) {
				Atom supported[] = { owner->plat.utf8_string, XA_STRING };
				XChangeProperty(owner->plat.dpy, req->requestor, req->property, XA_ATOM, 32,
					PropModeReplace, (unsigned char *)supported, 2);
				resp.property = req->property;
			} else if(req->target == owner->plat.utf8_string || req->target == XA_STRING) {
				XChangeProperty(owner->plat.dpy, req->requestor, req->property, req->target, 8,
					PropModeReplace, (unsigned char *)owner->clip_text, (int)owner->clip_len);
				resp.property = req->property;
			}

			XSendEvent(owner->plat.dpy, req->requestor, False, 0, (XEvent *)&resp);
			XFlush(owner->plat.dpy);
			pev->type = MKGUI_PLAT_NONE;
		} break;

		case SelectionNotify: {
			pev->type = MKGUI_PLAT_NONE;
			platform_xdnd_selection(owner, &xev->xselection, pev);
		} break;

		default: {
			pev->type = MKGUI_PLAT_NONE;
		} break;
	}
}

// [=]===^=[ platform_find_window_owner ]==========================[=]
static struct mkgui_ctx *platform_find_window_owner(Window xwin, int32_t *popup_idx) {
	*popup_idx = -1;
	for(uint32_t i = 0; i < window_registry_count; ++i) {
		struct mkgui_ctx *c = window_registry[i];
		if(c->plat.win == xwin) {
			return c;
		}
		for(uint32_t p = 0; p < c->popup_count; ++p) {
			if(c->popups[p].plat.xwin == xwin) {
				*popup_idx = (int32_t)p;
				return c;
			}
		}
	}
	return NULL;
}

// [=]===^=[ platform_next_event ]=================================[=]
static void platform_next_event(struct mkgui_ctx *ctx, struct mkgui_plat_event *pev) {
	memset(pev, 0, sizeof(*pev));
	pev->popup_idx = -1;

	if(platform_deferred_pop(ctx, pev)) {
		return;
	}

	XEvent xev;
	XNextEvent(ctx->plat.dpy, &xev);

	if(xev.xany.window == ctx->plat.win) {
		platform_translate_xevent(ctx, &xev, pev);
		return;
	}

	for(uint32_t i = 0; i < ctx->popup_count; ++i) {
		if(ctx->popups[i].plat.xwin == xev.xany.window) {
			pev->popup_idx = (int32_t)i;
			platform_translate_xevent(ctx, &xev, pev);
			return;
		}
	}

	if(ctx->parent && xev.xany.window == ctx->parent->plat.win) {
		if(xev.type == Expose) {
			ctx->parent->dirty = 1;
			ctx->parent->dirty_full = 1;
		} else if(xev.type == ConfigureNotify) {
			int32_t nw = xev.xconfigure.width;
			int32_t nh = xev.xconfigure.height;
			if(nw != ctx->parent->win_w || nh != ctx->parent->win_h) {
				ctx->parent->win_w = nw;
				ctx->parent->win_h = nh;
				platform_fb_resize(ctx->parent);
				ctx->parent->dirty = 1;
				ctx->parent->dirty_full = 1;
			}
		}
		pev->type = MKGUI_PLAT_NONE;
		return;
	}

	int32_t foreign_popup = -1;
	struct mkgui_ctx *owner = platform_find_window_owner(xev.xany.window, &foreign_popup);
	if(owner && owner != ctx) {
		struct mkgui_plat_event foreign;
		memset(&foreign, 0, sizeof(foreign));
		foreign.popup_idx = foreign_popup;
		platform_translate_xevent(owner, &xev, &foreign);
		if(foreign.type != MKGUI_PLAT_NONE) {
			platform_deferred_push(owner, &foreign);
		}
		pev->type = MKGUI_PLAT_NONE;
		return;
	}

	pev->type = MKGUI_PLAT_NONE;
}

// ---------------------------------------------------------------------------
// Font rendering (FreeType)
// ---------------------------------------------------------------------------

static FT_Library plat_ft_lib;
static FT_Face plat_ft_face;

static char plat_fc_font_path[4096];

// [=]===^=[ platform_find_font ]==================================[=]
static const char *platform_find_font(void) {
	const char *env = getenv("MKGUI_FONT");
	if(env) {
		return env;
	}

	FcConfig *config = FcInitLoadConfigAndFonts();
	if(config) {
		FcPattern *pat = FcNameParse((FcChar8 *)"sans-serif");
		if(pat) {
			FcConfigSubstitute(config, pat, FcMatchPattern);
			FcDefaultSubstitute(pat);
			FcResult result;
			FcPattern *match = FcFontMatch(config, pat, &result);
			if(match) {
				FcChar8 *file = NULL;
				if(FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch && file) {
					snprintf(plat_fc_font_path, sizeof(plat_fc_font_path), "%s", (char *)file);
				}
				FcPatternDestroy(match);
			}
			FcPatternDestroy(pat);
		}
		FcConfigDestroy(config);
		FcFini();
		if(plat_fc_font_path[0]) {
			return plat_fc_font_path;
		}
	}

	return NULL;
}

// [=]===^=[ platform_font_rasterize ]==============================[=]
static void platform_font_rasterize(struct mkgui_ctx *ctx, int32_t pixel_size) {
	if(!plat_ft_face) {
		return;
	}
	FT_Set_Pixel_Sizes(plat_ft_face, 0, (FT_UInt)pixel_size);

	ctx->font_ascent = (int32_t)(plat_ft_face->size->metrics.ascender >> 6);
	ctx->font_height = (int32_t)((plat_ft_face->size->metrics.ascender - plat_ft_face->size->metrics.descender) >> 6);

	for(uint32_t c = MKGUI_GLYPH_FIRST; c <= MKGUI_GLYPH_LAST; ++c) {
		if(FT_Load_Char(plat_ft_face, c, FT_LOAD_RENDER)) {
			continue;
		}
		FT_GlyphSlot slot = plat_ft_face->glyph;
		struct mkgui_glyph *g = &ctx->glyphs[c - MKGUI_GLYPH_FIRST];

		g->width = (int32_t)slot->bitmap.width;
		g->height = (int32_t)slot->bitmap.rows;
		g->bearing_x = slot->bitmap_left;
		g->bearing_y = slot->bitmap_top;
		g->advance = (int32_t)(slot->advance.x >> 6);

		uint32_t bmp_size = (uint32_t)(g->width * g->height);
		if(bmp_size > MKGUI_GLYPH_MAX_BMP) {
			bmp_size = MKGUI_GLYPH_MAX_BMP;
		}
		uint32_t glyph_idx = c - MKGUI_GLYPH_FIRST;
		memset(glyph_staging[glyph_idx], 0, MKGUI_GLYPH_MAX_BMP);
		if(slot->bitmap.buffer) {
			memcpy(glyph_staging[glyph_idx], slot->bitmap.buffer, bmp_size);
		}
	}

	ctx->char_width = ctx->glyphs['M' - MKGUI_GLYPH_FIRST].advance;
	if(ctx->char_width == 0) {
		ctx->char_width = sc(ctx, 7);
	}
	glyph_atlas_build(ctx);
}

// [=]===^=[ platform_font_init ]==================================[=]
static void platform_font_init(struct mkgui_ctx *ctx) {
	if(FT_Init_FreeType(&plat_ft_lib)) {
		fprintf(stderr, "mkgui: cannot init freetype\n");
		ctx->font_ascent = sc(ctx, 11);
		ctx->font_height = sc(ctx, 13);
		ctx->char_width = sc(ctx, 7);
		return;
	}

	const char *path = platform_find_font();
	if(!path || FT_New_Face(plat_ft_lib, path, 0, &plat_ft_face)) {
		fprintf(stderr, "mkgui: cannot load font%s%s\n", path ? ": " : "", path ? path : "");
		plat_ft_face = NULL;
		ctx->font_ascent = sc(ctx, 11);
		ctx->font_height = sc(ctx, 13);
		ctx->char_width = sc(ctx, 7);
		return;
	}

	int32_t font_px = (int32_t)(13.0f * ctx->scale + 0.5f);
	platform_font_rasterize(ctx, font_px);
}

// [=]===^=[ platform_font_set_size ]===============================[=]
static void platform_font_set_size(struct mkgui_ctx *ctx, int32_t pixel_size) {
	platform_font_rasterize(ctx, pixel_size);
}

// [=]===^=[ platform_font_fini ]==================================[=]
static void platform_font_fini(struct mkgui_ctx *ctx) {
	(void)ctx;
	if(plat_ft_face) {
		FT_Done_Face(plat_ft_face);
		plat_ft_face = NULL;
	}
	if(plat_ft_lib) {
		FT_Done_FreeType(plat_ft_lib);
		plat_ft_lib = NULL;
	}
}

// ---------------------------------------------------------------------------
// Clipboard
// ---------------------------------------------------------------------------

// [=]===^=[ platform_clipboard_set ]==============================[=]
static void platform_clipboard_set(struct mkgui_ctx *ctx, const char *text, uint32_t len) {
	if(len >= MKGUI_CLIP_MAX) {
		len = MKGUI_CLIP_MAX - 1;
	}
	memcpy(ctx->clip_text, text, len);
	ctx->clip_text[len] = '\0';
	ctx->clip_len = len;
	XSetSelectionOwner(ctx->plat.dpy, ctx->plat.clipboard, ctx->plat.win, CurrentTime);
	XFlush(ctx->plat.dpy);
}

// [=]===^=[ platform_clipboard_get ]==============================[=]
static uint32_t platform_clipboard_get(struct mkgui_ctx *ctx, char *buf, uint32_t buf_size) {
	struct mkgui_platform *plat = &ctx->plat;

	if(XGetSelectionOwner(plat->dpy, plat->clipboard) == plat->win) {
		uint32_t len = ctx->clip_len;
		if(len >= buf_size) {
			len = buf_size - 1;
		}
		memcpy(buf, ctx->clip_text, len);
		buf[len] = '\0';
		return len;
	}

	XConvertSelection(plat->dpy, plat->clipboard, plat->utf8_string, plat->mkgui_clip_prop, plat->win, CurrentTime);
	XFlush(plat->dpy);

	XEvent xev;
	for(uint32_t i = 0; i < 50; ++i) {
		if(XCheckTypedWindowEvent(plat->dpy, plat->win, SelectionNotify, &xev)) {
			if(xev.xselection.property == None) {
				return 0;
			}
			Atom type;
			int format;
			unsigned long nitems, bytes_after;
			unsigned char *data = NULL;
			XGetWindowProperty(plat->dpy, plat->win, plat->mkgui_clip_prop, 0, 1024 * 1024, True,
				AnyPropertyType, &type, &format, &nitems, &bytes_after, &data);
			if(data && nitems > 0) {
				uint32_t len = (uint32_t)nitems;
				if(len >= buf_size) {
					len = buf_size - 1;
				}
				memcpy(buf, data, len);
				buf[len] = '\0';
				XFree(data);
				return len;
			}
			if(data) {
				XFree(data);
			}
			return 0;
		}
		struct timespec ts = {0, 10000000};
		nanosleep(&ts, NULL);
	}
	return 0;
}

// [=]===^=[ platform_clipboard_get_alloc ]========================[=]
static char *platform_clipboard_get_alloc(struct mkgui_ctx *ctx, uint32_t *out_len) {
	struct mkgui_platform *plat = &ctx->plat;
	*out_len = 0;

	if(XGetSelectionOwner(plat->dpy, plat->clipboard) == plat->win) {
		uint32_t len = ctx->clip_len;
		char *buf = (char *)malloc(len + 1);
		if(!buf) {
			return NULL;
		}
		memcpy(buf, ctx->clip_text, len);
		buf[len] = '\0';
		*out_len = len;
		return buf;
	}

	XConvertSelection(plat->dpy, plat->clipboard, plat->utf8_string, plat->mkgui_clip_prop, plat->win, CurrentTime);
	XFlush(plat->dpy);

	XEvent xev;
	for(uint32_t i = 0; i < 50; ++i) {
		if(XCheckTypedWindowEvent(plat->dpy, plat->win, SelectionNotify, &xev)) {
			if(xev.xselection.property == None) {
				return NULL;
			}
			Atom type;
			int format;
			unsigned long nitems, bytes_after;
			unsigned char *data = NULL;
			XGetWindowProperty(plat->dpy, plat->win, plat->mkgui_clip_prop, 0, 1024 * 1024, True,
				AnyPropertyType, &type, &format, &nitems, &bytes_after, &data);
			if(data && nitems > 0) {
				uint32_t len = (uint32_t)nitems;
				char *buf = (char *)malloc(len + 1);
				if(!buf) {
					XFree(data);
					return NULL;
				}
				memcpy(buf, data, len);
				buf[len] = '\0';
				XFree(data);
				*out_len = len;
				return buf;
			}
			if(data) {
				XFree(data);
			}
			return NULL;
		}
		struct timespec ts = {0, 10000000};
		nanosleep(&ts, NULL);
	}
	return NULL;
}

// ---------------------------------------------------------------------------
// GL view child window
// ---------------------------------------------------------------------------

// [=]===^=[ platform_glview_create ]==============================[=]
static uint32_t platform_glview_create(struct mkgui_ctx *ctx, struct mkgui_glview_data *gv, int32_t x, int32_t y, int32_t w, int32_t h) {
	struct mkgui_platform *plat = &ctx->plat;

	XSetWindowAttributes wa;
	wa.background_pixmap = None;
	wa.event_mask = 0;
	gv->plat.xwin = XCreateWindow(plat->dpy, plat->win, x, y, (uint32_t)w, (uint32_t)h, 0,
		CopyFromParent, InputOutput, CopyFromParent, CWBackPixmap | CWEventMask, &wa);

	XMapWindow(plat->dpy, gv->plat.xwin);
	XFlush(plat->dpy);
	return 1;
}

// [=]===^=[ platform_glview_destroy ]=============================[=]
static void platform_glview_destroy(struct mkgui_ctx *ctx, struct mkgui_glview_data *gv) {
	if(gv->plat.xwin) {
		XDestroyWindow(ctx->plat.dpy, gv->plat.xwin);
		gv->plat.xwin = 0;
		XFlush(ctx->plat.dpy);
	}
}

// [=]===^=[ platform_glview_reposition ]==========================[=]
static void platform_glview_reposition(struct mkgui_ctx *ctx, struct mkgui_glview_data *gv, int32_t x, int32_t y, int32_t w, int32_t h) {
	if(!gv->plat.xwin) {
		return;
	}
	XMoveResizeWindow(ctx->plat.dpy, gv->plat.xwin, x, y, (uint32_t)w, (uint32_t)h);
}

// [=]===^=[ platform_glview_show ]================================[=]
static void platform_glview_show(struct mkgui_ctx *ctx, struct mkgui_glview_data *gv, uint32_t visible) {
	if(!gv->plat.xwin) {
		return;
	}
	if(visible) {
		XMapWindow(ctx->plat.dpy, gv->plat.xwin);
	} else {
		XUnmapWindow(ctx->plat.dpy, gv->plat.xwin);
	}
}

