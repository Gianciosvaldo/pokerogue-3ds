/* Clowning Around on the host: what the upstream differential (mystery_clown_diff_test) cannot compare, because its oracle never plays a battle, the shop that
 * follows or a save: the item shuffle and the type shuffle played through the run's own phases (the texts in order, the empty heal-only shop, the next wave), the
 * new types surviving a save and a load, and the fight played through the run's own phases (the double battle of two bosses, the win's victory lines, the shop that
 * fills itself, the ability offer after it with its Yes, its No and its refused answers, the outro, the next wave). */
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
    static const int species[3] = {SPECIES_ID_SNORLAX, SPECIES_ID_PIKACHU, SPECIES_ID_GENGAR};
    for (int i = 0; i < 3; i++) {
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
    run.mystery_type = GC_ME_CLOWNING_AROUND;
    gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_CLOWNING_AROUND, GC_ME_ULTRA, 0));
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_MYSTERY);
}

static int count_key(const char *key)
{
    int n = 0;
    for (int i = 0; i < run.n_items; i++) if (!strcmp(run.items[i].key, key)) n += run.items[i].stack;
    return n;
}

static int said2, said3, order_ok;
static uint32_t said_species;
static void drain(const char *want2, const char *want3)
{
    gc_event e;
    while (gc_run_next_event(&run, &e)) {
        if (e.kind != GC_EV_MESSAGE || !e.msg.key) continue;
        if (!strcmp(e.msg.key, want2)) {
            said2++;
            order_ok &= !said3;   /* selected2 before selected3 */
            for (int i = 0; i < e.msg.n_args; i++)
                if (e.msg.args[i].name && !strcmp(e.msg.args[i].name, "switchPokemon")) said_species = e.msg.args[i].v >> 16;
        }
        if (!strcmp(e.msg.key, want3)) said3++;
    }
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

static void test_items(void)
{
    puts("test_clown_items...");
    said2 = said3 = 0; order_ok = 1;
    start("cl-items", 112);
    /* the second Pokemon holds the most: 3 Leftovers, 2 Berries (Rogue, Berries), a Lucky Egg and a Soothe Bell (Ultra) and a Mint-free vitamin that stays */
    const uint32_t id = run.party[1].b.id;
    gc_item_add(&run, "LEFTOVERS", -1, id, 3);
    gc_item_add(&run, "BERRY", BERRY_TYPE_LUM, id, 2);
    gc_item_add(&run, "LUCKY_EGG", -1, id, 1);
    gc_item_add(&run, "SOOTHE_BELL", -1, id, 1);
    gc_item_add(&run, "GOLDEN_EGG", -1, id, 1);
    gc_item_add(&run, "BASE_STAT_BOOSTER", STAT_ATK, id, 2);
    gc_item_add(&run, "WHITE_HERB", -1, run.party[0].b.id, 1);
    gc_items_to_battle(&run);
    for (int i = 0; i < 3; i++) run.party[i].b.hp = 1;
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 1, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    drain("mysteryEncounters/clowningAround:option.2.selected2", "mysteryEncounters/clowningAround:option.2.selected3");
    gc_run_until_screen(&run);
    drain("mysteryEncounters/clowningAround:option.2.selected2", "mysteryEncounters/clowningAround:option.2.selected3");
    assert(said2 == 1 && said3 == 1 && order_ok);
    assert(said_species == SPECIES_ID_PIKACHU);   /* the Pokemon with the most transferable non-berry stacks */
    /* nothing but the Berries, Ultra and Rogue items moved: the vitamin stays, and so does the other holder's White Herb? (a tier-less item is Ultra: it is taken and given back as a pool item) */
    assert(count_key("BASE_STAT_BOOSTER") == 2);
    assert(count_key("LUCKY_EGG") == 0 && count_key("SOOTHE_BELL") == 0 && count_key("GOLDEN_EGG") == 0);
    assert(count_key("BERRY") == 2);   /* as many Berries as it had */
    for (int i = 0; i < run.n_items; i++) if (run.items[i].pokemon_id != id) assert(!strcmp(run.items[i].key, "WHITE_HERB"));   /* the others are not touched */
    assert(run.screen.kind == GC_SCREEN_REWARD && run.screen.n_options == 0);   /* leaveEncounterWithoutBattle(true): the heal-only shop */
    for (int i = 0; i < 3; i++) assert(run.party[i].b.hp == 1);   /* a heal-only shop, not a heal: the party stays hurt */
    const int wave = run.wave;
    leave_shop();
    assert(run.wave == wave + 1);
    puts("ok");
}


static int held(const char *key, int arg)
{
    int n = 0;
    for (int i = 0; i < run.n_items; i++) if (!strcmp(run.items[i].key, key) && (arg < -1 || run.items[i].arg == arg) && run.items[i].pokemon_id == run.party[0].b.id) n += run.items[i].stack;
    return n;
}

/* far more Berries and Rogue items than the pools hold: every pool entry ends exactly at its limit, and the draws past the end of the pool give nothing */
static void test_pools(void)
{
    puts("test_clown_pools...");
    start("cl-pools", 130);
    const uint32_t id = run.party[0].b.id;
    gc_item_add(&run, "BERRY", BERRY_TYPE_LUM, id, 20);
    gc_item_add(&run, "BERRY", BERRY_TYPE_SITRUS, id, 12);
    gc_item_add(&run, "LEFTOVERS", -1, id, 40);
    gc_item_add(&run, "GOLDEN_PUNCH", -1, id, 3);
    gc_items_to_battle(&run);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 1, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    gc_run_until_screen(&run);
    static const struct { int type, n; } berries[11] = {
        {BERRY_TYPE_APICOT, 3}, {BERRY_TYPE_ENIGMA, 2}, {BERRY_TYPE_GANLON, 3}, {BERRY_TYPE_LANSAT, 3}, {BERRY_TYPE_LEPPA, 2}, {BERRY_TYPE_LIECHI, 3},
        {BERRY_TYPE_LUM, 2}, {BERRY_TYPE_PETAYA, 3}, {BERRY_TYPE_SALAC, 2}, {BERRY_TYPE_SITRUS, 2}, {BERRY_TYPE_STARF, 3},
    };
    for (int i = 0; i < 11; i++) assert(held("BERRY", berries[i].type) == berries[i].n);   /* 28 Berries of 32 were made: the pool ran dry */
    static const struct { const char *key; int n; } rogue[8] = {
        {"LEFTOVERS", 4}, {"SHELL_BELL", 4}, {"SOUL_DEW", 10}, {"SCOPE_LENS", 1}, {"BATON", 1}, {"FOCUS_BAND", 5}, {"KINGS_ROCK", 3}, {"GRIP_CLAW", 5},
    };
    for (int i = 0; i < 8; i++) assert(held(rogue[i].key, -1) == rogue[i].n);
    int ultra = 0;
    static const char *u[5] = {"REVIVER_SEED", "GOLDEN_PUNCH", "ATTACK_TYPE_BOOSTER", "QUICK_CLAW", "WIDE_LENS"};
    for (int k = 0; k < 5; k++) ultra += held(u[k], -2);
    assert(ultra == 3);   /* three Ultra stacks in, three out */
    puts("ok");
}

/* the Attack Type Booster generator finds no attacking move in a party of status moves: upstream's apply throws, nothing is added, the draw still counts */
static void test_no_attack_move(void)
{
    puts("test_clown_no_attack...");
    start("cl-noatk", 130);
    for (int i = 0; i < 3; i++) { run.party[i].b.moves[0] = MOVE_ID_GROWL; run.party[i].b.moves[1] = 0; run.party[i].b.n_moves = 1; }
    const uint32_t id = run.party[0].b.id;
    gc_item_add(&run, "GOLDEN_PUNCH", -1, id, 40);
    gc_items_to_battle(&run);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 1, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    gc_run_until_screen(&run);
    for (int i = 0; i < run.n_items; i++) assert(strcmp(run.items[i].key, "ATTACK_TYPE_BOOSTER"));
    assert(held("REVIVER_SEED", -1) + held("GOLDEN_PUNCH", -1) + held("QUICK_CLAW", -1) + held("WIDE_LENS", -1) < 40);
    puts("ok");
}

/* removeModifier does not run updateModifiers: a shuffle that adds nothing prunes nothing (an evolution tracker on a Pokemon that cannot use it stays) */
static void test_nothing_added(void)
{
    puts("test_clown_nothing_added...");
    start("cl-none", 130);
    gc_item_add(&run, "EVOLUTION_TRACKER_GIMMIGHOUL", -1, run.party[0].b.id, 3);
    gc_items_to_battle(&run);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 1, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    gc_run_until_screen(&run);
    assert(held("EVOLUTION_TRACKER_GIMMIGHOUL", -1) == 3);
    puts("ok");
}

static void test_types(void)
{
    puts("test_clown_types...");
    said2 = said3 = 0; order_ok = 1; said_species = 0;
    start("cl-types", 150);
    int8_t before[3][PR_MAX_TYPES];
    int n_before[3];
    for (int i = 0; i < 3; i++) n_before[i] = pr_get_types(&run.party[i].b, before[i], PR_TYPES_NO_TERA | PR_TYPES_BYPASS_SUMMON | PR_TYPES_IGNORE_THIRD);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 2, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    gc_run_until_screen(&run);
    drain("mysteryEncounters/clowningAround:option.3.selected2", "mysteryEncounters/clowningAround:option.3.selected3");
    assert(said2 == 1 && said3 == 1 && order_ok && said_species == 0);
    for (int i = 0; i < 3; i++) {
        int8_t now[PR_MAX_TYPES];
        const int n = pr_get_types(&run.party[i].b, now, PR_TYPES_NO_TERA | PR_TYPES_BYPASS_SUMMON | PR_TYPES_IGNORE_THIRD);
        assert(n == 2 && run.party[i].b.custom_types[0] == 0 && run.party[i].b.custom_types[1] != 0);   /* the first slot keeps the Pokemon's own type, the second is new */
        assert(now[0] == before[i][0]);
        for (int t = 0; t < n_before[i]; t++) assert(now[1] != before[i][t]);
    }
    /* the new second type survives a save and a load (saving is allowed from the next wave's command screen) */
    leave_shop();
    assert(run.screen.kind == GC_SCREEN_COMMAND && gc_run_can_save(&run));
    static uint8_t buf[1 << 20];
    const size_t len = gc_run_save(&run, buf, sizeof buf);
    assert(len > 0);
    static gc_run loaded;
    memset(&loaded, 0, sizeof loaded);
    assert(gc_run_load(&loaded, buf, len, &why));
    for (int i = 0; i < 3; i++) assert(loaded.party[i].b.custom_types[1] == run.party[i].b.custom_types[1] && loaded.party[i].b.custom_types[1] != 0);
    puts("ok");
}

/* the messages of the fight and its ending, in the order they came */
static char log_keys[40][64];
static int n_log, said_species_chosen;
static void log_events(void)
{
    gc_event e;
    while (gc_run_next_event(&run, &e)) {
        if (e.kind != GC_EV_MESSAGE || !e.msg.key) continue;
        if (n_log < 40) snprintf(log_keys[n_log++], sizeof log_keys[0], "%s", e.msg.key);
        for (int i = 0; i < e.msg.n_args; i++)
            if (e.msg.args[i].name && !strcmp(e.msg.args[i].name, "chosenPokemon")) said_species_chosen = (int)(e.msg.args[i].v >> 16);
    }
}
static int logged(const char *key)
{
    int n = 0;
    for (int i = 0; i < n_log; i++) n += !strcmp(log_keys[i], key);
    return n;
}
static int logged_at(const char *key)
{
    for (int i = 0; i < n_log; i++) if (!strcmp(log_keys[i], key)) return i;
    return -1;
}

/* a won double battle: both clowns at 1 HP and unable to hurt anyone, Tackle at whoever still stands */
static void win_the_fight(void)
{
    const char *why;
    for (int e = 0; e < run.n_enemy; e++) {
        run.enemy[e].b.hp = 1;
        run.enemy[e].b.stats[1] = run.enemy[e].b.stats[3] = 1;
    }
    for (int guard = 0; guard < 60 && run.screen.kind != GC_SCREEN_REWARD; guard++) {
        if (run.screen.kind == GC_SCREEN_LEARN_MOVE) { gc_choice forget = {GC_CH_FORGET, -1, 0, 0}; assert(gc_run_choose(&run, &forget, &why)); }
        else if (run.screen.kind == GC_SCREEN_COMMAND || run.screen.kind == GC_SCREEN_TARGET) {
            gc_choice fight = {run.screen.kind == GC_SCREEN_TARGET ? GC_CH_TARGET : GC_CH_FIGHT, 0, -1, 0};
            const int target = run.enemy[0].b.hp > 0 ? 2 : 3;
            if (fight.kind == GC_CH_TARGET) fight.a = (int16_t)target; else fight.b = (int16_t)target;
            assert(gc_run_choose(&run, &fight, &why));
        } else break;
        log_events();
        gc_run_until_screen(&run);
        log_events();
    }
    assert(run.screen.kind == GC_SCREEN_REWARD);
}

static void begin_fight(const char *seed, int wave)
{
    n_log = 0;
    start(seed, wave);
    assert(run.screen.n_options == 3 && run.screen.option_ok[0]);
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};
    snprintf(gc_int(&run)->custom_trainer_name, sizeof gc_int(&run)->custom_trainer_name, "stale");   /* nothing of an earlier fight's naming carries over */
    gc_int(&run)->custom_team_on = true;
    assert(gc_run_choose(&run, &ch, &why));
    log_events();
}

