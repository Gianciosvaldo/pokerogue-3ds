/* Gym leaders, the Elite Four and the Champion in the run (src/gc_encounter.c, src/gc_battle.c), played wave by wave with an unbeatable party:
 *  - the boss trainer waves (wave % 30 == 20, or 0 with offsetGym) are a trainer battle against a gym leader with a team of 2 to 6, not the wild
 *    stand-in game-core used while run-core's Gym/E4 closures were off;
 *  - waves 182, 184, 186 and 188 are Elite Four members and 190 a Champion, each with six Pokemon, as run-core's fixed-battle table says;
 *  - none of those waves used a stand-in (run-core's fixed-battle and trainer-config flags, GC_STUB_UNPORTED_ENEMY), and winning one gives what the
 *    trainer's config says: the "defeated" text and the prize money (the config's multiplier), no GC_STUB_TRAINER_REWARDS;
 *  - the enemy AI reads the trainer's own isBoss (gym leaders, the Elite Four and the Champion are bosses, an ordinary trainer is not);
 *  - a battle saved at its start and loaded back is the same gym leader with the same team;
 *  - the Town Youngster (wave 5) and the evil team grunt fights (35, 62, 64, 112) are trainer battles too: the same team at all four waves of a run,
 *    a team of 2, 3, 3 and 6, a pair of grunts when the (unseeded upstream) double roll says so, no stand-in, the grunts' rewards and no boss AI;
 *  - so are the admins (66, 114, 164) and the two bosses (115, 165): a full team, no stand-in apart from the held items of a trainer with a genModifiersFunc,
 *    the boss AI for the bosses, the second boss the same team's second form of the first (the fights share one seed offset), the bosses' reward screen.
 * Each fixture fails on game-core as it was (the wave would be a wild battle). */
#include "../src/internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run, back;
static gc_system sys;
static unsigned char save[GC_SAVE_MAX];
static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "gym/e4 FAIL line %d (wave %d, trainer %d): %s\n", __LINE__, run.wave, run.trainer_type, #x); exit(1); } } while (0)

static int pin_roll[2] = {-1, -1};   /* the unseeded evil-team rolls (gender, double) a fight pins, -1 = drawn */
static int n_gym, n_e4, n_champion, n_ordinary, n_defeated, n_saved, n_ai_boss, n_ai_plain, n_turns, n_talk, n_talk_moved_on, n_defeated_moved_on;

/* a run that has just won wave - 1, with a party of three (all the waves' levels are far above theirs, so the stats are made up in play) */
static void start(const char *seed, int wave, int biome)
{
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 31337);
    run.sys = &sys;
    run.biome = biome;
    run.n_items = 0;
    run.n_party = 3;
    for (int i = 0; i < 3; i++) {
        gc_starter s;
        memset(&s, 0, sizeof s);
        s.species = (uint16_t)(i == 0 ? SPECIES_ID_BULBASAUR : i == 1 ? SPECIES_ID_PIKACHU : SPECIES_ID_GENGAR);
        s.tera_type = -1;
        for (int k = 0; k < 6; k++) s.ivs[k] = 31;
        s.moves[0] = MOVE_ID_TACKLE;   /* four types, so that no team is immune to all of them */
        s.moves[1] = MOVE_ID_SHADOW_BALL;
        s.moves[2] = MOVE_ID_EARTHQUAKE;
        s.moves[3] = MOVE_ID_PSYCHIC;
        gc_mon_new_player(&run, &run.party[i], &s, 100);
        run.party_order[i] = (int8_t)i;
    }
    run.money = 1000;
    run.battle_type = BATTLE_TYPE_WILD;
    gc_items_to_battle(&run);
    gc_rq_reset(&gc_int(&run)->rq);
    if (pin_roll[0] >= 0) {
        gc_int(&run)->evil_roll_pinned = true;
        gc_int(&run)->evil_roll[0] = (uint8_t)pin_roll[0];
        gc_int(&run)->evil_roll[1] = (uint8_t)pin_roll[1];
    }
    run.wave = wave - 1;
    run.in_battle = true;
    gc_rq_push(&run, gc_rp(RP_NEW_BATTLE, 0, 0, 0));
    gc_run_until_screen(&run);
}

