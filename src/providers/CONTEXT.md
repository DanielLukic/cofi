# Providers

## Purpose
The providers subsystem defines the shared tab-provider contract and the
process-wide registry that lets feature folders contribute tabs, modal prefixes,
delegate opcodes, command targets, hotkey modes, and row/action callbacks.

## Boundary

### Owns
- `CofiTabProvider`, row/action status types, modal policy, and provider row-cell
  metadata.
- Registration, lookup, runtime enablement state, disabled-provider apply/string
  helpers, and dynamic tab-handle assignment.
- Filtered-row to raw-row mapping and generation tokens for provider refreshes.
- Built-in provider and command registration order for the single cofi binary.

### Does Not Own
- Feature tab behavior, row data, matching strings, actions, persistence, or
  overlay content implemented by individual feature folders.
- Tab rendering, tab switching, prefix dispatch, or modal UI mechanics.
- Command parsing/execution semantics, hotkey grabbing, daemon delegation, or
  config-file storage.
- Persistence of the disabled-provider list in `options.json` and the
  provider-enablement overlay UI; those live in `config/`.

## Public Surface
- `cofi_tab_provider.h`: `CofiTabProvider`, `CofiActionStatus`,
  `CofiRowCells`, `CofiRowFlags`, `CofiModalPolicy`, provider constants, registry
  APIs, filtered-map APIs, generation APIs, dispatch helpers, and test reset.
- `builtin_plugins.h`: `cofi_register_builtin_plugins()`.

## Acceptance Criteria
1. `cofi_init_provider_defaults()` zeroes the provider struct, marks providers
   hidden by default, and sets `COFI_MODAL_HIDE_ON_ESC`.
2. `cofi_register_tab_provider()` rejects null input, registry capacity overflow,
   duplicate provider ids, duplicate tab handles, invalid tab handles, and dynamic
   tab allocation past `COFI_MAX_TAB_HANDLES`.
3. Dynamic providers registered with `COFI_PROVIDER_DYNAMIC_TAB` receive stable
   tab handles starting after `TAB_COUNT`.
4. Registered providers are enabled by default and remain addressable by provider
   id even if later disabled.
5. Provider lookup by tab, modal prefix, tab prefix, delegate opcode, and hotkey
   mode returns only enabled providers.
6. Required providers and providers without non-empty ids are not disableable;
   attempts to disable them leave them enabled.
7. `cofi_apply_disabled_providers()` disables only disableable providers whose id
   appears as a comma- or whitespace-separated token.
8. `cofi_build_disabled_providers_string()` emits disabled provider ids as a
   comma-separated string and truncates safely to the caller buffer.
9. `cofi_list_provider_tabs()` lists enabled dynamic-tab providers only, in
   registration order, up to the caller-provided capacity.
10. Filtered maps are capped at `COFI_MAX_FILTERED`; out-of-range filtered
    indexes and unknown provider ids map to `-1`.
11. Provider generation tokens increment per provider on `cofi_next_generation()`
    and return `-1` for invalid provider ids.
12. Dispatch helpers return neutral values for disabled, missing, or callback-less
    providers and otherwise call the provider callback with the caller-supplied
    arguments unchanged.
13. `cofi_registry_reset()` clears all providers, filtered maps, enablement
    state, generations, and dynamic-tab allocation for tests.
14. `cofi_register_builtin_plugins()` registers core commands before provider
    tabs, then registers the built-in feature providers in deterministic order.
15. `on_query_changed` is for genuine query changes only and may select the
    best/top match. Provider-owned data refresh, periodic tick, and mutation
    paths must apply the selection-preservation policy (`preserve_selection()`
    then `restore_selection()`), not `reset_selection()`. (TFD-835 tracks
    providers not yet compliant.) (authoritative rule: `docs/architecture.md` §
    Cross-cutting invariants)

## Notes
This folder is intentionally only the shared interface and registry. New feature
providers belong in their feature folder and should expose a small
`<feature>_provider_register()` entrypoint consumed by `builtin_plugins.c`.
