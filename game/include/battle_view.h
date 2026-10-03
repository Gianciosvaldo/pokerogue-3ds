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
} bv_mon;

typedef enum {
  BEV_MESSAGE,     /* text, like a MessagePhase: a = callbackDelay ms (0 = upstream's default:
                    * 1500, or none with a prompt), prompt = wait for a button */
  BEV_INTRO_WILD,  /* the wild encounter: arenas slide in, the enemy untints, cries, info box, message */
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
  BEV_EXP,         /* battler (player), a = new exp ratio x 1000, b = new level (0 = same) */
  BEV_WAIT,        /* a = ms */
  BEV_POKEMON_ANIM,/* PokemonAnimPhase: battler, a = PokemonAnimType (the Substitute doll's
                    * ADD, PRE_MOVE, POST_MOVE, REMOVE), or -1: the doll goes without an animation
                    * (SubstituteTag.onRemove while the Pokemon is in focus) */
  BEV_VISIBLE,     /* battler, a = 0: hide it (SemiInvulnerableTag.onAdd), 1: show it again two
                    * frames later (onRemove); the queue doesn't wait */
  BEV_FORM_CHANGE, /* battler, a = species, b = form (Mega Evolution, Gigantamax, form change) */
} bev_type;

typedef struct {
  bev_type type;
  int8_t battler, target;
  int a, b;
  const char* text;          /* BEV_MESSAGE, BEV_SUMMON: the text (already translated) */
  bool prompt;               /* BEV_MESSAGE: wait for a button */
  const bv_mon* mon;         /* BEV_SUMMON */
} bv_event;

/* The player's command (upstream Command enum: FIGHT 0, BALL 1, POKEMON 2, RUN 3). */
typedef struct {
  int command;
  int move_slot;  /* FIGHT: 0..3 */
} bv_command;

typedef struct {
  int biome;             /* BiomeId (arena images) */
  bv_mon player, enemy;  /* the first Pokemon of each side (singles) */
  bool trainer;          /* a trainer battle (not drawn yet) */
} bv_setup;

void bv_begin(const bv_setup* setup);
/* Queues events (copied; text is copied too) and plays them; on_done runs after the last one. */
void bv_play(const bv_event* ev, int n, void (*on_done)(void* arg), void* arg);
bool bv_playing(void);
void bv_ask_command(void (*on_command)(bv_command c, void* arg), void* arg);
/* The screen's current view of a side's Pokemon (after the events played so far). */
bv_mon* bv_mon_of(int battler);

#ifdef __cplusplus
}
#endif
#endif
