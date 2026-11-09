/* The Safari Zone on the host, through gc_run_choose as the app plays it: the parts the upstream differential (mystery_safari_diff_test) does not play, because its oracle
 * stops at catchPokemon: the entry fee comes off and is refused without the money, the four actions wait on a screen of their own, running from all three Pokemon (or
 * throwing a ball at each) ends the encounter in the heal shop, every catch puts the Pokemon in the party (and a full party is asked what to release, and may let it go),
 * and a catch gives nobody experience or friendship (the VictoryPhase it queues finds no participant). */
#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static gc_run run;
static gc_system sys;

static void start(const char *seed, int wave, int n_mons, double money, int biome)
{
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    gc_system_init_defaults(&sys);
    run.sys = &sys;
    run.wave = wave;
    run.biome = biome;
    run.money = money;
    run.n_items = 0;
    gc_items_to_battle(&run);
    run.n_party = n_mons;
    for (int i = 0; i < n_mons; i++) {
        gc_starter s;
        memset(&s, 0, sizeof s);
        s.species = i == 0 ? SPECIES_ID_PIKACHU : i == 1 ? SPECIES_ID_LAPRAS : i == 2 ? SPECIES_ID_ONIX : i == 3 ? SPECIES_ID_EEVEE : i == 4 ? SPECIES_ID_ABRA : SPECIES_ID_GEODUDE;
        s.tera_type = -1;
        for (int k = 0; k < 6; k++) s.ivs[k] = 15;
        s.moves[0] = MOVE_ID_TACKLE;
        gc_mon_new_player(&run, &run.party[i], &s, 40 + 5 * i);
        run.party[i].pause_evolutions = true;
        run.party_order[i] = (int8_t)i;
    }
    gc_items_to_battle(&run);
    run.battle_type = BATTLE_TYPE_WILD;
    gc_rq_reset(&gc_int(&run)->rq);
    run.mystery_type = GC_ME_SAFARI_ZONE;
    gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_SAFARI_ZONE, GC_ME_GREAT, 0));
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_MYSTERY);
}

static bool choose_mystery(int option)
{
    gc_choice c = {GC_CH_MYSTERY, (int16_t)option, 0, 0};
    const char *why = "";
    return gc_run_choose(&run, &c, &why);
}

static bool on_actions(void)
{
    return run.screen.kind == GC_SCREEN_MYSTERY && run.screen.me_stage == GC_ME_STAGE_SAFARI && run.screen.n_options == 4;
}

/* the "battle:pokemonCaught" texts since the last call */
static int caught_texts(void)
{
    gc_event e;
    int n = 0;
    while (gc_run_next_event(&run, &e)) n += e.kind == GC_EV_MESSAGE && e.msg.key && !strcmp(e.msg.key, "battle:pokemonCaught");
    return n;
}

/* a PR_ARG_MONEY_EN_US value: binary64 in the low and high words */
static double arg_double(const pr_msg_arg *a)
{
    const uint64_t bits = (uint64_t)a->v | ((uint64_t)a->fusion_v << 32);
    double d;
    memcpy(&d, &bits, sizeof d);
    return d;
}

static void test_screen_and_fee(void)
{
    puts("test_safari_screen_and_fee...");
    start("safari-fee", 45, 3, 5000, BIOME_ID_FOREST);
    assert(run.screen.n_options == 2 && run.screen.option_ok[0] && run.screen.option_ok[1]);
    const double cost = pr_run_get_wave_money_amount(45, 2.0);
    assert(gc_safari_cost(&run) == cost);
    bool money_arg = false, num_arg = false;
    for (int a = 0; a < run.screen.n_me_args; a++) {
        money_arg |= run.screen.me_args[a].name && !strcmp(run.screen.me_args[a].name, "money") && arg_double(&run.screen.me_args[a]) == cost;
        num_arg |= run.screen.me_args[a].name && !strcmp(run.screen.me_args[a].name, "numEncounters") && run.screen.me_args[a].v == 3;
    }
    assert(money_arg && num_arg);

    assert(choose_mystery(0));
    assert(on_actions());
    assert(run.money == 5000 - cost);
    assert(run.n_enemy == 1 && run.enemy[0].b.hp == run.enemy[0].b.stats[0] && run.enemy[0].b.level > 0);
    assert(gc_int(&run)->safari.active && gc_int(&run)->safari.remaining == 2);
    assert(run.screen.slot == 0);   /* the cursor starts on the ball */
    for (int o = 0; o < 4; o++) assert(run.screen.option_ok[o]);
    { gc_choice c = {GC_CH_MYSTERY, 4, 0, 0}; const char *why = ""; assert(!gc_run_choose(&run, &c, &why)); assert(on_actions()); }   /* no fifth action */

    start("safari-fee", 45, 3, cost, BIOME_ID_FOREST);   /* exactly the fee is enough */
    assert(run.screen.option_ok[0]);
    assert(choose_mystery(0) && on_actions() && run.money == 0);

    start("safari-fee", 45, 3, cost - 1, BIOME_ID_FOREST);
    assert(!run.screen.option_ok[0] && run.screen.option_ok[1]);
    assert(!choose_mystery(0));   /* refused: the screen stays */
    assert(run.screen.kind == GC_SCREEN_MYSTERY && run.screen.me_stage == GC_ME_STAGE_FIRST && run.money == cost - 1);
    puts("ok");
}

