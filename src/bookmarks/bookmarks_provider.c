#include "bookmarks/bookmarks_provider.h"

#include "bookmarks/bookmarks.h"
#include "commands/command_mode.h"
#include "commands/command_registry.h"
#include "core/app/app_data.h"
#include "core/log/log.h"
#include "core/selection/selection.h"
#include "core/slot_store/slot_store.h"
#include "daemon/detach_launch.h"
#include "profiles/browser_profiles.h"
#include "profiles/chrome_launch.h"
#include "providers/cofi_tab_provider.h"
#include "ui/tab_switching.h"

#include <gtk/gtk.h>
#include <string.h>

#define FOLDER_DISPLAY_MAX 10
#define BOOKMARK_PAYLOAD_PREFIX "bookmark:chrome:"

static BookmarksMode s_bookmarks_mode;
static CofiTabProvider s_bookmarks_provider;
static int s_bookmarks_provider_id = -1;

#ifdef COFI_TESTING
typedef gboolean (*BookmarkLaunchImpl)(const char *const *argv);
static BookmarkLaunchImpl s_launch_impl_for_test = NULL;
void bookmarks_provider_set_launch_impl_for_test(BookmarkLaunchImpl impl) {
    s_launch_impl_for_test = impl;
}
#endif

static gboolean dispatch_launch(const char *const *argv) {
#ifdef COFI_TESTING
    if (s_launch_impl_for_test) return s_launch_impl_for_test(argv);
#endif
    return detach_launch_argv_array(argv);
}

static TabMode bookmarks_tab_mode(void) {
    const CofiTabProvider *provider = cofi_get_provider(s_bookmarks_provider_id);
    return provider ? (TabMode)provider->tab_mode : TAB_WINDOWS;
}

static const BookmarkEntry *entry_at_row(int raw_idx) {
    if (!s_bookmarks_mode.filtered_indices || !s_bookmarks_mode.entries) return NULL;
    if (raw_idx < 0 || (guint)raw_idx >= s_bookmarks_mode.filtered_indices->len) return NULL;
    int entry_idx = g_array_index(s_bookmarks_mode.filtered_indices, int, raw_idx);
    if (entry_idx < 0 || (guint)entry_idx >= s_bookmarks_mode.entries->len) return NULL;
    return &g_array_index(s_bookmarks_mode.entries, BookmarkEntry, entry_idx);
}

static int bookmarks_row_count(AppData *app) {
    (void)app;
    if (!s_bookmarks_mode.filtered_indices) return 1;
    int n = (int)s_bookmarks_mode.filtered_indices->len;
    return n > 0 ? n : 1;
}

static void bookmarks_format_row(AppData *app, int raw_idx, CofiRowCells *out) {
    (void)app;
    static char folder_display[FOLDER_DISPLAY_MAX * 4 + 8];
    const BookmarkEntry *entry = entry_at_row(raw_idx);
    if (!entry) {
        out->cell_count = 1;
        if (s_bookmarks_mode.last_error[0]) {
            out->cells[0].text = s_bookmarks_mode.last_error;
            out->row_flags = COFI_ROW_ERROR;
        } else {
            out->cells[0].text = "No matching bookmarks found";
            out->row_flags = 0;
        }
        return;
    }

    bookmarks_truncate_folder_display(entry->folder_breadcrumb,
                                      folder_display, sizeof(folder_display),
                                      FOLDER_DISPLAY_MAX);

    out->cell_count = 5;
    out->cells[0].text = "[bm]";
    out->cells[0].width_hint = 4;
    out->cells[1].text = entry->profile_label;
    out->cells[1].width_hint = 12;
    out->cells[2].text = entry->name;
    out->cells[2].width_hint = 30;
    out->cells[3].text = folder_display;
    out->cells[3].width_hint = 10;
    out->cells[4].text = entry->url;
    out->cells[4].width_hint = 40;
    out->row_flags = COFI_ROW_ACTIONABLE | COFI_ROW_SLOTTABLE;
}

static const char *bookmarks_match_string(AppData *app, int raw_idx) {
    (void)app;
    static char buf[3072];
    const BookmarkEntry *entry = entry_at_row(raw_idx);
    bookmarks_format_match_text(entry, buf, sizeof(buf));
    return buf;
}

static const char *bookmarks_row_identity(AppData *app, int raw_idx) {
    (void)app;
    static char identity[BOOKMARK_PROFILE_DIR_LEN + BOOKMARK_URL_LEN + 32];
    const BookmarkEntry *entry = entry_at_row(raw_idx);
    if (!entry) return "";
    g_snprintf(identity, sizeof(identity), "bookmark:%s:%s",
               entry->profile_dir, entry->url);
    return identity;
}

