/* The Expert Pokemon Breeder against upstream at 8555c08: replays the "-breeder" trace of tests/mystery-breeder.producer.test.ts (the real encounter: onInit in
 * executeWithSeedOffset(.., waveIndex), onOptionPhase in executeWithSeedOffset(.., getSeedOffset() * 500), initBattleWithEnemyConfig really run for the fight, the win's
 * rewards callback or onGameOver recorded on the player's party, friendship and held items) through game-core.
 * Usage: mystery_breeder_diff_test <trace>; exit 0 only with 0 mismatches. */
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

typedef struct { int species, form, level, boss, segments, ivs[6], nature, ability, passive, shiny, variant, gender, friendship; long long id; int hp, stats[6], n_moves, moves[4]; } mon_row;
static void read_mon(mon_row *b)
{
    b->species = (int)integer(); b->form = (int)integer(); b->level = (int)integer();
    b->boss = (int)integer(); b->segments = (int)integer();
    for (int k = 0; k < 6; k++) b->ivs[k] = (int)integer();
    b->nature = (int)integer(); b->ability = (int)integer(); b->passive = (int)integer(); b->shiny = (int)integer(); b->variant = (int)integer();
    b->gender = (int)integer(); b->friendship = (int)integer(); b->id = integer();
    b->hp = (int)integer();
    integer();   /* max HP: stats[0] */
    for (int k = 0; k < 6; k++) b->stats[k] = (int)integer();
    b->n_moves = (int)integer();
    for (int k = 0; k < b->n_moves; k++) { const int mv = (int)integer(); if (k < 4) b->moves[k] = mv; }
}

static int uid(const gc_mon *m) { return m->b.id >= 0x4000 && m->b.id < 0x4010 ? (int)(m->b.id - 0x4000) : -1; }

