/* A Pokemon switched out by a move during a turn (src/data/moves/move.ts: ForceSwitchOutAttr, which
 * ChillyReceptionAttr and PartingShotAttr use too): the checks, the Pokemon leaving the field and
 * the phase that brings in its replacement. The player's party prompt (SwitchPhase) is where a
 * turn hands control back to the caller; a trainer's pick comes from bt->next_summon_index; a
 * wild Pokemon that flees ends the battle (BattleEndPhase). Single battles, wild or trainer, are
 * modelled: a mystery encounter's own rules report themselves unported (PR_FEAT_TRAINER). */
#include "pr/engine.h"
#include <math.h>
#include <string.h>

/* isActive(true): allowed in battle (not fainted; challenges are not modelled) and on the field */
static bool is_active_on_field(const pr_pokemon *p)
{
    return p && p->hp > 0 && p->on_field && !p->switch_out_status;
}

/* party.filter(p => p.isAllowedInBattle() && !p.isOnField()): the indexes, in party order */
static int eligible_switch_ins(const pr_battle *bt, int side, int out[PR_MAX_PARTY])
{
    int n = 0;
    for (int k = 0; k < bt->n_party[side]; k++) {
        const pr_pokemon *p = bt->party[side][k];
        bool on_field = p->on_field && !p->switch_out_status;
        if (p->hp > 0 && !on_field) out[n++] = k;
    }
    return n;
}

/* The enemy party's Pokemon that could come in for `out`: allowed in battle, off the field and,
 * when `same_slot`, owned by the same trainer (trainerSlot) */
static int enemy_reserves(const pr_battle *bt, const pr_pokemon *out, bool same_slot, int res[PR_MAX_PARTY])
{
    int n = 0;
    for (int k = 0; k < bt->n_party[1]; k++) {
        const pr_pokemon *p = bt->party[1][k];
        bool on_field = p->on_field && !p->switch_out_status;
        if (p->hp > 0 && !on_field && (!same_slot || p->trainer_slot == out->trainer_slot)) res[n++] = k;
    }
    return n;
}

/* currentBattle.trainer.getNextSummonIndex(trainerSlot), or -2 when the game gave no pick */
static int next_summon_index(pr_battle *bt, int trainer_slot)
{
    if (!bt->next_summon_index) { pr_note_unported(bt, PR_FEAT_TRAINER); return -2; }
    return bt->next_summon_index(bt, trainer_slot, bt->next_summon_ctx);
}

/* move.getAttrs("ForceSwitchOutAttr").find(attr => attr.switchType === FORCE_SWITCH) */
static bool has_force_switch_attr(int move)
{
    const pr_move *mv = &pr_moves[move];
    for (unsigned i = 0; i < mv->nattr; i++) {
        const pr_attr *a = &pr_attrs[mv->attr0 + i];
        if (a->cls == PR_AC_ForceSwitchOutAttr
            && ((const pr_attr_ForceSwitchOutAttr *)a->p)->switchType == SWITCH_TYPE_FORCE_SWITCH)
            return true;
        if (a->cls == PR_AC_ChillyReceptionAttr
            && ((const pr_attr_ChillyReceptionAttr *)a->p)->switchType == SWITCH_TYPE_FORCE_SWITCH)
            return true;
    }
    return false;
}

bool pr_switch_out_condition(pr_battle *bt, bool self_switch, int switch_type, const pr_pokemon *user,
                             const pr_pokemon *target, int move)
{
    const pr_pokemon *out = self_switch ? user : target;
    if (!out) { pr_note_unported(bt, PR_FEAT_MOVE_CONDITION); return true; }   /* upstream would throw */
    bool player = out->is_player;
    /* a Dondozo with an allied Tatsugiri in its mouth can't leave (doubles only) */
    if (pr_get_battler_tag(out, BATTLER_TAG_TYPE_COMMANDED)) pr_note_unported(bt, PR_FEAT_DOUBLES);

    if (!self_switch) {
        if (!target) { pr_note_unported(bt, PR_FEAT_MOVE_CONDITION); return true; }
        if (pr_move_hits_substitute(bt, move, user, target)) return false;
        /* Roar and Whirlwind in a trainer battle whose targeted side has one Pokemon left */
        if (has_force_switch_attr(move) && bt->battle_type == BATTLE_TYPE_TRAINER) {
            int target_side = player ? 0 : 1;
            if (!bt->n_party[target_side]) { pr_note_unported(bt, PR_FEAT_PARTY); return true; }
            int remaining = 0;
            for (int k = 0; k < bt->n_party[target_side]; k++) remaining += bt->party[target_side][k]->hp > 0;
            if (remaining <= 1) return false;
        }
        /* no forcing a wild Pokemon out of a mystery encounter that doesn't allow fleeing */
        if (!player && bt->battle_type == BATTLE_TYPE_MYSTERY_ENCOUNTER) pr_note_unported(bt, PR_FEAT_TRAINER);
        /* Suction Cups, Guard Dog: applied for real, so the ability shows */
        bool blocked = false;
        pr_ab_params ap = {.pokemon = target, .move = -1, .flag = &blocked};
        pr_apply_ab_attrs(bt, PR_AC_ForceSwitchOutImmunityAbAttr, &ap);
        if (blocked) return false;
    }

    if (!player && bt->battle_type == BATTLE_TYPE_WILD) {
        /* no Baton Pass for a wild Pokemon, no fleeing on a wave that is a multiple of 10, and
         * U-turn and co. don't take a wild Pokemon away */
        return switch_type != SWITCH_TYPE_BATON_PASS && bt->wave_index % 10 != 0
               && !(self_switch && pr_moves[move].category != MOVE_CATEGORY_STATUS);
    }
    int side = player ? 0 : 1, eligible[PR_MAX_PARTY];
    if (!bt->n_party[side]) { pr_note_unported(bt, PR_FEAT_PARTY); return true; }
    if (!player) return enemy_reserves(bt, out, true, eligible) > 0;       /* the trainer's own Pokemon */
    return eligible_switch_ins(bt, side, eligible) > 0;
}

