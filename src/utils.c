#include "utils.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
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

// --- Modifier alias table ---

typedef struct {
    const char *name;       // lowercase alias
    GdkModifierType mask;
    const char *canonical;  // display name for suggestions
} ModifierAlias;

static const ModifierAlias modifier_aliases[] = {
    { "ctrl",      GDK_CONTROL_MASK, "Ctrl"    },
    { "control",   GDK_CONTROL_MASK, "Control" },
    { "shift",     GDK_SHIFT_MASK,   "Shift"   },
    { "alt",       GDK_MOD1_MASK,    "Alt"     },
    { "mod1",      GDK_MOD1_MASK,    "Mod1"    },
    { "super",     GDK_SUPER_MASK,   "Super"   },
    { "mod4",      GDK_SUPER_MASK,   "Mod4"    },
    { "win",       GDK_SUPER_MASK,   "Win"     },
    { "windows",   GDK_SUPER_MASK,   "Windows" },
    { "meta",      GDK_META_MASK,    "Meta"    },
    { "hyper",     GDK_HYPER_MASK,   "Hyper"   },
};
#define NUM_MOD_ALIASES ((int)(sizeof(modifier_aliases) / sizeof(modifier_aliases[0])))

// --- Key alias table ---

typedef struct {
    const char *name;       // lowercase alias
    guint keyval;
    const char *canonical;  // display name for suggestions
} KeyAlias;

static const KeyAlias key_aliases[] = {
    { "tab",       GDK_KEY_Tab,       "Tab"       },
    { "space",     GDK_KEY_space,     "Space"     },
    { "return",    GDK_KEY_Return,    "Return"    },
    { "enter",     GDK_KEY_Return,    "Enter"     },
    { "escape",    GDK_KEY_Escape,    "Escape"    },
    { "esc",       GDK_KEY_Escape,    "Esc"       },
    { "backspace", GDK_KEY_BackSpace, "BackSpace" },
    { "delete",    GDK_KEY_Delete,    "Delete"    },
    { "del",       GDK_KEY_Delete,    "Del"       },
    { "insert",    GDK_KEY_Insert,    "Insert"    },
    { "ins",       GDK_KEY_Insert,    "Ins"       },
    { "home",      GDK_KEY_Home,      "Home"      },
    { "end",       GDK_KEY_End,       "End"       },
    { "pageup",    GDK_KEY_Page_Up,   "PageUp"    },
    { "page_up",   GDK_KEY_Page_Up,   "Page_Up"   },
    { "pagedown",  GDK_KEY_Page_Down, "PageDown"  },
    { "page_down", GDK_KEY_Page_Down, "Page_Down" },
    { "up",        GDK_KEY_Up,        "Up"        },
    { "down",      GDK_KEY_Down,      "Down"      },
    { "left",      GDK_KEY_Left,      "Left"      },
    { "right",     GDK_KEY_Right,     "Right"     },
    { "grave",     GDK_KEY_grave,     "grave"     },
    { "backslash", GDK_KEY_backslash, "backslash" },
    { "slash",     GDK_KEY_slash,     "slash"     },
    { "minus",     GDK_KEY_minus,     "minus"     },
    { "equal",     GDK_KEY_equal,     "equal"     },
    { "period",    GDK_KEY_period,    "period"    },
    { "comma",     GDK_KEY_comma,     "comma"     },
    { "semicolon", GDK_KEY_semicolon, "semicolon" },
};
#define NUM_KEY_ALIASES ((int)(sizeof(key_aliases) / sizeof(key_aliases[0])))

// Simple edit-distance (Levenshtein) for short strings, capped at max_dist
static int edit_distance(const char *a, const char *b) {
    int la = (int)strlen(a);
    int lb = (int)strlen(b);
    if (la > 20 || lb > 20) return 99;

    int dp[21][21];
    for (int i = 0; i <= la; i++) dp[i][0] = i;
    for (int j = 0; j <= lb; j++) dp[0][j] = j;
    for (int i = 1; i <= la; i++) {
        for (int j = 1; j <= lb; j++) {
            int cost = (a[i-1] == b[j-1]) ? 0 : 1;
            dp[i][j] = dp[i-1][j] + 1;               // delete
            if (dp[i][j-1] + 1 < dp[i][j])
                dp[i][j] = dp[i][j-1] + 1;            // insert
            if (dp[i-1][j-1] + cost < dp[i][j])
                dp[i][j] = dp[i-1][j-1] + cost;       // replace
        }
    }
    return dp[la][lb];
}

// Find the closest modifier name to `token` (lowercase). Returns canonical name or NULL.
// On ties, prefer the alias whose length is closest to the token length.
static const char* suggest_modifier(const char *token) {
    int best_dist = 3; // max distance to consider a suggestion
    int best_len_diff = 99;
    const char *best = NULL;
    int token_len = (int)strlen(token);
    for (int i = 0; i < NUM_MOD_ALIASES; i++) {
        int d = edit_distance(token, modifier_aliases[i].name);
        int ld = abs((int)strlen(modifier_aliases[i].name) - token_len);
        if (d < best_dist || (d == best_dist && ld < best_len_diff)) {
            best_dist = d;
            best_len_diff = ld;
            best = modifier_aliases[i].canonical;
        }
    }
    return best;
}