static void test_fight(void)
{
    puts("test_clown_fight...");
    begin_fight("cl-fight", 120);
    int ability, first, second;
    gc_clown_roll(&run, &ability, &first, &second);
    assert(!gc_int(&run)->custom_trainer_name[0] && !gc_int(&run)->custom_team_on);
    assert(run.screen.kind == GC_SCREEN_COMMAND && gc_int(&run)->clown.active);
    assert(run.battle_type == BATTLE_TYPE_TRAINER && run.trainer_type == TRAINER_TYPE_HARLEQUIN && run.double_battle && run.me_battle);
    assert(run.trainer_variant == TRAINER_VARIANT_DOUBLE && run.trainer_name >= 0 && run.trainer_partner_name == -1);
    assert(run.n_enemy == 2 && !run.screen.can_run && !run.screen.can_ball && !run.me_catch_allowed);
    assert(!gc_int(&run)->me_reward.active && !gc_int(&run)->me_no_stats && run.mystery_type == -1);
    assert(logged("battle:trainerAppearedDouble") == 1 && logged("battle:trainerAppeared") == 0);
    const gc_mon *mime = &run.enemy[0], *blace = &run.enemy[1];
    assert(mime->b.species == SPECIES_ID_MR_MIME && blace->b.species == SPECIES_ID_BLACEPHALON);
    assert(mime->b.is_boss && blace->b.is_boss && mime->b.boss_segments >= 2 && blace->b.boss_segments >= 2 && mime->trainer_slot == TRAINER_SLOT_TRAINER);
    assert(mime->b.moves[0] == MOVE_ID_TEETER_DANCE && mime->b.moves[3] == MOVE_ID_PSYCHIC && blace->b.moves[0] == MOVE_ID_TRICK && blace->b.moves[3] == MOVE_ID_MIND_BLOWN);
    assert(blace->b.custom_types[0] == first + 1 && blace->b.custom_types[1] == second + 1 && pr_get_ability(&blace->b, false) == ability);
    assert(mime->b.level < blace->b.level);   /* STRONG, then STRONGER */
    /* the three start-of-battle effects ran before the first command: Mr. Mime has the ability Role Play copied from Blacephalon */
    assert(gc_int(&run)->n_me_start == 0);
    assert(pr_get_ability(&mime->b, false) == ability);
    puts("ok");
}