/* hpDroppedBelowHalf(target): its damage this turn took it from at least half its max HP to below */
static bool hp_dropped_below_half(const pr_pokemon *p)
{
    double hp = p->hp, max = pr_get_stat(p, STAT_HP, true), initial = hp + p->damage_taken;
    return initial >= max / 2 && hp < max / 2;
}

/* globalScene.isNewBiome(): a wave that is a multiple of 10, or of 5 with short biomes (endless),
 * or 49 mod 50 in endless or daily. (gameMode.hasRandomBiomes, checked next to it, is never set
 * by any game mode.) */
bool pr_is_new_biome(pr_battle *bt)
{
    int w = bt->wave_index;
    if (w % 10 == 0) return true;
    if (bt->game_mode < 0) {
        if (w % 5 == 0 || w % 50 == 49) pr_note_unported(bt, PR_FEAT_WAVE);
        return false;
    }
    bool short_biomes = bt->game_mode == GAME_MODES_ENDLESS || bt->game_mode == GAME_MODES_SPLICED_ENDLESS;
    bool daily = bt->game_mode == GAME_MODES_DAILY;
    return (short_biomes && w % 5 == 0) || ((short_biomes || daily) && w % 50 == 49);
}

static int field_index(const pr_battle *bt, const pr_pokemon *p)
{
    int b = pr_battler_index(bt, p);
    return b < 0 ? -1 : b % 2;
}

/* queueDeferred("SwitchPhase", switchType, fieldIndex, isModal, doReturn) */
static void defer_switch_phase(pr_battle *bt, pr_engine *e, int switch_type, int field, bool modal, bool do_return)
{
    int ph = pr_phase_new(e, PH_SWITCH, -1);
    if (ph < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); e->overflow = true; return; }
    pr_phase *s = &e->pool[ph];
    s->sw_type = (int8_t)switch_type;
    s->sw_field_index = (int8_t)field;
    s->sw_modal = modal;
    s->sw_do_return = do_return;
    s->sw_player = true;
    pr_phase_add_deferred(bt, e, ph);
}

/* queueDeferred("SwitchSummonPhase", switchType, fieldIndex, slotIndex, doReturn, player): a
 * PartyMemberPokemonPhase, so it has no battler index */
static void defer_switch_summon(pr_battle *bt, pr_engine *e, int switch_type, int field, int slot, bool do_return,
                                bool player)
{
    int ph = pr_phase_new(e, PH_SWITCH_SUMMON, -1);
    if (ph < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); e->overflow = true; return; }
    pr_phase *s = &e->pool[ph];
    s->sw_type = (int8_t)switch_type;
    s->sw_field_index = (int8_t)field;
    s->sw_slot = (int8_t)slot;
    s->sw_do_return = do_return;
    s->sw_player = player;
    pr_phase_add_deferred(bt, e, ph);
}

static void push_boundary(pr_battle *bt, pr_engine *e, pr_phase_type type)
{
    int ph = pr_phase_new(e, type, -1);
    if (ph < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); e->overflow = true; return; }
    pr_phase_push(bt, e, ph);
}

