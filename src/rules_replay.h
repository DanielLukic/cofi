#ifndef RULES_REPLAY_H
#define RULES_REPLAY_H

#include <glib.h>

#include "app_data.h"

int replay_rule_against_open_windows(AppData *app, const Rule *rule);
int replay_all_rules_against_open_windows(AppData *app);
gboolean replay_selected_filtered_rule(AppData *app);

#endif
