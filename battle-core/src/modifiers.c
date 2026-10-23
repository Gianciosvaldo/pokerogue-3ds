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
    [PR_MOD_EVO_TRACKER] = PR_MOD_HELD_ITEM,
    [PR_MOD_EXP_BOOSTER] = PR_MOD_HELD_ITEM,
    [PR_MOD_FRIENDSHIP_BOOSTER] = PR_MOD_HELD_ITEM,
    [PR_MOD_SWITCH_EFFECT_TRANSFER] = PR_MOD_HELD_ITEM,
    [PR_MOD_CONTACT_HELD_TRANSFER] = PR_MOD_HELD_ITEM,
    [PR_MOD_TURN_HELD_TRANSFER] = PR_MOD_HELD_ITEM,
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
    [PR_MOD_ENEMY_TURN_HEAL] = PR_MOD_ENEMY_PERSISTENT,
    [PR_MOD_ENEMY_ATTACK_STATUS_EFFECT_CHANCE] = PR_MOD_ENEMY_PERSISTENT,
    [PR_MOD_ENEMY_STATUS_EFFECT_HEAL_CHANCE] = PR_MOD_ENEMY_PERSISTENT,
    [PR_MOD_ENEMY_ENDURE_CHANCE] = PR_MOD_ENEMY_PERSISTENT,
    [PR_MOD_ENEMY_FUSED_CHANCE] = PR_MOD_ENEMY_PERSISTENT,
    [PR_MOD_SURVIVE_DAMAGE] = PR_MOD_HELD_ITEM,
    [PR_MOD_BYPASS_SPEED_CHANCE] = PR_MOD_HELD_ITEM,
    [PR_MOD_FLINCH_CHANCE] = PR_MOD_HELD_ITEM,
    [PR_MOD_RESET_NEGATIVE_STAT_STAGE] = PR_MOD_HELD_ITEM,
    [PR_MOD_BERRY] = PR_MOD_HELD_ITEM,
    [PR_MOD_TURN_STATUS_EFFECT] = PR_MOD_HELD_ITEM,
    [PR_MOD_MOVE_ACCURACY_BOOSTER] = PR_MOD_HELD_ITEM,
    [PR_MOD_INSTANT_REVIVE] = PR_MOD_HELD_ITEM,
    [PR_MOD_DAMAGE_MONEY_REWARD] = PR_MOD_HELD_ITEM,
    [PR_MOD_MONEY_MULTIPLIER] = PR_MOD_PERSISTENT,
    [PR_MOD_PRESERVE_BERRY] = PR_MOD_PERSISTENT,
};

