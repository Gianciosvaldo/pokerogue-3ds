/* reward-core: the rewards after a wave (the reward options, their pools, weights and luck upgrades, the
 * fixed rewards of some waves) and, next, what each reward does when it is picked.
 * Ported from upstream src/modifier/modifier-type.ts, src/modifier/init-modifier-pools.ts and
 * src/phases/select-modifier-phase.ts at 8555c08; function names follow upstream so each one diffs against its
 * TS original. It reads battle-core's and run-core's generated tables and RNG and never changes either.
 *
 * Who fills what: the run loop (game-core) owns the run state and fills a pr_reward_ctx from it before each
 * reward screen. A few inputs are other ports' answers (marked "caller" below): canSetStatus, the learnable
 * level moves, a move's type for item spawns. The diff test takes them from upstream, so reward-core is
 * checked given correct inputs. */
#ifndef PR_REWARD_H
#define PR_REWARD_H
#include <stdbool.h>
#include <stdint.h>
#include "pr/run.h"          /* pr_run: game mode, seeds, Phaser.Math.RND */
#include "reward_items.h"    /* enum pr_item: modifierTypes keys (generated) */

/* ---- the item catalogue (gen/out/reward_data.c, dumped from upstream) ---- */
/* Generator kinds: an item whose type is made when it is rolled (ModifierTypeGenerator) */
enum {
    PR_GEN_NONE, PR_GEN_BERRY, PR_GEN_BASE_STAT, PR_GEN_TEMP_STAT, PR_GEN_ATTACK_TYPE, PR_GEN_SPECIES_STAT,
    PR_GEN_RARE_SPECIES_STAT, PR_GEN_TM_COMMON, PR_GEN_TM_GREAT, PR_GEN_TM_ULTRA, PR_GEN_EVOLUTION_ITEM,
    PR_GEN_RARE_EVOLUTION_ITEM, PR_GEN_FORM_CHANGE_ITEM, PR_GEN_RARE_FORM_CHANGE_ITEM, PR_GEN_MINT,
    PR_GEN_TERA_SHARD, PR_GEN_SHUCKLE_JUICE, PR_GEN_BLACK_SLUDGE
};
enum {
    PR_ITEMF_HELD = 1,          /* its type is a PokemonHeldItemModifierType */
    PR_ITEMF_FORM_CHANGE = 2,   /* a FormChangeItemModifierType */
    PR_ITEMF_POKEMON = 4,       /* a PokemonModifierType: picking it opens the party menu */
    PR_ITEMF_PERSISTENT = 8,    /* it becomes a PersistentModifier in the player's list */
};
typedef struct {
    uint8_t gen;                /* PR_GEN_* */
    uint8_t flags;              /* PR_ITEMF_* (a generator: of the types it makes) */
    int16_t name, group;        /* string ids (pr_reward_strings); -1 for a generator (per type: pr_gen_types) */
    int16_t max_stack, max_stack_threshold;   /* a persistent item that isn't held: getMaxStackCount(),
                                               * getMaxStackCount(true); -1 */
    int16_t max_held;           /* a held item: getMaxHeldItemCount(); -2 = the holder's IV for the vitamin's stat;
                                 * -3 = per generated type (pr_gen_type.max_held, berries); -1 not held */
} pr_item_info;
extern const pr_item_info pr_items[PR_ITEM_COUNT_];
extern const char *const pr_item_keys[PR_ITEM_COUNT_];
/* Interned strings: English names and groups. Index 0 is an undefined group (NULL), which upstream's
 * duplicate check compares equal to another undefined group. */
extern const char *const pr_reward_strings[];
extern const int pr_reward_string_count;
/* Every type a generator can make: arg (BerryType, Stat, PokemonType, species stat key index, MoveId,
 * EvolutionItem, FormChangeItem, Nature, Shuckle Juice's stat change), its name, group and, held, its
 * getMaxHeldItemCount() (-2 a vitamin: the holder's IV; -1 not held); sorted by item, then arg */
