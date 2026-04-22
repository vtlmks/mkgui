// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mkgui.h"

#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#else
#include <GL/gl.h>
#include <GL/glx.h>
#endif

#include "mkgui_glview.h"

enum {
	ID_WINDOW = 0,

	ID_MENU, ID_FILE_MENU, ID_EDIT_MENU, ID_VIEW_MENU, ID_HELP_MENU,
	ID_FILE_NEW, ID_OPEN, ID_SAVE, ID_FILE_SEP1, ID_EXIT,
	ID_EDIT_CUT, ID_COPY, ID_PASTE,
	ID_VIEW_GRID, ID_VIEW_STATUS, ID_VIEW_SEP1,
	ID_VIEW_SMALL, ID_VIEW_MEDIUM, ID_VIEW_LARGE,
	ID_HELP_ABOUT,

	ID_TOOLBAR, ID_TB_NEW, ID_TB_SEP1, ID_TB_OPEN, ID_TB_SAVE,

	ID_TABS,
	ID_TAB_CONTROLS, ID_TAB_TREE, ID_TAB_DATA, ID_TAB_LAYOUT, ID_TAB_MEDIA,

	ID_STATUSBAR,

	/* Controls tab */
	ID_CTL_HBOX,
	ID_CTL_LVBOX,
	ID_GRP_SETTINGS, ID_CTL_FORM,
	ID_LBL_NAME, ID_INPUT1,
	ID_LBL_ENABLE, ID_CHECK1,
	ID_LBL_MODE, ID_DROPDOWN1,
	ID_LBL_SEARCH, ID_COMBOBOX1,
	ID_LBL_VOLUME, ID_SLIDER1,
	ID_GRP_RADIO, ID_RADIO_HBOX, ID_RADIO1, ID_RADIO2, ID_RADIO3,
	ID_PROG_HBOX, ID_PROG_LBL, ID_PROGRESS1, ID_SPINNER1,
	ID_METER_HBOX, ID_METER_LBL, ID_METER1,
	ID_COUNT_HBOX, ID_LBL_COUNT, ID_SPINBOX1,
	ID_BTN_HBOX, ID_BUTTON1, ID_BTN_BROWSE, ID_SPACER1, ID_THEME_CHECK,
	ID_CTL_RVBOX,
	ID_GRP_EXTRA, ID_EXTRA_FORM,
	ID_LBL_POWER, ID_TOGGLE1,
	ID_LBL_DATE, ID_DATEPICKER1,
	ID_LBL_IP, ID_IPINPUT1,
	ID_LBL_TBMODE, ID_TBMODE_DROP,
	ID_LBL_COLOR, ID_BTN_COLOR,
	ID_DIVIDER1,
	ID_GRP_COLLAPSIBLE,
	ID_COLL_LBL,
	ID_LINK1,
	ID_LBL_WRAP,
	ID_LBL_AMOUNT, ID_AMOUNT_INPUT,

	/* Tree / Text tab */
	ID_TREE_SPLIT, ID_TREE_LVBOX, ID_TREE_LBL, ID_TREEVIEW1,
	ID_TREE_RVBOX, ID_TEXT_LBL, ID_TEXTAREA1,

	/* Data Views tab */
	ID_DATA_SPLIT,
	ID_DATA_LVBOX, ID_LV_LBL, ID_LISTVIEW1,
	ID_DATA_RVBOX, ID_GV_LBL, ID_GRIDVIEW1,
	ID_RL_LBL, ID_RICHLIST1,
	ID_IV_HBOX, ID_IV_ICON, ID_IV_THUMB, ID_IV_COMPACT, ID_IV_DETAIL, ID_ITEMVIEW1,

	/* Layout tab */
	ID_LAY_HBOX,
	ID_LAY_VBOX, ID_VBOX_BTN1, ID_VBOX_BTN2, ID_VBOX_INPUT, ID_VBOX_SPACER, ID_VBOX_BTN3,
	ID_LAY_FORM, ID_FORM_LBL1, ID_FORM_INP1, ID_FORM_LBL2, ID_FORM_INP2,
	ID_FORM_LBL3, ID_FORM_DRP1, ID_FORM_LBL4, ID_FORM_CHK1,

	/* Context menu item IDs (not widget IDs) */
	ID_CTX_CUT = 900, ID_CTX_COPY, ID_CTX_PASTE, ID_CTX_DELETE,
	ID_CTX_SELECT_ALL, ID_CTX_PROPERTIES,
	ID_CTX_COL_NAME, ID_CTX_COL_VALUE, ID_CTX_COL_DESC,
	ID_CTX_COL_AUTOSIZE,

	/* Media tab */
	ID_MEDIA_HBOX,
	ID_MEDIA_LVBOX,
	ID_PANEL1, ID_PANEL_LBL, ID_PANEL_BTN,
	ID_PATH_LBL, ID_PATHBAR1,
	ID_SB_LBL, ID_SCROLLBAR1,
	ID_MEDIA_RVBOX,
	ID_CANVAS_LBL, ID_CANVAS1,
	ID_IMG_LBL, ID_IMAGE1,
	ID_GL_LBL, ID_GLVIEW1,
};

#define DEMO_GRID_ROWS 8
static uint32_t demo_grid_order[DEMO_GRID_ROWS];

static char *demo_richlist_titles[] = {
	"Bohemian Rhapsody", "Hotel California", "Stairway to Heaven",
	"Imagine", "Smells Like Teen Spirit", "Hey Jude",
	"Like a Rolling Stone", "Yesterday", "Purple Haze",
	"Billie Jean", "Comfortably Numb", "Sweet Child O' Mine",
};
static char *demo_richlist_artists[] = {
	"Queen", "Eagles", "Led Zeppelin",
	"John Lennon", "Nirvana", "The Beatles",
	"Bob Dylan", "The Beatles", "Jimi Hendrix",
	"Michael Jackson", "Pink Floyd", "Guns N' Roses",
};

// [=]===^=[ demo_richlist_cb ]===================================[=]
static void demo_richlist_cb(uint32_t row, struct mkgui_richlist_row *out, void *userdata) {
	(void)userdata;
	uint32_t count = sizeof(demo_richlist_titles) / sizeof(demo_richlist_titles[0]);
	uint32_t idx = row % count;
	snprintf(out->title, sizeof(out->title), "%s", demo_richlist_titles[idx]);
	snprintf(out->subtitle, sizeof(out->subtitle), "%s", demo_richlist_artists[idx]);
	snprintf(out->right_text, sizeof(out->right_text), "%u:%02u", 2 + row % 4, (row * 17) % 60);
	out->thumbnail = NULL;
}

// [=]===^=[ demo_grid_cb ]======================================[=]
static void demo_grid_cb(uint32_t row, uint32_t col, char *buf, uint32_t buf_size, void *userdata) {
	(void)userdata;
	uint32_t logical = demo_grid_order[row];
	if(col == 0) {
		snprintf(buf, buf_size, "Input %u", logical + 1);
	} else {
		snprintf(buf, buf_size, "Out %u", col);
	}
}

static int32_t demo_lv_sort_dir;
static uint32_t demo_lv_row_count = 1000000;

// [=]===^=[ demo_row_cb ]=======================================[=]
static void demo_row_cb(uint32_t row, uint32_t col, char *buf, uint32_t buf_size, void *userdata) {
	(void)userdata;
	uint32_t r = (demo_lv_sort_dir < 0) ? (demo_lv_row_count - 1 - row) : row;
	switch(col) {
		case 0: {
			snprintf(buf, buf_size, "text-x-generic\tItem %u", r + 1);
		} break;

		case 1: {
			snprintf(buf, buf_size, "%u", (r + 1) * 1024);
		} break;

		case 2: {
			snprintf(buf, buf_size, "%lld", (long long)(1741900800 - (long long)r * 86400));
		} break;

		case 3: {
			snprintf(buf, buf_size, "%u/100", ((r * 7 + 13) % 101));
		} break;

		default: {
			buf[0] = '\0';
		} break;
	}
}

