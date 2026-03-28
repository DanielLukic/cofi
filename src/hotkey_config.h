#ifndef HOTKEY_CONFIG_H
#define HOTKEY_CONFIG_H

#include <stddef.h>

#define MAX_HOTKEY_BINDINGS 64

typedef struct {
    char key[64];       // e.g. "Mod1+Tab"
    char command[256];  // e.g. "show windows"
} HotkeyBinding;

typedef struct {
    HotkeyBinding bindings[MAX_HOTKEY_BINDINGS];
    int count;
} HotkeyConfig;

void init_hotkey_config(HotkeyConfig *config);
int save_hotkey_config(const HotkeyConfig *config);
int load_hotkey_config(HotkeyConfig *config);
int add_hotkey_binding(HotkeyConfig *config, const char *key, const char *command);
int remove_hotkey_binding(HotkeyConfig *config, const char *key);
int find_hotkey_binding(const HotkeyConfig *config, const char *key);
int format_hotkey_display(const HotkeyConfig *config, char *buf, size_t buf_size);

// Parse :hotkeys command args. Returns:
//   0 = show bindings
//   1 = bind (key_out + cmd_out populated)
//   2 = unbind (key_out populated)
int parse_hotkey_command(const char *args, char *key_out, size_t key_size,
                         char *cmd_out, size_t cmd_size);

#endif // HOTKEY_CONFIG_H
