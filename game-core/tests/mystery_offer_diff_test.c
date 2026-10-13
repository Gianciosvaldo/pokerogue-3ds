/* An Offer You Can't Refuse against upstream at 8555c08: replays the "-offer" trace of
 * tests/mystery-actions.producer.test.ts (the real encounter's onInit run again for the strongest Pokemon and its price,
 * the options' meetsRequirements() and the Pokemon option 2 picks, onPreOptionPhase of the sale with
 * removePokemonFromPlayerParty really run, onOptionPhase in executeWithSeedOffset(.., * 500), the ModifierRewardPhase it
 * queued really rewarded, the rewards phase's doEncounterRewardsAndContinue with the PartyExpPhase really started and
 * EggLapsePhase.start()) through game-core's gc_mystery_setup_screen / gc_mystery_execute.
 *
 * Per case, compared with what upstream did: the starting stats and level cap; the strongest Pokemon and its price; which
 * options are offered and the Pokemon the extortion option picks (an extortion move first, else an ability); the run RNG
 * (nothing is drawn) and money; the order of the queued phases; the experience phase with its exact amount; the heal-only
 * shop; the party after (who is left, in what order); the whole item list after, entry by entry, in order (the sold
 * Pokemon's held items are pruned by updateModifiers, and a species item whose holder is gone takes the entry before it
 * with it); the Shiny Charm given (or the stack-full fallback) and the messages; and the eggs' wave counters.
 * Usage: mystery_offer_diff_test <trace>; exit 0 only with 0 mismatches. */
#include "../src/internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;
static FILE *f;

static long long integer(void) { long long v; if (fscanf(f, "%lld", &v) != 1) { fputs("malformed input\n", stderr); exit(2); } return v; }
static double real(void) { double v; if (fscanf(f, "%lf", &v) != 1) { fputs("malformed input\n", stderr); exit(2); } return v; }
static void word(char *out, size_t cap)
{
    char fmt[16];
    snprintf(fmt, sizeof fmt, "%%%zus", cap - 1);
    if (fscanf(f, fmt, out) != 1) { fputs("malformed input\n", stderr); exit(2); }
}
static void tag(char want)
{
    char c;
    if (fscanf(f, " %c", &c) != 1 || c != want) { fprintf(stderr, "wrong record: wanted %c, got %c\n", want, c); exit(2); }
}
static pr_rng rng_vals(void)
{
    pr_rng r = {0};
    r.c = real(); r.s0 = real(); r.s1 = real(); r.s2 = real();
    return r;
}
static bool same_rng(const pr_rng *a, const pr_rng *b) { return a->c == b->c && a->s0 == b->s0 && a->s1 == b->s1 && a->s2 == b->s2; }

#define MAX_MSG 8
static char msgs[MAX_MSG][64];
static int n_msgs;
static void collect(void)
{
    gc_event e;
    while (gc_run_next_event(&run, &e)) {
        if (e.kind != GC_EV_MESSAGE || !e.msg.key || n_msgs >= MAX_MSG) continue;
        const char *k = strrchr(e.msg.key, ':');
        k = k ? k + 1 : e.msg.key;
        if (!strcmp(k, "receiveMoney")) snprintf(msgs[n_msgs++], sizeof msgs[0], "receiveMoney:%d", (int)e.msg.args[0].v);
        else snprintf(msgs[n_msgs++], sizeof msgs[0], "%s", k);
    }
}

typedef struct { char key[48]; int arg, holder, stack; } item_row;
static int read_items(item_row *rows)
{
    const int n = (int)integer();
    if (n < 0 || n > 120) { fputs("item count\n", stderr); exit(2); }
    for (int k = 0; k < n; k++) { word(rows[k].key, sizeof rows[k].key); rows[k].arg = (int)integer(); rows[k].holder = (int)integer(); rows[k].stack = (int)integer(); }
    return n;
}