static char *demo_file_names[] = {
	"README.md", "Makefile", "main.c", "util.h", "render.cpp",
	"build.sh", "style.css", "index.html", "app.js", "config.json",
	"data.xml", "report.pdf", "photo.jpg", "logo.png", "banner.gif",
	"diagram.svg", "movie.mp4", "clip.avi", "song.mp3", "alert.wav",
	"backup.zip", "source.tar.gz", "notes.txt", "slides.pptx", "budget.xlsx",
	"letter.docx", "records.csv", "schema.sql", "server.py", "main.rs",
	"hello.go", "App.java", "gem.rb", "init.lua", "test.ts",
	"math.f90", "stats.r", "page.php", "Cargo.toml", "Dockerfile",
};
#define DEMO_FILE_COUNT (sizeof(demo_file_names) / sizeof(demo_file_names[0]))

// [=]===^=[ demo_itemview_label ]=================================[=]
static void demo_itemview_label(uint32_t item, char *buf, uint32_t buf_size, void *userdata) {
	(void)userdata;
	if(item < DEMO_FILE_COUNT) {
		snprintf(buf, buf_size, "%s", demo_file_names[item]);
	} else {
		snprintf(buf, buf_size, "file_%u.dat", item);
	}
}

// [=]===^=[ demo_itemview_icon ]==================================[=]
static void demo_itemview_icon(uint32_t item, char *buf, uint32_t buf_size, void *userdata) {
	(void)userdata;
	char *name = item < DEMO_FILE_COUNT ? demo_file_names[item] : "file.dat";
	char *dot = strrchr(name, '.');
	char *icon = "file-document-outline";
	if(strcmp(name, "Makefile") == 0 || strcmp(name, "Dockerfile") == 0) {
		icon = "file-cog";
	} else if(dot) {
		if(strcmp(dot, ".c") == 0 || strcmp(dot, ".h") == 0) {
			icon = "language-c";
		} else if(strcmp(dot, ".cpp") == 0 || strcmp(dot, ".hpp") == 0) {
			icon = "language-cpp";
		} else if(strcmp(dot, ".py") == 0) {
			icon = "language-python";
		} else if(strcmp(dot, ".rs") == 0) {
			icon = "language-rust";
		} else if(strcmp(dot, ".go") == 0) {
			icon = "language-go";
		} else if(strcmp(dot, ".java") == 0) {
			icon = "language-java";
		} else if(strcmp(dot, ".js") == 0) {
			icon = "language-javascript";
		} else if(strcmp(dot, ".ts") == 0) {
			icon = "language-typescript";
		} else if(strcmp(dot, ".rb") == 0) {
			icon = "language-ruby";
		} else if(strcmp(dot, ".lua") == 0) {
			icon = "language-lua";
		} else if(strcmp(dot, ".php") == 0) {
			icon = "language-php";
		} else if(strcmp(dot, ".r") == 0) {
			icon = "language-r";
		} else if(strcmp(dot, ".f90") == 0) {
			icon = "language-fortran";
		} else if(strcmp(dot, ".swift") == 0) {
			icon = "language-swift";
		} else if(strcmp(dot, ".kt") == 0) {
			icon = "language-kotlin";
		} else if(strcmp(dot, ".cs") == 0) {
			icon = "language-csharp";
		} else if(strcmp(dot, ".css") == 0) {
			icon = "language-css3";
		} else if(strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0) {
			icon = "language-html5";
		} else if(strcmp(dot, ".md") == 0) {
			icon = "language-markdown";
		} else if(strcmp(dot, ".json") == 0 || strcmp(dot, ".toml") == 0) {
			icon = "code-json";
		} else if(strcmp(dot, ".xml") == 0) {
			icon = "xml";
		} else if(strcmp(dot, ".sh") == 0) {
			icon = "script-text";
		} else if(strcmp(dot, ".sql") == 0) {
			icon = "database";
		} else if(strcmp(dot, ".pdf") == 0) {
			icon = "file-pdf-box";
		} else if(strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0) {
			icon = "file-jpg-box";
		} else if(strcmp(dot, ".png") == 0) {
			icon = "file-png-box";
		} else if(strcmp(dot, ".gif") == 0) {
			icon = "file-gif-box";
		} else if(strcmp(dot, ".svg") == 0 || strcmp(dot, ".bmp") == 0) {
			icon = "file-image";
		} else if(strcmp(dot, ".mp4") == 0 || strcmp(dot, ".avi") == 0 || strcmp(dot, ".mkv") == 0) {
			icon = "file-video";
		} else if(strcmp(dot, ".mp3") == 0 || strcmp(dot, ".wav") == 0 || strcmp(dot, ".ogg") == 0 || strcmp(dot, ".flac") == 0) {
			icon = "file-music";
		} else if(strcmp(dot, ".zip") == 0 || strcmp(dot, ".gz") == 0 || strcmp(dot, ".7z") == 0) {
			icon = "folder-zip";
		} else if(strcmp(dot, ".xlsx") == 0 || strcmp(dot, ".xls") == 0 || strcmp(dot, ".csv") == 0) {
			icon = "file-excel";
		} else if(strcmp(dot, ".docx") == 0 || strcmp(dot, ".doc") == 0) {
			icon = "file-word";
		} else if(strcmp(dot, ".pptx") == 0 || strcmp(dot, ".ppt") == 0) {
			icon = "file-powerpoint";
		} else if(strcmp(dot, ".txt") == 0 || strcmp(dot, ".log") == 0) {
			icon = "file-document-outline";
		}
	}
	snprintf(buf, buf_size, "%s", icon);
}

// [=]===^=[ demo_canvas_cb ]=====================================[=]
static void demo_canvas_cb(struct mkgui_ctx *ctx, uint32_t id, uint32_t *pixels, int32_t x, int32_t y, int32_t w, int32_t h, void *userdata) {
	(void)id; (void)userdata;
	if(w <= 0 || h <= 0) {
		return;
	}
	int32_t win_w, win_h;
	mkgui_get_window_size(ctx, &win_w, &win_h);
	uint32_t uw = (uint32_t)w;
	uint32_t uh = (uint32_t)h;
	for(uint32_t iy = 0; iy < uh; ++iy) {
		for(uint32_t ix = 0; ix < uw; ++ix) {
			uint32_t px = (uint32_t)x + ix;
			uint32_t py = (uint32_t)y + iy;
			uint32_t r = ix * 255 / (uw > 1 ? uw - 1 : 1);
			uint32_t g = iy * 255 / (uh > 1 ? uh - 1 : 1);
			uint32_t b = (ix + iy) * 128 / (uw + uh > 2 ? uw + uh - 2 : 1);
			pixels[py * (uint32_t)win_w + px] = 0xff000000 | (r << 16) | (g << 8) | b;
		}
	}
}


struct demo_state {
#ifdef _WIN32
	HGLRC gl_ctx;
	HDC gl_hdc;
#else
	GLXContext gl_ctx;
#endif
};

