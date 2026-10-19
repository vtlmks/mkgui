# Getting started with mkgui

## Prerequisites

**Linux:** Git, Bash, GCC, binutils, pkg-config, Python 3, FreeType2, Fontconfig, and Xlib + XShm development headers. Python is needed to build the icon catalogue; applications do not need it at runtime.

```sh
# Arch
pacman -S git base-devel python freetype2 fontconfig libx11 libxext

# Debian/Ubuntu
apt install git build-essential pkg-config python3 libfreetype-dev libfontconfig-dev libx11-dev libxext-dev
```

**Windows (cross-compile from Linux):** MinGW-w64. Font rendering goes through GDI on Windows, so FreeType and Fontconfig are not required.

```sh
# Arch
pacman -S mingw-w64-gcc

# Debian/Ubuntu
apt install gcc-mingw-w64-x86-64
```

**OpenGL is optional.** The core library, editor, static library, and all widgets except `MKGUI_GLVIEW` build and run without any OpenGL dependency. Only link `-lGL` (Linux) or `-lopengl32` (Windows) if your application actually uses the GL view widget. The bundled `demo` binary uses `MKGUI_GLVIEW`, so the demo build pulls in OpenGL; the editor and `libmkgui.so.1.0.0` do not.

For the default build including the demo, also install `libglvnd` on Arch or `libgl-dev` on Debian/Ubuntu. Use `./build.sh library` or `./build.sh editor` to build without the demo's OpenGL dependency.

## Building mkgui

```sh
git clone https://github.com/vtlmks/mkgui.git
cd mkgui
git checkout v1.0.0
git submodule update --init --recursive
./build.sh           # default: library + icon catalogue + demo + editor + tests
./build.sh editor    # libraries + visual designer
./build.sh library   # shared/static toolkit libraries + icon catalogue
./build.sh icons     # icon catalogue only
./build.sh core      # libraries + demo + tests, without the editor
```

`mkwin/` is a source submodule pinned to the revision required by mkgui. Run `git submodule update --init --recursive` after changing the mkgui checkout too. mkwin is compiled into the toolkit; applications do not need a separate mkwin shared library. The Fluent source snapshot is already vendored in this repository.

The default build produces:

| Output | Description |
|--------|-------------|
| `out/linux/libmkgui.so.1.0.0` | Linux shared library, SONAME `libmkgui.so.1.0.0` |
| `out/windows/mkgui.dll`, `libmkgui.dll.a` | Windows shared library and MinGW import library |
| `out/linux/libmkgui.a` | Linux static library for linking |
| `out/windows/libmkgui.a` | Windows static library (when MinGW is available) |
| `out/linux/demo`, `out/windows/demo.exe` | Demo application |
| `out/linux/editor`, `out/windows/editor.exe` | Visual GUI designer |
| `out/linux/libmkguiicons.so.1` | Shared SVG catalogue |
| `out/windows/mkguiicons.dll` | Windows SVG catalogue |
| `out/linux/test_layout`, `test_widgets`, `test_events`, `test_events_ext`, `test_smoke`, `test_window_visibility`, `test_dynamic` | Unit tests |
| `out/linux/mkguibench`, `out/windows/mkguibench.exe` | Layout, rendering, dirty-region, presentation, and complete-frame benchmark harness |

`-Lout/linux -lmkgui` or `-Lout/windows -lmkgui` selects the shared library. Link the explicit `libmkgui.a` path to select static linkage.

