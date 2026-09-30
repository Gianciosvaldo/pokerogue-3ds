/* game-core: overall system data across runs (Dex entries, starter data, unlocks).
 * Follows upstream GameData (src/system/game-data.ts). */
#ifndef GC_SYSTEM_H
#define GC_SYSTEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GC_MAX_SPECIES_ID 9000

/* Per-species Dex data entry (upstream DexEntry) */
typedef struct {
    uint64_t seen_attr;
    uint64_t caught_attr;
    uint32_t nature_attr;
    uint32_t seen_count;
    uint32_t caught_count;
    uint32_t hatched_count;
    uint8_t ivs[6];
} gc_dex_entry;

/* Per-starter data entry (upstream StarterDataEntry) */
typedef struct {
    uint32_t ability_attr;
    uint32_t passive_attr;
    uint32_t value_reduction;
    int32_t candy_count;
    uint16_t friendship;
    uint8_t egg_moves;
    uint16_t moveset[4];   /* 0 if default / empty */
    uint16_t classic_win_count;
} gc_starter_entry;

/* Upstream Unlocks */
typedef struct {
    bool endless_mode;
    bool mini_black_hole;
    bool spliced_endless_mode;
    bool eviolite;
} gc_unlocks;

/* Overall system data (upstream GameData) */
typedef struct {
    gc_dex_entry dex[GC_MAX_SPECIES_ID + 1];           /* 1-indexed by SpeciesId */
    gc_starter_entry starters[GC_MAX_SPECIES_ID + 1];  /* 1-indexed by SpeciesId (starters only) */
    gc_unlocks unlocks;
} gc_system;

/* Initialize system data with new-save defaults (upstream initDexData + initStarterData) */
void gc_system_init_defaults(gc_system *sys);

#endif
