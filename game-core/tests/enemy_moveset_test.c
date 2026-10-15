/* Task 64: adapter/constructor host consistency, not a new upstream differential.
 * Direct Task 63 output is the oracle for move order, maximum PP and RNG.
 * Also exercise the real wild constructor with its finish draws and fallback counts. */
#include "../src/internal.h"
#include "pr/moveset.h"
#include <stdio.h>
#include <stdlib.h>

static gc_run run;
static unsigned checks, supported, fallback, wild_supported, wild_fallback, wild_draws;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "enemy moveset line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

static void setup(unsigned seed)
{
    memset(&run, 0, sizeof run);
    run.run.mode = GAME_MODES_CLASSIC;
    run.battle_type = BATTLE_TYPE_WILD;
    run.wave = 1;
    run.biome = BIOME_ID_TOWN;
    char text[40];
    snprintf(text, sizeof text, "moveset-adapter-%u", seed);
    pr_seedstr s;
    pr_seed_from_ascii(&s, text);
    pr_rng_sow(&run.run.rng, &s);
}

static void check_moves(const gc_mon *m, const pr_generated_moveset *out)
{
    CHECK(m->b.n_moves == out->count);
    for (int i = 0; i < out->count; i++) {
        CHECK(m->b.moves[i] == out->moves[i]);
        CHECK(gc_move_max_pp(&m->b, i) == out->pp[i]);
        CHECK(m->b.pp_used[i] == 0);
        /* Preserve actual Source PokemonMove.ppUp, including Recover -4. */
        CHECK(m->b.pp_up[i] == out->pp_up[i]);
        CHECK(m->b.max_pp_override[i] == -1);
    }
}

static void adapter_matrix(void)
{
    const int species[] = {SPECIES_ID_CATERPIE, SPECIES_ID_WEEDLE, SPECIES_ID_MAGIKARP,
        SPECIES_ID_DITTO, SPECIES_ID_SMEARGLE, SPECIES_ID_ETERNATUS, SPECIES_ID_RATTATA};
    const int levels[] = {1, 5, 24, 25, 70, 100};
    for (unsigned si = 0; si < sizeof species / sizeof *species; si++)
    for (unsigned li = 0; li < sizeof levels / sizeof *levels; li++)
    for (int nature = 0; nature < PR_NATURE_COUNT; nature++)
    for (int boss = 0; boss < 2; boss++) {
        setup(checks);
        const int id = species[si], form = id == SPECIES_ID_ETERNATUS ? nature % 2 : 0;
        pr_enemy_gen g = {.id = 123, .fusion_species = -1, .friendship = -1,
            .form = form, .nature = nature, .boss_segments = boss ? 2 : 0};
        for (int k = 0; k < 6; k++) g.ivs[k] = 31;
        gc_mon m;
        gc_mon_new_enemy(&run, &m, id, levels[li], &g, 0);
        /* Poison previous-battle modifiers: the new constructor must ignore them. */
        run.bt.n_mods[1] = 1;
        run.bt.mods[1][0] = (pr_modifier){.cls = PR_MOD_BASE_STAT,
            .pokemon_id = g.id, .stat = STAT_ATK, .stack_count = 10};
        const pr_form *f = gc_form(id, form);
        int atk = (int)floor((2 * f->base_stats[STAT_ATK] + 31) * levels[li] * .01) + 5;
        int spa = (int)floor((2 * f->base_stats[STAT_SPATK] + 31) * levels[li] * .01) + 5;
        double a = pr_nature_mult[nature][STAT_ATK], b = pr_nature_mult[nature][STAT_SPATK];
        atk = a > 1 ? (int)ceil(atk * a) : (int)floor(atk * a);
        spa = b > 1 ? (int)ceil(spa * b) : (int)floor(spa * b);
        pr_moveset_ctx c = {.species = id, .form = form, .level = levels[li],
            .ability = m.b.ability, .attack = atk, .special_attack = spa,
            .boss = boss, .passive = boss};
        pr_rng expected_rng = run.run.rng;
        pr_generated_moveset out;
        bool expected = pr_run_enemy_moveset(&expected_rng, &c, &out) == PR_MOVESET_OK;
        gc_mon before = m;
        CHECK(gc_enemy_moveset(&run, &m, &g) == expected);
        CHECK(!memcmp(&run.run.rng, &expected_rng, sizeof expected_rng));
        CHECK(run.stubs[GC_STUB_ENEMY_MOVESET] == 0);
        if (expected) { check_moves(&m, &out); supported++; }
        else { CHECK(!memcmp(&m, &before, sizeof m)); fallback++; }
    }
}

static void unsupported_boundaries(void)
{
    for (int variant = 0; variant < 9; variant++) {
        setup(variant);
        pr_enemy_gen g = {.id = 123, .fusion_species = -1, .friendship = -1};
        gc_mon m;
        gc_mon_new_enemy(&run, &m, SPECIES_ID_MAGIKARP, 10, &g, 0);
        switch (variant) {
        case 0: m.b.has_trainer = true; break;
        case 1: m.trainer_slot = TRAINER_SLOT_TRAINER; break;
        case 2: run.battle_type = BATTLE_TYPE_MYSTERY_ENCOUNTER; break;
        case 3: run.run.mode = GAME_MODES_DAILY; break;
        case 4: run.run.has_challenges = true; break;
        case 5: g.fusion_species = SPECIES_ID_DITTO; break;
        case 6: m.b.fusion_species = SPECIES_ID_DITTO; break;
        case 7: run.run.mode = GAME_MODES_SPLICED_ENDLESS; break;
        case 8: run.me_battle = true; break;
        }
        gc_mon before = m;
        pr_rng rng = run.run.rng;
        CHECK(!gc_enemy_moveset(&run, &m, &g));
        CHECK(!memcmp(&m, &before, sizeof m));
        CHECK(!memcmp(&run.run.rng, &rng, sizeof rng));
    }
}

