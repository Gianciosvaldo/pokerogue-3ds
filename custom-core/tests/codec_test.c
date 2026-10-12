/* custom-core codec tests (task C04): the version-1 blob of the persisted custom metadata.
 *
 * The expected bytes are literals worked out by hand from the wire table in docs/CUSTOM-CONTENT-CONTRACTS.md, section 2.6, not
 * produced by the encoder under test. Large structures are static. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cc/codec.h"
#include "builders.h"
#include "check.h"

#define NONCE 0x11223344u
#define UID(seq) (((uint64_t)NONCE << 32) | (seq))

static cc_meta g_meta, g_out;
static uint8_t g_buf[CC_WIRE_META_MAX + 64], g_buf2[CC_WIRE_META_MAX + 64], g_bad[CC_WIRE_META_MAX + 64];

/* A mid-sized state, every part present: 2 ledger entries, 2 records, a report, a history of 3 + 2 events. */
static void build_mid(void)
{
    cc_meta_init(&g_meta, NONCE);
    g_meta.counters.next_record_id = 3;
    g_meta.counters.next_run_seq = 4;
    g_meta.counters.next_txn_id = 4;
    g_meta.ledger.n = 2;
    g_meta.ledger.e[0] = mk_entry(2, UID(2), 2, CC_TXN_TERMINAL, CC_TXO_INSERTED);
    g_meta.ledger.e[1] = mk_entry(3, UID(3), 0, CC_TXN_TERMINAL, CC_TXO_REPORT_ONLY);
    g_meta.roster.count = 2;
    g_meta.roster.rec[0] = mk_nem(1, UID(1), 900);
    g_meta.roster.rec[1] = mk_nem(2, UID(2), 901);
    g_meta.has_report = 1;
    g_meta.report = mk_terminal(UID(3), 3);
    g_meta.history.n_turns = 2;
    g_meta.history.flags = CC_HF_BEGAN_MID;
    g_meta.history.turns[0] = mk_turn(1, 1, 1, 3);
    g_meta.history.turns[0].flags = CC_TF_SOURCE_LOSS | CC_TF_BEGAN_MID;
    g_meta.history.turns[1] = mk_turn(1, 2, 4, 2);
    /* the first event of turn 1 carries every optional field */
    cc_event *e = &g_meta.history.turns[1].events[0];
    e->target_form = 2; e->source_form = 3; e->result = 5; e->source_side = CC_SIDE_ENEMY;
    e->known = CC_EK_TARGET_ID | CC_EK_SOURCE_SIDE | CC_EK_SOURCE_ID | CC_EK_SOURCE_SPECIES | CC_EK_MOVE | CC_EK_RESULT |
               CC_EK_SHOWN | CC_EK_HP_DELTA | CC_EK_HP_AFTER;
    e->source_species = 19; e->move_id = 33; e->target_id = 7; e->source_id = 900;
    e->shown = 12; e->hp_delta = -12; e->hp_after = 8;
}

/* every limit at once: 16 records, 8 ledger entries, 5 turns of 64 events */
static void build_max(void)
{
    cc_meta_init(&g_meta, NONCE);
    g_meta.counters.next_record_id = 40;
    g_meta.counters.next_run_seq = 60;
    g_meta.counters.next_txn_id = 100;
    g_meta.roster.count = CC_MAX_NEMESES;
    for (unsigned i = 0; i < CC_MAX_NEMESES; i++) g_meta.roster.rec[i] = mk_nem(10 + i, UID(1 + i), 900 + i);
    g_meta.ledger.n = CC_LEDGER_ENTRIES;
    for (unsigned i = 0; i < CC_LEDGER_ENTRIES; i++)
        g_meta.ledger.e[i] = mk_entry(1 + i, UID(20 + i), 0, CC_TXN_TERMINAL, CC_TXO_REPORT_ONLY);
    g_meta.has_report = 1;
    g_meta.report = mk_terminal(UID(30), 90);
    g_meta.history.n_turns = CC_HISTORY_TURNS;
    for (unsigned i = 0; i < CC_HISTORY_TURNS; i++) g_meta.history.turns[i] = mk_turn(1, (uint16_t)(1 + i), 1 + i * 64, 64);
    g_meta.history.turns[4].flags = CC_TF_EVICTED;       /* a full turn that lost 300 older events */
    g_meta.history.turns[4].dropped = 300;
}

static size_t enc(void *buf, size_t cap)
{
    size_t n = 0;
    cc_status s = cc_meta_encode(&g_meta, buf, cap, &n);
    CHECK_STATUS(s, CC_OK);
    return s == CC_OK ? n : 0;
}

