/* The move benefit scores the enemy AI ranks moves and targets by: Move.getUserBenefitScore /
 * getTargetBenefitScore, AttackMove.getTargetBenefitScore, and the getUserBenefitScore /
 * getTargetBenefitScore of each MoveAttr class that has its own (src/data/moves/move.ts), plus
 * FirstMoveCondition's (src/data/moves/move-condition.ts). Which class's method an attribute runs
 * comes from gen/out/ai_data.c (dumped from the live objects), so an attribute that inherits a
 * method lands on the right code; an owner class the dump didn't know reports PR_AI_FEAT_ATTR_SCORE. */
#include "ai_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static bool has_trainer(const pr_pokemon *p) { return p->is_player || p->has_trainer; }

/* Phaser's Sine.easeOut(v) = Math.sin(v * Math.PI / 2) for v = 0, 0.1 .. 1 (Baton Pass reads no
 * other values), as V8 computes them, so the port doesn't depend on the platform's sin() */
static const double sine_ease_out[11] = {
    0, 0.15643446504023087, 0.3090169943749474, 0.45399049973954675, 0.58778525229247314,
    0.70710678118654746, 0.80901699437494745, 0.89100652418836779, 0.95105651629515353,
    0.98768834059513777, 1,
};

/* ---- session-wide attribute state ---- */
#define AI_SESSION_MAX 32
typedef struct { uint16_t move; int8_t effect; } multi_status_entry;
typedef struct { uint16_t move; double ratio; } heal_ratio_entry;
static multi_status_entry multi_status[AI_SESSION_MAX];
static int n_multi_status;
static heal_ratio_entry heal_ratio[AI_SESSION_MAX];
static int n_heal_ratio;

void pr_ai_session_reset(void) { n_multi_status = n_heal_ratio = 0; }

void pr_ai_session_set_multi_status(int move, int effect)
{
    for (int i = 0; i < n_multi_status; i++)
        if (multi_status[i].move == move) { multi_status[i].effect = (int8_t)effect; return; }
    if (n_multi_status < AI_SESSION_MAX) multi_status[n_multi_status++] = (multi_status_entry){(uint16_t)move, (int8_t)effect};
}

/* effects[0] (the constructor's super(effects[0])) until the move is used */
int pr_ai_session_multi_status(int move)
{
    for (int i = 0; i < n_multi_status; i++)
        if (multi_status[i].move == move) return multi_status[i].effect;
    const ai_move_info *mi = &ai_moves[move];
    for (unsigned i = 0; i < mi->nattr; i++)
        if (ai_attrs[mi->attr0 + i].n_effects) return ai_attrs[mi->attr0 + i].effects[0];
    return 0;
}

void pr_ai_session_set_heal_ratio(int move, double ratio)
{
    for (int i = 0; i < n_heal_ratio; i++)
        if (heal_ratio[i].move == move) { heal_ratio[i].ratio = ratio; return; }
    if (n_heal_ratio < AI_SESSION_MAX) heal_ratio[n_heal_ratio++] = (heal_ratio_entry){(uint16_t)move, ratio};
}

double pr_ai_session_heal_ratio(int move)
{
    for (int i = 0; i < n_heal_ratio; i++)
        if (heal_ratio[i].move == move) return heal_ratio[i].ratio;
    return 1;
}

/* ---- the tables ---- */
bool pr_ai_tables_match(void)
{
    static int8_t checked = -1;
    if (checked >= 0) return checked;
    bool ok = ai_move_count == PR_MOVE_COUNT && !ai_unknown_notes[0];
    for (int id = 0; ok && id < PR_MOVE_COUNT; id++) {
        const ai_move_info *mi = &ai_moves[id];
        if (mi->nattr != pr_moves[id].nattr) { ok = false; break; }
        for (unsigned i = 0; i < mi->nattr; i++)
            if (strcmp(ai_attrs[mi->attr0 + i].cls, pr_attr_class_name[pr_attrs[pr_moves[id].attr0 + i].cls])) {
                ok = false;
                break;
            }
    }
    checked = ok;
    return ok;
}

/* ---- MoveAttr.getUserBenefitScore overrides ---- */

/* ForceSwitchOutAttr.getUserBenefitScore with the attribute's selfSwitch and switchType */
static double force_switch_ubs(pr_battle *bt, const pr_pokemon *user, bool self_switch, int switch_type)
{
    /* globalScene.getEnemyParty(), whichever side the user is on */
    bool found = false;
    if (!bt->n_party[1]) pr_note_unported(bt, PR_FEAT_PARTY);
    for (int i = 0; i < bt->n_party[1] && !found; i++)
        found = ai_is_active(bt->party[1][i], false) && !ai_is_on_field(bt->party[1][i]);
    if (!found) return -20;
    double ret = self_switch ? floor((1 - pr_get_hp_ratio(user, false)) * 20) : 0;
    if (self_switch && switch_type == SWITCH_TYPE_BATON_PASS) {
        int total = 0;
        for (int s = 0; s < 7; s++) total += user->stat_stages[s];
        int k = abs(total) < 10 ? abs(total) : 10;
        ret = ret / 2 + sine_ease_out[k] * (total >= 0 ? 10 : -10);
    }
    return ret;
}

