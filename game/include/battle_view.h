/*
 * battle_view: the battle screen. Upstream's battle field (arena, Pokemon sprites), the info
 * boxes (src/ui/battle-info/), the message window and the command and fight menus
 * (battle-message-, command- and fight-ui-handler.ts), playing back a list of battle events.
 *
 * The battle logic lives elsewhere (battle-core, driven by the run loop). The caller:
 *   1. bv_begin(): sets up both sides and the arena, shows the screen (ui_set).
 *   2. bv_play(): queues events (messages, damage, faints, summons...); the screen plays them in
 *      order, each one waiting for its animation or message, then calls on_done.
 *   3. bv_ask_command(): shows "What will X do?" with the command and fight menus and calls
 *      back with the player's choice.
 * Events use this header's own bv_event; battle_events.h maps battle-core's events onto them.
 */
#ifndef GM_BATTLE_VIEW_H
#define GM_BATTLE_VIEW_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BV_PLAYER 0 /* battler index 0 = player's first, 2 = enemy's first (upstream BattlerIndex) */
#define BV_ENEMY 2

/* One Pokemon as the screen shows it. */
typedef struct {
  int species, form;         /* SpeciesId, form index */
  bool female, shiny;
  int variant;               /* 0..2 */
  char name[40];             /* the name to render (nickname or species name) */
  int level;
  int hp, max_hp;
  int status;                /* StatusEffect (0 = none) */
  int gender;                /* Gender: -1 genderless, 0 male, 1 female */
  int types[3], n_types;     /* PokemonType */
  float exp_ratio;           /* 0..1 of the level's EXP bar */
  int moves[4], pp_left[4], pp_max[4], n_moves;
  bool boss;
  int boss_segments;
  int8_t stat_stages[7];     /* ATK DEF SPATK SPDEF SPD ACC EVA */
  bool caught_before;        /* the "owned" ball icon on the enemy's box */
  bool substitute;           /* a Substitute doll stands in front of it (set by the screen as the
                              * doll's animations play; battle_events.c compares it with
                              * battle-core's) */
  bool hidden;               /* out of sight in the air, underground or underwater (upstream's
                              * pokemon.setVisible(false) from a SemiInvulnerableTag); the info
                              * box stays */
  float sprite_scale;        /* getSpriteScale, which uses permanent form keys even after Transform */
} bv_mon;

typedef enum {
  BEV_MESSAGE,     /* text, like a MessagePhase: a = callbackDelay ms (0 = upstream's default:
                    * 1500, or none with a prompt), prompt = wait for a button */
  BEV_INTRO_WILD,  /* the encounter: arenas slide in, then the message. Wild (a = 0, the enemy battler's
                    * Pokemon is on the field): the enemy untints, cries, its info box slides in, and the
                    * message is "a wild X appeared". Trainer (a = 1): nobody on the enemy side yet, and the
                    * message is `text` ("X would like to battle!") */
  BEV_SUMMON,      /* battler sent out (mon = its data); player: text, or "Go! X!" when NULL */
  BEV_WITHDRAW,    /* battler returns to its ball */
  BEV_MOVE_ANIM,   /* an animation (battle_anim.h): b = ba_kind (move, common, charge), a = its id
                    * (MoveId, CommonAnim, ChargeAnim), battler = user, target (-1 = none) */
  BEV_DAMAGE,      /* hit flash and HP bar: battler, a = new hp, b = HitResult (upstream) */
  BEV_HEAL,        /* HP bar up: battler, a = new hp */
  BEV_STATUS,      /* status icon: battler, a = StatusEffect */
  BEV_STAT_STAGES, /* battler, a = stat (1..7, Stat enum), b = stages (+/-) */
  BEV_ABILITY,     /* ability bar: battler, a = AbilityId, b = passive */
  BEV_FAINT,       /* battler faints: cry, sprite drops, info box leaves */
  BEV_EXP,         /* battler (player), a = new exp ratio x 1000, b = new level (0 = same); with `mon`, the HP,
                    * max HP and level after the level up are taken from it */
  BEV_WAIT,        /* a = ms */
  BEV_POKEMON_ANIM,/* PokemonAnimPhase: battler, a = PokemonAnimType (the Substitute doll's
                    * ADD, PRE_MOVE, POST_MOVE, REMOVE), or -1: the doll goes without an animation
                    * (SubstituteTag.onRemove while the Pokemon is in focus) */
  BEV_VISIBLE,     /* battler, a = 0: hide it (SemiInvulnerableTag.onAdd), 1: show it again two
                    * frames later (onRemove); the queue doesn't wait */
  BEV_FORM_CHANGE, /* battler, a = species, b = form (Mega Evolution, Gigantamax, form change) */
  BEV_MAX_HP,      /* battler, a = new max HP (a form with other base stats); the HP follows in a heal or damage event */
  BEV_CATCH,       /* AttemptCapturePhase's animation on the enemy battler: the ball (a = PokeballType 0..4) is thrown, takes
                    * the Pokemon in, bounces and shakes b times. caught: the ball locks and the event ends with the Pokemon
                    * inside, for the messages and the enemy's withdraw that follow (the ball fades at that withdraw). Not
                    * caught: the ball breaks free after the shakes, the Pokemon is back whole and the ball fades */
  BEV_TRANSFORM,   /* mon = temporary display; a = play Transform sound (Imposter) */
  BEV_ILLUSION,    /* PR_DEV_ILLUSION: mon = the new look, name and types when a disguise starts or ends; a = play the
                    * Transform sound (breakIllusion plays it, setIllusion doesn't) */
  BEV_LEVEL_UP,    /* LevelUpPhase.showLevelUpMessages: the fanfare and `text` ("X grew to Lv. N!") with a prompt, then
                    * promptLevelUpStats: the stat window with the gains (stats[6..11] - stats[0..5]), A, the totals, A */
} bev_type;

