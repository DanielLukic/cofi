#include "sinks_provider.h"

#include "app_data.h"
#include "command_mode.h"
#include "command_registry.h"
#include "cofi_tab_provider.h"
#include "harpoon_config.h"
#include "log.h"
#include "match.h"
#include "selection.h"
#include "sinks.h"
#include "slot_store.h"
#include "tab_switching.h"
#include "window_lifecycle.h"

#include <gtk/gtk.h>
#include <string.h>

static CofiTabProvider s_sinks_provider;
static int s_sinks_provider_id = -1;

static TabMode sinks_tab_mode(void) {
    const CofiTabProvider *provider = cofi_get_provider(s_sinks_provider_id);
    return provider ? (TabMode)provider->tab_mode : TAB_WINDOWS;
}

static int sinks_row_count(AppData *app) {
    if (!app) return 0;
    if (app->sinks_mode.last_error[0] != '\0') return 1;
    if (app->sinks_mode.sink_count == 0) return 1;
    if (app->sinks_mode.filtered_count == 0) return 1;
    return app->sinks_mode.filtered_count;
}

static SinkEntry *sink_at_visible(AppData *app, int idx) {
    if (!app || idx < 0 || idx >= app->sinks_mode.filtered_count) return NULL;
    int raw = app->sinks_mode.filtered_indices[idx];
    if (raw < 0 || raw >= app->sinks_mode.sink_count) return NULL;
    return &app->sinks_mode.sinks[raw];
}

static void sinks_format_row(AppData *app, int raw_idx, CofiRowCells *out) {
    static char marker[4];
    marker[0] = '\0';
    const char *description = "Loading sinks...";

    if (app->sinks_mode.last_error[0] != '\0') {
        description = app->sinks_mode.last_error;
        out->row_flags = COFI_ROW_ERROR;
    } else if (app->sinks_mode.sink_count == 0) {
        out->row_flags = 0;
    } else if (app->sinks_mode.filtered_count == 0) {
        description = "No matching audio sinks found";
        out->row_flags = 0;
    } else {
        SinkEntry *sink = sink_at_visible(app, raw_idx);
        if (sink) {
            marker[0] = '[';
            marker[1] = sink->is_default ? '*' : ' ';
            marker[2] = ']';
            marker[3] = '\0';
            description = sink->description;
            out->row_flags = COFI_ROW_ACTIONABLE | COFI_ROW_SLOTTABLE;
        }
    }

    out->cell_count = 2;
    out->cells[0].text = marker[0] ? marker : "   ";
    out->cells[0].width_hint = 3;
    out->cells[1].text = description;
}

static const char *sinks_match_string(AppData *app, int raw_idx) {
    SinkEntry *sink = sink_at_visible(app, raw_idx);
    return sink ? sink->name : "";
}

static const char *sinks_row_identity(AppData *app, int raw_idx) {
    SinkEntry *sink = sink_at_visible(app, raw_idx);
    return sink ? sink->description : "";
}

static const char *sinks_slot_payload_for(AppData *app, int raw_idx) {
    SinkEntry *sink = sink_at_visible(app, raw_idx);
    return sink ? sink->name : NULL;
}

static void sinks_on_query_changed(AppData *app, const char *query) {
    sinks_filter(app, query);
}

static void sinks_on_enter(AppData *app) {
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "Audio sinks...");
    sinks_refresh_async(app);
}

static void sinks_on_leave(AppData *app) {
    (void)app;
}

static void sinks_on_tick(AppData *app, int generation) {
    (void)generation;
    sinks_refresh_async(app);
}

static CofiActionStatus sinks_on_enter_pressed(AppData *app, int filtered_idx,
                                                int raw_idx,
                                                const char *entry_text,
                                                int modifier_state) {
    (void)filtered_idx;
    (void)entry_text;
    (void)modifier_state;
    SinkEntry *sink = sink_at_visible(app, raw_idx);
    if (!sink) return COFI_NO_OP;
    return sinks_switch_name(app, sink->name) ? COFI_HANDLED_HIDE : COFI_ACTION_ERROR;
}

