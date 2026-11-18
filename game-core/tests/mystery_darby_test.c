/* D'Arby the Gambler's hand rules (src/gc_darby.c) against lorenzo's spec (2026-11-17): the max-STAB score with the spec's own example, the
 * base-Speed tie-break and the coin, and the cheat: never when the fair card wins or ties, only to a strict winner, to the largest margin
 * (ties by base Speed), 75% of the time. */
#include "../src/internal.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void seed_rng(pr_rng *r, unsigned n)
{
    char t[24];
    pr_seedstr s;
    snprintf(t, sizeof t, "darby-%u", n);
    pr_seed_from_ascii(&s, t);
    pr_rng_sow(r, &s);
}

static gc_darby_card card(int species) { gc_darby_card c = {(uint16_t)species, 0}; return c; }

static void test_score(void)
{
    const gc_darby_card charizard = card(SPECIES_ID_CHARIZARD), swampert = card(SPECIES_ID_SWAMPERT);
    /* the spec's example: Charizard's best against Swampert is Flying 1x (Fire 0.5x); Swampert's is Water 2x (Ground 0x) */
    assert(gc_darby_score4(&charizard, &swampert) == 4);
    assert(gc_darby_score4(&swampert, &charizard) == 8);
    assert(gc_darby_compare(&swampert, &charizard) == 1 && gc_darby_compare(&charizard, &swampert) == -1);
    /* 4x and 0x and 0.25x come out exact: Grass/Steel? use Ferrothorn vs Charizard (Fire 4x on Grass/Steel), Ghost against Normal 0x */
    const gc_darby_card ferrothorn = card(SPECIES_ID_FERROTHORN), snorlax = card(SPECIES_ID_SNORLAX), gengar = card(SPECIES_ID_GENGAR);
    assert(gc_darby_score4(&charizard, &ferrothorn) == 16);
    const gc_darby_card dusknoir = card(SPECIES_ID_DUSKNOIR);
    assert(gc_darby_score4(&dusknoir, &snorlax) == 0);
    /* Bug against Fire/Flying: 0.5 x 0.5 = 0.25x (a dual-type attacker would take its best type: Beedrill's Poison is 1x) */
    const gc_darby_card pinsir = card(SPECIES_ID_PINSIR);   /* pure Bug */
    (void)gengar;
    assert(gc_darby_score4(&pinsir, &charizard) == 1);
    puts("ok score (the spec's Charizard/Swampert example, 4x, 0x)");
}

static void test_tie_break(void)
{
    /* the same species both sides: equal score, equal Speed: the coin, both ways over seeds */
    const gc_darby_card a = card(SPECIES_ID_PIKACHU), b = card(SPECIES_ID_PIKACHU);
    assert(gc_darby_compare(&a, &b) == 0);
    int wins = 0;
    for (int i = 0; i < 400; i++) {
        pr_rng r;
        seed_rng(&r, (uint32_t)i * 2654435761u + 1);
        wins += gc_darby_player_wins(&a, &b, &r);
    }
    assert(wins > 150 && wins < 250);
    /* equal score, different base Speed: the faster wins, no coin. Raichu (110) against Pikachu (90): Electric vs Electric 0.5x both ways */
    const gc_darby_card raichu = card(SPECIES_ID_RAICHU);
    assert(gc_darby_score4(&raichu, &a) == gc_darby_score4(&a, &raichu));
    assert(gc_darby_base_speed(&raichu) > gc_darby_base_speed(&a));
    assert(gc_darby_compare(&raichu, &a) == 1);
    printf("ok tie-break (base Speed first, the coin gave %d of 400)\n", wins);
}