static void test_empty_exact_bytes(void)
{
    cc_meta_init(&g_meta, 0x01020304u);
    /* magic "PRCM" = 50 52 43 4D, schema 1, reserved 0, body_len 23 | counters: 1, 1, nonce 04 03 02 01, txn 1 | 0 ledger, 0 records, no report */
    static const uint8_t want[35] = {0x50, 0x52, 0x43, 0x4D, 0x01, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00,
                                     0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x03, 0x02, 0x01,
                                     0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00};
    const size_t n = enc(g_buf, sizeof g_buf);
    CHECK_EQ(n, 35);
    CHECK(n == 35 && memcmp(g_buf, want, 35) == 0);
    CHECK_STATUS(cc_meta_decode(want, 35, &g_out), CC_OK);
    CHECK_EQ(g_out.counters.install_nonce, 0x01020304u);
    CHECK_EQ(g_out.roster.count, 0);
    CHECK_EQ(g_out.has_report, 0);
}

static void test_mid_layout(void)
{
    build_mid();
    const size_t n = enc(g_buf, sizeof g_buf);
    CHECK_EQ(n, 1074);                                   /* 12 + 20 + 45 + 739 + 1 + 41 + 2 + 126 + 88 */
    if (n != 1074) return;
    CHECK_EQ(g_buf[0], 0x50); CHECK_EQ(g_buf[1], 0x52); CHECK_EQ(g_buf[2], 0x43); CHECK_EQ(g_buf[3], 0x4D);
    CHECK_EQ(g_buf[4], 1); CHECK_EQ(g_buf[5], 0);        /* schema u16 */
    CHECK_EQ(g_buf[6], 0); CHECK_EQ(g_buf[7], 0);        /* reserved */
    CHECK_EQ(g_buf[8], 0x26); CHECK_EQ(g_buf[9], 0x04); CHECK_EQ(g_buf[10], 0); CHECK_EQ(g_buf[11], 0);   /* body_len 1062 = 0x0426 */
    /* counters at 12: record 3, run seq 4, nonce 0x11223344, txn 4 */
    CHECK_EQ(g_buf[12], 3); CHECK_EQ(g_buf[16], 4);
    CHECK_EQ(g_buf[20], 0x44); CHECK_EQ(g_buf[21], 0x33); CHECK_EQ(g_buf[22], 0x22); CHECK_EQ(g_buf[23], 0x11);
    CHECK_EQ(g_buf[24], 4); CHECK_EQ(g_buf[25], 0);
    /* n_ledger at 32, entry 0 at 33: txn 2, run uid 0x11223344_00000002, record 2, kind 1, outcome 1 */
    CHECK_EQ(g_buf[32], 2);
    CHECK_EQ(g_buf[33], 2); CHECK_EQ(g_buf[41], 2);
    CHECK_EQ(g_buf[45], 0x44); CHECK_EQ(g_buf[48], 0x11);
    CHECK_EQ(g_buf[49], 2); CHECK_EQ(g_buf[53], CC_TXN_TERMINAL); CHECK_EQ(g_buf[54], CC_TXO_INSERTED);
    /* entry 1 at 55: kind/outcome at 76/77 */
    CHECK_EQ(g_buf[55], 3); CHECK_EQ(g_buf[59 + 12], 0);
    CHECK_EQ(g_buf[75], CC_TXN_TERMINAL); CHECK_EQ(g_buf[76], CC_TXO_REPORT_ONLY);
    /* n_records at 77; record 0 at 78: id 1, life STORED, traits 0, known u16 */
    CHECK_EQ(g_buf[77], 2);
    CHECK_EQ(g_buf[78], 1); CHECK_EQ(g_buf[82], CC_LIFE_STORED); CHECK_EQ(g_buf[83], 0);
    const uint16_t known = (uint16_t)(CC_NK_ABILITY | CC_NK_PASSIVE | CC_NK_MOVES | CC_NK_IVS | CC_NK_NATURE | CC_NK_LOADOUT |
                                      CC_NK_FINISH_MOVE | CC_NK_FALLEN | CC_NK_FINISHER_VERIFIED);
    CHECK_EQ(g_buf[84] | (g_buf[85] << 8), known);
    /* source_run_uid at 86 (8), pokemon id 900 = 0x384 at 94, wave 27 at 98, biome 3 at 100, species 19 at 101, form 1 at 103 */
    CHECK_EQ(g_buf[86], 1); CHECK_EQ(g_buf[93], 0x11);
    CHECK_EQ(g_buf[94], 0x84); CHECK_EQ(g_buf[95], 0x03);
    CHECK_EQ(g_buf[98], 27); CHECK_EQ(g_buf[100], 3); CHECK_EQ(g_buf[101], 19); CHECK_EQ(g_buf[103], 1);
    /* record 1 starts at 78 + 369 = 447; has_report at 78 + 738 = 816 */
    CHECK_EQ(g_buf[447], 2);
    CHECK_EQ(g_buf[816], 1);
    /* terminal at 817: run uid 3, txn 3, outcome DEFEAT, known 7, biome 3, wave 27 */
    CHECK_EQ(g_buf[817], 3); CHECK_EQ(g_buf[825], 3); CHECK_EQ(g_buf[833], CC_OUT_DEFEAT); CHECK_EQ(g_buf[834], 7);
    CHECK_EQ(g_buf[835], 3); CHECK_EQ(g_buf[836], 27);
    /* history at 858: 2 turns, flags 0; turn 0 header at 860: battle_seq 1, wave 3, turn 1, dropped 0, flags 0, 3 events */
    CHECK_EQ(g_buf[858], 2); CHECK_EQ(g_buf[859], CC_HF_BEGAN_MID);
    CHECK_EQ(g_buf[860], 1); CHECK_EQ(g_buf[864], 3); CHECK_EQ(g_buf[866], 1); CHECK_EQ(g_buf[870], CC_TF_SOURCE_LOSS | CC_TF_BEGAN_MID); CHECK_EQ(g_buf[871], 3);
    /* first event at 872: seq 1, kind DAMAGE at 876, known = SHOWN (0x40) at 882, target species 25 at 884, shown 12 at 898 */
    CHECK_EQ(g_buf[872], 1); CHECK_EQ(g_buf[876], CC_EV_DAMAGE);
    CHECK_EQ(g_buf[882], 0x40); CHECK_EQ(g_buf[883], 0);
    CHECK_EQ(g_buf[884], 25); CHECK_EQ(g_buf[885], 0);
    CHECK_EQ(g_buf[898], 12);
    /* turn 1 starts at 860 + 12 + 3 * 38 = 986 with turn number 2 and 2 events; the blob ends after its second event */
    CHECK_EQ(g_buf[986], 1); CHECK_EQ(g_buf[992], 2); CHECK_EQ(g_buf[997], 2);
    /* its first event at 998: seq 4, kind DAMAGE, sides PLAYER/ENEMY, forms 2/3, result 5, known 0x1FF, species 25/19, move 33, ids 7/900,
     * shown 12, hp_delta -12, hp_after 8 */
    CHECK_EQ(g_buf[998], 4); CHECK_EQ(g_buf[1002], CC_EV_DAMAGE); CHECK_EQ(g_buf[1003], CC_SIDE_PLAYER); CHECK_EQ(g_buf[1004], CC_SIDE_ENEMY);
    CHECK_EQ(g_buf[1005], 2); CHECK_EQ(g_buf[1006], 3); CHECK_EQ(g_buf[1007], 5);
    CHECK_EQ(g_buf[1008], 0xFF); CHECK_EQ(g_buf[1009], 0x01);   /* all nine known bits: 0x01FF */
    CHECK_EQ(g_buf[1010], 25); CHECK_EQ(g_buf[1012], 19); CHECK_EQ(g_buf[1014], 33);
    CHECK_EQ(g_buf[1016], 7); CHECK_EQ(g_buf[1020], 0x84); CHECK_EQ(g_buf[1021], 0x03);
    CHECK_EQ(g_buf[1024], 12); CHECK_EQ(g_buf[1028], 0xF4); CHECK_EQ(g_buf[1031], 0xFF); CHECK_EQ(g_buf[1032], 8);
    CHECK_EQ(g_buf[1073], 0);

    CHECK_STATUS(cc_meta_decode(g_buf, n, &g_out), CC_OK);
    CHECK_EQ(g_out.roster.count, 2);
    CHECK_EQ(g_out.roster.rec[1].record_id, 2);
    CHECK_EQ(g_out.roster.rec[0].source_pokemon_id, 900);
    CHECK_EQ(g_out.roster.rec[0].items[0].stack, 2);
    CHECK_EQ(g_out.roster.rec[0].items[0].arg, -1);
    CHECK(strcmp(g_out.roster.rec[0].items[0].key, "LEFTOVERS") == 0);
    CHECK_EQ(g_out.history.n_turns, 2);
    CHECK_EQ(g_out.history.flags, CC_HF_BEGAN_MID);
    CHECK_EQ(g_out.history.turns[0].flags, CC_TF_SOURCE_LOSS | CC_TF_BEGAN_MID);
    CHECK_EQ(g_out.history.turns[1].n_events, 2);
    CHECK_EQ(g_out.history.turns[1].events[0].move_id, 33);
    CHECK_EQ(g_out.history.turns[1].events[0].source_id, 900);
    CHECK_EQ(g_out.history.turns[1].events[0].target_id, 7);
    CHECK_EQ(g_out.history.turns[1].events[0].hp_delta, -12);
    CHECK_EQ(g_out.report.finish_actor.id, 900);
    CHECK_EQ(g_out.report.txn_id, 3);
    /* a decoded state re-encodes to the same bytes */
    size_t m = 0;
    CHECK_STATUS(cc_meta_encode(&g_out, g_buf2, sizeof g_buf2, &m), CC_OK);
    CHECK(m == n && memcmp(g_buf, g_buf2, n) == 0);
}

