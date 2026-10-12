/* custom-core: the version-1 rematch policy (task C05; docs/CUSTOM-CONTENT-CONTRACTS.md, section 2.11).
 *
 * Pure like the rest of the module: no globals, no I/O, no allocation, no clock, and no ambient random numbers (the only
 * randomness is the custom stream below, started from a seed the caller hands in and recorded in the schedule). Nothing here
 * spawns an enemy, reads the game, touches a wallet or grants a reward: it decides, as data, and the adapter (C10) acts.
 *
 * The flow of one run:
 *
 *   cc_roster_release_except(roster, run)      a record still held by an earlier run is returned (a crash, a quit)
 *   cc_rematch_start(schedule, roster, ...)    at the start of the run: freeze the candidates, draw ONCE, choose at most one
 *   cc_rematch_window(schedule, roster, enc)   at each window wave (27, 77, 127, 177), when the adapter knows what kind of
 *                                              encounter the wave will be: the proposal (offer it, or why not)
 *   cc_rematch_decide(schedule, roster, yes)   the player's answer: accept (the record is CHALLENGED) or decline (consumed)
 *   cc_rematch_result(schedule, roster, won)   the rematch's outcome: DEFEATED (reward owed) or back to STORED
 *   cc_reward_claim(meta, record)              the once-per-record reward, as one transaction on the persisted metadata
 *   cc_roster_release(roster, run)             the run is over: whatever it still held goes back to STORED
 *
 * Every mutating call is atomic in memory (all checks before the first write, an error changes nothing), idempotent where a
 * retry or a reload can repeat it (the second call returns the first call's answer and changes nothing), and never draws.
 * Reading (cc_rematch_view, a replayed window) never changes the schedule, so scouting cannot reroll anything. */
#ifndef CC_SCHEDULER_H
#define CC_SCHEDULER_H

#include "cc/model.h"

/* ---- the custom policy stream, algorithm 1 (CC_RNG_ALGO_V1) ----
 * SplitMix64: state += 0x9E3779B97F4A7C15; z = state; z = (z ^ z >> 30) * 0xBF58476D1CE4E5B9; z = (z ^ z >> 27) * 0x94D049BB133111EB;
 * result z ^ z >> 31, all modulo 2^64. tools/golden_rematch.py is an independent copy; tests/scheduler_test.c holds its values. */
uint64_t cc_rng_v1_next(uint64_t *state);
/* One integer in [0, n), n >= 1: the next output modulo n. The bias is below n / 2^64 (under 10^-18 for the 16 records a roster
 * holds), which is accepted instead of a rejection loop so that a run's draw is always exactly one step of the stream. */
unsigned cc_rng_v1_pick(uint64_t *state, unsigned n);

/* ---- what the adapter reports about a window's encounter ---- */
enum {
    CC_ENC_ORDINARY_WILD = 0,   /* an ordinary wild encounter */
    CC_ENC_TRAINER,
    CC_ENC_FIXED_OR_SCRIPTED,
    CC_ENC_MYSTERY,
    CC_ENC_BOSS,
    CC_ENC_FINAL,               /* the final wave of the mode */
    CC_ENC_UNSUPPORTED,         /* a mechanic the rematch cannot reproduce */
    CC_ENC_COUNT_
};
enum {
    CC_ENF_FUSED   = 1u << 0,   /* the wild enemy is a fusion */
    CC_ENF_TERA    = 1u << 1,
    CC_ENF_DOUBLES = 1u << 2,
    CC_ENF_ALL     = 7u
};
typedef struct {
    uint16_t wave;              /* the wave being generated */
    uint8_t kind;               /* CC_ENC_* */
    uint8_t flags;              /* CC_ENF_*: only meaningful for an ordinary wild encounter (any kind refuses first) */
} cc_encounter;

/* The skip reason an encounter earns, CC_SKIP_NONE when it is an ordinary, nonfinal, nonboss, supported single wild encounter.
 * Precedence: final, boss, trainer, mystery, fixed/scripted, unsupported (kind), then fused/Tera/doubles (reported as
 * CC_SKIP_UNSUPPORTED). CC_E_RANGE for a kind or flag this version does not define (nothing is written to *reason). */
