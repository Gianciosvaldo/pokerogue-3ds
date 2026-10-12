/* custom-core: the version-1 contract functions (see docs/CUSTOM-CONTENT-CONTRACTS.md, "API").
 *
 * Everything here is pure: no globals, no I/O, no allocation, no clock, no random numbers, no game calls.
 * Validators never modify their argument. Mutating functions (cc_nemesis_transition, the allocators,
 * cc_ledger_append) change nothing when they return an error. */
#ifndef CC_MODEL_H
#define CC_MODEL_H

#include "cc/types.h"
#include "cc/wire.h"

typedef enum {
    CC_OK = 0,
    CC_E_NULL,           /* a required pointer was NULL */
    CC_E_RANGE,          /* a value outside its range, or not a defined enum value */
    CC_E_COUNT,          /* a count over its limit */
    CC_E_UNKNOWN_BITS,   /* a flag or "known" bit version 1 does not define */
    CC_E_NONCANONICAL,   /* a value stored for a field whose "known" bit is clear */
    CC_E_ORDER,          /* not strictly increasing where it must be */
    CC_E_DUPLICATE,      /* the same identity twice */
    CC_E_CONSISTENCY,    /* fields that contradict each other */
    CC_E_LIFECYCLE,      /* a forbidden record transition */
    CC_E_OVERFLOW,       /* arithmetic would overflow, or an identifier space is exhausted */
    CC_E_VERSION,        /* a schema version this build does not read (older or newer) */
    CC_E_FORMAT          /* bytes that are not a custom metadata blob: wrong magic, reserved bits, truncation, trailing bytes */
} cc_status;

const char *cc_status_str(cc_status s);
const char *cc_refusal_str(int refusal);        /* CC_SUPPORT_OK / CC_REFUSE_* */

/* ---- runtime observation ---- */
cc_status cc_obs_validate(const cc_obs *o);

/* ---- persistent history ---- */
cc_status cc_actor_validate(const cc_actor *a);
cc_status cc_event_validate(const cc_event *e);
cc_status cc_turn_validate(const cc_turn *t);
/* Turns oldest first, keys (battle_seq, turn) strictly increasing, event seq strictly increasing across turns. */
cc_status cc_history_validate(const cc_history *h);
/* committed: the transaction ID must be set (a stored report); false for a candidate that has none yet. */
cc_status cc_terminal_validate(const cc_terminal *t, bool committed);

/* ---- Nemesis records ---- */
/* Structure, ranges, "known" canonicity and lifecycle bookkeeping. Any trait is representable here. */
cc_status cc_nemesis_validate(const cc_nemesis *n);
/* CC_SUPPORT_OK, or why version 1 will not remember this enemy. Input must pass cc_nemesis_validate. */
int cc_nemesis_support(const cc_nemesis *n);
/* The insertion gate: the terminal outcome must be a defeat whose finishing actor is this enemy, then
 * cc_nemesis_support. `term` must pass cc_terminal_validate, `candidate` cc_nemesis_validate. */
int cc_insertion_check(const cc_terminal *term, const cc_nemesis *candidate);
/* cc_nemesis_validate plus cc_nemesis_support == OK: what a stored record has to satisfy. */
cc_status cc_nemesis_validate_stored(const cc_nemesis *n);
cc_status cc_roster_validate(const cc_roster *r);

/* ---- record lifecycle ---- */
bool cc_life_can_transition(uint8_t from, uint8_t to);
bool cc_life_overwritable(uint8_t life);        /* may a full roster overwrite a record in this state? */
/* Moves `n` to `to` and updates the bookkeeping fields with it:
 *   STORED -> SCHEDULED          run_uid != 0                       sets sched_run_uid
 *   SCHEDULED -> CHALLENGED      wave is one of cc_window_waves     sets challenge_wave
 *   SCHEDULED/CHALLENGED -> STORED                                  clears sched_run_uid, challenge_wave
 *   CHALLENGED -> DEFEATED
 *   DEFEATED -> REWARD_CLAIMED   reward_txn != 0                    sets reward_txn (once: a second claim is refused)
 * Anything else, or a missing argument, returns an error and leaves `n` unchanged. */
cc_status cc_nemesis_transition(cc_nemesis *n, uint8_t to, cc_run_uid run_uid, uint16_t wave, cc_txn_id reward_txn);

/* ---- run-bound schedule ---- */
cc_status cc_schedule_validate(const cc_schedule *s);

/* ---- identifiers and the transaction ledger ---- */
cc_status cc_counters_validate(const cc_counters *c);
/* Each allocator hands out the current value and advances the counter; the caller persists the counter BEFORE using
 * the identifier. Exhaustion returns CC_E_OVERFLOW and leaves the counter unchanged. A run UID is
 * (install_nonce << 32) | run_seq, never 0. */
cc_status cc_alloc_record_id(cc_counters *c, cc_record_id *out);
cc_status cc_alloc_txn_id(cc_counters *c, cc_txn_id *out);
cc_status cc_alloc_run_uid(cc_counters *c, cc_run_uid *out);
/* True when `uid` is one these counters have issued: its high half is the install nonce, its low half a sequence number
 * below next_run_seq. A stored run UID, in the report, a record or the ledger, has to be one. */
bool cc_run_uid_issued(const cc_counters *c, cc_run_uid uid);

cc_status cc_ledger_validate(const cc_ledger *l);
/* The remembered transaction of `kind` for this run (CC_TXN_TERMINAL) or this record (CC_TXN_REWARD_CLAIM), if any.
 * For TERMINAL the record_id argument is ignored, for REWARD_CLAIM the run_uid argument is ignored. */
bool cc_ledger_find(const cc_ledger *l, uint8_t kind, cc_run_uid run_uid, cc_record_id record_id, cc_ledger_entry *out);
/* Appends (the oldest entry falls off a full ledger). Refuses a txn_id that does not exceed the last one and a second
 * transaction for a key already present: that is what makes retries idempotent. */
cc_status cc_ledger_append(cc_ledger *l, const cc_ledger_entry *e);

/* ---- the persisted metadata as a whole ---- */
void cc_meta_init(cc_meta *m, uint32_t install_nonce);   /* an empty, valid state */
cc_status cc_meta_validate(const cc_meta *m);

/* ---- checked size arithmetic and the encoded size of version 1 ---- */
cc_status cc_size_add(size_t a, size_t b, size_t *out);
cc_status cc_size_mul(size_t a, size_t b, size_t *out);
/* The exact encoded length of the metadata blob for these counts. events_per_turn has n_turns entries; the report
 * and its history are present iff has_report (n_turns must be 0 without it). Counts over a limit return CC_E_COUNT. */
cc_status cc_wire_meta_size_for(unsigned n_records, unsigned n_ledger, bool has_report, unsigned n_turns,
                                const unsigned *events_per_turn, size_t *out);
cc_status cc_wire_meta_size(const cc_meta *m, size_t *out);

#endif
