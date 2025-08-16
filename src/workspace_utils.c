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

// Create standard workspace instructions widget
GtkWidget* create_workspace_instructions(GtkWidget *parent_container) {
    GtkWidget *instructions = gtk_label_new("[Press 1-9, 0 for workspace 10, Esc to cancel]");
    gtk_widget_set_halign(instructions, GTK_ALIGN_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(instructions), TRUE);
    gtk_box_pack_end(GTK_BOX(parent_container), instructions, FALSE, FALSE, 0);
    return instructions;
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