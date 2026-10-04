/* The reward screen's adapter (src/gc_reward.c): each way SelectModifierPhase comes up (first roll, reroll with
 * and without the Lock Capsule, the copy after a TM, the no-victory screen) must hand reward-core exactly what
 * game-core holds and give back exactly what it rolled, on the same RNG. Each roll is replayed straight
 * through reward-core from the same RNG state and compared (options, arguments, tiers, upgrade counts, costs,
 * RNG state, thresholds). tests/reward_diff_test.c compares the adapter's inputs to upstream's. */
#include "../src/internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;
static int checks;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "reward FAIL line %d: %s\n", __LINE__, #x); exit(1); } checks++; } while (0)

static bool same_rng(const pr_rng *a, const pr_rng *b)
{
    return a->c == b->c && a->s0 == b->s0 && a->s1 == b->s1 && a->s2 == b->s2 && a->n == b->n;
}

static bool same_offer(const gc_offer *g, const pr_reward_option *o)
{
    return !strcmp(g->key, pr_item_keys[o->type.item]) && g->arg == o->type.arg && g->tier == o->tier
           && g->cost == o->cost && g->upgrade_count == o->upgrade_count;
}

static void setup(const char *seed, uint32_t entropy, const int *species, int n, int wave, int level)
{
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, entropy);
    gc_starter team[6];
    memset(team, 0, sizeof team);
    for (int i = 0; i < n; i++) {
        team[i].species = (uint16_t)species[i];
        team[i].tera_type = -1;
        for (int k = 0; k < 6; k++) team[i].ivs[k] = 15;
    }
    const char *why;
    CHECK(gc_run_set_starters(&run, team, n, &why));
    run.sys = &sys;
    run.wave = wave;
    for (int i = 0; i < run.n_party; i++) {
        run.party[i].b.level = (uint8_t)level;
        gc_mon_recalc(&run, &run.party[i]);
    }
}

static const int TEAM3[] = {SPECIES_ID_BULBASAUR, SPECIES_ID_CHARMANDER, SPECIES_ID_PIKACHU};

/* What reward-core says for the same screen from the same RNG: the adapter's own call replayed by hand */
typedef struct {
    int n;
    pr_reward_option o[PR_REWARD_MAX_OPTIONS];
    pr_rng rng;
    pr_reward_thresholds th;
} replay;

static replay direct(const pr_rng *before, const pr_reward_select *sel)
{
    replay r;
    memset(&r, 0, sizeof r);
    pr_reward_ctx *ctx = gc_reward_ctx(&run);
    run.run.rng = *before;
    r.n = pr_reward_select_start(ctx, sel, &r.th, r.o);
    r.rng = run.run.rng;
    return r;
}

static void expect_replay(const replay *r, const gc_offer *g, int n, const pr_rng *adapter_rng)
{
    gc_internal *in = gc_int(&run);
    CHECK(r->n == n);
    for (int i = 0; i < n; i++) CHECK(same_offer(&g[i], &r->o[i]));
    CHECK(same_rng(&r->rng, adapter_rng));
    CHECK(!memcmp(&r->th, &in->reward.th, sizeof r->th));
}

static int roll(int kind, gc_offer out[GC_MAX_OPTIONS], double *cost)
{
    return gc_port_reward_options(&run, kind, out, cost);
}

