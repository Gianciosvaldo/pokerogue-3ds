/* Ported move attribute classes (src/data/moves/move.ts). Each apply loop walks the
 * move's attrs in upstream order; a class without a case here is reported unported. */
#include "pr/battle.h"
#include <math.h>

#define ATTR(m, i) (&pr_attrs[pr_moves[m].attr0 + (i)])
#define P(T, a) ((const pr_attr_##T *)(a)->p)

int pr_beat_up_hit_count(pr_battle *bt, const pr_pokemon *user)
{
    int side = user->is_player ? 0 : 1, count = 0;
    if (!bt->n_party[side]) pr_note_unported(bt, PR_FEAT_PARTY);
    for (int i = 0; i < bt->n_party[side]; i++) {
        const pr_pokemon *p = bt->party[side][i];
        /* The pin checks status and identity, not HP or field activity. */
        if (p->id == user->id || p->status == STATUS_EFFECT_NONE) count++;
    }
    return count;
}

static int count_positive_stat_stages(const pr_pokemon *p)
{
    int n = 0;
    for (int i = 0; i < 7; i++) if (p->stat_stages[i] > 0) n += p->stat_stages[i];
    return n;
}

/* toDmgValue: Math.max(Math.floor(v), 1), where NaN stays NaN */
static double to_dmg_value(double v)
{
    v = floor(v);
    return v > 1 || v != v ? v : 1;
}

/* randSeedInt(range) inside executeWithSeedOffset(fn, turn << 6, waveSeed): a fresh RNG
 * sown from the shifted wave seed; the global RNG state is restored afterwards, so a
 * copy stands in for it. Returns -1 when the caller didn't provide the wave seed. */
int pr_wave_seed_roll(pr_battle *bt, int range)
{
    if (!bt->wave_seed.len) { pr_note_unported(bt, PR_FEAT_WAVE); return -1; }
    pr_rng r;
    pr_seedstr s;
    pr_seed_shift(&s, &bt->wave_seed, bt->turn << 6);
    pr_rng_sow(&r, &s);
    return pr_rng_seed_int(&r, range, 0);
}

