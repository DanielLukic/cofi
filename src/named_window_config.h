#ifndef NAMED_WINDOW_CONFIG_H
#define NAMED_WINDOW_CONFIG_H

#include "named_window.h"

// Save named windows to JSON config file
void save_named_windows(const NamedWindowManager *manager);

// Load named windows from JSON config file
void load_named_windows(NamedWindowManager *manager);

#endif // NAMED_WINDOW_CONFIG_H