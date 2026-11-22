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

typedef enum { GC_ACHV_GROUP_JOJO = 0, GC_ACHV_GROUP_RUNS, GC_ACHV_GROUP_COLLECTION, GC_ACHV_GROUP_EGGS, GC_ACHV_GROUP_TRAINERS, GC_ACHV_GROUP_FOOTBALL, GC_ACHV_GROUP_COUNT } gc_achv_group;

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

#define GC_ACHV_COUNT 162   /* 0..99 as first made, the 17 "Defeat X" of the second batch, Twenty Faces, the 34 "Flawless X", D'Arby's two, then the third
                              * batch's "Defeat X" and "Flawless X" in pairs (ids stay under 192, where upstream's own start) */
#define GC_ACHV_TWENTY_FACES 117
#define GC_ACHV_DARBY 152   /* beating D'Arby the Gambler (Standard or All-In: a won hand or a caught cheat) */
#define GC_ACHV_DARBY_REMATCH 153   /* beating D'Arby's rematch, the battle for the sealed souls (lorenzo 2026-11-18) */
extern const gc_achv_def gc_achv_defs[GC_ACHV_COUNT];

/* The JoJo challengers' bit order: custom-core's cc_jojo_chars. */
enum { GC_JOJO_KARS, GC_JOJO_GYRO, GC_JOJO_DIAVOLO, GC_JOJO_KIRA, GC_JOJO_PUCCI, GC_JOJO_JOLYNE, GC_JOJO_JOSEPH, GC_JOJO_JOTARO, GC_JOJO_DIO,
       GC_JOJO_KOICHI, GC_JOJO_MISTA, GC_JOJO_WAMUU, GC_JOJO_ESIDISI, GC_JOJO_VALENTINE, GC_JOJO_POLNAREFF, GC_JOJO_RISOTTO,
       GC_JOJO_STROHEIM,
       /* the second batch (2026-11-15) */
       GC_JOJO_CAESAR, GC_JOJO_KAKYOIN, GC_JOJO_AVDOL, GC_JOJO_VANILLAICE, GC_JOJO_HOLHORSE, GC_JOJO_IGGY, GC_JOJO_NARANCIA, GC_JOJO_BRUNO,
       GC_JOJO_ABBACCHIO, GC_JOJO_WEATHER, GC_JOJO_JOHNNY, GC_JOJO_SANDMAN, GC_JOJO_DIEGO, GC_JOJO_SHIGECHI, GC_JOJO_JOSUKE, GC_JOJO_ROHAN,
       GC_JOJO_GIORNO,
       /* the third batch (lorenzo 2026-11-22) */
       GC_JOJO_GHIACCIO, GC_JOJO_PROSCIUTTO, GC_JOJO_JODIO, GC_JOJO_JONATHAN, GC_JOJO_COUNT };

/* The achievement of beating one challenger ("Defeat X"): ids 0..16 for the first 17 (the table index), then GC_ACHV_JOJO2_FIRST + (index - 17)
 * for the second batch, because the ids of the others (the sets, the runs, ...) were taken before it existed and a profile keeps them by id;
 * the third batch on (GC_JOJO_GHIACCIO..) takes GC_ACHV_JOJO3_FIRST + 2 * (index - GC_JOJO_GHIACCIO), its "Flawless X" the id after it.
 * -1 for an index outside the table. */
#define GC_ACHV_JOJO2_FIRST 100
#define GC_ACHV_JOJO3_FIRST 154
int gc_achv_jojo_id(int jojo);

/* The achievement of beating one challenger with none of your Pokemon fainting in that battle ("Flawless X"): GC_ACHV_FLAWLESS_FIRST + the table
 * index for the first 34, the id after the "Defeat X" for the third batch on (-1 for an index outside the table). */
#define GC_ACHV_FLAWLESS_FIRST 118
int gc_achv_flawless_id(int jojo);

/* The order of the Achievements list: the id shown at list position `pos` (0..GC_ACHV_COUNT-1), -1 outside it. The ids are the saved identity of an
 * achievement and cannot move; the list can. It starts with every "Defeat X" in table order (the first 17 and the second batch together), then
 * every "Flawless X" in table order, then the other JoJo ones (the Part sets, the counts with Twenty Faces between Ten Faces and Every Face, the feats), then every other group in id order. */
int gc_achv_order(int pos);

/* A JoJo challenger (index in the order above) was beaten in a Bizarre run. `run_tag` names the run (any value that is the same for the
 * whole run and differs between runs), `clean` is true when none of the party fainted in the battle. */
void gc_achv_jojo_won(gc_system *sys, int jojo, uint32_t run_tag, bool clean);

