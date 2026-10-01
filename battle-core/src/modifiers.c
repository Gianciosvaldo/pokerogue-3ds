/* Ports of upstream's modifiers (src/modifier/modifier.ts) where the battle logic
 * applies them, and of the BattleScene helpers that apply them
 * (applyModifiers, applyModifier, findModifier, getHeldItems).
 *
 * applyModifiers(Type, player, ...args) keeps the modifiers of that side that are
 * instanceof Type and whose shouldApply(...args) holds, then calls apply(...args) on
 * each in list order. applyModifier does the same but stops at the first apply that
 * returns true. The shouldApply checks here never depend on values an apply changes,
 * so filtering and applying in one pass is the same thing. */
#include "pr/battle.h"
#include "pr/engine.h"
#include <math.h>
#include <string.h>

const uint8_t pr_mod_parent[PR_MOD_COUNT_] = {
    [PR_MOD_NONE] = PR_MOD_NONE,
    [PR_MOD_PERSISTENT] = PR_MOD_NONE,
    [PR_MOD_HELD_ITEM] = PR_MOD_PERSISTENT,
    [PR_MOD_TURN_HEAL] = PR_MOD_HELD_ITEM,
    [PR_MOD_HIT_HEAL] = PR_MOD_HELD_ITEM,
    [PR_MOD_TEMP_STAT_STAGE_BOOSTER] = PR_MOD_PERSISTENT, /* via LapsingPersistentModifier */
    [PR_MOD_TEMP_CRIT_BOOSTER] = PR_MOD_PERSISTENT,
    [PR_MOD_BASE_STAT] = PR_MOD_HELD_ITEM,
    [PR_MOD_BASE_STAT_TOTAL] = PR_MOD_HELD_ITEM,
    [PR_MOD_BASE_STAT_FLAT] = PR_MOD_HELD_ITEM,
    [PR_MOD_INCREMENTING_STAT] = PR_MOD_HELD_ITEM,
    [PR_MOD_STAT_BOOSTER] = PR_MOD_HELD_ITEM,
    [PR_MOD_EVOLUTION_STAT_BOOSTER] = PR_MOD_STAT_BOOSTER,
    [PR_MOD_SPECIES_STAT_BOOSTER] = PR_MOD_STAT_BOOSTER,
    [PR_MOD_CRIT_BOOSTER] = PR_MOD_HELD_ITEM,
    [PR_MOD_SPECIES_CRIT_BOOSTER] = PR_MOD_CRIT_BOOSTER,
    [PR_MOD_ATTACK_TYPE_BOOSTER] = PR_MOD_HELD_ITEM,
    [PR_MOD_NATURE_WEIGHT] = PR_MOD_HELD_ITEM,
    [PR_MOD_MULTI_HIT] = PR_MOD_HELD_ITEM,
    [PR_MOD_FIELD_EFFECT] = PR_MOD_HELD_ITEM,
    [PR_MOD_ENEMY_PERSISTENT] = PR_MOD_PERSISTENT,
    [PR_MOD_ENEMY_DAMAGE_MULTIPLIER] = PR_MOD_ENEMY_PERSISTENT,
    [PR_MOD_ENEMY_DAMAGE_BOOSTER] = PR_MOD_ENEMY_DAMAGE_MULTIPLIER,
    [PR_MOD_ENEMY_DAMAGE_REDUCER] = PR_MOD_ENEMY_DAMAGE_MULTIPLIER,
};

