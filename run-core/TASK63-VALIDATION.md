# Task 63 validation — 2026-10-05

The final R regression runs in the `Task 63 full regression` workflow on PR #112's head; its results are recorded in the PR. This is a bounded enemy moveset implementation, not complete enemy moveset parity.
Original baseline source: main `a3f44d3403c1da26bf4438dc2f598a0b389be017`.
The final candidate incorporates current main `68a60c94527064440b33f3beb321dc6748ef5123` unchanged.
Upstream pin: `8555c08c823b856cbec4eb99ca84ea52a955836d`.
Node 22.23.3; GCC 13.3.0; Vitest 4.1.11. The existing external harness was reused;
setup-upstream was not rerun. Host tests and CI build success do not prove console behavior.

## Changes and authorization

The user explicitly authorized overriding the former Task 63 restrictions to repair its blockers.
No comparator, tolerance, seed or required count was weakened. Port changes are in run-core;
Task 73 files are incorporated unchanged from main. Game-core wiring remains Task 64.

Existing baseline repairs:

- Daily producer clears IVS/NATURE/SHINY overrides left by classic test setup. Daily C form selection uses the active daily RNG at offset zero.
- C EnemyPokemon constructor uses the ordinary boss segment calculation. The special Eternatus segments belong to the later EncounterPhase.
- Gym/E4 snapshot stub initializes an empty moveset for trainer post-process callbacks, consuming no moveset rolls and preserving the first snapshot.
- Trainer-party producer retains the first constructor snapshot; later rival regeneration calls formerly overwrote it with changed ability/form/Tera traits.
- Rival, gym/E4 and trainer-party producers clear mock call history and detach destroyed battle-info objects from the mocked UI after complete scalar observations. MockContainer.destroy clears children but does not remove itself from its parent, unlike real Phaser destruction. Implementations, random inputs and case counts remain unchanged.
- Rival producer releases each fight's display objects after its observations, and the gym/E4 producer gives the arena its biome's trainer pool. See [Takeover repairs](#takeover-repairs-2026-10-05).

No upstream source, JSON dumps, art, assets or binaries are committed. Generated tables were read-only;
redump was neither needed nor run.

## New differential suite

Producer: `run-core/tests/moveset.producer.test.ts`. It calls the real upstream method on real
Pokemon objects. Flags: `DIFF_MOVESET=1`, `DIFF_N`, `DIFF_SEED`, `DIFF_OUT`.
Trace: `$BUILD/moveset-$SEED.txt`. C comparator: `$BUILD/movesettest TRACE N SEED`.
Move IDs, order, PP, PP Ups and post-case RNG state are compared; strict completion footer required.

Supported: Caterpie/Weedle form 0 level-only wild enemies and trainer levels 1–24;
Magikarp form 0 levels 1–24; Ditto form 0 wild enemies and trainer levels 1–79;
fixed Smeargle and Eternatus forms 0/1, including inverse handling. Native abilities only,
no passive/fusion, clean constructor boundary, with effective ATK/SPATK supplied by the caller.

General TM/egg/signature paths, evolution/prevolution/relearn merges, superseding/useless-move
rules, broader species/forms, modified ability state, fusion, Tera and other challenges remain
unported. Unsupported input returns a named reason with unchanged RNG/output. The bounded
species do not need those general paths; no broader parity is inferred from existing snapshot suites.

Working directory: `/workspace/scratch/809689da2702/task63-resume`.
PATH starts `/root/.npm/_npx/d18f28baf1132559/node_modules/node/bin`.

```sh
BUILD=/workspace/scratch/809689da2702/validation/task63-moveset-current sh run-core/tools/movesetdifftest.sh /workspace/scratch/809689da2702/upstream-reference 2000 777
BUILD=/workspace/scratch/809689da2702/validation/task63-moveset-current sh run-core/tools/movesetdifftest.sh /workspace/scratch/809689da2702/upstream-reference 2000 9001
BUILD=/workspace/scratch/809689da2702/validation/task63-moveset-current sh run-core/tools/movesetdifftest.sh /workspace/scratch/809689da2702/upstream-reference 2000 4242
BUILD=/workspace/scratch/809689da2702/validation/task63-moveset-current sh run-core/tools/movesetdifftest.sh /workspace/scratch/809689da2702/upstream-reference 2000 31337
```

All exit 0:

| Seed | Cases | Identical | Mismatches | Forced STAB | Soft rejections |
| --- | ---: | ---: | ---: | ---: | ---: |
| 777 | 2000 | 2000 | 0 | 593 | 117 |
| 9001 | 2000 | 2000 | 0 | 603 | 105 |
| 4242 | 2000 | 2000 | 0 | 603 | 111 |
| 31337 | 2000 | 2000 | 0 | 610 | 112 |

Each seed additionally prints: wild 1002, trainer 998, boss 996, level 1334, fixed 666,
Eternatus0 168, Eternatus1 165, inverse 165. Each host-only unported probe (species, form,
level-pool, tm-egg-pool, ability-state, fusion, tera, challenge, invalid) prints
`cases 1 reported 1 failures 0`. Those nine probes per seed are **not** differential matches.

After integrating newer main, each trace was replayed through the rebuilt candidate:

```sh
run-core/build/movesettest ../validation/task63-moveset-current/moveset-777.txt 2000 777
run-core/build/movesettest ../validation/task63-moveset-current/moveset-9001.txt 2000 9001
run-core/build/movesettest ../validation/task63-moveset-current/moveset-4242.txt 2000 4242
run-core/build/movesettest ../validation/task63-moveset-current/moveset-31337.txt 2000 31337
```

Again each prints 2000 cases / 2000 identical / 0 mismatches, with the same path counts.

## Takeover repairs (2026-10-05)

Two producer defects stopped required R commands. Both are card 100 work (lorenzo's producer-edit
authorization in docs/HANDOFF.md). No comparator, C source, seed, count or tolerance changed.
Local runs: Node 22.22.0, GCC 13.3.0, upstream `8555c08` prepared by `battle-core/tools/setup-upstream.sh`.

**Rival: leaked display objects.** `MockContainer.destroy` only empties its own list: it neither
destroys its children (Phaser's Container does) nor leaves its parent, and `scene.field`,
`scene.spriteTeraSparkleHandler` and the mock factory's `textures.list` keep references too. Every
fight's EnemyPokemon (26 per input run) and about 78 sprites stayed alive, and each sprite's
animation listener stayed on the scene's animation manager (807 after setup, +468 per input run),
so every later destroy scanned a longer list. Time grew with the square of `DIFF_N` (100, 200, 300,
400 runs: 24, 112, 273, 518 s cumulative) and memory without bound; four 3000-run seeds started
together were killed by the kernel's out-of-memory killer. After its observations and with no RNG
draw, the producer now collects each fight's display subtrees, removes the Pokemon from the field,
destroys the collected objects, drops them from the sparkle handler and truncates the factory list.
Objects still alive after 50 and 150 runs (seed 9001, `v8.queryObjects` after a forced GC): with
the child destroys alone, EnemyPokemon 1302 and 3902 and Phaser sprites 30850 and 90849; with the
whole repair, 3 and 3 and 869 and 869.

```sh
sh run-core/tools/difftest.sh $UP 500 777 rival
```

Unchanged-main producer 868 s, PR producer before this repair 829 s, after it 48 s (wall times on
a shared 4-core machine). Each prints
`rival summary: 3000 fights, 0 mismatches`, and the three traces are byte-identical (SHA-256
`88119837e3f91a1cfbc83a29f24de4f07345bc133aa46951d9aebbbbd1a758b8`).

At the required count, through the CI driver (`TASK63_RESULTS=<dir> python3
run-core/tools/task63-regression.py $UP <main a3f44d3 checkout> <seed> rival`, which runs
`difftest.sh $UP 3000 <seed> rival`, the unchanged-main C replay and the species mutation), seeds
777, 9001, 4242 and 31337 each print `rival summary: 18000 fights, 0 mismatches` for the candidate
and for the unchanged-main replay, and `18000 fights, 18000 mismatches` (exit 1) for the mutant.
Each producer run took about 2 to 3 minutes; peak resident memory was about 650 MB.

**Gym/E4: wrong trainer pool.** The producer set `scene.arena.biomeId`, but only the Arena
constructor copies a biome's trainer pool, so every gym wave drew from Town's pool (Youngster only,
no boss tier). Once the crash repair let the suite run, `difftest.sh $UP 3000 777 gym_e4` printed
`3000 configurations, 1546 fully matched, 9120 matched members, 1454 mismatches`, every one
`expected 63` (Youngster). The producer now also sets `scene.arena.trainerPool` from
`allBiomes.get(biome)`, which draws no RNG. Upstream now picks real gym leaders, so this trace
changes by design. 300 configurations on seed 777: 300 fully matched, 0 mismatches.
The gym/E4 producer also gets the rival producer's display cleanup: with the trainer-pool repair
alone, four 3000-configuration seeds started together were killed for memory. On 300
configurations (seed 777) the cleaned producer's trace is byte-identical to the uncleaned one.
The 3000-configuration results for all four seeds are those of the CI regression on PR #112.

## Harness cleanup equivalence

The old and repaired rival producers each ran 50 input runs on seed 777 (300 fights)
in a private harness. This additional check does not replace the required 3000-run suite.
Both exits were 0; complete traces are byte-identical, SHA-256
`3f5353a4c20fcb65b52b83dee50bf9daaf272b76ed85b807e39c1abe2c6bd3f2`.
The command for each producer copy, from `/workspace/scratch/809689da2702/upstream-r63-c`:

```sh
DIFF_N=50 DIFF_SEED=777 DIFF_OUT=/workspace/scratch/809689da2702/validation/r63-cleanup-equivalence/before.txt npx vitest run test/tests/zz-dump/rival-cleanup-control.producer.test.ts
DIFF_N=50 DIFF_SEED=777 DIFF_OUT=/workspace/scratch/809689da2702/validation/r63-cleanup-equivalence/after.txt npx vitest run test/tests/zz-dump/rival-cleanup-control.producer.test.ts
```

Only the copied producer changed between commands; the pin, input generator and assertions did not.

The full trainer-party seed-777 cleanup check also completed:

```sh
BUILD=/workspace/scratch/809689da2702/validation/r63-cleanup-full sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-r63-a 3000 777 trainerparty
```

Exit 0: cases 3000, double 1630, whole parties matched 2525, upstream member throws 106,
fused members 1793; `pass 9662 fail 0 not covered 1792`.
The full before/after traces are byte-identical, SHA-256
`45f59e079669aba4a1b3884ace69034ce00aba5ae0f478ac4d3808b8aeeedb99`.
The before counts and supported cases are retained.

## Negative controls

From `/workspace/scratch/809689da2702`, a temporary C mutant adds one to generated PP:

```sh
gcc -Itask63-resume/run-core/include -Itask63-resume/battle-core/include -Itask63-resume/battle-core/gen/out -O2 -std=c99 -Wall -Wextra -ffp-contract=off task63-resume/run-core/tests/movesettest.c validation/task63-moveset-mutant.c task63-resume/run-core/build/librun.a task63-resume/run-core/build/battle-core/libprcore.a -lm -o validation/r63-integrated-moveset-mutant
validation/r63-integrated-moveset-mutant validation/task63-moveset-current/moveset-777.txt 2000 777
task63-resume/run-core/build/movesettest validation/task63-moveset-truncated.txt 2000 777
```

Build exit 0. Mutant exit 1: `cases 2000 identical 0 mismatches 2000`.
The copied trace with END removed exits 2: `missing completion footer`.
Neither mutant nor trace is committed.

## R baseline and final regression

[Completed commands, before/after C summaries, integrated replays and explicit coverage counts](TASK63-R-REGRESSION.md). The full matrix is not yet complete; this checkpoint does not claim an R pass.
That report distinguishes fresh final upstream production from paired unchanged-main C replays;
it does not claim all original-main producers completed a fresh before matrix.

Fresh unchanged-main attempts, Node 22, working directory `../task63-baseline`:

```sh
BUILD=/workspace/scratch/809689da2702/validation/task63-before-current/777 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-reference 3000 777 wildenemy
BUILD=/workspace/scratch/809689da2702/validation/task63-before-current/777 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-reference 3000 777 trainerparty
BUILD=/workspace/scratch/809689da2702/validation/task63-before-current/777 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-reference 3000 777 gym_e4
BUILD=/workspace/scratch/809689da2702/validation/task63-before-current/777 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-reference 20000 777 wave
BUILD=/workspace/scratch/809689da2702/validation/task63-before-current/777 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-reference 20000 777 shop
```

Wild exit 1: 2999 pass / 1 fail / 0 not covered, 3000 cases; Eternatus line 1624.
Trainer-party exit 1: 9651 pass / 11 fail / 1792 not covered; 3000 cases,
1630 double, 2518 whole parties, 106 upstream member throws, 1790 fused.
Gym/E4 exit 1 before comparison: upstream TypeError from the snapshot stub's missing moveset.
Wave and shop exit 0; each existing component printed 20000 pass / 0 fail / 0 not covered.
A following biome attempt was interrupted and is not counted as completed.

The original wild trace replayed through the repaired C comparator prints 3000 pass / 0 fail /
0 not covered: bosses 357 vs 356 before, locked 181 unchanged, Bug Net 998 vs 997,
fusions 605 unchanged. The failing case was repaired, not classified unported.

Local stale build artifacts caused permission errors in an initial integrated game-core attempt
and a seed-9001 wave attempt. Clean rebuilds and complete reruns resolved them; no source or
comparator bypass was used. Those failed attempts are not counted as passes.

## Host validation and remaining limits

```sh
make -C run-core
make -C game-core clean
make -C game-core test
make -C run-core test-daily-rng
git diff --check
```

Final host checks exit 0. Game-core: reward lifecycle host 46, resume 35981, item bridge 39,
reward adapter 3512, switch prompts 25, turn tail 182, boss segments 152, reward apply 638355;
all zero failures. Saved/unsaved digest `8decbf9d` / `8decbf9d`, unchanged from the original
main host baseline. Daily RNG: 27 passed / 27 checks / 0 failed.

The remaining moveset gap is the explicitly unported general domain listed above and Task 64
integration. No complete enemy moveset parity, unavailable differential check, or console behavior
is claimed. GitHub CI build results are separate from upstream differential validation.
