#include "commands/command_registry.h"

#include <string.h>

#define COFI_MAX_COMMANDS 64

static const CommandSpec *s_commands[COFI_MAX_COMMANDS];
static int s_command_count = 0;

static int command_has_name(const CommandSpec *spec, const char *name) {
    if (!spec || !name) return 0;
    if (spec->primary && strcmp(spec->primary, name) == 0) return 1;
    for (int i = 0; i < 5 && spec->aliases[i]; i++) {
        if (strcmp(spec->aliases[i], name) == 0) return 1;
    }
    return 0;
}

static int name_is_registered(const char *name) {
    for (int i = 0; i < s_command_count; i++) {
        if (command_has_name(s_commands[i], name)) {
            return 1;
        }
    }
    return 0;
}

int cofi_register_command(const CommandSpec *spec) {
    if (!spec || !spec->primary || !spec->owner_provider_id ||
        s_command_count >= COFI_MAX_COMMANDS) {
        return -1;
    }
    if (name_is_registered(spec->primary)) {
        return -1;
    }
    for (int i = 0; i < 5 && spec->aliases[i]; i++) {
        if (name_is_registered(spec->aliases[i])) {
            return -1;
        }
    }
    s_commands[s_command_count++] = spec;
    return s_command_count - 1;
}

void cofi_command_registry_reset(void) {
    for (int i = 0; i < COFI_MAX_COMMANDS; i++) {
        s_commands[i] = NULL;
    }
    s_command_count = 0;
}

int cofi_command_count(void) {
    return s_command_count;
}

const CommandSpec *cofi_command_at(int index) {
    if (index < 0 || index >= s_command_count) {
        return NULL;
    }
    return s_commands[index];
}

const CommandSpec *cofi_command_by_primary(const char *primary) {
    if (!primary) return NULL;
    for (int i = 0; i < s_command_count; i++) {
        const CommandSpec *spec = s_commands[i];
        if (spec->primary && strcmp(spec->primary, primary) == 0) {
            return spec;
        }
    }
    return NULL;
}

const CommandSpec *cofi_command_for_token(const char *token) {
    if (!token) return NULL;
    for (int i = 0; i < s_command_count; i++) {
        if (command_has_name(s_commands[i], token)) {
            return s_commands[i];
        }
    }
    return NULL;
}
