#ifndef TILING_DIALOG_H
#define TILING_DIALOG_H

#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include "window_info.h"

// Forward declaration
struct AppData;

// Tiling options
typedef enum {
    TILE_LEFT_HALF,      // L - Tile left half
    TILE_RIGHT_HALF,     // R - Tile right half  
    TILE_TOP_HALF,       // T - Tile top half
    TILE_BOTTOM_HALF,    // B - Tile bottom half
    TILE_GRID_1,         // 1 - Top-left (3x3 grid)
    TILE_GRID_2,         // 2 - Top-center (3x3 grid)
    TILE_GRID_3,         // 3 - Top-right (3x3 grid)
    TILE_GRID_4,         // 4 - Middle-left (3x3 grid)
    TILE_GRID_5,         // 5 - Middle-center (3x3 grid)
    TILE_GRID_6,         // 6 - Middle-right (3x3 grid)
    TILE_GRID_7,         // 7 - Bottom-left (3x3 grid)
    TILE_GRID_8,         // 8 - Bottom-center (3x3 grid)
    TILE_GRID_9,         // 9 - Bottom-right (3x3 grid)
    TILE_FULLSCREEN,     // F - Fullscreen toggle
    TILE_CENTER          // C - Center window (no resize)
} TileOption;

// Tiling dialog structure
typedef struct {
    GtkWidget *window;
    GtkWidget *content_box;
    WindowInfo *target_window;
    Display *display;
    struct AppData *app_data;
    gboolean option_selected;
} TilingDialog;

// Show the tiling dialog
void show_tiling_dialog(struct AppData *app);

// Apply tiling to window
void apply_tiling(Display *display, Window window_id, TileOption option);

#endif // TILING_DIALOG_H
