/* custom-core: fixed limits of the version-1 custom content model (see docs/CUSTOM-CONTENT-CONTRACTS.md).
 *
 * Product defaults come from docs/CUSTOM-CONTENT-ADVANCED-AGENT.md ("Explicit first-release choices"). They are
 * custom policy, not upstream rules. The ID ceilings are the largest values of the pinned upstream enums
 * (8555c08) as generated into battle-core/gen/out/pr_enums.h; tests/layout_test.c fails when a regenerated enum
 * exceeds one of them, so a pin update cannot silently outgrow this file. */
#ifndef CC_LIMITS_H
#define CC_LIMITS_H

#define CC_SCHEMA_VERSION 1u
#define CC_META_MAGIC 0x4D435250u        /* "PRCM" as little-endian bytes */

/* ---- first-release defaults ---- */
#define CC_MAX_NEMESES 16                /* roster records */
#define CC_HISTORY_TURNS 5               /* turns kept for the post-mortem */
#define CC_MAX_EVENTS_PER_TURN 64        /* observed events kept per turn */
#define CC_NUM_WINDOWS 4                 /* rematch windows: waves 27, 77, 127, 177 */
#define CC_MAX_FROZEN CC_MAX_NEMESES     /* candidates frozen into a run's schedule */
#define CC_LEDGER_ENTRIES 8              /* committed transactions remembered for idempotency */

/* ---- record shape ---- */
#define CC_MAX_MOVES 4
#define CC_MAX_LOADOUT_ITEMS 8           /* held items kept for history; more sets CC_NK_LOADOUT_TRUNCATED */
#define CC_ITEM_KEY_BYTES 33             /* modifierTypes key, up to 32 ASCII characters + NUL (longest pinned key: 32) */

/* ---- ID ceilings (inclusive); 0 means "none" wherever a field allows it ---- */
#define CC_SPECIES_ID_MAX 8901           /* SPECIES_ID_BLOODMOON_URSALUNA */
#define CC_MOVE_ID_MAX 920               /* MOVE_ID_NIHIL_LIGHT */
#define CC_ABILITY_ID_MAX 319            /* ABILITY_ID_AURA_GUARD */
#define CC_BIOME_ID_MAX 50               /* BIOME_ID_END */
#define CC_NATURE_MAX 24                 /* NATURE_QUIRKY */
#define CC_STATUS_MAX 7                  /* STATUS_EFFECT_FAINT */
#define CC_HIT_RESULT_MAX 15             /* HIT_RESULT_INDIRECT_KO */
#define CC_BATTLER_MAX 3                 /* BattlerIndex 0..3: player 0/1, enemy 2/3 */

/* ---- storage budgets (docs/CUSTOM-CONTENT-ADVANCED-AGENT.md, "Storage, randomness and transaction requirements") ---- */
#define CC_META_BUDGET_BYTES (64u * 1024u)       /* encoded custom metadata */
#define CC_RUNTIME_BUDGET_BYTES (128u * 1024u)   /* incremental history/roster runtime storage */

#endif
