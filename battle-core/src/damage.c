/* Ports of the damage path: Pokemon.getAttackDamage / getBaseDamage /
 * getMoveEffectiveness / getAttackTypeEffectiveness / calculateStabMultiplier /
 * getCriticalHitResult / getMoveType and Move.calculateBattlePower.
 * Every applyAbAttrs / applyMoveAttrs / modifier / tag call site is kept in upstream order.
 * Battler tags, arena tags and held items are not modelled yet: the code states where
 * they would apply, and the difftest only generates states without them. */
#include "pr/battle.h"
#include <math.h>

static double type_mult(int atk, int def)
{
    if (atk == POKEMON_TYPE_UNKNOWN || def == POKEMON_TYPE_UNKNOWN) return 1;
    if (atk < 0 || atk >= PR_TYPE_COUNT || def < 0 || def >= PR_TYPE_COUNT) return 1;
    return pr_type_chart_x2[atk][def] / 2.0;
}

/* getAlliesGenerator(): active Pokemon on p's side in speed order. Only singles is
 * modelled (the side is p alone); speed ordering for doubles is not ported. */
static const pr_pokemon *allies_singles(pr_battle *bt, const pr_pokemon *p)
{
    if (bt->double_battle) pr_note_unported(bt, PR_AC_MoveAttr);
    return p;
}

int pr_get_move_type(pr_battle *bt, const pr_pokemon *p, int move)
{
    int t = pr_moves[move].type;
    pr_hook_move(bt, (uint16_t)move, PR_AC_VariableMoveTypeAttr);
    if (!pr_move_has_attr((uint16_t)move, PR_AC_CallMoveAttr)) {
        pr_ab_params pp = {.pokemon = p, .opponent = p, .move = move, .simulated = true, .holder = &t};
        pr_apply_ab_attrs(bt, PR_AC_MoveTypeChangeAbAttr, &pp);
    }
    if (p->terastallized && (move == MOVE_ID_TERA_BLAST || (move == MOVE_ID_TERA_STARSTORM && t == POKEMON_TYPE_STELLAR)))
        return t;
    /* ION_DELUGE arena tag and ELECTRIFIED battler tag: not modelled (absent) */
    return t;
}

double pr_get_attack_type_effectiveness(pr_battle *bt, const pr_pokemon *def, int move_type,
                                        const pr_pokemon *source, int move)
{
    if (move_type == POKEMON_TYPE_STELLAR) return def->terastallized ? 2 : 1;
    int8_t types[3];
    int n = pr_get_types(def, types, PR_TYPES_ORIGINAL_IF_STELLAR);

    /* isForciblyUngrounded(): not forcibly grounded (IGNORE_FLYING tag / Gravity absent),
     * no FLOATING/TELEKINESIS tags, then UngroundedAbAttr (Levitate). */
    if (move_type == POKEMON_TYPE_GROUND) {
        bool ungrounded = false;
        pr_ab_params pp = {.pokemon = def, .move = -1, .flag = &ungrounded};
        pr_apply_ab_attrs(bt, PR_AC_UngroundedAbAttr, &pp);
        if (ungrounded && !(move >= 0 && pr_move_has_attr((uint16_t)move, PR_AC_NeutralDamageAgainstFlyingTypeAttr)))
            return 0;
    }

    double multi = 1;
    for (int i = 0; i < n; i++) {
        double tm = type_mult(move_type, types[i]);
        /* source?.isActive(true): on the field and not fainted */
        if (source && source->hp > 0 && tm == 0) {
            /* checkIgnoreTypeImmunity: forcibly grounded Flying and IGNORE_DARK/IGNORE_GHOST
             * tags are not modelled; then IgnoreTypeImmunityAbAttr (Scrappy, Mind's Eye). */
            bool ignore = false;
            pr_ab_params pp = {.pokemon = source, .move = -1, .simulated = true, .flag = &ignore,
                               .move_type = move_type, .defender_type = types[i]};
            pr_apply_ab_attrs(bt, PR_AC_IgnoreTypeImmunityAbAttr, &pp);
            if (ignore) continue;
        }
        multi *= tm;
    }
    if (move >= 0) pr_hook_move(bt, (uint16_t)move, PR_AC_MoveTypeChartOverrideAttr);
    if (bt->weather == WEATHER_TYPE_STRONG_WINDS && !pr_weather_effect_suppressed(bt)
        && pr_is_of_type(def, POKEMON_TYPE_FLYING) && type_mult(move_type, POKEMON_TYPE_FLYING) == 2)
        multi /= 2;
    return multi;
}

