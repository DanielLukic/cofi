#ifndef WORKSPACE_RENAME_OVERLAY_H
#define WORKSPACE_RENAME_OVERLAY_H

#include <gtk/gtk.h>

// Forward declaration
typedef struct AppData AppData;

// Create workspace rename overlay content
void create_workspace_rename_overlay_content(GtkWidget *parent_container, AppData *app, int workspace_index);

// Handle key press in workspace rename overlay
gboolean handle_workspace_rename_key_press(AppData *app, guint keyval);

#endif // WORKSPACE_RENAME_OVERLAY_H