void pr_apply_variable_power(pr_battle *bt, const pr_pokemon *user, const pr_pokemon *target, int move,
                             double *power)
{
    for (unsigned i = 0; i < pr_moves[move].nattr; i++) {
        const pr_attr *a = ATTR(move, i);
        if (!pr_attr_is(a->cls, PR_AC_VariablePowerAttr)) continue;
        switch (a->cls) {
        case PR_AC_MovePowerMultiplierAttr:
            *power *= pr_closure_call(bt, P(MovePowerMultiplierAttr, a)->powerMultiplierFunc, a->cls, 1,
                                      user, target, move);
            break;
        case PR_AC_WeightPowerAttr: {
            double w = pr_get_weight(bt, target);
            static const double th[5] = {10, 25, 50, 100, 200};
            int k = 0;
            while (w >= th[k]) if (++k == 5) break;
            *power = (k + 1) * 20;
            break;
        }
        case PR_AC_CompareWeightPowerAttr: {
            double uw = pr_get_weight(bt, user), tw = pr_get_weight(bt, target);
            if (!uw) break;
            double rel = (tw / uw) * 100;
            *power = rel < 20.01 ? 120 : rel < 25.01 ? 100 : rel < 33.35 ? 80 : rel < 50.01 ? 60 : 40;
            break;
        }
        case PR_AC_HpPowerAttr:
            *power = to_dmg_value(150 * pr_get_hp_ratio(user, false));
            break;
        case PR_AC_LowHpPowerAttr: {
            double r = pr_get_hp_ratio(user, false);
            *power = r < 0.0417 ? 200 : r < 0.1042 ? 150 : r < 0.2083 ? 100 : r < 0.3542 ? 80 : r < 0.6875 ? 40 : 20;
            break;
        }
        case PR_AC_OpponentHighHpPowerAttr:
            *power = to_dmg_value(P(OpponentHighHpPowerAttr, a)->maxBasePower * pr_get_hp_ratio(target, false));
            break;
        case PR_AC_PositiveStatStagePowerAttr:
            *power += count_positive_stat_stages(user) * 20;
            break;
        case PR_AC_PunishmentPowerAttr: {
            double v = 60 + count_positive_stat_stages(target) * 20;
            *power = v < 200 ? v : 200;
            break;
        }
        case PR_AC_GyroBallPowerAttr: {
            /* user.getEffectiveStat(SPD): no opponent, no move */
            double us = pr_get_effective_stat(bt, user, STAT_SPD, NULL, -1, false, false);
            if (us < 1) { *power = 1; break; }
            double v = (25 * pr_get_effective_stat(bt, target, STAT_SPD, NULL, -1, false, false)) / us + 1;
            *power = floor(v < 150 ? v : 150);
            break;
        }
        case PR_AC_ElectroBallPowerAttr: {
            double ratio = (double)pr_get_effective_stat(bt, target, STAT_SPD, NULL, -1, false, false)
                           / pr_get_effective_stat(bt, user, STAT_SPD, NULL, -1, false, false);
            static const double th[5] = {0.25, 1.0 / 3, 0.5, 1, -1};
            static const double pw[5] = {150, 120, 80, 60, 40};
            int k = 0;
            while (k < 4 && ratio > th[k]) if (++k == 5) break;
            *power = pw[k];
            break;
        }
        case PR_AC_AntiSunlightPowerDecreaseAttr:
            switch (pr_get_effective_weather_for_move(bt, user)) {
            case WEATHER_TYPE_RAIN: case WEATHER_TYPE_SANDSTORM: case WEATHER_TYPE_HAIL:
            case WEATHER_TYPE_SNOW: case WEATHER_TYPE_FOG: case WEATHER_TYPE_HEAVY_RAIN:
                *power *= 0.5;
            }
            break;
        case PR_AC_TeraBlastPowerAttr:
            if (user->terastallized && pr_get_tera_type(user) == POKEMON_TYPE_STELLAR) *power = 100;
            break;
        case PR_AC_ConsecutiveUseDoublePowerAttr:
            /* Rollout, Ice Ball, Fury Cutter: the constructor closure (closures_moves.c) gives
             * this.getMultiplier(count) = Math.pow(2, count) */
            *power *= pr_closure_call(bt, P(ConsecutiveUseDoublePowerAttr, a)->powerMultiplierFunc, a->cls, 1,
                                      user, target, move);
            break;
        case PR_AC_ConsecutiveUseMultiBasePowerAttr:
            /* Echoed Voice: count + 1 */
            *power *= pr_closure_call(bt, P(ConsecutiveUseMultiBasePowerAttr, a)->powerMultiplierFunc, a->cls, 1,
                                      user, target, move);
            break;
        case PR_AC_TurnDamagedDoublePowerAttr: {
            /* Revenge, Avalanche: user.turnData.attacksReceived.find(r => r.damage && r.sourceId === target.id) */
            if (pr_received_damage_from(bt, user, target->id)) *power *= 2;
            break;
        }
        case PR_AC_LastMoveDoublePowerAttr: {
            /* Fusion Flare / Bolt: for (p of getLastTurnOrder().slice(0, -1).reverse()) {
             *   const [lastMove] = p.getLastXMoves(1);
             *   if (lastMove.result !== FAIL) { if (SUCCESS && lastMove.move === this.move) x2; break; } } */
            for (int k = bt->n_last_turn_order - 2; k >= 0; k--) {
                const pr_pokemon *p = k < PR_MAX_TURN_ORDER ? pr_field_pokemon_by_id(bt, bt->last_turn_order[k]) : NULL;
                if (!p) { pr_note_unported(bt, PR_FEAT_TURN_ORDER); break; }
                const pr_turn_move *last = pr_last_move(bt, p, 0);
                /* An empty history makes upstream read .result of undefined and throw */
                if (!last) { pr_note_unported(bt, PR_FEAT_TURN_ORDER); break; }
                if (last->result != MOVE_RESULT_FAIL) {
                    if (last->result == MOVE_RESULT_SUCCESS && last->move == P(LastMoveDoublePowerAttr, a)->move)
                        *power *= 2;
                    break;
                }
            }
            break;
        }
        case PR_AC_MultiHitPowerIncrementAttr: {
            /* Triple Kick, Triple Axel */
            int hits_total = user->hit_count - (user->hits_left > 0 ? user->hits_left : 0);
            *power = pr_moves[move].power * (1 + (hits_total % P(MultiHitPowerIncrementAttr, a)->maxHits));
            break;
        }
        case PR_AC_WaterShurikenPowerAttr:
            if (user->species == SPECIES_ID_BATTLE_BOND_GRENINJA && user->form == 1) *power = 20;
            break;
        case PR_AC_DoublePowerChanceAttr: { /* Fickle Beam */
            int roll = pr_wave_seed_roll(bt, 100);
            if (roll >= 0 && roll < P(DoublePowerChanceAttr, a)->chance) *power *= 2;
            break;
        }
        case PR_AC_MagnitudePowerAttr: {
            static const int thresholds[6] = {5, 15, 35, 65, 85, 95};
            static const int powers[7] = {10, 30, 50, 70, 90, 110, 150};
            int roll = pr_wave_seed_roll(bt, 100);
            if (roll < 0) break;
            int k = 0;
            while (k < 6 && !(roll < thresholds[k])) k++;
            *power = powers[k];
            break;
        }
        case PR_AC_FriendshipPowerAttr: { /* Return, Frustration, Pika Papow, Veevee Volley */
            /* wild Pokemon use their species' base friendship */
            int f = user->is_player || user->has_trainer ? user->friendship : pr_pokemon_species(user)->base_friendship;
            if (f < 0) f = 0;
            if (f > 255) f = 255;
            *power = to_dmg_value((P(FriendshipPowerAttr, a)->invert ? 255 - f : f) / 2.5);
            break;
        }
        case PR_AC_LessPPMorePowerAttr: { /* Trump Card */
            int pp_used = 0;
            for (int k = 0; k < 4; k++)
                if (PR_MOVES(user)[k] == move) { pp_used = PR_PP_USED(user)[k]; break; }
            int remains = pr_moves[move].pp - pp_used;
            if (remains < 0) remains = 0;
            static const double pw[4] = {200, 80, 60, 50};
            *power = remains < 4 ? pw[remains] : 40;
            break;
        }
        case PR_AC_SpitUpPowerAttr: {
            /* multiplier x stockpiled count, only while a StockpilingTag with stacks exists */
            const pr_battler_tag *st = pr_find_battler_tag_class(user, PR_TAGCLASS_STOCKPILING);
            if (st && st->stockpiled_count > 0) *power = (double)P(SpitUpPowerAttr, a)->multiplier * st->stockpiled_count;
            break;
        }
        case PR_AC_BeatUpAttr: {
            /* The hit number, modulo the party members that count (the user, and allies without a
             * status), picks where beatUpFunc starts walking the whole party; the first member that
             * counts gives its species' base Attack / 10 + 5. */
            int side = user->is_player ? 0 : 1, n = bt->n_party[side];
            if (!n) { pr_note_unported(bt, PR_FEAT_PARTY); break; }
            pr_pokemon *const *party = bt->party[side];
            int allies = 0;
            for (int k = 0; k < n; k++)
                if (party[k]->id == user->id || !party[k]->status) allies++;
            if (!allies) { *power = 0; break; } /* x % 0 is NaN: the loop doesn't run */
            int idx = (user->hit_count - user->hits_left) % allies;
            if (idx < 0) { pr_note_unported(bt, PR_FEAT_HIT_STATE); break; } /* party[-k] is undefined: upstream throws */
            *power = 0;
            for (int k = idx; k < n; k++) {
                if (party[k]->id != user->id && party[k]->status) continue;
                pr_pokemon sp = {.species = party[k]->species};
                *power = pr_pokemon_form(&sp)->base_stats[STAT_ATK] / 10.0 + 5; /* species.getBaseStat: form 0 */
                break;
            }
            break;
        }
        case PR_AC_PresentPowerAttr: {
            /* A global-RNG roll; the heal outcome keeps the power, ends the move's other hits and
             * queues a heal for the target. Upstream does this even when simulated. */
            if (!bt->rng_live) { pr_note_unported(bt, PR_FEAT_GLOBAL_RNG); break; }
            pr_pokemon *u = (pr_pokemon *)user; /* user.turnData is written */
            bool first_hit = u->hit_count == u->hits_left;
            int roll = pr_rng_seed_int(&bt->rng, first_hit ? 100 : 80, 0);
            if (roll <= 40) *power = 40;
            else if (roll <= 70) *power = 80;
            else if (roll <= 80) *power = 120;
            else if (roll <= 100) {
                u->hit_count = 1;
                u->hits_left = 1;
                pr_note_deferred(bt, a->cls); /* PokemonHealPhase: target heals a quarter of its max HP */
            }
            break;
        }
        case PR_AC_RoundPowerAttr:
            if (user->joined_round) *power *= 2;
            break;
        case PR_AC_RageFistPowerAttr:
            *power = 50 * (1 + (user->summon_hit_count < 6 ? user->summon_hit_count : 6));
            break;
        case PR_AC_CombinedPledgePowerAttr:
            if (user->combining_pledge && user->combining_pledge != move) *power *= 150.0 / 80;
            break;
        default:
            pr_note_unported(bt, a->cls);
        }
    }
}

