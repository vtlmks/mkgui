// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#include "mkgui.c"

#ifdef _WIN32
#include <GL/gl.h>
#else
#include <GL/gl.h>
#include <GL/glx.h>
#endif

enum {
	ID_WINDOW = 0,
	ID_MENU, ID_FILE_MENU, ID_EDIT_MENU, ID_VIEW_MENU, ID_HELP_MENU,
	ID_FILE_NEW, ID_OPEN, ID_SAVE, ID_FILE_SEP1, ID_EXIT,
	ID_EDIT_CUT, ID_COPY, ID_PASTE,
	ID_VIEW_GRID, ID_VIEW_STATUS, ID_VIEW_SEP1,
	ID_VIEW_SMALL, ID_VIEW_MEDIUM, ID_VIEW_LARGE,
	ID_HELP_ABOUT,
	ID_TOOLBAR, ID_TB_NEW, ID_TB_OPEN, ID_TB_SAVE,
	ID_TABS,
	ID_TAB1, ID_TAB3, ID_TAB4, ID_TAB5, ID_TAB7, ID_TAB6,
	ID_STATUSBAR,
	ID_TAB1_VBOX, ID_GRP_INPUT, ID_TAB1_FORM,
	ID_LABEL1, ID_INPUT1,
	ID_LBL_ENABLE, ID_CHECK1,
	ID_LBL_MODE, ID_DROPDOWN1,
	ID_LBL_VOLUME, ID_SLIDER1,
	ID_GRP_RADIO, ID_TAB1_RADIO_HBOX, ID_RADIO1, ID_RADIO2, ID_RADIO3,
	ID_TAB1_PROG_HBOX, ID_PROG_LBL, ID_PROGRESS1, ID_SPINNER1,
	ID_TAB1_COUNT_HBOX, ID_LBL_COUNT, ID_SPINBOX1,
	ID_TAB1_BTN_HBOX, ID_BUTTON1, ID_THEME_CHECK,
	ID_TAB3_SPLIT, ID_TAB3_LVBOX, ID_TREE_LBL, ID_TREEVIEW1,
	ID_TAB3_RVBOX, ID_TEXT_LBL, ID_TEXTAREA1,
	ID_TAB4_VBOX, ID_LABEL2, ID_LISTVIEW1,
	ID_TAB5_VBOX, ID_TAB5_BTN_HBOX,
	ID_IV_ICON, ID_IV_THUMB, ID_IV_COMPACT, ID_IV_DETAIL, ID_ITEMVIEW1,
	ID_HBOX1, ID_VBOX1, ID_VBOX_BTN1, ID_VBOX_BTN2, ID_VBOX_BTN3, ID_VBOX_INPUT,
	ID_FORM1, ID_FORM_LBL1, ID_FORM_INP1, ID_FORM_LBL2, ID_FORM_INP2,
	ID_FORM_LBL3, ID_FORM_DRP1, ID_FORM_LBL4, ID_FORM_CHK1,
	ID_TAB6_HBOX, ID_TAB6_LVBOX,
	ID_PANEL1, ID_PANEL_LBL, ID_PANEL_BTN,
	ID_SB_LBL, ID_SCROLLBAR1,
	ID_TAB6_RVBOX, ID_IMG_LBL, ID_IMAGE1, ID_GL_LBL, ID_GLVIEW1,
};

// [=]===^=[ demo_row_cb ]=======================================[=]
static void demo_row_cb(uint32_t row, uint32_t col, char *buf, uint32_t buf_size, void *userdata) {
	(void)userdata;
	switch(col) {
		case 0: {
			snprintf(buf, buf_size, "text-x-generic\tItem %u", row + 1);
		} break;

		case 1: {
			snprintf(buf, buf_size, "%u", (row + 1) * 1024);
		} break;

		case 2: {
			snprintf(buf, buf_size, "%lld", (long long)(1741900800 - (long long)row * 86400));
		} break;

		case 3: {
			snprintf(buf, buf_size, "%u/100", ((row * 7 + 13) % 101));
		} break;

		default: {
			buf[0] = '\0';
		} break;
	}
}

