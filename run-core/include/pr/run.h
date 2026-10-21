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
    PR_RUN_UNPORTED_DAILY = 1u << 0,      /* a custom daily seed's config (event dailies: run->daily_custom) */
    PR_RUN_UNPORTED_CHALLENGES = 1u << 1, /* challenge hooks (FIXED_BATTLES...) */
    PR_RUN_UNPORTED_SPECIES = 1u << 3,       /* globalScene.randomSpecies (any species), for an empty biome pool */
    PR_RUN_UNPORTED_FIXED_BATTLE = 1u << 4,  /* a fixed battle's trainer (config closures) */
    PR_RUN_UNPORTED_TRAINER = 1u << 5,       /* a trainer config the data doesn't cover */
    PR_RUN_UNDEFINED_UPSTREAM = 1u << 2,  /* upstream's own answer is undefined here (e.g. randSeedItem([])) */
};

enum {
    PR_CHALLENGE_SINGLE_GENERATION = 1u << 0, /* Challenges.SINGLE_GENERATION */
    PR_CHALLENGE_SINGLE_TYPE       = 1u << 1, /* Challenges.SINGLE_TYPE */
    PR_CHALLENGE_FRESH_START       = 1u << 4, /* Challenges.FRESH_START */
    PR_CHALLENGE_RULES_PORTED = PR_CHALLENGE_SINGLE_GENERATION | PR_CHALLENGE_SINGLE_TYPE | PR_CHALLENGE_FRESH_START,
};

typedef struct pr_run {
    int mode;                  /* gameMode.modeId (GameModes) */
    bool has_challenges;       /* gameMode.challenges has an active challenge */
    uint32_t challenge_flags;  /* bit Challenges id; 0 with has_challenges=true means legacy/unknown challenge state */
    uint8_t challenge_generation; /* SINGLE_GENERATION value: 0 off, 1..9 */
    uint8_t challenge_type;       /* SINGLE_TYPE value: 0 off, 1..18 (PokemonType + 1) */
    uint8_t challenge_fresh_start;/* FRESH_START value: 0 off, 1..2 */
    pr_seedstr seed;           /* globalScene.seed: the run seed */
    pr_seedstr wave_seed;      /* globalScene.waveSeed */
    pr_rng rng;                /* Phaser.Math.RND */
    bool offset_gym;           /* globalScene.offsetGym */
    bool daily_custom;         /* the daily seed carries a custom config (gameMode.dailyConfig, parsed from an
                                * event seed): its overrides aren't ported. A plain daily seed has none */
    unsigned unported;
} pr_run;

/* ---- Challenge mode validation (src/challenges.c) ---- */
typedef struct {
    uint16_t species;
    uint8_t form;
    uint16_t fusion_species; /* 0 = not fused */
    uint8_t fusion_form;
} pr_challenge_mon;

/* ChallengeType.POKEMON_IN_BATTLE for Single Generation / Single Type. */
bool pr_run_challenge_mon_allowed(const pr_run *run, const pr_challenge_mon *mon);
/* Every non-fainted/usable party member must satisfy the active Mono-gen/Mono-type rules. */
bool pr_run_challenge_party_allowed(const pr_run *run, const pr_challenge_mon *party, int n_party);
/* Starter Choice validation for Single Generation, Single Type and Fresh Start. */
bool pr_run_challenge_starter_allowed(const pr_run *run, int species, int form);
/* Capture-to-party validation used by the offline run layer: a captured mon must be legal for the
 * active Mono-gen/Mono-type formation. Fresh Start does not restrict later captures upstream. */
bool pr_run_challenge_capture_allowed(const pr_run *run, const pr_challenge_mon *mon);
/* True when run->has_challenges includes hooks outside the three rules ported here. */
bool pr_run_has_unported_challenges(const pr_run *run);

