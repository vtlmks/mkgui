# mkgui

Minimal GUI toolkit for Linux (X11) and Windows (Win32). Software rendered, available as a shared library or a unity build (`#include "mkgui.c"`). Breeze-inspired styling. Uses FreeType2, Xlib/GDI, and the shared mkguiicons catalogue. The X11/Win32 windowing layer lives in a small sibling library, [mkwin](https://github.com/vtlmks/mkwin), pulled in as a git submodule.

This is a personal project by a single developer. It works well for what it does, but it has not been exhaustively tested across every platform, hardware configuration, or edge case. Use it, enjoy it, file issues if something breaks -- but don't expect the test coverage of a toolkit backed by a corporation. There are no guarantees, no roadmap, and no promises.

![Demo application](screenshots/demo.png)

## Project status

**v1.0.0 is the first stable release.** The author has used mkgui to ship several working applications. The shared library starts at version 1.0.0. Choose a release tag for an application and stay on it. Other tags may change the API or ABI; updating is explicit. Each full release has its own Linux SONAME, headers, static archive, and pkg-config package, so applications using different releases can coexist, including releases with the same major number.

## Versioning

Linux installs `libmkgui.so.1.0.0` with that same full SONAME. Each release installs its own headers, static archive, and pkg-config package, such as `mkgui1.0.0`; it does not replace another release, even when their major numbers match. Windows applications ship a matching `mkgui.dll` beside their executable. The icon catalogue has its own independent version.

`master` is the development branch. It can change behaviour, break API, or introduce regressions between commits without notice -- that's what it's for.

**For anything you intend to ship, pin to a release tag.** Tags are the stability anchor; master is not.

```bash
# direct clone pinned to a tag
git clone https://github.com/<user>/mkgui.git
cd mkgui
git checkout v1.0.0
git submodule update --init --recursive   # fetch the mkwin platform-layer submodule

# or as a submodule of your own project
git submodule add https://github.com/<user>/mkgui.git mkgui
cd mkgui && git checkout v1.0.0 && git submodule update --init --recursive
```

mkgui's own platform layer, `mkwin`, is itself a submodule, so the `git submodule update --init --recursive` step is required after any clone or checkout -- `mkgui.c` `#include`s the mkwin sources and will not compile without them. (Tags before `v0.5.0-beta` predate the submodule and simply skip that step.)

If you need a fix that only exists on master, open an issue so it can be cherry-picked into the next tag rather than silently depending on a moving target.

## Features

- 45+ widget types: buttons, inputs, checkboxes, dropdowns, sliders, spinboxes, treeviews, listviews, gridviews, tabs, menus, toolbars, statusbars, toggles, comboboxes, datepickers, and more
- Auto-layout containers: VBox, HBox, Form, Group, Tabs, Splitters
- Software rendering via XShm (Linux) and GDI (Windows) -- no GPU required
- Built-in file dialogs, message boxes, input dialogs, color picker
- Shared Fluent SVG icon catalogue via PlutoSVG, with theme-aware monochrome
- Toolbar display modes: icons only, text only, icons + text
- DPI scaling with auto-detection and `MKWIN_SCALE` environment variable override
- Keyboard accelerators with automatic menu shortcut display
- Undo/redo in text input and textarea widgets
- External file drag-and-drop (XDnd on Linux, WM_DROPFILES on Windows)
- Visual editor with drag-and-drop, property editing, and C code generation
- Shared library: include `mkgui.h` and link `-lmkgui`
- Unity and static builds remain available

## Visual editor

![Widget editor](screenshots/editor.png)

The editor generates complete, compilable C source files. Design your UI visually, set events, and hit Generate -- the output compiles and runs immediately.

See [documentation/editor.md](documentation/editor.md) for the full editor guide.

## Getting started

mkgui pulls its X11/Win32 windowing layer from the [mkwin](https://github.com/vtlmks/mkwin) git submodule, so make sure it is checked out -- `mkgui.c` `#include`s the mkwin sources and will not compile without them.

1. Get the mkgui source **with its submodule**: clone the repo, then run `git submodule update --init --recursive`.
2. Run `./build.sh` to build the libraries, demo, editor, and tests.
3. `#include "mkgui.h"` in your application and link the shared library:
   ```bash
   # Linux, with the libraries beside the executable
   gcc -std=c99 -O2 myapp.c -I. -Lout/linux -lmkgui '-Wl,-rpath,$ORIGIN' -o out/linux/myapp

   # Windows (MinGW)
   x86_64-w64-mingw32-gcc -std=c99 -O2 myapp.c -I. -Lout/windows -lmkgui -mwindows -o out/windows/myapp.exe
   ```
4. On Linux, install with `./install_icons.sh` and `./install.sh` (`PREFIX` and `DESTDIR` are supported), or keep the libraries beside the executable. On Windows, ship `mkgui.dll` and `mkguiicons.dll` beside the executable.

Installed Linux builds use `gcc myapp.c $(pkg-config --cflags --libs mkgui1.0.0) -o myapp`.
See [getting started](documentation/getting_started.md) for static and unity builds.

OpenGL is only required if the application uses the `MKGUI_GLVIEW` widget (add `-lGL` on Linux or `-lopengl32` on Windows). The core library, editor, and all other widgets have no GPU dependency.

## Quick example

```c
#include "mkgui.h"

enum { ID_WIN = 0, ID_BTN = 1, ID_LBL = 2 };

static void on_event(struct mkgui_window *win, struct mkgui_event *ev, void *userdata) {
    (void)userdata;
    switch(ev->type) {
        case MKGUI_EVENT_CLOSE: {
            mkgui_ctx_quit(mkgui_window_get_ctx(win));
        } break;

        case MKGUI_EVENT_CLICK: {
            if(ev->id == ID_BTN) {
                mkgui_label_set(win, ID_LBL, "Clicked!");
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

    struct mkgui_ctx *ctx = mkgui_ctx_create();
    if(!ctx) return 1;
    struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, 3, NULL, 0, 0);
    if(!win) {
        mkgui_ctx_destroy(ctx);
        return 1;
    }

    mkgui_ctx_run(ctx, on_event, NULL);
    mkgui_window_destroy(win);
    mkgui_ctx_destroy(ctx);
    return 0;
}
```

## Icons

Linux and Windows use the same Fluent SVG catalogue through `libmkguiicons.so.1`
or `mkguiicons.dll`. The library contains 21,788 SVG designs, exposed as 6,683
names covering styles, sizes, languages, and directions. It is approximately
28 MiB and is shared by applications. The upstream snapshot is vendored as
ordinary files, without a submodule.

```c
#include "mkguiicons_names.h"
mkgui_set_icon(win, ID_SAVE, MKICON_SAVE);
```

Icons are rasterized directly at the current pixel size. Monochrome icons follow
the theme text colour. The editor browses this catalogue, with name/keyword search,
name groups, and style selection. Projects store icon names; no icon manifest or
per-application SVG directory is needed. Custom SVGs can still be registered with
`mkgui_icon_load_svg()`.

Build with `./build.sh icons`. Install on Linux with `./install_icons.sh`
(default prefix `/usr`; supports `PREFIX` and `DESTDIR`), then refresh the
system linker cache when installing systemwide. On Windows, put `mkguiicons.dll`
beside the executable. Build outputs already include the library beside the
editor and demo. See [the icon API](documentation/icons.md) for enumeration,
naming, linking, and installation.

## Building the demo and editor

```bash
./build.sh          # normal (debug symbols)
./build.sh release  # optimized, stripped
./build.sh debug    # -g -O0
```

## Benchmarking

`out/linux/mkguibench` measures layout, full rendering, dirty rendering, completed presentation, and complete frames independently:

```bash
out/linux/mkguibench layout mixed 2048 1920 1080 10000
out/linux/mkguibench render buttons 256 1920 1080 1000
out/linux/mkguibench dirty mixed 256 1920 1080 10000
out/linux/mkguibench present fill 0 1920 1080 1000
out/linux/mkguibench frame mixed 256 1920 1080 1000
```

The output is one machine-readable row containing total time, time per iteration, time per widget or pixel, a minimum effective framebuffer bandwidth, and a framebuffer checksum. Use a real X server for presentation measurements. Xvfb is suitable for correctness and repeatability checks but does not represent display-server or hardware presentation cost.

For hardware counters, run a sufficiently long case pinned to one CPU:

```bash
taskset -c 2 perf stat -r 5 -e cycles,instructions,branches,branch-misses,cache-references,cache-misses out/linux/mkguibench render mixed 256 1920 1080 5000
```

## Building on Windows

mkgui targets C99 and builds with either **MinGW GCC** or **clang-cl** (LLVM's MSVC-compatible driver). Plain MSVC `cl.exe` is not tested but the source should be compatible; please report issues.

### MSYS2 / MinGW (recommended)

The easiest option is **MSYS2**, which gives you a native MinGW GCC on Windows:

1. Install [MSYS2](https://www.msys2.org/)
2. Open the **MSYS2 UCRT64** terminal
3. Install the toolchain:
   ```bash
   pacman -S mingw-w64-ucrt-x86_64-gcc
   ```
4. Build your application, then ship `mkgui.dll` and `mkguiicons.dll` beside it:
   ```bash
   gcc -std=c99 -O2 myapp.c -I. -Lout/windows -lmkgui -mwindows -o out/windows/myapp.exe
   ```
   Add `-lopengl32` only if the application uses `MKGUI_GLVIEW`. Building `demo.c` requires `-lopengl32` since the demo showcases the GL view widget.

Cross-compiling from Linux also works. The included `build.sh` automatically builds Windows executables if `x86_64-w64-mingw32-gcc` is available.

### clang-cl (MSVC toolchain)

For a unity build with `clang-cl`, include `mkgui.c` in the application and supply an MSVC-compatible import library for `mkguiicons.dll`. On Windows, install the **Build Tools for Visual Studio** (or the full IDE) and the **Windows SDK**, then from a Developer Command Prompt:

```cmd
clang-cl /O2 /I mkwin ^
  -D_CRT_SECURE_NO_WARNINGS ^
  myapp.c ^
  /link /subsystem:windows /entry:mainCRTStartup ^
  gdi32.lib user32.lib kernel32.lib shell32.lib advapi32.lib mkguiicons.lib
```

Add `opengl32.lib` if the application uses `MKGUI_GLVIEW`. `/entry:mainCRTStartup` keeps `int main(void)` as the entry point under `/subsystem:windows` (equivalent to MinGW's `-mwindows`).

Cross-compiling from Linux with `clang-cl` is also possible if you have a copy of the MSVC CRT headers/libraries and the Windows SDK. Point clang at them with `/imsvc` for each include directory and `/libpath:` for each library directory, and use `-fuse-ld=lld-link` so the LLVM linker handles the COFF output:

```bash
clang-cl /O2 /I mkwin -fuse-ld=lld-link \
  /imsvc <msvc-crt>/include \
  /imsvc <winsdk>/Include/ucrt \
  /imsvc <winsdk>/Include/um \
  /imsvc <winsdk>/Include/shared \
  -DMKGUI_LIBRARY -DMKGUI_STATIC \
  -D_CRT_SECURE_NO_WARNINGS \
  mkgui.c demo.c \
  /Fedemo.exe \
  /link /subsystem:windows /entry:mainCRTStartup \
  /libpath:<msvc-crt>/lib/x86_64 \
  /libpath:<winsdk>/Lib/ucrt/x86_64 \
  /libpath:<winsdk>/Lib/um/x86_64 \
  gdi32.lib user32.lib kernel32.lib opengl32.lib shell32.lib advapi32.lib mkguiicons.lib
```

## Documentation

- [API reference and widget guide](documentation/doc.md)
- [Visual editor guide](documentation/editor.md)
- [Adding new widgets](documentation/adding_widgets.md)
- [Layout engine reference](documentation/layout.md)
- [Benchmark harness and current baseline](documentation/benchmark.md)

## Limitations

- Latin-1 (ISO 8859-1) text only -- covers Western European languages. No CJK, Arabic, or complex text shaping
- No right-to-left (RTL) layout support
- No accessibility API integration (tab navigation and keyboard accelerators are supported)

## Contributing

Solo project with a strict C style (hard tabs, `if(` not `if (`, unity build, no `const`, no forward declarations, etc. - see the source for the full flavour). Bug reports with a minimal reproducer are very welcome; please file an issue. Pull requests are disabled by choice - any fixes will be applied in-house from a good report. Security-sensitive reports: see [SECURITY.md](SECURITY.md).

## License

MIT
