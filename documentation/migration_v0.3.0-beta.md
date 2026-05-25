# Migrating to v0.3.0-beta

v0.3.0-beta is a breaking release. Two changes drive the breakage:

1. **`mkgui_ctx` is split into `mkgui_ctx` (application-level state) and `mkgui_window` (per-window state).** The previous "ctx" was structurally a window pretending to be a context.
2. **Public-API naming is now consistent: `mkgui_<subsystem>_<verb>`.** Subject first, verb second. This makes tab-completion useful (`mkgui_dialog_<TAB>` narrows to dialogs, `mkgui_window_<TAB>` to window operations, etc.) instead of dumping ~170 names.

Migration for a typical 100-line app: 10-15 minutes of mechanical search-and-replace plus one structural change to `main()`.

## Mental model

**Before:**
```
mkgui_ctx*  ==  one window + all the global state (theme, icons, X display)
```
Top-level window via `mkgui_create(widgets, count)`. Child/dialog windows via `mkgui_create_child(parent_ctx, ...)`. Theme, scale, app class, quit flag — all stored on each ctx, copied between parent and child.

**After:**
```
mkgui_ctx*     ==  application-level state (theme defaults, scale defaults,
                   app class, window registry, app-wide quit flag)
mkgui_window*  ==  per-window state (widget tree, events, framebuffer,
                   X11 window, per-window close flag, timers)
```
One ctx per process, created at startup with `mkgui_ctx_create()`. Windows live on that ctx, created via `mkgui_window_create(ctx, parent_or_NULL, widgets, count, title, w, h)`. The single `mkgui_window_create` replaces both `mkgui_create` (pass `parent=NULL`) and `mkgui_create_child` (pass `parent=parent_win`).

## Step 1: Rewrite main()

**Before:**
```c
int main(void) {
   struct mkgui_widget widgets[] = { ... };
   struct mkgui_ctx *ctx = mkgui_create(widgets, count);
   if(!ctx) return 1;
   mkgui_set_app_class(ctx, "myapp");
   mkgui_run(ctx, on_event, NULL);
   mkgui_destroy(ctx);
   return 0;
}
```