double pr_get_move_effectiveness_ex(pr_battle *bt, const pr_pokemon *def, const pr_pokemon *src, int move,
                                    bool simulated, bool *cancelled_out)
{
    if (def->turn_move_effectiveness >= 0) return def->turn_move_effectiveness;
    const pr_move *m = &pr_moves[move];
    if (pr_move_has_attr((uint16_t)move, PR_AC_TypelessAttr)) return 1;
    int move_type = pr_get_move_type(bt, src, move);
    double tm = (m->category != MOVE_CATEGORY_STATUS || pr_move_has_attr((uint16_t)move, PR_AC_RespectAttackTypeImmunityAttr))
                    ? pr_get_attack_type_effectiveness(bt, def, move_type, src, move)
                    : 1;
    /* move.isTypeImmune: powder moves vs Grass, Prankster status moves vs Dark */
    if (m->target != MOVE_TARGET_USER) {
        bool immune = false;
        int8_t dt[3];
        int dn = pr_get_types(def, dt, PR_TYPES_ORIGINAL_IF_STELLAR);
        for (int i = 0; i < dn; i++) {
            if (dt[i] == POKEMON_TYPE_GRASS && (m->flags & MOVE_FLAGS_POWDER_MOVE)) immune = true;
            if (dt[i] == POKEMON_TYPE_DARK && pr_has_ability_active(bt, src, ABILITY_ID_PRANKSTER)
                && m->category == MOVE_CATEGORY_STATUS && pr_is_opponent(bt, src, def))
                immune = true;
        }
        if (immune) tm = 0;
    }
    /* Tar Shot tag: not modelled (absent) */

    bool cancelled = false;
    pr_ab_params pp = {.pokemon = def, .opponent = src, .move = move, .simulated = simulated,
                       .flag = &cancelled, .value = &tm};
    pr_apply_ab_attrs(bt, PR_AC_TypeImmunityAbAttr, &pp);
    if (!cancelled) pr_apply_ab_attrs(bt, PR_AC_MoveImmunityAbAttr, &pp);
    if (simulated && !cancelled) {
        pr_ab_params fp = {.pokemon = allies_singles(bt, def), .opponent = src, .move = move,
                           .simulated = simulated, .flag = &cancelled};
        pr_apply_ab_attrs(bt, PR_AC_FieldPriorityMoveImmunityAbAttr, &fp);
    }
    if (m->category != MOVE_CATEGORY_STATUS) pr_apply_ab_attrs(bt, PR_AC_FullHpResistTypeAbAttr, &pp);
    /* status moves: hitsSubstitute (substitute tag not modelled) */
    if (cancelled_out) *cancelled_out = cancelled;
    return cancelled ? 0 : tm;
}

double pr_get_move_effectiveness(pr_battle *bt, const pr_pokemon *def, const pr_pokemon *src, int move)
{
    return pr_get_move_effectiveness_ex(bt, def, src, move, true, NULL);
}

