/* The mystery encounters' selection rules against the pinned upstream (tests/mystery-rules.producer.test.ts): per encounter the tier, the per-run cap, the wave range,
 * the least party size, the money multiplier, the wave-modulus gate and the disallowed challenges of game-core's table; per biome the set of encounters that can spawn there
 * (mysteryEncountersByBiome). Custom Pillar Men has no upstream entry and is left out. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int mismatches, compared;
#define EQ(what, got, want) do { compared++; if ((got) != (want)) { if (mismatches++ < 30) printf("MISMATCH %s: got %ld want %ld\n", what, (long)(got), (long)(want)); } } while (0)

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: mystery_rules_diff_test <rules.txt>\n"); return 2; }
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror(argv[1]); return 2; }
    static unsigned char want_biome[64][GC_ME_PILLAR_MEN];
    static bool biome_given[64];
    int n_types = 0, n_biomes = 0;
    char line[2048];
    while (fgets(line, sizeof line, f)) {
        if (line[0] == 'T') {
            int id, max, wmin, wmax, party, mod, chal, tier;
            double money;
            char name[64];
            if (sscanf(line, "T %d %63s tier=%d max=%d wave=%d-%d party=%d money=%lf mod=%d chal=%d", &id, name, &tier, &max, &wmin, &wmax, &party, &money, &mod, &chal) != 10) continue;
            if (id < 0 || id >= GC_ME_PILLAR_MEN) { printf("MISMATCH %s: no game-core encounter %d\n", name, id); mismatches++; continue; }
            int g_tier, g_min, g_max, g_seen, g_party, g_chal, g_mod;
            double g_money;
            if (!gc_mystery_rule(id, &g_tier, &g_min, &g_max, &g_seen, &g_party, &g_chal, &g_money, &g_mod)) { mismatches++; continue; }
            const int want_tier = tier == 66 ? GC_ME_COMMON : tier == 40 ? GC_ME_GREAT : tier == 19 ? GC_ME_ULTRA : GC_ME_ROGUE;
            char w[96];
            snprintf(w, sizeof w, "%s tier", name); EQ(w, g_tier, want_tier);
            snprintf(w, sizeof w, "%s cap", name); EQ(w, g_seen, max);
            snprintf(w, sizeof w, "%s min wave", name); EQ(w, g_min, wmin);
            snprintf(w, sizeof w, "%s max wave", name); EQ(w, g_max, wmax);
            snprintf(w, sizeof w, "%s party", name); EQ(w, g_party, party);
            snprintf(w, sizeof w, "%s money x1000", name); EQ(w, (long)(g_money * 1000 + 0.5), (long)(money * 1000 + 0.5));
            snprintf(w, sizeof w, "%s wave modulus", name); EQ(w, g_mod, mod);
            snprintf(w, sizeof w, "%s challenges", name); EQ(w, g_chal, chal);
            n_types++;
        } else if (line[0] == 'B') {
            int biome;
            int off = 0;
            if (sscanf(line, "B %d %n", &biome, &off) < 1 || biome < 0 || biome >= 64) continue;
            biome_given[biome] = true;
            for (const char *p = line + off; *p && *p != '\n';) {
                char *e;
                const long t = strtol(p, &e, 10);
                if (e == p) break;
                if (t >= 0 && t < GC_ME_PILLAR_MEN) want_biome[biome][t] = 1;
                p = *e == ',' ? e + 1 : e;
            }
            n_biomes++;
        }
    }
    fclose(f);
    for (int b = 0; b < 64; b++)   /* the biomes upstream's map lists (the ids between them, and the End, are not biomes with encounters) */
        for (int t = 0; biome_given[b] && t < GC_ME_PILLAR_MEN; t++) {
            char w[64];
            snprintf(w, sizeof w, "biome %d encounter %d", b, t);
            EQ(w, gc_mystery_in_biome(b, t) ? 1 : 0, want_biome[b][t]);
        }
    printf("Mystery encounter rules: %d encounters, %d biomes, %d values compared. TOTAL mismatches %d\n", n_types, n_biomes, compared, mismatches);
    return mismatches ? 1 : 0;
}