**After:**
```c
int main(void) {
   struct mkgui_widget widgets[] = { ... };
   struct mkgui_ctx *ctx = mkgui_ctx_create();
   if(!ctx) return 1;
   mkgui_ctx_set_app_class(ctx, "myapp");
   struct mkgui_window *win = mkgui_window_create(ctx, NULL, widgets, count, NULL, 0, 0);
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

The `NULL, 0, 0` at the end of `mkgui_window_create` mean "use the title and size from the MKGUI_WINDOW widget in `widgets[]`". Pass an explicit `"My Title", 800, 600` to override.

## Step 2: Rewrite callbacks

The callback parameter type changes from `struct mkgui_ctx *ctx` to `struct mkgui_window *win`. Inside callbacks, `win` is the window the event came from. If you need the application ctx, call `mkgui_window_get_ctx(win)`.

**Before:**
```c
static void on_event(struct mkgui_ctx *ctx, struct mkgui_event *ev, void *ud) {
   if(ev->type == MKGUI_EVENT_CLOSE) {
      mkgui_quit(ctx);
   }
   if(ev->type == MKGUI_EVENT_CLICK && ev->id == ID_BUTTON) {
      mkgui_label_set(ctx, ID_LABEL, "Clicked");
   }
}
```

**After:**
```c
static void on_event(struct mkgui_window *win, struct mkgui_event *ev, void *ud) {
   if(ev->type == MKGUI_EVENT_CLOSE) {
      mkgui_ctx_quit(mkgui_window_get_ctx(win));   // app-wide quit
   }
   if(ev->type == MKGUI_EVENT_CLICK && ev->id == ID_BUTTON) {
      mkgui_label_set(win, ID_LABEL, "Clicked");   // widget op, still per-window
   }
}
```

Widget operations (`mkgui_label_set`, `mkgui_button_set_text`, `mkgui_listview_setup`, every `mkgui_<widget>_*` function) still take a window — they operate on widgets which belong to a window. Only the genuinely application-level operations move to ctx.

## Step 3: Function renames

Bulk-rename every old call site to the new name. The full table:

### Type renames
| Old | New |
|---|---|
| `struct mkgui_ctx *` (window) | `struct mkgui_window *` |
| `ctx` (variable name, where it was a window) | `win` |

### Lifecycle
| Old | New |
|---|---|
| `mkgui_create(widgets, count)` | `mkgui_ctx_create()` + `mkgui_window_create(ctx, NULL, widgets, count, NULL, 0, 0)` |
| `mkgui_create_child(parent, widgets, count, title, w, h)` | `mkgui_window_create(parent_ctx, parent, widgets, count, title, w, h)` |
| `mkgui_destroy(win)` | `mkgui_window_destroy(win)` + `mkgui_ctx_destroy(ctx)` |
| `mkgui_destroy_child(win)` | `mkgui_window_destroy(win)` |

### Ctx-scoped (act on the whole app)
| Old | New |
|---|---|
| `mkgui_set_app_class(ctx, name)` | `mkgui_ctx_set_app_class(ctx, name)` |
| `mkgui_set_theme(ctx, theme)` | `mkgui_ctx_set_theme(ctx, theme)` |
| `mkgui_set_scale(ctx, scale)` | `mkgui_ctx_set_scale(ctx, scale)` |
| `mkgui_get_scale(ctx)` | `mkgui_ctx_get_scale(ctx)` |
| `mkgui_run(ctx, cb, ud)` | `mkgui_ctx_run(ctx, cb, ud)` |
| `mkgui_quit(ctx)` | `mkgui_ctx_quit(ctx)` |
| `mkgui_should_quit(ctx)` | `mkgui_ctx_should_quit(ctx)` |
| `mkgui_wait(ctx)` | `mkgui_ctx_wait(ctx)` |
| `mkgui_wait_until(ctx, deadline_ns)` | `mkgui_ctx_wait_until(ctx, deadline_ns)` |

### Window-scoped (act on one window)
| Old | New |
|---|---|
| `mkgui_set_title(win, title)` | `mkgui_window_set_title(win, title)` |
| `mkgui_set_callback(win, cb, ud)` | `mkgui_window_set_callback(win, cb, ud)` |
| `mkgui_set_window_instance(win, name)` | `mkgui_window_set_instance(win, name)` |
| `mkgui_set_window_icon(win, sizes, n)` | `mkgui_window_set_icon(win, sizes, n)` |
| `mkgui_invalidate(win)` | `mkgui_window_invalidate(win)` |
| `mkgui_poll(win, &ev)` | `mkgui_window_poll(win, &ev)` |
| `mkgui_add_timer(win, ns, cb, ud)` | `mkgui_window_add_timer(win, ns, cb, ud)` |
| `mkgui_remove_timer(win, id)` | `mkgui_window_remove_timer(win, id)` |

### Widget management
| Old | New |
|---|---|
| `mkgui_add_widget(win, w, after_id)` | `mkgui_widget_add(win, w, after_id)` |
| `mkgui_remove_widget(win, id)` | `mkgui_widget_remove(win, id)` |

### Dialogs
| Old | New |
|---|---|
| `mkgui_message_box(parent, title, msg, icon, btns)` | `mkgui_dialog_message(parent, title, msg, icon, btns)` |
| `mkgui_confirm_dialog(parent, title, msg)` | `mkgui_dialog_confirm(parent, title, msg)` |
| `mkgui_input_dialog(parent, title, prompt, default, out, sz)` | `mkgui_dialog_input(parent, title, prompt, default, out, sz)` |
| `mkgui_open_dialog(parent, opts)` | `mkgui_dialog_open(parent, opts)` |
| `mkgui_save_dialog(parent, opts)` | `mkgui_dialog_save(parent, opts)` |
| `mkgui_color_dialog(parent, init, &out)` | `mkgui_dialog_color(parent, init, &out)` |
| `mkgui_dialog_path(parent, index)` | `mkgui_dialog_get_path(parent, index)` |

### Internal field renames (if you accessed `ctx->close_requested` directly)
| Old | New |
|---|---|
| `ctx->close_requested` | `win->should_close` (use `mkgui_window_should_close(win)` instead) |

### Time and sleep
The ms/us time API is gone. Single nanosecond API. Convert at the call site for the rare cases you actually want ms/us.

| Old | New |
|---|---|
| `mkgui_time_ms()` | `(uint32_t)(mkgui_now_ns() / 1000000ull)` |
| `mkgui_time_us()` | `((double)mkgui_now_ns() * 1e-3)` |
| `mkgui_sleep_ms(ms)` | `mkgui_sleep_ns((uint64_t)(ms) * 1000000ull)` |

## Step 4: New API surface in v0.3.0-beta

These are additive — new capability that didn't exist in v0.2.x.

### Window-close cancellation
The X close button auto-sets `should_close` (same default as v0.2.x). New: you can cancel it from your event handler to refuse the close (e.g. unsaved-changes prompt):
```c
case MKGUI_EVENT_CLOSE: {
   uint32_t ans = mkgui_dialog_message(win, "Quit?",
                                       "Save changes before quitting?",
                                       MKGUI_DLG_ICON_QUESTION,
                                       MKGUI_DLG_BUTTONS_YES_NO_CANCEL);
   if(ans == MKGUI_DLG_RESULT_CANCEL) {
      mkgui_window_set_close(win, 0);   // cancel: keep window open
   } else if(ans == MKGUI_DLG_RESULT_YES) {
      save_work();
      mkgui_ctx_quit(mkgui_window_get_ctx(win));
   } else {
      mkgui_ctx_quit(mkgui_window_get_ctx(win));
   }
} break;
```

### App-wide quit cancellation
Symmetric: `mkgui_ctx_set_quit(ctx, 0)` clears the app-wide quit flag if you change your mind after calling `mkgui_ctx_quit(ctx)`.

### Host-owned main loop
Replaces the implicit `mkgui_ctx_run` with explicit pump-and-wait. For game-style loops or apps that need custom pacing:
```c
uint64_t next_tick = mkgui_now_ns() + 16000000ull;   // 60 Hz host work
while(!mkgui_ctx_should_quit(ctx)) {
   struct mkgui_event ev;
   while(mkgui_window_poll(win, &ev)) {
      on_event(win, &ev, NULL);
   }
   mkgui_ctx_pump_others(ctx);   // drain events from any non-primary windows

   uint64_t now = mkgui_now_ns();
   if(now >= next_tick) {
      run_one_frame();
      next_tick += 16000000ull;
   }

   mkgui_ctx_wait_until(ctx, next_tick);
}
```

The host pattern coexists with the library's own animation pacing: any animated widget (spinner, progress shimmer, GL view) keeps its own internal frame rate because `mkgui_ctx_wait_until` caps its wait whenever animation is active, regardless of how far away the caller's deadline is.

### Window getter for callbacks
`mkgui_window_get_ctx(win)` returns the owning ctx. Use it inside event callbacks (which receive `win`) to call ctx-scoped functions.

## Migration sed script

The bulk of the rename is mechanical. This will get your code 95% of the way there:

```bash
sed -i \
  -e 's/\bstruct mkgui_ctx \*/struct mkgui_window */g' \
  -e 's/\bmkgui_set_title\b/mkgui_window_set_title/g' \
  -e 's/\bmkgui_set_callback\b/mkgui_window_set_callback/g' \
  -e 's/\bmkgui_set_window_instance\b/mkgui_window_set_instance/g' \
  -e 's/\bmkgui_set_window_icon\b/mkgui_window_set_icon/g' \
  -e 's/\bmkgui_invalidate\b/mkgui_window_invalidate/g' \
  -e 's/\bmkgui_poll\b/mkgui_window_poll/g' \
  -e 's/\bmkgui_add_timer\b/mkgui_window_add_timer/g' \
  -e 's/\bmkgui_remove_timer\b/mkgui_window_remove_timer/g' \
  -e 's/\bmkgui_set_theme\b/mkgui_ctx_set_theme/g' \
  -e 's/\bmkgui_set_scale\b/mkgui_ctx_set_scale/g' \
  -e 's/\bmkgui_get_scale\b/mkgui_ctx_get_scale/g' \
  -e 's/\bmkgui_should_quit\b/mkgui_ctx_should_quit/g' \
  -e 's/\bmkgui_wait_until\b/mkgui_ctx_wait_until/g' \
  -e 's/\bmkgui_wait\b/mkgui_ctx_wait/g' \
  -e 's/\bmkgui_run\b/mkgui_ctx_run/g' \
  -e 's/\bmkgui_quit\b/mkgui_ctx_quit/g' \
  -e 's/\bmkgui_set_app_class\b/mkgui_ctx_set_app_class/g' \
  -e 's/\bmkgui_message_box\b/mkgui_dialog_message/g' \
  -e 's/\bmkgui_confirm_dialog\b/mkgui_dialog_confirm/g' \
  -e 's/\bmkgui_input_dialog\b/mkgui_dialog_input/g' \
  -e 's/\bmkgui_open_dialog\b/mkgui_dialog_open/g' \
  -e 's/\bmkgui_save_dialog\b/mkgui_dialog_save/g' \
  -e 's/\bmkgui_color_dialog\b/mkgui_dialog_color/g' \
  -e 's/\bmkgui_dialog_path\b/mkgui_dialog_get_path/g' \
  -e 's/\bmkgui_add_widget\b/mkgui_widget_add/g' \
  -e 's/\bmkgui_remove_widget\b/mkgui_widget_remove/g' \
  -e 's/\bmkgui_destroy_child\b/mkgui_window_destroy/g' \
  -e 's/\bclose_requested\b/should_close/g' \
  *.c *.h
```

The remaining 5%:
- `main()` needs the explicit `mkgui_ctx_create` + `mkgui_window_create` pair (replaces single `mkgui_create`), and the matching `mkgui_window_destroy` + `mkgui_ctx_destroy` at the end.
- `mkgui_create_child(parent, widgets, count, title, w, h)` becomes `mkgui_window_create(mkgui_window_get_ctx(parent), parent, widgets, count, title, w, h)`.
- Callback functions that called `mkgui_quit(ctx)` (where `ctx` was the window passed in): change to `mkgui_ctx_quit(mkgui_window_get_ctx(win))`.
