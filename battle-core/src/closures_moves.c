/* Hand ports of upstream move closures. Each entry names the closure it was
 * written against (owner + hash from gen/out/closures.txt); the JS source is
 * quoted above each function. A closure whose inputs aren't modelled yet
 * reports itself unported instead of guessing. */
#include "pr/battle.h"

#define UNPORTED(id) do { pr_note_unported(bt, (id)); return 1; } while (0)
#define ARGS pr_battle *bt, const pr_closure_info *self, const pr_pokemon *u, const pr_pokemon *t, int m

/* Earthquake, Magnitude, Bulldoze: arena.terrainType === GRASSY && target.isGrounded() ? .5 : 1 */
static double grassy_grounded_half(ARGS)
{
    return bt->terrain == TERRAIN_GRASSY && pr_is_grounded(bt, t, false) ? .5 : 1;
}

/* Facade: user.status is BURN/POISON/TOXIC/PARALYSIS ? 2 : 1 */
static double facade(ARGS)
{
    int s = u->status;
    return (s == STATUS_EFFECT_BURN || s == STATUS_EFFECT_POISON || s == STATUS_EFFECT_TOXIC
            || s == STATUS_EFFECT_PARALYSIS) ? 2 : 1;
}

/* Smelling Salts: target.status?.effect === PARALYSIS ? 2 : 1 */
static double smelling_salts(ARGS)
{
    return t->status == STATUS_EFFECT_PARALYSIS ? 2 : 1;
}

/* Weather Ball: getEffectiveWeatherForMove(user) === NONE ? 1 : 2 */
static double weather_ball(ARGS)
{
    return pr_get_effective_weather_for_move(bt, u) == WEATHER_TYPE_NONE ? 1 : 2;
}

/* Brine: target.getHpRatio() < .5 ? 2 : 1 */
static double brine(ARGS)
{
    return pr_get_hp_ratio(t, false) < .5 ? 2 : 1;
}

/* Venoshock, Barb Barrage: target poisoned or badly poisoned ? 2 : 1 */
static double target_poisoned(ARGS)
{
    return (t->status == STATUS_EFFECT_POISON || t->status == STATUS_EFFECT_TOXIC) ? 2 : 1;
}

/* Hex: target.status || target.hasAbility(COMATOSE) ? 2 : 1
 * (upstream `status` is a Status object or null; NONE never occurs as an object here) */
static double hex(ARGS)
{
    return (t->status != STATUS_EFFECT_NONE || pr_has_ability_active(bt, t, ABILITY_ID_COMATOSE)) ? 2 : 1;
}

/* Infernal Parade: target.status ? 2 : 1 */
static double infernal_parade(ARGS)
{
    return t->status != STATUS_EFFECT_NONE ? 2 : 1;
}

/* Grav Apple: arena.getTag("GRAVITY") ? 1.5 : 1 */
static double grav_apple(ARGS)
{
    return pr_arena_has_tag(bt, ARENA_TAG_TYPE_GRAVITY, ARENA_TAG_SIDE_BOTH) ? 1.5 : 1;
}

/* Expanding Force: arena.terrainType === PSYCHIC && user.isGrounded() ? 1.5 : 1 */
static double expanding_force(ARGS)
{
    return bt->terrain == TERRAIN_PSYCHIC && pr_is_grounded(bt, u, false) ? 1.5 : 1;
}

/* Misty Explosion: arena.terrainType === MISTY && user.isGrounded() ? 1.5 : 1 */
static double misty_explosion(ARGS)
{
    return bt->terrain == TERRAIN_MISTY && pr_is_grounded(bt, u, false) ? 1.5 : 1;
}

/* Rising Voltage: arena.terrainType === ELECTRIC && target.isGrounded() ? 2 : 1 */
static double rising_voltage(ARGS)
{
    return bt->terrain == TERRAIN_ELECTRIC && pr_is_grounded(bt, t, false) ? 2 : 1;
}

/* Terrain Pulse: arena.terrainType !== NONE && user.isGrounded() ? 2 : 1 */
static double terrain_pulse(ARGS)
{
    return bt->terrain != TERRAIN_NONE && pr_is_grounded(bt, u, false) ? 2 : 1;
}

/* Psyblade: arena.terrainType === ELECTRIC && user.isGrounded() ? 1.5 : 1 */
static double psyblade(ARGS)
{
    return bt->terrain == TERRAIN_ELECTRIC && pr_is_grounded(bt, u, false) ? 1.5 : 1;
}

/* Collision Course, Electro Drift:
 * target.getAttackTypeEffectiveness(move.type, { source: user }) >= 2 ? 4 / 3 : 1
 * (no `move` in the options, so no MoveTypeChartOverrideAttr; move.type is the base type) */
static double super_effective_boost(ARGS)
{
    return pr_get_attack_type_effectiveness(bt, t, pr_moves[m].type, u, -1) >= 2 ? 4.0 / 3 : 1;
}

