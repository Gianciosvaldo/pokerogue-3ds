/* Ability attribute dispatch: ports of applyAbAttrs (src/data/abilities/apply-ab-attrs.ts),
 * Pokemon.canApplyAbility, and the canApply/apply pairs of the AbAttr classes on the
 * damage path (src/data/abilities/ab-attrs.ts). A class with no case here is reported
 * unported, never skipped silently. */
#include "pr/engine.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

#define AB_FLAG_BYPASS_FAINT (1u << 0)
#define AB_FLAG_IGNORABLE (1u << 1)
#define AB_FLAG_UNSUPPRESSABLE (1u << 2)

#define P(T, a) ((const pr_attr_##T *)(a)->p)
/* Nullable closure field: PR_NO_FN when absent. Structs with a `nulls` mask always
 * store PR_NO_FN for a null fn field, so the id alone is enough. */
#define FN(T, a, f) (P(T, a)->f)

static double to_dmg_value(double v)
{
    v = floor(v);
    return v > 1 ? v : 1;
}

bool pr_is_opponent(const pr_battle *bt, const pr_pokemon *a, const pr_pokemon *b)
{
    (void)bt;
    return a->is_player != b->is_player;
}

const pr_pokemon *pr_get_ally(const pr_battle *bt, const pr_pokemon *p)
{
    if (!bt->double_battle) return NULL;
    for (int i = 0; i < 4; i++)
        if (bt->field[i] == p) return bt->field[i ^ 1];
    return NULL;
}

const pr_attr *pr_ability_first_attr(uint16_t ability, uint16_t cls)
{
    if (ability >= PR_ABILITY_COUNT) return NULL;
    const pr_ability *a = &pr_abilities[ability];
    for (unsigned i = 0; i < a->nattr; i++)
        if (pr_attr_is(pr_attrs[a->attr0 + i].cls, cls)) return &pr_attrs[a->attr0 + i];
    return NULL;
}

bool pr_move_is_attack(int move)
{
    /* Move.is("AttackMove"): AttackMove and ChargingAttackMove (the mixin keeps AttackMove.is) */
    const char *k = pr_moves[move].kind;
    return !strcmp(k, "AttackMove") || !strcmp(k, "ChargingAttackMove");
}

/* Move.getPriority(user) */
int pr_move_get_priority(pr_battle *bt, int move, const pr_pokemon *user)
{
    double prio = pr_moves[move].priority;
    /* applyMoveAttrs("IncrementMovePriorityAttr", user, null, move, priority): Grassy Glide */
    const pr_move *mv = &pr_moves[move];
    for (unsigned i = 0; i < mv->nattr; i++) {
        const pr_attr *a = &pr_attrs[mv->attr0 + i];
        if (!pr_attr_is(a->cls, PR_AC_IncrementMovePriorityAttr)) continue;
        const pr_attr_IncrementMovePriorityAttr *x = P(IncrementMovePriorityAttr, a);
        if (pr_closure_call(bt, x->moveIncrementFunc, a->cls, 0, user, NULL, move)) prio += x->increaseAmount;
    }
    pr_ab_params pp = {.pokemon = user, .move = move, .simulated = true, .value = &prio};
    pr_apply_ab_attrs(bt, PR_AC_ChangeMovePriorityAbAttr, &pp);
    return (int)prio;
}

bool pr_has_ability_active(pr_battle *bt, const pr_pokemon *p, int ability)
{
    if (p->ability == ability && pr_can_apply_ability(bt, p, p->ability)) return true;
    return p->passive && p->passive == ability && pr_can_apply_ability(bt, p, p->passive);
}

bool pr_has_ability_with_attr(pr_battle *bt, const pr_pokemon *p, uint16_t cls)
{
    if (pr_can_apply_ability(bt, p, p->ability) && pr_ability_first_attr(p->ability, cls)) return true;
    return p->passive && pr_can_apply_ability(bt, p, p->passive) && pr_ability_first_attr(p->passive, cls);
}

bool pr_move_hits_substitute(pr_battle *bt, int move, const pr_pokemon *user, const pr_pokemon *target)
{
    int tgt = pr_moves[move].target;
    if (tgt == MOVE_TARGET_USER || tgt == MOVE_TARGET_USER_SIDE || tgt == MOVE_TARGET_ENEMY_SIDE
        || tgt == MOVE_TARGET_BOTH_SIDES || !target || !pr_has_battler_tag(bt, target, BATTLER_TAG_TYPE_SUBSTITUTE))
        return false;
    bool bypassed = false;
    pr_ab_params pp = {.pokemon = user, .move = -1, .flag = &bypassed};
    pr_apply_ab_attrs(bt, PR_AC_InfiltratorAbAttr, &pp);
    unsigned fl = pr_moves[move].flags;
    return !bypassed && !(fl & MOVE_FLAGS_SOUND_BASED) && !(fl & MOVE_FLAGS_IGNORE_SUBSTITUTE);
}

/* Move.doesFlagEffectApply({flag, user, target, isFollowUp}) */
bool pr_move_does_flag_effect_apply_ex(pr_battle *bt, int move, unsigned flag, const pr_pokemon *user,
                                       const pr_pokemon *target, bool is_follow_up)
{
    switch (flag) {
    case MOVE_FLAGS_MAKES_CONTACT:
        if (pr_has_ability_with_attr(bt, user, PR_AC_IgnoreContactAbAttr) || pr_move_hits_substitute(bt, move, user, target))
            return false;
        break;
    case MOVE_FLAGS_IGNORE_ABILITIES:
        if (pr_has_ability_with_attr(bt, user, PR_AC_MoveAbilityBypassAbAttr)) {
            bool ignored = false;
            pr_ab_params pp = {.pokemon = user, .move = move, .flag = &ignored};
            pr_apply_ab_attrs(bt, PR_AC_MoveAbilityBypassAbAttr, &pp);
            if (ignored) return true;
        }
        return (pr_moves[move].flags & flag) && !is_follow_up;
    case MOVE_FLAGS_IGNORE_PROTECT:
        if (pr_has_ability_with_attr(bt, user, PR_AC_IgnoreProtectOnContactAbAttr)
            && pr_move_does_flag_effect_apply_ex(bt, move, MOVE_FLAGS_MAKES_CONTACT, user, NULL, false))
            return true;
        break;
    }
    return (pr_moves[move].flags & flag) != 0;
}

bool pr_move_does_flag_effect_apply(pr_battle *bt, int move, unsigned flag, const pr_pokemon *user,
                                    const pr_pokemon *target)
{
    return pr_move_does_flag_effect_apply_ex(bt, move, flag, user, target, false);
}

/* Weather.isEffectSuppressed(): reads the first SuppressWeatherEffectAbAttr straight off
 * each active Pokemon's ability (or passive), with no canApplyAbility check. */
bool pr_weather_effect_suppressed(pr_battle *bt)
{
    if (bt->weather == WEATHER_TYPE_NONE) return false;
    bool immutable = bt->weather == WEATHER_TYPE_HEAVY_RAIN || bt->weather == WEATHER_TYPE_HARSH_SUN
                     || bt->weather == WEATHER_TYPE_STRONG_WINDS;
    for (int i = 0; i < 4; i++) {
        const pr_pokemon *p = bt->field[i];
        if (!p || p->hp <= 0) continue;
        const pr_attr *a = pr_ability_first_attr(p->ability, PR_AC_SuppressWeatherEffectAbAttr);
        if (!a && p->passive) a = pr_ability_first_attr(p->passive, PR_AC_SuppressWeatherEffectAbAttr);
        if (a && (!immutable || P(SuppressWeatherEffectAbAttr, a)->affectsImmutable)) return true;
    }
    return false;
}

