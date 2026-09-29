/* game-core internals shared by the files in src/ */
#ifndef GC_INTERNAL_H
#define GC_INTERNAL_H
#include "gc/run.h"
#include "gc/ports.h"
#include "gc/data.h"
#include <math.h>
#include <string.h>

/* ---- the run's phase queue: upstream's PhaseTree for the run-level phases (src/phase-tree.ts). A push
 * goes to the bottom level (runs last); a phase added while another runs (unshiftNew) goes to the level
 * above, so the phases one phase adds run right after it, in the order it added them. */
enum {
    RP_NONE,
    RP_NEW_BATTLE,       /* NewBattlePhase + the EncounterPhase that follows it */
    RP_TURN_INIT,        /* TurnInitPhase, then the COMMAND screen */
    RP_COMMAND,          /* CommandPhase: waits on the COMMAND screen */
    RP_TURN,             /* battle-core's turn, from TurnStartPhase to its next stop */
    RP_SWITCH,           /* battle-core's SwitchPhase: waits on the SWITCH screen */
    RP_VICTORY,          /* VictoryPhase: a = enemy party index */
    RP_EXP,              /* ExpPhase / ShowPartyExpBarPhase: a = party index, d = exp */
    RP_LEVEL_UP,         /* LevelUpPhase: a = party index, b = last level, c = new level */
    RP_LEARN_MOVE,       /* LearnMovePhase: a = party index, b = move; may wait on LEARN_MOVE */
    RP_EVOLUTION,        /* EvolutionPhase: a = party index, b = gc_evo_data index, c = last level */
    RP_TRAINER_NEXT,     /* the trainer's next Pokemon (stands in for SwitchSummonPhase) */
    RP_BATTLE_END,       /* BattleEndPhase: a = victory */
    RP_TRAINER_VICTORY,  /* TrainerVictoryPhase */
    RP_MONEY_REWARD,     /* MoneyRewardPhase: d = money multiplier */
    RP_MODIFIER_REWARD,  /* ModifierRewardPhase: key */
    RP_SELECT_MODIFIER,  /* SelectModifierPhase: waits on REWARD */
    RP_SELECT_BIOME,     /* SelectBiomePhase: may wait on BIOME */
    RP_SWITCH_BIOME,     /* SwitchBiomePhase: a = biome */
    RP_PARTY_HEAL,       /* PartyHealPhase */
    RP_GAME_OVER,        /* GameOverPhase: a = victory; waits on GAME_OVER / CLEAR */
    RP_COUNT_
};
typedef struct { uint8_t type; int32_t a, b, c; double d; const char *key; } gc_rphase;
#define GC_RQ_LEVELS 12
#define GC_RQ_CAP 24
typedef struct {
    gc_rphase q[GC_RQ_LEVELS][GC_RQ_CAP];
    int n[GC_RQ_LEVELS];
    int n_levels, current_level;
    gc_rphase current;          /* the phase running now (type RP_NONE = none) */
    bool waiting;               /* it waits on a screen */
    bool overflow;
} gc_rqueue;

/* run-level state that isn't part of the public struct */
typedef struct {
    gc_rqueue rq;
    uint32_t stub_rng;          /* the stand-ins' own RNG, so they never move upstream's */
    int reroll_count;
    int enemy_switch_counter;
    bool lock_rarities;
    int last_battle_type;       /* the finished battle's BattleType, -1 = none yet */
    bool last_final_boss;       /* it was the classic final boss */
    double last_max_exp_level;  /* getMaxExpLevel() when it ended, for LevelCapPhase */
    bool pending_trainer_victory;
} gc_internal;
gc_internal *gc_int(gc_run *run);

void gc_rq_reset(gc_rqueue *q);
void gc_rq_push(gc_run *run, gc_rphase p);        /* pushNew */
void gc_rq_unshift(gc_run *run, gc_rphase p);     /* unshiftNew */
bool gc_rq_has(gc_run *run, int type);
bool gc_rq_remove(gc_run *run, int type);          /* tryRemovePhase */
void gc_rq_remove_all(gc_run *run, int type);
void gc_rq_clear(gc_run *run);
bool gc_rq_next(gc_rqueue *q, gc_rphase *out);
static inline gc_rphase gc_rp(int type, int a, int b, int c) { gc_rphase p = {(uint8_t)type, a, b, c, 0, NULL}; return p; }

/* ---- events (src/run.c) ---- */
gc_event *gc_ev_new(gc_run *run, int kind);
void gc_ev_msg(gc_run *run, const char *key, int n, ...);   /* n pr_msg_arg values follow */
pr_msg_arg gc_m_mon(gc_run *run, const char *name, int side, int idx);   /* getPokemonNameWithAffix */
pr_msg_arg gc_m_mon_name(gc_run *run, const char *name, int side, int idx);   /* getNameToRender */
pr_msg_arg gc_m_trainer(gc_run *run, const char *name);                  /* the trainer's name */
pr_msg_arg gc_m_item(const char *name, const char *item_key);            /* a modifier type's name */
void gc_ev_battle(gc_run *run);                   /* battle-core's events of the last call, as GC_EV_BATTLE */
void gc_stub(gc_run *run, int which);

