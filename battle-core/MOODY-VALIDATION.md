# Moody: actual post-turn stat choices (2026-10-11)

Base main `968c5bac2e055dea3f17262de0dd1ca91a7806d1`. Source and locales
remain pinned at `8555c08c823b856cbec4eb99ca84ea52a955836d` and
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`; Node 22.23.3, GCC 14.2.0.

The existing TurnEndPhase caller reached MoodyAbAttr, but its leaf returned
unsupported. The new leaf follows `ab-attrs.ts:4308` exactly: EFFECTIVE_STATS
order ATK/DEF/SPATK/SPDEF/SPD; raise candidates below +6, lower candidates above
-6; battle-RNG choice for raise, remove that chosen stat from lower candidates,
queue +2, then choose/queue -1. Candidate lists use the pre-queued stage values.
An empty list consumes no draw; a one-element list still calls the source RNG
API. Both changes go through the existing stat phase, ability display, passive,
Simple/Contrary and clamping machinery. Simulation returns before RNG/queue
work. Moody has no Speed Boost switched-in/failed-run-away refusal.

## Independent source comparisons

The additive builder derives a new source producer from the unchanged Speed
Boost turn scaffold. It supplies new controlled inputs and delegates the actual
Moody canApply/apply methods, TurnEndPhase, RNG and StatStageChangePhase. It does
not supply a replacement Moody formula or C-generated expectations. Complete
Pokemon/party state, raw/effective stats, stages, turn counters, moves/PP/history,
field/arena/commands, phase trace, pending queue, both RNG streams and rendered
texts are compared using the unchanged existing effective-stat replay.

Twenty loaded scenarios on both sides include ordinary stats, all +6/all -6,
mixed boundaries, one raise/lower candidate, suppressed/no ability/fainted/
KO-before-turn-end controls, primary/passive combinations with Simple/Contrary,
both holders, turn count zero, switched-in/failed-run-away flags, Protect and
ordinary attacks. Loaded ability/passive configurations are bounded test inputs,
not an assertion about every species' natural ability availability or game modes.

| Seed | Requested/completed | Before covered/exact | Final covered/exact | Final text differences/missing |
|---:|---:|---:|---:|---:|
| 777 | 2000 | 400 | 2000 | 0 / 0 |
| 9001 | 2000 | 400 | 2000 | 0 / 0 |
| 4242 | 2000 | 400 | 2000 | 0 / 0 |
| 31337 | 2000 | 400 | 2000 | 0 / 0 |

Before: 1600 unsupported Moody turns per seed, owning driver exit 1. Final:
8000 complete exact turns/texts, zero parse/stat mismatches, zero excluded
turns. Each seed supplies 100 cases per scenario and 1000 holders per side.
Actual applies: 1700/1700/1699/1700; seed4242 has one ordinary attack KO before
turn end. Suppressed/no ability/fainted/KO controls do not reach apply; they are
not mislabeled leaf canApply refusals. No source exceptions or skipped cases.

Ten actual runtime mutants are rejected after an intact exit-0 control: wrong
Speed Boost gate, wrong candidate order, +1 raise, -2 drop, allowing the same
stat, including a stat at cap/floor, omitting lower when all capped, omitting
raise when all floored, and simulation attempting queue work. Nine malformed
header/count/version/order/completion/RUN/END fixtures refuse with exit 2.
The queue-less API boundary separately reports unsupported for real work and
preserves state/RNG for simulation; it is not a whole-turn simulation fixture.

## Retained checks

Twelve saved source pairs, before/after the production leaf: plain/rich/switch/
summon at seed777 and ITEMS/TRAINER at all four seeds, 24000 cases on each
revision. No failures or coverage loss. Plain1958 and switch1891 unchanged;
rich1854 ->1858, summon1867 ->1868, five newly exact covered turns. ITEMS
1939/1945/1940/1956 and TRAINER1946/1943/1945/1953 unchanged, with zero covered
text differences/missing messages. Excluded turns retain their own reasons and
receive no pass credit. Producers/comparators/seeds/tolerances are unchanged.

`make -j2 -C game-core test build/gcplay` passes, paired 200-run digest
`a7e631e4`. Paired 1000-run normal/save: 1000 ended each, zero cleared/stuck/
crashed/timed out, 49963 turns, digest `8304ead1`; 14632 round trips, largest
save11472 bytes. These are host consistency checks, not gameplay-clear or
upstream whole-run/hardware evidence. No starter/UI/custom-content work changed.

## Reproduce

```sh
battle-core/tools/moodydiff.sh "$UP" 2000 777
battle-core/tools/moodydiff.sh "$UP" 2000 9001
battle-core/tools/moodydiff.sh "$UP" 2000 4242
battle-core/tools/moodydiff.sh "$UP" 2000 31337
TURNTEST_LOCALES="$UP/locales/en" python3 battle-core/tools/moody-mutations.py battle-core/build/moody-777.txt
TURNTEST_LOCALES="$UP/locales/en" python3 battle-core/tools/moody-malformed.py battle-core/build/moodytest battle-core/build/moody-777.txt
```

The wrapper requires the source pin and Node22. This slice proves bounded
singles through the actual caller; broader doubles, arbitrary loaded ability
combinations and hardware acceptance remain separate work.
