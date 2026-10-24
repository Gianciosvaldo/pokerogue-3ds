/* custom-core model tests: boundary, invalid-input and invariant checks for the version-1 contracts.
 *
 * Every expected value below is a literal written by hand from docs/CUSTOM-CONTENT-CONTRACTS.md or from the handoff's
 * defaults (16 records, 5 turns, 64 events, windows 27/77/127/177); none is computed with the code under test.
 * Large structures are static: nothing here is a stack object the size of a save buffer. */
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "cc/model.h"
#include "check.h"

/* ------------------------------------------------------------------ builders */

static cc_actor mk_actor(uint32_t id, uint16_t species, uint8_t side)
{
    cc_actor a;
    memset(&a, 0, sizeof a);
    a.id = id;
    a.species = species;
    a.side = side;
    a.level = 50;
    return a;
}

/* a shown 12-point hit on a player Pokemon (species 25), no attribution */
static cc_event mk_damage(uint32_t seq)
{
    cc_event e;
    memset(&e, 0, sizeof e);
    e.seq = seq;
    e.kind = CC_EV_DAMAGE;
    e.target_side = CC_SIDE_PLAYER;
    e.target_species = 25;
    e.known = CC_EK_SHOWN;
    e.shown = 12;
    return e;
}

static cc_turn mk_turn(uint32_t battle_seq, uint16_t turn, uint32_t first_seq, unsigned n)
{
    cc_turn t;
    memset(&t, 0, sizeof t);
    t.battle_seq = battle_seq;
    t.wave = 3;
    t.turn = turn;
    t.n_events = (uint8_t)n;
    for (unsigned i = 0; i < n; i++) t.events[i] = mk_damage(first_seq + i);
    return t;
}

static cc_terminal mk_terminal(cc_run_uid run, cc_txn_id txn)
{
    cc_terminal t;
    memset(&t, 0, sizeof t);
    t.run_uid = run;
    t.txn_id = txn;
    t.outcome = CC_OUT_DEFEAT;
    t.wave = 27;
    t.biome = 3;
    t.known = CC_TK_FINISH_MOVE | CC_TK_FINISH_ACTOR | CC_TK_FALLEN_ACTOR;
    t.finish_move = 33;
    t.finish_actor = mk_actor(900, 19, CC_SIDE_ENEMY);
    t.fallen_actor = mk_actor(7, 25, CC_SIDE_PLAYER);
    return t;
}

/* a stored, supported record: enemy Rattata (19) that finished the run at wave 27 */
static cc_nemesis mk_nem(cc_record_id id, cc_run_uid run, uint32_t pokemon_id)
{
    cc_nemesis n;
    memset(&n, 0, sizeof n);
    n.record_id = id;
    n.life = CC_LIFE_STORED;
    n.known = CC_NK_ABILITY | CC_NK_PASSIVE | CC_NK_MOVES | CC_NK_IVS | CC_NK_NATURE | CC_NK_LOADOUT |
              CC_NK_FINISH_MOVE | CC_NK_FALLEN | CC_NK_FINISHER_VERIFIED;
    n.source_run_uid = run;
    n.source_pokemon_id = pokemon_id;
    n.source_wave = 27;
    n.source_biome = 3;
    n.species = 19;
    n.form = 1;
    n.gender = 0;
    n.orig_level = 30;
    n.nature = 4;
    n.ability = 50;
    n.passive = 0;
    for (int i = 0; i < 6; i++) n.ivs[i] = (uint8_t)(10 + i);
    n.n_moves = 2;
    n.moves[0] = 33;
    n.moves[1] = 98;
    n.n_items = 1;
    strcpy(n.items[0].key, "LEFTOVERS");
    n.items[0].arg = -1;
    n.items[0].stack = 2;
    n.finish_move = 33;
    n.fallen_species = 25;
    n.fallen_form = 0;
    return n;
}

/* ------------------------------------------------------------------ observation */

static cc_obs mk_obs(void)
{
    cc_obs o;
    memset(&o, 0, sizeof o);
    o.kind = CC_OBS_DAMAGE;
    o.battler = 0;
    o.target = -1;
    o.battler_species = 25;
    o.id = 4;
    o.value = 10;
    o.hp_after = 5;
    o.hp_max_after = 30;
    return o;
}

static void test_obs(void)
{
    cc_obs o = mk_obs();
    CHECK_STATUS(cc_obs_validate(&o), CC_OK);
    CHECK_STATUS(cc_obs_validate(NULL), CC_E_NULL);

    o = mk_obs(); o.kind = CC_OBS_NONE;              CHECK_STATUS(cc_obs_validate(&o), CC_E_RANGE);
    o = mk_obs(); o.kind = CC_OBS_KIND_COUNT_;       CHECK_STATUS(cc_obs_validate(&o), CC_E_RANGE);
    o = mk_obs(); o.battler = -1;                    CHECK_STATUS(cc_obs_validate(&o), CC_E_RANGE);
    o = mk_obs(); o.battler = 4;                     CHECK_STATUS(cc_obs_validate(&o), CC_E_RANGE);
    o = mk_obs(); o.battler = 3;                     CHECK_STATUS(cc_obs_validate(&o), CC_OK);
    o = mk_obs(); o.target = -2;                     CHECK_STATUS(cc_obs_validate(&o), CC_E_RANGE);
    o = mk_obs(); o.target = 4;                      CHECK_STATUS(cc_obs_validate(&o), CC_E_RANGE);
    o = mk_obs(); o.battler_species = 0;             CHECK_STATUS(cc_obs_validate(&o), CC_E_RANGE);
    o = mk_obs(); o.battler_species = 8902;          CHECK_STATUS(cc_obs_validate(&o), CC_E_RANGE);
    o = mk_obs(); o.battler_species = 8901;          CHECK_STATUS(cc_obs_validate(&o), CC_OK);
    /* a target position needs a species; no target position carries none */
    o = mk_obs(); o.target = 2;                      CHECK_STATUS(cc_obs_validate(&o), CC_E_RANGE);
    o = mk_obs(); o.target = 2; o.target_species = 19; CHECK_STATUS(cc_obs_validate(&o), CC_OK);
    o = mk_obs(); o.target_species = 19;             CHECK_STATUS(cc_obs_validate(&o), CC_E_NONCANONICAL);
    o = mk_obs(); o.target_id = 5;                   CHECK_STATUS(cc_obs_validate(&o), CC_E_NONCANONICAL);
    o = mk_obs(); o.target = 2; o.target_species = 8901; CHECK_STATUS(cc_obs_validate(&o), CC_OK);
    o = mk_obs(); o.target = 2; o.target_species = 8902; CHECK_STATUS(cc_obs_validate(&o), CC_E_RANGE);
    /* snapshot HP: both present or both absent, and consistent */
    o = mk_obs(); o.hp_after = -1;                   CHECK_STATUS(cc_obs_validate(&o), CC_E_CONSISTENCY);
    o = mk_obs(); o.hp_after = -1; o.hp_max_after = -1; CHECK_STATUS(cc_obs_validate(&o), CC_OK);
    o = mk_obs(); o.hp_after = 31;                   CHECK_STATUS(cc_obs_validate(&o), CC_E_CONSISTENCY);
    o = mk_obs(); o.hp_max_after = 0; o.hp_after = 0; CHECK_STATUS(cc_obs_validate(&o), CC_E_RANGE);
    o = mk_obs(); o.hp_after = -2;                   CHECK_STATUS(cc_obs_validate(&o), CC_E_RANGE);
    o = mk_obs(); o.status_after = 7;                CHECK_STATUS(cc_obs_validate(&o), CC_OK);
    o = mk_obs(); o.status_after = 8;                CHECK_STATUS(cc_obs_validate(&o), CC_E_RANGE);
    /* per kind */
    o = mk_obs(); o.id = 15;                         CHECK_STATUS(cc_obs_validate(&o), CC_OK);
    o = mk_obs(); o.id = 16;                         CHECK_STATUS(cc_obs_validate(&o), CC_E_RANGE);
    o = mk_obs(); o.value = -1;                      CHECK_STATUS(cc_obs_validate(&o), CC_E_RANGE);
    o = mk_obs(); o.kind = CC_OBS_HP; o.id = 0; o.value = -7; CHECK_STATUS(cc_obs_validate(&o), CC_OK);
    o = mk_obs(); o.kind = CC_OBS_HP; o.id = 1;      CHECK_STATUS(cc_obs_validate(&o), CC_E_NONCANONICAL);
    o = mk_obs(); o.kind = CC_OBS_MOVE_ANIM; o.value = 0; o.id = 920; CHECK_STATUS(cc_obs_validate(&o), CC_OK);
    o = mk_obs(); o.kind = CC_OBS_MOVE_ANIM; o.value = 0; o.id = 921; CHECK_STATUS(cc_obs_validate(&o), CC_E_RANGE);
    o = mk_obs(); o.kind = CC_OBS_MOVE_ANIM; o.value = 0; o.id = 0;   CHECK_STATUS(cc_obs_validate(&o), CC_E_RANGE);
    o = mk_obs(); o.kind = CC_OBS_MOVE_ANIM; o.value = 3; o.id = 5;   CHECK_STATUS(cc_obs_validate(&o), CC_E_NONCANONICAL);
    o = mk_obs(); o.kind = CC_OBS_SWITCH_IN; o.id = 0; o.value = 5;   CHECK_STATUS(cc_obs_validate(&o), CC_OK);
    o = mk_obs(); o.kind = CC_OBS_SWITCH_IN; o.id = 0; o.value = 6;   CHECK_STATUS(cc_obs_validate(&o), CC_E_RANGE);
    o = mk_obs(); o.kind = CC_OBS_FAINT; o.id = 0; o.value = 0;       CHECK_STATUS(cc_obs_validate(&o), CC_OK);
    o = mk_obs(); o.kind = CC_OBS_FAINT; o.id = 0; o.value = 1;       CHECK_STATUS(cc_obs_validate(&o), CC_E_NONCANONICAL);
    o = mk_obs(); o.kind = CC_OBS_SWITCH_OUT; o.id = 2; o.value = 0;  CHECK_STATUS(cc_obs_validate(&o), CC_E_NONCANONICAL);

    /* optional attribution (C02): the adapter's assertion, same known-bit rules as an event */
    const uint16_t all_src = CC_EK_SOURCE_SIDE | CC_EK_SOURCE_ID | CC_EK_SOURCE_SPECIES | CC_EK_MOVE;
    o = mk_obs();
    o.src_known = all_src; o.src_side = 1; o.src_id = 9; o.src_species = 6; o.src_form = 2; o.src_move = 33;
    CHECK_STATUS(cc_obs_validate(&o), CC_OK);
    CHECK_EQ(CC_EK_SOURCE_ALL, all_src);
    o = mk_obs(); o.src_known = CC_EK_SOURCE_SIDE;                  CHECK_STATUS(cc_obs_validate(&o), CC_OK);
    o = mk_obs(); o.src_known = CC_EK_MOVE; o.src_move = 920;       CHECK_STATUS(cc_obs_validate(&o), CC_OK);
    o = mk_obs(); o.src_known = CC_EK_MOVE; o.src_move = 1;         CHECK_STATUS(cc_obs_validate(&o), CC_OK);
    o = mk_obs(); o.src_known = CC_EK_SOURCE_SIDE | CC_EK_SOURCE_ID; o.src_id = 1; CHECK_STATUS(cc_obs_validate(&o), CC_OK);
    o = mk_obs(); o.src_known = CC_EK_SOURCE_SIDE | CC_EK_SOURCE_SPECIES; o.src_species = 8901; CHECK_STATUS(cc_obs_validate(&o), CC_OK);
    o = mk_obs(); o.src_known = CC_EK_SOURCE_SIDE | CC_EK_SOURCE_SPECIES; o.src_species = 1; CHECK_STATUS(cc_obs_validate(&o), CC_OK);
    o = mk_obs(); o.src_known = CC_EK_SOURCE_ID; o.src_id = 9;      CHECK_STATUS(cc_obs_validate(&o), CC_E_CONSISTENCY);
    o = mk_obs(); o.src_known = CC_EK_SOURCE_SPECIES; o.src_species = 6; CHECK_STATUS(cc_obs_validate(&o), CC_E_CONSISTENCY);
    o = mk_obs(); o.src_known = CC_EK_SHOWN;                        CHECK_STATUS(cc_obs_validate(&o), CC_E_UNKNOWN_BITS);
    o = mk_obs(); o.src_known = CC_EK_TARGET_ID;                    CHECK_STATUS(cc_obs_validate(&o), CC_E_UNKNOWN_BITS);
    o = mk_obs(); o.src_known = CC_EK_RESULT;                       CHECK_STATUS(cc_obs_validate(&o), CC_E_UNKNOWN_BITS);
    o = mk_obs(); o.src_known = CC_EK_HP_AFTER;                     CHECK_STATUS(cc_obs_validate(&o), CC_E_UNKNOWN_BITS);
    o = mk_obs(); o.src_known = 1u << 9;                            CHECK_STATUS(cc_obs_validate(&o), CC_E_UNKNOWN_BITS);
    o = mk_obs(); o.src_known = 0x8000u;                            CHECK_STATUS(cc_obs_validate(&o), CC_E_UNKNOWN_BITS);
    /* a clear bit requires a zero value */
    o = mk_obs(); o.src_side = 1;                                   CHECK_STATUS(cc_obs_validate(&o), CC_E_NONCANONICAL);
    o = mk_obs(); o.src_id = 1;                                     CHECK_STATUS(cc_obs_validate(&o), CC_E_NONCANONICAL);
    o = mk_obs(); o.src_species = 1;                                CHECK_STATUS(cc_obs_validate(&o), CC_E_NONCANONICAL);
    o = mk_obs(); o.src_form = 1;                                   CHECK_STATUS(cc_obs_validate(&o), CC_E_NONCANONICAL);
    o = mk_obs(); o.src_move = 1;                                   CHECK_STATUS(cc_obs_validate(&o), CC_E_NONCANONICAL);
    o = mk_obs(); o.src_known = CC_EK_SOURCE_SIDE | CC_EK_SOURCE_ID; o.src_id = 1; o.src_form = 1;
    CHECK_STATUS(cc_obs_validate(&o), CC_E_NONCANONICAL);           /* the form belongs to the species bit */
    /* known values in range */
    o = mk_obs(); o.src_known = CC_EK_SOURCE_SIDE; o.src_side = 2;  CHECK_STATUS(cc_obs_validate(&o), CC_E_RANGE);
    o = mk_obs(); o.src_known = CC_EK_SOURCE_SIDE | CC_EK_SOURCE_ID; o.src_id = 0; CHECK_STATUS(cc_obs_validate(&o), CC_E_RANGE);
    o = mk_obs(); o.src_known = CC_EK_SOURCE_SIDE | CC_EK_SOURCE_SPECIES; o.src_species = 0; CHECK_STATUS(cc_obs_validate(&o), CC_E_RANGE);
    o = mk_obs(); o.src_known = CC_EK_SOURCE_SIDE | CC_EK_SOURCE_SPECIES; o.src_species = 8902; CHECK_STATUS(cc_obs_validate(&o), CC_E_RANGE);
    o = mk_obs(); o.src_known = CC_EK_MOVE; o.src_move = 0;         CHECK_STATUS(cc_obs_validate(&o), CC_E_RANGE);
    o = mk_obs(); o.src_known = CC_EK_MOVE; o.src_move = 921;       CHECK_STATUS(cc_obs_validate(&o), CC_E_RANGE);
    /* only a hit, a heal or a faint may name a source */
    {
        cc_obs base = mk_obs();
        base.src_known = CC_EK_SOURCE_SIDE | CC_EK_MOVE; base.src_side = 1; base.src_move = 33;
        o = base; o.kind = CC_OBS_FAINT; o.id = 0; o.value = 0;     CHECK_STATUS(cc_obs_validate(&o), CC_OK);
        o = base; o.kind = CC_OBS_HP; o.id = 0; o.value = 7;        CHECK_STATUS(cc_obs_validate(&o), CC_OK);
        o = base; o.kind = CC_OBS_HP; o.id = 0; o.value = 1;        CHECK_STATUS(cc_obs_validate(&o), CC_OK);
        o = base; o.kind = CC_OBS_HP; o.id = 0; o.value = 0;        CHECK_STATUS(cc_obs_validate(&o), CC_E_CONSISTENCY);
        o = base; o.kind = CC_OBS_HP; o.id = 0; o.value = -7;       CHECK_STATUS(cc_obs_validate(&o), CC_E_CONSISTENCY);
        o = base; o.kind = CC_OBS_HP; o.id = 0; o.value = -1;       CHECK_STATUS(cc_obs_validate(&o), CC_E_CONSISTENCY);
        o = base; o.kind = CC_OBS_SWITCH_IN; o.id = 0; o.value = 0; CHECK_STATUS(cc_obs_validate(&o), CC_E_CONSISTENCY);
        o = base; o.kind = CC_OBS_SWITCH_OUT; o.id = 0; o.value = 0; CHECK_STATUS(cc_obs_validate(&o), CC_E_CONSISTENCY);
        o = base; o.kind = CC_OBS_MOVE_ANIM; o.id = 33; o.value = 0; CHECK_STATUS(cc_obs_validate(&o), CC_E_CONSISTENCY);
    }
}

