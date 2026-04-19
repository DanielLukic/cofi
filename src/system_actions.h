#ifndef SYSTEM_ACTIONS_H
#define SYSTEM_ACTIONS_H

#include "apps.h"

void system_actions_load(AppEntry *out, int *count, int max);
void system_actions_invoke(const AppEntry *entry);

#endif
