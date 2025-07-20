#include "dynamic_display.h"
#include "app_data.h"
#include "log.h"
#include <string.h>
#include <math.h>

// Static cache for performance
static DisplayLineCalculation cached_calculation = {0};
static gboolean cache_valid = FALSE;
static guint cache_timestamp = 0;

// Initialize dynamic display configuration with sensible defaults
void init_dynamic_display_config(DynamicDisplayConfig *config) {
    if (!config) return;
    
    config->screen_height_percentage = DEFAULT_SCREEN_HEIGHT_PERCENTAGE;
    config->min_lines = DEFAULT_MIN_LINES;
    config->max_lines = DEFAULT_MAX_LINES;
    config->fallback_lines = DEFAULT_FALLBACK_LINES;
    config->enable_hidpi_scaling = TRUE;
}

// Check if modern GdkMonitor API is available (GTK 3.22+)
gboolean has_modern_monitor_api(void) {
    // Check GTK version at runtime
    return gtk_check_version(3, 22, 0) == NULL;
}

// Get monitor information using the best available API
gboolean get_monitor_info_best_api(GdkDisplay *display, GtkWidget *window, 
                                  GdkRectangle *geometry, GdkRectangle *workarea, 
                                  gint *scale_factor) {
    if (!display || !geometry) return FALSE;
    
    // Initialize output parameters
    memset(geometry, 0, sizeof(GdkRectangle));
    if (workarea) memset(workarea, 0, sizeof(GdkRectangle));
    if (scale_factor) *scale_factor = 1;
    
    if (has_modern_monitor_api()) {
        // Use modern GdkMonitor API (GTK 3.22+)
        GdkMonitor *monitor = NULL;
        
        if (window && gtk_widget_get_realized(window)) {
            GdkWindow *gdk_window = gtk_widget_get_window(window);
            if (gdk_window) {
                monitor = gdk_display_get_monitor_at_window(display, gdk_window);
            }
        }
        
        if (!monitor) {
            // Fallback to primary monitor
            monitor = gdk_display_get_primary_monitor(display);
        }
        
        if (!monitor) {
            // Fallback to first monitor
            if (gdk_display_get_n_monitors(display) > 0) {
                monitor = gdk_display_get_monitor(display, 0);
            }
        }
        
        if (monitor) {
            gdk_monitor_get_geometry(monitor, geometry);
            if (workarea) {
                gdk_monitor_get_workarea(monitor, workarea);
            }
            if (scale_factor) {
                *scale_factor = gdk_monitor_get_scale_factor(monitor);
            }
            
            log_debug("Modern API: Monitor geometry %dx%d, scale factor %d", 
                     geometry->width, geometry->height, scale_factor ? *scale_factor : 1);
            return TRUE;
        }
    } else {
        // Use deprecated GdkScreen API for older GTK versions
        GdkScreen *screen = gdk_display_get_default_screen(display);
        if (!screen) return FALSE;
        
        gint monitor_num = 0;
        if (window && gtk_widget_get_realized(window)) {
            GdkWindow *gdk_window = gtk_widget_get_window(window);
            if (gdk_window) {
                monitor_num = gdk_screen_get_monitor_at_window(screen, gdk_window);
            }
        }
        
        // Get monitor geometry
        gdk_screen_get_monitor_geometry(screen, monitor_num, geometry);
        
        // Get workarea if available
        if (workarea) {
            gdk_screen_get_monitor_workarea(screen, monitor_num, workarea);
        }
        
        // Get scale factor if available
        if (scale_factor) {
            *scale_factor = gdk_screen_get_monitor_scale_factor(screen, monitor_num);
        }
        
        log_debug("Legacy API: Monitor %d geometry %dx%d, scale factor %d", 
                 monitor_num, geometry->width, geometry->height, scale_factor ? *scale_factor : 1);
        return TRUE;
    }
    
    return FALSE;
}

// Get comprehensive screen information
gboolean get_screen_info(GtkWidget *window, ScreenInfo *screen_info) {
    if (!screen_info) return FALSE;
    
    // Initialize structure
    memset(screen_info, 0, sizeof(ScreenInfo));
    screen_info->scale_factor = 1;
    
    GdkDisplay *display = window ? gtk_widget_get_display(window) : gdk_display_get_default();
    if (!display) {
        log_warn("Could not get display for screen info");
        return FALSE;
    }
    
    GdkRectangle geometry, workarea;
    gint scale_factor;
    
    if (!get_monitor_info_best_api(display, window, &geometry, &workarea, &scale_factor)) {
        log_warn("Could not get monitor information");
        return FALSE;
    }
    
    // Fill screen info structure
    screen_info->width = geometry.width;
    screen_info->height = geometry.height;
    screen_info->workarea_width = workarea.width > 0 ? workarea.width : geometry.width;
    screen_info->workarea_height = workarea.height > 0 ? workarea.height : geometry.height;
    screen_info->scale_factor = scale_factor;
    screen_info->is_hidpi = (scale_factor > 1);
    screen_info->workarea_available = (workarea.width > 0 && workarea.height > 0);
    
    log_debug("Screen info: %dx%d (workarea: %dx%d), scale: %d, HiDPI: %s", 
             screen_info->width, screen_info->height,
             screen_info->workarea_width, screen_info->workarea_height,
             screen_info->scale_factor, screen_info->is_hidpi ? "yes" : "no");
    
    return TRUE;
}

