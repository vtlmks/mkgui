# mkgui Layout Engine

This document describes how mkgui arranges widgets on screen: the rules the
engine follows, the values it uses, and where those values come from. It is
aimed at anyone writing a new widget or trying to predict how an existing
widget will size itself inside a container.

All layout code lives in [mkgui.c](../mkgui.c), in the functions prefixed
`lc_`, `layout_`, `measure_container`, `natural_height`, `natural_width`.

---

## 1. Model

mkgui is a **pure container-based, single-pass tree layout**. There are no
anchor flags, no absolute coordinates on widgets, no constraints. Every
widget's rect is produced by its parent.

Widgets store only:

- `w`, `h` -- optional size hints (0 means "let the engine decide")
- `weight` -- flex share for expandable axes (0 treated as 1)
- `margin_l/r/t/b` -- outer margins (only applied by the generic-container fallback)
- `flags` -- `MKGUI_FIXED`, `MKGUI_HIDDEN`, `MKGUI_SCROLL`, `MKGUI_NO_PAD`,
  `MKGUI_ALIGN_{START,CENTER,END}`, `MKGUI_REGION_{TOP,BOTTOM,LEFT,RIGHT}`
- `style` -- per-widget-type style bits

There are no `x`/`y` fields on widgets. Positions live in a parallel
`ctx->rects[]` array, indexed identically to `ctx->widgets[]`, and are
rewritten every layout pass.

## 2. Entry point and pass order

A layout pass is driven from `layout_widgets` in [mkgui.c](../mkgui.c):

1. **`layout_build_index`** -- builds `parent[]`, `first_child[]`,
   `next_sibling[]` arrays plus a hash table (`layout_id_map`) for O(1)
   ID-to-index lookup. Iterates widgets in reverse so child lists are in
   declaration order.
2. For each top-level `MKGUI_WINDOW`:
    - Set its rect to `(0, 0, ctx->win_w, ctx->win_h)`.
    - Call **`layout_node`** (itself a thin wrapper over `lc_layout_node`)
      which recursively positions every descendant.
    - If `ctx->dirty_full`, call **`layout_compute_min_size`** to push a
      minimum window size to the platform layer.

Everything after the index build is a single recursive walk. There is no
separate measure pass and no fixpoint; measurement happens on demand inside
`lc_layout_node` via `lc_measure_container`.

## 3. Shared layout context

Both the runtime and the editor call the same layout code through a
`struct layout_ctx`. The context abstracts:

- `widgets` + `widget_stride` -- editor uses a bigger struct, `lw_get`
  multiplies by stride.
- `rects` -- output array.
- `first_child`, `next_sibling`, `parent`, `id_map`, `hash_mask` -- the tree
  index.
- `has_scroll` -- the runtime honours `MKGUI_SCROLL`; the editor ignores it.

`lw_sw(ctx, v)` scales positive values through `sc()` (DPI scale) and
passes non-positive values through as-is so that "0 means auto" is
preserved.

## 4. Container kinds

The engine recognises exactly these container widgets and handles them
specially in `lc_layout_node`:

| Widget                         | Axis / role                                    |
|--------------------------------|------------------------------------------------|
| `MKGUI_WINDOW`                 | Places chrome (menu/toolbar/pathbar/statusbar), then one body region |
| `MKGUI_VBOX`, `MKGUI_PANEL`    | Vertical flex                                  |
| `MKGUI_HBOX`                   | Horizontal flex                                |
| `MKGUI_FORM`                   | Two-column label/control grid                  |
| `MKGUI_GROUP`                  | Bordered titled box, holds exactly one child container |
| `MKGUI_TABS`, `MKGUI_TAB`      | Tab strip + per-tab body                       |
| `MKGUI_HSPLIT`, `MKGUI_VSPLIT` | Two-child ratio split                          |
| `MKGUI_TOOLBAR`                | Children laid out by `render_toolbar`, not the engine |

Everything else is a leaf, and any other container node not in this list
falls into a generic "fill parent minus margins, optionally fixed" path.

