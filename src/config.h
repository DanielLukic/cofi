#ifndef CONFIG_H
#define CONFIG_H

// Forward declaration for WindowAlignment (defined in app_data.h)
typedef enum {
    ALIGN_CENTER,
    ALIGN_TOP,
    ALIGN_TOP_LEFT,
    ALIGN_TOP_RIGHT,
    ALIGN_LEFT,
    ALIGN_RIGHT,
    ALIGN_BOTTOM,
    ALIGN_BOTTOM_LEFT,
    ALIGN_BOTTOM_RIGHT
} WindowAlignment;

// Unified configuration structure
typedef struct {
    int close_on_focus_loss;        // Whether to close window when focus is lost
    WindowAlignment alignment;      // Window alignment setting
    int workspaces_per_row;        // Number of workspaces per row in grid layout (0 = linear)
} CofiConfig;

// Configuration management functions (options only - harpoon slots handled separately)
void save_config(const CofiConfig *config);
void load_config(CofiConfig *config);

// Initialize config with default values
void init_config_defaults(CofiConfig *config);

/* Scoring constants from fzy (moved to bottom to maintain compatibility) */
#define SCORE_MATCH_CONSECUTIVE 16
#define SCORE_MATCH_SLASH 9
#define SCORE_MATCH_WORD 8
#define SCORE_MATCH_CAPITAL 7
#define SCORE_MATCH_DOT 6

#define SCORE_GAP_LEADING -9
#define SCORE_GAP_TRAILING -10
#define SCORE_GAP_INNER -11

#endif /* CONFIG_H */