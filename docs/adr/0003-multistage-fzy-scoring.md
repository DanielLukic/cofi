# 0003 — Multi-stage fzy scoring with bonus heuristics

**Status:** Superseded by [0004](0004-fzf-fuzzymatchv2-full-row.md)
**Date:** 2025-06-24 (initial port `7a37de7`); tuned through 2025-07

## Context

Ported from gofi (Go predecessor). Used fzy's `has_match` / `match_bonus` with an explicit stage cascade — word-boundary → initials → subsequence → fuzzy — plus consecutive-word and anchor-position bonuses (`c48647c`, 2025-06-26) and a current-workspace bonus (`8571247`, 2025-07-26). Each stage scored against window title and class separately, with results merged.

## Decision

Multi-pass scoring on per-field targets, with stage-specific bonus tables.

## Consequences

- Predictable for short queries that aligned with one of the stages.
- Hard to reason about for multi-word queries: which stage a query landed in depended on the input shape, not the user's intent.
- ~170 lines of stage-management code that grew with every new heuristic.