/* ---- applyMoveAttrs("VariableMoveTypeAttr", user, null, move, moveType) ---- */
int pr_hidden_power_type(const pr_pokemon *u)
{
    static const int8_t types[16] = {
        POKEMON_TYPE_FIGHTING, POKEMON_TYPE_FLYING, POKEMON_TYPE_POISON, POKEMON_TYPE_GROUND,
        POKEMON_TYPE_ROCK, POKEMON_TYPE_BUG, POKEMON_TYPE_GHOST, POKEMON_TYPE_STEEL,
        POKEMON_TYPE_FIRE, POKEMON_TYPE_WATER, POKEMON_TYPE_GRASS, POKEMON_TYPE_ELECTRIC,
        POKEMON_TYPE_PSYCHIC, POKEMON_TYPE_ICE, POKEMON_TYPE_DRAGON, POKEMON_TYPE_DARK};
    int v = (u->ivs[STAT_HP] & 1) + (u->ivs[STAT_ATK] & 1) * 2 + (u->ivs[STAT_DEF] & 1) * 4
            + (u->ivs[STAT_SPD] & 1) * 8 + (u->ivs[STAT_SPATK] & 1) * 16 + (u->ivs[STAT_SPDEF] & 1) * 32;
    return types[(v * 15) / 63];
}

