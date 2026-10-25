/* game-core: the optional hooks of the custom content (task C10; docs/CUSTOM-CONTENT-CONTRACTS.md, section 2.16).
 *
 * Disabled by default: with no hooks installed (gc_run_set_custom(run, NULL), the default) game-core runs exactly as it did before, bit for
 * bit (tests/custom_hooks_test.c compares seeded runs with and without the observer installed). The hooks never change what game-core would
 * decide on its own; the only decision they can take is the one the player answers on GC_SCREEN_CUSTOM_OFFER, and that decision replaces the
 * identity of the one enemy of an ordinary wild encounter (see gc_custom_enemy), nothing else.
 *
 * The hooks run inside game-core's calls (gc_run_choose, gc_run_set_starters, gc_run_load...). They must not call back into game-core and must
 * not keep the pointers they are given. Like run->ask_switch they are a setting of the app's, not run state: they are not saved, and the app
 * installs them again after gc_run_load.
 *
 * What game-core adds, in the order a wave happens:
 *   encounter()   once per wave, when the wave's encounter has been generated (the ordinary enemy, its stats and items are final) and before
 *                 the battle's save point. It says what the encounter is. Returning true asks the player: the run stops on
 *                 GC_SCREEN_CUSTOM_OFFER (GC_CH_CONFIRM, a = 1 accept, 0 decline); events up to the wave's GC_EV_WAVE have been queued, the
 *                 save point has not. No save is possible while the offer waits.
 *   answer()      the player's answer. For an accept it fills `spec`; game-core builds that enemy in place of the ordinary one (through its
 *                 own construction, stat, item and AI paths, at the ordinary wave level) and then calls built(). A decline changes nothing.
 *   faint()       every faint of the battle, with the attacker when battle-core's own state names it (pr_faint_note).
 *   trainer()     before the wave's encounter is final, on an ordinary random single trainer wave (not a double, a boss, the final wave, a
 *                 fixed rival, Gym or Elite Four battle, nor Classic's wave 20): the app may hand over a team that replaces the ordinary
 *                 party (see gc_custom_team). The ordinary trainer type stays (money, AI, rewards); the name is the team's own plain text.
 */
#ifndef GC_CUSTOM_H
#define GC_CUSTOM_H
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct gc_run;

/* What kind of encounter the wave is. The order of the values is not an API: use the names. */
enum {
    GC_ENC_ORDINARY_WILD = 0,
    GC_ENC_TRAINER,
    GC_ENC_FIXED_OR_SCRIPTED,    /* run-core could not generate the battle (a fixed battle or closure not ported): game-core stood in with a wild one */
    GC_ENC_MYSTERY,
    GC_ENC_BOSS,
    GC_ENC_FINAL,                /* the final wave of the mode */
    GC_ENC_UNSUPPORTED,
};
enum {
    GC_ENF_FUSED = 1u << 0,      /* the wild enemy is a fusion */
    GC_ENF_TERA = 1u << 1,       /* it is terastallized */
    GC_ENF_DOUBLES = 1u << 2,    /* the roll was a double battle (game-core plays it as a single battle) */
};

typedef struct {
    int wave;
    int biome;
    uint8_t kind;                /* GC_ENC_* */
    uint8_t flags;               /* GC_ENF_*: only meaningful for an ordinary wild encounter */
    int level;                   /* the ordinary enemy's level (0 when there is no enemy yet: a mystery encounter) */
    uint16_t species;            /* the ordinary enemy's species (0 when none) */
} gc_custom_encounter;

#define GC_CUSTOM_MAX_MOVES 4
#define GC_CUSTOM_MAX_ITEMS 8

/* The identity an accepted rematch gives the one enemy. Applied: species, form, gender, shiny, nature (when has_nature), ability, moves.
 * Never applied: the level (the wave's own), IVs, held items and everything else, which game-core's own paths produce. */
typedef struct {
    uint16_t species;
    uint8_t form;
    int8_t gender;               /* -1 genderless, 0 male, 1 female */
    bool shiny;
    bool has_nature;
    uint8_t nature;
    uint16_t ability;            /* an ability id: it must be one of the form's three abilities; its index is then derived, never guessed */
    uint8_t n_moves;             /* 1..4, each a valid move id */
    uint16_t moves[GC_CUSTOM_MAX_MOVES];
} gc_custom_enemy;

#define GC_CUSTOM_MAX_TEAM 6
#define GC_CUSTOM_MAX_TEAM_ITEMS 5

typedef struct {
    const char *key;             /* a modifierTypes key as game-core's item tables spell it; static storage (pr_item_names entry or equal text) */
    int16_t arg;                 /* -1 when the item has none */
    uint8_t stack;               /* 1..99 */
} gc_custom_team_item;

typedef struct {
    uint16_t species;
    uint8_t form;
    uint8_t ability_slot;        /* 0, 1: the form's abilities; 2: the hidden one */
    bool shiny;
    uint8_t variant;             /* shiny tier 0..2 */
    uint16_t moves[GC_CUSTOM_MAX_MOVES];   /* a valid move id each, 0 = empty slot (at least one) */
    uint8_t n_items;
    gc_custom_team_item items[GC_CUSTOM_MAX_TEAM_ITEMS];
} gc_custom_team_member;

