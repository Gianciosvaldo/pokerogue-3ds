/*
 * combat_observer: the app's side of the custom content history (task C03; docs/CUSTOM-CONTENT-CONTRACTS.md, section 2.8).
 *
 * It reads game-core's raw GC_EV_WAVE / GC_EV_TURN / GC_EV_BATTLE events as run_screen.c pulls them off the run's queue
 * (drain(), before be_convert_events and before anything is played), turns battle-core's pr_event list into custom-core
 * observations and gives each BATTLE range to the reducer once, as one batch. It does not decide, draw or change anything in the
 * run: no gameplay call, no gameplay random number, no file. What it keeps is its own: the reducer, a batch buffer and, once the run
 * ends, a report that owns its data (nothing in it points into the gc_run).
 *
 * What is observed (battle-core/include/pr/events.h): PR_EV_DAMAGE, PR_EV_HP, PR_EV_FAINT, PR_EV_SWITCH_OUT, PR_EV_SWITCH_IN, each
 * with the field snapshot the event carries. Everything else (texts, animations, stat arrows...) is outside the contract and is not
 * counted as lost. PR_EV_MOVE_ANIM is not given to the reducer at all: it produces no stored event and would only add a way for a
 * batch to be refused.
 *
 * What is NOT established, on purpose: who dealt a hit, with which move, and what caused a loss. Nothing in a raw event says so, and
 * this module never infers it from event order, a move animation or an amount. Every stored event has its source bits clear.
 * Separately, co_finish_run copies the engine's final FaintPhase provenance for
 * the report header. This does not attribute any raw history event.
 * Stable Pokemon identities are not derived either (a snapshot's party_slot is an index into a party that can be re-ordered
 * between the event and the drain): species and form come from the snapshot, ids stay unknown.
 *
 * The run uid comes from a source the app installs (co_set_uid_source: custom_store.c hands out durable ones, persisted before the
 * run is observed). Without one, or when it gives none, the uid is a counter of this session: the report says so (uid_durable) and
 * nothing is stored for it.
 */
#ifndef GM_COMBAT_OBSERVER_H
#define GM_COMBAT_OBSERVER_H

#include <stdbool.h>
#include <stdint.h>

#include "cc/reducer.h"
#include "gc/run.h"

#ifdef __cplusplus
extern "C" {
#endif

/* What the observer did, for the host tests and the debug overlay. Counts since co_begin_run. */
typedef struct {
  uint32_t drains;            /* co_observe calls taken in (a replay is not one) */
  uint32_t replayed_drains;   /* calls whose drain number was not new: ignored */
  uint32_t skipped_drains;    /* drain numbers that never arrived (the history is flagged incomplete) */
  uint32_t ranges;            /* GC_EV_BATTLE ranges seen */
  uint32_t batches;           /* batches handed to the reducer, including flagged empty ones */
  uint32_t accepted;          /* ... the reducer took */
  uint32_t rejected;          /* ... it refused (nothing changed; the loss is recorded) */
  uint32_t empty_ranges;      /* ranges with no observed event: no batch, so no empty turn */
  uint32_t bad_range;         /* BATTLE ranges outside the game's event buffer */
  uint32_t unkeyed;           /* ranges seen before the wave or the turn was known (not observed) */
  uint32_t unrepresentable;   /* observed-kind events the reducer could not take (dropped, the batch is flagged) */
  uint32_t loss_flagged;      /* batches flagged because the game said events were lost */
  uint32_t events_in;         /* observations given to the reducer in accepted batches */
  uint32_t ignored;           /* ... of which the reducer made no event from (a PR_EV_HP of 0) */
  uint32_t sign_conflicts;    /* measured HP movement contradicting an event kind */
  uint32_t evicted;           /* events pushed out of a full turn */
  int last_status;            /* cc_status of the last refusal, CC_OK if none */
} co_stats;

/* What a finished run leaves, by value. Valid until the next co_begin_run. */
typedef struct {
  bool valid;                 /* a run was observed and has ended */
  cc_run_uid run_uid;         /* the uid this observation ran under (never 0) */
  bool uid_durable;           /* ... and it came from the uid source: a stored report may name it. False: a session counter */
  uint8_t outcome;            /* CC_OUT_DEFEAT or CC_OUT_CLEAR */
  int16_t biome;              /* the run's biome id as it ended */
  uint16_t wave;              /* the wave it ended on */
  bool began_mid;             /* the run was continued from a save: earlier battles were not observed */
  bool untracked_loss;        /* something was lost that no stored turn records (the history is incomplete beyond its flags) */
  /* The player's Pokemon that fell last in the retained events, by the snapshot of its FAINT event. Only for a defeat. It is a
   * statement about the retained history, not about who finished the run. */
  bool fallen_known;
  uint16_t fallen_species;
  uint8_t fallen_form;
  gc_faint_record final_blow; /* current-wave terminal faint; never inferred from retained events */
  cc_history history;         /* the last turns, oldest first */
  co_stats stats;
} co_report;

/* Where run uids come from: called once per co_begin_run, returns a durable uid (persisted before it is returned) or 0 for none.
 * NULL (the default) means none. The observer works the same either way; only the report's uid_durable differs. */
typedef cc_run_uid (*co_uid_source)(void);
void co_set_uid_source(co_uid_source source);

/* Starts observing a new run (any earlier report is dropped). `resumed`: the run was loaded from a save. */
void co_begin_run(bool resumed);

/* What a custom run's save carries of the observation (task C10): the retained turns and the counters that keep the next battle's turns
 * after them. The reducer's own state is not copied: co_carry_restore rebuilds it from these and validates it, so a damaged carry is
 * refused (and the observation starts empty, flagged as began-mid, exactly as co_begin_run(true) does). */
typedef struct {
  cc_history hist;
  uint32_t next_seq;          /* the seq of the next stored event */
  uint32_t battle_seq;        /* battles seen so far: the next battle is battle_seq + 1 */
} co_carry;
/* The observation as it stands. False when no run is being observed (nothing is written to *out). */
bool co_carry_get(co_carry* out);
/* co_begin_run(true), then the carry. False when the carry does not validate (the observation then starts empty). */
bool co_begin_run_carry(const co_carry* carry);

/* Takes in the events one drain pulled off game-core's queue, once.
 *   drain_no   1, 2, 3... per run, one per drain. A number that is not new is a replay (ignored); a jump means a drain was never
 *              observed (the history is flagged incomplete).
 *   q, nq      the drain's gc events in order. Only WAVE, TURN and BATTLE are read.
 *   be, n_be   run->battle_events and run->n_battle_events as they stand during the drain (valid until the next gc_run_choose).
 *   battle_overflow  run->battle_events_overflow: some battle events never reached the buffer. The drain's batches are flagged.
 *   queue_overflow   run->ev_overflow: game-core's own queue dropped an event at some point. game-core never clears it, so this
 *                    counts once per run; a drain that filled the whole queue counts too.
 * Ignored when no run is being observed or the run has ended. */
void co_observe(uint32_t drain_no, const gc_event* q, int nq, const pr_event* be, int n_be, bool battle_overflow, bool queue_overflow);

/* The run has ended: closes the reducer and captures the report from what it holds. Call once the run object is still valid and
 * before its slot is cleared. A second call changes nothing. False when no run was observed. */
bool co_finish(bool victory, int biome, int wave);
bool co_finish_run(bool victory, const gc_run* run);

/* The report of the run that just ended; NULL when there is none (no run, or it has not ended). */
const co_report* co_report_get(void);
const co_stats* co_stats_get(void);

#ifdef __cplusplus
}
#endif
#endif
