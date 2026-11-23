#define _POSIX_C_SOURCE 200809L
/* Challenge Mode's starter rules against upstream at 8555c08: replays tests/challenge-starters.producer.test.ts's trace (Fresh Start through the
 * real getRunValueLimit, getSpeciesStarterValue, checkStarterValidForChallenge, getStarterData and SelectStarterPhase.initBattle) through
 * game-core:
 *  - gc_starter_budget(GAME_MODES_CHALLENGE) is the cost limit;
 *  - gc_challenge_starter_cost is the starter's value (Fresh Start: the base cost);
 *  - gc_challenge_starter_valid is checkStarterValidForChallenge, with and without soft;
 *  - gc_challenge_starter_view is getStarterData's copy of the profile: caught attributes, natures, abilities, passive, value reduction, egg
 *    moves, IVs;
 *  - the party gc_starters_build_party makes from the same starters on the same Phaser RNG state matches upstream's after its
 *    STARTER_MODIFY: id, species, form, level, nature, ability index and ability, passive, gender, luck, tera type, HP, the six stats, the six
 *    IVs, the four moves, and the RNG state afterwards.
 * Usage: challenge_starters_diff_test <trace> */
#include "../src/internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;
static long checks, failures, cases, queries, members, fresh_cases, views_changed, soft_only;

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
static uint64_t hex(char **save) { return strtoull(tok(save), NULL, 16); }
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

