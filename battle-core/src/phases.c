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
static bool is_active(const pr_pokemon *p) { return p && p->hp > 0; }
static bool is_on_field(const pr_pokemon *p) { return p && p->on_field && !p->switch_out_status; }
static bool is_active_on_field(const pr_pokemon *p) { return is_active(p) && is_on_field(p); }
static int32_t max_hp(const pr_pokemon *p) { return pr_get_stat(p, STAT_HP, false); }

/* getPokemon() of a PokemonPhase: the Pokemon in that field slot */
static pr_pokemon *slot(pr_battle *bt, int b) { return b >= 0 && b < 4 ? bt->field[b] : NULL; }

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

/* globalScene.triggerPokemonFormChange(pokemon, <trigger class>): only species with a form
 * change answering to that trigger can change; those are not ported yet */
static void form_change(pr_battle *bt, const pr_pokemon *p, int trigger_class)
{
    const pr_species *sp = pr_pokemon_species(p);   /* species ids are sparse: never index the table by id */
    if (sp && (sp->form_change_triggers & (1u << trigger_class))) pr_note_unported(bt, PR_FEAT_FORM_CHANGE);
}

/* globalScene.applyModifiers(<a turn-path modifier class>, player, pokemon): held items and
 * side-wide modifiers outside the damage calculation are not ported yet */
