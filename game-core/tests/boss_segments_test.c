/* Task 93: boss segments in battles. game-core creates the enemy; battle-core's boss shield code reads
 * `boss_segments` and `boss_segment_index` on the battle Pokemon, which game-core never used to set, so a
 * boss took damage like any other Pokemon and its segments never broke. Host consistency checks on seeded
 * runs, not a new upstream comparison (battle-core's own boss-segment differential is unchanged):
 *  - a wild boss wave and every other enemy: the count is run-core's getEncounterBossSegments, the shield
 *    index is count - 1, and a hit that passes the first segment stops there and sets off its stat boost;
 *  - a trainer's Pokemon on a boss wave, and a mystery encounter's boss;
 *  - save and load with a segment already broken: the same fields, and the same next turn;
 *  - a boss that is caught is a PlayerPokemon: no shields, no boss passive. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;
static int checks;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "boss segments FAIL line %d: %s\n", __LINE__, #x); exit(1); } checks++; } while (0)

static int alive_member(void)
{
    for (int i = 0; i < run.n_party; i++) if (run.party[i].b.hp > 0) return i;
    return 0;
}

static void heal_party(void)
{
    for (int i = 0; i < run.n_party; i++) run.party[i].b.hp = run.party[i].b.stats[0];
}

static void drain(void)
{
    gc_event e;
    while (gc_run_next_event(&run, &e)) {}
}

static void new_run(gc_run *r, const char *seed)
{
    memset(r, 0, sizeof *r);
    gc_system_init_defaults(&sys);
    gc_run_new(r, GAME_MODES_CLASSIC, seed, 7);
    gc_starter team[3];
    memset(team, 0, sizeof team);
    const int species[3] = {SPECIES_ID_BULBASAUR, SPECIES_ID_CHARMANDER, SPECIES_ID_SQUIRTLE};
    for (int i = 0; i < 3; i++) {
        team[i].species = (uint16_t)species[i];
        team[i].tera_type = -1;
        for (int k = 0; k < 6; k++) team[i].ivs[k] = 15;
    }
    const char *why;
    CHECK(gc_run_set_starters(r, team, 3, &why));
    r->sys = &sys;
}

/* a level-100 party, healed at every command; plays on until `want()` holds at a COMMAND screen (-1 = gave up
 * after `max_wave`, or the run asked something this loop doesn't answer) */
static int play_until(const char *seed, bool (*want)(void), int max_wave)
{
    const char *why;
    new_run(&run, seed);
    for (int i = 0; i < run.n_party; i++) {
        run.party[i].b.level = 100;
        gc_mon_recalc(&run, &run.party[i]);
    }
    heal_party();
    for (int step = 0; step < 30000; step++) {
        drain();
        const gc_screen *s = gc_run_screen(&run);
        gc_choice c;
        memset(&c, 0, sizeof c);
        switch (s->kind) {
        case GC_SCREEN_COMMAND:
            if (want()) return run.wave;
            if (run.wave > max_wave) return -1;
            heal_party();
            c.kind = GC_CH_FIGHT;
            c.b = -1;
            for (int i = 0; i < 4; i++) if (s->move_ok[i]) { c.a = (int16_t)i; break; }
            break;
        case GC_SCREEN_TARGET: c.kind=GC_CH_TARGET;c.a=-1;for(int k=2;k<4;k++)if(s->target_ok[k]){c.a=k;break;}if(c.a<0)for(int k=0;k<2;k++)if(s->target_ok[k]){c.a=k;break;}break;
        case GC_SCREEN_SWITCH:
            c.kind = GC_CH_PICK;
            c.a = -1;
            for (int i = 0; i < run.n_party; i++) if (s->pick_ok[i]) { c.a = (int16_t)i; break; }
            break;
        case GC_SCREEN_LEARN_MOVE: c.kind = GC_CH_FORGET; c.a = -1; break;
        case GC_SCREEN_EVOLVE: c.kind = GC_CH_CANCEL; c.a = 1; break;
        case GC_SCREEN_REWARD: c.kind = GC_CH_SKIP; break;
        case GC_SCREEN_BIOME: c.kind = GC_CH_BIOME; break;
        case GC_SCREEN_MYSTERY: c.kind = GC_CH_MYSTERY; c.b = (int16_t)alive_member(); break;
        default: return -1;
        }
        if (!gc_run_choose(&run, &c, &why)) return -1;
    }
    return -1;
}

/* what run-core's getEncounterBossSegments says of this enemy (0 = not a boss) */
static int expected_segments(const gc_mon *m)
{
    return pr_run_get_encounter_boss_segments(&run.run, run.wave, m->b.level, m->b.species, false);
}

