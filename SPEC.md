# COFI Specification

Cofi is a keyboard-driven window switcher for X11/Linux with native GTK UI.

## Window List

Cofi displays a list of open windows on the system.

- Show all normal application windows
- Exclude docks, desktop backgrounds, and cofi's own window
- Display format: 6 fixed-width columns, monospace font
  - Harpoon slot indicator (1 char: digit/letter if assigned, blank otherwise)
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
- On the Windows tab only, pressing `.` with an empty query repeats the last successful non-empty query-driven window activation for this session
- Repeat re-runs the saved query against the current live window list and immediately activates the current top match
- `.` with a non-empty query inserts a literal period normally
- If no repeatable query has been stored yet, or the saved query has no current matches, `.` is a no-op and cofi stays open

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
- **Tab / Shift+Tab** — switch between tabs (Windows, Workspaces, Harpoon, Names, Config, Hotkeys, Apps)
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

## System Hotkeys

Cofi runs as a daemon and registers global hotkeys via XGrabKey on the X11 root window.

Hotkey bindings are stored in `hotkeys.json` as `{key, command}` pairs. Each binding maps a key combination to a cofi command string.

- Default bindings:
  - `Mod1+Tab` → `show windows!`
  - `Mod1+grave` → `show command!`
  - `Mod1+BackSpace` → `show workspaces!`
- Hotkey format: `Modifier+KeyName` using X11 names (`XStringToKeysym`)
  - Modifiers: `Mod1`, `Mod2`, `Mod3`, `Mod4`, `Control`, `Shift`
- CapsLock/NumLock variants grabbed automatically
- Retry/Exit dialog on grab failure (e.g. another app owns the key)
- When cofi is already visible, show-mode hotkeys switch mode inline (no reopen)
- Pressing a hotkey while visible cancels any pending focus-loss close timer

### Auto-Execute Marker (`!`)

Hotkey command strings support a `!` suffix to control dispatch behavior:

- **With `!`** — command executes immediately. If it needs UI (e.g. overlay dialog), cofi opens automatically. If it doesn't need UI (e.g. `:jw 3`), it runs silently on the active X11 window.
- **Without `!`** — cofi opens with the command prefilled in the command line, letting the user edit or add arguments before pressing Enter.

Examples:
- `"show windows!"` — opens cofi in windows mode immediately
- `"show run!"` — opens cofi in run mode immediately
- `"jw 3!"` — jumps to workspace 3 silently, no cofi window
- `"maw"` — opens cofi with `:maw` prefilled, user types direction + Enter

## Run Mode

Shell run entry triggered by typing `!` in the search field or via `show run`.

- Mode indicator changes from `>` to `!`
- `!` is indicator-only; run entry text is raw command text (no literal `!` prefix)
- Enter launches the trimmed command detached via the user's shell, then closes cofi
- Bare `!` (legacy tolerated by parser) or whitespace-only commands are a no-op
- Run history is session-only, separate from command history, and navigable with Up/Down
- Deleting back to empty exits run mode cleanly
- `extract_run_command()` remains backward-compatible and still tolerates legacy `!foo` input
- Run mode never updates repeat-last-query state

## Single Instance

Only one cofi instance runs at a time.

- Primary guard is a Unix domain socket at `$XDG_RUNTIME_DIR/cofi.sock` (fallback `/tmp/cofi.sock`)
- `cofi` with no delegate flags returns `already running` + exit 1 when daemon exists
- Delegate flags (`--windows`, `--workspaces`, `--harpoon`, `--names`, `--command`, `--run`, `--applications`) connect to the daemon, send a 1-byte opcode, and exit 0
- If no daemon exists, cofi binds the socket, starts daemon mode, and applies the requested startup delegate mode
- Stale socket path is recovered by connect-fail → unlink → bind retry

## Daemon Startup Model

- **Daemon (default)** — starts hidden, registers hotkeys, waits. Window shown only on hotkey press.
- **Delegate launch** — any delegate flag opens the requested tab/mode via socket; no separate no-daemon mode exists.

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

- Only meaningfully visible windows are numbered. Excluded:
  - Minimized or shaded windows
  - Cofi's own window
  - Non-normal windows such as docks
  - Windows whose visible content area is too small after accounting for occlusion by windows above them in the stacking order
- Visibility is based on the window content rect, not decoration leaks such as titlebars or borders
- A window must pass all of these checks to receive a digit slot:
  - total visible content fraction meets the configured threshold
  - largest single visible content fragment also meets that threshold
  - largest visible fragment is at least 8x8 px
