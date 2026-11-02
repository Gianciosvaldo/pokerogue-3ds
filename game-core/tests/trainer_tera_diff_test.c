/* Trainer Terastallization against upstream at 8555c08: replays the trace of tests/trainer-tera.producer.test.ts (random trainer types, waves,
 * party sizes and parties with Shedinja; upstream's own Trainer.genAI run on the config with a sown RNG, then config.trainerAI.instantTeras
 * and Trainer.shouldTera per slot) through gc_trainer_gen_ai / gc_trainer_instant_teras.
 *
 * Per case, compared: the slots that Terastallize (the list, in order), the Tera type of every party member afterwards, and the RNG state after
 * (the random pick draws from it). Usage: trainer_tera_diff_test <trace>; exit 0 only with 0 mismatches. */
#include "../src/internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static FILE *f;
static double number(void) { double v; if (fscanf(f, "%lf", &v) != 1) { fputs("malformed input\n", stderr); exit(2); } return v; }
static void tag(char want)
{
    char c;
    if (fscanf(f, " %c", &c) != 1 || c != want) { fprintf(stderr, "wrong record: wanted %c\n", want); exit(2); }
}

int main(int argc, char **argv)
{
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: trainer_tera_diff_test <trace>\n", stderr); return 2; }
    char head[16];
    int version, cases;
    if (fscanf(f, "%15s %d %d", head, &version, &cases) != 3 || strcmp(head, "TRAINERTERA") || version != 1 || cases < 1) { fputs("bad header\n", stderr); return 2; }
    int threw = 0, mismatches = 0, with_list = 0, random_pick = 0, fixed_pick = 0, shedinja_cases = 0, typed = 0, static_only = 0, waves_before = 0, no_config = 0;
    for (int i = 0; i < cases; i++) {
        tag('C');
        const int id = (int)number(), type = (int)number(), wave = (int)number(), n = (int)number();
        char seedstr[32];
        if (fscanf(f, "%31s", seedstr) != 1) return 2;
        pr_rng rng;
        memset(&rng, 0, sizeof rng);
        rng.c = number(); rng.s0 = number(); rng.s1 = number(); rng.s2 = number();
        if (id != i || n < 1 || n > 6) { fputs("case header\n", stderr); return 2; }
        memset(&run, 0, sizeof run);
        run.battle_type = BATTLE_TYPE_TRAINER;
        run.trainer_type = type;
        run.wave = wave;
        run.n_enemy = n;
        run.run.rng = rng;
        int before[6], shed = 0;
        for (int k = 0; k < n; k++) {
            run.enemy[k].b.species = (uint16_t)number();
            before[k] = (int)number();
            run.enemy[k].b.tera_type = (int8_t)before[k];
            shed |= run.enemy[k].b.species == SPECIES_ID_SHEDINJA;
        }
        tag('R');
        char first[16];
        if (fscanf(f, "%15s", first) != 1) return 2;
        const bool u_threw = !strcmp(first, "X");
        const int u_n = u_threw ? 0 : atoi(first);
        int u_list[8] = {0};
        for (int k = 0; k < u_n; k++) { const int v = (int)number(); if (k < 8) u_list[k] = v; }
        int u_tera[6], u_should[6];
        for (int k = 0; k < n; k++) u_tera[k] = (int)number();
        for (int k = 0; k < n; k++) u_should[k] = (int)number();
        pr_rng after;
        memset(&after, 0, sizeof after);
        if (!u_threw) { after.c = number(); after.s0 = number(); after.s1 = number(); after.s2 = number(); }
        else threw++;

        const gc_trainer_config *tc = gc_trainer_config_of(type);
        gc_trainer_gen_ai(&run);
        int8_t mine[8];
        const int m_n = gc_trainer_instant_teras(&run, mine, 8);
        int bad = 0;
#define EXPECT(cond, what) do { if (!(cond)) { bad++; if (mismatches < 12) fprintf(stderr, "case %d (trainer %d, wave %d, %d members): %s\n", i, type, wave, n, what); } } while (0)
        EXPECT(m_n == u_n, "how many slots Terastallize");
        for (int k = 0; k < m_n && k < u_n; k++) EXPECT(mine[k] == u_list[k], "the slot that Terastallizes");
        for (int k = 0; k < n; k++) {
            EXPECT(run.enemy[k].b.tera_type == u_tera[k], "a member's Tera type");
            int listed = 0;
            for (int j = 0; j < m_n; j++) listed |= mine[j] == k;
            EXPECT(listed == u_should[k], "shouldTera");
        }
        if (!u_threw) EXPECT(run.run.rng.c == after.c && run.run.rng.s0 == after.s0 && run.run.rng.s1 == after.s1 && run.run.rng.s2 == after.s2, "the RNG state after");
        else EXPECT(m_n == (tc ? __builtin_popcount(tc->tera_static) : 0), "upstream threw: only the config's own slots stay");
        if (u_n) with_list++;
        if (!tc) no_config++;
        if (tc && tc->tera_random && wave >= tc->tera_min_wave) {
            const bool fixed = tc->tera_slot[n] >= 0 && !(tc->tera_slot[n] < n && run.enemy[tc->tera_slot[n]].b.species == SPECIES_ID_SHEDINJA && tc->tera_type >= 0 && tc->tera_type != POKEMON_TYPE_BUG);
            if (fixed) fixed_pick++; else random_pick++;
            if (tc->tera_type >= 0) typed++;
        } else if (tc && tc->tera_random) {
            waves_before++;
        }
        if (tc && tc->tera_static && !tc->tera_random) static_only++;
        if (shed) shedinja_cases++;
        if (bad) mismatches++;
        (void)seedstr;
    }
    char end[16];
    if (fscanf(f, "%15s", end) != 1 || strcmp(end, "END")) { fputs("missing END\n", stderr); return 2; }
    printf("TOTAL cases=%d mismatches=%d with-tera-slots=%d fixed-slot=%d random-pick=%d typed=%d static-only=%d before-min-wave=%d with-shedinja=%d no-config=%d upstream-threw=%d\n",
           cases, mismatches, with_list, fixed_pick, random_pick, typed, static_only, waves_before, shedinja_cases, no_config, threw);
    return mismatches ? 1 : 0;
}
