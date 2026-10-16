/* The phases of a turn (src/phases/): each pr_phase type's start(), in upstream's order of
 * operations. Anything a phase would do that isn't ported yet reports itself (pr_note_unported)
 * at the point upstream does it, so a turn that meets it is never claimed faithful; the phase
 * is still created where upstream creates it, so the order of what follows stays right. */
#include "pr/engine.h"
#include <math.h>
#include <string.h>

#define PH(i) (&e->pool[i])

static double to_dmg_value(double v) { double f = floor(v); return f < 1 ? 1 : f; }

/* ---------------------------------------------------------------- Pokemon helpers */

/* isActive(): isAllowedInBattle (not fainted; no challenges are modelled) */
static bool is_classic_final_boss(const pr_battle *bt)
{
    return (bt->game_mode == GAME_MODES_CLASSIC || bt->game_mode == GAME_MODES_CHALLENGE)
        && bt->wave_index == 200;
}

static bool is_active(const pr_pokemon *p) { return p && p->hp > 0; }
static bool is_on_field(const pr_pokemon *p) { return p && p->on_field && !p->switch_out_status; }
static bool is_active_on_field(const pr_pokemon *p) { return is_active(p) && is_on_field(p); }
static int32_t max_hp(const pr_pokemon *p) { return pr_get_stat(p, STAT_HP, false); }

/* getPokemon() of a PokemonPhase: the Pokemon in that field slot */
static pr_pokemon *slot(pr_battle *bt, int b) { return b >= 0 && b < 4 ? bt->field[b] : NULL; }

/* MoveEffectPhase.getUserPokemon(): a delayed attack's phase was given the source's Pokemon.id as
 * its battlerIndex (getPokemonById: both parties; only the field is modelled), the others their
 * user's field slot */
static pr_pokemon *me_user(pr_battle *bt, const pr_phase *fx)
{
    if (fx->user_id < 0) return slot(bt, fx->battler);
    pr_pokemon *p = pr_field_pokemon_by_id(bt, fx->user_id);
    if (!p) pr_note_unported(bt, PR_FEAT_PARTY);
    return p;
}
/* PokemonPhase.player and fieldIndex: battlerIndex < 2 and battlerIndex % 2, the Pokemon.id's for a
 * delayed attack */
static bool me_player(const pr_phase *fx) { return fx->user_id < 0 && fx->battler < 2; }
static int me_field_index(const pr_phase *fx) { return fx->user_id < 0 ? fx->battler % 2 : (int)(fx->user_id % 2); }

/* globalScene.getField(true).filter(p => list.includes(p.getBattlerIndex())), in field order */
static int active_in_list(pr_battle *bt, const pr_target_list *t, pr_pokemon **out)
{
    int n = 0;
    for (int i = 0; i < 4; i++) {
        if (!is_active(bt->field[i])) continue;
        for (int k = 0; t && k < t->n; k++)
            if (t->v[k] == i) { out[n++] = bt->field[i]; break; }
    }
    return n;
}

static pr_target_list *tlist(pr_engine *e, int list) { return list >= 0 ? &e->target_lists[list] : NULL; }

/* Registered native ability-trigger transitions. Attribute predicates choose
 * whether to call the scene trigger; the scene then matches the raw form key. */
static bool native_ability_form_target(const pr_pokemon *p, int *target)
{
    switch(p->species) {
    case SPECIES_ID_DARMANITAN: case SPECIES_ID_GALAR_DARMANITAN:
    case SPECIES_ID_WISHIWASHI: case SPECIES_ID_MORPEKO:
    case SPECIES_ID_EISCUE: case SPECIES_ID_MIMIKYU:
        if(p->form<2)*target=1-p->form;
        return true;
    case SPECIES_ID_MINIOR:
        if(p->form<14)*target=p->form<7?p->form+7:p->form-7;
        return true;
    case SPECIES_ID_ZYGARDE: {
        const pr_form *from=pr_pokemon_base_form(p);
        const char *keys[]={"50-pc","complete","10-pc","10-complete"};
        const char *to=NULL;
        for(int i=0;i<4;i++)if(from->form_key&&!strcmp(from->form_key,keys[i]))to=keys[i^1];
        const pr_species *sp=pr_pokemon_species(p);
        for(int i=0;to&&i<sp->nforms;i++)if(!strcmp(pr_forms[sp->form0+i].form_key,to))*target=i;
        return true;
    }
    default:return false;
    }
}

/* globalScene.triggerPokemonFormChange(pokemon, <trigger class>): mid-battle form changes */
static void form_change_with_delay(pr_battle *bt, const pr_pokemon *p, int trigger_class, bool delayed)
{
    if (!p) return;
    const pr_species *sp = pr_pokemon_species(p);
    if (!sp || !(sp->form_change_triggers & (1u << trigger_class))) return;

    /* Meloetta's registered post-move predicate cannot trigger without Relic
     * Song as its newest history entry. Preserve explicit unsupported reporting
     * for that positive path; ordinary reserve entrants need no form phase. */
    if (p->species == SPECIES_ID_MELOETTA && trigger_class == PR_FCT_SpeciesFormChangePostMoveTrigger) {
        const pr_turn_move *last = pr_last_move(bt, p, 0);
        if (!last || last->move != MOVE_ID_RELIC_SONG) return;
    }

    pr_pokemon *mon = (pr_pokemon *)p;
    int target_form = -1;

    bool stance = mon->species == SPECIES_ID_AEGISLASH;
    if (stance) {
        if (trigger_class == PR_FCT_SpeciesFormChangeActiveTrigger) {
            if (mon->form == 1 && !is_active_on_field(mon)) target_form = 0;
        } else if (trigger_class == PR_FCT_SpeciesFormChangePreMoveTrigger
                   && pr_has_ability_active(bt, mon, ABILITY_ID_STANCE_CHANGE)) {
            /* The registered trigger reads turnCommands, including for called moves. */
            int b = pr_battler_index(bt, mon);
            if (b >= 0 && b < 4 && bt->commands[b].command >= 0 && !bt->commands[b].no_move) {
                int move = bt->commands[b].move;
                if (mon->form == 1 && move == MOVE_ID_KINGS_SHIELD) target_form = 0;
                else if (mon->form == 0 && move > 0 && move < PR_MOVE_COUNT
                         && pr_moves[move].category != MOVE_CATEGORY_STATUS) target_form = 1;
            }
        }
    /* Weather form triggers are quiet phases upstream, queued before mutation. */
    } else if (trigger_class == PR_FCT_SpeciesFormChangeAbilityTrigger && native_ability_form_target(mon,&target_form)) {
        /* Match source registrations; queue the mutation below. */
    } else if (mon->species == SPECIES_ID_CASTFORM || mon->species == SPECIES_ID_CHERRIM) {
        int ability = mon->species == SPECIES_ID_CASTFORM ? ABILITY_ID_FORECAST : ABILITY_ID_FLOWER_GIFT;
        bool raw = mon->base_ability == ability || mon->passive == ability;
        bool suppressed = pr_weather_effect_suppressed(bt);
        if (trigger_class == PR_FCT_SpeciesFormChangeActiveTrigger && !is_active_on_field(mon)) target_form = 0;
        else if (trigger_class == PR_FCT_SpeciesFormChangeWeatherTrigger
            && !mon->ability_suppressed && !suppressed && pr_has_ability_active(bt, mon, ability)) {
            if (bt->weather == WEATHER_TYPE_SUNNY || bt->weather == WEATHER_TYPE_HARSH_SUN) target_form = 1;
            else if (mon->species == SPECIES_ID_CASTFORM) {
                if (bt->weather == WEATHER_TYPE_RAIN || bt->weather == WEATHER_TYPE_HEAVY_RAIN) target_form = 2;
                if (bt->weather == WEATHER_TYPE_HAIL || bt->weather == WEATHER_TYPE_SNOW) target_form = 3;
            }
        } else if (trigger_class == PR_FCT_SpeciesFormChangeRevertWeatherFormTrigger && raw) {
            bool weather_reverts = mon->species == SPECIES_ID_CHERRIM
                ? bt->weather != WEATHER_TYPE_SUNNY && bt->weather != WEATHER_TYPE_HARSH_SUN
                : bt->weather == WEATHER_TYPE_NONE || bt->weather == WEATHER_TYPE_SANDSTORM
                  || bt->weather == WEATHER_TYPE_STRONG_WINDS || bt->weather == WEATHER_TYPE_FOG;
            if (weather_reverts || suppressed || mon->ability_suppressed
                || (mon->ability_override != 0 && mon->ability_override != ability)) target_form = 0;
        }
    } else if (mon->species == SPECIES_ID_EISCUE && trigger_class == PR_FCT_SpeciesFormChangeAbilityTrigger) {
        target_form = mon->form == 0 ? 1 : 0;
    } else if (mon->species == SPECIES_ID_BATTLE_BOND_GRENINJA && trigger_class == PR_FCT_SpeciesFormChangeAbilityTrigger) {
        /* The attribute's formFunc decides whether this trigger is called.
         * Both registered form changes have no additional conditions. */
        target_form = mon->form == 0 ? 1 : 0;
    } else if (mon->species == SPECIES_ID_ETERNATUS && mon->form == 0
        && trigger_class == PR_FCT_SpeciesFormChangeManualTrigger) {
        target_form = 1;
    } else goto other_forms;
    if (target_form >= 0 && target_form != mon->form) {
        pr_engine *e = bt->engine;
        if (!e) { pr_note_unported(bt, PR_FEAT_FORM_CHANGE); return; }
        int ph = pr_phase_new(e, PH_QUIET_FORM_CHANGE, (int8_t)pr_battler_index(bt, mon));
        if (ph < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return; }
        e->pool[ph].pokemon = mon;
        e->pool[ph].move_id = (uint16_t)target_form;
        if (delayed) pr_phase_push(bt, e, ph);
        else pr_phase_unshift(bt, e, ph);
    }
    return;
other_forms:

    /* Report unported only for form change triggers on other species */
    if (target_form < 0 && (sp->form_change_triggers & (1u << trigger_class))) {
        pr_note_unported(bt, PR_FEAT_FORM_CHANGE);
    }
}

static void form_change(pr_battle *bt, const pr_pokemon *p, int trigger_class)
{
    form_change_with_delay(bt, p, trigger_class, false);
}

/* globalScene.applyModifiers(<a turn-path modifier class>, player, pokemon): held items and
 * side-wide modifiers outside the damage calculation are not ported yet */
static void turn_modifiers(pr_battle *bt, bool player, const pr_pokemon *p)
{
    int side = player ? 0 : 1;
    for (int i = 0; i < bt->n_mods[side]; i++) {
        const pr_modifier *m = &bt->mods[side][i];
        /* Fusion-specific Leek/stat-item eligibility remains outside the full-turn proof.
         * Do not contaminate the raw damage API's source-proven projection. */
        bool fused_item = p && p->fusion_species && (m->cls == PR_MOD_SPECIES_CRIT_BOOSTER || m->cls == PR_MOD_SPECIES_STAT_BOOSTER
                || m->cls == PR_MOD_EVOLUTION_STAT_BOOSTER)
            && (m->pokemon_id == -1 || m->pokemon_id == (int64_t)p->id);
        bool vitamin_over_cap = p && m->cls == PR_MOD_BASE_STAT && m->stat >= STAT_HP && m->stat <= STAT_SPD
            && m->pokemon_id == (int64_t)p->id
            && (int64_t)m->stack_count + m->virtual_stack_count > p->ivs[m->stat];
        if (pr_mod_is_ported_item(m) && !fused_item && !vitamin_over_cap) continue;
        if (fused_item || vitamin_over_cap || !pr_mod_is(m, PR_MOD_HELD_ITEM) || !p || m->pokemon_id == (int64_t)p->id) {
            pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS);
            return;
        }
    }
}

/* pokemonMove.getMovePp(): maxPpOverride || pp + ppUp * toDmgValue(pp / 5) */
static int move_pp(const pr_move *m, int pp_up, int max_pp_override)
{
    if (max_pp_override > 0) return max_pp_override;
    return m->pp + pp_up * (int)to_dmg_value(m->pp / 5.0);
}

static int ref_pp_used(const pr_phase *ph)
{
    return ph->move.slot >= 0 ? (ph->move.from_transform ? ph->pokemon->transform_pp_used : ph->pokemon->pp_used)[ph->move.slot] : ph->move.pp_used;
}

static int ref_move_pp(const pr_phase *ph)
{
    const pr_move *m = &pr_moves[ph->move.id];
    if (ph->move.slot < 0) return move_pp(m, ph->move.pp_up, ph->move.max_pp_override);
    int s = ph->move.slot;
    return move_pp(m, ph->move.from_transform ? ph->pokemon->transform_pp_up[s] : ph->pokemon->pp_up[s],
                   ph->move.from_transform ? ph->pokemon->transform_max_pp_override[s] : ph->pokemon->max_pp_override[s]);
}

/* pokemonMove.usePp(count): ppUsed = min(ppUsed + count, getMovePp()) */
static void ref_use_pp(pr_phase *ph, int count)
{
    if(ph->move.slot>=0){pr_use_moveset_pp(ph->pokemon,ph->move.slot,ph->move.from_transform,count);return;}
    int next=ref_pp_used(ph)+count,max=ref_move_pp(ph);
    ph->move.pp_used=(uint8_t)(next>max?max:next);
}

static bool is_charging_move(int move)
{
    const char *k = pr_moves[move].kind;
    return !strcmp(k, "ChargingAttackMove") || !strcmp(k, "ChargingSelfStatusMove");
}

static bool is_field_targeted(int move)
{
    int t = pr_moves[move].target;
    return t == MOVE_TARGET_BOTH_SIDES || t == MOVE_TARGET_USER_SIDE || t == MOVE_TARGET_ENEMY_SIDE;
}

static bool is_ally_target(int move)
{
    switch (pr_moves[move].target) {
    case MOVE_TARGET_USER: case MOVE_TARGET_NEAR_ALLY: case MOVE_TARGET_ALLY: case MOVE_TARGET_USER_OR_NEAR_ALLY:
    case MOVE_TARGET_USER_AND_ALLIES: case MOVE_TARGET_USER_SIDE:
        return true;
    }
    return false;
}

/* move.applyConditions(user, target, sequence): every condition closure must hold */
bool pr_move_apply_conditions(pr_battle *bt, int move, const pr_pokemon *user, const pr_pokemon *target, int seq)
{
    const pr_move *m = &pr_moves[move];
    const pr_fnlist *l = seq == 2 ? &m->conditions_seq2 : seq == 3 ? &m->conditions_seq3 : &m->conditions;
    for (int i = 0; i < l->n; i++)
        if (!pr_closure_call(bt, l->v[i], PR_FEAT_MOVE_CONDITION, 1, user, target, move)) return false;
    return true;
}

/* applyMoveAttrs(<cls>, ...) for a class with no port on the turn path */
static void move_attrs_unported(pr_battle *bt, int move, uint16_t cls)
{
    pr_hook_move(bt, (uint16_t)move, cls);
}

/* ---------------------------------------------------------------- queueing helpers */

static double damage_and_update(pr_battle *bt, pr_engine *e, pr_pokemon *p, double damage, int result,
                                bool ignore_faint_phase, pr_pokemon *source, bool critical);

static int new_pokemon_phase(pr_engine *e, pr_phase_type t, int battler)
{
    return pr_phase_new(e, t, (int8_t)battler);
}

static void unshift_new(pr_battle *bt, pr_engine *e, pr_phase_type t, int battler)
{
    pr_phase_unshift(bt, e, new_pokemon_phase(e, t, battler));
}

void pr_unshift_new(pr_battle *bt, pr_engine *e, pr_phase_type t, int battler) { unshift_new(bt, e, t, battler); }

/* unshiftNew("CommonAnimPhase", battlerIndex, targetIndex, anim) */
void pr_unshift_common_anim(pr_battle *bt, pr_engine *e, int battler, int target, int anim)
{
    int ph = new_pokemon_phase(e, PH_COMMON_ANIM, battler);
    if (ph >= 0) {
        PH(ph)->anim = (uint16_t)anim;
        PH(ph)->anim_target = (int8_t)target;
    }
    pr_phase_unshift(bt, e, ph);
}

/* unshiftNew("PokemonAnimPhase", key, pokemon): battler = the Pokemon's BattlerIndex */
void pr_unshift_pokemon_anim(pr_battle *bt, pr_engine *e, int battler, int type)
{
    int ph = new_pokemon_phase(e, PH_POKEMON_ANIM, battler);
    if (ph >= 0) PH(ph)->anim = (uint16_t)type;
    pr_phase_unshift(bt, e, ph);
}

/* PokemonAnimPhase is a BattlePhase that caches its actual Pokemon object. */
void pr_unshift_pokemon_anim_for(pr_battle *bt, pr_engine *e, const pr_pokemon *p, int type)
{
    int ph = new_pokemon_phase(e, PH_POKEMON_ANIM, pr_battler_index(bt, p));
    if (ph >= 0) {
        PH(ph)->anim = (uint16_t)type;
        PH(ph)->pokemon = (pr_pokemon *)p;
    }
    pr_phase_unshift(bt, e, ph);
}

static void push_new(pr_battle *bt, pr_engine *e, pr_phase_type t, int battler)
{
    pr_phase_push(bt, e, new_pokemon_phase(e, t, battler));
}

/* unshiftNew("MovePhase", pokemon, targets, pokemonMove, useMode, timingModifier): slot is the
 * moveset slot, -1 for a detached `new PokemonMove(id)` */
int pr_unshift_move_phase(pr_battle *bt, pr_engine *e, pr_pokemon *p, int move, int slot, const int8_t *targets,
                          int n_targets, int use_mode, int timing)
{
    int mp = new_pokemon_phase(e, PH_MOVE, pr_battler_index(bt, p));
    if (mp < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return -1; }
    pr_phase *m = PH(mp);
    m->pokemon = p;
    m->move.id = (uint16_t)move;
    m->move.slot = (int8_t)slot;
    m->move.from_transform = PR_HAS_SUMMON_MOVES(p);
    m->move.pp_used = 0;
    m->use_mode = (uint8_t)use_mode;
    m->timing = (uint8_t)timing;
    m->targets = (int8_t)pr_targets_new(e, targets, n_targets);
    m->entry.move = MOVE_ID_NONE;
    m->entry.use_mode = (uint8_t)use_mode;
    m->entry.result = PR_MOVE_RESULT_UNDEFINED;
    pr_phase_unshift(bt, e, mp);
    return mp;
}

/* PokemonPhase with no battler given: the first active Pokemon of getField(), else PLAYER */
static int default_battler(pr_battle *bt)
{
    for (int i = 0; i < 4; i++)
        if (is_active(bt->field[i])) return i;
    return 0;
}

/* ---------------------------------------------------------------- AttemptCapturePhase */

static double get_pokeball_catch_multiplier(int pokeball_type)
{
    switch (pokeball_type) {
    case POKEBALL_TYPE_POKEBALL: return 1.0;
    case POKEBALL_TYPE_GREAT_BALL: return 1.5;
    case POKEBALL_TYPE_ULTRA_BALL: return 2.0;
    case POKEBALL_TYPE_ROGUE_BALL: return 3.0;
    case POKEBALL_TYPE_MASTER_BALL: return -1.0;
    case POKEBALL_TYPE_LUXURY_BALL: return 1.0;
    default: return 1.0;
    }
}

static double get_status_effect_catch_rate_multiplier(int status)
{
    switch (status) {
    case STATUS_EFFECT_POISON:
    case STATUS_EFFECT_TOXIC:
    case STATUS_EFFECT_PARALYSIS:
    case STATUS_EFFECT_BURN:
        return 1.5;
    case STATUS_EFFECT_SLEEP:
    case STATUS_EFFECT_FREEZE:
        return 2.5;
    default:
        return 1.0;
    }
}

static int get_critical_capture_chance(const pr_battle *bt, int modified_catch_rate)
{
    if (bt->game_mode == GAME_MODES_CHALLENGE) return 0;
    double dex_multiplier = (bt->game_mode == GAME_MODES_DAILY) ? 2.5 : 0.0;
    double charm_multiplier = 1.0;
    int cap_rate = modified_catch_rate < 255 ? modified_catch_rate : 255;
    return (int)floor((charm_multiplier * dex_multiplier * cap_rate) / 6.0);
}

/* BattleScene.addFaintedEnemyScore: capture and faint use the same calculation. */
static void add_fainted_enemy_score(pr_battle *bt, const pr_pokemon *pokemon)
{
    if (bt->max_exp_level <= 0) { pr_note_unported(bt, PR_FEAT_WAVE); return; }
    int iv = 0;
    for (int i = 0; i < 6; i++) iv += pokemon->ivs[i];
    const pr_form *fm = pr_pokemon_form(pokemon);
    double score = fm->base_exp * (fm->exp_boost ? 1.5 : 1)
        * ((double)pokemon->level / bt->max_exp_level) * ((iv / 93.0) * 0.2 + 0.8);
    for (int i = 0; i < bt->n_mods[1]; i++) {
        const pr_modifier *m = &bt->mods[1][i];
        if (!pr_mod_is(m, PR_MOD_HELD_ITEM) || m->pokemon_id != (int64_t)pokemon->id) continue;
        if (!pr_mod_is_ported_item(m) || m->unknown_subclass)
            pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS);
        /* getScoreMultiplier is per modifier object, independent of stack count. */
        if (m->cls == PR_MOD_BASE_STAT || m->cls == PR_MOD_BASE_STAT_FLAT) score *= 1.1;
        else if (m->cls == PR_MOD_BASE_STAT_TOTAL || m->cls == PR_MOD_INCREMENTING_STAT
            || m->cls == PR_MOD_ATTACK_TYPE_BOOSTER) score *= 1.2;
    }
    if (pokemon->is_boss) {
        if (pokemon->boss_segments) score *= sqrt(pokemon->boss_segments);
        else pr_note_unported(bt, PR_FEAT_BOSS_SEGMENTS); /* missing source context */
    }
    bt->battle_score += ceil(score);
}

static void attempt_capture_phase(pr_battle *bt, pr_engine *e, int ph)
{
    int b = PH(ph)->battler;
    if (b < 0 || b >= 4) b = 2;
    pr_pokemon *pokemon = slot(bt, b);
    if (!pokemon || pokemon->hp <= 0) return;

    pr_battler_tag *sub = (pr_battler_tag *)pr_find_battler_tag_class(pokemon, PR_TAGCLASS_SUBSTITUTE);
    if (sub) sub->in_focus = false;

    int pokeball_type = PH(ph)->sw_slot >= 0 ? PH(ph)->sw_slot : POKEBALL_TYPE_POKEBALL;

    double _3m = 3.0 * max_hp(pokemon);
    double _2h = 2.0 * pokemon->hp;
    const pr_form *fm = pr_pokemon_form(pokemon);
    double catch_rate = fm ? fm->catch_rate : 255;
    double pokeball_mult = get_pokeball_catch_multiplier(pokeball_type);
    double status_mult = get_status_effect_catch_rate_multiplier(pokemon->status);
    double shiny_mult = 1.0;

    double modified_catch_rate_float = (((_3m - _2h) * catch_rate * pokeball_mult) / _3m) * status_mult * shiny_mult;
    /* Master Ball uses a -1 multiplier upstream. That deliberately makes the intermediate
     * catch-rate math NaN, but the later guaranteed-catch branches never consume it. Avoid
     * converting NaN to int in C while preserving the same RNG behavior (the crit roll still runs). */
    bool guaranteed_ball = pokeball_mult == -1.0;
    int modified_catch_rate = guaranteed_ball ? 0 : (int)round(modified_catch_rate_float);

    int shake_probability = guaranteed_ball ? 0
        : (int)round(65536.0 / js_pow(255.0 / modified_catch_rate, 0.1875));
    int critical_capture_chance = guaranteed_ball ? 0 : get_critical_capture_chance(bt, modified_catch_rate);

    int crit_roll = pr_battle_rand_seed_int(bt, 256, 0);
    bool is_critical = crit_roll < critical_capture_chance;

    bool caught = true;

    if (is_critical) {
        int r = pr_battle_rand_seed_int(bt, 65536, 0);
        if (pokeball_mult == -1.0 || modified_catch_rate >= 255 || r < shake_probability) {
            caught = true;
        } else {
            caught = false;
        }
    } else {
        for (int i = 0; i < 3; i++) {
            int r = pokeball_mult == -1.0 || modified_catch_rate >= 255 ? -1 : pr_battle_rand_seed_int(bt, 65536, 0);
            if (pokeball_mult == -1.0 || modified_catch_rate >= 255 || r < shake_probability) {
                /* shake passes */
            } else {
                caught = false;
                break;
            }
        }
    }

    if (caught) {
        add_fainted_enemy_score(bt, pokemon);

        pr_show_text(bt, e, pr_msg1("battle:pokemonCaught", pr_arg_pokemon(bt, "pokemonName", pokemon)));
        unshift_new(bt, e, PH_VICTORY, b);
        pr_leave_field(bt, e, pokemon, true);
    } else {
        bt->last_used_pokeball = (int8_t)pokeball_type;
        if (sub) sub->in_focus = true;
    }
}

/* ---------------------------------------------------------------- TurnStartPhase */

static void handle_fight_command(pr_battle *bt, pr_engine *e, pr_pokemon *p, int b)
{
    struct pr_turn_command *c = &bt->commands[b];
    if (c->no_move) return;                         /* no queuedMove */
    /* (turnCommand.targets, set to [fieldIndex] for a player's MoveId.NONE command, equals the move's
     * own targets there: NONE targets the user) */
    /* moveset.find(m => m.moveId === queuedMove.move && m.ppUsed < m.getMovePp()) ?? new PokemonMove */
    int slot = -1;
    for (int k = 0; k < PR_N_MOVES(p); k++)
        if (PR_MOVES(p)[k] == c->move
            && PR_PP_USED(p)[k] < move_pp(&pr_moves[c->move], PR_PP_UP(p)[k], PR_MAX_PP(p)[k])) {
            slot = k;
            break;
        }
    if (pr_move_has_attr(c->move, PR_AC_MoveHeaderAttr)) {
        /* MoveHeaderPhase(pokemon, move): the same PokemonMove the MovePhase gets */
        int h = new_pokemon_phase(e, PH_MOVE_HEADER, b);
        if (h >= 0) {
            PH(h)->pokemon = p;
            PH(h)->move.id = c->move;
            PH(h)->move.slot = (int8_t)slot;
            PH(h)->move.from_transform = PR_HAS_SUMMON_MOVES(p);
            PH(h)->move.pp_used = 0;
        }
        pr_phase_unshift(bt, e, h);
    }
    int ph = new_pokemon_phase(e, PH_MOVE, b);
    if (ph < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return; }
    pr_phase *m = PH(ph);
    m->pokemon = p;
    m->move.id = c->move;
    m->move.slot = (int8_t)slot;
    m->move.from_transform = PR_HAS_SUMMON_MOVES(p);
    m->move.pp_used = 0;
    m->use_mode = c->use_mode;
    /* turnCommand.targets ?? queuedMove.targets: the command's own array */
    if (e->command_targets[b] < 0) e->command_targets[b] = (int8_t)pr_targets_new(e, c->targets, c->n_targets);
    m->targets = e->command_targets[b];
    m->entry.move = MOVE_ID_NONE;
    m->entry.use_mode = c->use_mode;
    m->entry.result = PR_MOVE_RESULT_UNDEFINED;
    pr_phase_push(bt, e, ph);
}

