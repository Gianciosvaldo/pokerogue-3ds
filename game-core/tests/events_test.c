/* Timed events in a run (gc/events.h): what game-core does with the effects the app's source says are open. The schedule (which event is
 * open when) is custom-core's and has its own tests; here the source is a plain struct the test changes, so each effect is read in the
 * place upstream reads it.
 *
 * Sections (run with a name to run one, no argument for all):
 *   shiny : the enemy's shiny roll. The shiny test is (E ^ F) < threshold with E from the trainer and secret ids and F from the enemy's own
 *           id, so a run whose ids are chosen to put E ^ F just below or just above 64 * multiplier shows the multiplier exactly; the
 *           expectation is upstream's threshold (BASE_SHINY_CHANCE 64 x getShinyEncounterMultiplier()), not this port's output.
 *   candy : the Classic candy friendship multiplier (getClassicFriendshipMultiplier(): 3, or an event's when higher; Classic only).
 *   rewards: the items an event gives after a Classic wave (getFixedBattleEventRewards): which phases VictoryPhase queues, in which order, and
 *           the tier ball a full stack turns into. Expected tiers are read from upstream's init-modifier-pools.ts (CANDY_JAR, EXP_CHARM
 *           ULTRA; CATCHING_CHARM, ABILITY_CHARM, SUPER_EXP_CHARM, MEGA_BRACELET, DYNAMAX_BAND ROGUE; SHINY_CHARM, HEALING_CHARM MASTER) and
 *           max stacks from modifier.ts / modifier-type.ts, not from the port's tables.
 *   voucher: the voucher a defeated boss gives (TrainerVictoryPhase): upgraded while an event says getUpgradeUnlockedVouchers().
 *   delibirdy: Delibirdy Day: the bonus charm of getDelibirdyBuff after each of Delibird-y's three gifts, and its tier moved GREAT -> COMMON.
 *   snow  : Snowy Town: the Town's weather pool replaced by {SNOW: 1}, no other biome touched.
 *   fusion: Fusion Day also changes the shop: DNA Splicers' weights per tier (init-modifier-pools.ts), asked of reward-core's own weight function.
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

    /* luck-boosted species (Lucky Species Hour): each party member of a listed species adds 1 to the sum (getPartyLuckValue:
     * p.getLuck() + (eventSpecies.includes(p.species.speciesId) ? 1 : 0)); the party is Bulbasaur (1), Charmander (4), Squirtle (7) */
    memset(&open_now, 0, sizeof open_now);
    open_now.luck_species[0] = 133;                                   /* Eevee: nobody in the party */
    CHECK_EQ(party_luck(&SRC, 0), 0);
    open_now.luck_species[0] = SPECIES_ID_BULBASAUR;
    CHECK_EQ(party_luck(&SRC, 0), 1);
    CHECK_EQ(party_luck(&SRC, 5), 6);                                 /* on top of the members' own luck */
    CHECK_EQ(party_luck(NULL, 0), 0);                                 /* no source: nothing */
    open_now.luck_species[1] = SPECIES_ID_CHARMANDER;
    CHECK_EQ(party_luck(&SRC, 0), 2);
    open_now.luck_species[2] = SPECIES_ID_SQUIRTLE;
    CHECK_EQ(party_luck(&SRC, 0), 3);
    open_now.luck_species[3] = 133;                                   /* a species after them changes nothing */
    CHECK_EQ(party_luck(&SRC, 0), 3);
    open_now.luck_boost = 1;                                          /* with the hour's flat boost: 3 + 1 */
    CHECK_EQ(party_luck(&SRC, 0), 4);
    CHECK_EQ(party_luck(&SRC, 12), 14);                               /* the sum is capped at 14 first, then the boost, then 14 again */
    open_now.luck_boost = 0;
    CHECK_EQ(party_luck(&SRC, 13), 14);                               /* 13 + 3 = 16, capped */
    /* the list's last slot (the twelfth) is read too */
    memset(&open_now, 0, sizeof open_now);
    for (int i = 0; i < GC_EVENT_SPECIES_MAX - 1; i++) open_now.luck_species[i] = (uint16_t)(133 + i);
    open_now.luck_species[GC_EVENT_SPECIES_MAX - 1] = SPECIES_ID_SQUIRTLE;
    CHECK_EQ(party_luck(&SRC, 0), 1);
    /* a member counts once however often its species is listed (the list has no duplicates upstream, but a wrong source must not double it) */
    memset(&open_now, 0, sizeof open_now);
    open_now.luck_species[0] = SPECIES_ID_BULBASAUR;
    open_now.luck_species[1] = SPECIES_ID_BULBASAUR;
    CHECK_EQ(party_luck(&SRC, 0), 1);
    /* a member that cannot fight (fainted) adds neither its luck nor the species' */
    CHECK_EQ(party_luck(&SRC, 5), 6);
    run.party[0].b.hp = 0;
    CHECK_EQ(gc_party_luck(&run), 0);
    /* asked at each call: the hour closing is seen by the next one */
    memset(&open_now, 0, sizeof open_now);
    CHECK_EQ(party_luck(&SRC, 0), 0);
}

/* the starter bars' friendship points a Pokemon's gain of 3 feeds, in a run of `mode` with the source installed. A fused Pokemon feeds each root's bar. */
static int friendship_points(int mode, const gc_event_source *src, bool fused, int *second_root)
{
    static const char seed[] = "events-candy";
    static gc_system sys;
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, mode, seed, 7);
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
    run.sys = &sys;   /* after the starters, as catch_test does: with system data the starters must be unlocked */
    gc_mon *m = &run.party[0];
    CHECK_EQ(m->b.species, SPECIES_ID_BULBASAUR);
    if (fused) m->b.fusion_species = SPECIES_ID_SQUIRTLE;
    const uint16_t r1 = gc_species_root_id(m->b.species);
    const uint16_t r2 = fused ? gc_species_root_id(m->b.fusion_species) : r1;
    CHECK(r1 != r2 || !fused);
    const int b1 = sys.starters[r1].friendship, b2 = sys.starters[r2].friendship;
    gc_system_add_friendship(&sys, &run, m, 3, false);
    if (second_root) *second_root = sys.starters[r2].friendship - b2;
    return sys.starters[r1].friendship - b1;
}

