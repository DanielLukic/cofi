# COFI - C/GTK Window Switcher

COFI is a fast window switcher for X11 Linux desktops, written in C with GTK3. It provides Alt-Tab functionality with real-time fuzzy search and intelligent result ranking.

## Features

- **Instant window switching** with MRU (Most Recently Used) ordering
- **Multi-stage fuzzy matching** with intelligent scoring:
  - Word boundary matches: "comm" → "Commodoro" 
  - Initials matching: "ddl" → "Daniel Dario Lukic"
  - Subsequence matching: "th" → "Thunderbird"
- **Alt-Tab behavior** - pressing Enter switches to the most recent window
- **Harpoon-style assignments** - assign windows to number keys for instant access
- **Command mode** - vim-style commands for window management (`:cw2`, `:tL`, `:j5`)
- **Event-driven updates** - real-time window list synchronization
- **Single instance** - subsequent launches activate existing window
- **Zero external dependencies** - direct X11 window activation
- **Lightweight** - minimal memory footprint, fast startup

## Building

### Dependencies

- GTK3 development libraries
- X11 development libraries  
- GNU Make
- GCC

On Debian/Ubuntu:

```bash
sudo apt install libgtk-3-dev libx11-dev build-essential
```

### Compilation

```bash
make
```

To build with debug output:

```bash
make debug
```

## Usage

Run cofi:

```bash
./cofi
```

### Command-line Options

![Command Line Options](doc/cofi-options.png)

- `--log-level LEVEL` (-l) - Set log level: trace, debug, info (default), warn, error, fatal
- `--log-file FILE` (-f) - Write logs to file
- `--no-log` (-n) - Disable logging
- `--align POSITION` (-a) - Window position: center (default), top, top_left, top_right, left, right, bottom, bottom_left, bottom_right
- `--no-auto-close` (-C) - Don't close window when focus is lost
- `--workspaces` (-w) - Start with the Workspaces tab active
- `--command` - Start directly in command mode with `:` prompt active
- `--version` (-v) - Show version
- `--help` (-h) - Show help
- `--help-commands` (-H) - Show command mode help

### Configuration

COFI saves configuration to `~/.config/cofi/`:

- **`~/.config/cofi/options.json`** - Application settings
  - `close_on_focus_loss` - Auto-close on focus loss (boolean)
  - `align` - Default window alignment
  - `workspaces_per_row` - Grid layout for workspace view (0 = linear)
  - `tile_columns` - Tiling grid columns: 2 (2x2 grid) or 3 (3x2 grid), default 2
  - `quick_workspace_slots` - Alt+1-9 always switches workspaces when true (boolean, default false)
- **`~/.config/cofi/harpoon.json`** - Window assignments
  - Slots 0-9: Ctrl+0-9 / Alt+0-9
  - Slots a-z: Ctrl+a-z / Alt+a-z (excluding h,j,k,l,u)

### Keyboard Shortcuts

#### Window/Workspace Navigation
- **Up/Down arrows** - Navigate through windows
- **Ctrl+k/Ctrl+j** - Navigate up/down (Vim-style)
- **Tab** - Switch between Windows and Workspaces tabs
- **Enter** - Activate selected window/workspace
- **Escape** - Cancel and close
- **Type to search** - Filter windows in real-time

#### Harpoon-Style Window Assignment
- **Ctrl+0-9** - Assign current window to number key (0-9)
- **Ctrl+a-z** - Assign current window to letter key (a-z, see exclusions below)
- **Alt+0-9** - Switch directly to window assigned to number
- **Alt+a-z** - Switch directly to window assigned to letter
- **Ctrl+key** on assigned window - Remove assignment (toggle)

**Excluded Keys:**
The following keys are reserved for navigation and cannot be used for harpoon assignments:
- **Ctrl+j** - Navigate down in selection
- **Ctrl+k** - Navigate up in selection
- **Ctrl+u** - Clear search text (GTK default behavior)

**Override Exclusions with Shift:**
You can override the key exclusions by holding Shift:
- **Ctrl+Shift+j/k/u** - Assign these normally excluded keys as harpoon slots
- This is safe because activation still uses Alt+key (without Shift)
- Example: Ctrl+Shift+j assigns to slot 'j', then Alt+j activates it

This gives you 33 available harpoon slots by default (0-9 and a-i, l-t, v-z), or all 36 slots (0-9 and a-z) when using the Shift override.

#### Quick Workspace Slots Mode

When `quick_workspace_slots` is enabled in options.json:
- **Alt+1-9** - Always switches directly to workspaces 1-9 (regardless of current tab)
- **Ctrl+1-9** - Still assigns/unassigns harpoon slots as normal
- **Harpoon window access** - Use the Harpoon tab or other methods to access harpooned windows

This mode is useful if you prefer quick workspace switching over quick window switching, similar to many window managers and browsers.

### Command Mode

Press `:` to enter command mode for advanced window management operations. Commands can be typed with or without spaces between the command and arguments.

![Command Mode](doc/cofi-commands.png)

#### Available Commands

**Window Management:**
- `:cw [N]` or `:change-workspace [N]` - Move selected window to workspace N
  
  ![Change Workspace](doc/cofi-move-workspace.png)
