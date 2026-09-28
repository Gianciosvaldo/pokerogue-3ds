/* A Pokemon switched out by a move during a turn (src/data/moves/move.ts: ForceSwitchOutAttr, which
 * ChillyReceptionAttr and PartingShotAttr use too): the checks, the Pokemon leaving the field and
 * the phase that brings in its replacement. That phase (SwitchPhase's party prompt, or
 * SwitchSummonPhase) is where a turn hands control back to the caller; a wild Pokemon that flees
 * ends the battle (BattleEndPhase). Only wild single battles are modelled: a trainer's or a
 * mystery encounter's own rules report themselves unported (PR_FEAT_TRAINER). */
#include "pr/engine.h"
#include <string.h>

/* isActive(true): allowed in battle (not fainted; challenges are not modelled) and on the field */
static bool is_active_on_field(const pr_pokemon *p)
{
    return p && p->hp > 0 && p->on_field && !p->switch_out_status;
}

/* the party entry's HP and whether it is on the field, reading the Pokemon itself when it is */
static const pr_pokemon *party_mon(const pr_battle *bt, int side, int k)
{
    for (int i = 0; i < 4; i++)
        if (bt->field[i] && bt->field[i]->id == bt->party[side][k].id) return bt->field[i];
    return NULL;
}

/* party.filter(p => p.isAllowedInBattle() && !p.isOnField()): the indexes, in party order */
static int eligible_switch_ins(const pr_battle *bt, int side, int out[PR_MAX_PARTY])
{
    int n = 0;
    for (int k = 0; k < bt->n_party[side]; k++) {
        const pr_pokemon *f = party_mon(bt, side, k);
        int32_t hp = f ? f->hp : bt->party[side][k].hp;
        bool on_field = f && f->on_field && !f->switch_out_status;
        if (hp > 0 && !on_field) out[n++] = k;
    }
    return n;
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
        /* Roar and Whirlwind against a trainer with one Pokemon left */
        if (has_force_switch_attr(move) && bt->battle_type == BATTLE_TYPE_TRAINER) pr_note_unported(bt, PR_FEAT_TRAINER);
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
    if (!player) { pr_note_unported(bt, PR_FEAT_TRAINER); return true; }   /* the trainer's own slot */
    int side = player ? 0 : 1, eligible[PR_MAX_PARTY];
    if (!bt->n_party[side]) { pr_note_unported(bt, PR_FEAT_PARTY); return true; }
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
static bool is_new_biome(pr_battle *bt)
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
    if (bt->battle_type != BATTLE_TYPE_WILD) { pr_note_unported(bt, PR_FEAT_TRAINER); return; }

    /* a wild Pokemon flees */
    if (emergency_exit) return;
    const pr_pokemon *ally = pr_get_ally(bt, out);
    if (out->hp > 0) {
        pr_leave_field(bt, e, out, false);
        pr_queue_message(bt, e);                                     /* moveTriggers:fled */
        if (bt->double_battle && ally) pr_note_unported(bt, PR_FEAT_DOUBLES);   /* redirectPokemonMoves */
    }
    /* clearEnemyHeldItemModifiers(out), then updateModifiers(false): with none of its held items
     * left to remove, recalculating the enemy party's stats gives the stats they already have */
    for (int i = 0; i < bt->n_mods[1]; i++)
        if (pr_mod_is(&bt->mods[1][i], PR_MOD_HELD_ITEM) && bt->mods[1][i].pokemon_id == (int64_t)out->id)
            pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS);
    if (!is_active_on_field(ally) && out->hp > 0) {
        push_boundary(bt, e, PH_BATTLE_END);
        if (is_new_biome(bt)) push_boundary(bt, e, PH_SELECT_BIOME);
        push_boundary(bt, e, PH_NEW_BATTLE);
    }
}