const char *const pr_mod_class_name[PR_MOD_COUNT_] = {
    [PR_MOD_NONE] = "",
    [PR_MOD_PERSISTENT] = "PersistentModifier",
    [PR_MOD_HELD_ITEM] = "PokemonHeldItemModifier",
    [PR_MOD_TURN_HEAL] = "TurnHealModifier",
    [PR_MOD_HIT_HEAL] = "HitHealModifier",
    [PR_MOD_TEMP_STAT_STAGE_BOOSTER] = "TempStatStageBoosterModifier",
    [PR_MOD_TEMP_CRIT_BOOSTER] = "TempCritBoosterModifier",
    [PR_MOD_BASE_STAT] = "BaseStatModifier",
    [PR_MOD_BASE_STAT_TOTAL] = "PokemonBaseStatTotalModifier",
    [PR_MOD_BASE_STAT_FLAT] = "PokemonBaseStatFlatModifier",
    [PR_MOD_INCREMENTING_STAT] = "PokemonIncrementingStatModifier",
    [PR_MOD_STAT_BOOSTER] = "StatBoosterModifier",
    [PR_MOD_EVOLUTION_STAT_BOOSTER] = "EvolutionStatBoosterModifier",
    [PR_MOD_SPECIES_STAT_BOOSTER] = "SpeciesStatBoosterModifier",
    [PR_MOD_CRIT_BOOSTER] = "CritBoosterModifier",
    [PR_MOD_SPECIES_CRIT_BOOSTER] = "SpeciesCritBoosterModifier",
    [PR_MOD_ATTACK_TYPE_BOOSTER] = "AttackTypeBoosterModifier",
    [PR_MOD_NATURE_WEIGHT] = "PokemonNatureWeightModifier",
    [PR_MOD_MULTI_HIT] = "PokemonMultiHitModifier",
    [PR_MOD_FIELD_EFFECT] = "FieldEffectModifier",
    [PR_MOD_ENEMY_PERSISTENT] = "EnemyPersistentModifier",
    [PR_MOD_ENEMY_DAMAGE_MULTIPLIER] = "EnemyDamageMultiplierModifier",
    [PR_MOD_ENEMY_DAMAGE_BOOSTER] = "EnemyDamageBoosterModifier",
    [PR_MOD_ENEMY_DAMAGE_REDUCER] = "EnemyDamageReducerModifier",
};

int pr_mod_class_by_name(const char *name)
{
    for (int c = 1; c < PR_MOD_COUNT_; c++)
        if (!strcmp(pr_mod_class_name[c], name)) return c;
    return -1;
}

bool pr_mod_is(const pr_modifier *m, int cls)
{
    for (int c = m->cls; c != PR_MOD_NONE; c = pr_mod_parent[c])
        if (c == cls) return true;
    return false;
}

int32_t pr_mod_get_stack_count(const pr_modifier *m)
{
    return m->stack_count + m->virtual_stack_count;
}

/* The modifiers of one side that are instanceof `cls`. An object whose own class the
 * port doesn't know could override shouldApply/apply, so it makes the query unported. */
static pr_modifier *next_of(pr_battle *bt, bool player, int cls, int *i)
{
    int side = player ? 0 : 1;
    while (*i < bt->n_mods[side]) {
        pr_modifier *m = &bt->mods[side][(*i)++];
        if (!pr_mod_is(m, cls)) continue;
        if (m->unknown_subclass) {
            pr_note_unported(bt, PR_FEAT_MODIFIERS);
            continue;
        }
        return m;
    }
    return NULL;
}
#define EACH_MOD(m, bt, player, cls) \
    for (int it_ = 0; ((m) = next_of((bt), (player), (cls), &it_)) != NULL;)

/* PokemonHeldItemModifier.shouldApply(pokemon): pokemonId === -1 || pokemon.id === pokemonId */
static bool held_applies(const pr_modifier *m, const pr_pokemon *p)
{
    return p && (m->pokemon_id == -1 || m->pokemon_id == (int64_t)p->id);
}

/* ---- getHeldItems: findModifiers(m => m instanceof PokemonHeldItemModifier && m.pokemonId === id) ----
 * No shouldApply or apply is called, so a subclass the port doesn't know is fine here. */
static bool is_held_by(const pr_modifier *m, const pr_pokemon *p)
{
    return pr_mod_is(m, PR_MOD_HELD_ITEM) && m->pokemon_id == (int64_t)p->id;
}

int pr_held_item_count(pr_battle *bt, const pr_pokemon *p, bool transferable_only)
{
    int side = p->is_player ? 0 : 1, n = 0;
    for (int i = 0; i < bt->n_mods[side]; i++) {
        const pr_modifier *m = &bt->mods[side][i];
        if (is_held_by(m, p) && (!transferable_only || m->is_transferable)) n++;
    }
    return n;
}

