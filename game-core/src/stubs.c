/* Stand-ins for the workstreams that haven't landed (gc/ports.h). Each one keeps a run playable and
 * counts itself in run->stubs; none of them rolls on Phaser.Math.RND (they use gc_stub_rand), so the
 * run's own rolls stay where upstream has them, minus what the stand-in itself would have rolled. */
#include "internal.h"

/* ---- ai-core: a random usable move, on the stand-ins' own RNG ---- */
void gc_port_enemy_command(gc_run *run, int enemy_slot)
{
    pr_battle *bt = &run->bt;
    pr_pokemon *p = bt->field[enemy_slot];
    if (!p || !p->on_field || p->hp <= 0) return;
    struct pr_turn_command *tc = &bt->commands[enemy_slot];
    memset(tc, 0, sizeof *tc);
    tc->command = COMMAND_FIGHT;
    tc->use_mode = MOVE_USE_MODE_NORMAL;
    if (p->n_move_queue) {
        /* a move locked in by the last turn */
        const pr_turn_move *q = &p->move_queue[0];
        tc->move = q->move;
        tc->use_mode = q->use_mode;
        tc->n_targets = q->n_targets;
        memcpy(tc->targets, q->targets, sizeof tc->targets);
        tc->cursor = PR_CURSOR_UNDEFINED;
        return;
    }
    gc_stub(run, GC_STUB_ENEMY_AI);
    int usable[4], n = 0;
    for (int i = 0; i < p->n_moves; i++) if (gc_move_usable(p, i, NULL)) usable[n++] = i;
    const int move = n ? p->moves[usable[gc_stub_rand(run, (uint32_t)n)]] : MOVE_ID_STRUGGLE;
    tc->move = (uint16_t)move;
    tc->cursor = PR_CURSOR_UNDEFINED;
    int8_t tg[4];
    bool multiple = false;
    int nt = pr_get_move_targets(bt, p, move, -1, tg, &multiple);
    if (nt > 4) nt = 4;
    if (nt > 1 && !multiple) {
        /* one target picked among several (upstream: getNextTargets) */
        tg[0] = tg[gc_stub_rand(run, (uint32_t)nt)];
        nt = 1;
    }
    tc->n_targets = (uint8_t)nt;
    memcpy(tc->targets, tg, (size_t)nt);
}

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

/* ---- reward-core: a placeholder pool ---- */
typedef struct { const char *key; uint8_t target; int8_t tier; } stub_reward;
static const stub_reward pool[] = {
    {"POTION", GC_TARGET_POKEMON, 0}, {"SUPER_POTION", GC_TARGET_POKEMON, 1}, {"REVIVE", GC_TARGET_POKEMON, 1},
    {"FULL_HEAL", GC_TARGET_POKEMON, 1}, {"ETHER", GC_TARGET_MOVE, 0}, {"ELIXIR", GC_TARGET_POKEMON, 1},
    {"RARE_CANDY", GC_TARGET_POKEMON, 1}, {"POKEBALL", GC_TARGET_NONE, 0}, {"GREAT_BALL", GC_TARGET_NONE, 1},
    {"HYPER_POTION", GC_TARGET_POKEMON, 2}, {"MAX_REVIVE", GC_TARGET_POKEMON, 2},
};
#define POOL_N ((int)(sizeof pool / sizeof pool[0]))

int gc_port_reward_options(gc_run *run, int reroll_count, gc_offer out[GC_MAX_OPTIONS])
{
    (void)reroll_count;
    gc_stub(run, GC_STUB_REWARDS);
    int n = 3 + gc_item_stack(run, "GOLDEN_POKEBALL");
    if (n > GC_MAX_OPTIONS) n = GC_MAX_OPTIONS;
    for (int i = 0; i < n; i++) {
        const stub_reward *r = &pool[gc_stub_rand(run, POOL_N)];
        out[i].key = r->key;
        out[i].arg = -1;
        out[i].tier = r->tier;
        out[i].target = r->target;
        out[i].cost = 0;
    }
    return n;
}

