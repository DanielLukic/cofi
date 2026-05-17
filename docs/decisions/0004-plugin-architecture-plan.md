# cofi plugin architecture plan

Status: draft for TFD-675

Progress:

- Phases 1-3 landed in `f047647`.
- Provider required metadata now replaces the hardcoded Config exception.
- Command parse definitions now carry explicit `core` or provider ownership.
- New providers can request dynamic tab handles; existing provider tabs still
  keep legacy `TAB_*` handles for compatibility.

## Problem

The provider system made tabs cheaper to add, but it is not yet a plugin
architecture. Providers now own list rendering and tab-local behavior, but
plugin behavior still leaks through central command metadata, static `TabMode`
values, hardcoded prefix claims, daemon opcodes, hotkey delegates, and config
special cases.

TFD-675 should move ownership to compiled-in plugin modules without introducing
dynamic `.so` loading, scripting, sandboxing, or an external package manager.
The first goal is not "plugin marketplace." The first goal is that adding,
disabling, or removing a compiled-in capability does not require editing a stack
of unrelated core tables.

## Current State

Already-good pieces:

- `CofiTabProvider` is the tab/list provider API.
- The provider registry already tracks enabled/disabled state.
- `cofi_get_provider_for_command()` resolves provider primaries/aliases.
- `cofi_get_provider_for_prefix()` resolves simple modal prefixes such as `!`
  and `=`.
- Command candidates and help already call `command_primary_is_available()`.

Remaining broken windows:

- Core command truth is now centralized in `command_registry`, but core commands
  still register from one built-in list.
- Provider-owned commands live on their providers today; the remaining cleanup
  is making provider modules register command specs through the same registry
  API instead of exposing command fields on `CofiTabProvider`.
- Existing provider tabs still use legacy static `TabMode` values during
  migration, though new provider tabs can use dynamic handles.