int32_t pr_held_item_stack_sum(pr_battle *bt, const pr_pokemon *p, bool transferable_only)
{
    int side = p->is_player ? 0 : 1;
    int32_t n = 0;
    for (int i = 0; i < bt->n_mods[side]; i++) {
        const pr_modifier *m = &bt->mods[side][i];
        if (is_held_by(m, p) && (!transferable_only || m->is_transferable)) n += m->stack_count;
    }
    return n;
}

int32_t pr_held_multi_lens_stacks(pr_battle *bt, const pr_pokemon *p)
{
    int side = p->is_player ? 0 : 1;
    for (int i = 0; i < bt->n_mods[side]; i++) {
        const pr_modifier *m = &bt->mods[side][i];
        if (is_held_by(m, p) && pr_mod_is(m, PR_MOD_MULTI_HIT)) return pr_mod_get_stack_count(m);
    }
    return 0;
}

/* ---- calculateBaseStats ---- */
/* FieldEffectModifier.apply: fieldDuration += 2 * stackCount. applyModifier stops at the first. */
void pr_apply_field_effect_modifier(pr_battle *bt, const pr_pokemon *user, int32_t *duration)
{
    pr_modifier *m;
    EACH_MOD(m, bt, user->is_player, PR_MOD_FIELD_EFFECT) {
        if (!held_applies(m, user)) continue;
        *duration += 2 * m->stack_count;
        return;
    }
}

static double clamp_stat(double v)
{
    return v < 1 ? 1 : v > 999999 ? 999999 : v; /* Math.min(Math.max(newVal, 1), 999999) */
}

void pr_apply_base_stat_modifiers(pr_battle *bt, const pr_pokemon *p, double base[6])
{
    pr_modifier *m;
    /* Shuckle Juice: every stat + statModifier (HP gets half) */
    EACH_MOD(m, bt, p->is_player, PR_MOD_BASE_STAT_TOTAL) {
        if (!held_applies(m, p)) continue;
        for (int i = 0; i < 6; i++)
            base[i] = clamp_stat(i == 0 ? floor(base[i] + m->num / 2.0) : floor(base[i] + m->num));
    }
    /* Old Gateau: +20 to the lower of HP/Speed, Atk/SpAtk and Def/SpDef, judged on the
     * species form's own base stats (getSpeciesForm().baseStats) */
    EACH_MOD(m, bt, p->is_player, PR_MOD_BASE_STAT_FLAT) {
        if (!held_applies(m, p)) continue;
        const uint8_t *bs = pr_pokemon_form(p)->base_stats;
        int pick[3] = {bs[STAT_HP] < bs[STAT_SPD] ? STAT_HP : STAT_SPD,
                       bs[STAT_ATK] < bs[STAT_SPATK] ? STAT_ATK : STAT_SPATK,
                       bs[STAT_DEF] < bs[STAT_SPDEF] ? STAT_DEF : STAT_SPDEF};
        for (int i = 0; i < 6; i++)
            if (i == pick[0] || i == pick[1] || i == pick[2]) base[i] = clamp_stat(floor(base[i] + 20));
    }
    /* (fusion averaging goes here once fusions are ported) */
    /* Vitamins: floor(base * (1 + stacks * 0.1)) */
    EACH_MOD(m, bt, p->is_player, PR_MOD_BASE_STAT) {
        if (!held_applies(m, p)) continue;
        base[m->stat] = floor(base[m->stat] * (1 + pr_mod_get_stack_count(m) * 0.1));
    }
}

/* applyModifier(PokemonIncrementingStatModifier, ...): the first one applies (apply is always true).
 * Upstream reads this.stackCount here, not getStackCount(). */
void pr_apply_incrementing_stat_modifier(pr_battle *bt, const pr_pokemon *p, int stat, double *value)
{
    pr_modifier *m;
    EACH_MOD(m, bt, p->is_player, PR_MOD_INCREMENTING_STAT) {
        if (!held_applies(m, p)) continue;
        if (stat == STAT_HP) {
            *value += 2 * m->stack_count;
            if (m->stack_count == 50) *value = floor(*value * 1.1);
        } else {
            *value += m->stack_count;
            if (m->stack_count == 50) *value = floor(*value * 1.05);
        }
        return;
    }
}