const char *const pr_mod_class_name[PR_MOD_COUNT_] = {
    [PR_MOD_EVO_TRACKER] = "EvoTrackerModifier",
    [PR_MOD_EXP_BOOSTER] = "PokemonExpBoosterModifier",
    [PR_MOD_FRIENDSHIP_BOOSTER] = "PokemonFriendshipBoosterModifier",
    [PR_MOD_SWITCH_EFFECT_TRANSFER] = "SwitchEffectTransferModifier",
    [PR_MOD_CONTACT_HELD_TRANSFER] = "ContactHeldItemTransferChanceModifier",
    [PR_MOD_TURN_HELD_TRANSFER] = "TurnHeldItemTransferModifier",
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
    [PR_MOD_ENEMY_TURN_HEAL] = "EnemyTurnHealModifier",
    [PR_MOD_ENEMY_ATTACK_STATUS_EFFECT_CHANCE] = "EnemyAttackStatusEffectChanceModifier",
    [PR_MOD_ENEMY_STATUS_EFFECT_HEAL_CHANCE] = "EnemyStatusEffectHealChanceModifier",
    [PR_MOD_ENEMY_ENDURE_CHANCE] = "EnemyEndureChanceModifier",
    [PR_MOD_ENEMY_FUSED_CHANCE] = "EnemyFusionChanceModifier",
    [PR_MOD_SURVIVE_DAMAGE] = "SurviveDamageModifier",
    [PR_MOD_BYPASS_SPEED_CHANCE] = "BypassSpeedChanceModifier",
    [PR_MOD_FLINCH_CHANCE] = "FlinchChanceModifier",
    [PR_MOD_RESET_NEGATIVE_STAT_STAGE] = "ResetNegativeStatStageModifier",
    [PR_MOD_BERRY] = "BerryModifier",
    [PR_MOD_TURN_STATUS_EFFECT] = "TurnStatusEffectModifier",
    [PR_MOD_MOVE_ACCURACY_BOOSTER] = "PokemonMoveAccuracyBoosterModifier",
    [PR_MOD_INSTANT_REVIVE] = "PokemonInstantReviveModifier",
    [PR_MOD_DAMAGE_MONEY_REWARD] = "DamageMoneyRewardModifier",
    [PR_MOD_MONEY_MULTIPLIER] = "MoneyMultiplierModifier",
    [PR_MOD_PRESERVE_BERRY] = "PreserveBerryModifier",
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

bool pr_mod_is_ported_item(const pr_modifier *m)
{
    if (!m) return false;
    if (m->cls == PR_MOD_CONTACT_HELD_TRANSFER)
        return !m->unknown_subclass && m->mult==0.1 && m->stack_count>=1
            && m->virtual_stack_count>=0 && (int64_t)m->stack_count+m->virtual_stack_count<=5;
    if (m->cls == PR_MOD_SWITCH_EFFECT_TRANSFER) return !m->unknown_subclass && m->stack_count==1 && m->virtual_stack_count==0;
    if (m->cls == PR_MOD_TURN_HELD_TRANSFER) return !m->unknown_subclass && m->stack_count==1 && m->virtual_stack_count==0;
    if (m->cls == PR_MOD_EVO_TRACKER)
        return !m->unknown_subclass && !m->is_transferable && m->n_species == 1
            && m->species[0] == SPECIES_ID_GIMMIGHOUL && m->num == 10
            && m->stack_count >= 1 && m->virtual_stack_count >= 0
            && (int64_t)m->stack_count + m->virtual_stack_count <= 999;
    if (m->cls == PR_MOD_BASE_STAT_TOTAL)
        return !m->unknown_subclass && m->pokemon_id >= 0 && m->stack_count >= 1 && m->stack_count <= 2
            && m->virtual_stack_count == 0 && (m->num == 10 || m->num == -15);
    if (m->cls == PR_MOD_BASE_STAT_FLAT)
        return !m->unknown_subclass && m->pokemon_id >= 0 && m->stack_count == 1 && m->virtual_stack_count == 0;
    if (m->cls == PR_MOD_BASE_STAT) {
        int64_t stacks = (int64_t)m->stack_count + m->virtual_stack_count;
        return !m->unknown_subclass && m->pokemon_id >= 0 && m->stack_count >= 1 && m->virtual_stack_count >= 0
            && stacks <= 31 && m->stat >= STAT_HP && m->stat <= STAT_SPD;
    }
    if (m->cls == PR_MOD_PRESERVE_BERRY) return !m->unknown_subclass && m->stack_count >= 0 && m->virtual_stack_count >= 0 && (int64_t)m->stack_count + m->virtual_stack_count >= 1 && (int64_t)m->stack_count + m->virtual_stack_count <= 3;
    if (m->cls == PR_MOD_BERRY) {
        return m->berry_type == BERRY_TYPE_SITRUS
            || m->berry_type == BERRY_TYPE_LUM
            || m->berry_type == BERRY_TYPE_LEPPA
            || m->berry_type == BERRY_TYPE_LIECHI
            || m->berry_type == BERRY_TYPE_GANLON
            || m->berry_type == BERRY_TYPE_PETAYA
            || m->berry_type == BERRY_TYPE_APICOT
            || m->berry_type == BERRY_TYPE_SALAC
            || m->berry_type == BERRY_TYPE_ENIGMA
            || m->berry_type == BERRY_TYPE_LANSAT
            || m->berry_type == BERRY_TYPE_STARF;
    }
    if (m->cls == PR_MOD_DAMAGE_MONEY_REWARD || m->cls == PR_MOD_MONEY_MULTIPLIER) {
        int64_t stacks = (int64_t)m->stack_count + m->virtual_stack_count;
        return !m->unknown_subclass && m->stack_count >= 1 && m->virtual_stack_count >= 0 && stacks <= 5;
    }
    if (m->cls == PR_MOD_TEMP_STAT_STAGE_BOOSTER || m->cls == PR_MOD_TEMP_CRIT_BOOSTER) {
        if (m->unknown_subclass || m->stack_count<1 || m->stack_count>2
            || m->virtual_stack_count!=0 || m->pokemon_id!=0) return false;
        if (m->cls == PR_MOD_TEMP_CRIT_BOOSTER) return m->stat==0;
        return m->stat>=STAT_ATK && m->stat<=STAT_ACC && m->mult==(m->stat==STAT_ACC?1.0:0.2);
    }
    if(m->cls==PR_MOD_EXP_BOOSTER||m->cls==PR_MOD_FRIENDSHIP_BOOSTER){
        int64_t stacks=(int64_t)m->stack_count+m->virtual_stack_count;
        return !m->unknown_subclass && m->stack_count>=1 && m->virtual_stack_count>=0
            && stacks<=(m->cls==PR_MOD_EXP_BOOSTER?99:3)
            && (m->cls==PR_MOD_FRIENDSHIP_BOOSTER || m->mult==0.4 || m->mult==1.0);
    }
    if (m->cls == PR_MOD_FIELD_EFFECT) {
        int64_t stacks=(int64_t)m->stack_count+m->virtual_stack_count;
        return !m->unknown_subclass && m->stack_count>=1 && m->virtual_stack_count>=0 && stacks<=2;
    }
    if (m->cls == PR_MOD_STAT_BOOSTER || m->cls == PR_MOD_EVOLUTION_STAT_BOOSTER || m->cls == PR_MOD_SPECIES_STAT_BOOSTER) {
        if (m->unknown_subclass || m->stack_count != 1 || m->virtual_stack_count != 0) return false;
        if (m->cls == PR_MOD_STAT_BOOSTER || m->cls == PR_MOD_EVOLUTION_STAT_BOOSTER)
            return m->mult == 1.5 && m->stats_mask == ((1u << STAT_DEF) | (1u << STAT_SPDEF)) && m->n_species == 0;
        if (m->mult != 2) return false;
        if (m->n_species == 1 && m->species[0] == SPECIES_ID_PIKACHU)
            return m->stats_mask == ((1u << STAT_ATK) | (1u << STAT_SPATK));
        if (m->n_species == 3 && m->species[0] == SPECIES_ID_CUBONE && m->species[1] == SPECIES_ID_MAROWAK
            && m->species[2] == SPECIES_ID_ALOLA_MAROWAK) return m->stats_mask == (1u << STAT_ATK);
        if (m->n_species == 1 && m->species[0] == SPECIES_ID_DITTO)
            return m->stats_mask == (1u << STAT_DEF) || m->stats_mask == (1u << STAT_SPD);
        if (m->n_species == 1 && m->species[0] == SPECIES_ID_CLAMPERL)
            return m->stats_mask == (1u << STAT_SPDEF) || m->stats_mask == (1u << STAT_SPATK);
        return false;
    }
    if (m->cls == PR_MOD_CRIT_BOOSTER || m->cls == PR_MOD_SPECIES_CRIT_BOOSTER) {
        if (m->unknown_subclass || m->stack_count != 1 || m->virtual_stack_count != 0) return false;
        if (m->cls == PR_MOD_CRIT_BOOSTER) return m->num == 1 && m->n_species == 0;
        return m->num == 2 && m->n_species == 3 && m->species[0] == SPECIES_ID_FARFETCHD
            && m->species[1] == SPECIES_ID_GALAR_FARFETCHD && m->species[2] == SPECIES_ID_SIRFETCHD;
    }
    if (m->cls == PR_MOD_ATTACK_TYPE_BOOSTER) {
        int64_t stacks = (int64_t)m->stack_count + m->virtual_stack_count;
        return !m->unknown_subclass && m->stack_count >= 1 && m->virtual_stack_count >= 0 && stacks <= 99
            && m->move_type >= 0 && m->move_type < 18 && m->mult == 20 * 0.01;
    }
    if (m->cls == PR_MOD_NATURE_WEIGHT) {
        int64_t stacks = (int64_t)m->stack_count + m->virtual_stack_count;
        return !m->unknown_subclass && m->stack_count >= 1 && m->virtual_stack_count >= 0 && stacks <= 10;
    }
    if (m->cls == PR_MOD_MULTI_HIT)
        return !m->unknown_subclass;
    if (m->cls == PR_MOD_INSTANT_REVIVE)
        return !m->unknown_subclass;
    if (m->cls == PR_MOD_MOVE_ACCURACY_BOOSTER)
        return !m->unknown_subclass && m->num == 5;
    if (m->cls == PR_MOD_TURN_STATUS_EFFECT)
        return !m->unknown_subclass && (m->status_effect == STATUS_EFFECT_TOXIC || m->status_effect == STATUS_EFFECT_BURN);
    int cls = m->cls;
    return cls == PR_MOD_TURN_HEAL || cls == PR_MOD_HIT_HEAL
        || cls == PR_MOD_SURVIVE_DAMAGE || cls == PR_MOD_BYPASS_SPEED_CHANCE
        || cls == PR_MOD_FLINCH_CHANCE || cls == PR_MOD_RESET_NEGATIVE_STAT_STAGE
        || cls == PR_MOD_ENEMY_DAMAGE_BOOSTER || cls == PR_MOD_ENEMY_DAMAGE_REDUCER
        || cls == PR_MOD_ENEMY_TURN_HEAL
        || cls == PR_MOD_ENEMY_ATTACK_STATUS_EFFECT_CHANCE
        || cls == PR_MOD_ENEMY_STATUS_EFFECT_HEAL_CHANCE
        || cls == PR_MOD_ENEMY_ENDURE_CHANCE
        || cls == PR_MOD_ENEMY_FUSED_CHANCE;
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

void pr_apply_base_stat_before_fusion(pr_battle *bt, const pr_pokemon *p, double base[6])
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
}

void pr_apply_base_stat_after_fusion(pr_battle *bt, const pr_pokemon *p, double base[6])
{
    pr_modifier *m;
    /* Vitamins: floor(base * (1 + stacks * 0.1)) */
    EACH_MOD(m, bt, p->is_player, PR_MOD_BASE_STAT) {
        if (!held_applies(m, p)) continue;
        base[m->stat] = floor(base[m->stat] * (1 + pr_mod_get_stack_count(m) * 0.1));
    }
}

/* Retain the combined raw projection API for callers without a fusion step. */
void pr_apply_base_stat_modifiers(pr_battle *bt, const pr_pokemon *p, double base[6])
{
    pr_apply_base_stat_before_fusion(bt, p, base);
    pr_apply_base_stat_after_fusion(bt, p, base);
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
static void apply_nature_weight_modifier(pr_battle *bt, const pr_pokemon *p, double *mult, bool bounded)
{
    if (!p || !mult) return;
    pr_modifier *m;
    EACH_MOD(m, bt, p->is_player, PR_MOD_NATURE_WEIGHT) {
        if (!held_applies(m, p)) continue;
        if (bounded && !pr_mod_is_ported_item(m)) { pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS); return; }
        if (*mult != 1) {
            *mult += 0.1 * pr_mod_get_stack_count(m) * (*mult > 1 ? 1 : -1);
            return;
        }
    }
}

/* Strict leaf admission and raw upstream projection have separate contracts.
 * Raw callers preserve the pinned helper arithmetic even for synthetic inventory
 * states outside acquisition caps; full turns flag those states at admission. */
void pr_apply_nature_weight_modifier(pr_battle *bt, const pr_pokemon *p, double *mult)
{
    apply_nature_weight_modifier(bt, p, mult, true);
}

void pr_project_nature_weight_modifier(pr_battle *bt, const pr_pokemon *p, double *mult)
{
    apply_nature_weight_modifier(bt, p, mult, false);
}

void pr_apply_enemy_status_heal_modifiers(pr_battle *bt, pr_engine *e, pr_pokemon *p)
{
    if (!p || p->is_player || !p->status) return;
    pr_modifier *m;
    EACH_MOD(m, bt, false, PR_MOD_ENEMY_STATUS_EFFECT_HEAL_CHANCE) {
        double chance = 0.025 * pr_mod_get_stack_count(m);
        if (pr_rng_seed_float(&bt->rng) <= chance) {
            pr_queue_msg(bt, e, pr_status_msg(bt, p->status, PR_ST_HEAL, p));
            pr_reset_status_phase(bt, e, p, false);
            return;
        }
    }
}

void pr_apply_enemy_attack_status_modifiers(pr_battle *bt, pr_engine *e, pr_pokemon *user, pr_pokemon *target)
{
    if (!user || user->is_player || !target) return;
    pr_modifier *eligible[16], *m;
    int n = 0;
    EACH_MOD(m, bt, false, PR_MOD_ENEMY_ATTACK_STATUS_EFFECT_CHANCE) {
        if (n < 16) eligible[n++] = m;
    }
    if (n == 0) return;

    /* applyShuffledModifiers: Fisher-Yates shuffle using randSeedInt(len) */
    for (int i = n - 1; i > 0; i--) {
        int r = pr_battle_rand_seed_int(bt, i + 1, 0);
        pr_modifier *tmp = eligible[i];
        eligible[i] = eligible[r];
        eligible[r] = tmp;
    }

    for (int i = 0; i < n; i++) {
        pr_modifier *m = eligible[i];
        int status = m->berry_type; /* effect stored in berry_type */
        double chance = 0.025 * (status == STATUS_EFFECT_BURN || status == STATUS_EFFECT_POISON ? 2.0 : 1.0);
        if (pr_rng_seed_float(&bt->rng) <= chance * pr_mod_get_stack_count(m)) {
            if (pr_try_set_status(bt, e, target, status, user, -1, false, true)) {
                return;
            }
        }
    }
}

void pr_apply_enemy_endure_modifiers(pr_battle *bt, pr_engine *e, pr_pokemon *target)
{
    if (!target || target->is_player || target->wave_endured) return;
    pr_modifier *m;
    EACH_MOD(m, bt, false, PR_MOD_ENEMY_ENDURE_CHANCE) {
        int chance = 2 * pr_mod_get_stack_count(m);
        if (pr_battle_rand_seed_int(bt, 100, 0) < chance) {
            pr_add_tag(bt, e, target, BATTLER_TAG_TYPE_ENDURE_TOKEN, 1, 0, -1);
            target->wave_endured = true;
            return;
        }
    }
}

static const pr_species *held_species(int id)
{
    int lo=0,hi=PR_SPECIES_COUNT-1;
    while(lo<=hi){int mid=(lo+hi)/2;if(pr_species_table[mid].id==id)return &pr_species_table[mid];if(pr_species_table[mid].id<id)lo=mid+1;else hi=mid-1;}
    return NULL;
}
static bool max_key(const char *k)
{
    return !strcmp(k,"gigantamax")||!strcmp(k,"gigantamax-rapid")||!strcmp(k,"gigantamax-single")||!strcmp(k,"eternamax");
}
/* Pokemon.isMax(): either component's original form key. */
static bool is_max(const pr_pokemon *p)
{
    if(max_key(pr_pokemon_base_form(p)->form_key))return true;
    const pr_species *fusion=p->fusion_species?held_species(p->fusion_species):NULL;
    return fusion&&p->fusion_form<fusion->nforms&&max_key(pr_forms[fusion->form0+p->fusion_form].form_key);
}

static bool species_listed(const pr_modifier *m, int species)
{
    for (int i = 0; i < m->n_species; i++)
        if (m->species[i] == species) return true;
    return false;
}

static int max_hp(const pr_pokemon *p) { return p->stats[STAT_HP]; }
static double to_dmg_value(double v) { double f = floor(v); return f < 1 ? 1 : f; }

int pr_sitrus_berry_fired = 0, pr_lum_berry_fired = 0, pr_leppa_berry_fired = 0;
int pr_liechi_berry_fired = 0, pr_ganlon_berry_fired = 0, pr_petaya_berry_fired = 0, pr_apicot_berry_fired = 0, pr_salac_berry_fired = 0;
int pr_enigma_berry_fired = 0, pr_lansat_berry_fired = 0, pr_starf_berry_fired = 0;

static int pr_pokemon_move_max_pp(const pr_pokemon *p, int s)
{
    if (s < 0 || s >= PR_N_MOVES(p)) return 0;
    int move = PR_MOVES(p)[s];
    if (move <= 0 || move >= PR_MOVE_COUNT) return 0;
    int ovr = PR_MAX_PP(p)[s];
    if (ovr >= 0) return ovr;
    int base = pr_moves[move].pp;
    if (base <= 0) return 0;
    return base + (base * PR_PP_UP(p)[s]) / 5;
}

static double get_hp_ratio(const pr_pokemon *p)
{
    return js_round(((double)p->hp / max_hp(p)) * 100.0) / 100.0;
}

bool pr_berry_should_apply(pr_battle *bt, const pr_pokemon *p, const pr_modifier *m)
{
    if (!m || m->cls != PR_MOD_BERRY) return false;
    if (!held_applies(m, p)) return false;

    switch (m->berry_type) {
    case BERRY_TYPE_SITRUS:
        return get_hp_ratio(p) < 0.5;
    case BERRY_TYPE_LUM:
        return p->status != STATUS_EFFECT_NONE || pr_has_battler_tag(bt, p, BATTLER_TAG_TYPE_CONFUSED);
    case BERRY_TYPE_ENIGMA: {
        int kept = pr_attacks_received_kept(p);
        for (int i = 0; i < kept; i++) {
            int res = p->attacks_received[i].result;
            if (res == HIT_RESULT_SUPER_EFFECTIVE || res == HIT_RESULT_EXTREMELY_EFFECTIVE)
                return true;
        }
        return false;
    }
    case BERRY_TYPE_LEPPA: {
        double hp_ratio_req = 0.25;
        pr_ab_params pp = {.pokemon = (pr_pokemon *)p, .move = -1, .value = &hp_ratio_req};
        pr_apply_ab_attrs((pr_battle *)bt, PR_AC_ReduceBerryUseThresholdAbAttr, &pp);
        for (int i = 0; i < PR_N_MOVES(p); i++) {
            int max_pp = pr_pokemon_move_max_pp(p, i);
            if (max_pp > 0 && PR_PP_USED(p)[i] >= max_pp) return true;
        }
        return false;
    }
    case BERRY_TYPE_LIECHI:
    case BERRY_TYPE_GANLON:
    case BERRY_TYPE_PETAYA:
    case BERRY_TYPE_APICOT:
    case BERRY_TYPE_SALAC: {
        double hp_ratio_req = 0.25;
        int stat = m->berry_type - BERRY_TYPE_ENIGMA;
        pr_ab_params pp = {.pokemon = (pr_pokemon *)p, .move = -1, .value = &hp_ratio_req};
        pr_engine *saved = bt->engine; bt->engine = NULL;
        pr_apply_ab_attrs((pr_battle *)bt, PR_AC_ReduceBerryUseThresholdAbAttr, &pp);
        bt->engine = saved;
        return get_hp_ratio(p) < hp_ratio_req && p->stat_stages[stat - 1] < 6;
    }
    case BERRY_TYPE_LANSAT: {
        double hp_ratio_req = 0.25;
        pr_ab_params pp = {.pokemon = (pr_pokemon *)p, .move = -1, .value = &hp_ratio_req};
        pr_engine *saved = bt->engine; bt->engine = NULL;
        pr_apply_ab_attrs((pr_battle *)bt, PR_AC_ReduceBerryUseThresholdAbAttr, &pp);
        bt->engine = saved;
        return get_hp_ratio(p) < 0.25 && !pr_has_battler_tag(bt, p, BATTLER_TAG_TYPE_CRIT_BOOST);
    }
    case BERRY_TYPE_STARF: {
        double hp_ratio_req = 0.25;
        pr_ab_params pp = {.pokemon = (pr_pokemon *)p, .move = -1, .value = &hp_ratio_req};
        pr_engine *saved = bt->engine; bt->engine = NULL;
        pr_apply_ab_attrs((pr_battle *)bt, PR_AC_ReduceBerryUseThresholdAbAttr, &pp);
        bt->engine = saved;
        return get_hp_ratio(p) < 0.25;
    }
    default:
        return false;
    }
}

void pr_apply_berry(pr_battle *bt, pr_engine *e, pr_pokemon *p, pr_modifier *m)
{
    pr_apply_berry_ex(bt, e, p, m, NULL, NULL, NULL);
}

void pr_apply_berry_ex(pr_battle *bt, pr_engine *e, pr_pokemon *p, pr_modifier *m, int8_t *ssc_stats, int8_t *ssc_stages, int *n_ssc)
{
    if (!m || m->cls != PR_MOD_BERRY) return;
    const int btype = m->berry_type;
    bool preserve = false;
    const int side = p->is_player ? 0 : 1;
    for (int i = 0; i < bt->n_mods[side]; i++) {
        const pr_modifier *pouch = &bt->mods[side][i];
        if (pouch->cls != PR_MOD_PRESERVE_BERRY) continue;
        if (!pr_mod_is_ported_item(pouch)) { pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS); continue; }
        if (!preserve) preserve = pr_battle_rand_seed_int(bt, 10, 0) < pr_mod_get_stack_count(pouch) * 3;
    }
    pr_berry_effect(bt, e, p, btype, ssc_stats, ssc_stages, n_ssc);
    pr_ab_params pp = {.pokemon = p, .move = -1};
    pr_apply_ab_attrs(bt, PR_AC_PostItemLostAbAttr, &pp);
    pr_record_eaten_berry(bt, p, btype, !preserve);
    if (!preserve) pr_lose_held_item(bt, e, p, m);
}

