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

/* arena.terrainType (src/data/terrain.ts TerrainType; not part of the dumped enums) */
enum { TERRAIN_NONE, TERRAIN_MISTY, TERRAIN_ELECTRIC, TERRAIN_GRASSY, TERRAIN_PSYCHIC };

/* State the port doesn't model yet, reported in the unported list next to attribute
 * classes (ids continue after the last class id). */
enum {
    PR_FEAT_DOUBLES = PR_AC_COUNT_,  /* more than one Pokemon per side */
    PR_FEAT_MODIFIERS,               /* a modifier whose own class has no port (see pr_modifier) */
    PR_FEAT_BATTLER_TAGS,
    PR_FEAT_ARENA_TAGS,
    PR_FEAT_MOVE_HISTORY,            /* summonData.moveHistory / turnData.attacksReceived entries */
    PR_FEAT_TURN_ORDER,              /* the turn queue (last turn order, move queue) */
    PR_FEAT_FAINT_HISTORY,
    PR_FEAT_WAVE,                    /* wave data the caller didn't provide (wave seed, level cap) */
    PR_FEAT_PARTY,
    PR_FEAT_HIT_STATE,               /* state kept from an earlier hit of the same move use */
    PR_FEAT_COUNT_
};
const char *pr_unported_name(uint16_t id);

/* Modifiers (src/modifiers.c): upstream's PersistentModifier instances, held items
 * included. Classes are the upstream ones the battle logic queries; the parent table
 * gives instanceof. */
enum {
    PR_MOD_NONE,
    PR_MOD_PERSISTENT,               /* PersistentModifier (any class without a port, e.g. Exp. Charm) */
    PR_MOD_HELD_ITEM,                /* PokemonHeldItemModifier (Leftovers, berries... whose effects aren't on the ported paths) */
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
    PR_MOD_ENEMY_PERSISTENT,
    PR_MOD_ENEMY_DAMAGE_MULTIPLIER,
    PR_MOD_ENEMY_DAMAGE_BOOSTER,     /* the enemy's Damage token */
    PR_MOD_ENEMY_DAMAGE_REDUCER,     /* the enemy's Protection token */
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
} pr_modifier;
#define PR_MAX_MODIFIERS 96        /* per side */

typedef struct pr_pokemon {
    uint32_t id;               /* Pokemon.id: what held items point at */
    uint16_t species;
    uint8_t form;
    uint8_t level;
    uint8_t ivs[6];
    uint8_t nature;
    uint16_t ability;          /* active ability (getAbility()) */
    uint16_t base_ability;     /* getAbility(ignoreOverride=true): what calculateStats checks */
    uint16_t passive;          /* 0 (NONE) when no passive is active */
    bool is_player;
    int32_t stats[6];
    int32_t summon_stats[6];   /* summonData.stats: 0 = no override */
    int32_t hp;
    int8_t stat_stages[7];     /* ATK..EVA, i.e. index stat-1 */
    uint8_t status;            /* StatusEffect */
    bool terastallized;
    int8_t tera_type;
    int8_t type_override[2];   /* -2 = none (summonData.types not set) */
    uint16_t moves[4];                 /* moveset, 0 = empty slot */
    uint8_t pp_used[4];
    int8_t gender;                     /* Gender: -1 genderless, 0 male, 1 female */
    uint8_t friendship;
    bool has_trainer;                  /* hasTrainer(): enemy owned by a trainer */
    bool is_boss;                      /* isBoss(): enemy with boss segments */
    uint8_t n_tags;                    /* summonData.tags.length: tags aren't modelled, see pr_has_battler_tag */
    uint32_t stellar_types_boosted;    /* stellarTypesBoosted, bit per PokemonType */
    /* summonData fields read on the damage path */
    uint8_t summon_hit_count;          /* summonData.hitCount (Rage Fist) */
    uint8_t n_move_history;            /* summonData.moveHistory.length (entries not modelled) */
    /* turnData (PokemonTurnData) fields read on the damage path */
    uint8_t n_attacks_received;        /* turnData.attacksReceived.length (entries not modelled) */
    bool switched_in_this_turn;
    bool acted, stat_stages_decreased, joined_round;
    int32_t damage_taken;
    uint16_t combining_pledge;         /* MoveId, 0 = none */
    int8_t hit_count, hits_left;       /* defaults 0 and -1 */
    double turn_move_effectiveness; /* turnData.moveEffectiveness; < 0 = unset */
} pr_pokemon;

