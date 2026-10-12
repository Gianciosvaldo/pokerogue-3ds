/* Replays tests/mystery-actions.producer.test.ts (upstream 8555c08, the real encounters' own option code) through
 * gc_mystery_execute and the reward screen it opens: for each case the run RNG after the option (the option's draws
 * run on the seed offset, so it must not move), the money and items, the shop the encounter set up (the items
 * rolled, fillRemaining, rerollMultiplier, allowLuckUpgrades), the options that shop shows with their arguments,
 * tiers and upgrade counts, and the RNG after the screen. Usage: mystery_actions_diff_test <trace>; exit 0 only
 * with 0 mismatches. */
#include "../src/internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;
static FILE *f;

static long long integer(void) { long long v; if (fscanf(f, "%lld", &v) != 1) { fputs("malformed input\n", stderr); exit(2); } return v; }
static double real(void) { double v; if (fscanf(f, "%lf", &v) != 1) { fputs("malformed input\n", stderr); exit(2); } return v; }
static void word(char *out, size_t cap)
{
    char fmt[16];
    snprintf(fmt, sizeof fmt, "%%%zus", cap - 1);
    if (fscanf(f, fmt, out) != 1) { fputs("malformed input\n", stderr); exit(2); }
}
static void tag(char want)
{
    char c;
    if (fscanf(f, " %c", &c) != 1 || c != want) { fprintf(stderr, "wrong record: wanted %c, got %c\n", want, c); exit(2); }
}
static pr_rng rng_rec(char want)
{
    pr_rng r = {0};
    tag(want);
    r.c = real(); r.s0 = real(); r.s1 = real(); r.s2 = real();
    return r;
}
static bool same_rng(const pr_rng *a, const pr_rng *b) { return a->c == b->c && a->s0 == b->s0 && a->s1 == b->s1 && a->s2 == b->s2; }

