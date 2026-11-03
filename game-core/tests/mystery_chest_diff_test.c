/* The Mysterious Chest against upstream at 8555c08: replays the "-chest" trace of tests/mystery-chest.producer.test.ts (the real
 * encounter: onInit in executeWithSeedOffset(.., wave), onPreOptionPhase in (.., getSeedOffset()) and onOptionPhase in (.., * 500), the
 * trap's await taking the rest out of the offset, initBattleWithEnemyConfig really run) through game-core.
 *
 * Per case, compared with what upstream did: the starting stats; which options are offered; that onInit leaves the run's RNG alone and its
 * config (Gimmighoul, form 0, boss, the four moves, the half level boost, no switching); the chest's roll; the run RNG after the option; the
 * rewards set (the tiers of each band and the message, or the trap's fill-the-rest shop), the messages said, the HP and status of every
 * party member (who faints on a tie), the game over, and for the trap fight the boss that enters (id, form, level, IVs, nature, ability, HP,
 * stats, moves, segments, items), that fleeing stays on, catching is allowed and no start effect is queued; for leaving: the heal shop.
 * Usage: mystery_chest_diff_test <trace>; exit 0 only with 0 mismatches. */
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


int main(int argc, char **argv)
{
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_chest_diff_test <trace>\n", stderr); return 2; }
    tag('V');
    integer();
    integer();
    int n_mons = 0;
    int cases = 0, mismatches = 0, skipped = 0, by_band[5] = {0}, leaves = 0, traps = 0, game_overs = 0, items_checked = 0, moves_checked = 0, edges = 0, ties = 0;
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
        case_no = (int)integer();
        word(run_seed, sizeof run_seed); word(wave_seed, sizeof wave_seed);
        const int wave = (int)integer(), option = (int)integer(), biome = (int)integer();
        const int trainer_id = (int)integer(), secret_id = (int)integer();
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
        tag('B');
        int balls[5];
        for (int k = 0; k < 5; k++) balls[k] = (int)integer();
        tag('L');
        static item_row before_items[120], boss_rows[120];
        const int n_before = read_items(before_items, 1);
        tag('A');
        const int meets[2] = {(int)integer(), (int)integer()};
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
        struct { int species, form; double level_add; int boss, disable_switch, n_moves, moves[4], n_mods; } init;
        memset(&init, 0, sizeof init);
        init.species = (int)integer(); init.form = (int)integer(); init.level_add = real(); init.boss = (int)integer(); init.disable_switch = (int)integer();
        init.n_moves = (int)integer();
        for (int k = 0; k < init.n_moves && k < 4; k++) init.moves[k] = (int)integer();
        init.n_mods = (int)integer();
        tag('O');
        const int roll = (int)integer();
        tag('Q');
        const pr_rng after_option = rng_vals();
        tag('S');
        const int n_rewards = (int)integer(), fill = (int)integer(), n_tiers = (int)integer();
        int tiers[8];
        for (int k = 0; k < n_tiers && k < 8; k++) tiers[k] = (int)integer();
        const int n_funcs = (int)integer(), n_opts = (int)integer(), leave_calls = (int)integer(), leave_heal = (int)integer();
        tag('M');
        const int n_said = (int)integer();
        char said[4][96];
        for (int k = 0; k < n_said && k < 4; k++) word(said[k], sizeof said[k]);
        tag('Y');
        int up_hp[6], up_status[6];
        for (int m = 0; m < n_mons; m++) { up_hp[m] = (int)integer(); up_status[m] = (int)integer(); }
        tag('G');
        const int game_over = (int)integer();
        const int cleared = (int)integer();
        boss_row up;
        memset(&up, 0, sizeof up);
        int st_effect = 0, n_boss_items = 0, n_effects = 0, tag_ok = 1, dbl = 0;
        const bool battling = option == 0 && roll < 30 && !game_over;
        if (battling) {
            tag('D');
            integer();   /* encounterMode */
            dbl = (int)integer();
            const int n_levels = (int)integer();
            for (int k = 0; k < n_levels; k++) integer();
            const int n_queued = (int)integer();
            char tmp[64];
            for (int k = 0; k < n_queued; k++) word(tmp, sizeof tmp);
            tag('X'); read_boss(&up, true);
            tag('N'); st_effect = (int)integer(); integer(); integer();
            tag('T'); { char tg[96]; word(tg, sizeof tg); tag_ok = !strcmp(tg, "-"); }
            tag('J'); n_boss_items = read_items(boss_rows, 0);
            tag('F'); n_effects = (int)integer();
        }
        tag('E');
        (void)cleared;

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
        run.mystery_type = GC_ME_MYSTERIOUS_CHEST;
        memset(&run.screen, 0, sizeof run.screen);
        run.screen.kind = GC_SCREEN_MYSTERY;
        run.screen.mystery_type = GC_ME_MYSTERIOUS_CHEST;
        gc_mystery_setup_screen(&run);
        CHECK(run.screen.n_options == 2, "%d options", run.screen.n_options);
        for (int o = 0; o < 2; o++) CHECK(run.screen.option_ok[o] == (meets[o] != 0), "option %d enabled %d, upstream %d", o, run.screen.option_ok[o], meets[o]);
        CHECK(same_rng(&run.run.rng, &after_init), "onInit moved the run RNG (c %.0f, upstream %.0f)", run.run.rng.c, after_init.c);
        /* onInit's config */
        EQ("onInit species", init.species, SPECIES_ID_GIMMIGHOUL);
        EQ("onInit form", init.form, 0);
        EQ("onInit is a boss", init.boss, 1);
        EQ("onInit disables switching", init.disable_switch, 1);
        EQ("onInit level modifier x2", (int)(init.level_add * 2), 1);
        EQ("onInit move count", init.n_moves, 4);
        { static const int want[4] = {MOVE_ID_NASTY_PLOT, MOVE_ID_SHADOW_BALL, MOVE_ID_POWER_GEM, MOVE_ID_THIEF};
          for (int k = 0; k < 4; k++) EQ("onInit move", want[k], init.moves[k]); }
        EQ("onInit item configs", init.n_mods, -1);
        if (option == 0) EQ("the chest's roll", gc_chest_roll(&run), roll);

        {
            gc_event e;
            while (gc_run_next_event(&run, &e)) {}
        }
        bool ok = false;
        const char *w2 = "";
        gc_mystery_execute(&run, option, 0, 0, &w2, &ok);
        CHECK(ok, "option refused: %s", w2);
        CHECK(same_rng(&run.run.rng, &after_option), "the run RNG after the option differs (c %.0f, upstream %.0f)", run.run.rng.c, after_option.c);

        /* what was said */
        static const char *const keys[6] = {
            "mysteryEncounters/mysteriousChest:option.1.normal", "mysteryEncounters/mysteriousChest:option.1.good",
            "mysteryEncounters/mysteriousChest:option.1.great", "mysteryEncounters/mysteriousChest:option.1.amazing",
            "mysteryEncounters/mysteriousChest:option.1.bad", "battle:fainted"};
        int c_said[6] = {0}, up_said[6] = {0};
        { gc_event e;
          while (gc_run_next_event(&run, &e))
              if (e.kind == GC_EV_MESSAGE && e.msg.key) for (int k = 0; k < 6; k++) c_said[k] += !strcmp(e.msg.key, keys[k]); }
        for (int k = 0; k < n_said && k < 4; k++) for (int j = 0; j < 6; j++) up_said[j] += !strcmp(said[k], keys[j]);
        for (int j = 0; j < 6; j++) EQ(keys[j], c_said[j], up_said[j]);

        /* the party afterwards: who fainted */
        for (int m = 0; m < n_mons; m++) {
            const gc_mon *mon = &run.party[st[m]];
            char nm[32];
            snprintf(nm, sizeof nm, "party %d hp", m); EQ(nm, mon->b.hp, up_hp[m]);
            snprintf(nm, sizeof nm, "party %d status", m); EQ(nm, mon->b.status, up_status[m]);
        }
        EQ("fleeing is not off", gc_int(&run)->me_no_flee, 0);
        EQ("no switch flag left behind", gc_int(&run)->me_no_switch, 0);

        const int band = roll >= 75 ? 0 : roll >= 45 ? 1 : roll >= 35 ? 2 : roll >= 30 ? 3 : 4;
        if (option == 1) {
            CHECK(run.n_enemy == 0 && !run.me_battle, "leaving started a battle");
            EQ("rewards set", n_rewards, 0);
            EQ("leave calls", leave_calls, 1);
            EQ("leave has the heal shop", leave_heal, 1);
            int healshop = 0, shop = 0;
            for (gc_rphase p; gc_rq_next(&gc_int(&run)->rq, &p);) if (p.type == RP_SELECT_MODIFIER) { healshop += p.a == 1; shop += p.a == 2; }
            EQ("heal shop", healshop, 1);
            EQ("no item shop", shop, 0);
            leaves++;
        } else if (band < 4) {
            CHECK(run.n_enemy == 0 && !run.me_battle, "a reward band started a battle");
            static const int want_n[4] = {4, 3, 2, 1};
            static const int want_tier[4][4] = {{0, 0, 1, 1}, {2, 2, 2, 2}, {3, 3, 3, 3}, {4, 4, 4, 4}};
            EQ("rewards set", n_rewards, 1);
            EQ("rewards fill the rest", fill, 0);
            EQ("tiers set", n_tiers, want_n[band]);
            EQ("no reward functions", n_funcs, 0);
            EQ("no reward options", n_opts, 0);
            EQ("leave calls", leave_calls, 1);
            EQ("leave has no heal shop", leave_heal, 0);
            const pr_reward_custom *cs = &gc_int(&run)->reward.me_custom;
            EQ("shop tiers", cs->n_tiers, n_tiers);
            for (int k = 0; k < n_tiers && k < cs->n_tiers; k++) { char nm[32]; snprintf(nm, sizeof nm, "shop tier %d", k); EQ(nm, cs->tiers[k], tiers[k]); EQ("tier is the band's", tiers[k], want_tier[band][k]); }
            EQ("shop functions", cs->n_funcs, 0);
            EQ("shop options", cs->n_options, 0);
            EQ("shop fill", cs->fill_remaining, 0);
            int healshop = 0, shop = 0;
            for (gc_rphase p; gc_rq_next(&gc_int(&run)->rq, &p);) if (p.type == RP_SELECT_MODIFIER) { healshop += p.a == 1; shop += p.a == 2; }
            EQ("item shop", shop, 1);
            EQ("no heal shop", healshop, 0);
            by_band[band]++;
            edges += (roll == 29 || roll == 30 || roll == 34 || roll == 35 || roll == 44 || roll == 45 || roll == 74 || roll == 75);
        } else if (game_over) {
            CHECK(run.n_enemy == 0 && !run.me_battle, "a game over started a battle");
            EQ("rewards set", n_rewards, 0);
            int total = 0, over = 0;
            for (gc_rphase p; gc_rq_next(&gc_int(&run)->rq, &p);) { total++; over += p.type == RP_GAME_OVER; }
            EQ("game over queued", over, 1);
            EQ("the queue was cleared first", total, 1);
            by_band[4]++; traps++; game_overs++;
            edges += (roll == 29);
        } else {
            CHECK(run.n_enemy == 1 && run.in_battle && run.me_battle, "no battle (%d enemies)", run.n_enemy);
            EQ("double", run.double_battle, dbl);
            EQ("catch allowed", run.me_catch_allowed, 1);
            EQ("rewards set", gc_int(&run)->me_reward.active, n_rewards == 1);
            EQ("rewards fill the rest", gc_int(&run)->me_reward.fill, fill);
            EQ("reward is no item", gc_int(&run)->me_reward.has_item, 0);
            EQ("no reward function", gc_int(&run)->me_reward.func, -1);
            EQ("no reward tiers", gc_int(&run)->me_reward.n_tiers, n_tiers);
            EQ("upstream's reward functions", n_funcs, 0);
            EQ("upstream's reward options", n_opts, 0);
            EQ("leave calls", leave_calls, 0);
            CHECK(tag_ok, "the boss carries tags upstream");
            const gc_mon *e = &run.enemy[0];
            EQ("boss species", e->b.species, up.species);
            EQ("boss form", e->b.form, up.form);
            EQ("boss level", e->b.level, up.level);
            EQ("boss segments", e->b.boss_segments, up.segments);
            for (int k = 0; k < 6; k++) { char n[32]; snprintf(n, sizeof n, "boss iv %d", k); EQ(n, e->b.ivs[k], up.ivs[k]); }
            EQ("boss nature", e->b.nature, up.nature);
            EQ("boss ability index", e->ability_index, up.ability);
            EQ("boss passive flag", e->b.passive_flag, up.passive);
            EQ("boss shiny", e->b.shiny, up.shiny);
            EQ("boss variant", e->variant, up.variant);
            EQ("boss gender", e->b.gender, up.gender);
            EQ("boss friendship", e->b.friendship, up.friendship);
            EQ("boss id", e->b.id, up.id);
            EQ("boss max hp", e->b.stats[0], up.stats[0]);
            EQ("boss hp", e->b.hp, up.hp);
            for (int k = 0; k < 6; k++) { char n[32]; snprintf(n, sizeof n, "boss stat %d", k); EQ(n, e->b.stats[k], up.stats[k]); }
            EQ("boss move count", e->b.n_moves, up.n_moves);
            for (int k = 0; k < up.n_moves && k < 4; k++) { char n[32]; snprintf(n, sizeof n, "boss move %d", k); EQ(n, e->b.moves[k], up.moves[k]); moves_checked++; }
            EQ("boss status", e->b.status, st_effect);
            EQ("boss item count", run.n_enemy_items, n_boss_items);
            for (int k = 0; k < run.n_enemy_items && k < n_boss_items; k++) {
                const gc_item *it = &run.enemy_items[k];
                CHECK(!strcmp(it->key, boss_rows[k].key) && it->arg == boss_rows[k].arg && it->stack == boss_rows[k].stack && it->pokemon_id == e->b.id,
                      "boss item %d is %s/%d x%d, upstream %s/%d x%d", k, it->key, it->arg, it->stack, boss_rows[k].key, boss_rows[k].arg, boss_rows[k].stack);
                items_checked++;
            }
            EQ("party item count", run.n_items, n_before);
            EQ("start-of-battle effects", gc_int(&run)->n_me_start, n_effects);
            by_band[4]++; traps++;
            edges += (roll == 29);
            {   /* a tie for the highest level among the Pokemon that were up */
                int top = -1, n_top = 0;
                for (int m = 0; m < n_mons; m++) if (hps[m] > 0 && setup[m].level > top) top = setup[m].level;
                for (int m = 0; m < n_mons; m++) if (hps[m] > 0 && setup[m].level == top) n_top++;
                ties += n_top > 1;
            }
        }
        if (bad) mismatches++;
        cases++;
    }
    printf("Mystery encounter Mysterious Chest: %d cases run (reward bands %d/%d/%d/%d, trap %d of which %d game over, leave %d), %d skipped (option not available), "
           "%d on a band edge, %d trap fights with a tie for the highest level, %d items and %d moves compared, %d fields. TOTAL mismatches %d\n",
           cases, by_band[0], by_band[1], by_band[2], by_band[3], traps, game_overs, leaves, skipped, edges, ties, items_checked, moves_checked, fields_checked, mismatches);
    return mismatches ? 1 : 0;
}