/* ------------------------------------------------------------------ events */

static void test_events(void)
{
    cc_event e = mk_damage(1);
    CHECK_STATUS(cc_event_validate(&e), CC_OK);
    CHECK_STATUS(cc_event_validate(NULL), CC_E_NULL);

    /* one valid literal of every kind */
    e = mk_damage(1);
    e.known = CC_EK_TARGET_ID | CC_EK_SOURCE_SIDE | CC_EK_SOURCE_ID | CC_EK_SOURCE_SPECIES | CC_EK_MOVE | CC_EK_RESULT |
              CC_EK_SHOWN | CC_EK_HP_DELTA | CC_EK_HP_AFTER;
    e.target_id = 7; e.source_side = CC_SIDE_ENEMY; e.source_id = 900; e.source_species = 19; e.source_form = 2;
    e.move_id = 33; e.result = 4; e.shown = 12; e.hp_delta = -12; e.hp_after = 0;
    CHECK_STATUS(cc_event_validate(&e), CC_OK);                       /* every optional field known */
    e.hp_delta = 0;                                                   /* a measured zero is a value, not "unknown" */
    CHECK_STATUS(cc_event_validate(&e), CC_OK);

    e = mk_damage(1); e.kind = CC_EV_HEAL; e.known = CC_EK_SHOWN | CC_EK_HP_DELTA; e.hp_delta = 5;
    CHECK_STATUS(cc_event_validate(&e), CC_OK);
    e.hp_delta = 0;                                                   /* a heal of exactly nothing */
    CHECK_STATUS(cc_event_validate(&e), CC_OK);
    e = mk_damage(1); e.kind = CC_EV_HP_LOSS; e.known = CC_EK_SHOWN | CC_EK_HP_DELTA; e.hp_delta = 0;
    CHECK_STATUS(cc_event_validate(&e), CC_OK);
    e = mk_damage(1); e.known = CC_EK_SOURCE_SIDE | CC_EK_SOURCE_SPECIES | CC_EK_SHOWN;
    e.source_side = CC_SIDE_ENEMY; e.source_species = 8901;           CHECK_STATUS(cc_event_validate(&e), CC_OK);
    e.source_species = 8902;                                          CHECK_STATUS(cc_event_validate(&e), CC_E_RANGE);
    e = mk_damage(1); e.kind = CC_EV_HP_LOSS; e.known = CC_EK_SHOWN | CC_EK_HP_DELTA; e.hp_delta = -3;
    CHECK_STATUS(cc_event_validate(&e), CC_OK);
    e = mk_damage(1); e.kind = CC_EV_FAINT; e.known = CC_EK_SOURCE_SIDE | CC_EK_SOURCE_ID | CC_EK_MOVE | CC_EK_HP_AFTER;
    e.source_side = CC_SIDE_ENEMY; e.source_id = 4; e.move_id = 1; e.shown = 0;
    CHECK_STATUS(cc_event_validate(&e), CC_OK);
    e = mk_damage(1); e.kind = CC_EV_SWITCH_OUT; e.known = 0; e.shown = 0;
    CHECK_STATUS(cc_event_validate(&e), CC_OK);
    e = mk_damage(1); e.kind = CC_EV_SWITCH_IN; e.known = CC_EK_HP_AFTER; e.shown = 0; e.hp_after = 9;
    CHECK_STATUS(cc_event_validate(&e), CC_OK);
    e = mk_damage(1); e.kind = CC_EV_STATUS; e.known = CC_EK_SHOWN; e.shown = 2;
    CHECK_STATUS(cc_event_validate(&e), CC_OK);

    /* kind and flag bits */
    e = mk_damage(1); e.kind = CC_EV_NONE;               CHECK_STATUS(cc_event_validate(&e), CC_E_RANGE);
    e = mk_damage(1); e.kind = CC_EV_KIND_COUNT_;        CHECK_STATUS(cc_event_validate(&e), CC_E_RANGE);
    e = mk_damage(1); e.known |= 1u << 9;                CHECK_STATUS(cc_event_validate(&e), CC_E_UNKNOWN_BITS);
    e = mk_damage(1); e.known |= 1u << 15;               CHECK_STATUS(cc_event_validate(&e), CC_E_UNKNOWN_BITS);
    e = mk_damage(1); e.target_side = 2;                 CHECK_STATUS(cc_event_validate(&e), CC_E_RANGE);
    e = mk_damage(1); e.target_species = 0;              CHECK_STATUS(cc_event_validate(&e), CC_E_RANGE);
    e = mk_damage(1); e.target_species = 8902;           CHECK_STATUS(cc_event_validate(&e), CC_E_RANGE);
    e = mk_damage(1); e.target_species = 8901;           CHECK_STATUS(cc_event_validate(&e), CC_OK);

    /* a damage event needs a number: shown or measured */
    e = mk_damage(1); e.known = 0; e.shown = 0;          CHECK_STATUS(cc_event_validate(&e), CC_E_CONSISTENCY);
    e = mk_damage(1); e.known = CC_EK_HP_DELTA; e.shown = 0; e.hp_delta = -1; CHECK_STATUS(cc_event_validate(&e), CC_OK);
    /* HP loss without a hit is never attributed */
    e = mk_damage(1); e.kind = CC_EV_HP_LOSS; e.known = CC_EK_SHOWN | CC_EK_SOURCE_SIDE; e.source_side = 1;
    CHECK_STATUS(cc_event_validate(&e), CC_E_CONSISTENCY);
    e = mk_damage(1); e.kind = CC_EV_HP_LOSS; e.known = CC_EK_SHOWN | CC_EK_MOVE; e.move_id = 5;
    CHECK_STATUS(cc_event_validate(&e), CC_E_CONSISTENCY);
    /* a switch carries no amounts and no source */
    e = mk_damage(1); e.kind = CC_EV_SWITCH_IN; e.known = CC_EK_MOVE; e.shown = 0; e.move_id = 5;
    CHECK_STATUS(cc_event_validate(&e), CC_E_CONSISTENCY);
    /* a faint carries no amounts */
    e = mk_damage(1); e.kind = CC_EV_FAINT; e.known = CC_EK_SHOWN;
    CHECK_STATUS(cc_event_validate(&e), CC_E_CONSISTENCY);
    /* a status change needs the new status, in 1..7 */
    e = mk_damage(1); e.kind = CC_EV_STATUS; e.known = 0; e.shown = 0;   CHECK_STATUS(cc_event_validate(&e), CC_E_CONSISTENCY);
    e = mk_damage(1); e.kind = CC_EV_STATUS; e.shown = 0;                CHECK_STATUS(cc_event_validate(&e), CC_E_RANGE);
    e = mk_damage(1); e.kind = CC_EV_STATUS; e.shown = 8;                CHECK_STATUS(cc_event_validate(&e), CC_E_RANGE);
    e = mk_damage(1); e.kind = CC_EV_STATUS; e.shown = 7;                CHECK_STATUS(cc_event_validate(&e), CC_OK);

    /* a source id or species needs its side */
    e = mk_damage(1); e.known |= CC_EK_SOURCE_ID; e.source_id = 4;       CHECK_STATUS(cc_event_validate(&e), CC_E_CONSISTENCY);
    e = mk_damage(1); e.known |= CC_EK_SOURCE_SPECIES; e.source_species = 4; CHECK_STATUS(cc_event_validate(&e), CC_E_CONSISTENCY);
    e = mk_damage(1); e.known |= CC_EK_SOURCE_SIDE; e.source_side = 2;   CHECK_STATUS(cc_event_validate(&e), CC_E_RANGE);

    /* the canonical rule: every optional field holds zero while its bit is clear */
    e = mk_damage(1); e.target_id = 1;                   CHECK_STATUS(cc_event_validate(&e), CC_E_NONCANONICAL);
    e = mk_damage(1); e.source_side = 1;                 CHECK_STATUS(cc_event_validate(&e), CC_E_NONCANONICAL);
    e = mk_damage(1); e.source_id = 1;                   CHECK_STATUS(cc_event_validate(&e), CC_E_NONCANONICAL);
    e = mk_damage(1); e.source_species = 1;              CHECK_STATUS(cc_event_validate(&e), CC_E_NONCANONICAL);
    e = mk_damage(1); e.source_form = 1;                 CHECK_STATUS(cc_event_validate(&e), CC_E_NONCANONICAL);
    e = mk_damage(1); e.move_id = 1;                     CHECK_STATUS(cc_event_validate(&e), CC_E_NONCANONICAL);
    e = mk_damage(1); e.result = 1;                      CHECK_STATUS(cc_event_validate(&e), CC_E_NONCANONICAL);
    e = mk_damage(1); e.known = CC_EK_HP_DELTA; e.shown = 1; e.hp_delta = -1; CHECK_STATUS(cc_event_validate(&e), CC_E_NONCANONICAL);
    e = mk_damage(1); e.hp_delta = -1;                   CHECK_STATUS(cc_event_validate(&e), CC_E_NONCANONICAL);
    e = mk_damage(1); e.hp_after = 1;                    CHECK_STATUS(cc_event_validate(&e), CC_E_NONCANONICAL);

    /* known values must be in range */
    e = mk_damage(1); e.known |= CC_EK_TARGET_ID; e.target_id = 0;       CHECK_STATUS(cc_event_validate(&e), CC_E_RANGE);
    e = mk_damage(1); e.known |= CC_EK_SOURCE_SIDE | CC_EK_SOURCE_ID; e.source_id = 0; CHECK_STATUS(cc_event_validate(&e), CC_E_RANGE);
    e = mk_damage(1); e.known |= CC_EK_SOURCE_SIDE | CC_EK_SOURCE_SPECIES; e.source_species = 0; CHECK_STATUS(cc_event_validate(&e), CC_E_RANGE);
    e = mk_damage(1); e.known |= CC_EK_MOVE; e.move_id = 0;              CHECK_STATUS(cc_event_validate(&e), CC_E_RANGE);
    e = mk_damage(1); e.known |= CC_EK_MOVE; e.move_id = 920;            CHECK_STATUS(cc_event_validate(&e), CC_OK);
    e = mk_damage(1); e.known |= CC_EK_MOVE; e.move_id = 921;            CHECK_STATUS(cc_event_validate(&e), CC_E_RANGE);
    e = mk_damage(1); e.known |= CC_EK_RESULT; e.result = 15;            CHECK_STATUS(cc_event_validate(&e), CC_OK);
    e = mk_damage(1); e.known |= CC_EK_RESULT; e.result = 16;            CHECK_STATUS(cc_event_validate(&e), CC_E_RANGE);
    e = mk_damage(1); e.shown = -1;                                      CHECK_STATUS(cc_event_validate(&e), CC_E_RANGE);
    e = mk_damage(1); e.shown = INT32_MAX;                               CHECK_STATUS(cc_event_validate(&e), CC_OK);
    e = mk_damage(1); e.known |= CC_EK_HP_AFTER; e.hp_after = -1;        CHECK_STATUS(cc_event_validate(&e), CC_E_RANGE);
    /* damage never raises HP, a heal never lowers it, an HP loss never raises it */
    e = mk_damage(1); e.known |= CC_EK_HP_DELTA; e.hp_delta = 1;         CHECK_STATUS(cc_event_validate(&e), CC_E_CONSISTENCY);
    e = mk_damage(1); e.kind = CC_EV_HEAL; e.known |= CC_EK_HP_DELTA; e.hp_delta = -1; CHECK_STATUS(cc_event_validate(&e), CC_E_CONSISTENCY);
    e = mk_damage(1); e.kind = CC_EV_HP_LOSS; e.known |= CC_EK_HP_DELTA; e.hp_delta = 1; CHECK_STATUS(cc_event_validate(&e), CC_E_CONSISTENCY);
}

