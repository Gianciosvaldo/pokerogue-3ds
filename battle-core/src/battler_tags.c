/* Battler tags being added, lapsed and removed: Pokemon.addTag, lapseTag, lapseTags, removeTag,
 * removeTagsBySourceId (src/field/pokemon.ts) and, per tag type, what getBattlerTag() builds
 * (src/data/battler-tags.ts): its constructor's fields, canAdd, onAdd, onOverlap, lapse and onRemove.
 *
 * Only some tag types have those ported (pr_tag_type_ported). The others still take part where
 * upstream would reach them: a lapse or removal that would run one of their methods reports
 * the tag type (PR_FEAT_TAG_0 + type) and leaves the tag as it is. Which lapses reach a tag
 * comes from the lapse types upstream builds each tag with (pr_battler_tags_info, dumped). */
#include "pr/engine.h"
#include <math.h>
#include <string.h>

/* The tag types whose constructor, canAdd, onAdd, onOverlap, lapse and onRemove are ported. */
bool pr_tag_type_ported(int type)
{
    switch (type) {
    case BATTLER_TAG_TYPE_CHARGING:     /* SerializableBattlerTag(CHARGING, CUSTOM, 1, sourceMove, sourceId) */
    case BATTLER_TAG_TYPE_FLYING:       /* SemiInvulnerableTag: onAdd / onRemove only hide and show the sprite */
    case BATTLER_TAG_TYPE_UNDERGROUND:
    case BATTLER_TAG_TYPE_UNDERWATER:
    case BATTLER_TAG_TYPE_HIDDEN:
    case BATTLER_TAG_TYPE_FLINCHED:     /* FlinchedTag */
    case BATTLER_TAG_TYPE_CONFUSED:     /* ConfusedTag */
    case BATTLER_TAG_TYPE_FRENZY:       /* FrenzyTag (MoveLockTag): Outrage, Thrash, Petal Dance, Raging Fury */
    case BATTLER_TAG_TYPE_TRAPPED:      /* TrappedTag: Mean Look, Block, Spider Web... */
    case BATTLER_TAG_TYPE_NO_RETREAT:   /* NoRetreatTag */
    case BATTLER_TAG_TYPE_ALWAYS_GET_HIT:          /* Glaive Rush: SerializableBattlerTag(PRE_MOVE, 1) */
    case BATTLER_TAG_TYPE_RECEIVE_DOUBLE_DAMAGE:
    case BATTLER_TAG_TYPE_MINIMIZED:    /* MinimizeTag */
    case BATTLER_TAG_TYPE_CHARGED:      /* TypeBoostTag(CHARGED, sourceMove, ELECTRIC, 2, oneUse) */
    case BATTLER_TAG_TYPE_TAR_SHOT:     /* TarShotTag */
    case BATTLER_TAG_TYPE_AUTOTOMIZED:  /* AutotomizedTag */
    case BATTLER_TAG_TYPE_RECHARGING:   /* RechargingTag: Hyper Beam and co. */
    case BATTLER_TAG_TYPE_SEEDED:       /* SeedTag: Leech Seed */
    case BATTLER_TAG_TYPE_SUBSTITUTE:   /* SubstituteTag: Substitute, Shed Tail */
    case BATTLER_TAG_TYPE_ENDURING:     /* EnduringTag: Endure */
        return true;
    }
    return pr_damaging_trap_move(type) != 0 || pr_battler_tag_is(type, PR_TAGCLASS_PROTECTED);
}

/* DamagingTrapTag's subclasses (Bind, Wrap, Fire Spin...): the move each one names as its
 * sourceMove, whatever move added it; 0 for any other tag type */
int pr_damaging_trap_move(int type)
{
    switch (type) {
    case BATTLER_TAG_TYPE_BIND: return MOVE_ID_BIND;
    case BATTLER_TAG_TYPE_WRAP: return MOVE_ID_WRAP;
    case BATTLER_TAG_TYPE_FIRE_SPIN: return MOVE_ID_FIRE_SPIN;
    case BATTLER_TAG_TYPE_WHIRLPOOL: return MOVE_ID_WHIRLPOOL;
    case BATTLER_TAG_TYPE_CLAMP: return MOVE_ID_CLAMP;
    case BATTLER_TAG_TYPE_SAND_TOMB: return MOVE_ID_SAND_TOMB;
    case BATTLER_TAG_TYPE_MAGMA_STORM: return MOVE_ID_MAGMA_STORM;
    case BATTLER_TAG_TYPE_SNAP_TRAP: return MOVE_ID_SNAP_TRAP;
    case BATTLER_TAG_TYPE_THUNDER_CAGE: return MOVE_ID_THUNDER_CAGE;
    case BATTLER_TAG_TYPE_INFESTATION: return MOVE_ID_INFESTATION;
    }
    return 0;
}

static bool lapses_on(int type, int lapse_type)
{
    return (pr_battler_tags_info[type].lapse_mask >> lapse_type) & 1u;
}

