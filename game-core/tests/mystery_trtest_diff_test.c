/* A Trainer's Test against upstream at 8555c08: replays the "-trainer" trace of tests/mystery-trtest.producer.test.ts (the real encounter: onInit in
 * executeWithSeedOffset(.., waveIndex), onOptionPhase in executeWithSeedOffset(.., getSeedOffset() * 500), initBattleWithEnemyConfig really run for the stat trainer's
 * fight, setEncounterRewards, the eggs and leaveEncounterWithoutBattle recorded) through game-core. The win is not played: it is what the won fight's phase does
 * (gc_trtest_victory, then the phases it queued).
 * Usage: mystery_trtest_diff_test <trace>; exit 0 only with 0 mismatches. */
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
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_trtest_diff_test <trace>\n", stderr); return 2; }
    char w[16];
    word(w, sizeof w);
    if (strcmp(w, "V")) return 2;
    integer();
    integer();
    int n_mons = 0;
    int cases = 0, mismatches = 0, by_option[2] = {0}, by_trainer[5] = {0}, enemy_checked = 0, items_checked = 0, moves_checked = 0, eggs_checked = 0, female = 0, texts_checked = 0, bosses = 0;
    for (;;) {
        word(w, sizeof w);
        if (!strcmp(w, "Z")) break;
        if (!strcmp(w, "P")) {
            n_mons = (int)integer();
            if (n_mons < 1 || n_mons > 6) { fputs("party size\n", stderr); return 2; }
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
        case_option = option; case_wave = wave;
        struct { int species, form, level, nature, ivs[6], hp, max, stats[6], n_moves, moves[4], luck, ability; } setup[6];
        tag("U");
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
        static item_row before_items[120], after_items[120], enemy_rows[120];
        tag("L");
        const int n_before = read_items(before_items);
        tag("A");
        const int meets[2] = {(int)integer(), (int)integer()};
        tag("R");
        const pr_rng before = rng_vals();
        tag("I");
        const pr_rng after_init = rng_vals();
        tag("W");
        const int init_msgs = (int)integer(), up_trainer_type = (int)integer();
        char up_key[16], egg_desc[96], up_name[32];
        word(up_key, sizeof up_key); word(egg_desc, sizeof egg_desc); word(up_name, sizeof up_name);
        tag("Y");
        const int n_configs = (int)integer(), level_mult = (int)integer(), cfg_trainer = (int)integer(), cfg_female = (int)integer(), cfg_double = (int)integer(), n_pokemon_configs = (int)integer();
        skip_line();
        tag("Q");
        const int pre = (int)integer();
        const pr_rng rng_pre = rng_vals();
        const pr_rng after_option = rng_vals();
        tag("T");
        const int n_rewards = (int)integer(), fill = (int)integer(), n_funcs = (int)integer();
        char funcs[4][48];
        for (int k = 0; k < n_funcs && k < 4; k++) word(funcs[k], sizeof funcs[k]);
        const int n_tiers = (int)integer();
        int tiers[4] = {0};
        for (int k = 0; k < n_tiers && k < 4; k++) tiers[k] = (int)integer();
        const int n_given = (int)integer();
        const double reroll = real();
        const int leave_calls = (int)integer(), leave_arg = (int)integer();
        tag("Z2");
        const int n_eggs = (int)integer();
        int egg_tier[4] = {0}, egg_source[4] = {0}, egg_pulled[4] = {0};
        for (int k = 0; k < n_eggs && k < 4; k++) {
            if (k) semicolon();
            egg_tier[k] = (int)integer(); egg_source[k] = (int)integer(); egg_pulled[k] = (int)integer();
            char d[96];
            word(d, sizeof d);
        }
        tag("O");
        const double money_after = (double)integer();
        char egg_type[32];
        word(egg_type, sizeof egg_type);
        tag("V");
        const int n_after = read_items(after_items);
        tag("H");
        const int n_queued = (int)integer();
        char queued[8][64] = {{0}};
        for (int k = 0; k < n_queued; k++) { char q[64]; word(q, sizeof q); if (k < 8) snprintf(queued[k], sizeof queued[k], "%s", q); }
        int enc_mode = 0, battle_double = 0, n_levels = 0, levels[6] = {0}, up_trainer = 0, up_variant = 0, up_template = 0, n_effects = 0, n_enemy_items = 0, n_x = 0;
        mon_row up[6];
        struct { int ball, tera, ability, luck, slot, fusion; char name[40]; } x3[6];
        if (option == 0) {
            tag("D");
            enc_mode = (int)integer(); battle_double = (int)integer(); n_levels = (int)integer();
            for (int k = 0; k < n_levels; k++) { const int l = (int)integer(); if (k < 6) levels[k] = l; }
            up_trainer = (int)integer(); up_variant = (int)integer(); up_template = (int)integer();
            skip_line();
            tag("F");
            n_effects = (int)integer();
            tag("J");
            n_enemy_items = (int)integer();
            for (int k = 0; k < n_enemy_items; k++) {
                enemy_rows[k].holder = (int)integer();
                word(enemy_rows[k].key, sizeof enemy_rows[k].key);
                enemy_rows[k].arg = (int)integer(); enemy_rows[k].stack = (int)integer();
            }
            tag("X");
            n_x = (int)integer();
            for (int k = 0; k < n_x && k < 6; k++) { if (k) semicolon(); read_mon(&up[k]); }
            tag("X3");
            integer();
            for (int k = 0; k < 6; k++) {
                if (k) semicolon();
                x3[k].ball = (int)integer(); x3[k].tera = (int)integer(); x3[k].ability = (int)integer(); x3[k].luck = (int)integer(); x3[k].slot = (int)integer(); x3[k].fusion = (int)integer();
                word(x3[k].name, sizeof x3[k].name);
            }
            tag("N");
            skip_line();
        }
        tag("E");
        (void)pre; (void)rng_pre; (void)up_name; (void)egg_pulled; (void)enc_mode; (void)battle_double; (void)n_x;

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
        run.mystery_type = GC_ME_A_TRAINERS_TEST;
        memset(&run.screen, 0, sizeof run.screen);
        run.screen.kind = GC_SCREEN_MYSTERY;
        run.screen.mystery_type = GC_ME_A_TRAINERS_TEST;
        { gc_event e; while (gc_run_next_event(&run, &e)) {} }
        gc_mystery_setup_screen(&run);
        CHECK(run.screen.n_options == 2, "%d options", run.screen.n_options);
        for (int o = 0; o < 2; o++) {
            CHECK(run.screen.option_ok[o] == (meets[o] != 0), "option %d enabled %d, upstream %d", o, run.screen.option_ok[o], meets[o]);
            EQ("option takes no pick", run.screen.option_pick[o], 0);
        }
        CHECK(same_rng(&run.run.rng, &after_init), "onInit moved the run RNG (c %.0f, upstream %.0f)", run.run.rng.c, after_init.c);
        EQ("onInit messages", init_msgs, 0);

        /* onInit's pick and the screen's dialogue tokens */
        const int pick = gc_trtest_pick(&run);
        EQ("the stat trainer (type)", 120 + pick, up_trainer_type);
        EQ("the stat trainer (config)", 120 + pick, cfg_trainer);
        CHECK(!strcmp(gc_trtest_key(pick), up_key), "stat trainer key %s, upstream %s", gc_trtest_key(pick), up_key);
        EQ("one enemy party config", n_configs, 1);
        EQ("level additive modifier", level_mult, 1);
        EQ("no Pokemon configs", n_pokemon_configs, 0);
        EQ("config female unset", cfg_female, -1);
        EQ("config double unset", cfg_double, 0);
        {
            const gc_screen *sc = &run.screen;
            char want[48];
            snprintf(want, sizeof want, "trainerNames:%s", up_key);
            int have = 0;
            for (int a = 0; a < sc->n_me_args; a++)
                if (sc->me_args[a].name && !strcmp(sc->me_args[a].name, "statTrainerName")) have = sc->me_args[a].s && !strcmp(sc->me_args[a].s, want);
            EQ("statTrainerName token", have, 1);
            texts_checked++;
        }
        by_trainer[pick]++;

        /* the option */
        bool ok = false;
        const char *w2 = "";
        gc_mystery_execute(&run, option, 0, 0, &w2, &ok);
        CHECK(ok, "option refused: %s", w2);
        CHECK(same_rng(&run.run.rng, &after_option), "the run RNG after the option differs (c %.0f, upstream %.0f)", run.run.rng.c, after_option.c);
        EQ("money unchanged by the option", (long long)run.money, (long long)money_after);
        EQ("rewards set once", n_rewards, 1);
        EQ("no reward options", n_given, 0);
        EQ("one egg", n_eggs, 1);
        EQ("the egg is from an event", egg_source[0], 4);
        check_items("after the option", after_items, n_after);
        items_checked += n_after;
        const gc_internal *in = gc_int(&run);
        gc_event ev;
        if (option == 0) {
            EQ("the shop fills the rest", fill, 1);
            EQ("one reward function", n_funcs, 1);
            CHECK(!strcmp(funcs[0], "RELIC_GOLD"), "upstream's reward function is %s", funcs[0]);
            EQ("two reward tiers", n_tiers, 2);
            EQ("both ROGUE", tiers[0] == MODIFIER_TIER_ROGUE && tiers[1] == MODIFIER_TIER_ROGUE, 1);
            EQ("no leave call", leave_calls, 0);
            EQ("the egg is Epic", egg_tier[0], 2);
            CHECK(!strcmp(egg_type, "an_Epic_Egg"), "egg text %s", egg_type);
            EQ("battle phase queued", !strcmp(queued[0], "+MysteryEncounterBattlePhase"), 1);
            CHECK(run.n_enemy == 6 && run.in_battle && run.me_battle, "no battle (%d enemies)", run.n_enemy);
            EQ("trainer battle", run.battle_type, BATTLE_TYPE_TRAINER);
            EQ("single battle", run.double_battle, 0);
            EQ("trainer type", run.trainer_type, up_trainer);
            EQ("trainer type (config)", run.trainer_type, cfg_trainer);
            EQ("trainer variant", run.trainer_variant == TRAINER_VARIANT_FEMALE, up_variant == 1);
            female += run.trainer_variant == TRAINER_VARIANT_FEMALE;
            EQ("party template", up_template, 0);
            EQ("enemy levels", n_levels, 6);
            EQ("enemy items", run.n_enemy_items, n_enemy_items);
            EQ("battle effects", in->n_me_start, n_effects);
            EQ("the fight is flagged", in->trtest.active, 1);
            EQ("no pending outro yet", in->me_post.pending, 1);
            for (int e = 0; e < 6 && e < run.n_enemy; e++) {
                const gc_mon *m = &run.enemy[e];
                enemy_checked++;
                char nm[48];
                EQ("species", m->b.species, up[e].species);
                EQ("form", m->b.form, up[e].form);
                EQ("level", m->b.level, up[e].level);
                EQ("level is the battle's", m->b.level, levels[e]);
                EQ("boss", m->b.is_boss, up[e].boss);
                EQ("segments", m->b.boss_segments, up[e].segments);
                bosses += up[e].boss;
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
                EQ("ability", pr_get_ability(&m->b, true), x3[e].ability);   /* the slot's own: Trace copies the player's Ball Fetch once the fight has started */
                EQ("luck", m->luck, x3[e].luck);
                EQ("trainer slot", m->trainer_slot, x3[e].slot);
                EQ("fusion", m->b.fusion_species, x3[e].fusion);
            }
            for (int k = 0; k < n_enemy_items; k++) {
                bool found = false;
                for (int j = 0; j < run.n_enemy_items; j++) {
                    const gc_item *it = &run.enemy_items[j];
                    found = found || (!strcmp(it->key, enemy_rows[k].key) && it->arg == enemy_rows[k].arg && it->stack == enemy_rows[k].stack && enemy_rows[k].holder >= 0 && enemy_rows[k].holder < 6 && it->pokemon_id == run.enemy[enemy_rows[k].holder].b.id);
                }
                CHECK(found, "enemy lacks item %s/%d x%d on %d", enemy_rows[k].key, enemy_rows[k].arg, enemy_rows[k].stack, enemy_rows[k].holder);
                items_checked++;
            }
            EQ("the player's party is whole", run.n_party, n_mons);

            /* the win: BattleEndPhase, TrainerVictoryPhase, the callback phase (the egg), the shop, the outro, the next wave */
            while (gc_run_next_event(&run, &ev)) {}
            gc_trtest_victory(&run);
            EQ("the fight is over", in->trtest.active, 0);
            int battle_end = 0, trainer_victory = 0, before_rewards = 0, shop = 0, post = 0, post_b = -1, post_c = -1, order_ok = 1, seq = 0;
            for (gc_rphase p; gc_rq_next(&gc_int(&run)->rq, &p);) {
                if (p.type == RP_BATTLE_END) { battle_end++; order_ok &= seq == 0; seq = 1; }
                if (p.type == RP_TRAINER_VICTORY) { trainer_victory++; order_ok &= seq == 1; seq = 2; }
                if (p.type == RP_ME_BEFORE_REWARDS) { before_rewards++; order_ok &= seq == 2; seq = 3; gc_phase_start(&run, &p); }
                if (p.type == RP_SELECT_MODIFIER) { shop += p.a == 2; order_ok &= seq == 3; seq = 4; }
                if (p.type == RP_ME_POST) { post++; post_b = p.b; post_c = p.c; order_ok &= seq == 4; seq = 5; gc_phase_start(&run, &p); }
            }
            EQ("the battle ends once", battle_end, 1);
            EQ("TrainerVictoryPhase runs", trainer_victory, 1);
            EQ("the eggs phase runs", before_rewards, 1);
            EQ("the item shop", shop, 1);
            EQ("one outro", post, 1);
            EQ("phases in handleMysteryEncounterVictory's order", order_ok, 1);
            EQ("outro names the stat trainer", post_b, pick);
            EQ("outro names the Epic Egg", post_c, 2);
            const pr_reward_custom *c = &gc_int(&run)->reward.me_custom;
            EQ("shop: fill the rest", c->fill_remaining, fill);
            EQ("shop: one function", c->n_funcs, n_funcs);
            EQ("shop: RELIC_GOLD", c->funcs[0], gc_reward_item_index(funcs[0]));
            EQ("shop: tiers", c->n_tiers, n_tiers);
            for (int k = 0; k < n_tiers && k < c->n_tiers; k++) EQ("shop: tier", c->tiers[k], tiers[k]);
            EQ("shop: no options", c->n_options, n_given);
        } else {
            EQ("the shop fills nothing", fill, 0);
            EQ("no reward functions", n_funcs, 0);
            EQ("no reward tiers", n_tiers, 0);
            EQ("reroll multiplier -1", (long long)reroll, -1);
            EQ("one leave call", leave_calls, 1);
            EQ("leave without a special result", leave_arg, -1);
            EQ("the egg is Rare", egg_tier[0], 1);
            CHECK(!strcmp(egg_type, "a_Rare_Egg"), "egg text %s", egg_type);
            EQ("PartyHealPhase(true) queued", !strcmp(queued[0], "PartyHealPhase_true"), 1);
            EQ("no battle", run.n_enemy, 0);
            int heal = 0, shop = 0, post = 0, post_b = -1, post_c = -1;
            for (gc_rphase p; gc_rq_next(&gc_int(&run)->rq, &p);) {
                if (p.type == RP_PARTY_HEAL) heal++;
                if (p.type == RP_SELECT_MODIFIER) { shop += p.a == 1; }
                if (p.type == RP_ME_POST) { post++; post_b = p.b; post_c = p.c; gc_phase_start(&run, &p); }
            }
            EQ("the party heals", heal, 1);
            EQ("the heal shop", shop, 1);
            EQ("one outro", post, 1);
            EQ("outro names the stat trainer", post_b, pick);
            EQ("outro names the Rare Egg", post_c, 1);
        }
        /* the eggs, the outro text */
        EQ("egg count", sys.n_eggs, n_eggs);
        if (sys.n_eggs == 1) {
            EQ("egg tier", sys.eggs[0].tier, egg_tier[0]);
            EQ("egg source", sys.eggs[0].source_type, egg_source[0]);
        }
        eggs_checked += 3;
        {
            int n_msg = 0, ok_args = 0;
            char want_name[48];
            snprintf(want_name, sizeof want_name, "trainerNames:%s", up_key);
            while (gc_run_next_event(&run, &ev)) {
                if (ev.kind != GC_EV_MESSAGE || !ev.msg.key || strcmp(ev.msg.key, "mysteryEncounters/aTrainersTest:outro")) continue;
                n_msg++;
                int name_ok = 0, egg_ok = 0;
                for (int a = 0; a < ev.msg.n_args; a++) {
                    if (!ev.msg.args[a].name || !ev.msg.args[a].s) continue;
                    if (!strcmp(ev.msg.args[a].name, "statTrainerName")) name_ok = !strcmp(ev.msg.args[a].s, want_name);
                    if (!strcmp(ev.msg.args[a].name, "eggType")) egg_ok = strstr(ev.msg.args[a].s, option == 0 ? "eggTypes.epic" : "eggTypes.rare") != NULL;
                }
                ok_args = name_ok && egg_ok;
            }
            EQ("one outro text", n_msg, 1);
            EQ("outro text names the trainer and the egg", ok_args, 1);
            texts_checked += 2;
        }
        if (bad) mismatches++;
        cases++; by_option[option]++;
    }
    printf("Mystery encounter A Trainer's Test: %d cases run (accept %d, refuse %d; Buck %d, Cheryl %d, Marley %d, Mira %d, Riley %d; female trainer %d), %d enemies compared (%d bosses), "
           "%d items and %d moves compared, %d egg checks, %d texts, %d fields. TOTAL mismatches %d\n",
           cases, by_option[0], by_option[1], by_trainer[0], by_trainer[1], by_trainer[2], by_trainer[3], by_trainer[4], female, enemy_checked, bosses, items_checked, moves_checked, eggs_checked, texts_checked, fields_checked, mismatches);
    return mismatches ? 1 : 0;
}
