/* A move's MoveEffectAttrs (src/data/moves/move.ts): the apply() of each subclass, called by
 * MoveEffectPhase.triggerMoveEffects in move order.
 *
 * Ported so far: the gates in front of the common secondary effects (canApply, the move's
 * effect chance and its roll on the battle RNG), so a 10% burn that doesn't happen costs
 * nothing, and the effects that only queue phases or clear a status. An effect that would go
 * on to do something not ported yet reports its class (pr_note_unported) at that point. */
#include "pr/engine.h"
#include <math.h>
#include <string.h>

#define P(T, a) ((const pr_attr_##T *)(a)->p)

static double to_dmg_value(double v) { double f = floor(v); return f < 1 ? 1 : f; }

/* pokemonMove.getMovePp() for moveset slot s: maxPpOverride || pp + ppUp * toDmgValue(pp / 5) */
static int reduce_pp_move_pp(const pr_pokemon *p, int s)
{
    if (p->max_pp_override[s] > 0) return p->max_pp_override[s];
    int pp = pr_moves[p->moves[s]].pp;
    return pp + p->pp_up[s] * (int)to_dmg_value(pp / 5.0);
}

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
                chance *= 2;
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

/* AddBattlerTagAttr.apply (FlinchAttr, ConfuseAttr, RechargeAttr, TrapAttr, FallDownAttr and
 * ExposedMoveAttr share its fields): the chance roll, then (selfTarget ? user : target).addTag(tagType,
 * randBattleSeedIntRange(min, max), move.id, user.id); whether the tag was added */
static bool add_battler_tag(pr_battle *bt, pr_engine *e, const pr_attr *a, const pr_effect_info *ef,
                            pr_pokemon *user, pr_pokemon *target, int move)
{
    const pr_attr_AddBattlerTagAttr *x = (const pr_attr_AddBattlerTagAttr *)a->p;
    if (!effect_can_apply(ef, user, target)) return false;
    double chance = get_move_chance(bt, ef, user, target, move, ef->self_target, true);
    if (!chance_roll(bt, chance)) return false;
    int turns = pr_battle_rand_seed_int(bt, x->turnCountMax - x->turnCountMin + 1, x->turnCountMin);
    return pr_add_tag(bt, e, ef->self_target ? user : target, x->tagType, turns, move, (int64_t)user->id);
}

/* StatStageChangeAttr.apply: canApply, the condition, the chance roll, then
 * StatStageChangePhase({battlerIndex, changes: groupStatChange(stats, getLevels(user)),
 * sourcePokemon: user, message: options.message}); no StatStageChangeAttr has a message.
 * Whether the phase was queued. */
static bool stat_stage_change(pr_battle *bt, pr_engine *e, const pr_effect_info *ef, uint16_t cls, pr_pokemon *user,
                              pr_pokemon *target, int move, const int32_t *stats, int n, int stages)
{
    if (!effect_can_apply(ef, user, target)) return false;
    if (ef->condition != PR_NO_FN && !pr_closure_call(bt, ef->condition, cls, 0, user, target, move)) return false;
    double chance = get_move_chance(bt, ef, user, target, move, ef->self_target, true);
    if (!chance_roll(bt, chance)) return false;
    /* Growth.getLevels runs only after the parent apply gates and chance roll.
     * The existing effective-weather helper applies the user's override before
     * checking field weather suppression, matching the pinned source. */
    if (cls == PR_AC_GrowthStatStageChangeAttr) {
        int weather = pr_get_effective_weather_for_move(bt, user);
        if (weather == WEATHER_TYPE_SUNNY || weather == WEATHER_TYPE_HARSH_SUN) stages++;
    }
    pr_pokemon *on = ef->self_target ? user : target;
    if (pr_unshift_stat_stage_change(bt, e, pr_battler_index(bt, on), stats, n, stages, user) < 0) e->overflow = true;
    return true;
}

/* PartingShotAttr: after the stat drop is queued, the user switches out only if the drop will
 * happen: no Mist on the target's side, and Attack or Sp. Atk can move (the stat-protecting
 * abilities simulated, the stages inside ±6, Contrary turning the drop into a raise) */
