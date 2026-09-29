/* gcplay: game-core on a PC terminal.
 *
 *   gcplay [--seed S] [--starters BULBASAUR,CHARMANDER,...]     play a run by typing the choices
 *   gcplay --auto N [--first K] [--jobs J] [--verbose]           N seeded soak runs with random choices
 *
 * The soak runs pick random legal answers on every screen (their own RNG, seeded per run), each in its
 * own process so a crash or a hang is caught and reported with its seed, and sum up where the runs
 * ended, the stand-ins they used and battle-core's unported effects they hit. */
#define _POSIX_C_SOURCE 200809L
#include "gc/run.h"
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static const char *species_name(int id)
{
    for (int i = 0; i < PR_SPECIES_COUNT; i++) if (pr_species_table[i].id == id) return pr_species_table[i].name;
    return "?";
}
static const char *move_name(int id) { return id > 0 && id < PR_MOVE_COUNT ? pr_moves[id].name : "-"; }

/* ---------------------------------------------------------------- printing */
static void print_arg(const gc_run *run, const gc_arg *a)
{
    printf(" %s=", a->name);
    switch (a->type) {
    case GC_ARG_POKEMON: {
        int side = a->v / 8, idx = a->v % 8;
        const gc_mon *m = side ? &run->enemy[idx] : &run->party[idx];
        printf("%s%s", side ? (run->battle_type == 1 ? "Foe " : "Wild ") : "", species_name(m->b.species));
        break;
    }
    case GC_ARG_SPECIES: printf("%s", species_name(a->v)); break;
    case GC_ARG_MOVE: printf("%s", move_name(a->v)); break;
    case GC_ARG_TRAINER: printf("trainer#%d", run->trainer_type); break;
    case GC_ARG_STRING: printf("%s", a->s ? a->s : ""); break;
    default: printf("%d", a->v); break;
    }
}

static void print_event(const gc_run *run, const gc_event *e)
{
    switch (e->kind) {
    case GC_EV_MESSAGE:
        printf("  [%s]", e->key);
        for (int i = 0; i < e->n_args; i++) print_arg(run, &e->args[i]);
        printf("\n");
        break;
    case GC_EV_WAVE: printf("== wave %d  biome %d  %s\n", e->a, e->b, e->c == 1 ? "trainer" : "wild"); break;
    case GC_EV_BIOME: printf("== new biome %d\n", e->a); break;
    case GC_EV_HP: {
        const gc_mon *m = e->a ? &run->enemy[e->b] : &run->party[e->b];
        printf("  %s %s HP %d/%d\n", e->a ? "foe" : "you", species_name(m->b.species), e->c, m->b.stats[0]);
        break;
    }
    case GC_EV_EXP: printf("  exp +%d (party %d)\n", e->b, e->a); break;
    case GC_EV_MONEY: printf("  money %+d -> %d\n", e->a, e->b); break;
    case GC_EV_TURN: printf("-- turn %d\n", e->a); break;
    default: break;
    }
}

static void print_mon(const gc_mon *m)
{
    const pr_pokemon *b = &m->b;
    printf("%s L%d HP %d/%d", species_name(b->species), b->level, b->hp, b->stats[0]);
    if (b->status) printf(" st%d", b->status);
    printf(" [");
    for (int i = 0; i < b->n_moves; i++) printf("%s%s %d/%d", i ? ", " : "", move_name(b->moves[i]), b->pp_used[i], pr_moves[b->moves[i]].pp);
    printf("]");
}

