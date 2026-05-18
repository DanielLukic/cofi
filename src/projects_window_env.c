#include "projects_window_env.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static const char *next_env_entry(const char *data, size_t len, size_t *offset) {
    while (*offset < len && data[*offset] == '\0') (*offset)++;
    if (*offset >= len) return NULL;

    const char *entry = data + *offset;
    const char *end = memchr(entry, '\0', len - *offset);
    if (!end) {
        *offset = len;
        return NULL;
    }

    *offset = (size_t)(end - data) + 1;
    return entry;
}

gboolean projects_windowid_from_environ(const char *environ_data,
                                        size_t len,
                                        Window *window_out) {
    if (window_out) *window_out = 0;
    if (!environ_data || len == 0) return FALSE;

    size_t offset = 0;
    const char *entry;
    while ((entry = next_env_entry(environ_data, len, &offset)) != NULL) {
        if (strncmp(entry, "WINDOWID=", 9) != 0) continue;

        errno = 0;
        char *end = NULL;
        unsigned long id = strtoul(entry + 9, &end, 10);
        if (errno != 0 || end == entry + 9 || *end != '\0' || id == 0) return FALSE;

        if (window_out) *window_out = (Window)id;
        return TRUE;
    }

    return FALSE;
}
