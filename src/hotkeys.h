#ifndef HOTKEYS_H
#define HOTKEYS_H

#include <X11/Xlib.h>
#include <string.h>
#include "hotkey_config.h"

typedef struct AppData AppData;

typedef struct {
    KeySym sym;
    unsigned int mod;
    char key_name[64];
    char command[256];
} GrabbedHotkey;

typedef struct {
    GrabbedHotkey grabbed_hotkeys[MAX_HOTKEY_BINDINGS];
    int grabbed_count;
} HotkeyGrabState;

int populate_hotkey_grab_state(const HotkeyConfig *config, HotkeyGrabState *state);

static inline void init_hotkey_grab_state(HotkeyGrabState *state) {
    if (!state) {
        return;
    }
    memset(state, 0, sizeof(*state));
}

// Register all system hotkeys via XGrabKey. Shows a Retry/Exit dialog on failure.
void setup_hotkeys(AppData *app);

// Unregister all system hotkeys.
void cleanup_hotkeys(AppData *app);

// Unregister then re-register all hotkeys (call after bind/unbind).
void regrab_hotkeys(AppData *app);

// Call from handle_x11_event when a KeyPress event is received.
void handle_hotkey_event(AppData *app, XKeyEvent *event);


#endif // HOTKEYS_H
