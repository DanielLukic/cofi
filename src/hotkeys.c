#include "hotkeys.h"
#include "app_data.h"
#include "hotkey_config.h"
#include "command_mode.h"
#include "x11_utils.h"
#include "types.h"
#include "log.h"
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>

extern void show_window(AppData *app);
extern void enter_command_mode(AppData *app);

typedef struct {
    KeySym sym;
    unsigned int mod;
    char key_name[64];
    char command[256];
} GrabbedHotkey;

static GrabbedHotkey grabbed_hotkeys[MAX_HOTKEY_BINDINGS];
static int grabbed_count = 0;

static const unsigned int mod_variants[] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
#define NUM_MOD_VARIANTS ((int)(sizeof(mod_variants) / sizeof(mod_variants[0])))

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
    for (int i = 0; i < grabbed_count; i++) {
        KeyCode kc = XKeysymToKeycode(display, grabbed_hotkeys[i].sym);
        if (kc == 0) continue;
        for (int v = 0; v < NUM_MOD_VARIANTS; v++)
            XUngrabKey(display, kc, grabbed_hotkeys[i].mod | mod_variants[v], root);
    }
    XFlush(display);
}

static void show_grab_failure_dialog(AppData *app, const char *failed_keys) {
    GtkWidget *dialog = gtk_message_dialog_new(
        app->window ? GTK_WINDOW(app->window) : NULL,
        GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_NONE,
        "Could not register hotkey(s):\n%s\n\n"
        "Another application may be using these shortcuts.\n"
        "Remove conflicting shortcuts in System Settings → Keyboard Shortcuts,\n"
        "then click Retry.",
        failed_keys);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Retry", GTK_RESPONSE_ACCEPT);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Exit",  GTK_RESPONSE_CANCEL);
    gtk_window_set_title(GTK_WINDOW(dialog), "Cofi — Hotkey Conflict");
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    if (response != GTK_RESPONSE_ACCEPT) {
        log_info("User chose Exit on hotkey grab failure");
        exit(1);
    }
}

static void build_hotkey_list(AppData *app) {
    grabbed_count = 0;
    HotkeyConfig *hc = &app->hotkey_config;

    for (int i = 0; i < hc->count && grabbed_count < MAX_HOTKEY_BINDINGS; i++) {
        KeySym sym;
        unsigned int mod;
        if (!parse_hotkey(hc->bindings[i].key, &sym, &mod)) {
            log_info("Hotkey disabled or invalid: '%s'", hc->bindings[i].key);
            continue;
        }
        grabbed_hotkeys[grabbed_count].sym = sym;
        grabbed_hotkeys[grabbed_count].mod = mod;
        strncpy(grabbed_hotkeys[grabbed_count].key_name, hc->bindings[i].key,
                sizeof(grabbed_hotkeys[grabbed_count].key_name) - 1);
        strncpy(grabbed_hotkeys[grabbed_count].command, hc->bindings[i].command,
                sizeof(grabbed_hotkeys[grabbed_count].command) - 1);
        grabbed_count++;
    }
}

static void append_failed_key(char *failed_keys, size_t size, const char *key_name) {
    if (failed_keys[0])
        strncat(failed_keys, "\n", size - strlen(failed_keys) - 1);
    strncat(failed_keys, key_name, size - strlen(failed_keys) - 1);
}

