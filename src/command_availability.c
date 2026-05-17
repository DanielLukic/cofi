#include "command_availability.h"

#include "command_parse_defs.h"
#include "cofi_tab_provider.h"

#include <string.h>

static const char *provider_command_owner(const char *primary) {
    if (!primary) return NULL;
    for (int i = 0; COMMAND_PARSE_DEFS[i].primary; i++) {
        if (strcmp(COMMAND_PARSE_DEFS[i].primary, primary) == 0) {
            return COMMAND_PARSE_DEFS[i].provider_command;
        }
    }
    return NULL;
}

int command_primary_is_available(const char *primary) {
    const char *provider_command = provider_command_owner(primary);
    if (!provider_command) return 1;
    return cofi_get_provider_for_command(provider_command) != NULL;
}