static void turn_modifiers(pr_battle *bt, bool player, const pr_pokemon *p)
{
    int side = player ? 0 : 1;
    for (int i = 0; i < bt->n_mods[side]; i++) {
        const pr_modifier *m = &bt->mods[side][i];
        if (!pr_mod_is(m, PR_MOD_HELD_ITEM) || !p || m->pokemon_id == (int64_t)p->id) {
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
    return ph->move.slot >= 0 ? ph->pokemon->pp_used[ph->move.slot] : ph->move.pp_used;
}

static int ref_move_pp(const pr_phase *ph)
{
    const pr_move *m = &pr_moves[ph->move.id];
    if (ph->move.slot < 0) return move_pp(m, 0, 0);
    int s = ph->move.slot;
    return move_pp(m, ph->pokemon->pp_up[s], ph->pokemon->max_pp_override[s]);
}

/* pokemonMove.usePp(count): ppUsed = min(ppUsed + count, getMovePp()) */
static void ref_use_pp(pr_phase *ph, int count)
{
    int v = ref_pp_used(ph) + count, max = ref_move_pp(ph);
    if (v > max) v = max;
    if (ph->move.slot >= 0) ph->pokemon->pp_used[ph->move.slot] = (uint8_t)v;
    else ph->move.pp_used = (uint8_t)v;
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
                                bool ignore_faint_phase, pr_pokemon *source);

static int new_pokemon_phase(pr_engine *e, pr_phase_type t, int battler)
{
    return pr_phase_new(e, t, (int8_t)battler);
}

static void unshift_new(pr_battle *bt, pr_engine *e, pr_phase_type t, int battler)
{
    pr_phase_unshift(bt, e, new_pokemon_phase(e, t, battler));
}

void pr_unshift_new(pr_battle *bt, pr_engine *e, pr_phase_type t, int battler) { unshift_new(bt, e, t, battler); }

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

/* ---------------------------------------------------------------- TurnStartPhase */

static void handle_fight_command(pr_battle *bt, pr_engine *e, pr_pokemon *p, int b)
{
    struct pr_turn_command *c = &bt->commands[b];
    if (c->no_move) return;                         /* no queuedMove */
    /* (turnCommand.targets, set to [fieldIndex] for a player's MoveId.NONE command, equals the move's
     * own targets there: NONE targets the user) */
    /* moveset.find(m => m.moveId === queuedMove.move && m.ppUsed < m.getMovePp()) ?? new PokemonMove */
    int slot = -1;
    for (int k = 0; k < p->n_moves; k++)
        if (p->moves[k] == c->move
            && p->pp_used[k] < move_pp(&pr_moves[c->move], p->pp_up[k], p->max_pp_override[k])) {
            slot = k;
            break;
        }
    if (pr_move_has_attr(c->move, PR_AC_MoveHeaderAttr)) {
        int h = new_pokemon_phase(e, PH_MOVE_HEADER, b);
        pr_phase_unshift(bt, e, h);
    }
    int ph = new_pokemon_phase(e, PH_MOVE, b);
    if (ph < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return; }
    pr_phase *m = PH(ph);
    m->pokemon = p;
    m->move.id = c->move;
    m->move.slot = (int8_t)slot;
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
        }
    }
    pr_speed_iter_init(bt, &it, ARENA_TAG_SIDE_BOTH);
    while ((p = pr_speed_iter_next(bt, &it))) {
        int b = pr_battler_index(bt, p);
        if (bt->commands[b].command != COMMAND_FIGHT) continue;
        pr_hook_ab(bt, p, PR_AC_BypassSpeedChanceAbAttr);     /* Quick Draw */
        turn_modifiers(bt, p->is_player, p);                  /* Quick Claw */
    }

    for (int k = 0; k < n; k++) {
        int b = order[k];
        pr_pokemon *mon = bt->field[b];
        struct pr_turn_command *c = &bt->commands[b];
        if (c->command < 0 || c->skip) continue;
        if (c->command == COMMAND_FIGHT) mon->order = (int8_t)k;
        switch (c->command) {
        case COMMAND_FIGHT: handle_fight_command(bt, e, mon, b); break;
        case COMMAND_BALL: unshift_new(bt, e, PH_ATTEMPT_CAPTURE, -1); break;
        case COMMAND_POKEMON: unshift_new(bt, e, PH_SWITCH_SUMMON, b); pr_note_unported(bt, PR_FEAT_PHASE_0 + PH_SWITCH_SUMMON); break;
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
    int end = new_pokemon_phase(e, PH_MOVE_END, m->battler);
    if (end < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return; }
    pr_pokemon *t[4];
    int n = active_in_list(bt, tlist(e, m->targets), t);
    pr_phase *me = PH(end);
    for (int i = 0; i < n; i++) me->end_targets[i] = (int8_t)pr_battler_index(bt, t[i]);
    me->n_end_targets = (uint8_t)n;
    me->was_follow_up = m->use_mode >= MOVE_USE_MODE_INDIRECT;   /* isVirtual */
    pr_phase_unshift(bt, e, end);
}

static void move_fail(pr_phase *m) { m->failed = true; m->entry.result = MOVE_RESULT_FAIL; }
static void move_cancel(pr_phase *m) { m->cancelled = true; m->entry.result = MOVE_RESULT_FAIL; }

void pr_cancel_current_move(pr_engine *e, const pr_pokemon *p)
{
    if (!e || e->current < 0) return;
    pr_phase *m = PH(e->current);
    if (m->type == PH_MOVE && (!p || m->pokemon == p)) move_cancel(m);
}

/* the Pokemon's moveQueue.shift() */
static void move_queue_shift(pr_pokemon *p, pr_turn_move *out)
{
    if (!p->n_move_queue) return;
    if (out) *out = p->move_queue[0];
    memmove(&p->move_queue[0], &p->move_queue[1], (size_t)(p->n_move_queue - 1) * sizeof p->move_queue[0]);
    p->n_move_queue--;
}

/* PreMoveMessageAttr.apply: a message when the move's message function returns one. The
 * functions live in the move list, so each user is ported by move id. */
static bool pre_move_message(pr_battle *bt, int move, const pr_pokemon *target, int mode)
{
    switch (move) {
    case MOVE_ID_STRUGGLE:
    case MOVE_ID_MAGNITUDE:          /* "Magnitude N!": always a text, whatever the roll */
        return true;
    case MOVE_ID_FICKLE_BEAM: {      /* doublePowerChanceMessageFunc(30): the same roll as its power */
        int roll = pr_wave_seed_roll(bt, 100);
        return roll >= 0 && roll < 30;
    }
    case MOVE_ID_POLTERGEIST:        /* attackedByItemMessageFunc: the target's first transferable item */
        return target && pr_held_item_count(bt, target, true) > 0;
    case MOVE_ID_CHILLY_RECEPTION:   /* no text when called indirectly */
        return mode < MOVE_USE_MODE_INDIRECT;
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
    pr_push_history_shared(bt, e, m->pokemon, m->entry, m->targets);
    pr_lapse_tags(bt, e, m->pokemon, BATTLER_TAG_LAPSE_TYPE_MOVE_EFFECT);
    move_queue_shift(m->pokemon, NULL);
}

static double pokemon_damage(pr_battle *bt, pr_engine *e, pr_pokemon *p, double damage, bool prevent_endure,
                             bool ignore_faint_phase);

/* ---- status effects (Status, Pokemon.cureStatus / resetStatus) ---- */

/* Pokemon.clearStatus(confusion, reloadAssets): status = null */
static void clear_status(pr_battle *bt, pr_pokemon *p, bool confusion)
{
    uint8_t last = p->status;
    p->status = 0;
    p->status_toxic_turns = p->status_sleep_turns = p->status_freeze_turns = -1;
    if (last == STATUS_EFFECT_SLEEP && pr_get_battler_tag(p, BATTLER_TAG_TYPE_NIGHTMARE))
        pr_note_unported(bt, PR_FEAT_TAG_LAPSE); /* lapseTag(NIGHTMARE) */
    if (confusion && pr_get_battler_tag(p, BATTLER_TAG_TYPE_CONFUSED))
        pr_note_unported(bt, PR_FEAT_TAG_LAPSE); /* lapseTag(CONFUSED) */
}

/* Pokemon.resetStatus(revive, confusion = false, reloadAssets = false, asPhase = true) */
static void reset_status_as_phase(pr_battle *bt, pr_engine *e, pr_pokemon *p, bool revive)
{
    if (!revive && p->status == STATUS_EFFECT_FAINT) return;
    int ph = new_pokemon_phase(e, PH_RESET_STATUS, pr_battler_index(bt, p));
    if (ph < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); e->overflow = true; return; }
    PH(ph)->pokemon = p;
    pr_phase_unshift(bt, e, ph);
}

void pr_reset_status_phase(pr_battle *bt, pr_engine *e, pr_pokemon *p, bool revive)
{
    reset_status_as_phase(bt, e, p, revive);
}

/* Pokemon.cureStatus(effect): the cure message, then the status is cleared at once */
static void cure_status(pr_battle *bt, pr_engine *e, pr_pokemon *p, int effect)
{
    if (effect != p->status) return;
    pr_queue_message(bt, e);
    clear_status(bt, p, false);
}

/* Status.incrementTurn(): the counters count down while truthy (undefined is -1 here) */
static void status_increment_turn(pr_pokemon *p)
{
    p->status_toxic_turns++;
    if (p->status_sleep_turns > 0) p->status_sleep_turns--;
    if (p->status_freeze_turns > 0) p->status_freeze_turns--;
}

/* MovePhase.triggerStatus(effect, cancel): the activation message and its animation */
static void trigger_status(pr_battle *bt, pr_engine *e, int ph, bool cancel)
{
    pr_phase *m = PH(ph);
    pr_queue_message(bt, e);
    unshift_new(bt, e, PH_COMMON_ANIM, pr_battler_index(bt, m->pokemon));
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
        reset_status_as_phase(bt, e, user, false);
        return false;
    }
    status_increment_turn(user);
    double turns = user->status_sleep_turns < 0 ? 0 : user->status_sleep_turns;
    pr_ab_params pp = {.pokemon = user, .move = -1, .value = &turns, .effect = STATUS_EFFECT_SLEEP};
    pr_apply_ab_attrs(bt, PR_AC_ReduceStatusEffectDurationAbAttr, &pp);   /* Early Bird */
    user->status_sleep_turns = (int16_t)turns;
    if (user->status_sleep_turns <= 0) {
        cure_status(bt, e, user, STATUS_EFFECT_SLEEP);
        return false;
    }
    /* applyMoveAttrs("BypassSleepAttr"): Sleep Talk, Snore */
    bool cancel = !pr_move_has_attr(m->move.id, PR_AC_BypassSleepAttr);
    trigger_status(bt, e, ph, cancel);
    return cancel;
}

/* MovePhase.checkFreeze */
static bool check_freeze(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *m = PH(ph);
    pr_pokemon *user = m->pokemon;
    if (user->status != STATUS_EFFECT_FREEZE) return false;
    if (m->use_mode == MOVE_USE_MODE_INDIRECT) {
        reset_status_as_phase(bt, e, user, false);
        return false;
    }
    int move = m->move.id;
    bool thaws = move_thaws_user(move);
    int8_t ty[3];
    int n = pr_get_types(user, ty, PR_TYPES_ORIGINAL_IF_STELLAR);
    bool fire = false;
    for (int i = 0; i < n; i++) fire |= ty[i] == POKEMON_TYPE_FIRE;
    if (thaws && (move != MOVE_ID_BURN_UP || fire)) {   /* thawed after the other checks (doThawCheck) */
        m->thaw = true;
        return false;
    }
    status_increment_turn(user);
    if (thaws || !pr_battle_rand_seed_int(bt, 4, 0) || (user->status_freeze_turns < 0 ? 0 : user->status_freeze_turns) <= 0) {
        cure_status(bt, e, user, STATUS_EFFECT_FREEZE);
        return false;
    }
    trigger_status(bt, e, ph, true);
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
            if (!quiet) pr_queue_message(bt, e);                  /* queueStatusImmuneMessage */
            return false;
        }
        if (pr_is_grounded(bt, p, false) && !ignore_field && bt->terrain == TERRAIN_MISTY) {
            if (!quiet) pr_queue_message(bt, e);
            return false;
        }
    }
    int8_t ty[3];
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
        if (!quiet) pr_queue_message(bt, e);
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
        if (!quiet) pr_queue_message(bt, e);
        return false;
    }
    return true;
}

