# Wonder Skin accuracy behavior

Base main `fb55cca0130b069ee8fc791e1a2e897964af9d97`; target source
`8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3 / GCC 14.2.

Source ab-attrs.ts:607 accepts status moves whose current accuracy holder is at
least 50, then sets it to 50. Its apply also changes the holder during simulation.
Move.calculateBattleAccuracy (move.ts:1055) dispatches this after variable move
accuracy and before Wide Lens, Fog and Gravity. The existing C calculation now
calls the real dispatcher instead of marking this leaf unsupported. No new
Pokemon state, generated tables, game-core or presentation code is required.

The new producer builder delegates the actual source canApply/apply methods over
the unchanged Speed Boost turn scaffold. Twenty loaded states cover Thunder Wave,
Darkrai's Dark Void (50), Tackle, Swift, Tearful Look (always hit), suppression,
primary/passive slots, accuracy/evasion stages, Fog, Gravity, canonical Wide Lens
one/two stacks and wrong owner, absent ability, Screech and Sing. Both holder
sides are tested. Source exceptions or incomplete coverage reject the fixture.

Two actual source calculateBattleAccuracy probes per turn compare numerical
accuracy in both directions and assert no RNG consumption. They make modifier
ordering observable even when a seed's hit rolls yield unchanged outcomes.
The owning replay copy adds the existing Wide Lens amount/virtual-stack fields
and accuracy-probe transport; ordinary state/text/RNG/phase comparisons remain
unchanged. The original replay scaffold did not print those modifier fields;
no fields were removed from expectations to bypass that transport limitation.

A separate actual applyAbAttrs helper fixture compares 56 cases: status/damage,
both sides, normal/simulated dispatch, accuracy -1/0/49/49.9/50/50.1/100, resulting
holder values and both ability-use sets. These are source-produced dispatch
checks, not simulated whole turns. Integer modifier transport remains bounded
to the existing canonical Wide Lens representation.

```sh
source /workspace/shared/activate-parity.sh
for seed in 777 9001 4242 31337; do
  battle-core/tools/wonder-skin-diff.sh /path/to/pinned/upstream 2000 "$seed"
done
# Copy the generated additive producer into another external checkout if desired.
WONDERSKIN_HELPERS=1 WONDERSKIN_N=2000 WONDERSKIN_SEED=777 \
  WONDERSKIN_OUT=/absolute/helpers.txt node node_modules/vitest/vitest.mjs \
  run test/tests/zz-dump/wonder-skin.producer.test.ts --maxWorkers=1
battle-core/build/wonder-skin-helper-test /absolute/helpers.txt
TURNTEST_LOCALES=/path/to/pinned/upstream/locales/en \
  python3 battle-core/tools/wonder-skin-mutations.py \
    battle-core/build/wonder-skin-777.txt /absolute/helpers.txt
TURNTEST_LOCALES=/path/to/pinned/upstream/locales/en \
  python3 battle-core/tools/wonder-skin-malformed.py \
    battle-core/build/wonder-skin-test battle-core/build/wonder-skin-777.txt
```

Results:

- Pre-change C has 1,500 numerical accuracy mismatches and 400 unsupported turns
  per seed; only 100 covered turns match. The before-snapshot reader labels the
  computed accuracy differences as parse mismatches; they are not input omissions.
  The owning strict wrapper rejects the baseline rather than crediting its subset.
- All 8,000 complete turns / 16,000 numerical probes match, with zero parse/stat/
  state/RNG/phase/text differences, skipped/unsupported cases or missing texts.
  All 56 helper cases match holder values and normal/simulated bookkeeping.
- Eight compiled production mutations are detected after intact turn/helper
  controls return0: wrong cap, damage moves capped, excluding exact50, including
  values below50, skipping simulated effect, missing caller, wrong holder, and
  capping after Wide Lens. Three boundary mutations are detected by helpers;
  item ordering is detected by the numerical probes.
- Twelve malformed transports return2 after intact0: missing completion, wrong
  count/version, fractional count, case order, trailing records, duplicate/missing
  RUN, missing END, missing/duplicate accuracy probe and nonfinite accuracy. Both
  accuracy probe records are required per case.
- Retained 24,000 baseline cases before/after lose no coverage and have no covered
  mismatch; rich seed777 grows from 1,871 to 1,872. Retained Moody, knockout-stat
  and leave-weather suites: 24,000/24,000 complete turns pass. Established source
  producers/comparators/counts/seeds/tolerances are unchanged.
- Battle builds/unit tests and game-core host tests pass. Paired 200-run digest
  `031085fd`; paired 1,000-run normal/save digest `6be1a3db`, 49,864 turns,
  1,000 ended each, zero stuck/crashed/timed out, 14,633 round trips.

This proves the loaded ordinary singles accuracy path and the source dispatcher
boundaries. It does not complete general ability bypass, challenges, all doubles,
all modifiers or broader variable-accuracy moves. Host/3DS CI results are recorded
on the task PR; custom-core checks are excluded by user instruction. No emulator,
console, complete battle-core or whole-run source parity is claimed.
