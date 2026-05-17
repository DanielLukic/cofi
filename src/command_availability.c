#include "command_availability.h"

#include "command_parse_defs.h"
#include "cofi_tab_provider.h"

#include <string.h>

static const char *command_owner(const char *primary) {
    if (!primary) return NULL;
    for (int i = 0; COMMAND_PARSE_DEFS[i].primary; i++) {
        if (strcmp(COMMAND_PARSE_DEFS[i].primary, primary) == 0) {
            return COMMAND_PARSE_DEFS[i].owner_provider_id;
        }
    }
    return NULL;
}

int command_primary_is_available(const char *primary) {
    const char *owner = command_owner(primary);
    if (!owner) return 0;
    if (strcmp(owner, COMMAND_OWNER_CORE) == 0) return 1;

    int provider_id = cofi_get_provider_id(owner);
    return provider_id >= 0 && cofi_provider_is_enabled(provider_id);
}