static void test_cheat(void)
{
    const gc_darby_card venusaur = card(SPECIES_ID_VENUSAUR);   /* the player's pick: Grass/Poison */
    /* fair card Swampert loses to Venusaur (Grass 4x on Water/Ground); the bench: Charizard (Fire 2x) and Moltres? use Talonflame-like: Charizard 2x,
     * Pidgeot (Flying 2x), the rest lose */
    gc_darby_card deck[GC_DARBY_DECK] = {card(SPECIES_ID_SWAMPERT), card(SPECIES_ID_SNORLAX), card(SPECIES_ID_CHARIZARD), card(SPECIES_ID_GOLEM),
                                         card(SPECIES_ID_QUAGSIRE), card(SPECIES_ID_ALAKAZAM)};
    int cheats = 0;
    for (int i = 0; i < 1000; i++) {
        pr_rng r;
        seed_rng(&r, (uint32_t)i * 2654435761u + 7);
        bool cheated;
        const int shown = gc_darby_reveal(deck, 0, &venusaur, &r, &cheated);
        if (cheated) {
            cheats++;
            assert(gc_darby_compare(&deck[shown], &venusaur) == 1);   /* only to a strict winner */
        } else {
            assert(shown == 0);
        }
    }
    assert(cheats > 700 && cheats < 800);
    /* the fair card wins: never a cheat */
    for (int i = 0; i < 200; i++) {
        pr_rng r;
        seed_rng(&r, (uint32_t)i + 99);
        bool cheated = true;
        assert(gc_darby_reveal(deck, 2, &venusaur, &r, &cheated) == 2 && !cheated);
    }
    /* nothing on the bench beats the pick: he loses cleanly */
    gc_darby_card weak[GC_DARBY_DECK] = {card(SPECIES_ID_SWAMPERT), card(SPECIES_ID_QUAGSIRE), card(SPECIES_ID_GOLEM), card(SPECIES_ID_RHYDON),
                                         card(SPECIES_ID_GASTRODON), card(SPECIES_ID_WHISCASH)};
    for (int i = 0; i < 200; i++) {
        pr_rng r;
        seed_rng(&r, (uint32_t)i + 5);
        bool cheated = true;
        assert(gc_darby_reveal(weak, 0, &venusaur, &r, &cheated) == 0 && !cheated);
    }
    /* the largest margin wins the swap: against Ferrothorn (Grass/Steel), Charizard's Fire is 4x, Golem's Ground 2x */
    const gc_darby_card ferrothorn = card(SPECIES_ID_FERROTHORN);
    gc_darby_card pick[GC_DARBY_DECK] = {card(SPECIES_ID_SWAMPERT), card(SPECIES_ID_GOLEM), card(SPECIES_ID_CHARIZARD), card(SPECIES_ID_QUAGSIRE),
                                         card(SPECIES_ID_GASTRODON), card(SPECIES_ID_WHISCASH)};
    for (int i = 0; i < 200; i++) {
        pr_rng r;
        seed_rng(&r, (uint32_t)i + 11);
        bool cheated;
        const int shown = gc_darby_reveal(pick, 0, &ferrothorn, &r, &cheated);
        if (cheated) assert(shown == 2);
    }
    printf("ok cheat (%d of 1000 when he can, only to a strict winner, never when fair wins or nothing beats the pick, biggest margin)\n", cheats);
}


/* ---------------------------------------------------------------- the encounter on a run */
static gc_run run;
static gc_system sys;
static bool offer = true;

static bool pillar(void *user, const struct gc_run *r, int which, gc_custom_team *team)
{
    (void)user; (void)r; (void)team;
    return which < 0 && offer;
}
static const gc_custom_hooks hooks = {.pillar = pillar};

static const int PARTY[6] = {SPECIES_ID_VENUSAUR, SPECIES_ID_PIKACHU, SPECIES_ID_LAPRAS, SPECIES_ID_GENGAR, SPECIES_ID_SNORLAX, SPECIES_ID_ALAKAZAM};

static void make_run(const char *seed, bool with_hook, int n_party, int wave)
{
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    if (with_hook) gc_run_set_custom(&run, &hooks);
    gc_system_init_defaults(&sys);
    run.sys = &sys;
    run.wave = wave;
    run.biome = BIOME_ID_PLAINS;
    run.n_items = 0;
    run.n_party = n_party;
    for (int i = 0; i < n_party; i++) {
        gc_starter st;
        memset(&st, 0, sizeof st);
        st.species = (uint16_t)PARTY[i];
        st.tera_type = -1;
        for (int k = 0; k < 6; k++) st.ivs[k] = 15;
        st.moves[0] = MOVE_ID_TACKLE;
        gc_mon_new_player(&run, &run.party[i], &st, 40 + i);
        run.party_order[i] = (int8_t)i;
    }
    run.money = 5000;
    run.battle_type = BATTLE_TYPE_WILD;
    gc_items_to_battle(&run);
}

