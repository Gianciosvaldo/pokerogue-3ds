/* custom-core: primitive data contracts of the version-1 custom content model.
 *
 * Plain fixed-width data: no pointers, no callbacks, no bitfields, no native enums as storage, nothing that depends
 * on the address of a live game object. The structs below are the *in-memory* contract. The persisted layout is
 * defined field by field in docs/CUSTOM-CONTENT-CONTRACTS.md (never fwrite/memcpy these structs: padding and
 * alignment are not part of the format).
 *
 * Three kinds of data stay distinct:
 *   runtime observations  cc_obs                  what the screen layer saw; never stored
 *   persistent history    cc_event / cc_turn / cc_history / cc_terminal / cc_nemesis / cc_meta
 *   run-bound policy      cc_schedule             frozen at run start, belongs to one run
 *
 * Optional values carry an explicit "known" bit. A field whose bit is clear MUST be zero, so there is one
 * canonical encoding of "unknown" and an absent value can never be mistaken for a measured 0.
 * Nothing here calls the game. No function in this module writes a file, draws, or draws a gameplay random number. */
#ifndef CC_TYPES_H
#define CC_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cc/limits.h"

/* ---- identifiers (0 is never a valid identifier) ---- */
typedef uint64_t cc_run_uid;     /* one run observation, durable across restarts; the run seed is NOT one (seeds repeat) */
typedef uint64_t cc_txn_id;      /* committed custom-state transaction, strictly increasing */
typedef uint32_t cc_record_id;   /* roster record, strictly increasing, never reused */

enum { CC_SIDE_PLAYER = 0, CC_SIDE_ENEMY = 1 };

/* ======================================================================== runtime observation (not stored) */

enum {
    CC_OBS_NONE = 0,
    CC_OBS_DAMAGE,       /* PR_EV_DAMAGE: id = HitResult, value = the shown damage number (>= 0) */
    CC_OBS_HP,           /* PR_EV_HP: value > 0 a shown heal, value < 0 status/indirect HP loss */
    CC_OBS_FAINT,        /* PR_EV_FAINT */
    CC_OBS_SWITCH_OUT,   /* PR_EV_SWITCH_OUT */
    CC_OBS_SWITCH_IN,    /* PR_EV_SWITCH_IN */
    CC_OBS_MOVE_ANIM,    /* PR_EV_MOVE_ANIM: id = MoveId, battler = user, target = target. NOT proof of a later effect */
    CC_OBS_KIND_COUNT_
};

/* One observation as the adapter hands it to the reducer. `battler`/`target` are field POSITIONS; they identify
 * nobody across events. battler_id/target_id are stable Pokemon ids and are 0 unless the adapter derived them
 * independently (never from a field index alone). */
typedef struct {
    uint8_t kind;                   /* CC_OBS_* */
    int8_t battler;                 /* BattlerIndex 0..3, -1 none */
    int8_t target;                  /* BattlerIndex 0..3, -1 none */
    uint8_t battler_form, target_form;
    uint16_t battler_species, target_species;   /* from the event's field snapshot; 0 = nobody there */
    uint32_t battler_id, target_id;             /* 0 = not derived */
    uint16_t id;                    /* kind specific, see above */
    int32_t value;                  /* kind specific, see above */
    int32_t hp_after, hp_max_after; /* `battler` in the event's field snapshot; -1 = no snapshot */
    uint8_t status_after;           /* StatusEffect in that snapshot, 0 none */
} cc_obs;

/* ======================================================================== persistent history */

/* A Pokemon a record names. id is Pokemon.id, stable inside one run only. */
typedef struct {
    uint32_t id;                    /* != 0 */
    uint16_t species;               /* 1..CC_SPECIES_ID_MAX */
    uint8_t form;
    uint8_t side;                   /* CC_SIDE_* */
    uint8_t level;                  /* 1..255 */
} cc_actor;

enum {
    CC_EV_NONE = 0,                 /* not a valid stored kind */
    CC_EV_DAMAGE,                   /* a shown hit on the target */
    CC_EV_HEAL,                     /* a shown heal */
    CC_EV_HP_LOSS,                  /* HP lost without a shown hit (status, weather, recoil...): never attributed */
    CC_EV_FAINT,
    CC_EV_SWITCH_OUT,
    CC_EV_SWITCH_IN,
    CC_EV_STATUS,                   /* the target's status changed; shown = the new StatusEffect */
    CC_EV_KIND_COUNT_
};