/* ------------------------------------------------------------------ turns and history */

static void test_turns(void)
{
    static cc_turn t;
    t = mk_turn(1, 0, 1, 0);
    CHECK_STATUS(cc_turn_validate(&t), CC_OK);                         /* an empty turn */
    t = mk_turn(1, 1, 1, 1);
    CHECK_STATUS(cc_turn_validate(&t), CC_OK);
    t = mk_turn(1, 1, 1, 64);
    CHECK_STATUS(cc_turn_validate(&t), CC_OK);                         /* exactly full */
    t.n_events = 65;
    CHECK_STATUS(cc_turn_validate(&t), CC_E_COUNT);                    /* one over */
    t.n_events = 255;
    CHECK_STATUS(cc_turn_validate(&t), CC_E_COUNT);
    CHECK_STATUS(cc_turn_validate(NULL), CC_E_NULL);

    t = mk_turn(0, 1, 1, 1);  CHECK_STATUS(cc_turn_validate(&t), CC_E_RANGE);     /* battle_seq starts at 1 */
    t = mk_turn(1, 1, 1, 1);  t.wave = 0; CHECK_STATUS(cc_turn_validate(&t), CC_E_RANGE);
    t = mk_turn(1, 1, 1, 1);  t.flags = 8; CHECK_STATUS(cc_turn_validate(&t), CC_E_UNKNOWN_BITS);
    t = mk_turn(1, 1, 1, 1);  t.flags = CC_TF_SOURCE_LOSS | CC_TF_BEGAN_MID; CHECK_STATUS(cc_turn_validate(&t), CC_OK);

    /* eviction: flag, count and a full turn travel together */
    t = mk_turn(1, 1, 1, 64); t.flags = CC_TF_EVICTED; t.dropped = 3;
    CHECK_STATUS(cc_turn_validate(&t), CC_OK);
    t = mk_turn(1, 1, 1, 64); t.flags = CC_TF_EVICTED; t.dropped = 0;
    CHECK_STATUS(cc_turn_validate(&t), CC_E_CONSISTENCY);              /* the flag without a count */
    t = mk_turn(1, 1, 1, 64); t.flags = 0; t.dropped = 1;
    CHECK_STATUS(cc_turn_validate(&t), CC_E_CONSISTENCY);              /* a count without the flag */
    t = mk_turn(1, 1, 1, 63); t.flags = CC_TF_EVICTED; t.dropped = 1;
    CHECK_STATUS(cc_turn_validate(&t), CC_E_CONSISTENCY);              /* evicted, yet not full */
    t = mk_turn(1, 1, 1, 64); t.flags = CC_TF_EVICTED; t.dropped = 65535;
    CHECK_STATUS(cc_turn_validate(&t), CC_OK);

    /* order inside a turn */
    t = mk_turn(1, 1, 5, 3);
    CHECK_STATUS(cc_turn_validate(&t), CC_OK);
    t.events[1].seq = 5;                                               /* repeats the first */
    CHECK_STATUS(cc_turn_validate(&t), CC_E_DUPLICATE);
    t = mk_turn(1, 1, 5, 3); t.events[2].seq = 4;                      /* goes back */
    CHECK_STATUS(cc_turn_validate(&t), CC_E_ORDER);
    t = mk_turn(1, 1, 5, 3); t.events[1].kind = CC_EV_NONE;            /* a bad event is reported as itself */
    CHECK_STATUS(cc_turn_validate(&t), CC_E_RANGE);
    /* slots beyond n_events are not read */
    t = mk_turn(1, 1, 5, 2); t.events[2].kind = 200;
    CHECK_STATUS(cc_turn_validate(&t), CC_OK);
}

static void test_history(void)
{
    static cc_history h;
    memset(&h, 0, sizeof h);
    CHECK_STATUS(cc_history_validate(&h), CC_OK);                      /* empty */
    CHECK_STATUS(cc_history_validate(NULL), CC_E_NULL);

    /* five turns, 3 events each, seq 1..15 */
    memset(&h, 0, sizeof h);
    h.n_turns = 5;
    for (unsigned i = 0; i < 5; i++) h.turns[i] = mk_turn(1, (uint16_t)(i + 1), 1 + 3 * i, 3);
    CHECK_STATUS(cc_history_validate(&h), CC_OK);
    h.n_turns = 6;
    CHECK_STATUS(cc_history_validate(&h), CC_E_COUNT);                 /* the sixth turn does not fit */
    h.n_turns = 5;

    h.flags = CC_HF_BEGAN_MID;
    CHECK_STATUS(cc_history_validate(&h), CC_OK);
    h.flags = 2;
    CHECK_STATUS(cc_history_validate(&h), CC_E_UNKNOWN_BITS);
    h.flags = 0;

    /* turn keys: the same turn twice, then going back */
    h.turns[3].turn = h.turns[2].turn;
    CHECK_STATUS(cc_history_validate(&h), CC_E_DUPLICATE);
    h.turns[3].turn = 1;
    CHECK_STATUS(cc_history_validate(&h), CC_E_ORDER);
    h.turns[3].turn = 4;

    /* a new battle may restart the turn counter; equal numbers in different battles never merge */
    h.turns[3].battle_seq = 2; h.turns[3].turn = 1;
    h.turns[4].battle_seq = 2; h.turns[4].turn = 1;
    CHECK_STATUS(cc_history_validate(&h), CC_E_DUPLICATE);
    h.turns[4].turn = 2;
    CHECK_STATUS(cc_history_validate(&h), CC_OK);
    h.turns[3].battle_seq = 1; h.turns[3].turn = 4;
    h.turns[4].battle_seq = 1; h.turns[4].turn = 5;

    /* event order across turns */
    h.turns[2].events[0].seq = h.turns[1].events[2].seq;               /* repeats the previous turn's last */
    CHECK_STATUS(cc_history_validate(&h), CC_E_DUPLICATE);
    h.turns[2].events[0].seq = 2;                                      /* goes back */
    CHECK_STATUS(cc_history_validate(&h), CC_E_ORDER);
    h.turns[2].events[0].seq = 7;

    /* an empty turn in the middle does not reset the order tracking */
    h.turns[2].n_events = 0;
    h.turns[3].events[0].seq = 3;
    CHECK_STATUS(cc_history_validate(&h), CC_E_ORDER);
    h.turns[3].events[0].seq = 10;
    CHECK_STATUS(cc_history_validate(&h), CC_OK);

    /* an invalid turn is reported as itself */
    h.turns[0].wave = 0;
    CHECK_STATUS(cc_history_validate(&h), CC_E_RANGE);
}

/* ------------------------------------------------------------------ terminal summary */

static void test_terminal(void)
{
    cc_terminal t = mk_terminal(10, 4);
    CHECK_STATUS(cc_terminal_validate(&t, true), CC_OK);
    CHECK_STATUS(cc_terminal_validate(NULL, true), CC_E_NULL);
    CHECK_STATUS(cc_terminal_validate(&t, false), CC_E_RANGE);          /* a candidate has no transaction id yet */
    t.txn_id = 0;
    CHECK_STATUS(cc_terminal_validate(&t, false), CC_OK);
    CHECK_STATUS(cc_terminal_validate(&t, true), CC_E_RANGE);

    t = mk_terminal(0, 4);   CHECK_STATUS(cc_terminal_validate(&t, true), CC_E_RANGE);
    t = mk_terminal(10, 4);  t.outcome = CC_OUT_NONE;      CHECK_STATUS(cc_terminal_validate(&t, true), CC_E_RANGE);
    t = mk_terminal(10, 4);  t.outcome = CC_OUT_COUNT_;    CHECK_STATUS(cc_terminal_validate(&t, true), CC_E_RANGE);
    t = mk_terminal(10, 4);  t.known |= 8;                 CHECK_STATUS(cc_terminal_validate(&t, true), CC_E_UNKNOWN_BITS);
    t = mk_terminal(10, 4);  t.biome = 50;                 CHECK_STATUS(cc_terminal_validate(&t, true), CC_OK);
    t = mk_terminal(10, 4);  t.biome = 51;                 CHECK_STATUS(cc_terminal_validate(&t, true), CC_E_RANGE);
    t = mk_terminal(10, 4);  t.wave = 0;                   CHECK_STATUS(cc_terminal_validate(&t, true), CC_E_RANGE);
    t = mk_terminal(10, 4);  t.wave = 65535;               CHECK_STATUS(cc_terminal_validate(&t, true), CC_OK);

    /* only a defeat has a finisher and a fallen Pokemon */
    for (uint8_t o = CC_OUT_CLEAR; o <= CC_OUT_RESET; o++) {
        t = mk_terminal(10, 4); t.outcome = o;
        CHECK_STATUS(cc_terminal_validate(&t, true), CC_E_CONSISTENCY);
        memset(&t.finish_actor, 0, sizeof t.finish_actor);
        memset(&t.fallen_actor, 0, sizeof t.fallen_actor);
        t.known = 0; t.finish_move = 0;
        CHECK_STATUS(cc_terminal_validate(&t, true), CC_OK);
    }
    /* a defeat whose cause is unknown is a valid, useful report */
    t = mk_terminal(10, 4); t.known = 0; t.finish_move = 0;
    memset(&t.finish_actor, 0, sizeof t.finish_actor);
    memset(&t.fallen_actor, 0, sizeof t.fallen_actor);
    CHECK_STATUS(cc_terminal_validate(&t, true), CC_OK);

    /* known values valid; unknown values zero */
    t = mk_terminal(10, 4); t.finish_move = 0;             CHECK_STATUS(cc_terminal_validate(&t, true), CC_E_RANGE);
    t = mk_terminal(10, 4); t.finish_move = 921;           CHECK_STATUS(cc_terminal_validate(&t, true), CC_E_RANGE);
    t = mk_terminal(10, 4); t.known &= (uint8_t)~CC_TK_FINISH_MOVE;
    CHECK_STATUS(cc_terminal_validate(&t, true), CC_E_NONCANONICAL);   /* the move is still there */
    t = mk_terminal(10, 4); t.finish_actor.side = 2;       CHECK_STATUS(cc_terminal_validate(&t, true), CC_E_RANGE);
    t = mk_terminal(10, 4); t.finish_actor.id = 0;         CHECK_STATUS(cc_terminal_validate(&t, true), CC_E_RANGE);
    t = mk_terminal(10, 4); t.fallen_actor.level = 0;      CHECK_STATUS(cc_terminal_validate(&t, true), CC_E_RANGE);
    t = mk_terminal(10, 4); t.known &= (uint8_t)~CC_TK_FINISH_ACTOR;
    CHECK_STATUS(cc_terminal_validate(&t, true), CC_E_NONCANONICAL);
    t = mk_terminal(10, 4); t.known &= (uint8_t)~CC_TK_FALLEN_ACTOR;
    CHECK_STATUS(cc_terminal_validate(&t, true), CC_E_NONCANONICAL);
    t = mk_terminal(10, 4); t.known = CC_TK_FINISH_MOVE | CC_TK_FALLEN_ACTOR;
    memset(&t.finish_actor, 0, sizeof t.finish_actor);
    CHECK_STATUS(cc_terminal_validate(&t, true), CC_OK);
    t.finish_actor.level = 1;                        CHECK_STATUS(cc_terminal_validate(&t, true), CC_E_NONCANONICAL);
    t.finish_actor.level = 0; t.finish_actor.form = 1; CHECK_STATUS(cc_terminal_validate(&t, true), CC_E_NONCANONICAL);
    t.finish_actor.form = 0; t.finish_actor.side = 1;  CHECK_STATUS(cc_terminal_validate(&t, true), CC_E_NONCANONICAL);
    t.finish_actor.side = 0; t.finish_actor.species = 1; CHECK_STATUS(cc_terminal_validate(&t, true), CC_E_NONCANONICAL);
    t.finish_actor.species = 0; t.finish_actor.id = 1; CHECK_STATUS(cc_terminal_validate(&t, true), CC_E_NONCANONICAL);
    t = mk_terminal(10, 4); t.known = CC_TK_FINISH_MOVE | CC_TK_FINISH_ACTOR;
    memset(&t.fallen_actor, 0, sizeof t.fallen_actor);
    CHECK_STATUS(cc_terminal_validate(&t, true), CC_OK);
    t.fallen_actor.level = 1;                        CHECK_STATUS(cc_terminal_validate(&t, true), CC_E_NONCANONICAL);
    t.fallen_actor.level = 0; t.fallen_actor.id = 1;  CHECK_STATUS(cc_terminal_validate(&t, true), CC_E_NONCANONICAL);

    cc_actor a = mk_actor(1, 8901, CC_SIDE_ENEMY);
    CHECK_STATUS(cc_actor_validate(&a), CC_OK);
    CHECK_STATUS(cc_actor_validate(NULL), CC_E_NULL);
    a.species = 8902; CHECK_STATUS(cc_actor_validate(&a), CC_E_RANGE);
    a = mk_actor(1, 1, CC_SIDE_PLAYER); a.level = 255; CHECK_STATUS(cc_actor_validate(&a), CC_OK);
}

/* ------------------------------------------------------------------ Nemesis records */