static void note_tag(pr_battle *bt, int type)
{
    pr_note_unported(bt, (uint16_t)(PR_FEAT_TAG_0 + type));
}

/* the ported TrappedTag subclasses; each is source-linked (isSourceLinked) */
static bool is_trap(int type)
{
    return type == BATTLER_TAG_TYPE_TRAPPED || type == BATTLER_TAG_TYPE_NO_RETREAT || pr_damaging_trap_move(type);
}

static int find_tag(const pr_pokemon *p, int type)
{
    for (int i = 0; i < p->n_tags; i++)
        if (p->tags[i].type == type) return i;
    return -1;
}

static void splice_tag(pr_pokemon *p, int i)
{
    memmove(&p->tags[i], &p->tags[i + 1], (size_t)(p->n_tags - i - 1) * sizeof p->tags[0]);
    p->n_tags--;
}

/* ---------------------------------------------------------------- per tag type */

/* getBattlerTag(tagType, turnCount, sourceMove, sourceId): the fields its constructor sets
 * (`if (sourceMove)`: 0 stays none; a sourceId only where the class takes one) */
static pr_battler_tag construct(int type, int turn_count, int source_move, int64_t source_id)
{
    pr_battler_tag t;
    memset(&t, 0, sizeof t);
    t.type = (uint8_t)type;
    t.turn_count = (int16_t)turn_count;
    t.source_move = (uint16_t)source_move;
    t.source_id = -1;
    t.source_index = -1;
    switch (type) {
    case BATTLER_TAG_TYPE_CHARGING:
        t.turn_count = 1;
        t.source_id = source_id;
        break;
    case BATTLER_TAG_TYPE_TRAPPED:                /* TrappedTag(tagType, CUSTOM, turnCount, sourceMove, sourceId) */
        t.source_id = source_id;
        break;
    case BATTLER_TAG_TYPE_NO_RETREAT:             /* NoRetreatTag(sourceId) */
        t.turn_count = 0;
        t.source_move = MOVE_ID_NO_RETREAT;
        t.source_id = source_id;
        break;
    case BATTLER_TAG_TYPE_FLINCHED:               /* FlinchedTag(sourceMove) */
    case BATTLER_TAG_TYPE_ALWAYS_GET_HIT:
    case BATTLER_TAG_TYPE_RECEIVE_DOUBLE_DAMAGE:
    case BATTLER_TAG_TYPE_CHARGED:
        t.turn_count = 1;
        break;
    case BATTLER_TAG_TYPE_MINIMIZED:              /* MinimizeTag() */
        t.turn_count = 1;
        t.source_move = MOVE_ID_MINIMIZE;
        break;
    case BATTLER_TAG_TYPE_TAR_SHOT:               /* TarShotTag() */
        t.turn_count = 0;
        t.source_move = 0;
        break;
    case BATTLER_TAG_TYPE_RECHARGING:             /* RechargingTag(sourceMove) */
        t.turn_count = 2;
        break;
    case BATTLER_TAG_TYPE_AUTOTOMIZED:            /* AutotomizedTag(): sourceMove = AUTOTOMIZE */
        t.turn_count = 1;
        t.source_move = MOVE_ID_AUTOTOMIZE;
        break;
    case BATTLER_TAG_TYPE_SEEDED:                 /* SeedTag(sourceId): TURN_END, 1 turn, LEECH_SEED */
        t.turn_count = 1;
        t.source_move = MOVE_ID_LEECH_SEED;
        t.source_id = source_id;
        break;
    case BATTLER_TAG_TYPE_SUBSTITUTE:             /* SubstituteTag(sourceMove, sourceId) */
        t.turn_count = 0;
        t.source_id = source_id;
        break;
    case BATTLER_TAG_TYPE_ENDURING:               /* EnduringTag(tagType, TURN_END, sourceMove) */
        t.turn_count = 0;
        break;
    default:                                      /* CONFUSED, FRENZY, the semi-invulnerable tags */
        if (pr_damaging_trap_move(type)) {        /* BindTag(turnCount, sourceId) and co. */
            t.source_move = (uint16_t)pr_damaging_trap_move(type);
            t.source_id = source_id;
        }
        if (pr_battler_tag_is(type, PR_TAGCLASS_PROTECTED)) t.turn_count = 0;   /* ProtectedTag(sourceMove, tagType) */
        break;
    }
    return t;
}

