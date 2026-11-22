/* custom-core: the JoJo trainers of a custom run (docs/CUSTOM-CONTENT-ADVANCED-AGENT.md, custom content; lorenzo 2026-10-18..20).
 *
 * Pure like the rest of the module: no globals except the constant table, no I/O, no allocation, no clock, no ambient random numbers.
 * The module only DECIDES, as data: which character (if any) an ordinary random trainer wave becomes, and what that character's team is.
 * Nothing here builds a Pokemon: the adapter (game-core's gc/custom.h trainer hook and the app) acts, through the game's own paths.
 *
 *   cc_jojo_start(state, seed)         at the start of the run: the stream starts from a seed the caller hands in (kept apart from
 *                                      gameplay RNG, the gacha and the rematch stream)
 *   cc_jojo_roll(state, biome)         at each eligible wave: -1, or the index in cc_jojo_chars of the character the wave becomes
 *
 * Rules (lorenzo, 2026-10-18 15:10 and 15:15): each eligible wave rolls a chance of CC_JOJO_P_PERCENT; a hit picks one of the characters
 * whose biome it is that were not dealt yet this run; no limit per run besides that (one fight per character, at most one
 * character per wave). The Pillar Men (biome -1) are never dealt here: only their own mystery encounter uses them. What an eligible wave is
 * (an ordinary random single trainer wave, not a boss, the final wave, a rival, a Gym or Elite Four trainer) is the adapter's call. */
#ifndef CC_JOJO_H
#define CC_JOJO_H

#include <stdint.h>

#include "cc/model.h"

#define CC_JOJO_P_PERCENT 30u   /* chance per eligible wave that has an undealt character in its biome (25 until the Bizarre mode, lorenzo 2026-11-15: "slightly higher") */
#define CC_JOJO_MAX_CHARS 64   /* the roll state is a 64-bit mask of dealt characters: the table can never have more */
#define CC_JOJO_MAX_MEMBERS 6
#define CC_JOJO_MAX_ITEMS 5
#define CC_JOJO_NONE (-1)
#define CC_JOJO_MAX_MORE_BIOMES 3   /* biomes a character is in besides `biome` (Ghiaccio: every ice and snow biome, lorenzo 2026-11-22) */

typedef struct {
    const char *key;            /* a modifierTypes key as game-core's item tables spell it ("LEFTOVERS", "ATTACK_TYPE_BOOSTER") */
    int16_t arg;                /* its argument (a type, a stat), -1 when it has none */
    uint8_t stack;
} cc_jojo_item;

typedef struct {
    uint16_t species;           /* SpeciesId */
    uint8_t form;               /* form index */
    uint8_t ability_slot;       /* 0, 1: the form's abilities; 2: the hidden one */
    uint8_t shiny;              /* 0/1 */
    uint8_t variant;            /* shiny tier 0..2 (0 for a species with no variant palettes) */
    uint16_t moves[4];          /* MoveId, 0 = empty slot (a Ditto has only Transform) */
    uint8_t n_items;
    cc_jojo_item items[CC_JOJO_MAX_ITEMS];
} cc_jojo_member;

/* cc_jojo_char.passive: Pucci's Made in Heaven (not an AbilityId; lorenzo, 2026-11-16). No passive at first; his first faint is the
 * C-Moon stage (a new look, no effect), his second wakes Made in Heaven: Speed +1 at the start of every turn for his Pokemon on the
 * field, up to +6. game-core's GC_CUSTOM_PASSIVE_MADE_IN_HEAVEN, battle-core's PR_CUSTOM_PASSIVE_MADE_IN_HEAVEN. */
#define CC_JOJO_PASSIVE_MADE_IN_HEAVEN (-2)
/* cc_jojo_char.passive: DIO's The World (not an AbilityId; lorenzo, 2026-11-16). Time stops on one to two turns a battle in which the
 * player picked a move: his Pokemon acts, the player's cannot. game-core's GC_CUSTOM_PASSIVE_THE_WORLD, battle-core's
 * PR_CUSTOM_PASSIVE_THE_WORLD. */
