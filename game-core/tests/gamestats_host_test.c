/* GameStats (upstream src/system/game-stats.ts) in game-core: the recorders' arithmetic, the places they are called from in a real
 * headless wave, the "me_no_stats" skip (Training Session), and the save format (a "gstats" section only once something is counted,
 * an older profile loads with zeros). The seen / caught / hatched counters have their own differential (tools/gamestatsdiff.sh). */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;
static uint8_t buf[GC_SAVE_MAX];
static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "GameStats host line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static void settle(gc_run *r) { gc_event ev; while (gc_run_next_event(r, &ev)) {} }

static void recorders(void)
{
    gc_system s;
    gc_system_init_defaults(&s);
    gc_stats_session_started(&s, GAME_MODES_CLASSIC);
    gc_stats_session_started(&s, GAME_MODES_CHALLENGE);
    gc_stats_session_started(&s, GAME_MODES_ENDLESS);
    gc_stats_session_started(&s, GAME_MODES_SPLICED_ENDLESS);
    gc_stats_session_started(&s, GAME_MODES_DAILY);
    CHECK(s.gstats.classic_sessions_played == 2 && s.gstats.endless_sessions_played == 2 && s.gstats.daily_run_sessions_played == 1);

    /* BattleEndPhase: battles always, the wave only in the endless modes, a trainer only when won */
    gc_stats_battle_end(&s, GAME_MODES_CLASSIC, 40, true, false);
    CHECK(s.stats.total_battles == 1 && s.gstats.highest_endless_wave == 0 && s.gstats.trainers_defeated == 0);
    gc_stats_battle_end(&s, GAME_MODES_ENDLESS, 40, true, true);
    CHECK(s.stats.total_battles == 2 && s.gstats.highest_endless_wave == 41 && s.gstats.trainers_defeated == 1);
    gc_stats_battle_end(&s, GAME_MODES_SPLICED_ENDLESS, 10, false, true);   /* lower wave never lowers it; a lost trainer fight is not defeated */
    CHECK(s.stats.total_battles == 3 && s.gstats.highest_endless_wave == 41 && s.gstats.trainers_defeated == 1);

    /* the "highest" ones only rise; zero and negative values are ignored; the top saturates */
    gc_stats_damage(&s, 120); gc_stats_damage(&s, 90); gc_stats_damage(&s, -5); gc_stats_damage(&s, 0);
    CHECK(s.stats.highest_damage == 120);
    gc_stats_heal(&s, 33); gc_stats_heal(&s, 7); gc_stats_heal(&s, 0);
    CHECK(s.gstats.highest_heal == 33);
    gc_stats_level_up(&s, 55); gc_stats_level_up(&s, 30);
    CHECK(s.gstats.highest_level == 55);
    gc_stats_run_loaded(&s, 1234.0); gc_stats_run_loaded(&s, 99.0);
    CHECK(s.gstats.highest_money == 1234);
    gc_stats_run_loaded(&s, 1e12);
    CHECK(s.gstats.highest_money == UINT32_MAX);
    gc_stats_run_loaded(NULL, 5); gc_stats_pokemon_fused(NULL); gc_stats_pokemon_defeated(NULL);   /* a run with no profile is fine */
    s.gstats.pokemon_fused = UINT32_MAX; gc_stats_pokemon_fused(&s);
    CHECK(s.gstats.pokemon_fused == UINT32_MAX);
    gc_stats_pokemon_defeated(&s); gc_stats_pokemon_defeated(&s);
    CHECK(s.gstats.pokemon_defeated == 2);
    gc_stats_add_play_time(&s, 100); gc_stats_add_play_time(&s, UINT32_MAX);
    CHECK(s.gstats.play_time == UINT32_MAX);
}

