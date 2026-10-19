// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../mkgui.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <pthread.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif

struct native_window {
#ifdef _WIN32
	HWND handle;
#else
	Window handle;
#endif
};

struct dialog_action {
	char *title;
	uint32_t cancel;
};

static uint32_t checks;
static uint32_t failures;
static uint32_t close_calls;
#ifndef _WIN32
static Display *display;
#endif

// [=]===^=[ check ]============================================================================[=]
static void check(uint32_t condition, char *message) {
	++checks;

	if(!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		++failures;

	}
}

#ifdef _WIN32
struct native_search {
	char *title;
	struct native_window window;
};

// [=]===^=[ match_window ]=====================================================================[=]
static BOOL CALLBACK match_window(HWND handle, LPARAM parameter) {
	struct native_search *search = (struct native_search *)parameter;
	DWORD process;
	char title[256];
	GetWindowThreadProcessId(handle, &process);

	if(process == GetCurrentProcessId() && GetWindowTextA(handle, title, sizeof(title)) && strcmp(title, search->title) == 0) {
		search->window.handle = handle;
		return FALSE;

	}

	return TRUE;

}

// [=]===^=[ native_find ]======================================================================[=]
static struct native_window native_find(char *title) {
	struct native_search search = {.title = title};
	EnumWindows(match_window, (LPARAM)&search);
	return search.window;

}
#else
// [=]===^=[ native_error ]=====================================================================[=]
static int native_error(Display *connection, XErrorEvent *event) {
	if(connection != display || event->error_code != BadWindow) {
		fprintf(stderr, "FAIL: X11 error %u in native test driver\n", event->error_code);
		exit(1);

	}

	return 0;

}

// [=]===^=[ native_search ]====================================================================[=]
static Window native_search(Window root, char *title) {
	Atom type;
	int format;
	unsigned long count, remaining;
	unsigned char *data = NULL;
	Window found = 0;
	Atom pid = XInternAtom(display, "_NET_WM_PID", False);

	if(XGetWindowProperty(display, root, pid, 0, 1, False, XA_CARDINAL, &type, &format, &count, &remaining, &data) == Success) {
		if(type == XA_CARDINAL && format == 32 && count == 1 && *(unsigned long *)data == (unsigned long)getpid()) {
			char *name = NULL;

			if(XFetchName(display, root, &name) && name) {
				if(strcmp(name, title) == 0) {
					found = root;

				}

				XFree(name);

			}
		}

		XFree(data);

	}

	if(!found) {
		Window parent, tree_root, *children = NULL;
		unsigned int child_count = 0;

		if(XQueryTree(display, root, &tree_root, &parent, &children, &child_count)) {
			for(uint32_t i = 0; i < child_count && !found; ++i) {
				found = native_search(children[i], title);

			}

			XFree(children);

		}
	}

	return found;

}

// [=]===^=[ native_find ]======================================================================[=]
static struct native_window native_find(char *title) {
	struct native_window window = {.handle = native_search(DefaultRootWindow(display), title)};
	return window;

}
#endif

// [=]===^=[ native_key ]=======================================================================[=]
static void native_key(struct native_window window, uint32_t keysym) {
#ifdef _WIN32
	if(keysym < 128) {
		PostMessageA(window.handle, WM_CHAR, keysym, 1);

	} else {
		WPARAM key = keysym == MKGUI_KEY_RETURN ? VK_RETURN : VK_ESCAPE;
		PostMessageA(window.handle, WM_KEYDOWN, key, 1);
		PostMessageA(window.handle, WM_KEYUP, key, 1);

	}
#else
	XEvent event = {0};
	event.xkey.type = KeyPress;
	event.xkey.display = display;
	event.xkey.window = window.handle;
	event.xkey.root = DefaultRootWindow(display);
	event.xkey.same_screen = True;
	event.xkey.keycode = XKeysymToKeycode(display, keysym);
	XSendEvent(display, window.handle, False, KeyPressMask, &event);
	event.xkey.type = KeyRelease;
	XSendEvent(display, window.handle, False, KeyReleaseMask, &event);
	XFlush(display);
#endif

}

