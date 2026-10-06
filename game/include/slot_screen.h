/*
 * slot_screen: upstream's save slot select (src/ui/handlers/save-slot-select-ui-handler.ts): five session slots, each with
 * what the run in it looks like, in LOAD mode (from the title: Load Game) and SAVE mode (a new run chooses its slot).
 *
 * Bottom screen: the slot list (upstream's slot windows: 304 x 70, 76 px apart; three show at a time on the 3DS's 240 rows).
 * Top screen: the highlighted slot in full: mode and wave, biome, score, money, the party with levels, or why a slot
 * can't be loaded.
 *
 * LOAD: A on a slot with a run offers Load Game / Delete Run (upstream also offers Rename Run: it needs the keyboard, so it
 * is left out); a slot that can't be loaded offers Delete Run only, and says why. SAVE (a new run): A on an empty slot picks
 * it; on a run it asks "Overwrite the data in the selected slot?"; a slot that can't be loaded is refused, it is never
 * overwritten (delete it from Load Game first).
 */
#ifndef GM_SLOT_SCREEN_H
#define GM_SLOT_SCREEN_H

#include "run_saves.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { SLOTS_LOAD, SLOTS_NEW } slots_mode;

typedef struct {
  slots_mode mode;
  /* RSV_SLOTS entries the caller keeps alive while the screen is open; after changing them (a delete) call
   * slot_screen_refresh */
  const rsv_info* info;
  int cursor;     /* the slot the cursor starts on */
  /* LOAD: the player chose Load Game for a slot that holds a run. SAVE: a new run goes into the slot (empty, or the player
   * agreed to overwrite). The screen stays up until the caller replaces it. */
  void (*on_pick)(int slot, void* arg);
  /* Delete Run confirmed: the caller removes the slot's files and refreshes the screen. */
  void (*on_delete)(int slot, void* arg);
  /* B: back to the title. */
  void (*on_back)(void* arg);
  void* arg;
} slots_args;

/* Makes the slot list the screen (ui_set, with a fade). */
void slot_screen_open(const slots_args* a);
/* The caller's info changed. */
void slot_screen_refresh(void);
/* A message in the bottom box that waits for A (why a Continue failed...). */
void slot_screen_message(const char* text);

#ifdef __cplusplus
}
#endif
#endif