/* Group B, the runs: what the app tells at the end of a run. The wave is told whenever a run reaches one (only a new best is kept). A won run
 * is told once, after game-core has counted the Classic win (gc_classic_finish); `feats` are the GC_FEAT_* it was won with. A Bizarre win also
 * counts as a Classic win (a Classic (Bizarre) run is a Classic run with the JoJo trainers and the Pillar Men on). `bizarre` is true for that
 * mode only: a plain Classic win counts for the Classic achievements alone (its counter keeps the old name custom_wins, which saves hold). */
enum { GC_FEAT_SIX_UNEVOLVED = 1, GC_FEAT_SOLO = 2, GC_FEAT_NO_BALL = 4 };
void gc_achv_wave_reached(gc_system *sys, uint32_t wave);
/* D'Arby the Gambler was beaten (a won hand, or his cheat caught), at either wager. */
void gc_achv_darby_won(gc_system *sys);
void gc_achv_darby_rematch_won(gc_system *sys);
void gc_achv_run_won(gc_system *sys, bool bizarre, uint8_t feats);

/* Group C, the collection, reads the Pokedex of the profile itself (nothing to report): species caught, shinies owned, legendaries and
 * mythicals caught, generations completed. "Every species" and a "completed generation" count the species the game lets the player catch
 * (battle-core's `catchable`), of SpeciesId 1..1025 (the ones a new profile starts with do not count towards "catch N species"); "legendary" is a legendary or a sub-legendary of that range. */

/* Group D, the eggs: game-core's hatching (gc_system_hatch_ready_eggs) reports each hatch here itself; hatches are counted in stats.total_hatches.
 * `tier` is the egg's EggTier (0 common .. 3 legendary). */
enum { GC_HATCH_EPIC = 1, GC_HATCH_LEGENDARY = 2, GC_HATCH_SHINY = 4 };
void gc_achv_egg_hatched(gc_system *sys, int tier, bool shiny);

/* Group E, the trainers, reads the profile itself (nothing to report): the first win against a Gym Leader, an Elite Four member or a Champion
 * sets voucherUnlocks[trainer type] (TrainerVictoryPhase's validateVoucher, game-core's trainer_victory, checked against upstream by
 * trainer_victory_diff_test), and that set is what these count. A profile that beat them before the achievements existed gets them at once.
 * A "region" is the set of trainers a Classic run can meet in that region: its Gym Leaders (none in Alola and Blueberry), its four Elite Four
 * members and its Champion; where upstream picks one of two (Hala/Molayne, Red/Lance, Steven/Wallace...) "every" asks for both, which takes
 * more than one run. A run meets one region's Elite Four and Champion (run-core's pr_run_fixed_e4_trainer), so a region's table is checked
 * against it by achv_test. */
enum { GC_REGION_KANTO, GC_REGION_JOHTO, GC_REGION_HOENN, GC_REGION_SINNOH, GC_REGION_UNOVA, GC_REGION_KALOS, GC_REGION_ALOLA, GC_REGION_GALAR,
       GC_REGION_PALDEA, GC_REGION_BLUEBERRY, GC_REGION_COUNT };
/* The first achievement of the region list: its id is GC_ACHV_REGION_FIRST + GC_REGION_*, and GC_ACHV_GRAND_TOUR follows the last. */
#define GC_ACHV_REGION_FIRST 80
#define GC_ACHV_GRAND_TOUR (GC_ACHV_REGION_FIRST + GC_REGION_COUNT)
/* The trainer types (TrainerType values) of a region: gym[] lists the Gym Leaders (0-terminated, empty in Alola and Blueberry), elite[] the
 * Elite Four members (4 to 6, 0-terminated) and champion[] the Champions (1 or 2, 0-terminated). False for a bad region. */
typedef struct {
    const char *name;
    uint16_t gym_first, gym_last;   /* TrainerType range of its Gym Leaders; 0, 0 = none */
    uint16_t elite[7];
    uint16_t champion[3];
} gc_region;
const gc_region *gc_achv_region(int region);

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

/* Group F, football: the playoff tells the profile the matches it won (football-core's football_career_flush_achievements, at the checkpoint
 * that records them). Each is a win; `title` is the seventh round's, `shutout` a win with nothing conceded, `blowout` a win by
 * GC_FB_BLOWOUT_MARGIN points or more. Losses count for nothing. Ids 91..99: matches won 1/10/50/200, titles 1/3/10, shutout, blowout. */
#define GC_FB_BLOWOUT_MARGIN 21u
void gc_achv_football_won(gc_system *sys, unsigned wins, unsigned titles, unsigned shutouts, unsigned blowouts);

/* "1 Egg Voucher", "1 Legendary Egg"... (a static string) */
const char *gc_achv_reward_name(const gc_achv_def *d);

#ifdef __cplusplus
}
#endif
#endif
