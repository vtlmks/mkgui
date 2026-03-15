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

	plat->wm_delete = XInternAtom(plat->dpy, "WM_DELETE_WINDOW", False);
	plat->clipboard = XInternAtom(plat->dpy, "CLIPBOARD", False);
	plat->utf8_string = XInternAtom(plat->dpy, "UTF8_STRING", False);
	plat->targets = XInternAtom(plat->dpy, "TARGETS", False);
	plat->mkgui_clip_prop = XInternAtom(plat->dpy, "MKGUI_CLIP", False);
	XSetWMProtocols(plat->dpy, plat->win, &plat->wm_delete, 1);

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
	plat->is_child = 1;

	XSetWindowAttributes wa;
	wa.background_pixmap = None;
	wa.bit_gravity = NorthWestGravity;
	plat->win = XCreateWindow(plat->dpy, plat->root, 0, 0, (uint32_t)w, (uint32_t)h, 0,
		(int)plat->depth, InputOutput, plat->visual, CWBackPixmap | CWBitGravity, &wa);
	XStoreName(plat->dpy, plat->win, title);

	XSetTransientForHint(plat->dpy, plat->win, pplat->win);

	plat->wm_delete = XInternAtom(plat->dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(plat->dpy, plat->win, &plat->wm_delete, 1);

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
	struct mkgui_platform *plat = &ctx->plat;
	if(plat->img) {
		platform_fb_destroy(plat, &plat->shm, plat->img);
		plat->img = NULL;
	}
	platform_fb_create(plat, &plat->shm, &plat->img, &ctx->pixels, ctx->win_w, ctx->win_h);
}

// ---------------------------------------------------------------------------
// Blit / flush
// ---------------------------------------------------------------------------

// [=]===^=[ platform_blit ]=======================================[=]
static void platform_blit(struct mkgui_ctx *ctx) {
	struct mkgui_platform *plat = &ctx->plat;
	XShmPutImage(plat->dpy, plat->win, plat->gc, plat->img, 0, 0, 0, 0, (uint32_t)ctx->win_w, (uint32_t)ctx->win_h, False);
	XSync(plat->dpy, False);
}

// [=]===^=[ platform_blit_region ]================================[=]
static void platform_blit_region(struct mkgui_ctx *ctx, int32_t x, int32_t y, int32_t w, int32_t h) {
	struct mkgui_platform *plat = &ctx->plat;
	XShmPutImage(plat->dpy, plat->win, plat->gc, plat->img, x, y, x, y, (uint32_t)w, (uint32_t)h, False);
	XSync(plat->dpy, False);
}

// [=]===^=[ platform_popup_blit ]=================================[=]
static void platform_popup_blit(struct mkgui_ctx *ctx, struct mkgui_popup *p) {
	struct mkgui_platform *plat = &ctx->plat;
	XShmPutImage(plat->dpy, p->plat.xwin, plat->gc, p->plat.img, 0, 0, 0, 0, (uint32_t)p->w, (uint32_t)p->h, False);
	XSync(plat->dpy, False);
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
	struct pollfd pfd;
	pfd.fd = ConnectionNumber(ctx->plat.dpy);
	pfd.events = POLLIN;
	if(poll(&pfd, 1, timeout_ms) > 0) {
		XEventsQueued(ctx->plat.dpy, QueuedAfterReading);
	}
}

// [=]===^=[ platform_pending ]====================================[=]
static uint32_t platform_pending(struct mkgui_ctx *ctx) {
	if(XEventsQueued(ctx->plat.dpy, QueuedAlready) > 0) {
		return 1;
	}
	return 0;
}

// [=]===^=[ platform_translate_keysym ]===========================[=]
static uint32_t platform_translate_keysym(KeySym ks) {
	return (uint32_t)ks;
}