/* ---- a first roll is resetSeed + thresholds + count + options ---- */
static void test_first_roll(void)
{
    int luck_upgrades = 0, rolls = 0;
    for (int i = 0; i < 60; i++) {
        setup("rewardfirst", 100 + (uint32_t)i, TEAM3, 1 + i % 3, 5 + i * 3, 5 + i);
        if (i % 4 == 0) { run.party[0].b.shiny = true; run.party[0].variant = 2; run.party[0].luck = 3; }
        if (i % 5 == 0) gc_item_add(&run, "GOLDEN_POKEBALL", -1, 0, 1);
        if (i % 7 == 0) gc_item_add(&run, "LEFTOVERS", -1, run.party[0].b.id, 1);
        const unsigned unported = run.run.unported;
        const pr_rng before = run.run.rng;
        gc_offer out[GC_MAX_OPTIONS];
        double cost;
        const int n = roll(GC_REWARD_FIRST, out, &cost);
        const pr_rng after = run.run.rng;
        CHECK(n > 0 && run.run.unported == unported && run.stubs[GC_STUB_REWARDS] == 0);
        CHECK(n == 3 + (i % 5 == 0));   /* the Golden Poke Ball adds a fourth */
        pr_reward_select sel = {0};
        const replay r = direct(&before, &sel);
        expect_replay(&r, out, n, &after);
        for (int k = 0; k < n; k++) luck_upgrades += out[k].upgrade_count > 0;
        rolls += n;
        /* resetSeed(): the options don't depend on the RNG the screen is entered with */
        run.run.rng = (pr_rng){0.5, 0.25, 0.125, 0.0625, 3};
        gc_offer again[GC_MAX_OPTIONS];
        const int m = roll(GC_REWARD_FIRST, again, &cost);
        CHECK(m == n);
        for (int k = 0; k < n; k++) CHECK(!strcmp(again[k].key, out[k].key) && again[k].arg == out[k].arg && again[k].tier == out[k].tier);
        CHECK(same_rng(&run.run.rng, &after));
    }
    CHECK(luck_upgrades > 0 && rolls >= 180);   /* a lucky party's luck upgrades come through */
    printf("first roll: ok (%d options replayed, %d luck upgrades)\n", rolls, luck_upgrades);
}

/* ---- a reroll is a new phase: rerollCount + 1, no reset, the old tiers only with the Lock Capsule's lock ---- */
static void test_reroll(void)
{
    int locked = 0, same_tiers = 0;
    for (int i = 0; i < 60; i++) {
        setup("rewardreroll", 500 + (uint32_t)i, TEAM3, 3, 10 + i * 2, 12 + i);
        gc_internal *in = gc_int(&run);
        gc_offer a[GC_MAX_OPTIONS], b[GC_MAX_OPTIONS], c[GC_MAX_OPTIONS];
        double cost;
        const int na = roll(GC_REWARD_FIRST, a, &cost);
        CHECK(na == 3 && in->reroll_count == 0);
        /* without the lock */
        in->lock_rarities = false;
        pr_rng before = run.run.rng;
        const int nb = roll(GC_REWARD_REROLL, b, &cost);
        pr_rng after = run.run.rng;
        CHECK(nb == 3 && in->reroll_count == 1 && !in->reward.has_custom && !in->reward.is_copy);
        pr_reward_select sel = {0};
        sel.reroll_count = 1;
        replay r = direct(&before, &sel);
        expect_replay(&r, b, nb, &after);
        /* with the lock: the previous options' tiers */
        in->lock_rarities = true;
        int8_t tiers[PR_REWARD_MAX_OPTIONS];
        const int nt = pr_reward_get_locked_tiers(in->reward.options, in->reward.n_options, tiers);
        CHECK(nt == nb);
        before = run.run.rng;
        const int nc = roll(GC_REWARD_REROLL, c, &cost);
        after = run.run.rng;
        CHECK(nc == 3 && in->reroll_count == 2);
        memset(&sel, 0, sizeof sel);
        sel.reroll_count = 2;
        sel.locked_tiers = tiers;
        sel.n_locked_tiers = nt;
        r = direct(&before, &sel);
        expect_replay(&r, c, nc, &after);
        locked++;
        /* the locked tier is the base tier: a luck upgrade still raises the option above it */
        same_tiers += c[0].tier - c[0].upgrade_count == b[0].tier && c[1].tier - c[1].upgrade_count == b[1].tier
                      && c[2].tier - c[2].upgrade_count == b[2].tier;
    }
    CHECK(same_tiers == locked);   /* the Lock Capsule keeps every base tier */
    printf("reroll: ok (%d rerolls with and without the lock replayed)\n", locked * 2);
}

