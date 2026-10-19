// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT

#ifdef MKGUI_TEST_MINIMAL
#define MKGUI_NO_INPUT
#define MKGUI_NO_CHECKBOX
#define MKGUI_NO_DROPDOWN
#define MKGUI_NO_SLIDER
#define MKGUI_NO_LISTVIEW
#define MKGUI_NO_MENU
#define MKGUI_NO_MENUITEM
#define MKGUI_NO_TABS
#define MKGUI_NO_TAB
#define MKGUI_NO_HSPLIT
#define MKGUI_NO_VSPLIT
#define MKGUI_NO_TREEVIEW
#define MKGUI_NO_STATUSBAR
#define MKGUI_NO_TOOLBAR
#define MKGUI_NO_SPINBOX
#define MKGUI_NO_RADIO
#define MKGUI_NO_PROGRESS
#define MKGUI_NO_TEXTAREA
#define MKGUI_NO_GROUP
#define MKGUI_NO_SPINNER
#define MKGUI_NO_ITEMVIEW
#define MKGUI_NO_PANEL
#define MKGUI_NO_SCROLLBAR
#define MKGUI_NO_IMAGE
#define MKGUI_NO_GLVIEW
#define MKGUI_NO_CANVAS
#define MKGUI_NO_FORM
#define MKGUI_NO_SPACER
#define MKGUI_NO_PATHBAR
#define MKGUI_NO_IPINPUT
#define MKGUI_NO_TOGGLE
#define MKGUI_NO_COMBOBOX
#define MKGUI_NO_DATEPICKER
#define MKGUI_NO_GRIDVIEW
#define MKGUI_NO_RICHLIST
#define MKGUI_NO_METER
#define MKGUI_NO_DIVIDER
#define MKGUI_NO_LOGVIEW
#define MKGUI_NO_TREELIST
#endif

#include "../mkgui.c"

// [=]===^=[ main ]================================================[=]
int main(void) {
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, 1, "Features", "", 0, 400, 300, 0, MKGUI_WINDOW_HIDDEN, 0),
		MKGUI_W(MKGUI_VBOX, 2, "", "", 1, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, 3, "Button", "", 2, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_LABEL, 4, "Label", "", 2, 0, 0, 0, 0, 0),
	};
	uint32_t failures = 0;
	uint32_t disabled = 0;
	struct mkgui_ctx *ctx = mkgui_ctx_create();

	if(ctx) {
		struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, sizeof(widgets) / sizeof(widgets[0]), NULL, 0, 0);

		if(win) {
			volatile uint32_t invoke_dialogs = 0; // NOTE(peter): Retain selected dialog entry points for configuration link checks.

			if(invoke_dialogs) {
#if MKGUI_HAS_DIALOGS
				char text[32];
				mkgui_dialog_message(win, "Features", "Message", MKGUI_DLG_ICON_NONE, MKGUI_DLG_BUTTONS_OK);
				mkgui_dialog_input(win, "Features", "Text", "", text, sizeof(text));
#endif
#if MKGUI_HAS_FILEDIALOG
				mkgui_dialog_folder(win, NULL);
#endif
#if MKGUI_HAS_COLORPICKER
				uint32_t color;
				mkgui_dialog_color(win, 0, &color);
#endif
#if MKGUI_HAS_ICONBROWSER
				char name[64];
				mkgui_icon_browser(win, 16, name, sizeof(name));
#endif
			}

			struct mkgui_widget dynamic = MKGUI_W(MKGUI_BUTTON, 100, "Dynamic", "", 2, 0, 0, 0, 0, 0);
			uint32_t added = mkgui_widget_add(win, dynamic, 0);

			if(added != 1 || win->widget_count != 5) {
				++failures;

			}

			for(uint32_t type = 0; type < MKGUI_WIDGET_COUNT; ++type) {
				if(!mkgui_widget_type_enabled(type)) {
					struct mkgui_widget invalid[] = {widgets[0], widgets[2]};
					invalid[1].type = type;
					struct mkgui_window *rejected = mkgui_window_create(ctx, NULL, invalid, 2, NULL, 0, 0);
					++disabled;

					if(rejected) {
						++failures;
						mkgui_window_destroy(rejected);

					}

					dynamic.type = type;

					if(mkgui_widget_add(win, dynamic, 0) != 0 || win->widget_count != 5 || render_desc[type].fn) {
						fprintf(stderr, "FAIL: excluded type %u remains available\n", type);
						++failures;

					}
				}
			}

			if(mkgui_widget_type_enabled(MKGUI_WIDGET_COUNT) || mkgui_widget_type_enabled(UINT32_MAX)) {
				++failures;

			}

			layout_widgets(win);
			mkgui_flush(win);
			mkgui_window_destroy(win);

		} else {
			fprintf(stderr, "FAIL: cannot create enabled widget tree\n");
			++failures;

		}

		mkgui_ctx_destroy(ctx);

	} else {
		fprintf(stderr, "FAIL: cannot create context\n");
		++failures;

	}

#if defined(MKGUI_TEST_MINIMAL) && !defined(MKGUI_HAS_ALL)
	if(disabled != MKGUI_WIDGET_COUNT - 5) {
		++failures;

	}
#elif defined(MKGUI_HAS_ALL)
	if(disabled != 0 || !MKGUI_HAS_DIALOGS || !MKGUI_HAS_FILEDIALOG || !MKGUI_HAS_COLORPICKER || !MKGUI_HAS_ICONBROWSER) {
		++failures;

	}
#endif

	fprintf(stderr, "features: %u excluded widget types, %u failures\n", disabled, failures);
	return failures ? 1 : 0;

}