static void parting_shot(pr_battle *bt, pr_engine *e, const pr_attr *a, const pr_effect_info *ef, pr_pokemon *user,
                         pr_pokemon *target, int move)
{
    const pr_attr_PartingShotAttr *x = (const pr_attr_PartingShotAttr *)a->p;
    if (!stat_stage_change(bt, e, ef, a->cls, user, target, move, x->stats.v, x->stats.n, x->stages)) return;
    int side = target->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY;
    if (pr_arena_get_tag_on_side(bt, ARENA_TAG_TYPE_MIST, side)) return;
    int mod = pr_has_ability_active(bt, target, ABILITY_ID_CONTRARY) ? 1 : -1;
    uint16_t cancelled = 0;
    bool can[2];
    static const int8_t which[2] = {STAT_ATK, STAT_SPATK}, minus_one[1] = {-1};
    for (int k = 0; k < 2; k++) {
        pr_ab_params pp = {.pokemon = target, .move = -1, .simulated = true, .ch_stat = &which[k],
                           .ch_stages = minus_one, .n_ch = 1, .cancelled_stats = &cancelled};
        pr_apply_ab_attrs(bt, PR_AC_ProtectStatAbAttr, &pp);
        int st = target->stat_stages[which[k] - 1] + mod;
        can[k] = !(cancelled & (1u << which[k])) && st >= -6 && st <= 6;
    }
    if (!can[0] && !can[1]) return;
    if (pr_switch_out_condition(bt, true, SWITCH_TYPE_SWITCH, user, target, move))
        pr_force_switch_out(bt, e, true, SWITCH_TYPE_SWITCH, user, target, move);
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
    case PR_AC_PartyStatusCureAttr: {
        const pr_attr_PartyStatusCureAttr *x = P(PartyStatusCureAttr, a);
        /* This leaf proves singles, not the distinct active-partner display path. */
        int side = user->is_player ? 0 : 1;
        if (bt->double_battle || bt->n_party[side] < 1) { pr_note_unported(bt, a->cls); return; }
        for (int k = 0; k < bt->n_party[side]; k++) {
            pr_pokemon *p = bt->party[side][k];
            if (!p || (p != user && p->on_field && !p->switch_out_status)) { pr_note_unported(bt, a->cls); return; }
        }
        /* Custom canApply deliberately omits target HP. */
        if (ef->self_target) {
            if (user->hp <= 0 || pr_get_battler_tag(user, BATTLER_TAG_TYPE_FRENZY)) return;
        } else if (target && pr_get_battler_tag(target, BATTLER_TAG_TYPE_PROTECTED)
                   && !(pr_moves[move].flags & MOVE_FLAGS_IGNORE_PROTECT)) return;
        for (int k = 0; k < bt->n_party[side]; k++)
            pr_reset_status_phase(bt, e, bt->party[side][k], false);
        if (x->message) {
            /* The pin's dumped payload is already rendered English. Use its
             * exact factory value to select the existing locale representation. */
            const char *key = !strcmp(x->message, "A bell chimed!") ? "moveTriggers:bellChimed"
                : !strcmp(x->message, "A soothing aroma wafted through the area!")
                    ? "moveTriggers:soothingAromaWaftedThroughArea" : NULL;
            if (key) pr_queue_msg(bt, e, pr_msg0(key));
            else pr_queue_message(bt, e);
        }
        return;
    }
    case PR_AC_StatStageChangeAttr: {
        const pr_attr_StatStageChangeAttr *x = P(StatStageChangeAttr, a);
        stat_stage_change(bt, e, ef, a->cls, user, target, move, x->stats.v, x->stats.n, x->stages);
        return;
    }
    case PR_AC_GrowthStatStageChangeAttr: {
        const pr_attr_GrowthStatStageChangeAttr *x = P(GrowthStatStageChangeAttr, a);
        stat_stage_change(bt, e, ef, a->cls, user, target, move, x->stats.v, x->stats.n, x->stages);
        return;
    }
    case PR_AC_PartingShotAttr:
        parting_shot(bt, e, a, ef, user, target, move);
        return;
    case PR_AC_ReducePpMoveAttr:                         /* Spite */
    case PR_AC_AttackReducePpMoveAttr: {                 /* Eerie Spell: only when the move has PP left */
        int reduction = a->cls == PR_AC_ReducePpMoveAttr ? P(ReducePpMoveAttr, a)->reduction
                                                         : P(AttackReducePpMoveAttr, a)->reduction;
        bool unknown;
        const pr_turn_move *tm = pr_last_non_virtual_move(bt, target, false, true, &unknown);
        if (unknown) return;                             /* reported */
        int s = -1;
        for (int k = 0; tm && k < target->n_moves; k++)
            if (target->moves[k] == tm->move) { s = k; break; }
        int max = s < 0 ? 0 : reduce_pp_move_pp(target, s);
        if (a->cls == PR_AC_AttackReducePpMoveAttr) {
            double ratio = s < 0 ? 0 : 1.0 - (double)target->pp_used[s] / max;   /* getPpRatio() */
            if (!(ratio == ratio && ratio != 0)) return;
        } else if (s < 0) {
            pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT);  /* the condition rules it out; upstream would throw */
            return;
        }
        int last = target->pp_used[s], v = last + reduction;
        target->pp_used[s] = (uint8_t)(v < max ? v : max);
        pr_queue_msg(bt, e, pr_msg3("battle:ppReduced", pr_arg_pokemon(bt, "targetName", target),
                                    pr_arg_move("moveName", target->moves[s]),
                                    pr_arg_int("reduction", target->pp_used[s] - last)));
        return;
    }
    case PR_AC_ForceSwitchOutAttr: {                     /* Roar, Whirlwind, Dragon Tail, U-turn, Baton Pass... */
        const pr_attr_ForceSwitchOutAttr *x = P(ForceSwitchOutAttr, a);
        pr_force_switch_out(bt, e, x->selfSwitch, x->switchType, user, target, move);
        return;
    }
    case PR_AC_ChillyReceptionAttr: {                    /* snow first, whether or not the user can leave */
        const pr_attr_ChillyReceptionAttr *x = P(ChillyReceptionAttr, a);
        pr_try_set_weather(bt, e, WEATHER_TYPE_SNOW, user);
        pr_force_switch_out(bt, e, x->selfSwitch, x->switchType, user, target, move);
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
        /* getMoveTargets(user, move), which draws a RANDOM_NEAR_ENEMY move's target from the battle
         * RNG when there are two opponents (doubles; in singles it draws nothing) */
        int8_t mt[4];
        bool multiple;
        int nmt = bt->double_battle ? pr_get_move_targets(bt, user, move, -1, mt, &multiple) : 1;
        /* Shield Dust stops Sparkling Aria's cure when it has one target (left as before in singles) */
        if (move == MOVE_ID_SPARKLING_ARIA && pr_has_ability_with_attr(bt, target, PR_AC_IgnoreMoveEffectsAbAttr)) {
            if (!bt->double_battle) {
                pr_note_unported(bt, a->cls);
                return;
            }
            if (nmt == 1) return;
        }
        pr_pokemon *p = ef->self_target ? user : target;
        if (!p->status) return;
        for (unsigned i = 0; i < x->effects.n; i++)
            if (x->effects.v[i] == p->status) {
                pr_queue_msg(bt, e, pr_status_msg(bt, p->status, PR_ST_HEAL, p));
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
        pr_unshift_heal_msg(bt, e, pr_battler_index(bt, user), amount,
                            PR_HEAL_MESSAGE | PR_HEAL_NO_FULL_HP_MESSAGE | PR_HEAL_SKIP_ANIM,
                            heal_stat ? pr_msg1("battle:drainMessage", pr_arg_pokemon(bt, "pokemonName", target))
                                      : pr_msg1("battle:regainHealth", pr_arg_pokemon(bt, "pokemonName", user)));
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
        pr_queue_msg(bt, e, pr_msg1("moveTriggers:hitWithRecoil", pr_arg_pokemon(bt, "pokemonName", user)));
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
    case PR_AC_TrapAttr:                                 /* Bind, Wrap, Fire Spin... */
    case PR_AC_ProtectAttr:                              /* Protect, Detect, Spiky Shield... Endure */
    case PR_AC_LeechSeedAttr:
        add_battler_tag(bt, e, a, ef, user, target, move);
        return;
    case PR_AC_ConfuseAttr: {
        /* !selfTarget && target.isSafeguarded(user): Safeguard on the target's side, unless the
         * user's Infiltrator gets through; a status move says so */
        if (!ef->self_target) {
            int side = target->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY;
            if (pr_arena_get_tag_on_side(bt, ARENA_TAG_TYPE_SAFEGUARD, side)) {
                bool bypassed = false;
                pr_ab_params ip = {.pokemon = user, .move = -1, .flag = &bypassed};
                pr_apply_ab_attrs(bt, PR_AC_InfiltratorAbAttr, &ip);
                if (!bypassed) {
                    if (pr_moves[move].category == MOVE_CATEGORY_STATUS) pr_queue_msg(bt, e, pr_msg1("moveTriggers:safeguard", pr_arg_pokemon(bt, "targetName", target)));
                    return;
                }
            }
        }
        add_battler_tag(bt, e, a, ef, user, target, move);
        return;
    }
    case PR_AC_ClearWeatherAttr:                         /* Defog in fog; no canApply check */
        if (bt->weather == P(ClearWeatherAttr, a)->weatherType) pr_try_set_weather(bt, e, WEATHER_TYPE_NONE, user);
        return;
    case PR_AC_ClearTerrainAttr:                         /* Defog, Ice Spinner, Steel Roller */
        pr_try_set_terrain(bt, e, TERRAIN_NONE, true, user);
        return;
    case PR_AC_JawLockAttr:                              /* Jaw Lock: both trapped, each by the other */
        if (!effect_can_apply(ef, user, target)) return;
        if (pr_find_battler_tag_class(user, PR_TAGCLASS_TRAPPED) || pr_find_battler_tag_class(target, PR_TAGCLASS_TRAPPED))
            return;
        if (!chance_roll(bt, get_move_chance(bt, ef, user, target, move, ef->self_target, false))) return;
        if (pr_add_tag(bt, e, target, BATTLER_TAG_TYPE_TRAPPED, 1, move, (int64_t)user->id))
            pr_add_tag(bt, e, user, BATTLER_TAG_TYPE_TRAPPED, 1, move, (int64_t)target->id);
        return;
    case PR_AC_AddBattlerTagIfBoostedAttr:               /* Alluring Voice: confuses a target whose stats rose this turn */
        if (target->stat_stages_increased) add_battler_tag(bt, e, a, ef, user, target, move);
        return;
    case PR_AC_FallDownAttr:                             /* Smack Down, Thousand Arrows */
        if (pr_is_grounded(bt, target, true)) return;
        if (!add_battler_tag(bt, e, a, ef, user, target, move)) return;
        pr_remove_tag(bt, e, target, BATTLER_TAG_TYPE_FLOATING);
        pr_remove_tag(bt, e, target, BATTLER_TAG_TYPE_TELEKINESIS);
        if (pr_get_battler_tag(target, BATTLER_TAG_TYPE_FLYING)) {
            pr_remove_tag(bt, e, target, BATTLER_TAG_TYPE_FLYING);
            pr_add_tag(bt, e, target, BATTLER_TAG_TYPE_INTERRUPTED, 0, 0, -1);
        }
        pr_queue_msg(bt, e, pr_msg1("moveTriggers:fallDown", pr_arg_pokemon(bt, "targetPokemonName", target)));
        return;
    case PR_AC_WishAttr: {                               /* no canApply check */
        /* a WishTag on the target's slot (Wish targets its user): half the user's max HP, in 2 turns */
        double hp = floor(pr_get_stat(user, STAT_HP, false) / 2.0);
        pr_positional_tag pt = {.type = POSITIONAL_TAG_TYPE_WISH, .turn_count = 2,
                                .target_index = (int8_t)pr_battler_index(bt, target), .source_id = -1,
                                .heal_hp = hp < 1 ? 1 : hp, .pokemon_name = pr_arg_pokemon(bt, "", user).v};
        pr_pos_add(bt, e, &pt);
        return;
    }
    case PR_AC_RepeatMoveAttr: {                         /* Instruct: no canApply check */
        /* the target uses the moveset move the condition found, again, at the targets of its last
         * use (recomputed for a move that hits several), as a normal move going first */
        int slot = pr_instruct_slot(target);
        bool unknown;
        const pr_turn_move *last = pr_last_non_virtual_move(bt, target, false, true, &unknown);
        if (slot < 0 || !last) { pr_note_unported(bt, a->cls); return; }
        int mm = target->moves[slot];
        int8_t tg[4];
        int n;
        if (pr_move_is_multi_target(mm)) {
            bool multiple;
            n = pr_get_move_targets(bt, target, mm, -1, tg, &multiple);
        } else {
            n = last->n_targets;
            memcpy(tg, last->targets, (size_t)n);
        }
        /* in doubles, a lone target that has fainted (and isn't the target's ally): its ally, when
         * that one hasn't fainted */
        if (bt->double_battle && n == 1) {
            const pr_pokemon *ft = tg[0] >= 0 && tg[0] < 4 ? bt->field[tg[0]] : NULL;
            if (!ft) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return; }   /* upstream would throw */
            const pr_pokemon *fa = ft->hp <= 0 && ft != pr_get_ally(bt, target) ? pr_get_ally(bt, ft) : NULL;
            if (fa && fa->hp > 0) tg[0] = (int8_t)pr_battler_index(bt, fa);
        }
        pr_queue_msg(bt, e, pr_msg2("moveTriggers:instructingMove", pr_arg_pokemon(bt, "userPokemonName", user),
                                    pr_arg_pokemon(bt, "targetPokemonName", target)));
        pr_unshift_move_phase(bt, e, target, mm, slot, tg, n, MOVE_USE_MODE_NORMAL, MOVE_PHASE_TIMING_MODIFIER_FIRST);
        return;
    }
    case PR_AC_ExposedMoveAttr:                          /* Foresight, Odor Sleuth, Miracle Eye */
        if (add_battler_tag(bt, e, a, ef, user, target, move))
            pr_queue_msg(bt, e, pr_msg2("moveTriggers:exposedMove", pr_arg_pokemon(bt, "pokemonName", user),
                                        pr_arg_pokemon(bt, "targetPokemonName", target)));
        return;
    case PR_AC_AddTypeAttr:                              /* Forest's Curse, Trick-or-Treat: no canApply check */
        target->added_type = (int8_t)P(AddTypeAttr, a)->type;
        pr_queue_msg(bt, e, pr_msg2("moveTriggers:addType", pr_arg_type("typeName", P(AddTypeAttr, a)->type),
                                    pr_arg_pokemon(bt, "pokemonName", target)));
        return;
    case PR_AC_OrderUpStatBoostAttr:                     /* Order Up: only with a Tatsugiri commanding (doubles) */
        if (pr_get_battler_tag(user, BATTLER_TAG_TYPE_COMMANDED)) pr_note_unported(bt, a->cls);
        return;
    case PR_AC_HealOnAllyAttr:                           /* Pollen Puff: heals only the user's ally (doubles) */
        if (bt->double_battle) pr_note_unported(bt, a->cls);
        return;
    case PR_AC_StatusIfBoostedAttr:                      /* Burning Jealousy (self-targeted, reads the real target) */
        if (target->stat_stages_increased)
            pr_try_set_status(bt, e, target, P(StatusIfBoostedAttr, a)->effect, user, -1, false, true);
        return;
    case PR_AC_FlameBurstAttr:                           /* hits the target's ally: a single battle has none */
        if (bt->double_battle) pr_note_unported(bt, a->cls);
        return;
    case PR_AC_MoneyAttr: {                              /* Pay Day: moneyScattered += getWaveMoneyAmount(0.2) */
        int w = bt->wave_index, set = (int)ceil(w / 10.0) - 1;
        double v = js_pow((set + 1 + (0.75 + (((w - 1) % 10) + 1) / 10.0)) * 100, 1 + 0.005 * set) * 0.2;
        bt->money_scattered += (int32_t)(floor(v / 10) * 10);
        pr_queue_msg(bt, e, pr_msg0("moveTriggers:coinsScatteredEverywhere"));
        return;
    }
    case PR_AC_RemoveHeldItemAttr:
        pr_remove_move_held_item(bt, e, user, target, P(RemoveHeldItemAttr, a)->berriesOnly, NULL);
        return;
    case PR_AC_StealHeldItemChanceAttr: {                /* Thief, Covet: randSeedFloat() > chance fails */
        if (!bt->rng_live) { pr_note_unported(bt, PR_FEAT_GLOBAL_RNG); return; }
        if (pr_rng_seed_float(&bt->rng) > P(StealHeldItemChanceAttr, a)->chance) return;
        if (pr_held_item_count(bt, target, true)) pr_note_unported(bt, a->cls);   /* the transfer isn't ported */
        return;
    }
    case PR_AC_EatBerryAttr: {                           /* Stuff Cheeks, Teatime */
        pr_pokemon *target_p = P(EatBerryAttr, a)->selfTarget ? user : target;
        if (!target_p) return;
        int side = target_p->is_player ? 0 : 1;
        int n_berries = 0;
        pr_modifier *berries[PR_MAX_MODIFIERS];
        for (int i = 0; i < bt->n_mods[side]; i++) {
            pr_modifier *m = &bt->mods[side][i];
            if (pr_mod_is(m, PR_MOD_BERRY) && m->pokemon_id == (int64_t)target_p->id) {
                berries[n_berries++] = m;
            }
        }
        if (n_berries > 0) {
            int idx = pr_battle_rand_seed_int(bt, n_berries, 0);
            pr_apply_berry(bt, e, target_p, berries[idx]);
        }
        return;
    }
    case PR_AC_StealEatBerryAttr: {                      /* Bug Bite, Pluck */
        bool cancelled = false;
        pr_ab_params cp = {.pokemon = target, .move = -1, .flag = &cancelled};
        pr_apply_ab_attrs(bt, PR_AC_BlockItemTheftAbAttr, &cp);
        /* any held item might be a berry: eating it isn't ported */
        if (!cancelled && pr_held_item_count(bt, target, false)) pr_note_unported(bt, a->cls);
        return;
    }
    case PR_AC_WeatherChangeAttr:                        /* Rain Dance, Sunny Day, Sandstorm, Hail, Snowscape, */
        pr_try_set_weather(bt, e, P(WeatherChangeAttr, a)->weatherType, user);   /* Chilly Reception */
        return;
    case PR_AC_TerrainChangeAttr:                        /* the four terrain moves: no animation of their own */
        pr_try_set_terrain(bt, e, P(TerrainChangeAttr, a)->terrainType, true, user);
        return;
    case PR_AC_AddSubstituteAttr: {                      /* Substitute, Shed Tail */
        /* the HP cost (Shed Tail rounds up), taken as indirect damage with no faint phase, then
         * addTag(SUBSTITUTE, 0, move, user.id) */
        if (!effect_can_apply(ef, user, target)) return;
        const pr_attr_AddSubstituteAttr *x = P(AddSubstituteAttr, a);
        double cost = pr_get_stat(user, STAT_HP, false) * x->hpCost;
        cost = x->roundUp ? ceil(cost) : (floor(cost) < 1 ? 1 : floor(cost));
        pr_damage_and_update(bt, e, user, cost, HIT_RESULT_INDIRECT, true, NULL);
        pr_add_tag(bt, e, user, BATTLER_TAG_TYPE_SUBSTITUTE, 0, move, user->id);
        return;
    }
    case PR_AC_RemoveAllSubstitutesAttr: {               /* Tidy Up */
        if (!effect_can_apply(ef, user, target)) return;
        static const int32_t sub[] = {BATTLER_TAG_TYPE_SUBSTITUTE};
        pr_speed_iter it;
        pr_pokemon *p;
        pr_speed_iter_init(bt, &it, ARENA_TAG_SIDE_BOTH);
        while ((p = pr_speed_iter_next(bt, &it))) pr_find_and_remove_tags(bt, e, p, sub, 1);
        return;
    }
    case PR_AC_RemoveBattlerTagAttr:                     /* Rapid Spin, Mortal Spin, Feint, Spit Up, Swallow */
        if (!effect_can_apply(ef, user, target)) return;
        pr_find_and_remove_tags(bt, e, ef->self_target ? user : target, P(RemoveBattlerTagAttr, a)->tagTypes.v,
                                P(RemoveBattlerTagAttr, a)->tagTypes.n);
        return;
    case PR_AC_RemoveArenaTagsAttr:                      /* Feint, Defog */
    case PR_AC_RemoveArenaTrapAttr:                      /* Rapid Spin, Mortal Spin, Defog, Tidy Up */
    case PR_AC_RemoveScreensAttr: {                      /* Brick Break, Psychic Fangs, Raging Bull, Defog */
        /* arena.removeTagsOnSide(tagTypes, getTagSideFunc(user, target)) */
        if (!effect_can_apply(ef, user, target)) return;
        const pr_intlist *types;
        pr_fn_id side_fn;
        if (a->cls == PR_AC_RemoveArenaTagsAttr) {
            types = &P(RemoveArenaTagsAttr, a)->tagTypes;
            side_fn = P(RemoveArenaTagsAttr, a)->getTagSideFunc;
        } else if (a->cls == PR_AC_RemoveArenaTrapAttr) {
            types = &P(RemoveArenaTrapAttr, a)->tagTypes;
            side_fn = P(RemoveArenaTrapAttr, a)->getTagSideFunc;
        } else {
            types = &P(RemoveScreensAttr, a)->tagTypes;
            side_fn = P(RemoveScreensAttr, a)->getTagSideFunc;
        }
        if (!target) {                                   /* the side closures may read the target */
            pr_note_unported(bt, a->cls);
            return;
        }
        int side = (int)pr_closure_call(bt, side_fn, a->cls, -1, user, target, move);
        if (side < 0) return;
        pr_arena_remove_tags_on_side(bt, e, types->v, types->n, side, false);
        return;
    }
    case PR_AC_AddArenaTagAttr:                          /* Reflect, Tailwind, Quick Guard, Gravity... */
    case PR_AC_AddArenaTrapTagAttr:                      /* Spikes, Toxic Spikes, Stealth Rock, Sticky Web */
    case PR_AC_AddPledgeEffectAttr: {                    /* the combined pledges' sea of fire, rainbow, swamp */
        const pr_attr_AddArenaTagAttr *x = (const pr_attr_AddArenaTagAttr *)a->p;
        const pr_turn_move *last = pr_last_move(bt, user, 0);
        if (a->cls == PR_AC_AddPledgeEffectAttr) {
            /* only when the last move succeeded and this pledge combined with the required one */
            if (!last || last->result != MOVE_RESULT_SUCCESS) return;
            if (user->combining_pledge != P(AddPledgeEffectAttr, a)->requiredPledge) return;
        }
        if (!effect_can_apply(ef, user, target)) return;
        /* the move's own chance (not getMoveChance), then the last move's result */
        double chance = pr_moves[move].chance;
        if (!chance_roll(bt, chance)) return;
        last = pr_last_move(bt, user, 0);
        if (!last || last->result != MOVE_RESULT_SUCCESS) return;
        if (!target) {
            pr_note_unported(bt, a->cls);
            return;
        }
        const pr_pokemon *on = x->selfSideTarget ? user : target;
        bool trap_on_user = pr_move_has_attr((uint16_t)move, PR_AC_AddArenaTrapTagAttr) && target == user;
        int side = on->is_player == trap_on_user ? ARENA_TAG_SIDE_ENEMY : ARENA_TAG_SIDE_PLAYER;
        pr_arena_add_tag(bt, e, x->tagType, x->turnCount, move, (int64_t)user->id, side, false);
        return;
    }
    case PR_AC_AddArenaTrapTagHitAttr: {                 /* Stone Axe, Ceaseless Edge: no canApply check */
        const pr_attr_AddArenaTrapTagHitAttr *x = P(AddArenaTrapTagHitAttr, a);
        double chance = get_move_chance(bt, ef, user, target, move, ef->self_target, true);
        const pr_pokemon *on = x->selfSideTarget ? user : target;
        int side = on->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY;
        if (!chance_roll(bt, chance)) return;
        const pr_turn_move *last = pr_last_move(bt, user, 0);
        if (!last || last->result != MOVE_RESULT_SUCCESS) return;
        pr_arena_add_tag(bt, e, x->tagType, 0, move, (int64_t)user->id, side, false);
        return;
    }
    case PR_AC_SwapArenaTagsAttr: {                      /* Court Change */
        if (!effect_can_apply(ef, user, target)) return;
        const pr_intlist *valid = &P(SwapArenaTagsAttr, a)->validTags;
        /* findTagsOnSide for each side (a BOTH-sided tag is found on both), then every one
         * removed quietly by type from its side, then re-added on the other side as many
         * times as it had layers, quietly (each extra layer's onAdd isn't) */
        pr_arena_tag found[2][PR_MAX_ARENA_TAGS];
        int nfound[2] = {0, 0};
        const int sides[2] = {ARENA_TAG_SIDE_PLAYER, ARENA_TAG_SIDE_ENEMY};
        for (int s = 0; s < 2; s++)
            for (int i = 0; i < bt->n_arena_tags; i++) {
                const pr_arena_tag *t = &bt->arena_tags[i];
                bool ok = false;
                for (int k = 0; k < valid->n; k++)
                    if (valid->v[k] == t->type) ok = true;
                if (ok && pr_arena_tag_on_side(t, sides[s])) found[s][nfound[s]++] = *t;
            }
        for (int s = 0; s < 2; s++)
            for (int i = 0; i < nfound[s]; i++) pr_arena_remove_tag_on_side(bt, e, found[s][i].type, sides[s], true);
        for (int s = 0; s < 2; s++)
            for (int i = 0; i < nfound[s]; i++) {
                const pr_arena_tag *t = &found[s][i];
                for (int l = 0; l < t->layers; l++)
                    pr_arena_add_tag(bt, e, t->type, t->turn_count, t->source_move, t->source_id, sides[1 - s], true);
            }
        pr_queue_msg(bt, e, pr_msg1("moveTriggers:swapArenaTags", pr_arg_pokemon(bt, "pokemonName", user)));
        return;
    }
    case PR_AC_SacrificialAttr:                          /* Explosion, Self-Destruct, Misty Explosion */
    case PR_AC_SacrificialAttrOnHit: {                   /* Memento, Final Gambit: only if the move hit */
        if (a->cls == PR_AC_SacrificialAttrOnHit && !effect_can_apply(ef, user, target)) return;
        /* damageAndUpdate(user.hp, {result: INDIRECT, ignoreSegments: true}), then damageTaken +=
         * user.hp: the HP left afterwards, 0 unless the user got through */
        pr_damage_and_update(bt, e, user, user->hp, HIT_RESULT_INDIRECT, false, NULL);
        user->damage_taken += user->hp;
        return;
    }
    case PR_AC_HalfSacrificialAttr: {                    /* Mind Blown, Steel Beam */
        if (!effect_can_apply(ef, user, target)) return;
        bool cancelled = false;
        pr_ab_params ap = {.pokemon = user, .move = -1, .flag = &cancelled};
        pr_apply_ab_attrs(bt, PR_AC_BlockNonDirectDamageAbAttr, &ap);
        if (cancelled) return;
        double d = floor(pr_get_stat(user, STAT_HP, false) / 2.0);
        pr_damage_and_update(bt, e, user, d < 1 ? 1 : d, HIT_RESULT_INDIRECT, false, NULL);
        pr_queue_msg(bt, e, pr_msg1("moveTriggers:cutHpPowerUpMove", pr_arg_pokemon(bt, "pokemonName", user)));
        return;
    }
    case PR_AC_ResetStatsAttr:                           /* Haze, Freezy Frost (everyone active); Clear Smog */
        /* no canApply check: a target that fainted to the hit is reset too */
        if (P(ResetStatsAttr, a)->targetAllPokemon) {
            for (int i = 0; i < 4; i++)
                if (bt->field[i] && bt->field[i]->hp > 0) memset(bt->field[i]->stat_stages, 0, sizeof bt->field[i]->stat_stages);
        } else {
            memset(target->stat_stages, 0, sizeof target->stat_stages);
        }
        pr_queue_msg(bt, e, P(ResetStatsAttr, a)->targetAllPokemon ? pr_msg0("moveTriggers:statEliminated")
                            : pr_msg1("moveTriggers:resetStats", pr_arg_pokemon(bt, "pokemonName", target)));
        return;
    case PR_AC_AverageStatsAttr: {
        if (!effect_can_apply(ef, user, target)) return;
        const pr_attr_AverageStatsAttr *x = P(AverageStatsAttr, a);
        for (int k = 0; k < x->stats.n; k++) {
            int stat = x->stats.v[k];
            double average = floor((pr_get_stat(user, stat, false)
                                  + pr_get_stat(target, stat, false)) / 2.0);
            /* Pokemon.setStat(..., false) ignores nonpositive values. */
            if (average > 0) user->summon_stats[stat] = target->summon_stats[stat] = average;
        }
        pr_queue_msg(bt, e, pr_msg1(x->msgKey, pr_arg_pokemon(bt, "pokemonName", user)));
        return;
    }
    case PR_AC_CopyStatsAttr: {
        /* CopyStatsAttr: every battle stat stage straight onto the user, no StatStageChangePhase;
         * CRIT_BOOST presence follows the target's (an existing one is only overlapped) */
        if (!effect_can_apply(ef, user, target)) return;
        for (int s = 0; s < 7; s++) {
            int v = target->stat_stages[s];
            user->stat_stages[s] = (int8_t)(v < -6 ? -6 : v > 6 ? 6 : v);
        }
        if (pr_get_battler_tag(target, BATTLER_TAG_TYPE_CRIT_BOOST))
            pr_add_tag(bt, e, user, BATTLER_TAG_TYPE_CRIT_BOOST, 0, move, -1);
        else
            pr_remove_tag(bt, e, user, BATTLER_TAG_TYPE_CRIT_BOOST);
        pr_queue_msg(bt, e, pr_msg2("moveTriggers:copiedStatChanges",
                                    pr_arg_pokemon(bt, "pokemonName", user),
                                    pr_arg_pokemon(bt, "targetName", target)));
        return;
    }
    case PR_AC_SwapStatStagesAttr: {                     /* Heart Swap, Power Swap, Guard Swap */
        if (!effect_can_apply(ef, user, target)) return;
        const pr_intlist *st = &P(SwapStatStagesAttr, a)->stats;
        for (int k = 0; k < st->n; k++) {
            int8_t tmp = user->stat_stages[st->v[k] - 1];
            user->stat_stages[st->v[k] - 1] = target->stat_stages[st->v[k] - 1];
            target->stat_stages[st->v[k] - 1] = tmp;
        }
        if (st->n == 7)
            pr_queue_msg(bt, e, pr_msg1("moveTriggers:switchedStatChanges", pr_arg_pokemon(bt, "pokemonName", user)));
        else if (st->n == 2)
            pr_queue_msg(bt, e, pr_msg3("moveTriggers:switchedTwoStatChanges", pr_arg_pokemon(bt, "pokemonName", user),
                                        pr_arg_stat("firstStat", st->v[0]), pr_arg_stat("secondStat", st->v[1])));
        return;
    }
    case PR_AC_CopyBiomeTypeAttr: {
        if (!effect_can_apply(ef, user, target)) return;
        int8_t one = POKEMON_TYPE_UNKNOWN;
        if (bt->terrain != TERRAIN_NONE) {
            switch (bt->terrain) {
            case TERRAIN_ELECTRIC: one = POKEMON_TYPE_ELECTRIC; break;
            case TERRAIN_MISTY: one = POKEMON_TYPE_FAIRY; break;
            case TERRAIN_GRASSY: one = POKEMON_TYPE_GRASS; break;
            case TERRAIN_PSYCHIC: one = POKEMON_TYPE_PSYCHIC; break;
            default: break;
            }
        } else if (bt->has_biome) {
            switch (bt->biome) {
            case BIOME_ID_TOWN: case BIOME_ID_PLAINS: case BIOME_ID_METROPOLIS: one = POKEMON_TYPE_NORMAL; break;
            case BIOME_ID_GRASS: case BIOME_ID_TALL_GRASS: one = POKEMON_TYPE_GRASS; break;
            case BIOME_ID_FOREST: case BIOME_ID_JUNGLE: one = POKEMON_TYPE_BUG; break;
            case BIOME_ID_SLUM: case BIOME_ID_SWAMP: one = POKEMON_TYPE_POISON; break;
            case BIOME_ID_SEA: case BIOME_ID_BEACH: case BIOME_ID_LAKE: case BIOME_ID_SEABED: one = POKEMON_TYPE_WATER; break;
            case BIOME_ID_MOUNTAIN: one = POKEMON_TYPE_FLYING; break;
            case BIOME_ID_BADLANDS: one = POKEMON_TYPE_GROUND; break;
            case BIOME_ID_CAVE: case BIOME_ID_DESERT: one = POKEMON_TYPE_ROCK; break;
            case BIOME_ID_ICE_CAVE: case BIOME_ID_SNOWY_FOREST: one = POKEMON_TYPE_ICE; break;
            case BIOME_ID_MEADOW: case BIOME_ID_FAIRY_CAVE: case BIOME_ID_ISLAND: one = POKEMON_TYPE_FAIRY; break;
            case BIOME_ID_POWER_PLANT: one = POKEMON_TYPE_ELECTRIC; break;
            case BIOME_ID_VOLCANO: one = POKEMON_TYPE_FIRE; break;
            case BIOME_ID_GRAVEYARD: case BIOME_ID_TEMPLE: one = POKEMON_TYPE_GHOST; break;
            case BIOME_ID_DOJO: case BIOME_ID_CONSTRUCTION_SITE: one = POKEMON_TYPE_FIGHTING; break;
            case BIOME_ID_FACTORY: case BIOME_ID_LABORATORY: one = POKEMON_TYPE_STEEL; break;
            case BIOME_ID_RUINS: case BIOME_ID_SPACE: one = POKEMON_TYPE_PSYCHIC; break;
            case BIOME_ID_WASTELAND: case BIOME_ID_END: one = POKEMON_TYPE_DRAGON; break;
            case BIOME_ID_ABYSS: one = POKEMON_TYPE_DARK; break;
            default: break;
            }
        }
        pr_set_summon_types(bt, user, &one, 1);
        pr_queue_msg(bt, e, pr_msg2("moveTriggers:transformedIntoType",
                                  pr_arg_pokemon(bt, "pokemonName", user), pr_arg_type("typeName", one)));
        return;
    }
    case PR_AC_ChangeTypeAttr: {                         /* Soak, Magic Powder: no canApply check */
        int8_t one = (int8_t)P(ChangeTypeAttr, a)->type;   /* summonData.types = [type] */
        pr_set_summon_types(bt, target, &one, 1);
        pr_queue_msg(bt, e, pr_msg2("moveTriggers:transformedIntoType", pr_arg_pokemon(bt, "pokemonName", target),
                                    pr_arg_type("typeName", one)));
        return;
    }
    case PR_AC_RemoveTypeAttr: {                         /* Burn Up, Double Shock (POST_TARGET, the user) */
        /* not the user's active Tera type; else summonData.types = getTypes() without the type
         * ([UNKNOWN] when none is left), then the callback's message */
        const pr_attr_RemoveTypeAttr *x = P(RemoveTypeAttr, a);
        if (!effect_can_apply(ef, user, target)) return;
        if (user->terastallized && pr_get_tera_type(user) == x->removedType) return;
        int8_t ty[PR_MAX_TYPES], keep[PR_MAX_TYPES];
        int n = pr_get_types(user, ty, 0), k = 0;
        for (int i = 0; i < n; i++)
            if (ty[i] != x->removedType) keep[k++] = ty[i];
        if (!k) keep[k++] = POKEMON_TYPE_UNKNOWN;
        pr_set_summon_types(bt, user, keep, k);
        if (x->messageCallback != PR_NO_FN) pr_closure_call(bt, x->messageCallback, a->cls, 0, user, target, move);
        return;
    }
    case PR_AC_CueNextRoundAttr: {                       /* Round (last hit only); no canApply check */
        /* the next Round in the queue moves next, and its user has joined the round */
        pr_phase *next = pr_find_move_phase(e, MOVE_ID_ROUND, -1);
        if (!next) return;
        next->timing = MOVE_PHASE_TIMING_MODIFIER_FIRST;  /* forceMoveNext(phase.move.moveId === ROUND) */
        next->pokemon->joined_round = true;
        return;
    }
    case PR_AC_RageAttr:                                 /* PRE_APPLY; no canApply check */
        pr_queue_msg(bt, e, pr_msg1("moveTriggers:rageIsBuilding", pr_arg_pokemon(bt, "pokemonName", user)));
        pr_add_tag(bt, e, user, BATTLER_TAG_TYPE_RAGE, 0, move, (int64_t)user->id);
        return;
    case PR_AC_MessageAttr:                              /* Splash, Celebrate, Lock-On, Perish Song...: */
        switch (move) {                                  /* every message upstream builds is non-empty */
        case MOVE_ID_SPLASH: pr_queue_msg(bt, e, pr_msg0("moveTriggers:splash")); break;
        case MOVE_ID_MIND_READER: case MOVE_ID_LOCK_ON:
            pr_queue_msg(bt, e, pr_msg2("moveTriggers:tookAimAtTarget", pr_arg_pokemon(bt, "pokemonName", user),
                                        pr_arg_pokemon(bt, "targetName", target)));
            break;
        case MOVE_ID_PERISH_SONG:
            pr_queue_msg(bt, e, pr_msg2("moveTriggers:faintCountdown", pr_arg_pokemon(bt, "pokemonName", target),
                                        pr_arg_int("turnCount", 3)));
            break;
        case MOVE_ID_CELEBRATE: {
            pr_msg_arg name = {"playerName", PR_ARG_STR, 0, bt->player_name ? bt->player_name : ""};
            pr_queue_msg(bt, e, pr_msg1("moveTriggers:celebrate", name));
            break;
        }
        case MOVE_ID_LASER_FOCUS: pr_queue_msg(bt, e, pr_msg_p(bt, "battlerTags:laserFocusOnAdd", user)); break;
        default: pr_queue_message(bt, e);
        }
        return;
    case PR_AC_RestAttr: {                               /* Rest */
        /* user.trySetStatus(SLEEP, user, duration, null, overrideStatus, quiet) first; then
         * HealAttr.apply, whose addHealPhase Rest overrides: PokemonHealPhase(maxHp), no options */
        pr_msg rest = pr_msg1("moveTriggers:restBecameHealthy", pr_arg_pokemon(bt, "pokemonName", user));
        if (!pr_try_set_status_msg(bt, e, user, STATUS_EFFECT_SLEEP, user, P(RestAttr, a)->duration, true, true, &rest))
            return;
        if (!effect_can_apply(ef, user, target)) return;
        double ratio = P(RestAttr, a)->healRatio;
        pr_ab_params hp = {.pokemon = user, .opponent = target, .move = move, .value = &ratio};
        pr_apply_ab_attrs(bt, PR_AC_MoveHealBoostAbAttr, &hp);
        pr_unshift_heal(bt, e, pr_battler_index(bt, user), pr_get_stat(user, STAT_HP, false), 0);
        return;
    }
    case PR_AC_DestinyBondAttr:                          /* PRE_APPLY; no canApply check */
        pr_queue_msg(bt, e, pr_msg1("moveTriggers:tryingToTakeFoeDown", pr_arg_pokemon(bt, "pokemonName", user)));
        pr_add_tag(bt, e, user, BATTLER_TAG_TYPE_DESTINY_BOND, 0, move, (int64_t)user->id);
        return;
    case PR_AC_CurseAttr: {                              /* Curse: no canApply check */
        /* a Ghost user (user.getTypes()) curses the target for half its own max HP (the attack
         * fails on one already cursed); any other user: Attack and Defense +1, then Speed -1 */
        int8_t ty[PR_MAX_TYPES];
        int n = pr_get_types(user, ty, 0);
        bool ghost = false;
        for (int i = 0; i < n; i++) ghost |= ty[i] == POKEMON_TYPE_GHOST;
        if (ghost) {
            if (pr_get_battler_tag(target, BATTLER_TAG_TYPE_CURSED)) {
                pr_queue_msg(bt, e, pr_msg0("battle:attackFailed"));
                return;
            }
            double d = floor(pr_get_stat(user, STAT_HP, false) / 2.0);
            pr_damage_and_update(bt, e, user, d < 1 ? 1 : d, HIT_RESULT_INDIRECT, false, NULL);
            pr_queue_msg(bt, e, pr_msg2("battlerTags:cursedOnAdd", pr_arg_pokemon(bt, "pokemonNameWithAffix", user),
                                        pr_arg_pokemon(bt, "pokemonName", target)));
            pr_add_tag(bt, e, target, BATTLER_TAG_TYPE_CURSED, 0, move, (int64_t)user->id);
            return;
        }
        static const int32_t up[2] = {STAT_ATK, STAT_DEF}, down[1] = {STAT_SPD};
        if (pr_unshift_stat_stage_change(bt, e, pr_battler_index(bt, user), up, 2, 1, user) < 0) e->overflow = true;
        if (pr_unshift_stat_stage_change(bt, e, pr_battler_index(bt, user), down, 1, -1, user) < 0) e->overflow = true;
        return;
    }
    case PR_AC_PsychoShiftEffectAttr: {                  /* Psycho Shift: no canApply gate */
        /* the user's status (Comatose: sleep) goes to the target, and a user with a status of its
         * own is healed of it after its move (PsychoShiftTag) */
        int st = user->status ? user->status
                 : pr_has_ability_active(bt, user, ABILITY_ID_COMATOSE) ? STATUS_EFFECT_SLEEP : STATUS_EFFECT_NONE;
        if (!pr_try_set_status(bt, e, target, st, user, -1, false, true)) return;
        if (user->status) pr_add_tag(bt, e, user, BATTLER_TAG_TYPE_PSYCHO_SHIFT, 0, move, -1);
        return;
    }
    case PR_AC_HealAttr:                                 /* Recover, Soft-Boiled, Roost, Heal Pulse... */
    case PR_AC_VariableHealAttr: {                       /* Moonlight, Swallow, Shore Up...: healRatio = healFunc(user) first */
        bool variable = a->cls == PR_AC_VariableHealAttr;
        const pr_attr_HealAttr *x = P(HealAttr, a);
        const pr_attr_VariableHealAttr *v = P(VariableHealAttr, a);
        double ratio = variable ? pr_closure_call(bt, v->healFunc, a->cls, 0, user, target, move) : x->healRatio;
        bool self_target = variable ? v->selfTarget : x->selfTarget, show_anim = variable ? v->showAnim : x->showAnim;
        if (!effect_can_apply(ef, user, target)) return;
        pr_ab_params hp = {.pokemon = user, .opponent = target, .move = move, .value = &ratio};
        pr_apply_ab_attrs(bt, PR_AC_MoveHealBoostAbAttr, &hp);   /* Mega Launcher's Heal Pulse */
        pr_pokemon *on = self_target ? user : target;
        /* PokemonHealPhase(Math.round(maxHp * ratio), {message, showFullHpMessage, skipAnim: !showAnim}):
         * a move's heal rounds half up */
        pr_unshift_heal_msg(bt, e, pr_battler_index(bt, on), js_round(pr_get_stat(on, STAT_HP, false) * ratio),
                            PR_HEAL_MESSAGE | (show_anim ? 0 : PR_HEAL_SKIP_ANIM),
                            pr_msg1("moveTriggers:healHp", pr_arg_pokemon(bt, "pokemonName", on)));
        return;
    }
    }
    pr_note_unported(bt, a->cls);
}

