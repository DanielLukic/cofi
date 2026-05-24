#ifndef GEOM_RULE_SYNC_H
#define GEOM_RULE_SYNC_H

typedef struct AppData AppData;

int geom_rule_sync_for_pattern(AppData *app, const char *pattern);
int geom_rule_sync_all_layout_patterns(AppData *app);

#endif