static pr_pokemon *start(int mode, int move, pr_pokemon **foe)
{
    const char *why = NULL;
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, mode, "gamestats", 777);
    run.sys = &sys;
    sys.dex[SPECIES_ID_BULBASAUR].caught_attr = 1u | 4u | 128u; sys.starters[SPECIES_ID_BULBASAUR].ability_attr = 7;                 /* what the starter select needs to hand the starter over */
    sys.dex[SPECIES_ID_BULBASAUR].nature_attr = 0xFFFFFFFFu;
    gc_starter s = {.species = SPECIES_ID_BULBASAUR, .tera_type = -1, .moves = {(uint16_t)move}};
    memset(s.ivs, 15, sizeof s.ivs);
    if (!gc_run_set_starters(&run, &s, 1, &why)) fprintf(stderr, "why: %s\n", why ? why : "?");
    CHECK(why == NULL); settle(&run);
    CHECK(run.screen.kind == GC_SCREEN_COMMAND);
    pr_pokemon *p = run.bt.field[0], *f = run.bt.field[2];
    CHECK(p && f);
    p->moves[0] = (uint16_t)move; p->n_moves = 1; p->pp_used[0] = 0;
    f->moves[0] = MOVE_ID_SPLASH; f->n_moves = 1; f->pp_used[0] = 0;
    *foe = f;
    return p;
}

static void fight(void)
{
    const char *why = NULL;
    gc_choice c = {GC_CH_FIGHT, 0, -1, 0};
    run.n_battle_events = 0;
    CHECK(gc_run_choose(&run, &c, &why));
    settle(&run);
}

static void headless_waves(void)
{
    pr_pokemon *foe;
    pr_pokemon *p = start(GAME_MODES_CLASSIC, MOVE_ID_TACKLE, &foe);
    CHECK(sys.gstats.classic_sessions_played == 1 && sys.gstats.endless_sessions_played == 0);
    CHECK(sys.gstats.pokemon_seen >= 1);                       /* the wild foe was seen (setPokemonSeen) */
    CHECK(sys.stats.total_battles == 0 && sys.gstats.pokemon_defeated == 0);
    const int foe_max = foe->stats[STAT_HP];
    foe->hp = foe_max;
    p->hp = p->stats[STAT_HP];
    fight();                                                   /* Tackle: a hit, maybe not a knock-out */
    CHECK(sys.stats.highest_damage > 0 && sys.stats.highest_damage <= (uint32_t)foe_max + 200u);   /* the hit the lead dealt */
    const uint32_t hit = sys.stats.highest_damage;
    foe->hp = 1;
    p->hp = p->stats[STAT_HP];
    const int level = p->level;
    run.party[0].exp = gc_level_total_exp(level + 1, gc_species(p->species)->growth_rate) - 1;   /* one point short of the next level */
    fight();                                                   /* the knock-out: VictoryPhase, then the wave's end */
    CHECK(foe->hp == 0);
    CHECK(sys.gstats.pokemon_defeated == 1);
    CHECK(sys.stats.highest_damage >= hit);
    CHECK(sys.gstats.highest_level >= (uint32_t)level + 1);    /* LevelUpPhase */
    CHECK(sys.stats.total_battles == 1);                       /* BattleEndPhase */
    CHECK(sys.gstats.highest_endless_wave == 0 && sys.gstats.trainers_defeated == 0);   /* Classic wild wave */

    /* a heal that goes through the heal phase (Recover) records the amount; a hurt foe's hit on the player does not count as damage */
    p = start(GAME_MODES_ENDLESS, MOVE_ID_RECOVER, &foe);
    CHECK(sys.gstats.endless_sessions_played == 1 && sys.gstats.classic_sessions_played == 0);
    const int max = p->stats[STAT_HP];
    p->hp = max / 4;
    fight();
    CHECK(sys.gstats.highest_heal > 0 && sys.gstats.highest_heal <= (uint32_t)max);
    CHECK(sys.stats.highest_damage == 0);                      /* the foe only Splashed; the lead never dealt damage */

    /* only the player's side counts: a foe's Tackle and its Recover leave highestDamage and highestHeal alone */
    p = start(GAME_MODES_CLASSIC, MOVE_ID_SPLASH, &foe);
    foe->moves[0] = MOVE_ID_TACKLE;
    p->hp = p->stats[STAT_HP];
    fight();
    CHECK(p->hp < p->stats[STAT_HP]);                          /* the foe's Tackle did land */
    CHECK(sys.stats.highest_damage == 0 && sys.gstats.highest_heal == 0);
    p = start(GAME_MODES_CLASSIC, MOVE_ID_SPLASH, &foe);
    foe->moves[0] = MOVE_ID_RECOVER;
    foe->hp = foe->stats[STAT_HP] / 4;
    fight();
    CHECK(foe->hp > foe->stats[STAT_HP] / 4);                  /* the foe did heal */
    CHECK(sys.stats.highest_damage == 0 && sys.gstats.highest_heal == 0);

    /* a run without a profile plays on */
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, "gamestats-nosys", 777);
    gc_starter s = {.species = SPECIES_ID_BULBASAUR, .tera_type = -1, .moves = {MOVE_ID_TACKLE}};
    memset(s.ivs, 15, sizeof s.ivs);
    const char *why = NULL;
    CHECK(gc_run_set_starters(&run, &s, 1, &why)); settle(&run);
    pr_pokemon *lone = run.bt.field[2];
    lone->hp = 1;
    fight();
    CHECK(lone->hp == 0);
}