double pr_calculate_battle_power(pr_battle *bt, const pr_pokemon *src, const pr_pokemon *def, int move)
{
    const pr_move *m = &pr_moves[move];
    if (m->category == MOVE_CATEGORY_STATUS) return -1;
    double power = m->power;
    pr_apply_variable_power(bt, src, def, move, &power);

    int type_change = m->type;
    pr_ab_params tp = {.pokemon = src, .opponent = def, .move = move, .simulated = true, .holder = &type_change};
    pr_apply_ab_attrs(bt, PR_AC_MoveTypeChangeAbAttr, &tp);

    pr_ab_params pp = {.pokemon = src, .opponent = def, .move = move, .simulated = true, .value = &power};
    pr_apply_ab_attrs(bt, PR_AC_VariableMovePowerAbAttr, &pp);
    const pr_pokemon *ally = pr_get_ally(bt, src);
    if (ally) {
        pr_ab_params ap = pp;
        ap.pokemon = ally;
        pr_apply_ab_attrs(bt, PR_AC_AllyMoveCategoryPowerBoostAbAttr, &ap);
    }

    if (src->terastallized && pr_get_tera_type(src) == m->type && power < 60 && m->priority <= 0
        && !pr_move_has_attr((uint16_t)move, PR_AC_MultiHitAttr))
        power = 60; /* the Multi Lens check needs held items: none are modelled */

    /* Field auras: a Set of FieldMoveTypePowerBoostAbAttr instances across the active field,
     * filtered by getCondition(); each distinct attr applies once with the attacker's params. */
    const pr_attr *auras[8];
    int n_auras = 0;
    for (int i = 0; i < 4; i++) {
        const pr_pokemon *p = bt->field[i];
        if (!p || p->hp <= 0) continue;
        uint16_t abs[2] = {p->ability, p->passive};
        for (int k = 0; k < 2; k++) {
            if (k == 1 && !p->passive) continue;
            const pr_ability *ab = &pr_abilities[abs[k]];
            for (unsigned j = 0; j < ab->nattr; j++) {
                const pr_attr *a = &pr_attrs[ab->attr0 + j];
                if (!pr_attr_is(a->cls, PR_AC_FieldMoveTypePowerBoostAbAttr)) continue;
                if (!pr_can_apply_ability(bt, p, abs[k])) continue;
                if (a->cond != PR_NO_FN && !pr_closure_call(bt, a->cond, a->cls, 0, p, NULL, -1)) continue;
                bool dup = false;
                for (int q = 0; q < n_auras; q++) if (auras[q] == a) dup = true;
                if (!dup && n_auras < 8) auras[n_auras++] = a;
            }
        }
    }
    for (int q = 0; q < n_auras; q++) {
        const pr_attr_FieldMoveTypePowerBoostAbAttr *x = auras[q]->p;
        if (pr_closure_call(bt, x->condition, auras[q]->cls, 0, src, def, move)) power *= x->powerMultiplier;
    }

    pr_ab_params up = {.pokemon = allies_singles(bt, src), .opponent = def, .move = move, .simulated = true,
                       .value = &power};
    pr_apply_ab_attrs(bt, PR_AC_UserFieldMoveTypePowerBoostAbAttr, &up);

    /* TypeBoostTag (Flash Fire), WeakenMoveTypeTag, AttackTypeBoosterModifier, Helping Hand,
     * Supreme Overlord: tags and items are not modelled (absent).
     * Terrain power: getTerrainPowerMultiplier is 1 with no terrain. */
    if (bt->terrain) pr_note_unported(bt, PR_AC_MoveAttr);
    return power;
}

double pr_get_base_damage(pr_battle *bt, const pr_pokemon *def, const pr_pokemon *src, int move,
                          int category, bool is_critical)
{
    bool phys = category == MOVE_CATEGORY_PHYSICAL;
    double level_mult = (2.0 * src->level) / 5 + 2;
    double power = pr_calculate_battle_power(bt, src, def, move);
    double atk = pr_get_effective_stat(bt, src, phys ? STAT_ATK : STAT_SPATK, def, -1, is_critical, false);
    pr_hook_move(bt, (uint16_t)move, PR_AC_VariableAtkAttr);
    double dfn = pr_get_effective_stat(bt, def, phys ? STAT_DEF : STAT_SPDEF, src, move, is_critical, true);
    pr_hook_move(bt, (uint16_t)move, PR_AC_VariableDefAttr);
    return (level_mult * power * atk) / dfn / 50 + 2;
}