static void turn_start(pr_battle *bt, pr_engine *e)
{
    /* getCommandOrder: the active field, player side first; commands other than FIGHT go first
     * (the comparator keeps the original order otherwise, so this is a stable partition) */
    int order[4], n = 0;
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < 4; i++) {
            if (!is_active(bt->field[i])) continue;
            bool fight = bt->commands[i].command == COMMAND_FIGHT;
            if (fight == (pass == 1)) order[n++] = i;
        }

    pr_speed_iter it;
    pr_pokemon *p;
    pr_speed_iter_init(bt, &it, ARENA_TAG_SIDE_BOTH);
    while ((p = pr_speed_iter_next(bt, &it))) {
        int b = pr_battler_index(bt, p);
        const struct pr_turn_command *pre = &bt->pre_commands[b];
        if (pre->command >= 0 && pre->skip) continue;
        if (pre->command == COMMAND_TERA) {
            int t = new_pokemon_phase(e, PH_TERA, b);
            pr_phase_push(bt, e, t);
        } else if (p->queued_mega || pre->command == COMMAND_MEGA) {
            pr_mega_evolve(bt, p);
        } else if (p->queued_dynamax || pre->command == COMMAND_DYNAMAX || pre->command == COMMAND_GIGANTAMAX) {
            pr_dynamax(bt, p);
        }
    }
    pr_speed_iter_init(bt, &it, ARENA_TAG_SIDE_BOTH);
    while ((p = pr_speed_iter_next(bt, &it))) {
        int b = pr_battler_index(bt, p);
        if (bt->commands[b].command != COMMAND_FIGHT) continue;
        pr_ab_params params = {.pokemon = p, .move = -1};
        pr_apply_ab_attrs(bt, PR_AC_BypassSpeedChanceAbAttr, &params);
        pr_apply_bypass_speed_chance_modifiers(bt, e, p);     /* Quick Claw */
    }

    for (int k = 0; k < n; k++) {
        int b = order[k];
        pr_pokemon *mon = bt->field[b];
        struct pr_turn_command *c = &bt->commands[b];
        if (c->command < 0 || c->skip) continue;
        if (c->command == COMMAND_FIGHT) mon->order = (int8_t)k;
        switch (c->command) {
        case COMMAND_FIGHT: handle_fight_command(bt, e, mon, b); break;
        case COMMAND_BALL: {
            int target_battler = (c->n_targets > 0 && c->targets[0] >= 0) ? 2 + (c->targets[0] % 2) : 2;
            int q = new_pokemon_phase(e, PH_ATTEMPT_CAPTURE, target_battler);
            if (q >= 0) PH(q)->sw_slot = c->cursor;
            pr_phase_unshift(bt, e, q);
            break;
        }
        case COMMAND_POKEMON: {
            /* SwitchSummonPhase(args[0] ? BATON_PASS : SWITCH, getFieldIndex(), cursor, true, isPlayer()) */
            int q = new_pokemon_phase(e, PH_SWITCH_SUMMON, -1);
            if (q < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); e->overflow = true; break; }
            pr_phase *s = PH(q);
            s->sw_type = c->baton ? SWITCH_TYPE_BATON_PASS : SWITCH_TYPE_SWITCH;
            s->sw_field_index = (int8_t)(b % 2);
            s->sw_slot = c->cursor;
            s->sw_do_return = true;
            s->sw_player = mon->is_player;
            pr_phase_unshift(bt, e, q);
            break;
        }
        case COMMAND_RUN: unshift_new(bt, e, PH_ATTEMPT_RUN, -1); break;
        }
    }

    push_new(bt, e, PH_CHECK_INTERLUDE, -1);
    /* queueTurnEndPhases; WeatherEffectPhase is a PokemonPhase built with no battler */
    push_new(bt, e, PH_WEATHER_EFFECT, default_battler(bt));
    push_new(bt, e, PH_POSITIONAL_TAG, -1);
    push_new(bt, e, PH_BERRY, -1);
    push_new(bt, e, PH_CHECK_STATUS_EFFECT, -1);
    push_new(bt, e, PH_TURN_END, -1);
}

/* ---------------------------------------------------------------- MovePhase */

static void queue_move_end(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *m = PH(ph);
    /* this.pokemon.getBattlerIndex(): -1 once the Pokemon has been switched out */
    int end = new_pokemon_phase(e, PH_MOVE_END, (int8_t)pr_battler_index(bt, m->pokemon));
    if (end < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return; }
    pr_pokemon *t[4];
    int n = active_in_list(bt, tlist(e, m->targets), t);
    pr_phase *me = PH(end);
    for (int i = 0; i < n; i++) me->end_targets[i] = t[i];
    me->n_end_targets = (uint8_t)n;
    me->was_follow_up = m->use_mode >= MOVE_USE_MODE_INDIRECT;   /* isVirtual */
    pr_phase_unshift(bt, e, end);
}

/* the list moveHistoryEntry.targets is */
static int entry_list(const pr_phase *m) { return m->entry_targets == -2 ? m->targets : m->entry_targets; }

static void move_fail(pr_phase *m) { m->failed = true; m->entry.result = MOVE_RESULT_FAIL; }
static void move_cancel(pr_phase *m) { m->cancelled = true; m->entry.result = MOVE_RESULT_FAIL; }

void pr_cancel_current_move(pr_engine *e, const pr_pokemon *p)
{
    if (!e || e->current < 0) return;
    pr_phase *m = PH(e->current);
    if (m->type == PH_MOVE && (!p || m->pokemon == p)) move_cancel(m);
}

void pr_reflect_move(pr_battle *bt, pr_engine *e, pr_pokemon *pokemon, const pr_pokemon *opponent, int move)
{
    int8_t tg[4];
    int n = 1;
    if (pr_move_is_multi_target(move)) {
        bool multiple;
        n = pr_get_move_targets(bt, pokemon, move, -1, tg, &multiple);
    } else {
        tg[0] = (int8_t)pr_battler_index(bt, opponent);
    }
    if (pr_unshift_move_phase(bt, e, pokemon, move, -1, tg, n, MOVE_USE_MODE_REFLECTED, MOVE_PHASE_TIMING_MODIFIER_FIRST) < 0)
        e->overflow = true;
}

/* MoveReflectPhase: Magic Coat before Magic Bounce */
static void move_reflect_phase(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *r = PH(ph);
    pr_pokemon *p = r->pokemon, *opponent = r->source_p;
    int move = r->move_id;
    if (pr_get_battler_tag(p, BATTLER_TAG_TYPE_MAGIC_COAT)) {
        pr_reflect_move(bt, e, p, opponent, move);
        return;
    }
    pr_ab_params ap = {.pokemon = p, .opponent = opponent, .move = move};
    pr_apply_ab_attrs(bt, PR_AC_ReflectStatusMoveAbAttr, &ap);
}

void pr_fail_current_move(pr_engine *e)
{
    if (!e || e->current < 0) return;
    pr_phase *m = PH(e->current);
    if (m->type == PH_MOVE) move_fail(m);
}

/* the Pokemon's moveQueue.shift() */
static void move_queue_shift(pr_pokemon *p, pr_turn_move *out)
{
    if (!p->n_move_queue) return;
    if (out) *out = p->move_queue[0];
    memmove(&p->move_queue[0], &p->move_queue[1], (size_t)(p->n_move_queue - 1) * sizeof p->move_queue[0]);
    p->n_move_queue--;
}

/* attackedByItemMessageFunc: getHeldItems uses exact owner IDs and list order,
 * then filters transferable items. No RNG, removal, or stack-count selection. */
bool pr_poltergeist_message(pr_battle *bt, const pr_pokemon *target, pr_msg *out)
{
    if (!target) return false;
    int side = target->is_player ? 0 : 1;
    for (int k = 0; k < bt->n_mods[side]; k++) {
        const pr_modifier *m = &bt->mods[side][k];
        if (m->pokemon_id != (int64_t)target->id || !m->is_transferable
            || !pr_mod_is(m, PR_MOD_HELD_ITEM)) continue;
        const char *key = pr_held_item_name_key(m);
        /* An unsupported identity remains a visible missing-text failure. */
        if (!key) { *out = pr_msg0(NULL); return true; }
        *out = pr_msg2("moveTriggers:attackedByItem",
            pr_arg_pokemon(bt, "pokemonName", target), pr_arg_key("itemName", key));
        return true;
    }
    return false;
}

/* PreMoveMessageAttr.apply: a message when the move's message function returns one. The
 * functions live in the move list, so each user is ported by move id. */
/* PreMoveMessageAttr's text (Struggle, Magnitude, Fickle Beam, Poltergeist, Chilly Reception);
 * false when it is "" and nothing shows */
static bool pre_move_message(pr_battle *bt, int move, const pr_pokemon *user, const pr_pokemon *target, int mode,
                             pr_msg *out)
{
    switch (move) {
    case MOVE_ID_STRUGGLE:
        *out = pr_msg1("moveTriggers:struggle", pr_arg_pokemon(bt, "pokemonName", user));
        return true;
    case MOVE_ID_MAGNITUDE: {        /* "Magnitude N!": always a text, whatever the roll */
        static const int thresholds[] = {5, 15, 35, 65, 85, 95};
        int roll = pr_wave_seed_roll(bt, 100), m = 0;
        while (m < 6 && roll >= thresholds[m]) m++;
        *out = pr_msg1("moveTriggers:magnitudeMessage", pr_arg_int("magnitude", m + 4));
        return true;
    }
    case MOVE_ID_FICKLE_BEAM: {      /* doublePowerChanceMessageFunc(30): the same roll as its power */
        int roll = pr_wave_seed_roll(bt, 100);
        if (!(roll >= 0 && roll < 30)) return false;
        *out = pr_msg1("moveTriggers:goingAllOutForAttack", pr_arg_pokemon(bt, "pokemonName", user));
        return true;
    }
    case MOVE_ID_POLTERGEIST:        /* attackedByItemMessageFunc: the target's first transferable item */
        return pr_poltergeist_message(bt, target, out);
    case MOVE_ID_CHILLY_RECEPTION:   /* no text when called indirectly */
        if (mode >= MOVE_USE_MODE_INDIRECT) return false;
        *out = pr_msg1("moveTriggers:chillyReception", pr_arg_pokemon(bt, "pokemonName", user));
        return true;
    }
    pr_note_unported(bt, PR_AC_PreMoveMessageAttr);
    return false;
}

/* arena.isMoveWeatherCancelled(user, move): weather && !isEffectSuppressed() && Weather's own
 * check, which reads the move's type first whatever the weather: Harsh Sun stops Water attacks,
 * Heavy Rain Fire attacks */
static bool move_weather_cancelled(pr_battle *bt, const pr_pokemon *user, int move)
{
    if (bt->weather == WEATHER_TYPE_NONE || pr_weather_effect_suppressed(bt)) return false;
    int type = pr_get_move_type(bt, user, move);
    switch (bt->weather) {
    case WEATHER_TYPE_HARSH_SUN: return pr_move_is_attack(move) && type == POKEMON_TYPE_WATER;
    case WEATHER_TYPE_HEAVY_RAIN: return pr_move_is_attack(move) && type == POKEMON_TYPE_FIRE;
    }
    return false;
}

static void handle_pre_move_failures(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *m = PH(ph);
    if (!m->cancelled && !m->failed) return;
    m->entry.result = MOVE_RESULT_FAIL;
    pr_push_history_shared(bt, e, m->pokemon, m->entry, entry_list(m));
    pr_lapse_tags(bt, e, m->pokemon, BATTLER_TAG_LAPSE_TYPE_MOVE_EFFECT);
    move_queue_shift(m->pokemon, NULL);
}

static double pokemon_damage(pr_battle *bt, pr_engine *e, pr_pokemon *p, double damage, bool prevent_endure,
                             bool ignore_faint_phase);

/* ---- status effects (Status, Pokemon.cureStatus / resetStatus) ---- */

/* Pokemon.clearStatus(confusion, reloadAssets): status = null */
static void clear_status(pr_battle *bt, pr_engine *e, pr_pokemon *p, bool confusion)
{
    uint8_t last = p->status;
    p->status = 0;
    p->status_toxic_turns = p->status_sleep_turns = p->status_freeze_turns = -1;
    /* waking up ends a nightmare: lapseTag(NIGHTMARE), a CUSTOM lapse */
    if (last == STATUS_EFFECT_SLEEP) pr_lapse_tag(bt, e, p, BATTLER_TAG_TYPE_NIGHTMARE, BATTLER_TAG_LAPSE_TYPE_CUSTOM);
    if (confusion) pr_lapse_tag(bt, e, p, BATTLER_TAG_TYPE_CONFUSED, BATTLER_TAG_LAPSE_TYPE_CUSTOM);
}

/* Pokemon.resetStatus(revive, confusion = false, reloadAssets = false, asPhase = true) */
static void reset_status_as_phase(pr_battle *bt, pr_engine *e, pr_pokemon *p, bool revive, bool confusion)
{
    if (!revive && p->status == STATUS_EFFECT_FAINT) return;
    int ph = new_pokemon_phase(e, PH_RESET_STATUS, pr_battler_index(bt, p));
    if (ph < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); e->overflow = true; return; }
    PH(ph)->pokemon = p;
    PH(ph)->affect_confusion = confusion;
    pr_phase_unshift(bt, e, ph);
}

void pr_reset_status_phase_ex(pr_battle *bt, pr_engine *e, pr_pokemon *p, bool revive, bool confusion)
{
    reset_status_as_phase(bt, e, p, revive, confusion);
}

void pr_reset_status_phase(pr_battle *bt, pr_engine *e, pr_pokemon *p, bool revive)
{
    reset_status_as_phase(bt, e, p, revive, false);
}

/* Pokemon.cureStatus(effect, msg = getStatusEffectHealText(effect, name)): the cure message, then
 * the status is cleared at once */
static void cure_status(pr_battle *bt, pr_engine *e, pr_pokemon *p, int effect, const pr_msg *msg)
{
    if (effect != p->status) return;
    pr_queue_msg(bt, e, msg ? *msg : pr_status_msg(bt, effect, PR_ST_HEAL, p));
    clear_status(bt, e, p, false);
}

/* Status.incrementTurn(): the counters count down while truthy (undefined is -1 here) */
static void status_increment_turn(pr_pokemon *p)
{
    p->status_toxic_turns++;
    if (p->status_sleep_turns > 0) p->status_sleep_turns--;
    if (p->status_freeze_turns > 0) p->status_freeze_turns--;
}

/* MovePhase.triggerStatus(effect, cancel): the activation message and its animation */
static void trigger_status(pr_battle *bt, pr_engine *e, int ph, int effect, bool cancel)
{
    pr_phase *m = PH(ph);
    pr_queue_msg(bt, e, pr_status_msg(bt, effect, PR_ST_ACTIVATION, m->pokemon));
    pr_unshift_common_anim(bt, e, pr_battler_index(bt, m->pokemon), -1, COMMON_ANIM_POISON + (effect - 1));
    if (cancel) move_cancel(PH(ph));
}

/* move.findAttr(attr => attr.selfTarget && attr.is("HealStatusEffectAttr") && attr.isOfEffect(FREEZE)) */
static bool move_thaws_user(int move)
{
    const pr_move *mv = &pr_moves[move];
    for (unsigned i = 0; i < mv->nattr; i++) {
        unsigned k = mv->attr0 + i;
        if (!pr_attr_is(pr_attrs[k].cls, PR_AC_HealStatusEffectAttr) || !pr_attr_effects[k].self_target) continue;
        const pr_attr_HealStatusEffectAttr *x = (const pr_attr_HealStatusEffectAttr *)pr_attrs[k].p;
        for (unsigned j = 0; j < x->effects.n; j++)
            if (x->effects.v[j] == STATUS_EFFECT_FREEZE) return true;
    }
    return false;
}

/* MovePhase.checkSleep */
static bool check_sleep(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *m = PH(ph);
    pr_pokemon *user = m->pokemon;
    if (user->status != STATUS_EFFECT_SLEEP) return false;
    if (m->use_mode == MOVE_USE_MODE_INDIRECT) {       /* Dancer wakes its user up */
        reset_status_as_phase(bt, e, user, false, false);
        return false;
    }
    status_increment_turn(user);
    double turns = user->status_sleep_turns < 0 ? 0 : user->status_sleep_turns;
    pr_ab_params pp = {.pokemon = user, .move = -1, .value = &turns, .effect = STATUS_EFFECT_SLEEP};
    pr_apply_ab_attrs(bt, PR_AC_ReduceStatusEffectDurationAbAttr, &pp);   /* Early Bird */
    user->status_sleep_turns = (int16_t)turns;
    if (user->status_sleep_turns <= 0) {
        cure_status(bt, e, user, STATUS_EFFECT_SLEEP, NULL);
        return false;
    }
    /* applyMoveAttrs("BypassSleepAttr"): Sleep Talk, Snore */
    bool cancel = !pr_move_has_attr(m->move.id, PR_AC_BypassSleepAttr);
    trigger_status(bt, e, ph, STATUS_EFFECT_SLEEP, cancel);
    return cancel;
}

/* MovePhase.checkFreeze */
static bool check_freeze(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *m = PH(ph);
    pr_pokemon *user = m->pokemon;
    if (user->status != STATUS_EFFECT_FREEZE) return false;
    if (m->use_mode == MOVE_USE_MODE_INDIRECT) {
        reset_status_as_phase(bt, e, user, false, false);
        return false;
    }
    int move = m->move.id;
    bool thaws = move_thaws_user(move);
    int8_t ty[PR_MAX_TYPES];
    int n = pr_get_types(user, ty, PR_TYPES_ORIGINAL_IF_STELLAR);
    bool fire = false;
    for (int i = 0; i < n; i++) fire |= ty[i] == POKEMON_TYPE_FIRE;
    if (thaws && (move != MOVE_ID_BURN_UP || fire)) {   /* thawed after the other checks (doThawCheck) */
        m->thaw = true;
        return false;
    }
    status_increment_turn(user);
    if (thaws || !pr_battle_rand_seed_int(bt, 4, 0) || (user->status_freeze_turns < 0 ? 0 : user->status_freeze_turns) <= 0) {
        cure_status(bt, e, user, STATUS_EFFECT_FREEZE, NULL);
        return false;
    }
    trigger_status(bt, e, ph, STATUS_EFFECT_FREEZE, true);
    return true;
}

/* Pokemon.isSafeguarded(attacker): Safeguard on this side, unless the attacker's Infiltrator
 * (applied, not simulated) gets through */
static bool is_safeguarded(pr_battle *bt, const pr_pokemon *p, const pr_pokemon *attacker)
{
    int side = p->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY;
    if (!pr_arena_get_tag_on_side(bt, ARENA_TAG_TYPE_SAFEGUARD, side)) return false;
    bool bypassed = false;
    if (attacker) {
        pr_ab_params ip = {.pokemon = attacker, .move = -1, .flag = &bypassed};
        pr_apply_ab_attrs(bt, PR_AC_InfiltratorAbAttr, &ip);
    }
    return !bypassed;
}

bool pr_can_set_status(pr_battle *bt, pr_engine *e, pr_pokemon *p, int effect, bool quiet, bool override_status,
                       const pr_pokemon *source, bool ignore_field)
{
    if (effect != STATUS_EFFECT_FAINT) {
        /* Rest fails on its own status; anything else on any status, or one already on its way */
        if (override_status ? p->status == effect : (p->status || p->pending_status)) {
            /* queueStatusImmuneMessage(quiet, overrideStatus ? "overlap" : "other") */
            if (!quiet) pr_queue_msg(bt, e, override_status ? pr_status_msg(bt, p->status, PR_ST_OVERLAP, p)
                                                            : pr_msg_p(bt, "abilityTriggers:moveImmunity", p));
            return false;
        }
        if (pr_is_grounded(bt, p, false) && !ignore_field && bt->terrain == TERRAIN_MISTY) {
            if (!quiet) pr_queue_msg(bt, e, pr_terrain_block_msg(bt, TERRAIN_MISTY, p));
            return false;
        }
    }
    int8_t ty[PR_MAX_TYPES];
    int n = pr_get_types(p, ty, PR_TYPES_ORIGINAL_IF_STELLAR);
    bool immune = false;
    switch (effect) {
    case STATUS_EFFECT_POISON:
    case STATUS_EFFECT_TOXIC:
        /* Poison and Steel types, unless the source's Corrosion gets through (not simulated) */
        for (int i = 0; i < n && !immune; i++) {
            if (ty[i] != POKEMON_TYPE_POISON && ty[i] != POKEMON_TYPE_STEEL) continue;
            if (!source) { immune = true; break; }
            bool cancel = false;
            pr_ab_params cp = {.pokemon = source, .move = -1, .flag = &cancel, .effect = effect, .defender_type = ty[i]};
            pr_apply_ab_attrs(bt, PR_AC_IgnoreTypeStatusEffectImmunityAbAttr, &cp);
            immune = !cancel;
        }
        break;
    case STATUS_EFFECT_PARALYSIS: immune = pr_is_of_type(p, POKEMON_TYPE_ELECTRIC); break;
    case STATUS_EFFECT_SLEEP: immune = pr_is_grounded(bt, p, false) && bt->terrain == TERRAIN_ELECTRIC; break;
    case STATUS_EFFECT_FREEZE:
        immune = pr_is_of_type(p, POKEMON_TYPE_ICE)
                 || (!ignore_field && (bt->weather == WEATHER_TYPE_SUNNY || bt->weather == WEATHER_TYPE_HARSH_SUN));
        break;
    case STATUS_EFFECT_BURN: immune = pr_is_of_type(p, POKEMON_TYPE_FIRE); break;
    }
    if (immune) {
        /* the reason: Electric Terrain for sleep (whether or not it was the terrain), else "other" */
        if (!quiet) pr_queue_msg(bt, e, effect == STATUS_EFFECT_SLEEP ? pr_terrain_block_msg(bt, TERRAIN_ELECTRIC, p)
                                                                      : pr_msg_p(bt, "abilityTriggers:moveImmunity", p));
        return false;
    }
    /* the Pokemon's own abilities (Limber, Insomnia...), then its side's (itself included) */
    bool cancelled = false;
    pr_ab_params pp = {.pokemon = p, .move = -1, .flag = &cancelled, .effect = effect, .simulated = quiet};
    pr_apply_ab_attrs(bt, PR_AC_StatusEffectImmunityAbAttr, &pp);
    if (cancelled) return false;
    pr_speed_iter it;
    pr_pokemon *ally;
    pr_speed_iter_init(bt, &it, p->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY);
    while ((ally = pr_speed_iter_next(bt, &it))) {
        pr_ab_params ap = {.pokemon = ally, .opponent = source, .target = p, .move = -1, .flag = &cancelled,
                           .effect = effect, .simulated = quiet};
        pr_apply_ab_attrs(bt, PR_AC_UserFieldStatusEffectImmunityAbAttr, &ap);   /* Sweet Veil, Flower Veil */
        if (cancelled) return false;
    }
    if (source && source != p && is_safeguarded(bt, p, source)) {
        if (!quiet) pr_queue_msg(bt, e, pr_msg1("moveTriggers:safeguard", pr_arg_pokemon(bt, "targetName", p)));
        return false;
    }
    return true;
}

bool pr_try_set_status(pr_battle *bt, pr_engine *e, pr_pokemon *p, int effect, pr_pokemon *source, int sleep_turns,
                       bool override_status, bool quiet)
{
    return pr_try_set_status_msg(bt, e, p, effect, source, sleep_turns, override_status, quiet, NULL);
}

/* trySetStatus with the message the ObtainStatusEffectPhase shows instead of the plain obtain
 * text (a sourceText's obtainSource text, or overrideMessage) */
bool pr_try_set_status_msg(pr_battle *bt, pr_engine *e, pr_pokemon *p, int effect, pr_pokemon *source, int sleep_turns,
                           bool override_status, bool quiet, const pr_msg *msg)
{
    if (!effect) return false;
    if (!pr_can_set_status(bt, e, p, effect, quiet, override_status, source, false)) return false;
    if (p->hp <= 0 && effect != STATUS_EFFECT_FAINT) return false;
    /* falling asleep or freezing during its own multi-hit attack ends the attack */
    if ((effect == STATUS_EFFECT_SLEEP || effect == STATUS_EFFECT_FREEZE) && e->current >= 0
        && PH(e->current)->type == PH_MOVE_EFFECT && slot(bt, PH(e->current)->battler) == p) {
        p->hit_count = 1;
        p->hits_left = 1;
    }
    if (override_status) reset_status_as_phase(bt, e, p, false, false);
    else p->pending_status = (uint8_t)effect;
    int ph = new_pokemon_phase(e, PH_OBTAIN_STATUS_EFFECT, pr_battler_index(bt, p));
    if (ph < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); e->overflow = true; return true; }
    pr_phase *q = PH(ph);
    q->pokemon = p;
    q->ose_effect = (uint8_t)effect;
    q->ose_sleep_turns = (int8_t)sleep_turns;
    q->source_p = source;
    q->msg = msg ? *msg : pr_status_msg(bt, effect, PR_ST_OBTAIN, p);   /* fixed by the constructor */
    pr_phase_unshift(bt, e, ph);
    return true;
}

/* ObtainStatusEffectPhase.start */
static void obtain_status_effect_phase(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *q = PH(ph);
    pr_pokemon *p = slot(bt, q->battler);
    if (!p) { pr_note_unported(bt, PR_FEAT_PHASE_0 + PH_OBTAIN_STATUS_EFFECT); return; }
    int effect = q->ose_effect;
    /* doSetStatus(effect, sleepTurnsRemaining): 2 or 3 turns of sleep when not given */
    int sleep = q->ose_sleep_turns >= 0 ? q->ose_sleep_turns
                : effect == STATUS_EFFECT_SLEEP ? (pr_battle_rand_seed_int(bt, 3, 0) == 0 ? 2 : 3) : 0;
    p->pending_status = 0;
    if (effect == STATUS_EFFECT_SLEEP) {
        /* falling asleep while semi-invulnerable ends it and drops the queued move */
        static const int invuln[] = {BATTLER_TAG_TYPE_FLYING, BATTLER_TAG_TYPE_UNDERGROUND, BATTLER_TAG_TYPE_UNDERWATER,
                                     BATTLER_TAG_TYPE_HIDDEN};
        for (unsigned i = 0; i < sizeof invuln / sizeof *invuln; i++)
            if (pr_get_battler_tag(p, invuln[i])) pr_note_unported(bt, PR_FEAT_TAG_LAPSE);
    }
    p->status = (uint8_t)effect;
    p->status_toxic_turns = 0;
    p->status_sleep_turns = (int16_t)sleep;
    p->status_freeze_turns = effect == STATUS_EFFECT_FREEZE ? 3 : -1;
    /* the status animation plays, and its callback queues the message, then (no species changes
     * form on a status) */
    pr_event_add(bt, e, PR_EV_COMMON_ANIM, q->battler, -1, COMMON_ANIM_POISON + (effect - 1), 0, NULL);
    pr_queue_msg(bt, e, q->msg);
    if (effect && effect != STATUS_EFFECT_FAINT) {
        bt->ignore_abilities = false;                       /* arena.setIgnoreAbilities(false) */
        bt->ignoring_effect_source = -1;
        pr_ab_params pp = {.pokemon = p, .opponent = q->source_p, .move = -1, .effect = effect};
        pr_apply_ab_attrs(bt, PR_AC_PostSetStatusAbAttr, &pp);                   /* Synchronize */
        q = PH(ph);
        if (q->source_p) pr_hook_ab(bt, q->source_p, PR_AC_ConfusionOnStatusEffectAbAttr); /* Poison Puppeteer */
    }
}

