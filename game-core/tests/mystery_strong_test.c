/* The Strong Stuff on the host: the parts the upstream differential (mystery_strong_diff_test) cannot compare, because its oracle does not
 * play the battle: the juice really changes the party's stats, the Shuckle's fight cannot be fled from or caught and never asks "Will you
 * switch?", Gastro Acid and Stealth Rock land before the first command, the Shuckle's Defense and Sp. Def are up a stage, and winning
 * gives the Soul Dew in the ordinary shop. */
#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static gc_run run;
static gc_system sys;

static void start(const char *seed, int wave, bool ask_switch)
{
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    gc_system_init_defaults(&sys);
    run.sys = &sys;
    run.wave = wave;
    run.biome = BIOME_ID_PLAINS;
    run.ask_switch = ask_switch;
    gc_items_to_battle(&run);
    run.n_party = 3;
    for (int i = 0; i < 3; i++) {
        gc_starter s;
        memset(&s, 0, sizeof s);
        /* Pikachu (BST 320) < Lapras (535) < Onix... Onix is 385: Lapras and Onix take the -15 */
        s.species = i == 0 ? SPECIES_ID_PIKACHU : i == 1 ? SPECIES_ID_LAPRAS : SPECIES_ID_ONIX;
        s.tera_type = -1;
        for (int k = 0; k < 6; k++) s.ivs[k] = 15;
        s.moves[0] = MOVE_ID_TACKLE;
        gc_mon_new_player(&run, &run.party[i], &s, 50);
        run.party[i].pause_evolutions = true;
        run.party_order[i] = (int8_t)i;
    }
    gc_items_to_battle(&run);
    run.battle_type = BATTLE_TYPE_WILD;
    gc_rq_reset(&gc_int(&run)->rq);
    run.mystery_type = GC_ME_THE_STRONG_STUFF;
    gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_THE_STRONG_STUFF, GC_ME_GREAT, 0));
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_MYSTERY);
}

static int juice(int mon_index, int arg)
{
    int n = 0;
    for (int i = 0; i < run.n_items; i++) {
        const gc_item *it = &run.items[i];
        if (!strcmp(it->key, "MYSTERY_ENCOUNTER_SHUCKLE_JUICE") && it->arg == arg && it->pokemon_id == run.party[mon_index].b.id) n += it->stack;
    }
    return n;
}

static void test_juice_changes_the_party(void)
{
    puts("test_strong_juice_changes_the_party...");
    start("strong-juice", 45, false);
    const int hp_before = run.party[0].b.stats[STAT_HP], atk_before = run.party[0].b.stats[STAT_ATK];
    const int lapras_before = run.party[1].b.stats[STAT_ATK], onix_before = run.party[2].b.stats[STAT_ATK];
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    /* Lapras and Onix hold the two highest base stat totals: Foul juice; Pikachu gets the Sweet one */
    assert(juice(1, -15) == 1 && juice(2, -15) == 1 && juice(0, 10) == 1);
    assert(juice(0, -15) == 0 && juice(1, 10) == 0);
    assert(run.party[0].b.stats[STAT_ATK] > atk_before && run.party[0].b.stats[STAT_HP] >= hp_before);
    assert(run.party[1].b.stats[STAT_ATK] < lapras_before && run.party[2].b.stats[STAT_ATK] < onix_before);
    /* the ordinary shop follows, with no battle and no heal shop */
    assert(run.screen.kind == GC_SCREEN_REWARD && !run.me_battle && run.n_enemy == 0);
    puts("ok");
}

static void test_fight(void)
{
    puts("test_strong_fight...");
    start("strong-fight", 45, true);   /* the SWITCH style asks "Will you switch?" in other fights */
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 1, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    /* disableSwitch: no question even in the SWITCH style; no fleeing, no catching */
    assert(run.screen.kind == GC_SCREEN_COMMAND);
    assert(run.ask_switch);
    assert(run.n_enemy == 1 && run.enemy[0].b.species == SPECIES_ID_SHUCKLE && run.enemy[0].b.is_boss && run.enemy[0].b.boss_segments == 5);
    assert(!run.screen.can_run);
    assert(!run.me_catch_allowed);
    /* Gastro Acid and Stealth Rock were used before the first command: the lead's ability is off and the rocks lie on the player's side */
    assert(run.party[run.party_order[0]].b.ability_suppressed);
    const pr_arena_tag *rocks = pr_arena_get_tag_on_side(&run.bt, ARENA_TAG_TYPE_STEALTH_ROCK, ARENA_TAG_SIDE_PLAYER);
    assert(rocks);
    /* the boss's own boost: Defense and Sp. Def up one stage (Simple doubles it, Contrary turns it down) */
    const gc_mon *e = &run.enemy[0];
    int want = 1;
    if (e->b.ability == ABILITY_ID_SIMPLE || e->b.passive == ABILITY_ID_SIMPLE) want = 2;
    if (e->b.ability == ABILITY_ID_CONTRARY || e->b.passive == ABILITY_ID_CONTRARY) want = -1;
    assert(e->b.stat_stages[STAT_DEF - 1] == want && e->b.stat_stages[STAT_SPDEF - 1] == want);
    /* winning gives the Soul Dew among the ordinary rewards */
    run.enemy[0].b.hp = 1;
    gc_choice fight = {GC_CH_FIGHT, 0, -1, 0};
    for (int guard = 0; guard < 8 && run.screen.kind == GC_SCREEN_COMMAND; guard++) assert(gc_run_choose(&run, &fight, &why));
    assert(run.screen.kind == GC_SCREEN_REWARD);
    bool dew = false;
    for (int i = 0; i < run.screen.n_options; i++) dew |= !strcmp(run.screen.options[i].key, "SOUL_DEW");
    assert(dew && run.screen.n_options >= 3);
    puts("ok");
}

int main(void)
{
    test_juice_changes_the_party();
    test_fight();
    puts("all strong host tests passed");
    return 0;
}