static void candy_section(void)
{
    memset(&open_now, 0, sizeof open_now);
    CHECK_EQ(friendship_points(GAME_MODES_CLASSIC, NULL, false, NULL), 9);        /* 3 x the non-event 3 */
    CHECK_EQ(friendship_points(GAME_MODES_CLASSIC, &SRC, false, NULL), 9);        /* nothing open */
    open_now.candy_mult = 4;
    CHECK_EQ(friendship_points(GAME_MODES_CLASSIC, &SRC, false, NULL), 12);       /* Sweet Tooth: x4 */
    open_now.candy_mult = 10;
    CHECK_EQ(friendship_points(GAME_MODES_CLASSIC, &SRC, false, NULL), 30);
    open_now.candy_mult = 3;
    CHECK_EQ(friendship_points(GAME_MODES_CLASSIC, &SRC, false, NULL), 9);        /* equal to the non-event value */
    open_now.candy_mult = 2;
    CHECK_EQ(friendship_points(GAME_MODES_CLASSIC, &SRC, false, NULL), 9);        /* an event never lowers it */
    open_now.candy_mult = 1;
    CHECK_EQ(friendship_points(GAME_MODES_CLASSIC, &SRC, false, NULL), 9);
    /* the other modes have no Classic multiplier: x1 whatever the event says */
    open_now.candy_mult = 4;
    CHECK_EQ(friendship_points(GAME_MODES_ENDLESS, NULL, false, NULL), 3);
    CHECK_EQ(friendship_points(GAME_MODES_ENDLESS, &SRC, false, NULL), 3);
    /* a fusion feeds each root with half: floor(3 x mult / 2) */
    int second = 0;
    memset(&open_now, 0, sizeof open_now);
    CHECK_EQ(friendship_points(GAME_MODES_CLASSIC, &SRC, true, &second), 4);      /* floor(9 / 2) */
    CHECK_EQ(second, 4);
    open_now.candy_mult = 4;
    CHECK_EQ(friendship_points(GAME_MODES_CLASSIC, &SRC, true, &second), 6);      /* 12 / 2 */
    CHECK_EQ(second, 6);
    /* Fusion Day: a fusion's candy is divided by 1.5, not 2, in any game mode (upstream's areFusionsBoosted() is not Classic-only);
     * Math.floor(friendship * (mult / 1.5)). A Pokemon that is not a fusion is not changed. */
    memset(&open_now, 0, sizeof open_now);
    open_now.boost_fusions = 1;
    CHECK_EQ(friendship_points(GAME_MODES_CLASSIC, &SRC, true, &second), 6);      /* 3 * (3 / 1.5) */
    CHECK_EQ(second, 6);
    CHECK_EQ(friendship_points(GAME_MODES_CLASSIC, &SRC, false, &second), 9);
    CHECK_EQ(friendship_points(GAME_MODES_ENDLESS, &SRC, true, &second), 2);      /* 3 * (1 / 1.5) = 2 where unboosted gives floor(3 / 2) = 1 */
    CHECK_EQ(second, 2);
    CHECK_EQ(friendship_points(GAME_MODES_ENDLESS, &SRC, false, &second), 3);
    open_now.candy_mult = 4;                                                      /* with Sweet Tooth: 3 * (4 / 1.5) = 8 */
    CHECK_EQ(friendship_points(GAME_MODES_CLASSIC, &SRC, true, &second), 8);
    CHECK_EQ(second, 8);
    CHECK_EQ(friendship_points(GAME_MODES_CLASSIC, &SRC, false, &second), 12);
    open_now.candy_mult = 10;                                                     /* 3 * (10 / 1.5) = 20 */
    CHECK_EQ(friendship_points(GAME_MODES_CLASSIC, &SRC, true, &second), 20);
    open_now.boost_fusions = 0;
    CHECK_EQ(friendship_points(GAME_MODES_CLASSIC, &SRC, true, &second), 15);     /* the boost off again: floor(30 / 2) */
    /* no run behind the gain (a caller with none): there is no source to ask, no crash, and the gain is what it was before events (a
     * missing run counts as Classic's x3) */
    static gc_system sys2;
    gc_system_init_defaults(&sys2);
    const int before = sys2.starters[SPECIES_ID_BULBASAUR].friendship;
    gc_mon m;
    memset(&m, 0, sizeof m);
    m.b.species = SPECIES_ID_BULBASAUR;
    gc_system_add_friendship(&sys2, NULL, &m, 3, false);
    CHECK_EQ(sys2.starters[SPECIES_ID_BULBASAUR].friendship - before, 9);
}

