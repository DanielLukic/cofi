# Sinks

## Purpose
Sinks provides the audio-sink provider surface and slot-enabled sink recall behavior.

## Boundary

### Owns
- Audio sink inventory parsing from `pactl get-default-sink` and `pactl list sinks` output.
- Async refresh and set-default-sink subprocess orchestration for the SINKS tab.
- Sink filtering, default-sink marking, row formatting, row identity, and slot payloads.
- Slot-enabled sink recall through provider shortcuts and `sinks @SLOT` command arguments.
- The `sinks` command and `sink` alias that surface or directly activate audio sinks.

### Does Not Own
- PulseAudio/PipeWire policy, `pactl` behavior, or OS audio routing beyond invoking `pactl set-default-sink`.
- The generic provider registry, command registry, tab switching, slot-store implementation, or modal infrastructure.
- Global key handling for slot assignment/recall beyond exposing provider slot callbacks.
- Persistence of sink inventory; sinks are refreshed from the live audio system and are not saved across cofi restarts.
- Window hiding, display repainting, or textbuffer mechanics beyond requesting them after SINKS actions.

## Public Surface
- `sinks/sinks.h`
- `SinkEntry`, `SinksMode`
- `MAX_SINKS`, `MAX_SINK_NAME_LEN`, `MAX_SINK_DESC_LEN`
- `init_sinks_mode()`
- `sinks_refresh_async()`, `sinks_filter()`, `sinks_switch_name()`
- `sinks/sinks_provider.h`
- `sinks_provider_register()`

## Acceptance Criteria
1. `sinks_provider_register()` registers an optional hidden-by-default dynamic
   tab provider with id `sinks`, display name `SINKS`, hide-on-Esc modal
   policy, slot storage enabled, 1500 ms tick refresh, and SINKS row, query,
   lifecycle, activation, slot, and command callbacks.
2. Registering the provider also registers the `sinks` command and `sink`
   alias with help format `sinks, sink [@SLOT|SINK]`.
3. Invoking `sinks` without arguments exits command mode, records the current
   tab as prefix origin, surfaces the SINKS tab, and does not hide cofi.
4. Invoking `sinks @SLOT` looks up the slot payload in the `sinks` namespace,
   attempts to activate that sink name, and hides cofi only on successful
   activation.
5. Invoking `sinks SINK` searches loaded sink names and descriptions by
   substring, activates the first matching sink name, and hides cofi only on
   successful activation.
6. Invalid command arguments leave cofi visible, set command help state, and
   show `No matching sink or sink slot.` in the text buffer.
7. Entering the SINKS tab sets the entry placeholder to `Audio sinks...` and
   starts an async refresh.
8. Provider ticks start an async refresh; a refresh request is ignored while a
   previous refresh is still in flight.
9. Refresh invokes `pactl get-default-sink`, a separator line, and `pactl list
   sinks`; malformed output, subprocess spawn failure, or non-zero `pactl`
   exit status sets a user-visible error and refreshes display.
10. Sink parsing preserves `pactl list sinks` order, accepts sink names with
    spaces or punctuation, preserves Unicode descriptions, and marks the sink
    whose name equals the trimmed default-sink output.
11. A sink without a description uses its name as the display description.
12. Empty parsed inventory reports `No sinks found`.
13. Refresh snapshots include sink names and default flags only; description
    changes alone do not force repaint, but default-sink changes do.
14. Refreshes preserve the selected sink by sink name when it remains visible;
    otherwise selection and provider scroll reset to the first row.
15. Filtering with an empty query shows every loaded sink in parsed order.
16. Non-empty filtering matches against the sink description and sink name
    combined through the shared match predicate.
17. Row count returns one placeholder/error/no-results row when there is an
    error, no inventory, or no filtered match; otherwise it returns the
    filtered sink count.
18. Normal sink rows render two cells: a three-character default marker
    (`[*]` for default, `[ ]` otherwise) and the sink description.
19. Sink rows are actionable and slottable; placeholder, error, and no-match
    rows are not actionable.
20. Row match text is the sink name, row identity is the sink description, and
    slot payload is the sink name.
21. Pressing Enter on a valid sink row runs `pactl set-default-sink <name>`;
    invalid rows are no-op.
22. Successful set-default completion triggers a refresh and hides cofi.
23. Failed set-default spawn or completion records the subprocess error (or a
    fallback pactl error), refreshes display, and does not hide cofi.
24. Slot recall uses the stored sink-name payload and follows the same
    activation success/error behavior as pressing Enter.

## Notes
SINKS depends on `pactl`; systems without a compatible PulseAudio/PipeWire `pactl` command should surface an error row rather than stale sink data.
