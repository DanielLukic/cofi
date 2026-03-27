#include "workspace_utils.h"
#include "x11_utils.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Get workspace count with maximum limit applied
int get_limited_workspace_count(Display *display) {
    int workspace_count = get_number_of_desktops(display);
    if (workspace_count > MAX_WORKSPACE_COUNT) {
        workspace_count = MAX_WORKSPACE_COUNT;
    }
    return workspace_count;
}

// Get workspace names with memory management
WorkspaceNames* get_workspace_names(Display *display) {
    WorkspaceNames *ws_names = malloc(sizeof(WorkspaceNames));
    if (!ws_names) {
        log_error("Failed to allocate WorkspaceNames structure");
        return NULL;
    }
    
    ws_names->names = get_desktop_names(display, &ws_names->count);
    return ws_names;
}

// Free workspace names structure
void free_workspace_names(WorkspaceNames *ws_names) {
    if (!ws_names) return;
    
    if (ws_names->names) {
        for (int i = 0; i < ws_names->count; i++) {
            free(ws_names->names[i]);
        }
        free(ws_names->names);
    }
    free(ws_names);
}

// Get workspace name or generate default
const char* get_workspace_name_or_default(WorkspaceNames *names, int index) {
    static char default_name[64];
    
    if (names && names->names && index < names->count && names->names[index]) {
        return names->names[index];
    }
    
    snprintf(default_name, sizeof(default_name), "Workspace %d", index + 1);
    return default_name;
}

