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
typedef struct { uint8_t biome, weight; } pr_biome_link;   /* weight 0 = a plain link, else [biome, weight] */
typedef struct {
    bool present;
    uint8_t trainer_chance;       /* arena.trainerChance in this biome (0 = no trainers) */
    uint8_t n_links;
    pr_biome_link links[PR_MAX_BIOME_LINKS];   /* biomeLinks, in order */
} pr_biome_info;
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

/* ---- helpers ---- */
/* executeWithSeedOffset(fn, offset, seedOverride): fn rolls on a fresh RNG sown from
 * shiftCharCodes(seedOverride || seed, offset), and RND's state comes back afterwards, so a
 * copy stands in for it. seed_override NULL = the run seed. */
pr_rng pr_run_seed_offset_rng(const pr_run *run, int offset, const pr_seedstr *seed_override);
double js_round(double x);   /* battle-core */
double js_pow(double x, double y);

#endif
