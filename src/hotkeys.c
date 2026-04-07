#include "hotkeys.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

#include "app_data.h"
#include "command_mode.h"
#include "hotkey_config.h"
#include "log.h"
#include "types.h"
#include "x11_utils.h"

extern void show_window(AppData *app);
extern void hide_window(AppData *app);
extern void enter_command_mode(AppData *app);

static const unsigned int mod_variants[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask};
#define NUM_MOD_VARIANTS ((int)(sizeof(mod_variants) / sizeof(mod_variants[0])))

static int grab_error_occurred = 0;

static int grab_error_handler(Display *display, XErrorEvent *error) {
    (void)display;
    if (error->error_code == BadAccess) {
        grab_error_occurred = 1;
    }
    return 0;
}

static int find_keycodes_for_sym(Display *display, KeySym sym,
                                 KeyCode *out, int max_out) {
    int min_kc = 0;
    int max_kc = 0;
    XDisplayKeycodes(display, &min_kc, &max_kc);

    int keycode_count = max_kc - min_kc + 1;
    int syms_per_keycode = 0;
    KeySym *map = XGetKeyboardMapping(display, (KeyCode)min_kc,
                                      keycode_count, &syms_per_keycode);
    if (!map) {
        return 0;
    }

    int count = 0;
    for (int keycode = min_kc; keycode <= max_kc && count < max_out; keycode++) {
        KeySym *syms = map + (keycode - min_kc) * syms_per_keycode;
        for (int col = 0; col < syms_per_keycode; col++) {
            if (syms[col] == sym) {
                out[count++] = (KeyCode)keycode;
                break;
            }
        }
    }

    XFree(map);
    return count;
}

