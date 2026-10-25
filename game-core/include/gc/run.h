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
    GC_EV_BATTLE,        /* play battle-core's events run->battle_events[a] .. [a + b - 1] (a turn, the
                          * rest of one after a switch prompt or after a victory's experience, the start of
                          * a battle after the "Will you switch?" answer). They stay valid until the next
                          * gc_run_choose */
    GC_EV_WAVE,          /* a new wave: a = wave, b = biome, c = BattleType (the app sets the scene up) */
    GC_EV_BIOME,         /* the run moves to biome a (SwitchBiomePhase) */
    GC_EV_TRAINER,       /* a trainer appears: a = TrainerType, b = TrainerVariant */
    GC_EV_SUMMON,        /* outside a turn, a Pokemon comes onto the field: a = side (0 player, 1 enemy),
                          * b = party index (party[] or enemy[]), c = field slot */
    GC_EV_RECALL,        /* outside a turn, a Pokemon goes back into its ball: a = side, b = party index */
    GC_EV_HP,            /* outside a turn (items, the party heal): a = side, b = party index, c = HP now */
    GC_EV_EXP,           /* a = party index, c = level before; see exp_amount/exp_before */
    GC_EV_LEVEL_UP,      /* a = party index, b = the new level (stats are already updated), c = the level before */
    GC_EV_MOVE_LEARNED,  /* a = party index, b = move slot, c = MoveId */
    GC_EV_EVOLVED,       /* a = party index, b = species before, c = species after */
    GC_EV_MONEY,         /* money_change / money_after; a/b truncate in-range values, otherwise zero */
    GC_EV_TURN,          /* a turn is about to run: a = turn number (its events follow as GC_EV_BATTLE) */
    GC_EV_MYSTERY,       /* MysteryEncounterSelectPhase: a = MysteryEncounterType, b = tier, c = wave */
    GC_EV_SAVE_POINT,    /* the run has reached upstream's save point (the start of an encounter): a good
                          * moment for the app to call gc_run_save and write the buffer */
    GC_EV_EGG_HATCH,     /* presentation only: a species, b form, c gender; appearance below */
    GC_EV_DIALOGUE,      /* a trainer speaks (TrainerConfig dialogue): a = TrainerType, b = TrainerVariant, c = 0 before the fight
                          * (EncounterPhase, after the trainer slides in and before "X would like to battle!"), 1 after it is won
                          * (TrainerVictoryPhase, after "X was defeated!"). msg.key is the line's locale key; the text keeps its
                          * @c{expression} markers, @d{delay} and "$" page breaks. The app shows it with the trainer's name and,
                          * where gc_trainer_config.has_char_sprite, the trainer's portrait */
    GC_EV_COUNT,
} gc_event_kind;

typedef struct {
    uint8_t kind;               /* gc_event_kind */
    int32_t a, b, c;
    pr_msg msg;                 /* GC_EV_MESSAGE */
    /* GC_EV_EXP only: exact native award and the pre-award display state.
     * b mirrors the amount only when it fits int32, otherwise zero. Runtime
     * events are not part of the session save format. */
    double exp_amount, exp_before;
    double money_change, money_after; /* GC_EV_MONEY only: native Numbers, never narrowed */
    bool prompt;               /* GC_EV_MESSAGE: require confirmation */
    uint8_t egg_tier, egg_variant;
    bool egg_shiny, egg_move_rare;
    uint8_t egg_nature, egg_ivs[6], egg_previous_ivs[6];
    uint16_t egg_move;
} gc_event;