/* firstFailureCheck, for a user with no tags (those checks report themselves) */
static bool first_failure_check(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *m = PH(ph);
    pr_pokemon *user = m->pokemon;
    const pr_move *mv = &pr_moves[m->move.id];
    if (check_sleep(bt, e, ph) || check_freeze(bt, e, ph)) return true;
    /* checkPP */
    if (mv->pp != -1 && m->use_mode < MOVE_USE_MODE_IGNORE_PP && ref_pp_used(m) >= ref_move_pp(m)) {
        move_cancel(m);
        pr_queue_msg(bt, e, pr_msg0("battle:attackFailed"));
        return true;
    }
    /* checkValidity */
    pr_target_list *tl = tlist(e, m->targets);
    if (mv->unimplemented) {
        move_cancel(m);
        pr_queue_msg(bt, e, pr_msg1("battle:moveNotImplemented", pr_arg_move("moveName", (int)(m->move.id | PR_MSG_MOVE_BARE))));
        return true;
    }
    if (m->move.id == MOVE_ID_NONE || !tl || tl->n == 0) {
        move_cancel(m);
        return true;
    }
    /* (challenges restricting moves are not modelled: classic mode has none) */
    if (pr_lapse_tag(bt, e, user, BATTLER_TAG_TYPE_TRUANT, BATTLER_TAG_LAPSE_TYPE_PRE_MOVE) && m->cancelled) return true;
    /* checkPreUseInterrupt (Focus Punch): each PreUseInterruptAttr whose condition holds cancels the
     * move with its message (the running phase is this MovePhase) */
    for (unsigned i = 0; i < mv->nattr; i++) {
        const pr_attr *a = &pr_attrs[mv->attr0 + i];
        if (a->cls != PR_AC_PreUseInterruptAttr) continue;
        pr_pokemon *tg[4];                                   /* getActiveTargetPokemon()[0] */
        int n = active_in_list(bt, tl, tg);
        if (!pr_closure_call(bt, ((const pr_attr_PreUseInterruptAttr *)a->p)->conditionFunc, PR_AC_PreUseInterruptAttr,
                             0, user, n ? tg[0] : NULL, m->move.id))
            continue;
        move_cancel(m);
        /* the attr's message: Focus Punch's is the only one */
        pr_queue_msg(bt, e, pr_msg1("moveTriggers:lostFocus", pr_arg_pokemon(bt, "pokemonName", user)));
        return true;
    }
    static const int tag_checks[] = {BATTLER_TAG_TYPE_FLINCHED, BATTLER_TAG_TYPE_DISABLED, BATTLER_TAG_TYPE_HEAL_BLOCK,
                                     BATTLER_TAG_TYPE_THROAT_CHOPPED};
    for (unsigned i = 0; i < sizeof tag_checks / sizeof *tag_checks; i++)
        if (pr_lapse_tag(bt, e, user, tag_checks[i], BATTLER_TAG_LAPSE_TYPE_PRE_MOVE) && m->cancelled) return true;
    /* checkGravity */
    if (pr_arena_get_tag_on_side(bt, ARENA_TAG_TYPE_GRAVITY, ARENA_TAG_SIDE_BOTH) && (mv->flags & MOVE_FLAGS_GRAVITY)) {
        pr_queue_msg(bt, e, pr_msg2("battle:moveDisabledGravity", pr_arg_pokemon(bt, "pokemonNameWithAffix", user),
                                    pr_arg_move("moveName", m->move.id)));
        return true;
    }
    static const int tag_checks2[] = {BATTLER_TAG_TYPE_TAUNT, BATTLER_TAG_TYPE_IMPRISON, BATTLER_TAG_TYPE_CONFUSED};
    for (unsigned i = 0; i < sizeof tag_checks2 / sizeof *tag_checks2; i++)
        if (pr_lapse_tag(bt, e, user, tag_checks2[i], BATTLER_TAG_LAPSE_TYPE_PRE_MOVE) && m->cancelled) return true;
    /* checkPara: fully paralysed one time in eight */
    if (user->status == STATUS_EFFECT_PARALYSIS && pr_battle_rand_seed_int(bt, 8, 0) == 0) {
        trigger_status(bt, e, ph, STATUS_EFFECT_PARALYSIS, true);
        return true;
    }
    if (pr_lapse_tag(bt, e, user, BATTLER_TAG_TYPE_INFATUATED, BATTLER_TAG_LAPSE_TYPE_PRE_MOVE) && m->cancelled) return true;
    return false;
}

/* getCounterAttackTarget(user, damageCategory): the newest received attack that isn't a
 * status move nor from an ally (and of the category, when given) */
int pr_counter_attack_target(pr_battle *bt, const pr_pokemon *user, int category /* -1 = any */)
{
    int ub = pr_battler_index(bt, user);
    int kept = pr_attacks_received_kept(user);
    for (int i = 0; i < kept; i++) {
        const pr_attack_record *r = &user->attacks_received[i];
        int cat = pr_moves[r->move].category;
        if (cat != MOVE_CATEGORY_STATUS && !pr_are_allies(r->source_battler_index, ub)
            && (category < 0 || cat == category))
            return r->source_battler_index;
    }
    pr_attacks_received_exhausted(bt, user);
    return -2;                                           /* null */
}

static void resolve_counter_attack_target(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *m = PH(ph);
    pr_target_list *tl = tlist(e, m->targets);
    if (!tl || tl->n != 1 || tl->v[0] != BATTLER_INDEX_ATTACKER) return;
    int target = BATTLER_INDEX_ATTACKER;
    const pr_move *mv = &pr_moves[m->move.id];
    for (unsigned i = 0; i < mv->nattr; i++) {
        const pr_attr *a = &pr_attrs[mv->attr0 + i];
        if (!pr_attr_is(a->cls, PR_AC_CounterRedirectAttr)) continue;
        const pr_attr_CounterRedirectAttr *c = a->p;
        int filter = (c->nulls & 2u) ? -1 : c->moveFilter; /* bit 1: moveFilter */
        int d = pr_counter_attack_target(bt, m->pokemon, filter);
        if (d != -2 && d != BATTLER_INDEX_ATTACKER) {
            /* in doubles, an attacker that is no longer active on the field: the first Pokemon of
             * its side's field with HP left, else ATTACKER */
            const pr_pokemon *dp = d >= 0 && d < 4 ? bt->field[d] : NULL;
            if (bt->double_battle && !is_active_on_field(dp)) {
                int side = d >= BATTLER_INDEX_ENEMY ? 2 : 0;
                d = BATTLER_INDEX_ATTACKER;
                for (int k = side; k < side + 2; k++)
                    if (bt->field[k] && bt->field[k]->hp > 0) { d = pr_battler_index(bt, bt->field[k]); break; }
            }
            target = d;
        }
    }
    tl->v[0] = (int8_t)target;
    pr_targets_changed(bt, e, m->targets);
    if (target == BATTLER_INDEX_ATTACKER) move_fail(m);
}

void pr_resolve_redirect_target(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *m = PH(ph);
    pr_target_list *tl = tlist(e, m->targets);
    if (!tl || tl->n != 1) return;
    /* the redirecting abilities (Lightning Rod, Storm Drain) of everyone but the user, in speed order */
    int current = tl->v[0], redirect = current;
    pr_speed_iter it;
    pr_pokemon *p;
    pr_speed_iter_init(bt, &it, ARENA_TAG_SIDE_BOTH);
    while ((p = pr_speed_iter_next(bt, &it)))
        if (p != m->pokemon) {
            pr_ab_params rp = {.pokemon = p, .opponent = m->pokemon, .move = m->move.id, .holder = &redirect};
            pr_apply_ab_attrs(bt, PR_AC_RedirectMoveAbAttr, &rp);
        }
    bool by_ability = current != redirect;
    /* center-of-attention tags on the opponents (Follow Me, Rage Powder, Spotlight) */
    for (int i = 0; i < 4; i++) {
        pr_pokemon *o = bt->field[i];
        if (!is_active_on_field(o) || !pr_is_opponent(bt, m->pokemon, o)) continue;
        const pr_battler_tag *tag = pr_get_battler_tag(o, BATTLER_TAG_TYPE_CENTER_OF_ATTENTION);
        if (tag && (tag->source_move != MOVE_ID_RAGE_POWDER
                    || (!pr_is_of_type(m->pokemon, POKEMON_TYPE_GRASS)
                        && !pr_has_ability_active(bt, m->pokemon, ABILITY_ID_OVERCOAT)))) {
            redirect = pr_battler_index(bt, o);
            by_ability = false;
        }
    }
    if (current == redirect) return;
    /* BypassRedirectAttr (Snipe Shot...: abilitiesOnly, only against an ability's redirect), then
     * BlockRedirectAbAttr on the user (Stalwart, Propeller Tail), shown and hidden */
    const pr_move *mv = &pr_moves[m->move.id];
    for (unsigned i = 0; i < mv->nattr; i++) {
        const pr_attr *a = &pr_attrs[mv->attr0 + i];
        if (pr_attr_is(a->cls, PR_AC_BypassRedirectAttr)
            && (!((const pr_attr_BypassRedirectAttr *)a->p)->abilitiesOnly || by_ability))
            redirect = current;
    }
    if (pr_has_ability_with_attr(bt, m->pokemon, PR_AC_BlockRedirectAbAttr)) {
        redirect = current;
        /* passive: getPassiveAbility().hasAttr(...), the species' passive whether unlocked or not */
        const pr_pokemon *u = m->pokemon;
        int pa = u->custom_passive >= 0 ? u->custom_passive : pr_pokemon_base_form(u)->passive;
        bool passive = pr_ability_first_attr((uint16_t)pa, PR_AC_BlockRedirectAbAttr) != NULL;
        pr_queue_ability_display(bt, e, u, passive, true);
        pr_queue_ability_display(bt, e, u, passive, false);
    }
    tl->v[0] = (int8_t)redirect;
    pr_targets_changed(bt, e, m->targets);
}

/* move.getFailedText(user, targets[0]): the attributes in order until one has a text. Only
 * ForceSwitchOutAttr's (and ChillyReceptionAttr's) does more than read state: it applies the
 * Suction Cups-style abilities of the target for real, so they show. The text itself isn't
 * modelled (a MessagePhase is a MessagePhase). */
/* HealAttr and its subclasses: selfTarget */
static bool heal_self_target(const pr_attr *a)
{
    switch (a->cls) {
    case PR_AC_HealAttr: return ((const pr_attr_HealAttr *)a->p)->selfTarget;
    case PR_AC_RestAttr: return ((const pr_attr_RestAttr *)a->p)->selfTarget;
    case PR_AC_VariableHealAttr: return ((const pr_attr_VariableHealAttr *)a->p)->selfTarget;
    case PR_AC_HealOnAllyAttr: return ((const pr_attr_HealOnAllyAttr *)a->p)->selfTarget;
    }
    return true;
}

/* failMove's text: move.getFailedText(user, targets[0], move) (the first attribute with one),
 * else getTerrainBlockMessage(targets[0], terrain) when the terrain stopped it, else
 * battle:attackFailed. ForceSwitchOutAttr's check applies the target's
 * ForceSwitchOutImmunityAbAttr for real (Suction Cups shows its bar). */
static pr_msg failed_text(pr_battle *bt, pr_engine *e, int ph, bool due_to_terrain)
{
    pr_phase *m = PH(ph);
    const pr_move *mv = &pr_moves[m->move.id];
    pr_pokemon *targets[4];
    int nt = active_in_list(bt, tlist(e, m->targets), targets);
    pr_pokemon *u = m->pokemon, *t = nt ? targets[0] : NULL;
    for (unsigned i = 0; i < mv->nattr; i++) {
        const pr_attr *a = &pr_attrs[mv->attr0 + i];
        if (a->cls == PR_AC_AddSubstituteAttr) {       /* Shed Tail, Substitute: a doll already, or too little HP */
            const pr_attr_AddSubstituteAttr *x = (const pr_attr_AddSubstituteAttr *)a->p;
            if (pr_get_battler_tag(u, BATTLER_TAG_TYPE_SUBSTITUTE))
                return pr_msg1("moveTriggers:substituteOnOverlap", pr_arg_pokemon(bt, "pokemonName", u));
            double c = pr_get_stat(u, STAT_HP, true) * x->hpCost;
            c = x->roundUp ? ceil(c) : (floor(c) < 1 ? 1 : floor(c));
            if (u->hp <= c) return pr_msg0("moveTriggers:substituteNotEnoughHp");
        } else if (pr_attr_is(a->cls, PR_AC_HealAttr)) {
            const pr_pokemon *h = heal_self_target(a) ? u : t;
            if (!h) { pr_note_unported(bt, PR_FEAT_MOVE_CONDITION); continue; }   /* upstream would throw */
            if (h->hp >= pr_get_stat(h, STAT_HP, true))
                return pr_msg1("battle:hpIsFull", pr_arg_pokemon(bt, "pokemonName", h));
        } else if (pr_attr_is(a->cls, PR_AC_ForceSwitchOutAttr)) {
            if (!t) { pr_note_unported(bt, PR_FEAT_MOVE_CONDITION); continue; }   /* upstream would throw */
            bool cancelled = false;
            pr_ab_params ap = {.pokemon = t, .move = -1, .flag = &cancelled};
            pr_apply_ab_attrs(bt, PR_AC_ForceSwitchOutImmunityAbAttr, &ap);
            if (cancelled) return pr_msg1("moveTriggers:cannotBeSwitchedOut", pr_arg_pokemon(bt, "pokemonName", t));
        }
    }
    if (due_to_terrain) return pr_terrain_block_msg(bt, bt->terrain, t);
    return pr_msg0("battle:attackFailed");
}

static void fail_move(pr_battle *bt, pr_engine *e, int ph, bool due_to_terrain)
{
    pr_phase *m = PH(ph);
    pr_pokemon *user = m->pokemon;
    int id = m->move.id;
    if (due_to_terrain || id == MOVE_ID_ROAR || id == MOVE_ID_WHIRLWIND || id == MOVE_ID_TRICK_OR_TREAT
        || id == MOVE_ID_FORESTS_CURSE) {
        pr_pokemon *targets[4];
        int nt = active_in_list(bt, tlist(e, m->targets), targets);
        pr_ab_params params = {.pokemon = user, .opponent = nt ? targets[0] : NULL, .move = id};
        pr_apply_ab_attrs(bt, PR_AC_PokemonTypeChangeAbAttr, &params);
    }
    pr_turn_move tm = {.move = (uint16_t)id, .use_mode = m->use_mode, .result = MOVE_RESULT_FAIL};
    pr_push_history_shared(bt, e, user, tm, m->targets);
    pr_queue_msg(bt, e, failed_text(bt, e, ph, due_to_terrain));
    pr_lapse_tags(bt, e, user, BATTLER_TAG_LAPSE_TYPE_MOVE_EFFECT);
}

static bool third_failure_check(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *m = PH(ph);
    pr_pokemon *user = m->pokemon;
    pr_pokemon *targets[4];
    int nt = active_in_list(bt, tlist(e, m->targets), targets);
    bool fails_conditions = !pr_move_apply_conditions(bt, m->move.id, user, nt ? targets[0] : NULL, 3);
    bool terrain = false;
    if (bt->terrain == TERRAIN_PSYCHIC) {
        /* isMoveTerrainCancelled: a priority move at a grounded opponent in Psychic Terrain */
        if (!is_field_targeted(m->move.id) && pr_move_get_priority(bt, m->move.id, user) > 0) {
            pr_target_list *tl = tlist(e, m->targets);
            for (int i = 0; i < 4 && !terrain; i++) {
                pr_pokemon *o = bt->field[i];
                if (!is_active(o) || !pr_is_opponent(bt, user, o)) continue;
                bool listed = false;
                for (int k = 0; tl && k < tl->n; k++) listed |= tl->v[k] == i;
                if (listed && pr_is_grounded(bt, o, false)) terrain = true;
            }
        }
    }
    bool failed = fails_conditions || terrain;
    if (!failed && nt && pr_is_opponent(bt, user, targets[0])) {
        /* Source walks the defending field with one shared cancellation holder. */
        bool cancelled = false;
        for (int i = 0; i < 4; i++) {
            pr_pokemon *o = bt->field[i];
            if (o && (i < 2) != user->is_player) {
                pr_ab_params params = {.pokemon = o, .opponent = user, .move = m->move.id, .flag = &cancelled};
                pr_apply_ab_attrs(bt, PR_AC_FieldPriorityMoveImmunityAbAttr, &params);
            }
        }
        failed = cancelled;
    }
    if (failed) {
        fail_move(bt, e, ph, terrain);
        return true;
    }
    return false;
}

static void use_move(pr_battle *bt, pr_engine *e, int ph, bool charging)
{
    pr_phase *m = PH(ph);
    pr_pokemon *user = m->pokemon;
    pr_turn_move q;
    if (user->n_move_queue) {
        move_queue_shift(user, &q);
        m->use_mode = q.use_mode;
    }
    const pr_battler_tag *ch = pr_get_battler_tag(user, BATTLER_TAG_TYPE_CHARGING);
    if (!charging && ch && ch->source_move == m->move.id) pr_lapse_tag(bt, e, user, BATTLER_TAG_TYPE_CHARGING, BATTLER_TAG_LAPSE_TYPE_CUSTOM);
    if (third_failure_check(bt, e, ph)) return;
    pr_pokemon *targets[4];
    int nt = active_in_list(bt, tlist(e, m->targets), targets);
    pr_ab_params type_change = {.pokemon = user, .opponent = nt ? targets[0] : NULL, .move = m->move.id};
    pr_apply_ab_attrs(bt, PR_AC_PokemonTypeChangeAbAttr, &type_change);
    if (!pr_move_apply_conditions(bt, m->move.id, user, nt ? targets[0] : NULL, 4)) {
        fail_move(bt, e, ph, false);
        return;
    }
    if (charging) {
        /* chargeMove: MoveChargePhase(battlerIndex, targets[0], move, NORMAL ? IGNORE_PP : useMode) */
        int c = new_pokemon_phase(e, PH_MOVE_CHARGE, (int8_t)pr_battler_index(bt, user));   /* user.getBattlerIndex() */
        if (c < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return; }
        pr_target_list *tl = tlist(e, m->targets);
        PH(c)->pokemon = user;
        PH(c)->move = m->move;
        PH(c)->move_id = m->move.id;
        PH(c)->charge_target = tl && tl->n ? tl->v[0] : -1;
        PH(c)->use_mode = m->use_mode == MOVE_USE_MODE_NORMAL ? MOVE_USE_MODE_IGNORE_PP : m->use_mode;
        pr_phase_unshift(bt, e, c);
        return;
    }
    /* executeMove: unshiftNew("MoveEffectPhase", battlerIndex, targets, move, useMode) */
    /* user.getBattlerIndex(): the user's field slot now, which a ToggleDoublePositionPhase since
     * the MovePhase was made can have changed */
    int fx = new_pokemon_phase(e, PH_MOVE_EFFECT, (int8_t)pr_battler_index(bt, user));
    if (fx < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return; }
    PH(fx)->targets = m->targets;
    PH(fx)->move_id = m->move.id;
    PH(fx)->use_mode = m->use_mode;
    /* the constructor drops the user from an ALL_NEAR_OTHERS list (a doubles slot that took a
     * fainted ally's index): targets.splice(i, i + 1) on the shared array, which removes i + 1
     * entries from i */
    pr_target_list *tl = tlist(e, m->targets);
    if (tl && pr_moves[m->move.id].target == MOVE_TARGET_ALL_NEAR_OTHERS)
        for (int k = 0; k < tl->n; k++)
            if (tl->v[k] == PH(fx)->battler) {
                int del = k + 1 < tl->n - k ? k + 1 : tl->n - k;
                memmove(&tl->v[k], &tl->v[k + del], (size_t)(tl->n - k - del));
                tl->n = (uint8_t)(tl->n - del);
                pr_targets_changed(bt, e, m->targets);
                break;
            }
    PH(fx)->n_hit_checks = tl ? tl->n : 0;
    for (int k = 0; k < PH(fx)->n_hit_checks && k < 4; k++) {
        PH(fx)->hit_check[k] = HIT_CHECK_RESULT_PENDING;
        PH(fx)->hit_eff[k] = 0;
    }
    pr_phase_unshift(bt, e, fx);
}

static void move_phase(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *m = PH(ph);
    pr_pokemon *user = m->pokemon;
    if (!is_active_on_field(user)) {
        queue_move_end(bt, e, ph);
        return;
    }
    int mode = m->use_mode;
    bool ignore_status = mode >= MOVE_USE_MODE_FOLLOW_UP, follow_up = mode == MOVE_USE_MODE_FOLLOW_UP;

    pr_remove_tag(bt, e, user, BATTLER_TAG_TYPE_ALWAYS_GET_HIT);
    pr_remove_tag(bt, e, user, BATTLER_TAG_TYPE_RECEIVE_DOUBLE_DAMAGE);
    /* an Encore added this turn overrides the move and its targets (the history entry keeps
     * the old targets array) */
    int encore_slot = pr_encore_try_override(user);
    if (encore_slot >= 0) {
        int move = PR_MOVES(user)[encore_slot];
        int8_t tg[4], mt[4];
        int n = 1;
        if (pr_moves[move].target == MOVE_TARGET_USER_OR_NEAR_ALLY) {
            tg[0] = (int8_t)pr_battler_index(bt, user);          /* Acupressure: itself */
        } else {
            bool multiple;
            int nm = pr_get_move_targets(bt, user, move, -1, mt, &multiple);
            if (multiple || nm == 1) {
                memcpy(tg, mt, (size_t)nm);
                n = nm;
            } else {
                int k = pr_battle_rand_seed_int(bt, nm, 0);
                if (k >= nm) {                                    /* [undefined] */
                    pr_note_unported(bt, (uint16_t)(PR_FEAT_TAG_0 + BATTLER_TAG_TYPE_ENCORE));
                    n = 0;
                } else {
                    tg[0] = mt[k];
                }
            }
        }
        if (m->entry_targets == -2) m->entry_targets = m->targets;
        m->move.id = (uint16_t)move;
        m->move.slot = (int8_t)encore_slot;
        m->move.from_transform = PR_HAS_SUMMON_MOVES(user);
        m->move.pp_used = 0;
        m->targets = (int8_t)pr_targets_new(e, tg, n);
    }
    user->acted = true;

    if (!ignore_status) {
        first_failure_check(bt, e, ph);
        pr_lapse_tags(bt, e, user, BATTLER_TAG_LAPSE_TYPE_PRE_MOVE);
    } else if (follow_up) {
        /* followUpMoveFirstFailureCheck: Heal Block, Throat Chop, then Gravity (its message
         * doesn't cancel the move, as in the first failure check) */
        if (!(pr_lapse_tag(bt, e, user, BATTLER_TAG_TYPE_HEAL_BLOCK, BATTLER_TAG_LAPSE_TYPE_PRE_MOVE) && PH(ph)->cancelled)
            && !(pr_lapse_tag(bt, e, user, BATTLER_TAG_TYPE_THROAT_CHOPPED, BATTLER_TAG_LAPSE_TYPE_PRE_MOVE) && PH(ph)->cancelled)
            && pr_arena_get_tag_on_side(bt, ARENA_TAG_TYPE_GRAVITY, ARENA_TAG_SIDE_BOTH)
            && (pr_moves[PH(ph)->move.id].flags & MOVE_FLAGS_GRAVITY))
            pr_queue_msg(bt, e, pr_msg2("battle:moveDisabledGravity", pr_arg_pokemon(bt, "pokemonNameWithAffix", user),
                                        pr_arg_move("moveName", PH(ph)->move.id)));
        m = PH(ph);
    }
    if (m->cancelled) {
        handle_pre_move_failures(bt, e, ph);
        queue_move_end(bt, e, ph);
        return;
    }
    /* doThawCheck: the move thaws its user out once the checks have passed */
    if (!follow_up && !ignore_status && m->thaw) {
        pr_msg healed = pr_msg2("statusEffect:freeze.healByMove", pr_arg_pokemon(bt, "pokemonNameWithAffix", user),
                                pr_arg_move("moveName", m->move.id));
        cure_status(bt, e, user, STATUS_EFFECT_FREEZE, &healed);
    }

    int move = m->move.id;
    /* arena.ignoreAbilities (Sunsteel Strike and co. don't work when called indirectly) */
    if (pr_move_does_flag_effect_apply_ex(bt, move, MOVE_FLAGS_IGNORE_ABILITIES, user, NULL, mode >= MOVE_USE_MODE_INDIRECT)) {
        bt->ignore_abilities = true;
        bt->ignoring_effect_source = (int8_t)pr_battler_index(bt, user);
    }

    pr_resolve_redirect_target(bt, e, ph);
    resolve_counter_attack_target(bt, e, ph);

    bool charging_move = is_charging_move(move);
    bool charging = charging_move && !pr_get_battler_tag(user, BATTLER_TAG_TYPE_CHARGING);
    bool releasing = charging_move && !charging;
    if (!releasing && mode < MOVE_USE_MODE_IGNORE_PP) {
        /* usePP: Pressure on each active target adds to the PP used */
        pr_pokemon *targets[4];
        int nt = active_in_list(bt, tlist(e, m->targets), targets);
        double pp = 1;
        for (int i = 0; i < nt; i++) {
            pr_ab_params ap = {.pokemon = targets[i], .opponent = user, .move = -1, .value = &pp};
            pr_apply_ab_attrs(bt, PR_AC_IncreasePpUsedAbAttr, &ap);
        }
        ref_use_pp(m, (int)pp);
    }
    if (!follow_up) form_change(bt, user, PR_FCT_SpeciesFormChangePreMoveTrigger);

    /* showMoveText */
    if (move != MOVE_ID_NONE && !pr_get_battler_tag(user, BATTLER_TAG_TYPE_RECHARGING)
        && !pr_get_battler_tag(user, BATTLER_TAG_TYPE_INTERRUPTED)) {
        m->entry.move = (uint16_t)move;
        pr_queue_msg(bt, e, pr_msg2(mode == MOVE_USE_MODE_REFLECTED ? "battle:magicCoatActivated" : "battle:useMove",
                                    pr_arg_pokemon(bt, "pokemonNameWithAffix", user), pr_arg_move("moveName", move)));
        if (pr_move_has_attr((uint16_t)move, PR_AC_PreMoveMessageAttr)) {
            /* applyMoveAttrs("PreMoveMessageAttr", pokemon, getActiveTargetPokemon()[0], move) */
            pr_pokemon *tg[4];
            pr_msg pm;
            int n = active_in_list(bt, tlist(e, m->targets), tg);
            if (pre_move_message(bt, move, user, n ? tg[0] : NULL, mode, &pm)) pr_queue_msg(bt, e, pm);
        }
    }

    /* secondFailureCheck */
    pr_pokemon *targets[4];
    int nt = active_in_list(bt, tlist(e, m->targets), targets);
    bool second_failed = false;
    pr_msg second_text = pr_msg0("battle:attackFailed");
    if (!pr_move_apply_conditions(bt, move, user, nt ? targets[0] : NULL, 2)) {
        if (pr_move_has_attr((uint16_t)move, PR_AC_HealOnAllyAttr))
            second_text = pr_msg2("battle:moveDisabledHealBlock", pr_arg_pokemon(bt, "pokemonNameWithAffix", user),
                                  pr_arg_move("moveName", move));
        m->failed = true;
        second_failed = true;
    } else if (move_weather_cancelled(bt, user, move)) {
        second_text = pr_msg0(pr_weather_key(bt->weather, PR_WEATHER_BLOCK));
        m->failed = true;
        second_failed = true;
    } else {
        pr_lapse_tag(bt, e, user, BATTLER_TAG_TYPE_POWDER, BATTLER_TAG_LAPSE_TYPE_PRE_MOVE);
        second_failed = m->failed;
        if (second_failed) {
            handle_pre_move_failures(bt, e, ph);
            queue_move_end(bt, e, ph);
            return;
        }
    }
    if (second_failed && m->failed) {
        pr_queue_msg(bt, e, second_text);
        handle_pre_move_failures(bt, e, ph);
        queue_move_end(bt, e, ph);
        return;
    }

    if (!pr_move_has_attr((uint16_t)move, PR_AC_CopyMoveAttr) && mode != MOVE_USE_MODE_REFLECTED)
        bt->last_move = (uint16_t)move;

    /* resolveFinalPreMoveCancellationChecks */
    nt = active_in_list(bt, tlist(e, m->targets), targets);
    if ((nt == 0 && !pr_move_has_attr((uint16_t)move, PR_AC_AddArenaTrapTagAttr))
        || (user->n_move_queue > 0 && user->move_queue[0].move == MOVE_ID_NONE)) {
        pr_queue_msg(bt, e, pr_msg0("battle:attackFailed"));
        move_fail(m);
        move_queue_shift(user, NULL);
        pr_push_history_shared(bt, e, user, m->entry, entry_list(m));
    } else {
        pr_lapse_tags(bt, e, user, BATTLER_TAG_LAPSE_TYPE_MOVE);
        use_move(bt, e, ph, charging);
    }
    queue_move_end(bt, e, ph);
}