/* newTag.canAdd(pokemon) */
static bool can_add(pr_battle *bt, pr_engine *e, pr_pokemon *p, const pr_battler_tag *t)
{
    switch (t->type) {
    case BATTLER_TAG_TYPE_CONFUSED:
        /* blocked by Misty Terrain on the ground; isGrounded() runs first, and applies Levitate (not
         * simulated). The terrain's message only when the running phase is a MoveEffectPhase of a
         * status move (queueStatusImmuneMessage(false, MISTY)) */
        if (pr_is_grounded(bt, p, false) && bt->terrain == TERRAIN_MISTY) {
            if (e && e->current >= 0 && e->pool[e->current].type == PH_MOVE_EFFECT
                && pr_moves[e->pool[e->current].move_id].category == MOVE_CATEGORY_STATUS)
                pr_queue_message(bt, e);
            return false;
        }
        return true;
    case BATTLER_TAG_TYPE_TRAPPED: {
        /* getSourcePokemon() searches both parties; only the field is modelled */
        pr_pokemon *source = pr_field_pokemon_by_id(bt, t->source_id);
        if (!source) {
            pr_note_unported(bt, PR_FEAT_PARTY);
            return false;
        }
        if (t->source_move && pr_move_hits_substitute(bt, t->source_move, source, p)) return false;
        return !pr_find_battler_tag_class(p, PR_TAGCLASS_TRAPPED) && !pr_is_of_type(p, POKEMON_TYPE_GHOST);
    }
    case BATTLER_TAG_TYPE_NO_RETREAT:
        return !pr_find_battler_tag_class(p, PR_TAGCLASS_TRAPPED);
    case BATTLER_TAG_TYPE_TAR_SHOT:
        return !p->terastallized;
    case BATTLER_TAG_TYPE_SEEDED:
        return !pr_is_of_type(p, POKEMON_TYPE_GRASS);
    }
    /* DamagingTrapTag: !getTag(TrappedTag) && !getTag(SUBSTITUTE); no Ghost check, no source */
    if (pr_damaging_trap_move(t->type))
        return !pr_find_battler_tag_class(p, PR_TAGCLASS_TRAPPED) && !pr_get_battler_tag(p, BATTLER_TAG_TYPE_SUBSTITUTE);
    return true;
}

/* tag.onAdd(pokemon); t is the tag in p->tags */
static void on_add(pr_battle *bt, pr_engine *e, pr_pokemon *p, pr_battler_tag *t)
{
    switch (t->type) {
    case BATTLER_TAG_TYPE_CONFUSED:
        pr_unshift_new(bt, e, PH_COMMON_ANIM, pr_battler_index(bt, p));   /* CommonAnim.CONFUSION */
        pr_queue_message(bt, e);
        return;
    case BATTLER_TAG_TYPE_TRAPPED:
    case BATTLER_TAG_TYPE_NO_RETREAT:
    case BATTLER_TAG_TYPE_CHARGED:
    case BATTLER_TAG_TYPE_TAR_SHOT:
    case BATTLER_TAG_TYPE_ENDURING:
        pr_queue_message(bt, e);
        return;
    case BATTLER_TAG_TYPE_SEEDED: {
        /* SeedTag.onAdd: nothing without its source (getSourcePokemon searches both parties; only
         * the field is modelled), else the message, then sourceIndex = the source's battler index */
        pr_pokemon *source = t->source_id >= 0 ? pr_field_pokemon_by_id(bt, t->source_id) : NULL;
        if (!source) {
            if (t->source_id >= 0) pr_note_unported(bt, PR_FEAT_PARTY);
            return;
        }
        pr_queue_message(bt, e);
        t->source_index = (int8_t)pr_battler_index(bt, source);
        return;
    }
    case BATTLER_TAG_TYPE_SUBSTITUTE: {
        /* SubstituteTag.onAdd: the doll gets a quarter of its source's max HP, its animation
         * (PokemonAnimPhase), the message, then the Pokemon's damaging traps go */
        pr_pokemon *source = t->source_id >= 0 ? pr_field_pokemon_by_id(bt, t->source_id) : NULL;
        if (!source) {
            if (t->source_id >= 0) pr_note_unported(bt, PR_FEAT_PARTY);
            return;
        }
        t->hp = (int32_t)floor(pr_get_stat(source, STAT_HP, false) / 4.0);
        t->in_focus = false;
        pr_unshift_new(bt, e, PH_POKEMON_ANIM, -1);                      /* SUBSTITUTE_ADD */
        pr_queue_message(bt, e);
        static const int32_t traps[] = {
            BATTLER_TAG_TYPE_BIND, BATTLER_TAG_TYPE_WRAP, BATTLER_TAG_TYPE_FIRE_SPIN, BATTLER_TAG_TYPE_WHIRLPOOL,
            BATTLER_TAG_TYPE_CLAMP, BATTLER_TAG_TYPE_SAND_TOMB, BATTLER_TAG_TYPE_MAGMA_STORM,
            BATTLER_TAG_TYPE_SNAP_TRAP, BATTLER_TAG_TYPE_THUNDER_CAGE, BATTLER_TAG_TYPE_INFESTATION,
        };
        /* findAndRemoveTags(tag => tag instanceof DamagingTrapTag); t may move */
        pr_find_and_remove_tags(bt, e, p, traps, (int)(sizeof traps / sizeof traps[0]));
        return;
    }
    case BATTLER_TAG_TYPE_AUTOTOMIZED:
        if (pr_get_weight(bt, p) > 0.1) pr_queue_message(bt, e);
        t->autotomize_count++;
        return;
    case BATTLER_TAG_TYPE_RECHARGING:
        /* pushMoveQueue({move: NONE, targets: [], useMode: NORMAL}): next turn's placeholder */
        if (p->n_move_queue < PR_MAX_MOVE_QUEUE) {
            pr_turn_move q;
            memset(&q, 0, sizeof q);
            q.move = MOVE_ID_NONE;
            q.use_mode = MOVE_USE_MODE_NORMAL;
            q.result = PR_MOVE_RESULT_UNDEFINED;
            p->move_queue[p->n_move_queue++] = q;
        } else {
            pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT);
            if (e) e->overflow = true;
        }
        return;
    }
    if (pr_damaging_trap_move(t->type)) pr_queue_message(bt, e);   /* TrappedTag.onAdd: getTrapMessage() */
    if (pr_battler_tag_is(t->type, PR_TAGCLASS_PROTECTED)) pr_queue_message(bt, e);   /* ProtectedTag.onAdd */
}