/* the AI reads isBoss from the trainer in front of it: checked at a command screen once the enemy has chosen (turn 2 on) */
static void check_ai(void)
{
    if (run.battle_type != BATTLE_TYPE_TRAINER || run.bt.turn < 2) return;
    const gc_trainer_config *cfg = gc_trainer_config_of(run.trainer_type);
    CHECK(cfg);
    CHECK(gc_int(&run)->ai_trainer.is_boss == cfg->is_boss);
    if (cfg->is_boss) n_ai_boss++; else n_ai_plain++;
}

/* the same fight after a save and a load at its start */
static void check_save_load(void)
{
    CHECK(gc_run_can_save(&run) && run.battle_type == BATTLE_TYPE_TRAINER);
    const int type = run.trainer_type, n = run.n_enemy;
    uint16_t species[6];
    int level[6];
    for (int i = 0; i < n; i++) { species[i] = run.enemy[i].b.species; level[i] = run.enemy[i].b.level; }
    const size_t len = gc_run_save(&run, save, sizeof save);
    CHECK(len > 0);
    memset(&back, 0, sizeof back);
    CHECK(gc_run_load(&back, save, len, NULL));
    CHECK(back.wave == run.wave && back.battle_type == BATTLE_TYPE_TRAINER && back.trainer_type == type && back.n_enemy == n);
    for (int i = 0; i < n; i++) CHECK(back.enemy[i].b.species == species[i] && back.enemy[i].b.level == level[i]);
    n_saved++;
}

/* play the battle on screen to its end with an unbeatable party: true when the rewards come up (victory), false on a game over */
static bool win(bool *defeated_text, bool *boss_defeated)
{
    /* the trainer in front of us and its speaker's name now: the events of the win must carry them, whatever the run holds when they are read */
    const int type = run.trainer_type;
    char speaker[128] = "";
    if (run.trainer_names[GC_TNAME_SPEAKER]) snprintf(speaker, sizeof speaker, "%s", run.trainer_names[GC_TNAME_SPEAKER]);
    for (int step = 0; step < 20000; step++) {
        gc_event e;
        while (gc_run_next_event(&run, &e)) {
            /* a win on a wave with no reward screen (every tenth: the gym leaders, the Champion) goes on to the next wave in the same batch of
             * events: the run no longer holds this trainer when the app plays them, so what they say comes from the events */
            const bool moved_on = run.battle_type != BATTLE_TYPE_TRAINER || run.trainer_type != type;
            if (e.kind == GC_EV_MESSAGE && e.msg.key && !strcmp(e.msg.key, "battle:trainerDefeated")) {
                *defeated_text = true;
                CHECK(e.a == type);   /* the victory music is the beaten trainer's */
                if (moved_on) n_defeated_moved_on++;
            }
            if (e.kind == GC_EV_DIALOGUE && e.c == 1) {
                CHECK(e.a == type && e.msg.key);
                CHECK(e.msg.n_args == 1 && e.msg.args[0].s && e.msg.args[0].s[0] && !strchr(e.msg.args[0].s, '@'));   /* a speaker, no template left */
                CHECK(!strcmp(e.msg.args[0].s, speaker));
                n_talk++;
                if (moved_on) n_talk_moved_on++;
            }
        }
        const gc_screen *s = gc_run_screen(&run);
        gc_choice c = {0};
        for (int i = 0; i < run.n_party; i++) {
            pr_pokemon *p = &run.party[i].b;
            if (p->hp <= 0) continue;
            p->hp = p->stats[0] = 100000;
            p->stats[1] = p->stats[3] = 100000;
            p->level = 250;   /* a one-hit KO move fails on a target of a higher level */
        }
        if (run.bt.field[0]) run.bt.field[0]->stat_stages[5] = 6;
        switch (s->kind) {
        case GC_SCREEN_COMMAND: {
            check_ai();
            n_turns++;
            int mv = 0;
            for (int i = 0; i < 4; i++) if (s->move_ok[(i + step) % 4]) { mv = (i + step) % 4; break; }
            for (int i = 0; i < run.n_party; i++) for (int k = 0; k < 4; k++) run.party[i].b.pp_used[k] = 0;
            c = (gc_choice){GC_CH_FIGHT, (int16_t)mv, -1, 0};
            break;
        }
        case GC_SCREEN_TARGET: c = (gc_choice){GC_CH_TARGET, s->target_ok[2] ? 2 : 3, 0, 0}; break;
        case GC_SCREEN_SWITCH: {
            c.kind = GC_CH_PICK;
            c.a = -1;
            for (int i = 0; i < run.n_party; i++) if (s->pick_ok[i]) { c.a = (int16_t)i; break; }
            break;
        }
        case GC_SCREEN_LEARN_MOVE: c = (gc_choice){GC_CH_FORGET, -1, 0, 0}; break;
        case GC_SCREEN_EVOLVE: c.kind = GC_CH_CANCEL; break;
        case GC_SCREEN_CHECK_SWITCH: c.kind = GC_CH_CONFIRM; break;
        case GC_SCREEN_REWARD:
            *boss_defeated = true;
            return true;
        case GC_SCREEN_GAME_OVER: return false;
        default:
            fprintf(stderr, "unexpected screen %d at wave %d\n", s->kind, run.wave);
            CHECK(false);
        }
        const char *why = NULL;
        if (!gc_run_choose(&run, &c, &why)) { fprintf(stderr, "choose failed: screen %d: %s\n", s->kind, why ? why : "?"); CHECK(false); }
    }
    CHECK(false);
    return false;
}