/* applyModifier(PokemonNatureWeightModifier, ...): apply is false when the nature is neutral
 * for the stat, so the search goes on to the next one */
void pr_apply_nature_weight_modifier(pr_battle *bt, const pr_pokemon *p, double *mult)
{
    pr_modifier *m;
    EACH_MOD(m, bt, p->is_player, PR_MOD_NATURE_WEIGHT) {
        if (!held_applies(m, p)) continue;
        if (*mult != 1) {
            *mult += 0.1 * pr_mod_get_stack_count(m) * (*mult > 1 ? 1 : -1);
            return;
        }
    }
}

/* Pokemon.isMax(): Gigantamax / Eternamax forms (fusion form keys not ported) */
static bool is_max(const pr_pokemon *p)
{
    const char *k = pr_pokemon_form(p)->form_key;
    return !strcmp(k, "gigantamax") || !strcmp(k, "gigantamax-rapid") || !strcmp(k, "gigantamax-single")
           || !strcmp(k, "eternamax");
}

static bool species_listed(const pr_modifier *m, int species)
{
    for (int i = 0; i < m->n_species; i++)
        if (m->species[i] == species) return true;
    return false;
}

/* applyModifiers(StatBoosterModifier, isPlayer, this, stat, statVal) in getEffectiveStat */
void pr_apply_stat_booster_modifiers(pr_battle *bt, const pr_pokemon *p, int stat, double *value)
{
    pr_modifier *m;
    EACH_MOD(m, bt, p->is_player, PR_MOD_STAT_BOOSTER) {
        if (!held_applies(m, p) || !((m->stats_mask >> stat) & 1)) continue;
        switch (m->cls) {
        case PR_MOD_EVOLUTION_STAT_BOOSTER:
            /* Eviolite: not on a Max form, and only while the species can still evolve
             * (the half boost for fusions comes with fusions) */
            if (is_max(p)) continue;
            if (pr_pokemon_species(p)->has_evolutions) *value *= m->mult;
            break;
        case PR_MOD_SPECIES_STAT_BOOSTER:
            /* getSpeciesForm(true).speciesId, or the fusion's (not ported) */
            if (!species_listed(m, p->species)) continue;
            *value *= m->mult;
            break;
        default:
            *value *= m->mult;
        }
    }
}

/* applyModifiers(TempStatStageBoosterModifier, isPlayer, stat, multiplier): side-wide, no Pokemon */
void pr_apply_temp_stat_stage_booster_modifiers(pr_battle *bt, bool player, int stat, double *mult)
{
    /* shouldApply: TEMP_BATTLE_STATS (ATK, DEF, SPATK, SPDEF, SPD, ACC) includes stat and it's this item's stat */
    if (stat < STAT_ATK || stat > STAT_ACC) return;
    pr_modifier *m;
    EACH_MOD(m, bt, player, PR_MOD_TEMP_STAT_STAGE_BOOSTER)
        if (m->stat == stat) *mult += m->mult;
}

/* applyModifiers(CritBoosterModifier, source.isPlayer(), source, critStage) */
void pr_apply_crit_booster_modifiers(pr_battle *bt, const pr_pokemon *src, int *stage)
{
    pr_modifier *m;
    EACH_MOD(m, bt, src->is_player, PR_MOD_CRIT_BOOSTER) {
        if (!held_applies(m, src)) continue;
        if (m->cls == PR_MOD_SPECIES_CRIT_BOOSTER && !species_listed(m, src->species)) continue;
        *stage += m->num;
    }
}

/* applyModifiers(TempCritBoosterModifier, source.isPlayer(), critStage): Dire Hit, +1 each */
void pr_apply_temp_crit_booster_modifiers(pr_battle *bt, bool player, int *stage)
{
    pr_modifier *m;
    EACH_MOD(m, bt, player, PR_MOD_TEMP_CRIT_BOOSTER) (*stage)++;
}

