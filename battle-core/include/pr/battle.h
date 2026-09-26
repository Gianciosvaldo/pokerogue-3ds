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

typedef struct pr_pokemon {
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
    uint16_t moves[4];
    int8_t gender;                     /* Gender: -1 genderless, 0 male, 1 female */
    /* turnData (PokemonTurnData) fields read on the damage path */
    uint8_t n_attacks_received;        /* turnData.attacksReceived.length (entries not modelled) */
    bool switched_in_this_turn;
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
    uint8_t terrain;           /* TerrainType; terrain effects beyond "none" are not ported */
    int n_arena_tags;          /* arena tags are not modelled yet: must stay 0 for faithful results */
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
/* getTag(type): battler tags are not modelled yet, so no Pokemon carries one. Every
 * tag query goes through here, so porting tags fixes all callers at once. */
bool pr_has_battler_tag(const pr_pokemon *p, int tag_type);
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

/* ---- move attributes (src/move_attrs.c) ---- */
/* applyMoveAttrs("VariablePowerAttr", user, target, move, power) */
void pr_apply_variable_power(pr_battle *bt, const pr_pokemon *user, const pr_pokemon *target, int move,
                             double *power);

/* ---- rng (battle.ts randSeedInt) ---- */
int32_t pr_battle_rand_seed_int(pr_battle *bt, int32_t range, int32_t min);

/* ---- pokemon.ts ---- */
const pr_form *pr_pokemon_form(const pr_pokemon *p);
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
double pr_get_stat_stage_multiplier(pr_battle *bt, const pr_pokemon *p, int stat,
                                    const pr_pokemon *opponent, int move, bool is_critical);
int32_t pr_get_effective_stat(pr_battle *bt, const pr_pokemon *p, int stat,
                              const pr_pokemon *opponent, int move, bool is_critical, bool for_defend);
int pr_get_move_type(pr_battle *bt, const pr_pokemon *p, int move);
double pr_get_attack_type_effectiveness(pr_battle *bt, const pr_pokemon *def, int move_type,
                                        const pr_pokemon *source, int move);
double pr_get_move_effectiveness(pr_battle *bt, const pr_pokemon *def, const pr_pokemon *src, int move);
/* getMoveEffectiveness with simulated and the cancelled holder */
double pr_get_move_effectiveness_ex(pr_battle *bt, const pr_pokemon *def, const pr_pokemon *src, int move,
                                    bool simulated, bool *cancelled_out);
double pr_calculate_battle_power(pr_battle *bt, const pr_pokemon *src, const pr_pokemon *def, int move);
double pr_get_base_damage(pr_battle *bt, const pr_pokemon *def, const pr_pokemon *src, int move,
                          int category, bool is_critical);
double pr_calculate_stab_multiplier(pr_battle *bt, const pr_pokemon *src, int move);

typedef struct { bool cancelled; int result /* HitResult */; int32_t damage; } pr_damage_result;
/* simulated=true: random factor is 1 and no RNG is consumed, as upstream. */
pr_damage_result pr_get_attack_damage(pr_battle *bt, pr_pokemon *def, pr_pokemon *src, int move,
                                      bool is_critical, bool simulated);
bool pr_get_critical_hit_result(pr_battle *bt, const pr_pokemon *def, const pr_pokemon *src, int move);

#endif
