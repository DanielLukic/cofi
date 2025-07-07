#ifndef TYPES_H
#define TYPES_H

#include <X11/Xlib.h>

// Common size constants - centralized from various headers
#define MAX_WINDOWS 256
#define MAX_TITLE_LEN 512
#define MAX_CLASS_LEN 128
#define MAX_WORKSPACES 32
#define MAX_WORKSPACE_NAME_LEN 256
#define MAX_HARPOON_SLOTS 36

// Window type constants
#define WINDOW_TYPE_NORMAL "Normal"
#define WINDOW_TYPE_SPECIAL "Special"

// Show mode for instance communication
typedef enum {
    SHOW_MODE_WINDOWS,      // Show windows tab
    SHOW_MODE_WORKSPACES,   // Show workspaces tab
    SHOW_MODE_COMMAND       // Show in command mode
} ShowMode;

// Forward declarations for main structures
typedef struct WindowInfo WindowInfo;
typedef struct WorkspaceInfo WorkspaceInfo;
typedef struct HarpoonSlot HarpoonSlot;
typedef struct HarpoonManager HarpoonManager;
typedef struct DBusService DBusService;

#endif // TYPES_H