cc_status cc_encounter_classify(const cc_encounter *enc, uint8_t *reason);

/* ---- the proposal ---- */
enum {
    CC_PROP_NONE = 0,           /* nothing to offer here: why says why */
    CC_PROP_OFFER,              /* offer the rematch of record_id at this window */
    CC_PROP_RESUME              /* this window's offer was accepted earlier (a reload): rebuild the same rematch */
};
enum {
    CC_WHY_OFFER = 0,           /* with CC_PROP_OFFER / CC_PROP_RESUME */
    CC_WHY_NOT_A_WINDOW,        /* the wave is not one of cc_window_waves */
    CC_WHY_NO_CANDIDATE,        /* the roster had no eligible record when the run started */
    CC_WHY_SKIPPED,             /* this window was skipped (now, or earlier and replayed): `skip` says why */
    CC_WHY_DECLINED,            /* the offer was declined: consumed, never offered again this run */
    CC_WHY_RESOLVED             /* the rematch was accepted at another window */
};
/* The level the enemy gets. There is no scaling: the rematch enemy is built by the game's own path at the level the wave
 * already generated, with the standard species-stat calculation. custom-core knows no level and no stat formula. */
enum { CC_LEVEL_ORDINARY_WAVE = 1 };
#define CC_REMATCH_REWARD_VOUCHERS 1u    /* the fixed allowance: one regular egg voucher, once per record */

typedef struct {
    uint8_t kind;               /* CC_PROP_* */
    uint8_t why;                /* CC_WHY_* */
    uint8_t skip;               /* CC_SKIP_*: the reason for CC_WHY_SKIPPED, else CC_SKIP_NONE */
    uint8_t opportunity;        /* 1..CC_NUM_WINDOWS for an offer or a window answer, else 0 */
    uint16_t wave;              /* the window's wave for an offer, else the wave asked about */
    uint8_t level_rule;         /* CC_LEVEL_ORDINARY_WAVE for an offer, else 0 */
    uint8_t reward_vouchers;    /* CC_REMATCH_REWARD_VOUCHERS for an offer, else 0 (paid only through cc_reward_claim) */
    cc_record_id record_id;     /* the record to rebuild, 0 for CC_PROP_NONE */
} cc_proposal;

/* ---- the run's schedule ---- */

/* Starts a run's schedule in `*s` (every byte of it is written) and returns the roster's one eligible record to SCHEDULED.
 *   run_uid        the uid the schedule is keyed by: it must stay the same when the run is loaded again
 *   seed           the run's custom seed, recorded in the schedule; the adapter keeps it independent of gameplay and gacha RNG
 *   representable  NULL, or roster->count bytes, nonzero where the adapter can rebuild that record's snapshot; a zero byte
 *                  keeps the record out of the candidates (an incompatible snapshot is never silently reshaped)
 * Candidates are the records with life STORED (a scheduled, challenged, defeated or claimed record is never one), in record_id
 * order, frozen into s->frozen_ids. No candidate: state NO_CANDIDATE and no draw. Otherwise ONE draw of the stream picks
 * frozen_ids[pick] and the stream's state after it is stored, so nothing ever draws again. The windows start PENDING.
 * Refused (nothing changes): a NULL, run_uid 0, an invalid roster, a record still held by any run (release it first,
 * CC_E_CONSISTENCY). */
cc_status cc_rematch_start(cc_schedule *s, cc_roster *r, cc_run_uid run_uid, uint64_t seed, const uint8_t *representable);

/* The adapter's report of a window wave's encounter, before the wave is generated. Fills *out. Calling it for a wave that is not a
 * window, for a run with no candidate, or again for a window already answered changes nothing and returns that answer.
 *   - the first pending window, an ineligible encounter: the window is SKIPPED with its reason (CC_PROP_NONE). When it was the last
 *     one the schedule is EXHAUSTED and the record goes back to STORED.
 *   - the first pending window, an eligible encounter: the schedule is OFFERED (CC_PROP_OFFER).
 *   - the offered window again (a reload at the prompt): the same offer.   - the accepted window again: CC_PROP_RESUME.
 *   - a later window while an earlier one is still pending or offered: CC_E_ORDER (a window was never reported), no change.
 * The classification is read once, when the window is first reported; a replay does not look at it again. */