bool pr_try_set_status(pr_battle *bt, pr_engine *e, pr_pokemon *p, int effect, pr_pokemon *source, int sleep_turns,
                       bool override_status, bool quiet)
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
    if (override_status) reset_status_as_phase(bt, e, p, false);
    else p->pending_status = (uint8_t)effect;
    int ph = new_pokemon_phase(e, PH_OBTAIN_STATUS_EFFECT, pr_battler_index(bt, p));
    if (ph < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); e->overflow = true; return true; }
    pr_phase *q = PH(ph);
    q->pokemon = p;
    q->ose_effect = (uint8_t)effect;
    q->ose_sleep_turns = (int8_t)sleep_turns;
    q->source_p = source;
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
    /* the animation's callback: the message, then (no species changes form on a status) */
    pr_queue_message(bt, e);
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
        pr_queue_message(bt, e);
        return true;
    }
    /* checkValidity */
    pr_target_list *tl = tlist(e, m->targets);
    if (mv->unimplemented) {
        move_cancel(m);
        pr_queue_message(bt, e);
        return true;
    }
    if (m->move.id == MOVE_ID_NONE || !tl || tl->n == 0) {
        move_cancel(m);
        return true;
    }
    /* (challenges restricting moves are not modelled: classic mode has none) */
    if (pr_lapse_tag(bt, e, user, BATTLER_TAG_TYPE_TRUANT, BATTLER_TAG_LAPSE_TYPE_PRE_MOVE) && m->cancelled) return true;
    /* checkPreUseInterrupt (Focus Punch) */
    if (pr_move_has_attr(m->move.id, PR_AC_PreUseInterruptAttr)) {
        pr_note_unported(bt, PR_AC_PreUseInterruptAttr);
        return false;
    }
    static const int tag_checks[] = {BATTLER_TAG_TYPE_FLINCHED, BATTLER_TAG_TYPE_DISABLED, BATTLER_TAG_TYPE_HEAL_BLOCK,
                                     BATTLER_TAG_TYPE_THROAT_CHOPPED};
    for (unsigned i = 0; i < sizeof tag_checks / sizeof *tag_checks; i++)
        if (pr_lapse_tag(bt, e, user, tag_checks[i], BATTLER_TAG_LAPSE_TYPE_PRE_MOVE) && m->cancelled) return true;
    /* checkGravity */
    if (pr_arena_get_tag_on_side(bt, ARENA_TAG_TYPE_GRAVITY, ARENA_TAG_SIDE_BOTH) && (mv->flags & MOVE_FLAGS_GRAVITY)) {
        pr_queue_message(bt, e);
        return true;
    }
    static const int tag_checks2[] = {BATTLER_TAG_TYPE_TAUNT, BATTLER_TAG_TYPE_IMPRISON, BATTLER_TAG_TYPE_CONFUSED};
    for (unsigned i = 0; i < sizeof tag_checks2 / sizeof *tag_checks2; i++)
        if (pr_lapse_tag(bt, e, user, tag_checks2[i], BATTLER_TAG_LAPSE_TYPE_PRE_MOVE) && m->cancelled) return true;
    /* checkPara: fully paralysed one time in eight */
    if (user->status == STATUS_EFFECT_PARALYSIS && pr_battle_rand_seed_int(bt, 8, 0) == 0) {
        trigger_status(bt, e, ph, true);
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
            if (bt->double_battle) pr_note_unported(bt, PR_FEAT_DOUBLES);
            target = d;
        }
    }
    tl->v[0] = (int8_t)target;
    pr_targets_changed(bt, e, m->targets);
    if (target == BATTLER_INDEX_ATTACKER) move_fail(m);
}

static void resolve_redirect_target(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *m = PH(ph);
    pr_target_list *tl = tlist(e, m->targets);
    if (!tl || tl->n != 1) return;
    pr_speed_iter it;
    pr_pokemon *p;
    pr_speed_iter_init(bt, &it, ARENA_TAG_SIDE_BOTH);
    while ((p = pr_speed_iter_next(bt, &it)))
        if (p != m->pokemon) pr_hook_ab(bt, p, PR_AC_RedirectMoveAbAttr);
    /* center-of-attention tags on the opponents (Follow Me, Rage Powder, Spotlight) */
    for (int i = 0; i < 4; i++) {
        pr_pokemon *o = bt->field[i];
        if (is_active(o) && pr_is_opponent(bt, m->pokemon, o) && pr_get_battler_tag(o, BATTLER_TAG_TYPE_CENTER_OF_ATTENTION))
            pr_note_unported(bt, PR_FEAT_TAG_LAPSE);
    }
}

