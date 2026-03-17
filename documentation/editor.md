# mkgui Editor

Visual GUI editor for designing mkgui interfaces. Produces a project file that
can be saved/loaded and a complete compilable C source file via code generation.

## Quick start

1. Launch the editor binary
2. Click a widget in the **palette** (right panel) to enter placement mode
3. Click on the canvas to place the widget -- repeat to place more of the same
4. Press **Escape** to exit placement mode
5. Select placed widgets to edit properties in the right panel
6. **File > Generate Code** to export a compilable C file

## Canvas

The central area is a live-rendered canvas showing the window being designed.
Widgets are rendered using the real mkgui renderer, not wireframe mockups, so
what you see is what the generated program will look like.

- Default size: 640x480
- Minimum size: 64x64
- Drag any edge or corner of the canvas to resize the window

Grid snapping is active by default (8px grid). All placement, movement, and
resizing snaps to the grid.

## Placing widgets

Click a widget or container in the palette to enter **placement mode**. The
status bar shows "Place: WidgetType". Each click on the canvas places an
instance. You can place as many as you want without re-selecting the palette
entry. Press **Escape** to return to normal selection mode.

### Smart parenting

Widgets are automatically parented to whatever container they overlap when
placed. Specific behaviors:

- **Ctrl held + container selected**: new widgets are always added to the
  currently selected container regardless of click position. This is useful for
  building a layout quickly, e.g. select a VBox, then Ctrl-click to add five
  HBoxes to it in sequence, without each new HBox nesting inside the previous
  one.
- **Tabs**: widgets placed over a TABS widget go into the currently active tab.
- **Form**: placing a widget inside a Form auto-inserts a Label in the left
  column.
- **HBox/VBox**: children get w=0 (HBox) or h=0 (VBox) by default so they flex
  to fill available space.

## Selecting and editing widgets

Click a widget on the canvas to select it. The selection shows 8 resize handles
(corners and midpoints). The treeview on the left and the property panel on the
right update to reflect the selection.

- **Click and drag** a widget to move it (grid-snapped).
- **Drag a resize handle** to resize (minimum 4x4, grid-snapped).
- **Click canvas background** to deselect.

## Treeview

The left panel shows the widget hierarchy as a tree.

- Click a node to select that widget.
- Drag a node to reparent or reorder:
  - Drop above a target to insert before it.
  - Drop onto a target to insert as a child.
  - Drop below a target to insert after it.
- The Window (root) widget cannot be moved.
- Circular parenting is prevented.

## Properties panel

The right panel shows editable properties for the selected widget.

### Basic

| Field  | Description |
|--------|-------------|
| Type   | Read-only widget type |
| ID     | Unique identifier used in generated code (e.g. `ID_BUTTON_1`) |
| Label  | Display text |
| Icon   | Icon name (click browse button to open icon picker) |
| Parent | Dropdown to change parent container |
| Tab#   | Tab order (0-999) |

### Position and size

| Field  | Key | Description |
|--------|-----|-------------|
| X      | X   | Horizontal position |
| Y      | Y   | Vertical position |
| W      | W   | Width |
| H      | H   | Height |
| Weight |     | Flex weight (visible when parent is HBox/VBox) |
| Align  |     | Cross-axis alignment (visible when parent is HBox/VBox) |

Pressing the letter key (X/Y/W/H/L/I) while a widget is selected jumps focus
to that property field for quick editing.

### Flags

A grid of checkboxes for widget flags:

| Flag     | Description |
|----------|-------------|
| Left, Top, Right, Bottom | Anchor edges for responsive resizing |
| Hidden   | Widget starts hidden |
| Disabled | Widget starts disabled |
| Checked  | Initial checked state (checkbox, radio, menu item) |
| Scroll   | Enable scrolling (box, form, panel) |
| Border   | Visible border (panel, image, canvas) |
| Sunken   | Recessed appearance |
| No Pad   | Remove padding |
| Password | Mask text input |
| Readonly | Prevent editing (input, textarea) |
| Fixed    | Lock size in flex container |
| RgnTop, RgnBot, RgnLeft, RgnRight | Split region assignment |
| Separator, MChk, MRad | Menu item options |

### Tab management

Visible when a TABS widget is selected:

- **Add Tab** -- create a new tab
- **Remove Tab** -- delete the active tab

### Menu management

Visible when a MENU or MENUITEM is selected:

- **Add Item** -- add a menu item
- **Remove Item** -- delete selected item
- A dedicated **menu tree** shows the menu hierarchy with drag-and-drop
  reordering.

### Data items

Visible when a DROPDOWN or COMBOBOX is selected:

- Item list with add, remove, move up, move down buttons
- Edit item text inline
- These items are used in code generation to pre-populate the widget

### Events

A list of events available for the selected widget type. Enable events with
the checkboxes. Enabled events generate `case` stubs in the output code.

## Keyboard shortcuts

| Key              | Action |
|------------------|--------|
| Ctrl+S           | Save project |
| Ctrl+Z           | Undo |
| Ctrl+Shift+Z     | Redo |
| Ctrl+Y           | Redo (alternate) |
| Delete           | Delete selected widget |
| Escape           | Cancel placement / close help popup |
| F1               | Show help for hovered palette widget |
| X                | Focus X position field |
| Y                | Focus Y position field |
| W                | Focus width field |
| H                | Focus height field |
| L                | Focus label field |
| I                | Focus ID field |

## Toolbar

| Button | Icon            | Action |
|--------|-----------------|--------|
| New    | document-new    | Clear project |
| Open   | document-open   | Load project file |
| Save   | document-save   | Save project (Ctrl+S) |
| Gen    |                 | Generate C code |
| Test   |                 | Launch live preview window |
| Undo   | edit-undo       | Undo |
| Redo   | edit-redo       | Redo |

## Menus

### File

- **New** -- clear all widgets, reset canvas
- **Open** -- load a .mkgui project
- **Save** -- save to current file
- **Save As...** -- save with new filename
- **Recent Files** -- submenu with up to 10 recently opened projects
- **Generate Code** -- export compilable C source
- **Exit** -- close editor

### Edit

- **Undo** -- undo last action
- **Redo** -- redo next action
- **Delete** -- delete selected widget

## Layout system

### Anchors

Anchor flags control how a widget responds when its parent resizes:

- **Left+Right**: widget stretches horizontally
- **Top+Bottom**: widget stretches vertically
- **Right only**: widget sticks to right edge
- **Bottom only**: widget sticks to bottom edge

### HBox / VBox

Flex layout containers. Children are arranged horizontally (HBox) or vertically
(VBox) with 6px gaps.

- Set **w=0** (HBox) or **h=0** (VBox) for a child to flex and fill space.
- **Weight** controls the proportion of flex space (default 1).
- **Align** sets cross-axis alignment (start/center/end).
- **Fixed** flag locks a child to its explicit size.

### Form

Two-column layout: labels on the left, controls on the right.

- Row height defaults to 24px.
- A Label is auto-inserted when placing a widget into a Form.
- Control width flexes to fill the right column (w=0).

### Split (HSplit / VSplit)

Two-region container with a draggable divider.

- Children must have a **region flag** set:
  - HSplit: RgnTop / RgnBot
  - VSplit: RgnLeft / RgnRight

### Tabs

Tab container. Children must be TAB type widgets.

- Click tab headers on the canvas to switch active tab during design.
- Add/Remove tabs from the properties panel.

## Code generation

**File > Generate Code** (or the Gen toolbar button) opens a save dialog and
writes a complete C source file containing:

1. `#include "mkgui.c"` (unity build)
2. An `enum` with all widget IDs
3. Listview row callback stubs (for LISTVIEW widgets)
4. A `main()` function with:
   - Widget array initialization with types, IDs, labels, positions, anchors
   - Setup calls for widgets that need them (dropdown items, slider range,
     spinbox range, combobox items, listview columns)
   - Event loop with `case` stubs for every enabled event
   - Automatic `MKGUI_EVENT_CLOSE` handler

The generated file compiles directly with no modifications needed.

## Test preview

The **Test** toolbar button launches a separate window running the designed GUI
with full interactivity. This lets you verify behavior (tab switching, button
clicks, scrolling, etc.) before generating code.

## Undo / Redo

The editor maintains a 64-level undo history. Snapshots are taken before any
destructive operation: widget add/delete, move/resize, property changes,
reparenting, tab/menu/data operations.

## File format

Projects are saved as binary `.mkgui` files containing the complete widget tree,
properties, hierarchy, data items, and editor state. The format stores widget
types as numeric values, making it forward-compatible with new widget types.

## Tips

- **Rapid layout building**: select a container, hold Ctrl, click palette
  entries to add multiple children without nesting them inside each other.
- **Quick property editing**: press X/Y/W/H/L/I to jump to that field without
  reaching for the mouse.
- **Tab workflow**: create a TABS widget, use "Add Tab" in properties, then
  click tab headers on the canvas to switch between tabs while placing widgets.
- **Form workflow**: just place controls into a Form -- labels are inserted
  automatically.
- **F1 help**: hover over any palette widget and press F1 to see what it does,
  what events it emits, and what flags are relevant.