// Convert Pango units to pixels with scale factor consideration
gint pango_units_to_pixels_scaled(gint pango_units, gint scale_factor) {
    // Pango units are 1024ths of a point, and points are 1/72 inch
    // We need to convert to pixels and account for scale factor
    gint pixels = PANGO_PIXELS(pango_units);
    
    // For HiDPI displays, we might need to adjust
    // However, Pango should already handle this internally in most cases
    return pixels;
}

// Create a temporary layout for font measurement
PangoLayout* create_measurement_layout(GtkWidget *widget) {
    if (!widget) return NULL;
    
    PangoContext *context = gtk_widget_get_pango_context(widget);
    if (!context) return NULL;
    
    PangoLayout *layout = pango_layout_new(context);
    if (!layout) return NULL;
    
    // Set sample text for measurement
    pango_layout_set_text(layout, "Ag", -1);  // Mixed case with descender
    
    return layout;
}

// Measure font metrics using Pango
gboolean measure_font_metrics(GtkWidget *widget, FontMetrics *font_metrics) {
    if (!widget || !font_metrics) return FALSE;
    
    // Initialize structure
    memset(font_metrics, 0, sizeof(FontMetrics));
    
    PangoLayout *layout = create_measurement_layout(widget);
    if (!layout) {
        log_warn("Could not create Pango layout for font measurement");
        return FALSE;
    }
    
    // Get pixel extents of the sample text
    PangoRectangle ink_rect, logical_rect;
    pango_layout_get_pixel_extents(layout, &ink_rect, &logical_rect);
    
    // Get font metrics from the layout's context
    PangoContext *context = pango_layout_get_context(layout);
    PangoFontDescription *font_desc = pango_context_get_font_description(context);
    PangoFontMetrics *metrics = pango_context_get_metrics(context, font_desc, NULL);
    
    if (metrics) {
        // Convert Pango units to pixels
        font_metrics->ascent = PANGO_PIXELS(pango_font_metrics_get_ascent(metrics));
        font_metrics->descent = PANGO_PIXELS(pango_font_metrics_get_descent(metrics));
        
        // Calculate font height and line height
        font_metrics->font_height = font_metrics->ascent + font_metrics->descent;
        
        // Use logical rectangle height as line height (includes line spacing)
        font_metrics->line_height = logical_rect.height;
        
        // Ensure line height is at least font height
        if (font_metrics->line_height < font_metrics->font_height) {
            font_metrics->line_height = font_metrics->font_height;
        }
        
        font_metrics->metrics_valid = TRUE;
        
        log_debug("Font metrics: height=%d, line_height=%d, ascent=%d, descent=%d", 
                 font_metrics->font_height, font_metrics->line_height,
                 font_metrics->ascent, font_metrics->descent);
        
        pango_font_metrics_unref(metrics);
    } else {
        log_warn("Could not get Pango font metrics");
        font_metrics->metrics_valid = FALSE;
    }
    
    g_object_unref(layout);
    return font_metrics->metrics_valid;
}

// Calculate maximum display lines based on screen and font information
gint calculate_max_display_lines(GtkWidget *window, const DynamicDisplayConfig *config,
                                DisplayLineCalculation *result) {
    if (!config || !result) return DEFAULT_FALLBACK_LINES;

    // Initialize result structure
    memset(result, 0, sizeof(DisplayLineCalculation));
    result->config = *config;
    result->fallback_reason = NULL;

    // Get screen information
    if (!get_screen_info(window, &result->screen_info)) {
        result->fallback_reason = "Could not get screen information";
        result->effective_lines = config->fallback_lines;
        log_warn("Screen info failed, using fallback: %d lines", result->effective_lines);
        return result->effective_lines;
    }

    // Get font metrics
    if (!measure_font_metrics(window, &result->font_metrics)) {
        result->fallback_reason = "Could not measure font metrics";
        result->effective_lines = config->fallback_lines;
        log_warn("Font metrics failed, using fallback: %d lines", result->effective_lines);
        return result->effective_lines;
    }

    // Calculate available height
    gint available_height;
    if (result->screen_info.workarea_available) {
        available_height = result->screen_info.workarea_height;
        log_debug("Using workarea height: %d", available_height);
    } else {
        available_height = result->screen_info.height;
        log_debug("Using full screen height: %d", available_height);
    }

    // Apply percentage
    gint target_height = (gint)(available_height * config->screen_height_percentage);
    log_debug("Target height (%.1f%% of %d): %d",
             config->screen_height_percentage * 100, available_height, target_height);

    // Calculate number of lines that fit
    if (result->font_metrics.line_height > 0) {
        result->calculated_lines = target_height / result->font_metrics.line_height;
    } else {
        result->fallback_reason = "Invalid line height";
        result->effective_lines = config->fallback_lines;
        log_warn("Invalid line height, using fallback: %d lines", result->effective_lines);
        return result->effective_lines;
    }

    // Apply constraints
    result->effective_lines = result->calculated_lines;
    if (result->effective_lines < config->min_lines) {
        result->effective_lines = config->min_lines;
        log_debug("Applied minimum constraint: %d -> %d", result->calculated_lines, result->effective_lines);
    }
    if (result->effective_lines > config->max_lines) {
        result->effective_lines = config->max_lines;
        log_debug("Applied maximum constraint: %d -> %d", result->calculated_lines, result->effective_lines);
    }

    result->calculation_successful = TRUE;

    log_debug("Dynamic line calculation: %d lines (target_height=%d, line_height=%d, scale=%d)",
             result->effective_lines, target_height, result->font_metrics.line_height,
             result->screen_info.scale_factor);

    return result->effective_lines;
}