## 5. VBOX / PANEL rules

`MKGUI_PANEL` is treated as a vertical box throughout layout.

**Pass 1 -- classify children and sum minimums**:

- Skip `MKGUI_HIDDEN`.
- **Fixed** children are the union of three cases, decided by
  `vbox_child_is_fixed`:
    - the `MKGUI_FIXED` flag is set, or
    - the widget is a collapsible `MKGUI_GROUP`, or
    - the widget is a **single-line leaf** (input, button, label,
      checkbox, radio, toggle, dropdown, combobox, spinbox, datepicker,
      ipinput, slider, scrollbar, progress, meter, divider, spinner) and
      `weight == 0`.

  Their height is taken from `w->h` (scaled), falling back to
  `lc_measure_container` for containers or `natural_height` for leaves,
  and added to `fixed_total`. Collapsed groups are clamped to
  `ctx->font_height + 4` (header only).
- **Flexible** children: same resolution used as a **minimum** added to
  `min_total`; their `weight` (or 1) added to `weight_total`.

The single-line-leaf rule matches Qt/GTK size-policy conventions: a
`QLineEdit` / `GtkEntry` holds its natural height and does not stretch
unless the programmer asks for it. To opt a single-line widget back into
flex behaviour, set `weight > 0` explicitly
(`mkgui_set_weight(id, 1)`).

The per-type classification lives in `widget_vflex_default` in
[mkgui.c](../mkgui.c). Multi-line leaves (textarea, listview, gridview,
richlist, treeview, itemview, image, canvas, glview) and all containers
return 1 (expand by default); single-line leaves return 0.

**Pass 2 -- scrollbar and remainder**:

- `gap_total = (visible_count - 1) * ctx->box_gap`
- `content_h = fixed_total + min_total + gap_total`
- If the box has `MKGUI_SCROLL` and `content_h > ph`, steal
  `ctx->scrollbar_w` from the **inner width** and clamp remaining to 0.
- `remaining = ph - fixed_total - min_total - gap_total`

**Pass 3 -- place children top-to-bottom**:

- Fixed children get their pre-computed height verbatim.
- Flex children get `base_h + remaining * wt / weight_total` (64-bit
  intermediate to avoid overflow). Flex children are then **floored** at
  their content minimum so weights cannot collapse them below usable size.
- Horizontal placement honours `MKGUI_ALIGN_{START,CENTER,END}` when a
  non-zero explicit `w` exists; otherwise the child fills the inner width.
- Advance `cy += ch + ctx->box_gap`.

## 6. HBOX rules

Mirror of VBOX for the main (horizontal) axis: flex distribution by
weight, `MKGUI_FIXED` children take their declared width, etc. Single-line
widgets **do** expand horizontally by default because filling form-row
width is usually what you want.

Cross-axis (height) placement uses `widget_vflex_default` too:

- Widgets with explicit `h` and `MKGUI_ALIGN_*` are placed at that height
  with the requested alignment (top/center/bottom).
- Single-line leaves (anything for which `widget_vflex_default` returns 0)
  take their natural height (or explicit `h` if set) and **center
  vertically** by default. An OK/Cancel button row in a taller HBOX sits
  in the middle instead of stretching to fill.
- Multi-line leaves and containers fill the full HBOX height.

Scrolling steals from the inner **height**.

## 7. FORM rules

`MKGUI_FORM` is a two-column grid of `(label, control)` pairs in declaration
order.

- Column 1 width = `max(label_text_width + 8)` over all even-indexed children.
- Column 2 width = `pw - col1`.
- Row height = `ctx->font_height + 10`.
- Rows separated by `ctx->box_gap`.
- A trailing odd child (no control) just sits in column 1 of its row.

## 8. GROUP rules

`MKGUI_GROUP` is a bordered titled frame that contains **one** inner
container (VBOX/HBOX/FORM/PANEL). It reserves:

- `gtop = ctx->font_height + 4` -- title strip at the top.
- `gpad = ctx->box_pad` -- symmetric inset on left, right, bottom.