static double sacrificial_eff(pr_battle *bt, const pr_pokemon *user, const pr_pokemon *target, int move)
{
    return pr_get_attack_type_effectiveness(bt, target, pr_moves[move].type, user, -1);
}

static double attr_ubs(pr_battle *bt, const ai_attr_info *a, pr_pokemon *user, pr_pokemon *target, int move)
{
    switch (a->ubs) {
    case AI_OWN_MoveAttr:
        return 0;
    case AI_OWN_HighCritAttr:
        return 3;
    case AI_OWN_CritOnlyAttr:
        return 5;
    case AI_OWN_SurviveDamageAttr:
        return target->hp > 1 ? 0 : -20;
    case AI_OWN_RecoilAttr:
        return floor(pr_moves[move].power / 5.0 / -4);
    case AI_OWN_SacrificialAttr:
    case AI_OWN_SacrificialAttrOnHit:
        if (user->is_boss) return -20;
        return ceil(((1 - pr_get_hp_ratio(user, false)) * 10 - 10) * (sacrificial_eff(bt, user, target, move) - 0.5));
    case AI_OWN_HalfSacrificialAttr:
        if (user->is_boss) return -10;
        return ceil(((1 - pr_get_hp_ratio(user, false) / 2) * 10 - 10) * (sacrificial_eff(bt, user, target, move) - 0.5));
    case AI_OWN_AddSubstituteAttr:
        return user->is_boss ? -10 : 5;
    case AI_OWN_SacrificialFullRestoreAttr:
        return -20;
    case AI_OWN_HitHealAttr:
        if (a->heal_stat >= 0) {
            pr_stat_params sp = {.move = -1, .simulated = true};
            double heal = pr_get_effective_stat_p(bt, target, a->heal_stat, &sp);
            return floor(fmax(0, fmin(1, (heal + user->hp) / ai_max_hp(user) - 0.33)) / pr_get_hp_ratio(user, false));
        }
        return floor(fmax(1 - pr_get_hp_ratio(user, false) - 0.33, 0) * (pr_moves[move].power / 4.0));
    case AI_OWN_StealHeldItemChanceAttr:
    case AI_OWN_RemoveHeldItemAttr:
        return pr_held_item_count(bt, target, false) > 0 ? 5 : 0;
    case AI_OWN_HealStatusEffectAttr:
        return user->status ? 10 : 0;
    case AI_OWN_BypassSleepAttr:
        return user->status == STATUS_EFFECT_SLEEP ? 200 : -10;
    case AI_OWN_TerrainChangeAttr:
        return bt->terrain != TERRAIN_NONE ? 0 : 6;
    case AI_OWN_PartingShotAttr: {
        double score = 0;   /* super: StatStageChangeAttr has none, MoveAttr's 0 */
        if (pr_switch_out_condition(bt, a->self_switch, a->switch_type, user, target, move)) {
            double sw = force_switch_ubs(bt, user, a->self_switch, a->switch_type);
            if (sw > 0) score += sw;
        }
        return score;
    }
    case AI_OWN_GulpMissileTagAttr: {
        bool cramorant = pr_has_ability_active(bt, user, ABILITY_ID_GULP_MISSILE) && user->species == SPECIES_ID_CRAMORANT;
        bool tag = pr_get_battler_tag(user, BATTLER_TAG_TYPE_GULP_MISSILE_ARROKUDA)
                   || pr_get_battler_tag(user, BATTLER_TAG_TYPE_GULP_MISSILE_PIKACHU);   /* getTag(GulpMissileTag) */
        return cramorant && !tag ? 10 : 0;
    }
    case AI_OWN_RevivalBlessingAttr: {
        bool any = false;
        if (!bt->n_party[1]) pr_note_unported(bt, PR_FEAT_PARTY);
        for (int i = 0; i < bt->n_party[1] && !any; i++)
            any = bt->party[1][i]->hp <= 0 && !bt->party[1][i]->is_boss;
        return has_trainer(user) && any ? 20 : -20;
    }
    case AI_OWN_ForceSwitchOutAttr:
        return force_switch_ubs(bt, user, a->self_switch, a->switch_type);
    case AI_OWN_ShiftStatAttr:
        return pr_get_stat(user, a->stat_to_switch_with, false) > pr_get_stat(user, a->stat_to_switch, false) ? 10 : 0;
    case AI_OWN_UNKNOWN:
        pr_note_unported(bt, PR_AI_FEAT_ATTR_SCORE);
        return 0;
    }
    /* a class with only a getTargetBenefitScore can't own getUserBenefitScore */
    pr_note_unported(bt, PR_AI_FEAT_ATTR_SCORE);
    return 0;
}

