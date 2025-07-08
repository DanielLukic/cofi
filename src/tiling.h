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
    TILE_CENTER_THIRD,   // Shift+C - Center window at 1/3 size
    TILE_CENTER_TWO_THIRDS,  // Ctrl+C - Center window at 2/3 size
    TILE_CENTER_THREE_QUARTERS, // Ctrl+Shift+C - Center window at 3/4 size
    TILE_GRID_1_NARROW,  // Shift+1 - Grid 1 at 1/3 width
    TILE_GRID_2_NARROW,  // Shift+2 - Grid 2 at 1/3 width
    TILE_GRID_3_NARROW,  // Shift+3 - Grid 3 at 1/3 width
    TILE_GRID_4_NARROW,  // Shift+4 - Grid 4 at 1/3 width
    TILE_GRID_5_NARROW,  // Shift+5 - Grid 5 at 1/3 width
    TILE_GRID_6_NARROW,  // Shift+6 - Grid 6 at 1/3 width
    TILE_GRID_7_NARROW,  // Shift+7 - Grid 7 at 1/3 width
    TILE_GRID_8_NARROW,  // Shift+8 - Grid 8 at 1/3 width
    TILE_GRID_9_NARROW,  // Shift+9 - Grid 9 at 1/3 width
    TILE_GRID_1_WIDE,    // Ctrl+1 - Grid 1 at 3/2 width
    TILE_GRID_2_WIDE,    // Ctrl+2 - Grid 2 at 3/2 width
    TILE_GRID_3_WIDE,    // Ctrl+3 - Grid 3 at 3/2 width
    TILE_GRID_4_WIDE,    // Ctrl+4 - Grid 4 at 3/2 width
    TILE_GRID_5_WIDE,    // Ctrl+5 - Grid 5 at 3/2 width
    TILE_GRID_6_WIDE,    // Ctrl+6 - Grid 6 at 3/2 width
    TILE_GRID_7_WIDE,    // Ctrl+7 - Grid 7 at 3/2 width
    TILE_GRID_8_WIDE,    // Ctrl+8 - Grid 8 at 3/2 width
    TILE_GRID_9_WIDE,    // Ctrl+9 - Grid 9 at 3/2 width
    TILE_GRID_1_WIDER,   // Ctrl+Shift+1 - Grid 1 at 4/3 width
    TILE_GRID_2_WIDER,   // Ctrl+Shift+2 - Grid 2 at 4/3 width
    TILE_GRID_3_WIDER,   // Ctrl+Shift+3 - Grid 3 at 4/3 width
    TILE_GRID_4_WIDER,   // Ctrl+Shift+4 - Grid 4 at 4/3 width
    TILE_GRID_5_WIDER,   // Ctrl+Shift+5 - Grid 5 at 4/3 width
    TILE_GRID_6_WIDER,   // Ctrl+Shift+6 - Grid 6 at 4/3 width
    TILE_GRID_7_WIDER,   // Ctrl+Shift+7 - Grid 7 at 4/3 width
    TILE_GRID_8_WIDER,   // Ctrl+Shift+8 - Grid 8 at 4/3 width
    TILE_GRID_9_WIDER,   // Ctrl+Shift+9 - Grid 9 at 4/3 width
    TILE_FULLSCREEN,     // F - Fullscreen toggle
    TILE_CENTER          // C - Center window (no resize)
} TileOption;

// Apply tiling to window
void apply_tiling(Display *display, Window window_id, TileOption option, int tile_columns);

#endif // TILING_H
