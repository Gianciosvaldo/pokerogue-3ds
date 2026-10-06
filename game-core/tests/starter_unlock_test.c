#include "gc/run.h"
#include "gc/system.h"
#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK_WHY(x, why) do { if (!(x)) { fprintf(stderr, "CHECK failed at line %d: %s (why: %s)\n", __LINE__, #x, (why) ? (why) : "NULL"); exit(1); } } while (0)
#define CHECK(x) CHECK_WHY(x, NULL)

static void test_locked_species(void)
{
    printf("test_locked_species...\n");
    gc_run run;
    gc_system sys;
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, "unlocktest1", 1001);
    run.sys = &sys;

    /* Pikachu (25) is not a default starter, so caught_attr is 0 initially */
    CHECK(sys.dex[SPECIES_ID_PIKACHU].caught_attr == 0);

    gc_starter team[1];
    memset(&team[0], 0, sizeof team[0]);
    team[0].species = SPECIES_ID_PIKACHU;
    team[0].nature = NATURE_DOCILE;
    team[0].tera_type = -1;

    gc_run run_before = run;
    gc_system sys_before = sys;

    const char *why = NULL;
    bool ok = gc_run_set_starters(&run, team, 1, &why);
    CHECK(!ok);
    CHECK(why && !strcmp(why, "starterSelectUiHandler:lockedSpecies"));

    CHECK(memcmp(&run, &run_before, sizeof(gc_run)) == 0);
    CHECK(memcmp(&sys, &sys_before, sizeof(gc_system)) == 0);
    printf("ok\n");
}

static void test_locked_ability_second(void)
{
    printf("test_locked_ability_second...\n");
    gc_run run;
    gc_system sys;
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, "unlocktest2", 1002);
    run.sys = &sys;

    /* Bulbasaur (species 1) is a default starter with ability_attr = 1 (Ability 1 unlocked, Ability 2 locked) */
    CHECK(sys.starters[SPECIES_ID_BULBASAUR].ability_attr == 1);

    gc_starter team[1];
    memset(&team[0], 0, sizeof team[0]);
    team[0].species = SPECIES_ID_BULBASAUR;
    team[0].nature = NATURE_DOCILE;
    team[0].ability_index = 1; /* Ability 2 (locked) */
    team[0].tera_type = -1;

    gc_run run_before = run;
    gc_system sys_before = sys;

    const char *why = NULL;
    bool ok = gc_run_set_starters(&run, team, 1, &why);
    CHECK(!ok);
    CHECK(why && !strcmp(why, "starterSelectUiHandler:lockedAbility"));

    CHECK(memcmp(&run, &run_before, sizeof(gc_run)) == 0);
    CHECK(memcmp(&sys, &sys_before, sizeof(gc_system)) == 0);
    printf("ok\n");
}

static void test_locked_ability_hidden(void)
{
    printf("test_locked_ability_hidden...\n");
    gc_run run;
    gc_system sys;
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, "unlocktest3", 1003);
    run.sys = &sys;

    CHECK(sys.starters[SPECIES_ID_BULBASAUR].ability_attr == 1);

    gc_starter team[1];
    memset(&team[0], 0, sizeof team[0]);
    team[0].species = SPECIES_ID_BULBASAUR;
    team[0].nature = NATURE_DOCILE;
    team[0].ability_index = 2; /* Hidden Ability (locked) */
    team[0].tera_type = -1;

    gc_run run_before = run;
    gc_system sys_before = sys;

    const char *why = NULL;
    bool ok = gc_run_set_starters(&run, team, 1, &why);
    CHECK(!ok);
    CHECK(why && !strcmp(why, "starterSelectUiHandler:lockedAbility"));

    CHECK(memcmp(&run, &run_before, sizeof(gc_run)) == 0);
    CHECK(memcmp(&sys, &sys_before, sizeof(gc_system)) == 0);
    printf("ok\n");
}

