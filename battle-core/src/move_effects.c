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
    if (PR_MAX_PP(p)[s] > 0) return PR_MAX_PP(p)[s];
    int pp = pr_moves[PR_MOVES(p)[s]].pp;
    return pp + PR_PP_UP(p)[s] * (int)to_dmg_value(pp / 5.0);
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

/* StatusEffectAttr omits the parent gate: its chance rolls even on a fainted
 * target. Secret Power supplies its own outer HP gate before using this child. */
static bool status_effect_apply(pr_battle *bt, pr_engine *e, const pr_effect_info *ef,
                                pr_pokemon *user, pr_pokemon *target, int move, int effect)
{
    double chance = get_move_chance(bt, ef, user, target, move, ef->self_target, true);
    if (!chance_roll(bt, chance)) return false;
    return pr_try_set_status(bt, e, target, effect, user, -1, false,
                             pr_moves[move].category != MOVE_CATEGORY_STATUS);
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
 * sourcePokemon: user, message: options.message}); Belly Drum supplies a message.
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
    int ph = pr_unshift_stat_stage_change(bt, e, pr_battler_index(bt, on), stats, n, stages, user);
    if (ph < 0) e->overflow = true;
    else if (cls == PR_AC_CutHpStatStageBoostAttr && move == MOVE_ID_BELLY_DRUM) {
        /* The pin's sole custom StatStageChangeAttr message is evaluated when
         * the phase runs, using its current recipient rather than a frozen name. */
        e->pool[ph].ssc_message = true;
        e->pool[ph].move_id = MOVE_ID_BELLY_DRUM;
    }
    return true;
}

