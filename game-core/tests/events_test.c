/* Timed events in a run (gc/events.h): what game-core does with the effects the app's source says are open. The schedule (which event is
 * open when) is custom-core's and has its own tests; here the source is a plain struct the test changes, so each effect is read in the
 * place upstream reads it.
 *
 * Sections (run with a name to run one, no argument for all):
 *   shiny : the enemy's shiny roll. The shiny test is (E ^ F) < threshold with E from the trainer and secret ids and F from the enemy's own
 *           id, so a run whose ids are chosen to put E ^ F just below or just above 64 * multiplier shows the multiplier exactly; the
 *           expectation is upstream's threshold (BASE_SHINY_CHANCE 64 x getShinyEncounterMultiplier()), not this port's output.
 *   candy : the Classic candy friendship multiplier (getClassicFriendshipMultiplier(): 3, or an event's when higher; Classic only).
 *   voucher: the voucher a defeated boss gives (TrainerVictoryPhase): upgraded while an event says getUpgradeUnlockedVouchers().
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
    if (!only || !strcmp(only, "fusion")) fusion_section();
    if (!only || !strcmp(only, "voucher")) voucher_section();
    printf("events: %d checks, 0 failures\n", checks);
    return 0;
}