static void test_win_and_swap(int answer)   /* 0 = Yes (member 1), 1 = No */
{
    puts(answer ? "test_clown_win_no..." : "test_clown_win_yes...");
    begin_fight(answer ? "cl-win-no" : "cl-win-yes", 120);
    int ability, first, second;
    gc_clown_roll(&run, &ability, &first, &second);
    const int wave = run.wave;
    win_the_fight();
    assert(logged("battle:trainerDefeated") == 1);   /* TrainerVictoryPhase */
    assert(!gc_int(&run)->clown.active && run.screen.n_options >= 3);   /* the shop that fills itself */
    const char *why;
    gc_choice skip = {GC_CH_SKIP, 0, 0, 0};
    assert(gc_run_choose(&run, &skip, &why));
    log_events();
    gc_run_until_screen(&run);
    log_events();
    /* after the shop: the offer, still on the same wave, waiting */
    assert(run.screen.kind == GC_SCREEN_MYSTERY && run.screen.me_stage == GC_ME_STAGE_ABILITY && run.wave == wave);
    assert(run.screen.n_options == 2 && run.screen.option_pick[0] == GC_PICK_MEMBER && run.screen.option_pick[1] == GC_PICK_NONE);
    assert(logged("mysteryEncounters/clowningAround:option.1.applyAbilityDialogue") == 1 && logged("mysteryEncounters/clowningAround:option.1.abilityPrompt") == 1);
    assert(logged_at("mysteryEncounters/clowningAround:option.1.applyAbilityDialogue") < logged_at("mysteryEncounters/clowningAround:option.1.applyAbilityMessage"));
    assert(logged_at("mysteryEncounters/clowningAround:option.1.applyAbilityMessage") < logged_at("mysteryEncounters/clowningAround:option.1.abilityPrompt"));
    assert(logged("mysteryEncounters/clowningAround:outro") == 0 && logged("mysteryEncounters/clowningAround:option.1.abilityGained") == 0);
    /* answers that are not answers leave the offer as it was */
    gc_choice bad_kind = {GC_CH_SKIP, 0, 0, 0}, bad_a = {GC_CH_MYSTERY, 2, 0, 0}, bad_member = {GC_CH_MYSTERY, 0, 3, 0}, bad_negative = {GC_CH_MYSTERY, 0, -1, 0};
    assert(!gc_run_choose(&run, &bad_kind, &why) && !gc_run_choose(&run, &bad_a, &why) && !gc_run_choose(&run, &bad_member, &why) && !gc_run_choose(&run, &bad_negative, &why));
    assert(run.screen.kind == GC_SCREEN_MYSTERY && run.screen.me_stage == GC_ME_STAGE_ABILITY);
    int before[3];
    for (int i = 0; i < 3; i++) before[i] = run.party[i].b.custom_ability;
    gc_choice reply = {GC_CH_MYSTERY, (int16_t)(answer ? 1 : 0), 1, 0};
    assert(gc_run_choose(&run, &reply, &why));
    log_events();
    gc_run_until_screen(&run);
    log_events();
    assert(logged("mysteryEncounters/clowningAround:outro") == 1);
    assert(run.wave == wave + 1);   /* the next wave */
    for (int i = 0; i < 3; i++) assert(i == 1 && !answer ? run.party[i].b.custom_ability == ability : run.party[i].b.custom_ability == before[i]);
    if (!answer) {
        assert(logged("mysteryEncounters/clowningAround:option.1.abilityGained") == 1 && said_species_chosen == run.party[1].b.species);
        assert(logged_at("mysteryEncounters/clowningAround:option.1.abilityGained") < logged_at("mysteryEncounters/clowningAround:outro"));
        assert(pr_get_ability(&run.party[1].b, false) == ability);
    } else {
        assert(logged("mysteryEncounters/clowningAround:option.1.abilityGained") == 0);
    }
    puts("ok");
}

