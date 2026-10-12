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

/* reward-core: the options of one SelectModifierPhase (getPlayerModifierTypeOptions), rolled on run->run.rng
 * from a pr_reward_ctx made from the run (src/gc_reward.c). `kind` is the phase:
 *   GC_REWARD_FIRST       its first screen: resetSeed(), the pool's thresholds, the options
 *   GC_REWARD_RESTRICTED  the screen after a lost battle with no enemy fainted: no options, no reroll
 *   GC_REWARD_REROLL      the screen after a paid reroll (the previous options' tiers are kept while the
 *                         Lock Capsule's lock is on)
 *   GC_REWARD_COPY        the screen again after a TM or Memory Mushroom (gc_reward_queue_copy): the same
 *                         options, nothing rolled
 *   GC_REWARD_CUSTOM      a mystery encounter's shop: SelectModifierPhase(0, undefined, the encounter's
 *                         customModifierSettings), which the encounter left in the run (gc_mystery.c)
 * Fills out, and *reroll_cost (SelectModifierPhase.getRerollCost; < 0: no reroll). Returns how many options. */
enum { GC_REWARD_FIRST, GC_REWARD_RESTRICTED, GC_REWARD_REROLL, GC_REWARD_COPY, GC_REWARD_CUSTOM };
int gc_port_reward_options(gc_run *run, int kind, gc_offer out[GC_MAX_OPTIONS], double *reroll_cost);
/* Take reward `o` (what it does: reward-core's application for healing, revives, PP restores and PP Ups,
 * src/gc_reward.c; for the other items a stand-in that adds them to run->items or applies the few it knows),
 * used on party member `party` (an index into party[]) / move slot `move_slot` where it needs one. false (and
 * *why) when it can't be used there; nothing changes then. o->cost > 0 means a purchase: a reward that does
 * nothing is refused and keeps the screen; a free reward is taken either way where upstream takes it. */
bool gc_port_apply_reward(gc_run *run, const gc_offer *o, int party, int move_slot, const char **why);
/* ModifierRewardPhase: a fixed reward (EXP_CHARM at wave 10, GOLDEN_POKEBALL...) given as it is. Returns -1, or the
 * reward-core item index given instead when the item's stack was full (addModifier's itemStackFull fallback). */
int gc_port_give_reward(gc_run *run, const char *key);

#endif
