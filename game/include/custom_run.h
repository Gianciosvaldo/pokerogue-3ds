/*
 * custom_run: the optional Nemesis rematch of a custom run (task C10; docs/CUSTOM-CONTENT-CONTRACTS.md, section 2.16).
 *
 * A CUSTOM run is a Classic run the player starts from the title's "Custom Run" (a separate slot list). It is the only kind of run this
 * module touches: an ordinary run never installs the hooks, so it is played, saved and observed exactly as before. A custom run
 *   - carries a sticky flag (its slot is a custom file, "csession_<n>", that the ordinary loader never reads and that never loads as an
 *     ordinary session), the run uid its schedule is keyed by, the frozen rematch schedule, the observation's retained turns, whether
 *     the enemy on the field is a rematch enemy, the reward ids;
 *   - asks the player at an eligible window (waves 27, 77, 127 and 177, custom-core's scheduler decides) whether to fight the Nemesis
 *     it was dealt, and on yes builds that enemy through game-core's own enemy construction (gc/custom.h) at the wave's ordinary level;
 *   - remembers, from game-core's faint hook, the enemy that took the last Pokemon of the party when battle-core's own state names it
 *     (never from the last animation or the last enemy standing), and hands it to the store as the candidate of the run's report;
 *   - records a rematch's result durably (won: the enemy fainted and the run went on; lost: anything else) and claims the record's one
 *     regular voucher through profile_tx.
 *
 * Where the state lives. The roster is custom.sav (custom_store.h: every change is cs_txn, one staged write); the schedule and the
 * rest of the run's overlay live in the custom slot file next to the core run bytes, written by ONE save_write (a temp file with a
 * checksum renamed into place, the previous kept): the run bytes and the overlay can never be from different saves. The two files are
 * still two files, so a crash between them is possible; cr_load reconciles them by rules (custom_run.c, "reconcile") and refuses, with
 * a reason and without touching either file, a run whose reservation another run took.
 *
 * What it does not do: no scaling, no boosts, no boss segments, no change to what the player carries, no second try of a lost rematch
 * in the same run, no rematch for anything but an ordinary single wild encounter. Not carried over a resume: the hooks themselves (the
 * app installs them again), and the offer (never saved: no save is possible while it waits).
 */
#ifndef GM_CUSTOM_RUN_H
#define GM_CUSTOM_RUN_H

#include <stdbool.h>
#include <stdint.h>

#include "cc/jojo.h"
#include "combat_observer.h"
#include "custom_store.h"
#include "gc/custom.h"
#include "gc/run.h"
#include "run_saves.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CR_WHY_LEN 200
#define CR_FILE_PREFIX "csession_"

/* What the custom runs of this session did, for the host tests and the debug overlay. */
typedef struct {
  uint32_t runs_started, runs_resumed;
  uint32_t waves;                 /* waves reported by game-core */
  uint32_t windows;               /* ... of which a window of the schedule */
  uint32_t offers, accepts, declines;
  uint32_t built, refused;        /* accepted rematches game-core built / refused (a refusal is not a custom combat) */
  uint32_t wins, losses;          /* rematch results written to the roster */
  uint32_t claims, claim_retries; /* vouchers granted through profile_tx / owed rewards found waiting */
  uint32_t candidates;            /* defeats whose finisher the faint hook named (handed to the store) */
  uint32_t candidates_none;       /* defeats with no reliable finisher (nothing handed over) */
  uint32_t inserted;              /* ... records the store inserted */
  uint32_t degraded;              /* runs whose rematch stopped because the roster could not be written */
  uint32_t reconciled;            /* resumes whose roster and overlay were brought back in line */
  uint32_t resume_refused;        /* resumes refused for an overlay/roster mismatch */
  uint32_t jojo_waves;            /* eligible trainer waves the JoJo roll was asked about */
  uint32_t jojo_fights;           /* ... of which a JoJo trainer took the wave */
} cr_stats;

const cr_stats* cr_stats_get(void);
void cr_stats_reset(void);

/* ---- runs ---- */

/* The custom file can be used (a custom run cannot be started or resumed without it). `why` (may be NULL) says why not. */
bool cr_usable(char why[CR_WHY_LEN]);

