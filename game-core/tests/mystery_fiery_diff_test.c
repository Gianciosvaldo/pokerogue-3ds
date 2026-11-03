/* Fiery Fallout against upstream at 8555c08: replays the "-fiery" trace of tests/mystery-fiery.producer.test.ts (the real encounter: onInit with no
 * enemy on the field, onOptionPhase in executeWithSeedOffset(.., getSeedOffset() * 500), initBattleWithEnemyConfig really run, the callback of
 * setEncounterRewards run by hand in executeWithSeedOffset(.., waveIndex * 1000), the Volcarona's post-summon effect called by hand) through game-core.
 *
 * Per case, compared with what upstream did: the starting stats; which options are offered and the third option's primary Pokemon (the first Fire
 * type of the party, fainted or not, else the first with a fire resistant ability that can fight); the weather onInit leaves (Sunny without a turn
 * count, kept when already Sunny, the "continues" text for a primal weather) and its messages; the run RNG after onInit and after the option;
 * the fight: the two Volcarona (levels, ids, IVs, nature, ability, gender, moves, stats, items), the double battle, no fleeing, the Fire Spin
 * effects from each Volcarona at the matching Pokemon, the rewards set and the lead's booster of the callback (its item, its stack, its text);
 * hunkering down: the 20% damage to every Pokemon that can fight and is no Fire type, the one burned (the roll, canSetStatus), its ability, its
 * text; calming them: the experience of the primary Pokemon and the booster. Usage: mystery_fiery_diff_test <trace>; exit 0 only with 0 mismatches. */
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
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_fiery_diff_test <trace>\n", stderr); return 2; }
    tag('V');
    integer();
    integer();
    int n_mons = 0;
    int cases = 0, mismatches = 0, skipped = 0, by_option[3] = {0}, items_checked = 0, moves_checked = 0, fainted_primary = 0, burned = 0, burn_refused = 0, cured_on_summon = 0, damaged = 0;
    int weather_set = 0, weather_same = 0, weather_primal = 0, lead_stacked = 0, ability_primary = 0, enemy_checked = 0;
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
        tag('H');
        int hps[6];
        for (int m = 0; m < n_mons; m++) hps[m] = (int)integer();
        tag('S');
        int sts[6];
        for (int m = 0; m < n_mons; m++) sts[m] = (int)integer();
        tag('B');
        int balls[5];
        for (int k = 0; k < 5; k++) balls[k] = (int)integer();
        tag('L');
        static item_row before_items[120], boss_rows[120], after_items[120], cb_items[120];
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
        const int w_before = (int)integer(), w_before_turns = (int)integer(), w_before_max = (int)integer();
        const int w_after = (int)integer(), w_after_turns = (int)integer(), w_after_max = (int)integer();
        const int init_msgs = (int)integer();
        (void)integer();   /* the animation phases */
        tag('Q');
        (void)rng_vals();
        const pr_rng after_option = rng_vals();
        tag('T');
        const int n_rewards = (int)integer(), fill = (int)integer(), n_funcs = (int)integer(), n_given = (int)integer(), n_tiers = (int)integer();
        const int leave_calls = (int)integer(), leave_heal = (int)integer(), has_cb = (int)integer();
        rec("X2");
        const int n_exp = (int)integer();
        int exp_idx = -1, exp_base = 0, exp_wave = 0;
        for (int k = 0; k < n_exp; k++) {
            if (k) semicolon();
            const int idx = (int)integer(), base = (int)integer(), w = (int)integer();
            if (k < 1) { exp_idx = idx; exp_base = base; exp_wave = w; }
        }
        tag('M');
        const int n_said = (int)integer();
        char said[6][96];
        for (int k = 0; k < n_said && k < 6; k++) word(said[k], sizeof said[k]);
        tag('O');
        (void)integer();   /* the option's primary Pokemon */
        const double money_after = (double)integer();
        tag('Y');
        int up_hp[6], up_max[6], up_stats[6][6], up_status[6], up_ability[6];
        for (int m = 0; m < n_mons; m++) {
            if (m) semicolon();
            up_hp[m] = (int)integer(); up_max[m] = (int)integer();
            for (int k = 0; k < 6; k++) up_stats[m][k] = (int)integer();
            up_status[m] = (int)integer(); up_ability[m] = (int)integer(); (void)integer();
        }
        tag('V');
        const int n_after = read_items(after_items, 1);
        rec("CB");
        int cb_present = 0, n_cb_said = 0, n_cb_items = 0;
        pr_rng cb_r0 = {0}, cb_r1 = {0};
        char cb_said[2][96];
        if (has_cb) {
            cb_present = 1;
            cb_r0 = rng_vals(); cb_r1 = rng_vals();
            n_cb_said = (int)integer();
            for (int k = 0; k < n_cb_said && k < 2; k++) word(cb_said[k], sizeof cb_said[k]);
            n_cb_items = read_items(cb_items, 1);
        }
        boss_row up[2];
        memset(up, 0, sizeof up);
        int st_effect[2] = {0, 0}, post_flag[2] = {0, 0}, n_effects = 0, dbl = 0, n_levels = 0, levels[2] = {0, 0}, n_boss_items = 0, n_post[2] = {0, 0};
        char post[2][96];
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
            for (int n = 0; n < 2; n++) {
                tag('X'); read_boss(&up[n], true);
                tag('N'); st_effect[n] = (int)integer(); (void)integer(); post_flag[n] = (int)integer(); (void)integer();
                tag('K'); (void)rng_vals(); (void)rng_vals(); n_post[n] = (int)integer();
                for (int k = 0; k < n_post[n] && k < 1; k++) word(post[n], sizeof post[n]);
            }
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
            mon->b.status = (uint8_t)sts[m];
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
            mon->b.custom_ability = (int16_t)setup[m].ability;   /* the ability upstream resolved (its own, or a custom one), as a Pokemon with a lookup reads it */
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
            mon->b.hp = hps[m];
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
        run.bt.weather = (uint8_t)w_before;
        run.bt.weather_turns_left = (int16_t)w_before_turns;
        run.bt.weather_max_duration = (int16_t)w_before_max;
        gc_rq_reset(&gc_int(&run)->rq);
        run.mystery_type = GC_ME_FIERY_FALLOUT;
        memset(&run.screen, 0, sizeof run.screen);
        run.screen.kind = GC_SCREEN_MYSTERY;
        run.screen.mystery_type = GC_ME_FIERY_FALLOUT;
        { gc_event e; while (gc_run_next_event(&run, &e)) {} }
        gc_mystery_setup_screen(&run);
        CHECK(run.screen.n_options == 3, "%d options", run.screen.n_options);
        for (int o = 0; o < 3; o++) CHECK(run.screen.option_ok[o] == (meets[o] != 0), "option %d enabled %d, upstream %d", o, run.screen.option_ok[o], meets[o]);
        CHECK(same_rng(&run.run.rng, &after_init), "onInit moved the run RNG (c %.0f, upstream %.0f)", run.run.rng.c, after_init.c);
        EQ("weather after onInit", run.bt.weather, w_after);
        EQ("weather turns left", run.bt.weather_turns_left, w_after_turns);
        EQ("weather duration", run.bt.weather_max_duration, w_after_max);
        {
            char got[4][96];
            EQ("onInit messages", count_msgs(&run, NULL, got, 4), init_msgs);
        }
        if (meets[2]) {
            const int p = gc_fiery_primary(&run);
            EQ("third option's primary Pokemon", p >= 0 ? (int)(run.party[p].b.id - 0x4000) : -1, up_primary3);
        }
        {
            /* the dialogue tokens: volcaronaName, and option3PrimaryName when the third option is on */
            const gc_screen *sc = &run.screen;
            bool volcarona = false, primary_token = false;
            int primary_species = -1;
            for (int a = 0; a < sc->n_me_args; a++) {
                if (!sc->me_args[a].name) continue;
                if (!strcmp(sc->me_args[a].name, "volcaronaName") && sc->me_args[a].kind == PR_ARG_KEY && sc->me_args[a].s && !strcmp(sc->me_args[a].s, "pokemon:volcarona")) volcarona = true;
                if (!strcmp(sc->me_args[a].name, "option3PrimaryName")) { primary_token = true; primary_species = (int)(sc->me_args[a].v >> 16); }
            }
            EQ("Volcarona token", volcarona, 1);
            EQ("third option's primary token", primary_token, meets[2] != 0);
            if (meets[2] && primary_token) EQ("third option's primary token names", primary_species, setup[up_primary3].species);
        }
        weather_same += w_before == 1;
        weather_primal += w_before == 7 || w_before == 8 || w_before == 9;
        weather_set += w_before != 1 && w_before != 7 && w_before != 8 && w_before != 9;

        bool ok = false;
        const char *w2 = "";
        gc_mystery_execute(&run, option, 0, 0, &w2, &ok);
        CHECK(ok, "option refused: %s", w2);
        CHECK(same_rng(&run.run.rng, &after_option), "the run RNG after the option differs (c %.0f, upstream %.0f)", run.run.rng.c, after_option.c);
        EQ("money unchanged", (long long)run.money, (long long)money_after);
        {
            char got[6][96];
            const int n_got = count_msgs(&run, "mysteryEncounters/fieryFallout:", got, 6);
            /* option 3 shows the booster's text at once (the rewards phase of the encounter that leaves); a won fight shows it after the battle */
            int n_expect = n_said;
            if (option == 2) n_expect += n_cb_said;
            EQ("text count", n_got, n_expect);
            for (int k = 0; k < n_said && k < n_got && k < 6; k++) { CHECK(!strcmp(said[k], got[k]), "text %d is %s, upstream %s", k, got[k], said[k]); fields_checked++; }
        }

        /* the shop and the experience */
        int heal = 0, shop = 0;
        double got_exp = -1;
        for (gc_rphase p; gc_rq_next(&gc_int(&run)->rq, &p);) {
            if (p.type == RP_SELECT_MODIFIER) { heal += p.a == 1; shop += p.a == 2; }
            if (p.type == RP_EXP && (int)run.party[p.a].b.id - 0x4000 == exp_idx) got_exp = p.d;
        }
        const gc_internal *in = gc_int(&run);
        if (option == 0) {
            EQ("rewards set", n_rewards, 1);
            EQ("rewards fill the rest", fill, 1);
            EQ("no reward functions", n_funcs, 0);
            EQ("no reward options", n_given, 0);
            EQ("no reward tiers", n_tiers, 0);
            EQ("a callback", has_cb, 1);
            EQ("leave calls", leave_calls, 0);
            EQ("reward active", in->me_reward.active, 1);
            EQ("reward fill", in->me_reward.fill, 1);
            EQ("reward has no single item", in->me_reward.has_item, 0);
            EQ("reward function", in->me_reward.func, -1);
            EQ("reward callback", in->me_reward.before, 1);
            EQ("lead item pending", in->fiery_item, 1);
            EQ("no experience", n_exp, 0);
            EQ("fleeing is off", in->me_no_flee, 1);
            EQ("catching is allowed", run.me_catch_allowed, 1);
            CHECK(run.n_enemy == 2 && run.in_battle && run.me_battle, "no battle (%d enemies)", run.n_enemy);
            EQ("double battle", run.double_battle, 1);
            EQ("double battle upstream", dbl, 1);
            EQ("enemy levels", n_levels, 2);
            EQ("battle effects", in->n_me_start, n_effects);
            for (int k = 0; k < n_effects && k < in->n_me_start && k < 4; k++) {
                EQ("effect source", in->me_start[k].source, fx[k][0]);
                EQ("effect target", in->me_start[k].target, fx[k][2]);
                EQ("effect move", in->me_start[k].move, fx[k][3]);
                EQ("effect use mode", in->me_start[k].use_mode, fx[k][4]);
            }
            EQ("enemy items", run.n_enemy_items, n_boss_items);
            for (int n = 0; n < 2; n++) {
                const gc_mon *e = &run.enemy[n];
                enemy_checked++;
                EQ("species", e->b.species, up[n].species);
                EQ("form", e->b.form, up[n].form);
                EQ("level", e->b.level, up[n].level);
                EQ("level of the battle", e->b.level, levels[n]);
                EQ("not a boss", e->b.is_boss, 0);
                EQ("segments", e->b.boss_segments, up[n].segments);
                for (int k = 0; k < 6; k++) { char nm[32]; snprintf(nm, sizeof nm, "iv %d", k); EQ(nm, e->b.ivs[k], up[n].ivs[k]); }
                EQ("nature", e->has_custom_nature ? e->base_nature : e->b.nature, up[n].nature);
                EQ("ability index", e->ability_index, up[n].ability);
                EQ("passive flag", e->b.passive_flag, up[n].passive);
                EQ("shiny", e->b.shiny, up[n].shiny);
                EQ("variant", e->variant, up[n].variant);
                EQ("gender", e->b.gender, up[n].gender);
                EQ("gender of the config", e->b.gender, n);
                EQ("friendship", e->b.friendship, up[n].friendship);
                EQ("id", e->b.id, up[n].id);
                EQ("max hp", e->b.stats[0], up[n].stats[0]);
                EQ("hp", e->b.hp, up[n].hp);
                for (int k = 0; k < 6; k++) { char nm[32]; snprintf(nm, sizeof nm, "stat %d", k); EQ(nm, e->b.stats[k], up[n].stats[k]); }
                EQ("move count", e->b.n_moves, up[n].n_moves);
                for (int k = 0; k < up[n].n_moves && k < 4; k++) { char nm[32]; snprintf(nm, sizeof nm, "move %d", k); EQ(nm, e->b.moves[k], up[n].moves[k]); moves_checked++; }
                EQ("status", e->b.status, st_effect[n]);
                EQ("post-summon tag upstream", post_flag[n], 1);
                /* the summon at the battle's start has run the effect: Sp. Def and Spd up one stage, nothing else (the oracle's Volcarona has no Simple or Contrary) */
                EQ("post-summon flag spent", e->b.me_post_summon, PR_ME_POST_NONE);
                EQ("Sp. Def stage", e->b.stat_stages[STAT_SPDEF - 1], 1);
                EQ("Spd stage", e->b.stat_stages[STAT_SPD - 1], 1);
                EQ("Def stage", e->b.stat_stages[STAT_DEF - 1], 0);
                EQ("Sp. Atk stage", e->b.stat_stages[STAT_SPATK - 1], 0);
                EQ("post-summon effects upstream", n_post[n], 1);
                CHECK(!strcmp(post[n], "StatStageChangePhase_4_1_5_1"), "upstream's post-summon effect is %s", post[n]);
                for (int k = 0; k < n_boss_items; k++) {
                    if (boss_item_enemy[k] != n) continue;
                    bool found = false;
                    for (int j = 0; j < run.n_enemy_items; j++) {
                        const gc_item *it = &run.enemy_items[j];
                        found = found || (!strcmp(it->key, boss_rows[k].key) && it->arg == boss_rows[k].arg && it->stack == boss_rows[k].stack && it->pokemon_id == e->b.id);
                    }
                    CHECK(found, "enemy %d lacks item %s/%d x%d", n, boss_rows[k].key, boss_rows[k].arg, boss_rows[k].stack);
                    items_checked++;
                }
            }
            /* the callback, as the rewards phase of the won fight runs it */
            { gc_event e; while (gc_run_next_event(&run, &e)) {} }
            EQ("items before the callback", run.n_items, n_after);
            gc_fiery_reward(&run);
        } else if (option == 1) {
            EQ("no rewards set", n_rewards, 0);
            EQ("no callback", has_cb, 0);
            EQ("leave calls", leave_calls, 1);
            EQ("leaves with the heal shop", leave_heal, 1);
            EQ("heal shop", heal, 1);
            EQ("no item shop", shop, 0);
            EQ("no experience", n_exp, 0);
            CHECK(run.n_enemy == 0, "an enemy in an encounter without a battle");
            EQ("items unchanged", run.n_items, n_after);
        } else {
            EQ("rewards set", n_rewards, 1);
            EQ("rewards fill the rest", fill, 1);
            EQ("no reward functions", n_funcs, 0);
            EQ("a callback", has_cb, 1);
            EQ("leave calls", leave_calls, 1);
            EQ("leaves without the heal shop", leave_heal, 0);
            EQ("no heal shop", heal, 0);
            EQ("the item shop", shop, 1);
            EQ("shop fill", in->reward.me_custom.fill_remaining, 1);
            EQ("experience set", n_exp, 1);
            EQ("experience Pokemon", exp_idx, up_primary3);
            EQ("base experience", gc_form(SPECIES_ID_VOLCARONA, 0)->base_exp * 2, exp_base);
            EQ("experience scaled by the wave", exp_wave, 1);
            { pr_exp_party pp; int idx[PR_MAX_EXP_PARTY]; double amt[PR_MAX_EXP_PARTY];
              const uint32_t id = (uint32_t)(0x4000 + exp_idx);
              gc_exp_party_build(&run, &pp, &id, 1);
              pp.trainer_battle = false; pp.me_exp_multiplier = 1.0;
              const int n = pr_run_apply_party_exp(&pp, exp_base, true, run.wave, idx, amt);
              double want = 0;
              for (int k = 0; k < n; k++) if (run.party[run.party_order[idx[k]]].b.id == id) want = amt[k];
              EQ("experience the primary Pokemon gets", (long long)(got_exp < 0 ? 0 : got_exp), (long long)want); }
            fainted_primary += setup[exp_idx].hp <= 0;
            ability_primary += exp_idx >= 0;
            CHECK(run.n_enemy == 0, "an enemy in an encounter without a battle");
        }
        if (has_cb) {
            /* the booster of the lead, rolled on the stream of the wave */
            { char got[4][96]; const int n_msgs = count_msgs(&run, "mysteryEncounters/fieryFallout:foundItem", got, 4);
              EQ("booster text", n_msgs, option == 0 ? n_cb_said : 0); }
            CHECK(same_rng(&run.run.rng, &after_option), "the callback moved the run RNG (c %.0f)", run.run.rng.c);
            (void)cb_r0; (void)cb_r1;
            EQ("item count after the callback", run.n_items, n_cb_items);
            for (int k = 0; k < n_cb_items; k++) {
                const uint32_t holder = cb_items[k].holder < 0 ? 0 : (uint32_t)(0x4000 + cb_items[k].holder);
                char held[96] = "";
                for (int j = 0; j < run.n_items; j++) if (!strcmp(run.items[j].key, "ATTACK_TYPE_BOOSTER") && run.items[j].pokemon_id == 0x4000) snprintf(held + strlen(held), sizeof held - strlen(held), " %d", run.items[j].arg);
                CHECK(has_row(cb_items, n_cb_items, &run, cb_items[k].key, cb_items[k].arg, holder, cb_items[k].stack),
                      "item %s/%d held by %d x%d is missing after the callback (the lead holds boosters of:%s)", cb_items[k].key, cb_items[k].arg, cb_items[k].holder, cb_items[k].stack, held);
                items_checked++;
            }
            for (int k = 0; k < n_before; k++) if (!strcmp(before_items[k].key, "ATTACK_TYPE_BOOSTER") && before_items[k].holder == 0) { lead_stacked++; break; }
        } else {
            EQ("item count", run.n_items, n_after);
        }

        for (int m = 0; m < n_mons; m++) {
            const gc_mon *mon = &run.party[st[m]];
            char nm[48];
            snprintf(nm, sizeof nm, "party %d max hp", m); EQ(nm, mon->b.stats[0], up_max[m]);
            for (int k = 0; k < 6; k++) { snprintf(nm, sizeof nm, "party %d stat %d", m, k); EQ(nm, mon->b.stats[k], up_stats[m][k]); }
            snprintf(nm, sizeof nm, "party %d hp", m); EQ(nm, mon->b.hp, up_hp[m]);
            /* the oracle never plays the battle's summon; the engine does, and Magma Armor / Water Veil / Water Bubble / Thermal Exchange cure a Freeze / Burn they find on
             * the Pokemon as it comes out (a player's Pokemon that was not benched and fights: the one status is simply gone) */
            snprintf(nm, sizeof nm, "party %d status", m);
            {
                const int ab = up_ability[m];
                const bool cured = option == 0 && ((ab == ABILITY_ID_MAGMA_ARMOR && up_status[m] == 5) || ((ab == ABILITY_ID_WATER_VEIL || ab == ABILITY_ID_WATER_BUBBLE || ab == ABILITY_ID_THERMAL_EXCHANGE) && up_status[m] == 6));
                if (cured && mon->b.status == 0) cured_on_summon++;
                else EQ(nm, mon->b.status, up_status[m]);
            }
            snprintf(nm, sizeof nm, "party %d ability", m); EQ(nm, pr_get_ability(&mon->b, false), up_ability[m]);
            if (option == 1) {
                burned += sts[m] == 0 && up_status[m] == 6;
                damaged += up_hp[m] < hps[m];
            }
        }
        if (option == 1) burn_refused += n_said == 0 && n_mons > 0;
        if (bad) mismatches++;
        cases++; by_option[option]++;
    }
    printf("Mystery encounter Fiery Fallout: %d cases run (fight %d, hunker down %d, calm them %d), %d skipped (option not available), weather set %d / already Sunny %d / primal %d, "
           "%d Pokemon burned, %d with nobody burned, %d damaged, %d calms with a fainted primary, %d leads already holding boosters, %d Volcarona, %d items and %d moves compared, %d Freeze/Burn cured on summon by Magma Armor/Water Veil/Water Bubble/Thermal Exchange, %d fields. TOTAL mismatches %d\n",
           cases, by_option[0], by_option[1], by_option[2], skipped, weather_set, weather_same, weather_primal, burned, burn_refused, damaged, fainted_primary, lead_stacked, enemy_checked, items_checked, moves_checked, cured_on_summon, fields_checked, mismatches);
    (void)ability_primary;
    return mismatches ? 1 : 0;
}
