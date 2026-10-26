/* Dark Deal (src/data/mystery-encounters/encounters/dark-deal-encounter.ts). Option 1, the deal:
 *   onPreOptionPhase (executeWithSeedOffset(.., getSeedOffset())): getRandomPlayerPokemon(true, false, true) - a random Pokemon of the
 *     party that has not fainted, or a fainted one when only one is left standing - leaves the party for good, with the held items
 *     it carried (form-change items aside, which stay behind and are lost with it); its types are remembered;
 *   onOptionPhase (.., getSeedOffset() * 500): ModifierRewardPhase(ROGUE_BALL) (five Rogue Balls), then a boss of 6/7/8/[9,10]
 *     starter strength (65 / 50 / 10 / 5 %: randSeedInt(100) >= 65 / >= 15 / >= 5 / below), chosen from the starters that share a
 *     type with the removed Pokemon (getRandomSpeciesByStarterCost), never an Ultra Beast, Paradox, Mythical or special form listed
 *     below. It fights with the removed Pokemon's held items, in order, and the form-index 0 when the species has forms
 *     (initBattleWithEnemyConfig with that one EnemyPokemonConfig: a boss of the wave's level, no item rolls, one boss).
 * Option 2 leaves with the heal shop (leaveEncounterWithoutBattle(true)). Rewards after the fight are the ordinary ones.
 * Known differences from upstream's state right after the option, all of them things upstream does later: the stats of a boss with forms
 * are made for form 0 (upstream assigns formIndex = 0 and leaves the stats of the form it was made with until something recalculates
 * them), the dex sighting and Terapagos' Tera Shift come with this port's battle start.
 * Not ported: the Single Type challenge's override of the boss types (this port has no challenges). */
#include "internal.h"
#include <math.h>
#include <string.h>

static const uint16_t excluded_bosses[] = {
    SPECIES_ID_ETERNATUS,
    /* Ultra Beasts */
    SPECIES_ID_NIHILEGO, SPECIES_ID_BUZZWOLE, SPECIES_ID_PHEROMOSA, SPECIES_ID_XURKITREE, SPECIES_ID_CELESTEELA, SPECIES_ID_KARTANA,
    SPECIES_ID_GUZZLORD, SPECIES_ID_POIPOLE, SPECIES_ID_NAGANADEL, SPECIES_ID_STAKATAKA, SPECIES_ID_BLACEPHALON, SPECIES_ID_COSMOG,
    SPECIES_ID_COSMOEM, SPECIES_ID_SOLGALEO, SPECIES_ID_LUNALA, SPECIES_ID_NECROZMA,
    /* Paradox */
    SPECIES_ID_GREAT_TUSK, SPECIES_ID_SCREAM_TAIL, SPECIES_ID_BRUTE_BONNET, SPECIES_ID_FLUTTER_MANE, SPECIES_ID_SLITHER_WING,
    SPECIES_ID_SANDY_SHOCKS, SPECIES_ID_ROARING_MOON, SPECIES_ID_WALKING_WAKE, SPECIES_ID_GOUGING_FIRE, SPECIES_ID_RAGING_BOLT,
    SPECIES_ID_KORAIDON, SPECIES_ID_IRON_TREADS, SPECIES_ID_IRON_BUNDLE, SPECIES_ID_IRON_HANDS, SPECIES_ID_IRON_JUGULIS,
    SPECIES_ID_IRON_MOTH, SPECIES_ID_IRON_THORNS, SPECIES_ID_IRON_VALIANT, SPECIES_ID_IRON_LEAVES, SPECIES_ID_IRON_BOULDER,
    SPECIES_ID_IRON_CROWN, SPECIES_ID_MIRAIDON,
    /* Mythical */
    SPECIES_ID_MEW, SPECIES_ID_CELEBI, SPECIES_ID_JIRACHI, SPECIES_ID_DEOXYS, SPECIES_ID_PHIONE, SPECIES_ID_MANAPHY, SPECIES_ID_DARKRAI,
    SPECIES_ID_SHAYMIN, SPECIES_ID_ARCEUS, SPECIES_ID_VICTINI, SPECIES_ID_KELDEO, SPECIES_ID_MELOETTA, SPECIES_ID_GENESECT,
    SPECIES_ID_DIANCIE, SPECIES_ID_HOOPA, SPECIES_ID_VOLCANION, SPECIES_ID_MAGEARNA, SPECIES_ID_MARSHADOW, SPECIES_ID_ZERAORA,
    SPECIES_ID_MELTAN, SPECIES_ID_MELMETAL, SPECIES_ID_ZARUDE, SPECIES_ID_PECHARUNT,
    /* special forms */
    SPECIES_ID_ETERNAL_FLOETTE, SPECIES_ID_BLOODMOON_URSALUNA, SPECIES_ID_BATTLE_BOND_GRENINJA,
};

