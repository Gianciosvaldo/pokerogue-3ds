/* Berries Abound against upstream at 8555c08: replays the "-berries" trace of tests/mystery-berries.producer.test.ts (the real encounter: onInit in
 * executeWithSeedOffset(.., wave), then one option with onOptionPhase in (.., * 500) and initBattleWithEnemyConfig really run; the berries callback
 * by hand in the offset of wave * 1000) through game-core.
 *
 * Per case, compared with what upstream did: the starting stats; that onInit leaves the run's RNG alone; the boss onInit made (species, level, IVs,
 * nature, ability, id, HP, stats, moves), the number of berries, the Pokemon the race sends and the boss's Speed; for a fight (option 1, or the race
 * lost): the run RNG after the option, the five-berry shop, the boss that enters (new id, segments, items), its enrage effect, and the callback
 * (message, berries given, the run's RNG left alone); for the race won: the berries, the experience, the shop; for leaving: the heal shop.
 * Usage: mystery_berries_diff_test <trace>; exit 0 only with 0 mismatches. */
#include "../src/internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys, sys_before;
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
#define CHECK(cond, ...) do { if (!(cond)) { bad++; if (shown < (getenv("BRY_ALL") ? 100000 : 14)) { shown++; fprintf(stderr, "case %d (option %d wave %d): ", case_no, case_option, case_wave); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } } while (0)
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
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_berries_diff_test <trace>\n", stderr); return 2; }
    tag('V');
    integer();
    integer();
    int n_mons = 0;
    int cases = 0, mismatches = 0, by_option[3] = {0}, items_checked = 0, moves_checked = 0, wins = 0, losses = 0, enraged = 0, berries_given = 0, shop_checked = 0, full_skips = 0;
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
        static item_row before_items[120], boss_rows[120], after_items[120];
        const int n_before = read_items(before_items, 1);
        tag('R');
        const pr_rng before = rng_vals();
        tag('I');
        const pr_rng after_init = rng_vals();
        tag('W');
        boss_row init;
        memset(&init, 0, sizeof init);
        init.species = (int)integer(); init.form = (int)integer(); init.level = (int)integer(); integer();   /* isBoss */
        for (int k = 0; k < 6; k++) init.ivs[k] = (int)integer();
        init.nature = (int)integer(); init.ability = (int)integer(); init.passive = (int)integer(); init.shiny = (int)integer(); init.variant = (int)integer();
        init.gender = (int)integer(); init.friendship = (int)integer(); init.id = integer(); init.hp = (int)integer();
        for (int k = 0; k < 6; k++) init.stats[k] = (int)integer();
        init.n_moves = (int)integer();
        for (int k = 0; k < init.n_moves && k < 4; k++) init.moves[k] = (int)integer();
        tag('M');
        const int up_berries = (int)integer(), up_fastest = (int)integer(), up_speed = (int)integer(), up_name_token = (int)integer();
        pr_rng after_option = {0}, r0 = {0}, r1 = {0};
        int mode = 0, dbl = 0, n_levels = 0, levels[2], n_queued = 0;
        char queued[4][48], tags_text[96] = "";
        int n_rewards = 0, fill = 0, n_funcs = 0, has_cb = 0, leave_calls = 0, leave_heal = -1, n_opts = 0;
        pr_reward_option shop[8];
        int n_boss_items = 0, n_msgs = 0, msg_counts[2], n_items_after = 0;
        int exp_calls = 0, exp_who = -1, exp_amount = -1, exp_wave = -1, up_grab = -1;
        int fx_n = 0, fx_phases = 0;
        char fx_key[96] = "-", fx_text[160] = "-";
        boss_row up;
        memset(&up, 0, sizeof up);
        tag('Q'); after_option = rng_vals();
        char next_tag = '?';
        if (option != 2) {
            /* Q carries the battle fields only for a fight; a race won has the shop line right after the RNG */
            int c1 = fgetc(f);
            while (c1 == ' ' || c1 == '\n') c1 = fgetc(f);
            ungetc(c1, f);
            if (c1 != 'S') {
                mode = (int)integer(); dbl = (int)integer(); n_levels = (int)integer();
                for (int k = 0; k < n_levels && k < 2; k++) levels[k] = (int)integer();
                n_queued = (int)integer();
                for (int k = 0; k < n_queued && k < 4; k++) word(queued[k], sizeof queued[k]);
            }
        }
        (void)next_tag;
        tag('S'); n_rewards = (int)integer(); fill = (int)integer(); n_funcs = (int)integer(); has_cb = (int)integer(); leave_calls = (int)integer(); leave_heal = (int)integer();
        n_opts = (int)integer();
        for (int k = 0; k < n_opts && k < 8; k++) {
            char key[32];
            word(key, sizeof key);
            shop[k].type.item = (uint16_t)gc_reward_item_index(key);
            shop[k].type.arg = (int16_t)integer();
            shop[k].tier = (int8_t)integer();
            shop[k].upgrade_count = (int8_t)integer();
        }
        const bool battle = option == 0 || (option == 1 && !leave_calls);
        if (option == 2) {
            tag('E');
        } else {
            if (battle) {
                tag('X'); read_boss(&up, true);
                tag('T'); word(tags_text, sizeof tags_text);
                tag('J'); n_boss_items = read_items(boss_rows, 0);
                tag('F'); fx_n = (int)integer(); word(fx_key, sizeof fx_key); fx_phases = (int)integer(); word(fx_text, sizeof fx_text);
            } else {
                tag('K'); exp_calls = (int)integer(); exp_who = (int)integer(); exp_amount = (int)integer(); exp_wave = (int)integer(); up_grab = (int)integer();
            }
            tag('Y'); r0 = rng_vals(); r1 = rng_vals(); n_msgs = (int)integer();
            for (int k = 0; k < n_msgs && k < 2; k++) msg_counts[k] = (int)integer();
            tag('G'); n_items_after = read_items(after_items, 1);
            tag('E');
        }
        (void)fx_n; (void)mode; (void)levels; (void)n_queued; (void)queued; (void)n_funcs; (void)exp_wave;

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
            mon->b.n_moves = (uint8_t)setup[m].n_moves;
            memset(mon->b.moves, 0, sizeof mon->b.moves);
            memset(mon->b.pp_used, 0, sizeof mon->b.pp_used);
            for (int k = 0; k < setup[m].n_moves; k++) mon->b.moves[k] = (uint16_t)setup[m].moves[k];
            for (int k = 0; k < 6; k++) mon->b.ivs[k] = (uint8_t)setup[m].ivs[k];
            mon->base_nature = (uint8_t)setup[m].nature;
            mon->has_custom_nature = false;
            mon->b.nature = (uint8_t)setup[m].nature;
            mon->luck = (int8_t)setup[m].luck;
            mon->b.ability = mon->b.base_ability = (uint16_t)setup[m].ability;   /* the pool's weights read the party's abilities */
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
        run.mystery_type = GC_ME_BERRIES_ABOUND;
        memset(&run.screen, 0, sizeof run.screen);
        run.screen.kind = GC_SCREEN_MYSTERY;
        run.screen.mystery_type = GC_ME_BERRIES_ABOUND;
        gc_mystery_setup_screen(&run);
        CHECK(run.screen.n_options == 3, "%d options", run.screen.n_options);
        for (int o = 0; o < 3; o++) CHECK(run.screen.option_ok[o], "option %d is not enabled", o);
        CHECK(same_rng(&run.run.rng, &after_init) && same_rng(&before, &after_init), "onInit moved the run RNG (c %.0f, upstream %.0f)", run.run.rng.c, after_init.c);

        /* onInit: the boss, as the encounter keeps it, the berries, the fastest Pokemon and the boss's Speed */
        {
            gc_mon boss;
            int level = 0;
            pr_rng after;
            const bool made = gc_me_init_boss(&run, &boss, &level, &after);
            CHECK(made, "no boss for biome %d wave %d", biome, wave);
            if (made) {
                EQ("onInit species", boss.b.species, init.species);
                EQ("onInit form", boss.b.form, init.form);
                EQ("onInit level", level, init.level);
                for (int k = 0; k < 6; k++) { char n[32]; snprintf(n, sizeof n, "onInit iv %d", k); EQ(n, boss.b.ivs[k], init.ivs[k]); }
                EQ("onInit nature", boss.has_custom_nature ? boss.base_nature : boss.b.nature, init.nature);
                EQ("onInit ability index", boss.ability_index, init.ability);
                EQ("onInit passive flag", boss.b.passive_flag, init.passive);
                EQ("onInit shiny", boss.b.shiny, init.shiny);
                EQ("onInit variant", boss.variant, init.variant);
                EQ("onInit gender", boss.b.gender, init.gender);
                EQ("onInit friendship", boss.b.friendship, init.friendship);
                EQ("onInit id", boss.b.id, init.id);
                EQ("onInit hp", boss.b.hp, init.hp);
                for (int k = 0; k < 6; k++) { char n[32]; snprintf(n, sizeof n, "onInit stat %d", k); EQ(n, boss.b.stats[k], init.stats[k]); }
                EQ("onInit move count", boss.b.n_moves, init.n_moves);
                for (int k = 0; k < init.n_moves && k < 4; k++) { EQ("onInit move", boss.b.moves[k], init.moves[k]); moves_checked++; }
                EQ("boss speed", boss.b.stats[STAT_SPD], up_speed);
            }
            const int fastest = gc_berries_fastest(&run);
            EQ("fastest Pokemon", fastest >= 0 ? (int)run.party[fastest].b.id - 0x4000 : -1, up_fastest);
            (void)up_berries; (void)up_name_token;
        }

        memcpy(&sys_before, &sys, sizeof sys);
        { gc_event e; while (gc_run_next_event(&run, &e)) {} }
        bool ok = false;
        const char *w2 = "";
        gc_mystery_execute(&run, option, 0, 0, &w2, &ok);
        CHECK(ok, "option refused: %s", w2);
        CHECK(same_rng(&run.run.rng, &after_option), "the run RNG after the option differs (c %.0f, upstream %.0f)", run.run.rng.c, after_option.c);

        if (option == 2) {
            EQ("leave calls", leave_calls, 1);
            EQ("leaves with the heal shop", leave_heal, 1);
            EQ("rewards set", n_rewards, 0);
            CHECK(run.n_enemy == 0 && !run.me_battle, "a battle for the leave option");
            int heal = 0;
            for (gc_rphase p; gc_rq_next(&gc_int(&run)->rq, &p);) if (p.type == RP_SELECT_MODIFIER) heal += p.a == 1;
            EQ("the heal shop opens", heal, 1);
            if (bad) mismatches++;
            cases++; by_option[2]++;
            continue;
        }

        EQ("shop options", n_opts, 5);
        EQ("rewards set", n_rewards, 1);
        EQ("shop fill", fill, 0);
        EQ("a callback before the rewards", has_cb, 1);
        pr_reward_option mine[8];
        int n_mine = 0;
        if (battle) {
            CHECK(run.n_enemy == 1 && run.in_battle && run.me_battle, "no battle (%d enemies)", run.n_enemy);
            EQ("fight leaves nothing", leave_calls, 0);
            EQ("rewards set in C", gc_int(&run)->me_reward.active, 1);
            EQ("reward fill in C", gc_int(&run)->me_reward.fill, fill);
            EQ("callback in C", gc_int(&run)->me_reward.before, has_cb);
            EQ("shop size in C", gc_int(&run)->me_reward.n_more, n_opts);
            EQ("catch allowed", run.me_catch_allowed, 1);
            EQ("fleeing is off", gc_int(&run)->me_no_flee, 1);
            EQ("double", run.double_battle, dbl);
            for (int k = 0; k < gc_int(&run)->me_reward.n_more && k < 8; k++) mine[n_mine++] = gc_int(&run)->me_reward.more[k];
            const gc_mon *e = &run.enemy[0];
            EQ("boss species", e->b.species, up.species);
            /* Schooling (Wishiwashi) and Flower Gift (Cherrim) change form as the battle starts, which the oracle (no battle) does not play */
            const bool shifting = up.species == 746 || up.species == 421;
            if (!shifting) EQ("boss form", e->b.form, up.form);
            EQ("boss level", e->b.level, up.level);
            EQ("boss segments", e->b.boss_segments, up.segments);
            for (int k = 0; k < 6; k++) { char n[32]; snprintf(n, sizeof n, "boss iv %d", k); EQ(n, e->b.ivs[k], up.ivs[k]); }
            EQ("boss nature", e->has_custom_nature ? e->base_nature : e->b.nature, up.nature);
            EQ("boss ability index", e->ability_index, up.ability);
            EQ("boss passive flag", e->b.passive_flag, up.passive);
            EQ("boss shiny", e->b.shiny, up.shiny);
            EQ("boss variant", e->variant, up.variant);
            EQ("boss gender", e->b.gender, up.gender);
            EQ("boss friendship", e->b.friendship, up.friendship);
            EQ("boss id (new)", e->b.id, up.id);
            if (!shifting) { EQ("boss max hp", e->b.stats[0], up.stats[0]); EQ("boss hp", e->b.hp, up.hp); }
            if (!shifting) for (int k = 0; k < 6; k++) { char n[32]; snprintf(n, sizeof n, "boss stat %d", k); EQ(n, e->b.stats[k], up.stats[k]); }
            EQ("boss move count", e->b.n_moves, up.n_moves);
            for (int k = 0; k < up.n_moves && k < 4; k++) { char n[32]; snprintf(n, sizeof n, "boss move %d", k); EQ(n, e->b.moves[k], up.moves[k]); moves_checked++; }
            EQ("boss item count", run.n_enemy_items, n_boss_items);
            for (int k = 0; k < run.n_enemy_items && k < n_boss_items; k++) {
                const gc_item *it = &run.enemy_items[k];
                CHECK(!strcmp(it->key, boss_rows[k].key) && it->arg == boss_rows[k].arg && it->stack == boss_rows[k].stack && it->pokemon_id == e->b.id,
                      "boss item %d is %s/%d x%d, upstream %s/%d x%d", k, it->key, it->arg, it->stack, boss_rows[k].key, boss_rows[k].arg, boss_rows[k].stack);
                items_checked++;
            }
            /* the enrage effect: the battle has started (the boss's PostSummonPhase ran): the message, and one stage up for the stats of the set */
            const bool enrage = strstr(tags_text, "MYSTERY_ENCOUNTER_POST_SUMMON") != NULL;
            EQ("post-summon flag spent", e->b.me_post_summon, 0);
            EQ("enraged only when the race is lost", enrage, option == 1);
            if (enrage) {
                CHECK(!strcmp(fx_key, "mysteryEncounters/berriesAbound:option.2.bossEnraged"), "enrage message %s", fx_key);
                const char *want = wave < 50 ? "StatStageChangePhase:2x1,4x1,5x1" : "StatStageChangePhase:1x1,2x1,3x1,4x1,5x1";
                CHECK(!strcmp(fx_text, want), "enrage changes %s, upstream %s", want, fx_text);
                EQ("one enrage phase", fx_phases, 1);
                /* Defense, Special Defense and Speed went up by one (Simple and Contrary change that, Dauntless Shield and Intrepid Sword add to it) */
                const int ab = e->b.ability, pa = e->b.passive;
                const bool plain = ab != ABILITY_ID_SIMPLE && ab != ABILITY_ID_CONTRARY && pa != ABILITY_ID_SIMPLE && pa != ABILITY_ID_CONTRARY
                    && ab != ABILITY_ID_DAUNTLESS_SHIELD && pa != ABILITY_ID_DAUNTLESS_SHIELD && ab != ABILITY_ID_INTREPID_SWORD && pa != ABILITY_ID_INTREPID_SWORD;
                if (plain) {
                    EQ("Defense stage", e->b.stat_stages[STAT_DEF - 1], 1);
                    EQ("Special Defense stage", e->b.stat_stages[STAT_SPDEF - 1], 1);
                    EQ("Speed stage", e->b.stat_stages[STAT_SPD - 1], 1);
                    /* Attack and Special Attack: up from wave 50 on (an Intimidate in the player's party takes the Attack stage down again) */
                    bool intimidate = false;
                    for (int m = 0; m < run.n_party; m++) intimidate = intimidate || run.party[m].b.ability == ABILITY_ID_INTIMIDATE;
                    /* Download adds a stage of its own */
                    if (!intimidate && ab != ABILITY_ID_DOWNLOAD && pa != ABILITY_ID_DOWNLOAD) {
                        EQ("Attack stage", e->b.stat_stages[STAT_ATK - 1], wave >= 50);
                        EQ("Special Attack stage", e->b.stat_stages[STAT_SPATK - 1], wave >= 50);
                    }
                    enraged++;
                }
            } else EQ("no effects", fx_phases, 0);
            if (option == 1) losses++;
        } else {
            EQ("race leaves", leave_calls, 1);
            EQ("race leaves without the heal shop", leave_heal, 0);
            CHECK(run.n_enemy == 0 && !run.me_battle, "a battle after a won race");
            const pr_reward_custom *cs = &gc_int(&run)->reward.me_custom;
            EQ("shop options in C", cs->n_options, n_opts);
            EQ("shop functions in C", cs->n_funcs, 0);
            EQ("shop fill in C", cs->fill_remaining, 0);
            for (int k = 0; k < cs->n_options && k < 8; k++) mine[n_mine++] = cs->options[k];
            EQ("experience set", exp_calls, 1);
            EQ("experience goes to the fastest", exp_who, up_fastest);
            EQ("base experience of the boss", gc_form(init.species, init.form)->base_exp, exp_amount);
            EQ("experience scaled by the wave", exp_wave, 1);
            /* the Pokemon alone gets the experience (the others only through an Exp. Share) */
            bool others = false;
            double got = 0;
            const uint32_t id = (uint32_t)(0x4000 + exp_who);
            for (gc_rphase p; gc_rq_next(&gc_int(&run)->rq, &p);) {
                if (p.type != RP_EXP) continue;
                if (run.party[p.a].b.id != id) others = true; else got = p.d;
            }
            CHECK(!others || gc_item_stack(&run, "EXP_SHARE") > 0, "experience for a Pokemon that did not race, without an Exp. Share");
            { pr_exp_party pp; int idx[PR_MAX_EXP_PARTY]; double amt[PR_MAX_EXP_PARTY];
              double want = 0;
              gc_exp_party_build(&run, &pp, &id, 1);
              pp.trainer_battle = false; pp.me_exp_multiplier = 1.0;
              const int n = pr_run_apply_party_exp(&pp, exp_amount, true, run.wave, idx, amt);
              for (int k = 0; k < n; k++) if (run.party[run.party_order[idx[k]]].b.id == id) want = amt[k];
              EQ("experience the Pokemon that raced gets", (long long)got, (long long)want); }
            wins++;
        }
        EQ("shop built", n_mine, n_opts);
        for (int k = 0; k < n_mine && k < n_opts; k++) {
            EQ("shop item", mine[k].type.item, shop[k].type.item);
            EQ("shop berry", mine[k].type.arg, shop[k].type.arg);
            EQ("shop tier", mine[k].tier, shop[k].tier);
            EQ("shop upgrade", mine[k].upgrade_count, shop[k].upgrade_count);
            shop_checked++;
        }

        /* the callback: the rewards phase runs it in the offset of wave * 1000; a won race has run it already */
        if (battle) {
            { gc_event e; while (gc_run_next_event(&run, &e)) {} }
            /* the battle's start swaps a fainted lead for the first Pokemon that can fight; the oracle plays no battle, so the party goes back to its order */
            for (int m = 0; m < n_mons; m++) run.party_order[m] = (int8_t)st[m];
            gc_berries_reward(&run);
        }
        CHECK(same_rng(&run.run.rng, &after_option) && same_rng(&r0, &r1), "the callback moved the global RNG");
        {
            int msgs = 0, count = -1;
            gc_event e;
            while (gc_run_next_event(&run, &e)) {
                if (e.kind != GC_EV_MESSAGE || !e.msg.key || strcmp(e.msg.key, "battle:rewardGainCount")) continue;
                msgs++;
                for (int a = 0; a < e.msg.n_args; a++) if (e.msg.args[a].name && !strcmp(e.msg.args[a].name, "count")) count = (int)e.msg.args[a].v;
            }
            EQ("berry messages", msgs, n_msgs);
            if (n_msgs > 0 && msgs > 0) EQ("berries said", count, msg_counts[0]);
            if (!battle) EQ("berries grabbed", count, up_grab);
            berries_given += count > 0 ? count : 0;
        }
        EQ("item count after", run.n_items, n_items_after);
        for (int k = 0; k < run.n_items && k < n_items_after; k++) {
            const gc_item *it = &run.items[k];
            const int holder = it->pokemon_id ? (int)it->pokemon_id - 0x4000 : -1;
            CHECK(!strcmp(it->key, after_items[k].key) && it->arg == after_items[k].arg && holder == after_items[k].holder && it->stack == after_items[k].stack,
                  "item %d is %s/%d holder %d x%d, upstream %s/%d holder %d x%d", k, it->key, it->arg, holder, it->stack, after_items[k].key, after_items[k].arg, after_items[k].holder, after_items[k].stack);
            items_checked++;
        }
        if (bad) mismatches++;
        cases++; by_option[option]++;
    }
    printf("Mystery encounter Berries Abound: %d cases run (fight %d, race %d of which %d won and %d lost, leave %d), %d enraged bosses, %d berries given by the callback, %d shop berries, "
           "%d items and %d moves compared, %d fields. TOTAL mismatches %d\n",
           cases, by_option[0], by_option[1], wins, losses, by_option[2], enraged, berries_given, shop_checked, items_checked, moves_checked, fields_checked, mismatches);
    (void)full_skips;
    return mismatches ? 1 : 0;
}
