/* The Winstrate Challenge on the host: what the upstream differential (mystery_winstrate_diff_test) cannot compare, because its oracle never plays a battle or
 * the shop: the five fights played through the run's own phases (the trainers in order, their teams, no heal between them, the prize money of each win,
 * the weather that does not carry over), the end (an Egg Voucher Premium, a shop of one Macho Brace), a loss in the middle, and the refusal. */
#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static gc_run run;
static gc_system sys;

static const int TANKS[3] = {SPECIES_ID_SNORLAX, SPECIES_ID_SNORLAX, SPECIES_ID_SNORLAX};

static void start(const char *seed, int wave, const int *species, int n, int level)
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
    for (int i = 0; i < n; i++) {
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
    gc_items_to_battle(&run);
    run.battle_type = BATTLE_TYPE_WILD;
    gc_rq_reset(&gc_int(&run)->rq);
    run.mystery_type = GC_ME_THE_WINSTRATE_CHALLENGE;
    gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_THE_WINSTRATE_CHALLENGE, GC_ME_ROGUE, 0));
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_MYSTERY);
}

static int said_victory, said_victory2, said_reward, said_dialogue[5], said_defeated;
static void drain(void)
{
    static const char *const names[5] = {"Victor", "Victoria", "Vivi", "Vicky", "Vito"};
    gc_event e;
    while (gc_run_next_event(&run, &e)) {
        if (e.kind == GC_EV_DIALOGUE && e.msg.key) {
            for (int k = 0; k < 5; k++) {
                char want[64];
                snprintf(want, sizeof want, "dialogue:winstrates%s.encounter.1", names[k]);
                if (!strcmp(e.msg.key, want)) said_dialogue[k]++;
            }
        }
        if (e.kind != GC_EV_MESSAGE || !e.msg.key) continue;
        if (!strcmp(e.msg.key, "mysteryEncounters/theWinstrateChallenge:victory")) said_victory++;
        if (!strcmp(e.msg.key, "mysteryEncounters/theWinstrateChallenge:victory2")) said_victory2++;
        if (!strcmp(e.msg.key, "battle:rewardGain")) said_reward++;
        if (!strcmp(e.msg.key, "battle:trainerDefeated")) said_defeated++;
    }
}

static void reset_said(void)
{
    said_victory = said_victory2 = said_reward = said_defeated = 0;
    memset(said_dialogue, 0, sizeof said_dialogue);
}

static void answer_other_screens(void)
{
    const char *why;
    if (run.screen.kind == GC_SCREEN_LEARN_MOVE) {
        gc_choice forget = {GC_CH_FORGET, -1, 0, 0};
        assert(gc_run_choose(&run, &forget, &why));
        gc_run_until_screen(&run);
    }
}

/* the fight on the command screen is won: the foes are weakened so that one hit knocks them out and none can hurt the lead */
static uint32_t weakened_id;
static void weaken(void)
{
    for (int e = 0; e < run.n_enemy; e++) {
        run.enemy[e].b.hp = 1;
        run.enemy[e].b.stats[1] = run.enemy[e].b.stats[3] = 1;
    }
    weakened_id = run.enemy[0].b.id;
}

/* plays the fight on screen to its end; returns when the screen is no longer a command screen or the next trainer's team is on it */
static void play_one_fight(bool weak_foes)
{
    const char *why;
    gc_choice fight = {GC_CH_FIGHT, 0, -1, 0};
    if (weak_foes) weaken();
    const uint32_t first = run.enemy[0].b.id;
    for (int guard = 0; guard < 120 && run.screen.kind == GC_SCREEN_COMMAND && run.enemy[0].b.id == first; guard++) {
        assert(gc_run_choose(&run, &fight, &why));
        drain();
        answer_other_screens();
    }
}

static void leave_shop(void)
{
    const char *why;
    gc_choice skip = {GC_CH_SKIP, 0, 0, 0};
    for (int guard = 0; guard < 6 && run.screen.kind == GC_SCREEN_REWARD; guard++) {
        assert(gc_run_choose(&run, &skip, &why));
        gc_run_until_screen(&run);
        drain();
    }
}