/* cc_event.known: which optional fields carry a value */
enum {
    CC_EK_TARGET_ID     = 1u << 0,  /* target_id */
    CC_EK_SOURCE_SIDE   = 1u << 1,  /* source_side */
    CC_EK_SOURCE_ID     = 1u << 2,  /* source_id (needs SOURCE_SIDE) */
    CC_EK_SOURCE_SPECIES= 1u << 3,  /* source_species + source_form (needs SOURCE_SIDE) */
    CC_EK_MOVE          = 1u << 4,  /* move_id */
    CC_EK_RESULT        = 1u << 5,  /* result (HitResult) */
    CC_EK_SHOWN         = 1u << 6,  /* shown: the number the game displayed (a magnitude, >= 0) */
    CC_EK_HP_DELTA      = 1u << 7,  /* hp_delta: measured from consecutive snapshots (damage < 0, heal > 0) */
    CC_EK_HP_AFTER      = 1u << 8,  /* hp_after */
    CC_EK_ALL           = (1u << 9) - 1u
};

/* shown and hp_delta are separate observations. They are never added together. */
typedef struct {
    uint32_t seq;                   /* strictly increasing observation order across the whole history */
    uint8_t kind;                   /* CC_EV_*, not NONE */
    uint8_t target_side;            /* CC_SIDE_*, always known */
    uint8_t source_side;            /* iff CC_EK_SOURCE_SIDE */
    uint8_t target_form;
    uint8_t source_form;            /* iff CC_EK_SOURCE_SPECIES */
    uint8_t result;                 /* iff CC_EK_RESULT, 0..CC_HIT_RESULT_MAX */
    uint16_t known;                 /* CC_EK_* */
    uint16_t target_species;        /* always known, 1..CC_SPECIES_ID_MAX */
    uint16_t source_species;        /* iff CC_EK_SOURCE_SPECIES */
    uint16_t move_id;               /* iff CC_EK_MOVE, 1..CC_MOVE_ID_MAX */
    uint32_t target_id;             /* iff CC_EK_TARGET_ID, != 0 */
    uint32_t source_id;             /* iff CC_EK_SOURCE_ID, != 0 */
    int32_t shown;                  /* iff CC_EK_SHOWN */
    int32_t hp_delta;               /* iff CC_EK_HP_DELTA */
    int32_t hp_after;               /* iff CC_EK_HP_AFTER, >= 0 */
} cc_event;

/* cc_turn.flags. A turn with flags == 0 is a complete record of everything the observer was given for it. */
enum {
    CC_TF_EVICTED     = 1u << 0,    /* more than CC_MAX_EVENTS_PER_TURN events: the oldest were dropped (dropped > 0) */
    CC_TF_SOURCE_LOSS = 1u << 1,    /* the game's own event buffer overflowed before the observer saw this turn */
    CC_TF_BEGAN_MID   = 1u << 2,    /* observation started after this turn had begun */
    CC_TF_ALL         = (1u << 3) - 1u
};

typedef struct {
    uint32_t battle_seq;            /* >= 1: counts battles within the run; turns of different battles never merge */
    uint16_t wave;                  /* >= 1 */
    uint16_t turn;                  /* the battle's turn counter; 0 = events before the first turn */
    uint16_t dropped;               /* events evicted from this turn (saturates at 65535) */
    uint8_t flags;                  /* CC_TF_* */
    uint8_t n_events;               /* 0..CC_MAX_EVENTS_PER_TURN */
    cc_event events[CC_MAX_EVENTS_PER_TURN];   /* oldest first */
} cc_turn;

enum {
    CC_HF_BEGAN_MID = 1u << 0,      /* observation of the run started after its first battle (a resumed save) */
    CC_HF_ALL       = 1u
};

typedef struct {
    uint8_t n_turns;                /* 0..CC_HISTORY_TURNS, oldest first */
    uint8_t flags;                  /* CC_HF_* */
    cc_turn turns[CC_HISTORY_TURNS];
} cc_history;

/* ======================================================================== terminal summary */

