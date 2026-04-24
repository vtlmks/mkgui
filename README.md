# mkgui

Minimal GUI toolkit for Linux (X11) and Windows (Win32). Software rendered, single-file unity build. Breeze-inspired styling. No third-party dependencies beyond FreeType2 and Xlib/GDI.

This is a personal project by a single developer. It works well for what it does, but it has not been exhaustively tested across every platform, hardware configuration, or edge case. Use it, enjoy it, file issues if something breaks -- but don't expect the test coverage of a toolkit backed by a corporation. There are no guarantees, no roadmap, and no promises.

![Demo application](screenshots/demo.png)

## Project status

mkgui is in **beta**. The author has used it to ship several working applications, so the core widget set, layout engine, and rendering pipeline are stable in practice. The public API is not frozen -- naming, flags, and signatures may still change between tags if something turns out to be a bad idea.

## Versioning

`master` is the development branch. It can change behaviour, break API, or introduce regressions between commits without notice -- that's what it's for.

**For anything you intend to ship, pin to a release tag.** Tags are the stability anchor; master is not.

```bash
# direct clone pinned to a tag
git clone https://github.com/<user>/mkgui.git
cd mkgui
git checkout v0.1.0-beta

# or as a submodule
git submodule add https://github.com/<user>/mkgui.git mkgui
cd mkgui && git checkout v0.1.0-beta
```

If you need a fix that only exists on master, open an issue so it can be cherry-picked into the next tag rather than silently depending on a moving target.

## Features

- 45+ widget types: buttons, inputs, checkboxes, dropdowns, sliders, spinboxes, treeviews, listviews, gridviews, tabs, menus, toolbars, statusbars, toggles, comboboxes, datepickers, and more
- Auto-layout containers: VBox, HBox, Form, Group, Tabs, Splitters
- Software rendering via XShm (Linux) and GDI (Windows) -- no GPU required
- Built-in file dialogs, message boxes, input dialogs, color picker
- SVG icon system via PlutoSVG (Freedesktop icon naming, theme-aware monochrome)
- Toolbar display modes: icons only, text only, icons + text
- DPI scaling with auto-detection and `MKGUI_SCALE` environment variable override
- Keyboard accelerators with automatic menu shortcut display
- Undo/redo in text input and textarea widgets
- External file drag-and-drop (XDnd on Linux, WM_DROPFILES on Windows)
- Visual editor with drag-and-drop, property editing, and C code generation
- Unity build: `#include "mkgui.c"` and compile
- ~260KB stripped binary, ~5MB BSS, instant startup

## Visual editor

![Widget editor](screenshots/editor.png)

The editor generates complete, compilable C source files. Design your UI visually, set events, and hit Generate -- the output compiles and runs immediately.

See [documentation/editor.md](documentation/editor.md) for the full editor guide.

## Getting started

1. Copy the mkgui source into your project
2. `#include "mkgui.c"` in your application
3. Compile and link:
   ```bash
   # Linux
   gcc -std=c99 -O2 myapp.c -o myapp \
       $(pkg-config --cflags freetype2 fontconfig) -lX11 -lXext \
       $(pkg-config --libs freetype2 fontconfig) -lm

   # Windows (MinGW)
   x86_64-w64-mingw32-gcc -std=c99 -O2 myapp.c -o myapp.exe -lgdi32 -mwindows
   ```
4. Place an `icons/` directory next to your executable with SVG icons (see Icon section below)

OpenGL is only required if the application uses the `MKGUI_GLVIEW` widget (add `-lGL` on Linux or `-lopengl32` on Windows). The core library, editor, and all other widgets have no GPU dependency.

## Quick example

```c
#include "mkgui.c"

enum { ID_WIN = 0, ID_BTN = 1, ID_LBL = 2 };

static void on_event(struct mkgui_ctx *ctx, struct mkgui_event *ev, void *userdata) {
    (void)userdata;
    switch(ev->type) {
        case MKGUI_EVENT_CLOSE: {
            mkgui_quit(ctx);
        } break;

        case MKGUI_EVENT_CLICK: {
            if(ev->id == ID_BTN) {
                mkgui_label_set(ctx, ID_LBL, "Clicked!");
            }
        } break;
    }
}

int main(void) {
    struct mkgui_widget widgets[] = {
        MKGUI_W(MKGUI_WINDOW, ID_WIN, "Hello", "", 0,      400, 300, 0, 0, 0),
        MKGUI_W(MKGUI_BUTTON, ID_BTN, "Click", "", ID_WIN, 100, 0, MKGUI_FIXED, 0, 0),
        MKGUI_W(MKGUI_LABEL,  ID_LBL, "Ready", "", ID_WIN, 0, 0, 0, 0, 0),
    };

    struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
    if(!ctx) return 1;

    mkgui_run(ctx, on_event, NULL);
    mkgui_destroy(ctx);
    return 0;
}
```

## Icons

mkgui uses SVG icons via PlutoSVG, following the Freedesktop icon naming standard. Icons are loaded at runtime from a flat directory:

```c
mkgui_icon_load_svg_dir(ctx, "icons");           // load all SVGs from icons/
mkgui_icon_load_svg(ctx, "my-icon", "path.svg"); // load a single icon
```

Monochrome icons using `currentColor` automatically follow the theme text color and update on theme switch.