static void replay_party(int idx, int fresh, int n, pr_rng before, pr_rng after, const gc_starter *team, char exp[][400])
{
    char seed[48];
    snprintf(seed, sizeof seed, "challenge-diff-%d", idx);
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CHALLENGE, seed, 7);
    run.sys = &sys;
    gc_challenge_set set;
    memset(&set, 0, sizeof set);
    set.value[CHALLENGES_FRESH_START] = (uint8_t)fresh;
    const char *why = NULL;
    CHECK(gc_run_set_challenges(&run, &set, &why), "case %d: gc_run_set_challenges refused Fresh Start %d: %s", idx, fresh, why ? why : "");
    run.run.rng = before;
    gc_starters_build_party(&run, team, n);
    CHECK(run.n_party == n, "case %d: %d members, expected %d", idx, run.n_party, n);
    CHECK(same_rng(&run.run.rng, &after), "case %d (Fresh Start %d): the RNG after the party is c=%.17g s=(%.17g %.17g %.17g), upstream c=%.17g s=(%.17g %.17g %.17g)",
          idx, fresh, run.run.rng.c, run.run.rng.s0, run.run.rng.s1, run.run.rng.s2, after.c, after.s0, after.s1, after.s2);
    for (int k = 0; k < n && k < run.n_party; k++) {
        const gc_mon *m = &run.party[run.party_order[k]];
        const pr_pokemon *p = &m->b;
        char line[400];
        snprintf(line, sizeof line, "%s", exp[k]);
        char *save = NULL;
        strtok_r(line, " \n", &save);   /* M */
        const long id = inum(&save), species = inum(&save), form = inum(&save), level = inum(&save), nature = inum(&save), ability_index = inum(&save);
        const long ability = inum(&save), passive = inum(&save), gender = inum(&save), luck = inum(&save), tera = inum(&save), hp = inum(&save);
        members++;
        CHECK((uint32_t)id == p->id, "case %d member %d: id %u, upstream %ld", idx, k, p->id, id);
        CHECK(species == p->species, "case %d member %d: species %d, upstream %ld", idx, k, p->species, species);
        CHECK(form == p->form, "case %d (Fresh Start %d) member %d (%ld): form %d, upstream %ld", idx, fresh, k, species, p->form, form);
        CHECK(level == p->level, "case %d member %d: level %d, upstream %ld", idx, k, p->level, level);
        CHECK(nature == p->nature, "case %d member %d: nature %d, upstream %ld", idx, k, p->nature, nature);
        CHECK(ability_index == m->ability_index, "case %d (Fresh Start %d) member %d (%ld): ability index %d, upstream %ld", idx, fresh, k, species, m->ability_index, ability_index);
        CHECK(ability == p->base_ability, "case %d (Fresh Start %d) member %d (%ld): ability %d, upstream %ld", idx, fresh, k, species, p->base_ability, ability);
        CHECK(passive == (p->passive_flag ? 1 : 0), "case %d (Fresh Start %d) member %d (%ld): passive %d, upstream %ld", idx, fresh, k, species, p->passive_flag, passive);
        CHECK(gender == p->gender, "case %d member %d: gender %d, upstream %ld", idx, k, p->gender, gender);
        CHECK(luck == m->luck, "case %d (Fresh Start %d) member %d (%ld): luck %d, upstream %ld", idx, fresh, k, species, m->luck, luck);
        CHECK(tera == p->tera_type, "case %d (Fresh Start %d) member %d (%ld form %d): tera type %d, upstream %ld", idx, fresh, k, species, p->form, p->tera_type, tera);
        CHECK(hp == p->hp, "case %d (Fresh Start %d) member %d (%ld): hp %d, upstream %ld", idx, fresh, k, species, p->hp, hp);
        for (int s = 0; s < 6; s++) {
            const long want = inum(&save);
            CHECK(want == p->stats[s], "case %d (Fresh Start %d) member %d (%ld): stat %d is %d, upstream %ld", idx, fresh, k, species, s, p->stats[s], want);
        }
        for (int s = 0; s < 6; s++) {
            const long want = inum(&save);
            CHECK(want == p->ivs[s], "case %d (Fresh Start %d) member %d (%ld): iv %d is %d, upstream %ld", idx, fresh, k, species, s, p->ivs[s], want);
        }
        for (int s = 0; s < 4; s++) {
            const long want = inum(&save);
            const long got = s < p->n_moves ? p->moves[s] : 0;
            CHECK(want == got, "case %d (Fresh Start %d) member %d (%ld): move %d is %ld, upstream %ld", idx, fresh, k, species, s, got, want);
        }
    }
    cases++;
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "challenge-starters.txt", "r");
    if (!f) { perror("trace"); return 2; }
    char *line = NULL;
    size_t cap = 0;
    long n_cases = 0;
    static gc_starter team[6];
    static char exp[6][400];
    gc_challenge_set set;
    memset(&set, 0, sizeof set);
    gc_system_init_defaults(&sys);
    int idx = -1, fresh = 0, n = 0, got_s = 0, got_m = 0;
    pr_rng before = {0}, after = {0};
    bool open_case = false, saw_end = false;
    int q_species = 0, q_form = 0;
    double q_cost = 0;
    int q_hard = 0, q_soft = 0;
    while (getline(&line, &cap, f) > 0) {
        char *save = NULL;
        char *kind = strtok_r(line, " \n", &save);
        if (!kind) continue;
        if (!strcmp(kind, "V")) {
            inum(&save);
            inum(&save);
            n_cases = inum(&save);
        } else if (!strcmp(kind, "C")) {
            if (open_case) replay_party(idx, fresh, n, before, after, team, exp);
            open_case = false;
            idx = (int)inum(&save);
            fresh = (int)inum(&save);
            const long budget = inum(&save);
            memset(&set, 0, sizeof set);
            set.value[CHALLENGES_FRESH_START] = (uint8_t)fresh;
            CHECK(gc_starter_budget(GAME_MODES_CHALLENGE) == budget, "case %d: the cost limit is %d, upstream getRunValueLimit() %ld", idx, gc_starter_budget(GAME_MODES_CHALLENGE), budget);
            if (fresh) fresh_cases++;
        } else if (!strcmp(kind, "D")) {
            const int sp = (int)inum(&save);
            gc_dex_entry *d = &sys.dex[sp];
            gc_starter_entry *s = &sys.starters[sp];
            d->caught_attr = hex(&save);
            d->nature_attr = (uint32_t)inum(&save);
            s->ability_attr = (uint32_t)inum(&save);
            s->passive_attr = (uint32_t)inum(&save);
            s->value_reduction = (uint32_t)inum(&save);
            s->egg_moves = (uint8_t)inum(&save);
            for (int i = 0; i < 6; i++) d->ivs[i] = (uint8_t)inum(&save);
        } else if (!strcmp(kind, "Q")) {
            q_species = (int)inum(&save);
            q_form = (int)inum(&save);
            inum(&save);
            inum(&save);
            inum(&save);   /* shiny, variant, female: no ported challenge looks at them */
            q_cost = num(&save);
            q_hard = (int)inum(&save);
            q_soft = (int)inum(&save);
            queries++;
            const double cost = gc_challenge_starter_cost(&sys, &set, q_species);
            CHECK(cost == q_cost, "case %d (Fresh Start %d): the cost of %d is %.4f, upstream %.4f", idx, fresh, q_species, cost, q_cost);
            const int hard = gc_challenge_starter_valid(&set, q_species, q_form, false);
            const int soft = gc_challenge_starter_valid(&set, q_species, q_form, true);
            CHECK(hard == q_hard, "case %d (Fresh Start %d): %d form %d valid is %d, upstream %d", idx, fresh, q_species, q_form, hard, q_hard);
            CHECK(soft == q_soft, "case %d (Fresh Start %d): %d form %d soft-valid is %d, upstream %d", idx, fresh, q_species, q_form, soft, q_soft);
            if (q_soft && !q_hard) soft_only++;
        } else if (!strcmp(kind, "W")) {
            const int sp = (int)inum(&save);
            gc_starter_view v;
            gc_challenge_starter_view(&sys, &set, sp, &v);
            const uint64_t caught = hex(&save);
            const long nature = inum(&save), ability = inum(&save), passive = inum(&save), reduction = inum(&save), egg = inum(&save);
            CHECK(caught == v.caught_attr, "case %d (Fresh Start %d): %d caught attributes %llx, upstream %llx", idx, fresh, sp, (unsigned long long)v.caught_attr, (unsigned long long)caught);
            CHECK(nature == (long)v.nature_attr, "case %d (Fresh Start %d): %d natures %u, upstream %ld", idx, fresh, sp, v.nature_attr, nature);
            CHECK(ability == (long)v.ability_attr, "case %d (Fresh Start %d): %d abilities %u, upstream %ld", idx, fresh, sp, v.ability_attr, ability);
            CHECK(passive == (long)v.passive_attr, "case %d (Fresh Start %d): %d passive %u, upstream %ld", idx, fresh, sp, v.passive_attr, passive);
            CHECK(reduction == (long)v.value_reduction, "case %d (Fresh Start %d): %d value reduction %u, upstream %ld", idx, fresh, sp, v.value_reduction, reduction);
            CHECK(egg == v.egg_moves, "case %d (Fresh Start %d): %d egg moves %d, upstream %ld", idx, fresh, sp, v.egg_moves, egg);
            for (int i = 0; i < 6; i++) {
                const long want = inum(&save);
                CHECK(want == v.ivs[i], "case %d (Fresh Start %d): %d IV %d is %d, upstream %ld", idx, fresh, sp, i, v.ivs[i], want);
            }
            if (fresh && (v.value_reduction != sys.starters[sp].value_reduction || v.egg_moves != sys.starters[sp].egg_moves)) views_changed++;
        } else if (!strcmp(kind, "B")) {
            n = (int)inum(&save);
            before = read_rng(&save);
            tok(&save);   /* | */
            after = read_rng(&save);
            memset(team, 0, sizeof team);
            got_s = got_m = 0;
            open_case = true;
        } else if (!strcmp(kind, "S")) {
            gc_starter *s = &team[got_s++];
            s->species = (uint16_t)inum(&save);
            s->form = (uint8_t)inum(&save);
            s->shiny = inum(&save) != 0;
            s->variant = (uint8_t)inum(&save);
            s->female = inum(&save) != 0;
            s->ability_index = (uint8_t)inum(&save);
            s->nature = (uint8_t)inum(&save);
            s->passive = inum(&save) != 0;
            s->tera_type = (int8_t)inum(&save);
            for (int i = 0; i < 6; i++) s->ivs[i] = (uint8_t)inum(&save);
            for (int i = 0; i < 4; i++) s->moves[i] = (uint16_t)inum(&save);
        } else if (!strcmp(kind, "M")) {
            snprintf(exp[got_m], sizeof exp[got_m], "M %s", save);
            got_m++;
        } else if (!strcmp(kind, "Z")) {
            saw_end = true;
        }
    }
    if (open_case) replay_party(idx, fresh, n, before, after, team, exp);
    free(line);
    fclose(f);
    if (!saw_end || cases != n_cases) {
        fprintf(stderr, "incomplete trace: %ld of %ld cases, end marker %d\n", cases, n_cases, saw_end);
        return 2;
    }
    printf("CHALLENGE-STARTERS cases=%ld fresh=%ld queries=%ld members=%ld softOnly=%ld viewsChanged=%ld checks=%ld\n", cases, fresh_cases, queries, members, soft_only,
           views_changed, checks);
    printf("TOTAL mismatches=%ld\n", failures);
    return failures ? 1 : 0;
}
