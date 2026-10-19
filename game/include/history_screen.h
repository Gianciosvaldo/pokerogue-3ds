/*
 * history_screen: the Run History (upstream's run-history-ui-handler.ts and run-info-ui-handler.ts), from the title's Run History entry.
 *
 * LIST (run-history-ui-handler.ts): one window per finished run, newest first, 304 x 52 px 56 apart as upstream's RunEntryContainer: the
 * result ("Victory!", or "Defeated by" and what beat the player: the wild Pokemon with their levels, the trainer's name, the rival),
 * the mode and wave, when it ended, and the party with levels. The bottom screen has the list (four rows show at a time); the top
 * screen has the highlighted run in full (the arena it ended in, mode, wave, biome, score, money, party). UP/DOWN move (wrapping, as
 * upstream), A opens the run's info, B leaves. An empty history says "Empty".
 *
 * RUN INFO (run-info-ui-handler.ts): the top screen has upstream's layout (the header with the run's name, the result window, the
 * info window with mode, money and luck, and the six party rows with icon, level, nature and ability). The bottom screen has what
 * upstream squeezes into each party row at a type size the 3DS can't show (stats with the nature's arrows, the four moves) for the
 * highlighted Pokemon; A on it opens the Summary screen (menus/summary.c) with that run's party, UP/DOWN there page through it, B comes
 * back. UP/DOWN move on the party, B goes back to the list.
 *
 * Not built, on purpose and not faked: the run length (the port does not count play time), the player's items and each Pokemon's held
 * items (the "view held items" button), the Hall of Fame and the ending art of a win, the trainer's sprite on a defeat by a trainer, a
 * defeat's player gender context, and the personal best tint of an Endless run.
 */
#ifndef GM_HISTORY_SCREEN_H
#define GM_HISTORY_SCREEN_H

#include "run_history.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  rh_status status;   /* of the caller's rhist_load: RH_UNREADABLE shows why the list is empty */
  void (*on_exit)(void* arg);   /* B on the list: back to the title */
  void* arg;
} hist_args;

/* Makes the Run History the screen (ui_set, with a fade). Reads the entries from run_history.h's copy (rhist_count / rhist_get), which the
 * caller has just loaded. */
void history_screen_open(const hist_args* a);

#ifdef __cplusplus
}
#endif
#endif