static void test_signed_fields(void)
{
    /* negative values survive the round trip: arg -1 above, a negative hp_delta, and a genderless record */
    build_mid();
    g_meta.roster.rec[1].gender = -1;
    cc_event *e = &g_meta.history.turns[1].events[1];
    e->kind = CC_EV_HP_LOSS;
    e->known = CC_EK_HP_DELTA | CC_EK_HP_AFTER;
    e->shown = 0;
    e->hp_delta = -2147483647 - 1;
    e->hp_after = 2147483647;
    const size_t n = enc(g_buf, sizeof g_buf);
    CHECK_STATUS(cc_meta_decode(g_buf, n, &g_out), CC_OK);
    CHECK_EQ(g_out.roster.rec[1].gender, -1);
    CHECK_EQ(g_out.history.turns[1].events[1].hp_delta, -2147483647 - 1);
    CHECK_EQ(g_out.history.turns[1].events[1].hp_after, 2147483647);
}

static void test_max(void)
{
    build_max();
    const size_t n = enc(g_buf, sizeof g_buf);
    CHECK_EQ(n, CC_WIRE_META_MAX);
    CHECK_EQ(n, 18378);                                  /* the contract's stated maximum, 17.95 KiB */
    CHECK(n <= CC_META_BUDGET_BYTES);
    CHECK_STATUS(cc_meta_decode(g_buf, n, &g_out), CC_OK);
    CHECK_EQ(g_out.roster.count, 16);
    CHECK_EQ(g_out.history.n_turns, 5);
    CHECK_EQ(g_out.history.turns[4].n_events, 64);
    CHECK_EQ(g_out.history.turns[4].dropped, 300);
    CHECK_EQ(g_out.history.turns[4].flags, CC_TF_EVICTED);
    CHECK_EQ(g_buf[15942], 0x2C); CHECK_EQ(g_buf[15943], 0x01); CHECK_EQ(g_buf[15944], CC_TF_EVICTED);   /* turn 4's header: dropped 300, flags */
    size_t m = 0;
    CHECK_STATUS(cc_meta_encode(&g_out, g_buf2, sizeof g_buf2, &m), CC_OK);
    CHECK(m == n && memcmp(g_buf, g_buf2, n) == 0);
    /* an exact-size buffer works, one byte short does not and is left untouched */
    uint8_t exact[CC_WIRE_META_MAX];
    size_t got = 0;
    CHECK_STATUS(cc_meta_encode(&g_meta, exact, sizeof exact, &got), CC_OK);
    memset(g_buf2, 0xAA, sizeof g_buf2);
    got = 12345;
    CHECK_STATUS(cc_meta_encode(&g_meta, g_buf2, n - 1, &got), CC_E_COUNT);
    CHECK_EQ(got, 12345);
    int touched = 0;
    for (size_t i = 0; i < sizeof g_buf2; i++) touched += g_buf2[i] != 0xAA;
    CHECK_EQ(touched, 0);
}