/* one boss wave: `kind` 0 gym leader, 1 Elite Four, 2 Champion */
static void fight(const char *seed, int wave, int biome, int kind)
{
    start(seed, wave, biome);
    CHECK(run.wave == wave);
    const gc_trainer_config *cfg = gc_trainer_config_of(run.trainer_type);
    CHECK(run.battle_type == BATTLE_TYPE_TRAINER);   /* and not the wild stand-in */
    CHECK(cfg && cfg->is_boss);
    /* the one stand-in left: the held items of a trainer with a genModifiersFunc (Red, Steven, Wallace, Kieran), which run-core cannot make yet */
    const int t = run.trainer_type;
    const bool no_items = t >= 0 && t < pr_trainer_item_flag_count && (pr_trainer_item_flags[t] & 2);
    CHECK(run.stubs[GC_STUB_UNPORTED_ENEMY] == (no_items ? 1u : 0u) && !run.run_core_unported[4] && !run.run_core_unported[5]);
    const int e4 = pr_run_fixed_e4_trainer(&run.run, wave);
    if (kind == 0) {
        CHECK(e4 < 0 && run.n_enemy >= 2 && run.n_enemy <= 6);
        n_gym++;
    } else {
        CHECK(e4 == run.trainer_type && run.n_enemy == 6 && !run.double_battle);
        if (kind == 2) n_champion++; else n_e4++;
    }
    for (int i = 0; i < run.n_enemy; i++) CHECK(run.enemy[i].b.species > 0 && run.enemy[i].b.level >= 1);
    check_save_load();

    const double money = run.money, mult = cfg->money_multiplier;
    bool defeated = false, boss = false;
    CHECK(win(&defeated, &boss));
    CHECK(defeated && boss);
    CHECK(!run.stubs[GC_STUB_TRAINER_REWARDS]);
    /* MoneyRewardPhase: getMoneyAmount(waveIndex) * moneyMultiplier (no Amulet Coin, no Happy Hour here) */
    const double prize = pr_run_get_money_reward(wave, mult, NULL, 0, false);
    CHECK(run.money == pr_run_add_money(money, prize));
    CHECK(run.money > money);
    n_defeated++;
}

