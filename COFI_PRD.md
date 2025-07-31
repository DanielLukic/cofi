# COFI Product Requirements Document

## Executive Summary

COFI (C-based Objective Fast Interface) is a high-performance window switcher for X11 Linux desktops, written in C with GTK3. It provides instant window switching with real-time fuzzy search, intelligent result ranking, and advanced window management features. COFI replaces traditional Alt-Tab functionality with a modern, searchable interface inspired by tools like fzf while adding unique features like harpoon-style window assignments and comprehensive tiling capabilities.

## Target Environment

- **Operating System**: Linux with X11 display server
- **Window Manager**: Any EWMH-compliant window manager
- **Dependencies**: GTK3, X11/Xlib, D-Bus (via GIO)
- **Build Requirements**: GCC, GNU Make, pkg-config

## Core Architecture

### Technology Stack

1. **X11/Xlib**: Direct window management and property extraction
2. **GTK3**: Native UI framework for rendering and input handling
3. **D-Bus**: Inter-process communication for single instance enforcement
4. **GLib/GIO**: Event loop integration and utility functions
5. **JSON-C**: Configuration file parsing (implied by implementation)

### Application Structure

The application follows a modular architecture with clear separation of concerns:

- **Main Entry Point**: Application initialization, argument parsing, GTK setup
- **X11 Layer**: Window enumeration, property extraction, event monitoring
- **Data Management**: Window information, history tracking, filtering
- **UI Layer**: GTK widgets, display formatting, keyboard handling
- **Feature Modules**: Harpoon assignments, custom names, workspaces, tiling
- **Support Systems**: Logging, configuration, IPC, utilities

## Feature Specifications

### 1. Window Switching

#### 1.1 Core Functionality
- **Instant Activation**: Direct X11 window activation without external tools
- **MRU Ordering**: Windows displayed in Most Recently Used order
- **Alt-Tab Behavior**: Default selection on second window for quick switching
- **Real-time Updates**: Window list automatically syncs with X11 changes

#### 1.2 Display Format
- **5-Column Layout**: Harpoon slot, desktop, instance, title, class, window ID
- **Bottom-Up Display**: Best matches/most recent at bottom (fzf-style)
- **Monospace Formatting**: DejaVu Sans Mono for consistent alignment
- **Dynamic Sizing**: Display lines calculated based on screen height

#### 1.3 Window Information
- Window title (UTF-8 support)
- Application class and instance names
- Desktop/workspace assignment
- Window type (Normal vs Special)
- Process ID tracking
- Sticky window indicators

### 2. Search and Filtering

#### 2.1 Multi-Stage Matching Algorithm
1. **Word Boundary Matches** (Score: 2000)
   - Matches at start of words
   - Example: "comm" → "**C**ommodoro"

2. **Initials Matching** (Score: 1900)
   - Matches first letters of words
   - Example: "ddl" → "**D**aniel **D**ario **L**ukic"

3. **Subsequence Matching** (Score: 1500)
   - Characters in order but not adjacent
   - Example: "th" → "**T**under**h**ird"

4. **Fuzzy Matching** (Variable score)
   - Sublime Text-inspired algorithm
   - Character proximity bonuses
   - Separator and camel-case awareness

#### 2.2 Search Features
- **Case-Insensitive**: All matching ignores case
- **Multi-Field Search**: Searches title, class, instance simultaneously
- **Real-time Filtering**: Results update as user types
- **Workspace Bonus**: Current workspace windows get +25 score boost

### 3. Harpoon-Style Window Assignments

#### 3.1 Assignment System
- **36 Slots Total**: Numbers 0-9, letters a-z
- **Quick Assignment**: Ctrl+[0-9,a-z] assigns current window
- **Instant Switch**: Alt+[0-9,a-z] jumps to assigned window
- **Toggle Behavior**: Re-assigning same slot removes assignment

#### 3.2 Intelligent Features
- **Persistent Storage**: Assignments saved to ~/.config/cofi/harpoon.json
- **Smart Reassignment**: Automatically finds similar windows when original closes
- **Wildcard Support**: Title patterns with * for flexible matching
- **Visual Indicators**: Slot numbers shown in main window list

#### 3.3 Excluded Keys
- Ctrl+j/k (navigation), Ctrl+u (clear) are reserved
- Can be overridden with Shift modifier (Ctrl+Shift+j)

### 4. Custom Window Naming

#### 4.1 Name Management
- **Assign Names**: Give windows meaningful custom identifiers
- **Display Format**: "custom_name - original_title"
- **Persistent Storage**: Saved to ~/.config/cofi/names.json
- **Smart Matching**: Names reassign to similar windows automatically

#### 4.2 Names Tab Interface
- **Dedicated View**: Shows all named windows
- **Edit Names**: Ctrl+E to modify existing names
- **Delete Names**: Ctrl+D to remove custom names
- **Status Tracking**: Shows orphaned vs assigned names

### 5. Workspace Management

