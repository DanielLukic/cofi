# cofi plugin architecture — brainstorm consolidation

Two-round divergent brainstorm with sam (codex). Compiled-in plugins assumed (no `.so` loading needed for v1). Below: filter sheet — no critique, no recommendation. Pick what stays, cuts, defers.

## A. The spine — pick one

- **A1.** Action registry as primitive (commands/hotkeys/menu/Enter all map to actions)
- **A2.** Action *graph* — actions w/ inputs, confirms, dry-run, undo, composition (select-sink → notify → close), introspectable, dynamically disable-able with reason
- **A3.** Plain command + hotkey registries, no unification

## B. Plugin = capability bundle (which capabilities ship in v1?)

tab · row injector · action · keybinding · query-prefix · status · event subscriber · rule predicate · rule consequence · decorator · config · migration · preview

## C. Surfaces (which exist in v1?)

- C1. Tabs (always-visible flag, ordering, hide/show)
- C2. Row injectors into existing tabs
- C3. Status strip / mini-tab in chrome
- C4. Right-pane preview surface (hovered row → text/icon/kv/actions)
- C5. Modal overlay surface (confirms)
- C6. Decorators (badges/sublabels/colors on rows)
- C7. Notification toasts (paired with action events)

## D. Query prefix mini-languages (which to claim?)

`!` shell/run · `@` windows · `#` harpoon/slots · `>` commands/actions · `=` calc/convert · `/` files/path bin · `?` help · plugin-claimed + fallback priority

## E. Filter / matching

- E1. Core scorer over plugin "match string" (default)
- E2. Opt-in `score(row, query)` for non-textual matching
- E3. Federated cross-plugin search (probably drop)

## F. Commands / aliases / collisions

- F1. Canonical id `plugin.action`; aliases user-owned
- F2. Core ships alias presets (built-ins keep `:lock` etc.)
- F3. Collisions surface as Config rows: choose winner / disable / rename
- F4. Command palette shows all matches instead of failing lookup
- F5. Subcommand args + arg completers via existing candidate preview
- F6. `confirm: bool` on actions → core modal

## G. Events

- G1. Typed event bus (not stringly): on_focus_changed, on_window_appeared, on_window_class_changed, on_workspace_changed, on_query_changed, on_tab_entered/exited, on_action_executed, on_idle_after_show
- G2. Plugin `publish(topic, payload)` for inter-plugin glue
- G3. Plugin pipelines: action → notify → history → rules → status (declared, not magical)

## H. Per-window plugin context

- H1. Plugins attach metadata to WindowId (audio sink, role, profile, rule hits, last action)
- H2. Decorators read context → badges/sublabels
- H3. Actions target hovered window even from non-Windows tabs

## I. Rules as a plugin surface

- I1. Plugins contribute predicates (window class=Zoom)
- I2. Plugins contribute consequences (route audio sink=Headset)
- I3. Selectors as plugin contribs too

## J. State / config / migration

- J1. Plugin declares config schema → Config tab auto-renders + validates
- J2. Per-plugin state file `~/.config/cofi/plugins/<id>.json`
- J3. Migration provider for state-format bumps

## K. Permissions (compiled-in)

needs_x11_read · needs_x11_write · needs_shell_launch · needs_dbus · needs_state_file · needs_hotkey_grab — docs-only now, guardrails later

## L. Scripting hooks (second-class)

- L1. Shell script actions returning JSON-line rows
- L2. Lua/Python later if pressure
- L3. Scripts use same action/row/config API; no GTK/X11 reach

## M. Lifecycle / dev

- M1. Static registry, init→register→runtime→teardown
- M2. Per-plugin enable/disable in config
- M3. Runtime `:plugin disable <id>`
- M4. Mock host for tests; record registrations; fake event bus; golden row snapshots; collision tests

## N. Manifest / introspection

- N1. Rich registry metadata (id, surfaces, actions, config, perms, version)
- N2. Hotkeys tab = registry view (proves API self-describes)
- N3. Help plugin generated from registry
- N4. About/Config shows what's enabled and why
- N5. Docs generated from manifests

## O. Existing code → plugin candidates

- Strong: apps · system_actions · path_binaries · tiling · run_mode (claims `!`)
- Maybe: Rules (once predicate/consequence API exists) · Hotkeys tab
- Stay core: filter/scoring · scroll/render/overlay · key dispatch · X11 ingestion · Names · Harpoon (uses API internally)

## P. Speculative plugin ideas

scratchpad · clipboard · calculator/converter (`=` prefix) · session layout snapshots · focus recipes (coding/meeting/writing bundles) · notification inbox (DBus) · SSH/project launcher · kill/freeze/inspect process rows · calendar/timezones in status · command palette over all actions · help plugin

## Q. Lock-first candidates (which to nail before any code?)

- Q1. Action registry shape (single vs graph) — **A1 vs A2**
- Q2. Surfaces are core, plugins ship data+intent (Rule Zero)
- Q3. Manifest metadata richness (drives UI/docs/tests)
- Q4. Typed event bus, not stringly
- Q5. Namespacing + alias collision policy