/* existingTag.onOverlap(pokemon) */
static void on_overlap(pr_battle *bt, pr_engine *e, pr_pokemon *p, pr_battler_tag *t)
{
    switch (t->type) {
    case BATTLER_TAG_TYPE_CONFUSED:
    case BATTLER_TAG_TYPE_CHARGED:
        pr_queue_message(bt, e);
        return;
    case BATTLER_TAG_TYPE_AUTOTOMIZED:
        on_add(bt, e, p, t);
        return;
    }
}

/* ConfusedTag.lapse's hit on itself: 40 power, the effective Attack against the effective Defense */
static void confusion_self_hit(pr_battle *bt, pr_engine *e, pr_pokemon *p)
{
    double atk = pr_get_effective_stat(bt, p, STAT_ATK, NULL, -1, false, false);
    double def = pr_get_effective_stat(bt, p, STAT_DEF, NULL, -1, false, false);
    double roll = pr_battle_rand_seed_int(bt, 16, 85) / 100.0;       /* randBattleSeedIntRange(85, 100) */
    double damage = floor((((2.0 * p->level) / 5 + 2) * 40 * atk / def / 50 + 2) * roll);
    if (damage < 1) damage = 1;                                        /* toDmgValue */
    pr_queue_message(bt, e);
    pr_damage_and_update(bt, e, p, damage, HIT_RESULT_CONFUSION, false, NULL);
    pr_cancel_current_move(e, p);
}

/* MoveLockTag.getNextTargets: a random near enemy again for RANDOM_NEAR_ENEMY moves
 * (getMoveTargets), else the last targets */
static void next_targets(pr_battle *bt, const pr_pokemon *p, int move, const pr_turn_move *last, pr_turn_move *out)
{
    if (pr_moves[move].target != MOVE_TARGET_RANDOM_NEAR_ENEMY) {
        memcpy(out->targets, last->targets, sizeof out->targets);
        out->n_targets = last->n_targets;
        return;
    }
    if (pr_move_has_attr((uint16_t)move, PR_AC_VariableTargetAttr)) pr_note_unported(bt, PR_AC_VariableTargetAttr);
    if (bt->double_battle) pr_note_unported(bt, PR_FEAT_DOUBLES);
    /* set = [opponents[randBattleSeedInt(opponents.length)]], opponents = getOpponents(false) (not
     * fainted, field order); then only those active on the field */
    const pr_pokemon *opp[2];
    int n = 0;
    for (int k = 0; k < 4; k++) {
        const pr_pokemon *o = bt->field[k];
        if (o && o->is_player != p->is_player && o->hp > 0 && n < 2) opp[n++] = o;
    }
    int pick = pr_battle_rand_seed_int(bt, n, 0);
    out->n_targets = 0;
    if (pick < n) {
        const pr_pokemon *o = opp[pick];
        if (o->on_field && !o->switch_out_status) out->targets[out->n_targets++] = (int8_t)pr_battler_index(bt, o);
    }
}

/* SeedTag.lapse past its countdown: the opponent standing at sourceIndex gets what the seeded
 * Pokemon loses (an eighth of its max HP), or loses it instead against Liquid Ooze */
