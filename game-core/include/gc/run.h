/* game-core: the run between the battles. A new run and its starters, each wave's encounter, the
 * battle's commands, victory, experience, level ups, new moves, evolution, money, rewards, biome
 * changes, game over and saving, following upstream's run-level phases (src/phases/: EncounterPhase,
 * CommandPhase, VictoryPhase, ExpPhase, LevelUpPhase, LearnMovePhase, EvolutionPhase, BattleEndPhase,
 * TrainerVictoryPhase, SelectModifierPhase, SelectBiomePhase, NewBattlePhase, GameOverPhase).
 *
 * It calls battle-core for the turns, run-core for the waves and the enemies, and the enemy AI and the
 * rewards through gc/ports.h, where stubs stand in until ai-core and reward-core land. It has no
 * graphics, sound or file access: the app shows the events and screens, and writes the save buffer.
 *
 * How the app drives a run:
 *     gc_run_new(run, mode, seed)                 a new run, waiting on the STARTERS screen
 *     gc_run_set_starters(run, starters, n, &why) runs to the first screen of wave 1
 *     loop:
 *         while (gc_run_next_event(run, &ev))    show each event in order (text, HP bars, exp...)
 *         s = gc_run_screen(run)                  what the player must answer, and the legal choices
 *         gc_run_choose(run, &choice, &why)       the answer; runs on to the next screen
 *     until s->kind is GC_SCREEN_GAME_OVER or GC_SCREEN_CLEAR (then the run is finished).
 * A run can be saved whenever gc_run_can_save() is true (between battles, never mid-turn) and loaded
 * back into the same screen.
 *
 * Everything the app shows can also be read from the run itself (party, money, wave, the battle's
 * field): see gc_run below. The app must not change it. */
#ifndef GC_RUN_H
#define GC_RUN_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "pr/engine.h"   /* battle-core: pr_battle, pr_pokemon, pr_engine */
#include "pr/run.h"      /* run-core: pr_run, pr_biome_depths */
#include "gc/system.h"   /* game-core: gc_system */

#define GC_API_VERSION 1

/* ---------------------------------------------------------------- events */
/* Messages are battle-core's pr_msg (include/pr/text.h): upstream's i18n key ("battle:expGain") and its
 * named arguments, fixed when the message is made, so pr_msg_format gives the same text upstream shows.
 * The turns' own events (moves, damage, faints, switches, their texts) are battle-core's pr_event list
 * (include/pr/events.h), handed over as GC_EV_BATTLE. */
typedef enum {
    GC_EV_MESSAGE,       /* a text box: msg */
    GC_EV_BATTLE,        /* play battle-core's events run->battle_events[a] .. [a + b - 1] (a turn, or the
                          * rest of one after a switch prompt). They stay valid until the next
                          * gc_run_choose */
    GC_EV_WAVE,          /* a new wave: a = wave, b = biome, c = BattleType (the app sets the scene up) */
    GC_EV_BIOME,         /* the run moves to biome a (SwitchBiomePhase) */
    GC_EV_TRAINER,       /* a trainer appears: a = TrainerType, b = TrainerVariant */
    GC_EV_SUMMON,        /* outside a turn, a Pokemon comes onto the field: a = side (0 player, 1 enemy),
                          * b = party index (party[] or enemy[]), c = field slot */
    GC_EV_RECALL,        /* outside a turn, a Pokemon goes back into its ball: a = side, b = party index */
    GC_EV_HP,            /* outside a turn (items, the party heal): a = side, b = party index, c = HP now */
    GC_EV_EXP,           /* a = party index, b = exp gained, c = level before */
    GC_EV_LEVEL_UP,      /* a = party index, b = the new level (stats are already updated) */
    GC_EV_MOVE_LEARNED,  /* a = party index, b = move slot, c = MoveId */
    GC_EV_EVOLVED,       /* a = party index, b = species before, c = species after */
    GC_EV_MONEY,         /* a = the change, b = money now */
    GC_EV_TURN,          /* a turn is about to run: a = turn number (its events follow as GC_EV_BATTLE) */
    GC_EV_MYSTERY,       /* MysteryEncounterSelectPhase: a = MysteryEncounterType, b = tier, c = wave */
    GC_EV_SAVE_POINT,    /* the run has reached upstream's save point (the start of an encounter): a good
                          * moment for the app to call gc_run_save and write the buffer */
} gc_event_kind;

typedef struct {
    uint8_t kind;               /* gc_event_kind */
    int32_t a, b, c;
    pr_msg msg;                 /* GC_EV_MESSAGE */
} gc_event;

