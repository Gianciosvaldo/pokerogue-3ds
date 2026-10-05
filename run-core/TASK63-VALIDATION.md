# Task 63 validation checkpoint — 2026-10-05

Still draft. This is a bounded implementation, not complete enemy moveset parity.
Base source: main `a3f44d3403c1da26bf4438dc2f598a0b389be017`.
Pinned external upstream: `8555c08c823b856cbec4eb99ca84ea52a955836d`.
Node 22.23.3; GCC 13.3.0; Vitest 4.1.11. Existing external harness reused;
setup-upstream was not rerun. Host results do not prove console behavior.

## Changes and authorization

The user authorized overriding the previous Task 63 restrictions to resolve its
blockers. No comparison, tolerance, seed or required count is loosened.
Existing daily correction is retained. Three further baseline repairs:

- C constructor boss segments use ordinary encounter calculation; special
  Eternatus segments belong to the later EncounterPhase.
- gym/E4 snapshot stub initializes an empty moveset for trainer post-process
  callbacks; it consumes no moveset RNG and preserves the first snapshot.
- Rival producer releases mock call history after each fight; mock implementations
  and generated cases are unchanged.
- Trainer-party producer retains the first constructor snapshot; later rival
  post-process regeneration calls previously overwrote it with changed traits.

All edits are in run-core. Other cores and game-core wiring are unchanged.
No upstream source/dumps/assets or binaries are committed. Generated tables
are read-only; redump was not needed and was not run.

## Commands and measured outputs

Working directory unless noted: `/workspace/scratch/809689da2702/task63-resume`.
Upstream: `/workspace/scratch/809689da2702/upstream-reference`.
PATH begins `/root/.npm/_npx/d18f28baf1132559/node_modules/node/bin`.

```
make -C ../task63-baseline/run-core
```
Exit 0. Baseline cores/docs were fetched at the main SHA above; their source
content is main, while the local Git commit is a snapshot rather than remote history.

```
BUILD=/workspace/scratch/809689da2702/validation/task63-moveset-current sh run-core/tools/movesetdifftest.sh /workspace/scratch/809689da2702/upstream-reference 2000 777
```
Exit 0:

```
moveset seed 777: cases 2000 identical 2000 mismatches 0
paths: wild 1002 trainer 998 boss 996 level 1334 fixed 666 STAB 593 rejection 117 Eternatus0 168 Eternatus1 165 inverse 165
unported guard species: cases 1 reported 1 failures 0 (host only)
unported guard form: cases 1 reported 1 failures 0 (host only)
unported guard level-pool: cases 1 reported 1 failures 0 (host only)
unported guard tm-egg-pool: cases 1 reported 1 failures 0 (host only)
unported guard ability-state: cases 1 reported 1 failures 0 (host only)
unported guard fusion: cases 1 reported 1 failures 0 (host only)
unported guard tera: cases 1 reported 1 failures 0 (host only)
unported guard challenge: cases 1 reported 1 failures 0 (host only)
unported guard invalid: cases 1 reported 1 failures 0 (host only)
```

The remaining seeds (9001, 4242, 31337) are running; no result is claimed until
recorded. All unported guard counts above are host-only probes, not differential
cases: they check reason, unchanged RNG and unchanged output.

Mutation command, repository workspace working directory:

```
gcc -Itask63-resume/run-core/include -Itask63-resume/battle-core/include -Itask63-resume/battle-core/gen/out -O2 -std=c99 -Wall -Wextra -ffp-contract=off task63-resume/run-core/tests/movesettest.c validation/task63-moveset-mutant.c validation/task63-moveset-current/librun.a validation/task63-moveset-current/battle-core/libprcore.a -lm -o validation/task63-moveset-mutant
validation/task63-moveset-mutant validation/task63-moveset-current/moveset-777.txt 2000 777
```
Temporary mutant adds one to generated PP. Build exit 0; comparator exit 1:
`moveset seed 777: cases 2000 identical 0 mismatches 2000`.

```
validation/task63-moveset-current/movesettest validation/task63-moveset-truncated.txt 2000 777
```
Trace copied with END footer removed: exit 2, `missing completion footer`.
Neither mutation nor trace is committed.

```
make -C game-core test
git diff --check
```
Both exit 0. Game-core: resume 35,981 checks; item bridge 39; reward adapter
3,512; switch prompts 25; turn tail 182; boss segments 152; reward apply
638,355, all zero failures. Paired digest `8decbf9d` / `8decbf9d`.

Before, unchanged main, Node 22, workspace/task63-baseline:

```
BUILD=/workspace/scratch/809689da2702/validation/task63-before-current/777 sh run-core/tools/difftest.sh /workspace/scratch/809689da2702/upstream-reference 3000 777 wildenemy
```
Exit 1: 3000 cases, 2999 pass / 1 fail / 0 not covered; Eternatus line 1624.
356 bosses / 181 locked / 997 Bug Net / 605 fusions matched.

After correction, same trace and unchanged comparator:

```
make -s -C run-core BUILD=/workspace/scratch/809689da2702/validation/task63-repaired-current
../validation/task63-repaired-current/wildenemytest ../validation/task63-before-current/777/wildenemy-cases.txt
```
Both exit 0: 3000 cases, 3000 pass / 0 fail / 0 not covered.
357 bosses / 181 locked / 998 Bug Net / 605 fusions matched.
Coverage increased by the repaired case; none was reclassified unported.

## Remaining gates

Complete R-before and R-after matrices are running/not yet complete. No blanket
R pass is claimed. Required counts remain 20000 for wave/shop/biome/encounter/
newbattle/exp/partyexp/upkeep/trainerspecies/luck/daily; 10000 battlegen; 8000
pokemongen; 3000 trainerparty/rival/gym_e4/wildenemy/enemyitems; 2000 enemybuffs,
all four seeds. Challenge and README DIFF_FOCUS=1 runs are still pending.
Daily's actual scope is five fixed runs / 250 waves / 775 checks, regardless
of the command's N and seed; it is never counted as 20000 random cases.

The full per-seed table, baseline summary lines and producer repair results
will replace this checkpoint after completion. No 3DS build is claimed.
