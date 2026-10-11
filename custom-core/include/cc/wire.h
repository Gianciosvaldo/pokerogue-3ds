/* custom-core: byte widths of the version-1 persisted layout (little-endian, packed, no padding).
 * Each constant is the sum of the field widths listed in docs/CUSTOM-CONTENT-CONTRACTS.md, "Wire layout".
 * There is no encoder in this module yet (the codec belongs to task C04); these constants and cc_wire_meta_size
 * fix the sizes it must produce. tests/layout_test.c checks every one against a literal computed by hand. */
#ifndef CC_WIRE_H
#define CC_WIRE_H

#include "cc/limits.h"

/* actor: id4 species2 form1 side1 level1 */
#define CC_WIRE_ACTOR 9u
/* event: seq4 kind1 target_side1 source_side1 target_form1 source_form1 result1 known2 target_species2
 *        source_species2 move_id2 target_id4 source_id4 shown4 hp_delta4 hp_after4 */
#define CC_WIRE_EVENT 38u
/* turn header: battle_seq4 wave2 turn2 dropped2 flags1 n_events1, then n_events events */
#define CC_WIRE_TURN_HEADER 12u
/* history header: n_turns1 flags1, then n_turns turns */
#define CC_WIRE_HISTORY_HEADER 2u
/* terminal: run_uid8 txn_id8 outcome1 known1 biome1 wave2 finish_move2 finish_actor9 fallen_actor9 */
#define CC_WIRE_TERMINAL 41u
/* item: key33 arg2 stack2 */
#define CC_WIRE_ITEM 37u
/* nemesis: record_id4 life1 traits1 known2 | source_run_uid8 source_pokemon_id4 source_wave2 source_biome1 |
 *          species2 form1 gender1 shiny1 orig_level1 nature1 | ability2 passive2 | ivs6 | n_moves1 moves8 |
 *          n_items1 items(8*37) | finish_move2 fallen_species2 fallen_form1 | sched_run_uid8 challenge_wave2 reward_txn8 */
#define CC_WIRE_NEMESIS 369u
/* counters: next_record_id4 next_run_seq4 install_nonce4 next_txn_id8 */
#define CC_WIRE_COUNTERS 20u
/* ledger entry: txn_id8 run_uid8 record_id4 kind1 outcome1; the ledger is n1 then n entries */
#define CC_WIRE_LEDGER_ENTRY 22u
/* envelope header: magic4 schema2 reserved2 body_len4 (body_len counts every byte after the header) */
#define CC_WIRE_ENVELOPE_HEADER 12u

/* body: counters | n_ledger1 + entries | n_records1 + records | has_report1 [ terminal | history ] */
#define CC_WIRE_TURN_MAX (CC_WIRE_TURN_HEADER + CC_MAX_EVENTS_PER_TURN * CC_WIRE_EVENT)
#define CC_WIRE_HISTORY_MAX (CC_WIRE_HISTORY_HEADER + CC_HISTORY_TURNS * CC_WIRE_TURN_MAX)
#define CC_WIRE_META_MAX                                                                                            \
    (CC_WIRE_ENVELOPE_HEADER + CC_WIRE_COUNTERS + (1u + CC_LEDGER_ENTRIES * CC_WIRE_LEDGER_ENTRY) +                  \
     (1u + CC_MAX_NEMESES * CC_WIRE_NEMESIS) + 1u + CC_WIRE_TERMINAL + CC_WIRE_HISTORY_MAX)

#endif