/* ---------------------------------------------------------------- screens and choices */
typedef enum {
    GC_SCREEN_NONE,         /* nothing to answer (only while events are pending, or before a run exists) */
    GC_SCREEN_STARTERS,     /* pick the starters: gc_run_set_starters */
    GC_SCREEN_COMMAND,      /* Fight, Ball, Pokemon or Run for field slot `slot` (CommandPhase) */
    GC_SCREEN_SWITCH,       /* pick the party member to send into field slot `slot` (after a faint, or a
                             * move that switches out): GC_CH_PICK */
    GC_SCREEN_LEARN_MOVE,   /* party member `party` wants to learn `move` but knows 4 moves: GC_CH_FORGET */
    GC_SCREEN_EVOLVE,       /* party member `party` is evolving into `species`: GC_CH_EVOLVE or GC_CH_CANCEL */
    GC_SCREEN_REWARD,       /* the reward options after a wave (SelectModifierPhase) */
    GC_SCREEN_BIOME,        /* choose the next biome (the player holds a Map) */
    GC_SCREEN_MYSTERY,      /* Mystery Encounter node selected; Task 46 supplies its action choices */
    GC_SCREEN_GAME_OVER,    /* the party is out of Pokemon: the run is over */
    GC_SCREEN_CLEAR,        /* the run is won (classic wave 200) */
} gc_screen_kind;

typedef enum {
    GC_CH_FIGHT,            /* COMMAND: a = move slot (0-3), b = target BattlerIndex (-1 = the move's own) */
    GC_CH_BALL,             /* COMMAND: a = PokeballType */
    GC_CH_POKEMON,          /* COMMAND: a = party index to switch in */
    GC_CH_RUN,              /* COMMAND */
    GC_CH_PICK,             /* SWITCH: a = party index, or -1 for none when the screen allows it */
    GC_CH_FORGET,           /* LEARN_MOVE: a = the move slot to forget (0-3), or -1 to not learn it */
    GC_CH_EVOLVE,           /* EVOLVE */
    GC_CH_CANCEL,           /* EVOLVE: stop the evolution (upstream's B button); a = 1 also answers yes to
                             * "pause evolutions for this Pokemon?" */
    GC_CH_REWARD,           /* REWARD: a = option index, b = the party index it is used on (-1 = none),
                             * c = the move slot it is used on (-1 = none) */
    GC_CH_SHOP,             /* REWARD: a = shop item index, b = party index, c = move slot */
    GC_CH_REROLL,           /* REWARD */
    GC_CH_SKIP,             /* REWARD: take nothing */
    GC_CH_BIOME,            /* BIOME: a = index into screen.biomes */
    GC_CH_MYSTERY,          /* MYSTERY: Task 45 continuation placeholder; Task 46 replaces with actions */
    GC_CH_CONTINUE,         /* GAME_OVER / CLEAR */
} gc_choice_kind;

typedef struct {
    uint8_t kind;           /* gc_choice_kind */
    int16_t a, b, c;
} gc_choice;

#define GC_MAX_OPTIONS 8
/* One reward on offer. The rewards themselves are reward-core's (gc/ports.h). */
typedef struct {
    const char *key;        /* upstream's modifierTypes key ("POTION", "RARE_CANDY"...) */
    int16_t arg;            /* the item's parameter (a berry, a stat, a type, a move), -1 = none */
    int8_t tier;            /* ModifierTier, for the ball colour */
    uint8_t target;         /* GC_TARGET_*: what it must be used on */
    double cost;            /* shop items: the price; 0 for a free reward */
} gc_offer;
enum { GC_TARGET_NONE, GC_TARGET_POKEMON, GC_TARGET_MOVE };

