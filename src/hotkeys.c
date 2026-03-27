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
    char name[64];
} HotkeyDef;

// Active grabbed hotkeys (built from config at setup time)
static HotkeyDef active_hotkeys[3];
static int active_hotkey_count = 0;

// Grab with CapsLock/NumLock variants so hotkeys work regardless of lock state
static const unsigned int mod_variants[] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
#define NUM_MOD_VARIANTS ((int)(sizeof(mod_variants) / sizeof(mod_variants[0])))

// Parse "Mod1+Tab" → modifiers mask + KeySym. Returns 1 on success, 0 if disabled/invalid.
static int parse_hotkey(const char *spec, KeySym *sym_out, unsigned int *mod_out) {
    if (!spec || spec[0] == '\0') return 0;

    char buf[64];
    strncpy(buf, spec, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    unsigned int mod = 0;
    char *tok = strtok(buf, "+");
    char *next = tok ? strtok(NULL, "+") : NULL;

    while (tok && next) {
        if      (strcmp(tok, "Mod1")    == 0) mod |= Mod1Mask;
        else if (strcmp(tok, "Mod2")    == 0) mod |= Mod2Mask;
        else if (strcmp(tok, "Mod3")    == 0) mod |= Mod3Mask;
        else if (strcmp(tok, "Mod4")    == 0) mod |= Mod4Mask;
        else if (strcmp(tok, "Control") == 0) mod |= ControlMask;
        else if (strcmp(tok, "Shift")   == 0) mod |= ShiftMask;
        else {
            log_warn("Unknown modifier '%s' in hotkey '%s'", tok, spec);
            return 0;
        }
        tok  = next;
        next = strtok(NULL, "+");
    }

    // tok is now the key name
    KeySym sym = XStringToKeysym(tok);
    if (sym == NoSymbol) {
        log_warn("Unknown key name '%s' in hotkey '%s'", tok, spec);
        return 0;
    }

    *sym_out = sym;
    *mod_out = mod;
    return 1;
}

static int grab_error_occurred = 0;

static int grab_error_handler(Display *display, XErrorEvent *error) {
    (void)display;
    if (error->error_code == BadAccess)
        grab_error_occurred = 1;
    return 0;
}

static void ungrab_all(Display *display) {
    Window root = DefaultRootWindow(display);
    for (int i = 0; i < active_hotkey_count; i++) {
        KeyCode kc = XKeysymToKeycode(display, active_hotkeys[i].sym);
        if (kc == 0) continue;
        for (int v = 0; v < NUM_MOD_VARIANTS; v++)
            XUngrabKey(display, kc, active_hotkeys[i].mod | mod_variants[v], root);
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

// Build the active hotkey list from config
static void build_hotkey_list(AppData *app) {
    active_hotkey_count = 0;

    struct { const char *spec; ShowMode mode; } sources[] = {
        { app->config.hotkey_windows,    SHOW_MODE_WINDOWS    },
        { app->config.hotkey_command,    SHOW_MODE_COMMAND    },
        { app->config.hotkey_workspaces, SHOW_MODE_WORKSPACES },
    };

    for (int i = 0; i < 3; i++) {
        KeySym sym;
        unsigned int mod;
        if (!parse_hotkey(sources[i].spec, &sym, &mod)) {
            log_info("Hotkey for mode %d disabled or invalid: '%s'", sources[i].mode, sources[i].spec);
            continue;
        }
        active_hotkeys[active_hotkey_count].sym  = sym;
        active_hotkeys[active_hotkey_count].mod  = mod;
        active_hotkeys[active_hotkey_count].mode = sources[i].mode;
        snprintf(active_hotkeys[active_hotkey_count].name,
                 sizeof(active_hotkeys[active_hotkey_count].name),
                 "%s", sources[i].spec);
        active_hotkey_count++;
    }
}

void setup_hotkeys(AppData *app) {
    build_hotkey_list(app);

    Display *display = app->display;
    Window root = DefaultRootWindow(display);

    while (1) {
        char failed_keys[256] = "";
        XErrorHandler old_handler = XSetErrorHandler(grab_error_handler);

        for (int i = 0; i < active_hotkey_count; i++) {
            KeyCode kc = XKeysymToKeycode(display, active_hotkeys[i].sym);
            if (kc == 0) {
                log_warn("No keycode found for hotkey %s", active_hotkeys[i].name);
                if (failed_keys[0]) strncat(failed_keys, "\n", sizeof(failed_keys) - strlen(failed_keys) - 1);
                strncat(failed_keys, active_hotkeys[i].name, sizeof(failed_keys) - strlen(failed_keys) - 1);
                continue;
            }

            grab_error_occurred = 0;
            for (int v = 0; v < NUM_MOD_VARIANTS; v++)
                XGrabKey(display, kc, active_hotkeys[i].mod | mod_variants[v],
                         root, False, GrabModeAsync, GrabModeAsync);
            XSync(display, False);

            if (grab_error_occurred) {
                log_warn("Failed to grab hotkey %s (BadAccess)", active_hotkeys[i].name);
                if (failed_keys[0]) strncat(failed_keys, "\n", sizeof(failed_keys) - strlen(failed_keys) - 1);
                strncat(failed_keys, active_hotkeys[i].name, sizeof(failed_keys) - strlen(failed_keys) - 1);
            }
        }

        XSetErrorHandler(old_handler);

        if (failed_keys[0] == '\0') {
            log_info("All hotkeys registered (%d active)", active_hotkey_count);
            break;
        }

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

    for (int i = 0; i < active_hotkey_count; i++) {
        KeyCode kc = XKeysymToKeycode(app->display, active_hotkeys[i].sym);
        if (event->keycode == kc && clean_state == active_hotkeys[i].mod) {
            log_debug("Hotkey fired: %s", active_hotkeys[i].name);
            app->focus_timestamp = event->time;
            app->pending_hotkey_mode = (int)active_hotkeys[i].mode;
            g_idle_add(dispatch_hotkey_idle, app);
            return;
        }
    }
}
