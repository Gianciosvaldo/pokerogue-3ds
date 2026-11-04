/* Uncommon Breed against upstream at 8555c08: replays the "-breed" trace of tests/mystery-breed.producer.test.ts (the real encounter: onInit in executeWithSeedOffset(.., waveIndex),
 * onOptionPhase in executeWithSeedOffset(.., getSeedOffset() * 500), initBattleWithEnemyConfig really run for the fight, catchPokemon recorded and completed with the real
 * Pokemon.addToParty, the Pokemon's post-summon effect called by hand) through game-core. Usage: mystery_breed_diff_test <trace>; exit 0 only with 0 mismatches. */
#include "../src/internal.h"
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
static void tag(char want)
{
    char c = '?';
    if (fscanf(f, " %c", &c) != 1 || c != want) { fprintf(stderr, "wrong record: wanted %c, got %c\n", want, c); exit(2); }
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
static int read_items(item_row *rows, int holder_field)
{
    const int n = (int)integer();
    if (n < 0 || n > 120) { fputs("item count\n", stderr); exit(2); }
    for (int k = 0; k < n; k++) {
        word(rows[k].key, sizeof rows[k].key);
        rows[k].arg = (int)integer();
        rows[k].holder = holder_field ? (int)integer() : -1;
        rows[k].stack = (int)integer();
    }
    return n;
}

static int bad, shown, case_no, case_option, case_wave;
#define CHECK(cond, ...) do { if (!(cond)) { bad++; if (shown < 14) { shown++; fprintf(stderr, "case %d (option %d wave %d): ", case_no, case_option, case_wave); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } } while (0)
static int fields_checked;
#define EQ(name, c_val, up_val) do { fields_checked++; CHECK((long long)(c_val) == (long long)(up_val), "%s is %lld, upstream %lld", name, (long long)(c_val), (long long)(up_val)); } while (0)

typedef struct { int species, form, level, segments, ivs[6], nature, ability, passive, shiny, variant, gender, friendship; long long id; int luck, hp, stats[6], n_moves, moves[4], tera; } boss_row;
static void read_boss(boss_row *b, bool is_x)
{
    b->species = (int)integer(); b->form = (int)integer(); b->level = (int)integer();
    if (is_x) integer();   /* isBoss */
    b->segments = (int)integer();
    for (int k = 0; k < 6; k++) b->ivs[k] = (int)integer();
    b->nature = (int)integer(); b->ability = (int)integer(); b->passive = (int)integer(); b->shiny = (int)integer(); b->variant = (int)integer();
    b->gender = (int)integer(); b->friendship = (int)integer(); b->id = integer();
    b->luck = is_x ? 0 : (int)integer();
    b->hp = (int)integer();
    if (is_x) integer();   /* max HP: stats[0] */
    for (int k = 0; k < 6; k++) b->stats[k] = (int)integer();
    b->n_moves = (int)integer();
    for (int k = 0; k < b->n_moves && k < 4; k++) b->moves[k] = (int)integer();
    b->tera = is_x ? 0 : (int)integer();
}




static void rec(const char *want)
{
    char w[16];
    word(w, sizeof w);
    if (strcmp(w, want)) { fprintf(stderr, "wrong record: wanted %s, got %s\n", want, w); exit(2); }
}

/* the items of the party, as "key arg holder stack" rows */
static bool has_row(const item_row *rows, int n, const gc_run *run, const char *key, int arg, uint32_t holder, int stack)
{
    (void)rows; (void)n;
    for (int k = 0; k < run->n_items; k++) {
        const gc_item *it = &run->items[k];
        if (!strcmp(it->key, key) && it->arg == arg && it->pokemon_id == holder && it->stack + it->virtual_stack == stack) return true;
    }
    return false;
}

static int count_msgs(gc_run *r, const char *prefix, char out[][96], int cap)
{
    int n = 0;
    gc_event e;
    while (gc_run_next_event(r, &e)) {
        if (e.kind != GC_EV_MESSAGE || !e.msg.key) continue;
        if (prefix && strncmp(e.msg.key, prefix, strlen(prefix))) continue;
        if (n < cap) snprintf(out[n], 96, "%s", e.msg.key);
        n++;
    }
    return n;
}

int main(int argc, char **argv)
{
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_breed_diff_test <trace>\n", stderr); return 2; }
    tag('V');
    integer();
    integer();
    int n_mons = 0;
    int cases = 0, mismatches = 0, skipped = 0, by_option[3] = {0}, items_checked = 0, moves_checked = 0, enemy_checked = 0, caught_checked = 0, stages_checked = 0, ivs_raised = 0, berries_taken = 0;
    int egg_self = 0, egg_attack = 0, primary_checked = 0;
    char c;
    while (fscanf(f, " %c", &c) == 1) {
        if (c == 'Z') break;
        if (c == 'P') {
            n_mons = (int)integer();
            if (n_mons < 1 || n_mons > 6) { fputs("party size\n", stderr); return 2; }
            for (int m = 0; m < n_mons * 2; m++) integer();
            continue;
        }
        if (c != 'C' || !n_mons) { fprintf(stderr, "wrong record %c\n", c); return 2; }
        char run_seed[64], wave_seed[64];
        case_no = (int)integer();
        word(run_seed, sizeof run_seed); word(wave_seed, sizeof wave_seed);
        const int wave = (int)integer(), option = (int)integer(), biome = (int)integer();
        const int trainer_id = (int)integer(), secret_id = (int)integer();
        const double money_before = (double)integer();
        case_option = option; case_wave = wave;
        struct { int species, form, level, nature, ivs[6], hp, max, stats[6], n_moves, moves[4], luck, ability; } setup[6];
        tag('U');
        for (int m = 0; m < n_mons; m++) {
            if (m) semicolon();
            setup[m].species = (int)integer(); setup[m].form = (int)integer(); setup[m].level = (int)integer(); setup[m].nature = (int)integer();
            for (int k = 0; k < 6; k++) setup[m].ivs[k] = (int)integer();
            setup[m].hp = (int)integer(); setup[m].max = (int)integer();
            for (int k = 0; k < 6; k++) setup[m].stats[k] = (int)integer();
            setup[m].n_moves = (int)integer();
            for (int k = 0; k < setup[m].n_moves; k++) setup[m].moves[k] = (int)integer();
            setup[m].luck = (int)integer();
            setup[m].ability = (int)integer();
        }
        tag('B');
        int balls[5];
        for (int k = 0; k < 5; k++) balls[k] = (int)integer();
        tag('L');
        static item_row before_items[120], boss_rows[120], after_items[120];
        const int n_before = read_items(before_items, 1);
        tag('A');
        const int meets[3] = {(int)integer(), (int)integer(), (int)integer()};
        const int up_primary3 = (int)integer();
        {
            char t = '?';
            if (fscanf(f, " %c", &t) != 1) return 2;
            if (t == 'E') { skipped++; continue; }
            ungetc(t, f);
        }
        tag('R');
        const pr_rng before = rng_vals();
        tag('I');
        const pr_rng after_init = rng_vals();
        tag('W');
        const int init_msgs = (int)integer(), up_egg = (int)integer();
        char up_name[64];
        word(up_name, sizeof up_name);
        rec("X0");
        boss_row first;
        read_boss(&first, true);
        tag('Q');
        (void)rng_vals();
        const pr_rng after_option = rng_vals();
        tag('T');
        const int n_rewards = (int)integer(), fill = (int)integer(), n_funcs = (int)integer(), n_given = (int)integer(), n_tiers = (int)integer();
        const int leave_calls = (int)integer(), leave_heal = (int)integer(), has_cb = (int)integer();
        rec("X2");
        const int n_exp = (int)integer();
        int exp_idx = -1, exp_wave = 0;
        double exp_base = 0;
        for (int k = 0; k < n_exp; k++) {
            if (k) semicolon();
            const int idx = (int)integer();
            const double base = real();
            const int w = (int)integer();
            if (k < 1) { exp_idx = idx; exp_base = base; exp_wave = w; }
        }
        tag('M');
        const int n_said = (int)integer();
        for (int k = 0; k < n_said; k++) { char tmp[96]; word(tmp, sizeof tmp); }
        tag('O');
        (void)integer();   /* the option's primary Pokemon */
        const double money_after = (double)integer();
        tag('G');
        const int n_caught = (int)integer();
        boss_row pre, post;
        int c_ball_null = 0, c_type = 0, c_show = 0, c_obtain = 0, c_added = 0, c_pokeball = 0;
        for (int k = 0; k < n_caught; k++) {
            if (k) semicolon();
            read_boss(&pre, true);
            semicolon();
            c_ball_null = (int)integer(); c_type = (int)integer(); c_show = (int)integer(); c_obtain = (int)integer(); c_added = (int)integer();
            if (c_added) { c_pokeball = (int)integer(); read_boss(&post, true); }
        }
        tag('V');
        const int n_after = read_items(after_items, 1);
        boss_row up;
        memset(&up, 0, sizeof up);
        int st_effect = 0, post_flag = 0, n_effects = 0, dbl = 0, n_levels = 0, levels[2] = {0, 0}, n_boss_items = 0, n_post = 0, n_post_said = 0;
        char post_effect[96] = "", post_said[96] = "";
        int fx[4][5];
        static int boss_item_enemy[120];
        if (option == 0) {
            tag('D');
            integer();
            dbl = (int)integer();
            n_levels = (int)integer();
            for (int k = 0; k < n_levels; k++) { const int l = (int)integer(); if (k < 2) levels[k] = l; }
            const int n_queued = (int)integer();
            { char tmp[64]; for (int k = 0; k < n_queued; k++) word(tmp, sizeof tmp); }
            tag('F'); n_effects = (int)integer();
            for (int k = 0; k < n_effects && k < 4; k++) {
                if (k) semicolon();
                for (int j = 0; j < 5; j++) fx[k][j] = (int)integer();
            }
            tag('J');
            n_boss_items = (int)integer();
            for (int k = 0; k < n_boss_items; k++) {
                boss_item_enemy[k] = (int)integer();
                word(boss_rows[k].key, sizeof boss_rows[k].key);
                boss_rows[k].arg = (int)integer(); boss_rows[k].stack = (int)integer();
            }
            tag('X'); read_boss(&up, true);
            tag('N'); st_effect = (int)integer(); (void)integer(); post_flag = (int)integer(); (void)integer();
            tag('K'); (void)rng_vals(); (void)rng_vals(); n_post = (int)integer();
            for (int k = 0; k < n_post && k < 1; k++) word(post_effect, sizeof post_effect);
            n_post_said = (int)integer();
            for (int k = 0; k < n_post_said && k < 1; k++) word(post_said, sizeof post_said);
        }
        tag('E');

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
            mon->b.n_moves = (uint8_t)setup[m].n_moves;
            memset(mon->b.moves, 0, sizeof mon->b.moves);
            memset(mon->b.pp_used, 0, sizeof mon->b.pp_used);
            for (int k = 0; k < setup[m].n_moves; k++) mon->b.moves[k] = (uint16_t)setup[m].moves[k];
            for (int k = 0; k < 6; k++) mon->b.ivs[k] = (uint8_t)setup[m].ivs[k];
            mon->base_nature = (uint8_t)setup[m].nature;
            mon->has_custom_nature = false;
            mon->b.nature = (uint8_t)setup[m].nature;
            mon->luck = (int8_t)setup[m].luck;
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
        for (int k = 0; k < 5; k++) run.pokeballs[k] = (uint8_t)balls[k];
        run.n_enemy = 0;
        run.in_battle = false;
        run.me_battle = false;
        run.battle_type = BATTLE_TYPE_WILD;
        pr_seed_from_ascii(&run.run.wave_seed, wave_seed);
        run.run.rng = before;
        gc_rq_reset(&gc_int(&run)->rq);
        run.mystery_type = GC_ME_UNCOMMON_BREED;
        memset(&run.screen, 0, sizeof run.screen);
        run.screen.kind = GC_SCREEN_MYSTERY;
        run.screen.mystery_type = GC_ME_UNCOMMON_BREED;
        { gc_event e; while (gc_run_next_event(&run, &e)) {} }
        gc_mystery_setup_screen(&run);
        CHECK(run.screen.n_options == 3, "%d options", run.screen.n_options);
        for (int o = 0; o < 3; o++) CHECK(run.screen.option_ok[o] == (meets[o] != 0), "option %d enabled %d, upstream %d", o, run.screen.option_ok[o], meets[o]);
        CHECK(same_rng(&run.run.rng, &after_init), "onInit moved the run RNG (c %.0f, upstream %.0f)", run.run.rng.c, after_init.c);
        EQ("onInit messages", init_msgs, 0);
        if (meets[2]) {
            const int p = gc_mystery_primary_pokemon(&run, GC_ME_UNCOMMON_BREED, 2);
            EQ("third option's primary Pokemon", p >= 0 ? (int)(run.party[p].b.id - 0x4000) : -1, up_primary3);
            primary_checked++;
        }
        {
            /* the dialogue tokens: enemyPokemon, and option3PrimaryName / option3PrimaryMove when the third option is on */
            const gc_screen *sc = &run.screen;
            bool enemy_token = false, primary_token = false, move_token = false;
            int enemy_species = -1, primary_species = -1;
            for (int a = 0; a < sc->n_me_args; a++) {
                if (!sc->me_args[a].name) continue;
                if (!strcmp(sc->me_args[a].name, "enemyPokemon")) { enemy_token = true; enemy_species = (int)(sc->me_args[a].v >> 16); }
                if (!strcmp(sc->me_args[a].name, "option3PrimaryName")) { primary_token = true; primary_species = (int)(sc->me_args[a].v >> 16); }
                if (!strcmp(sc->me_args[a].name, "option3PrimaryMove")) move_token = true;
            }
            EQ("enemyPokemon token", enemy_token, 1);
            EQ("enemyPokemon token names", enemy_species, first.species);
            EQ("third option's primary token", primary_token, meets[2] != 0);
            EQ("third option's move token", move_token, meets[2] != 0);
            if (meets[2] && primary_token) EQ("third option's primary token names", primary_species, setup[up_primary3].species);
        }

        /* onInit's Pokemon */
        {
            gc_mon rolled;
            int egg = -1;
            if (!gc_breed_roll(&run, &rolled, &egg)) { CHECK(0, "no Pokemon rolled (the species or biome is not ported)"); }
            else {
                const gc_mon *e = &rolled;
                enemy_checked++;
                EQ("rolled species", e->b.species, first.species);
                EQ("rolled form", e->b.form, first.form);
                EQ("rolled level", e->b.level, first.level);
                EQ("rolled level is highest - 2", e->b.level, ({ int hi = 0; for (int m = 0; m < n_mons; m++) if (setup[m].level > hi) hi = setup[m].level; hi - 2; }));
                EQ("rolled bars", e->b.boss_segments, first.segments);
                for (int k = 0; k < 6; k++) { char nm[32]; snprintf(nm, sizeof nm, "rolled iv %d", k); EQ(nm, e->b.ivs[k], first.ivs[k]); }
                EQ("rolled nature", e->has_custom_nature ? e->base_nature : e->b.nature, first.nature);
                EQ("rolled ability index", e->ability_index, first.ability);
                EQ("rolled passive flag", e->b.passive_flag, first.passive);
                EQ("rolled shiny", e->b.shiny, first.shiny);
                EQ("rolled variant", e->variant, first.variant);
                EQ("rolled gender", e->b.gender, first.gender);
                EQ("rolled friendship", e->b.friendship, first.friendship);
                EQ("rolled id", e->b.id, first.id);
                EQ("rolled max hp", e->b.stats[0], first.stats[0]);
                EQ("rolled hp", e->b.hp, first.hp);
                for (int k = 0; k < 6; k++) { char nm[32]; snprintf(nm, sizeof nm, "rolled stat %d", k); EQ(nm, e->b.stats[k], first.stats[k]); }
                EQ("rolled move count", e->b.n_moves, first.n_moves);
                for (int k = 0; k < first.n_moves && k < 4; k++) { char nm[32]; snprintf(nm, sizeof nm, "rolled move %d", k); EQ(nm, e->b.moves[k], first.moves[k]); moves_checked++; }
                EQ("egg move", egg, up_egg);
                if (up_egg) {
                    bool known = false;
                    for (int k = 0; k < e->b.n_moves; k++) known = known || e->b.moves[k] == up_egg;
                    CHECK(known, "the egg move %d is not in the moveset", up_egg);
                    if (!strcmp(pr_moves[up_egg].kind, "SelfStatusMove")) egg_self++; else egg_attack++;
                }
            }
        }

        bool ok = false;
        const char *w2 = "";
        gc_mystery_execute(&run, option, 0, 0, &w2, &ok);
        CHECK(ok, "option refused: %s", w2);
        CHECK(same_rng(&run.run.rng, &after_option), "the run RNG after the option differs (c %.0f, upstream %.0f)", run.run.rng.c, after_option.c);
        EQ("money unchanged", (long long)run.money, (long long)money_after);
        {
            /* the encounter's own texts come only from the battle: the Pokemon's post-summon message, with the Pokemon named as the dialogue token names it */
            int n_stat_boost = 0, n_other = 0;
            gc_event e;
            while (gc_run_next_event(&run, &e)) {
                if (e.kind == GC_EV_BATTLE) {
                    for (int j = e.a; j < e.a + e.b; j++) {
                        const pr_event *pe = &run.battle_events[j];
                        if (pe->type != PR_EV_TEXT || !pe->msg.key || strcmp(pe->msg.key, "mysteryEncounters/uncommonBreed:option.1.statBoost")) continue;
                        n_stat_boost++;
                        int species = -1, kind = -1;
                        for (int a = 0; a < pe->msg.n_args; a++) if (pe->msg.args[a].name && !strcmp(pe->msg.args[a].name, "enemyPokemon")) { species = (int)(pe->msg.args[a].v >> 16); kind = pe->msg.args[a].kind; }
                        EQ("stat boost message names the Pokemon", species, up.species);
                        EQ("stat boost message has the plain name", kind, PR_ARG_POKEMON_NAME);
                    }
                    continue;
                }
                if (e.kind == GC_EV_MESSAGE && e.msg.key && !strncmp(e.msg.key, "mysteryEncounters/uncommonBreed:", 32)) n_other++;
            }
            EQ("stat boost message count", n_stat_boost, option == 0 ? n_post_said : 0);
            if (option == 0) CHECK(!strcmp(post_said, "mysteryEncounters/uncommonBreed:option.1.statBoost"), "upstream says %s", post_said);
            EQ("no other text", n_other, 0);
        }

        /* the shop, the experience and the catch */
        int heal = 0, shop = 0, catches = 0;
        double got_exp = -1;
        for (gc_rphase p; gc_rq_next(&gc_int(&run)->rq, &p);) {
            if (p.type == RP_SELECT_MODIFIER) { heal += p.a == 1; shop += p.a == 2; }
            if (p.type == RP_EXP && (int)run.party[p.a].b.id - 0x4000 == exp_idx) got_exp = p.d;
            if (p.type == RP_CATCH) {
                catches++;
                EQ("the catch is of the first enemy", p.a, 0);
                EQ("the catch says no text", p.c, 2);
                /* the Pokemon about to be caught is the one upstream hands to catchPokemon */
                const gc_mon *e = &run.enemy[p.a];
                EQ("caught species", e->b.species, pre.species);
                EQ("caught level", e->b.level, pre.level);
                for (int k = 0; k < 6; k++) { char nm[32]; snprintf(nm, sizeof nm, "caught iv %d", k); EQ(nm, e->b.ivs[k], pre.ivs[k]); }
                EQ("caught nature", e->has_custom_nature ? e->base_nature : e->b.nature, pre.nature);
                EQ("caught id", e->b.id, pre.id);
                EQ("caught move count", e->b.n_moves, pre.n_moves);
                for (int k = 0; k < pre.n_moves && k < 4; k++) { char nm[32]; snprintf(nm, sizeof nm, "caught move %d", k); EQ(nm, e->b.moves[k], pre.moves[k]); moves_checked++; }
                const bool done = gc_catch_start(&run, &p);
                CHECK(done, "the catch asked a question with %d in the party", n_mons);
            }
        }
        const gc_internal *in = gc_int(&run);
        if (option == 0) {
            EQ("rewards set", n_rewards, 1);
            EQ("rewards fill the rest", fill, 1);
            EQ("no reward functions", n_funcs, 0);
            EQ("no reward options", n_given, 0);
            EQ("no reward tiers", n_tiers, 0);
            EQ("no callback", has_cb, 0);
            EQ("leave calls", leave_calls, 0);
            EQ("reward active", in->me_reward.active, 1);
            EQ("reward fill", in->me_reward.fill, 1);
            EQ("reward has no single item", in->me_reward.has_item, 0);
            EQ("reward function", in->me_reward.func, -1);
            EQ("no experience", n_exp, 0);
            EQ("fleeing is off", in->me_no_flee, 1);
            EQ("catching is allowed", run.me_catch_allowed, 1);
            CHECK(run.n_enemy == 1 && run.in_battle && run.me_battle, "no battle (%d enemies)", run.n_enemy);
            EQ("single battle", run.double_battle, 0);
            EQ("single battle upstream", dbl, 0);
            EQ("enemy levels", n_levels, 1);
            EQ("battle effects", in->n_me_start, n_effects);
            for (int k = 0; k < n_effects && k < in->n_me_start && k < 4; k++) {
                EQ("effect source", in->me_start[k].source, fx[k][0]);
                EQ("effect target", in->me_start[k].target, fx[k][2]);
                EQ("effect move", in->me_start[k].move, fx[k][3]);
                EQ("effect use mode", in->me_start[k].use_mode, fx[k][4]);
            }
            EQ("enemy items", run.n_enemy_items, n_boss_items);
            const gc_mon *e = &run.enemy[0];
            enemy_checked++;
            EQ("species", e->b.species, up.species);
            /* Schooling (Wishiwashi) and Flower Gift (Cherrim) change form as the battle starts, which the oracle (no battle) does not play */
            const bool shifting = up.species == 746 || up.species == 421;
            if (!shifting) EQ("form", e->b.form, up.form);
            EQ("level", e->b.level, up.level);
            (void)levels;
            EQ("not a boss", e->b.is_boss, up.segments ? 1 : 0);
            EQ("segments", e->b.boss_segments, up.segments);
            for (int k = 0; k < 6; k++) { char nm[32]; snprintf(nm, sizeof nm, "iv %d", k); EQ(nm, e->b.ivs[k], up.ivs[k]); }
            EQ("nature", e->has_custom_nature ? e->base_nature : e->b.nature, up.nature);
            EQ("ability index", e->ability_index, up.ability);
            EQ("passive flag", e->b.passive_flag, up.passive);
            EQ("shiny", e->b.shiny, up.shiny);
            EQ("variant", e->variant, up.variant);
            EQ("gender", e->b.gender, up.gender);
            EQ("friendship", e->b.friendship, up.friendship);
            EQ("id", e->b.id, up.id);
            if (!shifting) EQ("max hp", e->b.stats[0], up.stats[0]);
            if (!shifting) for (int k = 0; k < 6; k++) { char nm[32]; snprintf(nm, sizeof nm, "stat %d", k); EQ(nm, e->b.stats[k], up.stats[k]); }
            EQ("move count", e->b.n_moves, up.n_moves);
            for (int k = 0; k < up.n_moves && k < 4; k++) { char nm[32]; snprintf(nm, sizeof nm, "move %d", k); EQ(nm, e->b.moves[k], up.moves[k]); moves_checked++; }
            EQ("status", e->b.status, st_effect);
            EQ("post-summon tag upstream", post_flag, 1);
            EQ("post-summon flag spent", e->b.me_post_summon, PR_ME_POST_NONE);
            EQ("post-summon effects upstream", n_post, 1);
            /* the stat changes: Def, SpDef and Spd below wave 50, else Atk, Def, SpAtk, SpDef and Spd, one stage each */
            CHECK(!strcmp(post_effect, wave < 50 ? "StatStageChangePhase_2_1_4_1_5_1" : "StatStageChangePhase_1_1_2_1_3_1_4_1_5_1"), "upstream's post-summon effect is %s at wave %d", post_effect, wave);
            /* the egg move may move the stages of its user too (a stat-raising move, a move that lowers its own stats): they are compared only
             * when the egg move is an attack that changes none of the user's */
            const char *kind = up_egg ? pr_moves[up_egg].kind : "";
            const bool plain_attack = !up_egg || !strcmp(kind, "AttackMove");
            if (plain_attack) {
                /* the oracle plays no summon: Simple doubles the change and Contrary turns it round, as they do in the battle */
                const int ability = pr_get_ability(&e->b, false);
                const int mult = ability == ABILITY_ID_SIMPLE ? 2 : ability == ABILITY_ID_CONTRARY ? -1 : 1;
                const int want[5] = {wave >= 50, 1, wave >= 50, 1, 1};
                for (int s = 0; s < 5; s++) { char nm[32]; snprintf(nm, sizeof nm, "stage %d", s); EQ(nm, e->b.stat_stages[s], want[s] * mult); stages_checked++; }
            }
            for (int k = 0; k < n_boss_items; k++) {
                bool found = false;
                for (int j = 0; j < run.n_enemy_items; j++) {
                    const gc_item *it = &run.enemy_items[j];
                    found = found || (!strcmp(it->key, boss_rows[k].key) && it->arg == boss_rows[k].arg && it->stack == boss_rows[k].stack && it->pokemon_id == e->b.id);
                }
                CHECK(found, "enemy lacks item %s/%d x%d", boss_rows[k].key, boss_rows[k].arg, boss_rows[k].stack);
                items_checked++;
            }
            EQ("no catch yet", catches, 0);
            EQ("items unchanged", run.n_items, n_after);
        } else {
            EQ("rewards set", n_rewards, 1);
            EQ("rewards fill the rest", fill, 1);
            EQ("no reward functions", n_funcs, 0);
            EQ("no reward options", n_given, 0);
            EQ("no callback", has_cb, 0);
            EQ("leave calls", leave_calls, 1);
            EQ("leaves without the heal shop", leave_heal, 0);
            EQ("no heal shop", heal, 0);
            EQ("the item shop", shop, 1);
            EQ("shop fill", in->reward.me_custom.fill_remaining, 1);
            EQ("the Pokemon is caught", catches, 1);
            EQ("catchPokemon called once", n_caught, 1);
            EQ("no ball thrown", c_ball_null, 1);
            EQ("a Pokeball", c_type, 0);
            EQ("no catch text", c_show, 0);
            EQ("not an obtain", c_obtain, 0);
            EQ("the party had room", c_added, 1);
            EQ("party grew", run.n_party, n_mons + 1);
            if (run.n_party == n_mons + 1 && c_added) {
                const gc_mon *g = &run.party[run.party_order[n_mons]];
                caught_checked++;
                EQ("joined species", g->b.species, post.species);
                EQ("joined form", g->b.form, post.form);
                EQ("joined level", g->b.level, post.level);
                EQ("joined is no boss", g->b.is_boss, post.segments ? 1 : 0);
                for (int k = 0; k < 6; k++) { char nm[32]; snprintf(nm, sizeof nm, "joined iv %d", k); EQ(nm, g->b.ivs[k], post.ivs[k]); }
                EQ("joined nature", g->has_custom_nature ? g->base_nature : g->b.nature, post.nature);
                EQ("joined ability index", g->ability_index, post.ability);
                EQ("joined shiny", g->b.shiny, post.shiny);
                EQ("joined variant", g->variant, post.variant);
                EQ("joined gender", g->b.gender, post.gender);
                EQ("joined friendship", g->b.friendship, post.friendship);
                EQ("joined id", g->b.id, post.id);
                EQ("joined ball", g->pokeball, c_pokeball);
                EQ("joined max hp", g->b.stats[0], post.stats[0]);
                EQ("joined hp", g->b.hp, post.hp);
                for (int k = 0; k < 6; k++) { char nm[32]; snprintf(nm, sizeof nm, "joined stat %d", k); EQ(nm, g->b.stats[k], post.stats[k]); }
                EQ("joined move count", g->b.n_moves, post.n_moves);
                for (int k = 0; k < post.n_moves && k < 4; k++) { char nm[32]; snprintf(nm, sizeof nm, "joined move %d", k); EQ(nm, g->b.moves[k], post.moves[k]); moves_checked++; }
                EQ("joined is not on the field", g->b.on_field, 0);
            }
            if (option == 1) {
                EQ("no experience", n_exp, 0);
                EQ("items after the food", run.n_items, n_after);
                for (int k = 0; k < n_after && k < run.n_items; k++) {
                    const gc_item *it = &run.items[k];
                    const uint32_t holder = after_items[k].holder < 0 ? 0 : (uint32_t)(0x4000 + after_items[k].holder);
                    CHECK(!strcmp(it->key, after_items[k].key) && it->arg == after_items[k].arg && it->pokemon_id == holder && it->stack == after_items[k].stack,
                          "item %d is %s/%d x%d held by %d, upstream %s/%d x%d held by %d", k, it->key, it->arg, it->stack, (int)it->pokemon_id - 0x4000,
                          after_items[k].key, after_items[k].arg, after_items[k].stack, after_items[k].holder);
                    items_checked++;
                }
                berries_taken += n_after < n_before || 1;
            } else {
                EQ("experience set", n_exp, 1);
                EQ("experience Pokemon", exp_idx, up_primary3);
                EQ("experience value", exp_base, gc_form(pre.species, pre.form)->base_exp * (double)pre.level / 5 + 1);
                EQ("experience not scaled by the wave", exp_wave, 0);
                { pr_exp_party pp;
                  int idx[PR_MAX_EXP_PARTY]; double amt[PR_MAX_EXP_PARTY];
                  const uint32_t id = (uint32_t)(0x4000 + exp_idx);
                  gc_exp_party_build(&run, &pp, &id, 1);
                  pp.trainer_battle = false; pp.me_exp_multiplier = 1.0;
                  const int n = pr_run_apply_party_exp(&pp, exp_base, false, run.wave, idx, amt);
                  double want = 0;
                  for (int k = 0; k < n; k++) if (run.party[run.party_order[idx[k]]].b.id == id) want = amt[k];
                  EQ("experience the primary Pokemon gets", (long long)(got_exp < 0 ? 0 : got_exp), (long long)want); }
                EQ("items unchanged", run.n_items, n_after);
                for (int k = 0; k < 6; k++) ivs_raised += pre.ivs[k] > setup[0].ivs[0] - 99 && pre.ivs[k] > first.ivs[k];
            }
            CHECK(run.n_enemy <= 1, "enemies in an encounter without a battle");
        }
        for (int m = 0; m < n_mons; m++) {
            const gc_mon *mon = &run.party[st[m]];
            char nm[48];
            snprintf(nm, sizeof nm, "party %d hp", m); EQ(nm, mon->b.hp, setup[m].hp);
        }
        if (bad) mismatches++;
        cases++; by_option[option]++;
    }
    printf("Mystery encounter Uncommon Breed: %d cases run (battle %d, food %d, charm %d), %d skipped (option not available or a full party), %d Pokemon rolled, "
           "%d with a self-targeted egg move / %d with an attack one, %d primary Pokemon, %d caught Pokemon compared, %d IVs raised by the second roll, %d stage sets, %d items and %d moves compared, %d fields. TOTAL mismatches %d\n",
           cases, by_option[0], by_option[1], by_option[2], skipped, enemy_checked, egg_self, egg_attack, primary_checked, caught_checked, ivs_raised, stages_checked, items_checked, moves_checked, fields_checked, mismatches);
    (void)berries_taken;
    return mismatches ? 1 : 0;
}