/* the player's party in order, as the original indexes */
static void party_order_string(char *out, size_t cap)
{
    out[0] = 0;
    for (int k = 0; k < run.n_party; k++) {
        char t[16];
        snprintf(t, sizeof t, "%s%d", k ? " " : "", uid(&run.party[run.party_order[k]]));
        strncat(out, t, cap - strlen(out) - 1);
    }
}
static void want_order_string(char *out, size_t cap, const int *order, int n)
{
    out[0] = 0;
    for (int k = 0; k < n; k++) {
        char t[16];
        snprintf(t, sizeof t, "%s%d", k ? " " : "", order[k]);
        strncat(out, t, cap - strlen(out) - 1);
    }
}
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
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_breeder_diff_test <trace>\n", stderr); return 2; }
    char w[16];
    word(w, sizeof w);
    if (strcmp(w, "V")) return 2;
    integer();
    integer();
    int n_mons = 0;
    int cases = 0, mismatches = 0, by_option[3] = {0}, wins = 0, losses = 0, items_checked = 0, moves_checked = 0, enemy_checked = 0, eggs_checked = 0, space = 0, female = 0, texts_checked = 0;
    for (;;) {
        word(w, sizeof w);
        if (!strcmp(w, "Z")) break;
        if (!strcmp(w, "P")) {
            n_mons = (int)integer();
            if (n_mons < 3 || n_mons > 6) { fputs("party size\n", stderr); return 2; }
            for (int m = 0; m < n_mons * 2; m++) integer();
            continue;
        }
        if (strcmp(w, "C") || !n_mons) { fprintf(stderr, "wrong record %s\n", w); return 2; }
        char run_seed[64], wave_seed[64];
        case_no = (int)integer();
        word(run_seed, sizeof run_seed); word(wave_seed, sizeof wave_seed);
        const int wave = (int)integer(), option = (int)integer(), biome = (int)integer();
        const int trainer_id = (int)integer(), secret_id = (int)integer();
        const double money_before = (double)integer();
        const int path = (int)integer();
        case_option = option; case_wave = wave;
        struct { int species, form, level, nature, friendship, ivs[6], hp, max, stats[6], n_moves, moves[4], luck, ability; } setup[6];
        tag("U");
        for (int m = 0; m < n_mons; m++) {
            if (m) semicolon();
            setup[m].species = (int)integer(); setup[m].form = (int)integer(); setup[m].level = (int)integer(); setup[m].nature = (int)integer(); setup[m].friendship = (int)integer();
            for (int k = 0; k < 6; k++) setup[m].ivs[k] = (int)integer();
            setup[m].hp = (int)integer(); setup[m].max = (int)integer();
            for (int k = 0; k < 6; k++) setup[m].stats[k] = (int)integer();
            setup[m].n_moves = (int)integer();
            for (int k = 0; k < setup[m].n_moves; k++) setup[m].moves[k] = (int)integer();
            setup[m].luck = (int)integer();
            setup[m].ability = (int)integer();
        }
        static item_row before_items[120], fight_items[120], win_items[120], loss_items[120], enemy_rows[120];
        tag("L");
        const int n_before = read_items(before_items);
        tag("A");
        const int meets[3] = {(int)integer(), (int)integer(), (int)integer()};
        tag("R");
        const pr_rng before = rng_vals();
        tag("I");
        const pr_rng after_init = rng_vals();
        tag("W");
        const int init_msgs = (int)integer();
        int picks[3], commons[3], rares[3];
        for (int k = 0; k < 3; k++) picks[k] = (int)integer();
        for (int k = 0; k < 3; k++) { commons[k] = (int)integer(); rares[k] = (int)integer(); }
        const int sprite0 = (int)integer();
        tag("Y");
        const int trainer_type = (int)integer(), up_female = (int)integer(), up_double = (int)integer();
        skip_line();   /* the three Pokemon's config fields: the fight's Pokemon (X) carry them */
        tag("K");
        bool token_present[9] = {0};
        {
            static const char *const names[9] = {"pokemon1CommonEggs", "pokemon1Name", "pokemon1RareEggs", "pokemon2CommonEggs", "pokemon2Name", "pokemon2RareEggs", "pokemon3CommonEggs", "pokemon3Name", "pokemon3RareEggs"};
            char line[512];
            if (!fgets(line, sizeof line, f)) return 2;
            for (int k = 0; k < 9; k++) { const char *at = strstr(line, names[k]); token_present[k] = at && (at == line || at[-1] == ' ') && (at[strlen(names[k])] == ' ' || at[strlen(names[k])] == '\n'); }
        }
        tag("K2");
        char up_egg_text[6][48];
        for (int k = 0; k < 3; k++) { char nm[48]; word(nm, sizeof nm); }
        for (int k = 0; k < 6; k++) word(up_egg_text[k], sizeof up_egg_text[k]);
        tag("G");
        int generated[3][4] = {{0}};
        for (int e = 0; e < 3; e++) {
            if (e) semicolon();
            for (int k = 0; k < 4; k++) generated[e][k] = (int)integer();
        }
        tag("Q");
        integer();
        (void)rng_vals();
        const pr_rng after_option = rng_vals();
        tag("T");
        const int n_rewards = (int)integer(), fill = (int)integer(), n_funcs = (int)integer();
        char func0[48];
        word(func0, sizeof func0);
        const int n_tiers = (int)integer(), n_given = (int)integer();
        (void)real();
        const int leave_calls = (int)integer(), has_cb = (int)integer(), has_game_over = (int)integer();
        tag("Z2");
        const int n_eggs = (int)integer();
        int egg_tier[40], egg_source[40];
        for (int k = 0; k < n_eggs; k++) {
            if (k) semicolon();
            egg_tier[k] = (int)integer(); egg_source[k] = (int)integer();
            (void)integer();
            char d[64];
            word(d, sizeof d);
        }
        tag("OU");
        const int n_outro = (int)integer();
        skip_line();
        tag("O");
        const double money_after = (double)integer();
        { char nm[48]; word(nm, sizeof nm); }
        const int up_chosen = (int)integer();
        tag("H");
        skip_line();
        tag("D");
        const int enc_mode = (int)integer(), battle_double = (int)integer(), n_levels = (int)integer();
        int levels[3] = {0, 0, 0};
        for (int k = 0; k < n_levels; k++) { const int l = (int)integer(); if (k < 3) levels[k] = l; }
        const int up_trainer = (int)integer(), up_variant = (int)integer();
        skip_line();
        tag("F");
        const int n_effects = (int)integer();
        tag("J");
        const int n_enemy_items = (int)integer();
        for (int k = 0; k < n_enemy_items; k++) {
            enemy_rows[k].holder = (int)integer();
            word(enemy_rows[k].key, sizeof enemy_rows[k].key);
            enemy_rows[k].arg = (int)integer(); enemy_rows[k].stack = (int)integer();
        }
        mon_row up[3];
        tag("X");
        const int n_x = (int)integer();
        for (int k = 0; k < n_x && k < 3; k++) { if (k) semicolon(); read_mon(&up[k]); }
        tag("X3");
        integer();
        struct { int ball, tera, ability, luck, slot, fusion; char name[40]; } x3[3];
        for (int k = 0; k < 3; k++) {
            if (k) semicolon();
            x3[k].ball = (int)integer(); x3[k].tera = (int)integer(); x3[k].ability = (int)integer(); x3[k].luck = (int)integer(); x3[k].slot = (int)integer(); x3[k].fusion = (int)integer();
            word(x3[k].name, sizeof x3[k].name);
        }
        tag("N");
        int status[3], ai[3];
        for (int k = 0; k < 3; k++) { if (k) semicolon(); status[k] = (int)integer(); ai[k] = (int)integer(); }
        char tera_slots[24], tera_mode[8];
        word(tera_slots, sizeof tera_slots);
        word(tera_mode, sizeof tera_mode);
        tag("B");
        const int fight_party_n = (int)integer();
        int fight_party[6];
        for (int k = 0; k < fight_party_n; k++) fight_party[k] = (int)integer();
        bar();
        const int n_fight_items = read_items(fight_items);
        int up_order[6], up_len, up_friend_before, up_friend_after, up_hp = 0, up_failed, up_do_rewards = 0, up_failed_calls = 0, n_up_outro = 0, n_win_items = 0, n_loss_items = 0;
        char up_result[16] = "0";
        word(w, sizeof w);
        if (!strcmp(w, "WIN")) {
            up_len = (int)integer();
            for (int k = 0; k < up_len; k++) up_order[k] = (int)integer();
            bar();
            up_friend_before = (int)integer(); up_friend_after = (int)integer();
            bar();
            n_win_items = read_items(win_items);
            bar();
            up_failed = (int)integer();
        } else {
            if (strcmp(w, "LOSS")) { fprintf(stderr, "wrong record: %s\n", w); return 2; }
            word(up_result, sizeof up_result);
            up_len = (int)integer();
            for (int k = 0; k < up_len; k++) up_order[k] = (int)integer();
            bar();
            up_friend_before = (int)integer(); up_friend_after = (int)integer(); up_hp = (int)integer();
            bar();
            n_loss_items = read_items(loss_items);
            bar();
            up_failed = (int)integer(); up_do_rewards = (int)integer();
            bar();
            up_failed_calls = (int)integer();
            skip_line();
            n_up_outro = 1;
        }
        tag("E");
        (void)status; (void)ai; (void)up_chosen; (void)battle_double; (void)enc_mode; (void)up_hp; (void)up_do_rewards; (void)sprite0;

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
            mon->b.fusion_species = 0;
            mon->b.level = (uint8_t)setup[m].level;
            mon->b.status = 0;
            mon->b.friendship = (uint8_t)setup[m].friendship;
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
        run.mystery_type = GC_ME_THE_EXPERT_POKEMON_BREEDER;
        memset(&run.screen, 0, sizeof run.screen);
        run.screen.kind = GC_SCREEN_MYSTERY;
        run.screen.mystery_type = GC_ME_THE_EXPERT_POKEMON_BREEDER;
        { gc_event e; while (gc_run_next_event(&run, &e)) {} }
        gc_mystery_setup_screen(&run);
        CHECK(run.screen.n_options == 3, "%d options", run.screen.n_options);
        for (int o = 0; o < 3; o++) {
            CHECK(run.screen.option_ok[o] == (meets[o] != 0), "option %d enabled %d, upstream %d", o, run.screen.option_ok[o], meets[o]);
            EQ("option takes no pick", run.screen.option_pick[o], 0);
        }
        CHECK(same_rng(&run.run.rng, &after_init), "onInit moved the run RNG (c %.0f, upstream %.0f)", run.run.rng.c, after_init.c);
        EQ("onInit messages", init_msgs, 0);

        /* the three offered Pokemon and the eggs each earns */
        int cpick[3] = {-1, -1, -1};
        const bool got = gc_breeder_picks(&run, cpick);
        EQ("three Pokemon offered", got, 1);
        if (got) {
            for (int k = 0; k < 3; k++) {
                EQ("pick", uid(&run.party[cpick[k]]), picks[k]);
                int c, r;
                gc_breeder_eggs(&run.party[cpick[k]], &c, &r);
                EQ("common eggs", c, commons[k]);
                EQ("rare eggs", r, rares[k]);
                eggs_checked += 2;
            }
        }
        {   /* the dialogue tokens: pokemonNName and, for each, its Common and Rare egg counts (an egg token exists upstream only for a count above 0) */
            const gc_screen *sc = &run.screen;
            static const char *const names[3] = {"pokemon1Name", "pokemon2Name", "pokemon3Name"};
            static const char *const cn[3] = {"pokemon1CommonEggs", "pokemon2CommonEggs", "pokemon3CommonEggs"};
            static const char *const rn[3] = {"pokemon1RareEggs", "pokemon2RareEggs", "pokemon3RareEggs"};
            for (int k = 0; k < 3; k++) {
                int have_name = 0, species = -1, cval = -1, rval = -1;
                for (int a = 0; a < sc->n_me_args; a++) {
                    if (!sc->me_args[a].name) continue;
                    if (!strcmp(sc->me_args[a].name, names[k])) { have_name = 1; species = (int)(sc->me_args[a].v >> 16); }
                    if (!strcmp(sc->me_args[a].name, cn[k])) cval = (int)sc->me_args[a].v;
                    if (!strcmp(sc->me_args[a].name, rn[k])) rval = (int)sc->me_args[a].v;
                }
                EQ("name token", have_name, token_present[k * 3 + 1]);
                EQ("name token names", species, setup[picks[k]].species);
                EQ("common egg token", cval, commons[k]);
                EQ("rare egg token", rval, rares[k]);
                EQ("common egg text exists", commons[k] > 0, token_present[k * 3]);
                EQ("rare egg text exists", rares[k] > 0, token_present[k * 3 + 2]);
                {   /* the text upstream is "<count>_<Rarity>_Egg(s)", "-" for none */
                    char want[48];
                    if (commons[k] > 0) { snprintf(want, sizeof want, "%d_Common_Egg%s", commons[k], commons[k] == 1 ? "" : "s"); CHECK(!strcmp(up_egg_text[k * 2 + 1], want), "upstream's common egg text is %s, expected %s", up_egg_text[k * 2 + 1], want); }
                    else CHECK(!strcmp(up_egg_text[k * 2 + 1], "-"), "upstream has a common egg text for none");
                    if (rares[k] > 0) { snprintf(want, sizeof want, "%d_Rare_Egg%s", rares[k], rares[k] == 1 ? "" : "s"); CHECK(!strcmp(up_egg_text[k * 2], want), "upstream's rare egg text is %s, expected %s", up_egg_text[k * 2], want); }
                    else CHECK(!strcmp(up_egg_text[k * 2], "-"), "upstream has a rare egg text for none");
                }
                texts_checked += 4;
            }
        }

        /* the option: the fight with the chosen Pokemon alone */
        bool ok = false;
        const char *w2 = "";
        gc_mystery_execute(&run, option, 0, 0, &w2, &ok);
        CHECK(ok, "option refused: %s", w2);
        CHECK(same_rng(&run.run.rng, &after_option), "the run RNG after the option differs (c %.0f, upstream %.0f)", run.run.rng.c, after_option.c);
        for (int e = 0; e < 3; e++) for (int k = 0; k < 4; k++) { char nm[48]; snprintf(nm, sizeof nm, "member %d generated move %d", e, k); EQ(nm, gc_breeder_generated[e][k], generated[e][k]); moves_checked++; }
        EQ("money unchanged", (long long)run.money, (long long)money_after);
        EQ("rewards set", n_rewards, 1);
        EQ("rewards fill the rest", fill, 1);
        EQ("one reward function", n_funcs, 1);
        CHECK(!strcmp(func0, "SOOTHE_BELL"), "upstream's reward is %s", func0);
        EQ("no reward options", n_given, 0);
        EQ("no reward tiers", n_tiers, 0);
        EQ("leave calls", leave_calls, 0);
        EQ("has the cleanup callback", has_cb, 1);
        EQ("has an onGameOver", has_game_over, 1);
        {
            const gc_internal *in = gc_int(&run);
            EQ("reward active", in->me_reward.active, 1);
            EQ("reward fill", in->me_reward.fill, 1);
            EQ("reward has no single item", in->me_reward.has_item, 0);
            EQ("reward function is the Soothe Bell", in->me_reward.func, gc_reward_item_index("SOOTHE_BELL"));
            EQ("reward callback before the shop", in->me_reward.before, 1);
            EQ("battle effects", in->n_me_start, n_effects);
        }
        CHECK(run.n_enemy == 3 && run.in_battle && run.me_battle, "no battle (%d enemies)", run.n_enemy);
        EQ("trainer battle", run.battle_type, BATTLE_TYPE_TRAINER);
        EQ("single battle", run.double_battle, up_double);
        EQ("trainer type", run.trainer_type, up_trainer);
        EQ("trainer type (config)", run.trainer_type, trainer_type);
        EQ("trainer variant", run.trainer_variant == TRAINER_VARIANT_FEMALE, up_variant == 1);
        EQ("config female unset", up_female, -1);
        female += run.trainer_variant == TRAINER_VARIANT_FEMALE;
        EQ("enemy levels", n_levels, 3);
        EQ("enemy items", run.n_enemy_items, n_enemy_items);
        if (biome == BIOME_ID_SPACE) space++;
        for (int e = 0; e < 3 && e < run.n_enemy; e++) {
            const gc_mon *m = &run.enemy[e];
            enemy_checked++;
            char nm[48];
            EQ("species", m->b.species, up[e].species);
            EQ("form", m->b.form, up[e].form);
            EQ("level", m->b.level, up[e].level);
            EQ("level is the battle's", m->b.level, levels[e]);
            EQ("boss", m->b.is_boss, up[e].segments ? 1 : 0);
            EQ("segments", m->b.boss_segments, up[e].segments);
            for (int k = 0; k < 6; k++) { snprintf(nm, sizeof nm, "enemy %d iv %d", e, k); EQ(nm, m->b.ivs[k], up[e].ivs[k]); }
            EQ("nature", m->has_custom_nature ? m->base_nature : m->b.nature, up[e].nature);
            EQ("ability index", m->ability_index, up[e].ability);
            EQ("passive flag", m->b.passive_flag, up[e].passive);
            EQ("shiny", m->b.shiny, up[e].shiny);
            EQ("variant", m->variant, up[e].variant);
            EQ("gender", m->b.gender, up[e].gender);
            EQ("friendship", m->b.friendship, up[e].friendship);
            EQ("id", m->b.id, up[e].id);
            EQ("max hp", m->b.stats[0], up[e].stats[0]);
            EQ("hp", m->b.hp, up[e].hp);
            for (int k = 0; k < 6; k++) { snprintf(nm, sizeof nm, "enemy %d stat %d", e, k); EQ(nm, m->b.stats[k], up[e].stats[k]); }
            EQ("move count", m->b.n_moves, up[e].n_moves);
            for (int k = 0; k < up[e].n_moves && k < 4; k++) { snprintf(nm, sizeof nm, "enemy %d move %d", e, k); EQ(nm, m->b.moves[k], up[e].moves[k]); moves_checked++; }
            EQ("pokeball", m->pokeball, x3[e].ball);
            EQ("tera type", m->b.tera_type, x3[e].tera);
            EQ("ability", pr_get_ability(&m->b, false), x3[e].ability);
            EQ("luck", m->luck, x3[e].luck);
            EQ("trainer slot", m->trainer_slot, x3[e].slot);
        }
        for (int k = 0; k < n_enemy_items; k++) {
            bool found = false;
            for (int j = 0; j < run.n_enemy_items; j++) {
                const gc_item *it = &run.enemy_items[j];
                found = found || (!strcmp(it->key, enemy_rows[k].key) && it->arg == enemy_rows[k].arg && it->stack == enemy_rows[k].stack && enemy_rows[k].holder >= 0 && enemy_rows[k].holder < 3 && it->pokemon_id == run.enemy[enemy_rows[k].holder].b.id);
            }
            CHECK(found, "enemy lacks item %s/%d x%d on %d", enemy_rows[k].key, enemy_rows[k].arg, enemy_rows[k].stack, enemy_rows[k].holder);
            items_checked++;
        }
        {   /* trainerAI.instantTeras: the Cleffa's slot (0) and nothing else; teraMode INSTANT_TERA */
            int8_t teras[8];
            const int nt = gc_trainer_instant_teras(&run, teras, 8);
            EQ("the instant teras", nt, 1);
            EQ("it is the Cleffa", nt == 1 && teras[0] == 0, strcmp(tera_slots, "0") == 0);
            (void)tera_mode;
        }

        /* the party during the fight: the chosen Pokemon alone */
        {
            char got_order[48], want[48];
            party_order_string(got_order, sizeof got_order);
            want_order_string(want, sizeof want, fight_party, fight_party_n);
            CHECK(!strcmp(got_order, want), "during the fight the party is [%s], upstream [%s]", got_order, want);
            EQ("the party is the chosen Pokemon alone", run.n_party, 1);
            EQ("the chosen Pokemon", uid(&run.party[run.party_order[0]]), picks[option]);
            check_items("during the fight", fight_items, n_fight_items);
            items_checked += n_fight_items;
        }

        if (path == 0) {
            /* the win: the rewards callback runs (the chosen Pokemon gets friendship, the others come back), the shop, then the eggs and the outro */
            gc_event e;
            while (gc_run_next_event(&run, &e)) {}
            EQ("friendship before the win", run.party[run.party_order[0]].b.friendship, up_friend_before);
            gc_me_win_rewards(&run);
            int shop = 0, post = 0, post_b = -1, before_rewards = 0;
            for (gc_rphase p; gc_rq_next(&gc_int(&run)->rq, &p);) {
                if (p.type == RP_ME_BEFORE_REWARDS) { before_rewards++; gc_phase_start(&run, &p); }
                if (p.type == RP_SELECT_MODIFIER) shop += p.a == 2;
                if (p.type == RP_ME_POST) { post++; post_b = p.b; gc_phase_start(&run, &p); }
            }
            EQ("the callback runs", before_rewards, 1);
            EQ("the shop is the item shop", shop, 1);
            EQ("one outro", post, 1);
            EQ("it is the plain outro", post_b, 0);
            EQ("the party is back", run.n_party, up_len);
            char got_order[48], want[48];
            party_order_string(got_order, sizeof got_order);
            want_order_string(want, sizeof want, up_order, up_len);
            CHECK(!strcmp(got_order, want), "after the win the party is [%s], upstream [%s]", got_order, want);
            EQ("friendship after the win", run.party[run.party_order[0]].b.friendship, up_friend_after);
            check_items("after the win", win_items, n_win_items);
            items_checked += n_win_items;
            EQ("not failed", up_failed, 0);
            wins++;
        } else {
            gc_mon *chosen = &run.party[run.party_order[0]];
            const int cid = uid(chosen);
            EQ("friendship before the loss", chosen->b.friendship, up_friend_before);
            chosen->b.hp = 0;
            { gc_event e; while (gc_run_next_event(&run, &e)) {} }
            gc_rq_reset(&gc_int(&run)->rq);
            gc_rphase over = gc_rp(RP_GAME_OVER, 0, 0, 0);
            const bool continued = gc_phase_start(&run, &over);
            EQ("the run goes on", continued, strcmp(up_result, "0") == 0);
            EQ("the party is back", run.n_party, up_len);
            char got_order[48], want[48];
            party_order_string(got_order, sizeof got_order);
            want_order_string(want, sizeof want, up_order, up_len);
            CHECK(!strcmp(got_order, want), "after the loss the party is [%s], upstream [%s]", got_order, want);
            for (int k = 0; k < run.n_party; k++) if (uid(&run.party[run.party_order[k]]) == cid) {
                EQ("the chosen Pokemon's friendship after the loss", run.party[run.party_order[k]].b.friendship, up_friend_after);
                EQ("it stays fainted", run.party[run.party_order[k]].b.hp, 0);
            }
            check_items("after the loss", loss_items, n_loss_items);
            items_checked += n_loss_items;
            EQ("failed flag", gc_int(&run)->breeder.failed, up_failed);
            EQ("rewards cleared", gc_int(&run)->me_reward.active, 0);
            EQ("handleMysteryEncounterBattleFailed called", up_failed_calls, 1);
            EQ("one outro text upstream", n_up_outro, 1);
            int heal = 0, shop = 0, post = 0, battle_end = 0, post_b = -1;
            for (gc_rphase p; gc_rq_next(&gc_int(&run)->rq, &p);) {
                if (p.type == RP_SELECT_MODIFIER) { heal += p.a == 1; shop += p.a == 2; }
                if (p.type == RP_BATTLE_END) battle_end++;
                if (p.type == RP_ME_POST) { post++; post_b = p.b; gc_phase_start(&run, &p); }
            }
            EQ("the heal shop", heal, 1);
            EQ("no item shop", shop, 0);
            EQ("the battle ends once", battle_end, 1);
            EQ("one outro", post, 1);
            EQ("it is the failed outro", post_b, 1);
            losses++;
        }
        /* the texts the outro said */
        {
            int n_msg = 0, n_gained = 0;
            gc_event ev;
            int counts[4] = {0}, tiers[4] = {0};
            while (gc_run_next_event(&run, &ev)) {
                if (ev.kind != GC_EV_MESSAGE || !ev.msg.key) continue;
                if (!strncmp(ev.msg.key, "mysteryEncounters/theExpertPokemonBreeder:", 42)) {
                    n_msg++;
                    if (!strcmp(ev.msg.key + 42, "gainedEggs") && n_gained < 4) {
                        for (int a = 0; a < ev.msg.n_args; a++) {
                            if (!ev.msg.args[a].name) continue;
                            if (!strcmp(ev.msg.args[a].name, "count")) counts[n_gained] = (int)ev.msg.args[a].v;
                            if (!strcmp(ev.msg.args[a].name, "rarity")) tiers[n_gained] = ev.msg.args[a].s && strstr(ev.msg.args[a].s, "great") ? 1 : 0;
                        }
                        n_gained++;
                    }
                }
            }
            /* the outro dialogue upstream: the speaker's line, then one line per egg kind; the failed one has the one line */
            EQ("outro messages", n_msg, path == 0 ? n_outro : 1);
            if (path == 0) {
                const int pc = commons[option], pr_ = rares[option];
                EQ("egg messages", n_gained, (pc > 0) + (pr_ > 0));
                int k = 0;
                if (pc > 0) { EQ("common egg count said", counts[k], pc); EQ("common egg rarity said", tiers[k], 0); k++; }
                if (pr_ > 0) { EQ("rare egg count said", counts[k], pr_); EQ("rare egg rarity said", tiers[k], 1); }
                texts_checked += 4;
            }
        }
        if (path == 0) {
            /* the eggs: Common ones, then Rare ones, from the event source */
            int c_eggs[2] = {0, 0}, up_eggs[2] = {0, 0}, bad_source = 0;
            for (int k = 0; k < sys.n_eggs; k++) { c_eggs[sys.eggs[k].tier == 0 ? 0 : 1]++; bad_source += sys.eggs[k].source_type != 4; }
            for (int k = 0; k < n_eggs; k++) { up_eggs[egg_tier[k] == 0 ? 0 : 1]++; bad_source += egg_source[k] != 4; }
            EQ("common eggs given", c_eggs[0], up_eggs[0]);
            EQ("rare eggs given", c_eggs[1], up_eggs[1]);
            EQ("the eggs are from an event", bad_source, 0);
            eggs_checked += 3;
        } else {
            EQ("no eggs on a loss", sys.n_eggs, 0);
        }
        if (bad) mismatches++;
        cases++; by_option[option]++;
    }
    printf("Mystery encounter The Expert Pokemon Breeder: %d cases run (options %d/%d/%d, wins %d, losses %d, Space %d, female trainer %d), %d enemies compared, %d items and %d moves compared, "
           "%d egg counts, %d texts, %d fields. TOTAL mismatches %d\n",
           cases, by_option[0], by_option[1], by_option[2], wins, losses, space, female, enemy_checked, items_checked, moves_checked, eggs_checked, texts_checked, fields_checked, mismatches);
    return mismatches ? 1 : 0;
}