typedef struct { uint16_t item; int16_t arg; int16_t name, group, max_held; } pr_gen_type;
extern const pr_gen_type pr_gen_types[];
extern const int pr_gen_type_count;
/* The species stat boosters' keys, as a generated one's arg */
enum { PR_SSB_LIGHT_BALL, PR_SSB_THICK_CLUB, PR_SSB_METAL_POWDER, PR_SSB_QUICK_POWDER, PR_SSB_DEEP_SEA_SCALE,
       PR_SSB_DEEP_SEA_TOOTH };

/* The PLAYER pool: per ModifierTier, its entries in order; weight -1 = a weight function (src/pools.c) */
typedef struct { uint16_t item; int16_t weight, max_weight; } pr_pool_entry_p;
extern const pr_pool_entry_p pr_player_pool_entries[];
extern const pr_pool_tier pr_player_pool[6];   /* run-core's pr_pool_tier: start, n (-1 = no such tier) */

/* TMs: tmPoolTiers (bits 0-2, -1 = not a TM) and isUnimplemented (bit 3) by MoveId; each species form's
 * getTms(formKey) as a slice of pr_tm_pool */
extern const int8_t pr_tm_tier[];
extern const int pr_tm_tier_count;
typedef struct { uint32_t start; uint16_t n; } pr_tm_list;
extern const pr_tm_list pr_tm_lists[];
extern const uint16_t pr_tm_pool[];
/* Species (ascending id): hasEvolutions (bit 0), hasFormChanges (bit 1), forms.length, first pr_reward_forms row
 * (max(forms.length, 1) rows: the species' own row when it has no forms) */
typedef struct { uint16_t id; uint8_t flags; uint8_t n_forms; uint16_t form_start; } pr_reward_species;
extern const pr_reward_species pr_reward_species_data[];
extern const int pr_reward_species_count;
typedef struct { uint16_t tms; int16_t key; uint8_t is_max; } pr_reward_form;   /* tm list, formKey string, isMax */
extern const pr_reward_form pr_reward_forms[];
/* Item evolutions (getEvolutions, the ones with an item, in order) by species */
typedef struct { uint16_t species, item; uint8_t level; int16_t pre_form_key; uint16_t cond_start, n_cond; int16_t evo_form_key; } pr_item_evo;
typedef struct { uint8_t key; int32_t a; uint16_t list_start, n_list; } pr_evo_cond;   /* EvoCondKey data */
typedef struct { uint16_t species, start, n; } pr_species_slice;
extern const pr_item_evo pr_item_evos[];
extern const pr_evo_cond pr_evo_conds[];
extern const int16_t pr_evo_cond_lists[];
extern const pr_species_slice pr_item_evo_species[];
extern const int pr_item_evo_species_count;
/* Form changes with an item trigger (getFormChanges, in order) by species. flags: bit 0 formKey has mega or
 * primal, bit 1 gigantamax or eternamax, bit 2 the trigger is active. Conditions (SpeciesFormChangeCondition
 * closures, classified by the dump): 1 hasAbility(a), 2 not the daily run, 3 hpRatio >= 0.5, 4 hpRatio < 0.5,
 * 5 the dex has caught species a; 0 = not a SpeciesFormChangeCondition */
typedef struct { uint16_t item; int16_t pre_form_key; uint8_t flags; uint16_t cond_start, n_cond; } pr_item_fc;
typedef struct { uint8_t kind; int16_t a; } pr_fc_cond;
extern const pr_item_fc pr_item_fcs[];
extern const pr_fc_cond pr_fc_conds[];
extern const pr_species_slice pr_item_fc_species[];
extern const int pr_item_fc_species_count;

/* An item and its generated argument (-1 = none): a ModifierType as the rolls see it */
typedef struct { uint16_t item; int16_t arg; } pr_item_type;
/* type.name and type.group as string ids */
int pr_item_type_name(pr_item_type t);
int pr_item_type_group(pr_item_type t);