static void fail_move(pr_battle *bt, pr_engine *e, int ph, bool due_to_terrain)
{
    pr_phase *m = PH(ph);
    pr_pokemon *user = m->pokemon;
    int id = m->move.id;
    if (due_to_terrain || id == MOVE_ID_ROAR || id == MOVE_ID_WHIRLWIND || id == MOVE_ID_TRICK_OR_TREAT
        || id == MOVE_ID_FORESTS_CURSE)
        pr_hook_ab(bt, user, PR_AC_PokemonTypeChangeAbAttr);
    pr_turn_move tm = {.move = (uint16_t)id, .use_mode = m->use_mode, .result = MOVE_RESULT_FAIL};
    pr_push_history_shared(bt, e, user, tm, m->targets);
    pr_queue_message(bt, e);
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
        /* FieldPriorityMoveImmunityAbAttr on the defending side (Queenly Majesty, Dazzling) */
        for (int i = 0; i < 4; i++) {
            pr_pokemon *o = bt->field[i];
            if (o && (i < 2) != user->is_player) pr_hook_ab(bt, o, PR_AC_FieldPriorityMoveImmunityAbAttr);
        }
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
    pr_hook_ab(bt, user, PR_AC_PokemonTypeChangeAbAttr);     /* Protean, Libero */
    if (!pr_move_apply_conditions(bt, m->move.id, user, nt ? targets[0] : NULL, 4)) {
        fail_move(bt, e, ph, false);
        return;
    }
    if (charging) {
        /* chargeMove: MoveChargePhase(battlerIndex, targets[0], move, NORMAL ? IGNORE_PP : useMode) */
        int c = new_pokemon_phase(e, PH_MOVE_CHARGE, m->battler);
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
    int fx = new_pokemon_phase(e, PH_MOVE_EFFECT, m->battler);
    if (fx < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return; }
    PH(fx)->targets = m->targets;
    PH(fx)->move_id = m->move.id;
    PH(fx)->use_mode = m->use_mode;
    /* the constructor drops the user from an ALL_NEAR_OTHERS list (doubles only) */
    pr_target_list *tl = tlist(e, m->targets);
    if (tl && pr_moves[m->move.id].target == MOVE_TARGET_ALL_NEAR_OTHERS)
        for (int k = 0; k < tl->n; k++)
            if (tl->v[k] == m->battler) pr_note_unported(bt, PR_FEAT_DOUBLES);
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
    if (pr_get_battler_tag(user, BATTLER_TAG_TYPE_ENCORE)) pr_note_unported(bt, PR_FEAT_TAG_LAPSE);
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
            pr_queue_message(bt, e);
        m = PH(ph);
    }
    if (m->cancelled) {
        handle_pre_move_failures(bt, e, ph);
        queue_move_end(bt, e, ph);
        return;
    }
    /* doThawCheck: the move thaws its user out once the checks have passed */
    if (!follow_up && !ignore_status && m->thaw) cure_status(bt, e, user, STATUS_EFFECT_FREEZE);

    int move = m->move.id;
    /* arena.ignoreAbilities (Sunsteel Strike and co. don't work when called indirectly) */
    if (pr_move_does_flag_effect_apply_ex(bt, move, MOVE_FLAGS_IGNORE_ABILITIES, user, NULL, mode >= MOVE_USE_MODE_INDIRECT)) {
        bt->ignore_abilities = true;
        bt->ignoring_effect_source = (int8_t)pr_battler_index(bt, user);
    }

    resolve_redirect_target(bt, e, ph);
    resolve_counter_attack_target(bt, e, ph);

    bool charging_move = is_charging_move(move);
    bool charging = charging_move && !pr_get_battler_tag(user, BATTLER_TAG_TYPE_CHARGING);
    bool releasing = charging_move && !charging;
    if (!releasing && mode < MOVE_USE_MODE_IGNORE_PP) {
        /* usePP: Pressure on each active target adds to the PP used */
        pr_pokemon *targets[4];
        int nt = active_in_list(bt, tlist(e, m->targets), targets);
        for (int i = 0; i < nt; i++) pr_hook_ab(bt, targets[i], PR_AC_IncreasePpUsedAbAttr);
        ref_use_pp(m, 1);
    }
    if (!follow_up) form_change(bt, user, PR_FCT_SpeciesFormChangePreMoveTrigger);

    /* showMoveText */
    if (move != MOVE_ID_NONE && !pr_get_battler_tag(user, BATTLER_TAG_TYPE_RECHARGING)
        && !pr_get_battler_tag(user, BATTLER_TAG_TYPE_INTERRUPTED)) {
        m->entry.move = (uint16_t)move;
        pr_queue_message(bt, e);
        if (pr_move_has_attr((uint16_t)move, PR_AC_PreMoveMessageAttr)) {
            /* applyMoveAttrs("PreMoveMessageAttr", pokemon, getActiveTargetPokemon()[0], move) */
            pr_pokemon *tg[4];
            int n = active_in_list(bt, tlist(e, m->targets), tg);
            if (pre_move_message(bt, move, n ? tg[0] : NULL, mode)) pr_queue_message(bt, e);
        }
    }

    /* secondFailureCheck */
    pr_pokemon *targets[4];
    int nt = active_in_list(bt, tlist(e, m->targets), targets);
    bool second_failed = false;
    if (!pr_move_apply_conditions(bt, move, user, nt ? targets[0] : NULL, 2)) {
        m->failed = true;
        second_failed = true;
    } else if (move_weather_cancelled(bt, user, move)) {
        /* the failed text is getWeatherBlockMessage(weatherType) */
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
        pr_queue_message(bt, e);
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
        pr_queue_message(bt, e);
        move_fail(m);
        move_queue_shift(user, NULL);
        pr_push_history_shared(bt, e, user, m->entry, m->targets);
    } else {
        pr_lapse_tags(bt, e, user, BATTLER_TAG_LAPSE_TYPE_MOVE);
        use_move(bt, e, ph, charging);
    }
    queue_move_end(bt, e, ph);
}

/* ---------------------------------------------------------------- MoveChargePhase */

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
    if (target && !is_active_on_field(target)) target = NULL;
    if (!user || !target || !is_charging_move(move)) return;     /* super.end() only */
    const pr_move *mv = &pr_moves[move];

    /* the charge animation's callback: showChargeText, applyMoveChargeAttrs("MoveEffectAttr"),
     * addTag(CHARGING, 1, move.id, user.id) */
    pr_queue_message(bt, e);
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
    if (pr_get_ally(bt, user)) pr_note_unported(bt, PR_FEAT_DOUBLES);   /* AllyStatMultiplierAbAttr */
    return acc / eva;
}

/* Move.calculateBattleAccuracy(user, target) */
static double battle_accuracy(pr_battle *bt, pr_pokemon *user, pr_pokemon *target, int move)
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
    pr_hook_ab(bt, target, PR_AC_WonderSkinAbAttr);
    if (acc == -1) return acc;
    bool ohko = pr_move_has_attr((uint16_t)move, PR_AC_OneHitKOAccuracyAttr);
    if (!ohko) turn_modifiers(bt, user->is_player, user);        /* Wide Lens */
    if (bt->weather == WEATHER_TYPE_FOG) acc = floor(acc * 0.9);
    if (!ohko && pr_arena_get_tag_on_side(bt, ARENA_TAG_TYPE_GRAVITY, ARENA_TAG_SIDE_BOTH)) acc = floor(acc * 1.67);
    return acc;
}

static bool check_bypass_acc_and_invuln(pr_battle *bt, pr_pokemon *user, pr_pokemon *target, int move)
{
    if (pr_has_ability_with_attr(bt, user, PR_AC_AlwaysHitAbAttr) || pr_has_ability_with_attr(bt, target, PR_AC_AlwaysHitAbAttr))
        return true;
    if (pr_move_has_attr((uint16_t)move, PR_AC_ToxicAccuracyAttr) && pr_is_of_type(user, POKEMON_TYPE_POISON)) return true;
    if (pr_get_battler_tag(user, BATTLER_TAG_TYPE_IGNORE_ACCURACY)) pr_note_unported(bt, PR_FEAT_TAG_LAPSE);
    return is_field_targeted(move);
}

/* MoveEffectPhase.hitCheck(target) */
static int hit_check(pr_battle *bt, pr_engine *e, int ph, pr_pokemon *target, double *eff_out)
{
    pr_phase *fx = PH(ph);
    pr_pokemon *user = slot(bt, fx->battler);
    int move = fx->move_id;
    const pr_move *mv = &pr_moves[move];
    *eff_out = 0;
    if (mv->target == MOVE_TARGET_USER) { *eff_out = 1; return HIT_CHECK_RESULT_HIT; }
    bool field_targeted = is_field_targeted(move);
    if (!is_active_on_field(target) && !field_targeted) return HIT_CHECK_RESULT_TARGET_NOT_ON_FIELD;
    if (!field_targeted && bt->double_battle) pr_note_unported(bt, PR_FEAT_DOUBLES);   /* Commander */
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
        pr_note_unported(bt, PR_FEAT_PHASE_0 + PH_MOVE_REFLECT);
        return HIT_CHECK_RESULT_REFLECTED;
    }
    if (field_targeted) { *eff_out = 1; return HIT_CHECK_RESULT_HIT; }
    bool cancel_msg = false;
    double eff = pr_get_move_effectiveness_ex(bt, target, user, move, false, &cancel_msg);
    if (eff == 0) return cancel_msg ? HIT_CHECK_RESULT_NO_EFFECT_NO_MESSAGE : HIT_CHECK_RESULT_NO_EFFECT;
    double accuracy = battle_accuracy(bt, user, target, move);
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
    switch (result) {
    case HIT_RESULT_EXTREMELY_EFFECTIVE: case HIT_RESULT_SUPER_EFFECTIVE: case HIT_RESULT_NOT_VERY_EFFECTIVE:
    case HIT_RESULT_MOSTLY_INEFFECTIVE: case HIT_RESULT_ONE_HIT_KO:
        pr_queue_message(bt, e);
    }
}

