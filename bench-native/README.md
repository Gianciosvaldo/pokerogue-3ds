# bench-native: how fast the C battle engine runs on a 3DS

A console app that replays whole battle turns recorded inside upstream PokeRogue through `battle-core`'s C turn engine, times every turn, and checks on the console itself that each covered turn still matches upstream line for line. It answers the question the QuickJS benchmark ([`../benchmark/`](../benchmark/)) answered for the JS route: is the logic fast enough on an Old 3DS?

`battle-core` is read-only here. The bench compiles battle-core's own sources and its own tests (`tests/turntest.c`, `tests/difftest.c`) unchanged, and redirects four names in them (`pr_run_turn` or `pr_get_attack_damage` to a timed wrapper, `printf` to the log file, `exit` to a clean abort, `main` to a function the bench calls). See `source/bench_turntest.c` and `source/bench_difftest.c`.

## What is timed
- **One turn** = one `pr_run_turn` call: TurnStartPhase to the end of the turn, with both sides' commands already given. That covers move order, every move and its effects, abilities, statuses, tags, weather and terrain, fainting and the end-of-turn phases, including the message and animation phases the engine queues (it doesn't draw them).
- **Not in the turn time:** enemy AI (not ported), command menus, switching (not ported), drawing, audio, reading and checking the test records.
- **Single battles only.** The engine doesn't do doubles yet, so there is no doubles number.
- **Covered** means the turn touched nothing the port lacks. Uncovered turns still run (unported steps are skipped and reported), so their times are slightly low; the table gives both.
- **Damage calculation:** each `pr_get_attack_damage` call in the damage test is timed on its own. Upstream's enemy AI (`getNextMove`) runs one simulated damage calculation per attack move and opposing target, so this sizes that part of the AI.
- The turns: 400 per kind, 8 kinds (`plain` and the 7 turntest modes `RICH`, `STATUS`, `ONHIT`, `WEATHER`, `CALLED`, `PROTECT`, `ARENA`, which aim at the harder parts of the engine), seed 777, plus 1,000 damage cases. `romfs/cases/MANIFEST.txt` records the battle-core commit they came from.

Also reported: the one-time startup step (linking the hand-ported closures; the data tables are compiled into the binary, so nothing else loads), the first turn after launch, the engine's state sizes, the deepest stack one turn uses (a second, untimed pass paints the stack before each turn), and heap use.

## Results so far (battle-core `b4e13d0`)
| Machine | Turn median | p95 | Worst | Damage calc median | Startup |
|---|---|---|---|---|---|
| PC (Xeon, gcc -O2, host build) | 0.025 ms | 0.042 ms | 0.39 ms | 0.0016 ms | 0.5 ms |
| Azahar emulator, Old 3DS mode | 0.64 ms | 0.88 ms | 1.66 ms | 0.046 ms | 16.7 ms |
| Real Old 3DS | not measured yet | | | | |

Covered turns (2,849 of 3,200) and all 1,000 damage cases matched upstream in both runs, so the ARM build computes the same results as the PC. Averages 16.9 phases per turn. Memory: `pr_battle` 12.6 KB, `pr_engine` 45.6 KB, deepest stack in one turn 3.7 KB, and the engine allocates nothing. On ARM, battle-core is 571 KB of code and tables (424 KB of it the generated data) plus 21 KB of static data.

For comparison, the QuickJS route ran singles turns at 375 ms median in the same emulator and 1.21 s on the real console (upstream's "turn" there also includes the AI, the command phase and the test harness). Azahar doesn't model cache misses, and the real console ran the QuickJS bench about 3.1x slower than the emulator; whether native code shows the same gap is unknown until the console run.

## Building
```sh
# 1. an upstream checkout (once), then the recorded cases (after every battle-core change)
../battle-core/tools/setup-upstream.sh /path/to/upstream
tools/make-cases.sh /path/to/upstream          # [N per kind=400] [seed=777] [damage cases=1000]
# 2. the .3dsx (cases go into its romfs), with devkitARM in docker
docker run --rm -v "$PWD/..":/src -w /src/bench-native devkitpro/devkitarm make BENCH_REV=$(git rev-parse --short HEAD)
# 3. optional: the same bench on the PC
make -f Makefile.host && build-host/bench romfs/cases
```
The recorded cases come from upstream and stay out of git (`romfs/` is ignored); the built `.3dsx` carries them and must not be shared. If the case format and battle-core drift apart, the bench reports parse mismatches instead of timings.

## Running on the console
Copy `pokerogue-native-bench.3dsx` to `sd:/3ds/`, start it from the Homebrew Launcher and wait for **Done** (about 2 minutes in the emulator; the recorded turns are read and checked between the timed calls, which is most of that). It writes `sd:/3ds/pokerogue-native-bench-result.txt` (the numbers) and `sd:/3ds/pokerogue-native-bench-log.txt` (the full test output). On a New 3DS it leaves the speedup off, so it runs at the Old 3DS clock; the result file names the model.
