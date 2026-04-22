// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT
//
// Public accessors for the native window handles behind a MKGUI_GLVIEW.
// Kept out of mkgui.h so general consumers don't pull <windows.h> / <X11/Xlib.h>.
// Include this header only if your app creates an OpenGL context on a glview;
// include the appropriate platform header first so the types are defined.

#ifndef MKGUI_GLVIEW_H
#define MKGUI_GLVIEW_H

#include "mkgui.h"

#ifdef _WIN32
MKGUI_API HWND mkgui_glview_get_hwnd(struct mkgui_ctx *ctx, uint32_t id);
#else
MKGUI_API Window mkgui_glview_get_x11_window(struct mkgui_ctx *ctx, uint32_t id);
MKGUI_API Display *mkgui_glview_get_x11_display(struct mkgui_ctx *ctx);
#endif

#endif
