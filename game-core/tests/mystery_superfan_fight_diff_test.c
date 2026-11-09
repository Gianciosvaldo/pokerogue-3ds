/* The Bug-Type Superfan's fight against upstream at 8555c08: replays the "-superfanfight" trace of tests/mystery-superfan-fight.producer.test.ts (the real encounter:
 * onInit in executeWithSeedOffset(.., waveIndex), option 1's onOptionPhase in executeWithSeedOffset(.., getSeedOffset() * 500), initBattleWithEnemyConfig really run:
 * the trainer, its team by wave band, the levels and the held items) through game-core.
 * Usage: mystery_superfan_fight_diff_test <trace>; exit 0 only with 0 mismatches. */
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

int main(int argc, char **argv)
{
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_superfan_fight_diff_test <trace>\n", stderr); return 2; }
    char w[16];
    word(w, sizeof w);
    if (strcmp(w, "V")) return 2;
    integer();
    integer();
    int n_mons = 0;
    int cases = 0, mismatches = 0, bands[8] = {0}, enemy_checked = 0, items_checked = 0, moves_checked = 0, forms = 0, bosses = 0, tutor_checked = 0;
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
        const int wave = (int)integer(), biome = (int)integer();
        const int trainer_id = (int)integer(), secret_id = (int)integer();
        const double money_before = (double)integer();
        case_option = 0; case_wave = wave;
        struct { int species, form, level, luck; } setup[6];
        tag("U");
        for (int m = 0; m < n_mons; m++) {
            if (m) semicolon();
            setup[m].species = (int)integer(); setup[m].form = (int)integer(); setup[m].level = (int)integer(); setup[m].luck = (int)integer();
        }
        tag("R");
        const pr_rng before = rng_vals();
        tag("I");
        const pr_rng after_init = rng_vals();
        tag("Y");
        const int trainer_type = (int)integer(), up_female = (int)integer(), up_double = (int)integer();
        const int n_templates = (int)integer(), tpl_size = (int)integer(), tpl_last_strength = (int)integer();
        char tname[96];
        word(tname, sizeof tname);
        tag("Q");
        (void)rng_vals();
        const pr_rng after_option = rng_vals();
        tag("T");
        const int n_rewards = (int)integer(), fill = (int)integer(), n_funcs = (int)integer();
        for (int k = 0; k < n_funcs; k++) { char fn[48]; word(fn, sizeof fn); }
        const int n_tiers = (int)integer(), n_given = (int)integer();
        (void)real();
        const int has_on_rewards = (int)integer(), has_do_rewards = (int)integer();
        tag("M");
        const int n_tutor = (int)integer();
        int tutor[4] = {0};
        for (int k = 0; k < n_tutor; k++) { const int mv = (int)integer(); if (k < 4) tutor[k] = mv; }
        tag("D");
        const int enc_mode = (int)integer(), battle_double = (int)integer(), n_levels = (int)integer();
        int levels[6] = {0};
        for (int k = 0; k < n_levels; k++) { const int l = (int)integer(); if (k < 6) levels[k] = l; }
        const int up_trainer = (int)integer(), up_variant = (int)integer(), up_template_index = (int)integer();
        char name_key[48], name[96], partner_key[48];
        word(name_key, sizeof name_key); word(name, sizeof name); word(partner_key, sizeof partner_key);
        tag("F");
        const int n_effects = (int)integer();
        static item_row enemy_rows[120];
        tag("J");
        const int n_enemy_items = (int)integer();
        for (int k = 0; k < n_enemy_items; k++) {
            enemy_rows[k].holder = (int)integer();
            word(enemy_rows[k].key, sizeof enemy_rows[k].key);
            enemy_rows[k].arg = (int)integer(); enemy_rows[k].stack = (int)integer();
        }
        mon_row up[6];
        tag("X");
        const int n_x = (int)integer();
        for (int k = 0; k < n_x && k < 6; k++) { if (k) semicolon(); read_mon(&up[k]); }
        tag("X3");
        integer();
        struct { int ball, tera, ability, luck, slot, fusion; char name[40]; } x3[6];
        for (int k = 0; k < n_x && k < 6; k++) {
            if (k) semicolon();
            x3[k].ball = (int)integer(); x3[k].tera = (int)integer(); x3[k].ability = (int)integer(); x3[k].luck = (int)integer(); x3[k].slot = (int)integer(); x3[k].fusion = (int)integer();
            word(x3[k].name, sizeof x3[k].name);
        }
        tag("N");
        for (int k = 0; k < n_x; k++) { if (k) semicolon(); integer(); integer(); }
        char tera_slots[24], tera_mode[8];
        word(tera_slots, sizeof tera_slots);
        word(tera_mode, sizeof tera_mode);
        tag("E");
        (void)enc_mode; (void)battle_double; (void)up_template_index; (void)name_key; (void)partner_key; (void)tera_mode;

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
            mon->luck = (int8_t)setup[m].luck;
            mon->b.shiny = setup[m].luck > 0;
            mon->b.on_field = false;
            run.party_order[m] = (int8_t)st[m];
        }
        run.n_party = n_mons;
        gc_items_to_battle(&run);
        bad = 0;
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
        run.mystery_type = GC_ME_BUG_TYPE_SUPERFAN;
        memset(&run.screen, 0, sizeof run.screen);
        run.screen.kind = GC_SCREEN_MYSTERY;
        run.screen.mystery_type = GC_ME_BUG_TYPE_SUPERFAN;
        { gc_event e; while (gc_run_next_event(&run, &e)) {} }
        gc_mystery_setup_screen(&run);
        CHECK(run.screen.n_options == 3 && run.screen.option_ok[0], "%d options, the fight %d", run.screen.n_options, run.screen.option_ok[0]);
        CHECK(same_rng(&run.run.rng, &after_init), "onInit moved the run RNG (c %.0f, upstream %.0f)", run.run.rng.c, after_init.c);

        /* onInit's team: the config for the wave */
        const int band = gc_superfan_band(wave);
        EQ("the party template count", n_templates, 1);
        EQ("the team size", tpl_size, band == 0 ? 2 : band + 2 > 5 ? 5 : band + 2);
        EQ("the last member's strength", tpl_last_strength, band >= 6 ? 4 : 3);
        EQ("config female", up_female, 1);
        EQ("config double", up_double, 0);
        CHECK(strstr(tname, "Superfan") != NULL, "upstream's trainer name is %s", tname);

        /* option 1 */
        bool ok = false;
        const char *w2 = "";
        gc_mystery_execute(&run, 0, 0, 0, &w2, &ok);
        CHECK(ok, "option refused: %s", w2);
        CHECK(same_rng(&run.run.rng, &after_option), "the run RNG after the option differs (c %.0f, upstream %.0f)", run.run.rng.c, after_option.c);
        for (int k = 0; k < 4; k++) { char nm[48]; snprintf(nm, sizeof nm, "tutor move %d", k); EQ(nm, gc_int(&run)->superfan.tutor[k], tutor[k]); tutor_checked++; }
        EQ("four tutor moves", n_tutor, 4);
        EQ("money unchanged", (long long)run.money, (long long)money_before);
        EQ("rewards set", n_rewards, 1);
        EQ("rewards fill the rest", fill, 1);
        EQ("no reward functions", n_funcs, 0);
        EQ("no reward options", n_given, 0);
        EQ("no reward tiers", n_tiers, 0);
        EQ("has the tutor callback", has_on_rewards, 1);
        EQ("no doEncounterRewards", has_do_rewards, 0);
        {
            const gc_internal *in = gc_int(&run);
            EQ("reward active", in->me_reward.active, 1);
            EQ("reward fill", in->me_reward.fill, 1);
            EQ("reward has no single item", in->me_reward.has_item, 0);
            EQ("reward function", in->me_reward.func, -1);
            EQ("battle effects", in->n_me_start, n_effects);
        }
        CHECK(run.n_enemy == n_x && run.in_battle && run.me_battle, "no battle (%d enemies, upstream %d)", run.n_enemy, n_x);
        EQ("trainer battle", run.battle_type, BATTLE_TYPE_TRAINER);
        EQ("single battle", run.double_battle, 0);
        EQ("trainer type", run.trainer_type, up_trainer);
        EQ("trainer type (config)", run.trainer_type, trainer_type);
        EQ("trainer variant", run.trainer_variant == TRAINER_VARIANT_FEMALE, up_variant == 1);
        EQ("enemy levels", n_levels, n_x);
        EQ("enemy items", run.n_enemy_items, n_enemy_items);
        for (int e = 0; e < n_x && e < 6 && e < run.n_enemy; e++) {
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
            forms += up[e].form > 0;
            bosses += up[e].segments > 0;
        }
        for (int k = 0; k < n_enemy_items; k++) {
            bool found = false;
            for (int j = 0; j < run.n_enemy_items; j++) {
                const gc_item *it = &run.enemy_items[j];
                found = found || (!strcmp(it->key, enemy_rows[k].key) && it->arg == enemy_rows[k].arg && it->stack == enemy_rows[k].stack && enemy_rows[k].holder >= 0 && enemy_rows[k].holder < n_x && it->pokemon_id == run.enemy[enemy_rows[k].holder].b.id);
            }
            CHECK(found, "enemy lacks item %s/%d x%d on %d", enemy_rows[k].key, enemy_rows[k].arg, enemy_rows[k].stack, enemy_rows[k].holder);
            items_checked++;
        }
        {   /* trainerAI.instantTeras: none */
            int8_t teras[8];
            EQ("no instant teras", gc_trainer_instant_teras(&run, teras, 8), strcmp(tera_slots, "-") == 0 ? 0 : 1);
        }
        if (bad) mismatches++;
        cases++; bands[band]++;
    }
    printf("Mystery encounter The Bug-Type Superfan, the fight: %d cases run (bands %d/%d/%d/%d/%d/%d/%d/%d, %d members with a form, %d bosses), %d enemies compared, %d items, %d moves and %d tutor moves compared, %d fields. TOTAL mismatches %d\n",
           cases, bands[0], bands[1], bands[2], bands[3], bands[4], bands[5], bands[6], bands[7], forms, bosses, enemy_checked, items_checked, moves_checked, tutor_checked, fields_checked, mismatches);
    return mismatches ? 1 : 0;
}
