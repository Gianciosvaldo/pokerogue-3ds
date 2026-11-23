# The app half of an encounter's one save, at its first question

Source: `8555c08c823b856cbec4eb99ca84ea52a955836d`. game-core's half (what is saved, how it is loaded, the host test, the soak and the 25 planted faults) is in
`game-core/ME-SAVE-VALIDATION.md`. lorenzo's decision: **save once, when the encounter's question screen shows**, on a still screen, never during animations; the
encounter's later screens and its fight are not saved.

## What the app does

- `run_screen.c`: `mystery_screen()` calls `save_point()` before it sets the question up. `save_point()` is the one place the run is written while the scene is up: it
  waits (a 50 ms timer) while the UI is busy or a battle scene plays, so nothing is written during an animation, and it writes only when game-core says
  `gc_run_can_save`. `submit()` still writes a postponed save before the first answer if no timer was free.
- A loaded encounter question comes back through `rs_resume`, which skips the loaded `SAVE_POINT` (the slot already holds this run) and shows the question again
  from game-core's queue (`RP_MYSTERY`); nothing is summoned, as in a fresh encounter.
- No game-core, battle-core or run-core change in this half.

## Tests (`game/tests`)

| What | Command | Result |
|---|---|---|
| Whole host suite on the head, clean build | `rm -rf game/tests/build game-core/build && JOBS=2 game/tests/run_host_tests.sh` | `game/tests: all passed`; 23 runtest runs, every one with 0 model violations |
| Every encounter forced once per run (`RUNTEST_ME=cycle`, Fiery Fallout skipped, see below), system data round trip, saved at the question and continued from the slot | `RUNTEST_ME=cycle RUNTEST_SYS=1 RUNTEST_SAVE=resume build/runtest 300 0` | 268 questions saved, 63 resumed from the slot, 29 kinds saved, 25 continued |
| The same with "Will you switch?" on | `RUNTEST_ASK_SWITCH=1 RUNTEST_ME=cycle RUNTEST_SYS=1 RUNTEST_SAVE=resume build/runtest 300 0` | 266 saved, 67 resumed, 29 kinds, 25 continued |
| The same in custom runs (all 31 encounters reach their question there) | `RUNTEST_CUSTOM=strong RUNTEST_ME=cycle RUNTEST_SAVE=resume build/runtest 100 0` | 183 saved, 44 resumed, 31 kinds, 22 continued |
| Planted fault: the question screen does not call `save_point()` | `ME_SAVE_FAULT_RUNS=300 python3 game/tests/check_me_save_faults.py` | unchanged tree passes 300 runs in 2 configurations; `question-not-saved` caught; 1 of 1 |

The runtest accounting counts a save point at the encounter's question, requires the slot to hold the run when the question is asked, and checks that a loaded
question is the one that was saved (the slot is inspected before and after). Floors: from 300 runs, at least 20 encounter kinds saved at their question and 10
continued; from 100 runs of a cycle, at least one saved and one continued.

## Not covered, and what was found on the way

- **No console, no Azahar.** The save on a real SD card, and what the player sees at the question after a load, were not run on hardware.
- **`save_point()`'s busy guard is not tested.** The host fake has no animation to wait for, so a fault that drops the wait cannot fail a host test.
- **Fiery Fallout (type 11) is out of the cycle.** Its fight is a double battle that begins in a single scene; runtest reports "a centre move in a single scene" for
  it (`RUNTEST_ME=11`). It predates this work and is not fixed here. game-core's soak (31 encounters x 12 runs) does save and load its question.
- **Music check**: encounter trainer fights (Clowning Around, Weird Dream, Trainer's Test, Winstrate) queue a `TrainerVictoryPhase`; the check for the trainer's
  victory music now follows the `battle:trainerDefeated` message (a link-time wrap of `bgm_on_trainer_defeated`) instead of assuming a wave end.
- **Safari**: a long run can find a natural IV Scanner, so "offered without one" now allows one that the run found.
- Plain runtest runs never see an encounter (their waves are not forced into one): only custom runs and the forced ones above do.