/* ---- GameMode (src/game-mode.ts) ---- */
int pr_run_get_starting_level(pr_run *run);
int pr_run_get_starting_money(pr_run *run);
int pr_run_get_wave_for_difficulty(const pr_run *run, int wave, bool ignore_curve_changes);
bool pr_run_is_wave_final(int mode, int wave);
bool pr_run_is_boss(int wave);
bool pr_run_is_battle_classic_final_boss(const pr_run *run, int wave);
bool pr_run_is_paradox_wave(int mode, int wave);
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
/* getLevelForWave() for a battle of `wave`, rolling on `rng`. newBattle builds the Battle inside
 * executeWithSeedOffset(waveIndex << 3, waveSeed) (pr_run_battle_rng), where the battleSeed field
 * initializer rolls 16 times before the levels: pr_run_new_battle does both in order. */
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
/* gameMode.getOverrideSpecies(waveIndex), which arena.randomSpecies checks first and returns as it is
 * (no evolution step): the daily run's final boss, randSeedItem over the legend-like species with a base
 * total of 600 or more (not Eternatus or Arceus); -1 for none. pr_run_random_species returns it too, but a
 * caller composing the evolution step has to check this first. */
int pr_run_get_override_species(pr_run *run, int wave);
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

/* Victory experience. A party member as applyPartyExp sees it, and the modifiers it reads */
#define PR_MAX_EXP_PARTY 6
typedef struct { uint32_t id; int level; int hp; bool pokerus, participated; } pr_exp_member;
typedef struct { uint32_t pokemon_id; int stacks; double boost_percent; } pr_exp_egg;   /* Lucky/Golden Egg */
typedef struct {
    pr_exp_member party[PR_MAX_EXP_PARTY];   /* getPlayerParty(), in order */
    int n_party;
    int n_participants;                      /* currentBattle.playerParticipantIds.size */
    double max_exp_level;                    /* getMaxExpLevel() */
    bool trainer_battle;                     /* a trainer battle, or a mystery encounter's trainer battle */
    double me_exp_multiplier;                /* another mystery encounter's expMultiplier; NaN = not one */
    int exp_share_stacks, exp_balance_stacks, oval_charm_stacks;   /* the first one held (findModifier); 0 = none */
    pr_exp_egg eggs[16];                     /* PokemonExpBoosterModifiers, in modifier-list order */
    int n_eggs;
} pr_exp_party;
/* Pokemon.getExpValue() */
double pr_run_get_exp_value(int base_exp, int level);
/* applyPartyExp: returns how many members get experience (party indexes and amounts, in order) */
int pr_run_apply_party_exp(const pr_exp_party *pp, double exp_value, bool use_wave_index_multiplier, int wave,
                           int out_index[PR_MAX_EXP_PARTY], double out_exp[PR_MAX_EXP_PARTY]);

/* ---- between-wave upkeep (src/upkeep.c) ---- */
/* trySpreadPokerus() for the wave: pokerus[] is the party's flags in order, updated in place;
 * returns how many caught it */
int pr_run_try_spread_pokerus(const pr_run *run, int wave, bool *pokerus, int n_party);
/* addMoney (capped at Number.MAX_SAFE_INTEGER) */
double pr_run_add_money(double money, double amount);
/* The Coin Cases' interest (MoneyInterestModifier), each one's stack count in list order */
double pr_run_apply_money_interest(double money, const int *stacks, int n);
/* getPartyLuckValue(party): a party member as it sees it */
typedef struct {
    int species;
    int luck, fusion_luck;        /* .luck, .fusionLuck */
    bool fusion;                  /* isFusion() */
    bool allowed;                 /* isAllowedInBattle(): not fainted and allowed by the challenges */
} pr_luck_member;
/* The party's luck (0-14): the members' luck, +1 for each of a timed event's luck-boosted species, the
 * event's luck boost; in the daily run a roll on the run seed instead (a custom daily seed's luck isn't
 * ported) */
int pr_run_get_party_luck_value(pr_run *run, const pr_luck_member *party, int n, const int *event_species,
                                int n_event_species, int event_luck_boost);