static void test_nemesis_fields(void)
{
    cc_nemesis n = mk_nem(1, 10, 900);
    CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);
    CHECK_STATUS(cc_nemesis_validate_stored(&n), CC_OK);
    CHECK_STATUS(cc_nemesis_validate(NULL), CC_E_NULL);

    n = mk_nem(0, 10, 900);                      CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 900); n.life = CC_LIFE_NONE;   CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 900); n.life = CC_LIFE_COUNT_; CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 900); n.known |= 1u << 10;     CHECK_STATUS(cc_nemesis_validate(&n), CC_E_UNKNOWN_BITS);
    n = mk_nem(1, 0, 900);                       CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 0);                        CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 900); n.source_wave = 0;       CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 900); n.source_biome = 51;     CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 900); n.species = 0;           CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 900); n.species = 8902;        CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 900); n.gender = -1;           CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);
    n = mk_nem(1, 10, 900); n.gender = 2;            CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 900); n.gender = -2;           CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 900); n.shiny = 2;             CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 900); n.orig_level = 0;        CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);

    /* nature */
    n = mk_nem(1, 10, 900); n.nature = 24;           CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);
    n = mk_nem(1, 10, 900); n.nature = 25;           CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 900); n.known &= (uint16_t)~CC_NK_NATURE;
    CHECK_STATUS(cc_nemesis_validate(&n), CC_E_NONCANONICAL);          /* nature 4 left behind */
    n.nature = 0;                                    CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);
    /* ability: 1..319 */
    n = mk_nem(1, 10, 900); n.ability = 0;           CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 900); n.ability = 319;         CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);
    n = mk_nem(1, 10, 900); n.ability = 320;         CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 900); n.known &= (uint16_t)~CC_NK_ABILITY;
    CHECK_STATUS(cc_nemesis_validate(&n), CC_E_NONCANONICAL);
    /* passive: 0 is "known to have none" */
    n = mk_nem(1, 10, 900); n.passive = 320;         CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 900); n.passive = 7;           CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);
    n = mk_nem(1, 10, 900); n.known &= (uint16_t)~CC_NK_PASSIVE; n.passive = 7;
    CHECK_STATUS(cc_nemesis_validate(&n), CC_E_NONCANONICAL);
    /* IVs */
    n = mk_nem(1, 10, 900); n.ivs[5] = 31;           CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);
    n = mk_nem(1, 10, 900); n.ivs[5] = 32;           CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 900); n.known &= (uint16_t)~CC_NK_IVS;
    CHECK_STATUS(cc_nemesis_validate(&n), CC_E_NONCANONICAL);

    /* moves */
    n = mk_nem(1, 10, 900); n.n_moves = 4; n.moves[2] = 1; n.moves[3] = 920;
    CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);                      /* four moves, the highest id */
    n.moves[3] = 921;                                CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 900); n.n_moves = 5;           CHECK_STATUS(cc_nemesis_validate(&n), CC_E_COUNT);
    n = mk_nem(1, 10, 900); n.n_moves = 0;           CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 900); n.moves[1] = n.moves[0]; CHECK_STATUS(cc_nemesis_validate(&n), CC_E_DUPLICATE);
    n = mk_nem(1, 10, 900); n.moves[1] = 0;          CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 900); n.moves[2] = 5;          CHECK_STATUS(cc_nemesis_validate(&n), CC_E_NONCANONICAL);
    n = mk_nem(1, 10, 900); n.known &= (uint16_t)~CC_NK_MOVES;
    CHECK_STATUS(cc_nemesis_validate(&n), CC_E_NONCANONICAL);          /* n_moves still 2 */
    n.n_moves = 0;                                   CHECK_STATUS(cc_nemesis_validate(&n), CC_E_NONCANONICAL);   /* moves still set */
    n.moves[0] = n.moves[1] = 0;                     CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);
    n = mk_nem(1, 10, 900); n.known &= (uint16_t)~CC_NK_MOVES; memset(n.moves, 0, sizeof n.moves);
    CHECK_STATUS(cc_nemesis_validate(&n), CC_E_NONCANONICAL);          /* unknown moves, n_moves still 2 */

    /* loadout */
    n = mk_nem(1, 10, 900); n.n_items = 0; memset(n.items, 0, sizeof n.items);
    CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);                      /* known to hold nothing */
    n = mk_nem(1, 10, 900); n.n_items = 9;           CHECK_STATUS(cc_nemesis_validate(&n), CC_E_COUNT);
    n = mk_nem(1, 10, 900); n.known &= (uint16_t)~CC_NK_LOADOUT;
    CHECK_STATUS(cc_nemesis_validate(&n), CC_E_NONCANONICAL);
    n = mk_nem(1, 10, 900); n.items[1].stack = 1;    CHECK_STATUS(cc_nemesis_validate(&n), CC_E_NONCANONICAL);   /* beyond n_items */
    n = mk_nem(1, 10, 900); n.items[1].arg = 1;      CHECK_STATUS(cc_nemesis_validate(&n), CC_E_NONCANONICAL);   /* beyond n_items */
    n = mk_nem(1, 10, 900); n.items[1].key[0] = 'A'; CHECK_STATUS(cc_nemesis_validate(&n), CC_E_NONCANONICAL);
    n = mk_nem(1, 10, 900); n.items[7].key[32] = 'A'; CHECK_STATUS(cc_nemesis_validate(&n), CC_E_NONCANONICAL);
    n = mk_nem(1, 10, 900); n.known &= (uint16_t)~CC_NK_LOADOUT; memset(n.items, 0, sizeof n.items);
    CHECK_STATUS(cc_nemesis_validate(&n), CC_E_NONCANONICAL);          /* unknown loadout, n_items still 1 */
    n.n_items = 0;                                   CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);
    n = mk_nem(1, 10, 900); n.known &= (uint16_t)~CC_NK_LOADOUT; n.n_items = 0;
    CHECK_STATUS(cc_nemesis_validate(&n), CC_E_NONCANONICAL);          /* unknown loadout, item 0 still there */
    n = mk_nem(1, 10, 900); n.items[0].key[0] = 'a'; CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);          /* lower case */
    n = mk_nem(1, 10, 900); n.items[0].key[3] = ' '; CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 900); n.items[0].key[0] = 0;   CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);          /* empty key */
    n = mk_nem(1, 10, 900); n.items[0].key[20] = 'X'; CHECK_STATUS(cc_nemesis_validate(&n), CC_E_NONCANONICAL); /* after the NUL */
    n = mk_nem(1, 10, 900); memset(n.items[0].key, 'A', CC_ITEM_KEY_BYTES);
    CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);                 /* 33 characters: no terminator */
    n = mk_nem(1, 10, 900); memset(n.items[0].key, 0, CC_ITEM_KEY_BYTES); memset(n.items[0].key, 'B', 32);
    CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);                      /* 32 characters, the longest allowed */
    n = mk_nem(1, 10, 900); memset(n.items[0].key, 0, CC_ITEM_KEY_BYTES); strcpy(n.items[0].key, "TM_1_2"); CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);
    {
        /* every character class at both ends of its range is accepted... */
        static const char *ok_keys[] = {"A", "Z", "0", "9", "_", "AZ09_", "ZZZZ", "TM_99"};
        for (unsigned i = 0; i < sizeof ok_keys / sizeof ok_keys[0]; i++) {
            n = mk_nem(1, 10, 900); memset(n.items[0].key, 0, CC_ITEM_KEY_BYTES); strcpy(n.items[0].key, ok_keys[i]);
            CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);
        }
        /* ...and the characters just outside them are not */
        static const char outside[] = {'@', '[', '/', ':', '`', '{', '-', '\x7f', '\x01', '\xc3'};
        for (unsigned i = 0; i < sizeof outside; i++) {
            n = mk_nem(1, 10, 900); memset(n.items[0].key, 0, CC_ITEM_KEY_BYTES); strcpy(n.items[0].key, "AB");
            n.items[0].key[1] = outside[i];
            CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
        }
    }
    n = mk_nem(1, 10, 900); n.items[0].arg = -2;     CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 900); n.items[0].arg = 32767;  CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);
    n = mk_nem(1, 10, 900); n.items[0].stack = 0;    CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 900); n.n_items = 8;
    for (int i = 0; i < 8; i++) { memset(&n.items[i], 0, sizeof n.items[i]); strcpy(n.items[i].key, "BERRY"); n.items[i].arg = (int16_t)i; n.items[i].stack = 1; }
    CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);                      /* a full loadout */
    n.known |= CC_NK_LOADOUT_TRUNCATED;              CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);               /* and more were held */
    n.n_items = 7; memset(&n.items[7], 0, sizeof n.items[7]);
    CHECK_STATUS(cc_nemesis_validate(&n), CC_E_CONSISTENCY);           /* truncated, yet room left */
    n = mk_nem(1, 10, 900); n.known &= (uint16_t)~CC_NK_LOADOUT; n.n_items = 0; memset(n.items, 0, sizeof n.items);
    n.known |= CC_NK_LOADOUT_TRUNCATED;              CHECK_STATUS(cc_nemesis_validate(&n), CC_E_CONSISTENCY);    /* truncated, nothing kept */

    /* defeat summary */
    n = mk_nem(1, 10, 900); n.finish_move = 0;       CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 900); n.finish_move = 920;     CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);
    n = mk_nem(1, 10, 900); n.finish_move = 921;     CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 900); n.known &= (uint16_t)~CC_NK_FINISH_MOVE;
    CHECK_STATUS(cc_nemesis_validate(&n), CC_E_NONCANONICAL);
    n = mk_nem(1, 10, 900); n.fallen_species = 0;    CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 900); n.fallen_species = 8901; CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);
    n = mk_nem(1, 10, 900); n.fallen_species = 8902; CHECK_STATUS(cc_nemesis_validate(&n), CC_E_RANGE);
    n = mk_nem(1, 10, 900); n.known &= (uint16_t)~CC_NK_FALLEN;
    CHECK_STATUS(cc_nemesis_validate(&n), CC_E_NONCANONICAL);
    n.fallen_species = 0; n.fallen_form = 3;         CHECK_STATUS(cc_nemesis_validate(&n), CC_E_NONCANONICAL);
    n.fallen_form = 0;                               CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);
}

static void test_lifecycle_fields(void)
{
    cc_nemesis n;
    /* STORED holds no bookkeeping */
    n = mk_nem(1, 10, 900); n.sched_run_uid = 10;    CHECK_STATUS(cc_nemesis_validate(&n), CC_E_CONSISTENCY);
    n = mk_nem(1, 10, 900); n.challenge_wave = 27;   CHECK_STATUS(cc_nemesis_validate(&n), CC_E_CONSISTENCY);
    n = mk_nem(1, 10, 900); n.reward_txn = 3;        CHECK_STATUS(cc_nemesis_validate(&n), CC_E_CONSISTENCY);
    /* SCHEDULED: a run, no wave yet */
    n = mk_nem(1, 10, 900); n.life = CC_LIFE_SCHEDULED;
    CHECK_STATUS(cc_nemesis_validate(&n), CC_E_CONSISTENCY);           /* no run */
    n.sched_run_uid = 11;                            CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);
    n.challenge_wave = 27;                           CHECK_STATUS(cc_nemesis_validate(&n), CC_E_CONSISTENCY);
    n.challenge_wave = 0; n.reward_txn = 5;          CHECK_STATUS(cc_nemesis_validate(&n), CC_E_CONSISTENCY);    /* no reward yet */
    /* CHALLENGED and DEFEATED: a run and one of the four window waves, no reward */
    const uint16_t good[4] = {27, 77, 127, 177};
    const uint16_t bad[5] = {0, 26, 28, 178, 200};
    for (int s = 0; s < 2; s++) {
        const uint8_t life = s ? CC_LIFE_DEFEATED : CC_LIFE_CHALLENGED;
        for (int i = 0; i < 4; i++) {
            n = mk_nem(1, 10, 900); n.life = life; n.sched_run_uid = 11; n.challenge_wave = good[i];
            CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);
        }
        for (int i = 0; i < 5; i++) {
            n = mk_nem(1, 10, 900); n.life = life; n.sched_run_uid = 11; n.challenge_wave = bad[i];
            CHECK_STATUS(cc_nemesis_validate(&n), CC_E_CONSISTENCY);
        }
        n = mk_nem(1, 10, 900); n.life = life; n.sched_run_uid = 0; n.challenge_wave = 27;
        CHECK_STATUS(cc_nemesis_validate(&n), CC_E_CONSISTENCY);
        n = mk_nem(1, 10, 900); n.life = life; n.sched_run_uid = 11; n.challenge_wave = 27; n.reward_txn = 5;
        CHECK_STATUS(cc_nemesis_validate(&n), CC_E_CONSISTENCY);
    }
    /* REWARD_CLAIMED needs the transaction */
    n = mk_nem(1, 10, 900); n.life = CC_LIFE_REWARD_CLAIMED; n.sched_run_uid = 11; n.challenge_wave = 77;
    CHECK_STATUS(cc_nemesis_validate(&n), CC_E_CONSISTENCY);
    n.reward_txn = 5;                                CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);
}

