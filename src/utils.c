#include "utils.h"
#include <string.h>

// Safe string copy - always null-terminates the destination
void safe_string_copy(char *dest, const char *src, size_t dest_size) {
    if (!dest || dest_size == 0) return;
    
    if (!src) {
        dest[0] = '\0';
        return;
    }
    
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}