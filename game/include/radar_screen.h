/*
 * radar_screen: the Nemesis Radar (task C06; docs/CUSTOM-CONTENT-CONTRACTS.md, section 2.12): a read-only look at the enemies the
 * custom content remembers, from the title screen.
 *
 * Bottom screen: the roster, newest first, as a scrolling list (an icon, the name and level, where the run ended, and what state the
 * record is in), and below it one line saying whether a rematch is scheduled. A on a record is "Scout": a page with everything the
 * record knows (ability, nature, moves, held items, IVs, the last move, where it came from) and the windows of the schedule that holds
 * it. Top screen: the highlighted record, with the biome it ended a run in as the background, its front sprite when the packs have
 * one, and a faint tint of its first type when the species is in battle-core's tables.
 *
 * What it does not do, on purpose: it starts nothing. There is no fight button, no "invasion" and no location: nothing in this
 * version can spawn a rematch enemy, and the screen says so. A record that is not in a schedule says "no rematch is scheduled"; a
 * record named by a schedule shows that schedule's real windows (waves 27, 77, 127 and 177 and what became of each). The app has no
 * stored schedule yet (it lives in the live adapter, C10), so it passes none.
 *
 * Scouting is free and changes nothing: the screen copies what it is given when it opens (nothing it holds can outlive the owner),
 * writes no file, draws no random number, spends nothing and never creates or changes a schedule. The tint is colour on the screen
 * only. Species, move, ability, nature and biome names are the game's (i18n); the screen's own words are plain English, as the
 * post-mortem's are.
 *
 * Buttons: UP / DOWN select (scroll on the Scout page), LEFT / RIGHT jump a page, A Scout, B back. The game framework has no touch
 * input for these screens (only the shop's own layer reads the touch screen), so there are no touch buttons.
 */
#ifndef GM_RADAR_SCREEN_H
#define GM_RADAR_SCREEN_H

#include <stdbool.h>
#include <stddef.h>

#include "cc/model.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RADAR_WHY_LEN 160

typedef struct {
  const cc_meta* meta;          /* NULL: the custom file cannot be used, and `why` says why */
  const char* why;              /* copied when the screen opens; may be NULL */
  const cc_schedule* schedule;  /* this run's frozen schedule, if there is one; NULL otherwise. Copied; never changed */
  void (*on_exit)(void* arg);   /* B on the list: called once, from the screen's input; the callee leaves the screen */
  void* arg;
} radar_args;

/* Makes the Radar the screen (ui_set, with a fade). False (nothing opened) for NULL, an invalid roster or an invalid schedule. */
bool radar_open(const radar_args* a);

/* The one-line answer to "is a rematch scheduled for this record?", in plain English. `schedule` may be NULL. Exposed for the host tests. */
void radar_rematch_text(const cc_nemesis* n, const cc_schedule* schedule, char* out, size_t len);

#ifdef __cplusplus
}
#endif
#endif