static CofiActionStatus sinks_slot_recall(AppData *app, const char *payload) {
    return sinks_switch_name(app, payload) ? COFI_HANDLED_HIDE : COFI_ACTION_ERROR;
}

static CofiActionStatus sinks_on_command_args(AppData *app, const char *args) {
    char slot = '\0';
    if (slot_parse_at_key_arg(args, &slot)) {
        const char *payload = slot_lookup(&app->harpoon.store, "sinks", slot);
        if (!payload) return COFI_ACTION_ERROR;
        return sinks_slot_recall(app, payload);
    }

    if (!args || args[0] == '\0') return COFI_NO_OP;
    for (int i = 0; i < app->sinks_mode.sink_count; i++) {
        SinkEntry *sink = &app->sinks_mode.sinks[i];
        if (g_strrstr(sink->name, args) || g_strrstr(sink->description, args))
            return sinks_slot_recall(app, sink->name);
    }
    return COFI_ACTION_ERROR;
}

static void sinks_show_command_error(AppData *app, const char *message) {
    if (!app || !app->textbuffer) return;
    gtk_text_buffer_set_text(app->textbuffer, message, -1);
    app->command_mode.showing_help = TRUE;
}

static gboolean sinks_command_handler(AppData *app,
                                      WindowInfo *window __attribute__((unused)),
                                      const char *args) {
    exit_command_mode(app);

    if (args && args[0] != '\0') {
        CofiActionStatus status = sinks_on_command_args(app, args);
        if (status == COFI_HANDLED_HIDE) {
            hide_window(app);
        } else if (status == COFI_ACTION_ERROR || status == COFI_NO_OP) {
            sinks_show_command_error(app, "No matching sink or sink slot.");
        }
        return FALSE;
    }

    if (app) {
        app->prefix_origin_tab = app->current_tab;
    }
    surface_tab(app, sinks_tab_mode());
    return FALSE;
}

static const CommandSpec s_sinks_command = {
    .primary = "sinks",
    .aliases = {"sink", NULL},
    .owner_provider_id = "sinks",
    .handler = sinks_command_handler,
    .description = "Switch to audio sinks tab",
    .help_format = "sinks, sink [@SLOT|SINK]",
    .keeps_open_on_hotkey_auto = 1
};

void sinks_provider_register(void) {
    cofi_init_provider_defaults(&s_sinks_provider);
    s_sinks_provider_id = -1;
    s_sinks_provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    s_sinks_provider.id = "sinks";
    s_sinks_provider.display_name = "SINKS";
    s_sinks_provider.shortcut_hint = "Shortcuts: Enter=Activate sink  Ctrl+key=Assign slot  Alt+key=Recall slot";
    s_sinks_provider.prefix_char = 0;
    s_sinks_provider.required = 0;
    s_sinks_provider.hidden_by_default = 1;
    s_sinks_provider.modal_policy = COFI_MODAL_HIDE_ON_ESC;
    s_sinks_provider.row_count = sinks_row_count;
    s_sinks_provider.format_row = sinks_format_row;
    s_sinks_provider.match_string = sinks_match_string;
    s_sinks_provider.row_identity = sinks_row_identity;
    s_sinks_provider.on_enter = sinks_on_enter;
    s_sinks_provider.on_leave = sinks_on_leave;
    s_sinks_provider.on_query_changed = sinks_on_query_changed;
    s_sinks_provider.on_tick = sinks_on_tick;
    s_sinks_provider.tick_interval_ms = 1500;
    s_sinks_provider.on_enter_pressed = sinks_on_enter_pressed;
    s_sinks_provider.on_command_args = sinks_on_command_args;
    s_sinks_provider.slot_store_enabled = 1;
    s_sinks_provider.slot_payload_for = sinks_slot_payload_for;
    s_sinks_provider.slot_recall = sinks_slot_recall;
    s_sinks_provider_id = cofi_register_tab_provider(&s_sinks_provider);
    if (s_sinks_provider_id >= 0) {
        cofi_register_command(&s_sinks_command);
    }
}