void pr_force_switch_out(pr_battle *bt, pr_engine *e, bool self_switch, int switch_type, pr_pokemon *user,
                         pr_pokemon *target, int move)
{
    if (!pr_switch_out_condition(bt, self_switch, switch_type, user, target, move)) return;
    pr_pokemon *out = self_switch ? user : target;
    /* lapseTag(COMMANDED): Tatsugiri leaves Dondozo's mouth first (reported by the condition) */

    /* Wimp Out or Emergency Exit set off by U-turn, Volt Switch or Flip Turn keeps the user in */
    bool emergency_exit = pr_ability_first_attr(target->ability, PR_AC_PostDamageForceSwitchAbAttr)
                          && (move == MOVE_ID_U_TURN || move == MOVE_ID_VOLT_SWITCH || move == MOVE_ID_FLIP_TURN)
                          && hp_dropped_below_half(target);

    if (out->is_player) {
        if (emergency_exit) return;
        int eligible[PR_MAX_PARTY];
        if (!bt->n_party[0]) { pr_note_unported(bt, PR_FEAT_PARTY); return; }
        int n = eligible_switch_ins(bt, 0, eligible);
        if (n == 0 || out->hp <= 0) return;
        if (switch_type == SWITCH_TYPE_FORCE_SWITCH) {
            /* dragged out: a random eligible party member, on the battle RNG */
            int slot = eligible[pr_battle_rand_seed_int(bt, n, 0)];
            defer_switch_summon(bt, e, switch_type, field_index(bt, out), slot, false, true);
        } else {
            /* the player picks (SwitchPhase); Baton Pass and Shed Tail keep the effects to pass on */
            pr_leave_field(bt, e, out, switch_type == SWITCH_TYPE_SWITCH);
            defer_switch_phase(bt, e, switch_type, field_index(bt, out), true, true);
        }
        return;
    }
    if (bt->battle_type != BATTLE_TYPE_WILD) {
        /* an enemy trainer's Pokemon: another one of theirs comes in, a random one when dragged
         * out, else the trainer's pick; it stays on the field until then */
        if (bt->battle_type == BATTLE_TYPE_MYSTERY_ENCOUNTER) { pr_note_unported(bt, PR_FEAT_TRAINER); return; }
        if (!bt->n_party[1]) { pr_note_unported(bt, PR_FEAT_PARTY); return; }
        int eligible[PR_MAX_PARTY];
        int n = enemy_reserves(bt, out, bt->trainer_partner, eligible);
        if (n == 0 || out->hp <= 0) return;
        int slot = switch_type == SWITCH_TYPE_FORCE_SWITCH ? eligible[pr_battle_rand_seed_int(bt, n, 0)]
                                                           : next_summon_index(bt, out->trainer_slot);
        if (slot == -2) return;
        defer_switch_summon(bt, e, switch_type, field_index(bt, out), slot, false, false);
        return;
    }

    /* a wild Pokemon flees */
    if (emergency_exit) return;
    const pr_pokemon *ally = pr_get_ally(bt, out);
    if (out->hp > 0) {
        pr_leave_field(bt, e, out, false);
        pr_queue_msg(bt, e, pr_msg1("moveTriggers:fled", pr_arg_pokemon(bt, "pokemonName", out)));
        if (bt->double_battle && ally) pr_note_unported(bt, PR_FEAT_DOUBLES);   /* redirectPokemonMoves */
    }
    /* clearEnemyHeldItemModifiers(out), then updateModifiers(false): with none of its held items
     * left to remove, recalculating the enemy party's stats gives the stats they already have */
    for (int i = 0; i < bt->n_mods[1]; i++)
        if (pr_mod_is(&bt->mods[1][i], PR_MOD_HELD_ITEM) && bt->mods[1][i].pokemon_id == (int64_t)out->id)
            pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS);
    if (!is_active_on_field(ally) && out->hp > 0) {
        push_boundary(bt, e, PH_BATTLE_END);
        if (pr_is_new_biome(bt)) push_boundary(bt, e, PH_SELECT_BIOME);
        push_boundary(bt, e, PH_NEW_BATTLE);
    }
}

/* ---------------------------------------------------------------- Wimp Out, Emergency Exit */

/* ForceSwitchOutHelper.getSwitchOutCondition(pokemon, opponent) */
static bool helper_switch_out_condition(pr_battle *bt, const pr_pokemon *p, const pr_pokemon *opponent)
{
    if (p->is_player) {
        /* the opponent's Suction Cups or Guard Dog, applied for real (the ability shows) */
        bool blocked = false;
        pr_ab_params ap = {.pokemon = opponent, .move = -1, .flag = &blocked};
        pr_apply_ab_attrs(bt, PR_AC_ForceSwitchOutImmunityAbAttr, &ap);
        return !blocked;
    }
    /* `!waveIndex && waveIndex % 10 === 0`: only a wave index of 0 */
    if (bt->battle_type == BATTLE_TYPE_WILD && bt->wave_index == 0) return false;
    if (bt->battle_type == BATTLE_TYPE_MYSTERY_ENCOUNTER) { pr_note_unported(bt, PR_FEAT_TRAINER); return true; }
    if (bt->battle_type == BATTLE_TYPE_WILD) return true;
    int res[PR_MAX_PARTY];
    if (!bt->n_party[1]) { pr_note_unported(bt, PR_FEAT_PARTY); return true; }
    return enemy_reserves(bt, p, true, res) > 0;                            /* the trainer's own Pokemon */
}

/* PostDamageForceSwitchAbAttr.canApply({pokemon, source, damage}): 1, 0, or -1 when a held item
 * might be a Shell Bell (the heal it would give is not modelled) */