/* ---------------------------------------------------------------- screens and choices */
typedef enum {
    GC_SCREEN_NONE,         /* nothing to answer (only while events are pending, or before a run exists) */
    GC_SCREEN_STARTERS,     /* pick the starters: gc_run_set_starters */
    GC_SCREEN_COMMAND,      /* Fight, Ball, Pokemon or Run for field slot `slot` (CommandPhase) */
    GC_SCREEN_SWITCH,       /* pick the party member to send into field slot `slot` (after a faint, or a
                             * move that switches out): GC_CH_PICK. After a catch with six in the party it is
                             * the release prompt instead (events battle:partyFull, menu:swapOrRelease):
                             * pick the member to release, or -1 = No (can_skip) */
    GC_SCREEN_LEARN_MOVE,   /* party member `party` wants to learn `move` but knows 4 moves: GC_CH_FORGET */
    GC_SCREEN_EVOLVE,       /* party member `party` is evolving into `species`: GC_CH_EVOLVE or GC_CH_CANCEL */
    GC_SCREEN_REWARD,       /* the reward options after a wave (SelectModifierPhase) */
    GC_SCREEN_BIOME,        /* choose the next biome (the player holds a Map) */
    GC_SCREEN_MYSTERY,      /* Mystery Encounter node selected; Task 46 supplies its action choices */
    GC_SCREEN_GAME_OVER,    /* the party is out of Pokemon: the run is over */
    GC_SCREEN_CLEAR,        /* the run is won (classic wave 200) */
    GC_SCREEN_CHECK_SWITCH, /* CheckSwitchPhase: "Will you switch Pokemon?" at the start of a wild battle,
                             * only when run->ask_switch is set: GC_CH_CONFIRM */
    GC_SCREEN_CUSTOM_OFFER, /* custom content (gc/custom.h): the app's hook offered a rematch before the battle's save point:
                             * GC_CH_CONFIRM, a = 1 accept, 0 decline. Only with hooks installed; never saved (no save point yet) */
    GC_SCREEN_TARGET,       /* choose one of target_ok; GC_CH_TARGET or GC_CH_CANCEL */
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
    GC_CH_LOCK_RARITIES,    /* REWARD: switch the Lock Capsule's lock on or off (toggleRerollLock; needs the capsule) */
    GC_CH_CONFIRM,          /* CHECK_SWITCH: a = 1 yes (the party screen follows), 0 no; CUSTOM_OFFER: a = 1 accept, 0 decline */
    GC_CH_TRANSFER,         /* REWARD: Manage Items (openModifierTransferScreen): a = the party index that gets the item,
                             * b = the held item's index in run->items, c = how many copies; the screen stays open */
    GC_CH_DISCARD,          /* REWARD: Manage Items' discard: b = the held item's index in run->items, c = how many copies */
    GC_CH_TARGET,           /* TARGET: a = target BattlerIndex 0..3 */
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
    int8_t upgrade_count;   /* ModifierTypeOption.upgradeCount: how many tiers the luck upgrade raised it */
    bool is_purchase;       /* set by SHOP even at price 0; free offers use cost 0 or -1 */
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
    bool can_ball, can_switch, can_run, can_undo;
    bool fight_only;        /* a mystery encounter skips to the FIGHT menu (skipToFightInput): the menu cannot be backed out of */
    bool target_ok[4];       /* TARGET: legal single-target choices */
    bool move_ok[4];
    const char *move_why[4];
    bool struggle;          /* every move is unusable: FIGHT with any slot uses Struggle */
    /* SWITCH: which party members can be sent in; can_skip = GC_CH_PICK -1 is allowed */
    bool pick_ok[6];
    bool can_skip;
    bool revival_blessing;     /* SWITCH picker selects fainted party members to revive */
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
    bool option_ok[8];
    /* what the encounter's screen needs to word its texts and ask for its picks (gc_mystery_setup_screen): the {{tokens}} the intro, option
     * labels and tooltips use (only the encounters ported with their texts fill them), and for each option what the player picks first:
     * GC_PICK_NONE, a party member (GC_CH_MYSTERY.b) or a party member and then one of its moves (b and c) */
    pr_msg_arg me_args[8];
    int n_me_args;
    uint8_t option_pick[8];
} gc_screen;
enum { GC_PICK_NONE = 0, GC_PICK_MEMBER = 1, GC_PICK_MEMBER_MOVE = 2 };


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
    uint8_t trainer_slot;       /* enemies: TrainerSlot */
    int32_t met_level;          /* Pokemon.metLevel: its level when it joined (a catch sets it again) */
    int8_t met_biome;           /* Pokemon.metBiome (BiomeId), -1 for a starter */
    uint16_t met_species;       /* Pokemon.metSpecies */
    uint16_t used_tms[64];      /* TMs used on this Pokemon (up to 64) */
    uint8_t n_used_tms;
    /* b.nature is getNature() for battle/stat callers. Before a Mint it is also
     * the base nature; old saves default has_custom_nature to false. */
    uint8_t base_nature;
    bool has_custom_nature;
} gc_mon;

