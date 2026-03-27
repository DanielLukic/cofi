#include "hotkeys.h"
#include "app_data.h"
#include "types.h"
#include "log.h"
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>

// Defined in main.c — owns all the tab/mode switching logic
extern void dispatch_hotkey_mode(AppData *app, ShowMode mode);

static gboolean dispatch_hotkey_idle(gpointer data) {
    AppData *app = (AppData *)data;
    dispatch_hotkey_mode(app, (ShowMode)app->pending_hotkey_mode);
    app->pending_hotkey_mode = -1;
    return FALSE;
}

typedef struct {
    KeySym sym;
    unsigned int mod;
    ShowMode mode;
    const char *name;
} HotkeyDef;

static const HotkeyDef hotkey_defs[] = {
    { XK_Tab,       Mod1Mask, SHOW_MODE_WINDOWS,      "Alt+Tab"       },
    { XK_grave,     Mod1Mask, SHOW_MODE_COMMAND,      "Alt+`"         },
    { XK_BackSpace, Mod1Mask, SHOW_MODE_WORKSPACES,   "Alt+Backspace" },
    { XK_backslash, Mod1Mask, SHOW_MODE_ASSIGN_SLOTS, "Alt+\\"        },
};
#define NUM_HOTKEYS ((int)(sizeof(hotkey_defs) / sizeof(hotkey_defs[0])))

// Grab with CapsLock/NumLock variants so hotkeys work regardless of lock state
static const unsigned int mod_variants[] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
#define NUM_MOD_VARIANTS ((int)(sizeof(mod_variants) / sizeof(mod_variants[0])))

static int grab_error_occurred = 0;

static int grab_error_handler(Display *display, XErrorEvent *error) {
    (void)display;
    if (error->error_code == BadAccess)
        grab_error_occurred = 1;
    return 0;
}

static void ungrab_all(Display *display) {
    Window root = DefaultRootWindow(display);
    for (int i = 0; i < NUM_HOTKEYS; i++) {
        KeyCode kc = XKeysymToKeycode(display, hotkey_defs[i].sym);
        if (kc == 0) continue;
        for (int v = 0; v < NUM_MOD_VARIANTS; v++)
            XUngrabKey(display, kc, hotkey_defs[i].mod | mod_variants[v], root);
    }
    XFlush(display);
}

static void show_grab_failure_dialog(AppData *app, const char *failed_keys) {
    GtkWidget *dialog = gtk_message_dialog_new(
        app->window ? GTK_WINDOW(app->window) : NULL,
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_ERROR,
        GTK_BUTTONS_NONE,
        "Could not register hotkey(s):\n%s\n\n"
        "Another application may be using these shortcuts.\n"
        "Remove conflicting shortcuts in System Settings → Keyboard Shortcuts,\n"
        "then click Retry.",
        failed_keys);

    gtk_dialog_add_button(GTK_DIALOG(dialog), "Retry", GTK_RESPONSE_ACCEPT);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Exit",  GTK_RESPONSE_CANCEL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (response != GTK_RESPONSE_ACCEPT) {
        log_info("User chose Exit on hotkey grab failure");
        exit(1);
    }
}

void setup_hotkeys(AppData *app) {
    Display *display = app->display;
    Window root = DefaultRootWindow(display);

    while (1) {
        char failed_keys[256] = "";
        XErrorHandler old_handler = XSetErrorHandler(grab_error_handler);

        for (int i = 0; i < NUM_HOTKEYS; i++) {
            KeyCode kc = XKeysymToKeycode(display, hotkey_defs[i].sym);
            if (kc == 0) {
                log_warn("No keycode found for hotkey %s", hotkey_defs[i].name);
                if (failed_keys[0]) strncat(failed_keys, "\n", sizeof(failed_keys) - strlen(failed_keys) - 1);
                strncat(failed_keys, hotkey_defs[i].name, sizeof(failed_keys) - strlen(failed_keys) - 1);
                continue;
            }

            grab_error_occurred = 0;
            for (int v = 0; v < NUM_MOD_VARIANTS; v++)
                XGrabKey(display, kc, hotkey_defs[i].mod | mod_variants[v],
                         root, False, GrabModeAsync, GrabModeAsync);
            XSync(display, False);

            if (grab_error_occurred) {
                log_warn("Failed to grab hotkey %s (BadAccess)", hotkey_defs[i].name);
                if (failed_keys[0]) strncat(failed_keys, "\n", sizeof(failed_keys) - strlen(failed_keys) - 1);
                strncat(failed_keys, hotkey_defs[i].name, sizeof(failed_keys) - strlen(failed_keys) - 1);
            }
        }

        XSetErrorHandler(old_handler);

        if (failed_keys[0] == '\0') {
            log_info("All hotkeys registered");
            break;
        }

        // Release whatever we managed to grab before showing dialog
        ungrab_all(display);
        show_grab_failure_dialog(app, failed_keys);
        log_info("Retrying hotkey registration");
    }
}

void cleanup_hotkeys(AppData *app) {
    ungrab_all(app->display);
    log_debug("Hotkeys unregistered");
}

void handle_hotkey_event(AppData *app, XKeyEvent *event) {
    unsigned int clean_state = event->state & ~(LockMask | Mod2Mask);

    for (int i = 0; i < NUM_HOTKEYS; i++) {
        KeyCode kc = XKeysymToKeycode(app->display, hotkey_defs[i].sym);
        if (event->keycode == kc && clean_state == hotkey_defs[i].mod) {
            log_debug("Hotkey fired: %s", hotkey_defs[i].name);
            app->focus_timestamp = event->time;
            app->pending_hotkey_mode = (int)hotkey_defs[i].mode;
            g_idle_add(dispatch_hotkey_idle, app);
            return;
        }
    }
}
