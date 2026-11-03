/* Fiery Fallout (src/data/mystery-encounters/encounters/fiery-fallout-encounter.ts at pinned upstream 8555c08).
 *
 * onInit (executeWithSeedOffset(.., waveIndex)) rolls nothing: it builds the config of two Volcarona (not bosses, the first male, the second
 * female, each with the post-summon effect of Sp. Def and Spd up one stage) and sets the weather to Sunny with no Pokemon as the source
 * (arena.trySetWeather(SUNNY, getEnemyPokemon()): an ME wave has no enemy yet), so that the weather has no turn count and stays. A weather
 * that is already Sunny is left alone, and a primal weather (Harsh Sun, Heavy Rain, Strong Winds) only shows its "continues" message.
 * The encounter needs the wave range 40..180 in a Volcano; fleeing is off; catching is allowed.
 *
 * Option 1, battle (no await before initBattleWithEnemyConfig, so every roll is on the option's stream): setEncounterRewards({fillRemaining:
 * true}, undefined, giveLeadPokemonAttackTypeBoostItem); startOfBattleEffects gets Fire Spin from the first Volcarona at the first Pokemon and
 * from the second at the second; a double battle, switching off; two getLevelForWave rolls, then the two Volcarona (addEnemyPokemon, no data
 * source: no boss, no second IV roll), the gender of the config over them, the items rolled after the await on the run's RNG (generateEnemy-
 * Modifiers). Winning gives the lead (the first of the party) an attack type booster before the shop: generateModifierType(ATTACK_TYPE_BOOSTER)
 * rolled in executeWithSeedOffset(.., waveIndex * 1000) as the rewards phase runs it (Charcoal if none can be made), and its message.
 * Option 2, hunker down (no await): every Pokemon allowed in battle that is not a Fire type (Tera type left out) takes floor(max HP * 20 /
 * 100), left at 1 HP at the least; of those without a status, one is chosen by randSeedInt(count) and, when canSetStatus(BURN, true) says yes,
 * burned, told so, and given the ability Heatproof for good (customPokemonData.ability, the fusion's when it is a fusion). Leaves with the
 * heal shop.
 * Option 3, calm them (a Fire Pokemon that is not fainted, or one allowed in battle with a fire resistant ability; the primary Pokemon is the
 * first Fire type of the whole party, fainted or not, and only without one the first with the ability): no battle, setEncounterExp(primary.id,
 * Volcarona's base experience * 2, with the wave multiplier), the lead's booster as above and the ordinary shop (fillRemaining), no heal shop.
 *
 * Not ported: the sprites, the lava animations, and the abilities that react to the weather changing (Forecast, Flower Gift, Protosynthesis...)
 * of the Pokemon on the field when onInit sets it. */
#include "internal.h"
#include <math.h>
#include <string.h>

#define DAMAGE_PERCENTAGE 20

static bool is_fire(const gc_mon *m)
{
    int8_t types[PR_MAX_TYPES];
    const int n = pr_get_types(&m->b, types, PR_TYPES_NO_TERA);
    for (int i = 0; i < n; i++) if (types[i] == POKEMON_TYPE_FIRE) return true;
    return false;
}

/* FIRE_RESISTANT_ABILITIES (requirement-groups.ts) */
static bool fire_resistant_ability(const gc_mon *m)
{
    static const int abilities[] = {ABILITY_ID_FLAME_BODY, ABILITY_ID_FLASH_FIRE, ABILITY_ID_WELL_BAKED_BODY, ABILITY_ID_HEATPROOF,
        ABILITY_ID_THERMAL_EXCHANGE, ABILITY_ID_THICK_FAT, ABILITY_ID_WATER_BUBBLE, ABILITY_ID_MAGMA_ARMOR, ABILITY_ID_WATER_VEIL,
        ABILITY_ID_STEAM_ENGINE, ABILITY_ID_PRIMORDIAL_SEA};
    const int ability = pr_get_ability(&m->b, false);
    for (size_t j = 0; j < sizeof abilities / sizeof abilities[0]; j++)
        if (ability == abilities[j] || (m->b.passive_flag && m->b.passive == abilities[j])) return true;
    return false;
}