#### 5.1 Navigation Features
- **Jump to Workspace**: Quick number-based switching
- **Move Windows**: Send windows to other workspaces
- **Rename Workspaces**: Custom workspace names via EWMH
- **Visual Indicators**: Current workspace and window locations

#### 5.2 Display Modes
- **Grid Layout**: Visual workspace widgets in rows/columns
- **Linear List**: Traditional vertical list view
- **Quick Slots**: Alt+1-9 for direct workspace switching (optional)

### 6. Window Tiling

#### 6.1 Tiling Options
- **Half-Screen**: Left, Right, Top, Bottom (50%)
- **Quarters**: 25% sizing in all directions
- **Two-Thirds**: 66.7% sizing options
- **Three-Quarters**: 75% sizing options
- **Grid Positions**: 2×2 or 3×2 grid layouts
- **Center Options**: Various centered sizes

#### 6.2 Advanced Features
- **Multi-Monitor Aware**: Tiles within current monitor
- **Work Area Respect**: Avoids panels and docks
- **Size Hint Compliance**: Respects application constraints
- **Quick Syntax**: :tl2 (left 50%), :tr4 (right 75%)

### 7. Command Mode

#### 7.1 Vim-Style Commands
Enter command mode with ':' and execute operations:

**Window Management:**
- `:cw [N]` - Change workspace
- `:pw` - Pull window to current workspace
- `:cl` - Close window
- `:tm` - Toggle monitor

**Window Properties:**
- `:aot` - Always on top
- `:sb` - Skip taskbar
- `:ew` - Every workspace (sticky)
- `:mw` - Maximize window

**Tiling Commands:**
- `:tw [option]` - Tile window
- `:tL` - Tile left
- `:t4` - Grid position 4
- `:tr3` - Right 66%

**Navigation:**
- `:jw [N]` - Jump to workspace
- `:help` - Show command help

#### 7.2 No-Space Syntax
Commands support compact vim-style entry:
- `:cw2` → Change to workspace 2
- `:j5` → Jump to workspace 5
- `:tl1` → Tile left 25%

### 8. User Interface

#### 8.1 Multi-Tab Interface
- **Windows Tab**: Main window list with search
- **Workspaces Tab**: Workspace navigation and management
- **Harpoon Tab**: View and manage slot assignments
- **Names Tab**: Manage custom window names

#### 8.2 Window Properties
- **Borderless Design**: Clean, minimal appearance
- **Stay on Top**: Always visible when activated
- **Skip Taskbar**: Doesn't clutter taskbar
- **Position Options**: 9 alignment positions

#### 8.3 Keyboard Navigation
- **Arrow Keys/Ctrl+j,k**: Navigate selection
- **Tab/Shift+Tab**: Switch between tabs
- **Enter**: Activate selection
- **Escape**: Cancel and close
- **Type to Search**: Instant filtering

### 9. Configuration System

#### 9.1 Configuration Files
Located in ~/.config/cofi/:
- **options.json**: Application settings
- **harpoon.json**: Window assignments
- **names.json**: Custom window names

#### 9.2 Configurable Options
- Window alignment (9 positions)
- Auto-close on focus loss
- Workspace grid layout
- Tiling grid columns (2 or 3)
- Quick workspace slots mode

#### 9.3 Command-Line Options
- Logging controls (level, file, disable)
- Starting tab selection
- Window positioning
- Direct command mode entry

### 10. Performance Features

#### 10.1 Event-Driven Architecture
- X11 PropertyNotify monitoring
- No polling - zero CPU when idle
- GIOChannel integration with GTK

#### 10.2 Optimizations
- Pre-interned X11 atoms
- Fixed-size arrays (no allocations)
- Cached display calculations
- Batch event processing

#### 10.3 Single Instance
- D-Bus IPC for instance management
- Fast lock file checks
- Clean inter-process communication

## Technical Requirements

### System Integration

1. **X11/EWMH Compliance**
   - Full Extended Window Manager Hints support
   - Direct X11 manipulation for all operations
   - UTF-8 string handling throughout

2. **GTK3 Integration**
   - Native GTK widgets and styling
   - CSS theming support
   - Proper focus management

3. **File System**
   - XDG base directory compliance
   - JSON configuration format
   - Automatic directory creation

### Security Considerations

1. **Process Isolation**
   - User-specific lock files
   - Session-based D-Bus usage
   - No elevated privileges required

2. **Input Validation**
   - Buffer overflow prevention
   - Path traversal protection
   - JSON parsing safety

### Error Handling

1. **Graceful Degradation**
   - Missing properties use defaults
   - Failed operations logged but don't crash
   - Fallback behaviors for missing features

2. **User Feedback**
   - Clear error messages
   - Logging for debugging
   - Status indicators in UI

## Future Extensibility

The modular architecture supports:
- Additional matching algorithms
- New window management features
- Plugin system potential
- Alternative UI backends
- Extended automation capabilities

## Success Metrics

1. **Performance**: < 50ms startup time
2. **Memory**: < 10MB resident memory
3. **Responsiveness**: Instant filtering (< 16ms)
4. **Reliability**: No crashes in normal use
5. **Compatibility**: Works with all major window managers