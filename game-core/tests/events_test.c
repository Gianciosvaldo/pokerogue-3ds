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
 *   delibirdy: Delibirdy Day: the bonus charm of getDelibirdyBuff after each of Delibird-y's three gifts, and its tier moved GREAT -> COMMON.
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

int main(int argc, char **argv)
{
    const char *only = argc > 1 ? argv[1] : NULL;
    if (!only || !strcmp(only, "shiny")) shiny_section();
    if (!only || !strcmp(only, "luck")) luck_section();
    if (!only || !strcmp(only, "candy")) candy_section();
    if (!only || !strcmp(only, "delibirdy")) delibirdy_section();
    if (!only || !strcmp(only, "voucher")) voucher_section();
    printf("events: %d checks, 0 failures\n", checks);
    return 0;
}
