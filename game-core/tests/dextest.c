/*
 * Differential test for game-core's system defaults (gc_system_init_defaults).
 * Compares C system defaults against upstream's dumped GameData defaults.
 * Usage: dextest <defaults_file>
 */
#include "gc/system.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_system sys;

int main(int argc, char **argv) {
    const char *filepath = (argc > 1) ? argv[1] : "/tmp/gc_dex_defaults.txt";
    FILE *f = fopen(filepath, "r");
    if (!f) {
        fprintf(stderr, "dextest: failed to open defaults file: %s\n", filepath);
        return 1;
    }

    gc_system_init_defaults(&sys);

    char line[512];
    int species_compared = 0;
    int starters_compared = 0;
    int unlocks_compared = 0;
    int mismatches = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "DEX ", 4) == 0) {
            uint16_t id;
            uint64_t seen_attr, caught_attr;
            uint32_t nature_attr, seen_count, caught_count, hatched_count;
            unsigned int ivs[6];

            if (sscanf(line + 4, "%" SCNu16 " %" SCNu64 " %" SCNu64 " %" SCNu32 " %" SCNu32 " %" SCNu32 " %" SCNu32 " %u %u %u %u %u %u",
                       &id, &seen_attr, &caught_attr, &nature_attr, &seen_count, &caught_count, &hatched_count,
                       &ivs[0], &ivs[1], &ivs[2], &ivs[3], &ivs[4], &ivs[5]) != 13) {
                fprintf(stderr, "dextest: malformed DEX line: %s\n", line);
                mismatches++;
                continue;
            }

            if (id > GC_MAX_SPECIES_ID) {
                fprintf(stderr, "dextest: species ID %u exceeds GC_MAX_SPECIES_ID\n", id);
                mismatches++;
                continue;
            }

            species_compared++;
            gc_dex_entry *e = &sys.dex[id];

            if (e->seen_attr != seen_attr) {
                fprintf(stderr, "DEX %u seen_attr mismatch: C=%" PRIu64 " UP=%" PRIu64 "\n", id, e->seen_attr, seen_attr);
                mismatches++;
            }
            if (e->caught_attr != caught_attr) {
                fprintf(stderr, "DEX %u caught_attr mismatch: C=%" PRIu64 " UP=%" PRIu64 "\n", id, e->caught_attr, caught_attr);
                mismatches++;
            }
            if (e->nature_attr != nature_attr) {
                fprintf(stderr, "DEX %u nature_attr mismatch: C=%" PRIu32 " UP=%" PRIu32 "\n", id, e->nature_attr, nature_attr);
                mismatches++;
            }
            if (e->seen_count != seen_count) {
                fprintf(stderr, "DEX %u seen_count mismatch: C=%" PRIu32 " UP=%" PRIu32 "\n", id, e->seen_count, seen_count);
                mismatches++;
            }
            if (e->caught_count != caught_count) {
                fprintf(stderr, "DEX %u caught_count mismatch: C=%" PRIu32 " UP=%" PRIu32 "\n", id, e->caught_count, caught_count);
                mismatches++;
            }
            if (e->hatched_count != hatched_count) {
                fprintf(stderr, "DEX %u hatched_count mismatch: C=%" PRIu32 " UP=%" PRIu32 "\n", id, e->hatched_count, hatched_count);
                mismatches++;
            }
            for (int k = 0; k < 6; k++) {
                if (e->ivs[k] != (uint8_t)ivs[k]) {
                    fprintf(stderr, "DEX %u ivs[%d] mismatch: C=%u UP=%u\n", id, k, e->ivs[k], ivs[k]);
                    mismatches++;
                }
            }
        } else if (strncmp(line, "STARTER ", 8) == 0) {
            uint16_t id;
            uint32_t ability_attr, passive_attr, value_reduction;
            int32_t candy_count;
            uint32_t friendship;
            unsigned int egg_moves, m0, m1, m2, m3, classic_win;

            if (sscanf(line + 8, "%" SCNu16 " %" SCNu32 " %" SCNu32 " %" SCNu32 " %" SCNd32 " %" SCNu32 " %u %u %u %u %u %u",
                       &id, &ability_attr, &passive_attr, &value_reduction, &candy_count, &friendship,
                       &egg_moves, &m0, &m1, &m2, &m3, &classic_win) != 12) {
                fprintf(stderr, "dextest: malformed STARTER line: %s\n", line);
                mismatches++;
                continue;
            }

            if (id > GC_MAX_SPECIES_ID) {
                fprintf(stderr, "dextest: starter ID %u exceeds GC_MAX_SPECIES_ID\n", id);
                mismatches++;
                continue;
            }

            starters_compared++;
            gc_starter_entry *s = &sys.starters[id];

            if (s->ability_attr != ability_attr) {
                fprintf(stderr, "STARTER %u ability_attr mismatch: C=%" PRIu32 " UP=%" PRIu32 "\n", id, s->ability_attr, ability_attr);
                mismatches++;
            }
            if (s->passive_attr != passive_attr) {
                fprintf(stderr, "STARTER %u passive_attr mismatch: C=%" PRIu32 " UP=%" PRIu32 "\n", id, s->passive_attr, passive_attr);
                mismatches++;
            }
            if (s->value_reduction != value_reduction) {
                fprintf(stderr, "STARTER %u value_reduction mismatch: C=%" PRIu32 " UP=%" PRIu32 "\n", id, s->value_reduction, value_reduction);
                mismatches++;
            }
            if (s->candy_count != candy_count) {
                fprintf(stderr, "STARTER %u candy_count mismatch: C=%" PRId32 " UP=%" PRId32 "\n", id, s->candy_count, candy_count);
                mismatches++;
            }
            if (s->friendship != (uint16_t)friendship) {
                fprintf(stderr, "STARTER %u friendship mismatch: C=%u UP=%" PRIu32 "\n", id, s->friendship, friendship);
                mismatches++;
            }
            if (s->egg_moves != (uint8_t)egg_moves) {
                fprintf(stderr, "STARTER %u egg_moves mismatch: C=%u UP=%u\n", id, s->egg_moves, egg_moves);
                mismatches++;
            }
            if (s->moveset[0] != (uint16_t)m0 || s->moveset[1] != (uint16_t)m1 ||
                s->moveset[2] != (uint16_t)m2 || s->moveset[3] != (uint16_t)m3) {
                fprintf(stderr, "STARTER %u moveset mismatch\n", id);
                mismatches++;
            }
            if (s->classic_win_count != (uint16_t)classic_win) {
                fprintf(stderr, "STARTER %u classic_win_count mismatch: C=%u UP=%u\n", id, s->classic_win_count, classic_win);
                mismatches++;
            }
        } else if (strncmp(line, "UNLOCKS ", 8) == 0) {
            int endless, mini_black_hole, spliced_endless, eviolite;
            if (sscanf(line + 8, "%d %d %d %d", &endless, &mini_black_hole, &spliced_endless, &eviolite) != 4) {
                fprintf(stderr, "dextest: malformed UNLOCKS line: %s\n", line);
                mismatches++;
                continue;
            }

            unlocks_compared++;
            if (sys.unlocks.endless_mode != (bool)endless ||
                sys.unlocks.mini_black_hole != (bool)mini_black_hole ||
                sys.unlocks.spliced_endless_mode != (bool)spliced_endless ||
                sys.unlocks.eviolite != (bool)eviolite) {
                fprintf(stderr, "UNLOCKS mismatch\n");
                mismatches++;
            }
        }
    }

    fclose(f);

    printf("dextest: compared %d species and %d starters (%d unlocks). Mismatches: %d\n",
           species_compared, starters_compared, unlocks_compared, mismatches);

    return mismatches > 0 ? 1 : 0;
}
