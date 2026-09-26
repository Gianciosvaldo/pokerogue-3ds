/* Ability attribute dispatch: ports of applyAbAttrs (src/data/abilities/apply-ab-attrs.ts),
 * Pokemon.canApplyAbility, and the canApply/apply pairs of the AbAttr classes on the
 * damage path (src/data/abilities/ab-attrs.ts). A class with no case here is reported
 * unported, never skipped silently. */
#include "pr/battle.h"
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

bool pr_move_does_flag_effect_apply(pr_battle *bt, int move, unsigned flag, const pr_pokemon *user,
                                    const pr_pokemon *target)
{
    switch (flag) {
    case MOVE_FLAGS_MAKES_CONTACT:
        if (pr_has_ability_with_attr(bt, user, PR_AC_IgnoreContactAbAttr) || pr_move_hits_substitute(bt, move, user, target))
            return false;
        break;
    case MOVE_FLAGS_IGNORE_ABILITIES:
    case MOVE_FLAGS_IGNORE_PROTECT:
        pr_note_unported(bt, PR_AC_MoveAttr);
        break;
    }
    return (pr_moves[move].flags & flag) != 0;
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

/* canApply + apply for one attr. Returns false when the class has no C port. */
static bool apply_one(pr_battle *bt, const pr_attr *a, pr_ab_params *pp)
{
    const pr_pokemon *p = pp->pokemon, *o = pp->opponent;
    int move = pp->move;
    switch (a->cls) {
    /* ---- stats ---- */
    case PR_AC_StatMultiplierAbAttr: {
        const pr_attr_StatMultiplierAbAttr *x = P(StatMultiplierAbAttr, a);
        if (pp->stat != x->stat) return true;
        if (x->condition != PR_NO_FN && !call_bool(bt, x->condition, a->cls, p, NULL, move < 0 ? 0 : move)) return true;
        *pp->value *= x->multiplier;
        return true;
    }
    case PR_AC_AllyStatMultiplierAbAttr: {
        const pr_attr_AllyStatMultiplierAbAttr *x = P(AllyStatMultiplierAbAttr, a);
        if (pp->stat == x->stat && !(pp->ignore_ability && x->ignorable)) *pp->value *= x->multiplier;
        return true;
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
        if (ok) {
            *pp->value *= x->multiplier;
            *pp->flag = true;
        }
        return true;
    }
    case PR_AC_IgnoreOpponentStatStagesAbAttr:
        if (type_in(&P(IgnoreOpponentStatStagesAbAttr, a)->stats, pp->stat)) *pp->flag = true;
        return true;

    /* ---- move type and power ---- */
    case PR_AC_MoveTypeChangeAbAttr: {
        const pr_attr_MoveTypeChangeAbAttr *x = P(MoveTypeChangeAbAttr, a);
        if (call_bool(bt, x->condition, a->cls, p, o, move)) *pp->holder = x->newType;
        return true;
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
        if (call_bool(bt, cond, a->cls, p, o, move)) *pp->value *= mult;
        return true;
    }
    case PR_AC_AllyMoveCategoryPowerBoostAbAttr: {
        const pr_attr_AllyMoveCategoryPowerBoostAbAttr *x = P(AllyMoveCategoryPowerBoostAbAttr, a);
        if (call_bool(bt, x->condition, a->cls, p, o, move)) *pp->value *= x->powerMultiplier;
        return true;
    }
    case PR_AC_UserFieldMoveTypePowerBoostAbAttr: {
        const pr_attr_UserFieldMoveTypePowerBoostAbAttr *x = P(UserFieldMoveTypePowerBoostAbAttr, a);
        if (call_bool(bt, x->condition, a->cls, p, o, move)) *pp->value *= x->powerMultiplier;
        return true;
    }
    case PR_AC_PreAttackWeatherOverrideAbAttr: {
        int w = P(PreAttackWeatherOverrideAbAttr, a)->weatherType;
        if (*pp->holder != w) *pp->holder = w;
        return true;
    }
    case PR_AC_WeightMultiplierAbAttr:
        *pp->value *= P(WeightMultiplierAbAttr, a)->multiplier;
        return true;
    case PR_AC_UngroundedAbAttr:
        *pp->flag = true;
        return true;

    /* ---- effectiveness and immunities ---- */
    case PR_AC_IgnoreTypeImmunityAbAttr: {
        const pr_attr_IgnoreTypeImmunityAbAttr *x = P(IgnoreTypeImmunityAbAttr, a);
        if (!*pp->flag && x->defenderType == pp->defender_type && type_in(&x->allowedMoveTypes, pp->move_type))
            *pp->flag = true;
        return true;
    }
    case PR_AC_TypeImmunityAbAttr:
    case PR_AC_TypeImmunityHealAbAttr:
    case PR_AC_TypeImmunityStatStageChangeAbAttr:
    case PR_AC_TypeImmunityAddBattlerTagAbAttr: {
        int immune;
        switch (a->cls) {
        case PR_AC_TypeImmunityHealAbAttr: immune = P(TypeImmunityHealAbAttr, a)->immuneType; break;
        case PR_AC_TypeImmunityStatStageChangeAbAttr: immune = P(TypeImmunityStatStageChangeAbAttr, a)->immuneType; break;
        case PR_AC_TypeImmunityAddBattlerTagAbAttr: immune = P(TypeImmunityAddBattlerTagAbAttr, a)->immuneType; break;
        default: pr_note_unported(bt, a->cls); return true; /* no plain instances exist */
        }
        int tgt = pr_moves[move].target;
        if (tgt == MOVE_TARGET_BOTH_SIDES || tgt == MOVE_TARGET_ENEMY_SIDE || tgt == MOVE_TARGET_USER_SIDE) return true;
        if (o == p || pr_get_move_type(bt, o, move) != immune) return true;
        *pp->value = 0;
        if (a->cls == PR_AC_TypeImmunityHealAbAttr) {
            if (p->hp < pr_get_stat(p, STAT_HP, true) && !pp->simulated) {
                pr_note_deferred(bt, a->cls); /* PokemonHealPhase */
                *pp->flag = true;
            }
        } else {
            *pp->flag = true;
            if (!pp->simulated) pr_note_deferred(bt, a->cls); /* StatStageChangePhase / addTag */
        }
        return true;
    }
    case PR_AC_NonSuperEffectiveImmunityAbAttr:
        if (pr_move_is_attack(move) && *pp->value < 2) {
            *pp->flag = true;
            *pp->value = 0;
        }
        return true;
    case PR_AC_MoveImmunityAbAttr:
    case PR_AC_MoveImmunityStatStageChangeAbAttr: {
        pr_fn_id cond = a->cls == PR_AC_MoveImmunityAbAttr ? P(MoveImmunityAbAttr, a)->immuneCondition
                                                           : P(MoveImmunityStatStageChangeAbAttr, a)->immuneCondition;
        if (a->cls == PR_AC_MoveImmunityStatStageChangeAbAttr && pp->simulated) return true;
        if (*pp->flag || !call_bool(bt, cond, a->cls, p, o, move)) return true;
        *pp->flag = true;
        if (a->cls == PR_AC_MoveImmunityStatStageChangeAbAttr) pr_note_deferred(bt, a->cls);
        return true;
    }
    case PR_AC_ChangeMovePriorityAbAttr: {
        /* Prankster, Gale Wings, Triage: moveFunc(pokemon, move) */
        const pr_attr_ChangeMovePriorityAbAttr *x = P(ChangeMovePriorityAbAttr, a);
        if (call_bool(bt, x->moveFunc, a->cls, p, NULL, move)) *pp->value += x->changeAmount;
        return true;
    }
    case PR_AC_FieldPriorityMoveImmunityAbAttr: {
        if (*pp->flag) return true;
        int tgt = pr_moves[move].target;
        bool ally_target = tgt == MOVE_TARGET_USER || tgt == MOVE_TARGET_NEAR_ALLY || tgt == MOVE_TARGET_ALLY
                           || tgt == MOVE_TARGET_USER_OR_NEAR_ALLY || tgt == MOVE_TARGET_USER_AND_ALLIES
                           || tgt == MOVE_TARGET_USER_SIDE;
        if (pr_move_get_priority(bt, move, o) > 0 && !ally_target && pr_is_opponent(bt, o, p)) *pp->flag = true;
        return true;
    }
    case PR_AC_FullHpResistTypeAbAttr:
        if (!pr_move_has_attr((uint16_t)move, PR_AC_FixedDamageAttr) && p->hp >= pr_get_stat(p, STAT_HP, true)
            && *pp->value > 0.5) {
            *pp->value = 0.5;
            ((pr_pokemon *)p)->turn_move_effectiveness = 0.5; /* pokemon.turnData.moveEffectiveness */
        }
        return true;

    /* ---- damage ---- */
    case PR_AC_StabBoostAbAttr:
        if (*pp->value > 1) *pp->value += 0.5;
        return true;
    case PR_AC_MultCritAbAttr:
        if (*pp->value > 1) *pp->value *= P(MultCritAbAttr, a)->multAmount;
        return true;
    case PR_AC_BypassBurnDamageReductionAbAttr:
        if (!*pp->flag) *pp->flag = true;
        return true;
    case PR_AC_MoveDamageBoostAbAttr: {
        const pr_attr_MoveDamageBoostAbAttr *x = P(MoveDamageBoostAbAttr, a);
        if (call_bool(bt, x->condition, a->cls, p, o, move)) *pp->value = to_dmg_value(*pp->value * x->damageMultiplier);
        return true;
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
        if (call_bool(bt, cond, a->cls, p, o, move)) *pp->value = to_dmg_value(*pp->value * mult);
        return true;
    }
    case PR_AC_FormBlockDamageAbAttr: {
        /* Disguise, Ice Face. canApply: damage > 0, in the blocking form, condition, and the move
         * doesn't hit a substitute. apply does nothing when simulated. */
        const pr_attr_FormBlockDamageAbAttr *x = P(FormBlockDamageAbAttr, a);
        if (!(*pp->value > 0 && p->form == x->formIndex && call_bool(bt, x->condition, a->cls, p, o, move)
              && !pr_move_hits_substitute(bt, move, o, p)))
            return true;
        if (pp->simulated) return true;
        *pp->value = 0;
        /* Disguise's recoil hits at once (a later passive then sees the lower HP); the form change is queued */
        if (x->recoil > 0) pr_damage_indirect(bt, (pr_pokemon *)p, to_dmg_value(pr_get_stat(p, STAT_HP, true) * x->recoil));
        pr_note_deferred(bt, a->cls);
        return true;
    }
    case PR_AC_AlliedFieldDamageReductionAbAttr:
        *pp->value = to_dmg_value(*pp->value * P(AlliedFieldDamageReductionAbAttr, a)->damageMultiplier);
        return true;
    case PR_AC_PreDefendFullHpEndureAbAttr:
        /* canApply: full HP, max HP > 1, damage >= hp, no STURDY tag */
        if (p->hp >= pr_get_stat(p, STAT_HP, true) && pr_get_stat(p, STAT_HP, true) > 1 && *pp->value >= p->hp
            && !pr_has_battler_tag(bt, p, BATTLER_TAG_TYPE_STURDY) && !pp->simulated)
            pr_note_deferred(bt, a->cls); /* adds the STURDY tag; the damage itself is capped later */
        return true;

    /* ---- crits ---- */
    case PR_AC_ConditionalCritAbAttr:
        if (!*pp->flag && call_bool(bt, P(ConditionalCritAbAttr, a)->condition, a->cls, p, o, move)) *pp->flag = true;
        return true;
    case PR_AC_BonusCritAbAttr:
        *pp->holder += 1;
        return true;
    case PR_AC_BlockCritAbAttr:
        *pp->flag = true;
        return true;

    /* ---- screens and Substitute ---- */
    case PR_AC_InfiltratorAbAttr:
        if (!*pp->flag) *pp->flag = true; /* canApply: bypassed not yet set */
        return true;
    }
    return false;
}

void pr_apply_ab_attrs(pr_battle *bt, uint16_t cls, pr_ab_params *pp)
{
    const pr_pokemon *p = pp->pokemon;
    if (!p) return;
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
            if (!apply_one(bt, a, pp)) pr_note_unported(bt, a->cls);
        }
    }
}