typedef struct { int result; double damage; bool critical; } move_damage;

/* Pokemon.damage(damage, ignoreSegments, preventEndure, ignoreFaintPhase) */
static double pokemon_damage(pr_battle *bt, pr_engine *e, pr_pokemon *p, double damage, bool prevent_endure,
                             bool ignore_faint_phase)
{
    if (p->hp <= 0) return 0;
    if (p->is_boss) pr_note_unported(bt, PR_FEAT_BOSS_SEGMENTS);
    if (!prevent_endure && p->hp - damage <= 0) {
        bool survive = false;
        if (p->hp >= 1 && pr_get_battler_tag(p, BATTLER_TAG_TYPE_ENDURING))
            survive = pr_lapse_tag(bt, e, p, BATTLER_TAG_TYPE_ENDURING, BATTLER_TAG_LAPSE_TYPE_CUSTOM);
        else if ((p->hp == max_hp(p) && pr_get_battler_tag(p, BATTLER_TAG_TYPE_STURDY))
                 || (p->hp >= 1 && pr_get_battler_tag(p, BATTLER_TAG_TYPE_ENDURE_TOKEN)))
            pr_note_unported(bt, PR_FEAT_TAG_LAPSE);
        if (!survive) turn_modifiers(bt, p->is_player, p);       /* Focus Band */
        if (survive) damage = p->hp - 1;
    }
    if (damage > p->hp) damage = p->hp;
    p->hp -= (int32_t)damage;
    if (p->hp <= 0 && !ignore_faint_phase) {
        pr_queue_faint(bt, e, pr_battler_index(bt, p), prevent_endure, -1);
        pr_lapse_tag(bt, e, p, BATTLER_TAG_TYPE_COMMANDED, BATTLER_TAG_LAPSE_TYPE_CUSTOM);
    }
    return damage;
}

/* Pokemon.damageAndUpdate(damage, {result, isCritical, ignoreSegments, ignoreFaintPhase, source}) */
static double damage_and_update(pr_battle *bt, pr_engine *e, pr_pokemon *p, double damage, int result,
                                bool ignore_faint_phase, pr_pokemon *source)
{
    bool indirect = result == HIT_RESULT_INDIRECT || result == HIT_RESULT_INDIRECT_KO;
    unshift_new(bt, e, PH_DAMAGE_ANIM, pr_battler_index(bt, p));
    if (p->switch_out_status && source) damage = 0;
    damage = pokemon_damage(bt, e, p, damage, indirect, ignore_faint_phase);
    if (!source || source->hit_count <= 1) pr_hook_ab(bt, p, PR_AC_PostDamageAbAttr);
    return damage;
}

double pr_damage_and_update(pr_battle *bt, pr_engine *e, pr_pokemon *p, double damage, int result,
                            bool ignore_faint_phase, pr_pokemon *source)
{
    return damage_and_update(bt, e, p, damage, result, ignore_faint_phase, source);
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
        damage_and_update(bt, e, user, d, HIT_RESULT_INDIRECT, false, NULL);
        pr_queue_message(bt, e);                               /* moveTriggers:keptGoingAndCrashed */
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
        turn_modifiers(bt, false, target);                       /* EnemyEndureChanceModifier */
    }
    double final = blocked ? 0 : damage_and_update(bt, e, target, d.damage, d.result, true, user);
    if (crit) pr_queue_message(bt, e);
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
    if (user->is_player && !target->is_player) turn_modifiers(bt, true, user);   /* DamageMoneyRewardModifier */
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
        && !pr_has_ability_with_attr(bt, target, PR_AC_IgnoreMoveEffectsAbAttr) && !pr_move_hits_substitute(bt, move, user, target))
        turn_modifiers(bt, user->is_player, user);
    /* applyOnGetHitAbEffects */
    pr_ab_params dp = {.pokemon = target, .opponent = user, .move = move, .hit_result = d.result, .damage = d.damage};
    pr_apply_ab_attrs(bt, PR_AC_PostDefendAbAttr, &dp);                  /* Static, Rough Skin, Justified... */
    if (d.critical) pr_apply_ab_attrs(bt, PR_AC_PostReceiveCritStatStageChangeAbAttr, &dp);   /* Anger Point */
    pr_lapse_tags(bt, e, target, BATTLER_TAG_LAPSE_TYPE_AFTER_HIT);
    pr_ab_params ap = {.pokemon = user, .opponent = target, .move = move, .hit_result = d.result, .damage = d.damage};
    pr_apply_ab_attrs(bt, PR_AC_PostAttackAbAttr, &ap);                  /* Poison Touch, Stench... */
    if (!user->is_player && pr_move_is_attack(move)) turn_modifiers(bt, false, target);   /* EnemyAttackStatusEffectChanceModifier */
    if (pr_move_is_attack(move)) turn_modifiers(bt, user->is_player, user);                /* ContactHeldItemTransferChanceModifier */
}

static void apply_move_effects(pr_battle *bt, pr_engine *e, int ph, pr_pokemon *target, double eff, bool first_target)
{
    pr_phase *fx = PH(ph);
    pr_pokemon *user = slot(bt, fx->battler);
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
        if (user->hit_count > 1) pr_hook_ab(bt, target, PR_AC_PostDamageAbAttr);
    }
}

pr_pokemon *pr_move_effect_user(pr_battle *bt, pr_engine *e, int *move)
{
    if (!e || e->current < 0 || e->pool[e->current].type != PH_MOVE_EFFECT) return NULL;
    const pr_phase *fx = &e->pool[e->current];
    if (move) *move = fx->move_id;
    return slot(bt, fx->battler);
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
        pr_pokemon *user = slot(bt, fx->battler);
        if (user) {
            user->hit_count = 1;
            user->hits_left = 1;
        }
    }
}