static void start(const char *seed)
{
    make_run(seed, true, 4, 45);
    gc_rq_reset(&gc_int(&run)->rq);
    run.mystery_type = GC_ME_DARBY;
    gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_DARBY, GC_ME_ULTRA, 0));
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_MYSTERY && run.screen.mystery_type == GC_ME_DARBY && run.screen.n_options == 3);
    assert(run.screen.option_pick[GC_DARBY_STANDARD] == GC_PICK_MEMBER && run.screen.option_pick[GC_DARBY_ALL_IN] == GC_PICK_NONE);
}

static bool choose(int a, int b)
{
    gc_choice c = {GC_CH_MYSTERY, (int16_t)a, (int16_t)b, 0};
    const char *why = NULL;
    return gc_run_choose(&run, &c, &why);
}

static int said(const char *key)
{
    int n = 0;
    gc_event e;
    while (gc_run_next_event(&run, &e)) if (e.kind == GC_EV_MESSAGE && e.msg.key && !strcmp(e.msg.key, key)) n++;
    return n;
}

/* the messages said since the last drain, as one string of keys */
static void drain(char *out, size_t n)
{
    out[0] = 0;
    gc_event e;
    while (gc_run_next_event(&run, &e))
        if (e.kind == GC_EV_MESSAGE && e.msg.key && strlen(out) + strlen(e.msg.key) + 2 < n) { strcat(out, e.msg.key); strcat(out, "|"); }
}

static void set_deck(const int species[GC_DARBY_DECK], int fair, unsigned seed)
{
    gc_internal *in = gc_int(&run);
    for (int k = 0; k < GC_DARBY_DECK; k++) in->darby.deck[k] = card(species[k]);
    in->darby.fair = (int8_t)fair;
    seed_rng(&in->darby.r, seed);
}

static const int WEAK[GC_DARBY_DECK] = {SPECIES_ID_SWAMPERT, SPECIES_ID_QUAGSIRE, SPECIES_ID_GOLEM, SPECIES_ID_RHYDON, SPECIES_ID_GASTRODON, SPECIES_ID_WHISCASH};
static const int FAIR_WINS[GC_DARBY_DECK] = {SPECIES_ID_CHARIZARD, SPECIES_ID_QUAGSIRE, SPECIES_ID_GOLEM, SPECIES_ID_RHYDON, SPECIES_ID_GASTRODON, SPECIES_ID_WHISCASH};
static const int CAN_CHEAT[GC_DARBY_DECK] = {SPECIES_ID_SWAMPERT, SPECIES_ID_SNORLAX, SPECIES_ID_CHARIZARD, SPECIES_ID_GOLEM, SPECIES_ID_QUAGSIRE, SPECIES_ID_ALAKAZAM};

static void test_offer(void)
{
    int bizarre = 0, plain = 0, small = 0, early = 0;
    for (int i = 0; i < 400; i++) {
        char seed[16];
        int t, tier;
        snprintf(seed, sizeof seed, "do-%d", i);
        const int wave = 40 + i % 150;
        make_run(seed, true, 4, wave); pr_run_reset_seed(&run.run, wave); assert(gc_mystery_select(&run, &t, &tier)); bizarre += t == GC_ME_DARBY;
        if (t == GC_ME_DARBY) assert(tier == GC_ME_ULTRA);
        make_run(seed, false, 4, wave); pr_run_reset_seed(&run.run, wave); assert(gc_mystery_select(&run, &t, &tier)); plain += t == GC_ME_DARBY;
        make_run(seed, true, 3, wave); pr_run_reset_seed(&run.run, wave); assert(gc_mystery_select(&run, &t, &tier)); small += t == GC_ME_DARBY;
        make_run(seed, true, 4, 10 + i % 30); pr_run_reset_seed(&run.run, run.wave); assert(gc_mystery_select(&run, &t, &tier)); early += t == GC_ME_DARBY;
    }
    assert(bizarre > 0 && plain == 0 && small == 0 && early == 0);
    /* no last wave: still offered at 190 */
    int tier, minw, maxw, seen, party, ch, mask;
    double money;
    assert(gc_mystery_rule(GC_ME_DARBY, &tier, &minw, &maxw, &seen, &party, &ch, &money, &mask));
    assert(tier == GC_ME_ULTRA && minw == 40 && maxw >= 200 && seen == 1 && party == 4);
    for (int b = 0; b < 60; b++) assert(gc_mystery_in_biome(b, GC_ME_DARBY) || b >= BIOME_ID_END + 1);
    printf("ok offer (Bizarre only, wave 40+, a party of four, Ultra: drawn %d of 400; never plain, three Pokemon or before wave 40)\n", bizarre);
}

