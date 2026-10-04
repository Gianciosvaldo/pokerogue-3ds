/*
 * battle_setup: a TEST FIXTURE that builds a battle-core battle without the run (the app no longer uses it:
 * run_screen.c drives game-core's run). The converter tests (evtest, switchtest, stat_banner_test,
 * form_anim_test) use it to run single turns with hand-made Pokemon.
 *
 * It makes fresh Pokemon (species, level, moves; random IVs, nature, gender; the species' first ability)
 * and a wild singles battle, and runs turns with the player's FIGHT or POKEMON command. The enemy's command
 * comes from ai-core (pr_ai_enemy_command), as upstream's EnemyCommandPhase.
 */
#ifndef GM_BATTLE_SETUP_H
#define GM_BATTLE_SETUP_H

#include <stdbool.h>
#include <stdint.h>

#include "pr/battle.h"
#include "pr/ai.h"
#include "pr/engine.h"

typedef struct {
  pr_battle bt;
  pr_engine eng;
  pr_ai ai;
  pr_pokemon mon[2][PR_MAX_PARTY]; /* [0] player, [1] enemy */
  uint32_t next_id;
} bs_battle;

/* Resets b and sets up a wild singles battle in `biome` at `wave`, seeded with `seed`. */
void bs_begin(bs_battle* b, int biome, int wave, const char* seed);
/* Adds a Pokemon to a side's party (the first one goes on the field). Returns it, or NULL. */
pr_pokemon* bs_add(bs_battle* b, int side, int species, int level, const int* moves, int n_moves);
/* Runs one turn: the player uses move slot `slot`, the enemy what ai-core picks. The events are in
 * b->eng.events. */
void bs_turn_fight(bs_battle* b, int slot);
/* Runs one turn where the player switches to party slot `slot` (Command.POKEMON). */
void bs_turn_switch(bs_battle* b, int slot);
/* The turn stopped at a SwitchPhase for the player (a faint): the party screen picks, then
 * bs_pick(slot) runs on (events in b->eng.events again). */
bool bs_awaiting_player_pick(const bs_battle* b);
void bs_pick(bs_battle* b, int slot);
/* 0 = still going, 1 = the player won (every enemy fainted), 2 = the player lost. */
int bs_outcome(const bs_battle* b);

#endif
