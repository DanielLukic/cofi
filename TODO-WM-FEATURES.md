# Window Management Features for COFI

## Overview

This document outlines the design for new window management features to be added to COFI without interfering with existing navigation and harpoon functionality.

## Current Modifier Usage

- **Ctrl** - Used for harpoon assignments and navigation (j/k/h/l)
- **Alt** - Used for harpoon activation and workspace switching
- **Shift** - Used to override harpoon exclusions
- **Super/Windows** - Currently unused!

## Proposed New Features

### 1. Move Window to Workspace - `Super+W`

**Functionality:**
- Opens a workspace selection dialog
- Shows current workspace highlighted
- Number keys (1-9,0) select target workspace (1-based indexing)
- Visual feedback shows where window will move
- Escape cancels the operation

**Implementation:**
- Set `_NET_WM_DESKTOP` property on the window
- Use ClientMessage protocol for EWMH compliance
- Show dialog with workspace list and current position

### 2. Move Window to Monitor - `Super+M`

**Functionality:**
- Cycles window through available monitors
- Wraps around (last monitor → first monitor)
- No dialog needed - instant action
- Could show brief overlay notification

**Implementation:**
- Use Xinerama or XRandR to detect monitors
- Calculate new window position maintaining relative position
- Use `XMoveResizeWindow` or `_NET_MOVERESIZE_WINDOW`

### 3. Tile Window - `Super+T`

**Functionality:**
- Opens tiling options dialog with these options:
  - `L` - Tile left half
  - `R` - Tile right half
  - `T` - Tile top half
  - `B` - Tile bottom half
  - `1-9` - Tile to grid position (3x3 grid)
  - `F` - Fullscreen toggle
  - `C` - Center window

**Implementation:**
- Get monitor/workspace geometry
- Calculate target position and size
- Use `XMoveResizeWindow` for immediate effect

## Implementation Strategy

1. **Add Super key support**
   - Add `GDK_SUPER_MASK` handling to key press handler
   - Update keyboard event processing

2. **Create modal dialogs**
   - Similar to existing COFI window style
   - Borderless, centered, stay-on-top
   - Clear visual indicators

3. **X11 window manipulation functions**
   - Extend `x11_utils.c` with new EWMH functions
   - Add monitor detection capabilities
   - Implement window geometry calculations

## Alternative Approach

If Super key isn't available or conflicts with user's window manager:
- `Ctrl+Shift+W` - Move to workspace
- `Ctrl+Shift+M` - Move to monitor
- `Ctrl+Shift+T` - Tile window

## Why This Design Works

1. **Non-intrusive** - Super key is completely unused in COFI
2. **Mnemonic** - Easy to remember: W(orkspace), M(onitor), T(ile)
3. **Consistent** - Uses modal dialogs like harpoon assignments
4. **Discoverable** - Dialogs show all available options
5. **Efficient** - Quick access without complex modifier combinations

## Technical Considerations

- Ensure EWMH compliance for maximum WM compatibility
- Handle edge cases (sticky windows, fullscreen state)
- Respect window size hints and constraints
- Provide visual feedback for all operations
- Test with multiple monitor configurations