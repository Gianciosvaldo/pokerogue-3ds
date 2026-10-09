# Bounded effective-stat held-item caller proof

Target: upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, Node 22.23.3.
This promotes seven existing effective-stat callers, with canonical real stack 1
and virtual stack 0. It adds no damage/stat formula, item class, acquisition,
transfer or save schema. Generic StatBooster has no factory; Muscle Band and
Wise Glasses are absent at this pin and remain unpromoted.

| Identity | Stats | Multiplier | Eligible species |
| --- | --- | --- | --- |
| Eviolite | DEF, SPDEF | 1.5 | Unevolved, non-Max holder |
| Light Ball | ATK, SPATK | 2 | Pikachu |
| Thick Club | ATK | 2 | Cubone, Marowak, Alolan Marowak |
| Metal Powder | DEF | 2 | Ditto |
| Quick Powder | SPD | 2 | Ditto |
| Deep Sea Scale | SPDEF | 2 | Clamperl |
| Deep Sea Tooth | SPATK | 2 | Clamperl |

Identity admission checks exact masks, multipliers and ordered species lists.
The existing `pr_apply_stat_booster_modifiers` and getEffectiveStat arithmetic
remain unchanged, preserving raw synthetic direct-damage/stat coverage outside
acquisition bounds. Fused Eviolite/species-stat items refuse at the full-turn
boundary; the original raw helper's primary-species behavior is retained and
is not claimed as fusion parity.

The new suite uses real pinned MoveEffectPhase, item apply/shouldApply and
getEffectiveStat. It compares all five effective stats explicitly before/after,
including Quick Powder when the boost does not change move order. Full state,
HP/damage, phases, messages and global/battle RNG are compared exactly. Ten
states cover both sides, eligible/ineligible species, bench/wrong/opposite-side
owners, wildcard, Max Eviolite refusal, stages, duplicate sequential items and
fixed/status moves. The three Thick Club species are varied. Existing producers,
comparators, strict guards, seeds, case counts and exclusions are unchanged.

Source SummonPhase.updateModifiers prunes held records whose owner is missing.
The first new fixture exposed 20 wildcard stat-item records surviving a forced
replacement summon. C now prunes only canonical Eviolite/species-stat orphans
at both existing summon update boundaries before recalculating party stats.
Known ordinary/bench owners and list order remain intact. Other item families,
virtual-stack resets and broader updateModifiers lifecycle remain separate.

## Results

Seeds 777, 9001, 4242 and 31337 each compare 2000 covered/pass full turns and
2000 identical texts; parse/stat/turn/text mismatches and uncovered cases are 0.
Each seed records family counts 286/286/286/286/286/285/285 and actual numerical
apply counts 142/170/170/170/1808/170/170 in the table's order. Apply observers
delegate to the original methods and count changed NumberHolder values during
turn execution; snapshot probes are outside those counts. Refusal counters
observe base StatBooster.shouldApply and false EvolutionStatBooster.apply:

| Seed | Recorded refusal calls by family |
| --- | --- |
| 777 | 2194/2570/2570/2260/722/2069/2561 |
| 9001 | 2190/2570/2570/2284/722/2012/2561 |
| 4242 | 2239/2570/2570/2298/722/2051/2561 |
| 31337 | 2208/2570/2570/2265/722/2040/2561 |

These call counts include rejected stat arguments and are not unique item
acquisitions. The 63 identity guards and 4 fused matching/wildcard admission
guards pass. Fourteen malformed/incomplete/fractional/invalid identity inputs
all exit 2; fractional requested N exits 1 before trace generation.

Actual C source mutations on seed 777, all exiting 1 while leaf guards pass:

- Disconnect the effective-stat held-item caller: 1373 numerical snapshot parse
  mismatches, 627 remaining covered/pass; the request contract refuses the loss.
- Disconnect Quick Powder only: 198 numerical SPD snapshot mismatches, 1802
  remaining covered/pass. This proves the numerical speed effect independently
  of whether move order changes.
- Disconnect summon orphan cleanup: 20 turn-state failures, 1980/2000 pass,
  with 2000 identical texts. The check propagates state failures.

All 44 retained before/after pairs are byte-identical: 40 previous full-turn
suites and four unchanged legacy direct-damage/stat seeds. Raw legacy coverage
remains 2000/2000/2000/1999, all passing with 0 stat/carried-state mismatches and
0 uncovered. Seed 31337 retains one existing upstream Fusion Flare throw.

Before/after `make -C game-core -j2 test` exits 0. Normal and saved 1000-run logs
are byte-identical to their respective baselines: 1000 ended, 0 cleared/stuck/
crashed/timed out, 49206 turns, paired digest `4d9f91a1`, 14291 save/load round
trips, max save 11400 bytes. Every stub/report row stays unchanged. The included
200-run host pair is `2f376abe`. Host consistency does not establish upstream
whole-run parity. Exact-head host/3DS CI is required separately; skipped Task 63
jobs and hardware acceptance are not credited.

## Reproduction

Evidence: `/workspace/validation/lane-a/ten/stat`, baseline main `e99b5f9`.
Node commands run from the external pinned `/workspace/shared/upstream-lane-a`.
The source loop repeats these exact commands for all four seeds:

```sh
. /workspace/shared/activate-parity.sh
make -C battle-core -j2
STATITEM_N=2000 STATITEM_SEED=777 STATITEM_OUT=/workspace/validation/lane-a/ten/stat/stat-777.txt node node_modules/vitest/vitest.mjs run test/tests/zz-dump/effective-stat-item.producer.test.ts --maxWorkers=1
TURNTEST_LOCALES=/workspace/shared/upstream-lane-a/locales/en battle-core/build/statitemtest /workspace/validation/lane-a/ten/stat/stat-777.txt
make -C game-core -j2 test
./game-core/build/gcplay --auto 1000 --jobs 8
./game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

Retained turn suites replay their original files through both baseline/final
owning binaries with TURNTEST_LOCALES set. Legacy fixtures replay unchanged
through each build/difftest. Mutation builds use the exact modified C callsites,
the same seed-777 trace and the strict final driver; no fixture is weakened.
