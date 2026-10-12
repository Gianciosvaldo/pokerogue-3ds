# Parental Bond and Skill Link callbacks

Base main `699f9e818a77cddff2436690e434de09ad32ef5c`. Source pin
`8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3 / GCC 14.2.

AddSecondStrikeAbAttr delegates canBeMultiStrikeEnhanced(user,true,target), then
adds one to the hit holder. MoveEffectPhase calls it after intrinsic hit count
and before Multi-Lens. The eligibility helper and Parental Bond's final-strike
quarter-damage closure were already ported; the unsupported caller and missing
leaf prevented their normal use. MaxMultiHitAbAttr sets the rolled bucket to 0,
not the final count to 5. The source draws its battle RNG bucket before calling
Skill Link, then interprets the thresholds. C now preserves that draw and order.
Both source leaves also change their numerical holders during simulation.

The additive builder delegates the actual source canApply/apply methods over
the unchanged Speed Boost scaffold. Two ability families × 24 states × both
holder sides cover suppression/passives, status/fixed/intrinsic/check-all-hits
moves, Pollen Puff targeting an enemy, a spread move with one target, Protect,
one/two Multi-Lenses and wrong ownership, knockout interruption, Substitute,
Stamina/Weak Armor between hits, low user HP, combined Parental Bond/Skill Link,
Tera, Dragon Rage, absent ability, charging Dive and excluded Endeavor. Actual
activation/refusal guards require the relevant family/state coverage.

Separate actual applyAbAttrs source helpers compare 440 combinations: both
abilities/sides, normal/simulated calls, eleven eligible/excluded moves and
input holders -1/0/1/3/19. They compare output holders, wave/summon use sets and
unchanged RNG. These are dispatch boundaries, not synthetic whole turns.

The initial additive fixture incorrectly stopped at VictoryPhase and therefore
produced 41 trace differences on seed777 while the other 1,959 turns matched.
That new cut was removed: the final producer retains the original turn boundary
and all knockout cases. It does not omit victory execution to make the check pass.
Established producers/comparators/tolerances/seeds/counts remain unchanged.

```sh
source /workspace/shared/activate-parity.sh
for seed in 777 9001 4242 31337; do
  battle-core/tools/multi-hit-abilities-diff.sh /path/to/pinned/upstream 2000 "$seed"
done
make -C battle-core build/multi-hit-abilities-helper-test
# External checkout with only the generated additive producer:
MULTIHITABIL_HELPERS=1 MULTIHITABIL_N=2000 \
  MULTIHITABIL_OUT=/absolute/helpers.txt node node_modules/vitest/vitest.mjs \
  run test/tests/zz-dump/multi-hit-abilities.producer.test.ts --maxWorkers=1
battle-core/build/multi-hit-abilities-helper-test /absolute/helpers.txt
TURNTEST_LOCALES=/path/to/pinned/upstream/locales/en \
  python3 battle-core/tools/multi-hit-abilities-mutations.py \
    battle-core/build/multi-hit-abilities-777.txt /absolute/helpers.txt
TURNTEST_LOCALES=/path/to/pinned/upstream/locales/en \
  python3 battle-core/tools/multi-hit-abilities-malformed.py \
    battle-core/build/multi-hit-abilities-test battle-core/build/multi-hit-abilities-777.txt
make -C battle-core all test
make -C game-core test
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

Results:

- Baseline has 536 covered exact / 1,464 unsupported turns per seed; the owning
  wrapper rejects the subset. After: all 8,000 complete turns match, with zero
  skipped/unsupported/parse/stat/state/RNG/phase/text differences or missing text.
  All 440 source dispatch checks match holder values, bookkeeping and RNG.
- Twelve compiled production mutations fail after intact 0/0: two extra hits,
  replacement instead of addition, ignoring eligibility, skipping simulated
  parent/Skill Link effects, wrong bucket, missing callers, skipping Skill Link's
  RNG draw, wrong move context, wrong bucket threshold and missing last-strike
  damage reduction. Replacement and simulation boundaries are detected by the
  helper suite; all other mutations are detected by full turns.
- Nine malformed full-turn traces reject with exit 2 after intact 0.
- Retained 24,000 before/after cases: no covered mismatch or coverage loss.
  Rich seed777 rises 1,924→1,929. Plain 1,958, switch 1,891, summon 1,869,
  ITEMS 1,939/1,945/1,940/1,956 and TRAINER 1,946/1,943/1,945/1,953 stay unchanged.
  All 56,000 retained Moody/knockout/leave-weather/Wonder Skin/priority/Protean/
  field-priority full turns pass unchanged, including numerical probes.
- Battle builds/boundary and game-core host tests pass. Paired 200-run digest
  `6006b9c3`; paired 1,000-run normal/save digest `05c563b4`, 49,360 turns,
  1,000 ended each, zero stuck/crashed/timed out and 14,516 save round trips.

Exact-head host/3DS CI is recorded on this task's PR. Custom-core checks are
excluded by user instruction. Other agents' concurrent changes are preserved.
No new save fields or generated tables are introduced. Natural multiple-target/
ally Pollen Puff behavior, Beat Up's party count, broader doubles, remaining
forms and scripted bosses are not completed by this bounded singles proof.