typedef struct {
  bev_type type;
  int8_t battler, target;
  int a, b;
  int c;                     /* BEV_FORM_CHANGE: the gender + 2 when the change brings one (-1 genderless .. 1 female), 0 = as it was:
                              * a Ditto whose transform ends takes its own gender back */
  const char* text;          /* BEV_MESSAGE, BEV_SUMMON: the text (already translated) */
  bool prompt;               /* BEV_MESSAGE: wait for a button */
  bool caught;               /* BEV_CATCH: the ball holds (b is then 3: a catch always passes the three shake checks) */
  const bv_mon* mon;         /* BEV_SUMMON */
  const int* stats;          /* BEV_LEVEL_UP: the six permanent stats before, then after */
} bv_event;

/* The player's command (upstream Command enum: FIGHT 0, BALL 1, POKEMON 2, RUN 3). */
typedef struct {
  int command;
  int move_slot;  /* FIGHT: 0..3 */
} bv_command;

typedef struct {
  int biome;             /* BiomeId (arena images) */
  bv_mon player, enemy;  /* the first Pokemon of each side (singles); enemy.species 0 = nobody */
  bool trainer;          /* a trainer battle (the trainer itself is not drawn yet) */
  bool player_present;   /* the player's lead is already out (it stays out between wild waves): its sprite
                          * and info box are there from the start, with no summon. Its platform stays too: only the enemy side
                          * slides in (upstream's NextEncounterPhase / NewBiomeEncounterPhase; the player's platform slides in
                          * only when a run begins, EncounterPhase) */
  bool next_encounter;   /* the next wave of the same biome with the lead out, on the same screen with no fade: the old enemy
                          * platform slides out to the right as the new one slides in from the left (NextEncounterPhase) */
} bv_setup;

/* The biome the screen shows now (the last bv_begin's), -1 before the first. */
int bv_biome(void);

/* What the command menu allows (game-core's COMMAND screen). A command that is off, or a move that can't be
 * used, plays the error sound; an unusable move shows its reason (an upstream message) instead. */
typedef struct {
  bool can_ball, can_switch, can_run;
  bool move_ok[4];
  const char* move_why[4];   /* the reason text for each move that is not ok (already translated), or NULL; the strings
                              * stay the caller's and must live until the command is answered */
  bool struggle;             /* every move is unusable: FIGHT goes straight to Struggle (move slot 0) */
  /* START on the command menu (ui-inputs.ts buttonMenu: UiMode.COMMAND opens the menu overlay). Called with the argument of
   * bv_ask_command; NULL = START does nothing. The fight menu is a mode of its own upstream and does not open it. */
  void (*on_menu)(void* arg);
} bv_command_opts;

void bv_begin(const bv_setup* setup);
/* Queues events (copied; text is copied too) and plays them; on_done runs after the last one. */
void bv_play(const bv_event* ev, int n, void (*on_done)(void* arg), void* arg);
bool bv_playing(void);
/* opts NULL = everything allowed. */
void bv_ask_command(const bv_command_opts* opts, void (*on_command)(bv_command c, void* arg), void* arg);
/* The screen's current view of a side's Pokemon (after the events played so far). */
bv_mon* bv_mon_of(int battler);

#ifdef __cplusplus
}
#endif
#endif
