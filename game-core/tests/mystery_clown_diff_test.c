/* Clowning Around (options 2 and 3) against upstream at 8555c08: replays the "-clown" trace of tests/mystery-clown.producer.test.ts (the real encounter: onInit in
 * executeWithSeedOffset(.., waveIndex), onPreOptionPhase in (.., getSeedOffset()) and onOptionPhase in (.., getSeedOffset() * 500) really run: the item shuffle and the type
 * shuffle; setEncounterRewards and leaveEncounterWithoutBattle recorded) through game-core.
 * Usage: mystery_clown_diff_test <trace>; exit 0 only with 0 mismatches. */
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
static void bar(void)
{
    char s[4];
    word(s, sizeof s);
    if (strcmp(s, "|")) { fprintf(stderr, "wrong record: wanted |, got %s (byte %ld)\n", s, ftell(f)); exit(2); }
}
static void skip_line(void) { int c; while ((c = fgetc(f)) != EOF && c != '\n') {} }
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

/* "-1,5" / "-" / "9,1": a comma list from the trace */
static int csv(char *w, int *out)
{
    int n = 0;
    if (!strcmp(w, "-")) return 0;
    for (char *p = w; *p;) { out[n++] = atoi(p); while (*p && *p != ',') p++; if (*p == ',') p++; }
    return n;
}