static void test_encode_refuses(void)
{
    size_t n = 7;
    build_mid();
    CHECK_STATUS(cc_meta_encode(NULL, g_buf, sizeof g_buf, &n), CC_E_NULL);
    CHECK_STATUS(cc_meta_encode(&g_meta, NULL, sizeof g_buf, &n), CC_E_NULL);
    CHECK_STATUS(cc_meta_encode(&g_meta, g_buf, sizeof g_buf, NULL), CC_E_NULL);
    CHECK_STATUS(cc_meta_encode(&g_meta, g_buf, 0, &n), CC_E_COUNT);
    CHECK_STATUS(cc_meta_encode(&g_meta, g_buf, 34, &n), CC_E_COUNT);
    CHECK_EQ(n, 7);
    /* an invalid state is never written */
    g_meta.schema = 2;
    CHECK_STATUS(cc_meta_encode(&g_meta, g_buf, sizeof g_buf, &n), CC_E_VERSION);
    build_mid();
    g_meta.roster.rec[1].record_id = 1;                  /* duplicate id */
    CHECK_STATUS(cc_meta_encode(&g_meta, g_buf, sizeof g_buf, &n), CC_E_DUPLICATE);
    build_mid();
    g_meta.counters.next_run_seq = 3;                    /* the report's run was never issued */
    CHECK_STATUS(cc_meta_encode(&g_meta, g_buf, sizeof g_buf, &n), CC_E_CONSISTENCY);
    build_mid();
    g_meta.history.turns[0].events[0].known = 0;         /* a hit with nothing known */
    CHECK(cc_meta_encode(&g_meta, g_buf, sizeof g_buf, &n) != CC_OK);
    CHECK_EQ(n, 7);
}

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static void wr32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