int main(int argc, char **argv)
{
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_offer_diff_test <trace>\n", stderr); return 2; }
    tag('V');
    integer();
    integer();
    int n_mons = 0;
    int cases = 0, mismatches = 0, shown = 0, by_option[3] = {0}, disabled = 0, setup_checked = 0, exp_phases = 0, items_checked = 0, msgs_checked = 0,
        order_checked = 0, terms_checked = 0, cut_checked = 0, by_move = 0, by_ability = 0;
    char c;
    while (fscanf(f, " %c", &c) == 1) {
        if (c == 'Z') break;
        if (c == 'P') {
            n_mons = (int)integer();
            if (n_mons < 2 || n_mons > 6) { fputs("party size\n", stderr); return 2; }
            for (int m = 0; m < n_mons * 2; m++) integer();
            continue;
        }
        if (c != 'C' || !n_mons) { fprintf(stderr, "wrong record %c\n", c); return 2; }
        char run_seed[64], wave_seed[64];
        const int i = (int)integer();
        word(run_seed, sizeof run_seed); word(wave_seed, sizeof wave_seed);
        const int wave = (int)integer(), option = (int)integer();
        const double money_start = real();
        const int balls_start = (int)integer(), runs = (int)integer();
        struct { int species, form, level, nature, ivs[6], pokerus, ability, passive, has_passive, hp, max, stats[6], n_moves, moves[4]; } setup[6];
        tag('U');
        for (int m = 0; m < n_mons; m++) {
            setup[m].species = (int)integer(); setup[m].form = (int)integer(); setup[m].level = (int)integer(); setup[m].nature = (int)integer();
            for (int k = 0; k < 6; k++) setup[m].ivs[k] = (int)integer();
            setup[m].pokerus = (int)integer(); setup[m].ability = (int)integer(); setup[m].passive = (int)integer(); setup[m].has_passive = (int)integer();
            setup[m].hp = (int)integer(); setup[m].max = (int)integer();
            for (int k = 0; k < 6; k++) setup[m].stats[k] = (int)integer();
            setup[m].n_moves = (int)integer();
            for (int k = 0; k < setup[m].n_moves; k++) setup[m].moves[k] = (int)integer();
        }
        tag('L');
        item_row before_items[120], after_items[120];
        const int n_before = read_items(before_items);
        tag('G');
        const int n_eggs = (int)integer();
        int eggs[8];
        for (int k = 0; k < n_eggs; k++) eggs[k] = (int)integer();
        tag('A');
        const int meets[3] = {(int)integer(), (int)integer(), (int)integer()};
        const int primary = (int)integer(), strongest = (int)integer();
        const double price = real(), max_exp_level = real();
        pr_rng before = {0}, after_pre = {0}, after_option = {0}, after_shop = {0};
        double money_after = 0;
        int leave_arg = 0, n_queued = 0, n_up_msgs = 0, n_exp = 0, n_after = 0, n_order = 0, order[6], up_balls = 0, up_stacks = 0, up_eggs[8], n_up_eggs = 0;
        char queued[8][48], up_msgs[MAX_MSG][64], t_args[6][24];
        struct { int kind, index; double exp; } up_exp[6];
        if (runs) {
            tag('R'); before = rng_vals();
            tag('B'); after_pre = rng_vals();
            tag('Q'); after_option = rng_vals(); money_after = real(); leave_arg = (int)integer();
            tag('N');
            n_up_msgs = (int)integer();
            for (int k = 0; k < n_up_msgs; k++) word(up_msgs[k], sizeof up_msgs[k]);
            tag('X');
            n_queued = (int)integer();
            for (int k = 0; k < n_queued; k++) word(queued[k], sizeof queued[k]);
            tag('T');
            for (int k = 0; k < 6; k++) word(t_args[k], sizeof t_args[k]);
            tag('F');
            n_exp = (int)integer();
            for (int k = 0; k < n_exp; k++) { up_exp[k].kind = (int)integer(); up_exp[k].index = (int)integer(); up_exp[k].exp = real(); }
            tag('S'); after_shop = rng_vals();
            tag('W');
            n_order = (int)integer();
            for (int k = 0; k < n_order; k++) order[k] = (int)integer();
            tag('M'); up_balls = (int)integer(); up_stacks = (int)integer();
            tag('V');
            n_after = read_items(after_items);
            tag('H');
            char w[24];
            for (int k = 0; k < n_eggs; k++) { word(w, sizeof w); up_eggs[n_up_eggs++] = atoi(w); }
            if (!n_eggs) word(w, sizeof w);   /* "-" */
        }
        tag('E');
        (void)after_pre; (void)up_balls;

        int st[6], at[6];
        for (int m = 0; m < n_mons; m++) { st[m] = (m + i % n_mons) % n_mons; at[st[m]] = m; }
        memset(&run, 0, sizeof run);
        gc_run_new(&run, GAME_MODES_CLASSIC, run_seed, 42);
        gc_starter team[1];
        memset(team, 0, sizeof team);
        team[0].species = SPECIES_ID_BULBASAUR;
        team[0].tera_type = -1;
        for (int k = 0; k < 6; k++) team[0].ivs[k] = 15;
        const char *why;
        if (!gc_run_set_starters(&run, team, 1, &why)) { fprintf(stderr, "starters: %s\n", why); return 2; }
        gc_system_init_defaults(&sys);
        for (int k = 0; k < n_eggs; k++) gc_system_add_egg(&sys, (uint32_t)(k + 1), eggs[k], 0, SPECIES_ID_BULBASAUR);
        run.sys = &sys;
        const gc_mon tmpl = run.party[0];
        run.n_items = 0;
        memset(run.items, 0, sizeof run.items);
        for (int m = 0; m < n_mons; m++) {
            run.party[st[m]] = tmpl;
            gc_mon *mon = &run.party[st[m]];
            mon->b.id = (uint32_t)(0x4000 + m);
            mon->b.species = (uint16_t)setup[m].species;
            mon->b.form = (uint8_t)setup[m].form;
            mon->b.fusion_species = 0;
            mon->b.level = (uint8_t)setup[m].level;
            mon->b.n_moves = (uint8_t)setup[m].n_moves;
            memset(mon->b.moves, 0, sizeof mon->b.moves);
            memset(mon->b.pp_used, 0, sizeof mon->b.pp_used);
            for (int k = 0; k < setup[m].n_moves; k++) mon->b.moves[k] = (uint16_t)setup[m].moves[k];
            for (int k = 0; k < 6; k++) mon->b.ivs[k] = (uint8_t)setup[m].ivs[k];
            mon->base_nature = (uint8_t)setup[m].nature;
            mon->has_custom_nature = false;
            mon->b.nature = (uint8_t)setup[m].nature;
            mon->b.on_field = false;
            mon->pokerus = setup[m].pokerus != 0;
            mon->b.ability = mon->b.base_ability = (uint16_t)setup[m].ability;
            mon->b.passive = (uint16_t)setup[m].passive;
            mon->b.passive_flag = setup[m].has_passive != 0;
            run.party_order[m] = (int8_t)st[m];
        }
        run.n_party = n_mons;
        for (int k = 0; k < n_before; k++) {
            const int idx = gc_reward_item_index(before_items[k].key);
            if (idx < 0) { fprintf(stderr, "unknown item %s\n", before_items[k].key); return 2; }
            gc_item *it = &run.items[run.n_items++];
            it->key = pr_item_keys[idx];
            it->arg = (int16_t)before_items[k].arg;
            it->pokemon_id = before_items[k].holder < 0 ? 0 : (uint32_t)(0x4000 + before_items[k].holder);
            it->stack = before_items[k].stack;
        }
        gc_items_to_battle(&run);
        int bad = 0;
#define CHECK(cond, ...) do { if (!(cond)) { bad++; if (shown < 14) { shown++; fprintf(stderr, "case %d (option %d wave %d): ", i, option, wave); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } } while (0)
        for (int m = 0; m < n_mons; m++) {
            gc_mon *mon = &run.party[st[m]];
            pr_calculate_stats(&run.bt, &mon->b);
            bool same = true;
            for (int k = 0; k < 6; k++) same = same && mon->b.stats[k] == setup[m].stats[k];
            CHECK(same && mon->b.stats[0] == setup[m].max, "starting stats of Pokemon %d differ (C max %d, upstream %d)", m, mon->b.stats[0], setup[m].max);
            setup_checked++;
            mon->b.hp = setup[m].hp;
        }
        run.wave = wave;
        run.biome = BIOME_ID_PLAINS;
        run.money = money_start;
        run.pokeballs[0] = (uint8_t)balls_start;
        run.battle_type = BATTLE_TYPE_WILD;
        CHECK(pr_run_get_max_exp_level(&run.run, wave, false) == max_exp_level, "level cap %.0f, upstream %.0f", pr_run_get_max_exp_level(&run.run, wave, false), max_exp_level);
        pr_seed_from_ascii(&run.run.wave_seed, wave_seed);
        run.run.rng = before;
        gc_rq_reset(&gc_int(&run)->rq);
        run.mystery_type = GC_ME_AN_OFFER_YOU_CANT_REFUSE;
        memset(&run.screen, 0, sizeof run.screen);
        run.screen.kind = GC_SCREEN_MYSTERY;
        run.screen.mystery_type = GC_ME_AN_OFFER_YOU_CANT_REFUSE;
        gc_mystery_setup_screen(&run);
        {
            double c_price = 0;
            const int c_strong = gc_offer_terms(&run, &c_price);
            CHECK(c_strong == st[strongest] && c_price == price, "strongest Pokemon %d for %.0f, upstream %d for %.0f", c_strong < 0 ? -1 : at[c_strong], c_price, strongest, price);
            terms_checked++;
        }
        CHECK(run.screen.n_options == 3, "%d options", run.screen.n_options);
        CHECK(run.screen.option_ok[0] == (meets[0] != 0) && run.screen.option_ok[2] == (meets[2] != 0), "options 1/3 are %d/%d", run.screen.option_ok[0], run.screen.option_ok[2]);
        CHECK(run.screen.option_ok[1] == (meets[1] != 0), "option 2 is %s, upstream %s", run.screen.option_ok[1] ? "enabled" : "disabled", meets[1] ? "enabled" : "disabled");
        {
            const int p = gc_mystery_primary_pokemon(&run, GC_ME_AN_OFFER_YOU_CANT_REFUSE, 1);
            CHECK(meets[1] ? p == st[primary] : p == -1, "primary Pokemon %d, upstream %d", p < 0 ? -1 : at[p], meets[1] ? primary : -1);
            terms_checked++;
        }
        if (!runs) {
            CHECK(!run.screen.option_ok[option], "option %d is enabled in C but upstream could not run it", option);
            if (bad) mismatches++;
            cases++; disabled++;
            continue;
        }

        collect();
        n_msgs = 0;
        bool ok = false;
        const char *w2 = "";
        gc_mystery_execute(&run, option, 0, 0, &w2, &ok);
        collect();
        CHECK(ok, "option refused: %s", w2);
        CHECK(same_rng(&run.run.rng, &before) && same_rng(&before, &after_pre) && same_rng(&before, &after_option) && same_rng(&before, &after_shop), "the run RNG moved");
        CHECK(run.money == money_after, "money %.0f, want %.0f", run.money, money_after);
        CHECK(leave_arg == 1, "upstream left with addHealPhase %d", leave_arg);

        /* the queue upstream's phases ran in: [the experience | the Shiny Charm], the heal-only shop, the next wave */
        const char *want_first = option == 0 ? "ModifierRewardPhase:SHINY_CHARM" : option == 1 ? "PartyExpPhase" : NULL;
        const int want_n = want_first ? 3 : 2;
        CHECK(n_queued == want_n && (!want_first || !strcmp(queued[0], want_first)) && !strcmp(queued[want_n - 2], "SelectModifierPhase") && !strcmp(queued[want_n - 1], "+PostMysteryEncounterPhase"),
              "upstream queued %d phases (%s ...)", n_queued, queued[0]);
        CHECK(!strcmp(t_args[0], "0") && !strcmp(t_args[1], "u") && !strcmp(t_args[2], "0") && !strcmp(t_args[3], "-1"), "upstream's shop is not the heal-only one (%s %s %s %s)", t_args[0], t_args[1], t_args[2], t_args[3]);
        {
            gc_rqueue copy = gc_int(&run)->rq;
            gc_rphase p;
            int pos = 0, seen_exp = 0;
            bool seen_reward = false, seen_shop = false;
            while (gc_rq_next(&copy, &p)) {
                if (p.type == RP_EXP) {
                    CHECK(!seen_shop, "an experience phase after the shop");
                    if (seen_exp < n_exp) {
                        const bool same = up_exp[seen_exp].index == at[p.a] && p.d == up_exp[seen_exp].exp && up_exp[seen_exp].kind == (p.b ? 0 : 1);
                        CHECK(same, "experience phase %d is Pokemon %d for %.17g, upstream Pokemon %d for %.17g", seen_exp, at[p.a], p.d, up_exp[seen_exp].index, up_exp[seen_exp].exp);
                    }
                    seen_exp++;
                } else if (p.type == RP_MODIFIER_REWARD) {
                    CHECK(pos == 0 && !seen_shop && p.key && !strcmp(p.key, "SHINY_CHARM"), "the reward phase is at %d (%s)", pos, p.key ? p.key : "-");
                    seen_reward = true;
                } else if (p.type == RP_SELECT_MODIFIER) {
                    CHECK(p.a == 1 && !seen_shop, "the shop is a=%d", (int)p.a);
                    seen_shop = true;
                } else {
                    CHECK(seen_shop && (p.type == RP_NEW_BATTLE || p.type == RP_SELECT_BIOME), "queued phase %d at position %d", p.type, pos);
                }
                pos++;
            }
            CHECK(seen_exp == n_exp, "%d experience phases queued, upstream %d", seen_exp, n_exp);
            CHECK(seen_reward == (option == 0) && seen_shop, "reward phase %d, shop %d", seen_reward, seen_shop);
            exp_phases += seen_exp;
        }
        if (option == 0) {
            /* the party after the sale and the list before the Shiny Charm is given */
            CHECK(gc_item_stack(&run, "SHINY_CHARM") == up_stacks, "%d Shiny Charm stacks before the reward, upstream %d", gc_item_stack(&run, "SHINY_CHARM"), up_stacks);
            /* the first phase of the queue is the reward: run it */
            gc_rphase p;
            if (gc_rq_next(&gc_int(&run)->rq, &p) && p.type == RP_MODIFIER_REWARD) {
                gc_phase_start(&run, &p);
                collect();
            }
        }
        CHECK(n_msgs == n_up_msgs, "%d messages, upstream %d", n_msgs, n_up_msgs);
        for (int k = 0; k < n_msgs && k < n_up_msgs; k++) { CHECK(!strcmp(msgs[k], up_msgs[k]), "message %d is %s, upstream %s", k, msgs[k], up_msgs[k]); msgs_checked++; }
        CHECK(run.pokeballs[0] == up_balls, "%d Poke Balls, upstream %d", run.pokeballs[0], up_balls);

        /* who is left, in party order */
        CHECK(run.n_party == n_order, "%d in the party, upstream %d", run.n_party, n_order);
        for (int k = 0; k < run.n_party && k < n_order; k++) {
            CHECK(run.party[run.party_order[k]].b.id == (uint32_t)(0x4000 + order[k]), "party slot %d holds Pokemon %d, upstream %d", k, (int)run.party[run.party_order[k]].b.id - 0x4000, order[k]);
            order_checked++;
        }
        /* the item list, entry by entry */
        CHECK(run.n_items == n_after, "%d items, upstream %d", run.n_items, n_after);
        for (int k = 0; k < run.n_items && k < n_after; k++) {
            const gc_item *it = &run.items[k];
            const int holder = it->pokemon_id ? (int)it->pokemon_id - 0x4000 : -1;
            CHECK(!strcmp(it->key, after_items[k].key) && it->arg == after_items[k].arg && holder == after_items[k].holder && it->stack == after_items[k].stack,
                  "item %d is %s/%d held by %d x%d, upstream %s/%d held by %d x%d", k, it->key, it->arg, holder, it->stack, after_items[k].key, after_items[k].arg, after_items[k].holder, after_items[k].stack);
            items_checked++;
        }
        if (option == 0) cut_checked += n_before - n_after > 0;
        CHECK(sys.n_eggs == n_eggs, "%d eggs", sys.n_eggs);
        for (int k = 0; k < n_eggs && k < sys.n_eggs; k++) {
            /* the lapse after leaving: the encounter's leave steps the eggs once */
            CHECK(sys.eggs[k].hatch_waves == up_eggs[k], "egg %d has %d waves left, upstream %d", k, sys.eggs[k].hatch_waves, up_eggs[k]);
        }
        if (option == 1) { const gc_mon *pm = &run.party[st[primary]]; (void)pm; by_move += 0; by_ability += 0; }
        if (bad) mismatches++;
        cases++; by_option[option]++;
    }
    printf("Mystery encounter An Offer You Can't Refuse: %d cases run (sell %d, extort %d, leave %d), %d not runnable (no extortion move or ability), %d starting parties, %d strongest-Pokemon and primary checks, "
           "%d party slots, %d items compared, %d sales that cut the list, %d messages, %d experience phases. TOTAL mismatches %d\n",
           cases - disabled, by_option[0], by_option[1], by_option[2], disabled, setup_checked, terms_checked, order_checked, items_checked, cut_checked, msgs_checked, exp_phases, mismatches);
    (void)by_move; (void)by_ability;
    return mismatches ? 1 : 0;
}
