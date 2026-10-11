/* custom-core: observation and history validation. */
#include "cc/model.h"

static bool actor_is_zero(const cc_actor *a)
{
    return !a->id && !a->species && !a->form && !a->side && !a->level;
}

cc_status cc_obs_validate(const cc_obs *o)
{
    if (!o) return CC_E_NULL;
    if (o->kind == CC_OBS_NONE || o->kind >= CC_OBS_KIND_COUNT_) return CC_E_RANGE;
    if (o->battler < 0 || o->battler > CC_BATTLER_MAX) return CC_E_RANGE;
    if (o->target < -1 || o->target > CC_BATTLER_MAX) return CC_E_RANGE;
    if (o->battler_species == 0 || o->battler_species > CC_SPECIES_ID_MAX) return CC_E_RANGE;
    if (o->target_species > CC_SPECIES_ID_MAX) return CC_E_RANGE;
    if (o->target < 0) {
        if (o->target_species || o->target_form || o->target_id) return CC_E_NONCANONICAL;
    } else if (o->target_species == 0) {
        return CC_E_RANGE;
    }
    if (o->hp_after < -1 || o->hp_max_after < -1) return CC_E_RANGE;
    if ((o->hp_after == -1) != (o->hp_max_after == -1)) return CC_E_CONSISTENCY;
    if (o->hp_max_after == 0) return CC_E_RANGE;
    if (o->hp_after > o->hp_max_after) return CC_E_CONSISTENCY;
    if (o->status_after > CC_STATUS_MAX) return CC_E_RANGE;
    switch (o->kind) {
    case CC_OBS_DAMAGE:
        if (o->id > CC_HIT_RESULT_MAX) return CC_E_RANGE;
        if (o->value < 0) return CC_E_RANGE;
        break;
    case CC_OBS_HP:
        if (o->id) return CC_E_NONCANONICAL;
        break;
    case CC_OBS_MOVE_ANIM:
        if (o->id < 1 || o->id > CC_MOVE_ID_MAX) return CC_E_RANGE;
        if (o->value) return CC_E_NONCANONICAL;
        break;
    case CC_OBS_SWITCH_IN:
        if (o->id) return CC_E_NONCANONICAL;
        if (o->value < 0 || o->value > 5) return CC_E_RANGE;      /* the party slot */
        break;
    default:                                                      /* FAINT, SWITCH_OUT */
        if (o->id || o->value) return CC_E_NONCANONICAL;
        break;
    }

    /* optional attribution: the adapter's assertion, with the same known-bit rules as cc_event */
    const uint16_t sk = o->src_known;
    if (sk & ~(uint16_t)CC_EK_SOURCE_ALL) return CC_E_UNKNOWN_BITS;
    if ((sk & (CC_EK_SOURCE_ID | CC_EK_SOURCE_SPECIES)) && !(sk & CC_EK_SOURCE_SIDE)) return CC_E_CONSISTENCY;
    if (!(sk & CC_EK_SOURCE_SIDE) && o->src_side) return CC_E_NONCANONICAL;
    if (!(sk & CC_EK_SOURCE_ID) && o->src_id) return CC_E_NONCANONICAL;
    if (!(sk & CC_EK_SOURCE_SPECIES) && (o->src_species || o->src_form)) return CC_E_NONCANONICAL;
    if (!(sk & CC_EK_MOVE) && o->src_move) return CC_E_NONCANONICAL;
    if ((sk & CC_EK_SOURCE_SIDE) && o->src_side > CC_SIDE_ENEMY) return CC_E_RANGE;
    if ((sk & CC_EK_SOURCE_ID) && o->src_id == 0) return CC_E_RANGE;
    if ((sk & CC_EK_SOURCE_SPECIES) && (o->src_species < 1 || o->src_species > CC_SPECIES_ID_MAX)) return CC_E_RANGE;
    if ((sk & CC_EK_MOVE) && (o->src_move < 1 || o->src_move > CC_MOVE_ID_MAX)) return CC_E_RANGE;
    /* only a hit, a heal or a faint can name a source; a loss without a hit, a switch or an animation never does */
    if (sk && o->kind != CC_OBS_DAMAGE && o->kind != CC_OBS_FAINT && !(o->kind == CC_OBS_HP && o->value > 0))
        return CC_E_CONSISTENCY;
    return CC_OK;
}

cc_status cc_actor_validate(const cc_actor *a)
{
    if (!a) return CC_E_NULL;
    if (a->id == 0) return CC_E_RANGE;
    if (a->species < 1 || a->species > CC_SPECIES_ID_MAX) return CC_E_RANGE;
    if (a->side > CC_SIDE_ENEMY) return CC_E_RANGE;
    if (a->level < 1) return CC_E_RANGE;
    return CC_OK;
}