/* Pokemon.canApplyAbility(passive). Fusion, Transform, Mold Breaker (arena.ignoreAbilities) and
 * summonData.abilitySuppressed are not modelled; none can occur in the current state. */
bool pr_can_apply_ability(pr_battle *bt, const pr_pokemon *p, uint16_t ability)
{
    const pr_ability *ab = &pr_abilities[ability];
    /* Mold Breaker, Sunsteel Strike...: the move in progress ignores ignorable abilities
     * of every Pokemon but its user */
    if (bt->ignore_abilities && bt->ignoring_effect_source != pr_battler_index(bt, p) && (ab->flags & AB_FLAG_IGNORABLE))
        return false;
    /* Gastro Acid and co. */
    if (p->ability_suppressed && !(ab->flags & AB_FLAG_UNSUPPRESSABLE)) return false;
    /* Neutralizing Gas: arena.getTag(NEUTRALIZING_GAS) suppresses the abilities of Pokemon on the field
     * (and PreSummonAbAttr abilities off it), except unsuppressable ones and the gas user's own: its
     * other ability too while it is the only source (sourceCount 1). */
    const pr_arena_tag *gas = pr_arena_get_tag_on_side(bt, ARENA_TAG_TYPE_NEUTRALIZING_GAS, ARENA_TAG_SIDE_BOTH);
    if (gas && !gas->being_removed
        && (pr_battler_index(bt, p) >= 0 || pr_ability_first_attr(ability, PR_AC_PreSummonAbAttr))) {
        uint16_t src_attr = PR_AC_PreLeaveFieldRemoveSuppressAbilitiesSourceAbAttr;
        bool this_suppressing = pr_ability_first_attr(ability, src_attr) != NULL;
        /* hasAbilityWithAttr(..., canApply = false) */
        bool has_suppressing = pr_ability_first_attr(p->ability, src_attr) || (p->passive && pr_ability_first_attr(p->passive, src_attr));
        bool unsuppressable = (ab->flags & AB_FLAG_UNSUPPRESSABLE) || this_suppressing
                              || (has_suppressing && !(gas->source_count > 1));
        if (!unsuppressable) return false;
    }
    if (!(p->hp > 0 || (ab->flags & AB_FLAG_BYPASS_FAINT))) return false;
    for (unsigned i = 0; i < ab->conditions.n; i++)
        if (!pr_closure_call(bt, ab->conditions.v[i], PR_AC_AbAttr, 1, p, NULL, -1)) return false;
    return true;
}

static bool call_bool(pr_battle *bt, pr_fn_id fn, uint16_t cls, const pr_pokemon *a, const pr_pokemon *b, int move)
{
    return pr_closure_call(bt, fn, cls, 0, a, b, move) != 0;
}

/* attr.getCondition(): extraCondition, except for classes that override it. */
static bool condition_ok(pr_battle *bt, const pr_attr *a, const pr_pokemon *p)
{
    switch (a->cls) {
    case PR_AC_LowHpMoveTypePowerBoostAbAttr:
        return pr_get_hp_ratio(p, false) <= 0.33;
    case PR_AC_TypeImmunityAbAttr:
    case PR_AC_TypeImmunityHealAbAttr:
    case PR_AC_TypeImmunityStatStageChangeAbAttr:
    case PR_AC_TypeImmunityAddBattlerTagAbAttr:
    case PR_AC_NonSuperEffectiveImmunityAbAttr:
        /* getCondition() returns this.condition, which no instance sets */
        return true;
    }
    if (a->cond == PR_NO_FN) return true;
    return call_bool(bt, a->cond, a->cls, p, NULL, -1);
}

static bool type_in(const pr_intlist *l, int t)
{
    for (unsigned i = 0; i < l->n; i++)
        if (l->v[i] == t) return true;
    return false;
}

/* What applySingleAbAttrs does between canApply and apply outside a simulation: show the
 * ability, queue its trigger message. The turn engine keeps these phases (NULL: no engine). */
typedef struct { pr_engine *e; bool shown; uint16_t ability; } ab_ctx;
/* findModifiers(PokemonHeldItemModifier held by p): the item stealers stop at a non-empty list */
static bool has_held_items(const pr_battle *bt, const pr_pokemon *p)
{
    int side = p->is_player ? 0 : 1;
    for (int i = 0; i < bt->n_mods[side]; i++)
        if (pr_mod_is(&bt->mods[side][i], PR_MOD_HELD_ITEM) && bt->mods[side][i].pokemon_id == (int64_t)p->id) return true;
    return false;
}
static bool has_trigger_message(uint16_t cls);
static bool shows_ability(const pr_attr *a);
static void before_apply(pr_battle *bt, const pr_attr *a, const pr_ab_params *pp, ab_ctx *c)
{
    if (!c->e) return;
    c->shown = shows_ability(a);
    if (c->shown) pr_queue_ability_display(bt, c->e, pp->pokemon, true);
    if (has_trigger_message(a->cls)) pr_queue_message(bt, c->e);
}
/* canApply(params) false: skip this attr; true: the ability applies, apply follows */
#define GATE(can) do { if (!(can)) return 0; before_apply(bt, a, pp, c); } while (0)

