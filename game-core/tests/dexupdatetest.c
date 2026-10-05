/*
 * Differential test for game-core's Dex updates logic.
 * Compares C system data state against upstream GameData after random operations.
 * Usage: dexupdatetest <updates_file>
 * Reads the dumps of gen/dump-dexupdates.test.ts and gen/dump-dexcatch.test.ts (card 94: OP_CATCH, the dex, candy
 * and IV side of a catch, and the "added as a starter" messages it shows, OP_MSG).
 */
#include "gc/system.h"
#include "gc/run.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_system sys;

/* the species whose "added as a starter" message the last OP_CATCH showed, in order */
static gc_caught_report last_report;

int main(int argc, char **argv) {
    const char *filepath = (argc > 1) ? argv[1] : "/tmp/gc_dex_updates.txt";
    FILE *f = fopen(filepath, "r");
    if (!f) {
        fprintf(stderr, "dexupdatetest: failed to open updates file: %s\n", filepath);
        return 1;
    }

    gc_system_init_defaults(&sys);

    /* Construct a dummy run for gc_system_add_friendship mode check */
    gc_run dummy_run;
    memset(&dummy_run, 0, sizeof(dummy_run));
    dummy_run.run.mode = 0; /* GAME_MODES_CLASSIC */

    char line[512];
    int ops_executed = 0;
    int species_compared = 0;
    int starters_compared = 0;
    int mismatches = 0;
    int messages_compared = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "OP_SEEN ", 8) == 0) {
            unsigned int species, form, shiny, variant, inc, trainer;
            int gender;
            if (sscanf(line + 8, "%u %u %d %u %u %u %u", &species, &form, &gender, &shiny, &variant, &inc, &trainer) == 7) {
                gc_mon m;
                memset(&m, 0, sizeof(m));
                m.b.species = (uint16_t)species;
                m.b.form = (uint8_t)form;
                m.b.gender = (int8_t)gender;
                m.b.shiny = (bool)shiny;
                m.variant = (uint8_t)variant;
                gc_system_set_pokemon_seen(&sys, &m, (bool)inc, (bool)trainer);
                ops_executed++;
            }
        } else if (strncmp(line, "OP_IVS ", 7) == 0) {
            unsigned int species, ivs[6];
            if (sscanf(line + 7, "%u %u %u %u %u %u %u", &species, &ivs[0], &ivs[1], &ivs[2], &ivs[3], &ivs[4], &ivs[5]) == 7) {
                uint8_t iv_bytes[6];
                for (int k = 0; k < 6; k++) iv_bytes[k] = (uint8_t)ivs[k];
                gc_system_update_species_dex_ivs(&sys, (uint16_t)species, iv_bytes);
                ops_executed++;
            }
        } else if (strncmp(line, "OP_UNLOCK_ROOT ", 15) == 0) {
            unsigned int root_id;
            if (sscanf(line + 15, "%u", &root_id) == 1 && root_id <= GC_MAX_SPECIES_ID) {
                sys.dex[root_id].caught_attr |= 157ULL;
                ops_executed++;
            }
        } else if (strncmp(line, "OP_CAUGHT ", 10) == 0) {
            unsigned int species, form, shiny, variant, ability_idx, nature;
            int gender;
            if (sscanf(line + 10, "%u %u %d %u %u %u %u", &species, &form, &gender, &shiny, &variant, &ability_idx, &nature) == 7) {
                gc_mon m;
                memset(&m, 0, sizeof(m));
                m.b.species = (uint16_t)species;
                m.b.form = (uint8_t)form;
                m.b.gender = (int8_t)gender;
                m.b.shiny = (bool)shiny;
                m.variant = (uint8_t)variant;
                m.ability_index = (uint8_t)ability_idx;
                m.b.nature = (uint8_t)nature;
                gc_system_set_pokemon_caught(&sys, &m, false, false);
                ops_executed++;
            }
        } else if (strncmp(line, "OP_UNLOCK_SPECIES ", 18) == 0) {
            unsigned int species;
            uint64_t attr;
            if (sscanf(line + 18, "%u %" SCNu64, &species, &attr) == 2 && species <= GC_MAX_SPECIES_ID) {
                sys.dex[species].caught_attr = attr;
                ops_executed++;
            }
        } else if (strncmp(line, "OP_CATCH ", 9) == 0) {
            unsigned int species, form, shiny, variant, ability_idx, nature, daily, boss, ivs[6];
            int gender;
            if (sscanf(line + 9, "%u %u %d %u %u %u %u %u %u %u %u %u %u %u %u", &species, &form, &gender, &shiny, &variant,
                       &ability_idx, &nature, &daily, &boss, &ivs[0], &ivs[1], &ivs[2], &ivs[3], &ivs[4], &ivs[5]) == 15) {
                gc_mon m;
                memset(&m, 0, sizeof(m));
                m.b.species = (uint16_t)species;
                m.b.form = (uint8_t)form;
                m.b.gender = (int8_t)gender;
                m.b.shiny = (bool)shiny;
                m.variant = (uint8_t)variant;
                m.ability_index = (uint8_t)ability_idx;
                m.b.nature = (uint8_t)nature;
                m.b.is_boss = (bool)boss;
                for (int k = 0; k < 6; k++) m.b.ivs[k] = (uint8_t)ivs[k];
                /* AttemptCapturePhase.catch(): the IVs of the species' root, then setPokemonCaught(pokemon) */
                gc_system_update_species_dex_ivs(&sys, gc_species_root_for_starter((uint16_t)species), m.b.ivs);
                gc_system_set_pokemon_caught_ex(&sys, &m, true, false, true, (bool)daily, &last_report);
                ops_executed++;
            }
        } else if (strncmp(line, "OP_MSG ", 7) == 0) {
            int n = 0, off = 0;
            if (sscanf(line + 7, "%d%n", &n, &off) >= 1) {
                bool same = n == last_report.n_started;
                for (int k = 0; k < n; k++) {
                    int id = -2, used = 0;
                    if (sscanf(line + 7 + off, "%d%n", &id, &used) != 1) { same = false; break; }
                    off += used;
                    if (k >= last_report.n_started || last_report.started[k] != id) same = false;
                }
                if (!same) {
                    fprintf(stderr, "OP_MSG mismatch after op %d: UP %s", ops_executed, line + 7);
                    fprintf(stderr, "  C started %d:", last_report.n_started);
                    for (int k = 0; k < last_report.n_started; k++) fprintf(stderr, " %u", last_report.started[k]);
                    fprintf(stderr, "\n");
                    mismatches++;
                }
                messages_compared++;
            }
        } else if (strncmp(line, "OP_FRIENDSHIP ", 13) == 0) {
            unsigned int species, capped;
            int amount;
            if (sscanf(line + 13, "%u %d %u", &species, &amount, &capped) == 3) {
                gc_mon m;
                memset(&m, 0, sizeof(m));
                m.b.species = (uint16_t)species;
                m.b.friendship = 0;
                gc_system_add_friendship(&sys, &dummy_run, &m, amount, (bool)capped);
                ops_executed++;
            }
        } else if (strncmp(line, "DEX ", 4) == 0) {
            uint16_t id;
            uint64_t seen_attr, caught_attr;
            uint32_t nature_attr, seen_count, caught_count, hatched_count;
            unsigned int ivs[6];

            if (sscanf(line + 4, "%" SCNu16 " %" SCNu64 " %" SCNu64 " %" SCNu32 " %" SCNu32 " %" SCNu32 " %" SCNu32 " %u %u %u %u %u %u",
                       &id, &seen_attr, &caught_attr, &nature_attr, &seen_count, &caught_count, &hatched_count,
                       &ivs[0], &ivs[1], &ivs[2], &ivs[3], &ivs[4], &ivs[5]) != 13) {
                fprintf(stderr, "dexupdatetest: malformed DEX line: %s\n", line);
                mismatches++;
                continue;
            }

            if (id > GC_MAX_SPECIES_ID) continue;

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
                fprintf(stderr, "dexupdatetest: malformed STARTER line: %s\n", line);
                mismatches++;
                continue;
            }

            if (id > GC_MAX_SPECIES_ID) continue;

            starters_compared++;
            gc_starter_entry *s = &sys.starters[id];

            if (s->ability_attr != ability_attr) {
                fprintf(stderr, "STARTER %u ability_attr mismatch: C=%" PRIu32 " UP=%" PRIu32 "\n", id, s->ability_attr, ability_attr);
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
            /* card 94: what a catch must leave alone (egg moves, passives, cost reduction, the moveset, ribbons) */
            if (s->passive_attr != passive_attr || s->value_reduction != value_reduction || s->egg_moves != (uint8_t)egg_moves
                || s->moveset[0] != m0 || s->moveset[1] != m1 || s->moveset[2] != m2 || s->moveset[3] != m3
                || s->classic_win_count != classic_win) {
                fprintf(stderr, "STARTER %u egg moves/passives/moveset/wins differ from upstream (egg moves C=%u UP=%u)\n", id, s->egg_moves, egg_moves);
                mismatches++;
            }
        }
    }

    fclose(f);

    printf("dexupdatetest: executed %d operations, compared %d species and %d starters", ops_executed, species_compared,
           starters_compared);
    if (messages_compared) printf(" and the starter messages of %d catches", messages_compared);
    printf(". Mismatches: %d\n", mismatches);

    return mismatches > 0 ? 1 : 0;
}
