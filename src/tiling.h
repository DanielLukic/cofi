#ifndef TILING_H
#define TILING_H

#include <X11/Xlib.h>

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

// Apply tiling to window
void apply_tiling(Display *display, Window window_id, TileOption option, int tile_columns);

#endif // TILING_H
