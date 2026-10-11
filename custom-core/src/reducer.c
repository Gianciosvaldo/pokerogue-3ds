/* custom-core: the bounded history reducer (docs/CUSTOM-CONTENT-CONTRACTS.md, section 2.8). */
#include <string.h>

#include "cc/reducer.h"

/* ======================================================================== batch digest */

/* FNV-1a over the fields one by one in little-endian order, never over struct bytes: padding and layout cannot leak in. It
 * only tells a replay of the last batch from a different batch with the same id, so run_uid and batch_id (equal when it is
 * compared) and n_obs (implied by the observations hashed) are left out. */
#define FNV_BASIS 14695981039346656037ULL
#define FNV_PRIME 1099511628211ULL

static void fnv8(uint64_t *h, uint8_t v)
{
    *h ^= v;
    *h *= FNV_PRIME;
}

static void fnv16(uint64_t *h, uint16_t v)
{
    fnv8(h, (uint8_t)v);
    fnv8(h, (uint8_t)(v >> 8));
}

static void fnv32(uint64_t *h, uint32_t v)
{
    fnv16(h, (uint16_t)v);
    fnv16(h, (uint16_t)(v >> 16));
}

static uint64_t batch_digest(const cc_batch *b)
{
    uint64_t h = FNV_BASIS;
    fnv32(&h, b->battle_seq);
    fnv16(&h, b->wave);
    fnv16(&h, b->turn);
    fnv8(&h, b->flags);
    for (unsigned i = 0; i < b->n_obs; i++) {
        const cc_obs *o = &b->obs[i];
        fnv8(&h, o->kind);
        fnv8(&h, (uint8_t)o->battler);
        fnv8(&h, (uint8_t)o->target);
        fnv8(&h, o->battler_form);
        fnv8(&h, o->target_form);
        fnv16(&h, o->battler_species);
        fnv16(&h, o->target_species);
        fnv32(&h, o->battler_id);
        fnv32(&h, o->target_id);
        fnv16(&h, o->id);
        fnv32(&h, (uint32_t)o->value);
        fnv32(&h, (uint32_t)o->hp_after);
        fnv32(&h, (uint32_t)o->hp_max_after);
        fnv8(&h, o->status_after);
        fnv16(&h, o->src_known);
        fnv8(&h, o->src_side);
        fnv8(&h, o->src_form);
        fnv16(&h, o->src_species);
        fnv16(&h, o->src_move);
        fnv32(&h, o->src_id);
    }
    return h;
}

/* ======================================================================== one observation -> at most one event */

typedef cc_snapshot cc_tracker[CC_BATTLER_MAX + 1];

/* The movement of the position's snapshot HP since its previous snapshot, if there is one for the same species and form. */
static bool measure(const cc_snapshot *prev, const cc_obs *o, int32_t *delta)
{
    if (o->hp_after < 0) return false;
    if (prev->species != o->battler_species || prev->form != o->battler_form) return false;   /* species 0: none */
    *delta = o->hp_after - prev->hp;                            /* both >= 0: cannot overflow */
    return true;
}

/* Builds the event for a valid observation and advances the tracker. Returns false when the observation produces no event
 * (MOVE_ANIM, a PR_EV_HP of 0); the tracker is then untouched. Pure in everything but `trk`, `*e` and `*conflicts`. */