typedef struct pr_battle {
    pr_rng rng;                /* Phaser.Math.RND (global) */
    pr_rng battle_rng;         /* currentBattle.battleSeedState */
    bool battle_rng_live;
    pr_seedstr battle_seed;
    int turn;
    bool double_battle;
    uint8_t weather;           /* WeatherType */
    uint8_t terrain;           /* TerrainType */
    int n_arena_tags;          /* arena.tags.length: tags aren't modelled, see pr_arena_has_tag */
    /* globalScene.modifiers (player, [0]) and enemyModifiers ([1]), in upstream list order */
    pr_modifier mods[2][PR_MAX_MODIFIERS];
    uint8_t n_mods[2];
    pr_seedstr wave_seed;      /* globalScene.waveSeed; len 0 = unknown */
    int max_exp_level;         /* globalScene.getMaxExpLevel(); 0 = unknown */
    int8_t turn_command[4];    /* currentBattle.turnCommands[i]?.command per BattlerIndex; -1 = none */
    /* Faint bookkeeping: arena.playerFaints, currentBattle.enemyFaints, and the turn of the
     * newest entry of currentBattle.playerFaintsHistory / enemyFaintsHistory (-1 = empty) */
    int player_faints, enemy_faints;
    int last_player_faint_turn, last_enemy_faint_turn;
    uint8_t n_last_turn_order; /* dynamicQueueManager.getLastTurnOrder().length */
    pr_pokemon *field[4];      /* player 0/1, enemy 2/3 as in upstream BattlerIndex */
    /* Coverage: attribute classes that would have run upstream but have no C handler
     * yet. A result is only claimed faithful when this stays empty. */
    uint16_t unported[PR_MAX_UNPORTED];
    uint8_t n_unported;
    /* Side effects (heal, stat stage change, battler tag) that the phase engine will
     * carry out; recorded, not yet executed. They don't change the damage result. */
    uint16_t deferred[PR_MAX_UNPORTED];
    uint8_t n_deferred;
} pr_battle;

/* ---- hooks (src/hooks.c) ---- */
/* Upstream applyAbAttrs("<Class>", ...) presence check: does this Pokemon's ability
 * or passive carry an attr that is-a <cls>? Records it as unported when it does. */
bool pr_hook_ab(pr_battle *bt, const pr_pokemon *p, uint16_t cls);
/* applyMoveAttrs("<Class>", ...) presence check for a move. */
bool pr_hook_move(pr_battle *bt, uint16_t move, uint16_t cls);
bool pr_move_has_attr(uint16_t move, uint16_t cls);
void pr_note_unported(pr_battle *bt, uint16_t cls);

/* ---- upstream closures (src/closures.c) ----
 * Each hand-ported closure is registered under the upstream owner + source hash it
 * was written against. If upstream changes the source, the hash stops matching and
 * the closure reads as unported instead of running stale logic. */
/* Arguments follow the upstream closure's own order: (user, target, move) for attack
 * conditions, (target, user, move) for defend conditions, (pokemon) for ability
 * conditions. `self` carries the probed captured values (self->cap). */
typedef double (*pr_closure_fn)(pr_battle *bt, const pr_closure_info *self, const pr_pokemon *a,
                                const pr_pokemon *b, int move);
typedef struct { const char *owner, *hash; pr_closure_fn fn; } pr_closure_reg;
void pr_closures_init(void);
/* Calls closure `id`; when it has no (current) C port, records `cls` as unported
 * and returns `fallback`. */
double pr_closure_call(pr_battle *bt, uint16_t id, uint16_t cls, double fallback,
                       const pr_pokemon *a, const pr_pokemon *b, int move);
static inline bool pr_cap_has(const pr_closure_info *self, int v) { return (self->cap >> (v + 1)) & 1u; }
int pr_closures_ported(void);

/* ---- ability attributes (src/ab_attrs.c) ----
 * One params struct for every applyAbAttrs call site; each site fills what its
 * attribute family reads. Pointers are upstream's NumberHolder/BooleanHolder. */