/* ---- what the rolls read ---- */
#define PR_REWARD_MAX_LEVEL_MOVES 160
#define PR_REWARD_MAX_USED_TMS 64
#define PR_REWARD_MAX_ATTACK_TYPES 16
/* A party member (PlayerPokemon), as the weights and generators read it */
typedef struct {
    uint32_t id;                  /* Pokemon.id: what held items point at */
    int16_t species, form;        /* species.speciesId, formIndex */
    int16_t fusion_species;       /* fusionSpecies.speciesId, -1 = not a fusion */
    int16_t fusion_form;          /* fusionFormIndex */
    int8_t gender, fusion_gender; /* Gender (-1 genderless) */
    uint16_t level;
    double exp;                    /* PlayerPokemon.exp; Task 44 Rare/Rarer Candy syncs this when <= absolute cap */
    int32_t hp, max_hp;           /* hp, getMaxHp() */
    uint8_t status;               /* status.effect (StatusEffect), 0 = none */
    uint8_t nature;               /* getNature() */
    int8_t tera_type;             /* teraType */
    uint8_t friendship;
    uint16_t ability;             /* getAbility(true).id */
    uint16_t cur_ability;         /* getAbility().id (a Transform or Skill Swap still in summonData) */
    uint16_t passive;             /* getPassiveAbility().id if hasPassive(), else 0 */
    bool can_apply_ability, can_apply_passive;   /* caller: canApplyAbility(), canApplyAbility(true) (battle-core) */
    int8_t types[2]; uint8_t n_types;   /* getTypes(false, false, true, true): PARTY_TYPE evolutions */
    /* getMoveset() (the summon moveset while one is set): each move, its ppUsed, ppUp and getMovePp() */
    struct { uint16_t move; uint8_t pp_used, pp_up, max_pp; } moves[4];
    uint8_t n_moves;
    uint16_t base_moves[4];       /* this.moveset / getMoveset(true): the moves */
    uint8_t n_base_moves;
    bool pause_evolutions;
    bool allowed_in_challenge;    /* isAllowedInChallenge() */
    bool can_toxic, can_burn;     /* caller: canSetStatus(TOXIC or BURN, true, true, null, true) (battle-core) */
    bool has_learnable_level_moves;   /* caller: getLearnableLevelMoves().length > 0 (Memory Mushroom) */
    /* caller: getLevelMoves({includeEvolutionMoves: true, includeRelearnerMoves: true}) moves, which
     * getCompatibleTms(true, true, true) leaves out; and usedTMs */
    uint16_t level_moves[PR_REWARD_MAX_LEVEL_MOVES]; uint16_t n_level_moves;
    /* Task 43 Memory Mushroom metadata for the entries above. level -1 represents null;
     * source uses upstream LearnableMoveSource values. Existing callers may leave these zeroed,
     * which means ordinary LEVEL source at level 0. */
    int16_t level_move_levels[PR_REWARD_MAX_LEVEL_MOVES];
    int8_t level_move_sources[PR_REWARD_MAX_LEVEL_MOVES];
    /* getUnlockedEggMoves() input. Only unlocked egg moves are supplied. */
    uint16_t egg_moves[4]; uint8_t n_egg_moves;
    bool met_as_starter;          /* metBiome === -1 */
    bool egg_relearn_allowed;     /* ChallengeType.EGG_MOVE_RELEARN_AVAILABILITY ValueHolder */
    uint16_t used_tms[PR_REWARD_MAX_USED_TMS]; uint8_t n_used_tms;
    /* caller: the attack type booster's input: for each attack move in getMoveset(), in order, its types
     * for item spawns (VariableMoveTypeAttr.getTypesForItemSpawn or getMoveTypeForItemSpawn) */
    int8_t attack_types[PR_REWARD_MAX_ATTACK_TYPES]; uint8_t n_attack_types;
    int persistent_treasure;      /* getPersistentTreasureCount() (Gimmighoul's evolution) */
    /* Full application-state transport. Evolution item application queues a request;
     * these permanent attributes and stats are preserved until the caller executes it. */
    uint8_t ivs[6];
    int32_t stats[6];
    int8_t ability_index, fusion_ability_index;
    bool shiny, fusion_shiny, passive_flag;
    uint8_t variant, fusion_variant;
    int8_t luck, fusion_luck;
} pr_reward_mon;

