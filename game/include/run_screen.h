/*
 * run_screen: the app's run driver. It owns the game-core run (gc_run) and shows it with the battle screen:
 * the run's events become screen events (battle_events.c), its screens (command, switch, learn a move,
 * evolve, rewards, biome, game over) become the menus the game already has, and each answer goes back to
 * gc_run_choose. Replaces the demo battle (battle_setup.c).
 *
 * Where a real screen doesn't exist yet it uses a stand-in, listed here so none is mistaken for the real thing:
 *   - starters:   a random team within the cost budget (or the species in rundriver.txt), no starter select
 *   - rewards:    names are the item key in words ("Rare Candy"), picked from an option list, then a Pokemon
 *   - biome:      an option list (the Map screen, menus/biome_map.c, is not wired in)
 *   - evolution:  automatic
 *   - mystery encounters: a message, then the first usable option
 *   - game over / clear: a message, then a new run
 *   - trainers:   the battle starts with their message; the trainer sprite is not drawn
 * Not wired yet: saving, the shop's own screen, the Pokedex, eggs, music.
 */
#ifndef GM_RUN_SCREEN_H
#define GM_RUN_SCREEN_H

#include <stdint.h>

#include "gc/run.h"

typedef struct {
  const char* seed;       /* upstream's 24-character run seed; NULL = a random one from `entropy` */
  uint32_t entropy;       /* a new run's randomness (and the stand-in team's) */
  int starters[6];        /* species of the stand-in team; n_starters 0 = a random team within the budget */
  int n_starters;
} rs_config;

/* What the driver has done, for the host test and the debug overlay. */
typedef struct {
  int runs, game_overs, clears;
  int waves;                  /* WAVE events shown */
  int fights, balls, switches, runs_tried;    /* commands answered (accepted by game-core) */
  int refused;                /* answers game-core refused (the player was told why) */
  int events[16];             /* game-core events handled, by gc_event_kind */
  int screens[16];            /* screens handled, by gc_screen_kind */
  int dup_dropped;            /* game-core messages dropped because the battle events already showed them */
  int not_found;              /* switch-ins whose Pokemon the converter could not find (a bug when not 0) */
  int hp_resynced;            /* COMMAND screens where the HP or status shown was behind the run (an event battle-core did not make) */
  int shown_mismatch;         /* COMMAND screens where the Pokemon on screen was not the one on the field (a bug) */
  int stuck;                  /* screens the driver could not answer (a bug) */
  uint32_t bev;               /* screen events played */
} rs_stats;

/* Starts a new run and shows its first wave. Call once the UI is up; the driver then runs from the UI's
 * callbacks. */
void rs_start(const rs_config* cfg);

const rs_stats* rs_stats_get(void);
const gc_run* rs_run(void);

#endif