/* ---- MoveAttr.getTargetBenefitScore overrides ---- */

static bool has_physical_attack(const pr_pokemon *u)
{
    for (int i = 0; i < PR_N_MOVES(u); i++) {
        int m = PR_MOVES(u)[i];
        if (pr_move_is_attack(m) && pr_moves[m].category == MOVE_CATEGORY_PHYSICAL) return true;
    }
    return false;
}

/* StatStageChangeAttr.getTargetBenefitScore */
static double stat_stage_tbs(pr_battle *bt, const ai_attr_info *a, const pr_pokemon *user, const pr_pokemon *target)
{
    int move_levels = a->stages;
    if (a->levels == AI_LEVELS_GROWTH) {
        int w = pr_get_effective_weather_for_move(bt, user);
        if (w == WEATHER_TYPE_SUNNY || w == WEATHER_TYPE_HARSH_SUN) move_levels++;
    }
    double ret = 0;
    for (int i = 0; i < a->n_stats; i++) {
        int stat = a->stats[i];
        int levels = move_levels;
        int stage = target->stat_stages[stat - 1];
        if (levels > 0) levels = (stage + levels < 6 ? stage + levels : 6) - stage;
        else levels = (stage + levels > -6 ? stage + levels : -6) - stage;
        bool no_effect = false;
        switch (stat) {
        /* upstream checks for physical attacks in all four cases */
        case STAT_ATK: case STAT_SPATK:
            if (a->self_target) no_effect = !has_physical_attack(user);
            break;
        case STAT_DEF: case STAT_SPDEF:
            if (!a->self_target) no_effect = !has_physical_attack(user);
            break;
        }
        if (no_effect) continue;
        ret += levels * 4 + (levels > 0 ? -2 : 2);
    }
    return ret;
}

/* t is attr.selfTarget ? user : target, as Move.getTargetBenefitScore passes it */
static double attr_tbs(pr_battle *bt, const ai_attr_info *a, int attr_index, pr_pokemon *user, pr_pokemon *t,
                       int move)
{
    switch (a->tbs) {
    case AI_OWN_MoveAttr:
        return 0;
    case AI_OWN_TargetHalfHpDamageAttr: {
        double r = pr_get_hp_ratio(t, false);
        return r > 0.5 ? floor((r - 0.5) * -24 + 4) : -20;
    }
    case AI_OWN_HealAttr: {
        double ratio = strcmp(a->cls, "VariableHealAttr") ? a->heal_ratio : pr_ai_session_heal_ratio(move);
        double score = (1 - pr_get_hp_ratio(a->self_target ? user : t, false)) * 20 - ratio * 10;
        return js_round(score / (1 - ratio / 2));
    }
    case AI_OWN_FlameBurstAttr:
        return pr_get_ally(bt, t) == NULL ? 0 : -5;
    case AI_OWN_MultiHitAttr:
        return -5;
    case AI_OWN_StatusEffectAttr:
    case AI_OWN_MultiStatusEffectAttr: {
        double chance = ai_move_chance(bt, attr_index, user, t, move, a->self_target, false);
        double score = chance < 0 ? -10 : floor(chance * -0.1);
        pr_pokemon *p = a->self_target ? user : t;
        bool multi = a->tbs == AI_OWN_MultiStatusEffectAttr;
        int effect = multi ? pr_ai_session_multi_status(move) : a->effects[0];
        if (multi && p->status) return 0;
        return pr_can_set_status(bt, NULL, p, effect, true, false, user, false) ? score : 0;
    }
    case AI_OWN_PsychoShiftEffectAttr: {
        int status = user->status ? user->status
                                  : pr_has_ability_active(bt, user, ABILITY_ID_COMATOSE) ? STATUS_EFFECT_SLEEP : STATUS_EFFECT_NONE;
        return !t->status && status && pr_can_set_status(bt, NULL, t, status, true, false, user, false) ? -10 : 0;
    }
    case AI_OWN_StealHeldItemChanceAttr:
    case AI_OWN_RemoveHeldItemAttr:
        return pr_held_item_count(bt, t, false) > 0 ? -5 : 0;
    case AI_OWN_StatStageChangeAttr:
        return stat_stage_tbs(bt, a, user, t);
    case AI_OWN_AddBattlerTagAttr: {
        double chance = ai_move_chance(bt, attr_index, user, t, move, a->self_target, false);
        if (chance < 0) chance = 100;
        return floor(a->tag_score * (chance / 100));
    }
    case AI_OWN_HitsTagAttr:
        return pr_get_battler_tag(t, a->tag_type) ? (a->double_damage ? 10 : 5) : 0;
    case AI_OWN_RepeatMoveAttr:
        return 2;
    case AI_OWN_ReducePpMoveAttr: {
        bool unknown;
        const pr_turn_move *last = pr_last_non_virtual_move(bt, t, false, true, &unknown);
        int slot = -1;
        for (int i = 0; last && i < PR_N_MOVES(t) && slot < 0; i++)
            if (PR_MOVES(t)[i] == last->move) slot = i;
        if (slot < 0) return 0;
        double max_pp = ai_move_pp(t, slot);
        double pp_left = max_pp - PR_PP_USED(t)[slot];
        double value = -(8 - ceil(fmin(max_pp, 30) / 5));
        return pp_left < 4 ? value / 4 * pp_left : value;
    }
    case AI_OWN_UNKNOWN:
        pr_note_unported(bt, PR_AI_FEAT_ATTR_SCORE);
        return 0;
    }
    pr_note_unported(bt, PR_AI_FEAT_ATTR_SCORE);
    return 0;
}

