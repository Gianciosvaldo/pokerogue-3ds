/* Battle state and the ported Pokemon/damage functions.
 * Function names follow upstream (src/field/pokemon.ts etc.) so each C function
 * can be diffed against the TS method it replaces. */
#ifndef PR_BATTLE_H
#define PR_BATTLE_H
#include <stdbool.h>
#include <stdint.h>
#include "pr/rng.h"
#include "pr_data.h"

#define PR_MAX_UNPORTED 16
#define PR_MAX_BERRY_HISTORY 128

/* arena.terrainType (src/data/terrain.ts TerrainType; not part of the dumped enums) */
enum { TERRAIN_NONE, TERRAIN_MISTY, TERRAIN_ELECTRIC, TERRAIN_GRASSY, TERRAIN_PSYCHIC };

/* State the port doesn't model yet, reported in the unported list next to attribute
 * classes (ids continue after the last class id). */
enum {
    PR_FEAT_DOUBLES = PR_AC_COUNT_,  /* more than one Pokemon per side */
    PR_FEAT_MODIFIERS,               /* a modifier whose own class has no port (see pr_modifier) */
    PR_FEAT_MOVE_HISTORY,            /* moveHistory / attacksReceived entries older than the ones kept */
    PR_FEAT_TURN_ORDER,              /* last turn order entries beyond the ones kept, or off the field */
    PR_FEAT_FAINT_HISTORY,
    PR_FEAT_WAVE,                    /* wave data the caller didn't provide (wave seed, level cap) */
    PR_FEAT_PARTY,                   /* the party (bt->party) the caller didn't provide */
    PR_FEAT_HIT_STATE,               /* state kept from an earlier hit of the same move use */
    PR_FEAT_GLOBAL_RNG,              /* Phaser.Math.RND's state the caller didn't provide */
    PR_FEAT_BOSS_SEGMENTS,           /* what damage to a boss sets off (segments cleared, their stat boosts) */
    /* the turn engine */
    PR_FEAT_TAG_LAPSE,               /* a battler tag lapses or is removed (its lapse/onRemove) */
    PR_FEAT_ARENA_TAG_TURN,          /* arena tags on the turn path (lapse, protection, Trick Room...) */
    PR_FEAT_WEATHER_TURN,            /* weather or terrain on the turn path (damage, lapse, blocked moves) */
    PR_FEAT_TURN_MODIFIERS,          /* held items and other modifiers on the turn path */
    PR_FEAT_FORM_CHANGE,             /* triggerPokemonFormChange for a species with a matching trigger */
    PR_FEAT_MOVE_EFFECTS,            /* a move's MoveEffectAttrs (secondary effects, recoil, drain...) */
    PR_FEAT_ENGINE_LIMIT,            /* a fixed-size table of the engine ran out */
    PR_FEAT_FINAL_BOSS,
    PR_FEAT_STATUS_TURN,             /* a status effect's turn counter or end-of-turn damage */
    PR_FEAT_MOVE_CONDITION,          /* a move's own condition (move.applyConditions) with no C port */
    PR_FEAT_TRAINER,                 /* a trainer or mystery encounter battle's own rules */
    PR_FEAT_PHASE_0,                 /* + pr_phase_type: a phase that isn't ported was created */
    PR_FEAT_TAG_0 = PR_FEAT_PHASE_0 + 64,  /* + BattlerTagType: that tag added, lapsed or removed, not ported */
    PR_FEAT_COUNT_ = PR_FEAT_TAG_0 + BATTLER_TAG_TYPE_COUNT_
};
const char *pr_unported_name(uint16_t id);
const char *pr_phase_name_of(int phase_type);  /* src/engine.c */

/* Modifiers (src/modifiers.c): upstream's PersistentModifier instances, held items
 * included. Classes are the upstream ones the battle logic queries; the parent table
 * gives instanceof. */
enum {
    PR_MOD_NONE,
    PR_MOD_PERSISTENT,               /* PersistentModifier (any class without a port, e.g. Exp. Charm) */
    PR_MOD_HELD_ITEM,                /* PokemonHeldItemModifier (Leftovers, berries... whose effects aren't on the ported paths) */
    PR_MOD_TURN_HEAL,                /* Leftovers */
    PR_MOD_HIT_HEAL,                 /* Shell Bell */
    PR_MOD_TEMP_STAT_STAGE_BOOSTER,  /* X Attack, X Defense... (side-wide) */
    PR_MOD_TEMP_CRIT_BOOSTER,        /* Dire Hit (side-wide) */
    PR_MOD_BASE_STAT,                /* vitamins */
    PR_MOD_BASE_STAT_TOTAL,          /* Shuckle Juice */
    PR_MOD_BASE_STAT_FLAT,           /* Old Gateau */
    PR_MOD_INCREMENTING_STAT,        /* Macho Brace */
    PR_MOD_STAT_BOOSTER,
    PR_MOD_EVOLUTION_STAT_BOOSTER,   /* Eviolite */
    PR_MOD_SPECIES_STAT_BOOSTER,     /* Light Ball, Thick Club, Metal Powder... */
    PR_MOD_CRIT_BOOSTER,             /* Scope Lens */
    PR_MOD_SPECIES_CRIT_BOOSTER,     /* Leek */
    PR_MOD_ATTACK_TYPE_BOOSTER,      /* Charcoal, Mystic Water... */
    PR_MOD_NATURE_WEIGHT,            /* Soul Dew */
    PR_MOD_MULTI_HIT,                /* Multi Lens */
    PR_MOD_FIELD_EFFECT,             /* Mystical Rock */
    PR_MOD_ENEMY_PERSISTENT,
    PR_MOD_ENEMY_DAMAGE_MULTIPLIER,
    PR_MOD_ENEMY_DAMAGE_BOOSTER,     /* the enemy's Damage token */
    PR_MOD_ENEMY_DAMAGE_REDUCER,     /* the enemy's Protection token */
    PR_MOD_ENEMY_TURN_HEAL,          /* Recovery token */
    PR_MOD_ENEMY_ATTACK_STATUS_EFFECT_CHANCE,
    PR_MOD_ENEMY_STATUS_EFFECT_HEAL_CHANCE,
    PR_MOD_ENEMY_ENDURE_CHANCE,
    PR_MOD_ENEMY_FUSED_CHANCE,
    PR_MOD_SURVIVE_DAMAGE,           /* Focus Band */
    PR_MOD_BYPASS_SPEED_CHANCE,      /* Quick Claw */
    PR_MOD_FLINCH_CHANCE,            /* King's Rock */
    PR_MOD_RESET_NEGATIVE_STAT_STAGE,/* White Herb */
    PR_MOD_BERRY,                    /* Berries */
    PR_MOD_TURN_STATUS_EFFECT,       /* Toxic Orb / Flame Orb; status_effect selects identity */
    PR_MOD_MOVE_ACCURACY_BOOSTER,    /* Wide Lens: num=accuracyAmount (5) */
    PR_MOD_INSTANT_REVIVE,          /* Reviver Seed; max held stack 1 */
    PR_MOD_DAMAGE_MONEY_REWARD,    /* Golden Punch */
    PR_MOD_MONEY_MULTIPLIER,       /* Amulet Coin dependency of damage money */
    PR_MOD_PRESERVE_BERRY,          /* Berry Pouch; side-wide */
    PR_MOD_COUNT_
};
extern const uint8_t pr_mod_parent[PR_MOD_COUNT_];
extern const char *const pr_mod_class_name[PR_MOD_COUNT_];   /* upstream class names */
int pr_mod_class_by_name(const char *name);                  /* -1 when unknown */

