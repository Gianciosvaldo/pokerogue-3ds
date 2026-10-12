# Shared capture/faint scoring

Base main `527b7cd58ea62121b85b59490f93c1db9fe206ba`. Source
`8555c08c823b856cbec4eb99ca84ea52a955836d`, Node 22.23.3 / GCC 14.2.
The user reported the duplicated score calculations at phases.c:434/2674;
the current base had them at 440/2806. Both omitted sqrt(bossSegments).

Both actual C callers now use one private port of BattleScene.addFaintedEnemyScore.
It applies the source's base experience/form boost, level/cap and IV factors,
matching enemy-owned held-item multipliers in array order, then the boss square
root, and only then ceil. Multipliers apply once per modifier object, regardless
of stacks. Vitamins/Old Gateau use 1.1; Shuckle Juice/Macho Brace/type boosters
use 1.2; other known held items inherit 1. Unknown/unported item contexts still
report unsupported. Missing boss segment context still reports unsupported;
missing level cap returns explicitly unsupported instead of dividing by zero.
Final-boss lifecycle and general item acquisition are not completed here.

The additive producer executes the actual pinned BattleScene method on a live
scene. It varies six species, boosted forms, IVs, wave-derived real level caps,
fractional starting scores, boss segments 0/1/2/3/4/5/8, zero to eight actual
exported modifier objects, enemy/player lists, owners and stacks. The item objects
exercise actual score getters; their acquisition/type generation is outside this
numeric API proof. The C harness executes actual PH_ATTEMPT_CAPTURE (Master Ball)
and PH_FAINT callers. It compares score exactly, unchanged global RNG and the
callers' expected battle RNG (capture has its original critical-capture draw).
This is score/API and C caller proof, not complete upstream capture/faint turns.

A first source matrix used levels 1–1,000. C's uint8_t level truncated values
above 255: seed777 had 1,464/2,000 caller mismatches. That source corpus and
failure log are retained in the execution workspace. The producer still defaults
to the original 1–1,000 matrix; an explicit LEVEL_MAX parameter allows the
separate supported-range matrix. The owning C harness rejects levels above 255
with exit 2 instead of silently truncating them. Widening level remains a real
whole-core/Endless prerequisite, including game-core's serialized level field;
this change does not alter the existing save representation.

```sh
source /workspace/shared/activate-parity.sh
for seed in 777 9001 4242 31337; do
  battle-core/tools/boss-score-diff.sh /path/to/pinned/upstream 1000 "$seed" 255
done
# Reproduce the unresolved high-level prerequisite (refuses with exit 2):
battle-core/tools/boss-score-diff.sh /path/to/pinned/upstream 1000 777 1000
python3 battle-core/tools/boss-score-mutations.py battle-core/build/boss-score-777.txt
python3 battle-core/tools/boss-score-malformed.py battle-core/build/boss-score-test battle-core/build/boss-score-777.txt
make -C battle-core all test
make -C game-core test
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

Results within levels 1–255:

- Four seeds × 1,000 actual source API cases × both C callers: all 8,000 score
  and caller RNG checks match. Baseline matched 66/2,000 per seed and failed
  1,934; after matched all 2,000. Separate unsupported contexts remain in
  1,666/2,000 executions per seed (forms/items outside this score-only proof).
  No cases are omitted from the numerical comparison.
- Twelve compiled production mutations are detected: missing capture/faint
  callers, missing/root versus segment multiplier, premature rounding before
  root/items, floor, applying factors per stack, wrong owner, missing items,
  wrong 1.1 factor, and an extra RNG draw. Intact control passes.
- Nine malformed inputs refuse with exit 2, including oversized levels.
- Retained 24,000 before/after full turns: no covered mismatch or coverage loss;
  all covered counts unchanged (rich seed777 remains 1,932/2,000).
  All 72,000 dedicated Beat Up/ability full turns pass unchanged.
- Battle host/boundary and game-core tests pass. Paired 200-run digest
  `74e28602`; paired 1,000-run normal/save digest `bbf2f2f6`, 49,463 turns,
  1,000 ended each, zero stuck/crashed/timed out and 14,528 save round trips.

Exact-head host/game-host/3DS CI is recorded on the PR. Custom-core checks are
excluded by user instruction. No source expectations, established comparators,
tolerances, counts or seeds changed. No save/generated data change or hardware
claim. The score bug is fixed for represented inputs; the wider level and flee
completion gates remain explicit in REMAINING-BATTLE-CORE.md.
