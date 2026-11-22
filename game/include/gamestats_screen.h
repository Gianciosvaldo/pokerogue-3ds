/*
 * gamestats_screen: upstream's Game Stats screen (src/ui/handlers/game-stats-ui-handler.ts), from the title's "Stats" entry.
 *
 * The bottom screen is upstream's layout: a header ("Stats for Guest": the game is offline, upstream's own name for no account) and two columns
 * of nine stats each, label on the left, value on the right; UP/DOWN scroll a row (two stats) at a time, with the arrows upstream shows when there
 * is more above or below; B (or its hint) leaves. A stat marked hidden reads "???" until it is not 0. The top screen only has the background.
 * What is shown and how it is worded is gamestats_model.h; it reads the profile and changes nothing.
 */
#ifndef GM_GAMESTATS_SCREEN_H
#define GM_GAMESTATS_SCREEN_H

#include <stdint.h>

#include "gc/system.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  const gc_system* sys;                 /* read when the screen opens and at each scroll, not copied */
  uint32_t (*play_seconds)(void);       /* the play time now (system_data.h's sysd_play_seconds); NULL: the profile's own */
  void (*on_exit)(void* arg);           /* B: back to where it came from */
  void* arg;
} gamestats_args;

/* Makes the stats the screen (ui_set, with a fade). */
void gamestats_screen_open(const gamestats_args* a);

#ifdef __cplusplus
}
#endif
#endif
