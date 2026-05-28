# Harpoon

## Purpose
Harpoon gives users stable keyboard slots for returning to important windows
and provider-backed rows, plus a separate visible-window slot mode for quickly
activating windows on the current workspace.

## Boundary

### Owns
- Window harpoon slots keyed by digits and letters, stored as `MatchEntry`
  references that can rebind when a window closes and reopens.
- Slot assignment, unassignment, recall, filtering, row formatting, delete
  confirmation, pattern-edit entrypoints, and the hidden Harpoon provider tab.
- Shared provider-slot key handling through `SlotStore` when the active provider
  opts into `slot_store_enabled`.
- Per-workspace visible-window slot assignment for digit recall, including
  occlusion-aware candidate filtering and overlay indicator placement.
- Workspace jump/move overlay content and key handling that uses x11 workspace
  helpers to switch or move selected windows.

### Does Not Own
- The generic `SlotStore` data structure, file format, parser, or save/load
  mechanics beyond choosing Harpoon/provider payloads to store in it.
- Matching entry creation semantics, matching garbage collection policy,
  filtering algorithms, or custom-name/pattern-edit behavior outside slot use.
- X11 activation, desktop switching, geometry queries, monitor/workarea data, or
  highlight rendering; Harpoon calls those lower-level services.
- Provider-specific meaning of non-window slot payloads; providers own payload
  construction and recall behavior.

## Public Surface
- `HarpoonSlot`, `HarpoonManager`, `init_harpoon_manager()`,
  `assign_window_to_slot()`, `unassign_slot()`, `get_window_slot()`,
  `get_slot_window()`, `is_slot_assigned()`, `harpoon_tab_id()`,
  `serialize_window_slot_payload()`, and `deserialize_window_slot_payload()`
- `save_harpoon_slots()` and `load_harpoon_slots()`
- `harpoon_provider_register()`, `harpoon_tab_mode()`, `filter_harpoon()`,
  `harpoon_selected_slot()`, and `handle_harpoon_tab_keys()`
- `handle_harpoon_assignment()`, `handle_harpoon_workspace_switching()`, and
  `harpoon_assign_or_toggle_window()`
- `WorkspaceSlot`, `WorkspaceSlotManager`, `init_workspace_slots()`,
  `assign_workspace_slots()`, and `get_workspace_slot_window()`
- Harpoon delete and workspace overlay content/key handlers declared in
  `overlay_harpoon.h` and `workspace_overlay.h`

## Acceptance Criteria
1. Initializing a `HarpoonManager` clears every window slot, initializes the
   shared slot store, and leaves matching/window-list pointers unset until app
   setup wires them.
2. Assigning a window slot captures or reuses a `MatchEntry`, stores its
   positive `match_id`, marks the slot assigned, and mirrors the payload into
   the shared slot store under the `windows` tab id.
3. Assigning a window already represented in another Harpoon slot clears the old
   slot before assigning the new one, so one window identity has at most one
   active Harpoon slot.
4. Pressing the same assignment shortcut on a slot already matching the selected
   window toggles that slot off, runs matching garbage collection, and persists
   both matching entries and Harpoon slots.
5. Window slot recall returns the live bound X11 window when available; if the
   binding is stale, it reassigns live windows through the matching manager
   before deciding whether the slot can be activated.
6. Closed windows do not delete assigned Harpoon slots by themselves. Slots keep
   their `match_id` so matching can rebind them when a matching window returns.
7. Loading Harpoon slots hydrates only `windows` tab payloads into
   `HarpoonManager.slots`; other provider payloads remain in the shared slot
   store and are not treated as window Harpoon slots.
8. Saving Harpoon slots synchronizes assigned and cleared window slots into the
   shared slot store before writing it to disk.
9. `Ctrl+<slot>` assigns or clears provider slots when the active provider
   exposes `slot_payload_for`; otherwise it assigns window slots only from the
   Windows tab and only when a filtered window is selected.
10. Assignment shortcut parsing accepts digits, keypad digits, and letters;
    unshifted `j`, `k`, and `u` are reserved from assignment so navigation and
    related shortcuts can fall through.
11. `Alt+<slot>` recalls provider slots through the active provider when it
    exposes `slot_recall`; missing provider payloads are handled as a consumed
    no-op with a warning.
12. In default digit mode, `Alt+<letter-or-digit>` from the Windows tab recalls
    assigned Harpoon window slots, activates the resolved window, highlights it,
    sets workspace-switch state, and hides cofi.
13. In workspace digit mode, `Alt+1` through `Alt+9` switch directly to the
    corresponding zero-based workspace when that workspace exists.
14. In per-workspace digit mode, `Alt+1` through `Alt+9` only recall visible
    workspace slots when the active tab is `TAB_WINDOWS`; on other tabs the
    digit falls through to the plain workspace-switch branch. Recall first
    calls `assign_workspace_slots(app)`, then `get_workspace_slot_window()`.
15. Workspace slot assignment considers windows on the current desktop plus
    normal sticky windows, excluding cofi's own window, hidden/shaded windows,
    missing geometry, mostly occluded windows, tiny visible fragments, and
    candidates outside monitor/workarea clips.
16. Workspace slots are sorted row-first by default or column-first when
    configured, assigned densely from `1` through `9`, capped at nine visible
    windows, and never mutate or save digit-slot configuration.
17. Workspace slot overlay positions use the centroid of the largest visible
    window fragment when occlusion data is available, falling back to window
    center coordinates for geometry candidates.
18. The Harpoon provider is hidden by default, registers the `harpoon`/`hp`
    command, filters assigned slots by slot key/title/class/instance, shows one
    non-actionable empty row when no slots match, and resets selection on query
    changes.
19. Harpoon provider rows expose slot key, original title, bound class,
    instance, and type; row identity is the actual slot index, not filtered row
    position.
20. In the Harpoon tab, `Ctrl+D` opens delete confirmation for the selected
    actual slot and `Ctrl+P` opens pattern editing with `Slot: <key>` context.
21. Confirming a Harpoon delete clears the selected slot, runs matching garbage
    collection, saves Harpoon slots, logs the user action, rebuilds filtered
    Harpoon rows without a query reset, restores selection, and refreshes the
    display.
22. Workspace jump/move overlays render limited workspace grids using current
    workspace names, one-based labels, and visual markers for the user's current
    workspace and the selected window's workspace.
23. Workspace overlay key handling resolves digit, arrow, and HJKL navigation
    through x11 workspace helpers; invalid keys fall through, successful actions
    hide cofi, and move-all switches to the target workspace after moving each
    queued window.

## Notes
- Harpoon window slots and per-workspace digit slots are different concepts:
  window Harpoon slots persist by `MatchEntry`, while workspace digit slots are
  recomputed from visible windows on the current workspace.
- Provider slots share the Harpoon-backed `SlotStore`, but the provider owns the
  payload semantics and recall behavior.
- The digit `0` is a valid Harpoon slot key for assignment/recall, but direct
  workspace and per-workspace digit modes only use `1` through `9`.
