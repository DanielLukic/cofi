#ifndef RULES_DISPATCH_H
#define RULES_DISPATCH_H

#include <X11/Xlib.h>
#include "rules/rules.h"

typedef struct AppData AppData;

void rules_apply(AppData *app, RuleTrigger trigger,
                 const Window *new_window_ids, int new_window_count);
void rules_apply_for_title_change(AppData *app, Window window_id);

#endif // RULES_DISPATCH_H
