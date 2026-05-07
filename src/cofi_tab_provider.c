#include "cofi_tab_provider.h"

#include <string.h>

#define COFI_MAX_PROVIDERS 32
#define COFI_MAX_FILTERED  1024

typedef struct {
    CofiTabProvider provider;
    int raw_map[COFI_MAX_FILTERED];
    int filtered_count;
    int generation;
} ProviderEntry;

static ProviderEntry s_registry[COFI_MAX_PROVIDERS];
static int s_count = 0;

void cofi_init_provider_defaults(CofiTabProvider *p) {
    if (!p) return;
    memset(p, 0, sizeof(*p));
    p->hidden_by_default = 1;
    p->modal_policy = COFI_MODAL_HIDE_ON_ESC;
}

int cofi_register_tab_provider(const CofiTabProvider *provider) {
    if (!provider || s_count >= COFI_MAX_PROVIDERS) return -1;
    int id = s_count++;
    memset(&s_registry[id], 0, sizeof(s_registry[id]));
    s_registry[id].provider = *provider;
    return id;
}

const CofiTabProvider *cofi_get_provider(int provider_id) {
    if (provider_id < 0 || provider_id >= s_count) return NULL;
    return &s_registry[provider_id].provider;
}

const CofiTabProvider *cofi_get_provider_for_tab(int tab_mode) {
    for (int i = 0; i < s_count; i++) {
        if (s_registry[i].provider.tab_mode == tab_mode)
            return &s_registry[i].provider;
    }
    return NULL;
}

int cofi_provider_count(void) {
    return s_count;
}

void cofi_set_filtered_map(int provider_id, const int *raw_map, int count) {
    if (provider_id < 0 || provider_id >= s_count || !raw_map) return;
    if (count < 0) count = 0;
    if (count > COFI_MAX_FILTERED) count = COFI_MAX_FILTERED;
    ProviderEntry *e = &s_registry[provider_id];
    memcpy(e->raw_map, raw_map, (size_t)count * sizeof(int));
    e->filtered_count = count;
}

int cofi_filtered_to_raw(int provider_id, int filtered_idx) {
    if (provider_id < 0 || provider_id >= s_count) return -1;
    ProviderEntry *e = &s_registry[provider_id];
    if (filtered_idx < 0 || filtered_idx >= e->filtered_count) return -1;
    return e->raw_map[filtered_idx];
}

int cofi_get_filtered_count(int provider_id) {
    if (provider_id < 0 || provider_id >= s_count) return 0;
    return s_registry[provider_id].filtered_count;
}

int cofi_next_generation(int provider_id) {
    if (provider_id < 0 || provider_id >= s_count) return -1;
    return ++s_registry[provider_id].generation;
}

int cofi_current_generation(int provider_id) {
    if (provider_id < 0 || provider_id >= s_count) return -1;
    return s_registry[provider_id].generation;
}

int cofi_call_row_count(int provider_id, AppData *app) {
    const CofiTabProvider *p = cofi_get_provider(provider_id);
    if (!p || !p->row_count) return 0;
    return p->row_count(app);
}

CofiActionStatus cofi_call_on_enter_pressed(int provider_id, AppData *app,
                                             int filtered_idx, int raw_idx,
                                             const char *entry_text) {
    const CofiTabProvider *p = cofi_get_provider(provider_id);
    if (!p || !p->on_enter_pressed) return COFI_NO_OP;
    return p->on_enter_pressed(app, filtered_idx, raw_idx, entry_text);
}

CofiActionStatus cofi_call_on_command_args(int provider_id, AppData *app,
                                            const char *args) {
    const CofiTabProvider *p = cofi_get_provider(provider_id);
    if (!p || !p->on_command_args) return COFI_NO_OP;
    return p->on_command_args(app, args);
}

void cofi_registry_reset(void) {
    memset(s_registry, 0, sizeof(s_registry));
    s_count = 0;
}
