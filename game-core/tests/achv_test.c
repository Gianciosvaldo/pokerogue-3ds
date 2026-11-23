/* The custom achievements (gc/achv.h): spec tests. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "gc/achv.h"
#include "gc/gacha.h"
#include "gc/run.h"
#include "gc/system.h"
#include "pr/reward.h"
#include "pr/run.h"

static gc_system S;

static void fresh(void) { gc_system_init_defaults(&S); }

static int eval(gc_achv_new *o) { return gc_achv_evaluate(&S, "testrun", 1000, o); }

static int has_text(const uint8_t *buf, size_t n, const char *t)
{
    const size_t k = strlen(t);
    for (size_t i = 0; i + k <= n; i++)
        if (!memcmp(buf + i, t, k)) return 1;
    return 0;
}

static uint32_t vouchers(void) { return S.voucher_counts[0] + S.voucher_counts[1] + S.voucher_counts[2] + S.voucher_counts[3]; }

int main(void)
{
    /* every challenger gives its own achievement and one regular voucher, once */
    for (int j = 0; j < GC_JOJO_COUNT; j++) {
        fresh();
        gc_achv_jojo_won(&S, j, 7, false);
        gc_achv_new n;
        assert(eval(&n) >= 1);
        assert(gc_achv_unlocked(&S, gc_achv_jojo_id(j)));
        for (int k = 0; k < GC_JOJO_COUNT; k++) assert(gc_achv_unlocked(&S, gc_achv_jojo_id(k)) == (k == j));   /* only this one's */
        assert(S.voucher_counts[GC_VOUCHER_REGULAR] >= 1);
        const uint32_t before = vouchers();
        assert(eval(&n) == 0 && n.n == 0);            /* nothing twice */
        gc_achv_jojo_won(&S, j, 7, false);
        assert(eval(&n) == 0 && vouchers() == before); /* the same challenger again gives nothing new */
    }

    /* the ids of the "Defeat X": the first 17 by index, the second batch from GC_ACHV_JOJO2_FIRST; all distinct, all JoJo group, none out of range */
    {
        assert(GC_JOJO_COUNT == 40 && GC_JOJO_CAESAR == 17 && GC_JOJO_GHIACCIO == 34 && GC_ACHV_FLAWLESS_FIRST + GC_JOJO_GHIACCIO + 2 == GC_ACHV_JOJO3_FIRST &&
               GC_ACHV_DARBY == GC_ACHV_JOJO3_FIRST - 2 && GC_ACHV_DARBY_REMATCH == GC_ACHV_JOJO3_FIRST - 1 && GC_ACHV_FLAWLESS_FIRST == GC_ACHV_TWENTY_FACES + 1 &&
               GC_ACHV_TWENTY_FACES == GC_ACHV_JOJO2_FIRST + 17 && GC_ACHV_JOBIN == GC_ACHV_JOJO3_FIRST + 2 * (GC_JOJO_OKUYASU - GC_JOJO_GHIACCIO) && GC_ACHV_JOBIN_PUNISH == GC_ACHV_JOBIN + 1 &&
               GC_ACHV_JOJO4_FIRST == GC_ACHV_JOBIN + 2 && GC_ACHV_COUNT == GC_ACHV_JOJO4_FIRST + 2 * (GC_JOJO_COUNT - GC_JOJO_OKUYASU));
        assert(GC_ACHV_COUNT <= 192);   /* upstream's own achievements use 192 and up in achievement_flags */
        assert(gc_achv_jojo_id(-1) == -1 && gc_achv_jojo_id(GC_JOJO_COUNT) == -1);
        for (int j = 0; j < GC_JOJO_COUNT; j++) {
            const int id = gc_achv_jojo_id(j);
            assert(id >= 0 && id < GC_ACHV_COUNT && gc_achv_defs[id].group == GC_ACHV_GROUP_JOJO);
            assert(id == (j < 17 ? j : j < 34 ? 100 + j - 17 : j < 39 ? 154 + 2 * (j - 34) : 166 + 2 * (j - 39)));   /* 164, 165 are Jobin's */
            for (int k = 0; k < j; k++) assert(gc_achv_jojo_id(k) != id);
        }
        assert(!strcmp(gc_achv_defs[gc_achv_jojo_id(GC_JOJO_GIORNO)].desc, "Defeat Giorno Giovanna.") && !strcmp(gc_achv_defs[gc_achv_jojo_id(GC_JOJO_KARS)].desc, "Defeat Kars."));
        /* lorenzo's names (2026-11-18): each a title of its own, at most 63 characters (the achievement bar's 64-byte copy), no two the same */
        assert(!strcmp(gc_achv_defs[gc_achv_jojo_id(GC_JOJO_KARS)].name, "Eventually, He Stopped Thinking") && !strcmp(gc_achv_defs[gc_achv_flawless_id(GC_JOJO_GIORNO)].name, "Primo Mafioso"));
        for (int a = 0; a < GC_ACHV_COUNT; a++) {
            assert(gc_achv_defs[a].name[0] && strlen(gc_achv_defs[a].name) <= 63);
            for (int b = 0; b < a; b++) assert(strcmp(gc_achv_defs[a].name, gc_achv_defs[b].name) != 0);
        }
        assert(strstr(gc_achv_defs[26].desc, "40"));   /* Every Face names the number */
        /* lorenzo's titles for the third batch (2026-11-22) */
        assert(!strcmp(gc_achv_defs[gc_achv_jojo_id(GC_JOJO_GHIACCIO)].name, "It's Pronounced Venezia!") && !strcmp(gc_achv_defs[gc_achv_flawless_id(GC_JOJO_GHIACCIO)].name, "Absolute Zero Casualties"));
        assert(!strcmp(gc_achv_defs[gc_achv_jojo_id(GC_JOJO_PROSCIUTTO)].name, "Cut the Line, Beat the Heat") && !strcmp(gc_achv_defs[gc_achv_flawless_id(GC_JOJO_PROSCIUTTO)].name, "Not a Scratch, Not a Wrinkle"));
        assert(!strcmp(gc_achv_defs[gc_achv_jojo_id(GC_JOJO_JODIO)].name, "Weathered the November Storm") && !strcmp(gc_achv_defs[gc_achv_flawless_id(GC_JOJO_JODIO)].name, "Not a Drop, Not a Dime"));
        assert(!strcmp(gc_achv_defs[gc_achv_jojo_id(GC_JOJO_JONATHAN)].name, "Beyond the Sunlight Yellow Overdrive") && !strcmp(gc_achv_defs[gc_achv_flawless_id(GC_JOJO_JONATHAN)].name, "Unscathed by the Sunlight's Ripple"));
        assert(!strcmp(gc_achv_defs[gc_achv_jojo_id(GC_JOJO_JONATHAN)].desc, "Defeat Jonathan Joestar.") && !strcmp(gc_achv_defs[gc_achv_jojo_id(GC_JOJO_PROSCIUTTO)].desc, "Defeat Prosciutto & Pesci."));
        for (int j = GC_JOJO_GHIACCIO; j < GC_JOJO_COUNT; j++)   /* the pair: "Defeat X" then its "Flawless X" */
            assert(gc_achv_flawless_id(j) == gc_achv_jojo_id(j) + 1 && gc_achv_defs[gc_achv_jojo_id(j)].reward == gc_achv_defs[gc_achv_jojo_id(0)].reward &&
                   gc_achv_defs[gc_achv_flawless_id(j)].n == gc_achv_defs[gc_achv_flawless_id(0)].n);
    }

    /* "Flawless X": beating that challenger with none of the party fainted; a win with a faint is not one and a later clean win still is; the
     * ids are 118 + the table index; they pay two regular vouchers and are the only thing a clean win adds for that character */
    {
        assert(gc_achv_flawless_id(-1) == -1 && gc_achv_flawless_id(GC_JOJO_COUNT) == -1 && gc_achv_flawless_id(0) == 118 && gc_achv_flawless_id(33) == 151);
        assert(gc_achv_flawless_id(GC_JOJO_GHIACCIO) == 155 && gc_achv_flawless_id(GC_JOJO_JONATHAN) == 161 && gc_achv_jojo_id(GC_JOJO_JONATHAN) == 160);
        assert(gc_achv_jojo_id(GC_JOJO_TOORU) == 162 && gc_achv_flawless_id(GC_JOJO_TOORU) == 163 && !strcmp(gc_achv_defs[162].name, "Broke the Flow of Calamity") &&
               !strcmp(gc_achv_defs[163].name, "We Went Beyond the Logic of This World") && !strcmp(gc_achv_defs[162].desc, "Defeat Tooru."));
        assert(gc_achv_jojo_id(GC_JOJO_OKUYASU) == 166 && gc_achv_flawless_id(GC_JOJO_OKUYASU) == 167 && !strcmp(gc_achv_defs[166].name, "Not as Dumb as He Looks") &&
               !strcmp(gc_achv_defs[167].name, "He Couldn't Erase a Single Thing") && !strcmp(gc_achv_defs[166].desc, "Defeat Okuyasu Nijimura."));   /* after Jobin's 164, 165 */
        for (int j = 0; j < GC_JOJO_COUNT; j++) {
            const int id = gc_achv_flawless_id(j);
            const char *base = gc_achv_defs[gc_achv_jojo_id(j)].desc;   /* "Defeat X." */
            assert(gc_achv_defs[id].group == GC_ACHV_GROUP_JOJO && strncmp(gc_achv_defs[id].desc, base, strlen(base) - 1) == 0 &&
                   strstr(gc_achv_defs[id].desc, "without any of your Pokemon fainting"));   /* "Defeat X without ..." */
            fresh();
            gc_achv_jojo_won(&S, j, 3, false);
            eval(NULL);
            assert(gc_achv_unlocked(&S, gc_achv_jojo_id(j)) && !gc_achv_unlocked(&S, id));         /* beaten, but not flawless */
            const uint32_t before = vouchers();
            gc_achv_jojo_won(&S, j, 4, true);                                                      /* again, clean this time */
            eval(NULL);
            assert(gc_achv_unlocked(&S, id));
            assert(vouchers() == before + 2 + 1);                                                  /* two regular vouchers, and "Untouched" (a Plus one) */
            for (int k = 0; k < GC_JOJO_COUNT; k++) assert(gc_achv_unlocked(&S, gc_achv_flawless_id(k)) == (k == j));
            gc_achv_jojo_won(&S, j, 5, false);                                                     /* a later faint takes nothing away */
            eval(NULL);
            assert(gc_achv_unlocked(&S, id));
        }
        fresh();
        S.achv.jojo_clean_mask_hi = 0xFFFFFF00u;                                                   /* bits of characters this build does not have (40..63) */
        eval(NULL);
        for (int j = 0; j < GC_JOJO_COUNT; j++) assert(!gc_achv_unlocked(&S, gc_achv_flawless_id(j)));
        fresh();
        S.achv.jojo_clean_mask = 0xFFFFFFFFu;                                                      /* a clean mask alone is not a beaten one, but the flag only asks the clean mask */
        S.achv.jojo_clean_mask_hi = 0xFFu;
        eval(NULL);
        for (int j = 0; j < GC_JOJO_COUNT; j++) assert(gc_achv_unlocked(&S, gc_achv_flawless_id(j)));
    }

    /* the sets */
    static const struct { int id; int members[9]; int n; } SETS[] = {
        {17, {GC_JOJO_KARS, GC_JOJO_WAMUU, GC_JOJO_ESIDISI}, 3},
        {18, {GC_JOJO_JOSEPH, GC_JOJO_KARS, GC_JOJO_WAMUU, GC_JOJO_ESIDISI, GC_JOJO_STROHEIM, GC_JOJO_CAESAR}, 6},
        {19, {GC_JOJO_JOTARO, GC_JOJO_DIO, GC_JOJO_POLNAREFF, GC_JOJO_KAKYOIN, GC_JOJO_AVDOL, GC_JOJO_VANILLAICE, GC_JOJO_HOLHORSE, GC_JOJO_IGGY}, 8},
        {20, {GC_JOJO_KOICHI, GC_JOJO_KIRA, GC_JOJO_JOSUKE, GC_JOJO_ROHAN, GC_JOJO_SHIGECHI, GC_JOJO_OKUYASU}, 6},
        {21, {GC_JOJO_DIAVOLO, GC_JOJO_MISTA, GC_JOJO_RISOTTO, GC_JOJO_GIORNO, GC_JOJO_BRUNO, GC_JOJO_ABBACCHIO, GC_JOJO_NARANCIA, GC_JOJO_GHIACCIO, GC_JOJO_PROSCIUTTO}, 9},
        {22, {GC_JOJO_JOLYNE, GC_JOJO_PUCCI, GC_JOJO_WEATHER}, 3},
        {23, {GC_JOJO_VALENTINE, GC_JOJO_GYRO, GC_JOJO_JOHNNY, GC_JOJO_SANDMAN, GC_JOJO_DIEGO}, 5},
    };
    for (unsigned k = 0; k < sizeof SETS / sizeof SETS[0]; k++) {
        fresh();
        for (int i = 0; i < SETS[k].n; i++) {
            assert(!gc_achv_unlocked(&S, SETS[k].id));
            gc_achv_jojo_won(&S, SETS[k].members[i], 1, false);
            eval(NULL);
        }
        assert(gc_achv_unlocked(&S, SETS[k].id));
        /* the progress line counts exactly the members, and a challenger outside the set adds nothing */
        uint32_t h, n;
        assert(gc_achv_progress_of(&S, SETS[k].id, &h, &n) && h == (uint32_t)SETS[k].n && n == (uint32_t)SETS[k].n);
    }
    /* every Part set but the Pillar Men's: the six Parts hold every challenger of Parts 2..7 (the Pillar Men sit in Battle Tendency) with nobody
     * in two; Jonathan (Part 1), Tooru (Part 8) and Jodio (Part 9) have no Part set */
    {
        fresh();
        uint32_t total = 0;
        for (int id = 18; id <= 23; id++) {
            uint32_t h, n;
            assert(gc_achv_progress_of(&S, id, &h, &n) && h == 0);
            total += n;
        }
        assert(total == GC_JOJO_COUNT - 3);
        for (int j = 0; j < GC_JOJO_COUNT; j++) {   /* one challenger moves exactly one Part set's line (Jonathan and Jodio none) */
            fresh();
            gc_achv_jojo_won(&S, j, 1, false);
            int moved = 0;
            for (int id = 18; id <= 23; id++) {
                uint32_t h, n;
                assert(gc_achv_progress_of(&S, id, &h, &n));
                moved += h;
            }
            assert(moved == (j == GC_JOJO_JONATHAN || j == GC_JOJO_JODIO || j == GC_JOJO_TOORU ? 0 : 1));
        }
    }

    /* counts of different challengers: 5, 10, 20 and all 34 (the last gives a tier-3 shiny egg) */
    fresh();
    for (int j = 0; j < GC_JOJO_COUNT; j++) {
        gc_achv_jojo_won(&S, j, 100 + j, false); /* a new run each time */
        eval(NULL);
        assert(gc_achv_unlocked(&S, 24) == (j + 1 >= 5));
        assert(gc_achv_unlocked(&S, 25) == (j + 1 >= 10));
        assert(gc_achv_unlocked(&S, GC_ACHV_TWENTY_FACES) == (j + 1 >= 20));
        assert(gc_achv_unlocked(&S, 26) == (j + 1 >= GC_JOJO_COUNT));
        assert(!gc_achv_unlocked(&S, 27));
    }
    assert(S.n_eggs == 1 && S.eggs[0].is_shiny && S.eggs[0].variant_tier == 2 && S.eggs[0].species != 0 && S.eggs[0].tier == 2);

    /* the beaten mask is 64 bits (two profile fields): challengers 32 and 33 (Rohan, Giorno) count towards the totals and the progress line
     * like the first 32 */
    fresh();
    for (int j = 0; j < 3; j++) gc_achv_jojo_won(&S, j, 40 + j, false);
    gc_achv_jojo_won(&S, GC_JOJO_ROHAN, 50, false);
    gc_achv_jojo_won(&S, GC_JOJO_GIORNO, 51, false);
    assert(S.achv.jojo_beaten == 7u && S.achv.jojo_beaten_hi == 3u);
    eval(NULL);
    uint32_t have = 0, need = 0;
    assert(gc_achv_unlocked(&S, 24) && !gc_achv_unlocked(&S, 25));   /* five different challengers, not ten */
    assert(gc_achv_progress_of(&S, 25, &have, &need) && have == 5 && need == 10);
    assert(gc_achv_progress_of(&S, 26, &have, &need) && have == 5 && need == GC_JOJO_COUNT);
    assert(gc_achv_unlocked(&S, gc_achv_jojo_id(GC_JOJO_ROHAN)) && gc_achv_unlocked(&S, gc_achv_jojo_id(GC_JOJO_GIORNO)));
    /* a bit for a character the table does not have (a profile from a later build) counts for nothing, and neither do the ones it does have
     * towards another one's achievement */
    fresh();
    S.achv.jojo_beaten_hi = 0xFFFFFF00u;                /* indexes 40..63 */
    eval(NULL);
    for (int j = 0; j < GC_JOJO_COUNT; j++) assert(!gc_achv_unlocked(&S, gc_achv_jojo_id(j)));
    assert(!gc_achv_unlocked(&S, 24) && gc_achv_progress_of(&S, 24, &have, &need) && have == 0);
    S.achv.jojo_beaten = 0xFFFFFFFFu;                   /* all 32 of the first word, and eight of the second: all 40 */
    S.achv.jojo_beaten_hi |= 0xFFu;
    eval(NULL);
    assert(gc_achv_progress_of(&S, 26, &have, &need) && have == GC_JOJO_COUNT && need == GC_JOJO_COUNT && gc_achv_unlocked(&S, 26));
    fresh();
    gc_achv_jojo_won(&S, 63, 1, false);                 /* an index the table does not have is refused: nothing recorded */
    gc_achv_jojo_won(&S, GC_JOJO_COUNT, 1, false);
    assert(S.achv.jojo_battles == 0 && S.achv.jojo_beaten == 0 && S.achv.jojo_beaten_hi == 0);

    /* Twenty Faces: the line, the reward (one Premium voucher) and where it sits in the list */
    {
        fresh();
        for (int j = 0; j < 20; j++) gc_achv_jojo_won(&S, j, 200 + j, false);
        eval(NULL);
        uint32_t h, n;
        assert(gc_achv_progress_of(&S, GC_ACHV_TWENTY_FACES, &h, &n) && h == 20 && n == 20);
        assert(S.voucher_counts[GC_VOUCHER_PREMIUM] == 1);
    }

    /* the list order: a permutation of the ids; the 34 "Defeat X" first in table order, then the other JoJo ones, then the rest by id */
    {
        int seen[GC_ACHV_COUNT] = {0};
        for (int pos = 0; pos < GC_ACHV_COUNT; pos++) {
            const int id = gc_achv_order(pos);
            assert(id >= 0 && id < GC_ACHV_COUNT && !seen[id]);
            seen[id] = 1;
        }
        assert(gc_achv_order(-1) == -1 && gc_achv_order(GC_ACHV_COUNT) == -1);
        for (int j = 0; j < GC_JOJO_COUNT; j++) assert(gc_achv_order(j) == gc_achv_jojo_id(j));
        for (int j = 0; j < GC_JOJO_COUNT; j++) assert(gc_achv_order(GC_JOJO_COUNT + j) == gc_achv_flawless_id(j));   /* then the 34 "Flawless X" */
        /* the other JoJo ones: 17..25, Twenty Faces, 26..29 */
        int pos = 2 * GC_JOJO_COUNT;
        for (int id = 17; id <= 29; id++) {
            assert(gc_achv_order(pos++) == id);
            if (id == 25) assert(gc_achv_order(pos++) == GC_ACHV_TWENTY_FACES);
        }
        /* everything else in id order, from the runs */
        for (int id = 30; id < GC_ACHV_JOJO2_FIRST; id++) assert(gc_achv_order(pos++) == id);
        assert(gc_achv_order(pos++) == GC_ACHV_DARBY);   /* D'Arby's two, the only non-JoJo ones after 99 */
        assert(gc_achv_order(pos++) == GC_ACHV_DARBY_REMATCH);
        assert(gc_achv_order(pos++) == GC_ACHV_JOBIN);   /* Jobin's two follow, in id order, after every id the third batch took */
        assert(gc_achv_order(pos++) == GC_ACHV_JOBIN_PUNISH);
        assert(pos == GC_ACHV_COUNT);   /* the third batch is in the two JoJo blocks above */
    }

    /* three in one run; the count belongs to a run */
    fresh();
    gc_achv_jojo_won(&S, 0, 5, false);
    gc_achv_jojo_won(&S, 1, 5, false);
    eval(NULL);
    assert(!gc_achv_unlocked(&S, 27));
    gc_achv_jojo_won(&S, 2, 6, false);   /* another run: counts from one */
    eval(NULL);
    assert(!gc_achv_unlocked(&S, 27));
    gc_achv_jojo_won(&S, 3, 6, false);
    gc_achv_jojo_won(&S, 4, 6, false);
    eval(NULL);
    assert(gc_achv_unlocked(&S, 27) && S.achv.best_run_jojo == 3);

    /* a clean win; a win with a faint is not one */
    fresh();
    gc_achv_jojo_won(&S, 0, 1, false);
    eval(NULL);
    assert(!gc_achv_unlocked(&S, 29));
    gc_achv_jojo_won(&S, 0, 1, true);
    eval(NULL);
    assert(gc_achv_unlocked(&S, 29) && S.voucher_counts[GC_VOUCHER_PLUS] == 1);

    /* a hundred battles: a legendary egg, and the gift moves neither the pity counters nor the pull statistics */
    fresh();
    S.egg_pity[3] = 5;
    for (int i = 0; i < 99; i++) gc_achv_jojo_won(&S, i % 3, 9, false);
    eval(NULL);
    assert(!gc_achv_unlocked(&S, 28) && S.n_eggs == 0);
    gc_achv_jojo_won(&S, 0, 9, false);
    assert(!gc_achv_unlocked(&S, 28));
    {
        gc_achv_new n;
        eval(&n);
        assert(gc_achv_unlocked(&S, 28));
        assert(S.n_eggs == 1 && S.eggs[0].tier == 3 && S.eggs[0].species != 0);
        assert(S.egg_pity[3] == 5 && S.stats.eggs_pulled == 0 && S.stats.legendary_eggs_pulled == 0);
    }
    {
        gc_system copy;
        fresh();
        for (int i = 0; i < 100; i++) gc_achv_jojo_won(&S, 0, 9, false);
        memcpy(&copy, &S, sizeof copy);
        gc_achv_evaluate(&S, "same", 5, NULL);
        gc_achv_evaluate(&copy, "same", 5, NULL);
        assert(S.n_eggs == 1 && S.eggs[0].id == copy.eggs[0].id && S.eggs[0].species == copy.eggs[0].species); /* the same text gives the same egg */
    }

    /* a full egg list holds the egg reward back, without losing the progress, until there is room */
    fresh();
    for (int i = 0; i < 99; i++) assert(gc_system_add_egg(&S, 1000 + i, 10, 0, 1));
    for (int i = 0; i < 100; i++) gc_achv_jojo_won(&S, 0, 9, false);
    eval(NULL);
    assert(!gc_achv_unlocked(&S, 28) && S.n_eggs == 99);
    assert(gc_achv_unlocked(&S, 0));
    S.n_eggs--;
    eval(NULL);
    assert(gc_achv_unlocked(&S, 28) && S.n_eggs == 99);

    /* save and load keep the progress and the unlocks; a profile with no progress is saved as before (no extra section) */
    {
        static uint8_t buf[4 << 20];
        fresh();
        size_t plain = gc_system_save(&S, buf, sizeof buf);
        assert(plain > 0);
        assert(!has_text(buf, plain, "achv"));
        gc_achv_jojo_won(&S, GC_JOJO_DIO, 77, true);
        gc_achv_jojo_won(&S, GC_JOJO_KARS, 77, false);
        eval(NULL);
        size_t n = gc_system_save(&S, buf, sizeof buf);
        assert(n > plain);
        static gc_system loaded;
        const char *why = NULL;
        gc_system_init_defaults(&loaded);
        assert(gc_system_load(&loaded, buf, n, &why));
        assert(!memcmp(&loaded.achv, &S.achv, sizeof S.achv));
        assert(gc_achv_unlocked(&loaded, GC_JOJO_DIO) && gc_achv_unlocked(&loaded, GC_JOJO_KARS));
        assert(loaded.voucher_counts[0] == S.voucher_counts[0]);
    }

    /* group B, the runs: Classic win counts (game-core counts them in stats.classic_wins), each with its reward */
    {
        static const struct { int id; uint32_t wins; } WINS[] = {{30, 1}, {31, 5}, {32, 10}, {33, 25}, {34, 50}};
        for (unsigned k = 0; k < sizeof WINS / sizeof WINS[0]; k++) {
            fresh();
            S.stats.classic_wins = WINS[k].wins - 1;
            eval(NULL);
            assert(!gc_achv_unlocked(&S, WINS[k].id));              /* one short */
            S.stats.classic_wins = WINS[k].wins;
            eval(NULL);
            assert(gc_achv_unlocked(&S, WINS[k].id));
            for (unsigned j = k + 1; j < sizeof WINS / sizeof WINS[0]; j++) assert(!gc_achv_unlocked(&S, WINS[j].id));   /* the next ones wait */
        }
        fresh();
        S.stats.classic_wins = 50;
        gc_achv_new fresh_n;
        eval(&fresh_n);
        assert(fresh_n.n == 5);
        assert(S.n_eggs == 2);                                       /* the Legendary egg and the tier 3 shiny egg */
        assert(S.eggs[0].tier == 3 || S.eggs[1].tier == 3);
        assert(S.eggs[0].variant_tier == 2 || S.eggs[1].variant_tier == 2);
        fresh();
        gc_achv_run_won(&S, false, 0);
        eval(NULL);
        assert(!gc_achv_unlocked(&S, 35));                           /* a Classic win is no Custom win */
        gc_achv_run_won(&S, true, 0);
        eval(NULL);
        assert(gc_achv_unlocked(&S, 35) && !gc_achv_unlocked(&S, 36));
        for (int i = 0; i < 9; i++) gc_achv_run_won(&S, true, 0);
        eval(NULL);
        assert(gc_achv_unlocked(&S, 36));
    }
    /* D'Arby the Gambler: one achievement, for beating him at either wager (lorenzo 2026-11-18); nothing else gives it */
    {
        fresh();
        eval(NULL);
        assert(!gc_achv_unlocked(&S, GC_ACHV_DARBY) && !gc_achv_unlocked(&S, GC_ACHV_DARBY));
        gc_achv_run_won(&S, true, 0);
        eval(NULL);
        assert(!gc_achv_unlocked(&S, GC_ACHV_DARBY));
        const uint32_t before = vouchers();
        gc_achv_darby_won(&S);
        gc_achv_new nw;
        eval(&nw);
        assert(gc_achv_unlocked(&S, GC_ACHV_DARBY) && S.achv.darby_wins == 1 && vouchers() == before + 2);   /* two Regular vouchers, once */
        gc_achv_darby_won(&S);
        gc_achv_darby_won(NULL);
        eval(NULL);
        assert(vouchers() == before + 2 && S.achv.darby_wins == 2);
        /* his rematch won (lorenzo 2026-11-18): "Debt Paid in Full", the Flawless JoJo reward (two Regular vouchers), once; the hand's wins never give it */
        assert(!gc_achv_unlocked(&S, GC_ACHV_DARBY_REMATCH) && !strcmp(gc_achv_defs[GC_ACHV_DARBY_REMATCH].name, "Debt Paid in Full"));
        assert(gc_achv_defs[GC_ACHV_DARBY_REMATCH].reward == gc_achv_defs[GC_ACHV_FLAWLESS_FIRST].reward && gc_achv_defs[GC_ACHV_DARBY_REMATCH].arg == gc_achv_defs[GC_ACHV_FLAWLESS_FIRST].arg
               && gc_achv_defs[GC_ACHV_DARBY_REMATCH].n == gc_achv_defs[GC_ACHV_FLAWLESS_FIRST].n);
        const uint32_t before2 = vouchers();
        gc_achv_darby_rematch_won(&S);
        gc_achv_darby_rematch_won(NULL);
        eval(NULL);
        assert(gc_achv_unlocked(&S, GC_ACHV_DARBY_REMATCH) && S.achv.darby_rematch_wins == 1 && vouchers() == before2 + 2);
        gc_achv_darby_rematch_won(&S);
        eval(NULL);
        assert(vouchers() == before2 + 2);
    }
    /* Jobin's two (lorenzo 2026-11-23): his titles; the beetle duel won gives the normal JoJo reward (one Regular voucher), the team beaten after being caught the
     * Flawless one (two); each once, and the counters are separate */
    {
        fresh();
        assert(!strcmp(gc_achv_defs[GC_ACHV_JOBIN].name, "Not quite a lamborghini gallardo, but it will do") && !strcmp(gc_achv_defs[GC_ACHV_JOBIN_PUNISH].name, "cheated and got away with it"));
        assert(gc_achv_defs[GC_ACHV_JOBIN].reward == gc_achv_defs[gc_achv_jojo_id(0)].reward && gc_achv_defs[GC_ACHV_JOBIN].arg == gc_achv_defs[gc_achv_jojo_id(0)].arg
               && gc_achv_defs[GC_ACHV_JOBIN].n == gc_achv_defs[gc_achv_jojo_id(0)].n);
        assert(gc_achv_defs[GC_ACHV_JOBIN_PUNISH].reward == gc_achv_defs[GC_ACHV_FLAWLESS_FIRST].reward && gc_achv_defs[GC_ACHV_JOBIN_PUNISH].arg == gc_achv_defs[GC_ACHV_FLAWLESS_FIRST].arg
               && gc_achv_defs[GC_ACHV_JOBIN_PUNISH].n == gc_achv_defs[GC_ACHV_FLAWLESS_FIRST].n);
        const uint32_t before = vouchers();
        assert(!gc_achv_unlocked(&S, GC_ACHV_JOBIN) && !gc_achv_unlocked(&S, GC_ACHV_JOBIN_PUNISH));
        gc_achv_jobin_won(&S);
        gc_achv_jobin_won(NULL);
        eval(NULL);
        assert(gc_achv_unlocked(&S, GC_ACHV_JOBIN) && !gc_achv_unlocked(&S, GC_ACHV_JOBIN_PUNISH) && S.achv.jobin_wins == 1 && vouchers() == before + gc_achv_defs[GC_ACHV_JOBIN].n);
        gc_achv_jobin_won(&S);
        eval(NULL);
        assert(vouchers() == before + gc_achv_defs[GC_ACHV_JOBIN].n);
        const uint32_t before2 = vouchers();
        gc_achv_jobin_punish_won(&S);
        gc_achv_jobin_punish_won(NULL);
        eval(NULL);
        assert(gc_achv_unlocked(&S, GC_ACHV_JOBIN_PUNISH) && S.achv.jobin_punish_wins == 1 && vouchers() == before2 + gc_achv_defs[GC_ACHV_JOBIN_PUNISH].n);
        gc_achv_jobin_punish_won(&S);
        eval(NULL);
        assert(vouchers() == before2 + gc_achv_defs[GC_ACHV_JOBIN_PUNISH].n);
    }
    /* waves: only the best is kept, and a lower wave never lowers it */
    {
        static const struct { int id; uint32_t wave; } W[] = {{37, 50}, {38, 100}, {39, 150}, {40, 190}};
        fresh();
        for (unsigned k = 0; k < 4; k++) {
            gc_achv_wave_reached(&S, W[k].wave - 1);
            eval(NULL);
            assert(!gc_achv_unlocked(&S, W[k].id));
            gc_achv_wave_reached(&S, W[k].wave);
            eval(NULL);
            assert(gc_achv_unlocked(&S, W[k].id));
        }
        gc_achv_wave_reached(&S, 3);
        assert(S.achv.best_wave == 190);
        uint32_t h, n;
        assert(gc_achv_progress_of(&S, 40, &h, &n) && h == 190 && n == 190);
        fresh();
        gc_achv_wave_reached(&S, 120);
        assert(gc_achv_progress_of(&S, 39, &h, &n) && h == 120 && n == 150);
    }
    /* feats: each of its own, and a win without any feat gives none */
    {
        static const struct { int id; uint8_t feat; } F[] = {{41, GC_FEAT_SIX_UNEVOLVED}, {42, GC_FEAT_SOLO}, {43, GC_FEAT_NO_BALL}};
        for (unsigned k = 0; k < 3; k++) {
            fresh();
            gc_achv_run_won(&S, false, 0);
            eval(NULL);
            for (unsigned j = 0; j < 3; j++) assert(!gc_achv_unlocked(&S, F[j].id));
            gc_achv_run_won(&S, false, F[k].feat);
            eval(NULL);
            for (unsigned j = 0; j < 3; j++) assert(gc_achv_unlocked(&S, F[j].id) == (j == k));
        }
    }
    /* group B survives a save */
    {
        fresh();
        gc_achv_wave_reached(&S, 77);
        gc_achv_run_won(&S, true, GC_FEAT_SOLO | GC_FEAT_NO_BALL);
        static uint8_t buf[600000];
        size_t n = gc_system_save(&S, buf, sizeof buf);
        static gc_system loaded;
        const char *why = NULL;
        gc_system_init_defaults(&loaded);
        assert(n && gc_system_load(&loaded, buf, n, &why));
        assert(!memcmp(&loaded.achv, &S.achv, sizeof S.achv) && loaded.achv.best_wave == 77 && loaded.achv.custom_wins == 1);
    }

    /* group F, the football counters survive a save, and an old save (no counters) loads them at zero */
    {
        fresh();
        gc_achv_football_won(&S, 12, 2, 3, 4);
        static uint8_t buf[600000];
        size_t n = gc_system_save(&S, buf, sizeof buf);
        static gc_system loaded;
        const char *why = NULL;
        gc_system_init_defaults(&loaded);
        assert(n && gc_system_load(&loaded, buf, n, &why));
        assert(!memcmp(&loaded.achv, &S.achv, sizeof S.achv));
        assert(loaded.achv.fb_wins == 12 && loaded.achv.fb_titles == 2 && loaded.achv.fb_shutouts == 3 && loaded.achv.fb_blowouts == 4);
        eval(NULL);
        assert(gc_achv_unlocked(&S, 92) && gc_achv_unlocked(&S, 98) && gc_achv_unlocked(&S, 99) && !gc_achv_unlocked(&S, 93) && !gc_achv_unlocked(&S, 96));
    }

    /* group C, the collection: the Pokedex of the profile */
    {
        static int all[1100], n_all, leg[200], n_leg, myth[100], n_myth;
        for (int i = 0; i < pr_species_gen_count; i++) {
            const pr_species_gen *g = &pr_species_gens[i];
            if (g->id < 1 || g->id > 1025 || !g->catchable) continue;
            all[n_all++] = g->id;
            if (g->legendary || g->sub_legendary) leg[n_leg++] = g->id;
            if (g->mythical) myth[n_myth++] = g->id;
        }
        assert(n_all > 900 && n_leg > 10 && n_myth > 5);
        static const struct { int id; int n; } CAUGHT[] = {{44, 10}, {45, 50}, {46, 100}, {47, 250}, {48, 500}};
        for (unsigned k = 0; k < 5; k++) {
            fresh();
            /* the species that are not legendary, so the count is only the count */
            static gc_system base;
            gc_system_init_defaults(&base);
            int put = 0, i = 0;
            for (; i < n_all && put < CAUGHT[k].n - 1; i++)
                if (!base.dex[all[i]].caught_attr) { S.dex[all[i]].caught_attr = 1; put++; }
            eval(NULL);
            assert(!gc_achv_unlocked(&S, CAUGHT[k].id));
            while (base.dex[all[i]].caught_attr) i++;
            S.dex[all[i]].caught_attr = 1;
            eval(NULL);
            assert(gc_achv_unlocked(&S, CAUGHT[k].id));
            if (k < 4) assert(!gc_achv_unlocked(&S, CAUGHT[k + 1].id));
        }
        assert(S.voucher_counts[GC_VOUCHER_GOLDEN] >= 1);
        fresh();
        for (int i = 0; i < n_all - 1; i++) S.dex[all[i]].caught_attr = 1;
        eval(NULL);
        assert(!gc_achv_unlocked(&S, 49) && !gc_achv_unlocked(&S, 59));
        S.dex[all[n_all - 1]].caught_attr = 1;
        eval(NULL);
        assert(gc_achv_unlocked(&S, 49) && gc_achv_unlocked(&S, 58) && gc_achv_unlocked(&S, 59) && gc_achv_unlocked(&S, 56));
        /* a species that cannot be caught is not needed, and caught ones do not count for "every species" */
        fresh();
        int uncatchable = 0;
        for (int i = 0; i < pr_species_gen_count; i++)
            if (pr_species_gens[i].id >= 1 && pr_species_gens[i].id <= 1025 && !pr_species_gens[i].catchable) { uncatchable = pr_species_gens[i].id; break; }
        if (uncatchable) {
            for (int i = 0; i < n_all; i++) S.dex[all[i]].caught_attr = 1;
            eval(NULL);
            assert(gc_achv_unlocked(&S, 49));                  /* without the uncatchable one */
        }
        /* a legendary that cannot be caught is no part of "every legendary", and one caught does not stand in for a missing one */
        {
            int odd = 0;
            for (int i = 0; i < pr_species_gen_count && !odd; i++) {
                const pr_species_gen *g = &pr_species_gens[i];
                if (g->id >= 1 && g->id <= 1025 && !g->catchable && (g->legendary || g->sub_legendary)) odd = g->id;
            }
            fresh();
            for (int i = 0; i < n_leg - 1; i++) S.dex[leg[i]].caught_attr = 1;
            if (odd) S.dex[odd].caught_attr = 1;
            eval(NULL);
            assert(!gc_achv_unlocked(&S, 56));
        }
        /* shinies */
        fresh();
        S.dex[all[0]].caught_attr = 1 | 2;
        eval(NULL);
        assert(gc_achv_unlocked(&S, 50) && !gc_achv_unlocked(&S, 51) && !gc_achv_unlocked(&S, 53));
        for (int i = 1; i < 4; i++) S.dex[all[i]].caught_attr = 1 | 2;
        eval(NULL);
        assert(!gc_achv_unlocked(&S, 51));                            /* four shinies are not five */
        S.dex[all[4]].caught_attr = 1 | 2;
        eval(NULL);
        assert(gc_achv_unlocked(&S, 51) && !gc_achv_unlocked(&S, 52));
        for (int i = 5; i < 25; i++) S.dex[all[i]].caught_attr = 1 | 2;
        eval(NULL);
        assert(gc_achv_unlocked(&S, 52));
        assert(S.n_eggs == 1 && S.eggs[0].variant_tier == 2);   /* the guaranteed tier 3 shiny egg */
        fresh();
        S.dex[all[0]].caught_attr = 1 | 64;                        /* tier 3 variant but not shiny */
        eval(NULL);
        assert(!gc_achv_unlocked(&S, 53) && !gc_achv_unlocked(&S, 50));
        S.dex[all[0]].caught_attr = 1 | 2 | 64;
        eval(NULL);
        assert(gc_achv_unlocked(&S, 53));
        /* legendaries and mythicals */
        fresh();
        S.dex[leg[0]].caught_attr = 1;
        eval(NULL);
        assert(gc_achv_unlocked(&S, 54) && !gc_achv_unlocked(&S, 55) && !gc_achv_unlocked(&S, 57));
        for (int i = 1; i < 5; i++) S.dex[leg[i]].caught_attr = 1;
        eval(NULL);
        assert(gc_achv_unlocked(&S, 55) && !gc_achv_unlocked(&S, 56));
        for (int i = 0; i < n_leg - 1; i++) S.dex[leg[i]].caught_attr = 1;
        eval(NULL);
        assert(!gc_achv_unlocked(&S, 56));
        S.dex[leg[n_leg - 1]].caught_attr = 1;
        eval(NULL);
        assert(gc_achv_unlocked(&S, 56) && S.n_eggs >= 1);
        fresh();
        S.dex[myth[0]].caught_attr = 1;
        eval(NULL);
        assert(gc_achv_unlocked(&S, 57));
        /* one generation: Generation 1 (1..151) caught, nothing else */
        fresh();
        for (int i = 0; i < n_all && all[i] <= 151; i++) S.dex[all[i]].caught_attr = 1;
        eval(NULL);
        assert(gc_achv_unlocked(&S, 58) && !gc_achv_unlocked(&S, 59));
        uint32_t h, n;
        assert(gc_achv_progress_of(&S, 59, &h, &n) && h == 1 && n == 9);
        fresh();
        for (int i = 0; i < n_all && all[i] <= 151; i++) if (all[i] != 25) S.dex[all[i]].caught_attr = 1;
        eval(NULL);
        assert(!gc_achv_unlocked(&S, 58));                          /* Pikachu missing */
        fresh();
        S.dex[252].caught_attr = 0;                                 /* the first of the next generation is a starter a new profile has */
        for (int i = 0; i < n_all; i++) if (all[i] > 151 && all[i] <= 251) S.dex[all[i]].caught_attr = 1;
        eval(NULL);
        assert(gc_achv_unlocked(&S, 58));                           /* any one generation, not the first */
        /* nothing caught: nothing unlocked, and it does not change the profile */
        fresh();
        assert(eval(NULL) == 0);                                    /* the starting species are not catches */
    }

    /* group D, the eggs */
    {
        static const struct { int id; uint32_t n; } H[] = {{60, 1}, {61, 10}, {62, 50}, {63, 100}, {64, 500}};
        for (unsigned k = 0; k < 5; k++) {
            fresh();
            S.stats.total_hatches = H[k].n - 1;
            eval(NULL);
            assert(!gc_achv_unlocked(&S, H[k].id));
            S.stats.total_hatches = H[k].n;
            eval(NULL);
            assert(gc_achv_unlocked(&S, H[k].id));
            if (k < 4) assert(!gc_achv_unlocked(&S, H[k + 1].id));
        }
        static const struct { int id; int tier; bool shiny; } F[] = {{65, 2, false}, {66, 3, false}, {67, 0, true}};
        for (unsigned k = 0; k < 3; k++) {
            fresh();
            gc_achv_egg_hatched(&S, 1, false);                       /* a Rare plain egg is none of them */
            eval(NULL);
            for (unsigned j = 0; j < 3; j++) assert(!gc_achv_unlocked(&S, F[j].id));
            gc_achv_egg_hatched(&S, F[k].tier, F[k].shiny);
            eval(NULL);
            for (unsigned j = 0; j < 3; j++) assert(gc_achv_unlocked(&S, F[j].id) == (j == k));
        }
        fresh();
        gc_achv_egg_hatched(&S, 3, true);
        static uint8_t buf[600000];
        size_t n = gc_system_save(&S, buf, sizeof buf);
        static gc_system loaded;
        const char *why = NULL;
        gc_system_init_defaults(&loaded);
        assert(n && gc_system_load(&loaded, buf, n, &why) && loaded.achv.hatch_feats == (GC_HATCH_LEGENDARY | GC_HATCH_SHINY));
    }

    /* group E, the trainers: the profile's voucherUnlocks, which the first win against a Gym Leader, an Elite Four member or a Champion sets */
    {
        enum { GYMS = 74, ELITES = 42, CHAMPS = 16 };
        uint32_t h, n;
        /* every one of them has a voucher in the reward table, so the real victory sets the bit this group reads (trainer_victory_diff_test and
         * gym_e4_test check the flow itself) */
        for (int t = TRAINER_TYPE_BROCK; t <= TRAINER_TYPE_GRUSHA; t++) {
            pr_reward_trainer_victory tv;
            assert(pr_reward_trainer_victory_rewards(t, false, false, &tv) == 0 && tv.voucher_type >= 0 && tv.unlock_voucher);
        }
        for (int t = TRAINER_TYPE_LORELEI; t <= TRAINER_TYPE_DRAYTON; t++) {
            pr_reward_trainer_victory tv;
            assert(pr_reward_trainer_victory_rewards(t, false, false, &tv) == 0 && tv.voucher_type >= 0 && tv.unlock_voucher);
        }
        for (int t = TRAINER_TYPE_BLUE; t <= TRAINER_TYPE_KIERAN; t++) {
            pr_reward_trainer_victory tv;
            assert(pr_reward_trainer_victory_rewards(t, false, false, &tv) == 0 && tv.voucher_type >= 0 && tv.unlock_voucher);
        }
        assert(TRAINER_TYPE_GRUSHA - TRAINER_TYPE_BROCK + 1 == GYMS && TRAINER_TYPE_DRAYTON - TRAINER_TYPE_LORELEI + 1 == ELITES
               && TRAINER_TYPE_KIERAN - TRAINER_TYPE_BLUE + 1 == CHAMPS);

        /* a new profile has beaten nobody */
        fresh();
        assert(eval(NULL) == 0);
        for (int id = 68; id < GC_ACHV_JOJO2_FIRST; id++) assert(gc_achv_progress_of(&S, id, &h, &n) && h == 0 && n > 0);   /* trainers and football: every one has a count (the ids from 100 are "Defeat X") */

        /* each one alone gives its own first achievement, once, and no count above one */
        for (int t = TRAINER_TYPE_BROCK; t <= TRAINER_TYPE_GRUSHA; t++) {
            fresh();
            S.voucher_unlocks[t] = 1;
            gc_achv_new nn;
            assert(eval(&nn) == 1 && nn.n == 1 && nn.id[0] == 68 && S.voucher_counts[GC_VOUCHER_REGULAR] == 1);
            assert(eval(NULL) == 0);
        }
        for (int t = TRAINER_TYPE_LORELEI; t <= TRAINER_TYPE_DRAYTON; t++) {
            fresh();
            S.voucher_unlocks[t] = 1;
            gc_achv_new nn;
            assert(eval(&nn) == 1 && nn.id[0] == 73 && S.voucher_counts[GC_VOUCHER_REGULAR] == 2);
        }
        for (int t = TRAINER_TYPE_BLUE; t <= TRAINER_TYPE_KIERAN; t++) {
            fresh();
            S.voucher_unlocks[t] = 1;
            gc_achv_new nn;
            assert(eval(&nn) == 1 && nn.id[0] == 77 && S.voucher_counts[GC_VOUCHER_PLUS] == 1);
        }
        /* an evil team boss or a rival is none of them */
        fresh();
        S.voucher_unlocks[TRAINER_TYPE_MAXIE] = 1;
        S.voucher_unlocks[TRAINER_TYPE_RIVAL] = 1;
        S.voucher_unlocks[TRAINER_TYPE_ROCKET_BOSS_GIOVANNI_1] = 1;
        assert(eval(NULL) == 0);

        /* the counts; "every" is the whole table */
        static const struct { int first, last, total; int ids[5]; int need[5]; } SETS[3] = {
            {TRAINER_TYPE_BROCK, TRAINER_TYPE_GRUSHA, GYMS, {68, 69, 70, 71, 72}, {1, 8, 25, 50, GYMS}},
            {TRAINER_TYPE_LORELEI, TRAINER_TYPE_DRAYTON, ELITES, {73, 74, 75, 76, -1}, {1, 4, 16, ELITES, 0}},
            {TRAINER_TYPE_BLUE, TRAINER_TYPE_KIERAN, CHAMPS, {77, 78, 79, -1, -1}, {1, 5, CHAMPS, 0, 0}},
        };
        for (int k = 0; k < 3; k++) {
            fresh();
            int given = 0;
            for (int t = SETS[k].first; t <= SETS[k].last; t++) {
                S.voucher_unlocks[t] = 1;
                given++;
                eval(NULL);
                for (int j = 0; j < 5 && SETS[k].ids[j] >= 0; j++)
                    assert(gc_achv_unlocked(&S, SETS[k].ids[j]) == (given >= SETS[k].need[j]));
            }
            assert(given == SETS[k].total);
            for (int j = 0; j < 5 && SETS[k].ids[j] >= 0; j++) {
                assert(gc_achv_progress_of(&S, SETS[k].ids[j], &h, &n) && h == n && n == (uint32_t)SETS[k].need[j]);
            }
        }
        fresh();
        for (int t = TRAINER_TYPE_BROCK; t < TRAINER_TYPE_GRUSHA; t++) S.voucher_unlocks[t] = 1;   /* all but the last */
        eval(NULL);
        assert(gc_achv_unlocked(&S, 71) && !gc_achv_unlocked(&S, 72));
        assert(gc_achv_progress_of(&S, 72, &h, &n) && h == GYMS - 1 && n == GYMS);

        /* the regions: every member of one unlocks it and only it; one short does not */
        static const int WANT[GC_REGION_COUNT] = {13, 13, 14, 13, 18, 13, 6, 19, 13, 5};   /* gym leaders + Elite Four + one Champion slot */
        for (int r = 0; r < GC_REGION_COUNT; r++) {
            const gc_region *reg = gc_achv_region(r);
            assert(reg && reg->name[0]);
            int members[40], nm = 0;
            if (reg->gym_first)
                for (int t = reg->gym_first; t <= reg->gym_last; t++) members[nm++] = t;
            for (int i = 0; reg->elite[i]; i++) members[nm++] = reg->elite[i];
            members[nm++] = reg->champion[0];
            assert(nm == WANT[r]);
            assert(strstr(gc_achv_defs[GC_ACHV_REGION_FIRST + r].name, reg->name));   /* the achievement is this region's */
            assert(strstr(gc_achv_defs[GC_ACHV_REGION_FIRST + r].desc, reg->name));
            for (int skip = -1; skip < nm; skip++) {
                fresh();
                for (int i = 0; i < nm; i++) if (i != skip) S.voucher_unlocks[members[i]] = 1;
                eval(NULL);
                for (int q = 0; q < GC_REGION_COUNT; q++) {
                    const bool want = q == r && skip < 0;
                    /* Bruno stands in two Elite Fours, but a region also needs its Gym Leaders (and none shares them), so no other region follows */
                    assert(gc_achv_unlocked(&S, GC_ACHV_REGION_FIRST + q) == want);
                }
                assert(gc_achv_progress_of(&S, GC_ACHV_REGION_FIRST + r, &h, &n) && n == (uint32_t)nm && h == (uint32_t)(skip < 0 ? nm : nm - 1));
                assert(!gc_achv_unlocked(&S, GC_ACHV_GRAND_TOUR));
            }
        }
        /* a region's Champion is either of its pair: each one alone completes it, and with neither it is one short */
        for (int r = 0; r < GC_REGION_COUNT; r++) {
            const gc_region *reg = gc_achv_region(r);
            for (int c = -1; reg->champion[c < 0 ? 0 : c]; c++) {
                fresh();
                if (reg->gym_first) for (int t = reg->gym_first; t <= reg->gym_last; t++) S.voucher_unlocks[t] = 1;
                for (int i = 0; reg->elite[i]; i++) S.voucher_unlocks[reg->elite[i]] = 1;
                if (c >= 0) S.voucher_unlocks[reg->champion[c]] = 1;
                eval(NULL);
                assert(gc_achv_unlocked(&S, GC_ACHV_REGION_FIRST + r) == (c >= 0));
            }
        }
        /* Alola and Blueberry have no Gym Leaders: their Elite Four and Champions alone are the whole region */
        assert(gc_achv_region(GC_REGION_ALOLA)->gym_first == 0 && gc_achv_region(GC_REGION_BLUEBERRY)->gym_first == 0);
        assert(!gc_achv_region(-1) && !gc_achv_region(GC_REGION_COUNT));

        /* every region, then the Grand Tour (a tier of the egg reward); one region short is not */
        fresh();
        for (int t = TRAINER_TYPE_BROCK; t <= TRAINER_TYPE_GRUSHA; t++) S.voucher_unlocks[t] = 1;
        for (int t = TRAINER_TYPE_LORELEI; t <= TRAINER_TYPE_DRAYTON; t++) S.voucher_unlocks[t] = 1;
        for (int t = TRAINER_TYPE_BLUE; t <= TRAINER_TYPE_KIERAN; t++) S.voucher_unlocks[t] = 1;
        S.voucher_unlocks[TRAINER_TYPE_KIERAN] = 0;   /* the last Champion: Blueberry is one short */
        eval(NULL);
        assert(!gc_achv_unlocked(&S, GC_ACHV_GRAND_TOUR) && !gc_achv_unlocked(&S, 79) && !gc_achv_unlocked(&S, GC_ACHV_REGION_FIRST + GC_REGION_BLUEBERRY));
        assert(gc_achv_progress_of(&S, GC_ACHV_GRAND_TOUR, &h, &n) && h == GC_REGION_COUNT - 1 && n == GC_REGION_COUNT);
        S.voucher_unlocks[TRAINER_TYPE_KIERAN] = 1;
        gc_achv_new all;
        eval(&all);
        assert(gc_achv_unlocked(&S, GC_ACHV_GRAND_TOUR) && gc_achv_unlocked(&S, 72) && gc_achv_unlocked(&S, 76) && gc_achv_unlocked(&S, 79));
        assert(S.n_eggs >= 1);

        /* a profile that beat them before the achievements existed gets them all at the first evaluation, once */
        {
            gc_achv_new again;
            assert(eval(&again) == 0 && again.n == 0);
        }

        /* the group survives a save (it keeps no counter of its own: the voucher bits are the data) */
        {
            static uint8_t buf[4 << 20];
            size_t sz = gc_system_save(&S, buf, sizeof buf);
            static gc_system loaded;
            const char *why = NULL;
            gc_system_init_defaults(&loaded);
            assert(sz && gc_system_load(&loaded, buf, sz, &why));
            for (int id = 68; id < GC_ACHV_COUNT; id++) assert(gc_achv_unlocked(&loaded, id) == gc_achv_unlocked(&S, id));
            assert(!memcmp(loaded.voucher_unlocks, S.voucher_unlocks, sizeof S.voucher_unlocks));
        }

        /* the region tables against run-core: a run meets one region's Elite Four and Champion, so the five trainers of every seed belong to
         * one table; every Elite Four member and Champion is in some region; every Gym Leader is in exactly one */
        {
            static gc_run run;
            static const int waves[5] = {CLASSIC_FIXED_BOSS_WAVES_ELITE_FOUR_1, CLASSIC_FIXED_BOSS_WAVES_ELITE_FOUR_2, CLASSIC_FIXED_BOSS_WAVES_ELITE_FOUR_3,
                                         CLASSIC_FIXED_BOSS_WAVES_ELITE_FOUR_4, CLASSIC_FIXED_BOSS_WAVES_CHAMPION};
            static bool seen_region[GC_REGION_COUNT], seen_type[400];
            for (int seed = 0; seed < 400; seed++) {
                char text[32];
                snprintf(text, sizeof text, "region-%d", seed);
                memset(&run, 0, sizeof run);
                gc_run_new(&run, GAME_MODES_CLASSIC, text, 31337);
                int t[5];
                for (int w = 0; w < 5; w++) {
                    t[w] = pr_run_fixed_e4_trainer(&run.run, waves[w]);
                    assert(t[w] > 0);
                    seen_type[t[w]] = true;
                }
                int fit = 0;
                for (int r = 0; r < GC_REGION_COUNT; r++) {
                    const gc_region *reg = gc_achv_region(r);
                    bool all_in = true;
                    for (int w = 0; w < 5; w++) {
                        bool in = false;
                        if (w < 4) {
                            for (int i = 0; reg->elite[i]; i++) in |= reg->elite[i] == t[w];
                        } else {
                            for (int i = 0; reg->champion[i]; i++) in |= reg->champion[i] == t[w];
                        }
                        all_in &= in;
                    }
                    if (all_in) { fit++; seen_region[r] = true; }
                }
                assert(fit >= 1);   /* Bruno is in two Elite Fours, but a Champion, a third member or a fourth tells them apart */
            }
            for (int r = 0; r < GC_REGION_COUNT; r++) assert(seen_region[r]);
            for (int t = TRAINER_TYPE_LORELEI; t <= TRAINER_TYPE_DRAYTON; t++) {
                bool in = false;
                for (int r = 0; r < GC_REGION_COUNT; r++) for (int i = 0; gc_achv_region(r)->elite[i]; i++) in |= gc_achv_region(r)->elite[i] == t;
                assert(in && seen_type[t]);   /* in a table, and a run really meets it */
            }
            for (int t = TRAINER_TYPE_BLUE; t <= TRAINER_TYPE_KIERAN; t++) {
                bool in = false;
                for (int r = 0; r < GC_REGION_COUNT; r++) for (int i = 0; gc_achv_region(r)->champion[i]; i++) in |= gc_achv_region(r)->champion[i] == t;
                assert(in);
            }
            for (int t = TRAINER_TYPE_BROCK; t <= TRAINER_TYPE_GRUSHA; t++) {
                int in = 0;
                for (int r = 0; r < GC_REGION_COUNT; r++) {
                    const gc_region *reg = gc_achv_region(r);
                    in += reg->gym_first && t >= reg->gym_first && t <= reg->gym_last;
                }
                assert(in == 1);
            }
        }
    }

    /* every achievement has a name, a description and a reward text; progress lines never exceed their need */
    fresh();
    for (int i = 0; i < GC_ACHV_COUNT; i++) {
        assert(gc_achv_defs[i].name[0] && gc_achv_defs[i].desc[0] && gc_achv_reward_name(&gc_achv_defs[i])[0]);
        uint32_t h, n;
        if (gc_achv_progress_of(&S, i, &h, &n)) assert(h <= n && n > 0);
    }

    puts("achv: all passed");
    return 0;
}