static void test_locked_form(void)
{
    printf("test_locked_form...\n");
    gc_run run;
    gc_system sys;
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, "unlocktest4", 1004);
    run.sys = &sys;

    /* Unlock Meowth (species 52) base form only */
    sys.dex[SPECIES_ID_MEOWTH].caught_attr = 157ULL; /* bit 7 = form 0 unlocked, bit 8 = form 1 (Gigantamax) locked */
    sys.dex[SPECIES_ID_MEOWTH].nature_attr = (1U << (NATURE_HARDY + 1));
    sys.starters[SPECIES_ID_MEOWTH].ability_attr = 1;

    gc_starter team[1];
    memset(&team[0], 0, sizeof team[0]);
    team[0].species = SPECIES_ID_MEOWTH;
    team[0].nature = NATURE_HARDY;
    team[0].form = 1; /* Gigantamax form (locked) */
    team[0].tera_type = -1;

    gc_run run_before = run;
    gc_system sys_before = sys;

    const char *why = NULL;
    bool ok = gc_run_set_starters(&run, team, 1, &why);
    CHECK(!ok);
    CHECK(why && !strcmp(why, "starterSelectUiHandler:lockedForm"));

    CHECK(memcmp(&run, &run_before, sizeof(gc_run)) == 0);
    CHECK(memcmp(&sys, &sys_before, sizeof(gc_system)) == 0);
    printf("ok\n");
}

static void test_locked_passive(void)
{
    printf("test_locked_passive...\n");
    gc_run run;
    gc_system sys;
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, "unlocktest5", 1005);
    run.sys = &sys;

    CHECK(sys.starters[SPECIES_ID_BULBASAUR].passive_attr == 0);

    gc_starter team[1];
    memset(&team[0], 0, sizeof team[0]);
    team[0].species = SPECIES_ID_BULBASAUR;
    team[0].nature = NATURE_DOCILE;
    team[0].passive = true; /* Passive requested (locked) */
    team[0].tera_type = -1;

    gc_run run_before = run;
    gc_system sys_before = sys;

    const char *why = NULL;
    bool ok = gc_run_set_starters(&run, team, 1, &why);
    CHECK(!ok);
    CHECK(why && !strcmp(why, "starterSelectUiHandler:lockedPassive"));

    CHECK(memcmp(&run, &run_before, sizeof(gc_run)) == 0);
    CHECK(memcmp(&sys, &sys_before, sizeof(gc_system)) == 0);
    printf("ok\n");
}

static void test_locked_shiny(void)
{
    printf("test_locked_shiny...\n");
    gc_run run;
    gc_system sys;
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, "unlocktest_shiny", 1010);
    run.sys = &sys;

    /* Bulbasaur caught_attr = 157ULL (NON_SHINY bit 0 set, SHINY bit 1 locked) */
    CHECK((sys.dex[SPECIES_ID_BULBASAUR].caught_attr & (1ULL << 1)) == 0);

    gc_starter team[1];
    memset(&team[0], 0, sizeof team[0]);
    team[0].species = SPECIES_ID_BULBASAUR;
    team[0].nature = NATURE_DOCILE;
    team[0].shiny = true;
    team[0].tera_type = -1;

    gc_run run_before = run;
    gc_system sys_before = sys;

    const char *why = NULL;
    bool ok = gc_run_set_starters(&run, team, 1, &why);
    CHECK(!ok);
    CHECK(why && !strcmp(why, "starterSelectUiHandler:lockedShiny"));

    CHECK(memcmp(&run, &run_before, sizeof(gc_run)) == 0);
    CHECK(memcmp(&sys, &sys_before, sizeof(gc_system)) == 0);
    printf("ok\n");
}

