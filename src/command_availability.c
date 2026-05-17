#include "command_availability.h"

#include "cofi_tab_provider.h"

#include <string.h>

static const char *provider_command_owner(const char *primary) {
    if (!primary) return NULL;
    if (strcmp(primary, "calc") == 0) return "calc";
    if (strcmp(primary, "config") == 0) return "config";
    if (strcmp(primary, "harpoon") == 0) return "harpoon";
    if (strcmp(primary, "hotkeys") == 0) return "hotkeys";
    if (strcmp(primary, "names") == 0) return "names";
    if (strcmp(primary, "proc") == 0) return "proc";
    if (strcmp(primary, "profiles") == 0) return "profiles";
    if (strcmp(primary, "rules") == 0) return "rules";
    if (strcmp(primary, "run") == 0) return "run";
    if (strcmp(primary, "sinks") == 0) return "sinks";
    if (strcmp(primary, "tmux") == 0) return "sessions";
    if (strcmp(primary, "workspaces") == 0) return "workspaces";
    return NULL;
}

int command_primary_is_available(const char *primary) {
    const char *provider_command = provider_command_owner(primary);
    if (!provider_command) return 1;
    return cofi_get_provider_for_command(provider_command) != NULL;
}
