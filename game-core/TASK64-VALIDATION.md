# Task 64: bounded wild enemy moveset integration

Baseline: `04b6c71a74692ac9ba2845c31ff940462ec9c9ca`.
Branch: `outside/enemy-movesets-integration`. Target remains upstream
`8555c08c823b856cbec4eb99ca84ea52a955836d`. GCC 14.2.0, GNU Make 4.4.1.

## Scope and remaining gates

The actual wild constructor calls `pr_run_enemy_moveset` between run-core's base
and finish draws. Supported clean Classic/Endless inputs use the run RNG and
Task 63's move order and PP. Constructor stats are calculated on a copy without
the previous battle's modifiers. Unsupported inputs leave that RNG and mon
unchanged and enter the existing counted last-four-level-moves fallback.

This is a **first slice**, not completion of Task 64 or whole-enemy upstream parity.
Task 63 supports Caterpie/Weedle, Magikarp below level 25, Ditto, and fixed
Smeargle/Eternatus dispatch. Its restrictions on ability/passive state and pools
still apply. Trainer construction restores its member seed-offset streams before
returning snapshots; those boundaries must be exposed by the run-core owner
before this adapter can use them. Trainer, Daily/custom, challenge and fusion
constructors remain deferred. Tasks 65/66 are not unlocked by this partial slice.

Constructor refresh now preserves PP Ups and maximum-PP overrides along with
move IDs. Unsigned battle PP-Up storage represents upstream Eternatus Recover's
-4 PP Ups through the existing max-PP override (1 PP). A direct adapter/save
fixture checks this representation; no final-boss form transition is claimed.

The attached October 4 roadmap is planning context, not current implementation
evidence. The current scheduling inventory still has Task 96's remaining item
families before full Task 64. Several families need reward-core application APIs;
this bounded integration uses the prerequisite API already merged in Task 63.

## Required host validation

All commands below ran from `/workspace/pokerogue-3ds`. Full captured logs are
in `/tmp/task64-{baseline,final}-{test,soak,saved}.log` in this task's machine.

```sh
make -C game-core -j4 test
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

Both baseline and final CI host suites passed. Established checks were not edited.
Final additive adapter suite: **29,493 checks**, 1,125 supported and 975 rejected
matrix cases; 800 real wild constructors, 6 supported (all 6 consume moveset RNG)
and 794 counted fallback cases. It checks move order, maximum PP, PP Ups, RNG,
constructor finish outputs, no mutation on rejection, stale-modifier isolation,
and save/load persistence. A temporary mutant generating moves on a detached RNG
fails the RNG comparison (exit 1); no mutant or upstream data is committed.

| Check | Baseline | Final |
|---|---|---|
| CI 200-run normal/saved digests | `be82fcf1` / `be82fcf1` | `a6662b64` / `a6662b64` |
| 1,000-run normal/saved digests | `f05a2ed4` / `f05a2ed4` | `5f646aee` / `5f646aee` |
| Ended / cleared, each path | 1,000 / 0 | 1,000 / 0 |
| Stuck / crashed / timed out, each path | 0 / 0 / 0 | 0 / 0 / 0 |
| Turns, each path | 49,324 | 49,315 |
| Moveset fallback calls, each path | 15,025 | 14,716 |

Final saved path: 14,305 round trips, largest save 11,536 / 131,072 bytes.
Digest changes are expected: supported constructors now consume actual moveset
draws and use generated moves, altering subsequent draws and battle choices;
their fallback counters are also removed from the unchanged digest input. The
fallback count is zero in supported constructor fixtures, not across all runs.
All 1,000 runs still use unsupported movesets somewhere. Full soak counter
reports remain in the logs, including fixed-battle and other remaining stubs.

## Additional app checks and explicit coverage failure

```sh
make -C game/tests -j2 runtest saves_test flowtest
game/tests/build/saves_test
game/tests/build/flowtest
game/tests/build/runtest 1000 0
game/tests/build/runtest 5000 0
```

Save slots: 68 checks, 0 failed. Flow: 45 checks, 0 failed.
The 1,000-run app check **failed** its rare-evolution coverage floors: only
1 completed evolution and 1 post-evolution move, below the existing 5/3 thresholds;
6 cancellations met the other floor. All 1,000 runs ended, zero stuck, screen/run
mismatches and switch-ins missing; digest `94c70521`. The chooser, seeds and
assertions were preserved. This failure remains reported, not counted as a pass.

The expanded 5,000-run check passed: 5,000 game-over screens, 15 completed
evolutions, 31 cancelled, 15 post-evolution moves, zero stuck, screen/run
mismatches, switch-ins missing or model violations; digest `e934ec4d`.
This establishes host coverage for the changed trajectories, not upstream parity
or graphical/console acceptance.

No new upstream differential, emulator run or 3DS build was performed. This
adapter compares against the merged run-core API; its upstream evidence remains
Task 63's recorded differential scope. GitHub CI results are unavailable until
API access is restored. Do not mark this branch merged or CI-validated from local
host results alone.