/* a wild boss by run-core's rule (not by what game-core did with it) */
static bool is_wild_boss(void)
{
    return run.battle_type == BATTLE_TYPE_WILD && run.n_enemy == 1 && expected_segments(&run.enemy[0]) > 0;
}

/* every enemy on the field's party: the segments run-core gave, a shield for each but the last */
static void check_enemies(void)
{
    for (int i = 0; i < run.n_enemy; i++) {
        const pr_pokemon *p = &run.enemy[i].b;
        const int want = expected_segments(&run.enemy[i]);
        CHECK(p->boss_segments == want);
        CHECK(p->is_boss == (want > 0));
        CHECK(p->boss_segment_index == (want > 0 ? want - 1 : 0));
    }
}

static void set_moves(gc_mon *m, int move)
{
    gc_mon_set_move(m, 0, move);
    m->b.n_moves = 1;
    m->b.pp_used[0] = 0;
}

/* The first hit on the boss in the field: Strength, with the HP set so that the hit is between one and three
 * segments. The boss has no ability or passive, so that its stat stages say only what its segments gave.
 * The hit has to stop at the end of the first segment and break one shield; `boost` is the stat stages
 * the boss got for it (false: the hit didn't land, or does nothing to this species: try another seed). */
static bool break_one_segment(gc_mon *foe, int *boost)
{
    const char *why = "";
    gc_mon *me = &run.party[run.party_order[0]];
    set_moves(me, MOVE_ID_STRENGTH);
    set_moves(foe, MOVE_ID_SPLASH);
    foe->b.ability = ABILITY_ID_NONE;
    foe->b.passive = ABILITY_ID_NONE;
    const pr_damage_result d = pr_get_attack_damage(&run.bt, &foe->b, &me->b, MOVE_ID_STRENGTH, false, true);
    if (d.cancelled || d.damage < 8) return false;                 /* a Ghost takes nothing from Strength */
    const int segments = foe->b.boss_segments;
    const int segment_hp = (int)(d.damage * 0.65);                 /* the hit is above one segment, below three */
    foe->b.stats[0] = (int32_t)(segments * segment_hp);
    foe->b.hp = foe->b.stats[0];
    gc_choice fight = {GC_CH_FIGHT, 0, -1, 0};
    CHECK(gc_run_choose(&run, &fight, &why));
    drain();
    if (foe->b.hp == foe->b.stats[0]) return false;                /* it missed */
    CHECK(foe->b.hp > 0);                                          /* without shields the hit is bigger than a segment */
    CHECK(foe->b.hp == foe->b.stats[0] - segment_hp);              /* and stops at the end of the first one */
    CHECK(foe->b.boss_segment_index == segments - 2);
    *boost = 0;
    for (int st = 0; st < 7; st++) *boost += foe->b.stat_stages[st];
    return true;
}

/* A wild boss: the count is run-core's, the shield index is one under it, and the first hit breaks one
 * shield and sets off the stat boost that comes with it. */
static bool wild_boss(const char *seed)
{
    if (play_until(seed, is_wild_boss, 60) < 0) return false;
    drain();
    check_enemies();
    gc_mon *foe = &run.enemy[gc_enemy_active_index(&run)];
    CHECK(foe->b.is_boss && foe->b.boss_segments >= 2);
    CHECK(foe->b.boss_segment_index == foe->b.boss_segments - 1);
    CHECK(run.bt.field[2] == &foe->b);
    int boost;
    if (!break_one_segment(foe, &boost)) return false;
    CHECK(boost == 1);                                             /* one random stat, one stage */
    return true;
}

static bool is_trainer_battle(void)
{
    return run.battle_type == BATTLE_TYPE_TRAINER && run.n_enemy >= 1;
}

/* A trainer's boss: the classic rival's fights 5 and 6 are the only ones run-core gives segments (their
 * partyMemberFuncs call setBoss); addEnemyPokemon makes every other trainer's Pokemon with boss = false.
 * game-core still plays the rival's waves as wild stand-ins (fixed battles, card 100), so the fixture runs
 * run-core's rival through the calls gen_trainer makes (the party, then each member's finish), makes the
 * battle Pokemon with gc_mon_new_enemy, and puts the first one in a trainer battle's place. A trainer's
 * Pokemon keeps its shields but gets no stat boost from breaking one (`!hasTrainer`). */
