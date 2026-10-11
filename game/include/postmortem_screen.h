/*
 * postmortem_screen: what the last turns of a run looked like, from the combat observer's report (combat_observer.h).
 *
 * Opens from the result screen (game over or clear). Top screen: how the run ended, the Pokemon of the player's that fell last in
 * the retained events, and whether the history is complete. Bottom screen: one of the last five observed turns at a time, oldest
 * to newest, as a list of what the battle events showed (hits, heals, HP lost without a hit, faints, switches), the turn's totals
 * and, when the turn is not a full record, a notice saying why.
 *
 * What it does not say: who struck a Pokemon, with what, or why an HP loss happened. The battle events do not report that and the
 * observer never infers it, so "source not recorded" is the whole truth, not a placeholder. Numbers the game displayed (hits,
 * heals) and HP the engine measured (losses without a hit) are listed as what they are and never added to each other. A turn that
 * lost events says so and its totals read "at least".
 *
 * Buttons: LEFT / RIGHT turn, UP / DOWN scroll, A continue (to the title), B back (to the result screen).
 *
 * Stand-ins, not upstream's: this screen and all its words (plain English) are the custom content's own; upstream has nothing like
 * it. Species names are the game's (i18n).
 */
#ifndef GM_POSTMORTEM_SCREEN_H
#define GM_POSTMORTEM_SCREEN_H

#include <stddef.h>

#include "combat_observer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { PM_BACK, PM_CONTINUE } pm_exit;

typedef struct {
  const co_report* report;                  /* must stay valid while the screen is up (the observer owns it until the next run) */
  void (*on_exit)(pm_exit how, void* arg);  /* called from the screen's input; the callee leaves the screen */
  void* arg;
} pm_args;

/* Makes the post-mortem the screen (ui_set, with a fade). False (and nothing opened) when there is no valid report. */
bool postmortem_open(const pm_args* a);

/* One stored event as a line of text ("Your Pikachu took 24 damage"). Exposed for the host tests. */
void pm_describe(const cc_event* e, char* out, size_t n);

#ifdef __cplusplus
}
#endif
#endif
