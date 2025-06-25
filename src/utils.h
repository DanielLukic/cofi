#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

// Safe string copy - always null-terminates the destination
void safe_string_copy(char *dest, const char *src, size_t dest_size);

#endif // UTILS_H