// Get dynamic max display lines with caching
gint get_dynamic_max_display_lines(struct AppData *app) {
    if (!app || !app->window) {
        log_trace("Invalid app data for dynamic line calculation - using default");
        return DEFAULT_FALLBACK_LINES;
    }

    // Check cache validity
    guint current_time = g_get_monotonic_time() / 1000;  // Convert to milliseconds
    if (cache_valid && (current_time - cache_timestamp) < CACHE_INVALIDATION_TIMEOUT_MS) {
        log_debug("Using cached line calculation: %d lines", cached_calculation.effective_lines);
        return cached_calculation.effective_lines;
    }

    // Initialize configuration from app config
    DynamicDisplayConfig config;
    init_dynamic_display_config(&config);

    // Override defaults with app configuration if available
    // TODO: Temporarily disabled until config structure is properly updated
    // For now, just use the dynamic calculation with defaults
    config.screen_height_percentage = 0.5;  // 50% of screen height
    config.min_lines = 5;                   // Minimum 5 lines
    config.max_lines = 50;                  // Maximum 50 lines
    config.fallback_lines = 20;             // Fallback to 20 lines
    config.enable_hidpi_scaling = TRUE;     // Always enable HiDPI scaling

    log_debug("Using dynamic sizing with default configuration");

    // Perform calculation
    gint lines = calculate_max_display_lines(app->window, &config, &cached_calculation);

    // Update cache
    cache_valid = TRUE;
    cache_timestamp = current_time;

    return lines;
}

// Invalidate cache to force recalculation
void invalidate_display_line_cache(struct AppData *app) {
    (void)app;  // Unused parameter
    cache_valid = FALSE;
    log_debug("Display line cache invalidated");
}

// Debug function to print calculation details
void debug_print_calculation(const DisplayLineCalculation *calc) {
    if (!calc) return;

    log_debug("=== Dynamic Display Line Calculation ===");
    log_debug("Screen: %dx%d (workarea: %dx%d), scale: %d, HiDPI: %s",
             calc->screen_info.width, calc->screen_info.height,
             calc->screen_info.workarea_width, calc->screen_info.workarea_height,
             calc->screen_info.scale_factor, calc->screen_info.is_hidpi ? "yes" : "no");
    log_debug("Font: height=%d, line_height=%d, ascent=%d, descent=%d",
             calc->font_metrics.font_height, calc->font_metrics.line_height,
             calc->font_metrics.ascent, calc->font_metrics.descent);
    log_debug("Config: percentage=%.1f%%, min=%d, max=%d, fallback=%d",
             calc->config.screen_height_percentage * 100,
             calc->config.min_lines, calc->config.max_lines, calc->config.fallback_lines);
    log_debug("Result: calculated=%d, effective=%d, successful=%s",
             calc->calculated_lines, calc->effective_lines,
             calc->calculation_successful ? "yes" : "no");
    if (calc->fallback_reason) {
        log_debug("Fallback reason: %s", calc->fallback_reason);
    }
    log_debug("========================================");
}

// Error handling
const char* dynamic_display_error_string(DynamicDisplayError error) {
    switch (error) {
        case DYNAMIC_DISPLAY_SUCCESS: return "Success";
        case DYNAMIC_DISPLAY_ERROR_NO_WINDOW: return "No window provided";
        case DYNAMIC_DISPLAY_ERROR_NO_SCREEN: return "Could not access screen information";
        case DYNAMIC_DISPLAY_ERROR_NO_MONITOR: return "Could not access monitor information";
        case DYNAMIC_DISPLAY_ERROR_FONT_MEASUREMENT: return "Font measurement failed";
        case DYNAMIC_DISPLAY_ERROR_CALCULATION: return "Line calculation failed";
        default: return "Unknown error";
    }
}
