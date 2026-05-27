#ifndef TAB_HEADER_H
#define TAB_HEADER_H

#include <glib.h>

#include "core/app/app_data.h"

void tab_header_format(AppData *app, TabMode current_tab, int max_columns,
                       GString *output);

#endif /* TAB_HEADER_H */
