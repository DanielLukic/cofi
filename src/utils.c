#include "utils.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <gdk/gdkkeysyms.h>

// Safe string copy - always null-terminates the destination
void safe_string_copy(char *dest, const char *src, size_t dest_size) {
    if (!dest || dest_size == 0) return;
    
    if (!src) {
        dest[0] = '\0';
        return;
    }
    
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

// Parse shortcut string like "Super+w", "Ctrl+w", "Ctrl+Shift+w", "Alt+Tab"
gboolean parse_shortcut(const char *shortcut_str, guint *key, GdkModifierType *mods) {
    if (!shortcut_str || !key || !mods) return FALSE;
    
    *key = 0;
    *mods = 0;
    
    // Create a copy of the string to work with
    char buffer[256];
    safe_string_copy(buffer, shortcut_str, sizeof(buffer));
    
    // Convert to lowercase for easier parsing
    for (char *p = buffer; *p; p++) {
        *p = tolower(*p);
    }
    
    // Parse modifiers and key
    char *token = strtok(buffer, "+");
    char *last_token = NULL;
    
    while (token) {
        // Trim whitespace
        while (*token && isspace(*token)) token++;
        char *end = token + strlen(token) - 1;
        while (end > token && isspace(*end)) *end-- = '\0';
        
        // Check if this is a modifier
        if (strcmp(token, "ctrl") == 0 || strcmp(token, "control") == 0) {
            *mods |= GDK_CONTROL_MASK;
        } else if (strcmp(token, "shift") == 0) {
            *mods |= GDK_SHIFT_MASK;
        } else if (strcmp(token, "alt") == 0 || strcmp(token, "mod1") == 0) {
            *mods |= GDK_MOD1_MASK;
        } else if (strcmp(token, "super") == 0 || strcmp(token, "mod4") == 0 || 
                   strcmp(token, "win") == 0 || strcmp(token, "windows") == 0) {
            *mods |= GDK_SUPER_MASK;
        } else {
            // This should be the key
            last_token = token;
        }
        
        token = strtok(NULL, "+");
    }
    
    // Parse the key
    if (!last_token) return FALSE;
    
    // Handle single character keys
    if (strlen(last_token) == 1) {
        *key = gdk_unicode_to_keyval(last_token[0]);
        return TRUE;
    }
    
    // Handle special keys
    if (strcmp(last_token, "tab") == 0) {
        *key = GDK_KEY_Tab;
    } else if (strcmp(last_token, "space") == 0) {
        *key = GDK_KEY_space;
    } else if (strcmp(last_token, "return") == 0 || strcmp(last_token, "enter") == 0) {
        *key = GDK_KEY_Return;
    } else if (strcmp(last_token, "escape") == 0 || strcmp(last_token, "esc") == 0) {
        *key = GDK_KEY_Escape;
    } else if (strcmp(last_token, "up") == 0) {
        *key = GDK_KEY_Up;
    } else if (strcmp(last_token, "down") == 0) {
        *key = GDK_KEY_Down;
    } else if (strcmp(last_token, "left") == 0) {
        *key = GDK_KEY_Left;
    } else if (strcmp(last_token, "right") == 0) {
        *key = GDK_KEY_Right;
    } else if (strncmp(last_token, "f", 1) == 0 && isdigit(last_token[1])) {
        // Function keys F1-F12
        int fnum = atoi(last_token + 1);
        if (fnum >= 1 && fnum <= 12) {
            *key = GDK_KEY_F1 + (fnum - 1);
        }
    } else {
        // Unknown key
        return FALSE;
    }
    
    return TRUE;
}