static bool heal(gc_mon *m, int points, int percent, bool revive, bool status)
{
    pr_pokemon *b = &m->b;
    if ((b->hp <= 0) != revive) return false;
    if (revive || status) {
        b->status = STATUS_EFFECT_NONE;
        b->status_toxic_turns = 0;
        b->status_sleep_turns = b->status_freeze_turns = -1;
    }
    const int max = b->stats[0];
    int add = (int)floor(percent * 0.01 * max);
    if (add < points) add = points;
    if (add < 1) add = 1;
    b->hp = b->hp + add > max ? max : b->hp + add;
    return true;
}

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
    const int ball = pokeball_type(k);
    if (ball >= 0) {
        const int add = ball == POKEBALL_TYPE_MASTER_BALL ? 1 : 5;
        run->pokeballs[ball] = (uint8_t)(run->pokeballs[ball] + add > 99 ? 99 : run->pokeballs[ball] + add);
        gc_ev_msg(run, "battle:rewardGain", 1, (gc_arg){"modifierName", GC_ARG_STRING, 0, k});
        return true;
    }
    if (o->target == GC_TARGET_NONE) {
        gc_item_add(run, k, o->arg, 0, 1);
        return true;
    }
    if (party < 0 || party >= run->n_party) { *why = "pick a Pokemon"; return false; }
    gc_mon *m = &run->party[party];
    pr_pokemon *b = &m->b;
    bool ok = true;
    if (!strcmp(k, "POTION")) ok = heal(m, 20, 10, false, false);
    else if (!strcmp(k, "SUPER_POTION")) ok = heal(m, 50, 25, false, false);
    else if (!strcmp(k, "HYPER_POTION")) ok = heal(m, 200, 50, false, false);
    else if (!strcmp(k, "MAX_POTION")) ok = heal(m, 0, 100, false, false);
    else if (!strcmp(k, "FULL_RESTORE")) ok = heal(m, 0, 100, false, true);
    else if (!strcmp(k, "REVIVE")) ok = heal(m, 0, 50, true, false);
    else if (!strcmp(k, "MAX_REVIVE")) ok = heal(m, 0, 100, true, false);
    else if (!strcmp(k, "FULL_HEAL")) {
        ok = b->hp > 0 && b->status != STATUS_EFFECT_NONE;
        if (ok) { b->status = STATUS_EFFECT_NONE; b->status_toxic_turns = 0; b->status_sleep_turns = b->status_freeze_turns = -1; }
    } else if (!strcmp(k, "ETHER") || !strcmp(k, "MAX_ETHER")) {
        if (move_slot < 0 || move_slot >= b->n_moves) { *why = "pick a move"; return false; }
        const int pts = k[0] == 'M' ? -1 : 10;
        b->pp_used[move_slot] = (uint8_t)(pts > -1 ? (b->pp_used[move_slot] > pts ? b->pp_used[move_slot] - pts : 0) : 0);
    } else if (!strcmp(k, "ELIXIR") || !strcmp(k, "MAX_ELIXIR")) {
        const int pts = k[0] == 'M' ? -1 : 10;
        for (int i = 0; i < b->n_moves; i++)
            b->pp_used[i] = (uint8_t)(pts > -1 ? (b->pp_used[i] > pts ? b->pp_used[i] - pts : 0) : 0);
    } else if (!strcmp(k, "RARE_CANDY")) {
        if (b->level >= pr_run_get_max_exp_level(&run->run, run->wave, false)) { *why = "partyUiHandler:anyEffect"; return false; }
        const pr_species *sp = gc_species(b->species);
        b->level++;
        if (b->level <= pr_run_get_max_exp_level(&run->run, run->wave, true)) m->exp = gc_level_total_exp(b->level, sp ? sp->growth_rate : 0);
        /* FRIENDSHIP_GAIN_FROM_RARE_CANDY (6), capped at RARE_CANDY_FRIENDSHIP_CAP (200) */
        if (b->friendship < 200) b->friendship = (uint8_t)(b->friendship + 6 > 200 ? 200 : b->friendship + 6);
        gc_rq_unshift(run, gc_rp(RP_LEVEL_UP, party, b->level - 1, b->level));
    } else {
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