int pr_wimp_out_can_apply(pr_battle *bt, const pr_pokemon *p, const pr_pokemon *source, double damage,
                          double hp_ratio)
{
    /* not when the Pokemon cut its own HP */
    const pr_turn_move *last = pr_last_move(bt, p, 0);
    if (last && (last->move == MOVE_ID_BELLY_DRUM || last->move == MOVE_ID_SUBSTITUTE || last->move == MOVE_ID_CURSE
                 || last->move == MOVE_ID_PAIN_SPLIT))
        return 0;
    if (source) {
        const pr_turn_move *el = pr_last_move(bt, source, 0);
        if (el) {
            /* Dragon Tail and Circle Throw switch it out first; Sky Drop holding it in the air */
            if (el->move == MOVE_ID_DRAGON_TAIL || el->move == MOVE_ID_CIRCLE_THROW
                || (el->move == MOVE_ID_SKY_DROP && el->result == MOVE_RESULT_OTHER))
                return 0;
            if (pr_moves[el->move].chance >= 0 && pr_has_ability_active(bt, source, ABILITY_ID_SHEER_FORCE)) return 0;
            if (source->hits_left > 1) return 0;                            /* only after the last hit */
            if (source->hit_count > 1) damage = p->damage_taken;
        }
    }
    double max = pr_get_stat(p, STAT_HP, true), limit = max * hp_ratio;
    if (!(p->hp + damage >= limit)) return 0;
    /* calculateShellBellRecovery: getHeldItems().find(m => m instanceof HitHealModifier) */
    int side = p->is_player ? 0 : 1;
    double shell_bell_heal = pr_calculate_shell_bell_recovery(bt, p);
    for (int i = 0; i < bt->n_mods[side]; i++) {
        const pr_modifier *m = &bt->mods[side][i];
        if (m->cls == PR_MOD_TURN_HEAL || m->cls == PR_MOD_HIT_HEAL) continue;
        if (pr_mod_is(m, PR_MOD_HELD_ITEM) && m->pokemon_id == (int64_t)p->id
            && (m->cls == PR_MOD_HELD_ITEM || m->unknown_subclass))
            return -1;
    }
    if (!(p->hp - shell_bell_heal < limit)) return 0;
    /* getOpponents(): the other side's field, isActive(true), in field order */
    for (int k = 0; k < 2; k++) {
        const pr_pokemon *o = bt->field[(p->is_player ? 2 : 0) + k];
        if (!is_active_on_field(o)) continue;
        if (!helper_switch_out_condition(bt, p, o)) return 0;
    }
    return 1;
}

/* ForceSwitchOutHelper(SWITCH).switchOutLogic(pokemon) */
void pr_wimp_out_apply(pr_battle *bt, pr_engine *e, pr_pokemon *p)
{
    if (p->is_player) {
        int eligible[PR_MAX_PARTY];
        if (!bt->n_party[0]) { pr_note_unported(bt, PR_FEAT_PARTY); return; }
        if (eligible_switch_ins(bt, 0, eligible) == 0) return;
        /* the party screen, deferred; the Pokemon stays on the field until the switch */
        if (p->hp > 0) defer_switch_phase(bt, e, SWITCH_TYPE_SWITCH, field_index(bt, p), true, true);
        return;
    }
    if (bt->battle_type != BATTLE_TYPE_WILD) {
        /* an enemy trainer's Pokemon: the trainer's pick comes in (any of its party's reserves
         * will do for the check, whoever's they are) */
        if (bt->battle_type == BATTLE_TYPE_MYSTERY_ENCOUNTER) { pr_note_unported(bt, PR_FEAT_TRAINER); return; }
        int res[PR_MAX_PARTY];
        if (!bt->n_party[1]) { pr_note_unported(bt, PR_FEAT_PARTY); return; }
        if (enemy_reserves(bt, p, false, res) == 0) return;
        if (p->hp > 0) {
            int slot = next_summon_index(bt, p->trainer_slot);
            if (slot != -2) defer_switch_summon(bt, e, SWITCH_TYPE_SWITCH, field_index(bt, p), slot, false, false);
        }
        return;
    }
    const pr_pokemon *ally = pr_get_ally(bt, p);
    if (!bt->wave_index || bt->wave_index % 10 == 0) return;
    if (p->hp > 0) {
        pr_leave_field(bt, e, p, false);
        pr_queue_msg(bt, e, pr_msg1("moveTriggers:fled", pr_arg_pokemon(bt, "pokemonName", p)));
        if (bt->double_battle && ally) pr_note_unported(bt, PR_FEAT_DOUBLES);   /* redirectPokemonMoves */
    }
    if (!is_active_on_field(ally)) {
        /* clearEnemyHeldItemModifiers(): every enemy held item, then updateModifiers(false) */
        for (int i = 0; i < bt->n_mods[1]; i++)
            if (pr_mod_is(&bt->mods[1][i], PR_MOD_HELD_ITEM)) pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS);
        if (p->hp) {
            push_boundary(bt, e, PH_BATTLE_END);
            if (pr_is_new_biome(bt)) push_boundary(bt, e, PH_SELECT_BIOME);
            push_boundary(bt, e, PH_NEW_BATTLE);
        }
    }
}

/* ---------------------------------------------------------------- the switch itself */

/* SwitchPhase.start: a forced (modal) switch is skipped when every Pokemon allowed in battle is on
 * the field already, or when the fainted Pokemon it replaces has been revived; otherwise the party
 * screen opens and the turn waits for the player's pick (pr_resume_switch) */
void pr_switch_phase(pr_battle *bt, pr_engine *e, int ph)
{
    const pr_phase *s = &e->pool[ph];
    /* challenges (isAllowedInChallenge) aren't modelled: every Pokemon that hasn't fainted is allowed */
    if (!bt->n_party[0]) { pr_note_unported(bt, PR_FEAT_PARTY); return; }
    if (bt->double_battle) pr_note_unported(bt, PR_FEAT_DOUBLES);   /* the field check, fieldIndex 0 */
    if (s->sw_modal) {
        bool all_on_field = true;                              /* getPokemonAllowedInBattle().every(isOnField) */
        for (int k = 0; k < bt->n_party[0]; k++) {
            const pr_pokemon *p = bt->party[0][k];
            if (p->hp > 0 && !(p->on_field && !p->switch_out_status)) all_on_field = false;
        }
        if (all_on_field) return;
        if (!s->sw_do_return) {
            if (s->sw_field_index < 0 || s->sw_field_index >= bt->n_party[0]) {
                pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT);   /* party[fieldIndex] undefined: throws */
                return;
            }
            if (bt->party[0][s->sw_field_index]->hp > 0) return;
        }
    }
    e->awaiting_pick = true;
}


