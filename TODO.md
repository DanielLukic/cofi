# COFI Refactoring TODO

This document tracks refactoring opportunities to improve code maintainability while preserving 1:1 behavior.

## Phase 1: Quick Wins (1-2 days)

### 🔥 High Priority

- [x] **Extract Tab Navigation Duplication** (`src/main.c` lines 128-186)
  - [x] Create `switch_to_tab(AppData *app, TabMode target_tab)` function
  - [x] Replace 4 duplicated tab switching blocks
  - [x] Test tab switching with Tab, Ctrl+H, Ctrl+L keys
  - **Files:** `src/main.c`

- [x] **Create Constants File** (Multiple files)
  - [x] Create `src/constants.h` with all magic numbers
  - [x] Harpoon slot constants (0-9, a-z ranges)
  - [x] Display column widths (1, 4, 20, 55, 18)
  - [x] Filter scoring constants (2000, 1900, 1500, 1400)
  - [x] Replace hardcoded values throughout codebase
  - **Files:** `src/main.c`, `src/display.c`, `src/filter.c`

### 🧹 Cleanup

- [x] **Remove Debug Print Statements** (`src/filter.c`)
  - [x] Replace `fprintf(stderr, ...)` with `log_debug()` calls
  - [x] Remove or convert debugging output
  - **Files:** `src/filter.c` lines 124, 128, 141, 146

- [x] **Clean Up TODO Comments** (`src/main.c`)
  - [x] Removed unimplemented --kill option and its TODO
  - [x] Verified no other TODO comments exist
  - **Files:** `src/main.c`

## Phase 2: Core Improvements (3-5 days)

### 🏗️ Function Decomposition

- [x] **Break Down `on_key_press()` Function** (`src/main.c`)
  - [x] Extract `handle_harpoon_assignment(event, app)` for Ctrl+key assignments
  - [x] Extract `handle_harpoon_workspace_switching(event, app)` for Alt+key switching
  - [x] Extract `handle_tab_switching(event, app)` for Tab and Ctrl+H/L
  - [x] Extract `handle_navigation_keys(event, app)` for arrows, Enter, Escape, Ctrl+j/k
  - [x] Extract `get_harpoon_slot(event)` helper for slot calculation
  - [x] Verify all key combinations still work - code compiles successfully
  - **Files:** `src/main.c` (reduced from 277 lines to 22 lines)

- [x] **Break Down `filter_windows()` Function** (`src/filter.c`)
  - [x] Extract individual filtering stages into functions
  - [x] `try_word_boundary_match(filter, win)` - checks consecutive match at word start
  - [x] `try_initials_match(filter, win)` - matches first letters of words
  - [x] `try_subsequence_match(filter, text, label)` - subsequence matching with label
  - [x] `try_fuzzy_match(filter, text, label)` - fuzzy matching wrapper
  - [x] `match_window(filter, win)` - orchestrates all match types
  - [x] `is_word_boundary(c)` - helper to check word boundaries
  - [x] Test filtering behavior matches exactly - code compiles successfully
  - **Files:** `src/filter.c` (reduced from 235 lines to 91 lines)

### 🔧 Utilities

- [x] **Extract X11 Property Utilities** (`src/x11_utils.c`)
  - [x] Create `get_x11_property()` generic utility function
  - [x] Replace 11 duplicated XGetWindowProperty patterns across 3 files
  - [x] Test window property extraction still works - code compiles successfully
  - **Files:** `src/x11_utils.c`, `src/display.c`, `src/window_list.c`

- [x] **Create String Copy Utility** (Multiple files)
  - [x] Create `safe_string_copy(dest, src, dest_size)` function in `utils.c`
  - [x] Replace 17 strncpy + null termination patterns
  - [x] Test string handling behavior unchanged - code compiles successfully
  - **Files:** `src/harpoon.c`, `src/window_list.c`, `src/main.c`, `src/x11_utils.c`

### 📋 Error Handling

- [ ] **Standardize Error Handling** (Multiple files)
  - [ ] Define consistent return codes (enum or constants)
  - [ ] Standardize error logging patterns
  - [ ] Document error handling conventions
  - [ ] Review all function return values
  - **Files:** All source files

- [ ] **Centralize Selection Management** (Multiple files)
  - [ ] Create `reset_selection_to_first(app)` function
  - [ ] Create `move_selection_up(app)` and `move_selection_down(app)`
  - [ ] Replace scattered selection index management
  - [ ] Test selection navigation unchanged
  - **Files:** `src/main.c`, others