static void test_locked_variant_1(void)
{
    printf("test_locked_variant_1...\n");
    gc_run run;
    gc_system sys;
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, "unlocktest_var1", 1011);
    run.sys = &sys;

    /* Enable Shiny bit 1 and DEFAULT_VARIANT bit 4, but leave VARIANT_2 bit 5 locked */
    sys.dex[SPECIES_ID_BULBASAUR].caught_attr = 157ULL | (1ULL << 1);

    gc_starter team[1];
    memset(&team[0], 0, sizeof team[0]);
    team[0].species = SPECIES_ID_BULBASAUR;
    team[0].nature = NATURE_DOCILE;
    team[0].shiny = true;
    team[0].variant = 1; /* Variant 2 requested (locked) */
    team[0].tera_type = -1;

    gc_run run_before = run;
    gc_system sys_before = sys;

    const char *why = NULL;
    bool ok = gc_run_set_starters(&run, team, 1, &why);
    CHECK(!ok);
    CHECK(why && !strcmp(why, "starterSelectUiHandler:lockedVariant"));

    CHECK(memcmp(&run, &run_before, sizeof(gc_run)) == 0);
    CHECK(memcmp(&sys, &sys_before, sizeof(gc_system)) == 0);
    printf("ok\n");
}

static void test_locked_variant_2(void)
{
    printf("test_locked_variant_2...\n");
    gc_run run;
    gc_system sys;
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, "unlocktest_var2", 1012);
    run.sys = &sys;

    /* Enable Shiny bit 1 and DEFAULT_VARIANT bit 4, but leave VARIANT_3 bit 6 locked */
    sys.dex[SPECIES_ID_BULBASAUR].caught_attr = 157ULL | (1ULL << 1);

    gc_starter team[1];
    memset(&team[0], 0, sizeof team[0]);
    team[0].species = SPECIES_ID_BULBASAUR;
    team[0].nature = NATURE_DOCILE;
    team[0].shiny = true;
    team[0].variant = 2; /* Variant 3 requested (locked) */
    team[0].tera_type = -1;

    gc_run run_before = run;
    gc_system sys_before = sys;

    const char *why = NULL;
    bool ok = gc_run_set_starters(&run, team, 1, &why);
    CHECK(!ok);
    CHECK(why && !strcmp(why, "starterSelectUiHandler:lockedVariant"));

    CHECK(memcmp(&run, &run_before, sizeof(gc_run)) == 0);
    CHECK(memcmp(&sys, &sys_before, sizeof(gc_system)) == 0);
    printf("ok\n");
}

static void test_locked_female(void)
{
    printf("test_locked_female...\n");
    gc_run run;
    gc_system sys;
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, "unlocktest_female", 1013);
    run.sys = &sys;

    /* Set caught_attr with MALE bit 2, but remove FEMALE bit 3 */
    sys.dex[SPECIES_ID_BULBASAUR].caught_attr = 1ULL | (1ULL << 2) | (1ULL << 4) | (1ULL << 7); /* MALE only */

    gc_starter team[1];
    memset(&team[0], 0, sizeof team[0]);
    team[0].species = SPECIES_ID_BULBASAUR;
    team[0].nature = NATURE_DOCILE;
    team[0].female = true; /* Female requested (locked) */
    team[0].tera_type = -1;

    gc_run run_before = run;
    gc_system sys_before = sys;

    const char *why = NULL;
    bool ok = gc_run_set_starters(&run, team, 1, &why);
    CHECK(!ok);
    CHECK(why && !strcmp(why, "starterSelectUiHandler:lockedGender"));

    CHECK(memcmp(&run, &run_before, sizeof(gc_run)) == 0);
    CHECK(memcmp(&sys, &sys_before, sizeof(gc_system)) == 0);
    printf("ok\n");
}

