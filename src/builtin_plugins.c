#include "builtin_plugins.h"

#include "apps_provider.h"
#include "sessions_provider.h"
#include "calc_provider.h"
#include "config_provider.h"
#include "core_commands.h"
#include "harpoon_provider.h"
#include "hotkeys_provider.h"
#include "names_provider.h"
#include "proc_provider.h"
#include "profiles_provider.h"
#include "rules_provider.h"
#include "run_provider.h"
#include "projects_provider.h"
#include "sinks_provider.h"
#include "workspaces_provider.h"

void cofi_register_builtin_plugins(void) {
    cofi_register_core_commands();
    sessions_provider_register();
    apps_provider_register();
    calc_provider_register();
    config_provider_register();
    harpoon_provider_register();
    hotkeys_provider_register();
    names_provider_register();
    rules_provider_register();
    workspaces_provider_register();
    sinks_provider_register();
    run_provider_register();
    proc_provider_register();
    projects_provider_register();
    profiles_provider_register();
}
