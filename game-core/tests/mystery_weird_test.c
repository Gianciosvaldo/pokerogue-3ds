/* Weird Dream on the host: what the upstream differential (mystery_weird_diff_test) cannot compare, because its oracle stubs the phases around the option: the transformation and the
 * refusal played through the run's own phases (the texts in order, the full heal, the shop, the next wave), the dream's plan staying the same however often the screen is read, and the
 * transformed party (new species, the second type, the passive) surviving a save and a load. */
#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static gc_run run;
static gc_system sys;

static void start(const char *seed, int wave, int n)
{
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    gc_system_init_defaults(&sys);
    run.sys = &sys;
    run.wave = wave;
    run.biome = BIOME_ID_PLAINS;
    run.money = 1000;
    gc_items_to_battle(&run);
    run.n_party = n;
    static const int species[6] = {SPECIES_ID_SNORLAX, SPECIES_ID_PIKACHU, SPECIES_ID_GENGAR, SPECIES_ID_EEVEE, SPECIES_ID_MAGIKARP, SPECIES_ID_RATTATA};
    for (int i = 0; i < n; i++) {
        gc_starter s;
        memset(&s, 0, sizeof s);
        s.species = species[i];
        s.tera_type = -1;
        for (int k = 0; k < 6; k++) s.ivs[k] = 15;
        s.moves[0] = MOVE_ID_TACKLE;
        s.moves[1] = i == 1 ? MOVE_ID_THUNDERBOLT : MOVE_ID_SHADOW_BALL;
        gc_mon_new_player(&run, &run.party[i], &s, 400);
        run.party[i].pause_evolutions = true;
        run.party_order[i] = (int8_t)i;
    }
    gc_items_to_battle(&run);
    run.battle_type = BATTLE_TYPE_WILD;
    gc_rq_reset(&gc_int(&run)->rq);
    gc_weird_reset_ranges();
    run.mystery_type = GC_ME_WEIRD_DREAM;
    gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_WEIRD_DREAM, GC_ME_ROGUE, 0));
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_MYSTERY);
}

static char log_keys[40][96];
static int n_log;
static void log_events(void)
{
    gc_event e;
    while (gc_run_next_event(&run, &e)) {
        if (e.kind != GC_EV_MESSAGE || !e.msg.key) continue;
        if (n_log < 40) snprintf(log_keys[n_log++], sizeof log_keys[0], "%s", e.msg.key);
    }
}
static int logged_at(const char *key)
{
    for (int i = 0; i < n_log; i++) if (!strcmp(log_keys[i], key)) return i;
    return -1;
}

static void leave_shop(void)
{
    const char *why;
    gc_choice skip = {GC_CH_SKIP, 0, 0, 0};
    for (int guard = 0; guard < 6 && run.screen.kind == GC_SCREEN_REWARD; guard++) {
        assert(gc_run_choose(&run, &skip, &why));
        gc_run_until_screen(&run);
    }
}

static void test_plan_is_stable(void)
{
    puts("test_weird_plan...");
    start("wd-plan", 80, 4);
    int first[4];
    for (int k = 0; k < 4; k++) { first[k] = gc_weird_species(&run, k); assert(first[k] > 0 && first[k] != run.party[k].b.species); }
    const pr_rng before = run.run.rng;
    for (int again = 0; again < 3; again++)
        for (int k = 0; k < 4; k++) assert(gc_weird_species(&run, k) == first[k]);
    assert(run.run.rng.c == before.c);   /* the plan uses the wave's own seed, never the run's stream */
    gc_mon a, b;
    assert(gc_weird_new_pokemon(&run, 1, &a) && gc_weird_new_pokemon(&run, 1, &b));
    assert(a.b.species == first[1] && a.b.species == b.b.species && !memcmp(a.b.ivs, b.b.ivs, sizeof a.b.ivs) && a.b.nature == b.b.nature);
    puts("ok");
}