static bool seed_drain(pr_battle *bt, pr_engine *e, pr_pokemon *p, const pr_battler_tag *t)
{
    /* pokemon.getOpponents().find(o => o.getBattlerIndex() === sourceIndex): isActive(true) */
    pr_pokemon *source = NULL;
    for (int i = 0; i < 4 && !source; i++) {
        pr_pokemon *o = bt->field[i];
        if (o && o->is_player != p->is_player && o->hp > 0 && o->on_field && !o->switch_out_status
            && t->source_index >= 0 && pr_battler_index(bt, o) == t->source_index)
            source = o;
    }
    if (!source) return true;
    bool cancelled = false;
    pr_ab_params ap = {.pokemon = p, .move = -1, .flag = &cancelled};
    pr_apply_ab_attrs(bt, PR_AC_BlockNonDirectDamageAbAttr, &ap);
    if (cancelled) return true;
    pr_unshift_new(bt, e, PH_COMMON_ANIM, pr_battler_index(bt, source));   /* CommonAnim.LEECH_SEED */
    double hp = floor(pr_get_stat(p, STAT_HP, false) / 8.0);
    double damage = pr_damage_and_update(bt, e, p, hp < 1 ? 1 : hp, HIT_RESULT_INDIRECT, false, NULL);
    /* hasAbilityWithAttr("ReverseDrainAbAttr", false): no canApplyAbility check */
    bool reverse = pr_ability_first_attr(p->ability, PR_AC_ReverseDrainAbAttr)
                   || (p->passive && pr_ability_first_attr(p->passive, PR_AC_ReverseDrainAbAttr));
    pr_unshift_heal(bt, e, pr_battler_index(bt, source), reverse ? -damage : damage,
                    PR_HEAL_MESSAGE | PR_HEAL_NO_FULL_HP_MESSAGE | PR_HEAL_SKIP_ANIM);
    return true;
}

/* SubstituteTag.onHit: the running MoveEffectPhase's first strike, if it hits the doll, says so */
static void substitute_on_hit(pr_battle *bt, pr_engine *e, pr_pokemon *p)
{
    int move;
    pr_pokemon *attacker = pr_move_effect_user(bt, e, &move);
    if (!attacker) return;
    if (attacker->hit_count == attacker->hits_left && pr_move_hits_substitute(bt, move, attacker, p))
        pr_queue_message(bt, e);
}

/* ContactProtectedTag.onContact(attacker, user = the protected Pokemon) */
static void protect_on_contact(pr_battle *bt, pr_engine *e, pr_pokemon *p, const pr_battler_tag *t, pr_pokemon *attacker)
{
    int32_t stat = STAT_ATK;
    int stages = -1;
    switch (t->type) {
    case BATTLER_TAG_TYPE_SPIKY_SHIELD: {
        /* the protected Pokemon's Magic Guard (sic) decides; an eighth of the attacker's max HP */
        bool cancelled = false;
        pr_ab_params ap = {.pokemon = p, .move = -1, .flag = &cancelled};
        pr_apply_ab_attrs(bt, PR_AC_BlockNonDirectDamageAbAttr, &ap);
        if (!cancelled) {
            double d = floor(pr_get_stat(attacker, STAT_HP, false) * (1.0 / 8));
            pr_damage_and_update(bt, e, attacker, d < 1 ? 1 : d, HIT_RESULT_INDIRECT, false, NULL);
        }
        return;
    }
    case BATTLER_TAG_TYPE_BANEFUL_BUNKER:
        pr_try_set_status(bt, e, attacker, STATUS_EFFECT_POISON, p, -1, false, true);
        return;
    case BATTLER_TAG_TYPE_BURNING_BULWARK:
        pr_try_set_status(bt, e, attacker, STATUS_EFFECT_BURN, p, -1, false, true);
        return;
    case BATTLER_TAG_TYPE_KINGS_SHIELD: stat = STAT_ATK; stages = -1; break;
    case BATTLER_TAG_TYPE_OBSTRUCT: stat = STAT_DEF; stages = -2; break;
    case BATTLER_TAG_TYPE_SILK_TRAP: stat = STAT_SPD; stages = -1; break;
    default: return;
    }
    /* StatStageChangePhase({battlerIndex: attacker, changes, sourcePokemon: getSourcePokemon()}):
     * a protection tag has no sourceId, so no source */
    pr_unshift_stat_stage_change(bt, e, pr_battler_index(bt, attacker), &stat, 1, stages, NULL);
}

