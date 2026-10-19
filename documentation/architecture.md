# Internal architecture

This page describes toolkit internals. Application code uses the opaque context/window handles and declarations in [mkgui.h](../mkgui.h). Build and deployment instructions are in [getting started](getting_started.md).

## Libraries and source organization

`build.sh` builds `libmkgui.so`/`mkgui.dll`, static toolkit archives, and the independent `mkguiicons` shared library. The demo and editor link the shared toolkit. Applications select their toolkit release through its pkg-config package, such as `mkgui1.0.0`; within a checkout, `-Lout/linux -lmkgui` selects the local library. Applications calling `mkguiicons_*()` directly also link the catalogue.

The toolkit's implementation compiles as one translation unit, `mkgui.c`, which includes the widget sources, mkwin backend, and embedded PlutoSVG/PlutoVG. This internal organization does not require applications to include implementation sources. Optional source-inclusion builds remain available.

| Source | Responsibility |
|---|---|
| `mkgui.h` | Public types, constants, and API |
| `mkgui_glview.h` | Public native-handle accessors for OpenGL child windows |
| `mkgui.c` | Context/window lifecycle, arenas, layout, dispatch tables, event loop |
| `mkgui_internal.h` | Private window, widget-state, and rendering structures |
| `mkgui_render.c` | Drawing primitives, text command queue, glyph rendering |
| `mkgui_icon.c` | Catalogue resolution, custom icons, SVG rasterization, cache |
| `mkgui_textedit.c`, `mkgui_undo.c` | Shared text-edit operations and undo history |
| `mkgui_<widget>.c` | Per-widget rendering, interaction, and public setters/getters |
| `mkgui_dialogs.c`, `mkgui_filedialog.c`, `mkgui_colorpicker.c`, `mkgui_iconbrowser.c` | Built-in dialogs |
| `mkwin/` | Pinned source submodule: native windows, fonts, events, framebuffers, drag-and-drop |
| `plutosvg/`, `plutovg/` | Embedded SVG parser and vector rasterizer |
| `mkgui_editor.h`, `mkgui_editor_api.c` | Private bridge used by the matching editor build |
| `mkguiicons.c`, `mkguiicons_data.S` | Independent SVG catalogue and lookup ABI |
| `tools/generate_icon_library.py` | Catalogue generation from the vendored Fluent snapshot |

`MKGUI_LIBRARY` exports public definitions when building the toolkit. ELF internals use hidden visibility; Windows uses explicit export/import declarations. `MKGUI_STATIC` selects static-archive linkage for consumers.

`mkgui.version` controls the complete Linux filename and SONAME, plus the installed header directory, archive, and pkg-config package. Each full release can coexist with every other release; there is no shared major-version runtime alias. `mkguiicons.version` versions the catalogue independently. The editor uses `mkgui_editor_api_v2()` for private drawing/layout access. Changes to the bridge's layout require a new bridge revision and a matching editor rebuild; this is separate from the public ABI.

PlutoVG font and image-file code is excluded through `PLUTOVG_NO_FONT` and `PLUTOVG_NO_IMAGE_IO`. Widget text uses the platform font backend. The toolkit does not need separate PlutoSVG/PlutoVG runtime libraries.

## Context and window state

One application context owns defaults, the quit flag, the primary window, and the window registry. Each window owns widget definitions, auxiliary state, layout rectangles, event state, timers, accelerators, popups, and damage tracking. mkwin owns native handles and framebuffer resources.

`struct mkgui_widget` is public and passed by value in arrays. It contains IDs, parent IDs, size hints, flags/style, weight, margins, cached label width, label text, and an icon name. It has no x/y coordinates. Computed rectangles are private window state.

Inputs, lists, trees, and other stateful widgets have per-type auxiliary arrays. `init_widget_aux()` initializes these during window creation or dynamic insertion. Widget removal releases owned storage and removes the corresponding auxiliary entry. The `find_*_data()` helpers are internal.

## Allocation and ownership

`vm_arena` reserves virtual address space and commits pages as arrays grow. Linux uses `mmap`, `mprotect`, and `munmap`; Windows uses `VirtualAlloc` and `VirtualFree`.

Window widget/auxiliary arrays and layout scratch have fixed reservations, bounded by the implementation's capacity constants. Growing them within those reservations preserves their base addresses. Removing widgets or auxiliary entries can still move entries within an array.

Icon metadata, icon hash slots, custom SVG records, and glyph pixels use growing reservations. A larger mapping can replace a full reservation; callers retain icon indices rather than pointers into the metadata. The editor bridge follows the current metadata pointer.

Layout scratch, queued text, fonts, and icon caches are process-private state used by windows in that process. Loading the shared library in another process creates independent writable state. Library code and immutable catalogue data can be shared between processes.