/* ---- the price: SelectModifierPhase.getRerollCost ---- */
static void test_reroll_cost(void)
{
    for (int i = 0; i < 30; i++) {
        setup("rewardcost", 900 + (uint32_t)i, TEAM3, 2, 1 + i * 6, 10);
        gc_internal *in = gc_int(&run);
        gc_offer o[GC_MAX_OPTIONS];
        double cost;
        for (int lock = 0; lock < 2; lock++) {
            in->lock_rarities = lock;
            roll(GC_REWARD_FIRST, o, &cost);
            for (int count = 0; count < 3; count++) {
                int tiers[PR_REWARD_MAX_OPTIONS];
                for (int k = 0; k < 3; k++) tiers[k] = o[k].tier;
                const double expected = pr_run_get_reroll_cost(run.wave, lock, tiers, 3, count, NAN, 1.0);
                CHECK(cost == expected);
                CHECK(gc_reward_reroll_cost(&run) == expected);
                CHECK(expected > 0);
                roll(GC_REWARD_REROLL, o, &cost);
            }
        }
    }
    /* the Black Sludge makes the shop dearer; a screen with rerollMultiplier -1 has no reroll */
    setup("rewardcost", 1, TEAM3, 1, 20, 10);
    gc_offer o[GC_MAX_OPTIONS];
    double plain, sludge, none;
    roll(GC_REWARD_FIRST, o, &plain);
    gc_item_add(&run, "MYSTERY_ENCOUNTER_BLACK_SLUDGE", -1, 0, 1);
    roll(GC_REWARD_FIRST, o, &sludge);
    CHECK(sludge > plain);
    CHECK(roll(GC_REWARD_RESTRICTED, o, &none) == 0 && none == -1);
    printf("reroll cost: ok\n");
}

/* ---- the screen again after a TM or the Memory Mushroom: copy() ---- */
static void test_copy(void)
{
    for (int i = 0; i < 40; i++) {
        setup("rewardcopy", 1300 + (uint32_t)i, TEAM3, 3, 10 + i * 3, 20);
        gc_internal *in = gc_int(&run);
        in->lock_rarities = i % 2;
        gc_offer a[GC_MAX_OPTIONS], b[GC_MAX_OPTIONS], c[GC_MAX_OPTIONS];
        double cost_a, cost_b, cost_c;
        roll(GC_REWARD_FIRST, a, &cost_a);
        const int rerolls = i % 3;
        int n = 3;
        for (int k = 0; k < rerolls; k++) { n = roll(GC_REWARD_REROLL, a, &cost_a); }
        CHECK(in->reroll_count == rerolls);
        gc_reward_queue_copy(&run);
        const pr_reward_thresholds th = in->reward.th;
        const pr_rng before = run.run.rng;
        const unsigned unported = run.run.unported;
        const int nc = roll(GC_REWARD_COPY, b, &cost_b);
        /* the same options, no roll, no new thresholds, the same price and rerollCount */
        CHECK(nc == n && same_rng(&run.run.rng, &before) && !memcmp(&th, &in->reward.th, sizeof th));
        CHECK(run.run.unported == unported && in->reroll_count == rerolls && in->reward.is_copy);
        for (int k = 0; k < n; k++)
            CHECK(!strcmp(a[k].key, b[k].key) && a[k].arg == b[k].arg && a[k].tier == b[k].tier
                  && a[k].upgrade_count == b[k].upgrade_count && a[k].target == b[k].target);
        CHECK(cost_a == cost_b);
        /* the copy of a copy */
        gc_reward_queue_copy(&run);
        CHECK(roll(GC_REWARD_COPY, c, &cost_c) == n && cost_c == cost_a && same_rng(&run.run.rng, &before));
        for (int k = 0; k < n; k++) CHECK(!strcmp(a[k].key, c[k].key) && a[k].arg == c[k].arg);
        /* a reroll from a copy is a plain new phase */
        if (cost_c >= 0) {
            pr_reward_select sel = {0};
            sel.reroll_count = rerolls + 1;
            int8_t tiers[PR_REWARD_MAX_OPTIONS];
            if (in->lock_rarities) {
                sel.n_locked_tiers = pr_reward_get_locked_tiers(in->reward.options, in->reward.n_options, tiers);
                sel.locked_tiers = tiers;
            }
            const pr_rng from = run.run.rng;
            gc_offer d[GC_MAX_OPTIONS];
            double cost_d;
            const int nd = roll(GC_REWARD_REROLL, d, &cost_d);
            const pr_rng to = run.run.rng;
            const replay r = direct(&from, &sel);
            expect_replay(&r, d, nd, &to);
            CHECK(!in->reward.is_copy && !in->reward.has_custom);
        }
    }
    printf("copy: ok\n");
}