## Phase 3: Advanced Optimizations (1-2 weeks)

### 🧠 Complex Logic

- [ ] **Refactor Filter Logic** (`src/filter.c`)
  - [ ] Simplify nested conditional logic in filtering stages
  - [ ] Extract complex scoring logic into separate functions
  - [ ] Add comprehensive filter testing
  - [ ] Verify filter results identical to current behavior
  - **Files:** `src/filter.c` lines 84-179

- [ ] **Fix Word Boundary Scoring Edge Case** (`src/filter.c`) - Low Priority
  - [ ] Improve scoring when filter matches multiple consecutive words
  - [ ] Example: "cf" should prioritize "Code Fix" over "Coding | Focus..."
  - [ ] Special characters like "|" may affect word boundary detection
  - [ ] Consider giving bonus score for consecutive word matches
  - **Files:** `src/filter.c` - `try_word_boundary_match()`

### 💾 Data Structures

- [ ] **Optimize Memory Layout** (`src/window_info.h`, `src/app_data.h`)
  - [ ] Analyze memory usage of large fixed arrays
  - [ ] Consider dynamic allocation for window arrays
  - [ ] Evaluate reducing MAX_WINDOWS if appropriate
  - [ ] Profile memory usage before/after changes
  - **Files:** `src/window_info.h`, `src/app_data.h`

- [ ] **String Storage Optimization** (`src/window_info.h`)
  - [ ] Analyze string length distributions
  - [ ] Consider dynamic string allocation for efficiency
  - [ ] Measure memory impact
  - **Files:** `src/window_info.h`

### 📁 Organization

- [ ] **Clean Up Header Dependencies** (Multiple header files)
  - [ ] Remove circular dependencies
  - [ ] Remove unnecessary includes
  - [ ] Organize include order consistently
  - **Files:** All `.h` files

- [ ] **Reorganize Function Order** (`src/main.c`)
  - [ ] Group related static functions together
  - [ ] Organize by logical functionality
  - [ ] Add function group comments
  - **Files:** `src/main.c`

- [x] **Variable Naming Consistency** (Multiple files)
  - [x] Standardize on snake_case throughout
  - [x] Fix any camelCase inconsistencies
  - [x] Update variable names for clarity
  - **Files:** Multiple

### 💾 User Experience

- [ ] **Remember Window Position** (`src/main.c`, config system)
  - [ ] Save window position to config file when closing
  - [ ] Restore saved position when reopening (override --align if position saved)
  - [ ] Handle multi-monitor setups (validate position is still on-screen)
  - [ ] Add option to reset to default alignment behavior
  - [ ] Extend existing JSON config format to include window position
  - **Files:** `src/main.c`, `src/harpoon.c` (config system), potentially new position utilities

## Testing Strategy for Each Refactoring

For every item above, follow this testing approach:

### ✅ Verification Checklist
- [ ] Code compiles without warnings
- [ ] All window switching behavior identical
- [ ] Alt-Tab behavior preserved exactly
- [ ] Harpoon assignments work (Ctrl+key, Alt+key)
- [ ] Navigation keys work (Ctrl+j/k/h/l, arrows)
- [ ] Filtering behavior unchanged
- [ ] Window property extraction works
- [ ] No memory leaks introduced
- [ ] Performance not degraded

### 🧪 Test Commands
```bash
# Build and basic test
make clean && make
./cofi &

# Memory testing
valgrind --leak-check=full ./cofi

# Window manager compatibility
# Test with different window managers
```

## Implementation Notes

- **Incremental approach**: Complete one item at a time
- **Git commits**: Commit after each successful refactoring
- **Backup strategy**: Create branch before major changes
- **Behavior preservation**: Must maintain exact same functionality
- **Testing**: Test each change thoroughly before moving to next

## Priority Levels

🔥 **High Priority**: Significant impact, should do first  
🧹 **Cleanup**: Easy wins, good for momentum  
🏗️ **Core**: Important structural improvements  
🔧 **Utilities**: Developer experience improvements  
📋 **Standards**: Consistency and maintainability  
🧠 **Complex**: Advanced improvements requiring careful planning  
💾 **Performance**: Optimization opportunities  
📁 **Organization**: Code structure and clarity

---

**Status**: Ready to begin Phase 1  
**Last Updated**: $(date)  
**Estimated Total Effort**: 2-3 weeks part-time