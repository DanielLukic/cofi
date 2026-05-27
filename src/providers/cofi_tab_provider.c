#include "providers/cofi_tab_provider.h"

#include <ctype.h>
#include <string.h>

#include "core/app/app_data.h"

#define COFI_MAX_PROVIDERS 32
#define COFI_MAX_FILTERED  1024

typedef struct {
    CofiTabProvider provider;
    int raw_map[COFI_MAX_FILTERED];
    int filtered_count;
    int generation;
    int enabled;
} ProviderEntry;

static ProviderEntry s_registry[COFI_MAX_PROVIDERS];
static int s_count = 0;
static int s_next_dynamic_tab = TAB_COUNT + 1;

void cofi_init_provider_defaults(CofiTabProvider *p) {
    if (!p) return;
    memset(p, 0, sizeof(*p));
    p->hidden_by_default = 1;
    p->modal_policy = COFI_MODAL_HIDE_ON_ESC;
}

int cofi_register_tab_provider(const CofiTabProvider *provider) {
    if (!provider || s_count >= COFI_MAX_PROVIDERS) return -1;
    CofiTabProvider copy = *provider;
    if (copy.tab_mode == COFI_PROVIDER_DYNAMIC_TAB) {
        if (s_next_dynamic_tab >= COFI_MAX_TAB_HANDLES) return -1;
        copy.tab_mode = s_next_dynamic_tab++;
    }
    if (copy.tab_mode < TAB_WINDOWS || copy.tab_mode >= COFI_MAX_TAB_HANDLES) {
        return -1;
    }
    if (copy.id && copy.id[0] && cofi_get_provider_id(copy.id) >= 0) {
        return -1;
    }
    if (cofi_get_provider_id_for_tab(copy.tab_mode) >= 0) {
        return -1;
    }

    int id = s_count++;
    memset(&s_registry[id], 0, sizeof(s_registry[id]));
    s_registry[id].provider = copy;
    s_registry[id].enabled = 1;
    return id;
}

const CofiTabProvider *cofi_get_provider(int provider_id) {
    if (provider_id < 0 || provider_id >= s_count) return NULL;
    return &s_registry[provider_id].provider;
}

int cofi_get_provider_id(const char *id) {
    if (!id) return -1;
    for (int i = 0; i < s_count; i++) {
        const char *provider_id = s_registry[i].provider.id;
        if (provider_id && strcmp(provider_id, id) == 0) {
            return i;
        }
    }
    return -1;
}

const CofiTabProvider *cofi_get_provider_for_tab(int tab_mode) {
    for (int i = 0; i < s_count; i++) {
        if (!s_registry[i].enabled) continue;
        if (s_registry[i].provider.tab_mode == tab_mode)
            return &s_registry[i].provider;
    }
    return NULL;
}

int cofi_get_provider_id_for_tab(int tab_mode) {
    for (int i = 0; i < s_count; i++) {
        if (!s_registry[i].enabled) continue;
        if (s_registry[i].provider.tab_mode == tab_mode)
            return i;
    }
    return -1;
}

int cofi_list_provider_tabs(int *tabs, int max_tabs) {
    if (!tabs || max_tabs <= 0) return 0;

    int count = 0;
    for (int i = 0; i < s_count && count < max_tabs; i++) {
        int tab = s_registry[i].provider.tab_mode;
        if (tab >= TAB_COUNT && s_registry[i].enabled) {
            tabs[count++] = tab;
        }
    }

    return count;
}

int cofi_provider_count(void) {
    return s_count;
}

int cofi_provider_is_enabled(int provider_id) {
    if (provider_id < 0 || provider_id >= s_count) return 0;
    return s_registry[provider_id].enabled;
}

void cofi_set_provider_enabled(int provider_id, int enabled) {
    if (provider_id < 0 || provider_id >= s_count) return;
    if (!cofi_provider_is_disableable(provider_id)) {
        s_registry[provider_id].enabled = 1;
        return;
    }
    s_registry[provider_id].enabled = enabled ? 1 : 0;
}

int cofi_provider_is_disableable(int provider_id) {
    if (provider_id < 0 || provider_id >= s_count) return 0;
    const CofiTabProvider *provider = &s_registry[provider_id].provider;
    if (!provider->id || provider->id[0] == '\0') return 0;
    return provider->required ? 0 : 1;
}