static void move_effect_end(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *fx = PH(ph);
    pr_pokemon *user = slot(bt, fx->battler);
    if (--user->hits_left >= 1 && me_first_target(bt, e, ph, NULL)) {
        /* addNextHitPhase */
        int nx = new_pokemon_phase(e, PH_MOVE_EFFECT, fx->battler);
        if (nx < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return; }
        PH(nx)->targets = fx->targets;
        PH(nx)->move_id = fx->move_id;
        PH(nx)->use_mode = fx->use_mode;
        pr_target_list *tl = tlist(e, fx->targets);
        PH(nx)->n_hit_checks = tl ? tl->n : 0;
        pr_phase_unshift(bt, e, nx);
        return;
    }
    int total = user->hit_count - (user->hits_left > 0 ? user->hits_left : 0);
    if (total > 1 || user->hits_left > 0) pr_queue_message(bt, e);
    turn_modifiers(bt, fx->battler < 2, user);                  /* HitHealModifier (Shell Bell) */
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
            if (is_active(p) && p != user) pr_hook_ab(bt, p, PR_AC_PostMoveUsedAbAttr);
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

static void move_effect_phase(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *fx = PH(ph);
    pr_pokemon *user = slot(bt, fx->battler);
    if (!user) return;
    int move = fx->move_id;
    if (user->is_player) bt->last_player_involved = (int8_t)(fx->battler % 2);
    else bt->last_enemy_involved = (int8_t)(fx->battler % 2);

    /* applyMoveAttrs("OverrideMoveEffectAttr", user, getFirstTarget() ?? null, move, overridden, useMode):
     * the move-calling attributes queue their move and return without overriding anything */
    const pr_move *omv = &pr_moves[move];
    for (unsigned i = 0; i < omv->nattr; i++) {
        const pr_attr *a = &pr_attrs[omv->attr0 + i];
        if (!pr_attr_is(a->cls, PR_AC_OverrideMoveEffectAttr)) continue;
        switch (a->cls) {
        case PR_AC_NaturePowerAttr: case PR_AC_CopyMoveAttr: case PR_AC_RandomMoveAttr: case PR_AC_RandomMovesetMoveAttr: {
            pr_pokemon *first0;
            me_first_target(bt, e, ph, &first0);
            call_move(bt, e, move, a, user, first0);
            break;
        }
        default:            /* DelayedAttackAttr (Future Sight), AwaitCombinedPledgeAttr, MovesetCopyMoveAttr (Mimic) */
            pr_note_unported(bt, a->cls);
        }
    }
    fx = PH(ph);
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
                int v = pr_battle_rand_seed_int(bt, 20, 0);
                pr_hook_ab(bt, user, PR_AC_MaxMultiHitAbAttr);
                hits = v >= 13 ? 2 : v >= 6 ? 3 : v >= 3 ? 4 : 5;
                break;
            }
            case MULTI_HIT_TYPE_TWO: hits = 2; break;
            case MULTI_HIT_TYPE_THREE: hits = 3; break;
            case MULTI_HIT_TYPE_TEN: hits = 10; break;
            default: pr_note_unported(bt, PR_AC_MultiHitAttr); break;   /* Beat Up's count */
            }
        }
        pr_hook_ab(bt, user, PR_AC_AddSecondStrikeAbAttr);
        turn_modifiers(bt, user->is_player, user);           /* Multi Lens */
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
            pr_note_unported(bt, PR_FEAT_PHASE_0 + PH_MOVE_REFLECT);
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

    /* postAnimCallback (the animation runs first; it queues nothing) */
    if (fx->first_hit && fx->use_mode != MOVE_USE_MODE_DELAYED_ATTACK) {
        pr_push_history_shared(bt, e, user, fx->entry, fx->targets);
        pr_hook_ab(bt, user, PR_AC_ExecutedMoveAbAttr);
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
            pr_queue_message(bt, e);
            /* fall through */
        case HIT_CHECK_RESULT_NO_EFFECT_NO_MESSAGE: case HIT_CHECK_RESULT_PROTECTED:
        case HIT_CHECK_RESULT_TARGET_NOT_ON_FIELD:
            miss_effects(bt, e, user, move, PR_AC_NoEffectAttr);
            break;
        case HIT_CHECK_RESULT_MISS:
            pr_queue_message(bt, e);
            miss_effects(bt, e, user, move, PR_AC_MissEffectAttr);
            break;
        case HIT_CHECK_RESULT_REFLECTED:
            unshift_new(bt, e, PH_MOVE_REFLECT, pr_battler_index(bt, targets[i]));
            break;
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
        pr_pokemon *t = slot(bt, me->end_targets[i]);
        if (!t) continue;
        pr_ab_params pp = {.pokemon = t, .move = -1};
        pr_apply_ab_attrs(bt, PR_AC_PostSummonRemoveEffectAbAttr, &pp);  /* Limber, Oblivious... */
    }
}

/* ---------------------------------------------------------------- FaintPhase */

static void reset_turn_data(pr_pokemon *p)
{
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

static void reset_summon_data(pr_pokemon *p)
{
    memset(p->stat_stages, 0, sizeof p->stat_stages);
    p->n_move_queue = 0;
    p->n_tags = 0;
    p->ability_suppressed = false;
    p->summon_hit_count = 0;
    p->ability_override = -1;
    p->ability = p->base_ability;
    memset(p->summon_stats, 0, sizeof p->summon_stats);
    p->type_override[0] = p->type_override[1] = -2;
    p->added_type = 0;
    p->n_move_history = 0;
    p->temp_turn_count = p->temp_wave_turn_count = 1;
    p->n_ab_applied_summon = 0;
}

/* the player's party entry's HP, reading the Pokemon itself when it is on the field */
static int32_t party_hp(pr_battle *bt, int side, int k)
{
    for (int i = 0; i < 4; i++)
        if (bt->field[i] && bt->field[i]->id == bt->party[side][k].id) return bt->field[i]->hp;
    return bt->party[side][k].hp;
}

static bool party_on_field(pr_battle *bt, int side, int k)
{
    for (int i = 0; i < 4; i++)
        if (bt->field[i] && bt->field[i]->id == bt->party[side][k].id) return is_on_field(bt->field[i]);
    return false;
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

static void faint_phase(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *f = PH(ph);
    pr_pokemon *p = slot(bt, f->battler);
    if (!p || p->hp > 0) return;
    bool player = f->battler < 2;
    pr_pokemon *source = f->source >= 0 ? slot(bt, f->source) : NULL;
    if (source) {
        if (pr_get_battler_tag(p, BATTLER_TAG_TYPE_DESTINY_BOND) || pr_get_battler_tag(p, BATTLER_TAG_TYPE_GRUDGE))
            pr_note_unported(bt, PR_FEAT_TAG_LAPSE);
    }
    reset_summon_data(p);
    if (!f->prevent_instant_revive) turn_modifiers(bt, player, p);   /* Reviver Seed */
    for (int i = 0; i < 2; i++) {
        pr_pokemon *q = bt->field[i];
        if (q && (is_active(q) || q->hp <= 0)) add_participant(bt, q->id);
    }
    if (bt->wave_index == 200 && !player) pr_note_unported(bt, PR_FEAT_FINAL_BOSS);

    /* doFaint */
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
    pr_queue_message(bt, e);
    form_change(bt, p, PR_FCT_SpeciesFormChangeActiveTrigger);
    /* resetTera */
    bool was_tera = p->terastallized;
    p->terastallized = false;
    p->stellar_types_boosted = 0;
    if (was_tera) form_change(bt, p, PR_FCT_SpeciesFormChangeLapseTeraTrigger);
    pr_hook_ab(bt, p, PR_AC_PostFaintAbAttr);
    pr_speed_iter it;
    pr_pokemon *q;
    pr_speed_iter_init(bt, &it, ARENA_TAG_SIDE_BOTH);
    while ((q = pr_speed_iter_next(bt, &it))) pr_hook_ab(bt, q, PR_AC_PostKnockOutAbAttr);
    if (p->n_attacks_received > 0 && is_on_field(source)) {
        pr_hook_ab(bt, source, PR_AC_PostVictoryAbAttr);
        if (pr_move_has_attr(p->attacks_received[0].move, PR_AC_PostVictoryStatStageChangeAttr))
            pr_note_unported(bt, PR_AC_PostVictoryStatStageChangeAttr);
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
        else if (bench > 0) push_new(bt, e, PH_SWITCH, -1);
    } else {
        unshift_new(bt, e, PH_VICTORY, f->battler);
        if (bt->battle_type == BATTLE_TYPE_TRAINER || bt->battle_type == BATTLE_TYPE_MYSTERY_ENCOUNTER)
            pr_note_unported(bt, PR_FEAT_PARTY);
    }
    if (bt->double_battle) pr_note_unported(bt, PR_FEAT_DOUBLES);

    /* the faint cry's callback, then the tween's */
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
        /* addFaintedEnemyScore */
        int iv = 0;
        for (int i = 0; i < 6; i++) iv += p->ivs[i];
        const pr_form *fm = pr_pokemon_form(p);
        double score = fm->base_exp * (fm->exp_boost ? 1.5 : 1) * ((double)p->level / bt->max_exp_level) * ((iv / 93.0) * 0.2 + 0.8);
        if (bt->max_exp_level <= 0) pr_note_unported(bt, PR_FEAT_WAVE);
        for (int i = 0; i < bt->n_mods[1]; i++)
            if (pr_mod_is(&bt->mods[1][i], PR_MOD_HELD_ITEM) && bt->mods[1][i].pokemon_id == (int64_t)p->id)
                pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS);                /* getScoreMultiplier */
        if (p->is_boss) pr_note_unported(bt, PR_FEAT_BOSS_SEGMENTS);
        bt->battle_score += ceil(score);
        /* addPostBattleLoot: the enemy's transferable held items */
        for (int i = 0; i < bt->n_mods[1]; i++)
            if (pr_mod_is(&bt->mods[1][i], PR_MOD_HELD_ITEM) && bt->mods[1][i].pokemon_id == (int64_t)p->id)
                pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS);
    }
    /* leaveField */
    reset_turn_data(p);
    pr_speed_iter_init(bt, &it, ARENA_TAG_SIDE_BOTH);
    while ((q = pr_speed_iter_next(bt, &it)))
        if (q != p)
            pr_remove_tags_by_source_id(bt, e, q, (int64_t)p->id);
    reset_summon_data(p);
    pr_hook_ab(bt, p, PR_AC_PreLeaveFieldAbAttr);
    p->switch_out_status = true;
    form_change(bt, p, PR_FCT_SpeciesFormChangeActiveTrigger);
    p->on_field = false;
}