typedef struct {
    uint8_t kind;           /* gc_screen_kind */
    int8_t slot;            /* COMMAND / SWITCH: the field slot (0 in singles) */
    int8_t party;           /* LEARN_MOVE / EVOLVE: the party index */
    uint16_t move;          /* LEARN_MOVE: the new move */
    uint16_t species;       /* EVOLVE: what it becomes */
    /* COMMAND: which commands are allowed now; a move slot that can't be picked has its reason (an
     * upstream i18n key, e.g. "battle:moveNoPp"; {{moveName}} is the slot's move and
     * {{pokemonNameWithAffix}} the Pokemon) in move_why */
    bool can_ball, can_switch, can_run;
    bool move_ok[4];
    const char *move_why[4];
    bool struggle;          /* every move is unusable: FIGHT with any slot uses Struggle */
    /* SWITCH: which party members can be sent in; can_skip = GC_CH_PICK -1 is allowed */
    bool pick_ok[6];
    bool can_skip;
    /* REWARD */
    gc_offer options[GC_MAX_OPTIONS];
    int n_options;
    gc_offer shop[16];
    int n_shop;
    double reroll_cost;     /* < 0: no reroll */
    /* BIOME */
    uint8_t biomes[PR_MAX_BIOME_LINKS];
    int n_biomes;
    /* MYSTERY */
    int8_t mystery_type;         /* MysteryEncounterType 0..30 */
    int8_t mystery_tier;         /* 0 COMMON, 1 GREAT, 2 ULTRA, 3 ROGUE */
} gc_screen;

/* ---------------------------------------------------------------- the run's state */
/* A Pokemon of the run: battle-core's Pokemon (species, form, level, stats, HP, status, moves and PP,
 * ability, nature, gender, friendship, shiny...) plus what only the run keeps. Party members keep their
 * `b` across battles; its summon and turn data are reset as upstream resets them. */
typedef struct gc_mon {
    pr_pokemon b;
    double exp;                 /* total experience (Pokemon.exp) */
    uint8_t ability_index;      /* 0, 1 or 2 (hidden) */
    uint8_t variant;
    bool pokerus;
    bool pause_evolutions;
    int8_t luck;
    uint8_t pokeball;           /* PokeballType it was caught in */
    int16_t met_wave;           /* -1 for a starter */
    int16_t boss_segments;      /* enemies: 0 = not a boss */
    int16_t boss_segment_index;
    uint8_t trainer_slot;       /* enemies: TrainerSlot */
} gc_mon;

/* The player's items and the enemy's: upstream's modifier list entries, by modifierTypes key */
typedef struct {
    const char *key;            /* "LEFTOVERS", "EXP_SHARE", "BERRY"... (a static string) */
    int16_t arg;                /* BerryType, Stat, PokemonType...; -1 = none */
    int32_t stack;
    uint32_t pokemon_id;        /* held items: the Pokemon's id; 0 = not a held item */
} gc_item;
#define GC_MAX_ITEMS 128

enum gc_mystery_type {
    GC_ME_MYSTERIOUS_CHALLENGERS = 0, GC_ME_MYSTERIOUS_CHEST, GC_ME_DARK_DEAL, GC_ME_FIGHT_OR_FLIGHT,
    GC_ME_SLUMBERING_SNORLAX, GC_ME_TRAINING_SESSION, GC_ME_DEPARTMENT_STORE_SALE,
    GC_ME_SHADY_VITAMIN_DEALER, GC_ME_FIELD_TRIP, GC_ME_SAFARI_ZONE, GC_ME_LOST_AT_SEA,
    GC_ME_FIERY_FALLOUT, GC_ME_THE_STRONG_STUFF, GC_ME_THE_POKEMON_SALESMAN,
    GC_ME_AN_OFFER_YOU_CANT_REFUSE, GC_ME_DELIBIRDY, GC_ME_ABSOLUTE_AVARICE, GC_ME_A_TRAINERS_TEST,
    GC_ME_TRASH_TO_TREASURE, GC_ME_BERRIES_ABOUND, GC_ME_CLOWNING_AROUND, GC_ME_PART_TIMER,
    GC_ME_DANCING_LESSONS, GC_ME_WEIRD_DREAM, GC_ME_THE_WINSTRATE_CHALLENGE,
    GC_ME_TELEPORTING_HIJINKS, GC_ME_BUG_TYPE_SUPERFAN, GC_ME_FUN_AND_GAMES,
    GC_ME_UNCOMMON_BREED, GC_ME_GLOBAL_TRADE_SYSTEM, GC_ME_THE_EXPERT_POKEMON_BREEDER,
    GC_ME_COUNT
};
enum gc_mystery_tier { GC_ME_COMMON = 0, GC_ME_GREAT, GC_ME_ULTRA, GC_ME_ROGUE };

#define GC_MAX_MYSTERY_HISTORY 48
typedef struct {
    uint8_t type;                /* MysteryEncounterType */
    uint8_t tier;                /* gc_mystery_tier */
    uint16_t wave;
} gc_mystery_seen;