/* One of the player's modifiers (globalScene.modifiers, in order) */
typedef struct {
    uint16_t item;                /* its type's id (enum pr_item) */
    int16_t arg;                  /* generated types: the argument (a form change item: FormChangeItem) */
    int32_t stack;                /* stackCount */
    int8_t owner;                 /* a held item: its holder's party index; -1 otherwise */
    int16_t battle_count;         /* lures and the Silver Poke Ball: getBattleCount() */
    bool active;                  /* a form change item: .active */
} pr_owned_item;

#define PR_REWARD_MAX_PARTY 6
#define PR_REWARD_MAX_OWNED 256
#define PR_REWARD_MAX_EVOLUTION_EVENTS 8
/* In enqueue (unshiftNew call) order; the phase runner consumes in reverse order.
 * Target species/form belong to the evolving component. Fusion requests retain
 * primary_species as FusionSpeciesFormEvolution does. form_key -1 means null. */
typedef struct {
    uint32_t pokemon_id;
    uint16_t primary_species, target_species, item;
    int16_t pre_form_key, evo_form_key;
    int16_t last_level;
    uint8_t evolution_level;
    bool fusion;
} pr_reward_evolution_event;
typedef struct {
    pr_run *run;                  /* mode, run seed, run->rng = Phaser.Math.RND */
    int wave;                     /* currentBattle.waveIndex */
    int luck;                     /* getPartyLuckValue(party): run-core's pr_run_get_party_luck_value */
    pr_reward_mon party[PR_REWARD_MAX_PARTY];
    int n_party;
    pr_owned_item items[PR_REWARD_MAX_OWNED];
    int n_items;
    int pokeball_counts[5];       /* pokeballCounts by PokeballType */
    int biome, time_of_day, weather;   /* arena: biomeId, getTimeOfDay(), weatherType (evolution conditions) */
    uint32_t challenges;          /* bit (1 << Challenges) per active challenge. The rolls read FRESH_START and
                                   * HARDCORE; MOVESET_RANDOMIZER's TM shuffle isn't ported (flagged); the other
                                   * challenges reach the rolls only through the caller's fields */
    bool unlocked_eviolite, unlocked_mini_black_hole;   /* gameData.isUnlocked(...) */
    bool fusions_boosted;         /* timedEventManager.areFusionsBoosted() */
    /* gameData.dexData[species].caughtAttr != 0; NULL = nothing caught */
    bool (*caught)(void *user, int species);
    void *caught_user;
    /* Task 44: LevelUpPhase requests produced by Rare/Rarer Candy application. */
    struct { int8_t party_idx; uint16_t old_level, new_level; } level_events[PR_REWARD_MAX_PARTY];
    uint8_t n_level_events;
    /* Caller supplies the actual unseeded getRandomStatus coin flip for DNA Splicers.
     * Required only when both input statuses are nonvolatile. Seeded RNG cannot reproduce it. */
    /* Current application only: caller resets availability before each use.
     * This is the actual external unseeded draw, never the seeded reward RNG. */
    bool fusion_status_choice_available;
    uint8_t fusion_status_choice; /* 0 secondary, 1 primary */
    pr_reward_evolution_event evolution_events[PR_REWARD_MAX_EVOLUTION_EVENTS];
    uint8_t n_evolution_events;
    bool evolution_events_overflow; /* no success reported when the request cannot be retained */
} pr_reward_ctx;

/* ---- the reward options ---- */
typedef struct {
    pr_item_type type;
    int8_t tier;                  /* type.tier (ModifierTier) */
    int8_t upgrade_count;         /* ModifierTypeOption.upgradeCount (the luck upgrade shown) */
    double cost;                  /* ModifierTypeOption.cost (0 for a reward) */
} pr_reward_option;

/* modifierPoolThresholds after regenerateModifierPoolThresholds(party, PLAYER, rerollCount): per tier, the
 * running totals of the non-zero weights and each one's pool index. Upstream keeps it between screens (a
 * copy of the screen after a TM rolls with the old one), so the caller keeps it too. */
