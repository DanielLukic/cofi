#ifndef COFI_MODAL_H
#define COFI_MODAL_H

#include <glib.h>
#include <gdk/gdk.h>

#ifndef APPDATA_TYPEDEF_DEFINED
typedef struct AppData AppData;
#define APPDATA_TYPEDEF_DEFINED
#endif

#include "providers/cofi_tab_provider.h"

void     cofi_enter_modal(AppData *app, const CofiTabProvider *provider);
void     cofi_exit_modal(AppData *app);
gboolean cofi_handle_modal_key(AppData *app, GdkEventKey *event);

#endif /* COFI_MODAL_H */
