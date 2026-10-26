/* game-core: overall system data across runs (Dex entries, starter data, unlocks).
 * Follows upstream GameData (src/system/game-data.ts). */
#ifndef GC_SYSTEM_H
#define GC_SYSTEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GC_MAX_SPECIES_ID 9000
#define GC_MAX_EGGS 100

/* Forward declarations */
typedef struct gc_run gc_run;
typedef struct gc_mon gc_mon;

/* Per-species Dex data entry (upstream DexEntry) */
typedef struct {
    uint64_t seen_attr;
    uint64_t caught_attr;
    uint32_t nature_attr;
    uint32_t seen_count;
    uint32_t caught_count;
    uint32_t hatched_count;
    uint8_t ivs[6];
    uint64_t ribbon_flags; /* RibbonData payload; independent of win counters */
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
    uint8_t preference_flags; /* 1 nature, 2 appearance, 4 ability; absent in older saves */
    uint8_t nature_preference, appearance_preference, ability_preference;
} gc_starter_entry;

/* Upstream Unlocks */
typedef struct {
    bool endless_mode;
    bool mini_black_hole;
    bool spliced_endless_mode;
    bool eviolite;
} gc_unlocks;

/* An Egg held by system data (upstream EggData / Egg) */
typedef struct {
    uint32_t id;
    int32_t hatch_waves;  /* waves remaining to hatch */
    uint8_t tier;         /* EggTier */
    uint16_t species;     /* species ID (0 = random) */
    uint8_t source_type,variant_tier,egg_move_index;
    bool is_shiny,override_hidden_ability;
    uint64_t timestamp_ms;
} gc_egg;

/* VoucherType enum values */
enum {
    GC_VOUCHER_REGULAR = 0,
    GC_VOUCHER_PLUS = 1,
    GC_VOUCHER_PREMIUM = 2,
    GC_VOUCHER_GOLDEN = 3,
};

/* Stat milestones tracked across the system profile */
typedef struct {
    uint32_t classic_wins;
    uint32_t ribbons_owned; /* first counted roots (GameStats.ribbonsOwned) */
    uint32_t eggs_pulled,rare_eggs_pulled,epic_eggs_pulled,legendary_eggs_pulled,manaphy_eggs_pulled;
    uint32_t endless_wins;
    uint32_t total_battles;
    uint32_t total_catches;
    uint32_t total_hatches;
    uint32_t total_shinies;
    uint32_t highest_wave;
    uint32_t highest_damage;
    uint64_t total_money_earned;
} gc_stat_milestones;

/* What the custom achievements count (gc_achv.c; no upstream equivalent). Only ever grows. */
typedef struct {
    uint32_t jojo_beaten;   /* bit i: the JoJo challenger i (the table's order in custom-core) has been beaten */
    uint32_t jojo_battles;  /* JoJo battles won */
    uint32_t jojo_clean;    /* ... of them with none of the party fainted */
    uint32_t run_tag;       /* the run run_jojo counts in (a hash of its seed) */
    uint8_t run_jojo;       /* challengers beaten in that run */
    uint8_t best_run_jojo;  /* the most in one run */
    uint32_t custom_wins;   /* Custom runs won */
    uint32_t best_wave;     /* the highest wave any run reached */
    uint8_t feats;          /* GC_FEAT_* bits: won a run with that feat at least once */
    uint8_t hatch_feats;    /* GC_HATCH_* bits: hatched an Epic egg / a Legendary egg / a shiny from an egg */
} gc_achv_progress;

/* Overall system data (upstream GameData) */
typedef struct {
    gc_dex_entry dex[GC_MAX_SPECIES_ID + 1];           /* 1-indexed by SpeciesId */
    gc_starter_entry starters[GC_MAX_SPECIES_ID + 1];  /* 1-indexed by SpeciesId (starters only) */
    gc_unlocks unlocks;
    gc_egg eggs[GC_MAX_EGGS];
    int n_eggs;
    uint32_t voucher_counts[4];                         /* by VoucherType */
    uint8_t voucher_unlocks[512];                       /* 1 if voucher for trainer_type is unlocked */
    uint8_t achievement_flags[256];                     /* 1 if achievement_id is unlocked */
    bool classic_voucher_unlocked; /* achievement voucher, separate from trainer unlocks */
    uint32_t egg_pity[4],unlock_pity[4];
    gc_stat_milestones stats;                          /* profile stat milestones */
    gc_achv_progress achv;                             /* custom achievements (gc_achv.c) */
} gc_system;