static void test_envelope_refusals(void)
{
    build_mid();
    const size_t n = enc(g_buf, sizeof g_buf);
    CHECK_STATUS(cc_meta_decode(NULL, n, &g_out), CC_E_NULL);
    CHECK_STATUS(cc_meta_decode(g_buf, n, NULL), CC_E_NULL);
    /* too short for the header, empty, wrong magic in each byte */
    for (size_t k = 0; k < 12; k++) CHECK_STATUS(cc_meta_decode(g_buf, k, &g_out), CC_E_FORMAT);
    for (int b = 0; b < 4; b++) {
        memcpy(g_bad, g_buf, n);
        g_bad[b] ^= 0x01;
        CHECK_STATUS(cc_meta_decode(g_bad, n, &g_out), CC_E_FORMAT);
    }
    /* schema: any other than 1 is a version problem, older or newer, and is reported before the rest is read */
    static const uint16_t schemas[] = {0, 2, 3, 0x0100, 0x8000, 0xFFFF};
    for (unsigned i = 0; i < sizeof schemas / sizeof schemas[0]; i++) {
        memcpy(g_bad, g_buf, n);
        g_bad[4] = (uint8_t)schemas[i];
        g_bad[5] = (uint8_t)(schemas[i] >> 8);
        CHECK_STATUS(cc_meta_decode(g_bad, n, &g_out), CC_E_VERSION);
    }
    /* a future blob with an unreadable layout (wrong length) is still a version problem, not "corrupt" */
    memcpy(g_bad, g_buf, n);
    g_bad[4] = 2;
    wr32(g_bad + 8, 5);
    CHECK_STATUS(cc_meta_decode(g_bad, n, &g_out), CC_E_VERSION);
    /* reserved must be zero, in each byte */
    for (int b = 6; b < 8; b++) {
        memcpy(g_bad, g_buf, n);
        g_bad[b] = 1;
        CHECK_STATUS(cc_meta_decode(g_bad, n, &g_out), CC_E_FORMAT);
    }
    /* body_len must be exactly the bytes present */
    const uint32_t body = rd32(g_buf + 8);
    static const int32_t deltas[] = {-1, 1, -100, 100};
    for (unsigned i = 0; i < sizeof deltas / sizeof deltas[0]; i++) {
        memcpy(g_bad, g_buf, n);
        wr32(g_bad + 8, body + (uint32_t)deltas[i]);
        CHECK_STATUS(cc_meta_decode(g_bad, n, &g_out), CC_E_FORMAT);
    }
    wr32(g_bad + 8, 0xFFFFFFFFu);
    CHECK_STATUS(cc_meta_decode(g_bad, n, &g_out), CC_E_FORMAT);
    /* trailing bytes, even if body_len is raised to cover them */
    memcpy(g_bad, g_buf, n);
    g_bad[n] = 0;
    CHECK_STATUS(cc_meta_decode(g_bad, n + 1, &g_out), CC_E_FORMAT);
    wr32(g_bad + 8, body + 1);
    CHECK_STATUS(cc_meta_decode(g_bad, n + 1, &g_out), CC_E_FORMAT);
    /* longer than any valid blob */
    static uint8_t huge[CC_WIRE_META_MAX + 100];
    memcpy(huge, g_buf, n);
    wr32(huge + 8, (uint32_t)(sizeof huge - 12));
    CHECK_STATUS(cc_meta_decode(huge, sizeof huge, &g_out), CC_E_FORMAT);
    /* a refused blob leaves an all-zero (invalid) state, never a half-read one */
    memset(&g_out, 0x5A, sizeof g_out);
    CHECK_STATUS(cc_meta_decode(g_bad, n + 1, &g_out), CC_E_FORMAT);
    int nonzero = 0;
    const uint8_t *o = (const uint8_t *)&g_out;
    for (size_t i = 0; i < sizeof g_out; i++) nonzero += o[i] != 0;
    CHECK_EQ(nonzero, 0);
    CHECK_STATUS(cc_meta_validate(&g_out), CC_E_VERSION);
}