static void ungrab_all(Display *display, const HotkeyGrabState *state) {
    Window root = DefaultRootWindow(display);
    KeyCode keycodes[16];

    for (int i = 0; i < state->grabbed_count; i++) {
        int keycode_count = find_keycodes_for_sym(display,
                                                  state->grabbed_hotkeys[i].sym,
                                                  keycodes, 16);
        for (int k = 0; k < keycode_count; k++) {
            for (int v = 0; v < NUM_MOD_VARIANTS; v++) {
                XUngrabKey(display, keycodes[k],
                           state->grabbed_hotkeys[i].mod | mod_variants[v],
                           root);
            }
        }
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
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Exit", GTK_RESPONSE_CANCEL);
    gtk_window_set_title(GTK_WINDOW(dialog), "Cofi — Hotkey Conflict");
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (response != GTK_RESPONSE_ACCEPT) {
        log_info("User chose Exit on hotkey grab failure");
        exit(1);
    }
}


static void append_failed_key(char *failed_keys, size_t size, const char *key_name) {
    if (failed_keys[0] != '\0') {
        strncat(failed_keys, "\n", size - strlen(failed_keys) - 1);
    }
    strncat(failed_keys, key_name, size - strlen(failed_keys) - 1);
}

void setup_hotkeys(AppData *app) {
    HotkeyGrabState *state = &app->hotkey_grab_state;
    populate_hotkey_grab_state(&app->hotkey_config, state);

    Display *display = app->display;
    Window root = DefaultRootWindow(display);

    while (1) {
        char failed_keys[256] = "";
        XErrorHandler old_handler = XSetErrorHandler(grab_error_handler);

        for (int i = 0; i < state->grabbed_count; i++) {
            KeyCode keycodes[16];
            int keycode_count = find_keycodes_for_sym(display,
                                                      state->grabbed_hotkeys[i].sym,
                                                      keycodes, 16);
            if (keycode_count == 0) {
                log_warn("No keycode for hotkey %s", state->grabbed_hotkeys[i].key_name);
                append_failed_key(failed_keys, sizeof(failed_keys), state->grabbed_hotkeys[i].key_name);
                continue;
            }

            grab_error_occurred = 0;
            for (int k = 0; k < keycode_count; k++) {
                for (int v = 0; v < NUM_MOD_VARIANTS; v++) {
                    XGrabKey(display, keycodes[k],
                             state->grabbed_hotkeys[i].mod | mod_variants[v],
                             root, False, GrabModeAsync, GrabModeAsync);
                }
            }

            XSync(display, False);
            if (!grab_error_occurred) {
                continue;
            }

            log_warn("Failed to grab hotkey %s (BadAccess)", state->grabbed_hotkeys[i].key_name);
            append_failed_key(failed_keys, sizeof(failed_keys), state->grabbed_hotkeys[i].key_name);
        }

        XSetErrorHandler(old_handler);

        if (failed_keys[0] == '\0') {
            log_info("All hotkeys registered (%d active)", state->grabbed_count);
            break;
        }

        ungrab_all(display, state);
        show_grab_failure_dialog(app, failed_keys);
        log_info("Retrying hotkey registration");
    }
}

void cleanup_hotkeys(AppData *app) {
    HotkeyGrabState *state = &app->hotkey_grab_state;
    ungrab_all(app->display, state);
    init_hotkey_grab_state(state);
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
    if (app->command_mode.state != CMD_MODE_COMMAND) {
        enter_command_mode(app);
    }

    strncpy(app->command_mode.command_buffer, command,
            sizeof(app->command_mode.command_buffer) - 1);
    app->command_mode.command_buffer[sizeof(app->command_mode.command_buffer) - 1] = '\0';
    app->command_mode.cursor_pos = (int)strlen(app->command_mode.command_buffer);
    gtk_entry_set_text(GTK_ENTRY(app->entry), command);
    gtk_editable_set_position(GTK_EDITABLE(app->entry), -1);
}

static WindowInfo *find_window_by_id(AppData *app, Window id) {
    for (int i = 0; i < app->window_count; i++) {
        if (app->windows[i].id == id) {
            return &app->windows[i];
        }
    }
    return NULL;
}

static gboolean hotkey_dispatch_idle(gpointer data) {
    HotkeyDispatch *dispatch = (HotkeyDispatch *)data;
    AppData *app = dispatch->app;
    char *command = dispatch->command;

    app->focus_timestamp = dispatch->timestamp;
    app->pending_hotkey_mode = -1;

    size_t len = strlen(command);
    int auto_execute = (len > 0 && command[len - 1] == '!');
    if (auto_execute) {
        command[len - 1] = '\0';
    }

    if (auto_execute) {
        gboolean keeps_open = should_keep_open_on_hotkey_auto(command);
        if (keeps_open) {
            show_window(app);
        }

        Window active = get_active_window_id(app->display);
        WindowInfo *target = (active && active != app->own_window_id)
                                 ? find_window_by_id(app, active)
                                 : NULL;
        execute_command_with_window(command, app, target);

        if (!keeps_open && app->window_visible) {
            hide_window(app);
        }
    } else {
        prefill_command_mode(app, command);
    }

    free(dispatch);
    return FALSE;
}

void handle_hotkey_event(AppData *app, XKeyEvent *event) {
    HotkeyGrabState *state = &app->hotkey_grab_state;
    unsigned int clean_state = event->state & ~(LockMask | Mod2Mask);

    for (int i = 0; i < state->grabbed_count; i++) {
        KeyCode keycodes[16];
        int keycode_count = find_keycodes_for_sym(app->display,
                                                  state->grabbed_hotkeys[i].sym,
                                                  keycodes, 16);

        gboolean keycode_matches = FALSE;
        for (int k = 0; k < keycode_count; k++) {
            if (keycodes[k] == (KeyCode)event->keycode) {
                keycode_matches = TRUE;
                break;
            }
        }

        if (!keycode_matches || clean_state != state->grabbed_hotkeys[i].mod) {
            continue;
        }

        log_debug("Hotkey fired: %s → %s",
                  state->grabbed_hotkeys[i].key_name,
                  state->grabbed_hotkeys[i].command);

        if (app->window_visible && app->focus_loss_timer > 0) {
            g_source_remove(app->focus_loss_timer);
            app->focus_loss_timer = 0;
        }

        app->pending_hotkey_mode = 1;

        HotkeyDispatch *dispatch = malloc(sizeof(HotkeyDispatch));
        if (!dispatch) {
            log_error("Failed to allocate HotkeyDispatch");
            app->pending_hotkey_mode = -1;
            return;
        }

        dispatch->app = app;
        dispatch->timestamp = event->time;
        strncpy(dispatch->command, state->grabbed_hotkeys[i].command,
                sizeof(dispatch->command) - 1);
        dispatch->command[sizeof(dispatch->command) - 1] = '\0';
        g_idle_add(hotkey_dispatch_idle, dispatch);
        return;
    }
}
