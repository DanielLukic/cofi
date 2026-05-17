#ifndef COFI_TAB_PROVIDER_H
#define COFI_TAB_PROVIDER_H

#include <glib.h>

#define COFI_PROVIDER_DYNAMIC_TAB (-1)

/* Provider API — types + registry.
 * AppData is forward-declared here; providers that need full AppData access
 * include app_data.h in their own .c files. */

#ifndef APPDATA_TYPEDEF_DEFINED
typedef struct AppData AppData;
#define APPDATA_TYPEDEF_DEFINED
#endif
typedef struct WindowInfo WindowInfo;
typedef struct _GdkEventKey GdkEventKey;

typedef enum {
    COFI_HANDLED_HIDE,     /* core hides cofi window */
    COFI_HANDLED_KEEP,     /* stay open, clear entry, refresh display */
    COFI_HANDLED_REFRESH,  /* stay open, preserve entry, re-filter */
    COFI_NO_OP,            /* nothing happened */
    COFI_ACTION_ERROR      /* show transient error row, keep open */
} CofiActionStatus;

typedef enum {
    COFI_ROW_ACTIONABLE = 1 << 0,
    COFI_ROW_SLOTTABLE  = 1 << 1,
    COFI_ROW_ERROR      = 1 << 2,
} CofiRowFlags;

typedef enum {
    COFI_MODAL_HIDE_ON_ESC,
    COFI_MODAL_RETURN_KEEP_QUERY,
    COFI_MODAL_CLEAR_THEN_RETURN,
} CofiModalPolicy;

typedef struct {
    struct {
        const char *text;
        int   width_hint;   /* preferred column width; 0 = flexible */
        int   align;        /* 0 = left, 1 = right */
    } cells[8];
    int cell_count;
    int row_flags;
} CofiRowCells;

typedef struct CofiTabProvider {
    /* Existing providers may use legacy TabMode values. New providers can set
     * COFI_PROVIDER_DYNAMIC_TAB and receive a tab handle at registration. */
    int tab_mode;

    const char *id;
    const char *display_name;
    const char *shortcut_hint;
    const char *(*get_shortcut_hint)(AppData *app);
    char prefix_char;

    int required;                  /* required providers cannot be disabled */
    int hidden_by_default;
    CofiModalPolicy modal_policy;
    int initial_selection_index;

    int  (*row_count)(AppData *);
    void (*format_row)(AppData *, int raw_idx, CofiRowCells *out);
    const char *(*match_string)(AppData *, int raw_idx);
    const char *(*row_identity)(AppData *, int raw_idx);

    int  (*score_row)(AppData *, int raw_idx, const char *query);

    void (*on_enter)(AppData *);
    void (*on_leave)(AppData *);
    void (*on_surface)(AppData *);
    void (*on_query_changed)(AppData *, const char *query);
    void (*on_selection_changed)(AppData *, int filtered_idx);
    gboolean (*handle_key)(GdkEventKey *, AppData *);

    void (*on_tick)(AppData *, int generation);
    int  tick_interval_ms;

    CofiActionStatus (*on_enter_pressed)(AppData *, int filtered_idx, int raw_idx,
                                         const char *entry_text, int modifier_state);
    CofiActionStatus (*on_command_args)(AppData *, const char *args);

    int slot_store_enabled;
    const char *(*slot_payload_for)(AppData *, int raw_idx);
    CofiActionStatus (*slot_recall)(AppData *, const char *payload);
} CofiTabProvider;

/* === Registry === */

void cofi_init_provider_defaults(CofiTabProvider *p);
int  cofi_register_tab_provider(const CofiTabProvider *p);

const CofiTabProvider *cofi_get_provider(int provider_id);
int  cofi_get_provider_id(const char *id);
const CofiTabProvider *cofi_get_provider_for_tab(int tab_mode);
int  cofi_get_provider_id_for_tab(int tab_mode);
int  cofi_list_provider_tabs(int *tabs, int max_tabs);
int  cofi_provider_count(void);
int  cofi_provider_is_enabled(int provider_id);
void cofi_set_provider_enabled(int provider_id, int enabled);
int  cofi_provider_is_disableable(int provider_id);
void cofi_apply_disabled_providers(const char *disabled_ids);
void cofi_build_disabled_providers_string(char *out, size_t out_size);

/* === Filtered→raw mapping (core populates after each filter pass) === */

void cofi_set_filtered_map(int provider_id, const int *raw_map, int count);
int  cofi_filtered_to_raw(int provider_id, int filtered_idx);
int  cofi_get_filtered_count(int provider_id);

/* === Generation token for on_tick === */

int cofi_next_generation(int provider_id);
int cofi_current_generation(int provider_id);

/* === Dispatch helpers === */

int  cofi_call_row_count(int provider_id, AppData *app);
CofiActionStatus cofi_call_on_enter_pressed(int provider_id, AppData *app,
                                             int filtered_idx, int raw_idx,
                                             const char *entry_text,
                                             int modifier_state);
CofiActionStatus cofi_call_on_command_args(int provider_id, AppData *app,
                                            const char *args);

/* === Test support === */

void cofi_registry_reset(void);

const CofiTabProvider *cofi_get_provider_for_prefix(char prefix);

#endif /* COFI_TAB_PROVIDER_H */
