# Stat-item ordering through forms, fusion and Transform

Target: source `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3, GCC 14.2.

Pokemon.calculateBaseStats applies Shuckle Juice's total effects and Old Gateau's
flat effects to the primary species before fusion averaging or unfused Spliced
Endless halving. Vitamins apply afterward. The port previously averaged/halved
first and then applied all three groups. It now separates those stages while
retaining the combined raw projection API for callers without a fusion step.

Old Gateau deliberately chooses its lower-stat pairs from the effective species
form, including Transform, while modifying the permanent species' base-stat
array. Each total/flat instance applies once regardless of stack count; vitamins
use the source's stack multiplier. The patch preserves those pinned details.

The battle inventory admits bounded native total/flat/vitamin records. Total
effects accept 10/-15 and one/two stacks; flat effects accept one stack; vitamins
accept represented stat/stack values and the owning actor's IV cap. Raw API
arithmetic remains available outside acquisition caps. Unknown classes and
invalid battle inventories remain explicitly unsupported.

The game-core bridge accepts explicit native Shuckle Juice arguments 10/-15,
Old Gateau's no-argument value 0 and vitamin stat IDs 0–5. It preserves the classes'
nontransferable flags and represented virtual stacks. It does not infer a
missing juice variant or generated vitamin stat, or add a mystery-event choice.

Validation:

- The initial no-item changeForm matrix matched 768/768 cases across native
  fusion and actual Transform state, so no generic form mutation was invented.
- Adding real source modifier instances exposed 704 failures in 1,536 cases.
  The stage fix makes the identical unchanged capture 1,536/1,536 exact.
- Expanded source changeForm API: 16,384/16,384 exact, 8,192 each at seeds 777/2034.
  Twelve species, both sides, fusion, actual Transform setup, Classic/Spliced
  mode, items, owner/refusal controls, native ability indices and Tera. Full 125
  permanent/temporary/effective vectors and both RNG streams are compared.
  Final captures also observe source item caps and nontransferable flags.
- Complete source native form turns: 8,000/8,000 exact, 2,000 each at
  seeds 777/9001/4242/31337. They cover unfused Classic/Spliced actors and actual
  queued form/stat recalculation with total/flat/vitamin records. All requested
  turns are covered with identical state, RNG, queues and messages; existing
  comparison and tolerances are unchanged.
- Six compiled faults are caught and eleven malformed captures rejected:
  stage reversal, premature vitamins, missing halving, permanent-form Gateau
  selection, incorrectly multiplied total effects and missing inventory admission.
- Game-core bridge/form/legal-save checks: 96 assertions pass. Loaded actor
  fusion, form, HP and stats match; items retain their class/transfer metadata.
  Missing variants, invalid stats and an owned vitamin above its IV cap are
  refused or reported unsupported. The latter uses the run's collected unsupported
  counters, because the bridge drains the battle's temporary counter list.
- Full game-core suite passes; paired ordinary save/no-save digests both
  `99e9513d`. These are consistency results, not upstream run parity.
- Retained 24,000 mixed turns, 72,000 dedicated ability turns and 16,000 Ditto
  cases pass without coverage loss or covered mismatch. Full app host suite passes,
  including 10,000 ordinary runs and ordinary/custom save-resume flows. Host and
  3DS CI are separate publication gates; see the PR checks.

The initial turn fixture selected Spliced mode after calculating its reserves.
Source changeForm subsequently refreshed that party, exposing inconsistent
initial stats. The final source setup selects mode/items before calculating all
party members. The invalid initial capture is excluded; runtime and comparison
rules were not changed to accommodate it.

Reproduce (repeat API seed 2034 and turn seeds 9001/4242/31337):

```sh
battle-core/tools/stat-order-diff.sh /path/to/pinned-upstream api 8192 777
battle-core/tools/stat-order-diff.sh /path/to/pinned-upstream turn 2000 777
TURNTEST_LOCALES=/path/to/pinned-upstream/locales/en \
  python3 battle-core/tools/check-stat-order-proof.py /abs/stat-api.txt /abs/stat-turn.txt
make -C game-core test
ASAN_OPTIONS=detect_leaks=0 JOBS=2 game/tests/run_host_tests.sh
```

Source gameplay methods, expectations and retained exclusions remain unchanged.
Producers use only additive UI/asset/timing transport stubs. Local app tests
disable LeakSanitizer for the ptrace environment; normal CI settings remain.

API fusion/Transform coverage does not establish complete fused move turns or
fused display-name parity. Native registered callbacks in those contexts, item/
form eligibility, Challenge FLIP_STAT, wide levels and other audit slices remain
separate. No run-core change, emulator or hardware validation is claimed.