int main(int argc, char **argv)
{
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_clown_diff_test <trace>\n", stderr); return 2; }
    char w[16];
    word(w, sizeof w);
    if (strcmp(w, "V")) return 2;
    integer();
    integer();
    int n_mons = 0;
    int cases = 0, mismatches = 0, by_option[3] = {0}, items_checked = 0, types_checked = 0, texts_checked = 0, fused = 0, swapped = 0, abilities_seen = 0;
    int seen_ability[300] = {0};
    for (;;) {
        word(w, sizeof w);
        if (!strcmp(w, "Z")) break;
        if (!strcmp(w, "P")) {
            n_mons = (int)integer();
            if (n_mons < 1 || n_mons > 6) { fputs("party size\n", stderr); return 2; }
            continue;
        }
        if (strcmp(w, "C") || !n_mons) { fprintf(stderr, "wrong record %s\n", w); return 2; }
        char run_seed[64], wave_seed[64];
        case_no = (int)integer();
        word(run_seed, sizeof run_seed); word(wave_seed, sizeof wave_seed);
        const int wave = (int)integer(), option = (int)integer(), biome = (int)integer();
        const int trainer_id = (int)integer(), secret_id = (int)integer();
        const double money_before = (double)integer();
        case_option = option; case_wave = wave;
        struct { int species, form, fusion, fusion_form, level, nature, ivs[6], hp, max, stats[6], n_moves, moves[4], luck, ability; } setup[6];
        tag("U");
        for (int m = 0; m < n_mons; m++) {
            if (m) semicolon();
            setup[m].species = (int)integer(); setup[m].form = (int)integer(); setup[m].fusion = (int)integer(); setup[m].fusion_form = (int)integer();
            setup[m].level = (int)integer(); setup[m].nature = (int)integer();
            for (int k = 0; k < 6; k++) setup[m].ivs[k] = (int)integer();
            setup[m].hp = (int)integer(); setup[m].max = (int)integer();
            for (int k = 0; k < 6; k++) setup[m].stats[k] = (int)integer();
            setup[m].n_moves = (int)integer();
            for (int k = 0; k < setup[m].n_moves; k++) setup[m].moves[k] = (int)integer();
            setup[m].luck = (int)integer();
            setup[m].ability = (int)integer();
        }
        static item_row before_items[120], after_items[120];
        tag("L");
        const int n_before = read_items(before_items);
        tag("A");
        const int meets[3] = {(int)integer(), (int)integer(), (int)integer()};
        tag("R");
        const pr_rng before = rng_vals();
        tag("I");
        const pr_rng after_init = rng_vals();
        tag("W");
        const int up_ability = (int)integer(), up_first = (int)integer(), up_second = (int)integer();
        char up_ability_name[48], up_blace[32];
        word(up_ability_name, sizeof up_ability_name); word(up_blace, sizeof up_blace);
        tag("Y");
        const int n_configs = (int)integer(), cfg_trainer = (int)integer(), cfg_double = (int)integer(), n_pokemon_configs = (int)integer();
        skip_line();
        tag("Q");
        const pr_rng rng_pre = rng_vals();
        const pr_rng after_pre = rng_vals();
        const pr_rng after_option = rng_vals();
        tag("T");
        const int n_rewards = (int)integer(), leave_calls = (int)integer(), leave_arg = (int)integer(), up_holder = (int)integer();
        tag("V");
        const int n_after = read_items(after_items);
        tag("K");
        struct { int custom[2], n_custom, fusion_custom[2], n_fusion_custom, types[3], n_types; } up_types[6];
        for (int m = 0; m < n_mons; m++) {
            if (m) semicolon();
            char a[24], b[24], c[24];
            word(a, sizeof a); word(b, sizeof b); word(c, sizeof c);
            up_types[m].n_custom = csv(a, up_types[m].custom);
            up_types[m].n_fusion_custom = csv(b, up_types[m].fusion_custom);
            up_types[m].n_types = csv(c, up_types[m].types);
        }
        tag("E");
        (void)rng_pre; (void)after_pre; (void)up_ability_name;

        /* the storage order is rotated, so that party order and storage order differ */
        int st[6];
        for (int m = 0; m < n_mons; m++) st[m] = (m + case_no % n_mons) % n_mons;
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
        for (int m = 0; m < n_mons; m++) {
            run.party[st[m]] = tmpl;
            gc_mon *mon = &run.party[st[m]];
            mon->b.id = (uint32_t)(0x4000 + m);
            mon->b.species = (uint16_t)setup[m].species;
            mon->b.form = (uint8_t)setup[m].form;
            mon->b.fusion_species = (uint16_t)setup[m].fusion;
            mon->b.fusion_form = (uint8_t)setup[m].fusion_form;
            memset(mon->b.custom_types, 0, sizeof mon->b.custom_types);
            memset(mon->b.fusion_custom_types, 0, sizeof mon->b.fusion_custom_types);
            mon->b.level = (uint8_t)setup[m].level;
            mon->b.status = 0;
            mon->b.n_moves = (uint8_t)setup[m].n_moves;
            memset(mon->b.moves, 0, sizeof mon->b.moves);
            memset(mon->b.pp_used, 0, sizeof mon->b.pp_used);
            for (int k = 0; k < setup[m].n_moves; k++) mon->b.moves[k] = (uint16_t)setup[m].moves[k];
            for (int k = 0; k < 6; k++) mon->b.ivs[k] = (uint8_t)setup[m].ivs[k];
            mon->base_nature = (uint8_t)setup[m].nature;
            mon->has_custom_nature = false;
            mon->b.nature = (uint8_t)setup[m].nature;
            mon->luck = (int8_t)setup[m].luck;
            mon->b.shiny = setup[m].luck > 0;
            mon->b.ability = mon->b.base_ability = (uint16_t)setup[m].ability;
            mon->b.custom_ability = (int16_t)setup[m].ability;
            mon->b.on_field = false;
            run.party_order[m] = (int8_t)st[m];
            fused += setup[m].fusion != 0;
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
        run.money = money_before;
        run.wave = wave;
        run.biome = biome;
        run.n_enemy = 0;
        run.in_battle = false;
        run.me_battle = false;
        run.battle_type = BATTLE_TYPE_WILD;
        pr_seed_from_ascii(&run.run.wave_seed, wave_seed);
        run.run.rng = before;
        gc_rq_reset(&gc_int(&run)->rq);
        run.mystery_type = GC_ME_CLOWNING_AROUND;
        memset(&run.screen, 0, sizeof run.screen);
        run.screen.kind = GC_SCREEN_MYSTERY;
        run.screen.mystery_type = GC_ME_CLOWNING_AROUND;
        { gc_event e; while (gc_run_next_event(&run, &e)) {} }
        gc_mystery_setup_screen(&run);
        CHECK(run.screen.n_options == 3, "%d options", run.screen.n_options);
        for (int o = 0; o < 3; o++) CHECK(run.screen.option_ok[o] == (meets[o] != 0), "option %d enabled %d, upstream %d", o, run.screen.option_ok[o], meets[o]);
        CHECK(same_rng(&run.run.rng, &after_init), "the screen moved the run RNG (c %.0f, upstream %.0f)", run.run.rng.c, after_init.c);

        /* onInit's ability and types, and the screen's dialogue tokens */
        int ability, first, second;
        gc_clown_roll(&run, &ability, &first, &second);
        EQ("Blacephalon's ability", ability, up_ability);
        EQ("Blacephalon's first type", first, up_first);
        EQ("Blacephalon's second type", second, up_second);
        EQ("one enemy party config", n_configs, 1);
        EQ("the clown trainer (Harlequin)", cfg_trainer, 25);
        EQ("a double battle", cfg_double, 1);
        EQ("two Pokemon configs", n_pokemon_configs, 2);
        {
            const gc_screen *sc = &run.screen;
            int have_ability = 0, have_name = 0;
            for (int a = 0; a < sc->n_me_args; a++) {
                if (!sc->me_args[a].name) continue;
                if (!strcmp(sc->me_args[a].name, "ability")) have_ability = sc->me_args[a].kind == PR_ARG_ABILITY && (int)sc->me_args[a].v == up_ability;
                if (!strcmp(sc->me_args[a].name, "blacephalonName")) have_name = sc->me_args[a].s && !strcmp(sc->me_args[a].s, "pokemon:blacephalon") && !strcmp(up_blace, "Blacephalon");
            }
            EQ("ability token", have_ability, 1);
            EQ("blacephalonName token", have_name, 1);
            texts_checked += 2;
        }
        if (up_ability >= 0 && up_ability < 300 && !seen_ability[up_ability]) { seen_ability[up_ability] = 1; abilities_seen++; }

        /* the option */
        bool ok = false;
        const char *w2 = "";
        gc_mystery_execute(&run, option, 0, 0, &w2, &ok);
        CHECK(ok, "option refused: %s", w2);
        CHECK(same_rng(&run.run.rng, &after_option), "the run RNG after the option differs (c %.0f, upstream %.0f)", run.run.rng.c, after_option.c);
        EQ("no setEncounterRewards", n_rewards, 0);
        EQ("one leave call", leave_calls, 1);
        EQ("leave with the heal shop", leave_arg, 1);
        check_items("after the option", after_items, n_after);
        items_checked += n_after;
        for (int m = 0; m < n_mons; m++) {
            const gc_mon *mon = &run.party[st[m]];
            char nm[48];
            snprintf(nm, sizeof nm, "Pokemon %d custom type 0", m);
            EQ(nm, mon->b.custom_types[0], up_types[m].n_custom > 0 && up_types[m].custom[0] >= 0 ? up_types[m].custom[0] + 1 : 0);
            snprintf(nm, sizeof nm, "Pokemon %d custom type 1", m);
            EQ(nm, mon->b.custom_types[1], up_types[m].n_custom > 1 && up_types[m].custom[1] >= 0 ? up_types[m].custom[1] + 1 : 0);
            snprintf(nm, sizeof nm, "Pokemon %d fusion custom type 0", m);
            EQ(nm, mon->b.fusion_custom_types[0], up_types[m].n_fusion_custom > 0 && up_types[m].fusion_custom[0] >= 0 ? up_types[m].fusion_custom[0] + 1 : 0);
            snprintf(nm, sizeof nm, "Pokemon %d fusion custom type 1", m);
            EQ(nm, mon->b.fusion_custom_types[1], up_types[m].n_fusion_custom > 1 && up_types[m].fusion_custom[1] >= 0 ? up_types[m].fusion_custom[1] + 1 : 0);
            int8_t got[PR_MAX_TYPES];
            const int n_got = pr_get_types(&mon->b, got, PR_TYPES_NO_TERA | PR_TYPES_BYPASS_SUMMON | PR_TYPES_IGNORE_THIRD);
            snprintf(nm, sizeof nm, "Pokemon %d type count", m);
            EQ(nm, n_got, up_types[m].n_types);
            for (int t = 0; t < n_got && t < up_types[m].n_types; t++) { snprintf(nm, sizeof nm, "Pokemon %d type %d", m, t); EQ(nm, got[t], up_types[m].types[t]); }
            types_checked += n_got;
        }

        /* the texts the option says: "selected2" (with the Pokemon the items were shuffled on) and "selected3" */
        {
            gc_event ev;
            int n2 = 0, n3 = 0, who_ok = 0, name_ok = 0;
            const char *want2 = option == 1 ? "mysteryEncounters/clowningAround:option.2.selected2" : "mysteryEncounters/clowningAround:option.3.selected2";
            const char *want3 = option == 1 ? "mysteryEncounters/clowningAround:option.2.selected3" : "mysteryEncounters/clowningAround:option.3.selected3";
            while (gc_run_next_event(&run, &ev)) {
                if (ev.kind != GC_EV_MESSAGE || !ev.msg.key) continue;
                if (!strcmp(ev.msg.key, want2)) {
                    n2++;
                    for (int a = 0; a < ev.msg.n_args; a++) {
                        if (!ev.msg.args[a].name) continue;
                        if (!strcmp(ev.msg.args[a].name, "blacephalonName")) name_ok = ev.msg.args[a].s && !strcmp(ev.msg.args[a].s, "pokemon:blacephalon");
                        if (!strcmp(ev.msg.args[a].name, "switchPokemon")) who_ok = up_holder >= 0 && (int)(ev.msg.args[a].v >> 16) == setup[up_holder].species;
                    }
                }
                if (!strcmp(ev.msg.key, want3)) n3++;
            }
            EQ("selected2 once", n2, 1);
            EQ("selected3 once", n3, 1);
            EQ("selected2 names Blacephalon", name_ok, 1);
            if (option == 1) { EQ("selected2 names the Pokemon", who_ok, 1); swapped++; }
            else EQ("no Pokemon token in the type shuffle's text", up_holder, -1);
            texts_checked += 3;
        }
        {
            int heal_shop = 0;
            for (gc_rphase p; gc_rq_next(&gc_int(&run)->rq, &p);) if (p.type == RP_SELECT_MODIFIER && p.a == 1) heal_shop++;
            EQ("the heal shop", heal_shop, 1);
        }
        if (bad) mismatches++;
        cases++; by_option[option]++;
    }
    printf("Mystery encounter Clowning Around: %d cases run (item shuffle %d, type shuffle %d; %d abilities rolled, %d fused Pokemon), %d items compared, %d types compared, %d texts, "
           "%d fields. TOTAL mismatches %d\n",
           cases, by_option[1], by_option[2], abilities_seen, fused, items_checked, types_checked, texts_checked, fields_checked, mismatches);
    return mismatches ? 1 : 0;
}