static void test_transformation(void)
{
    puts("test_weird_transformation...");
    for (int n = 1; n <= 6; n += 5) {
        n_log = 0;
        start("wd-dream", 90, n);
        int planned[6], old[6];
        for (int k = 0; k < n; k++) { planned[k] = gc_weird_species(&run, k); old[k] = run.party[k].b.species; }
        const char *why;
        gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};
        assert(gc_run_choose(&run, &ch, &why));
        log_events();
        assert(run.n_party == n);
        int passives = 0;
        for (int k = 0; k < n; k++) {
            const gc_mon *m = &run.party[run.party_order[k]];
            assert(m->b.species == planned[k] && m->b.species != old[k]);
            assert(m->b.custom_types[1] != 0);                      /* a random second type */
            assert(m->b.hp == m->b.stats[0] && m->b.hp > 0);        /* the full heal that follows */
            passives += m->b.passive_flag;
        }
        assert(passives >= 1);
        int gateaus = 0;
        for (int i = 0; i < run.n_items; i++)
            if (!strcmp(run.items[i].key, "MYSTERY_ENCOUNTER_OLD_GATEAU")) { gateaus++; assert(run.items[i].non_transferable); }   /* the item upstream makes with isTransferable false */
        int small = 0;
        for (int k = 0; k < n; k++) small += gc_form(planned[k], 0)->base_total < 570;
        assert(gateaus == small);   /* one Old Gateau for each new Pokemon under 570 base stat total */
        const int cut = logged_at("mysteryEncounters/weirdDream:option.1.cutscene"), done = logged_at("mysteryEncounters/weirdDream:option.1.dreamComplete");
        assert(cut >= 0 && done > cut);
        assert(run.screen.kind == GC_SCREEN_REWARD);                 /* the shop: Memory Mushroom, Rogue Ball, four Mints */
        const int wave = run.wave;
        leave_shop();
        assert(run.wave == wave + 1 && run.screen.kind == GC_SCREEN_COMMAND && gc_run_can_save(&run));

        static uint8_t buf[1 << 20];
        const size_t len = gc_run_save(&run, buf, sizeof buf);
        assert(len > 0);
        static gc_run loaded;
        memset(&loaded, 0, sizeof loaded);
        assert(gc_run_load(&loaded, buf, len, &why));
        assert(loaded.n_party == n);
        for (int k = 0; k < n; k++) {
            const gc_mon *a = &run.party[run.party_order[k]], *b = &loaded.party[loaded.party_order[k]];
            assert(a->b.species == b->b.species && a->b.custom_types[1] == b->b.custom_types[1] && a->b.passive_flag == b->b.passive_flag);
        }
    }
    puts("ok");
}

static void test_refusal(void)
{
    puts("test_weird_refusal...");
    start("wd-refuse", 100, 3);
    /* a level 1 and a level 2 member: ceil(0.9) = 1, ceil(1.8) = 2, and never below 1 */
    run.party[1].b.level = 1; gc_mon_recalc(&run, &run.party[1]);
    run.party[2].b.level = 2; gc_mon_recalc(&run, &run.party[2]);
    int before[3];
    for (int k = 0; k < 3; k++) before[k] = run.party[k].b.level;
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 2, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    for (int k = 0; k < 3; k++) {
        const double want = ceil(0.9 * before[k]);
        assert(run.party[k].b.level == (int)want && (run.party[k].b.level < before[k] || before[k] <= 2));
        assert(run.party[k].exp == (double)gc_level_total_exp(run.party[k].b.level, gc_species(run.party[k].b.species)->growth_rate));
    }
    assert(run.screen.kind == GC_SCREEN_REWARD);                     /* the heal-only shop */
    const int wave = run.wave;
    leave_shop();
    assert(run.wave == wave + 1);
    for (int k = 0; k < 3; k++) assert(run.party[k].b.hp == run.party[k].b.stats[0]);
    puts("ok");
}

