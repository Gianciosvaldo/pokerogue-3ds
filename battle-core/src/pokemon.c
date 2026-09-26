/* Ports of src/field/pokemon.ts: stats, types, stat stages, effective stats. */
#include "pr/battle.h"
#include <math.h>

const pr_species *pr_pokemon_species(const pr_pokemon *p)
{
    /* Species ids are sparse; the table is sorted by id. */
    int lo = 0, hi = PR_SPECIES_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (pr_species_table[mid].id == p->species) return &pr_species_table[mid];
        if (pr_species_table[mid].id < p->species) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

const pr_form *pr_pokemon_form(const pr_pokemon *p)
{
    const pr_species *s = pr_pokemon_species(p);
    if (!s) return &pr_forms[0];
    return &pr_forms[s->form0 + (p->form < s->nforms ? p->form : 0)];
}

/* calculateStats(). Modifiers (vitamins, nature mints...), fusion and challenges
 * are not ported yet; the hooks below flag Wonder Guard. */
void pr_calculate_stats(pr_battle *bt, pr_pokemon *p)
{
    const pr_form *f = pr_pokemon_form(p);
    pr_check_modifiers(bt, p->is_player); /* stat modifiers (vitamins, mints...) */
    for (int s = 0; s < 6; s++) {
        double v = floor((2 * f->base_stats[s] + p->ivs[s]) * p->level * 0.01);
        if (s == STAT_HP) {
            v = v + p->level + 10;
            if (p->base_ability == ABILITY_ID_WONDER_GUARD) v = 1; /* hasAbility(WONDER_GUARD, false, true) */
            p->hp = (int32_t)v; /* fresh Pokemon: hp starts at max */
        } else {
            v += 5;
            double m = pr_nature_mult[p->nature][s];
            if (m != 1) {
                v = (m > 1) ? ceil(v * m) : floor(v * m);
                if (v < 1) v = 1;
            }
        }
        if (v < 1) v = 1;
        p->stats[s] = (int32_t)v;
    }
}

int32_t pr_get_stat(const pr_pokemon *p, int stat, bool bypass_summon)
{
    if (!bypass_summon && p->summon_stats[stat]) return p->summon_stats[stat];
    return p->stats[stat];
}

/* getTeraType(). hasSpecies() also matches the fusion species: fusion not ported yet. */
int pr_get_tera_type(const pr_pokemon *p)
{
    if (p->species == SPECIES_ID_TERAPAGOS) return POKEMON_TYPE_STELLAR;
    if (p->species == SPECIES_ID_OGERPON) {
        static const int8_t t[8] = {POKEMON_TYPE_GRASS, POKEMON_TYPE_WATER, POKEMON_TYPE_FIRE, POKEMON_TYPE_ROCK,
                                    POKEMON_TYPE_GRASS, POKEMON_TYPE_WATER, POKEMON_TYPE_FIRE, POKEMON_TYPE_ROCK};
        if (p->form < 8) return t[p->form];
    }
    if (p->species == SPECIES_ID_SHEDINJA) return POKEMON_TYPE_BUG;
    return p->tera_type;
}

/* getTypes({}) : returns count, out gets up to 3 types. */
int pr_get_types(const pr_pokemon *p, int8_t out[3], unsigned flags)
{
    /* A Stellar tera reports [STELLAR] unless the caller asks for the original types. */
    int tera = pr_get_tera_type(p);
    bool use_tera = !((flags & PR_TYPES_ORIGINAL_IF_STELLAR) && tera == POKEMON_TYPE_STELLAR);
    if (!(flags & PR_TYPES_NO_TERA) && p->terastallized && use_tera) {
        out[0] = (int8_t)tera;
        return 1;
    }
    int8_t t1, t2;
    if (p->type_override[0] != -2) {
        t1 = p->type_override[0];
        t2 = p->type_override[1];
    } else {
        const pr_form *f = pr_pokemon_form(p);
        t1 = f->type1;
        t2 = f->type2 < 0 ? POKEMON_TYPE_UNKNOWN : f->type2;
    }
    /* new Set([t1, t2]) then drop UNKNOWN when something else is present */
    int n = 0;
    out[n++] = t1;
    if (t2 != t1) out[n++] = t2;
    if (n > 1) {
        int k = 0;
        for (int i = 0; i < n; i++)
            if (out[i] != POKEMON_TYPE_UNKNOWN) out[k++] = out[i];
        n = k;
    }
    return n;
}

bool pr_is_of_type(const pr_pokemon *p, int type)
{
    int8_t t[3];
    int n = pr_get_types(p, t, 0);
    for (int i = 0; i < n; i++)
        if (t[i] == type) return true;
    return false;
}

double pr_get_stat_stage_multiplier(pr_battle *bt, const pr_pokemon *p, int stat, const pr_pokemon *opponent,
                                    int move, bool ignore_opp_ability, bool is_critical, bool simulated,
                                    bool ignore_held_items)
{
    int stage = p->stat_stages[stat - 1];
    bool ignored = false;
    if (opponent) {
        if (is_critical) {
            if ((stat == STAT_ATK || stat == STAT_SPATK) && stage < 0) stage = 0;
            if ((stat == STAT_DEF || stat == STAT_SPDEF) && stage > 0) stage = 0;
        }
        if (!ignore_opp_ability) {
            pr_ab_params pp = {.pokemon = opponent, .move = -1, .simulated = simulated, .stat = stat, .flag = &ignored};
            pr_apply_ab_attrs(bt, PR_AC_IgnoreOpponentStatStagesAbAttr, &pp);
        }
        /* IgnoreOpponentStatStagesAttr (Chip Away, Sacred Sword...) sets the flag */
        if (move >= 0 && pr_move_has_attr((uint16_t)move, PR_AC_IgnoreOpponentStatStagesAttr)) ignored = true;
    }
    if (ignored) return 1;
    double m = (double)(2 + stage > 2 ? 2 + stage : 2) / (double)(2 - stage > 2 ? 2 - stage : 2);
    if (!ignore_held_items) pr_check_modifiers(bt, p->is_player); /* TempStatStageBoosterModifier (X items) */
    return m < 4 ? m : 4;
}

int32_t pr_get_effective_stat_p(pr_battle *bt, const pr_pokemon *p, int stat, const pr_stat_params *sp)
{
    double v = pr_get_stat(p, stat, false);
    if (!sp->ignore_held_items) pr_check_modifiers(bt, p->is_player); /* StatBoosterModifier */

    /* Ruin abilities: getField(true) in field order, stop once one applied. Never ignored. */
    bool field_applied = false;
    for (int i = 0; i < 4 && !field_applied; i++) {
        const pr_pokemon *f = bt->field[i];
        if (!f || f->hp <= 0) continue;
        pr_ab_params fp = {.pokemon = f, .opponent = p, .move = -1, .simulated = sp->simulated, .stat = stat,
                           .value = &v, .flag = &field_applied};
        pr_apply_ab_attrs(bt, PR_AC_FieldMultiplyStatAbAttr, &fp);
    }

    /* move ?? allMoves[MoveId.NONE] */
    int m = sp->move >= 0 ? sp->move : MOVE_ID_NONE;
    pr_ab_params ap = {.pokemon = p, .move = m, .simulated = sp->simulated, .stat = stat, .value = &v};
    if (!sp->ignore_ability) pr_apply_ab_attrs(bt, PR_AC_StatMultiplierAbAttr, &ap);

    const pr_pokemon *ally = pr_get_ally(bt, p);
    if (ally) {
        pr_ab_params lp = ap;
        lp.pokemon = ally;
        lp.ignore_ability = (sp->move >= 0 && (pr_moves[sp->move].flags & MOVE_FLAGS_IGNORE_ABILITIES))
                            || sp->ignore_ally_ability;
        pr_apply_ab_attrs(bt, PR_AC_AllyStatMultiplierAbAttr, &lp);
    }

    int weather = !sp->for_defend || sp->ignore_opp_ability || !sp->opponent
                      ? bt->weather
                      : pr_get_effective_weather_for_move(bt, sp->opponent);

    double ret = v * pr_get_stat_stage_multiplier(bt, p, stat, sp->opponent, sp->move, sp->ignore_opp_ability,
                                                  sp->is_critical, sp->simulated, sp->ignore_held_items);
    switch (stat) {
    case STAT_ATK:
        if (pr_has_battler_tag(bt, p, BATTLER_TAG_TYPE_SLOW_START)) ret = (double)((int32_t)ret >> 1);
        break;
    case STAT_DEF:
        if (pr_is_of_type(p, POKEMON_TYPE_ICE) && weather == WEATHER_TYPE_SNOW) ret *= 1.5;
        break;
    case STAT_SPDEF:
        if (pr_is_of_type(p, POKEMON_TYPE_ROCK) && weather == WEATHER_TYPE_SANDSTORM) ret *= 1.5;
        break;
    case STAT_SPD: {
        int side = p->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY;
        if (pr_arena_has_tag(bt, ARENA_TAG_TYPE_TAILWIND, side)) ret *= 2;
        if (pr_arena_has_tag(bt, ARENA_TAG_TYPE_GRASS_WATER_PLEDGE, side)) ret = (double)((int32_t)ret >> 2);
        if (pr_has_battler_tag(bt, p, BATTLER_TAG_TYPE_SLOW_START)) ret = (double)((int32_t)ret >> 1);
        if (p->status == STATUS_EFFECT_PARALYSIS) ret = (double)((int32_t)ret >> 1);
        if (pr_has_battler_tag(bt, p, BATTLER_TAG_TYPE_UNBURDEN) && pr_has_ability_active(bt, p, ABILITY_ID_UNBURDEN)) ret *= 2;
        break;
    }
    }
    /* HighestStatBoostTag (Protosynthesis, Quark Drive) for this stat: its multiplier isn't modelled */
    if (pr_has_battler_tag_class(bt, p, PR_TAGCLASS_HIGHEST_STAT_BOOST)) pr_note_unported(bt, PR_FEAT_BATTLER_TAGS);
    ret = floor(ret);
    return ret > 1 ? (int32_t)ret : 1;
}

int32_t pr_get_effective_stat(pr_battle *bt, const pr_pokemon *p, int stat,
                              const pr_pokemon *opponent, int move, bool is_critical, bool for_defend)
{
    pr_stat_params sp = {.opponent = opponent, .move = move, .is_critical = is_critical, .for_defend = for_defend,
                         .simulated = true};
    return pr_get_effective_stat_p(bt, p, stat, &sp);
}

/* isForciblyGrounded(): IGNORE_FLYING tag or Gravity */
bool pr_is_forcibly_grounded(pr_battle *bt, const pr_pokemon *p)
{
    return pr_has_battler_tag(bt, p, BATTLER_TAG_TYPE_IGNORE_FLYING) || pr_arena_has_tag(bt, ARENA_TAG_TYPE_GRAVITY, ARENA_TAG_SIDE_BOTH);
}

/* isForciblyUngrounded(): FLOATING / TELEKINESIS tags, then UngroundedAbAttr (Levitate) */
bool pr_is_forcibly_ungrounded(pr_battle *bt, const pr_pokemon *p)
{
    if (pr_is_forcibly_grounded(bt, p)) return false;
    if (pr_has_battler_tag(bt, p, BATTLER_TAG_TYPE_FLOATING) || pr_has_battler_tag(bt, p, BATTLER_TAG_TYPE_TELEKINESIS))
        return true;
    bool ungrounded = false;
    pr_ab_params pp = {.pokemon = p, .move = -1, .simulated = true, .flag = &ungrounded};
    pr_apply_ab_attrs(bt, PR_AC_UngroundedAbAttr, &pp);
    return ungrounded;
}

/* isGrounded(ignoreSemiInvulnerable) */
bool pr_is_grounded(pr_battle *bt, const pr_pokemon *p, bool ignore_semi_invulnerable)
{
    if (pr_is_forcibly_grounded(bt, p)) return true;
    if (pr_is_forcibly_ungrounded(bt, p)) return false;
    bool semi = !ignore_semi_invulnerable && pr_has_battler_tag_class(bt, p, PR_TAGCLASS_SEMI_INVULNERABLE);
    int8_t t[3];
    int n = pr_get_types(p, t, PR_TYPES_ORIGINAL_IF_STELLAR);
    bool flying = false;
    for (int i = 0; i < n; i++) if (t[i] == POKEMON_TYPE_FLYING) flying = true;
    return !semi && !flying;
}

/* JS Math.round: halves go toward +infinity, and it is exact (no x + 0.5 rounding). */
double js_round(double x)
{
    double r = floor(x);
    return (x - r >= 0.5) ? r + 1 : r;
}

/* hasAbility(ability): ability or passive. Suppression is not ported. */
bool pr_has_ability(const pr_pokemon *p, int ability)
{
    return p->ability == ability || (p->passive && p->passive == ability);
}

double pr_get_hp_ratio(const pr_pokemon *p, bool precise)
{
    double r = (double)p->hp / pr_get_stat(p, STAT_HP, true);
    return precise ? r : js_round(r * 100) / 100;
}

/* getWeight(): Autotomize tag not modelled (no tags yet). */
double pr_get_weight(pr_battle *bt, const pr_pokemon *p)
{
    if (pr_has_battler_tag(bt, p, BATTLER_TAG_TYPE_AUTOTOMIZED)) pr_note_unported(bt, PR_FEAT_BATTLER_TAGS);
    const pr_form *f = pr_pokemon_form(p);
    double w = f->weight_x10 / 10.0; /* same double as upstream's decimal literal */
    pr_ab_params pp = {.pokemon = p, .move = -1, .simulated = true, .value = &w};
    pr_apply_ab_attrs(bt, PR_AC_WeightMultiplierAbAttr, &pp);
    return w > 0.1 ? w : 0.1;
}

/* weather.ts getEffectiveWeatherForMove: an override (Cloud Nine-like attrs on the user)
 * wins over suppression. */
int pr_get_effective_weather_for_move(pr_battle *bt, const pr_pokemon *user)
{
    int w = WEATHER_TYPE_NONE;
    pr_ab_params pp = {.pokemon = user, .move = -1, .simulated = true, .holder = &w};
    pr_apply_ab_attrs(bt, PR_AC_PreAttackWeatherOverrideAbAttr, &pp);
    if (w != WEATHER_TYPE_NONE) return w;
    if (bt->weather == WEATHER_TYPE_NONE || pr_weather_effect_suppressed(bt)) return WEATHER_TYPE_NONE;
    return bt->weather;
}
