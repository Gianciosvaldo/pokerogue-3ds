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
  the threads left are on main: `battle-core/WHERE-I-STOPPED.md`, `game-core/WIP-dex-and-unlocks.md`.
  (`game/WIP-run-driver.md` was deleted when card 110 landed the run driver: `game/source/run_screen.c`.)
- battle-core: singles battles replay upstream's turns; per its README all covered test cases match. The newest pieces
  (`pr_start_battle`, command-screen checks `pr_can_switch` etc.) are in but **not yet in the README**, and the trainer
  battle re-check still had 35 mismatches when work stopped (Task 1).
- game-core: a Classic run plays start to finish on a PC with save/resume. No dex or starter unlocks yet (Task 3).
- game/: the battle screen shows real battle-core turns in the emulator, including hiding a Pokemon in Fly/Dig and the
  Substitute doll (PRs #34 and #35, merged). It is still fed by a stand-in, not by game-core's run.
- Nothing of the game app has run on a real console. Only the speed and graphics benchmarks have.

## Current checkpoint (2026-10-08; historical sections retained)

The opening September 30 status and original task problem statements are dated
history. Use [current scheduling](PARITY-TASKS.md#current-next-work-2026-10-08)
and [ledger](PARITY-LEDGER.md) for source-backed state at main `0a10696`.
Merged #147–#154 add bounded reward Mint/vitamin/Lure prerequisites, battle
Orb/Wide Lens/Reviver Seed dispatch, and game Mint/Orb adapters. These do not
complete Tasks 96/104 or unlock full Task 64/65/66. Vitamin/Lure caller integration,
unsupported Mint inputs and other held-item lifecycle still need bounded work.
The dex/starter foundation and app run driver are already landed; do not
implement the old opening descriptions again. Root/component historical rows
may also be stale: consult owning code and dated validation records.

Actual-head host/3DS build CI succeeded for each #147–#154; conditional Task 63
jobs were skipped, not validated. Source differentials establish only their
compared scope; soaks establish host consistency. Console acceptance, whole-run
upstream parity and the reserved run-core owner boundaries remain unchanged.
One task may run in each independent folder lane, and each lane merges its
current validated PR before starting another; stop at the user's session limit.

## Rules (read these before touching anything)
1. **Branch and pull request only.** Never push to `main`. Do not merge your own PR unless explicitly authorized; the October 7 Codex exception below applies here. One task per branch, one PR per task.
2. **Never weaken, skip, or edit a check to make it pass.** If a diff test fails, the fix goes in the C port. Do not
   change `*.producer.test.ts`, the comparison in `turntest.c` / `difftest.c` / `aitest.c`, tolerances, seeds, or case
   counts, and do not make a failing case report "unported" or "not covered" instead of matching. If you believe the
   test itself is wrong, stop and write that up in the PR. Covered counts must not go down. *Adding* a new test or a new
   test mode (as the tasks below ask) is expected. Loosening an existing one never is.
3. **Do not hand-edit generated files** (`*/gen/out/*`). Regenerate them with the folder's `tools/redump.sh`.
4. **Never commit**: Pokemon or PokeRogue art, music, fonts, logos, `.pak` files, upstream JSON dumps or upstream
   source, built binaries (`.3dsx`, `.elf`, test executables). Keep the upstream checkout outside the repo. `.gitignore`
   covers most of it; do not bypass it.
5. Stay inside the folder(s) the task names. Do not reformat or refactor unrelated code.
6. The PR description must list the exact commands you ran and the numbers they printed. Anything you could not run,
   say so. Do not write "all tests pass" without the output.
7. No 3DS builds needed on your machine (they need devkitPro; only Task 14 builds one, inside CI), and nothing here
   requires the console or emulator. If a step seems to, stop and say so.

## Settled project decisions (lorenzo, 2026-10-04)
Lorenzo answered these; a task PR does not reopen them. If one looks wrong for your task, say so in the PR and stop.

1. **Reference runtime is Node 22.** Run every upstream differential on Node 22 (Node 24 is not a substitute).
   Why: Node 22 / V8 12.4 computes `Math.pow` with fdlibm, and battle-core's `src/js_math.c` reproduces it bit for bit.
   Node 24 returns a different last bit for some inputs. That reaches ai-core through the Stealth Rock matchup score and
   is the origin of the Node 24 failures of 2, 9, 9 and 12 cases across the four seeds (Task 88, #102, ai-core README).
   run-core's shop comparison also fails under Node 24 (#112); that one was not traced to `Math.pow`. Upstream's own
   `package.json` asks for Node >= 24.9.0, so Node 22 is a choice of which engine's numbers the port reproduces, not
   proof that it matches every JavaScript engine. Never waive those Node 24 failures, never loosen a comparison to hide
   them, and if you see a failure only on Node 24, report it as that.
2. **The game is fully offline.** No account login, no server daily rankings, no run upload, no other online feature.
   There is no import of browser saves and no port of upstream's save-version migration code; the port's own save
   format is the only one. Anything upstream gets from a server needs a local replacement chosen in its own card, and
   none is chosen yet.
3. **The upstream pin stays `8555c08c823b856cbec4eb99ca84ea52a955836d`** until a Classic singles run is playable start to
   finish in the app (Milestone 1 of the post-parity plan). After that lorenzo decides whether to move the pin. The
   public beta `4f09698` is not a target before then.
4. **Cards 92 to 96 are approved**, in the lane C order given in [PARITY-TASKS.md](PARITY-TASKS.md) (92, 93, 94, 77, 95,
   96, then 64, 65, 66 and the rest). Card 64 no longer waits for 62.
5. **Run-core producer edits are allowed for card 100 only** (the `gym_e4` producer's `TypeError` and the `rival`
   producer's memory use). Rule 2 still applies to every other check: change a producer minimally, state the case counts
   before and after, never loosen a comparison.
6. **The app driver (card 110) is authorized**: wire `game/` to game-core's run one screen flow at a time, accepted first
   in the Azahar emulator. Console acceptance stays a human gate (Task 85). It runs as a project thread, not as an outside
   agent task, and not in a PR together with other work.
7. **Skipped for now**: the custom-content list (Milestone 4 cards stay unplanned). **Not decided yet**: timed events
   (date-driven seasonal boosts and rewards; none is ported, and the pinned list's one event active today, the "Item
   Update waiting room", gives a Mega Bracelet and a Dynamax Band at Classic wave 115), the second soak chooser (109)
   and the differential CI job (148).

## Codex continuation authorization (user instruction, 2026-10-07)

The user explicitly overrode review before merging: Codex may merge its own PRs
once their required validation passes, and start the next unblocked task without
asking for approval. Keep separate task branches/PRs and check CI on the actual
head before merging. This exception changes the review/merge checkpoint only;
it does not authorize pushing directly to main, weakening checks, changing the
upstream pin, taking over the reserved run-core lane, or waiving human hardware
gates. Partial task slices do not unlock prerequisites that remain missing.

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

## Batch 2 (Tasks 5 to 15)
Ordered by what moves the beta closest to a Classic singles run (beta plan M1). Same rules, same format. As with
Tasks 1 to 4, none of this was run by the author: the "Problem" lines come from the READMEs and from reading the code.

### Lanes: who may run at the same time
Two tasks in the same folder at once will conflict, so each folder is a **lane** and a lane runs **one task at a time,
in the order listed**, each merged before the next starts. Different lanes can run together.

| Lane | Folder | Order | Waits for |
|---|---|---|---|
| A | `battle-core/` | Task 1, 2, then 5, 6, 7, 8, 9 | Tasks 5 to 9 start only after Tasks 1 and 2 are merged |
| B | `reward-core/` | Task 10, then 11 | Nothing |
| C | `game-core/` | Task 3, then 12 and 13 (either order, never together) | 12 needs Tasks 1 and 2 merged. 13 needs Task 3 merged |
| D | `.github/` | Task 4, then 14 | 14 needs Task 4 merged |
| E | `game/` | Task 15 | Nothing |

With three agent slots: run lane A and lane B, and use the third slot for C, D and E in turn (Task 4 first, as a trial).
Lane A changes code that game-core, ai-core and reward-core read, so get Task 4's CI merged before lane A's later tasks.

### Task 5: Catch roll (battle-core, lane A)
**Problem.** `PH_ATTEMPT_CAPTURE` exists in `battle-core/src/engine.c` but the phase isn't ported; the species table has
`catch_rate`, and `pr_battle` has `last_used_pokeball`. Catching is an M1 requirement.
**Do.** Port only the capture *roll* of upstream's `AttemptCapturePhase` (`src/phases/attempt-capture-phase.ts`): the modified
catch rate, the shake checks and the RNG draws they make, every ball type, status and HP effects, bosses as upstream treats
them, and the message keys upstream queues. Not the party-full flow, the dex, candy or animations. Add a producer mode that
runs upstream's phase on random wild targets and compare shakes, caught or not, and the RNG state after. If upstream's
harness can't run the phase headless, stop and say so in the PR. Do not fake the comparison.
**Pass.** The new mode shows 0 mismatches over at least 2,000 cases on each of four seeds (777, 9001, 4242, 31337), and
the existing modes from Task 1's regression list still show 0 mismatches with covered counts not lower.

### Task 6: Flee roll (battle-core, lane A)
**Problem.** `pr_can_run` says whether RUN is allowed; its comment in `battle-core/include/pr/engine.h` says whether the
escape succeeds is the `AttemptRunPhase` roll, "not ported yet".
**Do.** Port upstream's `AttemptRunPhase` roll (its escape chance, the attempt counter it reads and raises, the RNG draw, the
success and failure texts) and wire the `COMMAND_RUN` path in `phases.c` (line ~311) to it. Add a producer mode (or extend
Task 5's) that runs upstream's phase and compares the result and the RNG state after.
**Pass.** 0 mismatches over at least 2,000 cases on each of the four seeds, existing modes unchanged as in Task 5.

### Task 7: Held items, part 1: Leftovers and Shell Bell (battle-core, lane A)
**Problem.** Held items are the biggest gap for real runs. Any turn with a held item on a Pokemon is reported
unported (`PR_FEAT_TURN_MODIFIERS`, noted in `phases.c` and `switching.c`: `grep -n PR_FEAT_TURN_MODIFIERS battle-core/src/*.c`),
so those turns are never checked. `battle-core/README.md` "Not done" lists Leftovers, Focus Band, berries, Quick Claw, King's
Rock and Shell Bell.
**Do.** Add a producer mode `TURN_ITEMS=1` that gives random Pokemon random held items from the list a task covers, and port
the turn-path effect of **Leftovers** (end-of-turn heal) and **Shell Bell** (heal on dealing damage) as upstream's modifier
classes do it (`src/modifier/`), including their texts and stack counts. Leave every other held item reporting unported as now.
**Pass.** `TURN_ITEMS=1` (with only these two items enabled) shows 0 mismatches over at least 2,000 cases on each of the four
seeds and a nonzero count of turns where each item fires. The other modes are unchanged.

### Task 8: Held items, part 2: chance items (battle-core, lane A)
**Problem.** Same gap as Task 7.
**Do.** Extend `TURN_ITEMS` and port **Focus Band** (survives a fatal hit by chance), **Quick Claw** (moves first by chance),
**King's Rock** (flinch chance) and **White Herb** (resets lowered stats; see `phases.c` near the `PR_FEAT_TURN_MODIFIERS`
note about it). Check the class names and rolls in upstream's source at `8555c08`.
**Pass.** As Task 7 with these four items enabled: 0 mismatches over 2,000+ cases per seed, each item fires at least once,
other modes unchanged.

### Task 9: Berries, first slice (battle-core, lane A)
**Problem.** Berries are eaten on thresholds during the turn; `phases.c` (~line 2432, `eatBerries`) still reports them unported.
Upstream has 11 berry types: `BERRY_TYPE_*` in `battle-core/gen/out/pr_enums.h`.
**Do.** Extend `TURN_ITEMS` and port **Sitrus** (HP), **Lum** (status cure) and **Leppa** (PP) as upstream's `BerryModifier`
does, including when they trigger, stack counts, texts, and what a berry eaten does for Belch, Stuff Cheeks and Cud Chew (the
existing conditions in `TURN_COND`). The stat-pinch berries (Liechi, Ganlon, Petaya, Apicot, Salac, Lansat, Starf) and
Enigma stay unported for a later task.
**Pass.** As Task 7 with these three berries enabled, plus the `TURN_COND=1` runs still at 0 mismatches.

### Task 10: What rewards do when picked, part 1: healing, revives, PP (reward-core, lane B)
**Problem.** `reward-core/README.md` "Not done yet": what each reward does when picked. Only the options are ported.
**Do.** Add an apply API to `reward-core/include/pr/reward.h` (or a new header) and port, for the player's party, the
potions (`PokemonHpRestoreModifierType` family, Full Restore included), the revives (Revive, Max Revive, Sacred Ash) and
the PP items (Ether, Max Ether, Elixir, Max Elixir, PP Up, PP Max). Names and numbers are upstream's at `8555c08`
(`src/modifier/`). Add a producer test that applies each item in upstream to random parties (random HP, fainted members,
PP and PP Ups) and compares the party after and the message. The game-core stand-ins are not touched here.
**Pass.** The new test shows 0 mismatches over at least 4,000 cases on each of three seeds with every item above applied
(say how many applications of each), and the existing `tools/difftest.sh` rewards and victory runs from the README
(seed 99, 4,000 cases) still show 0 mismatches.

### Task 11: What rewards do when picked, part 2: Rare Candy, vitamins, held items (reward-core, lane B)
**Problem.** Continuation of Task 10.
**Do.** Extend the apply API and test to **Rare Candy** (level up, calling battle-core's stat code read-only), the vitamins
(base stat modifiers with their stack limit) and the items that are added to the modifier list (held items and persistent
items, with stack caps and what "transfer" means). Mints are a later task.
**Pass.** As Task 10, for these items. Depends on Task 10 being merged.

### Task 12: Wire battle start and trainer switches into game-core (game-core, lane C)
**Problem.** `game-core/README.md`'s stand-in table lists: "the summon phases at a battle's start aren't run", "a trainer's
next Pokemon goes onto the field without its summon phases", "a trainer's switch-out picked by the AI does nothing", "the rest
of a turn after a victory is lost". battle-core has since gained `pr_start_battle`, `pr_continue` and trainer switches (see
`battle-core/WHERE-I-STOPPED.md`), so some rows may already be stale. Check each against the code first.
**Do.** Replace the stand-ins that battle-core now covers with calls to it, and delete the matching rows and
stub counters. Leave rows that still wait on something. Requires Tasks 1 and 2 merged.
**Pass.** `make -C game-core test` passes, `build/gcplay --auto 1000 --jobs 8` reports no crashes or hangs, and the stub
counts the soak report prints for the rows you wired drop to 0 (paste before and after). The digests may change. Say so and why.

### Task 13: Dex updates during a run (game-core, lane C)
**Problem.** Step 3 of `game-core/WIP-dex-and-unlocks.md` without the catching parts. Requires Task 3 merged.
**Do.** Hook `setPokemonSeen` at each enemy encounter, the evolution and form-change updates, and `addFriendship` (candy
progress with upstream's multiplier and caps, replacing the two direct `friendship +=` the note names) into game-core, plus
the `SPECIES_CAUGHT` evolution condition reading the dex. Not catching or the party-full flow.
**Pass.** A diff test comparing the dex and starter data after those functions on random Pokemon in upstream and in C shows
0 mismatches (say how many), and `make -C game-core test` still passes.

### Task 14: CI builds the 3DS app (`.github/`, lane D)
**Problem.** Task 4's CI only builds on the host. Nothing checks the real 3DS build.
**Do.** Add a job to Task 4's workflow that runs the README's build (`docker run --rm -v "$PWD":/src -w /src/game
devkitpro/devkitarm make`) and fails if it fails. No artifacts uploaded, no packs, no upstream download. Requires Task 4 merged.
**Pass.** The job is green on its own PR, and red on a scratch branch (not merged) with a deliberate compile error in `game/`.

### Task 15: Two animation details (`game/`, lane E) (weak check)
**Problem.** `game/README.md` "Not done yet": animations aimed at a Substitute doll (upstream's `onSubstitute`) and the grey
part of a sprite's tone (`tone[3]` in `battle_anim.c`, not reproduced). PR #35 draws the doll but doesn't aim animations at it.
**Do.** Read upstream's `src/data/battle-anims.ts` and its sprite shader at `8555c08`. Put the target choice for a doll and
the tone's grey math in small pure C functions with host tests in `game/tests/` whose expected values you derive from
upstream's source (say how). Wire them in. Do not change the rendering API or anything that needs the console.
**Pass.** `make -C game/tests animtest && build/animtest <pokerogue-assets>` (needs upstream's assets checkout and libjansson)
runs ASan-clean with the same "moves: N played" counts as before, and the new tests pass. **This check only covers the
logic. Whether it looks right is unverified until someone runs the emulator.** Say that in the PR.

---

## Not for an outside agent yet
- **Driving the battle screen from game-core's run** is done (card 110: `game/source/run_screen.c`; what is still a
  stand-in is listed in `game/include/run_screen.h`). The screens still missing are large and judged on screen, so they
  stay project-thread work, not an outside-agent task.
- **The second half of berries** (Liechi, Ganlon, Petaya, Apicot, Salac, Lansat, Starf, Enigma), other held items, mints,
  TMs and evolution items as rewards: same shape as Tasks 7 to 11, worth doing after those land.
- **ai-core's backlog** (`ai-core/README.md`): its held-item scores, trainer switching moves and Illusion depend on
  battle-core work that Tasks 1 and 7 to 9 do first, so wait for them.
- **Catching's screens, the party-full flow and dex/candy on a catch**, and **doubles, forms and Transform, Neutralizing Gas**
  in battle-core. Large; Sunday's work.
- Anything that needs a real 3DS, the SD packs, or the Azahar emulator.
