#include "command_handlers_layout.h"

#include <string.h>

#include "layout_manager.h"
#include "log.h"

static const char *trim_left(const char *text) {
    while (text && *text == ' ') {
        text++;
    }

    return text;
}

gboolean cmd_layout(AppData *app, WindowInfo *window __attribute__((unused)), const char *args) {
    const char *subcommand = trim_left(args);
    if (!subcommand || subcommand[0] == '\0') {
        log_warn("layout requires a subcommand: main+stack, refresh, off");
        return FALSE;
    }

    if (strcmp(subcommand, "main+stack") == 0) {
        return apply_layout(app, LAYOUT_PATTERN_MAIN_STACK);
    }

    if (strcmp(subcommand, "refresh") == 0) {
        return refresh_layout(app);
    }

    if (strcmp(subcommand, "off") == 0) {
        layout_off(app);
        return TRUE;
    }

    log_warn("Unknown layout subcommand: %s", subcommand);
    return FALSE;
}
