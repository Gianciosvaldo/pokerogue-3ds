/* Tests of the JoJo trainers (cc/jojo.h): the table's structure and the roll. Expected values of the roll are literals printed by
 * tools/golden_jojo.py (an independent copy written apart from the C code). */
#include <string.h>

#include "cc/jojo.h"
#include "cc/scheduler.h"
#include "check.h"

static const int SEQ[5][12] = {
    {14, 4, 14, 9, 1, 30, 14, 4, 13, 41, 20, 24},
    {5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5},
    {14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14},
    {0, 12, 25, 99, 0, 12, 25, 99, 0, 12, 25, 99},
    {18, 23, 26, 2, 21, 8, 27, 14, 14, 14, 14, 14},   /* the biomes only the second batch of characters has, then the crowded Desert */
};
static const struct { uint64_t seed; int seq; int want[12]; uint64_t state; } GOLDEN[] = {
    {0x0ULL, 0, {22, -1, 1, 3, -1, -1, -1, 2, -1, -1, -1, -1}, 0x2DC6E5E736F71B91ULL},
    {0x0ULL, 1, {6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, 0x86BE3DC240E4526BULL},
    {0x0ULL, 2, {22, -1, 1, 27, -1, -1, -1, 28, -1, -1, -1, -1}, 0xB4E8FF0139CD2B3DULL},
    {0x0ULL, 3, {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, 0x4A4F4A4F424F5A41ULL},
    {0x0ULL, 4, {19, -1, 30, 33, -1, -1, -1, 28, -1, -1, -1, -1}, 0x2DC6E5E736F71B91ULL},
    {0x1ULL, 0, {-1, -1, -1, 3, 9, -1, 27, -1, -1, -1, -1, -1}, 0x8F8F6C2DB7AC9F7BULL},
    {0x1ULL, 1, {-1, -1, -1, 6, -1, -1, -1, -1, -1, -1, -1, -1}, 0x6164AAEEBEC3C6A9ULL},
    {0x1ULL, 2, {-1, -1, -1, 27, 1, -1, 22, -1, -1, -1, -1, -1}, 0x8F8F6C2DB7AC9F7BULL},
    {0x1ULL, 3, {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, 0x4A4F4A4F424F5A40ULL},
    {0x1ULL, 4, {-1, -1, -1, 33, 23, -1, 29, -1, -1, -1, -1, -1}, 0x8F8F6C2DB7AC9F7BULL},
    {0x12d687ULL, 0, {-1, 2, 22, -1, 9, -1, -1, -1, -1, -1, 17, 20}, 0x2DC6E5E737054E16ULL},
    {0x12d687ULL, 1, {-1, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, 0x24F5B77BC03D0105ULL},
    {0x12d687ULL, 2, {-1, 28, 1, -1, 22, -1, -1, -1, -1, 27, -1, -1}, 0xF157F274387055ECULL},
    {0x12d687ULL, 3, {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, 0x4A4F4A4F425D8CC6ULL},
    {0x12d687ULL, 4, {-1, 21, 30, -1, 23, -1, -1, -1, -1, 22, 27, 28}, 0x6A35D95A359A4640ULL},
    {0xdeadbeefcafef00dULL, 0, {28, 2, -1, -1, -1, 10, -1, -1, -1, -1, 17, 13}, 0x785A90387D596B9CULL},
    {0xdeadbeefcafef00dULL, 1, {6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, 0xD151E8138746A276ULL},
    {0xdeadbeefcafef00dULL, 2, {28, 27, -1, -1, -1, 1, -1, -1, -1, 22, -1, -1}, 0x3BEB9CC57EC47372ULL},
    {0xdeadbeefcafef00dULL, 3, {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, 0x94E2F4A088B1AA4CULL},
    {0xdeadbeefcafef00dULL, 4, {19, 21, -1, -1, -1, 24, -1, -1, -1, 22, 28, -1}, 0x169209F1FCA3E7B1ULL},
};
static const struct { uint64_t seed; int want[12]; uint64_t state; } GOLDEN_SMALL[] = {
    {0, {1, -1, 0, 2, -1, -1, -1, -1, -1, -1, -1, -1}, 0xDA4291D4BBEDB6FEULL},
    {1, {-1, -1, -1, 1, 0, -1, 2, -1, -1, -1, -1, -1}, 0xB4E8FF0139CD2B3CULL},
    {2, {0, -1, 1, 2, -1, -1, -1, -1, -1, -1, 3, -1}, 0xDA4291D4BBEDB700ULL},
    {3, {-1, 0, 1, 2, -1, -1, -1, -1, -1, -1, -1, -1}, 0xDA4291D4BBEDB6FFULL},
    {4, {-1, -1, -1, 0, -1, 2, 1, -1, -1, -1, 3, -1}, 0xB4E8FF0139CD2B41ULL},
    {5, {1, -1, -1, -1, -1, -1, -1, 0, -1, -1, -1, 3}, 0x8F8F6C2DB7AC9F7FULL},
    {6, {-1, -1, 1, -1, 2, 0, -1, -1, -1, -1, 3, -1}, 0x16B18547BA82AF2EULL},
    {7, {2, 1, -1, -1, -1, -1, 0, -1, -1, -1, -1, -1}, 0xB4E8FF0139CD2B42ULL},
};

#define POOL_N 21
static const int POOL[POOL_N] = {14, 4, 9, 29, 7, 5, 10, 19, 1, 30, 24, 20, 13, 41, 18, 23, 26, 2, 21, 8, 27};   /* the biomes some character is in */
static const int BIOME_OF[34] = {-1, 14, 4, 9, 29, 7, 5, 10, 19, 1, 30, -1, -1, 24, 20, 13, 41,
                                 20, 29, 18, 24, 23, 14, 21, 8, 19, 30, 14, 14, 27, 26, 1, 41, 2};

static int same(const cc_jojo_state *a, const cc_jojo_state *b) { return a->rng_state == b->rng_state && a->used == b->used && a->fights == b->fights; }

static void test_table(void)
{
    CHECK_EQ(cc_jojo_count, 34u);
    unsigned pool = 0, passive = 0, members = 0;
    for (unsigned i = 0; i < cc_jojo_count && i < 34; i++) {
        const cc_jojo_char *c = &cc_jojo_chars[i];
        CHECK(c->id && c->id[0] && c->name && c->name[0] && c->line && c->line[0]);
        CHECK(strlen(c->line) <= 80);
        CHECK_EQ(c->biome, BIOME_OF[i]);
        CHECK(c->n_members >= 1 && c->n_members <= CC_JOJO_MAX_MEMBERS);
        if (c->biome >= 0) pool++;
        if (c->passive >= 0) passive++;
        for (unsigned j = 0; j < i; j++) CHECK(strcmp(c->id, cc_jojo_chars[j].id) != 0);
        for (unsigned m = 0; m < c->n_members; m++) {
            const cc_jojo_member *mm = &c->member[m];
            members++;
            CHECK(mm->species > 0);
            CHECK(mm->ability_slot <= 2);
            CHECK(mm->shiny <= 1);
            CHECK(mm->variant <= 2);
            CHECK(mm->shiny || mm->variant == 0);
            CHECK(mm->moves[0] > 0);
            for (int k = 0; k < 4; k++) {
                if (mm->moves[k] == 0) continue;
                for (int l = 0; l < k; l++) CHECK(mm->moves[k] != mm->moves[l]);
            }
            CHECK(mm->n_items <= CC_JOJO_MAX_ITEMS);
            for (unsigned k = 0; k < mm->n_items; k++) {
                CHECK(mm->items[k].key && mm->items[k].key[0]);
                CHECK(mm->items[k].stack >= 1);
            }
        }
    }
    CHECK_EQ(pool, 31u);       /* the Pillar Men are not in a biome pool */
    CHECK_EQ(passive, 1u);     /* Pucci only */
    CHECK_EQ(members, 168u);
    CHECK_EQ(cc_jojo_find("kira"), 3);
    CHECK_EQ(cc_jojo_find("stroheim"), 16);
    CHECK_EQ(cc_jojo_find("caesar"), 17);
    CHECK_EQ(cc_jojo_find("giorno"), 33);
    CHECK_EQ(cc_jojo_find("kir"), CC_JOJO_NONE);
    CHECK_EQ(cc_jojo_find("kiraa"), CC_JOJO_NONE);
    CHECK_EQ(cc_jojo_find(NULL), CC_JOJO_NONE);
    CHECK_EQ(cc_jojo_chars[4].passive, 3);   /* Pucci: Speed Boost */
    /* a Ditto has only Transform */
    CHECK_EQ(cc_jojo_chars[0].member[1].species, 132);
    CHECK_EQ(cc_jojo_chars[0].member[1].moves[1], 0);
}

static void test_roll_table(void)
{
    for (unsigned g = 0; g < sizeof GOLDEN / sizeof *GOLDEN; g++) {
        cc_jojo_state s;
        cc_jojo_start(&s, GOLDEN[g].seed);
        for (int i = 0; i < 12; i++) CHECK_EQ(cc_jojo_roll(&s, SEQ[GOLDEN[g].seq][i]), GOLDEN[g].want[i]);
        CHECK(s.rng_state == GOLDEN[g].state);
        CHECK_STATUS(cc_jojo_validate(&s), CC_OK);
    }
}

static void test_roll_small(void)
{
    /* three characters of biome 7 and one of biome 5: the pick among several */
    cc_jojo_char t[4];
    memset(t, 0, sizeof t);
    t[0].biome = t[1].biome = t[2].biome = 7;
    t[3].biome = 5;
    for (unsigned g = 0; g < sizeof GOLDEN_SMALL / sizeof *GOLDEN_SMALL; g++) {
        cc_jojo_state s;
        cc_jojo_start(&s, GOLDEN_SMALL[g].seed);
        for (int i = 0; i < 12; i++) CHECK_EQ(cc_jojo_roll_in(&s, t, 4, i < 10 ? 7 : 5), GOLDEN_SMALL[g].want[i]);
        CHECK(s.rng_state == GOLDEN_SMALL[g].state);
    }
}

static void test_rules(void)
{
    /* no limit per run, but never the same character twice, never a Pillar Man, never outside the biome */
    unsigned runs_over_two = 0;
    unsigned long total = 0;
    for (unsigned seed = 0; seed < 4000; seed++) {
        cc_jojo_state s;
        cc_jojo_start(&s, seed * 7919u + 1);
        uint64_t seen = 0;
        unsigned dealt = 0;
        for (int w = 0; w < 60; w++) {
            const int biome = POOL[(w * 5 + (int)seed) % POOL_N];
            const int r = cc_jojo_roll(&s, biome);
            if (r == CC_JOJO_NONE) continue;
            CHECK(r >= 0 && r < 34);
            CHECK(cc_jojo_chars[r].biome == biome);
            CHECK(!(seen >> r & 1ull));
            CHECK(cc_jojo_chars[r].biome >= 0);
            seen |= 1ull << r;
            dealt++;
        }
        CHECK(dealt <= 31);
        CHECK_EQ(s.fights, dealt);
        CHECK_EQ(s.used, seen);
        total += dealt;
        if (dealt > 2) runs_over_two++;
    }
    /* with 60 eligible waves over the 21 pool biomes a run deals about 15 characters (the independent copy in tools/golden_jojo.py: 14.99); the old cap of 2 is gone */
    CHECK(runs_over_two > 3950);
    CHECK(total > 4000ul * 14 && total < 4000ul * 16);

    /* the chance: a fresh state and one eligible wave in a biome with a character deals one about 30% of the time */
    unsigned deal = 0;
    for (unsigned seed = 0; seed < 20000; seed++) {
        cc_jojo_state s;
        cc_jojo_start(&s, seed);
        if (cc_jojo_roll(&s, 14) != CC_JOJO_NONE) deal++;
    }
    CHECK(deal > 5700 && deal < 6300);   /* 20000 * 0.30 = 6000, about 4.5 standard deviations either side (the independent copy: 5863) */

    /* nothing is drawn when nobody can be dealt: a biome with no character, a Pillar Man's biome */
    cc_jojo_state s, before;
    cc_jojo_start(&s, 99);
    before = s;
    CHECK_EQ(cc_jojo_roll(&s, 0), CC_JOJO_NONE);        /* Town: no character */
    CHECK_EQ(cc_jojo_roll(&s, 25), CC_JOJO_NONE);       /* Space: Kars, not in a pool */
    CHECK_EQ(cc_jojo_roll(&s, 12), CC_JOJO_NONE);       /* Badlands: Wamuu */
    CHECK_EQ(cc_jojo_roll(&s, 22), CC_JOJO_NONE);       /* Ruins: nobody */
    CHECK_EQ(cc_jojo_roll(&s, -1), CC_JOJO_NONE);
    CHECK(same(&s, &before));
    s.used = 3u << 5;                                    /* two characters dealt already: others still can be */
    s.fights = 2;
    unsigned more = 0;
    for (unsigned seed = 0; seed < 200; seed++) {
        cc_jojo_state t = s;
        t.rng_state = seed;
        if (cc_jojo_roll(&t, 14) == 1) more++;           /* Gyro is index 1 */
    }
    CHECK(more > 5 && more < 26);                        /* 30% of 200 seeds, a quarter of them Gyro (one of the four Desert characters): about 15; the independent copy: 17 */
}

static void test_validate(void)
{
    cc_jojo_state s;
    cc_jojo_start(&s, 5);
    CHECK_STATUS(cc_jojo_validate(&s), CC_OK);
    CHECK_STATUS(cc_jojo_validate(NULL), CC_E_NULL);
    s.used = 1ull << 34;                                 /* a character the table does not have */
    s.fights = 1;
    CHECK_STATUS(cc_jojo_validate(&s), CC_E_RANGE);
    s.used = 1;
    s.fights = 2;                                        /* count disagrees with the mask */
    CHECK_STATUS(cc_jojo_validate(&s), CC_E_CONSISTENCY);
    s.used = 7;
    s.fights = 3;                                        /* three characters are fine now */
    CHECK_STATUS(cc_jojo_validate(&s), CC_OK);
    /* an invalid state deals nobody and is left alone (one that would otherwise deal: some seed hits the 30%) */
    for (unsigned seed = 0; seed < 200; seed++) {
        cc_jojo_state bad, copy;
        cc_jojo_start(&bad, seed);
        bad.used = 1ull << 34;
        bad.fights = 1;
        copy = bad;
        CHECK_EQ(cc_jojo_roll(&bad, 14), CC_JOJO_NONE);
        CHECK(same(&bad, &copy));
    }
    CHECK_EQ(cc_jojo_roll(NULL, 14), CC_JOJO_NONE);
    /* the stream is apart from the rematch stream: the same seed gives other numbers */
    cc_jojo_start(&s, 77);
    uint64_t other = 77;
    CHECK(s.rng_state != other);
    uint64_t a = s.rng_state;
    CHECK(cc_rng_v1_next(&a) != cc_rng_v1_next(&other));
}

static void test_wide_table(void)
{
    /* more than 32 characters: the mask is 64 bits, so a table of 40 in one biome is dealt completely, nobody twice */
    cc_jojo_char big[64];
    memset(big, 0, sizeof big);
    for (unsigned i = 0; i < 64; i++) big[i].biome = i < 40 ? 7 : 5;
    for (unsigned seed = 0; seed < 50; seed++) {
        cc_jojo_state s;
        cc_jojo_start(&s, seed);
        uint64_t seen = 0;
        unsigned dealt = 0;
        for (int w = 0; w < 2000 && dealt < 40; w++) {
            const int r = cc_jojo_roll_in(&s, big, 40, 7);
            if (r == CC_JOJO_NONE) continue;
            CHECK(r >= 0 && r < 40);
            CHECK(!(seen >> r & 1ull));
            seen |= 1ull << r;
            dealt++;
            CHECK_EQ(s.fights, dealt);
            CHECK(s.used == seen);
            CHECK_STATUS(cc_jojo_validate_in(&s, 40), CC_OK);
        }
        CHECK_EQ(dealt, 40u);
        CHECK(s.used == (1ull << 40) - 1);
        CHECK_EQ(cc_jojo_roll_in(&s, big, 40, 7), CC_JOJO_NONE);   /* everybody is dealt: nothing left, nothing drawn */
    }
    /* the pick reaches the characters of the high half (indexes 32..39) */
    unsigned high = 0;
    for (unsigned seed = 0; seed < 400; seed++) {
        cc_jojo_state s;
        cc_jojo_start(&s, seed);
        s.used = (1ull << 32) - 1;                            /* the low half is dealt already */
        s.fights = 32;
        const int r = cc_jojo_roll_in(&s, big, 40, 7);
        if (r != CC_JOJO_NONE) {
            CHECK(r >= 32 && r < 40);
            CHECK_EQ(s.fights, 33u);
            high++;
        }
    }
    CHECK(high > 50);
    /* the last slot, index 63, and the validation at the limits */
    cc_jojo_state s;
    cc_jojo_start(&s, 1);
    s.used = 1ull << 63;
    s.fights = 1;
    CHECK_STATUS(cc_jojo_validate_in(&s, 64), CC_OK);
    CHECK_STATUS(cc_jojo_validate_in(&s, 63), CC_E_RANGE);     /* a character a 63-character table does not have */
    CHECK_STATUS(cc_jojo_validate_in(&s, 65), CC_E_RANGE);     /* a table the mask cannot hold */
    CHECK_STATUS(cc_jojo_validate_in(&s, 40), CC_E_RANGE);
    s.used = 1ull << 40;
    CHECK_STATUS(cc_jojo_validate_in(&s, 40), CC_E_RANGE);     /* index 40 of a 40-character table */
    CHECK_STATUS(cc_jojo_validate_in(&s, 41), CC_OK);
    s.fights = 2;
    CHECK_STATUS(cc_jojo_validate_in(&s, 41), CC_E_CONSISTENCY);
    /* a table of 65 deals nobody and is left alone */
    cc_jojo_state c;
    cc_jojo_start(&c, 9);
    cc_jojo_state keep = c;
    cc_jojo_char huge[65];
    memset(huge, 0, sizeof huge);
    for (unsigned i = 0; i < 65; i++) huge[i].biome = 7;
    for (int i = 0; i < 50; i++) CHECK_EQ(cc_jojo_roll_in(&c, huge, 65, 7), CC_JOJO_NONE);
    CHECK(same(&c, &keep));
}

/* The chance is exactly 30 in 100: a character is dealt when the first draw of the stream (0..99) is below 30. Seeds are scanned for the draws
 * around the line (28..31) so a chance of 29 or 31 and a comparison off by one are all caught; the literal 30 is on purpose (a change of the
 * chance has to change this test and the goldens with it, tools/golden_jojo.py). */
static void test_chance_line(void)
{
    cc_jojo_char one[1];
    memset(one, 0, sizeof one);
    one[0].biome = 5;
    int hit[100] = {0};
    for (uint64_t seed = 0; seed < 4000; seed++) {
        cc_jojo_state s;
        cc_jojo_start(&s, seed);
        uint64_t probe = s.rng_state;
        unsigned d = cc_rng_v1_pick(&probe, 100);
        int r = cc_jojo_roll_in(&s, one, 1, 5);
        CHECK_EQ(r, d < 30 ? 0 : CC_JOJO_NONE);
        CHECK_EQ(s.fights, d < 30 ? 1 : 0);
        hit[d]++;
    }
    CHECK(hit[28] > 0 && hit[29] > 0 && hit[30] > 0 && hit[31] > 0);
}

int main(void)
{
    test_table();
    test_roll_table();
    test_roll_small();
    test_rules();
    test_validate();
    test_wide_table();
    test_chance_line();
    return cc_summary("jojo_test");
}
