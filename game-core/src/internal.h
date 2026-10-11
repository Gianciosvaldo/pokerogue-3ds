/* game-core internals shared by the files in src/ */
#ifndef GC_INTERNAL_H
#define GC_INTERNAL_H
#include "gc/run.h"
#include "gc/ports.h"
#include "gc/data.h"
#include "pr/ai.h"
#include "pr/reward.h"
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
    RP_CONTINUE,         /* the rest of the turn after a VictoryPhase's experience (battle-core's pr_continue) */
    RP_CHECK_SWITCH,     /* CheckSwitchPhase: waits on CHECK_SWITCH (only with run->ask_switch) */
    RP_BATTLE_END,       /* BattleEndPhase: a = victory */
    RP_TRAINER_VICTORY,  /* TrainerVictoryPhase */
    RP_MONEY_REWARD,     /* MoneyRewardPhase: d = money multiplier */
    RP_MODIFIER_REWARD,  /* ModifierRewardPhase: key */
    RP_SELECT_MODIFIER,  /* SelectModifierPhase: waits on REWARD */
    RP_SELECT_BIOME,     /* SelectBiomePhase: may wait on BIOME */
    RP_SWITCH_BIOME,     /* SwitchBiomePhase: a = biome */
    RP_PARTY_HEAL,       /* PartyHealPhase */
    RP_GAME_OVER,        /* GameOverPhase: a = victory; waits on GAME_OVER / CLEAR */
    RP_CATCH,            /* AttemptCapturePhase.catch() after the roll: a = enemy index, b = ball; waits on SWITCH when the party is full */
    RP_MYSTERY,          /* MysteryEncounterPhase: a = selected type, b = selected tier */
    RP_REWARD_FORM,      /* bounded FormChangePhase: a = storage party index, b = target form */
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

/* ---- the reward screen (src/gc_reward.c): what a SelectModifierPhase keeps between its calls ---- */
typedef struct {
    pr_reward_option options[PR_REWARD_MAX_OPTIONS];   /* typeOptions of the open phase */
    int n_options;
    bool has_custom, is_copy;
    pr_reward_custom custom;    /* its customModifierSettings: the no-victory screen's, or a copy's old options */
    pr_reward_thresholds th;    /* modifierPoolThresholds: global upstream, kept so a copy rolls with the old ones */
    /* copy(): the options, rerollCount and rerollMultiplier a copy of the screen is made from, until it starts */
    bool copy_pending;
    int copy_reroll_count;
    double copy_reroll_multiplier;
    pr_reward_option copy_options[PR_REWARD_MAX_OPTIONS];
    int n_copy_options;
} gc_reward_state;

