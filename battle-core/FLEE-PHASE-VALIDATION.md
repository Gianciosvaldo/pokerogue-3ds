# AttemptRunPhase source differential

Base main `433ff4e522d657e963bcb3d3a363ac8683cb34d1`. Source
`8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3 / GCC 14.2.

The numerical expression matched the pin for ordinary active singles, but the
phase and field boundaries differed. C queued SelectBiome after every successful
Endless escape; source modes never set hasRandomBiomes. C also excluded fainted
enemy partners from the speed total/boss check, required on-field activity where
the source requires only positive HP for players, and left status counters behind.
After held-item removal, source updateModifiers also normalizes persistent stacks
and refreshes the enemy party's stats; C did neither.

The actual phase now follows those source boundaries. Both sides retain their
original escape-attempt increment and seeded roll before Run Away dispatch.
Only pr_is_new_biome queues selection. Success clears all enemy-slot HP/status
and pending status, resets virtual/removes zero-count persistent modifiers and
refreshes all enemy party stats. The source's unfused Spliced Endless halving is
now represented during stat refresh. Base-stat total/flat modifier ordering before
that halving remains explicitly unsupported; flee removes held items first.
The existing formula is exported for direct source comparison without replacing
the actual phase caller. No Pokemon/save fields or generated tables change.

The additive source fixture runs actual AttemptRunPhase.start and its real
calculateEscapeChance on naturally initialized live singles/doubles scenes.
It restores the real battle RNG method that GameManager substitutes. Only
presentation/destruction and the next-phase execution boundary are stubbed;
queues and ability methods run normally, with delegated observation. The
projection compares exact chance, outcomes, attempt counter, all field stats,
HP/status counters, failed-run flags, held/persistent objects and their stacks,
ability use sets, global/battle RNG, full pending queue kind/order/index presence
and passive display payload, activation order and rendered English flee text.

Twenty-four states cover both field sides, five default modes, biome boundary
waves, boss/nonboss ratios and attempt caps, stages/paralysis/toxic/sleep,
primary/passive/suppressed/duplicate Run Away, fainted players/enemies including
a fainted boss partner, meaningful speed ties, stage/status ordering and Trick
Room, held/persistent cleanup, virtual/zero stacks and stat refresh. A separate
transient corpus covers switching-out slot semantics. These are phase-local
states, not complete doubles turns or challenge eligibility rules.

Fixture development found two transport mistakes: Run Away was initially loaded
only into ability_override without resolving C ability, and stage indexing used
the wrong source/C slot. They are corrected; setStatStage and STAT_SPD-1 now
produce nonzero stages. Extending the projection to modifier objects also refused
an incorrect TurnHeal constructor with fractional stacks; its actual signature
is now used. A corresponding constructor correction in the previously merged
score producer leaves the seed777 emitted score oracle byte-identical. The full
stat projection then exposed the missing refresh/spliced halving. No established
source expectations, producers, comparators, tolerances, counts or seeds changed.

```sh
source /workspace/shared/activate-parity.sh
for seed in 777 9001 4242 31337; do
  battle-core/tools/flee-phase-diff.sh /path/to/pinned/upstream 2000 "$seed" 0
done
battle-core/tools/flee-phase-diff.sh /path/to/pinned/upstream 2000 777 1
TURNTEST_LOCALES=/path/to/pinned/upstream/locales/en \
  python3 battle-core/tools/flee-phase-mutations.py battle-core/build/flee-phase-777-0.txt
TURNTEST_LOCALES=/path/to/pinned/upstream/locales/en \
  python3 battle-core/tools/flee-phase-malformed.py battle-core/build/flee-phase-test battle-core/build/flee-phase-777-0.txt
make -C battle-core all test
make -C game-core test
game-core/tools/fleediff.sh /path/to/pinned/upstream 600 777
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

Results:

- Baseline final projection: 722/732/729/700 exact out of 2,000 per seed, with
  1,278/1,268/1,271/1,300 mismatches. The baseline adapter changes only the old
  helper's name/visibility so the new numeric oracle can call its unchanged body.
- After: all 8,000 main plus 2,000 transient cases match every projected field,
  with no skipped/unsupported/deferred/overflow cases. Each main seed includes
  1,000 natural doubles; successes 1,165/1,156/1,159/1,190, transient 1,165.
- Eighteen compiled production mutations fail after intact control: biome
  presence, inactive enemy and player count, effective versus raw speed, boss
  slope/attempt bonus, attempt increment, RNG, ability dispatch, failed flag,
  status/item/virtual/zero-stack cleanup, stat refresh, phase order and spliced
  halving. Eleven malformed complete-case/queue/stats/text inputs refuse with exit 2.
- Unchanged original #243 game-core differential still matches 587 singles,
  337 successes/250 failures/118 biome selects, zero mismatches. Its thirteen
  source-skipped speed ties remain unchanged; the new phase suite has no skips.
- Retained 24,000 before/after complete turns: no covered mismatch/loss and no
  covered count changes (rich777 remains 1,932/2,000). All 72,000 dedicated
  ability/Beat Up turns and 8,000 retained score API caller checks pass.
- Battle and game-core host tests pass. Paired 200-run digest `74e28602`;
  paired 1,000-run normal/save digest `bbf2f2f6`, 49,463 turns, 1,000 ended,
  zero stuck/crashed/timed out and 14,528 save round trips. Wrapper smoke N120 passes.

Exact-head host/game-host/3DS CI is recorded on the PR. Custom-core checks are
excluded by user instruction. No hardware, full doubles, arbitrary challenge
rules, Transform, weather/Neutralizing Gas leave lifecycle or complete core parity
is claimed. The user's next priority is Ditto/Transform, including effective
sprite identity, stat/moves/type copying, reset/save behavior and shared Imposter
paths. Wide levels and broader stat-modifier ordering remain completion gates.