static void test_every_truncation(void)
{
    build_mid();
    const size_t n = enc(g_buf, sizeof g_buf);
    int accepted = 0;
    for (size_t k = 0; k < n; k++) {
        /* exact-size heap copies, so a read past the end is an address error under the sanitizer, not a read of the next static byte.
         * The length in the header is left as it was, and also corrected to the new length: both must refuse */
        uint8_t *cut = malloc(k ? k : 1);
        if (!cut) { CHECK(0); return; }
        memcpy(cut, g_buf, k);
        if (cc_meta_decode(cut, k, &g_out) == CC_OK) accepted++;
        if (k >= 12) {
            wr32(cut + 8, (uint32_t)(k - 12));
            if (cc_meta_decode(cut, k, &g_out) == CC_OK) accepted++;
        }
        free(cut);
    }
    CHECK_EQ(accepted, 0);
}

/* flips one bit of every byte: the result is refused, or it is a different valid state that re-encodes to exactly those bytes */
static void flip_sweep(const uint8_t *blob, size_t n, size_t stride, int *accepted, int *refused, int *noncanonical)
{
    for (size_t i = 0; i < n; i += stride) {
        for (int bit = 0; bit < 8; bit++) {
            memcpy(g_bad, blob, n);
            g_bad[i] ^= (uint8_t)(1u << bit);
            const cc_status s = cc_meta_decode(g_bad, n, &g_out);
            if (s != CC_OK) {
                (*refused)++;
                continue;
            }
            (*accepted)++;
            size_t m = 0;
            if (cc_meta_encode(&g_out, g_buf2, sizeof g_buf2, &m) != CC_OK || m != n || memcmp(g_buf2, g_bad, n) != 0)
                (*noncanonical)++;
        }
    }
}

static void test_bit_flips(void)
{
    int acc = 0, ref = 0, nc = 0;
    build_mid();
    size_t n = enc(g_buf, sizeof g_buf);
    static uint8_t keep[CC_WIRE_META_MAX];
    memcpy(keep, g_buf, n);
    flip_sweep(keep, n, 1, &acc, &ref, &nc);
    CHECK_EQ(nc, 0);
    CHECK(ref > 0);
    CHECK(acc > 0);                                      /* plenty of flips are still valid states (a changed HP, a level...) */
    CHECK_EQ(acc + ref, (int)n * 8);

    acc = ref = nc = 0;
    build_max();
    n = enc(g_buf, sizeof g_buf);
    memcpy(keep, g_buf, n);
    flip_sweep(keep, n, 13, &acc, &ref, &nc);
    CHECK_EQ(nc, 0);
    CHECK(ref > 0);
}

/* patches one field of a valid blob; the blob keeps its length, so only the field's own rule can refuse it */
static cc_status patched(size_t at, size_t width, uint64_t v)
{
    build_mid();
    const size_t n = enc(g_buf, sizeof g_buf);
    memcpy(g_bad, g_buf, n);
    for (size_t i = 0; i < width; i++) g_bad[at + i] = (uint8_t)(v >> (8 * i));
    return cc_meta_decode(g_bad, n, &g_out);
}