Textarea text, tree nodes, log storage, custom SVG text, and image/icon pixels have allocations appropriate to their individual lifetimes. These are released by their owning widgets or cache teardown. Last-window teardown releases shared caches; context destruction also releases icons preloaded before any window was created.

## Layout

`layout_build_index()` builds parent, first-child, next-sibling, and ID lookup tables. `lc_layout_node()` positions the tree, using `lc_measure_container()` and leaf natural sizes where needed. The results populate the private rectangle array. `lc_window_min_size()` computes the content minimum used by the runtime and editor.

The editor prepares runtime widget descriptions and calls the same layout functions through its private bridge. It has separate scratch arrays and disables runtime scrolling in the canvas preview. The designed window grows to the calculated content minimum.

The full sizing and alignment rules are in [layout.md](layout.md). Window chrome reserves space around the content; place a VBox, HBox, or other layout container in that content area to arrange ordinary children.

## Frame rendering and presentation

A dirty window is processed by the internal `mkgui_flush()`:

1. Compute layout and synchronize OpenGL child geometry.
2. Acquire a writable framebuffer through `mkgui_frame_begin()` and combine any repair damage.
3. Clear/render affected regions, including queued text.
4. Run the render callback, notifications, and tooltip rendering.
5. Submit current damage through `mkgui_frame_present()`.
6. Render/present dirty popups.

Linux main windows alternate two XShm images. Completion events protect images still being read by the server. Buffer acquisition updates the toolkit's pixel pointer before drawing. Windows uses a DIB section and GDI presentation. Popups have separate framebuffer resources.

An older buffer may need both current and previous damage redrawn. Repair regions are excluded from the submitted damage history, so repair does not expand future frames indefinitely. Structural changes request a full redraw; setters and interaction paths can mark individual widgets dirty.

Rendering uses per-type descriptor tables and primitives from `mkgui_render.c`. Glyph coverage is packed in an atlas; text commands are flushed into the active framebuffer. OpenGL child drawing belongs to the application and uses its own context.

## Events and loops

mkwin translates native events into `struct mkwin_event`, with `MKWIN_EV_*` kinds. `mkgui_window_poll()` handles timers, focus/hover transitions, and queued native events, returning public `struct mkgui_event` values. Per-type press, key, release, and click tables route widget interactions.

`mkgui_ctx_run()` provides the normal application loop. It dispatches primary-window events through the supplied callback and secondary-window events through their registered callbacks. It exits on quit, primary close, or when no windows are visible.

Host-owned loops use the public poll/wait API, with `mkgui_ctx_pump_others()` for registered secondary callbacks. `mkgui_ctx_wait_until()` flushes dirty windows and waits for an event or deadline. Its deadline is an absolute `mkgui_now_ns()` value.

`mkgui_window_close()` requests deferred destruction. `mkgui_ctx_reap_closed()` destroys flagged secondary windows at a point outside event polling; `mkgui_ctx_run()` calls it automatically. Direct `mkgui_window_destroy()` releases immediately.

Built-in modal dialogs run nested loops. They poll the dialog and keep their parent visually updated through the same rendering path. They do not run the application's ordinary event callbacks for the blocked parent.

## Icons

The catalogue library contains SVG bytes, names, source-size variants, and search metadata. It does not render. Name lookup selects a source design, then the toolkit rasterizes it at the requested pixel dimensions using PlutoSVG/PlutoVG.

The process caches rasterized icons by name and size. Monochrome SVG paint follows the current theme; colour icons retain their colours. Theme/scale changes refresh applicable cache entries. Explicit custom SVGs and ARGB pixels enter the same toolkit icon system.

Names are the persistent identity. Catalogue enumeration indices may change between compatible updates. See [icons.md](icons.md) for public lookup/enumeration contracts.

## Feature configuration

`mkgui_features.h` resolves `MKGUI_NO_<WIDGET>` exclusions into `MKGUI_HAS_<WIDGET>` constants. The default enables all widgets. `MKGUI_HAS_ALL` overrides exclusions. Public enum values and structure layouts remain the same across feature configurations.

Descriptor entries, auxiliary initialization/teardown, layout paths, and built-in dialogs are gated consistently. Disabled widget types are rejected during creation/insertion. Dialog availability also depends on the widget types needed to build it.

The editor emits a sibling `*_features.h` configuration. To specialize a source or static build, compile the implementation with that configuration. Defining exclusions in application code does not change an existing shared library or archive.

## Capacity constants

Public text, item, selection, and popup bounds are declared in `mkgui.h`. Window-count and auxiliary implementation details are in `mkgui_internal.h`; virtual reservations are defined in `mkgui.c`. Changing a consumer's copy of a constant cannot resize storage inside an already built shared library.

The editor's project capacity is `ED_MAX_WIDGETS` in `editor.c`. Icon cache storage grows on demand rather than using the former fixed metadata limits.
