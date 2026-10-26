/* The custom achievements (gc/achv.h): spec tests. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "gc/achv.h"
#include "gc/gacha.h"
#include "gc/system.h"
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
