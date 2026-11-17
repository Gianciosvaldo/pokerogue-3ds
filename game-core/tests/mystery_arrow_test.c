/* The Stand Arrow (src/gc_arrow.c) on the host: offered only in runs the app's pillar hook calls Bizarre, once per run; the first option pierces one of
 * the player's Pokemon (60% it awakens and two IVs rise like Training Session's light training, 30% nothing, 10% it leaves for good with its items),
 * the second is a boss fight with an Ultra, Ultra, Great shop, the third leaves with the heal-only shop. Custom content: there is no upstream to compare
 * with, so the odds, the IV rule and the side effects are asserted from the specification above, with the boundaries pinned. */
#include "../src/internal.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static gc_run run;
static gc_system sys;
static bool offer = true;

static bool pillar(void *user, const struct gc_run *r, int which, gc_custom_team *team)
{
    (void)user; (void)r; (void)team;
    return which < 0 && offer;
}
static const gc_custom_hooks hooks = {.pillar = pillar};

static void start(const char *seed, bool with_hook, int n_party)
{
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    if (with_hook) gc_run_set_custom(&run, &hooks);
    gc_system_init_defaults(&sys);
    run.sys = &sys;
    run.wave = 45;
    run.biome = BIOME_ID_PLAINS;
    run.n_items = 0;
    run.n_party = n_party;
    for (int i = 0; i < n_party; i++) {
        gc_starter s;
        memset(&s, 0, sizeof s);
        s.species = (uint16_t)(i == 0 ? SPECIES_ID_PIKACHU : i == 1 ? SPECIES_ID_LAPRAS : SPECIES_ID_GENGAR);
        s.tera_type = -1;
        for (int k = 0; k < 6; k++) s.ivs[k] = 15;
        s.moves[0] = MOVE_ID_TACKLE;
        gc_mon_new_player(&run, &run.party[i], &s, 40);
        run.party_order[i] = (int8_t)i;
    }
    run.money = 1000;
    run.battle_type = BATTLE_TYPE_WILD;
    gc_items_to_battle(&run);
    gc_rq_reset(&gc_int(&run)->rq);
    run.mystery_type = GC_ME_STAND_ARROW;
    gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_STAND_ARROW, GC_ME_GREAT, 0));
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_MYSTERY);
}

static bool queue_has(int type, int a)
{
    gc_rqueue copy = gc_int(&run)->rq;
    gc_rphase p;
    while (gc_rq_next(&copy, &p)) if (p.type == type && (a < 0 || p.a == a)) return true;
    return false;
}

static int said(const char *key)
{
    int n = 0;
    gc_event e;
    while (gc_run_next_event(&run, &e)) if (e.kind == GC_EV_MESSAGE && e.msg.key && !strcmp(e.msg.key, key)) n++;
    return n;
}

/* how often each of the keys was said (drains the events) */
static void tally(const char *const *keys, int n, int *out)
{
    gc_event e;
    for (int i = 0; i < n; i++) out[i] = 0;
    while (gc_run_next_event(&run, &e))
        if (e.kind == GC_EV_MESSAGE && e.msg.key)
            for (int i = 0; i < n; i++) out[i] += !strcmp(e.msg.key, keys[i]);
}

static int iv_sum(const gc_mon *m)
{
    int s = 0;
    for (int i = 0; i < 6; i++) s += m->b.ivs[i];
    return s;
}