static void test_walk_away(void)
{
    start("dw");
    assert(choose(GC_DARBY_WALK_AWAY, 0));
    assert(run.n_party == 4 && run.money == 5000 && run.screen.kind != GC_SCREEN_REWARD);
    puts("ok walk away (nothing lost, nothing given, no shop)");
}

/* the Standard wager up to the card screen; the soul is member `soul` */
static void to_card(const char *seed, int soul)
{
    start(seed);
    assert(!choose(GC_DARBY_STANDARD, 9));   /* no such Pokemon */
    assert(choose(GC_DARBY_STANDARD, soul));
    assert(run.screen.kind == GC_SCREEN_MYSTERY && run.screen.me_stage == GC_ME_STAGE_DARBY_CARD && run.screen.n_options == 1);
}

static void test_deal(void)
{
    for (int i = 0; i < 50; i++) {
        char seed[16];
        snprintf(seed, sizeof seed, "dd-%d", i);
        to_card(seed, 0);
        const gc_internal *in = gc_int(&run);
        assert(in->darby.level == 43);   /* the party's highest */
        assert(in->darby.fair >= 0 && in->darby.fair < GC_DARBY_DECK);
        int cards = 0;
        for (int k = 0; k < GC_DARBY_DECK; k++) {
            for (int j = 0; j < k; j++) assert(in->darby.deck[j].species != in->darby.deck[k].species);
            assert(gc_form(in->darby.deck[k].species, 0));
        }
        for (int a = 0; a < run.screen.n_me_args; a++) cards += !strncmp(run.screen.me_args[a].name, "card", 4) && run.screen.me_args[a].kind == PR_ARG_POKEMON_NAME;
        assert(cards == GC_DARBY_DECK);
        /* the soul and the card are independent: the soul can be played */
        assert(gc_darby_pick_ok(&run, 0, 0) && gc_darby_pick_ok(&run, 0, 3));
    }
    puts("ok deal (six distinct cards face up at the party's highest level, the fair one among them)");
}

static void test_standard_win(void)
{
    to_card("dsw", 2);
    set_deck(WEAK, 0, 1);
    const int lv[4] = {run.party[0].b.level, run.party[1].b.level, run.party[2].b.level, run.party[3].b.level};
    assert(choose(0, 0));   /* Venusaur: Grass 4x on Swampert */
    char keys[2048];
    drain(keys, sizeof keys);
    assert(strstr(keys, "darby:result.win") && !strstr(keys, "handLost") && !strstr(keys, "detector"));
    assert(strstr(keys, "darby:reveal.player") && strstr(keys, "darby:reveal.darby") && strstr(keys, "darby:line."));
    assert(!gc_int(&run)->darby.cheated && gc_int(&run)->darby.shown == 0);
    /* 2 Rarer Candy: every member two levels up */
    for (int i = 0; i < 4; i++) assert(run.party[i].b.level == lv[i] + 2);
    /* two vitamins of choice */
    for (int v = 0; v < 2; v++) {
        assert(run.screen.kind == GC_SCREEN_MYSTERY && run.screen.me_stage == GC_ME_STAGE_DARBY_VITAMIN && run.screen.n_options == 6);
        for (int o = 0; o < 6; o++) assert(run.screen.option_ok[o] && run.screen.option_pick[o] == GC_PICK_MEMBER);
        assert(!choose(6, 0));
        assert(choose(v == 0 ? 1 : 5, v == 0 ? 3 : 1));
    }
    int protein = 0, carbos = 0;
    for (int i = 0; i < run.n_items; i++) {
        if (!strcmp(run.items[i].key, "BASE_STAT_BOOSTER") && run.items[i].arg == 1 && run.items[i].pokemon_id == run.party[3].b.id) protein++;
        if (!strcmp(run.items[i].key, "BASE_STAT_BOOSTER") && run.items[i].arg == 5 && run.items[i].pokemon_id == run.party[1].b.id) carbos++;
    }
    assert(protein == 1 && carbos == 1);
    assert(run.n_party == 4 && run.screen.kind != GC_SCREEN_REWARD);   /* no ordinary shop */
    puts("ok Standard win (2 Rarer Candy, two vitamins on the Pokemon picked, no shop, the soul kept)");
}

