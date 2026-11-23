/*
 * egglist_model: what upstream's Egg List shows for an egg (src/ui/handlers/egg-list-ui-handler.ts `setEggDetails`, with Egg.getKey,
 * getEggDescriptor, getEggHatchWavesMessage and getEggTypeDescriptor from src/data/egg.ts) as plain data. No drawing and no i18n lookups: the
 * screen fills `egm_texts` from the locale and gives a species-name function.
 *
 * Not ported: an egg's own `eggDescriptor` (upstream's Breeder / Trainer's Test rewards can carry one, and it replaces the type line); the port's
 * gc_egg has no such field, so those eggs show the line of their source type.
 */
#ifndef GM_EGGLIST_MODEL_H
#define GM_EGGLIST_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#include "gc/system.h"
#include "scroll_grid.h"

#ifdef __cplusplus
extern "C" {
#endif

/* the locale strings the panel uses (the keys are in egg:...). tier: 0 default, 1 great, 2 ultra, 3 master, 4 manaphy. waves: soon, close, notClose, longTime. */
typedef struct {
  char egg[40];             /* egg:egg */
  char tier[5][40];         /* defaultTier, greatTier, ultraTier, masterTier, manaphyTier */
  char waves[4][200];       /* hatchWavesMessageSoon, ...Close, ...NotClose, ...LongTime */
  char gacha_legendary[64]; /* gachaTypeLegendary */
  char gacha_move[64];      /* gachaTypeMove */
  char gacha_shiny[64];     /* gachaTypeShiny */
  char event_type[64];      /* eventType */
  char same_species[120];   /* sameSpeciesEgg, with "{{species}}" in it */
} egm_texts;

typedef const char* (*egm_species_name)(int species_id, void* ud);

typedef struct {
  char key[8];   /* the icon / egg frame: "0".."3", or "manaphy" */
  char name[96]; /* "Egg (Common)" */
  char date[32]; /* "Fri, 02/13/2026" */
  char waves[200];
  char type[200]; /* "" for a source type upstream has no text for */
} egm_details;

/* upstream isManaphyEgg: a Phione or Manaphy egg, or a legacy Common egg (no species yet) whose id is divisible by 204 */
bool egm_is_manaphy(const gc_egg* e);

/* Egg.getKey: "0".."3" (the tier) or "manaphy": the frame of the egg's icon and sprite */
void egm_key(const gc_egg* e, char* out, int n);

/* The panel's texts for `e`. tz_offset_s: seconds the local time is ahead of UTC at that moment (the date is shown in local time). */
void egm_describe(const gc_egg* e, const egm_texts* t, egm_species_name name, void* ud, int tz_offset_s, egm_details* out);

/* toLocaleString("en-US", {weekday: "short", year: "numeric", month: "2-digit", day: "numeric"}): "Fri, 02/13/2026"; "Invalid Date" past JavaScript's range */
void egm_date(uint64_t timestamp_ms, int tz_offset_s, char* out, int n);

/*
 * The list's own state (EggListUiHandler): the 9 x 11 grid of icons over a scroll_grid, the handler's cursor and which egg the panel shows.
 * The state holds indexes only: the caller keeps the list and reads egg `detail` / `egl_cell_egg(cell)` from it.
 */
#define EGL_ROWS 9
#define EGL_COLS 11
#define EGL_ICONS (EGL_ROWS * EGL_COLS)

typedef struct {
  scroll_grid grid;
  int n;
  int cursor; /* the handler's cursor, -1 until shown; inside the visible grid */
  int detail; /* index into eggs of the egg the panel shows */
} egl_state;

/* show(): n >= 1 (upstream opens the list only when there are eggs) */
void egl_open(egl_state* s, int n);
/* UP / DOWN / LEFT / RIGHT: true when the cursor or the scroll position changed (upstream plays the select sound then) */
bool egl_input(egl_state* s, sg_dir d);
/* The icon in cell i (0..EGL_ICONS-1) of the visible grid: the egg's index, or -1 when the cell is empty (the grid holds min(99, n) cells). */
int egl_cell_egg(const egl_state* s, int cell);
/* A tap on cell `cell`: the cursor goes there (the app's own; upstream has no touch). False when the cell has no egg or the cursor is already there. */
bool egl_select_cell(egl_state* s, int cell);
/* the number of cells the grid has (upstream's initEggIcons: min(rows * columns, n)) */
int egl_cell_count(const egl_state* s);

#ifdef __cplusplus
}
#endif
#endif
