#ifndef WORKSPACE_INFO_H
#define WORKSPACE_INFO_H

#include "types.h"

typedef struct WorkspaceInfo {
    int id;
    char name[MAX_WORKSPACE_NAME_LEN];
    int is_current;
} WorkspaceInfo;

#endif // WORKSPACE_INFO_H