void pr_apply_variable_move_type(pr_battle *bt, const pr_pokemon *user, int move, int *type)
{
    const pr_move *mv = &pr_moves[move];
    for (unsigned i = 0; i < mv->nattr; i++) {
        const pr_attr *a = ATTR(move, i);
        if (!pr_attr_is(a->cls, PR_AC_VariableMoveTypeAttr)) continue;
        /* Fusions aren't modelled, so fusionSpecies never matches. */
        int sp = user->species, form = user->form;
        switch (a->cls) {
        case PR_AC_HiddenPowerTypeAttr:
            *type = pr_hidden_power_type(user);
            break;
        case PR_AC_WeatherBallTypeAttr:
            switch (pr_get_effective_weather_for_move(bt, user)) {
            case WEATHER_TYPE_SUNNY: case WEATHER_TYPE_HARSH_SUN: *type = POKEMON_TYPE_FIRE; break;
            case WEATHER_TYPE_RAIN: case WEATHER_TYPE_HEAVY_RAIN: *type = POKEMON_TYPE_WATER; break;
            case WEATHER_TYPE_SANDSTORM: *type = POKEMON_TYPE_ROCK; break;
            case WEATHER_TYPE_HAIL: case WEATHER_TYPE_SNOW: *type = POKEMON_TYPE_ICE; break;
            default: *type = mv->type; break;
            }
            break;
        case PR_AC_TerrainPulseTypeAttr:
            if (!pr_is_grounded(bt, user, false)) break;
            switch (bt->terrain) {
            case TERRAIN_MISTY: *type = POKEMON_TYPE_FAIRY; break;
            case TERRAIN_ELECTRIC: *type = POKEMON_TYPE_ELECTRIC; break;
            case TERRAIN_GRASSY: *type = POKEMON_TYPE_GRASS; break;
            case TERRAIN_PSYCHIC: *type = POKEMON_TYPE_PSYCHIC; break;
            default: *type = mv->type; break;
            }
            break;
        case PR_AC_FormChangeItemTypeAttr: /* Judgment, Multi-Attack */
            if (sp == SPECIES_ID_ARCEUS || sp == SPECIES_ID_SILVALLY) {
                if (form >= 0 && form <= POKEMON_TYPE_STELLAR && form != POKEMON_TYPE_STELLAR) *type = form;
            } else {
                *type = mv->type;
            }
            break;
        case PR_AC_TechnoBlastTypeAttr:
            if (sp == SPECIES_ID_GENESECT) {
                static const int8_t drive[5] = {POKEMON_TYPE_NORMAL, POKEMON_TYPE_ELECTRIC, POKEMON_TYPE_FIRE,
                                                POKEMON_TYPE_ICE, POKEMON_TYPE_WATER};
                *type = form >= 1 && form <= 4 ? drive[form] : POKEMON_TYPE_NORMAL;
            }
            break;
        case PR_AC_AuraWheelTypeAttr:
            if (sp == SPECIES_ID_MORPEKO) *type = form == 1 ? POKEMON_TYPE_DARK : POKEMON_TYPE_ELECTRIC;
            break;
        case PR_AC_RagingBullTypeAttr:
            if (sp == SPECIES_ID_PALDEA_TAUROS)
                *type = form == 1 ? POKEMON_TYPE_FIRE : form == 2 ? POKEMON_TYPE_WATER : POKEMON_TYPE_FIGHTING;
            break;
        case PR_AC_IvyCudgelTypeAttr:
            if (sp == SPECIES_ID_OGERPON) {
                switch (form) {
                case 1: case 5: *type = POKEMON_TYPE_WATER; break;
                case 2: case 6: *type = POKEMON_TYPE_FIRE; break;
                case 3: case 7: *type = POKEMON_TYPE_ROCK; break;
                default: *type = POKEMON_TYPE_GRASS; break;
                }
            }
            break;
        case PR_AC_TeraBlastTypeAttr:
            if (user->terastallized) *type = pr_get_tera_type(user);
            break;
        case PR_AC_TeraStarstormTypeAttr:
            if (user->terastallized && sp == SPECIES_ID_TERAPAGOS) *type = POKEMON_TYPE_STELLAR;
            break;
        case PR_AC_MatchUserTypeAttr: { /* Revelation Dance */
            int8_t t[PR_MAX_TYPES];
            if (pr_get_types(user, t, PR_TYPES_ORIGINAL_IF_STELLAR) > 0) *type = t[0];
            break;
        }
        case PR_AC_CombinedPledgeTypeAttr:
            if (!user->combining_pledge) break;
            if (move == MOVE_ID_FIRE_PLEDGE && user->combining_pledge == MOVE_ID_WATER_PLEDGE) *type = POKEMON_TYPE_WATER;
            else if (move == MOVE_ID_WATER_PLEDGE && user->combining_pledge == MOVE_ID_GRASS_PLEDGE) *type = POKEMON_TYPE_GRASS;
            else if (move == MOVE_ID_GRASS_PLEDGE && user->combining_pledge == MOVE_ID_FIRE_PLEDGE) *type = POKEMON_TYPE_FIRE;
            break;
        default:
            pr_note_unported(bt, a->cls);
        }
    }
}