/* Knock Off: target.getHeldItems().filter((i) => i.isTransferable).length > 0 ? 1.5 : 1 */
static double knock_off(ARGS)
{
    return pr_held_item_count(bt, t, true) > 0 ? 1.5 : 1;
}

/* Wake-Up Slap: targetSleptOrComatoseCondition.apply(user, target, move) ? 2 : 1, where the
 * condition (move-condition.ts, not covered by the hash) is
 * target.status?.effect === SLEEP || target.hasAbility(COMATOSE) */
static double wake_up_slap(ARGS)
{
    return (t->status == STATUS_EFFECT_SLEEP || pr_has_ability_active(bt, t, ABILITY_ID_COMATOSE)) ? 2 : 1;
}

/* Payback: target.turnData.acted
 *   || globalScene.currentBattle.turnCommands[target.getBattlerIndex()]?.command === 1 (BALL) ? 2 : 1 */
static double payback(ARGS)
{
    int i = pr_battler_index(bt, t);
    return t->acted || (i >= 0 && bt->turn_command[i] == COMMAND_BALL) ? 2 : 1;
}

/* Assurance: target.turnData.damageTaken > 0 ? 2 : 1 */
static double assurance(ARGS)
{
    return t->damage_taken > 0 ? 2 : 1;
}

/* Acrobatics: itemCount = user.getHeldItems().filter(i => i.isTransferable)
 *   .reduce((v, m) => v + m.stackCount, 0);  Math.max(1, 2 - .2 * itemCount) */
static double acrobatics(ARGS)
{
    double v = 2 - .2 * pr_held_item_stack_sum(bt, u, true);
    return v > 1 ? v : 1;
}

/* Retaliate: lastPlayerFaint = currentBattle.playerFaintsHistory.at(-1) (and the enemy's);
 * lastPlayerFaint !== undefined && turn - lastPlayerFaint.turn === 1 && user.isPlayer()
 *   || lastEnemyFaint !== undefined && turn - lastEnemyFaint.turn === 1 && user.isEnemy() ? 2 : 1 */
static double retaliate(ARGS)
{
    bool p = bt->last_player_faint_turn >= 0 && bt->turn - bt->last_player_faint_turn == 1 && u->is_player;
    bool e = bt->last_enemy_faint_turn >= 0 && bt->turn - bt->last_enemy_faint_turn == 1 && !u->is_player;
    return p || e ? 2 : 1;
}

/* Stomping Tantrum: lastNonDancerMove = user.getLastXMoves(2)[1];
 * lastNonDancerMove && (result === MISS || result === FAIL) ? 2 : 1
 * Temper Flare: user.getLastXMoves(2)[1]?.result === 3 (MISS) || ...result === 2 (FAIL) ? 2 : 1 */
static double failed_last_move(ARGS)
{
    const pr_turn_move *e = pr_last_move(bt, u, 1);
    return e && (e->result == MOVE_RESULT_MISS || e->result == MOVE_RESULT_FAIL) ? 2 : 1;
}

/* ConsecutiveUsePowerMultiplierAttr's constructor closure (Rollout, Ice Ball, Fury Cutter,
 * Echoed Voice). It captures limit, resetOnFail, resetOnLimit and comboMoves, which the dump
 * probes into self->vals = [limit, resetOnFail, resetOnLimit, ...comboMoves]:
 *   const moveHistory = user.getLastXMoves(limit + 1).slice(1);
 *   let count = 0; let turnMove;
 *   while (((turnMove = moveHistory.shift())?.move === move.id
 *           || (comboMoves.length > 0 && comboMoves.includes(turnMove?.move ?? MoveId.NONE)))
 *          && (!resetOnFail || turnMove?.result === MoveResult.SUCCESS)) {
 *     if (count < limit - 1) count++; else if (resetOnLimit) count = 0; else break;
 *   }
 *   return this.getMultiplier(count);
 * where getMultiplier is Math.pow(2, count) (ConsecutiveUseDoublePowerAttr) or count + 1
 * (ConsecutiveUseMultiBasePowerAttr), `this` being the attribute that owns the closure. */