typedef struct pr_ab_params {
    const pr_pokemon *pokemon;     /* the ability's owner */
    const pr_pokemon *opponent;    /* attacker/defender, or FieldMultiplyStat's target */
    int move;                      /* -1 = none */
    bool simulated;
    bool ignore_ability;           /* AllyStatMultiplierAbAttr.ignoreAbility */
    int stat;
    double *value;                 /* statVal / power / damage / typeMultiplier / critMult / weight / multiplier */
    bool *flag;                    /* cancelled / ignored / hasApplied / blockCrit / isCritical / isUngrounded */
    int *holder;                   /* moveType / weather / critStage */
    int move_type, defender_type;  /* IgnoreTypeImmunityAbAttr */
} pr_ab_params;
/* applyAbAttrs("<cls>", params): ability then passive, conditions, canApply, apply. */
void pr_apply_ab_attrs(pr_battle *bt, uint16_t cls, pr_ab_params *pp);
/* ability.hasAttr / pokemon.getAbility().getAttrs(): presence only, no applicability checks */
const pr_attr *pr_ability_first_attr(uint16_t ability, uint16_t cls);
/* Pokemon.canApplyAbility for `ability` (the ability or the passive) */
bool pr_can_apply_ability(pr_battle *bt, const pr_pokemon *p, uint16_t ability);
/* hasAbility(ability) / hasAbilityWithAttr(cls) with canApply = true */
bool pr_has_ability_active(pr_battle *bt, const pr_pokemon *p, int ability);
bool pr_has_ability_with_attr(pr_battle *bt, const pr_pokemon *p, uint16_t cls);
/* getTag(type): battler tags are not modelled yet. Every tag query goes through here, so
 * porting tags fixes all callers at once. A Pokemon that does carry tags (n_tags > 0)
 * makes the query unanswerable, which is reported as unported. */
bool pr_has_battler_tag(pr_battle *bt, const pr_pokemon *p, int tag_type);
/* findTag(t => t instanceof <class>) for tag classes that span several tag types */
enum { PR_TAGCLASS_PROTECTED, PR_TAGCLASS_SEMI_INVULNERABLE, PR_TAGCLASS_TYPE_BOOST, PR_TAGCLASS_HELPING_HAND,
       PR_TAGCLASS_SUPREME_OVERLORD, PR_TAGCLASS_CRIT_BOOST, PR_TAGCLASS_TAR_SHOT, PR_TAGCLASS_HIGHEST_STAT_BOOST,
       PR_TAGCLASS_EXPOSED, PR_TAGCLASS_STOCKPILING };
bool pr_has_battler_tag_class(pr_battle *bt, const pr_pokemon *p, int tag_class);
/* arena.getTagOnSide(type, side); ARENA_TAG_SIDE_BOTH matches either side, as arena.getTag(type).
 * Arena tags are not modelled yet either. */
bool pr_arena_has_tag(pr_battle *bt, int tag_type, int side);
/* waveData.abilitiesApplied.has(ability): wave data is not modelled yet (empty, as at wave start) */
bool pr_wave_ability_applied(const pr_pokemon *p, int ability);
/* Move.doesFlagEffectApply({flag, user, target}) */
bool pr_move_does_flag_effect_apply(pr_battle *bt, int move, unsigned flag, const pr_pokemon *user,
                                    const pr_pokemon *target);
bool pr_is_opponent(const pr_battle *bt, const pr_pokemon *a, const pr_pokemon *b);
const pr_pokemon *pr_get_ally(const pr_battle *bt, const pr_pokemon *p);
bool pr_weather_effect_suppressed(pr_battle *bt);
int pr_move_get_priority(pr_battle *bt, int move, const pr_pokemon *user);
bool pr_move_is_attack(int move);

/* ---- modifiers (src/modifiers.c) ----
 * Each function is one upstream globalScene.applyModifiers / applyModifier /
 * findModifier / getHeldItems call, named after the modifier class it applies. */
bool pr_mod_is(const pr_modifier *m, int cls);
int32_t pr_mod_get_stack_count(const pr_modifier *m);        /* stackCount + virtualStackCount */
/* pokemon.getHeldItems() (pokemonId === id exactly), filtered to isTransferable when asked:
 * the number of items, and the sum of their stackCount fields (not getStackCount()) */