/* ---- trainers (data in gen/out/run_trainers.c) ---- */
typedef struct {
    int8_t size, strength; int8_t strengths[8];   /* size, .strength, getStrength(i) */
    int8_t evo_kind;              /* .evoLevelThresholdKind (NORMAL when undefined) */
    uint8_t member[8];            /* per index: isSameSpecies bit 0, isBalanced bit 1, getEvoThresholdKind bits 2-3 */
    int8_t offsets[8];            /* per index: genPartyMember's compound-template offset (0 for a plain one) */
} pr_party_template;
/* partyTemplateFunc captured as segments: in game mode `mode`, from currentBattle.waveIndex `from_wave`
 * on (0 = no current battle), the template is pr_party_templates[template] */
typedef struct { uint8_t mode; uint16_t from_wave; uint16_t template; } pr_template_segment;
typedef struct {
    uint16_t type;                       /* TrainerType */
    bool double_only, has_double, has_genders, has_type_double, has_template_func;
    uint8_t n_templates;
    uint16_t templates[16];              /* config.partyTemplates, as pr_party_templates indexes */
    uint16_t segment_start, n_segments;  /* partyTemplateFunc, in pr_template_segments */
    /* names in the English locale for trainersCommon:<class>, .male and .female (-1 = no such key) */
    int16_t names, names_male, names_female;
} pr_trainer_info;
extern const pr_trainer_info pr_trainers[];   /* by TrainerType */
extern const int pr_trainer_count;
extern const pr_party_template pr_party_templates[];
extern const pr_template_segment pr_template_segments[];

/* ---- a new battle (src/newbattle.c) ---- */
typedef struct {
    int biome, trainer_chance;           /* the arena: biomeId and its trainerChance */
    int prev_wave;                       /* currentBattle.waveIndex before newBattle (0 = none yet) */
    int n_lures, n_double_chance_abilities;   /* what getDoubleBattleChance counts */
    bool enable_fixed_rivals;            /* Task 48: caller opts into the six ported Classic rival closures */
    bool enable_gym_e4;                   /* Task 49: caller opts into Gym/E4 fixed trainer closures */
    pr_me_save me;                       /* mysteryEncounterSaveData */
    bool player_female;                  /* settings.isPlayerFemale: fixed rival variant */
    /* Optional scene callback, evaluated only when a trainer/wild double roll
     * actually needs it. NULL retains the arithmetic-only count projection. */
    double (*double_chance)(void *ctx, int wave);
    void *double_chance_ctx;
} pr_new_battle_ctx;
typedef struct {
    int battle_type;                     /* BattleType */
    bool double_battle;
    int trainer_type, variant, template_index;   /* -1 for no trainer */
    int name, partner_name;              /* index among the class's locale names, -1 = none rolled */
    int n_levels, levels[PR_MAX_PARTY_TEMPLATE];   /* currentBattle.enemyLevels */
    bool me_spawn_chance_reset;          /* a mystery encounter sets encounterSpawnChance back to its base */
    char battle_seed[17];                /* currentBattle.battleSeed: where the battle's own RNG starts */
} pr_new_battle;
/* randomString(length, true) on `rng`; out holds length + 1 chars */
void pr_run_random_seed_string(pr_rng *rng, char *out, int length);
/* newBattle() for wave `wave` (not a loaded save), rolling on run->rng from resetSeed(wave).
 * Returns 0, or -1 with PR_RUN_UNPORTED_FIXED_BATTLE / _TRAINER / _DAILY in run->unported. */
int pr_run_new_battle(pr_run *run, int wave, const pr_new_battle_ctx *ctx, pr_new_battle *out);