/* run-level state that isn't part of the public struct */
typedef struct {
    gc_rqueue rq;
    uint32_t stub_rng;          /* the stand-ins' own RNG, so they never move upstream's */
    int reroll_count;
    int battle_item_count[2];   /* represented inventory prefix; rebuilt on load/setup */
    /* ai-core's view of the battle: enemySwitchCounter, each Pokemon's AI entry (initialTeamIndex,
     * waveData.abilityRevealed); reset at each new battle. ai.trainer is set at each call, as it
     * points into this struct */
    pr_ai ai;
    pr_ai_trainer ai_trainer;
    bool lock_rarities;         /* globalScene.lockModifierTiers (the Lock Capsule's switch) */
    gc_reward_state reward;
    int last_battle_type;       /* the finished battle's BattleType, -1 = none yet */
    bool last_final_boss;       /* it was the classic final boss */
    double last_max_exp_level;  /* getMaxExpLevel() when it ended, for LevelCapPhase */
    bool pending_trainer_victory;
    bool battle_won;            /* a VictoryPhase has run in this battle: its BattleEndPhase is the victory's */
    uint8_t thrown_ball;        /* the PokeballType of the last ball command: the one a catch is made with */
    bool pending_me_miss;       /* EncounterPhase increments spawn chance after the save point */
    bool at_save_point;         /* from GC_EV_SAVE_POINT until the battle's first turn runs (gc_run_can_save) */
    /* gc_trainer_names' ring: run->trainer_names' texts, gc_m_trainer's key for each and whether that
     * key is the whole name (bit per GC_TNAME_*) */
    uint8_t name_ring, name_exact[3];
    char name_text[3][GC_TNAME_COUNT_][128];
    char name_arg[3][GC_TNAME_COUNT_][72];
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
void gc_run_until_screen(gc_run *run);            /* runs phases until one waits on a screen */
static inline gc_rphase gc_rp(int type, int a, int b, int c) { gc_rphase p = {(uint8_t)type, a, b, c, 0, NULL}; return p; }

/* ---- events (src/gc_run.c) ---- */
gc_event *gc_ev_new(gc_run *run, int kind);
void gc_ev_msg(gc_run *run, const char *key, int n, ...);   /* n pr_msg_arg values follow */
pr_msg_arg gc_m_mon(gc_run *run, const char *name, int side, int idx);   /* getPokemonNameWithAffix */
pr_msg_arg gc_m_mon_name(gc_run *run, const char *name, int side, int idx);   /* getNameToRender */
pr_msg_arg gc_m_trainer(gc_run *run, const char *name, int which);       /* trainer.getName (GC_TNAME_*) */
pr_msg_arg gc_m_item(const char *name, const char *item_key);            /* a modifier type's name */
void gc_ev_battle(gc_run *run);                   /* battle-core's events of the last call, as GC_EV_BATTLE */
void gc_stub(gc_run *run, int which);

/* ---- Pokemon (src/gc_mon.c) ---- */
const pr_species *gc_species(int id);
const pr_form *gc_form(int species, int form);
int gc_form_ability(const pr_form *f, int ability_index);
void gc_mon_new_player(gc_run *run, gc_mon *m, const gc_starter *s, int level);
void gc_mon_new_enemy(gc_run *run, gc_mon *m, int species, int level, const pr_enemy_gen *g, int trainer_slot);
/* Task 63's clean wild constructor slice; false leaves the RNG/mon untouched. */
bool gc_enemy_moveset(gc_run *run, gc_mon *m, const pr_enemy_gen *g);
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

/* ---- learnsets (src/gc_learnset.c): field/learnsets.ts getLevelMoves, as the reward screen reads it ---- */
/* One row of PlayerPokemon.getLevelMoves: its level (-1 RELEARN_MOVE, 0 EVOLVE_MOVE), the move and its
 * LearnableMoveSource (PR_LEARN_SOURCE_*) */
typedef struct { int16_t level; uint16_t move; int8_t source; } gc_learn_row;
/* Pokemon.getLevelMoves({startingLevel, includeEvolutionMoves, includePrevolutionMoves, includeRelearnerMoves}):
 * the rows, or -1 when they don't fit in `cap` */
int gc_level_moves(const gc_mon *m, int starting_level, bool evo, bool prevo, bool relearn, gc_learn_row *out, int cap);
/* PlayerPokemon.getUnlockedEggMoves(): how many (out holds 4) */
int gc_unlocked_egg_moves(const gc_run *run, const gc_mon *m, uint16_t out[4]);

/* ---- rewards (src/gc_reward.c): the adapter between game-core's run and reward-core ---- */
/* getPartyLuckValue(party) */
int gc_party_luck(gc_run *run);
/* A pr_reward_ctx made from the run as it is now (a static: valid until the next call). Its party is in
 * upstream's party order (run->party_order), so ctx party index k is run->party[run->party_order[k]]. */
pr_reward_ctx *gc_reward_ctx(gc_run *run);
/* The types an attack move counts as for the type-boosting items: VariableMoveTypeAttr.getTypesForItemSpawn
 * or Pokemon.getMoveTypeForItemSpawn. Returns how many (out holds 2). */
int gc_types_for_item_spawn(gc_run *run, const gc_mon *m, int move, int8_t out[2]);
/* SelectModifierPhase.copy(): queue the screen again, with the options it showed (what a TM or the Memory
 * Mushroom does when it is taken) */
void gc_reward_queue_copy(gc_run *run);
/* SelectModifierPhase.getRerollCost(lockModifierTiers) for the screen that is open */
double gc_reward_reroll_cost(gc_run *run);
/* the reward-core item (enum pr_item) of a modifierTypes key, -1 = none */
int gc_reward_item_index(const char *key);
/* what a reward is used on (GC_TARGET_*): the type's own PokemonModifierType / PokemonMoveModifierType class */
uint8_t gc_reward_target(const char *key);
/* Task 60: the application of healing, revive, PP restore and PP Up rewards (Potions, Full Restore, Revives,
 * Sacred Ash, Ether, Elixir, PP Up, PP Max) by reward-core's pr_reward_apply, on a context made from the run,
 * its result written back. `party` is an index into run->party[], `move_slot` a moveset slot. GC_APPLY_REFUSED
 * (and *why) = it can't be used there; GC_APPLY_UNHANDLED = not one of these items, the
 * caller's own code applies it. A free reward (offer cost 0) is taken even when it did nothing and has no target
 * (Sacred Ash with nobody fainted), as SelectModifierPhase.applyModifier does with cost -1.
 * Vitamin exception: an applied purchase returns REFUSED with an empty reason,
 * preserving upstream's persistent-add error/no-payment result after mutation. */
enum { GC_APPLY_UNHANDLED, GC_APPLY_DONE, GC_APPLY_REFUSED };
int gc_reward_apply(gc_run *run, const gc_offer *o, int party, int move_slot, const char **why);
int gc_reward_lifecycle_apply(gc_run *run, const gc_offer *o, int party, const char **why);
/* Task 96: Poke Ball rewards. add_pokeballs = AddPokeballModifier.apply (+5, Master Ball +1, at most 99) for a reward
 * key, false when the key is not a Poke Ball; gc_reward_gain = ModifierRewardPhase.doReward: give the reward, then
 * queue battle:rewardGain naming it. */
bool gc_reward_add_pokeballs(gc_run *run, const char *key);
void gc_reward_gain(gc_run *run, const char *key);
void gc_reward_change_form(gc_run *run, int party, int form);

/* ---- items (src/gc_items.c) ---- */
int gc_item_stack(const gc_run *run, const char *key);                 /* stack count of a player item, 0 = none */
void gc_item_add(gc_run *run, const char *key, int16_t arg, uint32_t pokemon_id, int stack);
void gc_item_remove_at(gc_run *run, int idx);
/* The enemy's held items (the modifiers with a Pokemon) and the player's, as catching moves them. Each rebuilds
 * battle-core's modifier lists (gc_items_to_battle) from the lists as they stand. */
void gc_items_take_enemy_held(gc_run *run);      /* addToParty: findModifiers(held, false) -> addModifier: onto the player's list */
void gc_items_drop_enemy_held(gc_run *run);      /* clearEnemyHeldItemModifiers() */
void gc_items_drop_held_of(gc_run *run, uint32_t pokemon_id);   /* removePartyMemberModifiers: a released member's */
int gc_item_remove_stacks(gc_run *run, const char *key, int count);
void gc_items_from_battle(gc_run *run); /* persist consumed berries/White Herb */
void gc_items_to_battle(gc_run *run);                                  /* the player's and the enemy's lists into bt->mods */
double gc_apply_exp_boosters(const gc_run *run, double exp);          /* ExpBoosterModifier */

/* ---- the encounter and the battle (src/gc_encounter.c, src/gc_battle.c) ---- */
void gc_new_battle(gc_run *run);                  /* NewBattlePhase + EncounterPhase */
/* Task 45: getMysteryEncounter() inside executeWithSeedOffset(wave * 16). */
bool gc_mystery_select(gc_run *run, int *type, int *tier);
void gc_mystery_record(gc_run *run, int type, int tier);
void gc_mystery_increment_miss(gc_run *run);
void gc_mystery_setup_screen(gc_run *run);
bool gc_mystery_execute(gc_run *run, int option, int b, int c, const char **why, bool *ok);
bool gc_gen_wild(gc_run *run, int level);
void gc_trainer_names(gc_run *run);               /* run->trainer_names for the current trainer */
bool gc_is_new_biome(const gc_run *run, int wave);   /* isNewBiome for the battle of `wave` */
bool gc_battle_setup_field(gc_run *run, bool reset_arena);   /* bt from the parties, before the first turn */
void gc_battle_restore_field(gc_run *run); /* pointers only; summon effects already saved */
void gc_ai_reset(gc_run *run);                    /* ai-core's state for a new battle */
void gc_ai_enemy_party(gc_run *run);              /* ...and the enemy party's entries once it is made */
void gc_battle_turn_init(gc_run *run);            /* TurnInitPhase's part: participants, resetTurnData */
/* The player's command for field slot 0: fills bt->commands[0]; false (and why) if not allowed */
bool gc_battle_player_command(gc_run *run, const gc_choice *c, const char **why);
void gc_battle_command_screen(gc_run *run);       /* fills run->screen for COMMAND */
/* battle-core's turn from TurnStartPhase (or the pick after a SwitchPhase, pick >= -1), to its next
 * stop: returns the stop's phase type */
int gc_battle_run(gc_run *run, int pick);
/* After a stop at PH_VICTORY and the experience: the rest of the turn (the other moves, the end-of-turn
 * effects, the trainer's next Pokemon) to the next stop (pr_continue) */
int gc_battle_continue(gc_run *run);
/* After the wild battle's start stopped at PH_CHECK_SWITCH: the answer, run to the next stop */
int gc_battle_answer_switch(gc_run *run, bool yes);
void gc_battle_switch_screen(gc_run *run);        /* fills run->screen for SWITCH */
void gc_battle_end_cleanup(gc_run *run);          /* after a battle: the party off the field */
int gc_enemy_active_index(const gc_run *run);     /* the enemy party index on the field, -1 = none */
int gc_player_active_index(const gc_run *run);
bool gc_battle_queued_command(gc_run *run);       /* a move locked in by the last turn: no COMMAND screen */
int gc_battle_slot_of(const gc_run *run, int party_index);   /* upstream party slot of party[i] */
void gc_sync_party_order(gc_run *run);            /* party_order from battle-core's party list */

/* ---- catching (src/gc_catch.c) ---- */
/* CommandPhase.checkCanUseBall + handleBallCommand's boss rule: NULL = this ball may be thrown now, else the locale
 * key of the text upstream shows instead */
const char *gc_catch_ball_refusal(const gc_run *run, int ball);
bool gc_catch_is_catch(const gc_run *run, int enemy_idx);   /* at a stop for a VictoryPhase: the enemy was caught, not fainted */
bool gc_catch_start(gc_run *run, gc_rphase *p);
bool gc_catch_choose(gc_run *run, gc_rphase *p, const gc_choice *c, const char **why, bool *ok);

/* run-phase handlers (src/gc_phases.c): return true when the phase is done, false when it waits on a screen */
bool gc_phase_start(gc_run *run, gc_rphase *p);
bool gc_phase_choose(gc_run *run, gc_rphase *p, const gc_choice *c, const char **why, bool *ok);

uint32_t gc_stub_rand(gc_run *run, uint32_t n);

/* GlobalScene wallet transport, separate from currentBattle Pay Day money. */
void gc_wallet_to_battle(gc_run *run);
void gc_wallet_from_battle(gc_run *run);

int gc_temp_add(gc_run *run, int item, int arg);
int gc_temp_apply(gc_run *run, const gc_offer *o, const char **why);
bool gc_temp_lapse(gc_run *run, int *expired);

bool gc_classic_finish(gc_run *run,bool victory,const char **why);

int gc_multi_add(gc_run *run,int storage);
int gc_multi_scene_apply(gc_run *run,const gc_offer *o,int storage,const char **why,bool selection);
int gc_wide_add(gc_run *run,int storage);
int gc_wide_scene_apply(gc_run *run,const gc_offer *o,int storage,const char **why,bool selection);

#endif

int gc_orb_add(gc_run *run,int storage,int item);
int gc_orb_scene_apply(gc_run *run,const gc_offer *o,int storage,const char **why,bool selection);

int gc_lure_add(gc_run *run,int item);
int gc_lure_apply(gc_run *run,const gc_offer *o,const char **why);
int gc_soothe_add(gc_run *run,int storage);
bool gc_soothe_battle_gain(gc_run *run,int storage);
int gc_soothe_scene_apply(gc_run *run,const gc_offer *o,int storage,const char **why,bool selection);
