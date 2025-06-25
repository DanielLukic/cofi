#ifndef WORKSPACE_INFO_H
#define WORKSPACE_INFO_H

#define MAX_WORKSPACE_NAME_LEN 256
#define MAX_WORKSPACES 32

typedef struct {
    int id;
    char name[MAX_WORKSPACE_NAME_LEN];
    int is_current;
} WorkspaceInfo;

#endif // WORKSPACE_INFO_H