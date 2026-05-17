# Architecture Decision Records

Substantive design decisions for cofi. Lightweight ADRs — not strict Nygard format, but each entry captures *what was decided, why, and what alternatives were considered*.

Numbered so they sort. Add new ones; never edit accepted ones — supersede them with a new ADR that references the old.

## Index

| # | Title | Status |
|---|---|---|
| 0001 | [Plugin API design](0001-plugin-api.md) | Proposed |
| 0002 | [Plugin ideas / candidate features](0002-plugin-ideas.md) | Superseded by 0004 |
| 0003 | [Plugin shape evaluation](0003-plugin-shape-evaluation.md) | Superseded by 0001/0004 |
| 0004 | [Plugin architecture plan](0004-plugin-architecture-plan.md) | Draft |

## Template

```markdown
# NNNN — Title

**Status:** Draft | Proposed | Accepted | Superseded by ADR-XXXX
**Date:** YYYY-MM-DD

## Context
What's the situation, what forces are at play, what constraints matter.

## Decision
What we are going to do.

## Consequences
What becomes easier, what becomes harder, what risks we accept.

## Alternatives considered
Other options and why they were rejected.
```
