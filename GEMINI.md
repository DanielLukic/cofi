# COFI - C/GTK Port of GOFI Window Switcher

## Project Overview

COFI is a high-performance window switcher for X11, written in C with a native GTK3 interface. It replaces the traditional Alt-Tab workflow with a fast, search-driven experience, inspired by tools like `fzf` and the VIM Harpoon plugin. The core design prioritizes speed, efficiency, and zero-dependency window management.

## Core Architecture

### 1. X11/EWMH Integration

- **Direct X11 Communication:** Uses Xlib for all window interactions, avoiding external tools like `wmctrl`.
- **EWMH Compliance:** Leverages Extended Window Manager Hints for robust integration with modern window managers.
- **Window Properties:** Fetches essential data:
    - `_NET_WM_NAME` / `WM_NAME` (Title)
    - `WM_CLASS` (Class/Instance)
    - `_NET_WM_WINDOW_TYPE` (Normal, Dialog, etc.)
    - `_NET_WM_DESKTOP` (Workspace)
    - `_NET_WM_PID` (Process ID)
- **Event-Driven Updates:** Monitors `_NET_CLIENT_LIST` and `_NET_ACTIVE_WINDOW` via `PropertyNotify` events, ensuring the window list is always up-to-date without polling.

### 2. Data Flow & Filtering

The application maintains several lists to manage window state:

1.  **`windows`**: The raw, unordered list of all open windows fetched directly from X11.
2.  **`history`**: A Most Recently Used (MRU) ordered list. The currently active window is always at the top. This list provides the canonical ordering for display.
3.  **`filtered`**: The final list presented to the user after applying the multi-stage search filter to the `history` list.

The filtering process is designed for intelligent and relevant results:

1.  **Word Boundary Matches** (Score: 2000): "ff" matches "**F**ire**f**ox".
2.  **Initials Matches** (Score: 1900): "vsc" matches "**V**isual **S**tudio **C**ode".
3.  **Subsequence Matches** (Score: 1500): "mail" matches "Thunderbird **Mail**".
4.  **Fuzzy Matches** (Variable Score): A fallback for more complex, non-sequential character patterns.

### 3. Display & UI (GTK3)

- **Native GTK Window:** A simple, borderless, always-on-top window for the search UI.
- **Layout:**
    - A `GtkTextView` displays the filtered window list.
    - A `GtkEntry` at the bottom captures user input for real-time filtering.
- **Bottom-Up Display:** Mimics `fzf`, with the top search result (and default selection) at the bottom of the list, closest to the input field.
- **Alt-Tab Swap Logic:** For instant switching between the two most recent windows, the top two entries in the display are visually swapped. The underlying data structures remain unchanged. Selecting the top item activates the *second* most recent window, and vice-versa.

### 4. Harpoon-Style Bookmarking

- **Persistent Assignments:** Users can "harpoon" a window to a specific key (e.g., `Ctrl+1`, `Ctrl+m`) for instant access (`Alt+1`, `Alt+m`).
- **Configuration:** Assignments are saved to `~/.config/cofi.json`.
- **Intelligent Reassignment:** If a harpooned window is closed, COFI attempts to reassign its slot to a similar window (e.g., a new terminal window) based on fuzzy matching of the window's class, instance, and title.

### 5. Single Instance Management

- **Lock File:** Uses a PID lock file (`/tmp/cofi.lock`) to ensure only one instance runs at a time.
- **IPC via Signals:** If a second instance is launched, it sends a `SIGUSR1` signal to the primary instance, which then brings its own window to the foreground.

## Key Data Structures

```c
// Main application state container
typedef struct {
    // GTK Widgets
    GtkWidget *window;
    GtkWidget *entry;
    GtkWidget *textview;

    // Window Lists
    WindowInfo windows[MAX_WINDOWS];      // Raw list from X11
    WindowInfo history[MAX_WINDOWS];      // MRU-ordered list
    WindowInfo filtered[MAX_WINDOWS];     // Search results for display
    int window_count;
    int history_count;
    int filtered_count;
    int selected_index;

    // X11 and System State
    Display *display;
    Window own_window_id;
    int active_window_id;

    // Features
    HarpoonManager harpoon;
    // ... other configuration fields
} AppData;

// Represents a single application window
typedef struct {
    Window id;
    char title[MAX_TITLE_LEN];
    char class_name[MAX_CLASS_LEN];
    char instance[MAX_CLASS_LEN];
    char type[16]; // "Normal", "Special", etc.
    int desktop;
    int pid;
} WindowInfo;
```

## File Organization

- **`main.c`**: Entry point, GTK setup, and main event loop.
- **`app_init.c`**: Initializes `AppData` and X11 connection.
- **`x11_utils.c`**: Low-level X11 functions for getting window properties and activation.
- **`x11_events.c`**: Manages X11 event listening and processing via a `GIOChannel`.
- **`window_list.c`**: Fetches the list of all client windows.
- **`history.c`**: Manages the MRU window order.
- **`filter.c`**: Implements the multi-stage search and scoring logic.
- **`match.c` / `fuzzy_match.c`**: Core fuzzy string matching algorithms.
- **`display.c`**: Renders the `filtered` list into the GTK `TextView`.
- **`harpoon.c`**: Manages harpoon assignments, persistence, and reassignment.
- **`instance.c`**: Handles single-instance logic.
- **`cli_args.c`**: Parses command-line arguments.

## Coding Conventions

- **Style:** Consistent `snake_case` for all functions and variables.
- **Modularity:** Code is organized into small, single-responsibility files (e.g., `history.c`, `filter.c`).
- **Function Size:** Functions are kept small and focused (typically 10-30 lines).
- **Testing:** A comprehensive test suite in the `test/` directory covers core logic, especially filtering and matching algorithms. Tests are written in C and executed via shell scripts.
- **Logging:** A dedicated logging utility (`log.c`) is used for debugging instead of `printf`.
