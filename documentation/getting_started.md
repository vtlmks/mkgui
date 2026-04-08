# Getting started with mkgui

## Prerequisites

**Linux:** GCC, FreeType2, Xlib + XShm development headers.

```sh
# Arch
pacman -S gcc freetype2 libx11 libxext

# Debian/Ubuntu
apt install gcc libfreetype-dev libx11-dev libxext-dev
```

**Windows (cross-compile from Linux):** MinGW-w64. FreeType and font rendering are handled through GDI on Windows, so no extra libraries are needed.

```sh
# Arch
pacman -S mingw-w64-gcc

# Debian/Ubuntu
apt install gcc-mingw-w64-x86-64
```

## Building mkgui

```sh
git clone <mkgui-repo-url>
cd mkgui
./build.sh
```

This produces:

| Output | Description |
|--------|-------------|
| `out/libmkgui.a` | Static library for linking |
| `out/editor` | Visual UI designer |
| `out/extract_icons` | Icon extraction tool |
| `out/demo` | Demo application |

Build variants: `./build.sh release` (stripped), `./build.sh debug` (no optimization), `./build.sh clean`.

## Option A: Static library (recommended)

Link your application against `libmkgui.a` and include `mkgui.h`.

**myapp.c:**
```c
#include "mkgui.h"

enum { ID_WINDOW = 1, ID_VBOX, ID_LABEL, ID_BUTTON };

static void on_event(struct mkgui_ctx *ctx, struct mkgui_event *ev, void *userdata) {
	if(ev->type == MKGUI_EVENT_CLICK && ev->id == ID_BUTTON) {
		mkgui_label_set(ctx, ID_LABEL, "Clicked!");
	}
}

int main(void) {
	struct mkgui_widget widgets[] = {
		MKGUI_W(MKGUI_WINDOW, ID_WINDOW, "My App",    "", 0,         400, 300, 0, 0, 0),
		MKGUI_W(MKGUI_VBOX,   ID_VBOX,   "",          "", ID_WINDOW, 0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_LABEL,  ID_LABEL,  "Hello",     "", ID_VBOX,   0, 0, 0, 0, 0),
		MKGUI_W(MKGUI_BUTTON, ID_BUTTON, "Click me",  "edit-find", ID_VBOX, 120, 0, MKGUI_FIXED, 0, 0),
	};
	struct mkgui_ctx *ctx = mkgui_create(widgets, 4);
	mkgui_set_app_class(ctx, "myapp");
	mkgui_icon_load_app_icons(ctx, "myapp");
	mkgui_run(ctx, on_event, NULL);
	mkgui_destroy(ctx);
	return 0;
}
```

**Compile:**
```sh
gcc -std=c99 -O2 myapp.c -I/path/to/mkgui -L/path/to/mkgui -lmkgui \
    $(pkg-config --cflags --libs freetype2) -lX11 -lXext -lm -o myapp
```

## Option B: Unity build

Include `mkgui.c` directly in your source file. No separate compilation or linking needed -- everything compiles as one translation unit.

**myapp.c:**
```c
#include "mkgui.c"

// ... same code as above ...
```

**Compile:**
```sh
gcc -std=c99 -O2 myapp.c $(pkg-config --cflags --libs freetype2) -lX11 -lXext -lm -o myapp
```

## Icons

mkgui uses SVG icons from Freedesktop-compatible icon themes (Papirus, Breeze, Adwaita, etc.).

### Setting up icons

1. **Unpack an icon theme** next to the editor binary. Download a Freedesktop icon theme (e.g. Papirus) and extract it so the directory sits alongside the editor (e.g. `./Papirus/`). The editor detects any directory containing an `index.theme` file. You can have multiple themes side by side.

2. **Design your UI** in the editor. When browsing icons, a dropdown lets you pick which theme to select from. You can mix icons from different themes. The editor tracks which icons are used and their source paths.

3. **Extract icons** using the generated icon list:
```sh
# Copy icons listed by the editor, resolve mkgui required icons from Papirus
# Last argument is the preferred SVG source size (16px) for theme lookup
extract_icons myapp_icons.txt icons/ ./Papirus 16
```
The tool reads `myapp_icons.txt` (editor-managed) and also `myapp_icons_extra.txt` if it exists (user-managed, for icons loaded dynamically in code). The output directory is cleaned and rebuilt from scratch each run.