/* The player's items and the enemy's: upstream's modifier list entries, by modifierTypes key */
typedef struct {
    const char *key;            /* "LEFTOVERS", "EXP_SHARE", "BERRY"... (a static string) */
    int16_t arg;                /* BerryType, Stat, PokemonType...; -1 = none */
    int32_t stack;
    uint32_t pokemon_id;        /* stable holder ID; 0 maps to wildcard -1 */
    bool detached;             /* battle loot owner 0, distinct from wildcard -1 */
    int32_t virtual_stack;     /* transient battle state; excluded from saves */
    int32_t battle_count;      /* canonical temporary booster lifetime; old saves default 0 */
    bool non_transferable;    /* explicit held-item lock, e.g. final-boss Mini Black Hole */
    uint8_t type_tier;         /* cached ModifierType.tier + 1; 0 absent */
    uint32_t type_ref;         /* transient shared ModifierType reference, excluded from saves */
    uint16_t loot_ref;         /* transient detached-stock identity, excluded from saves */
    bool eviolite_base;        /* transferred Eviolite inherits the base stat-booster clone */
    bool form_inactive;        /* form items default active; legacy saves therefore default false */
} gc_item;
#define GC_MAX_ITEMS (PR_MAX_MODIFIERS + 32) /* staging/validation headroom; battle/reward limit remains explicit */

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

/* Read-only FaintPhase provenance for the current battle, copied before the
 * observer's pointers expire. Residual damage never inherits an earlier hit. */
typedef struct {
    bool observed, direct;
    uint8_t remaining, source_side, form;
    uint16_t species, move;
    int wave, turn;
} gc_faint_record;

typedef struct gc_run {
    int api_version;
    gc_faint_record last_faint[2]; /* most recent actual faint per side; not saved mid-battle */
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
    bool double_battle;         /* current encounter width, retained across save/load */
    /* the wild battle a mystery encounter starts (battle_type stays WILD here): MysteryEncounter.catchAllowed
     * decides whether a ball may be thrown at it */
    bool me_battle, me_catch_allowed;
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
    uint8_t terminal_result; /* 0 unprocessed, 1 defeat, 2 awarded Classic clear */
    /* settings.general.battleStyle: false = SET (the default here), the wild battle's start doesn't ask "Will
     * you switch Pokemon?"; true = SWITCH (upstream's own default), it opens GC_SCREEN_CHECK_SWITCH. A setting
     * of the app's, not run state: it isn't saved and the app sets it again after gc_run_load. Off until the
     * app has the screen */
    bool ask_switch;
} gc_run;

/* Whether the encounter's party screen accepts the member for the option (a member that cannot be picked is shown but not selectable). */
bool gc_mystery_member_ok(const gc_run *run, int option, int idx);

/* game-core's own stand-ins, counted in run->stubs */
enum {
    GC_STUB_ENEMY_MOVESET,      /* run-core has no enemy moveset yet: the last 4 level moves */
    GC_STUB_REWARDS,            /* reward-core couldn't roll a reward screen (it flagged it): the screen is empty */
    GC_STUB_DOUBLE_AS_SINGLE,   /* a double battle roll played as a single battle (M1 rule) */
    GC_STUB_MYSTERY_ENCOUNTER,  /* a mystery encounter roll played as a wild battle */
    GC_STUB_EVOLUTION,          /* an evolution condition other than level (item, friendship...) not checked */
    GC_STUB_UNPORTED_ENEMY,     /* run-core couldn't generate an enemy (a PR_RUN_UNPORTED_* case): a stand-in */
    GC_STUB_TRAINER_REWARDS,    /* a trainer's own rewards (modifierRewardFuncs, voucher) that reward-core has no row for: not given */
    GC_STUB_POST_BATTLE,        /* PostBattle abilities (Pickup, Honey Gather), Pay Day money, lapsing items */
    GC_STUB_TRAINER_TITLE,      /* a trainer's name in a message without its title ("Aaron" for "Ace Trainer
                                 * Aaron"): battle-core's messages take one locale key per argument */
    GC_STUB_REWARD_INPUT,       /* a reward-core input game-core can't give: an item it doesn't know, a learnset too long */
    GC_STUB_REWARD_APPLY,       /* a reward taken without its own effect (not a healing, revive, PP or PP Up item: reward-core applies those) */
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
 * *why) if the choice isn't legal on this screen. Vitamin purchase exception:
 * pinned upstream may add the item/update stats, then report an error with an
 * empty reason and no payment. That applied purchase also returns false. */
