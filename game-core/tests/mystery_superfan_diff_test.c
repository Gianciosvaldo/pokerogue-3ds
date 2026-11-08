/* The Bug-Type Superfan's options 2 and 3 and its requirements against upstream at 8555c08: replays the "-superfan" trace of tests/mystery-superfan.producer.test.ts (the real
 * encounter: the requirements, option 2's onPreOptionPhase in executeWithSeedOffset(.., getSeedOffset()), option 3's party screen answered by the oracle, onOptionPhase in
 * executeWithSeedOffset(.., getSeedOffset() * 500), setEncounterRewards recorded) through game-core.
 * Usage: mystery_superfan_diff_test <trace>; exit 0 only with 0 mismatches. */
#include "../src/internal.h"
#include "gc/gacha.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;
static FILE *f;

static long long integer(void) { long long v; if (fscanf(f, "%lld", &v) != 1) { fprintf(stderr, "malformed input at byte %ld\n", ftell(f)); exit(2); } return v; }
static double real(void) { double v; if (fscanf(f, "%lf", &v) != 1) { fprintf(stderr, "malformed input at byte %ld\n", ftell(f)); exit(2); } return v; }
static void word(char *out, size_t cap)
{
    char fmt[16];
    snprintf(fmt, sizeof fmt, "%%%zus", cap - 1);
    if (fscanf(f, fmt, out) != 1) { fputs("malformed input\n", stderr); exit(2); }
}
static void tag(const char *want)
{
    char w[16];
    word(w, sizeof w);
    if (strcmp(w, want)) { fprintf(stderr, "wrong record: wanted %s, got %s (byte %ld)\n", want, w, ftell(f)); exit(2); }
}
static void semicolon(void)
{
    char s[4];
    word(s, sizeof s);
    if (strcmp(s, ";")) { fprintf(stderr, "wrong record: wanted ;, got %s\n", s); exit(2); }
}
static pr_rng rng_vals(void)
{
    pr_rng r = {0};
    r.c = real(); r.s0 = real(); r.s1 = real(); r.s2 = real();
    return r;
}
static bool same_rng(const pr_rng *a, const pr_rng *b) { return a->c == b->c && a->s0 == b->s0 && a->s1 == b->s1 && a->s2 == b->s2; }

typedef struct { char key[48]; int arg, holder, stack; } item_row;
static int read_items(item_row *rows)
{
    const int n = (int)integer();
    if (n < 0 || n > 120) { fputs("item count\n", stderr); exit(2); }
    for (int k = 0; k < n; k++) {
        word(rows[k].key, sizeof rows[k].key);
        rows[k].arg = (int)integer();
        rows[k].holder = (int)integer();
        rows[k].stack = (int)integer();
    }
    return n;
}

static int bad, shown, case_no, case_option, case_wave;
#define CHECK(cond, ...) do { if (!(cond)) { bad++; if (shown < (getenv("SHOW") ? atoi(getenv("SHOW")) : 14)) { shown++; fprintf(stderr, "case %d (option %d wave %d): ", case_no, case_option, case_wave); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } } while (0)
static int fields_checked;
#define EQ(name, c_val, up_val) do { fields_checked++; CHECK((long long)(c_val) == (long long)(up_val), "%s is %lld, upstream %lld", name, (long long)(c_val), (long long)(up_val)); } while (0)

static void check_items(const char *what, const item_row *rows, int n)
{
    char nm[96];
    snprintf(nm, sizeof nm, "%s: item count", what);
    EQ(nm, run.n_items, n);
    for (int k = 0; k < n && k < run.n_items; k++) {
        const gc_item *it = &run.items[k];
        const int holder = it->pokemon_id ? (int)(it->pokemon_id - 0x4000) : -1;
        CHECK(!strcmp(it->key, rows[k].key) && it->arg == rows[k].arg && holder == rows[k].holder && it->stack == rows[k].stack,
              "%s: item %d is %s/%d holder %d x%d, upstream %s/%d holder %d x%d", what, k, it->key, it->arg, holder, it->stack, rows[k].key, rows[k].arg, rows[k].holder, rows[k].stack);
    }
}

