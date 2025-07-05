#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "gtk_window.h"
#include "log.h"


void apply_window_position(GtkWidget *window, WindowAlignment alignment) {
    switch (alignment) {
        case ALIGN_CENTER:
            gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
            break;
        case ALIGN_TOP:
        case ALIGN_TOP_LEFT:
        case ALIGN_TOP_RIGHT:
        case ALIGN_LEFT:
        case ALIGN_RIGHT:
        case ALIGN_BOTTOM:
        case ALIGN_BOTTOM_LEFT:
        case ALIGN_BOTTOM_RIGHT:
            gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_NONE);
            // Position will be set after window is realized
            break;
    }
}

// Callback to reposition window whenever size changes
void on_window_size_allocate(GtkWidget *window, GtkAllocation *allocation, gpointer user_data) {
    AppData *app = (AppData *)user_data;
    WindowAlignment alignment = app->alignment;
    
    // Only reposition if we have a valid size
    if (allocation->width <= 1 || allocation->height <= 1) {
        return;
    }
    GdkScreen *screen = gtk_window_get_screen(GTK_WINDOW(window));
    GdkDisplay *display = gdk_screen_get_display(screen);
    
    // Get mouse pointer position to determine current monitor
    gint mouse_x, mouse_y;
    gdk_display_get_pointer(display, NULL, &mouse_x, &mouse_y, NULL);
    
    // Get the monitor at mouse position
    gint monitor_num = gdk_screen_get_monitor_at_point(screen, mouse_x, mouse_y);
    GdkRectangle monitor_geometry;
    gdk_screen_get_monitor_geometry(screen, monitor_num, &monitor_geometry);
    
    // Use the allocation size
    gint window_width = allocation->width;
    gint window_height = allocation->height;
    
    log_debug("Repositioning window on size change: alignment=%d, size=%dx%d", 
              alignment, window_width, window_height);
    
    gint x = 0, y = 0;
    
    switch (alignment) {
        case ALIGN_TOP:
            x = monitor_geometry.x + (monitor_geometry.width - window_width) / 2;
            y = monitor_geometry.y;
            break;
        case ALIGN_TOP_LEFT:
            x = monitor_geometry.x;
            y = monitor_geometry.y;
            break;
        case ALIGN_TOP_RIGHT:
            x = monitor_geometry.x + monitor_geometry.width - window_width;
            y = monitor_geometry.y;
            break;
        case ALIGN_LEFT:
            x = monitor_geometry.x;
            y = monitor_geometry.y + (monitor_geometry.height - window_height) / 2;
            break;
        case ALIGN_RIGHT:
            x = monitor_geometry.x + monitor_geometry.width - window_width;
            y = monitor_geometry.y + (monitor_geometry.height - window_height) / 2;
            break;
        case ALIGN_BOTTOM:
            x = monitor_geometry.x + (monitor_geometry.width - window_width) / 2;
            y = monitor_geometry.y + monitor_geometry.height - window_height;
            break;
        case ALIGN_BOTTOM_LEFT:
            x = monitor_geometry.x;
            y = monitor_geometry.y + monitor_geometry.height - window_height;
            break;
        case ALIGN_BOTTOM_RIGHT:
            x = monitor_geometry.x + monitor_geometry.width - window_width;
            y = monitor_geometry.y + monitor_geometry.height - window_height;
            break;
        case ALIGN_CENTER:
        default:
            // This shouldn't happen for non-center alignments
            x = monitor_geometry.x + (monitor_geometry.width - window_width) / 2;
            y = monitor_geometry.y + (monitor_geometry.height - window_height) / 2;
            break;
    }
    
    log_debug("Monitor geometry: x=%d, y=%d, width=%d, height=%d",
              monitor_geometry.x, monitor_geometry.y, 
              monitor_geometry.width, monitor_geometry.height);
    log_debug("Calculated position: x=%d, y=%d", x, y);
    
    // Set gravity hint before moving
    gtk_window_set_gravity(GTK_WINDOW(window), GDK_GRAVITY_STATIC);
    gtk_window_move(GTK_WINDOW(window), x, y);
}