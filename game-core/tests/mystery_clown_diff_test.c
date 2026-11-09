/* Clowning Around against upstream at 8555c08: replays the "-clown" trace of tests/mystery-clown.producer.test.ts (the real encounter: onInit in
 * executeWithSeedOffset(.., waveIndex), onPreOptionPhase in (.., getSeedOffset()) and onOptionPhase in (.., getSeedOffset() * 500) really run: the fight (initBattleWithEnemyConfig),
 * the item shuffle and the type shuffle; setEncounterRewards and leaveEncounterWithoutBattle recorded; the fight's ability swap offer (onPostOptionPhase) answered by a script)
 * through game-core. The win is not played: it is what the won fight's phase does (gc_clown_victory, then the phases it queued).
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

static int bad, shown, case_no, case_option, case_wave;
#define CHECK(cond, ...) do { if (!(cond)) { bad++; if (shown < (getenv("SHOW") ? atoi(getenv("SHOW")) : 14)) { shown++; fprintf(stderr, "case %d (option %d wave %d): ", case_no, case_option, case_wave); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } } while (0)
static int fields_checked;
#define EQ(name, c_val, up_val) do { fields_checked++; CHECK((long long)(c_val) == (long long)(up_val), "%s is %lld, upstream %lld", name, (long long)(c_val), (long long)(up_val)); } while (0)

/* the fight starts inside the call here (upstream's harness stops before the first summon): Intimidate, when it is Blacephalon's rolled ability, lowers the player's Attack at once and
 * a held White Herb restores it and is used up, one stack at a time. `herb_used`: the White Herb stacks may be fewer than upstream's, nothing else may differ */