static void print_screen(const gc_run *run)
{
    const gc_screen *s = gc_run_screen(run);
    switch (s->kind) {
    case GC_SCREEN_COMMAND: {
        const gc_mon *me = gc_field_mon(run, 0), *foe = gc_field_mon(run, 1);
        printf("\nwave %d  money %.0f\n  foe: ", run->wave, run->money);
        if (foe) print_mon(foe);
        printf("\n  you: ");
        if (me) print_mon(me);
        printf("\nCOMMAND: f <slot 0-3> | p <party> (switch)%s\n", s->struggle ? "  (no usable move: f 0 = Struggle)" : "");
        for (int i = 0; i < 4; i++) if (me && i < me->b.n_moves && !s->move_ok[i]) printf("  slot %d: %s\n", i, s->move_why[i]);
        break;
    }
    case GC_SCREEN_SWITCH:
        printf("SWITCH (send in a party member%s):\n", s->can_skip ? ", -1 for none" : "");
        for (int i = 0; i < run->n_party; i++) if (s->pick_ok[i]) { printf("  %d: ", i); print_mon(&run->party[i]); printf("\n"); }
        break;
    case GC_SCREEN_LEARN_MOVE:
        printf("LEARN MOVE: %s wants %s. Forget slot 0-3, or -1: ", species_name(run->party[s->party].b.species), move_name(s->move));
        print_mon(&run->party[s->party]);
        printf("\n");
        break;
    case GC_SCREEN_EVOLVE:
        printf("EVOLVE: %s into %s? y / n / N (n and pause)\n", species_name(run->party[s->party].b.species), species_name(s->species));
        break;
    case GC_SCREEN_REWARD:
        printf("REWARD (money %.0f): o <i> [party] [move] | s <i> [party] [move] | r (reroll %.0f) | x\n", run->money, s->reroll_cost);
        for (int i = 0; i < s->n_options; i++) printf("  o%d %s\n", i, s->options[i].key);
        for (int i = 0; i < s->n_shop; i++) printf("  s%d %s %.0f\n", i, s->shop[i].key, s->shop[i].cost);
        for (int i = 0; i < run->n_party; i++) { printf("  party %d: ", i); print_mon(&run->party[i]); printf("\n"); }
        break;
    case GC_SCREEN_BIOME:
        printf("BIOME:");
        for (int i = 0; i < s->n_biomes; i++) printf(" %d=%d", i, s->biomes[i]);
        printf("\n");
        break;
    case GC_SCREEN_GAME_OVER: printf("GAME OVER at wave %d, score %d\n", run->wave, run->score); break;
    case GC_SCREEN_CLEAR: printf("CLEAR! score %d\n", run->score); break;
    default: printf("(screen %d)\n", s->kind); break;
    }
}

static void drain(gc_run *run, bool show)
{
    gc_event e;
    while (gc_run_next_event(run, &e)) if (show) print_event(run, &e);
}

/* ---------------------------------------------------------------- the random chooser */
static uint64_t rs;
static uint32_t rnd(uint32_t n)
{
    rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17;
    return n ? (uint32_t)(rs % n) : 0;
}

static int alive_member(const gc_run *run)
{
    int c[6], n = 0;
    for (int i = 0; i < run->n_party; i++) if (run->party[i].b.hp > 0) c[n++] = i;
    return n ? c[rnd((uint32_t)n)] : 0;
}

/* one random legal answer; false when nothing was accepted */
static bool auto_choose(gc_run *run)
{
    const gc_screen *s = gc_run_screen(run);
    gc_choice c = {0};
    const char *why;
    for (int tries = 0; tries < 64; tries++) {
        memset(&c, 0, sizeof c);
        switch (s->kind) {
        case GC_SCREEN_COMMAND: {
            if (s->can_switch && rnd(100) < 10) {
                c.kind = GC_CH_POKEMON;
                c.a = (int16_t)rnd((uint32_t)run->n_party);
            } else {
                c.kind = GC_CH_FIGHT;
                int ok[4], n = 0;
                for (int i = 0; i < 4; i++) if (s->move_ok[i]) ok[n++] = i;
                c.a = (int16_t)(n ? ok[rnd((uint32_t)n)] : 0);
                c.b = -1;
            }
            break;
        }
        case GC_SCREEN_SWITCH: {
            int ok[6], n = 0;
            for (int i = 0; i < run->n_party; i++) if (s->pick_ok[i]) ok[n++] = i;
            c.kind = GC_CH_PICK;
            c.a = (int16_t)(n ? ok[rnd((uint32_t)n)] : -1);
            break;
        }
        case GC_SCREEN_LEARN_MOVE:
            c.kind = GC_CH_FORGET;
            c.a = (int16_t)((int)rnd(5) - 1);
            break;
        case GC_SCREEN_EVOLVE:
            c.kind = rnd(10) ? GC_CH_EVOLVE : GC_CH_CANCEL;
            c.a = (int16_t)(c.kind == GC_CH_CANCEL && rnd(2));
            break;
        case GC_SCREEN_REWARD:
            if (tries > 40) { c.kind = GC_CH_SKIP; break; }
            if (s->n_shop && rnd(100) < 5) {
                c.kind = GC_CH_SHOP;
                c.a = (int16_t)rnd((uint32_t)s->n_shop);
            } else if (s->reroll_cost >= 0 && s->reroll_cost < run->money / 4 && rnd(100) < 5) {
                c.kind = GC_CH_REROLL;
            } else if (s->n_options) {
                c.kind = GC_CH_REWARD;
                c.a = (int16_t)rnd((uint32_t)s->n_options);
            } else {
                c.kind = GC_CH_SKIP;
            }
            c.b = (int16_t)(tries < 20 ? alive_member(run) : (int)rnd((uint32_t)run->n_party));
            c.c = (int16_t)rnd(4);
            break;
        case GC_SCREEN_BIOME:
            c.kind = GC_CH_BIOME;
            c.a = (int16_t)rnd((uint32_t)s->n_biomes);
            break;
        default:
            return false;
        }
        if (gc_run_choose(run, &c, &why)) return true;
    }
    return false;
}