static void test_support_and_gate(void)
{
    cc_nemesis n = mk_nem(1, 10, 900);
    CHECK_EQ(cc_nemesis_support(&n), CC_SUPPORT_OK);
    CHECK_EQ(cc_nemesis_support(NULL), CC_REFUSE_NOT_ATTRIBUTED);

    /* each trait is refused for its own reason */
    const struct { uint8_t bit; int want; } traits[] = {
        {CC_TR_BOSS, CC_REFUSE_BOSS}, {CC_TR_FUSED, CC_REFUSE_FUSED}, {CC_TR_TERA, CC_REFUSE_TERA},
        {CC_TR_TRAINER_OWNED, CC_REFUSE_TRAINER_OWNED},
        {CC_TR_MYSTERY, CC_REFUSE_MYSTERY}, {CC_TR_FIXED_SCRIPTED, CC_REFUSE_FIXED_SCRIPTED},
        {CC_TR_REMATCH, CC_REFUSE_REMATCH},
    };
    for (unsigned i = 0; i < sizeof traits / sizeof *traits; i++) {
        n = mk_nem(1, 10, 900); n.traits = traits[i].bit;
        CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);                  /* representable... */
        CHECK_EQ(cc_nemesis_support(&n), traits[i].want);              /* ...but refused */
        CHECK_STATUS(cc_nemesis_validate_stored(&n), CC_E_CONSISTENCY);/* and so never a stored record */
    }
    /* Doubles describes the source encounter; it does not make an exact killer
     * unsuitable for a later single rematch. Attribution requirements remain. */
    n = mk_nem(1, 10, 900); n.traits = CC_TR_DOUBLES;
    CHECK_EQ(cc_nemesis_support(&n), CC_SUPPORT_OK);
    CHECK_STATUS(cc_nemesis_validate_stored(&n), CC_OK);
    n.known &= (uint16_t)~CC_NK_FINISHER_VERIFIED;
    CHECK_EQ(cc_nemesis_support(&n), CC_REFUSE_NOT_ATTRIBUTED);
    n = mk_nem(1, 10, 900); n.traits = CC_TR_DOUBLES | CC_TR_BOSS;
    CHECK_EQ(cc_nemesis_support(&n), CC_REFUSE_BOSS);
    /* several traits: the first listed wins */
    n = mk_nem(1, 10, 900); n.traits = CC_TR_TERA | CC_TR_BOSS;
    CHECK_EQ(cc_nemesis_support(&n), CC_REFUSE_BOSS);

    n = mk_nem(1, 10, 900); n.known &= (uint16_t)~CC_NK_FINISHER_VERIFIED;
    CHECK_EQ(cc_nemesis_support(&n), CC_REFUSE_NOT_ATTRIBUTED);
    n = mk_nem(1, 10, 900); n.known &= (uint16_t)~CC_NK_ABILITY; n.ability = 0;
    CHECK_EQ(cc_nemesis_support(&n), CC_REFUSE_ABILITY_UNKNOWN);
    n = mk_nem(1, 10, 900); n.known &= (uint16_t)~CC_NK_MOVES; n.n_moves = 0; memset(n.moves, 0, sizeof n.moves);
    CHECK_EQ(cc_nemesis_support(&n), CC_REFUSE_MOVES_UNKNOWN);
    /* IVs, nature, passive and loadout are history: not needing them is not a refusal */
    n = mk_nem(1, 10, 900); n.known = CC_NK_ABILITY | CC_NK_MOVES | CC_NK_FINISHER_VERIFIED;
    n.nature = 0; n.passive = 0; memset(n.ivs, 0, sizeof n.ivs); n.n_items = 0; memset(n.items, 0, sizeof n.items);
    n.finish_move = 0; n.fallen_species = 0;
    CHECK_STATUS(cc_nemesis_validate_stored(&n), CC_OK);

    /* the insertion gate */
    cc_terminal t = mk_terminal(10, 0);
    n = mk_nem(1, 10, 900);                          /* matches mk_terminal: enemy 900, species 19, run 10, wave 27, biome 3 */
    CHECK_EQ(cc_insertion_check(&t, &n), CC_SUPPORT_OK);
    CHECK_EQ(cc_insertion_check(NULL, &n), CC_REFUSE_NOT_ATTRIBUTED);
    CHECK_EQ(cc_insertion_check(&t, NULL), CC_REFUSE_NOT_ATTRIBUTED);
    for (uint8_t o = CC_OUT_CLEAR; o <= CC_OUT_RESET; o++) {
        t = mk_terminal(10, 0); t.outcome = o;
        CHECK_EQ(cc_insertion_check(&t, &n), CC_REFUSE_NOT_DEFEAT);    /* clear, abandon and reset create no record */
    }
    t = mk_terminal(10, 0); t.known &= (uint8_t)~CC_TK_FINISH_ACTOR;
    CHECK_EQ(cc_insertion_check(&t, &n), CC_REFUSE_NOT_ATTRIBUTED);    /* an unattributed defeat creates no record */
    t = mk_terminal(10, 0); t.finish_actor.side = CC_SIDE_PLAYER;
    CHECK_EQ(cc_insertion_check(&t, &n), CC_REFUSE_NOT_ATTRIBUTED);
    t = mk_terminal(10, 0); t.finish_actor.id = 901;
    CHECK_EQ(cc_insertion_check(&t, &n), CC_REFUSE_NOT_ATTRIBUTED);    /* another enemy finished it */
    t = mk_terminal(10, 0); t.finish_actor.species = 20;
    CHECK_EQ(cc_insertion_check(&t, &n), CC_REFUSE_NOT_ATTRIBUTED);
    t = mk_terminal(11, 0);
    CHECK_EQ(cc_insertion_check(&t, &n), CC_REFUSE_NOT_ATTRIBUTED);    /* a different run */
    t = mk_terminal(10, 0); t.wave = 28;
    CHECK_EQ(cc_insertion_check(&t, &n), CC_REFUSE_NOT_ATTRIBUTED);
    t = mk_terminal(10, 0); t.biome = 4;
    CHECK_EQ(cc_insertion_check(&t, &n), CC_REFUSE_NOT_ATTRIBUTED);
    /* attribution holds, so the enemy's own limits are what refuse it */
    t = mk_terminal(10, 0); n = mk_nem(1, 10, 900); n.traits = CC_TR_BOSS;
    CHECK_EQ(cc_insertion_check(&t, &n), CC_REFUSE_BOSS);
    n = mk_nem(1, 10, 900); n.known &= (uint16_t)~CC_NK_MOVES; n.n_moves = 0; memset(n.moves, 0, sizeof n.moves);
    CHECK_EQ(cc_insertion_check(&t, &n), CC_REFUSE_MOVES_UNKNOWN);
}

static cc_roster g_roster;

static void fill_roster(unsigned count)
{
    memset(&g_roster, 0, sizeof g_roster);
    g_roster.count = (uint8_t)count;
    for (unsigned i = 0; i < count; i++) g_roster.rec[i] = mk_nem(1 + i, 10, 900 + i);
}

static void test_roster(void)
{
    fill_roster(0);
    CHECK_STATUS(cc_roster_validate(&g_roster), CC_OK);
    CHECK_STATUS(cc_roster_validate(NULL), CC_E_NULL);
    fill_roster(1);
    CHECK_STATUS(cc_roster_validate(&g_roster), CC_OK);
    fill_roster(16);
    CHECK_STATUS(cc_roster_validate(&g_roster), CC_OK);                /* full */
    g_roster.count = 17;
    CHECK_STATUS(cc_roster_validate(&g_roster), CC_E_COUNT);           /* one over */
    g_roster.count = 255;
    CHECK_STATUS(cc_roster_validate(&g_roster), CC_E_COUNT);

    fill_roster(3);
    g_roster.rec[1].record_id = 1;                                     /* repeats the first */
    CHECK_STATUS(cc_roster_validate(&g_roster), CC_E_DUPLICATE);
    fill_roster(3);
    g_roster.rec[2].record_id = 1;                                     /* ... after a gap, going back */
    g_roster.rec[1].record_id = 5;
    CHECK_STATUS(cc_roster_validate(&g_roster), CC_E_ORDER);
    fill_roster(3);
    g_roster.rec[2].source_pokemon_id = g_roster.rec[0].source_pokemon_id;  /* same enemy of the same run twice */
    CHECK_STATUS(cc_roster_validate(&g_roster), CC_E_DUPLICATE);
    fill_roster(3);
    g_roster.rec[2].source_run_uid = 11;                               /* the same Pokemon id in another run is another enemy */
    g_roster.rec[2].source_pokemon_id = g_roster.rec[0].source_pokemon_id;
    CHECK_STATUS(cc_roster_validate(&g_roster), CC_OK);

    /* at most one record belongs to the run in progress */
    fill_roster(3);
    g_roster.rec[0].life = CC_LIFE_SCHEDULED; g_roster.rec[0].sched_run_uid = 11;
    CHECK_STATUS(cc_roster_validate(&g_roster), CC_OK);
    g_roster.rec[1].life = CC_LIFE_CHALLENGED; g_roster.rec[1].sched_run_uid = 11; g_roster.rec[1].challenge_wave = 27;
    CHECK_STATUS(cc_roster_validate(&g_roster), CC_E_CONSISTENCY);
    g_roster.rec[1].life = CC_LIFE_DEFEATED;                           /* a won rematch is not "in progress" */
    CHECK_STATUS(cc_roster_validate(&g_roster), CC_OK);

    /* a stored record is a supported one */
    fill_roster(2);
    g_roster.rec[1].traits = CC_TR_FUSED;
    CHECK_STATUS(cc_roster_validate(&g_roster), CC_E_CONSISTENCY);
    fill_roster(2);
    g_roster.rec[1].species = 0;
    CHECK_STATUS(cc_roster_validate(&g_roster), CC_E_RANGE);
}

/* ------------------------------------------------------------------ lifecycle */

static void test_lifecycle(void)
{
    /* the whole transition table, written out by hand: rows are "from", columns "to":
     * NONE STORED SCHEDULED CHALLENGED DEFEATED REWARD_CLAIMED */
    static const int allowed[6][6] = {
        /* NONE           */ {0, 0, 0, 0, 0, 0},
        /* STORED         */ {0, 0, 1, 0, 0, 0},
        /* SCHEDULED      */ {0, 1, 0, 1, 0, 0},
        /* CHALLENGED     */ {0, 1, 0, 0, 1, 0},
        /* DEFEATED       */ {0, 0, 0, 0, 0, 1},
        /* REWARD_CLAIMED */ {0, 0, 0, 0, 0, 0},
    };
    for (int f = 0; f < 6; f++)
        for (int t = 0; t < 6; t++) CHECK_EQ(cc_life_can_transition((uint8_t)f, (uint8_t)t), allowed[f][t]);
    CHECK(!cc_life_can_transition(6, 1));
    CHECK(!cc_life_can_transition(1, 6));
    CHECK(!cc_life_can_transition(255, 255));

    CHECK(!cc_life_overwritable(CC_LIFE_NONE));
    CHECK(cc_life_overwritable(CC_LIFE_STORED));
    CHECK(!cc_life_overwritable(CC_LIFE_SCHEDULED));
    CHECK(!cc_life_overwritable(CC_LIFE_CHALLENGED));
    CHECK(!cc_life_overwritable(CC_LIFE_DEFEATED));                    /* an unclaimed reward is not evicted */
    CHECK(cc_life_overwritable(CC_LIFE_REWARD_CLAIMED));
    CHECK(!cc_life_overwritable(99));

    /* the happy path with its bookkeeping */
    cc_nemesis n = mk_nem(1, 10, 900);
    CHECK_STATUS(cc_nemesis_transition(&n, CC_LIFE_SCHEDULED, 11, 0, 0), CC_OK);
    CHECK_EQ(n.life, CC_LIFE_SCHEDULED);
    CHECK_EQ(n.sched_run_uid, 11);
    CHECK_STATUS(cc_nemesis_transition(&n, CC_LIFE_CHALLENGED, 11, 77, 0), CC_OK);
    CHECK_EQ(n.challenge_wave, 77);
    CHECK_STATUS(cc_nemesis_transition(&n, CC_LIFE_DEFEATED, 0, 0, 0), CC_OK);
    CHECK_EQ(n.life, CC_LIFE_DEFEATED);
    CHECK_STATUS(cc_nemesis_transition(&n, CC_LIFE_REWARD_CLAIMED, 0, 0, 42), CC_OK);
    CHECK_EQ(n.reward_txn, 42);
    CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);

    /* a second claim is refused and changes nothing */
    cc_nemesis before;
    memcpy(&before, &n, sizeof n);
    CHECK_STATUS(cc_nemesis_transition(&n, CC_LIFE_REWARD_CLAIMED, 0, 0, 43), CC_E_LIFECYCLE);
    CHECK(memcmp(&before, &n, sizeof n) == 0);
    CHECK_EQ(n.reward_txn, 42);
    CHECK_STATUS(cc_nemesis_transition(&n, CC_LIFE_STORED, 0, 0, 0), CC_E_LIFECYCLE);   /* terminal */
    CHECK(memcmp(&before, &n, sizeof n) == 0);

    /* argument checks, each leaving the record untouched */
    n = mk_nem(1, 10, 900);
    memcpy(&before, &n, sizeof n);
    CHECK_STATUS(cc_nemesis_transition(&n, CC_LIFE_SCHEDULED, 0, 0, 0), CC_E_RANGE);         /* needs a run */
    CHECK_STATUS(cc_nemesis_transition(&n, CC_LIFE_CHALLENGED, 11, 27, 0), CC_E_LIFECYCLE);  /* skips SCHEDULED */
    CHECK_STATUS(cc_nemesis_transition(&n, CC_LIFE_DEFEATED, 0, 0, 0), CC_E_LIFECYCLE);
    CHECK_STATUS(cc_nemesis_transition(&n, CC_LIFE_NONE, 0, 0, 0), CC_E_LIFECYCLE);
    CHECK_STATUS(cc_nemesis_transition(&n, 77, 0, 0, 0), CC_E_LIFECYCLE);
    CHECK_STATUS(cc_nemesis_transition(NULL, CC_LIFE_SCHEDULED, 11, 0, 0), CC_E_NULL);
    CHECK(memcmp(&before, &n, sizeof n) == 0);
    CHECK_STATUS(cc_nemesis_transition(&n, CC_LIFE_SCHEDULED, 11, 0, 0), CC_OK);
    memcpy(&before, &n, sizeof n);
    CHECK_STATUS(cc_nemesis_transition(&n, CC_LIFE_CHALLENGED, 11, 28, 0), CC_E_RANGE);      /* not a window wave */
    CHECK_STATUS(cc_nemesis_transition(&n, CC_LIFE_CHALLENGED, 12, 27, 0), CC_E_CONSISTENCY);/* another run */
    CHECK(memcmp(&before, &n, sizeof n) == 0);
    CHECK_STATUS(cc_nemesis_transition(&n, CC_LIFE_CHALLENGED, 11, 177, 0), CC_OK);
    CHECK_STATUS(cc_nemesis_transition(&n, CC_LIFE_DEFEATED, 0, 0, 0), CC_OK);
    memcpy(&before, &n, sizeof n);
    CHECK_STATUS(cc_nemesis_transition(&n, CC_LIFE_REWARD_CLAIMED, 0, 0, 0), CC_E_RANGE);    /* needs the transaction */
    CHECK(memcmp(&before, &n, sizeof n) == 0);

    /* an unclaimed or abandoned rematch goes back to STORED with its bookkeeping cleared */
    n = mk_nem(1, 10, 900);
    CHECK_STATUS(cc_nemesis_transition(&n, CC_LIFE_SCHEDULED, 11, 0, 0), CC_OK);
    CHECK_STATUS(cc_nemesis_transition(&n, CC_LIFE_STORED, 0, 0, 0), CC_OK);
    CHECK_EQ(n.sched_run_uid, 0);
    CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);
    CHECK_STATUS(cc_nemesis_transition(&n, CC_LIFE_SCHEDULED, 12, 0, 0), CC_OK);
    CHECK_STATUS(cc_nemesis_transition(&n, CC_LIFE_CHALLENGED, 12, 127, 0), CC_OK);
    CHECK_STATUS(cc_nemesis_transition(&n, CC_LIFE_STORED, 0, 0, 0), CC_OK);          /* the rematch was lost */
    CHECK_EQ(n.challenge_wave, 0);
    CHECK_STATUS(cc_nemesis_validate(&n), CC_OK);

    /* a record that is already inconsistent cannot move */
    n = mk_nem(1, 10, 900); n.sched_run_uid = 5;
    CHECK_STATUS(cc_nemesis_transition(&n, CC_LIFE_SCHEDULED, 11, 0, 0), CC_E_CONSISTENCY);
}