// [=]===^=[ native_click ]=====================================================================[=]
static void native_click(struct mkgui_window *win, struct native_window window, uint32_t id) {
	int32_t x, y, width, height;
	mkgui_get_geometry(win, id, &x, &y, &width, &height);
	x += width / 2;
	y += height / 2;
#ifdef _WIN32
	LPARAM position = MAKELPARAM((uint16_t)x, (uint16_t)y);
	PostMessageA(window.handle, WM_LBUTTONDOWN, MK_LBUTTON, position);
	PostMessageA(window.handle, WM_LBUTTONUP, 0, position);
#else
	XEvent event = {0};
	event.xbutton.type = ButtonPress;
	event.xbutton.display = display;
	event.xbutton.window = window.handle;
	event.xbutton.root = DefaultRootWindow(display);
	event.xbutton.same_screen = True;
	event.xbutton.x = x;
	event.xbutton.y = y;
	event.xbutton.button = Button1;
	XSendEvent(display, window.handle, False, ButtonPressMask, &event);
	event.xbutton.type = ButtonRelease;
	XSendEvent(display, window.handle, False, ButtonReleaseMask, &event);
	XFlush(display);
#endif

}

// [=]===^=[ native_close ]=====================================================================[=]
static void native_close(struct native_window window) {
#ifdef _WIN32
	PostMessageA(window.handle, WM_CLOSE, 0, 0);
#else
	XEvent event = {0};
	event.xclient.type = ClientMessage;
	event.xclient.window = window.handle;
	event.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", False);
	event.xclient.format = 32;
	event.xclient.data.l[0] = (long)XInternAtom(display, "WM_DELETE_WINDOW", False);
	XSendEvent(display, window.handle, False, NoEventMask, &event);
	XFlush(display);
#endif

}

// [=]===^=[ await_event ]======================================================================[=]
static uint32_t await_event(struct mkgui_window *win, uint32_t type, uint32_t id) {
	uint64_t deadline = mkgui_now_ns() + UINT64_C(2000000000);
	struct mkgui_event event;

	while(mkgui_now_ns() < deadline) {
		while(mkgui_window_poll(win, &event)) {
			if(event.type == type && event.id == id) {
				return 1;

			}
		}

		mkgui_ctx_wait_until(mkgui_window_get_ctx(win), mkgui_now_ns() + UINT64_C(1000000));

	}

	return 0;

}

// [=]===^=[ await_closed ]=====================================================================[=]
static uint32_t await_closed(struct mkgui_window *win, char *title) {
	uint64_t deadline = mkgui_now_ns() + UINT64_C(2000000000);
	mkgui_window_invalidate(win);

	while(mkgui_now_ns() < deadline) {
		mkgui_ctx_wait_until(mkgui_window_get_ctx(win), mkgui_now_ns() + UINT64_C(1000000));

		if(!native_find(title).handle) {
			return 1;

		}
	}

	return 0;

}

// [=]===^=[ timer_tick ]=======================================================================[=]
static void timer_tick(struct mkgui_window *win, uint32_t id, void *userdata) {
	uint32_t *calls = userdata;
	++*calls;

	if(*calls == 3) {
		mkgui_window_remove_timer(win, id);

	}
}

// [=]===^=[ secondary_event ]==================================================================[=]
static void secondary_event(struct mkgui_window *win, struct mkgui_event *event, void *userdata) {
	if(event->type == MKGUI_EVENT_CLOSE) {
		++close_calls;
		mkgui_window_close(win);

	}
}

