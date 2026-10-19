# mkgui Layout Engine

This document describes how mkgui arranges widgets on screen: the rules the
engine follows, the values it uses, and where those values come from. It is
aimed at anyone writing a new widget or trying to predict how an existing
widget will size itself inside a container.

All layout code lives in [mkgui.c](../mkgui.c), in the functions prefixed
`lc_`, `layout_`, `natural_height`, `natural_width`.

---

## 1. Model

mkgui uses **container-based tree layout with recursive measurement**. There are no
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
`win->rects[]` array, indexed identically to `win->widgets[]`, and are
rewritten every layout pass.

## 2. Entry point and pass order

A layout pass is driven from `layout_widgets` in [mkgui.c](../mkgui.c):

1. **`layout_build_index`** -- builds `parent[]`, `first_child[]`,
   `next_sibling[]` arrays plus a hash table (`layout_id_map`) for O(1)
   ID-to-index lookup. It links siblings in declaration order.
2. For each top-level `MKGUI_WINDOW`:
    - Set its rect to `(0, 0, win->win_w, win->win_h)`.
    - Call **`layout_node`** (itself a thin wrapper over `lc_layout_node`)
      which recursively positions every descendant.
    - If `win->dirty_full` and this is not a canvas-only window, call **`layout_compute_min_size`** to push a minimum window size to the platform layer.

Everything after the index build is a single recursive walk. There is no
separate measure pass and no fixpoint; measurement happens on demand inside
`lc_layout_node` via `lc_measure_container`.

## 3. Shared layout context

Both the runtime and the editor call the same internal layout code through a
`struct layout_ctx`. Application code uses public widget definitions and geometry accessors; the `win->` fields below describe implementation code. The context abstracts:

- `widgets` + `widget_stride` -- editor uses a bigger struct, `lw_get`
  multiplies by stride.
- `rects` -- output array.
- `first_child`, `next_sibling`, `parent`, `id_map`, `hash_mask` -- the tree
  index.
- `has_scroll` -- the runtime honours `MKGUI_SCROLL`; the editor ignores it.

`lw_sw(win, v)` scales positive values through `sc()` (DPI scale) and
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
| `MKGUI_GROUP`                  | Bordered titled box, insets a child layout container |
| `MKGUI_TABS`, `MKGUI_TAB`      | Tab strip + per-tab body                       |
| `MKGUI_HSPLIT`, `MKGUI_VSPLIT` | Two-child ratio split                          |
| `MKGUI_TOOLBAR`                | Children laid out by `render_toolbar`, not the engine |

Everything else is a leaf, and any other container node not in this list
falls into a generic "fill parent minus margins, optionally fixed" path.

## 5. VBOX / PANEL rules

`MKGUI_PANEL` uses vertical box placement. Runtime scroll state is allocated for VBox/HBox; a Panel does not get that state.

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
  `win->font_height + 4` (header only).
- **Flexible** children: same resolution used as a **minimum** added to
  `min_total`; their `weight` (or 1) added to `weight_total`.

The single-line-leaf rule matches Qt/GTK size-policy conventions: a
`QLineEdit` / `GtkEntry` holds its natural height and does not stretch
unless the programmer asks for it. To opt a single-line widget back into
flex behaviour, set `weight > 0` explicitly
(`mkgui_set_weight(win, id, 1)`).

The per-type classification lives in `widget_vflex_default` in
[mkgui.c](../mkgui.c). Multi-line leaves (textarea, listview, gridview,
richlist, treeview, itemview, image, canvas, glview) and all containers
return 1 (expand by default); single-line leaves return 0. This classification comes from `widget_desc[].fixed_h`, rather than a switch in `widget_vflex_default()`.

**Pass 2 -- scrollbar and remainder**:

- `gap_total = (visible_count - 1) * win->box_gap`
- `content_h = fixed_total + min_total + gap_total`
- If the box has `MKGUI_SCROLL` and `content_h > ph`, steal
  `win->scrollbar_w` from the **inner width** and clamp remaining to 0.
- `remaining = ph - fixed_total - min_total - gap_total`

**Pass 3 -- place children top-to-bottom**:

- Fixed children get their pre-computed height verbatim.
- Flex children get `base_h + remaining * wt / weight_total` (64-bit
  intermediate to avoid overflow). Flex children are then **floored** at
  their content minimum so weights cannot collapse them below usable size.
- Horizontal placement honours `MKGUI_ALIGN_{START,CENTER,END}` when a
  non-zero explicit `w` exists; otherwise the child fills the inner width.
- Advance `cy += ch + win->box_gap`.

## 6. HBOX rules

Mirror of VBOX for the main (horizontal) axis: flex distribution by
weight, `MKGUI_FIXED` children take their declared width, etc. Single-line
widgets **do** expand horizontally by default because filling form-row
width is usually what you want.