/* ---- Move ---- */

double pr_ai_user_benefit_score(pr_battle *bt, pr_ai *ai, pr_pokemon *user, pr_pokemon *target, int move)
{
    (void)ai;
    if (!pr_ai_tables_match()) pr_note_unported(bt, PR_AI_FEAT_TABLES);
    const ai_move_info *mi = &ai_moves[move];
    double score = 0;
    for (unsigned i = 0; i < mi->nattr; i++) score += attr_ubs(bt, &ai_attrs[mi->attr0 + i], user, target, move);
    /* move.conditions: only FirstMoveCondition has a score of its own (10 when it holds, -20 when not) */
    if (mi->first_move_conds & 128) {
        pr_note_unported(bt, PR_AI_FEAT_ATTR_SCORE);
    } else {
        for (int k = 0; k < mi->first_move_conds; k++) score += user->temp_wave_turn_count == 1 ? 10 : -20;
    }
    return score;
}

double pr_ai_target_benefit_score(pr_battle *bt, pr_ai *ai, pr_pokemon *user, pr_pokemon *target, int move)
{
    (void)ai;
    if (!pr_ai_tables_match()) pr_note_unported(bt, PR_AI_FEAT_TABLES);
    const ai_move_info *mi = &ai_moves[move];
    double score = 0;
    /* a Pokemon inside its ally's mouth (Commander) is scored -20 from either side */
    const pr_pokemon *ally = pr_get_ally(bt, target);
    const pr_battler_tag *cmd = ally ? pr_get_battler_tag(ally, BATTLER_TAG_TYPE_COMMANDED) : NULL;
    if (cmd && pr_pokemon_by_id(bt, cmd->source_id) == target) {
        score = 20 * (target->is_player == user->is_player ? -1 : 1);
    } else {
        for (unsigned i = 0; i < mi->nattr; i++) {
            const ai_attr_info *a = &ai_attrs[mi->attr0 + i];
            score += attr_tbs(bt, a, pr_moves[move].attr0 + i, user, a->self_target ? user : target, move)
                     * (target != user && a->self_target ? -1 : 1);
        }
    }
    if (!pr_move_is_attack(move)) return score;

    /* AttackMove.getTargetBenefitScore: minus how hard the move hits */
    const pr_move *m = &pr_moves[move];
    double eff = pr_get_attack_type_effectiveness(bt, target, m->type, user, move);
    double attack = js_pow(eff - 1, 2) * (eff < 1 ? -2 : 2);
    bool phys = m->category == MOVE_CATEGORY_PHYSICAL;
    pr_stat_params sp = {.opponent = target, .move = -1, .simulated = true};
    double stat = pr_get_effective_stat_p(bt, user, phys ? STAT_ATK : STAT_SPATK, &sp);
    double off = pr_get_effective_stat_p(bt, user, phys ? STAT_SPATK : STAT_ATK, &sp);
    pr_apply_variable_atk(bt, user, target, move, &stat);
    double ratio = off / stat;
    if (ratio <= 0.75) attack *= 2;
    else if (ratio <= 0.875) attack *= 1.5;
    double power = ai_moves[move].eff_power;
    pr_apply_variable_power(bt, user, target, move, &power);
    attack += floor(power / 5);
    return score - attack;
}
