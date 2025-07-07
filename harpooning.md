# Harpoon Tab Implementation Plan

## Overview

This document outlines the implementation plan for adding a third "Harpoon" tab to COFI, allowing users to view, edit, and manage their harpoon window assignments.

## Architecture Changes

### 1. Tab System Extension

**Enum Update** (`app_data.h`):
```c
typedef enum {
    TAB_WINDOWS,
    TAB_WORKSPACES,
    TAB_HARPOON
} TabMode;
```

**Data Structure Updates** (`app_data.h`):
```c
typedef struct {
    // ... existing fields ...
    
    // Harpoon tab data
    HarpoonSlot filtered_harpoon[MAX_HARPOON_SLOTS];
    int filtered_harpoon_count;
    
    // Selection state
    struct {
        int window_index;
        int workspace_index;
        int harpoon_index;  // New field
    } selection;
    
    // Edit state for harpoon
    struct {
        gboolean editing;
        int editing_slot;
        char edit_buffer[MAX_TITLE_LEN];
    } harpoon_edit;
    
    // Delete confirmation state
    struct {
        gboolean pending_delete;
        int delete_slot;
    } harpoon_delete;
} AppData;
```

### 2. Display Format for Harpoon Tab

```
Slot  Title                                           Class         Instance     Type
─────────────────────────────────────────────────────────────────────────────────
  0   * EMPTY *                                      -             -            -
> 1   Firefox - Page Title                           firefox       Navigator    Normal
  2   Terminal - ~/Projects                          gnome-term    Terminal     Normal  
  3   * EMPTY *                                      -             -            -
  4   Slack - Workspace Name                         slack         slack        Normal
  5   Meet - *                                       chrome        Chrome       Normal
  6   * EMPTY *                                      -             -            -
  7   * EMPTY *                                      -             -            -
  8   * EMPTY *                                      -             -            -
  9   * EMPTY *                                      -             -            -
  a   VS Code - project.c                            code          Code         Normal
  b   Thunderbird - Inbox                            thunderbird   Mail         Normal
  c   * EMPTY *                                      -             -            -
  ...
  z   * EMPTY *                                      -             -            -
─────────────────────────────────────────────────────────────────────────────────
[ Windows ]   [ Workspaces ]   [ HARPOON ]
```

## Implementation Steps

### Step 1: Add Basic Harpoon Tab

**Files to modify:**
- `src/app_data.h` - Add TAB_HARPOON enum value
- `src/cli_args.cpp` - Add --harpoon command line argument
- `src/main.c` - Update tab switching logic and instance management
- `src/display.c` - Add harpoon display formatting
- `src/app_init.c` - Initialize harpoon tab data

**Command Line Argument:**
```cpp
// In cli_args.cpp
// Add to help text (around line 26)
"  --harpoon              Start with the Harpoon tab active\n"

// Add switch option (around line 76)
auto harpoon_option = popl::Switch("", "harpoon", "Start with the Harpoon tab active");
parser.add(harpoon_option);

// Handle the option (around line 149)
if (harpoon_option.is_set()) {
    app->current_tab = TAB_HARPOON;
}
```

**Instance Management Update:**
```c
// In main.c (around line 615)
typedef enum {
    SHOW_MODE_WINDOWS,
    SHOW_MODE_WORKSPACES,
    SHOW_MODE_HARPOON  // New mode
} ShowMode;

// Update check_existing_instance logic
ShowMode mode;
if (app->current_tab == TAB_WORKSPACES) {
    mode = SHOW_MODE_WORKSPACES;
} else if (app->current_tab == TAB_HARPOON) {
    mode = SHOW_MODE_HARPOON;
} else {
    mode = SHOW_MODE_WINDOWS;
}
```

**Key functions to add:**
```c
// In display.c
void format_harpoon_list(AppData *app, GString *output);
void display_harpoon_slot(HarpoonSlot *slot, int slot_number, 
                         gboolean selected, GString *output);

// In main.c  
void handle_harpoon_tab_keys(AppData *app, guint keyval, 
                            GdkModifierType state);
```

**Display logic:**
- Show all 36 harpoon slots (0-9, a-z excluding h,j,k,l,u)
- Empty slots shown as "* EMPTY *"
- Assigned slots show window information
- Tab header updated to show three tabs