static bool trainer_boss(const char *seed)
{
    if (play_until(seed, is_trainer_battle, 60) < 0) return false;
    drain();
    for (int fight = 5; fight <= 6; fight++) {
        const int wave = fight == 5 ? 145 : 195;
        pr_run_reset_seed(&run.run, wave);
        static pr_trainer_party party;
        pr_trainer_party_ctx tc;
        memset(&tc, 0, sizeof tc);
        tc.enemy.wave = wave;
        tc.enemy.biome = run.biome;
        tc.enemy.battle_type = BATTLE_TYPE_TRAINER;
        tc.enemy.trainer_type = TRAINER_TYPE_RIVAL_5 + (fight - 5);
        tc.enemy.event_shiny_multiplier = 1;
        tc.n_levels = 6;
        for (int i = 0; i < 6; i++) tc.levels[i] = 120 + 10 * i;
        CHECK(pr_run_gen_rival_party(&run.run, &tc, fight, &party) == 0);
        CHECK(party.n == 6);
        static gc_mon made[6];
        for (int e = 0; e < party.n; e++) {
            pr_enemy_gen_ctx ec = tc.enemy;
            ec.species = party.species[e];
            ec.level = tc.levels[e];
            ec.trainer_slot = TRAINER_SLOT_TRAINER;
            CHECK(pr_run_enemy_pokemon_finish(&run.run, &ec, &party.mons[e]) == 0);
            gc_mon_new_enemy(&run, &made[e], party.species[e], tc.levels[e], &party.mons[e], TRAINER_SLOT_TRAINER);
        }
        /* fight 5: the first Pokemon has 2 segments and the last 3; fight 6: 3, 2 and run-core's count for the ace */
        const int ace = pr_run_get_encounter_boss_segments(&run.run, 195, tc.levels[5], party.species[5], true);
        const int want[2][6] = {{2, 0, 0, 0, 0, 3}, {3, 2, 0, 0, 0, ace}};
        for (int e = 0; e < 6; e++) {
            const pr_pokemon *p = &made[e].b;
            CHECK(p->has_trainer);
            CHECK(p->boss_segments == want[fight - 5][e]);
            CHECK(p->is_boss == (want[fight - 5][e] > 0));
            CHECK(p->boss_segment_index == (want[fight - 5][e] > 0 ? want[fight - 5][e] - 1 : 0));
        }
        if (fight == 5) {
            /* in a battle: the first one in the trainer battle's place at its first command */
            const bool on_field = run.enemy[0].b.on_field;
            run.enemy[0] = made[0];
            run.enemy[0].b.on_field = on_field;
            gc_mon *foe = &run.enemy[0];
            CHECK(run.bt.field[2] == &foe->b);
            int boost;
            if (!break_one_segment(foe, &boost)) return false;
            CHECK(boost == 0);
        }
    }
    return true;
}

/* A mystery encounter's boss: its own count (Slumbering Snorlax: getEncounterBossSegments for its level). */
static void mystery_boss(void)
{
    const char *why = "";
    bool ok = false;
    new_run(&run, "boss-mystery");
    /* a party that lives through the start-of-battle Snore */
    for (int i = 0; i < run.n_party; i++) {
        run.party[i].b.level = 60;
        gc_mon_recalc(&run, &run.party[i]);
        run.party[i].b.hp = run.party[i].b.stats[0];
    }
    drain();
    gc_rq_clear(&run);
    gc_int(&run)->rq.waiting = false;
    gc_int(&run)->rq.current.type = RP_NONE;
    run.wave = 21;
    run.screen.mystery_type = GC_ME_SLUMBERING_SNORLAX;
    gc_mystery_setup_screen(&run);
    CHECK(gc_mystery_execute(&run, 0, 0, 0, &why, &ok));
    gc_run_until_screen(&run);
    CHECK(run.screen.kind == GC_SCREEN_COMMAND && run.n_enemy == 1);
    const pr_pokemon *p = &run.enemy[0].b;
    CHECK(p->species == SPECIES_ID_SNORLAX);
    /* the count is the level's, as the encounter's own boss (the differential compares it with upstream for every wave and level) */
    const int want = pr_run_get_encounter_boss_segments(&run.run, run.wave, p->level, p->species, true);
    CHECK(p->is_boss && p->boss_segments == want && p->boss_segment_index == want - 1);
    CHECK(run.bt.field[2] == p);
}

/* what a turn left behind, to compare the run that played it with the one that was loaded and played it */
typedef struct {
    int32_t hp;
    uint8_t index;
    int8_t stages[7];
    double rng[5];
} turn_result;