/* BattleScene.updateModifiers at the berry boundaries: no item-loss callbacks.
 * Unknown item families retain explicit unsupported reporting. */
void pr_refresh_berry_modifiers(pr_battle *bt, int side)
{
    int w = 0;
    for (int i = 0; i < bt->n_mods[side]; i++) {
        pr_modifier m = bt->mods[side][i];
        if (pr_mod_is(&m, PR_MOD_HELD_ITEM) && !pr_pokemon_by_id(bt, m.pokemon_id)) continue;
        m.virtual_stack_count = 0;
        if (m.stack_count == 0) continue;
        if (!pr_mod_is_ported_item(&m)) pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS);
        bt->mods[side][w++] = m;
    }
    bt->n_mods[side] = w;
    if (!bt->n_party[side]) {pr_note_unported(bt, PR_FEAT_PARTY); return;}
    for (int k = 0; k < bt->n_party[side]; k++)
        if (bt->party[side][k]) pr_recalculate_stats(bt, bt->party[side][k]);
}

void pr_reset_wave_data(pr_pokemon *p)
{
    p->n_ab_applied_wave=0;
    p->temp_wave_turn_count=1;
    p->wave_endured=false;
}

void pr_reset_battle_data(pr_pokemon *p)
{
    pr_reset_wave_data(p);
    p->has_eaten_berry = false;
    p->zero_to_hero_shown = false;
    p->n_battle_berries = 0;
}

