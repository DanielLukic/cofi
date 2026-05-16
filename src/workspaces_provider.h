#ifndef WORKSPACES_PROVIDER_H
#define WORKSPACES_PROVIDER_H

#include "app_data.h"

void workspaces_provider_register(void);
void filter_workspaces(AppData *app, const char *filter);
WorkspaceInfo *workspaces_selected_workspace(AppData *app);

#endif /* WORKSPACES_PROVIDER_H */