static void bookmarks_on_enter(AppData *app) {
    if (!app) return;
    if (app->entry) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry),
                                       "Type to filter bookmarks...");
    }
    bookmarks_load(&s_bookmarks_mode);
}

static void bookmarks_on_query_changed(AppData *app, const char *query) {
    bookmarks_filter(&s_bookmarks_mode, query);
    reset_selection(app);
}

static gboolean launch_bookmark(const char *profile_dir, const char *url) {
    if (!profile_dir || !url || url[0] == '\0') return FALSE;

    /* Locate the chrome path: build a transient entry shaped like a Chrome
     * profile so the resolver can apply the google-chrome → -stable fallback. */
    BrowserProfileEntry shim;
    memset(&shim, 0, sizeof(shim));
    shim.backend = BROWSER_PROFILE_CHROME;
    g_strlcpy(shim.executable, "google-chrome", sizeof(shim.executable));
    g_strlcpy(shim.browser_name, "Chrome", sizeof(shim.browser_name));
    g_strlcpy(shim.profile_dir, profile_dir, sizeof(shim.profile_dir));

    gchar *chrome_path = chrome_launch_resolve_executable(&shim);
    if (!chrome_path) {
        log_error("bookmarks: no chrome executable on PATH for profile '%s'", profile_dir);
        return FALSE;
    }

    char **argv = chrome_launch_build_argv(chrome_path, profile_dir, url);
    gboolean ok = dispatch_launch((const char *const *)argv);
    if (ok) {
        log_info("bookmarks: opened %s in profile '%s' via %s",
                 url, profile_dir, chrome_path);
    }
    g_strfreev(argv);
    g_free(chrome_path);
    return ok;
}

static CofiActionStatus bookmarks_on_enter_pressed(AppData *app, int filtered_idx,
                                                   int raw_idx,
                                                   const char *entry_text,
                                                   int modifier_state) {
    (void)app; (void)filtered_idx; (void)entry_text; (void)modifier_state;
    const BookmarkEntry *entry = entry_at_row(raw_idx);
    if (!entry) return COFI_NO_OP;
    return launch_bookmark(entry->profile_dir, entry->url)
        ? COFI_HANDLED_HIDE : COFI_ACTION_ERROR;
}

static const char *bookmarks_slot_payload_for(AppData *app, int raw_idx) {
    (void)app;
    static char payload[SLOT_STORE_PAYLOAD_LEN];
    const BookmarkEntry *entry = entry_at_row(raw_idx);
    if (!entry) return NULL;
    int needed = g_snprintf(payload, sizeof(payload), "%s%s:%s",
                            BOOKMARK_PAYLOAD_PREFIX,
                            entry->profile_dir, entry->url);
    if (needed < 0 || (size_t)needed >= sizeof(payload)) {
        log_warn("bookmarks: payload exceeds %d bytes; refusing to assign slot",
                 SLOT_STORE_PAYLOAD_LEN);
        return NULL;
    }
    return payload;
}

/* Parse "bookmark:chrome:<profile_dir>:<url-with-arbitrary-colons>" by reading
 * the three known colon-delimited prefix tokens, then taking the entire
 * remainder as the URL. Mirrors projects' colon-tolerant parse so URLs that
 * embed ':' are preserved. */
static gboolean parse_payload(const char *payload, char *profile_dir,
                              size_t profile_dir_size, char *url, size_t url_size) {
    if (!payload || !profile_dir || !url) return FALSE;
    if (!g_str_has_prefix(payload, BOOKMARK_PAYLOAD_PREFIX)) return FALSE;
    const char *rest = payload + strlen(BOOKMARK_PAYLOAD_PREFIX);
    const char *colon = strchr(rest, ':');
    if (!colon || colon == rest) return FALSE;
    size_t dir_len = (size_t)(colon - rest);
    if (dir_len >= profile_dir_size) return FALSE;
    memcpy(profile_dir, rest, dir_len);
    profile_dir[dir_len] = '\0';
    const char *url_start = colon + 1;
    if (url_start[0] == '\0') return FALSE;
    g_strlcpy(url, url_start, url_size);
    return TRUE;
}

