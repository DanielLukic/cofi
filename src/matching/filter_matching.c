#include "core/app/app_data.h"
#include "matching/match_entry.h"
#include "matching/match.h"
#include "core/log/log.h"
#include <string.h>
#include <stdio.h>

// Filter named windows based on search text
void filter_matching(AppData *app, const char *filter) {
    app->filtered_matching_count = 0;
    
    if (!filter || !*filter) {
        // No filter - show all named windows
        for (int i = 0; i < app->matching.count; i++) {
            app->filtered_matching[app->filtered_matching_count] = app->matching.entries[i];
            app->filtered_matching_count++;
        }
        return;
    }
    
    // Build searchable string for each named window
    char searchable[1024];
    for (int i = 0; i < app->matching.count; i++) {
        MatchEntry *entry = &app->matching.entries[i];
        
        snprintf(searchable, sizeof(searchable), "%s %s %s %s %s",
                 entry->custom_name, entry->original_title,
                 entry->class_name, entry->instance, entry->type);
        
        // Use has_match for filtering
        if (has_match(filter, searchable)) {
            app->filtered_matching[app->filtered_matching_count] = *entry;
            app->filtered_matching_count++;
        }
    }
}