/* the phases a won wave queues after its BattleEndPhase, as text: "egg,LOCK_CAPSULE,SHINY_CHARM,shop". `have` is a list of {key, stack}. */
typedef struct { const char *key; int stack; int tier; } have_t;
static const char *wave_queue(int mode, int wave, const gc_event_source *src, const have_t *have, int n_have, bool wave_asked_check)
{
    static char out[512];
    static const char seed[] = "events-rewards";
    memset(&run, 0, sizeof run);
    gc_run_new(&run, mode, seed, 7);
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
    for (int i = 0; i < n_have; i++) {
        gc_item_add(&run, have[i].key, -1, 0, have[i].stack);
        if (have[i].tier) run.items[run.n_items - 1].type_tier = (uint8_t)have[i].tier;
    }
    run.wave = wave;
    gc_rq_reset(&gc_int(&run)->rq);
    const int before = asked;
    gc_queue_battle_end(&run);
    if (wave_asked_check) CHECK(asked > before);   /* the source is asked at the moment of the victory, never cached */
    gc_rqueue *q = &gc_int(&run)->rq;
    out[0] = 0;
    for (int l = 0; l < q->n_levels; l++)
        for (int k = 0; k < q->n[l]; k++) {
            const gc_rphase *ph = &q->q[l][k];
            const char *t = ph->type == RP_MODIFIER_REWARD ? ph->key : ph->type == RP_SELECT_MODIFIER ? "shop" : ph->type == RP_EGG_LAPSE ? "egg"
                          : ph->type == RP_BATTLE_END ? "end" : ph->type == RP_TRAINER_VICTORY ? "trainer" : ph->type == RP_NEW_BATTLE ? "new" : ph->type == RP_GAME_OVER ? "over" : ph->type == RP_SELECT_BIOME ? "biome" : "?";
            if (out[0]) strcat(out, ",");
            strcat(out, t);
        }
    return out;
}

static gc_event_effects charm_hour(void)
{
    gc_event_effects fx;
    memset(&fx, 0, sizeof fx);
    fx.rewards[0] = (gc_event_reward){8, "SHINY_CHARM"};
    fx.rewards[1] = (gc_event_reward){8, "ABILITY_CHARM"};
    fx.rewards[2] = (gc_event_reward){8, "CATCHING_CHARM"};
    fx.rewards[3] = (gc_event_reward){25, "SHINY_CHARM"};
    return fx;
}