/* CombinationPokemonRequirement.Some(TypeRequirement(FIRE, excludeFainted), AbilityRequirement(.., excludeDisallowed)) on the option */
bool gc_fiery_option_ok(const gc_run *run)
{
    for (int k = 0; k < run->n_party; k++) {
        const gc_mon *m = &run->party[run->party_order[k]];
        if (m->b.hp > 0 && is_fire(m)) return true;
        if (gc_mon_allowed(m) && fire_resistant_ability(m)) return true;
    }
    return false;
}

/* queryParty answers with the FIRST requirement that matches anyone in the whole party (a fainted Fire type counts), and the option's
 * primary Pokemon is the first of its answer */
int gc_fiery_primary(const gc_run *run)
{
    if (!gc_fiery_option_ok(run)) return -1;
    for (int k = 0; k < run->n_party; k++)
        if (is_fire(&run->party[run->party_order[k]])) return run->party_order[k];
    for (int k = 0; k < run->n_party; k++)
        if (gc_mon_allowed(&run->party[run->party_order[k]]) && fire_resistant_ability(&run->party[run->party_order[k]])) return run->party_order[k];
    return -1;
}

/* onInit: arena.trySetWeather(SUNNY) with no source Pokemon */
void gc_fiery_init(gc_run *run)
{
    pr_battle *bt = &run->bt;
    if (bt->weather == WEATHER_TYPE_SUNNY) return;
    if (pr_weather_immutable(bt->weather)) {
        gc_ev_msg(run, pr_weather_key(bt->weather, PR_WEATHER_CONTINUE), 0);
        return;
    }
    bt->weather = WEATHER_TYPE_SUNNY;
    bt->weather_turns_left = bt->weather_max_duration = 0;
    gc_ev_msg(run, pr_weather_key(WEATHER_TYPE_SUNNY, PR_WEATHER_START), 0);
}

/* giveLeadPokemonAttackTypeBoostItem, as the rewards phase runs it: in executeWithSeedOffset(.., waveIndex * 1000) */
static void give_lead_item(gc_run *run)
{
    if (run->n_party < 1) return;
    gc_mon *lead = &run->party[run->party_order[0]];
    const pr_rng saved = run->run.rng;
    run->run.rng = pr_run_seed_offset_rng(&run->run, run->wave * 1000, NULL);
    pr_reward_ctx *ctx = gc_reward_ctx(run);
    int type = pr_reward_generate(ctx, PR_ITEM_ATTACK_TYPE_BOOSTER);
    run->run.rng = saved;
    if (type < 0) type = POKEMON_TYPE_FIRE;   /* the default is Charcoal */
    int held = 0;
    for (int k = 0; k < run->n_items; k++) {
        const gc_item *it = &run->items[k];
        if (!strcmp(it->key, "ATTACK_TYPE_BOOSTER") && it->arg == type && it->pokemon_id == lead->b.id) held += it->stack;
    }
    /* applyModifierTypeToPlayerPokemon leaves a Pokemon at the item's maximum stack as it is; the text comes anyway */
    if (held < 99) gc_item_add(run, "ATTACK_TYPE_BOOSTER", (int16_t)type, lead->b.id, 1);
    gc_items_to_battle(run);
    gc_ev_msg(run, "mysteryEncounters/fieryFallout:foundItem", 2,
              gc_m_item_arg("itemName", "ATTACK_TYPE_BOOSTER", type), pr_arg_pokemon_name(&run->bt, "leadPokemon", &lead->b));
}

void gc_fiery_reward(gc_run *run)
{
    gc_internal *in = gc_int(run);
    if (!in->fiery_item) return;
    in->fiery_item = false;
    give_lead_item(run);
}

