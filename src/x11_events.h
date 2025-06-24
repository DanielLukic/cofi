#ifndef X11_EVENTS_H
#define X11_EVENTS_H

#include <X11/Xlib.h>
#include <glib.h>
#include "app_data.h"

// Initialize X11 event monitoring
void setup_x11_event_monitoring(AppData *app);

// Cleanup X11 event monitoring
void cleanup_x11_event_monitoring(void);

// Process X11 events (called by GLib)
gboolean process_x11_events(GIOChannel *source, GIOCondition condition, gpointer data);

// Handle individual X11 events
void handle_x11_event(AppData *app, XEvent *event);

#endif // X11_EVENTS_H