#include "geom/tiling.h"
#include "core/log/log.h"
#include "x11/x11_utils.h"
#include "x11/workarea.h"
#include "x11/size_hints.h"
#include "x11/monitor_move.h"
#include "x11/frame_extents.h"
#include "x11/xrandr_helpers.h"
#include <stdlib.h>
#include <string.h>

// Tiling calculation structure
typedef struct {
    int x, y;
    int width, height;
} TileGeometry;

// New helper functions
static void get_target_work_area(Display *display, Window window_id, WorkArea *work_area);
static void calculate_tile_geometry(TileOption option, const WorkArea *work_area, int tile_columns, TileGeometry *geometry);
static void apply_window_position(Display *display, Window window_id,
                                  const TileGeometry *geometry,
                                  const WorkArea *work_area,
                                  const WindowSizeHints *size_hints,
                                  TileOption option);

// Get the work area for the monitor containing the window
static void get_target_work_area(Display *display, Window window_id, WorkArea *work_area) {
    // Get current window geometry
    int window_x, window_y, window_width, window_height;
    if (!get_window_geometry(display, window_id, &window_x, &window_y, &window_width, &window_height)) {
        log_error("Failed to get window geometry for tiling");
        // Fallback to screen dimensions
        Screen *screen = DefaultScreenOfDisplay(display);
        work_area->x = 0;
        work_area->y = 0;
        work_area->width = WidthOfScreen(screen);
        work_area->height = HeightOfScreen(screen);
        return;
    }
    
    // Find which monitor the window is on
    int current_monitor_index = get_window_monitor_xrandr(display, window_x, window_y, window_width, window_height);
    
    // Get monitor information
    MonitorInfo *monitors;
    int monitor_count = get_monitors_xrandr(display, &monitors);
    
    int monitor_x, monitor_y, monitor_width, monitor_height;
    if (current_monitor_index >= 0 && current_monitor_index < monitor_count) {
        monitor_x = monitors[current_monitor_index].x;
        monitor_y = monitors[current_monitor_index].y;
        monitor_width = monitors[current_monitor_index].width;
        monitor_height = monitors[current_monitor_index].height;
        log_debug("Using monitor %d: %dx%d at (%d,%d)", current_monitor_index, 
                  monitor_width, monitor_height, monitor_x, monitor_y);
    } else {
        // Fallback to screen dimensions
        Screen *screen = DefaultScreenOfDisplay(display);
        monitor_x = 0;
        monitor_y = 0;
        monitor_width = WidthOfScreen(screen);
        monitor_height = HeightOfScreen(screen);
        log_debug("Using fallback screen dimensions: %dx%d", monitor_width, monitor_height);
    }
    
    free(monitors);
    
    // Get the actual work area (excluding panels/docks)
    WorkArea desktop_work_area;
    get_current_work_area(display, &desktop_work_area);
    
    // Intersect work area with monitor bounds
    int work_x = (desktop_work_area.x > monitor_x) ? desktop_work_area.x : monitor_x;
    int work_y = (desktop_work_area.y > monitor_y) ? desktop_work_area.y : monitor_y;
    int work_right = (desktop_work_area.x + desktop_work_area.width < monitor_x + monitor_width) ?
                     desktop_work_area.x + desktop_work_area.width : monitor_x + monitor_width;
    int work_bottom = (desktop_work_area.y + desktop_work_area.height < monitor_y + monitor_height) ?
                      desktop_work_area.y + desktop_work_area.height : monitor_y + monitor_height;
    
    work_area->x = work_x;
    work_area->y = work_y;
    work_area->width = work_right - work_x;
    work_area->height = work_bottom - work_y;
    
    log_debug("Work area on monitor: %dx%d+%d+%d", 
              work_area->width, work_area->height, work_area->x, work_area->y);
}

// Apply window position with size hints
static gboolean tile_anchors_right(TileOption option) {
    switch (option) {
        case TILE_RIGHT_HALF:
        case TILE_RIGHT_QUARTER:
        case TILE_RIGHT_TWO_THIRDS:
        case TILE_RIGHT_THREE_QUARTERS:
        case TILE_GRID_3:
        case TILE_GRID_6:
        case TILE_GRID_9:
            return TRUE;
        default:
            return FALSE;
    }
}

