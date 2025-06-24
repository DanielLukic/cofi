#ifndef WINDOW_INFO_H
#define WINDOW_INFO_H

#include <X11/Xlib.h>

#define MAX_WINDOWS 256
#define MAX_TITLE_LEN 512
#define MAX_CLASS_LEN 128

typedef struct {
    Window id;
    char title[MAX_TITLE_LEN];
    char class_name[MAX_CLASS_LEN];
    char instance[MAX_CLASS_LEN];
    char type[16]; // "Normal" or "Special"
    int desktop;
    int pid;
} WindowInfo;

#endif // WINDOW_INFO_H