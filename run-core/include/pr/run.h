/* run-core: the run around the battles (waves, biomes, the shop's economy; the reward pools next).
 * Ported from upstream src/game-mode.ts, src/battle.ts and src/battle-scene.ts; function
 * names follow upstream so each one diffs against its TS original. It reads battle-core's
 * RNG, V8 Math.pow and generated tables and never changes battle-core. */
#ifndef PR_RUN_H
#define PR_RUN_H
#include <stdbool.h>
#include <stdint.h>
#include "pr/rng.h"
#include "pr_data.h"   /* battle-core's generated tables: GameModes, BiomeId, species */

/* Mode rules the port doesn't model yet. A function that meets one sets its bit in
 * run->unported and returns a placeholder, so a caller (or the test) never takes a guess
 * for upstream's answer. */
enum {
    PR_RUN_UNPORTED_DAILY = 1u << 0,      /* the daily run's seed config (trainer waves, money, final boss) */
    PR_RUN_UNPORTED_CHALLENGES = 1u << 1, /* challenge hooks (FIXED_BATTLES...) */
    PR_RUN_UNPORTED_SPECIES = 1u << 3,    /* globalScene.randomSpecies (any species), for an empty biome pool */
    PR_RUN_UNDEFINED_UPSTREAM = 1u << 2,  /* upstream's own answer is undefined here (e.g. randSeedItem([])) */
};

typedef struct pr_run {
    int mode;                  /* gameMode.modeId (GameModes) */
    bool has_challenges;       /* gameMode.challenges has an active challenge */
    pr_seedstr seed;           /* globalScene.seed: the run seed */
    pr_seedstr wave_seed;      /* globalScene.waveSeed */
    pr_rng rng;                /* Phaser.Math.RND */
    bool offset_gym;           /* globalScene.offsetGym */
    unsigned unported;
} pr_run;

/* ---- GameMode (src/game-mode.ts) ---- */
int pr_run_get_starting_level(pr_run *run);
int pr_run_get_starting_money(pr_run *run);
int pr_run_get_wave_for_difficulty(const pr_run *run, int wave, bool ignore_curve_changes);
bool pr_run_is_wave_final(int mode, int wave);
bool pr_run_is_boss(int wave);
bool pr_run_is_battle_classic_final_boss(const pr_run *run, int wave);
bool pr_run_is_endless_boss(const pr_run *run, int wave);
bool pr_run_is_endless_minor_boss(const pr_run *run, int wave);
bool pr_run_is_endless_major_boss(const pr_run *run, int wave);
bool pr_run_is_trainer_boss(const pr_run *run, int wave, int biome, bool offset_gym);
bool pr_run_is_fixed_battle(pr_run *run, int wave);
int pr_run_get_clear_score_bonus(const pr_run *run);
int pr_run_get_enemy_modifier_chance(const pr_run *run, bool is_boss);
/* isWaveTrainer(wave), with arena.trainerChance of the current biome. The final roll uses
 * run->rng (Phaser.Math.RND, as newBattle leaves it after resetSeed); the checks of earlier
 * waves use executeWithSeedOffset on the run seed and leave run->rng untouched. */
bool pr_run_is_wave_trainer(pr_run *run, int wave, int trainer_chance);

/* ---- BattleScene (src/battle-scene.ts) ---- */
/* resetSeed(wave): waveSeed = shiftCharCodes(seed, wave); Phaser.Math.RND.sow([waveSeed]) */
void pr_run_reset_seed(pr_run *run, int wave);
/* getMaxExpLevel(ignoreLevelCap) with currentBattle.waveIndex = wave (0 = no battle yet) */
double pr_run_get_max_exp_level(const pr_run *run, int wave, bool ignore_level_cap);
/* getEncounterBossSegments(wave, level, species, forceBoss); species = SpeciesId, or -1 for none */
int pr_run_get_encounter_boss_segments(pr_run *run, int wave, int level, int species, bool force_boss);
/* getDoubleBattleChance(wave): x1/4 for each Lure-type modifier the player holds and for each
 * field Pokemon whose ability applies DoubleBattleChanceAbAttr (the caller counts those
 * through battle-core's modifier lists and ability dispatch) */