/* ---- the no-victory screen: no options, no reroll, but the seed is reset and the thresholds made ---- */
static void test_restricted(void)
{
    for (int i = 0; i < 20; i++) {
        setup("rewardrestricted", 1700 + (uint32_t)i, TEAM3, 2, 10 + i, 10);
        gc_internal *in = gc_int(&run);
        gc_offer o[GC_MAX_OPTIONS];
        double cost;
        const pr_rng before = run.run.rng;
        const int n = roll(GC_REWARD_RESTRICTED, o, &cost);
        const pr_rng after = run.run.rng;
        CHECK(n == 0 && cost == -1 && in->reward.has_custom && run.stubs[GC_STUB_REWARDS] == 0);
        pr_reward_custom custom;
        memset(&custom, 0, sizeof custom);
        custom.reroll_multiplier = -1;
        custom.allow_luck_upgrades = true;
        pr_reward_select sel = {0};
        sel.custom = &custom;
        const replay r = direct(&before, &sel);
        expect_replay(&r, o, n, &after);
        CHECK(!same_rng(&before, &after));
        /* the screen shown again (a copy) is still empty and has no reroll */
        gc_reward_queue_copy(&run);
        CHECK(roll(GC_REWARD_COPY, o, &cost) == 0 && cost == -1);
    }
    printf("no-victory screen: ok\n");
}

/* ---- through the run's screens: the phase, the Lock Capsule's switch, the paid reroll, the copy ---- */
static void test_phase_flow(void)
{
    setup("rewardflow", 2100, TEAM3, 3, 30, 25);
    gc_internal *in = gc_int(&run);
    run.in_battle = false;
    run.money = 100000;
    gc_rq_unshift(&run, gc_rp(RP_SELECT_MODIFIER, 0, 0, 0));
    in->rq.waiting = false;
    gc_run_until_screen(&run);
    const gc_screen *s = gc_run_screen(&run);
    CHECK(s->kind == GC_SCREEN_REWARD && s->n_options == 3);
    const double first_cost = s->reroll_cost;
    CHECK(first_cost == pr_run_get_reroll_cost(30, false, (int[]){s->options[0].tier, s->options[1].tier, s->options[2].tier}, 3, 0, NAN, 1.0));
    gc_offer first[3];
    memcpy(first, s->options, sizeof first);

    const char *why = NULL;
    gc_choice lock = {GC_CH_LOCK_RARITIES, 0, 0, 0};
    CHECK(!gc_run_choose(&run, &lock, &why) && why);   /* no Lock Capsule */
    gc_item_add(&run, "LOCK_CAPSULE", -1, 0, 1);
    s = gc_run_screen(&run);
    CHECK(gc_run_choose(&run, &lock, &why) && in->lock_rarities);
    s = gc_run_screen(&run);
    CHECK(s->kind == GC_SCREEN_REWARD && s->n_options == 3 && !memcmp(s->options, first, sizeof first));
    CHECK(s->reroll_cost != first_cost && s->reroll_cost == gc_reward_reroll_cost(&run));
    const double lock_cost = s->reroll_cost;
    const double money = run.money;

    gc_choice reroll = {GC_CH_REROLL, 0, 0, 0};
    CHECK(gc_run_choose(&run, &reroll, &why));
    s = gc_run_screen(&run);
    CHECK(run.money == money - lock_cost && in->reroll_count == 1);
    CHECK(s->kind == GC_SCREEN_REWARD && s->n_options == 3);
    for (int k = 0; k < 3; k++) CHECK(s->options[k].tier - s->options[k].upgrade_count == first[k].tier);   /* locked tiers */
    CHECK(s->reroll_cost == 2 * lock_cost);
    gc_offer second[3];
    memcpy(second, s->options, sizeof second);

    /* switch the lock off: the price is the plain one again, for rerollCount 1 */
    CHECK(gc_run_choose(&run, &lock, &why) && !in->lock_rarities);
    s = gc_run_screen(&run);
    CHECK(s->reroll_cost == pr_run_get_reroll_cost(30, false, (int[]){0}, 3, 1, NAN, 1.0));

    /* a TM or the Memory Mushroom taken: the screen again, the same options */
    gc_reward_queue_copy(&run);
    gc_choice skip = {GC_CH_SKIP, 0, 0, 0};
    const pr_rng rng = run.run.rng;
    const double plain_cost = s->reroll_cost;
    CHECK(gc_run_choose(&run, &skip, &why));
    s = gc_run_screen(&run);
    CHECK(s->kind == GC_SCREEN_REWARD && s->n_options == 3 && !memcmp(s->options, second, sizeof second));
    CHECK(s->reroll_cost == plain_cost && same_rng(&run.run.rng, &rng) && in->reroll_count == 1);
    CHECK(run.stubs[GC_STUB_REWARDS] == 0);
    CHECK(gc_run_choose(&run, &skip, &why));
    CHECK(gc_run_screen(&run)->kind != GC_SCREEN_REWARD);

    /* the no-victory screen through the queue: empty, and it cannot be rerolled or locked */
    setup("rewardflow", 2101, TEAM3, 3, 30, 25);
    in = gc_int(&run);
    run.money = 100000;
    gc_item_add(&run, "LOCK_CAPSULE", -1, 0, 1);
    gc_rq_unshift(&run, gc_rp(RP_SELECT_MODIFIER, 1, 0, 0));
    in->rq.waiting = false;
    gc_run_until_screen(&run);
    s = gc_run_screen(&run);
    CHECK(s->kind == GC_SCREEN_REWARD && s->n_options == 0 && s->reroll_cost < 0);
    CHECK(!gc_run_choose(&run, &reroll, &why));
    CHECK(!gc_run_choose(&run, &lock, &why));
    printf("phase flow: ok\n");
}