static void rewards_section(void)
{
    memset(&open_now, 0, sizeof open_now);
    /* no event: the wave's queue is upstream's own (a wave not a multiple of 10: the egg lapse, then the shop) */
    CHECK(!strcmp(wave_queue(GAME_MODES_CLASSIC, 8, NULL, NULL, 0, false), "end,egg,shop,new"));
    CHECK(!strcmp(wave_queue(GAME_MODES_CLASSIC, 8, &SRC, NULL, 0, true), "end,egg,shop,new"));
    open_now = charm_hour();
    /* the rewards come after the egg lapse and before the shop, in the list's order; other waves get none */
    CHECK(!strcmp(wave_queue(GAME_MODES_CLASSIC, 8, &SRC, NULL, 0, true), "end,egg,SHINY_CHARM,ABILITY_CHARM,CATCHING_CHARM,shop,new"));
    CHECK(!strcmp(wave_queue(GAME_MODES_CLASSIC, 25, &SRC, NULL, 0, true), "end,egg,SHINY_CHARM,shop,new"));
    CHECK(!strcmp(wave_queue(GAME_MODES_CLASSIC, 7, &SRC, NULL, 0, true), "end,egg,shop,new"));
    CHECK(!strcmp(wave_queue(GAME_MODES_CLASSIC, 9, &SRC, NULL, 0, true), "end,egg,shop,new"));
    /* Classic only, and never the last wave */
    CHECK(!strcmp(wave_queue(GAME_MODES_ENDLESS, 8, &SRC, NULL, 0, false), "end,egg,shop,new"));
    CHECK(!strcmp(wave_queue(GAME_MODES_SPLICED_ENDLESS, 8, &SRC, NULL, 0, false), "end,egg,shop,new"));
    memset(&open_now, 0, sizeof open_now);
    open_now.rewards[0] = (gc_event_reward){200, "SHINY_CHARM"};
    CHECK(!strcmp(wave_queue(GAME_MODES_CLASSIC, 200, &SRC, NULL, 0, false), "end,over"));   /* the final wave: only the BattleEndPhase */
    /* the Lock Capsule of wave 165 comes first (upstream's switch is before the event loop) */
    open_now.rewards[0] = (gc_event_reward){165, "EXP_CHARM"};
    CHECK(!strcmp(wave_queue(GAME_MODES_CLASSIC, 165, &SRC, NULL, 0, true), "end,egg,LOCK_CAPSULE,EXP_CHARM,shop,new"));
    /* Waiting Room: both items on wave 115, in order */
    memset(&open_now, 0, sizeof open_now);
    open_now.rewards[0] = (gc_event_reward){115, "MEGA_BRACELET"};
    open_now.rewards[1] = (gc_event_reward){115, "DYNAMAX_BAND"};
    CHECK(!strcmp(wave_queue(GAME_MODES_CLASSIC, 115, &SRC, NULL, 0, true), "end,egg,MEGA_BRACELET,DYNAMAX_BAND,shop,new"));
    /* a wave that is a multiple of 10 has no shop: the event's reward still comes first, then the wave's own charm and ball */
    memset(&open_now, 0, sizeof open_now);
    open_now.rewards[0] = (gc_event_reward){100, "CANDY_JAR"};
    CHECK(!strcmp(wave_queue(GAME_MODES_CLASSIC, 100, &SRC, NULL, 0, true), "end,egg,CANDY_JAR,EXP_CHARM,GOLDEN_POKEBALL,biome,new"));
    /* a list ends at its first empty slot, a NULL or unknown key is skipped */
    memset(&open_now, 0, sizeof open_now);
    open_now.rewards[0] = (gc_event_reward){8, "SHINY_CHARM"};
    open_now.rewards[1] = (gc_event_reward){0, "ABILITY_CHARM"};
    open_now.rewards[2] = (gc_event_reward){8, "CATCHING_CHARM"};
    CHECK(!strcmp(wave_queue(GAME_MODES_CLASSIC, 8, &SRC, NULL, 0, true), "end,egg,SHINY_CHARM,shop,new"));
    memset(&open_now, 0, sizeof open_now);
    open_now.rewards[0] = (gc_event_reward){8, NULL};
    open_now.rewards[1] = (gc_event_reward){8, "NOT_AN_ITEM"};
    open_now.rewards[2] = (gc_event_reward){8, "CANDY_JAR"};
    CHECK(!strcmp(wave_queue(GAME_MODES_CLASSIC, 8, &SRC, NULL, 0, true), "end,egg,CANDY_JAR,shop,new"));

    /* a full stack: the reward is the ball of the item's tier (existingItem.getStackCount() + 1 > getMaxStackCount()) */
    static const struct { const char *key; int max; const char *ball; } full[] = {
        {"SHINY_CHARM", 4, "MASTER_BALL"},     {"HEALING_CHARM", 5, "MASTER_BALL"},
        {"ABILITY_CHARM", 4, "ROGUE_BALL"},    {"CATCHING_CHARM", 3, "ROGUE_BALL"},  {"SUPER_EXP_CHARM", 30, "ROGUE_BALL"},
        {"MEGA_BRACELET", 1, "ROGUE_BALL"},    {"DYNAMAX_BAND", 1, "ROGUE_BALL"},
        {"CANDY_JAR", 99, "ULTRA_BALL"},       {"EXP_CHARM", 99, "ULTRA_BALL"},
    };
    for (unsigned i = 0; i < sizeof full / sizeof *full; i++) {
        memset(&open_now, 0, sizeof open_now);
        open_now.rewards[0] = (gc_event_reward){8, full[i].key};
        char want[128];
        have_t h = {full[i].key, full[i].max, 0};
        snprintf(want, sizeof want, "end,egg,%s,shop,new", full[i].ball);
        CHECK(!strcmp(wave_queue(GAME_MODES_CLASSIC, 8, &SRC, &h, 1, true), want));
        if (full[i].max > 1) {   /* one below the maximum: the item itself */
            h.stack = full[i].max - 1;
            snprintf(want, sizeof want, "end,egg,%s,shop,new", full[i].key);
            CHECK(!strcmp(wave_queue(GAME_MODES_CLASSIC, 8, &SRC, &h, 1, true), want));
        }
    }
    /* an item that is no modifier (Rare Candy, a consumable) has no stack to fill: given as it is */
    memset(&open_now, 0, sizeof open_now);
    open_now.rewards[0] = (gc_event_reward){8, "RARE_CANDY"};
    CHECK(!strcmp(wave_queue(GAME_MODES_CLASSIC, 8, &SRC, NULL, 0, true), "end,egg,RARE_CANDY,shop,new"));
    /* an item with no tier in any pool that is full stops the loop (upstream: console.warn, break): the rewards after it are lost */
    int no_tier = -1;
    for (int it = 0; it < PR_ITEM_COUNT_ && no_tier < 0; it++) {
        if (!(pr_items[it].flags & PR_ITEMF_PERSISTENT) || pr_items[it].max_stack < 1 || pr_items[it].max_stack > 5) continue;
        bool in_pool = false;
        for (int t = 0; t < 6; t++)
            for (int k = 0; k < pr_player_pool[t].n; k++) in_pool |= pr_player_pool_entries[pr_player_pool[t].start + k].item == it;
        if (!in_pool) no_tier = it;
    }
    CHECK(no_tier >= 0);
    if (no_tier >= 0) {
        memset(&open_now, 0, sizeof open_now);
        open_now.rewards[0] = (gc_event_reward){8, pr_item_keys[no_tier]};
        open_now.rewards[1] = (gc_event_reward){8, "CANDY_JAR"};
        have_t fullone = {pr_item_keys[no_tier], pr_items[no_tier].max_stack, 0};
        CHECK(!strcmp(wave_queue(GAME_MODES_CLASSIC, 8, &SRC, &fullone, 1, true), "end,egg,shop,new"));
        fullone.stack = pr_items[no_tier].max_stack - 1 > 0 ? pr_items[no_tier].max_stack - 1 : 1;
        if (pr_items[no_tier].max_stack > 1) {
            char want[128];
            snprintf(want, sizeof want, "end,egg,%s,CANDY_JAR,shop,new", pr_item_keys[no_tier]);
            CHECK(!strcmp(wave_queue(GAME_MODES_CLASSIC, 8, &SRC, &fullone, 1, true), want));
        }
    }
    /* another item's stack does not matter, and an item the player lacks is given */
    memset(&open_now, 0, sizeof open_now);
    open_now.rewards[0] = (gc_event_reward){8, "SHINY_CHARM"};
    have_t other = {"ABILITY_CHARM", 4, 0};
    CHECK(!strcmp(wave_queue(GAME_MODES_CLASSIC, 8, &SRC, &other, 1, true), "end,egg,SHINY_CHARM,shop,new"));
    /* the stack is read when the phases are queued: two rewards of one wave both see 3 of 4 */
    open_now.rewards[1] = (gc_event_reward){8, "SHINY_CHARM"};
    have_t three = {"SHINY_CHARM", 3, 0};
    CHECK(!strcmp(wave_queue(GAME_MODES_CLASSIC, 8, &SRC, &three, 1, true), "end,egg,SHINY_CHARM,SHINY_CHARM,shop,new"));
    /* the tier the held item's type already carries wins over the pool's: a Shiny Charm from a ROGUE roll (tier 3, stored +1) */
    memset(&open_now, 0, sizeof open_now);
    open_now.rewards[0] = (gc_event_reward){8, "SHINY_CHARM"};
    have_t carried = {"SHINY_CHARM", 4, 4};
    CHECK(!strcmp(wave_queue(GAME_MODES_CLASSIC, 8, &SRC, &carried, 1, true), "end,egg,ROGUE_BALL,shop,new"));
}