static void test_loss_accept(void)
{
    /* the Standard soul (member 2, Lapras) leaves with its items; a fainted Pokemon can be the soul */
    to_card("dla", 2);
    set_deck(FAIR_WINS, 0, 2);
    const uint32_t lapras = run.party[2].b.id;
    gc_item_add(&run, "LEFTOVERS", -1, lapras, 1);
    assert(choose(0, 0));
    char keys[2048];
    drain(keys, sizeof keys);
    assert(strstr(keys, "darby:result.handLost") && !strstr(keys, "result.win"));
    assert(run.screen.kind == GC_SCREEN_MYSTERY && run.screen.me_stage == GC_ME_STAGE_DARBY_ACCUSE && run.screen.n_options == 2);
    assert(choose(0, 0));   /* Accept the Deal */
    assert(run.n_party == 3 && run.money == 5000);
    for (int i = 0; i < run.n_party; i++) assert(run.party[i].b.id != lapras);
    for (int i = 0; i < run.n_items; i++) assert(run.items[i].pokemon_id != lapras);
    assert(run.screen.kind != GC_SCREEN_REWARD);
    /* a fainted soul */
    start("dlf");
    run.party[1].b.hp = 0;
    assert(gc_darby_pick_ok(&run, GC_DARBY_STANDARD, 1));
    assert(choose(GC_DARBY_STANDARD, 1));
    puts("ok loss, accepted (the soul leaves with its held items, money kept, no shop; a fainted Pokemon can be wagered)");
}

static void test_accusations(void)
{
    /* a false call: he played fair and won: the soul and all the money go */
    to_card("dfc", 1);
    set_deck(FAIR_WINS, 0, 3);
    assert(choose(0, 0));
    assert(!gc_int(&run)->darby.cheated && run.screen.me_stage == GC_ME_STAGE_DARBY_ACCUSE);
    assert(choose(1, 0));
    assert(run.n_party == 3 && run.money == 0);
    /* a true call: he swapped: the full reward, nothing lost */
    int caught = 0;
    for (unsigned sd = 0; sd < 40 && !caught; sd++) {
        to_card("dtc", 1);
        set_deck(CAN_CHEAT, 0, sd * 7919u + 3);
        assert(choose(0, 0));
        if (!gc_int(&run)->darby.cheated) continue;
        assert(run.screen.me_stage == GC_ME_STAGE_DARBY_ACCUSE);
        const int lv0 = run.party[0].b.level;
        assert(choose(1, 0));
        char keys[2048];
        drain(keys, sizeof keys);
        assert(strstr(keys, "darby:result.caught") && strstr(keys, "darby:result.win"));
        assert(run.n_party == 4 && run.money == 5000 && run.party[0].b.level == lv0 + 2);
        assert(run.screen.me_stage == GC_ME_STAGE_DARBY_VITAMIN);
        caught = 1;
    }
    assert(caught);
    puts("ok accusations (a false call costs the soul and all the money; catching the swap wins the full reward)");
}

static void test_detector(void)
{
    int caught = 0, clean = 0, none = 0;
    for (unsigned sd = 0; sd < 60; sd++) {
        to_card("ddt", 1);
        gc_mon *pika = &run.party[1];
        pika->b.moves[1] = MOVE_ID_FORESIGHT;
        pika->b.n_moves = 2;
        set_deck(sd % 3 == 2 ? FAIR_WINS : CAN_CHEAT, 0, sd * 104729u + 5);
        assert(choose(0, 0));
        char keys[2048];
        drain(keys, sizeof keys);
        const gc_internal *in = gc_int(&run);
        if (in->darby.won) { assert(!strstr(keys, "detector")); none++; continue; }   /* only after a loss */
        assert(in->darby.detector == 1);
        if (in->darby.cheated) { assert(strstr(keys, "darby:detector.caught") && !strstr(keys, "detector.clean")); caught++; }
        else { assert(strstr(keys, "darby:detector.clean") && !strstr(keys, "detector.caught")); clean++; }
    }
    assert(caught > 0 && clean > 0 && none > 0);
    /* nobody qualifies: no line; a fainted one does not count */
    to_card("ddn", 1);
    run.party[2].b.moves[1] = MOVE_ID_TRICK;
    run.party[2].b.hp = 0;
    int ab, mv;
    assert(gc_darby_detector(&run, &ab, &mv) < 0);
    run.party[2].b.hp = 1;
    assert(gc_darby_detector(&run, &ab, &mv) == 2 && mv == MOVE_ID_TRICK && ab < 0);
    set_deck(FAIR_WINS, 0, 9);
    run.party[2].b.moves[1] = MOVE_ID_TACKLE;
    assert(choose(0, 0));
    assert(said("mysteryEncounters/darby:detector.caught") + 0 == 0);
    printf("ok lie detector (after a loss only: %d caught, %d clean, %d wins without it; conscious members only)\n", caught, clean, none);
}