/* ---------------------------------------------------------------- MoveChargePhase */

/* ChargingMove: chargeAnim (ChargeAnim[`${MoveId}_CHARGING`]) and the chargeText key; showChargeText
 * fills {USER} and {TARGET} with getPokemonNameWithAffix */
static const struct { uint16_t move; uint16_t anim; const char *key; } charge_texts[] = {
    {MOVE_ID_RAZOR_WIND, CHARGE_ANIM_RAZOR_WIND_CHARGING, "moveTriggers:whippedUpAWhirlwind"},
    {MOVE_ID_FLY, CHARGE_ANIM_FLY_CHARGING, "moveTriggers:flewUpHigh"},
    {MOVE_ID_SOLAR_BEAM, CHARGE_ANIM_SOLAR_BEAM_CHARGING, "moveTriggers:tookInSunlight"},
    {MOVE_ID_DIG, CHARGE_ANIM_DIG_CHARGING, "moveTriggers:dugAHole"},
    {MOVE_ID_SKULL_BASH, CHARGE_ANIM_SKULL_BASH_CHARGING, "moveTriggers:loweredItsHead"},
    {MOVE_ID_SKY_ATTACK, CHARGE_ANIM_SKY_ATTACK_CHARGING, "moveTriggers:isGlowing"},
    {MOVE_ID_DIVE, CHARGE_ANIM_DIVE_CHARGING, "moveTriggers:hidUnderwater"},
    {MOVE_ID_BOUNCE, CHARGE_ANIM_BOUNCE_CHARGING, "moveTriggers:sprangUp"},
    {MOVE_ID_SHADOW_FORCE, CHARGE_ANIM_SHADOW_FORCE_CHARGING, "moveTriggers:vanishedInstantly"},
    {MOVE_ID_SKY_DROP, CHARGE_ANIM_SKY_DROP_CHARGING, "moveTriggers:tookTargetIntoSky"},
    {MOVE_ID_FREEZE_SHOCK, CHARGE_ANIM_FREEZE_SHOCK_CHARGING, "moveTriggers:becameCloakedInFreezingLight"},
    {MOVE_ID_ICE_BURN, CHARGE_ANIM_ICE_BURN_CHARGING, "moveTriggers:becameCloakedInFreezingAir"},
    {MOVE_ID_PHANTOM_FORCE, CHARGE_ANIM_PHANTOM_FORCE_CHARGING, "moveTriggers:vanishedInstantly"},
    {MOVE_ID_GEOMANCY, CHARGE_ANIM_GEOMANCY_CHARGING, "moveTriggers:isChargingPower"},
    {MOVE_ID_SOLAR_BLADE, CHARGE_ANIM_SOLAR_BLADE_CHARGING, "moveTriggers:isGlowing"},
    {MOVE_ID_METEOR_BEAM, CHARGE_ANIM_METEOR_BEAM_CHARGING, "moveTriggers:isOverflowingWithSpacePower"},
    {MOVE_ID_ELECTRO_SHOT, CHARGE_ANIM_ELECTRO_SHOT_CHARGING, "moveTriggers:absorbedElectricity"},
};


/* The charging turn of a two-turn move (Fly, Solar Beam...): the charge text, the move's charge
 * effects (semi-invulnerability, a stat raise), the CHARGING tag; then either the move runs at
 * once (Solar Beam in sun) or waits in the move queue for next turn. */
static void move_charge_phase(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *c = PH(ph);
    pr_pokemon *user = slot(bt, c->battler);
    int move = c->move.id;
    /* getTargetPokemon: getField(true).find(p => targetIndex === p.getBattlerIndex()) */
    pr_pokemon *target = slot(bt, c->charge_target);
    if (target && !is_active(target)) target = NULL;              /* isActive(): on the field or not */
    if (!user || !target || !is_charging_move(move)) return;     /* super.end() only */
    const pr_move *mv = &pr_moves[move];

    /* the charge animation's callback: showChargeText, applyMoveChargeAttrs("MoveEffectAttr"),
     * addTag(CHARGING, 1, move.id, user.id) */
    pr_msg text = pr_msg0(NULL);
    for (size_t i = 0; i < sizeof charge_texts / sizeof charge_texts[0]; i++) {
        if (charge_texts[i].move != move) continue;
        pr_event_add(bt, e, PR_EV_CHARGE_ANIM, c->battler, c->charge_target, charge_texts[i].anim, 0, NULL);
        text = pr_msg2(charge_texts[i].key, pr_arg_pokemon(bt, "pokemonName", user),
                       pr_arg_pokemon(bt, "targetName", target));
    }
    pr_queue_msg(bt, e, text);
    for (unsigned i = 0; i < mv->ncattr; i++)
        if (pr_attr_is(pr_attrs[mv->cattr0 + i].cls, PR_AC_MoveEffectAttr))
            pr_apply_move_effect(bt, e, ph, mv->cattr0 + i, user, target);
    pr_add_tag(bt, e, user, BATTLER_TAG_TYPE_CHARGING, 1, move, user->id);

    /* end(): applyMoveChargeAttrs("InstantChargeAttr", user, null, move, instantCharge) */
    c = PH(ph);
    bool instant = false;
    for (unsigned i = 0; i < mv->ncattr; i++) {
        const pr_attr *a = &pr_attrs[mv->cattr0 + i];
        if (!pr_attr_is(a->cls, PR_AC_InstantChargeAttr)) continue;
        if (a->cls != PR_AC_WeatherInstantChargeAttr) { pr_note_unported(bt, a->cls); continue; }
        /* the condition: getEffectiveWeatherForMove(user) is one of weatherTypes */
        const pr_attr_WeatherInstantChargeAttr *x = (const pr_attr_WeatherInstantChargeAttr *)a->p;
        int w = pr_get_effective_weather_for_move(bt, user);
        for (unsigned k = 0; k < x->weatherTypes.n; k++)
            if (x->weatherTypes.v[k] == w) instant = true;
    }
    pr_turn_move tm = {.move = (uint16_t)move, .use_mode = c->use_mode, .result = PR_MOVE_RESULT_UNDEFINED,
                       .targets = {c->charge_target}, .n_targets = 1};
    if (instant) {
        /* the pending MoveEndPhase goes; MovePhase(user, [targetIndex], move, useMode, FIRST) */
        pr_phase_try_remove(bt, e, PH_MOVE_END, c->battler);
        int mp = new_pokemon_phase(e, PH_MOVE, c->battler);
        if (mp < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return; }
        c = PH(ph);
        pr_phase *m = PH(mp);
        m->pokemon = user;
        m->move = c->move;
        m->use_mode = c->use_mode;
        m->timing = MOVE_PHASE_TIMING_MODIFIER_FIRST;
        m->targets = (int8_t)pr_targets_new(e, &c->charge_target, 1);
        m->entry.move = MOVE_ID_NONE;
        m->entry.use_mode = c->use_mode;
        m->entry.result = PR_MOVE_RESULT_UNDEFINED;
        pr_phase_unshift(bt, e, mp);
    } else if (user->n_move_queue < PR_MAX_MOVE_QUEUE) {
        user->move_queue[user->n_move_queue++] = tm;              /* pushMoveQueue */
    } else {
        pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT);
    }
    /* pushMoveHistory({move, targets: [targetIndex], result: OTHER, useMode}) */
    tm.result = MOVE_RESULT_OTHER;
    pr_push_history_shared(bt, e, user, tm, -1);
}

/* ---------------------------------------------------------------- MoveEffectPhase */

static bool me_first_target(pr_battle *bt, pr_engine *e, int ph, pr_pokemon **out)
{
    pr_pokemon *t[4];
    int n = active_in_list(bt, tlist(e, PH(ph)->targets), t);
    if (out) *out = n ? t[0] : NULL;
    return n > 0;
}

/* Pokemon.getAccuracyMultiplier(target, move) */
static double accuracy_multiplier(pr_battle *bt, pr_pokemon *user, pr_pokemon *target, int move)
{
    if (pr_move_has_attr((uint16_t)move, PR_AC_OneHitKOAccuracyAttr)) return 1;
    double acc_stage = user->stat_stages[STAT_ACC - 1];
    double eva_stage = target->stat_stages[STAT_EVA - 1];
    bool ignore_acc = false, ignore_eva = false;
    pr_ab_params ia = {.pokemon = target, .stat = STAT_ACC, .flag = &ignore_acc, .move = -1};
    pr_apply_ab_attrs(bt, PR_AC_IgnoreOpponentStatStagesAbAttr, &ia);
    pr_ab_params ie = {.pokemon = user, .stat = STAT_EVA, .flag = &ignore_eva, .move = -1};
    pr_apply_ab_attrs(bt, PR_AC_IgnoreOpponentStatStagesAbAttr, &ie);
    if (pr_move_has_attr((uint16_t)move, PR_AC_IgnoreOpponentStatStagesAttr)) ignore_eva = true;
    pr_apply_temp_stat_stage_booster_modifiers(bt, user->is_player, STAT_ACC, &acc_stage);
    acc_stage = ignore_acc ? 0 : (acc_stage < 6 ? acc_stage : 6);
    eva_stage = ignore_eva ? 0 : eva_stage;
    if (pr_find_battler_tag_class(target, PR_TAGCLASS_EXPOSED)) eva_stage = eva_stage < 0 ? eva_stage : 0;
    double acc = 1;
    if (acc_stage != eva_stage)
        acc = acc_stage > eva_stage ? (3 + fmin(acc_stage - eva_stage, 6)) / 3 : 3 / (3 + fmin(eva_stage - acc_stage, 6));
    pr_ab_params sa = {.pokemon = user, .stat = STAT_ACC, .value = &acc, .move = move};
    pr_apply_ab_attrs(bt, PR_AC_StatMultiplierAbAttr, &sa);
    double eva = 1;
    pr_ab_params se = {.pokemon = target, .stat = STAT_EVA, .value = &eva, .move = move};
    pr_apply_ab_attrs(bt, PR_AC_StatMultiplierAbAttr, &se);
    /* the user's ally's AllyStatMultiplierAbAttr (Victory Star), on both multipliers */
    const pr_pokemon *ally = pr_get_ally(bt, user);
    if (ally) {
        bool ignore = pr_has_ability_with_attr(bt, user, PR_AC_MoveAbilityBypassAbAttr)
                      || (pr_moves[move].flags & MOVE_FLAGS_IGNORE_ABILITIES);
        pr_ab_params la = {.pokemon = ally, .stat = STAT_ACC, .value = &acc, .ignore_ability = ignore, .move = move};
        pr_apply_ab_attrs(bt, PR_AC_AllyStatMultiplierAbAttr, &la);
        pr_ab_params le = la;
        le.stat = STAT_EVA;
        le.value = &eva;
        pr_apply_ab_attrs(bt, PR_AC_AllyStatMultiplierAbAttr, &le);
    }
    return acc / eva;
}

/* Move.calculateBattleAccuracy(user, target) */
double pr_calculate_battle_accuracy(pr_battle *bt, pr_pokemon *user, pr_pokemon *target, int move)
{
    double acc = pr_moves[move].accuracy;
    const pr_move *mv = &pr_moves[move];
    for (unsigned i = 0; i < mv->nattr; i++) {
        uint16_t cls = pr_attrs[mv->attr0 + i].cls;
        if (!pr_attr_is(cls, PR_AC_VariableAccuracyAttr)) continue;
        if (pr_attr_is(cls, PR_AC_SheerColdAccuracyAttr)) {
            if (user->level < target->level) acc = 0;
            else {
                double base = pr_is_of_type(user, POKEMON_TYPE_ICE) ? 30 : 20;
                acc = fmin(fmax(base + 100 * (1 - (double)target->level / user->level), 0), 100);
            }
        } else if (pr_attr_is(cls, PR_AC_OneHitKOAccuracyAttr)) {
            if (user->level < target->level) acc = 0;
            else acc = fmin(fmax(30 + 100 * (1 - (double)target->level / user->level), 0), 100);
        } else if (pr_attr_is(cls, PR_AC_ToxicAccuracyAttr)) {
            if (pr_is_of_type(user, POKEMON_TYPE_POISON)) acc = -1;
        } else if (pr_attr_is(cls, PR_AC_AlwaysHitMinimizeAttr)) {
            if (pr_get_battler_tag(target, BATTLER_TAG_TYPE_MINIMIZED)) acc = -1;
        } else if (pr_attr_is(cls, PR_AC_ThunderAccuracyAttr)) {
            int w = pr_get_effective_weather_for_move(bt, user);
            if (w == WEATHER_TYPE_SUNNY || w == WEATHER_TYPE_HARSH_SUN) acc = 50;
            else if (w == WEATHER_TYPE_RAIN || w == WEATHER_TYPE_HEAVY_RAIN) acc = -1;
        } else if (pr_attr_is(cls, PR_AC_StormAccuracyAttr)) {
            int w = pr_get_effective_weather_for_move(bt, user);
            if (w == WEATHER_TYPE_RAIN || w == WEATHER_TYPE_HEAVY_RAIN) acc = -1;
        } else if (pr_attr_is(cls, PR_AC_BlizzardAccuracyAttr)) {
            int w = pr_get_effective_weather_for_move(bt, user);
            if (w == WEATHER_TYPE_HAIL || w == WEATHER_TYPE_SNOW) acc = -1;
        } else if (cls != PR_AC_VariableAccuracyAttr) {
            pr_note_unported(bt, cls);
        }
    }
    pr_ab_params wonder = {.pokemon = target, .opponent = user, .move = move, .value = &acc};
    pr_apply_ab_attrs(bt, PR_AC_WonderSkinAbAttr, &wonder);
    if (acc == -1) return acc;
    bool ohko = pr_move_has_attr((uint16_t)move, PR_AC_OneHitKOAccuracyAttr);
    if (!ohko) {
        pr_apply_accuracy_booster_modifiers(bt, user, &acc);
        turn_modifiers(bt, user->is_player, user);
    }
    if (bt->weather == WEATHER_TYPE_FOG) acc = floor(acc * 0.9);
    if (!ohko && pr_arena_get_tag_on_side(bt, ARENA_TAG_TYPE_GRAVITY, ARENA_TAG_SIDE_BOTH)) acc = floor(acc * 1.67);
    return acc;
}

static bool check_bypass_acc_and_invuln(pr_battle *bt, pr_pokemon *user, pr_pokemon *target, int move)
{
    if (pr_has_ability_with_attr(bt, user, PR_AC_AlwaysHitAbAttr) || pr_has_ability_with_attr(bt, target, PR_AC_AlwaysHitAbAttr))
        return true;
    if (pr_move_has_attr((uint16_t)move, PR_AC_ToxicAccuracyAttr) && pr_is_of_type(user, POKEMON_TYPE_POISON)) return true;
    if (pr_get_battler_tag(user, BATTLER_TAG_TYPE_IGNORE_ACCURACY)) {
        /* getLastXMoves(-1).find(m => m.move === LOCK_ON || m.move === MIND_READER)?.targets
         * includes the target */
        for (int k = 0; k < user->n_move_history; k++) {
            const pr_turn_move *tm = pr_last_move(bt, user, k);
            if (!tm) break;                                        /* older than the kept ones: reported */
            if (tm->move != MOVE_ID_LOCK_ON && tm->move != MOVE_ID_MIND_READER) continue;
            for (int i = 0; i < tm->n_targets; i++)
                if (tm->targets[i] == pr_battler_index(bt, target)) return true;
            break;
        }
    }
    return is_field_targeted(move);
}

/* MoveEffectPhase.hitCheck(target) */
static int hit_check(pr_battle *bt, pr_engine *e, int ph, pr_pokemon *target, double *eff_out)
{
    pr_phase *fx = PH(ph);
    pr_pokemon *user = me_user(bt, fx);
    int move = fx->move_id;
    const pr_move *mv = &pr_moves[move];
    *eff_out = 0;
    if (mv->target == MOVE_TARGET_USER) { *eff_out = 1; return HIT_CHECK_RESULT_HIT; }
    bool field_targeted = is_field_targeted(move);
    if (!is_active_on_field(target) && !field_targeted) return HIT_CHECK_RESULT_TARGET_NOT_ON_FIELD;
    /* Commander: moves against a Tatsugiri in its ally's mouth miss */
    if (!field_targeted && bt->double_battle) {
        const pr_pokemon *ally = pr_get_ally(bt, target);
        const pr_battler_tag *cmd = ally ? pr_get_battler_tag(ally, BATTLER_TAG_TYPE_COMMANDED) : NULL;
        if (cmd && pr_pokemon_by_id(bt, cmd->source_id) == target) return HIT_CHECK_RESULT_MISS;
    }
    bool bypass = field_targeted || check_bypass_acc_and_invuln(bt, user, target, move);
    const pr_battler_tag *semi = pr_find_battler_tag_class(target, PR_TAGCLASS_SEMI_INVULNERABLE);
    if (semi && !bypass) {
        bool hits_tag = false;
        for (unsigned i = 0; i < mv->nattr; i++) {
            const pr_attr *a = &pr_attrs[mv->attr0 + i];
            if (pr_attr_is(a->cls, PR_AC_HitsTagAttr) && ((const pr_attr_HitsTagAttr *)a->p)->tagType == semi->type)
                hits_tag = true;
        }
        if (!hits_tag) return HIT_CHECK_RESULT_MISS;
    }
    if (!field_targeted) {
        /* protectedCheck */
        int tside = target->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY;
        bool conditional = false, bypass = false;
        if (!is_ally_target(move)) pr_arena_conditional_protect(bt, e, tside, target, move, &conditional, &bypass);
        if (mv->target != MOVE_TARGET_ENEMY_SIDE && mv->target != MOVE_TARGET_BOTH_SIDES
            && (bypass || !pr_move_does_flag_effect_apply_ex(bt, move, MOVE_FLAGS_IGNORE_PROTECT, user, target, false))) {
            if (conditional) return HIT_CHECK_RESULT_PROTECTED;
            /* target.findTags(t => t instanceof ProtectedTag).some(t => (!isStatusMove || t.blockStatus)
             * && target.lapseTag(t.tagType)): the list is taken first; lapseTag finds each by type */
            bool status_move = mv->category == MOVE_CATEGORY_STATUS;
            uint8_t found[PR_MAX_BATTLER_TAGS];
            int nf = 0;
            for (int i = 0; i < target->n_tags; i++)
                if (pr_battler_tag_is(target->tags[i].type, PR_TAGCLASS_PROTECTED)) found[nf++] = target->tags[i].type;
            for (int k = 0; k < nf; k++) {
                int t = found[k];
                /* blockStatus: false for Burning Bulwark and the stat-lowering ones */
                bool block_status = t != BATTLER_TAG_TYPE_BURNING_BULWARK && t != BATTLER_TAG_TYPE_KINGS_SHIELD
                                    && t != BATTLER_TAG_TYPE_OBSTRUCT && t != BATTLER_TAG_TYPE_SILK_TRAP;
                if ((!status_move || block_status) && pr_lapse_tag(bt, e, target, t, BATTLER_TAG_LAPSE_TYPE_CUSTOM))
                    return HIT_CHECK_RESULT_PROTECTED;
            }
        }
    }
    /* isMoveReflectableBy */
    if (fx->use_mode != MOVE_USE_MODE_REFLECTED && !semi && (mv->flags & MOVE_FLAGS_REFLECTABLE)
        && (pr_get_battler_tag(target, BATTLER_TAG_TYPE_MAGIC_COAT) || pr_has_ability_with_attr(bt, target, PR_AC_ReflectStatusMoveAbAttr))) {
        return HIT_CHECK_RESULT_REFLECTED;
    }
    if (field_targeted) { *eff_out = 1; return HIT_CHECK_RESULT_HIT; }
    bool cancel_msg = false;
    double eff = pr_get_move_effectiveness_ex(bt, target, user, move, false, &cancel_msg);
    if (eff == 0) return cancel_msg ? HIT_CHECK_RESULT_NO_EFFECT_NO_MESSAGE : HIT_CHECK_RESULT_NO_EFFECT;
    double accuracy = pr_calculate_battle_accuracy(bt, user, target, move);
    if (user->hits_left < user->hit_count
        && (!(mv->flags & MOVE_FLAGS_CHECK_ALL_HITS) || pr_has_ability_with_attr(bt, user, PR_AC_MaxMultiHitAbAttr))) {
        *eff_out = eff;
        return HIT_CHECK_RESULT_HIT;
    }
    bool bypass_acc = bypass || pr_get_battler_tag(target, BATTLER_TAG_TYPE_ALWAYS_GET_HIT)
                      || (pr_get_battler_tag(target, BATTLER_TAG_TYPE_TELEKINESIS) && !pr_move_has_attr((uint16_t)move, PR_AC_OneHitKOAttr));
    if (accuracy == -1 || bypass_acc) { *eff_out = eff; return HIT_CHECK_RESULT_HIT; }
    double mult = accuracy_multiplier(bt, user, target, move);
    int rand = pr_battle_rand_seed_int(bt, 100, 0);
    if (rand < accuracy * mult) { *eff_out = eff; return HIT_CHECK_RESULT_HIT; }
    return HIT_CHECK_RESULT_MISS;
}

/* move.getAttrs("MoveEffectAttr") filtered by trigger and the phase's hit state, then applied */
static void trigger_move_effects(pr_battle *bt, pr_engine *e, int ph, int trigger, pr_pokemon *user,
                                 pr_pokemon *target, int first_target /* -1 = null */, int self_target /* -1 = any */)
{
    pr_phase *fx = PH(ph);
    const pr_move *mv = &pr_moves[fx->move_id];
    int sel[16], n = 0;
    for (unsigned i = 0; i < mv->nattr && n < 16; i++) {
        int ai = mv->attr0 + (int)i;
        if (!pr_attr_is(pr_attrs[ai].cls, PR_AC_MoveEffectAttr)) continue;
        const pr_effect_info *ef = &pr_attr_effects[ai];
        if (ef->trigger != trigger) continue;
        if (self_target >= 0 && ef->self_target != (self_target != 0)) continue;
        if (ef->first_hit_only && !fx->first_hit) continue;
        if (ef->last_hit_only && !fx->last_hit) continue;
        if (ef->first_target_only && !(first_target < 0 || first_target)) continue;
        sel[n++] = ai;
    }
    for (int k = 0; k < n; k++) pr_apply_move_effect(bt, e, ph, sel[k], user, target);
}

static void queue_hit_result_message(pr_battle *bt, pr_engine *e, int result)
{
    const char *k = NULL;
    switch (result) {
    case HIT_RESULT_EXTREMELY_EFFECTIVE: k = "battle:hitResultExtremelyEffective"; break;
    case HIT_RESULT_SUPER_EFFECTIVE: k = "battle:hitResultSuperEffective"; break;
    case HIT_RESULT_NOT_VERY_EFFECTIVE: k = "battle:hitResultNotVeryEffective"; break;
    case HIT_RESULT_MOSTLY_INEFFECTIVE: k = "battle:hitResultMostlyIneffective"; break;
    case HIT_RESULT_ONE_HIT_KO: k = "battle:hitResultOneHitKo"; break;
    }
    if (k) pr_queue_msg(bt, e, pr_msg0(k));
}

typedef struct { int result; double damage; bool critical; } move_damage;

static void boss_segments_cleared(pr_battle *bt, pr_engine *e, pr_pokemon *p, int segment_index)
{
    bool do_stat_boost = !p->has_trainer;
    int8_t stats[7], stages[7];
    int n = 0;

    while (p->boss_segment_index > 0 && segment_index - 1 < p->boss_segment_index) {
        p->boss_segment_index--;
        if (!do_stat_boost) continue;

        int eligible[5], weights[5], n_eligible = 0, total = 0;
        for (int stat = STAT_ATK; stat <= STAT_SPD; stat++) {
            int queued = 0;
            for (int i = 0; i < n; i++) if (stats[i] == stat) queued += stages[i];
            if (p->stat_stages[stat - 1] + queued >= 6) continue;
            int weight = (int)pr_get_stat(p, stat, false);
            if (weight < 0) weight = 0;
            eligible[n_eligible] = stat;
            weights[n_eligible] = weight;
            total += weight;
            n_eligible++;
        }
        if (!n_eligible || total <= 0) {
            do_stat_boost = false;
            continue;
        }

        /* weightedPick uses the global randSeedInt, not the battle stream. */
        if (!bt->rng_live) { pr_note_unported(bt, PR_FEAT_GLOBAL_RNG); break; }
        int roll = pr_rng_seed_int(&bt->rng, total, 0);
        int picked = eligible[0], upto = 0;
        for (int i = 0; i < n_eligible; i++) {
            upto += weights[i];
            if (roll < upto) { picked = eligible[i]; break; }
        }

        int boost = 1;
        if (p->boss_segments >= 3 && p->boss_segment_index == 0) boost++;
        if (p->boss_segments >= 5 && p->boss_segment_index == 1) boost++;
        if (n < 7) {
            stats[n] = (int8_t)picked;
            stages[n] = (int8_t)boost;
            n++;
        } else {
            pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT);
        }
    }

    /* The source still queues an empty phase when every stat is capped. */
    int ph = pr_unshift_stat_stage_change_ex(bt, e, pr_battler_index(bt, p), stats, stages, n, p, 0);
    if (ph >= 0) PH(ph)->ssc_ignore_abilities = true;
}

/* Pokemon.damage(damage, ignoreSegments=false, preventEndure, ignoreFaintPhase) */
static double pokemon_damage(pr_battle *bt, pr_engine *e, pr_pokemon *p, double damage, bool prevent_endure,
                             bool ignore_faint_phase)
{
    if (p->hp <= 0) return 0;

    int cleared_boss_segment_index = p->boss_segment_index + 1;
    bool ported_boss = p->is_boss && p->boss_segments > 0;
    if (p->is_boss && !ported_boss) {
        /* Old callers only supplied is_boss. Do not silently guess a segment count. */
        pr_note_unported(bt, PR_FEAT_BOSS_SEGMENTS);
    } else if (ported_boss) {
        double segment_hp = (double)max_hp(p) / p->boss_segments;
        int min_segment = (is_classic_final_boss(bt) && p->form == 0) ? 1 : 0;
        damage = pr_calculate_boss_segment_damage(damage, p->hp, segment_hp, min_segment,
                                                  p->boss_segment_index, &cleared_boss_segment_index);
        if (min_segment == 1 && p->boss_segment_index < 1 && damage > p->hp - 1)
            damage = p->hp - 1;
    }

    if (!prevent_endure && p->hp - damage <= 0) {
        bool survive = false;
        if (p->hp >= 1 && pr_get_battler_tag(p, BATTLER_TAG_TYPE_ENDURING))
            survive = pr_lapse_tag(bt, e, p, BATTLER_TAG_TYPE_ENDURING, BATTLER_TAG_LAPSE_TYPE_CUSTOM);
        else if (p->hp == max_hp(p) && pr_get_battler_tag(p, BATTLER_TAG_TYPE_STURDY)) {
            survive = true;
            pr_lapse_tag(bt, e, p, BATTLER_TAG_TYPE_STURDY, BATTLER_TAG_LAPSE_TYPE_CUSTOM);
        } else if (p->hp >= 1 && pr_get_battler_tag(p, BATTLER_TAG_TYPE_ENDURE_TOKEN))
            survive = pr_lapse_tag(bt, e, p, BATTLER_TAG_TYPE_ENDURE_TOKEN, BATTLER_TAG_LAPSE_TYPE_CUSTOM);
        if (!survive) pr_apply_survive_damage_modifiers(bt, e, p, &survive);   /* Focus Band */
        if (survive) damage = p->hp - 1;
    }
    if (damage > p->hp) damage = p->hp;
    p->hp -= (int32_t)damage;

    if (ported_boss && cleared_boss_segment_index <= p->boss_segment_index)
        boss_segments_cleared(bt, e, p, cleared_boss_segment_index);

    if (p->hp <= 0 && !ignore_faint_phase) {
        pr_queue_faint(bt, e, pr_battler_index(bt, p), prevent_endure, -1);
        pr_lapse_tag(bt, e, p, BATTLER_TAG_TYPE_COMMANDED, BATTLER_TAG_LAPSE_TYPE_CUSTOM);
    }
    return damage;
}