/* Which optional fields each event kind may carry, and which it needs at least one of. */
static const uint16_t kind_allowed[CC_EV_KIND_COUNT_] = {
    [CC_EV_DAMAGE] = CC_EK_TARGET_ID | CC_EK_SOURCE_SIDE | CC_EK_SOURCE_ID | CC_EK_SOURCE_SPECIES | CC_EK_MOVE |
                     CC_EK_RESULT | CC_EK_SHOWN | CC_EK_HP_DELTA | CC_EK_HP_AFTER,
    [CC_EV_HEAL] = CC_EK_TARGET_ID | CC_EK_SOURCE_SIDE | CC_EK_SOURCE_ID | CC_EK_SOURCE_SPECIES | CC_EK_MOVE |
                   CC_EK_SHOWN | CC_EK_HP_DELTA | CC_EK_HP_AFTER,
    [CC_EV_HP_LOSS] = CC_EK_TARGET_ID | CC_EK_SHOWN | CC_EK_HP_DELTA | CC_EK_HP_AFTER,
    [CC_EV_FAINT] = CC_EK_TARGET_ID | CC_EK_SOURCE_SIDE | CC_EK_SOURCE_ID | CC_EK_SOURCE_SPECIES | CC_EK_MOVE |
                    CC_EK_HP_AFTER,
    [CC_EV_SWITCH_OUT] = CC_EK_TARGET_ID | CC_EK_HP_AFTER,
    [CC_EV_SWITCH_IN] = CC_EK_TARGET_ID | CC_EK_HP_AFTER,
    [CC_EV_STATUS] = CC_EK_TARGET_ID | CC_EK_SOURCE_SIDE | CC_EK_SOURCE_ID | CC_EK_SOURCE_SPECIES | CC_EK_MOVE |
                     CC_EK_SHOWN,
};
static const uint16_t kind_need_any[CC_EV_KIND_COUNT_] = {
    [CC_EV_DAMAGE] = CC_EK_SHOWN | CC_EK_HP_DELTA,
    [CC_EV_HEAL] = CC_EK_SHOWN | CC_EK_HP_DELTA,
    [CC_EV_HP_LOSS] = CC_EK_SHOWN | CC_EK_HP_DELTA,
    [CC_EV_STATUS] = CC_EK_SHOWN,
};

cc_status cc_event_validate(const cc_event *e)
{
    if (!e) return CC_E_NULL;
    if (e->kind == CC_EV_NONE || e->kind >= CC_EV_KIND_COUNT_) return CC_E_RANGE;
    if (e->known & ~(uint16_t)CC_EK_ALL) return CC_E_UNKNOWN_BITS;
    if (e->target_side > CC_SIDE_ENEMY) return CC_E_RANGE;
    if (e->target_species < 1 || e->target_species > CC_SPECIES_ID_MAX) return CC_E_RANGE;
    const uint16_t k = e->known;
    if (k & ~kind_allowed[e->kind]) return CC_E_CONSISTENCY;
    if (kind_need_any[e->kind] && !(k & kind_need_any[e->kind])) return CC_E_CONSISTENCY;
    if ((k & (CC_EK_SOURCE_ID | CC_EK_SOURCE_SPECIES)) && !(k & CC_EK_SOURCE_SIDE)) return CC_E_CONSISTENCY;

    /* a clear "known" bit means a zero value */
    if (!(k & CC_EK_TARGET_ID) && e->target_id) return CC_E_NONCANONICAL;
    if (!(k & CC_EK_SOURCE_SIDE) && e->source_side) return CC_E_NONCANONICAL;
    if (!(k & CC_EK_SOURCE_ID) && e->source_id) return CC_E_NONCANONICAL;
    if (!(k & CC_EK_SOURCE_SPECIES) && (e->source_species || e->source_form)) return CC_E_NONCANONICAL;
    if (!(k & CC_EK_MOVE) && e->move_id) return CC_E_NONCANONICAL;
    if (!(k & CC_EK_RESULT) && e->result) return CC_E_NONCANONICAL;
    if (!(k & CC_EK_SHOWN) && e->shown) return CC_E_NONCANONICAL;
    if (!(k & CC_EK_HP_DELTA) && e->hp_delta) return CC_E_NONCANONICAL;
    if (!(k & CC_EK_HP_AFTER) && e->hp_after) return CC_E_NONCANONICAL;

    /* known values in range */
    if ((k & CC_EK_TARGET_ID) && e->target_id == 0) return CC_E_RANGE;
    if ((k & CC_EK_SOURCE_SIDE) && e->source_side > CC_SIDE_ENEMY) return CC_E_RANGE;
    if ((k & CC_EK_SOURCE_ID) && e->source_id == 0) return CC_E_RANGE;
    if ((k & CC_EK_SOURCE_SPECIES) && (e->source_species < 1 || e->source_species > CC_SPECIES_ID_MAX)) return CC_E_RANGE;
    if ((k & CC_EK_MOVE) && (e->move_id < 1 || e->move_id > CC_MOVE_ID_MAX)) return CC_E_RANGE;
    if ((k & CC_EK_RESULT) && e->result > CC_HIT_RESULT_MAX) return CC_E_RANGE;
    if (k & CC_EK_SHOWN) {
        if (e->kind == CC_EV_STATUS) {
            if (e->shown < 1 || e->shown > CC_STATUS_MAX) return CC_E_RANGE;
        } else if (e->shown < 0) {
            return CC_E_RANGE;                                    /* a magnitude; the kind carries the direction */
        }
    }
    if (k & CC_EK_HP_DELTA) {
        if (e->kind == CC_EV_HEAL ? e->hp_delta < 0 : e->hp_delta > 0) return CC_E_CONSISTENCY;
    }
    if ((k & CC_EK_HP_AFTER) && e->hp_after < 0) return CC_E_RANGE;
    return CC_OK;
}