/* Bounded removal family: supported named battle items and all berry identities.
 * Unsupported item names remain explicit rather than producing a false message. */
static const char *removed_item_key(const pr_modifier *m)
{
    static const char *const berries[] = {
        "berry:sitrus.name", "berry:lum.name", "berry:enigma.name", "berry:liechi.name",
        "berry:ganlon.name", "berry:petaya.name", "berry:apicot.name", "berry:salac.name",
        "berry:lansat.name", "berry:starf.name", "berry:leppa.name"
    };
    if (m->unknown_subclass) return NULL;
    if (m->cls == PR_MOD_BERRY)
        return m->berry_type >= 0 && m->berry_type < 11 ? berries[m->berry_type] : NULL;
    switch (m->cls) {
    case PR_MOD_TURN_HEAL: return "modifierType:ModifierType.LEFTOVERS.name";
    case PR_MOD_HIT_HEAL: return "modifierType:ModifierType.SHELL_BELL.name";
    case PR_MOD_SURVIVE_DAMAGE: return "modifierType:ModifierType.FOCUS_BAND.name";
    case PR_MOD_BYPASS_SPEED_CHANCE: return "modifierType:ModifierType.QUICK_CLAW.name";
    case PR_MOD_FLINCH_CHANCE: return "modifierType:ModifierType.KINGS_ROCK.name";
    case PR_MOD_RESET_NEGATIVE_STAT_STAGE: return "modifierType:ModifierType.WHITE_HERB.name";
    default: return NULL;
    }
}