#define PR_REWARD_MAX_TIER_ENTRIES 40
typedef struct {
    int n[6];
    int total[6][PR_REWARD_MAX_TIER_ENTRIES], index[6][PR_REWARD_MAX_TIER_ENTRIES];
} pr_reward_thresholds;

/* CustomModifierSettings (fixed battles, mystery encounters, the no-victory screen) */
#define PR_REWARD_MAX_OPTIONS 16
typedef struct {
    int8_t tiers[PR_REWARD_MAX_OPTIONS]; int n_tiers;                    /* guaranteedModifierTiers */
    pr_reward_option options[PR_REWARD_MAX_OPTIONS]; int n_options;      /* guaranteedModifierTypeOptions */
    uint16_t funcs[PR_REWARD_MAX_OPTIONS]; int n_funcs;                  /* guaranteedModifierTypeFuncs (items) */
    bool fill_remaining;
    double reroll_multiplier;     /* NaN = not set */
    bool allow_luck_upgrades;     /* default true */
} pr_reward_custom;

enum {
    PR_REWARD_UNPORTED_CHALLENGE = 1u << 8,   /* a challenge hook (TM compatibility...) the port doesn't model */
    PR_REWARD_UNPORTED_OPTIONS = 1u << 9,     /* more options, owned items or moves than the fixed tables hold */
    PR_REWARD_UNPORTED_FUNC = 1u << 10,       /* a guaranteed item func the port doesn't make */
};

/* regenerateModifierPoolThresholds(party, PLAYER, rerollCount) on ctx->run->rng (the generators roll) */
void pr_reward_regenerate_thresholds(pr_reward_ctx *ctx, int reroll_count, pr_reward_thresholds *th);
/* getPlayerModifierTypeOptions(count, party, modifierTiers, customModifierSettings) on ctx->run->rng.
 * tiers: the locked tiers (NULL = none); custom NULL = none. Returns how many options (out holds
 * PR_REWARD_MAX_OPTIONS), or -1 with a flag in ctx->run->unported. */
int pr_reward_get_player_options(pr_reward_ctx *ctx, const pr_reward_thresholds *th, int count, const int8_t *tiers,
                                 int n_tiers, const pr_reward_custom *custom, pr_reward_option *out);
/* SelectModifierPhase.getModifierCount(): 3, + Golden Poke Balls, + the Silver Poke Ball, or the custom count */
int pr_reward_get_modifier_count(const pr_reward_ctx *ctx, const pr_reward_custom *custom);

/* SelectModifierPhase.start(): resetSeed() on a first roll (reroll_count 0, not a copy), the thresholds (not
 * for a copy), the count and the options. A reroll passes reroll_count + 1 and, with the Lock Capsule's lock
 * on, the previous options' tiers. */
typedef struct {
    int reroll_count;
    bool is_copy;                 /* the screen again after a TM or Memory Mushroom (custom = the old options) */
    const int8_t *locked_tiers; int n_locked_tiers;   /* lockModifierTiers ? modifierTiers : none */
    /* A rerolled SelectModifierPhase owns its modifierTiers for the lifetime of the phase.
     * Keep a C-owned copy here so locked tiers survive pr_reward_reroll() and can be reused
     * if the phase is copied/reopened (TM / Memory Mushroom flows). */
    int8_t owned_locked_tiers[PR_REWARD_MAX_OPTIONS];
    int n_owned_locked_tiers;
    const pr_reward_custom *custom;
} pr_reward_select;
int pr_reward_select_start(pr_reward_ctx *ctx, const pr_reward_select *sel, pr_reward_thresholds *th,
                           pr_reward_option *out);

/* ---- the fixed rewards of a victory (VictoryPhase): ModifierRewardPhase items, in the order they're given,
 * and whether a reward screen (SelectModifierPhase) follows. Returns how many items. ---- */
int pr_reward_victory_rewards(const pr_run *run, int wave, uint16_t out[8], bool *select_screen);

