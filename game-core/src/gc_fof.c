/* Fight or Flight (src/data/mystery-encounters/encounters/fight-or-flight-encounter.ts at pinned upstream 8555c08).
 *
 * onInit (executeWithSeedOffset(.., waveIndex)), all of it on that one stream, in this order: the boss's level (getLevelForWave plus one
 * level per ten waves, rounded), the boss itself (getRandomEncounterPokemon: arena.randomSpecies(wave, level, 0, party luck, true), then
 * new EnemyPokemon(species, level, NONE, boss = true) with its boss moveset; no second IV roll, which only addEnemyPokemon makes; the shiny
 * rerolls are the timed events', which this port does not have), then the item: the player pool's thresholds regenerated, and options of
 * one guaranteed tier (waves up to 40 Great, up to 120 Ultra, up to 160 Rogue, then Master; no luck upgrades) drawn until one is neither
 * a TM nor the Candy Jar. Nothing of it is kept: it is worked out again from the run each time it is needed (the stream starts from the wave
 * seed, and the party, which the thresholds read, cannot change on the way), as the Global Trade System does.
 *
 * Option 1, battle (onOptionPhase, executeWithSeedOffset(.., getSeedOffset() * 500)): setEncounterRewards({guaranteedModifierTypeOptions:
 * [the item], fillRemaining: false}); initBattleWithEnemyConfig: getLevelForWave once more (enemyLevels, unused: the config has its
 * own level), the boss again from its data source (no rolls except its new id, randSeedInt(4294967296)), boss segments, full HP, no
 * passive flag, and the MYSTERY_ENCOUNTER_POST_SUMMON tag. After its first await, on the run's own RNG, the wild pool's thresholds are
 * regenerated and the boss's items rolled (generateEnemyModifiers; the stats are made again). In the battle the boss's PostSummonPhase
 * boosts one of Atk, Def, SpAtk and SpDef by two stages (battle-core), and the player cannot flee. Winning it opens the shop of the item.
 * Option 2, steal (needs a Pokemon allowed in battle with Pluck, Covet, Knock Off, Thief, Trick or Switcheroo; the first one in party
 * order): the same rewards, the Pokemon alone gets the boss species' base experience (setEncounterExp, scaled by the wave), and the
 * encounter leaves with the item's shop and no heal shop. Option 3 leaves with the heal shop.
 *
 * Not ported: the non-seeded roll nothing, but the shown name of an item that has an argument (the stat of a booster) is its type's. */
#include "internal.h"
#include <math.h>
#include <string.h>

typedef struct {
    gc_mon boss;
    int level;
    pr_reward_option item;
} fof_t;

/* getEncounterPokemonLevelForWave(1): the level of the wave plus Math.max(Math.round(wave / 10 * 1), 0) */
static int boosted_level(gc_run *run, pr_rng *r)
{
    const int base = pr_run_get_level_for_wave(&run->run, r, run->wave);
    return base + (int)floor((double)run->wave / 10 + 0.5);
}

/* onInit, on the stream of the wave seed (the boss, its level and the item); false when run-core has no roll for the biome and wave or the species */
bool gc_fof_init(gc_run *run, gc_mon *boss, int *level, pr_reward_option *item)
{
    static fof_t made;
    fof_t *f = &made;
    pr_rng r = pr_run_seed_offset_rng(&run->run, run->wave, NULL);
    const pr_rng saved = run->run.rng;
    const unsigned flags = run->run.unported;
    const bool dbl = run->double_battle;
    bool ok = false;
    run->double_battle = false;
    f->level = boosted_level(run, &r);

    /* arena.randomSpecies(wave, level, 0, luck, true) and getWildSpeciesForLevel, on `r` */
    run->run.rng = r;
    int species = pr_run_get_override_species(&run->run, run->wave);
    if (species < 0 && run->run.unported == flags) {
        bool evo_boss = false;
        species = pr_run_random_species(&run->run, run->biome, run->wave, f->level, gc_party_luck(run), 1, run->wave_cycle_offset, &evo_boss);
        if (species >= 0) species = pr_run_get_wild_species_for_level(&run->run.rng, species, f->level, true, evo_boss, false);
    }
    r = run->run.rng;
    run->run.rng = saved;
    if (species <= 0 || run->run.unported != flags) goto done;

    if (!gc_me_new_boss(run, &r, species, f->level, false, &f->boss)) goto done;
    /* the constructor's calculateStats: the stats (and full HP) of the boss as it stands, before any item */
    pr_update_passive(&run->bt, &f->boss.b);
    pr_calculate_stats(&run->bt, &f->boss.b);

    /* the item: regenerateModifierPoolThresholds(party, PLAYER, 0), then options of the wave's tier until one is no TM and no Candy Jar */
    {
        const int w = run->wave;
        pr_reward_custom custom;
        memset(&custom, 0, sizeof custom);
        custom.tiers[0] = (int8_t)(w > 160 ? 4 : w > 120 ? 3 : w > 40 ? 2 : 1);
        custom.n_tiers = 1;
        custom.allow_luck_upgrades = false;
        custom.reroll_multiplier = NAN;
        pr_reward_ctx *ctx = gc_reward_ctx(run);
        pr_reward_thresholds th;
        run->run.rng = r;
        pr_reward_regenerate_thresholds(ctx, 0, &th);
        for (int tries = 0; tries < 10000; tries++) {
            if (pr_reward_get_player_options(ctx, &th, 1, NULL, 0, &custom, &f->item) != 1) break;
            const char *key = pr_item_keys[f->item.type.item];
            if (!strstr(key, "TM_") && strcmp(key, "CANDY_JAR")) { ok = true; break; }
        }
        run->run.rng = saved;
        if (run->run.unported != flags) ok = false;
    }
done:
    run->run.rng = saved;
    run->double_battle = dbl;
    run->run.unported = flags;
    if (ok) { *boss = f->boss; *level = f->level; *item = f->item; }
    return ok;
}