/* a random team within the budget, from every species with a starter cost */
static int random_team(gc_starter *out)
{
    int budget = gc_starter_budget(GAME_MODES_CLASSIC), n = 0;
    for (int tries = 0; tries < 200 && n < 6 && budget > 0; tries++) {
        const pr_species *sp = &pr_species_table[rnd(PR_SPECIES_COUNT)];
        const int cost = gc_starter_cost(sp->id);
        if (cost <= 0 || cost > budget) continue;
        bool dup = false;
        for (int k = 0; k < n; k++) dup |= out[k].species == sp->id;
        if (dup) continue;
        gc_starter *s = &out[n++];
        memset(s, 0, sizeof *s);
        s->species = sp->id;
        s->ability_index = (uint8_t)rnd(2);
        s->nature = (uint8_t)rnd(25);
        s->female = sp->male_permille >= 0 && (int)rnd(1000) >= sp->male_permille;
        s->tera_type = -1;
        for (int i = 0; i < 6; i++) s->ivs[i] = (uint8_t)rnd(32);
        budget -= cost;
    }
    return n;
}

/* ---------------------------------------------------------------- soak runs */
typedef struct {
    int status;                 /* 0 ended, 1 stuck, 2 crashed, 3 timed out */
    int wave, turns, score, clear;
    int waves_to_trainer;
    uint32_t stubs[16];
    uint32_t unported[PR_FEAT_COUNT_];
    char stuck_screen;
    char team[64];
} soak_result;

static void soak_one(int index, int verbose, soak_result *r)
{
    static gc_run run;
    char seed[32];
    snprintf(seed, sizeof seed, "soak%08d", index);
    rs = 0x9E3779B97F4A7C15ull ^ ((uint64_t)index * 0xD1B54A32D192ED03ull);
    rnd(1);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, (uint32_t)index);
    gc_starter team[6];
    int n = random_team(team);
    const char *why;
    r->team[0] = 0;
    for (int i = 0; i < n; i++) {
        size_t l = strlen(r->team);
        snprintf(r->team + l, sizeof r->team - l, "%s%d", i ? "," : "", team[i].species);
    }
    if (!gc_run_set_starters(&run, team, n, &why)) { r->status = 1; r->stuck_screen = 'S'; return; }
    for (int step = 0; step < 200000; step++) {
        gc_event e;
        while (gc_run_next_event(&run, &e)) {
            if (e.kind == GC_EV_TURN) r->turns++;
            if (verbose) print_event(&run, &e);
        }
        const gc_screen *s = gc_run_screen(&run);
        if (s->kind == GC_SCREEN_GAME_OVER || s->kind == GC_SCREEN_CLEAR) { r->clear = s->kind == GC_SCREEN_CLEAR; break; }
        if (verbose) print_screen(&run);
        if (!auto_choose(&run)) { r->status = 1; r->stuck_screen = (char)('0' + s->kind); break; }
    }
    r->wave = run.wave;
    r->score = run.score;
    memcpy(r->stubs, run.stubs, sizeof r->stubs);
    memcpy(r->unported, run.unported, sizeof r->unported);
}

static int cmp_desc(const void *a, const void *b)
{
    const uint32_t *x = a, *y = b;
    return (y[1] > x[1]) - (y[1] < x[1]);
}