static void test_field_rules(void)
{
    /* counts */
    CHECK_STATUS(patched(32, 1, 9), CC_E_COUNT);         /* n_ledger over 8 */
    CHECK_STATUS(patched(77, 1, 17), CC_E_COUNT);        /* n_records over 16 */
    CHECK_STATUS(patched(77, 1, 255), CC_E_COUNT);
    CHECK_STATUS(patched(816, 1, 2), CC_E_RANGE);        /* has_report is 0 or 1 */
    CHECK_STATUS(patched(816, 1, 255), CC_E_RANGE);
    CHECK_STATUS(patched(858, 1, 6), CC_E_COUNT);        /* n_turns over 5 */
    CHECK_STATUS(patched(871, 1, 65), CC_E_COUNT);       /* n_events over 64 */
    CHECK(patched(32, 1, 3) != CC_OK);                   /* a count that promises more than the body holds */
    CHECK(patched(32, 1, 1) != CC_OK);                   /* ... or fewer: the entry left over is read as the next field */
    /* counters */
    CHECK_STATUS(patched(12, 4, 0), CC_E_RANGE);         /* next_record_id 0 */
    CHECK_STATUS(patched(16, 4, 0), CC_E_RANGE);
    CHECK_STATUS(patched(24, 8, 0), CC_E_RANGE);
    CHECK_STATUS(patched(12, 4, 2), CC_E_CONSISTENCY);   /* a record id at or above the counter */
    CHECK_STATUS(patched(16, 4, 3), CC_E_CONSISTENCY);   /* a run uid the install never issued (the report's run 3) */
    CHECK_STATUS(patched(20, 4, 0x11223345), CC_E_CONSISTENCY);   /* another install's nonce */
    CHECK_STATUS(patched(24, 8, 3), CC_E_CONSISTENCY);   /* the ledger and the report are above the txn counter */
    /* ledger */
    CHECK_STATUS(patched(33, 8, 4), CC_E_ORDER);         /* txn 4 after entry 1's txn 3 */
    CHECK_STATUS(patched(55, 8, 4), CC_E_CONSISTENCY);   /* txn 4 is not below the counter 4 */
    CHECK_STATUS(patched(53, 1, 0), CC_E_RANGE);         /* kind NONE */
    CHECK_STATUS(patched(53, 1, 3), CC_E_RANGE);
    CHECK_STATUS(patched(54, 1, 0), CC_E_RANGE);
    CHECK_STATUS(patched(54, 1, 4), CC_E_RANGE);
    CHECK_STATUS(patched(49, 4, 0), CC_E_RANGE);         /* an INSERTED entry names a record */
    CHECK_STATUS(patched(71, 4, 1), CC_E_NONCANONICAL);  /* a REPORT_ONLY entry names none */
    /* roster record 0 at 78 */
    CHECK_STATUS(patched(78, 4, 0), CC_E_RANGE);         /* record id 0 */
    CHECK_STATUS(patched(78, 4, 2), CC_E_DUPLICATE);     /* the same id twice */
    CHECK_STATUS(patched(447, 4, 1), CC_E_DUPLICATE);
    CHECK_STATUS(patched(82, 1, 0), CC_E_RANGE);         /* life NONE */
    CHECK_STATUS(patched(82, 1, 6), CC_E_RANGE);
    CHECK_STATUS(patched(82, 1, 2), CC_E_CONSISTENCY);   /* SCHEDULED without its run */
    CHECK_STATUS(patched(83, 1, 1), CC_E_CONSISTENCY);   /* a stored record with a trait */
    CHECK_STATUS(patched(84, 2, 0x0400), CC_E_UNKNOWN_BITS);
    CHECK_STATUS(patched(84, 2, 0x01BF), CC_E_CONSISTENCY);   /* FINISHER_VERIFIED dropped: a stored record has to be supported */
    CHECK_STATUS(patched(84, 2, 0x03FF), CC_E_CONSISTENCY);   /* LOADOUT_TRUNCATED with 1 item */
    CHECK_STATUS(patched(84, 2, 0x03BF & ~CC_NK_NATURE), CC_E_NONCANONICAL);   /* NATURE dropped: its value stays */
    CHECK_STATUS(patched(86, 8, 0), CC_E_RANGE);         /* source run uid 0 */
    CHECK_STATUS(patched(86, 8, UID(9)), CC_E_CONSISTENCY);   /* ... or one never issued */
    CHECK_STATUS(patched(94, 4, 0), CC_E_RANGE);         /* source pokemon id */
    CHECK_STATUS(patched(94, 4, 901), CC_OK);            /* a different pokemon of another run: valid */
    CHECK_STATUS(patched(98, 2, 0), CC_E_RANGE);         /* source wave */
    CHECK_STATUS(patched(100, 1, 51), CC_E_RANGE);       /* biome over 50 */
    CHECK_STATUS(patched(101, 2, 0), CC_E_RANGE);        /* species */
    CHECK_STATUS(patched(101, 2, 8902), CC_E_RANGE);
    CHECK_STATUS(patched(104, 1, 2), CC_E_RANGE);        /* gender */
    CHECK_STATUS(patched(104, 1, 0xFE), CC_E_RANGE);     /* -2 */
    CHECK_STATUS(patched(105, 1, 2), CC_E_RANGE);        /* shiny */
    CHECK_STATUS(patched(106, 1, 0), CC_E_RANGE);        /* original level */
    CHECK_STATUS(patched(107, 1, 25), CC_E_RANGE);       /* nature */
    CHECK_STATUS(patched(108, 2, 0), CC_E_RANGE);        /* ability, known */
    CHECK_STATUS(patched(108, 2, 320), CC_E_RANGE);
    CHECK_STATUS(patched(110, 2, 320), CC_E_RANGE);      /* passive */
    CHECK_STATUS(patched(112, 1, 32), CC_E_RANGE);       /* an IV */
    CHECK_STATUS(patched(118, 1, 0), CC_E_RANGE);        /* n_moves 0 with moves known */
    CHECK_STATUS(patched(118, 1, 5), CC_E_COUNT);
    CHECK_STATUS(patched(119, 2, 0), CC_E_RANGE);        /* a listed move of 0 */
    CHECK_STATUS(patched(121, 2, 33), CC_E_DUPLICATE);   /* the same move twice */
    CHECK_STATUS(patched(123, 2, 7), CC_E_NONCANONICAL); /* a move after the last */
    /* the loadout: n_items at 127, item 0 at 128 (key 33, arg i16, stack u16) */
    CHECK_STATUS(patched(127, 1, 9), CC_E_COUNT);
    CHECK_STATUS(patched(128, 1, 'l'), CC_E_RANGE);      /* a lower-case key */
    CHECK_STATUS(patched(128, 1, 0), CC_E_RANGE);        /* an empty key */
    CHECK_STATUS(patched(161, 2, 0xFFFE), CC_E_RANGE);   /* arg -2 */
    CHECK_STATUS(patched(163, 2, 0), CC_E_RANGE);        /* stack 0 */
    CHECK_STATUS(patched(166, 1, 'X'), CC_E_NONCANONICAL);   /* a nonzero byte in an unused item slot */
}