/* ------------------------------------------------------------------ schedule */

static cc_schedule mk_sched(uint8_t state)
{
    cc_schedule s;
    memset(&s, 0, sizeof s);
    s.state = state;
    s.run_uid = 11;
    if (state == CC_SCHED_NO_CANDIDATE) return s;
    s.rng_algo = CC_RNG_ALGO_V1;
    s.n_frozen = 3;
    s.frozen_ids[0] = 2; s.frozen_ids[1] = 5; s.frozen_ids[2] = 9;
    s.chosen_id = 5;
    s.rng_seed = 0x1122334455667788ull;
    s.rng_state = 0x99AABBCCDDEEFF00ull;
    s.n_windows = 4;
    for (int i = 0; i < 4; i++) s.windows[i].wave = cc_window_waves[i];
    return s;
}

static void test_schedule(void)
{
    CHECK_EQ(cc_window_waves[0], 27);
    CHECK_EQ(cc_window_waves[1], 77);
    CHECK_EQ(cc_window_waves[2], 127);
    CHECK_EQ(cc_window_waves[3], 177);

    cc_schedule s = mk_sched(CC_SCHED_NO_CANDIDATE);
    CHECK_STATUS(cc_schedule_validate(&s), CC_OK);
    CHECK_STATUS(cc_schedule_validate(NULL), CC_E_NULL);
    s.chosen_id = 5;     CHECK_STATUS(cc_schedule_validate(&s), CC_E_CONSISTENCY);
    s = mk_sched(CC_SCHED_NO_CANDIDATE); s.rng_seed = 1;       CHECK_STATUS(cc_schedule_validate(&s), CC_E_CONSISTENCY);
    s = mk_sched(CC_SCHED_NO_CANDIDATE); s.rng_state = 1;      CHECK_STATUS(cc_schedule_validate(&s), CC_E_CONSISTENCY);
    s = mk_sched(CC_SCHED_NO_CANDIDATE); s.rng_algo = CC_RNG_ALGO_V1; CHECK_STATUS(cc_schedule_validate(&s), CC_E_CONSISTENCY);
    s = mk_sched(CC_SCHED_NO_CANDIDATE); s.n_frozen = 1; s.frozen_ids[0] = 4; CHECK_STATUS(cc_schedule_validate(&s), CC_E_CONSISTENCY);
    s = mk_sched(CC_SCHED_NO_CANDIDATE); s.windows[2].wave = 127; CHECK_STATUS(cc_schedule_validate(&s), CC_E_NONCANONICAL);
    s = mk_sched(CC_SCHED_NO_CANDIDATE); s.windows[1].state = CC_WIN_OFFERED; CHECK_STATUS(cc_schedule_validate(&s), CC_E_NONCANONICAL);
    s = mk_sched(CC_SCHED_NO_CANDIDATE); s.windows[3].reason = CC_SKIP_BOSS;  CHECK_STATUS(cc_schedule_validate(&s), CC_E_NONCANONICAL);
    s = mk_sched(CC_SCHED_NO_CANDIDATE); s.n_windows = 4;                     CHECK_STATUS(cc_schedule_validate(&s), CC_E_CONSISTENCY);
    s = mk_sched(CC_SCHED_NO_CANDIDATE); s.state = CC_SCHED_COUNT_;           CHECK_STATUS(cc_schedule_validate(&s), CC_E_RANGE);

    s = mk_sched(CC_SCHED_ACTIVE);
    CHECK_STATUS(cc_schedule_validate(&s), CC_OK);
    s.windows[0].state = CC_WIN_SKIPPED; s.windows[0].reason = CC_SKIP_TRAINER;
    CHECK_STATUS(cc_schedule_validate(&s), CC_OK);                     /* window 1 skipped, three pending */
    s.windows[1].state = CC_WIN_SKIPPED; s.windows[1].reason = CC_SKIP_MYSTERY;
    s.windows[2].state = CC_WIN_SKIPPED; s.windows[2].reason = CC_SKIP_FIXED_OR_SCRIPTED;
    CHECK_STATUS(cc_schedule_validate(&s), CC_OK);                     /* the last window is still to come */
    s.windows[3].state = CC_WIN_SKIPPED; s.windows[3].reason = CC_SKIP_BOSS;
    CHECK_STATUS(cc_schedule_validate(&s), CC_E_CONSISTENCY);          /* all skipped is EXHAUSTED, not ACTIVE */
    s.state = CC_SCHED_EXHAUSTED;
    CHECK_STATUS(cc_schedule_validate(&s), CC_OK);
    s.windows[3].reason = CC_SKIP_FINAL;                                CHECK_STATUS(cc_schedule_validate(&s), CC_OK);
    s.windows[3].reason = CC_SKIP_UNSUPPORTED;                          CHECK_STATUS(cc_schedule_validate(&s), CC_OK);
    s.windows[3].reason = CC_SKIP_COUNT_;                               CHECK_STATUS(cc_schedule_validate(&s), CC_E_RANGE);
    s.windows[3].reason = CC_SKIP_NONE;                                 CHECK_STATUS(cc_schedule_validate(&s), CC_E_CONSISTENCY);  /* skipped without a reason */
    s = mk_sched(CC_SCHED_EXHAUSTED);                                   CHECK_STATUS(cc_schedule_validate(&s), CC_E_CONSISTENCY);  /* nothing skipped */

    /* OFFERED, DECLINED and ACCEPTED: skipped prefix, the state's window, pending after */
    const uint8_t states[3] = {CC_SCHED_OFFERED, CC_SCHED_DECLINED, CC_SCHED_ACCEPTED};
    const uint8_t wins[3] = {CC_WIN_OFFERED, CC_WIN_DECLINED, CC_WIN_ACCEPTED};
    for (int k = 0; k < 3; k++) {
        for (int at = 0; at < 4; at++) {
            s = mk_sched(states[k]);
            for (int i = 0; i < at; i++) { s.windows[i].state = CC_WIN_SKIPPED; s.windows[i].reason = CC_SKIP_TRAINER; }
            s.windows[at].state = wins[k];
            CHECK_STATUS(cc_schedule_validate(&s), CC_OK);
            if (at < 3) {                                              /* a second offer after the first */
                s.windows[at + 1].state = wins[k];
                CHECK_STATUS(cc_schedule_validate(&s), CC_E_CONSISTENCY);
            }
            if (at > 0) {                                              /* a pending window before the offer */
                s = mk_sched(states[k]);
                s.windows[at].state = wins[k];
                CHECK_STATUS(cc_schedule_validate(&s), CC_E_CONSISTENCY);
            }
        }
        s = mk_sched(states[k]);                                       /* no window in that state at all */
        CHECK_STATUS(cc_schedule_validate(&s), CC_E_CONSISTENCY);
    }
    s = mk_sched(CC_SCHED_DECLINED); s.windows[0].state = CC_WIN_DECLINED; s.windows[3].state = CC_WIN_SKIPPED; s.windows[3].reason = 1;
    CHECK_STATUS(cc_schedule_validate(&s), CC_E_CONSISTENCY);          /* a declined schedule offers nothing later */

    /* identity and draw record */
    s = mk_sched(CC_SCHED_ACTIVE); s.run_uid = 0;                       CHECK_STATUS(cc_schedule_validate(&s), CC_E_RANGE);
    s = mk_sched(CC_SCHED_NONE);                                        CHECK_STATUS(cc_schedule_validate(&s), CC_E_RANGE);
    s = mk_sched(CC_SCHED_ACTIVE); s.state = CC_SCHED_COUNT_;           CHECK_STATUS(cc_schedule_validate(&s), CC_E_RANGE);
    s = mk_sched(CC_SCHED_ACTIVE); s.rng_algo = CC_RNG_ALGO_NONE;       CHECK_STATUS(cc_schedule_validate(&s), CC_E_CONSISTENCY);
    s = mk_sched(CC_SCHED_ACTIVE); s.rng_algo = 2;                      CHECK_STATUS(cc_schedule_validate(&s), CC_E_CONSISTENCY);
    s = mk_sched(CC_SCHED_ACTIVE); s.n_windows = 3;                     CHECK_STATUS(cc_schedule_validate(&s), CC_E_CONSISTENCY);
    s = mk_sched(CC_SCHED_ACTIVE); s.windows[2].wave = 128;             CHECK_STATUS(cc_schedule_validate(&s), CC_E_RANGE);
    s = mk_sched(CC_SCHED_ACTIVE); s.windows[1].state = CC_WIN_COUNT_;  CHECK_STATUS(cc_schedule_validate(&s), CC_E_RANGE);
    s = mk_sched(CC_SCHED_ACTIVE); s.windows[1].reason = CC_SKIP_TRAINER; CHECK_STATUS(cc_schedule_validate(&s), CC_E_CONSISTENCY);  /* a reason on a pending window */
    s = mk_sched(CC_SCHED_ACTIVE); s.chosen_id = 6;                     CHECK_STATUS(cc_schedule_validate(&s), CC_E_CONSISTENCY);  /* not frozen */
    s = mk_sched(CC_SCHED_ACTIVE); s.chosen_id = 0;                     CHECK_STATUS(cc_schedule_validate(&s), CC_E_CONSISTENCY);
    s = mk_sched(CC_SCHED_ACTIVE); s.frozen_ids[1] = 2;                 CHECK_STATUS(cc_schedule_validate(&s), CC_E_DUPLICATE);
    s = mk_sched(CC_SCHED_ACTIVE); s.frozen_ids[1] = 1;                 CHECK_STATUS(cc_schedule_validate(&s), CC_E_ORDER);
    s = mk_sched(CC_SCHED_ACTIVE); s.frozen_ids[0] = 0;                 CHECK_STATUS(cc_schedule_validate(&s), CC_E_RANGE);
    s = mk_sched(CC_SCHED_ACTIVE); s.n_frozen = 17;                     CHECK_STATUS(cc_schedule_validate(&s), CC_E_COUNT);
    s = mk_sched(CC_SCHED_ACTIVE); s.n_frozen = 16;
    for (int i = 0; i < 16; i++) s.frozen_ids[i] = (cc_record_id)(i + 1);
    s.chosen_id = 16;                                                   CHECK_STATUS(cc_schedule_validate(&s), CC_OK);
    s = mk_sched(CC_SCHED_ACTIVE); s.n_frozen = 0;                      CHECK_STATUS(cc_schedule_validate(&s), CC_E_CONSISTENCY);
}

/* ------------------------------------------------------------------ identifiers and the ledger */

