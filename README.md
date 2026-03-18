# mkgui

Minimal GUI toolkit for Linux (X11) and Windows (Win32). Software rendered, single-file unity build. Breeze-inspired styling. No third-party dependencies beyond FreeType2 and Xlib/GDI.

This is a personal project by a single developer. It works well for what it does, but it has not been exhaustively tested across every platform, hardware configuration, or edge case. Use it, enjoy it, file issues if something breaks -- but don't expect the test coverage of a toolkit backed by a corporation. There are no guarantees, no roadmap, and no promises.

![Demo application](screenshots/demo.png)

## Features

- 25+ widget types: buttons, inputs, checkboxes, dropdowns, sliders, spinboxes, treeviews, listviews, tabs, menus, toolbars, statusbars, and more
- Auto-layout containers: VBox, HBox, Form, Group, Tabs, Splitters
- Software rendering via XShm (Linux) and GDI (Windows) -- no GPU required
- Built-in file dialogs, message boxes, input dialogs
- Material Design Icons (7400+ icons, monochrome, theme-tinted)
- Dual icon pack support for toolbar-specific icon sizes
- Toolbar display modes: icons only, text only, icons + text
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
       $(pkg-config --cflags freetype2) -lX11 -lXext $(pkg-config --libs freetype2)

   # Windows (MinGW)
   x86_64-w64-mingw32-gcc -std=c99 -O2 myapp.c -o myapp.exe -lgdi32 -mwindows
   ```
4. Place `mdi_icons.dat` next to your executable for icon support

## Quick example

```c
#include "mkgui.c"

enum { ID_WIN = 0, ID_BTN = 1, ID_LBL = 2 };

int main(void) {
    struct mkgui_widget widgets[] = {
        { MKGUI_WINDOW, ID_WIN, "Hello",  "",  0,      0, 0, 400, 300, 0, 0 },
        { MKGUI_BUTTON, ID_BTN, "Click",  "",  ID_WIN, 10, 10, 100, 28, 0, 0 },
        { MKGUI_LABEL,  ID_LBL, "Ready",  "",  ID_WIN, 10, 50, 200, 24, 0, 0 },
    };

    struct mkgui_ctx *ctx = mkgui_create(widgets, 3);
    if(!ctx) return 1;

    struct mkgui_event ev;
    uint32_t running = 1;
    while(running) {
        while(mkgui_poll(ctx, &ev)) {
            if(ev.type == MKGUI_EVENT_CLOSE) running = 0;
            if(ev.type == MKGUI_EVENT_CLICK && ev.id == ID_BTN) {
                mkgui_label_set(ctx, ID_LBL, "Clicked!");
            }
        }
        mkgui_wait(ctx);
    }

    mkgui_destroy(ctx);
    return 0;
}
```

## Icon pack

mkgui uses an external `mdi_icons.dat` file containing pre-rasterized Material Design Icons. To generate or regenerate the icon pack from the MDI font:

```bash
./tools/gen_icons ext/materialdesignicons-webfont.ttf 18 mdi_icons.dat
```

The second argument is the icon size in pixels. For toolbar-specific larger icons:

```bash
./tools/gen_icons ext/materialdesignicons-webfont.ttf 32 mdi_icons_toolbar.dat icons.txt
```

Where `icons.txt` lists only the icon names used in your toolbar (one per line). If `mdi_icons_toolbar.dat` exists, the toolbar uses it automatically; otherwise it falls back to the regular icon pack.

## Building the demo and editor

```bash
./build.sh          # normal (debug symbols)
./build.sh release  # optimized, stripped
./build.sh debug    # -g -O0
```

## Building on Windows

mkgui requires GCC (C99). It does not compile with MSVC (`cl.exe`).

The easiest option is **MSYS2**, which gives you a native MinGW GCC on Windows:

1. Install [MSYS2](https://www.msys2.org/)
2. Open the **MSYS2 UCRT64** terminal
3. Install the toolchain and FreeType:
   ```bash
   pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-freetype
   ```
4. Build:
   ```bash
   gcc -std=c99 -O2 demo.c -o demo.exe -lgdi32 -mwindows \
       $(pkg-config --cflags freetype2) $(pkg-config --libs freetype2)
   ```

Alternatively, if you already use **WSL2** on Windows, just build the Linux version -- it runs natively under WSL2 with X11 forwarding or WSLg.

Cross-compiling from Linux also works. The included `build.sh` automatically builds Windows executables if `x86_64-w64-mingw32-gcc` is available.

## Documentation

- [API reference and widget guide](documentation/doc.md)
- [Visual editor guide](documentation/editor.md)
- [Adding new widgets](documentation/adding_widgets.md)

## Limitations

- ASCII/Latin text only. No IME, CJK, or complex text shaping
- No right-to-left (RTL) layout support
- No DPI scaling / HiDPI awareness
- No accessibility API integration

## License

MIT
