/* Ports of the damage path: Pokemon.getAttackDamage / getBaseDamage /
 * getMoveEffectiveness / getAttackTypeEffectiveness / calculateStabMultiplier /
 * getCriticalHitResult / getMoveType and Move.calculateBattlePower.
 * Every applyAbAttrs / applyMoveAttrs / modifier / tag call site is kept in upstream order. */
#include "pr/battle.h"
#include "pr/engine.h"
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
    if (bt->double_battle) pr_note_unported(bt, PR_FEAT_DOUBLES);
    return p;
}

static int side_of(const pr_pokemon *p)
{
    return p->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY;
}

int pr_get_move_type(pr_battle *bt, const pr_pokemon *p, int move)
{
    int t = pr_moves[move].type;
    pr_apply_variable_move_type(bt, p, move, &t);
    if (!pr_move_has_attr((uint16_t)move, PR_AC_CallMoveAttr)) {
        pr_ab_params pp = {.pokemon = p, .opponent = p, .move = move, .simulated = true, .holder = &t};
        pr_apply_ab_attrs(bt, PR_AC_MoveTypeChangeAbAttr, &pp);
    }
    if (p->terastallized && (move == MOVE_ID_TERA_BLAST || (move == MOVE_ID_TERA_STARSTORM && t == POKEMON_TYPE_STELLAR)))
        return t;
    /* IonDelugeTag: Normal becomes Electric */
    if (pr_arena_has_tag(bt, ARENA_TAG_TYPE_ION_DELUGE, ARENA_TAG_SIDE_BOTH) && t == POKEMON_TYPE_NORMAL)
        t = POKEMON_TYPE_ELECTRIC;
    if (pr_has_battler_tag(bt, p, BATTLER_TAG_TYPE_ELECTRIFIED)) t = POKEMON_TYPE_ELECTRIC;
    return t;
}

double pr_get_attack_type_effectiveness_opts(pr_battle *bt, const pr_pokemon *def, int move_type,
                                             const pr_pokemon *source, int move, bool simulated,
                                             bool ignore_strong_winds)
{
    if (move_type == POKEMON_TYPE_STELLAR) return def->terastallized ? 2 : 1;
    int8_t types[PR_MAX_TYPES];
    int n = pr_get_types(def, types, PR_TYPES_ORIGINAL_IF_STELLAR);

    /* Ground moves (bar Thousand Arrows) miss anything made airborne by something other
     * than its typing or semi-invulnerability (Levitate, Magnet Rise, Telekinesis). */
    if (pr_is_forcibly_ungrounded(bt, def) && move_type == POKEMON_TYPE_GROUND
        && !(move >= 0 && pr_move_has_attr((uint16_t)move, PR_AC_NeutralDamageAgainstFlyingTypeAttr)))
        return 0;

    double multi = 1;
    for (int i = 0; i < n; i++) {
        double tm = type_mult(move_type, types[i]);
        /* source?.isActive(true): on the field and not fainted */
        if (source && source->hp > 0 && source->on_field && !source->switch_out_status && tm == 0) {
            /* checkIgnoreTypeImmunity: grounded Flying types, Foresight/Miracle Eye tags,
             * then IgnoreTypeImmunityAbAttr (Scrappy, Mind's Eye). */
            if (move_type == POKEMON_TYPE_GROUND && types[i] == POKEMON_TYPE_FLYING && pr_is_forcibly_grounded(bt, def))
                continue;
            bool exposed = false; /* findTag(IGNORE_DARK / IGNORE_GHOST tag whose ignoreImmunity() holds) */
            for (int k = 0; k < def->n_tags && !exposed; k++)
                exposed = pr_exposed_tag_ignores_immunity(def->tags[k].type, types[i], move_type);
            if (exposed) continue;
            bool ignore = false;
            pr_ab_params pp = {.pokemon = source, .move = -1, .simulated = simulated, .flag = &ignore,
                               .move_type = move_type, .defender_type = types[i]};
            pr_apply_ab_attrs(bt, PR_AC_IgnoreTypeImmunityAbAttr, &pp);
            if (ignore) continue;
        }
        multi *= tm;
    }
    if (move >= 0) pr_apply_type_chart_override(bt, source, def, move, &multi, types, n, move_type);
    if (!ignore_strong_winds && bt->weather == WEATHER_TYPE_STRONG_WINDS && !pr_weather_effect_suppressed(bt)
        && pr_is_of_type(def, POKEMON_TYPE_FLYING) && type_mult(move_type, POKEMON_TYPE_FLYING) == 2) {
        multi /= 2;
        if (!simulated && bt->engine) pr_queue_message(bt, bt->engine); /* strongWindsEffectMessage */
    }
    return multi;
}