/* a boss defeated through the real TrainerVictoryPhase: the voucher reward it queues ("" when none) */
static const char *voucher_reward(int tt, int unlocked_before, const gc_event_source *src, char *out, size_t n)
{
    static gc_system sys;
    gc_system_init_defaults(&sys);
    sys.voucher_unlocks[tt] = (uint8_t)unlocked_before;
    memset(&run, 0, sizeof run);
    run.sys = &sys;
    run.trainer_type = tt;
    run.run.mode = GAME_MODES_CLASSIC;
    gc_run_set_events(&run, src);
    gc_internal *in = gc_int(&run);
    gc_rq_reset(&in->rq);
    gc_rphase start = gc_rp(RP_TRAINER_VICTORY, 0, 0, 0);
    gc_phase_start(&run, &start);
    gc_event ev;
    while (gc_run_next_event(&run, &ev)) {}
    out[0] = 0;
    gc_rphase q;
    while (gc_rq_next(&in->rq, &q))
        if (q.type == RP_MODIFIER_REWARD && q.key && !strncmp(q.key, "VOUCHER", 7)) snprintf(out, n, "%s", q.key);
    return out;
}

static void voucher_section(void)
{
    /* TrainerVictoryPhase's table by VoucherType (REGULAR, PLUS, PREMIUM, GOLDEN): the plain one, and the one an event that upgrades the
     * unlocked vouchers gives (getUpgradeUnlockedVouchers) */
    static const char *const plain[4] = {"VOUCHER", "VOUCHER", "VOUCHER_PLUS", "VOUCHER_PREMIUM"};
    static const char *const upgraded[4] = {"VOUCHER_PLUS", "VOUCHER_PLUS", "VOUCHER_PLUS", "VOUCHER_PREMIUM"};
    int bosses = 0, by_type[4] = {0, 0, 0, 0};
    char got[48];
    for (int tt = 0; tt < pr_reward_trainer_count && tt < 512; tt++) {
        const pr_reward_trainer *t = &pr_reward_trainers[tt];
        if (!t->has_config || !t->is_boss || t->voucher_type < 0 || t->voucher_type > 3) continue;
        bosses++;
        by_type[t->voucher_type]++;
        memset(&open_now, 0, sizeof open_now);
        CHECK(!strcmp(voucher_reward(tt, 1, NULL, got, sizeof got), plain[t->voucher_type]));       /* no source */
        CHECK(!strcmp(voucher_reward(tt, 1, &SRC, got, sizeof got), plain[t->voucher_type]));       /* nothing open */
        open_now.shiny_encounter_mult = 2;
        open_now.luck_boost = 1;
        CHECK(!strcmp(voucher_reward(tt, 1, &SRC, got, sizeof got), plain[t->voucher_type]));       /* other effects change nothing */
        open_now.upgrade_vouchers = 1;
        CHECK(!strcmp(voucher_reward(tt, 1, &SRC, got, sizeof got), upgraded[t->voucher_type]));    /* Voucher Plus Day */
        /* the first win only unlocks the voucher: no reward, with or without the event */
        CHECK(!strcmp(voucher_reward(tt, 0, &SRC, got, sizeof got), ""));
        CHECK(!strcmp(voucher_reward(tt, 0, NULL, got, sizeof got), ""));
    }
    /* every boss of the table (152 at the pin: 136 with a Plus voucher, 16 with a Premium one; none has a Regular or Golden one, so those
     * two rows of the tables are not reachable through a trainer) */
    CHECK_EQ(bosses, 152);
    CHECK_EQ(by_type[1], 136);
    CHECK_EQ(by_type[2], 16);
    CHECK_EQ(by_type[0] + by_type[3], 0);
}

/* ---- Delibird-y (Delibirdy Day): the bonus charm of getDelibirdyBuff, and the tier GREAT -> COMMON ---- */
static gc_system dsys;
static void dstart(const char *seed, int wave, const gc_event_source *src)
{
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    gc_system_init_defaults(&dsys);
    run.sys = &dsys;
    gc_run_set_events(&run, src);
    run.wave = wave;
    run.biome = BIOME_ID_PLAINS;
    run.money = 1000000;
    run.n_party = 2;
    for (int i = 0; i < 2; i++) {
        gc_starter st;
        memset(&st, 0, sizeof st);
        st.species = i ? SPECIES_ID_LAPRAS : SPECIES_ID_PIKACHU;
        st.tera_type = -1;
        for (int k = 0; k < 6; k++) st.ivs[k] = 15;
        st.moves[0] = MOVE_ID_TACKLE;
        gc_mon_new_player(&run, &run.party[i], &st, 30);
        run.party[i].pause_evolutions = true;
        run.party_order[i] = (int8_t)i;
    }
    run.n_items = 0;
    memset(run.items, 0, sizeof run.items);
    run.battle_type = BATTLE_TYPE_WILD;
    gc_rq_reset(&gc_int(&run)->rq);
    run.mystery_type = GC_ME_DELIBIRDY;
}

static void dopen(void)
{
    gc_items_to_battle(&run);
    gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_DELIBIRDY, GC_ME_GREAT, 0));
    gc_run_until_screen(&run);
    CHECK(run.screen.kind == GC_SCREEN_MYSTERY);
}

static void dchoose(int option, int member, int item)
{
    const char *why = "";
    gc_choice ch = {GC_CH_MYSTERY, option, member, item};
    CHECK(gc_run_choose(&run, &ch, &why));
}

static int dheld(const char *key)
{
    int n = 0;
    for (int i = 0; i < run.n_items; i++) if (!strcmp(run.items[i].key, key)) n += run.items[i].stack;
    return n;
}

