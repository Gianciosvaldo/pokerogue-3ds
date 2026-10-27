/* Slumbering Snorlax on the host: the parts the upstream differential (mystery_snorlax_diff_test) cannot compare, because its oracle
 * does not play the battle: the start-of-battle Snore is really used by the sleeping boss (its flinch and Endure tags are lapsed by the
 * cleanup phase), the player cannot flee, a win opens the Leftovers shop and the rest of the rewards, the rest option heals and leaves. */
#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static gc_run run;
static gc_system sys;

static void start(const char *seed, int level)
{
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    gc_system_init_defaults(&sys);
    run.sys = &sys;
    run.wave = 45;
    run.biome = BIOME_ID_PLAINS;
    gc_items_to_battle(&run);
    run.n_party = 2;
    for (int i = 0; i < 2; i++) {
        gc_starter s;
        memset(&s, 0, sizeof s);
        s.species = i ? SPECIES_ID_LAPRAS : SPECIES_ID_PIKACHU;
        s.tera_type = -1;
        for (int k = 0; k < 6; k++) s.ivs[k] = 15;
        s.moves[0] = MOVE_ID_TACKLE;
        gc_mon_new_player(&run, &run.party[i], &s, level);
        run.party[i].pause_evolutions = true;
        run.party_order[i] = (int8_t)i;
    }
    run.battle_type = BATTLE_TYPE_WILD;
    gc_rq_reset(&gc_int(&run)->rq);
    run.mystery_type = GC_ME_SLUMBERING_SNORLAX;
    gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_SLUMBERING_SNORLAX, GC_ME_GREAT, 0));
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_MYSTERY);
}

static bool message_seen(const char *key)
{
    gc_event e;
    bool seen = false;
    while (gc_run_next_event(&run, &e)) if (e.kind == GC_EV_MESSAGE && e.msg.key && !strcmp(e.msg.key, key)) seen = true;
    return seen;
}

static void test_snore_and_no_flee(void)
{
    puts("test_snorlax_snore_and_no_flee...");
    start("sno-snore", 60);
    assert(run.screen.n_options == 3 && run.screen.option_ok[0] && run.screen.option_ok[1]);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(run.screen.kind == GC_SCREEN_COMMAND);
    assert(run.n_enemy == 1 && run.enemy[0].b.species == SPECIES_ID_SNORLAX && run.enemy[0].b.is_boss);
    /* the Snore was used at the lead before the first command */
    gc_mon *lead = &run.party[run.party_order[0]];
    assert(lead->b.hp > 0 && lead->b.hp < lead->b.stats[0]);
    /* the boss is still asleep, the Snore used up one of its turns, and no flinch or Endure is left over */
    assert(run.enemy[0].b.status == STATUS_EFFECT_SLEEP && run.enemy[0].b.status_sleep_turns < 6);
    for (int i = 0; i < lead->b.n_tags; i++) assert(lead->b.tags[i].type != BATTLER_TAG_TYPE_FLINCHED);
    assert(!run.screen.can_run);
    gc_choice run_away = {GC_CH_RUN, 0, 0, 0};
    assert(!gc_run_choose(&run, &run_away, &why));
    puts("ok");
}

static void test_win_opens_leftovers(void)
{
    puts("test_snorlax_win_opens_leftovers...");
    start("sno-win", 100);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(run.screen.kind == GC_SCREEN_COMMAND);
    run.enemy[0].b.hp = 1;
    gc_choice fight = {GC_CH_FIGHT, 0, -1, 0};
    for (int guard = 0; guard < 8 && run.screen.kind == GC_SCREEN_COMMAND; guard++) assert(gc_run_choose(&run, &fight, &why));
    assert(run.screen.kind == GC_SCREEN_REWARD);
    bool leftovers = false;
    for (int i = 0; i < run.screen.n_options; i++) leftovers = leftovers || !strcmp(run.screen.options[i].key, "LEFTOVERS");
    assert(leftovers);
    /* fillRemaining: the rest of the ordinary rewards come with it */
    assert(run.screen.n_options >= 2);
    puts("ok");
}

static void test_rest_heals_and_leaves(void)
{
    puts("test_snorlax_rest_heals_and_leaves...");
    start("sno-rest", 40);
    gc_mon *lead = &run.party[run.party_order[0]];
    lead->b.hp = 1;
    run.party[run.party_order[1]].b.hp = 0;
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 1, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    assert(lead->b.hp == lead->b.stats[0]);
    assert(run.party[run.party_order[1]].b.hp == run.party[run.party_order[1]].b.stats[0]);
    /* no shop: the run goes straight to the next wave */
    assert(run.wave == 46 && run.screen.kind != GC_SCREEN_REWARD && !run.me_battle);
    assert(message_seen("mysteryEncounters/slumberingSnorlax:option.2.restResult"));
    puts("ok");
}

int main(void)
{
    test_snore_and_no_flee();
    test_win_opens_leftovers();
    test_rest_heals_and_leaves();
    puts("all snorlax host tests passed");
    return 0;
}
