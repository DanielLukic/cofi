# COFI - C/GTK Window Switcher

Intent: this file is the contributor workflow and agent operating contract. It is not
a session log, subsystem inventory, or source-file index.

See also:
- `SPEC.md` for intended product behavior.
- `docs/architecture.md` for process model, layers, subsystem map, and build/test shape.
- `docs/gotchas.md` for fragile implementation invariants and regressions to avoid.
- `docs/adr/` for accepted and superseded design decisions.

## Development Workflow

### Branches

- `main` - stable releases, updated via PR from `develop`.
- `develop` - active development, all work merges here first.
- `fix/*` / `feat/*` - short-lived PR branches off `develop`.

### Hard Rules

- NEVER push to `develop`, `main`, or `release` without explicit user consent.
- NEVER create merge commits - always rebase for clean history.
- NEVER merge PRs that change app logic without user testing first.

### Normal Change Flow

1. Branch from `develop`.
2. Add or update tests for behavior changes when feasible.
3. Make the change.
4. Run the relevant tests (`mise run test` unless a narrower subset is clearly enough).
5. Rebuild and restart cofi for behavior changes so the user can verify the running app.
6. Wait for user verification before committing unless the user explicitly asks for an earlier checkpoint.
7. Push the branch and create a PR targeting `develop`.
8. Wait for user approval before merging.

### Subagent Worktrees

- Instruct agents to branch from `develop`.
- Use `isolation: "worktree"` for parallel work.
- Always place worktrees under `.worktrees/` inside this repo:
  ```bash
  git worktree add .worktrees/<branch-slug> -b <branch-name>
  ```
- `.worktrees/` is ignored and local only.
- Review agent diffs before merging; agents may have started from the wrong base.

## Build, Test, Run

```bash
mise run build                # incremental build
mise run rebuild              # full rebuild; required after header changes
mise run test                 # unit/regression tests
mise run test-target test_fzf_algo # targeted test binary
mise run test-integration      # opt-in Xvfb GUI screenshot tests
mise run install              # copy release binary + install user service
mise run install-dev          # symlink ~/.local/bin/cofi to this worktree + install service
mise run restart              # rebuild and restart, preserving dev symlink installs
systemctl --user restart cofi # restart without rebuild
journalctl --user -u cofi -f  # tail logs
```

## Tracking

- Linear entry point: `./.claude/skills/linear/bin/linear`.
- Team/project defaults come from `.linear.conf`.
- Status flow: Backlog -> Todo -> In Progress -> In Review -> Done.
- Use issue IDs like `TFD-82` in branches, commits, PRs, and discussion.

## Engineering Standards

These apply to every change.

- Separation of concerns: each module should own one clear responsibility.
- High cohesion, low coupling: prefer clean APIs over globals, naked `extern`, or cross-module reach-through.
- Boy Scout Rule: leave touched code cleaner than you found it.
- Broken Windows: fix design damage you touch, or file a ticket instead of normalizing it.
- File size is a signal: when a file has reached its natural limit, split as part of the change that grows it.
- Write small functions: aim for 10-15 lines, max 30 unless there is a strong reason.
- Use `snake_case` for functions and variables.
- Work in small commits with clear messages.
- Write tests for essential behavior and user-visible changes where the code structure allows it.
- Use logging instead of `printf`/ad hoc prints.

### Test Discipline

- Feature work: prefer a failing test first, then implement.
- Refactoring: first add or verify behavioral tests that pass on current code, then refactor with those tests still passing.
- Structural/wiring tests are not enough for refactors; cover behavior that would break if the refactor were wrong.
- If targeted coverage is not practical, call out the gap explicitly.

## Reporting

When handing work back, state:

- which tests were run;
- whether cofi was restarted;
- whether the change is committed or intentionally left uncommitted.

## Architecture Constraints

- Window management uses direct Xlib/EWMH, not `wmctrl` or shelling out.
- Window-list updates are event-driven from `PropertyNotify`; do not add polling casually.
- cofi is a single binary with daemon mode plus Unix-socket delegation for later invocations.
- Global hotkeys use X11 grabs; test hotkey behavior against X11 focus/grab semantics.
