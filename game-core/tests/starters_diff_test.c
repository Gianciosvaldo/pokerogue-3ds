#define _POSIX_C_SOURCE 200809L
/* The starter rules of the Endless modes against upstream at 8555c08: replays tests/starters.producer.test.ts's trace (the real
 * SelectStarterPhase.initBattle in Classic, Endless and Spliced Endless, and getRunValueLimit()) through game-core:
 *  - gc_starter_budget(mode) is the starter select's cost limit in each mode;
 *  - the party gc_starters_build_party makes from the same starters on the same Phaser RNG state matches upstream's: id, species,
 *    level, nature, ability, gender, luck, the fusion half of a Spliced Endless starter (species, form, ability index, gender,
 *    shiny, variant, luck), the HP and the six stats, and the RNG state afterwards (the fusion draws from it, so a draw too
 *    many or too few shows).
 * (gc_run_set_starters' own cost rule is checked in tests/starters_host_test.c: it reads gc_starter_budget.)
 * Usage: starters_diff_test <trace> */
#include "../src/internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;
static long checks, failures, cases, members, fused_members, hidden_members;

#define FAIL(...) do { if (failures < 40) { fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } failures++; } while (0)
#define CHECK(c, ...) do { checks++; if (!(c)) FAIL(__VA_ARGS__); } while (0)

static char *tok(char **save)
{
    char *t = strtok_r(NULL, " \n", save);
    if (!t) { fprintf(stderr, "short line\n"); exit(2); }
    return t;
}
static double num(char **save) { return strtod(tok(save), NULL); }
static long inum(char **save) { return strtol(tok(save), NULL, 10); }
static pr_rng read_rng(char **save)
{
    pr_rng r = {0};
    r.c = num(save);
    r.s0 = num(save);
    r.s1 = num(save);
    r.s2 = num(save);
    return r;
}
static bool same_rng(const pr_rng *a, const pr_rng *b) { return a->c == b->c && a->s0 == b->s0 && a->s1 == b->s1 && a->s2 == b->s2; }

