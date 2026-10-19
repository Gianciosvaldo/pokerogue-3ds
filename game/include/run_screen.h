/*
 * run_screen: the app's run driver. It owns the game-core run (gc_run) and shows it with the battle screen:
 * the run's events become screen events (battle_events.c), its screens (command, switch, learn a move,
 * evolve, rewards, biome, game over) become the menus the game already has, and each answer goes back to
 * gc_run_choose. Replaces the demo battle (battle_setup.c).
 *
 * Where a real screen doesn't exist yet it uses a stand-in, listed here so none is mistaken for the real thing:
 *   - starters:   the team the starter select screen handed over (cfg.team); the species in rundriver.txt (a developer's run,
 *                 which has no system data); or, with neither (the host test's runs after the first), a random team within the
 *                 cost budget (of the species the system data has caught, when the run has system data)
 *   - rewards:    the reward / shop screen (menus/shop_ui.c, on the stack through reward_host.c); item names are the
 *                 item key ("RARE_CANDY"), and a Pokemon is picked on the party screen
 *   - biome:      the biome map (menus/biome_map.c, through reward_host.c)
 *   - evolution:  automatic
 *   - mystery encounters: a message, then the first usable option
 *   - game over / clear: with an app around (cfg.on_end) the result screen; without one a message, then a new run
 *   - trainers:   the battle starts with their message; the trainer sprite is not drawn
 *   - catching:   the throw, bounce, shakes and "Gotcha!" are upstream's (AttemptCapturePhase); whether the ball held is the run's.
 *                 battle-core drops the number of shakes a ball made before it broke free, so a break-free always wobbles once.
 *                 Not drawn: the ball's open and star particles, the caught Pokemon's info panel, critical captures (battle-core
 *                 does not report them). A full party asks "release one?" (Yes / No) where upstream also offers Summary and Pokedex
 *   - custom rematch: a custom run (cfg.custom) asks "Fight it?" on game-core's GC_SCREEN_CUSTOM_OFFER; the intro of that wave is the
 *                 statement, the enemy appears after the answer
 * Saving and loading are wired (cfg.save, rs_resume). START on the command menu or the reward screen opens the in-run menu (run_menu.h): Save
 * and Quit calls cfg.on_quit. The music follows the scenes (bgm.h). Not wired yet: the Pokedex, eggs.
 */
#ifndef GM_RUN_SCREEN_H
#define GM_RUN_SCREEN_H

#include <stdint.h>

#include <stdbool.h>

#include "gc/run.h"
#include "custom_store.h"
#include "gc/system.h"
#include "run_saves.h"

typedef struct {
  const char* seed;       /* upstream's 24-character run seed; NULL = a random one from `entropy` */
  uint32_t entropy;       /* a new run's randomness (and the stand-in team's) */
  int starters[6];        /* species of the stand-in team (a developer's rundriver.txt: checked against the system data when `sys` is
                           * set, so set it to NULL for those); n_starters 0 = a random team within the budget */
  int n_starters;
  /* The team the player chose on the starter select screen, for a new run's first start (before `starters`). The caller has
   * checked it with gc_starter_is_available; game-core checks it again. */
  gc_starter team[6];
  int n_team;
  /* The system data (dex, starter unlocks, candy...) the run reads and updates, attached to the run as gc_run.sys; NULL = the run
   * has none and nothing is unlocked or recorded. `save_sys` writes it (system_data.h's sysd_save): called after each save of the
   * run, and a false return counts as a refused write. */
  gc_system* sys;
  bool (*save_sys)(void);
  /* Saving: with `save`, the run is written into save slot `slot` (run_saves.h) at each of game-core's save points: the
   * COMMAND screen of a battle's first turn, once the scene is up. Without it the run is never written. */
  bool save;
  int slot;
  /* A custom run (task C10, custom_run.h): the rematch hooks are installed, the run saves into the custom slot `slot` (a file the ordinary
   * loader never reads) with its overlay, and rs_resume loads from there. false = an ordinary run, played and saved exactly as before. */
  bool custom;
  /* The run ended (screen kind GC_SCREEN_GAME_OVER or GC_SCREEN_CLEAR). The driver stops and calls this from the UI loop; the
   * run stays readable through rs_run() until the next rs_start / rs_resume. NULL = the stand-in: a message, then a new run. */
  void (*on_end)(int screen_kind, void* arg);
  void* end_arg;
  /* Save and Quit (run_menu.h) was confirmed from the menu START opens. The driver has stopped, faded to black and dropped the run; the
   * run stays readable through rs_run() until the next rs_start / rs_resume, and its slot holds the save of the battle's start. NULL =
   * no app around: START does nothing. */
  void (*on_quit)(void* arg);
  void* quit_arg;
} rs_config;

/* What the driver has done, for the host test and the debug overlay. */
typedef struct {
  int runs, game_overs, clears;
  int loads;                  /* runs that came from a save (rs_resume) */
  int saves, save_failed;     /* save points written, and the writes the SD card refused */
  int waves;                  /* WAVE events shown */
  int fights, balls, switches, runs_tried;    /* commands answered (accepted by game-core) */
  int refused;                /* answers game-core refused (the player was told why) */
  int quits;                  /* Save and Quit confirmed */
  int throws, catches;        /* balls thrown (the throw animation shown), and the ones that held */
  int releases, kept_out;     /* full-party catches: a member was released for the caught Pokemon / the caught Pokemon was let go */
  int evolves, cancels;       /* evolutions accepted / cancelled */
  int events[16];             /* game-core events handled, by gc_event_kind */
  int screens[16];            /* screens handled, by gc_screen_kind */
  int dup_dropped;            /* game-core messages dropped because the battle events already showed them */
  int not_found;              /* switch-ins whose Pokemon the converter could not find (a bug when not 0) */
  int hp_resynced;            /* COMMAND screens where the HP or status shown was behind the run (an event battle-core did not make) */
  int shown_mismatch;         /* COMMAND screens where the Pokemon on screen was not the one on the field (a bug) */
  int stuck;                  /* screens the driver could not answer (a bug) */
  uint32_t bev;               /* screen events played */
} rs_stats;

/* Starts a new run and shows its first wave (the UI fades to it). Call once the UI is up; the driver then runs from the UI's
 * callbacks. False when the run could not be set up (nothing was started). */
bool rs_start(const rs_config* cfg);

/* Continues the run saved in slot cfg->slot: loads it, and shows it from its battle's start, as a fresh wave. EMPTY and
 * INCOMPATIBLE mean nothing was started and the file was not touched (`why` says why for INCOMPATIBLE); the driver is then idle.
 * The stats (rs_stats) keep counting, they are reset by rs_start only. */
rsv_status rs_resume(const rs_config* cfg, char why[RSV_WHY_LEN]);

/* The finisher of the defeat that ended the custom run, when game-core's faint hook named one (valid from the call of cfg.on_end until the
 * next rs_start / rs_resume); NULL for an ordinary run, a clear, or a defeat with no reliable finisher. Hand it to cs_commit_defeat. */
const cs_defeat* rs_custom_defeat(void);

const rs_stats* rs_stats_get(void);
const gc_run* rs_run(void);

#endif
