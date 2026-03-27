# Hotkeys V2 Plan

## Goal
Make the 3 show-mode hotkeys configurable via cofi.json. Drop Alt+\ (assign_slots) entirely.

## Changes

### 1. Drop Alt+\ from hotkeys.c
Remove the `SHOW_MODE_ASSIGN_SLOTS` entry from `hotkey_defs[]`.

### 2. CofiConfig — add hotkey strings
```c
char hotkey_windows[64];     // default: "Mod1+Tab"
char hotkey_command[64];     // default: "Mod1+grave"
char hotkey_workspaces[64];  // default: "Mod1+BackSpace"
```
Defaults set in `init_config_defaults`. Empty string = disabled.

### 3. config.c — load/save hotkeys section
```json
"hotkeys": {
    "windows":    "Mod1+Tab",
    "command":    "Mod1+grave",
    "workspaces": "Mod1+BackSpace"
}
```
Missing key → keep default. `""` or `null` → disabled.

### 4. hotkeys.c — parse_hotkey + dynamic setup
- `parse_hotkey(spec, &sym, &mod)` splits on `+`, maps modifier names to masks, calls `XStringToKeysym` for key name
- `setup_hotkeys` builds active list from config (skipping disabled)
- Store active grabbed set in module-level array for `ungrab_all` and `handle_hotkey_event`

### 5. README/docs
Document key format: `Modifier+KeyName` using X11 names (Mod1, Mod2, Mod4, Control, Shift + XStringToKeysym names).

## Key format
`Mod1+Tab`, `Mod1+grave`, `Mod1+BackSpace`, `""` = disabled.
Modifiers: Mod1, Mod2, Mod3, Mod4, Control, Shift (Lock reserved for CapsLock variants internally).
