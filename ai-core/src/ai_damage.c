/* The two damage-path functions the AI calls with ignore flags: Pokemon.getMoveEffectiveness and
 * Pokemon.getAttackDamage. They are battle-core's ports (src/damage.c) with upstream's
 * ignoreAbility / ignoreAllyAbility branches put back; battle-core's own versions don't take the
 * flags because no battle path passes them. Keep in step with src/damage.c until battle-core
 * exports flagged versions, then call those instead. */
#include "ai_internal.h"
#include <math.h>

static int32_t to_dmg_value(double v)
{
    v = floor(v);
    return v > 1 ? (int32_t)v : 1;
}

/* getAlliesGenerator(): battle-core models singles only here (the side is p alone) */
static const pr_pokemon *allies_singles(pr_battle *bt, const pr_pokemon *p)
{
    if (bt->double_battle) pr_note_unported(bt, PR_FEAT_DOUBLES);
    return p;
}

double ai_move_effectiveness(pr_battle *bt, const pr_pokemon *def, const pr_pokemon *src, int move,
                             bool ignore_ability, bool use_illusion, bool *cancelled_out)
{
    const bool simulated = true;
    if (cancelled_out) *cancelled_out = false;
    if (def->turn_move_effectiveness >= 0) return def->turn_move_effectiveness;
    const pr_move *m = &pr_moves[move];
    if (pr_move_has_attr((uint16_t)move, PR_AC_TypelessAttr)) return 1;
    int move_type = pr_get_move_type(bt, src, move);
    if (use_illusion) ai_note_illusion(bt, def);
    double tm = (m->category != MOVE_CATEGORY_STATUS || pr_move_has_attr((uint16_t)move, PR_AC_RespectAttackTypeImmunityAttr))
                    ? pr_get_attack_type_effectiveness_ex(bt, def, move_type, src, move, simulated)
                    : 1;
    /* move.isTypeImmune: powder moves vs Grass, Prankster status moves vs Dark */
    if (m->target != MOVE_TARGET_USER) {
        bool immune = false;
        int8_t dt[PR_MAX_TYPES];
        int dn = pr_get_types(def, dt, PR_TYPES_ORIGINAL_IF_STELLAR);
        for (int i = 0; i < dn; i++) {
            if (dt[i] == POKEMON_TYPE_GRASS && (m->flags & MOVE_FLAGS_POWDER_MOVE)) immune = true;
            if (dt[i] == POKEMON_TYPE_DARK && pr_has_ability_active(bt, src, ABILITY_ID_PRANKSTER)
                && m->category == MOVE_CATEGORY_STATUS && pr_is_opponent(bt, src, def))
                immune = true;
        }
        if (immune) tm = 0;
    }
    if (pr_has_battler_tag_class(bt, def, PR_TAGCLASS_TAR_SHOT) && move_type == POKEMON_TYPE_FIRE) tm *= 2;

    bool cancelled = false;
    pr_ab_params pp = {.pokemon = def, .opponent = src, .move = move, .simulated = simulated,
                       .flag = &cancelled, .value = &tm};
    if (!ignore_ability) {
        pr_apply_ab_attrs(bt, PR_AC_TypeImmunityAbAttr, &pp);
        if (!cancelled) pr_apply_ab_attrs(bt, PR_AC_MoveImmunityAbAttr, &pp);
        if (!cancelled) {
            pr_ab_params fp = {.pokemon = allies_singles(bt, def), .opponent = src, .move = move,
                               .simulated = simulated, .flag = &cancelled};
            pr_apply_ab_attrs(bt, PR_AC_FieldPriorityMoveImmunityAbAttr, &fp);
        }
    }
    if (!ignore_ability && m->category != MOVE_CATEGORY_STATUS) pr_apply_ab_attrs(bt, PR_AC_FullHpResistTypeAbAttr, &pp);
    if (m->category == MOVE_CATEGORY_STATUS && pr_move_hits_substitute(bt, move, src, def)) tm = 0;
    if (cancelled_out) *cancelled_out = cancelled;
    return cancelled ? 0 : tm;
}