// [=]===^=[ platform_next_event ]=================================[=]
static void platform_next_event(struct mkgui_ctx *ctx, struct mkgui_plat_event *pev) {
	XEvent xev;
	XNextEvent(ctx->plat.dpy, &xev);

	memset(pev, 0, sizeof(*pev));
	pev->popup_idx = -1;

	if(xev.xany.window != ctx->plat.win) {
		for(uint32_t i = 0; i < ctx->popup_count; ++i) {
			if(ctx->popups[i].plat.xwin == xev.xany.window) {
				pev->popup_idx = (int32_t)i;
				break;
			}
		}
		if(pev->popup_idx < 0) {
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
			}
			pev->type = MKGUI_PLAT_NONE;
			return;
		}
	}

	switch(xev.type) {
		case Expose: {
			pev->type = MKGUI_PLAT_EXPOSE;
		} break;

		case ConfigureNotify: {
			if(xev.xconfigure.window == ctx->plat.win) {
				pev->type = MKGUI_PLAT_RESIZE;
				pev->width = xev.xconfigure.width;
				pev->height = xev.xconfigure.height;
			}
		} break;

		case MotionNotify: {
			pev->type = MKGUI_PLAT_MOTION;
			pev->x = xev.xmotion.x;
			pev->y = xev.xmotion.y;
		} break;

		case ButtonPress: {
			pev->type = MKGUI_PLAT_BUTTON_PRESS;
			pev->x = xev.xbutton.x;
			pev->y = xev.xbutton.y;
			pev->button = xev.xbutton.button;
		} break;

		case ButtonRelease: {
			pev->type = MKGUI_PLAT_BUTTON_RELEASE;
			pev->x = xev.xbutton.x;
			pev->y = xev.xbutton.y;
			pev->button = xev.xbutton.button;
		} break;

		case KeyPress: {
			char buf[32];
			KeySym ks;
			int32_t len = XLookupString(&xev.xkey, buf, sizeof(buf) - 1, &ks, NULL);
			buf[len] = '\0';

			pev->type = MKGUI_PLAT_KEY;
			pev->keysym = platform_translate_keysym(ks);
			pev->keymod = xev.xkey.state;
			memcpy(pev->text, buf, (uint32_t)(len + 1));
			pev->text_len = len;
		} break;

		case ClientMessage: {
			if((Atom)xev.xclient.data.l[0] == ctx->plat.wm_delete) {
				pev->type = MKGUI_PLAT_CLOSE;
			}
		} break;

		case LeaveNotify: {
			pev->type = MKGUI_PLAT_LEAVE;
		} break;

		case SelectionRequest: {
			XSelectionRequestEvent *req = &xev.xselectionrequest;
			XSelectionEvent resp;
			memset(&resp, 0, sizeof(resp));
			resp.type = SelectionNotify;
			resp.requestor = req->requestor;
			resp.selection = req->selection;
			resp.target = req->target;
			resp.time = req->time;
			resp.property = None;

			if(req->target == ctx->plat.targets) {
				Atom supported[] = { ctx->plat.utf8_string, XA_STRING };
				XChangeProperty(ctx->plat.dpy, req->requestor, req->property, XA_ATOM, 32,
					PropModeReplace, (unsigned char *)supported, 2);
				resp.property = req->property;
			} else if(req->target == ctx->plat.utf8_string || req->target == XA_STRING) {
				XChangeProperty(ctx->plat.dpy, req->requestor, req->property, req->target, 8,
					PropModeReplace, (unsigned char *)ctx->clip_text, (int)ctx->clip_len);
				resp.property = req->property;
			}

			XSendEvent(ctx->plat.dpy, req->requestor, False, 0, (XEvent *)&resp);
			XFlush(ctx->plat.dpy);
			pev->type = MKGUI_PLAT_NONE;
		} break;

		case SelectionNotify: {
			pev->type = MKGUI_PLAT_NONE;
		} break;

		default: {
			pev->type = MKGUI_PLAT_NONE;
		} break;
	}
}

// ---------------------------------------------------------------------------
// Font rendering (FreeType)
// ---------------------------------------------------------------------------

static FT_Library plat_ft_lib;
static FT_Face plat_ft_face;

static const char *plat_font_paths[] = {
	"/usr/share/fonts/noto/NotoSans-Regular.ttf",
	"/usr/share/fonts/TTF/IBMPlexSans-Regular.ttf",
	"/usr/share/fonts/TTF/DejaVuSans.ttf",
	"/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
	"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
	"/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf",
	"/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
	"/usr/share/fonts/Adwaita/Adwaita-Regular.ttf",
};

// [=]===^=[ platform_find_font ]==================================[=]
static const char *platform_find_font(void) {
	const char *env = getenv("MKGUI_FONT");
	if(env) {
		return env;
	}
	for(uint32_t i = 0; i < sizeof(plat_font_paths) / sizeof(plat_font_paths[0]); ++i) {
		FILE *f = fopen(plat_font_paths[i], "rb");
		if(f) {
			fclose(f);
			return plat_font_paths[i];
		}
	}
	return NULL;
}

// [=]===^=[ platform_font_init ]==================================[=]
static void platform_font_init(struct mkgui_ctx *ctx) {
	if(FT_Init_FreeType(&plat_ft_lib)) {
		fprintf(stderr, "mkgui: cannot init freetype\n");
		ctx->font_ascent = 11;
		ctx->font_height = 13;
		ctx->char_width = 7;
		return;
	}

	const char *path = platform_find_font();
	if(!path || FT_New_Face(plat_ft_lib, path, 0, &plat_ft_face)) {
		fprintf(stderr, "mkgui: cannot load font%s%s\n", path ? ": " : "", path ? path : "");
		plat_ft_face = NULL;
		ctx->font_ascent = 11;
		ctx->font_height = 13;
		ctx->char_width = 7;
		return;
	}

	FT_Set_Pixel_Sizes(plat_ft_face, 0, 13);

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
		if(slot->bitmap.buffer) {
			memcpy(g->bitmap, slot->bitmap.buffer, bmp_size);
		}
	}

	ctx->char_width = ctx->glyphs['M' - MKGUI_GLYPH_FIRST].advance;
	if(ctx->char_width == 0) {
		ctx->char_width = 7;
	}
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