/* ---- applyMoveAttrs("MoveTypeChartOverrideAttr", source, target, move, multi, types, moveType) ---- */
static bool types_include(const int8_t *types, int n, int t)
{
    for (int i = 0; i < n; i++) if (types[i] == t) return true;
    return false;
}

static double chart(int atk, int def)
{
    if (atk == POKEMON_TYPE_UNKNOWN || def == POKEMON_TYPE_UNKNOWN) return 1;
    if (atk < 0 || atk >= PR_TYPE_COUNT || def < 0 || def >= PR_TYPE_COUNT) return 1;
    return pr_type_chart_x2[atk][def] / 2.0;
}

void pr_apply_type_chart_override(pr_battle *bt, const pr_pokemon *user, const pr_pokemon *target, int move,
                                  double *mult, const int8_t *types, int ntypes, int move_type)
{
    const pr_move *mv = &pr_moves[move];
    for (unsigned i = 0; i < mv->nattr; i++) {
        const pr_attr *a = ATTR(move, i);
        if (!pr_attr_is(a->cls, PR_AC_MoveTypeChartOverrideAttr)) continue;
        switch (a->cls) {
        case PR_AC_FreezeDryAttr:
            if (types_include(types, ntypes, POKEMON_TYPE_WATER)) *mult *= 2 / chart(move_type, POKEMON_TYPE_WATER);
            break;
        case PR_AC_NihilLightAttr:
            if (move_type == POKEMON_TYPE_DRAGON && types_include(types, ntypes, POKEMON_TYPE_FAIRY)) {
                double eff = 1;
                for (int k = 0; k < ntypes; k++) eff *= types[k] == POKEMON_TYPE_FAIRY ? 1 : chart(move_type, types[k]);
                *mult = eff;
            }
            break;
        case PR_AC_NeutralDamageAgainstFlyingTypeAttr: /* Thousand Arrows */
            if (!pr_is_grounded(bt, target, true) && types_include(types, ntypes, POKEMON_TYPE_FLYING) && *mult <= 0)
                *mult = 1;
            break;
        case PR_AC_HitsSameTypeAttr: { /* Synchronoise: user.getTypes() with default options */
            if (!user) { pr_note_unported(bt, a->cls); break; }
            int8_t ut[PR_MAX_TYPES];
            int un = pr_get_types(user, ut, 0);
            if (types_include(ut, un, POKEMON_TYPE_UNKNOWN)) { *mult = 0; break; }
            bool shares = false;
            for (int k = 0; k < un; k++) if (types_include(types, ntypes, ut[k])) shares = true;
            if (!shares) *mult = 0;
            break;
        }
        case PR_AC_FlyingTypeMultiplierAttr: /* Flying Press: no move passed, so no recursion */
            *mult *= pr_get_attack_type_effectiveness(bt, target, POKEMON_TYPE_FLYING, user, -1);
            break;
        case PR_AC_IceNoEffectTypeAttr: /* Sheer Cold */
            if (types_include(types, ntypes, POKEMON_TYPE_ICE)) *mult = 0;
            break;
        default:
            pr_note_unported(bt, a->cls);
        }
    }
}

