# Playing the football playoff mode

Select **Football Playoff** from the title menu. This entry leaves existing
Classic/Custom save slots and the Continue selection intact. The usual starter
selector uses the shared profile's unlocks, starter costs and discounts; the mode
validator rejects evolved species, alternate forms, duplicates and over-budget
teams before accepting the selector. The starter editor keeps its existing
profile save path. An unreadable profile uses the same defaults as the rest of
the app, with a startup notice that profile changes cannot be saved.

After drafting, the round screen shows the roster, automatic positions and SPD /
STR. Press A or the Continue button to load the match. The field/score/down/clock
are on top; Offense -> Formation -> Run/Pass -> Named play is on the touch screen. D-pad/A/B also
navigate; B returns one menu level. Each formation/type offers two plays, with a
short risk/gain description before selection (see `PLAYBOOK.md`). Only new action presses are used, so holding A does not accidentally
call a play. When the opponent has possession, choose Man Coverage, Zone Coverage or Blitz
on the bottom screen before its snap. The opponent calls its own offensive plays
and defense. Playcalling is disabled while a snap is running. See `DEFENSE.md`. Use Left/Right or the contact/tackle row to select runner tactics or defensive
dives before a snap (`TACTICS.md`). The match-result screen
requires acknowledgement before team progression begins.

EXP and level-up messages are acknowledged with A (or the Continue button).
Evolution presents explicit species choices and Cancel: Nincada can become
Ninjask OR Shedinja, or remain Nincada. Evolving never silently adds a second
party member. Level-up moves are learned without a prompt in the live mode (moves play no part in a
match): with four moves known, the weakest one makes room (lowest power, status moves count
as 0, earliest slot on a tie). The older six-player mode still asks. Evolution and every
learn answer go through the shared EXP/evolution phase adapter.

The shop shows classic item artwork, prices, sold stock and the selected
teammate's stats. Tap the target row or use Left/Right to choose a teammate. Tap
an offer or use D-pad/A to buy. B or Next match leaves the shop. After the seventh
shop, the champion screen returns to the regular title; losing a match shows its
score, then elimination and the title. Rare stones use existing held-item
ownership. Before the next match, use Left/Right or tap the Mega row to select
one compatible holder, or None. The transformation lasts for that match only;
see `MEGA.md`.

START opens Keep playing / Save and return / Delete football save and return.
Checkpoints are written before snaps, between rounds and in the shop. Selecting
Football Playoff again resumes that file; the app reports backup recovery. Finish
an active snap or EXP/evolution/move prompts before manually saving. An unexpected
close during those phases resumes the last checkpoint. Deletion needs a second
confirmation. Damaged/newer files are preserved until explicit deletion. No
Classic/Custom session file or Continue selection is changed. See `SAVE.md`.

Shared profile changes from evolution are saved through the app's existing
`sysd_save` gate after progression and before exit. An actual failed write offers
retry or continued play; leaving with unsaved profile changes requires an explicit
choice after that failure. No save failure is silently treated as success.
Football does not grant Classic win rewards or create Classic history records.

## Implementation and ownership

- `football-core/src/football_mode.c`: headless orchestration of round intro,
  real matches, results, event messages, evolution/move prompts, shops and ending.
- `game/source/football_screen.c`: the existing `ui_screen` callbacks, starter
  host, physical input/coordinates, profile saves and resource ownership.
- `game/source/title_screen.c` / `app_flow.c`: the title entry and return path.

The app owns one checked heap allocation for the dedicated `gc_run`, made once
before drafting and freed on every exit, including draft cancellation. The mode
state is small and fixed-size; the run is never copied onto the main-thread
stack. Sprite metadata is fixed-size, with the renderer's existing 4 MiB retained
texture budget. New assets load only between rounds, after synchronizing pending
GPU work and clearing the previous sprite cache. Missing/oversized assets use
team markers. Borrowed frames are refreshed before drawing the new round. No
extra framebuffer targets or nested render/input loops are created.

The screen consumes the app's existing button and touch callbacks, which already
use one libctru scan per frame. It does not poll hid again. The app's rr frame
lifecycle owns both screens; football only submits draw calls into those targets.
A frame input fence prevents simultaneous buttons from advancing multiple menu
states. The previous global game-speed setting is restored when leaving.

## Validation and device smoke test

`make -C football-core test` includes the existing match, rules, renderer and shop
suites, plus complete seven-round mode flows for Ninjask, Shedinja and evolution
cancellation, move prompts, shops and elimination. A screen test uses the real
mode/phase engine with SDK/view fakes to verify draft validation, input fences,
overlay pause, resource cleanup, profile failure/retry and returning to title.
`game/tests/flowtest` checks that the title choice receives the shared profile and
save gate without entering Classic/Custom slot routing. Normal CI also compiles
the actual 3DS screen and checks ARM startup/compiler stack reports.

On console/emulator, smoke-test: enter Football Playoff, draft default unlocked
starters, start a round, call a run and pass, check opponent playcalling and score
changes, acknowledge the result, answer progression prompts, buy/leave the shop,
then confirm START returns to the title with the previous Continue slot intact.
Also cancel a draft and re-enter. Host tests do not establish visual quality,
console input feel or measured device performance.
