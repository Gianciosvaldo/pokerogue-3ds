/*
 * gc_achv: the custom achievements (no upstream equivalent; custom content, tested by spec tests, never called upstream parity).
 *
 * A list of achievements with a reward each, from one Egg Voucher up to a legendary egg or an egg with a guaranteed tier-3 shiny. Their ids are
 * 0..GC_ACHV_COUNT-1 and index `achievement_flags` of the system data (upstream's own achievements use 192 and up). What they count is in
 * `gc_system.achv`; the app reports events (gc_achv_jojo_won...), then asks gc_achv_evaluate for the achievements that are now met: each is
 * unlocked once and its reward goes to the profile in the same call. A reward that cannot be given yet (the egg list is full) leaves its
 * achievement locked, and a later evaluate gives it.
 */
#ifndef GC_ACHV_H
#define GC_ACHV_H

#include <stdbool.h>
#include <stdint.h>

#include "gc/system.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { GC_ACHV_GROUP_JOJO = 0, GC_ACHV_GROUP_RUNS, GC_ACHV_GROUP_COLLECTION, GC_ACHV_GROUP_COUNT } gc_achv_group;

typedef enum {
    GC_AREW_VOUCHER,      /* arg = VoucherType, n = how many */
    GC_AREW_EGG,          /* arg = EggTier (0..3) */
    GC_AREW_SHINY_EGG,    /* an egg with a guaranteed tier-3 shiny (arg = EggTier of the egg) */
} gc_achv_reward_kind;

typedef struct {
    const char *name;
    const char *desc;
    uint8_t group;        /* gc_achv_group */
    uint8_t reward;       /* gc_achv_reward_kind */
    uint8_t arg, n;
} gc_achv_def;

#define GC_ACHV_COUNT 60
extern const gc_achv_def gc_achv_defs[GC_ACHV_COUNT];

/* The JoJo challengers' bit order: custom-core's cc_jojo_chars. */
enum { GC_JOJO_KARS, GC_JOJO_GYRO, GC_JOJO_DIAVOLO, GC_JOJO_KIRA, GC_JOJO_PUCCI, GC_JOJO_JOLYNE, GC_JOJO_JOSEPH, GC_JOJO_JOTARO, GC_JOJO_DIO,
       GC_JOJO_KOICHI, GC_JOJO_MISTA, GC_JOJO_WAMUU, GC_JOJO_ESIDISI, GC_JOJO_VALENTINE, GC_JOJO_POLNAREFF, GC_JOJO_RISOTTO,
       GC_JOJO_STROHEIM, GC_JOJO_COUNT };

/* A JoJo challenger (index in the order above) was beaten in a Custom run. `run_tag` names the run (any value that is the same for the
 * whole run and differs between runs), `clean` is true when none of the party fainted in the battle. */
void gc_achv_jojo_won(gc_system *sys, int jojo, uint32_t run_tag, bool clean);

/* Group B, the runs: what the app tells at the end of a run. The wave is told whenever a run reaches one (only a new best is kept). A won run
 * is told once, after game-core has counted the Classic win (gc_classic_finish); `feats` are the GC_FEAT_* it was won with. A Custom win also
 * counts as a Classic win (a Custom run is a Classic run with the custom content on). */
enum { GC_FEAT_SIX_UNEVOLVED = 1, GC_FEAT_SOLO = 2, GC_FEAT_NO_BALL = 4 };
void gc_achv_wave_reached(gc_system *sys, uint32_t wave);
void gc_achv_run_won(gc_system *sys, bool custom, uint8_t feats);

/* Group C, the collection, reads the Pokedex of the profile itself (nothing to report): species caught, shinies owned, legendaries and
 * mythicals caught, generations completed. "Every species" and a "completed generation" count the species the game lets the player catch
 * (battle-core's `catchable`), of SpeciesId 1..1025 (the ones a new profile starts with do not count towards "catch N species"); "legendary" is a legendary or a sub-legendary of that range. */

/* A progress line for the screen: how far a count-based achievement is. False for one with no count. */
bool gc_achv_progress_of(const gc_system *sys, int id, uint32_t *have, uint32_t *need);

bool gc_achv_unlocked(const gc_system *sys, int id);

#define GC_ACHV_MAX_NEW 32
typedef struct {
    int n;
    uint8_t id[GC_ACHV_MAX_NEW];
} gc_achv_new;

/* Unlocks every achievement that is met and whose reward could be given, and gives the rewards. `seed_text` (any text) and `now_ms` make a
 * reward egg. `out` may be NULL. Returns how many were unlocked. */
int gc_achv_evaluate(gc_system *sys, const char *seed_text, uint64_t now_ms, gc_achv_new *out);

/* "1 Egg Voucher", "1 Legendary Egg"... (a static string) */
const char *gc_achv_reward_name(const gc_achv_def *d);

#ifdef __cplusplus
}
#endif
#endif