/* ---- applyMoveAttrs("VariableMoveCategoryAttr", user, target, move, category) ---- */
int pr_apply_variable_move_category(pr_battle *bt, const pr_pokemon *user, const pr_pokemon *target, int move,
                                    int category)
{
    const pr_move *mv = &pr_moves[move];
    for (unsigned i = 0; i < mv->nattr; i++) {
        const pr_attr *a = ATTR(move, i);
        if (!pr_attr_is(a->cls, PR_AC_VariableMoveCategoryAttr)) continue;
        switch (a->cls) {
        case PR_AC_PhotonGeyserCategoryAttr: {
            pr_stat_params sp = {.opponent = target, .move = move, .simulated = true};
            if (pr_get_effective_stat_p(bt, user, STAT_ATK, &sp) > pr_get_effective_stat_p(bt, user, STAT_SPATK, &sp))
                category = MOVE_CATEGORY_PHYSICAL;
            break;
        }
        case PR_AC_TeraMoveCategoryAttr: {
            pr_stat_params sp = {.opponent = target, .move = move, .ignore_ability = true, .ignore_opp_ability = true,
                                 .ignore_ally_ability = true, .ignore_held_items = true, .simulated = true};
            if (user->terastallized
                && pr_get_effective_stat_p(bt, user, STAT_ATK, &sp) > pr_get_effective_stat_p(bt, user, STAT_SPATK, &sp))
                category = MOVE_CATEGORY_PHYSICAL;
            break;
        }
        case PR_AC_ShellSideArmCategoryAttr: {
            pr_base_damage_params bp = {.move = move, .category = MOVE_CATEGORY_PHYSICAL, .ignore_ability = true,
                                        .ignore_source_ability = true, .ignore_ally_ability = true,
                                        .ignore_source_ally_ability = true, .simulated = true};
            double phys = pr_get_base_damage(bt, target, user, &bp);
            bp.category = MOVE_CATEGORY_SPECIAL;
            double spec = pr_get_base_damage(bt, target, user, &bp);
            if (phys > spec) category = MOVE_CATEGORY_PHYSICAL;
            else if (phys == spec && pr_battle_rand_seed_int(bt, 2, 0) == 0) category = MOVE_CATEGORY_PHYSICAL;
            break;
        }
        default:
            pr_note_unported(bt, a->cls);
        }
    }
    return category;
}