static void test_leave_costs_nothing(void)
{
    puts("test_safari_leave_costs_nothing...");
    start("safari-leave", 45, 3, 5000, BIOME_ID_JUNGLE);
    assert(choose_mystery(1));
    assert(run.money == 5000 && run.screen.kind == GC_SCREEN_REWARD);   /* leaveEncounterWithoutBattle(true): the heal shop */
    assert(!gc_int(&run)->safari.active && run.n_party == 3);
    puts("ok");
}

/* the player's policy: `action` every time, or the ball first and then whatever `action` is */
static int play(int action, int *catches_out, int *prompts_out, bool release_when_full)
{
    int steps = 0, catches = 0, prompts = 0, texts = 0;
    assert(choose_mystery(0));
    assert(run.sys->dex[run.enemy[0].b.species].seen_count > 0);   /* setPokemonSeen */
    while (steps < 200) {
        if (run.screen.kind == GC_SCREEN_SWITCH) {
            /* the full party's prompt: release the first or let the Pokemon go */
            prompts++;
            gc_choice pick = {GC_CH_PICK, (int16_t)(release_when_full ? 0 : -1), 0, 0};
            const char *why = "";
            assert(gc_run_choose(&run, &pick, &why));
            continue;
        }
        if (!on_actions()) break;
        steps++;
        const int before = run.n_party;
        const double exp_before = run.party[0].exp;
        assert(choose_mystery(action));
        if (action == 0 && (run.n_party > before || run.screen.kind == GC_SCREEN_SWITCH)) catches++;
        assert(run.party[0].exp == exp_before);   /* a catch is no battle: nobody gains experience */
        texts += caught_texts();
        if (on_actions()) assert(run.sys->dex[run.enemy[0].b.species].seen_count > 0);
    }
    assert(texts == catches);   /* catchPokemon says "caught" once for each catch */
    *catches_out = catches;
    *prompts_out = prompts;
    return steps;
}

static void test_running_ends_the_encounter(void)
{
    puts("test_safari_running_ends_the_encounter...");
    start("safari-run", 45, 3, 5000, BIOME_ID_SWAMP);
    int catches, prompts;
    const int steps = play(3, &catches, &prompts, false);
    assert(steps == 3 && catches == 0 && prompts == 0);   /* three Pokemon, run from each */
    assert(run.screen.kind == GC_SCREEN_REWARD && run.n_party == 3 && !gc_int(&run)->safari.active);
    puts("ok");
}

static void test_catches_join_the_party(void)
{
    puts("test_safari_catches_join_the_party...");
    int total_catches = 0, ended = 0, fled = 0;
    for (int i = 0; i < 40; i++) {
        char seed[24];
        snprintf(seed, sizeof seed, "safari-catch-%d", i);
        start(seed, 30 + i, 3, 100000, BIOME_ID_FOREST);
        int catches, prompts;
        const int steps = play(0, &catches, &prompts, false);
        assert(steps >= 3 && prompts == 0);
        assert(run.screen.kind == GC_SCREEN_REWARD);   /* it always ends in the heal shop */
        assert(run.n_party == 3 + catches && catches <= 3);
        for (int k = 3; k < run.n_party; k++) {
            assert(run.party[k].b.is_player && run.party[k].b.hp > 0 && run.party[k].pokeball == POKEBALL_TYPE_POKEBALL && run.party[k].met_wave == 30 + i);
            assert(run.sys->dex[run.party[k].b.species].caught_attr != 0);
        }
        total_catches += catches;
        ended++;
        fled += catches < 3;
    }
    assert(total_catches > 20 && fled > 0 && ended == 40);   /* balls catch and balls miss: some Pokemon get away */
    puts("ok");
}

static void test_full_party(void)
{
    puts("test_safari_full_party...");
    int releases = 0, skips = 0;
    for (int i = 0; i < 30 && (!releases || !skips); i++) {
        char seed[24];
        snprintf(seed, sizeof seed, "safari-full-%d", i);
        const bool release = i % 2 == 0;
        start(seed, 40 + i, 6, 100000, BIOME_ID_JUNGLE);
        int catches, prompts;
        play(0, &catches, &prompts, release);
        assert(run.screen.kind == GC_SCREEN_REWARD && run.n_party == 6);
        assert(prompts == catches);   /* every catch asked what to release */
        if (catches && release) releases++;
        if (catches && !release) skips++;
        if (catches && !release) for (int k = 0; k < 6; k++) assert(run.party[k].b.species != 0);
    }
    assert(releases > 0 && skips > 0);
    puts("ok");
}

int main(void)
{
    test_screen_and_fee();
    test_leave_costs_nothing();
    test_running_ends_the_encounter();
    test_catches_join_the_party();
    test_full_party();
    puts("mystery_safari_test: all passed");
    return 0;
}
