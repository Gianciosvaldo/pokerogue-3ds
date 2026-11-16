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

int main(int argc, char **argv)
{
    const char *only = argc > 1 ? argv[1] : NULL;
    if (!only || !strcmp(only, "shiny")) shiny_section();
    if (!only || !strcmp(only, "luck")) luck_section();
    if (!only || !strcmp(only, "candy")) candy_section();
    if (!only || !strcmp(only, "rewards")) rewards_section();
    if (!only || !strcmp(only, "voucher")) voucher_section();
    printf("events: %d checks, 0 failures\n", checks);
    return 0;
}