// Find the closest key name to `token` (lowercase). Returns canonical name or NULL.
static const char* suggest_key(const char *token) {
    int best_dist = 3;
    const char *best = NULL;
    for (int i = 0; i < NUM_KEY_ALIASES; i++) {
        int d = edit_distance(token, key_aliases[i].name);
        if (d < best_dist) {
            best_dist = d;
            best = key_aliases[i].canonical;
        }
    }
    return best;
}

// Try to match a modifier token (lowercase). Returns TRUE and sets mask on match.
static gboolean match_modifier(const char *token, GdkModifierType *mask) {
    for (int i = 0; i < NUM_MOD_ALIASES; i++) {
        if (strcmp(token, modifier_aliases[i].name) == 0) {
            *mask = modifier_aliases[i].mask;
            return TRUE;
        }
    }
    return FALSE;
}

// Try to match a key token (lowercase). Returns TRUE and sets keyval on match.
static gboolean match_key(const char *token, guint *keyval) {
    // Single character keys
    if (strlen(token) == 1) {
        *keyval = gdk_unicode_to_keyval(token[0]);
        return TRUE;
    }

    // Named keys from alias table
    for (int i = 0; i < NUM_KEY_ALIASES; i++) {
        if (strcmp(token, key_aliases[i].name) == 0) {
            *keyval = key_aliases[i].keyval;
            return TRUE;
        }
    }

    // Function keys F1-F12
    if (token[0] == 'f' && isdigit(token[1])) {
        int fnum = atoi(token + 1);
        if (fnum >= 1 && fnum <= 12) {
            *keyval = GDK_KEY_F1 + (fnum - 1);
            return TRUE;
        }
    }

    return FALSE;
}

static void set_error(char *error_msg, size_t error_msg_size, const char *fmt, ...) {
    if (!error_msg || error_msg_size == 0) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(error_msg, error_msg_size, fmt, ap);
    va_end(ap);
}

// Core parser with optional error output
gboolean parse_shortcut_with_error(const char *shortcut_str, guint *key,
                                   GdkModifierType *mods,
                                   char *error_msg, size_t error_msg_size) {
    if (error_msg && error_msg_size > 0) error_msg[0] = '\0';

    if (!shortcut_str || !key || !mods) {
        set_error(error_msg, error_msg_size, "Invalid arguments to parse_shortcut");
        return FALSE;
    }

    if (shortcut_str[0] == '\0') {
        set_error(error_msg, error_msg_size, "Empty shortcut string");
        return FALSE;
    }

    *key = 0;
    *mods = 0;

    // Copy and lowercase
    char buffer[256];
    safe_string_copy(buffer, shortcut_str, sizeof(buffer));
    for (char *p = buffer; *p; p++) {
        *p = tolower(*p);
    }

    // Tokenize by '+'
    // We need to collect all tokens first (strtok destroys the buffer)
    char *tokens[16];
    int token_count = 0;
    char *tok = strtok(buffer, "+");
    while (tok && token_count < 16) {
        // Trim whitespace
        while (*tok && isspace(*tok)) tok++;
        char *end = tok + strlen(tok) - 1;
        while (end > tok && isspace(*end)) *end-- = '\0';

        if (*tok) {
            tokens[token_count++] = tok;
        }
        tok = strtok(NULL, "+");
    }

    if (token_count == 0) {
        set_error(error_msg, error_msg_size, "No key or modifier found in '%s'", shortcut_str);
        return FALSE;
    }

    // Last token is the key; everything before is a modifier
    char *key_token = tokens[token_count - 1];

    // Parse modifiers (all tokens except the last)
    for (int i = 0; i < token_count - 1; i++) {
        GdkModifierType mask;
        if (match_modifier(tokens[i], &mask)) {
            *mods |= mask;
        } else {
            // Not a known modifier -- try to suggest
            const char *suggestion = suggest_modifier(tokens[i]);
            if (suggestion) {
                set_error(error_msg, error_msg_size,
                          "Unknown modifier '%s'. Did you mean '%s'?", tokens[i], suggestion);
            } else {
                set_error(error_msg, error_msg_size,
                          "Unknown modifier '%s'. Valid modifiers: Ctrl, Alt, Super, Shift, Meta, Hyper",
                          tokens[i]);
            }
            return FALSE;
        }
    }

    // Parse the key
    if (match_key(key_token, key)) {
        return TRUE;
    }

    // Key not recognized -- try to suggest
    const char *key_suggestion = suggest_key(key_token);
    // Also check if it's actually a modifier used as a key
    GdkModifierType dummy;
    if (match_modifier(key_token, &dummy)) {
        set_error(error_msg, error_msg_size,
                  "'%s' is a modifier, not a key. Use it before '+', e.g. '%s+Tab'",
                  key_token, key_token);
    } else if (key_suggestion) {
        set_error(error_msg, error_msg_size,
                  "Unknown key '%s'. Did you mean '%s'?", key_token, key_suggestion);
    } else {
        set_error(error_msg, error_msg_size,
                  "Unknown key '%s'. Examples: Tab, Space, Return, Escape, BackSpace, a-z, F1-F12",
                  key_token);
    }
    return FALSE;
}

// Original API preserved for backward compatibility
gboolean parse_shortcut(const char *shortcut_str, guint *key, GdkModifierType *mods) {
    return parse_shortcut_with_error(shortcut_str, key, mods, NULL, 0);
}
