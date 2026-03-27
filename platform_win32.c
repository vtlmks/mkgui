// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// ---------------------------------------------------------------------------
// Per-ctx event queue helpers
// ---------------------------------------------------------------------------

// [=]===^=[ evq_push_ctx ]========================================[=]
static void evq_push_ctx(struct mkgui_platform *plat, struct mkgui_plat_event *pev) {
	uint32_t next = (plat->evq_head + 1) % MKGUI_EVQ_SIZE;
	if(next == plat->evq_tail) {
		return;
	}
	plat->evq_buf[plat->evq_head] = *pev;
	plat->evq_head = next;
}

// [=]===^=[ evq_pop_ctx ]=========================================[=]
static uint32_t evq_pop_ctx(struct mkgui_platform *plat, struct mkgui_plat_event *pev) {
	if(plat->evq_head == plat->evq_tail) {
		return 0;
	}
	*pev = plat->evq_buf[plat->evq_tail];
	plat->evq_tail = (plat->evq_tail + 1) % MKGUI_EVQ_SIZE;
	return 1;
}

// ---------------------------------------------------------------------------
// Key translation
// ---------------------------------------------------------------------------

// [=]===^=[ platform_translate_vk ]================================[=]
static uint32_t platform_translate_vk(WPARAM vk) {
	switch(vk) {
		case VK_BACK:   { return 0xff08; } break;
		case VK_TAB:    { return 0xff09; } break;
		case VK_RETURN: { return 0xff0d; } break;
		case VK_ESCAPE: { return 0xff1b; } break;
		case VK_DELETE: { return 0xffff; } break;
		case VK_HOME:   { return 0xff50; } break;
		case VK_LEFT:   { return 0xff51; } break;
		case VK_UP:     { return 0xff52; } break;
		case VK_RIGHT:  { return 0xff53; } break;
		case VK_DOWN:   { return 0xff54; } break;
		case VK_PRIOR:  { return 0xff55; } break;
		case VK_NEXT:   { return 0xff56; } break;
		case VK_END:    { return 0xff57; } break;
		default: break;
	}
	return (uint32_t)vk;
}

// [=]===^=[ platform_get_keymod ]=================================[=]
static uint32_t platform_get_keymod(void) {
	uint32_t mod = 0;
	if(GetKeyState(VK_SHIFT) & 0x8000) {
		mod |= 0x01;
	}
	if(GetKeyState(VK_CONTROL) & 0x8000) {
		mod |= 0x04;
	}
	if(GetKeyState(VK_MENU) & 0x8000) {
		mod |= 0x08;
	}
	return mod;
}

// ---------------------------------------------------------------------------
// WndProc
// ---------------------------------------------------------------------------

// [=]===^=[ wndproc_find_owner ]===================================[=]
static struct mkgui_ctx *wndproc_find_owner(HWND hwnd, int32_t *popup_idx) {
	*popup_idx = -1;
	for(uint32_t i = 0; i < window_registry_count; ++i) {
		struct mkgui_ctx *c = window_registry[i];
		if(c->plat.hwnd == hwnd) {
			return c;
		}
		for(uint32_t p = 0; p < c->popup_count; ++p) {
			if(c->popups[p].plat.hwnd == hwnd) {
				*popup_idx = (int32_t)p;
				return c;
			}
		}
	}
	return NULL;
}