static double consecutive_use(ARGS)
{
    if (!self->vals || self->n_vals < 3) UNPORTED(PR_AC_ConsecutiveUsePowerMultiplierAttr);
    int limit = self->vals[0];
    bool reset_on_fail = self->vals[1], reset_on_limit = self->vals[2];
    const int16_t *combo = self->vals + 3;
    int n_combo = self->n_vals - 3;
    int count = 0;
    for (int k = 1;; k++) {
        /* moveHistory.shift(): entry k of getLastXMoves(limit + 1), undefined past the end */
        const pr_turn_move *tm = k <= limit ? pr_last_move(bt, u, k) : NULL;
        if (!tm && k <= limit && k < u->n_move_history) return 1; /* older than the kept entries (reported) */
        int looked_up = tm ? tm->move : MOVE_ID_NONE;
        bool in_combo = false;
        for (int c = 0; c < n_combo; c++) in_combo |= combo[c] == looked_up;
        bool match = ((tm && tm->move == m) || in_combo)
                     && (!reset_on_fail || (tm && tm->result == MOVE_RESULT_SUCCESS));
        if (!match) break;
        /* With NONE among the combo moves upstream would loop on the missing entries */
        if (!tm) UNPORTED(PR_FEAT_MOVE_HISTORY);
        if (count < limit - 1) count++;
        else if (reset_on_limit) count = 0;
        else break;
    }
    int id = (int)(self - pr_closures_meta);
    for (unsigned i = 0; i < pr_moves[m].nattr; i++) {
        const pr_attr *a = &pr_attrs[pr_moves[m].attr0 + i];
        if (a->cls == PR_AC_ConsecutiveUseDoublePowerAttr
            && ((const pr_attr_ConsecutiveUseDoublePowerAttr *)a->p)->powerMultiplierFunc == id)
            return js_pow(2, count);
        if (a->cls == PR_AC_ConsecutiveUseMultiBasePowerAttr
            && ((const pr_attr_ConsecutiveUseMultiBasePowerAttr *)a->p)->powerMultiplierFunc == id)
            return count + 1;
    }
    UNPORTED(PR_AC_ConsecutiveUsePowerMultiplierAttr);
}

/* Dynamax Cannon: if (target.level > globalScene.getMaxExpLevel()) {
 *   overLevel = target.level - maxExpLevel; damageFactor = maxExpLevel * .05;
 *   return 1 + Math.min(1, overLevel / damageFactor); } return 1; */
static double dynamax_cannon(ARGS)
{
    if (!bt->max_exp_level) UNPORTED(PR_FEAT_WAVE);
    if (t->level > bt->max_exp_level) {
        double over = t->level - bt->max_exp_level;
        double factor = bt->max_exp_level * .05;
        double r = over / factor;
        return 1 + (r < 1 ? r : 1);
    }
    return 1;
}

/* Bolt Beak, Fishious Rend: target.turnData.acted ? 1 : 2 */
static double target_not_acted(ARGS)
{
    return t->acted ? 1 : 2;
}

/* Lash Out: user.turnData.statStagesDecreased ? 2 : 1 */
static double lash_out(ARGS)
{
    return u->stat_stages_decreased ? 2 : 1;
}

/* Last Respects: 1 + Math.min(user.isPlayer() ? arena.playerFaints : currentBattle.enemyFaints, 100) */
static double last_respects(ARGS)
{
    int f = u->is_player ? bt->player_faints : bt->enemy_faints;
    return 1 + (f < 100 ? f : 100);
}

/* Grassy Glide priority: arena.terrainType === GRASSY && user.isGrounded() */
static double grassy_glide(ARGS)
{
    return bt->terrain == TERRAIN_GRASSY && pr_is_grounded(bt, u, false);
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
    {"move:797", "96f28b35e308", expanding_force},
    {"move:802", "2444769ee78b", misty_explosion},
    {"move:804", "a34673858968", rising_voltage},
    {"move:805", "fabf9892eb01", terrain_pulse},
    {"move:875", "2835041fb433", psyblade},
    {"move:878", "e9c7832692dc", super_effective_boost},
    {"move:879", "e9c7832692dc", super_effective_boost},
    {"move:282", "3b4717f26cc0", knock_off},
    {"move:358", "eb36d00c9b65", wake_up_slap},
    {"move:371", "f43144fff1c9", payback},
    {"move:372", "24376a7b4b8e", assurance},
    {"move:512", "8459b95e80a8", acrobatics},
    {"move:514", "cc1c3783a6d6", retaliate},
    {"move:205", "8cbec46369f4", consecutive_use}, /* Rollout */
    {"move:301", "8cbec46369f4", consecutive_use}, /* Ice Ball */
    {"move:210", "60455f51213a", consecutive_use}, /* Fury Cutter */
    {"move:497", "5141b1d3bea9", consecutive_use}, /* Echoed Voice */
    {"move:707", "4a9da7faa50b", failed_last_move},
    {"move:915", "eb786cc32675", failed_last_move},
    {"move:744", "39036af00597", dynamax_cannon},
    {"move:754", "b92457609c24", target_not_acted},
    {"move:755", "b92457609c24", target_not_acted},
    {"move:808", "e64793309a8e", lash_out},
    {"move:803", "1a3ab5759086", grassy_glide}, /* closure 387 */
    {"move:854", "3cbd6fbd3493", last_respects},
};
const int pr_move_closures_n = sizeof pr_move_closures / sizeof pr_move_closures[0];