/* applyModifiers(AttackTypeBoosterModifier, source.isPlayer(), source, moveType, power) */
void pr_apply_attack_type_booster_modifiers(pr_battle *bt, const pr_pokemon *src, int move_type, double *power)
{
    pr_modifier *m;
    EACH_MOD(m, bt, src->is_player, PR_MOD_ATTACK_TYPE_BOOSTER) {
        if (!held_applies(m, src) || m->move_type != move_type) continue;
        if (*power >= 1) *power = floor(*power * (1 + pr_mod_get_stack_count(m) * m->mult));
    }
}

static int max_hp(const pr_pokemon *p) { return p->stats[STAT_HP]; }
static double to_dmg_value(double v) { double f = floor(v); return f < 1 ? 1 : f; }

void pr_apply_turn_heal_modifiers(pr_battle *bt, pr_engine *e, pr_pokemon *p)
{
    pr_modifier *m;
    EACH_MOD(m, bt, p->is_player, PR_MOD_TURN_HEAL) {
        if (!held_applies(m, p)) continue;
        if (p->hp > 0 && p->hp < max_hp(p)) {
            double amount = to_dmg_value(max_hp(p) / 16.0) * m->stack_count;
            pr_msg msg = pr_msg2("modifier:turnHealApply",
                                 pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                                 pr_arg_key("typeName", "modifierType:ModifierType.LEFTOVERS.name"));
            pr_unshift_heal_msg(bt, e, pr_battler_index(bt, p), amount, PR_HEAL_MESSAGE, msg);
        }
    }
}

void pr_apply_hit_heal_modifiers(pr_battle *bt, pr_engine *e, pr_pokemon *p)
{
    pr_modifier *m;
    EACH_MOD(m, bt, p->is_player, PR_MOD_HIT_HEAL) {
        if (!held_applies(m, p)) continue;
        if (p->hp > 0 && p->total_damage_dealt > 0 && p->hp < max_hp(p)) {
            double amount = to_dmg_value((p->total_damage_dealt * m->stack_count) / 8.0);
            pr_msg msg = pr_msg2("modifier:hitHealApply",
                                 pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                                 pr_arg_key("typeName", "modifierType:ModifierType.SHELL_BELL.name"));
            pr_unshift_heal_msg(bt, e, pr_battler_index(bt, p), amount, PR_HEAL_MESSAGE, msg);
        }
    }
}

double pr_calculate_shell_bell_recovery(pr_battle *bt, const pr_pokemon *p)
{
    pr_modifier *m;
    EACH_MOD(m, bt, p->is_player, PR_MOD_HIT_HEAL) {
        if (!held_applies(m, p)) continue;
        return to_dmg_value((p->total_damage_dealt * m->stack_count) / 8.0);
    }
    return 0;
}

/* getMoveTargets(user, move) as far as canBeMultiStrikeEnhanced reads it: whether the move is
 * a spread move ("multiple") with more than one active target. */
static bool spread_with_several_targets(pr_battle *bt, int move, const pr_pokemon *user)
{
    /* applyMoveAttrs("VariableTargetAttr") can change the target first (Expanding Force...) */
    if (pr_hook_move(bt, (uint16_t)move, PR_AC_VariableTargetAttr)) return false;
    bool user_in = false, ally_in = false, opponents_in = false;
    switch (pr_moves[move].target) {
    case MOVE_TARGET_ALL_NEAR_OTHERS: case MOVE_TARGET_ALL_OTHERS:
        ally_in = opponents_in = true; break;
    case MOVE_TARGET_ALL_NEAR_ENEMIES: case MOVE_TARGET_ALL_ENEMIES: case MOVE_TARGET_ENEMY_SIDE:
        opponents_in = true; break;
    case MOVE_TARGET_USER_AND_ALLIES: case MOVE_TARGET_USER_SIDE:
        user_in = ally_in = true; break;
    case MOVE_TARGET_ALL: case MOVE_TARGET_BOTH_SIDES:
        user_in = ally_in = opponents_in = true; break;
    default:
        return false; /* not "multiple" (RANDOM_NEAR_ENEMY's roll only happens with 2+ opponents: doubles) */
    }
    /* the set, filtered by isActive(true): on the field and not fainted */
    int n = 0;
    const pr_pokemon *ally = pr_get_ally(bt, user);
    for (int i = 0; i < 4; i++) {
        const pr_pokemon *p = bt->field[i];
        if (!p || p->hp <= 0) continue;
        if ((p == user && user_in) || (p == ally && ally && ally_in) || (pr_is_opponent(bt, user, p) && opponents_in)) n++;
    }
    return n > 1;
}

