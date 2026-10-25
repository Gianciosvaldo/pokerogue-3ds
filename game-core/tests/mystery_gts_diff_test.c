/* The Global Trade System against upstream at 8555c08: replays the trace of tests/mystery-gts.producer.test.ts (the real
 * encounter's onInit run again in executeWithSeedOffset(.., waveIndex) for a random party of 1-6 Pokemon of every kind, the
 * options' meetsRequirements() and the party-select filters, then the chosen option for real: onPreOptionPhase with
 * selectPokemonForOption answering, onOptionPhase in executeWithSeedOffset(.., * 500) with the trade, the held-item
 * carry-over, removePokemonFromPlayerParty, the dex update and the item trade's tier/regeneration really run) through
 * game-core's gc_mystery_setup_screen / gc_mystery_execute / gc_gts_*.
 *
 * Per case, compared with what upstream did: the starting stats; which options are on and the party-select filter of every
 * Pokemon; the transferable flag of every held item; the three trade options of every party member (every number of each
 * Pokemon and its moveset); the run RNG before and after (Check Trade Offers and Leave draw nothing from it, Wonder Trade
 * draws the Pokemon in the pre-phase, the item trade draws the trader's name after the option phase); the received Pokemon;
 * the party order; the whole item list entry by entry; the dex chain; the reward the item trade sets (item, argument, tier,
 * luck upgrades) and the tier its withTierFromPool answered; the messages and the trader's name entry; the shop; and money.
 * Not compared: the Poke Ball of the received Pokemon (unseeded upstream), a pair trader name's half (RNG-checked only).
 * Usage: mystery_gts_diff_test <trace>; exit 0 only with 0 mismatches. */
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
static void rest_of_line(void) { int ch; while ((ch = fgetc(f)) != EOF && ch != '\n') {} }
static pr_rng rng_vals(void)
{
    pr_rng r = {0};
    r.c = real(); r.s0 = real(); r.s1 = real(); r.s2 = real();
    return r;
}
static bool same_rng(const pr_rng *a, const pr_rng *b) { return a->c == b->c && a->s0 == b->s0 && a->s1 == b->s1 && a->s2 == b->s2; }

typedef struct {
    long long id;
    int species, form, level, hp, stats[6], ball, met_wave, met_level, met_biome, met_species, ivs[6], nature, ability, gender, shiny, variant, friendship, luck;
    double exp;
    int tera, passive, pokerus, n_moves, moves[4];
} up_mon;
static up_mon read_mon(void)
{
    up_mon u;
    u.id = integer(); u.species = (int)integer(); u.form = (int)integer(); u.level = (int)integer(); u.hp = (int)integer();
    for (int k = 0; k < 6; k++) u.stats[k] = (int)integer();
    u.ball = (int)integer(); u.met_wave = (int)integer(); u.met_level = (int)integer(); u.met_biome = (int)integer(); u.met_species = (int)integer();
    for (int k = 0; k < 6; k++) u.ivs[k] = (int)integer();
    u.nature = (int)integer(); u.ability = (int)integer(); u.gender = (int)integer(); u.shiny = (int)integer(); u.variant = (int)integer();
    u.friendship = (int)integer(); u.luck = (int)integer(); u.exp = real(); u.tera = (int)integer(); u.passive = (int)integer(); u.pokerus = (int)integer();
    u.n_moves = (int)integer();
    if (u.n_moves < 0 || u.n_moves > 4) { fputs("move count\n", stderr); exit(2); }
    for (int k = 0; k < u.n_moves; k++) u.moves[k] = (int)integer();
    return u;
}

typedef struct { int id; unsigned long long caught, nature, count, seen_attr, seen_count; int ivs[6], candy, abil; } up_dex;
static up_dex read_dex(void)
{
    up_dex d;
    d.id = (int)integer(); d.caught = (unsigned long long)integer(); d.nature = (unsigned long long)integer(); d.count = (unsigned long long)integer();
    for (int k = 0; k < 6; k++) d.ivs[k] = (int)integer();
    d.candy = (int)integer(); d.abil = (int)integer(); d.seen_attr = (unsigned long long)integer(); d.seen_count = (unsigned long long)integer();
    return d;
}

