#ifndef HOTKEYS_H
#define HOTKEYS_H

#include <X11/Xlib.h>

typedef struct AppData AppData;

// Register all system hotkeys via XGrabKey. Shows a Retry/Exit dialog on failure.
void setup_hotkeys(AppData *app);

// Unregister all system hotkeys.
void cleanup_hotkeys(AppData *app);

// Call from handle_x11_event when a KeyPress event is received.
void handle_hotkey_event(AppData *app, XKeyEvent *event);


#endif // HOTKEYS_H