static const char *demo_file_names[] = {
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
	const char *name = item < DEMO_FILE_COUNT ? demo_file_names[item] : "file.dat";
	const char *dot = strrchr(name, '.');
	const char *icon = "file-document-outline";
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

// [=]===^=[ main ]==============================================[=]
int main(void) {
	struct mkgui_widget widgets[] = {
		{ MKGUI_WINDOW,   ID_WINDOW,    "mkgui demo",       "",  0,           0,  0, 1000, 700, 0, 0 },

		{ MKGUI_MENU,     ID_MENU,      "",                  "", ID_WINDOW,   0, 0, 0, 0, 0, 0 },
		{ MKGUI_MENUITEM, ID_FILE_MENU, "File",              "", ID_MENU,     0,  0,  0,  0, 0, 0 },
		{ MKGUI_MENUITEM, ID_EDIT_MENU, "Edit",              "", ID_MENU,     0,  0,  0,  0, 0, 0 },
		{ MKGUI_MENUITEM, ID_VIEW_MENU, "View",              "", ID_MENU,     0,  0,  0,  0, 0, 0 },
		{ MKGUI_MENUITEM, ID_HELP_MENU, "Help",              "", ID_MENU,     0,  0,  0,  0, 0, 0 },
		{ MKGUI_MENUITEM, ID_FILE_NEW,  "New",               "file-plus",    ID_FILE_MENU,0,  0,  0,  0, 0, 0 },
		{ MKGUI_MENUITEM, ID_OPEN,      "Open",              "folder-open",  ID_FILE_MENU,0,  0,  0,  0, 0, 0 },
		{ MKGUI_MENUITEM, ID_SAVE,      "Save",              "content-save", ID_FILE_MENU,0,  0,  0,  0, 0, 0 },
		{ MKGUI_MENUITEM, ID_FILE_SEP1, "",                  "",             ID_FILE_MENU,0,  0,  0,  0, MKGUI_SEPARATOR, 0 },
		{ MKGUI_MENUITEM, ID_EXIT,      "Exit",              "exit-to-app",  ID_FILE_MENU,0,  0,  0,  0, 0, 0 },
		{ MKGUI_MENUITEM, ID_EDIT_CUT,  "Cut",               "content-cut",  ID_EDIT_MENU,0,  0,  0,  0, 0, 0 },
		{ MKGUI_MENUITEM, ID_COPY,      "Copy",              "content-copy", ID_EDIT_MENU,0,  0,  0,  0, 0, 0 },
		{ MKGUI_MENUITEM, ID_PASTE,     "Paste",             "content-paste",ID_EDIT_MENU,0,  0,  0,  0, 0, 0 },
		{ MKGUI_MENUITEM, ID_VIEW_GRID, "Show Grid",         "grid",         ID_VIEW_MENU,0,  0,  0,  0, MKGUI_MENU_CHECK | MKGUI_CHECKED, 0 },
		{ MKGUI_MENUITEM, ID_VIEW_STATUS,"Show Statusbar",   "dock-bottom",  ID_VIEW_MENU,0,  0,  0,  0, MKGUI_MENU_CHECK | MKGUI_CHECKED, 0 },
		{ MKGUI_MENUITEM, ID_VIEW_SEP1, "",                  "",             ID_VIEW_MENU,0,  0,  0,  0, MKGUI_SEPARATOR, 0 },
		{ MKGUI_MENUITEM, ID_VIEW_SMALL,"Small",             "",             ID_VIEW_MENU,0,  0,  0,  0, MKGUI_MENU_RADIO, 0 },
		{ MKGUI_MENUITEM, ID_VIEW_MEDIUM,"Medium",           "",             ID_VIEW_MENU,0,  0,  0,  0, MKGUI_MENU_RADIO | MKGUI_CHECKED, 0 },
		{ MKGUI_MENUITEM, ID_VIEW_LARGE,"Large",             "",             ID_VIEW_MENU,0,  0,  0,  0, MKGUI_MENU_RADIO, 0 },
		{ MKGUI_MENUITEM, ID_HELP_ABOUT,"About",             "information",  ID_HELP_MENU,0,  0,  0,  0, 0, 0 },

		{ MKGUI_TOOLBAR,  ID_TOOLBAR,   "",                  "", ID_WINDOW,   0, 0, 0, 0, 0, 0 },
		{ MKGUI_BUTTON,   ID_TB_NEW,    "New",               "file-plus",  ID_TOOLBAR,  0,  0,  0,  0, 0, 0 },
		{ MKGUI_BUTTON,   ID_TB_OPEN,   "Open",              "folder-open", ID_TOOLBAR,  0,  0,  0,  0, MKGUI_TOOLBAR_SEP, 0 },
		{ MKGUI_BUTTON,   ID_TB_SAVE,   "Save",              "content-save", ID_TOOLBAR,  0,  0,  0,  0, 0, 0 },

		{ MKGUI_TABS,     ID_TABS,      "",                  "", ID_WINDOW,   0, 0, 0, 0, MKGUI_ANCHOR_LEFT | MKGUI_ANCHOR_TOP | MKGUI_ANCHOR_RIGHT | MKGUI_ANCHOR_BOTTOM, 0 },
		{ MKGUI_TAB,      ID_TAB1,      "General",           "cog",       ID_TABS, 0, 0, 0, 0, 0, 0 },
		{ MKGUI_TAB,      ID_TAB3,      "Tree / Text",       "folder",    ID_TABS, 0, 0, 0, 0, 0, 0 },
		{ MKGUI_TAB,      ID_TAB4,      "Listview",          "view-list", ID_TABS, 0, 0, 0, 0, 0, 0 },
		{ MKGUI_TAB,      ID_TAB5,      "Item View",         "",          ID_TABS, 0, 0, 0, 0, 0, 0 },
		{ MKGUI_TAB,      ID_TAB7,      "Layout",            "",          ID_TABS, 0, 0, 0, 0, 0, 0 },
		{ MKGUI_TAB,      ID_TAB6,      "Extras",            "",          ID_TABS, 0, 0, 0, 0, 0, 0 },

		/* Tab 1: General */
		{ MKGUI_VBOX,     ID_TAB1_VBOX, "",                  "", ID_TAB1, 0, 0, 0, 0, MKGUI_ANCHOR_LEFT | MKGUI_ANCHOR_TOP | MKGUI_ANCHOR_RIGHT | MKGUI_ANCHOR_BOTTOM, 0 },
		{ MKGUI_GROUP,    ID_GRP_INPUT, "Settings",          "", ID_TAB1_VBOX, 0, 0, 0, 150, MKGUI_FIXED, 0 },
		{ MKGUI_FORM,     ID_TAB1_FORM, "",                  "", ID_GRP_INPUT, 0, 0, 0, 0, MKGUI_ANCHOR_LEFT | MKGUI_ANCHOR_TOP | MKGUI_ANCHOR_RIGHT | MKGUI_ANCHOR_BOTTOM, 0 },
		{ MKGUI_LABEL,    ID_LABEL1,    "Name:",             "", ID_TAB1_FORM, 0, 0, 0, 0, 0, 0 },
		{ MKGUI_INPUT,    ID_INPUT1,    "",                  "", ID_TAB1_FORM, 0, 0, 0, 0, 0, 0 },
		{ MKGUI_LABEL,    ID_LBL_ENABLE,"Enable:",           "", ID_TAB1_FORM, 0, 0, 0, 0, 0, 0 },
		{ MKGUI_CHECKBOX, ID_CHECK1,    "Enable feature",    "", ID_TAB1_FORM, 0, 0, 0, 0, 0, 0 },
		{ MKGUI_LABEL,    ID_LBL_MODE,  "Mode:",             "", ID_TAB1_FORM, 0, 0, 0, 0, 0, 0 },
		{ MKGUI_DROPDOWN, ID_DROPDOWN1, "",                  "", ID_TAB1_FORM, 0, 0, 0, 0, 0, 0 },
		{ MKGUI_LABEL,    ID_LBL_VOLUME,"Volume:",           "", ID_TAB1_FORM, 0, 0, 0, 0, 0, 0 },
		{ MKGUI_SLIDER,   ID_SLIDER1,   "",                  "", ID_TAB1_FORM, 0, 0, 0, 0, 0, 0 },
		{ MKGUI_GROUP,    ID_GRP_RADIO, "Priority",          "", ID_TAB1_VBOX, 0, 0, 0, 56, MKGUI_FIXED, 0 },
		{ MKGUI_HBOX,     ID_TAB1_RADIO_HBOX, "",            "", ID_GRP_RADIO, 0, 0, 0, 0, MKGUI_ANCHOR_LEFT | MKGUI_ANCHOR_TOP | MKGUI_ANCHOR_RIGHT | MKGUI_ANCHOR_BOTTOM, 0 },
		{ MKGUI_RADIO,    ID_RADIO1,    "Low",               "", ID_TAB1_RADIO_HBOX, 0, 0, 0, 0, MKGUI_CHECKED, 1 },
		{ MKGUI_RADIO,    ID_RADIO2,    "Medium",            "", ID_TAB1_RADIO_HBOX, 0, 0, 0, 0, 0, 1 },
		{ MKGUI_RADIO,    ID_RADIO3,    "High",              "", ID_TAB1_RADIO_HBOX, 0, 0, 0, 0, 0, 1 },
		{ MKGUI_HBOX,     ID_TAB1_PROG_HBOX, "",             "", ID_TAB1_VBOX, 0, 0, 0, 28, MKGUI_FIXED, 0 },
		{ MKGUI_LABEL,    ID_PROG_LBL,  "Progress:",         "", ID_TAB1_PROG_HBOX, 0, 0, 80, 0, MKGUI_FIXED, 0 },
		{ MKGUI_PROGRESS, ID_PROGRESS1, "",                  "", ID_TAB1_PROG_HBOX, 0, 0, 0, 0, 0, 1 },
		{ MKGUI_SPINNER,  ID_SPINNER1,  "",                  "", ID_TAB1_PROG_HBOX, 0, 0, 28, 0, MKGUI_FIXED, 0 },
		{ MKGUI_HBOX,     ID_TAB1_COUNT_HBOX, "",            "", ID_TAB1_VBOX, 0, 0, 0, 24, MKGUI_FIXED, 0 },
		{ MKGUI_LABEL,    ID_LBL_COUNT, "Count:",            "", ID_TAB1_COUNT_HBOX, 0, 0, 80, 0, MKGUI_FIXED, 0 },
		{ MKGUI_SPINBOX,  ID_SPINBOX1,  "",                  "", ID_TAB1_COUNT_HBOX, 0, 0, 120, 0, MKGUI_FIXED, 0 },
		{ MKGUI_HBOX,     ID_TAB1_BTN_HBOX, "",              "", ID_TAB1_VBOX, 0, 0, 0, 28, MKGUI_FIXED, 0 },
		{ MKGUI_BUTTON,   ID_BUTTON1,   "Apply",             "", ID_TAB1_BTN_HBOX, 0, 0, 100, 0, MKGUI_FIXED, 0 },
		{ MKGUI_CHECKBOX, ID_THEME_CHECK,"Light theme",      "brightness-6", ID_TAB1_BTN_HBOX, 0, 0, 200, 0, MKGUI_FIXED, 0 },

		/* Tab 3: Tree / Text */
		{ MKGUI_VSPLIT,   ID_TAB3_SPLIT,"",                 "", ID_TAB3, 0, 0, 0, 0, MKGUI_ANCHOR_LEFT | MKGUI_ANCHOR_TOP | MKGUI_ANCHOR_RIGHT | MKGUI_ANCHOR_BOTTOM, 0 },
		{ MKGUI_VBOX,     ID_TAB3_LVBOX,"",                  "", ID_TAB3_SPLIT, 0, 0, 0, 0, MKGUI_REGION_LEFT, 0 },
		{ MKGUI_LABEL,    ID_TREE_LBL,  "Project tree:",     "", ID_TAB3_LVBOX, 0, 0, 0, 20, MKGUI_FIXED, 0 },
		{ MKGUI_TREEVIEW, ID_TREEVIEW1, "",                  "", ID_TAB3_LVBOX, 0, 0, 0, 0, 0, 1 },
		{ MKGUI_VBOX,     ID_TAB3_RVBOX,"",                  "", ID_TAB3_SPLIT, 0, 0, 0, 0, MKGUI_REGION_RIGHT, 0 },
		{ MKGUI_LABEL,    ID_TEXT_LBL,  "Notes:",            "", ID_TAB3_RVBOX, 0, 0, 0, 20, MKGUI_FIXED, 0 },
		{ MKGUI_TEXTAREA, ID_TEXTAREA1, "",                  "", ID_TAB3_RVBOX, 0, 0, 0, 0, 0, 1 },

		/* Tab 4: Listview */
		{ MKGUI_VBOX,     ID_TAB4_VBOX, "",                  "", ID_TAB4, 0, 0, 0, 0, MKGUI_ANCHOR_LEFT | MKGUI_ANCHOR_TOP | MKGUI_ANCHOR_RIGHT | MKGUI_ANCHOR_BOTTOM, 0 },
		{ MKGUI_LABEL,    ID_LABEL2,    "Data list (1M rows):", "", ID_TAB4_VBOX, 0, 0, 0, 20, MKGUI_FIXED, 0 },
		{ MKGUI_LISTVIEW, ID_LISTVIEW1, "",                  "", ID_TAB4_VBOX, 0, 0, 0, 0, 0, 1 },

		/* Tab 5: Item View */
		{ MKGUI_VBOX,     ID_TAB5_VBOX, "",                  "", ID_TAB5, 0, 0, 0, 0, MKGUI_ANCHOR_LEFT | MKGUI_ANCHOR_TOP | MKGUI_ANCHOR_RIGHT | MKGUI_ANCHOR_BOTTOM, 0 },
		{ MKGUI_HBOX,     ID_TAB5_BTN_HBOX, "",              "", ID_TAB5_VBOX, 0, 0, 0, 24, MKGUI_FIXED, 0 },
		{ MKGUI_BUTTON,   ID_IV_ICON,   "Icons",             "", ID_TAB5_BTN_HBOX, 0, 0, 0, 0, 0, 1 },
		{ MKGUI_BUTTON,   ID_IV_THUMB,  "Thumbnails",        "", ID_TAB5_BTN_HBOX, 0, 0, 0, 0, 0, 1 },
		{ MKGUI_BUTTON,   ID_IV_COMPACT,"Compact",           "", ID_TAB5_BTN_HBOX, 0, 0, 0, 0, 0, 1 },
		{ MKGUI_BUTTON,   ID_IV_DETAIL, "Detail",            "", ID_TAB5_BTN_HBOX, 0, 0, 0, 0, 0, 1 },
		{ MKGUI_ITEMVIEW, ID_ITEMVIEW1, "",                  "", ID_TAB5_VBOX, 0, 0, 0, 0, 0, 1 },

		/* Tab 7: Layout */
		{ MKGUI_HBOX,     ID_HBOX1,     "",                  "", ID_TAB7, 0, 0, 0, 0, MKGUI_ANCHOR_LEFT | MKGUI_ANCHOR_TOP | MKGUI_ANCHOR_RIGHT | MKGUI_ANCHOR_BOTTOM, 0 },
		{ MKGUI_VBOX,     ID_VBOX1,     "",                  "", ID_HBOX1, 0, 0, 200, 0, MKGUI_PANEL_BORDER | MKGUI_FIXED, 0 },
		{ MKGUI_BUTTON,   ID_VBOX_BTN1, "First",             "file-plus",    ID_VBOX1, 0, 0, 0, 28, MKGUI_FIXED, 0 },
		{ MKGUI_BUTTON,   ID_VBOX_BTN2, "Second",            "content-save", ID_VBOX1, 0, 0, 0, 28, MKGUI_FIXED, 0 },
		{ MKGUI_INPUT,    ID_VBOX_INPUT,"",                  "", ID_VBOX1, 0, 0, 0, 24, MKGUI_FIXED, 0 },
		{ MKGUI_BUTTON,   ID_VBOX_BTN3, "Flex (fills rest)", "", ID_VBOX1, 0, 0, 0, 0, 0, 1 },
		{ MKGUI_FORM,     ID_FORM1,     "",                  "", ID_HBOX1, 0, 0, 0, 0, MKGUI_PANEL_BORDER, 1 },
		{ MKGUI_LABEL,    ID_FORM_LBL1, "Name:",             "", ID_FORM1, 0, 0, 0, 0, 0, 0 },
		{ MKGUI_INPUT,    ID_FORM_INP1, "",                  "", ID_FORM1, 0, 0, 0, 0, 0, 0 },
		{ MKGUI_LABEL,    ID_FORM_LBL2, "Email address:",    "", ID_FORM1, 0, 0, 0, 0, 0, 0 },
		{ MKGUI_INPUT,    ID_FORM_INP2, "",                  "", ID_FORM1, 0, 0, 0, 0, 0, 0 },
		{ MKGUI_LABEL,    ID_FORM_LBL3, "Category:",         "", ID_FORM1, 0, 0, 0, 0, 0, 0 },
		{ MKGUI_DROPDOWN, ID_FORM_DRP1, "",                  "", ID_FORM1, 0, 0, 0, 0, 0, 0 },
		{ MKGUI_LABEL,    ID_FORM_LBL4, "Subscribe:",        "", ID_FORM1, 0, 0, 0, 0, 0, 0 },
		{ MKGUI_CHECKBOX, ID_FORM_CHK1, "Yes",               "", ID_FORM1, 0, 0, 0, 0, 0, 0 },

		/* Tab 6: Extras */
		{ MKGUI_HBOX,     ID_TAB6_HBOX, "",                  "", ID_TAB6, 0, 0, 0, 0, MKGUI_ANCHOR_LEFT | MKGUI_ANCHOR_TOP | MKGUI_ANCHOR_RIGHT | MKGUI_ANCHOR_BOTTOM, 0 },
		{ MKGUI_VBOX,     ID_TAB6_LVBOX,"",                  "", ID_TAB6_HBOX, 0, 0, 320, 0, MKGUI_FIXED, 0 },
		{ MKGUI_PANEL,    ID_PANEL1,    "",                  "", ID_TAB6_LVBOX, 0, 0, 0, 120, MKGUI_PANEL_BORDER | MKGUI_PANEL_SUNKEN | MKGUI_FIXED, 0 },
		{ MKGUI_LABEL,    ID_PANEL_LBL, "Panel container:",  "", ID_PANEL1, 8, 4, 200, 20, 0, 0 },
		{ MKGUI_BUTTON,   ID_PANEL_BTN, "Inside Panel",      "", ID_PANEL1, 8, 30, 120, 28, 0, 0 },
		{ MKGUI_LABEL,    ID_SB_LBL,    "Scrollbar:",        "", ID_TAB6_LVBOX, 0, 0, 0, 20, MKGUI_FIXED, 0 },
		{ MKGUI_SCROLLBAR,ID_SCROLLBAR1,"",                  "", ID_TAB6_LVBOX, 0, 0, MKGUI_SCROLLBAR_W, 0, 0, 1 },
		{ MKGUI_VBOX,     ID_TAB6_RVBOX,"",                  "", ID_TAB6_HBOX, 0, 0, 0, 0, 0, 1 },
		{ MKGUI_LABEL,    ID_IMG_LBL,   "Image:",            "", ID_TAB6_RVBOX, 0, 0, 0, 20, MKGUI_FIXED, 0 },
		{ MKGUI_IMAGE,    ID_IMAGE1,    "",                  "", ID_TAB6_RVBOX, 0, 0, 0, 200, MKGUI_PANEL_BORDER | MKGUI_FIXED, 0 },
		{ MKGUI_LABEL,    ID_GL_LBL,    "OpenGL:",           "", ID_TAB6_RVBOX, 0, 0, 0, 20, MKGUI_FIXED, 0 },
		{ MKGUI_GLVIEW,   ID_GLVIEW1,   "",                  "", ID_TAB6_RVBOX, 0, 0, 0, 0, MKGUI_PANEL_BORDER, 1 },

		{ MKGUI_STATUSBAR, ID_STATUSBAR, "Ready",            "", ID_WINDOW, 0, 0, 0, 0, 0, 0 },
	};

	uint32_t widget_count = sizeof(widgets) / sizeof(widgets[0]);
	struct mkgui_ctx *ctx = mkgui_create(widgets, widget_count);
	if(!ctx) {
		return 1;
	}

	const char *modes[] = { "Auto", "Manual", "Custom", "Debug" };
	mkgui_dropdown_setup(ctx, ID_DROPDOWN1, modes, 4);
	mkgui_slider_setup(ctx, ID_SLIDER1, 0, 100, 50);
	mkgui_spinbox_setup(ctx, ID_SPINBOX1, 0, 999, 42, 1);
	mkgui_progress_setup(ctx, ID_PROGRESS1, 100);
	mkgui_progress_set(ctx, ID_PROGRESS1, 65);

	int32_t sb_widths[] = { -1, 150, 100 };
	mkgui_statusbar_setup(ctx, ID_STATUSBAR, 3, sb_widths);
	mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, "Ready");
	mkgui_statusbar_set(ctx, ID_STATUSBAR, 1, "Ln 1, Col 1");
	mkgui_statusbar_set(ctx, ID_STATUSBAR, 2, "UTF-8");

	struct mkgui_column cols[] = {
		{ "Name",     200, MKGUI_CELL_ICON_TEXT },
		{ "Size",     100, MKGUI_CELL_SIZE },
		{ "Modified", 150, MKGUI_CELL_DATE },
		{ "Progress", 120, MKGUI_CELL_PROGRESS },
	};
	mkgui_listview_setup(ctx, ID_LISTVIEW1, 1000000, 4, cols, demo_row_cb, NULL);

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

	mkgui_textarea_set(ctx, ID_TEXTAREA1, "Type your notes here.\nLine 2.\nLine 3.");

	mkgui_itemview_setup(ctx, ID_ITEMVIEW1, 200, MKGUI_VIEW_ICON, demo_itemview_label, demo_itemview_icon, NULL);

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

	const char *form_cats[] = { "General", "Support", "Sales", "Billing" };
	mkgui_dropdown_setup(ctx, ID_FORM_DRP1, form_cats, 4);

	mkgui_scrollbar_setup(ctx, ID_SCROLLBAR1, 1000, 100);

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

	float gl_speed = 30.0f;

	struct mkgui_event ev;
	uint32_t running = 1;
	while(running) {
		while(mkgui_poll(ctx, &ev)) {
			switch(ev.type) {
				case MKGUI_EVENT_CLOSE: {
					running = 0;
				} break;

				case MKGUI_EVENT_CLICK: {
					if(ev.id == ID_BUTTON1) {
						mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, "Applied!");
						printf("Button clicked! Input: %s\n", mkgui_input_get(ctx, ID_INPUT1));

					} else if(ev.id == ID_TB_NEW) {
						mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, "New");

					} else if(ev.id == ID_TB_OPEN) {
						mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, "Open");

					} else if(ev.id == ID_TB_SAVE) {
						mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, "Save");

					} else if(ev.id == ID_IV_ICON) {
						mkgui_itemview_set_view(ctx, ID_ITEMVIEW1, MKGUI_VIEW_ICON);

					} else if(ev.id == ID_IV_THUMB) {
						mkgui_itemview_set_view(ctx, ID_ITEMVIEW1, MKGUI_VIEW_THUMBNAIL);

					} else if(ev.id == ID_IV_COMPACT) {
						mkgui_itemview_set_view(ctx, ID_ITEMVIEW1, MKGUI_VIEW_COMPACT);

					} else if(ev.id == ID_IV_DETAIL) {
						mkgui_itemview_set_view(ctx, ID_ITEMVIEW1, MKGUI_VIEW_DETAIL);
					}
				} break;

				case MKGUI_EVENT_MENU: {
					if(ev.id == ID_EXIT) {
						if(mkgui_confirm_dialog(ctx, "Quit", "Are you sure you want to exit?")) {
							running = 0;
						}

					} else if(ev.id == ID_FILE_NEW) {
						char name[256];
						if(mkgui_input_dialog(ctx, "New File", "File name:", "untitled.txt", name, sizeof(name))) {
							char buf[256];
							snprintf(buf, sizeof(buf), "Created: %s", name);
							mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, buf);
						}

					} else if(ev.id == ID_OPEN) {
						struct mkgui_file_dialog_opts open_opts = {0};
						uint32_t count = mkgui_open_dialog(ctx, &open_opts);
						if(count > 0) {
							char buf[256];
							snprintf(buf, sizeof(buf), "Opened %u file(s): %s", count, mkgui_dialog_path(ctx, 0));
							mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, buf);
						}

					} else if(ev.id == ID_SAVE) {
						struct mkgui_file_dialog_opts save_opts = {0};
						save_opts.default_name = "untitled.txt";
						if(mkgui_save_dialog(ctx, &save_opts)) {
							char buf[256];
							snprintf(buf, sizeof(buf), "Save to: %s", mkgui_dialog_path(ctx, 0));
							mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, buf);
						}

					} else if(ev.id == ID_HELP_ABOUT) {
						mkgui_message_box(ctx, "About", "mkgui demo v1.0");
					}
				} break;

				case MKGUI_EVENT_TAB_CHANGED: {
					printf("Tab changed to %d\n", ev.value);
				} break;

				case MKGUI_EVENT_CHECKBOX_CHANGED: {
					if(ev.id == ID_THEME_CHECK) {
						mkgui_set_theme(ctx, ev.value ? light_theme() : default_theme());
					}
					printf("Checkbox %u = %d\n", ev.id, ev.value);
				} break;

				case MKGUI_EVENT_DROPDOWN_CHANGED: {
					printf("Dropdown %u = %d\n", ev.id, ev.value);
				} break;

				case MKGUI_EVENT_SLIDER_CHANGED: {
					char buf[64];
					snprintf(buf, sizeof(buf), "Volume: %d", ev.value);
					mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, buf);
				} break;

				case MKGUI_EVENT_SPINBOX_CHANGED: {
					printf("Spinbox %u = %d\n", ev.id, ev.value);
				} break;

				case MKGUI_EVENT_RADIO_CHANGED: {
					printf("Radio %u selected\n", ev.id);
				} break;

				case MKGUI_EVENT_ITEMVIEW_SELECT: {
					char buf[64];
					snprintf(buf, sizeof(buf), "Item selected: %d", ev.value);
					mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, buf);
				} break;

				case MKGUI_EVENT_LISTVIEW_SELECT: {
					char buf[64];
					snprintf(buf, sizeof(buf), "Selected row: %d", ev.value);
					mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, buf);
				} break;

				case MKGUI_EVENT_LISTVIEW_SORT: {
					printf("Sort column %d dir %d\n", ev.col, ev.value);
				} break;

				case MKGUI_EVENT_TREEVIEW_SELECT: {
					printf("Tree node selected: %d\n", ev.value);
				} break;

				case MKGUI_EVENT_TREEVIEW_EXPAND: {
					printf("Tree node expanded: %d\n", ev.value);
				} break;

				case MKGUI_EVENT_TREEVIEW_COLLAPSE: {
					printf("Tree node collapsed: %d\n", ev.value);
				} break;

				case MKGUI_EVENT_SCROLL: {
					char buf[64];
					snprintf(buf, sizeof(buf), "Scroll %u = %d", ev.id, ev.value);
					mkgui_statusbar_set(ctx, ID_STATUSBAR, 0, buf);
				} break;

				default: {
				} break;
			}
		}

		if(gl_ctx && widget_visible(ctx, (uint32_t)find_widget_idx(ctx, ID_GLVIEW1))) {
			int32_t glw, glh;
			mkgui_glview_get_size(ctx, ID_GLVIEW1, &glw, &glh);
			if(glw > 0 && glh > 0) {
#ifdef _WIN32
				wglMakeCurrent(gl_hdc, gl_ctx);
#else
				glXMakeCurrent(mkgui_glview_get_x11_display(ctx), mkgui_glview_get_x11_window(ctx, ID_GLVIEW1), gl_ctx);
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
				glRotatef((float)ctx->anim_time * gl_speed, 0.4f, 1.0f, 0.2f);

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
				SwapBuffers(gl_hdc);
#else
				glXSwapBuffers(mkgui_glview_get_x11_display(ctx), mkgui_glview_get_x11_window(ctx, ID_GLVIEW1));
#endif
				}
		}

		mkgui_wait(ctx);
	}

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
