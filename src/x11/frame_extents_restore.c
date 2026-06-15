#include "x11/frame_extents_restore.h"

#include <glib.h>
#include <stdint.h>

#include "core/app/app_data.h"
#include "core/log/log.h"
#include "geom/window_geometry_matching.h"

#define FRAME_EXTENTS_RESTORE_DELAY_MS 150

typedef struct {
    AppData *app;
    Window window_id;
    guint source_id;
} DeferredFrameRestore;

static GHashTable *pending_frame_restores = NULL;

#ifdef COFI_TESTING
static FrameExtentsTimeoutAddFunc frame_extents_timeout_add = g_timeout_add;
static FrameExtentsSourceRemoveFunc frame_extents_source_remove = g_source_remove;

void frame_extents_restore_set_timeout_add_for_test(FrameExtentsTimeoutAddFunc timeout_add) {
    frame_extents_timeout_add = timeout_add ? timeout_add : g_timeout_add;
}

void frame_extents_restore_set_source_remove_for_test(FrameExtentsSourceRemoveFunc source_remove) {
    frame_extents_source_remove = source_remove ? source_remove : g_source_remove;
}
#else
#define frame_extents_timeout_add g_timeout_add
#define frame_extents_source_remove g_source_remove
#endif

static gpointer window_key(Window window_id) {
    return (gpointer)(uintptr_t)window_id;
}

static GHashTable *pending_restores(void) {
    if (!pending_frame_restores) {
        pending_frame_restores = g_hash_table_new(g_direct_hash, g_direct_equal);
    }
    return pending_frame_restores;
}

static WindowInfo *find_window(AppData *app, Window window_id) {
    if (!app) {
        return NULL;
    }

    for (int i = 0; i < app->window_count; i++) {
        if (app->windows[i].id == window_id) {
            return &app->windows[i];
        }
    }
    return NULL;
}

static gboolean deferred_restore_cb(gpointer data) {
    DeferredFrameRestore *ctx = (DeferredFrameRestore *)data;
    GHashTable *pending = pending_restores();

    if (g_hash_table_lookup(pending, window_key(ctx->window_id)) == ctx) {
        g_hash_table_remove(pending, window_key(ctx->window_id));

        WindowInfo *window = find_window(ctx->app, ctx->window_id);
        if (window) {
            log_info("GEOMDBG: extents-changed deferred re-apply 0x%lx", ctx->window_id);
            restore_window_geometry_for_window(ctx->app, window);
        }
    }

    g_free(ctx);
    return G_SOURCE_REMOVE;
}

void handle_net_frame_extents_property(AppData *app, Window window_id) {
    if (!find_window(app, window_id)) {
        return;
    }

    GHashTable *pending = pending_restores();
    gpointer key = window_key(window_id);
    DeferredFrameRestore *existing = g_hash_table_lookup(pending, key);
    if (existing) {
        frame_extents_source_remove(existing->source_id);
        g_hash_table_remove(pending, key);
        g_free(existing);
    }

    DeferredFrameRestore *ctx = g_new0(DeferredFrameRestore, 1);
    ctx->app = app;
    ctx->window_id = window_id;
    ctx->source_id = frame_extents_timeout_add(FRAME_EXTENTS_RESTORE_DELAY_MS,
                                               deferred_restore_cb,
                                               ctx);
    g_hash_table_insert(pending, key, ctx);
}

void cleanup_frame_extents_restore_timeouts(void) {
    if (!pending_frame_restores) {
        return;
    }

    GHashTableIter iter;
    gpointer key;
    gpointer value;
    g_hash_table_iter_init(&iter, pending_frame_restores);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        (void)key;
        DeferredFrameRestore *ctx = (DeferredFrameRestore *)value;
        if (ctx->source_id > 0) {
            frame_extents_source_remove(ctx->source_id);
        }
        g_free(ctx);
    }

    g_hash_table_destroy(pending_frame_restores);
    pending_frame_restores = NULL;
}