typedef struct pr_modifier {
    uint8_t cls;
    /* The upstream object is a subclass the port doesn't know (its nearest known ancestor
     * is cls). Queries that would call its own shouldApply/apply report it unported. */
    bool unknown_subclass;
    bool is_transferable;          /* PokemonHeldItemModifier.isTransferable */
    int64_t pokemon_id;            /* PokemonHeldItemModifier.pokemonId (ids are 32-bit unsigned); -1 = any Pokemon */
    int32_t stack_count, virtual_stack_count;
    int8_t stat;                   /* BaseStatModifier / TempStatStageBoosterModifier stat */
    uint8_t stats_mask;            /* StatBoosterModifier.stats, bit per Stat */
    int8_t move_type;              /* AttackTypeBoosterModifier.moveType */
    int16_t num;                   /* CritBoosterModifier.stageIncrement, PokemonBaseStatTotalModifier.statModifier */
    /* StatBoosterModifier.multiplier, AttackTypeBoosterModifier.boostMultiplier,
     * TempStatStageBoosterModifier.boost, EnemyDamageMultiplierModifier.damageMultiplier */
    double mult;
    uint16_t species[4];           /* Species*Modifier.species */
    uint8_t n_species;
    int8_t berry_type;             /* BerryModifier.berryType */
    int8_t status_effect;          /* TurnStatusEffectModifier.getStatusEffect(): TOXIC or BURN */
} pr_modifier;
#define PR_MAX_MODIFIERS 96        /* per side */

/* Battler tags (src/tags.c): summonData.tags, in upstream order. A tag's class and the
 * parameters getBattlerTag() gives it (Flash Fire's type and boost...) follow from its
 * type; only what can change after creation is stored. */
typedef struct pr_battler_tag {
    uint8_t type;                  /* BattlerTagType */
    int16_t turn_count;
    uint16_t source_move;          /* 0 = none */
    int64_t source_id;             /* -1 = none */
    int8_t crit_stages;            /* CritBoostTag.critStages */
    int8_t stockpiled_count;       /* StockpilingTag.stockpiledCount */
    int8_t stock_def, stock_spdef; /* StockpilingTag.statChangeCounts[DEF], [SPDEF] */
    int16_t autotomize_count;      /* AutotomizedTag.autotomizeCount */
    int8_t faint_count;            /* SupremeOverlordTag.faintCount */
    int8_t stat;                   /* HighestStatBoostTag.stat */
    double multiplier;             /* HighestStatBoostTag.multiplier */
    int32_t hp;                    /* SubstituteTag.hp */
    bool in_focus;                 /* SubstituteTag.sourceInFocus */
    int8_t source_index;           /* SeedTag.sourceIndex (-1 = unset) */
    bool activated;                /* ShellTrapTag.activated */
    bool base_pure_flying;         /* RoostedTag.isBasePureFlying */
    uint16_t move_id;              /* DisabledTag.moveId, EncoreTag.moveId (0: NONE, or undefined) */
    bool added_this_turn;          /* EncoreTag.#addedThisTurn (private upstream, so not dumped) */
} pr_battler_tag;
#define PR_MAX_BATTLER_TAGS 24
/* summonData.types: Roost gives Flying back on top of up to three others */
#define PR_MAX_SUMMON_TYPES 4

/* Stored Healing Wish/Lunar Dance effects, in queue order. Source deduplicates
 * by position and move. Bounds are reported, never truncated silently. */
#define PR_MAX_PENDING_HEALS 16
typedef struct pr_pending_heal {
    int64_t source_id;
    uint32_t pokemon_name;         /* message identity frozen when queued */
    uint16_t move_id;
    uint8_t target_index;
    uint8_t restore_pp;
} pr_pending_heal;

/* Arena tags: arena.tags, in upstream order */
typedef struct pr_arena_tag {
    uint8_t type;                  /* ArenaTagType */
    uint8_t side;                  /* ArenaTagSide */
    int16_t turn_count, max_duration;
    uint16_t source_move;          /* 0 = none */
    int64_t source_id;             /* -1 = none */
    int8_t source_count;           /* SuppressAbilitiesTag.sourceCount */
    int8_t layers;                 /* EntryHazardTag.layers (1 for the other tags) */
    bool being_removed;            /* SuppressAbilitiesTag.beingRemoved */
    uint8_t n_pending_heals;
    pr_pending_heal pending_heals[PR_MAX_PENDING_HEALS];
} pr_arena_tag;
#define PR_MAX_ARENA_TAGS 24

/* arena.positionalTagManager.tags: Future Sight and Doom Desire (DelayedAttackTag), Wish (WishTag) */
typedef struct pr_positional_tag {
    uint8_t type;                  /* PositionalTagType */
    int16_t turn_count;
    int8_t target_index;           /* BattlerIndex of the slot it lands on */
    int64_t source_id;             /* DelayedAttackTag.sourceId */
    uint16_t source_move;          /* DelayedAttackTag.sourceMove */
    double heal_hp;                /* WishTag.healHp */
    uint32_t pokemon_name;         /* WishTag.pokemonName: its user as a message names it (pr_msg_pokemon) */
} pr_positional_tag;
#define PR_MAX_POSITIONAL_TAGS 8

/* A summonData.moveHistory / moveQueue entry (TurnMove) */
enum { PR_MOVE_RESULT_UNDEFINED = 0xff };   /* TurnMove.result left undefined */
typedef struct pr_turn_move {
    uint16_t move;                 /* MoveId */
    uint8_t use_mode;              /* MoveUseMode */
    uint8_t result;                /* MoveResult, or PR_MOVE_RESULT_UNDEFINED */
    int8_t targets[4];             /* BattlerIndex; the damage-path difftest leaves them empty */
    uint8_t n_targets;
} pr_turn_move;
/* The newest entries kept per Pokemon. The deepest ported read is Rollout's (limit + 1 = 6);
 * a read past the kept entries reports PR_FEAT_MOVE_HISTORY instead of guessing. */
#define PR_MAX_MOVE_HISTORY 8
#define PR_MAX_MOVE_QUEUE 4

/* A turnData.attacksReceived entry (AttackMoveResult) */
typedef struct pr_attack_record {
    uint16_t move;                 /* MoveId */
    uint8_t result;                /* HitResult */
    bool critical;
    int8_t source_battler_index;   /* BattlerIndex (-1 = ATTACKER) */
    int64_t source_id;             /* Pokemon.id of the attacker */
    double damage;
} pr_attack_record;
#define PR_MAX_ATTACKS_RECEIVED 8  /* newest entries kept; older ones read as PR_FEAT_MOVE_HISTORY */

/* Battle identity subset of summonData.illusion. Presentation metadata (nickname,
 * sprite/ball/shiny variants) stays with the application, not these type reads. */
typedef struct pr_illusion {
    uint16_t species, fusion_species;
    uint8_t form, fusion_form;
    int8_t gender;
    bool active;
} pr_illusion;

