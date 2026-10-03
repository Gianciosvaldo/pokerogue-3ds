/* Additive resume regression. Uses the unchanged soak chooser and save path. */
#define main gcplay_program_main
#include "../tools/gcplay.c"
#undef main
#include "../src/internal.h"

static int checks;
#define VERIFY(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL resume line %d: %s\n", __LINE__, #c); return 1; } } while (0)

static int fixtures(void)
{
    static gc_run original, loaded;
    static uint8_t buf[GC_SAVE_MAX];
    const char *why = NULL;
    gc_starter team[3] = {0};
    team[0].species = SPECIES_ID_BULBASAUR;
    team[1].species = SPECIES_ID_CHARMANDER;
    team[2].species = SPECIES_ID_SQUIRTLE;
    for (int i = 0; i < 3; i++) { team[i].tera_type = -1; memset(team[i].ivs, 15, 6); }
    for (int trainer = 0; trainer < 2; trainer++) {
        gc_run_new(&original, GAME_MODES_CLASSIC, "resume-regression", 7);
        VERIFY(gc_run_set_starters(&original, team, 3, &why));
        if (trainer) {
            original.battle_type = BATTLE_TYPE_TRAINER;
            original.enemy[0].b.ability = original.enemy[0].b.base_ability = ABILITY_ID_INTIMIDATE;
            original.enemy[0].b.ability_override = ABILITY_ID_INTIMIDATE;
            gc_battle_setup_field(&original, true);
            VERIFY(original.party[original.party_order[0]].b.stat_stages[STAT_ATK - 1] == -1);
        }
        original.wave = original.bt.wave_index = 48;
        original.mystery_type = GC_ME_FIGHT_OR_FLIGHT;
        original.mystery_tier = GC_ME_COMMON;
        original.bt.battle_rng = original.run.rng;
        original.bt.battle_rng_live = true;
        pr_rng_seed_int(&original.bt.battle_rng, 4, 0);
        for (int history = 0; history <= GC_MAX_MYSTERY_HISTORY; history += GC_MAX_MYSTERY_HISTORY) {
            original.n_mystery_history = (uint8_t)history;
            for (int i = 0; i < history; i++)
                original.mystery_history[i] = (gc_mystery_seen){GC_ME_DEPARTMENT_STORE_SALE, GC_ME_COMMON, (uint16_t)(i + 1)};
            size_t len = gc_run_save(&original, buf, sizeof buf);
            VERIFY(len > 0);
            memset(&loaded, 0xA5, sizeof loaded);
            VERIFY(gc_run_load(&loaded, buf, len, &why) && !loaded.load_dropped);
            VERIFY(loaded.n_mystery_history == original.n_mystery_history);
            VERIFY(!memcmp(loaded.mystery_history, original.mystery_history, (size_t)history * sizeof(gc_mystery_seen)));
            VERIFY(loaded.mystery_type == original.mystery_type && loaded.mystery_tier == original.mystery_tier);
            VERIFY(!memcmp(loaded.party[loaded.party_order[0]].b.stat_stages,
                           original.party[original.party_order[0]].b.stat_stages, sizeof original.party[0].b.stat_stages));
            VERIFY(loaded.enemy[0].b.on_field == original.enemy[0].b.on_field);
            VERIFY(loaded.bt.field[2] == &loaded.enemy[0].b);
            VERIFY(loaded.bt.battle_rng_live);
            VERIFY(!memcmp(&loaded.bt.battle_rng, &original.bt.battle_rng, sizeof(pr_rng)));
            for (int i = 0; i < 128; i++) {
                original.run.rng = original.bt.battle_rng;
                loaded.run.rng = loaded.bt.battle_rng;
                int a = -1, b = -1, at = -1, bt = -1;
                VERIFY(gc_mystery_select(&original, &a, &at) == gc_mystery_select(&loaded, &b, &bt));
                VERIFY(a == b && at == bt);
                VERIFY(!memcmp(&original.run.rng, &loaded.run.rng, sizeof(pr_rng)));
                VERIFY(pr_rng_seed_int(&original.bt.battle_rng, 256, 0) == pr_rng_seed_int(&loaded.bt.battle_rng, 256, 0));
            }
        }
        original.n_mystery_history = GC_MAX_MYSTERY_HISTORY + 1;
        VERIFY(gc_run_save(&original, buf, sizeof buf) == 0);
        original.n_mystery_history = 1;
        original.mystery_history[0].type = GC_ME_COUNT;
        size_t len = gc_run_save(&original, buf, sizeof buf);
        VERIFY(len > 0 && !gc_run_load(&loaded, buf, len, &why));
    }
    return 0;
}

int main(void)
{
    if (fixtures()) return 1;
    const int seeds[] = {235, 431, 558};
    for (size_t i = 0; i < sizeof seeds / sizeof seeds[0]; i++) {
        soak_result a = {0}, b = {0};
        save_test = 0; soak_one(seeds[i], 0, &a);
        save_test = 1; soak_one(seeds[i], 0, &b);
        VERIFY(a.status == 0 && b.status == 0);
        VERIFY(a.wave == b.wave && a.turns == b.turns && a.score == b.score && a.clear == b.clear);
        VERIFY(!memcmp(a.stubs, b.stubs, sizeof a.stubs));
        VERIFY(!memcmp(a.unported, b.unported, sizeof a.unported));
        VERIFY(!memcmp(a.run_core, b.run_core, sizeof a.run_core));
        printf("resume seed soak%08d: wave %d, turns %d, score %d, identical\n", seeds[i], a.wave, a.turns, a.score);
    }
    printf("resume regression: %d checks passed\n", checks);
    return 0;
}
