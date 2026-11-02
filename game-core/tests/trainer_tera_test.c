/* Trainer Terastallization in the run (src/gc_encounter.c: gc_trainer_gen_ai after the enemy's items, src/gc_battle.c: ai_trainer_on), played wave by wave
 * with an unbeatable party, on the gym leaders, the Elite Four and the Champion:
 *  - the config's own Tera slots (the Champion's) and the one slot genAI picks (the gym leaders' from wave 100, the Elite Four's always) are what the
 *    AI is told: instantTeras, tera mode INSTANT_TERA; a gym leader before wave 100 has no pick and the AI's tera mode stays NO_TERA;
 *  - the member that is listed Terastallizes on its first move (the enemy's TeraPhase), and no other enemy does; the pick gets the trainer's
 *    specialty type; the pick is kept by a save and a load at the battle's start.
 * Each fixture fails on game-core as it was (nobody ever Terastallized). */
#include "../src/internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run, back;
static gc_system sys;
static unsigned char save[GC_SAVE_MAX];
static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "trainer tera FAIL line %d (wave %d, trainer %d): %s\n", __LINE__, run.wave, run.trainer_type, #x); exit(1); } } while (0)

static int n_listed_lost, n_ai_told, n_defeated_moved_on, n_talk, n_talk_moved_on;
static int n_gym, n_gym_late, n_e4, n_champion, n_defeated, n_saved, n_turns, n_listed, n_teraed, n_picked, n_static, n_typed, n_none;
static int tera_messages;
static unsigned seen_listed;   /* bit i: the listed slot i was in the field at a command screen (the first one is its first move) */
static int expect_n = -1;

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
    run.wave = wave - 1;
    run.in_battle = true;
    gc_rq_push(&run, gc_rp(RP_NEW_BATTLE, 0, 0, 0));
    gc_run_until_screen(&run);
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
    for (int i = 0; i < n; i++) CHECK(back.enemy[i].tera_instant == run.enemy[i].tera_instant && back.enemy[i].b.tera_type == run.enemy[i].b.tera_type);
    int8_t a[8], b[8];
    CHECK(gc_trainer_instant_teras(&back, b, 8) == gc_trainer_instant_teras(&run, a, 8));
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
            if (e.kind == GC_EV_MESSAGE && e.msg.key && !strcmp(e.msg.key, "battle:pokemonTerastallized")) tera_messages++;
            if (e.kind == GC_EV_BATTLE) {   /* the turn's battle events carry the text of a TeraPhase */
                const pr_event *ev = gc_run_battle_event_data(&run) + e.a;
                for (int k = 0; k < e.b; k++)
                    if (ev[k].type == PR_EV_TEXT && ev[k].msg.key && !strcmp(ev[k].msg.key, "battle:pokemonTerastallized")) tera_messages++;
            }
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
            n_turns++;
            if (run.battle_type == BATTLE_TYPE_TRAINER) {
                int8_t li[8];
                const int nl = gc_trainer_instant_teras(&run, li, 8);
                for (int q = 2; q < 4; q++) {
                    const pr_pokemon *en = run.bt.field[q];
                    const pr_ai_mon *am = en ? pr_ai_mon_of(&gc_int(&run)->ai, en) : NULL;
                    for (int k = 0; am && k < nl; k++) if (am->initial_team_index == li[k]) seen_listed |= 1u << li[k];
                }
            }
            /* the AI was told what the config and genAI list, from the enemy's first command on */
            if (expect_n >= 0 && run.battle_type == BATTLE_TYPE_TRAINER && run.bt.turn >= 2) {
                CHECK(gc_int(&run)->ai_trainer.n_instant_teras == expect_n);
                CHECK(gc_int(&run)->ai_trainer.tera_mode == (expect_n ? PR_TERA_AI_INSTANT_TERA : PR_TERA_AI_NO_TERA));
                n_ai_told++;
            }
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
    CHECK(run.battle_type == BATTLE_TYPE_TRAINER && cfg && cfg->is_boss);
    const int e4 = pr_run_fixed_e4_trainer(&run.run, wave);
    if (kind == 0) { CHECK(e4 < 0); n_gym++; if (wave >= 100) n_gym_late++; }
    else { CHECK(e4 == run.trainer_type); if (kind == 2) n_champion++; else n_e4++; }
    /* what the config says: the listed slots, and the random pick if it applies at this wave */
    const bool random = cfg->tera_random && wave >= cfg->tera_min_wave;
    int picked = 0, picked_slot = -1;
    for (int i = 0; i < run.n_enemy; i++) if (run.enemy[i].tera_instant) { picked++; picked_slot = i; }
    CHECK(picked == (random ? 1 : 0));
    if (random && cfg->tera_type >= 0) { CHECK(run.enemy[picked_slot].b.tera_type == cfg->tera_type); n_typed++; }
    if (random && run.n_enemy <= 6 && cfg->tera_slot[run.n_enemy] >= 0 && run.enemy[cfg->tera_slot[run.n_enemy]].b.species != SPECIES_ID_SHEDINJA)
        CHECK(picked_slot == cfg->tera_slot[run.n_enemy]);
    int8_t listed[8];
    const int n_listed_now = gc_trainer_instant_teras(&run, listed, 8);
    CHECK(n_listed_now == picked + __builtin_popcount(cfg->tera_static));
    /* the AI is told the same when the enemy chooses (checked at the first command screen: ai_trainer was set by the enemy's choice) */
    n_picked += picked;
    n_static += __builtin_popcount(cfg->tera_static);
    if (!n_listed_now) n_none++;
    n_listed += n_listed_now;
    check_save_load();
    tera_messages = 0;
    seen_listed = 0;
    expect_n = n_listed_now;
    bool defeated = false, boss = false;
    CHECK(win(&defeated, &boss));
    CHECK(defeated && boss);
    /* a listed member that got a command in the field Terastallizes at it (a TeraPhase and its text), once, and nobody else does. (A member the
     * trainer switches in over a fallen one can be knocked out before it commands: it never gets to Terastallize, as upstream's.) */
    CHECK(tera_messages == __builtin_popcount(seen_listed));
    CHECK(__builtin_popcount(seen_listed) <= n_listed_now);
    n_teraed += tera_messages;
    n_listed_lost += n_listed_now - __builtin_popcount(seen_listed);
    n_defeated++;
}

