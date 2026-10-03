/* Ports of src/field/pokemon.ts: stats, types, stat stages, effective stats. */
#include "pr/battle.h"
#include "pr/engine.h"
#include "pr/text.h"
#include <math.h>
#include <string.h>

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

const pr_species *pr_pokemon_fusion_species(const pr_pokemon *p)
{
    if (!p->fusion_species) return NULL;
    int lo = 0, hi = PR_SPECIES_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (pr_species_table[mid].id == p->fusion_species) return &pr_species_table[mid];
        if (pr_species_table[mid].id < p->fusion_species) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

const pr_form *pr_pokemon_fusion_form(const pr_pokemon *p)
{
    const pr_species *fs = pr_pokemon_fusion_species(p);
    if (!fs) return NULL;
    return &pr_forms[fs->form0 + (p->fusion_form < fs->nforms ? p->fusion_form : 0)];
}

/* calculateStats() with calculateBaseStats(). Fusion and challenges (FLIP_STAT) are
 * not ported yet; the Wonder Guard check reads the permanent ability and the passive. */
void pr_recalculate_stats(pr_battle *bt, pr_pokemon *p)
{
    int32_t hp = p->hp, last_max = p->stats[STAT_HP];
    pr_calculate_stats(bt, p);
    int32_t max = p->stats[STAT_HP];
    if (hp > max) p->hp = max;
    else p->hp = hp && last_max && max > last_max ? hp + (max - last_max) : hp;
}

void pr_calculate_stats(pr_battle *bt, pr_pokemon *p)
{
    const pr_form *f = pr_pokemon_form(p);
    const pr_form *ff = pr_pokemon_fusion_form(p);
    double base[6];
    for (int s = 0; s < 6; s++) {
        if (ff) {
            base[s] = ceil((f->base_stats[s] + ff->base_stats[s]) / 2.0);
        } else {
            base[s] = f->base_stats[s];
        }
    }
    pr_apply_base_stat_modifiers(bt, p, base);
    for (int s = 0; s < 6; s++) {
        double v = floor((2 * base[s] + p->ivs[s]) * p->level * 0.01);
        if (s == STAT_HP) {
            v = v + p->level + 10;
            pr_apply_incrementing_stat_modifier(bt, p, s, &v);
            /* hasAbility(WONDER_GUARD, canApply false, ignoreOverride true): the permanent ability or the passive */
            if (p->base_ability == ABILITY_ID_WONDER_GUARD || p->passive == ABILITY_ID_WONDER_GUARD) v = 1;
            p->hp = (int32_t)v; /* fresh Pokemon: hp starts at max */
        } else {
            v += 5;
            double m = pr_nature_mult[p->nature][s];
            pr_apply_nature_weight_modifier(bt, p, &m);
            if (m != 1) {
                v = (m > 1) ? ceil(v * m) : floor(v * m);
                if (v < 1) v = 1;
            }
            pr_apply_incrementing_stat_modifier(bt, p, s, &v);
        }
        if (v < 1) v = 1;
        p->stats[s] = (int32_t)v;
    }
}

double pr_get_stat(const pr_pokemon *p, int stat, bool bypass_summon)
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
int pr_get_types(const pr_pokemon *p, int8_t out[PR_MAX_TYPES], unsigned flags)
{
    /* A Stellar tera reports [STELLAR] unless the caller asks for the original types. */
    int tera = pr_get_tera_type(p);
    bool use_tera = !((flags & PR_TYPES_ORIGINAL_IF_STELLAR) && tera == POKEMON_TYPE_STELLAR);
    if (!(flags & PR_TYPES_NO_TERA) && p->terastallized && use_tera) {
        out[0] = (int8_t)tera;
        return 1;
    }
    /* getBaseTypes: summonData.types when set (and not bypassed), else the form's two */
    int8_t base[PR_MAX_SUMMON_TYPES];
    int nb;
    if (!(flags & PR_TYPES_BYPASS_SUMMON) && p->n_summon_types) {
        nb = p->n_summon_types;
        memcpy(base, p->summon_types, (size_t)nb);
    } else {
        const pr_form *f = pr_pokemon_form(p);
        const pr_form *ff = pr_pokemon_fusion_form(p);
        int8_t type1 = f->type1;
        int8_t second_type = f->type2 < 0 ? POKEMON_TYPE_UNKNOWN : f->type2;
        if (ff) {
            int8_t fusion_type1 = ff->type1;
            int8_t fusion_type2 = ff->type2 < 0 ? POKEMON_TYPE_UNKNOWN : ff->type2;
            if (fusion_type2 != POKEMON_TYPE_UNKNOWN && fusion_type2 != type1) {
                second_type = fusion_type2;
            } else if (fusion_type1 != type1) {
                second_type = fusion_type1;
            }
        }
        base[0] = type1;
        base[1] = second_type;
        nb = 2;
    }
    /* new Set(base), then drop UNKNOWN when something else is present */
    int n = 0;
    for (int i = 0; i < nb; i++) {
        bool have = false;
        for (int k = 0; k < n; k++) have = have || out[k] == base[i];
        if (!have) out[n++] = base[i];
    }
    if (n > 1) {
        int k = 0;
        for (int i = 0; i < n; i++)
            if (out[i] != POKEMON_TYPE_UNKNOWN) out[k++] = out[i];
        n = k;
    }
    /* then the type Forest's Curse or Trick-or-Treat added, if the set lacks it */
    if (!(flags & PR_TYPES_IGNORE_THIRD) && p->added_type) {
        bool have = false;
        for (int i = 0; i < n; i++) have = have || out[i] == p->added_type;
        if (!have) out[n++] = p->added_type;
    }
    return n;
}

bool pr_set_summon_types(pr_battle *bt, pr_pokemon *p, const int8_t *types, int n)
{
    if (n > PR_MAX_SUMMON_TYPES) {
        pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT);
        return false;
    }
    memcpy(p->summon_types, types, (size_t)n);
    p->n_summon_types = (uint8_t)n;
    return true;
}