double pr_get_attack_type_effectiveness_ex(pr_battle *bt, const pr_pokemon *def, int move_type,
                                           const pr_pokemon *source, int move, bool simulated)
{
    return pr_get_attack_type_effectiveness_opts(bt, def, move_type, source, move, simulated, false);
}

/* getAttackTypeEffectiveness(moveType, {source, move}): simulated by default */
double pr_get_attack_type_effectiveness(pr_battle *bt, const pr_pokemon *def, int move_type,
                                        const pr_pokemon *source, int move)
{
    return pr_get_attack_type_effectiveness_ex(bt, def, move_type, source, move, true);
}

double pr_get_move_effectiveness_ex(pr_battle *bt, const pr_pokemon *def, const pr_pokemon *src, int move,
                                    bool simulated, bool *cancelled_out)
{
    if (def->turn_move_effectiveness >= 0) return def->turn_move_effectiveness;
    const pr_move *m = &pr_moves[move];
    if (pr_move_has_attr((uint16_t)move, PR_AC_TypelessAttr)) return 1;
    int move_type = pr_get_move_type(bt, src, move);
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
    pr_apply_ab_attrs(bt, PR_AC_TypeImmunityAbAttr, &pp);
    if (!cancelled) pr_apply_ab_attrs(bt, PR_AC_MoveImmunityAbAttr, &pp);
    if (simulated && !cancelled) {
        pr_ab_params fp = {.pokemon = allies_singles(bt, def), .opponent = src, .move = move,
                           .simulated = simulated, .flag = &cancelled};
        pr_apply_ab_attrs(bt, PR_AC_FieldPriorityMoveImmunityAbAttr, &fp);
    }
    if (m->category != MOVE_CATEGORY_STATUS) pr_apply_ab_attrs(bt, PR_AC_FullHpResistTypeAbAttr, &pp);
    if (m->category == MOVE_CATEGORY_STATUS && pr_move_hits_substitute(bt, move, src, def)) tm = 0;
    if (cancelled_out) *cancelled_out = cancelled;
    return cancelled ? 0 : tm;
}

double pr_get_move_effectiveness(pr_battle *bt, const pr_pokemon *def, const pr_pokemon *src, int move)
{
    return pr_get_move_effectiveness_ex(bt, def, src, move, true, NULL);
}

/* arena.getTerrainPowerMultiplier */
static double terrain_power_multiplier(const pr_battle *bt, int attack_type)
{
    if ((bt->terrain == TERRAIN_ELECTRIC && attack_type == POKEMON_TYPE_ELECTRIC)
        || (bt->terrain == TERRAIN_GRASSY && attack_type == POKEMON_TYPE_GRASS)
        || (bt->terrain == TERRAIN_PSYCHIC && attack_type == POKEMON_TYPE_PSYCHIC))
        return 1.3;
    return 1;
}

