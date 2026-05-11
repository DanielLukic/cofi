# Contributing to cofi

Thanks for your interest. cofi is a small C/GTK3 window switcher; this guide covers the contributor surface. Agent-facing rules live in [CLAUDE.md](CLAUDE.md) and product behavior in [SPEC.md](SPEC.md).

## Quick start

```bash
make clean && make      # build
make test               # 70 test binaries
./restart.sh            # rebuild + restart systemd user service
bash scripts/install-hooks.sh   # one-time: install pre-push hook
```

See [README.md](README.md) for runtime usage and [docs/glossary.md](docs/glossary.md) for domain terms.

## Branches

- `main` — stable, updated via PR from `develop`
- `develop` — default target for all PRs
- `feat/*`, `fix/*`, `chore/*`, `docs/*` — short-lived topic branches off `develop`

Branch from `develop`, never from `main`. Never push directly to `develop`, `main`, or `release`.

## Tests

- Add or update tests for behavior changes. The runner is `make test`; entries live in `Makefile` and `test/*.c`.
- For refactors, write behavioral tests against current code first; they must still pass after.
- If a UI/X11 path is hard to cover, say so in the PR — don't skip silently.

## Commit messages

Imperative mood. Conventional-style type prefix when useful:

```
feat: add jump-slot command for per-workspace slot activation
fix: recompute window size + recenter on each show
docs: add glossary
chore: ignore .gitnexus/ index dir
```

Body explains *why*, not *what* — well-named code already shows the what.

## Pull requests

1. Run `make test` locally (the pre-push hook will run it again).
2. Push the branch — GitHub Actions runs the `Build` workflow (compile + `make test`) on every push.
3. Open a PR targeting `develop`. Include a short summary and a test plan.
4. Wait for the maintainer to review and merge. Don't self-merge.

## Issues

Use the GitHub issue tracker for bugs and feature requests. Reference Linear ticket IDs (`TFD-NN`) in PRs when relevant; the maintainer manages the Linear board.

## Code style

See `CLAUDE.md` → **Design Principles** and **Coding Guidelines**. Highlights:

- `snake_case` for functions and variables
- Small functions (10–15 lines, max 30)
- One module = one responsibility
- Logging via `log_*` helpers, never `printf`
- `make clean && make` after header changes (no auto header deps yet)
