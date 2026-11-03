/* Teleporting Hijinks against upstream at 8555c08: replays the "-teleport" trace of tests/mystery-teleport.producer.test.ts (the real encounter:
 * onInit in executeWithSeedOffset(.., wave), onPreOptionPhase in (.., getSeedOffset()), onOptionPhase in (.., getSeedOffset() * 500), the awaits
 * taking the rest out of the offset, newArena and initBattleWithEnemyConfig really run, the boss's post-summon effect called by hand) through
 * game-core.
 *
 * Per case, compared with what upstream did: the starting stats; which options are offered (the money, the Steel or Electric Pokemon that is
 * not fainted); the price; the run RNG after the option; the money paid; the new biome and the arena it leaves (no weather, terrain, tags,
 * faints, Terastallizations used); the rewards set (fill, the two Steel and Electric type boosters of the machine); the experience of the
 * Pokemon that helped (which Pokemon, 100, with the wave multiplier); the texts; the boss that enters (id, form, level, IVs, nature, ability,
 * HP, stats, moves, bars, items), that fleeing is off and catching is allowed; and the stages the post-summon effect raises, with its message.
 * Usage: mystery_teleport_diff_test <trace>; exit 0 only with 0 mismatches. */
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



int main(int argc, char **argv)
{
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_teleport_diff_test <trace>\n", stderr); return 2; }
    tag('V');
    integer();
    integer();
    int n_mons = 0;
    int cases = 0, mismatches = 0, skipped = 0, by_option[3] = {0}, items_checked = 0, moves_checked = 0, fainted_primary = 0, same_biome = 0, high = 0, contrary = 0, enemy_msgs = 0;
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
        tag('B');
        int balls[5];
        for (int k = 0; k < 5; k++) balls[k] = (int)integer();
        tag('L');
        static item_row before_items[120], boss_rows[120], after_items[120];
        const int n_before = read_items(before_items, 1);
        tag('A');
        const int meets[3] = {(int)integer(), (int)integer(), (int)integer()};
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
        const double up_price = (double)integer();
        tag('Q');
        (void)rng_vals();
        const pr_rng after_option = rng_vals();
        tag('S');
        const int n_rewards = (int)integer(), fill = (int)integer(), n_funcs = (int)integer(), n_given = (int)integer(), n_tiers = (int)integer();
        const int leave_calls = (int)integer(), leave_heal = (int)integer();
        (void)leave_heal;
        tag('O');
        const int n_opts = (int)integer();
        struct { char key[48]; int arg, tier, upgrades; } opts[4];
        for (int k = 0; k < n_opts && k < 4; k++) {
            if (k) semicolon();
            word(opts[k].key, sizeof opts[k].key);
            opts[k].arg = (int)integer(); opts[k].tier = (int)integer(); opts[k].upgrades = (int)integer();
        }
        tag('X');
        { char t2 = '?'; if (fscanf(f, "%c", &t2) != 1 || t2 != '2') { fputs("X2 expected\n", stderr); return 2; } }
        const int n_exp = (int)integer();
        int exp_idx[2] = {-1, -1}, exp_base[2] = {0, 0}, exp_wave[2] = {0, 0};
        for (int k = 0; k < n_exp; k++) {
            if (k) semicolon();
            const int idx = (int)integer(), base = (int)integer(), w = (int)integer();
            if (k < 2) { exp_idx[k] = idx; exp_base[k] = base; exp_wave[k] = w; }
        }
        tag('M');
        const int n_said = (int)integer();
        char said[6][96];
        for (int k = 0; k < n_said && k < 6; k++) word(said[k], sizeof said[k]);
        tag('T');
        const int up_primary = (int)integer();
        const double money_after = (double)integer();
        tag('G');
        const int up_biome = (int)integer(), up_weather = (int)integer(), up_terrain = (int)integer(), up_tags = (int)integer(), up_faints = (int)integer(), up_teras = (int)integer();
        tag('Y');
        int up_hp[6], up_max[6], up_stats[6][6];
        for (int m = 0; m < n_mons; m++) {
            if (m) semicolon();
            up_hp[m] = (int)integer(); up_max[m] = (int)integer();
            for (int k = 0; k < 6; k++) up_stats[m][k] = (int)integer();
        }
        tag('V');
        const int n_after = read_items(after_items, 1);
        boss_row up;
        memset(&up, 0, sizeof up);
        int boss_index = 0, st_effect = 0, n_boss_items = 0, n_effects = 0, dbl = 0, n_post = 0, mult = 1;
        char post[4][96];
        int fx[4][5];
        tag('D');
        integer();
        dbl = (int)integer();
        const int n_levels = (int)integer();
        for (int k = 0; k < n_levels; k++) integer();
        const int n_queued = (int)integer();
        { char tmp[64]; for (int k = 0; k < n_queued; k++) word(tmp, sizeof tmp); }
        tag('X'); read_boss(&up, true);
        tag('N'); st_effect = (int)integer(); integer();
        tag('J'); n_boss_items = read_items(boss_rows, 0);
        tag('F'); n_effects = (int)integer();
        for (int k = 0; k < n_effects && k < 4; k++) {
            if (k) semicolon();
            for (int j = 0; j < 5; j++) fx[k][j] = (int)integer();
        }
        tag('K'); (void)rng_vals(); (void)rng_vals(); n_post = (int)integer();
        for (int k = 0; k < n_post && k < 4; k++) word(post[k], sizeof post[k]);
        tag('Z'); mult = (int)integer(); boss_index = (int)integer();
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
        gc_rq_reset(&gc_int(&run)->rq);
        run.mystery_type = GC_ME_TELEPORTING_HIJINKS;
        memset(&run.screen, 0, sizeof run.screen);
        run.screen.kind = GC_SCREEN_MYSTERY;
        run.screen.mystery_type = GC_ME_TELEPORTING_HIJINKS;
        gc_mystery_setup_screen(&run);
        CHECK(run.screen.n_options == 3, "%d options", run.screen.n_options);
        for (int o = 0; o < 3; o++) CHECK(run.screen.option_ok[o] == (meets[o] != 0), "option %d enabled %d, upstream %d", o, run.screen.option_ok[o], meets[o]);
        CHECK(same_rng(&run.run.rng, &after_init), "onInit moved the run RNG (c %.0f, upstream %.0f)", run.run.rng.c, after_init.c);
        { double price; int prim; gc_teleport_tokens(&run, &price, &prim); EQ("price", (long long)price, (long long)up_price);
          /* option2PrimaryName is the first Steel or Electric Pokemon in party order, fainted or not */
          if (option == 1) EQ("primary Pokemon", prim >= 0 ? (int)(run.party[prim].b.id - 0x4000) : -1, up_primary); }
        const double price_c = pr_run_get_wave_money_amount(run.wave, 1.75);
        /* the tooltip's {{price, money}} token: the same number */
        {
            const gc_screen *sc = &run.screen;
            double shown = -1;
            for (int a = 0; a < sc->n_me_args; a++) {
                if (!sc->me_args[a].name || strcmp(sc->me_args[a].name, "price") || sc->me_args[a].kind != PR_ARG_MONEY_EN_US) continue;
                const uint64_t bits = (uint64_t)sc->me_args[a].v | ((uint64_t)sc->me_args[a].fusion_v << 32);
                memcpy(&shown, &bits, sizeof shown);
            }
            EQ("price token", (long long)shown, (long long)up_price);
        }

        {
            gc_event e;
            while (gc_run_next_event(&run, &e)) {}
        }
        bool ok = false;
        const char *w2 = "";
        gc_mystery_execute(&run, option, 0, 0, &w2, &ok);
        CHECK(ok, "option refused: %s", w2);
        CHECK(same_rng(&run.run.rng, &after_option), "the run RNG after the option differs (c %.0f, upstream %.0f)", run.run.rng.c, after_option.c);
        EQ("rewards set", n_rewards, 1);
        EQ("rewards fill the rest", fill, 1);
        EQ("no reward functions", n_funcs, 0);
        EQ("no reward tiers", n_tiers, 0);
        EQ("leave calls", leave_calls, 0);
        EQ("money after", (long long)run.money, (long long)money_after);
        EQ("money paid", (long long)(money_before - run.money), option == 0 ? (long long)price_c : 0);
        EQ("no catching bar", run.me_catch_allowed, 1);

        /* the texts, in order, and the boss's enraged message that the battle's first phase shows */
        const int enraged_before = enemy_msgs;
        {
            char got[6][96];
            int n_got = 0;
            gc_event e;
            while (gc_run_next_event(&run, &e)) {
                if (e.kind == GC_EV_BATTLE) {
                    for (int j = e.a; j < e.a + e.b; j++) {
                        const pr_event *pe = &run.battle_events[j];
                        if (pe->type != PR_EV_TEXT || !pe->msg.key || strcmp(pe->msg.key, "mysteryEncounters/teleportingHijinks:bossEnraged")) continue;
                        enemy_msgs++;
                        int species = -1, kind = -1;
                        for (int a = 0; a < pe->msg.n_args; a++) if (pe->msg.args[a].name && !strcmp(pe->msg.args[a].name, "enemyPokemon")) { species = (int)(pe->msg.args[a].v >> 16); kind = pe->msg.args[a].kind; }
                        EQ("enraged message names the boss", species, up.species);
                        EQ("enraged message has the affix form", kind, PR_ARG_POKEMON);
                    }
                    continue;
                }
                if (e.kind != GC_EV_MESSAGE || !e.msg.key) continue;
                if (!strncmp(e.msg.key, "mysteryEncounters/teleportingHijinks:", 37) && n_got < 6) snprintf(got[n_got++], sizeof got[0], "%s", e.msg.key);
            }
            /* upstream's queueEncounterMessage of the enraged text is not in `said` (it comes from the by-hand effect) */
            int n_up = 0;
            for (int k = 0; k < n_said && k < 6; k++) if (!strncmp(said[k], "mysteryEncounters/teleportingHijinks:", 37)) n_up++;
            int n_up_texts = 0;
            for (int k = 0; k < n_said && k < 6; k++) if (!strncmp(said[k], "mysteryEncounters/teleportingHijinks:", 37) && strcmp(said[k], "mysteryEncounters/teleportingHijinks:bossEnraged")) n_up_texts++;
            int n_got_texts = 0;
            for (int k = 0; k < n_got; k++) if (strcmp(got[k], "mysteryEncounters/teleportingHijinks:bossEnraged")) n_got_texts++;
            EQ("text count", n_got_texts, n_up_texts);
            for (int k = 0, g = 0, u = 0; k < 6; k++) {
                while (u < n_said && (strncmp(said[u], "mysteryEncounters/teleportingHijinks:", 37) || !strcmp(said[u], "mysteryEncounters/teleportingHijinks:bossEnraged"))) u++;
                while (g < n_got && !strcmp(got[g], "mysteryEncounters/teleportingHijinks:bossEnraged")) g++;
                if (u >= n_said || g >= n_got) break;
                CHECK(!strcmp(said[u], got[g]), "text %d is %s, upstream %s", k, got[g], said[u]);
                fields_checked++;
                u++; g++;
            }
            (void)n_up;
        }

        EQ("enraged messages", enemy_msgs - enraged_before, option < 2);

        /* the new arena */
        {
            const pr_battle *bt = &run.bt;
            EQ("biome", run.biome, up_biome);
            if (up_weather >= 0) EQ("weather", bt->weather, up_weather);
            if (up_terrain >= 0) EQ("terrain", bt->terrain, up_terrain);
            /* a Neutralizing Gas boss sets its arena tag as it is summoned, which the oracle (no battle) does not play */
            if (run.enemy[0].b.ability != ABILITY_ID_NEUTRALIZING_GAS && run.enemy[0].b.passive != ABILITY_ID_NEUTRALIZING_GAS) EQ("arena tags", bt->n_arena_tags, up_tags);
            EQ("player faints", bt->player_faints, up_faints);
            EQ("Terastallizations used", (int)bt->player_teras_used, up_teras);
            if (option < 2) CHECK(biome != run.biome, "the machine did not change the biome (%d)", run.biome);
            else EQ("inspecting keeps the biome", run.biome, biome);
            same_biome += biome == run.biome;
        }

        /* the shop the win opens: option 3's two type boosters, then the ordinary rewards; the fighting options none of their own */
        {
            const gc_internal *in = gc_int(&run);
            EQ("reward options", in->me_reward.n_more, n_opts);
            EQ("guaranteed options set", n_given, n_opts);
            EQ("reward active", in->me_reward.active, 1);
            EQ("reward fill", in->me_reward.fill, 1);
            EQ("reward has no single item", in->me_reward.has_item, 0);
            EQ("reward function", in->me_reward.func, -1);
            for (int k = 0; k < n_opts && k < in->me_reward.n_more && k < 4; k++) {
                const pr_reward_option *o = &in->me_reward.more[k];
                CHECK(!strcmp(pr_item_keys[o->type.item], opts[k].key) && o->type.arg == opts[k].arg && o->tier == opts[k].tier && o->upgrade_count == opts[k].upgrades,
                      "reward option %d is %s/%d tier %d, upstream %s/%d tier %d", k, pr_item_keys[o->type.item], o->type.arg, o->tier, opts[k].key, opts[k].arg, opts[k].tier);
                fields_checked++;
            }
            /* setEncounterExp(primary.id, 100) */
            EQ("experience set", in->tele_exp.pending, n_exp);
            if (n_exp > 0) {
                EQ("experience Pokemon", (int)(run.party[in->tele_exp.primary].b.id - 0x4000), exp_idx[0]);
                EQ("experience base", (int)gc_int(&run)->tele_exp.base, exp_base[0]);
                EQ("experience wave multiplier", gc_int(&run)->tele_exp.wave, exp_wave[0]);
                fainted_primary += setup[exp_idx[0]].hp <= 0;
            }
        }

        EQ("party items unchanged", run.n_items, n_before);
        (void)n_after;
        for (int m = 0; m < n_mons; m++) {
            const gc_mon *mon = &run.party[st[m]];
            char nm[40];
            snprintf(nm, sizeof nm, "party %d max hp", m); EQ(nm, mon->b.stats[0], up_max[m]);
            for (int k = 0; k < 6; k++) { snprintf(nm, sizeof nm, "party %d stat %d", m, k); EQ(nm, mon->b.stats[k], up_stats[m][k]); }
        }

        CHECK(run.n_enemy == 1 && run.in_battle && run.me_battle, "no battle (%d enemies)", run.n_enemy);
        EQ("double", run.double_battle, dbl);
        EQ("fleeing is off", gc_int(&run)->me_no_flee, 1);
        const gc_mon *e = &run.enemy[0];
        EQ("boss species", e->b.species, up.species);
        /* Schooling (Wishiwashi) and Flower Gift (Cherrim) change form as the battle starts, which the oracle (no battle) does not play */
        const bool shifting = up.species == 746 || up.species == 421;
        if (!shifting) EQ("boss form", e->b.form, up.form);
        EQ("boss level", e->b.level, up.level);
        EQ("boss segments", e->b.boss_segments, up.segments);
        EQ("boss segment index", e->b.boss_segment_index, boss_index);
        for (int k = 0; k < 6; k++) { char n[32]; snprintf(n, sizeof n, "boss iv %d", k); EQ(n, e->b.ivs[k], up.ivs[k]); }
        EQ("boss nature", e->has_custom_nature ? e->base_nature : e->b.nature, up.nature);
        EQ("boss ability index", e->ability_index, up.ability);
        EQ("boss passive flag", e->b.passive_flag, up.passive);
        EQ("boss shiny", e->b.shiny, up.shiny);
        EQ("boss variant", e->variant, up.variant);
        EQ("boss gender", e->b.gender, up.gender);
        EQ("boss friendship", e->b.friendship, up.friendship);
        EQ("boss id", e->b.id, up.id);
        if (!shifting) { EQ("boss max hp", e->b.stats[0], up.stats[0]); EQ("boss hp", e->b.hp, up.hp); }
        if (!shifting) for (int k = 0; k < 6; k++) { char n[32]; snprintf(n, sizeof n, "boss stat %d", k); EQ(n, e->b.stats[k], up.stats[k]); }
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
        EQ("no start-of-battle effects", gc_int(&run)->n_me_start, n_effects);
        (void)fx;
        /* the post-summon effect: its message, then the stages (the holder's Simple doubles it, Contrary reverses it) */
        if (option < 2) {
            const int low = wave < 50;
            EQ("post-summon effects", n_post, 2);
            CHECK(n_post == 2 && !strcmp(post[0], "MSG_mysteryEncounters/teleportingHijinks:bossEnraged"), "upstream's post-summon message differs");
            EQ("post-summon flag spent", run.enemy[0].b.me_post_summon, 0);
            /* Dauntless Shield and Intrepid Sword add a stage of their own as the boss is summoned, Download another: the oracle does not summon it */
            const int ab = e->b.ability, pa = e->b.passive;
            const bool plain = ab != ABILITY_ID_DAUNTLESS_SHIELD && pa != ABILITY_ID_DAUNTLESS_SHIELD && ab != ABILITY_ID_INTREPID_SWORD && pa != ABILITY_ID_INTREPID_SWORD
                && ab != ABILITY_ID_DOWNLOAD && pa != ABILITY_ID_DOWNLOAD && ab != ABILITY_ID_OPPORTUNIST && pa != ABILITY_ID_OPPORTUNIST;
            if (plain) {
                bool intimidate = false;
                for (int m = 0; m < run.n_party; m++) intimidate = intimidate || run.party[m].b.ability == ABILITY_ID_INTIMIDATE;
                if (!intimidate) EQ("Atk stage", e->b.stat_stages[STAT_ATK - 1], low ? 0 : mult);
                EQ("Def stage", e->b.stat_stages[STAT_DEF - 1], mult);
                EQ("Sp. Atk stage", e->b.stat_stages[STAT_SPATK - 1], low ? 0 : mult);
                EQ("Sp. Def stage", e->b.stat_stages[STAT_SPDEF - 1], mult);
                EQ("Spd stage", e->b.stat_stages[STAT_SPD - 1], mult);
            }
            high += !low; contrary += mult == -1;
        } else {
            EQ("no post-summon effects", n_post, 0);
            if (e->b.ability != ABILITY_ID_DAUNTLESS_SHIELD && e->b.passive != ABILITY_ID_DAUNTLESS_SHIELD && e->b.ability != ABILITY_ID_INTREPID_SWORD && e->b.passive != ABILITY_ID_INTREPID_SWORD
                && e->b.ability != ABILITY_ID_DOWNLOAD && e->b.passive != ABILITY_ID_DOWNLOAD)
                for (int s2 = 1; s2 < 5; s2++) EQ("no stage change", e->b.stat_stages[s2], 0);
        }
        (void)up_hp;
        if (bad) mismatches++;
        cases++; by_option[option]++;
    }
    printf("Mystery encounter Teleporting Hijinks: %d cases run (money %d, a Pokemon helps %d, inspect %d), %d skipped (option not available), %d fights from wave 50 on, "
           "%d with a fainted Pokemon as the helper, %d with a Contrary boss, %d enraged texts, %d inspects in the same biome, %d items and %d moves compared, %d fields. TOTAL mismatches %d\n",
           cases, by_option[0], by_option[1], by_option[2], skipped, high, fainted_primary, contrary, enemy_msgs, same_biome, items_checked, moves_checked, fields_checked, mismatches);
    return mismatches ? 1 : 0;
}