Icons with source paths are copied directly. Icons without a source path (including mkgui's required internal icons) are resolved from the theme directory.

4. **Add dynamic icons** (optional). If your code loads icons at runtime that aren't in the editor's UI, list them in `myapp_icons_extra.txt`:
```
# Extra icons loaded dynamically in code
audio-volume-high
media-playback-start
media-playback-pause
```
This file is never touched by the editor, so your additions are safe across re-exports.

5. **Load icons** at startup:
```c
struct mkgui_ctx *ctx = mkgui_create(widgets, count);
mkgui_set_app_class(ctx, "myapp");
mkgui_icon_load_app_icons(ctx, "myapp");
```

On Linux, this searches the following paths in order:

1. `$MYAPP_ICON_DIR` environment variable (override for development/packaging)
2. `../share/myapp/icons/` relative to the executable (covers FHS installs at any prefix)
3. `$XDG_DATA_HOME/myapp/icons/` (default: `~/.local/share/myapp/icons/`)
4. Each `$XDG_DATA_DIRS` entry + `/myapp/icons/` (default: `/usr/share/` and `/usr/local/share/`)
5. `./icons/` (development fallback)

On Windows, it checks the executable's own directory for an `icons/` subdirectory.

6. **Install** the `icons/` directory to `<prefix>/share/<app_name>/icons/` (e.g. `/usr/share/myapp/icons/`).

### Mixing icon themes

You can pick icons from different themes (e.g. Papirus for toolbar icons, Breeze for status icons). However, all icons are stored in a flat directory keyed by name. If you select the same icon name (e.g. "document-save") from two different themes, only one version is kept. The editor warns when this happens during export.

### Icon names

Icons use standard Freedesktop names. Common examples:

| Name | Usage |
|------|-------|
| `document-save` | Save action |
| `document-open` | Open file |
| `document-new` | New document |
| `edit-copy` | Copy |
| `edit-paste` | Paste |
| `edit-undo` | Undo |
| `edit-redo` | Redo |
| `edit-find` | Search |
| `folder` | Directory |
| `folder-open` | Open directory |
| `go-previous` | Back/left |
| `go-next` | Forward/right |
| `dialog-warning` | Warning indicator |
| `dialog-error` | Error indicator |

Browse all available icons in the editor's icon browser, or see the [Freedesktop Icon Naming Specification](https://specifications.freedesktop.org/icon-naming-spec/latest/).

### Icons on Windows

Freedesktop icon themes often use symlinks for icon aliases (e.g., `media-floppy.svg` linking to `document-save.svg`). On Linux this works transparently. On Windows, symlinks may not be preserved when extracting the theme archive. Use an extraction tool that resolves symlinks to copies (e.g., 7-Zip or tar with `--dereference`), or the aliased icons will be missing.

### Theme compatibility

Icons adapt to the active theme. Monochrome icons using `currentColor` (like Papirus action icons) automatically follow `ctx->theme.text` -- they're light on dark themes, dark on light themes, and update when you call `mkgui_set_theme()`.

## DPI scaling

mkgui auto-detects display DPI on startup (via Xft.dpi on Linux, GetDpiForWindow on Windows). Override with:

```c
mkgui_set_scale(ctx, 2.0f);  // 2x scale for HiDPI displays
```

Or set the `MKGUI_SCALE` environment variable before launching: `MKGUI_SCALE=1.5`, `MKGUI_SCALE=150%`, or `MKGUI_SCALE=144dpi`. The env var takes precedence over auto-detection.

All widgets, fonts, spacing, icons, and dialog windows scale automatically. The scale factor can be changed at runtime. Range: 0.5 to 4.0.

## Widget layout

Widgets are defined as a flat array of `struct mkgui_widget`. Each widget has a `parent_id` that determines its position in the layout tree. Container widgets (VBOX, HBOX, FORM, GROUP, PANEL) arrange their children automatically.

```c
struct mkgui_widget widgets[] = {
//   MKGUI_W(type, id, label, icon, parent, w, h, flags, style, weight)
    MKGUI_W(MKGUI_WINDOW, ID_WINDOW, "Title",  "", 0,      800, 600, 0, 0, 0),
    MKGUI_W(MKGUI_VBOX,   ID_VBOX,   "",       "", ID_WINDOW, 0, 0,   0, 0, 0),
    MKGUI_W(MKGUI_LABEL,  ID_LABEL,  "Hello",  "", ID_VBOX,0,   0,   0, 0, 0),
    MKGUI_W(MKGUI_BUTTON, ID_BTN,    "OK",     "", ID_VBOX,100, 0, MKGUI_FIXED, 0, 0),
};
```

- `w=0, h=0`: widget takes its natural size from font height
- `MKGUI_FIXED`: widget uses its specified `w`/`h` instead of stretching
- `weight`: controls how flexible widgets share remaining space in a container

See `documentation/doc.md` for the full API reference.

## The editor

The visual designer lets you create widget layouts interactively:

```sh
./editor
```

- Drag widgets from the palette onto the canvas
- Set properties in the right panel
- Click the icon field to browse available icons
- Generate C code with File > Generate Code
- Test your layout with the Test button

The editor outputs a complete `.c` file with widget array, event handler stubs, and an `mkgui_run()` loop.

## Cross-platform

mkgui runs on Linux (X11) and Windows (Win32). Write once, compile for both:

```sh
# Linux
gcc -std=c99 -O2 myapp.c -lmkgui $(pkg-config --cflags --libs freetype2) -lX11 -lXext -lm

# Windows (cross-compile, no FreeType needed -- uses GDI for fonts)
x86_64-w64-mingw32-gcc -std=c99 -O2 myapp.c -lmkgui -lgdi32 -mwindows
```

The same widget definitions, event handling, and icon names work identically on both platforms.