**Initialization Update:**
```c
// In app_init.c
void init_app_data(AppData *app) {
    // ... existing code ...
    
    // Preserve current_tab if set by CLI args
    TabMode saved_tab = app->current_tab;
    
    // ... initialization ...
    
    // Restore tab selection from CLI
    if (saved_tab == TAB_WORKSPACES || saved_tab == TAB_HARPOON) {
        app->current_tab = saved_tab;
    } else {
        app->current_tab = TAB_WINDOWS;
    }
}

### Step 2: Keyboard Navigation

**Navigation keys:**
- `Ctrl+J` / `Down Arrow` - Move selection down
- `Ctrl+K` / `Up Arrow` - Move selection up
- Standard vim navigation preserved (j/k without Ctrl)

**Implementation:**
```c
// In main.c
case GDK_KEY_j:
    if (state & GDK_CONTROL_MASK && app->current_tab == TAB_HARPOON) {
        move_harpoon_selection_down(app);
        return TRUE;
    }
    break;
```

### Step 3: Delete Functionality

**Delete flow:**
1. Press `d` - Enter delete mode, show confirmation prompt
2. Press `d` or `y` - Confirm delete
3. Press `n` or `Escape` - Cancel delete

**Visual feedback:**
```
> 2   Terminal - ~/Projects          [DELETE? y/n]
```

**Implementation:**
```c
// In main.c
void handle_harpoon_delete(AppData *app, guint keyval) {
    if (!app->harpoon_delete.pending_delete) {
        // First 'd' press
        app->harpoon_delete.pending_delete = TRUE;
        app->harpoon_delete.delete_slot = get_selected_harpoon_slot(app);
    } else {
        // Second key press
        if (keyval == GDK_KEY_d || keyval == GDK_KEY_y) {
            unassign_slot(&app->harpoon, app->harpoon_delete.delete_slot);
            save_harpoon_config(&app->harpoon);
        }
        app->harpoon_delete.pending_delete = FALSE;
    }
    refresh_display(app);
}
```

### Step 4: Wildcard Support

**Pattern matching rules:**
- `*` - Matches zero or more characters
- `.` - Matches exactly one character
- Patterns work in window titles only

**Implementation approach:**
1. **Input sanitization** - Replace existing `*` with `.` when saving
2. **Pattern compilation** - Convert wildcards to regex pattern
3. **Matching engine** - Use POSIX regex (`<regex.h>`) or simple custom matcher

**Custom matcher implementation (if avoiding regex):**
```c
gboolean match_pattern(const char *pattern, const char *text) {
    const char *p = pattern;
    const char *t = text;
    const char *star_p = NULL;
    const char *star_t = NULL;
    
    while (*t) {
        if (*p == '*') {
            star_p = p++;
            star_t = t;
        } else if (*p == '.' || *p == *t) {
            p++;
            t++;
        } else if (star_p) {
            p = star_p + 1;
            t = ++star_t;
        } else {
            return FALSE;
        }
    }
    
    while (*p == '*') p++;
    return !*p;
}
```

**Window matching update:**
```c
// In window_matcher.c
gboolean fuzzy_match_window(HarpoonSlot *slot, WindowInfo *window) {
    // Exact class, instance, type match required
    if (strcmp(slot->class_name, window->class_name) != 0 ||
        strcmp(slot->instance, window->instance) != 0 ||
        strcmp(slot->type, window->type) != 0) {
        return FALSE;
    }
    
    // Check if title contains wildcards
    if (strchr(slot->title, '*') || strchr(slot->title, '.')) {
        return match_pattern(slot->title, window->title);
    }
    
    // Fall back to existing fuzzy match logic
    return existing_fuzzy_match(slot->title, window->title);
}
```

### Step 5: Overlay-based Editing

**Edit flow:**
1. Press `Ctrl+E` on a slot - Show edit overlay dialog
2. GTK Entry widget pre-filled with current title
3. `Enter` - Save changes
4. `Escape` - Cancel edit

**Visual Design:**
```
┌─────────────────────────────────────────┐
│       Edit Harpoon Slot: m              │
├─────────────────────────────────────────┤
│                                         │
│  Title: [Terminal*________________]     │
│                                         │
├─────────────────────────────────────────┤
│  Press Enter to save, Escape to cancel  │
└─────────────────────────────────────────┘
```

**Why Overlay Manager:**
- **Clean focus handling** - Overlay captures all input, no keyboard interception needed
- **Proper GTK Entry** - Real text widget with cursor, selection, copy/paste support
- **UI consistency** - Matches delete confirmation overlay pattern
- **Simpler implementation** - No custom text editing logic needed

**Implementation:**
```c
// In app_data.h
typedef enum {
    OVERLAY_NONE,
    OVERLAY_TILING,
    OVERLAY_WORKSPACE_MOVE,
    OVERLAY_WORKSPACE_JUMP,
    OVERLAY_HARPOON_DELETE,
    OVERLAY_HARPOON_EDIT  // New overlay type
} OverlayType;