Cross-axis (height) placement fills the available height by default, including
buttons, spinners, and other single-line controls:

- Widgets with explicit `h` and `MKGUI_ALIGN_*` are placed at that height
  with the requested alignment (top/center/bottom).
- A child with an explicit `h` takes that height and centers vertically unless
  alignment requests another position. Fixed single-line controls with `h=0`
  use their natural height.
- Containers and other vertically flexible widgets with `h=0` fill the HBOX
  height, including when `MKGUI_FIXED` sets their width. Controls with `h=0`
  and no `MKGUI_FIXED` also fill the height. A compact button row can give
  the HBOX itself a fixed height.

Scrolling steals from the inner **height**.

## 7. FORM rules

`MKGUI_FORM` is a two-column grid of `(label, control)` pairs in declaration
order.

- Column 1 width = `max(label_text_width + 8)` over all even-indexed children.
- Column 2 width = `pw - col1`.
- Row height = `win->font_height + 10`.
- Rows separated by `win->box_gap`.
- A trailing odd child (no control) just sits in column 1 of its row.

## 8. GROUP rules

`MKGUI_GROUP` is a bordered titled frame that contains **one** inner
container (VBOX/HBOX/FORM/PANEL). It reserves:

- `gtop = win->font_height + 4` -- title strip at the top.
- `gpad = win->box_pad` -- symmetric inset on left, right, bottom.

Collapsed groups (`MKGUI_GROUP_COLLAPSED`) early-out with no child layout
and a measured height of just `gtop`.

## 9. TABS / TAB rules

The layout engine does not pick the active tab; it lays out **every** tab
into the same rect. With `inset = sc(win, MKGUI_TAB_INSET)`, a TAB adds `(inset, tab_height+inset)` to `(x, y)` and subtracts `(2*inset, tab_height+2*inset)` from `(w, h)` for the tab strip and border. Which tab
is drawn is decided at render time.

## 10. SPLIT rules

`MKGUI_HSPLIT` (horizontal splitter -- children stacked vertically) and
`MKGUI_VSPLIT` (vertical splitter -- children side-by-side).

- Ratio comes from `find_split_data(win, w->id)->ratio`, default 0.5.
- Children are matched by `MKGUI_REGION_TOP/BOTTOM` (HSPLIT) or
  `MKGUI_REGION_LEFT/RIGHT` (VSPLIT).
- Divider takes `win->split_thick` pixels out of the second region's
  leading edge.

## 11. WINDOW chrome

Window layout is a special case. Before any body child is positioned,
chrome children are pulled out and placed around the edges in this order:

1. **Menu** (`MKGUI_MENU`) -- full width, top, height `win->menu_height`.
2. **Toolbar** (`MKGUI_TOOLBAR`) -- full width, below menu. Height depends
   on mode:
    - `TEXT_ONLY` -- `win->font_height + 10`
    - `ICONS_ONLY` (with icons) -- `icon_size + 10`
    - mixed -- `max(icon_size, font_height) + 10`
    - no icons available -- `win->font_height + 10`
3. **Pathbar** (`MKGUI_PATHBAR`, only if `h == 0`) -- full width minus
   L/R margins, height `win->pathbar_height`.
4. **Statusbar** (`MKGUI_STATUSBAR`) -- full width, bottom, height
   `win->statusbar_height`.

Whatever remains is the body region passed down to the rest of the tree.
The window chrome collection has 16 slots; additional chrome children are not assigned by this path. Use one menu, toolbar, pathbar, and statusbar per window.

## 12. Generic-container fallback

Any container that is not one of the kinds above:

- Each child rect is set to parent-minus-margins.
- If `MKGUI_FIXED`, the child's own `w`/`h` override, falling back to
  container measurement or `natural_{width,height}`.

VBOX/HBOX/FORM placement does not apply child margins. The generic fill path, including children of WINDOW, GROUP, TABS, and TAB, applies margins; the window pathbar also uses its horizontal margins.

## 13. Padding rule

Box placement applies `win->box_pad` on each side according to the parent type:

- A VBOX/HBOX/FORM/PANEL gets padding unless its parent is also a
  VBOX/HBOX/FORM/GROUP/TABS/PANEL.
- `STYLE_BORDER_BIT` on the style field forces padding regardless of
  nesting.
- `MKGUI_NO_PAD` on the flags field forces no padding regardless.

Bordered boxes request padding even when nested. `MKGUI_NO_PAD` overrides that request. The current measurement helper suppresses nested padding under VBOX/HBOX/FORM/GROUP/TABS; placement also suppresses it under PANEL.

## 14. Measurement (`lc_measure_container`)

Returns the intrinsic size of a container along one axis. Used whenever a
child declared `w == 0` or `h == 0` and the parent needs a number.

Algorithm:

- GROUP: delegate to the inner container, add `gtop + gpad` (vertical) or
  `gpad*2` (horizontal).
- FORM: returns `rows * (font_height+10) + max(rows-1, 0) * box_gap`. The current helper returns that row-stack extent for either axis; it does not compute an intrinsic label/control width.
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
[mkgui.h](../mkgui.h). To change these compile-time metrics, edit their definitions and rebuild the toolkit. Definitions in an application do not change an existing shared library.

| Widget                                                          | Natural height                                |
|-----------------------------------------------------------------|-----------------------------------------------|
| BUTTON                                                          | `font_height + MKGUI_NAT_BUTTON_VPAD` (12)    |
| INPUT, COMBOBOX, DROPDOWN, SPINBOX, DATEPICKER, IPINPUT, PATHBAR | `font_height + MKGUI_NAT_INPUT_VPAD` (10)    |
| CHECKBOX, RADIO, TOGGLE                                         | `font_height + MKGUI_NAT_CHECKBOX_VPAD` (6)   |
| LABEL                                                           | `font_height + MKGUI_NAT_LABEL_VPAD` (4)      |
| SLIDER, SCROLLBAR                                               | `MKGUI_NAT_SLIDER_H` (24)                     |
| PROGRESS, METER                                                 | `MKGUI_NAT_PROGRESS_H` (20)                   |
| DIVIDER                                                         | `MKGUI_NAT_DIVIDER_H` (6)                     |
| LISTVIEW, GRIDVIEW, RICHLIST, TREEVIEW, TREELIST, ITEMVIEW, TEXTAREA, LOGVIEW      | `row_height * 3`                              |
| TABS                                                            | `tab_height + row_height * 2`                 |
| HSPLIT, VSPLIT                                                  | `split_min_px*2 + split_thick`                |
| IMAGE, CANVAS, GLVIEW, SPINNER                                           | `MKGUI_NAT_CANVAS_H` (24)                     |
| VBOX, HBOX, FORM, PANEL, GROUP                                  | `row_height`                                  |

| Widget            | Natural width                                                                         |
|-------------------|---------------------------------------------------------------------------------------|
| BUTTON            | `text_width + (icon_size + MKGUI_NAT_BUTTON_ICON_GAP if icon) + MKGUI_NAT_BUTTON_HPAD` |
| LABEL             | `text_width + MKGUI_NAT_LABEL_HPAD` (4)                                               |
| CHECKBOX, RADIO   | `MKGUI_NAT_CHECKBOX_BOX_W + text_width` (22)                                          |
| TOGGLE            | `MKGUI_NAT_TOGGLE_BOX_W + text_width` (44)                                            |
| DIVIDER           | `MKGUI_NAT_DIVIDER_W` (4)                                                             |
| anything else     | `MKGUI_NAT_DEFAULT_W` (40)                                                            |

## 16. Minimum window size (`lc_min_size`, `lc_window_min_size`)

Recursive lower bound for the OS-level minimum window size. VBox/HBox/Panel containers marked `MKGUI_SCROLL` use a minimum of `(100, row_height * 3)` rather than their full content extent. Runtime scroll state is provided for VBox/HBox as described above. Splits recursively sum the
appropriate axis. Leaves use their explicit size or
`natural_{width,height}`. Containers delegate back to `lc_measure_container`
so there is exactly one source of truth.

The OS minimum and editor canvas enforce a scaled `(200, 100)` floor before
`box_pad*2` is added on each axis. `mkgui_get_min_size()` returns the content
minimum including padding, without the OS floor. The editor uses the same
calculation to grow the designed window when its contents no longer fit.

## 17. Global values (metric constants)

Defined in [mkgui.h](../mkgui.h). All are scaled into `win` fields by
`mkgui_recompute_metrics`. Toolkit and editor implementation code uses the scaled `win->` fields. Shared-library applications use public geometry and sizing APIs; these private fields are not exposed.

| Constant                        | Default | `win` field          |
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
3. Set `.fixed_h = 1` in `widget_desc[]` for a single-line control that should hold its natural height in a VBox. Leave it zero for default vertical flex. `widget_vflex_default()` reads that descriptor; it has no per-type switch.
4. If the widget is a **container**, decide whether to handle it in
   `lc_layout_node` (custom rules) or let the generic fallback distribute
   full-parent rects. Also add it to the padding predicate if it should
   suppress nested children's padding.
5. Use `sc(win, N)` for every pixel constant so DPI scaling is automatic.
   Never write raw px.
6. Use `win->box_gap` / `win->box_pad` / `win->row_height` etc. -- do not
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

## 19. Literal offsets

Some layout formulas combine scaled font metrics with unscaled offsets, such as `font_height + 4` for a group header and `font_height + 10` for a form row. These formulas are implementation details; applications using the shared library cannot override them with compile-time definitions in their own source.