bool pr_is_of_type(const pr_pokemon *p, int type)
{
    int8_t t[PR_MAX_TYPES];
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
    if (!ignore_held_items) pr_apply_temp_stat_stage_booster_modifiers(bt, p->is_player, stat, &m); /* X items */
    return m < 4 ? m : 4;
}

int32_t pr_get_effective_stat_p(pr_battle *bt, const pr_pokemon *p, int stat, const pr_stat_params *sp)
{
    double v = pr_get_stat(p, stat, false);
    if (!sp->ignore_held_items) pr_apply_stat_booster_modifiers(bt, p, stat, &v);

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
    /* findTag(t => t instanceof HighestStatBoostTag && t.stat === stat): Protosynthesis, Quark Drive */
    for (int i = 0; i < p->n_tags; i++)
        if (pr_battler_tag_is(p->tags[i].type, PR_TAGCLASS_HIGHEST_STAT_BOOST) && p->tags[i].stat == stat) {
            ret *= p->tags[i].multiplier;
            break;
        }
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
    pr_ab_params pp = {.pokemon = p, .move = -1, .simulated = false, .flag = &ungrounded}; /* not simulated upstream */
    pr_apply_ab_attrs(bt, PR_AC_UngroundedAbAttr, &pp);
    return ungrounded;
}

