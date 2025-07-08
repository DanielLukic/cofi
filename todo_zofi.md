# ZOFI - Zig Port of COFI Window Switcher

## Setup

Work in a git worktree: `git worktree add gw_zofi -b gw_zofi`

## Development Guidelines

- Write small functions
- Properly separate by context/functionality into separate files

## Conversion Plan

Converting COFI from C to Zig incrementally, keeping only GTK UI components in C.

**Estimated Timeline: 3-4 weeks**

## Conversion Order

### Foundation
- [ ] Set up Zig build system alongside Makefile
- [ ] Convert simple utilities (log.c, utils.c, match.c)
- [ ] Port data structures (WindowInfo, AppData) to Zig idioms

### Core Algorithms
- [ ] Convert fuzzy matching and filtering algorithms
- [ ] Port history management and MRU logic
- [ ] Convert window matcher logic
- [ ] Port harpoon window assignment logic

### System Components
- [ ] Convert atom cache to Zig
- [ ] Port selection handling
- [ ] Convert workarea calculations
- [ ] Port monitor move logic
- [ ] Convert tiling logic algorithms
- [ ] Port command mode parsing
- [ ] Convert size hints handling

### IPC & Window System
- [ ] Port D-Bus service (dbus_service.c, instance.c)
- [ ] Convert X11 operations (x11_utils.c, x11_events.c, window_list.c)

### C Wrapper Layer
- [ ] Create Zig wrapper APIs for:
  - [ ] GTK UI code (keep gtk_window.c, display.c in C)
  - [ ] Overlay systems (keep *_overlay.c files in C)

## Benefits
- Compile-time memory safety
- Better error handling with error unions
- No manual memory management
- Cleaner string handling with slices
- Modern build system

## Notes
- GTK remains in C due to complex callback system and GObject model
- Incremental conversion allows testing at each step
- Can maintain hybrid C/Zig build during transition