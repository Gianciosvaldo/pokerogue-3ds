# Critical held-item caller proof and raw helper regression repair

Target upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, Node 22.23.3.
This promotes existing Scope Lens (+1 stage) and Leek (+2 stage) critical-stage
callers for canonical real stack 1, virtual 0. Leek eligibility covers Farfetch'd,
Galarian Farfetch'd and Sirfetch'd, form 0. Lucky Punch has no factory at this pin.
No new critical formula, class, fields, acquisition or save lifecycle is added.
Fusion eligibility is unproved and refused by full-turn admission. The original
raw SpeciesCrit helper's primary-species behavior is retained, not expanded or
claimed as fusion parity. Scope Lens does not require a species check.

The additive producer drives real MoveEffectPhase, item shouldApply/apply and
getCriticalHitResult. Both sides, ordinary/high/always-critical attacks,
Battle Armor, blocked eligibility/owner, wildcard/order, fixed/status attacks,
three eligible bird species and stages 0–3 are compared in full state, RNG,
phase queues and messages. Ten states each execute 200 cases per seed.
Each seed records Scope applies800, Leek applies800, shouldApply refusals400,
stage counts400/400/600/400, species680/660/660. Crit outcomes for seeds
777/9001/4242/31337 are924/924/941/942. Each source replay compares2000/2000
covered turns and texts exactly, with0 parse/stat/turn/text mismatches.

## Raw helper regression found during validation

The unchanged legacy producer creates real type stacks1–99, Soul stacks1–10,
and critical stacks1, and sometimes adds1–2 virtual stacks independently.
Upstream raw apply helpers still perform their arithmetic beyond acquisition
caps. Prior Soul #164 and type #169 strict helper admission incorrectly removed
that raw projection coverage; their bounded full-turn suites had missed this.
Seed777/N2000 at pre-Soul main:2000 covered/pass,0 stats mismatches/uncovered.
Post-Soul/pre-type:1990 covered/pass,7 all-case stat mismatches,10 uncovered.
Post-type/pre-critical:1989 covered/pass,7 stats,11 uncovered.
Unmerged strict critical helper:1940 covered/pass,7 stats,60 uncovered.
No producer, comparator, established strict guard, tolerance or exclusion changed.

Explicit `pr_project_*` helpers now preserve original upstream raw arithmetic and
flags for damage/stat callers; strict `pr_apply_*` entrypoints retain their
bounded contract. Full-turn unsupported inventory admission stays in phases.c.
The unchanged seed 777 fixture is restored to 2000 covered/pass, 0 stat/carried-state
mismatches/uncovered. All four legacy seeds now match pre-Soul output byte for
byte: covered/pass 2000/2000/2000/1999, 0 stat/carried-state mismatches and
0 uncovered. Seed 31337 retains the producer's existing excluded upstream Fusion
Flare throw; no new exclusion was added. Strict Soul16/type20/critical24 guards still pass. Three
additional engine checks prove fused Scope allowed and fused Leek (matching or
wildcard holder) refused, while raw stage projection has no unsupported flag.
This repair does not claim full-turn support for over-cap or fused inventory.

## Commands and evidence

```sh
. /workspace/shared/activate-parity.sh
make -C battle-core -j2
# Repeated for seeds 777, 9001, 4242 and 31337 with corresponding SEED/OUT:
CRITITEM_N=2000 CRITITEM_SEED=777 CRITITEM_OUT=/workspace/validation/lane-a/ten/crit/crit-777.txt node node_modules/vitest/vitest.mjs run test/tests/zz-dump/critical-item.producer.test.ts --maxWorkers=1
TURNTEST_LOCALES=/workspace/shared/upstream-lane-a/locales/en battle-core/build/crititemtest /workspace/validation/lane-a/ten/crit/crit-777.txt
# Unchanged legacy producer/comparator, four seeds:
DIFF_N=2000 DIFF_SEED=777 DIFF_OUT=/workspace/validation/lane-a/ten/crit/legacy-777.txt node node_modules/vitest/vitest.mjs run test/tests/zz-dump/difftest.producer.test.ts --maxWorkers=1
battle-core/build/difftest /workspace/validation/lane-a/ten/crit/legacy-777.txt
make -C game-core -j2 test
./game-core/build/gcplay --auto 1000 --jobs 8
./game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

Actual commands use separate flag arguments (`--auto 1000 --jobs 8`); Node
commands run from isolated upstream-lane-a. Evidence lives outside Git at
`/workspace/validation/lane-a/ten/crit`. The36 retained before/after full-turn
pairs (plain/rich/switch/summon, ITEMS/COND and Wide/Reviver/Multi/Golden/Soul/type)
are byte-identical with retained covered counts,0 mismatches. The14 malformed
input variants (fractional/incomplete/invalid identity/cap) all exit2.

Actual source mutations disconnect the new project callsites: critical damage
caller fails581/2000 covered turns; type caller fails889/2000; nature-stat caller
produces2208 stat mismatches (turns themselves remain2000/2000, strict wrapper
correctly exits1). Leaf guards keep passing, proving caller sensitivity.

Host test exits0. Before/after each normal and saved1000 runs ends1000, clears0,
stuck/crash/timeout0,49206 turns,14291 saves,max11400 bytes. Before digest pair
5022ea67; after pair086babdb. Only the held unsupported report decreases
983 runs/52064 uses to982/52050,14 fewer reports; the digest hashes that report.
Every other log row is unchanged.200-run paired host digest04c33894 remains.
These soaks establish internal consistency, not upstream whole-run parity.
Exact-head host/3DS CI is required separately; conditional Task63 skips are not
credited. Console acceptance and wider ability/form/fusion/doubles gates remain.