static void test_ids(void)
{
    cc_counters c = {1, 1, 0xAABBCCDDu, 1};
    CHECK_STATUS(cc_counters_validate(&c), CC_OK);
    CHECK_STATUS(cc_counters_validate(NULL), CC_E_NULL);
    cc_counters bad = c; bad.next_record_id = 0; CHECK_STATUS(cc_counters_validate(&bad), CC_E_RANGE);
    bad = c; bad.next_run_seq = 0;                CHECK_STATUS(cc_counters_validate(&bad), CC_E_RANGE);
    bad = c; bad.next_txn_id = 0;                 CHECK_STATUS(cc_counters_validate(&bad), CC_E_RANGE);

    cc_record_id r = 0;
    cc_txn_id t = 0;
    cc_run_uid u = 0;
    CHECK_STATUS(cc_alloc_record_id(&c, &r), CC_OK);  CHECK_EQ(r, 1);  CHECK_EQ(c.next_record_id, 2);
    CHECK_STATUS(cc_alloc_record_id(&c, &r), CC_OK);  CHECK_EQ(r, 2);
    CHECK_STATUS(cc_alloc_txn_id(&c, &t), CC_OK);     CHECK_EQ((long long)t, 1);  CHECK_EQ((long long)c.next_txn_id, 2);
    CHECK_STATUS(cc_alloc_run_uid(&c, &u), CC_OK);    CHECK((u == 0xAABBCCDD00000001ull));  CHECK_EQ(c.next_run_seq, 2);
    CHECK_STATUS(cc_alloc_run_uid(&c, &u), CC_OK);    CHECK((u == 0xAABBCCDD00000002ull));
    CHECK_STATUS(cc_alloc_record_id(NULL, &r), CC_E_NULL);
    CHECK_STATUS(cc_alloc_record_id(&c, NULL), CC_E_NULL);
    CHECK_STATUS(cc_alloc_txn_id(&c, NULL), CC_E_NULL);
    CHECK_STATUS(cc_alloc_run_uid(NULL, &u), CC_E_NULL);

    /* a zero nonce still gives a nonzero run UID */
    cc_counters z = {1, 1, 0, 1};
    CHECK_STATUS(cc_alloc_run_uid(&z, &u), CC_OK);    CHECK((u == 1ull));

    /* the last values are never handed out, and the counter then stays put */
    cc_counters e = {UINT32_MAX - 1, UINT32_MAX - 1, 7, UINT64_MAX - 1};
    CHECK_STATUS(cc_alloc_record_id(&e, &r), CC_OK);  CHECK_EQ(r, UINT32_MAX - 1);  CHECK_EQ(e.next_record_id, UINT32_MAX);
    CHECK_STATUS(cc_alloc_record_id(&e, &r), CC_E_OVERFLOW);  CHECK_EQ(e.next_record_id, UINT32_MAX);
    CHECK_STATUS(cc_alloc_txn_id(&e, &t), CC_OK);     CHECK((t == UINT64_MAX - 1));  CHECK((e.next_txn_id == UINT64_MAX));
    CHECK_STATUS(cc_alloc_txn_id(&e, &t), CC_E_OVERFLOW);  CHECK((e.next_txn_id == UINT64_MAX));
    CHECK_STATUS(cc_alloc_run_uid(&e, &u), CC_OK);    CHECK((u == (7ull << 32 | (UINT32_MAX - 1))));
    CHECK_STATUS(cc_alloc_run_uid(&e, &u), CC_E_OVERFLOW);  CHECK_EQ(e.next_run_seq, UINT32_MAX);
    /* allocating from a corrupt counter set is refused */
    CHECK_STATUS(cc_alloc_record_id(&bad, &r), CC_E_RANGE);
}

static cc_ledger_entry mk_entry(cc_txn_id txn, cc_run_uid run, cc_record_id rec, uint8_t kind, uint8_t outcome)
{
    cc_ledger_entry e;
    memset(&e, 0, sizeof e);
    e.txn_id = txn; e.run_uid = run; e.record_id = rec; e.kind = kind; e.outcome = outcome;
    return e;
}

static void test_ledger(void)
{
    cc_ledger l;
    memset(&l, 0, sizeof l);
    CHECK_STATUS(cc_ledger_validate(&l), CC_OK);
    CHECK_STATUS(cc_ledger_validate(NULL), CC_E_NULL);
    cc_ledger_entry out;
    CHECK(!cc_ledger_find(&l, CC_TXN_TERMINAL, 5, 0, &out));
    CHECK(!cc_ledger_find(NULL, CC_TXN_TERMINAL, 5, 0, &out));

    /* a terminal transaction, then its retry */
    cc_ledger_entry a = mk_entry(1, 5, 3, CC_TXN_TERMINAL, CC_TXO_INSERTED);
    CHECK_STATUS(cc_ledger_append(&l, &a), CC_OK);
    CHECK_EQ(l.n, 1);
    CHECK(cc_ledger_find(&l, CC_TXN_TERMINAL, 5, 0, &out));
    CHECK_EQ(out.record_id, 3);                                        /* the retry finds what was inserted */
    CHECK_EQ(out.outcome, CC_TXO_INSERTED);
    cc_ledger_entry retry = mk_entry(2, 5, 4, CC_TXN_TERMINAL, CC_TXO_INSERTED);
    CHECK_STATUS(cc_ledger_append(&l, &retry), CC_E_DUPLICATE);        /* same run: no second insertion */
    CHECK_EQ(l.n, 1);
    CHECK(!cc_ledger_find(&l, CC_TXN_TERMINAL, 6, 0, &out));
    CHECK(!cc_ledger_find(&l, CC_TXN_REWARD_CLAIM, 5, 3, &out));       /* different kind: not found */

    /* a report-only terminal transaction has no record */
    cc_ledger_entry ro = mk_entry(2, 6, 0, CC_TXN_TERMINAL, CC_TXO_REPORT_ONLY);
    CHECK_STATUS(cc_ledger_append(&l, &ro), CC_OK);
    /* a reward claim is keyed by the record, whatever run claims it */
    cc_ledger_entry c1 = mk_entry(3, 7, 3, CC_TXN_REWARD_CLAIM, CC_TXO_CLAIMED);
    CHECK_STATUS(cc_ledger_append(&l, &c1), CC_OK);
    cc_ledger_entry c2 = mk_entry(4, 8, 3, CC_TXN_REWARD_CLAIM, CC_TXO_CLAIMED);
    CHECK_STATUS(cc_ledger_append(&l, &c2), CC_E_DUPLICATE);           /* the record's reward is claimed once */
    CHECK(cc_ledger_find(&l, CC_TXN_REWARD_CLAIM, 0, 3, &out));
    CHECK_EQ((long long)out.txn_id, 3);
    CHECK_EQ(l.n, 3);
    CHECK_STATUS(cc_ledger_validate(&l), CC_OK);

    /* the transaction id must grow */
    cc_ledger_entry old = mk_entry(3, 9, 0, CC_TXN_TERMINAL, CC_TXO_REPORT_ONLY);
    CHECK_STATUS(cc_ledger_append(&l, &old), CC_E_ORDER);
    old.txn_id = 2;
    CHECK_STATUS(cc_ledger_append(&l, &old), CC_E_ORDER);
    CHECK_EQ(l.n, 3);

    /* malformed entries */
    cc_ledger_entry b;
    b = mk_entry(0, 9, 0, CC_TXN_TERMINAL, CC_TXO_REPORT_ONLY);   CHECK_STATUS(cc_ledger_append(&l, &b), CC_E_RANGE);
    b = mk_entry(9, 0, 0, CC_TXN_TERMINAL, CC_TXO_REPORT_ONLY);   CHECK_STATUS(cc_ledger_append(&l, &b), CC_E_RANGE);
    b = mk_entry(9, 9, 0, CC_TXN_NONE, CC_TXO_REPORT_ONLY);       CHECK_STATUS(cc_ledger_append(&l, &b), CC_E_RANGE);
    b = mk_entry(9, 9, 0, CC_TXN_KIND_COUNT_, CC_TXO_REPORT_ONLY);CHECK_STATUS(cc_ledger_append(&l, &b), CC_E_RANGE);
    b = mk_entry(9, 9, 0, CC_TXN_TERMINAL, CC_TXO_INSERTED);      CHECK_STATUS(cc_ledger_append(&l, &b), CC_E_RANGE);          /* inserted, but which record? */
    b = mk_entry(9, 9, 4, CC_TXN_TERMINAL, CC_TXO_REPORT_ONLY);   CHECK_STATUS(cc_ledger_append(&l, &b), CC_E_NONCANONICAL);  /* report only, yet a record */
    b = mk_entry(9, 9, 4, CC_TXN_TERMINAL, CC_TXO_CLAIMED);       CHECK_STATUS(cc_ledger_append(&l, &b), CC_E_RANGE);
    b = mk_entry(9, 9, 0, CC_TXN_REWARD_CLAIM, CC_TXO_CLAIMED);   CHECK_STATUS(cc_ledger_append(&l, &b), CC_E_RANGE);          /* a claim needs its record */
    b = mk_entry(9, 9, 4, CC_TXN_REWARD_CLAIM, CC_TXO_INSERTED);  CHECK_STATUS(cc_ledger_append(&l, &b), CC_E_RANGE);
    CHECK_STATUS(cc_ledger_append(&l, NULL), CC_E_NULL);
    CHECK_STATUS(cc_ledger_append(NULL, &b), CC_E_NULL);
    CHECK_EQ(l.n, 3);

    /* a full ledger drops its oldest entry, nothing else */
    memset(&l, 0, sizeof l);
    for (unsigned i = 1; i <= 8; i++) {
        cc_ledger_entry x = mk_entry(i, 100 + i, i, CC_TXN_TERMINAL, CC_TXO_INSERTED);
        CHECK_STATUS(cc_ledger_append(&l, &x), CC_OK);
    }
    CHECK_EQ(l.n, 8);
    CHECK_STATUS(cc_ledger_validate(&l), CC_OK);
    cc_ledger_entry nine = mk_entry(9, 109, 9, CC_TXN_TERMINAL, CC_TXO_INSERTED);
    CHECK_STATUS(cc_ledger_append(&l, &nine), CC_OK);
    CHECK_EQ(l.n, 8);
    CHECK((l.e[0].txn_id == 2));
    CHECK((l.e[7].txn_id == 9));
    CHECK(!cc_ledger_find(&l, CC_TXN_TERMINAL, 101, 0, &out));         /* the oldest is forgotten... */
    CHECK(cc_ledger_find(&l, CC_TXN_TERMINAL, 102, 0, &out));          /* ...the rest remembered */
    CHECK(cc_ledger_find(&l, CC_TXN_TERMINAL, 109, 0, &out));
    CHECK_STATUS(cc_ledger_validate(&l), CC_OK);

    /* a hand-corrupted ledger is rejected and never appended to */
    memset(&l, 0, sizeof l);
    l.n = 9;                                                           CHECK_STATUS(cc_ledger_validate(&l), CC_E_COUNT);
    memset(&l, 0, sizeof l); l.n = 2;
    l.e[0] = mk_entry(5, 1, 1, CC_TXN_TERMINAL, CC_TXO_INSERTED);
    l.e[1] = mk_entry(5, 2, 2, CC_TXN_TERMINAL, CC_TXO_INSERTED);      CHECK_STATUS(cc_ledger_validate(&l), CC_E_DUPLICATE);
    l.e[1].txn_id = 4;                                                 CHECK_STATUS(cc_ledger_validate(&l), CC_E_ORDER);
    l.e[1].txn_id = 6; l.e[1].run_uid = 1;                             CHECK_STATUS(cc_ledger_validate(&l), CC_E_DUPLICATE);   /* same run twice */
    l.e[1].run_uid = 2;                                                CHECK_STATUS(cc_ledger_validate(&l), CC_OK);
    l.e[1].run_uid = 1; l.e[1].kind = CC_TXN_REWARD_CLAIM; l.e[1].outcome = CC_TXO_CLAIMED; l.e[1].record_id = 1;
    CHECK_STATUS(cc_ledger_validate(&l), CC_OK);                       /* same ids, different kind */
    cc_ledger_entry fresh = mk_entry(7, 3, 3, CC_TXN_TERMINAL, CC_TXO_INSERTED);
    l.e[1].txn_id = 5;
    CHECK_STATUS(cc_ledger_append(&l, &fresh), CC_E_DUPLICATE);        /* validates what it is given first */
}

/* ------------------------------------------------------------------ the whole metadata and sizes */

static cc_meta g_meta;

/* run UIDs this install has issued: (nonce 0x100) << 32 | 1..next_run_seq-1 */
#define UID(seq) ((0x100ull << 32) | (seq))

static void build_meta(unsigned records, unsigned ledger, bool report, unsigned turns, unsigned events_per_turn)
{
    cc_meta_init(&g_meta, 0x100);
    g_meta.counters.next_run_seq = 40;
    g_meta.counters.next_txn_id = 100;
    g_meta.counters.next_record_id = 50;
    g_meta.roster.count = (uint8_t)records;
    for (unsigned i = 0; i < records; i++) g_meta.roster.rec[i] = mk_nem(10 + i, UID(1 + i), 900 + i);
    g_meta.ledger.n = (uint8_t)ledger;
    for (unsigned i = 0; i < ledger; i++)
        g_meta.ledger.e[i] = mk_entry(1 + i, UID(20 + i), 0, CC_TXN_TERMINAL, CC_TXO_REPORT_ONLY);
    if (report) {
        g_meta.has_report = 1;
        g_meta.report = mk_terminal(UID(30), 90);
        g_meta.history.n_turns = (uint8_t)turns;
        for (unsigned i = 0; i < turns; i++)
            g_meta.history.turns[i] = mk_turn(1, (uint16_t)(1 + i), 1 + i * 64, events_per_turn);
    }
}