double pr_run_get_double_battle_chance(int wave, int n_lure_modifiers, int n_double_chance_abilities);

/* ---- mystery encounters and trainer parties (src/wave.c) ---- */
/* mysteryEncounterSaveData: encounterSpawnChance, encounteredEvents.length and the last one's waveIndex */
typedef struct { double spawn_chance; int n_encountered; int last_encounter_wave; } pr_me_save;
bool pr_run_is_mystery_encounter_valid_for_wave(const pr_run *run, bool wild, int wave);
/* isWaveMysteryEncounter(battleType, wave): rolls on the run seed at offset wave * 3000 */
bool pr_run_is_wave_mystery_encounter(pr_run *run, bool wild, int wave, const pr_me_save *save);
#define PR_MAX_PARTY_TEMPLATE 8
/* Trainer.getPartyLevels(wave) for a party template: strengths[i] = template.getStrength(i),
 * base_strength = template.strength (used when a double battle widens a 1-member template).
 * Returns the party size. */
int pr_run_get_party_levels(const pr_run *run, int wave, int size, const uint8_t *strengths, int base_strength,
                            bool is_double, int out[PR_MAX_PARTY_TEMPLATE]);

/* ---- Battle (src/battle.ts) ---- */
/* getLevelForWave() for a battle of `wave`, rolling on `rng` (newBattle builds the Battle inside
 * executeWithSeedOffset(waveIndex << 3, waveSeed), so pass pr_run_battle_rng's result) */
int pr_run_get_level_for_wave(pr_run *run, pr_rng *rng, int wave);
/* The Phaser.Math.RND a new Battle is built with: sown from shiftCharCodes(waveSeed, wave << 3) */
pr_rng pr_run_battle_rng(const pr_run *run, int wave);

/* ---- the shop's economy (src/shop.c) ---- */
/* The items of getPlayerShopModifierTypeOptionsForWave, in its order (modifierTypeInitObj keys) */
enum {
    PR_SHOP_POTION, PR_SHOP_ETHER, PR_SHOP_REVIVE, PR_SHOP_SUPER_POTION, PR_SHOP_FULL_HEAL, PR_SHOP_ELIXIR,
    PR_SHOP_MAX_ETHER, PR_SHOP_HYPER_POTION, PR_SHOP_MAX_REVIVE, PR_SHOP_MEMORY_MUSHROOM, PR_SHOP_MAX_POTION,
    PR_SHOP_MAX_ELIXIR, PR_SHOP_FULL_RESTORE, PR_SHOP_SACRED_ASH, PR_SHOP_ITEM_COUNT_
};
extern const char *const pr_shop_item_name[PR_SHOP_ITEM_COUNT_];
typedef struct { int item; double cost; } pr_shop_option;   /* ModifierTypeOption: type and cost */
/* getWaveMoneyAmount(moneyMultiplier) with currentBattle.waveIndex = wave */
double pr_run_get_wave_money_amount(int wave, double money_multiplier);
/* getPlayerShopModifierTypeOptionsForWave(wave, baseCost): the options in order; returns their count.
 * The shop screen shows them with baseCost = Black Sludge applied to getWaveMoneyAmount(1); buying
 * one charges Black Sludge applied to the option built from the plain getWaveMoneyAmount(1). */
int pr_run_get_player_shop_options(pr_run *run, int wave, double base_cost, pr_shop_option out[PR_SHOP_ITEM_COUNT_]);
bool pr_run_get_shop_status(pr_run *run);
/* HealShopCostModifier (Black Sludge) through applyModifier; multiplier 0 = none held */
double pr_run_apply_heal_shop_cost(double cost, double black_sludge_multiplier);
/* MoneyRewardPhase: wave money x the battle's multiplier, each Amulet Coin (MoneyMultiplierModifier,
 * in the modifier list's order: each floors its bonus, so the order changes the total;
 * given by its getStackCount()), then Happy Hour's x2 */