- The threshold is configured by `slot_occlusion_threshold` as an integer percent (`5` means 5%)
- Windows are auto-numbered by screen position: left-to-right, top-to-bottom
- No manual assignment needed — numbering updates automatically as windows move, resize, minimize, or restore
- Alt+N is a no-op if the current workspace has fewer than N visible windows
- Letter-based harpoon slots (Alt+a-z) remain global and unaffected

### Slot Overlay Indicators

After slot assignment, numbered overlays appear briefly on each assigned window.

- Overlays are independent X11 windows (visible even after cofi hides)
- Overlay position is the center of the largest visible content fragment, not the raw window center
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
- Names tab (Ctrl+E to edit, Ctrl+D to delete — works on both active and orphaned entries)
- Searchable — custom names are included in filter matching

## Command Mode

Vim-style command entry triggered by typing `:` in the search field.

- Mode indicator changes from `>` to `:`
- Supports compact no-space syntax: `:cw2` is equivalent to `:cw 2`
- Command history: last 10 commands, navigable with Up/Down
- Help available via `:help` with paged output
- Help/config display persists while typing (dismissed on Esc)
- When entered from hidden/delegated flows, target selection is pinned by `command_target_id`: capture active window ID before `show_window()`, then `enter_command_mode()` resolves that ID in the current filtered list

### Window Commands

- `:cw [N|dir]` (`:change-workspace`) — move selected window to workspace N or direction (h/j/k/l)
- `:pw` (`:pull-window`, `:p`) — pull selected window to current workspace
- `:cl` (`:close-window`, `:c`) — close selected window
- `:sw` (`:swap-windows`) — swap two windows (positions and sizes)
- `:maw [N|dir]` (`:move-all-to-workspace`) — move all windows from current workspace to target
- `:mw` (`:max`, `:maximize-window`) — toggle maximize selected window
- `:miw` (`:min`, `:minimize-window`) — toggle minimize (restore if already minimized, closes cofi)

### Tiling Commands

- `:tw` or `:t` (`:tile-window`) — tile selected window
  - Halves: `L`, `R`, `T`, `B` (50%)
  - Quarters: `l1`, `r1`, `l2`, `r2` (25%)
  - Two-thirds: `L2`, `R2` (66%)
  - Three-quarters: `L3`, `R3` (75%)
  - Center: `c` with optional size
  - Grid positions: 2x2 or 3x2

### Workspace Commands

- `:jw [N|dir]` (`:jump-workspace`, `:j`) — jump to workspace N or direction (h/j/k/l)
- `:rw [N]` (`:rename-workspace`) — rename workspace N (current if omitted)

### Window Property Commands

- `:sb` (`:skip-taskbar`) — toggle skip taskbar
- `:aot` (`:at`, `:always-on-top`) — toggle always on top
- `:ab` (`:always-below`) — toggle always below
- `:ew` (`:every-workspace`) — toggle sticky (all desktops)

### Monitor Commands

- `:tm` (`:toggle-monitor`) — move window to next monitor
- `:hm` (`:horizontal-maximize-window`) — toggle horizontal maximize
- `:vm` (`:vertical-maximize-window`) — toggle vertical maximize

### Mouse Commands

- `:mouse` (`:m`, `:ma`, `:ms`, `:mh`) — mouse control with subcommand: `away`, `show`, `hide`

### Naming Commands

- `:an` (`:assign-name`, `:n`) — assign custom name to selected window

### Configuration Commands

- `:set <key> <value>` — set a config option at runtime (also accepts `key=value`)
- `:config` (`:conf`, `:cfg`) — switch to the interactive Config tab
- `:show` (`:s`) — switch cofi mode: `windows`, `command`, `run`, `workspaces`, `harpoon`, `names`, `config`, `hotkeys`, `apps`/`applications`

### Hotkey Management Commands

- `:hotkeys` (`:hotkey`, `:hk`) — show all hotkey bindings
- `:hotkeys <key> <command>` — bind a hotkey (e.g. `:hotkeys Mod4+1 jw 1`)
- `:hotkeys <key>` — unbind a hotkey
- Optional sugar keywords accepted: `list`, `set`, `add`, `del`, `rm`, `remove`

### Slot Commands

- `:as` (`:assign-slots`) — assign workspace window slots by screen position

### Help

- `:help` (`:h`, `:?`) — show command help with paged output

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

## Apps Tab

Launch installed desktop applications from a dedicated Apps tab.

- Data source: XDG desktop applications via GLib/GIO app-info APIs
- Excludes hidden / invalid / `NoDisplay` apps
- Enter launches the selected app detached and closes cofi
- `--applications` starts directly on the Apps tab
- `:show apps` switches to the Apps tab

