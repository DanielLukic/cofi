#include "tab_metadata.h"

#include <ctype.h>
#include <string.h>

#include "cofi_tab_provider.h"

static const char *provider_tab_name(TabMode tab) {
    const CofiTabProvider *p = cofi_get_provider_for_tab(tab);
    if (!p) return NULL;
    if (p->display_name && p->display_name[0]) return p->display_name;
    return p->id;
}

const char *tab_display_name(TabMode tab) {
    switch (tab) {
        case TAB_WINDOWS:    return "Windows";
        case TAB_WORKSPACES: return "Workspaces";
        case TAB_HARPOON:    return "Harpoon";
        case TAB_NAMES:      return "Names";
        case TAB_CONFIG:     return "Config";
        case TAB_HOTKEYS:    return "Hotkeys";
        case TAB_RULES:      return "Rules";
        case TAB_APPS:       return "Apps";
        case TAB_SESSIONS:   return "Sessions";
        case TAB_COUNT:      return NULL;
    }
    return provider_tab_name(tab);
}

const char *tab_active_name(TabMode tab) {
    static char active_name[32];

    switch (tab) {
        case TAB_WINDOWS:    return "WINDOWS";
        case TAB_WORKSPACES: return "WORKSPACES";
        case TAB_HARPOON:    return "HARPOON";
        case TAB_NAMES:      return "NAMES";
        case TAB_CONFIG:     return "CONFIG";
        case TAB_HOTKEYS:    return "HOTKEYS";
        case TAB_RULES:      return "RULES";
        case TAB_APPS:       return "APPS";
        case TAB_SESSIONS:   return "SESSIONS";
        case TAB_COUNT:      return NULL;
    }

    const char *name = provider_tab_name(tab);
    if (!name) return NULL;
    size_t i = 0;
    for (; name[i] && i < sizeof(active_name) - 1; i++) {
        active_name[i] = (char)toupper((unsigned char)name[i]);
    }
    active_name[i] = '\0';
    return active_name;
}

const char *tab_log_name(TabMode tab) {
    static char log_name[32];

    switch (tab) {
        case TAB_WINDOWS:    return "windows";
        case TAB_WORKSPACES: return "workspaces";
        case TAB_HARPOON:    return "harpoon";
        case TAB_NAMES:      return "names";
        case TAB_CONFIG:     return "config";
        case TAB_HOTKEYS:    return "hotkeys";
        case TAB_RULES:      return "rules";
        case TAB_APPS:       return "apps";
        case TAB_SESSIONS:   return "sessions";
        case TAB_COUNT:      return NULL;
    }

    const char *name = provider_tab_name(tab);
    if (!name) return NULL;
    size_t i = 0;
    for (; name[i] && i < sizeof(log_name) - 1; i++) {
        log_name[i] = (char)tolower((unsigned char)name[i]);
    }
    log_name[i] = '\0';
    return log_name;
}