/* A starter as the starter select screen hands it over (upstream's Starter) */
typedef struct {
    uint16_t species;
    uint8_t form, ability_index, nature;
    bool female, shiny;
    uint8_t variant;
    bool passive, pokerus;
    int8_t tera_type;           /* -1 = the species' first type */
    uint8_t ivs[6];
    uint16_t moves[4];          /* 0 = empty; all 0 = the default moveset (gc_starter_default_moves) */
} gc_starter;

#define GC_MAX_EVENTS 256
#define GC_MAX_UNPORTED (PR_FEAT_COUNT_ + 16)
#define GC_MAX_BATTLE_EVENTS 384
/* The Trainer.getName calls the game makes (gc_run.trainer_names) */
enum {
    GC_TNAME_TITLED,            /* getName(NONE, true): appeared, defeated */
    GC_TNAME_TRAINER,           /* getName(TRAINER): sends out, comes back */
    GC_TNAME_PARTNER,           /* getName(TRAINER_PARTNER): a double's second Pokemon */
    GC_TNAME_SPEAKER,           /* getName(TRAINER, true): the victory dialogue's speaker */
    GC_TNAME_COUNT_
};

typedef struct gc_run {
    int api_version;
    gc_system *sys;             /* overall system data across runs (NULL = no system updates) */
    pr_run run;                 /* run-core: mode, seed, wave seed, Phaser.Math.RND */
    pr_biome_depths depths;
    int wave, biome;
    int wave_cycle_offset;
    double money;
    int score;
    int trainer_id, secret_id;  /* gameData.trainerId / secretId (shiny checks) */
    uint8_t pokeballs[8];       /* pokeballCounts by PokeballType */
    gc_mon party[6];
    int n_party;
    /* upstream's party order (the party screen's, which switching in battle changes), as indexes into
     * party[]. party[] itself never moves, so an index in an event or a choice always means the same
     * Pokemon */
    int8_t party_order[6];
    gc_item items[GC_MAX_ITEMS];
    int n_items;
    pr_me_save me;              /* mysteryEncounterSaveData spawn state */
    gc_mystery_seen mystery_history[GC_MAX_MYSTERY_HISTORY];
    uint8_t n_mystery_history;
    int8_t mystery_type, mystery_tier; /* selected node; -1 when none is active */
    /* the current battle */
    bool in_battle;
    int battle_type;            /* BattleType */
    int trainer_type, trainer_variant;   /* -1 = none */
    int16_t trainer_name, trainer_partner_name;   /* run-core's rolled locale name indexes, -1 = none */
    /* Trainer.getName as upstream builds it, by GC_TNAME_*: text with $t(key) where upstream translates
     * (i18next's nesting syntax), e.g. "$t(trainerClasses:aceTrainer) $t(trainersCommon:aceTrainer.male.3)"
     * or "$t(titles:gymLeader) $t(trainerNames:brock)". NULL = no trainer */
    const char *trainer_names[GC_TNAME_COUNT_];
    char battle_seed[17];
    gc_mon enemy[6];
    int n_enemy;
    gc_item enemy_items[GC_MAX_ITEMS];
    int n_enemy_items;
    pr_battle bt;               /* battle-core's scene: field, weather, tags, the turn's commands */
    pr_engine eng;
    gc_screen screen;           /* what gc_run_screen returns */
    uint64_t internal_[2048];   /* game-core's own state (the run's phase queue...): opaque */
    /* the event queue */
    gc_event events[GC_MAX_EVENTS];
    int ev_head, ev_count;
    bool ev_overflow;
    /* battle-core's events since the last gc_run_choose (GC_EV_BATTLE points into them) */
    pr_event battle_events[GC_MAX_BATTLE_EVENTS];
    int n_battle_events;
    bool battle_events_overflow;
    /* what the run hit that isn't ported yet: counts by battle-core's unported ids, and game-core's own
     * stand-ins (GC_STUB_*), for the soak runs' report */
    uint32_t unported[GC_MAX_UNPORTED];   /* battle-core's ids, then ai-core's (PR_AI_FEAT_*) */
    uint32_t stubs[16];
    uint32_t run_core_unported[8];      /* run-core's stand-ins, by PR_RUN_UNPORTED_* bit */
    int load_dropped;           /* after gc_run_load: saved values this build couldn't read back (0 normally;
                                 * more when the save comes from a build whose battle structs differ) */
    bool finished;
} gc_run;

