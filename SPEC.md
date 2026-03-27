# COFI Specification

Cofi is a keyboard-driven window switcher for X11/Linux with native GTK UI.

## Window List

Cofi displays a list of open windows on the system.

- Show all normal application windows
- Exclude docks, desktop backgrounds, and cofi's own window
- Display format: 5 fixed-width columns, monospace font
  - Desktop indicator: `[0-9]` or `[S]` for sticky
  - Instance name (20 chars, truncated)
  - Window title (55 chars, truncated)
  - Class name (18 chars, truncated)
  - Window ID (hex)
- Display order: bottom-up (first entry at bottom, fzf-style)
- Selection indicator: `>` prefix on selected row

## MRU Ordering

Windows are ordered by most recently used (MRU). The most recently focused window appears first.

- When a window gains focus, it moves to the front of the list
- When a window closes, it is removed without disrupting order of remaining windows

## Alt-Tab Behavior

When cofi opens, the selection starts on the second entry (index 1) — the previously active window.

- Pressing Enter immediately switches to that previous window, enabling quick Alt-Tab-style toggling
- No swap of data structures is needed; this is purely about initial selection placement

## Search

Real-time filtering as the user types. Case-insensitive.

### Search Target

The search matches against the full displayed row as the user sees it — desktop indicator, instance name, window title, class name. This enables queries like "2ter" to find terminals on desktop 2, or combining any visible fields.

### Match Stages (in priority order)

1. **Word boundary** — query matches the start of a word
   - "comm" matches "Commodoro"
2. **Initials** — each character matches the first letter of consecutive words
   - "ddl" matches "Daniel Dario Lukic"
3. **Subsequence** — characters appear in order within the target
   - "th" matches "Thunderbird"
4. **Fuzzy** — fallback with variable scoring

### Scoring Adjustments

- Windows on the current desktop receive a scoring bonus
- Normal windows rank above special windows (docks, dialogs, etc.)

### Behavior

- Results update instantly as the user types
- Empty query shows all windows in MRU order
- Selection resets to default position (index 1) when filter changes

## Keyboard Navigation

- **Up/Down** (or **Ctrl+k/j**) — move selection through the window list
- **Enter** — activate the selected window
- **Escape** — close cofi without switching
- **Tab / Shift+Tab** — switch between tabs (Windows, Workspaces, Harpoon, Names)
- Typing any character starts filtering immediately (no mode switch needed)

## Window Appearance

- Borderless (no title bar or window decorations)
- Always on top of other windows
- Skips taskbar and pager
- Centered on screen
- Auto-closes when it loses focus (configurable)

## Layout

- Window list (top) — scrollable text view
- Search entry (bottom) — single-line input

## Window Activation

When the user selects a window and presses Enter:

- Switch to the window's desktop/workspace if different from current
- Raise and focus the selected window
- Close cofi

## Single Instance

Only one cofi instance runs at a time.

- Launching cofi while it's already open brings the existing window to focus
- Inter-process communication via D-Bus

## Live Window List

The window list updates automatically in real-time:

- New windows appear immediately
- Closed windows disappear immediately
- No polling — purely event-driven

## Harpoon Slots

Assign windows to persistent slots for direct access.

- 36 slots available: 0-9 and a-z
- **Ctrl+[key]** — assign the selected window to that slot
  - Ctrl+j, Ctrl+k, Ctrl+u are reserved for navigation
  - Ctrl+Shift+j/k/u overrides the reservation and assigns anyway
- **Alt+[key]** — activate the window in that slot directly (no exceptions)
  - Alt+digit behavior depends on `digit_slot_mode` (see below)
  - Alt+letter always activates global harpoon slots
- Assignments persist across cofi restarts (stored in config)
- When an assigned window closes, the slot attempts to reassign to a matching window (by class/title pattern)
- Harpoon tab shows all current assignments
- Harpoon indicators visible in the window list

## Workspace Window Slots

When `digit_slot_mode` is set to `per-workspace`, Alt+1-9 activates the Nth window on the current workspace.

- Only visible windows are numbered. Excluded:
  - Minimized or shaded windows
  - Occluded windows (>80% covered by a window above in the stacking order)
  - Cofi's own window
  - Minimizing or moving windows lets the user control which windows get slots
- Windows are auto-numbered by screen position: left-to-right, top-to-bottom
- No manual assignment needed — numbering updates automatically as windows move, resize, minimize, or restore
- Alt+N is a no-op if the current workspace has fewer than N visible windows
- Letter-based harpoon slots (Alt+a-z) remain global and unaffected

### Slot Overlay Indicators

After slot assignment, numbered overlays appear briefly centered on each assigned window.

- Overlays are independent X11 windows (visible even after cofi hides)
- Duration controlled by `slot_overlay_duration_ms` config (default 750, 0 = disabled)
- Catppuccin-themed: dark background, light text

### Auto-Assignment