static void test_fused_gets_the_fusion_ability(void)
{
    puts("test_clown_fused_swap...");
    begin_fight("cl-fused", 120);
    int ability, first, second;
    gc_clown_roll(&run, &ability, &first, &second);
    run.party[1].b.fusion_species = SPECIES_ID_PIKACHU;   /* a fusion: the ability goes on the fusion half's data */
    run.party[1].b.fusion_custom_ability = -1;
    const int own = run.party[1].b.custom_ability;
    win_the_fight();
    const char *why;
    gc_choice skip = {GC_CH_SKIP, 0, 0, 0};
    assert(gc_run_choose(&run, &skip, &why));
    gc_run_until_screen(&run);
    assert(run.screen.me_stage == GC_ME_STAGE_ABILITY);
    gc_choice reply = {GC_CH_MYSTERY, 0, 1, 0};
    assert(gc_run_choose(&run, &reply, &why));
    assert(run.party[1].b.fusion_custom_ability == ability && run.party[1].b.custom_ability == own);
    puts("ok");
}

static void test_shuffles_say_the_outro(void)
{
    puts("test_clown_shuffle_outro...");
    for (int option = 1; option <= 2; option++) {
        n_log = 0;
        start(option == 1 ? "cl-o1" : "cl-o2", 120);
        const char *why;
        gc_choice ch = {GC_CH_MYSTERY, (int16_t)option, 0, 0};
        assert(gc_run_choose(&run, &ch, &why));
        log_events();
        assert(run.screen.kind == GC_SCREEN_REWARD);
        gc_choice skip = {GC_CH_SKIP, 0, 0, 0};
        const int wave = run.wave;
        assert(gc_run_choose(&run, &skip, &why));
        log_events();
        gc_run_until_screen(&run);
        log_events();
        assert(logged("mysteryEncounters/clowningAround:outro") == 1 && run.wave == wave + 1 && run.screen.kind != GC_SCREEN_MYSTERY);
        assert(logged_at("mysteryEncounters/clowningAround:outro") > (option == 1 ? logged_at("mysteryEncounters/clowningAround:option.2.selected3") : logged_at("mysteryEncounters/clowningAround:option.3.selected3")));
    }
    puts("ok");
}

int main(void)
{
    test_items();
    test_pools();
    test_no_attack_move();
    test_nothing_added();
    test_types();
    test_fight();
    test_win_and_swap(0);
    test_win_and_swap(1);
    test_fused_gets_the_fusion_ability();
    test_shuffles_say_the_outro();
    puts("all clown host tests passed");
    return 0;
}