/* canApply, then apply, for one attr: 1 applied, 0 canApply was false, -1 no C port. */
static int apply_one(pr_battle *bt, const pr_attr *a, pr_ab_params *pp, ab_ctx *c)
{
    const pr_pokemon *p = pp->pokemon, *o = pp->opponent;
    int move = pp->move;
    switch (a->cls) {
    /* ---- stats ---- */
    case PR_AC_StatMultiplierAbAttr: {
        const pr_attr_StatMultiplierAbAttr *x = P(StatMultiplierAbAttr, a);
        GATE(pp->stat == x->stat
             && (x->condition == PR_NO_FN || call_bool(bt, x->condition, a->cls, p, NULL, move < 0 ? 0 : move)));
        *pp->value *= x->multiplier;
        return 1;
    }
    case PR_AC_AllyStatMultiplierAbAttr: {
        const pr_attr_AllyStatMultiplierAbAttr *x = P(AllyStatMultiplierAbAttr, a);
        GATE(pp->stat == x->stat && !(pp->ignore_ability && x->ignorable));
        *pp->value *= x->multiplier;
        return 1;
    }
    case PR_AC_FieldMultiplyStatAbAttr: {
        const pr_attr_FieldMultiplyStatAbAttr *x = P(FieldMultiplyStatAbAttr, a);
        bool ok = x->canStack;
        if (!ok && !*pp->flag && x->stat == pp->stat) {
            /* target.getAbilityAttrs("FieldMultiplyStatAbAttr").every(attr => attr.stat !== stat) */
            ok = true;
            uint16_t abs[2] = {o->ability, o->passive};
            for (int k = 0; k < 2; k++) {
                if (k == 1 && !o->passive) continue;
                if (!pr_can_apply_ability(bt, o, abs[k])) continue;
                const pr_ability *ab = &pr_abilities[abs[k]];
                for (unsigned i = 0; i < ab->nattr; i++) {
                    const pr_attr *t = &pr_attrs[ab->attr0 + i];
                    if (t->cls == PR_AC_FieldMultiplyStatAbAttr && P(FieldMultiplyStatAbAttr, t)->stat == pp->stat) ok = false;
                }
            }
        }
        GATE(ok);
        *pp->value *= x->multiplier;
        *pp->flag = true;
        return 1;
    }
    case PR_AC_IgnoreOpponentStatStagesAbAttr:
        GATE(type_in(&P(IgnoreOpponentStatStagesAbAttr, a)->stats, pp->stat));
        *pp->flag = true;
        return 1;

    /* ---- move type and power ---- */
    case PR_AC_MoveTypeChangeAbAttr: {
        const pr_attr_MoveTypeChangeAbAttr *x = P(MoveTypeChangeAbAttr, a);
        GATE(call_bool(bt, x->condition, a->cls, p, o, move));
        *pp->holder = x->newType;
        return 1;
    }
    case PR_AC_MovePowerBoostAbAttr:
    case PR_AC_MoveTypePowerBoostAbAttr:
    case PR_AC_LowHpMoveTypePowerBoostAbAttr: {
        /* skipDuringMovesetGen only matters while the AI generates movesets */
        const pr_attr_MovePowerBoostAbAttr *x = P(MovePowerBoostAbAttr, a);
        pr_fn_id cond; double mult;
        if (a->cls == PR_AC_MovePowerBoostAbAttr) { cond = x->condition; mult = x->powerMultiplier; }
        else if (a->cls == PR_AC_MoveTypePowerBoostAbAttr) {
            cond = P(MoveTypePowerBoostAbAttr, a)->condition; mult = P(MoveTypePowerBoostAbAttr, a)->powerMultiplier;
        } else {
            cond = P(LowHpMoveTypePowerBoostAbAttr, a)->condition; mult = P(LowHpMoveTypePowerBoostAbAttr, a)->powerMultiplier;
        }
        GATE(call_bool(bt, cond, a->cls, p, o, move));
        *pp->value *= mult;
        return 1;
    }
    case PR_AC_AllyMoveCategoryPowerBoostAbAttr: {
        /* FieldMovePowerBoostAbAttr: canApply is always true, apply checks the condition */
        const pr_attr_AllyMoveCategoryPowerBoostAbAttr *x = P(AllyMoveCategoryPowerBoostAbAttr, a);
        GATE(true);
        if (call_bool(bt, x->condition, a->cls, p, o, move)) *pp->value *= x->powerMultiplier;
        return 1;
    }
    case PR_AC_UserFieldMoveTypePowerBoostAbAttr: {
        const pr_attr_UserFieldMoveTypePowerBoostAbAttr *x = P(UserFieldMoveTypePowerBoostAbAttr, a);
        GATE(true);
        if (call_bool(bt, x->condition, a->cls, p, o, move)) *pp->value *= x->powerMultiplier;
        return 1;
    }
    case PR_AC_PreAttackWeatherOverrideAbAttr: {
        int w = P(PreAttackWeatherOverrideAbAttr, a)->weatherType;
        GATE(*pp->holder != w);
        *pp->holder = w;
        return 1;
    }
    case PR_AC_WeightMultiplierAbAttr:
        GATE(true);
        *pp->value *= P(WeightMultiplierAbAttr, a)->multiplier;
        return 1;
    case PR_AC_UngroundedAbAttr:
        GATE(true);
        *pp->flag = true;
        return 1;

    /* ---- effectiveness and immunities ---- */
    case PR_AC_IgnoreTypeImmunityAbAttr: {
        const pr_attr_IgnoreTypeImmunityAbAttr *x = P(IgnoreTypeImmunityAbAttr, a);
        GATE(!*pp->flag && x->defenderType == pp->defender_type && type_in(&x->allowedMoveTypes, pp->move_type));
        *pp->flag = true;
        return 1;
    }
    case PR_AC_TypeImmunityHealAbAttr:
    case PR_AC_TypeImmunityStatStageChangeAbAttr:
    case PR_AC_TypeImmunityAddBattlerTagAbAttr: {
        int immune;
        switch (a->cls) {
        case PR_AC_TypeImmunityHealAbAttr: immune = P(TypeImmunityHealAbAttr, a)->immuneType; break;
        case PR_AC_TypeImmunityStatStageChangeAbAttr: immune = P(TypeImmunityStatStageChangeAbAttr, a)->immuneType; break;
        default: immune = P(TypeImmunityAddBattlerTagAbAttr, a)->immuneType; break;
        }
        int tgt = pr_moves[move].target;
        GATE(tgt != MOVE_TARGET_BOTH_SIDES && tgt != MOVE_TARGET_ENEMY_SIDE && tgt != MOVE_TARGET_USER_SIDE
             && o != p && pr_get_move_type(bt, o, move) == immune);
        *pp->value = 0;
        if (a->cls == PR_AC_TypeImmunityHealAbAttr) {
            if (p->hp < pr_get_stat(p, STAT_HP, true) && !pp->simulated) {
                /* unshiftNew("PokemonHealPhase", toDmgValue(maxHp / 4), {message}); the damage-only
                 * test has no engine and records it as deferred */
                if (c->e) pr_unshift_heal(bt, c->e, pr_battler_index(bt, p), to_dmg_value(pr_get_stat(p, STAT_HP, true) / 4.0), PR_HEAL_MESSAGE);
                else pr_note_deferred(bt, a->cls);
                *pp->flag = true;
            }
        } else {
            *pp->flag = true;
            if (!pp->simulated) pr_note_deferred(bt, a->cls); /* StatStageChangePhase / addTag */
        }
        return 1;
    }
    case PR_AC_NonSuperEffectiveImmunityAbAttr:
        GATE(pr_move_is_attack(move) && *pp->value < 2);
        *pp->flag = true;
        *pp->value = 0;
        return 1;
    case PR_AC_MoveImmunityAbAttr:
    case PR_AC_MoveImmunityStatStageChangeAbAttr: {
        pr_fn_id cond = a->cls == PR_AC_MoveImmunityAbAttr ? P(MoveImmunityAbAttr, a)->immuneCondition
                                                           : P(MoveImmunityStatStageChangeAbAttr, a)->immuneCondition;
        GATE(!(a->cls == PR_AC_MoveImmunityStatStageChangeAbAttr && pp->simulated) && !*pp->flag
             && call_bool(bt, cond, a->cls, p, o, move));
        *pp->flag = true;
        if (a->cls == PR_AC_MoveImmunityStatStageChangeAbAttr) pr_note_deferred(bt, a->cls);
        return 1;
    }
    case PR_AC_ChangeMovePriorityAbAttr: {
        /* Prankster, Gale Wings, Triage: moveFunc(pokemon, move) */
        const pr_attr_ChangeMovePriorityAbAttr *x = P(ChangeMovePriorityAbAttr, a);
        GATE(call_bool(bt, x->moveFunc, a->cls, p, NULL, move));
        *pp->value += x->changeAmount;
        return 1;
    }
    case PR_AC_FieldPriorityMoveImmunityAbAttr: {
        int tgt = pr_moves[move].target;
        bool ally_target = tgt == MOVE_TARGET_USER || tgt == MOVE_TARGET_NEAR_ALLY || tgt == MOVE_TARGET_ALLY
                           || tgt == MOVE_TARGET_USER_OR_NEAR_ALLY || tgt == MOVE_TARGET_USER_AND_ALLIES
                           || tgt == MOVE_TARGET_USER_SIDE;
        GATE(!*pp->flag && pr_move_get_priority(bt, move, o) > 0 && !ally_target && pr_is_opponent(bt, o, p));
        *pp->flag = true;
        return 1;
    }
    case PR_AC_FullHpResistTypeAbAttr:
        GATE(!pr_move_has_attr((uint16_t)move, PR_AC_FixedDamageAttr) && p->hp >= pr_get_stat(p, STAT_HP, true)
             && *pp->value > 0.5);
        *pp->value = 0.5;
        ((pr_pokemon *)p)->turn_move_effectiveness = 0.5; /* pokemon.turnData.moveEffectiveness */
        return 1;

    /* ---- damage ---- */
    case PR_AC_StabBoostAbAttr:
        GATE(*pp->value > 1);
        *pp->value += 0.5;
        return 1;
    case PR_AC_MultCritAbAttr:
        GATE(*pp->value > 1);
        *pp->value *= P(MultCritAbAttr, a)->multAmount;
        return 1;
    /* ---- status effects ---- */
    case PR_AC_StatusEffectImmunityAbAttr: {                            /* Limber, Insomnia, Water Veil... */
        const pr_attr_StatusEffectImmunityAbAttr *x = P(StatusEffectImmunityAbAttr, a);
        GATE(!*pp->flag && ((x->immuneEffects.n == 0 && pp->effect != STATUS_EFFECT_FAINT)
                            || type_in(&x->immuneEffects, pp->effect)));
        *pp->flag = true;
        return 1;
    }
    case PR_AC_UserFieldStatusEffectImmunityAbAttr: {                   /* Sweet Veil, Pastel Veil */
        const pr_attr_UserFieldStatusEffectImmunityAbAttr *x = P(UserFieldStatusEffectImmunityAbAttr, a);
        GATE((!*pp->flag && x->immuneEffects.n == 0 && pp->effect != STATUS_EFFECT_FAINT)
             || type_in(&x->immuneEffects, pp->effect));
        *pp->flag = true;
        return 1;
    }
    case PR_AC_ConditionalUserFieldStatusEffectImmunityAbAttr: {        /* Flower Veil */
        const pr_attr_ConditionalUserFieldStatusEffectImmunityAbAttr *x = P(ConditionalUserFieldStatusEffectImmunityAbAttr, a);
        GATE(!*pp->flag && call_bool(bt, x->condition, a->cls, pp->target, o, -1)
             && ((!*pp->flag && x->immuneEffects.n == 0 && pp->effect != STATUS_EFFECT_FAINT)
                 || type_in(&x->immuneEffects, pp->effect)));
        *pp->flag = true;
        return 1;
    }
    case PR_AC_IgnoreTypeStatusEffectImmunityAbAttr: {                  /* Corrosion */
        const pr_attr_IgnoreTypeStatusEffectImmunityAbAttr *x = P(IgnoreTypeStatusEffectImmunityAbAttr, a);
        GATE(!*pp->flag && type_in(&x->statusEffect, pp->effect) && type_in(&x->defenderType, pp->defender_type));
        *pp->flag = true;
        return 1;
    }
    case PR_AC_SynchronizeStatusAbAttr: {
        if (!pp->simulated && !c->e) return -1;
        int ef = pp->effect;
        GATE(o && (ef == STATUS_EFFECT_BURN || ef == STATUS_EFFECT_PARALYSIS || ef == STATUS_EFFECT_POISON
                   || ef == STATUS_EFFECT_TOXIC));
        if (!pp->simulated) pr_try_set_status(bt, c->e, (pr_pokemon *)o, ef, (pr_pokemon *)p, -1, false, true);
        return 1;
    }
    case PR_AC_PostSummonHealStatusAbAttr: {                            /* Limber, Insomnia, Water Veil... */
        const pr_attr_PostSummonHealStatusAbAttr *x = P(PostSummonHealStatusAbAttr, a);
        if (!(p->status && (x->immuneEffects.n == 0 || type_in(&x->immuneEffects, p->status)))) return 0;
        if (!c->e) return -1;                         /* apply ignores simulated: it needs the queue */
        GATE(true);
        /* getTriggerMessage runs before apply: it names the status this attr cured last time */
        if (pr_status_healed_get(c->ability)) pr_queue_message(bt, c->e);
        pr_status_healed_set(c->ability, p->status);
        pr_reset_status_phase(bt, c->e, (pr_pokemon *)p, false);
        return 1;
    }
    case PR_AC_PostSummonRemoveBattlerTagAbAttr: {                      /* Oblivious, Own Tempo */
        const pr_attr_PostSummonRemoveBattlerTagAbAttr *x = P(PostSummonRemoveBattlerTagAbAttr, a);
        for (unsigned i = 0; i < x->immuneTags.n; i++)
            if (pr_get_battler_tag(p, x->immuneTags.v[i])) return -1;       /* removeTag: onRemove */
        return 0;
    }
    case PR_AC_PostTurnStatusHealAbAttr: {                              /* Poison Heal */
        const pr_attr_PostTurnStatusHealAbAttr *x = P(PostTurnStatusHealAbAttr, a);
        if (!(p->status && type_in(&x->effects, p->status) && p->hp < pr_get_stat(p, STAT_HP, true))) return 0;
        if (!c->e) return -1;
        GATE(true);
        pr_unshift_heal(bt, c->e, pr_battler_index(bt, p), to_dmg_value(pr_get_stat(p, STAT_HP, true) / 8.0), PR_HEAL_MESSAGE);
        return 1;
    }
    /* ---- on-hit abilities: PostDefend (the Pokemon hit), PostReceiveCrit, PostAttack (the attacker).
     * The ones whose effect adds a tag, weather, terrain, an ability or an item run their canApply
     * (random rolls included, so the RNG stays in step) and report themselves when it holds. */
    case PR_AC_PostDefendStatStageChangeAbAttr: {                       /* Justified, Weak Armor, Gooey... */
        const pr_attr_PostDefendStatStageChangeAbAttr *x = P(PostDefendStatStageChangeAbAttr, a);
        if (!c->e) return -1;
        GATE(call_bool(bt, x->condition, a->cls, p, o, move));
        int8_t st = (int8_t)x->stat, sg = (int8_t)x->stages;
        if (x->allOthers) {
            /* getOpponents() (active on the field, field order), then getAlly() */
            for (int k = 0; k < 4; k++) {
                pr_pokemon *other = bt->field[k];
                if (other && other->is_player != p->is_player && other->hp > 0 && other->on_field && !other->switch_out_status)
                    pr_unshift_stat_stage_change_ex(bt, c->e, k, &st, &sg, 1, (pr_pokemon *)p, 0);
            }
            const pr_pokemon *ally = pr_get_ally(bt, p);
            if (ally) pr_unshift_stat_stage_change_ex(bt, c->e, pr_battler_index(bt, ally), &st, &sg, 1, (pr_pokemon *)p, 0);
        } else {
            pr_unshift_stat_stage_change_ex(bt, c->e, pr_battler_index(bt, x->selfTarget ? p : o), &st, &sg, 1,
                                            (pr_pokemon *)p, 0);
        }
        return 1;
    }
    case PR_AC_PostDefendHpGatedStatStageChangeAbAttr: {                /* Berserk, Anger Shell */
        const pr_attr_PostDefendHpGatedStatStageChangeAbAttr *x = P(PostDefendHpGatedStatStageChangeAbAttr, a);
        if (!c->e) return -1;
        double threshold = to_dmg_value(pr_get_stat(p, STAT_HP, true) * x->hpGate);
        GATE(pr_moves[move].category != MOVE_CATEGORY_STATUS && p->hp <= threshold && p->hp + pp->damage > threshold);
        int8_t st[7], sg[7];
        int n = 0;
        for (unsigned i = 0; i < x->changes.n && n < 7; i++, n++) { st[n] = x->changes.v[i].stat; sg[n] = x->changes.v[i].stages; }
        pr_unshift_stat_stage_change_ex(bt, c->e, pr_battler_index(bt, x->selfTarget ? p : o), st, sg, n, (pr_pokemon *)p, 0);
        return 1;
    }
    case PR_AC_FlinchStatStageChangeAbAttr: {                           /* Steadfast */
        const pr_attr_FlinchStatStageChangeAbAttr *x = P(FlinchStatStageChangeAbAttr, a);
        if (!c->e) return -1;
        GATE(true);
        int8_t st[7], sg[7];
        int n = 0;
        for (unsigned i = 0; i < x->changes.n && n < 7; i++, n++) { st[n] = x->changes.v[i].stat; sg[n] = x->changes.v[i].stages; }
        pr_unshift_stat_stage_change_ex(bt, c->e, pr_battler_index(bt, p), st, sg, n, (pr_pokemon *)p, 0);
        return 1;
    }
    case PR_AC_PostReceiveCritStatStageChangeAbAttr: {                  /* Anger Point */
        const pr_attr_PostReceiveCritStatStageChangeAbAttr *x = P(PostReceiveCritStatStageChangeAbAttr, a);
        if (!c->e) return -1;
        GATE(true);
        int8_t st = (int8_t)x->stat, sg = (int8_t)x->stages;
        pr_unshift_stat_stage_change_ex(bt, c->e, pr_battler_index(bt, p), &st, &sg, 1, (pr_pokemon *)p, 0);
        return 1;
    }
    case PR_AC_EffectSporeAbAttr:
    case PR_AC_PostDefendApplyStatusEffectAbAttr: {                     /* Static, Flame Body, Poison Point */
        const pr_attr_PostDefendApplyStatusEffectAbAttr *x = P(PostDefendApplyStatusEffectAbAttr, a);
        if (!c->e) return -1;
        /* Effect Spore: not against Grass types or Overcoat, before anything is rolled */
        if (a->cls == PR_AC_EffectSporeAbAttr
            && (pr_is_of_type(o, POKEMON_TYPE_GRASS) || pr_has_ability_active(bt, o, ABILITY_ID_OVERCOAT)))
            return 0;
        /* canApply picks the effect first (a roll when there are several), then contact, the
         * attacker's status, the chance roll and canSetStatus */
        int ef = x->effects.n == 1 ? x->effects.v[0] : x->effects.v[pr_battle_rand_seed_int(bt, x->effects.n, 0)];
        bool can = (x->contactRequired ? pr_move_does_flag_effect_apply(bt, move, MOVE_FLAGS_MAKES_CONTACT, o, p)
                                       : pr_moves[move].category != MOVE_CATEGORY_STATUS)
                   && !o->status && (x->chance == -1 || pr_battle_rand_seed_int(bt, 100, 0) < x->chance)
                   && pr_can_set_status(bt, c->e, (pr_pokemon *)o, ef, true, false, p, false);
        GATE(can);
        /* apply picks again, independently of the effect canApply checked */
        ef = x->effects.n == 1 ? x->effects.v[0] : x->effects.v[pr_battle_rand_seed_int(bt, x->effects.n, 0)];
        pr_try_set_status(bt, c->e, (pr_pokemon *)o, ef, (pr_pokemon *)p, -1, false, true);
        return 1;
    }
    case PR_AC_PostDefendContactDamageAbAttr: {                         /* Rough Skin, Iron Barbs */
        const pr_attr_PostDefendContactDamageAbAttr *x = P(PostDefendContactDamageAbAttr, a);
        if (!c->e) return -1;
        GATE(!pp->simulated && pr_move_does_flag_effect_apply(bt, move, MOVE_FLAGS_MAKES_CONTACT, o, p)
             && !pr_has_ability_with_attr(bt, o, PR_AC_BlockNonDirectDamageAbAttr));
        pr_pokemon *w = (pr_pokemon *)o;
        pr_damage_and_update(bt, c->e, w, to_dmg_value(pr_get_stat(w, STAT_HP, true) * (1.0 / x->damageRatio)),
                             HIT_RESULT_INDIRECT, false, NULL);
        w->damage_taken += (int32_t)to_dmg_value(pr_get_stat(w, STAT_HP, true) * (1.0 / x->damageRatio));
        return 1;
    }
    case PR_AC_PostDefendTypeChangeAbAttr: {                            /* Color Change */
        if (pp->hit_result >= HIT_RESULT_NO_EFFECT || p->terastallized
            || pr_move_has_attr((uint16_t)move, PR_AC_TypelessAttr) || o->hits_left > 1)
            return 0;
        int type = pr_get_move_type(bt, o, move);
        int8_t ty[3];
        int n = pr_get_types(p, ty, PR_TYPES_ORIGINAL_IF_STELLAR);
        for (int i = 0; i < n; i++)
            if (ty[i] == type) return 0;
        GATE(true);
        if (!pp->simulated) {
            pr_pokemon *w = (pr_pokemon *)p;
            w->type_override[0] = (int8_t)type;                  /* summonData.types = [type] */
            w->type_override[1] = -1;
        }
        return 1;
    }
    case PR_AC_PostDefendMoveDisableAbAttr: {                           /* Cursed Body */
        const pr_attr_PostDefendMoveDisableAbAttr *x = P(PostDefendMoveDisableAbAttr, a);
        if (!c->e) return -1;
        if (pr_get_battler_tag(o, BATTLER_TAG_TYPE_DISABLED)
            || !pr_move_does_flag_effect_apply(bt, move, MOVE_FLAGS_MAKES_CONTACT, o, p)
            || !(x->chance == -1 || pr_battle_rand_seed_int(bt, 100, 0) < x->chance))
            return 0;
        return -1;                                              /* addTag(DISABLED) */
    }
    case PR_AC_PostDefendContactApplyTagChanceAbAttr: {                 /* Cute Charm */
        const pr_attr_PostDefendContactApplyTagChanceAbAttr *x = P(PostDefendContactApplyTagChanceAbAttr, a);
        if (!c->e) return -1;
        if (!pr_move_does_flag_effect_apply(bt, move, MOVE_FLAGS_MAKES_CONTACT, o, p)
            || !(pr_battle_rand_seed_int(bt, 100, 0) < x->chance))
            return 0;
        return -1;                                              /* canAddTag, addTag */
    }
    case PR_AC_PostDefendPerishSongAbAttr:                              /* Perish Body */
        if (!pr_move_does_flag_effect_apply(bt, move, MOVE_FLAGS_MAKES_CONTACT, o, p)
            || pr_get_battler_tag(o, BATTLER_TAG_TYPE_PERISH_SONG))
            return 0;
        return -1;                                              /* addTag(PERISH_SONG) on both */
    case PR_AC_PostDefendAbilitySwapAbAttr:                             /* Wandering Spirit */
    case PR_AC_PostDefendAbilityGiveAbAttr:                             /* Mummy, Lingering Aroma */
        /* contact, then the attacker's ability flags: setTempAbility isn't ported */
        return pr_move_does_flag_effect_apply(bt, move, MOVE_FLAGS_MAKES_CONTACT, o, p) ? -1 : 0;
    case PR_AC_PostDefendWeatherChangeAbAttr:                           /* Sand Spit */
        return call_bool(bt, P(PostDefendWeatherChangeAbAttr, a)->condition, a->cls, p, o, move) ? -1 : 0;
    case PR_AC_PostDefendApplyArenaTrapTagAbAttr:                       /* Toxic Debris */
        return call_bool(bt, P(PostDefendApplyArenaTrapTagAbAttr, a)->condition, a->cls, p, o, move) ? -1 : 0;
    case PR_AC_PostDefendApplyBattlerTagAbAttr:                         /* Wind Power, Electromorphosis */
        return call_bool(bt, P(PostDefendApplyBattlerTagAbAttr, a)->condition, a->cls, p, o, move) ? -1 : 0;
    case PR_AC_PostDefendTerrainChangeAbAttr:                           /* Seed Sower */
        return pp->hit_result < HIT_RESULT_NO_EFFECT ? -1 : 0;
    case PR_AC_ReverseDrainAbAttr: {                                    /* Liquid Ooze */
        /* canApply applies the attacker's BlockNonDirectDamageAbAttr (Magic Guard) as it is simulated */
        bool cancelled = false;
        pr_ab_params bp = {.pokemon = o, .move = -1, .flag = &cancelled, .simulated = pp->simulated};
        pr_apply_ab_attrs(bt, PR_AC_BlockNonDirectDamageAbAttr, &bp);
        return !cancelled && pr_move_has_attr((uint16_t)move, PR_AC_HitHealAttr) ? -1 : 0;
    }
    case PR_AC_PostDefendStealHeldItemAbAttr:                           /* Pickpocket */
        if (pp->simulated || pp->hit_result >= HIT_RESULT_NO_EFFECT
            || !call_bool(bt, P(PostDefendStealHeldItemAbAttr, a)->condition, a->cls, p, o, move))
            return 0;
        return has_held_items(bt, o) ? -1 : 0;                  /* the roll for the item, the transfer */
    case PR_AC_PostAttackApplyStatusEffectAbAttr:
    case PR_AC_PostAttackContactApplyStatusEffectAbAttr: {              /* Poison Touch, Toxic Chain */
        const pr_attr_PostAttackApplyStatusEffectAbAttr *x = P(PostAttackApplyStatusEffectAbAttr, a);
        if (!c->e) return -1;
        if (!call_bool(bt, x->attackCondition, a->cls, p, o, move)) return 0;
        if (!pp->simulated
            && !(!pr_has_ability_with_attr(bt, o, PR_AC_IgnoreMoveEffectsAbAttr) && p != o
                 && (!x->contactRequired || pr_move_does_flag_effect_apply(bt, move, MOVE_FLAGS_MAKES_CONTACT, p, o))
                 && pr_battle_rand_seed_int(bt, 100, 0) < x->chance && !p->status))   /* the attacker's own status */
            return 0;
        int ef = x->effects.n == 1 ? x->effects.v[0] : x->effects.v[pr_battle_rand_seed_int(bt, x->effects.n, 0)];
        GATE(pp->simulated || pr_can_set_status(bt, c->e, (pr_pokemon *)o, ef, true, false, p, false));
        ef = x->effects.n == 1 ? x->effects.v[0] : x->effects.v[pr_battle_rand_seed_int(bt, x->effects.n, 0)];
        pr_try_set_status(bt, c->e, (pr_pokemon *)o, ef, (pr_pokemon *)p, -1, false, true);
        return 1;
    }
    case PR_AC_PostAttackApplyBattlerTagAbAttr: {                       /* Stench */
        const pr_attr_PostAttackApplyBattlerTagAbAttr *x = P(PostAttackApplyBattlerTagAbAttr, a);
        if (!c->e) return -1;
        /* the contact check and the chance take (user = the one hit, target = the attacker) */
        if (!call_bool(bt, x->attackCondition, a->cls, p, o, move) || pr_has_ability_with_attr(bt, o, PR_AC_IgnoreMoveEffectsAbAttr)
            || p == o || (x->contactRequired && !pr_move_does_flag_effect_apply(bt, move, MOVE_FLAGS_MAKES_CONTACT, o, p)))
            return 0;
        int roll = pr_battle_rand_seed_int(bt, 100, 0);
        if (!(roll < pr_closure_call(bt, x->chance, a->cls, 0, o, p, move)) || p->status) return 0;
        return -1;                                              /* addTag(FLINCHED) */
    }
    case PR_AC_PostAttackStealHeldItemAbAttr:                           /* Magician */
        if (!call_bool(bt, P(PostAttackStealHeldItemAbAttr, a)->attackCondition, a->cls, p, o, move) || pp->simulated
            || pp->hit_result >= HIT_RESULT_NO_EFFECT)
            return 0;
        return has_held_items(bt, o) ? -1 : 0;
    /* ---- battler tag immunities (addTag): BaseBattlerTagImmunityAbAttr's canApply */
    case PR_AC_BattlerTagImmunityAbAttr: {                              /* Oblivious, Own Tempo, Inner Focus... */
        const pr_attr_BattlerTagImmunityAbAttr *x = P(BattlerTagImmunityAbAttr, a);
        GATE(!*pp->flag && type_in(&x->immuneTagTypes, pp->tag_type));
        *pp->flag = true;
        return 1;
    }
    case PR_AC_UserFieldBattlerTagImmunityAbAttr: {                     /* Sweet Veil, Aroma Veil */
        const pr_attr_UserFieldBattlerTagImmunityAbAttr *x = P(UserFieldBattlerTagImmunityAbAttr, a);
        GATE(!*pp->flag && type_in(&x->immuneTagTypes, pp->tag_type));
        *pp->flag = true;
        return 1;
    }
    case PR_AC_ConditionalUserFieldBattlerTagImmunityAbAttr: {          /* Flower Veil */
        const pr_attr_ConditionalUserFieldBattlerTagImmunityAbAttr *x = P(ConditionalUserFieldBattlerTagImmunityAbAttr, a);
        GATE(!*pp->flag && type_in(&x->immuneTagTypes, pp->tag_type) && call_bool(bt, x->condition, a->cls, pp->target, o, -1));
        *pp->flag = true;
        return 1;
    }
    case PR_AC_ReduceStatusEffectDurationAbAttr:                        /* Early Bird */
        GATE(pp->effect == P(ReduceStatusEffectDurationAbAttr, a)->statusEffect);
        *pp->value -= 1;
        return 1;
    case PR_AC_BlockStatusDamageAbAttr: {                               /* Poison Heal */
        const pr_attr_BlockStatusDamageAbAttr *x = P(BlockStatusDamageAbAttr, a);
        GATE(!*pp->flag && p->status && type_in(&x->effects, p->status));
        *pp->flag = true;
        return 1;
    }
    case PR_AC_ReduceBurnDamageAbAttr:                                  /* Heatproof */
        GATE(true);
        *pp->value = to_dmg_value(*pp->value * P(ReduceBurnDamageAbAttr, a)->multiplier);
        return 1;
    case PR_AC_BlockNonDirectDamageAbAttr:     /* Magic Guard */
    case PR_AC_BlockRecoilDamageAttr:          /* Rock Head */
    case PR_AC_BypassBurnDamageReductionAbAttr:
    case PR_AC_BlockOneHitKOAbAttr:            /* Sturdy against OHKO moves */
    case PR_AC_FieldPreventExplosiveMovesAbAttr: /* Damp */
        GATE(!*pp->flag); /* CancelInteractionAbAttr */
        *pp->flag = true;
        return 1;
    case PR_AC_MoveDamageBoostAbAttr: {
        const pr_attr_MoveDamageBoostAbAttr *x = P(MoveDamageBoostAbAttr, a);
        GATE(call_bool(bt, x->condition, a->cls, p, o, move));
        *pp->value = to_dmg_value(*pp->value * x->damageMultiplier);
        return 1;
    }
    case PR_AC_ReceivedMoveDamageMultiplierAbAttr:
    case PR_AC_ReceivedTypeDamageMultiplierAbAttr: {
        pr_fn_id cond; double mult;
        if (a->cls == PR_AC_ReceivedMoveDamageMultiplierAbAttr) {
            cond = P(ReceivedMoveDamageMultiplierAbAttr, a)->condition;
            mult = P(ReceivedMoveDamageMultiplierAbAttr, a)->damageMultiplier;
        } else {
            cond = P(ReceivedTypeDamageMultiplierAbAttr, a)->condition;
            mult = P(ReceivedTypeDamageMultiplierAbAttr, a)->damageMultiplier;
        }
        /* PokemonDefendCondition: (target, user, move) */
        GATE(call_bool(bt, cond, a->cls, p, o, move));
        *pp->value = to_dmg_value(*pp->value * mult);
        return 1;
    }
    case PR_AC_FormBlockDamageAbAttr: {
        /* Disguise, Ice Face. apply does nothing when simulated. */
        const pr_attr_FormBlockDamageAbAttr *x = P(FormBlockDamageAbAttr, a);
        GATE(*pp->value > 0 && p->form == x->formIndex && call_bool(bt, x->condition, a->cls, p, o, move)
             && !pr_move_hits_substitute(bt, move, o, p));
        if (pp->simulated) return 1;
        *pp->value = 0;
        /* Disguise's recoil hits at once (a later passive then sees the lower HP); the form change is queued */
        if (x->recoil > 0) pr_damage_indirect(bt, (pr_pokemon *)p, to_dmg_value(pr_get_stat(p, STAT_HP, true) * x->recoil));
        pr_note_deferred(bt, a->cls);
        return 1;
    }
    case PR_AC_AlliedFieldDamageReductionAbAttr:
        GATE(true);
        *pp->value = to_dmg_value(*pp->value * P(AlliedFieldDamageReductionAbAttr, a)->damageMultiplier);
        return 1;
    case PR_AC_PreDefendFullHpEndureAbAttr:
        GATE(p->hp >= pr_get_stat(p, STAT_HP, true) && pr_get_stat(p, STAT_HP, true) > 1 && *pp->value >= p->hp
             && !pr_has_battler_tag(bt, p, BATTLER_TAG_TYPE_STURDY));
        if (!pp->simulated) pr_note_deferred(bt, a->cls); /* adds the STURDY tag; the damage itself is capped later */
        return 1;

    /* ---- crits ---- */
    case PR_AC_ConditionalCritAbAttr:
        GATE(!*pp->flag && call_bool(bt, P(ConditionalCritAbAttr, a)->condition, a->cls, p, o, move));
        *pp->flag = true;
        return 1;
    case PR_AC_BonusCritAbAttr:
        GATE(true);
        *pp->holder += 1;
        return 1;
    case PR_AC_BlockCritAbAttr:
        GATE(true);
        *pp->flag = true;
        return 1;

    /* ---- screens and Substitute ---- */
    case PR_AC_InfiltratorAbAttr:
        GATE(!*pp->flag);
        *pp->flag = true;
        return 1;

    /* ---- Mold Breaker and co. ---- */
    case PR_AC_MoveAbilityBypassAbAttr:
        GATE(!*pp->flag && call_bool(bt, P(MoveAbilityBypassAbAttr, a)->moveIgnoreFunc, a->cls, p, NULL, move));
        *pp->flag = true;
        return 1;

    /* ---- end of turn ---- */
    case PR_AC_FetchBallAbAttr:
        GATE(!pp->simulated && bt->last_used_pokeball >= 0 && p->is_player);
        pr_note_unported(bt, a->cls); /* globalScene.pokeballCounts isn't modelled */
        bt->last_used_pokeball = -1;
        return 1;

    /* ---- secondary effect chance (MoveEffectAttr.getMoveChance) ---- */
    /* ---- StatStageChangePhase: protection from drops, reactions ---- */
    case PR_AC_ProtectStatAbAttr: {                                     /* Clear Body, Hyper Cutter, Keen Eye... */
        const pr_attr_ProtectStatAbAttr *x = P(ProtectStatAbAttr, a);
        int ps = (x->nulls & (1u << 2)) ? -1 : x->protectedStat;          /* null: every stat */
        bool any = false;
        for (int i = 0; i < pp->n_ch; i++)
            any |= (pp->ch_stat[i] == ps || ps < 0) && !(*pp->cancelled_stats & (1u << pp->ch_stat[i]));
        GATE(any);
        if (ps >= 0) *pp->cancelled_stats |= (uint16_t)(1u << ps);
        else for (int i = 0; i < pp->n_ch; i++) *pp->cancelled_stats |= (uint16_t)(1u << pp->ch_stat[i]);
        return 1;
    }
    case PR_AC_ConditionalUserFieldProtectStatAbAttr: {                 /* Flower Veil */
        /* the constructor drops its protectedStat argument, so every stat is protected */
        const pr_attr_ConditionalUserFieldProtectStatAbAttr *x = P(ConditionalUserFieldProtectStatAbAttr, a);
        if (!pp->target) return 0;
        bool any = false;
        for (int i = 0; i < pp->n_ch; i++) any |= !(*pp->cancelled_stats & (1u << pp->ch_stat[i]));
        GATE(any && call_bool(bt, x->condition, a->cls, pp->target, NULL, -1));
        for (int i = 0; i < pp->n_ch; i++) *pp->cancelled_stats |= (uint16_t)(1u << pp->ch_stat[i]);
        return 1;
    }
    case PR_AC_ReflectStatStageChangeAbAttr: {                          /* Mirror Armor */
        if (!pp->simulated && !c->e) return -1;
        int8_t rs[7], rg[7];
        int nr = 0;
        for (int i = 0; i < pp->n_ch && nr < 7; i++)
            if (!(*pp->cancelled_stats & (1u << pp->ch_stat[i]))) { rs[nr] = pp->ch_stat[i]; rg[nr++] = pp->ch_stages[i]; }
        GATE(o && nr > 0);
        /* StatStageChangePhase({battlerIndex: source, changes: reflected, sourcePokemon: undefined,
         * ignoreAbilities: false, sourceEffectType: MIRROR_ARMOR}) */
        if (!pp->simulated
            && pr_unshift_stat_stage_change_ex(bt, c->e, pr_battler_index(bt, o), rs, rg, nr, NULL,
                                               STAT_CHANGE_SOURCE_MIRROR_ARMOR) < 0)
            return -1;
        for (int i = 0; i < nr; i++) *pp->cancelled_stats |= (uint16_t)(1u << rs[i]);
        return 1;
    }
    case PR_AC_StatStageChangeCopyAbAttr:                               /* Opportunist */
        if (!pp->simulated && !c->e) return -1;
        GATE(true);
        if (!pp->simulated
            && pr_unshift_stat_stage_change_ex(bt, c->e, pr_battler_index(bt, p), pp->ch_stat, pp->ch_stages, pp->n_ch,
                                               (pr_pokemon *)p, STAT_CHANGE_SOURCE_OPPORTUNIST) < 0)
            return -1;
        return 1;
    case PR_AC_PostStatStageChangeStatStageChangeAbAttr: {              /* Defiant, Competitive */
        /* changeFunc(pokemon, changes) returns a StatChange; the two upstream closures are
         * {stat: ATK or SPATK, stages: changes[0].stages < 0 ? 2 * changes.length : 0} */
        pr_fn_id fn = P(PostStatStageChangeStatStageChangeAbAttr, a)->changeFunc;
        const char *h = fn < PR_CLOSURE_COUNT ? pr_closures_meta[fn].hash : "";
        int8_t st = !strcmp(h, "f798b2762ab2") ? STAT_ATK : !strcmp(h, "8d8ced84c6c1") ? STAT_SPATK : -1;
        if (st < 0) { if (fn < PR_CLOSURE_COUNT) pr_closure_misses[fn]++; return -1; }
        if (!pp->simulated && !c->e) return -1;
        pr_closure_hits[fn]++;
        int8_t sg = (int8_t)(pp->n_ch && pp->ch_stages[0] < 0 ? 2 * pp->n_ch : 0);
        GATE(!pp->self_target && sg != 0);
        if (!pp->simulated
            && pr_unshift_stat_stage_change_ex(bt, c->e, pr_battler_index(bt, p), &st, &sg, 1, (pr_pokemon *)p, 0) < 0)
            return -1;
        return 1;
    }
    case PR_AC_StatStageChangeMultiplierAbAttr:                 /* Simple x2, Contrary x-1 */
        GATE(true);
        *pp->value *= P(StatStageChangeMultiplierAbAttr, a)->multiplier;
        return 1;
    case PR_AC_MoveEffectChanceMultiplierAbAttr:
        GATE(!(*pp->value <= 0 || move == MOVE_ID_ORDER_UP || move == MOVE_ID_ELECTRO_SHOT));
        *pp->value *= P(MoveEffectChanceMultiplierAbAttr, a)->chanceMultiplier;
        if (*pp->value > 100) *pp->value = 100;
        return 1;
    case PR_AC_IgnoreMoveEffectsAbAttr:
        GATE(*pp->value > 0);
        *pp->value = 0;
        return 1;
    }
    return -1;
}
#undef GATE