static void test_locked_nature(void)
{
    printf("test_locked_nature...\n");
    gc_run run;
    gc_system sys;
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, "unlocktest_nature", 1014);
    run.sys = &sys;

    /* Bulbasaur default nature in init_defaults is Docile (6) -> nature_attr = (1U << 7).
     * Adamant (3) is locked because (1U << 4) is not set in nature_attr. */
    CHECK((sys.dex[SPECIES_ID_BULBASAUR].nature_attr & (1U << (NATURE_ADAMANT + 1))) == 0);

    gc_starter team[1];
    memset(&team[0], 0, sizeof team[0]);
    team[0].species = SPECIES_ID_BULBASAUR;
    team[0].nature = NATURE_ADAMANT; /* Adamant requested (locked) */
    team[0].tera_type = -1;

    gc_run run_before = run;
    gc_system sys_before = sys;

    const char *why = NULL;
    bool ok = gc_run_set_starters(&run, team, 1, &why);
    CHECK(!ok);
    CHECK(why && !strcmp(why, "starterSelectUiHandler:lockedNature"));

    CHECK(memcmp(&run, &run_before, sizeof(gc_run)) == 0);
    CHECK(memcmp(&sys, &sys_before, sizeof(gc_system)) == 0);
    printf("ok\n");
}

static void test_legal_cost_reduction(void)
{
    printf("test_legal_cost_reduction...\n");
    gc_run run;
    gc_system sys;
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, "unlocktest6", 1006);
    run.sys = &sys;

    /* Base costs: Bulbasaur (3), Charmander (3), Squirtle (3), Caterpie (2). Total base = 11 > 10.
     * With Bulbasaur value_reduction = 1, Bulbasaur cost becomes 2. Total reduced cost = 2+3+3+2 = 10 <= 10. */
    sys.dex[SPECIES_ID_CATERPIE].caught_attr = 157ULL;
    sys.dex[SPECIES_ID_CATERPIE].nature_attr = (1U << (NATURE_HARDY + 1));
    sys.starters[SPECIES_ID_CATERPIE].ability_attr = 1;

    sys.starters[SPECIES_ID_BULBASAUR].value_reduction = 1;

    gc_starter team[4];
    memset(team, 0, sizeof team);
    team[0].species = SPECIES_ID_BULBASAUR;  /* cost 3 -> reduced to 2 */
    team[0].nature = NATURE_DOCILE;
    team[0].tera_type = -1;
    team[1].species = SPECIES_ID_CHARMANDER; /* cost 3 */
    team[1].nature = NATURE_QUIRKY;
    team[1].tera_type = -1;
    team[2].species = SPECIES_ID_SQUIRTLE;   /* cost 3 */
    team[2].nature = NATURE_HARDY;
    team[2].tera_type = -1;
    team[3].species = SPECIES_ID_CATERPIE;   /* cost 2 */
    team[3].nature = NATURE_HARDY;
    team[3].tera_type = -1;

    const char *why = NULL;
    bool ok = gc_run_set_starters(&run, team, 4, &why);
    CHECK_WHY(ok, why);
    printf("ok\n");
}

static void test_exact_budget(void)
{
    printf("test_exact_budget...\n");
    gc_run run;
    gc_system sys;
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, "unlocktest7", 1007);
    run.sys = &sys;

    /* 3 + 3 + 3 + 1 = 10. Bulbasaur (3), Charmander (3), Squirtle (3), Weedle (1). */
    sys.dex[SPECIES_ID_WEEDLE].caught_attr = 157ULL;
    sys.dex[SPECIES_ID_WEEDLE].nature_attr = (1U << (NATURE_HARDY + 1));
    sys.starters[SPECIES_ID_WEEDLE].ability_attr = 1;

    gc_starter team[4];
    memset(team, 0, sizeof team);
    team[0].species = SPECIES_ID_BULBASAUR;  /* 3 */
    team[0].nature = NATURE_DOCILE;
    team[0].tera_type = -1;
    team[1].species = SPECIES_ID_CHARMANDER; /* 3 */
    team[1].nature = NATURE_QUIRKY;
    team[1].tera_type = -1;
    team[2].species = SPECIES_ID_SQUIRTLE;   /* 3 */
    team[2].nature = NATURE_HARDY;
    team[2].tera_type = -1;
    team[3].species = SPECIES_ID_WEEDLE;     /* 1 */
    team[3].nature = NATURE_HARDY;
    team[3].tera_type = -1;

    const char *why = NULL;
    bool ok = gc_run_set_starters(&run, team, 4, &why);
    CHECK(ok);
    printf("ok\n");
}