/* ---- a new enemy Pokemon (src/pokemongen.c; data in gen/out/run_pokemongen.c) ---- */
/* Gender (src/data/gender.ts, outside src/enums; gen/dump-pokemongen.test.ts checks the values) */
enum { GENDER_GENDERLESS = -1, GENDER_MALE = 0, GENDER_FEMALE = 1 };
typedef struct {
    uint16_t id;
    double male_percent;          /* malePercent, -1 = null (genderless) */
    bool same_abilities;          /* ability2 === ability1 */
    bool has_hidden;              /* abilityHidden is set */
    uint16_t n_forms;             /* forms.length (0 for a species without forms) */
    uint16_t form_start;          /* in pr_form_gens: n_forms entries, or 1 for the species itself */
    /* getCompatibleFusionSpeciesFilter's inputs, and isCatchable */
    bool has_evolutions, has_prevolution, sub_legendary, legendary, mythical, trainer_forbidden, catchable;
    uint8_t starter_cost;
} pr_species_gen;
typedef struct {
    uint8_t n_types; int8_t types[2];   /* what the tera type is picked from, in order (no fusion) */
    bool has_variants;                  /* variantData has "<id>-<formKey>" or "<id>" */
    int8_t type1, type2;                /* the raw types (type2 -2 = null), for a fusion's getBaseTypes */
} pr_form_gen;
extern const pr_species_gen pr_species_gens[];   /* by SpeciesId */
extern const int pr_species_gen_count;
extern const pr_form_gen pr_form_gens[];
extern const int8_t pr_trainer_specialty[];      /* by TrainerType: specialtyType, -1 = none */
/* the Golden Bug Net's pool: a roll below `upto` (and at or past the entry before) picks `species` */
typedef struct { uint16_t species, upto; } pr_weighted_species;
extern const pr_weighted_species pr_golden_bug_net[];
extern const int pr_golden_bug_net_count, pr_golden_bug_net_total;
extern const int pr_trainer_specialty_count;

typedef struct {
    int species, level;
    int trainer_slot;             /* TrainerSlot: 0 NONE (wild), 1 TRAINER, 2 TRAINER_PARTNER */
    bool boss, shiny_lock;        /* the EnemyPokemon constructor's boss and shinyLock */
    /* the scene */
    int wave, biome, wave_cycle_offset;
    int battle_type;              /* currentBattle.battleType */
    int trainer_type;             /* currentBattle.trainer's TrainerType, -1 = none */
    int hidden_ability_charm_stacks, shiny_charm_stacks;   /* the player's charms, 0 = none */
    int enemy_fusion_stacks;      /* the enemy's EnemyFusionChanceModifier, 0 = none */
    double enemy_fusion_chance;   /* its .chance */
    uint16_t trainer_id, secret_id;   /* gameData */
    bool event_active;            /* timedEventManager.isEventActive() */
    double event_shiny_multiplier;    /* getShinyEncounterMultiplier() */
    double event_trainer_shiny_chance;   /* getClassicTrainerShinyChance() */
} pr_enemy_gen_ctx;
typedef struct {
    int ability_index;
    int fusion_species;           /* -1 = not a fusion; else generateFusionSpecies' species, ability index,
                                   * gender and form */
    int fusion_ability_index, fusion_gender, fusion_form;
    uint32_t id;
    int ivs[6];
    int gender, form, nature, tera_type;
    int boss_segments;            /* 0 = not a boss */
    bool shiny;
    int variant, luck;
    int friendship;               /* set for a trainer's Pokemon; -1 = species.baseFriendship (not dumped) */
    /* Task 48: a rival member's executeWithSeedOffset RNG is restored only after the
     * EnemyPokemon constructor and addEnemyPokemon postProcess have both finished. */
    bool rival_pending;
    int8_t rival_fight, rival_slot;
    pr_rng rival_rng;
    int moveset[4];               /* moveset (0 = none) */
} pr_enemy_gen;

/* Returns fixed moveset for Eternatus */
void pr_run_get_eternatus_moveset(int form, bool inverse_battle, int moveset[4]);
/* new EnemyPokemon(species, level, trainerSlot, boss, shinyLock) without a data source, on run->rng:
 * the Pokemon constructor's rolls and setBoss, which come before generateAndPopulateMoveset */
int pr_run_enemy_pokemon_base(pr_run *run, const pr_enemy_gen_ctx *ctx, pr_enemy_gen *out);
int pr_run_enemy_pokemon_base_rng(pr_rng *rng, pr_run *run, const pr_enemy_gen_ctx *ctx, pr_enemy_gen *out);

/* ---- daily runs (src/daily.c) ---- */
int pr_run_get_daily_starters(const pr_run *run, pr_enemy_gen out_starters[3], int out_species[3]);
int pr_run_get_daily_starting_biome(const pr_run *run);
/* the rest of the EnemyPokemon constructor after generateAndPopulateMoveset: shiny, variant (on the
 * wave seed at offset id), luck, a trainer's Pokemon's IVs and friendship; then addEnemyPokemon's
 * second IV roll for a boss (its postProcess callback and dev overrides aren't modelled) */
