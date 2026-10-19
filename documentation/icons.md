# Shared icon catalogue

`libmkguiicons.so.1` and `mkguiicons.dll` contain the same Fluent SVG catalogue.
Applications share one library; they do not need icon directories, manifests, or
an extraction step. mkgui itself is available as a shared library, static library, or unity
build.

## Building and installing

```sh
./build.sh icons
./install_icons.sh
```

The installer defaults to `/usr` and supports `PREFIX` and `DESTDIR`.
Systemwide installation requires write access and a subsequent `ldconfig`.
For packaging, use `PREFIX=/usr DESTDIR="$package_root" ./install_icons.sh`.
The installer provides the versioned library, SONAME/development symlinks,
headers, pkg-config metadata, and upstream notices. Build outputs use `$ORIGIN`
so the editor and demo find the library alongside themselves.

On Linux, link with `$(pkg-config --cflags --libs mkguiicons)` after installation.
For a local build, use `-Lout/linux -lmkguiicons` and place the library beside the
executable with `-Wl,-rpath,'$ORIGIN'`. On Windows, use
`-Lout/windows -lmkguiicons` and put `mkguiicons.dll` beside the executable.

The checked-in upstream snapshot is recorded in
`fluentui-system-icons/UPSTREAM`. Python 3 generates the catalogue during the
build. No Python runtime is needed by applications. The provider contains SVG
bytes and lookup tables; it does not include a renderer or compression library.

## Names and styles

```c
#include "mkgui.h"
#include "mkguiicons_names.h"

mkgui_set_icon(win, ID_SAVE, MKICON_SAVE);
mkgui_set_icon(win, ID_SAVE, MKICON_SAVE_FILLED);
```

`MKICON_SAVE` expands to `"save"`. Names drop the upstream `ic_fluent_` prefix,
size, and default `regular` suffix; word separators become hyphens. Additional
styles use `:filled`, `:light`, and `:color`. Default names prefer regular, then
filled, light, or colour when regular does not exist. Localized designs and
trailing direction variants use `@ar`, `@he`, `@rtl`, etc. Explicitly named
families such as `arrow-left` retain their names. Generated C macros cover every
selectable name. Lookup accepts the catalogue names directly.

Monochrome designs follow mkgui's text colour; colour designs retain their colours.
The provider selects the closest available SVG design size, preferring the larger
one on a tie. mkgui rasterizes that SVG directly at the required pixel dimensions,
including arbitrary scales such as 2.13. It does not resize a cached bitmap.

## Enumeration and SVG lookup

```c
#include <stdio.h>
#include "mkguiicons.h"

// [=]===^=[ main ]=============================================================================[=]
int main(void) {
	uint32_t count = mkguiicons_count();

	for(uint32_t i = 0; i < count; ++i) {
		const char *name = mkguiicons_name(i);
		uint32_t length;
		const char *svg = mkguiicons_svg(name, 24, &length);

		if(svg) {
			printf("%s: %u SVG bytes\n", name, length);

		}
	}

	return 0;

}
```

| Function | Returns |
|---|---|
| `mkguiicons_count()` | Number of selectable names, including style variants |
| `mkguiicons_name(index)` | Stable string name |
| `mkguiicons_keywords(index)` | Upstream description/metaphor text for searching |
| `mkguiicons_topic(index)` | Name group index |
| `mkguiicons_topic_count()` | Number of name groups |
| `mkguiicons_topic_name(index)` | First name component used as the group label |
| `mkguiicons_size_count(index)` | Number of available SVG source sizes |
| `mkguiicons_size(index, variant)` | Source size in ascending order |
| `mkguiicons_svg(name, size, &length)` | SVG bytes selected by name and preferred size |
| `mkguiicons_release()` | Upstream icon release string |

All returned pointers belong to the library and remain valid until unload.
SVG bytes are NUL terminated and `length` excludes that terminator. The length
pointer must be non-NULL. Unknown names return NULL and length zero; size zero
selects 24. Invalid string indices return NULL, invalid topic indices return
`UINT32_MAX`, and invalid size indices return zero. Enumeration indices can
change on update; save names.

The SONAME major describes the provider ABI, independently of mkgui versions.
Compatible catalogue updates retain existing names and exported signatures.
The full filename records the upstream release; compatible updates retain
`libmkguiicons.so.1`. Multiple mkgui versions can use that same provider.

## Editor and custom icons

The editor picker uses these public enumeration functions, with name/keyword
search, name groups, style selection, and thumbnails rendered at the active scale.
Projects and generated C store names. The picker returns a name through
`mkgui_icon_browser(win, size, out_name, name_size)`.

Applications may still register explicit custom SVG files through
`mkgui_icon_load_svg()` or custom pixels through `mkgui_icon_add()`. Unknown widget
names display the existing magenta placeholder. Desktop theme scanning and the
old per-application SVG directory/export-list workflow have been removed.

## Runtime storage

Each process owns its rasterized icon cache. Metadata, lookup slots, and custom
SVG records grow in reserve/commit arenas as icons are added. Indices stay stable through
growth and replacement. Cached pixel atlases and loaded SVG text are
released with the last window, or by context destruction if no window was opened.
Raster icons added before the first window survive its initialization. Failed
replacements retain the previous icon.