static turn_result take(void)
{
    const pr_pokemon *e = &run.enemy[0].b;
    turn_result t;
    memset(&t, 0, sizeof t);
    t.hp = e->hp;
    t.index = e->boss_segment_index;
    for (int i = 0; i < 7; i++) t.stages[i] = e->stat_stages[i];
    t.rng[0] = run.run.rng.c;
    t.rng[1] = run.run.rng.s0;
    t.rng[2] = run.run.rng.s1;
    t.rng[3] = run.run.rng.s2;
    t.rng[4] = run.run.rng.n;
    return t;
}

/* Save and load with a segment already broken (hand-set at the save point: the boss at the end of its
 * first segment): the loaded run has the same shield state, and the turn after plays out the same. */
static bool save_in_the_middle(const char *seed)
{
    const char *why = "";
    if (play_until(seed, is_wild_boss, 60) < 0) return false;
    drain();
    gc_mon *foe = &run.enemy[0];
    const int segments = foe->b.boss_segments;
    CHECK(segments >= 2);
    foe->b.boss_segment_index = (uint8_t)(segments - 2);
    foe->b.hp = foe->b.stats[0] - foe->b.stats[0] / segments;
    CHECK(gc_run_can_save(&run));
    static uint8_t buf[GC_SAVE_MAX];
    const size_t len = gc_run_save(&run, buf, sizeof buf);
    CHECK(len > 0);
    const pr_pokemon saved = run.enemy[0].b;

    /* the turn on the run that was saved ... */
    gc_choice fight = {GC_CH_FIGHT, 0, -1, 0};
    CHECK(gc_run_choose(&run, &fight, &why));
    drain();
    const turn_result first = take();

    /* ... and on the run that comes back from the buffer (gc_run_load replaces the whole run) */
    CHECK(gc_run_load(&run, buf, len, &why));
    CHECK(!run.load_dropped);
    const pr_pokemon *b = &run.enemy[0].b;
    CHECK(b->is_boss && b->boss_segments == saved.boss_segments && b->boss_segment_index == saved.boss_segment_index);
    CHECK(b->hp == saved.hp && b->stats[0] == saved.stats[0]);
    CHECK(run.bt.field[2] == b);
    CHECK(gc_run_choose(&run, &fight, &why));
    drain();
    const turn_result second = take();
    CHECK(first.hp == second.hp);
    CHECK(first.index == second.index);
    CHECK(!memcmp(first.stages, second.stages, sizeof first.stages));
    CHECK(!memcmp(first.rng, second.rng, sizeof first.rng));
    return true;
}

/* A boss that is caught: the party member is no boss. */
static void caught_boss(void)
{
    const char *why = "";
    CHECK(play_until("boss-caught-0", is_wild_boss, 60) >= 0);
    drain();
    gc_mon *foe = &run.enemy[0];
    CHECK(foe->b.is_boss && foe->b.boss_segments > 0);
    const int n_before = run.n_party;
    run.pokeballs[POKEBALL_TYPE_MASTER_BALL] = 3;
    gc_choice ball = {GC_CH_BALL, POKEBALL_TYPE_MASTER_BALL, 0, 0};
    CHECK(gc_run_choose(&run, &ball, &why));
    drain();
    /* the swap prompt of a full party isn't what this checks: a party with room */
    if (run.screen.kind == GC_SCREEN_SWITCH) { fprintf(stderr, "boss segments: party full at the catch\n"); exit(1); }
    CHECK(run.n_party == n_before + 1);
    const pr_pokemon *c = &run.party[run.n_party - 1].b;
    CHECK(c->is_player && !c->is_boss && c->boss_segments == 0 && c->boss_segment_index == 0);
}

#define SCAN(fn, prefix, tries, want) do { \
    int got = 0; \
    for (int i = 0; i < (tries) && got < (want); i++) { \
        char seed[40]; \
        snprintf(seed, sizeof seed, prefix "%d", i); \
        if (fn(seed)) got++; \
    } \
    printf("  %-20s %d seeds\n", #fn, got); \
    CHECK(got >= (want)); \
} while (0)

/* no argument: every fixture; or one of wild, trainer, mystery, save, caught */
int main(int argc, char **argv)
{
    const char *only = argc > 1 ? argv[1] : NULL;
    if (!only || !strcmp(only, "wild")) SCAN(wild_boss, "boss-wild-", 24, 3);
    if (!only || !strcmp(only, "trainer")) SCAN(trainer_boss, "boss-trainer-", 24, 1);
    if (!only || !strcmp(only, "mystery")) mystery_boss();
    if (!only || !strcmp(only, "save")) SCAN(save_in_the_middle, "boss-save-", 12, 2);
    if (!only || !strcmp(only, "caught")) caught_boss();
    printf("boss segments: %d checks, 0 failures\n", checks);
    return 0;
}
