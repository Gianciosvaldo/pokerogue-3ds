/* Task 60: what taking a healing, revive, PP restore or PP Up reward does (gc_port_apply_reward ->
 * gc_reward_apply -> reward-core's pr_reward_apply, the result written back into the run).
 *
 * Two parts, both through the port function the reward screen calls:
 *  1. a table of hand-made cases with the numbers worked out from upstream's source at 8555c08
 *     (PokemonHpRestoreModifier, PokemonPpRestoreModifier, PokemonAllMovePpRestoreModifier, PokemonPpUpModifier,
 *     and each type's selectFilter / moveSelectFilter in modifier-type.ts): full HP, a fainted target,
 *     statuses, empty PP, PP Ups, invalid targets, a reward that would do nothing, the Healing Charm, a shuffled
 *     party order, a purchase against a free reward;
 *  2. random states (party order, HP, status, PP, PP Ups, Healing Charm stacks, targets good and bad, free or
 *     bought) checked against a second implementation of upstream's rules written out in this file, not
 *     against reward-core. It prints how often each item was taken and refused.
 * Neither part needs an upstream checkout. The application against upstream itself is
 * tools/applydiff.sh. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;
static long checks;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "reward apply FAIL line %d: %s\n", __LINE__, #x); exit(1); } checks++; } while (0)
#define CHECKF(x, ...) do { if (!(x)) { fprintf(stderr, "reward apply FAIL line %d: %s\n  ", __LINE__, #x); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); exit(1); } checks++; } while (0)

static const char *const NO_EFFECT = "partyUiHandler:anyEffect";

/* ---- a run with `n` party members (cost-1 species so that six fit the budget), everything at a known state ---- */
static void setup(const char *seed, int n)
{
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    gc_starter team[6];
    memset(team, 0, sizeof team);
    int k = 0;
    for (int i = 0; i < gc_starter_cost_count && k < n; i++) {
        if (gc_starter_costs[i].cost != 1) continue;
        team[k].species = gc_starter_costs[i].id;
        team[k].tera_type = -1;
        for (int j = 0; j < 6; j++) team[k].ivs[j] = 15;
        k++;
    }
    const char *why;
    CHECK(k == n);
    CHECK(gc_run_set_starters(&run, team, n, &why));
    CHECK(run.n_party == n);
    run.sys = &sys;
    for (int i = 0; i < n; i++) {
        pr_pokemon *b = &run.party[i].b;
        b->level = 50;
        gc_mon_recalc(&run, &run.party[i]);
        b->stats[0] = 300;
        b->hp = 300;
        b->status = STATUS_EFFECT_NONE;
        for (int s = 0; s < 4; s++) { b->pp_used[s] = 0; b->pp_up[s] = 0; b->max_pp_override[s] = -1; }
    }
}

static void drain(void) { gc_event e; while (gc_run_next_event(&run, &e)) {} }

static gc_mon *mon(int storage) { return &run.party[storage]; }

static void faint(int storage)
{
    mon(storage)->b.hp = 0;
    mon(storage)->b.status = STATUS_EFFECT_FAINT;
}

/* give the Healing Charm `stacks` stacks (the run's own item list, as a reward would) */
static void charm(int stacks)
{
    run.n_items = 0;
    if (stacks > 0) gc_item_add(&run, "HEALING_CHARM", -1, 0, stacks);
}

static gc_offer offer(const char *key, double cost)
{
    gc_offer o;
    memset(&o, 0, sizeof o);
    o.key = key;
    o.arg = -1;
    o.target = gc_reward_target(key);
    o.cost = cost;
    return o;
}

typedef struct { bool ok; const char *why; } result;
static result take(const char *key, double cost, int party, int slot)
{
    const gc_offer o = offer(key, cost);
    result r = {false, ""};
    r.ok = gc_port_apply_reward(&run, &o, party, slot, &r.why);
    return r;
}

/* the party's whole state, to see that nothing but the target changed */
typedef struct { int hp, status, pp_used[4], pp_up[4]; } snap;
static void snapshot(snap out[6])
{
    for (int i = 0; i < run.n_party; i++) {
        const pr_pokemon *b = &run.party[i].b;
        out[i].hp = b->hp;
        out[i].status = b->status;
        for (int s = 0; s < 4; s++) { out[i].pp_used[s] = b->pp_used[s]; out[i].pp_up[s] = b->pp_up[s]; }
    }
}
static bool same(const snap *a, const snap *b) { return !memcmp(a, b, sizeof *a); }

/* ---- 1. the table ---- */
typedef struct {
    const char *name, *key;
    double cost;                    /* 0 = a free reward, else a purchase */
    int max_hp, hp, status;         /* before */
    int charm;                      /* Healing Charm stacks */
    int pp_used[4], pp_up[4];       /* before: move slots 0-3 (n_moves 4) */
    int party, slot;                /* the choice: a party[] index (the target is party index 2 of 4), a move slot */
    bool ok;                        /* expected: taken */
    const char *why;                /* expected refusal */
    int hp_after, status_after;     /* expected target state (when taken; else unchanged) */
    int pp_used_after[4], pp_up_after[4];
} acase;

