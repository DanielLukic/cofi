# 0013 — One match entry per rule (1:1, no pattern dedup)

**Status:** Accepted
**Date:** 2026-05-29
**Supersedes:** none

## Context

Rules map title-glob patterns to command sequences and fire on window-open and
title-change events. Each rule needs a stable `match_id` to anchor per-rule,
per-window transition state (`once`, `new_only`, applied-window bookkeeping).

The prior implementation used `matching_find_or_create_pattern_entry`, which
returned the existing match entry when a rule with the same pattern already
existed. This meant two rules with identical patterns shared one `match_id`.
The shared entry was the last remaining cross-subsystem sharing path in the
matching store, and the root cause of a class of dangling-reference bugs: when
one of the sharing rules was deleted, the entry's lifetime became ambiguous —
neither owner was solely responsible for deletion.

The change (`caeb4cc`) replaced the find-or-create path with create-only
`matching_create_pattern_entry`. Three call sites were repointed. The old
function was deleted. Startup now persists the repaired state immediately so
orphan-repair entries are not re-leaked on subsequent loads.

## Decision

Each rule owns exactly one match entry; no match entry is shared across
rules. The rule's `match_id` is unique to that rule. Two rules with identical
patterns produce two distinct match entries with distinct IDs.

Owner-delete (see ADR 0012) applies: when a rule is removed, its match entry
is deleted. The 1:1 invariant makes ownership unambiguous at every point.

## Consequences

- **Dangling-reference class eliminated structurally.** With no shared
  entries, deleting one rule can never affect another rule's identity binding.

- **Aligns with ADR 0012.** 1:1 ownership is a prerequisite for
  owner-delete; a shared entry cannot have a single unambiguous owner.

- **Transition state is cleanly isolated.** `once`/`new_only`/`applied`
  state is bound to a `match_id` unique to the rule. Two rules with the same
  pattern independently track whether each has fired for each window.

- **Slight storage overhead.** Duplicate patterns in `matching.json` are
  explicitly accepted as the cost of clean ownership. The data volume is
  negligible for any realistic rule count.

- **Legacy migration path.** On load, rules with shared/missing match IDs
  get new entries assigned. The repaired IDs are persisted immediately so the
  repair does not re-run and re-allocate on every startup.

## Alternatives considered

- **Dedup by pattern (find-or-create).** Rejected because shared ownership
  made deletion ambiguous: removing one rule left the shared entry live for
  the other, but the remaining owner had no way to know the entry was now
  exclusively its own. This produced dangling references across rule/entry
  lifecycles.

- **Anonymous pattern matching without match entries.** Rejected because
  rules require cross-restart state (`once` fired, window rebinding). Without
  a persistent `match_id`, per-rule per-window transition state cannot survive
  a process restart.
