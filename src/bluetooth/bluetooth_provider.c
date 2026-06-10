#include "bluetooth/bluetooth_provider.h"

#include "bluetooth/bluetooth_bluez.h"
#include "bluetooth/bluetooth_model.h"
#include "commands/command_mode.h"
#include "commands/command_registry.h"
#include "core/app/app_data.h"
#include "providers/cofi_tab_provider.h"
#include "ui/display.h"
#include "ui/tab_switching.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <string.h>

static CofiTabProvider s_bluetooth_provider;
static int s_bluetooth_provider_id = -1;

static TabMode bluetooth_tab_mode(void) {
    const CofiTabProvider *provider = cofi_get_provider(s_bluetooth_provider_id);
    return provider ? (TabMode)provider->tab_mode : TAB_WINDOWS;
}

static int bluetooth_row_count(AppData *app) {
    return bluetooth_model_row_count(app);
}

static void bluetooth_format_row(AppData *app, int raw_idx, CofiRowCells *out) {
    const BtDeviceEntry *device = bluetooth_model_device_at_visible(app, raw_idx);
    if (!device) {
        out->cell_count = 1;
        out->cells[0].text = bluetooth_model_status_message(app);
        out->row_flags = bluetooth_bluez_bus_state() == BUS_FAILED ? COFI_ROW_ERROR : 0;
        return;
    }

    gboolean show_adapter = bluetooth_model_show_adapter_column(app);
    out->cell_count = show_adapter ? 5 : 4;
    out->cells[0].text = bluetooth_connected_glyph(device->connected);
    out->cells[0].width_hint = 2;
    out->cells[1].text = bluetooth_icon_glyph(device->icon);
    out->cells[1].width_hint = 2;
    out->cells[2].text = device->alias;
    out->cells[2].width_hint = 28;
    if (show_adapter) {
        out->cells[3].text = device->adapter_name;
        out->cells[3].width_hint = 18;
        out->cells[4].text = bluetooth_model_device_state_text((BtDeviceEntry *)device);
    } else {
        out->cells[3].text = bluetooth_model_device_state_text((BtDeviceEntry *)device);
    }
    out->row_flags = COFI_ROW_ACTIONABLE;
}

static const char *bluetooth_match_string(AppData *app, int raw_idx) {
    return bluetooth_model_match_string(app, raw_idx);
}

static const char *bluetooth_row_identity(AppData *app, int raw_idx) {
    return bluetooth_model_row_identity(app, raw_idx);
}

static void bluetooth_on_enter(AppData *app) {
    if (!app) {
        return;
    }
    if (app->entry) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry),
                                       "Bluetooth devices...");
    }
    bluetooth_model_on_enter(app);
}

static void bluetooth_on_leave(AppData *app) {
    bluetooth_model_on_leave(app);
}

static void bluetooth_on_tick(AppData *app, int generation) {
    bluetooth_model_on_tick(app, generation);
}

static void bluetooth_on_query_changed(AppData *app, const char *query) {
    bluetooth_model_on_query_changed(app, query);
}

static CofiActionStatus bluetooth_on_enter_pressed(AppData *app,
                                                   int filtered_idx,
                                                   int raw_idx,
                                                   const char *entry_text,
                                                   int modifier_state) {
    (void)filtered_idx;
    (void)entry_text;
    (void)modifier_state;
    return bluetooth_model_toggle_device(app, raw_idx)
        ? COFI_HANDLED_KEEP
        : COFI_NO_OP;
}

static gboolean bluetooth_handle_key(GdkEventKey *event, AppData *app) {
    if (!event || !app) {
        return FALSE;
    }

    int provider_id = cofi_get_provider_id_for_tab(app->current_tab);
    int selected = app->selection.provider_index;
    int raw = cofi_filtered_to_raw(provider_id, selected);
    if (raw < 0) {
        raw = selected;
    }

    gboolean handled = FALSE;
    switch (event->keyval) {
        case GDK_KEY_c:
        case GDK_KEY_C:
            handled = bluetooth_model_connect_device(app, raw);
            break;
        case GDK_KEY_d:
        case GDK_KEY_D:
            handled = bluetooth_model_disconnect_device(app, raw);
            break;
        case GDK_KEY_r:
        case GDK_KEY_R:
            bluetooth_model_refresh(app);
            handled = TRUE;
            break;
        default:
            return FALSE;
    }

    if (handled) {
        update_display(app);
    }
    return handled;
}

static gboolean bluetooth_command_handler(AppData *app,
                                          WindowInfo *window __attribute__((unused)),
                                          const char *args __attribute__((unused))) {
    exit_command_mode(app);
    if (app) {
        app->prefix_origin_tab = app->current_tab;
    }
    surface_tab(app, bluetooth_tab_mode());
    return FALSE;
}

static const CommandSpec s_bluetooth_command = {
    .primary = "bluetooth",
    .aliases = {"bt", NULL},
    .owner_provider_id = "bluetooth",
    .handler = bluetooth_command_handler,
    .description = "Switch to bluetooth devices tab",
    .help_format = "bluetooth, bt",
    .keeps_open_on_hotkey_auto = 1
};

void bluetooth_provider_register(void) {
    cofi_init_provider_defaults(&s_bluetooth_provider);
    s_bluetooth_provider_id = -1;
    s_bluetooth_provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    s_bluetooth_provider.id = "bluetooth";
    s_bluetooth_provider.display_name = "BLUETOOTH";
    s_bluetooth_provider.shortcut_hint = "Shortcuts: Enter=Toggle  c=Connect  d=Disconnect  r=Refresh";
    s_bluetooth_provider.hidden_by_default = 1;
    s_bluetooth_provider.row_count = bluetooth_row_count;
    s_bluetooth_provider.format_row = bluetooth_format_row;
    s_bluetooth_provider.match_string = bluetooth_match_string;
    s_bluetooth_provider.row_identity = bluetooth_row_identity;
    s_bluetooth_provider.on_enter = bluetooth_on_enter;
    s_bluetooth_provider.on_leave = bluetooth_on_leave;
    s_bluetooth_provider.on_tick = bluetooth_on_tick;
    s_bluetooth_provider.tick_interval_ms = 3000;
    s_bluetooth_provider.on_query_changed = bluetooth_on_query_changed;
    s_bluetooth_provider.handle_key = bluetooth_handle_key;
    s_bluetooth_provider.on_enter_pressed = bluetooth_on_enter_pressed;
    s_bluetooth_provider_id = cofi_register_tab_provider(&s_bluetooth_provider);
    if (s_bluetooth_provider_id >= 0) {
        cofi_register_command(&s_bluetooth_command);
    }
}