#define T 2   /* the target's party[] index in the table's runs */
#define ANY {0, 0, 0, 0}
static const acase table[] = {
    /* ---- Potions: restorePoints x HealingBooster, restorePercent of max HP, whichever is more, at least 1 ---- */
    {"Potion heals 30 (10% of 300 beats 20 points)", "POTION", 0, 300, 100, 0, 0, ANY, ANY, T, 0, true, NULL, 130, 0, ANY, ANY},
    {"Potion on a full-HP Pokemon: no effect", "POTION", 0, 300, 300, 0, 0, ANY, ANY, T, 0, false, NO_EFFECT, 300, 0, ANY, ANY},
    {"Potion on a fainted Pokemon: no effect", "POTION", 0, 300, 0, STATUS_EFFECT_FAINT, 0, ANY, ANY, T, 0, false, NO_EFFECT, 0, STATUS_EFFECT_FAINT, ANY, ANY},
    {"Potion stops at max HP", "POTION", 0, 300, 295, 0, 0, ANY, ANY, T, 0, true, NULL, 300, 0, ANY, ANY},
    {"Potion: 20 points beat 10% of 100", "POTION", 0, 100, 50, 0, 0, ANY, ANY, T, 0, true, NULL, 70, 0, ANY, ANY},
    {"Potion keeps a status", "POTION", 0, 300, 100, STATUS_EFFECT_BURN, 0, ANY, ANY, T, 0, true, NULL, 130, STATUS_EFFECT_BURN, ANY, ANY},
    {"Potion with one Healing Charm: floor(20 x 1.1) = 22", "POTION", 0, 100, 50, 0, 1, ANY, ANY, T, 0, true, NULL, 72, 0, ANY, ANY},
    {"Potion with two Healing Charms: floor(20 x 1.2) = 24", "POTION", 0, 100, 50, 0, 2, ANY, ANY, T, 0, true, NULL, 74, 0, ANY, ANY},
    {"Potion with five Healing Charms: floor(20 x 1.5) = 30", "POTION", 0, 100, 50, 0, 5, ANY, ANY, T, 0, true, NULL, 80, 0, ANY, ANY},
    {"Super Potion: 25% of 300 beats 50 points", "SUPER_POTION", 0, 300, 10, 0, 0, ANY, ANY, T, 0, true, NULL, 85, 0, ANY, ANY},
    {"Super Potion: 50 points beat 25% of 100", "SUPER_POTION", 0, 100, 10, 0, 0, ANY, ANY, T, 0, true, NULL, 60, 0, ANY, ANY},
    {"Hyper Potion: 200 points beat 50% of 300", "HYPER_POTION", 0, 300, 1, 0, 0, ANY, ANY, T, 0, true, NULL, 201, 0, ANY, ANY},
    {"Hyper Potion: 50% of 600 beats 200 points", "HYPER_POTION", 0, 600, 1, 0, 0, ANY, ANY, T, 0, true, NULL, 301, 0, ANY, ANY},
    {"Hyper Potion with three Healing Charms: floor(200 x 1.3) = 260 beats 150", "HYPER_POTION", 0, 300, 1, 0, 3, ANY, ANY, T, 0, true, NULL, 261, 0, ANY, ANY},
    {"Hyper Potion with five Healing Charms: 300 points, capped at max HP", "HYPER_POTION", 0, 300, 100, 0, 5, ANY, ANY, T, 0, true, NULL, 300, 0, ANY, ANY},
    {"Max Potion fills", "MAX_POTION", 0, 300, 10, 0, 0, ANY, ANY, T, 0, true, NULL, 300, 0, ANY, ANY},
    {"Max Potion on full HP: no effect", "MAX_POTION", 0, 300, 300, 0, 0, ANY, ANY, T, 0, false, NO_EFFECT, 300, 0, ANY, ANY},
    {"Max Potion on a fainted Pokemon: no effect", "MAX_POTION", 0, 300, 0, STATUS_EFFECT_FAINT, 0, ANY, ANY, T, 0, false, NO_EFFECT, 0, STATUS_EFFECT_FAINT, ANY, ANY},
    /* ---- Full Restore: HP and the status ---- */
    {"Full Restore heals and cures", "FULL_RESTORE", 0, 300, 100, STATUS_EFFECT_TOXIC, 0, ANY, ANY, T, 0, true, NULL, 300, 0, ANY, ANY},
    {"Full Restore at full HP still cures a status", "FULL_RESTORE", 0, 300, 300, STATUS_EFFECT_SLEEP, 0, ANY, ANY, T, 0, true, NULL, 300, 0, ANY, ANY},
    {"Full Restore at full HP with no status: no effect", "FULL_RESTORE", 0, 300, 300, 0, 0, ANY, ANY, T, 0, false, NO_EFFECT, 300, 0, ANY, ANY},
    {"Full Restore on a fainted Pokemon: no effect", "FULL_RESTORE", 0, 300, 0, STATUS_EFFECT_FAINT, 0, ANY, ANY, T, 0, false, NO_EFFECT, 0, STATUS_EFFECT_FAINT, ANY, ANY},
    /* ---- Full Heal: a status (and only that; HP is not touched) or a confusion, a living Pokemon ---- */
    {"Full Heal cures a burn and leaves HP alone", "FULL_HEAL", 0, 300, 100, STATUS_EFFECT_BURN, 0, ANY, ANY, T, 0, true, NULL, 100, 0, ANY, ANY},
    {"Full Heal cures a sleeper at full HP", "FULL_HEAL", 0, 300, 300, STATUS_EFFECT_SLEEP, 0, ANY, ANY, T, 0, true, NULL, 300, 0, ANY, ANY},
    {"Full Heal ignores the Healing Charm", "FULL_HEAL", 0, 300, 100, STATUS_EFFECT_TOXIC, 5, ANY, ANY, T, 0, true, NULL, 100, 0, ANY, ANY},
    {"Full Heal on a Pokemon with no status: no effect", "FULL_HEAL", 0, 300, 100, 0, 0, ANY, ANY, T, 0, false, NO_EFFECT, 100, 0, ANY, ANY},
    {"Full Heal on a fainted Pokemon: no effect", "FULL_HEAL", 0, 300, 0, STATUS_EFFECT_FAINT, 0, ANY, ANY, T, 0, false, NO_EFFECT, 0, STATUS_EFFECT_FAINT, ANY, ANY},
    {"Bought Full Heal on a healthy Pokemon: refused", "FULL_HEAL", 300, 300, 100, 0, 0, ANY, ANY, T, 0, false, NO_EFFECT, 100, 0, ANY, ANY},
    {"Bought Full Heal cures", "FULL_HEAL", 300, 300, 100, STATUS_EFFECT_PARALYSIS, 0, ANY, ANY, T, 0, true, NULL, 100, 0, ANY, ANY},
    /* ---- Revives: the fainted only; the Healing Charm doesn't count; a status is cleared ---- */
    {"Revive brings back half", "REVIVE", 0, 300, 0, STATUS_EFFECT_FAINT, 0, ANY, ANY, T, 0, true, NULL, 150, 0, ANY, ANY},
    {"Revive on a Pokemon that can still fight: no effect", "REVIVE", 0, 300, 100, 0, 0, ANY, ANY, T, 0, false, NO_EFFECT, 100, 0, ANY, ANY},
    {"Revive ignores the Healing Charm", "REVIVE", 0, 300, 0, STATUS_EFFECT_FAINT, 5, ANY, ANY, T, 0, true, NULL, 150, 0, ANY, ANY},
    {"Revive rounds 50% of 1 HP down and gives 1", "REVIVE", 0, 1, 0, STATUS_EFFECT_FAINT, 0, ANY, ANY, T, 0, true, NULL, 1, 0, ANY, ANY},
    {"Revive rounds 50% of 301 down: 150", "REVIVE", 0, 301, 0, STATUS_EFFECT_FAINT, 0, ANY, ANY, T, 0, true, NULL, 150, 0, ANY, ANY},
    {"Max Revive brings back everything", "MAX_REVIVE", 0, 300, 0, STATUS_EFFECT_FAINT, 0, ANY, ANY, T, 0, true, NULL, 300, 0, ANY, ANY},
    {"Max Revive on a Pokemon that can still fight: no effect", "MAX_REVIVE", 0, 300, 299, 0, 0, ANY, ANY, T, 0, false, NO_EFFECT, 299, 0, ANY, ANY},
    /* ---- Ether and Max Ether: one move ---- */
    {"Ether gives back 10 PP", "ETHER", 0, 300, 300, 0, 0, {0, 12, 3, 0}, ANY, T, 1, true, NULL, 300, 0, {0, 2, 3, 0}, ANY},
    {"Ether doesn't go below 0 PP used", "ETHER", 0, 300, 300, 0, 0, {0, 12, 3, 0}, ANY, T, 2, true, NULL, 300, 0, {0, 12, 0, 0}, ANY},
    {"Ether on a move with all its PP: no effect", "ETHER", 0, 300, 300, 0, 0, {0, 12, 3, 0}, ANY, T, 0, false, NO_EFFECT, 300, 0, {0, 12, 3, 0}, ANY},
    {"Ether works on a fainted Pokemon (its selectFilter is () => null)", "ETHER", 0, 300, 0, STATUS_EFFECT_FAINT, 0, {5, 0, 0, 0}, ANY, T, 0, true, NULL, 0, STATUS_EFFECT_FAINT, {0, 0, 0, 0}, ANY},
    {"Ether with no move chosen: pick a move", "ETHER", 0, 300, 300, 0, 0, {5, 5, 5, 5}, ANY, T, -1, false, "pick a move", 300, 0, ANY, ANY},
    {"Ether on a slot past the moveset: pick a move", "ETHER", 0, 300, 300, 0, 0, {5, 5, 5, 5}, ANY, T, 4, false, "pick a move", 300, 0, ANY, ANY},
    {"PP Up with no move chosen: pick a move", "PP_UP", 0, 300, 300, 0, 0, ANY, ANY, T, -1, false, "pick a move", 300, 0, ANY, ANY},
    {"Max Ether refills the move", "MAX_ETHER", 0, 300, 300, 0, 0, {0, 12, 0, 0}, ANY, T, 1, true, NULL, 300, 0, {0, 0, 0, 0}, ANY},
    {"Max Ether on a move with all its PP: no effect", "MAX_ETHER", 0, 300, 300, 0, 0, {0, 12, 0, 0}, ANY, T, 3, false, NO_EFFECT, 300, 0, {0, 12, 0, 0}, ANY},
    /* ---- Elixir and Max Elixir: every move ---- */
    {"Elixir gives back 10 PP to each move", "ELIXIR", 0, 300, 300, 0, 0, {3, 12, 0, 25}, ANY, T, 0, true, NULL, 300, 0, {0, 2, 0, 15}, ANY},
    {"Elixir with every move at full PP: no effect", "ELIXIR", 0, 300, 300, 0, 0, ANY, ANY, T, 0, false, NO_EFFECT, 300, 0, ANY, ANY},
    {"Max Elixir refills every move", "MAX_ELIXIR", 0, 300, 300, 0, 0, {3, 12, 0, 25}, ANY, T, 0, true, NULL, 300, 0, {0, 0, 0, 0}, ANY},
    {"Max Elixir with every move at full PP: no effect", "MAX_ELIXIR", 0, 300, 300, 0, 0, ANY, ANY, T, 0, false, NO_EFFECT, 300, 0, ANY, ANY},
    /* ---- PP Up and PP Max: ppUp + points, at most 3 ---- */
    {"PP Up adds one", "PP_UP", 0, 300, 300, 0, 0, ANY, {0, 1, 0, 0}, T, 1, true, NULL, 300, 0, ANY, {0, 2, 0, 0}},
    {"PP Up from none", "PP_UP", 0, 300, 300, 0, 0, ANY, ANY, T, 0, true, NULL, 300, 0, ANY, {1, 0, 0, 0}},
    {"PP Up on a move with 3 PP Ups: no effect", "PP_UP", 0, 300, 300, 0, 0, ANY, {3, 0, 0, 0}, T, 0, false, NO_EFFECT, 300, 0, ANY, {3, 0, 0, 0}},
    {"PP Max adds up to the limit", "PP_MAX", 0, 300, 300, 0, 0, ANY, {0, 2, 0, 0}, T, 1, true, NULL, 300, 0, ANY, {0, 3, 0, 0}},
    {"PP Max from none", "PP_MAX", 0, 300, 300, 0, 0, ANY, ANY, T, 2, true, NULL, 300, 0, ANY, {0, 0, 3, 0}},
    {"PP Max on a move with 3 PP Ups: no effect", "PP_MAX", 0, 300, 300, 0, 0, ANY, {0, 0, 0, 3}, T, 3, false, NO_EFFECT, 300, 0, ANY, {0, 0, 0, 3}},
    {"PP Up works on a fainted Pokemon", "PP_UP", 0, 300, 0, STATUS_EFFECT_FAINT, 0, ANY, ANY, T, 0, true, NULL, 0, STATUS_EFFECT_FAINT, ANY, {1, 0, 0, 0}},
    {"PP Up keeps the PP used", "PP_UP", 0, 300, 300, 0, 0, {4, 0, 0, 0}, ANY, T, 0, true, NULL, 300, 0, {4, 0, 0, 0}, {1, 0, 0, 0}},
    /* ---- invalid targets ---- */
    {"No party member: pick a Pokemon", "POTION", 0, 300, 100, 0, 0, ANY, ANY, -1, 0, false, "pick a Pokemon", 100, 0, ANY, ANY},
    {"A party index past the party: pick a Pokemon", "REVIVE", 0, 300, 0, STATUS_EFFECT_FAINT, 0, ANY, ANY, 4, 0, false, "pick a Pokemon", 0, STATUS_EFFECT_FAINT, ANY, ANY},
    {"No party member for a PP Up: pick a Pokemon", "PP_UP", 0, 300, 300, 0, 0, ANY, ANY, 7, 0, false, "pick a Pokemon", 300, 0, ANY, ANY},
    /* ---- a purchase is refused the same way, and is taken the same way ---- */
    {"Bought Potion on a full-HP Pokemon: refused", "POTION", 500, 300, 300, 0, 0, ANY, ANY, T, 0, false, NO_EFFECT, 300, 0, ANY, ANY},
    {"Bought Potion heals", "POTION", 500, 300, 100, 0, 0, ANY, ANY, T, 0, true, NULL, 130, 0, ANY, ANY},
    {"Bought Max Revive", "MAX_REVIVE", 4000, 300, 0, STATUS_EFFECT_FAINT, 0, ANY, ANY, T, 0, true, NULL, 300, 0, ANY, ANY},
    {"Bought Ether", "ETHER", 300, 300, 300, 0, 0, {0, 0, 7, 0}, ANY, T, 2, true, NULL, 300, 0, {0, 0, 0, 0}, ANY},
    {"Bought Full Restore with nothing to heal: refused", "FULL_RESTORE", 1000, 300, 300, 0, 0, ANY, ANY, T, 0, false, NO_EFFECT, 300, 0, ANY, ANY},
};
#define N_TABLE ((int)(sizeof table / sizeof table[0]))