double pr_run_get_money_reward(int wave, double money_multiplier, const int *amulet_coin_stacks, int n_amulet_coins,
                               bool happy_hour);
/* SelectModifierPhase.getRerollCost(lockRarities); option_tiers[i] < 0 = undefined tier,
 * custom_multiplier NaN = no customModifierSettings.rerollMultiplier */
double pr_run_get_reroll_cost(int wave, bool lock_rarities, const int *option_tiers, int n_options, int reroll_count,
                              double custom_multiplier, double black_sludge_multiplier);

/* ---- biomes (src/biome.c; data in gen/out/run_biomes.c, dumped from upstream's allBiomes) ---- */
#define PR_BIOME_ID_LIMIT 64       /* BiomeId values are below this (the dump checks) */
#define PR_MAX_BIOME_LINKS 8
typedef struct { uint8_t biome, weight; } pr_biome_link;
typedef struct { uint16_t start, n; } pr_pool_slice;
#define PR_BIOME_POOL_TIERS 9      /* BiomePoolTier COMMON..BOSS_ULTRA_RARE */   /* weight 0 = a plain link, else [biome, weight] */
typedef struct {
    bool present;
    uint8_t trainer_chance;       /* arena.trainerChance in this biome (0 = no trainers) */
    uint8_t n_links;
    pr_biome_link links[PR_MAX_BIOME_LINKS];   /* biomeLinks, in order */
    /* pokemonPool[tier][timeOfDay] (tod stored at TimeOfDay + 1, ALL = -1 first) and trainerPool[tier],
     * as slices of pr_biome_pool_data */
    pr_pool_slice species[PR_BIOME_POOL_TIERS][5];
    pr_pool_slice trainers[PR_BIOME_POOL_TIERS];
} pr_biome_info;
extern const uint16_t pr_biome_pool_data[];
extern const pr_biome_info pr_biomes[PR_BIOME_ID_LIMIT];
extern const uint8_t pr_biome_order[];   /* Object.values(BiomeId) */
extern const int pr_biome_count;
/* biomeDepths: [depth, chance] per biome, built once at startup by initBiomeDepths */
typedef struct {
    bool set[PR_BIOME_ID_LIMIT];
    int depth[PR_BIOME_ID_LIMIT], chance[PR_BIOME_ID_LIMIT];
} pr_biome_depths;
/* initBiomeDepths(): walks the links from TOWN; reaching END jumps to a random biome, rolled on
 * `rng` (Phaser.Math.RND at startup), so the table isn't fixed data */
void pr_run_init_biome_depths(pr_biome_depths *d, pr_rng *rng);
/* generateRandomBiome(waveIndex) */
int pr_run_generate_random_biome(const pr_biome_depths *d, pr_rng *rng, int wave);
/* SelectBiomePhase.start() after the battle of `current_wave` in `current_biome`: resets the seed
 * (resetSeed() of the current wave), then returns the next biome, or -1 when the player holds a
 * Map and picks among `choices` (n_choices of them, in the order the menu lists them). */
int pr_run_select_next_biome(pr_run *run, const pr_biome_depths *d, int current_biome, int current_wave, bool has_map,
                             uint8_t choices[PR_MAX_BIOME_LINKS], int *n_choices);

/* getGeneratedWaveCycleOffset() and getGeneratedOffsetGym(), both from the run seed */
int pr_run_get_generated_wave_cycle_offset(const pr_run *run);
bool pr_run_get_generated_offset_gym(const pr_run *run);
/* arena.getTimeOfDay() in `biome` during `wave` (TimeOfDay) */
int pr_run_get_time_of_day(int biome, int wave, int wave_cycle_offset);
/* arena.randomSpecies(wave, level, 0, luck, isBoss) on run->rng, up to the evolution step: returns the
 * species handed to getWildSpeciesForLevel (not ported yet) and the isBoss it gets (*evo_is_boss).
 * is_boss < 0 = undefined. -1 with a PR_RUN_UNPORTED_* flag when it can't answer. */
