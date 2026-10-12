# Knockout stat abilities

Pinned source: `8555c08c823b856cbec4eb99ca84ea52a955836d`; English locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`. Node 22, GCC 14.2,
`-ffp-contract=off`. All expectations are produced by the unchanged external source.

## Behavior and scope

`ab-attrs.ts` PostVictoryStatStageChangeAbAttr (2076) and
PostKnockOutStatStageChangeAbAttr (2139) queue the existing StatStageChangePhase.
The real FaintPhase caller (141–152) applies all-knockout abilities in speed order,
then victory abilities when a recorded attack has an on-field defeat source,
then the winning move's existing post-victory effect. The C caller now dispatches
these abilities instead of merely recording an unsupported base class.

Covered abilities: Moxie, Chilling Neigh, Grim Neigh, both As One variants,
Beast Boost, Eelevate, Soul-Heart. Soul-Heart also fires for poison knockouts;
victory abilities do not. As One's source-defined unsuppressability is preserved.
The highest-stat closure uses getStat(stat, false), excludes HP/accuracy/evasion,
and resolves ties in ATK/DEF/SPATK/SPDEF/SPD order with a strict `>` comparison.
Closure hash `6297309b617b` is checked; unknown functions remain unsupported.
Simulation returns before queuing; non-simulated calls without an engine report
the unsupported leaf explicitly.

The generator gives this mixed fixed-list/function field a typed representation.
Tables were regenerated with tools/redump.sh; closure manifests did not drift.
New stat-change lists use separate names to avoid renumbering existing lists.
Redumping now also reproduces the three pre-existing local transformation command
values consumed by phases.c, preserving their existing generated values.

Battle Bond's extra condition and battle flags, CopyFaintedAlly's victim context,
additional game modes, and unrestricted doubles are outside this slice.
They are not claimed as supported. The simulation checks below are C API boundary
checks, not source-produced simulated whole turns.

## Reproduction

```sh
source /workspace/shared/activate-parity.sh
for seed in 777 9001 4242 31337; do
  battle-core/tools/knockout-stats-diff.sh /path/to/pinned/upstream 2000 "$seed"
done
TURNTEST_LOCALES=/path/to/pinned/upstream/locales/en \
  python3 battle-core/tools/knockout-stats-mutations.py battle-core/build/knockout-stats-777.txt
TURNTEST_LOCALES=/path/to/pinned/upstream/locales/en \
  python3 battle-core/tools/knockout-stats-malformed.py \
    battle-core/build/knockout-stats-test battle-core/build/knockout-stats-777.txt
```

The additive producer builder uses the unchanged Fell Stinger turn scaffold,
delegates the real ability methods, and drains real source turns. Each seed loads
250 cases per ability, both holder sides, and 22 scenarios: all five raw winners,
an all-stat tie, no knockout, Simple/Contrary in both ability positions, caps and
floors, suppression, passive triggers, stages which would change a wrongly weighted
winner, no ability, holder knockout, misses, poison knockout, Protect, and a
fainted Soul-Heart holder. Source exceptions or missing per-ability scenario
coverage reject the fixture. No source handlers or expected results are replaced.

## Results

- Four seeds × 2,000: all 8,000 turns pass, with no parse/stat/state/RNG/phase/text
  mismatches, missing messages, skipped cases, or unsupported cases.
- Before: 527/524/525/524 covered; after: 2,000 each. The same source fixtures
  add 5,900 covered turns. Before uses the established Fell Stinger replay
  projection with pre-change production objects; ordinary turntest cannot read
  this fixture's EFFECTIVE records.
- Eleven compiled runtime mutants fail after the intact control passes: wrong
  boost amounts, tie order, stat order, stage weighting, Soul-Heart stat/amount,
  simulation queuing, missing victory/all-knockout callers, and direct-only
  Soul-Heart. Simulation queuing is rejected by the C boundary check.
- Nine malformed transports return 2 after an intact return-0 control: missing
  completion, wrong count/version, fractional count, case ordering, extra records,
  duplicate/missing RUN, missing END. Sixteen queue/simulation boundaries pass.
- Retained plain/rich/switch/summon at seed 777 and ITEMS/TRAINER at all four seeds:
  24,000 cases before and after, no coverage loss or covered mismatch. Rich grows
  from 1,858 to 1,871 exact covered turns; the other eleven pairs retain coverage.
  Existing unsupported records remain explicit; no seed exclusions or tolerances
  change. Established fixtures/comparators remain unchanged.
- Host unit tests and paired 200-run save tests pass. Paired 1,000-run normal/save
  soak: 1,000 ended each, zero stuck/crashed/timed out, 49,864 turns,
  matching digest `5cab9205`. Existing reported stand-ins remain explicit.

3DS and repository CI results are recorded on the task PR; this document does not
claim an emulator run.
