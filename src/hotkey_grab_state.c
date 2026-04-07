#include "hotkeys.h"

#include <X11/keysym.h>
#include <string.h>

static int parse_hotkey(const char *spec, KeySym *sym_out, unsigned int *mod_out) {
    if (!spec || spec[0] == '\0') {
        return 0;
    }

    char buf[64];
    strncpy(buf, spec, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    unsigned int mod = 0;
    char *tok = strtok(buf, "+");
    char *next = tok ? strtok(NULL, "+") : NULL;

    while (tok && next) {
        if (strcmp(tok, "Mod1") == 0) {
            mod |= Mod1Mask;
        } else if (strcmp(tok, "Mod2") == 0) {
            mod |= Mod2Mask;
        } else if (strcmp(tok, "Mod3") == 0) {
            mod |= Mod3Mask;
        } else if (strcmp(tok, "Mod4") == 0) {
            mod |= Mod4Mask;
        } else if (strcmp(tok, "Control") == 0) {
            mod |= ControlMask;
        } else if (strcmp(tok, "Shift") == 0) {
            mod |= ShiftMask;
        } else {
            return 0;
        }

        tok = next;
        next = strtok(NULL, "+");
    }

    KeySym sym = XStringToKeysym(tok);
    if (sym == NoSymbol) {
        return 0;
    }

    *sym_out = sym;
    *mod_out = mod;
    return 1;
}

int populate_hotkey_grab_state(const HotkeyConfig *config, HotkeyGrabState *state) {
    if (!config || !state) {
        return 0;
    }

    init_hotkey_grab_state(state);

    for (int i = 0; i < config->count && state->grabbed_count < MAX_HOTKEY_BINDINGS; i++) {
        KeySym sym = NoSymbol;
        unsigned int mod = 0;
        if (!parse_hotkey(config->bindings[i].key, &sym, &mod)) {
            continue;
        }

        GrabbedHotkey *slot = &state->grabbed_hotkeys[state->grabbed_count];
        slot->sym = sym;
        slot->mod = mod;
        strncpy(slot->key_name, config->bindings[i].key, sizeof(slot->key_name) - 1);
        slot->key_name[sizeof(slot->key_name) - 1] = '\0';
        strncpy(slot->command, config->bindings[i].command, sizeof(slot->command) - 1);
        slot->command[sizeof(slot->command) - 1] = '\0';
        state->grabbed_count++;
    }

    return state->grabbed_count;
}