/* the item names of the "battle:rewardGain" texts, in the order they were said */
static char dsaid[8][32];
static int dn_said;
static void dlisten(void)
{
    gc_event e;
    dn_said = 0;
    while (gc_run_next_event(&run, &e)) {
        if (e.kind != GC_EV_MESSAGE || !e.msg.key || !strstr(e.msg.key, "rewardGain") || dn_said >= 8) continue;
        dsaid[dn_said][0] = 0;
        for (int a = 0; a < e.msg.n_args; a++)
            if (e.msg.args[a].name && !strcmp(e.msg.args[a].name, "modifierName") && e.msg.args[a].s) {
                const char *q = strstr(e.msg.args[a].s, "ModifierType.");
                if (q) snprintf(dsaid[dn_said], sizeof dsaid[0], "%s", q + 13);
            }
        if (strstr(e.msg.key, "rewardGain") && !dsaid[dn_said][0]) snprintf(dsaid[dn_said], sizeof dsaid[0], "?");
        dn_said++;
    }
}

static void delibirdy_section(void)
{
    memset(&open_now, 0, sizeof open_now);
    /* no source, or nothing open: the Amulet Coin and nothing else */
    for (int k = 0; k < 2; k++) {
        dstart("events-deli-0", 40, k ? &SRC : NULL);
        gc_item_add(&run, "LEFTOVERS", -1, run.party[0].b.id, 1);
        dopen();
        dchoose(0, 0, 0);
        CHECK_EQ(dheld("AMULET_COIN"), 1);
        CHECK_EQ(dheld("CATCHING_CHARM") + dheld("SHINY_CHARM") + dheld("HEALING_CHARM"), 0);
        CHECK_EQ(dsys.voucher_counts[GC_VOUCHER_REGULAR], 0);
    }
    /* one candidate: given, and said after the Amulet Coin (both are unshifted in this phase, so they run in the order they were unshifted) */
    open_now.delibirdy_buff[0] = "CATCHING_CHARM";
    dstart("events-deli-1", 40, &SRC);
    gc_item_add(&run, "LEFTOVERS", -1, run.party[0].b.id, 1);
    dopen();
    dchoose(0, 0, 0);
    CHECK_EQ(dheld("AMULET_COIN"), 1);
    CHECK_EQ(dheld("CATCHING_CHARM"), 1);
    dlisten();
    CHECK_EQ(dn_said, 2);
    CHECK(strstr(dsaid[0], "AMULET_COIN") != NULL);
    CHECK(strstr(dsaid[1], "CATCHING_CHARM") != NULL);
    /* the charm at its maximum stack (3) is no candidate: a Voucher instead (one regular egg voucher), the coin as before */
    dstart("events-deli-2", 40, &SRC);
    gc_item_add(&run, "CATCHING_CHARM", -1, 0, 3);
    gc_item_add(&run, "LEFTOVERS", -1, run.party[0].b.id, 1);
    dopen();
    dchoose(0, 0, 0);
    CHECK_EQ(dheld("CATCHING_CHARM"), 3);
    CHECK_EQ(dsys.voucher_counts[GC_VOUCHER_REGULAR], 1);
    CHECK_EQ(dheld("AMULET_COIN"), 1);
    /* one below the maximum is still a candidate */
    dstart("events-deli-2b", 40, &SRC);
    gc_item_add(&run, "CATCHING_CHARM", -1, 0, 2);
    gc_item_add(&run, "LEFTOVERS", -1, run.party[0].b.id, 1);
    dopen();
    dchoose(0, 0, 0);
    CHECK_EQ(dheld("CATCHING_CHARM"), 3);
    CHECK_EQ(dsys.voucher_counts[GC_VOUCHER_REGULAR], 0);
    /* the coin at its maximum (5): the lead gets a Shell Bell as before, and the event's charm still comes */
    dstart("events-deli-3", 40, &SRC);
    gc_item_add(&run, "AMULET_COIN", -1, 0, 5);
    gc_item_add(&run, "LEFTOVERS", -1, run.party[0].b.id, 1);
    dopen();
    dchoose(0, 0, 0);
    CHECK_EQ(dheld("AMULET_COIN"), 5);
    CHECK_EQ(dheld("CATCHING_CHARM"), 1);
    CHECK_EQ(dheld("SHELL_BELL"), 1);
    /* two candidates: exactly one is given, both come up over many seeds, and the same seed always gives the same one */
    open_now.delibirdy_buff[1] = "SHINY_CHARM";
    int catching = 0, shiny = 0;
    for (int n = 0; n < 60; n++) {
        char seed[32];
        snprintf(seed, sizeof seed, "events-deli-pick-%d", n);
        dstart(seed, 40, &SRC);
        gc_item_add(&run, "LEFTOVERS", -1, run.party[0].b.id, 1);
        dopen();
        dchoose(0, 0, 0);
        const int c = dheld("CATCHING_CHARM"), sh = dheld("SHINY_CHARM");
        CHECK_EQ(c + sh, 1);
        catching += c;
        shiny += sh;
        dstart(seed, 40, &SRC);
        gc_item_add(&run, "LEFTOVERS", -1, run.party[0].b.id, 1);
        dopen();
        dchoose(0, 0, 0);
        CHECK_EQ(dheld("CATCHING_CHARM"), c);
    }
    CHECK(catching > 10 && shiny > 10);
    /* a full one is dropped from the candidates: always the other */
    for (int n = 0; n < 20; n++) {
        char seed[32];
        snprintf(seed, sizeof seed, "events-deli-full-%d", n);
        dstart(seed, 40, &SRC);
        gc_item_add(&run, "CATCHING_CHARM", -1, 0, 3);
        gc_item_add(&run, "LEFTOVERS", -1, run.party[0].b.id, 1);
        dopen();
        dchoose(0, 0, 0);
        CHECK_EQ(dheld("SHINY_CHARM"), 1);
        CHECK_EQ(dheld("CATCHING_CHARM"), 3);
    }
    /* all full: a Voucher */
    dstart("events-deli-allfull", 40, &SRC);
    gc_item_add(&run, "CATCHING_CHARM", -1, 0, 3);
    gc_item_add(&run, "SHINY_CHARM", -1, 0, 4);
    gc_item_add(&run, "LEFTOVERS", -1, run.party[0].b.id, 1);
    dopen();
    dchoose(0, 0, 0);
    CHECK_EQ(dsys.voucher_counts[GC_VOUCHER_REGULAR], 1);
    /* the other two options give it as well, after their own gift */
    memset(&open_now, 0, sizeof open_now);
    open_now.delibirdy_buff[0] = "ABILITY_CHARM";
    dstart("events-deli-food", 40, &SRC);
    gc_item_add(&run, "BERRY", BERRY_TYPE_SITRUS, run.party[1].b.id, 1);
    dopen();
    dchoose(1, 1, 0);
    CHECK_EQ(dheld("CANDY_JAR"), 1);
    CHECK_EQ(dheld("ABILITY_CHARM"), 1);
    CHECK_EQ(dheld("BERRY"), 0);                       /* the berry was given */
    dlisten();
    CHECK(dn_said == 2 && strstr(dsaid[0], "CANDY_JAR") && strstr(dsaid[1], "ABILITY_CHARM"));
    dstart("events-deli-item", 40, &SRC);
    gc_item_add(&run, "LEFTOVERS", -1, run.party[0].b.id, 1);
    dopen();
    dchoose(2, 0, 0);
    CHECK_EQ(dheld("HEALING_CHARM"), 1);
    CHECK_EQ(dheld("ABILITY_CHARM"), 1);
    CHECK_EQ(dheld("LEFTOVERS"), 0);
    /* an unknown key is no candidate; the list is still an event's, so with nothing else a Voucher */
    memset(&open_now, 0, sizeof open_now);
    open_now.delibirdy_buff[0] = "NOT_AN_ITEM";
    dstart("events-deli-unknown", 40, &SRC);
    gc_item_add(&run, "LEFTOVERS", -1, run.party[0].b.id, 1);
    dopen();
    dchoose(0, 0, 0);
    CHECK_EQ(dsys.voucher_counts[GC_VOUCHER_REGULAR], 1);

    /* the tier: while the event says COMMON, Delibird-y is a candidate of the Common draw and no longer of the Great draw. The same seed gives the
     * same tier roll with and without the event, so a draw that gives Delibird-y only with the event came up Common, and one that gives it only
     * without came up Great. Both happen, and the history still holds its own tier, GREAT (SeenEncounterData(encounter.encounterTier)) */
    int gained = 0, lost = 0, both = 0;
    for (int n = 0; n < 1500; n++) {
        char seed[32];
        snprintf(seed, sizeof seed, "events-deli-tier-%d", n);
        bool got[2] = {false, false};
        int roll = -1;
        for (int ev = 0; ev < 2; ev++) {
            memset(&open_now, 0, sizeof open_now);
            open_now.delibirdy_common = (uint8_t)ev;
            dstart(seed, 20 + (n % 150), &SRC);
            gc_item_add(&run, "BERRY", BERRY_TYPE_SITRUS, run.party[1].b.id, 1);
            int type = -1, tier = -1;
            pr_rng copy = run.run.rng;   /* the tier roll the draw makes first: randSeedInt(128) with the weights 66, 40, 19, 3 and no history */
            roll = pr_rng_seed_int(&copy, 66 + 40 + 19 + 3, 0);
            if (!gc_mystery_select(&run, &type, &tier) || type != GC_ME_DELIBIRDY) continue;
            CHECK_EQ(tier, GC_ME_GREAT);
            got[ev] = true;
        }
        if (got[1] && !got[0]) { gained++; CHECK(roll <= 66); }                 /* only the event: a Common roll */
        else if (got[0] && !got[1]) { lost++; CHECK(roll > 66 && roll <= 106); }   /* only without it: a Great roll */
        else if (got[0] && got[1]) both++;
    }
    CHECK(gained > 5 && lost > 5);
    /* the developer hook (a forced encounter) goes through the same tier, and records the encounter's own */
    for (int ev = 0; ev < 2; ev++) {
        memset(&open_now, 0, sizeof open_now);
        open_now.delibirdy_common = (uint8_t)ev;
        dstart("events-deli-force", 21, &SRC);
        gc_item_add(&run, "BERRY", BERRY_TYPE_SITRUS, run.party[1].b.id, 1);
        gc_debug_force_mystery(GC_ME_DELIBIRDY, 1);
        int type = -1, tier = -1;
        CHECK(gc_mystery_select(&run, &type, &tier));
        gc_debug_force_mystery(-1, 1);
        CHECK_EQ(type, GC_ME_DELIBIRDY);
        CHECK_EQ(tier, GC_ME_GREAT);
    }
    CHECK_EQ(both, 0);   /* on one roll the tier is Common or Great, never both: the event moves it from one to the other */
}

