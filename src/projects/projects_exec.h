#ifndef PROJECTS_EXEC_H
#define PROJECTS_EXEC_H

#include <glib.h>
#include <stddef.h>

#include "config/config.h"
#include "projects/projects.h"

typedef enum {
    PROJECT_TOOL_TMUX,
    PROJECT_TOOL_ZELLIJ,
    PROJECT_TOOL_ZOXIDE,
} ProjectTool;

const char *projects_tool_name(ProjectTool tool);
const char *projects_tool_config_value(const CofiConfig *config, ProjectTool tool);
gchar *projects_resolve_tool(const CofiConfig *config, ProjectTool tool,
                             char *err_buf, size_t err_size);

#endif /* PROJECTS_EXEC_H */