/* game-core's own stand-ins, counted in run->stubs */
enum {
    GC_STUB_ENEMY_MOVESET,      /* run-core has no enemy moveset yet: the last 4 level moves */
    GC_STUB_REWARDS,            /* no reward-core yet: a fixed placeholder list */
    GC_STUB_DOUBLE_AS_SINGLE,   /* a double battle roll played as a single battle (M1 rule) */
    GC_STUB_MYSTERY_ENCOUNTER,  /* a mystery encounter roll played as a wild battle */
    GC_STUB_TURN_TAIL,          /* the rest of a turn after a victory was cut short */
    GC_STUB_EVOLUTION,          /* an evolution condition other than level (item, friendship...) not checked */
    GC_STUB_WEATHER,            /* the biome's weather roll isn't ported */
    GC_STUB_UNPORTED_ENEMY,     /* run-core couldn't generate an enemy (a PR_RUN_UNPORTED_* case): a stand-in */
    GC_STUB_CATCH,              /* no catching yet */
    GC_STUB_TRAINER_REWARDS,    /* a trainer's own rewards (modifierRewardFuncs, vouchers) not given */
    GC_STUB_POST_BATTLE,        /* PostBattle abilities (Pickup, Honey Gather), Pay Day money, lapsing items */
    GC_STUB_ENEMY_SWITCH,       /* the AI switched a trainer's Pokemon out: battle-core doesn't run the enemy
                                 * side's SwitchSummonPhase yet, so the enemy does nothing that turn */
    GC_STUB_TRAINER_TITLE,      /* a trainer's name in a message without its title ("Aaron" for "Ace Trainer
                                 * Aaron"): battle-core's messages take one locale key per argument */
    GC_STUB_COUNT_
};
extern const char *const gc_stub_name[GC_STUB_COUNT_];

/* ---------------------------------------------------------------- the calls */
/* A new run in `mode` (GameModes; only CLASSIC is supported for now) from `seed` (upstream's 24-char
 * run seed; NULL = a new random one from `entropy`). Waits on the STARTERS screen. */
void gc_run_new(gc_run *run, int mode, const char *seed, uint32_t entropy);

/* The starter select screen's result: validates the cost budget (gc_starter_budget) and each starter,
 * builds the party, and runs to the first screen. false (and *why, an i18n key or a plain reason) if
 * the list isn't allowed. */
bool gc_run_set_starters(gc_run *run, const gc_starter *starters, int n, const char **why);

/* The next event to show, oldest first; false when there are none left. */
bool gc_run_next_event(gc_run *run, gc_event *out);

/* What the player must answer now. */
const gc_screen *gc_run_screen(const gc_run *run);

/* The player's answer to the current screen. Runs on to the next screen, queuing events. false (and
 * *why) if the choice isn't legal on this screen; nothing changes then. */
bool gc_run_choose(gc_run *run, const gc_choice *c, const char **why);

/* Saving, at upstream's save point: from GC_EV_SAVE_POINT (the start of an encounter) until the battle's
 * first turn runs, while its first COMMAND screen waits (gc_run_can_save). The buffer holds everything
 * needed to come back to that screen with the run as it was; gc_run_save returns the bytes written (0 =
 * not allowed now, or the buffer is too small; GC_SAVE_MAX always fits). gc_run_load replaces the whole
 * run and queues the scene's events (WAVE, TRAINER, SUMMON) before the COMMAND screen; false (and *why)
 * for a damaged save or one from a newer format, and the run is left empty then. A save made by another
 * build loads as far as its fields still match (see load_dropped). */
#define GC_SAVE_MAX (128 * 1024)
bool gc_run_can_save(const gc_run *run);
size_t gc_run_save(const gc_run *run, void *buf, size_t cap);
bool gc_run_load(gc_run *run, const void *buf, size_t len, const char **why);

/* ---- starters (upstream's starter select rules) ---- */
int gc_starter_budget(int mode);                       /* the cost limit: 10 in classic */
int gc_starter_cost(int species);                      /* speciesStarterCosts, -1 = not a starter */
/* The default moveset: the first four level 1-5 moves (getStarterMoves). Returns how many. */
int gc_starter_default_moves(int species, uint16_t out[4]);
extern const uint16_t gc_default_starters[];           /* defaultStarterSpecies (unlocked in a new save) */
extern const int gc_default_starter_count;

/* ---- helpers the app may use ---- */
const gc_mon *gc_field_mon(const gc_run *run, int side);   /* the Pokemon on the field (singles), or NULL */
double gc_exp_to_next_level(const gc_mon *m);             /* for the EXP bar: exp still needed, 0 at the cap */
double gc_exp_level_fraction(const gc_mon *m);            /* 0..1 of the way through the current level */

#endif