// Create arrow diamond showing directional navigation keys
// Matches tiling_overlay.c diamond: 40x30 cells, 5px vertical gap, 50px horizontal gap
static void create_arrow_diamond(GtkWidget *parent_container) {
    GtkWidget *diamond = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_halign(diamond, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(parent_container), diamond, FALSE, FALSE, 5);

    // ↑
    GtkWidget *up = gtk_label_new("K");
    gtk_widget_set_halign(up, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(up, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(up, 50, 36);
    gtk_style_context_add_class(gtk_widget_get_style_context(up), "grid-cell");
    gtk_box_pack_start(GTK_BOX(diamond), up, FALSE, FALSE, 0);

    // ← →
    GtkWidget *mid = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 50);
    gtk_widget_set_halign(mid, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(diamond), mid, FALSE, FALSE, 0);

    GtkWidget *left = gtk_label_new("H");
    gtk_widget_set_halign(left, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(left, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(left, 50, 36);
    gtk_style_context_add_class(gtk_widget_get_style_context(left), "grid-cell");

    GtkWidget *right = gtk_label_new("L");
    gtk_widget_set_halign(right, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(right, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(right, 50, 36);
    gtk_style_context_add_class(gtk_widget_get_style_context(right), "grid-cell");

    gtk_box_pack_start(GTK_BOX(mid), left, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mid), right, FALSE, FALSE, 0);

    // ↓
    GtkWidget *down = gtk_label_new("J");
    gtk_widget_set_halign(down, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(down, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(down, 50, 36);
    gtk_style_context_add_class(gtk_widget_get_style_context(down), "grid-cell");
    gtk_box_pack_start(GTK_BOX(diamond), down, FALSE, FALSE, 0);
}

// Create standard workspace instructions widget
GtkWidget* create_workspace_instructions(GtkWidget *parent_container) {
    GtkWidget *instructions = gtk_label_new("[Press 1-9, 0 for workspace 10, HJKL for adjacent, Esc to cancel]");
    gtk_widget_set_halign(instructions, GTK_ALIGN_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(instructions), TRUE);
    gtk_box_pack_end(GTK_BOX(parent_container), instructions, FALSE, FALSE, 0);
    return instructions;
}

// Create horizontal layout: arrow diamond on left | separator | workspace content on right
// Returns the right-side container for the caller to add workspace grid into
GtkWidget* create_workspace_layout_with_arrows(GtkWidget *parent) {
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 40);
    gtk_box_pack_start(GTK_BOX(parent), hbox, TRUE, TRUE, 20);

    // Left: label + arrow diamond
    GtkWidget *left_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_widget_set_halign(left_box, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(hbox), left_box, TRUE, TRUE, 10);

    GtkWidget *label = gtk_label_new("<b>Direction</b>");
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(left_box), label, FALSE, FALSE, 5);

    create_arrow_diamond(left_box);

    // Vertical separator
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(hbox), sep, FALSE, FALSE, 0);

    // Right: returned for caller to fill with workspace grid
    GtkWidget *right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_widget_set_halign(right_box, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(hbox), right_box, TRUE, TRUE, 10);

    return right_box;
}


// Parse workspace number from keyboard event
int parse_workspace_number_key(GdkEventKey *event) {
    // Handle number keys 1-9
    if (event->keyval >= GDK_KEY_1 && event->keyval <= GDK_KEY_9) {
        return event->keyval - GDK_KEY_1 + 1;  // Return 1-9
    }
    
    // Handle 0 for workspace 10
    if (event->keyval == GDK_KEY_0) {
        return 10;
    }
    
    return -1;  // Not a workspace number key
}

// Get workspace index from key event
int get_workspace_index_from_key(GdkEventKey *event, int workspace_count) {
    int workspace_num = parse_workspace_number_key(event);
    if (workspace_num == -1) {
        return -1;  // Not a workspace key
    }

    if (workspace_num > workspace_count) {
        return -1;  // Workspace doesn't exist
    }

    return workspace_num - 1;  // Convert to 0-based index
}

// Map arrow key to direction: 0=left, 1=right, 2=up, 3=down. Returns -1 if not arrow.
static int arrow_key_to_direction(guint keyval) {
    switch (keyval) {
        case GDK_KEY_Left:  case GDK_KEY_h: return 0;
        case GDK_KEY_Right: case GDK_KEY_l: return 1;
        case GDK_KEY_Up:    case GDK_KEY_k: return 2;
        case GDK_KEY_Down:  case GDK_KEY_j: return 3;
        default: return -1;
    }
}

// Map direction string to direction: 0=left, 1=right, 2=up, 3=down. Returns -1 if invalid.
static int string_to_direction(const char *arg) {
    if (!arg) return -1;
    if (strcmp(arg, "left")  == 0 || strcmp(arg, "h") == 0) return 0;
    if (strcmp(arg, "right") == 0 || strcmp(arg, "l") == 0) return 1;
    if (strcmp(arg, "up")    == 0 || strcmp(arg, "k") == 0) return 2;
    if (strcmp(arg, "down")  == 0 || strcmp(arg, "j") == 0) return 3;
    return -1;
}

// Compute adjacent workspace given direction and grid geometry.
// Returns 0-based index or -1 if at edge (no wrap).
static int adjacent_workspace(int current, int direction, int count, int per_row) {
    if (per_row <= 0) per_row = count; // linear layout = one row

    int col = current % per_row;
    int row = current / per_row;
    int rows = (count + per_row - 1) / per_row;

    switch (direction) {
        case 0: col--; break; // left
        case 1: col++; break; // right
        case 2: row--; break; // up
        case 3: row++; break; // down
    }

    if (col < 0 || col >= per_row || row < 0 || row >= rows)
        return -1; // at edge

    int target = row * per_row + col;
    return (target >= 0 && target < count) ? target : -1;
}

int resolve_workspace_from_key(Display *display, GdkEventKey *event, int workspaces_per_row) {
    int count = get_limited_workspace_count(display);

    // Try number key first
    int target = get_workspace_index_from_key(event, count);
    if (target >= 0) return target;

    // Try arrow / hjkl
    int dir = arrow_key_to_direction(event->keyval);
    if (dir >= 0) {
        int current = get_current_desktop(display);
        return adjacent_workspace(current, dir, count, workspaces_per_row);
    }

    return -1;
}

int resolve_workspace_from_arg(Display *display, const char *arg, int workspaces_per_row) {
    if (!arg || arg[0] == '\0') return -1;

    // Try direction string
    int dir = string_to_direction(arg);
    if (dir >= 0) {
        int count = get_limited_workspace_count(display);
        int current = get_current_desktop(display);
        return adjacent_workspace(current, dir, count, workspaces_per_row);
    }

    // Try number
    int num = atoi(arg);
    if (num >= 1 && num <= 36) {
        int count = get_limited_workspace_count(display);
        if (num <= count) return num - 1;
    }

    return -1;
}