static void test_all_in(void)
{
    /* three souls, one by one, none twice */
    start("dai");
    assert(choose(GC_DARBY_ALL_IN, 0));
    for (int k = 0; k < 3; k++) {
        assert(run.screen.kind == GC_SCREEN_MYSTERY && run.screen.me_stage == GC_ME_STAGE_DARBY_SOUL);
        if (k > 0) assert(!choose(0, k - 1));
        assert(choose(0, k));
    }
    assert(run.screen.me_stage == GC_ME_STAGE_DARBY_CARD);
    set_deck(WEAK, 0, 4);
    const double money = run.money;
    assert(choose(0, 0));
    assert(run.money > money);   /* the Big Nugget */
    pr_reward_ctx *ctx = gc_reward_ctx(&run);
    const int want = pr_reward_get_modifier_count(ctx, NULL);
    for (int shop = 0; shop < 2; shop++) {
        assert(run.screen.kind == GC_SCREEN_REWARD);
        assert(run.screen.n_options == want);
        for (int i = 0; i < run.screen.n_options; i++) assert(run.screen.options[i].tier == MODIFIER_TIER_MASTER);
        gc_choice c = {GC_CH_SKIP, 0, 0, 0};
        const char *why;
        assert(gc_run_choose(&run, &c, &why));
    }
    assert(run.screen.kind != GC_SCREEN_REWARD && run.n_party == 4);
    /* lost and accepted: all three leave */
    start("dal");
    assert(choose(GC_DARBY_ALL_IN, 0));
    for (int k = 1; k <= 3; k++) assert(choose(0, k));
    set_deck(FAIR_WINS, 0, 6);
    const uint32_t keep = run.party[0].b.id;
    assert(choose(0, 0));
    assert(choose(0, 0));
    assert(run.n_party == 1 && run.party[0].b.id == keep);
    printf("ok All-In (three distinct souls; a win: a Big Nugget and two screens of %d Master-tier items; a loss takes all three)\n", want);
}

/* the playtest hook (rundriver.txt "mystery=darby,5", lorenzo 2026-11-17): it reaches Bizarre runs for D'Arby, keeps his party of four and never fires
 * in a plain run (no pillar hook: the encounter does not exist there) */
static void test_forced(void)
{
    const struct { bool hook; int party; bool me; } C[] = {{true, 4, true}, {true, 3, false}, {false, 4, false}};
    for (int k = 0; k < 3; k++) {
        gc_debug_force_mystery(GC_ME_DARBY, 5);
        make_run("darby-forced", C[k].hook, C[k].party, 3);
        run.wave = 3; gc_new_battle(&run);
        assert(run.wave == 4 && run.battle_type != BATTLE_TYPE_MYSTERY_ENCOUNTER);
        /* the first wave from 5 that is not a trainer's becomes D'Arby (a trainer wave keeps its trainer) */
        bool got = false;
        for (int w = 5; w < 10 && !got; w++) {
            gc_new_battle(&run);
            assert(run.wave == w);
            if (run.battle_type == BATTLE_TYPE_TRAINER) continue;
            got = true;
            assert((run.battle_type == BATTLE_TYPE_MYSTERY_ENCOUNTER) == C[k].me);
            if (C[k].me) assert(run.mystery_type == GC_ME_DARBY);
        }
        assert(got);
    }
    gc_debug_force_mystery(-1, 1);
    puts("ok forced (rundriver mystery=darby,5: the first non-trainer wave from 5, Bizarre with four Pokemon only)");
}

int main(void)
{
    test_forced();
    test_score();
    test_tie_break();
    test_cheat();
    test_offer();
    test_walk_away();
    test_deal();
    test_standard_win();
    test_loss_accept();
    test_accusations();
    test_detector();
    test_all_in();
    puts("mystery_darby_test: all passed");
    return 0;
}