static bool make_event(cc_tracker trk, const cc_obs *o, cc_event *e, uint32_t *conflicts)
{
    memset(e, 0, sizeof *e);
    if (o->kind == CC_OBS_MOVE_ANIM) return false;
    if (o->kind == CC_OBS_HP && o->value == 0) return false;

    const unsigned p = (unsigned)o->battler;
    e->target_side = p >= 2 ? CC_SIDE_ENEMY : CC_SIDE_PLAYER;
    e->target_species = o->battler_species;
    e->target_form = o->battler_form;
    if (o->battler_id) {
        e->known |= CC_EK_TARGET_ID;
        e->target_id = o->battler_id;
    }
    /* the adapter's attribution, if any, verbatim; the reducer never adds to it */
    e->known |= o->src_known;
    e->source_side = o->src_side;
    e->source_form = o->src_form;
    e->source_species = o->src_species;
    e->move_id = o->src_move;
    e->source_id = o->src_id;

    int32_t delta = 0;
    const bool measured = measure(&trk[p], o, &delta);
    switch (o->kind) {
    case CC_OBS_DAMAGE:
        e->kind = CC_EV_DAMAGE;
        e->known |= CC_EK_RESULT | CC_EK_SHOWN;
        e->result = (uint8_t)o->id;
        e->shown = o->value;
        if (measured) {
            if (delta > 0) {
                (*conflicts)++;                                 /* HP rose across a hit: an unseen change, no value invented */
            } else {
                e->known |= CC_EK_HP_DELTA;
                e->hp_delta = delta;
            }
        }
        break;
    case CC_OBS_HP:
        if (o->value > 0) {
            e->kind = CC_EV_HEAL;
            e->known |= CC_EK_SHOWN;
            e->shown = o->value;
            if (measured) {
                if (delta < 0) {
                    (*conflicts)++;
                } else {
                    e->known |= CC_EK_HP_DELTA;
                    e->hp_delta = delta;
                }
            }
        } else {
            e->kind = CC_EV_HP_LOSS;
            e->known |= CC_EK_HP_DELTA;
            e->hp_delta = o->value;                             /* the engine's own measurement (phases.c:3032) */
        }
        break;
    case CC_OBS_FAINT:
        e->kind = CC_EV_FAINT;
        break;
    case CC_OBS_SWITCH_OUT:
        e->kind = CC_EV_SWITCH_OUT;
        break;
    default:                                                    /* CC_OBS_SWITCH_IN: cc_obs_validate allows nothing else */
        e->kind = CC_EV_SWITCH_IN;
        break;
    }
    if (o->hp_after >= 0) {
        e->known |= CC_EK_HP_AFTER;
        e->hp_after = o->hp_after;
    }

    if (o->kind == CC_OBS_FAINT || o->kind == CC_OBS_SWITCH_OUT) {
        memset(&trk[p], 0, sizeof trk[p]);                      /* the occupant is gone */
    } else if (o->hp_after >= 0) {
        trk[p].species = o->battler_species;
        trk[p].form = o->battler_form;
        trk[p].hp = o->hp_after;
    } else {
        memset(&trk[p], 0, sizeof trk[p]);                      /* HP now unknown */
    }
    return true;
}

/* ======================================================================== the plan (first pass, changes nothing) */

typedef struct {
    bool replay;                    /* an exact repeat of the last accepted batch */
    bool gap;                       /* batch ids were skipped */
    bool new_turn;
    bool fresh_tracker;             /* forget every position before the batch */
    uint32_t n_events;
    uint32_t ignored;
    uint32_t sign_conflicts;
    uint64_t digest;
} cc_plan;

