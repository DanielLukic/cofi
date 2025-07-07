# COFI - C/GTK Port of GOFI Window Switcher

## Project Overview

COFI is a C/GTK port of the golang GOFI window switcher. Instead of using xterm+fzf for the UI, COFI provides a native GTK window with integrated search functionality.

## Core Architecture

### 1. Window Management (X11)

- Direct X11 interaction using Xlib
- EWMH (Extended Window Manager Hints) support
- Monitor window list changes via _NET_CLIENT_LIST
- Track active window via _NET_ACTIVE_WINDOW
- Extract window properties:
    - Title (_NET_WM_NAME, WM_NAME)
    - Class/Instance (WM_CLASS)
    - Type (_NET_WM_WINDOW_TYPE)
    - Desktop (_NET_WM_DESKTOP)
    - PID (_NET_WM_PID)

### 2. History Management

- Maintain MRU (Most Recently Used) window list
- Move newly focused windows to front
- Preserve order when windows close
- Ignore self ("cofi" titled windows)

### 3. Window Ordering

- Partition windows by type (Normal vs Special)
- Normal windows appear first
- **Alt-Tab Swap Logic**:
    - The swap happens ONLY in the display layer, never in the data structures
    - Data structures (windows, history, filtered) maintain true window order
    - When displaying: swap positions 0 and 1 for visual display only
    - When user selects entry 0 (first displayed), activate the window that's actually shown there
    - This enables Alt-Tab-like behavior where pressing Enter immediately switches to previous window

### 4. Multi-Stage Filtering

Real-time search with intelligent scoring:

1. **Word Boundary Matches** (score: 2000) - highest priority
   - "comm" matches "Commodoro" (starts with "comm")
   
2. **Initials Matches** (score: 1900) - very high priority  
   - "ddl" matches "Daniel Dario Lukic" (D-D-L initials)
   
3. **Subsequence Matches** (score: 1500) - high priority
   - "th" matches "Thunderbird" (t-h in sequence)
   
4. **Fuzzy Matches** (variable score) - fallback
   - Complex character matching with scoring

Case-insensitive matching on window title, class name, and instance name.

### 5. Display Format

- 5-column layout:
    - Desktop indicator [0-9] or [S] for sticky
    - Instance name (20 chars)
    - Window title (55 chars)  
    - Class name (18 chars)
    - Window ID (hex)
- Selection indicator ">"
- Monospace font for alignment
- Bottom-up display (fzf-style) with first entry at bottom

### 6. GTK UI

- Borderless window
- Stay on top
- Skip taskbar
- Center on screen
- Text view for window list (top)
- Entry widget for search (bottom)
- Keyboard navigation:
    - Up/Down: Navigate selection
    - Enter: Activate selected window
    - Escape: Cancel
    - Real-time filtering as user types

### 7. Window Activation

- Direct X11 window activation using EWMH protocol
- Switch to window's desktop first (_NET_CURRENT_DESKTOP)
- Send activation message (_NET_ACTIVE_WINDOW)
- Raise and map window (XMapRaised)
- No external dependencies (replaced wmctrl)

### 8. Event-Driven Updates

- **PropertyNotify events**: Monitor root window for _NET_CLIENT_LIST and _NET_ACTIVE_WINDOW changes
- **GIOChannel integration**: X11 file descriptor monitored via GLib main loop
- **Real-time updates**: Window list refreshes automatically when windows are created/destroyed

### 9. Single Instance Management

- Uses D-Bus IPC for inter-process communication
- Second instance calls ShowWindow method on first instance via D-Bus
- Clean inter-process communication with automatic cleanup

### 10. Harpoon-Style Window Assignment

- Assign windows to number keys (Ctrl+number)
- Direct switching (Alt+number)
- Persistent storage in ~/.config/cofi.json
- Automatic reassignment when windows close
- Fuzzy matching for intelligent reassignment

## Key Data Structures

```c
typedef struct {
    Window id;
    char title[MAX_TITLE_LEN];
    char class_name[MAX_CLASS_LEN];
    char instance[MAX_CLASS_LEN];
    char type[16]; // "Normal" or "Special"
    int desktop;
    int pid;
} WindowInfo;

typedef struct {
    GtkWidget *window;
    GtkWidget *entry;
    GtkWidget *textview;
    GtkWidget *scrolled;
    GtkTextBuffer *textbuffer;
    
    WindowInfo windows[MAX_WINDOWS];        // Raw window list from X11
    WindowInfo history[MAX_WINDOWS];        // History-ordered windows
    WindowInfo filtered[MAX_WINDOWS];       // Filtered and display-ready windows
    int window_count;
    int history_count;
    int filtered_count;
    int selected_index;
    int active_window_id;                   // Currently active window
    
    Display *display;
    HarpoonManager harpoon;
} AppData;
```

## File Organization

- `src/main.c` - Application entry point and GTK setup
- `src/x11_utils.c` - X11 window property extraction and activation
- `src/window_list.c` - Window enumeration via EWMH
- `src/history.c` - MRU ordering and Alt-Tab swap logic
- `src/filter.c` - Multi-stage filtering with intelligent scoring
- `src/display.c` - GTK display formatting and window activation
- `src/x11_events.c` - Event-driven window list updates
- `src/instance.c` - Single instance management
- `src/harpoon.c` - Harpoon-style window assignments
- `src/window_matcher.c` - Window matching logic
- `src/match.c` - Fuzzy matching algorithms
- `src/log.c` - Logging infrastructure

## Coding Guidelines

1. **Write small functions** (10-15 lines, max 30)
2. **Use snake_case consistently** for functions and variables
3. **Work in small commit steps** with clear messages
4. **Write tests for essential functionality**
5. **Use logging instead of print statements**
6. **Test with background processes** (`./cofi &`)

## Technical Features

- **Zero external dependencies** for window activation
- **Event-driven architecture** with no polling
- **Intelligent fuzzy matching** with multi-stage scoring
- **Memory efficient** with fixed-size arrays
- **X11 EWMH compliant** for broad window manager support
- **GTK3 native UI** with proper focus handling