static void training_session(void)
{
    /* an encounter that prevents the game stats' updates (Training Session sets me_no_stats) leaves pokemonDefeated alone */
    pr_pokemon *foe;
    start(GAME_MODES_CLASSIC, MOVE_ID_TACKLE, &foe);
    gc_int(&run)->me_no_stats = true;
    foe->hp = 1;
    fight();
    CHECK(foe->hp == 0 && sys.gstats.pokemon_defeated == 0);
    gc_int(&run)->me_no_stats = false;
}

static void save_format(void)
{
    gc_system a, b;
    gc_system_init_defaults(&a);
    const size_t plain = gc_system_save(&a, buf, sizeof buf);
    CHECK(plain > 0);
    /* nothing counted: no section, so the bytes are the ones a profile always saved */
    gc_system_init_defaults(&b);
    CHECK(gc_system_save(&b, buf, sizeof buf) == plain);

    gc_system_init_defaults(&a);
    a.gstats.play_time = 123456; a.gstats.classic_sessions_played = 3; a.gstats.daily_run_sessions_won = 1;
    a.gstats.endless_sessions_played = 2; a.gstats.highest_endless_wave = 250; a.gstats.highest_level = 100;
    a.gstats.highest_money = 99999999; a.gstats.highest_heal = 321; a.gstats.pokemon_seen = 77; a.gstats.pokemon_defeated = 5000;
    a.gstats.pokemon_hatched = 12; a.gstats.sub_legendary_seen = 1; a.gstats.sub_legendary_caught = 2; a.gstats.sub_legendary_hatched = 3;
    a.gstats.legendary_seen = 4; a.gstats.legendary_caught = 5; a.gstats.legendary_hatched = 6;
    a.gstats.mythical_seen = 7; a.gstats.mythical_caught = 8; a.gstats.mythical_hatched = 9;
    a.gstats.shiny_seen = 10; a.gstats.shiny_caught = 11; a.gstats.shiny_hatched = 12;
    a.gstats.pokemon_fused = 13; a.gstats.trainers_defeated = 14;
    a.stats.total_battles = 4242; a.stats.highest_damage = 987654;
    const size_t n = gc_system_save(&a, buf, sizeof buf);
    CHECK(n > plain);
    const char *why = NULL;
    gc_system_init_defaults(&b);
    CHECK(gc_system_load(&b, buf, n, &why));
    CHECK(!memcmp(&a.gstats, &b.gstats, sizeof a.gstats));
    CHECK(b.stats.total_battles == 4242 && b.stats.highest_damage == 987654);
    uint8_t again[GC_SAVE_MAX];
    CHECK(gc_system_save(&b, again, sizeof again) == n && !memcmp(buf, again, n));   /* canonical: a round trip is byte-identical */

    /* a profile saved before the counters existed (no section) loads with them at zero, over whatever memory held */
    gc_system_init_defaults(&b);
    const size_t pn = gc_system_save(&b, again, sizeof again);
    gc_system e;
    memset(&e, 0xAB, sizeof e);
    CHECK(gc_system_load(&e, again, pn, &why));
    CHECK(e.gstats.play_time == 0 && e.gstats.pokemon_defeated == 0 && e.gstats.trainers_defeated == 0);
}

int main(void)
{
    recorders();
    headless_waves();
    training_session();
    save_format();
    printf("GameStats host: %d checks passed\n", checks);
    return 0;
}