static CofiActionStatus bookmarks_slot_recall(AppData *app, const char *payload) {
    (void)app;
    char profile_dir[BOOKMARK_PROFILE_DIR_LEN];
    char url[BOOKMARK_URL_LEN];
    if (!parse_payload(payload, profile_dir, sizeof(profile_dir),
                       url, sizeof(url))) {
        log_warn("bookmarks: slot has invalid payload: %s", payload ? payload : "(null)");
        return COFI_ACTION_ERROR;
    }
    return launch_bookmark(profile_dir, url)
        ? COFI_HANDLED_HIDE : COFI_ACTION_ERROR;
}

static CofiActionStatus bookmarks_on_command_args(AppData *app, const char *args) {
    if (!app || !args || args[0] == '\0') return COFI_NO_OP;

    char slot = '\0';
    if (slot_parse_at_key_arg(args, &slot)) {
        const char *payload = slot_lookup(&app->harpoon.store, "bookmarks", slot);
        if (!payload) return COFI_ACTION_ERROR;
        return bookmarks_slot_recall(app, payload);
    }

    bookmarks_load(&s_bookmarks_mode);
    bookmarks_filter(&s_bookmarks_mode, args);
    if (!s_bookmarks_mode.filtered_indices ||
        s_bookmarks_mode.filtered_indices->len == 0) {
        return COFI_ACTION_ERROR;
    }
    const BookmarkEntry *entry = entry_at_row(0);
    if (!entry) return COFI_ACTION_ERROR;
    return launch_bookmark(entry->profile_dir, entry->url)
        ? COFI_HANDLED_HIDE : COFI_ACTION_ERROR;
}

static gboolean bookmarks_command_handler(AppData *app,
                                          WindowInfo *window __attribute__((unused)),
                                          const char *args) {
    exit_command_mode(app);

    if (args && args[0] != '\0') {
        CofiActionStatus status = bookmarks_on_command_args(app, args);
        if (status == COFI_HANDLED_HIDE) return TRUE;
        if (status == COFI_ACTION_ERROR || status == COFI_NO_OP) {
            if (app && app->textbuffer) {
                gtk_text_buffer_set_text(app->textbuffer,
                                         "No matching bookmark.", -1);
                app->command_mode.showing_help = TRUE;
            }
        }
        return FALSE;
    }

    if (app) app->prefix_origin_tab = app->current_tab;
    surface_tab(app, bookmarks_tab_mode());
    return FALSE;
}

static const CommandSpec s_bookmarks_command = {
    .primary = "bookmarks",
    .aliases = {"bm", NULL},
    .owner_provider_id = "bookmarks",
    .handler = bookmarks_command_handler,
    .description = "Switch to bookmarks tab",
    .help_format = "bookmarks, bm [@SLOT|QUERY]",
    .keeps_open_on_hotkey_auto = 1,
};

void bookmarks_provider_register(void) {
    cofi_init_provider_defaults(&s_bookmarks_provider);
    bookmarks_mode_init(&s_bookmarks_mode);
    s_bookmarks_provider_id = -1;
    s_bookmarks_provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    s_bookmarks_provider.id = "bookmarks";
    s_bookmarks_provider.display_name = "BOOKMARKS";
    s_bookmarks_provider.shortcut_hint =
        "Shortcuts: Enter=Open  Ctrl+key=Assign slot  Alt+key=Recall slot";
    s_bookmarks_provider.required = 0;
    s_bookmarks_provider.hidden_by_default = 1;
    s_bookmarks_provider.modal_policy = COFI_MODAL_HIDE_ON_ESC;
    s_bookmarks_provider.initial_selection_index = 0;
    s_bookmarks_provider.row_count = bookmarks_row_count;
    s_bookmarks_provider.format_row = bookmarks_format_row;
    s_bookmarks_provider.match_string = bookmarks_match_string;
    s_bookmarks_provider.row_identity = bookmarks_row_identity;
    s_bookmarks_provider.on_enter = bookmarks_on_enter;
    s_bookmarks_provider.on_query_changed = bookmarks_on_query_changed;
    s_bookmarks_provider.on_enter_pressed = bookmarks_on_enter_pressed;
    s_bookmarks_provider.on_command_args = bookmarks_on_command_args;
    s_bookmarks_provider.slot_store_enabled = 1;
    s_bookmarks_provider.slot_payload_for = bookmarks_slot_payload_for;
    s_bookmarks_provider.slot_recall = bookmarks_slot_recall;
    s_bookmarks_provider_id = cofi_register_tab_provider(&s_bookmarks_provider);
    if (s_bookmarks_provider_id >= 0) {
        cofi_register_command(&s_bookmarks_command);
    }
}