/* ---- what the adapter hands reward-core ---- */
static void test_inputs(void)
{
    setup("rewardinputs", 3100, TEAM3, 3, 40, 30);
    gc_internal *in = gc_int(&run);
    (void)in;
    const uint32_t id1 = run.party[1].b.id;
    gc_item_add(&run, "GOLDEN_PUNCH", -1, id1, 1);
    gc_item_add(&run, "AMULET_COIN", -1, 0, 2);
    gc_item_add(&run, "LEFTOVERS", -1, run.party[2].b.id, 3);
    run.pokeballs[0] = 7;
    run.pokeballs[4] = 2;
    run.party[0].b.shiny = true; run.party[0].variant = 1; run.party[0].luck = 2;
    run.party[1].b.shiny = true; run.party[1].variant = 2; run.party[1].luck = 3;
    run.party[2].pause_evolutions = true;
    const int order[3] = {2, 0, 1};
    for (int k = 0; k < 3; k++) run.party_order[k] = (int8_t)order[k];

    pr_reward_ctx *c = gc_reward_ctx(&run);
    CHECK(c->n_party == 3 && c->wave == 40 && c->luck == gc_party_luck(&run) && c->luck == 5);
    for (int k = 0; k < 3; k++) CHECK(c->party[k].id == run.party[order[k]].b.id);   /* upstream's party order */
    CHECK(c->party[0].pause_evolutions && !c->party[1].pause_evolutions);
    CHECK(c->pokeball_counts[0] == 7 && c->pokeball_counts[4] == 2 && c->biome == run.biome);
    CHECK(c->n_items == 3);
    int punch = -1, coin = -1, left = -1;
    for (int i = 0; i < c->n_items; i++) {
        const char *key = pr_item_keys[c->items[i].item];
        if (!strcmp(key, "GOLDEN_PUNCH")) punch = i;
        if (!strcmp(key, "AMULET_COIN")) coin = i;
        if (!strcmp(key, "LEFTOVERS")) left = i;
    }
    CHECK(punch >= 0 && coin >= 0 && left >= 0);
    CHECK(c->items[punch].owner == 2 && c->items[coin].owner == -1 && c->items[left].owner == 0);   /* ctx indexes */
    CHECK(c->items[left].stack == 3 && c->items[coin].stack == 2);
    /* the persistent treasure: its Golden Punches and the run's Amulet Coin stacks (an item entry counts once) */
    CHECK(c->party[2].persistent_treasure == 1 + 1 && c->party[0].persistent_treasure == 1);

    /* the types an attack counts as for the type-boosting items */
    int8_t t[2];
    gc_mon *m = &run.party[0];
    m->b.species = SPECIES_ID_MORPEKO; m->b.form = 0;
    CHECK(gc_types_for_item_spawn(&run, m, MOVE_ID_AURA_WHEEL, t) == 2 && t[0] == POKEMON_TYPE_DARK && t[1] == POKEMON_TYPE_ELECTRIC);
    m->b.species = SPECIES_ID_BULBASAUR;
    CHECK(gc_types_for_item_spawn(&run, m, MOVE_ID_TACKLE, t) == 1 && t[0] == POKEMON_TYPE_NORMAL);
    m->b.tera_type = POKEMON_TYPE_FIRE;
    CHECK(gc_types_for_item_spawn(&run, m, MOVE_ID_TERA_BLAST, t) == 1 && t[0] == POKEMON_TYPE_NORMAL);   /* no Tera Orb */
    gc_item_add(&run, "TERA_ORB", -1, 0, 1);
    CHECK(gc_types_for_item_spawn(&run, m, MOVE_ID_TERA_BLAST, t) == 2 && t[0] == POKEMON_TYPE_NORMAL && t[1] == POKEMON_TYPE_FIRE);
    m->b.tera_type = POKEMON_TYPE_NORMAL;
    CHECK(gc_types_for_item_spawn(&run, m, MOVE_ID_TERA_BLAST, t) == 1);
    m->b.ability = ABILITY_ID_PIXILATE;
    CHECK(gc_types_for_item_spawn(&run, m, MOVE_ID_TACKLE, t) == 1 && t[0] == POKEMON_TYPE_FAIRY);   /* MoveTypeChangeAbAttr */
    m->b.hp = 0;
    CHECK(gc_types_for_item_spawn(&run, m, MOVE_ID_TACKLE, t) == 1 && t[0] == POKEMON_TYPE_FAIRY);   /* even fainted */

    /* the learnsets: TM list = level moves incl. evolution/relearn ones up to its level; egg moves are the unlocked bits */
    gc_mon *b = &run.party[0];
    b->b.species = SPECIES_ID_BULBASAUR;
    b->b.level = 30;
    b->b.hp = b->b.stats[STAT_HP];
    gc_learn_row rows[PR_REWARD_MAX_LEVEL_MOVES];
    const int n = gc_level_moves(b, 30, true, false, true, rows, PR_REWARD_MAX_LEVEL_MOVES);
    CHECK(n > 4);
    for (int i = 1; i < n; i++) CHECK(rows[i - 1].level <= rows[i].level);   /* sorted by level */
    for (int i = 0; i < n; i++) {
        CHECK(rows[i].level <= 30);
        for (int j = i + 1; j < n; j++) CHECK(rows[i].move != rows[j].move);   /* unique */
    }
    bool vine = false;
    for (int i = 0; i < n; i++) vine |= rows[i].move == MOVE_ID_VINE_WHIP;
    CHECK(vine);
    uint16_t egg[4];
    CHECK(gc_unlocked_egg_moves(&run, b, egg) == 0);
    sys.starters[SPECIES_ID_BULBASAUR].egg_moves = 0x5;
    CHECK(gc_unlocked_egg_moves(&run, b, egg) == 2);
    b->b.species = SPECIES_ID_IVYSAUR;   /* an evolved starter keeps its root's egg moves */
    CHECK(gc_unlocked_egg_moves(&run, b, egg) == 2);
    printf("inputs: ok\n");
}

int main(void)
{
    test_first_roll();
    test_reroll();
    test_reroll_cost();
    test_copy();
    test_restricted();
    test_phase_flow();
    test_inputs();
    printf("reward adapter: %d checks, 0 failures\n", checks);
    return 0;
}
