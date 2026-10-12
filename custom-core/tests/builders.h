/* Builders shared by codec_test.c and commit_test.c (copies of model_test.c's, which stays as C01 left it). Every value is a
 * literal chosen by hand. Large structures are static in the tests: nothing here is a stack object the size of a save buffer. */
#ifndef CC_TEST_BUILDERS_H
#define CC_TEST_BUILDERS_H

#include <stdint.h>
#include <string.h>

#include "cc/model.h"

static inline cc_actor mk_actor(uint32_t id, uint16_t species, uint8_t side)
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
static inline cc_event mk_damage(uint32_t seq)
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

static inline cc_turn mk_turn(uint32_t battle_seq, uint16_t turn, uint32_t first_seq, unsigned n)
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

static inline cc_terminal mk_terminal(cc_run_uid run, cc_txn_id txn)
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
static inline cc_nemesis mk_nem(cc_record_id id, cc_run_uid run, uint32_t pokemon_id)
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

static inline cc_ledger_entry mk_entry(cc_txn_id txn, cc_run_uid run, cc_record_id rec, uint8_t kind, uint8_t outcome)
{
    cc_ledger_entry e;
    memset(&e, 0, sizeof e);
    e.txn_id = txn; e.run_uid = run; e.record_id = rec; e.kind = kind; e.outcome = outcome;
    return e;
}

#endif