Slots auto-assign on every Alt+digit press — no manual step needed. Windows are re-scanned each time, so slots stay correct after moves/resizes.

**Typical workflow**: Alt+Tab (open cofi) → Alt+1 (jump). That's it.

### Manual Assignment

Also available for explicit control:

1. **CLI flag** — `cofi --assign-slots`
   - For use with external hotkeys (WM, sxhkd, etc.)
   - Assigns slots on the current workspace and exits silently
2. **Command mode** — `:as` (or `:assign-slots`)
   - Also auto-enables per-workspace mode if not already set

### Digit Slot Mode (config)

`digit_slot_mode` controls what Alt+digit does:

- `default` — global harpoon slots (existing behavior)
- `per-workspace` — workspace-scoped window slots by position
- `workspaces` — switch to workspace N

Replaces the old `quick_workspace_slots` boolean.

## Custom Window Names

Assign custom names to windows that override the displayed title.

- Display format: "custom_name - original_title"
- Names persist across cofi restarts (stored in config)
- When a named window closes, the name attempts to reassign to a matching window
- Names tab (Ctrl+E to edit, Ctrl+D to delete)
- Searchable — custom names are included in filter matching

## Command Mode

Vim-style command entry triggered by typing `:` in the search field.

- Mode indicator changes from `>` to `:`
- Supports compact no-space syntax: `:cw2` is equivalent to `:cw 2`
- Command history: last 10 commands, navigable with Up/Down
- Help available via `:help` with paged output

### Window Commands

- `:cw [N]` — move selected window to workspace N
- `:pw` — pull selected window to current workspace
- `:cl` — close selected window
- `:sw` — swap two windows (positions)
- `:maw [N]` — move all windows to workspace N

### Tiling Commands

- `:tw` or `:t` — tile selected window
  - Halves: `L`, `R`, `T`, `B` (50%)
  - Quarters: `l1`, `r1`, `l2`, `r2` (25%)
  - Two-thirds: `L2`, `R2` (66%)
  - Three-quarters: `L3`, `R3` (75%)
  - Center: `c` with optional size
  - Grid positions: 2x2 or 3x2

### Workspace Commands

- `:jw [N]` or `:j [N]` — jump to workspace N

### Window Property Commands

- `:sb` — toggle skip taskbar
- `:aot` — toggle always on top
- `:ew` — toggle sticky (all desktops)

### Monitor Commands

- `:tm` — move to next monitor
- `:mw` — move window between monitors
- `:hm` / `:vm` — horizontal/vertical maximize

### Mouse Commands

- `:ma` — mouse action
- `:mh` — mouse hide
- `:ms` — mouse show

### Naming Commands

- `:an` or `:n` — assign custom name to selected window

## Tiling

Position and resize the selected window to predefined screen regions.

- Half screen: left, right, top, bottom (50%)
- Quarters: four corners (25%)
- Two-thirds: left or right (66%)
- Three-quarters: left or right (75%)
- Center: with configurable size
- Grid positions: 2x2 or 3x2 layout
- Multi-monitor aware: tiles relative to the window's current monitor
- Respects work area (excludes panels, docks, taskbars)
- Respects window size hints (e.g., terminal minimum sizes)

## Workspace Management

View and interact with workspaces via the Workspaces tab.

- List all workspaces with window counts
- Switch to a workspace by selecting it and pressing Enter
- Rename workspaces with custom names (persistent)
- Display as grid or linear list (configurable)

## Tabs

Four tabs accessible via Tab/Shift+Tab:

1. **Windows** — main window list with search and MRU ordering
2. **Workspaces** — workspace list and management
3. **Harpoon** — harpoon slot assignments (Ctrl+E edit, Ctrl+D delete)
4. **Names** — custom window name assignments (Ctrl+E edit, Ctrl+D delete)

- Selection state is preserved per tab when switching

## Configuration

Stored in `~/.config/cofi/`:

- `options.json` — application settings
  - Window position (9 positions: top, center, bottom, corners)
  - Close on focus loss (on/off)
  - Grid layout for workspace display (on/off)
  - Text alignment
  - Digit slot mode (`default` / `per-workspace` / `workspaces`)
  - Slot overlay duration in ms (default 750, 0 = disabled)
- `harpoon.json` — harpoon slot assignments with match patterns
- `names.json` — custom window names

## CLI Arguments

- `--align ALIGNMENT` — window position (center, top, bottom, top_left, top_right, left, right, bottom_left, bottom_right)
- `--no-auto-close` — keep cofi open when it loses focus
- `--workspaces` — start on the Workspaces tab
- `--harpoon` — start on the Harpoon tab
- `--names` — start on the Names tab
- `--command` — start in command mode
- `--log-level LEVEL` — set log verbosity (trace, debug, info, warn, error, fatal)
- `--log-file FILE` — write logs to file
- `--no-log` — disable logging
- `--version` — show version
- `--help` — show usage
- `--help-commands` / `-H` — show command mode help
