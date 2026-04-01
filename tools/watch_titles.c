/*
 * watch_titles.c — prototype to verify per-window title change events
 *
 * Subscribes to PropertyNotify on all existing windows + new windows,
 * prints whenever _NET_WM_NAME or WM_NAME changes.
 *
 * Build: gcc -o watch_titles watch_titles.c -lX11
 * Run:   ./watch_titles
 */

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Atom net_client_list;
static Atom net_wm_name;
static Atom utf8_string;

static void subscribe(Display *dpy, Window w) {
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(dpy, w, &attrs)) return;
    XSelectInput(dpy, w, attrs.your_event_mask | PropertyChangeMask);
}

static void subscribe_all(Display *dpy, Window root) {
    Atom actual_type;
    int actual_format;
    unsigned long n_items, bytes_after;
    unsigned char *prop = NULL;

    if (XGetWindowProperty(dpy, root, net_client_list, 0, (~0L), False,
                           XA_WINDOW, &actual_type, &actual_format,
                           &n_items, &bytes_after, &prop) != Success || !prop)
        return;

    Window *wins = (Window *)prop;
    for (unsigned long i = 0; i < n_items; i++) {
        subscribe(dpy, wins[i]);
        printf("subscribed: 0x%lx\n", wins[i]);
    }
    XFree(prop);
}

static char *get_title(Display *dpy, Window w) {
    Atom actual_type;
    int actual_format;
    unsigned long n_items, bytes_after;
    unsigned char *prop = NULL;

    /* Try _NET_WM_NAME first (UTF-8) */
    if (XGetWindowProperty(dpy, w, net_wm_name, 0, 1024, False,
                           utf8_string, &actual_type, &actual_format,
                           &n_items, &bytes_after, &prop) == Success && prop) {
        char *title = strdup((char *)prop);
        XFree(prop);
        return title;
    }

    /* Fall back to WM_NAME */
    if (XGetWindowProperty(dpy, w, XA_WM_NAME, 0, 1024, False,
                           XA_STRING, &actual_type, &actual_format,
                           &n_items, &bytes_after, &prop) == Success && prop) {
        char *title = strdup((char *)prop);
        XFree(prop);
        return title;
    }

    return strdup("(no title)");
}

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "Cannot open display\n"); return 1; }

    Window root = DefaultRootWindow(dpy);

    net_client_list = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    net_wm_name     = XInternAtom(dpy, "_NET_WM_NAME", False);
    utf8_string     = XInternAtom(dpy, "UTF8_STRING", False);

    /* Watch root for new windows + client list changes */
    XSelectInput(dpy, root, PropertyChangeMask | SubstructureNotifyMask);

    /* Subscribe to all existing windows */
    subscribe_all(dpy, root);
    printf("watching for title changes... (Ctrl+C to stop)\n\n");
    fflush(stdout);

    XEvent ev;
    while (1) {
        XNextEvent(dpy, &ev);

        if (ev.type == PropertyNotify) {
            XPropertyEvent *pe = &ev.xproperty;

            /* New windows in client list — subscribe to them */
            if (pe->window == root && pe->atom == net_client_list) {
                subscribe_all(dpy, root);
                continue;
            }

            /* Per-window title change */
            if (pe->atom == net_wm_name || pe->atom == XA_WM_NAME) {
                char *title = get_title(dpy, pe->window);
                printf("TITLE CHANGE 0x%lx -> \"%s\"\n", pe->window, title);
                fflush(stdout);
                free(title);
            }
        }

        if (ev.type == CreateNotify) {
            subscribe(dpy, ev.xcreatewindow.window);
        }
    }

    XCloseDisplay(dpy);
    return 0;
}
