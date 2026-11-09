/* A Trainer's Test on the host: what the upstream differential (mystery_trtest_diff_test) cannot compare, because its oracle never plays a battle or the shop: the
 * accepted fight played through the run's own phases (the stat trainer's six, the win's victory lines, the Epic Egg, the shop of a Relic Gold and two Rogue-tier items), the
 * outro that says who gave which egg, a loss (no egg), and the refusal (the heal, the Rare Egg, the heal-only shop). */
#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static gc_run run;
static gc_system sys;

static void start(const char *seed, int wave)
{
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    gc_system_init_defaults(&sys);
    run.sys = &sys;
    run.wave = wave;
    run.biome = BIOME_ID_PLAINS;
    run.money = 1000;
    gc_items_to_battle(&run);
    run.n_party = 3;
    for (int i = 0; i < 3; i++) {
        gc_starter s;
        memset(&s, 0, sizeof s);
        s.species = SPECIES_ID_SNORLAX;
        s.tera_type = -1;
        for (int k = 0; k < 6; k++) s.ivs[k] = 15;
        s.moves[0] = MOVE_ID_TACKLE;
        gc_mon_new_player(&run, &run.party[i], &s, 400);
        run.party[i].pause_evolutions = true;
        run.party_order[i] = (int8_t)i;
    }
    gc_items_to_battle(&run);
    run.battle_type = BATTLE_TYPE_WILD;
    gc_rq_reset(&gc_int(&run)->rq);
    run.mystery_type = GC_ME_A_TRAINERS_TEST;
    gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_A_TRAINERS_TEST, GC_ME_ULTRA, 0));
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_MYSTERY);
}