#define CC_JOJO_PASSIVE_THE_WORLD (-3)
/* cc_jojo_char.passive: Kars's Perfect Being (not an AbilityId; lorenzo, 2026-11-16). After his fifth Pokemon is knocked out he becomes
 * the Ultimate Life Form: his last Pokemon takes nothing from physical moves and half from special ones. game-core's
 * GC_CUSTOM_PASSIVE_PERFECT_BEING, battle-core's PR_CUSTOM_PASSIVE_PERFECT_BEING. */
#define CC_JOJO_PASSIVE_PERFECT_BEING (-4)

typedef struct {
    const char *id;             /* "kira": the sprite is images/trainer/custom/<id> */
    const char *name;           /* "Yoshikage Kira" */
    const char *line;           /* the iconic line */
    int16_t biome;              /* BiomeId, -1 = none (the Pillar Men) */
    int16_t passive;            /* an AbilityId the whole team has as its passive, -1 = none, CC_JOJO_PASSIVE_MADE_IN_HEAVEN = Pucci's, CC_JOJO_PASSIVE_THE_WORLD = DIO's, CC_JOJO_PASSIVE_PERFECT_BEING = Kars's */
    uint8_t n_members;          /* 1..CC_JOJO_MAX_MEMBERS */
    cc_jojo_member member[CC_JOJO_MAX_MEMBERS];
    /* After the members so a row that leaves them out has none (zero): */
    uint8_t n_more_biomes;      /* 0..CC_JOJO_MAX_MORE_BIOMES: the character is dealt in these biomes too (a biome of -1 has none) */
    int16_t more_biomes[CC_JOJO_MAX_MORE_BIOMES];
    uint8_t doubles;            /* 1: a double battle, the members sent out in pairs (Prosciutto & Pesci, lorenzo 2026-11-22); 0: a single one */
} cc_jojo_char;

/* Whether character `c` is dealt in `biome` (its `biome` or one of its more_biomes; never for a biome below 0). */
int cc_jojo_in_biome(const cc_jojo_char *c, int biome);

extern const cc_jojo_char cc_jojo_chars[];
extern const unsigned cc_jojo_count;

/* The roll state of one run. Small and plain, so the adapter can persist it as is. */
typedef struct {
    uint64_t rng_state;         /* the JoJo stream (cc_rng_v1: SplitMix64) */
    uint64_t used;              /* bit i: character i was dealt this run (at most CC_JOJO_MAX_CHARS characters) */
    uint8_t fights;             /* characters dealt this run (the number of bits in `used`) */
} cc_jojo_state;

/* Starts a run's state in *s (every byte is written). The stream is the custom stream of algorithm 1 started from `seed` mixed with a
 * constant of this module, so it never repeats the rematch stream's numbers when both are given the same seed. */
void cc_jojo_start(cc_jojo_state *s, uint64_t seed);

/* Structural check of a state: `used` names only characters of the table, `fights` is the number of them. */
cc_status cc_jojo_validate(const cc_jojo_state *s);

/* One eligible wave of `biome`. Returns CC_JOJO_NONE or a character index (it is then marked dealt). Draws nothing when no undealt character
 * belongs to the biome; otherwise one draw for the chance and, on a hit, one for the pick among the
 * undealt characters of the biome in table order. A NULL or invalid state returns CC_JOJO_NONE and changes nothing. */
int cc_jojo_roll(cc_jojo_state *s, int biome);

/* The same two on a table the caller passes (count at most CC_JOJO_MAX_CHARS): the tests use small tables with several characters in one biome. */
cc_status cc_jojo_validate_in(const cc_jojo_state *s, unsigned count);
int cc_jojo_roll_in(cc_jojo_state *s, const cc_jojo_char *chars, unsigned count, int biome);

/* The index of the character with this id, or CC_JOJO_NONE. */
int cc_jojo_find(const char *id);

#endif
