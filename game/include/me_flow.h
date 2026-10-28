/*
 * me_flow: a Mystery Encounter from the app's side (upstream's MysteryEncounterPhase and the dialogue of its option phases): the text it
 * says before the question, the question (me_screen.h) and the picks an option asks for, ending in the GC_CH_MYSTERY choice game-core
 * wants and the "selected" text to say before that choice runs.
 *
 * Texts are the encounter's locale file (locales/<lang>/mystery-encounters/<name>-dialogue.json), worded with the {{tokens}} the screen
 * carries (gc_screen.me_args) and, for the picked Pokemon, selectedPokemon. A missing token uses a bounded plain-text fallback, and a
 * "selected" text that would need one is left out rather than said with a hole in it. Encounters whose texts are not ported yet carry no
 * tokens: they show their texts as the locale has them.
 *
 * The GTS asks for each of its three Pokemon offers or any transferable held item, with back navigation.
 * The encounter's `outro` and texts that depend on the result (boosts gained, the Nature changed...) are said by
 * the events game-core queues (or not at all yet).
 */
#ifndef GM_ME_FLOW_H
#define GM_ME_FLOW_H

#include <stdbool.h>
#include <stddef.h>

#include "gc/run.h"
#include "me_screen.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ME_SELECTED_LEN 1200

typedef struct {
  int option, b, c;                /* the GC_CH_MYSTERY choice */
  char selected[ME_SELECTED_LEN];  /* the text to say before it runs ("$" between pages); empty when there is none */
} me_choice;

/* The intro (the `intro` line, then `introDialogue`), "$" between pages, for the screen the run is on (GC_SCREEN_MYSTERY). "" if the
 * locale has neither. */
void me_flow_intro(const gc_run* run, char* out, size_t n);

/* The intro sprites upstream shows for the encounter type, laid out as MysteryEncounterIntroVisuals does (even spacing over -40..40 for
 * the sprites without an offset, the rest at their x/y), as screen coordinates for a platform centred at cx with its floor at floor_y.
 * Only the sprites whose image key is fixed are drawn; the Pokemon and trainers an encounter picks at run time keep their place empty.
 * Returns how many were written (at most max). */
int me_flow_intro_sprites(int type, float cx, float floor_y, me_intro_sprite* out, int max);

/* Opens the question for the run's current encounter. on_choice is called once, from the screen's input, after the screens popped. False
 * (nothing opened) when the run is not on an encounter screen. */
bool me_flow_open(const gc_run* run, void (*on_choice)(const me_choice* c, void* arg), void* arg);

/* The locale namespace of an encounter ("mysteryEncounters/lostAtSea"), NULL for an unknown type. Exposed for the host tests. */
const char* me_flow_ns(int type);

#ifdef __cplusplus
}
#endif
#endif