static void replay(int idx, int mode, int n, int charms, long limit, pr_rng before, pr_rng after, const gc_starter *team,
                   char exp[][256])
{
    char seed[48];
    snprintf(seed, sizeof seed, "starters-diff-%d", idx);
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, mode, seed, 7);
    run.sys = &sys;
    CHECK(gc_starter_budget(mode) == limit, "case %d: the starter limit in mode %d is %d, upstream %ld", idx, mode, gc_starter_budget(mode), limit);
    if (charms) gc_item_add(&run, "ABILITY_CHARM", -1, 0, charms);
    run.run.rng = before;
    gc_starters_build_party(&run, team, n);
    CHECK(run.n_party == n, "case %d: %d members, expected %d", idx, run.n_party, n);
    CHECK(same_rng(&run.run.rng, &after), "case %d (mode %d): the RNG after the party is c=%.17g s=(%.17g %.17g %.17g), upstream c=%.17g s=(%.17g %.17g %.17g)", idx,
          mode, run.run.rng.c, run.run.rng.s0, run.run.rng.s1, run.run.rng.s2, after.c, after.s0, after.s1, after.s2);
    for (int k = 0; k < n && k < run.n_party; k++) {
        const gc_mon *m = &run.party[run.party_order[k]];
        const pr_pokemon *p = &m->b;
        char line[256];
        snprintf(line, sizeof line, "%s", exp[k]);
        char *save = NULL;
        strtok_r(line, " \n", &save);   /* M */
        const long id = inum(&save), species = inum(&save), level = inum(&save), nature = inum(&save), ability = inum(&save);
        const long gender = inum(&save), luck = inum(&save), fs = inum(&save), ff = inum(&save), fa = inum(&save), fg = inum(&save);
        const long fshiny = inum(&save), fvariant = inum(&save), fluck = inum(&save), hp = inum(&save);
        members++;
        CHECK((uint32_t)id == p->id, "case %d member %d: id %u, upstream %ld", idx, k, p->id, id);
        CHECK(species == p->species, "case %d member %d: species %d, upstream %ld", idx, k, p->species, species);
        CHECK(level == p->level, "case %d member %d: level %d, upstream %ld", idx, k, p->level, level);
        CHECK(nature == p->nature, "case %d member %d: nature %d, upstream %ld", idx, k, p->nature, nature);
        CHECK(ability == m->ability_index, "case %d member %d: ability index %d, upstream %ld", idx, k, m->ability_index, ability);
        CHECK(gender == p->gender, "case %d member %d: gender %d, upstream %ld", idx, k, p->gender, gender);
        CHECK(luck == m->luck, "case %d member %d: luck %d, upstream %ld", idx, k, m->luck, luck);
        CHECK(fs == p->fusion_species, "case %d (mode %d) member %d: fusion species %d, upstream %ld", idx, mode, k, p->fusion_species, fs);
        if (fs) {
            fused_members++;
            if (fa == 2) hidden_members++;
            CHECK(ff == p->fusion_form, "case %d member %d: fusion form %d, upstream %ld", idx, k, p->fusion_form, ff);
            CHECK(fa == p->fusion_ability_index, "case %d member %d: fusion ability %d, upstream %ld", idx, k, p->fusion_ability_index, fa);
            CHECK(fg == p->fusion_gender, "case %d member %d: fusion gender %d, upstream %ld", idx, k, p->fusion_gender, fg);
            CHECK(fshiny == (m->fusion_shiny ? 1 : 0), "case %d member %d: fusion shiny %d, upstream %ld", idx, k, m->fusion_shiny, fshiny);
            CHECK(fvariant == m->fusion_variant, "case %d member %d: fusion variant %d, upstream %ld", idx, k, m->fusion_variant, fvariant);
            CHECK(fluck == m->fusion_luck, "case %d member %d: fusion luck %d, upstream %ld", idx, k, m->fusion_luck, fluck);
        }
        CHECK(hp == p->hp, "case %d (mode %d) member %d: hp %d, upstream %ld", idx, mode, k, p->hp, hp);
        for (int s = 0; s < 6; s++) {
            const long want = inum(&save);
            CHECK(want == p->stats[s], "case %d (mode %d) member %d (%d + %d): stat %d is %d, upstream %ld", idx, mode, k, p->species,
                  p->fusion_species, s, p->stats[s], want);
        }
    }
    cases++;
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "starters.txt", "r");
    if (!f) { perror("trace"); return 2; }
    char *line = NULL;
    size_t cap = 0;
    long n_cases = 0;
    long limit_of[3] = {-1, -1, -1};
    static gc_starter team[6];
    static char exp[6][256];
    int idx = -1, mode = 0, n = 0, charms = 0, got_s = 0, got_m = 0;
    long limit = 0;
    pr_rng before = {0}, after = {0};
    bool open_case = false;
    bool saw_end = false;
    while (getline(&line, &cap, f) > 0) {
        char *save = NULL;
        char *kind = strtok_r(line, " \n", &save);
        if (!kind) continue;
        if (!strcmp(kind, "V")) {
            inum(&save);
            inum(&save);
            n_cases = inum(&save);
        } else if (!strcmp(kind, "L")) {
            const int m = (int)inum(&save);
            const long v = inum(&save);
            CHECK(m >= 0 && m < 3, "limit for mode %d", m);
            if (m >= 0 && m < 3) limit_of[m] = v;
            CHECK(gc_starter_budget(m) == v, "mode %d: the starter limit is %d, upstream getRunValueLimit() %ld", m, gc_starter_budget(m), v);
        } else if (!strcmp(kind, "C")) {
            if (open_case) replay(idx, mode, n, charms, limit, before, after, team, exp);
            idx = (int)inum(&save);
            mode = (int)inum(&save);
            n = (int)inum(&save);
            charms = (int)inum(&save);
            limit = inum(&save);
            before = read_rng(&save);
            tok(&save);   /* | */
            after = read_rng(&save);
            memset(team, 0, sizeof team);
            got_s = got_m = 0;
            open_case = true;
        } else if (!strcmp(kind, "S")) {
            gc_starter *s = &team[got_s++];
            s->species = (uint16_t)inum(&save);
            s->shiny = inum(&save) != 0;
            s->variant = (uint8_t)inum(&save);
            s->female = inum(&save) != 0;
            s->ability_index = (uint8_t)inum(&save);
            s->nature = (uint8_t)inum(&save);
            for (int i = 0; i < 6; i++) s->ivs[i] = (uint8_t)inum(&save);
            s->tera_type = -1;
        } else if (!strcmp(kind, "M")) {
            char *rest = save;
            snprintf(exp[got_m], sizeof exp[got_m], "M %s", rest);
            got_m++;
        } else if (!strcmp(kind, "Z")) {
            saw_end = true;
        }
    }
    if (open_case) replay(idx, mode, n, charms, limit, before, after, team, exp);
    free(line);
    fclose(f);
    if (!saw_end || cases != n_cases || limit_of[0] < 0 || limit_of[1] < 0 || limit_of[2] < 0) {
        fprintf(stderr, "incomplete trace: %ld of %ld cases, end marker %d\n", cases, n_cases, saw_end);
        return 2;
    }
    printf("STARTERS cases=%ld members=%ld fused=%ld hidden=%ld checks=%ld\n", cases, members, fused_members, hidden_members, checks);
    printf("TOTAL mismatches=%ld\n", failures);
    return failures ? 1 : 0;
}