// [=]===^=[ demo_handle_file_action ]=============================[=]
static void demo_handle_file_action(struct mkgui_ctx *ctx, uint32_t id) {
	if(id == ID_EXIT) {
		if(mkgui_confirm_dialog(ctx, "Quit", "Are you sure you want to exit?")) {
			mkgui_quit(ctx);
		}

	} else if(id == ID_FILE_NEW || id == ID_TB_NEW) {
		char name[256];
		if(mkgui_input_dialog(ctx, "New File", "File name:", "untitled.txt", name, sizeof(name))) {
			char buf[512];
			snprintf(buf, sizeof(buf), "Created: %s", name);
			mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, buf);
		}

	} else if(id == ID_OPEN || id == ID_TB_OPEN) {
		struct mkgui_file_dialog_opts open_opts = {0};
		open_opts.multi_select = 1;
		uint32_t count = mkgui_open_dialog(ctx, &open_opts);
		if(count > 0) {
			char buf[MKGUI_PATH_MAX + 320];
			if(count == 1) {
				snprintf(buf, sizeof(buf), "Opened: %s", mkgui_dialog_path(ctx, 0));
			} else {
				snprintf(buf, sizeof(buf), "Opened %u files (first: %s)", count, mkgui_dialog_path(ctx, 0));
			}
			mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, buf);
		}

	} else if(id == ID_SAVE || id == ID_TB_SAVE) {
		struct mkgui_file_dialog_opts save_opts = {0};
		save_opts.default_name = "untitled.txt";
		if(mkgui_save_dialog(ctx, &save_opts)) {
			char buf[MKGUI_PATH_MAX + 320];
			snprintf(buf, sizeof(buf), "Save to: %s", mkgui_dialog_path(ctx, 0));
			mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, buf);
		}

	} else if(id == ID_HELP_ABOUT) {
		mkgui_message_box(ctx, "About", "mkgui demo v1.0\nDemonstrates all widget types.", MKGUI_DLG_ICON_INFO, MKGUI_DLG_BUTTONS_OK);
	}
}

