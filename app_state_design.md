# App State Management Design

## Current State Problems

1. State is scattered across AppData fields (current_tab, in_command_mode, overlay_manager, etc.)
2. State transitions happen in multiple places without coordination
3. Window recreation is the only reliable way to reset state
4. No clear separation between UI state and data state

## Proposed State System

### Core State Components

```c
typedef enum {
    MODE_NORMAL,      // Normal window/workspace/harpoon selection
    MODE_COMMAND,     // Command mode with ':' prompt
    MODE_OVERLAY      // Showing an overlay (tiling, workspace, etc.)
} AppMode;

typedef enum {
    TAB_WINDOWS,
    TAB_WORKSPACES,
    TAB_HARPOON
} TabState;

typedef struct {
    AppMode mode;
    TabState tab;
    OverlayType overlay_type;  // Only valid when mode == MODE_OVERLAY
    char search_filter[256];
    int selected_index;
    
    // Data state (doesn't change with UI transitions)
    WindowInfo *windows;
    WorkspaceInfo *workspaces;
    HarpoonSlot *harpoon_slots;
    // ... counts, etc.
} AppState;
```

### State Transition Functions

```c
// Primary state changes
void app_state_change_to_windows(AppState *state);
void app_state_change_to_workspaces(AppState *state);
void app_state_change_to_harpoon(AppState *state);
void app_state_change_to_command(AppState *state);
void app_state_show_overlay(AppState *state, OverlayType type);
void app_state_hide_overlay(AppState *state);

// State queries
bool app_state_is_command_mode(AppState *state);
bool app_state_is_overlay_active(AppState *state);
TabState app_state_get_tab(AppState *state);

// Filter/selection management
void app_state_set_filter(AppState *state, const char *filter);
void app_state_clear_filter(AppState *state);
void app_state_set_selection(AppState *state, int index);
void app_state_reset_selection(AppState *state);
```

## Implementation Strategy

### Phase 1: Create State Module
1. Define app_state.h with structures and function declarations
2. Implement app_state.c with state transition logic
3. Each transition function:
   - Updates state fields atomically
   - Triggers necessary UI updates
   - Logs state transitions for debugging

### Phase 2: Integrate with Existing Code
1. Add AppState to AppData
2. Replace direct field access with state functions
3. Update event handlers to use state transitions
4. Remove redundant state fields from AppData

### Phase 3: Window Reuse
1. Modify recreate_window_idle() to just update state
2. Add app_state_apply_to_ui() to sync UI with state
3. Keep window instance alive, update content based on state

## State Transition Examples

### Example: User presses Alt+Tab to show window
```c
// Current approach (destroys window):
recreate_window_idle() {
    destroy_window();
    setup_application();
    filter_windows("");
    show_window();
}

// New approach (reuses window):
show_window_with_state(ShowMode mode) {
    switch (mode) {
        case SHOW_MODE_WINDOWS:
            app_state_change_to_windows(&app->state);
            break;
        case SHOW_MODE_COMMAND:
            app_state_change_to_command(&app->state);
            break;
        // etc.
    }
    app_state_apply_to_ui(&app->state, app);
    gtk_widget_show(app->window);
    gtk_widget_grab_focus(app->entry);
}
```

### Example: User switches tabs
```c
// Instead of:
app->current_tab = TAB_WORKSPACES;
filter_workspaces(app, "");
update_display(app);

// Use:
app_state_change_to_workspaces(&app->state);
// This internally handles filter reset, selection reset, and UI update
```

## Benefits

1. **Clear state transitions**: All state changes go through defined functions
2. **Easier debugging**: Can log/trace all state transitions
3. **Window reuse**: State can be applied to existing UI
4. **Maintainability**: New features just add state transition functions
5. **Testing**: State transitions can be unit tested

## Risks and Mitigation

1. **Risk**: Large refactoring could introduce bugs
   - **Mitigation**: Implement incrementally, test each phase

2. **Risk**: Performance overhead from state abstraction
   - **Mitigation**: Keep state functions inline-able, measure performance

3. **Risk**: Complex state interactions
   - **Mitigation**: Document state invariants, add assertions