int pr_held_item_count(pr_battle *bt, const pr_pokemon *p, bool transferable_only);
int32_t pr_held_item_stack_sum(pr_battle *bt, const pr_pokemon *p, bool transferable_only);
/* getHeldItems().find(i => i instanceof PokemonMultiHitModifier)?.getStackCount() ?? 0 */
int32_t pr_held_multi_lens_stacks(pr_battle *bt, const pr_pokemon *p);
/* calculateBaseStats(): Shuckle Juice, Old Gateau, (fusion), vitamins */
void pr_apply_base_stat_modifiers(pr_battle *bt, const pr_pokemon *p, double base[6]);
void pr_apply_incrementing_stat_modifier(pr_battle *bt, const pr_pokemon *p, int stat, double *value);
void pr_apply_nature_weight_modifier(pr_battle *bt, const pr_pokemon *p, double *mult);
void pr_apply_stat_booster_modifiers(pr_battle *bt, const pr_pokemon *p, int stat, double *value);
void pr_apply_temp_stat_stage_booster_modifiers(pr_battle *bt, bool player, int stat, double *mult);
void pr_apply_crit_booster_modifiers(pr_battle *bt, const pr_pokemon *src, int *stage);
void pr_apply_temp_crit_booster_modifiers(pr_battle *bt, bool player, int *stage);
void pr_apply_attack_type_booster_modifiers(pr_battle *bt, const pr_pokemon *src, int move_type, double *power);
/* PokemonMultiHitModifier.apply(pokemon, moveId, null, damageMultiplier) for each Multi Lens */
void pr_apply_multi_hit_damage_modifiers(pr_battle *bt, const pr_pokemon *src, int move, double *mult);
/* globalScene.findModifier(m => m instanceof PokemonMultiHitModifier && m.pokemonId === p.id) (player list) */
bool pr_find_player_multi_lens(pr_battle *bt, const pr_pokemon *p);
void pr_apply_enemy_damage_booster_modifiers(pr_battle *bt, double *damage);
void pr_apply_enemy_damage_reducer_modifiers(pr_battle *bt, double *damage);
/* Move.canBeMultiStrikeEnhanced(user) (restrictSpread false, no target) */
bool pr_move_can_be_multi_strike_enhanced(int move);
/* Math.pow as the reference JS engine computes it (src/js_math.c) */
double js_pow(double x, double y);

/* ---- move attributes (src/move_attrs.c) ---- */
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
bool pr_apply_one_hit_ko(pr_battle *bt, const pr_pokemon *user, const pr_pokemon *target, int move);
double pr_hits_tag_multiplier(pr_battle *bt, const pr_pokemon *target, int move);
void pr_apply_modified_damage(pr_battle *bt, const pr_pokemon *user, const pr_pokemon *target, int move, double *dmg);
void pr_apply_pledge_stab(const pr_pokemon *user, int move, double *stab);

/* ---- rng (battle.ts randSeedInt) ---- */
int32_t pr_battle_rand_seed_int(pr_battle *bt, int32_t range, int32_t min);

/* ---- pokemon.ts ---- */
const pr_species *pr_pokemon_species(const pr_pokemon *p);
const pr_form *pr_pokemon_form(const pr_pokemon *p);
/* getBattlerIndex(): the Pokemon's slot in bt->field, -1 when off the field */
int pr_battler_index(const pr_battle *bt, const pr_pokemon *p);
void pr_calculate_stats(pr_battle *bt, pr_pokemon *p);
int pr_get_tera_type(const pr_pokemon *p);
bool pr_has_ability(const pr_pokemon *p, int ability);
double pr_get_hp_ratio(const pr_pokemon *p, bool precise);
double pr_get_weight(pr_battle *bt, const pr_pokemon *p);
int pr_get_effective_weather_for_move(pr_battle *bt, const pr_pokemon *user);
double js_round(double x);
/* getTypes(): flags mirror upstream's options object. */
enum { PR_TYPES_NO_TERA = 1, PR_TYPES_ORIGINAL_IF_STELLAR = 2 };
int pr_get_types(const pr_pokemon *p, int8_t out[3], unsigned flags);
bool pr_is_of_type(const pr_pokemon *p, int type);
int32_t pr_get_stat(const pr_pokemon *p, int stat, bool bypass_summon);
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
double pr_get_attack_type_effectiveness(pr_battle *bt, const pr_pokemon *def, int move_type,
                                        const pr_pokemon *source, int move);
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
bool pr_get_critical_hit_result(pr_battle *bt, const pr_pokemon *def, const pr_pokemon *src, int move);

#endif
