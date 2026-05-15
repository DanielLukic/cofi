#include "tab_metadata.h"

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
        case TAB_CALC:       return "Calc";
        case TAB_SINKS:      return "Sinks";
        case TAB_RUN:        return "Run";
        case TAB_PROC:       return "Proc";
        case TAB_TMUX:       return "Tmux";
        case TAB_COUNT:      return NULL;
    }
    return NULL;
}

const char *tab_active_name(TabMode tab) {
    switch (tab) {
        case TAB_WINDOWS:    return "WINDOWS";
        case TAB_WORKSPACES: return "WORKSPACES";
        case TAB_HARPOON:    return "HARPOON";
        case TAB_NAMES:      return "NAMES";
        case TAB_CONFIG:     return "CONFIG";
        case TAB_HOTKEYS:    return "HOTKEYS";
        case TAB_RULES:      return "RULES";
        case TAB_APPS:       return "APPS";
        case TAB_CALC:       return "CALC";
        case TAB_SINKS:      return "SINKS";
        case TAB_RUN:        return "RUN";
        case TAB_PROC:       return "PROC";
        case TAB_TMUX:       return "TMUX";
        case TAB_COUNT:      return NULL;
    }
    return NULL;
}

const char *tab_log_name(TabMode tab) {
    switch (tab) {
        case TAB_WINDOWS:    return "windows";
        case TAB_WORKSPACES: return "workspaces";
        case TAB_HARPOON:    return "harpoon";
        case TAB_NAMES:      return "names";
        case TAB_CONFIG:     return "config";
        case TAB_HOTKEYS:    return "hotkeys";
        case TAB_RULES:      return "rules";
        case TAB_APPS:       return "apps";
        case TAB_CALC:       return "calc";
        case TAB_SINKS:      return "sinks";
        case TAB_RUN:        return "run";
        case TAB_PROC:       return "proc";
        case TAB_TMUX:       return "tmux";
        case TAB_COUNT:      return NULL;
    }
    return NULL;
}