pr_damage_result ai_attack_damage(pr_battle *bt, pr_pokemon *def, pr_pokemon *src, int move, bool ignore_ability,
                                  bool ignore_ally_ability, bool is_critical)
{
    const bool simulated = true;
    pr_damage_result r = {false, HIT_RESULT_EFFECTIVE, 0};
    int category = pr_apply_variable_move_category(bt, src, def, move, pr_moves[move].category);

    bool cancelled = false;
    double type_multiplier = ai_move_effectiveness(bt, def, src, move, ignore_ability, false, &cancelled);
    if (cancelled || type_multiplier == 0) {
        r.cancelled = cancelled;
        r.result = move == MOVE_ID_SHEER_COLD ? HIT_RESULT_IMMUNE : HIT_RESULT_NO_EFFECT;
        return r;
    }

    bool full_hp = def->hp >= pr_get_stat(def, STAT_HP, true);   /* isFullHp() (nothing here changes hp) */
    double fixed = pr_apply_fixed_damage(bt, src, def, move);
    if (fixed != 0 && fixed == fixed) {
        double lens = 1;
        pr_apply_multi_hit_damage_modifiers(bt, src, move, &lens);
        fixed = to_dmg_value(fixed * lens);
        if (full_hp && !ignore_ability) {
            pr_ab_params ep = {.pokemon = def, .opponent = src, .move = move, .simulated = simulated, .value = &fixed};
            pr_apply_ab_attrs(bt, PR_AC_PreDefendFullHpEndureAbAttr, &ep);
        }
        r.damage = fixed;
        return r;
    }
    if (pr_apply_one_hit_ko(bt, src, def, move)) {
        r.result = HIT_RESULT_ONE_HIT_KO;
        r.damage = def->hp;
        return r;
    }

    pr_base_damage_params bp = {.move = move, .category = category, .ignore_ability = ignore_ability,
                                .ignore_ally_ability = ignore_ally_ability, .is_critical = is_critical,
                                .simulated = simulated};
    double base = pr_get_base_damage(bt, def, src, &bp);

    /* getWeatherMultiplierForMove */
    int weather = pr_get_effective_weather_for_move(bt, src);
    int move_type = pr_get_move_type(bt, src, move);
    double weather_mult = 1;
    bool overridden = false;
    const pr_move *mm = &pr_moves[move];
    for (unsigned i = 0; i < mm->nattr; i++) {
        const pr_attr *a = &pr_attrs[mm->attr0 + i];
        if (pr_attr_is(a->cls, PR_AC_OverrideWeatherMultiplierAttr)
            && ((const pr_attr_OverrideWeatherMultiplierAttr *)a->p)->weather == weather) {
            weather_mult = 1.5;
            overridden = true;
            break;
        }
    }
    if (!overridden) switch (weather) {
    case WEATHER_TYPE_SUNNY: case WEATHER_TYPE_HARSH_SUN:
        if (move_type == POKEMON_TYPE_FIRE) weather_mult = 1.5;
        else if (move_type == POKEMON_TYPE_WATER) weather_mult = 0.5;
        break;
    case WEATHER_TYPE_RAIN: case WEATHER_TYPE_HEAVY_RAIN:
        if (move_type == POKEMON_TYPE_FIRE) weather_mult = 0.5;
        else if (move_type == POKEMON_TYPE_WATER) weather_mult = 1.5;
        break;
    }

    double target_mult = 1;   /* spread moves in doubles: as battle-core, singles only */
    if (bt->double_battle) pr_note_unported(bt, PR_FEAT_DOUBLES);

    double multi_strike = 1;
    pr_apply_multi_hit_damage_modifiers(bt, src, move, &multi_strike);
    double glaive = pr_has_battler_tag(bt, def, BATTLER_TAG_TYPE_RECEIVE_DOUBLE_DAMAGE) ? 2 : 1;
    double crit_mult = is_critical ? 1.5 : 1;
    pr_ab_params cp = {.pokemon = src, .move = -1, .simulated = simulated, .value = &crit_mult};
    pr_apply_ab_attrs(bt, PR_AC_MultCritAbAttr, &cp);
    double random_mult = 1;   /* simulated */
    double stab = pr_calculate_stab_multiplier(bt, src, move, false, simulated);

    double burn = 1;
    if (category == MOVE_CATEGORY_PHYSICAL && src->status == STATUS_EFFECT_BURN
        && !pr_move_has_attr((uint16_t)move, PR_AC_BypassBurnDamageReductionAttr)) {
        bool bypass = false;
        pr_ab_params bp2 = {.pokemon = src, .move = -1, .simulated = simulated, .flag = &bypass};
        pr_apply_ab_attrs(bt, PR_AC_BypassBurnDamageReductionAbAttr, &bp2);
        if (!bypass) burn = 0.5;
    }
    double screen = 1;
    if (!is_critical) {
        int side = def->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY;
        for (int i = 0; i < bt->n_arena_tags; i++) {
            const pr_arena_tag *t = &bt->arena_tags[i];
            if (!pr_arena_tag_is(t->type, PR_ARENACLASS_WEAKEN_MOVE_SCREEN) || !pr_arena_tag_on_side(t, side)) continue;
            if (!pr_screen_tag_weakens(t->type, category)) continue;
            bool bypassed = false;
            pr_ab_params ip = {.pokemon = src, .move = -1, .flag = &bypassed};
            pr_apply_ab_attrs(bt, PR_AC_InfiltratorAbAttr, &ip);
            if (bypassed) continue;
            screen = bt->double_battle ? 2.0 / 3 : 1.0 / 2;
        }
    }
    double hits_tag = pr_hits_tag_multiplier(bt, def, move);

    double dmg = to_dmg_value(base * target_mult * multi_strike * weather_mult * glaive * crit_mult
                              * random_mult * stab * type_multiplier * burn * screen * hits_tag);

    pr_ab_params dp = {.pokemon = src, .opponent = def, .move = move, .simulated = simulated, .value = &dmg};
    pr_apply_ab_attrs(bt, PR_AC_MoveDamageBoostAbAttr, &dp);
    if (!src->is_player) pr_apply_enemy_damage_booster_modifiers(bt, &dmg);
    if (!def->is_player) pr_apply_enemy_damage_reducer_modifiers(bt, &dmg);
    pr_ab_params rp = {.pokemon = def, .opponent = src, .move = move, .simulated = simulated, .value = &dmg};
    if (!ignore_ability) {
        pr_apply_ab_attrs(bt, PR_AC_ReceivedMoveDamageMultiplierAbAttr, &rp);
        if (bt->double_battle) pr_note_unported(bt, PR_AC_AlliedFieldDamageReductionAbAttr);
    }
    pr_apply_modified_damage(bt, src, def, move, &dmg);
    if (full_hp && !ignore_ability) pr_apply_ab_attrs(bt, PR_AC_PreDefendFullHpEndureAbAttr, &rp);

    if (type_multiplier >= 4) r.result = HIT_RESULT_EXTREMELY_EFFECTIVE;
    else if (type_multiplier >= 2) r.result = HIT_RESULT_SUPER_EFFECTIVE;
    else if (type_multiplier <= 0.25) r.result = HIT_RESULT_MOSTLY_INEFFECTIVE;
    else if (type_multiplier <= 0.5) r.result = HIT_RESULT_NOT_VERY_EFFECTIVE;
    r.damage = dmg;
    return r;
}