bool pr_move_can_be_multi_strike_enhanced_ex(pr_battle *bt, int move, const pr_pokemon *user, bool restrict_spread,
                                             const pr_pokemon *target)
{
    const pr_move *mv = &pr_moves[move];
    if (!strcmp(mv->kind, "ChargingAttackMove") || !strcmp(mv->kind, "ChargingSelfStatusMove")) return false;
    if (restrict_spread && spread_with_several_targets(bt, move, user)) return false;
    if (mv->category == MOVE_CATEGORY_STATUS) return false;
    if (pr_move_has_attr((uint16_t)move, PR_AC_MultiHitAttr) || pr_move_has_attr((uint16_t)move, PR_AC_SacrificialAttr)
        || pr_move_has_attr((uint16_t)move, PR_AC_SacrificialAttrOnHit))
        return false;
    if (target && pr_move_has_attr((uint16_t)move, PR_AC_HealOnAllyAttr) && pr_get_ally(bt, target) == user) return false;
    return move != MOVE_ID_FLING && move != MOVE_ID_UPROAR && move != MOVE_ID_ROLLOUT && move != MOVE_ID_ICE_BALL
           && move != MOVE_ID_ENDEAVOR;
}

bool pr_move_can_be_multi_strike_enhanced(int move)
{
    return pr_move_can_be_multi_strike_enhanced_ex(NULL, move, NULL, false, NULL);
}

/* PokemonMultiHitModifier.apply(pokemon, moveId, null, damageMultiplier) */
void pr_apply_multi_hit_damage_modifiers(pr_battle *bt, const pr_pokemon *src, int move, double *mult)
{
    pr_modifier *m;
    EACH_MOD(m, bt, src->is_player, PR_MOD_MULTI_HIT) {
        if (!held_applies(m, src)) continue;
        if (!pr_move_can_be_multi_strike_enhanced(move)) continue;
        int32_t stacks = pr_mod_get_stack_count(m);
        if (src->hits_left == src->hit_count) *mult *= 1 - 0.25 * stacks;   /* first hit */
        else if (src->hit_count - src->hits_left != stacks + 1) *mult *= 0.25; /* a Multi Lens hit */
        /* else: an extra hit not from the lens (Parental Bond) */
    }
}

bool pr_find_player_multi_lens(pr_battle *bt, const pr_pokemon *p)
{
    for (int i = 0; i < bt->n_mods[0]; i++) {
        const pr_modifier *m = &bt->mods[0][i];
        if (pr_mod_is(m, PR_MOD_MULTI_HIT) && m->pokemon_id == (int64_t)p->id) return true;
    }
    return false;
}

/* EnemyDamageMultiplierModifier.apply: toDmgValue(value * damageMultiplier ** stacks) */
static void enemy_damage_multiplier(pr_battle *bt, int cls, double *damage)
{
    pr_modifier *m;
    EACH_MOD(m, bt, false, cls) {
        double v = floor(*damage * js_pow(m->mult, pr_mod_get_stack_count(m)));
        *damage = v > 1 ? v : 1;
    }
}

void pr_apply_enemy_damage_booster_modifiers(pr_battle *bt, double *damage)
{
    enemy_damage_multiplier(bt, PR_MOD_ENEMY_DAMAGE_BOOSTER, damage);
}

void pr_apply_enemy_damage_reducer_modifiers(pr_battle *bt, double *damage)
{
    enemy_damage_multiplier(bt, PR_MOD_ENEMY_DAMAGE_REDUCER, damage);
}