static void test_offer(void)
{
    /* never without the hook (a plain Classic run); with it, it is drawn; a hook that says no takes it away again */
    int without = 0, with = 0;
    for (int i = 0; i < 300; i++) {
        char seed[16];
        snprintf(seed, sizeof seed, "sa-%d", i);
        int t, tier;
        start(seed, false, 3);
        run.wave = 20 + i % 100;
        pr_run_reset_seed(&run.run, run.wave);
        assert(gc_mystery_select(&run, &t, &tier));
        without += t == GC_ME_STAND_ARROW;
        start(seed, true, 3);
        run.wave = 20 + i % 100;
        pr_run_reset_seed(&run.run, run.wave);
        assert(gc_mystery_select(&run, &t, &tier));
        with += t == GC_ME_STAND_ARROW;
    }
    assert(without == 0 && with > 0);
    start("sa-off", true, 3);
    offer = false;
    for (int i = 0; i < 100; i++) {
        int t, tier;
        run.wave = 20 + i;
        pr_run_reset_seed(&run.run, run.wave);
        assert(gc_mystery_select(&run, &t, &tier) && t != GC_ME_STAND_ARROW);
    }
    offer = true;
    /* once per run: seen already, then never again */
    start("sa-once", true, 3);
    run.n_mystery_history = 0;
    gc_mystery_record(&run, GC_ME_STAND_ARROW, GC_ME_GREAT);
    gc_mystery_record(&run, GC_ME_DARK_DEAL, GC_ME_ROGUE);   /* the last one is another, so only the cap can refuse it */
    for (int i = 0; i < 200; i++) {
        int t, tier;
        run.wave = 20 + i % 100;
        pr_run_reset_seed(&run.run, run.wave);
        assert(gc_mystery_select(&run, &t, &tier) && t != GC_ME_STAND_ARROW);
    }
    /* a party of one never meets it (the rule's minimum is two Pokemon allowed in battle) */
    start("sa-solo", true, 1);
    for (int i = 0; i < 200; i++) {
        int t, tier;
        run.wave = 20 + i % 100;
        pr_run_reset_seed(&run.run, run.wave);
        assert(gc_mystery_select(&run, &t, &tier) && t != GC_ME_STAND_ARROW);
    }
    printf("ok offer (picked %d of 300 in Bizarre runs, none without the hook, none after one, none with a party of one)\n", with);
}

static void test_boundaries(void)
{
    /* the three results by the roll, at every edge: 0..59 awaken, 60..89 nothing, 90..99 reject */
    for (int r = 0; r < 100; r++) assert(gc_arrow_result(r) == (r < 60 ? 0 : r < 90 ? 1 : 2));
    assert(gc_arrow_awaken_percent() == 60 && gc_arrow_lose_percent() == 10);
    puts("ok roll boundaries (59/60 and 89/90) and the odds the texts show");
}

static void test_screen(void)
{
    start("sa-screen", true, 3);
    assert(run.screen.n_options == 3 && run.screen.option_ok[0] && run.screen.option_ok[1] && run.screen.option_ok[2]);
    assert(run.screen.option_pick[0] == GC_PICK_MEMBER && run.screen.option_pick[1] == GC_PICK_NONE && run.screen.option_pick[2] == GC_PICK_NONE);
    bool awaken = false, lose = false;
    for (int i = 0; i < run.screen.n_me_args; i++) {
        if (!strcmp(run.screen.me_args[i].name, "awakenPercent")) awaken = run.screen.me_args[i].v == 60;
        if (!strcmp(run.screen.me_args[i].name, "losePercent")) lose = run.screen.me_args[i].v == 10;
    }
    assert(awaken && lose);
    for (int i = 0; i < 3; i++) assert(gc_mystery_pick_ok(&run, GC_ME_STAND_ARROW, 0, i));
    assert(!gc_mystery_pick_ok(&run, GC_ME_STAND_ARROW, 0, 3) && !gc_mystery_pick_ok(&run, GC_ME_STAND_ARROW, 0, -1));
    run.party[1].b.hp = 0;   /* a fainted Pokemon cannot be pierced */
    assert(!gc_mystery_pick_ok(&run, GC_ME_STAND_ARROW, 0, 1) && gc_mystery_pick_ok(&run, GC_ME_STAND_ARROW, 0, 2));
    /* the first option needs a second Pokemon able to fight on: a party of two with one fainted, or of one, has it shut */
    start("sa-two", true, 2);
    assert(run.screen.option_ok[0]);
    start("sa-two-b", true, 2);
    run.party[1].b.hp = 0;
    gc_mystery_setup_screen(&run);
    assert(!run.screen.option_ok[0] && run.screen.option_ok[1] && run.screen.option_ok[2]);
    start("sa-one", true, 1);
    assert(!run.screen.option_ok[0] && run.screen.option_ok[1] && run.screen.option_ok[2]);
    puts("ok screen (3 options, the party screen on the first, the odds as tokens, the first one shut without a second fighter)");
}

/* what the pierce did to party member 0 of a 3-Pokemon party */
enum { R_AWAKEN, R_NOTHING, R_REJECT };

