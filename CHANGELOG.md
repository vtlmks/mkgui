# Changelog

All notable changes to mkgui. Pre-1.0 betas may break the API between releases.

## v0.4.0-beta

### Added

- **`mkgui_window_close` / `mkgui_ctx_reap_closed` (deferred window destroy).** `mkgui_window_close` flags a window for destruction instead of freeing it immediately, so a window can safely tear itself down from inside its own event or timer callback (`mkgui_window_destroy` there is a use-after-free -- the poll/timer loop is still walking the window). The flagged windows are reaped by `mkgui_ctx_reap_closed`, which `mkgui_ctx_run` calls at the top of its loop; host-owned loops call it once per iteration outside their polls. Closing the primary window ends `mkgui_ctx_run`.
- **`mkgui_dialog_folder` (directory picker).** A third file-dialog mode alongside open/save: lists directories only, hides the filename/filter row, and confirms with "Select". Returns the highlighted subfolder or the folder currently being browsed when nothing is highlighted. Same `mkgui_file_dialog_opts` / `mkgui_dialog_get_path` surface as the other two.
- **`MKGUI_TREELIST` widget (columned tree / tree-table).** A listview whose rows nest into an expandable hierarchy, with the disclosure twisty and indentation in column 0 and full listview column machinery (per-column cell types, sortable/resizable/reorderable headers, multi-select, horizontal + vertical scroll). Retained node model with stable handle-based selection that survives expand/collapse; sorting is per-parent (siblings reorder within their parent, never across parents). Set up with `mkgui_treelist_setup()` and populate with `mkgui_treelist_add()`; see `documentation/doc.md` for the full API.
- **`GRIDVIEW_SELECT` and `LOGVIEW_LINE_CLICKED` events.** The gridview now emits a selection event and the logview a line-click event, dispatched through the same path as the other widget events.
- **Win32 platform test harness and broader event-injection coverage.** A mingw-built, wine-run test exercises the divergent Win32 windowing and message-translation surface; the Linux event-injection suite was extended to cover every event type plus payload and negative-path cases.

### Changed

- **Clipboard size limit removed.** Internal selection storage no longer has the fixed 4 KB cap; it grows on demand, so copy and paste handle arbitrarily large text. The Linux backend now implements the X11 INCR protocol on both sides (owner and requestor), streaming selections that exceed a single X request in chunks; Win32 already handled arbitrary sizes via the OS clipboard.
- **X11 framebuffer reused across resizes.** The XShm-backed framebuffer is grown in place on resize instead of being torn down and recreated, avoiding a shared-memory segment allocation on every resize step.

### Fixed

- **Timers on non-primary windows now fire when the loop is idle.** The idle wait only folded the primary (anchor) window's timer fds into `ppoll` (Linux) / bounded `MsgWaitForMultipleObjects` by its timers (Win32), so a timer on a transient window (e.g. a progress dialog's heartbeat) silently stopped firing once other event traffic went quiet, blocking the loop indefinitely. The wait now spans every window on the ctx, so app timers can live on whichever window owns them.
- **Editor: the right inspector panel scrolls when its property groups overflow.**
- **Editor: inactive-tab container outlines and their nested children are no longer drawn.**

## v0.3.0-beta

### Breaking

- **Split `mkgui_ctx` into `mkgui_ctx` (application context) and `mkgui_window` (per-window state).** The previous "ctx" type was structurally a window pretending to be a context. One ctx per process now owns any number of windows via `ctx->windows[]`. Each window has a back-pointer reachable via `mkgui_window_get_ctx(win)`.
- **Lifecycle API replaced.** `mkgui_create` / `mkgui_destroy` / `mkgui_create_child` / `mkgui_destroy_child` are gone. New:
  - `mkgui_ctx_create()` / `mkgui_ctx_destroy(ctx)` — one ctx per process.
  - `mkgui_window_create(ctx, parent_or_NULL, widgets, count, title, w, h)` — unified for top-level and transient windows. `parent=NULL` for top-level; non-NULL for transient/dialog (sets X11 transient_for / Win32 owner). `title=NULL` and `w=h=0` inherit from the `MKGUI_WINDOW` widget in `widgets[]`.
  - `mkgui_window_destroy(win)` — single destroy for any window.
  - `mkgui_ctx_destroy` auto-destroys any windows still alive on the ctx, so callers that forget the explicit destroy don't leak platform resources.
