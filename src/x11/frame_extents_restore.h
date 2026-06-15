#ifndef FRAME_EXTENTS_RESTORE_H
#define FRAME_EXTENTS_RESTORE_H

#include <X11/Xlib.h>
#include <glib.h>

#ifndef APPDATA_TYPEDEF_DEFINED
#define APPDATA_TYPEDEF_DEFINED
typedef struct AppData AppData;
#endif

void handle_net_frame_extents_property(AppData *app, Window window_id);
void cleanup_frame_extents_restore_timeouts(void);

#ifdef COFI_TESTING
typedef guint (*FrameExtentsTimeoutAddFunc)(guint interval, GSourceFunc function, gpointer data);
typedef gboolean (*FrameExtentsSourceRemoveFunc)(guint source_id);
void frame_extents_restore_set_timeout_add_for_test(FrameExtentsTimeoutAddFunc timeout_add);
void frame_extents_restore_set_source_remove_for_test(FrameExtentsSourceRemoveFunc source_remove);
#endif

#endif // FRAME_EXTENTS_RESTORE_H
