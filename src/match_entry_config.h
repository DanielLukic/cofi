#ifndef MATCH_ENTRY_CONFIG_H
#define MATCH_ENTRY_CONFIG_H

#include "match_entry.h"

// Save matching entries to JSON config file.
void save_match_entries(const MatchEntryManager *manager);

// Load matching entries from JSON config file.
void load_match_entries(MatchEntryManager *manager);

#endif // MATCH_ENTRY_CONFIG_H