/* ---- Memory Mushroom / move relearning (Task 43) ---- */
enum {
    PR_LEARN_SOURCE_LEVEL = 0,
    PR_LEARN_SOURCE_FUSION_LEVEL = 1,
    PR_LEARN_SOURCE_RELEARN = 2,
    PR_LEARN_SOURCE_FUSION_RELEARN = 3,
    PR_LEARN_SOURCE_EVOLUTION = 4,
    PR_LEARN_SOURCE_FUSION_EVOLUTION = 5,
    PR_LEARN_SOURCE_PREVO = 6,
    PR_LEARN_SOURCE_FUSION_PREVO = 7,
    PR_LEARN_SOURCE_TM = 8,
    PR_LEARN_SOURCE_FUSION_TM = 9,
    PR_LEARN_SOURCE_EGG = 10,
    PR_LEARN_SOURCE_FUSION_EGG = 11,
    PR_LEARN_SOURCE_OTHER = 12,
    PR_LEARN_SOURCE_FUSION_OTHER = 13,
};
typedef struct {
    int16_t level;                /* null upstream = -1 */
    uint16_t move;
    int8_t source;                /* PR_LEARN_SOURCE_* */
} pr_reward_memory_move;

enum {
    PR_REWARD_TARGET_NONE = 0,
    PR_REWARD_TARGET_REMEMBER_MOVE = 1, /* UiMode.PARTY + PartyUiMode.REMEMBER_MOVE_MODIFIER */
    PR_REWARD_TARGET_LEARN_MOVE = 2,    /* LearnMovePhase(..., LearnMoveType.MEMORY, cost) */
};
typedef struct {
    int mode;
    int party_idx;
    int option_idx;
    uint16_t move;
    double cost;
    bool copy_reward_screen;      /* SelectModifierPhase.copy() is queued before RememberMoveModifier.apply */
} pr_reward_target_state;

/* PlayerPokemon.getLearnableLevelMoves(): level/pre-evo/relearner entries supplied by the caller,
 * unlocked egg moves for run starters (except Daily / blocked challenges), then used TMs; moves already
 * known are removed and the result is stable-sorted by base LearnableMoveSource descending. */
int pr_reward_memory_moves(const pr_reward_ctx *ctx, int party_idx,
                           pr_reward_memory_move *out, int out_cap);

/* Selecting a Memory Mushroom opens PARTY in REMEMBER_MOVE_MODIFIER mode. Returns the number of
 * selectable moves, or 0 and PR_REWARD_TARGET_NONE if the target cannot relearn anything. */
int pr_reward_memory_begin(const pr_reward_ctx *ctx, int party_idx,
                           pr_reward_memory_move *out, int out_cap,
                           pr_reward_target_state *state);

/* Selecting one of the Memory Mushroom move rows queues LearnMovePhase with LearnMoveType.MEMORY.
 * Returns false for an invalid target/option. */
bool pr_reward_memory_choose(const pr_reward_ctx *ctx, int party_idx, int option_idx, double cost,
                             pr_reward_target_state *state);

/* ---- applying rewards (Task 10/18: healing, revives, PP, evolution items, form change items) ---- */
/* Apply a reward item to the player's party in ctx.
 * item: enum pr_item
 * arg: generated argument for items like evolution items / form change items, or 0
 * party_idx: 0..n_party-1, or -1 for Sacred Ash
 * move_idx: 0..3 for Ether/Max Ether/PP Up/PP Max, or -1
 * msg: if non-NULL, set to "It won’t have any effect." on failure, or NULL on success.
 * Returns true if applied, false if no effect. */
bool pr_reward_apply(pr_reward_ctx *ctx, uint16_t item, int16_t arg, int party_idx, int move_idx, const char **msg);
/* Task 44: LevelIncrementBoosterModifier / PokemonLevelIncrementModifier.
 * Upstream has RARE_CANDY (one party member) and RARER_CANDY (all party members); there is no
 * BITE_SIZED_CANDY in pinned 8555c08. Each Candy Jar stack adds one extra level to either item. */
int pr_reward_candy_level_count(const pr_reward_ctx *ctx);
bool pr_reward_apply_candy(pr_reward_ctx *ctx, uint16_t item, int party_idx);
double pr_reward_level_total_exp(int level, int growth_rate);

