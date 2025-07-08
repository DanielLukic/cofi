#ifndef DYNAMIC_DISPLAY_H
#define DYNAMIC_DISPLAY_H

#include <gtk/gtk.h>
#include <gdk/gdk.h>

// Forward declarations
struct AppData;

// Configuration structure for dynamic display calculation
typedef struct {
    double screen_height_percentage;  // Percentage of screen height to use (default: 0.5 for 50%)
    int min_lines;                   // Minimum number of lines (fallback)
    int max_lines;                   // Maximum number of lines (safety limit)
    int fallback_lines;              // Fallback if calculation fails
    gboolean enable_hidpi_scaling;   // Whether to account for HiDPI scaling
} DynamicDisplayConfig;

// Screen information structure
typedef struct {
    gint width;
    gint height;
    gint workarea_width;
    gint workarea_height;
    gint scale_factor;
    gboolean is_hidpi;
    gboolean workarea_available;
} ScreenInfo;

// Font metrics information structure
typedef struct {
    gint font_height;        // Height of font in pixels
    gint line_height;        // Total line height including spacing
    gint ascent;             // Font ascent in pixels
    gint descent;            // Font descent in pixels
    gboolean metrics_valid;  // Whether metrics were successfully measured
} FontMetrics;

// Main calculation result structure
typedef struct {
    gint calculated_lines;
    gint effective_lines;    // After applying min/max constraints
    ScreenInfo screen_info;
    FontMetrics font_metrics;
    DynamicDisplayConfig config;
    gboolean calculation_successful;
    const char* fallback_reason;  // Reason if fallback was used
} DisplayLineCalculation;

// Core API functions

/**
 * Initialize dynamic display configuration with defaults
 */
void init_dynamic_display_config(DynamicDisplayConfig *config);

/**
 * Get screen information for the monitor containing the given window
 * Uses modern GdkMonitor API when available, falls back to deprecated GdkScreen API
 */
gboolean get_screen_info(GtkWidget *window, ScreenInfo *screen_info);

/**
 * Measure font metrics for the current GTK context
 * Uses Pango to get accurate font height and line spacing information
 */
gboolean measure_font_metrics(GtkWidget *widget, FontMetrics *font_metrics);

/**
 * Calculate the maximum number of display lines based on screen and font metrics
 * This is the main function that combines screen info and font metrics
 */
gint calculate_max_display_lines(GtkWidget *window, const DynamicDisplayConfig *config, 
                                DisplayLineCalculation *result);

/**
 * Get the maximum display lines with caching for performance
 * Recalculates only when screen or font configuration changes
 */
gint get_dynamic_max_display_lines(struct AppData *app);

/**
 * Force recalculation of display lines (call when font or screen changes)
 */
void invalidate_display_line_cache(struct AppData *app);

// Utility functions

/**
 * Check if the current system supports modern GdkMonitor API (GTK 3.22+)
 */
gboolean has_modern_monitor_api(void);

/**
 * Get monitor information using the best available API
 */
gboolean get_monitor_info_best_api(GdkDisplay *display, GtkWidget *window, 
                                  GdkRectangle *geometry, GdkRectangle *workarea, 
                                  gint *scale_factor);

/**
 * Create a temporary Pango layout for font measurement
 */
PangoLayout* create_measurement_layout(GtkWidget *widget);

/**
 * Convert Pango units to pixels accounting for scale factor
 */
gint pango_units_to_pixels_scaled(gint pango_units, gint scale_factor);

/**
 * Debug function to print calculation details
 */
void debug_print_calculation(const DisplayLineCalculation *calc);

// Configuration constants
#define DEFAULT_SCREEN_HEIGHT_PERCENTAGE 0.5
#define DEFAULT_MIN_LINES 5
#define DEFAULT_MAX_LINES 50
#define DEFAULT_FALLBACK_LINES 20
#define CACHE_INVALIDATION_TIMEOUT_MS 1000

// Error handling
typedef enum {
    DYNAMIC_DISPLAY_SUCCESS = 0,
    DYNAMIC_DISPLAY_ERROR_NO_WINDOW,
    DYNAMIC_DISPLAY_ERROR_NO_SCREEN,
    DYNAMIC_DISPLAY_ERROR_NO_MONITOR,
    DYNAMIC_DISPLAY_ERROR_FONT_MEASUREMENT,
    DYNAMIC_DISPLAY_ERROR_CALCULATION
} DynamicDisplayError;

const char* dynamic_display_error_string(DynamicDisplayError error);

#endif // DYNAMIC_DISPLAY_H