/* the fight on the command screen is won: the foes are weakened so that one hit knocks them out and none can hurt the lead */
static void play_the_fight(void)
{
    const char *why;
    for (int e = 0; e < run.n_enemy; e++) {
        run.enemy[e].b.hp = 1;
        run.enemy[e].b.stats[1] = run.enemy[e].b.stats[3] = 1;
    }
    gc_choice fight = {GC_CH_FIGHT, 0, -1, 0};
    for (int guard = 0; guard < 200 && run.screen.kind == GC_SCREEN_COMMAND; guard++) {
        assert(gc_run_choose(&run, &fight, &why));
        log_events();
        if (run.screen.kind == GC_SCREEN_LEARN_MOVE) {
            gc_choice forget = {GC_CH_FORGET, -1, 0, 0};
            assert(gc_run_choose(&run, &forget, &why));
            gc_run_until_screen(&run);
            log_events();
        }
    }
}

static void test_fight(void)
{
    puts("test_weird_fight...");
    n_log = 0;
    start("wd-fight", 100, 4);
    int planned[4];
    for (int k = 0; k < 4; k++) planned[k] = gc_weird_species(&run, k);
    int passives_before = 0;
    for (int k = 0; k < 4; k++) passives_before += run.party[run.party_order[k]].b.passive_flag;
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 1, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    log_events();
    /* the trainer fight against the team the dream would have made; the player's own party is untouched */
    assert(run.screen.kind == GC_SCREEN_COMMAND && gc_int(&run)->weird.fight);
    assert(run.battle_type == BATTLE_TYPE_TRAINER && run.trainer_type == TRAINER_TYPE_PLAYER_M_ALTERNATE && !run.double_battle && run.me_battle);
    assert(run.n_enemy == 4 && !run.screen.can_run && !run.screen.can_ball && !run.me_catch_allowed);
    assert(logged_at("mysteryEncounters/weirdDream:option.2.selected2") >= 0 && logged_at("mysteryEncounters/weirdDream:option.1.cutscene") < 0);
    for (int k = 0; k < 4; k++) {
        const gc_mon *e = &run.enemy[k];
        assert(e->b.species == planned[k] && e->trainer_slot == TRAINER_SLOT_TRAINER && !e->b.passive_flag && e->b.hp == e->b.stats[0] && !e->b.is_player);
        assert(e->b.custom_types[1] != 0);                                      /* the second type the dream gave */
        assert(e->b.is_boss == (gc_form(e->b.species, e->b.form)->base_total > 570));
        assert(run.party[k].b.species != planned[k] && run.party[k].b.custom_types[1] == 0);
        int held = 0;
        for (int i = 0; i < run.n_enemy_items; i++) if (run.enemy_items[i].pokemon_id == e->b.id) { held++; assert(run.enemy_items[i].non_transferable); }
        assert(held >= (gc_form(e->b.species, e->b.form)->base_total < 570));   /* an Old Gateau at least for a Pokemon under 570 */
    }
    assert(sys.dex[run.enemy[0].b.species].seen_attr != 0);                      /* the foes count as seen */
    play_the_fight();
    /* the win: the shop of six tiers (no fill), the passive of one more Pokemon, then the next wave */
    assert(run.screen.kind == GC_SCREEN_REWARD && !gc_int(&run)->weird.fight);
    int passives_after = 0;
    for (int k = 0; k < 4; k++) passives_after += run.party[run.party_order[k]].b.passive_flag;
    assert(passives_after == passives_before + 1);
    assert(run.screen.n_options == 6);
    static const int tiers[6] = {MODIFIER_TIER_ROGUE, MODIFIER_TIER_ROGUE, MODIFIER_TIER_ULTRA, MODIFIER_TIER_ULTRA, MODIFIER_TIER_GREAT, MODIFIER_TIER_GREAT};
    for (int k = 0; k < 6; k++) assert(run.screen.options[k].tier == tiers[k]);
    assert(logged_at("battle:trainerDefeated") >= 0);
    const int wave = run.wave;
    leave_shop();
    assert(run.wave == wave + 1 && run.screen.kind == GC_SCREEN_COMMAND);
    puts("ok");
}

int main(void)
{
    test_plan_is_stable();
    test_transformation();
    test_refusal();
    test_fight();
    puts("all weird dream host tests passed");
    return 0;
}