double pr_calculate_battle_power(pr_battle *bt, const pr_pokemon *src, const pr_pokemon *def, int move,
                                 bool simulated)
{
    const pr_move *m = &pr_moves[move];
    if (m->category == MOVE_CATEGORY_STATUS) return -1;
    double power = m->power;
    pr_apply_variable_power(bt, src, def, move, &power);

    /* The ability type change only (no VariableMoveTypeAttr): Flash Fire, the sports
     * and type-boosting items look at this type. */
    int type_change = m->type;
    pr_ab_params tp = {.pokemon = src, .opponent = def, .move = move, .simulated = true, .holder = &type_change};
    pr_apply_ab_attrs(bt, PR_AC_MoveTypeChangeAbAttr, &tp);

    pr_ab_params pp = {.pokemon = src, .opponent = def, .move = move, .simulated = simulated, .value = &power};
    pr_apply_ab_attrs(bt, PR_AC_VariableMovePowerAbAttr, &pp);
    const pr_pokemon *ally = pr_get_ally(bt, src);
    if (ally) {
        pr_ab_params ap = pp;
        ap.pokemon = ally;
        pr_apply_ab_attrs(bt, PR_AC_AllyMoveCategoryPowerBoostAbAttr, &ap);
    }

    /* Non-priority, single-hit moves of the user's Tera type hit with at least 60 power
     * (a Multi Lens makes them multi-hit) */
    if (src->terastallized && pr_get_tera_type(src) == m->type && power < 60 && m->priority <= 0
        && !pr_move_has_attr((uint16_t)move, PR_AC_MultiHitAttr)
        && !pr_find_player_multi_lens(bt, src)) /* findModifier searches the player's list only */
        power = 60;

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

    pr_ab_params up = {.pokemon = allies_singles(bt, src), .opponent = def, .move = move, .simulated = simulated,
                       .value = &power};
    pr_apply_ab_attrs(bt, PR_AC_UserFieldMoveTypePowerBoostAbAttr, &up);

    /* The first TypeBoostTag (Flash Fire, Charge) for the ability-changed type */
    for (int i = 0; i < src->n_tags; i++) {
        int boosted;
        double boost;
        if (pr_battler_tag_is(src->tags[i].type, PR_TAGCLASS_TYPE_BOOST)
            && pr_type_boost_tag_params(src->tags[i].type, &boosted, &boost) && boosted == type_change) {
            power *= boost;
            break;
        }
    }
    if (!pr_move_has_attr((uint16_t)move, PR_AC_TypelessAttr)) {
        /* arena.applyTags(WeakenMoveTypeTag, type, power): every Mud Sport / Water Sport tag, either side */
        for (int i = 0; i < bt->n_arena_tags; i++) {
            const pr_arena_tag *t = &bt->arena_tags[i];
            if (pr_arena_tag_is(t->type, PR_ARENACLASS_WEAKEN_MOVE_TYPE) && pr_weaken_move_type_tag_type(t->type) == type_change)
                power *= 0.33;
        }
        pr_apply_attack_type_booster_modifiers(bt, src, type_change, &power);
    }
    if (pr_has_battler_tag_class(bt, src, PR_TAGCLASS_HELPING_HAND)) power *= 1.5;
    /* SupremeOverlordTag.getBoost(): 1 + 0.1 * faintCount */
    const pr_battler_tag *overlord = pr_get_battler_tag(src, BATTLER_TAG_TYPE_SUPREME_OVERLORD);
    if (overlord) power *= 1 + 0.1 * overlord->faint_count;

    if (pr_is_grounded(bt, src, false)) power *= terrain_power_multiplier(bt, pr_get_move_type(bt, src, move));
    if (pr_is_grounded(bt, def, false) && bt->terrain == TERRAIN_MISTY && type_change == POKEMON_TYPE_DRAGON)
        power *= 0.5;
    return power;
}

double pr_get_base_damage(pr_battle *bt, const pr_pokemon *def, const pr_pokemon *src, const pr_base_damage_params *bp)
{
    bool phys = bp->category == MOVE_CATEGORY_PHYSICAL;
    double level_mult = (2.0 * src->level) / 5 + 2;
    double power = pr_calculate_battle_power(bt, src, def, bp->move, bp->simulated);
    /* upstream passes ignoreSourceAbility as the attacker's ignoreOppAbility, and no move */
    pr_stat_params ap = {.opponent = def, .move = -1, .ignore_opp_ability = bp->ignore_source_ability,
                         .ignore_ability = bp->ignore_ability, .ignore_ally_ability = bp->ignore_ally_ability,
                         .is_critical = bp->is_critical, .simulated = bp->simulated};
    double atk = pr_get_effective_stat_p(bt, src, phys ? STAT_ATK : STAT_SPATK, &ap);
    pr_apply_variable_atk(bt, src, def, bp->move, &atk);
    pr_stat_params dp = {.opponent = src, .move = bp->move, .ignore_ability = bp->ignore_ability,
                         .ignore_opp_ability = bp->ignore_source_ability,
                         .ignore_ally_ability = bp->ignore_source_ally_ability, .is_critical = bp->is_critical,
                         .simulated = bp->simulated, .for_defend = true};
    double dfn = pr_get_effective_stat_p(bt, def, phys ? STAT_DEF : STAT_SPDEF, &dp);
    pr_apply_variable_def(bt, src, def, bp->move, &dfn);
    return (level_mult * power * atk) / dfn / 50 + 2;
}