static void test_zero_report_and_canonical_gaps(void)
{
    /* no report: nothing after has_report; decoding it works and the report area stays all zero */
    build_mid();
    g_meta.has_report = 0;
    memset(&g_meta.report, 0, sizeof g_meta.report);
    memset(&g_meta.history, 0, sizeof g_meta.history);
    size_t n = enc(g_buf, sizeof g_buf);
    CHECK_EQ(n, 817);
    CHECK_STATUS(cc_meta_decode(g_buf, n, &g_out), CC_OK);
    CHECK_EQ(g_out.has_report, 0);
    CHECK_EQ(g_out.report.run_uid, 0);
    /* a report with an empty history is valid (n_turns 0) */
    build_mid();
    g_meta.history.n_turns = 0;
    memset(g_meta.history.turns, 0, sizeof g_meta.history.turns);
    n = enc(g_buf, sizeof g_buf);
    CHECK_EQ(n, 860);
    CHECK_STATUS(cc_meta_decode(g_buf, n, &g_out), CC_OK);
    CHECK_EQ(g_out.history.n_turns, 0);
    /* extra bytes after a no-report blob are refused even when body_len covers them */
    build_mid();
    g_meta.has_report = 0;
    memset(&g_meta.report, 0, sizeof g_meta.report);
    memset(&g_meta.history, 0, sizeof g_meta.history);
    n = enc(g_buf, sizeof g_buf);
    memcpy(g_bad, g_buf, n);
    g_bad[n] = 0;
    wr32(g_bad + 8, (uint32_t)(n - 11));
    CHECK_STATUS(cc_meta_decode(g_bad, n + 1, &g_out), CC_E_FORMAT);
}

/* a deterministic stream of random states and damage: whatever comes out of decode is a valid state that re-encodes identically */
static uint32_t g_rng = 0x2545F491u;
static uint32_t rnd(void)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

static void test_random_damage(void)
{
    int accepted = 0, refused = 0, noncanonical = 0;
    for (int round = 0; round < 3000; round++) {
        if (round % 2) build_mid(); else build_max();
        size_t n = enc(g_buf, sizeof g_buf);
        memcpy(g_bad, g_buf, n);
        const unsigned hits = 1 + rnd() % 4;
        for (unsigned h = 0; h < hits; h++) g_bad[12 + rnd() % (n - 12)] = (uint8_t)rnd();
        if (rnd() % 8 == 0) n = 12 + rnd() % (n - 12);                      /* also cut short */
        if (cc_meta_decode(g_bad, n, &g_out) != CC_OK) {
            refused++;
            continue;
        }
        accepted++;
        size_t m = 0;
        if (cc_meta_encode(&g_out, g_buf2, sizeof g_buf2, &m) != CC_OK || m != n || memcmp(g_buf2, g_bad, n) != 0) noncanonical++;
    }
    CHECK_EQ(noncanonical, 0);
    CHECK(refused > 100);
    printf("random damage: %d refused, %d accepted as other valid states\n", refused, accepted);
}

int main(void)
{
    test_empty_exact_bytes();
    test_mid_layout();
    test_signed_fields();
    test_max();
    test_encode_refuses();
    test_envelope_refusals();
    test_every_truncation();
    test_bit_flips();
    test_field_rules();
    test_zero_report_and_canonical_gaps();
    test_random_damage();
    return cc_summary("codec_test");
}
