/* A wild battle's first enemy, as the EncounterPhase makes it (src/phases/encounter-phase.ts): the
 * species (globalScene.randomSpecies(waveIndex, level, true), i.e. arena.randomSpecies with the party's
 * luck, and the Golden Bug Net's replacement), the boss check, the shiny lock, and addEnemyPokemon up to
 * the moveset. The enemies after the first roll on the same RNG after the first one's moveset, which
 * isn't ported yet. The JS each step replaces is quoted where it happens. */
#include "pr/run.h"

int pr_run_gen_wild_enemy(pr_run *run, const pr_wild_enemy_ctx *ctx, pr_wild_enemy *out)
{
    pr_enemy_gen_ctx ec = ctx->enemy;
    const int wave = ec.wave, level = ctx->level;
    /* arena.randomSpecies(waveIndex, level, 0, getPartyLuckValue(party)): isBoss undefined; the override
     * species (the daily run's final boss) comes back as it is */
    const unsigned flags = run->unported;
    int species = pr_run_get_override_species(run, wave);
    if (species < 0) {
        if (run->unported != flags) return -1;
        bool evo_boss = false;
        species = pr_run_random_species(run, ec.biome, wave, level, ctx->luck, -1, ec.wave_cycle_offset, &evo_boss);
        if (species < 0) return -1;
        species = pr_run_get_wild_species_for_level(&run->rng, species, level, true, evo_boss, false);
    }
    /* the Golden Bug Net (BoostBugSpawnModifier): !gameMode.isBoss(waveIndex) && biome !== END &&
     * randSeedInt(10) === 0 → getGoldenBugNetSpecies(level): a weighted roll, then
     * getWildSpeciesForLevel(level, true, false) */
    if (ctx->golden_bug_net && !pr_run_is_boss(wave) && ec.biome != BIOME_ID_END && !pr_rng_seed_int(&run->rng, 10, 0)) {
        const int roll = pr_rng_seed_int(&run->rng, pr_golden_bug_net_total, 0);
        int pick = SPECIES_ID_SCYTHER;   /* upstream's fallback, past the last weight */
        for (int i = 0; i < pr_golden_bug_net_count; i++)
            if (roll < pr_golden_bug_net[i].upto) { pick = pr_golden_bug_net[i].species; break; }
        species = pr_run_get_wild_species_for_level(&run->rng, pick, level, true, false, false);
    }
    /* addEnemyPokemon(species, level, TrainerSlot.NONE, !!getEncounterBossSegments(waveIndex, level, species),
     *                 isEncounterShinyLocked()) */
    ec.species = species;
    ec.level = level;
    ec.trainer_slot = 0;
    ec.boss = pr_run_get_encounter_boss_segments(run, wave, level, species, false) != 0;
    /* isEncounterShinyLocked(): END in an endless mode or with the Fresh Start challenge */
    ec.shiny_lock = ec.biome == BIOME_ID_END
                    && (run->mode == GAME_MODES_ENDLESS || run->mode == GAME_MODES_SPLICED_ENDLESS || ctx->fresh_start);
    out->species = species;
    out->boss = ec.boss;
    out->shiny_lock = ec.shiny_lock;
    return pr_run_enemy_pokemon_base(run, &ec, &out->mon);
}
