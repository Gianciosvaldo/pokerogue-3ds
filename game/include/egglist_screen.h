/*
 * egglist_screen: upstream's Egg List (src/ui/handlers/egg-list-ui-handler.ts), from the title's "Egg List" entry.
 *
 * The bottom screen is upstream's layout: a grid of 9 rows by 11 columns of egg icons (the tier's colour, or the Manaphy egg) on the right, the
 * egg under the cursor on the left (its picture, "Egg (Common)", the date it was obtained, how close it is to hatching, and where it came from);
 * D-pad moves the cursor (wrapping at the edges, scrolling a row at a time past nine rows), B leaves. With no eggs, upstream does not open the
 * list but says "You are not hatching any Eggs at the moment!": this screen shows that message and B leaves. The top screen has only the background.
 * A tap on an icon moves the cursor there (the app's own). What is shown is egglist_model.h; the screen copies the eggs and changes nothing.
 */
#ifndef GM_EGGLIST_SCREEN_H
#define GM_EGGLIST_SCREEN_H

#include <stdint.h>

#include "gc/system.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  const gc_egg* eggs;                    /* copied when the screen opens */
  int n;                                 /* at most GC_MAX_EGGS are shown */
  int (*tz_offset)(uint64_t timestamp_ms); /* seconds local time is ahead of UTC at that moment (the date is local); NULL: 0 */
  void (*on_exit)(void* arg);            /* B: back to where it came from */
  void* arg;
} egglist_args;

/* Makes the Egg List the screen (ui_set, with a fade). */
void egglist_screen_open(const egglist_args* a);

#ifdef __cplusplus
}
#endif
#endif