/* isGrounded(ignoreSemiInvulnerable) */
bool pr_is_grounded(pr_battle *bt, const pr_pokemon *p, bool ignore_semi_invulnerable)
{
    if (pr_is_forcibly_grounded(bt, p)) return true;
    if (pr_is_forcibly_ungrounded(bt, p)) return false;
    bool semi = !ignore_semi_invulnerable && pr_has_battler_tag_class(bt, p, PR_TAGCLASS_SEMI_INVULNERABLE);
    int8_t t[PR_MAX_TYPES];
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

/* hasPassive() and getPassiveAbility(), stored in p->passive (0 = no passive). hasPassive is
 * Pokemon.passive or being a boss; the game-mode rules around it (no passive for the classic
 * final boss or endless bosses, the passives challenge, daily-run passives) and the test
 * overrides belong to the mode layer, which isn't ported. */
/* Pokemon.hasPassive(), with the game mode's rules when the mode is known: no passive for the
 * enemy in the classic final battle or on an endless boss wave, and a daily run's Pokemon has the
 * passive its custom data names. (The passives challenge isn't modelled.) */
bool pr_has_passive(pr_battle *bt, const pr_pokemon *p)
{
    int gm = bt->game_mode, w = bt->wave_index;
    if (gm == GAME_MODES_CHALLENGE) pr_note_unported(bt, PR_FEAT_WAVE);
    bool endless = gm == GAME_MODES_ENDLESS || gm == GAME_MODES_SPLICED_ENDLESS;
    if (!p->is_player && ((gm == GAME_MODES_CLASSIC && w == 200) || (endless && (w % 250 == 0 || w % 1000 == 0))))
        return false;
    if (gm == GAME_MODES_DAILY && p->custom_passive != -1) return true;
    return p->passive_flag || p->is_boss;
}

void pr_update_passive(pr_battle *bt, pr_pokemon *p)
{
    if (!pr_has_passive(bt, p)) { p->passive = 0; return; }
    if (p->custom_passive >= 0) p->passive = (uint16_t)p->custom_passive;
    else p->passive = pr_pokemon_form(p)->passive;
}

double pr_damage_indirect(pr_battle *bt, pr_pokemon *p, double damage)
{
    if (p->hp <= 0) return 0; /* isFainted() */
    /* Indirect damage sets preventEndure, so Endure, the Sturdy tag and Focus Band don't apply.
     * With ignoreSegments the HP loss is the same for a boss; what the cleared segments bring
     * (the segment index, random stat boosts drawn from the global RNG, their stat change) doesn't
     * change this damage calculation and is recorded for the turn engine. The classic final
     * boss's first-segment rule belongs to the mode layer. */
    if (p->is_boss) pr_note_deferred(bt, PR_FEAT_BOSS_SEGMENTS);
    double d = damage < p->hp ? damage : p->hp;
    p->hp -= (int32_t)d;
    pr_hook_ab(bt, p, PR_AC_PostDamageAbAttr); /* applyAbAttrs("PostDamageAbAttr"): no source */
    return d;
}

double pr_get_hp_ratio(const pr_pokemon *p, bool precise)
{
    double r = (double)p->hp / pr_get_stat(p, STAT_HP, true);
    return precise ? r : js_round(r * 100) / 100;
}

/* getWeight(): form weight minus 100 kg per Autotomize, then weight abilities, at least 0.1 */
double pr_get_weight(pr_battle *bt, const pr_pokemon *p)
{
    const pr_battler_tag *at = pr_find_battler_tag_class(p, PR_TAGCLASS_AUTOTOMIZED);
    double removed = at ? 100.0 * at->autotomize_count : 0;
    const pr_form *f = pr_pokemon_form(p);
    double w = f->weight_x10 / 10.0 - removed; /* weight_x10 / 10.0 is the same double as upstream's decimal literal */
    pr_ab_params pp = {.pokemon = p, .move = -1, .simulated = false, .value = &w}; /* not simulated upstream */
    pr_apply_ab_attrs(bt, PR_AC_WeightMultiplierAbAttr, &pp);
    return w > 0.1 ? w : 0.1;
}

/* weather.ts getEffectiveWeatherForMove: an override (Cloud Nine-like attrs on the user)
 * wins over suppression. */
int pr_get_effective_weather_for_move(pr_battle *bt, const pr_pokemon *user)
{
    int w = WEATHER_TYPE_NONE;
    pr_ab_params pp = {.pokemon = user, .move = -1, .simulated = false, .holder = &w}; /* not simulated upstream */
    pr_apply_ab_attrs(bt, PR_AC_PreAttackWeatherOverrideAbAttr, &pp);
    if (w != WEATHER_TYPE_NONE) return w;
    if (bt->weather == WEATHER_TYPE_NONE || pr_weather_effect_suppressed(bt)) return WEATHER_TYPE_NONE;
    return bt->weather;
}

/* Task 53: Mega Evolution & Dynamax */
bool pr_is_mega(const pr_pokemon *p)
{
    if (!p) return false;
    const pr_form *f = pr_pokemon_form(p);
    if (f && f->form_key) {
        if (!strncmp(f->form_key, "mega", 4) || !strcmp(f->form_key, "primal"))
            return true;
    }
    const pr_form *ff = pr_pokemon_fusion_form(p);
    if (ff && ff->form_key) {
        if (!strncmp(ff->form_key, "mega", 4) || !strcmp(ff->form_key, "primal"))
            return true;
    }
    return false;
}

bool pr_is_max(const pr_pokemon *p)
{
    if (!p) return false;
    if (p->is_dynamaxed) return true;
    const pr_form *f = pr_pokemon_form(p);
    if (f && f->form_key) {
        if (!strncmp(f->form_key, "gigantamax", 10) || !strcmp(f->form_key, "eternamax"))
            return true;
    }
    const pr_form *ff = pr_pokemon_fusion_form(p);
    if (ff && ff->form_key) {
        if (!strncmp(ff->form_key, "gigantamax", 10) || !strcmp(ff->form_key, "eternamax"))
            return true;
    }
    return false;
}

void pr_mega_evolve(pr_battle *bt, pr_pokemon *p)
{
    if (!p) return;
    p->queued_mega = false;
    const pr_species *sp = pr_pokemon_species(p);
    if (!sp) return;

    int mega_form_idx = -1;
    for (uint16_t i = 0; i < sp->nforms; i++) {
        const pr_form *f = &pr_forms[sp->form0 + i];
        if (f->form_key && !strncmp(f->form_key, "mega", 4)) {
            mega_form_idx = (int)i;
            break;
        }
    }
    if (mega_form_idx < 0) return;

    pr_msg_arg pre_name_arg = pr_arg_pokemon(bt, "preName", p);
    p->form = (uint8_t)mega_form_idx;

    pr_recalculate_stats(bt, p);

    const pr_form *mf = pr_pokemon_form(p);
    p->base_ability = mf->ability1;
    if (p->ability_override < 0) p->ability = mf->ability1;

    if (bt->engine) {
        pr_queue_msg(bt, bt->engine, pr_msg1("battlePokemonForm:megaChange", pre_name_arg));
    }

    pr_ab_params gain = {.pokemon = p, .move = -1, .only = 1, .on_gain = true};
    pr_apply_ab_attrs(bt, PR_AC_PostSummonAbAttr, &gain);
}

void pr_dynamax(pr_battle *bt, pr_pokemon *p)
{
    if (!p || p->is_dynamaxed) return;
    p->queued_dynamax = false;
    p->pre_dynamax_form = p->form;
    const pr_species *sp = pr_pokemon_species(p);

    if (sp) {
        for (uint16_t i = 0; i < sp->nforms; i++) {
            const pr_form *f = &pr_forms[sp->form0 + i];
            if (f->form_key && !strncmp(f->form_key, "gigantamax", 10)) {
                p->form = (uint8_t)i;
                break;
            }
        }
    }

    p->is_dynamaxed = true;
    p->dynamax_turns_left = 3;

    double mult = 1.5 + ((double)p->dynamax_level * 0.05);
    p->stats[STAT_HP] = (int32_t)js_round((double)p->stats[STAT_HP] * mult);
    p->hp = (int32_t)js_round((double)p->hp * mult);

    if (bt->engine) {
        const pr_form *f = pr_pokemon_form(p);
        if (f->form_key && strstr(f->form_key, "gigantamax")) {
            pr_msg_arg pre_name_arg = pr_arg_pokemon(bt, "preName", p);
            pr_queue_msg(bt, bt->engine, pr_msg1("battlePokemonForm:gigantamaxChange", pre_name_arg));
        }
    }
}
