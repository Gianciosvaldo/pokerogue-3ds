/* The Endless modes' starters (src/gc_run.c, src/gc_mon.c) on the host, no upstream needed (tools/startersdiff.sh is the differential):
 *  - the starter select's cost limit is 15 in Endless and Spliced Endless and 10 in the other modes (getRunValueLimit);
 *  - gc_run_set_starters applies it: a party costing 11..15 is refused in Classic and accepted in both Endless modes, one over 15 is refused
 *    everywhere;
 *  - a Spliced Endless party is fused as it is made, with a fusion species the starter filter allows (an unevolved species that evolves, not a
 *    sub-legendary / legendary / mythical / trainer-forbidden one, not the starter itself, not Ditto), and the stats are the fused ones;
 *    Classic and Endless starters are not fused;
 *  - the same seed gives the same party, and the fused party saves and loads back as it was. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run, back;
static gc_system sys;
static unsigned char save[GC_SAVE_MAX];
static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "starters FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

static void make_team(gc_starter *t, const uint16_t *species, int n)
{
    memset(t, 0, n * sizeof *t);
    for (int i = 0; i < n; i++) {
        t[i].species = species[i];
        t[i].tera_type = -1;
        for (int d = 0; d < gc_default_starter_count; d++)   /* the one nature a new save unlocks for it */
            if (gc_default_starters[d] == species[i]) t[i].nature = (uint8_t)gc_default_starter_natures[d];
        for (int k = 0; k < 6; k++) t[i].ivs[k] = (uint8_t)(10 + k);
    }
}

/* species the new save lets the player pick, highest cost first, filled up to `target` points (at most 6 of them) */
static int fill_team(uint16_t *out, int target)
{
    int n = 0, cost = 0;
    for (int c = 9; c >= 1; c--)
        for (int i = 0; i < 27 && n < 6; i++) {
            const int sp = gc_default_starters[i];
            if (gc_starter_cost(sp) != c || cost + c > target) continue;
            bool dup = false;
            for (int k = 0; k < n; k++) dup |= out[k] == sp;
            if (dup) continue;
            out[n++] = (uint16_t)sp;
            cost += c;
        }
    return cost == target ? n : -1;
}

static void start(int mode, const char *seed)
{
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, mode, seed, 7);
    run.sys = &sys;
}

static void drain(gc_run *r)
{
    gc_event e;
    while (gc_run_next_event(r, &e)) {}
}