/* option 1 */
static void fight(gc_run *run)
{
    gc_internal *in = gc_int(run);
    pr_rng r = gc_me_option_rng(run);
    run->double_battle = true;
    int level[2];
    for (int i = 0; i < 2; i++) level[i] = pr_run_get_level_for_wave(&run->run, &r, run->wave);
    gc_mon made[2];
    for (int i = 0; i < 2; i++) {
        if (!gc_me_new_enemy(run, &r, SPECIES_ID_VOLCARONA, level[i], false, false, &made[i])) {
            gc_stub(run, GC_STUB_UNPORTED_ENEMY);
            run->double_battle = false;
            run->n_enemy = 0;
            gc_me_leave_with_heal_shop(run);
            return;
        }
    }
    for (int i = 0; i < 2; i++) {
        gc_mon *e = &run->enemy[i];
        *e = made[i];
        e->b.gender = i ? GENDER_FEMALE : GENDER_MALE;
        e->b.passive_flag = false;
        e->b.me_post_summon = PR_ME_POST_SPDEF_SPD;
    }
    run->n_enemy = 2;
    run->battle_type = BATTLE_TYPE_WILD;
    /* after the await: the wild pool's thresholds and the item rolls, on the run's RNG */
    gc_enemy_finish_party_me(run);

    in->me_reward.active = true;
    in->me_reward.has_item = false;
    in->me_reward.func = -1;
    in->me_reward.fill = true;
    in->me_reward.before = true;
    in->fiery_item = true;
    in->me_no_flee = true;
    in->me_no_switch = true;
    gc_me_add_start_effect(run, 2, 0, MOVE_ID_FIRE_SPIN);
    gc_me_add_start_effect(run, 3, 1, MOVE_ID_FIRE_SPIN);
    gc_me_begin_battle(run, GC_ME_FIERY_FALLOUT);
}

/* option 2 */
static void hunker(gc_run *run)
{
    pr_rng r = gc_me_option_rng(run);
    int pool[PR_MAX_PARTY], n = 0;
    for (int k = 0; k < run->n_party; k++) {
        const int idx = run->party_order[k];
        if (gc_mon_allowed(&run->party[idx]) && !is_fire(&run->party[idx])) pool[n++] = idx;
    }
    for (int k = 0; k < n; k++) {
        gc_mon *m = &run->party[pool[k]];
        gc_me_apply_damage(m, (int)floor(m->b.stats[0] * ((double)DAMAGE_PERCENTAGE / 100)));
    }
    int burnable[PR_MAX_PARTY], nb = 0;
    for (int k = 0; k < n; k++) if (run->party[pool[k]].b.status == STATUS_EFFECT_NONE) burnable[nb++] = pool[k];
    if (nb > 0) {
        gc_mon *m = &run->party[burnable[pr_rng_seed_int(&r, nb, 0)]];
        if (pr_can_set_status(&run->bt, &run->eng, &m->b, STATUS_EFFECT_BURN, true, false, NULL, false)) {
            m->b.status = STATUS_EFFECT_BURN;
            m->b.pending_status = STATUS_EFFECT_NONE;
            gc_ev_msg(run, "mysteryEncounters/fieryFallout:option.2.targetBurned", 2,
                      pr_arg_pokemon_name(&run->bt, "burnedPokemon", &m->b), pr_arg_ability("abilityName", ABILITY_ID_HEATPROOF));
            /* applyAbilityOverrideToPokemon: a fusion's ability is its fusion data's */
            if (m->b.fusion_species > 0) m->b.fusion_custom_ability = ABILITY_ID_HEATPROOF;
            else m->b.custom_ability = ABILITY_ID_HEATPROOF;
        }
    }
    gc_me_leave_with_heal_shop(run);
}

/* option 3 */
static void calm(gc_run *run)
{
    const int primary = gc_fiery_primary(run);
    /* setEncounterExp(primary.id, Volcarona's base experience * 2), then the callback of setEncounterRewards, then its shop */
    if (primary >= 0) gc_me_exp_value(run, primary, gc_form(SPECIES_ID_VOLCARONA, 0)->base_exp * 2, true);
    give_lead_item(run);
    pr_reward_custom *c = &gc_int(run)->reward.me_custom;
    memset(c, 0, sizeof *c);
    c->fill_remaining = true;
    c->reroll_multiplier = NAN;
    c->allow_luck_upgrades = true;
    gc_me_end_shop(run);
}

void gc_fiery_execute(gc_run *run, int option)
{
    if (option == 0) fight(run);
    else if (option == 1) hunker(run);
    else calm(run);
}
