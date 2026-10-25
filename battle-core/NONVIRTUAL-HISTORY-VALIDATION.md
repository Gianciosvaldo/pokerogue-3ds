# Last nonvirtual move history

Pinned upstream: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`.

`Pokemon.getLastNonVirtualMove` searches the entire summon history, skipping
NONE, optionally Struggle, and virtual entries except optionally FOLLOW_UP.
It does not require a successful move result. The core formerly reported
unsupported when its eight retained entries hid the answer. The initial native
capture exposed 2,070 mismatches/unsupported results in 15,552 selector reads.

Four semantic query results now preserve each selector's complete answer while
keeping the eight-entry tail unchanged. Known absence is separate from an
unknown missing prefix. A new matching entry makes that selector known even
when older history is unavailable. Reset clears both history and summaries.
Results preserve the ported TurnMove fields: move, use mode, result (including
undefined), target count and ordered targets. Native `turn` is outside the
existing C TurnMove projection and is not claimed by this proof.

The move engine's shared target-list owner also updates selected cached entries
when their shared array changes, including after eviction from the retained
history. Its identity counter no longer uses the saturated 16-bit tail count.
A summon epoch separates old references from newly reset histories.
The 32-bit serial covers JavaScript's array-length domain; it is not an assertion
of arbitrary histories beyond native array capacity. Existing phase and alias
capacities are unchanged and remain separate completion concerns.

## Source observations

`tests/nonvirtual-history.producer.test.ts` runs actual pinned
`Pokemon.pushMoveHistory`, all four native query variants, and the actual
Disable/Mimic/Sketch/Spite/Instruct conditions. Native battle RNG is captured
before GameManager construction and restored after natural battle startup.
No production history, query, RNG or condition method is replaced. Actors are
on-field; the existing `pr_push_history_shared` owner retains its field guard.
The low-level C history append helper is not newly exposed as an off-field owner.

The retained corpus has 972 histories: nine lengths (0, 1, 7, 8, 9, 64, 65,535,
65,536, 65,537), six use modes, three first moves (NONE, Tackle, Struggle), and
six suffix patterns. Six stages observe the initial prefix, a new real move,
that move after nine virtual entries, reset, a new Follow Up, and that Follow Up
after nine NONE entries. This yields 52,488 native selector/condition results.
Twelve additional native shared-array histories provide 336 selector results,
including target mutation before/after eviction and after 16-bit saturation.
Old target arrays cannot mutate entries in a new summon after reset.
C compares exact projected fields, conditions and unchanged RNG. These are
source-defined boundaries and representative histories, not exhaustive mixed
battle-state coverage.

`tools/nonvirtual-history-diff.sh /path/to/pinned/upstream` verifies source pins
and clean production trees, reruns the producer, transports actual native
records, requires byte-identical retained observations, and runs both C owners.
The transport is strict about counts, order, result/target ranges and EOF.

## Save and failure evidence

The optional named `<party>_last_moves` section carries selector knowledge,
results and target-list identity. The live alias epoch is rebuilt with the engine rather than persisted
references. Existing fields retain their names and widths.
Older saves without it reconstruct positive answers from their known tail and
retain unknown prefixes. New saves retain exact answers through 5,832
query/condition roundtrips; all three run RNG streams are checked. Schema tests
reject duplicate sections/fields, wrong widths/counts/masks and invalid selected
TurnMoves. Partial legacy state can be saved and resumed again.

The targeted comparison passes 52,488 selector/condition observations plus
336 shared-target observations with zero mismatches/uncovered results. The
codec harness passes 161,856 checks across 5,832 native roundtrips. All 15 compiled
faults are caught and all 11 malformed captures are rejected.

The direct guard suite covers unknown raw prefixes, partial query knowledge,
short raw histories, resets, count saturation and exact selected results.
Compiled controls deliberately break aged selection, flag separation, Follow Up,
Struggle, result filtering, target/result retention, shared aliases and their
identity width, reset, legacy knowledge, saved data and mask validation.
The first fault run exposed a missing aged-new-entry test; the six-stage native
capture was added to catch that fault rather than weakening the control.

Normal retained game-core/app/custom/save/football tests and actual ARM build
and stack checks remain required. Only dedicated Task63 is skipped. No Windows
builder or release is made. This does not complete Protect streaks, arbitrary
history reads, faint/revive histories, remaining callbacks or wide Endless.
