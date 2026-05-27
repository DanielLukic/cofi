#ifndef SESSIONS_PROVIDER_H
#define SESSIONS_PROVIDER_H

#include "core/app/app_data.h"

void sessions_provider_register(void);
TabMode sessions_tab_mode(void);
void sessions_provider_remove_path(AppData *app, const char *path);
void sessions_provider_rename_path(AppData *app,
                                         const char *path,
                                         const char *name);

#endif /* SESSIONS_PROVIDER_H */