// [=]===^=[ demo_gl_render ]======================================[=]
static void demo_gl_render(struct mkgui_ctx *ctx, struct demo_state *state) {
	if(!state->gl_ctx || !mkgui_is_shown(ctx, ID_GLVIEW1)) {
		return;
	}
	int32_t glw = 0, glh = 0;
	mkgui_glview_get_size(ctx, ID_GLVIEW1, &glw, &glh);
	if(glw <= 0 || glh <= 0) {
		return;
	}
#ifdef _WIN32
	wglMakeCurrent(state->gl_hdc, state->gl_ctx);
#else
	glXMakeCurrent(mkgui_glview_get_x11_display(ctx), mkgui_glview_get_x11_window(ctx, ID_GLVIEW1), state->gl_ctx);
#endif
	glViewport(0, 0, glw, glh);
	glClearColor(0.1f, 0.12f, 0.15f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	float aspect = (float)glw / (float)glh;
	glFrustum(-aspect * 0.5, aspect * 0.5, -0.5, 0.5, 1.0, 100.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0.0f, 0.0f, -3.0f);
	glRotatef((float)mkgui_get_anim_time(ctx) * 30.0f, 0.4f, 1.0f, 0.2f);

	glEnable(GL_DEPTH_TEST);
	glBegin(GL_QUADS);
		glColor3f(0.2f, 0.6f, 0.9f); glVertex3f(-0.5f, -0.5f,  0.5f); glVertex3f( 0.5f, -0.5f,  0.5f); glVertex3f( 0.5f,  0.5f,  0.5f); glVertex3f(-0.5f,  0.5f,  0.5f);
		glColor3f(0.9f, 0.3f, 0.2f); glVertex3f(-0.5f, -0.5f, -0.5f); glVertex3f(-0.5f,  0.5f, -0.5f); glVertex3f( 0.5f,  0.5f, -0.5f); glVertex3f( 0.5f, -0.5f, -0.5f);
		glColor3f(0.2f, 0.8f, 0.3f); glVertex3f(-0.5f,  0.5f, -0.5f); glVertex3f(-0.5f,  0.5f,  0.5f); glVertex3f( 0.5f,  0.5f,  0.5f); glVertex3f( 0.5f,  0.5f, -0.5f);
		glColor3f(0.9f, 0.7f, 0.1f); glVertex3f(-0.5f, -0.5f, -0.5f); glVertex3f( 0.5f, -0.5f, -0.5f); glVertex3f( 0.5f, -0.5f,  0.5f); glVertex3f(-0.5f, -0.5f,  0.5f);
		glColor3f(0.7f, 0.2f, 0.8f); glVertex3f( 0.5f, -0.5f, -0.5f); glVertex3f( 0.5f,  0.5f, -0.5f); glVertex3f( 0.5f,  0.5f,  0.5f); glVertex3f( 0.5f, -0.5f,  0.5f);
		glColor3f(0.1f, 0.5f, 0.7f); glVertex3f(-0.5f, -0.5f, -0.5f); glVertex3f(-0.5f, -0.5f,  0.5f); glVertex3f(-0.5f,  0.5f,  0.5f); glVertex3f(-0.5f,  0.5f, -0.5f);
	glEnd();

#ifdef _WIN32
	SwapBuffers(state->gl_hdc);
#else
	glXSwapBuffers(mkgui_glview_get_x11_display(ctx), mkgui_glview_get_x11_window(ctx, ID_GLVIEW1));
#endif
}

// [=]===^=[ demo_gl_timer ]=======================================[=]
static void demo_gl_timer(struct mkgui_ctx *ctx, uint32_t timer_id, void *userdata) {
	(void)timer_id;
	demo_gl_render(ctx, (struct demo_state *)userdata);
}

// [=]===^=[ demo_event ]==========================================[=]
static void demo_event(struct mkgui_ctx *ctx, struct mkgui_event *ev, void *userdata) {
	(void)userdata;
	char buf[128];
	switch(ev->type) {
		case MKGUI_EVENT_CLOSE: {
			mkgui_quit(ctx);
		} break;

		case MKGUI_EVENT_CLICK: {
			if(ev->id == ID_BTN_COLOR) {
				uint32_t color = 0;
				if(mkgui_color_dialog(ctx, 0x3366cc, &color)) {
					char buf[64];
					snprintf(buf, sizeof(buf), "Color: #%06x", color);
					mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, buf);
				}

			} else if(ev->id == ID_BUTTON1) {
				mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, "Applied!");

			} else if(ev->id == ID_BTN_BROWSE) {
				char icon_name[64];
				char icon_path[1024];
				if(mkgui_icon_browser(ctx, 16, icon_name, sizeof(icon_name), icon_path, sizeof(icon_path))) {
					mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, icon_name);
				}

			} else if(ev->id == ID_TB_NEW || ev->id == ID_TB_OPEN || ev->id == ID_TB_SAVE) {
				demo_handle_file_action(ctx, ev->id);

			} else if(ev->id == ID_IV_ICON) {
				mkgui_itemview_set_view(ctx, ID_ITEMVIEW1, MKGUI_VIEW_ICON);

			} else if(ev->id == ID_IV_THUMB) {
				mkgui_itemview_set_view(ctx, ID_ITEMVIEW1, MKGUI_VIEW_THUMBNAIL);

			} else if(ev->id == ID_IV_COMPACT) {
				mkgui_itemview_set_view(ctx, ID_ITEMVIEW1, MKGUI_VIEW_COMPACT);

			} else if(ev->id == ID_IV_DETAIL) {
				mkgui_itemview_set_view(ctx, ID_ITEMVIEW1, MKGUI_VIEW_DETAIL);
			}
		} break;

		case MKGUI_EVENT_MENU: {
			demo_handle_file_action(ctx, ev->id);
		} break;

		case MKGUI_EVENT_CHECKBOX_CHANGED: {
			if(ev->id == ID_THEME_CHECK) {
				mkgui_set_theme(ctx, ev->value ? light_theme() : default_theme());
			}
		} break;

		case MKGUI_EVENT_SLIDER_CHANGED: {
			snprintf(buf, sizeof(buf), "Volume: %d", ev->value);
			mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, buf);
		} break;

		case MKGUI_EVENT_TOGGLE_CHANGED: {
			snprintf(buf, sizeof(buf), "Power: %s", ev->value ? "ON" : "OFF");
			mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, buf);
			uint32_t vis = ev->value ? 1 : 0;
			mkgui_set_visible(ctx, ID_DATEPICKER1, vis);
			mkgui_set_visible(ctx, ID_IPINPUT1, vis);
		} break;

		case MKGUI_EVENT_DATEPICKER_CHANGED: {
			snprintf(buf, sizeof(buf), "Date changed: %d", ev->value);
			mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, buf);
		} break;

		case MKGUI_EVENT_IPINPUT_CHANGED: {
			mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, "IP address changed");
		} break;

		case MKGUI_EVENT_COMBOBOX_CHANGED: {
			snprintf(buf, sizeof(buf), "Combobox selection: %d", ev->value);
			mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, buf);
		} break;

		case MKGUI_EVENT_DROPDOWN_CHANGED: {
			if(ev->id == ID_TBMODE_DROP) {
				uint32_t tb_mode_flags[] = { MKGUI_TOOLBAR_ICONS_TEXT, MKGUI_TOOLBAR_ICONS_ONLY, MKGUI_TOOLBAR_TEXT_ONLY };
				if(ev->value >= 0 && ev->value < 3) {
					mkgui_toolbar_set_mode(ctx, ID_TOOLBAR, tb_mode_flags[ev->value]);
				}
			}
		} break;

		case MKGUI_EVENT_ITEMVIEW_SELECT: {
			snprintf(buf, sizeof(buf), "Item selected: %d", ev->value);
			mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, buf);
		} break;

		case MKGUI_EVENT_LISTVIEW_SELECT: {
			snprintf(buf, sizeof(buf), "Selected row: %d", ev->value);
			mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, buf);
		} break;

		case MKGUI_EVENT_LISTVIEW_SORT: {
			if(ev->id == ID_LISTVIEW1) {
				demo_lv_sort_dir = ev->value;
				snprintf(buf, sizeof(buf), "Sort column %d %s", ev->col, ev->value > 0 ? "ascending" : "descending");
				mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, buf);
			}
		} break;

		case MKGUI_EVENT_SCROLL: {
			snprintf(buf, sizeof(buf), "Scroll: %d", ev->value);
			mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, buf);
		} break;

		case MKGUI_EVENT_PATHBAR_NAV: {
			snprintf(buf, sizeof(buf), "Pathbar navigate: segment %d", ev->value);
			mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, buf);
		} break;

		case MKGUI_EVENT_CONTEXT: {
			if(ev->id == ID_LISTVIEW1 || ev->id == ID_GRIDVIEW1 || ev->id == ID_ITEMVIEW1 || ev->id == ID_RICHLIST1) {
				mkgui_context_menu_clear(ctx);
				mkgui_context_menu_add(ctx, ID_CTX_CUT, "Cut", "content-cut", 0);
				mkgui_context_menu_add(ctx, ID_CTX_COPY, "Copy", "content-copy", 0);
				mkgui_context_menu_add(ctx, ID_CTX_PASTE, "Paste", "content-paste", 0);
				mkgui_context_menu_add_separator(ctx);
				mkgui_context_menu_add(ctx, ID_CTX_DELETE, "Delete", "delete", (ev->value < 0) ? MKGUI_DISABLED : 0);
				mkgui_context_menu_add(ctx, ID_CTX_SELECT_ALL, "Select All", "select-all", 0);
				mkgui_context_menu_add_separator(ctx);
				mkgui_context_menu_add(ctx, ID_CTX_PROPERTIES, "Properties", "information-outline", (ev->value < 0) ? MKGUI_DISABLED : 0);
				mkgui_context_menu_show(ctx);

			} else if(ev->id == ID_TREEVIEW1) {
				mkgui_context_menu_clear(ctx);
				mkgui_context_menu_add(ctx, ID_CTX_PROPERTIES, "Properties", "information-outline", (ev->value < 0) ? MKGUI_DISABLED : 0);
				mkgui_context_menu_show(ctx);
			}
		} break;

		case MKGUI_EVENT_CONTEXT_HEADER: {
			if(ev->id == ID_LISTVIEW1) {
				uint32_t cols = mkgui_listview_get_col_count(ctx, ID_LISTVIEW1);
				if(cols > 0) {
					mkgui_context_menu_clear(ctx);
					for(uint32_t c = 0; c < cols; ++c) {
						mkgui_context_menu_add(ctx, ID_CTX_COL_NAME + c, mkgui_listview_get_col_label(ctx, ID_LISTVIEW1, c), NULL, MKGUI_MENUITEM_CHECK | MKGUI_MENUITEM_CHECKED);
					}
					mkgui_context_menu_add_separator(ctx);
					mkgui_context_menu_add(ctx, ID_CTX_COL_AUTOSIZE, "Auto-size Column", "arrow-expand-horizontal", 0);
					mkgui_context_menu_show(ctx);
				}

			} else if(ev->id == ID_GRIDVIEW1) {
				uint32_t cols = mkgui_gridview_get_col_count(ctx, ID_GRIDVIEW1);
				if(cols > 0) {
					mkgui_context_menu_clear(ctx);
					for(uint32_t c = 0; c < cols; ++c) {
						mkgui_context_menu_add(ctx, ID_CTX_COL_NAME + c, mkgui_gridview_get_col_label(ctx, ID_GRIDVIEW1, c), NULL, MKGUI_MENUITEM_CHECK | MKGUI_MENUITEM_CHECKED);
					}
					mkgui_context_menu_add_separator(ctx);
					mkgui_context_menu_add(ctx, ID_CTX_COL_AUTOSIZE, "Auto-size Column", "arrow-expand-horizontal", 0);
					mkgui_context_menu_show(ctx);
				}
			}
		} break;

		case MKGUI_EVENT_CONTEXT_MENU: {
			snprintf(buf, sizeof(buf), "Context menu item: %u", ev->id);
			mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, buf);
		} break;

		case MKGUI_EVENT_GRIDVIEW_REORDER: {
			if(ev->id == ID_GRIDVIEW1) {
				int32_t src = ev->value;
				int32_t tgt = ev->col;
				if(src >= 0 && src < DEMO_GRID_ROWS && tgt >= 0 && tgt < DEMO_GRID_ROWS) {
					uint32_t tmp = demo_grid_order[src];
					if(src < tgt) {
						for(int32_t i = src; i < tgt; ++i) {
							demo_grid_order[i] = demo_grid_order[i + 1];
						}
					} else {
						for(int32_t i = src; i > tgt; --i) {
							demo_grid_order[i] = demo_grid_order[i - 1];
						}
					}
					demo_grid_order[tgt] = tmp;
				}
			}
		} break;

		default: {
		} break;
	}
}

