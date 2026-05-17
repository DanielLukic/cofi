#ifndef AGENT_SESSIONS_PROVIDER_H
#define AGENT_SESSIONS_PROVIDER_H

#include "app_data.h"

void agent_sessions_provider_register(void);
TabMode agent_sessions_tab_mode(void);
void agent_sessions_provider_remove_path(AppData *app, const char *path);

#endif /* AGENT_SESSIONS_PROVIDER_H */
