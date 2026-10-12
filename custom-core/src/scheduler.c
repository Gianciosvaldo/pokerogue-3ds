/* custom-core: the version-1 rematch policy (see cc/scheduler.h and section 2.11 of the contracts). */
#include <string.h>

#include "cc/scheduler.h"

/* ---- the custom policy stream, algorithm 1 ---- */

uint64_t cc_rng_v1_next(uint64_t *state)
{
    *state += 0x9E3779B97F4A7C15ull;
    uint64_t z = *state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

unsigned cc_rng_v1_pick(uint64_t *state, unsigned n)
{
    const uint64_t r = cc_rng_v1_next(state);
    return n ? (unsigned)(r % n) : 0;
}

/* ---- encounters ---- */

cc_status cc_encounter_classify(const cc_encounter *enc, uint8_t *reason)
{
    if (!enc || !reason) return CC_E_NULL;
    if (enc->kind >= CC_ENC_COUNT_ || (enc->flags & ~CC_ENF_ALL)) return CC_E_RANGE;
    uint8_t r;
    switch (enc->kind) {
    case CC_ENC_FINAL: r = CC_SKIP_FINAL; break;
    case CC_ENC_BOSS: r = CC_SKIP_BOSS; break;
    case CC_ENC_TRAINER: r = CC_SKIP_TRAINER; break;
    case CC_ENC_MYSTERY: r = CC_SKIP_MYSTERY; break;
    case CC_ENC_FIXED_OR_SCRIPTED: r = CC_SKIP_FIXED_OR_SCRIPTED; break;
    case CC_ENC_UNSUPPORTED: r = CC_SKIP_UNSUPPORTED; break;
    default: r = enc->flags ? CC_SKIP_UNSUPPORTED : CC_SKIP_NONE; break;
    }
    *reason = r;
    return CC_OK;
}

/* ---- helpers ---- */

static int find_record(const cc_roster *r, cc_record_id id)
{
    for (unsigned i = 0; i < r->count; i++)
        if (r->rec[i].record_id == id) return (int)i;
    return -1;
}

static bool is_held(const cc_nemesis *n)
{
    return n->life == CC_LIFE_SCHEDULED || n->life == CC_LIFE_CHALLENGED;
}

/* The schedule and the roster must agree while a window can still be answered: the chosen record is scheduled by this very run. */
static cc_status check_agreement(const cc_schedule *s, const cc_roster *r)
{
    if (s->state != CC_SCHED_ACTIVE && s->state != CC_SCHED_OFFERED) return CC_OK;
    const int i = find_record(r, s->chosen_id);
    if (i < 0) return CC_E_CONSISTENCY;
    const cc_nemesis *n = &r->rec[i];
    return n->life == CC_LIFE_SCHEDULED && n->sched_run_uid == s->run_uid ? CC_OK : CC_E_CONSISTENCY;
}

static cc_status check_inputs(const cc_schedule *s, const cc_roster *r)
{
    cc_status st = cc_schedule_validate(s);
    if (st != CC_OK) return st;
    st = cc_roster_validate(r);
    if (st != CC_OK) return st;
    return check_agreement(s, r);
}

static int window_of(uint16_t wave)
{
    for (int i = 0; i < CC_NUM_WINDOWS; i++)
        if (cc_window_waves[i] == wave) return i;
    return -1;
}

static int first_in_state(const cc_schedule *s, uint8_t state)
{
    for (int i = 0; i < CC_NUM_WINDOWS; i++)
        if (s->windows[i].state == state) return i;
    return -1;
}

/* ---- the run's schedule ---- */

cc_status cc_rematch_start(cc_schedule *s, cc_roster *r, cc_run_uid run_uid, uint64_t seed, const uint8_t *representable)
{
    if (!s || !r) return CC_E_NULL;
    if (run_uid == 0) return CC_E_RANGE;
    cc_status st = cc_roster_validate(r);
    if (st != CC_OK) return st;
    for (unsigned i = 0; i < r->count; i++)
        if (is_held(&r->rec[i])) return CC_E_CONSISTENCY;          /* release what an earlier run left first */

    cc_schedule ns;
    memset(&ns, 0, sizeof ns);
    ns.run_uid = run_uid;
    int chosen_slot = -1;
    for (unsigned i = 0; i < r->count; i++) {
        const cc_nemesis *n = &r->rec[i];
        if (n->life != CC_LIFE_STORED || cc_nemesis_support(n) != CC_SUPPORT_OK) continue;
        if (representable && !representable[i]) continue;
        ns.frozen_ids[ns.n_frozen++] = n->record_id;
    }
    if (ns.n_frozen == 0) {
        ns.state = CC_SCHED_NO_CANDIDATE;
    } else {
        uint64_t state = seed;
        const unsigned pick = cc_rng_v1_pick(&state, ns.n_frozen);  /* the run's one and only draw */
        ns.state = CC_SCHED_ACTIVE;
        ns.rng_algo = CC_RNG_ALGO_V1;
        ns.n_windows = CC_NUM_WINDOWS;
        ns.rng_seed = seed;
        ns.rng_state = state;
        ns.chosen_id = ns.frozen_ids[pick];
        for (int i = 0; i < CC_NUM_WINDOWS; i++) ns.windows[i].wave = cc_window_waves[i];
        chosen_slot = find_record(r, ns.chosen_id);
    }
    st = cc_schedule_validate(&ns);
    if (st != CC_OK) return st;
    if (chosen_slot >= 0) {
        cc_nemesis rec = r->rec[chosen_slot];
        st = cc_nemesis_transition(&rec, CC_LIFE_SCHEDULED, run_uid, 0, 0);
        if (st != CC_OK) return st;
        r->rec[chosen_slot] = rec;
    }
    *s = ns;
    return CC_OK;
}

/* `out` already holds the window's opportunity and wave (set before the switch): an offer adds what the offer is. */
static void fill_offer(cc_proposal *out, uint8_t kind, cc_record_id id)
{
    out->kind = kind;
    out->why = CC_WHY_OFFER;
    out->level_rule = CC_LEVEL_ORDINARY_WAVE;
    out->reward_vouchers = (uint8_t)CC_REMATCH_REWARD_VOUCHERS;
    out->record_id = id;
}

cc_status cc_rematch_window(cc_schedule *s, cc_roster *r, const cc_encounter *enc, cc_proposal *out)
{
    if (!s || !r || !enc || !out) return CC_E_NULL;
    uint8_t reason;
    cc_status st = cc_encounter_classify(enc, &reason);
    if (st != CC_OK) return st;
    st = check_inputs(s, r);
    if (st != CC_OK) return st;

    cc_proposal p;
    memset(&p, 0, sizeof p);
    p.wave = enc->wave;
    const int j = window_of(enc->wave);
    if (j < 0) {
        p.why = CC_WHY_NOT_A_WINDOW;
        *out = p;
        return CC_OK;
    }
    p.opportunity = (uint8_t)(j + 1);
    if (s->state == CC_SCHED_NO_CANDIDATE) {
        p.why = CC_WHY_NO_CANDIDATE;
        *out = p;
        return CC_OK;
    }
    const cc_window *w = &s->windows[j];
    switch (w->state) {
    case CC_WIN_SKIPPED:
        p.why = CC_WHY_SKIPPED;
        p.skip = w->reason;
        *out = p;
        return CC_OK;
    case CC_WIN_OFFERED:
        fill_offer(&p, CC_PROP_OFFER, s->chosen_id);
        *out = p;
        return CC_OK;
    case CC_WIN_ACCEPTED:
        fill_offer(&p, CC_PROP_RESUME, s->chosen_id);
        *out = p;
        return CC_OK;
    case CC_WIN_DECLINED:
        p.why = CC_WHY_DECLINED;
        *out = p;
        return CC_OK;
    default:
        break;
    }
    /* a pending window */
    if (s->state == CC_SCHED_DECLINED) {
        p.why = CC_WHY_DECLINED;
        *out = p;
        return CC_OK;
    }
    if (s->state == CC_SCHED_ACCEPTED) {
        p.why = CC_WHY_RESOLVED;
        *out = p;
        return CC_OK;
    }
    if (s->state != CC_SCHED_ACTIVE || j != first_in_state(s, CC_WIN_PENDING)) return CC_E_ORDER;

    cc_schedule ns = *s;
    if (reason != CC_SKIP_NONE) {
        ns.windows[j].state = CC_WIN_SKIPPED;
        ns.windows[j].reason = reason;
        int slot = -1;
        cc_nemesis rec;
        memset(&rec, 0, sizeof rec);
        if (j == CC_NUM_WINDOWS - 1) {                              /* the last window: nothing this run, the record is free again */
            ns.state = CC_SCHED_EXHAUSTED;
            slot = find_record(r, s->chosen_id);
            rec = r->rec[slot];
            st = cc_nemesis_transition(&rec, CC_LIFE_STORED, 0, 0, 0);
            if (st != CC_OK) return st;
        }
        st = cc_schedule_validate(&ns);
        if (st != CC_OK) return st;
        if (slot >= 0) r->rec[slot] = rec;
        *s = ns;
        p.why = CC_WHY_SKIPPED;
        p.skip = reason;
    } else {
        ns.windows[j].state = CC_WIN_OFFERED;
        ns.state = CC_SCHED_OFFERED;
        st = cc_schedule_validate(&ns);
        if (st != CC_OK) return st;
        *s = ns;
        fill_offer(&p, CC_PROP_OFFER, s->chosen_id);
    }
    *out = p;
    return CC_OK;
}

cc_status cc_rematch_decide(cc_schedule *s, cc_roster *r, bool accept, bool *replayed)
{
    if (!s || !r) return CC_E_NULL;
    if (replayed) *replayed = false;
    cc_status st = check_inputs(s, r);
    if (st != CC_OK) return st;
    if (s->state == CC_SCHED_ACCEPTED && accept) {
        if (replayed) *replayed = true;
        return CC_OK;
    }
    if (s->state == CC_SCHED_DECLINED && !accept) {
        if (replayed) *replayed = true;
        return CC_OK;
    }
    if (s->state != CC_SCHED_OFFERED) return CC_E_LIFECYCLE;
    const int j = first_in_state(s, CC_WIN_OFFERED);
    const int slot = find_record(r, s->chosen_id);
    cc_nemesis rec = r->rec[slot];
    cc_schedule ns = *s;
    if (accept) {
        st = cc_nemesis_transition(&rec, CC_LIFE_CHALLENGED, s->run_uid, s->windows[j].wave, 0);
        ns.state = CC_SCHED_ACCEPTED;
        ns.windows[j].state = CC_WIN_ACCEPTED;
    } else {
        st = cc_nemesis_transition(&rec, CC_LIFE_STORED, 0, 0, 0);
        ns.state = CC_SCHED_DECLINED;
        ns.windows[j].state = CC_WIN_DECLINED;
    }
    if (st != CC_OK) return st;
    st = cc_schedule_validate(&ns);
    if (st != CC_OK) return st;
    r->rec[slot] = rec;
    *s = ns;
    return CC_OK;
}

cc_status cc_rematch_result(cc_schedule *s, cc_roster *r, bool won, bool *replayed)
{
    if (!s || !r) return CC_E_NULL;
    if (replayed) *replayed = false;
    cc_status st = check_inputs(s, r);
    if (st != CC_OK) return st;
    if (s->state != CC_SCHED_ACCEPTED) return CC_E_LIFECYCLE;
    const int slot = find_record(r, s->chosen_id);
    if (slot < 0) return CC_E_CONSISTENCY;
    cc_nemesis rec = r->rec[slot];
    if (rec.life == CC_LIFE_CHALLENGED && rec.sched_run_uid == s->run_uid) {
        st = cc_nemesis_transition(&rec, won ? CC_LIFE_DEFEATED : CC_LIFE_STORED, 0, 0, 0);
        if (st != CC_OK) return st;
        r->rec[slot] = rec;
        return CC_OK;
    }
    /* the outcome was already applied: the same one again is a replay, a different one is refused */
    const bool was_won = (rec.life == CC_LIFE_DEFEATED || rec.life == CC_LIFE_REWARD_CLAIMED) && rec.sched_run_uid == s->run_uid;
    const bool was_lost = rec.life == CC_LIFE_STORED;
    if ((won && was_won) || (!won && was_lost)) {
        if (replayed) *replayed = true;
        return CC_OK;
    }
    return CC_E_LIFECYCLE;
}

cc_status cc_rematch_view_of(const cc_schedule *s, cc_rematch_view *out)
{
    if (!s || !out) return CC_E_NULL;
    cc_status st = cc_schedule_validate(s);
    if (st != CC_OK) return st;
    cc_rematch_view v;
    memset(&v, 0, sizeof v);
    v.state = s->state;
    v.n_candidates = s->n_frozen;
    v.record_id = s->chosen_id;
    int next = -1;
    if (s->state == CC_SCHED_ACTIVE) {
        for (int i = 0; i < CC_NUM_WINDOWS; i++)
            if (s->windows[i].state == CC_WIN_PENDING) v.n_pending++;
        next = first_in_state(s, CC_WIN_PENDING);
    } else if (s->state == CC_SCHED_OFFERED) {
        next = first_in_state(s, CC_WIN_OFFERED);
    }
    if (next >= 0) {
        v.next_opportunity = (uint8_t)(next + 1);
        v.next_wave = s->windows[next].wave;
    }
    *out = v;
    return CC_OK;
}

/* ---- records held by a run ---- */

static cc_status release_where(cc_roster *r, cc_run_uid uid, bool except, unsigned *released)
{
    if (released) *released = 0;
    if (!r) return CC_E_NULL;
    if (uid == 0 && !except) return CC_E_RANGE;                    /* "except 0" releases every held record: nothing is run 0 */
    cc_status st = cc_roster_validate(r);
    if (st != CC_OK) return st;
    for (unsigned i = 0; i < r->count; i++) {
        if (!is_held(&r->rec[i])) continue;
        if ((r->rec[i].sched_run_uid == uid) == except) continue;
        cc_nemesis rec = r->rec[i];
        st = cc_nemesis_transition(&rec, CC_LIFE_STORED, 0, 0, 0);
        if (st != CC_OK) return st;
        r->rec[i] = rec;                                           /* a valid roster holds at most one record, so nothing was changed before */
        if (released) *released = 1;
        break;
    }
    return CC_OK;
}

cc_status cc_roster_release(cc_roster *r, cc_run_uid run_uid, unsigned *released)
{
    return release_where(r, run_uid, false, released);
}

cc_status cc_roster_release_except(cc_roster *r, cc_run_uid keep_run_uid, unsigned *released)
{
    return release_where(r, keep_run_uid, true, released);
}

/* ---- the once-per-record reward ---- */

cc_status cc_reward_entitlement_of(const cc_nemesis *n, cc_reward_entitlement *out)
{
    if (!n || !out) return CC_E_NULL;
    cc_status st = cc_nemesis_validate(n);
    if (st != CC_OK) return st;
    cc_reward_entitlement e;
    memset(&e, 0, sizeof e);
    if (n->life == CC_LIFE_DEFEATED) {
        e.state = CC_RW_OWED;
        e.vouchers = (uint8_t)CC_REMATCH_REWARD_VOUCHERS;
    } else if (n->life == CC_LIFE_REWARD_CLAIMED) {
        e.state = CC_RW_CLAIMED;
    }
    *out = e;
    return CC_OK;
}

cc_status cc_reward_claim(cc_meta *m, cc_record_id record_id, cc_claim_result *res)
{
    if (!m || !res) return CC_E_NULL;
    memset(res, 0, sizeof *res);
    cc_status st = cc_meta_validate(m);
    if (st != CC_OK) return st;
    const int slot = find_record(&m->roster, record_id);
    if (slot < 0) return CC_E_RANGE;
    const cc_nemesis *n = &m->roster.rec[slot];
    cc_ledger_entry prior;
    const bool have_prior = cc_ledger_find(&m->ledger, CC_TXN_REWARD_CLAIM, 0, record_id, &prior);
    if (n->life == CC_LIFE_REWARD_CLAIMED) {
        if (have_prior && prior.txn_id != n->reward_txn) return CC_E_CONSISTENCY;
        res->replayed = true;
        res->txn_id = n->reward_txn;
        return CC_OK;
    }
    if (n->life != CC_LIFE_DEFEATED) return CC_E_LIFECYCLE;
    if (have_prior) return CC_E_CONSISTENCY;                      /* the ledger says it was claimed, the record says it was not */

    cc_counters counters = m->counters;
    cc_txn_id txn_id;
    st = cc_alloc_txn_id(&counters, &txn_id);
    if (st != CC_OK) return st;
    cc_ledger ledger = m->ledger;
    const cc_ledger_entry entry = {txn_id, n->sched_run_uid, record_id, CC_TXN_REWARD_CLAIM, CC_TXO_CLAIMED};
    st = cc_ledger_append(&ledger, &entry);
    if (st != CC_OK) return st;
    cc_nemesis rec = *n;
    st = cc_nemesis_transition(&rec, CC_LIFE_REWARD_CLAIMED, 0, 0, txn_id);
    if (st != CC_OK) return st;

    m->counters = counters;
    m->ledger = ledger;
    m->roster.rec[slot] = rec;
    res->txn_id = txn_id;
    return CC_OK;
}