int main(void)
{
    /* the limit */
    CHECK(gc_starter_budget(GAME_MODES_CLASSIC) == 10);
    CHECK(gc_starter_budget(GAME_MODES_ENDLESS) == 15);
    CHECK(gc_starter_budget(GAME_MODES_SPLICED_ENDLESS) == 15);
    CHECK(gc_starter_budget(GAME_MODES_DAILY) == 10);
    CHECK(gc_starter_budget(GAME_MODES_CHALLENGE) == 10);

    /* a party at the Endless limit (15 points), and one in between (11 or 12) and one over */
    uint16_t at15[6], mid[6], over[6];
    int n15 = -1, nmid = -1, nover = -1, mid_cost = 0;
    n15 = fill_team(at15, 15);
    for (int c = 12; c >= 11 && nmid < 0; c--) { nmid = fill_team(mid, c); mid_cost = c; }
    CHECK(n15 > 0 && nmid > 0);
    (void)mid_cost;
    /* over 15: the 15-point party and any default starter that does not repeat one of its members, if it has room for another */
    memcpy(over, at15, sizeof at15);
    nover = n15;
    for (int i = 0; i < 27 && nover == n15 && nover < 6; i++) {
        bool dup = false;
        for (int k = 0; k < nover; k++) dup |= over[k] == gc_default_starters[i];
        gc_starter probe;
        make_team(&probe, &gc_default_starters[i], 1);
        double probe_cost = 0;
        gc_system_init_defaults(&sys);
        if (!dup && gc_starter_is_available(&sys, &probe, &probe_cost, NULL)) over[nover++] = gc_default_starters[i];   /* one more member: at least 16 points */
    }
    CHECK(nover == n15 + 1 || n15 == 6);
    gc_starter team[6];
    const char *why = NULL;
    static const int modes[3] = {GAME_MODES_CLASSIC, GAME_MODES_ENDLESS, GAME_MODES_SPLICED_ENDLESS};
    int accepted = 0, refused = 0;
    for (int m = 0; m < 3; m++) {
        const bool endless = modes[m] != GAME_MODES_CLASSIC;
        start(modes[m], "starter-limit");
        make_team(team, mid, nmid);
        CHECK(gc_run_set_starters(&run, team, nmid, &why) == endless);
        endless ? accepted++ : refused++;
        start(modes[m], "starter-limit");
        make_team(team, at15, n15);
        CHECK(gc_run_set_starters(&run, team, n15, &why) == endless);
        endless ? accepted++ : refused++;
        if (nover > n15) {
            start(modes[m], "starter-limit");
            make_team(team, over, nover);
            CHECK(!gc_run_set_starters(&run, team, nover, &why));
            CHECK(why && !strcmp(why, "starterSelectUiHandler:cost"));
            refused++;
        }
    }

    /* Spliced Endless fuses what it starts with, Endless and Classic do not */
    int fused_seen = 0;
    for (int m = 0; m < 3; m++) {
        const int mode = modes[m];
        const int n = mode == GAME_MODES_CLASSIC ? 0 : n15;
        if (!n) continue;
        for (int s = 0; s < 40; s++) {
            char seed[32];
            snprintf(seed, sizeof seed, "fuse-%d", s);
            start(mode, seed);
            make_team(team, at15, n15);
            CHECK(gc_run_set_starters(&run, team, n15, &why));
            CHECK(run.n_party == n15);
            for (int i = 0; i < run.n_party; i++) {
                const pr_pokemon *p = &run.party[i].b;
                if (mode == GAME_MODES_ENDLESS) { CHECK(p->fusion_species == 0); continue; }
                CHECK(run.bt.game_mode == GAME_MODES_SPLICED_ENDLESS);   /* battle-core's Spliced rules (fusion level ups) need it */
                CHECK(p->fusion_species != 0 && p->fusion_species != p->species && p->fusion_species != SPECIES_ID_DITTO);
                const pr_species_gen *f = gc_species_gen(p->fusion_species);
                CHECK(f && f->catchable && f->has_evolutions && !f->has_prevolution);
                CHECK(!f->sub_legendary && !f->legendary && !f->mythical && !f->trainer_forbidden);
                CHECK(p->fusion_ability_index >= 0 && p->fusion_ability_index <= 2);
                CHECK(p->fusion_gender_known && p->fusion_gender >= -1 && p->fusion_gender <= 1);
                /* the stats are the fused ones: a copy recalculated from the same state is the same */
                pr_pokemon copy = *p;
                pr_calculate_stats(&run.bt, &copy);
                for (int k = 0; k < 6; k++) CHECK(copy.stats[k] == p->stats[k]);
                fused_seen++;
            }
        }
    }
    CHECK(fused_seen == 40 * n15);

    /* the same seed, the same party; a party saved at the first command screen loads back with its fusions */
    static gc_mon first[6];
    int saved_ok = 0;
    for (int pass = 0; pass < 2; pass++) {
        start(GAME_MODES_SPLICED_ENDLESS, "same-seed");
        make_team(team, at15, n15);
        CHECK(gc_run_set_starters(&run, team, n15, &why));
        if (!pass) memcpy(first, run.party, sizeof first);
        else
            for (int i = 0; i < n15; i++) {
                CHECK(first[i].b.id == run.party[i].b.id);
                CHECK(first[i].b.fusion_species == run.party[i].b.fusion_species);
                CHECK(first[i].b.fusion_form == run.party[i].b.fusion_form);
                CHECK(first[i].b.fusion_ability_index == run.party[i].b.fusion_ability_index);
                CHECK(first[i].b.fusion_gender == run.party[i].b.fusion_gender);
                CHECK(first[i].b.stats[0] == run.party[i].b.stats[0]);
            }
    }
    drain(&run);
    if (gc_run_can_save(&run)) {
        const size_t len = gc_run_save(&run, save, sizeof save);
        CHECK(len > 0);
        memset(&back, 0, sizeof back);
        back.sys = &sys;
        CHECK(gc_run_load(&back, save, len, &why));
        CHECK(back.run.mode == GAME_MODES_SPLICED_ENDLESS && back.n_party == n15);
        for (int i = 0; i < n15; i++) {
            CHECK(back.party[i].b.fusion_species == run.party[i].b.fusion_species && back.party[i].b.fusion_species != 0);
            CHECK(back.party[i].b.fusion_form == run.party[i].b.fusion_form);
            CHECK(back.party[i].b.fusion_ability_index == run.party[i].b.fusion_ability_index);
            CHECK(back.party[i].b.fusion_gender == run.party[i].b.fusion_gender);
            CHECK(back.party[i].fusion_luck == run.party[i].fusion_luck && back.party[i].fusion_shiny == run.party[i].fusion_shiny);
            for (int k = 0; k < 6; k++) CHECK(back.party[i].b.stats[k] == run.party[i].b.stats[k]);
        }
        saved_ok = 1;
    }
    printf("starters host test: %d checks, %d starter lists accepted, %d refused, %d fused starters, save/load %s\n", checks, accepted, refused,
           fused_seen, saved_ok ? "round-tripped" : "not reachable");
    return 0;
}