static double to_dmg_value(double v) { double f = floor(v); return f < 1 ? 1 : f; }

/* Pokemon.fieldSetup(resetSummonData) */
static void field_setup(pr_battle *bt, pr_engine *e, pr_pokemon *p, bool reset)
{
    p->switch_out_status = false;
    pr_form_change(bt, p, PR_FCT_SpeciesFormChangePostMoveTrigger);
    for (int i = 0; i < p->n_tags; i++)
        if (p->tags[i].type == BATTLER_TAG_TYPE_SUBSTITUTE) {
            pr_unshift_pokemon_anim(bt, e, pr_battler_index(bt, p), POKEMON_ANIM_TYPE_SUBSTITUTE_ADD);
            p->tags[i].in_focus = false;
            break;
        }
    /* Commander hiding the Pokemon behind an allied Dondozo: doubles only */
    if (reset) pr_reset_summon_data(p);
}

/* currentBattle.trainer?.getName(slot): the name the game gave, or "" (i18next's undefined) */
static pr_msg_arg trainer_name(const pr_battle *bt, int trainer_slot)
{
    const char *n = bt->trainer_names[trainer_slot == TRAINER_SLOT_TRAINER_PARTNER ? 1 : 0];
    return (pr_msg_arg){"trainerName", PR_ARG_TEMPLATE, 0, n ? n : ""};   /* getName() may nest keys */
}

/* SwitchSummonPhase: the party member in slotIndex comes in to the field slot (fieldIndex) and the
 * Pokemon there goes to slotIndex. Upstream runs it as SummonPhase.start, preSummon (withdrawing
 * the Pokemon first when doReturn), switchAndSummon, summon and, once the animations are over,
 * onEnd. An enemy trainer's switch with slotIndex -1 (the one FaintPhase queues) asks the trainer
 * for its pick (bt->next_summon_index). */