/* ---- VariableAtkAttr / VariableDefAttr (getBaseDamage) ---- */
void pr_apply_variable_atk(pr_battle *bt, const pr_pokemon *user, const pr_pokemon *target, int move, double *atk)
{
    const pr_move *mv = &pr_moves[move];
    for (unsigned i = 0; i < mv->nattr; i++) {
        const pr_attr *a = ATTR(move, i);
        if (!pr_attr_is(a->cls, PR_AC_VariableAtkAttr)) continue;
        switch (a->cls) {
        case PR_AC_TargetAtkUserAtkAttr: /* Foul Play: target.getEffectiveStat(ATK, {opponent: target}) */
            *atk = pr_get_effective_stat(bt, target, STAT_ATK, target, -1, false, false);
            break;
        case PR_AC_DefAtkAttr: /* Body Press: user.getEffectiveStat(DEF, {opponent: target}) */
            *atk = pr_get_effective_stat(bt, user, STAT_DEF, target, -1, false, false);
            break;
        default:
            pr_note_unported(bt, a->cls);
        }
    }
}

void pr_apply_variable_def(pr_battle *bt, const pr_pokemon *user, const pr_pokemon *target, int move, double *def)
{
    const pr_move *mv = &pr_moves[move];
    for (unsigned i = 0; i < mv->nattr; i++) {
        const pr_attr *a = ATTR(move, i);
        if (!pr_attr_is(a->cls, PR_AC_VariableDefAttr)) continue;
        switch (a->cls) {
        case PR_AC_DefDefAttr: /* Psyshock: target.getEffectiveStat(DEF, {opponent: user}); no crit, no move */
            *def = pr_get_effective_stat(bt, target, STAT_DEF, user, -1, false, false);
            break;
        default:
            pr_note_unported(bt, a->cls);
        }
    }
}

/* ---- FixedDamageAttr family (getAttackDamage). Returns the fixed damage, 0 = none. ---- */
double *pr_half_hp_initial(pr_battle *bt, int move)
{
    const pr_attr *a = NULL;
    for (unsigned i = 0; i < pr_moves[move].nattr && !a; i++)
        if (ATTR(move, i)->cls == PR_AC_TargetHalfHpDamageAttr) a = ATTR(move, i);
    if (!a) return NULL;
    for (int i = 0; i < bt->n_half_hp_initial; i++)
        if (bt->half_hp_initial[i].attr == a) return &bt->half_hp_initial[i].hp;
    int n = sizeof bt->half_hp_initial / sizeof bt->half_hp_initial[0];
    int k = bt->n_half_hp_initial < n ? bt->n_half_hp_initial++ : n - 1;
    bt->half_hp_initial[k].attr = a;
    bt->half_hp_initial[k].hp = NAN; /* undefined */
    return &bt->half_hp_initial[k].hp;
}