enum {
    CC_OUT_NONE = 0,
    CC_OUT_DEFEAT,                  /* the party was wiped out: the only outcome that may create a Nemesis */
    CC_OUT_CLEAR,
    CC_OUT_ABANDON,                 /* no such flow exists in the app yet */
    CC_OUT_RESET,                   /* no such flow exists in the app yet */
    CC_OUT_COUNT_
};

enum {
    CC_TK_FINISH_MOVE  = 1u << 0,   /* finish_move */
    CC_TK_FINISH_ACTOR = 1u << 1,   /* finish_actor: the Pokemon that dealt the finishing blow, reliably established */
    CC_TK_FALLEN_ACTOR = 1u << 2,   /* fallen_actor: the player's Pokemon that fell last */
    CC_TK_ALL          = 7u
};

typedef struct {
    cc_run_uid run_uid;             /* != 0 */
    cc_txn_id txn_id;               /* 0 = a candidate not yet committed; != 0 once stored */
    uint8_t outcome;                /* CC_OUT_* */
    uint8_t known;                  /* CC_TK_* */
    uint8_t biome;                  /* 0..CC_BIOME_ID_MAX */
    uint16_t wave;                  /* >= 1 */
    uint16_t finish_move;           /* iff CC_TK_FINISH_MOVE */
    cc_actor finish_actor;          /* iff CC_TK_FINISH_ACTOR (all-zero otherwise) */
    cc_actor fallen_actor;          /* iff CC_TK_FALLEN_ACTOR (all-zero otherwise) */
} cc_terminal;

/* ======================================================================== Nemesis record */

enum {
    CC_LIFE_NONE = 0,
    CC_LIFE_STORED,                 /* remembered, not part of any run's plan */
    CC_LIFE_SCHEDULED,              /* frozen into one run's schedule */
    CC_LIFE_CHALLENGED,             /* the rematch was accepted */
    CC_LIFE_DEFEATED,               /* the rematch was won; the reward is not yet claimed */
    CC_LIFE_REWARD_CLAIMED,         /* the one reward was committed; terminal */
    CC_LIFE_COUNT_
};

/* Traits of the enemy as it was. v1 stores only ordinary wild enemies, so a stored record has traits == 0; a
 * candidate with a trait set is refused (cc_nemesis_support) instead of being silently reshaped. */
enum {
    CC_TR_BOSS          = 1u << 0,
    CC_TR_FUSED         = 1u << 1,
    CC_TR_TERA          = 1u << 2,
    CC_TR_DOUBLES       = 1u << 3,
    CC_TR_TRAINER_OWNED = 1u << 4,
    CC_TR_MYSTERY       = 1u << 5,
    CC_TR_FIXED_SCRIPTED= 1u << 6,
    CC_TR_REMATCH       = 1u << 7,  /* itself a custom rematch enemy: a defeat by it never creates a record */
    CC_TR_ALL           = 0xFFu
};

/* cc_nemesis.known */
enum {
    CC_NK_ABILITY          = 1u << 0,   /* ability (1..CC_ABILITY_ID_MAX) */
    CC_NK_PASSIVE          = 1u << 1,   /* passive (0 = known to have none) */
    CC_NK_MOVES            = 1u << 2,   /* n_moves + moves */
    CC_NK_IVS              = 1u << 3,   /* ivs */
    CC_NK_NATURE           = 1u << 4,   /* nature */
    CC_NK_LOADOUT          = 1u << 5,   /* n_items + items (n_items 0 = known to hold nothing) */
    CC_NK_LOADOUT_TRUNCATED= 1u << 6,   /* more items were held than CC_MAX_LOADOUT_ITEMS (needs LOADOUT, n_items full) */
    CC_NK_FINISH_MOVE      = 1u << 7,   /* finish_move */
    CC_NK_FALLEN           = 1u << 8,   /* fallen_species + fallen_form */
    CC_NK_FINISHER_VERIFIED= 1u << 9,   /* this exact enemy was reliably established as the finishing source */
    CC_NK_ALL              = (1u << 10) - 1u
};

typedef struct {
    char key[CC_ITEM_KEY_BYTES];    /* upstream modifierTypes key, [A-Z0-9_]+, NUL padded to the end */
    int16_t arg;                    /* the item's parameter, -1 none */
    uint16_t stack;                 /* >= 1 */
} cc_item;

