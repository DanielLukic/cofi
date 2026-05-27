#ifndef WORKSPACES_PROVIDER_H
#define WORKSPACES_PROVIDER_H

#include "core/app/app_data.h"

TabMode workspaces_tab_mode(void);
void workspaces_provider_register(void);
void filter_workspaces(AppData *app, const char *filter);
WorkspaceInfo *workspaces_selected_workspace(AppData *app);

#endif /* WORKSPACES_PROVIDER_H */
