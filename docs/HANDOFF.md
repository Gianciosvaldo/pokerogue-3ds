# Handoff for outside coding agents

Written 2026-09-30 from the repo at `fb8d2dc` (main). **The author read the repo and did not build or run anything**,
so every number below is what the READMEs and stop notes record, not something re-checked today.

## The project in a few lines
A native C port of [PokeRogue](https://github.com/pagefaultgames/pokerogue) (upstream pinned at `8555c08`) for the
original (Old) 3DS, plus custom content later. Offline homebrew, saves to the SD card. The port is verified by
**differential tests**: a test runs upstream's own TypeScript inside its test harness, records what upstream did
(random states, turns, rolls, reward options), and the C code must reproduce it line for line. Those diffs are the
source of truth. "It compiles" and "it plays" are not.

Start with [`README.md`](../README.md) (folder table), then the README of the folder you work in.

## Who owns what
| Folder | What | Notes |
|---|---|---|
| `battle-core/` | Battle logic (turns, damage, abilities, moves, switching, events) | Diff-tested. Other folders read it and **never edit it**; Tasks 1 and 2 below are battle-core's own work |
| `ai-core/` | Enemy AI | Diff-tested, reads battle-core read-only |
| `run-core/` | Run logic around battles (waves, enemies, trainers, shop) | **lorenzo's own outside sessions build it. Do not touch.** |
| `reward-core/` | Player rewards after a wave | Diff-tested |
| `game-core/` | Headless run loop: starters, waves, exp, rewards, save/resume | Has host soak tests, no upstream needed |
| `game/` | The 3DS app (screens, drawing, input) | Needs the console or the Azahar emulator to judge. Not for outside agents yet |
| `assets-pipeline/`, `sd-builder/` | Builds the SD card packs from upstream | Done |
| `docs/` | Plans | `beta-plan.md` is the roadmap |

## Where things stand on main
- Every branch on the remote is fully merged into main (checked: 0 unmerged commits on all 15). The three stop notes
  the threads left are on main: `battle-core/WHERE-I-STOPPED.md`, `game-core/WIP-dex-and-unlocks.md`,
  `game/WIP-run-driver.md`.
- battle-core: singles battles replay upstream's turns; per its README all covered test cases match. The newest pieces
  (`pr_start_battle`, command-screen checks `pr_can_switch` etc.) are in but **not yet in the README**, and the trainer
  battle re-check still had 35 mismatches when work stopped (Task 1).
- game-core: a Classic run plays start to finish on a PC with save/resume. No dex or starter unlocks yet (Task 3).
- game/: the battle screen shows real battle-core turns in the emulator, including hiding a Pokemon in Fly/Dig and the
  Substitute doll (PRs #34 and #35, merged). It is still fed by a stand-in, not by game-core's run.
- Nothing of the game app has run on a real console. Only the speed and graphics benchmarks have.

## Rules (read these before touching anything)
1. **Branch and pull request only.** Never push to `main`. Never merge your own PR. One task per branch, one PR per task.
2. **Never weaken, skip, or edit a check to make it pass.** If a diff test fails, the fix goes in the C port. Do not
   change `*.producer.test.ts`, the comparison in `turntest.c` / `difftest.c` / `aitest.c`, tolerances, seeds, or case
   counts, and do not make a failing case report "unported" or "not covered" instead of matching. If you believe the
   test itself is wrong, stop and write that up in the PR. Covered counts must not go down.
3. **Do not hand-edit generated files** (`*/gen/out/*`). Regenerate them with the folder's `tools/redump.sh`.
4. **Never commit**: Pokemon or PokeRogue art, music, fonts, logos, `.pak` files, upstream JSON dumps or upstream
   source, built binaries (`.3dsx`, `.elf`, test executables). Keep the upstream checkout outside the repo. `.gitignore`
   covers most of it; do not bypass it.
5. Stay inside the folder(s) the task names. Do not reformat or refactor unrelated code.
6. The PR description must list the exact commands you ran and the numbers they printed. Anything you could not run,
   say so. Do not write "all tests pass" without the output.
7. No 3DS builds needed (they need devkitPro) and nothing here requires the console or emulator. If a step seems to,
   stop and say so.

## Setting up the upstream checks
Needs `git`, `gcc`, `make`, `python3`, Node 22 with `corepack`/`pnpm`, and network access (to clone upstream and
download a few small JSON files).
```sh
sh battle-core/tools/setup-upstream.sh ~/pokerogue-upstream      # clones pagefaultgames/pokerogue at 8555c08, installs, copies the test producers in
make -C battle-core                                               # builds battle-core/build/{turntest,difftest,...}
```
Other folders use the same upstream checkout: `ai-core`, `reward-core`, `run-core` and `game-core` each have
`tools/difftest.sh` or `tools/redump.sh <upstream dir>` (see their READMEs).

Whole-turn test (battle-core), the way the producer documents it (`battle-core/tests/turntest.producer.test.ts`):
```sh
cd ~/pokerogue-upstream
TURN_OUT=/tmp/turns.txt TURN_N=2000 TURN_SEED=777 [TURN_MODE=1 ...] \
  npx vitest run test/tests/zz-dump/turntest.producer.test.ts
cd <this repo> && battle-core/build/turntest /tmp/turns.txt       # prints FAIL CASE lines and a summary
```
`TURN_MODE` stands for one of the mode flags (`TURN_RICH`, `TURN_SWITCH`, `TURN_SUMMON`, `TURN_TRAINER`, `TURN_CPROBE`,
`TURN_START`, ...), each documented at the top of the producer. The baseline numbers per mode are in the table in
`battle-core/README.md`; use them to compare (mismatches 0, covered not lower).

---

## Tasks
Each is small, has a pass/fail you can read off a terminal, and is safe to work on in parallel with the others except
where noted.

### Task 1: Trainer-battle mismatches (battle-core)
**Problem.** `battle-core/WHERE-I-STOPPED.md`: the trainer-mode re-check (`TURN_TRAINER=1`) had 35 wrong turns over
four seeds: seed 777 had 1, 4242 had 1, 31337 had 1, and 9001 had 32 (28 of them text differences). The pattern seen:
the enemy uses Roar or Whirlwind in a trainer battle, upstream prints "But it failed!", the C port does something else
(seed 9001 cases 946, 959 and 964). Cause not diagnosed. The note says it may already be fixed on main, because trainer
switches landed after the re-check, so **first run the check on main** before changing anything.

**Do.** Generate trainer-mode turns for seeds 777, 4242, 31337, 9001 (`TURN_TRAINER=1`, `TURN_N=2000`). The exact
N and any other flags (for example `TURN_SWITCH=1`) that produced the original 35 are not recorded, so try
`TURN_TRAINER=1` alone and with `TURN_SWITCH=1`, and with `TURN_SUMMON=1`. Replay with `battle-core/build/turntest`.
Find why each FAIL CASE differs from upstream and fix it in `battle-core/src/` (switching is in `switching.c`, move
effects in `move_effects.c`, the phase order in `phases.c`). Compare against upstream's source at `8555c08`, not memory.

**Pass.** `turntest` shows 0 mismatches for every file above, AND the files from `TURN_N=2000` on seed 777 in the plain,
`TURN_RICH`, `TURN_SWITCH` and `TURN_SUMMON` modes still show 0 mismatches with covered counts not lower than before
your change (run them before and after, paste both). If main is already clean, say so in a PR that changes nothing
except deleting the obsolete part of the stop note.

### Task 2: Finish the battle-core checklist (battle-core, mostly tests and README)
**Problem.** Steps 2 and 3 of the same stop note. Never run: `TURN_CPROBE=1` (the command-screen checks:
`pr_fight_move`, `pr_move_usable`, `pr_move_selectable`, `pr_is_trapped`, `pr_can_switch`, `pr_can_run`) and
`TURN_START=1` (`pr_start_battle`, `pr_answer_check_switch`) on seeds 9001, 4242 and 31337. Seed 777 passed: CPROBE 2,125
of 2,125, START wild 276/276 and trainer 291/291. The README does not mention any of the new API yet (`grep
pr_start_battle battle-core/README.md` finds nothing).

**Do.** Run `TURN_CPROBE=1 TURN_N=2000` and `TURN_START=1` (once without and once with `TURN_TRAINER=1`) on all four
seeds and record the counts. Add a short section to `battle-core/README.md` for the new functions, `PR_ARG_TEMPLATE`, and
the modes `TURN_CPROBE` and `TURN_START`, in the style of the existing tables. Delete `WHERE-I-STOPPED.md` in the same PR
only if everything is green.

**Pass.** All of those runs show 0 mismatches and the README has the new section with the real counts. The `TURN_TRAINER`
runs will fail until Task 1 is merged. In that case do the other runs, put the failing ones in the PR description, and do
not touch the stop note.

### Task 3: Dex and starter unlock data, first slice only (game-core)
**Problem.** `game-core/WIP-dex-and-unlocks.md` has a full plan, including steps that hook into catching and evolution
that are not ready. Only do the data part: plan items 1, 2 and 6, and the default-state part of item 7.

**Do.**
- Add `gc_system` (per species: `seenAttr`, `caughtAttr`, `natureAttr`, counts, `ivs[6]`; per starter: `abilityAttr`,
  `passiveAttr`, `valueReduction`, `candyCount`, `friendship`, `eggMoves`, `moveset`, `classicWinCount`; plus `unlocks`),
  as the plan describes, in new files prefixed `gc_` (game-core's rule: object files are named by file name only).
- Add the new-save defaults: upstream's `initDexData` and `initStarterData` (`src/system/game-data.ts`).
- Add a dump `game-core/gen/dump-dex.test.ts` (wired into `game-core/tools/redump.sh`) for what the plan item 6 lists,
  and a diff test that builds a new save in upstream and in C and compares every species and starter.
- Do not hook anything into the run loop, catching, evolution, friendship or the save format yet.

**Pass.** The new diff test prints 0 mismatches over all species and starters (say how many it compared), AND
`make -C game-core test` still passes unchanged (the save checks and the with/without-save digest comparison).

### Task 4: A CI check on every pull request (repo root, `.github/`)
**Problem.** The only workflow is `.github/workflows/sd-builder-windows.yml`. Nothing tells lorenzo, or a reviewer,
whether a PR breaks the C code, so every outside PR has to be trusted.

**Do.** Add one workflow (Linux runner, triggered on `pull_request` and `workflow_dispatch`) that builds battle-core,
run-core, ai-core, reward-core and game-core on the host and runs the tests that need no upstream checkout:
`make -C game-core test`. Keep it cheap: no Windows runner, no upstream clone, no devkitPro, no artifacts. Note in the
workflow comment that the diff tests need an upstream checkout and run by hand.

**Pass.** The workflow is green on its own PR. To show it can go red, push a scratch branch (not to be merged) where a
test is deliberately broken and link the failing run in the PR description. The workflow must fail when `make test`
fails: no `|| true`, no `continue-on-error`.

---

## Not for an outside agent yet
- **Move animations aimed at the Substitute doll** (`game/`, upstream's `onSubstitute`). PR #35 merged: the doll is drawn
  and steps aside. Animations that should play on the doll are not done, and only a screenshot can tell if they look right.
- **The grey part of the animation tone** (`game/source/battle_anim.c`, `tone[3]`). Needs citro2d rendering checked on
  the emulator.
- **Driving the battle screen from game-core's run** (`game/WIP-run-driver.md`). Large, and judged on screen.
- **Hooking game-core to `pr_start_battle`, `pr_continue` and trainer switches** (the stand-in table in
  `game-core/README.md`). Changes the soak digests, and should wait for Tasks 1 and 2.
- **What each reward does when picked** (`reward-core/README.md`, "Not done yet"). Large; needs a new producer test.
- Anything that needs a real 3DS, the SD packs, or the Azahar emulator.
