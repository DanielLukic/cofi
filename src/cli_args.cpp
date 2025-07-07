#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// Include C++ headers first
#include "popl.hpp"

// Then include C headers with extern "C"
extern "C" {
#include "cli_args.h"
#include "log.h"
#include "version.h"
#include "utils.h"
#include "command_mode.h"

void print_usage(const char *prog_name) {
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  --log-level LEVEL    Set log level (trace, debug, info, warn, error, fatal)\n");
    printf("  --log-file FILE      Write logs to FILE\n");
    printf("  --no-log             Disable logging\n");
    printf("  --align ALIGNMENT    Set window alignment (center, top, top_left, top_right,\n");
    printf("                       left, right, bottom, bottom_left, bottom_right)\n");
    printf("  --no-auto-close      Don't close window when focus is lost\n");
    printf("  --workspaces         Start with the Workspaces tab active\n");
    printf("  --harpoon            Start with the Harpoon tab active\n");
    printf("  --command            Start in command mode (with ':' prompt)\n");
    printf("  --version            Show version information\n");
    printf("  --help               Show this help message\n");
    printf("  --help-commands, -H  Show command mode help\n");
}

void print_command_mode_help(void) {
    char *help_text = generate_command_help_text(HELP_FORMAT_CLI);
    if (help_text) {
        printf("%s\n", help_text);
        free(help_text);
    }
}

int parse_log_level(const char *level_str) {
    if (strcasecmp(level_str, "trace") == 0) return LOG_TRACE;
    if (strcasecmp(level_str, "debug") == 0) return LOG_DEBUG;
    if (strcasecmp(level_str, "info") == 0) return LOG_INFO;
    if (strcasecmp(level_str, "warn") == 0) return LOG_WARN;
    if (strcasecmp(level_str, "error") == 0) return LOG_ERROR;
    if (strcasecmp(level_str, "fatal") == 0) return LOG_FATAL;
    return -1;
}

WindowAlignment parse_alignment(const char *align_str) {
    if (strcasecmp(align_str, "center") == 0) return ALIGN_CENTER;
    if (strcasecmp(align_str, "top") == 0) return ALIGN_TOP;
    if (strcasecmp(align_str, "top_left") == 0) return ALIGN_TOP_LEFT;
    if (strcasecmp(align_str, "top_right") == 0) return ALIGN_TOP_RIGHT;
    if (strcasecmp(align_str, "left") == 0) return ALIGN_LEFT;
    if (strcasecmp(align_str, "right") == 0) return ALIGN_RIGHT;
    if (strcasecmp(align_str, "bottom") == 0) return ALIGN_BOTTOM;
    if (strcasecmp(align_str, "bottom_left") == 0) return ALIGN_BOTTOM_LEFT;
    if (strcasecmp(align_str, "bottom_right") == 0) return ALIGN_BOTTOM_RIGHT;
    return ALIGN_CENTER; // Default fallback
}

int parse_command_line(int argc, char *argv[], AppData *app, char **log_file, int *log_enabled, int *alignment_specified, int *close_on_focus_loss_specified, int *workspace_shortcut_specified) {
    (void)workspace_shortcut_specified; // Unused parameter
    
    using namespace popl;
    
    OptionParser op("Allowed options");
    
    auto log_level_opt = op.add<Value<std::string>>("l", "log-level", "Set log level (trace, debug, info, warn, error, fatal)");
    auto log_file_opt = op.add<Value<std::string>>("f", "log-file", "Write logs to FILE");
    auto no_log_opt = op.add<Switch>("n", "no-log", "Disable logging");
    auto align_opt = op.add<Value<std::string>>("a", "align", "Set window alignment");
    auto no_auto_close_opt = op.add<Switch>("C", "no-auto-close", "Don't close window when focus is lost");
    auto workspaces_opt = op.add<Switch>("w", "workspaces", "Start with the Workspaces tab active");
    auto harpoon_opt = op.add<Switch>("", "harpoon", "Start with the Harpoon tab active");
    auto command_opt = op.add<Switch>("c", "command", "Start in command mode (with ':' prompt)");
    auto version_opt = op.add<Switch>("v", "version", "Show version information");
    auto help_opt = op.add<Switch>("h", "help", "Show this help message");
    auto help_commands_opt = op.add<Switch>("H", "help-commands", "Show command mode help");
    
    try {
        op.parse(argc, argv);
    } catch (const std::exception& e) {
        fprintf(stderr, "Error: %s\n", e.what());
        print_usage(argv[0]);
        return 1;
    }
    
    // Check for unknown options
    auto unknown = op.unknown_options();
    if (!unknown.empty()) {
        fprintf(stderr, "Error: Unknown option(s): ");
        for (const auto& u : unknown) {
            fprintf(stderr, "%s ", u.c_str());
        }
        fprintf(stderr, "\n");
        print_usage(argv[0]);
        return 1;
    }
    
    // Handle help first
    if (help_opt->is_set()) {
        print_usage(argv[0]);
        return 3;
    }
    
    if (help_commands_opt->is_set()) {
        print_command_mode_help();
        return 4;
    }
    
    if (version_opt->is_set()) {
        printf("cofi version %s\n", VERSION_STRING);
        return 2;
    }
    
    // Process other options
    if (log_level_opt->is_set()) {
        int level = parse_log_level(log_level_opt->value().c_str());
        if (level < 0) {
            fprintf(stderr, "Invalid log level: %s\n", log_level_opt->value().c_str());
            print_usage(argv[0]);
            return 1;
        }
        log_set_level(level);
    }
    
    if (log_file_opt->is_set()) {
        *log_file = strdup(log_file_opt->value().c_str());
    }
    
    if (no_log_opt->is_set()) {
        *log_enabled = 0;
    }
    
    if (align_opt->is_set()) {
        app->config.alignment = parse_alignment(align_opt->value().c_str());
        *alignment_specified = 1;
    }
    
    if (no_auto_close_opt->is_set()) {
        app->config.close_on_focus_loss = 0;
        if (close_on_focus_loss_specified) *close_on_focus_loss_specified = 1;
    }
    
    if (workspaces_opt->is_set()) {
        app->current_tab = TAB_WORKSPACES;
    }
    
    if (harpoon_opt->is_set()) {
        app->current_tab = TAB_HARPOON;
    }
    
    if (command_opt->is_set()) {
        app->start_in_command_mode = 1;
    }
    
    return 0; // Success
}

} // extern "C"