typedef struct { char key[48]; int arg, holder, stack, xfer; } item_row;
static int read_items(item_row *rows, bool with_xfer)
{
    const int n = (int)integer();
    if (n < 0 || n > 120) { fputs("item count\n", stderr); exit(2); }
    for (int k = 0; k < n; k++) {
        word(rows[k].key, sizeof rows[k].key); rows[k].arg = (int)integer(); rows[k].holder = (int)integer(); rows[k].stack = (int)integer();
        rows[k].xfer = with_xfer ? (int)integer() : -1;
    }
    return n;
}

#define MAX_MSG 8
static char msgs[MAX_MSG][48], trader_arg[160];
static int n_msgs;
static void collect(void)
{
    gc_event e;
    while (gc_run_next_event(&run, &e)) {
        if (e.kind != GC_EV_MESSAGE || !e.msg.key || n_msgs >= MAX_MSG) continue;
        const char *k = strrchr(e.msg.key, ':');
        k = k ? k + 1 : e.msg.key;
        snprintf(msgs[n_msgs++], sizeof msgs[0], "%s", k);
        for (int a = 0; a < e.msg.n_args; a++)
            if (e.msg.args[a].name && !strcmp(e.msg.args[a].name, "tradeTrainerName") && e.msg.args[a].s) snprintf(trader_arg, sizeof trader_arg, "%s", e.msg.args[a].s);
    }
}

static int bad, shown, case_no, case_option, case_wave;
#define CHECK(cond, ...) do { if (!(cond)) { bad++; if (shown < 14) { shown++; fprintf(stderr, "case %d (option %d wave %d): ", case_no, case_option, case_wave); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } } while (0)
static int fields_checked;
#define EQ(name, c_val, up_val) do { fields_checked++; CHECK((long long)(c_val) == (long long)(up_val), "%s is %lld, upstream %lld", name, (long long)(c_val), (long long)(up_val)); } while (0)

/* every number of a Pokemon the trace has, and its moveset; `ball` is never compared (unseeded upstream) */
static void check_mon(const gc_mon *m, const up_mon *u, const char *what)
{
    const pr_pokemon *p = &m->b;
    char name[64];
#define F(label, c_val, up_val) do { snprintf(name, sizeof name, "%s %s", what, label); EQ(name, c_val, up_val); } while (0)
    F("id", p->id, u->id); F("species", p->species, u->species); F("form", p->form, u->form); F("level", p->level, u->level); F("hp", p->hp, u->hp);
    for (int k = 0; k < 6; k++) { char l[16]; snprintf(l, sizeof l, "stat %d", k); F(l, p->stats[k], u->stats[k]); }
    F("met wave", m->met_wave, u->met_wave); F("met level", m->met_level, u->met_level);
    F("met biome", m->met_biome, u->met_biome); F("met species", m->met_species, u->met_species);
    for (int k = 0; k < 6; k++) { char l[16]; snprintf(l, sizeof l, "iv %d", k); F(l, p->ivs[k], u->ivs[k]); }
    F("nature", p->nature, u->nature); F("ability index", m->ability_index, u->ability); F("gender", p->gender, u->gender);
    F("shiny", p->shiny, u->shiny); F("variant", m->variant, u->variant); F("friendship", p->friendship, u->friendship);
    F("luck", m->luck, u->luck); F("tera type", p->tera_type, u->tera); F("passive", p->passive_flag, u->passive); F("pokerus", m->pokerus, u->pokerus);
    snprintf(name, sizeof name, "%s exp", what);
    fields_checked++;
    CHECK(m->exp == u->exp, "%s is %.17g, upstream %.17g", name, m->exp, u->exp);
    F("move count", p->n_moves, u->n_moves);
    for (int k = 0; k < p->n_moves && k < u->n_moves; k++) { char l[16]; snprintf(l, sizeof l, "move %d", k); F(l, p->moves[k], u->moves[k]); }
#undef F
}