`./build.sh` is a Linux build script. Windows outputs are cross-compiled when `x86_64-w64-mingw32-gcc` is available. See [native Windows builds](#native-windows-builds) for MSYS2 commands.

Build modes: `./build.sh release` (optimized and stripped), `./build.sh debug` (no optimization), `./build.sh size` (`-Os` and stripped), `./build.sh asan` (AddressSanitizer + UBSan, Linux only), `./build.sh clean`. The default `normal` mode uses `-O2` with debug symbols. Modes and targets can be combined: `./build.sh release editor`. Targets build executables but do not run the tests.

The default build targets `x86-64-v2` for both Linux and Windows; the CPU target is set in `COMMON_CFLAGS` in `build.sh`.

The library is compiled with `-DMKGUI_LIBRARY`. Public functions are exported; toolkit internals remain hidden. Icon metadata, hash slots, and custom SVG source records grow on demand. Applications do not need library-build definitions.

For an overview of the changes from beta releases, see the [v1.0.0 migration guide](migration_v1.0.0.md).

## Checking the build

Build targets compile tests without running them. Run GUI tests with a working display, from the repository root. The shared-consumer tests exercise the actual library interface; the remaining tests include internals for widget, layout, and storage checks.

```sh
out/linux/test_shared
out/linux/test_shared_events
out/linux/test_editor_shared
out/linux/test_undo
out/linux/test_filedialog
out/linux/test_editor_features
```

`test_shared_events` drives native mouse/keyboard input and modal dismissal using only public toolkit functions plus native event injection. The editor persistence test needs a temporary directory for its project, generated source, and configuration:

```sh
test_dir=$(mktemp -d)
out/linux/test_editor_persistence "$test_dir"
rm -rf "$test_dir"
```

Other suites are in `tests/` and built under `out/linux/`. Windows builds include `test_shared.exe`, `test_shared_events.exe`, `test_editor_shared.exe`, `test_editor_persistence.exe`, `test_undo.exe`, `test_filedialog.exe`, `test_features_minimal.exe`, `test_icon_storage.exe`, and `test_platform_win32.exe`. Run them on Windows or use Wine, for example `wine out/windows/test_shared_events.exe`. Pass a writable Windows directory to the persistence test.

`python3 tests/test_install.py` checks release coexistence without a display or system installation. It builds distinguishable fixture libraries for five full releases, installs them into one temporary prefix, compiles consumers against each package, and verifies their headers, static linkage, SONAME dependencies, and continued operation after subsequent installations. It also checks `DESTDIR` staging and rejection of libraries with an incorrect SONAME.

## Option A: Shared library

Include `mkgui.h` and link the library from the selected release. Within a source checkout, `-Lout/linux -lmkgui` selects that checkout's library; its SONAME records the full release. The demo and editor use this model.

Use the complete [README example](../README.md#use-in-an-application) as `myapp.c`.

**Compile:**
```sh
gcc -std=c99 -O2 myapp.c -I. -Lout/linux -lmkgui \
    '-Wl,-rpath,$ORIGIN' -o out/linux/myapp
```

## Installing on Linux

`./install_icons.sh` installs the icon catalogue; `./install.sh` installs mkgui's
shared library, static archive, public headers, and release-specific pkg-config metadata (`mkgui1.0.0.pc` for this release).
Both scripts support `PREFIX` (default `/usr`) and `DESTDIR` for packaging.
The caller needs write access to the destination. Run these commands with that access for a systemwide installation:

```sh
./install_icons.sh
./install.sh
ldconfig
```

For release 1.0.0, the toolkit installs:

| File | Purpose |
|---|---|
| `/usr/lib/libmkgui.so.1.0.0` | Runtime library; SONAME is this complete filename |
| `/usr/lib/libmkgui1.0.0.so` | Development symlink to that runtime library |
| `/usr/lib/libmkgui1.0.0.a` | Static archive |
| `/usr/include/mkgui1.0.0/mkgui.h` | Public API header |
| `/usr/include/mkgui1.0.0/mkgui_glview.h` | Native OpenGL accessors |
| `/usr/lib/pkgconfig/mkgui1.0.0.pc` | Matching compiler and linker flags |

There is no systemwide unversioned toolkit header, `libmkgui.so`, or `mkgui.pc` to be overwritten by another release. Select the full release explicitly:

```sh
gcc myapp.c $(pkg-config --cflags --libs mkgui1.0.0) -o myapp
```

This selects the release-specific include directory and `-lmkgui1.0.0`. The linker records the library's SONAME, `libmkgui.so.1.0.0`. Releases 1.0.1, 1.1.0, or 2.0.0 install under their own full names and leave this application using 1.0.0.

The icon installer supplies its independently versioned library, headers, metadata, and notices. Both installers default to `/usr`; neither script changes privileges itself.

For package staging:

```sh
DESTDIR="$package_root" ./install_icons.sh
DESTDIR="$package_root" ./install.sh
```

`DESTDIR` changes the staging destination while keeping `/usr` as the installed prefix. Run `ldconfig` on the target system after installation, rather than against the staging directory.

For a writable local prefix:

```sh
PREFIX="$HOME/.local" ./install_icons.sh
PREFIX="$HOME/.local" ./install.sh
```

Then compile against the installation:

```sh
export PKG_CONFIG_PATH="$HOME/.local/lib/pkgconfig"
gcc myapp.c $(pkg-config --cflags --libs mkgui1.0.0) -Wl,-rpath,"$HOME/.local/lib" -o myapp
```

The explicit runtime path above lets the loader find the local prefix's libraries.
For deployment beside an executable, keep `libmkgui.so.1.0.0` and `libmkguiicons.so.1` beside it and build the executable with `'-Wl,-rpath,$ORIGIN'`. Retain the versioned icon library file when its SONAME path is a symlink. Neither the static archives nor import libraries are runtime files.

Linux uses the full release in the toolkit SONAME: `libmkgui.so.1.0.0`. Choose a tag and keep using its matching headers and library; other tags may change the API or ABI. Different full releases can be installed together, including releases with the same major number. Windows uses `mkgui.dll`, shipped with `mkguiicons.dll` beside
the executable. Its MinGW import library is `libmkgui.dll.a`.

## Native Windows builds

In the MSYS2 UCRT64 terminal, install Git, GCC, and Python:

```sh
pacman -S git mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-python
```

Clone/select the release and initialize mkwin as shown above. From the repository
root, build the catalogue, toolkit DLL, demo, and editor with native GCC:

```sh
mkdir -p out/windows
python3 tools/generate_icon_library.py
gcc -std=c99 -O2 -DMKGUIICONS_BUILD -shared mkguiicons.c mkguiicons_data.S -Wl,--out-implib,out/windows/libmkguiicons.dll.a -static-libgcc -o out/windows/mkguiicons.dll
gcc -std=c99 -O2 -march=x86-64-v2 -fwrapv -Imkwin -DMKGUI_LIBRARY -shared mkgui.c -Wl,--out-implib,out/windows/libmkgui.dll.a -static-libgcc -o out/windows/mkgui.dll -Lout/windows -lmkguiicons -lgdi32 -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic
gcc -std=c99 -O2 demo.c -Lout/windows -lmkgui -mwindows -lopengl32 -o out/windows/demo.exe
gcc -std=c99 -O2 editor.c -Lout/windows -lmkgui -mwindows -o out/windows/editor.exe
```

Build an application with the same compiler:

```sh
gcc -std=c99 -O2 myapp.c -I. -Lout/windows -lmkgui -mwindows -o out/windows/myapp.exe
```

The import libraries (`*.dll.a`) are link-time inputs. Distribute the executable
with `mkgui.dll` and `mkguiicons.dll`; the matching editor and toolkit should
come from the same build because the editor uses the private rendering bridge.
Applications calling `mkguiicons_*()` directly also link `-lmkguiicons`.

## Option B: Static library

Include `mkgui.h`, define `MKGUI_STATIC`, and link the explicit archive:

```sh
gcc -std=c99 -O2 -DMKGUI_STATIC -I. myapp.c out/linux/libmkgui.a \
    $(pkg-config --cflags --libs freetype2 fontconfig) -lX11 -lXext -lm \
    -Lout/linux -lmkguiicons '-Wl,-rpath,$ORIGIN' -o out/linux/myapp

x86_64-w64-mingw32-gcc -std=c99 -O2 -DMKGUI_STATIC -I. myapp.c \
    out/windows/libmkgui.a -Lout/windows -lmkguiicons -lgdi32 -mwindows \
    -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic -o out/windows/myapp.exe
```

For an installed release, use its matching header directory and static archive, followed by the platform and icon dependencies:

```sh
gcc -std=c99 -O2 -DMKGUI_STATIC myapp.c $(pkg-config --cflags mkgui1.0.0) "$(pkg-config --variable=libdir mkgui1.0.0)/libmkgui1.0.0.a" $(pkg-config --libs freetype2 fontconfig mkguiicons) -lX11 -lXext -lm -o myapp
```
This embeds the toolkit in the application; the icon catalogue remains a shared-library dependency.

## Option C: Unity build

Include `mkgui.c` directly in your source file. The toolkit compiles as one translation unit; link the shared icon catalogue separately. The toolkit sources, embedded SVG rasterizer, and platform backend are included transitively through `mkgui.c`. Feature exclusions can remove unused widget implementations.

In the [README example](../README.md#use-in-an-application), replace `#include "mkgui.h"` with `#include "mkgui.c"`. Keep the application code and icon-name include.

**Compile:**
```sh
gcc -std=c99 -O2 -Imkwin myapp.c $(pkg-config --cflags --libs freetype2 fontconfig) -lX11 -lXext -lm -Lout/linux -lmkguiicons '-Wl,-rpath,$ORIGIN' -o out/linux/myapp
```

Add `-lGL` only if your application uses `MKGUI_GLVIEW`. On Windows, the equivalent is `-lopengl32`.

Unity builds need the complete source checkout, including mkwin. The installed public headers are sufficient for shared/static consumers but do not install toolkit implementation sources.

## Icons

Both platforms use the shared Fluent catalogue. Build it with `./build.sh icons`.
On Linux, `./install_icons.sh` installs into `/usr` by default; run `ldconfig`
after a systemwide installation. Packagers can use `PREFIX=/usr DESTDIR=...`.
On Windows, ship `out/windows/mkguiicons.dll` beside the executable together with `mkgui.dll`. An application using only toolkit functions links `-lmkgui`; direct calls to `mkguiicons_*()` also require `-lmkguiicons`.

Include `mkguiicons_names.h` to use names such as `MKICON_SAVE`, `MKICON_SEARCH`,
and `MKICON_FOLDER_OPEN` in widget initializers or `mkgui_set_icon()`. String names
such as `"save"` work too. The editor's icon picker searches the same catalogue;
its projects and generated source contain names, requiring no separate icon files
or export lists. Monochrome icons follow the theme. Every scale is rasterized
directly from SVG at the required pixel size.

See [icons.md](icons.md) for the enumeration API, styles, custom icons, and the
shared library's version contract.

## DPI scaling

mkgui auto-detects display DPI on startup (via Xft.dpi on Linux, GetDpiForWindow on Windows). Override with:

```c
mkgui_ctx_set_scale(ctx, 2.0f);  // 2x scale for HiDPI displays
```

Or set the `MKWIN_SCALE` environment variable before launching: `MKWIN_SCALE=1.5`, `MKWIN_SCALE=150%`, or `MKWIN_SCALE=144dpi`. The env var takes precedence over auto-detection.

Widget geometry, fonts, icons, and dialog windows follow the scale. The scale factor can be changed at runtime. Range: 0.5 to 4.0.

## Widget layout

Widgets are defined as a flat array of `struct mkgui_widget`. Each widget has a `parent_id` that determines its position in the layout tree. VBox/HBox/Form arrange children, Panel shares VBox layout, and Group insets a child layout container.

```c
struct mkgui_widget widgets[] = {
//   MKGUI_W(type, id, label, icon, parent, w, h, flags, style, weight)
	MKGUI_W(MKGUI_WINDOW, ID_WINDOW, "Title",  "", 0,      800, 600, 0, 0, 0),
	MKGUI_W(MKGUI_VBOX,   ID_VBOX,   "",       "", ID_WINDOW, 0, 0,   0, 0, 0),
	MKGUI_W(MKGUI_LABEL,  ID_LABEL,  "Hello",  "", ID_VBOX,0,   0,   0, 0, 0),
	MKGUI_W(MKGUI_BUTTON, ID_BTN,    "OK",     "", ID_VBOX,100, 0, MKGUI_FIXED, 0, 0),
};
```

- `w=0, h=0`: automatic sizing according to the parent container and widget type; flexible children can fill the available space
- `MKGUI_FIXED`: removed from box flex distribution; the main-axis size uses its positive hint or a measured/natural fallback. Cross-axis sizing follows the parent's alignment rules
- `weight`: controls how flexible widgets share remaining space in a container

See the [layout guide](layout.md) for sizing rules and the [API reference](doc.md) for widget functions.

## The editor

The visual designer lets you create widget layouts interactively:

```sh
out/linux/editor
```

- Click a palette entry, then click the canvas to place widgets; Escape ends placement mode
- Set properties in the right panel
- Click the icon field to browse available icons
- Generate C code with File > Generate Code
- Test your layout with the Test button

The editor outputs a complete `.c` file with widget array, event handler stubs, and an `mkgui_ctx_run()` loop. Generated programs include `mkgui.h` and link the shared library by default. Define `MKGUI_UNITY` to compile the generated source with the toolkit included instead.

## Cross-platform

mkgui runs on Linux (X11) and Windows (Win32). Write once, compile for both:

```sh
# Linux
gcc -std=c99 -O2 myapp.c $(pkg-config --cflags --libs mkgui1.0.0) -o myapp

# Windows (cross-compile, no FreeType needed -- uses GDI for fonts)
x86_64-w64-mingw32-gcc -std=c99 -O2 -I. myapp.c -Lout/windows -lmkgui -mwindows -o out/windows/myapp.exe
```

Both platforms use the same public widget definitions, event interface, and catalogue icon names.