static cc_status plan_batch(const cc_reducer *r, const cc_batch *b, int32_t *bad_obs, cc_plan *pl)
{
    if (!r || !b) return CC_E_NULL;
    if (b->n_obs > 0 && !b->obs) return CC_E_NULL;
    if (b->flags & ~(uint8_t)CC_BF_ALL) return CC_E_UNKNOWN_BITS;
    if (b->n_obs > CC_MAX_BATCH_OBS) return CC_E_COUNT;
    if (b->run_uid == 0 || b->batch_id == 0 || b->battle_seq < 1 || b->wave < 1) return CC_E_RANGE;
    if (r->run_uid == 0 || r->closed) return CC_E_LIFECYCLE;
    if (b->run_uid != r->run_uid) return CC_E_CONSISTENCY;
    const cc_history *h = &r->hist;
    /* a state this module cannot have produced must not index out of bounds; cc_reducer_validate is the full check */
    if (h->n_turns > CC_HISTORY_TURNS) return CC_E_CONSISTENCY;
    const cc_turn *last = h->n_turns ? &h->turns[h->n_turns - 1] : NULL;
    if (last && last->n_events > CC_MAX_EVENTS_PER_TURN) return CC_E_CONSISTENCY;
    if (r->last_batch_id != 0 && !last) return CC_E_CONSISTENCY;   /* an accepted batch always leaves its turn */

    if (b->batch_id < r->last_batch_id) return CC_E_ORDER;
    pl->digest = batch_digest(b);
    if (b->batch_id == r->last_batch_id) {
        if (pl->digest != r->last_digest) return CC_E_DUPLICATE;
        pl->replay = true;
        return CC_OK;
    }
    pl->gap = r->last_batch_id != 0 && b->batch_id - r->last_batch_id > 1;

    pl->new_turn = true;
    pl->fresh_tracker = pl->gap || (b->flags & CC_BF_SOURCE_LOSS) != 0;
    if (last) {
        if (b->battle_seq < last->battle_seq) return CC_E_ORDER;
        if (b->battle_seq == last->battle_seq) {
            if (b->wave != last->wave) return CC_E_CONSISTENCY;
            if (b->turn < last->turn) return CC_E_ORDER;
            pl->new_turn = b->turn != last->turn;
        } else {
            if (b->wave < last->wave) return CC_E_ORDER;
            pl->fresh_tracker = true;                           /* another battle: other Pokemon */
        }
    }
    if (!pl->new_turn && (b->flags & CC_BF_BEGAN_MID)) return CC_E_CONSISTENCY;

    cc_tracker trk;
    if (pl->fresh_tracker) memset(trk, 0, sizeof trk);
    else memcpy(trk, r->pos, sizeof trk);
    cc_event ev;
    for (unsigned i = 0; i < b->n_obs; i++) {
        cc_status s = cc_obs_validate(&b->obs[i]);
        if (s == CC_OK && make_event(trk, &b->obs[i], &ev, &pl->sign_conflicts)) {
            s = cc_event_validate(&ev);                         /* cannot fail for a valid observation; a guard, not a rule */
            pl->n_events++;
        } else if (s == CC_OK) {
            pl->ignored++;
        }
        if (s != CC_OK) {
            *bad_obs = (int32_t)i;
            return s;
        }
    }
    if ((uint64_t)r->next_seq + pl->n_events > UINT32_MAX) return CC_E_OVERFLOW;
    return CC_OK;
}

/* ======================================================================== applying the plan (second pass) */