/* Historical record. Every value is what the enemy WAS (orig_level, ivs, items...). None of it is a combat template:
 * a later rematch builds the enemy through the game's own construction path at the ordinary wave level
 * (docs/CUSTOM-CONTENT-CONTRACTS.md, "Identity versus combat template"). */
typedef struct {
    cc_record_id record_id;         /* != 0, strictly increasing along the roster */
    uint8_t life;                   /* CC_LIFE_* */
    uint8_t traits;                 /* CC_TR_* */
    uint16_t known;                 /* CC_NK_* */
    /* origin */
    cc_run_uid source_run_uid;      /* != 0 */
    uint32_t source_pokemon_id;     /* != 0: Pokemon.id inside that run; (source_run_uid, source_pokemon_id) is unique */
    uint16_t source_wave;           /* >= 1 */
    uint8_t source_biome;           /* 0..CC_BIOME_ID_MAX */
    /* original identity */
    uint16_t species;               /* 1..CC_SPECIES_ID_MAX */
    uint8_t form;
    int8_t gender;                  /* -1 genderless, 0 male, 1 female */
    uint8_t shiny;                  /* 0 or 1 */
    uint8_t orig_level;             /* 1..255, as it was */
    uint8_t nature;                 /* iff CC_NK_NATURE */
    uint16_t ability;               /* iff CC_NK_ABILITY */
    uint16_t passive;               /* iff CC_NK_PASSIVE (0 = none) */
    uint8_t ivs[6];                 /* iff CC_NK_IVS, each 0..31 */
    uint8_t n_moves;                /* iff CC_NK_MOVES, 1..CC_MAX_MOVES */
    uint16_t moves[CC_MAX_MOVES];   /* distinct, nonzero up to n_moves, zero after */
    uint8_t n_items;                /* iff CC_NK_LOADOUT, 0..CC_MAX_LOADOUT_ITEMS */
    cc_item items[CC_MAX_LOADOUT_ITEMS];
    /* defeat summary */
    uint16_t finish_move;           /* iff CC_NK_FINISH_MOVE */
    uint16_t fallen_species;        /* iff CC_NK_FALLEN */
    uint8_t fallen_form;            /* iff CC_NK_FALLEN */
    /* lifecycle bookkeeping, consistent with `life` (cc_nemesis_validate) */
    cc_run_uid sched_run_uid;       /* the run that scheduled/challenged it */
    uint16_t challenge_wave;        /* the window wave that was accepted */
    cc_txn_id reward_txn;           /* the committed reward transaction, 0 until REWARD_CLAIMED */
} cc_nemesis;

typedef struct {
    uint8_t count;                  /* 0..CC_MAX_NEMESES; oldest first (record_id ascending) */
    cc_nemesis rec[CC_MAX_NEMESES];
} cc_roster;

/* ---- why a candidate is not stored (cc_nemesis_support, cc_insertion_check) ---- */
enum {
    CC_SUPPORT_OK = 0,
    CC_REFUSE_NOT_DEFEAT,           /* clear, abandon, reset */
    CC_REFUSE_NOT_ATTRIBUTED,       /* the finishing source is not reliably this enemy */
    CC_REFUSE_BOSS,
    CC_REFUSE_FUSED,
    CC_REFUSE_TERA,
    CC_REFUSE_DOUBLES,
    CC_REFUSE_TRAINER_OWNED,
    CC_REFUSE_MYSTERY,
    CC_REFUSE_FIXED_SCRIPTED,
    CC_REFUSE_REMATCH,
    CC_REFUSE_ABILITY_UNKNOWN,
    CC_REFUSE_MOVES_UNKNOWN,
    CC_REFUSE_COUNT_
};

/* ======================================================================== run-bound schedule */

enum {
    CC_SCHED_NONE = 0,              /* not a valid stored state */
    CC_SCHED_NO_CANDIDATE,          /* the frozen roster was empty: nothing to offer this run */
    CC_SCHED_ACTIVE,                /* one record chosen; windows still to come */
    CC_SCHED_OFFERED,               /* the player is being asked */
    CC_SCHED_DECLINED,              /* consumed: nothing more this run */
    CC_SCHED_ACCEPTED,              /* the rematch was taken (its result lives in the record's lifecycle) */
    CC_SCHED_EXHAUSTED,             /* all four windows were skipped: nothing this run */
    CC_SCHED_COUNT_
};