// [=]===^=[ main ]==============================================[=]
int main(void) {
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW,   ID_WINDOW,    "mkgui demo",       "",  0,           1000, 700, 0, 0, 0),

		/* Menu bar */
		MKGUI_W(MKGUI_MENU,     ID_MENU,      "",                  "", ID_WINDOW,   0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_MENUITEM, ID_FILE_MENU, "File",              "", ID_MENU,     0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_MENUITEM, ID_EDIT_MENU, "Edit",              "", ID_MENU,     0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_MENUITEM, ID_VIEW_MENU, "View",              "", ID_MENU,     0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_MENUITEM, ID_HELP_MENU, "Help",              "", ID_MENU,     0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_MENUITEM, ID_FILE_NEW,  "New",               "document-new",       ID_FILE_MENU, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_MENUITEM, ID_OPEN,      "Open",              "document-open",      ID_FILE_MENU, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_MENUITEM, ID_SAVE,      "Save",              "document-save",      ID_FILE_MENU, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_MENUITEM, ID_FILE_SEP1, "",                  "",                   ID_FILE_MENU, 0, 0, 0, MKGUI_MENUITEM_SEPARATOR, 0),
		MKGUI_W(MKGUI_MENUITEM, ID_EXIT,      "Exit",              "application-exit",   ID_FILE_MENU, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_MENUITEM, ID_EDIT_CUT,  "Cut",               "edit-cut",           ID_EDIT_MENU, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_MENUITEM, ID_COPY,      "Copy",              "edit-copy",          ID_EDIT_MENU, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_MENUITEM, ID_PASTE,     "Paste",             "edit-paste",         ID_EDIT_MENU, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_MENUITEM, ID_VIEW_GRID, "Show Grid",         "view-grid",          ID_VIEW_MENU, 0, 0, 0, MKGUI_MENUITEM_CHECK | MKGUI_MENUITEM_CHECKED, 0),
		MKGUI_W(MKGUI_MENUITEM, ID_VIEW_STATUS,"Show Statusbar",   "view-fullscreen",    ID_VIEW_MENU, 0, 0, 0, MKGUI_MENUITEM_CHECK | MKGUI_MENUITEM_CHECKED, 0),
		MKGUI_W(MKGUI_MENUITEM, ID_VIEW_SEP1, "",                  "",                   ID_VIEW_MENU, 0, 0, 0, MKGUI_MENUITEM_SEPARATOR, 0),
		MKGUI_W(MKGUI_MENUITEM, ID_VIEW_SMALL,"Small",             "",                   ID_VIEW_MENU, 0, 0, 0, MKGUI_MENUITEM_RADIO, 0),
		MKGUI_W(MKGUI_MENUITEM, ID_VIEW_MEDIUM,"Medium",           "",                   ID_VIEW_MENU, 0, 0, 0, MKGUI_MENUITEM_RADIO | MKGUI_MENUITEM_CHECKED, 0),
		MKGUI_W(MKGUI_MENUITEM, ID_VIEW_LARGE,"Large",             "",                   ID_VIEW_MENU, 0, 0, 0, MKGUI_MENUITEM_RADIO, 0),
		MKGUI_W(MKGUI_MENUITEM, ID_HELP_ABOUT,"About",             "help-about",         ID_HELP_MENU, 0, 0, 0, 0, 0),

		/* Toolbar */
		MKGUI_W(MKGUI_TOOLBAR,  ID_TOOLBAR,   "",                  "", ID_WINDOW,   0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   ID_TB_NEW,    "New",               "edit-copy",     ID_TOOLBAR, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   ID_TB_SEP1,   "",                  "",              ID_TOOLBAR, 0, 0, 0, MKGUI_BUTTON_SEPARATOR, 0),
		MKGUI_W(MKGUI_BUTTON,   ID_TB_OPEN,   "Open",              "folder-open",   ID_TOOLBAR, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   ID_TB_SAVE,   "Save",              "document-save", ID_TOOLBAR, 0, 0, 0, 0, 0),

		/* Tabs */
		MKGUI_W(MKGUI_TABS,     ID_TABS,      "",                  "", ID_WINDOW,   0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_TAB,      ID_TAB_CONTROLS, "Controls",       "preferences-system", ID_TABS, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_TAB,      ID_TAB_TREE,     "Tree / Text",    "folder",            ID_TABS, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_TAB,      ID_TAB_DATA,     "Data Views",     "view-list",         ID_TABS, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_TAB,      ID_TAB_LAYOUT,   "Layout",         "view-grid",         ID_TABS, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_TAB,      ID_TAB_MEDIA,    "Media",          "image-x-generic",   ID_TABS, 0, 0, 0, 0, 0),

		/* ---- Controls tab ---- */
		MKGUI_W(MKGUI_HBOX,     ID_CTL_HBOX,  "",                  "", ID_TAB_CONTROLS, 0, 0, 0, 0, 0),

		/* Controls: left column */
		MKGUI_W(MKGUI_VBOX,     ID_CTL_LVBOX, "",                  "", ID_CTL_HBOX, 0, 0, 0, 0, 1),
		MKGUI_W(MKGUI_GROUP,    ID_GRP_SETTINGS, "Settings",       "", ID_CTL_LVBOX, 0, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_FORM,     ID_CTL_FORM,  "",                  "", ID_GRP_SETTINGS, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_LABEL,    ID_LBL_NAME,  "Name:",             "", ID_CTL_FORM, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_INPUT,    ID_INPUT1,    "",                  "", ID_CTL_FORM, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_LABEL,    ID_LBL_ENABLE,"Enable:",           "", ID_CTL_FORM, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_CHECKBOX, ID_CHECK1,    "Enable feature",    "", ID_CTL_FORM, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_LABEL,    ID_LBL_MODE,  "Mode:",             "", ID_CTL_FORM, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_DROPDOWN, ID_DROPDOWN1, "",                  "", ID_CTL_FORM, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_LABEL,    ID_LBL_SEARCH,"Search:",           "", ID_CTL_FORM, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_COMBOBOX, ID_COMBOBOX1, "",                  "", ID_CTL_FORM, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_LABEL,    ID_LBL_VOLUME,"Volume:",           "", ID_CTL_FORM, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_SLIDER,   ID_SLIDER1,   "",                  "", ID_CTL_FORM, 0, 0, 0, 0, 0),

		MKGUI_W(MKGUI_GROUP,    ID_GRP_RADIO, "Priority",          "", ID_CTL_LVBOX, 0, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_HBOX,     ID_RADIO_HBOX,"",                  "", ID_GRP_RADIO, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_RADIO,    ID_RADIO1,    "Low",               "", ID_RADIO_HBOX, 0, 0, 0, MKGUI_RADIO_CHECKED, 1),
		MKGUI_W(MKGUI_RADIO,    ID_RADIO2,    "Medium",            "", ID_RADIO_HBOX, 0, 0, 0, 0, 1),
		MKGUI_W(MKGUI_RADIO,    ID_RADIO3,    "High",              "", ID_RADIO_HBOX, 0, 0, 0, 0, 1),

		MKGUI_W(MKGUI_HBOX,     ID_PROG_HBOX, "",                  "", ID_CTL_LVBOX, 0, 28, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_LABEL,    ID_PROG_LBL,  "Progress:",         "", ID_PROG_HBOX, 80, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_PROGRESS, ID_PROGRESS1, "",                  "", ID_PROG_HBOX, 0, 0, 0, MKGUI_PROGRESS_SHIMMER, 1),
		MKGUI_W(MKGUI_SPINNER,  ID_SPINNER1,  "",                  "", ID_PROG_HBOX, 28, 0, MKGUI_FIXED, 0, 0),

		MKGUI_W(MKGUI_HBOX,     ID_METER_HBOX,"",                  "", ID_CTL_LVBOX, 0, 28, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_LABEL,    ID_METER_LBL, "Meter:",            "", ID_METER_HBOX, 80, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_METER,    ID_METER1,    "",                  "", ID_METER_HBOX, 0, 0, 0, MKGUI_METER_TEXT, 1),

		MKGUI_W(MKGUI_HBOX,     ID_COUNT_HBOX,"",                  "", ID_CTL_LVBOX, 0, 24, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_LABEL,    ID_LBL_COUNT, "Count:",            "", ID_COUNT_HBOX, 80, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_SPINBOX,  ID_SPINBOX1,  "",                  "", ID_COUNT_HBOX, 120, 0, MKGUI_FIXED, 0, 0),

		MKGUI_W(MKGUI_HBOX,     ID_BTN_HBOX,  "",                  "", ID_CTL_LVBOX, 0, 28, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   ID_BUTTON1,   "Apply",             "document-save", ID_BTN_HBOX, 100, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   ID_BTN_BROWSE, "Browse Icons",     "edit-find", ID_BTN_HBOX, 120, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_SPACER,   ID_SPACER1,   "",                  "", ID_BTN_HBOX, 0, 0, 0, 0, 1),
		MKGUI_W(MKGUI_CHECKBOX, ID_THEME_CHECK,"Light theme",      "weather-clear", ID_BTN_HBOX, 140, 0, MKGUI_FIXED, 0, 0),

		/* Controls: right column */
		MKGUI_W(MKGUI_VBOX,     ID_CTL_RVBOX, "",                  "", ID_CTL_HBOX, 280, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_GROUP,    ID_GRP_EXTRA, "Extra Controls",    "", ID_CTL_RVBOX, 0, 0, 0, MKGUI_GROUP_COLLAPSIBLE, 1),
		MKGUI_W(MKGUI_FORM,     ID_EXTRA_FORM,"",                  "", ID_GRP_EXTRA, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_LABEL,    ID_LBL_POWER, "Power:",            "", ID_EXTRA_FORM, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_TOGGLE,   ID_TOGGLE1,   "",                  "", ID_EXTRA_FORM, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_LABEL,    ID_LBL_DATE,  "Date:",             "", ID_EXTRA_FORM, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_DATEPICKER, ID_DATEPICKER1, "",              "", ID_EXTRA_FORM, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_LABEL,    ID_LBL_IP,    "IP Address:",       "", ID_EXTRA_FORM, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_IPINPUT,  ID_IPINPUT1,  "",                  "", ID_EXTRA_FORM, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_LABEL,    ID_LBL_TBMODE,"Toolbar:",          "", ID_EXTRA_FORM, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_DROPDOWN, ID_TBMODE_DROP,"",                 "", ID_EXTRA_FORM, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_LABEL,    ID_LBL_AMOUNT,"Amount:",           "", ID_EXTRA_FORM, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_INPUT,    ID_AMOUNT_INPUT,"",                "", ID_EXTRA_FORM, 0, 0, 0, MKGUI_INPUT_NUMERIC, 0),
		MKGUI_W(MKGUI_LABEL,    ID_LBL_COLOR, "Color:",            "", ID_EXTRA_FORM, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   ID_BTN_COLOR, "Pick Color",  "color-management", ID_EXTRA_FORM, 0, 0, 0, 0, 0),

		MKGUI_W(MKGUI_DIVIDER,  ID_DIVIDER1,  "",                  "", ID_CTL_RVBOX, 0, 0, MKGUI_FIXED, 0, 0),

		MKGUI_W(MKGUI_GROUP,    ID_GRP_COLLAPSIBLE, "Collapsible", "", ID_CTL_RVBOX, 0, 0, 0, MKGUI_GROUP_COLLAPSIBLE, 1),
		MKGUI_W(MKGUI_VBOX,     ID_COLL_LBL,  "",                  "", ID_GRP_COLLAPSIBLE, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_LABEL,    ID_LINK1,     "mkgui on GitHub",   "", ID_COLL_LBL, 0, 0, MKGUI_FIXED, MKGUI_LABEL_LINK, 0),
		MKGUI_W(MKGUI_LABEL,    ID_LBL_WRAP,  "This label demonstrates word wrapping. Text flows to the next line when it reaches the edge of the widget.", "", ID_COLL_LBL, 0, 80, MKGUI_FIXED, MKGUI_LABEL_WRAP, 0),

		/* ---- Tree / Text tab ---- */
		MKGUI_W(MKGUI_VSPLIT,   ID_TREE_SPLIT,"",                  "", ID_TAB_TREE, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     ID_TREE_LVBOX,"",                  "", ID_TREE_SPLIT, 0, 0, MKGUI_REGION_LEFT, 0, 0),
		MKGUI_W(MKGUI_LABEL,    ID_TREE_LBL,  "Project tree:",     "", ID_TREE_LVBOX, 0, 20, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_TREEVIEW, ID_TREEVIEW1, "",                  "", ID_TREE_LVBOX, 0, 0, 0, 0, 1),
		MKGUI_W(MKGUI_VBOX,     ID_TREE_RVBOX,"",                  "", ID_TREE_SPLIT, 0, 0, MKGUI_REGION_RIGHT, 0, 0),
		MKGUI_W(MKGUI_LABEL,    ID_TEXT_LBL,  "Notes:",            "", ID_TREE_RVBOX, 0, 20, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_TEXTAREA, ID_TEXTAREA1, "",                  "", ID_TREE_RVBOX, 0, 0, 0, 0, 1),

		/* ---- Data Views tab ---- */
		MKGUI_W(MKGUI_VSPLIT,   ID_DATA_SPLIT,"",                  "", ID_TAB_DATA, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     ID_DATA_LVBOX,"",                  "", ID_DATA_SPLIT, 0, 0, MKGUI_REGION_LEFT, 0, 0),
		MKGUI_W(MKGUI_LABEL,    ID_LV_LBL,   "Listview (1M rows):", "", ID_DATA_LVBOX, 0, 20, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_LISTVIEW, ID_LISTVIEW1, "",                  "", ID_DATA_LVBOX, 0, 0, 0, 0, 1),
		MKGUI_W(MKGUI_VBOX,     ID_DATA_RVBOX,"",                  "", ID_DATA_SPLIT, 0, 0, MKGUI_REGION_RIGHT, 0, 0),
		MKGUI_W(MKGUI_LABEL,    ID_GV_LBL,   "Gridview:",         "", ID_DATA_RVBOX, 0, 20, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_GRIDVIEW, ID_GRIDVIEW1, "",                  "", ID_DATA_RVBOX, 0, 120, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_LABEL,    ID_RL_LBL,   "RichList:",         "", ID_DATA_RVBOX, 0, 20, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_RICHLIST, ID_RICHLIST1, "",                  "", ID_DATA_RVBOX, 0, 0, 0, 0, 1),
		MKGUI_W(MKGUI_HBOX,     ID_IV_HBOX,   "",                  "", ID_DATA_RVBOX, 0, 24, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   ID_IV_ICON,   "Icons",             "", ID_IV_HBOX, 0, 0, 0, 0, 1),
		MKGUI_W(MKGUI_BUTTON,   ID_IV_THUMB,  "Thumbs",            "", ID_IV_HBOX, 0, 0, 0, 0, 1),
		MKGUI_W(MKGUI_BUTTON,   ID_IV_COMPACT,"Compact",           "", ID_IV_HBOX, 0, 0, 0, 0, 1),
		MKGUI_W(MKGUI_BUTTON,   ID_IV_DETAIL, "Detail",            "", ID_IV_HBOX, 0, 0, 0, 0, 1),
		MKGUI_W(MKGUI_ITEMVIEW, ID_ITEMVIEW1, "",                  "", ID_DATA_RVBOX, 0, 0, 0, 0, 1),

		/* ---- Layout tab ---- */
		MKGUI_W(MKGUI_HBOX,     ID_LAY_HBOX,  "",                  "", ID_TAB_LAYOUT, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     ID_LAY_VBOX,  "",                  "", ID_LAY_HBOX, 200, 0, MKGUI_FIXED, MKGUI_VBOX_BORDER, 0),
		MKGUI_W(MKGUI_BUTTON,   ID_VBOX_BTN1, "First",             "document-new",  ID_LAY_VBOX, 0, 28, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   ID_VBOX_BTN2, "Second",            "document-save", ID_LAY_VBOX, 0, 28, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_INPUT,    ID_VBOX_INPUT,"",                  "", ID_LAY_VBOX, 0, 24, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_SPACER,   ID_VBOX_SPACER,"",                 "", ID_LAY_VBOX, 0, 0, 0, 0, 1),
		MKGUI_W(MKGUI_BUTTON,   ID_VBOX_BTN3, "Bottom (after spacer)", "", ID_LAY_VBOX, 0, 28, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_FORM,     ID_LAY_FORM,  "",                  "", ID_LAY_HBOX, 0, 0, 0, MKGUI_FORM_BORDER, 1),
		MKGUI_W(MKGUI_LABEL,    ID_FORM_LBL1, "Name:",             "", ID_LAY_FORM, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_INPUT,    ID_FORM_INP1, "",                  "", ID_LAY_FORM, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_LABEL,    ID_FORM_LBL2, "Email address:",    "", ID_LAY_FORM, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_INPUT,    ID_FORM_INP2, "",                  "", ID_LAY_FORM, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_LABEL,    ID_FORM_LBL3, "Category:",         "", ID_LAY_FORM, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_DROPDOWN, ID_FORM_DRP1, "",                  "", ID_LAY_FORM, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_LABEL,    ID_FORM_LBL4, "Subscribe:",        "", ID_LAY_FORM, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_CHECKBOX, ID_FORM_CHK1, "Yes",               "", ID_LAY_FORM, 0, 0, 0, 0, 0),

		/* ---- Media tab ---- */
		MKGUI_W(MKGUI_HBOX,     ID_MEDIA_HBOX,"",                  "", ID_TAB_MEDIA, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,     ID_MEDIA_LVBOX,"",                 "", ID_MEDIA_HBOX, 320, 0, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_PANEL,    ID_PANEL1,    "",                  "", ID_MEDIA_LVBOX, 0, 80, MKGUI_FIXED, MKGUI_PANEL_BORDER | MKGUI_PANEL_SUNKEN, 0),
		MKGUI_W(MKGUI_LABEL,    ID_PANEL_LBL, "Panel container:",  "", ID_PANEL1, 200, 20, 0, 0, 0),
		MKGUI_W(MKGUI_BUTTON,   ID_PANEL_BTN, "Inside Panel",      "", ID_PANEL1, 120, 28, 0, 0, 0),
		MKGUI_W(MKGUI_LABEL,    ID_PATH_LBL,  "Pathbar:",          "", ID_MEDIA_LVBOX, 0, 20, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_PATHBAR,  ID_PATHBAR1,  "",                  "", ID_MEDIA_LVBOX, 0, 24, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_LABEL,    ID_SB_LBL,    "Scrollbar:",        "", ID_MEDIA_LVBOX, 0, 20, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_SCROLLBAR,ID_SCROLLBAR1,"",                  "", ID_MEDIA_LVBOX, MKGUI_SCROLLBAR_W, 0, MKGUI_VERTICAL, 0, 1),
		MKGUI_W(MKGUI_VBOX,     ID_MEDIA_RVBOX,"",                 "", ID_MEDIA_HBOX, 0, 0, 0, 0, 1),
		MKGUI_W(MKGUI_LABEL,    ID_CANVAS_LBL,"Canvas:",           "", ID_MEDIA_RVBOX, 0, 20, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_CANVAS,   ID_CANVAS1,   "",                  "", ID_MEDIA_RVBOX, 0, 120, MKGUI_FIXED, MKGUI_CANVAS_BORDER, 0),
		MKGUI_W(MKGUI_LABEL,    ID_IMG_LBL,   "Image:",            "", ID_MEDIA_RVBOX, 0, 20, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_IMAGE,    ID_IMAGE1,    "",                  "", ID_MEDIA_RVBOX, 0, 120, MKGUI_FIXED, MKGUI_IMAGE_BORDER, 0),
		MKGUI_W(MKGUI_LABEL,    ID_GL_LBL,    "OpenGL:",           "", ID_MEDIA_RVBOX, 0, 20, MKGUI_FIXED, 0, 0),
		MKGUI_W(MKGUI_GLVIEW,   ID_GLVIEW1,   "",                  "", ID_MEDIA_RVBOX, 0, 0, 0, MKGUI_GLVIEW_BORDER, 1),

		MKGUI_W(MKGUI_STATUSBAR, ID_STATUSBAR, "Ready",            "", ID_WINDOW, 0, 0, 0, 0, 0),
	};

	uint32_t widget_count = sizeof(widgets) / sizeof(widgets[0]);
	struct mkgui_ctx *ctx = mkgui_create(widgets, widget_count);
	if(!ctx) {
		return 1;
	}

	mkgui_set_app_class(ctx, "mkgui_demo");
	mkgui_icon_load_svg_dir(ctx, "icons");

	mkgui_accel_add(ctx, ID_FILE_NEW, MKGUI_MOD_CONTROL, 'n');
	mkgui_accel_add(ctx, ID_OPEN, MKGUI_MOD_CONTROL, 'o');
	mkgui_accel_add(ctx, ID_SAVE, MKGUI_MOD_CONTROL, 's');
	mkgui_accel_add(ctx, ID_EXIT, MKGUI_MOD_CONTROL, 'q');
	mkgui_accel_add(ctx, ID_EDIT_CUT, MKGUI_MOD_CONTROL, 'x');
	mkgui_accel_add(ctx, ID_COPY, MKGUI_MOD_CONTROL, 'c');
	mkgui_accel_add(ctx, ID_PASTE, MKGUI_MOD_CONTROL, 'v');

	/* Controls tab setup */
	const char *modes[] = { "Auto", "Manual", "Custom", "Debug" };
	mkgui_dropdown_setup(ctx, ID_DROPDOWN1, modes, 4);

	const char *search_items[] = { "Apple", "Banana", "Cherry", "Date", "Elderberry", "Fig", "Grape", "Honeydew" };
	mkgui_combobox_setup(ctx, ID_COMBOBOX1, search_items, 8);

	mkgui_slider_setup(ctx, ID_SLIDER1, 0, 100, 50);
	mkgui_spinbox_setup(ctx, ID_SPINBOX1, 0, 999, 42, 1);
	mkgui_progress_setup(ctx, ID_PROGRESS1, 100);
	mkgui_progress_set(ctx, ID_PROGRESS1, 65);
	mkgui_meter_setup(ctx, ID_METER1, 100);
	mkgui_meter_set(ctx, ID_METER1, 72);

	mkgui_toggle_set(ctx, ID_TOGGLE1, 1);
	int32_t today_y, today_m, today_d;
	mkgui_today(&today_y, &today_m, &today_d);
	mkgui_datepicker_set(ctx, ID_DATEPICKER1, today_y, today_m, today_d);
	mkgui_ipinput_set(ctx, ID_IPINPUT1, "192.168.1.100");

	const char *tb_modes[] = { "Icons + Text", "Icons Only", "Text Only" };
	mkgui_dropdown_setup(ctx, ID_TBMODE_DROP, tb_modes, 3);

	int32_t sb_widths[] = { -1, 150, 100 };
	mkgui_statusbar_setup(ctx, ID_STATUSBAR, 3, sb_widths);
	mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, "Ready");
	mkgui_statusbar_set(ctx, ID_STATUSBAR, 1, "Ln 1, Col 1");
	mkgui_statusbar_set(ctx, ID_STATUSBAR, 2, "UTF-8");
	/* Data Views tab setup */
	struct mkgui_column cols[] = {
		{ "Name",     200, MKGUI_CELL_ICON_TEXT },
		{ "Size",     100, MKGUI_CELL_SIZE },
		{ "Modified", 150, MKGUI_CELL_DATE },
		{ "Progress", 120, MKGUI_CELL_PROGRESS },
	};
	mkgui_listview_setup(ctx, ID_LISTVIEW1, 1000000, 4, cols, demo_row_cb, NULL);

	struct mkgui_grid_column gcols[] = {
		{ "Source", 80, MKGUI_GRID_TEXT },
		{ "Out 1", 50, MKGUI_GRID_CHECK },
		{ "Out 2", 50, MKGUI_GRID_CHECK },
		{ "Out 3", 50, MKGUI_GRID_CHECK },
		{ "Out 4", 50, MKGUI_GRID_CHECK },
	};
	for(uint32_t i = 0; i < DEMO_GRID_ROWS; ++i) {
		demo_grid_order[i] = i;
	}
	mkgui_gridview_setup(ctx, ID_GRIDVIEW1, DEMO_GRID_ROWS, 5, gcols, demo_grid_cb, NULL);
	mkgui_richlist_setup(ctx, ID_RICHLIST1, 50, 56, demo_richlist_cb, NULL);

	mkgui_itemview_setup(ctx, ID_ITEMVIEW1, 200, MKGUI_VIEW_ICON, demo_itemview_label, demo_itemview_icon, NULL);

	mkgui_split_set_ratio(ctx, ID_DATA_SPLIT, 0.50f);

	/* Tree / Text tab setup */
	mkgui_treeview_setup(ctx, ID_TREEVIEW1);
	mkgui_treeview_add(ctx, ID_TREEVIEW1, 1, 0, "src");
	mkgui_treeview_add(ctx, ID_TREEVIEW1, 2, 1, "main.c");
	mkgui_treeview_add(ctx, ID_TREEVIEW1, 3, 1, "util.c");
	mkgui_treeview_add(ctx, ID_TREEVIEW1, 4, 1, "gui");
	mkgui_treeview_add(ctx, ID_TREEVIEW1, 5, 4, "window.c");
	mkgui_treeview_add(ctx, ID_TREEVIEW1, 6, 4, "render.c");
	mkgui_treeview_add(ctx, ID_TREEVIEW1, 7, 0, "include");
	mkgui_treeview_add(ctx, ID_TREEVIEW1, 8, 7, "defs.h");
	mkgui_treeview_add(ctx, ID_TREEVIEW1, 9, 7, "types.h");
	mkgui_treeview_add(ctx, ID_TREEVIEW1, 10, 0, "Makefile");
	mkgui_treeview_add(ctx, ID_TREEVIEW1, 11, 0, "README.md");

	mkgui_set_treenode_icon(ctx, ID_TREEVIEW1, 1, "folder");
	mkgui_set_treenode_icon(ctx, ID_TREEVIEW1, 4, "folder");
	mkgui_set_treenode_icon(ctx, ID_TREEVIEW1, 7, "folder");
	mkgui_set_treenode_icon(ctx, ID_TREEVIEW1, 2, "language-c");
	mkgui_set_treenode_icon(ctx, ID_TREEVIEW1, 3, "language-c");
	mkgui_set_treenode_icon(ctx, ID_TREEVIEW1, 5, "language-c");
	mkgui_set_treenode_icon(ctx, ID_TREEVIEW1, 6, "language-c");
	mkgui_set_treenode_icon(ctx, ID_TREEVIEW1, 8, "language-c");
	mkgui_set_treenode_icon(ctx, ID_TREEVIEW1, 9, "language-c");
	mkgui_set_treenode_icon(ctx, ID_TREEVIEW1, 10, "file-cog");
	mkgui_set_treenode_icon(ctx, ID_TREEVIEW1, 11, "file-document");

	mkgui_textarea_set(ctx, ID_TEXTAREA1, "Type your notes here.\nLine 2.\nLine 3.");

	/* Layout tab setup */
	const char *form_cats[] = { "General", "Support", "Sales", "Billing" };
	mkgui_dropdown_setup(ctx, ID_FORM_DRP1, form_cats, 4);

	/* Media tab setup */
	mkgui_pathbar_set(ctx, ID_PATHBAR1, "/home/user/projects/mkgui");
	mkgui_scrollbar_setup(ctx, ID_SCROLLBAR1, 1000, 100);
	mkgui_canvas_set_callback(ctx, ID_CANVAS1, demo_canvas_cb, NULL);

	{
		uint32_t test_img[64 * 64];
		for(uint32_t iy = 0; iy < 64; ++iy) {
			for(uint32_t ix = 0; ix < 64; ++ix) {
				uint32_t checker = ((ix / 8) + (iy / 8)) & 1;
				uint32_t r = checker ? 0x60 : 0x30;
				uint32_t g = checker ? 0x90 : 0x50;
				uint32_t b = checker ? 0xd0 : 0x80;
				test_img[iy * 64 + ix] = 0xff000000 | (r << 16) | (g << 8) | b;
			}
		}
		mkgui_image_set(ctx, ID_IMAGE1, test_img, 64, 64);
	}

