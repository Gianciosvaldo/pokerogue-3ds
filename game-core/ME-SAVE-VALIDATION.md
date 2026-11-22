# An encounter's one save, at its first question

Source: `8555c08c823b856cbec4eb99ca84ea52a955836d`. Upstream saves where an encounter starts, before its question is shown; the port had no save there (a run that was
closed during an encounter came back at the wave before it, or at the last battle's first command). lorenzo's decision: **save once, when the encounter's question screen
shows**, on a still screen, never during animations. The encounter's later screens and its fight are not saved: the one save is the question.

## What game-core does

- `gc_run_can_save(run)` is true at an encounter's first question while nothing has been chosen: the run is in an encounter wave (`BATTLE_TYPE_MYSTERY_ENCOUNTER`), the
  screen is `GC_SCREEN_MYSTERY` for the run's own encounter and tier, the phase queue is at `RP_MYSTERY` with nothing queued behind it, and the run is at its save point
  (`GC_EV_SAVE_POINT` came and no answer was taken). The first answer that is taken ends it, as the first turn ends a battle's save point. An answer the encounter
  refuses leaves the question, and its save, as they were.
- The save carries a one-byte optional `me_question` section. Without it the bytes are what they were; a save that has it holds the run as the encounter began (nobody
  sent out, no enemy). A build from before this change refuses such a save (a battle save with no enemy is damaged), so it never loads one wrongly.
- `gc_run_load` of such a save checks the marker against the rest of the run (it must be a Classic run in an encounter wave, with the encounter and tier in range and no
  fight begun), queues the events an encounter's start queues (`GC_EV_WAVE` with the encounter's battle type, `GC_EV_MYSTERY`, `GC_EV_SAVE_POINT`: nobody is
  summoned, as in a fresh one), restores what the question and its answers read (the counters, battle-core's party list and item list, the AI's seed) and what the last battle left in battle-core's two RNG
  mirrors (stale at the question and read nowhere before the next battle seeds them, but part of the state: the endgame diagnostic folds one of them, and a loaded question
  must equal a fresh one; the second is saved in an optional `me_btrng` section beside the marker), and queues
  `RP_MYSTERY`, which sets the question's screen up again. The question, which options are open and the texts' arguments are those of the saved screen, and so is
  everything after an answer.
- The screen's setup draws nothing from the run's RNG or the battle's in any upstream encounter, so the saved RNG is the one the setup found: no RNG is kept apart.
  `tests/mystery_save_test.c` compares both RNGs of a loaded question with the saved run's for every encounter, so an encounter whose setup drew would fail it.
- Two things a load must put back by hand, found by the soak: the AI's seed (`gc_ai_reset`: a battle's start sets it, an encounter's fight does not, and an enemy
  trainer's summons use it) and Weird Dream's BST ranges (`gc_weird_reset_ranges`: upstream widens them in place for the life of the page and a load is a new page, so the plan
  is drawn again from the initial ranges). **Side effect to know about:** any load of an encounter-question save does this, including a scratch load that only reads a slot
  (`rsv_inspect`, the title's slot summary), which resets the ranges a running session had widened. The ranges only matter to a second Weird Dream in the same session.

## Tests

| What | Command (from `game-core/`) | Result |
|---|---|---|
| Host test: every upstream encounter the fixed run can be given, at the question: saved, loaded, same screen, same events, saved again to the same bytes, the same answer to both leads to the same place; the refusals (marker value, marker on a battle save) | `make mystery-save-test` (part of `make test`) | 1905 checks, 30 of 31 encounters exercised and answered (the encounter's own rules for a party, money or items keep one from the fixed run) |
| Soak: whole runs forced to each encounter in turn, a save/wipe/load at every save point (`gcplay --auto N --save-test --force-mystery T`), the runs' digest and state digest must equal those without the round trips, and at least 300 encounter questions must have been saved | `make me-save-soak` (part of `make test`) | 354 questions saved over 31 encounters x 12 runs, digests equal |
| Planted faults: 25 one-line changes to `gc_save.c` / `gc_phases.c`, each must fail the host test or the soak | `sh tools/mesaveplant.sh` | 25 of 25 caught |

## Not covered

No console, no Azahar. The questions' presentation after a load (intro text, field sprites, music) is the app's: `game/tests/runtest.c` with `RUNTEST_ME=cycle`.
