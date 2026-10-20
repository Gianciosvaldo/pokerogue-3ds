/* The generic wild moveset generator (src/gc_movegen.c) against upstream at 8555c08: replays the trace of
 * tests/movegen.producer.test.ts. "M" records are a random Pokemon (species, form, level, ability slot, gender, nature, IVs,
 * the arena's weather) whose moveset upstream's generateAndPopulateMoveset made in executeWithSeedOffset(.., offset, seed):
 * the moves in order and the next two draws of the RNG are compared, and the stats the Pokemon had. "E" records give the
 * Move.calculateEffectivePower(pokemon) of every damaging move for such a Pokemon with a given ability: each is compared
 * exactly. Usage: movegen_diff_test <trace> [planted]; exit 0 only with 0 mismatches. */
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

static int bad, shown;
#define CHECK(cond, ...) do { if (!(cond)) { bad++; if (shown < 14) { shown++; fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } } while (0)

typedef struct { int species, form, level, ability_index, gender, nature, ivs[6]; } shape;

static void make_mon(gc_mon *m, const shape *s, int weather)
{
    pr_enemy_gen g;
    memset(&g, 0, sizeof g);
    g.ability_index = s->ability_index;
    g.fusion_species = -1;
    g.id = 1;
    for (int k = 0; k < 6; k++) g.ivs[k] = s->ivs[k];
    g.gender = s->gender;
    g.form = s->form;
    g.nature = s->nature;
    g.friendship = -1;
    run.bt.weather = (uint8_t)weather;
    gc_mon_new_enemy(&run, m, s->species, s->level, &g, 0);
}

int main(int argc, char **argv)
{
    if (argc < 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: movegen_diff_test <trace>\n", stderr); return 2; }
    char c;
    if (fscanf(f, " %c", &c) != 1 || c != 'V') return 2;
    integer();
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, "movegen", 7);
    gc_system_init_defaults(&sys);
    run.sys = &sys;
    run.wave = 45;

    int cases = 0, unsupported = 0, moves_checked = 0, powers_checked = 0, e_cases = 0, sig = 0, four = 0;
    static int by_weather[10];
    while (fscanf(f, " %c", &c) == 1) {
        if (c == 'Z') break;
        if (c == 'M') {
            shape s;
            const int idx = (int)integer();
            s.species = (int)integer(); s.form = (int)integer(); s.level = (int)integer(); s.ability_index = (int)integer();
            const int forced = (int)integer(), ability = (int)integer();
            s.gender = (int)integer(); s.nature = (int)integer();
            for (int k = 0; k < 6; k++) s.ivs[k] = (int)integer();
            const int weather = (int)integer(), offset = (int)integer();
            char seed_text[64];
            word(seed_text, sizeof seed_text);
            int stats[6], n, up_moves[4] = {0};
            for (int k = 0; k < 6; k++) stats[k] = (int)integer();
            n = (int)integer();
            for (int k = 0; k < n && k < 4; k++) up_moves[k] = (int)integer();
            const int d1 = (int)integer(), d2 = (int)integer();
            cases++;
            by_weather[weather]++;

            gc_mon m;
            make_mon(&m, &s, weather);
            if (forced >= 0) m.b.base_ability = m.b.ability = (uint16_t)forced;   /* customPokemonData.ability */
            CHECK(m.b.ability == ability, "case %d: species %d form %d ability slot %d is %d, upstream %d", idx, s.species, s.form, s.ability_index, m.b.ability, ability);
            (void)forced;
            pr_seedstr seed;
            pr_seed_from_ascii(&seed, seed_text);
            pr_rng r = pr_run_seed_offset_rng(&run.run, offset, &seed);
            if (!gc_generate_moveset(&run, &r, &m)) {
                unsupported++;
                if (getenv("MG_VERBOSE")) fprintf(stderr, "unsupported: case %d species %d form %d level %d\n", idx, s.species, s.form, s.level);
                continue;
            }
            const int e1 = pr_rng_seed_int(&r, 1000000, 0), e2 = pr_rng_seed_int(&r, 1000000, 0);
            CHECK(m.b.n_moves == n, "case %d: species %d form %d level %d: %d moves, upstream %d", idx, s.species, s.form, s.level, m.b.n_moves, n);
            for (int k = 0; k < n && k < 4; k++) {
                moves_checked++;
                CHECK(m.b.moves[k] == up_moves[k], "case %d: species %d form %d level %d ability %d weather %d: move %d is %d, upstream %d", idx, s.species, s.form, s.level, ability, weather, k, m.b.moves[k], up_moves[k]);
            }
            CHECK(e1 == d1 && e2 == d2, "case %d: species %d level %d: the RNG afterwards gives %d %d, upstream %d %d", idx, s.species, s.level, e1, e2, d1, d2);
            pr_calculate_stats(&run.bt, &m.b);
            for (int k = 0; k < 6; k++) CHECK(m.b.stats[k] == stats[k], "case %d: stat %d is %d, upstream %d", idx, k, m.b.stats[k], stats[k]);
            if (n == 4) four++;
            (void)sig;
        } else if (c == 'E') {
            shape s;
            const int idx = (int)integer();
            s.species = (int)integer(); s.form = (int)integer(); s.level = (int)integer(); s.ability_index = (int)integer();
            const int ability = (int)integer();
            s.gender = (int)integer(); s.nature = (int)integer();
            for (int k = 0; k < 6; k++) s.ivs[k] = (int)integer();
            const int weather = (int)integer();
            for (int k = 0; k < 6; k++) integer();
            const int n = (int)integer();
            static uint16_t moves[1024];
            static double up[1024], got[1024];
            for (int k = 0; k < n; k++) { moves[k] = (uint16_t)integer(); up[k] = real(); }
            gc_mon m;
            make_mon(&m, &s, weather);
            m.b.base_ability = m.b.ability = (uint16_t)ability;
            e_cases++;
            if (!gc_mg_effective_powers(&run, &m, moves, n, got)) { unsupported++; continue; }
            for (int k = 0; k < n; k++) {
                powers_checked++;
                CHECK(got[k] == up[k], "E case %d: species %d form %d level %d ability %d gender %d weather %d: move %d power %.17g, upstream %.17g", idx, s.species, s.form, s.level, ability, s.gender, weather, moves[k], got[k], up[k]);
            }
        } else {
            fprintf(stderr, "wrong record %c\n", c);
            return 2;
        }
    }
    printf("movegen: %d movesets (%d with four moves, %d moves), %d effective-power records (%d powers), %d unsupported, weathers", cases, four, moves_checked, e_cases, powers_checked, unsupported);
    for (int w = 0; w < 10; w++) printf(" %d", by_weather[w]);
    printf("\nTOTAL %d\n", bad);
    return bad ? 1 : 0;
}