typedef struct pr_pokemon {
    uint32_t id;               /* Pokemon.id: what held items point at */
    uint16_t species;
    uint8_t form;
    uint16_t fusion_species;   /* 0 = null (not fused) */
    uint8_t fusion_form;
    pr_illusion illusion;
    int8_t fusion_ability_index; /* 0, 1, 2 for ability1, ability2, ability_hidden; -1 = none */
    uint8_t level;
    uint8_t ivs[6];
    uint8_t nature;
    uint16_t ability;          /* active ability (getAbility()) */
    uint16_t base_ability;     /* getAbility(ignoreOverride=true): what calculateStats checks */
    bool ability_lookup_known; /* source index/override metadata supplied by the caller */
    int8_t ability_index;      /* Pokemon.abilityIndex; 0, 1, 2 (hidden) */
    int16_t custom_ability, fusion_custom_ability; /* -1 absent, NONE(0) is an override */
    int16_t lookup_ability_override; /* caller's active side override; -1 absent, zero ignored */
    int16_t daily_boss_ability; /* applicable daily event override, -1 absent */
    uint16_t temporary_fusion_species; /* summonData.fusionSpeciesForm, 0 absent */
    uint8_t temporary_fusion_form;
    int32_t ability_override;  /* summonData.ability (-1 = undefined); ability follows it while set */
    uint16_t passive;          /* hasPassive() ? getPassiveAbility() : 0 (NONE); pr_update_passive sets it */
    bool passive_flag;         /* Pokemon.passive (the passive is unlocked and enabled) */
    int16_t custom_passive;    /* customPokemonData.passive, -1 = none */
    int8_t added_type;         /* summonData.addedType (Forest's Curse, Trick-or-Treat); 0 = none (null) */
    bool is_player;
    int32_t stats[6];
    double summon_stats[6];    /* summonData.stats: 0 = no override (Gorilla Tactics' 1.5x can leave a fraction) */
    int32_t hp;
    int8_t stat_stages[7];     /* ATK..EVA, i.e. index stat-1 */
    uint8_t status;            /* StatusEffect (0 = no Status object) */
    /* Status.toxicTurnCount, sleepTurnsRemaining, freezeTurnsRemaining (-1 = undefined) */
    int16_t status_toxic_turns, status_sleep_turns, status_freeze_turns;
    bool terastallized;
    bool shiny;                        /* isShiny(true): shiny, or a shiny fusion */
    int8_t tera_type;
    int8_t summon_types[PR_MAX_SUMMON_TYPES];  /* summonData.types, in order */
    uint8_t n_summon_types;            /* its length; 0 = not set (getTypes uses the species') */
    uint16_t moves[4];                 /* moveset, 0 = empty slot */
    uint8_t pp_used[4];
    uint8_t pp_up[4];
    int16_t max_pp_override[4];        /* -1 = undefined */
    uint8_t n_moves;
    int8_t gender;                     /* Gender: -1 genderless, 0 male, 1 female */
    uint8_t friendship;
    bool has_trainer;                  /* hasTrainer(): enemy owned by a trainer */
    int8_t trainer_slot;               /* EnemyPokemon.trainerSlot (TrainerSlot): which trainer of a duo owns it */
    bool is_boss;                      /* isBoss(): enemy with boss segments */
    uint8_t boss_segments;              /* EnemyPokemon.bossSegments; 0 = not a boss */
    uint8_t boss_segment_index;         /* shields remaining: bossSegments - 1 initially */
    pr_battler_tag tags[PR_MAX_BATTLER_TAGS]; /* summonData.tags */
    uint8_t n_tags;
    uint32_t stellar_types_boosted;    /* stellarTypesBoosted, bit per PokemonType */
    bool on_field;                     /* in globalScene.field (isOnField() also needs !switch_out_status) */
    bool switch_out_status;
    bool ability_suppressed;           /* summonData.abilitySuppressed */
    /* summonData */
    uint8_t summon_hit_count;          /* summonData.hitCount (Rage Fist) */
    /* summonData.moveHistory: its length, and its newest min(length, PR_MAX_MOVE_HISTORY)
     * entries, oldest first (pr_push_move_history keeps both; pr_last_move reads them) */
    uint16_t n_move_history;
    pr_turn_move move_history[PR_MAX_MOVE_HISTORY];
    pr_turn_move move_queue[PR_MAX_MOVE_QUEUE];  /* summonData.moveQueue */
    uint8_t n_move_queue;
    uint16_t form_change_abilities[16];
    uint8_t n_form_change_abilities;
    /* turnData (PokemonTurnData) */
    /* turnData.attacksReceived: its length, and its newest entries, newest first as upstream
     * unshifts them (pr_unshift_attack_received keeps both) */
    uint16_t n_attacks_received;
    pr_attack_record attacks_received[PR_MAX_ATTACKS_RECEIVED];
    bool switched_in_this_turn, summoned_this_turn, failed_run_away;
    bool acted, stat_stages_increased, stat_stages_decreased, joined_round;
    int32_t damage_taken, total_damage_dealt, single_hit_damage_dealt;
    uint16_t combining_pledge;         /* MoveId, 0 = none */
    int8_t hit_count, hits_left;       /* defaults 0 and -1 */
    int8_t order;                      /* turnData.order, -1 = undefined */
    uint8_t pending_status;
    double turn_move_effectiveness; /* turnData.moveEffectiveness; < 0 = unset */
    /* tempSummonData */
    int32_t temp_turn_count, temp_wave_turn_count;
    /* summonData.abilitiesApplied / waveData.abilitiesApplied: the abilities that applied an
     * attribute outside a simulation (sets, in insertion order) */
    uint16_t ab_applied_summon[4], ab_applied_wave[4];
    uint8_t n_ab_applied_summon, n_ab_applied_wave;
    /* waveData */
    bool wave_endured;                 /* waveData.endured (EnemyEndureChanceModifier) */
    /* battleData */
    bool has_eaten_berry;              /* battleData.hasEatenBerry (Belch) */
    uint8_t battle_berries[PR_MAX_BERRY_HISTORY], turn_berries[PR_MAX_BERRY_HISTORY], last_berries[PR_MAX_BERRY_HISTORY];
    uint16_t n_battle_berries, n_turn_berries, n_last_berries;
    /* Task 53: Mega Evolution & Dynamax */
    bool queued_mega;
    bool queued_dynamax;
    bool is_dynamaxed;
    uint8_t dynamax_turns_left;
    uint8_t dynamax_level;
    uint8_t pre_dynamax_form;
    /* Transform's summonData identity and temporary moveset. Ordinary moves/PP
     * are Pokemon.moveset and remain permanent, as in the pinned source. */
    bool transformed;
    uint16_t transform_species;
    uint8_t transform_form;
    int8_t transform_gender;
    uint16_t transform_moves[4];
    uint8_t transform_pp_used[4], transform_pp_up[4], transform_n_moves;
    int16_t transform_max_pp_override[4];
    bool summon_moves_active; /* Mimic can override moves without transforming identity. */
    uint8_t summon_moves_alias_mask; /* slots sharing their PokemonMove with permanent moves */
    uint8_t variant;           /* permanent shiny variant; Transform does not copy it */
} pr_pokemon;

/* getMoveset(): temporary collection from Transform or Mimic; the fields above named
 * moves/PP always represent the permanent Pokemon.moveset. These selectors also
 * preserve constness and allow PP updates; pass an expression without side effects. */
#define PR_HAS_SUMMON_MOVES(p) ((p)->transformed || (p)->summon_moves_active)
#define PR_MOVES(p) (PR_HAS_SUMMON_MOVES(p) ? (p)->transform_moves : (p)->moves)
#define PR_PP_USED(p) (PR_HAS_SUMMON_MOVES(p) ? (p)->transform_pp_used : (p)->pp_used)
#define PR_PP_UP(p) (PR_HAS_SUMMON_MOVES(p) ? (p)->transform_pp_up : (p)->pp_up)
#define PR_MAX_PP(p) (PR_HAS_SUMMON_MOVES(p) ? (p)->transform_max_pp_override : (p)->max_pp_override)
#define PR_N_MOVES(p) (*((p)->transformed ? &(p)->transform_n_moves : &(p)->n_moves))

#define PR_MAX_PARTY 6
#define PR_MAX_QUEUED_MOVERS 8
#define PR_MAX_TURN_ORDER 8

/* A Pokemon has fainted (custom content, task C10). Reported from FaintPhase at doFaint, after a Reviver Seed could no longer save it and
 * before attacksReceived is cleared, so the caller can read, without guessing, who landed the hit that took the last HP.
 * `direct` is set only when the engine's own state names the attacker: the FaintPhase was queued by the hit itself (source != -1) AND the
 * fainted Pokemon's newest attacksReceived record names that same attacker with damage > 0. Every other loss of the last HP (poison,
 * weather, recoil, a tag, a faint with no attack record) is not direct, and nothing is inferred for it. The note and the Pokemon it
 * points at are valid only during the call, which must not change the battle. */
