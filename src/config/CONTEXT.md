# Config

## Purpose
The config subsystem owns cofi's primary options file, setting validation, the
config-entry registry, and the Config provider tab.

## Boundary

### Owns
- Defaults, load/save, validation, and conversion for base `CofiConfig` options
  stored in `~/.config/cofi/options.json`.
- The config-entry registry used by feature settings, command `set`, and the
  Config tab.
- The Config provider tab, its config edit/provider-enablement overlays, and
  the `config`/`conf`/`cfg` command.
- Persistence of the `disabled_providers` config key in `options.json`; this is
  the stored provider-id list consumed by the provider registry at startup or
  overlay save time.
- Immediate side effects intrinsic to base settings, such as applying `log_level`.

### Does Not Own
- Per-feature persistence files such as rules, harpoon slots, layouts, hotkeys,
  sessions, projects, or match entries.
- The JSON load/save primitive; tolerant JSON file I/O lives in `core/json/`.
- Feature-specific validation logic registered through `CofiConfigSpec`.
- Generic overlay hosting, stacking, hiding, and rendering infrastructure.
- Runtime provider enablement state, provider lookup filtering, or disabled-list
  apply/string helpers; those live in `providers/`.
- Feature behavior hidden or shown by provider enablement.

## Public Surface
- Types: `CofiConfig`, `ConfigEntry`, `ConfigFieldType`, `CofiConfigSpec`,
  and config enums.
- Lifecycle: `init_config_defaults()`, `load_config()`, and `save_config()`.
- Settings: `apply_config_setting()`, `get_next_enum_value()`,
  `build_config_entries()`, and enum/string conversion helpers.
- Registry: `cofi_register_config_entry()`, `cofi_config_entry_count()`,
  `cofi_config_entry_at()`, `cofi_config_entry_for_key()`, and
  `cofi_config_registry_reset()`.
- Tab: `config_provider_register()`, `config_tab_mode()`, `filter_config()`,
  `handle_config_tab_keys()`, selection helpers, and edit policy.
- Overlays: `create_config_edit_overlay_content()`,
  `create_provider_enablement_overlay_content()`,
  `handle_config_edit_key_press()`, and
  `handle_provider_enablement_key_press()`.

## Acceptance Criteria
1. `init_config_defaults()` initializes every base field to product defaults,
   including empty provider/tool-path strings and debug log level.
2. `load_config()` always starts from defaults, then overlays only valid values
   present under the `options` object in `~/.config/cofi/options.json`.
3. Missing config files are created with defaults; corrupt JSON or missing
   `options` leaves defaults in memory without rewriting the bad file.
4. Saved config writes the canonical top-level `options` object, uses dotted
   keys such as `rules.show_all_tags`, and includes registered feature settings
   by asking their `CofiConfigSpec.get_value` callbacks.
5. Legacy inputs remain readable: `rules_show_all_tags` maps to dotted rules,
   `quick_workspace_slots=true` maps to workspace digit mode, and fractional
   slot-occlusion thresholds convert to percent integers.
6. Invalid persisted values normally preserve init defaults; `tile_columns` is
   the exception, replacing invalid values with `3` instead of `2` (TFD-829).
7. Invalid persisted values for registered feature specs are ignored with a
   warning; the field keeps the default produced at registration time.
8. `apply_config_setting()` rejects null arguments, unknown keys, invalid enums,
   malformed booleans, negative constrained integers, unsupported tile columns,
   and out-of-range slot occlusion thresholds.
9. Boolean settings accept `true`/`false`, `on`/`off`, and `1`/`0`; successful
   mutations update only the addressed field.
10. Enum settings accept only published string values; `get_next_enum_value()`
   cycles known values in display order and wraps unknown current values.
11. Setting `log_level` validates against trace/debug/info/warn/error/fatal,
    stores the requested string, and applies the numeric logger level
    immediately.
12. `disabled_providers` stores an empty or comma-separated provider-id string
    up to `CONFIG_DISABLED_PROVIDERS_LEN`; overlong values are rejected.
13. The config registry rejects missing callbacks, empty or overlong keys,
    duplicate keys, and registrations past capacity; reset clears the registry.
14. `build_config_entries()` emits base settings in stable order, represents an
    empty disabled-provider list as `(none)`, then appends registered feature
    entries up to `MAX_CONFIG_ENTRIES`.
15. The Config tab filters by key plus display value, shows all entries for an
    empty query, and shows a non-actionable empty-state row when no entries match.
16. Config row identities are `config:<key>` so selection can return to a
    setting after filtering or value refresh.
17. Config selection clamps into the filtered list; selecting a missing key
    falls back to the first visible row.
18. Ctrl+T cycles boolean and enum settings, saves on success, refilters using
    current entry text, restores selection to the edited key, and refreshes.
19. Ctrl+E opens the edit overlay only for integer and string entries; Enter
    applies, saves or logs validation failure, then returns to the edited key.
20. The Config provider registers as a hidden required dynamic tab and exposes
    `config`, `conf`, and `cfg` aliases that surface the tab and keep cofi open.
21. The provider-enablement overlay lists enabled state, refuses required-provider
    toggles, saves on Enter, and refreshes back to `disabled_providers`.

## Notes
Base config persistence (`options.json`), the Config settings tab, and the
config edit/provider-enablement overlays share `CofiConfig` data and the
config-entry registry. They stay colocated here rather than splitting across
`config/` and `ui/` because the coupling is too tight to make a clean seam worth
the complexity.

`disabled_providers` is stored as a comma-separated string in `options.json` by
this subsystem, but runtime enablement state and disabled-list apply/string
helpers live in `providers/`. Format changes require coordinated updates across
both subsystems.

`tile_columns` defaults to `2` in `init_config_defaults()`, but invalid
persisted values fall back to `3` during `load_config()` (TFD-829). UI hints
should match the `3` fallback until the divergence is reconciled.
