# Mystical Rock weather/terrain caller proof

Pinned upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, Node 22.23.3.
The existing MYSTICAL_ROCK factory constructs FieldEffectModifier with held cap 2.
This slice promotes known ordinary identities with real stacks >=1, virtual
stacks >=0 and effective count <=2. It proves the existing weather and terrain
callers; no formula, class, field, acquisition or save schema is added.

Upstream FieldEffectModifier.apply adds `2 * stackCount`: real stacks determine
the extension, while virtual stacks are ignored. Scene.applyModifier stops at
the first successful matching item. C's existing helper and both weather/terrain
callers already implement these rules and remain unchanged. Full-turn canonical
admission is separate from raw helper arithmetic outside acquisition bounds;
unknown classes and invalid stack shapes remain unsupported.

The original temporary-booster follow-up encountered an established game caller
assertion that temporary full-turn admission remains unsupported. This PR uses
the independent FieldEffect proof instead. Temporary full-turn proof and its
explicitly authorized stronger caller-contract update remain a separate task;
this slice does not change that assertion.

## Source comparison

The new producer delegates real MoveEffectPhase, weather/terrain setting and
FieldEffectModifier.apply. Its observer verifies the exact real-stack increment
and counts genuine turn-time applications. Full states compare durations/max
values, subsequent lapse/expiry, damage/healing, owner/order/real/virtual stock,
phase/queue records, messages and both RNG streams. Real levels/IVs/natures and
turn numbers remain varied; Ball Fetch bounds ability scope.

Eight setters are Sunny Day, Rain Dance, Sandstorm, Hail and Misty/Electric/
Grassy/Psychic Terrain. Ten states (200 cases each per seed) cover both sides,
real stacks 1/2, real1+virtual1, bench/wrong/opposite-side owners, wildcard,
ordered duplicate first-match and wrong-first/valid-second, same-field refusal
with one-turn expiry, and immutable weather refusal/different-terrain replacement.
The effects have 260/260/260/260/240/240/240/240 cases per seed. Low-HP
wildcard and real1+virtual1 holders of Sandstorm/Hail faint into replacements;
this checks actual source summon cleanup rather than only duration arithmetic.
The first wildcard-only version exposed 26 turn-state mismatches: source summon
updateModifiers removes the orphan, while C retained it. The existing scoped
summon helper now includes canonical FieldEffect records: it prunes orphans,
resets surviving virtual counts and preserves real counts/owners/order before
party recalculation. Other families and existing stat-item behavior are retained.

Seeds 777, 9001, 4242 and 31337 each compare 2000 covered/pass turns and 2000
identical texts, with 0 parse/stat/turn/text mismatches, skips or uncovered cases.
Each records 1296 actual helper applies: 896 create duration 7, 400 duration 9.
The other 704 cases do not fire the helper. Virtual 1 + real 1 creates duration 7,
not 9; two matching items use only the first. All 19 identity/raw-helper guards
pass, including real-only arithmetic outside the cap with no raw API flag and
unchanged inventory/RNG. Eleven malformed/incomplete/fractional/class/stack
variants all exit 2. A fractional requested N is rejected before trace generation
(exit 1).

Actual C source mutations on the unchanged seed-777 source trace:

- Disconnect both actual weather/terrain callers: guards pass, 1296 turn-state
  failures, 704/2000 pass, exit 1. These failures match the actual apply count.
- Accumulate matching items instead of stopping at the first: the strict wrapper
  immediately fails the raw first-match guard (exit 1). Independent replay through
  the unchanged owning comparison also fails 200/2000 turn states (1800 pass,
  exit 1). This second diagnostic measures source sensitivity; it does not credit
  the failed strict wrapper or bypass its admission/contract gate.
- Omit canonical FieldEffect orphan/virtual summon cleanup: 52 turn-state
  failures, 1948/2000 pass, exit 1 with all raw/identity guards passing.
- Omit only the FieldEffect virtual reset: 26 turn-state failures, 1974/2000 pass,
  exit 1 with guards passing. This separates virtual persistence from duration.

No established producer, comparator, tolerance, seed, count, guard or exclusion
is edited. Existing full-turn, direct-damage/stat and weather/arena/form corpus
coverage remains unchanged: all 60 before/after pairs are byte-identical.
These comprise 44 prior full-turn suites, four legacy direct-damage/stat seeds,
and four seeds each of weather, arena and weather-form regressions.

| Retained mode | Covered/pass per seed (777/9001/4242/31337) | Mismatches |
| --- | --- | --- |
| Legacy direct damage/stat | 2000/2000/2000/1999 | 0 stats/carried-state/covered failures |
| TURN_WEATHER | 1957/1960/1955/1963 | 0; uncovered 43/40/45/37 unchanged |
| TURN_ARENA | 1852/1842/1832/1846 | 0; uncovered 148/158/168/154 unchanged |
| Weather-form callbacks | 2000/2000/2000/2000 identical | 0 |

The raw seed31337 keeps its existing upstream Fusion Flare throw. All 44 older
turn outputs, including their covered counts, stats and texts, are unchanged.

## Host consistency

Before/after `make -C game-core -j2 test` exits 0, with paired 200-run digest `74adeffb`.
Each normal/saved 1000-run path ends 1000, clears 0, stuck/crash/timeout 0, 50,394 turns,
paired digest `43f24f9f`. Saved runs perform 14,679 round trips, max 11,472 bytes.
Normal and saved logs are each byte-identical to their respective baselines:
every stub/unsupported/counter row is unchanged. Host consistency does not prove
upstream whole-run parity. Exact-head host/3DS CI is required separately; skipped
conditional Task 63 jobs and hardware acceptance are not credited.

## Reproduction

Evidence: `/workspace/validation/lane-a/five/field-effect`; baseline main `766bd05`.
Node commands execute inside the isolated pinned upstream-lane-a checkout.

```sh
. /workspace/shared/activate-parity.sh
make -C battle-core -j2
# Repeat for seeds 777, 9001, 4242, 31337:
FIELDITEM_N=2000 FIELDITEM_SEED=777 FIELDITEM_OUT=/workspace/validation/lane-a/five/field-effect/field-777.txt node node_modules/vitest/vitest.mjs run test/tests/zz-dump/field-effect.producer.test.ts --maxWorkers=1
TURNTEST_LOCALES=/workspace/shared/upstream-lane-a/locales/en battle-core/build/fielditemtest /workspace/validation/lane-a/five/field-effect/field-777.txt
# Established modes, same seeds/counts; repeat TURN_ARENA=1 instead:
TURN_WEATHER=1 TURN_N=2000 TURN_SEED=777 TURN_OUT=/workspace/validation/lane-a/five/field-effect/WEATHER-777.txt node node_modules/vitest/vitest.mjs run test/tests/zz-dump/turntest.producer.test.ts --maxWorkers=1
WFORMS_N=2000 WFORMS_SEED=777 WFORMS_OUT=/workspace/validation/lane-a/five/field-effect/forms-777.txt node node_modules/vitest/vitest.mjs run test/tests/zz-dump/weatherforms.producer.test.ts --maxWorkers=1
make -C game-core -j2 test
./game-core/build/gcplay --auto 1000 --jobs 8
./game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

Each retained file replays through its unchanged owning baseline/final binary;
turn/weatherform binaries use TURNTEST_LOCALES, raw files use build/difftest.
Source caller mutations use the strict final driver. Selection diagnostic builds
field-effect-replay.inc directly against the mutated library using GCC C99,
`-O2 -ffp-contract=off`, then uses the same trace and locale environment.