cc_status cc_turn_validate(const cc_turn *t)
{
    if (!t) return CC_E_NULL;
    if (t->flags & ~(uint8_t)CC_TF_ALL) return CC_E_UNKNOWN_BITS;
    if (t->battle_seq < 1 || t->wave < 1) return CC_E_RANGE;
    if (t->n_events > CC_MAX_EVENTS_PER_TURN) return CC_E_COUNT;
    /* eviction is the only way events are dropped by this module: the flag, the count and a full turn go together */
    if (((t->flags & CC_TF_EVICTED) != 0) != (t->dropped != 0)) return CC_E_CONSISTENCY;
    if ((t->flags & CC_TF_EVICTED) && t->n_events != CC_MAX_EVENTS_PER_TURN) return CC_E_CONSISTENCY;
    for (unsigned i = 0; i < t->n_events; i++) {
        cc_status s = cc_event_validate(&t->events[i]);
        if (s != CC_OK) return s;
        if (i > 0) {
            if (t->events[i].seq == t->events[i - 1].seq) return CC_E_DUPLICATE;
            if (t->events[i].seq < t->events[i - 1].seq) return CC_E_ORDER;
        }
    }
    return CC_OK;
}

cc_status cc_history_validate(const cc_history *h)
{
    if (!h) return CC_E_NULL;
    if (h->flags & ~(uint8_t)CC_HF_ALL) return CC_E_UNKNOWN_BITS;
    if (h->n_turns > CC_HISTORY_TURNS) return CC_E_COUNT;
    bool have_seq = false;
    uint32_t last_seq = 0;
    for (unsigned i = 0; i < h->n_turns; i++) {
        const cc_turn *t = &h->turns[i];
        cc_status s = cc_turn_validate(t);
        if (s != CC_OK) return s;
        if (i > 0) {
            const cc_turn *p = &h->turns[i - 1];
            if (t->battle_seq == p->battle_seq && t->turn == p->turn) return CC_E_DUPLICATE;
            if (t->battle_seq < p->battle_seq || (t->battle_seq == p->battle_seq && t->turn < p->turn)) return CC_E_ORDER;
        }
        for (unsigned j = 0; j < t->n_events; j++) {
            if (have_seq) {
                if (t->events[j].seq == last_seq) return CC_E_DUPLICATE;
                if (t->events[j].seq < last_seq) return CC_E_ORDER;
            }
            have_seq = true;
            last_seq = t->events[j].seq;
        }
    }
    return CC_OK;
}

cc_status cc_terminal_validate(const cc_terminal *t, bool committed)
{
    if (!t) return CC_E_NULL;
    if (t->run_uid == 0) return CC_E_RANGE;
    if (committed ? t->txn_id == 0 : t->txn_id != 0) return CC_E_RANGE;
    if (t->outcome == CC_OUT_NONE || t->outcome >= CC_OUT_COUNT_) return CC_E_RANGE;
    if (t->known & ~(uint8_t)CC_TK_ALL) return CC_E_UNKNOWN_BITS;
    if (t->biome > CC_BIOME_ID_MAX) return CC_E_RANGE;
    if (t->wave < 1) return CC_E_RANGE;
    if (t->outcome != CC_OUT_DEFEAT && t->known) return CC_E_CONSISTENCY;      /* only a defeat has a finisher or a fallen */
    if (t->known & CC_TK_FINISH_MOVE) {
        if (t->finish_move < 1 || t->finish_move > CC_MOVE_ID_MAX) return CC_E_RANGE;
    } else if (t->finish_move) {
        return CC_E_NONCANONICAL;
    }
    if (t->known & CC_TK_FINISH_ACTOR) {
        cc_status s = cc_actor_validate(&t->finish_actor);
        if (s != CC_OK) return s;
    } else if (!actor_is_zero(&t->finish_actor)) {
        return CC_E_NONCANONICAL;
    }
    if (t->known & CC_TK_FALLEN_ACTOR) {
        cc_status s = cc_actor_validate(&t->fallen_actor);
        if (s != CC_OK) return s;
    } else if (!actor_is_zero(&t->fallen_actor)) {
        return CC_E_NONCANONICAL;
    }
    return CC_OK;
}
