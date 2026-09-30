/* What game-core needs from the workstreams that haven't landed yet. Each call has a stand-in in
 * src/gc_stubs.c that keeps a run playable and counts itself in run->stubs; when ai-core, reward-core and
 * run-core's enemy moveset land, their headers replace these stand-ins one by one and nothing else in
 * game-core changes. */
#ifndef GC_PORTS_H
#define GC_PORTS_H
#include "gc/run.h"

/* ai-core: EnemyCommandPhase for the enemy on field slot `enemy_slot` (BattlerIndex 2 or 3): fills
 * bt->commands[enemy_slot] (and pre_commands) as upstream's EnemyPokemon.getNextMove / getNextTargets
 * and the trainer's switch decision would. */
void gc_port_enemy_command(gc_run *run, int enemy_slot);

/* run-core: generateAndPopulateMoveset for a new enemy (between pr_run_enemy_pokemon_base and
 * pr_run_enemy_pokemon_finish), rolling on run->run.rng as upstream does. */
void gc_port_enemy_moveset(gc_run *run, gc_mon *m);

/* reward-core: SelectModifierPhase's options for this wave (getPlayerModifierTypeOptions), rolled on
 * run->run.rng, and what one does when taken (the item's own apply, or adding it to run->items).
 * reroll_count is how many rerolls were paid for this wave. Returns how many options. */
int gc_port_reward_options(gc_run *run, int reroll_count, gc_offer out[GC_MAX_OPTIONS]);
/* Take reward `o`, used on party member `party` / move slot `move_slot` where it needs one. false
 * (and *why) when it can't be used there; nothing changes then. */
bool gc_port_apply_reward(gc_run *run, const gc_offer *o, int party, int move_slot, const char **why);
/* ModifierRewardPhase: a fixed reward (EXP_CHARM at wave 10, GOLDEN_POKEBALL...) given as it is. */
void gc_port_give_reward(gc_run *run, const char *key);

#endif