static void test_pierce_odds(void)
{
    int count[3] = {0, 0, 0};
    const int N = 1500;
    for (int i = 0; i < N; i++) {
        char seed[16];
        snprintf(seed, sizeof seed, "sa-odds-%d", i);
        start(seed, true, 3);
        const int sum = iv_sum(&run.party[0]);
        const char *why = "";
        bool ok = false;
        assert(gc_mystery_execute(&run, 0, 0, 0, &why, &ok) && ok);
        if (run.n_party == 2) {
            count[R_REJECT]++;
            assert(run.party[0].b.species != SPECIES_ID_PIKACHU || run.party[1].b.species != SPECIES_ID_PIKACHU);
        } else if (iv_sum(&run.party[0]) > sum) {
            count[R_AWAKEN]++;
        } else {
            count[R_NOTHING]++;
            assert(iv_sum(&run.party[0]) == sum);
        }
        assert(queue_has(RP_SELECT_MODIFIER, 1));   /* every result leaves with the heal-only shop */
    }
    const double pa = (double)count[R_AWAKEN] / N, pn = (double)count[R_NOTHING] / N, pr = (double)count[R_REJECT] / N;
    assert(pa > 0.55 && pa < 0.65);
    assert(pn > 0.25 && pn < 0.35);
    assert(pr > 0.06 && pr < 0.14);
    printf("ok odds over %d seeds: awakens %.3f, nothing %.3f, rejects %.3f\n", N, pa, pn, pr);
}

static void test_awaken_rule(void)
{
    /* every old IV value: the two that rise get +10 up to 10, +5 up to 20, else +3 (and 1 gets one more), none above 31, the rest stay */
    int checked = 0;
    for (int i = 0; i < 400; i++) {
        char seed[16];
        snprintf(seed, sizeof seed, "sa-iv-%d", i);
        start(seed, true, 3);
        static const uint8_t spread[6] = {0, 1, 10, 11, 21, 30};
        uint8_t before[6];
        for (int k = 0; k < 6; k++) before[k] = run.party[0].b.ivs[k] = spread[(k + i) % 6];
        const char *why = "";
        bool ok = false;
        assert(gc_mystery_execute(&run, 0, 0, 0, &why, &ok) && ok);
        if (run.n_party != 3 || iv_sum(&run.party[0]) == 0) continue;
        int changed = 0;
        for (int k = 0; k < 6; k++) {
            const int old = before[k], now = run.party[0].b.ivs[k];
            if (now == old) continue;
            changed++;
            int expect = old + (old <= 10 ? 10 : old <= 20 ? 5 : 3) + (old == 1 ? 1 : 0);
            if (expect > 31) expect = 31;
            assert(now == expect && now <= 31);
        }
        if (changed) {
            assert(changed == 2);
            checked++;
            static const char *const k[3] = {"mysteryEncounters/standArrow:result.awakens", "mysteryEncounters/standArrow:result.stats", "mysteryEncounters/standArrow:result.statsOne"};
            int n[3];
            tally(k, 3, n);
            assert(n[0] == 1 && n[1] == 1 && n[2] == 0);
        }
    }
    assert(checked > 100);
    /* only one IV below 31: one rises; none: nothing rises and the third text is said */
    for (int i = 0; i < 400; i++) {
        char seed[16];
        snprintf(seed, sizeof seed, "sa-one-%d", i);
        start(seed, true, 3);
        for (int k = 0; k < 6; k++) run.party[0].b.ivs[k] = 31;
        run.party[0].b.ivs[2] = 29;
        const char *why = "";
        bool ok = false;
        assert(gc_mystery_execute(&run, 0, 0, 0, &why, &ok) && ok);
        if (run.n_party != 3) continue;
        const int one = said("mysteryEncounters/standArrow:result.statsOne");
        if (one) { assert(run.party[0].b.ivs[2] == 31 && iv_sum(&run.party[0]) == 5 * 31 + 31); break; }
    }
    int maxed = 0;
    for (int i = 0; i < 400 && !maxed; i++) {
        char seed[16];
        snprintf(seed, sizeof seed, "sa-max-%d", i);
        start(seed, true, 3);
        for (int k = 0; k < 6; k++) run.party[0].b.ivs[k] = 31;
        const char *why = "";
        bool ok = false;
        assert(gc_mystery_execute(&run, 0, 0, 0, &why, &ok) && ok);
        if (run.n_party != 3) continue;
        maxed = said("mysteryEncounters/standArrow:result.statsNone");
        assert(iv_sum(&run.party[0]) == 6 * 31);
    }
    assert(maxed);
    printf("ok awakening: %d runs checked against the IV rule, the one-IV and all-31 texts\n", checked);
}