#define PID(m) ((uint32_t)(0x4000 + (m)))
static int holder_of(const gc_item *it, int n)
{
    if (!it->pokemon_id) return -1;
    if (it->pokemon_id >= PID(0) && it->pokemon_id < PID(n)) return (int)it->pokemon_id - 0x4000;
    return -2;
}

int main(int argc, char **argv)
{
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_gts_diff_test <trace>\n", stderr); return 2; }
    tag('V');
    integer();
    integer();
    int cases = 0, mismatches = 0, by_option[4] = {0}, disabled = 0, opts_checked = 0, filters = 0, xfers = 0, items_checked = 0, msgs_checked = 0,
        dex_checked = 0, shiny = 0, hidden = 0, rewards = 0, wonder_moves = 0;
    char c;
    while (fscanf(f, " %c", &c) == 1) {
        if (c == 'Z') break;
        if (c != 'C') { fprintf(stderr, "wrong record %c\n", c); return 2; }
        char run_seed[64], wave_seed[64];
        case_no = (int)integer();
        word(run_seed, sizeof run_seed); word(wave_seed, sizeof wave_seed);
        const int wave = (int)integer(), option = (int)integer();
        const double money_start = real();
        const int balls_start = (int)integer(), trainer_id = (int)integer(), secret_id = (int)integer(), charms = (int)integer(), biome = (int)integer(), weather = (int)integer();
        const int n_mons = (int)integer(), runs = (int)integer(), pick_mon = (int)integer(), pick_sub = (int)integer(), cycle_offset = (int)integer();
        case_option = option; case_wave = wave;
        if (n_mons < 1 || n_mons > 6) { fputs("party size\n", stderr); return 2; }
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
        tag('P');
        for (int m = 0; m < n_mons * 5; m++) integer();
        tag('L');
        static item_row before_items[120], after_items[120];
        const int n_before = read_items(before_items, true);
        tag('A');
        int meets[4], flags[6];
        for (int k = 0; k < 4; k++) meets[k] = (int)integer();
        for (int m = 0; m < n_mons; m++) flags[m] = (int)integer();
        const double max_exp_level = real();
        tag('Z');
        const pr_rng rng_start = rng_vals(), rng_init = rng_vals();
        static up_mon offers[6][3];
        for (int m = 0; m < n_mons; m++) {
            tag('O');
            if (integer() != 3) { fputs("offer count\n", stderr); return 2; }
            for (int j = 0; j < 3; j++) offers[m][j] = read_mon();
        }
        pr_rng before = {0}, after_pre = {0}, after_option = {0};
        double money_after = 0, money0 = 0;
        int leave_arg = 0, n_up_msgs = 0, n_after = 0, n_order = 0, order[6], n_chain = 0, has_new = 0, reward_n = -1, reward_tier_seen = -1;
        char up_msgs[MAX_MSG][48], trader_key[160] = "-";
        up_mon received;
        struct { char key[48]; int arg, tier, upgrade; } reward = {"", -1, -1, 0};
        up_dex dex_before[8], dex_after[8];
        if (runs) {
            tag('R'); before = rng_vals();
            tag('B'); after_pre = rng_vals();
            tag('Q'); after_option = rng_vals(); money_after = real(); leave_arg = (int)integer(); money0 = real();
            tag('N');
            n_up_msgs = (int)integer();
            for (int k = 0; k < n_up_msgs; k++) word(up_msgs[k], sizeof up_msgs[k]);
            tag('T');
            word(trader_key, sizeof trader_key);
            rest_of_line();
            tag('S');
            reward_n = (int)integer();
            if (reward_n > 0) {
                word(reward.key, sizeof reward.key); reward.arg = (int)integer(); reward.tier = (int)integer(); reward.upgrade = (int)integer();
                integer();   /* fillRemaining */
            }
            tag('G'); reward_tier_seen = (int)integer();
            tag('W');
            n_order = (int)integer();
            for (int k = 0; k < n_order; k++) order[k] = (int)integer();
            char t;
            if (fscanf(f, " %c", &t) != 1) return 2;
            if (t == 'M') { received = read_mon(); has_new = 1; if (fscanf(f, " %c", &t) != 1) return 2; }
            if (t != 'V') { fprintf(stderr, "wrong record: wanted V, got %c\n", t); return 2; }
            n_after = read_items(after_items, false);
            tag('D');
            n_chain = (int)integer();
            for (int k = 0; k < n_chain; k++) dex_after[k] = read_dex();
            tag('Y');
            for (int k = 0; k < n_chain; k++) dex_before[k] = read_dex();
        }
        tag('E');
        (void)rng_start; (void)money0; (void)after_pre; (void)reward_tier_seen;

        /* the starting state */
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
        run.sys = &sys;
        const gc_mon tmpl = run.party[0];
        run.n_items = 0;
        memset(run.items, 0, sizeof run.items);
        int st[6];
        for (int m = 0; m < n_mons; m++) { st[m] = (m + case_no % n_mons) % n_mons; }
        for (int m = 0; m < n_mons; m++) {
            run.party[st[m]] = tmpl;
            gc_mon *mon = &run.party[st[m]];
            mon->b.id = PID(m);
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
            it->pokemon_id = before_items[k].holder == -1 ? 0 : before_items[k].holder == -2 ? 0x9000u : PID(before_items[k].holder);
            it->stack = before_items[k].stack;
        }
        gc_items_to_battle(&run);
        bad = 0;
        for (int m = 0; m < n_mons; m++) {
            gc_mon *mon = &run.party[st[m]];
            pr_calculate_stats(&run.bt, &mon->b);
            bool same = true;
            for (int k = 0; k < 6; k++) same = same && mon->b.stats[k] == setup[m].stats[k];
            CHECK(same && mon->b.stats[0] == setup[m].max, "starting stats of Pokemon %d differ (C max %d, upstream %d)", m, mon->b.stats[0], setup[m].max);
            mon->b.hp = setup[m].hp;
        }
        run.trainer_id = trainer_id;
        run.secret_id = secret_id;
        run.wave_cycle_offset = cycle_offset;
        run.wave = wave;
        run.biome = (uint8_t)biome;
        run.money = money_start;
        run.pokeballs[0] = (uint8_t)balls_start;
        run.battle_type = BATTLE_TYPE_WILD;
        (void)weather; (void)charms;
        CHECK(pr_run_get_max_exp_level(&run.run, wave, false) == max_exp_level, "level cap %.0f, upstream %.0f", pr_run_get_max_exp_level(&run.run, wave, false), max_exp_level);
        pr_seed_from_ascii(&run.run.wave_seed, wave_seed);
        run.run.rng = runs ? before : rng_init;
        gc_rq_reset(&gc_int(&run)->rq);
        run.mystery_type = GC_ME_GLOBAL_TRADE_SYSTEM;
        memset(&run.screen, 0, sizeof run.screen);
        run.screen.kind = GC_SCREEN_MYSTERY;
        run.screen.mystery_type = GC_ME_GLOBAL_TRADE_SYSTEM;
        gc_mystery_setup_screen(&run);

        /* the options, filters and transferable flags */
        CHECK(run.screen.n_options == 4, "%d options", run.screen.n_options);
        for (int k = 0; k < 4; k++) CHECK(run.screen.option_ok[k] == (meets[k] != 0), "option %d is %s, upstream %s", k + 1, run.screen.option_ok[k] ? "enabled" : "disabled", meets[k] ? "enabled" : "disabled");
        if (option < 3) {
            for (int m = 0; m < n_mons; m++) { CHECK(gc_gts_pick_ok(&run, option, st[m]) == (flags[m] != 0), "party filter of Pokemon %d is %d, upstream %d", m, gc_gts_pick_ok(&run, option, st[m]), flags[m]); filters++; }
        }
        for (int k = 0; k < n_before; k++) {
            if (before_items[k].xfer < 0 || before_items[k].holder < 0) continue;
            CHECK(gc_gts_item_transferable(&run.items[k]) == (before_items[k].xfer != 0), "%s transferable %d, upstream %d", before_items[k].key, gc_gts_item_transferable(&run.items[k]), before_items[k].xfer);
            xfers++;
        }
        /* the three trade options of every party member (onInit, from the RNG at the encounter's start) */
        {
            gc_run *fresh = &run;
            const pr_rng saved = fresh->run.rng;
            for (int m = 0; m < n_mons; m++) {
                gc_mon got[3];
                if (!gc_gts_trade_options(fresh, st[m], got)) { CHECK(false, "trade options of Pokemon %d could not be made", m); continue; }
                for (int j = 0; j < 3; j++) {
                    char what[32];
                    snprintf(what, sizeof what, "offer %d.%d", m, j);
                    check_mon(&got[j], &offers[m][j], what);
                    opts_checked++;
                }
            }
            fresh->run.rng = saved;
        }
        if (!runs) {
            CHECK(option == 2 ? true : !run.screen.option_ok[option], "option %d is enabled in C but upstream could not run it", option);
            if (option == 2) { int any = 0; for (int m = 0; m < n_mons; m++) any |= gc_gts_pick_ok(&run, 2, st[m]); CHECK(!any, "C has a Pokemon with a transferable item, upstream none"); }
            if (bad) mismatches++;
            cases++; disabled++;
            continue;
        }

        for (int k = 0; k < n_chain; k++) {
            const int id = dex_before[k].id;
            gc_dex_entry *d = &sys.dex[id];
            d->caught_attr = dex_before[k].caught; d->nature_attr = (uint32_t)dex_before[k].nature; d->caught_count = (uint32_t)dex_before[k].count;
            d->seen_attr = dex_before[k].seen_attr; d->seen_count = (uint32_t)dex_before[k].seen_count;
            for (int j = 0; j < 6; j++) d->ivs[j] = (uint8_t)dex_before[k].ivs[j];
            if (dex_before[k].candy >= 0) sys.starters[id].candy_count = dex_before[k].candy;
            if (dex_before[k].abil >= 0) sys.starters[id].ability_attr = (uint32_t)dex_before[k].abil;
        }
        /* run the option for real */
        int b = option == 3 ? 0 : st[pick_mon], cc = pick_sub;
        collect();
        n_msgs = 0;
        trader_arg[0] = 0;
        const int n_items_before = run.n_items;
        bool ok = false;
        const char *w2 = "";
        gc_mystery_execute(&run, option, b, cc, &w2, &ok);
        collect();
        CHECK(ok, "option refused: %s", w2);
        CHECK(same_rng(&run.run.rng, &after_option), "the run RNG after the option differs from upstream's");
        if (option == 0 || option == 3) CHECK(same_rng(&before, &after_option) && same_rng(&before, &after_pre), "upstream drew from the run RNG");
        CHECK(run.money == money_after, "money %.0f, want %.0f", run.money, money_after);
        {
            gc_rqueue copy = gc_int(&run)->rq;
            gc_rphase p;
            int shop = -1;
            while (gc_rq_next(&copy, &p)) if (p.type == RP_SELECT_MODIFIER) shop = (int)p.a;
            CHECK(shop == (leave_arg ? 1 : 2), "the shop is a=%d, upstream left with addHealPhase %d", shop, leave_arg);
        }
        CHECK(n_msgs == n_up_msgs, "%d messages, upstream %d", n_msgs, n_up_msgs);
        for (int k = 0; k < n_msgs && k < n_up_msgs; k++) { CHECK(!strcmp(msgs[k], up_msgs[k]), "message %d is %s, upstream %s", k, msgs[k], up_msgs[k]); msgs_checked++; }
        if (option != 3) {
            char want[200];
            snprintf(want, sizeof want, "$t(%s)", trader_key);
            CHECK(!strcmp(trader_arg, want), "trader %s, upstream %s", trader_arg, want);
        }

        /* who is left, in party order */
        CHECK(run.n_party == n_order, "%d in the party, upstream %d", run.n_party, n_order);
        for (int k = 0; k < run.n_party && k < n_order; k++) {
            const gc_mon *pm = &run.party[run.party_order[k]];
            const int idx = order[k];
            if (idx >= 0) CHECK(pm->b.id == PID(idx), "party slot %d holds Pokemon %lld, upstream %d", k, (long long)pm->b.id - 0x4000, idx);
            else if (has_new) check_mon(pm, &received, "received");
        }
        if (has_new) {
            shiny += received.shiny; hidden += received.ability == 2;
            if (option == 1) wonder_moves += received.n_moves;
        }
        /* the item list, entry by entry */
        CHECK(run.n_items == n_after, "%d items, upstream %d (%d before)", run.n_items, n_after, n_items_before);
        for (int k = 0; k < run.n_items && k < n_after; k++) {
            const gc_item *it = &run.items[k];
            const int holder = holder_of(it, n_mons);
            CHECK(!strcmp(it->key, after_items[k].key) && it->arg == after_items[k].arg && holder == after_items[k].holder && it->stack == after_items[k].stack,
                  "item %d is %s/%d held by %d x%d, upstream %s/%d held by %d x%d", k, it->key, it->arg, holder, it->stack, after_items[k].key, after_items[k].arg, after_items[k].holder, after_items[k].stack);
            items_checked++;
        }
        /* the item trade's reward */
        if (option == 2) {
            const pr_reward_custom *cs = &gc_int(&run)->reward.me_custom;
            CHECK(reward_n == 1 && cs->n_options == 1, "%d rewards, upstream %d", cs->n_options, reward_n);
            if (cs->n_options >= 1 && reward_n == 1) {
                const pr_reward_option *o = &cs->options[0];
                CHECK(!strcmp(pr_item_keys[o->type.item], reward.key) && o->type.arg == reward.arg && o->tier == reward.tier && o->upgrade_count == reward.upgrade,
                      "reward %s/%d tier %d upgrades %d, upstream %s/%d tier %d upgrades %d", pr_item_keys[o->type.item], o->type.arg, o->tier, o->upgrade_count, reward.key, reward.arg, reward.tier, reward.upgrade);
                rewards++;
            }
        } else {
            CHECK(reward_n == -1, "upstream set rewards for option %d", option);
        }
        /* the dex chain */
        for (int k = 0; k < n_chain; k++) {
            const int id = dex_after[k].id;
            const gc_dex_entry *d = &sys.dex[id];
            EQ("dex caught attr", d->caught_attr, dex_after[k].caught);
            EQ("dex natures", d->nature_attr, dex_after[k].nature);
            EQ("dex caught count", d->caught_count, dex_after[k].count);
            EQ("dex seen attr", d->seen_attr, dex_after[k].seen_attr);
            EQ("dex seen count", d->seen_count, dex_after[k].seen_count);
            for (int j = 0; j < 6; j++) EQ("dex iv", d->ivs[j], dex_after[k].ivs[j]);
            if (dex_after[k].candy >= 0) {
                EQ("candy", sys.starters[id].candy_count, dex_after[k].candy);
                EQ("ability attr", sys.starters[id].ability_attr, dex_after[k].abil);
            }
            dex_checked++;
        }
        if (bad) mismatches++;
        cases++; by_option[option]++;
    }
    printf("Mystery encounter Global Trade System: %d cases run (check offers %d, wonder trade %d, item trade %d, leave %d), %d not runnable, %d trade options compared, %d party-select filters, "
           "%d transferable flags, %d items compared, %d messages, %d rewards, %d dex entries, %d fields, %d shiny, %d hidden-ability received. TOTAL mismatches %d\n",
           cases - disabled, by_option[0], by_option[1], by_option[2], by_option[3], disabled, opts_checked, filters, xfers, items_checked, msgs_checked, rewards, dex_checked, fields_checked, shiny, hidden, mismatches);
    (void)wonder_moves;
    return mismatches ? 1 : 0;
}