/* Begins a custom run for `run`, a fresh gc_run_new that has not been given its starters yet: allocates the run uid (durable first),
 * gives back what an earlier run still held, freezes the schedule (one draw of its own stream, seeded from `entropy` and the uid, never
 * from the game's random numbers), and installs the hooks. False when the roster could not be written or the file is unusable (the
 * run is not started; cr_error says why). */
bool cr_begin(gc_run* run, uint32_t entropy);
const char* cr_error(void);

/* The custom run is over or abandoned by the driver: hooks off, nothing kept. (Does not touch the roster: cr_on_run_end does.) */
void cr_end(void);
bool cr_active(void);

/* ---- the custom slot: "csession_<slot>" ---- */

/* The slot's summary as the slot list shows it (RSV_OK / EMPTY / INCOMPATIBLE with why). Never changes a file. */
rsv_status cr_inspect(int slot, rsv_info* out);
void cr_inspect_all(rsv_info out[RSV_SLOTS]);

/* Writes the run and its overlay into the slot, in one file. RSV_NOT_ALLOWED when game-core is not at a save point. */
rsv_save_result cr_save(int slot, const gc_run* run);

/* Loads the slot into `run`: the file must be a custom slot (magic, version, lengths), the overlay must decode and validate, the core
 * bytes must load, and the roster must agree with the overlay (cr_load reconciles what a crash between the files left, in one staged
 * write). Installs the hooks. On RSV_INCOMPATIBLE nothing was changed and `why` says why. Then call cr_begin_observer(). */
rsv_status cr_load(int slot, gc_run* run, char why[RSV_WHY_LEN]);
/* After cr_load: starts the observation of the resumed run with the carried turns (co_begin_run_carry). */
void cr_begin_observer(void);

/* Removes the slot's files. If the file still names a reservation, it is given back first (best effort). False when a file would not go. */
bool cr_clear(int slot);

/* ---- the driver's calls (run_screen.c) ---- */

/* Every screen the driver is about to handle: a rematch whose enemy has fainted is won when the run moves on (any screen but the
 * game over). */
void cr_on_screen(const gc_run* run, int screen_kind);
/* The offer on screen: the species the player is asked about (0 when none) and the answer. */
int cr_offer_species(void);
/* The driver has a message for the player (the voucher), once; NULL when none. */
const char* cr_take_message(void);

/* The run ended (GC_SCREEN_GAME_OVER or GC_SCREEN_CLEAR): settles an unresolved rematch as lost and gives back what the run still
 * held. Fills *out and returns true when the faint hook named a finisher for this defeat (hand it to cs_commit_defeat). */
bool cr_on_run_end(const gc_run* run, int screen_kind, cs_defeat* out);

/* Boot: finishes the vouchers owed to won rematches a crash left unclaimed. Call after ptx_boot. Returns how many it claimed. */
unsigned cr_settle_owed(void);

/* ---- the JoJo trainers (custom-core's cc/jojo.h) ----
 * Only a custom run has them: the hook that asks is installed by cr_begin / cr_load, so an ordinary run's trainers are the ordinary ones.
 * An eligible trainer wave (gc/custom.h: an ordinary random single trainer wave) rolls a chance of the wave becoming one of the characters
 * of its biome; the roll state is saved in the run's overlay with the character of the battle at the save point, so the same wave rolled
 * again after a load gives the same trainer. */

/* The JoJo character the current battle is fought against, or NULL (not a custom run, no JoJo wave, a wave that is not a trainer's). */
const cc_jojo_char* cr_jojo_foe(const gc_run* run);
/* A character's team as game-core takes it (also what the hook hands over). False when game-core cannot build it. */
bool cr_jojo_team(int idx, gc_custom_team* team);

/* ---- for tests ---- */
const gc_custom_hooks* cr_hooks(void);
typedef struct {
  bool active;
  cc_run_uid run_uid;
  cc_schedule sched;
  bool forced_done, rematch_live, rematch_enemy, enemy_down, degraded;
  uint32_t rematch_enemy_id;
  uint16_t enc_wave;
  uint8_t enc_kind, enc_flags;
  cc_record_id reward_record;
  bool have_candidate;
  cc_jojo_state jojo;
  uint8_t jojo_live;      /* the character of the battle + 1, 0 = none */
  uint16_t jojo_wave;
} cr_view;
void cr_view_get(cr_view* out);

#ifdef __cplusplus
}
#endif
#endif
