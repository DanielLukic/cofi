#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <gtk/gtk.h>

// Safe string copy - always null-terminates the destination
void safe_string_copy(char *dest, const char *src, size_t dest_size);

// Parse a shortcut string like "Super+w" or "Ctrl+Shift+w" into key and modifiers
gboolean parse_shortcut(const char *shortcut_str, guint *key, GdkModifierType *mods);

#endif // UTILS_H