/* 4 party members; the target is party[2]. Its moves are 4 distinct ones, so every slot is in use */
static void run_case(const acase *c)
{
    setup("reward-apply-table", 4);
    gc_mon *m = mon(T);
    pr_pokemon *b = &m->b;
    static const int ids[4] = {MOVE_ID_TACKLE, MOVE_ID_GROWL, MOVE_ID_EMBER, MOVE_ID_SCRATCH};
    for (int s = 0; s < 4; s++) gc_mon_set_move(m, s, ids[s]);
    b->n_moves = 4;
    b->stats[0] = c->max_hp;
    b->hp = c->hp;
    b->status = (uint8_t)c->status;
    for (int s = 0; s < 4; s++) { b->pp_used[s] = (uint8_t)c->pp_used[s]; b->pp_up[s] = (uint8_t)c->pp_up[s]; }
    charm(c->charm);
    drain();
    snap before[6], after[6];
    snapshot(before);
    const result r = take(c->key, c->cost, c->party, c->slot);
    snapshot(after);
    CHECKF(r.ok == c->ok, "%s: taken %d, expected %d", c->name, r.ok, c->ok);
    if (!c->ok) {
        CHECKF(!strcmp(r.why, c->why), "%s: why '%s', expected '%s'", c->name, r.why, c->why);
        CHECKF(same(before, after), "%s: a refused reward changed the party", c->name);
        gc_event e;
        CHECKF(!gc_run_next_event(&run, &e), "%s: a refused reward queued an event", c->name);
        return;
    }
    CHECKF(after[T].hp == c->hp_after, "%s: hp %d, expected %d", c->name, after[T].hp, c->hp_after);
    CHECKF(after[T].status == c->status_after, "%s: status %d, expected %d", c->name, after[T].status, c->status_after);
    for (int s = 0; s < 4; s++) {
        CHECKF(after[T].pp_used[s] == c->pp_used_after[s], "%s: pp_used[%d] %d, expected %d", c->name, s, after[T].pp_used[s], c->pp_used_after[s]);
        CHECKF(after[T].pp_up[s] == c->pp_up_after[s], "%s: pp_up[%d] %d, expected %d", c->name, s, after[T].pp_up[s], c->pp_up_after[s]);
    }
    /* nobody else changed */
    for (int i = 0; i < run.n_party; i++) if (i != T) CHECKF(!memcmp(&before[i], &after[i], sizeof before[i]), "%s: party %d changed", c->name, i);
    /* the one HP event the party screen listens to: the target's HP now */
    gc_event e;
    CHECKF(gc_run_next_event(&run, &e) && e.kind == GC_EV_HP && e.a == 0 && e.b == T && e.c == after[T].hp, "%s: HP event", c->name);
    CHECKF(!gc_run_next_event(&run, &e), "%s: more than one event", c->name);
    /* the reward leaves no item behind (a consumed reward isn't added to the player's items) */
    CHECKF(run.n_items == (c->charm > 0), "%s: left an item in run->items", c->name);
}

/* ---- Sacred Ash: every fainted member, no target ---- */
static void sacred_ash(void)
{
    setup("reward-apply-ash", 6);
    snap before[6], after[6];
    /* two fainted (one with a leftover status flag from its faint), one hurt, three well; nobody's PP touched */
    faint(1);
    faint(4);
    mon(3)->b.hp = 77;
    mon(2)->b.status = STATUS_EFFECT_BURN;
    mon(0)->b.pp_used[0] = 5;
    drain();
    snapshot(before);
    result r = take("SACRED_ASH", 0, -1, -1);
    snapshot(after);
    CHECK(r.ok);
    for (int i = 0; i < 6; i++) {
        if (i == 1 || i == 4) {
            CHECK(after[i].hp == 300 && after[i].status == STATUS_EFFECT_NONE);
        } else {
            CHECK(!memcmp(&before[i], &after[i], sizeof before[i]));
        }
    }
    int hp_events = 0;
    gc_event e;
    while (gc_run_next_event(&run, &e)) {
        CHECK(e.kind == GC_EV_HP && e.a == 0 && (e.b == 1 || e.b == 4) && e.c == 300);
        hp_events++;
    }
    CHECK(hp_events == 2);
    CHECK(run.n_items == 0);

    /* nobody fainted: a free reward is taken and changes nothing; bought, it is refused and keeps the screen */
    snapshot(before);
    r = take("SACRED_ASH", 0, -1, -1);
    CHECK(r.ok);
    snapshot(after);
    CHECK(!memcmp(before, after, sizeof before));
    CHECK(!gc_run_next_event(&run, &e));
    CHECK(run.n_items == 0);
    r = take("SACRED_ASH", 9000, -1, -1);
    CHECK(!r.ok && !strcmp(r.why, NO_EFFECT));
    snapshot(after);
    CHECK(!memcmp(before, after, sizeof before));

    /* bought, with one fainted: taken (a party member in the choice is ignored) */
    faint(5);
    r = take("SACRED_ASH", 9000, 0, 0);
    CHECK(r.ok);
    CHECK(mon(5)->b.hp == 300 && mon(5)->b.status == STATUS_EFFECT_NONE);
}

/* ---- the party order: a choice names party[], reward-core's party is in upstream's order ---- */
static void shuffled_party_order(void)
{
    setup("reward-apply-order", 6);
    const int8_t order[6] = {3, 5, 0, 4, 1, 2};   /* a permutation as switching in battle makes */
    memcpy(run.party_order, order, sizeof order);
    for (int i = 0; i < 6; i++) mon(i)->b.hp = 100;
    drain();
    for (int target = 0; target < 6; target++) {
        for (int i = 0; i < 6; i++) mon(i)->b.hp = 100;
        const result r = take("SUPER_POTION", 0, target, 0);
        CHECK(r.ok);
        for (int i = 0; i < 6; i++) CHECK(mon(i)->b.hp == (i == target ? 175 : 100));
        gc_event e;
        CHECK(gc_run_next_event(&run, &e) && e.kind == GC_EV_HP && e.b == target && e.c == 175);
        CHECK(!gc_run_next_event(&run, &e));
    }
    /* Ether: the PP of the right Pokemon */
    for (int target = 0; target < 6; target++) {
        for (int i = 0; i < 6; i++) mon(i)->b.pp_used[0] = 6;
        const result r = take("ETHER", 0, target, 0);
        CHECK(r.ok);
        for (int i = 0; i < 6; i++) CHECK(mon(i)->b.pp_used[0] == (i == target ? 0 : 6));
        drain();
    }
    /* Revive: only the fainted target, even with another fainted one next to it in the party order */
    for (int target = 0; target < 6; target++) {
        for (int i = 0; i < 6; i++) faint(i);
        const result r = take("REVIVE", 0, target, 0);
        CHECK(r.ok);
        for (int i = 0; i < 6; i++) CHECK(mon(i)->b.hp == (i == target ? 150 : 0));
        drain();
    }
}

/* ---- Task 96: Rare Candy and Rarer Candy ----
 * Upstream's PokemonLevelIncrementModifier (and the two types that make it) at 8555c08, worked out by hand:
 *  - levels: 1 + the Candy Jar's stacks, whatever level the Pokemon is at. There is NO level cap check: the
 *    type's selectFilter always returns null and Rarer Candy has no filter, so a candy is taken at or above
 *    getMaxExpLevel() (the differential, tools/applydiff.sh, replays upstream's own candy on members at, below and
 *    above the cap: every one is applied). The only refusal is game-core's own: no Pokemon picked for a Rare Candy;
 *  - exp: set to the new level's total (getLevelTotalExp) because getMaxExpLevel(true) has no limit;
 *  - friendship: addFriendship(6, true): +6 once per candy (the Candy Jar adds levels, not friendship), stopping at
 *    200, and a friendship already above 200 stays;
 *  - the starter's candy progress, in the same addFriendship: the 6 (not what the Pokemon's own friendship rose
 *    by) times 3 in Classic, 1 in the other modes, against the starter's cost-based cap (25 for cost 1); a full
 *    cap is a candy and the rest stays as progress;
 *  - one LevelUpPhase per member that got the levels (party order), old level -> new level.
 * The exp formula and the friendship rule themselves are reward-core's (reward-core/tests/candytest.c, and the
 * differential's 4000 cases a seed); this table is about what game-core does with them. */
typedef struct {
    const char *name, *key;
    int wave, mode, rot;                /* party_order is rotated by `rot` places */
    int target;                         /* the storage index a Rare Candy is used on (-1 = none) */
    int level[3], friendship[3];        /* before; the exp is 12345 for every member */
    int jars;                           /* Candy Jar stacks */
    int starter_progress;               /* each member's starter's friendship progress before */
    bool ok;
    const char *why;                    /* the refusal */
    int level_after[3], friendship_after[3];
    int starter_progress_after, starter_candy_after;   /* the starters of the members that got levels (the others' stay) */
} candy_table_case;

#define CAP (-1000)                     /* in a level slot: the wave's level cap; CAP + 3 is three above it */
static const candy_table_case candy_table[] = {
    {"Rare Candy: +1 level, +6 friendship, starter +18 (Classic)", "RARE_CANDY", 100, GAME_MODES_CLASSIC, 0, 0,
     {10, 10, 10}, {100, 50, 50}, 0, 2, true, NULL, {11, 10, 10}, {106, 50, 50}, 20, 0},
    {"Candy Jar x2: +3 levels, still +6 friendship and +18 starter progress", "RARE_CANDY", 100, GAME_MODES_CLASSIC, 0, 0,
     {10, 10, 10}, {100, 50, 50}, 2, 2, true, NULL, {13, 10, 10}, {106, 50, 50}, 20, 0},
    {"at the wave's level cap: still taken", "RARE_CANDY", 10, GAME_MODES_CLASSIC, 0, 0,
     {CAP, 10, 10}, {100, 50, 50}, 0, 2, true, NULL, {CAP + 1, 10, 10}, {106, 50, 50}, 20, 0},
    {"above the wave's level cap with Candy Jars: still taken", "RARE_CANDY", 10, GAME_MODES_CLASSIC, 0, 0,
     {CAP + 5, 10, 10}, {100, 50, 50}, 2, 2, true, NULL, {CAP + 8, 10, 10}, {106, 50, 50}, 20, 0},
    {"friendship 199 stops at 200, the starter still gets the whole +18", "RARE_CANDY", 100, GAME_MODES_CLASSIC, 0, 0,
     {10, 10, 10}, {199, 50, 50}, 0, 2, true, NULL, {11, 10, 10}, {200, 50, 50}, 20, 0},
    {"friendship 200 stays, the starter still gets +18", "RARE_CANDY", 100, GAME_MODES_CLASSIC, 0, 0,
     {10, 10, 10}, {200, 50, 50}, 0, 2, true, NULL, {11, 10, 10}, {200, 50, 50}, 20, 0},
    {"friendship above 200 is kept", "RARE_CANDY", 100, GAME_MODES_CLASSIC, 0, 0,
     {10, 10, 10}, {250, 50, 50}, 1, 2, true, NULL, {12, 10, 10}, {250, 50, 50}, 20, 0},
    {"starter progress reaches its cap of 25: a candy, the rest kept", "RARE_CANDY", 100, GAME_MODES_CLASSIC, 0, 0,
     {10, 10, 10}, {100, 50, 50}, 0, 10, true, NULL, {11, 10, 10}, {106, 50, 50}, 3, 1},
    {"Endless: starter +6, not +18", "RARE_CANDY", 100, GAME_MODES_ENDLESS, 0, 0,
     {10, 10, 10}, {100, 50, 50}, 0, 2, true, NULL, {11, 10, 10}, {106, 50, 50}, 8, 0},
    {"shuffled party order: the storage index asked for gets it", "RARE_CANDY", 100, GAME_MODES_CLASSIC, 1, 2,
     {10, 10, 10}, {100, 50, 50}, 0, 2, true, NULL, {10, 10, 11}, {100, 50, 56}, 20, 0},
    {"level 254 with 5 Candy Jars reaches 260 (pinned native increment)", "RARE_CANDY", 100, GAME_MODES_CLASSIC, 0, 0,
     {254, 10, 10}, {100, 50, 50}, 5, 2, true, NULL, {260, 10, 10}, {106, 50, 50}, 20, 0},
    {"no Pokemon picked: refused, nothing changes", "RARE_CANDY", 100, GAME_MODES_CLASSIC, 0, -1,
     {10, 10, 10}, {100, 50, 50}, 0, 2, false, "pick a Pokemon", {10, 10, 10}, {100, 50, 50}, 2, 0},
    {"a place past the party: refused, nothing changes", "RARE_CANDY", 100, GAME_MODES_CLASSIC, 0, 3,
     {10, 10, 10}, {100, 50, 50}, 0, 2, false, "pick a Pokemon", {10, 10, 10}, {100, 50, 50}, 2, 0},
    {"Rarer Candy: every member +1 level and +6 friendship, no target needed", "RARER_CANDY", 100, GAME_MODES_CLASSIC, 0, -1,
     {10, 12, 15}, {50, 50, 50}, 0, 2, true, NULL, {11, 13, 16}, {56, 56, 56}, 20, 0},
    {"Rarer Candy with Candy Jar x2: +3 levels each", "RARER_CANDY", 100, GAME_MODES_CLASSIC, 0, -1,
     {10, 12, 15}, {50, 50, 50}, 2, 2, true, NULL, {13, 15, 18}, {56, 56, 56}, 20, 0},
    {"Rarer Candy when every member is at or above the level cap: still taken", "RARER_CANDY", 10, GAME_MODES_CLASSIC, 0, -1,
     {CAP, CAP + 3, CAP + 10}, {199, 200, 250}, 0, 2, true, NULL, {CAP + 1, CAP + 4, CAP + 11}, {200, 200, 250}, 20, 0},
    {"Rarer Candy with a shuffled party order", "RARER_CANDY", 100, GAME_MODES_CLASSIC, 2, -1,
     {10, 12, 15}, {50, 60, 70}, 0, 10, true, NULL, {11, 13, 16}, {56, 66, 76}, 3, 1},
};
#define N_CANDY_TABLE ((int)(sizeof candy_table / sizeof candy_table[0]))