static void apply_batch(cc_reducer *r, const cc_batch *b, const cc_plan *pl, cc_reduce_result *res)
{
    cc_history *h = &r->hist;
    if (pl->gap) h->turns[h->n_turns - 1].flags |= CC_TF_SOURCE_LOSS;       /* a gap implies an earlier batch, so a turn */

    cc_turn *t;
    if (pl->new_turn) {
        if (h->n_turns == CC_HISTORY_TURNS) {
            memmove(&h->turns[0], &h->turns[1], (CC_HISTORY_TURNS - 1) * sizeof h->turns[0]);
            res->turn_evicted = 1;
        } else {
            h->n_turns++;
        }
        t = &h->turns[h->n_turns - 1];
        memset(t, 0, sizeof *t);
        t->battle_seq = b->battle_seq;
        t->wave = b->wave;
        t->turn = b->turn;
        if (b->flags & CC_BF_BEGAN_MID) t->flags |= CC_TF_BEGAN_MID;
    } else {
        t = &h->turns[h->n_turns - 1];
    }
    if (pl->gap || (b->flags & CC_BF_SOURCE_LOSS)) t->flags |= CC_TF_SOURCE_LOSS;

    /* Appending one event at a time would shift the turn once per event over the limit. Work out the total drop first: the
     * oldest existing events go first, then the first of the batch's own events when it alone exceeds the limit. */
    const unsigned existing = t->n_events;
    const unsigned total = existing + pl->n_events;
    const unsigned drop = total > CC_MAX_EVENTS_PER_TURN ? total - CC_MAX_EVENTS_PER_TURN : 0;
    const unsigned drop_old = drop < existing ? drop : existing;
    const unsigned drop_new = drop - drop_old;
    if (drop_old) {
        memmove(&t->events[0], &t->events[drop_old], (existing - drop_old) * sizeof t->events[0]);
        t->n_events = (uint8_t)(existing - drop_old);       /* the freed tail is refilled below: a dropping batch ends with a full turn */
    }

    cc_tracker trk;
    if (pl->fresh_tracker) memset(trk, 0, sizeof trk);
    else memcpy(trk, r->pos, sizeof trk);
    cc_event ev;
    uint32_t conflicts = 0;
    unsigned produced = 0;
    for (unsigned i = 0; i < b->n_obs; i++) {
        if (!make_event(trk, &b->obs[i], &ev, &conflicts)) continue;
        ev.seq = r->next_seq++;                                 /* an evicted event still consumes its number */
        if (produced++ < drop_new) continue;
        memcpy(&t->events[t->n_events], &ev, sizeof ev);
        t->n_events++;
    }
    if (drop) {
        const uint32_t d = (uint32_t)t->dropped + drop;
        t->dropped = d > 0xFFFFu ? 0xFFFFu : (uint16_t)d;
        t->flags |= CC_TF_EVICTED;
    }
    memcpy(r->pos, trk, sizeof trk);
    r->last_batch_id = b->batch_id;
    r->last_digest = pl->digest;

    res->new_turn = pl->new_turn;
    res->gap = pl->gap;
    res->events_added = (uint16_t)pl->n_events;
    res->ignored = (uint16_t)pl->ignored;
    res->evicted = (uint16_t)drop;
    res->sign_conflicts = (uint16_t)pl->sign_conflicts;
}

/* ======================================================================== public API */

cc_status cc_reducer_init(cc_reducer *r)
{
    if (!r) return CC_E_NULL;
    memset(r, 0, sizeof *r);
    r->next_seq = 1;
    return CC_OK;
}

cc_status cc_reducer_begin_run(cc_reducer *r, cc_run_uid uid, bool resumed)
{
    if (!r) return CC_E_NULL;
    if (uid == 0) return CC_E_RANGE;
    if (uid == r->run_uid) return CC_E_DUPLICATE;
    cc_reducer_init(r);
    r->run_uid = uid;
    if (resumed) r->hist.flags |= CC_HF_BEGAN_MID;
    return CC_OK;
}

cc_status cc_reducer_end_run(cc_reducer *r)
{
    if (!r) return CC_E_NULL;
    if (r->run_uid == 0) return CC_E_LIFECYCLE;
    r->closed = 1;
    return CC_OK;
}

cc_status cc_reduce(cc_reducer *r, const cc_batch *b, cc_reduce_result *out)
{
    cc_reduce_result res;
    memset(&res, 0, sizeof res);
    int32_t bad_obs = -1;
    cc_plan pl;
    memset(&pl, 0, sizeof pl);
    cc_status s = plan_batch(r, b, &bad_obs, &pl);
    if (s == CC_OK) {
        if (pl.replay) res.duplicate = 1;
        else apply_batch(r, b, &pl, &res);
    }
    res.bad_obs = bad_obs;
    if (out) *out = res;
    return s;
}

static bool all_zero(const void *p, size_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    while (n--)
        if (*b++) return false;
    return true;
}

