/* Stand-ins for the workstreams that haven't landed (gc/ports.h). Each one keeps a run playable and
 * counts itself in run->stubs; none of them rolls on Phaser.Math.RND (they use gc_stub_rand), so the
 * run's own rolls stay where upstream has them, minus what the stand-in itself would have rolled. */
#include "internal.h"

/* ---- run-core: the last four level moves the enemy could know at its level ---- */
void gc_port_enemy_moveset(gc_run *run, gc_mon *m)
{
    gc_stub(run, GC_STUB_ENEMY_MOVESET);
    const pr_species *sp = gc_species(m->b.species);
    uint16_t mv[4];
    int n = 0;
    if (sp)
        for (int i = sp->level_moves.n - 1; i >= 0 && n < 4; i--) {
            const pr_level_move *lm = &sp->level_moves.v[i];
            if (lm->level > m->b.level || pr_moves[lm->move].unimplemented) continue;
            bool dup = false;
            for (int k = 0; k < n; k++) dup |= mv[k] == lm->move;
            if (!dup) mv[n++] = lm->move;
        }
    if (!n) mv[n++] = MOVE_ID_TACKLE;
    m->b.n_moves = 0;
    for (int k = 0; k < n; k++) gc_mon_set_move(m, k, mv[n - 1 - k]);
}

/* ---- reward-core: what a reward does when it is taken. The options themselves are reward-core's
 * (gc_reward.c), and so is the application of healing, revives, PP restores and PP Ups (gc_reward_apply,
 * Task 60). What is left here is the rest: Poke Balls, Full Heal (reward-core has no application for it),
 * Rare Candy (without the Candy Jar) and the items that are added to the player's items as they are, each
 * one that needs a real effect counted ---- */
static int pokeball_type(const char *key)
{
    if (!strcmp(key, "POKEBALL")) return POKEBALL_TYPE_POKEBALL;
    if (!strcmp(key, "GREAT_BALL")) return POKEBALL_TYPE_GREAT_BALL;
    if (!strcmp(key, "ULTRA_BALL")) return POKEBALL_TYPE_ULTRA_BALL;
    if (!strcmp(key, "ROGUE_BALL")) return POKEBALL_TYPE_ROGUE_BALL;
    if (!strcmp(key, "MASTER_BALL")) return POKEBALL_TYPE_MASTER_BALL;
    return -1;
}

bool gc_port_apply_reward(gc_run *run, const gc_offer *o, int party, int move_slot, const char **why)
{
    const char *k = o->key;
    switch (gc_reward_apply(run, o, party, move_slot, why)) {
    case GC_APPLY_DONE: return true;
    case GC_APPLY_REFUSED: return false;
    default: break;
    }
    const int ball = pokeball_type(k);
    if (ball >= 0) {
        const int add = ball == POKEBALL_TYPE_MASTER_BALL ? 1 : 5;
        run->pokeballs[ball] = (uint8_t)(run->pokeballs[ball] + add > 99 ? 99 : run->pokeballs[ball] + add);
        /* upstream's name is "5× Poké Ball" (AddPokeballModifierType.name with its count): the stand-in
         * names the ball alone */
        static const char *const ball_key[] = {"pokeball:pokeBall", "pokeball:greatBall", "pokeball:ultraBall",
                                               "pokeball:rogueBall", "pokeball:masterBall"};
        gc_ev_msg(run, "battle:rewardGain", 1, pr_arg_key("modifierName", ball_key[ball]));
        return true;
    }
    const int idx = gc_reward_item_index(k);
    if (o->target == GC_TARGET_NONE) {
        if (idx < 0 || !(pr_items[idx].flags & PR_ITEMF_PERSISTENT)) gc_stub(run, GC_STUB_REWARD_APPLY);
        gc_item_add(run, k, o->arg, 0, 1);
        return true;
    }
    if (party < 0 || party >= run->n_party) { *why = "pick a Pokemon"; return false; }
    gc_mon *m = &run->party[party];
    pr_pokemon *b = &m->b;
    bool ok = true;
    if (!strcmp(k, "FULL_HEAL")) {
        ok = b->hp > 0 && b->status != STATUS_EFFECT_NONE;
        if (ok) { b->status = STATUS_EFFECT_NONE; b->status_toxic_turns = 0; b->status_sleep_turns = b->status_freeze_turns = -1; }
    } else if (!strcmp(k, "RARE_CANDY")) {
        if (b->level >= pr_run_get_max_exp_level(&run->run, run->wave, false)) { *why = "partyUiHandler:anyEffect"; return false; }
        const pr_species *sp = gc_species(b->species);
        b->level++;
        if (b->level <= pr_run_get_max_exp_level(&run->run, run->wave, true)) m->exp = gc_level_total_exp(b->level, sp ? sp->growth_rate : 0);
        /* FRIENDSHIP_GAIN_FROM_RARE_CANDY (6), capped at RARE_CANDY_FRIENDSHIP_CAP (200) */
        gc_system_add_friendship(run->sys, run, m, 6, true);
        gc_rq_unshift(run, gc_rp(RP_LEVEL_UP, party, b->level - 1, b->level));
    } else {
        /* a held item is added as it is; anything else would act on the Pokemon, and doesn't yet */
        if (idx < 0 || !(pr_items[idx].flags & PR_ITEMF_HELD)) gc_stub(run, GC_STUB_REWARD_APPLY);
        gc_item_add(run, k, o->arg, b->id, 1);
    }
    if (!ok) { *why = "partyUiHandler:anyEffect"; return false; }
    gc_event *ev = gc_ev_new(run, GC_EV_HP);
    if (ev) { ev->a = 0; ev->b = party; ev->c = b->hp; }
    return true;
}

void gc_port_give_reward(gc_run *run, const char *key)
{
    const int ball = pokeball_type(key);
    if (ball >= 0) { run->pokeballs[ball] = (uint8_t)(run->pokeballs[ball] + 5); return; }
    /* vouchers are system data (the gacha): nothing in the run */
    if (!strncmp(key, "VOUCHER", 7)) return;
    gc_item_add(run, key, -1, 0, 1);
}
