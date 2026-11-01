/* Delibird-y against upstream at 8555c08: replays the trace of tests/mystery-delibirdy.producer.test.ts (the real encounter on a random party
 * of 1-6 Pokemon holding items of every kind, the Amulet Coin / Candy Jar / Berry Pouch / Healing Charm at every stack, money short of the
 * price now and then: the encounter's requirements, every option's meetsRequirements(), the party screen's filter of each Pokemon and the
 * list of held items it names, then the chosen option for real: onPreOptionPhase with selectPokemonForOption answering, onOptionPhase in
 * executeWithSeedOffset(.., * 500) with the ModifierRewardPhase it queues and the text it says recorded) through game-core's
 * gc_mystery_setup_screen / gc_mystery_execute / gc_delibirdy_*.
 *
 * Per case, compared with what upstream did: the encounter's primary and money requirements; which options are on; the party-select filter
 * of every Pokemon for options 2 and 3 and the held items it lists (type, argument, stack, in order); the transferable flag of every held
 * item; the run RNG (upstream draws none); money; the shop the encounter leaves with; the phases queued ahead of it (the ModifierRewardPhase
 * and the item it gives); the messages; and the whole item list entry by entry.
 * Usage: mystery_delibirdy_diff_test <trace>; exit 0 only with 0 mismatches. */
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
typedef struct { int n; item_row it[24]; } list_row;
static list_row read_list(void)
{
    list_row l;
    l.n = (int)integer();
    if (l.n < 0 || l.n > 24) { fputs("list size\n", stderr); exit(2); }
    for (int k = 0; k < l.n; k++) { word(l.it[k].key, sizeof l.it[k].key); l.it[k].arg = (int)integer(); l.it[k].stack = (int)integer(); l.it[k].holder = -1; l.it[k].xfer = -1; }
    return l;
}

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
        snprintf(msgs[n_msgs], sizeof msgs[0], "%s", k);
        for (int a = 0; a < e.msg.n_args; a++) {
            if (!e.msg.args[a].name || strcmp(e.msg.args[a].name, "modifierName") || !e.msg.args[a].s) continue;
            /* "modifierType:ModifierType.SHELL_BELL.name" */
            const char *p = strstr(e.msg.args[a].s, "ModifierType.");
            if (!p) continue;
            p += strlen("ModifierType.");
            const char *end = strrchr(p, '.');
            snprintf(msgs[n_msgs] + strlen(msgs[n_msgs]), sizeof msgs[0] - strlen(msgs[n_msgs]), ":%.*s", (int)(end ? (size_t)(end - p) : strlen(p)), p);
        }
        n_msgs++;
    }
}

static int bad, shown, case_no, case_option, case_wave;
#define CHECK(cond, ...) do { if (!(cond)) { bad++; if (shown < 14) { shown++; fprintf(stderr, "case %d (option %d wave %d): ", case_no, case_option, case_wave); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } } while (0)
static int fields_checked;
#define EQ(name, c_val, up_val) do { fields_checked++; CHECK((long long)(c_val) == (long long)(up_val), "%s is %lld, upstream %lld", name, (long long)(c_val), (long long)(up_val)); } while (0)

#define PID(m) ((uint32_t)(0x4000 + (m)))
static int holder_of(const gc_item *it, int n)
{
    if (!it->pokemon_id) return -1;
    if (it->pokemon_id >= PID(0) && it->pokemon_id < PID(n)) return (int)it->pokemon_id - 0x4000;
    return -2;
}