typedef struct pr_faint_note {
    int64_t id;                    /* the fainted Pokemon's id */
    int8_t battler;                /* its field position */
    uint8_t side;                  /* 0 player, 1 enemy */
    uint8_t remaining;             /* Pokemon of its side with HP left after this one (party members, on the field or not) */
    bool direct;                   /* see above */
    int8_t source_battler;         /* direct: the attacker's field position when it hit, else -1 */
    int64_t source_id;             /* direct: the attacker's id, else 0 */
    uint16_t move;                 /* direct: the move of the hit, else 0 */
    int wave, turn;
    const struct pr_pokemon *fainted;
    const struct pr_pokemon *source;   /* direct: the attacker where it stands now (pr_pokemon_by_id), NULL if it is gone from the parties */
} pr_faint_note;
struct pr_battle;
typedef void (*pr_faint_observer_fn)(const struct pr_battle *bt, const pr_faint_note *note, void *ctx);

typedef struct pr_battle {
    pr_rng rng;                /* Phaser.Math.RND (global) */
    bool rng_live;             /* false = the caller didn't provide its state */
    pr_rng battle_rng;         /* currentBattle.battleSeedState */
    bool battle_rng_live;
    pr_seedstr battle_seed;
    int turn;
    bool double_battle;
    uint8_t weather;           /* WeatherType */
    uint8_t terrain;           /* TerrainType */
    pr_arena_tag arena_tags[PR_MAX_ARENA_TAGS]; /* arena.tags */
    uint8_t n_arena_tags;
    pr_positional_tag pos_tags[PR_MAX_POSITIONAL_TAGS];   /* arena.positionalTagManager.tags */
    uint8_t n_pos_tags;
    /* globalScene.modifiers (player, [0]) and enemyModifiers ([1]), in upstream list order */
    pr_modifier mods[2][PR_MAX_MODIFIERS];
    uint8_t n_mods[2];
    pr_seedstr wave_seed;      /* globalScene.waveSeed; len 0 = unknown */
    int max_exp_level;         /* globalScene.getMaxExpLevel(); 0 = unknown */
    /* turnCommand.cursor left undefined (a recharge turn's FIGHT command has -1) */
#define PR_CURSOR_UNDEFINED INT8_MIN
    /* currentBattle.turnCommands / preTurnCommands per BattlerIndex */
    struct pr_turn_command {
        int8_t command;        /* Command; -1 = none (null) */
        uint16_t move;         /* move.move (MoveId.NONE for a recharge turn's placeholder) */
        bool no_move;          /* turnCommand.move is undefined (FIGHT commands always have one) */
        int8_t targets[4];     /* move.targets */
        uint8_t n_targets;
        uint8_t use_mode;
        bool skip;
        int8_t cursor;         /* POKEMON: the party slot to send in; PR_CURSOR_UNDEFINED = undefined */
        bool baton;            /* POKEMON: args[0], the switch passes the baton */
    } commands[4], pre_commands[4];
    /* Faint bookkeeping: arena.playerFaints, currentBattle.enemyFaints and the faint histories
     * (playerFaintsHistory / enemyFaintsHistory: which Pokemon, on which turn) */
    int player_faints, enemy_faints;
    struct { uint32_t id; int turn; } faint_history[2][12];
    uint8_t n_faint_history[2];        /* [0] player, [1] enemy */
    /* dynamicQueueManager.getLastTurnOrder(): the Pokemon.id of each Pokemon whose MovePhase
     * started this turn, in order (its length, and its first PR_MAX_TURN_ORDER entries) */
    uint8_t n_last_turn_order;
    uint32_t last_turn_order[PR_MAX_TURN_ORDER];
    /* Pokemon.id of each MovePhase still queued this turn (phaseManager.hasPhaseOfType("MovePhase")) */
    uint32_t queued_movers[PR_MAX_QUEUED_MOVERS];
    uint8_t n_queued_movers;
    /* The parties, [0] player and [1] enemy, in party order (getPlayerParty() / getEnemyParty()).
     * Every member is a full Pokemon; the ones on the field are the same objects as bt->field.
     * n = 0 when the caller didn't provide one. */
    pr_pokemon *party[2][PR_MAX_PARTY];
    uint8_t n_party[2];
    /* TargetHalfHpDamageAttr.initialHp (Super Fang, Ruination...). Upstream keeps it on the
     * attribute instance, so it is shared by every user of the move and outlives the move
     * use; NaN = never set (undefined). pr_half_hp_initial finds a move's slot. */
    struct { const pr_attr *attr; double hp; } half_hp_initial[4];
    uint8_t n_half_hp_initial;
    pr_pokemon *field[4];      /* player 0/1, enemy 2/3 as in upstream BattlerIndex */
    /* ---- state the turn engine keeps (the damage path doesn't read it) ---- */
    int wave_index;
    int8_t game_mode;          /* gameMode.modeId (GameModes), -1 = unknown */
    uint8_t battle_type;       /* BattleType */
    uint16_t last_move;        /* currentBattle.lastMove, 0 = none */
    int8_t last_player_involved, last_enemy_involved;  /* -1 = undefined */
    uint32_t participants[PR_MAX_PARTY];               /* playerParticipantIds, sorted */
    uint8_t n_participants;
    int post_battle_loot;      /* currentBattle.postBattleLoot.length */
    double battle_score;       /* currentBattle.battleScore */
    double money;             /* globalScene.money; independent of Pay Day */
    int32_t money_scattered;   /* currentBattle.moneyScattered (Pay Day) */
    const char *player_name;   /* loggedInUser.username for Celebrate's text; NULL: none (upstream shows "") */
    /* A trainer battle's trainer (currentBattle.trainer). trainer_names: getName(TRAINER) and
     * getName(TRAINER_PARTNER), for "{{trainerName}} sent out..."; NULL shows "" (upstream's
     * undefined). trainer_partner: isPartner(), a duo like Tate & Liza whose Pokemon each
     * switch within their own trainer's (trainer_slot) */
    const char *trainer_names[2];
    bool trainer_partner;
    bool battle_style_set;     /* settings.general.battleStyle === SET: no "Will you switch?" at a battle's start */
    /* trainer.getNextSummonIndex(trainerSlot): the party slot the trainer sends in next, when
     * its Pokemon fainted or used U-turn and co. The game sets it (ai-core ports it). Called
     * where upstream calls it, with the TrainerSlot upstream passes; upstream runs the tie
     * break under executeWithSeedOffset, so it must leave the RNG as it found it. NULL in a
     * trainer battle: that switch reports itself unported (PR_FEAT_TRAINER). */
    int (*next_summon_index)(struct pr_battle *bt, int trainer_slot, void *ctx);
    void *next_summon_ctx;
    int16_t biome;             /* arena.biomeId (BiomeId), read when has_biome */
    bool has_biome;
    int16_t weather_turns_left, weather_max_duration;
    int16_t terrain_turns_left, terrain_max_duration;
    bool ignore_abilities;     /* arena.ignoreAbilities */
    int8_t ignoring_effect_source; /* arena.ignoringEffectSource (BattlerIndex), -1 = undefined */
    bool ability_bar_visible;  /* globalScene.abilityBar.isVisible() */
    int8_t last_used_pokeball; /* currentBattle.lastUsedPokeball (PokeballType), -1 = null */
    int escape_attempts;       /* currentBattle.escapeAttempts */
    bool successful_run;       /* currentBattle.successfulRun */
    struct pr_engine *engine;  /* the phase engine (src/engine.c); NULL outside a turn */
    /* Coverage: attribute classes that would have run upstream but have no C handler
     * yet. A result is only claimed faithful when this stays empty. */
    uint16_t unported[PR_MAX_UNPORTED];
    uint8_t n_unported;
    /* Side effects (heal, stat stage change, battler tag) that the phase engine will
     * carry out; recorded, not yet executed. They don't change the damage result. */
    uint16_t deferred[PR_MAX_UNPORTED];
    uint8_t n_deferred;
    /* Optional (NULL = none, the default): told of every faint at doFaint. Never changes the battle, an RNG draw, an event or a phase. */
    pr_faint_observer_fn faint_observer;
    void *faint_observer_ctx;
} pr_battle;