Collapsed groups (`MKGUI_GROUP_COLLAPSED`) early-out with no child layout
and a measured height of just `gtop`.

## 9. TABS / TAB rules

The layout engine does not pick the active tab; it lays out **every** tab
into the same rect. `MKGUI_TAB` insets by `(2, tab_height+2, 4, tab_height+4)`
on `(x, y, w, h)` to leave room for the tab strip and a border. Which tab
is drawn is decided at render time.

## 10. SPLIT rules

`MKGUI_HSPLIT` (horizontal splitter -- children stacked vertically) and
`MKGUI_VSPLIT` (vertical splitter -- children side-by-side).

- Ratio comes from `find_split_data(ctx, w->id)->ratio`, default 0.5.
- Children are matched by `MKGUI_REGION_TOP/BOTTOM` (HSPLIT) or
  `MKGUI_REGION_LEFT/RIGHT` (VSPLIT).
- Divider takes `ctx->split_thick` pixels out of the second region's
  leading edge.

## 11. WINDOW chrome

Window layout is a special case. Before any body child is positioned,
chrome children are pulled out and placed around the edges in this order:

1. **Menu** (`MKGUI_MENU`) -- full width, top, height `ctx->menu_height`.
2. **Toolbar** (`MKGUI_TOOLBAR`) -- full width, below menu. Height depends
   on mode:
    - `TEXT_ONLY` -- `ctx->font_height + 10`
    - `ICONS_ONLY` (with icons) -- `icon_size + 10`
    - mixed -- `max(icon_size, font_height) + 10`
    - no icons available -- `ctx->font_height + 10`
3. **Pathbar** (`MKGUI_PATHBAR`, only if `h == 0`) -- full width minus
   L/R margins, height `ctx->pathbar_height`.
4. **Statusbar** (`MKGUI_STATUSBAR`) -- full width, bottom, height
   `ctx->statusbar_height`.

Whatever remains is the body region passed down to the rest of the tree.
Widgets bigger than `chrome[16]` are silently dropped, so a window cannot
hold more than 16 chrome widgets.

## 12. Generic-container fallback

Any container that is not one of the kinds above:

- Each child rect is set to parent-minus-margins.
- If `MKGUI_FIXED`, the child's own `w`/`h` override, falling back to
  container measurement or `natural_{width,height}`.

This is also the only path that honours `margin_l/r/t/b`. Children inside
VBOX/HBOX/FORM/GROUP/TABS **do not** see their margins applied -- that is
by design, but it surprises people.

## 13. Padding rule

Padding (`ctx->box_pad` on each side) is applied at most **once** per
nesting chain:

- A VBOX/HBOX/FORM/PANEL gets padding unless its parent is also a
  VBOX/HBOX/FORM/GROUP/TABS/PANEL.
- `STYLE_BORDER_BIT` on the style field forces padding regardless of
  nesting.
- `MKGUI_NO_PAD` on the flags field forces no padding regardless.

`lc_measure_container` uses the exact same predicate so measurement and
placement agree.

## 14. Measurement (`lc_measure_container`)

Returns the intrinsic size of a container along one axis. Used whenever a
child declared `w == 0` or `h == 0` and the parent needs a number.

Algorithm:

- GROUP: delegate to the inner container, add `gtop + gpad` (vertical) or
  `gpad*2` (horizontal).
- FORM: height = `rows * (font_height+10) + (rows-1) * box_gap`. Width is
  not measured here -- FORM always fills its main-axis parent.
- VBOX/HBOX/PANEL:
    - `main = sum(child_main) + (visible-1) * box_gap`
    - `cross = max(child_cross)`
    - Child mains/crosses come from explicit `w/h`, then recursive
      measurement for containers, then `natural_height` / `natural_width`
      for leaves.
    - Add `box_pad*2` to each axis if the padding rule applies.

## 15. Natural sizes (leaf defaults)

`natural_height` and `natural_width` are the fallback when a leaf widget
did not declare a size. Everything here is run through `sc()` so it scales
with DPI. The numeric values come from the `MKGUI_NAT_*` defines in
[mkgui.h](../mkgui.h) -- override them at consumer build time if the
defaults do not suit your app.

