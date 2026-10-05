/*
 * run_screen: the app's run driver. It owns the game-core run (gc_run) and shows it with the battle screen:
 * the run's events become screen events (battle_events.c), its screens (command, switch, learn a move,
 * evolve, rewards, biome, game over) become the menus the game already has, and each answer goes back to
 * gc_run_choose. Replaces the demo battle (battle_setup.c).
 */
#ifndef GM_RUN_SCREEN_H
#define GM_RUN_SCREEN_H

#include <stdbool.h>
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

/* Sets active save slot (0..4). */
void rs_set_slot(int slot);
int rs_get_slot(void);

/* Starts a new run and shows its first wave. */
void rs_start(const rs_config* cfg);

/* Loads an existing run save from slot. Returns true if load succeeded. */
bool rs_load_slot(int slot);

/* Called when Game Over or Clear screen is dismissed to start next run. */
void rs_on_end_screen_dismissed(void);

const rs_stats* rs_stats_get(void);
const gc_run* rs_run(void);

#endif
