#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <getopt.h>
#include "cli_args.h"
#include "log.h"
#include "version.h"

void print_usage(const char *prog_name) {
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  --log-level LEVEL    Set log level (trace, debug, info, warn, error, fatal)\n");
    printf("  --log-file FILE      Write logs to FILE\n");
    printf("  --no-log             Disable logging\n");
    printf("  --align ALIGNMENT    Set window alignment (center, top, top_left, top_right,\n");
    printf("                       left, right, bottom, bottom_left, bottom_right)\n");
    printf("  --close-on-focus-loss Close window when focus is lost\n");
    printf("  --version            Show version information\n");
    printf("  --help               Show this help message\n");
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

int parse_command_line(int argc, char *argv[], AppData *app, char **log_file, int *log_enabled, int *alignment_specified) {
    static struct option long_options[] = {
        {"log-level", required_argument, 0, 'l'},
        {"log-file", required_argument, 0, 'f'},
        {"no-log", no_argument, 0, 'n'},
        {"align", required_argument, 0, 'a'},
        {"close-on-focus-loss", no_argument, 0, 'c'},
        {"version", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int c;
    int option_index = 0;
    
    while ((c = getopt_long(argc, argv, "l:f:nacvh", long_options, &option_index)) != -1) {
        switch (c) {
            case 'l': {
                int level = parse_log_level(optarg);
                if (level < 0) {
                    fprintf(stderr, "Invalid log level: %s\n", optarg);
                    print_usage(argv[0]);
                    return 1;
                }
                log_set_level(level);
                break;
            }
            case 'f':
                *log_file = optarg;
                break;
            case 'n':
                *log_enabled = 0;
                break;
            case 'a': {
                app->alignment = parse_alignment(optarg);
                app->has_saved_position = 0;  // Clear saved position when alignment is specified
                *alignment_specified = 1;
                break;
            }
            case 'c':
                app->close_on_focus_loss = 1;
                break;
            case 'v':
                printf("cofi version %s\n", VERSION_STRING);
                return 2; // Special return code for version
            case 'h':
                print_usage(argv[0]);
                return 3; // Special return code for help
            case '?':
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    return 0; // Success
}