static void test_over_budget_by_one(void)
{
    printf("test_over_budget_by_one...\n");
    gc_run run;
    gc_system sys;
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, "unlocktest8", 1008);
    run.sys = &sys;

    /* 3 + 3 + 3 + 2 = 11 > 10. Bulbasaur (3), Charmander (3), Squirtle (3), Caterpie (2). */
    sys.dex[SPECIES_ID_CATERPIE].caught_attr = 157ULL;
    sys.dex[SPECIES_ID_CATERPIE].nature_attr = (1U << (NATURE_HARDY + 1));
    sys.starters[SPECIES_ID_CATERPIE].ability_attr = 1;

    gc_starter team[4];
    memset(team, 0, sizeof team);
    team[0].species = SPECIES_ID_BULBASAUR;  /* 3 */
    team[0].nature = NATURE_DOCILE;
    team[0].tera_type = -1;
    team[1].species = SPECIES_ID_CHARMANDER; /* 3 */
    team[1].nature = NATURE_QUIRKY;
    team[1].tera_type = -1;
    team[2].species = SPECIES_ID_SQUIRTLE;   /* 3 */
    team[2].nature = NATURE_HARDY;
    team[2].tera_type = -1;
    team[3].species = SPECIES_ID_CATERPIE;   /* 2 */
    team[3].nature = NATURE_HARDY;
    team[3].tera_type = -1;

    gc_run run_before = run;
    gc_system sys_before = sys;

    const char *why = NULL;
    bool ok = gc_run_set_starters(&run, team, 4, &why);
    CHECK(!ok);
    CHECK(why && !strcmp(why, "starterSelectUiHandler:cost"));

    CHECK(memcmp(&run, &run_before, sizeof(gc_run)) == 0);
    CHECK(memcmp(&sys, &sys_before, sizeof(gc_system)) == 0);
    printf("ok\n");
}

static void test_duplicate_species(void)
{
    printf("test_duplicate_species...\n");
    gc_run run;
    gc_system sys;
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, "unlocktest9", 1009);
    run.sys = &sys;

    gc_starter team[2];
    memset(team, 0, sizeof team);
    team[0].species = SPECIES_ID_BULBASAUR;
    team[0].nature = NATURE_DOCILE;
    team[0].tera_type = -1;
    team[1].species = SPECIES_ID_BULBASAUR;
    team[1].nature = NATURE_DOCILE;
    team[1].tera_type = -1;

    gc_run run_before = run;
    gc_system sys_before = sys;

    const char *why = NULL;
    bool ok = gc_run_set_starters(&run, team, 2, &why);
    CHECK(!ok);
    CHECK(why && !strcmp(why, "the same species twice"));

    CHECK(memcmp(&run, &run_before, sizeof(gc_run)) == 0);
    CHECK(memcmp(&sys, &sys_before, sizeof(gc_system)) == 0);
    printf("ok\n");
}


/* ---- helpers for the cases below ---- */

/* A fresh classic run with `sys` attached, one starter offered. True when accepted. */
static bool offer(gc_system *sys, const gc_starter *st, const char **why)
{
    gc_run run;
    gc_run_new(&run, GAME_MODES_CLASSIC, "unlockoffer", 2001);
    run.sys = sys;
    return gc_run_set_starters(&run, st, 1, why);
}

/* The offer must be refused with `want`, and neither the run nor the system data may change. */
static void expect_refused(gc_system *sys, const gc_starter *st, const char *want)
{
    gc_run run;
    gc_run_new(&run, GAME_MODES_CLASSIC, "unlockrefuse", 2002);
    run.sys = sys;
    gc_run run_before = run;
    gc_system sys_before = *sys;
    const char *why = NULL;
    CHECK_WHY(!gc_run_set_starters(&run, st, 1, &why), why);
    CHECK_WHY(why && !strcmp(why, want), why);
    CHECK(memcmp(&run, &run_before, sizeof(gc_run)) == 0);
    CHECK(memcmp(sys, &sys_before, sizeof(gc_system)) == 0);
}