double pr_calculate_stab_multiplier(pr_battle *bt, const pr_pokemon *src, int move, bool ignore_source_ability,
                                    bool simulated)
{
    if (pr_move_has_attr((uint16_t)move, PR_AC_TypelessAttr)) return 1;
    int8_t types[PR_MAX_TYPES];
    int n = pr_get_types(src, types, PR_TYPES_NO_TERA);
    int tera = pr_get_tera_type(src);
    int move_type = pr_get_move_type(bt, src, move);
    bool match = false;
    /* upstream calls getMoveType a second time for the includes() check */
    int move_type2 = pr_get_move_type(bt, src, move);
    for (int i = 0; i < n; i++) if (types[i] == move_type2) match = true;
    double stab = 1;
    if (match && move_type != POKEMON_TYPE_STELLAR) stab += 0.5;
    pr_apply_pledge_stab(src, move, &stab);
    if (!ignore_source_ability) {
        pr_ab_params pp = {.pokemon = src, .move = -1, .simulated = simulated, .value = &stab};
        pr_apply_ab_attrs(bt, PR_AC_StabBoostAbAttr, &pp);
    }
    if (src->terastallized) {
        /* getTeraTypeBoost: a Stellar tera boosts each move type once (Terapagos always) */
        if (tera != POKEMON_TYPE_STELLAR) {
            stab += tera == move_type ? 0.5 : 0;
        } else {
            bool used = move_type >= 0 && move_type < 32 && ((src->stellar_types_boosted >> move_type) & 1u);
            if (!used || src->species == SPECIES_ID_TERAPAGOS) stab += match ? 0.5 : 0.2;
        }
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
    return pr_get_attack_damage_ex(bt, def, src, move, is_critical, simulated, NAN);
}

pr_damage_result pr_get_attack_damage_ex(pr_battle *bt, pr_pokemon *def, pr_pokemon *src, int move,
                                         bool is_critical, bool simulated, double effectiveness)
{
    pr_damage_result r = {false, HIT_RESULT_EFFECTIVE, 0};
    int category = pr_apply_variable_move_category(bt, src, def, move, pr_moves[move].category);

    bool cancelled = false;
    /* `effectiveness ?? getMoveEffectiveness(...)`: the caller's value skips the calculation */
    double type_multiplier = effectiveness == effectiveness
                                 ? effectiveness
                                 : pr_get_move_effectiveness_ex(bt, def, src, move, simulated, &cancelled);
    if (cancelled || type_multiplier == 0) {
        r.cancelled = cancelled;
        r.result = move == MOVE_ID_SHEER_COLD ? HIT_RESULT_IMMUNE : HIT_RESULT_NO_EFFECT;
        return r;
    }

    double fixed = pr_apply_fixed_damage(bt, src, def, move);
    if (fixed != 0 && fixed == fixed) { /* JS truthiness: NaN falls through to the normal calculation */
        double lens = 1;
        pr_apply_multi_hit_damage_modifiers(bt, src, move, &lens);
        fixed = to_dmg_value(fixed * lens);
        /* the early return skips the rest, so Sturdy-like abilities get their chance here */
        if (def->hp >= pr_get_stat(def, STAT_HP, true)) {
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

    pr_base_damage_params bp = {.move = move, .category = category, .is_critical = is_critical, .simulated = simulated};
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

    /* getMoveTargets: spread moves in doubles hit for 0.75. Singles only for now. */
    double target_mult = 1;
    if (bt->double_battle) pr_note_unported(bt, PR_FEAT_DOUBLES);

    double multi_strike = 1; /* Multi Lens (Parental Bond's own hits aren't ported) */
    pr_apply_multi_hit_damage_modifiers(bt, src, move, &multi_strike);
    double glaive = pr_has_battler_tag(bt, def, BATTLER_TAG_TYPE_RECEIVE_DOUBLE_DAMAGE) ? 2 : 1;
    double crit_mult = is_critical ? 1.5 : 1;
    pr_ab_params cp = {.pokemon = src, .move = -1, .simulated = simulated, .value = &crit_mult};
    pr_apply_ab_attrs(bt, PR_AC_MultCritAbAttr, &cp);
    double random_mult = simulated ? 1 : pr_battle_rand_seed_int(bt, 100 - 85 + 1, 85) / 100.0;
    double stab = pr_calculate_stab_multiplier(bt, src, move, false, simulated);

    double burn = 1;
    if (category == MOVE_CATEGORY_PHYSICAL && src->status == STATUS_EFFECT_BURN
        && !pr_move_has_attr((uint16_t)move, PR_AC_BypassBurnDamageReductionAttr)) {
        bool bypass = false;
        pr_ab_params bp2 = {.pokemon = src, .move = -1, .simulated = simulated, .flag = &bypass};
        pr_apply_ab_attrs(bt, PR_AC_BypassBurnDamageReductionAbAttr, &bp2);
        if (!bypass) burn = 0.5;
    }
    /* arena.applyTagsForSide(WeakenMoveScreenTag, defendingSide, source, moveCategory, screenMultiplier):
     * Reflect, Light Screen, Aurora Veil. Crits go through screens, and so does an Infiltrator. */
    double screen = 1;
    if (!is_critical) {
        int side = side_of(def);
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
    /* the enemy's Damage and Resistance tokens */
    if (!src->is_player) pr_apply_enemy_damage_booster_modifiers(bt, &dmg);
    if (!def->is_player) pr_apply_enemy_damage_reducer_modifiers(bt, &dmg);
    pr_ab_params rp = {.pokemon = def, .opponent = src, .move = move, .simulated = simulated, .value = &dmg};
    pr_apply_ab_attrs(bt, PR_AC_ReceivedMoveDamageMultiplierAbAttr, &rp);
    if (bt->double_battle) pr_note_unported(bt, PR_AC_AlliedFieldDamageReductionAbAttr);
    pr_apply_modified_damage(bt, src, def, move, &dmg);
    if (def->hp >= pr_get_stat(def, STAT_HP, true)) pr_apply_ab_attrs(bt, PR_AC_PreDefendFullHpEndureAbAttr, &rp);

    if (type_multiplier >= 4) r.result = HIT_RESULT_EXTREMELY_EFFECTIVE;
    else if (type_multiplier >= 2) r.result = HIT_RESULT_SUPER_EFFECTIVE;
    else if (type_multiplier <= 0.25) r.result = HIT_RESULT_MOSTLY_INEFFECTIVE;
    else if (type_multiplier <= 0.5) r.result = HIT_RESULT_NOT_VERY_EFFECTIVE;
    r.damage = dmg;
    return r;
}

bool pr_get_critical_hit_result(pr_battle *bt, const pr_pokemon *def, const pr_pokemon *src, int move)
{
    if (pr_move_has_attr((uint16_t)move, PR_AC_FixedDamageAttr)) return false;
    bool always = pr_move_has_attr((uint16_t)move, PR_AC_CritOnlyAttr);
    pr_ab_params cp = {.pokemon = src, .opponent = def, .move = move, .flag = &always};
    pr_apply_ab_attrs(bt, PR_AC_ConditionalCritAbAttr, &cp);
    bool always_tag = pr_has_battler_tag(bt, src, BATTLER_TAG_TYPE_ALWAYS_CRIT);

    /* getCritStage: HighCritAttr (+1 each), crit items, BonusCritAbAttr, Dragon Cheer tag */
    int stage = 0;
    const pr_move *m = &pr_moves[move];
    for (unsigned i = 0; i < m->nattr; i++)
        if (pr_attr_is(pr_attrs[m->attr0 + i].cls, PR_AC_HighCritAttr)) stage++;
    pr_apply_crit_booster_modifiers(bt, src, &stage);           /* Scope Lens, Leek */
    pr_apply_temp_crit_booster_modifiers(bt, src->is_player, &stage); /* Dire Hit */
    pr_ab_params sp = {.pokemon = src, .move = -1, .holder = &stage};
    pr_apply_ab_attrs(bt, PR_AC_BonusCritAbAttr, &sp);
    const pr_battler_tag *crit_boost = pr_find_battler_tag_class(src, PR_TAGCLASS_CRIT_BOOST); /* Focus Energy, Dragon Cheer */
    if (crit_boost) stage += crit_boost->crit_stages;

    static const int chance_tab[4] = {24, 8, 2, 1};
    int chance = chance_tab[stage < 0 ? 0 : stage > 3 ? 3 : stage];
    bool crit = always || always_tag || chance == 1;
    if (!crit) crit = pr_battle_rand_seed_int(bt, chance, 0) == 0;
    bool block = false;
    pr_ab_params bp = {.pokemon = def, .move = -1, .flag = &block};
    pr_apply_ab_attrs(bt, PR_AC_BlockCritAbAttr, &bp);
    /* arena.applyTagsForSide(NoCritTag, defender's side, blockCrit): Lucky Chant */
    if (pr_arena_has_tag_class(bt, PR_ARENACLASS_NO_CRIT, side_of(def))) block = true;
    return crit && !block;
}