static void test_reject(void)
{
    /* a rejected Pokemon is gone and takes its items; the others keep theirs */
    int rejected = 0;
    for (int i = 0; i < 300 && rejected < 5; i++) {
        char seed[16];
        snprintf(seed, sizeof seed, "sa-rej-%d", i);
        start(seed, true, 3);
        gc_item_add(&run, "LEFTOVERS", -1, run.party[1].b.id, 1);
        gc_item_add(&run, "LEFTOVERS", -1, run.party[0].b.id, 1);
        const uint32_t gone_id = run.party[1].b.id, kept_id = run.party[0].b.id;
        const char *why = "";
        bool ok = false;
        assert(gc_mystery_execute(&run, 0, 1, 0, &why, &ok) && ok);
        if (run.n_party != 2) continue;
        rejected++;
        for (int k = 0; k < run.n_party; k++) assert(run.party[k].b.id != gone_id);
        int held_by_gone = 0, held_by_kept = 0;
        for (int k = 0; k < run.n_items; k++) {
            held_by_gone += run.items[k].pokemon_id == gone_id;
            held_by_kept += run.items[k].pokemon_id == kept_id;
        }
        assert(held_by_gone == 0 && held_by_kept == 1);
        static const char *const k[3] = {"mysteryEncounters/standArrow:result.rejects", "mysteryEncounters/standArrow:result.gone", "mysteryEncounters/standArrow:result.awakens"};
        int n[3];
        tally(k, 3, n);
        assert(n[0] == 1 && n[1] == 1 && n[2] == 0);
    }
    assert(rejected == 5);
    /* a fainted pick (the screen would refuse it) takes the first Pokemon that can be pierced */
    start("sa-refused", true, 3);
    run.party[1].b.hp = 0;
    const char *why = "";
    bool ok = false;
    assert(gc_mystery_execute(&run, 0, 1, 0, &why, &ok) && ok);
    assert(run.party[1].b.hp == 0 || run.n_party == 2);
    puts("ok rejection (the Pokemon and its items are gone, the others keep theirs)");
}

static void test_wild_and_leave(void)
{
    start("sa-wild", true, 3);
    const char *why = "";
    bool ok = false;
    assert(gc_mystery_execute(&run, 1, 0, 0, &why, &ok) && ok);
    assert(run.battle_type == BATTLE_TYPE_WILD && run.n_enemy == 1 && run.enemy[0].b.is_boss);
    assert(run.in_battle && gc_rq_has(&run, RP_TURN_INIT));
    assert(gc_int(&run)->me_no_flee && run.bt.me_no_flee);   /* the boss fight cannot be fled */
    const double money = run.money;
    gc_rq_clear(&run);
    gc_me_win_rewards(&run);
    const pr_reward_custom *c = &gc_int(&run)->reward.me_custom;
    assert(c->fill_remaining && c->n_tiers == 3 && c->tiers[0] == MODIFIER_TIER_ULTRA && c->tiers[1] == MODIFIER_TIER_ULTRA && c->tiers[2] == MODIFIER_TIER_GREAT);
    assert(queue_has(RP_BATTLE_END, 1) && queue_has(RP_SELECT_MODIFIER, 2) && queue_has(RP_NEW_BATTLE, -1) && run.money == money);

    start("sa-leave", true, 3);
    const int items = run.n_items, party = run.n_party;
    assert(gc_mystery_execute(&run, 2, 0, 0, &why, &ok) && ok);
    assert(run.battle_type != BATTLE_TYPE_TRAINER && run.n_enemy == 0 + run.n_enemy);
    assert(queue_has(RP_SELECT_MODIFIER, 1) && run.n_party == party && run.n_items == items);
    puts("ok the wild option (boss fight, Ultra, Ultra, Great) and leaving (heal-only shop, nothing changed)");
}

int main(void)
{
    test_boundaries();
    test_offer();
    test_screen();
    test_pierce_odds();
    test_awaken_rule();
    test_reject();
    test_wild_and_leave();
    puts("mystery_arrow_test: all passed");
    return 0;
}