static bool is_excluded(int id)
{
    for (size_t i = 0; i < sizeof excluded_bosses / sizeof excluded_bosses[0]; i++) if (excluded_bosses[i] == id) return true;
    return false;
}

/* getRandomSpeciesByStarterCost([min, max], excluded, types): the starters in id order (getAllStarters(true)), those sharing a type
 * with `types` when it has any, the tier range widened while it holds none (min down to 0, then max up to 10), randSeedInt(length)
 * for the index, then Phaser.Math.RND.shuffle (frac() * (i + 1) from the end) and the species at that index; Bulbasaur if nothing
 * is left at all */
static int species_by_starter_cost(pr_rng *r, int min, int max, const int8_t *types, int n_types)
{
    static uint16_t pool[GC_MAX_SPECIES_ID + 1], list[GC_MAX_SPECIES_ID + 1];
    static uint8_t cost[GC_MAX_SPECIES_ID + 1];
    int n = 0, len = 0;
    for (int i = 0; i < gc_starter_cost_count && n <= GC_MAX_SPECIES_ID; i++) {
        const int id = gc_starter_costs[i].id;
        const pr_species_gen *sg = gc_species_gen(id);
        if (!sg || is_excluded(id)) continue;
        if (n_types > 0) {
            const pr_form_gen *fg = &pr_form_gens[sg->form_start];
            bool shares = false;
            for (int t = 0; t < n_types; t++) if (types[t] == fg->type1 || (fg->type2 != -2 && types[t] == fg->type2)) shares = true;
            if (!shares) continue;
        }
        pool[n] = (uint16_t)id;
        cost[n++] = gc_starter_costs[i].cost;
    }
    for (;;) {
        len = 0;
        for (int k = 0; k < n; k++) if (cost[k] >= min && cost[k] <= max) list[len++] = pool[k];
        if (len > 0 || (min == 0 && max == 10)) break;
        if (min > 0) min--; else max++;
    }
    if (!len) return SPECIES_ID_BULBASAUR;
    const int index = pr_rng_seed_int(r, len, 0);
    for (int i = len - 1; i > 0; i--) {
        const int j = (int)floor(pr_rng_frac(r) * (i + 1));
        const uint16_t t = list[j];
        list[j] = list[i];
        list[i] = t;
    }
    return list[index];
}

/* getRandomPlayerPokemon(true, false, true) on `r`: the party index (in party order) of the Pokemon it returns. Every Pokemon is
 * allowed in a challenge here (this port has no challenges) */
static int random_player_pokemon(gc_run *run, pr_rng *r)
{
    int able[6], n_able = 0, fainted[6], n_fainted = 0;
    for (int k = 0; k < run->n_party; k++) {
        if (run->party[run->party_order[k]].b.hp > 0) able[n_able++] = k;
        else fainted[n_fainted++] = k;
    }
    if (n_able == 1 && n_fainted > 0) return fainted[pr_rng_seed_int(r, n_fainted, 0)];
    if (n_able > 0) return able[pr_rng_seed_int(r, n_able, 0)];
    return pr_rng_seed_int(r, run->n_party, 0);
}

static bool is_form_change_item(const gc_item *it)
{
    return !strcmp(it->key, "FORM_CHANGE_ITEM") || !strcmp(it->key, "RARE_FORM_CHANGE_ITEM");
}

/* new EnemyPokemon(species, level, NONE, boss = true) on `r` (a seed-offset stream), and with `second_iv` addEnemyPokemon's second IV
 * roll (a boss made without a data source); the config's form index 0 is applied by the caller */