/* ---- hooks (src/hooks.c) ---- */
/* Upstream applyAbAttrs("<Class>", ...) presence check: does this Pokemon's ability
 * or passive carry an attr that is-a <cls>? Records it as unported when it does. */
bool pr_hook_ab(pr_battle *bt, const pr_pokemon *p, uint16_t cls);
/* applyMoveAttrs("<Class>", ...) presence check for a move. */
bool pr_hook_move(pr_battle *bt, uint16_t move, uint16_t cls);
bool pr_move_has_attr(uint16_t move, uint16_t cls);
/* MultiHitAttr.getHitCount(BEAT_UP): user plus party members without a status. */
int pr_beat_up_hit_count(pr_battle *bt, const pr_pokemon *user);
void pr_note_unported(pr_battle *bt, uint16_t cls);
/* Record a side effect of class `cls` for the phase engine (heal, stat change, tag) */
void pr_note_deferred(pr_battle *bt, uint16_t cls);

/* ---- upstream closures (src/closures.c) ----
 * Each hand-ported closure is registered under the upstream owner + source hash it
 * was written against. If upstream changes the source, the hash stops matching and
 * the closure reads as unported instead of running stale logic. */
/* Arguments follow the upstream closure's own order: (user, target, move) for attack
 * conditions, (target, user, move) for defend conditions, (pokemon) for ability
 * conditions. `self` carries the probed captured values (self->cap). */
typedef double (*pr_closure_fn)(pr_battle *bt, const pr_closure_info *self, const pr_pokemon *a,
                                const pr_pokemon *b, int move);
/* owner NULL: any owner (a closure every instance of an attribute class makes) */
typedef struct { const char *owner, *hash; pr_closure_fn fn; } pr_closure_reg;
void pr_closures_init(void);
/* Calls closure `id`; when it has no (current) C port, records `cls` as unported
 * and returns `fallback`. */
/* Whether closure id is the upstream function with that owner and source hash: for the few
 * closures with side effects, which their call sites port in place */
bool pr_closure_matches(uint16_t id, const char *owner, const char *hash);
double pr_closure_call(pr_battle *bt, uint16_t id, uint16_t cls, double fallback,
                       const pr_pokemon *a, const pr_pokemon *b, int move);
/* Calls that found no C port / that ran one, per closure id (for the tests' reports) */
extern uint32_t pr_closure_misses[], pr_closure_hits[];
static inline bool pr_cap_has(const pr_closure_info *self, int v) { return (self->cap >> (v + 1)) & 1u; }
int pr_closures_ported(void);

/* ---- ability attributes (src/ab_attrs.c) ----
 * One params struct for every applyAbAttrs call site; each site fills what its
 * attribute family reads. Pointers are upstream's NumberHolder/BooleanHolder. */
typedef struct pr_ab_params {
    const pr_pokemon *pokemon;     /* the ability's owner */
    const pr_pokemon *opponent;    /* attacker/defender, or FieldMultiplyStat's target */
    const pr_pokemon *victim;      /* PostKnockOutAbAttr: the Pokemon whose FaintPhase is running */
    int move;                      /* -1 = none */
    bool simulated;
    bool ignore_ability;           /* AllyStatMultiplierAbAttr.ignoreAbility */
    int stat;
    double *value;                 /* statVal / power / damage / typeMultiplier / critMult / weight / multiplier */
    bool *flag;                    /* cancelled / ignored / hasApplied / blockCrit / isCritical / isUngrounded */
    int *holder;                   /* moveType / weather / critStage */
    int move_type, defender_type;  /* IgnoreTypeImmunityAbAttr */
    /* StatStageChangePhase: the changes (Stat values and stages), the stats cancelled so far
     * (bit per Stat), whether the change came from the Pokemon's own move, and its target */
    const int8_t *ch_stat, *ch_stages;
    int n_ch;
    uint16_t *cancelled_stats;
    bool self_target;
    const pr_pokemon *target;
    int effect;                    /* a StatusEffect (status immunities, Early Bird, Synchronize...) */
    int hit_result;                /* PostMoveInteractionAbAttrParams (on-hit abilities): hitResult, damage */
    double damage;
    int tag_type;                  /* PreApplyBattlerTagAbAttrParams: the tag's BattlerTagType */
    int field_type;                /* PostWeatherChange: the new WeatherType; PostTerrainChange: the new TerrainType */
    int8_t only;                   /* params.passive given: 1 = the ability only, 2 = the passive only; 0 = both */
    bool after_form_change;         /* exclude recursive form callbacks */
    const int8_t *move_targets; /* PostDancingMoveAbAttr: original targets */
    int n_move_targets;
    bool on_gain;                  /* applyOnGainAbAttrs: only attrs that shouldActivateOnGain() */
} pr_ab_params;
/* applyAbAttrs("<cls>", params): ability then passive, conditions, canApply, apply. */
void pr_apply_ab_attrs(pr_battle *bt, uint16_t cls, pr_ab_params *pp);
/* PostSummonHealStatusAbAttr.statusHealed: the last status the attr cured, which its next trigger
 * message reads (before it cures again). Upstream keeps it on the attr object, so it outlives
 * battles and is shared by every Pokemon with the ability: a session-wide table here too, by
 * ability (each has at most one such attr). 0 = never cured. */
int pr_status_healed_get(uint16_t ability);
void pr_status_healed_set(uint16_t ability, int effect);
void pr_status_healed_clear(void);
int pr_status_healed_list(uint16_t *ability, uint8_t *effect, int max);   /* by ability id */
/* ability.hasAttr / pokemon.getAbility().getAttrs(): presence only, no applicability checks */
const pr_attr *pr_ability_first_attr(uint16_t ability, uint16_t cls);
/* Pokemon.canApplyAbility for `ability` (the ability or the passive) */
bool pr_can_apply_ability(pr_battle *bt, const pr_pokemon *p, uint16_t ability);
/* hasAbility(ability) / hasAbilityWithAttr(cls) with canApply = true */
bool pr_has_ability_active(pr_battle *bt, const pr_pokemon *p, int ability);
bool pr_has_ability_with_attr(pr_battle *bt, const pr_pokemon *p, uint16_t cls);
/* ---- tags (src/tags.c) ---- */
/* Tag classes the battle logic asks for with getTag(<Class>) / instanceof. Which class a tag
 * is follows from its type (getBattlerTag / getArenaTag); pr_*_tag_is is that mapping. */
enum { PR_TAGCLASS_PROTECTED, PR_TAGCLASS_SEMI_INVULNERABLE, PR_TAGCLASS_TYPE_BOOST, PR_TAGCLASS_HELPING_HAND,
       PR_TAGCLASS_SUPREME_OVERLORD, PR_TAGCLASS_CRIT_BOOST, PR_TAGCLASS_TAR_SHOT, PR_TAGCLASS_HIGHEST_STAT_BOOST,
       PR_TAGCLASS_EXPOSED, PR_TAGCLASS_STOCKPILING, PR_TAGCLASS_AUTOTOMIZED, PR_TAGCLASS_SUBSTITUTE,
       PR_TAGCLASS_TRAPPED,
       PR_TAGCLASS_COUNT_ };
extern const char *const pr_tagclass_name[PR_TAGCLASS_COUNT_];    /* upstream class names */
bool pr_battler_tag_is(int tag_type, int tag_class);
enum { PR_ARENACLASS_WEAKEN_MOVE_SCREEN, PR_ARENACLASS_WEAKEN_MOVE_TYPE, PR_ARENACLASS_NO_CRIT,
       PR_ARENACLASS_SUPPRESS_ABILITIES, PR_ARENACLASS_CONDITIONAL_PROTECT, PR_ARENACLASS_ENTRY_HAZARD,
       PR_ARENACLASS_COUNT_ };