void pr_switch_summon_phase(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *s = &e->pool[ph];
    int side = s->sw_player ? 0 : 1, field = s->sw_field_index, slot = s->sw_slot, type = s->sw_type;
    bool do_return = s->sw_do_return, player = s->sw_player;
    if (!bt->n_party[side]) { pr_note_unported(bt, PR_FEAT_PARTY); return; }
    if (bt->double_battle) pr_note_unported(bt, PR_FEAT_DOUBLES);
    if (field < 0 || field >= bt->n_party[side] || slot < -1 || slot >= PR_MAX_PARTY) {
        pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT);                       /* upstream would throw */
        return;
    }
    pr_pokemon *last = bt->party[side][field];                             /* getPokemon() */
    int tslot = field % 2 ? TRAINER_SLOT_TRAINER_PARTNER : TRAINER_SLOT_TRAINER;

    /* SummonPhase.start: PreSummonAbAttr for getPokemon(), the Pokemon still in the field slot */
    pr_hook_ab(bt, last, PR_AC_PreSummonAbAttr);

    /* preSummon: an enemy trainer picks the Pokemon to send in when none was given, then slides
     * in with its ball tray */
    if (!player) {
        if (bt->battle_type == BATTLE_TYPE_MYSTERY_ENCOUNTER) { pr_note_unported(bt, PR_FEAT_TRAINER); return; }
        if (slot == -1) {
            if (bt->battle_type != BATTLE_TYPE_TRAINER) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return; }
            slot = next_summon_index(bt, field ? TRAINER_SLOT_TRAINER_PARTNER : TRAINER_SLOT_TRAINER);
            if (slot == -2) return;
            if (slot < -1 || slot >= PR_MAX_PARTY) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return; }
            s->sw_slot = (int8_t)slot;
        }
        if (slot > -1) pr_event_add(bt, e, PR_EV_TRAINER_SHOW, -1, -1, (uint16_t)tslot, 0, NULL);
    }
    /* with doReturn (and a Pokemon in slotIndex), the Pokemon is withdrawn first: the tags it gave
     * the other side go, then leaveField(switchType === SWITCH, false) in the tween's callback;
     * switchAndSummon comes after, on a timer */
    if (do_return && slot >= 0 && slot < bt->n_party[side]) {
        pr_speed_iter it;
        pr_pokemon *q;
        pr_speed_iter_init(bt, &it, side == 0 ? ARENA_TAG_SIDE_ENEMY : ARENA_TAG_SIDE_PLAYER);
        while ((q = pr_speed_iter_next(bt, &it))) pr_remove_tags_by_source_id(bt, e, q, (int64_t)last->id);
        if (player) pr_show_text(bt, e, pr_msg1("battle:playerComeBack", pr_arg_pokemon(bt, "pokemonName", last)));
        else pr_show_text(bt, e, pr_msg2("battle:trainerComeBack", trainer_name(bt, tslot),
                                         pr_arg_pokemon_name(bt, "pokemonName", last)));   /* getNameToRender() */
        pr_event_add(bt, e, PR_EV_SWITCH_OUT, side * 2 + field, -1, 0, 0, NULL);
        pr_leave_field(bt, e, last, type == SWITCH_TYPE_SWITCH);
    }

    /* switchAndSummon */
    if (slot < 0 || slot >= bt->n_party[side]) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return; }   /* party[slot] undefined: throws */
    pr_pokemon *in = bt->party[side][slot];
    pr_reset_summon_data(in);
    if (last->on_field && !last->switch_out_status)                        /* isOnField() */
        pr_leave_field(bt, e, last, type == SWITCH_TYPE_SWITCH);
    pr_hook_ab(bt, in, PR_AC_PreSummonAbAttr);
    pr_ab_params pp = {.pokemon = last, .move = -1};
    pr_apply_ab_attrs(bt, PR_AC_PreSwitchOutAbAttr, &pp);
    if (type == SWITCH_TYPE_BATON_PASS) {
        /* the other side's tags from the Pokemon leaving now come from the one coming in, on
         * getEnemyField() / getPlayerField(): the first one or two of that party, on the field
         * or not. The Baton held item (SwitchEffectTransferModifier) moving over is a modifier,
         * which summon() below reports. */
        int other = 1 - side, n = bt->double_battle ? 2 : 1;
        for (int k = 0; k < n; k++) {
            pr_pokemon *q = bt->n_party[other] ? (k < bt->n_party[other] ? bt->party[other][k] : NULL)
                                               : bt->field[other * 2 + k];
            if (q) pr_transfer_tags_by_source_id(q, (int64_t)last->id, (int64_t)in->id);
        }
    }
    bt->party[side][slot] = last;
    bt->party[side][field] = in;
    bt->field[side * 2 + field] = in;
    /* showTextAndSummon (for an enemy on a timer, once the trainer and its ball tray are hidden):
     * the send-out text (getSendOutText; not a phase); Baton Pass and Shed Tail only move the
     * sprite of a passed Substitute, the others set the Pokemon up and reset its summon data */
    if (!player) pr_event_add(bt, e, PR_EV_TRAINER_HIDE, -1, -1, (uint16_t)tslot, 0, NULL);
    if (type == SWITCH_TYPE_FORCE_SWITCH || player)
        pr_show_text(bt, e, pr_msg1(type == SWITCH_TYPE_FORCE_SWITCH ? "battle:pokemonDraggedOut" : "battle:playerGo",
                                    pr_arg_pokemon(bt, "pokemonName", in)));
    else pr_show_text(bt, e, pr_msg2("battle:trainerGo", trainer_name(bt, tslot),
                                     pr_arg_pokemon_name(bt, "pokemonName", in)));   /* getPokemon().getNameToRender() */
    if (type != SWITCH_TYPE_BATON_PASS && type != SWITCH_TYPE_SHED_TAIL) field_setup(bt, e, in, true);

    /* summon(): the Pokemon joins the field, updateModifiers(player) recalculates the party's stats,
     * and fieldSetup() again */
    in->on_field = true;
    if (bt->n_mods[side]) pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS);
    for (int k = 0; k < bt->n_party[side]; k++) pr_recalculate_stats(bt, bt->party[side][k]);
    field_setup(bt, e, in, false);
    pr_event_add(bt, e, PR_EV_SWITCH_IN, side * 2 + field, -1, 0, field, NULL);

    /* SummonPhase.onEnd */
    int b = side * 2 + field;
    if (in->shiny) pr_unshift_new(bt, e, PH_SHINY_SPARKLE, b);
    pr_reset_turn_data(in);
    pr_form_change(bt, in, PR_FCT_SpeciesFormChangeActiveTrigger);
    {
        int q = pr_phase_new(e, PH_POST_SUMMON, (int8_t)b);                /* SwitchSummonPhase.queuePostSummon */
        pr_phase_unshift(bt, e, q);
    }
    /* SwitchSummonPhase.onEnd: the turn the switch took doesn't count for the new Pokemon when the
     * player chose it, or it was forced by a switching move or Wimp Out and co. */
    int last_move = bt->last_move;
    bool forced = last_move && (pr_move_has_attr((uint16_t)last_move, PR_AC_ForceSwitchOutAttr)
                                || pr_move_has_attr((uint16_t)last_move, PR_AC_PartingShotAttr)) && last->hp > 0;
    bool emergency = pr_has_ability_with_attr(bt, last, PR_AC_PostDamageForceSwitchAbAttr) && last->hp > 0;
    if (bt->commands[field].command == COMMAND_POKEMON || forced || emergency) {
        in->temp_turn_count--;
        in->temp_wave_turn_count--;
    }
    if (type == SWITCH_TYPE_BATON_PASS) pr_transfer_summon(bt, in, last);
    else if (type == SWITCH_TYPE_SHED_TAIL) pr_pass_substitute(bt, in, last);
    if (type != SWITCH_TYPE_INITIAL_SWITCH) {
        pr_reset_turn_data(in);
        in->switched_in_this_turn = true;
    }
    pr_reset_summon_data(last);
    pr_form_change(bt, in, PR_FCT_SpeciesFormChangeActiveTrigger);
    /* arena.triggerWeatherBasedFormChanges(pokemon) */
    pr_form_change(bt, in, PR_FCT_SpeciesFormChangeWeatherTrigger);
}

/* getAffectedPokemon().includes(pokemon) for an arena tag on `side` */
static bool tag_affects(const pr_battle *bt, int side, const pr_pokemon *p)
{
    for (int i = 0; i < 4; i++) {
        if (bt->field[i] != p) continue;
        if (side == ARENA_TAG_SIDE_BOTH) return p->hp > 0;              /* getField(true) */
        return (side == ARENA_TAG_SIDE_PLAYER) == (i < 2);
    }
    return false;
}