| Widget                                                          | Natural height                                |
|-----------------------------------------------------------------|-----------------------------------------------|
| BUTTON                                                          | `font_height + MKGUI_NAT_BUTTON_VPAD` (12)    |
| INPUT, COMBOBOX, DROPDOWN, SPINBOX, DATEPICKER, IPINPUT, PATHBAR | `font_height + MKGUI_NAT_INPUT_VPAD` (10)    |
| CHECKBOX, RADIO, TOGGLE                                         | `font_height + MKGUI_NAT_CHECKBOX_VPAD` (6)   |
| LABEL                                                           | `font_height + MKGUI_NAT_LABEL_VPAD` (4)      |
| SLIDER, SCROLLBAR                                               | `MKGUI_NAT_SLIDER_H` (24)                     |
| PROGRESS, METER                                                 | `MKGUI_NAT_PROGRESS_H` (20)                   |
| DIVIDER                                                         | `MKGUI_NAT_DIVIDER_H` (6)                     |
| LISTVIEW, GRIDVIEW, RICHLIST, TREEVIEW, ITEMVIEW, TEXTAREA      | `row_height * 3`                              |
| TABS                                                            | `tab_height + row_height * 2`                 |
| HSPLIT, VSPLIT                                                  | `split_min_px*2 + split_thick`                |
| IMAGE, CANVAS, GLVIEW                                           | `MKGUI_NAT_CANVAS_H` (24)                     |
| VBOX, HBOX, FORM, PANEL, GROUP                                  | `row_height`                                  |

| Widget            | Natural width                                                                         |
|-------------------|---------------------------------------------------------------------------------------|
| BUTTON            | `text_width + (icon_size + MKGUI_NAT_BUTTON_ICON_GAP if icon) + MKGUI_NAT_BUTTON_HPAD` |
| LABEL             | `text_width + MKGUI_NAT_LABEL_HPAD` (4)                                               |
| CHECKBOX, RADIO   | `MKGUI_NAT_CHECKBOX_BOX_W + text_width` (22)                                          |
| TOGGLE            | `MKGUI_NAT_TOGGLE_BOX_W + text_width` (44)                                            |
| DIVIDER           | `MKGUI_NAT_DIVIDER_W` (4)                                                             |
| anything else     | `MKGUI_NAT_DEFAULT_W` (40)                                                            |

## 16. Minimum window size (`layout_min_size`)

Recursive lower bound for the OS-level minimum window size. Scrollable
containers bottom-out at `(100, row_height * 3)` because they scroll
rather than forcing content to fit. Splits recursively sum the
appropriate axis. Leaves use their explicit size or
`natural_{width,height}`. Containers delegate back to `measure_container`
so there is exactly one source of truth.

The final min is clamped at a **floor of `(200, 100)` pixels** before
`box_pad*2` is added on each axis. This floor is not scaled.

## 17. Global values (metric constants)

Defined in [mkgui.h](../mkgui.h). All are scaled into `ctx` fields by
`mkgui_recompute_metrics`. Consumers reading layout should always use the
`ctx->` field, not the `MKGUI_*` constant.

