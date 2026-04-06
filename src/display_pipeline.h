#ifndef DISPLAY_PIPELINE_H
#define DISPLAY_PIPELINE_H

#include <glib.h>

typedef void (*display_render_item_fn)(gpointer context, gint index,
                                       gint selected_idx, GString *text);
typedef void (*display_overlay_scrollbar_fn)(gpointer context, GString *text,
                                             gint total_items, gint visible_items,
                                             gint scroll_offset, gint target_columns);

typedef struct {
    gint total_count;
    gint max_lines;
    gint scroll_offset;
    gint selected_idx;
    gint target_columns;
    gpointer context;
    gpointer overlay_context;
    display_render_item_fn render_item;
    display_overlay_scrollbar_fn overlay_scrollbar;
} DisplayPipelineRequest;

void calculate_display_range(gint total_count, gint max_lines, gint scroll_offset,
                             gint *start_idx_out, gint *end_idx_out);

gboolean render_display_pipeline(const DisplayPipelineRequest *request,
                                 GString *text);

#endif
