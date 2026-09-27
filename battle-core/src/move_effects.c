/* A move's MoveEffectAttrs (src/data/moves/move.ts): the apply() of each subclass, called by
 * MoveEffectPhase.triggerMoveEffects in move order.
 *
 * Ported so far: the gates in front of the common secondary effects (canApply, the move's
 * effect chance and its roll on the battle RNG), so a 10% burn that doesn't happen costs
 * nothing, and the effects that only queue phases or clear a status. An effect that would go
 * on to do something not ported yet reports its class (pr_note_unported) at that point. */
#include "pr/engine.h"

#define P(T, a) ((const pr_attr_##T *)(a)->p)

/* MoveEffectAttr.canApply(user, target): the Pokemon the effect lands on isn't fainted */
static bool effect_can_apply(const pr_effect_info *ef, const pr_pokemon *user, const pr_pokemon *target)
{
    const pr_pokemon *p = ef->self_target ? user : target;
    return p->hp > 0;
}

/* MoveEffectAttr.getMoveChance(user, target, move, selfEffect, showAbility) */
static double get_move_chance(pr_battle *bt, const pr_effect_info *ef, const pr_pokemon *user,
                              const pr_pokemon *target, int move, bool self_effect, bool show_ability)
{
    double chance = ef->chance_override >= 0 ? ef->chance_override : pr_moves[move].chance;
    pr_ab_params up = {.pokemon = user, .move = move, .simulated = !show_ability, .value = &chance};
    pr_apply_ab_attrs(bt, PR_AC_MoveEffectChanceMultiplierAbAttr, &up);
    if ((!pr_move_has_attr((uint16_t)move, PR_AC_FlinchAttr) || chance <= pr_moves[move].chance)
        && !pr_move_has_attr((uint16_t)move, PR_AC_SecretPowerAttr)) {
        /* arena.applyTagsForSide(WATER_FIRE_PLEDGE, userSide, moveChance): the rainbow doubles it */
        int side = user->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY;
        for (int i = 0; i < bt->n_arena_tags; i++)
            if (bt->arena_tags[i].type == ARENA_TAG_TYPE_WATER_FIRE_PLEDGE && pr_arena_tag_on_side(&bt->arena_tags[i], side))
                pr_note_unported(bt, PR_FEAT_ARENA_TAG_TURN);
    }
    if (!self_effect) {
        pr_ab_params tp = {.pokemon = target, .move = move, .simulated = !show_ability, .value = &chance};
        pr_apply_ab_attrs(bt, PR_AC_IgnoreMoveEffectsAbAttr, &tp);
    }
    return chance;
}

/* `moveChance < 0 || moveChance === 100 || user.randBattleSeedInt(100) < moveChance` */
static bool chance_roll(pr_battle *bt, double chance)
{
    return chance < 0 || chance == 100 || pr_battle_rand_seed_int(bt, 100, 0) < chance;
}

static void unshift_pokemon_phase(pr_battle *bt, pr_engine *e, pr_phase_type type, const pr_pokemon *p)
{
    int ph = pr_phase_new(e, type, (int8_t)pr_battler_index(bt, p));
    if (ph < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); e->overflow = true; return; }
    e->pool[ph].pokemon = (pr_pokemon *)p;
    pr_phase_unshift(bt, e, ph);
}

/* AddBattlerTagAttr.apply, from the chance roll on: addTag isn't ported */
static void add_battler_tag(pr_battle *bt, const pr_attr *a, const pr_effect_info *ef, pr_pokemon *user,
                            pr_pokemon *target, int move)
{
    if (!effect_can_apply(ef, user, target)) return;
    double chance = get_move_chance(bt, ef, user, target, move, ef->self_target, true);
    if (chance_roll(bt, chance)) pr_note_unported(bt, a->cls);
}

void pr_apply_move_effect(pr_battle *bt, pr_engine *e, int ph, int attr_index, pr_pokemon *user, pr_pokemon *target)
{
    const pr_attr *a = &pr_attrs[attr_index];
    const pr_effect_info *ef = &pr_attr_effects[attr_index];
    int move = e->pool[ph].move_id;
    if (!target && !ef->self_target) {
        /* POST_TARGET effects get a null target; only self-targeted ones read none */
        pr_note_unported(bt, a->cls);
        return;
    }
    switch (a->cls) {
    case PR_AC_StatStageChangeAttr: {
        if (!effect_can_apply(ef, user, target)) return;
        if (ef->condition != PR_NO_FN && !pr_closure_call(bt, ef->condition, a->cls, 0, user, target, move)) return;
        double chance = get_move_chance(bt, ef, user, target, move, ef->self_target, true);
        if (chance_roll(bt, chance)) {
            /* StatStageChangePhase({battlerIndex, changes: groupStatChange(stats, getLevels(user)),
             * sourcePokemon: user, message: options.message}); no StatStageChangeAttr has a message */
            const pr_attr_StatStageChangeAttr *x = P(StatStageChangeAttr, a);
            pr_pokemon *on = ef->self_target ? user : target;
            if (pr_unshift_stat_stage_change(bt, e, pr_battler_index(bt, on), x->stats.v, x->stats.n, x->stages, user) < 0)
                e->overflow = true;
        }
        return;
    }
    case PR_AC_StatusEffectAttr: {
        /* no canApply check: the chance is rolled even against a fainted target */
        double chance = get_move_chance(bt, ef, user, target, move, ef->self_target, true);
        if (chance_roll(bt, chance)) pr_note_unported(bt, a->cls); /* target.trySetStatus(...) */
        return;
    }
    case PR_AC_HealStatusEffectAttr: {
        const pr_attr_HealStatusEffectAttr *x = P(HealStatusEffectAttr, a);
        if (!effect_can_apply(ef, user, target)) return;
        /* Shield Dust stops Sparkling Aria's cure when it has one target (getMoveTargets) */
        if (move == MOVE_ID_SPARKLING_ARIA && pr_has_ability_with_attr(bt, target, PR_AC_IgnoreMoveEffectsAbAttr)) {
            pr_note_unported(bt, a->cls);
            return;
        }
        pr_pokemon *p = ef->self_target ? user : target;
        if (!p->status) return;
        for (unsigned i = 0; i < x->effects.n; i++)
            if (x->effects.v[i] == p->status) {
                pr_queue_message(bt, e);
                /* resetStatus(): revive = true, asPhase = true */
                unshift_pokemon_phase(bt, e, PH_RESET_STATUS, p);
                return;
            }
        return;
    }
    case PR_AC_AddBattlerTagAttr:
    case PR_AC_FlinchAttr:
        add_battler_tag(bt, a, ef, user, target, move);
        return;
    case PR_AC_ConfuseAttr: {
        /* !selfTarget && target.isSafeguarded(user) */
        if (!ef->self_target) {
            int side = target->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY;
            if (pr_arena_get_tag_on_side(bt, ARENA_TAG_TYPE_SAFEGUARD, side)) {
                pr_note_unported(bt, PR_FEAT_ARENA_TAG_TURN);
                return;
            }
        }
        add_battler_tag(bt, a, ef, user, target, move);
        return;
    }
    }
    pr_note_unported(bt, a->cls);
}