static const int evil_grunt_types[10] = {
    TRAINER_TYPE_ROCKET_GRUNT, TRAINER_TYPE_MAGMA_GRUNT, TRAINER_TYPE_AQUA_GRUNT, TRAINER_TYPE_GALACTIC_GRUNT, TRAINER_TYPE_PLASMA_GRUNT,
    TRAINER_TYPE_FLARE_GRUNT, TRAINER_TYPE_AETHER_GRUNT, TRAINER_TYPE_SKULL_GRUNT, TRAINER_TYPE_MACRO_GRUNT, TRAINER_TYPE_STAR_GRUNT,
};
static int n_youngster, n_grunt, n_grunt_double, n_grunt_female, n_grunt_male, n_evil_won, n_admin, n_boss;
static const int evil_bosses_1[10] = {
    TRAINER_TYPE_ROCKET_BOSS_GIOVANNI_1, TRAINER_TYPE_MAXIE, TRAINER_TYPE_ARCHIE, TRAINER_TYPE_CYRUS, TRAINER_TYPE_GHETSIS, TRAINER_TYPE_LYSANDRE,
    TRAINER_TYPE_LUSAMINE, TRAINER_TYPE_GUZMA, TRAINER_TYPE_ROSE, TRAINER_TYPE_PENNY,
};
static const int evil_bosses_2[10] = {
    TRAINER_TYPE_ROCKET_BOSS_GIOVANNI_2, TRAINER_TYPE_MAXIE_2, TRAINER_TYPE_ARCHIE_2, TRAINER_TYPE_CYRUS_2, TRAINER_TYPE_GHETSIS_2,
    TRAINER_TYPE_LYSANDRE_2, TRAINER_TYPE_LUSAMINE_2, TRAINER_TYPE_GUZMA_2, TRAINER_TYPE_ROSE_2, TRAINER_TYPE_PENNY_2,
};
static int boss_index(const int *row, int type) { for (int i = 0; i < 10; i++) if (row[i] == type) return i; return -1; }

/* the Town Youngster (wave 5) or a grunt fight; for a grunt the type of the run's first one, so that every fight of the run is the same team */
static int evil_fight(const char *seed, int wave, int team)
{
    start(seed, wave, BIOME_ID_PLAINS);
    CHECK(run.wave == wave && run.battle_type == BATTLE_TYPE_TRAINER);   /* and not the wild stand-in */
    const gc_trainer_config *cfg = gc_trainer_config_of(run.trainer_type);
    const bool boss_wave = wave == CLASSIC_FIXED_BOSS_WAVES_EVIL_BOSS_1 || wave == CLASSIC_FIXED_BOSS_WAVES_EVIL_BOSS_2;
    const bool admin_wave = wave == CLASSIC_FIXED_BOSS_WAVES_EVIL_ADMIN_1 || wave == CLASSIC_FIXED_BOSS_WAVES_EVIL_ADMIN_2 || wave == CLASSIC_FIXED_BOSS_WAVES_EVIL_ADMIN_3;
    CHECK(cfg && cfg->is_boss == (boss_wave || admin_wave));   /* admins are setBoss() too */
    /* the held items of a trainer with a genModifiersFunc (Guzma's second form) are the one stand-in left */
    const int ty = run.trainer_type;
    const bool no_items = ty >= 0 && ty < pr_trainer_item_flag_count && (pr_trainer_item_flags[ty] & 2);
    if (boss_wave || admin_wave) {
        CHECK(run.stubs[GC_STUB_UNPORTED_ENEMY] == (no_items ? 1u : 0u) && !run.run_core_unported[4] && !run.run_core_unported[5]);
        CHECK(run.n_enemy >= 5 && run.n_enemy <= 6 && !run.double_battle);
        if (boss_wave) {
            const int idx = boss_index(wave == CLASSIC_FIXED_BOSS_WAVES_EVIL_BOSS_1 ? evil_bosses_1 : evil_bosses_2, run.trainer_type);
            CHECK(idx >= 0);
            if (team >= 0) CHECK(idx == boss_index(evil_grunt_types, team));   /* the run's one evil team: the grunts' index is the boss's */
            n_boss++;
        } else {
            n_admin++;
        }
    } else if (wave == CLASSIC_FIXED_BOSS_WAVES_TOWN_YOUNGSTER) {
        CHECK(!run.stubs[GC_STUB_UNPORTED_ENEMY] && !run.run_core_unported[4] && !run.run_core_unported[5]);
        CHECK(run.trainer_type == TRAINER_TYPE_YOUNGSTER && !run.double_battle);
        CHECK(run.trainer_variant == TRAINER_VARIANT_DEFAULT || run.trainer_variant == TRAINER_VARIANT_FEMALE);
        n_youngster++;
    } else {
        CHECK(!run.stubs[GC_STUB_UNPORTED_ENEMY] && !run.run_core_unported[4] && !run.run_core_unported[5]);
        bool grunt = false;
        for (int i = 0; i < 10; i++) grunt |= run.trainer_type == evil_grunt_types[i];
        CHECK(grunt);
        if (team >= 0) CHECK(run.trainer_type == team);
        const int size = wave == 35 ? 2 : wave == 62 || wave == 64 ? 3 : 6;
        CHECK(run.n_enemy == size);
        CHECK(run.double_battle == (run.trainer_variant == TRAINER_VARIANT_DOUBLE));
        if (run.double_battle) n_grunt_double++;
        else if (run.trainer_variant == TRAINER_VARIANT_FEMALE) n_grunt_female++;
        else n_grunt_male++;
        n_grunt++;
    }
    for (int i = 0; i < run.n_enemy; i++) CHECK(run.enemy[i].b.species > 0 && run.enemy[i].b.level >= 1);
    const int type = run.trainer_type;
    check_save_load();
    const double money = run.money, mult = cfg->money_multiplier;
    bool defeated = false, boss = false;
    CHECK(win(&defeated, &boss));
    CHECK(defeated && boss);
    CHECK(!run.stubs[GC_STUB_TRAINER_REWARDS]);
    CHECK(run.money == pr_run_add_money(money, pr_run_get_money_reward(wave, mult, NULL, 0, false)));
    n_evil_won++;
    return type;
}