/* Pokemon.damageAndUpdate(damage, {result, isCritical, ignoreSegments, ignoreFaintPhase, source}) */
static double damage_and_update(pr_battle *bt, pr_engine *e, pr_pokemon *p, double damage, int result,
                                bool ignore_faint_phase, pr_pokemon *source, bool critical)
{
    bool indirect = result == HIT_RESULT_INDIRECT || result == HIT_RESULT_INDIRECT_KO;
    int anim = new_pokemon_phase(e, PH_DAMAGE_ANIM, pr_battler_index(bt, p));
    if (anim >= 0) {
        PH(anim)->anim_result = (uint8_t)result;
        PH(anim)->anim_critical = critical;
    }
    pr_phase_unshift(bt, e, anim);
    if (p->switch_out_status && source) damage = 0;
    damage = pokemon_damage(bt, e, p, damage, indirect, ignore_faint_phase);
    if (anim >= 0) PH(anim)->anim_amount = (int32_t)damage;   /* damagePhase.updateAmount(damage) */
    if (!source || source->hit_count <= 1) pr_post_damage_ab(bt, p, damage, source);
    return damage;
}

double pr_damage_and_update(pr_battle *bt, pr_engine *e, pr_pokemon *p, double damage, int result,
                            bool ignore_faint_phase, pr_pokemon *source)
{
    return damage_and_update(bt, e, p, damage, result, ignore_faint_phase, source, false);
}

/* applyMoveAttrs("NoEffectAttr" / "MissEffectAttr"): every one upstream runs crashDamageFunc (High
 * Jump Kick, Jump Kick, Axe Kick, Supercell Slam): unless Magic Guard blocks it, half the user's
 * max HP as indirect damage, the message, and turnData.damageTaken grows by the same amount */
static void miss_effects(pr_battle *bt, pr_engine *e, pr_pokemon *user, int move, uint16_t cls)
{
    const pr_move *mv = &pr_moves[move];
    for (unsigned i = 0; i < mv->nattr; i++) {
        const pr_attr *a = &pr_attrs[mv->attr0 + i];
        if (a->cls != cls) continue;
        pr_fn_id fn = cls == PR_AC_MissEffectAttr ? ((const pr_attr_MissEffectAttr *)a->p)->missEffectFunc
                                                  : ((const pr_attr_NoEffectAttr *)a->p)->noEffectFunc;
        if (!pr_closure_matches(fn, "move:26", "651822f1cdcc")) {
            pr_note_unported(bt, cls);
            continue;
        }
        bool cancelled = false;
        pr_ab_params cp = {.pokemon = user, .move = -1, .flag = &cancelled};
        pr_apply_ab_attrs(bt, PR_AC_BlockNonDirectDamageAbAttr, &cp);
        if (cancelled) continue;
        double d = to_dmg_value(max_hp(user) / 2.0);
        damage_and_update(bt, e, user, d, HIT_RESULT_INDIRECT, false, NULL, false);
        pr_queue_msg(bt, e, pr_msg1("moveTriggers:keptGoingAndCrashed", pr_arg_pokemon(bt, "pokemonName", user)));
        user->damage_taken += (int32_t)d;
    }
}

static move_damage apply_move_damage(pr_battle *bt, pr_engine *e, int ph, pr_pokemon *user, pr_pokemon *target, double eff)
{
    pr_phase *fx = PH(ph);
    int move = fx->move_id;
    move_damage r = {HIT_RESULT_EFFECTIVE, 0, false};
    if (pr_move_has_attr((uint16_t)move, PR_AC_HealOnAllyAttr) && target == pr_get_ally(bt, user)) {
        r.result = HIT_CHECK_RESULT_HIT;
        return r;
    }
    bool crit = pr_get_critical_hit_result(bt, target, user, move);
    move_attrs_unported(bt, move, PR_AC_StatChangeBeforeDmgCalcAttr);
    pr_damage_result d = pr_get_attack_damage_ex(bt, target, user, move, crit, false, eff);
    /* the first TypeBoostTag boosting the move's type goes if it is one-use (Charge's) */
    for (int i = 0; i < user->n_tags; i++) {
        int bty;
        double bval;
        if (!pr_battler_tag_is(user->tags[i].type, PR_TAGCLASS_TYPE_BOOST)) continue;
        pr_type_boost_tag_params(user->tags[i].type, &bty, &bval);
        if (bty != pr_get_move_type(bt, user, move)) continue;
        if (user->tags[i].type == BATTLER_TAG_TYPE_CHARGED) pr_remove_tag(bt, e, user, BATTLER_TAG_TYPE_CHARGED);
        break;
    }
    r.result = d.result;
    if (d.damage <= 0) return r;
    bool ohko = d.result == HIT_RESULT_ONE_HIT_KO;
    (void)ohko;
    pr_lapse_tags(bt, e, target, BATTLER_TAG_LAPSE_TYPE_HIT);
    pr_battler_tag *sub = (pr_battler_tag *)pr_find_battler_tag_class(target, PR_TAGCLASS_SUBSTITUTE);
    bool blocked = sub && pr_move_hits_substitute(bt, move, user, target);
    if (blocked) {
        /* the doll takes it: totalDamageDealt += min(damage, doll's HP) */
        user->total_damage_dealt += (int32_t)(d.damage < sub->hp ? d.damage : sub->hp);
        sub->hp -= (int32_t)d.damage;
    } else if (!target->is_player && d.damage >= target->hp) {
        pr_apply_enemy_endure_modifiers(bt, e, target);
        turn_modifiers(bt, false, target);                       /* EnemyEndureChanceModifier */
    }
    double final = blocked ? 0 : damage_and_update(bt, e, target, d.damage, d.result, true, user, crit);
    if (crit) pr_queue_msg(bt, e, pr_msg0("battle:hitResultCriticalHit"));
    r.critical = crit;
    if (final <= 0) return r;
    user->total_damage_dealt += (int32_t)final;
    user->single_hit_damage_dealt = (int32_t)final;
    target->summon_hit_count++;
    target->damage_taken += (int32_t)final;
    pr_attack_record rec = {.move = (uint16_t)move, .result = (uint8_t)d.result, .critical = crit,
                            .source_battler_index = (int8_t)pr_battler_index(bt, user), .source_id = user->id,
                            .damage = final};
    pr_unshift_attack_received(target, &rec);
    if (user->is_player && !target->is_player) {
        pr_apply_damage_money_modifiers(bt, user, final);
        turn_modifiers(bt, true, user); /* Preserve other missing item-family refusals. */
    }
    r.damage = final;
    return r;
}

static void on_faint_target(pr_battle *bt, pr_engine *e, int ph, pr_pokemon *user, pr_pokemon *target)
{
    pr_queue_faint(bt, e, pr_battler_index(bt, target), false, pr_battler_index(bt, user));
    pr_lapse_tag(bt, e, target, BATTLER_TAG_TYPE_COMMANDED, BATTLER_TAG_LAPSE_TYPE_CUSTOM);
    if (!PH(ph)->last_hit && user->hits_left > 1) PH(ph)->last_hit = true;
}

static void apply_on_target_effects(pr_battle *bt, pr_engine *e, int ph, pr_pokemon *user, pr_pokemon *target,
                                    bool first_target, move_damage d)
{
    int move = PH(ph)->move_id;
    bool deals = d.result == HIT_RESULT_EFFECTIVE || d.result == HIT_RESULT_EXTREMELY_EFFECTIVE
                 || d.result == HIT_RESULT_SUPER_EFFECTIVE || d.result == HIT_RESULT_NOT_VERY_EFFECTIVE
                 || d.result == HIT_RESULT_MOSTLY_INEFFECTIVE || d.result == HIT_RESULT_ONE_HIT_KO;
    trigger_move_effects(bt, e, ph, MOVE_EFFECT_TRIGGER_POST_APPLY, user, target, first_target, 0);
    /* applyHeldItemFlinchCheck (King's Rock) */
    if (!pr_move_has_attr((uint16_t)move, PR_AC_FlinchAttr) && deals
        && !pr_has_ability_with_attr(bt, target, PR_AC_IgnoreMoveEffectsAbAttr) && !pr_move_hits_substitute(bt, move, user, target)) {
        bool flinched = false;
        pr_apply_flinch_chance_modifiers(bt, e, user, target, deals, &flinched);
        if (flinched) {
            pr_add_tag(bt, e, target, BATTLER_TAG_TYPE_FLINCHED, 0, move, user->id);
        }
    }
    /* applyOnGetHitAbEffects */
    pr_ab_params dp = {.pokemon = target, .opponent = user, .move = move, .hit_result = d.result, .damage = d.damage};
    pr_apply_ab_attrs(bt, PR_AC_PostDefendAbAttr, &dp);                  /* Static, Rough Skin, Justified... */
    if (d.critical) pr_apply_ab_attrs(bt, PR_AC_PostReceiveCritStatStageChangeAbAttr, &dp);   /* Anger Point */
    pr_lapse_tags(bt, e, target, BATTLER_TAG_LAPSE_TYPE_AFTER_HIT);
    pr_ab_params ap = {.pokemon = user, .opponent = target, .move = move, .hit_result = d.result, .damage = d.damage};
    pr_apply_ab_attrs(bt, PR_AC_PostAttackAbAttr, &ap);                  /* Poison Touch, Stench... */
    if (!user->is_player && pr_move_is_attack(move)) {
        pr_apply_enemy_attack_status_modifiers(bt, e, user, target);
        turn_modifiers(bt, false, target);   /* EnemyAttackStatusEffectChanceModifier */
    }
    if (pr_move_is_attack(move)) turn_modifiers(bt, user->is_player, user);                /* ContactHeldItemTransferChanceModifier */
}

static void apply_move_effects(pr_battle *bt, pr_engine *e, int ph, pr_pokemon *target, double eff, bool first_target)
{
    pr_phase *fx = PH(ph);
    pr_pokemon *user = me_user(bt, fx);
    int move = fx->move_id;
    trigger_move_effects(bt, e, ph, MOVE_EFFECT_TRIGGER_PRE_APPLY, user, target, -1, -1);
    /* applyMove */
    move_damage d = {HIT_RESULT_STATUS, 0, false};
    int category = pr_apply_variable_move_category(bt, user, target, move, pr_moves[move].category);
    if (category != MOVE_CATEGORY_STATUS) {
        d = apply_move_damage(bt, e, ph, user, target, eff);
        if (user->hits_left == 1 || target->hp <= 0) queue_hit_result_message(bt, e, d.result);
        if (target->hp <= 0) on_faint_target(bt, e, ph, user, target);
    }
    trigger_move_effects(bt, e, ph, MOVE_EFFECT_TRIGGER_POST_APPLY, user, target, first_target, 1);
    if (!pr_move_hits_substitute(bt, move, user, target)) apply_on_target_effects(bt, e, ph, user, target, first_target, d);
    if (PH(ph)->last_hit) {
        form_change(bt, user, PR_FCT_SpeciesFormChangePostMoveTrigger);
        if (user->hit_count > 1) pr_post_damage_ab(bt, target, 0, user);   /* Wimp Out after a multi-hit */
    }
}

pr_pokemon *pr_move_effect_user(pr_battle *bt, pr_engine *e, int *move)
{
    if (!e || e->current < 0 || e->pool[e->current].type != PH_MOVE_EFFECT) return NULL;
    const pr_phase *fx = &e->pool[e->current];
    if (move) *move = fx->move_id;
    return me_user(bt, fx);
}

void pr_stop_multi_hit(pr_battle *bt, pr_engine *e, pr_pokemon *target)
{
    /* removeTarget(target): splice it out of the targets array (the one the command and the move
     * history share); with none left, hitCount = hitsLeft = 1 */
    if (!e || e->current < 0 || e->pool[e->current].type != PH_MOVE_EFFECT) return;
    pr_phase *fx = &e->pool[e->current];
    pr_target_list *t = tlist(e, fx->targets);
    if (!t) return;
    int b = pr_battler_index(bt, target);
    for (int i = 0; i < t->n; i++)
        if (t->v[i] == b) {
            memmove(&t->v[i], &t->v[i + 1], (size_t)(t->n - i - 1));
            t->n--;
            pr_targets_changed(bt, e, fx->targets);
            break;
        }
    if (t->n == 0) {
        pr_pokemon *user = me_user(bt, fx);
        if (user) {
            user->hit_count = 1;
            user->hits_left = 1;
        }
    }
}

static void move_effect_end(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *fx = PH(ph);
    pr_pokemon *user = me_user(bt, fx);
    if (--user->hits_left >= 1 && me_first_target(bt, e, ph, NULL)) {
        /* addNextHitPhase */
        int nx = new_pokemon_phase(e, PH_MOVE_EFFECT, fx->battler);
        if (nx < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return; }
        PH(nx)->user_id = fx->user_id;
        PH(nx)->targets = fx->targets;
        PH(nx)->move_id = fx->move_id;
        PH(nx)->use_mode = fx->use_mode;
        pr_target_list *tl = tlist(e, fx->targets);
        PH(nx)->n_hit_checks = tl ? tl->n : 0;
        pr_phase_unshift(bt, e, nx);
        return;
    }
    int total = user->hit_count - (user->hits_left > 0 ? user->hits_left : 0);
    if (total > 1 || user->hits_left > 0) pr_queue_msg(bt, e, pr_msg1("battle:attackHitsCount", pr_arg_int("count", total)));
    pr_apply_hit_heal_modifiers(bt, e, user);                   /* HitHealModifier (Shell Bell) */
    turn_modifiers(bt, me_player(fx), user);                    /* HitHealModifier (Shell Bell) */
    pr_pokemon *t[4];
    int n = active_in_list(bt, tlist(e, fx->targets), t);
    for (int i = 0; i < n; i++) t[i]->turn_move_effectiveness = -1;
    /* queueDancerResponses */
    int mode = fx->use_mode;
    bool any_hit = false;
    for (int i = 0; i < fx->n_hit_checks && i < 4; i++) any_hit |= fx->hit_check[i] == HIT_CHECK_RESULT_HIT;
    pr_target_list *tl = tlist(e, fx->targets);
    if (mode != MOVE_USE_MODE_INDIRECT && mode != MOVE_USE_MODE_REFLECTED && mode != MOVE_USE_MODE_DELAYED_ATTACK
        && tl && tl->n > 0 && any_hit)
        for (int i = 0; i < 4; i++) {
            pr_pokemon *p = bt->field[i];
            if (!is_active(p) || p == user) continue;
            if ((pr_moves[fx->move_id].flags & MOVE_FLAGS_DANCE_MOVE)
                && !pr_find_battler_tag_class(p, PR_TAGCLASS_SEMI_INVULNERABLE)
                && pr_has_ability_with_attr(bt, p, PR_AC_PostDancingMoveAbAttr)) {
                int nx = new_pokemon_phase(e, PH_DANCER, i);
                if (nx < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); continue; }
                PH(nx)->pokemon = p;
                PH(nx)->source_p = user;
                PH(nx)->move_id = fx->move_id;
                PH(nx)->targets = fx->targets;
                pr_phase_unshift(bt, e, nx);
            }
        }
}

/* CallMoveAttr.apply: the called move aims at its normal targets when it hits several or has only
 * one; otherwise at the move's own target (Mirror Move, Nature Power) or a random one of its
 * targets. LoadMoveAnimPhase, then a FOLLOW_UP MovePhase that goes first. */
static void call_move(pr_battle *bt, pr_engine *e, int move, const pr_attr *a, pr_pokemon *user, pr_pokemon *target)
{
    bool preserve = false;
    switch (a->cls) {
    case PR_AC_NaturePowerAttr: preserve = ((const pr_attr_NaturePowerAttr *)a->p)->preserveTargeting; break;
    case PR_AC_CopyMoveAttr: preserve = ((const pr_attr_CopyMoveAttr *)a->p)->preserveTargeting; break;
    case PR_AC_RandomMoveAttr: preserve = ((const pr_attr_RandomMoveAttr *)a->p)->preserveTargeting; break;
    case PR_AC_RandomMovesetMoveAttr: preserve = ((const pr_attr_RandomMovesetMoveAttr *)a->p)->preserveTargeting; break;
    }
    int called = pr_call_move_get(bt, e, move, a, user, target);
    int replace = pr_moves[called].target == MOVE_TARGET_NEAR_OTHER ? MOVE_TARGET_NEAR_ENEMY : -1;
    int8_t mt[4], targets[4];
    bool multiple;
    int n = pr_get_move_targets(bt, user, called, replace, mt, &multiple);
    int nt = 1;
    if (multiple || n == 1) {
        memcpy(targets, mt, (size_t)n);
        nt = n;
    } else if (preserve) {
        if (!target) { pr_note_unported(bt, a->cls); return; }   /* upstream reads a missing target */
        targets[0] = (int8_t)pr_battler_index(bt, target);
    } else {
        int k = pr_battle_rand_seed_int(bt, n, 0);
        if (k >= n) { pr_note_unported(bt, a->cls); return; }   /* no target: [undefined] */
        targets[0] = mt[k];
    }
    unshift_new(bt, e, PH_LOAD_MOVE_ANIM, -1);
    pr_unshift_move_phase(bt, e, user, called, -1, targets, nt, MOVE_USE_MODE_FOLLOW_UP,
                          MOVE_PHASE_TIMING_MODIFIER_FIRST);
}

/* MoveHeaderPhase: when the Pokemon is active on the field and the move usable (not NONE or
 * unimplemented, PP left; challenges are not modelled), applyMoveAttrs("MoveHeaderAttr", pokemon,
 * null, move): Focus Punch's message, Shell Trap's and Beak Blast's tags */
static void move_header_phase(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *h = PH(ph);
    pr_pokemon *p = h->pokemon;
    int move = h->move.id;
    if (!is_active_on_field(p) || move == MOVE_ID_NONE || pr_moves[move].unimplemented) return;
    int max_pp = ref_move_pp(h);
    if (max_pp != -1 && ref_pp_used(h) >= max_pp) return;           /* isOutOfPp */
    const pr_move *mv = &pr_moves[move];
    for (unsigned i = 0; i < mv->nattr; i++) {
        const pr_attr *a = &pr_attrs[mv->attr0 + i];
        if (!pr_attr_is(a->cls, PR_AC_MoveHeaderAttr)) continue;
        switch (a->cls) {
        case PR_AC_MessageHeaderAttr:                                /* Focus Punch's, the only one */
            pr_queue_msg(bt, e, pr_msg1("moveTriggers:isTighteningFocus", pr_arg_pokemon(bt, "pokemonName", p)));
            break;
        case PR_AC_AddBattlerTagHeaderAttr:
            pr_add_tag(bt, e, p, ((const pr_attr_AddBattlerTagHeaderAttr *)a->p)->tagType, 0, 0, -1);
            break;
        case PR_AC_BeakBlastHeaderAttr:
            pr_add_tag(bt, e, p, ((const pr_attr_BeakBlastHeaderAttr *)a->p)->tagType, 0, 0, -1);
            break;
        default:
            pr_note_unported(bt, a->cls);
        }
    }
}

static void move_effect_phase(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *fx = PH(ph);
    pr_pokemon *user = me_user(bt, fx);
    if (!user) return;
    int move = fx->move_id;
    if (user->is_player) bt->last_player_involved = (int8_t)me_field_index(fx);
    else bt->last_enemy_involved = (int8_t)me_field_index(fx);

    /* applyMoveAttrs("OverrideMoveEffectAttr", user, getFirstTarget() ?? null, move, overridden, useMode):
     * the move-calling attributes queue their move and return without overriding anything */
    const pr_move *omv = &pr_moves[move];
    bool overridden = false;
    for (unsigned i = 0; i < omv->nattr; i++) {
        const pr_attr *a = &pr_attrs[omv->attr0 + i];
        if (!pr_attr_is(a->cls, PR_AC_OverrideMoveEffectAttr)) continue;
        switch (a->cls) {
        case PR_AC_DelayedAttackAttr: {
            /* Future Sight, Doom Desire: not when this is the attack landing (DELAYED_ATTACK). The
             * charge animation, the message, a history entry (OTHER, [target]), and a tag on the
             * target's slot that lands in 3 turns */
            if (fx->use_mode == MOVE_USE_MODE_DELAYED_ATTACK) break;
            overridden = true;
            pr_pokemon *tgt;
            me_first_target(bt, e, ph, &tgt);
            if (!tgt) { pr_note_unported(bt, a->cls); break; }        /* upstream reads a missing target */
            int tb = pr_battler_index(bt, tgt);
            {
                /* MoveAnimPhase(new MoveChargeAnim(chargeAnim, move.id, user)), then chargeKey */
                const pr_attr_DelayedAttackAttr *x = (const pr_attr_DelayedAttackAttr *)a->p;
                int an = new_pokemon_phase(e, PH_MOVE_ANIM, pr_battler_index(bt, user));
                if (an >= 0) { PH(an)->anim = (uint16_t)x->chargeAnim; PH(an)->anim_target = -1; }
                pr_phase_unshift(bt, e, an);
                pr_queue_msg(bt, e, pr_msg1(x->chargeKey, pr_arg_pokemon(bt, "pokemonName", user)));
            }
            if (is_on_field(user)) {                                    /* pushMoveHistory */
                pr_turn_move tm;
                memset(&tm, 0, sizeof tm);
                tm.move = (uint16_t)move;
                tm.use_mode = fx->use_mode;
                tm.result = MOVE_RESULT_OTHER;
                tm.targets[0] = (int8_t)tb;
                tm.n_targets = 1;
                pr_push_move_history(user, tm);
            }
            pr_positional_tag pt = {.type = POSITIONAL_TAG_TYPE_DELAYED_ATTACK, .turn_count = 3,
                                    .target_index = (int8_t)tb, .source_id = user->id, .source_move = (uint16_t)move};
            pr_pos_add(bt, e, &pt);
            fx = PH(ph);
            break;
        }
        case PR_AC_NaturePowerAttr: case PR_AC_CopyMoveAttr: case PR_AC_RandomMoveAttr: case PR_AC_RandomMovesetMoveAttr: {
            pr_pokemon *first0;
            me_first_target(bt, e, ph, &first0);
            call_move(bt, e, move, a, user, first0);
            break;
        }
        case PR_AC_AwaitCombinedPledgeAttr: {
            /* the combined move's message when this is the second pledge; else the first queued
             * MovePhase of a Pokemon on the user's side (getMovePhase), when it holds a different
             * pledge, makes this one wait for it: only with an ally, which a single battle lacks */
            if (user->combining_pledge) {
                pr_queue_msg(bt, e, pr_msg0("moveTriggers:combiningPledge"));
                break;
            }
            const pr_phase *mine = pr_find_move_phase(e, -1, user->is_player ? 1 : 0);
            if (mine && mine->move.id != move && pr_move_has_attr(mine->move.id, PR_AC_AwaitCombinedPledgeAttr))
                pr_note_unported(bt, a->cls);
            break;
        }
        case PR_AC_MovesetCopyMoveAttr: {
            pr_pokemon *first;
            me_first_target(bt,e,ph,&first);
            pr_copy_last_move(bt,e,user,first,move,false);
            break;
        }
        default:
            pr_note_unported(bt, a->cls);
        }
    }
    fx = PH(ph);
    if (overridden) {                   /* stop before the other effects: this.end() */
        move_effect_end(bt, e, ph);
        return;
    }
    pr_lapse_tags(bt, e, user, BATTLER_TAG_LAPSE_TYPE_MOVE_EFFECT);

    pr_pokemon *first;
    me_first_target(bt, e, ph, &first);
    if (user->hits_left == -1) {
        int hits = 1;
        const pr_move *mv = &pr_moves[move];
        for (unsigned i = 0; i < mv->nattr; i++) {
            const pr_attr *a = &pr_attrs[mv->attr0 + i];
            if (!pr_attr_is(a->cls, PR_AC_MultiHitAttr)) continue;
            if (a->cls != PR_AC_MultiHitAttr) {
                pr_note_unported(bt, a->cls);
                continue;
            }
            const pr_attr_MultiHitAttr *mh = a->p;
            int hit_type = mh->intrinsicMultiHitType;
            /* applyMoveAttrs("ChangeMultiHitTypeAttr"): Water Shuriken hits 3 times for Ash-Greninja */
            for (unsigned j = 0; j < mv->nattr; j++) {
                int c = pr_attrs[mv->attr0 + j].cls;
                if (!pr_attr_is(c, PR_AC_ChangeMultiHitTypeAttr)) continue;
                if (c != PR_AC_WaterShurikenMultiHitTypeAttr) pr_note_unported(bt, c);
                else if (user->species == SPECIES_ID_BATTLE_BOND_GRENINJA && user->form == 1) hit_type = MULTI_HIT_TYPE_THREE;
            }
            switch (hit_type) {
            case MULTI_HIT_TYPE_TWO_TO_FIVE: {
                double rolled = pr_battle_rand_seed_int(bt, 20, 0);
                pr_ab_params max_hits = {.pokemon = user, .move = -1, .value = &rolled};
                pr_apply_ab_attrs(bt, PR_AC_MaxMultiHitAbAttr, &max_hits);
                int v = (int)rolled;
                hits = v >= 13 ? 2 : v >= 6 ? 3 : v >= 3 ? 4 : 5;
                break;
            }
            case MULTI_HIT_TYPE_TWO: hits = 2; break;
            case MULTI_HIT_TYPE_THREE: hits = 3; break;
            case MULTI_HIT_TYPE_TEN: hits = 10; break;
            case MULTI_HIT_TYPE_BEAT_UP: hits = pr_beat_up_hit_count(bt, user); break;
            default: pr_note_unported(bt, PR_AC_MultiHitAttr); break;   /* Beat Up's count */
            }
        }
        double count = hits;
        pr_ab_params second_strike = {.pokemon = user, .opponent = first, .move = move, .value = &count};
        pr_apply_ab_attrs(bt, PR_AC_AddSecondStrikeAbAttr, &second_strike);
        hits = (int)count;
        pr_apply_multi_hit_count_modifiers(bt, user, move, &hits);
        turn_modifiers(bt, user->is_player, user); /* Preserve other missing item-family refusals. */
        user->hit_count = (int8_t)hits;
        user->hits_left = (int8_t)hits;
    }

    fx->entry.move = (uint16_t)move;
    fx->entry.use_mode = fx->use_mode;
    fx->entry.result = MOVE_RESULT_PENDING;

    /* conductHitChecks */
    bool field_move = is_field_targeted(move);
    pr_pokemon *targets[4];
    int nt = active_in_list(bt, tlist(e, fx->targets), targets);
    bool any_success = false, all_miss = true;
    for (int i = 0; i < nt; i++) {
        double eff;
        int hc = hit_check(bt, e, ph, targets[i], &eff);
        if (field_move && hc == HIT_CHECK_RESULT_REFLECTED) {
            /* a field move stops at the first target that bounces it: targets = [target] */
            targets[0] = targets[i];
            fx->hit_check[0] = (uint8_t)hc;
            fx->hit_eff[0] = eff;
            fx->n_hit_checks = 1;
            nt = 1;
            break;
        }
        if (hc == HIT_CHECK_RESULT_HIT) any_success = true;
        else all_miss = all_miss || hc == HIT_CHECK_RESULT_MISS;
        if (i < 4) { fx->hit_check[i] = (uint8_t)hc; fx->hit_eff[i] = eff; }
    }
    if (any_success) fx->entry.result = MOVE_RESULT_SUCCESS;
    else {
        user->hit_count = 1;
        user->hits_left = 1;
        fx->entry.result = all_miss ? MOVE_RESULT_MISS : MOVE_RESULT_FAIL;
    }
    fx->first_hit = user->hit_count == user->hits_left;
    bool any_active = false;
    for (int i = 0; i < nt; i++) any_active |= is_active_on_field(targets[i]);
    fx->last_hit = user->hits_left == 1 || !any_active;

    /* the move's animation when it succeeded against a target or has a POST_TARGET effect: one per
     * target on the field (getTargets(), field order), or against the user when none is left;
     * then postAnimCallback */
    bool post_target = false;
    const pr_move *amv = &pr_moves[move];
    for (unsigned i = 0; i < amv->nattr; i++) {
        int ai = amv->attr0 + (int)i;
        if (pr_attr_is(pr_attrs[ai].cls, PR_AC_MoveEffectAttr) && pr_attr_effects[ai].trigger == MOVE_EFFECT_TRIGGER_POST_TARGET)
            post_target = true;
    }
    if (fx->entry.result == MOVE_RESULT_SUCCESS || post_target) {
        pr_target_list *atl = tlist(e, fx->targets);
        int ub = pr_battler_index(bt, user), shown = 0;
        for (int b = 0; b < 4; b++) {
            bool listed = false;
            for (int k = 0; atl && k < atl->n; k++) listed |= atl->v[k] == b;
            if (!listed || !is_active_on_field(bt->field[b])) continue;
            pr_event_add(bt, e, PR_EV_MOVE_ANIM, ub, b, move, 0, NULL);
            shown++;
        }
        if (!shown) pr_event_add(bt, e, PR_EV_MOVE_ANIM, ub, ub, move, 0, NULL);
    }

    /* postAnimCallback (the animation runs first; it queues nothing) */
    if (fx->first_hit && fx->use_mode != MOVE_USE_MODE_DELAYED_ATTACK) {
        pr_push_history_shared(bt, e, user, fx->entry, fx->targets);
        pr_ab_params xp = {.pokemon = user, .move = -1};
        pr_apply_ab_attrs(bt, PR_AC_ExecutedMoveAbAttr, &xp);   /* Gorilla Tactics */
    }
    /* applyToTargets */
    bool first_target = true;
    for (int i = 0; i < nt; i++) {
        switch (fx->hit_check[i]) {
        case HIT_CHECK_RESULT_HIT:
            apply_move_effects(bt, e, ph, targets[i], fx->hit_eff[i], first_target);
            first_target = false;
            if (field_move) i = nt;
            break;
        case HIT_CHECK_RESULT_NO_EFFECT:
            pr_queue_msg(bt, e, pr_msg1(move == MOVE_ID_SHEER_COLD ? "battle:hitResultImmune" : "battle:hitResultNoEffect",
                                        pr_arg_pokemon(bt, "pokemonName", targets[i])));
            /* fall through */
        case HIT_CHECK_RESULT_NO_EFFECT_NO_MESSAGE: case HIT_CHECK_RESULT_PROTECTED:
        case HIT_CHECK_RESULT_TARGET_NOT_ON_FIELD:
            miss_effects(bt, e, user, move, PR_AC_NoEffectAttr);
            break;
        case HIT_CHECK_RESULT_MISS:
            pr_queue_msg(bt, e, pr_msg_p(bt, "battle:attackMissed", targets[i]));
            miss_effects(bt, e, user, move, PR_AC_MissEffectAttr);
            break;
        case HIT_CHECK_RESULT_REFLECTED: {
            /* MoveReflectPhase(target, user, move) */
            int rp = new_pokemon_phase(e, PH_MOVE_REFLECT, pr_battler_index(bt, targets[i]));
            if (rp < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); e->overflow = true; break; }
            PH(rp)->pokemon = targets[i];
            PH(rp)->source_p = user;
            PH(rp)->move_id = (uint16_t)move;
            pr_phase_unshift(bt, e, rp);
            break;
        }
        }
    }
    /* stellarTypesBoosted gets the move's type for any damaging move */
    int move_type = pr_get_move_type(bt, user, move);
    if (pr_moves[move].category != MOVE_CATEGORY_STATUS) user->stellar_types_boosted |= 1u << move_type;
    if (fx->last_hit) trigger_move_effects(bt, e, ph, MOVE_EFFECT_TRIGGER_POST_TARGET, user, NULL, -1, -1);
    /* updateSubstitutes: a doll at 0 HP or less goes (removeTag) */
    nt = active_in_list(bt, tlist(e, fx->targets), targets);
    for (int i = 0; i < nt; i++) {
        const pr_battler_tag *sub = pr_find_battler_tag_class(targets[i], PR_TAGCLASS_SUBSTITUTE);
        if (sub && sub->hp <= 0) pr_remove_tag(bt, e, targets[i], BATTLER_TAG_TYPE_SUBSTITUTE);
    }
    move_effect_end(bt, e, ph);
}