enum { CC_WIN_PENDING = 0, CC_WIN_SKIPPED, CC_WIN_OFFERED, CC_WIN_DECLINED, CC_WIN_ACCEPTED, CC_WIN_COUNT_ };

/* Why a window was skipped: the encounter at that wave was not an ordinary, nonfinal, supported wild one. */
enum {
    CC_SKIP_NONE = 0,
    CC_SKIP_TRAINER,
    CC_SKIP_FIXED_OR_SCRIPTED,
    CC_SKIP_MYSTERY,
    CC_SKIP_BOSS,
    CC_SKIP_FINAL,
    CC_SKIP_UNSUPPORTED,
    CC_SKIP_COUNT_
};

typedef struct {
    uint16_t wave;                  /* exactly cc_window_waves[i] */
    uint8_t state;                  /* CC_WIN_* */
    uint8_t reason;                 /* CC_SKIP_*: != 0 iff state == CC_WIN_SKIPPED */
} cc_window;

enum { CC_RNG_ALGO_NONE = 0, CC_RNG_ALGO_V1 = 1 /* reserved for the algorithm C05 documents */ };

typedef struct {
    uint8_t state;                  /* CC_SCHED_* */
    uint8_t rng_algo;               /* CC_RNG_ALGO_*: V1 iff a record was chosen */
    uint8_t n_frozen;               /* 0..CC_MAX_FROZEN: the roster as it stood when the run started */
    uint8_t n_windows;              /* 0, or CC_NUM_WINDOWS iff a record was chosen */
    cc_run_uid run_uid;             /* != 0: the run this schedule belongs to */
    uint64_t rng_seed;              /* the custom policy stream, separate from gameplay and gacha RNG */
    uint64_t rng_state;             /* its state after the single draw: a reload never draws again */
    cc_record_id chosen_id;         /* 0 = none */
    cc_record_id frozen_ids[CC_MAX_FROZEN];   /* strictly ascending, nonzero */
    cc_window windows[CC_NUM_WINDOWS];
} cc_schedule;

extern const uint16_t cc_window_waves[CC_NUM_WINDOWS];   /* 27, 77, 127, 177 */

/* ======================================================================== identifiers and transactions */

typedef struct {
    uint32_t next_record_id;        /* >= 1 */
    uint32_t next_run_seq;          /* >= 1: the low half of the next run UID */
    uint32_t install_nonce;         /* the high half of every run UID; chosen once by the adapter */
    cc_txn_id next_txn_id;          /* >= 1 */
} cc_counters;

enum { CC_TXN_NONE = 0, CC_TXN_TERMINAL, CC_TXN_REWARD_CLAIM, CC_TXN_KIND_COUNT_ };
enum { CC_TXO_NONE = 0, CC_TXO_INSERTED, CC_TXO_REPORT_ONLY, CC_TXO_CLAIMED, CC_TXO_COUNT_ };

typedef struct {
    cc_txn_id txn_id;               /* strictly increasing along the ledger */
    cc_run_uid run_uid;             /* != 0 */
    cc_record_id record_id;         /* the record inserted / claimed; 0 for REPORT_ONLY */
    uint8_t kind;                   /* CC_TXN_* */
    uint8_t outcome;                /* CC_TXO_* */
} cc_ledger_entry;

typedef struct {
    uint8_t n;                      /* 0..CC_LEDGER_ENTRIES, oldest first */
    cc_ledger_entry e[CC_LEDGER_ENTRIES];
} cc_ledger;

/* ======================================================================== the persisted custom metadata */

/* Everything the custom metadata file holds (one blob, one envelope). The run-bound cc_schedule is NOT in it: it
 * lives in the custom run overlay C10 defines. */
typedef struct {
    uint32_t schema;                /* CC_SCHEMA_VERSION */
    cc_counters counters;
    cc_ledger ledger;
    cc_roster roster;
    uint8_t has_report;             /* 0 or 1 */
    cc_terminal report;             /* the most recent post-mortem, iff has_report */
    cc_history history;             /* its last turns, iff has_report */
} cc_meta;

#endif
