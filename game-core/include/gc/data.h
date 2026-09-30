/* game-core's generated tables (gen/out/gc_<name>.c, dumped from upstream by gen/dump-<name>.test.ts). */
#ifndef GC_DATA_H
#define GC_DATA_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* EvoCondKey (src/data/balance/pokemon-evolutions.ts) */
enum {
    GC_EVO_FRIENDSHIP = 1, GC_EVO_TIME, GC_EVO_MOVE, GC_EVO_MOVE_TYPE, GC_EVO_PARTY_TYPE, GC_EVO_WEATHER,
    GC_EVO_BIOME, GC_EVO_TYROGUE, GC_EVO_SHEDINJA, GC_EVO_TREASURE_TRACKER, GC_EVO_RANDOM_FORM,
    GC_EVO_SPECIES_CAUGHT, GC_EVO_GENDER, GC_EVO_NATURE, GC_EVO_HELD_ITEM,
};
/* One SpeciesEvolutionCondition entry: its key and the values it compares with */
typedef struct { uint8_t key, n; int16_t v[16]; const char *item_key; } gc_evo_cond;
/* A SpeciesFormEvolution: target species, level, EvolutionItem, preFormKey and evoFormKey as form
 * indexes (-1 = null, -2 = a key no form has), and its conditions in gc_evo_conds */
typedef struct {
    uint16_t species;
    uint8_t level, item;
    int8_t pre_form, evo_form;
    uint16_t cond_start;
    uint8_t n_cond;
} gc_evo;
typedef struct { uint16_t id, start, n; } gc_species_evo;   /* by SpeciesId, ascending */
extern const gc_species_evo gc_species_evos[];
extern const int gc_species_evo_count;
extern const gc_evo gc_evo_data[];
extern const gc_evo_cond gc_evo_conds[];

/* speciesStarterCosts: the starter species (sorted by id) and their cost */
typedef struct { uint16_t id; uint8_t cost; } gc_starter_cost_entry;
extern const gc_starter_cost_entry gc_starter_costs[];
extern const int gc_starter_cost_count;

/* A trainer config, as Trainer.getName and the money reward read it (gen/out/gc_trainers.c) */
typedef struct {
    uint16_t type;              /* TrainerType */
    const char *class_key;      /* trainersCommon:<class_key>: its personal names; NULL = it has none */
    double money_multiplier;    /* config.moneyMultiplier */
    bool is_boss, double_only;
    uint8_t n_reward_funcs;     /* config.modifierRewardFuncs.length: the rewards TrainerVictoryPhase gives */
    /* by TrainerVariant, then GC_TNAME_* (gc/run.h): the name as a gc_trainer_name_templates index. A
     * template is text with $t(key) where upstream translates; @N and @P stand for the trainer's and the
     * partner's personal names */
    uint16_t names[3][4];
} gc_trainer_config;
extern const gc_trainer_config gc_trainer_configs[];   /* ascending TrainerType */
extern const int gc_trainer_config_count;
extern const char *const gc_trainer_name_templates[];
const gc_trainer_config *gc_trainer_config_of(int type);   /* NULL = no such config */

#endif