/* Initialize system data with new-save defaults (upstream initDexData + initStarterData) */
void gc_system_init_defaults(gc_system *sys);

/* Dex and starter data helpers */
uint64_t gc_mon_dex_attr(const gc_mon *m);
/* the dex attributes a species can ever unlock (the mask setPokemonSpeciesCaught ANDs a catch with) */
uint64_t gc_species_full_unlocks_data(uint16_t species_id);
int16_t gc_species_prevolution(uint16_t species_id);
uint16_t gc_species_root_id(uint16_t species_id);
/* PokemonSpecies.getRootSpeciesId(true): stops at the first starter on the way down */
uint16_t gc_species_root_for_starter(uint16_t species_id);
int getStarterValueFriendshipCap(int starter_cost);

void gc_system_set_pokemon_seen(gc_system *sys, const gc_mon *m, bool increment_count, bool trainer);
bool gc_system_set_pokemon_caught(gc_system *sys, const gc_mon *m, bool increment_count, bool from_egg);
/* gameData.setPokemonCaught(pokemon, incrementCount, fromEgg, showMessage) for a catch: `show_message` is
 * upstream's own argument (a new starter's "added as a starter" message, then its prevolution), `daily` the Daily
 * Run's candy rule. Every species the message is for goes in `report`, in the order it shows. */
#define GC_CAUGHT_STARTED_MAX 8
typedef struct {
    uint16_t started[GC_CAUGHT_STARTED_MAX];
    int n_started;
} gc_caught_report;
bool gc_system_set_pokemon_caught_ex(gc_system *sys, const gc_mon *m, bool increment_count, bool from_egg,
                                     bool show_message, bool daily, gc_caught_report *report);
void gc_system_update_species_dex_ivs(gc_system *sys, uint16_t species_id, const uint8_t ivs[6]);
/* GameData.unlockSpeciesNature: requires an unlocked root, then marks the
 * exact species and every prevolution, without caught counts or candy. */
void gc_system_unlock_species_nature(gc_system *sys, uint16_t species_id, uint8_t nature);
bool gc_system_add_starter_candy(gc_system *sys, uint16_t species_id, int32_t count);
void gc_system_add_friendship(gc_system *sys, const gc_run *run, gc_mon *m, int friendship, bool capped);

/* Egg and Voucher helpers */
int gc_system_increment_egg_steps(gc_system *sys);
bool gc_system_add_egg(gc_system *sys, uint32_t id, int32_t hatch_waves, uint8_t tier, uint16_t species);
bool gc_system_has_voucher_unlock(const gc_system *sys, int trainer_type);
bool gc_system_unlock_voucher(gc_system *sys, int trainer_type, int voucher_type);
void gc_system_add_voucher_count(gc_system *sys, int voucher_type, int count);

/* Stable local slots for the named Classic victory achievements. */
enum { GC_ACHV_CLASSIC_VICTORY=192, GC_ACHV_UNEVOLVED_CLASSIC,
 GC_ACHV_10_RIBBONS, GC_ACHV_25_RIBBONS, GC_ACHV_50_RIBBONS,
 GC_ACHV_75_RIBBONS, GC_ACHV_100_RIBBONS };
#define GC_RIBBON_CLASSIC UINT64_C(0x08000000)
/* Achievements, Ribbons, and Stat Milestones helpers */
bool gc_system_has_achievement(const gc_system *sys, int achievement_id);
bool gc_system_unlock_achievement(gc_system *sys, int achievement_id);
void gc_system_add_classic_win(gc_system *sys, uint16_t species_id);
uint16_t gc_system_get_ribbons(const gc_system *sys, uint16_t species_id);

/* SD Card System Data Serializer */
/* Direct hidden-ability purchases are a user-requested extension at the same
 * base-species candy cost as the pinned native passive unlock. */
int gc_system_starter_unlock_cost(uint16_t species);
int gc_system_starter_reduction_cost(const gc_system *sys,uint16_t species);
bool gc_system_buy_starter_reduction(gc_system *sys,uint16_t species);
bool gc_system_buy_starter_unlock(gc_system *sys, uint16_t species, bool hidden_ability);
size_t gc_system_save(const gc_system *sys, void *buf, size_t cap);
bool gc_system_load(gc_system *sys, const void *buf, size_t len, const char **why);

#endif