int main(int argc, char **argv)
{
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_delibirdy_diff_test <trace>\n", stderr); return 2; }
    tag('V');
    integer();
    integer();
    int cases = 0, mismatches = 0, by_option[3] = {0}, disabled = 0, lists_checked = 0, filters = 0, xfers = 0, items_checked = 0, msgs_checked = 0, shell_bells = 0,
        reqs = 0, rewards = 0;
    char c;
    while (fscanf(f, " %c", &c) == 1) {
        if (c == 'Z') break;
        if (c != 'C') { fprintf(stderr, "wrong record %c\n", c); return 2; }
        char run_seed[64], wave_seed[64];
        case_no = (int)integer();
        word(run_seed, sizeof run_seed); word(wave_seed, sizeof wave_seed);
        const int wave = (int)integer(), option = (int)integer();
        const double money_start = real();
        const int n_mons = (int)integer(), runs = (int)integer(), pick_mon = (int)integer(), pick_sub = (int)integer();
        case_option = option; case_wave = wave;
        if (n_mons < 1 || n_mons > 6) { fputs("party size\n", stderr); return 2; }
        struct { int species, form, level, alive; } setup[6];
        tag('U');
        for (int m = 0; m < n_mons; m++) {
            setup[m].species = (int)integer(); setup[m].form = (int)integer(); setup[m].level = (int)integer(); setup[m].alive = (int)integer();
        }
        tag('L');
        static item_row before_items[120], after_items[120];
        const int n_before = read_items(before_items, true);
        tag('A');
        int meets[3], f1[6], f2[6];
        for (int k = 0; k < 3; k++) meets[k] = (int)integer();
        const int spawn_primary = (int)integer(), spawn_scene = (int)integer();
        for (int m = 0; m < n_mons; m++) f1[m] = (int)integer();
        for (int m = 0; m < n_mons; m++) f2[m] = (int)integer();
        list_row lists[2][6];
        tag('H');
        for (int m = 0; m < n_mons; m++) lists[0][m] = read_list();
        tag('I');
        for (int m = 0; m < n_mons; m++) lists[1][m] = read_list();
        tag('Z');
        const pr_rng rng_start = rng_vals();
        pr_rng before = {0}, after_pre = {0}, after_option = {0};
        double money_after = 0;
        int leave_arg = 0, n_up_msgs = 0, n_up_queue = 0, n_after = 0;
        char up_msgs[MAX_MSG][64], up_queue[8][64], chosen_token[96];
        if (runs) {
            tag('R'); before = rng_vals();
            tag('B'); after_pre = rng_vals();
            tag('Q'); after_option = rng_vals(); money_after = real(); leave_arg = (int)integer(); real();
            tag('N');
            n_up_msgs = (int)integer();
            for (int k = 0; k < n_up_msgs; k++) word(up_msgs[k], sizeof up_msgs[k]);
            tag('X');
            n_up_queue = (int)integer();
            if (n_up_queue > 8) { fputs("queue size\n", stderr); return 2; }
            for (int k = 0; k < n_up_queue; k++) word(up_queue[k], sizeof up_queue[k]);
            tag('T'); word(chosen_token, sizeof chosen_token);
            tag('V');
            n_after = read_items(after_items, false);
        }
        tag('E');
        (void)after_pre;

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
        for (int m = 0; m < n_mons; m++) st[m] = (m + case_no % n_mons) % n_mons;   /* storage order is not party order */
        for (int m = 0; m < n_mons; m++) {
            run.party[st[m]] = tmpl;
            gc_mon *mon = &run.party[st[m]];
            mon->b.id = PID(m);
            mon->b.species = (uint16_t)setup[m].species;
            mon->b.form = (uint8_t)setup[m].form;
            mon->b.fusion_species = 0;
            mon->b.level = (uint8_t)setup[m].level;
            mon->b.on_field = false;
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
            it->non_transferable = before_items[k].xfer == 2;   /* locked by the producer (a held-modifier config can say isTransferable: false): the run carries the flag */
        }
        gc_items_to_battle(&run);
        bad = 0;
        for (int m = 0; m < n_mons; m++) {
            gc_mon *mon = &run.party[st[m]];
            pr_calculate_stats(&run.bt, &mon->b);
            mon->b.hp = setup[m].alive ? mon->b.stats[0] : 0;
        }
        run.wave = wave;
        run.biome = BIOME_ID_PLAINS;
        run.money = money_start;
        run.battle_type = BATTLE_TYPE_WILD;
        pr_seed_from_ascii(&run.run.wave_seed, wave_seed);
        run.run.rng = rng_start;
        gc_rq_reset(&gc_int(&run)->rq);
        run.mystery_type = GC_ME_DELIBIRDY;
        memset(&run.screen, 0, sizeof run.screen);
        run.screen.kind = GC_SCREEN_MYSTERY;
        run.screen.mystery_type = GC_ME_DELIBIRDY;
        gc_mystery_setup_screen(&run);

        /* the requirements, the options, the party screen */
        CHECK(run.screen.n_options == 3, "%d options", run.screen.n_options);
        EQ("primary requirement", gc_delibirdy_spawnable(&run), spawn_primary);
        EQ("money requirement", gc_delibirdy_option_ok(&run, 0), spawn_scene);
        reqs += 2;
        for (int k = 0; k < 3; k++) CHECK(run.screen.option_ok[k] == (meets[k] != 0), "option %d is %s, upstream %s", k + 1, run.screen.option_ok[k] ? "enabled" : "disabled", meets[k] ? "enabled" : "disabled");
        for (int m = 0; m < n_mons; m++) {
            for (int o = 1; o <= 2; o++) {
                const int want = o == 1 ? f1[m] : f2[m];
                CHECK(gc_delibirdy_pick_ok(&run, o, st[m]) == (want != 0), "party filter of Pokemon %d for option %d is %d, upstream %d", m, o + 1, gc_delibirdy_pick_ok(&run, o, st[m]), want);
                CHECK(gc_mystery_member_ok(&run, o, st[m]) == (want != 0), "gc_mystery_member_ok of Pokemon %d for option %d differs", m, o + 1);
                filters++;
                int held[GC_MAX_ITEMS];
                const int n = gc_delibirdy_held_items(&run, o, st[m], held);
                const list_row *want_list = &lists[o - 1][m];
                CHECK(n == want_list->n, "Pokemon %d lists %d items for option %d, upstream %d", m, n, o + 1, want_list->n);
                for (int j = 0; j < n && j < want_list->n; j++) {
                    const gc_item *it = &run.items[held[j]];
                    CHECK(!strcmp(it->key, want_list->it[j].key) && it->arg == want_list->it[j].arg && it->stack == want_list->it[j].stack,
                          "Pokemon %d option %d list entry %d is %s/%d x%d, upstream %s/%d x%d", m, o + 1, j, it->key, it->arg, it->stack,
                          want_list->it[j].key, want_list->it[j].arg, want_list->it[j].stack);
                    lists_checked++;
                }
            }
        }
        for (int k = 0; k < n_before; k++) {
            if (before_items[k].xfer < 0 || before_items[k].holder < 0) continue;
            CHECK(gc_gts_item_transferable(&run.items[k]) == (before_items[k].xfer == 1), "%s transferable %d, upstream %d", before_items[k].key, gc_gts_item_transferable(&run.items[k]), before_items[k].xfer);
            xfers++;
        }
        if (!runs) {
            CHECK(!run.screen.option_ok[option], "option %d is enabled in C but upstream could not run it", option);
            if (bad) mismatches++;
            cases++; disabled++;
            continue;
        }

        /* run the option for real */
        run.run.rng = before;
        const int b = option == 0 ? 0 : st[pick_mon], cc = option == 0 ? 0 : pick_sub;
        collect();
        n_msgs = 0;
        bool ok = false;
        const char *w2 = "";
        gc_mystery_execute(&run, option, b, cc, &w2, &ok);
        collect();
        CHECK(ok, "option refused: %s", w2);
        CHECK(same_rng(&run.run.rng, &after_option) && same_rng(&before, &after_option), "the run RNG moved (upstream draws none)");
        EQ("money", run.money, money_after);
        {
            gc_rqueue copy = gc_int(&run)->rq;
            gc_rphase p;
            int shop = -1, n_q = 0;
            char got[8][64];
            while (gc_rq_next(&copy, &p)) {
                if (p.type == RP_SELECT_MODIFIER) shop = (int)p.a;
                if (p.type == RP_MODIFIER_REWARD && n_q < 8) snprintf(got[n_q++], sizeof got[0], "ModifierRewardPhase:%s", p.key);
            }
            CHECK(shop == (leave_arg ? 1 : 2), "the shop is a=%d, upstream left with addHealPhase %d", shop, leave_arg);
            CHECK(n_q == n_up_queue, "%d phases queued, upstream %d", n_q, n_up_queue);
            for (int k = 0; k < n_q && k < n_up_queue; k++) { CHECK(!strcmp(got[k], up_queue[k]), "phase %d is %s, upstream %s", k, got[k], up_queue[k]); rewards++; }
        }
        CHECK(n_msgs == n_up_msgs, "%d messages, upstream %d", n_msgs, n_up_msgs);
        for (int k = 0; k < n_msgs && k < n_up_msgs; k++) {
            CHECK(!strcmp(msgs[k], up_msgs[k]), "message %d is %s, upstream %s", k, msgs[k], up_msgs[k]);
            msgs_checked++;
            shell_bells += !strncmp(up_msgs[k], "rewardGain", 10);
        }
        (void)chosen_token;

        /* the item list, entry by entry */
        CHECK(run.n_items == n_after, "%d items, upstream %d", run.n_items, n_after);
        for (int k = 0; k < run.n_items && k < n_after; k++) {
            const gc_item *it = &run.items[k];
            const int holder = holder_of(it, n_mons);
            CHECK(!strcmp(it->key, after_items[k].key) && it->arg == after_items[k].arg && holder == after_items[k].holder && it->stack == after_items[k].stack,
                  "item %d is %s/%d held by %d x%d, upstream %s/%d held by %d x%d", k, it->key, it->arg, holder, it->stack, after_items[k].key, after_items[k].arg, after_items[k].holder, after_items[k].stack);
            items_checked++;
        }
        if (bad) mismatches++;
        cases++; by_option[option]++;
    }
    printf("Mystery encounter Delibird-y: %d cases run (give money %d, give food %d, give an item %d), %d not runnable, %d requirement checks, %d party-select filters, "
           "%d listed items, %d transferable flags, %d items compared, %d messages (%d shell bells), %d queued rewards, %d fields. TOTAL mismatches %d\n",
           cases - disabled, by_option[0], by_option[1], by_option[2], disabled, reqs, filters, lists_checked, xfers, items_checked, msgs_checked, shell_bells, rewards, fields_checked, mismatches);
    return mismatches ? 1 : 0;
}