/* Stuff Cheeks' native restriction scans all BerryModifiers on the side,
 * including another holder's stock. It does not ask shouldApply or getStackCount. */
bool pr_side_has_berry(const pr_battle *bt,bool player)
{
    int side=player?0:1;
    for(int k=0;k<bt->n_mods[side];k++)if(pr_mod_is(&bt->mods[side][k],PR_MOD_BERRY))return true;
    return false;
}

void pr_record_eaten_berry(pr_battle *bt, pr_pokemon *p, int berry, bool consumed)
{
    p->has_eaten_berry = true;
    if (consumed) {
        if (p->n_battle_berries >= PR_MAX_BERRY_HISTORY) pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT);
        else p->battle_berries[p->n_battle_berries++] = (uint8_t)berry;
    }
    if (p->n_turn_berries >= PR_MAX_BERRY_HISTORY) pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT);
    else p->turn_berries[p->n_turn_berries++] = (uint8_t)berry;
}

void pr_berry_effect(pr_battle *bt, pr_engine *e, pr_pokemon *p, int btype, int8_t *ssc_stats, int8_t *ssc_stages, int *n_ssc)
{
    switch (btype) {
    case BERRY_TYPE_SITRUS:
    case BERRY_TYPE_ENIGMA: {
        if (btype == BERRY_TYPE_SITRUS) pr_sitrus_berry_fired++;
        else pr_enigma_berry_fired++;

        double amount = to_dmg_value(max_hp(p) / 4.0);
        pr_ab_params rp = {.pokemon = p, .move = -1, .value = &amount};
        pr_apply_ab_attrs(bt, PR_AC_DoubleBerryEffectAbAttr, &rp);
        const char *key = btype == BERRY_TYPE_SITRUS ? "berry:sitrus.name" : "berry:enigma.name";
        pr_msg msg = pr_msg2("battle:hpHealBerry",
                             pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                             pr_arg_key("berryName", key));
        pr_unshift_heal_msg(bt, e, pr_battler_index(bt, p), amount, PR_HEAL_MESSAGE, msg);
        break;
    }
    case BERRY_TYPE_LUM: {
        pr_lum_berry_fired++;
        if (p->status != STATUS_EFFECT_NONE) {
            pr_queue_msg(bt, e, pr_status_msg(bt, p->status, PR_ST_HEAL, p));
        }
        pr_reset_status_phase_ex(bt, e, p, true, true);
        break;
    }
    case BERRY_TYPE_LEPPA: {
        pr_leppa_berry_fired++;
        int slot = -1;
        for (int i = 0; i < PR_N_MOVES(p); i++) {
            int max_pp = pr_pokemon_move_max_pp(p, i);
            if (PR_PP_USED(p)[i] == max_pp) {
                slot = i;
                break;
            }
        }
        if (slot < 0) {
            for (int i = 0; i < PR_N_MOVES(p); i++) {
                int max_pp = pr_pokemon_move_max_pp(p, i);
                if (PR_PP_USED(p)[i] < max_pp) {
                    slot = i;
                    break;
                }
            }
        }
        if (slot >= 0) {
            PR_PP_USED(p)[slot] = PR_PP_USED(p)[slot] > 10 ? PR_PP_USED(p)[slot] - 10 : 0;
            pr_sync_moveset_aliases(p);
            pr_msg msg = pr_msg3("battle:ppHealBerry",
                                 pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                                 pr_arg_move("moveName", PR_MOVES(p)[slot]),
                                 pr_arg_key("berryName", "berry:leppa.name"));
            pr_queue_msg(bt, e, msg);
        }
        break;
    }
    case BERRY_TYPE_LIECHI:
    case BERRY_TYPE_GANLON:
    case BERRY_TYPE_PETAYA:
    case BERRY_TYPE_APICOT:
    case BERRY_TYPE_SALAC: {
        if (btype == BERRY_TYPE_LIECHI) pr_liechi_berry_fired++;
        else if (btype == BERRY_TYPE_GANLON) pr_ganlon_berry_fired++;
        else if (btype == BERRY_TYPE_PETAYA) pr_petaya_berry_fired++;
        else if (btype == BERRY_TYPE_APICOT) pr_apicot_berry_fired++;
        else if (btype == BERRY_TYPE_SALAC) pr_salac_berry_fired++;

        int stat = btype - BERRY_TYPE_ENIGMA;
        double amount = 1.0;
        pr_ab_params rp = {.pokemon = p, .move = -1, .value = &amount};
        pr_apply_ab_attrs(bt, PR_AC_DoubleBerryEffectAbAttr, &rp);
        int8_t stages = (int8_t)amount;

        if (ssc_stats != NULL && ssc_stages != NULL && n_ssc != NULL) {
            int idx = -1;
            for (int k = 0; k < *n_ssc; k++) {
                if (ssc_stats[k] == (int8_t)stat) {
                    idx = k;
                    break;
                }
            }
            if (idx >= 0) {
                ssc_stages[idx] += stages;
            } else if (*n_ssc < 7) {
                ssc_stats[*n_ssc] = (int8_t)stat;
                ssc_stages[*n_ssc] = stages;
                (*n_ssc)++;
            }
        } else {
            int8_t st = (int8_t)stat;
            pr_unshift_stat_stage_change_ex(bt, e, pr_battler_index(bt, p), &st, &stages, 1, p, 0);
        }
        break;
    }
    case BERRY_TYPE_LANSAT: {
        pr_lansat_berry_fired++;
        pr_add_tag(bt, e, p, BATTLER_TAG_TYPE_CRIT_BOOST, 0, 0, -1);
        break;
    }
    case BERRY_TYPE_STARF: {
        pr_starf_berry_fired++;
        int rand_stat = pr_rng_seed_int(&bt->rng, 5, STAT_ATK);
        double amount = 2.0;
        pr_ab_params rp = {.pokemon = p, .move = -1, .value = &amount};
        pr_apply_ab_attrs(bt, PR_AC_DoubleBerryEffectAbAttr, &rp);
        int8_t stages = (int8_t)amount;

        if (ssc_stats != NULL && ssc_stages != NULL && n_ssc != NULL) {
            int idx = -1;
            for (int k = 0; k < *n_ssc; k++) {
                if (ssc_stats[k] == (int8_t)rand_stat) {
                    idx = k;
                    break;
                }
            }
            if (idx >= 0) {
                ssc_stages[idx] += stages;
            } else if (*n_ssc < 7) {
                ssc_stats[*n_ssc] = (int8_t)rand_stat;
                ssc_stages[*n_ssc] = stages;
                (*n_ssc)++;
            }
        } else {
            int8_t st = (int8_t)rand_stat;
            pr_unshift_stat_stage_change_ex(bt, e, pr_battler_index(bt, p), &st, &stages, 1, p, 0);
        }
        break;
    }
    default:
        break;
    }
}

