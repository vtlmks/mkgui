# mkgui

> **Warning:** This project is under heavy development. APIs are unstable and may change without notice. Not recommended for production use yet.

Minimal GUI toolkit for Linux (X11) and Windows (Win32). Software rendered, single-file unity build. No third-party dependencies beyond FreeType2 and Xlib (Linux) or GDI (Windows).

## Getting started

1. Add mkgui as a submodule or copy the source into your project
2. `#include "mkgui/mkgui.c"` in your application
3. Compile and link:
   ```bash
   gcc -std=c99 -O2 myapp.c -o myapp \
       $(pkg-config --cflags freetype2) -lX11 -lXext $(pkg-config --libs freetype2)
   ```
4. Place `mdi_icons.dat` next to your executable for icon support

## Icon pack

mkgui uses an external `mdi_icons.dat` file containing pre-rasterized Material Design Icons. This file must be distributed alongside the executable.

**Windows:** Place `mdi_icons.dat` in the same directory as the `.exe`.

**Linux:** Place `mdi_icons.dat` next to the binary.

To generate or regenerate the icon pack from the MDI font:

```bash
./tools/gen_icons ext/materialdesignicons-webfont.ttf 18 mdi_icons.dat
```

The second argument is the icon size in pixels. The icon pack contains ~7400 icons and is approximately 2.5 MB at 18x18.

## Building the demo and editor

```bash
./build.sh          # normal (debug symbols)
./build.sh release  # optimized, stripped
./build.sh debug    # -g -O0
```

## Documentation

See [documentation/doc.md](documentation/doc.md) for the full API reference, widget types, layout system, and examples.

## License

MIT