int pr_run_enemy_pokemon_finish(pr_run *run, const pr_enemy_gen_ctx *ctx, pr_enemy_gen *out);
/* getTypes() of a new enemy (no tera, no summon data): its form's types, combined with its fusion's.
 * Returns how many (1 or 2), or -1 where upstream reads an undefined form */
int pr_run_enemy_types(const pr_enemy_gen *mon, int species, int8_t types[2]);
/* battleScene.getSpeciesFormIndex(species, gender, undefined, false) for an enemy, on run->rng */
int pr_run_get_species_form_index(pr_run *run, const pr_enemy_gen_ctx *ctx, int gender);
/* Player form selection during EggLapsePhase/EggHatchPhase; uses the isolated egg RNG. */
int pr_run_get_egg_form_index(pr_rng *rng, pr_run *run, const pr_enemy_gen_ctx *ctx, int gender);

/* ---- a trainer's party member species (src/trainerspecies.c; data in gen/out/run_trainerspecies.c) ---- */
typedef struct {
    bool catchable;               /* isCatchable() */
    uint16_t root;                /* getRootSpeciesId() */
    int8_t types[2];              /* type1, type2 (-2 = null) */
} pr_species_trainer_info;
extern const pr_species_trainer_info pr_species_trainer[];   /* in pr_species_gens order (ascending id) */
typedef struct {
    uint16_t type;                /* TrainerType */
    bool has_pools;               /* config.speciesPools is set */
    struct { uint16_t start; int16_t n; } tiers[5];   /* by TrainerPoolTier, in pr_trainer_pool_entries; n -1 = no such tier */
    int16_t filter;               /* config.speciesFilter as a row of pr_trainer_species_filters, -1 = none */
    uint16_t sig_start, n_sig;    /* roots of signatureSpecies[TrainerType[type]], in pr_trainer_signature_roots */
    bool static_party, same_seed; /* hasStaticParty, useSameSeedForAllMembers */
    uint16_t derived_type;        /* getDerivedType() */
    uint16_t type_double;         /* trainerTypeDouble, 0 = none */
    bool double_only;
    uint16_t member_funcs;        /* bit i + 8: partyMemberFuncs has key i (-8 to 7; -1 is the last member) */
    uint16_t group_start, n_groups;   /* signatureSpecies as groups, in pr_trainer_signature_groups */
} pr_trainer_species;
typedef struct { uint16_t start, n; } pr_signature_group;   /* in pr_trainer_signature_species */
extern const pr_signature_group pr_trainer_signature_groups[];
extern const uint16_t pr_trainer_signature_species[];
extern const pr_trainer_species pr_trainer_species_data[];   /* by TrainerType */
extern const int pr_trainer_species_count;
extern const int16_t pr_trainer_pool_entries[];   /* a species, or -1 for an entry that is a list of species */
extern const uint16_t pr_trainer_signature_roots[];
extern const uint8_t pr_trainer_species_filters[];   /* pr_trainer_filter_bytes per row, bit i = species i */
extern const int pr_trainer_filter_bytes;

#define PR_MAX_PARTY_TYPES 16
typedef struct {
    int trainer_type, level, wave;
    int evo_kind;                 /* template.evoLevelThresholdKind (EvoLevelThresholdKind), NORMAL when undefined */
    bool balanced;                /* template.isBalanced(currentBattle.enemyParty.length) */
    bool classic_wave20_trainer;  /* as for pr_run_determine_enemy_species */
    int n_party;                  /* the enemy party so far: */
    uint16_t party_roots[6];      /* each one's species.getRootSpeciesId() */
    int n_party_types;
    int8_t party_types[PR_MAX_PARTY_TYPES];   /* enemyParty.flatMap(p => p.getTypes()) */
} pr_party_species_ctx;
/* trainer.genNewPartyMemberSpecies(level, strength) on run->rng (genPartyMember rolls it on the run seed at
 * its own offset: the caller sows run->rng). Returns the species, or -1 with PR_RUN_UNDEFINED_UPSTREAM
 * where upstream loops forever or picks from an empty list, PR_RUN_UNPORTED_TRAINER for an unknown type. */