/* ---- passive inventory multipliers (EXP Items & Economy) ---- */
/* EXP Share multiplier per non-participant party member: (stackCount * 0.2) / n_participants (or 0.0 if no EXP Share held or n_participants <= 0) */
double pr_reward_exp_share_multiplier(const pr_reward_ctx *ctx, int n_participants);
/* EXP All / Oval Charm multiplier bonus for participants when n_participants > 1: stackCount * 0.2 (or 0.0) */
double pr_reward_multiple_participant_exp_bonus(const pr_reward_ctx *ctx);
/* EXP Balance linear interpolation weight: 0.2 * stackCount (or 0.0) */
double pr_reward_exp_balance_weight(const pr_reward_ctx *ctx);
/* Golden Punch money reward multiplier for the given attacking Pokemon ID: 0.5 * stackCount (or 0.0 if not held) */
double pr_reward_golden_punch_money_multiplier(const pr_reward_ctx *ctx, uint32_t pokemon_id);
/* Amulet Coin global money multiplier factor: 1.0 + 0.2 * stackCount */
double pr_reward_money_multiplier(const pr_reward_ctx *ctx);

/* ---- inventory item transfer & stealing mechanics (Grip Claw, Mini Black Hole, Black Sludge) ---- */
/* Attempt to transfer quantity of held item at item_idx in ctx->items to party member to_p.
 * If bypass_cap is true, stolen items bypass normal max stack limits per holder.
 * Returns true if transfer succeeded (at least 1 item moved), false otherwise. */
bool pr_reward_try_transfer_held_item(pr_reward_ctx *ctx, int item_idx, int to_p, int quantity, bool bypass_cap);

/* Grip Claw / Mini Black Hole / enemy steal attempt: steals quantity items from holder from_p to holder to_p.
 * If item_idx is -1, selects an item randomly.
 * Stolen items bypass max_held limits when bypass_cap is true.
 * Returns true if any item was stolen/transferred. */
bool pr_reward_steal_held_item(pr_reward_ctx *ctx, int from_p, int to_p, int item_idx, int quantity, bool bypass_cap);

/* Applies Black Sludge shop cost multiplier factor (2.5 if Black Sludge is held/active, or 1.0 otherwise). */
double pr_reward_black_sludge_shop_cost_multiplier(const pr_reward_ctx *ctx);

/* ---- shop economy & Lock Capsule mechanics ---- */
/* Calculates the shop reroll cost for the given context, reroll_count, Lock Capsule state (lock_rarities),
 * previous options when lock_rarities is true, and custom settings (or NULL).
 * Returns -1.0 if rerolls are disabled (e.g. reroll_multiplier < 0), or non-negative cost. */
double pr_reward_reroll_cost(const pr_reward_ctx *ctx, int reroll_count, bool lock_rarities,
                            const pr_reward_option *options, int n_options,
                            const pr_reward_custom *custom);

/* Extract tier array from current options for Lock Capsule locked_tiers input.
 * Fills out_tiers (must be at least n_options in size) with tier of each option.
 * Returns number of tiers written. */
int pr_reward_get_locked_tiers(const pr_reward_option *options, int n_options, int8_t *out_tiers);

/* Attempt to deduct cost from money balance.
 * Returns true and deducts cost if money >= cost and cost >= 0.
 * Returns false if money < cost or cost < 0. */
bool pr_reward_try_deduct_money(double *money, double cost);

/* Performs a shop reroll state machine transition:
 * 1. Calculates reroll cost.
 * 2. Checks and deducts cost from *money.
 * 3. Prepares sel (increments reroll_count, attaches locked_tiers if lock_rarities is true).
 * 4. Calls pr_reward_select_start to generate new options.
 * Returns number of generated options on success, or -1 on failure. */
int pr_reward_reroll(pr_reward_ctx *ctx, double *money, bool lock_rarities,
                     const pr_reward_option *prev_options, int n_prev_options,
                     pr_reward_select *sel, pr_reward_thresholds *th,
                     pr_reward_option *out_options);

#endif