int pr_run_random_species(pr_run *run, int biome, int wave, int level, int luck, int is_boss, int wave_cycle_offset,
                          bool *evo_is_boss);
/* arena.randomTrainerType(wave, isBoss) on run->rng */
int pr_run_random_trainer_type(pr_run *run, int biome, int wave, bool is_boss);

/* ---- enemy species for a level (src/evolution.c; data in gen/out/run_evolutions.c) ---- */
/* An evolution (target, level, evoLevelThreshold [STRONG, NORMAL, WILD], -1 = none), or a
 * getPrevolutionLevels(true) entry (prevolution, level, threshold) */
typedef struct { uint16_t species; uint8_t level; int8_t threshold[3]; } pr_evo;
typedef struct {
    uint16_t id;
    bool has_prevolution;               /* speciesDataRegistry.hasPrevolution */
    uint16_t evo_start, n_evo;          /* getEvolutions, in pr_evo_data */
    uint16_t prevo_start, n_prevo;      /* getPrevolutionLevels(true), in pr_prevo_data */
} pr_species_evo;
extern const pr_species_evo pr_species_evos[];   /* by SpeciesId */
extern const int pr_species_evo_count;
extern const pr_evo pr_evo_data[], pr_prevo_data[];
enum { PR_EVO_KIND_STRONG, PR_EVO_KIND_NORMAL, PR_EVO_KIND_WILD };   /* EvoLevelThresholdKind */
/* determineEnemySpecies(species, level, allowEvolving, ..., encounterKind, tryForcePrevo) on `rng`.
 * classic_wave20_trainer: currentBattle.waveIndex === 20 && gameMode.isClassic && currentBattle.trainer
 * (which blocks evolving). */
int pr_run_determine_enemy_species(pr_rng *rng, int species, int level, bool allow_evolving, int encounter_kind,
                                   bool try_force_prevo, bool classic_wave20_trainer);
/* species.getWildSpeciesForLevel(level, allowEvolving, isBoss, gameMode): kind NORMAL for a boss, else WILD */
int pr_run_get_wild_species_for_level(pr_rng *rng, int species, int level, bool allow_evolving, bool is_boss,
                                      bool classic_wave20_trainer);

/* ---- experience (src/exp.c; levels 1-99 in gen/out/run_exp.c) ---- */
/* GrowthRate (src/data/exp.ts, outside src/enums so battle-core's generator doesn't emit it;
 * gen/dump-exp.test.ts fails if upstream's names or values differ from these) */
enum { GROWTH_RATE_ERRATIC, GROWTH_RATE_FAST, GROWTH_RATE_MEDIUM_FAST, GROWTH_RATE_MEDIUM_SLOW, GROWTH_RATE_SLOW,
       GROWTH_RATE_FLUCTUATING };
#define PR_GROWTH_RATE_COUNT 6
extern const int32_t pr_exp_table[PR_GROWTH_RATE_COUNT][99];
/* getLevelTotalExp(level, growthRate) and getLevelRelExp(level, growthRate) (GrowthRate) */
double pr_run_get_level_total_exp(int level, int rate);
double pr_run_get_level_rel_exp(int level, int rate);

/* ---- helpers ---- */
/* executeWithSeedOffset(fn, offset, seedOverride): fn rolls on a fresh RNG sown from
 * shiftCharCodes(seedOverride || seed, offset), and RND's state comes back afterwards, so a
 * copy stands in for it. seed_override NULL = the run seed. */
pr_rng pr_run_seed_offset_rng(const pr_run *run, int offset, const pr_seedstr *seed_override);
double js_round(double x);   /* battle-core */
double js_pow(double x, double y);

#endif