### Platform differences (important)

- **Linux:** if no bundled `icons/` directory is found, mkgui automatically falls back to the user's installed system icon theme (Papirus, Breeze, Adwaita, ...). Applications can run without shipping any icons and still display correct visuals. The editor's icon browser also scans system theme directories.
- **Windows:** there is no system icon theme. Both end-user applications and the editor require a bundled `icons/` directory (or an unpacked Freedesktop theme next to the binary for the editor's icon browser). Without one, widgets fall back to a magenta-diamond placeholder and the icon browser appears empty.

Windows users must therefore download a Freedesktop icon theme (e.g. [Papirus](https://github.com/PapirusDevelopmentTeam/papirus-icon-theme), [Breeze](https://invent.kde.org/frameworks/breeze-icons)) and either drop it next to the editor as `./Papirus/` for browsing, or run `extract_icons` to produce an `icons/` directory to ship with the application.

### Extracting icons

```bash
# extract_icons <icons_list.txt> <output_dir> [theme_dir] [size]
./out/extract_icons myapp_icons.txt icons/ ./Papirus 16
```

The icons list is produced by the editor; dynamically-loaded icons can be added to `myapp_icons_extra.txt`. See the [getting started guide](documentation/getting_started.md#icons) for the full workflow.

## Building the demo and editor

```bash
./build.sh          # normal (debug symbols)
./build.sh release  # optimized, stripped
./build.sh debug    # -g -O0
```

## Building on Windows

mkgui targets C99 and builds with either **MinGW GCC** or **clang-cl** (LLVM's MSVC-compatible driver). Plain MSVC `cl.exe` is not tested but the source should be compatible; please report issues.

### MSYS2 / MinGW (recommended)

The easiest option is **MSYS2**, which gives you a native MinGW GCC on Windows:

1. Install [MSYS2](https://www.msys2.org/)
2. Open the **MSYS2 UCRT64** terminal
3. Install the toolchain and FreeType:
   ```bash
   pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-freetype
   ```
4. Build your application (Windows uses GDI for fonts, so no fontconfig is needed):
   ```bash
   gcc -std=c99 -O2 myapp.c -o myapp.exe -lgdi32 -mwindows
   ```
   Add `-lopengl32` only if the application uses `MKGUI_GLVIEW`. Building `demo.c` requires `-lopengl32` since the demo showcases the GL view widget.

Cross-compiling from Linux also works. The included `build.sh` automatically builds Windows executables if `x86_64-w64-mingw32-gcc` is available.

### clang-cl (MSVC toolchain)

mkgui also compiles cleanly with `clang-cl` against the MSVC runtime. On Windows, install the **Build Tools for Visual Studio** (or the full IDE) and the **Windows SDK**, then from a Developer Command Prompt:

```cmd
clang-cl /O2 ^
  -DMKGUI_MAX_ICONS=32768 -D_CRT_SECURE_NO_WARNINGS ^
  myapp.c ^
  /link /subsystem:windows /entry:mainCRTStartup ^
  gdi32.lib user32.lib kernel32.lib shell32.lib advapi32.lib
```

Add `opengl32.lib` if the application uses `MKGUI_GLVIEW`. `/entry:mainCRTStartup` keeps `int main(void)` as the entry point under `/subsystem:windows` (equivalent to MinGW's `-mwindows`).

Cross-compiling from Linux with `clang-cl` is also possible if you have a copy of the MSVC CRT headers/libraries and the Windows SDK. Point clang at them with `/imsvc` for each include directory and `/libpath:` for each library directory, and use `-fuse-ld=lld-link` so the LLVM linker handles the COFF output:

```bash
clang-cl /O2 -fuse-ld=lld-link \
  /imsvc <msvc-crt>/include \
  /imsvc <winsdk>/Include/ucrt \
  /imsvc <winsdk>/Include/um \
  /imsvc <winsdk>/Include/shared \
  -DMKGUI_LIBRARY -DMKGUI_MAX_ICONS=32768 \
  -D_CRT_SECURE_NO_WARNINGS \
  mkgui.c demo.c \
  /Fedemo.exe \
  /link /subsystem:windows /entry:mainCRTStartup \
  /libpath:<msvc-crt>/lib/x86_64 \
  /libpath:<winsdk>/Lib/ucrt/x86_64 \
  /libpath:<winsdk>/Lib/um/x86_64 \
  gdi32.lib user32.lib kernel32.lib opengl32.lib shell32.lib advapi32.lib
```

## Documentation

- [API reference and widget guide](documentation/doc.md)
- [Visual editor guide](documentation/editor.md)
- [Adding new widgets](documentation/adding_widgets.md)
- [Layout engine reference](documentation/layout.md)

## Limitations

- Latin-1 (ISO 8859-1) text only -- covers Western European languages. No CJK, Arabic, or complex text shaping
- No right-to-left (RTL) layout support
- No accessibility API integration (tab navigation and keyboard accelerators are supported)

## Contributing

Solo project with a strict C style (hard tabs, `if(` not `if (`, unity build, no `const`, no forward declarations, etc. - see the source for the full flavour). Bug reports with a minimal reproducer are very welcome; please file an issue. Pull requests are disabled by choice - any fixes will be applied in-house from a good report. Security-sensitive reports: see [SECURITY.md](SECURITY.md).

## License

MIT