bool gc_me_new_boss(gc_run *run, pr_rng *r, int species, int level, bool second_iv, gc_mon *out)
{
    pr_enemy_gen_ctx ec;
    gc_enemy_ctx(run, &ec);
    ec.species = species;
    ec.level = level;
    ec.boss = true;
    pr_enemy_gen g;
    memset(&g, 0, sizeof g);
    g.fusion_species = -1;
    g.friendship = -1;
    if (pr_run_enemy_pokemon_base_rng(r, &run->run, &ec, &g) < 0) return false;
    g.boss_segments = pr_run_get_encounter_boss_segments(&run->run, run->wave, level, species, true);
    gc_mon_new_enemy(run, out, species, level, &g, 0);
    if (!gc_generate_moveset(run, r, out)) gc_port_enemy_moveset(run, out);
    const pr_rng saved = run->run.rng;
    run->run.rng = *r;
    ec.boss = second_iv;   /* the finish rolls the second IVs of a boss */
    const int rc = pr_run_enemy_pokemon_finish(&run->run, &ec, &g);
    *r = run->run.rng;
    run->run.rng = saved;
    if (rc < 0) return false;
    gc_refresh_enemy(run, out, species, level, &g, 0);
    return true;
}

void gc_dark_deal_execute(gc_run *run, int option)
{
    if (option != 0) { gc_me_leave_with_heal_shop(run); return; }

    /* onPreOptionPhase: the Pokemon, its types and its items; then it is gone */
    pr_rng pre = pr_run_seed_offset_rng(&run->run, run->wave * 1000 + 512, NULL);
    const int slot = random_player_pokemon(run, &pre);
    const int idx = run->party_order[slot];
    int8_t types[PR_MAX_TYPES];
    const int n_types = pr_get_types(&run->party[idx].b, types, PR_TYPES_NO_TERA);
    static gc_item kept[GC_MAX_ITEMS];
    int n_kept = 0;
    for (int i = 0; i < run->n_items; i++) {
        const gc_item *it = &run->items[i];
        if (it->pokemon_id == run->party[idx].b.id && it->pokemon_id != 0 && !is_form_change_item(it)) kept[n_kept++] = *it;
    }
    gc_party_remove(run, idx);
    gc_me_prune_items(run);
    gc_me_update_modifiers(run);

    /* onOptionPhase: the encounter's battle is a single battle (EnemyPartyConfig.doubleBattle unset) */
    run->double_battle = false;
    pr_rng r = pr_run_seed_offset_rng(&run->run, (run->wave * 1000 + 512) * 500, NULL);
    gc_reward_gain(run, "ROGUE_BALL");
    const int roll = pr_rng_seed_int(&r, 100, 0);
    const int lo = roll >= 65 ? 6 : roll >= 15 ? 7 : roll >= 5 ? 8 : 9;
    const int hi = roll >= 5 ? lo : 10;
    const int species = species_by_starter_cost(&r, lo, hi, types, n_types > 0 ? n_types : 0);
    /* initBattleWithEnemyConfig: battle.enemyLevels = [getLevelForWave()], rolled on the same stream */
    const int level = pr_run_get_level_for_wave(&run->run, &r, run->wave);
    gc_mon *boss = &run->enemy[0];
    if (!gc_me_new_boss(run, &r, species, level, true, boss)) {
        /* a species run-core has no rolls for: nobody fights, the encounter ends as if declined */
        gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        run->n_enemy = 0;
        gc_me_leave_with_heal_shop(run);
        return;
    }
    run->n_enemy = 1;
    const pr_species_gen *sg = gc_species_gen(species);
    if (sg && sg->n_forms > 0) {
        /* config.formIndex = 0, then setBoss: the stats are made again for the form */
        boss->b.form = 0;
        pr_update_passive(&run->bt, &boss->b);
        pr_calculate_stats(&run->bt, &boss->b);
        boss->b.hp = boss->b.stats[STAT_HP];
    }

    run->battle_type = BATTLE_TYPE_WILD;
    gc_enemy_finish_party_items(run, kept, n_kept);
    gc_me_begin_battle(run, GC_ME_DARK_DEAL);
}