#ifdef _WIN32
	HGLRC gl_ctx = NULL;
	HDC gl_hdc = NULL;
	mkgui_glview_init(ctx, ID_GLVIEW1);
	{
		HWND gl_hwnd = mkgui_glview_get_hwnd(ctx, ID_GLVIEW1);
		if(gl_hwnd) {
			gl_hdc = GetDC(gl_hwnd);
			PIXELFORMATDESCRIPTOR pfd;
			memset(&pfd, 0, sizeof(pfd));
			pfd.nSize = sizeof(pfd);
			pfd.nVersion = 1;
			pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
			pfd.iPixelType = PFD_TYPE_RGBA;
			pfd.cColorBits = 24;
			pfd.cDepthBits = 16;
			int32_t fmt = ChoosePixelFormat(gl_hdc, &pfd);
			SetPixelFormat(gl_hdc, fmt, &pfd);
			gl_ctx = wglCreateContext(gl_hdc);
		}
	}
#else
	GLXContext gl_ctx = NULL;
	mkgui_glview_init(ctx, ID_GLVIEW1);
	{
		Display *gl_dpy = mkgui_glview_get_x11_display(ctx);
		Window gl_win = mkgui_glview_get_x11_window(ctx, ID_GLVIEW1);
		if(gl_win) {
			int32_t glx_attrs[] = { GLX_RGBA, GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 16, None };
			XVisualInfo *vi = glXChooseVisual(gl_dpy, DefaultScreen(gl_dpy), glx_attrs);
			if(vi) {
				gl_ctx = glXCreateContext(gl_dpy, vi, NULL, GL_TRUE);
				XFree(vi);
			}
		}
	}
#endif

	struct demo_state state = {0};
#ifdef _WIN32
	state.gl_ctx = gl_ctx;
	state.gl_hdc = gl_hdc;
#else
	state.gl_ctx = gl_ctx;
#endif

	mkgui_add_timer(ctx, 16000000, demo_gl_timer, &state);
	mkgui_run(ctx, demo_event, &state);

#ifdef _WIN32
	if(gl_ctx) {
		wglMakeCurrent(NULL, NULL);
		wglDeleteContext(gl_ctx);
	}
#else
	if(gl_ctx) {
		glXMakeCurrent(mkgui_glview_get_x11_display(ctx), None, NULL);
		glXDestroyContext(mkgui_glview_get_x11_display(ctx), gl_ctx);
	}
#endif

	mkgui_destroy(ctx);
	return 0;
}
