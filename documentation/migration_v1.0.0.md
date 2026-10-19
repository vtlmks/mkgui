# Migrating to mkgui v1.0.0

v1.0.0 introduces shared toolkit and icon libraries. These instructions cover
the changes since v0.5.0-beta. Applications from earlier betas also need the
lifecycle/API changes in the [earlier migration guide](migration_v0.3.0-beta.md).

## Build and link

Update the mkwin submodule after selecting the mkgui release:

```sh
git submodule update --init --recursive
./build.sh
```

For the default shared-library model, replace `#include "mkgui.c"` with
`#include "mkgui.h"`. Applications using `printf`, allocation functions, or other
C library functions must include the corresponding standard headers themselves.

```sh
# Linux, using the uninstalled libraries beside the executable
gcc -std=c99 -O2 myapp.c -I. -Lout/linux -lmkgui '-Wl,-rpath,$ORIGIN' -o out/linux/myapp

# Windows, using the MinGW import library
x86_64-w64-mingw32-gcc -std=c99 -O2 myapp.c -I. -Lout/windows -lmkgui -mwindows -o out/windows/myapp.exe
```

Linux applications installed systemwide can use
`gcc myapp.c $(pkg-config --cflags --libs mkgui1.0.0) -o myapp` after running both
installers and refreshing the linker cache. Windows applications ship
`mkgui.dll` and `mkguiicons.dll` beside the executable.

Static archive consumers define `MKGUI_STATIC`. Unity builds can keep including
`mkgui.c`; both models still link the separate icon catalogue. Full commands,
prerequisites, and installation layouts are in [getting started](getting_started.md).

## Icon names and loading

Both platforms now use the same catalogue. Remove desktop-theme selection,
icon-directory loading, extraction tools, and per-application icon export lists.
The removed APIs are `mkgui_icon_load_svg_dir()`, `mkgui_icon_load_app_icons()`,
`mkgui_icon_get_dir()`, and `mkgui_icon_browser_theme()`.

Old icon-name aliases are gone. Select the intended icon from the editor picker
and use its canonical name or a constant from `mkguiicons_names.h`:

```c
#include "mkguiicons_names.h"

mkgui_set_icon(win, ID_SAVE, MKICON_SAVE);
```

There is no automatic mapping of old desktop-theme names. Explicit custom SVG
files can still be registered with `mkgui_icon_load_svg()`, and application
pixels with `mkgui_icon_add()`.

The browser now takes `(win, size, out_name, name_size)` and returns the chosen
catalogue name in `out_name`. Remove the old output-path argument and use the
returned name directly. Enumeration and SVG access are documented in
[the icon guide](icons.md).

## Application internals and feature configuration

Shared-library consumers cannot access the fields of `struct mkgui_window` or
`struct mkgui_ctx`; use public getters, setters, and callbacks. The bundled
editor's private bridge is for its matching toolkit build.

All widgets are available in the standard shared library. Existing source
applications also retain all widgets unless they define `MKGUI_NO_*` exclusions.
The editor writes a sibling `*_features.h` file alongside generated source;
keep those files together. Exclusions reduce a unity build or a toolkit compiled
with that same configuration. They do not change an already built shared library.
`MKGUI_HAS_ALL` overrides exclusions and enables all widget types and dialogs.

Generated applications use the public header by default. Define `MKGUI_UNITY`
when compiling generated source to include the toolkit instead. The obsolete
`MKGUI_MAX_ICONS` macro is removed; icon cache storage now grows on demand.

## Released versions

Linux uses the full release in the toolkit SONAME, beginning with `libmkgui.so.1.0.0`. Headers, static archives, and pkg-config packages also use the full release. Select this release with `pkg-config --cflags --libs mkgui1.0.0`. Applications using other releases retain their own headers and runtime libraries, even when their major numbers match.
Windows applications carry the matching DLL beside their executable.

The icon catalogue has an independent ABI, beginning with
`libmkguiicons.so.1`. Compatible catalogue updates preserve existing names and
can serve several mkgui releases. Enumeration indices can change; persist names.

Release tags remain fixed snapshots. Stay on the tag selected for an application; other tags may change the API or ABI. A maintenance branch can start from a release tag and publish a new patch tag. Updating an application to that tag is explicit, rather than replacing its installed runtime library. Each mkgui checkout retains its pinned mkwin revision.