extern const char *const pr_arenaclass_name[PR_ARENACLASS_COUNT_];
bool pr_arena_tag_is(int tag_type, int arena_class);
/* pokemon.getTag(type) / getTag(Class): the first matching tag, or NULL */
const pr_battler_tag *pr_get_battler_tag(const pr_pokemon *p, int tag_type);
const pr_battler_tag *pr_find_battler_tag_class(const pr_pokemon *p, int tag_class);
bool pr_has_battler_tag(pr_battle *bt, const pr_pokemon *p, int tag_type);
bool pr_has_battler_tag_class(pr_battle *bt, const pr_pokemon *p, int tag_class);
/* arena.getTagOnSide(type, side): a BOTH-sided tag matches any side, and side BOTH matches any
 * tag (arena.getTag(type)) */
const pr_arena_tag *pr_arena_get_tag_on_side(const pr_battle *bt, int tag_type, int side);
bool pr_arena_has_tag(pr_battle *bt, int tag_type, int side);
bool pr_arena_has_tag_class(pr_battle *bt, int arena_class, int side); /* getTagOnSide(Class, side) */
/* arena.applyTagsForSide(<Class>, side, ...): does this tag take part for `side`? */
bool pr_arena_tag_on_side(const pr_arena_tag *t, int side);
/* The parameters getBattlerTag() / getArenaTag() build these classes with */
bool pr_type_boost_tag_params(int tag_type, int *boosted_type, double *boost_value);
bool pr_exposed_tag_ignores_immunity(int tag_type, int defender_type, int move_type);
bool pr_screen_tag_weakens(int tag_type, int category);
int pr_weaken_move_type_tag_type(int tag_type);
/* waveData.abilitiesApplied.has(ability): wave data is not modelled yet (empty, as at wave start) */
bool pr_wave_ability_applied(const pr_pokemon *p, int ability);
/* Move.doesFlagEffectApply({flag, user, target}) */
bool pr_move_does_flag_effect_apply(pr_battle *bt, int move, unsigned flag, const pr_pokemon *user,
                                    const pr_pokemon *target);
bool pr_move_does_flag_effect_apply_ex(pr_battle *bt, int move, unsigned flag, const pr_pokemon *user,
                                       const pr_pokemon *target, bool is_follow_up);
/* Move.hitsSubstitute(user, target): target may be NULL */
bool pr_move_hits_substitute(pr_battle *bt, int move, const pr_pokemon *user, const pr_pokemon *target);
bool pr_is_opponent(const pr_battle *bt, const pr_pokemon *a, const pr_pokemon *b);
const pr_pokemon *pr_get_ally(const pr_battle *bt, const pr_pokemon *p);
bool pr_weather_effect_suppressed(pr_battle *bt);
/* Weather.isImmutable(): the primal weathers (Heavy Rain, Harsh Sun, Strong Winds) */
bool pr_weather_immutable(int weather);
int pr_move_get_priority(pr_battle *bt, int move, const pr_pokemon *user);
bool pr_move_is_attack(int move);

/* ---- modifiers (src/modifiers.c) ----
 * Each function is one upstream globalScene.applyModifiers / applyModifier /
 * findModifier / getHeldItems call, named after the modifier class it applies. */
bool pr_mod_is(const pr_modifier *m, int cls);
bool pr_mod_is_ported_item(const pr_modifier *m);
int32_t pr_mod_get_stack_count(const pr_modifier *m);        /* stackCount + virtualStackCount */
/* pokemon.getHeldItems() (pokemonId === id exactly), filtered to isTransferable when asked:
 * the number of items, and the sum of their stackCount fields (not getStackCount()) */
int pr_held_item_count(pr_battle *bt, const pr_pokemon *p, bool transferable_only);
int32_t pr_held_item_stack_sum(pr_battle *bt, const pr_pokemon *p, bool transferable_only);
/* getHeldItems().find(i => i instanceof PokemonMultiHitModifier)?.getStackCount() ?? 0 */
int32_t pr_held_multi_lens_stacks(pr_battle *bt, const pr_pokemon *p);
/* applyModifier(FieldEffectModifier, user.isPlayer(), user, duration): +2 turns per stack */
void pr_apply_field_effect_modifier(pr_battle *bt, const pr_pokemon *user, int32_t *duration);
/* calculateBaseStats(): Shuckle Juice, Old Gateau, (fusion), vitamins */
void pr_apply_base_stat_modifiers(pr_battle *bt, const pr_pokemon *p, double base[6]);
void pr_apply_incrementing_stat_modifier(pr_battle *bt, const pr_pokemon *p, int stat, double *value);
void pr_apply_nature_weight_modifier(pr_battle *bt, const pr_pokemon *p, double *mult);
/* Raw projection APIs preserve pinned apply arithmetic outside acquisition caps.
 * Full-turn unsupported inventory admission is handled by the phase engine. */
void pr_project_nature_weight_modifier(pr_battle *bt, const pr_pokemon *p, double *mult);
void pr_apply_stat_booster_modifiers(pr_battle *bt, const pr_pokemon *p, int stat, double *value);
void pr_apply_temp_stat_stage_booster_modifiers(pr_battle *bt, bool player, int stat, double *mult);
void pr_apply_crit_booster_modifiers(pr_battle *bt, const pr_pokemon *src, int *stage);
void pr_project_crit_booster_modifiers(pr_battle *bt, const pr_pokemon *src, int *stage);
void pr_apply_temp_crit_booster_modifiers(pr_battle *bt, bool player, int *stage);
void pr_apply_attack_type_booster_modifiers(pr_battle *bt, const pr_pokemon *src, int move_type, double *power);
void pr_project_attack_type_booster_modifiers(pr_battle *bt, const pr_pokemon *src, int move_type, double *power);
void pr_apply_accuracy_booster_modifiers(pr_battle *bt, const pr_pokemon *p, double *accuracy);
void pr_apply_turn_status_modifiers(pr_battle *bt, struct pr_engine *e, pr_pokemon *p);
void pr_apply_turn_heal_modifiers(pr_battle *bt, struct pr_engine *e, pr_pokemon *p);
void pr_apply_hit_heal_modifiers(pr_battle *bt, struct pr_engine *e, pr_pokemon *p);
double pr_calculate_shell_bell_recovery(pr_battle *bt, const pr_pokemon *p);
void pr_apply_survive_damage_modifiers(pr_battle *bt, struct pr_engine *e, pr_pokemon *p, bool *survive);
void pr_apply_bypass_speed_chance_modifiers(pr_battle *bt, struct pr_engine *e, pr_pokemon *p);
void pr_apply_flinch_chance_modifiers(pr_battle *bt, struct pr_engine *e, pr_pokemon *user, pr_pokemon *target, bool deals_damage, bool *flinched);
bool pr_apply_reset_negative_stat_stage_modifier(pr_battle *bt, struct pr_engine *e, pr_pokemon *p);
void pr_lose_held_item(pr_battle *bt, struct pr_engine *e, pr_pokemon *p, pr_modifier *m);
void pr_clear_enemy_held_item_modifiers(pr_battle *bt, const pr_pokemon *p);
extern int pr_kings_rock_fired;
extern int pr_sitrus_berry_fired, pr_lum_berry_fired, pr_leppa_berry_fired;
extern int pr_liechi_berry_fired, pr_ganlon_berry_fired, pr_petaya_berry_fired, pr_apicot_berry_fired, pr_salac_berry_fired;
extern int pr_enigma_berry_fired, pr_lansat_berry_fired, pr_starf_berry_fired;
bool pr_berry_should_apply(pr_battle *bt, const pr_pokemon *p, const pr_modifier *m);
void pr_refresh_berry_modifiers(pr_battle *bt, int side);
void pr_reset_wave_data(pr_pokemon *p);
void pr_reset_battle_data(pr_pokemon *p);
void pr_record_eaten_berry(pr_battle *bt, pr_pokemon *p, int berry, bool consumed);
void pr_berry_effect(pr_battle *bt, struct pr_engine *e, pr_pokemon *p, int berry, int8_t *stats, int8_t *stages, int *n);
void pr_apply_berry(pr_battle *bt, struct pr_engine *e, pr_pokemon *p, pr_modifier *m);
void pr_apply_berry_ex(pr_battle *bt, struct pr_engine *e, pr_pokemon *p, pr_modifier *m, int8_t *ssc_stats, int8_t *ssc_stages, int *n_ssc);
/* DamageMoneyRewardModifier.apply: called after player-to-enemy final damage. */
void pr_apply_damage_money_modifiers(pr_battle *bt, const pr_pokemon *p, double damage);
/* PokemonMultiHitModifier.apply(pokemon, moveId, hitCount) for each Multi Lens */
void pr_apply_multi_hit_count_modifiers(pr_battle *bt, const pr_pokemon *p, int move, int *count);
/* PokemonMultiHitModifier.apply(pokemon, moveId, null, damageMultiplier) for each Multi Lens */
void pr_apply_multi_hit_damage_modifiers(pr_battle *bt, const pr_pokemon *src, int move, double *mult);
/* globalScene.findModifier(m => m instanceof PokemonMultiHitModifier && m.pokemonId === p.id) (player list) */
bool pr_find_player_multi_lens(pr_battle *bt, const pr_pokemon *p);
void pr_apply_enemy_damage_booster_modifiers(pr_battle *bt, double *damage);
void pr_apply_enemy_damage_reducer_modifiers(pr_battle *bt, double *damage);
void pr_apply_enemy_turn_heal_modifiers(pr_battle *bt, struct pr_engine *e, pr_pokemon *p);
void pr_apply_enemy_status_heal_modifiers(pr_battle *bt, struct pr_engine *e, pr_pokemon *p);
void pr_apply_enemy_attack_status_modifiers(pr_battle *bt, struct pr_engine *e, pr_pokemon *user, pr_pokemon *target);
void pr_apply_enemy_endure_modifiers(pr_battle *bt, struct pr_engine *e, pr_pokemon *target);
/* Move.canBeMultiStrikeEnhanced(user) (restrictSpread false, no target) */
bool pr_move_can_be_multi_strike_enhanced(int move);
/* Move.canBeMultiStrikeEnhanced(user, restrictSpread, target) */
bool pr_move_can_be_multi_strike_enhanced_ex(pr_battle *bt, int move, const pr_pokemon *user, bool restrict_spread,
                                             const pr_pokemon *target);
