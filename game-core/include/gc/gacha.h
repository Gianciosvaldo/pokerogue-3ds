/* Bounded offline purchase/generation, independent entropy and scene RNGs. */
#ifndef GC_GACHA_H
#define GC_GACHA_H
#include "gc/system.h"
#include "pr/rng.h"
typedef struct {uint16_t species;int8_t cost;uint8_t tier;bool variants;} gc_egg_pool_entry;
extern const int gc_egg_pool_count;
extern const gc_egg_pool_entry gc_egg_pool[];
typedef struct {int machine,cursor;uint64_t timestamp_ms;pr_rng entropy,scene_rng;uint32_t entropy_draws;} gc_gacha_ctx;
typedef struct {int n;uint8_t order[25];const char *error_key;} gc_gacha_result;
/* Failures leave profile/RNG/result unchanged; expected capacity/cost refusal
 * returns its source key. Successful order indexes are relative to appended eggs. */
bool gc_system_gacha_pull(gc_system *sys,gc_gacha_ctx *ctx,gc_gacha_result *out,const char **why);
uint16_t gc_gacha_featured_species(uint64_t timestamp_ms);
/* Native hatch-count discount and same-species Egg constructor. No voucher,
 * gacha-pity or pull-stat changes. Refusal preserves profile and entropy. */
int gc_system_species_egg_cost(const gc_system *sys, uint16_t species);
bool gc_system_buy_species_egg(gc_system *sys, uint16_t species, gc_gacha_ctx *ctx, const char **why);
#endif