static int said_outro, said_defeated, outro_epic, outro_rare;
static char outro_trainer[48];
static void drain(void)
{
    gc_event e;
    while (gc_run_next_event(&run, &e)) {
        if (e.kind != GC_EV_MESSAGE || !e.msg.key) continue;
        if (!strcmp(e.msg.key, "mysteryEncounters/aTrainersTest:outro")) {
            said_outro++;
            for (int i = 0; i < e.msg.n_args; i++) {
                const pr_msg_arg *a = &e.msg.args[i];
                if (!a->name) continue;
                if (!strcmp(a->name, "statTrainerName") && a->s) snprintf(outro_trainer, sizeof outro_trainer, "%s", a->s);
                if (!strcmp(a->name, "eggType") && a->s) {
                    outro_epic += !strcmp(a->s, "mysteryEncounters/aTrainersTest:eggTypes.epic");
                    outro_rare += !strcmp(a->s, "mysteryEncounters/aTrainersTest:eggTypes.rare");
                }
            }
        }
        if (!strcmp(e.msg.key, "battle:trainerDefeated")) said_defeated++;
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

static void test_accept(void)
{
    puts("test_trtest_accept...");
    start("tt-accept", 154);
    assert(run.screen.n_options == 2 && run.screen.option_ok[0] && run.screen.option_ok[1]);
    const int pick = gc_trtest_pick(&run);
    static const int trainers[5] = {TRAINER_TYPE_BUCK, TRAINER_TYPE_CHERYL, TRAINER_TYPE_MARLEY, TRAINER_TYPE_MIRA, TRAINER_TYPE_RILEY};
    const int eggs_before = sys.n_eggs;
    const char *why;
    snprintf(gc_int(&run)->custom_trainer_name, sizeof gc_int(&run)->custom_trainer_name, "stale");   /* nothing of an earlier fight's naming carries over */
    gc_int(&run)->custom_team_on = true;
    gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    drain();
    assert(!gc_int(&run)->custom_trainer_name[0] && !gc_int(&run)->custom_team_on && run.trainer_name == -1);
    assert(run.screen.kind == GC_SCREEN_COMMAND && gc_int(&run)->trtest.active);
    assert(run.battle_type == BATTLE_TYPE_TRAINER && run.trainer_type == trainers[pick] && !run.double_battle && run.me_battle);
    assert(run.n_enemy == 6 && !run.screen.can_run && !run.screen.can_ball && !run.me_catch_allowed);
    assert(!gc_int(&run)->me_reward.active && !gc_int(&run)->me_no_stats);
    assert(sys.n_eggs == eggs_before);   /* the egg comes with the win */
    for (int e = 0; e < run.n_enemy; e++) {   /* one hit knocks them out and none can hurt the lead */
        run.enemy[e].b.hp = 1;
        run.enemy[e].b.stats[1] = run.enemy[e].b.stats[3] = 1;
    }
    gc_choice fight = {GC_CH_FIGHT, 0, -1, 0};
    for (int guard = 0; guard < 200 && run.screen.kind != GC_SCREEN_REWARD; guard++) {
        if (run.screen.kind == GC_SCREEN_LEARN_MOVE) { gc_choice forget = {GC_CH_FORGET, -1, 0, 0}; assert(gc_run_choose(&run, &forget, &why)); }
        else if (run.screen.kind == GC_SCREEN_COMMAND) assert(gc_run_choose(&run, &fight, &why));
        else break;
        drain();
        gc_run_until_screen(&run);
        drain();
    }
    assert(run.screen.kind == GC_SCREEN_REWARD);
    assert(said_defeated == 1);   /* TrainerVictoryPhase */
    assert(!gc_int(&run)->trtest.active);
    assert(sys.n_eggs == eggs_before + 1 && sys.eggs[eggs_before].tier == 2 && sys.eggs[eggs_before].source_type == 4);   /* an Epic Egg */
    assert(run.screen.n_options >= 3 && !strcmp(run.screen.options[0].key, "RELIC_GOLD"));   /* the guaranteed Relic Gold, then two Rogue-tier items */
    assert(run.screen.options[1].tier == MODIFIER_TIER_ROGUE && run.screen.options[2].tier == MODIFIER_TIER_ROGUE);
    const int wave = run.wave;
    leave_shop();
    assert(run.wave == wave + 1);
    assert(said_outro == 1 && outro_epic == 1 && outro_rare == 0);
    char want[48];
    snprintf(want, sizeof want, "trainerNames:%s", gc_trtest_key(pick));
    assert(!strcmp(outro_trainer, want));
    assert(sys.n_eggs == eggs_before + 1);   /* once */
    puts("ok");
}

static void test_loss(void)
{
    puts("test_trtest_loss...");
    said_outro = said_defeated = 0;
    start("tt-loss", 154);
    const char *why;
    const int eggs_before = sys.n_eggs;
    gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    drain();
    for (int i = 0; i < 3; i++) run.party[i].b.hp = 1;
    gc_choice fight = {GC_CH_FIGHT, 0, -1, 0};
    for (int guard = 0; guard < 400 && run.screen.kind != GC_SCREEN_GAME_OVER; guard++) {
        if (run.screen.kind == GC_SCREEN_COMMAND) assert(gc_run_choose(&run, &fight, &why));
        else if (run.screen.kind == GC_SCREEN_SWITCH) { gc_choice pick = {GC_CH_PICK, 0, 0, 0}; for (int i = 0; i < 6; i++) if (run.screen.pick_ok[i]) { pick.a = i; break; } assert(gc_run_choose(&run, &pick, &why)); }
        else break;
        drain();
    }
    assert(run.screen.kind == GC_SCREEN_GAME_OVER);
    assert(sys.n_eggs == eggs_before && said_outro == 0);
    puts("ok");
}

static void test_refusal(void)
{
    puts("test_trtest_refusal...");
    said_outro = outro_epic = outro_rare = 0;
    start("tt-refuse", 154);
    for (int i = 0; i < 3; i++) run.party[i].b.hp = 1;
    const int eggs_before = sys.n_eggs;
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 1, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    drain();
    gc_run_until_screen(&run);
    drain();
    assert(run.screen.kind == GC_SCREEN_REWARD && !gc_int(&run)->trtest.active && !run.in_battle);
    for (int i = 0; i < 3; i++) assert(run.party[i].b.hp == run.party[i].b.stats[0]);   /* PartyHealPhase */
    assert(sys.n_eggs == eggs_before + 1 && sys.eggs[eggs_before].tier == 1 && sys.eggs[eggs_before].source_type == 4);   /* a Rare Egg */
    assert(run.screen.n_options == 0);   /* setEncounterRewards({fillRemaining: false, rerollMultiplier: -1}): nothing to buy, like the other refusals' heal-only shops */
    leave_shop();
    assert(said_outro == 1 && outro_rare == 1 && outro_epic == 0);
    assert(sys.n_eggs == eggs_before + 1);
    gc_trtest_reward(&run);   /* the egg is given once, whoever asks again */
    assert(sys.n_eggs == eggs_before + 1);
    puts("ok");
}

int main(void)
{
    test_accept();
    test_loss();
    test_refusal();
    puts("all trtest host tests passed");
    return 0;
}