void setup_hotkeys(AppData *app) {
    build_hotkey_list(app);

    Display *display = app->display;
    Window root = DefaultRootWindow(display);

    while (1) {
        char failed_keys[256] = "";
        XErrorHandler old_handler = XSetErrorHandler(grab_error_handler);

        for (int i = 0; i < grabbed_count; i++) {
            KeyCode kc = XKeysymToKeycode(display, grabbed_hotkeys[i].sym);
            if (kc == 0) {
                log_warn("No keycode for hotkey %s", grabbed_hotkeys[i].key_name);
                append_failed_key(failed_keys, sizeof(failed_keys), grabbed_hotkeys[i].key_name);
                continue;
            }

            grab_error_occurred = 0;
            for (int v = 0; v < NUM_MOD_VARIANTS; v++)
                XGrabKey(display, kc, grabbed_hotkeys[i].mod | mod_variants[v],
                         root, False, GrabModeAsync, GrabModeAsync);
            XSync(display, False);

            if (grab_error_occurred) {
                log_warn("Failed to grab hotkey %s (BadAccess)", grabbed_hotkeys[i].key_name);
                append_failed_key(failed_keys, sizeof(failed_keys), grabbed_hotkeys[i].key_name);
            }
        }

        XSetErrorHandler(old_handler);

        if (failed_keys[0] == '\0') {
            log_info("All hotkeys registered (%d active)", grabbed_count);
            break;
        }

        ungrab_all(display);
        show_grab_failure_dialog(app, failed_keys);
        log_info("Retrying hotkey registration");
    }
}

void cleanup_hotkeys(AppData *app) {
    ungrab_all(app->display);
    grabbed_count = 0;
    log_debug("Hotkeys unregistered");
}

void regrab_hotkeys(AppData *app) {
    cleanup_hotkeys(app);
    setup_hotkeys(app);
    log_info("Hotkeys re-grabbed after config change");
}

typedef struct {
    AppData *app;
    char command[256];
    guint32 timestamp;
} HotkeyDispatch;

static void prefill_command_mode(AppData *app, const char *command) {
    show_window(app);
    if (app->command_mode.state != CMD_MODE_COMMAND)
        enter_command_mode(app);
    strncpy(app->command_mode.command_buffer, command,
            sizeof(app->command_mode.command_buffer) - 1);
    app->command_mode.cursor_pos = (int)strlen(app->command_mode.command_buffer);
    gtk_entry_set_text(GTK_ENTRY(app->entry), command);
    gtk_editable_set_position(GTK_EDITABLE(app->entry), -1);
}

static WindowInfo* find_window_by_id(AppData *app, Window id) {
    for (int i = 0; i < app->window_count; i++) {
        if (app->windows[i].id == id) return &app->windows[i];
    }
    return NULL;
}

static gboolean hotkey_dispatch_idle(gpointer data) {
    HotkeyDispatch *hd = (HotkeyDispatch *)data;
    AppData *app = hd->app;
    char *command = hd->command;

    app->focus_timestamp = hd->timestamp;
    app->pending_hotkey_mode = -1;

    size_t len = strlen(command);
    int auto_execute = (len > 0 && command[len - 1] == '!');
    if (auto_execute) command[len - 1] = '\0';

    if (auto_execute) {
        Window active = get_active_window_id(app->display);
        WindowInfo *target = (active && active != app->own_window_id)
                           ? find_window_by_id(app, active) : NULL;
        execute_command_with_window(command, app, target);
    } else {
        prefill_command_mode(app, command);
    }

    free(hd);
    return FALSE;
}

void handle_hotkey_event(AppData *app, XKeyEvent *event) {
    unsigned int clean_state = event->state & ~(LockMask | Mod2Mask);

    for (int i = 0; i < grabbed_count; i++) {
        KeyCode kc = XKeysymToKeycode(app->display, grabbed_hotkeys[i].sym);
        if (event->keycode == kc && clean_state == grabbed_hotkeys[i].mod) {
            log_debug("Hotkey fired: %s → %s", grabbed_hotkeys[i].key_name, grabbed_hotkeys[i].command);

            if (app->window_visible && app->focus_loss_timer > 0) {
                g_source_remove(app->focus_loss_timer);
                app->focus_loss_timer = 0;
            }

            app->pending_hotkey_mode = 1;

            HotkeyDispatch *hd = malloc(sizeof(HotkeyDispatch));
            if (!hd) {
                log_error("Failed to allocate HotkeyDispatch");
                app->pending_hotkey_mode = -1;
                return;
            }
            hd->app = app;
            hd->timestamp = event->time;
            strncpy(hd->command, grabbed_hotkeys[i].command, sizeof(hd->command) - 1);
            hd->command[sizeof(hd->command) - 1] = '\0';
            g_idle_add(hotkey_dispatch_idle, hd);
            return;
        }
    }
}
