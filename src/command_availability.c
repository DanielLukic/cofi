#include "command_availability.h"

#include "command_registry.h"
#include "cofi_tab_provider.h"

#include <string.h>

static const char *command_owner(const char *primary) {
    if (!primary) return NULL;
    const CommandSpec *spec = cofi_command_by_primary(primary);
    return spec ? spec->owner_provider_id : NULL;
}

int command_primary_is_available(const char *primary) {
    const char *owner = command_owner(primary);
    if (!owner) return 0;

    if (strcmp(owner, COMMAND_OWNER_CORE) == 0) return 1;

    int provider_id = cofi_get_provider_id(owner);
    return provider_id >= 0 && cofi_provider_is_enabled(provider_id);
}