- `:tm` or `:toggle-monitor` - Move window to next monitor
- `:tw [OPT]` or `:tile-window [OPT]` or `:t [OPT]` - Tile window
  - Options: L/R/T/B (halves), 1-4 (2x2 grid) or 1-6 (3x2 grid), F (fullscreen), C (center)
  - Direct tiling: `:t[lrtbc][1-4]` for quick sizing
    - Sizes: 1=25%, 2=50%, 3=66%, 4=75% (or 100% for center)
    - Examples: `:tr4` (right 75%), `:tl2` (left 50%), `:tc1` (center 33%), `:tc4` (fullscreen)
  
  ![Tiling Options](doc/cofi-tiling.png)
- `:cl` or `:close-window` or `:c` - Close selected window
- `:mw` or `:maximize-window` or `:m` - Toggle maximize
- `:hm` or `:horizontal-maximize-window` - Toggle horizontal maximize
- `:vm` or `:vertical-maximize-window` - Toggle vertical maximize

**Window Properties:**
- `:sb` or `:skip-taskbar` - Toggle skip taskbar
- `:at` or `:always-on-top` or `:aot` - Toggle always on top
- `:ew` or `:every-workspace` - Toggle sticky (show on all workspaces)

**Navigation:**
- `:jw [N]` or `:jump-workspace [N]` or `:j [N]` - Jump to workspace N
  
  ![Jump to Workspace](doc/cofi-jump-workspace.png)
- `:help` or `:h` or `:?` - Show command help

#### No-Space Syntax

Commands with arguments can be typed without spaces for faster entry:
- `:cw2` - Move window to workspace 2
- `:j5` - Jump to workspace 5  
- `:tL` - Tile window left half
- `:t5` - Tile to grid position 5
- `:tr4` - Tile window right 75%
- `:tl1` - Tile window left 25%
- `:tc3` - Center window at 75% size

This vim-style syntax works alongside traditional space-separated commands.

### Window Display

COFI shows windows in a 5-column format:
- **Desktop** - [0-9] for desktop number, [S] for sticky windows
- **Instance** - Application instance name
- **Title** - Window title (truncated to fit)
- **Class** - Application class name
- **ID** - Window ID in hexadecimal

The display is bottom-aligned (fzf-style) with the most recent window at the bottom.

## Example

![COFI Example](doc/example.png)

See how I assigned shortcuts for Thunderbird (m), and my current project (p), the terminal to that project (t), the volume control (v). Cofi makes it easy to jump to these windows directly. For example `<alt-tab><alt-m>` jumps to my mail. Without releasing the `<alt>` key.

To achieve this, I have my Linux Mint window switching reconfigured to map <alt-tab> to `cofi`. That's it!

## Recent Updates

### Version 1.x (2025)

- **Overlay Dialogs**: All dialogs (tiling, workspace operations) now use GTK overlays for seamless visual integration
- **Configurable Tiling Grid**: Choose between 2x2 (4 positions) or 3x2 (6 positions) grid layouts via `tile_columns` in config
- **Direct Command Mode**: Launch with `--command` to start directly in command mode
- **Vim-Style Commands**: No-space command syntax (`:cw2`, `:tL`, `:j5`) for faster operation
- **Direct Tiling Commands**: New syntax `:t[lrtbc][1-4]` for quick window tiling with specific sizes (25%, 50%, 66%, 75%)
- **Improved Tiling**: Better window tiling using maximize states
- **Enhanced Configuration**: Refactored configuration system with better validation

## Advanced Features

### Intelligent Search

COFI uses multi-stage matching with priority scoring:

1. **Word Boundary** (highest) - Matches at word starts
2. **Initials** (very high) - Matches first letters of words  
3. **Subsequence** (high) - Matches characters in order
4. **Fuzzy** (fallback) - Complex partial matching

### Harpoon Assignments

Inspired by the VIM Harpoon plugin:

- **Persistent** - Assignments saved to `~/.config/cofi/harpoon.json`
- **Automatic reassignment** - When windows close, intelligently reassign numbers
- **Visual indicators** - Assigned numbers shown in first column
- **Fuzzy reassignment** - Uses partial title matching for smart reassignment

### Single Instance

- Only one COFI window runs at a time
- Subsequent launches activate the existing window
- Clean process management with lock files

### Event-Driven Updates

- Real-time window list updates via X11 PropertyNotify events
- No polling - zero CPU overhead when idle
- Instant response to window creation/destruction

## Installation

```bash
sudo make install
```

This installs cofi to `/usr/local/bin/`.

To uninstall:

```bash
sudo make uninstall
```

## Testing

Run the test suite:

```bash
make test
```

## Architecture

COFI uses a modular architecture with specialized components for:

- Application lifecycle and GTK window management
- X11 window property extraction and activation
- Window enumeration via EWMH protocol
- MRU (Most Recently Used) ordering and Alt-Tab logic
- Multi-stage search with intelligent scoring
- Display formatting and user interface
- Event-driven updates for real-time synchronization
- Single instance management with IPC
- Harpoon-style window assignments
- Workspace switching and management

See [CLAUDE.md](CLAUDE.md) for detailed technical documentation.

## Requirements

- X11-based Linux desktop environment
- Window manager with EWMH support (most modern WMs)
- GTK3 runtime libraries

## License

See [LICENSE](LICENSE) for terms.