bool gc_run_choose(gc_run *run, const gc_choice *c, const char **why);
/* Full localized arguments, when available, for the last refused choice.
 * Cleared on the next choice; NULL when the reason is plain text/key only. */
const pr_msg *gc_run_refusal_message(const gc_run *run);

/* Saving, at upstream's save point: from GC_EV_SAVE_POINT (the start of an encounter) until the battle's
 * first turn runs, while its first COMMAND screen waits (gc_run_can_save). The buffer holds everything
 * needed to come back to that screen with the run as it was; gc_run_save returns the bytes written (0 =
 * not allowed now, or the buffer is too small; GC_SAVE_MAX always fits). gc_run_load replaces the whole
 * run and queues the scene's events (WAVE, TRAINER, SUMMON) before the COMMAND screen; false (and *why)
 * for a damaged save or one from a newer format, and the run is left empty then. A save made by another
 * build loads as far as its fields still match (see load_dropped). */
#define GC_SAVE_MAX (512 * 1024)
bool gc_run_can_save(const gc_run *run);
size_t gc_run_save(const gc_run *run, void *buf, size_t cap);
bool gc_run_load(gc_run *run, const void *buf, size_t len, const char **why);

/* ---- starters (upstream's starter select rules) ---- */
/* TM party labels: 0 not able, 1 able, 2 known in the effective moveset.
 * Compatibility uses raw species/form plus fusion, including while transformed. */
int gc_mon_tm_state(const gc_mon *mon, int move);
/* TmModifierType.selectFilter separately excludes permanent known moves. */
bool gc_mon_can_learn_tm(const gc_mon *mon, int move);

/* held items: getMaxStackCount() of `key`/`arg` held by `mon`, and the reward screen's transfer (GC_CH_TRANSFER does it;
 * returns the copies moved, 0 = refused) */
int gc_item_max_held(const gc_run *run, const char *key, int16_t arg, const gc_mon *mon);
/* Source party form-item list, as global inventory indices in source order.
 * Returns the total eligible count, writing at most capacity entries. */
int gc_form_item_options(const gc_run *run, int party, int *indices, int capacity);
/* Toggle one offered global inventory index while SelectModifierPhase waits. */
bool gc_form_item_toggle(gc_run *run, int party, int item, const char **why);
int gc_item_transfer(gc_run *run, int item_idx, int to, int qty);
int gc_item_discard(gc_run *run, int item_idx, int count);

int gc_party_luck(gc_run *run); /* actual allowed living party, clamped by native rules */
int gc_starter_budget(int mode);                       /* the cost limit: 10 in classic */
int gc_starter_cost(int species);                      /* base speciesStarterCosts, -1 = not a starter */
void gc_pokerus_starters(uint64_t unix_ms,uint16_t out[3]);
double gc_starter_value(const gc_system *sys, int species); /* cost with value reduction (game-data.ts:2018) */
bool gc_starter_is_available(const gc_system *sys, const gc_starter *starter, double *cost_out, const char **why);
/* The default moveset: the first four level 1-5 moves (getStarterMoves). Returns how many. */
int gc_starter_default_moves(int species, uint16_t out[4]);
/* Unique native starting-level moves for this form, then unlocked egg moves.
 * Returns -1 if invalid or the caller's buffer is too small; never truncates. */
int gc_starter_moves(const gc_system *sys, int species, int form, uint16_t *out, int cap);
extern const uint16_t gc_default_starters[];           /* defaultStarterSpecies (unlocked in a new save) */
extern const int gc_default_starter_count;

/* ---- helpers the app may use ---- */
/* The line the trainer of this battle says (TrainerConfig dialogue): `kind` 0 = getEncounterMessages, 1 = getVictoryMessages. Upstream picks
 * randSeedItem(messages) under executeWithSeedOffset(wave), so the choice is the wave's and costs the run no roll. The locale key (static
 * storage), or NULL when the trainer says nothing */
const char *gc_trainer_dialogue_key(const gc_run *run, int kind);
const gc_mon *gc_field_mon(const gc_run *run, int side);   /* the Pokemon on the field (singles), or NULL */
double gc_exp_to_next_level(const gc_mon *m);             /* for the EXP bar: exp still needed, 0 at the cap */
double gc_exp_level_fraction(const gc_mon *m);            /* 0..1 of the way through the current level */

#endif