/* tag.lapse(pokemon, lapseType): whether the tag stays */
static bool lapse(pr_battle *bt, pr_engine *e, pr_pokemon *p, pr_battler_tag *t, int lapse_type)
{
    switch (t->type) {
    case BATTLER_TAG_TYPE_FLINCHED:
        if (lapse_type == BATTLER_TAG_LAPSE_TYPE_PRE_MOVE) {
            pr_cancel_current_move(e, NULL);
            pr_queue_message(bt, e);
            pr_ab_params ap = {.pokemon = p, .move = -1};
            pr_apply_ab_attrs(bt, PR_AC_FlinchEffectAbAttr, &ap);
            return true;
        }
        return lapse_type == BATTLER_TAG_LAPSE_TYPE_TURN_END && --t->turn_count > 0;
    case BATTLER_TAG_TYPE_CONFUSED:
        /* the duration only ticks down on PRE_MOVE */
        if (!(lapse_type == BATTLER_TAG_LAPSE_TYPE_PRE_MOVE && --t->turn_count > 0)) return false;
        pr_queue_message(bt, e);
        pr_unshift_new(bt, e, PH_COMMON_ANIM, pr_battler_index(bt, p));
        if (pr_battle_rand_seed_int(bt, 3, 0) == 0) confusion_self_hit(bt, e, p);
        return true;
    case BATTLER_TAG_TYPE_RECHARGING:
        if (lapse_type == BATTLER_TAG_LAPSE_TYPE_PRE_MOVE) {
            /* "must recharge": the move is cancelled and the placeholder leaves the queue */
            pr_queue_message(bt, e);
            pr_cancel_current_move(e, NULL);
            if (p->n_move_queue) {
                memmove(&p->move_queue[0], &p->move_queue[1], (size_t)(p->n_move_queue - 1) * sizeof p->move_queue[0]);
                p->n_move_queue--;
            }
        }
        return --t->turn_count > 0;
    case BATTLER_TAG_TYPE_MINIMIZED:
    case BATTLER_TAG_TYPE_CHARGED:
        return lapse_type != BATTLER_TAG_LAPSE_TYPE_CUSTOM || --t->turn_count > 0;
    case BATTLER_TAG_TYPE_SEEDED:
        /* lapseType !== CUSTOM || BattlerTag.lapse, then the drain */
        if (lapse_type == BATTLER_TAG_LAPSE_TYPE_CUSTOM && !(--t->turn_count > 0)) return false;
        return seed_drain(bt, e, p, t);
    case BATTLER_TAG_TYPE_SUBSTITUTE:
        /* MOVE and AFTER_MOVE bring the Pokemon in and out of focus (PokemonAnimPhase); HIT: onHit.
         * Only a CUSTOM lapse removes it */
        if (lapse_type == BATTLER_TAG_LAPSE_TYPE_MOVE || lapse_type == BATTLER_TAG_LAPSE_TYPE_AFTER_MOVE) {
            pr_unshift_new(bt, e, PH_POKEMON_ANIM, -1);                  /* SUBSTITUTE_PRE/POST_MOVE */
            t->in_focus = lapse_type == BATTLER_TAG_LAPSE_TYPE_MOVE;
        } else if (lapse_type == BATTLER_TAG_LAPSE_TYPE_HIT) {
            substitute_on_hit(bt, e, p);
        }
        return lapse_type != BATTLER_TAG_LAPSE_TYPE_CUSTOM;
    case BATTLER_TAG_TYPE_ENDURING:
        if (lapse_type == BATTLER_TAG_LAPSE_TYPE_CUSTOM) {
            pr_queue_message(bt, e);
            return true;
        }
        return --t->turn_count > 0;
    case BATTLER_TAG_TYPE_FRENZY: {
        /* MoveLockTag: only a use of the locked move (or none) advances it */
        const pr_turn_move *last = pr_last_move(bt, p, 0);
        if (!t->source_move || !last || (last->move != t->source_move && last->move != MOVE_ID_NONE)) return true;
        bool ret = --t->turn_count > 0 && last->n_targets > 0 && last->result == MOVE_RESULT_SUCCESS;
        if (ret) {
            /* pushMoveQueue({move: sourceMove, targets: nextTargets, useMode: IGNORE_PP}) */
            pr_turn_move q;
            memset(&q, 0, sizeof q);
            q.move = t->source_move;
            q.use_mode = MOVE_USE_MODE_IGNORE_PP;
            q.result = PR_MOVE_RESULT_UNDEFINED;
            pr_turn_move lt = *last;
            next_targets(bt, p, t->source_move, &lt, &q);
            if (p->n_move_queue < PR_MAX_MOVE_QUEUE) p->move_queue[p->n_move_queue++] = q;
            else { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); if (e) e->overflow = true; }
        }
        return ret;
    }
    }
    if (pr_damaging_trap_move(t->type)) {
        /* DamagingTrapTag.lapse (TURN_END): BattlerTag.lapse, then while the tag lasts its message,
         * its animation and an eighth of max HP, unless BlockNonDirectDamageAbAttr (Magic Guard) */
        if (--t->turn_count <= 0) return false;
        pr_queue_message(bt, e);
        pr_unshift_new(bt, e, PH_COMMON_ANIM, pr_battler_index(bt, p));
        bool cancelled = false;
        pr_ab_params ap = {.pokemon = p, .move = -1, .flag = &cancelled};
        pr_apply_ab_attrs(bt, PR_AC_BlockNonDirectDamageAbAttr, &ap);
        if (!cancelled) {
            double damage = floor(pr_get_stat(p, STAT_HP, false) / 8.0);
            pr_damage_and_update(bt, e, p, damage < 1 ? 1 : damage, HIT_RESULT_INDIRECT, false, NULL);
        }
        return true;
    }
    if (pr_battler_tag_is(t->type, PR_TAGCLASS_PROTECTED)) {
        /* ProtectedTag.lapse: on CUSTOM (the hit check lapsing it) the message and stopMultiHit on
         * the running MoveEffectPhase; ContactProtectedTag.lapse then, for a contact move, onContact */
        if (lapse_type != BATTLER_TAG_LAPSE_TYPE_CUSTOM) return --t->turn_count > 0;
        pr_queue_message(bt, e);
        int move;
        pr_pokemon *attacker = pr_move_effect_user(bt, e, &move);
        if (attacker) pr_stop_multi_hit(bt, e, p);
        if (attacker && t->type != BATTLER_TAG_TYPE_PROTECTED
            && pr_move_does_flag_effect_apply_ex(bt, move, MOVE_FLAGS_MAKES_CONTACT, attacker, p, false)) {
            pr_battler_tag copy = *t;
            protect_on_contact(bt, e, p, &copy, attacker);
        }
        return true;
    }
    return --t->turn_count > 0;                   /* BattlerTag.lapse */
}