static int candy_level(int v, int cap) { return v <= CAP + 20 ? cap + (v - CAP) : v; }

/* the party's three starters' progress and candy, to compare what a candy did to them */
static void set_starters_progress(int progress)
{
    for (int p = 0; p < 3; p++) {
        gc_starter_entry *e = &sys.starters[gc_species_root_id(mon(p)->b.species)];
        e->friendship = progress;
        e->candy_count = 0;
    }
}

static void test_rare_and_rarer_candy_table(void)
{
    long levelled = 0, refused = 0;
    for (int i = 0; i < N_CANDY_TABLE; i++) {
        const candy_table_case *tc = &candy_table[i];
        setup("candy-table-test", 3);
        run.wave = tc->wave;
        run.run.mode = tc->mode;
        const int cap = (int)pr_run_get_max_exp_level(&run.run, run.wave, false);
        CHECK(cap >= 10 && cap < 200);
        /* the three members must have three starters of their own, at cost 1 (progress cap 25) */
        for (int p = 0; p < 3; p++) {
            const int root = gc_species_root_id(mon(p)->b.species);
            CHECK(gc_starter_cost(root) == 1);
            for (int q = 0; q < p; q++) CHECK(root != gc_species_root_id(mon(q)->b.species));
        }
        for (int p = 0; p < 3; p++) {
            mon(p)->b.level = (uint8_t)candy_level(tc->level[p], cap);
            mon(p)->b.friendship = (uint8_t)tc->friendship[p];
            mon(p)->exp = 12345;
            gc_mon_recalc(&run, mon(p));
        }
        for (int p = 0; p < 3; p++) run.party_order[p] = (int8_t)((p + tc->rot) % 3);
        if (tc->jars > 0) gc_item_add(&run, "CANDY_JAR", -1, 0, tc->jars);
        set_starters_progress(tc->starter_progress);
        drain();
        gc_rq_reset(&gc_int(&run)->rq);          /* what the battle left queued is not what the candy queues */
        gc_mon before[3];
        for (int p = 0; p < 3; p++) before[p] = *mon(p);

        result r = take(tc->key, 0, tc->target, -1);
        CHECKF(r.ok == tc->ok, "candy case %d '%s': ok=%d, expected %d", i, tc->name, r.ok, tc->ok);
        if (!tc->ok) {
            CHECKF(!strcmp(r.why, tc->why), "candy case %d '%s': why='%s', expected '%s'", i, tc->name, r.why, tc->why);
            refused++;
        }

        int expected_ups[3][3], n_ups = 0;
        for (int p = 0; p < 3; p++) {            /* by storage index; the LevelUpPhases come in party order */
            const gc_mon *m = mon(p);
            const int want = candy_level(tc->level_after[p], cap);
            CHECKF(m->b.level == want, "candy case %d '%s': mon(%d) level=%d, expected %d", i, tc->name, p, m->b.level, want);
            CHECKF(m->b.friendship == tc->friendship_after[p], "candy case %d '%s': mon(%d) friendship=%d, expected %d",
                   i, tc->name, p, m->b.friendship, tc->friendship_after[p]);
            const bool got_levels = m->b.level != before[p].b.level;
            const double want_exp = got_levels ? pr_reward_level_total_exp(m->b.level, gc_species(m->b.species)->growth_rate) : 12345;
            CHECKF(m->exp == want_exp, "candy case %d '%s': mon(%d) exp=%.0f, expected %.0f", i, tc->name, p, m->exp, want_exp);
            const gc_starter_entry *e = &sys.starters[gc_species_root_id(m->b.species)];
            CHECKF(e->friendship == (got_levels ? tc->starter_progress_after : tc->starter_progress) &&
                   e->candy_count == (got_levels ? tc->starter_candy_after : 0),
                   "candy case %d '%s': mon(%d) starter progress %d candy %d, expected %d %d", i, tc->name, p, e->friendship, e->candy_count,
                   got_levels ? tc->starter_progress_after : tc->starter_progress, got_levels ? tc->starter_candy_after : 0);
            /* nothing else of the member moved: HP, moves, PP, status */
            CHECK(m->b.hp == before[p].b.hp && m->b.status == before[p].b.status && !memcmp(m->b.moves, before[p].b.moves, sizeof m->b.moves)
                  && !memcmp(m->b.pp_used, before[p].b.pp_used, sizeof m->b.pp_used));
            if (got_levels) levelled++;
        }
        for (int k = 0; k < 3; k++) {             /* party index k = storage party_order[k] */
            const gc_mon *m = mon(run.party_order[k]);
            if (m->b.level != before[run.party_order[k]].b.level) {
                expected_ups[n_ups][0] = run.party_order[k];
                expected_ups[n_ups][1] = before[run.party_order[k]].b.level;
                expected_ups[n_ups][2] = m->b.level;
                n_ups++;
            }
        }
        gc_internal *in = gc_int(&run);
        gc_rphase qp;
        int n_got = 0;
        while (gc_rq_next(&in->rq, &qp)) {
            CHECKF(qp.type == RP_LEVEL_UP, "candy case %d '%s': queued phase type %d", i, tc->name, qp.type);
            CHECKF(n_got < n_ups && qp.a == expected_ups[n_got][0] && qp.b == expected_ups[n_got][1] && qp.c == expected_ups[n_got][2],
                   "candy case %d '%s': LevelUpPhase %d is storage %d %d -> %d", i, tc->name, n_got, qp.a, qp.b, qp.c);
            n_got++;
        }
        CHECKF(n_got == n_ups, "candy case %d '%s': %d LevelUpPhases queued, expected %d", i, tc->name, n_got, n_ups);
        CHECKF(tc->ok == (n_ups > 0), "candy case %d '%s': %d members got levels", i, tc->name, n_ups);
        gc_event e;                               /* a candy changes no HP: no HP event */
        while (gc_run_next_event(&run, &e)) CHECK(e.kind != GC_EV_HP);
    }
    printf("candy table: %d cases ok (%ld members levelled, %ld refused)\n", N_CANDY_TABLE, levelled, refused);
}

/* ---- the same through the real flow: a wave won, the reward screen, a candy, the level-up and the move it
 * brings, the next wave's command screen (the only place a run saves), a save, a load, the turn after ---- */
static gc_run flow_loaded;

static void flow_events(int *level_ups, int *new_level)
{
    gc_event e;
    while (gc_run_next_event(&run, &e))
        if (e.kind == GC_EV_LEVEL_UP) { (*level_ups)++; *new_level = e.b; }
}

static const gc_screen *flow_fight(gc_run *r)
{
    const char *why;
    gc_choice c = {GC_CH_FIGHT, 0, -1, 0};
    CHECK(gc_run_choose(r, &c, &why));
    return gc_run_screen(r);
}