/* an ordinary random trainer of some wave in [first, first + 40): the AI must not be told it faces a boss */
static void ordinary(const char *seed, int first)
{
    for (int wave = first; wave < first + 40; wave++) {
        if (wave % 10 == 5 || pr_run_is_trainer_boss(&run.run, wave, BIOME_ID_PLAINS, false) || pr_run_is_fixed_battle(&run.run, wave)) continue;
        start(seed, wave, BIOME_ID_PLAINS);
        const gc_trainer_config *cfg = run.battle_type == BATTLE_TYPE_TRAINER ? gc_trainer_config_of(run.trainer_type) : NULL;
        if (!cfg || cfg->is_boss || run.n_enemy < 2) continue;
        bool defeated = false, boss = false;
        CHECK(win(&defeated, &boss));
        CHECK(defeated);
        n_ordinary++;
        return;
    }
}

int main(void)
{
    const char *seeds[] = {"gym-e4-a", "gym-e4-b", "gym-e4-c", "gym-e4-d", "gym-e4-e", "gym-e4-f", "gym-e4-g", "gym-e4-h"};
    const int runs = (int)(sizeof seeds / sizeof *seeds);
    for (int i = 0; i < runs; i++) {
        start(seeds[i], 1, BIOME_ID_PLAINS);   /* the seed decides offsetGym */
        const bool offset_gym = run.run.offset_gym;
        for (int wave = offset_gym ? 30 : 20; wave < 200; wave += 30) fight(seeds[i], wave, 1 + (i * 5 + wave / 10) % 31, 0);   /* the start town has no boss pool */
        for (int wave = 182; wave <= 190; wave += 2) fight(seeds[i], wave, 1 + (i * 3 + wave) % 31, wave == 190 ? 2 : 1);
        ordinary(seeds[i], 12);
        /* the Youngster and the four grunt fights of the run: one team from the first to the last */
        pin_roll[0] = pin_roll[1] = -1;
        evil_fight(seeds[i], CLASSIC_FIXED_BOSS_WAVES_TOWN_YOUNGSTER, -1);
        const int team = evil_fight(seeds[i], CLASSIC_FIXED_BOSS_WAVES_EVIL_GRUNT_1, -1);
        evil_fight(seeds[i], CLASSIC_FIXED_BOSS_WAVES_EVIL_GRUNT_2, team);
        evil_fight(seeds[i], CLASSIC_FIXED_BOSS_WAVES_EVIL_GRUNT_3, team);
        evil_fight(seeds[i], CLASSIC_FIXED_BOSS_WAVES_EVIL_GRUNT_4, team);
        /* the three admins and the two bosses of the same run: the same team again */
        evil_fight(seeds[i], CLASSIC_FIXED_BOSS_WAVES_EVIL_ADMIN_1, team);
        evil_fight(seeds[i], CLASSIC_FIXED_BOSS_WAVES_EVIL_ADMIN_2, team);
        evil_fight(seeds[i], CLASSIC_FIXED_BOSS_WAVES_EVIL_ADMIN_3, team);
        evil_fight(seeds[i], CLASSIC_FIXED_BOSS_WAVES_EVIL_BOSS_1, team);
        evil_fight(seeds[i], CLASSIC_FIXED_BOSS_WAVES_EVIL_BOSS_2, team);
    }
    /* the unseeded rolls pinned: randInt(2) === 0 is the female grunt, randInt(3) === 0 a double (a grunt that has one) */
    for (int g = 0; g < 2; g++)
        for (int d = 0; d < 3; d++) {
            pin_roll[0] = g;
            pin_roll[1] = d;
            evil_fight("gym-e4-a", CLASSIC_FIXED_BOSS_WAVES_EVIL_GRUNT_1, -1);
            CHECK(run.trainer_variant == (d == 0 ? TRAINER_VARIANT_DOUBLE : g == 0 ? TRAINER_VARIANT_FEMALE : TRAINER_VARIANT_DEFAULT) && run.double_battle == (d == 0));
        }
    pin_roll[0] = pin_roll[1] = -1;
    CHECK(n_gym == 6 * runs);
    CHECK(n_e4 == 4 * runs);
    CHECK(n_champion == runs);
    CHECK(n_defeated == 11 * runs);
    CHECK(n_saved == 11 * runs + n_evil_won);   /* and the Youngster, the grunt, admin and boss fights */
    CHECK(n_ordinary > 0 && n_ai_plain > 0 && n_ai_boss > 0);
    CHECK(n_youngster == runs && n_grunt == 4 * runs + 6 && n_evil_won == 10 * runs + 6);
    CHECK(n_admin == 3 * runs && n_boss == 2 * runs);
    CHECK(n_grunt_double > 0 && n_grunt_female > 0 && n_grunt_male > 0);
    /* the gym leaders' and Elite Four's wins with no reward screen: the beaten trainer is named by the events themselves */
    CHECK(n_defeated_moved_on == 7 * runs);   /* the six gym waves and the Champion's 190 (the Elite Four's 182..188 have their reward screen) */
    CHECK(n_talk > 0 && n_talk_moved_on > 0);
    printf("evil teams: %d Town Youngsters, %d grunt fights (%d single male, %d single female, %d double), %d admins, %d bosses, all won with their prize money\n",
           n_youngster, n_grunt, n_grunt_male, n_grunt_female, n_grunt_double, n_admin, n_boss);
    printf("gym/e4: %d seeds, %d gym leaders, %d Elite Four, %d Champions won (prize money, no battle stand-ins), %d ordinary trainers, AI boss flag read %d times "
           "(%d bosses, %d not), %d battles saved and loaded back, %d victory lines (%d played after the run had moved on, as the events say); %d checks passed\n",
           runs, n_gym, n_e4, n_champion, n_ordinary, n_ai_boss + n_ai_plain, n_ai_boss, n_ai_plain, n_saved, n_talk, n_talk_moved_on, checks);
    return 0;
}