/* EntryHazardTag.apply(false, pokemon): Spikes, Stealth Rock, Toxic Spikes, Sticky Web */
static void entry_hazard_apply(pr_battle *bt, pr_engine *e, int i, pr_pokemon *p)
{
    pr_arena_tag t = bt->arena_tags[i];
    if (!tag_affects(bt, t.side, p)) return;
    if (t.type != ARENA_TAG_TYPE_STEALTH_ROCK && !pr_is_grounded(bt, p, false)) return;   /* groundedOnly */
    switch (t.type) {
    case ARENA_TAG_TYPE_SPIKES:
    case ARENA_TAG_TYPE_STEALTH_ROCK: {                                    /* DamagingTrapTag.activateTrap */
        bool cancelled = false;
        pr_ab_params pp = {.pokemon = p, .move = -1, .flag = &cancelled};
        pr_apply_ab_attrs(bt, PR_AC_BlockNonDirectDamageAbAttr, &pp);
        if (cancelled) return;
        double ratio = t.type == ARENA_TAG_TYPE_SPIKES
                           ? 1.0 / (10 - 2 * t.layers)
                           : 0.125 * pr_get_attack_type_effectiveness_opts(bt, p, POKEMON_TYPE_ROCK, NULL, -1, true, true);
        double damage = to_dmg_value(pr_get_stat(p, STAT_HP, true) * ratio);
        pr_queue_msg(bt, e, pr_msg_p(bt, t.type == ARENA_TAG_TYPE_SPIKES ? "arenaTag:spikesActivateTrap"
                                                                         : "arenaTag:stealthRockActivateTrap", p));
        pr_damage_and_update(bt, e, p, damage, HIT_RESULT_INDIRECT, false, NULL);
        p->damage_taken += (int32_t)damage;
        return;
    }
    case ARENA_TAG_TYPE_TOXIC_SPIKES:
        if (pr_is_of_type(p, POKEMON_TYPE_POISON)) {
            pr_arena_remove_tag_on_side(bt, e, t.type, t.side, false);
            return;
        }
        int st = t.layers == 1 ? STATUS_EFFECT_POISON : STATUS_EFFECT_TOXIC;
        /* sourceText: the tag's move name */
        pr_msg m = pr_msg2(pr_status_key(st, PR_ST_OBTAIN_SOURCE), pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                           pr_arg_move("sourceText", MOVE_ID_TOXIC_SPIKES));
        pr_try_set_status_msg(bt, e, p, st, NULL, 0, false, true, &m);
        return;
    case ARENA_TAG_TYPE_STICKY_WEB: {
        static const int8_t spd[1] = {STAT_SPD}, minus_one[1] = {-1};
        uint16_t cancelled = 0;
        pr_ab_params pp = {.pokemon = p, .move = -1, .ch_stat = spd, .ch_stages = minus_one, .n_ch = 1,
                           .cancelled_stats = &cancelled};
        pr_apply_ab_attrs(bt, PR_AC_ProtectStatAbAttr, &pp);
        if (cancelled) return;
        pr_queue_msg(bt, e, pr_msg_p(bt, "arenaTag:stickyWebActivateTrap", p));
        if (pr_unshift_stat_stage_change_ex(bt, e, pr_battler_index(bt, p), spd, minus_one, 1,
                                            pr_pokemon_by_id(bt, t.source_id), STAT_CHANGE_SOURCE_STICKY_WEB) < 0)
            e->overflow = true;
        return;
    }
    default:                                                               /* Imprison */
        pr_note_unported(bt, PR_FEAT_ARENA_TAG_TURN);
        return;
    }
}

/* PostSummonPhase: a Toxic count starts over, a pending heal (Healing Wish, Lunar Dance) and the
 * entry hazards act; PostSummonActivateAbilityPhase applies the PostSummon abilities, the ability
 * or the passive */
void pr_post_summon_phase(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *q = &e->pool[ph];
    pr_pokemon *p = q->battler >= 0 && q->battler < 4 ? bt->field[q->battler] : NULL;
    if (!p) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return; }
    if (q->ps_activate) {
        pr_ab_params pp = {.pokemon = p, .move = -1, .only = q->ps_passive ? 2 : 1};
        pr_apply_ab_attrs(bt, PR_AC_PostSummonAbAttr, &pp);
        return;
    }
    if (p->status == STATUS_EFFECT_TOXIC) p->status_toxic_turns = 0;
    if (pr_arena_get_tag_on_side(bt, ARENA_TAG_TYPE_PENDING_HEAL, ARENA_TAG_SIDE_BOTH))
        pr_note_unported(bt, PR_FEAT_ARENA_TAG_TURN);
    /* for (const tag of arena.tags): a tag removed on the way (Toxic Spikes absorbed) makes the
     * loop skip the one after it, as upstream's does */
    for (int i = 0; i < bt->n_arena_tags; i++)
        if (pr_arena_tag_is(bt->arena_tags[i].type, PR_ARENACLASS_ENTRY_HAZARD)) entry_hazard_apply(bt, e, i, p);
    /* a mystery encounter's post-summon tag, and the allies' Commander: not in a wild single battle */
    if (bt->battle_type == BATTLE_TYPE_MYSTERY_ENCOUNTER) pr_note_unported(bt, PR_FEAT_TRAINER);
}