static void test_candy_level_up_save_reload_resume(void)
{
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, "candy-flow", 7);
    gc_starter team = {.species = SPECIES_ID_BULBASAUR, .tera_type = -1};
    for (int j = 0; j < 6; j++) team.ivs[j] = 15;
    const char *why;
    CHECK(gc_run_set_starters(&run, &team, 1, &why));
    run.sys = &sys;
    drain();
    CHECK(run.screen.kind == GC_SCREEN_COMMAND && gc_run_can_save(&run));
    gc_mon *lead = mon(0);
    static const int moves[4] = {MOVE_ID_TACKLE, MOVE_ID_GROWL, MOVE_ID_VINE_WHIP, MOVE_ID_SCRATCH};
    for (int j = 0; j < 4; j++) gc_mon_set_move(lead, j, moves[j]);
    lead->b.n_moves = 4;
    lead->b.level = 6;
    lead->exp = pr_reward_level_total_exp(6, gc_species(SPECIES_ID_BULBASAUR)->growth_rate);
    gc_mon_recalc(&run, lead);
    lead->b.friendship = 150;
    run.enemy[0].b.hp = 1;

    /* win the wave: the reward screen */
    for (int turn = 0; turn < 8 && run.screen.kind == GC_SCREEN_COMMAND; turn++) { flow_fight(&run); drain(); }
    CHECK(run.screen.kind == GC_SCREEN_REWARD);
    CHECK(lead->b.level == 6);   /* the wave's experience is far from a level */
    const int wave = run.wave;
    /* the wave's victory already gave the lead friendship and its starter some candy progress */
    const int f0 = lead->b.friendship, progress0 = sys.starters[SPECIES_ID_BULBASAUR].friendship;
    const int candies0 = sys.starters[SPECIES_ID_BULBASAUR].candy_count;

    /* the candy, with two Candy Jars (+3 levels), on the lead: level 7's Leech Seed is the one new move, and it
     * asks which of the four moves to forget */
    gc_item_add(&run, "CANDY_JAR", -1, 0, 2);
    run.screen.options[0] = offer("RARE_CANDY", 0);
    gc_choice pick = {GC_CH_REWARD, 0, 0, -1};
    CHECK(gc_run_choose(&run, &pick, &why));
    int level_ups = 0, new_level = 0;
    flow_events(&level_ups, &new_level);
    CHECK(level_ups == 1 && new_level == 9);
    CHECK(lead->b.level == 9 && lead->b.friendship == f0 + 6);
    CHECK(lead->exp == pr_reward_level_total_exp(9, gc_species(SPECIES_ID_BULBASAUR)->growth_rate));
    CHECK(sys.starters[SPECIES_ID_BULBASAUR].friendship == progress0 + 18 && sys.starters[SPECIES_ID_BULBASAUR].candy_count == candies0);
    CHECK(run.screen.kind == GC_SCREEN_LEARN_MOVE && run.screen.party == 0 && run.screen.move == MOVE_ID_LEECH_SEED);
    /* a level-up that isn't finished is never saved: the queue isn't part of a save */
    CHECK(!gc_run_can_save(&run));
    static uint8_t buf[GC_SAVE_MAX];
    CHECK(gc_run_save(&run, buf, sizeof buf) == 0);
    gc_choice forget = {GC_CH_FORGET, 3, 0, 0};
    CHECK(gc_run_choose(&run, &forget, &why));
    CHECK(lead->b.moves[3] == MOVE_ID_LEECH_SEED && lead->b.moves[0] == MOVE_ID_TACKLE);
    drain();
    /* the next wave, at its command screen: the save point */
    for (int step = 0; step < 20 && run.screen.kind == GC_SCREEN_CHECK_SWITCH; step++) {
        gc_choice no = {GC_CH_CONFIRM, 0, 0, 0};
        CHECK(gc_run_choose(&run, &no, &why));
        drain();
    }
    CHECK(run.screen.kind == GC_SCREEN_COMMAND && run.wave == wave + 1);
    CHECK(gc_run_can_save(&run));
    const size_t n = gc_run_save(&run, buf, sizeof buf);
    CHECK(n > 0);
    memset(&flow_loaded, 0, sizeof flow_loaded);
    CHECK(gc_run_load(&flow_loaded, buf, n, &why) && !flow_loaded.load_dropped);
    flow_loaded.sys = &sys;
    const gc_mon *back = &flow_loaded.party[0];
    CHECK(back->b.level == 9 && back->b.friendship == f0 + 6 && back->exp == lead->exp);
    CHECK(back->b.moves[3] == MOVE_ID_LEECH_SEED && back->b.moves[0] == MOVE_ID_TACKLE && back->b.n_moves == 4);
    CHECK(!memcmp(back->b.stats, lead->b.stats, sizeof lead->b.stats) && back->b.hp == lead->b.hp);
    CHECK(flow_loaded.screen.kind == GC_SCREEN_COMMAND && flow_loaded.wave == wave + 1);
    CHECK(flow_loaded.n_items == run.n_items);
    /* the turn after the load is the turn the original plays */
    const int hp = lead->b.hp, foe_hp = run.enemy[0].b.hp, pp = lead->b.pp_used[0];
    CHECK(flow_loaded.party[0].b.pp_used[0] == pp);
    flow_fight(&run);
    flow_fight(&flow_loaded);
    CHECK(run.screen.kind == flow_loaded.screen.kind);
    CHECK(run.party[0].b.hp == flow_loaded.party[0].b.hp && run.enemy[0].b.hp == flow_loaded.enemy[0].b.hp);
    CHECK(run.party[0].b.pp_used[0] == pp + 1 && flow_loaded.party[0].b.pp_used[0] == pp + 1);
    CHECK(run.party[0].b.hp != hp || run.enemy[0].b.hp != foe_hp);
    printf("candy flow: reward -> level-up -> Leech Seed -> next wave -> save -> load -> turn ok\n");
}

/* ---- Poke Balls (Task 96): AddPokeballModifier at 8555c08, numbers worked out from its source ---- */
/* the three locale entries the name and the "You received" text come from, copied from upstream's locales/en */
static const char *poke_locale(void *ctx, const char *file, int fallback, size_t *len)
{
    (void)ctx;
    const char *t = NULL;
    if (fallback) return NULL;
    if (!strcmp(file, "battle")) t = "{\"rewardGain\": \"You received\\n{{modifierName}}!\"}";
    else if (!strcmp(file, "modifier-type")) t = "{\"ModifierType\": {\"AddPokeballModifierType\": {\"name\": \"{{modifierCount}}\u00d7 {{pokeballName}}\"}}}";
    else if (!strcmp(file, "pokeball"))
        t = "{\"pokeBall\": \"Pok\u00e9 Ball\", \"greatBall\": \"Great Ball\", \"ultraBall\": \"Ultra Ball\", "
            "\"rogueBall\": \"Rogue Ball\", \"masterBall\": \"Master Ball\"}";
    if (t) *len = strlen(t);
    return t;
}

static void test_pokeballs(void)
{
    static const struct { const char *key; int ball; int add; const char *text; } balls[] = {
        {"POKEBALL", POKEBALL_TYPE_POKEBALL, 5, "You received\n5\xc3\x97 Pok\xc3\xa9 Ball!"},
        {"GREAT_BALL", POKEBALL_TYPE_GREAT_BALL, 5, "You received\n5\xc3\x97 Great Ball!"},
        {"ULTRA_BALL", POKEBALL_TYPE_ULTRA_BALL, 5, "You received\n5\xc3\x97 Ultra Ball!"},
        {"ROGUE_BALL", POKEBALL_TYPE_ROGUE_BALL, 5, "You received\n5\xc3\x97 Rogue Ball!"},
        {"MASTER_BALL", POKEBALL_TYPE_MASTER_BALL, 1, "You received\n1\xc3\x97 Master Ball!"},
    };
    static const int starts[] = {0, 1, 93, 94, 95, 97, 98, 99};
    static const pr_text_source src = {poke_locale, NULL, "en"};
    int cases = 0;
    for (int b = 0; b < 5; b++) {
        for (size_t c = 0; c < sizeof starts / sizeof starts[0]; c++) {
            const int want = starts[c] + balls[b].add > 99 ? 99 : starts[c] + balls[b].add;
            for (int path = 0; path < 4; path++) {
                /* 0: taken from the reward screen free, 1: bought, 2: given by ModifierRewardPhase (with its text) */
                setup("reward-apply-pokeballs", 1);
                for (int k = 0; k < 8; k++) run.pokeballs[k] = (uint8_t)(10 + k);
                run.pokeballs[balls[b].ball] = (uint8_t)starts[c];
                uint8_t others[8];
                memcpy(others, run.pokeballs, sizeof others);
                drain();
                gc_event e;
                if (path < 2) {
                    const result r = take(balls[b].key, path ? 400 : 0, -1, -1);
                    CHECKF(r.ok, "%s taken from %d (%s)", balls[b].key, starts[c], path ? "bought" : "free");
                    CHECK(!gc_run_next_event(&run, &e));   /* addModifier shows no text */
                } else if (path == 2) {
                    gc_reward_gain(&run, balls[b].key);
                    CHECK(gc_run_next_event(&run, &e) && e.kind == GC_EV_MESSAGE);
                    CHECK(!strcmp(e.msg.key, "battle:rewardGain"));
                    CHECK(e.msg.n_args == 3 && !strcmp(e.msg.args[0].name, "modifierName") && e.msg.args[0].kind == PR_ARG_TEMPLATE);
                    CHECK(!strcmp(e.msg.args[1].name, "modifierCount") && e.msg.args[1].kind == PR_ARG_INT && (int)e.msg.args[1].v == balls[b].add);
                    CHECK(!strcmp(e.msg.args[2].name, "pokeballName") && e.msg.args[2].kind == PR_ARG_KEY);
                    char text[128];
                    pr_msg_format(&e.msg, &src, text, sizeof text);
                    CHECKF(!strcmp(text, balls[b].text), "text '%s', want '%s'", text, balls[b].text);
                    CHECK(!gc_run_next_event(&run, &e));
                } else {
                    gc_port_give_reward(&run, balls[b].key);   /* the state change alone, as the Mystery Encounters call it */
                    CHECK(!gc_run_next_event(&run, &e));
                }
                CHECKF(run.pokeballs[balls[b].ball] == want, "%s from %d: %d, want %d (path %d)", balls[b].key, starts[c],
                       run.pokeballs[balls[b].ball], want, path);
                for (int k = 0; k < 8; k++)
                    if (k != balls[b].ball) CHECK(run.pokeballs[k] == others[k]);
                cases++;
            }
        }
    }
    printf("pokeballs: %d cases ok (5 balls x 8 starting counts x free, bought, given with text, given plain)\n", cases);
    CHECK(!gc_reward_add_pokeballs(&run, "POTION"));
    CHECK(!gc_reward_add_pokeballs(&run, "GOLDEN_POKEBALL"));
}

/* ---- reward-core has no application for these: game-core's own code keeps them ---- */
static void unhandled_items(void)
{
    setup("reward-apply-unhandled", 3);
    const char *why = "";
    static const char *const others[] = {"LEFTOVERS", "EXP_SHARE"};
    for (size_t i = 0; i < sizeof others / sizeof others[0]; i++) {
        const gc_offer o = offer(others[i], 0);
        CHECK(gc_reward_apply(&run, &o, 0, 0, &why) == GC_APPLY_UNHANDLED);
    }
    /* a Poke Ball is added to the bag, not applied */
    const int balls = run.pokeballs[POKEBALL_TYPE_POKEBALL];
    const result r = take("POKEBALL", 0, -1, -1);
    CHECK(r.ok && run.pokeballs[POKEBALL_TYPE_POKEBALL] == balls + 5);
}

/* ---- Full Heal, and the cure inside Full Restore: the status and its counters, the confusion and Nightmare tags, and
 * the one text upstream queues (battlerTags:confusedOnRemove, when a confusion ends). Worked out from 8555c08:
 * PokemonStatusHealModifierType's filter is !hp || (!status && !getTag(CONFUSED)); PokemonStatusHealModifier.apply and
 * PokemonHpRestoreModifier.apply (healStatus) call resetStatus(true, true, false, false), which clears the status
 * (counters with it), lapses Nightmare only if the status was sleep, and lapses the confusion. ---- */