/* FaintPhase's generated callback creates a fresh StatStageChangeAttr. */
void pr_apply_post_victory_stat_change(pr_battle *bt, pr_engine *e, int attr, pr_pokemon *source, int move)
{
    const pr_attr_PostVictoryStatStageChangeAttr *x = P(PostVictoryStatStageChangeAttr, &pr_attrs[attr]);
    /* The only pinned factory has no condition; its generated field has no closure API. */
    if (!(x->nulls & 0x8u)) { pr_note_unported(bt, PR_AC_PostVictoryStatStageChangeAttr); return; }
    pr_effect_info fresh = {.self_target=x->showMessage, .chance_override=-1, .condition=PR_NO_FN};
    stat_stage_change(bt,e,&fresh,PR_AC_StatStageChangeAttr,source,source,move,x->stats.v,x->stats.n,x->stages);
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

/* EatBerryAttr/StealEatBerryAttr have different loss/refresh/history order
 * from the ordinary BerryModifier caller. Keep their native callback chain. */
bool pr_apply_eat_berry_move(pr_battle *bt,pr_engine *e,pr_pokemon *user,
                            pr_pokemon *target,bool self_target,bool steal)
{
    if(!user||(!self_target&&!target)){pr_note_unported(bt,PR_FEAT_PARTY);return false;}
    pr_pokemon *owner=self_target?user:target;
    if(!steal&&owner->hp<=0)return false;
    if(!target){pr_note_unported(bt,PR_FEAT_PARTY);return false;}
    if(steal){
        bool cancelled=false;pr_ab_params block={.pokemon=target,.move=-1,.flag=&cancelled};
        pr_apply_ab_attrs(bt,PR_AC_BlockItemTheftAbAttr,&block);
        if(cancelled)return false;
    }
    int side=owner->is_player?0:1,indices[PR_MAX_MODIFIERS],count=0;
    for(int k=0;k<bt->n_mods[side];k++){
        pr_modifier *m=&bt->mods[side][k];
        if(pr_mod_is(m,PR_MOD_BERRY)&&m->pokemon_id==(int64_t)owner->id)indices[count++]=k;
    }
    if(!count)return false;
    int index=indices[pr_battle_rand_seed_int(bt,count,0)];
    pr_modifier chosen=bt->mods[side][index];
    if(chosen.unknown_subclass||chosen.berry_type<0||chosen.berry_type>BERRY_TYPE_LEPPA){
        pr_note_unported(bt,PR_FEAT_MODIFIERS);return false;
    }
    bool preserve=false;
    if(steal){
        pr_ab_params lost={.pokemon=target,.move=-1};
        pr_apply_ab_attrs(bt,PR_AC_PostItemLostAbAttr,&lost);
        const char *name=pr_held_item_name_key(&chosen);
        if(!name){pr_note_unported(bt,PR_FEAT_MODIFIERS);return false;}
        pr_queue_msg(bt,e,pr_msg3("battle:stealEatBerry",pr_arg_pokemon_name(bt,"pokemonName",user),
            pr_arg_pokemon_name(bt,"targetName",target),pr_arg_key("berryName",name)));
    }else{
        for(int k=0;k<bt->n_mods[side];k++){
            const pr_modifier *pouch=&bt->mods[side][k];
            if(pouch->cls!=PR_MOD_PRESERVE_BERRY)continue;
            if(!pr_mod_is_ported_item(pouch)){pr_note_unported(bt,PR_FEAT_TURN_MODIFIERS);continue;}
            if(!preserve)preserve=pr_battle_rand_seed_int(bt,10,0)<pr_mod_get_stack_count(pouch)*3;
        }
    }
    if(!preserve){pr_lose_held_item(bt,e,owner,&bt->mods[side][index]);pr_refresh_berry_modifiers(bt,side);}
    pr_pokemon *consumer=steal?user:target;
    if(chosen.berry_type==BERRY_TYPE_STARF&&!bt->rng_live){pr_note_unported(bt,PR_FEAT_GLOBAL_RNG);return false;}
    pr_berry_effect(bt,e,consumer,chosen.berry_type,NULL,NULL,NULL);
    pr_pokemon *callback_owner=steal?target:consumer;
    pr_ab_params callback={.pokemon=callback_owner,.move=-1};
    pr_apply_ab_attrs(bt,PR_AC_PostItemLostAbAttr,&callback);
    callback.pokemon=consumer;pr_apply_ab_attrs(bt,PR_AC_HealFromBerryUseAbAttr,&callback);
    pr_record_eaten_berry(bt,consumer,chosen.berry_type,steal?consumer==target:!preserve);
    return true;
}

/* applyMoveAttrs(StatChangeBeforeDmgCalcAttr): target stages change now; the
 * user's requested gains remain queued until the current damage phase ends. */
void pr_apply_pre_damage_stat_changes(pr_battle *bt, pr_engine *e, pr_pokemon *user, pr_pokemon *target, int move)
{
    const pr_move *mv = &pr_moves[move];
    for (int i = 0; i < mv->nattr; i++) {
        int cls = pr_attrs[mv->attr0 + i].cls;
        if (!pr_attr_is(cls, PR_AC_StatChangeBeforeDmgCalcAttr)) continue;
        if (cls != PR_AC_SpectralThiefAttr) { pr_note_unported(bt, cls); continue; }
        /* The override has no parent HP, condition or chance gates. */
        for (int stat = STAT_ATK; stat <= STAT_EVA; stat++) {
            int target_value = target->stat_stages[stat - 1];
            int user_value = user->stat_stages[stat - 1];
            if (target_value > 0) {
                int available = target_value < 6 - user_value ? target_value : 6 - user_value;
                int32_t stats[] = {stat};
                pr_unshift_stat_stage_change(bt, e, pr_battler_index(bt, user), stats, 1, available, user);
                target->stat_stages[stat - 1] = (int8_t)(target_value - available);
            }
        }
        pr_queue_msg(bt, e, pr_msg2("moveTriggers:stealPositiveStats",
            pr_arg_pokemon(bt, "pokemonName", user), pr_arg_pokemon(bt, "targetName", target)));
    }
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
    case PR_AC_HpSplitAttr: {
        if (!effect_can_apply(ef, user, target)) return;
        double hp_value = floor(((double)target->hp + user->hp) / 2);
        pr_pokemon *actors[] = {user, target};
        for (int i = 0; i < 2; i++) {
            pr_pokemon *p = actors[i];
            if (p->hp < hp_value) {
                double healing = hp_value - p->hp;
                double room = pr_get_stat(p, STAT_HP, true) - p->hp;
                healing = healing < room ? healing : room;
                p->hp += (int32_t)healing;
                if (healing) pr_event_add(bt, e, PR_EV_HP, pr_battler_index(bt, p), -1, 0, (int32_t)healing, NULL);
            } else if (p->hp > hp_value) {
                double damage = pr_pokemon_damage(bt, e, p, p->hp - hp_value, true, false, false);
                if (damage) pr_event_add(bt, e, PR_EV_HP, pr_battler_index(bt, p), -1, 0, -(int32_t)damage, NULL);
            }
        }
        return;
    }
    case PR_AC_AbilityChangeAttr: {
        if (!effect_can_apply(ef, user, target)) return;
        const pr_attr_AbilityChangeAttr *x = P(AbilityChangeAttr, a);
        pr_pokemon *on = x->selfTarget ? user : target;
        pr_form_change(bt, on, PR_FCT_SpeciesFormChangeRevertWeatherFormTrigger);
        if (pr_break_illusion(on))
            pr_queue_msg(bt, e, pr_msg1("abilityTriggers:illusionBreak", pr_arg_pokemon(bt, "pokemonName", on)));
        pr_queue_msg(bt, e, pr_msg2("moveTriggers:acquiredAbility",
            pr_arg_pokemon(bt, "pokemonName", on), pr_arg_ability("abilityName", x->ability)));
        pr_set_temp_ability(bt, on, x->ability);
        pr_form_change(bt, on, PR_FCT_SpeciesFormChangeRevertWeatherFormTrigger);
        return;
    }
    case PR_AC_AbilityCopyAttr: {
        if (!effect_can_apply(ef, user, target)) return;
        pr_queue_msg(bt, e, pr_msg3("moveTriggers:copiedTargetAbility",
            pr_arg_pokemon(bt, "pokemonName", user), pr_arg_pokemon(bt, "targetName", target),
            pr_arg_ability("abilityName", target->ability)));
        pr_set_temp_ability(bt, user, target->ability);
        pr_pokemon *ally = (pr_pokemon *)pr_get_ally(bt, user);
        if (P(AbilityCopyAttr, a)->copyToPartner && bt->double_battle && ally && ally->hp) {
            pr_queue_msg(bt, e, pr_msg3("moveTriggers:copiedTargetAbility",
                pr_arg_pokemon(bt, "pokemonName", ally), pr_arg_pokemon(bt, "targetName", target),
                pr_arg_ability("abilityName", target->ability)));
            pr_set_temp_ability(bt, ally, target->ability);
        }
        return;
    }
    case PR_AC_AbilityGiveAttr:
        if (!effect_can_apply(ef, user, target)) return;
        pr_queue_msg(bt, e, pr_msg2("moveTriggers:acquiredAbility",
            pr_arg_pokemon(bt, "pokemonName", target), pr_arg_ability("abilityName", user->ability)));
        pr_set_temp_ability(bt, target, user->ability);
        return;
    case PR_AC_SwitchAbilitiesAttr: {
        if (!effect_can_apply(ef, user, target)) return;
        uint16_t old_ability = user->ability;
        pr_queue_msg(bt, e, pr_msg1("moveTriggers:swappedAbilitiesWithTarget", pr_arg_pokemon(bt, "pokemonName", user)));
        pr_set_temp_ability(bt, user, target->ability);
        pr_set_temp_ability(bt, target, old_ability);
        pr_weather_form_changes(bt, true, NULL);
        return;
    }
    case PR_AC_SuppressAbilitiesAttr:
    case PR_AC_SuppressAbilitiesIfActedAttr: {
        if (!effect_can_apply(ef, user, target)) return;
        /* The raw base effect does not call getCondition. Core Enforcer's
         * override alone tests acted and a fresh base attribute's condition. */
        if (a->cls == PR_AC_SuppressAbilitiesIfActedAttr
            && (!target->acted || !pr_can_suppress_ability(target))) return;
        pr_queue_msg(bt, e, pr_msg1("moveTriggers:suppressAbilities", pr_arg_pokemon(bt, "pokemonName", target)));
        /* Pokemon.suppressAbility: lose passive first, then primary, then set
         * suppression. IllusionBreak has no enabled registrations at the pin. */
        pr_ab_params lose = {.pokemon=target, .move=-1, .only=2};
        pr_apply_ab_attrs(bt, PR_AC_PreLeaveFieldAbAttr, &lose);
        lose.only = 1;
        pr_apply_ab_attrs(bt, PR_AC_PreLeaveFieldAbAttr, &lose);
        target->ability_suppressed = true;
        pr_weather_form_changes(bt, true, NULL);
        return;
    }
    case PR_AC_AfterYouAttr:
        /* This override always queues its message, then marks the first
         * queued MovePhase whose cached Pokemon is the target. */
        pr_queue_msg(bt, e, pr_msg1("moveTriggers:afterYou", pr_arg_pokemon(bt, "targetName", target)));
        pr_force_move_next(e, target);
        return;
    case PR_AC_ForceLastAttr:
        /* Native override omits parent HP/condition/chance gates. */
        pr_queue_msg(bt, e, pr_msg1("moveTriggers:forceLast", pr_arg_pokemon(bt, "targetPokemonName", target)));
        pr_force_move_last(e, target);
        return;
    case PR_AC_SketchAttr:
        if (effect_can_apply(ef,user,target)) pr_copy_last_move(bt,e,user,target,move,true);
        return;
    case PR_AC_TransformAttr: {
        if (!effect_can_apply(ef, user, target)) return;
        int transform = pr_phase_new(e, PH_TRANSFORM, (int8_t)pr_battler_index(bt, user));
        if (transform < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return; }
        e->pool[transform].transform_target = (int8_t)pr_battler_index(bt, target);
        pr_phase_unshift(bt, e, transform);
        return;
    }
    case PR_AC_PartyStatusCureAttr: {
        const pr_attr_PartyStatusCureAttr *x = P(PartyStatusCureAttr, a);
        int side = user->is_player ? 0 : 1;
        if (bt->n_party[side] < 1) { pr_note_unported(bt, a->cls); return; }
        for (int k = 0; k < bt->n_party[side]; k++) {
            pr_pokemon *p = bt->party[side][k];
            if (!p) { pr_note_unported(bt, a->cls); return; }
        }
        /* Custom canApply deliberately omits target HP. */
        if (ef->self_target) {
            if (user->hp <= 0 || pr_get_battler_tag(user, BATTLER_TAG_TYPE_FRENZY)) return;
        } else if (target && pr_get_battler_tag(target, BATTLER_TAG_TYPE_PROTECTED)
                   && !(pr_moves[move].flags & MOVE_FLAGS_IGNORE_PROTECT)) return;
        for (int k = 0; k < bt->n_party[side]; k++) {
            pr_pokemon *p = bt->party[side][k];
            if (!p->on_field || p->switch_out_status || p->id == user->id) {
                pr_reset_status_phase(bt, e, p, false);
            } else if (pr_has_ability_active(bt, p, x->abilityCondition)) {
                bool passive = pr_get_passive_ability(p) == x->abilityCondition;
                pr_queue_ability_display(bt, e, p, passive, true);
                pr_queue_ability_display(bt, e, p, passive, false);
            } else pr_reset_status_phase(bt, e, p, true);
        }
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
    case PR_AC_AcupressureStatStageChangeAttr: {
        /* This override omits MoveEffectAttr's HP/condition/chance gates.
         * BATTLE_STATS is ordered ATK, DEF, SPATK, SPDEF, SPD, ACC, EVA. */
        int32_t eligible[7];
        int n = 0;
        for (int stat = STAT_ATK; stat <= STAT_EVA; stat++)
            if (target->stat_stages[stat - 1] < 6) eligible[n++] = stat;
        if (n) {
            int32_t stat = eligible[pr_battle_rand_seed_int(bt, n, 0)];
            if (pr_unshift_stat_stage_change(bt, e, pr_battler_index(bt, target), &stat, 1, 2, user) < 0)
                e->overflow = true;
        }
        return;
    }
    case PR_AC_CutHpStatStageBoostAttr: {
        const pr_attr_CutHpStatStageBoostAttr *x = P(CutHpStatStageBoostAttr, a);
        /* Its override pays the cost before the inherited HP/condition/chance
         * gates, including on raw calls that fail the separate move condition. */
        double cost = to_dmg_value(pr_get_stat(user, STAT_HP, true) / x->cutRatio);
        pr_damage_and_update(bt, e, user, cost, HIT_RESULT_INDIRECT, false, NULL);
        stat_stage_change(bt, e, ef, a->cls, user, target, move, x->stats.v, x->stats.n, x->stages);
        return;
    }
    case PR_AC_SecretPowerAttr: {
        if (!effect_can_apply(ef, user, target)) return;
        int effect = STATUS_EFFECT_PARALYSIS;
        int32_t stat = -1;
        bool flinch = false;
        if (bt->terrain != TERRAIN_NONE) {
            switch (bt->terrain) {
            case TERRAIN_MISTY: stat = STAT_SPATK; break;
            case TERRAIN_GRASSY: effect = STATUS_EFFECT_SLEEP; break;
            case TERRAIN_PSYCHIC: stat = STAT_SPD; break;
            default: break;
            }
        } else if (bt->has_biome) {
            switch (bt->biome) {
            case BIOME_ID_PLAINS: case BIOME_ID_GRASS: case BIOME_ID_TALL_GRASS:
            case BIOME_ID_FOREST: case BIOME_ID_JUNGLE: case BIOME_ID_MEADOW:
                effect = STATUS_EFFECT_SLEEP; break;
            case BIOME_ID_SWAMP: case BIOME_ID_MOUNTAIN: case BIOME_ID_TEMPLE: case BIOME_ID_RUINS:
                stat = STAT_SPD; break;
            case BIOME_ID_ICE_CAVE: case BIOME_ID_SNOWY_FOREST: effect = STATUS_EFFECT_FREEZE; break;
            case BIOME_ID_VOLCANO: effect = STATUS_EFFECT_BURN; break;
            case BIOME_ID_FAIRY_CAVE: stat = STAT_SPATK; break;
            case BIOME_ID_DESERT: case BIOME_ID_CONSTRUCTION_SITE: case BIOME_ID_BEACH:
            case BIOME_ID_ISLAND: case BIOME_ID_BADLANDS: stat = STAT_ACC; break;
            case BIOME_ID_SEA: case BIOME_ID_LAKE: case BIOME_ID_SEABED: stat = STAT_ATK; break;
            case BIOME_ID_CAVE: case BIOME_ID_WASTELAND: case BIOME_ID_GRAVEYARD:
            case BIOME_ID_ABYSS: case BIOME_ID_SPACE: flinch = true; break;
            case BIOME_ID_END: stat = STAT_DEF; break;
            default: break;
            }
        }
        /* The source constructs a fresh child with default options, rather than
         * inheriting the outer attribute's gates or serializing a new attribute. */
        const pr_effect_info child = {.chance_override = -1, .condition = PR_NO_FN};
        if (stat >= 0) {
            stat_stage_change(bt, e, &child, PR_AC_StatStageChangeAttr, user, target, move, &stat, 1, -1);
        } else if (flinch) {
            const pr_attr_AddBattlerTagAttr tag = {.tagType = BATTLER_TAG_TYPE_FLINCHED, .failOnOverlap = true};
            const pr_attr attr = {PR_AC_AddBattlerTagAttr, PR_NO_FN, &tag};
            add_battler_tag(bt, e, &attr, &child, user, target, move);
        } else status_effect_apply(bt, e, &child, user, target, move, effect);
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
        for (int k = 0; tm && k < PR_N_MOVES(target); k++)
            if (PR_MOVES(target)[k] == tm->move) { s = k; break; }
        int max = s < 0 ? 0 : reduce_pp_move_pp(target, s);
        if (a->cls == PR_AC_AttackReducePpMoveAttr) {
            double ratio = s < 0 ? 0 : 1.0 - (double)PR_PP_USED(target)[s] / max;   /* getPpRatio() */
            if (!(ratio == ratio && ratio != 0)) return;
        } else if (s < 0) {
            pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT);  /* the condition rules it out; upstream would throw */
            return;
        }
        int last = PR_PP_USED(target)[s], v = last + reduction;
        PR_PP_USED(target)[s] = (uint8_t)(v < max ? v : max);
        pr_sync_moveset_aliases(target);
        pr_queue_msg(bt, e, pr_msg3("battle:ppReduced", pr_arg_pokemon(bt, "targetName", target),
                                    pr_arg_move("moveName", PR_MOVES(target)[s]),
                                    pr_arg_int("reduction", PR_PP_USED(target)[s] - last)));
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
        status_effect_apply(bt, e, ef, user, target, move, effect);
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
        pr_gulp_missile_tag(bt,e,user,move);
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
                                .heal_hp = hp < 1 ? 1 : hp, .pokemon_name = pr_arg_pokemon(bt, "", user).v,
                                .fusion_name = pr_arg_pokemon(bt, "", user).fusion_v};
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
        int mm = PR_MOVES(target)[slot];
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
    case PR_AC_OrderUpStatBoostAttr: {
        /* This override deliberately omits the parent gates and reads the cached key. */
        const pr_battler_tag *tag = pr_get_battler_tag(user, BATTLER_TAG_TYPE_COMMANDED);
        if (!tag) return;
        const char *key = pr_commanded_form_key(tag);
        int32_t stat = !strcmp(key, "droopy") ? STAT_DEF : !strcmp(key, "stretchy") ? STAT_SPD : STAT_ATK;
        pr_unshift_stat_stage_change(bt, e, pr_battler_index(bt, user), &stat, 1, 1, user);
        return;
    }
    case PR_AC_StatusIfBoostedAttr:                      /* Burning Jealousy (self-targeted, reads the real target) */
        if (target->stat_stages_increased)
            pr_try_set_status(bt, e, target, P(StatusIfBoostedAttr, a)->effect, user, -1, false, true);
        return;
    case PR_AC_FlameBurstAttr: {                        /* raw override: no parent HP/condition gates */
        pr_pokemon *ally = (pr_pokemon *)pr_get_ally(bt, target);
        bool cancelled = false;
        if (ally) {
            pr_ab_params ap = {.pokemon = ally, .move = -1, .flag = &cancelled};
            pr_apply_ab_attrs(bt, PR_AC_BlockNonDirectDamageAbAttr, &ap);
        }
        if (cancelled || !ally || ally->switch_out_status) return;
        double damage = floor(pr_get_stat(ally, STAT_HP, false) / 16.0);
        pr_damage_and_update(bt, e, ally, damage < 1 ? 1 : damage, HIT_RESULT_INDIRECT, false, NULL);
        return;
    }
    case PR_AC_MoneyAttr: {                              /* Pay Day: moneyScattered += getWaveMoneyAmount(0.2) */
        bt->money_scattered += pr_wave_money_amount(bt->wave_index, 0.2);
        pr_queue_msg(bt, e, pr_msg0("moveTriggers:coinsScatteredEverywhere"));
        return;
    }
    case PR_AC_RemoveHeldItemAttr:
        pr_remove_move_held_item(bt, e, user, target, P(RemoveHeldItemAttr, a)->berriesOnly, NULL);
        return;
    case PR_AC_StealHeldItemChanceAttr:
        pr_steal_move_held_item(bt, e, user, target, P(StealHeldItemChanceAttr, a)->chance);
        return;
    case PR_AC_EatBerryAttr:
        pr_apply_eat_berry_move(bt,e,user,target,P(EatBerryAttr,a)->selfTarget,false);
        return;
    case PR_AC_StealEatBerryAttr:
        pr_apply_eat_berry_move(bt,e,user,target,false,true);
        return;
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
    case PR_AC_SacrificialFullRestoreAttr:              /* Healing Wish, Lunar Dance */
    case PR_AC_SacrificialAttrOnHit: {                   /* Memento, Final Gambit: only if the move hit */
        if (a->cls == PR_AC_SacrificialAttrOnHit && !effect_can_apply(ef, user, target)) return;
        /* damageAndUpdate(user.hp, {result: INDIRECT, ignoreSegments: true}), then damageTaken +=
         * user.hp: the HP left afterwards, 0 unless the user got through */
        pr_damage_and_update_segments(bt, e, user, user->hp, HIT_RESULT_INDIRECT, true, false, NULL);
        user->damage_taken += user->hp;
        if (a->cls == PR_AC_SacrificialFullRestoreAttr) {
            const pr_attr_SacrificialFullRestoreAttr *x = P(SacrificialFullRestoreAttr,a);
            pr_arena_queue_heal_name(bt,e,pr_battler_index(bt,user),user->id,move,x->restorePP,
                                pr_arg_pokemon(bt,"",user));
        }
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
    case PR_AC_InvertStatsAttr:
        if (!effect_can_apply(ef, user, target)) return;
        for (int s = 0; s < 7; s++) target->stat_stages[s] = -target->stat_stages[s];
        pr_queue_msg(bt, e, pr_msg1("moveTriggers:invertStats", pr_arg_pokemon(bt, "pokemonName", target)));
        return;
    case PR_AC_SwapStatAttr: {
        if (!effect_can_apply(ef, user, target)) return;
        int stat = P(SwapStatAttr, a)->stat;
        double old_user = pr_get_stat(user, stat, false);
        double old_target = pr_get_stat(target, stat, false);
        /* Pokemon.setStat(..., false) ignores nonpositive values. */
        if (old_target > 0) user->summon_stats[stat] = old_target;
        if (old_user > 0) target->summon_stats[stat] = old_user;
        pr_queue_msg(bt, e, pr_msg2("moveTriggers:switchedStat",
            pr_arg_pokemon(bt, "pokemonName", user), pr_arg_stat("stat", stat)));
        return;
    }
    case PR_AC_ShiftStatAttr: {
        if (!effect_can_apply(ef, user, target)) return;
        const pr_attr_ShiftStatAttr *x = P(ShiftStatAttr, a);
        double first = pr_get_stat(user, x->statToSwitch, false);
        double second = pr_get_stat(user, x->statToSwitchWith, false);
        if (second > 0) user->summon_stats[x->statToSwitch] = second;
        if (first > 0) user->summon_stats[x->statToSwitchWith] = first;
        pr_queue_msg(bt, e, pr_msg3("moveTriggers:shiftedStats",
            pr_arg_pokemon(bt, "pokemonName", user), pr_arg_stat("statToSwitch", x->statToSwitch),
            pr_arg_stat("statToSwitchWith", x->statToSwitchWith)));
        return;
    }
    case PR_AC_CopyTypeAttr: {
        if (!effect_can_apply(ef, user, target)) return;
        int8_t types[PR_MAX_TYPES];
        int n = pr_get_types(target, types, PR_TYPES_ORIGINAL_IF_STELLAR);
        for (int i = 0; i < n; i++) if (types[i] == POKEMON_TYPE_UNKNOWN) { types[i] = POKEMON_TYPE_NORMAL; break; }
        if (!pr_set_summon_types(bt, user, types, n)) return;
        pr_queue_msg(bt, e, pr_msg2("moveTriggers:copyType",
            pr_arg_pokemon(bt, "pokemonName", user), pr_arg_pokemon(bt, "targetPokemonName", target)));
        return;
    }
    case PR_AC_FirstMoveTypeAttr: {
        if (!effect_can_apply(ef, user, target)) return;
        if (!PR_N_MOVES(user)) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return; }
        int8_t type = pr_moves[PR_MOVES(user)[0]].type;
        pr_set_summon_types(bt, user, &type, 1);
        pr_queue_msg(bt, e, pr_msg2("battle:transformedIntoType",
            pr_arg_pokemon(bt, "pokemonName", user), pr_arg_type("type", type)));
        return;
    }
    case PR_AC_ResistLastMoveTypeAttr: {
        if (!effect_can_apply(ef, user, target)) return;
        const pr_turn_move *last = pr_last_move(bt, target, 0);
        if (!last) return;
        int attack_type = pr_moves[last->move].type;
        if (attack_type == POKEMON_TYPE_STELLAR || attack_type == POKEMON_TYPE_UNKNOWN) return;
        int8_t current[PR_MAX_TYPES], choices[PR_TYPE_COUNT];
        int n = pr_get_types(user, current, PR_TYPES_ORIGINAL_IF_STELLAR), count = 0;
        /* UNKNOWN is neutral, and Stellar is neutral to these ordinary attacks.
         * Preserve enum order before the user's battle RNG selects a resistance. */
        for (int type = 0; type < PR_TYPE_COUNT; type++) {
            bool have = false;
            for (int i = 0; i < n; i++) have |= current[i] == type;
            if (!have && pr_type_chart_x2[attack_type][type] < 2) choices[count++] = (int8_t)type;
        }
        if (!count) return;
        int8_t type = choices[pr_battle_rand_seed_int(bt, count, 0)];
        pr_set_summon_types(bt, user, &type, 1);
        /* Unlike Conversion, the pin passes the enum's English title literally. */
        static const char *const names[] = {"Normal", "Fighting", "Flying", "Poison", "Ground", "Rock", "Bug", "Ghost", "Steel", "Fire", "Water", "Grass", "Electric", "Psychic", "Ice", "Dragon", "Dark", "Fairy", "Stellar"};
        pr_msg_arg name = {"type", PR_ARG_STR, 0, names[type], 0};
        pr_queue_msg(bt, e, pr_msg2("battle:transformedIntoType", pr_arg_pokemon(bt, "pokemonName", user), name));
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
            pr_msg_arg name = {"playerName", PR_ARG_STR, 0, bt->player_name ? bt->player_name : "", 0};
            pr_queue_msg(bt, e, pr_msg1("moveTriggers:celebrate", name));
            break;
        }
        case MOVE_ID_LASER_FOCUS: pr_queue_msg(bt, e, pr_msg_p(bt, "battlerTags:laserFocusOnAdd", user)); break;
        default: pr_queue_message(bt, e);
        }
        return;
    case PR_AC_RevivalBlessingAttr:
        pr_revival_blessing(bt,e,user);
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
    case PR_AC_HealOnAllyAttr:
    case PR_AC_HealAttr:                                 /* Recover, Soft-Boiled, Roost, Heal Pulse... */
    case PR_AC_VariableHealAttr: {                       /* Moonlight, Swallow, Shore Up...: healRatio = healFunc(user) first */
        bool variable = a->cls == PR_AC_VariableHealAttr;
        double ratio;
        bool self_target, show_anim;
        if (a->cls == PR_AC_HealOnAllyAttr) {
            const pr_attr_HealOnAllyAttr *x = P(HealOnAllyAttr, a);
            if (pr_is_opponent(bt, user, target) || target != pr_get_ally(bt, user)) return;
            ratio = x->healRatio; self_target = x->selfTarget; show_anim = x->showAnim;
        } else if (variable) {
            const pr_attr_VariableHealAttr *v = P(VariableHealAttr, a);
            ratio = pr_closure_call(bt, v->healFunc, a->cls, 0, user, target, move);
            self_target = v->selfTarget; show_anim = v->showAnim;
        } else {
            const pr_attr_HealAttr *x = P(HealAttr, a);
            ratio = x->healRatio; self_target = x->selfTarget; show_anim = x->showAnim;
        }
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

/* Shared announcement/removal identities: named battle items and all berries.
 * Unsupported item names remain explicit rather than producing a false message. */
const char *pr_held_item_name_key(const pr_modifier *m)
{
    static const char *const berries[] = {
        "berry:sitrus.name", "berry:lum.name", "berry:enigma.name", "berry:liechi.name",
        "berry:ganlon.name", "berry:petaya.name", "berry:apicot.name", "berry:salac.name",
        "berry:lansat.name", "berry:starf.name", "berry:leppa.name"
    };
    if (m->unknown_subclass) return NULL;
    if (m->cls == PR_MOD_BERRY)
        return m->berry_type >= 0 && m->berry_type < 11 ? berries[m->berry_type] : NULL;
    if(m->cls==PR_MOD_ATTACK_TYPE_BOOSTER){
        /* Static literal keys keep queued messages independent of later queries. */
        static const char *const keys[]={
#define TYPE_ITEM(x) "modifierType:AttackTypeBoosterItem." x
            TYPE_ITEM("silk_scarf"),TYPE_ITEM("black_belt"),TYPE_ITEM("sharp_beak"),TYPE_ITEM("poison_barb"),TYPE_ITEM("soft_sand"),TYPE_ITEM("hard_stone"),TYPE_ITEM("silver_powder"),TYPE_ITEM("spell_tag"),TYPE_ITEM("metal_coat"),TYPE_ITEM("charcoal"),TYPE_ITEM("mystic_water"),TYPE_ITEM("miracle_seed"),TYPE_ITEM("magnet"),TYPE_ITEM("twisted_spoon"),TYPE_ITEM("never_melt_ice"),TYPE_ITEM("dragon_fang"),TYPE_ITEM("black_glasses"),TYPE_ITEM("fairy_feather")
#undef TYPE_ITEM
        };
        return m->move_type>=0&&m->move_type<18?keys[m->move_type]:NULL;
    }
    switch (m->cls) {
    case PR_MOD_STAT_BOOSTER: case PR_MOD_EVOLUTION_STAT_BOOSTER:
        return m->mult==1.5&&m->stats_mask==((1u<<STAT_DEF)|(1u<<STAT_SPDEF))?"modifierType:ModifierType.EVIOLITE.name":NULL;
    case PR_MOD_SPECIES_STAT_BOOSTER:
        if(m->mult!=2)return NULL;
        if(m->n_species==1&&m->species[0]==SPECIES_ID_PIKACHU&&m->stats_mask==((1u<<STAT_ATK)|(1u<<STAT_SPATK)))return "modifierType:SpeciesBoosterItem.LIGHT_BALL.name";
        if(m->n_species==3&&m->species[0]==SPECIES_ID_CUBONE&&m->species[1]==SPECIES_ID_MAROWAK&&m->species[2]==SPECIES_ID_ALOLA_MAROWAK&&m->stats_mask==(1u<<STAT_ATK))return "modifierType:SpeciesBoosterItem.THICK_CLUB.name";
        if(m->n_species==1&&m->species[0]==SPECIES_ID_DITTO){if(m->stats_mask==(1u<<STAT_DEF))return "modifierType:SpeciesBoosterItem.METAL_POWDER.name";if(m->stats_mask==(1u<<STAT_SPD))return "modifierType:SpeciesBoosterItem.QUICK_POWDER.name";}
        if(m->n_species==1&&m->species[0]==SPECIES_ID_CLAMPERL){if(m->stats_mask==(1u<<STAT_SPDEF))return "modifierType:SpeciesBoosterItem.DEEP_SEA_SCALE.name";if(m->stats_mask==(1u<<STAT_SPATK))return "modifierType:SpeciesBoosterItem.DEEP_SEA_TOOTH.name";}
        return NULL;
    case PR_MOD_FIELD_EFFECT: return "modifierType:ModifierType.MYSTICAL_ROCK.name";
    case PR_MOD_SPECIES_CRIT_BOOSTER: return "modifierType:ModifierType.LEEK.name";
    case PR_MOD_EXP_BOOSTER: return m->mult==0.4?"modifierType:ModifierType.LUCKY_EGG.name":m->mult==1.0?"modifierType:ModifierType.GOLDEN_EGG.name":NULL;
    case PR_MOD_FRIENDSHIP_BOOSTER: return "modifierType:ModifierType.SOOTHE_BELL.name";
    case PR_MOD_MOVE_ACCURACY_BOOSTER: return "modifierType:ModifierType.WIDE_LENS.name";
    case PR_MOD_MULTI_HIT: return "modifierType:ModifierType.MULTI_LENS.name";
    case PR_MOD_CRIT_BOOSTER: return "modifierType:ModifierType.SCOPE_LENS.name";
    case PR_MOD_NATURE_WEIGHT: return "modifierType:ModifierType.SOUL_DEW.name";
    case PR_MOD_DAMAGE_MONEY_REWARD: return "modifierType:ModifierType.GOLDEN_PUNCH.name";
    case PR_MOD_INSTANT_REVIVE: return "modifierType:ModifierType.REVIVER_SEED.name";
    case PR_MOD_TURN_STATUS_EFFECT: return m->status_effect==STATUS_EFFECT_TOXIC?"modifierType:ModifierType.TOXIC_ORB.name":m->status_effect==STATUS_EFFECT_BURN?"modifierType:ModifierType.FLAME_ORB.name":NULL;
    case PR_MOD_SWITCH_EFFECT_TRANSFER: return "modifierType:ModifierType.BATON.name";
    case PR_MOD_CONTACT_HELD_TRANSFER: return "modifierType:ModifierType.GRIP_CLAW.name";
    case PR_MOD_TURN_HELD_TRANSFER: return "modifierType:ModifierType.MINI_BLACK_HOLE.name";
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
    const char *key = pr_held_item_name_key(m);
    if (!key) { pr_note_unported(bt, PR_AC_RemoveHeldItemAttr); return false; }
    if (removed) *removed = *m;
    pr_lose_held_item(bt, e, target, m);
    pr_queue_msg(bt, e, pr_msg3(berries_only ? "moveTriggers:incineratedItem" : "moveTriggers:knockedOffItem",
        pr_arg_pokemon(bt, "pokemonName", user), pr_arg_pokemon(bt, "targetName", target),
        pr_arg_key("itemName", key)));
    return true;
}
