/*
 * result_screen: the end of a run.
 *
 * CLEAR (upstream's EndCardPhase, src/phases/end-card-phase.ts): the end card from gfx.pak (images/cg/end_m, scale 0.5, at the
 * top left of the field) and "Congratulations!" (battle:congratulations, summary style at 128 px) 16 px above the bottom.
 * GAME OVER: upstream shows no screen of its own (it fades out and goes to the title); here the field is the biome behind a
 * dim, with "Game Over".
 *
 * Both: the bottom screen has the run's summary (wave, biome, score, money, party) and a line that waits for A. A hands over
 * to on_done, which leaves the screen (the app's flow clears the slot and goes to the title).
 *
 * With a combat report (the combat observer's, taken as the run ended) A on the line opens the post-mortem (postmortem_screen.h)
 * instead of leaving; the post-mortem's B comes back here, its A leaves. Without a report A leaves, as before.
 *
 * A Classic win with something to tell (win_gains.h: unlocks, vouchers, ribbons, achievements) goes on after the end card's line, as upstream's
 * GameOverPhase queues it behind the EndCardPhase: one message after the other in the box, each waiting for A (the "<thing> has been
 * unlocked." of an unlock, the first ribbon of a species line with its Egg Voucher Plus, the Egg Voucher Premium of a win that is not the
 * first), then the post-mortem or the exit. The achievement and voucher bars slide in over the top screen one after the other from the
 * moment the screen opens (upstream's AchvBar: 0.5 s in, in place until 10 s after it began, 0.5 s out), whatever page the box is on.
 *
 * A Classic win starts with the rival's ending dialogue (upstream's GameOverPhase: the screen fades in on the arena, she slides in from the
 * right of the field, one page of miscDialogue:ending per A with her expression changing as the text goes (@c{...}), a fade out, then the end
 * card fades in over 1 s). The name box and the text are on the bottom screen, her portrait on the top one.
 *
 * Stand-ins, not upstream's: the words "Game Over" and the lines at the bottom (the locales have none).
 */
#ifndef GM_RESULT_SCREEN_H
#define GM_RESULT_SCREEN_H

#include <stdbool.h>

#include "combat_observer.h"
#include "run_saves.h"
#include "win_gains.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  bool victory;          /* the run was cleared */
  rsv_info run;          /* the run as it ended (rsv_summarize) */
  const co_report* report; /* the combat history of the run, NULL = none (A leaves); owned by the observer, valid until the next run */
  const char* note;      /* one more line under the result (the report could not be stored...), NULL = none; copied */
  const wg_gains* gains; /* what the win gave (wg_collect), NULL = none; copied */
  bool ending;           /* a Classic win: the rival's ending dialogue comes before the end card */
  void (*on_done)(void* arg);
  void* arg;
} result_args;

/* Makes the result the screen (ui_set, with a fade). */
void result_screen_open(const result_args* a);

#ifdef __cplusplus
}
#endif
#endif
