/* The tables behind game-core's port of src/ai/ai-moveset-gen.ts (gen/out/gc_movegen_data.c, dumped by gen/dump-movegen.test.ts). */
#ifndef GC_MOVEGEN_H
#define GC_MOVEGEN_H
#include <stdbool.h>
#include <stdint.h>

/* src/data/balance/moves/moveset-generation.ts */
typedef struct {
    int base_level_weight_offset, evo_move_bp_threshold, evolution_move_weight, relearn_move_weight;
    double base_weight_multiplier;
    int move_power_ceiling, forced_signature_move_chance;
} gc_mg_constants;
extern const gc_mg_constants gc_mg_const;

/* gc_mg_move.flags */
enum {
    GC_MGF_CHARGING = 1 << 0, GC_MGF_RECHARGE = 1 << 1, GC_MGF_DELAYED = 1 << 2, GC_MGF_MULTIHIT_POWER_INC = 1 << 3,
    GC_MGF_MULTIHIT = 1 << 4, GC_MGF_DEF_ATK = 1 << 5, GC_MGF_PHOTON = 1 << 6, GC_MGF_SHELL_SIDE = 1 << 7,
    GC_MGF_FIXED_DAMAGE = 1 << 8, GC_MGF_TARGET_ATK_USER_ATK = 1 << 9, GC_MGF_SACRIFICIAL = 1 << 10,
    GC_MGF_SACRIFICIAL_ON_HIT = 1 << 11, GC_MGF_WEATHER_CHANGE = 1 << 12, GC_MGF_COND_TARGET_SLEEP = 1 << 13,
    GC_MGF_COND_USER_SLEEP = 1 << 14, GC_MGF_SELF_STATUS_SINGLE = 1 << 15, GC_MGF_THUNDER_ACC = 1 << 16,
    GC_MGF_STORM_ACC = 1 << 17, GC_MGF_ANTI_SUN = 1 << 18, GC_MGF_SLEEP_STATUS = 1 << 19, GC_MGF_SLEEP_SELF = 1 << 20,
    GC_MGF_POISON_ATTR = 1 << 21, GC_MGF_VARIABLE_TYPE = 1 << 22, GC_MGF_WEATHER_BALL_TYPE = 1 << 23,
    GC_MGF_TERA_CATEGORY = 1 << 24, GC_MGF_CALL_MOVE = 1 << 25,
};
/* gc_mg_move.vt_kind: the class of the move's last VariableMoveTypeAttr */
enum {
    GC_MGV_NONE, GC_MGV_FORM_CHANGE_ITEM, GC_MGV_TECHNO_BLAST, GC_MGV_AURA_WHEEL, GC_MGV_RAGING_BULL, GC_MGV_IVY_CUDGEL,
    GC_MGV_WEATHER_BALL, GC_MGV_HIDDEN_POWER, GC_MGV_TERA_BLAST, GC_MGV_TERA_STARSTORM, GC_MGV_MATCH_USER_TYPE,
    GC_MGV_TERRAIN_PULSE, GC_MGV_COMBINED_PLEDGE,
};
/* What the generator asks of a move beyond battle-core's pr_move: by MoveId */
typedef struct {
    uint32_t flags;
    uint8_t vt_kind;
    uint8_t boost_cat;          /* removeSelfStatBoost's category: 0 none, else MoveCategory + 1 */
    uint16_t weather_instant;   /* bit per WeatherType: isWeatherInstantCharge(move, weather) */
    uint8_t charge_sun;         /* an isChargingMove() whose WeatherInstantChargeAttr covers SUNNY */
} gc_mg_move;
extern const int gc_mg_move_count;
extern const gc_mg_move gc_mg_moves[];

/* gc_mg_ability.bits */
enum {
    GC_MGA_WEATHER_CHANGE = 1 << 0, GC_MGA_SUPPRESS_WEATHER = 1 << 1, GC_MGA_PREATTACK_WEATHER = 1 << 2,
    GC_MGA_IGNORE_CONTACT = 1 << 3, GC_MGA_DEF_POISON = 1 << 4, GC_MGA_ATK_POISON = 1 << 5, GC_MGA_ATK_TOXIC = 1 << 6,
    GC_MGA_ATK_TOXIC_NO_CONTACT = 1 << 7, GC_MGA_SUN_INSTANT = 1 << 8,
};
typedef struct { uint16_t bits; int8_t summon_weather; } gc_mg_ability;   /* by AbilityId */
extern const int gc_mg_ability_count;
extern const gc_mg_ability gc_mg_abilities[];

/* FORCED_SIGNATURE_MOVES: a species' move (is_list 0) or list of moves (gc_mg_sig_moves[first .. first + n)) */
typedef struct { uint16_t species, first; uint8_t n, is_list; } gc_mg_signature;
extern const uint16_t gc_mg_sig_moves[];
extern const int gc_mg_sig_count;
extern const gc_mg_signature gc_mg_signatures[];
extern const int gc_mg_stab_blacklist_count;
extern const uint16_t gc_mg_stab_blacklist[];

/* Move.effectivePowerAbilityCheck for an ability: the result a (ability, move) gets when nothing about the user matters
 * (dim 0), and where one of these does (dim 1 weather = WeatherType, 2 gender = Gender (move 0: every damaging move), 3 Flying, 4 the move's type =
 * PokemonType, for variable-type moves): by (ability, move, dim, val) */
typedef struct { uint16_t ability, move; uint8_t dim, val; double power_mult, acc_mult; uint8_t instant, max_multi; } gc_mg_pow;
extern const int gc_mg_pow_count;
extern const gc_mg_pow gc_mg_pows[];
#endif