/* ---------------------------------------------------------------- MoveEndPhase */

static void move_end_phase(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *me = PH(ph);
    pr_pokemon *p = slot(bt, me->battler);
    if (p) p->hits_left = -1;
    if (!me->was_follow_up && is_active_on_field(p)) pr_lapse_tags(bt, e, p, BATTLER_TAG_LAPSE_TYPE_AFTER_MOVE);
    bt->ignore_abilities = false;
    bt->ignoring_effect_source = -1;
    for (int i = 0; i < me->n_end_targets; i++) {
        pr_pokemon *t = me->end_targets[i];                  /* the Pokemon, wherever it is now */
        pr_ab_params pp = {.pokemon = t, .move = -1};
        pr_apply_ab_attrs(bt, PR_AC_PostSummonRemoveEffectAbAttr, &pp);  /* Limber, Oblivious... */
    }
}

/* ---------------------------------------------------------------- FaintPhase */

static void reset_turn_data(pr_pokemon *p)
{
    p->n_form_change_abilities = 0;
    p->n_turn_berries = 0;
    p->hit_count = 0;
    p->hits_left = -1;
    p->total_damage_dealt = p->single_hit_damage_dealt = p->damage_taken = 0;
    p->n_attacks_received = 0;
    p->stat_stages_increased = p->stat_stages_decreased = false;
    p->turn_move_effectiveness = -1;
    p->combining_pledge = 0;
    p->failed_run_away = p->joined_round = false;
    p->acted = false;
    p->order = -1;
    p->switched_in_this_turn = p->summoned_this_turn = false;
    p->pending_status = STATUS_EFFECT_NONE;
}

static void revert_dynamax(pr_pokemon *p)
{
    if (!p || !p->is_dynamaxed) return;
    double mult = 1.5 + ((double)p->dynamax_level * 0.05);
    p->stats[STAT_HP] = (int32_t)ceil((double)p->stats[STAT_HP] / mult);
    p->hp = (int32_t)ceil((double)p->hp / mult);
    if (p->hp > p->stats[STAT_HP]) p->hp = p->stats[STAT_HP];
    p->is_dynamaxed = false;
    p->dynamax_turns_left = 0;
    p->form = p->pre_dynamax_form;
}

static void reset_summon_data(pr_pokemon *p)
{
    p->n_last_berries = 0;
    pr_restore_transform(p);
    pr_break_illusion(p);
    revert_dynamax(p);
    p->queued_mega = p->queued_dynamax = false;
    p->is_dynamaxed = false;
    p->dynamax_turns_left = 0;
    p->dynamax_level = 0;
    memset(p->stat_stages, 0, sizeof p->stat_stages);
    p->n_move_queue = 0;
    p->n_tags = 0;
    p->ability_suppressed = false;
    p->summon_hit_count = 0;
    p->ability_override = -1;
    p->temporary_fusion_species = 0;
    p->temporary_fusion_form = 0;
    p->ability = p->base_ability;
    pr_refresh_ability(p);
    memset(p->summon_stats, 0, sizeof p->summon_stats);
    p->n_summon_types = 0;
    p->added_type = 0;
    p->n_move_history = 0;
    p->temp_turn_count = p->temp_wave_turn_count = 1;
    p->n_ab_applied_summon = 0;
}

void pr_reset_turn_data(pr_pokemon *p) { reset_turn_data(p); }
void pr_reset_summon_data(pr_pokemon *p) { reset_summon_data(p); }
void pr_form_change(pr_battle *bt, const pr_pokemon *p, int trigger_class) { form_change(bt, p, trigger_class); }
void pr_form_change_delayed(pr_battle *bt, const pr_pokemon *p, int trigger_class) { form_change_with_delay(bt, p, trigger_class, true); }

void pr_weather_form_changes(pr_battle *bt, bool revert, const pr_pokemon *source)
{
    pr_speed_iter it;
    pr_speed_iter_init(bt, &it, ARENA_TAG_SIDE_BOTH);
    pr_pokemon *p;
    while ((p = pr_speed_iter_next(bt, &it))) {
        if (!revert && p == source) continue;
        int ability = p->species == SPECIES_ID_CASTFORM ? ABILITY_ID_FORECAST
                    : p->species == SPECIES_ID_CHERRIM ? ABILITY_ID_FLOWER_GIFT : -1;
        if (ability < 0) continue;
        bool has = revert ? p->base_ability == ability || p->passive == ability
                          : pr_has_ability_active(bt, p, ability);
        if (has) form_change(bt, p, revert ? PR_FCT_SpeciesFormChangeRevertWeatherFormTrigger
                                          : PR_FCT_SpeciesFormChangeWeatherTrigger);
    }
}

static void quiet_form_change_phase(pr_battle *bt, pr_engine *e, int ph)
{
    pr_pokemon *p = PH(ph)->pokemon;
    int target = PH(ph)->move_id;
    if (!p || p->form == target || (!p->is_player && !is_active_on_field(p))) return;
    pr_msg_arg pre = pr_arg_pokemon(bt, "preName", p);
    pr_ab_params pp = {.pokemon = p, .move = -1};
    pr_apply_ab_attrs(bt, PR_AC_PreLeaveFieldAbAttr, &pp);
    pr_change_form_state(bt, p, target);
    if (is_active_on_field(p)) {
        uint16_t passive_ability = pr_get_passive_ability(p);
        bool active_seen = false, passive_seen = false;
        for (int i = 0; i < p->n_form_change_abilities; i++) {
            active_seen |= p->form_change_abilities[i] == p->ability;
            passive_seen |= p->form_change_abilities[i] == passive_ability;
        }
        if (!active_seen || !passive_seen) {
            bool add_passive = !passive_seen;
            bool add_active = !active_seen && (passive_seen || p->ability != passive_ability);
            int needed = add_passive + add_active;
            if (p->n_form_change_abilities + needed > 16) pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT);
            else {
                /* Source Set insertion: passive first, then active, no duplicate NONE. */
                if (add_passive) p->form_change_abilities[p->n_form_change_abilities++] = passive_ability;
                if (add_active) p->form_change_abilities[p->n_form_change_abilities++] = p->ability;
                pp.only = active_seen ? 2 : passive_seen ? 1 : 0;
                pp.after_form_change = true;
                pr_apply_ab_attrs(bt, PR_AC_PostSummonAbAttr, &pp);
            }
        }
    }
    const char *key = pr_pokemon_base_form(p)->form_key;
    if (key && strstr(key, "mega"))
        pr_show_text(bt, e, pr_msg2("battlePokemonForm:megaChange", pre, pr_arg_pokemon_name(bt, "pokemonName", p)));
    else if (key && strstr(key, "gigantamax"))
        pr_show_text(bt, e, pr_msg2("battlePokemonForm:gigantamaxChange", pre, pr_arg_pokemon_name(bt, "pokemonName", p)));
    else if (key && strstr(key, "eternamax"))
        pr_show_text(bt, e, pr_msg2("battlePokemonForm:eternamaxChange", pre, pr_arg_pokemon_name(bt, "pokemonName", p)));
    else if (target == 0)
        pr_show_text(bt, e, pr_msg1("battlePokemonForm:revertChange", pr_arg_pokemon(bt, "pokemonName", p)));
    else if (p->ability == ABILITY_ID_DISGUISE)
        pr_show_text(bt, e, pr_msg0("battlePokemonForm:disguiseChange"));
    else pr_show_text(bt, e, pr_msg1("battlePokemonForm:formChange", pre));
    pr_remove_tag(bt, e, p, BATTLER_TAG_TYPE_AUTOTOMIZED);
}

static int32_t party_hp(pr_battle *bt, int side, int k)
{
    return bt->party[side][k]->hp;
}

static bool party_on_field(pr_battle *bt, int side, int k)
{
    return is_on_field(bt->party[side][k]);
}

static void add_participant(pr_battle *bt, uint32_t id)
{
    for (int i = 0; i < bt->n_participants; i++)
        if (bt->participants[i] == id) return;
    if (bt->n_participants < PR_MAX_PARTY) bt->participants[bt->n_participants++] = id;
    else pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT);
}

static void remove_participant(pr_battle *bt, uint32_t id)
{
    for (int i = 0; i < bt->n_participants; i++)
        if (bt->participants[i] == id) {
            memmove(&bt->participants[i], &bt->participants[i + 1], (size_t)(bt->n_participants - i - 1) * sizeof(uint32_t));
            bt->n_participants--;
            return;
        }
}

/* FaintPhase.applyModifier stops at the first holder-matching Reviver Seed.
 * Queue revival, clear status immediately, then consume the item before returning
 * from FaintPhase: the ordinary faint history/field/friendship path is skipped. */
static bool instant_revive(pr_battle *bt, pr_engine *e, pr_pokemon *p)
{
    int side = p->is_player ? 0 : 1;
    for (int i = 0; i < bt->n_mods[side]; i++) {
        pr_modifier *m = &bt->mods[side][i];
        if (!pr_mod_is(m, PR_MOD_INSTANT_REVIVE)) continue;
        if (m->pokemon_id != -1 && m->pokemon_id != (int64_t)p->id) continue;
        if (!pr_mod_is_ported_item(m)) { pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS); continue; }
        pr_msg message = pr_msg2("modifier:pokemonInstantReviveApply",
            pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
            pr_arg_key("typeName", "modifierType:ModifierType.REVIVER_SEED.name"));
        pr_unshift_heal_msg(bt, e, pr_battler_index(bt, p), to_dmg_value(max_hp(p) / 2.0),
                           PR_HEAL_MESSAGE | PR_HEAL_REVIVE, message);
        clear_status(bt, e, p, false);
        pr_speed_iter it;
        pr_pokemon *ally;
        pr_speed_iter_init(bt, &it, p->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY);
        while ((ally = pr_speed_iter_next(bt, &it))) {
            pr_ab_params pp = {.pokemon = ally, .move = -1};
            pr_apply_ab_attrs(bt, PR_AC_CommanderAbAttr, &pp);
        }
        pr_lose_held_item(bt, e, p, m);
        /* updateModifiers clears virtual stacks and orphaned held items. */
        int w = 0;
        for (int j = 0; j < bt->n_mods[side]; j++) {
            pr_modifier copy = bt->mods[side][j];
            if (pr_mod_is(&copy, PR_MOD_HELD_ITEM) && !pr_pokemon_by_id(bt, copy.pokemon_id)) continue;
            copy.virtual_stack_count = 0;
            if (copy.stack_count) bt->mods[side][w++] = copy;
        }
        bt->n_mods[side] = w;
        for (int j = 0; j < bt->n_party[side]; j++) pr_recalculate_stats(bt, bt->party[side][j]);
        return true;
    }
    return false;
}

/* The optional faint observer (pr_faint_note): reads state only. `direct` needs the FaintPhase's own source AND the newest attack record to
 * agree on the attacker, so a switch of the slot between the hit and the FaintPhase cannot misname it. */
static void faint_notify(pr_battle *bt, const pr_phase *f, const pr_pokemon *p, bool player)
{
    pr_faint_note n;
    memset(&n, 0, sizeof n);
    n.id = (int64_t)p->id;
    n.battler = f->battler;
    n.side = player ? 0 : 1;
    n.source_battler = -1;
    n.wave = bt->wave_index;
    n.turn = bt->turn;
    n.fainted = p;
    for (int k = 0; k < bt->n_party[n.side]; k++)
        if (bt->party[n.side][k] != p && bt->party[n.side][k]->hp > 0 && n.remaining < 255) n.remaining++;
    if (f->source >= 0 && p->n_attacks_received > 0) {
        const pr_attack_record *r = &p->attacks_received[0];
        if (r->source_battler_index == f->source && r->damage > 0) {
            n.direct = true;
            n.source_battler = r->source_battler_index;
            n.source_id = r->source_id;
            n.move = r->move;
            n.source = pr_pokemon_by_id(bt, r->source_id);
        }
    }
    bt->faint_observer(bt, &n, bt->faint_observer_ctx);
}

static void faint_phase(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *f = PH(ph);
    pr_pokemon *p = slot(bt, f->battler);
    if (!p || p->hp > 0) return;
    bool player = f->battler < 2;
    pr_pokemon *source = f->source >= 0 ? slot(bt, f->source) : NULL;
    if (source) {
        const pr_battler_tag *db = pr_get_battler_tag(p, BATTLER_TAG_TYPE_DESTINY_BOND);
        if (db) {
            pr_battler_tag copy = *db;
            pr_destiny_bond_lapse(bt, e, &copy, source);
        }
        if (pr_get_battler_tag(p, BATTLER_TAG_TYPE_GRUDGE)) pr_grudge_lapse(bt, e, p, source);
    }
    reset_summon_data(p);
    if (!f->prevent_instant_revive) {
        if (instant_revive(bt, e, p)) return;
        turn_modifiers(bt, player, p); /* Preserve refusal for other missing item families. */
    }
    for (int i = 0; i < 2; i++) {
        pr_pokemon *q = bt->field[i];
        if (q && (is_active(q) || q->hp <= 0)) add_participant(bt, q->id);
    }
    if (bt->wave_index == 200 && !player) pr_note_unported(bt, PR_FEAT_FINAL_BOSS);

    /* doFaint */
    if (bt->faint_observer) faint_notify(bt, f, p, player);
    if (player) {
        bt->player_faints++;
        int k = bt->n_faint_history[0] < 12 ? bt->n_faint_history[0]++ : (pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT), 11);
        bt->faint_history[0][k].id = p->id;
        bt->faint_history[0][k].turn = bt->turn;
    } else {
        bt->enemy_faints++;
        int k = bt->n_faint_history[1] < 12 ? bt->n_faint_history[1]++ : (pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT), 11);
        bt->faint_history[1][k].id = p->id;
        bt->faint_history[1][k].turn = bt->turn;
    }
    pr_queue_msg(bt, e, pr_msg_p(bt, "battle:fainted", p));
    form_change_with_delay(bt, p, PR_FCT_SpeciesFormChangeActiveTrigger, true);
    /* resetTera */
    bool was_tera = p->terastallized;
    p->terastallized = false;
    p->stellar_types_boosted = 0;
    if (was_tera) form_change(bt, p, PR_FCT_SpeciesFormChangeLapseTeraTrigger);
    pr_ab_params faint_params = {.pokemon = p, .move = -1};
    pr_apply_ab_attrs(bt, PR_AC_PostFaintAbAttr, &faint_params);
    pr_speed_iter it;
    pr_pokemon *q;
    pr_speed_iter_init(bt, &it, ARENA_TAG_SIDE_BOTH);
    while ((q = pr_speed_iter_next(bt, &it))) {
        pr_ab_params params = {.pokemon = q, .victim = p, .move = -1};
        pr_apply_ab_attrs(bt, PR_AC_PostKnockOutAbAttr, &params);
    }
    if (p->n_attacks_received > 0 && is_on_field(source)) {
        pr_ab_params params = {.pokemon = source, .move = -1};
        pr_apply_ab_attrs(bt, PR_AC_PostVictoryAbAttr, &params);
        int victory_move = p->attacks_received[0].move;
        const pr_move *vm = &pr_moves[victory_move];
        for (int a=vm->attr0; a<vm->attr0+vm->nattr; a++)
            if (pr_attrs[a].cls == PR_AC_PostVictoryStatStageChangeAttr)
                pr_apply_post_victory_stat_change(bt,e,a,source,victory_move);
    }
    /* attacksReceived was cleared by nothing yet: resetSummonData leaves turnData alone */

    if (player) {
        int legal = 0, bench = 0;
        for (int k = 0; k < bt->n_party[0]; k++) {
            if (party_hp(bt, 0, k) <= 0) continue;
            legal++;
            if (!party_on_field(bt, 0, k)) bench++;
        }
        if (bt->n_party[0] == 0) pr_note_unported(bt, PR_FEAT_PARTY);
        if (legal == 0) unshift_new(bt, e, PH_GAME_OVER, -1);
        else if (bt->double_battle && legal == 1 && bench == 0) unshift_new(bt, e, PH_TOGGLE_DOUBLE_POSITION, -1);
        else if (bench > 0) {
            /* pushNew("SwitchPhase", SwitchType.SWITCH, fieldIndex, isModal true, doReturn false) */
            int sw = new_pokemon_phase(e, PH_SWITCH, -1);
            if (sw >= 0) {
                pr_phase *s = PH(sw);
                s->sw_type = SWITCH_TYPE_SWITCH;
                s->sw_field_index = (int8_t)(f->battler % 2);
                s->sw_modal = true;
                s->sw_do_return = false;
                s->sw_player = true;
            }
            pr_phase_push(bt, e, sw);
        }
    } else {
        unshift_new(bt, e, PH_VICTORY, f->battler);
        if (bt->battle_type == BATTLE_TYPE_MYSTERY_ENCOUNTER) pr_note_unported(bt, PR_FEAT_TRAINER);
        if (bt->battle_type == BATTLE_TYPE_TRAINER) {
            /* a reserve of the same trainer (isActive(), not on the field): the trainer's next
             * Pokemon, pushNew("SwitchSummonPhase", SWITCH, fieldIndex, -1, false, false) */
            bool reserve = false;
            if (!bt->n_party[1]) pr_note_unported(bt, PR_FEAT_PARTY);
            for (int k = 0; k < bt->n_party[1]; k++) {
                const pr_pokemon *q = bt->party[1][k];
                if (q->hp > 0 && !(q->on_field && !q->switch_out_status) && q->trainer_slot == p->trainer_slot)
                    reserve = true;
            }
            if (reserve) {
                int sw = new_pokemon_phase(e, PH_SWITCH_SUMMON, -1);
                if (sw >= 0) {
                    pr_phase *s = PH(sw);
                    s->sw_type = SWITCH_TYPE_SWITCH;
                    s->sw_field_index = (int8_t)(f->battler % 2);
                    s->sw_slot = -1;
                    s->sw_do_return = false;
                    s->sw_player = false;
                }
                pr_phase_push(bt, e, sw);
            }
        }
    }
    /* in double battles, moves still to come at the fainted Pokemon go to its ally */
    const pr_pokemon *fainted_ally = pr_get_ally(bt, p);
    if (bt->double_battle && fainted_ally) pr_redirect_moves(bt, e, p, fainted_ally);

    /* the faint cry's callback, then the tween's */
    pr_event_add(bt, e, PR_EV_FAINT, f->battler, -1, 0, 0, NULL);
    if (player) p->friendship = (uint8_t)(p->friendship > 5 ? p->friendship - 5 : 0);
    pr_lapse_tags(bt, e, p, BATTLER_TAG_LAPSE_TYPE_FAINT);
    /* doSetStatus(FAINT) */
    p->pending_status = STATUS_EFFECT_NONE;
    p->status = STATUS_EFFECT_FAINT;
    p->status_toxic_turns = 0;
    p->status_sleep_turns = 0;
    p->status_freeze_turns = -1;
    if (player) remove_participant(bt, p->id);
    else {
        add_fainted_enemy_score(bt, p);
        /* addPostBattleLoot: the enemy's transferable held items */
        for (int i = 0; i < bt->n_mods[1]; i++) {
            pr_modifier *m = &bt->mods[1][i];
            if (pr_mod_is(m, PR_MOD_HELD_ITEM) && m->pokemon_id == (int64_t)p->id) {
                if (m->is_transferable) {
                    bt->post_battle_loot++;
                    m->pokemon_id = 0;
                }
                if (!pr_mod_is_ported_item(m)) pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS);
            }
        }
    }
    pr_leave_field(bt, e, p, true);
}

/* Pokemon.leaveField(clearEffects): its turn data reset, the tags it gave the others removed,
 * its summon data too unless it passes them on (Baton Pass, Shed Tail, a wild Pokemon fleeing);
 * then off the field. The sprite, the Substitute doll and the info box are only drawn. */
void pr_leave_field(pr_battle *bt, pr_engine *e, pr_pokemon *p, bool clear_effects)
{
    pr_speed_iter it;
    pr_pokemon *q;
    reset_turn_data(p);
    pr_speed_iter_init(bt, &it, ARENA_TAG_SIDE_BOTH);
    while ((q = pr_speed_iter_next(bt, &it)))
        if (q != p)
            pr_remove_tags_by_source_id(bt, e, q, (int64_t)p->id);
    if (clear_effects) reset_summon_data(p);
    pr_ab_params params = {.pokemon = p, .move = -1};
    pr_apply_ab_attrs(bt, PR_AC_PreLeaveFieldAbAttr, &params);
    p->switch_out_status = true;
    form_change_with_delay(bt, p, PR_FCT_SpeciesFormChangeActiveTrigger, true);
    p->on_field = false;
}

/* ReturnPhase inherits Summon.start/preSummon, then ends without a replacement.
 * The timer executes switchAndSummon after leaveField in the tween callback. */
static void return_phase(pr_battle *bt, pr_engine *e, int ph)
{
    int field=PH(ph)->sw_field_index;
    pr_pokemon *p=field>=0 && field<bt->n_party[0] ? bt->party[0][field] : NULL;
    if (!p) { pr_note_unported(bt,PR_FEAT_PARTY);return; }
    pr_hook_ab(bt,p,PR_AC_PreSummonAbAttr);
    pr_speed_iter it;pr_speed_iter_init(bt,&it,ARENA_TAG_SIDE_ENEMY);pr_pokemon *q;
    while ((q=pr_speed_iter_next(bt,&it))) pr_remove_tags_by_source_id(bt,e,q,(int64_t)p->id);
    pr_show_text(bt,e,pr_msg1("battle:playerComeBack",pr_arg_pokemon(bt,"pokemonName",p)));
    pr_event_add(bt,e,PR_EV_SWITCH_OUT,field,-1,0,0,NULL);
    pr_leave_field(bt,e,p,true);
    /* ReturnPhase.onEnd */
    pr_reset_turn_data(p);
    pr_reset_summon_data(p);
    pr_form_change(bt,p,PR_FCT_SpeciesFormChangeActiveTrigger);
}

/* ---------------------------------------------------------------- end of turn */

static void check_interlude_phase(pr_battle *bt, pr_engine *e)
{
    if (bt->wave_index % 10 != 0) return;
    /* Source checks the complete enemy party, not a separate field scan.
     * Array.every also succeeds for an empty party. */
    for (int k = 0; k < bt->n_party[1]; k++)
        if (party_hp(bt, 1, k) > 0) return;
    /* phaseManager.onInterlude removes every matching tree phase. */
    const pr_phase_type removed[] = {PH_WEATHER_EFFECT, PH_BERRY, PH_CHECK_STATUS_EFFECT};
    for (size_t i = 0; i < sizeof removed / sizeof removed[0]; i++)
        while (pr_phase_try_remove(bt, e, removed[i], -1)) {}
    /* PhaseTree.find visits the highest level first and marks only its first match. */
    for (int l = e->n_levels - 1; l >= 0; l--)
        for (int k = 0; k < e->level_len[l]; k++) {
            pr_phase *p = &e->pool[e->levels[l][k]];
            if (p->type == PH_TURN_END) { p->upcoming_interlude = true; return; }
        }
}