// In overlay_manager.c
static void create_harpoon_edit_overlay_content(GtkWidget *parent_container, AppData *app, int slot_index) {
    HarpoonSlot *slot = &app->harpoon.slots[slot_index];
    
    // Header
    char slot_name[4];
    format_slot_name(slot_index, slot_name);
    char *header = g_strdup_printf("Edit Harpoon Slot: %s", slot_name);
    GtkWidget *header_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(header_label), header);
    g_free(header);
    
    // Entry widget
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), slot->title);
    gtk_entry_set_max_length(GTK_ENTRY(entry), MAX_TITLE_LEN - 1);
    gtk_widget_set_size_request(entry, 400, -1);
    
    // Store entry reference for key handler
    g_object_set_data(G_OBJECT(parent_container), "edit-entry", entry);
    
    // Instructions
    GtkWidget *instructions = gtk_label_new("Press Enter to save, Escape to cancel");
    
    // Layout
    gtk_box_pack_start(GTK_BOX(parent_container), header_label, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(parent_container), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(parent_container), entry, FALSE, FALSE, 20);
    gtk_box_pack_start(GTK_BOX(parent_container), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(parent_container), instructions, FALSE, FALSE, 10);
    
    // Focus the entry
    gtk_widget_grab_focus(entry);
}

// Handle key press in edit overlay
static gboolean handle_harpoon_edit_key_press(AppData *app, GdkEventKey *event) {
    GtkWidget *entry = g_object_get_data(G_OBJECT(app->dialog_container), "edit-entry");
    
    if (event->keyval == GDK_KEY_Return) {
        // Save the edited title
        const char *new_title = gtk_entry_get_text(GTK_ENTRY(entry));
        int slot_index = app->harpoon_edit.editing_slot;
        
        // Update slot title directly (no wildcard conversion needed here)
        safe_string_copy(app->harpoon.slots[slot_index].title, new_title, MAX_TITLE_LEN);
        save_harpoon_slots(&app->harpoon);
        
        log_info("USER: Edited harpoon slot %d title to: %s", slot_index, new_title);
        
        // Update display
        if (app->current_tab == TAB_HARPOON) {
            const char *filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
            gtk_entry_set_text(GTK_ENTRY(app->entry), "");
            gtk_entry_set_text(GTK_ENTRY(app->entry), filter);
        }
        
        hide_overlay(app);
        return TRUE;
    }
    
    // Let GTK Entry handle all other keys
    return FALSE;
}

// In main.c - handle Ctrl+E
if (event->keyval == GDK_KEY_e && (event->state & GDK_CONTROL_MASK)) {
    if (app->selection.harpoon_index < app->filtered_harpoon_count) {
        HarpoonSlot *slot = &app->filtered_harpoon[app->selection.harpoon_index];
        
        // Only allow edit if slot is assigned
        if (slot->assigned) {
            int actual_slot = app->filtered_harpoon_indices[app->selection.harpoon_index];
            app->harpoon_edit.editing_slot = actual_slot;
            show_harpoon_edit_overlay(app, actual_slot);
            return TRUE;
        }
    }
}
```

## File Structure Updates

### New files:
- `src/harpoon_tab.c` - Harpoon tab display and interaction logic
- `src/pattern_match.c` - Wildcard pattern matching implementation

### Modified files:
- `src/app_data.h` - Add new data structures
- `src/main.c` - Tab switching and key handling
- `src/display.c` - Add harpoon display formatting
- `src/window_matcher.c` - Update matching logic for wildcards
- `src/harpoon.c` - Add title sanitization

## Testing Strategy

1. **Tab switching** - Verify smooth transition between all three tabs
2. **Navigation** - Test Ctrl+J/K and arrow keys
3. **Delete confirmation** - Test dd, dy, dn combinations
4. **Wildcard matching** - Test various patterns:
   - `Firefox - *` matches any Firefox window
   - `Terminal - ~/Projects/*` matches project terminals
   - `Meet - .* Meeting` matches meeting windows
5. **Edit mode** - Test title editing and sanitization
6. **Persistence** - Verify changes saved to config file

## Implementation Order

1. **Phase 1** (Step 1): Basic tab with display
2. **Phase 2** (Step 2): Navigation support
3. **Phase 3** (Step 3): Delete functionality  
4. **Phase 4** (Step 4): Wildcard support
5. **Phase 5** (Step 5): Inline editing

Each phase should be a separate commit with tests.

## UI/UX Considerations

1. **Visual consistency** - Match existing tab styling
2. **Feedback** - Clear indicators for edit/delete modes
3. **Performance** - Efficient pattern matching for large window lists
4. **Error handling** - Graceful handling of invalid patterns
5. **Undo** - Consider adding undo for destructive operations

## Configuration Updates

The existing `~/.config/cofi_harpoon.json` format remains unchanged, ensuring backward compatibility. Wildcard patterns are stored directly in the title field.