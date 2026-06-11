#include <stdio.h>
#include <string.h>

#include "core/app/app_data.h"
#include "providers/builtin_plugins.h"
#include "providers/cofi_tab_provider.h"
#include "commands/command_availability.h"
#include "commands/command_registry.h"
#include "config/config.h"
#include "daemon/daemon_socket.h"
#include "ui/tab_header.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

#define ASSERT_EQ(name, a, b) ASSERT_TRUE(name, (a) == (b))
#define ASSERT_NOT_NULL(name, p) ASSERT_TRUE(name, (p) != NULL)
#define ASSERT_NULL(name, p) ASSERT_TRUE(name, (p) == NULL)

static void reset_and_register_builtins(void) {
    cofi_registry_reset();
    cofi_command_registry_reset();
    cofi_config_registry_reset();
    cofi_register_builtin_plugins();
}

static void test_builtin_registration_shape(void) {
    reset_and_register_builtins();

    ASSERT_EQ("all builtin providers registered", cofi_provider_count(), 19);
    ASSERT_EQ("core plus provider commands registered", cofi_command_count(), 47);

    int providers_have_valid_shape = 1;
    for (int i = 0; i < cofi_provider_count(); i++) {
        const CofiTabProvider *provider = cofi_get_provider(i);
        if (!provider || !provider->id || !provider->id[0] ||
            provider->tab_mode <= TAB_COUNT ||
            !cofi_get_provider_for_tab(provider->tab_mode)) {
            providers_have_valid_shape = 0;
        }
    }
    ASSERT_TRUE("builtin providers have ids, dynamic tabs, and tab lookup",
                providers_have_valid_shape);

    int commands_have_valid_owners = 1;
    for (int i = 0; i < cofi_command_count(); i++) {
        const CommandSpec *spec = cofi_command_at(i);
        if (!spec || !spec->owner_provider_id || !spec->owner_provider_id[0]) {
            commands_have_valid_owners = 0;
            continue;
        }
        if (strcmp(spec->owner_provider_id, COMMAND_OWNER_CORE) != 0) {
            if (cofi_get_provider_id(spec->owner_provider_id) < 0) {
                commands_have_valid_owners = 0;
            }
        }
    }
    ASSERT_TRUE("all commands have core or registered provider owners",
                commands_have_valid_owners);
}

static void test_builtin_disabled_provider_hides_command_and_tab(void) {
    reset_and_register_builtins();

    int projects_id = cofi_get_provider_id("projects");
    const CofiTabProvider *projects = cofi_get_provider(projects_id);
    ASSERT_NOT_NULL("projects provider registered", projects);

    const CommandSpec *tmux = cofi_command_for_token("tmux");
    ASSERT_NOT_NULL("tmux alias resolves before disable", tmux);
    ASSERT_TRUE("tmux alias resolves to projects",
                tmux && strcmp(tmux->primary, "projects") == 0);
    ASSERT_TRUE("projects command available before disable",
                command_primary_is_available("projects"));

    AppData app;
    memset(&app, 0, sizeof(app));
    app.config.show_all_tabs = 1;
    GString *header = g_string_new("");
    tab_header_format(&app, (TabMode)projects->tab_mode, 300, header);
    ASSERT_TRUE("show_all_tabs includes projects before disable",
                strstr(header->str, "PROJECTS") != NULL);
    g_string_free(header, TRUE);

    cofi_set_provider_enabled(projects_id, 0);

    ASSERT_NOT_NULL("raw tmux alias still exists in command registry",
                    cofi_command_for_token("tmux"));
    ASSERT_TRUE("disabled projects command unavailable",
                !command_primary_is_available("projects"));

    header = g_string_new("");
    tab_header_format(&app, TAB_WINDOWS, 300, header);
    ASSERT_TRUE("disabled projects hidden from show_all_tabs header",
                strstr(header->str, "PROJECTS") == NULL);
    g_string_free(header, TRUE);
}

static void test_builtin_delegate_and_hotkey_disablement(void) {
    reset_and_register_builtins();

    int harpoon_id = cofi_get_provider_id("harpoon");
    ASSERT_TRUE("harpoon provider registered", harpoon_id >= 0);
    ASSERT_NOT_NULL("harpoon delegate resolves before disable",
                    cofi_get_provider_for_delegate_opcode(COFI_OPCODE_HARPOON));
    ASSERT_NOT_NULL("harpoon hotkey resolves before disable",
                    cofi_get_provider_for_hotkey_mode(SHOW_MODE_HARPOON));

    cofi_set_provider_enabled(harpoon_id, 0);

    ASSERT_NULL("disabled harpoon delegate fails closed",
                cofi_get_provider_for_delegate_opcode(COFI_OPCODE_HARPOON));
    ASSERT_NULL("disabled harpoon hotkey fails closed",
                cofi_get_provider_for_hotkey_mode(SHOW_MODE_HARPOON));
}

static void test_names_provider_registration(void) {
    reset_and_register_builtins();

    int names_id = cofi_get_provider_id("names");
    ASSERT_TRUE("names provider registered", names_id >= 0);
    ASSERT_NOT_NULL("names delegate resolves",
                    cofi_get_provider_for_delegate_opcode(COFI_OPCODE_NAMES));

    const CommandSpec *names = cofi_command_for_token("nm");
    ASSERT_NOT_NULL("names alias resolves", names);
    ASSERT_TRUE("names alias resolves to names",
                names && strcmp(names->primary, "names") == 0);

    const CommandSpec *matching = cofi_command_for_token("m");
    ASSERT_NOT_NULL("matching alias still resolves", matching);
    ASSERT_TRUE("matching alias remains matching",
                matching && strcmp(matching->primary, "matching") == 0);
}

static void test_command_registration_rejects_collisions(void) {
    reset_and_register_builtins();

    static const CommandSpec primary_collision = {
        .primary = "projects",
        .owner_provider_id = COMMAND_OWNER_CORE,
    };
    static const CommandSpec alias_collision = {
        .primary = "unique-test-command",
        .aliases = {"tmux", NULL},
        .owner_provider_id = COMMAND_OWNER_CORE,
    };

    ASSERT_EQ("duplicate command primary rejected",
              cofi_register_command(&primary_collision), -1);
    ASSERT_EQ("duplicate command alias rejected",
              cofi_register_command(&alias_collision), -1);
}

int main(void) {
    printf("Plugin boundary tests\n");
    printf("=====================\n\n");

    test_builtin_registration_shape();
    test_builtin_disabled_provider_hides_command_and_tab();
    test_builtin_delegate_and_hotkey_disablement();
    test_names_provider_registration();
    test_command_registration_rejects_collisions();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