### Apps Matching And Ranking

Apps tab matching is local to the Apps launcher and does not reuse Windows-tab MRU/fuzzy ranking.

- Ranking priority: `name` > `generic_name` > `keywords`
- `generic_name` and `keywords` are token-matched to avoid cross-token false positives
- Alphabetical order is only used as a tie-breaker within the same ranking tier

## Workspace Management

View and interact with workspaces via the Workspaces tab.

- List all workspaces with window counts
- Switch to a workspace by selecting it and pressing Enter
- Rename workspaces with custom names (persistent)
- Display as grid (configurable rows via `workspaces_per_row`, 0 = single row)
- Directional navigation: HJKL and arrow keys in workspace overlays

## Tabs

Seven tabs accessible via Tab/Shift+Tab:

1. **Windows** — main window list with search and MRU ordering
2. **Workspaces** — workspace list and management
3. **Harpoon** — harpoon slot assignments (Ctrl+E edit, Ctrl+D delete)
4. **Names** — custom window name assignments (Ctrl+E edit, Ctrl+D delete)
5. **Config** — all config options (Ctrl+T toggle/cycle, Ctrl+E edit)
6. **Hotkeys** — hotkey bindings (Ctrl+E edit, Ctrl+D delete)
7. **Apps** — installed desktop application launcher

- Selection state is preserved per tab when switching

## Configuration

Stored in `~/.config/cofi/`:

- `options.json` — application settings
  - Window position (9 positions: top, center, bottom, corners)
  - Close on focus loss (on/off)
  - Workspace grid columns (`workspaces_per_row`, 0 = single row)
  - Tiling grid columns (2 or 3)
  - Digit slot mode (`default` / `per-workspace` / `workspaces`)
  - Slot overlay duration in ms (default 750, 0 = disabled)
  - Ripple effect (on/off, default on)
- `hotkeys.json` — system hotkey bindings as `{key, command}` pairs (up to 64)
  - Managed via `:hotkeys` command or by editing the file directly
  - Auto-generated with default show-mode bindings on first run
- `harpoon.json` — harpoon slot assignments with match patterns
- `names.json` — custom window names

Runtime config changes via `:set <key> <value>` are saved to `options.json` immediately. View current config with `:config`.

## CLI Arguments

- `--align ALIGNMENT` — window position (center, top, bottom, top_left, top_right, left, right, bottom_left, bottom_right)
- `--no-auto-close` — keep cofi open when it loses focus
- `--windows` / `-W` — delegate to Windows tab
- `--workspaces` — delegate to Workspaces tab
- `--harpoon` — delegate to Harpoon tab
- `--names` — delegate to Names tab
- `--command` — delegate to command mode
- `--run` — delegate to run mode
- `--applications` — delegate to Apps tab
- `--assign-slots` — assign workspace window slots and exit
- `--log-level LEVEL` — set log verbosity (trace, debug, info, warn, error, fatal)
- `--log-file FILE` — write logs to file
- `--no-log` — disable logging
- `--version` — show version
- `--help` — show usage
- `--help-commands` / `-H` — show command mode help

## Window Highlight (Ripple Effect)

When a window is activated (via Alt-Tab, harpoon, workspace switch), a visual ripple highlights the target window.

- Circle ripple: expanding ring centered on the target window
- Vsync-locked 60fps via GdkFrameClock + Cairo on a single ARGB popup window
- 200ms duration with ease-out easing
- Ring expands from 10% to 30% of screen height, 16px stroke width
- Color: light blue-white (#f0f0ff), full brightness for 80% then fades out
- Cancelled immediately on workspace switch to prevent accumulation
- Configurable: `ripple_enabled` in options.json (default true)
- Graceful fallback: disabled when no ARGB visual available (no compositor)
- HiDPI-safe: X11 pixel coordinates for positioning, GDK logical coordinates for drawing

## Compact Command Syntax

Commands support no-space compact form: `:cw2` = `:cw 2`, `:mawk` = `:maw k`.

- Compact forms defined in a data-driven table (COMPACT_FORMS) mapping command names to accepted suffix characters
- Parser picks the longest matching command name to resolve prefix ambiguity
- Exact command names are never split: `:maw` stays as command "maw" with no arg, not "m" + "aw"
- Aliases participate in matching: `:j3` matches alias "j" for "jw", producing "jw" + "3"

## Autostart

Scripts provided for automatic startup:

- **XDG autostart** — `scripts/install-xdg.sh` (installs `.desktop` file to `~/.config/autostart/`)
- **systemd user service** — `scripts/install-systemd.sh` (enables and starts `cofi.service`)