static void add_tag(pr_pokemon *b, int type)
{
    CHECK(b->n_tags < PR_MAX_BATTLER_TAGS);
    pr_battler_tag t;
    memset(&t, 0, sizeof t);
    t.type = (uint8_t)type;
    t.turn_count = 3;
    t.source_id = -1;
    t.source_index = -1;
    b->tags[b->n_tags++] = t;
}
static bool has_tag(int storage, int type) { return pr_get_battler_tag(&mon(storage)->b, type) != NULL; }

/* the message events the last call queued: n returned, `msgs` filled up to 4; HP events are counted in *hp_events */
static int messages(gc_event msgs[4], int *hp_events)
{
    gc_event e;
    int n = 0;
    *hp_events = 0;
    while (gc_run_next_event(&run, &e)) {
        if (e.kind == GC_EV_HP) (*hp_events)++;
        else if (e.kind == GC_EV_MESSAGE) { if (n < 4) msgs[n] = e; n++; }
        else CHECKF(0, "event kind %d", e.kind);
    }
    return n;
}

static void test_full_heal(void)
{
    gc_event msgs[4];
    int hp_events;
    result r;

    /* a rotated party order: the member at party index 1 is storage 3, so a text naming the wrong member shows */
    setup("full-heal-tags", 4);
    run.party_order[0] = 2; run.party_order[1] = 3; run.party_order[2] = 0; run.party_order[3] = 1;
    const int tgt = 1, st = run.party_order[tgt];
    pr_pokemon *b = &mon(st)->b;
    b->hp = 100;
    drain();

    /* confusion alone is enough (the filter reads the tag too), and the cure says so: one text, naming the target */
    add_tag(b, BATTLER_TAG_TYPE_CONFUSED);
    r = take("FULL_HEAL", 0, st, 0);
    CHECK(r.ok && b->hp == 100 && b->status == STATUS_EFFECT_NONE && !has_tag(st, BATTLER_TAG_TYPE_CONFUSED) && b->n_tags == 0);
    CHECK(messages(msgs, &hp_events) == 1 && hp_events == 1);
    CHECK(!strcmp(msgs[0].msg.key, "battlerTags:confusedOnRemove"));
    CHECK(msgs[0].msg.args[0].kind == PR_ARG_POKEMON && !strcmp(msgs[0].msg.args[0].name, "pokemonNameWithAffix"));
    CHECK((int)(msgs[0].msg.args[0].v >> 16) == b->species);
    for (int i = 0; i < 4; i++) if (i != st) CHECK((int)(msgs[0].msg.args[0].v >> 16) != mon(i)->b.species);
    /* again: nothing left to cure, refused, no event */
    r = take("FULL_HEAL", 0, st, 0);
    CHECK(!r.ok && !strcmp(r.why, NO_EFFECT));
    CHECK(messages(msgs, &hp_events) == 0 && hp_events == 0);

    /* a status and a confusion: both go, one text */
    b->status = STATUS_EFFECT_POISON;
    add_tag(b, BATTLER_TAG_TYPE_CONFUSED);
    r = take("FULL_HEAL", 0, st, 0);
    CHECK(r.ok && b->status == STATUS_EFFECT_NONE && b->n_tags == 0);
    CHECK(messages(msgs, &hp_events) == 1 && !strcmp(msgs[0].msg.key, "battlerTags:confusedOnRemove"));

    /* a status alone: no text at all (upstream calls neither queueMessage nor ui.showText for it) */
    b->status = STATUS_EFFECT_BURN;
    r = take("FULL_HEAL", 0, st, 0);
    CHECK(r.ok && b->status == STATUS_EFFECT_NONE);
    CHECK(messages(msgs, &hp_events) == 0 && hp_events == 1);

    /* sleep: the counter goes back to "undefined" (-1), and the Nightmare lapses with the sleep, the confusion too */
    b->status = STATUS_EFFECT_SLEEP;
    b->status_sleep_turns = 3;
    add_tag(b, BATTLER_TAG_TYPE_NIGHTMARE);
    add_tag(b, BATTLER_TAG_TYPE_CONFUSED);
    r = take("FULL_HEAL", 0, st, 0);
    CHECK(r.ok && b->status == STATUS_EFFECT_NONE && b->status_sleep_turns == -1 && b->status_toxic_turns == 0 && b->status_freeze_turns == -1);
    CHECK(b->n_tags == 0);
    CHECK(messages(msgs, &hp_events) == 1);   /* Nightmare lapses without a text */

    /* the toxic counter and the freeze counter */
    b->status = STATUS_EFFECT_TOXIC;
    b->status_toxic_turns = 4;
    r = take("FULL_HEAL", 0, st, 0);
    CHECK(r.ok && b->status == STATUS_EFFECT_NONE && b->status_toxic_turns == 0 && b->status_sleep_turns == -1 && b->status_freeze_turns == -1);
    drain();
    b->status = STATUS_EFFECT_FREEZE;
    b->status_freeze_turns = 2;
    r = take("FULL_HEAL", 0, st, 0);
    CHECK(r.ok && b->status_freeze_turns == -1);
    drain();

    /* Nightmare without the sleep stays, and is no reason to take the item (the filter reads the status and the confusion only) */
    add_tag(b, BATTLER_TAG_TYPE_NIGHTMARE);
    r = take("FULL_HEAL", 0, st, 0);
    CHECK(!r.ok && !strcmp(r.why, NO_EFFECT) && has_tag(st, BATTLER_TAG_TYPE_NIGHTMARE));
    b->status = STATUS_EFFECT_PARALYSIS;
    r = take("FULL_HEAL", 0, st, 0);
    CHECK(r.ok && b->status == STATUS_EFFECT_NONE && has_tag(st, BATTLER_TAG_TYPE_NIGHTMARE));
    CHECK(messages(msgs, &hp_events) == 0);

    /* tags the cure does not touch stay, in order, and nobody else changes */
    add_tag(b, BATTLER_TAG_TYPE_CONFUSED);
    add_tag(b, BATTLER_TAG_TYPE_TAUNT);
    b->status = STATUS_EFFECT_POISON;
    pr_pokemon *o = &mon(0)->b;
    o->status = STATUS_EFFECT_BURN;
    add_tag(o, BATTLER_TAG_TYPE_CONFUSED);
    drain();
    r = take("FULL_HEAL", 0, st, 0);
    CHECK(r.ok && b->n_tags == 2 && b->tags[0].type == BATTLER_TAG_TYPE_NIGHTMARE && b->tags[1].type == BATTLER_TAG_TYPE_TAUNT);
    CHECK(o->status == STATUS_EFFECT_BURN && has_tag(0, BATTLER_TAG_TYPE_CONFUSED));
    CHECK(messages(msgs, &hp_events) == 1);

    /* a bought Full Heal that the filter refuses keeps the screen (refused, nothing happens) */
    b->status = STATUS_EFFECT_NONE;
    drain();
    r = take("FULL_HEAL", 300, st, 0);
    CHECK(!r.ok && !strcmp(r.why, NO_EFFECT));
    CHECK(messages(msgs, &hp_events) == 0 && hp_events == 0);
    /* and one that cures a confusion alone says so just the same */
    add_tag(b, BATTLER_TAG_TYPE_CONFUSED);
    r = take("FULL_HEAL", 300, st, 0);
    CHECK(r.ok && messages(msgs, &hp_events) == 1);

    /* Full Restore at full HP: the confusion alone is a reason to take it (PokemonHpRestoreModifierType's filter), the cure is
     * the same, a healthy full-HP member is still refused */
    setup("full-restore-confusion", 3);
    b = &mon(1)->b;
    drain();
    r = take("FULL_RESTORE", 0, 1, 0);
    CHECK(!r.ok && !strcmp(r.why, NO_EFFECT));
    add_tag(b, BATTLER_TAG_TYPE_CONFUSED);
    drain();
    r = take("FULL_RESTORE", 0, 1, 0);
    CHECK(r.ok && b->hp == 300 && b->n_tags == 0);
    CHECK(messages(msgs, &hp_events) == 1 && !strcmp(msgs[0].msg.key, "battlerTags:confusedOnRemove"));
    /* below full HP, with a confusion and a toxic counter: healed, cured, one text */
    b->hp = 40;
    b->status = STATUS_EFFECT_TOXIC;
    b->status_toxic_turns = 3;
    add_tag(b, BATTLER_TAG_TYPE_CONFUSED);
    drain();
    r = take("FULL_RESTORE", 0, 1, 0);
    CHECK(r.ok && b->hp == 300 && b->status == STATUS_EFFECT_NONE && b->status_toxic_turns == 0 && b->n_tags == 0);
    CHECK(messages(msgs, &hp_events) == 1);
    /* a Potion heals and leaves the status, the counters and the tags alone */
    b->hp = 40;
    b->status = STATUS_EFFECT_TOXIC;
    b->status_toxic_turns = 3;
    add_tag(b, BATTLER_TAG_TYPE_CONFUSED);
    drain();
    r = take("POTION", 0, 1, 0);
    CHECK(r.ok && b->hp > 40 && b->status == STATUS_EFFECT_TOXIC && b->status_toxic_turns == 3 && b->n_tags == 1);
    CHECK(messages(msgs, &hp_events) == 0);

    /* a Revive leaves the counters at their cleared values (a fainted Pokemon has no tags upstream) */
    faint(2);
    mon(2)->b.status_toxic_turns = 0;
    r = take("REVIVE", 0, 2, 0);
    CHECK(r.ok && mon(2)->b.status == STATUS_EFFECT_NONE && mon(2)->b.status_sleep_turns == -1 && mon(2)->b.status_freeze_turns == -1);
    drain();

    printf("full heal: status, counters, confusion and Nightmare cures and their text: ok\n");
}

/* ---- the same through the real flow: confused in battle, the wave won, Full Heal on the reward screen, the next wave's
 * command screen, a save and a load. The confusion is added by battle-core's own addTag, so this also shows that the tag
 * is still on the lead when the reward screen opens (upstream keeps summonData until the next battle's ReturnPhase) ---- */