static gc_starter starter_of(int species)
{
    gc_starter st;
    memset(&st, 0, sizeof st);
    st.species = (uint16_t)species;
    st.nature = NATURE_DOCILE;
    st.tera_type = -1;
    return st;
}

/* variant is an index 0-2 (DEFAULT_VARIANT, VARIANT_2, VARIANT_3 = bits 4-6). Index 3 would test bit 7, which is
 * DEFAULT_FORM and set here, and 255 would shift by 259: the range has to be checked before the shift. */
static void test_variant_out_of_range(void)
{
    printf("test_variant_out_of_range...\n");
    gc_system sys;
    gc_system_init_defaults(&sys);
    /* shiny + all three variants + male + female + default form, no non-shiny: 2 | 4 | 8 | 16 | 32 | 64 | 128 */
    sys.dex[SPECIES_ID_BULBASAUR].caught_attr = 254ULL;

    gc_starter st = starter_of(SPECIES_ID_BULBASAUR);
    st.shiny = true;
    st.variant = 3;
    expect_refused(&sys, &st, "starterSelectUiHandler:lockedVariant");
    st.variant = 255;
    expect_refused(&sys, &st, "starterSelectUiHandler:lockedVariant");
    printf("ok\n");
}

/* Upstream only refuses an explicit non-shiny when NON_SHINY (bit 0) was never caught (:659-662). */
static void test_locked_nonshiny(void)
{
    printf("test_locked_nonshiny...\n");
    gc_system sys;
    gc_system_init_defaults(&sys);
    sys.dex[SPECIES_ID_BULBASAUR].caught_attr = 2ULL | 4ULL | 8ULL | 16ULL | 128ULL; /* shiny only, never non-shiny */

    gc_starter st = starter_of(SPECIES_ID_BULBASAUR);
    expect_refused(&sys, &st, "starterSelectUiHandler:lockedShiny");
    st.shiny = true;
    const char *why = NULL;
    CHECK_WHY(offer(&sys, &st, &why), why);
    printf("ok\n");
}

/* The same options, unlocked, are accepted: the refusals above come from the locks and not from the option itself. */
static void test_unlocked_options_accepted(void)
{
    printf("test_unlocked_options_accepted...\n");
    gc_system sys;
    gc_system_init_defaults(&sys);
    sys.dex[SPECIES_ID_BULBASAUR].caught_attr = 157ULL | 2ULL | 32ULL | 64ULL; /* + shiny, variants 2 and 3 */
    sys.dex[SPECIES_ID_BULBASAUR].nature_attr |= 1U << (NATURE_ADAMANT + 1);

    const char *why = NULL;
    for (int variant = 0; variant < 3; variant++) {
        gc_starter st = starter_of(SPECIES_ID_BULBASAUR);
        st.shiny = true;
        st.variant = (uint8_t)variant;
        CHECK_WHY(offer(&sys, &st, &why), why);
    }
    gc_starter fem = starter_of(SPECIES_ID_BULBASAUR);
    fem.female = true;
    CHECK_WHY(offer(&sys, &fem, &why), why);
    gc_starter nat = starter_of(SPECIES_ID_BULBASAUR);
    nat.nature = NATURE_ADAMANT;
    CHECK_WHY(offer(&sys, &nat, &why), why);
    printf("ok\n");
}

/* Genderless Magnemite: pokemon.ts:611-613 gives a caught genderless Pokemon neither MALE nor FEMALE, and the gender
 * defaults without a bit (game-data.ts:1923 female = malePercent === 0), so the starter must be accepted with no gender
 * bit in caught_attr. Insisting on the MALE bit would make every genderless species unselectable. */