/* attr.getTriggerMessage() gives a message (for any params) */
static bool has_trigger_message(uint16_t cls)
{
    static const uint16_t always[] = {
        PR_AC_BlockItemTheftAbAttr, PR_AC_NonSuperEffectiveImmunityAbAttr, PR_AC_FullHpResistTypeAbAttr,
        PR_AC_MoveImmunityAbAttr, PR_AC_PostDefendTypeChangeAbAttr, PR_AC_PostDefendContactDamageAbAttr,
        PR_AC_PostDefendPerishSongAbAttr, PR_AC_PostDefendAbilitySwapAbAttr, PR_AC_PostDefendAbilityGiveAbAttr,
        PR_AC_PokemonTypeChangeAbAttr, PR_AC_IntimidateImmunityAbAttr, PR_AC_PostSummonCopyAbilityAbAttr,
        PR_AC_PostSummonCopyAllyStatsAbAttr, PR_AC_ReflectStatStageChangeAbAttr, PR_AC_ProtectStatAbAttr,
        PR_AC_PreSetStatusEffectImmunityAbAttr, PR_AC_PreApplyBattlerTagAbAttr, PR_AC_ArenaTrapAbAttr,
        PR_AC_PostFaintContactDamageAbAttr, PR_AC_PostFaintHPDamageAbAttr, PR_AC_FormBlockDamageAbAttr,
        PR_AC_BypassSpeedChanceAbAttr, PR_AC_TerrainEventTypeChangeAbAttr};
    for (unsigned i = 0; i < sizeof always / sizeof always[0]; i++)
        if (pr_attr_is(cls, always[i])) return true;
    return false;
}