static int soak(int n, int first, int jobs, int verbose, int timeout)
{
    static soak_result agg_r;
    static uint32_t hit_runs[PR_FEAT_COUNT_], hit_total[PR_FEAT_COUNT_];
    uint32_t stub_runs[16] = {0}, stub_total[16] = {0};
    int ended = 0, stuck = 0, crashed = 0, timed = 0, clears = 0;
    long turns = 0;
    int wave_hist[21] = {0};
    int running = 0, next = 0;
    struct { pid_t pid; int fd; int index; } slot[64];
    if (jobs > 64) jobs = 64;
    if (verbose) jobs = 1;
    while (next < n || running) {
        while (running < jobs && next < n) {
            int p[2];
            if (pipe(p)) { perror("pipe"); return 2; }
            pid_t pid = fork();
            if (pid == 0) {
                close(p[0]);
                alarm((unsigned)timeout);
                soak_result r;
                memset(&r, 0, sizeof r);
                soak_one(first + next, verbose, &r);
                const char *b = (const char *)&r;
                size_t left = sizeof r;
                while (left) { ssize_t w = write(p[1], b, left); if (w <= 0) break; b += w; left -= (size_t)w; }
                _exit(0);
            }
            close(p[1]);
            slot[running].pid = pid;
            slot[running].fd = p[0];
            slot[running].index = first + next;
            running++;
            next++;
        }
        int st;
        pid_t done = wait(&st);
        if (done < 0) { if (errno == EINTR) continue; break; }
        int k = 0;
        while (k < running && slot[k].pid != done) k++;
        if (k == running) continue;
        soak_result *r = &agg_r;
        memset(r, 0, sizeof *r);
        size_t got = 0;
        char *b = (char *)r;
        ssize_t rd;
        while (got < sizeof *r && (rd = read(slot[k].fd, b + got, sizeof *r - got)) > 0) got += (size_t)rd;
        close(slot[k].fd);
        const int index = slot[k].index;
        slot[k] = slot[--running];
        if (WIFSIGNALED(st) || got != sizeof *r) {
            if (WIFSIGNALED(st) && WTERMSIG(st) == SIGALRM) { timed++; printf("TIMEOUT seed soak%08d\n", index); }
            else { crashed++; printf("CRASH seed soak%08d (signal %d)\n", index, WIFSIGNALED(st) ? WTERMSIG(st) : 0); }
            continue;
        }
        if (r->status == 1) { stuck++; printf("STUCK seed soak%08d at wave %d (screen %c) team %s\n", index, r->wave, r->stuck_screen, r->team); }
        else ended++;
        clears += r->clear;
        turns += r->turns;
        wave_hist[r->wave / 10 > 20 ? 20 : r->wave / 10]++;
        for (int i = 0; i < 16; i++) { stub_total[i] += r->stubs[i]; stub_runs[i] += r->stubs[i] > 0; }
        for (int i = 0; i < PR_FEAT_COUNT_; i++) { hit_total[i] += r->unported[i]; hit_runs[i] += r->unported[i] > 0; }
    }
    printf("\n%d runs: %d ended (%d cleared), %d stuck, %d crashed, %d timed out; %ld turns (%.1f per run)\n",
           n, ended, clears, stuck, crashed, timed, turns, n ? (double)turns / n : 0.0);
    printf("final wave: ");
    for (int i = 0; i <= 20; i++) if (wave_hist[i]) printf(" %d-%d:%d", i * 10, i * 10 + 9, wave_hist[i]);
    printf("\n\nstand-ins (runs that used it, times):\n");
    for (int i = 0; i < GC_STUB_COUNT_; i++)
        if (stub_total[i]) printf("  %-50s %6u runs %9u\n", gc_stub_name[i], stub_runs[i], stub_total[i]);
    static uint32_t order[PR_FEAT_COUNT_][2];
    int m = 0;
    for (int i = 0; i < PR_FEAT_COUNT_; i++) if (hit_runs[i]) { order[m][0] = (uint32_t)i; order[m][1] = hit_runs[i]; m++; }
    qsort(order, (size_t)m, sizeof order[0], cmp_desc);
    printf("\nbattle-core unported, by runs that hit it (%d kinds):\n", m);
    for (int i = 0; i < m; i++)
        printf("  %-44s %6u runs %9u\n", pr_unported_name((uint16_t)order[i][0]), order[i][1], hit_total[order[i][0]]);
    return crashed || timed ? 1 : 0;
}

