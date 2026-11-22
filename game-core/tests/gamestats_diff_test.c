/*
 * Differential test for the Game Stats that setPokemonSeen / setPokemonSpeciesCaught keep (src/gc_system.c): replays the trace of
 * tests/gamestats.producer.test.ts through gc_system_set_pokemon_seen / gc_system_set_pokemon_caught_ex and compares the 15 counters after
 * every op with upstream's gameStats (pokemonSeen/Caught/Hatched, the sub-legendary, legendary, mythical and shiny ones).
 * Usage: gamestats_diff_test <trace>. Exit 1 on a mismatch; the last line is "TOTAL mismatches in this trace: N".
 */
#include "gc/system.h"
#include "gc/run.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_system sys;

static void counters(uint32_t c[15]) {
    const gc_game_stats *g = &sys.gstats;
    const uint32_t v[15] = {g->pokemon_seen, sys.stats.total_catches, g->pokemon_hatched,
                            g->sub_legendary_seen, g->sub_legendary_caught, g->sub_legendary_hatched,
                            g->legendary_seen, g->legendary_caught, g->legendary_hatched,
                            g->mythical_seen, g->mythical_caught, g->mythical_hatched,
                            g->shiny_seen, g->shiny_caught, g->shiny_hatched};
    memcpy(c, v, sizeof v);
}

static const char *const NAMES[15] = {"pokemonSeen", "pokemonCaught", "pokemonHatched", "subLegendarySeen", "subLegendaryCaught", "subLegendaryHatched",
                                      "legendarySeen", "legendaryCaught", "legendaryHatched", "mythicalSeen", "mythicalCaught", "mythicalHatched",
                                      "shinySeen", "shinyCaught", "shinyHatched"};

static bool read_mon(const char *s, gc_mon *m) {
    unsigned species, form, shiny, variant, ability, nature, fusion, fusion_shiny;
    int gender;
    if (sscanf(s, "%u %u %d %u %u %u %u %u %u", &species, &form, &gender, &shiny, &variant, &ability, &nature, &fusion, &fusion_shiny) != 9) return false;
    memset(m, 0, sizeof *m);
    m->b.species = (uint16_t)species;
    m->b.form = (uint8_t)form;
    m->b.gender = (int8_t)gender;
    m->b.shiny = shiny;
    m->variant = (uint8_t)variant;
    m->ability_index = (uint8_t)ability;
    m->b.nature = (uint8_t)nature;
    m->b.fusion_species = (uint16_t)fusion;
    m->fusion_shiny = fusion_shiny;
    return true;
}

/* skips the nine fields of a Pokemon */
static const char *after_mon(const char *s) {
    for (int k = 0; k < 9; k++) {
        while (*s == ' ') s++;
        while (*s && *s != ' ') s++;
    }
    return s;
}

int main(int argc, char **argv) {
    FILE *f = fopen(argc > 1 ? argv[1] : "/tmp/gc_gamestats.txt", "r");
    if (!f) { fprintf(stderr, "gamestats_diff_test: cannot open the trace\n"); return 1; }
    gc_system_init_defaults(&sys);
    char line[512];
    int ops = 0, compared = 0, mismatches = 0;
    while (fgets(line, sizeof line, f)) {
        gc_mon m;
        if (!strncmp(line, "OP_SEEN ", 8)) {
            unsigned inc, trainer;
            if (!read_mon(line + 8, &m) || sscanf(after_mon(line + 8), "%u %u", &inc, &trainer) != 2) { fprintf(stderr, "bad line: %s", line); return 2; }
            gc_system_set_pokemon_seen(&sys, &m, inc, trainer);
            ops++;
        } else if (!strncmp(line, "OP_UNLOCK_SPECIES ", 18)) {
            unsigned species;
            uint64_t attr;
            if (sscanf(line + 18, "%u %" SCNu64, &species, &attr) == 2 && species <= GC_MAX_SPECIES_ID) sys.dex[species].caught_attr = attr;
        } else if (!strncmp(line, "OP_CATCH ", 9)) {
            unsigned egg, message;
            if (!read_mon(line + 9, &m) || sscanf(after_mon(line + 9), "%u %u", &egg, &message) != 2) { fprintf(stderr, "bad line: %s", line); return 2; }
            gc_system_set_pokemon_caught_ex(&sys, &m, true, egg, message, false, NULL);
            ops++;
        } else if (!strncmp(line, "GS ", 3)) {
            uint32_t up[15], c[15];
            unsigned long long v[15];
            if (sscanf(line + 3, "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6],
                       &v[7], &v[8], &v[9], &v[10], &v[11], &v[12], &v[13], &v[14]) != 15) { fprintf(stderr, "bad line: %s", line); return 2; }
            for (int k = 0; k < 15; k++) up[k] = (uint32_t)v[k];
            counters(c);
            compared++;
            for (int k = 0; k < 15; k++)
                if (up[k] != c[k]) {
                    if (mismatches < 20) fprintf(stderr, "op %d: %s upstream %u, C %u\n", ops, NAMES[k], up[k], c[k]);
                    mismatches++;
                }
        }
    }
    fclose(f);
    printf("gamestats_diff_test: %d ops, %d counter lines compared (15 counters each)\n", ops, compared);
    printf("TOTAL mismatches in this trace: %d\n", mismatches);
    return mismatches ? 1 : 0;
}
