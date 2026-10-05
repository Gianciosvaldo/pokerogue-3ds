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
 * Stand-ins, not upstream's: the words "Game Over" and the lines at the bottom (the locales have none); the end dialogue with the
 * rival, the unlocks and vouchers, and the run history entry are not built.
 */
#ifndef GM_RESULT_SCREEN_H
#define GM_RESULT_SCREEN_H

#include <stdbool.h>

#include "run_saves.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  bool victory;          /* the run was cleared */
  rsv_info run;          /* the run as it ended (rsv_summarize) */
  void (*on_done)(void* arg);
  void* arg;
} result_args;

/* Makes the result the screen (ui_set, with a fade). */
void result_screen_open(const result_args* a);

#ifdef __cplusplus
}
#endif
#endif