double pr_apply_fixed_damage(pr_battle *bt, const pr_pokemon *user, const pr_pokemon *target, int move)
{
    double dmg = 0;
    const pr_move *mv = &pr_moves[move];
    for (unsigned i = 0; i < mv->nattr; i++) {
        const pr_attr *a = ATTR(move, i);
        if (!pr_attr_is(a->cls, PR_AC_FixedDamageAttr)) continue;
        switch (a->cls) {
        case PR_AC_FixedDamageAttr: dmg = P(FixedDamageAttr, a)->damage; break;       /* Sonic Boom, Dragon Rage */
        case PR_AC_LevelDamageAttr: dmg = user->level; break;                         /* Seismic Toss, Night Shade */
        case PR_AC_UserHpDamageAttr: dmg = user->hp; break;                           /* Final Gambit */
        case PR_AC_MatchHpAttr: dmg = target->hp - user->hp; break;                   /* Endeavor */
        case PR_AC_RandomLevelDamageAttr:                                            /* Psywave */
            dmg = to_dmg_value(user->level * (pr_battle_rand_seed_int(bt, 101, 50) * 0.01));
            break;
        case PR_AC_TargetHalfHpDamageAttr: {                                         /* Super Fang, Ruination */
            /* With a Multi Lens, the first hit stores the target's HP on the attribute
             * (this.initialHp) and the lens hits use it; Parental Bond's extra hit doesn't. */
            int32_t lens = pr_held_multi_lens_stacks(bt, user);
            int hit = user->hit_count - user->hits_left;
            if (lens <= 0 || hit == lens + 1) { dmg = to_dmg_value(target->hp / 2.0); break; }
            double *initial = pr_half_hp_initial(bt, move);
            if (hit == 0) *initial = target->hp;
            dmg = to_dmg_value(*initial / 2);
            break;
        }
        case PR_AC_CounterDamageAttr: {                                              /* Counter, Mirror Coat... */
            /* user.turnData.attacksReceived.find(ar => category !== STATUS
             *   && !areAllies(user.getBattlerIndex(), ar.sourceBattlerIndex)
             *   && (moveFilter === undefined || category === moveFilter))?.damage ?? 0,
             * where category is allMoves[ar.move].category; then toDmgValue(damage * multiplier) */
            const pr_attr_CounterDamageAttr *c = P(CounterDamageAttr, a);
            bool any_category = c->nulls & (1u << 2);
            pr_attack_record record;
            double damage = pr_received_counter(bt, user, any_category ? -1 : c->moveFilter, &record)
                ? record.damage : 0;
            dmg = to_dmg_value(damage * c->multiplier);
            break;
        }
        default:
            pr_note_unported(bt, a->cls);
        }
    }
    return dmg;
}

/* ---- OneHitKOAttr: sets the holder unless the target is a boss ---- */
bool pr_apply_one_hit_ko(pr_battle *bt, const pr_pokemon *user, const pr_pokemon *target, int move)
{
    (void)bt; (void)user;
    bool ko = false;
    const pr_move *mv = &pr_moves[move];
    for (unsigned i = 0; i < mv->nattr; i++)
        if (pr_attr_is(ATTR(move, i)->cls, PR_AC_OneHitKOAttr) && !target->is_boss) ko = true;
    return ko;
}

/* ---- HitsTagAttr: x2 per doubleDamage instance whose tag the target has ---- */
double pr_hits_tag_multiplier(pr_battle *bt, const pr_pokemon *target, int move)
{
    double m = 1;
    const pr_move *mv = &pr_moves[move];
    for (unsigned i = 0; i < mv->nattr; i++) {
        const pr_attr *a = ATTR(move, i);
        if (!pr_attr_is(a->cls, PR_AC_HitsTagAttr)) continue;
        const pr_attr_HitsTagAttr *x = P(HitsTagAttr, a); /* HitsTagForDoubleDamageAttr has the same layout */
        if (x->doubleDamage && pr_has_battler_tag(bt, target, x->tagType)) m *= 2;
    }
    return m;
}

/* ---- ModifiedDamageAttr: False Swipe, Hold Back ---- */
void pr_apply_modified_damage(pr_battle *bt, const pr_pokemon *user, const pr_pokemon *target, int move, double *dmg)
{
    (void)user;
    const pr_move *mv = &pr_moves[move];
    for (unsigned i = 0; i < mv->nattr; i++) {
        const pr_attr *a = ATTR(move, i);
        if (!pr_attr_is(a->cls, PR_AC_ModifiedDamageAttr)) continue;
        switch (a->cls) {
        case PR_AC_SurviveDamageAttr: {
            double cap = target->hp - 1;
            if (cap < *dmg) *dmg = cap;
            break;
        }
        default:
            pr_note_unported(bt, a->cls);
        }
    }
}

/* ---- CombinedPledgeStabBoostAttr ---- */
void pr_apply_pledge_stab(const pr_pokemon *user, int move, double *stab)
{
    if (pr_move_has_attr((uint16_t)move, PR_AC_CombinedPledgeStabBoostAttr) && user->combining_pledge
        && user->combining_pledge != move)
        *stab = 1.5;
}
