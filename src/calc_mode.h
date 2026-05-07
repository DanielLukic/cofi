#ifndef CALC_MODE_H
#define CALC_MODE_H

#include <gtk/gtk.h>
#include "app_data.h"

void enter_calc_mode(AppData *app);
void exit_calc_mode(AppData *app);
gboolean handle_calc_key(GdkEventKey *event, AppData *app);

#endif /* CALC_MODE_H */
