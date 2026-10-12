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
 * (gc_reward.c), and so is the application of Poke Balls, Full Heal, healing, revives, PP restores and PP Ups
 * (gc_reward_apply). What is left here is the rest: the items that are added to the player's items as they are,
 * each one that needs a real effect counted ---- */
bool gc_port_apply_reward(gc_run *run, const gc_offer *o, int party, int move_slot, const char **why)
{
    const char *k = o->key;
    switch (gc_reward_apply(run, o, party, move_slot, why)) {
    case GC_APPLY_DONE: return true;
    case GC_APPLY_REFUSED: return false;
    default: break;
    }
    const int idx = gc_reward_item_index(k);
    if (o->target == GC_TARGET_NONE) {
        if (idx < 0 || !(pr_items[idx].flags & PR_ITEMF_PERSISTENT)) gc_stub(run, GC_STUB_REWARD_APPLY);
        gc_item_add(run, k, o->arg, 0, 1);
        return true;
    }
    if (party < 0 || party >= run->n_party) { *why = "pick a Pokemon"; return false; }
    const pr_pokemon *b = &run->party[party].b;
    /* a held item is added as it is; anything else would act on the Pokemon, and doesn't yet */
    if (idx < 0 || !(pr_items[idx].flags & PR_ITEMF_HELD) || idx == PR_ITEM_BASE_STAT_BOOSTER || idx == PR_ITEM_WIDE_LENS || idx == PR_ITEM_MULTI_LENS)
        gc_stub(run, GC_STUB_REWARD_APPLY); /* unsupported bounded persistent adapter path */
    gc_item_add(run, k, o->arg, b->id, 1);
    gc_event *ev = gc_ev_new(run, GC_EV_HP);
    if (ev) { ev->a = 0; ev->b = party; ev->c = b->hp; }
    return true;
}

void gc_port_give_reward(gc_run *run, const char *key)
{
    if (gc_reward_add_pokeballs(run, key)) return;
    /* vouchers are system data (the gacha), not run items: AddVoucherModifier.apply adds one to the count of its
     * VoucherType (sys NULL = no system updates) */
    if (!strcmp(key, "VOUCHER")) { gc_system_add_voucher_count(run->sys, GC_VOUCHER_REGULAR, 1); return; }
    if (!strcmp(key, "VOUCHER_PLUS")) { gc_system_add_voucher_count(run->sys, GC_VOUCHER_PLUS, 1); return; }
    if (!strcmp(key, "VOUCHER_PREMIUM")) { gc_system_add_voucher_count(run->sys, GC_VOUCHER_PREMIUM, 1); return; }
    gc_item_add(run, key, -1, 0, 1);
}
