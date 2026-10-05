#ifndef PR_MOVESET_H
#define PR_MOVESET_H
#include "pr/run.h"
/* Constructor-boundary input. Battle owner supplies effective ATK/SPATK.
 * This API does not wire game-core or calculate battle stats. */
typedef struct {
    int species, form, level, ability, attack, special_attack;
    bool trainer, boss, passive, fused, will_tera, inverse, other_challenges;
} pr_moveset_ctx;
typedef enum {
    PR_MOVESET_OK, PR_MOVESET_SPECIES, PR_MOVESET_FORM, PR_MOVESET_LEVEL_POOL,
    PR_MOVESET_TM_EGG_POOL, PR_MOVESET_ABILITY_STATE, PR_MOVESET_FUSION,
    PR_MOVESET_TERA, PR_MOVESET_CHALLENGE, PR_MOVESET_INVALID,
    PR_MOVESET_REASON_COUNT
} pr_moveset_result;
typedef struct {
    int count;
    uint16_t moves[4];
    int pp[4], pp_up[4];
    bool forced_stab, soft_rejection;
} pr_generated_moveset;
/* Unsupported inputs preserve RNG and output, returning an explicit reason. */
pr_moveset_result pr_run_enemy_moveset(pr_rng *, const pr_moveset_ctx *, pr_generated_moveset *);
const char *pr_moveset_reason(pr_moveset_result);
#endif
