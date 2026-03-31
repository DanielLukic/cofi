#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <gtk/gtk.h>

// Safe string copy - always null-terminates the destination
void safe_string_copy(char *dest, const char *src, size_t dest_size);

// Parse a shortcut string like "Super+w" or "Ctrl+Shift+w" into key and modifiers.
// Supports case-insensitive modifiers and common aliases:
//   Modifiers: ctrl/control, alt/mod1, super/mod4/win/windows, shift
//   Keys: enter/return, esc/escape, backspace, delete, space, tab, f1-f12
// If error_msg is non-NULL and parsing fails, writes a helpful diagnostic
// (e.g. "Did you mean 'Mod1'?") into error_msg (up to error_msg_size bytes).
gboolean parse_shortcut(const char *shortcut_str, guint *key, GdkModifierType *mods);
gboolean parse_shortcut_with_error(const char *shortcut_str, guint *key,
                                   GdkModifierType *mods,
                                   char *error_msg, size_t error_msg_size);

#endif // UTILS_H