static void test_the_chain(void)
{
    puts("test_winstrate_chain...");
    static const int trainers[5] = {TRAINER_TYPE_VICTOR, TRAINER_TYPE_VICTORIA, TRAINER_TYPE_VIVI, TRAINER_TYPE_VICKY, TRAINER_TYPE_VITO};
    static const int sizes[5] = {2, 2, 3, 1, 5};
    static const int firsts[5] = {SPECIES_ID_SWELLOW, SPECIES_ID_ROSERADE, SPECIES_ID_SEAKING, SPECIES_ID_MEDICHAM, SPECIES_ID_HISUI_ELECTRODE};
    reset_said();
    start("ws-chain", 154, TANKS, 3, 400);
    assert(run.screen.n_options == 2 && run.screen.option_ok[0] && run.screen.option_ok[1]);
    const double money_before = run.money;
    const uint32_t vouchers_before = sys.voucher_counts[GC_VOUCHER_PREMIUM];
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    drain();
    double money_prev = money_before;
    for (int k = 0; k < 5; k++) {
        assert(run.screen.kind == GC_SCREEN_COMMAND);
        assert(gc_int(&run)->winstrate.active && gc_int(&run)->winstrate.next == k + 1);
        assert(run.battle_type == BATTLE_TYPE_TRAINER && run.trainer_type == trainers[k] && !run.double_battle && run.me_battle);
        assert(run.n_enemy == sizes[k] && run.enemy[0].b.species == firsts[k] && !run.screen.can_run && !run.screen.can_ball);
        assert(said_dialogue[k] == 1);   /* the trainer's line, once, before the fight */
        assert(!run.me_catch_allowed && !gc_int(&run)->me_reward.active && !gc_int(&run)->me_no_stats);
        assert(run.trainer_names[GC_TNAME_TITLED] && strstr(run.trainer_names[GC_TNAME_TITLED], "titles:theWinstrates"));   /* "The Winstrates Victor" */
        assert(sys.dex[run.enemy[0].b.species].seen_count > 0);   /* the foes count as seen */
        for (int e = 0; e < run.n_enemy; e++) {   /* every Pokemon holds its items */
            int held = 0;
            for (int i = 0; i < run.n_enemy_items; i++) held += run.enemy_items[i].pokemon_id == run.enemy[e].b.id;
            assert(held > 0);
        }
        if (k == 0) {   /* a weather that lasts a few turns does not carry over to the next trainer */
            run.bt.weather = WEATHER_TYPE_RAIN;
            run.bt.weather_turns_left = 5;
            run.bt.weather_max_duration = 5;
        }
        if (k == 1) {   /* the biome's own (permanent: turns left 0) stays */
            assert(run.bt.weather == WEATHER_TYPE_NONE);
            run.bt.weather = WEATHER_TYPE_SUNNY;
            run.bt.weather_turns_left = 0;
        }
        if (k == 2) assert(run.bt.weather == WEATHER_TYPE_SUNNY);
        if (k == 3) { run.bt.terrain = TERRAIN_GRASSY; run.bt.terrain_turns_left = 4; run.bt.terrain_max_duration = 5; }
        if (k == 4) assert(run.bt.terrain == TERRAIN_NONE);
        const uint32_t foe_id = run.enemy[0].b.id;
        play_one_fight(true);
        if (k < 4) {
            assert(run.screen.kind == GC_SCREEN_COMMAND && run.enemy[0].b.id != foe_id);   /* the next trainer's team is out */
            assert(run.money > money_prev);   /* the prize money of each win (MoneyRewardPhase) */
            money_prev = run.money;
            if (k == 0) run.party[2].b.hp = 7;   /* nothing heals the party between two trainers */
            if (k == 1) assert(run.party[2].b.hp == 7);
        }
        assert(said_defeated == k + 1);
    }
    /* after the fifth: the voucher, both lines, and the shop of one Macho Brace */
    assert(run.screen.kind == GC_SCREEN_REWARD);
    assert(said_victory == 1 && said_victory2 == 1 && said_reward == 1);
    assert(!gc_int(&run)->winstrate.active);
    assert(sys.voucher_counts[GC_VOUCHER_PREMIUM] == vouchers_before + 1);
    assert(run.screen.n_options == 1 && !strcmp(run.screen.options[0].key, "MYSTERY_ENCOUNTER_MACHO_BRACE") && run.screen.options[0].tier == MODIFIER_TIER_MASTER);
    assert(run.money > money_prev - 1);
    const int wave = run.wave;
    leave_shop();
    assert(run.wave == wave + 1 && !gc_int(&run)->winstrate.active);
    puts("ok");
}

static void test_a_loss_in_the_middle(void)
{
    puts("test_winstrate_loss...");
    reset_said();
    start("ws-loss", 154, TANKS, 3, 400);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    drain();
    assert(run.screen.kind == GC_SCREEN_COMMAND && run.trainer_type == TRAINER_TYPE_VICTOR);
    /* the first fight is won, the second lost: the run is over, no voucher */
    play_one_fight(true);
    assert(run.screen.kind == GC_SCREEN_COMMAND && run.trainer_type == TRAINER_TYPE_VICTORIA);
    for (int i = 0; i < 3; i++) run.party[i].b.hp = 1;   /* nobody is left standing for long: the foes are not weakened */
    gc_choice fight = {GC_CH_FIGHT, 0, -1, 0};
    for (int guard = 0; guard < 400 && run.screen.kind != GC_SCREEN_GAME_OVER; guard++) {
        if (run.screen.kind == GC_SCREEN_COMMAND) assert(gc_run_choose(&run, &fight, &why));
        else if (run.screen.kind == GC_SCREEN_SWITCH) { gc_choice pick = {GC_CH_PICK, 0, 0, 0}; for (int i = 0; i < 6; i++) if (run.screen.pick_ok[i]) { pick.a = i; break; } assert(gc_run_choose(&run, &pick, &why)); }
        else break;
        drain();
    }
    assert(run.screen.kind == GC_SCREEN_GAME_OVER);
    assert(said_victory == 0 && said_reward == 0 && sys.voucher_counts[GC_VOUCHER_PREMIUM] == 0);
    puts("ok");
}

static void test_the_refusal(void)
{
    puts("test_winstrate_refusal...");
    start("ws-refuse", 154, TANKS, 3, 400);
    for (int i = 0; i < 3; i++) run.party[i].b.hp = 1;
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 1, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    drain();
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_REWARD && !gc_int(&run)->winstrate.active && !run.in_battle);
    assert(run.screen.n_options == 1 && !strcmp(run.screen.options[0].key, "RARER_CANDY"));
    for (int i = 0; i < 3; i++) assert(run.party[i].b.hp == run.party[i].b.stats[0]);   /* PartyHealPhase */
    const int wave = run.wave;
    leave_shop();
    assert(run.wave == wave + 1);
    puts("ok");
}

int main(void)
{
    test_the_chain();
    test_a_loss_in_the_middle();
    test_the_refusal();
    puts("all winstrate host tests passed");
    return 0;
}
