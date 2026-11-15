/* Kyurem's and Calyrex's form items in Classic and Custom reward screens (lorenzo 2026-11-15, "found no Dark Stone in 2-3 runs"): the real
 * reward context of a real run, the real roll. Upstream offers each item (rare form change items, the Rogue tier) only for a member of that
 * species in its base form whose dex condition holds (Zekrom, Reshiram, Glastrier or Spectrier caught), only while it does not hold the item.
 * The rarity itself (about one in two hundred screens, a little over one a full run) is upstream's; this proves the wiring end to end. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run r;
static gc_system sys;
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "kyurem items line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

/* a run whose only party member is `species` (level 100), with the dex as the caller set it */
static void run_with(int species)
{
    gc_run_new(&r, GAME_MODES_CLASSIC, "kyurem-items", 4242);
    gc_starter st[1];
    memset(st, 0, sizeof st);
    st[0].species = SPECIES_ID_BULBASAUR;
    st[0].tera_type = -1;
    memset(st[0].ivs, 15, 6);
    const char *why;
    CHECK(gc_run_set_starters(&r, st, 1, &why));
    r.sys = &sys;
    gc_event e;
    while (gc_run_next_event(&r, &e)) {}
    st[0].species = (uint16_t)species;
    gc_mon_new_player(&r, &r.party[0], &st[0], 100);
    r.bt.party[0][0] = &r.party[0].b;
    r.bt.field[0] = &r.party[0].b;
    CHECK(r.n_party == 1 && r.party[0].b.species == species && r.party[0].b.form == 0);
}

/* how many of 66 waves x 60 seeds of reward screens at the best luck offer each of the four items */
static void rolls(int count[4])
{
    static const int items[4] = { FORM_CHANGE_ITEM_DARK_STONE, FORM_CHANGE_ITEM_LIGHT_STONE, FORM_CHANGE_ITEM_ICY_REINS_OF_UNITY,
                                  FORM_CHANGE_ITEM_SHADOW_REINS_OF_UNITY };
    static pr_reward_thresholds th;
    static pr_reward_option out[PR_REWARD_MAX_OPTIONS];
    memset(count, 0, 4 * sizeof *count);
    for (int wave = 1; wave < 200; wave += 3)
        for (int t = 0; t < 60; t++) {
            char seed[24];
            snprintf(seed, sizeof seed, "K%dW%d", t, wave);
            pr_seed_from_ascii(&r.run.seed, seed);
            r.wave = wave;
            pr_reward_ctx *ctx = gc_reward_ctx(&r);
            ctx->luck = 14;
            pr_reward_select sel = {0};
            memset(&th, 0, sizeof th);
            const int n = pr_reward_select_start(ctx, &sel, &th, out);
            CHECK(n >= 3);
            for (int i = 0; i < n; i++)
                if (out[i].type.item == PR_ITEM_RARE_FORM_CHANGE_ITEM)
                    for (int k = 0; k < 4; k++)
                        if (out[i].type.arg == items[k]) count[k]++;
        }
}

int main(void)
{
    int c[4];
    gc_system_init_defaults(&sys);
    run_with(SPECIES_ID_KYUREM);
    rolls(c);
    CHECK(!c[0] && !c[1] && !c[2] && !c[3]);                                /* nothing caught: the condition fails, nothing is offered */

    sys.dex[SPECIES_ID_ZEKROM].caught_attr = 1;
    run_with(SPECIES_ID_KYUREM);
    rolls(c);
    fprintf(stderr, "Zekrom caught: Dark %d, Light %d\n", c[0], c[1]);
    CHECK(c[0] >= 5 && !c[1] && !c[2] && !c[3]);                             /* the Dark Stone comes up, and nothing else of this family */

    sys.dex[SPECIES_ID_RESHIRAM].caught_attr = 1;
    run_with(SPECIES_ID_KYUREM);
    rolls(c);
    fprintf(stderr, "Zekrom and Reshiram caught: Dark %d, Light %d\n", c[0], c[1]);
    CHECK(c[0] >= 5 && c[1] >= 5 && !c[2] && !c[3]);

    /* a Kyurem that holds the Dark Stone is not offered it again, but still the Light Stone */
    gc_item_add(&r, "RARE_FORM_CHANGE_ITEM", FORM_CHANGE_ITEM_DARK_STONE, r.party[0].b.id, 1);
    rolls(c);
    CHECK(!c[0] && c[1] >= 5);

    /* a Kyurem already in its Black form has no stone to take: the pin offers nothing for it */
    run_with(SPECIES_ID_KYUREM);
    r.party[0].b.form = 1;
    rolls(c);
    CHECK(!c[0] && !c[1]);

    /* Calyrex */
    sys.dex[SPECIES_ID_GLASTRIER].caught_attr = 1;
    run_with(SPECIES_ID_CALYREX);
    rolls(c);
    fprintf(stderr, "Glastrier caught: Icy %d, Shadow %d\n", c[2], c[3]);
    CHECK(!c[0] && !c[1] && c[2] >= 5 && !c[3]);
    sys.dex[SPECIES_ID_SPECTRIER].caught_attr = 1;
    run_with(SPECIES_ID_CALYREX);
    rolls(c);
    CHECK(!c[0] && !c[1] && c[2] >= 5 && c[3] >= 5);

    /* another species never gets them */
    run_with(SPECIES_ID_ZEKROM);
    rolls(c);
    CHECK(!c[0] && !c[1] && !c[2] && !c[3]);
    printf("kyurem items: %u checks\n", checks);
    return 0;
}