static int token_equals(const char *start, size_t len, const char *id) {
    return id && strlen(id) == len && strncmp(start, id, len) == 0;
}

static int disabled_list_contains(const char *disabled_ids, const char *id) {
    if (!disabled_ids || !id || id[0] == '\0') return 0;

    const char *p = disabled_ids;
    while (*p) {
        while (*p == ',' || isspace((unsigned char)*p)) p++;
        const char *start = p;
        while (*p && *p != ',' && !isspace((unsigned char)*p)) p++;
        if (p > start && token_equals(start, (size_t)(p - start), id)) {
            return 1;
        }
    }
    return 0;
}

void cofi_apply_disabled_providers(const char *disabled_ids) {
    for (int i = 0; i < s_count; i++) {
        const char *id = s_registry[i].provider.id;
        int disabled = cofi_provider_is_disableable(i) &&
                       disabled_list_contains(disabled_ids, id);
        cofi_set_provider_enabled(i, !disabled);
    }
}

void cofi_build_disabled_providers_string(char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = '\0';

    for (int i = 0; i < s_count; i++) {
        if (s_registry[i].enabled) continue;
        const char *id = s_registry[i].provider.id;
        if (!id || id[0] == '\0') continue;

        size_t used = strlen(out);
        if (used >= out_size - 1) return;
        const char *sep = used > 0 ? "," : "";
        int written = g_snprintf(out + used, out_size - used, "%s%s", sep, id);
        if (written < 0 || (size_t)written >= out_size - used) {
            out[out_size - 1] = '\0';
            return;
        }
    }
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
    if (!cofi_provider_is_enabled(provider_id)) return 0;
    const CofiTabProvider *p = cofi_get_provider(provider_id);
    if (!p || !p->row_count) return 0;
    return p->row_count(app);
}

CofiActionStatus cofi_call_on_enter_pressed(int provider_id, AppData *app,
                                             int filtered_idx, int raw_idx,
                                             const char *entry_text,
                                             int modifier_state) {
    if (!cofi_provider_is_enabled(provider_id)) return COFI_NO_OP;
    const CofiTabProvider *p = cofi_get_provider(provider_id);
    if (!p || !p->on_enter_pressed) return COFI_NO_OP;
    return p->on_enter_pressed(app, filtered_idx, raw_idx, entry_text, modifier_state);
}

CofiActionStatus cofi_call_on_command_args(int provider_id, AppData *app,
                                            const char *args) {
    if (!cofi_provider_is_enabled(provider_id)) return COFI_NO_OP;
    const CofiTabProvider *p = cofi_get_provider(provider_id);
    if (!p || !p->on_command_args) return COFI_NO_OP;
    return p->on_command_args(app, args);
}

void cofi_registry_reset(void) {
    memset(s_registry, 0, sizeof(s_registry));
    s_count = 0;
    s_next_dynamic_tab = TAB_COUNT + 1;
}

const CofiTabProvider *cofi_get_provider_for_prefix(char prefix) {
    for (int i = 0; i < s_count; i++) {
        if (!s_registry[i].enabled) continue;
        if (s_registry[i].provider.prefix_char == prefix)
            return &s_registry[i].provider;
    }
    return NULL;
}

const CofiTabProvider *cofi_get_provider_for_tab_prefix(char prefix) {
    for (int i = 0; i < s_count; i++) {
        if (!s_registry[i].enabled) continue;
        const char *chars = s_registry[i].provider.tab_prefix_chars;
        if (chars && strchr(chars, prefix))
            return &s_registry[i].provider;
    }
    return NULL;
}

const CofiTabProvider *cofi_get_provider_for_delegate_opcode(int opcode) {
    if (opcode <= 0) return NULL;
    for (int i = 0; i < s_count; i++) {
        if (!s_registry[i].enabled) continue;
        if (s_registry[i].provider.delegate_opcode == opcode)
            return &s_registry[i].provider;
    }
    return NULL;
}

const CofiTabProvider *cofi_get_provider_for_hotkey_mode(int mode) {
    int claim = COFI_PROVIDER_HOTKEY_MODE(mode);
    if (claim <= 0) return NULL;
    for (int i = 0; i < s_count; i++) {
        if (!s_registry[i].enabled) continue;
        if (s_registry[i].provider.hotkey_mode_claim == claim)
            return &s_registry[i].provider;
    }
    return NULL;
}
