# TODO: Workspace Switching Feature

## Feature Overview

Add the ability to browse and switch between workspaces (virtual desktops) in addition to the existing window switching functionality. This will be implemented as a tabbed interface with "Windows" and "Workspaces" tabs.

## Core Requirements

### 1. Workspace Listing
- List all existing workspaces with ID and name
- Use X11 atoms: `_NET_NUMBER_OF_DESKTOPS` and `_NET_DESKTOP_NAMES`
- Display format: `[ID] Workspace Name` (e.g., `[0] Main`, `[1] Development`)
- Show current workspace indicator similar to active window

### 2. Tabbed Interface
- Two tabs: "Windows" (default) and "Workspaces"
- Tab navigation:
  - `<Tab>` or `<Ctrl-L>`: Switch to next tab
  - `<Shift-Tab>` or `<Ctrl-H>`: Switch to previous tab
- Visual indication of active tab (e.g., underline, highlight, or [Windows] vs <Workspaces>)
- Preserve search text when switching tabs

### 3. Workspace Mode Behavior
- Same fuzzy search functionality as window mode
- Search matches against workspace names
- No harpooning in workspace mode (workspaces already have persistent IDs)
- Enter key switches to selected workspace using `_NET_CURRENT_DESKTOP`
- Up/Down navigation works the same as window mode

### 4. Command Line Argument
- New argument: `--workspaces` to start in workspace tab
- Challenge: Need to communicate with already running instance
- Recommended approach: File-based IPC
  - Write mode to `/tmp/cofi.mode` file
  - Send SIGUSR1 to running instance
  - Running instance reads file and switches to workspace tab

## Implementation Plan

### Phase 1: X11 Workspace Support (Foundation)
1. Add to `x11_utils.c`:
   ```c
   int get_number_of_desktops(Display *display);
   char** get_desktop_names(Display *display, int *count);
   int get_current_desktop(Display *display);
   void switch_to_desktop(Display *display, int desktop);
   ```

2. Create new struct for workspace info:
   ```c
   typedef struct {
       int id;
       char name[256];
       bool is_current;
   } WorkspaceInfo;
   ```

3. Add workspace tracking to `AppData`:
   ```c
   WorkspaceInfo workspaces[MAX_WORKSPACES];
   int workspace_count;
   ```

### Phase 2: Tab Infrastructure
1. Add tab state to `AppData`:
   ```c
   typedef enum {
       TAB_WINDOWS,
       TAB_WORKSPACES
   } TabMode;
   
   TabMode current_tab;
   ```

2. Modify `display.c` to show tab header:
   - Add function `format_tab_header(TabMode mode)`
   - Show tabs at top: `[Windows]  Workspaces` or `Windows  [Workspaces]`

3. Update keyboard handling in `main.c`:
   - Add Tab/Shift-Tab handling
   - Add Ctrl-H/L handling
   - Clear and rebuild display when switching tabs

### Phase 3: Workspace Filtering & Display
1. Create `workspace_filter.c` (parallel to `filter.c`):
   - Reuse fuzzy matching algorithms from `match.c`
   - Filter workspace list based on search input
   - No scoring needed (smaller dataset)

2. Modify display logic:
   - When in workspace mode, display workspaces instead of windows
   - Format: `[0] Main`, `[1] Development`, etc.
   - Highlight current workspace

3. Update activation logic:
   - In workspace mode, Enter switches to workspace instead of window
   - Use existing `switch_to_desktop()` function

### Phase 4: Command Line & IPC
1. Add `--workspaces` option to getopt parsing

2. Implement file-based IPC:
   ```c
   void write_startup_mode(TabMode mode);
   TabMode read_startup_mode(void);
   void clear_startup_mode(void);
   ```

3. Modify signal handler:
   - Check for startup mode file after SIGUSR1
   - Switch to specified tab if file exists
   - Clear file after reading

### Phase 5: Event Handling
1. Monitor workspace changes:
   - Add `_NET_NUMBER_OF_DESKTOPS` to PropertyNotify monitoring
   - Refresh workspace list when count changes

2. Update current workspace indicator:
   - Monitor `_NET_CURRENT_DESKTOP` changes
   - Update display when workspace switches

## Refactoring Considerations

### Before Implementation
1. **Extract display formatting logic**:
   - Move window formatting to `format_window_entry()`
   - Create parallel `format_workspace_entry()`
   - Share common formatting code

2. **Generalize filtering**:
   - Extract common filtering interface
   - Make `filter.c` work with generic items
   - Implement workspace-specific filtering

3. **Separate keyboard handling**:
   - Move keyboard logic from `main.c` to `keyboard.c`
   - Create mode-specific key handlers
   - Easier to add tab switching logic

4. **Abstract activation**:
   - Create generic "activate item" interface
   - Window activation and workspace switching as implementations
   - Cleaner separation of concerns

## Testing Strategy

1. **Manual Testing**:
   - Test with multiple workspace configurations
   - Verify tab switching preserves state
   - Test `--workspaces` argument with running instance
   - Test workspace name edge cases (empty, Unicode, very long)

2. **Edge Cases**:
   - Single workspace environment
   - Workspaces with no names
   - Rapid workspace switching
   - Signal handling race conditions

3. **Integration**:
   - Test with different window managers (i3, GNOME, KDE)
   - Verify EWMH compliance
   - Test performance with many workspaces

## Notes

- Keep changes minimal and modular
- Preserve existing window switching functionality
- Maintain single instance behavior
- Follow existing code style and patterns
- Use existing infrastructure where possible (fuzzy matching, display formatting)
- Consider future extensibility (more tabs? different modes?)