// [=]===^=[ dialog_driver ]====================================================================[=]
#ifdef _WIN32
static DWORD WINAPI dialog_driver(LPVOID userdata) {
#else
static void *dialog_driver(void *userdata) {
#endif
	struct dialog_action *action = userdata;

	for(uint32_t i = 0; i < 500; ++i) {
		struct native_window window = native_find(action->title);

		if(window.handle) {
#ifdef _WIN32
			ShowWindowAsync(window.handle, SW_HIDE);
#else
			XUnmapWindow(display, window.handle);
			XFlush(display);
#endif

			if(action->cancel == 2) {
				native_close(window);

			} else if(action->cancel) {
				native_key(window, MKGUI_KEY_ESCAPE);

			} else {
				native_key(window, MKGUI_KEY_RETURN);

			}

			return 0;

		}

#ifdef _WIN32
		Sleep(10);
#else
		struct timespec pause = {.tv_nsec = 10000000};
		nanosleep(&pause, NULL);
#endif

	}

	fprintf(stderr, "FAIL: modal dialog did not appear\n");
	exit(1);

}

// [=]===^=[ main ]=============================================================================[=]
int main(void) {
#ifndef _WIN32
	if(!XInitThreads()) {
		fprintf(stderr, "FAIL: Xlib thread initialization\n");
		return 1;

	}

	display = XOpenDisplay(NULL);

	if(!display) {
		fprintf(stderr, "FAIL: no X display\n");
		return 1;

	}

	XSetErrorHandler(native_error);
#endif

	struct mkgui_ctx *ctx = mkgui_ctx_create();
	check(ctx != NULL, "context creation");

	if(ctx) {
		struct mkgui_widget widgets[] = {
			MKGUI_W(MKGUI_WINDOW, 1, "mkgui shared events", "", 0, 400, 300, 0, MKGUI_WINDOW_HIDDEN, 0),
			MKGUI_W(MKGUI_VBOX, 2, "", "", 1, 0, 0, 0, 0, 0),
			MKGUI_W(MKGUI_INPUT, 3, "", "", 2, 0, 0, 0, 0, 0),
			MKGUI_W(MKGUI_CHECKBOX, 4, "Check", "", 2, 0, 0, 0, 0, 0),
			MKGUI_W(MKGUI_BUTTON, 5, "Button", "", 2, 0, 0, 0, 0, 0),
		};
		struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, sizeof(widgets) / sizeof(widgets[0]), NULL, 0, 0);
		check(win != NULL, "window creation");

		if(win) {
			mkgui_ctx_wait_until(ctx, mkgui_now_ns());
			struct native_window native = native_find("mkgui shared events");
			check(native.handle != 0, "find this process's native window");

			if(native.handle) {
				mkgui_set_focus(win, 3);
#ifdef _WIN32
				native_key(native, 'a');
				check(await_event(win, MKGUI_EVENT_INPUT_CHANGED, 3), "native character produces input event through shared library");
				check(strcmp(mkgui_input_get(win, 3), "a") == 0, "native input changes consumer-visible text");
#else
				mkgui_input_set(win, 3, "a");
#endif
				native_key(native, MKGUI_KEY_RETURN);
				check(await_event(win, MKGUI_EVENT_INPUT_SUBMIT, 3), "native Enter submits input");
				native_click(win, native, 4);
				check(await_event(win, MKGUI_EVENT_CHECKBOX_CHANGED, 4), "native mouse click changes checkbox");
				check(mkgui_checkbox_get(win, 4) != 0, "checkbox state crosses library boundary");
				native_click(win, native, 5);
				check(await_event(win, MKGUI_EVENT_CLICK, 5), "native mouse click activates button");

				for(uint32_t kind = 0; kind < 6; ++kind) {
					struct dialog_action action = {.title = kind == 3 ? "Color Picker" : kind == 5 ? "Icons" : kind == 4 ? "Open File" : "mkgui shared modal", .cancel = kind >= 4 ? 2 : kind == 1 || kind == 3};
					char text[64] = "unchanged";
#ifdef _WIN32
					HANDLE thread = CreateThread(NULL, 0, dialog_driver, &action, 0, NULL);
					uint32_t started = thread != NULL;
#else
					pthread_t thread;
					uint32_t started = pthread_create(&thread, NULL, dialog_driver, &action) == 0;
#endif
					check(started, "modal driver creation");

					if(started) {
						uint32_t accepted = 0;
						uint32_t color = 0x998877;

						switch(kind) {
							case 0:
							case 1: {
								accepted = mkgui_dialog_input(win, action.title, "Enter text", "shared", text, sizeof(text));

							} break;

							case 2: {
								accepted = mkgui_dialog_message(win, action.title, "Message", MKGUI_DLG_ICON_NONE, MKGUI_DLG_BUTTONS_OK);

							} break;

							case 3: {
								accepted = mkgui_dialog_color(win, 0x112233, &color);

							} break;

							case 4: {
								struct mkgui_file_dialog_opts options = {.start_path = "."};
								accepted = mkgui_dialog_open(win, &options);

							} break;

							case 5: {
								accepted = mkgui_icon_browser(win, 16, text, sizeof(text));

							} break;
						}
#ifdef _WIN32
						WaitForSingleObject(thread, INFINITE);
						CloseHandle(thread);
#else
						pthread_join(thread, NULL);
#endif
						check(accepted == (kind == 0 || kind == 2), "modal acceptance and cancellation");
						check(strcmp(text, kind == 0 ? "shared" : "unchanged") == 0 && color == 0x998877, "modal output ownership and cancellation preservation");

						if(strcmp(text, kind == 0 ? "shared" : "unchanged") != 0) {
							fprintf(stderr, "  kind=%u, text=[%s]\n", kind, text);

						}

						check(await_closed(win, action.title), "modal window destroyed after return");

					}
				}

				uint32_t timer_calls = 0;
				uint32_t timer_events = 0;
				uint32_t callback_timer = mkgui_window_add_timer(win, 10000000, timer_tick, &timer_calls);
				uint32_t event_timer = mkgui_window_add_timer(win, 15000000, NULL, NULL);
				check(callback_timer != 0, "callback timer creation");
				check(event_timer != 0, "NULL callback timer creation");
				check(mkgui_window_add_timer(win, 0, timer_tick, &timer_calls) == 0, "zero timer interval rejected");
				uint64_t timer_deadline = mkgui_now_ns() + UINT64_C(2000000000);
				struct mkgui_event timer_event;

				while((timer_calls < 3 || timer_events < 3) && mkgui_now_ns() < timer_deadline) {
					while(mkgui_window_poll(win, &timer_event)) {
						if(timer_event.type == MKGUI_EVENT_TIMER && timer_event.id == event_timer) {
							++timer_events;

							if(timer_events == 3) {
								mkgui_window_remove_timer(win, event_timer);

							}
						}
					}

					if(timer_calls < 3 || timer_events < 3) {
						mkgui_ctx_wait_until(ctx, timer_deadline);

					}
				}

				check(timer_calls == 3, "repeating callbacks wake an idle host loop");
				check(timer_events == 3, "NULL callback timer emits public events while idle");
				mkgui_window_remove_timer(win, callback_timer);
				mkgui_window_remove_timer(win, event_timer);

				struct mkgui_window *secondary = mkgui_window_create(ctx, win, widgets, sizeof(widgets) / sizeof(widgets[0]), "mkgui shared secondary", 300, 200);
				check(secondary != NULL, "secondary window creation");

				if(secondary) {
					mkgui_window_set_callback(secondary, secondary_event, NULL);
					struct native_window child = native_find("mkgui shared secondary");
					check(child.handle != 0, "find secondary native window");

					if(child.handle) {
						native_close(child);
						uint64_t deadline = mkgui_now_ns() + UINT64_C(2000000000);

						while(!close_calls && mkgui_now_ns() < deadline) {
							mkgui_ctx_pump_others(ctx);
							mkgui_ctx_wait_until(ctx, mkgui_now_ns() + UINT64_C(1000000));

						}

						check(close_calls == 1, "native close dispatches consumer callback");

					}

					mkgui_ctx_reap_closed(ctx);
					check(await_closed(win, "mkgui shared secondary"), "callback-requested destruction is reaped");

				}

				native_close(native);
				check(await_event(win, MKGUI_EVENT_CLOSE, 0), "primary native close event");
				check(mkgui_window_should_close(win), "primary close flag");

			}

			mkgui_window_destroy(win);

		}

		mkgui_ctx_destroy(ctx);

	}

#ifndef _WIN32
	XCloseDisplay(display);
#endif
	printf("Shared events: %u checks, %u failures\n", checks, failures);
	return failures ? 1 : 0;

}
