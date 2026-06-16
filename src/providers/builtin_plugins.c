#include "providers/builtin_plugins.h"

#include "apps/apps_provider.h"
#include "bluetooth/bluetooth_provider.h"
#include "bookmarks/bookmarks_provider.h"
#include "sessions/sessions_provider.h"
#include "calc/calc_provider.h"
#include "config/config_provider.h"
#include "commands/core_commands.h"
#include "emoji/emoji_provider.h"
#include "files/files_provider.h"
#include "harpoon/harpoon_provider.h"
#include "geom/geom_provider.h"
#include "daemon/hotkeys_provider.h"
#include "matching/matching_provider.h"
#include "names/names_provider.h"
#include "path/path_provider.h"
#include "proc/proc_provider.h"
#include "profiles/profiles_provider.h"
#include "rules/rules_provider.h"
#include "run/run_provider.h"
#include "projects/projects_provider.h"
#include "sinks/sinks_provider.h"
#include "workspaces/workspaces_provider.h"

void cofi_register_builtin_plugins(void) {
    cofi_register_core_commands();
    sessions_provider_register();
    apps_provider_register();
    path_provider_register();
    bluetooth_provider_register();
    calc_provider_register();
    config_provider_register();
    files_provider_register();
    harpoon_provider_register();
    geom_provider_register();
    hotkeys_provider_register();
    matching_provider_register();
    names_provider_register();
    rules_provider_register();
    workspaces_provider_register();
    sinks_provider_register();
    run_provider_register();
    proc_provider_register();
    projects_provider_register();
    profiles_provider_register();
    bookmarks_provider_register();
    emoji_provider_register();
}
