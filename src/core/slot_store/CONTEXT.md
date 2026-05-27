# Slot Store

## Purpose
The slot store provides shared key-to-payload persistence for provider-backed
slots. It lets features assign compact `@<key>` shortcuts to tab-specific
payloads without each feature inventing its own storage schema.

## Boundary

### Owns
- The `SlotStore` and `SlotEntry` in-memory mapping from `(tab_id, slot_key)`
  to a caller-defined payload string.
- Slot key parsing, index conversion, and normalization for numeric and letter
  slot keys.
- The JSON persistence schema for shared slot assignments.
- Tolerant loading of persisted slot records, including skipping malformed
  entries without failing the whole file.

### Does Not Own
- The meaning of a payload for any provider or feature.
- Recall behavior after a payload is found; callers decide how to activate it.
- Harpoon's window-slot model or per-workspace visible-window digit slots.
- UI rendering, key dispatch, or provider opt-in policy for slot support.
- JSON parser/writer primitives, which live in `core/json`.

## Public Surface
- `slot_store_init` creates an empty store at the default persistence path.
- `slot_store_init_with_path` creates an empty store using an explicit path.
- `slot_store_free` releases allocated entries owned by the store.
- `slot_index_from_key` converts valid slot keys to stable slot indexes.
- `slot_key_from_index` converts stable slot indexes back to canonical keys.
- `slot_parse_at_key_arg` parses command arguments of the form `@<slot>`.
- `slot_assign` creates or replaces a `(tab_id, slot_key)` assignment.
- `slot_clear` removes a `(tab_id, slot_key)` assignment.
- `slot_lookup` returns the payload assigned to a tab/key pair.
- `slot_for_payload` returns the key assigned to a tab/payload pair.
- `slot_save` writes the current store to JSON.
- `slot_load` replaces the current entries with valid records from JSON.

## Acceptance Criteria
1. A freshly initialized store starts empty and uses the default persistence
   path under `~/.config/cofi/harpoon.json`.
2. `slot_store_init_with_path` uses the caller-provided non-empty path while
   preserving the same empty-store initialization behavior.
3. `slot_store_free` releases the dynamic entry array and resets entry count
   and capacity to zero.
4. Slot keys `0` through `9` map to indexes `0` through `9`.
5. Letter slot keys map case-insensitively after the numeric range, so `a` and
   `A` both map to index `10`.
6. Invalid slot keys return `-1` from `slot_index_from_key`, and invalid slot
   indexes return `'\0'` from `slot_key_from_index`.
7. Canonical keys produced from letter indexes are lowercase.
8. `slot_parse_at_key_arg` accepts only one valid slot key prefixed by `@`,
   ignoring surrounding whitespace but rejecting extra non-whitespace text.
9. Assigning a slot with a missing store, invalid key, missing tab id, or empty
   payload is a no-op.
10. Assigning an existing tab/key pair replaces that entry's payload instead
   of creating a duplicate.
11. Assigning a new tab/key pair appends a new entry and marks it assigned.
12. Clearing a tab/key pair removes the matching entry and compacts the entry
   array so later lookups no longer find it.
13. `slot_lookup` returns `NULL` for invalid keys, missing tabs, or unassigned
   tab/key pairs.
14. `slot_for_payload` returns the assigned key only when both tab id and
   payload match an assigned entry.
15. `slot_save` writes assigned entries as a root object with a `slots` array;
   each element contains `slot`, `tab`, and `payload` string fields.
16. `slot_save` returns false when called without a store or without a
   configured path.
17. `slot_load` preserves the configured path, clears existing entries, and
   replaces them with valid entries from the configured JSON file.
18. `slot_load` returns false for a missing, unreadable, malformed, or empty
   slot file, and returns true once at least one valid entry is loaded.
19. Malformed persisted entries are skipped without preventing later valid
   entries in the same file from loading.
20. Tab ids and payloads are bounded by the fixed sizes in `slot_store.h`; the
   store truncates through GLib string-copy helpers rather than growing fields
   dynamically.

## Notes
The default file name remains `harpoon.json` for compatibility with existing
persisted slot assignments even though the store is shared by multiple
provider-backed features.
