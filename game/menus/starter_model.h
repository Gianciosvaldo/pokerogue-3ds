/*
 * starter_model: what the starter select screen decides, without drawing it: which species the cursor is on, the team, the
 * cost budget, and what each button does. A port of the input handling of upstream's StarterSelectUiHandler
 * (src/ui/handlers/starter-select-ui-handler.ts at the pinned commit): processInput, processBoxInput, processPartyIconInput,
 * processStartCursorInput, processRandomCursorInput, popPartyStarter and tryUpdateValue's rules.
 *
 * Plain C with no game-core or battle-core in it, so a host test can press buttons and look at the cursor, the team and the
 * cost (game/tests/startertest.c). The screen (starter.c) draws from it and plays the sounds the results ask for.
 *
 * What the model leaves out, because the screen does not have it yet (each one is a button or a focus upstream has):
 *   - the filter bar (generation, type, caught, unlocks, misc, sort): the list is upstream's default filter, caught species in
 *     species order. Upstream's UP from the first row goes to the filter bar and DOWN from a single row too; with none here
 *     UP from the first row wraps to the last row and DOWN from a single row does nothing. DOWN from the Start button and
 *     UP/DOWN from Randomize pass through the filter bar upstream: here they go where two presses would end up.
 *   - the cycle buttons (shiny, form, gender, ability, nature, tera), IVs, moves, nature, candy, favourites, rename.
 *     Every species is picked with the defaults upstream chooses for a species with no stored preference.
 */
#ifndef GAME_MENUS_STARTER_MODEL_H
#define GAME_MENUS_STARTER_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#include "mui.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The grid. Upstream has 9 x 9 containers 18 x 17 px apart with each 40 x 30 icon at half size. Drawn 1:1 on the 3DS, a half-size
 * icon has 20 x 15 real pixels and is not recognisable, so the screen shows the icons at full size in 6 x 5 cells 26 x 30 px
 * apart (lorenzo's choice, 2026-10-11). SM_PARTY_ROWS: the rows whose LEFT/RIGHT edge goes to the team member level with them
 * (the rows below go to Start; upstream rows 0-6 of 9). Building with -DSM_UPSTREAM_GRID gives upstream's geometry back:
 * startertest runs its upstream checks that way. */
#ifdef SM_UPSTREAM_GRID
#define SM_COLS 9
#define SM_ROWS 9
#define SM_CELL_W 18
#define SM_CELL_H 17
#define SM_PARTY_ROWS 7
#else
#define SM_COLS 6
#define SM_ROWS 5
#define SM_CELL_W 26
#define SM_CELL_H 30
#define SM_PARTY_ROWS 4
#endif
#define SM_PAGE (SM_COLS * SM_ROWS) /* the containers on screen */
#define SM_TEAM_MAX 6               /* PLAYER_PARTY_MAX_SIZE */

/* One species in the list (a StarterContainer). Species are listed in ascending order. */
typedef struct {
  uint16_t species;
  double cost;       /* getSpeciesStarterValue: the cost with the reductions bought */
  double base_cost;  /* speciesStarterCosts: the cost label is blue or gold when the cost is reduced */
  uint8_t form;      /* the default dex props (getSpeciesDefaultDexAttrProps): the icon */
  uint8_t variant;
  bool female, shiny;
  bool passive;      /* passiveAttr != 0: the passive background */
  bool hidden_ability; /* the hidden ability is unlocked: the capsule */
  bool won;          /* classicWinCount > 0: the champion ribbon */
} sm_entry;

/* Where the cursor is: on a species in the grid, a team member, the Start button, or the Randomize button. */
typedef enum { SM_GRID, SM_PARTY, SM_START, SM_RANDOM } sm_focus;

/* What a button did, for the screen to play and show. */
typedef enum {
  SM_IGNORED,   /* nothing: no sound (upstream's success and error both false) */
  SM_OK,        /* it did something: ui.playSelect */
  SM_ERROR,     /* it was refused: ui.playError */
  SM_MENU,      /* A on a species: open the options menu (sm_menu_*) */
  SM_ASK_START, /* "Begin with these Pokemon?" Yes / No */
  SM_ASK_EXIT,  /* "Do you want to exit?" Yes / No */
} sm_result;

typedef struct {
  const sm_entry* entries;
  int n;
  int budget;                 /* getRunValueLimit: 10 in Classic */
  int (*rnd)(void* ctx, int n); /* Math.floor(Math.random() * n) for the Randomize button */
  void* rnd_ctx;

  sm_focus focus;
  int cursor;                 /* the grid cursor, 0..SM_PAGE-1, on screen (upstream's this.cursor) */
  int scroll;                 /* the first row on screen (scrollCursor) */
  int party_cursor;           /* partyIconsCursorIndex */
  uint16_t party[SM_TEAM_MAX];/* partyStarterIds */
  int n_party;
  uint16_t shown;             /* the species Randomize just added: the panel shows it while the cursor stays on the button */
} sm_model;

/* `entries` must outlive the model. The cursor starts on the first species (show(): setCursor(0)). */
void sm_init(sm_model* m, const sm_entry* entries, int n, int budget, int (*rnd)(void*, int), void* rnd_ctx);

/* A button press (SM_* above). SUBMIT and MENU both mean Start (the 3DS's START button is MENU). */
sm_result sm_input(sm_model* m, mui_button b);

/* The touch screen's cursor move (upstream has none): put the cursor on the grid cell i on screen (SM_GRID: only a cell with a species in it),
 * on team member i (SM_PARTY: only a member the team has), or on Start or Randomize (SM_START, SM_RANDOM: i is ignored). False when
 * nothing is there. The panel then shows what the cursor is on. */
bool sm_point(sm_model* m, sm_focus focus, int i);
/* Scrolls the grid by `rows` (negative: up), clamped to the list; the cursor keeps its place on screen unless the species there is gone.
 * False when it did not move. */
bool sm_scroll(sm_model* m, int rows);

/* The species the info panel shows (the grid cursor's, or the team member's), 0 when the cursor is on a button. */
uint16_t sm_species(const sm_model* m);
/* The species in the grid cell at on-screen index i (0..SM_PAGE-1), or NULL when the list ends before it. */
const sm_entry* sm_cell(const sm_model* m, int i);
const sm_entry* sm_find(const sm_model* m, uint16_t species);

bool sm_in_party(const sm_model* m, uint16_t species, int* index);
double sm_value(const sm_model* m);          /* getPartyValue */
/* The options menu's "Add to Party" is offered: not in the team, the team has room, and the cost fits the budget. */
bool sm_can_add(const sm_model* m, uint16_t species);
/* A species' icon is lit (tryUpdateValue: alpha 1) when what is left of the budget pays for it; else it is greyed (0.375). */
bool sm_lit(const sm_model* m, const sm_entry* e);
bool sm_add(sm_model* m, uint16_t species);  /* addToParty after the cost check; false when sm_can_add is false */
void sm_remove(sm_model* m, int index);      /* popPartyStarter */

/* "x/10" the way tryUpdateValue writes it: JavaScript's number text, with the leading "0" of 0.5 left off. */
void sm_value_text(char* out, int n, double value, int budget);
/* The label of a container (updateStarterValueLabel): ".5", "3". */
void sm_cost_text(char* out, int n, double cost);
/* 0 = unchanged (WINDOW), 1 = reduced by one or a half (SUMMARY_BLUE), 2 = other reductions (SUMMARY_GOLD) */
int sm_cost_style(const sm_entry* e);

/* y of team icon i (calcStarterIconY) */
int sm_party_icon_y(int i);

#ifdef __cplusplus
}
#endif
#endif