int main(int argc, char **argv)
{
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_actions_diff_test <trace>\n", stderr); return 2; }
    tag('V');
    integer();
    const int total = (int)integer();
    (void)total;
    gc_system_init_defaults(&sys);

    /* the party of the encounter being replayed: the P record before its cases */
    static struct { int species, form, level, n_moves, moves[4]; } mons[6];
    int n_mons = 0;
    int cases = 0, mismatches = 0, shown = 0, by_option[4] = {0}, options_checked = 0, tm_options = 0;
    char c;
    while (fscanf(f, " %c", &c) == 1) {
        if (c == 'Z') { if (integer() != cases) { fputs("case count\n", stderr); return 2; } break; }
        if (c == 'P') {
            n_mons = (int)integer();
            if (n_mons < 1 || n_mons > 6) { fputs("party size\n", stderr); return 2; }
            for (int m = 0; m < n_mons; m++) {
                mons[m].species = (int)integer(); mons[m].form = (int)integer(); mons[m].level = (int)integer();
                mons[m].n_moves = (int)integer();
                if (mons[m].n_moves < 0 || mons[m].n_moves > 4) { fputs("moves\n", stderr); return 2; }
                for (int k = 0; k < mons[m].n_moves; k++) mons[m].moves[k] = (int)integer();
            }
            continue;
        }
        if (c != 'C' || !n_mons) { fprintf(stderr, "wrong record %c\n", c); return 2; }
        char run_seed[64], wave_seed[64];
        const int i = (int)integer();
        word(run_seed, sizeof run_seed); word(wave_seed, sizeof wave_seed);
        const int wave = (int)integer(), option = (int)integer();
        const double money = real();
        const pr_rng before = rng_rec('R'), after_option = rng_rec('Q');
        tag('F');
        const int n_funcs = (int)integer();
        char funcs[16][40];
        for (int k = 0; k < n_funcs; k++) word(funcs[k], sizeof funcs[k]);
        const int fill = (int)integer();
        char reroll[24];
        word(reroll, sizeof reroll);
        const int luck = (int)integer(), n_tiers = (int)integer(), n_guar_opts = (int)integer();
        tag('M');
        const double money_after = real();
        const int items_delta = (int)integer(), leaves = (int)integer();
        tag('O');
        const int count = (int)integer(), n_opts = (int)integer();
        struct { char key[40]; int arg, tier, upgrade; } want[16];
        for (int k = 0; k < n_opts; k++) {
            word(want[k].key, sizeof want[k].key);
            want[k].arg = (int)integer(); want[k].tier = (int)integer(); want[k].upgrade = (int)integer();
        }
        const pr_rng after_screen = rng_rec('S');
        tag('E');
        (void)count;

        /* the run as the encounter finds it */
        memset(&run, 0, sizeof run);
        gc_run_new(&run, GAME_MODES_CLASSIC, run_seed, 42);
        gc_starter team[1];
        memset(team, 0, sizeof team);
        team[0].species = SPECIES_ID_BULBASAUR;
        team[0].tera_type = -1;
        for (int k = 0; k < 6; k++) team[0].ivs[k] = 15;
        const char *why;
        if (!gc_run_set_starters(&run, team, 1, &why)) { fprintf(stderr, "starters: %s\n", why); return 2; }
        run.sys = &sys;
        for (int m = 0; m < n_mons; m++) {
            if (m) run.party[m] = run.party[0];
            gc_mon *mon = &run.party[m];
            mon->b.species = (uint16_t)mons[m].species;
            mon->b.form = (uint8_t)mons[m].form;
            mon->b.level = mons[m].level;
            mon->b.n_moves = (uint8_t)mons[m].n_moves;
            memset(mon->b.moves, 0, sizeof mon->b.moves);
            memset(mon->b.pp_used, 0, sizeof mon->b.pp_used);
            for (int k = 0; k < mons[m].n_moves; k++) mon->b.moves[k] = (uint16_t)mons[m].moves[k];
            run.party_order[m] = (int8_t)m;
        }
        run.n_party = n_mons;
        run.n_items = 0;
        memset(run.items, 0, sizeof run.items);
        run.wave = wave;
        run.biome = BIOME_ID_PLAINS;
        run.money = money;
        pr_seed_from_ascii(&run.run.wave_seed, wave_seed);
        run.run.rng = before;
        gc_rq_reset(&gc_int(&run)->rq);
        run.mystery_type = GC_ME_DEPARTMENT_STORE_SALE;
        memset(&run.screen, 0, sizeof run.screen);
        run.screen.kind = GC_SCREEN_MYSTERY;
        run.screen.mystery_type = GC_ME_DEPARTMENT_STORE_SALE;
        gc_mystery_setup_screen(&run);

        bool ok = false;
        const char *w2;
        gc_mystery_execute(&run, option, 0, 0, &w2, &ok);
        const gc_reward_state *rs = &gc_int(&run)->reward;
        int bad = 0;
#define CHECK(cond, ...) do { if (!(cond)) { bad++; if (shown < 12) { shown++; fprintf(stderr, "case %d (option %d wave %d): ", i, option, wave); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } } while (0)
        CHECK(ok, "option refused: %s", w2);
        CHECK(same_rng(&run.run.rng, &after_option), "the run RNG moved during the option");
        CHECK(run.money == money_after, "money %.0f, want %.0f", run.money, money_after);
        CHECK(run.n_items == items_delta, "%d items, want %d", run.n_items, items_delta);
        CHECK(leaves == 1, "upstream left %d times", leaves);
        const pr_reward_custom *cu = &rs->me_custom;
        CHECK(cu->n_funcs == n_funcs, "%d shop items, want %d", cu->n_funcs, n_funcs);
        for (int k = 0; k < n_funcs && k < cu->n_funcs; k++)
            CHECK(!strcmp(pr_item_keys[cu->funcs[k]], funcs[k]), "shop item %d is %s, want %s", k, pr_item_keys[cu->funcs[k]], funcs[k]);
        CHECK(cu->fill_remaining == (fill != 0), "fillRemaining %d, want %d", cu->fill_remaining, fill);
        CHECK(!strcmp(reroll, "nan") ? isnan(cu->reroll_multiplier) : cu->reroll_multiplier == atof(reroll), "rerollMultiplier");
        CHECK(cu->allow_luck_upgrades == (luck != 0), "allowLuckUpgrades %d, want %d", cu->allow_luck_upgrades, luck);
        CHECK(cu->n_tiers == n_tiers && cu->n_options == n_guar_opts, "guaranteed tiers/options %d/%d, want %d/%d", cu->n_tiers, cu->n_options, n_tiers, n_guar_opts);

        /* the shop it opens: the run goes on through the queue as gc_run_choose does */
        gc_rqueue *q = &gc_int(&run)->rq;
        q->waiting = false;
        memset(&run.screen, 0, sizeof run.screen);
        gc_run_until_screen(&run);
        CHECK(run.screen.kind == GC_SCREEN_REWARD, "screen %d, want the reward screen", run.screen.kind);
        CHECK(run.screen.n_options == n_opts, "%d options, want %d", run.screen.n_options, n_opts);
        for (int k = 0; k < n_opts && k < run.screen.n_options; k++) {
            const gc_offer *o = &run.screen.options[k];
            const bool tm = !strncmp(want[k].key, "TM_", 3);
            CHECK(!strcmp(o->key, want[k].key) && o->tier == want[k].tier && o->upgrade_count == want[k].upgrade && o->arg == want[k].arg,
                  "option %d is %s/%d tier %d up %d, want %s/%d tier %d up %d", k, o->key, o->arg, o->tier, o->upgrade_count,
                  want[k].key, want[k].arg, want[k].tier, want[k].upgrade);
            options_checked++; tm_options += tm;
        }
        CHECK(same_rng(&run.run.rng, &after_screen), "the run RNG after the shop differs");
        CHECK(gc_rq_has(&run, RP_NEW_BATTLE), "the next wave is not queued");
        if (bad) mismatches++;
        cases++; by_option[option]++;
        (void)total;
    }
    printf("Mystery encounter Department Store Sale: %d cases (options %d/%d/%d/%d; %d shop options compared, %d TMs), TOTAL mismatches %d\n",
           cases, by_option[0], by_option[1], by_option[2], by_option[3], options_checked, tm_options, mismatches);
    return mismatches ? 1 : 0;
}