static gboolean tile_anchors_bottom(TileOption option) {
    switch (option) {
        case TILE_BOTTOM_HALF:
        case TILE_BOTTOM_QUARTER:
        case TILE_BOTTOM_TWO_THIRDS:
        case TILE_BOTTOM_THREE_QUARTERS:
        case TILE_GRID_7:
        case TILE_GRID_8:
        case TILE_GRID_9:
            return TRUE;
        default:
            return FALSE;
    }
}

static void apply_window_position(Display *display, Window window_id,
                                const TileGeometry *geometry, const WorkArea *work_area,
                                const WindowSizeHints *size_hints, TileOption option) {
    int x = geometry->x;
    int y = geometry->y;
    int width = geometry->width;
    int height = geometry->height;
    FrameExtents extents = {0};
    gboolean has_extents = get_frame_extents(display, window_id, &extents) &&
                           frame_extents_valid(&extents);
    FrameExtents gtk_extents = {0};
    gboolean has_gtk_extents = !has_extents &&
                               get_gtk_frame_extents(display, window_id, &gtk_extents) &&
                               frame_extents_valid(&gtk_extents);
    int frame_width = width;
    int frame_height = height;
    gboolean anchor_right = tile_anchors_right(option);
    gboolean anchor_bottom = tile_anchors_bottom(option);
    
    log_debug("Applying window position: x=%d, y=%d, width=%d, height=%d", x, y, width, height);
    
    if (has_extents) {
        width -= extents.left + extents.right;
        height -= extents.top + extents.bottom;
        if (width < 1) width = 1;
        if (height < 1) height = 1;
    } else if (has_gtk_extents) {
        x -= gtk_extents.left;
        y -= gtk_extents.top;
        width += gtk_extents.left + gtk_extents.right;
        height += gtk_extents.top + gtk_extents.bottom;
        log_debug("Adjusted for GTK CSD extents: left=%d, right=%d, top=%d, bottom=%d",
                  gtk_extents.left, gtk_extents.right,
                  gtk_extents.top, gtk_extents.bottom);
    }
    log_debug("After frame adjustment: width=%d, height=%d", width, height);

    // Enforce size hints
    ensure_size_hints_satisfied(&x, &y, &width, &height, (WindowSizeHints *)size_hints);
    log_debug("After size hints: x=%d, y=%d, width=%d, height=%d", x, y, width, height);

    if (has_extents) {
        frame_width = width + extents.left + extents.right;
        frame_height = height + extents.top + extents.bottom;
    } else if (has_gtk_extents) {
        frame_width = width;
        frame_height = height;
    } else {
        frame_width = width;
        frame_height = height;
    }

    if (anchor_right) {
        x = work_area->x + work_area->width - frame_width;
        if (has_gtk_extents)
            x += gtk_extents.right;
    }
    if (anchor_bottom) {
        y = work_area->y + work_area->height - frame_height;
        if (has_gtk_extents)
            y += gtk_extents.bottom;
    }

    // Move and resize the window; x,y are frame-space (from work area calculation).
    xmove_resize_frame_aware(display, window_id, x, y, width, height);
    XFlush(display);
}

// Apply tiling to window
void apply_tiling(Display *display, Window window_id, TileOption option, int tile_columns) {
    if (!display || !window_id) {
        log_error("Invalid display or window for tiling");
        return;
    }
    
    // Handle fullscreen toggle separately
    if (option == TILE_FULLSCREEN) {
        set_window_fullscreen(display, window_id, WINDOW_STATE_TOGGLE);
        log_info("Toggled fullscreen for window");
        return;
    }
    
    // Choose the monitor while the window is still where the user sees it.
    // Some WMs restore a maximized window to its previous normal monitor when
    // unmaximizing, which must not change the target of the tile command.
    WorkArea work_area;
    get_target_work_area(display, window_id, &work_area);

    // Unmaximize before placement so the WM does not fight the resize request.
    unmaximize_and_settle(display, window_id);
    
    // Get window size hints
    WindowSizeHints size_hints;
    get_window_size_hints(display, window_id, &size_hints);
    
    // Calculate tile geometry
    TileGeometry geometry;
    calculate_tile_geometry(option, &work_area, tile_columns, &geometry);
    
    // Apply the position and size
    apply_window_position(display, window_id, &geometry, &work_area, &size_hints, option);

    log_info("Applied tiling option %d to window", option);
}

