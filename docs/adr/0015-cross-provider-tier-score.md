# 0015 — Cross-provider tier-based scoring via `tier_score_string`

**Status:** Accepted
**Date:** 2026-06-16
**Supersedes:** none

## Context

Provider tabs had drifted into per-feature ranking strategies. Windows used a
two-tier scorer with a hard separation between direct word-boundary matches and
indirect fuzzy matches. Bookmarks still used field-weighted boosts. Apps kept an
older local field-tier approach, while Emoji layered custom ranking and MRU.

The Bookmarks investigation behind TFD-807 exposed the practical divergence:
typing a profile name that appeared verbatim in a bookmark row could still lose
to a scattered fuzzy match in another row's title-like fields because the
bookmark scorer optimized field boosts rather than rank tiers.

The useful part of the Windows scorer after display-string assembly was already
string-only. The drift came from where that logic lived: inside
`src/ui/window_filter.c::match_window`, which made reuse awkward and invited new
per-provider scorers.

## Decision

Extract the string-only direct-vs-indirect tier logic from
`src/ui/window_filter.c::match_window` into the public cross-provider scorer
`src/matching/tier_score.{c,h}` as `tier_score_string(const char *filter, const
char *display)`.

Consumers migrate incrementally. Windows continues to wrap
`tier_score_string()` and adds its window-only Signal A bonus afterward.
Bookmarks use `tier_score_string()` directly over their combined match-string
corpus.

## Consequences

- There is now one place to tune `TIER_DIRECT_BASE` and Signal B behavior.
- Bookmarks ranking now follows the same direct-word-boundary guarantees as the
  Windows tab.
- Other providers such as Apps, Emoji, and Projects can migrate in separate
  tickets without rewriting the scorer again.
- Corpus ranking tests now have wider blast radius because changing
  `tier_score_string()` affects more than the Windows tab.

## Alternatives considered

- **Keep per-provider field boosts.** Rejected because the Bookmarks symptom
  recurs in Apps-like providers and every tab would need separate tuning.
- **Put the scorer into `fzf_algo`.** Rejected because the tier logic is not
  pure fzf scoring; it layers higher-level rank tiers above the fuzzy matcher.
- **Duplicate the tier logic in each consumer.** Rejected because it multiplies
  the tuning surface and invites drift between tabs.