cc_status cc_rematch_window(cc_schedule *s, cc_roster *r, const cc_encounter *enc, cc_proposal *out);

/* The player's answer to the offer. Accept: the window and the schedule are ACCEPTED and the record is CHALLENGED at that wave.
 * Decline: DECLINED, the record goes back to STORED, and the schedule is consumed (no window after it is ever offered). The same
 * answer again changes nothing (*replayed); the other answer, or no offer, is CC_E_LIFECYCLE. */
cc_status cc_rematch_decide(cc_schedule *s, cc_roster *r, bool accept, bool *replayed);

/* The accepted rematch's outcome. Won: CHALLENGED -> DEFEATED (the reward is owed, claimed separately). Lost: back to STORED (the
 * schedule stays ACCEPTED: no second try this run). The same outcome again changes nothing; a different one is CC_E_LIFECYCLE. */
cc_status cc_rematch_result(cc_schedule *s, cc_roster *r, bool won, bool *replayed);

/* A read-only look, for the radar and for tests. Never changes either argument and never draws. */
typedef struct {
    uint8_t state;              /* CC_SCHED_* */
    uint8_t n_candidates;       /* the frozen roster */
    uint8_t n_pending;          /* windows not yet answered */
    uint8_t next_opportunity;   /* 1..CC_NUM_WINDOWS: the next window to be reported or the offered one, else 0 */
    uint16_t next_wave;         /* its wave, else 0 */
    cc_record_id record_id;     /* the chosen record, 0 if none */
} cc_rematch_view;
cc_status cc_rematch_view_of(const cc_schedule *s, cc_rematch_view *out);

/* ---- records held by a run ---- */

/* Returns every record held (SCHEDULED or CHALLENGED) by `run_uid` to STORED; a DEFEATED record stays (its reward is owed).
 * *released, if given, is how many. cc_roster_release_except does it for every run but `keep_run_uid`: what a crash left behind. */
cc_status cc_roster_release(cc_roster *r, cc_run_uid run_uid, unsigned *released);
cc_status cc_roster_release_except(cc_roster *r, cc_run_uid keep_run_uid, unsigned *released);

/* ---- the once-per-record reward ---- */

enum { CC_RW_NOT_WON = 0, CC_RW_OWED, CC_RW_CLAIMED };   /* a record's reward, from its lifecycle alone */
typedef struct {
    uint8_t state;              /* CC_RW_* */
    uint8_t vouchers;           /* CC_REMATCH_REWARD_VOUCHERS iff OWED, else 0 */
} cc_reward_entitlement;
/* A pure function of the record's lifecycle: DEFEATED is owed, REWARD_CLAIMED is claimed, everything else was not won. It does not
 * depend on any write: a write that failed leaves the record DEFEATED, so the reward is still owed exactly once. */
cc_status cc_reward_entitlement_of(const cc_nemesis *n, cc_reward_entitlement *out);

typedef struct {
    bool replayed;              /* the reward was already claimed: nothing was changed */
    cc_txn_id txn_id;           /* the claim's transaction (the record's reward_txn on a replay) */
} cc_claim_result;
/* Claims the reward of `record_id` as one transaction on `m` (which must pass cc_meta_validate): allocates a transaction id,
 * appends the REWARD_CLAIM ledger entry (keyed by record) and moves DEFEATED -> REWARD_CLAIMED with that transaction. A record
 * that is already REWARD_CLAIMED is a replay and changes nothing; one that is not DEFEATED is CC_E_LIFECYCLE; an unknown record
 * is CC_E_RANGE; a ledger that disagrees with the record is CC_E_CONSISTENCY. Every check precedes the first write. The adapter
 * (C08) grants the voucher only after the write of the staged copy succeeded. */
cc_status cc_reward_claim(cc_meta *m, cc_record_id record_id, cc_claim_result *res);

#endif