static void wild_constructor(void)
{
    /* The real wild entry point must consume moveset draws BETWEEN the base and
     * finish draws. Check both supported encounters and unchanged fallback paths. */
    for (int seed = 0; seed < 800; seed++) {
        setup(seed);
        if (seed >= 400) run.biome = BIOME_ID_GRASS;
        pr_run expected = run.run;
        pr_wild_enemy_ctx wc = {0};
        wc.level = 5;
        wc.enemy.wave = run.wave;
        wc.enemy.biome = run.biome;
        wc.enemy.trainer_type = 0;
        wc.enemy.event_shiny_multiplier = 1;
        pr_wild_enemy w;
        CHECK(pr_run_gen_wild_enemy(&expected, &wc, &w) == 0);
        gc_mon m;
        gc_mon_new_enemy(&run, &m, w.species, wc.level, &w.mon, 0);
        pr_battle bt = {0};
        bt.game_mode = GAME_MODES_CLASSIC; bt.wave_index = 1;
        pr_calculate_stats(&bt, &m.b);
        pr_moveset_ctx c = {.species = w.species, .form = m.b.form, .level = wc.level,
            .ability = m.b.ability, .attack = m.b.stats[STAT_ATK], .special_attack = m.b.stats[STAT_SPATK]};
        pr_generated_moveset out;
        pr_rng before_moves = expected.rng;
        bool ok = pr_run_enemy_moveset(&expected.rng, &c, &out) == PR_MOVESET_OK;
        if (ok && memcmp(&before_moves, &expected.rng, sizeof before_moves)) wild_draws++;
        wc.enemy.species = w.species; wc.enemy.level = wc.level;
        wc.enemy.boss = w.boss; wc.enemy.shiny_lock = w.shiny_lock;
        CHECK(pr_run_enemy_pokemon_finish(&expected, &wc.enemy, &w.mon) == 0);
        CHECK(gc_gen_wild(&run, 5));
        CHECK(run.n_enemy == 1);
        CHECK(run.enemy[0].b.species == w.species);
        CHECK(run.enemy[0].b.nature == w.mon.nature);
        CHECK(run.enemy[0].b.tera_type == w.mon.tera_type);
        CHECK(!memcmp(&run.run.rng, &expected.rng, sizeof expected.rng));
        CHECK(run.stubs[GC_STUB_ENEMY_MOVESET] == (unsigned)!ok);
        if (ok) { check_moves(&run.enemy[0], &out); wild_supported++; }
        else wild_fallback++;
    }
    CHECK(wild_supported > 0 && wild_fallback > 0 && wild_draws > 0);
}

static void saved_fixed_moves(void)
{
    static gc_run loaded;
    static unsigned char buf[GC_SAVE_MAX];
    gc_run_new(&run, GAME_MODES_CLASSIC, "moveset-save", 777);
    gc_starter starter = {.species = SPECIES_ID_BULBASAUR, .tera_type = -1};
    const char *why;
    CHECK(gc_run_set_starters(&run, &starter, 1, &why));
    /* Keep a legal encounter save point while replacing its enemy with the
     * fixed form's generated moves. This checks representation/persistence,
     * not the deferred final boss transition or its form-selection logic. */
    pr_enemy_gen g = {.id = 987, .fusion_species = -1, .friendship = -1, .form = 1};
    gc_mon_new_enemy(&run, &run.enemy[0], SPECIES_ID_ETERNATUS, 100, &g, 0);
    CHECK(gc_enemy_moveset(&run, &run.enemy[0], &g));
    CHECK(run.enemy[0].b.moves[3] == MOVE_ID_RECOVER);
    CHECK(run.enemy[0].b.pp_up[3] == -4);
    CHECK(run.enemy[0].b.max_pp_override[3] == -1);
    CHECK(gc_move_max_pp(&run.enemy[0].b, 3) == 1);
    size_t n = gc_run_save(&run, buf, sizeof buf);
    CHECK(n > 0);
    CHECK(gc_run_load(&loaded, buf, n, &why));
    CHECK(!memcmp(run.enemy[0].b.moves, loaded.enemy[0].b.moves, sizeof run.enemy[0].b.moves));
    CHECK(!memcmp(run.enemy[0].b.pp_up, loaded.enemy[0].b.pp_up, sizeof run.enemy[0].b.pp_up));
    CHECK(gc_move_max_pp(&loaded.enemy[0].b, 3) == 1);
    CHECK(!memcmp(&run.run.rng, &loaded.run.rng, sizeof run.run.rng));
}

int main(void)
{
    adapter_matrix();
    unsupported_boundaries();
    wild_constructor();
    saved_fixed_moves();
    printf("enemy moveset adapter: %u checks, %u supported, %u rejected; wild constructors %u supported (%u draw RNG), %u fallback\n",
        checks, supported, fallback, wild_supported, wild_draws, wild_fallback);
    return 0;
}