static void test_full_heal_flow(void)
{
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, "full-heal-flow", 7);
    gc_starter team = {.species = SPECIES_ID_BULBASAUR, .tera_type = -1};
    for (int j = 0; j < 6; j++) team.ivs[j] = 15;
    const char *why;
    CHECK(gc_run_set_starters(&run, &team, 1, &why));
    run.sys = &sys;
    drain();
    CHECK(run.screen.kind == GC_SCREEN_COMMAND);
    gc_mon *lead = mon(0);
    run.enemy[0].b.hp = 1;
    CHECK(pr_add_tag(&run.bt, &run.eng, &lead->b, BATTLER_TAG_TYPE_CONFUSED, 5, MOVE_ID_CONFUSE_RAY, -1));
    CHECK(pr_get_battler_tag(&lead->b, BATTLER_TAG_TYPE_CONFUSED));
    drain();

    /* win the wave (a confused lead can hit itself, so give it a few turns) */
    for (int turn = 0; turn < 8 && run.screen.kind == GC_SCREEN_COMMAND; turn++) { flow_fight(&run); drain(); }
    CHECK(run.screen.kind == GC_SCREEN_REWARD);
    const int wave = run.wave;
    CHECK(pr_get_battler_tag(&lead->b, BATTLER_TAG_TYPE_CONFUSED) != NULL);   /* the tag came through the victory */

    /* Full Heal on the lead: the tag goes, the one text is the first thing shown, then the next wave */
    run.screen.options[0] = offer("FULL_HEAL", 0);
    gc_choice pick = {GC_CH_REWARD, 0, 0, -1};
    CHECK(gc_run_choose(&run, &pick, &why));
    CHECK(lead->b.n_tags == 0 && lead->b.status == STATUS_EFFECT_NONE);
    gc_event e;
    int texts = 0;
    bool first_text_is_cure = false;
    while (gc_run_next_event(&run, &e))
        if (e.kind == GC_EV_MESSAGE) {
            if (!texts) first_text_is_cure = !strcmp(e.msg.key, "battlerTags:confusedOnRemove");
            texts += !strcmp(e.msg.key, "battlerTags:confusedOnRemove");
        }
    CHECK(texts == 1 && first_text_is_cure);
    for (int step = 0; step < 20 && run.screen.kind == GC_SCREEN_CHECK_SWITCH; step++) {
        gc_choice no = {GC_CH_CONFIRM, 0, 0, 0};
        CHECK(gc_run_choose(&run, &no, &why));
        drain();
    }
    CHECK(run.screen.kind == GC_SCREEN_COMMAND && run.wave == wave + 1 && gc_run_can_save(&run));
    CHECK(lead->b.n_tags == 0);   /* the next battle's lead keeps what is left: nothing */
    static uint8_t buf[GC_SAVE_MAX];
    const size_t n = gc_run_save(&run, buf, sizeof buf);
    CHECK(n > 0);
    memset(&flow_loaded, 0, sizeof flow_loaded);
    CHECK(gc_run_load(&flow_loaded, buf, n, &why) && !flow_loaded.load_dropped);
    flow_loaded.sys = &sys;
    CHECK(flow_loaded.party[0].b.n_tags == 0 && flow_loaded.party[0].b.status == STATUS_EFFECT_NONE);
    printf("full heal flow: confused in battle -> reward -> Full Heal -> text -> next wave -> save -> load ok\n");
}

/* a slot in the moveset range with no move (reward-core's moveset leaves empty slots out) is no move */
static void empty_slot_is_no_move(void)
{
    setup("reward-apply-empty-slot", 2);
    pr_pokemon *b = &mon(0)->b;
    b->moves[2] = 0;
    b->n_moves = 3;
    b->pp_used[0] = b->pp_used[1] = 5;
    result r = take("ETHER", 0, 0, 2);
    CHECK(!r.ok && !strcmp(r.why, "pick a move"));
    r = take("PP_UP", 0, 0, 3);
    CHECK(!r.ok && !strcmp(r.why, "pick a move"));
    /* with a gap before the last move, reward-core's index and the slot differ: the right move is changed */
    b->moves[1] = 0;
    b->moves[2] = MOVE_ID_EMBER;
    b->pp_used[0] = 6; b->pp_used[2] = 6;
    r = take("ETHER", 0, 0, 2);
    CHECK(r.ok && b->pp_used[2] == 0 && b->pp_used[0] == 6);
    r = take("PP_UP", 0, 0, 1);
    CHECK(!r.ok && !strcmp(r.why, "pick a move"));
}

/* ---- PP Up and a fixed (overridden) maximum PP: PokemonPpUpModifierType's moveSelectFilter ---- */
static void max_pp_override_blocks_pp_up(void)
{
    setup("reward-apply-override", 2);
    pr_pokemon *b = &mon(0)->b;
    b->max_pp_override[0] = 10;
    const result r = take("PP_UP", 0, 0, 0);
    CHECK(!r.ok && !strcmp(r.why, NO_EFFECT) && b->pp_up[0] == 0);
    const result r2 = take("PP_MAX", 0, 0, 0);
    CHECK(!r2.ok && b->pp_up[0] == 0);
    /* another slot of the same Pokemon is fine, and Ether isn't affected by an override */
    CHECK(take("PP_UP", 0, 0, 1).ok && b->pp_up[1] == 1);
    b->pp_used[0] = 4;
    CHECK(take("ETHER", 0, 0, 0).ok && b->pp_used[0] == 0);
}

/* a move with fewer than 5 base PP can't take a PP Up */
static void low_pp_move_refuses_pp_up(void)
{
    setup("reward-apply-lowpp", 2);
    int low = -1;
    for (int i = 1; i < PR_MOVE_COUNT && low < 0; i++)
        if (!pr_moves[i].unimplemented && pr_moves[i].pp > 0 && pr_moves[i].pp < 5) low = i;
    CHECK(low > 0);
    gc_mon_set_move(mon(0), 1, low);
    mon(0)->b.n_moves = 2;
    const result r = take("PP_UP", 0, 0, 1);
    CHECK(!r.ok && !strcmp(r.why, NO_EFFECT) && mon(0)->b.pp_up[1] == 0);
    CHECK(take("PP_UP", 0, 0, 0).ok && mon(0)->b.pp_up[0] == 1);
}

/* ---- 2. random states against a second implementation of upstream's rules ---- */
static uint64_t rs = 0x9E3779B97F4A7C15ull;
static uint32_t rnd(uint32_t n)
{
    rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17;
    return (uint32_t)((rs >> 11) % n);
}

typedef struct { int hp, max_hp, status; int pp_used[4], pp_up[4], n_moves, base_pp[4], override[4]; } mmon;

/* PokemonHpRestoreModifier.apply's amount: restorePoints x multiplier (not for a revive), restorePercent of
 * max HP rounded down, the larger of the two, at least 1 */
static int hp_gain(int points, int percent, double mult, bool fainted_item, int max_hp)
{
    int pts = fainted_item ? points : (int)floor(points * mult);
    int pct = (int)floor(percent * 0.01 * max_hp);
    int g = pct > pts ? pct : pts;
    return g < 1 ? 1 : g;
}

static double healing_booster(int stacks) { return 1 + (1.1 - 1) * stacks; }

static const struct {
    const char *key;
    int kind;   /* 0 potion family, 1 revive, 2 ether, 3 elixir, 4 pp up, 5 sacred ash */
    int points, percent;
    bool heal_status;
} items[] = {
    {"POTION", 0, 20, 10, false}, {"SUPER_POTION", 0, 50, 25, false}, {"HYPER_POTION", 0, 200, 50, false},
    {"MAX_POTION", 0, 0, 100, false}, {"FULL_RESTORE", 0, 0, 100, true}, {"REVIVE", 1, 0, 50, false},
    {"MAX_REVIVE", 1, 0, 100, false}, {"ETHER", 2, 10, 0, false}, {"MAX_ETHER", 2, -1, 0, false},
    {"ELIXIR", 3, 10, 0, false}, {"MAX_ELIXIR", 3, -1, 0, false}, {"PP_UP", 4, 1, 0, false}, {"PP_MAX", 4, 3, 0, false},
    {"SACRED_ASH", 5, 0, 100, false},
};
#define N_ITEMS ((int)(sizeof items / sizeof items[0]))
static long taken_n[N_ITEMS], refused_n[N_ITEMS], consumed_n[N_ITEMS];

/* the model: returns true = taken. `tgt` is the party[] index, `slot` the moveset slot */
static bool model(int item, mmon *party, int n, int tgt, int slot, int charm_stacks, bool purchase, bool *no_target_msg)
{
    const int kind = items[item].kind;
    *no_target_msg = false;
    if (kind == 5) {
        bool any = false;
        for (int i = 0; i < n; i++) any |= party[i].hp <= 0;
        if (!any) return !purchase;   /* addModifier is false; a free reward ends the phase anyway */
        for (int i = 0; i < n; i++)
            if (party[i].hp <= 0) { party[i].hp = party[i].max_hp; party[i].status = STATUS_EFFECT_NONE; }
        return true;
    }
    if (tgt < 0 || tgt >= n) { *no_target_msg = true; return false; }
    mmon *m = &party[tgt];
    switch (kind) {
    case 0:   /* selectFilter, then apply */
        if (m->hp <= 0 || (m->hp >= m->max_hp && (!items[item].heal_status || m->status == STATUS_EFFECT_NONE))) return false;
        {
            const int g = hp_gain(items[item].points, items[item].percent, healing_booster(charm_stacks), false, m->max_hp);
            if (items[item].heal_status) m->status = STATUS_EFFECT_NONE;
            m->hp = m->hp + g > m->max_hp ? m->max_hp : m->hp + g;
        }
        return true;
    case 1:
        if (m->hp > 0) return false;
        {
            const int g = hp_gain(0, items[item].percent, 1, true, m->max_hp);
            m->status = STATUS_EFFECT_NONE;
            m->hp = m->hp + g > m->max_hp ? m->max_hp : m->hp + g;
        }
        return true;
    case 2: case 4:
        if (slot < 0 || slot >= m->n_moves) { *no_target_msg = true; return false; }   /* "pick a move" */
        if (kind == 2) {
            if (!m->pp_used[slot]) return false;
            const int pts = items[item].points;
            m->pp_used[slot] = pts > -1 ? (m->pp_used[slot] - pts > 0 ? m->pp_used[slot] - pts : 0) : 0;
        } else {
            if (m->base_pp[slot] < 5 || m->pp_up[slot] >= 3 || m->override[slot] > 0) return false;
            m->pp_up[slot] = m->pp_up[slot] + items[item].points > 3 ? 3 : m->pp_up[slot] + items[item].points;
        }
        return true;
    default: {   /* elixir */
        bool any = false;
        for (int s = 0; s < m->n_moves; s++) any |= m->pp_used[s] != 0;
        if (!any) return false;
        const int pts = items[item].points;
        for (int s = 0; s < m->n_moves; s++)
            m->pp_used[s] = pts > -1 ? (m->pp_used[s] - pts > 0 ? m->pp_used[s] - pts : 0) : 0;
        return true;
    }
    }
}