/* Snowy Town: the Town's weather pool is replaced by {SNOW: 1} (arena.ts setBiomeWeather: weatherPool = timedEventManager.getWeather()?.[biomeId]).
 * One slot of weight is a randSeedInt(1), which returns its minimum without drawing, so the run's RNG is left as it was. */
static int roll_weather(int biome, int wave, const gc_event_source *src, bool *rng_moved)
{
    static const char seed[] = "events-snow";
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    gc_run_set_events(&run, src);
    run.biome = biome;
    run.wave = wave;
    const pr_rng before = run.run.rng;
    const int w = gc_roll_biome_weather(&run);
    if (rng_moved) *rng_moved = memcmp(&before, &run.run.rng, sizeof before) != 0;
    return w;
}

static void snow_section(void)
{
    bool moved = false;
    memset(&open_now, 0, sizeof open_now);
    for (int wave = 1; wave <= 60; wave += 7) {
        CHECK_EQ(roll_weather(BIOME_ID_TOWN, wave, NULL, &moved), WEATHER_TYPE_NONE);        /* the Town's own pool is {NONE: 1} */
        CHECK_EQ(roll_weather(BIOME_ID_TOWN, wave, &SRC, &moved), WEATHER_TYPE_NONE);        /* a source with nothing open */
    }
    open_now.town_snow = 1;
    for (int wave = 1; wave <= 60; wave += 7) {                                              /* by day and by night alike */
        CHECK_EQ(roll_weather(BIOME_ID_TOWN, wave, &SRC, &moved), WEATHER_TYPE_SNOW);
        CHECK(!moved);
        CHECK_EQ(roll_weather(BIOME_ID_TOWN, wave, NULL, &moved), WEATHER_TYPE_NONE);         /* no source: upstream's own */
    }
    /* only the Town: every other biome rolls as it does without the event, on the same RNG */
    for (int biome = 1; biome < 20; biome++) {
        bool m1 = false, m2 = false;
        const int plain = roll_weather(biome, 20, NULL, &m1);
        const int event = roll_weather(biome, 20, &SRC, &m2);
        CHECK_EQ(event, plain);
        CHECK_EQ(m2, m1);
    }
    /* biome 10 (Seabed) has its own pool, {RAIN: 1} in the data: a non-vacuous other biome */
    CHECK(roll_weather(10, 20, &SRC, &moved) != WEATHER_TYPE_SNOW);
    /* asked at each roll */
    open_now.town_snow = 0;
    CHECK_EQ(roll_weather(BIOME_ID_TOWN, 8, &SRC, &moved), WEATHER_TYPE_NONE);
}