static void test_meta(void)
{
    cc_meta_init(&g_meta, 0xBEEF);
    CHECK_STATUS(cc_meta_validate(&g_meta), CC_OK);                    /* an empty state is valid */
    CHECK_EQ(g_meta.schema, 1);
    CHECK_EQ(g_meta.counters.next_record_id, 1);
    CHECK_EQ(g_meta.counters.install_nonce, 0xBEEF);
    CHECK_STATUS(cc_meta_validate(NULL), CC_E_NULL);

    build_meta(3, 2, true, 2, 3);
    CHECK_STATUS(cc_meta_validate(&g_meta), CC_OK);
    build_meta(16, 8, true, 5, 64);
    CHECK_STATUS(cc_meta_validate(&g_meta), CC_OK);                    /* every limit reached at once */

    /* versions: older and newer are both refused, never defaulted */
    build_meta(1, 0, false, 0, 0);
    g_meta.schema = 0;   CHECK_STATUS(cc_meta_validate(&g_meta), CC_E_VERSION);
    g_meta.schema = 2;   CHECK_STATUS(cc_meta_validate(&g_meta), CC_E_VERSION);
    g_meta.schema = 0xFFFFFFFFu; CHECK_STATUS(cc_meta_validate(&g_meta), CC_E_VERSION);

    /* identifiers must lie below the counters that issue them */
    build_meta(2, 2, true, 1, 2);
    g_meta.counters.next_record_id = 11;                               /* record 11 exists */
    CHECK_STATUS(cc_meta_validate(&g_meta), CC_E_CONSISTENCY);
    g_meta.counters.next_record_id = 12;
    CHECK_STATUS(cc_meta_validate(&g_meta), CC_OK);
    g_meta.counters.next_txn_id = 90;                                  /* the report's transaction */
    CHECK_STATUS(cc_meta_validate(&g_meta), CC_E_CONSISTENCY);
    g_meta.counters.next_txn_id = 91;
    CHECK_STATUS(cc_meta_validate(&g_meta), CC_OK);
    g_meta.counters.next_run_seq = 30;                                 /* the report's run UID */
    CHECK_STATUS(cc_meta_validate(&g_meta), CC_E_CONSISTENCY);
    g_meta.counters.next_run_seq = 31;
    CHECK_STATUS(cc_meta_validate(&g_meta), CC_OK);
    g_meta.counters.install_nonce = 0x101;                             /* UIDs from another install */
    CHECK_STATUS(cc_meta_validate(&g_meta), CC_E_CONSISTENCY);
    g_meta.counters.install_nonce = 0x100;
    g_meta.roster.rec[0].sched_run_uid = UID(35);                      /* a scheduled run that was never issued */
    g_meta.roster.rec[0].life = CC_LIFE_SCHEDULED;
    CHECK_STATUS(cc_meta_validate(&g_meta), CC_E_CONSISTENCY);
    g_meta.roster.rec[0].sched_run_uid = UID(5);
    CHECK_STATUS(cc_meta_validate(&g_meta), CC_OK);
    g_meta.roster.rec[0].life = CC_LIFE_REWARD_CLAIMED;
    g_meta.roster.rec[0].challenge_wave = 27;
    g_meta.roster.rec[0].reward_txn = 91;                              /* not yet issued */
    CHECK_STATUS(cc_meta_validate(&g_meta), CC_E_CONSISTENCY);
    g_meta.roster.rec[0].reward_txn = 90;
    CHECK_STATUS(cc_meta_validate(&g_meta), CC_OK);
    g_meta.ledger.e[1].txn_id = 91;
    CHECK_STATUS(cc_meta_validate(&g_meta), CC_E_CONSISTENCY);

    /* every stored run UID must be one this install issued: right nonce, sequence in 1..next_run_seq-1 */
    {
        const cc_run_uid bad_uids[] = {UID(0), UID(40), UID(41), (0x101ull << 32) | 1, (0x1ull << 32) | 1, 1ull};
        for (unsigned i = 0; i < sizeof bad_uids / sizeof bad_uids[0]; i++) {
            build_meta(2, 2, true, 1, 2);
            g_meta.roster.rec[0].source_run_uid = bad_uids[i];
            CHECK_STATUS(cc_meta_validate(&g_meta), CC_E_CONSISTENCY);
            build_meta(2, 2, true, 1, 2);
            g_meta.roster.rec[0].life = CC_LIFE_SCHEDULED;
            g_meta.roster.rec[0].sched_run_uid = bad_uids[i];
            CHECK_STATUS(cc_meta_validate(&g_meta), CC_E_CONSISTENCY);
            build_meta(2, 2, true, 1, 2);
            g_meta.ledger.e[1].run_uid = bad_uids[i];
            CHECK_STATUS(cc_meta_validate(&g_meta), CC_E_CONSISTENCY);
            build_meta(2, 2, true, 1, 2);
            g_meta.report.run_uid = bad_uids[i];
            CHECK_STATUS(cc_meta_validate(&g_meta), CC_E_CONSISTENCY);
        }
        /* the edges that are issued: sequence 1 and next_run_seq - 1 */
        build_meta(2, 2, true, 1, 2);
        g_meta.report.run_uid = UID(1);   CHECK_STATUS(cc_meta_validate(&g_meta), CC_OK);
        g_meta.report.run_uid = UID(39);  CHECK_STATUS(cc_meta_validate(&g_meta), CC_OK);
    }
    /* a ledger entry may not name a record this install has not issued either */
    build_meta(2, 2, true, 1, 2);
    g_meta.ledger.e[0].outcome = CC_TXO_INSERTED; g_meta.ledger.e[0].record_id = 49;
    CHECK_STATUS(cc_meta_validate(&g_meta), CC_OK);
    g_meta.ledger.e[0].record_id = 50;   CHECK_STATUS(cc_meta_validate(&g_meta), CC_E_CONSISTENCY);
    g_meta.ledger.e[0].record_id = 51;   CHECK_STATUS(cc_meta_validate(&g_meta), CC_E_CONSISTENCY);

    /* sub-structure failures surface as themselves */
    build_meta(2, 2, true, 1, 2);
    g_meta.roster.rec[1].record_id = g_meta.roster.rec[0].record_id;
    CHECK_STATUS(cc_meta_validate(&g_meta), CC_E_DUPLICATE);
    build_meta(2, 2, true, 1, 2);
    g_meta.ledger.n = 9;
    CHECK_STATUS(cc_meta_validate(&g_meta), CC_E_COUNT);
    build_meta(2, 2, true, 1, 2);
    g_meta.history.turns[0].wave = 0;
    CHECK_STATUS(cc_meta_validate(&g_meta), CC_E_RANGE);
    build_meta(2, 2, true, 1, 2);
    g_meta.report.txn_id = 0;
    CHECK_STATUS(cc_meta_validate(&g_meta), CC_E_RANGE);               /* a stored report has its transaction */
    build_meta(2, 2, true, 1, 2);
    g_meta.has_report = 2;
    CHECK_STATUS(cc_meta_validate(&g_meta), CC_E_RANGE);

    /* no report means an empty report and history */
    build_meta(1, 0, false, 0, 0);
    g_meta.report.wave = 5;                CHECK_STATUS(cc_meta_validate(&g_meta), CC_E_NONCANONICAL);
    build_meta(1, 0, false, 0, 0);
    g_meta.report.fallen_actor.id = 5;     CHECK_STATUS(cc_meta_validate(&g_meta), CC_E_NONCANONICAL);
    build_meta(1, 0, false, 0, 0);
    g_meta.history.n_turns = 1;            CHECK_STATUS(cc_meta_validate(&g_meta), CC_E_NONCANONICAL);
    build_meta(1, 0, false, 0, 0);
    g_meta.history.flags = 1;              CHECK_STATUS(cc_meta_validate(&g_meta), CC_E_NONCANONICAL);
    for (int field = 0; field < 8; field++) {                          /* each report field on its own */
        build_meta(1, 0, false, 0, 0);
        switch (field) {
        case 0: g_meta.report.run_uid = UID(1); break;
        case 1: g_meta.report.txn_id = 1; break;
        case 2: g_meta.report.outcome = CC_OUT_DEFEAT; break;
        case 3: g_meta.report.known = CC_TK_FINISH_MOVE; break;
        case 4: g_meta.report.biome = 1; break;
        case 5: g_meta.report.finish_move = 1; break;
        case 6: g_meta.report.finish_actor.id = 1; break;
        default: g_meta.report.fallen_actor.id = 1; break;
        }
        CHECK_STATUS(cc_meta_validate(&g_meta), CC_E_NONCANONICAL);
    }
}

static void test_sizes(void)
{
    size_t out = 99;
    /* checked arithmetic at the edges */
    CHECK_STATUS(cc_size_add(0, 0, &out), CC_OK);                      CHECK_EQ(out, 0);
    CHECK_STATUS(cc_size_add(SIZE_MAX, 0, &out), CC_OK);               CHECK(out == SIZE_MAX);
    CHECK_STATUS(cc_size_add(SIZE_MAX - 5, 5, &out), CC_OK);           CHECK(out == SIZE_MAX);
    out = 7;
    CHECK_STATUS(cc_size_add(SIZE_MAX, 1, &out), CC_E_OVERFLOW);       CHECK_EQ(out, 7);   /* unchanged on error */
    CHECK_STATUS(cc_size_add(1, SIZE_MAX, &out), CC_E_OVERFLOW);
    CHECK_STATUS(cc_size_add(SIZE_MAX / 2 + 1, SIZE_MAX / 2 + 1, &out), CC_E_OVERFLOW);
    CHECK_STATUS(cc_size_add(1, 2, NULL), CC_E_NULL);
    CHECK_STATUS(cc_size_mul(0, SIZE_MAX, &out), CC_OK);               CHECK_EQ(out, 0);
    CHECK_STATUS(cc_size_mul(SIZE_MAX, 0, &out), CC_OK);               CHECK_EQ(out, 0);
    CHECK_STATUS(cc_size_mul(SIZE_MAX, 1, &out), CC_OK);               CHECK(out == SIZE_MAX);
    CHECK_STATUS(cc_size_mul(3, 7, &out), CC_OK);                      CHECK_EQ(out, 21);
    out = 7;
    CHECK_STATUS(cc_size_mul(SIZE_MAX, 2, &out), CC_E_OVERFLOW);       CHECK_EQ(out, 7);
    CHECK_STATUS(cc_size_mul(SIZE_MAX / 2 + 1, 2, &out), CC_E_OVERFLOW);
    CHECK_STATUS(cc_size_mul(2, SIZE_MAX / 2, &out), CC_OK);           CHECK(out == SIZE_MAX - 1);
    CHECK_STATUS(cc_size_mul(1, 2, NULL), CC_E_NULL);

    /* encoded sizes, computed by hand from the layout in docs/CUSTOM-CONTENT-CONTRACTS.md:
     * envelope 12 + counters 20 + (1 + 22 n_ledger) + (1 + 369 n_records) + has_report 1
     *   [+ terminal 41 + history header 2 + per turn (12 + 38 events)] */
    unsigned ev[5] = {0};
    CHECK_STATUS(cc_wire_meta_size_for(0, 0, false, 0, NULL, &out), CC_OK);
    CHECK_EQ(out, 35);                                                 /* 12+20+1+1+1 */
    CHECK_STATUS(cc_wire_meta_size_for(0, 0, true, 0, NULL, &out), CC_OK);
    CHECK_EQ(out, 78);                                                 /* 35 + 41 + 2 */
    CHECK_STATUS(cc_wire_meta_size_for(1, 1, false, 0, NULL, &out), CC_OK);
    CHECK_EQ(out, 35 + 22 + 369);                                      /* 426 */
    ev[0] = 3; ev[1] = 64;
    CHECK_STATUS(cc_wire_meta_size_for(3, 2, true, 2, ev, &out), CC_OK);
    CHECK_EQ(out, 3799);                                               /* 12+20+45+1108+1+41+2+126+2444 */
    for (int i = 0; i < 5; i++) ev[i] = 64;
    CHECK_STATUS(cc_wire_meta_size_for(16, 8, true, 5, ev, &out), CC_OK);
    CHECK_EQ(out, 18378);                                              /* the maximum */
    CHECK(out <= 64u * 1024u);
    CHECK_EQ(CC_WIRE_META_MAX, 18378);

    /* over-limit counts */
    CHECK_STATUS(cc_wire_meta_size_for(17, 0, false, 0, NULL, &out), CC_E_COUNT);
    CHECK_STATUS(cc_wire_meta_size_for(0, 9, false, 0, NULL, &out), CC_E_COUNT);
    CHECK_STATUS(cc_wire_meta_size_for(0, 0, true, 6, ev, &out), CC_E_COUNT);
    CHECK_STATUS(cc_wire_meta_size_for(UINT_MAX, UINT_MAX, true, UINT_MAX, ev, &out), CC_E_COUNT);
    ev[2] = 65;
    CHECK_STATUS(cc_wire_meta_size_for(0, 0, true, 5, ev, &out), CC_E_COUNT);
    ev[2] = 64;
    CHECK_STATUS(cc_wire_meta_size_for(0, 0, false, 1, ev, &out), CC_E_CONSISTENCY);    /* turns without a report */
    CHECK_STATUS(cc_wire_meta_size_for(0, 0, true, 1, NULL, &out), CC_E_NULL);
    CHECK_STATUS(cc_wire_meta_size_for(0, 0, false, 0, NULL, NULL), CC_E_NULL);

    /* from a state */
    build_meta(3, 2, true, 2, 3);
    g_meta.history.turns[1] = mk_turn(1, 2, 100, 64);
    CHECK_STATUS(cc_wire_meta_size(&g_meta, &out), CC_OK);
    CHECK_EQ(out, 3799);
    build_meta(0, 0, false, 0, 0);
    CHECK_STATUS(cc_wire_meta_size(&g_meta, &out), CC_OK);
    CHECK_EQ(out, 35);
    cc_meta_init(&g_meta, 1);
    g_meta.schema = 9;
    CHECK_STATUS(cc_wire_meta_size(&g_meta, &out), CC_E_VERSION);       /* an invalid state has no size */
    CHECK_STATUS(cc_wire_meta_size(NULL, &out), CC_E_NULL);
    CHECK_STATUS(cc_wire_meta_size(&g_meta, NULL), CC_E_NULL);
}

static void test_text(void)
{
    CHECK(strcmp(cc_status_str(CC_OK), "ok") == 0);
    for (int s = CC_OK; s <= CC_E_FORMAT; s++) CHECK(cc_status_str((cc_status)s)[0] != 0);
    CHECK(strcmp(cc_status_str((cc_status)99), "unknown status") == 0);
    for (int r = CC_SUPPORT_OK; r < CC_REFUSE_COUNT_; r++) CHECK(cc_refusal_str(r)[0] != 0);
    CHECK(strcmp(cc_refusal_str(CC_SUPPORT_OK), "supported") == 0);
    CHECK(strcmp(cc_refusal_str(99), "unknown refusal") == 0);
    /* every code has its own text, so a message cannot be attached to the wrong code */
    for (int x = CC_OK; x <= CC_E_FORMAT; x++)
        for (int y = x + 1; y <= CC_E_FORMAT; y++)
            CHECK(strcmp(cc_status_str((cc_status)x), cc_status_str((cc_status)y)) != 0);
    for (int x = CC_SUPPORT_OK; x < CC_REFUSE_COUNT_; x++)
        for (int y = x + 1; y < CC_REFUSE_COUNT_; y++)
            CHECK(strcmp(cc_refusal_str(x), cc_refusal_str(y)) != 0);
}

int main(void)
{
    test_obs();
    test_events();
    test_turns();
    test_history();
    test_terminal();
    test_nemesis_fields();
    test_lifecycle_fields();
    test_support_and_gate();
    test_roster();
    test_lifecycle();
    test_schedule();
    test_ids();
    test_ledger();
    test_meta();
    test_sizes();
    test_text();
    return cc_summary("model_test");
}