/* ---------------------------------------------------------------- end of turn */

static void check_interlude_phase(pr_battle *bt, pr_engine *e)
{
    (void)e;
    if (bt->wave_index % 10 != 0) return;
    for (int i = 2; i < 4; i++)
        if (bt->field[i] && bt->field[i]->hp > 0) return;
    for (int k = 0; k < bt->n_party[1]; k++)
        if (party_hp(bt, 1, k) > 0) return;
    if (bt->n_party[1] == 0) pr_note_unported(bt, PR_FEAT_PARTY);
    pr_note_unported(bt, PR_FEAT_PHASE_0 + PH_CHECK_INTERLUDE);     /* onInterlude */
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
                int8_t ty[3];
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
                pr_queue_message(bt, e);                                   /* getWeatherDamageMessage */
                /* damageAndUpdate(damage, {result: INDIRECT, ignoreSegments: true}) */
                damage_and_update(bt, e, p, to_dmg_value(max_hp(p) / 16.0), HIT_RESULT_INDIRECT, false, NULL);
            }
        }
    }
    pr_speed_iter_init(bt, &it, ARENA_TAG_SIDE_BOTH);
    while ((p = pr_speed_iter_next(bt, &it))) {
        if (p->switch_out_status) continue;
        pr_ab_params lp = {.pokemon = p, .move = -1};
        pr_apply_ab_attrs(bt, PR_AC_PostWeatherLapseAbAttr, &lp);
    }
}

static void positional_tag_phase(pr_battle *bt, pr_engine *e)
{
    (void)bt; (void)e;
    /* arena.positionalTagManager: Wish and Future Sight aren't modelled; the producer starts
     * every case without them, and nothing ported adds one */
}