/* Weather.isTypeDamageImmune(type) */
static bool weather_type_damage_immune(int weather, int type)
{
    switch (weather) {
    case WEATHER_TYPE_SANDSTORM:
        return type == POKEMON_TYPE_GROUND || type == POKEMON_TYPE_ROCK || type == POKEMON_TYPE_STEEL;
    case WEATHER_TYPE_HAIL:
        return type == POKEMON_TYPE_ICE;
    }
    return false;
}

/* WeatherEffectPhase (a CommonAnimPhase). Sandstorm and Hail: unless an ability on the field
 * suppresses them (Cloud Nine, Air Lock), each Pokemon in speed order that isn't immune by type
 * or leaving the field, isn't shielded by its ability (Overcoat, Sand Veil..., Magic Guard) and
 * isn't underground or underwater takes 1/16 of its max HP after the damage message. Then the
 * lapse text (ui.showText: no phase) and, in its callback, the abilities acting on the weather
 * for each Pokemon still on the field (Rain Dish, Ice Body, Dry Skin, Solar Power). */
static void weather_effect_phase(pr_battle *bt, pr_engine *e)
{
    int w = bt->weather;
    if (w == WEATHER_TYPE_NONE) return;
    pr_speed_iter it;
    pr_pokemon *p;
    if (w == WEATHER_TYPE_SANDSTORM || w == WEATHER_TYPE_HAIL) {         /* isDamaging() */
        bool suppressed = false;
        pr_speed_iter_init(bt, &it, ARENA_TAG_SIDE_BOTH);
        while ((p = pr_speed_iter_next(bt, &it))) {
            pr_ab_params sp = {.pokemon = p, .move = -1, .flag = &suppressed};
            pr_apply_ab_attrs(bt, PR_AC_SuppressWeatherEffectAbAttr, &sp);
        }
        if (!suppressed) {
            pr_speed_iter_init(bt, &it, ARENA_TAG_SIDE_BOTH);
            while ((p = pr_speed_iter_next(bt, &it))) {
                int8_t ty[PR_MAX_TYPES];
                int n = pr_get_types(p, ty, PR_TYPES_ORIGINAL_IF_STELLAR);
                bool immune = p->switch_out_status;
                for (int i = 0; i < n; i++)
                    if (weather_type_damage_immune(w, ty[i])) immune = true;
                if (immune) continue;
                bool cancelled = false;
                pr_ab_params cp = {.pokemon = p, .move = -1, .flag = &cancelled};
                pr_apply_ab_attrs(bt, PR_AC_PreWeatherDamageAbAttr, &cp);
                pr_apply_ab_attrs(bt, PR_AC_BlockNonDirectDamageAbAttr, &cp);
                if (cancelled || pr_get_battler_tag(p, BATTLER_TAG_TYPE_UNDERGROUND)
                    || pr_get_battler_tag(p, BATTLER_TAG_TYPE_UNDERWATER))
                    continue;
                pr_queue_msg(bt, e, pr_msg_p(bt, pr_weather_key(w, PR_WEATHER_DAMAGE), p));
                /* damageAndUpdate(damage, {result: INDIRECT, ignoreSegments: true}) */
                damage_and_update(bt, e, p, to_dmg_value(max_hp(p) / 16.0), HIT_RESULT_INDIRECT, false, NULL, false);
            }
        }
    }
    /* ui.showText(getWeatherLapseMessage), then PostWeatherLapseAbAttr, then CommonAnimPhase.start
     * plays getWeatherAnim */
    pr_show_text(bt, e, pr_msg0(pr_weather_key(w, PR_WEATHER_LAPSE)));
    pr_speed_iter_init(bt, &it, ARENA_TAG_SIDE_BOTH);
    while ((p = pr_speed_iter_next(bt, &it))) {
        if (p->switch_out_status) continue;
        pr_ab_params lp = {.pokemon = p, .move = -1};
        pr_apply_ab_attrs(bt, PR_AC_PostWeatherLapseAbAttr, &lp);
    }
    pr_event_add(bt, e, PR_EV_COMMON_ANIM, default_battler(bt), -1, COMMON_ANIM_SUNNY + (w - 1), 0, NULL);
}

/* PositionalTagPhase: positionalTagManager.activateAllTags(). Each tag counts down; one that runs
 * out goes, triggering first if it should. A delayed attack needs its source (by Pokemon.id, in
 * either party) and a target in its slot that isn't the source and hasn't fainted: the message,
 * then a MoveEffectPhase given the source's id, [the slot], the move, DELAYED_ATTACK. Wish needs a
 * target that hasn't fainted: a PokemonHealPhase with its message and no "HP is full" text. */
static void positional_tag_phase(pr_battle *bt, pr_engine *e)
{
    pr_positional_tag left[PR_MAX_POSITIONAL_TAGS], tags[PR_MAX_POSITIONAL_TAGS];
    int n = bt->n_pos_tags, nl = 0;
    memcpy(tags, bt->pos_tags, (size_t)n * sizeof tags[0]);
    for (int i = 0; i < n; i++) {
        pr_positional_tag *t = &tags[i];
        if (--t->turn_count > 0) {
            left[nl++] = *t;
            continue;
        }
        pr_pokemon *target = t->target_index >= 0 && t->target_index < 4 ? bt->field[t->target_index] : NULL;
        if (t->type == POSITIONAL_TAG_TYPE_DELAYED_ATTACK) {
            pr_pokemon *source = pr_field_pokemon_by_id(bt, t->source_id);
            if (!source) {
                for (int s = 0; s < 2; s++)
                    for (int k = 0; k < bt->n_party[s]; k++)
                        if (bt->party[s][k]->id == t->source_id) pr_note_unported(bt, PR_FEAT_PARTY);   /* off the field */
                continue;
            }
            if (!target || source == target || target->hp <= 0) continue;
            pr_queue_msg(bt, e, pr_msg2("moveTriggers:tookMoveAttack", pr_arg_pokemon(bt, "pokemonName", target),
                                        pr_arg_move("moveName", t->source_move)));
            int fx = new_pokemon_phase(e, PH_MOVE_EFFECT, -1);
            if (fx < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); continue; }
            PH(fx)->user_id = t->source_id;
            int8_t tv[1] = {t->target_index};
            PH(fx)->targets = (int8_t)pr_targets_new(e, tv, 1);
            PH(fx)->move_id = t->source_move;
            PH(fx)->use_mode = MOVE_USE_MODE_DELAYED_ATTACK;
            PH(fx)->n_hit_checks = 1;
            PH(fx)->hit_check[0] = HIT_CHECK_RESULT_PENDING;
            PH(fx)->hit_eff[0] = 0;
            pr_phase_unshift(bt, e, fx);
        } else {
            if (!target || target->hp <= 0) continue;
            pr_msg_arg name = {"pokemonNameWithAffix", PR_ARG_POKEMON, t->pokemon_name, NULL, t->fusion_name};
            pr_unshift_heal_msg(bt, e, t->target_index, t->heal_hp, PR_HEAL_MESSAGE | PR_HEAL_NO_FULL_HP_MESSAGE,
                                pr_msg1("arenaTag:wishTagOnAdd", name));
        }
    }
    memcpy(bt->pos_tags, left, (size_t)nl * sizeof left[0]);
    bt->n_pos_tags = (uint8_t)nl;
}

static void berry_phase(pr_battle *bt, pr_engine *e)
{
    pr_speed_iter it;
    pr_pokemon *p;
    pr_speed_iter_init(bt, &it, ARENA_TAG_SIDE_BOTH);
    while ((p = pr_speed_iter_next(bt, &it))) {
        int side = p->is_player ? 0 : 1;
        for (int i = 0; i < bt->n_mods[side]; i++)
            if (!pr_mod_is_ported_item(&bt->mods[side][i]) && pr_mod_is(&bt->mods[side][i], PR_MOD_HELD_ITEM) && bt->mods[side][i].pokemon_id == (int64_t)p->id)
                pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS);

        bool has_usable_berry = false;
        for (int i = 0; i < bt->n_mods[side]; i++) {
            if (pr_berry_should_apply(bt, p, &bt->mods[side][i])) {
                has_usable_berry = true;
                break;
            }
        }
        if (!has_usable_berry) {
            pr_ab_params chew={.pokemon=p,.move=-1};
            pr_apply_ab_attrs(bt, PR_AC_CudChewConsumeBerryAbAttr, &chew);
            continue;
        }

        bool cancelled = false;
        for(int j=0;j<4;j++) {
            pr_pokemon *opp=bt->field[j];
            if(!opp || opp->is_player==p->is_player || !is_active_on_field(opp))continue;
            pr_ab_params pp = {.pokemon = opp, .move = -1, .flag = &cancelled};
            pr_apply_ab_attrs(bt, PR_AC_PreventBerryUseAbAttr, &pp);
        }
        if (cancelled) {
            pr_msg msg = pr_msg1("abilityTriggers:preventBerryUse",
                                 pr_arg_pokemon(bt, "pokemonNameWithAffix", p));
            pr_queue_msg(bt, e, msg);
            pr_ab_params chew={.pokemon=p,.move=-1};
            pr_apply_ab_attrs(bt, PR_AC_CudChewConsumeBerryAbAttr, &chew);
            continue;
        }

        pr_unshift_common_anim(bt, e, pr_battler_index(bt, p), pr_battler_index(bt, p), COMMON_ANIM_USE_ITEM);

        int8_t ssc_stats[7], ssc_stages[7];
        int n_ssc = 0;

        for (int i = 0; i < bt->n_mods[side]; i++) {
            if (pr_berry_should_apply(bt, p, &bt->mods[side][i])) {
                int before_count=bt->n_mods[side];
                pr_apply_berry_ex(bt, e, p, &bt->mods[side][i], ssc_stats, ssc_stages, &n_ssc);
                /* Removing a depleted record shifts the next source record into i. */
                if(bt->n_mods[side]<before_count)i--;
            }
        }

        if (n_ssc > 0) {
            pr_unshift_stat_stage_change_ex(bt, e, pr_battler_index(bt, p), ssc_stats, ssc_stages, n_ssc, p, 0);
        }

        pr_refresh_berry_modifiers(bt, side);
        pr_ab_params berry_callback={.pokemon=p,.move=-1};
        pr_apply_ab_attrs(bt,PR_AC_HealFromBerryUseAbAttr,&berry_callback);
        pr_apply_ab_attrs(bt, PR_AC_CudChewConsumeBerryAbAttr, &berry_callback);
    }
}

static void check_status_effect_phase(pr_battle *bt, pr_engine *e)
{
    pr_speed_iter it;
    pr_pokemon *p;
    pr_speed_iter_init(bt, &it, ARENA_TAG_SIDE_BOTH);
    while ((p = pr_speed_iter_next(bt, &it)))
        if (p->status == STATUS_EFFECT_POISON || p->status == STATUS_EFFECT_TOXIC || p->status == STATUS_EFFECT_BURN)
            unshift_new(bt, e, PH_POST_TURN_STATUS_EFFECT, pr_battler_index(bt, p));
}

static void post_turn_status_effect_phase(pr_battle *bt, pr_engine *e, int ph)
{
    pr_pokemon *p = slot(bt, PH(ph)->battler);
    if (!is_active_on_field(p) || !(p->status == STATUS_EFFECT_POISON || p->status == STATUS_EFFECT_TOXIC
                                     || p->status == STATUS_EFFECT_BURN) || p->switch_out_status) {
        if (is_classic_final_boss(bt)) pr_init_final_boss_phase_two(bt,e,p);
        return;
    }
    status_increment_turn(p);
    bool cancelled = false;
    pr_ab_params pp = {.pokemon = p, .move = -1, .flag = &cancelled};
    pr_apply_ab_attrs(bt, PR_AC_BlockNonDirectDamageAbAttr, &pp);   /* Magic Guard */
    pr_apply_ab_attrs(bt, PR_AC_BlockStatusDamageAbAttr, &pp);      /* Poison Heal */
    if (!cancelled) {
        pr_queue_msg(bt, e, pr_status_msg(bt, p->status, PR_ST_ACTIVATION, p));
        double damage = 0;
        int32_t mhp = max_hp(p);
        switch (p->status) {
        case STATUS_EFFECT_POISON: damage = to_dmg_value(mhp / 8.0); break;
        case STATUS_EFFECT_TOXIC: damage = to_dmg_value((double)mhp * p->status_toxic_turns / 16); break;
        case STATUS_EFFECT_BURN: {
            damage = to_dmg_value(mhp / 16.0);
            pr_ab_params bp = {.pokemon = p, .move = -1, .value = &damage};
            pr_apply_ab_attrs(bt, PR_AC_ReduceBurnDamageAbAttr, &bp);  /* Heatproof */
            break;
        }
        }
        if (damage) {
            int32_t before = p->hp;
            pokemon_damage(bt, e, p, damage, true, false);       /* damage(damage, false, preventEndure) */
            pr_event_add(bt, e, PR_EV_HP, PH(ph)->battler, -1, 0, p->hp - before, NULL);   /* the damage number */
            pr_post_damage_ab(bt, p, damage, NULL);              /* the damage asked for, not the HP lost */
        }
        /* CommonAnim.POISON + (effect - 1), then end() */
        pr_event_add(bt, e, PR_EV_COMMON_ANIM, PH(ph)->battler, -1, COMMON_ANIM_POISON + (p->status - 1), 0, NULL);
    }
    if (is_classic_final_boss(bt)) pr_init_final_boss_phase_two(bt,e,p);
}

/* Weather.lapse(): the primal weathers last; otherwise a count left counts down and the weather
 * ends at 0 (none left: it lasts) */
static bool weather_lapse(pr_battle *bt)
{
    if (pr_weather_immutable(bt->weather)) return true;
    if (bt->weather_turns_left) return --bt->weather_turns_left != 0;
    return true;
}

/* Terrain.lapse() */
static bool terrain_lapse(pr_battle *bt)
{
    if (bt->terrain_turns_left) return --bt->terrain_turns_left != 0;
    return true;
}

/* arena.trySetWeather(weather, user). It fails on the weather already up; a primal weather (Harsh
 * Sun, Heavy Rain, Strong Winds) only gives way to another primal one or to none, and otherwise
 * plays its animation and its "continues" message. A user makes it last 5 turns plus 2 per
 * FieldEffectModifier (Mystical Rock) stack; a primal weather has no count. The start (animation
 * and message) or clear message, then for each Pokemon in speed order the tags tied to the old
 * weather go and PostWeatherChangeAbAttr runs. */
bool pr_try_set_weather(pr_battle *bt, pr_engine *e, int weather, const pr_pokemon *user)
{
    if (bt->weather == weather) return false;                 /* canSetWeather */
    if (pr_weather_immutable(bt->weather) && !pr_weather_immutable(weather) && weather != WEATHER_TYPE_NONE) {
        pr_unshift_common_anim(bt, e, default_battler(bt), -1, COMMON_ANIM_SUNNY + (bt->weather - 1));
        pr_queue_msg(bt, e, pr_msg0(pr_weather_key(bt->weather, PR_WEATHER_CONTINUE)));
        return false;
    }
    int32_t duration = 0;
    if (user) {
        duration = 5;
        pr_apply_field_effect_modifier(bt, user, &duration);
    }
    int old = bt->weather;
    bt->weather = weather;
    if (weather == WEATHER_TYPE_NONE) {
        bt->weather_turns_left = bt->weather_max_duration = 0;
        pr_queue_msg(bt, e, pr_msg0(pr_weather_key(old, PR_WEATHER_CLEAR)));
    } else {
        /* new Weather(weather, duration, duration): a primal weather keeps no count */
        bt->weather_turns_left = bt->weather_max_duration = pr_weather_immutable(weather) ? 0 : duration;
        pr_unshift_common_anim(bt, e, default_battler(bt), -1, COMMON_ANIM_SUNNY + (weather - 1));
        pr_queue_msg(bt, e, pr_msg0(pr_weather_key(weather, PR_WEATHER_START)));
    }
    pr_speed_iter it;
    pr_pokemon *p;
    pr_speed_iter_init(bt, &it, ARENA_TAG_SIDE_BOTH);
    while ((p = pr_speed_iter_next(bt, &it))) {
        pr_remove_field_boost_tag(bt, e, p, false, weather);
        pr_ab_params wp = {.pokemon = p, .move = -1, .field_type = weather};
        pr_apply_ab_attrs(bt, PR_AC_PostWeatherChangeAbAttr, &wp);
    }
    return true;
}

/* arena.trySetTerrain(terrain, ignoreAnim, user): the same for a terrain, with no primal case,
 * an animation only when ignoreAnim is off (moves turn it off), and Mimicry
 * (TerrainEventTypeChangeAbAttr) after PostTerrainChangeAbAttr */
bool pr_try_set_terrain(pr_battle *bt, pr_engine *e, int terrain, bool ignore_anim, const pr_pokemon *user)
{
    if (bt->terrain == terrain) return false;                 /* canSetTerrain */
    int32_t duration = 0;
    if (user) {
        duration = 5;
        pr_apply_field_effect_modifier(bt, user, &duration);
    }
    int old = bt->terrain;
    bt->terrain = terrain;
    bt->terrain_turns_left = bt->terrain_max_duration = terrain == TERRAIN_NONE ? 0 : duration;
    if (terrain != TERRAIN_NONE && !ignore_anim)
        pr_unshift_common_anim(bt, e, default_battler(bt), -1, COMMON_ANIM_MISTY_TERRAIN + (terrain - 1));
    pr_queue_msg(bt, e, pr_msg0(terrain == TERRAIN_NONE ? pr_terrain_key(old, PR_TERRAIN_CLEAR)
                                                        : pr_terrain_key(terrain, PR_TERRAIN_START)));
    pr_speed_iter it;
    pr_pokemon *p;
    pr_speed_iter_init(bt, &it, ARENA_TAG_SIDE_BOTH);
    while ((p = pr_speed_iter_next(bt, &it))) {
        pr_remove_field_boost_tag(bt, e, p, true, terrain);
        pr_ab_params tp = {.pokemon = p, .move = -1, .field_type = terrain};
        pr_apply_ab_attrs(bt, PR_AC_PostTerrainChangeAbAttr, &tp);
        pr_ab_params mp = {.pokemon = p, .move = -1};
        pr_apply_ab_attrs(bt, PR_AC_TerrainEventTypeChangeAbAttr, &mp);
    }
    return true;
}

static void turn_end_phase(pr_battle *bt, pr_engine *e, int ph)
{
    /* currentBattle.incrementTurn() */
    bt->turn++;
    for (int i = 0; i < 4; i++) {
        bt->commands[i].command = -1;
        bt->pre_commands[i].command = -1;
        e->command_targets[i] = -1;
    }
    bt->battle_rng_live = false;
    bt->n_last_turn_order = 0;                            /* clearLastTurnOrder */
    if (bt->ability_bar_visible) {                        /* hideAbilityBar */
        int h = pr_phase_new(e, PH_HIDE_ABILITY, -1);
        pr_phase_unshift(bt, e, h);
    }
    pr_speed_iter it;
    pr_pokemon *p;
    if (!PH(ph)->upcoming_interlude) {
        pr_speed_iter_init(bt, &it, ARENA_TAG_SIDE_BOTH);
        while ((p = pr_speed_iter_next(bt, &it))) {
            if (!p->switch_out_status) {
                pr_lapse_tags(bt, e, p, BATTLER_TAG_LAPSE_TYPE_TURN_END);
                pr_apply_turn_heal_modifiers(bt, e, p);       /* TurnHealModifier (Leftovers) */
                turn_modifiers(bt, p->is_player, p);          /* TurnHealModifier (Leftovers) */
                /* Grassy Terrain heals a grounded Pokemon by 1/16 of its max HP, with a message */
                if (bt->terrain == TERRAIN_GRASSY && pr_is_grounded(bt, p, false))
                    pr_unshift_heal_msg(bt, e, pr_battler_index(bt, p), to_dmg_value(max_hp(p) / 16.0), PR_HEAL_MESSAGE,
                                        pr_msg1("battle:turnEndHpRestore", pr_arg_pokemon(bt, "pokemonName", p)));
                if (!p->is_player) {
                    pr_apply_enemy_turn_heal_modifiers(bt, e, p);
                    pr_apply_enemy_status_heal_modifiers(bt, e, p);
                    turn_modifiers(bt, false, p);
                }
                pr_ab_params pt = {.pokemon = p, .move = -1};
                pr_apply_ab_attrs(bt, PR_AC_PostTurnAbAttr, &pt);
            }
            if (p->is_dynamaxed) {
                if (p->dynamax_turns_left > 0) p->dynamax_turns_left--;
                if (p->dynamax_turns_left == 0) {
                    revert_dynamax(p);
                }
            }
            pr_apply_turn_status_modifiers(bt, e, p);        /* Toxic Orb / Flame Orb */
            pr_apply_turn_held_item_transfers(bt,e,p);
            turn_modifiers(bt, p->is_player, p);              /* TurnHeldItemTransferModifier */
            p->temp_turn_count++;
            p->temp_wave_turn_count++;
        }
        pr_arena_lapse_tags(bt, e);
    }
    if (bt->weather != WEATHER_TYPE_NONE && !weather_lapse(bt)) {
        pr_try_set_weather(bt, e, WEATHER_TYPE_NONE, NULL);
        /* triggerWeatherBasedFormChangesToNormal: Castform and Cherrim (form changes aren't ported) */
        pr_speed_iter_init(bt, &it, ARENA_TAG_SIDE_BOTH);
        while ((p = pr_speed_iter_next(bt, &it)))
            form_change(bt, p, PR_FCT_SpeciesFormChangeRevertWeatherFormTrigger);
    }
    if (bt->terrain != TERRAIN_NONE && !terrain_lapse(bt)) pr_try_set_terrain(bt, e, TERRAIN_NONE, true, NULL);
}

/* ---------------------------------------------------------------- presentation */

static void show_ability_phase(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *s = PH(ph);
    pr_pokemon *p = slot(bt, s->battler);
    if (!p) return;
    if (bt->ability_bar_visible) {
        bool passive = s->anim_result;
        pr_phase_unshift(bt, e, pr_phase_new(e, PH_HIDE_ABILITY, -1));
        pr_queue_ability_display(bt, e, p, passive, true);
        return;
    }
    if (p->is_player) bt->last_player_involved = (int8_t)(s->battler % 2);
    else bt->last_enemy_involved = (int8_t)(s->battler % 2);
    bt->ability_bar_visible = true;
    pr_event_add(bt, e, PR_EV_ABILITY_SHOW, s->battler, -1, s->anim, s->anim_result, NULL);
}

/* ---------------------------------------------------------------- StatStageChangePhase */

/* The constructor: selfTarget = sourcePokemon === getPokemon(); the changes keep only non-zero stages */
static int new_stat_stage_change(pr_battle *bt, pr_engine *e, int battler, const pr_phase *opts,
                                 const int8_t *stat, const int8_t *stages, int n)
{
    int ph = new_pokemon_phase(e, PH_STAT_STAGE_CHANGE, battler);
    if (ph < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); e->overflow = true; return -1; }
    pr_phase *q = PH(ph);
    q->pokemon = slot(bt, battler);
    q->source_p = opts->source_p;
    q->ssc_message = opts->ssc_message;
    q->ssc_ignore_abilities = opts->ssc_ignore_abilities;
    q->ssc_processed = opts->ssc_processed;
    q->ssc_source_type = opts->ssc_source_type;
    q->ssc_stockpile = opts->ssc_stockpile;
    q->ssc_self_target = opts->source_p && opts->source_p == slot(bt, battler);
    q->n_ssc = 0;
    for (int i = 0; i < n; i++)
        if (stages[i] != 0) {
            q->ssc_stat[q->n_ssc] = stat[i];
            q->ssc_stages[q->n_ssc++] = stages[i];
        }
    return ph;
}

int pr_unshift_stat_stage_change(pr_battle *bt, pr_engine *e, int battler, const int32_t *stats, int n_stats,
                                 int stages, pr_pokemon *source)
{
    int8_t st[7], sg[7];
    if (n_stats > 7) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return -1; }
    for (int i = 0; i < n_stats; i++) {
        st[i] = (int8_t)stats[i];
        sg[i] = (int8_t)stages;
    }
    pr_phase opts = {.source_p = source};
    int ph = new_stat_stage_change(bt, e, battler, &opts, st, sg, n_stats);
    if (ph >= 0) pr_phase_unshift(bt, e, ph);
    return ph;
}

int pr_unshift_stat_stage_change_ex(pr_battle *bt, pr_engine *e, int battler, const int8_t *stat,
                                    const int8_t *stages, int n, pr_pokemon *source, int source_type)
{
    if (battler < 0 || n > 7) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return -1; }
    pr_phase opts = {.source_p = source, .ssc_source_type = (uint8_t)source_type};
    int ph = new_stat_stage_change(bt, e, battler, &opts, stat, stages, n);
    if (ph >= 0) pr_phase_unshift(bt, e, ph);
    return ph;
}

