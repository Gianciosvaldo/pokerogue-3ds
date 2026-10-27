/* Training Session on the host: the parts the upstream differential (mystery_training_diff_test) cannot compare, because its oracle
 * does not play the battle: the picked Pokemon's clone really fights the rest of the party (and cannot be fled from), its items are the
 * party's clones that cannot be transferred, a win brings the Pokemon back at the END of the party with its items and the training's
 * effect (IVs, nature or ability) before the rewards, and with nobody else able to fight the run is over. */
#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static gc_run run;
static gc_system sys;

static void start(const char *seed, int level, int n_party)
{
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    gc_system_init_defaults(&sys);
    run.sys = &sys;
    run.wave = 45;
    run.biome = BIOME_ID_PLAINS;
    gc_items_to_battle(&run);
    run.n_party = n_party;
    static const int species[3] = {SPECIES_ID_PIKACHU, SPECIES_ID_LAPRAS, SPECIES_ID_EEVEE};
    for (int i = 0; i < n_party; i++) {
        gc_starter s;
        memset(&s, 0, sizeof s);
        s.species = species[i];
        s.tera_type = -1;
        for (int k = 0; k < 6; k++) s.ivs[k] = 15;
        s.moves[0] = MOVE_ID_TACKLE;
        gc_mon_new_player(&run, &run.party[i], &s, level);
        run.party[i].pause_evolutions = true;
        run.party_order[i] = (int8_t)i;
    }
    /* the first Pokemon holds a Leftovers */
    gc_item *it = &run.items[run.n_items++];
    memset(it, 0, sizeof *it);
    it->key = pr_item_keys[gc_reward_item_index("LEFTOVERS")];
    it->pokemon_id = run.party[0].b.id;
    it->stack = 1;
    gc_items_to_battle(&run);
    run.battle_type = BATTLE_TYPE_WILD;
    gc_rq_reset(&gc_int(&run)->rq);
    run.mystery_type = GC_ME_TRAINING_SESSION;
    gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_TRAINING_SESSION, GC_ME_COMMON, 0));
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_MYSTERY);
}

static int held(uint32_t id, const char *key)
{
    int n = 0;
    for (int i = 0; i < run.n_items; i++) if (run.items[i].pokemon_id == id && !strcmp(run.items[i].key, key)) n += run.items[i].stack;
    return n;
}

static void win_the_fight(void)
{
    const char *why;
    assert(run.screen.kind == GC_SCREEN_COMMAND);
    run.enemy[0].b.hp = 1;
    gc_choice fight = {GC_CH_FIGHT, 0, -1, 0};
    for (int guard = 0; guard < 12 && run.screen.kind == GC_SCREEN_COMMAND; guard++) assert(gc_run_choose(&run, &fight, &why));
}

static void test_light_training_returns_the_pokemon(void)
{
    puts("test_training_light_returns_the_pokemon...");
    start("trn-light", 60, 3);
    const uint32_t id = run.party[0].b.id;
    int before = 0;
    for (int k = 0; k < 6; k++) before += run.party[0].b.ivs[k];
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(run.screen.kind == GC_SCREEN_COMMAND);
    /* the clone fights the other two, with a new id, as a boss that carries a copy of the Leftovers nobody can take */
    assert(run.n_party == 2 && run.n_enemy == 1);
    assert(run.enemy[0].b.species == SPECIES_ID_PIKACHU && run.enemy[0].b.is_boss && run.enemy[0].b.id != id);
    assert(run.enemy[0].b.boss_segments == 2);
    assert(run.n_enemy_items == 1 && !strcmp(run.enemy_items[0].key, "LEFTOVERS") && run.enemy_items[0].non_transferable);
    assert(held(id, "LEFTOVERS") == 0);
    assert(!run.screen.can_run);
    gc_choice run_away = {GC_CH_RUN, 0, 0, 0};
    assert(!gc_run_choose(&run, &run_away, &why));
    assert(run.sys->dex[SPECIES_ID_PIKACHU].seen_attr == 0 || run.enemy[0].b.species == SPECIES_ID_PIKACHU);
    win_the_fight();
    assert(run.screen.kind == GC_SCREEN_REWARD);
    /* back at the end of the party, with its Leftovers and two IVs improved */
    assert(run.n_party == 3 && run.party[run.party_order[2]].b.id == id);
    assert(held(id, "LEFTOVERS") == 1);
    /* the battle's own view of the party has it again (the reward shop's level-ups read it) */
    assert(run.bt.n_party[0] == 3 && run.bt.party[0][2] == &run.party[run.party_order[2]].b);
    int after = 0;
    for (int k = 0; k < 6; k++) after += run.party[run.party_order[2]].b.ivs[k];
    assert(after >= before + 6 && after <= before + 20);
    puts("ok");
}

static void test_moderate_and_heavy_training(void)
{
    puts("test_training_moderate_and_heavy...");
    start("trn-nature", 60, 3);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 1, 1, NATURE_TIMID};
    assert(gc_run_choose(&run, &ch, &why));
    assert(run.n_party == 2 && run.enemy[0].b.species == SPECIES_ID_LAPRAS && run.enemy[0].b.boss_segments == 3);
    win_the_fight();
    assert(run.screen.kind == GC_SCREEN_REWARD && run.n_party == 3);
    const gc_mon *lapras = &run.party[run.party_order[2]];
    assert(lapras->b.species == SPECIES_ID_LAPRAS && lapras->has_custom_nature && lapras->b.nature == NATURE_TIMID);

    start("trn-ability", 60, 3);
    ch = (gc_choice){GC_CH_MYSTERY, 2, 2, 1};
    assert(gc_run_choose(&run, &ch, &why));
    assert(run.enemy[0].b.species == SPECIES_ID_EEVEE && run.enemy[0].b.boss_segments == 3);
    win_the_fight();
    assert(run.screen.kind == GC_SCREEN_REWARD && run.n_party == 3);
    const gc_mon *eevee = &run.party[run.party_order[2]];
    assert(eevee->b.species == SPECIES_ID_EEVEE && eevee->ability_index == 1);
    puts("ok");
}

static void test_nobody_left_to_fight(void)
{
    puts("test_training_nobody_left_to_fight...");
    start("trn-alone", 60, 2);
    run.party[1].b.hp = 0;
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(run.screen.kind == GC_SCREEN_GAME_OVER);
    puts("ok");
}

int main(void)
{
    test_light_training_returns_the_pokemon();
    test_moderate_and_heavy_training();
    test_nobody_left_to_fight();
    puts("all training host tests passed");
    return 0;
}