static void berry_phase(pr_battle *bt, pr_engine *e)
{
    (void)e;
    pr_speed_iter it;
    pr_pokemon *p;
    pr_speed_iter_init(bt, &it, ARENA_TAG_SIDE_BOTH);
    while ((p = pr_speed_iter_next(bt, &it))) {
        /* eatBerries: findModifier(BerryModifier that shouldApply) */
        int side = p->is_player ? 0 : 1;
        for (int i = 0; i < bt->n_mods[side]; i++)
            if (pr_mod_is(&bt->mods[side][i], PR_MOD_HELD_ITEM) && bt->mods[side][i].pokemon_id == (int64_t)p->id)
                pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS);
        pr_hook_ab(bt, p, PR_AC_CudChewConsumeBerryAbAttr);
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
        if (bt->wave_index == 200) pr_note_unported(bt, PR_FEAT_FINAL_BOSS);  /* end(): isClassicFinalBoss */
        return;
    }
    status_increment_turn(p);
    bool cancelled = false;
    pr_ab_params pp = {.pokemon = p, .move = -1, .flag = &cancelled};
    pr_apply_ab_attrs(bt, PR_AC_BlockNonDirectDamageAbAttr, &pp);   /* Magic Guard */
    pr_apply_ab_attrs(bt, PR_AC_BlockStatusDamageAbAttr, &pp);      /* Poison Heal */
    if (!cancelled) {
        pr_queue_message(bt, e);
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
            pokemon_damage(bt, e, p, damage, true, false);       /* damage(damage, false, preventEndure) */
            pr_hook_ab(bt, p, PR_AC_PostDamageAbAttr);
        }
    }
    if (bt->wave_index == 200) pr_note_unported(bt, PR_FEAT_FINAL_BOSS);
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
        unshift_new(bt, e, PH_COMMON_ANIM, default_battler(bt));  /* getWeatherAnim(oldWeatherType) */
        pr_queue_message(bt, e);                              /* getLegendaryWeatherContinuesMessage */
        return false;
    }
    int32_t duration = 0;
    if (user) {
        duration = 5;
        pr_apply_field_effect_modifier(bt, user, &duration);
    }
    bt->weather = weather;
    if (weather == WEATHER_TYPE_NONE) {
        bt->weather_turns_left = bt->weather_max_duration = 0;
        pr_queue_message(bt, e);                              /* getWeatherClearMessage(oldWeatherType) */
    } else {
        /* new Weather(weather, duration, duration): a primal weather keeps no count */
        bt->weather_turns_left = bt->weather_max_duration = pr_weather_immutable(weather) ? 0 : duration;
        unshift_new(bt, e, PH_COMMON_ANIM, default_battler(bt));  /* getWeatherAnim(weather) */
        pr_queue_message(bt, e);                              /* getWeatherStartMessage */
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
    bt->terrain = terrain;
    bt->terrain_turns_left = bt->terrain_max_duration = terrain == TERRAIN_NONE ? 0 : duration;
    if (terrain != TERRAIN_NONE && !ignore_anim)
        unshift_new(bt, e, PH_COMMON_ANIM, default_battler(bt));  /* getTerrainAnim(terrain) */
    pr_queue_message(bt, e);                                  /* getTerrainStartMessage / getTerrainClearMessage */
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

static void turn_end_phase(pr_battle *bt, pr_engine *e)
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
    pr_speed_iter_init(bt, &it, ARENA_TAG_SIDE_BOTH);
    while ((p = pr_speed_iter_next(bt, &it))) {
        if (!p->switch_out_status) {
            pr_lapse_tags(bt, e, p, BATTLER_TAG_LAPSE_TYPE_TURN_END);
            turn_modifiers(bt, p->is_player, p);          /* TurnHealModifier (Leftovers) */
            /* Grassy Terrain heals a grounded Pokemon by 1/16 of its max HP, with a message */
            if (bt->terrain == TERRAIN_GRASSY && pr_is_grounded(bt, p, false))
                pr_unshift_heal(bt, e, pr_battler_index(bt, p), to_dmg_value(max_hp(p) / 16.0), PR_HEAL_MESSAGE);
            if (!p->is_player) turn_modifiers(bt, false, p);
            pr_ab_params pt = {.pokemon = p, .move = -1};
            pr_apply_ab_attrs(bt, PR_AC_PostTurnAbAttr, &pt);
        }
        turn_modifiers(bt, p->is_player, p);              /* TurnStatusEffectModifier, TurnHeldItemTransferModifier */
        p->temp_turn_count++;
        p->temp_wave_turn_count++;
    }
    pr_arena_lapse_tags(bt, e);
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
        pr_phase_unshift(bt, e, pr_phase_new(e, PH_HIDE_ABILITY, -1));
        pr_phase_unshift(bt, e, pr_phase_new(e, PH_SHOW_ABILITY, s->battler));
        return;
    }
    if (p->is_player) bt->last_player_involved = (int8_t)(s->battler % 2);
    else bt->last_enemy_involved = (int8_t)(s->battler % 2);
    bt->ability_bar_visible = true;
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
                if (n_cancelled) pr_queue_message(bt, e);                /* mistApply */
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
    /* options.onChange: no caller on the ported paths passes one */
    /* queueStatChangeMessages: the given message, or one per distinct applied stage count */
    if (q->ssc_message) pr_queue_message(bt, e);
    else
        for (int i = 0; i < q->n_ssc; i++) {
            bool seen = false;
            for (int j = 0; j < i; j++) seen |= applied[j] == applied[i];
            if (!seen) pr_queue_message(bt, e);
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
        bool lowered = false;
        for (int i = 0; i < 7; i++) lowered |= p->stat_stages[i] < 0;
        int side = q->battler < 2 ? 0 : 1;
        for (int i = 0; lowered && i < bt->n_mods[side]; i++)
            if (pr_mod_is(&bt->mods[side][i], PR_MOD_HELD_ITEM) && bt->mods[side][i].pokemon_id == (int64_t)p->id) {
                pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS);            /* a White Herb may reset them */
                break;
            }
    }
}

/* ---------------------------------------------------------------- dispatch */

/* ResetStatusPhase: pokemon.clearStatus(affectConfusion = false, reloadAssets = false) */
static void reset_status_phase(pr_battle *bt, pr_engine *e, int ph)
{
    pr_pokemon *p = PH(ph)->pokemon;
    uint8_t last = p->status;
    p->status = 0;
    p->status_toxic_turns = p->status_sleep_turns = p->status_freeze_turns = -1;
    if (last == STATUS_EFFECT_SLEEP && pr_get_battler_tag(p, BATTLER_TAG_TYPE_NIGHTMARE))
        pr_note_unported(bt, PR_FEAT_TAG_LAPSE); /* lapseTag(NIGHTMARE) */
}

/* ---------------------------------------------------------------- PokemonHealPhase */

void pr_unshift_heal(pr_battle *bt, pr_engine *e, int battler, double hp, unsigned flags)
{
    int ph = new_pokemon_phase(e, PH_POKEMON_HEAL, battler);
    if (ph < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); e->overflow = true; return; }
    PH(ph)->heal_hp = hp;
    PH(ph)->heal_flags = (uint8_t)flags;
    pr_phase_unshift(bt, e, ph);
}

/* start() plays the HEALTH_UP animation (CommonAnimPhase) unless skipAnim or at full HP; either
 * way end() heals, then the phase ends */
static void pokemon_heal_phase(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *h = PH(ph);
    pr_pokemon *p = slot(bt, h->battler);
    unsigned f = h->heal_flags;
    bool message = f & PR_HEAL_MESSAGE;
    if (!p || (!(f & PR_HEAL_REVIVE) && !is_active_on_field(p))) return;
    if (pr_get_battler_tag(p, BATTLER_TAG_TYPE_HEAL_BLOCK) && h->heal_hp > 0) {
        pr_queue_message(bt, e);                                  /* healBlock.onActivation */
        return;
    }
    /* doHealPokemon */
    if (h->heal_hp > 0 && p->hp >= max_hp(p)) {
        if (!(f & PR_HEAL_NO_FULL_HP_MESSAGE)) message = true;    /* "HP is full" */
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
            damage_and_update(bt, e, p, -amount, HIT_RESULT_INDIRECT, false, NULL);
        } else {
            /* heal(amount) */
            double room = max_hp(p) - p->hp;
            p->hp += (int32_t)(amount < room ? amount : room);
        }
    }
    if ((f & PR_HEAL_STATUS) && p->status) {
        message = true;
        reset_status_as_phase(bt, e, p, true);
    }
    if (f & PR_HEAL_RESTORE_PP)
        for (int i = 0; i < p->n_moves; i++) p->pp_used[i] = 0;
    if (message) pr_queue_message(bt, e);
}

void pr_phase_run(pr_battle *bt, pr_engine *e, int ph)
{
    switch (PH(ph)->type) {
    case PH_TURN_START: turn_start(bt, e); break;
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
    case PH_TURN_END: turn_end_phase(bt, e); break;
    case PH_MESSAGE: case PH_DAMAGE_ANIM: case PH_COMMON_ANIM: case PH_POKEMON_ANIM: case PH_LOAD_MOVE_ANIM: break;
    case PH_SHOW_ABILITY: show_ability_phase(bt, e, ph); break;
    case PH_HIDE_ABILITY: bt->ability_bar_visible = false; break;
    case PH_RESET_STATUS: reset_status_phase(bt, e, ph); break;
    case PH_STAT_STAGE_CHANGE: stat_stage_change_phase(bt, e, ph); break;
    case PH_OBTAIN_STATUS_EFFECT: obtain_status_effect_phase(bt, e, ph); break;
    case PH_MOVE_CHARGE: move_charge_phase(bt, e, ph); break;
    case PH_POKEMON_HEAL: pokemon_heal_phase(bt, e, ph); break;
    default:
        /* a phase with no port: it was reported when it was queued */
        pr_note_unported(bt, (uint16_t)(PR_FEAT_PHASE_0 + PH(ph)->type));
        break;
    }
}
