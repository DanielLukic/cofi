# Core JSON

## Purpose
Core JSON owns the shared persistence helper that reads and writes tolerant JSON stores for cofi subsystems.

## Boundary

### Owns
- Tolerant JSON load/save helpers shared by persistent stores.
- Error handling conventions for malformed JSON files.
- The small wrapper around json-glib used by feature-specific config modules.
- Object-root-only file loading: callers get a parser only when the file exists,
  parses successfully, and the root node is a JSON object.
- Same-directory tmp+rename save semantics that leave the target file untouched
  on write or rename failure.

### Does Not Own
- Feature-specific schemas or migration policy.
- User-visible behavior for any saved feature.
- Filesystem location decisions owned by callers.

## Public Surface
- `cofi_json_load_object_file()` loads a JSON object-root file and returns a
  parser whose tree owns any borrowed objects, arrays, or strings.
- `cofi_json_save_root()` writes a caller-provided root node through
  `<path>.tmp` and `rename()`.
- `cofi_json_obj_str_or()` reads a string member or returns the caller fallback.
- `cofi_json_obj_int_or()` reads an integer member or returns the caller
  fallback.
- `cofi_json_obj_bool_or()` reads a boolean member or returns the caller
  fallback.
- `cofi_json_obj_array()` returns an array member or `NULL`.
- `cofi_json_obj_object()` returns an object member or `NULL`.

## Acceptance Criteria
1. Malformed JSON input fails closed without crashing callers.
2. Missing files are reported in the same way existing stores expect.
3. Non-object JSON roots are rejected so callers never borrow feature state from
   an unexpected top-level array, scalar, or null.
4. Saved JSON is written to a temporary sibling path and then renamed over the
   target so ordinary write failures leave the previous target file in place.
5. A failed rename unlinks the temporary file and leaves the target path
   untouched.
6. Saved JSON remains readable by every subsystem that uses the helper.
7. Callers retain ownership of feature-specific structs and defaults.
8. The helper stays independent of any single feature store.

## Notes
See `docs/adr/0009-tolerant-json-io-via-json-glib.md` for the persistence direction this helper supports.
`cofi_json_save_root()` does not call `fsync`; tmp+rename protects ordinary
write/rename failures, not mid-write crash durability.