- **Public API renamed for `mkgui_<subsystem>_<verb>` consistency.** Tab-completion now narrows productively: `mkgui_dialog_<TAB>`, `mkgui_window_<TAB>`, `mkgui_ctx_<TAB>` each return a handful of names instead of dumping ~170.

  | Old | New |
  |---|---|
  | `mkgui_set_app_class` | `mkgui_ctx_set_app_class` |
  | `mkgui_set_theme` | `mkgui_ctx_set_theme` (now applies to all windows) |
  | `mkgui_set_scale` / `mkgui_get_scale` | `mkgui_ctx_set_scale` / `mkgui_ctx_get_scale` |
  | `mkgui_run` | `mkgui_ctx_run` |
  | `mkgui_quit` / `mkgui_should_quit` | `mkgui_ctx_quit` / `mkgui_ctx_should_quit` |
  | `mkgui_wait` / `mkgui_wait_until` | `mkgui_ctx_wait` / `mkgui_ctx_wait_until` |
  | `mkgui_set_title` | `mkgui_window_set_title` |
  | `mkgui_set_callback` | `mkgui_window_set_callback` |
  | `mkgui_set_window_instance` | `mkgui_window_set_instance` |
  | `mkgui_set_window_icon` | `mkgui_window_set_icon` |
  | `mkgui_invalidate` | `mkgui_window_invalidate` |
  | `mkgui_poll` | `mkgui_window_poll` |
  | `mkgui_add_timer` / `mkgui_remove_timer` | `mkgui_window_add_timer` / `mkgui_window_remove_timer` |
  | `mkgui_add_widget` / `mkgui_remove_widget` | `mkgui_widget_add` / `mkgui_widget_remove` |
  | `mkgui_message_box` | `mkgui_dialog_message` |
  | `mkgui_confirm_dialog` | `mkgui_dialog_confirm` |
  | `mkgui_input_dialog` | `mkgui_dialog_input` |
  | `mkgui_open_dialog` | `mkgui_dialog_open` |
  | `mkgui_save_dialog` | `mkgui_dialog_save` |
  | `mkgui_color_dialog` | `mkgui_dialog_color` |
  | `mkgui_dialog_path` | `mkgui_dialog_get_path` |

- **Type and field renames.** `struct mkgui_ctx *` → `struct mkgui_window *` everywhere it referred to a window. Variable name `ctx` → `win` everywhere it pointed to a window. Internal field `close_requested` → `should_close`.
- **Callback signatures.** Every `mkgui_*_cb` callback's first parameter changed from `struct mkgui_ctx *` to `struct mkgui_window *`. Use `mkgui_window_get_ctx(win)` inside callbacks when you need to call a ctx-scoped function (`mkgui_ctx_quit`, etc.).

### Added

- **Host-owned main loop.** For game-style loops or custom pacing:
  - `mkgui_now_ns()` — monotonic clock in nanoseconds (`uint64_t`).
  - `mkgui_ctx_wait_until(ctx, deadline_ns)` — blocks until an event, an animation step, a tooltip/toast deadline, or the caller's absolute ns deadline, whichever fires first.
  - `mkgui_ctx_wait(ctx)` — convenience wrapper for `mkgui_ctx_wait_until(ctx, 0)` (no caller deadline).
  - `mkgui_window_poll(win, &ev)` — non-blocking event drain for a single window.
  - `mkgui_ctx_pump_others(ctx)` — drains every non-primary window's events through their registered callbacks (mirrors what `mkgui_ctx_run` does internally).
- **Per-window close cancellation.** X close button auto-sets `should_close` (same default as v0.2.x). New API to cancel from the event handler:
  - `mkgui_window_set_close(win, value)` — explicit setter, clears or sets the flag.
  - `mkgui_window_should_close(win)` — query.
- **Ctx-level quit cancellation.** Symmetric:
  - `mkgui_ctx_set_quit(ctx, value)` — clear or set the app-wide quit flag.
- **Window-to-ctx accessor.**
  - `mkgui_window_get_ctx(win)` — returns the owning ctx, for use inside callbacks.

### Changed

- **Platform wait timeout switched from `int32_t` ms to `int64_t` ns.** Internal `platform_wait_event` now takes nanoseconds; Linux uses `ppoll` for ns precision, Windows ceils to ms (sub-ms not available via `MsgWaitForMultipleObjects`).
- **`mkgui_ctx_set_theme` and `mkgui_ctx_set_scale` apply to all windows on the ctx.** Previously each window held its own theme/scale; new ctx-level setters iterate and update every window in the registry.
- **`mkgui_window_create` returns `NULL` when the window registry is full** (`MKGUI_MAX_WINDOWS` = 16). Previously it would silently create an untracked window.
- **Single nanosecond-only time API.** `mkgui_time_ms()`, `mkgui_time_us()`, and `mkgui_sleep_ms()` are gone. Use `mkgui_now_ns()` (monotonic ns, `uint64_t`) and `mkgui_sleep_ns(ns)`. Convert to other units at the call site: `(uint32_t)(mkgui_now_ns() / 1000000ull)` for ms, `(double)mkgui_now_ns() * 1e-3` for us. One time unit, one sleep function, no choice paralysis.

### Migration

See [`documentation/migration_v0.3.0-beta.md`](documentation/migration_v0.3.0-beta.md) for the full rename table, code transformation examples, and a sed script that handles ~95% of a typical migration mechanically. The remaining 5% (lifecycle pair in `main()`, child window creation, callbacks calling ctx-scoped functions) needs hand-fixing per the patterns documented.

## v0.2.0-beta

See git history.

## v0.1.0-beta

See git history.
