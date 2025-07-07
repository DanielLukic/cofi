# COFI Code Cleanup TODO List

## High Priority Tasks

### 1. Remove Unused Code
- [ ] Remove `windows_match_exact()` function from `window_matcher.h/c` - never called
- [ ] Remove `log_set_lock()` function from `log.h/c` - never called
- [ ] Remove `log_add_callback()` function from `log.h/c` - never called
- [ ] Make `strcasechr()` static in `match.c` - only used internally

### 2. Split Large Files
- [ ] **Refactor `overlay_manager.c` (1216 lines)**
  - [ ] Extract tiling overlay code to `tiling_overlay.c`
  - [ ] Extract workspace overlay code to `workspace_overlay.c`
  - [ ] Extract harpoon overlay code to `harpoon_overlay.c`
  - [ ] Keep core overlay management in `overlay_manager.c`

### 3. Standardize X11 Property Access
- [ ] Update `workarea.c` to use `get_x11_property()` wrapper instead of direct `XGetWindowProperty()` calls
- [ ] Verify all X11 property access uses the centralized wrapper

## Medium Priority Tasks

### 4. Break Down Large Functions
- [ ] **Refactor `update_display()` in `display.c` (~150+ lines)**
  - [ ] Extract `format_window_display()`
  - [ ] Extract `format_workspace_display()`
  - [ ] Extract `format_harpoon_display()`

- [ ] **Refactor `execute_command()` in `command_mode.c`**
  - [ ] Extract individual command handlers
  - [ ] Create command validation functions
  - [ ] Simplify command parsing logic

- [ ] **Refactor `fit_column()` text processing**
  - [ ] Extract `clean_text()` - remove non-ASCII and normalize spaces
  - [ ] Extract `trim_text()` - remove leading/trailing spaces
  - [ ] Extract `pad_text()` - apply padding to width

### 5. Eliminate Code Duplication
- [ ] **String Operations**
  - [ ] Replace all `strncpy()` with `safe_string_copy()`
  - [ ] Replace appropriate `snprintf()` calls with `safe_string_copy()`
  - [ ] Document when `snprintf()` is still needed

- [ ] **Instance/Class Swapping Logic**
  - [ ] Create `should_swap_instance_class()` function
  - [ ] Replace duplicated uppercase checking logic

- [ ] **Move `bonus.h` contents**
  - [ ] Move `bonus_states` and `bonus_index` arrays to `match.c`
  - [ ] Move related macros (`ASSIGN_LOWER`, etc.) to `match.c`
  - [ ] Delete `bonus.h`

### 6. Simplify Tab Display Logic
- [ ] Replace if/else chains in `format_tab_header()` with table-driven approach:
  ```c
  static const char* tab_labels[] = {
      [TAB_WINDOWS] = "WINDOWS",
      [TAB_WORKSPACES] = "WORKSPACES",
      [TAB_HARPOON] = "HARPOON"
  };
  ```

## Low Priority Tasks

### 7. Code Consistency
- [ ] **Standardize NULL checks**
  - [ ] Choose between `if (!ptr)` or `if (ptr == NULL)`
  - [ ] Apply consistently across codebase

- [ ] **Error Handling Patterns**
  - [ ] Create consistent error logging format
  - [ ] Standardize error return patterns

### 8. Clean Up Includes
- [ ] Remove unused includes from `main.c`:
  - [ ] `<sys/wait.h>`
  - [ ] `<unistd.h>`
- [ ] Audit all files for unnecessary includes
- [ ] Remove duplicate includes of `<string.h>` and `<strings.h>`

### 9. Documentation and Memory Management
- [ ] Document when to use `XFree()` vs `free()` vs `g_free()`
- [ ] Review `match_positions()` for potential memory leak paths
- [ ] Verify all X11 property getters call `XFree()` on error paths

## Future Considerations

### Configuration Consolidation
- [ ] Consider creating unified configuration structure for:
  - Window positioning
  - Window sizing
  - Behavior settings

### Selection Management
- [ ] Evaluate centralizing all selection state management in `selection.c`
- [ ] Remove inline selection logic from other files

## Implementation Notes

1. **Testing**: Each refactoring should be tested thoroughly, especially:
   - Overlay behavior after splitting files
   - X11 property access changes
   - String operation replacements

2. **Commits**: Make each cleanup task a separate commit for easy review/revert

3. **Priority**: Focus on High Priority tasks first as they provide the most value:
   - Removing dead code reduces confusion
   - Splitting large files improves maintainability
   - Standardizing patterns prevents bugs

4. **Backwards Compatibility**: Ensure refactoring doesn't break:
   - Command-line arguments
   - Configuration file format
   - D-Bus interface
   - Keyboard shortcuts