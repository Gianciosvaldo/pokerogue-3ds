/*
 * The starter select screen (game/menus/starter.c): a port of upstream's StarterSelectUiHandler and StarterSummary
 * (src/ui/handlers/starter-select-ui-handler.ts, src/ui/containers/starter-summary.ts) for a Classic run: the caught species
 * in a 9 x 9 grid, a team of up to six, the cost budget, and Begin / Exit. Like the other menus it reads no game state: the
 * caller lists the species (starter_args) and describes a species when the cursor reaches it (starter_events.describe), and gets
 * the team back through on_start.
 *
 * What is on the screen, and what is not yet (each is a separate task, none is faked):
 *   - there: the grid with its scrolling and cost labels, the team column with the cost budget, Randomize and Start, the info
 *     panel (sprite, dex number, name, types, growth rate, ability, passive, nature), the options menu with Add to Party and
 *     Remove from Party, and the Begin / Exit questions. The top screen shows the species' sprite and moves.
 *   - not there: the filter bar and sort, the cycle buttons (shiny, form, gender, ability, nature, tera), IVs, moves and nature
 *     management, candy purchases, favourites, rename, the Pokedex page, egg moves, the tutorial. The options menu lists only
 *     what works. See starter_model.h for how the cursor moves without the filter bar.
 */
#ifndef GAME_MENUS_STARTER_H
#define GAME_MENUS_STARTER_H

#include <stdbool.h>
#include <stdint.h>

#include "menus.h"
#include "starter_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* What the info panel shows for one species: how it would be picked (the defaults upstream uses for a species with no stored
 * preference: getStarterDetailsFromPreferences). */
typedef struct {
  menu_mon mon;           /* species, form, gender, shiny, variant, name, types, ability, passive, nature: all the screen reads */
  bool ability_hidden;    /* the ability is the hidden one (gold) */
  bool passive_unlocked, passive_enabled;
  uint16_t moves[4];      /* the moveset it starts with (0 = empty) */
} starter_info;

typedef struct {
  /* The panel's data for `species`, asked when the cursor reaches it. */
  void (*describe)(void* ctx, uint16_t species, starter_info* out);
  /* "Begin with these Pokemon?" answered Yes: the team, in order. True: the caller took it, and the screen takes no more buttons
   * until the caller replaces it. False: the caller refused it (it may say why with starter_message), and the screen goes on. */
  bool (*on_start)(void* ctx, const uint16_t* species, int n);
  /* "Do you want to exit?" answered Yes. */
  void (*on_exit)(void* ctx);
  /* Math.floor(Math.random() * n), for Randomize */
  int (*rnd)(void* ctx, int n);
  void* ctx;
  void (*configure)(void* ctx, uint16_t species);
} starter_events;

typedef struct {
  const sm_entry* entries; /* the species, ascending; kept by the caller while the screen is open */
  int n;
  int budget;              /* getRunValueLimit: 10 in Classic */
} starter_args;

void starter_open(const starter_args* args, const starter_events* ev);
void starter_close(void);
bool starter_active(void);
bool starter_input(mui_button b);
/* A tap at canvas (x, y) (the bottom screen's canvas: its physical y less 30): a species is pointed at, and a second tap on it is A (its
 * options menu); a team member is pointed at; Randomize and Start act at once; the strip beside the grid scrolls it a page up or down; a row
 * of the options menu picks it and a tap outside the menu is B; the Yes / No box takes its rows; a message waiting for a button takes the
 * tap as A. Leaving the screen (B with an empty team) has no tap. True when it did something. */
bool starter_touch(float x, float y);
void starter_update(float dt_ms);
/* The bottom screen: upstream's 320 x 180 canvas (set the origin first, as shop_ui_draw is called) */
void starter_draw(void);
/* The top screen: the same size canvas */
void starter_draw_top(void);
/* A message in the box at the bottom that waits for A (the caller's reasons: "that team is not allowed"). */
void starter_message(const char* text);
void starter_refresh(void);

#ifdef __cplusplus
}
#endif
#endif