static void random_cases(int count)
{
    static const int pool_moves[] = {MOVE_ID_TACKLE, MOVE_ID_GROWL, MOVE_ID_EMBER, MOVE_ID_SCRATCH, MOVE_ID_SPLASH, MOVE_ID_SURF,
                                     MOVE_ID_PROTECT, MOVE_ID_HYPER_BEAM, MOVE_ID_EXPLOSION, MOVE_ID_STRUGGLE, MOVE_ID_CALM_MIND,
                                     MOVE_ID_EARTHQUAKE};
    static const int statuses[] = {STATUS_EFFECT_NONE, STATUS_EFFECT_POISON, STATUS_EFFECT_TOXIC, STATUS_EFFECT_PARALYSIS,
                                   STATUS_EFFECT_SLEEP, STATUS_EFFECT_FREEZE, STATUS_EFFECT_BURN};
    long total_taken = 0, total_refused = 0;
    for (int c = 0; c < count; c++) {
        const int n = 1 + (int)rnd(6);
        setup("reward-apply-random", n);
        /* upstream's party order: a random permutation */
        for (int i = n - 1; i > 0; i--) {
            const int j = (int)rnd((uint32_t)i + 1);
            const int8_t t = run.party_order[i]; run.party_order[i] = run.party_order[j]; run.party_order[j] = t;
        }
        mmon party[6];
        memset(party, 0, sizeof party);
        for (int i = 0; i < n; i++) {
            gc_mon *g = mon(i);
            pr_pokemon *b = &g->b;
            mmon *m = &party[i];
            m->n_moves = 1 + (int)rnd(4);
            for (int s = 0; s < m->n_moves; s++) {
                gc_mon_set_move(g, s, pool_moves[rnd(sizeof pool_moves / sizeof pool_moves[0])]);
                m->base_pp[s] = pr_moves[b->moves[s]].pp;
                m->pp_up[s] = (int)rnd(4);
                b->pp_up[s] = (uint8_t)m->pp_up[s];
                if (rnd(25) == 0) { b->max_pp_override[s] = (int16_t)(5 + rnd(30)); m->override[s] = b->max_pp_override[s]; }
                else m->override[s] = -1;
                const int cap = gc_move_max_pp(b, s);
                m->pp_used[s] = rnd(3) ? (int)rnd((uint32_t)cap + 1) : 0;
                b->pp_used[s] = (uint8_t)m->pp_used[s];
            }
            b->n_moves = (uint8_t)m->n_moves;
            for (int s = m->n_moves; s < 4; s++) { b->moves[s] = 0; b->pp_used[s] = b->pp_up[s] = 0; }
            m->max_hp = 1 + (int)rnd(rnd(3) ? 700 : 4);
            switch (rnd(5)) {
            case 0: m->hp = 0; break;
            case 1: m->hp = m->max_hp; break;
            default: m->hp = 1 + (int)rnd((uint32_t)m->max_hp); break;
            }
            m->status = m->hp ? statuses[rnd(sizeof statuses / sizeof statuses[0])] : STATUS_EFFECT_FAINT;
            b->stats[0] = m->max_hp;
            b->hp = m->hp;
            b->status = (uint8_t)m->status;
        }
        const int stacks = rnd(3) ? (int)rnd(6) : 0;
        charm(stacks);
        drain();
        const int item = (int)rnd(N_ITEMS);
        const bool purchase = rnd(4) == 0;
        /* mostly a valid target, now and then a bad one */
        int tgt = rnd(10) ? (int)rnd((uint32_t)n) : (int)rnd(9) - 1;
        int slot = rnd(10) ? (int)rnd(4) : (int)rnd(7) - 1;
        if (items[item].kind == 5 && rnd(2)) tgt = -1;
        mmon expect[6];
        memcpy(expect, party, sizeof party);
        bool bad_target;
        const bool want = model(item, expect, n, tgt, slot, stacks, purchase, &bad_target);
        result r = take(items[item].key, purchase ? 100 + rnd(900) : 0, tgt, slot);
        CHECKF(r.ok == want, "case %d %s: taken %d, model says %d (target %d slot %d purchase %d, hp %d/%d status %d)", c,
               items[item].key, r.ok, want, tgt, slot, purchase,
               tgt >= 0 && tgt < n ? party[tgt].hp : -1, tgt >= 0 && tgt < n ? party[tgt].max_hp : -1, tgt >= 0 && tgt < n ? party[tgt].status : -1);
        if (!want) {
            const char *expect_why = bad_target ? (items[item].kind == 2 || items[item].kind == 4 ? (tgt < 0 || tgt >= n ? "pick a Pokemon" : "pick a move") : "pick a Pokemon") : NO_EFFECT;
            CHECKF(!strcmp(r.why, expect_why), "case %d %s: why '%s', model says '%s'", c, items[item].key, r.why, expect_why);
        }
        for (int i = 0; i < n; i++) {
            const pr_pokemon *b = &mon(i)->b;
            const mmon *e = &expect[i];
            CHECKF(b->hp == e->hp && b->status == e->status, "case %d %s: member %d hp %d status %d, model %d %d", c, items[item].key, i, b->hp, b->status, e->hp, e->status);
            for (int s = 0; s < e->n_moves; s++)
                CHECKF(b->pp_used[s] == e->pp_used[s] && b->pp_up[s] == e->pp_up[s], "case %d %s: member %d move %d pp_used %d pp_up %d, model %d %d",
                       c, items[item].key, i, s, b->pp_used[s], b->pp_up[s], e->pp_used[s], e->pp_up[s]);
            CHECK(b->n_moves == e->n_moves);
        }
        if (want) {
            taken_n[item]++;
            total_taken++;
            if (items[item].kind == 5 && !purchase) {
                bool any = false;
                for (int i = 0; i < n; i++) any |= party[i].hp <= 0;
                consumed_n[item] += !any;
            }
        } else {
            refused_n[item]++;
            total_refused++;
        }
        CHECK(run.n_items == (stacks > 0));
    }
    printf("random states: %d cases, %ld taken, %ld refused, all as the model says\n", count, total_taken, total_refused);
    printf("%-14s %8s %8s\n", "item", "taken", "refused");
    for (int i = 0; i < N_ITEMS; i++) {
        printf("%-14s %8ld %8ld", items[i].key, taken_n[i], refused_n[i]);
        if (consumed_n[i]) printf("   (%ld free, with nobody fainted: taken, no effect)", consumed_n[i]);
        printf("\n");
    }
}

static void test_tm_and_memory_mushroom(void)
{
    setup("tm-memory-test", 1);
    gc_mon *m = mon(0);
    m->b.level = 50;
    gc_mon_recalc(&run, m);

    /* TM application */
    const char *why = NULL;
    gc_offer tm_offer = offer("TM_COMMON", 0);
    tm_offer.arg = MOVE_ID_FLAMETHROWER;
    int res = gc_reward_apply(&run, &tm_offer, 0, 0, &why);
    CHECK(res == GC_APPLY_DONE);
    CHECK(m->n_used_tms == 1);
    CHECK(m->used_tms[0] == MOVE_ID_FLAMETHROWER);

    gc_internal *in = gc_int(&run);
    gc_rphase queued_p;
    CHECK(gc_rq_next(&in->rq, &queued_p) && queued_p.type == RP_SELECT_MODIFIER);
    CHECK(gc_rq_next(&in->rq, &queued_p) && queued_p.type == RP_LEARN_MOVE && queued_p.b == MOVE_ID_FLAMETHROWER);
    gc_rq_clear(&run);

    /* duplicate TM application doesn't duplicate in used_tms */
    res = gc_reward_apply(&run, &tm_offer, 0, 0, &why);
    CHECK(res == GC_APPLY_DONE);
    CHECK(m->n_used_tms == 1);
    gc_rq_clear(&run);

    /* Memory Mushroom application on mon level 50 with relearnable moves */
    pr_reward_ctx *ctx = gc_reward_ctx(&run);
    static pr_reward_memory_move moves[PR_REWARD_MAX_LEVEL_MOVES + 4 + PR_REWARD_MAX_USED_TMS];
    const int n_moves = pr_reward_memory_moves(ctx, 0, moves, (int)(sizeof moves / sizeof moves[0]));
    CHECK(n_moves >= 2);

    gc_offer mm_offer = offer("MEMORY_MUSHROOM", 0);

    /* Memory Mushroom with no move chosen or invalid slot: refused */
    res = gc_reward_apply(&run, &mm_offer, 0, -1, &why);
    CHECK(res == GC_APPLY_REFUSED && !strcmp(why, "pick a move"));

    res = gc_reward_apply(&run, &mm_offer, 0, n_moves, &why);
    CHECK(res == GC_APPLY_REFUSED && !strcmp(why, "pick a move"));

    /* Memory Mushroom applying slot 0 asserts moves[0].move is learned */
    res = gc_reward_apply(&run, &mm_offer, 0, 0, &why);
    CHECK(res == GC_APPLY_DONE);
    CHECK(gc_rq_next(&in->rq, &queued_p) && queued_p.type == RP_SELECT_MODIFIER);
    CHECK(gc_rq_next(&in->rq, &queued_p) && queued_p.type == RP_LEARN_MOVE && queued_p.b == moves[0].move);
    gc_rq_clear(&run);

    /* Memory Mushroom applying slot 1 asserts moves[1].move is learned */
    res = gc_reward_apply(&run, &mm_offer, 0, 1, &why);
    CHECK(res == GC_APPLY_DONE);
    CHECK(gc_rq_next(&in->rq, &queued_p) && queued_p.type == RP_SELECT_MODIFIER);
    CHECK(gc_rq_next(&in->rq, &queued_p) && queued_p.type == RP_LEARN_MOVE && queued_p.b == moves[1].move);
    gc_rq_clear(&run);

    /* Invalid party member for TM or Memory Mushroom */
    res = gc_reward_apply(&run, &tm_offer, -1, 0, &why);
    CHECK(res == GC_APPLY_REFUSED && !strcmp(why, "pick a Pokemon"));

    res = gc_reward_apply(&run, &mm_offer, 5, 0, &why);
    CHECK(res == GC_APPLY_REFUSED && !strcmp(why, "pick a Pokemon"));
}

int main(void)
{
    for (int i = 0; i < N_TABLE; i++) run_case(&table[i]);
    printf("table: %d cases ok\n", N_TABLE);
    sacred_ash();
    shuffled_party_order();
    test_pokeballs();
    unhandled_items();
    test_full_heal();
    test_full_heal_flow();
    empty_slot_is_no_move();
    max_pp_override_blocks_pp_up();
    low_pp_move_refuses_pp_up();
    test_tm_and_memory_mushroom();
    test_rare_and_rarer_candy_table();
    test_candy_level_up_save_reload_resume();
    printf("sacred ash, party order, Poke Balls, unhandled items, empty slot, max PP override, low-PP move, tm/memory, rare/rarer candy: ok\n");
    random_cases(30000);
    printf("reward apply: %ld checks, 0 failures\n", checks);
    return 0;
}
