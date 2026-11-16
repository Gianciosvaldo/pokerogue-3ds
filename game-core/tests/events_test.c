/* Timed events in a run (gc/events.h): what game-core does with the effects the app's source says are open. The schedule (which event is
 * open when) is custom-core's and has its own tests; here the source is a plain struct the test changes, so each effect is read in the
 * place upstream reads it.
 *
 * Sections (run with a name to run one, no argument for all):
 *   shiny : the enemy's shiny roll. The shiny test is (E ^ F) < threshold with E from the trainer and secret ids and F from the enemy's own
 *           id, so a run whose ids are chosen to put E ^ F just below or just above 64 * multiplier shows the multiplier exactly; the
 *           expectation is upstream's threshold (BASE_SHINY_CHANCE 64 x getShinyEncounterMultiplier()), not this port's output.
 *   luck  : the party luck value (getPartyLuckValue): min(getEventLuckBoost() + clamp(sum of the party's luck, 0, 14), 14). */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "events FAIL line %d: %s\n", __LINE__, #x); exit(1); } checks++; } while (0)
#define CHECK_EQ(a, b) do { long long va_ = (long long)(a), vb_ = (long long)(b); if (va_ != vb_) { \
    fprintf(stderr, "events FAIL line %d: %s == %s (%lld vs %lld)\n", __LINE__, #a, #b, va_, vb_); exit(1); } checks++; } while (0)

/* the app's source, here a struct the test sets: the effects "open now" */
static gc_event_effects open_now;
static int asked;
static void fake_effects(void *user, gc_event_effects *out)
{
    (void)user;
    asked++;
    *out = open_now;
}
static const gc_event_source SRC = {NULL, fake_effects};

static gc_run run;

/* wave 1 of a seeded Classic run, wild: the first enemy's id and shiny flag. src NULL = no source installed. */
static void wave1(const gc_event_source *src, int trainer_id, int secret_id, uint32_t *id, bool *shiny)
{
    static const char seed[] = "events-shiny";
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    run.trainer_id = trainer_id;
    run.secret_id = secret_id;
    gc_run_set_events(&run, src);
    gc_starter team[3];
    memset(team, 0, sizeof team);
    const int species[3] = {SPECIES_ID_BULBASAUR, SPECIES_ID_CHARMANDER, SPECIES_ID_SQUIRTLE};
    for (int i = 0; i < 3; i++) {
        team[i].species = (uint16_t)species[i];
        team[i].tera_type = -1;
        for (int k = 0; k < 6; k++) team[i].ivs[k] = 15;
    }
    const char *why = "";
    CHECK(gc_run_set_starters(&run, team, 3, &why));
    CHECK(run.battle_type == BATTLE_TYPE_WILD && run.n_enemy >= 1);
    *id = run.enemy[0].b.id;
    *shiny = run.enemy[0].b.shiny;
}

static unsigned fold(uint32_t id) { return (id >> 16) ^ (id & 0xffffu); }

/* The enemy shiny roll is decided by (E ^ F) < threshold, so for the enemy this seed produces: E chosen to make E ^ F equal `dist`. */
static bool shiny_with(const gc_event_source *src, unsigned dist, uint32_t id0)
{
    uint32_t id;
    bool shiny;
    const unsigned e = fold(id0) ^ dist;
    wave1(src, (int)e, 0, &id, &shiny);
    CHECK_EQ(id, id0);   /* the ids do not move the enemy: only the verdict does */
    return shiny;
}

static void shiny_section(void)
{
    uint32_t id0;
    bool s0;
    memset(&open_now, 0, sizeof open_now);
    wave1(NULL, 0, 0, &id0, &s0);

    /* no source: the base threshold, 64. E ^ F = 63 is shiny, 64 is not. */
    CHECK(shiny_with(NULL, 63, id0));
    CHECK(!shiny_with(NULL, 64, id0));
    /* a source with nothing open is the same */
    asked = 0;
    CHECK(shiny_with(&SRC, 63, id0));
    CHECK(!shiny_with(&SRC, 64, id0));
    CHECK(asked > 0);   /* it was asked at the roll */

    /* x2: 128 */
    open_now.shiny_encounter_mult = 2;
    CHECK(shiny_with(&SRC, 64, id0));
    CHECK(shiny_with(&SRC, 127, id0));
    CHECK(!shiny_with(&SRC, 128, id0));
    /* x3: 192 */
    open_now.shiny_encounter_mult = 3;
    CHECK(shiny_with(&SRC, 191, id0));
    CHECK(!shiny_with(&SRC, 192, id0));
    /* x10, the largest an event may ask */
    open_now.shiny_encounter_mult = 10;
    CHECK(shiny_with(&SRC, 639, id0));
    CHECK(!shiny_with(&SRC, 640, id0));

    /* an event that does not touch the shiny odds does not change them, whatever else it carries */
    memset(&open_now, 0, sizeof open_now);
    open_now.shiny_catch_mult = 3;
    open_now.candy_mult = 4;
    open_now.luck_boost = 1;
    open_now.upgrade_vouchers = 1;
    CHECK(!shiny_with(&SRC, 64, id0));
    CHECK(shiny_with(&SRC, 63, id0));

    /* the trainer chance is for a trainer's Pokemon only: a wild Pokemon stays at the base odds (x1) however large it is */
    memset(&open_now, 0, sizeof open_now);
    open_now.trainer_shiny_chance = 6554;
    CHECK(!shiny_with(&SRC, 64, id0));
    CHECK(!shiny_with(&SRC, 1000, id0));

    /* the source is asked again at every roll: a window that closes is not remembered */
    memset(&open_now, 0, sizeof open_now);
    open_now.shiny_encounter_mult = 2;
    CHECK(shiny_with(&SRC, 100, id0));
    open_now.shiny_encounter_mult = 0;
    CHECK(!shiny_with(&SRC, 100, id0));
    open_now.shiny_encounter_mult = 2;
    CHECK(shiny_with(&SRC, 100, id0));

    /* removing the source (NULL) removes the effect */
    CHECK(!shiny_with(NULL, 100, id0));
    gc_run_set_events(&run, &SRC);
    gc_run_set_events(&run, NULL);
    pr_enemy_gen_ctx ec;
    gc_enemy_ctx(&run, &ec);
    CHECK(!ec.event_active);

    /* the context itself, for the cases a wave-1 wild fight cannot show */
    gc_run_set_events(&run, &SRC);
    memset(&open_now, 0, sizeof open_now);
    open_now.trainer_shiny_chance = 6554;
    gc_enemy_ctx(&run, &ec);
    CHECK(ec.event_active);
    CHECK_EQ(ec.event_trainer_shiny_chance, 6554);
    CHECK_EQ(ec.event_shiny_multiplier, 1);   /* an unset multiplier is x1, not x0 */
    memset(&open_now, 0, sizeof open_now);
    open_now.shiny_encounter_mult = 2;
    gc_enemy_ctx(&run, &ec);
    CHECK(ec.event_active);
    CHECK_EQ(ec.event_shiny_multiplier, 2);
    CHECK_EQ(ec.event_trainer_shiny_chance, 0);
    memset(&open_now, 0, sizeof open_now);
    gc_enemy_ctx(&run, &ec);
    CHECK(!ec.event_active);   /* nothing open: run-core sees no event at all */
    CHECK_EQ(ec.event_shiny_multiplier, 1);

    /* a source that asks for more luck than the game's cap is held at the cap */
    open_now.luck_boost = 200;
    CHECK_EQ(gc_event_effects_now(&run).luck_boost, 14);
}


/* a party of three whose luck sums to `sum`, on a seeded Classic run with the source installed */
static int party_luck(const gc_event_source *src, int sum)
{
    static const char seed[] = "events-luck";
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    gc_run_set_events(&run, src);
    gc_starter team[3];
    memset(team, 0, sizeof team);
    const int species[3] = {SPECIES_ID_BULBASAUR, SPECIES_ID_CHARMANDER, SPECIES_ID_SQUIRTLE};
    for (int i = 0; i < 3; i++) {
        team[i].species = (uint16_t)species[i];
        team[i].tera_type = -1;
        for (int k = 0; k < 6; k++) team[i].ivs[k] = 15;
    }
    const char *why = "";
    CHECK(gc_run_set_starters(&run, team, 3, &why));
    CHECK_EQ(run.n_party, 3);
    for (int i = 0; i < 3; i++) run.party[i].luck = (int8_t)(sum > 0 && i == 0 ? sum : 0);
    return gc_party_luck(&run);
}

static void luck_section(void)
{
    memset(&open_now, 0, sizeof open_now);
    CHECK_EQ(party_luck(NULL, 0), 0);
    CHECK_EQ(party_luck(NULL, 5), 5);
    CHECK_EQ(party_luck(&SRC, 5), 5);                 /* a source with nothing open */
    open_now.luck_boost = 1;
    CHECK_EQ(party_luck(&SRC, 0), 1);                 /* +1 on an unlucky party */
    CHECK_EQ(party_luck(&SRC, 5), 6);
    CHECK_EQ(party_luck(&SRC, 13), 14);
    CHECK_EQ(party_luck(&SRC, 14), 14);               /* the cap of 14 holds */
    open_now.luck_boost = 3;
    CHECK_EQ(party_luck(&SRC, 12), 14);
    CHECK_EQ(party_luck(&SRC, 2), 5);
    open_now.luck_boost = 200;                        /* a wrong source is held at the cap */
    CHECK_EQ(party_luck(&SRC, 0), 14);
    /* the shop's luck is the same value: asked when the options are made, so an hour that closes is not remembered */
    open_now.luck_boost = 1;
    CHECK_EQ(party_luck(&SRC, 4), 5);
    open_now.luck_boost = 0;
    CHECK_EQ(gc_party_luck(&run), 4);
    CHECK_EQ(party_luck(NULL, 4), 4);
}

int main(int argc, char **argv)
{
    const char *only = argc > 1 ? argv[1] : NULL;
    if (!only || !strcmp(only, "shiny")) shiny_section();
    if (!only || !strcmp(only, "luck")) luck_section();
    printf("events: %d checks, 0 failures\n", checks);
    return 0;
}