double pr_calculate_stab_multiplier(pr_battle *bt, const pr_pokemon *src, int move)
{
    if (pr_move_has_attr((uint16_t)move, PR_AC_TypelessAttr)) return 1;
    int8_t types[3];
    int n = pr_get_types(src, types, PR_TYPES_NO_TERA);
    int move_type = pr_get_move_type(bt, src, move);
    bool match = false;
    /* upstream calls getMoveType a second time for the includes() check */
    int move_type2 = pr_get_move_type(bt, src, move);
    for (int i = 0; i < n; i++) if (types[i] == move_type2) match = true;
    double stab = 1;
    if (match && move_type != POKEMON_TYPE_STELLAR) stab += 0.5;
    pr_hook_move(bt, (uint16_t)move, PR_AC_CombinedPledgeStabBoostAttr);
    pr_ab_params pp = {.pokemon = src, .move = move, .simulated = true, .value = &stab};
    pr_apply_ab_attrs(bt, PR_AC_StabBoostAbAttr, &pp);
    if (src->terastallized) {
        int tera = pr_get_tera_type(src);
        if (tera != POKEMON_TYPE_STELLAR) stab += tera == move_type ? 0.5 : 0;
        else pr_note_unported(bt, PR_AC_MoveAttr); /* stellar once-per-type bookkeeping */
    }
    return stab < 2.25 ? stab : 2.25;
}

static int32_t to_dmg_value(double v)
{
    v = floor(v);
    return v > 1 ? (int32_t)v : 1;
}

pr_damage_result pr_get_attack_damage(pr_battle *bt, pr_pokemon *def, pr_pokemon *src, int move,
                                      bool is_critical, bool simulated)
{
    pr_damage_result r = {false, HIT_RESULT_EFFECTIVE, 0};
    const pr_move *m = &pr_moves[move];
    int category = m->category;
    pr_hook_move(bt, (uint16_t)move, PR_AC_VariableMoveCategoryAttr);

    bool cancelled = false;
    double type_multiplier = pr_get_move_effectiveness_ex(bt, def, src, move, simulated, &cancelled);
    if (cancelled || type_multiplier == 0) {
        r.cancelled = cancelled;
        r.result = move == MOVE_ID_SHEER_COLD ? HIT_RESULT_IMMUNE : HIT_RESULT_NO_EFFECT;
        return r;
    }
    if (pr_hook_move(bt, (uint16_t)move, PR_AC_FixedDamageAttr)) return r;
    if (pr_hook_move(bt, (uint16_t)move, PR_AC_OneHitKOAttr)) return r;

    double base = pr_get_base_damage(bt, def, src, move, category, is_critical);

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

    /* getMoveTargets: spread moves in doubles hit for 0.75. Singles only for now. */
    double target_mult = 1;
    if (bt->double_battle) pr_note_unported(bt, PR_AC_MoveAttr);

    double multi_strike = 1;  /* PokemonMultiHitModifier: held items not modelled */
    double glaive = 1;        /* RECEIVE_DOUBLE_DAMAGE tag: not modelled */
    double crit_mult = is_critical ? 1.5 : 1;
    pr_ab_params cp = {.pokemon = src, .move = -1, .simulated = simulated, .value = &crit_mult};
    pr_apply_ab_attrs(bt, PR_AC_MultCritAbAttr, &cp);
    double random_mult = simulated ? 1 : pr_battle_rand_seed_int(bt, 100 - 85 + 1, 85) / 100.0;
    double stab = pr_calculate_stab_multiplier(bt, src, move);

    double burn = 1;
    if (category == MOVE_CATEGORY_PHYSICAL && src->status == STATUS_EFFECT_BURN
        && !pr_move_has_attr((uint16_t)move, PR_AC_BypassBurnDamageReductionAttr)) {
        bool bypass = false;
        pr_ab_params bp = {.pokemon = src, .move = -1, .simulated = simulated, .flag = &bypass};
        pr_apply_ab_attrs(bt, PR_AC_BypassBurnDamageReductionAbAttr, &bp);
        if (!bypass) burn = 0.5;
    }
    double screen = 1;        /* WeakenMoveScreenTag: arena tags not modelled (absent) */
    double hits_tag = 1;      /* HitsTagAttr doubling needs battler tags: none are modelled */
    pr_hook_move(bt, (uint16_t)move, PR_AC_HitsTagAttr);

    double dmg = to_dmg_value(base * target_mult * multi_strike * weather_mult * glaive * crit_mult
                              * random_mult * stab * type_multiplier * burn * screen * hits_tag);

    pr_ab_params dp = {.pokemon = src, .opponent = def, .move = move, .simulated = simulated, .value = &dmg};
    pr_apply_ab_attrs(bt, PR_AC_MoveDamageBoostAbAttr, &dp);
    /* Enemy damage/resistance tokens: modifiers not modelled */
    pr_ab_params rp = {.pokemon = def, .opponent = src, .move = move, .simulated = simulated, .value = &dmg};
    pr_apply_ab_attrs(bt, PR_AC_ReceivedMoveDamageMultiplierAbAttr, &rp);
    if (bt->double_battle) pr_note_unported(bt, PR_AC_AlliedFieldDamageReductionAbAttr);
    pr_hook_move(bt, (uint16_t)move, PR_AC_ModifiedDamageAttr);
    if (def->hp >= pr_get_stat(def, STAT_HP, true)) pr_apply_ab_attrs(bt, PR_AC_PreDefendFullHpEndureAbAttr, &rp);

    if (type_multiplier >= 4) r.result = HIT_RESULT_EXTREMELY_EFFECTIVE;
    else if (type_multiplier >= 2) r.result = HIT_RESULT_SUPER_EFFECTIVE;
    else if (type_multiplier <= 0.25) r.result = HIT_RESULT_MOSTLY_INEFFECTIVE;
    else if (type_multiplier <= 0.5) r.result = HIT_RESULT_NOT_VERY_EFFECTIVE;
    r.damage = (int32_t)dmg;
    return r;
}