static void check_items(const char *what, const item_row *rows, int n, bool herb_used)
{
    char nm[96];
    int up_herbs = 0, herbs = 0;
    if (herb_used) {
        item_row kept[120];
        int nk = 0;
        for (int k = 0; k < n; k++) { if (!strcmp(rows[k].key, "WHITE_HERB")) up_herbs += rows[k].stack; else kept[nk++] = rows[k]; }
        int ck = 0;
        static gc_item mine[GC_MAX_ITEMS];
        for (int k = 0; k < run.n_items; k++) { if (!strcmp(run.items[k].key, "WHITE_HERB")) herbs += run.items[k].stack; else mine[ck++] = run.items[k]; }
        snprintf(nm, sizeof nm, "%s: item count without White Herbs", what);
        EQ(nm, ck, nk);
        for (int k = 0; k < nk && k < ck; k++) {
            const gc_item *it = &mine[k];
            const int holder = it->pokemon_id ? (int)(it->pokemon_id - 0x4000) : -1;
            CHECK(!strcmp(it->key, kept[k].key) && it->arg == kept[k].arg && holder == kept[k].holder && it->stack == kept[k].stack,
                  "%s: item %d is %s/%d holder %d x%d, upstream %s/%d holder %d x%d", what, k, it->key, it->arg, holder, it->stack, kept[k].key, kept[k].arg, kept[k].holder, kept[k].stack);
        }
        snprintf(nm, sizeof nm, "%s: White Herbs not more than upstream's", what);
        EQ(nm, herbs <= up_herbs, 1);
        return;
    }
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
    int enemy_checked = 0, moves_checked = 0, enemy_items_checked = 0, offers_yes = 0, offers_no = 0, ctor_checked = 0, names_seen[16] = {0};
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
        int rw_n = 0, rw_fill = 0, rw_funcs = 0, rw_tiers = 0, rw_options = 0;
        double rw_reroll = 0;
        int enc_mode = 0, battle_double = 0, n_levels = 0, levels[6] = {0}, up_trainer = 0, up_variant = 0, up_template = 0, up_name_idx = -1, up_partner_idx = -1;
        int n_effects = 0, eff_source[4] = {0}, eff_target[4] = {0}, eff_move[4] = {0}, eff_mode[4] = {0}, n_enemy_items = 0, n_x = 0;
        mon_row up[2];
        struct { int ball, tera, ability, luck, slot, fusion; char name[40]; } x3[2];
        int up_status[2] = {0, 0};
        int n_ctor = 0, ctor_species[2] = {0}, ctor_n[2] = {0}, ctor_moves[2][4] = {{0}};
        int cu_ability[2] = {-1, -1}, cu_types[2][2] = {{-1, -1}, {-1, -1}}, cu_n_types[2] = {0};
        int po_decision = 0, po_prompts = 0, po_threw = 0, po_n = 0, po_chosen = -1;
        char po_keys[6][48] = {{0}};
        int pa_ability[6] = {0}, pa_fusion[6] = {0};
        static item_row enemy_rows[120];
        if (option == 0) {
            tag("RW");
            rw_n = (int)integer(); rw_fill = (int)integer(); rw_funcs = (int)integer(); rw_tiers = (int)integer(); rw_options = (int)integer(); rw_reroll = real();
            tag("D");
            enc_mode = (int)integer(); battle_double = (int)integer(); n_levels = (int)integer();
            for (int k = 0; k < n_levels; k++) { const int l = (int)integer(); if (k < 6) levels[k] = l; }
            up_trainer = (int)integer(); up_variant = (int)integer(); up_template = (int)integer(); up_name_idx = (int)integer(); up_partner_idx = (int)integer();
            skip_line();
            tag("F");
            n_effects = (int)integer();
            for (int k = 0; k < n_effects; k++) {
                if (k) semicolon();
                char tg[16];
                eff_source[k] = (int)integer(); word(tg, sizeof tg); eff_target[k] = atoi(tg); eff_move[k] = (int)integer(); eff_mode[k] = (int)integer();
            }
            tag("J");
            n_enemy_items = (int)integer();
            for (int k = 0; k < n_enemy_items; k++) {
                enemy_rows[k].holder = (int)integer();
                word(enemy_rows[k].key, sizeof enemy_rows[k].key);
                enemy_rows[k].arg = (int)integer(); enemy_rows[k].stack = (int)integer();
            }
            tag("X");
            n_x = (int)integer();
            for (int k = 0; k < n_x && k < 2; k++) { if (k) semicolon(); read_mon(&up[k]); }
            tag("X3");
            integer();
            for (int k = 0; k < 2; k++) {
                if (k) semicolon();
                x3[k].ball = (int)integer(); x3[k].tera = (int)integer(); x3[k].ability = (int)integer(); x3[k].luck = (int)integer(); x3[k].slot = (int)integer(); x3[k].fusion = (int)integer();
                word(x3[k].name, sizeof x3[k].name);
            }
            tag("N");
            for (int k = 0; k < 2; k++) {
                if (k) semicolon();
                up_status[k] = (int)integer(); integer();
            }
            tag("G");
            n_ctor = (int)integer();
            for (int k = 0; k < n_ctor && k < 2; k++) {
                if (k) semicolon();
                ctor_species[k] = (int)integer(); ctor_n[k] = (int)integer();
                for (int j = 0; j < ctor_n[k]; j++) { const int mv = (int)integer(); if (j < 4) ctor_moves[k][j] = mv; }
            }
            tag("B");
            for (int k = 0; k < 2; k++) {
                if (k) semicolon();
                char tp[24];
                cu_ability[k] = (int)integer(); word(tp, sizeof tp);
                cu_n_types[k] = csv(tp, cu_types[k]);
            }
            tag("PO");
            po_decision = (int)integer(); po_prompts = (int)integer(); po_threw = (int)integer(); po_n = (int)integer();
            for (int k = 0; k < po_n; k++) { char key[48]; word(key, sizeof key); if (k < 6) snprintf(po_keys[k], sizeof po_keys[k], "%s", key); }
            po_chosen = (int)integer();
            tag("PA");
            for (int m = 0; m < n_mons; m++) {
                if (m) semicolon();
                char fa[16];
                pa_ability[m] = (int)integer(); word(fa, sizeof fa);
                pa_fusion[m] = !strcmp(fa, "-") ? -1 : atoi(fa);
            }
        }
        tag("E");
        (void)rng_pre; (void)after_pre; (void)up_ability_name; (void)enc_mode; (void)battle_double; (void)n_x; (void)rw_options; (void)po_threw;

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
            mon->b.custom_ability = ABILITY_ID_BALL_FETCH;   /* the producer gives every Pokemon this custom ability; a fused one reports its fusion half's */
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
        EQ("option takes no pick", run.screen.option_pick[option], 0);
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
        const gc_internal *in = gc_int(&run);
        int fight_post_b = -1, fight_post_c = -1;
        if (option == 0) {
            EQ("setEncounterRewards once", n_rewards, 1);
            EQ("no leave call", leave_calls, 0);
            EQ("the shop fills the rest", rw_fill, 1);
            EQ("no reward functions", rw_funcs, 0);
            EQ("no reward tiers", rw_tiers, 0);
            EQ("reroll multiplier unset", (long long)rw_reroll, 0);
            (void)leave_arg;
            CHECK(run.n_enemy == 2 && run.in_battle && run.me_battle, "no battle (%d enemies)", run.n_enemy);
            EQ("trainer battle", run.battle_type, BATTLE_TYPE_TRAINER);
            EQ("double battle", run.double_battle, 1);
            EQ("battle double (upstream)", battle_double, 1);
            EQ("trainer type", run.trainer_type, up_trainer);
            EQ("trainer type (config)", run.trainer_type, cfg_trainer);
            EQ("trainer variant (double)", run.trainer_variant == TRAINER_VARIANT_DOUBLE, up_variant == 2);
            EQ("party template", up_template, 0);
            EQ("trainer name", run.trainer_name, up_name_idx);
            EQ("no partner name", run.trainer_partner_name, up_partner_idx);
            EQ("enemy levels", n_levels, 2);
            EQ("enemy items", run.n_enemy_items, n_enemy_items);
            EQ("battle effects", in->n_me_start, n_effects);
            for (int k = 0; k < n_effects && k < in->n_me_start; k++) {
                EQ("effect source", in->me_start[k].source, eff_source[k]);
                EQ("effect target", in->me_start[k].target, eff_target[k]);
                EQ("effect move", in->me_start[k].move, eff_move[k]);
                EQ("effect ignores PP", in->me_start[k].use_mode == MOVE_USE_MODE_IGNORE_PP, eff_mode[k] != 0);
            }
            EQ("the fight is flagged", in->clown.active, 1);
            EQ("no catching", run.me_catch_allowed, 0);
            EQ("the encounter is over as an encounter", run.mystery_type, -1);
            EQ("the encounter does not prevent game stats", in->me_no_stats, 0);
            EQ("no shop of setEncounterRewards' own kind", in->me_reward.active, 0);
            EQ("the first turn is queued", gc_rq_has(&run, RP_TURN_INIT), 1);
            {
                int appeared = 0;
                gc_event e2;
                while (gc_run_next_event(&run, &e2)) if (e2.kind == GC_EV_MESSAGE && e2.msg.key && !strcmp(e2.msg.key, "battle:trainerAppearedDouble")) appeared++;
                EQ("the double trainer appears once", appeared, 1);
                texts_checked++;
            }
            EQ("the outro is pending", in->me_post.pending, 1);
            EQ("two constructor movesets", n_ctor, 2);
            if (up_name_idx >= 0 && up_name_idx < 16) names_seen[up_name_idx] = 1;
            for (int e = 0; e < 2 && e < run.n_enemy; e++) {
                const gc_mon *m = &run.enemy[e];
                enemy_checked++;
                char nm[48];
                EQ("species", m->b.species, up[e].species);
                EQ("species is the config's", m->b.species, e ? SPECIES_ID_BLACEPHALON : SPECIES_ID_MR_MIME);
                EQ("form", m->b.form, up[e].form);
                EQ("level", m->b.level, up[e].level);
                EQ("level is the battle's", m->b.level, levels[e]);
                EQ("boss", m->b.is_boss, up[e].boss);
                EQ("is a boss", m->b.is_boss, 1);
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
                EQ("ability", pr_get_ability(&m->b, true), x3[e].ability);
                EQ("luck", m->luck, x3[e].luck);
                EQ("trainer slot", m->trainer_slot, x3[e].slot);
                EQ("fusion", m->b.fusion_species, x3[e].fusion);
                EQ("status", m->b.status, up_status[e]);
                EQ("custom ability", m->b.custom_ability, cu_ability[e]);
                EQ("custom type count", (m->b.custom_types[0] || m->b.custom_types[1]) ? 2 : 0, cu_n_types[e]);
                for (int k = 0; k < cu_n_types[e] && k < 2; k++) { snprintf(nm, sizeof nm, "enemy %d custom type %d", e, k); EQ(nm, m->b.custom_types[k], cu_types[e][k] >= 0 ? cu_types[e][k] + 1 : 0); }
                /* the constructor's own moveset (generated, then replaced) */
                EQ("constructor species", up[e].species, ctor_species[e]);
                for (int k = 0; k < 4; k++) { snprintf(nm, sizeof nm, "enemy %d constructor move %d", e, k); EQ(nm, in->ctor_moves[e][k], k < ctor_n[e] ? ctor_moves[e][k] : 0); ctor_checked++; }
            }
            EQ("Blacephalon's ability is the rolled one", pr_get_ability(&run.enemy[1].b, true), up_ability);
            EQ("Blacephalon's first type", run.enemy[1].b.custom_types[0], up_first + 1);
            EQ("Blacephalon's second type", run.enemy[1].b.custom_types[1], up_second + 1);
            for (int k = 0; k < n_enemy_items; k++) {
                bool found = false;
                for (int j = 0; j < run.n_enemy_items; j++) {
                    const gc_item *it = &run.enemy_items[j];
                    found = found || (!strcmp(it->key, enemy_rows[k].key) && it->arg == enemy_rows[k].arg && it->stack == enemy_rows[k].stack && enemy_rows[k].holder >= 0 && enemy_rows[k].holder < 2 && it->pokemon_id == run.enemy[enemy_rows[k].holder].b.id);
                }
                CHECK(found, "enemy lacks item %s/%d x%d on %d", enemy_rows[k].key, enemy_rows[k].arg, enemy_rows[k].stack, enemy_rows[k].holder);
                enemy_items_checked++;
            }
            EQ("the player's party is whole", run.n_party, n_mons);
            check_items("after the option", after_items, n_after, up_ability == ABILITY_ID_INTIMIDATE);
            items_checked += n_after;
        } else {
            EQ("no setEncounterRewards", n_rewards, 0);
            EQ("one leave call", leave_calls, 1);
            EQ("leave with the heal shop", leave_arg, 1);
            EQ("no fight", in->clown.active, 0);
            check_items("after the option", after_items, n_after, false);
            items_checked += n_after;
        }
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
        (void)fight_post_b; (void)fight_post_c;

        if (option != 0) {
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
            int heal_shop = 0, post = 0, outro = 0, waited = 0;
            for (gc_rphase p; gc_rq_next(&gc_int(&run)->rq, &p);) {
                if (p.type == RP_SELECT_MODIFIER && p.a == 1) heal_shop++;
                if (p.type == RP_ME_POST) {
                    post++;
                    waited = !gc_phase_start(&run, &p);
                    gc_event ev;
                    while (gc_run_next_event(&run, &ev)) if (ev.kind == GC_EV_MESSAGE && ev.msg.key && !strcmp(ev.msg.key, "mysteryEncounters/clowningAround:outro")) outro++;
                }
            }
            EQ("the heal shop", heal_shop, 1);
            EQ("one post phase", post, 1);
            EQ("the post phase of a shuffle does not wait", waited, 0);
            EQ("the outro", outro, 1);
            texts_checked++;
        }
        } else {
            /* the win: BattleEndPhase, TrainerVictoryPhase, the shop that fills itself, the post phase (the ability offer, then the outro), the next wave */
            gc_event ev;
            while (gc_run_next_event(&run, &ev)) {}
            gc_clown_victory(&run);
            EQ("the fight is over", in->clown.active, 0);
            int battle_end = 0, trainer_victory = 0, shop = 0, order_ok = 1, seq = 0, post = 0;
            gc_rphase post_phase = {0};
            for (gc_rphase p; gc_rq_next(&gc_int(&run)->rq, &p);) {
                if (p.type == RP_BATTLE_END) { battle_end++; order_ok &= seq == 0 && p.a == 1; seq = 1; }
                if (p.type == RP_TRAINER_VICTORY) { trainer_victory++; order_ok &= seq == 1; seq = 2; }
                if (p.type == RP_SELECT_MODIFIER) { shop += p.a == 2; order_ok &= seq == 2; seq = 3; }
                if (p.type == RP_ME_POST) { post++; post_phase = p; order_ok &= seq == 3; seq = 4; }
            }
            EQ("the battle ends once", battle_end, 1);
            EQ("TrainerVictoryPhase runs", trainer_victory, 1);
            EQ("the item shop", shop, 1);
            EQ("one post phase", post, 1);
            EQ("phases in handleMysteryEncounterVictory's order", order_ok, 1);
            const pr_reward_custom *c = &gc_int(&run)->reward.me_custom;
            EQ("shop: fill the rest", c->fill_remaining, rw_fill);
            EQ("shop: no functions", c->n_funcs, 0);
            EQ("shop: no tiers", c->n_tiers, 0);
            EQ("shop: no options", c->n_options, 0);
            EQ("shop: the reroll multiplier is unset", isnan(c->reroll_multiplier), 1);
            EQ("shop: luck upgrades allowed", c->allow_luck_upgrades, 1);
            EQ("post phase is Clowning Around's fight", post_phase.a == GC_ME_CLOWNING_AROUND && post_phase.b == 0, 1);
            EQ("post phase carries the ability", post_phase.c, up_ability);

            /* onPostOptionPhase: handleSwapAbility */
            while (gc_run_next_event(&run, &ev)) {}
            const bool waits = !gc_phase_start(&run, &post_phase);
            EQ("the ability offer waits for an answer", waits, 1);
            EQ("the stage", run.screen.me_stage, GC_ME_STAGE_ABILITY);
            EQ("two choices", run.screen.n_options, 2);
            EQ("Yes asks for a party member", run.screen.option_pick[0], GC_PICK_MEMBER);
            EQ("No asks nothing", run.screen.option_pick[1], GC_PICK_NONE);
            EQ("both answers are on", run.screen.option_ok[0] && run.screen.option_ok[1], 1);
            EQ("the stage is Clowning Around's", run.screen.mystery_type, GC_ME_CLOWNING_AROUND);
            EQ("any Pokemon can be picked", gc_mystery_pick_ok(&run, GC_ME_CLOWNING_AROUND, 0, 0) && gc_mystery_pick_ok(&run, GC_ME_CLOWNING_AROUND, 0, n_mons - 1), 1);
            EQ("nobody out of the party can be picked", gc_mystery_pick_ok(&run, GC_ME_CLOWNING_AROUND, 0, n_mons), 0);
            {
                int have_ability = 0, have_name = 0;
                for (int a = 0; a < run.screen.n_me_args; a++) {
                    if (!run.screen.me_args[a].name) continue;
                    if (!strcmp(run.screen.me_args[a].name, "ability")) have_ability = run.screen.me_args[a].kind == PR_ARG_ABILITY && (int)run.screen.me_args[a].v == up_ability;
                    if (!strcmp(run.screen.me_args[a].name, "blacephalonName")) have_name = run.screen.me_args[a].s && !strcmp(run.screen.me_args[a].s, "pokemon:blacephalon");
                }
                EQ("the stage's ability token", have_ability, 1);
                EQ("the stage's blacephalonName token", have_name, 1);
            }
            /* the dialogue so far: upstream says the dialogue, the message and (once) the prompt */
            int said = 0;
            static const char *const want[3] = {"option.1.applyAbilityDialogue", "option.1.applyAbilityMessage", "option.1.abilityPrompt"};
            while (gc_run_next_event(&run, &ev)) {
                if (ev.kind != GC_EV_MESSAGE || !ev.msg.key) continue;
                char full[96];
                snprintf(full, sizeof full, "mysteryEncounters/clowningAround:%s", said < 3 ? want[said] : "-");
                CHECK(said < 3 && !strcmp(ev.msg.key, full) && !strcmp(po_keys[said], want[said]), "dialogue %d is %s, upstream %s", said, ev.msg.key, po_keys[said < 6 ? said : 5]);
                if (said == 1 || said == 2) {
                    int token_ok = 0;
                    for (int a = 0; a < ev.msg.n_args; a++)
                        if (ev.msg.args[a].name && !strcmp(ev.msg.args[a].name, "ability")) token_ok = ev.msg.args[a].kind == PR_ARG_ABILITY && (int)ev.msg.args[a].v == up_ability;
                    EQ("the offer's ability token", token_ok, 1);
                }
                said++;
            }
            EQ("three offer texts", said, 3);
            texts_checked += 3;

            /* the answer */
            gc_choice ch = {GC_CH_MYSTERY, (int16_t)(po_decision == 0 ? 1 : 0), 0, 0};
            int pick_idx = -1;
            if (po_decision != 0) {
                /* the chosen party member: upstream's index in the party array, the storage is rotated */
                for (int m = 0; m < n_mons; m++) if (po_chosen == m) pick_idx = st[m];
                ch.b = (int16_t)pick_idx;
            }
            bool ans_ok = false;
            const char *why2 = "";
            const bool done = gc_phase_choose(&run, &post_phase, &ch, &why2, &ans_ok);
            CHECK(ans_ok && done, "the answer was refused: %s", why2);
            EQ("the screen is gone", run.screen.kind == GC_SCREEN_MYSTERY, 0);
            int gained = 0, outro = 0, chosen_ok = 0, gained_ability_ok = 0;
            int order2 = 0;
            while (gc_run_next_event(&run, &ev)) {
                if (ev.kind != GC_EV_MESSAGE || !ev.msg.key) continue;
                if (!strcmp(ev.msg.key, "mysteryEncounters/clowningAround:option.1.abilityGained")) {
                    gained++;
                    order2 = order2 == 0 ? 1 : -1;
                    for (int a = 0; a < ev.msg.n_args; a++) {
                        if (!ev.msg.args[a].name) continue;
                        if (!strcmp(ev.msg.args[a].name, "chosenPokemon")) chosen_ok = po_chosen >= 0 && (int)(ev.msg.args[a].v >> 16) == setup[po_chosen].species;
                        if (!strcmp(ev.msg.args[a].name, "ability")) gained_ability_ok = ev.msg.args[a].kind == PR_ARG_ABILITY && (int)ev.msg.args[a].v == up_ability;
                    }
                }
                if (!strcmp(ev.msg.key, "mysteryEncounters/clowningAround:outro")) { outro++; order2 = order2 == 1 ? 2 : (order2 == 0 ? 2 : -1); }
            }
            EQ("abilityGained said iff yes", gained, po_decision != 0);
            EQ("the outro", outro, 1);
            EQ("abilityGained before the outro", order2 == 2 || po_decision == 0, 1);
            EQ("one prompt upstream (no backing out scripted)", po_prompts, 1);
            if (po_decision != 0) {
                EQ("abilityGained names the Pokemon", chosen_ok, 1);
                EQ("abilityGained names the ability", gained_ability_ok, 1);
                offers_yes++;
            } else offers_no++;
            texts_checked += 2;
            for (int m = 0; m < n_mons; m++) {
                const gc_mon *mon = &run.party[st[m]];
                char nm[48];
                snprintf(nm, sizeof nm, "Pokemon %d custom ability", m);
                EQ(nm, mon->b.custom_ability, pa_ability[m]);
                snprintf(nm, sizeof nm, "Pokemon %d fusion custom ability", m);
                EQ(nm, mon->b.fusion_custom_ability, pa_fusion[m]);
                snprintf(nm, sizeof nm, "Pokemon %d ability", m);
                if (!setup[m].fusion) { EQ(nm, pr_get_ability(&mon->b, false), pa_ability[m]); EQ("cached ability", mon->b.ability, pa_ability[m]); }   /* a fused Pokemon's own ability is its fusion half's unless the swap set one */
                else if (pa_fusion[m] >= 0) EQ(nm, pr_get_ability(&mon->b, false), pa_fusion[m]);
            }
        }
        if (bad) mismatches++;
        cases++; by_option[option]++;
    }
    int names = 0;
    for (int k = 0; k < 16; k++) names += names_seen[k];
    printf("Mystery encounter Clowning Around: %d cases run (fight %d with %d ability offers taken and %d declined, item shuffle %d, type shuffle %d; %d abilities rolled, %d fused Pokemon), "
           "%d enemies (%d moves, %d constructor moves, %d enemy items, %d trainer names) and %d items compared, %d types compared, %d texts, %d fields. TOTAL mismatches %d\n",
           cases, by_option[0], offers_yes, offers_no, by_option[1], by_option[2], abilities_seen, fused, enemy_checked, moves_checked, ctor_checked, enemy_items_checked, names,
           items_checked, types_checked, texts_checked, fields_checked, mismatches);
    return mismatches ? 1 : 0;
}
