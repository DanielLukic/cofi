#include "projects/projects_exec.h"

#include <string.h>

#include "core/log/log.h"

const char *projects_tool_name(ProjectTool tool) {
    switch (tool) {
        case PROJECT_TOOL_TMUX: return "tmux";
        case PROJECT_TOOL_ZELLIJ: return "zellij";
        case PROJECT_TOOL_ZOXIDE: return "zoxide";
        default: return "";
    }
}

const char *projects_tool_config_value(const CofiConfig *config, ProjectTool tool) {
    if (!config) return "";
    switch (tool) {
        case PROJECT_TOOL_TMUX: return config->projects_tmux_path;
        case PROJECT_TOOL_ZELLIJ: return config->projects_zellij_path;
        case PROJECT_TOOL_ZOXIDE: return config->projects_zoxide_path;
        default: return "";
    }
}

gchar *projects_resolve_tool(const CofiConfig *config, ProjectTool tool,
                             char *err_buf, size_t err_size) {
    const char *name = projects_tool_name(tool);
    const char *configured = projects_tool_config_value(config, tool);
    if (configured && configured[0] != '\0') {
        if (g_file_test(configured, G_FILE_TEST_IS_REGULAR) &&
            g_file_test(configured, G_FILE_TEST_IS_EXECUTABLE)) {
            return g_strdup(configured);
        }
        if (err_buf && err_size > 0) {
            g_snprintf(err_buf, err_size, "%s path is not executable: %s",
                       name, configured);
        }
        log_warn("projects: configured %s path is not executable: %s",
                 name, configured);
        return NULL;
    }

    gchar *resolved = g_find_program_in_path(name);
    if (!resolved) {
        if (err_buf && err_size > 0) {
            g_snprintf(err_buf, err_size, "%s not found on PATH", name);
        }
        log_warn("projects: %s not found on PATH", name);
    }
    return resolved;
}