- `$`, `\`, and `>` prefix claims still live in `prefix_tabs.c`.
- Daemon opcodes and some hotkey modes still directly name tabs/modes.

## Terms

- **Core**: window ingestion, display pipeline, input routing, selection,
  overlays, daemon process, X11/EWMH primitives, persistence helpers, and the
  registries.
- **Core-owned command**: a command that operates on core window/app state, such
  as `tw`, `cw`, `aot`, `show`, or `set`.
- **Plugin**: a compiled-in capability module that can contribute a provider tab,
  command metadata, prefixes, slots, config rows, daemon delegates, hotkey
  actions, or later rule predicates/actions.
- **Provider**: the list/tab surface of a plugin. A plugin may have no tab.

## V1 Constraints

- Keep `disabled_providers` as the persisted config backend. Improve the
  registry metadata behind it; do not redesign config storage in this slice.
- Keep existing daemon opcodes backward compatible.
- Keep plugin-specific config schemas and rule/action contribution out of the
  first implementation slices.
- Do not introduce dynamic loading.

## Design Decisions

- Disabled plugin behavior is enforced by registry lookup, not by hiding UI
  only.
- Required/non-disableable is provider/plugin metadata, not `strcmp(id,
  "config")`.
- Alias collisions are registration errors. Tests/debug builds should fail hard;
  production should log and skip the conflicting registration until a conflict
  UI exists.
- `show` remains a native/core command until tab surfacing is registry-backed.
- Stale persisted data such as slots for disabled plugins remains stored and is
  ignored while the plugin is disabled.

## Phase 0: Stop Central Table Churn

Do not commit more reshuffling of command tables as "plugin architecture." A
central command table with a different name is still a central command table.

The uncommitted command-metadata refactor should be treated as disposable unless
it becomes the small facade described below.

## Phase 1: Tighten Existing Provider Metadata (landed)

This is the smallest useful cleanup and should happen before bigger registry
work.

Changes:

- Add required/disableable metadata to `CofiTabProvider`.
- Replace `cofi_provider_is_disableable()`'s hardcoded `"config"` check with the
  metadata field.
- Make every provider registration set the field deliberately.

Acceptance:

- Config cannot be disabled.
- Non-required providers still can be disabled from the Config overlay.
- The Config overlay is generated from provider metadata.
- `show_all_tabs` still skips disabled providers.

## Phase 2: Command Ownership Cleanup (landed)

The immediate command problem is not that cofi lacks a giant new
`CofiCommandSpec`. The problem is that the existing central command tables have
incomplete and stringly ownership.

Keep this slice minimal:

- Every parser metadata entry gets an explicit owner:
  - `core` for core-owned commands;
  - provider id for provider-owned commands.
- Provider-owned command availability checks that provider id directly instead
  of routing through the former `.provider_command` strings and
  `cofi_get_provider_for_command()`.
- Commands with no owner are test failures.
- Candidate generation, help output, availability checks, keep-open policy,
  activation policy, and dispatch all read through one command metadata API.

Acceptance:

- command ordering in candidates is unchanged;
- alias resolution is unchanged;
- compact command parsing is unchanged;
- help text is unchanged;
- keep-open and activates-window policy is unchanged;
- disabled provider commands disappear from candidates, help, availability
  checks, and dispatch;
- core-owned commands remain available.

This is intentionally not a broad command-registry rewrite. It is the last
cleanup before command metadata starts moving into owner modules.

## Phase 3: Dynamic Provider Tab Handles (foundation landed)

Static `TabMode` is the biggest remaining obstacle to adding real plugin tabs.
Today, adding a tab requires touching enum values, tab metadata, visibility
state, tests, and often hardcoded routing.

Introduce dynamic provider tab handles while keeping existing core tabs stable:

- Windows can remain core-special.
- Existing provider tabs can keep compatibility enum values during migration.
- New provider tabs should not require adding `TAB_*` enum values.
- Header formatting, tab cycling, visibility, and `surface_tab` should use
  provider registry handles for provider tabs.

Acceptance:

- adding a new provider tab does not require editing `app_data.h`'s `TabMode`
  enum;
- `tab_metadata.c` no longer needs a case for every provider tab;
- `show_all_tabs` and tab header overflow work with dynamically registered
  provider tabs;
- disabled dynamic provider tabs do not appear in cycling/header output.

This phase is larger than Phase 1/2, but it is the first actual architectural
move toward plugins rather than table cleanup.

## Phase 4: Move Command Metadata to Owners

Once ownership is explicit and tab handles are no longer hardcoded for new
providers, move command metadata out of central tables one module at a time.

Progress:

- Provider-backed commands have moved out of the legacy command tables and live
  on their provider modules.
- Core commands now use one `CommandSpec` shape via `command_registry`, so parse
  metadata, handlers, help text, activation policy, keep-open policy, and owner
  metadata are no longer split between `COMMAND_PARSE_DEFS[]` and
  `COMMAND_DEFINITIONS[]`.
- Remaining work: make providers call the command registry directly for their
  command specs, then remove command fields from `CofiTabProvider`.

Suggested order:

1. Config, Names, or Rules command surface: simple tab surfacing, low coupling.
2. Profiles: real aliases plus slots, no prefix/delegate complexity.
3. Sessions: aliases and slots across a larger provider.
4. Calc and Run after prefix behavior is settled.

Acceptance for each migrated plugin:

- its command metadata lives in the owning module;
- central core metadata no longer lists that plugin command;
- aliases are preserved;
- disabled plugin command dispatch fails closed;
- command candidates/help/parser behavior remain unchanged when enabled.

Implementation can be a small command metadata registration API. It does not
need to duplicate the provider registry; it should index metadata and point back
to the owning provider/core owner.

## Phase 5: Prefix Claims

Some prefix behavior is already registry-backed:

- `!` resolves through the Run provider's `prefix_char`;
- `=` resolves through the Calc provider's `prefix_char`.

The remaining work is narrow:

- `$` and `\` should be owned by Apps or by an explicit core/provider prefix
  registration instead of `get_tab_claim()`;
- `>` should be clearly core-owned if it remains a core tab claim;
- disabled providers must make their prefixes fail closed;
- paste/full-entry prefix behavior must not regress.

Do not introduce a broad `CofiPrefixSpec` unless the simple provider field is no
longer enough.

## Phase 6: Delegate Surfaces

Daemon and hotkey delegates are not a plugin API today. Some paths already query
providers, especially Run via `cofi_get_provider_for_prefix('!')`, but fixed
opcodes and `ShowMode` values still encode core knowledge.

Defer this until command and tab ownership are stable.

Future acceptance:

- disabled plugin daemon delegates fail closed;
- disabled plugin hotkey delegates fail closed;
- fixed opcodes remain backward compatible;
- any new plugin-facing delegate uses a string/action id rather than requiring a
  new core opcode.

## Phase 7: Config and Rules

Do not add `CofiConfigSpec` or `CofiRuleSpec` in the early plugin architecture
slices.

Config has two separate problems:

- provider/plugin enablement, which is already covered by provider metadata and
  the existing `disabled_providers` storage;
- plugin-specific config rows, which need a design for row rendering, type
  validation, writeback, persistence, and overlay editing.

Rules also have two separate concepts:

- user-owned rule config data;
- possible plugin-contributed predicates/actions later.

Those are not the same thing. Treat plugin config rows and rule
predicates/actions as later work after command/tab/prefix ownership is clean.

## Tests Required

Minimum coverage as phases land:

- required vs disableable provider metadata;
- disabled provider skipped by `show_all_tabs`, header rendering, and tab
  cycling;
- every command metadata entry has an owner;
- disabled provider commands disappear from candidates, help, availability
  checks, and dispatch;
- alias collision rejection;
- compact command parsing stability;
- dynamic provider tab registration and visibility;
- prefix behavior for `!`, `=`, `$`, `\`, `>`, and `:`;
- daemon/hotkey disabled-plugin behavior once delegates move.

## Non-goals

- dynamic loading;
- scripting runtime;
- permissions/sandbox enforcement;
- rewriting all native commands as plugins;
- moving X11 window ingestion out of core;
- plugin-specific config schemas in the first command ownership slices;
- daemon protocol redesign in the first command ownership slices.