/* SummonPhase(fieldIndex, player) as EncounterPhase queues it (loaded false): the party member in
 * slot fieldIndex comes out. start: PreSummonAbAttr; preSummon: a fainted one swaps with the next
 * Pokemon allowed in battle, then the send-out text; summon: it joins the field; onEnd. */
void pr_summon_phase(pr_battle *bt, pr_engine *e, int ph)
{
    pr_phase *s = &e->pool[ph];
    int side = s->sw_player ? 0 : 1, field = s->sw_field_index;
    bool player = s->sw_player;
    if (bt->double_battle) pr_note_unported(bt, PR_FEAT_DOUBLES);
    if (!bt->n_party[side]) { pr_note_unported(bt, PR_FEAT_PARTY); return; }
    if (field < 0 || field >= bt->n_party[side]) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return; }
    pr_pokemon *p = bt->party[side][field];
    pr_hook_ab(bt, p, PR_AC_PreSummonAbAttr);

    /* preSummon: a Pokemon not allowed in battle is swapped with the first allowed one after it */
    if (p->hp <= 0) {
        if (p->on_field && !p->switch_out_status) pr_leave_field(bt, e, p, true);
        int legal = -1;
        for (int k = field + 1; k < bt->n_party[side] && legal < 0; k++)
            if (bt->party[side][k]->hp > 0) legal = k;
        if (legal < 0) { pr_note_unported(bt, PR_FEAT_PARTY); return; }   /* clearPhaseQueue + GameOverPhase */
        bt->party[side][field] = bt->party[side][legal];
        bt->party[side][legal] = p;
        p = bt->party[side][field];
    }
    int tslot = field % 2 ? TRAINER_SLOT_TRAINER_PARTNER : TRAINER_SLOT_TRAINER;
    if (player) {
        pr_show_text(bt, e, pr_msg1("battle:playerGo", pr_arg_pokemon(bt, "pokemonName", p)));
    } else if (bt->battle_type == BATTLE_TYPE_TRAINER) {
        pr_event_add(bt, e, PR_EV_TRAINER_HIDE, -1, -1, (uint16_t)tslot, 0, NULL);
        pr_show_text(bt, e, pr_msg2("battle:trainerSendOut", trainer_name(bt, tslot),
                                    pr_arg_pokemon_name(bt, "pokemonName", p)));   /* getNameToRender() */
    } else {
        pr_note_unported(bt, PR_FEAT_TRAINER);   /* a mystery encounter's summonWild; a wild one never gets here */
        return;
    }

    /* summon(): the Pokemon joins the field, updateModifiers(player) recalculates the party's stats,
     * then fieldSetup() */
    int b = side * 2 + field;
    bt->field[b] = p;
    p->on_field = true;
    p->switch_out_status = false;
    if (bt->n_mods[side]) pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS);
    for (int k = 0; k < bt->n_party[side]; k++) pr_recalculate_stats(bt, bt->party[side][k]);
    field_setup(bt, e, p, false);
    pr_event_add(bt, e, PR_EV_SWITCH_IN, b, -1, 0, field, NULL);

    /* onEnd */
    if (p->shiny) pr_unshift_new(bt, e, PH_SHINY_SPARKLE, b);
    pr_reset_turn_data(p);
    pr_form_change(bt, p, PR_FCT_SpeciesFormChangeActiveTrigger);
    p->summoned_this_turn = true;                                           /* queuePostSummon */
}

/* InitEncounterPhase: a PostSummonPhase for every Pokemon on the field that is an enemy or was
 * summoned this turn, in field order */
void pr_init_encounter_phase(pr_battle *bt, pr_engine *e, int ph)
{
    (void)ph;
    for (int b = 0; b < 4; b++) {
        pr_pokemon *p = bt->field[b];
        if (!p || p->hp <= 0 || !p->on_field || p->switch_out_status) continue;   /* getField(true) */
        if (b >= 2 || p->summoned_this_turn) pr_phase_unshift(bt, e, pr_phase_new(e, PH_POST_SUMMON, (int8_t)b));
    }
}

/* CheckSwitchPhase: "Will you switch?" at a wild battle's start, unless the battle style is Set, no
 * other Pokemon could come in, or the Pokemon is locked in (Outrage), trapped or commanded */
void pr_check_switch_phase(pr_battle *bt, pr_engine *e, int ph)
{
    int field = e->pool[ph].sw_field_index;
    if (bt->battle_style_set) return;
    pr_pokemon *p = bt->field[field];
    if (!p || !p->on_field) { pr_note_unported(bt, PR_FEAT_PARTY); return; }   /* SummonMissingPhase */
    bool others = false;
    for (int k = 1; k < bt->n_party[0]; k++) others |= bt->party[0][k]->hp > 0;
    if (!others) return;
    if (pr_get_battler_tag(p, BATTLER_TAG_TYPE_FRENZY) || pr_is_trapped(bt, p, NULL)
        || pr_get_battler_tag(p, BATTLER_TAG_TYPE_COMMANDED))
        return;
    /* useName: currentBattle.double */
    pr_show_text(bt, e, pr_msg1("battle:switchQuestion",
                                bt->double_battle ? pr_arg_pokemon(bt, "pokemonName", p)
                                                  : pr_arg_key("pokemonName", "battle:pokemon")));
    e->awaiting_confirm = true;
}