/* AbAttr.showAbility (every AbAttr struct starts with nulls and showAbility, but one) */
static bool shows_ability(const pr_attr *a)
{
    if (a->cls == PR_AC_PostVictoryFormChangeAbAttr) return ((const pr_attr_PostVictoryFormChangeAbAttr *)a->p)->showAbility;
    return ((const struct { uint32_t nulls; bool showAbility; } *)a->p)->showAbility;
}

static void set_add(pr_battle *bt, uint16_t *v, uint8_t *n, uint16_t x)
{
    for (int i = 0; i < *n; i++)
        if (v[i] == x) return;
    if (*n < 4) v[(*n)++] = x;
    else pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT);
}

static struct { uint16_t ability; uint8_t effect; } status_healed[16];
static int n_status_healed;

int pr_status_healed_get(uint16_t ability)
{
    for (int i = 0; i < n_status_healed; i++)
        if (status_healed[i].ability == ability) return status_healed[i].effect;
    return 0;
}

void pr_status_healed_set(uint16_t ability, int effect)
{
    int i = 0;
    while (i < n_status_healed && status_healed[i].ability < ability) i++;
    if (i < n_status_healed && status_healed[i].ability == ability) {
        status_healed[i].effect = (uint8_t)effect;
        return;
    }
    if (n_status_healed == (int)(sizeof status_healed / sizeof status_healed[0])) return;
    memmove(&status_healed[i + 1], &status_healed[i], (size_t)(n_status_healed - i) * sizeof status_healed[0]);
    status_healed[i].ability = ability;
    status_healed[i].effect = (uint8_t)effect;
    n_status_healed++;
}

