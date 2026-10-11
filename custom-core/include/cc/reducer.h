/* custom-core: the bounded history reducer (task C02; docs/CUSTOM-CONTENT-CONTRACTS.md, section 2.8).
 *
 * Folds batches of observations into the last CC_HISTORY_TURNS turns of at most CC_MAX_EVENTS_PER_TURN events each.
 * Pure: no globals, no I/O, no allocation, no clock, no random numbers, no game call. It observes nothing itself; the
 * adapter (C03) builds the batches.
 *
 * A rejected batch changes nothing, not even the remembered batch id. A cc_reducer is about 13 KB: keep it static or in
 * the run state, never on the 3DS stack. It is zeroed by cc_reducer_init and written member by member, so equal input
 * gives byte-equal state (padding included); the tests compare states with memcmp. */
#ifndef CC_REDUCER_H
#define CC_REDUCER_H

#include "cc/model.h"

/* cc_batch.flags */
enum {
    CC_BF_SOURCE_LOSS = 1u << 0,    /* the game's own event buffer overflowed before this batch (battle_events_overflow /
                                     * events_overflow): the turn is incomplete */
    CC_BF_BEGAN_MID   = 1u << 1,    /* this batch is the first the observer has of a turn already in progress; only valid on
                                     * the batch that would open the turn */
    CC_BF_ALL         = 3u
};

/* One input. Everything in it belongs to ONE turn of ONE battle; split a longer range into several batches. */
typedef struct {
    cc_run_uid run_uid;             /* != 0: the run begun with cc_reducer_begin_run */
    uint32_t batch_id;              /* != 0, strictly increasing within a run, chosen by the adapter; the unit of "counted once" */
    uint32_t battle_seq;            /* >= 1: counts battles within the run */
    uint16_t wave;                  /* >= 1; the same for every batch of one battle_seq, never decreasing across battles */
    uint16_t turn;                  /* the battle's turn counter; 0 = before the first turn */
    uint16_t n_obs;                 /* 0..CC_MAX_BATCH_OBS */
    uint8_t flags;                  /* CC_BF_* */
    const cc_obs *obs;              /* oldest first; may be NULL iff n_obs == 0 */
} cc_batch;

/* What one cc_reduce call did. Always written when `out` is non-NULL; on an error it is all zero except bad_obs. */
typedef struct {
    uint8_t duplicate;              /* 1: an exact replay of the last accepted batch; nothing changed */
    uint8_t new_turn;               /* the batch opened a turn */
    uint8_t turn_evicted;           /* ... and the oldest of the CC_HISTORY_TURNS fell off */
    uint8_t gap;                    /* batch ids were skipped: both neighbouring turns are marked CC_TF_SOURCE_LOSS */
    uint16_t events_added;          /* events the batch produced (some may already be evicted again) */
    uint16_t ignored;               /* observations that produce no event: MOVE_ANIM, and a PR_EV_HP of 0 */
    uint16_t evicted;               /* events dropped from the turn by this batch */
    uint16_t sign_conflicts;        /* measured HP movement contradicted the kind: hp_delta left unknown */
    int32_t bad_obs;                /* index of the observation behind a rejection, else -1 */
} cc_reduce_result;

/* Last known state of one field position, from the newest snapshot that is still trusted. species == 0: none (and then
 * form and hp are 0). */
typedef struct {
    uint16_t species;
    uint8_t form;
    int32_t hp;                     /* >= 0 */
} cc_snapshot;

typedef struct {
    cc_history hist;                /* read-only for the caller; copy it by value after cc_reducer_end_run */
    cc_run_uid run_uid;             /* 0 = no run begun */
    uint64_t last_digest;           /* of the last accepted batch */
    uint32_t last_batch_id;         /* 0 = none yet */
    uint32_t next_seq;              /* the seq of the next stored event, >= 1; never wraps (CC_E_OVERFLOW) */
    uint8_t closed;                 /* the run was ended: batches are refused */
    cc_snapshot pos[CC_BATTLER_MAX + 1];
} cc_reducer;

cc_status cc_reducer_init(cc_reducer *r);                    /* no run begun */
/* Resets everything for a new run. `resumed`: observation starts after the run's first battle (sets CC_HF_BEGAN_MID).
 * uid == 0 is CC_E_RANGE; the uid of the run held now (open or ended) is CC_E_DUPLICATE and changes nothing. */
cc_status cc_reducer_begin_run(cc_reducer *r, cc_run_uid uid, bool resumed);
/* Refuses further batches. Idempotent. CC_E_LIFECYCLE when no run was begun. */
cc_status cc_reducer_end_run(cc_reducer *r);

/* Validates the whole batch, then applies it; see section 2.8 for the order of the checks and what each returns. */
cc_status cc_reduce(cc_reducer *r, const cc_batch *b, cc_reduce_result *out);

/* Structure of the state: cc_history_validate, canonical (zeroed) unused slots, counters and snapshots consistent with it. */
cc_status cc_reducer_validate(const cc_reducer *r);

/* ---- per-turn totals ---- */

/* Sums of one kind on one side. `shown` and `measured` are separate observations of the same events and are never added
 * to each other; each sum covers only the events that have that observation (n_shown / n_measured of n). */
typedef struct {
    uint32_t n;
    uint32_t n_shown;
    uint32_t n_measured;
    uint64_t shown;                 /* sum of `shown` */
    uint64_t measured;              /* sum of |hp_delta| */
} cc_amounts;

typedef struct {
    uint8_t exact;                  /* 1 iff the turn's flags == 0: the sums cover everything the observer was given. When 0
                                     * they are lower bounds over the retained events and must not be shown as turn totals. */
    uint8_t flags;                  /* the turn's CC_TF_* */
    uint16_t dropped;
    uint16_t n_events;
    cc_amounts damage[2];           /* by target side (CC_SIDE_*) */
    cc_amounts heal[2];
    cc_amounts hp_loss[2];
    uint16_t n_faint[2], n_switch_out[2], n_switch_in[2], n_status[2];
} cc_turn_summary;

/* The turn must pass cc_turn_validate. `out` is untouched on an error. */
cc_status cc_turn_summarize(const cc_turn *t, cc_turn_summary *out);

#endif