static bool fof_init(gc_run *run, fof_t *f) { return gc_fof_init(run, &f->boss, &f->level, &f->item); }

bool gc_fof_preview(gc_run *run, int *species, int *form, bool *shiny, int *variant, const char **item_key)
{
    static fof_t f;
    if (!fof_init(run, &f)) return false;
    *species = f.boss.b.species;
    *form = f.boss.b.form;
    *shiny = f.boss.b.shiny;
    *variant = f.boss.variant;
    *item_key = pr_item_keys[f.item.type.item];
    return true;
}

/* the dialogue tokens onInit sets (enemyPokemon, itemName) for gc_mystery_setup_screen; false when onInit could not be made */
bool gc_fof_tokens(gc_run *run, pr_msg_arg *enemy_name, pr_msg_arg *item_name)
{
    static fof_t f;
    if (!fof_init(run, &f)) return false;
    *enemy_name = pr_arg_pokemon_name(&run->bt, "enemyPokemon", &f.boss.b);
    *item_name = gc_m_item("itemName", pr_item_keys[f.item.type.item]);
    return true;
}

/* option 1: the boss fights */
static void fight(gc_run *run, const fof_t *f)
{
    gc_internal *in = gc_int(run);
    /* initBattleWithEnemyConfig: a single battle; battle.enemyLevels = [getLevelForWave()] draws on the option's stream, and the
     * boss from its data source takes a new id from it */
    run->double_battle = false;
    pr_rng r = gc_me_option_rng(run);
    (void)pr_run_get_level_for_wave(&run->run, &r, run->wave);
    gc_mon *e = &run->enemy[0];
    *e = f->boss;
    e->b.id = (uint32_t)floor(pr_rng_real_in_range(&r, 0, 4294967296.0));
    e->b.boss_segments = (uint8_t)pr_run_get_encounter_boss_segments(&run->run, run->wave, f->level, e->b.species, true);
    e->b.boss_segment_index = (uint8_t)(e->b.boss_segments - 1);
    e->b.is_boss = true;
    e->b.passive_flag = false;
    e->b.me_post_summon = PR_ME_POST_STAT_BOOST;
    run->n_enemy = 1;
    run->battle_type = BATTLE_TYPE_WILD;
    /* after the first await: the wild pool's thresholds and the item rolls, on the run's RNG */
    gc_enemy_finish_party_me(run);
    in->me_no_flee = true;
    in->me_reward.active = true;
    in->me_reward.item = f->item;
    gc_me_begin_battle(run, GC_ME_FIGHT_OR_FLIGHT);
}

/* the shop of the item (setEncounterRewards({guaranteedModifierTypeOptions: [item], fillRemaining: false})) */
static void set_item_shop(gc_run *run, const pr_reward_option *item)
{
    pr_reward_custom *c = &gc_int(run)->reward.me_custom;
    memset(c, 0, sizeof *c);
    c->options[c->n_options++] = *item;
    c->fill_remaining = false;
    c->reroll_multiplier = NAN;
    c->allow_luck_upgrades = true;
}

void gc_fof_victory(gc_run *run)
{
    gc_internal *in = gc_int(run);
    in->me_reward.active = false;
    gc_rq_push(run, gc_rp(RP_BATTLE_END, 1, 0, 0));
    set_item_shop(run, &in->me_reward.item);
    gc_me_end_with_shop(run);
}

void gc_fof_execute(gc_run *run, int option)
{
    if (option == 2) { gc_me_leave_with_heal_shop(run); return; }
    static fof_t f;
    if (!fof_init(run, &f)) {
        /* a species or biome run-core has no rolls for: nobody guards anything, the encounter ends as if declined */
        gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        gc_me_leave_with_heal_shop(run);
        return;
    }
    if (option == 0) { fight(run, &f); return; }
    /* option 2 of the screen: the Pokemon steals the item */
    set_item_shop(run, &f.item);
    const int primary = gc_mystery_primary_pokemon(run, GC_ME_FIGHT_OR_FLIGHT, 1);
    if (primary >= 0) {
        pr_exp_party pp;
        const uint32_t id = run->party[primary].b.id;
        gc_exp_party_build(run, &pp, &id, 1);
        pp.trainer_battle = false;
        pp.me_exp_multiplier = 1.0;   /* isBattleMysteryEncounter(): the encounter's expMultiplier, 1 */
        gc_exp_party_queue(run, &pp, gc_form(f.boss.b.species, 0)->base_exp, true);
    }
    gc_me_end_shop(run);
}
