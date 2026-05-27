# CLI

## Purpose
The CLI subsystem translates process arguments into startup behavior for the
cofi binary. It handles user-facing help/version output, logging flags, display
overrides, and daemon-delegation requests before the main application starts.

## Boundary

### Owns
- Parsing `argv` for supported cofi command-line flags.
- Printing usage, version, and command-help text to stdout or parse errors to
  stderr.
- Translating CLI log-level and alignment strings into internal enum values.
- Populating the startup fields on `AppData` that later startup code consumes.

### Does Not Own
- The daemon socket protocol or delivery of delegated requests.
- Command registry behavior beyond formatting command help for CLI output.
- Runtime config loading or persistence of CLI overrides.
- GTK window creation, provider activation, or tab rendering.

## Public Surface
- `print_usage` writes the human-readable CLI usage text.
- `print_command_mode_help` writes command-mode help in CLI format.
- `parse_log_level` maps CLI log-level names to logger constants.
- `parse_alignment` maps CLI alignment names to `WindowAlignment`.
- `parse_command_line` parses argv and updates output parameters plus `AppData`.

## Acceptance Criteria
1. `--help` and `-h` print usage text and return exit code `3`.
2. `--help-commands` and `-H` print command-mode help and return exit code `4`.
3. `--version` and `-v` print the version string, include the git hash when
   compiled in, and return exit code `2`.
4. Unknown options print an error plus usage text and return exit code `1`.
5. Parser exceptions from the option parser print an error plus usage text and
   return exit code `1`.
6. `--log-level` and `-l` accept `trace`, `debug`, `info`, `warn`, `error`, and
   `fatal` case-insensitively, set the process log level, and mark the log-level
   output flag when provided.
7. Invalid log levels print an error plus usage text and return exit code `1`.
8. `--log-file` and `-f` duplicate the supplied path into the caller-owned
   `log_file` output pointer.
9. `--no-log` and `-n` set the caller's `log_enabled` output flag to false.
10. `--align` and `-a` set `app->config.alignment` from the supplied alignment
   string and mark the alignment-specified output flag.
11. Unknown alignment strings fall back to `ALIGN_CENTER`.
12. `--no-auto-close` and `-C` set `app->config.close_on_focus_loss` to false
   and mark the close-on-focus-loss output flag.
13. `--windows` and `-W` delegate startup to the Windows tab opcode.
14. `--workspaces` and `-w` delegate startup to the Workspaces tab opcode.
15. `--harpoon` delegates startup to the Harpoon tab opcode.
16. `--matching` and `--names` delegate startup to the Matching tab opcode.
17. `--show NAME` delegates startup through the generic show-tab opcode and
   copies `NAME` into `app->startup_delegate_tab_name`.
18. `--command` and `-c` delegate startup to command mode and set
   `app->start_in_command_mode`.
19. `--run` delegates startup to run mode and set `app->start_in_run_mode`.
20. `--applications` delegates startup to the Applications tab opcode and sets
   `app->apps_mode` to `APPS_MODE_DEFAULT`.
21. `--assign-slots` sets `app->assign_slots_and_exit` so startup can perform
   workspace slot assignment and terminate.
22. When multiple delegate flags are present, the parser applies them in source
   order and the later recognized delegate flag wins.
23. Successful parsing returns exit code `0` without printing usage text.

## Notes
The nonzero return codes for help, command help, and version are intentional
sentinel values consumed by the app entrypoint; they are not parse failures.

`cli_args.cpp` is the only C++ translation unit in cofi. It uses `popl.hpp`
and wraps C headers in `extern "C"` blocks, so building cofi requires a C++
compiler for this one file. TFD-830 tracks dropping `popl` and converting this
subsystem to C.

`parse_command_line()` writes these `AppData` fields as its startup output
surface: `startup_delegate_opcode`, `startup_delegate_tab_name`,
`start_in_command_mode`, `start_in_run_mode`, `current_tab`, `apps_mode`,
`config.alignment`, `config.close_on_focus_loss`, and
`assign_slots_and_exit`.

`print_command_mode_help()` may reset and re-register the command registry via
`cofi_command_registry_reset()` and `cofi_register_builtin_plugins()` when no
commands are loaded. This intentional standalone `--help-commands` path mutates
global command state.
