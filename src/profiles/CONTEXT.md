# Profiles

## Purpose
Profiles lets users discover Chrome browser profiles, fuzzy-filter them, launch a
selected profile, and assign or recall profiles through provider slots.

## Boundary

### Owns
- Chrome Local State profile discovery and parsing.
- Browser profile filtering, scoring, display rows, match strings, and row identity.
- Launching a selected Chrome profile through the configured browser executable.
- The optional `PROFILES` tab provider, command aliases, and profile slot payloads.

### Does Not Own
- Provider registry, command dispatch, modal lifecycle, or tab rendering mechanics.
- Harpoon slot storage persistence or global slot-key interpretation.
- Browser installation, Chrome profile creation/deletion/renaming, or non-Chrome
  browser discovery.
- Shell command parsing; profile launch is delegated as an argv array.

## Public Surface
- `profiles_provider_register()`
- `init_browser_profiles_mode()`
- `browser_profiles_parse_chrome_local_state()`
- `browser_profiles_load()`
- `browser_profiles_filter()`
- `browser_profiles_format_match_text()`
- `browser_profiles_launch()`

## Acceptance Criteria
1. Registering the provider creates an optional hidden dynamic tab with id
   `profiles`, display label `PROFILES`, hide-on-esc modal behavior, initial
   selection index `0`, and slot storage enabled.
2. The provider shortcut hint is exactly
   `Shortcuts: Enter=Open  Ctrl+key=Assign slot  Alt+key=Recall slot`.
3. The command surface registers primary command `profiles` with aliases
   `chrome`, `browser`, and `browsers`, and advertises
   `profiles, chrome [@SLOT|PROFILE]`.
4. Running the command without arguments exits command mode, records the origin
   tab for prefix return, surfaces the `PROFILES` tab, and leaves the window
   open.
5. Entering the tab sets the placeholder to `Type to filter browser profiles...`
   and reloads Chrome profiles from `$HOME/.config/google-chrome/Local State`.
6. A successful load clears `last_error` and applies an empty filter; read
   failures, empty content, parse failures, or zero parsed profiles set a
   caller-visible error string.
7. The Chrome Local State parser reads `profile.info_cache`, uses each JSON key
   as `profile_dir`, reads display name, email, and `active_time`, and falls
   back to `profile_dir` when a display name is missing.
8. Parsed Chrome entries are capped by the caller's maximum, tagged as
   `chrome` / `Chrome` / `google-chrome`, sorted by newest `active_time` first,
   and tie-broken by case-insensitive name.
9. An empty filter includes all loaded profiles in their current order.
10. A non-empty filter fuzzy-matches name, email, profile directory,
   name+email, email domain+name, marker `gc`, browser name, and browser id,
   then ranks by descending score and original index.
11. Name and structured identity matches outrank weaker backend marker matches,
   while the `gc` marker can still surface all Chrome profiles.
12. With no filtered profiles, the provider exposes one non-actionable status
   row containing `last_error` when present or `No matching browser profiles
   found` otherwise.
13. Profile rows render four cells: `[gc]`, profile name, email, and profile
   directory, and are both actionable and slottable.
14. Match text includes `[gc]`, Chrome identifiers, executable, profile name,
   email, and profile directory; row identity is `chrome:<profile_dir>`.
15. Query changes refilter the loaded profile list and reset provider selection
   to the best current match.
16. Pressing Enter on a profile launches it and hides on success, returns an
   action error on launch failure, and is a no-op when no valid row is selected.
17. Launch resolves `google-chrome`, falls back to `google-chrome-stable`, builds
   argv as browser path plus `--profile-directory=<profile_dir>`, and delegates
   process creation to detached argv launch.
18. Slot payloads for Chrome profiles are `profile:chrome:<profile_dir>`;
   invalid rows or unsupported backends do not produce a payload.
19. Slot recall reconstructs a minimal Chrome profile from the payload and
   launches it; malformed or unsupported payloads return an action error.
20. Command argument `@SLOT` resolves the payload from the `profiles` slot
   namespace and recalls it; missing slot payloads return an action error.
21. Non-slot command arguments reload and filter profiles, launch the first
   matching profile, and hide on success.
22. When command arguments find no matching profile or launch fails, the command
   output shows `No matching browser profile.` and the window remains open.

## Notes
- Chrome is the only supported backend today. Adding another browser requires
  extending backend identity, discovery, launch argv construction, row display,
  and slot payload handling together.
- Slot payloads persist only backend and profile directory; display name and
  email are rediscovered or reconstructed later.
- Browser launches are argv-based and do not pass through a shell.