static void test_genderless_starter(void)
{
    printf("test_genderless_starter...\n");
    gc_system sys;
    gc_system_init_defaults(&sys);

    gc_mon caught;
    memset(&caught, 0, sizeof caught);
    caught.b.gender = -1;
    uint64_t attr = gc_mon_dex_attr(&caught);
    CHECK(attr == (1ULL | 16ULL | 128ULL)); /* non-shiny, default variant, default form: no MALE, no FEMALE */

    CHECK(gc_starter_cost(SPECIES_ID_MAGNEMITE) >= 0);
    CHECK(gc_species(SPECIES_ID_MAGNEMITE)->male_permille < 0);
    sys.dex[SPECIES_ID_MAGNEMITE].caught_attr = attr;
    sys.dex[SPECIES_ID_MAGNEMITE].nature_attr = 1U << (NATURE_DOCILE + 1);
    sys.starters[SPECIES_ID_MAGNEMITE].ability_attr = 1;

    gc_starter st = starter_of(SPECIES_ID_MAGNEMITE);
    const char *why = NULL;
    CHECK_WHY(offer(&sys, &st, &why), why);
    st.female = true; /* a bit nobody can have */
    expect_refused(&sys, &st, "starterSelectUiHandler:lockedGender");
    printf("ok\n");
}

/* Happiny is female only (malePercent 0): female is its default, so it needs no bit, but a male starter is a
 * non-default preference and needs MALE in caught_attr. */
static void test_female_only_starter(void)
{
    printf("test_female_only_starter...\n");
    gc_system sys;
    gc_system_init_defaults(&sys);
    CHECK(gc_starter_cost(SPECIES_ID_HAPPINY) >= 0);
    CHECK(gc_species(SPECIES_ID_HAPPINY)->male_permille == 0);
    sys.dex[SPECIES_ID_HAPPINY].caught_attr = 1ULL | 8ULL | 16ULL | 128ULL; /* FEMALE only */
    sys.dex[SPECIES_ID_HAPPINY].nature_attr = 1U << (NATURE_DOCILE + 1);
    sys.starters[SPECIES_ID_HAPPINY].ability_attr = 1;

    gc_starter st = starter_of(SPECIES_ID_HAPPINY);
    st.female = true;
    const char *why = NULL;
    CHECK_WHY(offer(&sys, &st, &why), why);
    st.female = false;
    expect_refused(&sys, &st, "starterSelectUiHandler:lockedGender");
    printf("ok\n");
}

/* getSpeciesStarterValue's decrementValue: -1 above 1, halved at 1 and below. Cost 3: 3, 2, 1, 0.5, 0.25. */
static void test_value_reduction_steps(void)
{
    printf("test_value_reduction_steps...\n");
    gc_system sys;
    gc_system_init_defaults(&sys);
    const double want[5] = { 3.0, 2.0, 1.0, 0.5, 0.25 };
    for (int r = 0; r < 5; r++) {
        sys.starters[SPECIES_ID_BULBASAUR].value_reduction = (uint32_t)r;
        CHECK(gc_starter_value(&sys, SPECIES_ID_BULBASAUR) == want[r]);
    }
    printf("ok\n");
}

int main(void)
{
    test_locked_species();
    test_locked_ability_second();
    test_locked_ability_hidden();
    test_locked_form();
    test_locked_passive();
    test_locked_shiny();
    test_locked_variant_1();
    test_locked_variant_2();
    test_locked_female();
    test_locked_nature();
    test_legal_cost_reduction();
    test_exact_budget();
    test_over_budget_by_one();
    test_duplicate_species();
    test_variant_out_of_range();
    test_locked_nonshiny();
    test_unlocked_options_accepted();
    test_genderless_starter();
    test_female_only_starter();
    test_value_reduction_steps();
    printf("All starter unlock and cost enforcement tests passed successfully!\n");
    return 0;
}
