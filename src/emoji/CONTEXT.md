# Emoji

## Purpose
Emoji provides the searchable emoji tab, ranking behavior, and emoji usage history surfaced by cofi.

## Boundary

### Owns
- The generated emoji catalog exposed as `EMOJI_TABLE`.
- Emoji provider registration, row formatting, filtering, ranking, and row identity.
- Copying the selected emoji glyph to the clipboard on Enter.
- Emoji MRU history loading, deduplication, ordering, and JSON persistence.
- The `emoji` command that surfaces the dynamic Emoji tab.

### Does Not Own
- The provider registry, command registry, or tab-surfacing mechanics.
- Generic fuzzy-match algorithm behavior outside the emoji ranking policy.
- Clipboard implementation details beyond setting the selected glyph text.
- Generating the emoji catalog source; generated files are produced by `scripts/gen_emoji.py`.
- Global config, disabled-provider policy, or daemon `--show` dispatch.

## Public Surface
- `emoji/emoji_data.h`
- `EmojiEntry`
- `EMOJI_TABLE`, `EMOJI_TABLE_LEN`, `EMOJI_COUNT`
- `emoji/emoji_provider.h`
- `emoji_provider_register()`

## Acceptance Criteria
1. `EMOJI_TABLE` exposes a non-empty generated catalog whose rows have
   non-null glyph, display name, normalized name, aliases, and keywords.
2. `emoji_provider_register()` registers a dynamic tab provider with id
   `emoji`, display name `Emoji`, hide-on-Esc modal policy, initial selection
   index `0`, and the provider callbacks needed for rows, filtering,
   identities, Enter handling, and query changes.
3. Registering the provider also registers the `emoji` command, which exits
   command mode, records the current tab as prefix origin, and surfaces the
   Emoji tab.
4. Entering the Emoji tab loads MRU history at most once, changes the entry
   placeholder to `search emoji`, and shows the full table for an empty query.
5. Empty queries return every emoji in generated table order; MRU history does
   not reorder empty-query results.
6. Non-empty queries are normalized to lowercase ASCII with combining marks
   stripped, so accented input such as `rócket` can match catalog text.
7. Every query token must match an emoji through name, alias, keyword, acronym,
   prefix, or fuzzy matching; if any token has no positive match, that emoji is
   excluded from the filtered results.
8. Result ranking preserves tier boundaries: exact alias matches outrank exact
   names, name-word matches, name prefixes, word prefixes, acronyms, and
   keyword/fuzzy-only matches.
9. MRU promotion is scoped to the current ranking zone: it may reorder results
   within the structural zone (tiers >= 1) and within the junk zone (tier 0),
   but never lifts a junk-zone match above a structural-zone match or
   resurrects a non-match.
10. Equal-score non-promoted results use MRU recency as a tiebreaker, then
    generated table order for stable output.
11. Row formatting returns two cells: the emoji glyph with width hint `2`, and
    the emoji display name with flexible width; rows are actionable and
    slottable.
12. Row identity is the emoji glyph, used to restore the selected emoji when
    leaving and returning to the tab. On each query update inside the tab,
    selection deliberately resets to the best-scoring match rather than
    tracking the previously selected glyph.
13. Pressing Enter on a valid row copies that row's glyph to the clipboard,
    moves the glyph to the front of MRU history, persists history, and hides
    cofi.
14. Pressing Enter with an invalid row index is a no-op and does not mutate
    clipboard or history.
15. MRU history is stored as `~/.config/cofi/emoji_history.json` under a
    `picks` array of glyph objects, creating the config directories when
    needed.
16. MRU history deduplicates repeated glyphs by moving the existing glyph to
    the front without increasing history count.
17. History load preserves unknown glyphs with table index `-1`, while known
    glyphs are reindexed against the current generated table.
18. Multibyte emoji, variation selectors, and ZWJ sequences round-trip through
    MRU save/load without byte changes.

## Notes
`emoji_data.c` and `emoji_data.h` are generated; edit the generator or source data instead of hand-editing table rows. Ranking tiers intentionally prevent recency from making weak keyword matches beat stronger structural matches.