/* tag.onRemove(pokemon); t is a copy (the tag may already be out of the list) */
static void on_remove(pr_battle *bt, pr_engine *e, pr_pokemon *p, const pr_battler_tag *t)
{
    switch (t->type) {
    case BATTLER_TAG_TYPE_CONFUSED:
    case BATTLER_TAG_TYPE_TRAPPED:
    case BATTLER_TAG_TYPE_NO_RETREAT:
        pr_queue_message(bt, e);
        return;
    case BATTLER_TAG_TYPE_SUBSTITUTE:
        /* the removal animation (PokemonAnimPhase) unless the Pokemon is in focus (its own move
         * removed the doll: the sprite is just destroyed), then the message */
        if (!t->in_focus) pr_unshift_new(bt, e, PH_POKEMON_ANIM, -1);     /* SUBSTITUTE_REMOVE */
        pr_queue_message(bt, e);
        return;
    case BATTLER_TAG_TYPE_FRENZY:
        /* a frenzy that ran its course confuses: addTag(CONFUSED, randBattleSeedIntRange(2, 5)) */
        if (t->turn_count <= 0) {
            int turns = pr_battle_rand_seed_int(bt, 4, 2);
            pr_add_tag(bt, e, p, BATTLER_TAG_TYPE_CONFUSED, turns, 0, -1);
        }
        return;
    }
    if (pr_damaging_trap_move(t->type)) pr_queue_message(bt, e);   /* TrappedTag.onRemove */
}

/* ---------------------------------------------------------------- Pokemon's tag methods */

bool pr_add_tag(pr_battle *bt, pr_engine *e, pr_pokemon *p, int type, int turn_count, int source_move,
                int64_t source_id)
{
    if (!pr_tag_type_ported(type)) {
        note_tag(bt, type);
        return false;
    }
    int i = find_tag(p, type);
    if (i >= 0) {
        on_overlap(bt, e, p, &p->tags[i]);
        return false;
    }

    pr_battler_tag t = construct(type, turn_count, source_move, source_id);

    /* BattlerTagImmunityAbAttr on the Pokemon, then UserFieldBattlerTagImmunityAbAttr on its
     * side in speed order (the Pokemon itself included) */
    bool cancelled = false;
    pr_ab_params ip = {.pokemon = p, .move = -1, .flag = &cancelled, .tag_type = type};
    pr_apply_ab_attrs(bt, PR_AC_BattlerTagImmunityAbAttr, &ip);
    if (cancelled) return false;
    pr_speed_iter it;
    pr_pokemon *ally;
    pr_speed_iter_init(bt, &it, p->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY);
    while ((ally = pr_speed_iter_next(bt, &it))) {
        pr_ab_params ap = {.pokemon = ally, .target = p, .move = -1, .flag = &cancelled, .tag_type = type};
        pr_apply_ab_attrs(bt, PR_AC_UserFieldBattlerTagImmunityAbAttr, &ap);
        if (cancelled) return false;
    }

    if (!can_add(bt, e, p, &t)) return false;
    if (p->n_tags >= PR_MAX_BATTLER_TAGS) {
        pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT);
        if (e) e->overflow = true;
        return false;
    }
    p->tags[p->n_tags++] = t;
    on_add(bt, e, p, &p->tags[p->n_tags - 1]);
    return true;
}