void pr_status_healed_clear(void) { n_status_healed = 0; }

int pr_status_healed_list(uint16_t *ability, uint8_t *effect, int max)
{
    int n = n_status_healed < max ? n_status_healed : max;
    for (int i = 0; i < n; i++) {
        ability[i] = status_healed[i].ability;
        effect[i] = status_healed[i].effect;
    }
    return n;
}

void pr_apply_ab_attrs(pr_battle *bt, uint16_t cls, pr_ab_params *pp)
{
    const pr_pokemon *p = pp->pokemon;
    if (!p) return;
    /* what applySingleAbAttrs does around apply when not simulated; only the turn engine
     * keeps the phases and sets it touches (the damage test runs without one) */
    pr_engine *e = pp->simulated ? NULL : bt->engine;
    for (int passive = 0; passive < 2; passive++) {
        uint16_t ability = passive ? p->passive : p->ability;
        if (passive && (!p->passive || p->passive == p->ability)) continue;
        const pr_ability *ab = &pr_abilities[ability];
        bool checked = false, can = false;
        for (unsigned i = 0; i < ab->nattr; i++) {
            const pr_attr *a = &pr_attrs[ab->attr0 + i];
            if (!pr_attr_is(a->cls, cls)) continue;
            if (!checked) { can = pr_can_apply_ability(bt, p, ability); checked = true; }
            if (!can) break;
            if (!condition_ok(bt, a, p)) continue;
            ab_ctx c = {e, false, ability};
            int r = apply_one(bt, a, pp, &c);
            if (r < 0) { pr_note_unported(bt, a->cls); continue; }
            if (r == 0 || !e) continue;
            if (c.shown) pr_queue_ability_display(bt, e, p, false);
            pr_pokemon *w = (pr_pokemon *)p;
            set_add(bt, w->ab_applied_wave, &w->n_ab_applied_wave, ability);
            set_add(bt, w->ab_applied_summon, &w->n_ab_applied_summon, ability);
        }
    }
}
