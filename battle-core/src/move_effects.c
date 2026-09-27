/* A move's MoveEffectAttrs (src/data/moves/move.ts): the apply() of each subclass, called by
 * MoveEffectPhase.triggerMoveEffects in move order.
 *
 * Ported so far: the gates in front of the common secondary effects (canApply, the move's
 * effect chance and its roll on the battle RNG), so a 10% burn that doesn't happen costs
 * nothing, and the effects that only queue phases or clear a status. An effect that would go
 * on to do something not ported yet reports its class (pr_note_unported) at that point. */
#include "pr/engine.h"
#include <math.h>

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

/* AddBattlerTagAttr.apply (FlinchAttr, ConfuseAttr, RechargeAttr and TrapAttr share its fields): the chance roll, then
 * (selfTarget ? user : target).addTag(tagType, randBattleSeedIntRange(min, max), move.id, user.id) */
static void add_battler_tag(pr_battle *bt, pr_engine *e, const pr_attr *a, const pr_effect_info *ef,
                            pr_pokemon *user, pr_pokemon *target, int move)
{
    const pr_attr_AddBattlerTagAttr *x = (const pr_attr_AddBattlerTagAttr *)a->p;
    if (!effect_can_apply(ef, user, target)) return;
    double chance = get_move_chance(bt, ef, user, target, move, ef->self_target, true);
    if (!chance_roll(bt, chance)) return;
    int turns = pr_battle_rand_seed_int(bt, x->turnCountMax - x->turnCountMin + 1, x->turnCountMin);
    pr_add_tag(bt, e, ef->self_target ? user : target, x->tagType, turns, move, (int64_t)user->id);
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
    case PR_AC_StatusEffectAttr:
    case PR_AC_MultiStatusEffectAttr: {                 /* Tri Attack, Dire Claw */
        int effect;
        if (a->cls == PR_AC_MultiStatusEffectAttr) {
            /* this.effect = randSeedItem(this.effects): Phaser.Math.RND.pick, before the chance roll */
            const pr_intlist *l = &P(MultiStatusEffectAttr, a)->effects;
            if (l->n == 1) effect = l->v[0];
            else if (!bt->rng_live) { pr_note_unported(bt, PR_FEAT_GLOBAL_RNG); return; }
            else effect = l->v[pr_rng_integer_in_range(&bt->rng, 0, (int32_t)l->n - 1)];
        } else {
            effect = P(StatusEffectAttr, a)->effect;
        }
        /* no canApply check: the chance is rolled even against a fainted target */
        double chance = get_move_chance(bt, ef, user, target, move, ef->self_target, true);
        if (!chance_roll(bt, chance)) return;
        /* target.trySetStatus(effect, user, undefined, null, false, quiet): quiet unless a status move */
        pr_try_set_status(bt, e, target, effect, user, -1, false, pr_moves[move].category != MOVE_CATEGORY_STATUS);
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
    case PR_AC_HitHealAttr: {                            /* drain moves, Strength Sap */
        /* no canApply: it heals even a fainted user (the heal phase then does nothing) */
        const pr_attr_HitHealAttr *x = P(HitHealAttr, a);
        if (pr_has_ability_with_attr(bt, target, PR_AC_ReverseDrainAbAttr)) return;   /* Liquid Ooze */
        bool heal_stat = !(x->nulls & (1u << 3));
        double amount = heal_stat ? pr_get_effective_stat(bt, target, x->healStat, NULL, -1, false, false)
                                  : floor(user->single_hit_damage_dealt * x->healRatio);
        if (!heal_stat && amount < 1) amount = 1;                                  /* toDmgValue */
        pr_unshift_heal(bt, e, pr_battler_index(bt, user), amount,
                        PR_HEAL_MESSAGE | PR_HEAL_NO_FULL_HP_MESSAGE | PR_HEAL_SKIP_ANIM);
        return;
    }
    case PR_AC_RecoilAttr: {                             /* Double-Edge, Brave Bird, Struggle, Chloroblast... */
        const pr_attr_RecoilAttr *x = P(RecoilAttr, a);
        if (!effect_can_apply(ef, user, target)) return;
        if (!x->useHp && user->total_damage_dealt == 0) return;
        bool cancelled = false;
        if (!x->unblockable) {
            pr_ab_params rp = {.pokemon = user, .move = -1, .flag = &cancelled};
            pr_apply_ab_attrs(bt, PR_AC_BlockRecoilDamageAttr, &rp);
            pr_apply_ab_attrs(bt, PR_AC_BlockNonDirectDamageAbAttr, &rp);
        }
        if (cancelled) return;
        if (x->useHp) {
            /* no recoil when the move failed or missed (a missing entry counts as failed) */
            const pr_turn_move *last = pr_last_move(bt, user, 0);
            int r = last ? last->result : MOVE_RESULT_FAIL;
            if (r == MOVE_RESULT_FAIL || r == MOVE_RESULT_MISS) return;
        }
        double base = x->useHp ? pr_get_stat(user, STAT_HP, false) : user->total_damage_dealt;
        double recoil = floor(base * x->damageRatio);
        if (recoil < 1) recoil = 1;                                                 /* toDmgValue */
        /* damageAndUpdate(recoil, {result: INDIRECT, ignoreSegments: true}) */
        pr_damage_and_update(bt, e, user, recoil, HIT_RESULT_INDIRECT, false, NULL);
        pr_queue_message(bt, e);
        user->damage_taken += (int32_t)recoil;
        return;
    }
    case PR_AC_SemiInvulnerableAttr:                     /* a charge attr: Fly, Dig, Dive, Phantom Force... */
        if (!effect_can_apply(ef, user, target)) return;
        pr_add_tag(bt, e, user, P(SemiInvulnerableAttr, a)->tagType, 1, move, (int64_t)user->id);
        return;
    case PR_AC_GulpMissileTagAttr:                       /* a charge attr: Dive */
        if (!effect_can_apply(ef, user, target)) return;
        /* Cramorant with Gulp Missile catches something (a tag, then a form change) */
        if (pr_has_ability_active(bt, user, ABILITY_ID_GULP_MISSILE) && user->species == SPECIES_ID_CRAMORANT)
            pr_note_unported(bt, a->cls);
        return;
    case PR_AC_AddBattlerTagAttr:
    case PR_AC_FlinchAttr:
    case PR_AC_RechargeAttr:                             /* Hyper Beam and co.: RECHARGING */
    case PR_AC_TrapAttr:                                 /* Bind, Wrap, Fire Spin...: those tags aren't ported */
        add_battler_tag(bt, e, a, ef, user, target, move);
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
        add_battler_tag(bt, e, a, ef, user, target, move);
        return;
    }
    }
    pr_note_unported(bt, a->cls);
}