cc_status cc_reducer_validate(const cc_reducer *r)
{
    if (!r) return CC_E_NULL;
    cc_status s = cc_history_validate(&r->hist);
    if (s != CC_OK) return s;
    if (r->closed > 1) return CC_E_RANGE;
    if (r->next_seq < 1) return CC_E_RANGE;
    bool any_pos = false;
    for (unsigned p = 0; p <= CC_BATTLER_MAX; p++) {
        const cc_snapshot *sn = &r->pos[p];
        if (sn->species == 0) {
            if (sn->form || sn->hp) return CC_E_NONCANONICAL;
        } else {
            any_pos = true;
            if (sn->species > CC_SPECIES_ID_MAX) return CC_E_RANGE;
            if (sn->hp < 0) return CC_E_RANGE;
        }
    }
    /* unused slots are zero, so equal input means equal bytes */
    const cc_history *h = &r->hist;
    for (unsigned i = 0; i < CC_HISTORY_TURNS; i++) {
        const cc_turn *t = &h->turns[i];
        if (i >= h->n_turns) {
            if (!all_zero(t, sizeof *t)) return CC_E_NONCANONICAL;
        } else if (!all_zero(&t->events[t->n_events], (CC_MAX_EVENTS_PER_TURN - t->n_events) * sizeof t->events[0])) {
            return CC_E_NONCANONICAL;
        }
    }

    if (r->run_uid == 0) {                                      /* no run begun: exactly the initial state */
        if (r->closed || r->last_batch_id || r->last_digest || r->next_seq != 1 || h->n_turns || h->flags || any_pos)
            return CC_E_CONSISTENCY;
        return CC_OK;
    }
    if (r->last_batch_id == 0) {                                /* nothing accepted yet */
        if (r->last_digest || r->next_seq != 1 || h->n_turns || any_pos) return CC_E_CONSISTENCY;
        return CC_OK;
    }
    if (h->n_turns == 0) return CC_E_CONSISTENCY;               /* an accepted batch always leaves its turn */
    bool have_last = false;
    uint32_t newest = 0;
    for (unsigned i = 0; i < h->n_turns; i++) {
        const cc_turn *t = &h->turns[i];
        if (i > 0 && t->wave < h->turns[i - 1].wave) return CC_E_ORDER;
        if (i > 0 && t->battle_seq == h->turns[i - 1].battle_seq && t->wave != h->turns[i - 1].wave) return CC_E_CONSISTENCY;
        if (t->n_events) {
            have_last = true;
            newest = t->events[t->n_events - 1].seq;
        }
    }
    if (have_last && newest >= r->next_seq) return CC_E_CONSISTENCY;
    return CC_OK;
}

/* ======================================================================== per-turn totals */

static uint64_t magnitude(int32_t v)
{
    return v < 0 ? (uint64_t)(-(int64_t)v) : (uint64_t)v;
}

static void add_amounts(cc_amounts *a, const cc_event *e)
{
    a->n++;
    if (e->known & CC_EK_SHOWN) {
        a->n_shown++;
        a->shown += (uint64_t)e->shown;                         /* >= 0, validated */
    }
    if (e->known & CC_EK_HP_DELTA) {
        a->n_measured++;
        a->measured += magnitude(e->hp_delta);
    }
}

cc_status cc_turn_summarize(const cc_turn *t, cc_turn_summary *out)
{
    if (!t || !out) return CC_E_NULL;
    cc_status s = cc_turn_validate(t);
    if (s != CC_OK) return s;
    cc_turn_summary sm;
    memset(&sm, 0, sizeof sm);
    sm.exact = t->flags == 0;
    sm.flags = t->flags;
    sm.dropped = t->dropped;
    sm.n_events = t->n_events;
    for (unsigned i = 0; i < t->n_events; i++) {
        const cc_event *e = &t->events[i];
        const unsigned side = e->target_side;
        switch (e->kind) {
        case CC_EV_DAMAGE: add_amounts(&sm.damage[side], e); break;
        case CC_EV_HEAL: add_amounts(&sm.heal[side], e); break;
        case CC_EV_HP_LOSS: add_amounts(&sm.hp_loss[side], e); break;
        case CC_EV_FAINT: sm.n_faint[side]++; break;
        case CC_EV_SWITCH_OUT: sm.n_switch_out[side]++; break;
        case CC_EV_SWITCH_IN: sm.n_switch_in[side]++; break;
        default: sm.n_status[side]++; break;                    /* CC_EV_STATUS: cc_turn_validate allows nothing else */
        }
    }
    memcpy(out, &sm, sizeof sm);
    return CC_OK;
}