bool pr_get_critical_hit_result(pr_battle *bt, const pr_pokemon *def, const pr_pokemon *src, int move)
{
    if (pr_move_has_attr((uint16_t)move, PR_AC_FixedDamageAttr)) return false;
    bool always = pr_move_has_attr((uint16_t)move, PR_AC_CritOnlyAttr);
    pr_ab_params cp = {.pokemon = src, .opponent = def, .move = move, .flag = &always};
    pr_apply_ab_attrs(bt, PR_AC_ConditionalCritAbAttr, &cp);
    /* ALWAYS_CRIT tag: not modelled */

    /* getCritStage: HighCritAttr (+1 each), crit items (not modelled), BonusCritAbAttr,
     * Dragon Cheer tag (not modelled) */
    int stage = 0;
    const pr_move *m = &pr_moves[move];
    for (unsigned i = 0; i < m->nattr; i++)
        if (pr_attr_is(pr_attrs[m->attr0 + i].cls, PR_AC_HighCritAttr)) stage++;
    pr_ab_params sp = {.pokemon = src, .move = -1, .holder = &stage};
    pr_apply_ab_attrs(bt, PR_AC_BonusCritAbAttr, &sp);

    static const int chance_tab[4] = {24, 8, 2, 1};
    int chance = chance_tab[stage < 0 ? 0 : stage > 3 ? 3 : stage];
    bool crit = always || chance == 1;
    if (!crit) crit = pr_battle_rand_seed_int(bt, chance, 0) == 0;
    bool block = false;
    pr_ab_params bp = {.pokemon = def, .move = -1, .flag = &block};
    pr_apply_ab_attrs(bt, PR_AC_BlockCritAbAttr, &bp);
    /* Lucky Chant (NoCritTag): arena tags not modelled */
    return crit && !block;
}