int pr_run_gen_new_party_member_species(pr_run *run, const pr_party_species_ctx *ctx);

/* getRootSpeciesId() of `species`, -1 if unknown */
int pr_run_species_root(int species);

/* A trainer's party, member by member (src/trainerparty.c) */
typedef struct {
    pr_enemy_gen_ctx enemy;       /* the scene addEnemyPokemon sees (wave, biome, trainer_type, battle_type...);
                                   * species, level, slot, boss and shinyLock are set per member */
    bool is_double;               /* trainer.isDouble() */
    bool classic_wave20_trainer;
    bool enable_gym_e4;            /* Task 49: caller opts into Gym/E4 partyMemberFuncs */
    const pr_party_template *template_;   /* trainer.getPartyTemplate() */
    int n_levels, levels[PR_MAX_PARTY_TEMPLATE];   /* currentBattle.enemyLevels */
} pr_trainer_party_ctx;
typedef struct {
    int n;
    uint16_t species[PR_MAX_PARTY_TEMPLATE];
    int levels[PR_MAX_PARTY_TEMPLATE];          /* postProcess-adjusted level; normally ctx->levels */
    pr_enemy_gen mons[PR_MAX_PARTY_TEMPLATE];   /* each member's rolls up to its moveset */
} pr_trainer_party;
/* genPartyMember(e) for every level in turn, each on its own RNG (the run seed at the trainer's offset);
 * run->rng is left as it was. Returns 0, or -1 with a flag (partyMemberFuncs: PR_RUN_UNPORTED_TRAINER)
 * and the members made so far in out->n. */
int pr_run_gen_trainer_party(pr_run *run, const pr_trainer_party_ctx *ctx, pr_trainer_party *out);
/* Rival partyMemberFuncs for Classic waves 8/25/55/95/145/195. fight is 1..6. */
int pr_run_gen_rival_party(pr_run *run, const pr_trainer_party_ctx *ctx, int fight, pr_trainer_party *out);
/* Called by pr_run_enemy_pokemon_finish after the rival member's constructor state is complete. */
void pr_run_rival_finish(pr_run *run, const pr_enemy_gen_ctx *ctx, pr_enemy_gen *out);

/* Task 49 Gym/E4 closures. The member helper is called with the exact partyMemberFuncs key selected
 * by Trainer.genPartyMember and returns 1 when Task 49 owns it, 0 otherwise, -1 on error. */
int pr_run_gen_gym_e4_closure_member(pr_run *run, const pr_trainer_party_ctx *ctx,
                                     int index, int func_key, pr_enemy_gen *out, int *out_species);
/* Classic/Challenge fixed E4/Champion trainer selection at 182/184/186/188/190; -1 otherwise. */
int pr_run_fixed_e4_trainer(const pr_run *run, int wave);

/* A wild battle's first enemy (src/wildenemy.c) */
typedef struct {
    pr_enemy_gen_ctx enemy;       /* the scene (wave, biome, wave_cycle_offset, charms, ids, events...) */
    int level;                    /* currentBattle.enemyLevels[0] */
    int luck;                     /* getPartyLuckValue(party) */
    bool golden_bug_net;          /* the player holds a BoostBugSpawnModifier */
    bool fresh_start;             /* gameMode.hasChallenge(FRESH_START) */
} pr_wild_enemy_ctx;
typedef struct {
    int species;
    bool boss, shiny_lock;        /* what addEnemyPokemon was given */
    pr_enemy_gen mon;             /* its rolls up to the moveset */
} pr_wild_enemy;
/* The EncounterPhase's first wild enemy on run->rng, up to its moveset */
int pr_run_gen_wild_enemy(pr_run *run, const pr_wild_enemy_ctx *ctx, pr_wild_enemy *out);

