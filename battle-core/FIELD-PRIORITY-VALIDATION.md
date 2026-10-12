# Defending-field priority immunity

Base main `9761a6297e13d462bed4cb4d54a57e5c4684d856`. Source pin
`8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3 / GCC 14.2.

MovePhase.thirdFailureCheck (source move-phase.ts:850) first checks third-sequence
conditions and terrain. If they did not fail and the first active target is an
opponent, it walks the defending field, dispatching FieldPriorityMoveImmunityAbAttr
with a shared cancellation holder. The resulting cancellation fails the move
before Protean and before move effects. C's leaf already matches source
ab-attrs.ts:559: refuse an already cancelled holder, nonpositive priority, ally
target and same-side attacker; otherwise set cancellation. This change replaces
the unsupported caller hook with that real dispatch and uses its result.

The additive builder observes/delegates actual source canApply/apply over the
unchanged Speed Boost scaffold and drains the actual source turn phases. Three
ability families (Queenly Majesty, Dazzling, Armor Tail) × 16 states × both holder
sides cover positive/zero/negative priority, suppression/passive/dual slots,
Mold Breaker, Prankster Thunder Wave/Growl/self-targets, Psychic Terrain precedence,
Protean cancellation timing, legal first-turn Fake Out and full/reduced-HP Gale
Wings. Every state and side occurs, with required applies/refusals for all three
families on every seed. Both abilities are eligible on a dual holder, but the
shared cancellation prevents the second from applying.

The initial new fixture loaded Fake Out's waveTurnCount as 0, so its source-only
activation guard rejected the producer before writing a trace. The corrected
fixture uses the actual FirstMoveCondition requirement, waveTurnCount === 1.
No source condition or coverage requirement was weakened. The strict owning C
wrapper includes the unchanged replay projection and requires all cases and
complete stats/state/RNG/phase/text/footer. Established source producers,
comparators, seeds, counts and tolerances remain unchanged.

```sh
source /workspace/shared/activate-parity.sh
for seed in 777 9001 4242 31337; do
  battle-core/tools/field-priority-diff.sh /path/to/pinned/upstream 2000 "$seed"
done
TURNTEST_LOCALES=/path/to/pinned/upstream/locales/en \
  python3 battle-core/tools/field-priority-mutations.py \
    battle-core/build/field-priority-777.txt
TURNTEST_LOCALES=/path/to/pinned/upstream/locales/en \
  python3 battle-core/tools/field-priority-malformed.py \
    battle-core/build/field-priority-test battle-core/build/field-priority-777.txt
make -C battle-core all test
make -C game-core test
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

Results:

- Baseline: 372 covered exact turns and 1,628 unsupported per seed; owning
  wrapper exits 1, with no pass credit for its subset. After: four seeds × 2,000
  complete turns all match; zero skipped/unsupported/parse/stat/state/RNG/phase/
  text differences or missing texts. Each seed executes 1,000 ability show/hide
  phases, and every required ability state applies/refuses.
- Ten compiled production mutations fail after intact 0: missing dispatch,
  ignoring cancellation, wrong field/attacker/move, simulating the real caller,
  blocking zero/negative priority, repeated activation after cancellation and
  a leaf that fails to cancel. All are detected by full-turn comparisons.
- Nine malformed transports reject with exit 2 after intact 0: footer,
  count/version, fractional count, case order, trailing records, duplicate/
  missing RUN and missing END.
- Retained 24,000 before/after cases have no covered mismatch or coverage loss.
  Rich seed777 grows 1,906→1,924; plain 1,958, switch 1,891, summon 1,869,
  ITEMS 1,939/1,945/1,940/1,956 and TRAINER 1,946/1,943/1,945/1,953 stay unchanged.
  All 48,000 retained Moody/knockout/leave-weather/Wonder Skin/priority/Protean
  turns pass unchanged, including the numerical probes. A premature baseline
  read of seed31337 returned 2 because the source trace did not yet exist; it
  was rerun after the producer finished and correctly returns 1 for unsupported
  behavior. Missing input was never counted as a behavioral regression or pass.
- Battle builds/boundary tests and game-core host tests pass. Paired 200-run
  digest `b58649e2`; paired 1,000-run normal/save digest `9564c374`, 49,385 turns,
  1,000 ended each, zero stuck/crashed/timed out, 14,518 save round trips.

Exact-head host/3DS CI is recorded on the task PR. Custom-core checks are excluded
per the user's instruction. No generated tables, save fields, other owners'
code, disabled source ability registrations or broad doubles flags are changed.
This proves the loaded ordinary singles path; natural partner-field behavior,
redirect/Commander, wider doubles, boss lifecycle and hardware remain separate.