bool pr_is_new_biome(pr_battle *bt);
/* Math.pow as the reference JS engine computes it (src/js_math.c) */
double js_pow(double x, double y);

/* ---- move attributes (src/move_attrs.c) ---- */
/* randSeedInt(range) inside executeWithSeedOffset(fn, turn << 6, waveSeed) (Magnitude, Fickle Beam);
 * -1 (reported unported) when the wave seed isn't known */
int pr_wave_seed_roll(pr_battle *bt, int range);
/* applyMoveAttrs("VariablePowerAttr", user, target, move, power) */
void pr_apply_variable_power(pr_battle *bt, const pr_pokemon *user, const pr_pokemon *target, int move,
                             double *power);

void pr_apply_variable_move_type(pr_battle *bt, const pr_pokemon *user, int move, int *type);
void pr_apply_type_chart_override(pr_battle *bt, const pr_pokemon *user, const pr_pokemon *target, int move,
                                  double *mult, const int8_t *types, int ntypes, int move_type);
int pr_apply_variable_move_category(pr_battle *bt, const pr_pokemon *user, const pr_pokemon *target, int move,
                                    int category);
void pr_apply_variable_atk(pr_battle *bt, const pr_pokemon *user, const pr_pokemon *target, int move, double *atk);
void pr_apply_variable_def(pr_battle *bt, const pr_pokemon *user, const pr_pokemon *target, int move, double *def);
double pr_apply_fixed_damage(pr_battle *bt, const pr_pokemon *user, const pr_pokemon *target, int move);
/* The initialHp slot of `move`'s TargetHalfHpDamageAttr (NULL if the move has none) */
double *pr_half_hp_initial(pr_battle *bt, int move);
bool pr_apply_one_hit_ko(pr_battle *bt, const pr_pokemon *user, const pr_pokemon *target, int move);
double pr_hits_tag_multiplier(pr_battle *bt, const pr_pokemon *target, int move);
void pr_apply_modified_damage(pr_battle *bt, const pr_pokemon *user, const pr_pokemon *target, int move, double *dmg);
void pr_apply_pledge_stab(const pr_pokemon *user, int move, double *stab);

/* AttemptRunPhase.calculateEscapeChance: does not consume RNG or increment attempts. */
double pr_calculate_escape_chance(pr_battle *bt, int current_attempts);

/* ---- rng (battle.ts randSeedInt) ---- */
int32_t pr_battle_rand_seed_int(pr_battle *bt, int32_t range, int32_t min);

/* ---- turn history (src/history.c) ---- */
/* pushMoveHistory(turnMove) (the caller checks isOnField, as upstream does) */
void pr_push_move_history(pr_pokemon *p, pr_turn_move tm);
/* getLastXMoves(k + 1)[k]: the k-th newest history entry (0 = newest), NULL when the history is
 * shorter. An entry older than the kept ones reports PR_FEAT_MOVE_HISTORY and gives NULL. */
const pr_turn_move *pr_last_move(pr_battle *bt, const pr_pokemon *p, int k);
/* getLastNonVirtualMove(ignoreStruggle, ignoreFollowUp); *unknown when older entries were needed */
const pr_turn_move *pr_last_non_virtual_move(pr_battle *bt, const pr_pokemon *p, bool ignore_struggle,
                                             bool ignore_follow_up, bool *unknown);
/* turnData.attacksReceived.unshift(record) */
void pr_unshift_attack_received(pr_pokemon *p, const pr_attack_record *r);
/* The kept attacksReceived entries, newest first: their count. A reader that scans them all
 * without an answer calls pr_attacks_received_exhausted, which reports the older entries. */
int pr_attacks_received_kept(const pr_pokemon *p);
void pr_attacks_received_exhausted(pr_battle *bt, const pr_pokemon *p);
/* The Pokemon on the field with this Pokemon.id, or NULL */
pr_pokemon *pr_field_pokemon_by_id(const pr_battle *bt, int64_t id);
pr_pokemon *pr_pokemon_by_id(const pr_battle *bt, int64_t id);   /* getPokemonById: the parties */
/* areAllies(a, b) over BattlerIndex values (ATTACKER = -1 is nobody's ally) */
bool pr_are_allies(int a, int b);

/* ---- pokemon.ts ---- */
const pr_species *pr_pokemon_species(const pr_pokemon *p);
const pr_form *pr_pokemon_form(const pr_pokemon *p);
const pr_form *pr_pokemon_base_form(const pr_pokemon *p);
int pr_get_gender(const pr_pokemon *p);
double pr_get_sprite_scale(const pr_pokemon *p);
bool pr_can_transform_into(const pr_pokemon *user, const pr_pokemon *target);
void pr_restore_transform(pr_pokemon *p);
void pr_set_original_move(pr_pokemon *p, int slot, uint16_t move);
/* Synchronize the shared PokemonMove slots after modifying effective PP. */
void pr_sync_moveset_aliases(pr_pokemon *p);
void pr_use_moveset_pp(pr_pokemon *p, int slot, bool temporary, int count);
const pr_species *pr_pokemon_fusion_species(const pr_pokemon *p);
bool pr_is_mega(const pr_pokemon *p);
uint16_t pr_get_ability(const pr_pokemon *p, bool ignore_override);
void pr_refresh_ability(pr_pokemon *p);
void pr_change_form_state(pr_battle *bt, pr_pokemon *p, int form);
bool pr_is_max(const pr_pokemon *p);
void pr_mega_evolve(pr_battle *bt, pr_pokemon *p);
void pr_dynamax(pr_battle *bt, pr_pokemon *p);
const pr_form *pr_pokemon_fusion_form(const pr_pokemon *p);
/* getBattlerIndex(): the Pokemon's slot in bt->field, -1 when off the field */
int pr_battler_index(const pr_battle *bt, const pr_pokemon *p);
void pr_calculate_stats(pr_battle *bt, pr_pokemon *p);
/* calculateStats() on a Pokemon that has HP: kept, capped at the new maximum, or raised by as much
 * as the maximum grew (pr_calculate_stats sets it to the maximum, as for a new Pokemon) */