bool pr_remove_move_held_item(pr_battle *bt, pr_engine *e, pr_pokemon *user,
                             pr_pokemon *target, bool berries_only, pr_modifier *removed)
{
    if (!berries_only && target->is_player) return false;
    bool cancelled = false;
    pr_ab_params cp = {.pokemon = target, .move = -1, .flag = &cancelled};
    pr_apply_ab_attrs(bt, PR_AC_BlockItemTheftAbAttr, &cp);
    if (cancelled) return false;
    int side = target->is_player ? 0 : 1, indices[PR_MAX_MODIFIERS], n = 0;
    for (int k = 0; k < bt->n_mods[side]; k++) {
        const pr_modifier *m = &bt->mods[side][k];
        if (m->pokemon_id == (int64_t)target->id && m->is_transferable
            && pr_mod_is(m, PR_MOD_HELD_ITEM) && (!berries_only || pr_mod_is(m, PR_MOD_BERRY)))
            indices[n++] = k;
    }
    if (!n) return false;
    int selected = pr_battle_rand_seed_int(bt, n, 0);
    pr_modifier *m = &bt->mods[side][indices[selected]];
    const char *key = removed_item_key(m);
    if (!key) { pr_note_unported(bt, PR_AC_RemoveHeldItemAttr); return false; }
    if (removed) *removed = *m;
    pr_lose_held_item(bt, e, target, m);
    pr_queue_msg(bt, e, pr_msg3(berries_only ? "moveTriggers:incineratedItem" : "moveTriggers:knockedOffItem",
        pr_arg_pokemon(bt, "pokemonName", user), pr_arg_pokemon(bt, "targetName", target),
        pr_arg_key("itemName", key)));
    return true;
}