/* ---- an enemy's held items (src/enemyitems.c; data in gen/out/run_enemyitems.c) ---- */
enum { PR_POOL_PLAIN, PR_POOL_BERRY, PR_POOL_BASE_STAT, PR_POOL_ATTACK_TYPE };   /* generator kinds */
typedef struct { uint8_t kind, item; uint16_t weight; } pr_pool_entry;
typedef struct { uint16_t start; int16_t n; } pr_pool_tier;   /* n -1: the pool has no such tier */
extern const pr_pool_entry pr_pool_entries[];
extern const pr_pool_tier pr_enemy_pools[3][6];   /* [WILD, TRAINER, ENEMY_BUFF][ModifierTier] */
extern const int16_t pr_item_max_stacks[][4];     /* a buff's getMaxStackCount() and getMaxStackCount(true) below wave
                                                   * 2000, then both from wave 2000; -1 for other items */
extern const char *const pr_item_names[];         /* modifierTypes keys */
extern const int pr_item_count, pr_item_mini_black_hole;
extern const uint8_t pr_trainer_item_flags[];     /* by TrainerType: bit 0 config.isBoss, bit 1 genModifiersFunc */
extern const int pr_trainer_item_flag_count;

#define PR_MAX_ENEMY_ITEMS 1024   /* ceil(wave / 10) chances, x2.5 at a final wave, plus the Mini Black Hole */
typedef struct {
    bool boss;                    /* isBoss() */
    int n_attack_types;           /* the attack type booster's input: for each attack move in the moveset, in */
    int8_t attack_types[16];      /* order, getTypesForItemSpawn / getMoveTypeForItemSpawn */
} pr_item_enemy;
typedef struct {
    int wave;
    bool trainer_battle, double_battle;
    int trainer_type;             /* -1 = none */
    int n_party;
    pr_item_enemy party[6];       /* getEnemyParty(); the field is the first one, or two in a double */
} pr_enemy_items_ctx;
typedef struct { uint8_t item; int8_t arg; } pr_enemy_item;   /* arg: BerryType, Stat or PokemonType, -1 */
typedef struct {
    int n[6];
    pr_enemy_item items[6][PR_MAX_ENEMY_ITEMS];   /* each enemy's items in the order they're added */
    bool overflow;                /* an enemy got more than PR_MAX_ENEMY_ITEMS (past endless wave ~4,000) */
} pr_enemy_items;
/* The EncounterPhase's item step on run->rng: regenerateModifierPoolThresholds(enemy field, WILD or
 * TRAINER), whose generators roll once each, then generateEnemyModifiers() without held-item configs
 * (those are mystery encounters'). Assumes no enemy modifiers are left from before. Returns 0, or -1 with
 * PR_RUN_UNPORTED_TRAINER for a trainer with a genModifiersFunc. */
int pr_run_generate_enemy_items(pr_run *run, const pr_enemy_items_ctx *ctx, pr_enemy_items *out);

/* The endless enemy buffs (AddEnemyBuffModifierPhase, every 50th endless wave): the buffs the enemy side
 * holds, which carry over from wave to wave */
typedef struct { int n; struct { uint8_t item; int stack; } buff[32]; } pr_enemy_buffs;
/* AddEnemyBuffModifierPhase for `wave` on run->rng: regenerateModifierPoolThresholds(ENEMY_BUFF), then
 * ceil(wave / 250) buffs of the wave's tier (ULTRA every 1000th wave, GREAT every 250th, else COMMON),
 * each getEnemyBuffModifierForWave (retries while it would overflow a held buff), added to `buffs`.
 * Returns 0, or -1 with PR_RUN_UNDEFINED_UPSTREAM where upstream draws from an empty tier (and throws). */
int pr_run_add_enemy_buffs(pr_run *run, int wave, pr_enemy_buffs *buffs);

/* ---- helpers ---- */
/* executeWithSeedOffset(fn, offset, seedOverride): fn rolls on a fresh RNG sown from
 * shiftCharCodes(seedOverride || seed, offset), and RND's state comes back afterwards, so a
 * copy stands in for it. seed_override NULL = the run seed. */
pr_rng pr_run_seed_offset_rng(const pr_run *run, int offset, const pr_seedstr *seed_override);
double js_round(double x);   /* battle-core */
double js_pow(double x, double y);

#endif
