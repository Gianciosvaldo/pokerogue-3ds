/* The custom achievements (gc/achv.h): spec tests. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "gc/achv.h"
#include "gc/gacha.h"
#include "gc/system.h"

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
        assert(gc_achv_unlocked(&S, j));
        assert(S.voucher_counts[GC_VOUCHER_REGULAR] >= 1);
        const uint32_t before = vouchers();
        assert(eval(&n) == 0 && n.n == 0);            /* nothing twice */
        gc_achv_jojo_won(&S, j, 7, false);
        assert(eval(&n) == 0 && vouchers() == before); /* the same challenger again gives nothing new */
    }

    /* the sets */
    static const struct { int id; int members[5]; int n; } SETS[] = {
        {17, {GC_JOJO_KARS, GC_JOJO_WAMUU, GC_JOJO_ESIDISI}, 3},
        {18, {GC_JOJO_JOSEPH, GC_JOJO_KARS, GC_JOJO_WAMUU, GC_JOJO_ESIDISI, GC_JOJO_STROHEIM}, 5},
        {19, {GC_JOJO_JOTARO, GC_JOJO_DIO, GC_JOJO_POLNAREFF}, 3},
        {20, {GC_JOJO_KOICHI, GC_JOJO_KIRA}, 2},
        {21, {GC_JOJO_DIAVOLO, GC_JOJO_MISTA, GC_JOJO_RISOTTO}, 3},
        {22, {GC_JOJO_JOLYNE, GC_JOJO_PUCCI}, 2},
        {23, {GC_JOJO_VALENTINE, GC_JOJO_GYRO}, 2},
    };
    for (unsigned k = 0; k < sizeof SETS / sizeof SETS[0]; k++) {
        fresh();
        for (int i = 0; i < SETS[k].n; i++) {
            assert(!gc_achv_unlocked(&S, SETS[k].id));
            gc_achv_jojo_won(&S, SETS[k].members[i], 1, false);
            eval(NULL);
        }
        assert(gc_achv_unlocked(&S, SETS[k].id));
    }

    /* counts of different challengers: 5, 10 and all 17 (the last gives a tier-3 shiny egg) */
    fresh();
    for (int j = 0; j < GC_JOJO_COUNT; j++) {
        gc_achv_jojo_won(&S, j, 100 + j, false); /* a new run each time */
        eval(NULL);
        assert(gc_achv_unlocked(&S, 24) == (j + 1 >= 5));
        assert(gc_achv_unlocked(&S, 25) == (j + 1 >= 10));
        assert(gc_achv_unlocked(&S, 26) == (j + 1 >= GC_JOJO_COUNT));
        assert(!gc_achv_unlocked(&S, 27));
    }
    assert(S.n_eggs == 1 && S.eggs[0].is_shiny && S.eggs[0].variant_tier == 2 && S.eggs[0].species != 0 && S.eggs[0].tier == 2);

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