/* StatStageChangePhase.start */
static void stat_stage_change_phase(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *q = PH(ph);
    pr_pokemon *p = slot(bt, q->battler);
    if (!is_active_on_field(p)) return;                                  /* isActive(true) */
    if (!q->ssc_processed) {
        /* applyStageMultipliers: StatStageChangeMultiplierAbAttr (Simple, Contrary), not simulated */
        if (!q->ssc_ignore_abilities) {
            double mult = 1;
            pr_ab_params pp = {.pokemon = p, .move = -1, .value = &mult};
            pr_apply_ab_attrs(bt, PR_AC_StatStageChangeMultiplierAbAttr, &pp);
            for (int i = 0; i < q->n_ssc; i++) q->ssc_stages[i] = (int8_t)(q->ssc_stages[i] * mult);
        }
        /* removeCancelledChanges: drops caused by another Pokemon can be stopped */
        int8_t ds[7], dg[7];
        int nd = 0;
        for (int i = 0; i < q->n_ssc; i++)
            if (q->ssc_stages[i] < 0) { ds[nd] = q->ssc_stat[i]; dg[nd++] = q->ssc_stages[i]; }
        if (!q->ssc_self_target && nd) {
            uint16_t cancelled = 0;                                      /* cancelledStats, bit per Stat */
            int n_cancelled = 0;
            /* applyTagsForSide(MIST, side, ...): MistTag.apply(simulated false, pokemon, changes,
             * cancelledStats, source): the source's Infiltrator gets through; otherwise every drop
             * is cancelled, with a message */
            int side = p->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY;
            for (int i = 0; i < bt->n_arena_tags; i++) {
                if (bt->arena_tags[i].type != ARENA_TAG_TYPE_MIST || !pr_arena_tag_on_side(&bt->arena_tags[i], side))
                    continue;
                if (q->source_p) {
                    bool bypassed = false;
                    pr_ab_params ip = {.pokemon = q->source_p, .move = -1, .flag = &bypassed};
                    pr_apply_ab_attrs(bt, PR_AC_InfiltratorAbAttr, &ip);
                    if (bypassed) continue;
                }
                q = PH(ph);
                for (int k = 0; k < q->n_ssc; k++)
                    if (q->ssc_stages[k] < 0 && !(cancelled & (1u << q->ssc_stat[k]))) {
                        cancelled |= (uint16_t)(1u << q->ssc_stat[k]);
                        n_cancelled++;
                    }
                if (n_cancelled) pr_queue_msg(bt, e, pr_msg_p(bt, "arenaTag:mistApply", p));
            }
            q = PH(ph);
            /* checkAbilityProtection when cancelledStats.size < changes.length; the abilities
             * see the drops only */
            pr_ab_params pp = {.pokemon = p, .move = -1, .ch_stat = ds, .ch_stages = dg, .n_ch = nd,
                               .cancelled_stats = &cancelled, .target = p};
            if (n_cancelled < q->n_ssc) {
                pr_apply_ab_attrs(bt, PR_AC_ProtectStatAbAttr, &pp);                       /* Clear Body... */
                pr_apply_ab_attrs(bt, PR_AC_ConditionalUserFieldProtectStatAbAttr, &pp);   /* Flower Veil */
                const pr_pokemon *ally = pr_get_ally(bt, p);
                if (ally) {
                    pr_ab_params ap = pp;
                    ap.pokemon = ally;
                    pr_apply_ab_attrs(bt, PR_AC_ConditionalUserFieldProtectStatAbAttr, &ap);
                }
                q = PH(ph);
                if (q->source_p && q->ssc_source_type != STAT_CHANGE_SOURCE_MIRROR_ARMOR
                    && !pr_get_battler_tag(p, BATTLER_TAG_TYPE_OCTOLOCK)) {
                    pr_ab_params rp = pp;                                        /* Mirror Armor */
                    rp.opponent = q->source_p;
                    pr_apply_ab_attrs(bt, PR_AC_ReflectStatStageChangeAbAttr, &rp);
                }
            }
            q = PH(ph);
            int k = 0;
            for (int i = 0; i < q->n_ssc; i++)
                if (!(cancelled & (1u << q->ssc_stat[i]))) {
                    q->ssc_stat[k] = q->ssc_stat[i];
                    q->ssc_stages[k++] = q->ssc_stages[i];
                }
            q->n_ssc = (uint8_t)k;
        }
        /* splitBySign: the drops go to a phase of their own, unshifted */
        int8_t pos_s[7], pos_g[7], neg_s[7], neg_g[7];
        int np = 0, nn = 0;
        for (int i = 0; i < q->n_ssc; i++) {
            if (q->ssc_stages[i] >= 0) { pos_s[np] = q->ssc_stat[i]; pos_g[np++] = q->ssc_stages[i]; }
            else { neg_s[nn] = q->ssc_stat[i]; neg_g[nn++] = q->ssc_stages[i]; }
        }
        if (np && nn) {
            memcpy(q->ssc_stat, pos_s, (size_t)np);
            memcpy(q->ssc_stages, pos_g, (size_t)np);
            q->n_ssc = (uint8_t)np;
            pr_phase opts = *q;
            opts.ssc_processed = true;
            int drop = new_stat_stage_change(bt, e, q->battler, &opts, neg_s, neg_g, nn);
            if (drop >= 0) pr_phase_unshift(bt, e, drop);
            q = PH(ph);
        }
    }
    if (!q->n_ssc) return;
    /* getAppliedChanges: clamped to [-6, 6] */
    int8_t applied[7];
    for (int i = 0; i < q->n_ssc; i++) {
        int cur = p->stat_stages[q->ssc_stat[i] - 1], to = cur + q->ssc_stages[i];
        to = to < -6 ? -6 : to > 6 ? 6 : to;
        applied[i] = (int8_t)(to - cur);
    }
    /* options.onChange: only Stockpile passes one, bound to its StockpilingTag. Each stat raised
     * by a positive applied stage counts once (the Pokemon's current tag stands in for the bound
     * instance: the phase runs right after the tag's onAdd) */
    if (q->ssc_stockpile) {
        pr_battler_tag *st = NULL;
        for (int i = 0; i < p->n_tags; i++)
            if (p->tags[i].type == BATTLER_TAG_TYPE_STOCKPILING) st = &p->tags[i];
        bool def = false, spdef = false;
        for (int i = 0; i < q->n_ssc; i++) {
            def |= q->ssc_stat[i] == STAT_DEF && applied[i] > 0;
            spdef |= q->ssc_stat[i] == STAT_SPDEF && applied[i] > 0;
        }
        if (st && def) st->stock_def++;
        if (st && spdef) st->stock_spdef++;
    }
    /* isIncrease; playStatChangeAnimation when a stage moves */
    bool increase = false;
    int32_t moved = 0;
    for (int i = 0; i < q->n_ssc; i++) {
        increase |= q->ssc_stages[i] > 0;
        if (applied[i]) moved |= 1 << q->ssc_stat[i];
    }
    if (moved) pr_event_add(bt, e, PR_EV_STAT_STAGES, q->battler, -1, increase, moved, NULL);
    /* queueStatChangeMessages: the given message (no caller passes one), or one per group of
     * equal applied stages (Map.groupBy: in first-seen order), getStatStageChangeDescriptionKey
     * with the stats joined as formatStatsFragment does */
    if (q->ssc_message) pr_queue_message(bt, e);
    else
        for (int i = 0; i < q->n_ssc; i++) {
            bool seen = false;
            for (int j = 0; j < i; j++) seen |= applied[j] == applied[i];
            if (seen) continue;
            int8_t group[7];
            int ng = 0;
            for (int j = i; j < q->n_ssc; j++)
                if (applied[j] == applied[i]) group[ng++] = (int8_t)q->ssc_stat[j];
            int st = applied[i] < 0 ? -applied[i] : applied[i];
            const char *key = st == 1 ? (increase ? "battle:statRose" : "battle:statFell")
                            : st == 2 ? (increase ? "battle:statSharplyRose" : "battle:statHarshlyFell")
                            : st > 2 && st <= 6 ? (increase ? "battle:statRoseDrastically" : "battle:statSeverelyFell")
                            : (increase ? "battle:statWontGoAnyHigher" : "battle:statWontGoAnyLower");
            pr_queue_msg(bt, e, pr_msg3(key, pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                                        pr_arg_stats("stats", group, ng), pr_arg_int("count", ng)));
        }
    /* updateStatStages */
    for (int i = 0; i < q->n_ssc; i++) {
        int cur = p->stat_stages[q->ssc_stat[i] - 1];
        if (applied[i] > 0 && cur < 6) p->stat_stages_increased = true;
        else if (applied[i] < 0 && cur > -6) p->stat_stages_decreased = true;
        int to = cur + applied[i];
        p->stat_stages[q->ssc_stat[i] - 1] = (int8_t)(to < -6 ? -6 : to > 6 ? 6 : to);
    }
    /* triggerReactionAbilities, on this.options.changes (before clamping) */
    int8_t cs[7], cg[7];
    int nc = q->n_ssc;
    bool raise = false, self_target = q->ssc_self_target;
    for (int i = 0; i < nc; i++) {
        cs[i] = q->ssc_stat[i];
        cg[i] = q->ssc_stages[i];
        raise |= cg[i] > 0;
    }
    if (q->ssc_source_type != STAT_CHANGE_SOURCE_OPPORTUNIST && raise) {
        pr_speed_iter it;
        pr_pokemon *o;
        pr_speed_iter_init(bt, &it, p->is_player ? ARENA_TAG_SIDE_ENEMY : ARENA_TAG_SIDE_PLAYER);
        while ((o = pr_speed_iter_next(bt, &it))) {                      /* Opportunist */
            pr_ab_params cp = {.pokemon = o, .move = -1, .ch_stat = cs, .ch_stages = cg, .n_ch = nc};
            pr_apply_ab_attrs(bt, PR_AC_StatStageChangeCopyAbAttr, &cp);
        }
    }
    pr_ab_params pp = {.pokemon = p, .move = -1, .ch_stat = cs, .ch_stages = cg, .n_ch = nc, .self_target = self_target};
    pr_apply_ab_attrs(bt, PR_AC_PostStatStageChangeAbAttr, &pp);         /* Defiant, Competitive */
    q = PH(ph);
    /* checkWhiteHerb: once no other StatStageChangePhase for this battler is queued */
    if (!pr_phase_exists(e, PH_STAT_STAGE_CHANGE, q->battler)) {
        pr_apply_reset_negative_stat_stage_modifier(bt, e, p);
    }
}

/* ---------------------------------------------------------------- dispatch */

/* ResetStatusPhase: pokemon.clearStatus(affectConfusion = false, reloadAssets = false) */
static void reset_status_phase(pr_battle *bt, pr_engine *e, int ph)
{
    pr_pokemon *p = PH(ph)->pokemon;
    clear_status(bt, e, p, PH(ph)->affect_confusion);
}

/* ---------------------------------------------------------------- PokemonHealPhase */

void pr_unshift_heal_msg(pr_battle *bt, pr_engine *e, int battler, double hp, unsigned flags, pr_msg msg)
{
    int ph = new_pokemon_phase(e, PH_POKEMON_HEAL, battler);
    if (ph < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); e->overflow = true; return; }
    PH(ph)->heal_hp = hp;
    PH(ph)->heal_flags = (uint8_t)flags;
    PH(ph)->msg = msg;
    pr_phase_unshift(bt, e, ph);
}

void pr_unshift_heal_at(pr_battle *bt, pr_engine *e, int battler, double hp, unsigned flags, const char *site)
{
    pr_msg m = pr_msg0(NULL);
    m.args[0].s = site;
    pr_unshift_heal_msg(bt, e, battler, hp, flags, m);
}

/* start() plays the HEALTH_UP animation (CommonAnimPhase) unless skipAnim or at full HP; either
 * way end() heals, then the phase ends */
static void pokemon_heal_phase(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *h = PH(ph);
    pr_pokemon *p = slot(bt, h->battler);
    unsigned f = h->heal_flags;
    bool message = f & PR_HEAL_MESSAGE;
    pr_msg text = h->msg;
    /* start(): CommonAnimPhase plays HEALTH_UP unless skipAnim or at full HP */
    if (p && !(f & PR_HEAL_SKIP_ANIM) && p->hp < max_hp(p))
        pr_event_add(bt, e, PR_EV_COMMON_ANIM, h->battler, -1, COMMON_ANIM_HEALTH_UP, 0, NULL);
    if (!p || (!(f & PR_HEAL_REVIVE) && !is_active_on_field(p))) return;
    if (pr_get_battler_tag(p, BATTLER_TAG_TYPE_HEAL_BLOCK) && h->heal_hp > 0) {
        pr_queue_msg(bt, e, pr_msg_p(bt, "battlerTags:healBlockCannotHeal", p));
        return;
    }
    /* doHealPokemon */
    if (h->heal_hp > 0 && p->hp >= max_hp(p)) {
        if (!(f & PR_HEAL_NO_FULL_HP_MESSAGE)) {
            message = true;
            text = pr_msg1("battle:hpIsFull", pr_arg_pokemon(bt, "pokemonName", p));
        }
    } else {
        double amount;
        if (f & PR_HEAL_REVIVE) {
            amount = to_dmg_value(h->heal_hp);
        } else {
            /* Healing Charm (HealingBoosterModifier) on the phase's side */
            turn_modifiers(bt, h->battler < 2, NULL);
            amount = floor(h->heal_hp);
            double cap = max_hp(p) - ((f & PR_HEAL_PREVENT_FULL) ? 1 : 0);
            if (amount > cap) amount = cap;
        }
        if (amount < 0) {
            damage_and_update(bt, e, p, -amount, HIT_RESULT_INDIRECT, false, NULL, false);
        } else {
            /* heal(amount); the damage number shows amount */
            double room = max_hp(p) - p->hp;
            p->hp += (int32_t)(amount < room ? amount : room);
            pr_event_add(bt, e, PR_EV_HP, h->battler, -1, 0, (int32_t)amount, NULL);
        }
    }
    if ((f & PR_HEAL_STATUS) && p->status) {
        message = true;
        text = pr_status_msg(bt, p->status, PR_ST_HEAL, p);
        reset_status_as_phase(bt, e, p, true, false);
    }
    if (f & PR_HEAL_RESTORE_PP) {
        for (int i = 0; i < PR_N_MOVES(p); i++) PR_PP_USED(p)[i] = 0;
        pr_sync_moveset_aliases(p);
    }
    if (message) pr_queue_msg(bt, e, text);
}

/* AttemptRunPhase.calculateEscapeChance(escapeAttempts) */
double pr_calculate_escape_chance(pr_battle *bt, int current_attempts)
{
    double enemy_speed = 0.0;
    double player_speed = 0.0;
    bool is_boss = false;

    for (int i = 0; i < 2; i++) {
        pr_pokemon *p = bt->field[i];
        if (p && p->hp > 0) {
            player_speed += pr_get_stat(p, STAT_SPD, true);
        }
    }
    for (int i = 2; i < 4; i++) {
        pr_pokemon *p = bt->field[i];
        if (p) {
            enemy_speed += pr_get_stat(p, STAT_SPD, true);
            if (p->is_boss) is_boss = true;
        }
    }

    if (enemy_speed <= 0.0) enemy_speed = 1.0;

    double speed_ratio = player_speed / enemy_speed;
    double speed_cap = is_boss ? 6.0 : 4.0;
    double min_chance = 5.0;
    double max_chance = is_boss ? 25.0 : 95.0;
    double escape_bonus = is_boss ? 2.0 : 10.0;
    double escape_slope = (max_chance - min_chance) / speed_cap;

    double chance = js_round(escape_slope * speed_ratio + min_chance + escape_bonus * current_attempts);
    if (chance < min_chance) chance = min_chance;
    if (chance > max_chance) chance = max_chance;
    return chance;
}

static void attempt_run_phase(pr_battle *bt, pr_engine *e, int ph)
{
    (void)ph;
    int current_attempts = bt->escape_attempts++;
    int escape_roll = pr_battle_rand_seed_int(bt, 100, 0);
    double escape_chance = pr_calculate_escape_chance(bt, current_attempts);

    pr_speed_iter it;
    pr_speed_iter_init(bt, &it, ARENA_TAG_SIDE_PLAYER);
    pr_pokemon *p;
    while ((p = pr_speed_iter_next(bt, &it))) {
        pr_ab_params pp = {.pokemon = p, .value = &escape_chance};
        pr_apply_ab_attrs(bt, PR_AC_RunSuccessAbAttr, &pp);
    }

    if ((double)escape_roll < escape_chance) {
        for (int b = 2; b < 4; b++) {
            pr_pokemon *ep = bt->field[b];
            if (ep) {
                pr_ab_params pp = {.pokemon = ep};
                pr_apply_ab_attrs(bt, PR_AC_PreLeaveFieldAbAttr, &pp);
            }
        }

        pr_queue_msg(bt, e, pr_msg0("battle:runAwaySuccess"));

        bt->successful_run = true;

        /* clearEnemyHeldItemModifiers() */
        int w = 0;
        for (int i = 0; i < bt->n_mods[1]; i++) {
            if (!pr_mod_is(&bt->mods[1][i], PR_MOD_HELD_ITEM)) {
                if (pr_mod_is(&bt->mods[1][i], PR_MOD_PERSISTENT)) bt->mods[1][i].virtual_stack_count = 0;
                if (pr_mod_get_stack_count(&bt->mods[1][i])) bt->mods[1][w++] = bt->mods[1][i];
            } else {
                if (!pr_mod_is_ported_item(&bt->mods[1][i])) pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS);
            }
        }
        bt->n_mods[1] = w;
        /* updateModifiers(false): refresh all enemy party stats after removing held items. */
        if (!bt->n_party[1]) pr_note_unported(bt, PR_FEAT_PARTY);
        for (int k = 0; k < bt->n_party[1]; k++)
            if (bt->party[1][k]) pr_recalculate_stats(bt, bt->party[1][k]);

        for (int b = 2; b < 4; b++) {
            pr_pokemon *ep = bt->field[b];
            if (ep) {
                ep->hp = 0;
                ep->status = STATUS_EFFECT_FAINT;
                ep->pending_status = STATUS_EFFECT_NONE;
                ep->status_toxic_turns = 0;
                ep->status_sleep_turns = 0;
                ep->status_freeze_turns = -1;
            }
        }

        push_new(bt, e, PH_BATTLE_END, -1);
        if (pr_is_new_biome(bt)) {
            push_new(bt, e, PH_SELECT_BIOME, -1);
        }
        push_new(bt, e, PH_NEW_BATTLE, -1);
    } else {
        for (int b = 0; b < 2; b++) {
            pr_pokemon *pp = bt->field[b];
            if (pp && pp->hp > 0) {
                pp->failed_run_away = true;
            }
        }
        pr_queue_msg(bt, e, pr_msg0("battle:runAwayCannotEscape"));
    }
}

/* ToggleDoublePositionPhase: the first of getPlayerField() active on the field moves (its sprite;
 * to the centre after a faint left it alone); once there, one in field slot 1 swaps party slots
 * with the Pokemon in slot 0, and so takes field index 0 */
static void toggle_double_position_phase(pr_battle *bt)
{
    if (!bt->double_battle) return;                       /* singles: the one on the field is at index 0 */
    if (!bt->n_party[0]) { pr_note_unported(bt, PR_FEAT_PARTY); return; }
    int n = bt->n_party[0] < 2 ? bt->n_party[0] : 2;
    for (int k = 0; k < n; k++) {
        pr_pokemon *p = bt->party[0][k];
        if (!is_active_on_field(p)) continue;
        if (k == 1) {
            bt->party[0][1] = bt->party[0][0];
            bt->party[0][0] = p;
            bt->field[0] = bt->party[0][0];
            bt->field[1] = bt->party[0][1];
        }
        return;
    }
}

/* PokemonTransformPhase.start: callers check eligibility when they queue this phase.
 * The phase only guards a disappeared target; it does not repeat that condition. */
static void transform_phase(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *phase = PH(ph);
    pr_pokemon *user = phase->battler >= 0 && phase->battler < 4 ? bt->field[phase->battler] : NULL;
    pr_pokemon *target = phase->transform_target >= 0 && phase->transform_target < 4 ? bt->field[phase->transform_target] : NULL;
    if (!target) return;
    if (!user) { pr_note_unported(bt, PR_FEAT_PARTY); return; }
    const pr_form *form = pr_pokemon_form(target);
    int target_count = PR_N_MOVES(target);
    uint16_t copied_moves[4];
    memcpy(copied_moves, PR_MOVES(target), sizeof copied_moves);
    int target_gender = pr_get_gender(target);
    pr_detach_temporary_moves(e,user);
    user->transformed = true;
    user->summon_moves_active=true;user->summon_moves_alias_mask=0;
    user->transform_species = form->species;
    user->transform_form = form->form_index;
    user->ability_override = user->ability = target->ability;
    user->transform_gender = (int8_t)target_gender;
    pr_remove_tag(bt, e, user, BATTLER_TAG_TYPE_POWER_TRICK);
    for (int s = STAT_ATK; s <= STAT_SPD; s++) user->summon_stats[s] = pr_get_stat(target, s, false);
    memcpy(user->stat_stages, target->stat_stages, sizeof user->stat_stages);
    user->transform_n_moves = (uint8_t)target_count;
    for (int i = 0; i < 4; i++) {
        user->transform_moves[i] = i < target_count ? copied_moves[i] : 0;
        user->transform_pp_used[i] = user->transform_pp_up[i] = 0;
        int pp = pr_moves[user->transform_moves[i]].pp;
        user->transform_max_pp_override[i] = i < target_count && user->transform_moves[i] ? (pp < 5 ? pp : 5) : -1;
    }
    int8_t types[PR_MAX_TYPES];
    int n = pr_get_types(target, types, PR_TYPES_NO_TERA);
    pr_set_summon_types(bt, user, types, n);
    /* The source names target.name, not its illusion's rendered name. */
    pr_msg_arg target_name = {"targetName", PR_ARG_POKEMON_NAME,
        pr_msg_pokemon(target->species, target->form, 0), NULL,
        target->fusion_species ? pr_msg_pokemon(target->fusion_species, target->fusion_form, 0) : 0};
    pr_queue_msg(bt, e, pr_msg2("abilityTriggers:postSummonTransform",
        pr_arg_pokemon(bt, "pokemonNameWithAffix", user), target_name));
    pr_event_add(bt, e, PR_EV_TRANSFORM, phase->battler, phase->transform_target, 0, phase->transform_sound, NULL);
    /* Source assigns the copied ability first, then runs setTempAbility after
     * loadAssets resolves. In the headless engine the visual event is the boundary. */
    pr_set_temp_ability(bt, user, target->ability);
}

/* BattleScene.initFinalBossPhaseTwo: caller owns isClassicFinalBoss eligibility.
 * Dialogue, BGM and field scale are presentation; queue order remains gameplay. */
void pr_init_final_boss_phase_two(pr_battle *bt, pr_engine *e, pr_pokemon *p)
{
    if (!p || p->is_player || !p->boss_segments || p->form > 0 || p->boss_segment_index >= 1) return;
    bool found = false;
    for (int i = 0; i < bt->n_mods[1]; i++) {
        pr_modifier *m = &bt->mods[1][i];
        if (m->cls == PR_MOD_TURN_HELD_TRANSFER && m->pokemon_id == (int64_t)p->id) {
            m->is_transferable = false;
            found = true;
            break;
        }
    }
    if (!found) {
        if (bt->n_mods[1] >= PR_MAX_MODIFIERS) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return; }
        bt->mods[1][bt->n_mods[1]++] = (pr_modifier){.cls=PR_MOD_TURN_HELD_TRANSFER,
            .pokemon_id=p->id,.stack_count=1,.is_transferable=false};
    }
    /* The Source enemy constructor supplies these fixed moves, independent of
     * generic trainer/run generation. Inverse challenge requires its own context. */
    if (p->species == SPECIES_ID_ETERNATUS) {
        const uint16_t moves[] = {MOVE_ID_DYNAMAX_CANNON, MOVE_ID_CROSS_POISON,
            MOVE_ID_FLAMETHROWER, MOVE_ID_RECOVER};
        for (int i=0; i<4; i++) pr_set_original_move(p,i,moves[i]);
        p->pp_up[3] = -4;
    } else pr_note_unported(bt, PR_FEAT_FINAL_BOSS);
    form_change(bt,p,PR_FCT_SpeciesFormChangeManualTrigger);
    bt->double_battle = true;
    int count = 0;
    pr_pokemon *second = NULL;
    for (int i=0; i<bt->n_party[0]; i++) if (is_active(bt->party[0][i])) {
        if (++count == 2) second = bt->party[0][i];
    }
    if (count > 1) {
        push_new(bt,e,PH_TOGGLE_DOUBLE_POSITION,-1);
        if (!is_on_field(second)) {
            int summon = new_pokemon_phase(e,PH_SUMMON,1);
            if (summon >= 0) { PH(summon)->sw_player=true; PH(summon)->sw_field_index=1; }
            pr_phase_push(bt,e,summon);
            push_new(bt,e,PH_POST_SUMMON,1);
        }
    }
}

void pr_phase_run(pr_battle *bt, pr_engine *e, int ph)
{
    switch (PH(ph)->type) {
    case PH_TURN_START: turn_start(bt, e); break;
    case PH_DANCER: {
        pr_target_list *tl = tlist(e, PH(ph)->targets);
        pr_ab_params pp = {.pokemon = PH(ph)->pokemon, .opponent = PH(ph)->source_p,
            .move = PH(ph)->move_id, .move_targets = tl ? tl->v : NULL,
            .n_move_targets = tl ? tl->n : 0};
        pr_apply_ab_attrs(bt, PR_AC_PostMoveUsedAbAttr, &pp);
        break;
    }
    case PH_MOVE: move_phase(bt, e, ph); break;
    case PH_MOVE_EFFECT: move_effect_phase(bt, e, ph); break;
    case PH_MOVE_END: move_end_phase(bt, e, ph); break;
    case PH_FAINT: faint_phase(bt, e, ph); break;
    case PH_CHECK_INTERLUDE: check_interlude_phase(bt, e); break;
    case PH_WEATHER_EFFECT: weather_effect_phase(bt, e); break;
    case PH_POSITIONAL_TAG: positional_tag_phase(bt, e); break;
    case PH_BERRY: berry_phase(bt, e); break;
    case PH_CHECK_STATUS_EFFECT: check_status_effect_phase(bt, e); break;
    case PH_POST_TURN_STATUS_EFFECT: post_turn_status_effect_phase(bt, e, ph); break;
    case PH_TURN_END: turn_end_phase(bt, e, ph); break;
    case PH_MESSAGE: pr_event_add(bt, e, PR_EV_TEXT, -1, -1, 0, 0, &PH(ph)->msg); break;
    case PH_DAMAGE_ANIM:
        pr_event_add(bt, e, PR_EV_DAMAGE, PH(ph)->battler, -1, PH(ph)->anim_result, PH(ph)->anim_amount, NULL);
        if (is_classic_final_boss(bt)) pr_init_final_boss_phase_two(bt,e,slot(bt,PH(ph)->battler));
        break;
    case PH_COMMON_ANIM:
        pr_event_add(bt, e, PR_EV_COMMON_ANIM, PH(ph)->battler, PH(ph)->anim_target, PH(ph)->anim, 0, NULL);
        break;
    case PH_POKEMON_ANIM: pr_event_add(bt, e, PR_EV_POKEMON_ANIM, PH(ph)->battler, -1, PH(ph)->anim, 0, NULL); break;
    case PH_MOVE_ANIM:       /* a MoveAnim, or a MoveChargeAnim (ChargeAnim ids are 1000 and up) */
        pr_event_add(bt, e, PH(ph)->anim >= 1000 ? PR_EV_CHARGE_ANIM : PR_EV_MOVE_ANIM, PH(ph)->battler,
                     PH(ph)->anim_target, PH(ph)->anim, 0, NULL);
        break;
    case PH_LOAD_MOVE_ANIM: break;
    case PH_SHOW_ABILITY: show_ability_phase(bt, e, ph); break;
    case PH_HIDE_ABILITY:
        bt->ability_bar_visible = false;
        pr_event_add(bt, e, PR_EV_ABILITY_HIDE, -1, -1, 0, 0, NULL);
        break;
    case PH_RESET_STATUS: reset_status_phase(bt, e, ph); break;
    case PH_STAT_STAGE_CHANGE: stat_stage_change_phase(bt, e, ph); break;
    case PH_OBTAIN_STATUS_EFFECT: obtain_status_effect_phase(bt, e, ph); break;
    case PH_MOVE_CHARGE: move_charge_phase(bt, e, ph); break;
    case PH_ATTEMPT_RUN: attempt_run_phase(bt, e, ph); break;
    case PH_TRANSFORM: transform_phase(bt, e, ph); break;
    case PH_MOVE_HEADER: move_header_phase(bt, e, ph); break;
    case PH_POKEMON_HEAL: pokemon_heal_phase(bt, e, ph); break;
    case PH_SWITCH_SUMMON: pr_switch_summon_phase(bt, e, ph); break;
    case PH_POST_SUMMON: pr_post_summon_phase(bt, e, ph); break;
    case PH_SWITCH: pr_switch_phase(bt, e, ph); break;
    case PH_SUMMON: pr_summon_phase(bt, e, ph); break;
    case PH_INIT_ENCOUNTER: pr_init_encounter_phase(bt, e, ph); break;
    case PH_CHECK_SWITCH: pr_check_switch_phase(bt, e, ph); break;
    case PH_TOGGLE_DOUBLE_POSITION: toggle_double_position_phase(bt); break;
    case PH_MOVE_REFLECT: move_reflect_phase(bt, e, ph); break;
    case PH_SHINY_SPARKLE: pr_event_add(bt, e, PR_EV_SHINY_SPARKLE, PH(ph)->battler, -1, 0, 0, NULL); break;
    case PH_QUIET_FORM_CHANGE: quiet_form_change_phase(bt, e, ph); break;
    case PH_RETURN: return_phase(bt,e,ph); break;
    case PH_ATTEMPT_CAPTURE: attempt_capture_phase(bt, e, ph); break;
    default:
        /* a phase with no port: it was reported when it was queued */
        pr_note_unported(bt, (uint16_t)(PR_FEAT_PHASE_0 + PH(ph)->type));
        break;
    }
}