int main(void)
{
    const char *seeds[] = {"gym-e4-a", "gym-e4-b", "gym-e4-c", "gym-e4-d", "gym-e4-e", "gym-e4-f", "gym-e4-g", "gym-e4-h"};
    const int runs = (int)(sizeof seeds / sizeof *seeds);
    for (int i = 0; i < runs; i++) {
        start(seeds[i], 1, BIOME_ID_PLAINS);   /* the seed decides offsetGym */
        const bool offset_gym = run.run.offset_gym;
        for (int wave = offset_gym ? 30 : 20; wave < 200; wave += 30) fight(seeds[i], wave, 1 + (i * 5 + wave / 10) % 31, 0);
        for (int wave = 182; wave <= 190; wave += 2) fight(seeds[i], wave, 1 + (i * 3 + wave) % 31, wave == 190 ? 2 : 1);
    }
    CHECK(n_ai_told > 0);
    CHECK(n_gym_late > 0 && n_gym_late < n_gym);   /* gym leaders on both sides of wave 100 */
    CHECK(n_picked >= n_gym_late + n_e4 && n_static > 0 && n_teraed > 0 && n_none > 0);
    printf("trainer tera: %d seeds, %d gym leaders (%d from wave 100), %d Elite Four, %d Champions; %d picked slots, %d config slots, %d Terastallized in play "
           "(each listed one that got a command in the field; %d listed ones were knocked out before they could), the AI told its list at %d command screens, %d battles with nobody listed, %d picks given the specialty type, %d battles saved and loaded back; %d checks passed\n",
           runs, n_gym, n_gym_late, n_e4, n_champion, n_picked, n_static, n_teraed, n_listed_lost, n_ai_told, n_none, n_typed, n_saved, checks);
    return 0;
}