/* ---- Pokemon (src/mon.c) ---- */
const pr_species *gc_species(int id);
const pr_form *gc_form(int species, int form);
int gc_form_ability(const pr_form *f, int ability_index);
void gc_mon_new_player(gc_run *run, gc_mon *m, const gc_starter *s, int level);
void gc_mon_new_enemy(gc_run *run, gc_mon *m, int species, int level, const pr_enemy_gen *g, int trainer_slot);
void gc_mon_set_move(gc_mon *m, int slot, int move);                  /* setMove */
int gc_move_max_pp(const pr_pokemon *p, int slot);                     /* PokemonMove.getMovePp */
bool gc_move_usable(const pr_pokemon *p, int slot, const char **why);  /* the PP and implementation checks */
/* the moves LevelUpPhase queues for levels last+1..level (own level moves, no prevolution ones) */
int gc_level_up_moves(const gc_mon *m, int starting_level, uint16_t *out, int max);
/* the level-0 (EVOLVE_MOVE) moves EvolutionPhase queues */
int gc_evolution_moves(const gc_mon *m, uint16_t *out, int max);
void gc_mon_add_exp(gc_run *run, gc_mon *m, double exp);              /* addExp */
double gc_level_total_exp(int level, int growth_rate);
/* getEvolution(): the gc_evo_data index, -1 = none */
int gc_mon_get_evolution(gc_run *run, const gc_mon *m);
void gc_mon_evolve(gc_run *run, gc_mon *m, int evo);
void gc_mon_recalc(gc_run *run, gc_mon *m);                            /* calculateStats on a living Pokemon */
bool gc_mon_fainted(const gc_mon *m);
bool gc_mon_allowed(const gc_mon *m);                                  /* isAllowedInBattle */

/* ---- items (src/items.c) ---- */
int gc_item_stack(const gc_run *run, const char *key);                 /* stack count of a player item, 0 = none */
void gc_item_add(gc_run *run, const char *key, int16_t arg, uint32_t pokemon_id, int stack);
void gc_items_to_battle(gc_run *run);                                  /* the player's and the enemy's lists into bt->mods */
double gc_apply_exp_boosters(const gc_run *run, double exp);          /* ExpBoosterModifier */

/* ---- the encounter and the battle (src/encounter.c, src/battle.c) ---- */
void gc_new_battle(gc_run *run);                  /* NewBattlePhase + EncounterPhase */
bool gc_is_new_biome(const gc_run *run, int wave);   /* isNewBiome for the battle of `wave` */
bool gc_battle_setup_field(gc_run *run, bool reset_arena);   /* bt from the parties, before the first turn */
void gc_battle_turn_init(gc_run *run);            /* TurnInitPhase's part: participants, resetTurnData */
/* The player's command for field slot 0: fills bt->commands[0]; false (and why) if not allowed */
bool gc_battle_player_command(gc_run *run, const gc_choice *c, const char **why);
void gc_battle_command_screen(gc_run *run);       /* fills run->screen for COMMAND */
/* battle-core's turn from TurnStartPhase (or the pick after a SwitchPhase, pick >= -1), to its next
 * stop: returns the stop's phase type */
int gc_battle_run(gc_run *run, int pick);
void gc_battle_switch_screen(gc_run *run);        /* fills run->screen for SWITCH */
void gc_battle_end_cleanup(gc_run *run);          /* after a battle: the party off the field */
int gc_enemy_active_index(const gc_run *run);     /* the enemy party index on the field, -1 = none */
int gc_player_active_index(const gc_run *run);
bool gc_battle_queued_command(gc_run *run);       /* a move locked in by the last turn: no COMMAND screen */
bool gc_battle_trainer_next(gc_run *run);         /* the trainer's next Pokemon onto the field */
bool gc_battle_turn_tail_lost(const gc_run *run); /* battle-core stopped with turn phases still queued */
int gc_battle_slot_of(const gc_run *run, int party_index);   /* upstream party slot of party[i] */
void gc_sync_party_order(gc_run *run);            /* party_order from battle-core's party list */

/* run-phase handlers (src/phases.c): return true when the phase is done, false when it waits on a screen */
bool gc_phase_start(gc_run *run, gc_rphase *p);
bool gc_phase_choose(gc_run *run, gc_rphase *p, const gc_choice *c, const char **why, bool *ok);

uint32_t gc_stub_rand(gc_run *run, uint32_t n);

#endif
