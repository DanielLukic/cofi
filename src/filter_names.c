#include "app_data.h"
#include "named_window.h"
#include "match.h"
#include "log.h"
#include <string.h>
#include <stdio.h>

// Filter named windows based on search text
void filter_names(AppData *app, const char *filter) {
    app->filtered_names_count = 0;
    
    if (!filter || !*filter) {
        // No filter - show all named windows
        for (int i = 0; i < app->names.count; i++) {
            app->filtered_names[app->filtered_names_count] = app->names.entries[i];
            app->filtered_names_count++;
        }
        return;
    }
    
    // Build searchable string for each named window
    char searchable[1024];
    for (int i = 0; i < app->names.count; i++) {
        NamedWindow *entry = &app->names.entries[i];
        
        // Build searchable string: "custom_name original_title class instance"
        snprintf(searchable, sizeof(searchable), "%s %s %s %s",
                 entry->custom_name, entry->original_title, 
                 entry->class_name, entry->instance);
        
        // Use has_match for filtering
        if (has_match(filter, searchable)) {
            app->filtered_names[app->filtered_names_count] = *entry;
            app->filtered_names_count++;
        }
    }
}