/* applyModifiers(StatBoosterModifier, isPlayer, this, stat, statVal) in getEffectiveStat */
void pr_apply_stat_booster_modifiers(pr_battle *bt, const pr_pokemon *p, int stat, double *value)
{
    pr_modifier *m;
    EACH_MOD(m, bt, p->is_player, PR_MOD_STAT_BOOSTER) {
        if (!held_applies(m, p) || !((m->stats_mask >> stat) & 1)) continue;
        switch (m->cls) {
        case PR_MOD_EVOLUTION_STAT_BOOSTER: {
            if (is_max(p)) continue;
            bool unevolved=pr_pokemon_species(p)->has_evolutions;
            const pr_species *fusion=p->fusion_species?held_species(p->fusion_species):NULL;
            if(p->fusion_species&&!fusion){pr_note_unported(bt,PR_FEAT_MODIFIERS);continue;}
            if(fusion&&fusion->has_evolutions!=unevolved)*value*=1+(m->mult-1)/2;
            else if(unevolved)*value*=m->mult;
            break;
        }
        case PR_MOD_SPECIES_STAT_BOOSTER:
            if (!species_listed(m,p->species)&&!(p->fusion_species&&species_listed(m,p->fusion_species)))continue;
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
static void apply_crit_booster_modifiers(pr_battle *bt, const pr_pokemon *src, int *stage, bool bounded)
{
    if (!src || !stage) return;
    pr_modifier *m;
    EACH_MOD(m, bt, src->is_player, PR_MOD_CRIT_BOOSTER) {
        if (!held_applies(m, src)) continue;
        if (bounded && !pr_mod_is_ported_item(m)) {
            pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS); continue;
        }
        if (m->cls==PR_MOD_SPECIES_CRIT_BOOSTER&&!species_listed(m,src->species)
            &&!(src->fusion_species&&species_listed(m,src->fusion_species)))continue;
        *stage += m->num;
    }
}

/* Strict leaf admission and raw upstream projection have separate contracts.
 * Raw callers preserve the pinned helper arithmetic even for synthetic inventory
 * states outside acquisition caps; full turns flag those states at admission. */
void pr_apply_crit_booster_modifiers(pr_battle *bt, const pr_pokemon *src, int *stage)
{
    apply_crit_booster_modifiers(bt, src, stage, true);
}

void pr_project_crit_booster_modifiers(pr_battle *bt, const pr_pokemon *src, int *stage)
{
    apply_crit_booster_modifiers(bt, src, stage, false);
}

/* applyModifiers(TempCritBoosterModifier, source.isPlayer(), critStage): Dire Hit, +1 each */
void pr_apply_temp_crit_booster_modifiers(pr_battle *bt, bool player, int *stage)
{
    pr_modifier *m;
    EACH_MOD(m, bt, player, PR_MOD_TEMP_CRIT_BOOSTER) (*stage)++;
}

/* applyModifiers(AttackTypeBoosterModifier, source.isPlayer(), source, moveType, power) */
static void apply_attack_type_booster_modifiers(pr_battle *bt, const pr_pokemon *src, int move_type, double *power, bool bounded)
{
    if (!src || !power) return;
    pr_modifier *m;
    EACH_MOD(m, bt, src->is_player, PR_MOD_ATTACK_TYPE_BOOSTER) {
        if (!held_applies(m, src) || m->move_type != move_type) continue;
        if (bounded && !pr_mod_is_ported_item(m)) { pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS); continue; }
        if (*power >= 1) *power = floor(*power * (1 + pr_mod_get_stack_count(m) * m->mult));
    }
}

/* Strict leaf admission and raw upstream projection have separate contracts.
 * Raw callers preserve the pinned helper arithmetic even for synthetic inventory
 * states outside acquisition caps; full turns flag those states at admission. */
void pr_apply_attack_type_booster_modifiers(pr_battle *bt, const pr_pokemon *src, int move_type, double *power)
{
    apply_attack_type_booster_modifiers(bt, src, move_type, power, true);
}

void pr_project_attack_type_booster_modifiers(pr_battle *bt, const pr_pokemon *src, int move_type, double *power)
{
    apply_attack_type_booster_modifiers(bt, src, move_type, power, false);
}

/* PokemonMoveAccuracyBoosterModifier: shouldApply requires the matching holder
 * and a NumberHolder; apply adds accuracyAmount * getStackCount(), without RNG.
 * Only the pinned Wide Lens identity (amount 5) is bounded here. */
void pr_apply_accuracy_booster_modifiers(pr_battle *bt, const pr_pokemon *p, double *accuracy)
{
    if (!accuracy) return;
    pr_modifier *m;
    EACH_MOD(m, bt, p->is_player, PR_MOD_MOVE_ACCURACY_BOOSTER) {
        if (!held_applies(m, p)) continue;
        if (!pr_mod_is_ported_item(m)) {
            pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS);
            continue;
        }
        *accuracy += m->num * pr_mod_get_stack_count(m);
    }
}

