/* Hand ports of upstream move closures. Each entry names the closure it was
 * written against (owner + hash from gen/out/closures.txt); the JS source is
 * quoted above each function. A closure whose inputs aren't modelled yet
 * reports itself unported instead of guessing. */
#include "pr/battle.h"

#define UNPORTED(cls) do { pr_note_unported(bt, (cls)); return 1; } while (0)

/* Earthquake, Magnitude, Bulldoze:
 * arena.terrainType === GRASSY(3) && target.isGrounded() ? .5 : 1 */
static double grassy_grounded_half(pr_battle *bt, const pr_closure_info *self, const pr_pokemon *u, const pr_pokemon *t, int m)
{
    if (bt->terrain != 3) return 1;
    UNPORTED(PR_AC_MovePowerMultiplierAttr); /* isGrounded() not ported */
}

/* Facade: user.status is BURN/POISON/TOXIC/PARALYSIS ? 2 : 1 */
static double facade(pr_battle *bt, const pr_closure_info *self, const pr_pokemon *u, const pr_pokemon *t, int m)
{
    int s = u->status;
    return (s == STATUS_EFFECT_BURN || s == STATUS_EFFECT_POISON || s == STATUS_EFFECT_TOXIC
            || s == STATUS_EFFECT_PARALYSIS) ? 2 : 1;
}

/* Smelling Salts: target.status?.effect === PARALYSIS ? 2 : 1 */
static double smelling_salts(pr_battle *bt, const pr_closure_info *self, const pr_pokemon *u, const pr_pokemon *t, int m)
{
    return t->status == STATUS_EFFECT_PARALYSIS ? 2 : 1;
}

/* Weather Ball: getEffectiveWeatherForMove(user) === NONE ? 1 : 2 */
static double weather_ball(pr_battle *bt, const pr_closure_info *self, const pr_pokemon *u, const pr_pokemon *t, int m)
{
    return pr_get_effective_weather_for_move(bt, u) == WEATHER_TYPE_NONE ? 1 : 2;
}

/* Brine: target.getHpRatio() < .5 ? 2 : 1 */
static double brine(pr_battle *bt, const pr_closure_info *self, const pr_pokemon *u, const pr_pokemon *t, int m)
{
    return pr_get_hp_ratio(t, false) < .5 ? 2 : 1;
}

/* Venoshock, Barb Barrage: target poisoned or badly poisoned ? 2 : 1 */
static double target_poisoned(pr_battle *bt, const pr_closure_info *self, const pr_pokemon *u, const pr_pokemon *t, int m)
{
    return (t->status == STATUS_EFFECT_POISON || t->status == STATUS_EFFECT_TOXIC) ? 2 : 1;
}

/* Hex: target.status || target.hasAbility(COMATOSE) ? 2 : 1
 * (upstream `status` is a Status object or null; NONE never occurs as an object here) */
static double hex(pr_battle *bt, const pr_closure_info *self, const pr_pokemon *u, const pr_pokemon *t, int m)
{
    return (t->status != STATUS_EFFECT_NONE || pr_has_ability(t, ABILITY_ID_COMATOSE)) ? 2 : 1;
}

/* Infernal Parade: target.status ? 2 : 1 */
static double infernal_parade(pr_battle *bt, const pr_closure_info *self, const pr_pokemon *u, const pr_pokemon *t, int m)
{
    return t->status != STATUS_EFFECT_NONE ? 2 : 1;
}

/* Grav Apple: arena.getTag(GRAVITY) ? 1.5 : 1 */
static double grav_apple(pr_battle *bt, const pr_closure_info *self, const pr_pokemon *u, const pr_pokemon *t, int m)
{
    if (bt->n_arena_tags) UNPORTED(PR_AC_MovePowerMultiplierAttr);
    return 1;
}

/* Expanding Force (PSYCHIC 4, 1.5), Misty Explosion (MISTY 1, 1.5), Psyblade (ELECTRIC 2, 1.5),
 * Rising Voltage (ELECTRIC 2 && target grounded, 2), Terrain Pulse (terrain !== NONE && user grounded, 2):
 * all are 1 when there is no terrain, which is the only terrain state modelled. */
static double terrain_boost(pr_battle *bt, const pr_closure_info *self, const pr_pokemon *u, const pr_pokemon *t, int m)
{
    if (bt->terrain == 0) return 1;
    UNPORTED(PR_AC_MovePowerMultiplierAttr);
}

/* Collision Course, Electro Drift:
 * target.getAttackTypeEffectiveness(move.type, { source: user }) >= 2 ? 4 / 3 : 1
 * (no `move` in the options, so no MoveTypeChartOverrideAttr; move.type is the base type) */
static double super_effective_boost(pr_battle *bt, const pr_closure_info *self, const pr_pokemon *u, const pr_pokemon *t, int m)
{
    return pr_get_attack_type_effectiveness(bt, t, pr_moves[m].type, u, -1) >= 2 ? 4.0 / 3 : 1;
}

const pr_closure_reg pr_move_closures[] = {
    {"move:89", "d551c83b79fb", grassy_grounded_half},
    {"move:222", "d551c83b79fb", grassy_grounded_half},
    {"move:523", "d551c83b79fb", grassy_grounded_half},
    {"move:263", "ea1540a66027", facade},
    {"move:265", "2ccd7cdfa0e2", smelling_salts},
    {"move:311", "6fbad4dacd05", weather_ball},
    {"move:362", "72589d6d014e", brine},
    {"move:474", "bf9d9577ea99", target_poisoned},
    {"move:839", "bf9d9577ea99", target_poisoned},
    {"move:506", "1515515f0e09", hex},
    {"move:844", "816abc2701d9", infernal_parade},
    {"move:788", "580cb60ef7a2", grav_apple},
    {"move:797", "96f28b35e308", terrain_boost},
    {"move:802", "2444769ee78b", terrain_boost},
    {"move:804", "a34673858968", terrain_boost},
    {"move:805", "fabf9892eb01", terrain_boost},
    {"move:875", "2835041fb433", terrain_boost},
    {"move:878", "e9c7832692dc", super_effective_boost},
    {"move:879", "e9c7832692dc", super_effective_boost},
};
const int pr_move_closures_n = sizeof pr_move_closures / sizeof pr_move_closures[0];
