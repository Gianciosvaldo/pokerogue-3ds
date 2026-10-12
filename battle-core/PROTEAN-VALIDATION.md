# Protean and Libero type changes

Base main `1930826e3f5257f5108c0d8d796bb4b06ce2c31f`. Target source
`8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3 / GCC 14.2.

PokemonTypeChangeAbAttr (source ab-attrs.ts:1397) refuses Tera, Struggle,
CallMoveAttr/NaturePowerAttr and an already exclusively matching type. Otherwise
it selects the user's converted move type for its message and, outside simulation,
replaces summonData.types with that type. Added types remain. The source's Gen 9
once-per-summon conditions are commented out in init-abilities.ts; this port does
not introduce them. Existing ability-use sets and summon-type storage suffice.

MovePhase.useMove dispatches before fourth-sequence failure and charge handling.
failMove also dispatches for terrain failure and the listed Roar/Whirlwind/
Trick-or-Treat/Forest's Curse cases. Both C callers now use the actual dispatcher.
The trigger message snapshots the type chosen by canApply, before mutation.
Real calls without an engine remain explicitly unsupported rather than silently
claiming complete queue behavior; simulated calls change no Pokemon types.

The additive source builder delegates actual canApply/apply over the unchanged
Speed Boost turn scaffold. Two abilities × 20 states × both holder sides cover
damage/status, already matching type, suppression, passive slots, Tera, Struggle,
Nature Power and Copycat (including the subsequent called move), loaded mono/
dual types, an added type, previous ability use, primary/passive combination,
Aerilate/Normalize converted types, opponent Color Change, Psychic Terrain
failure, failed Forest's Curse and Dive charging. Every state and side occurs;
expected applies/refusals are required for each ability on every seed. Source
methods, comparisons, tolerances, existing seeds and counts are unchanged.

Separate actual applyAbAttrs helpers produce 80 normal/simulated checks for
both abilities and sides. They compare raw summon types, added-type preservation,
wave/summon use sets and unchanged RNG, including refusal boundaries and already
used abilities. The owning C helper initializes the same empty queue structure
as engine_reset before dispatching; a zeroed structure alone has no legal levels.
The new full-turn wrapper uses the unchanged Speed Boost replay projection and
requires every requested case, complete state/stats/phase/RNG/text and footer.

```sh
source /workspace/shared/activate-parity.sh
for seed in 777 9001 4242 31337; do
  battle-core/tools/protean-diff.sh /path/to/pinned/upstream 2000 "$seed"
done
make -C battle-core build/protean-helper-test
# In the external checkout containing only the generated additive producer:
PROTEAN_HELPERS=1 PROTEAN_N=2000 PROTEAN_SEED=777 \
  PROTEAN_OUT=/absolute/helpers.txt node node_modules/vitest/vitest.mjs \
  run test/tests/zz-dump/protean.producer.test.ts --maxWorkers=1
battle-core/build/protean-helper-test /absolute/helpers.txt
TURNTEST_LOCALES=/path/to/pinned/upstream/locales/en \
  python3 battle-core/tools/protean-mutations.py \
    battle-core/build/protean-777.txt /absolute/helpers.txt
TURNTEST_LOCALES=/path/to/pinned/upstream/locales/en \
  python3 battle-core/tools/protean-malformed.py \
    battle-core/build/protean-test battle-core/build/protean-777.txt
make -C battle-core all test
make -C game-core test
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

Results:

- Baseline C marks all 2,000 turns unsupported on each seed; the strict wrapper
  returns 1. After: 8,000 complete turns match with zero skipped, unsupported,
  parse/stat/state/RNG/phase/text differences or missing texts. All 80 source
  dispatcher helpers match.
- Twelve compiled production mutations are detected after intact turn/helper
  exits 0/0: wrong type, accepting already matching type, allowing Tera/Struggle/
  called moves, simulation writes, clearing added type, an invented once-per-
  summon restriction, missing normal/failure callers, wrong message type and
  using the raw move type. Simulation writes are caught by helpers; other
  mutations are caught by full turns, with boundary mutations also caught by
  helpers. Nine malformed full-turn traces reject with exit 2 after intact 0.
- Retained 24,000 before/after cases have zero covered differences or coverage
  loss. Rich seed777 grows 1,896→1,906; plain 1,958, switch 1,891, summon 1,869,
  ITEMS 1,939/1,945/1,940/1,956 and TRAINER 1,946/1,943/1,945/1,953 stay unchanged.
  Moody/knockout/leave-weather/Wonder Skin/priority suites pass 40,000 complete
  retained turns, including the unchanged priority and accuracy probes.
- Battle builds/boundary tests and game-core host tests pass. Paired 200-run
  digest `66a76998`; paired 1,000-run normal/save digest `430e4416`, 49,828 turns,
  1,000 ended each, zero stuck/crashed/timed out, 14,632 save round trips.

Host/3DS CI is recorded on this task's PR. Custom-core checks are excluded per
the user's instruction. Concurrent game/game-core changes are imported without
editing them; no new save fields are introduced. Full switch/doubles/form/boss
lifecycle and hardware acceptance remain separate proof, not completed by this
ordinary singles slice.