| Constant                        | Default | `ctx` field          |
|---------------------------------|---------|----------------------|
| `MKGUI_ROW_HEIGHT`              | 20      | `row_height`         |
| `MKGUI_TAB_HEIGHT`              | 26      | `tab_height`         |
| `MKGUI_MENU_HEIGHT`             | 22      | `menu_height`        |
| `MKGUI_SCROLLBAR_W`             | 14      | `scrollbar_w`        |
| `MKGUI_SPLIT_THICK`             | 5       | `split_thick`        |
| `MKGUI_SPLIT_MIN_PX`            | 50      | `split_min_px`       |
| `MKGUI_BOX_GAP`                 | 6       | `box_gap`            |
| `MKGUI_BOX_PAD`                 | 6       | `box_pad`            |
| `MKGUI_ICON_SIZE`               | 18      | `icon_size`          |
| `MKGUI_PATHBAR_HEIGHT`          | 24      | `pathbar_height`     |
| `MKGUI_TOOLBAR_HEIGHT_DEFAULT`  | 28      | `toolbar_height`     |
| `MKGUI_TOOLBAR_BTN_W`           | 28      | `toolbar_btn_w`      |
| `MKGUI_TOOLBAR_SEP_W`           | 8       | `toolbar_sep_w`      |
| `MKGUI_STATUSBAR_HEIGHT`        | 22      | `statusbar_height`   |
| `MKGUI_TAB_INSET`               | 2       | (used via `sc()`)    |
| `MKGUI_WIN_MIN_W`               | 200     | (used via `sc()`)    |
| `MKGUI_WIN_MIN_H`               | 100     | (used via `sc()`)    |

## 18. Writing a widget that plays nicely

Checklist for a new widget:

1. Add a case to `natural_height` (required if the widget can be a leaf
   child of a VBOX/HBOX/FORM without an explicit `h`).
2. Add a case to `natural_width` if the widget has an intrinsic width
   different from the `MKGUI_NAT_DEFAULT_W` fallback.
3. Classify the widget in `widget_vflex_default`: return 0 if it is a
   single-line control that should hold its natural height in a VBOX,
   return 1 if it should expand to fill leftover space. The default for
   anything not listed is 1, so you only need to add a case for
   single-line leaves.
4. If the widget is a **container**, decide whether to handle it in
   `lc_layout_node` (custom rules) or let the generic fallback distribute
   full-parent rects. Also add it to the padding predicate if it should
   suppress nested children's padding.
5. Use `sc(ctx, N)` for every pixel constant so DPI scaling is automatic.
   Never write raw px.
6. Use `ctx->box_gap` / `ctx->box_pad` / `ctx->row_height` etc. -- do not
   reference `MKGUI_BOX_GAP` etc. at runtime.
7. If your widget has its own natural-size literals that might be tweaked
   or overridden by consumers, add them to the `MKGUI_NAT_*` block in
   [mkgui.h](../mkgui.h) rather than inlining the number.
8. If the widget wants its children's margins honoured, make sure the
   layout path for it runs the generic fallback (or copy the
   `margin_l/r/t/b` handling explicitly).
9. If the widget owns sub-rects not matching any child widget (like
   toolbar button slots), compute and write them yourself during render,
   following the pattern in [mkgui_toolbar.c](../mkgui_toolbar.c).

## 19. Known hardcoded values in layout code

Magic numbers that are **not** routed through `MKGUI_*` defines. All of
these mix a scaled font metric with a literal pixel offset, so they
honour font scaling but not DPI scaling. The effect is small but causes
relative padding to drift slightly at high DPI.

| Location                        | Value                    | Role                                   |
|---------------------------------|--------------------------|----------------------------------------|
| `lc_measure_container` (GROUP)  | `font_height + 4`        | GROUP title strip height (`gtop`)      |
| `lc_layout_node` (GROUP)        | `font_height + 4`        | GROUP title strip height (`gtop`)      |
| VBOX fixed-group branch         | `font_height + 4`        | Collapsed GROUP height                 |
| FORM measure / layout           | `font_height + 10`       | FORM row height                        |
| WINDOW chrome / min size        | `font_height + 10`       | Toolbar row height                     |
| Toolbar with icons              | `+ 10`                   | Toolbar height padding                 |
| FORM label column               | `+ 8`                    | Label column padding                   |

These remain because `font_height` already dominates the expression; the
literal pixel offset is small next to the scaled font metric. Splitting
them into `MKGUI_*` defines would be a readability improvement, not a
correctness fix.

Values that used to be on this list and are now scaled via sc():

- `MKGUI_TAB_INSET` (2) -- MKGUI_TAB page inset and TABS min-size border.
- `MKGUI_WIN_MIN_W`, `MKGUI_WIN_MIN_H` (200, 100) -- window min-size floor.