// [=]===^=[ mkgui_wndproc ]=======================================[=]
static LRESULT CALLBACK mkgui_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	int32_t popup_idx = -1;
	struct mkgui_ctx *owner = wndproc_find_owner(hwnd, &popup_idx);

	if(!owner) {
		return DefWindowProcA(hwnd, msg, wp, lp);
	}

	struct mkgui_plat_event pev;
	memset(&pev, 0, sizeof(pev));
	pev.popup_idx = popup_idx;

	switch(msg) {
		case WM_GETMINMAXINFO: {
			if(owner && pev.popup_idx < 0 && (owner->plat.min_w > 0 || owner->plat.min_h > 0)) {
				MINMAXINFO *mmi = (MINMAXINFO *)lp;
				if(owner->plat.min_w > 0) {
					mmi->ptMinTrackSize.x = owner->plat.min_w;
				}
				if(owner->plat.min_h > 0) {
					mmi->ptMinTrackSize.y = owner->plat.min_h;
				}
			}
			return 0;
		} break;

		case WM_PAINT: {
			PAINTSTRUCT ps;
			BeginPaint(hwnd, &ps);
			EndPaint(hwnd, &ps);
			pev.type = MKGUI_PLAT_EXPOSE;
			evq_push_ctx(&owner->plat, &pev);
			return 0;
		} break;

		case WM_SIZE: {
			if(pev.popup_idx < 0) {
				pev.type = MKGUI_PLAT_RESIZE;
				pev.width = LOWORD(lp);
				pev.height = HIWORD(lp);
				uint32_t prev = (owner->plat.evq_head + MKGUI_EVQ_SIZE - 1) % MKGUI_EVQ_SIZE;
				if(owner->plat.evq_head != owner->plat.evq_tail && owner->plat.evq_buf[prev].type == MKGUI_PLAT_RESIZE) {
					owner->plat.evq_buf[prev] = pev;
				} else {
					evq_push_ctx(&owner->plat, &pev);
				}
			}
			return 0;
		} break;

		case WM_MOUSEMOVE: {
			pev.type = MKGUI_PLAT_MOTION;
			pev.x = (int16_t)LOWORD(lp);
			pev.y = (int16_t)HIWORD(lp);
			uint32_t prev = (owner->plat.evq_head + MKGUI_EVQ_SIZE - 1) % MKGUI_EVQ_SIZE;
			if(owner->plat.evq_head != owner->plat.evq_tail && owner->plat.evq_buf[prev].type == MKGUI_PLAT_MOTION) {
				owner->plat.evq_buf[prev] = pev;
			} else {
				evq_push_ctx(&owner->plat, &pev);
			}
			return 0;
		} break;

		case WM_LBUTTONDOWN: {
			pev.type = MKGUI_PLAT_BUTTON_PRESS;
			pev.x = (int16_t)LOWORD(lp);
			pev.y = (int16_t)HIWORD(lp);
			pev.button = 1;
			pev.keymod = platform_get_keymod();
			evq_push_ctx(&owner->plat, &pev);
			SetCapture(hwnd);
			return 0;
		} break;

		case WM_LBUTTONUP: {
			pev.type = MKGUI_PLAT_BUTTON_RELEASE;
			pev.x = (int16_t)LOWORD(lp);
			pev.y = (int16_t)HIWORD(lp);
			pev.button = 1;
			evq_push_ctx(&owner->plat, &pev);
			ReleaseCapture();
			return 0;
		} break;

		case WM_RBUTTONDOWN: {
			pev.type = MKGUI_PLAT_BUTTON_PRESS;
			pev.x = (int16_t)LOWORD(lp);
			pev.y = (int16_t)HIWORD(lp);
			pev.button = 3;
			evq_push_ctx(&owner->plat, &pev);
			return 0;
		} break;

		case WM_RBUTTONUP: {
			pev.type = MKGUI_PLAT_BUTTON_RELEASE;
			pev.x = (int16_t)LOWORD(lp);
			pev.y = (int16_t)HIWORD(lp);
			pev.button = 3;
			evq_push_ctx(&owner->plat, &pev);
			return 0;
		} break;

		case WM_MOUSEWHEEL: {
			int16_t delta = (int16_t)HIWORD(wp);
			pev.type = MKGUI_PLAT_BUTTON_PRESS;
			POINT pt = { (int16_t)LOWORD(lp), (int16_t)HIWORD(lp) };
			ScreenToClient(hwnd, &pt);
			pev.x = pt.x;
			pev.y = pt.y;
			pev.button = delta > 0 ? 4 : 5;
			evq_push_ctx(&owner->plat, &pev);
			pev.type = MKGUI_PLAT_BUTTON_RELEASE;
			evq_push_ctx(&owner->plat, &pev);
			return 0;
		} break;

		case WM_MOUSEHWHEEL: {
			int16_t delta = (int16_t)HIWORD(wp);
			pev.type = MKGUI_PLAT_BUTTON_PRESS;
			POINT pt = { (int16_t)LOWORD(lp), (int16_t)HIWORD(lp) };
			ScreenToClient(hwnd, &pt);
			pev.x = pt.x;
			pev.y = pt.y;
			pev.button = delta > 0 ? 7 : 6;
			evq_push_ctx(&owner->plat, &pev);
			pev.type = MKGUI_PLAT_BUTTON_RELEASE;
			evq_push_ctx(&owner->plat, &pev);
			return 0;
		} break;

		case WM_KEYDOWN: {
			pev.type = MKGUI_PLAT_KEY;
			pev.keysym = platform_translate_vk(wp);
			pev.keymod = platform_get_keymod();
			pev.text[0] = '\0';
			pev.text_len = 0;
			evq_push_ctx(&owner->plat, &pev);
			return 0;
		} break;

		case WM_CHAR: {
			uint32_t cp = (uint32_t)wp;
			if(cp >= 32) {
				pev.type = MKGUI_PLAT_KEY;
				pev.keysym = cp < 128 ? cp : 0;
				pev.keymod = platform_get_keymod();
				if(cp < 0x80) {
					pev.text[0] = (char)cp;
					pev.text[1] = '\0';
					pev.text_len = 1;
				} else if(cp < 0x800) {
					pev.text[0] = (char)(0xc0 | (cp >> 6));
					pev.text[1] = (char)(0x80 | (cp & 0x3f));
					pev.text[2] = '\0';
					pev.text_len = 2;
				} else {
					pev.text[0] = (char)(0xe0 | (cp >> 12));
					pev.text[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
					pev.text[2] = (char)(0x80 | (cp & 0x3f));
					pev.text[3] = '\0';
					pev.text_len = 3;
				}
				evq_push_ctx(&owner->plat, &pev);
			}
			return 0;
		} break;

		case WM_ENTERSIZEMOVE: {
			SetTimer(hwnd, 1, 16, NULL);
			return 0;
		} break;

		case WM_EXITSIZEMOVE: {
			KillTimer(hwnd, 1);
			return 0;
		} break;

		case WM_TIMER: {
			if(wp == 1 && pev.popup_idx < 0) {
				int64_t now;
				QueryPerformanceCounter((LARGE_INTEGER *)&now);
				double dt = (double)(now - owner->anim_prev) / (double)owner->perf_freq;
				owner->anim_prev = now;
				owner->anim_time += dt;
				for(uint32_t ti = 0; ti < owner->timer_count; ++ti) {
					struct mkgui_timer *tmr = &owner->timers[ti];
					if(tmr->active && tmr->handle && WaitForSingleObject(tmr->handle, 0) == WAIT_OBJECT_0) {
						++tmr->fire_count;
						LARGE_INTEGER next;
						next.QuadPart = tmr->epoch.QuadPart + (int64_t)(tmr->fire_count * (tmr->interval_ns / 100));
						SetWaitableTimer(tmr->handle, &next, 0, NULL, NULL, FALSE);
						if(tmr->cb) {
							tmr->cb(owner, tmr->id, tmr->userdata);
						}
					}
				}
				mkgui_resize_render(owner);
			}
			return 0;
		} break;

		case WM_CLOSE: {
			pev.type = MKGUI_PLAT_CLOSE;
			evq_push_ctx(&owner->plat, &pev);
			return 0;
		} break;

		case WM_MOUSELEAVE: {
			pev.type = MKGUI_PLAT_LEAVE;
			evq_push_ctx(&owner->plat, &pev);
			return 0;
		} break;

		default: break;
	}

	return DefWindowProcA(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// Framebuffer helpers
// ---------------------------------------------------------------------------

// [=]===^=[ platform_fb_create_dib ]==============================[=]
static uint32_t platform_fb_create_dib(HDC hdc_ref, HDC *hdc_mem, HBITMAP *hbmp, HBITMAP *hbmp_old, uint32_t **pixels, int32_t w, int32_t h) {
	BITMAPINFO bmi;
	memset(&bmi, 0, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = w;
	bmi.bmiHeader.biHeight = -h;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	*hdc_mem = CreateCompatibleDC(hdc_ref);
	*hbmp = CreateDIBSection(*hdc_mem, &bmi, DIB_RGB_COLORS, (void **)pixels, NULL, 0);
	*hbmp_old = (HBITMAP)SelectObject(*hdc_mem, *hbmp);
	return 1;
}

// [=]===^=[ platform_fb_destroy_dib ]=============================[=]
static void platform_fb_destroy_dib(HDC *hdc_mem, HBITMAP *hbmp, HBITMAP *hbmp_old) {
	if(*hdc_mem) {
		SelectObject(*hdc_mem, *hbmp_old);
		DeleteObject(*hbmp);
		DeleteDC(*hdc_mem);
		*hdc_mem = NULL;
		*hbmp = NULL;
	}
}

// ---------------------------------------------------------------------------
// Platform init / destroy
// ---------------------------------------------------------------------------

static uint32_t wc_registered;

// [=]===^=[ platform_register_class ]=============================[=]
static void platform_register_class(void) {
	if(wc_registered) {
		return;
	}
	WNDCLASSEXA wc;
	memset(&wc, 0, sizeof(wc));
	wc.cbSize = sizeof(wc);
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = mkgui_wndproc;
	wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
	wc.lpszClassName = "mkgui";
	RegisterClassExA(&wc);
	wc_registered = 1;
}

// [=]===^=[ platform_init ]=======================================[=]
static uint32_t platform_init(struct mkgui_ctx *ctx, const char *title, int32_t w, int32_t h) {
	struct mkgui_platform *plat = &ctx->plat;
	platform_register_class();

	RECT rc = { 0, 0, w, h };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

	plat->hwnd = CreateWindowExA(0, "mkgui", title, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
		NULL, NULL, GetModuleHandleA(NULL), NULL);

	if(!plat->hwnd) {
		return 0;
	}

	ctx->win_w = w;
	ctx->win_h = h;

	HDC hdc = GetDC(plat->hwnd);
	platform_fb_create_dib(hdc, &plat->hdc_mem, &plat->hbmp, &plat->hbmp_old, &ctx->pixels, w, h);
	ReleaseDC(plat->hwnd, hdc);

	plat->cursor_default = LoadCursorA(NULL, IDC_ARROW);
	plat->cursor_h_resize = LoadCursorA(NULL, IDC_SIZEWE);
	plat->cursor_active = 0;

	ShowWindow(plat->hwnd, SW_SHOW);
	UpdateWindow(plat->hwnd);

	return 1;
}

// [=]===^=[ platform_init_child ]=================================[=]
static uint32_t platform_init_child(struct mkgui_ctx *ctx, struct mkgui_ctx *parent, const char *title, int32_t w, int32_t h) {
	struct mkgui_platform *plat = &ctx->plat;
	platform_register_class();

	RECT rc = { 0, 0, w, h };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

	plat->hwnd = CreateWindowExA(0, "mkgui", title, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
		parent->plat.hwnd, NULL, GetModuleHandleA(NULL), NULL);

	if(!plat->hwnd) {
		return 0;
	}

	plat->is_child = 1;
	plat->parent_hwnd = parent->plat.hwnd;

	ctx->win_w = w;
	ctx->win_h = h;

	HDC hdc = GetDC(plat->hwnd);
	platform_fb_create_dib(hdc, &plat->hdc_mem, &plat->hbmp, &plat->hbmp_old, &ctx->pixels, w, h);
	ReleaseDC(plat->hwnd, hdc);

	plat->cursor_default = LoadCursorA(NULL, IDC_ARROW);
	plat->cursor_h_resize = LoadCursorA(NULL, IDC_SIZEWE);
	plat->cursor_active = 0;

	ShowWindow(plat->hwnd, SW_SHOW);
	UpdateWindow(plat->hwnd);

	return 1;
}

// [=]===^=[ platform_destroy ]====================================[=]
static void platform_destroy(struct mkgui_ctx *ctx) {
	struct mkgui_platform *plat = &ctx->plat;
	platform_fb_destroy_dib(&plat->hdc_mem, &plat->hbmp, &plat->hbmp_old);
	if(plat->hwnd) {
		DestroyWindow(plat->hwnd);
		plat->hwnd = NULL;
	}
}

// [=]===^=[ platform_set_cursor ]=================================[=]
static void platform_set_cursor(struct mkgui_ctx *ctx, uint32_t cursor_type) {
	struct mkgui_platform *plat = &ctx->plat;
	if(plat->cursor_active == cursor_type) {
		return;
	}
	plat->cursor_active = cursor_type;
	SetCursor(cursor_type ? plat->cursor_h_resize : plat->cursor_default);
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
	platform_fb_destroy_dib(&plat->hdc_mem, &plat->hbmp, &plat->hbmp_old);
	HDC hdc = GetDC(plat->hwnd);
	platform_fb_create_dib(hdc, &plat->hdc_mem, &plat->hbmp, &plat->hbmp_old, &ctx->pixels, ctx->win_w, ctx->win_h);
	ReleaseDC(plat->hwnd, hdc);
}

// ---------------------------------------------------------------------------
// Blit / flush
// ---------------------------------------------------------------------------

// [=]===^=[ platform_blit ]=======================================[=]
static void platform_blit(struct mkgui_ctx *ctx) {
	struct mkgui_platform *plat = &ctx->plat;
	HDC hdc = GetDC(plat->hwnd);
	SelectClipRgn(hdc, NULL);
	for(uint32_t i = 0; i < ctx->glview_count; ++i) {
		if(ctx->glviews[i].plat.hwnd && ctx->glviews[i].visible) {
			int32_t gx = ctx->glviews[i].last_x;
			int32_t gy = ctx->glviews[i].last_y;
			int32_t gw = ctx->glviews[i].last_w;
			int32_t gh = ctx->glviews[i].last_h;
			ExcludeClipRect(hdc, gx, gy, gx + gw, gy + gh);
		}
	}
	BitBlt(hdc, 0, 0, ctx->win_w, ctx->win_h, plat->hdc_mem, 0, 0, SRCCOPY);
	ReleaseDC(plat->hwnd, hdc);
}

// [=]===^=[ platform_blit_region ]================================[=]
static void platform_blit_region(struct mkgui_ctx *ctx, int32_t x, int32_t y, int32_t w, int32_t h) {
	struct mkgui_platform *plat = &ctx->plat;
	HDC hdc = GetDC(plat->hwnd);
	SelectClipRgn(hdc, NULL);
	for(uint32_t i = 0; i < ctx->glview_count; ++i) {
		if(ctx->glviews[i].plat.hwnd && ctx->glviews[i].visible) {
			int32_t gx = ctx->glviews[i].last_x;
			int32_t gy = ctx->glviews[i].last_y;
			int32_t gw = ctx->glviews[i].last_w;
			int32_t gh = ctx->glviews[i].last_h;
			ExcludeClipRect(hdc, gx, gy, gx + gw, gy + gh);
		}
	}
	BitBlt(hdc, x, y, w, h, plat->hdc_mem, x, y, SRCCOPY);
	ReleaseDC(plat->hwnd, hdc);
}

// [=]===^=[ platform_popup_blit ]=================================[=]
static void platform_popup_blit(struct mkgui_ctx *ctx, struct mkgui_popup *p) {
	(void)ctx;
	HDC hdc = GetDC(p->plat.hwnd);
	BitBlt(hdc, 0, 0, p->w, p->h, p->plat.hdc_mem, 0, 0, SRCCOPY);
	ReleaseDC(p->plat.hwnd, hdc);
}

// [=]===^=[ platform_flush ]======================================[=]
static void platform_flush(struct mkgui_ctx *ctx) {
	(void)ctx;
	GdiFlush();
}

// ---------------------------------------------------------------------------
// Popup windows
// ---------------------------------------------------------------------------

// [=]===^=[ platform_popup_init ]=================================[=]
static uint32_t platform_popup_init(struct mkgui_ctx *ctx, struct mkgui_popup *p, int32_t x, int32_t y, int32_t w, int32_t h) {
	struct mkgui_platform *plat = &ctx->plat;

	p->plat.hwnd = CreateWindowExA(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, "mkgui", "",
		WS_POPUP | WS_VISIBLE, x, y, w, h,
		plat->hwnd, NULL, GetModuleHandleA(NULL), NULL);

	if(!p->plat.hwnd) {
		return 0;
	}

	HDC hdc = GetDC(p->plat.hwnd);
	platform_fb_create_dib(hdc, &p->plat.hdc_mem, &p->plat.hbmp, &p->plat.hbmp_old, &p->pixels, w, h);
	ReleaseDC(p->plat.hwnd, hdc);

	return 1;
}

// [=]===^=[ platform_popup_fini ]=================================[=]
static void platform_popup_fini(struct mkgui_ctx *ctx, struct mkgui_popup *p) {
	(void)ctx;
	platform_fb_destroy_dib(&p->plat.hdc_mem, &p->plat.hbmp, &p->plat.hbmp_old);
	if(p->plat.hwnd) {
		DestroyWindow(p->plat.hwnd);
		p->plat.hwnd = NULL;
	}
}

// ---------------------------------------------------------------------------
// Screen size
// ---------------------------------------------------------------------------

// [=]===^=[ platform_screen_size ]================================[=]
static void platform_screen_size(struct mkgui_ctx *ctx, int32_t *sw, int32_t *sh) {
	(void)ctx;
	*sw = GetSystemMetrics(SM_CXSCREEN);
	*sh = GetSystemMetrics(SM_CYSCREEN);
}

// [=]===^=[ platform_set_min_size ]================================[=]
static void platform_set_min_size(struct mkgui_ctx *ctx, int32_t min_w, int32_t min_h) {
	ctx->plat.min_w = min_w;
	ctx->plat.min_h = min_h;
}

// ---------------------------------------------------------------------------
// Coordinate translation
// ---------------------------------------------------------------------------

// [=]===^=[ platform_translate_coords ]===========================[=]
static void platform_translate_coords(struct mkgui_ctx *ctx, int32_t lx, int32_t ly, int32_t *sx, int32_t *sy) {
	POINT pt = { lx, ly };
	ClientToScreen(ctx->plat.hwnd, &pt);
	*sx = pt.x;
	*sy = pt.y;
}

// ---------------------------------------------------------------------------
// Event translation
// ---------------------------------------------------------------------------

// [=]===^=[ platform_pump_messages ]===============================[=]
static void platform_pump_messages(void) {
	MSG msg;
	while(PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}
}

// [=]===^=[ platform_wait_event ]=================================[=]
static void platform_wait_event(struct mkgui_ctx *ctx, int32_t timeout_ms) {
	if(timeout_ms == 0 || ctx->plat.evq_head != ctx->plat.evq_tail) {
		return;
	}
	platform_pump_messages();
	if(ctx->plat.evq_head != ctx->plat.evq_tail) {
		return;
	}
	DWORD wait_ms = (DWORD)timeout_ms;
	for(uint32_t i = 0; i < ctx->timer_count; ++i) {
		if(ctx->timers[i].active) {
			DWORD timer_ms = (DWORD)(ctx->timers[i].interval_ns / 1000000);
			if(timer_ms < 1) {
				timer_ms = 1;
			}
			if(timer_ms < wait_ms) {
				wait_ms = timer_ms;
			}
		}
	}
	MsgWaitForMultipleObjects(0, NULL, FALSE, wait_ms, QS_ALLINPUT);
}

// [=]===^=[ platform_pending ]====================================[=]
static uint32_t platform_pending(struct mkgui_ctx *ctx) {
	platform_pump_messages();
	return ctx->plat.evq_head != ctx->plat.evq_tail;
}

// [=]===^=[ platform_next_event ]=================================[=]
static void platform_next_event(struct mkgui_ctx *ctx, struct mkgui_plat_event *pev) {
	if(!evq_pop_ctx(&ctx->plat, pev)) {
		memset(pev, 0, sizeof(*pev));
		pev->popup_idx = -1;
		pev->type = MKGUI_PLAT_NONE;
	}
}

// ---------------------------------------------------------------------------
// Font rendering (GDI)
// ---------------------------------------------------------------------------

static HDC plat_font_dc;
static HFONT plat_font_handle;

// [=]===^=[ platform_font_init ]==================================[=]
static void platform_font_init(struct mkgui_ctx *ctx) {
	plat_font_dc = CreateCompatibleDC(NULL);
	if(!plat_font_dc) {
		ctx->font_ascent = 11;
		ctx->font_height = 13;
		ctx->char_width = 7;
		return;
	}

	plat_font_handle = CreateFontA(
		-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
		ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
		"Segoe UI");

	if(!plat_font_handle) {
		plat_font_handle = CreateFontA(
			-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
			ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
			"Tahoma");
	}

	if(!plat_font_handle) {
		ctx->font_ascent = 11;
		ctx->font_height = 13;
		ctx->char_width = 7;
		return;
	}

	SelectObject(plat_font_dc, plat_font_handle);

	TEXTMETRICA tm;
	GetTextMetricsA(plat_font_dc, &tm);
	ctx->font_ascent = tm.tmAscent;
	ctx->font_height = tm.tmAscent + tm.tmDescent;

	MAT2 mat = { {0,1}, {0,0}, {0,0}, {0,1} };

	for(uint32_t c = MKGUI_GLYPH_FIRST; c <= MKGUI_GLYPH_LAST; ++c) {
		GLYPHMETRICS gm;
		DWORD buf_size = GetGlyphOutlineA(plat_font_dc, c, GGO_GRAY8_BITMAP, &gm, 0, NULL, &mat);
		if(buf_size == GDI_ERROR || buf_size == 0) {
			struct mkgui_glyph *g = &ctx->glyphs[c - MKGUI_GLYPH_FIRST];
			ABC abc;
			if(GetCharABCWidthsA(plat_font_dc, c, c, &abc)) {
				g->advance = abc.abcA + (int32_t)abc.abcB + abc.abcC;
			} else {
				g->advance = 7;
			}
			continue;
		}

		uint8_t *tmp = (uint8_t *)malloc(buf_size);
		if(!tmp) {
			continue;
		}
		GetGlyphOutlineA(plat_font_dc, c, GGO_GRAY8_BITMAP, &gm, buf_size, tmp, &mat);

		struct mkgui_glyph *g = &ctx->glyphs[c - MKGUI_GLYPH_FIRST];
		g->width = (int32_t)gm.gmBlackBoxX;
		g->height = (int32_t)gm.gmBlackBoxY;
		g->bearing_x = gm.gmptGlyphOrigin.x;
		g->bearing_y = gm.gmptGlyphOrigin.y;
		g->advance = gm.gmCellIncX;

		uint32_t pitch = (gm.gmBlackBoxX + 3) & ~3u;
		uint32_t bmp_size = (uint32_t)(g->width * g->height);
		if(bmp_size > MKGUI_GLYPH_MAX_BMP) {
			bmp_size = MKGUI_GLYPH_MAX_BMP;
		}

		for(uint32_t y = 0; y < (uint32_t)g->height && y * (uint32_t)g->width < bmp_size; ++y) {
			for(uint32_t x = 0; x < (uint32_t)g->width; ++x) {
				uint32_t idx = y * (uint32_t)g->width + x;
				if(idx >= bmp_size) {
					break;
				}
				uint8_t val = tmp[y * pitch + x];
				g->bitmap[idx] = (uint8_t)(val * 255 / 64);
			}
		}

		free(tmp);
	}

	ctx->char_width = ctx->glyphs['M' - MKGUI_GLYPH_FIRST].advance;
	if(ctx->char_width == 0) {
		ctx->char_width = 7;
	}
}

// [=]===^=[ platform_font_fini ]==================================[=]
static void platform_font_fini(struct mkgui_ctx *ctx) {
	(void)ctx;
	if(plat_font_handle) {
		DeleteObject(plat_font_handle);
		plat_font_handle = NULL;
	}
	if(plat_font_dc) {
		DeleteDC(plat_font_dc);
		plat_font_dc = NULL;
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

	if(!OpenClipboard(ctx->plat.hwnd)) {
		return;
	}
	EmptyClipboard();
	HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, len + 1);
	if(hg) {
		char *p = (char *)GlobalLock(hg);
		memcpy(p, text, len);
		p[len] = '\0';
		GlobalUnlock(hg);
		SetClipboardData(CF_TEXT, hg);
	}
	CloseClipboard();
}

// [=]===^=[ platform_clipboard_get ]==============================[=]
static uint32_t platform_clipboard_get(struct mkgui_ctx *ctx, char *buf, uint32_t buf_size) {
	if(!OpenClipboard(ctx->plat.hwnd)) {
		return 0;
	}
	HANDLE h = GetClipboardData(CF_TEXT);
	if(!h) {
		CloseClipboard();
		return 0;
	}
	const char *p = (const char *)GlobalLock(h);
	if(!p) {
		CloseClipboard();
		return 0;
	}
	uint32_t len = (uint32_t)strlen(p);
	if(len >= buf_size) {
		len = buf_size - 1;
	}
	memcpy(buf, p, len);
	buf[len] = '\0';
	GlobalUnlock(h);
	CloseClipboard();
	return len;
}

// [=]===^=[ platform_clipboard_get_alloc ]========================[=]
static char *platform_clipboard_get_alloc(struct mkgui_ctx *ctx, uint32_t *out_len) {
	*out_len = 0;
	if(!OpenClipboard(ctx->plat.hwnd)) {
		return NULL;
	}
	HANDLE h = GetClipboardData(CF_TEXT);
	if(!h) {
		CloseClipboard();
		return NULL;
	}
	const char *p = (const char *)GlobalLock(h);
	if(!p) {
		CloseClipboard();
		return NULL;
	}
	uint32_t len = (uint32_t)strlen(p);
	char *buf = (char *)malloc(len + 1);
	if(!buf) {
		GlobalUnlock(h);
		CloseClipboard();
		return NULL;
	}
	memcpy(buf, p, len);
	buf[len] = '\0';
	GlobalUnlock(h);
	CloseClipboard();
	*out_len = len;
	return buf;
}

// ---------------------------------------------------------------------------
// GL view child window
// ---------------------------------------------------------------------------

static uint32_t glview_wc_registered;

// [=]===^=[ glview_wndproc ]======================================[=]
static LRESULT CALLBACK glview_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	if(msg == WM_ERASEBKGND) {
		return 1;
	}
	return DefWindowProcA(hwnd, msg, wp, lp);
}

// [=]===^=[ platform_glview_create ]==============================[=]
static uint32_t platform_glview_create(struct mkgui_ctx *ctx, struct mkgui_glview_data *gv, int32_t x, int32_t y, int32_t w, int32_t h) {
	if(!glview_wc_registered) {
		WNDCLASSEXA wc;
		memset(&wc, 0, sizeof(wc));
		wc.cbSize = sizeof(wc);
		wc.style = CS_OWNDC;
		wc.lpfnWndProc = glview_wndproc;
		wc.lpszClassName = "mkgui_glview";
		RegisterClassExA(&wc);
		glview_wc_registered = 1;
	}
	gv->plat.hwnd = CreateWindowExA(0, "mkgui_glview", "", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
		x, y, w, h, ctx->plat.hwnd, NULL, GetModuleHandle(NULL), NULL);
	if(!gv->plat.hwnd) {
		return 0;
	}
	return 1;
}

// [=]===^=[ platform_glview_destroy ]=============================[=]
static void platform_glview_destroy(struct mkgui_ctx *ctx, struct mkgui_glview_data *gv) {
	(void)ctx;
	if(gv->plat.hwnd) {
		DestroyWindow(gv->plat.hwnd);
		gv->plat.hwnd = NULL;
	}
}

// [=]===^=[ platform_glview_reposition ]==========================[=]
static void platform_glview_reposition(struct mkgui_ctx *ctx, struct mkgui_glview_data *gv, int32_t x, int32_t y, int32_t w, int32_t h) {
	(void)ctx;
	if(!gv->plat.hwnd) {
		return;
	}
	MoveWindow(gv->plat.hwnd, x, y, w, h, TRUE);
}

// [=]===^=[ platform_glview_show ]================================[=]
static void platform_glview_show(struct mkgui_ctx *ctx, struct mkgui_glview_data *gv, uint32_t visible) {
	(void)ctx;
	if(!gv->plat.hwnd) {
		return;
	}
	ShowWindow(gv->plat.hwnd, visible ? SW_SHOW : SW_HIDE);
}