/* TurnStatusEffectModifier.apply: use the holder as source and the actual orb name.
 * trySetStatus owns rejection, pending status, ability callbacks and phase ordering.
 * Item stacks are retained; neither orb is consumed and there is no item RNG draw. */
void pr_apply_turn_status_modifiers(pr_battle *bt, pr_engine *e, pr_pokemon *p)
{
    pr_modifier *m;
    EACH_MOD(m, bt, p->is_player, PR_MOD_TURN_STATUS_EFFECT) {
        if (!held_applies(m, p)) continue;
        if (!pr_mod_is_ported_item(m)) {
            pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS);
            continue;
        }
        const char *name = m->status_effect == STATUS_EFFECT_TOXIC
            ? "modifierType:ModifierType.TOXIC_ORB.name" : "modifierType:ModifierType.FLAME_ORB.name";
        pr_msg msg = pr_msg2(pr_status_key(m->status_effect, PR_ST_OBTAIN_SOURCE),
                             pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                             pr_arg_key("sourceText", name));
        pr_try_set_status_msg(bt, e, p, m->status_effect, p, -1, false, true, &msg);
    }
}

void pr_apply_turn_heal_modifiers(pr_battle *bt, pr_engine *e, pr_pokemon *p)
{
    pr_modifier *m;
    EACH_MOD(m, bt, p->is_player, PR_MOD_TURN_HEAL) {
        if (!held_applies(m, p)) continue;
        if (p->hp < max_hp(p)) {
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
        if (p->total_damage_dealt > 0 && p->hp < max_hp(p)) {
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
    if (bt->double_battle) {                      /* the whole call (a RANDOM_NEAR_ENEMY move draws) */
        int8_t tg[4];
        bool multiple = false;
        int n = pr_get_move_targets(bt, user, move, -1, tg, &multiple);
        return multiple && n > 1;
    }
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

/* Golden Punch uses final damage, then each player's MoneyMultiplierModifier
 * sequentially floors its increment. addMoney clamps the IEEE754 sum at JS's
 * Number.MAX_SAFE_INTEGER; moneyScattered is a separate Pay Day balance. */
void pr_apply_damage_money_modifiers(pr_battle *bt, const pr_pokemon *p, double damage)
{
    if (!p || !p->is_player || !(damage > 0)) return;
    /* Refuse unsupported applicable items/dependencies before changing the wallet. */
    for (int i = 0; i < bt->n_mods[0]; i++) {
        const pr_modifier *m = &bt->mods[0][i];
        bool applicable = pr_mod_is(m, PR_MOD_MONEY_MULTIPLIER)
            || (pr_mod_is(m, PR_MOD_DAMAGE_MONEY_REWARD) && held_applies(m, p));
        if (applicable && !pr_mod_is_ported_item(m)) { pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS); return; }
    }
    pr_modifier *m;
    EACH_MOD(m, bt, true, PR_MOD_DAMAGE_MONEY_REWARD) {
        if (!held_applies(m, p)) continue;
        if (!pr_mod_is_ported_item(m)) { pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS); continue; }
        double amount = floor(damage * (0.5 * pr_mod_get_stack_count(m)));
        pr_modifier *coin;
        EACH_MOD(coin, bt, true, PR_MOD_MONEY_MULTIPLIER) {
            if (!pr_mod_is_ported_item(coin)) { pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS); continue; }
            amount += floor(amount * 0.2 * pr_mod_get_stack_count(coin));
        }
        bt->money = fmin(bt->money + amount, 9007199254740991.0);
    }
}

/* PokemonMultiHitModifier.apply(pokemon, moveId, hitCount): called once,
 * after intrinsic strikes/Parental Bond and before this move's hit checks. */
void pr_apply_multi_hit_count_modifiers(pr_battle *bt, const pr_pokemon *src, int move, int *count)
{
    if (!src || !count) return;
    pr_modifier *m;
    EACH_MOD(m, bt, src->is_player, PR_MOD_MULTI_HIT) {
        if (!held_applies(m, src)) continue;
        if (!pr_mod_is_ported_item(m)) { pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS); continue; }
        if (!pr_move_can_be_multi_strike_enhanced_ex(bt, move, src, false, NULL)) continue;
        *count += pr_mod_get_stack_count(m);
    }
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
        double mult = m->mult != 0 ? m->mult : (cls == PR_MOD_ENEMY_DAMAGE_BOOSTER ? 1.05 : 0.975);
        double v = floor(*damage * js_pow(mult, pr_mod_get_stack_count(m)));
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

void pr_apply_enemy_turn_heal_modifiers(pr_battle *bt, pr_engine *e, pr_pokemon *p)
{
    if (!p || p->is_player) return;
    pr_modifier *m;
    EACH_MOD(m, bt, false, PR_MOD_ENEMY_TURN_HEAL) {
        if (p->hp < max_hp(p)) {
            double amount = (max_hp(p) * m->stack_count * 2.0) / 100.0;
            pr_msg msg = pr_msg1("modifier:enemyTurnHealApply",
                                 pr_arg_pokemon(bt, "pokemonNameWithAffix", p));
            pr_unshift_heal_msg(bt, e, pr_battler_index(bt, p), amount,
                                PR_HEAL_MESSAGE | PR_HEAL_PREVENT_FULL, msg);
        }
    }
}

int pr_kings_rock_fired = 0;

void pr_clear_enemy_held_item_modifiers(pr_battle *bt, const pr_pokemon *p)
{
    int w = 0;
    for (int i = 0; i < bt->n_mods[1]; i++) {
        if (!pr_mod_is(&bt->mods[1][i], PR_MOD_HELD_ITEM) || (p && bt->mods[1][i].pokemon_id != (int64_t)p->id)) {
            bt->mods[1][w++] = bt->mods[1][i];
        }
    }
    bt->n_mods[1] = w;
}

void pr_lose_held_item(pr_battle *bt, pr_engine *e, pr_pokemon *p, pr_modifier *m)
{
    if (!m) return;
    int side = p->is_player ? 0 : 1;
    m->stack_count--;
    if (m->stack_count <= 0) {
        int idx = -1;
        for (int i = 0; i < bt->n_mods[side]; i++) {
            if (&bt->mods[side][i] == m) {
                idx = i;
                break;
            }
        }
        if (idx >= 0) {
            memmove(&bt->mods[side][idx], &bt->mods[side][idx + 1],
                    (size_t)(bt->n_mods[side] - idx - 1) * sizeof(pr_modifier));
            bt->n_mods[side]--;
        }
    }
    pr_ab_params pp = {.pokemon = p, .move = -1};
    pr_apply_ab_attrs(bt, PR_AC_PostItemLostAbAttr, &pp);
}

void pr_apply_survive_damage_modifiers(pr_battle *bt, pr_engine *e, pr_pokemon *p, bool *survive)
{
    pr_modifier *m;
    EACH_MOD(m, bt, p->is_player, PR_MOD_SURVIVE_DAMAGE) {
        if (!held_applies(m, p)) continue;
        if (!*survive && pr_battle_rand_seed_int(bt, 10, 0) < pr_mod_get_stack_count(m)) {
            *survive = true;
            pr_queue_msg(bt, e, pr_msg2("modifier:surviveDamageApply",
                                 pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                                 pr_arg_key("typeName", "modifierType:ModifierType.FOCUS_BAND.name")));
            return;
        }
    }
}

void pr_apply_bypass_speed_chance_modifiers(pr_battle *bt, pr_engine *e, pr_pokemon *p)
{
    pr_modifier *m;
    EACH_MOD(m, bt, p->is_player, PR_MOD_BYPASS_SPEED_CHANCE) {
        if (!held_applies(m, p)) continue;
        if (pr_battle_rand_seed_int(bt, 10, 0) < pr_mod_get_stack_count(m)) {
            if (pr_add_tag(bt, e, p, BATTLER_TAG_TYPE_BYPASS_SPEED, 0, 0, -1)) {
                pr_queue_msg(bt, e, pr_msg2("modifier:bypassSpeedChanceApply",
                                     pr_arg_pokemon(bt, "pokemonName", p),
                                     pr_arg_key("itemName", "modifierType:ModifierType.QUICK_CLAW.name")));
                return;
            }
        }
    }
}

void pr_apply_flinch_chance_modifiers(pr_battle *bt, pr_engine *e, pr_pokemon *user, pr_pokemon *target, bool deals_damage, bool *flinched)
{
    if (!deals_damage) return;
    pr_modifier *m;
    EACH_MOD(m, bt, user->is_player, PR_MOD_FLINCH_CHANCE) {
        if (!held_applies(m, user)) continue;
        if (!*flinched && pr_battle_rand_seed_int(bt, 100, 0) < pr_mod_get_stack_count(m) * 10) {
            *flinched = true;
            pr_kings_rock_fired++;
            return;
        }
    }
}

bool pr_apply_reset_negative_stat_stage_modifier(pr_battle *bt, pr_engine *e, pr_pokemon *p)
{
    bool stat_restored = false;
    for (int s = 0; s < 7; s++) {
        if (p->stat_stages[s] < 0) {
            stat_restored = true;
            break;
        }
    }
    if (!stat_restored) return false;

    pr_modifier *m;
    EACH_MOD(m, bt, p->is_player, PR_MOD_RESET_NEGATIVE_STAT_STAGE) {
        if (!held_applies(m, p)) continue;
        for (int s = 0; s < 7; s++) {
            if (p->stat_stages[s] < 0) {
                p->stat_stages[s] = 0;
            }
        }
        pr_queue_msg(bt, e, pr_msg2("modifier:resetNegativeStatStageApply",
                             pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                             pr_arg_key("typeName", "modifierType:ModifierType.WHITE_HERB.name")));
        pr_lose_held_item(bt, e, p, m);
        return true;
    }
    return false;
}