// Calculate tile geometry based on tiling option
static void calculate_tile_geometry(TileOption option, const WorkArea *work_area, 
                                  int tile_columns, TileGeometry *geometry) {
    int work_x = work_area->x;
    int work_y = work_area->y;
    int work_width = work_area->width;
    int work_height = work_area->height;

    switch (option) {
        case TILE_LEFT_HALF:
            geometry->x = work_x;
            geometry->y = work_y;
            geometry->width = work_width / 2;
            geometry->height = work_height;
            break;
        case TILE_RIGHT_HALF:
            geometry->x = work_x + work_width / 2;
            geometry->y = work_y;
            geometry->width = work_width / 2;
            geometry->height = work_height;
            break;
        case TILE_TOP_HALF:
            geometry->x = work_x;
            geometry->y = work_y;
            geometry->width = work_width;
            geometry->height = work_height / 2;
            break;
        case TILE_BOTTOM_HALF:
            geometry->x = work_x;
            geometry->y = work_y + work_height / 2;
            geometry->width = work_width;
            geometry->height = work_height / 2;
            break;
        case TILE_LEFT_QUARTER:
            geometry->x = work_x;
            geometry->y = work_y;
            geometry->width = work_width / 4;
            geometry->height = work_height;
            break;
        case TILE_RIGHT_QUARTER:
            geometry->x = work_x + (work_width * 3) / 4;
            geometry->y = work_y;
            geometry->width = work_width / 4;
            geometry->height = work_height;
            break;
        case TILE_TOP_QUARTER:
            geometry->x = work_x;
            geometry->y = work_y;
            geometry->width = work_width;
            geometry->height = work_height / 4;
            break;
        case TILE_BOTTOM_QUARTER:
            geometry->x = work_x;
            geometry->y = work_y + (work_height * 3) / 4;
            geometry->width = work_width;
            geometry->height = work_height / 4;
            break;
        case TILE_LEFT_TWO_THIRDS:
            geometry->x = work_x;
            geometry->y = work_y;
            geometry->width = (work_width * 2) / 3;
            geometry->height = work_height;
            break;
        case TILE_RIGHT_TWO_THIRDS:
            geometry->x = work_x + work_width / 3;
            geometry->y = work_y;
            geometry->width = (work_width * 2) / 3;
            geometry->height = work_height;
            break;
        case TILE_TOP_TWO_THIRDS:
            geometry->x = work_x;
            geometry->y = work_y;
            geometry->width = work_width;
            geometry->height = (work_height * 2) / 3;
            break;
        case TILE_BOTTOM_TWO_THIRDS:
            geometry->x = work_x;
            geometry->y = work_y + work_height / 3;
            geometry->width = work_width;
            geometry->height = (work_height * 2) / 3;
            break;
        case TILE_LEFT_THREE_QUARTERS:
            geometry->x = work_x;
            geometry->y = work_y;
            geometry->width = (work_width * 3) / 4;
            geometry->height = work_height;
            break;
        case TILE_RIGHT_THREE_QUARTERS:
            geometry->x = work_x + work_width / 4;
            geometry->y = work_y;
            geometry->width = (work_width * 3) / 4;
            geometry->height = work_height;
            break;
        case TILE_TOP_THREE_QUARTERS:
            geometry->x = work_x;
            geometry->y = work_y;
            geometry->width = work_width;
            geometry->height = (work_height * 3) / 4;
            break;
        case TILE_BOTTOM_THREE_QUARTERS:
            geometry->x = work_x;
            geometry->y = work_y + work_height / 4;
            geometry->width = work_width;
            geometry->height = (work_height * 3) / 4;
            break;
        case TILE_GRID_1:
        case TILE_GRID_2:
        case TILE_GRID_3:
        case TILE_GRID_4:
        case TILE_GRID_5:
        case TILE_GRID_6:
        case TILE_GRID_7:
        case TILE_GRID_8:
        case TILE_GRID_9: {
            // Dynamic grid based on tile_columns
            int grid_position = option - TILE_GRID_1;
            int row = grid_position / tile_columns;
            int col = grid_position % tile_columns;
            
            geometry->width = work_width / tile_columns;
            geometry->height = work_height / 2;
            geometry->x = work_x + col * geometry->width;
            geometry->y = work_y + row * geometry->height;
            break;
        }
        case TILE_CENTER:
            // Center window at 50% size
            geometry->width = work_width / 2;
            geometry->height = work_height / 2;
            geometry->x = work_x + (work_width - geometry->width) / 2;
            geometry->y = work_y + (work_height - geometry->height) / 2;
            break;
        case TILE_CENTER_THIRD:
            geometry->width = work_width / 3;
            geometry->height = work_height / 3;
            geometry->x = work_x + (work_width - geometry->width) / 2;
            geometry->y = work_y + (work_height - geometry->height) / 2;
            break;
        case TILE_CENTER_TWO_THIRDS:
            geometry->width = (work_width * 2) / 3;
            geometry->height = (work_height * 2) / 3;
            geometry->x = work_x + (work_width - geometry->width) / 2;
            geometry->y = work_y + (work_height - geometry->height) / 2;
            break;
        case TILE_CENTER_THREE_QUARTERS:
            geometry->width = (work_width * 3) / 4;
            geometry->height = (work_height * 3) / 4;
            geometry->x = work_x + (work_width - geometry->width) / 2;
            geometry->y = work_y + (work_height - geometry->height) / 2;
            break;
        case TILE_GRID_1_NARROW:
        case TILE_GRID_2_NARROW:
        case TILE_GRID_3_NARROW:
        case TILE_GRID_4_NARROW:
        case TILE_GRID_5_NARROW:
        case TILE_GRID_6_NARROW:
        case TILE_GRID_7_NARROW:
        case TILE_GRID_8_NARROW:
        case TILE_GRID_9_NARROW: {
            int grid_position = option - TILE_GRID_1_NARROW;
            int row = grid_position / tile_columns;
            int col = grid_position % tile_columns;
            
            int tile_width = work_width / tile_columns;
            geometry->width = tile_width / 3;
            geometry->height = work_height / 2;
            geometry->x = work_x + col * tile_width;
            geometry->y = work_y + row * geometry->height;
            break;
        }
        case TILE_GRID_1_WIDE:
        case TILE_GRID_2_WIDE:
        case TILE_GRID_3_WIDE:
        case TILE_GRID_4_WIDE:
        case TILE_GRID_5_WIDE:
        case TILE_GRID_6_WIDE:
        case TILE_GRID_7_WIDE:
        case TILE_GRID_8_WIDE:
        case TILE_GRID_9_WIDE: {
            int grid_position = option - TILE_GRID_1_WIDE;
            int row = grid_position / tile_columns;
            int col = grid_position % tile_columns;
            
            int tile_width = work_width / tile_columns;
            geometry->width = (tile_width * 3) / 2;
            geometry->height = work_height / 2;
            geometry->x = work_x + col * tile_width;
            geometry->y = work_y + row * geometry->height;
            break;
        }
        case TILE_GRID_1_WIDER:
        case TILE_GRID_2_WIDER:
        case TILE_GRID_3_WIDER:
        case TILE_GRID_4_WIDER:
        case TILE_GRID_5_WIDER:
        case TILE_GRID_6_WIDER:
        case TILE_GRID_7_WIDER:
        case TILE_GRID_8_WIDER:
        case TILE_GRID_9_WIDER: {
            int grid_position = option - TILE_GRID_1_WIDER;
            int row = grid_position / tile_columns;
            int col = grid_position % tile_columns;
            
            int tile_width = work_width / tile_columns;
            geometry->width = (tile_width * 4) / 3;
            geometry->height = work_height / 2;
            geometry->x = work_x + col * tile_width;
            geometry->y = work_y + row * geometry->height;
            break;
        }
        default:
            log_error("Unknown tiling option: %d", option);
            // Set safe defaults
            geometry->x = work_x;
            geometry->y = work_y;
            geometry->width = work_width / 2;
            geometry->height = work_height / 2;
            break;
    }
    
    log_debug("Calculated tile geometry: x=%d, y=%d, width=%d, height=%d", 
              geometry->x, geometry->y, geometry->width, geometry->height);
}