/* ---------------------------------------------------------------- interactive */
static int parse_species(const char *name)
{
    char *end;
    long v = strtol(name, &end, 10);
    if (*name && !*end) return (int)v;
    for (int i = 0; i < PR_SPECIES_COUNT; i++) if (!strcmp(pr_species_table[i].name, name)) return pr_species_table[i].id;
    return -1;
}

static int play(const char *seed, const char *starters)
{
    static gc_run run;
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, (uint32_t)getpid());
    gc_starter team[6];
    int n = 0;
    char buf[256];
    snprintf(buf, sizeof buf, "%s", starters ? starters : "BULBASAUR,CHARMANDER,SQUIRTLE");
    for (char *t = strtok(buf, ","); t && n < 6; t = strtok(NULL, ",")) {
        int sp = parse_species(t);
        if (sp < 0) { fprintf(stderr, "no species %s\n", t); return 2; }
        memset(&team[n], 0, sizeof team[n]);
        team[n].species = (uint16_t)sp;
        team[n].tera_type = -1;
        for (int i = 0; i < 6; i++) team[n].ivs[i] = 15;
        n++;
    }
    const char *why;
    if (!gc_run_set_starters(&run, team, n, &why)) { fprintf(stderr, "starters refused: %s\n", why); return 2; }
    char line[256];
    for (;;) {
        drain(&run, true);
        print_screen(&run);
        const gc_screen *s = gc_run_screen(&run);
        if (s->kind == GC_SCREEN_GAME_OVER || s->kind == GC_SCREEN_CLEAR) return 0;
        printf("> ");
        fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) return 0;
        gc_choice c = {0};
        char cmd[8] = "";
        int a = -1, b = -1, d = -1;
        int k = sscanf(line, "%7s %d %d %d", cmd, &a, &b, &d);
        if (k <= 0) continue;
        switch (s->kind) {
        case GC_SCREEN_COMMAND:
            c.kind = cmd[0] == 'p' ? GC_CH_POKEMON : cmd[0] == 'b' ? GC_CH_BALL : cmd[0] == 'r' ? GC_CH_RUN : GC_CH_FIGHT;
            c.a = (int16_t)a; c.b = -1;
            break;
        case GC_SCREEN_SWITCH: c.kind = GC_CH_PICK; c.a = (int16_t)atoi(cmd); break;
        case GC_SCREEN_LEARN_MOVE: c.kind = GC_CH_FORGET; c.a = (int16_t)atoi(cmd); break;
        case GC_SCREEN_EVOLVE: c.kind = cmd[0] == 'y' ? GC_CH_EVOLVE : GC_CH_CANCEL; c.a = cmd[0] == 'N'; break;
        case GC_SCREEN_REWARD:
            c.kind = cmd[0] == 'o' ? GC_CH_REWARD : cmd[0] == 's' ? GC_CH_SHOP : cmd[0] == 'r' ? GC_CH_REROLL : GC_CH_SKIP;
            c.a = (int16_t)a; c.b = (int16_t)b; c.c = (int16_t)d;
            break;
        case GC_SCREEN_BIOME: c.kind = GC_CH_BIOME; c.a = (int16_t)atoi(cmd); break;
        default: break;
        }
        if (!gc_run_choose(&run, &c, &why)) printf("  not allowed: %s\n", why);
    }
}

int main(int argc, char **argv)
{
    const char *seed = NULL, *starters = NULL;
    int autos = 0, first = 0, jobs = 4, verbose = 0, timeout = 60;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = argv[++i];
        else if (!strcmp(argv[i], "--starters") && i + 1 < argc) starters = argv[++i];
        else if (!strcmp(argv[i], "--auto") && i + 1 < argc) autos = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--first") && i + 1 < argc) first = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--jobs") && i + 1 < argc) jobs = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--timeout") && i + 1 < argc) timeout = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--verbose")) verbose = 1;
        else { fprintf(stderr, "usage: %s [--seed S] [--starters A,B,C] | --auto N [--first K] [--jobs J] [--verbose]\n", argv[0]); return 2; }
    }
    if (autos > 0) return soak(autos, first, jobs < 1 ? 1 : jobs, verbose, timeout);
    return play(seed, starters);
}