int main(int argc, char **argv)
{
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_superfan_diff_test <trace>\n", stderr); return 2; }
    char w[16];
    word(w, sizeof w);
    if (strcmp(w, "V")) return 2;
    integer();
    integer();
    int cases = 0, by_option[3] = {0}, tiers[4] = {0}, specials = 0, items_checked = 0, filters_checked = 0, spawn[2] = {0}, rng_checked = 0;
    static item_row before_items[120], after_items[120];
    for (;;) {
        word(w, sizeof w);
        if (!strcmp(w, "Z")) break;
        if (!strcmp(w, "P")) {
            const int n = (int)integer();
            for (int m = 0; m < n * 2; m++) integer();
            continue;
        }
        if (strcmp(w, "C")) { fprintf(stderr, "wrong record %s\n", w); return 2; }
        char run_seed[64], wave_seed[64];
        case_no = (int)integer();
        word(run_seed, sizeof run_seed); word(wave_seed, sizeof wave_seed);
        const int wave = (int)integer(), option = (int)integer();
        case_option = option; case_wave = wave;
        struct { int orig, species, form, level, alive; } setup[6];
        int n_mons = 0;
        tag("U");
        for (;;) {
            if (n_mons >= 6) { fputs("party size\n", stderr); return 2; }
            setup[n_mons].orig = (int)integer(); setup[n_mons].species = (int)integer(); setup[n_mons].form = (int)integer();
            setup[n_mons].level = (int)integer(); setup[n_mons].alive = (int)integer();
            n_mons++;
            /* the next token is ";" (another Pokemon) or the next record's tag */
            const long at = ftell(f);
            char s[4];
            word(s, sizeof s);
            if (!strcmp(s, ";")) continue;
            fseek(f, at, SEEK_SET);
            break;
        }
        tag("L");
        const int n_before = read_items(before_items);
        tag("A");
        const int meets[4] = {(int)integer(), (int)integer(), (int)integer(), (int)integer()};

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
        for (int m = 0; m < n_mons; m++) st[m] = (m + case_no % n_mons) % n_mons;   /* party order and storage order differ */
        for (int m = 0; m < n_mons; m++) {
            run.party[st[m]] = tmpl;
            gc_mon *mon = &run.party[st[m]];
            mon->b.id = (uint32_t)(0x4000 + setup[m].orig);
            mon->b.species = (uint16_t)setup[m].species;
            mon->b.form = (uint8_t)setup[m].form;
            mon->b.fusion_species = 0;
            mon->b.level = (uint8_t)setup[m].level;
            mon->b.status = 0;
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
            it->pokemon_id = before_items[k].holder < 0 ? 0 : (uint32_t)(0x4000 + before_items[k].holder);
            it->stack = before_items[k].stack;
        }
        gc_items_to_battle(&run);
        for (int m = 0; m < n_mons; m++) {
            gc_mon *mon = &run.party[st[m]];
            pr_calculate_stats(&run.bt, &mon->b);
            mon->b.hp = setup[m].alive ? mon->b.stats[0] : 0;
            if (!setup[m].alive) mon->b.status = STATUS_EFFECT_FAINT;
        }
        run.wave = wave;
        run.biome = BIOME_ID_PLAINS;
        run.n_enemy = 0;
        run.in_battle = false;
        run.battle_type = BATTLE_TYPE_WILD;
        pr_seed_from_ascii(&run.run.wave_seed, wave_seed);
        gc_rq_reset(&gc_int(&run)->rq);
        run.mystery_type = GC_ME_BUG_TYPE_SUPERFAN;
        memset(&run.screen, 0, sizeof run.screen);
        run.screen.kind = GC_SCREEN_MYSTERY;
        run.screen.mystery_type = GC_ME_BUG_TYPE_SUPERFAN;
        { gc_event e; while (gc_run_next_event(&run, &e)) {} }
        gc_mystery_setup_screen(&run);
        cases++;

        /* the requirements: the encounter's (wave range, party size, the Some(..) of three) and each option's */
        {
            const int ok_wave = wave >= 30 && wave <= 180;
            int allowed = 0;
            for (int m = 0; m < run.n_party; m++) allowed += gc_mon_allowed(&run.party[m]);   /* PartySizeRequirement(3, 6, excludeDisallowedPokemon) */
            const int spawn_c = ok_wave && allowed >= 3 && gc_superfan_spawnable(&run);
            EQ("the encounter's requirements", spawn_c, meets[0]);
            spawn[spawn_c]++;
            CHECK(run.screen.n_options == 3, "%d options", run.screen.n_options);
            for (int o = 0; o < 3; o++) EQ("option enabled", run.screen.option_ok[o], meets[1 + o]);
        }

        char w2[16];
        word(w2, sizeof w2);
        if (!strcmp(w2, "E")) continue;   /* a disabled option 2: nothing was run */
        if (option == 1 && strcmp(w2, "Q")) { fprintf(stderr, "wrong record: wanted Q, got %s\n", w2); return 2; }
        int picked_member = -1, picked_item = -1;
        if (option == 2) {
            if (strcmp(w2, "F")) { fprintf(stderr, "wrong record: wanted F, got %s\n", w2); return 2; }
            /* the party screen's filter for every member, in the party's order */
            for (int m = 0; m < n_mons; m++) {
                const int up = (int)integer();
                EQ("party screen filter", gc_mystery_member_ok(&run, 2, st[m]), up);
                filters_checked++;
            }
            tag("V");
            char first[16];
            word(first, sizeof first);
            int offered_n = 0;
            if (strcmp(first, "-")) {
                const int mon = atoi(first);
                offered_n = (int)integer();
                int held[GC_MAX_ITEMS];
                const int n_c = gc_mystery_held_items(&run, 2, st[mon], held, GC_MAX_ITEMS);
                EQ("items offered", n_c, offered_n);
                for (int k = 0; k < offered_n; k++) { char lb[64]; word(lb, sizeof lb); }
                picked_member = mon;
            }
            tag("PK");
            char pk[16];
            word(pk, sizeof pk);
            if (strcmp(pk, "-")) picked_item = (int)integer(); else picked_item = -1;
            tag("GO");
            integer();
            if (picked_item < 0) { tag("E"); continue; }
        }
        if (option == 2) tag("Q");
        (void)rng_vals();
        const pr_rng up_pre = rng_vals(), up_after = rng_vals();
        tag("S");
        const int up_bugs = (int)integer();
        char tok_num[64], tok_item[64];
        word(tok_num, sizeof tok_num);
        word(tok_item, sizeof tok_item);
        const int n_sel = (int)integer();
        { char line[400]; if (!fgets(line, sizeof line, f)) return 2; (void)n_sel; }
        tag("T");
        const int n_rewards = (int)integer(), fill = (int)integer(), n_funcs = (int)integer();
        char funcs[4][48];
        for (int k = 0; k < n_funcs && k < 4; k++) word(funcs[k], sizeof funcs[k]);
        const int n_opts = (int)integer();
        struct { char key[48]; int arg, tier, upgrade; double cost; } opts[8];
        for (int k = 0; k < n_opts; k++) {
            if (k) semicolon();
            word(opts[k].key, sizeof opts[k].key);
            opts[k].arg = (int)integer(); opts[k].tier = (int)integer(); opts[k].upgrade = (int)integer(); opts[k].cost = real();
        }
        const int n_tiers = (int)integer(), leave_calls = (int)integer(), leave_arg = (int)integer();
        tag("W");
        const int n_after = read_items(after_items);
        tag("E");
        by_option[option]++;
        EQ("setEncounterRewards calls", n_rewards, 1);
        EQ("leaveEncounterWithoutBattle calls", leave_calls, 1);
        EQ("a heal phase", leave_arg, option == 2);
        EQ("guaranteed tiers", n_tiers, 0);
        EQ("fillRemaining", fill, 0);
        (void)tok_num; (void)tok_item;

        /* the run, as the option's code finds it, then the option */
        EQ("Bug types", gc_superfan_num_bug_types(&run), up_bugs);
        gc_rq_reset(&gc_int(&run)->rq);
        memset(&gc_int(&run)->reward.me_custom, 0, sizeof gc_int(&run)->reward.me_custom);
        bool ok = false;
        const pr_rng saved = run.run.rng;
        const bool executed = gc_mystery_execute(&run, option, option == 2 ? st[picked_member] : 0, option == 2 ? picked_item : 0, &why, &ok) && ok;
        CHECK(executed, "the option was rejected: %s", why ? why : "");
        if (!executed) continue;
        EQ("the run RNG is untouched", same_rng(&run.run.rng, &saved), 1);

        /* the streams: the option select's (wave * 1000 + 512), then its x500 */
        {
            pr_rng pre = gc_superfan_pre_rng;
            if (option == 2) pre = pr_run_seed_offset_rng(&run.run, wave * 1000 + 512, NULL);
            CHECK(same_rng(&pre, &up_pre), "the pre-option stream is at c=%.0f, upstream %.0f", pre.c, up_pre.c);
            const pr_rng after = pr_run_seed_offset_rng(&run.run, (wave * 1000 + 512) * 500, NULL);
            CHECK(same_rng(&after, &up_after), "the option stream differs");
            rng_checked += 2;
        }

        /* the shop: setEncounterRewards' options, then its funcs */
        const pr_reward_custom *c = &gc_int(&run)->reward.me_custom;
        EQ("shop options", c->n_options, n_opts);
        EQ("shop funcs", c->n_funcs, n_funcs);
        EQ("shop fills", c->fill_remaining, 0);
        for (int k = 0; k < c->n_options && k < n_opts; k++) {
            const pr_reward_option *o = &c->options[k];
            CHECK(!strcmp(pr_item_keys[o->type.item], opts[k].key) && o->type.arg == opts[k].arg && o->tier == opts[k].tier && o->upgrade_count == opts[k].upgrade && o->cost == opts[k].cost,
                  "shop option %d is %s/%d tier %d, upstream %s/%d tier %d", k, pr_item_keys[o->type.item], o->type.arg, o->tier, opts[k].key, opts[k].arg, opts[k].tier);
            items_checked++;
        }
        for (int k = 0; k < c->n_funcs && k < n_funcs; k++) {
            CHECK(!strcmp(pr_item_keys[c->funcs[k]], funcs[k]), "shop func %d is %s, upstream %s", k, pr_item_keys[c->funcs[k]], funcs[k]);
            items_checked++;
        }
        if (option == 1) {
            tiers[up_bugs < 2 ? 0 : up_bugs < 4 ? 1 : up_bugs < 6 ? 2 : 3]++;
            specials += n_opts > 2;
        }
        check_items("held items after", after_items, n_after);
    }
    printf("\nMystery encounter The Bug-Type Superfan: %d cases run (option 2: %d, option 3: %d; reward tiers by Bug types %d/%d/%d/%d, %d with a special item; the encounter offered %d times, not %d), %d party screen filters, %d shop items, %d RNG streams, %d fields. TOTAL mismatches %d\n",
           cases, by_option[1], by_option[2], tiers[0], tiers[1], tiers[2], tiers[3], specials, spawn[1], spawn[0], filters_checked, items_checked, rng_checked, fields_checked, bad);
    return bad ? 1 : 0;
}