/* A trainer's team in place of the ordinary party. Built through game-core's own construction, stat, passive, item and AI paths: the levels
 * are the ordinary party's (the team's last members take the highest ones), the IVs, nature and ids come from the ordinary party's members,
 * so a team draws nothing from the run's random numbers; the gender follows the id; the held items are the team's own, the wave-scaled
 * ones are not given to its members. */
typedef struct {
    const char *name;            /* the trainer's name as plain text, at most 47 characters; copied */
    int16_t passive;             /* an AbilityId every member has as its passive, -1 = none */
    uint8_t n;                   /* 1..GC_CUSTOM_MAX_TEAM */
    gc_custom_team_member member[GC_CUSTOM_MAX_TEAM];
} gc_custom_team;

/* A Pokemon of a faint, by value (nothing points into the run). */
typedef struct {
    uint32_t id;                 /* Pokemon.id */
    uint16_t species;
    uint8_t form;
    uint16_t start_species;      /* an enemy: its species and form when the battle began (they differ after a battle form change); the player's: = species */
    uint8_t start_form;
    int8_t gender;
    bool shiny;
    uint8_t level;
    uint8_t nature;
    uint16_t ability;            /* the Pokemon's own ability (not one a move or an ability swapped in mid-battle) */
    uint16_t passive;            /* 0 = none */
    uint8_t ivs[6];
    uint8_t n_moves;             /* the permanent moveset (not a Transform's copy) */
    uint16_t moves[GC_CUSTOM_MAX_MOVES];
    bool is_boss, fused, tera, trainer_owned;
    uint8_t n_items;             /* the held items game-core tracks for it (an enemy's), up to GC_CUSTOM_MAX_ITEMS */
    struct { const char *key; int16_t arg; uint16_t stack; } items[GC_CUSTOM_MAX_ITEMS];   /* key: a static string */
} gc_custom_actor;

typedef struct {
    int wave, turn;
    uint8_t side;                /* the side that fainted: 0 the player's, 1 the enemy's */
    uint8_t remaining;           /* Pokemon of that side with HP left (battle-core's pr_faint_note.remaining) */
    bool direct;                 /* battle-core's own state names the attacker (pr_faint_note.direct): the others are not attributed */
    uint16_t move;               /* direct: the move of the hit */
    gc_custom_actor fainted;
    bool has_source;             /* direct, and the attacker is still in its party (it can be on the bench by now) */
    uint8_t source_side;         /* has_source: the side the attacker belongs to (0 the player's, 1 the enemy's); a hit on itself has the fainted one's side */
    gc_custom_actor source;
} gc_custom_faint;

typedef struct gc_custom_hooks {
    void *user;
    /* true = ask the player. Called once per wave (also when the wave is a trainer, a mystery encounter or a boss, so the caller can account for it). */
    bool (*encounter)(void *user, const struct gc_run *run, const gc_custom_encounter *enc);
    /* The answer. Accepted: fill `spec` and return true to have it built (false = nothing is built: the encounter stays ordinary). */
    bool (*answer)(void *user, const struct gc_run *run, bool accepted, gc_custom_enemy *spec);
    /* After an accepted answer: the enemy was built (ok) or game-core refused the spec (why, and the ordinary enemy stays). */
    void (*built)(void *user, const struct gc_run *run, bool ok, const char *why);
    void (*faint)(void *user, const struct gc_run *run, const gc_custom_faint *note);
    /* An eligible trainer wave (see above): true = `team` (zeroed by the caller) is filled and replaces the ordinary party. A team that fails
     * gc_custom_team_check leaves the ordinary party. */
    bool (*trainer)(void *user, const struct gc_run *run, const gc_custom_encounter *enc, gc_custom_team *team);
} gc_custom_hooks;

/* Installs (or, with NULL, removes) the hooks. The pointer is kept: it must outlive the run. Safe at any time, also in the middle of a battle
 * (the faint observer is installed into the battle at once). */
void gc_run_set_custom(struct gc_run *run, const gc_custom_hooks *hooks);

/* Whether game-core can build this enemy, without building it: species, form, gender, ability (one of the form's three), nature, moves.
 * *why (may be NULL) says what is wrong. Pure. */
bool gc_custom_enemy_check(const gc_custom_enemy *spec, const char **why);

/* Whether game-core can build this team: sizes, species, form, ability slot, shiny tier, moves, item keys and stacks. Pure. */
bool gc_custom_team_check(const gc_custom_team *team, const char **why);

/* Gives the current trainer battle a plain-text name (all of the trainer's message names), or, with NULL or "", takes it away. A battle
 * built from a team has it from the start; the app calls this after gc_run_load for a saved battle of a team. No effect outside a trainer battle. */
void gc_custom_set_trainer_name(struct gc_run *run, const char *name);

#ifdef __cplusplus
}
#endif
#endif