void pr_recalculate_stats(pr_battle *bt, pr_pokemon *p);
int pr_get_tera_type(const pr_pokemon *p);
/* p->passive from hasPassive() and getPassiveAbility() */
/* Enabled passives include Daily custom ability NONE, whose activation phase still queues. */
bool pr_has_passive(pr_battle *bt, const pr_pokemon *p);
void pr_update_passive(pr_battle *bt, pr_pokemon *p);
/* damageAndUpdate(damage, {result: INDIRECT, ignoreSegments: true, ignoreFaintPhase: true}) with no
 * source: the HP loss happens at once, as upstream does it in the middle of a damage calculation */
double pr_damage_indirect(pr_battle *bt, pr_pokemon *p, double damage);
double pr_get_hp_ratio(const pr_pokemon *p, bool precise);
double pr_get_weight(pr_battle *bt, const pr_pokemon *p);
int pr_get_effective_weather_for_move(pr_battle *bt, const pr_pokemon *user);
double js_round(double x);
/* getTypes(): flags mirror upstream's options object. */
enum { PR_TYPES_NO_TERA = 1, PR_TYPES_ORIGINAL_IF_STELLAR = 2, PR_TYPES_BYPASS_SUMMON = 4, PR_TYPES_IGNORE_THIRD = 8, PR_TYPES_USE_ILLUSION = 16 };
/* at most PR_MAX_TYPES entries: the summon types, then the added type */
#define PR_MAX_TYPES (PR_MAX_SUMMON_TYPES + 1)
int pr_get_types(const pr_pokemon *p, int8_t out[PR_MAX_TYPES], unsigned flags);
/* Dormant/manual helper support only: pinned upstream does not register Illusion's
 * ability attrs. Never call these automatically on summon or damage. */
void pr_set_illusion(pr_pokemon *p, const pr_pokemon *source);
void pr_set_illusion_species(pr_pokemon *p, uint16_t species, uint8_t form);
bool pr_break_illusion(pr_pokemon *p);
const pr_form *pr_apparent_form(const pr_pokemon *p, bool use_illusion);
const pr_form *pr_apparent_fusion_form(const pr_pokemon *p, bool use_illusion);
int pr_apparent_gender(const pr_pokemon *p, bool use_illusion);
/* summonData.types = types (n of them); false, reported, past PR_MAX_SUMMON_TYPES */
bool pr_set_summon_types(pr_battle *bt, pr_pokemon *p, const int8_t *types, int n);
bool pr_is_of_type(const pr_pokemon *p, int type);
double pr_get_stat(const pr_pokemon *p, int stat, bool bypass_summon);
double pr_get_stat_stage_multiplier(pr_battle *bt, const pr_pokemon *p, int stat, const pr_pokemon *opponent,
                                    int move, bool ignore_opp_ability, bool is_critical, bool simulated,
                                    bool ignore_held_items);
/* GetEffectiveStatParams. Upstream defaults: everything false, simulated true, move none (-1). */
typedef struct {
    const pr_pokemon *opponent;
    int move;
    bool ignore_ability, ignore_opp_ability, ignore_ally_ability, is_critical, ignore_held_items, for_defend;
    bool simulated;
} pr_stat_params;
int32_t pr_get_effective_stat_p(pr_battle *bt, const pr_pokemon *p, int stat, const pr_stat_params *sp);
/* Shorthand for the common case (no ignore flags, simulated). */
int32_t pr_get_effective_stat(pr_battle *bt, const pr_pokemon *p, int stat,
                              const pr_pokemon *opponent, int move, bool is_critical, bool for_defend);
/* isGrounded(ignoreSemiInvulnerable) and its two halves */
bool pr_is_forcibly_grounded(pr_battle *bt, const pr_pokemon *p);
bool pr_is_forcibly_ungrounded(pr_battle *bt, const pr_pokemon *p);
bool pr_is_grounded(pr_battle *bt, const pr_pokemon *p, bool ignore_semi_invulnerable);
int pr_get_move_type(pr_battle *bt, const pr_pokemon *p, int move);
/* HiddenPowerTypeAttr: Hidden Power's type from the user's IVs */
int pr_hidden_power_type(const pr_pokemon *u);
double pr_get_attack_type_effectiveness(pr_battle *bt, const pr_pokemon *def, int move_type,
                                        const pr_pokemon *source, int move);
/* the same with simulated = false passed through (IgnoreTypeImmunityAbAttr, the Strong Winds message) */
double pr_get_attack_type_effectiveness_ex(pr_battle *bt, const pr_pokemon *def, int move_type,
                                           const pr_pokemon *source, int move, bool simulated);
/* ... and ignoreStrongWinds (Stealth Rock) */
double pr_get_attack_type_effectiveness_opts(pr_battle *bt, const pr_pokemon *def, int move_type,
                                             const pr_pokemon *source, int move, bool simulated,
                                             bool ignore_strong_winds);
double pr_get_move_effectiveness(pr_battle *bt, const pr_pokemon *def, const pr_pokemon *src, int move);
/* getMoveEffectiveness with simulated and the cancelled holder */
double pr_get_move_effectiveness_ex(pr_battle *bt, const pr_pokemon *def, const pr_pokemon *src, int move,
                                    bool simulated, bool *cancelled_out);
double pr_calculate_battle_power(pr_battle *bt, const pr_pokemon *src, const pr_pokemon *def, int move,
                                 bool simulated);
/* GetBaseDamageParams (defaults: ignore flags false, not critical, simulated) */
typedef struct {
    int move, category;
    bool ignore_ability, ignore_source_ability, ignore_ally_ability, ignore_source_ally_ability, is_critical;
    bool simulated;
} pr_base_damage_params;
double pr_get_base_damage(pr_battle *bt, const pr_pokemon *def, const pr_pokemon *src, const pr_base_damage_params *bp);
double pr_calculate_stab_multiplier(pr_battle *bt, const pr_pokemon *src, int move, bool ignore_source_ability,
                                    bool simulated);

/* damage is a double like upstream's: it isn't capped by the target's HP here, and with
 * the enemy's damage tokens (up to 999 stacks of x1.05 in endless) it can pass 2^63. */
typedef struct { bool cancelled; int result /* HitResult */; double damage; } pr_damage_result;
/* simulated=true: random factor is 1 and no RNG is consumed, as upstream. */
pr_damage_result pr_get_attack_damage(pr_battle *bt, pr_pokemon *def, pr_pokemon *src, int move,
                                      bool is_critical, bool simulated);
/* the same with getAttackDamage's `effectiveness` option (NAN = undefined: computed as above) */
pr_damage_result pr_get_attack_damage_ex(pr_battle *bt, pr_pokemon *def, pr_pokemon *src, int move,
                                         bool is_critical, bool simulated, double effectiveness);
/* src/utils/damage.ts calculateBossSegmentDamage. */
double pr_calculate_boss_segment_damage(double damage, double current_hp, double segment_hp,
                                        int min_segment_index, int current_segment_index,
                                        int *cleared_index);
bool pr_get_critical_hit_result(pr_battle *bt, const pr_pokemon *def, const pr_pokemon *src, int move);

#endif

