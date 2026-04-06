#include "display_pipeline.h"

static gint calculate_flipped_offset(gint total_count, gint max_lines,
                                     gint scroll_offset) {
    if (total_count <= max_lines) {
        return 0;
    }

    return (total_count - max_lines) - scroll_offset;
}

void calculate_display_range(gint total_count, gint max_lines, gint scroll_offset,
                             gint *start_idx_out, gint *end_idx_out) {
    if (!start_idx_out || !end_idx_out) {
        return;
    }

    gint start_idx = scroll_offset;
    gint end_idx = start_idx + max_lines;

    if (end_idx > total_count) {
        end_idx = total_count;
    }

    *start_idx_out = start_idx;
    *end_idx_out = end_idx;
}

gboolean render_display_pipeline(const DisplayPipelineRequest *request,
                                 GString *text) {
    if (!request || !text || !request->render_item) {
        return FALSE;
    }

    gint start_idx = 0;
    gint end_idx = 0;
    calculate_display_range(request->total_count, request->max_lines,
                            request->scroll_offset, &start_idx, &end_idx);

    gint display_line = 0;
    for (gint i = end_idx - 1;
         i >= start_idx && display_line < request->max_lines;
         i--, display_line++) {
        request->render_item(request->context, i, request->selected_idx, text);
    }

    if (request->overlay_scrollbar) {
        gint flipped = calculate_flipped_offset(request->total_count,
                                                request->max_lines,
                                                request->scroll_offset);
        request->overlay_scrollbar(request->overlay_context, text,
                                   request->total_count, request->max_lines,
                                   flipped, request->target_columns);
    }

    return TRUE;
}
