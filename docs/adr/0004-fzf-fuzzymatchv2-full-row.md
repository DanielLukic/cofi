# 0004 — Single-pass fzf FuzzyMatchV2 over the full display row

**Status:** Accepted
**Date:** 2026-03-26 (`bbe6f56` full-row target, then `8725f1d` fzf v2)
**Supersedes:** [0003](0003-multistage-fzy-scoring.md)

## Context

The multi-stage fzy regime (ADR-0003) had grown bonus heuristics for word-boundary, initials, workspace, special-window pinning. Behavior was hard to predict for multi-word queries and the stage code was ~170 LOC of branching.

## Decision

1. Score against the **full display row** (desktop + instance + title + class), not per-field. What the user sees is what scoring sees.
2. Replace fzy with **fzf's FuzzyMatchV2 algorithm** (`src/fzf_algo.c`, 378 LOC). Single pass. Position bonuses are part of the algorithm, not bolted on.

Workspace-priority bonus and special-window pinning remain as small post-score adjustments.

## Consequences

- Display order = search order (cross-cutting invariant — see [architecture.md](../architecture.md)).
- Behavior matches user mental model from fzf/telescope.
- ~170 lines of dead multi-stage code removed (`dc7ef41`, 2026-03-28).
- `SPEC.md` § Search still documents the legacy multi-stage stage list — drift; flagged for cleanup.

## Alternatives considered

- Keep fzy, fix full-row target only (`bbe6f56`): adopted for two days, then dropped in favor of fzf v2 — fzy's gap penalties were too punitive on multi-field rows.
- Skim (`skim-rs`): C++/Rust dependency, rejected.