void pr_lapse_tags(pr_battle *bt, pr_engine *e, pr_pokemon *p, int lapse_type)
{
    /* tags.filter(t => lapseType === FAINT || (t.lapseTypes has lapseType && !t.lapse(this, lapseType)))
     *     .forEach(t => { t.onRemove(this); tags.splice(tags.indexOf(t), 1); })
     * Every lapse runs before the first removal. */
    bool gone[PR_MAX_BATTLER_TAGS] = {0};
    int n = p->n_tags;
    for (int i = 0; i < n; i++) {
        int type = p->tags[i].type;
        if (lapse_type != BATTLER_TAG_LAPSE_TYPE_FAINT && !lapses_on(type, lapse_type)) continue;
        if (!pr_tag_type_ported(type)) {
            note_tag(bt, type);
            continue;
        }
        gone[i] = lapse_type == BATTLER_TAG_LAPSE_TYPE_FAINT || !lapse(bt, e, p, &p->tags[i], lapse_type);
    }
    /* in list order: onRemove, then the splice (removed counts the earlier splices; a tag onRemove
     * adds goes to the end, after every one still to go) */
    for (int i = 0, removed = 0; i < n; i++) {
        if (!gone[i]) continue;
        pr_battler_tag t = p->tags[i - removed];
        on_remove(bt, e, p, &t);
        splice_tag(p, i - removed);
        removed++;
    }
}

bool pr_lapse_tag(pr_battle *bt, pr_engine *e, pr_pokemon *p, int type, int lapse_type)
{
    int i = find_tag(p, type);
    if (i < 0) return false;
    if (!pr_tag_type_ported(type)) {
        note_tag(bt, type);
        return true;
    }
    if (!lapse(bt, e, p, &p->tags[i], lapse_type)) {
        pr_battler_tag t = p->tags[i];
        on_remove(bt, e, p, &t);
        splice_tag(p, i);
    }
    return true;
}

void pr_remove_tag(pr_battle *bt, pr_engine *e, pr_pokemon *p, int type)
{
    int i = find_tag(p, type);
    if (i < 0) return;
    if (!pr_tag_type_ported(type)) {
        note_tag(bt, type);
        return;
    }
    pr_battler_tag t = p->tags[i];
    on_remove(bt, e, p, &t);
    splice_tag(p, i);
}

void pr_find_and_remove_tags(pr_battle *bt, pr_engine *e, pr_pokemon *p, const int32_t *types, int n_types)
{
    /* findAndRemoveTags(t => types.includes(t.tagType)): the matches in list order, then each
     * one's turnCount = 0, onRemove, splice */
    pr_battler_tag match[PR_MAX_BATTLER_TAGS];
    int n = 0;
    for (int i = 0; i < p->n_tags; i++) {
        int type = p->tags[i].type;
        bool listed = false;
        for (int k = 0; k < n_types && !listed; k++) listed = types[k] == type;
        if (!listed) continue;
        if (!pr_tag_type_ported(type)) {
            note_tag(bt, type);
            continue;
        }
        match[n++] = p->tags[i];
    }
    for (int k = 0; k < n; k++) {
        int i = find_tag(p, match[k].type);
        if (i < 0) continue;
        p->tags[i].turn_count = 0;
        pr_battler_tag t = p->tags[i];
        on_remove(bt, e, p, &t);
        splice_tag(p, i);
    }
}

void pr_remove_field_boost_tag(pr_battle *bt, pr_engine *e, pr_pokemon *p, bool terrain, int now)
{
    /* findAndRemoveTags(tag => "weatherTypes" in tag && !tag.weatherTypes.find(w => w === weather)),
     * and the terrain version: only Protosynthesis's tag lists weathers (Sunny, Harsh Sun), only
     * Quark Drive's lists terrains (Electric). turnCount = 0, onRemove (HighestStatBoostTag's
     * message), splice. */
    int type = terrain ? BATTLER_TAG_TYPE_QUARK_DRIVE : BATTLER_TAG_TYPE_PROTOSYNTHESIS;
    bool listed = terrain ? now == TERRAIN_ELECTRIC : (now == WEATHER_TYPE_SUNNY || now == WEATHER_TYPE_HARSH_SUN);
    int i = find_tag(p, type);
    if (i < 0 || listed) return;
    p->tags[i].turn_count = 0;
    pr_queue_message(bt, e);
    splice_tag(p, i);
}

void pr_remove_tags_by_source_id(pr_battle *bt, pr_engine *e, pr_pokemon *p, int64_t source_id)
{
    /* findAndRemoveTags(t => t.isSourceLinked() && t.sourceId === sourceId): the matches, then
     * each one's turnCount = 0, onRemove, splice */
    pr_battler_tag match[PR_MAX_BATTLER_TAGS];
    int n = 0;
    for (int i = 0; i < p->n_tags; i++) {
        if (p->tags[i].source_id != source_id) continue;
        int type = p->tags[i].type;
        if (!pr_tag_type_ported(type)) {
            note_tag(bt, type);                   /* isSourceLinked() and what follows aren't known */
            continue;
        }
        if (is_trap(type)) match[n++] = p->tags[i];
    }
    for (int k = 0; k < n; k++) {
        int i = find_tag(p, match[k].type);
        if (i < 0) continue;
        p->tags[i].turn_count = 0;
        pr_battler_tag t = p->tags[i];
        on_remove(bt, e, p, &t);
        splice_tag(p, i);
    }
}
