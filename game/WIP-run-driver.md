# Where the "battle screen on game-core's run" piece stopped (2026-09-30)

Paused by lorenzo before any code was written. Main at `16a0678` (PR #28 game-core and PR #30
battle-core `visible` are both merged). Nothing below is built or tested; it is the reading and the
plan, so the next thread can start coding straight away.

## The piece
1. Drive the battle screen from `game-core/include/gc/run.h` instead of the `battle_setup.c` stand-in.
2. Hide a Pokemon under a SemiInvulnerableTag (Fly, Dig, Dive...) using `pr_event_field.visible`.
3. If it fits: the grey part of the tone in move animations, and Substitute (`PR_EV_POKEMON_ANIM`).

## What the reading found (game-core facts the plan depends on)
- `gc_run` is ~300 KB: keep it static. The run runs ahead to the next screen, so one drain of
  `gc_run_next_event` can hold a whole turn, the victory, exp, level ups and messages.
- New wave order: `WAVE`, [`TRAINER`], `SAVE_POINT`, MESSAGE appeared, [`RECALL`s], `SUMMON` enemy,
  [`SUMMON` player], [MESSAGE `trainerSendOut`], [MESSAGE `playerGo`], [`levelCapUp`]. A `SUMMON` comes
  BEFORE its send-out text. When the lead stays out between wild waves there is no player `SUMMON`.
- `GC_EV_BATTLE` points into `run->battle_events` (valid until the next `gc_run_choose`).
- After a victory `gc_battle_end_cleanup` sets `bt.field[2] = NULL` and `bt.n_party[1] = 0`, and a
  biome's party heal changes HP before the drain is read. So `be_convert`'s end-of-turn catch-up
  must use the LAST event's field snapshot, not `bt`.
- `bt.party[0][k]` is upstream's party order; `run->party[i]` never moves. The party screen shows
  battle order `k`; `GC_CH_POKEMON` / `GC_CH_PICK` take `run->party` indexes (`run->party_order[k]`).
- battle-core's `PR_EV_SWITCH_IN` value is the FIELD index, not a party slot. Find the Pokemon by
  species + form + max HP in the side's list (prefer the one on the field).
- Learn move: game-core queues `learnMovePrompt` + `learnMoveLimitReached` before the LEARN_MOVE
  screen, and `countdownPoof` / `learnMoveForgetSuccess` / `learnMoveAnd` / `learnMove` (or
  `learnMoveNotLearned`) after `GC_CH_FORGET`. `game/menus/learnmove.c` shows all of these itself,
  so drop game-core's copies around that screen. Call `gc_run_choose` from the dialog's
  `on_result`, play the new events only after its `on_end`.
- `GC_EV_EXP` (a party, b gained, c level before) is followed by `GC_EV_LEVEL_UP` events. Only the
  field Pokemon's bar animates; keep an app-side copy of each member's exp to get the ratio before.

## Plan (files)
- `battle_events.c`: `be_convert(sides, events, n, shown_player, shown_enemy, out, max)` taking an
  event array and the Pokemon each side can send out, instead of `pr_engine` + `bt`.
- New `run_screen.c/.h`: owns the static `gc_run`; drains events, splits them at each `WAVE`
  (fade, `bv_begin` from the run's state, go on), folds a `SUMMON` and its send-out text into one
  `BEV_SUMMON`, and the wild intro takes the appeared text. Screens: COMMAND -> `bv_ask_command`
  (with `move_ok` / `move_why`), SWITCH -> `ps_open` (FAINT_SWITCH unless `can_skip`),
  LEARN_MOVE -> `learn_move_start`. Stand-ins until the menus thread's screens: starters (a random
  team within the budget, as gcplay's soak runs), rewards and biome (`ui_push_options`), evolution
  (evolve automatically, swap the sprite), game over (message, then a new run).
- Visibility: `bool hidden` in `bv_mon`, a `BEV_VISIBLE` event. Show before the event whose snapshot
  first says visible, hide after the event whose snapshot first says hidden (so Fly's charge
  animation plays before the sprite goes). Not after a FAINT.
- `main.c`: boot into the run; delete `battle_setup.c` and move `game/tests` evtest / switchtest
  to a host `runtest` that plays whole runs through the converter (stub i18n, real locales).

## Test setup notes
- `dockerd` then `docker pull devkitpro/devkitarm` worked in this container (not blocked).
- Upstream locales: `git clone` pokerogue-locales, `git fetch --unshallow`, checkout `270ed0a`.
- Upstream source: `git clone --filter=blob:none --no-checkout` pokerogue, checkout `8555c08 -- src`.