/* the shop's DNA Splicers weights with fusions boosted (init-modifier-pools.ts): in the GREAT pool 4 in a Spliced Endless run, else 2 in Classic
 * while boosted, else 0; in the ROGUE pool 24 unless (Classic and boosted) or Spliced Endless, with two unfused party members in both. */
extern int pr_reward_weight(pr_reward_ctx *ctx, int item, int tier, int reroll_count);
static int splicers_weight(int mode, const gc_event_source *src, int tier, int unfused)
{
    static const char seed[] = "events-fusion";
    memset(&run, 0, sizeof run);
    gc_run_new(&run, mode, seed, 7);
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
    for (int i = 0; i < 3; i++) run.party[i].b.fusion_species = i < unfused ? 0 : SPECIES_ID_PIDGEY;   /* the others are fusions (a Spliced Endless team starts fused) */
    return pr_reward_weight(gc_reward_ctx(&run), PR_ITEM_DNA_SPLICERS, tier, 0);
}

static void fusion_section(void)
{
    memset(&open_now, 0, sizeof open_now);
    CHECK_EQ(splicers_weight(GAME_MODES_CLASSIC, NULL, MODIFIER_TIER_GREAT, 3), 0);
    CHECK_EQ(splicers_weight(GAME_MODES_CLASSIC, NULL, MODIFIER_TIER_ROGUE, 3), 24);
    CHECK_EQ(splicers_weight(GAME_MODES_CLASSIC, &SRC, MODIFIER_TIER_ROGUE, 3), 24);   /* a source with nothing open */
    open_now.boost_fusions = 1;
    CHECK_EQ(splicers_weight(GAME_MODES_CLASSIC, &SRC, MODIFIER_TIER_GREAT, 3), 2);
    CHECK_EQ(splicers_weight(GAME_MODES_CLASSIC, &SRC, MODIFIER_TIER_ROGUE, 3), 0);
    CHECK_EQ(splicers_weight(GAME_MODES_CLASSIC, NULL, MODIFIER_TIER_GREAT, 3), 0);    /* no source: the weights are upstream's own */
    CHECK_EQ(splicers_weight(GAME_MODES_CLASSIC, &SRC, MODIFIER_TIER_GREAT, 2), 2);    /* two unfused members are enough */
    CHECK_EQ(splicers_weight(GAME_MODES_CLASSIC, &SRC, MODIFIER_TIER_GREAT, 1), 0);    /* one is not: party.filter(p => !p.fusionSpecies).length > 1 */
    CHECK_EQ(splicers_weight(GAME_MODES_CLASSIC, &SRC, MODIFIER_TIER_ROGUE, 1), 0);
    /* Classic only: Endless is unchanged, Spliced Endless has its own weights whatever the event says */
    CHECK_EQ(splicers_weight(GAME_MODES_ENDLESS, &SRC, MODIFIER_TIER_GREAT, 3), 0);
    CHECK_EQ(splicers_weight(GAME_MODES_ENDLESS, &SRC, MODIFIER_TIER_ROGUE, 3), 24);
    CHECK_EQ(splicers_weight(GAME_MODES_SPLICED_ENDLESS, &SRC, MODIFIER_TIER_GREAT, 3), 4);
    CHECK_EQ(splicers_weight(GAME_MODES_SPLICED_ENDLESS, &SRC, MODIFIER_TIER_ROGUE, 3), 0);
}

int main(int argc, char **argv)
{
    const char *only = argc > 1 ? argv[1] : NULL;
    if (!only || !strcmp(only, "shiny")) shiny_section();
    if (!only || !strcmp(only, "luck")) luck_section();
    if (!only || !strcmp(only, "candy")) candy_section();
    if (!only || !strcmp(only, "delibirdy")) delibirdy_section();
    if (!only || !strcmp(only, "snow")) snow_section();
    if (!only || !strcmp(only, "fusion")) fusion_section();
    if (!only || !strcmp(only, "rewards")) rewards_section();
    if (!only || !strcmp(only, "voucher")) voucher_section();
    printf("events: %d checks, 0 failures\n", checks);
    return 0;
}
