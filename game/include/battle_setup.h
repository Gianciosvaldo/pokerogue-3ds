/*
 * battle_setup: a STAND-IN that builds a battle-core battle for the demo, until game-core's
 * battle adapter lands (the run loop owns building Pokemon and battles; this file goes then).
 *
 * It makes fresh Pokemon (species, level, moves; random IVs, nature, gender; the species'
 * first ability) and a wild singles battle, and runs turns with a FIGHT command for each side.
 * The enemy picks a random move with PP (ai-core replaces that).
 */
#ifndef GM_BATTLE_SETUP_H
#define GM_BATTLE_SETUP_H

#include <stdbool.h>
#include <stdint.h>

#include "pr/battle.h"
#include "pr/engine.h"

typedef struct {
  pr_battle bt;
  pr_engine eng;
  pr_pokemon mon[2][PR_MAX_PARTY]; /* [0] player, [1] enemy */
  uint32_t next_id;
} bs_battle;

/* Resets b and sets up a wild singles battle in `biome` at `wave`, seeded with `seed`. */
void bs_begin(bs_battle* b, int biome, int wave, const char* seed);
/* Adds a Pokemon to a side's party (the first one goes on the field). Returns it, or NULL. */
pr_pokemon* bs_add(bs_battle* b, int side, int species, int level, const int* moves, int n_moves);
/* Runs one turn: the player uses move slot `slot` and the enemy a random move. The events are in
 * b->eng.events. */
void bs_turn_fight(bs_battle* b, int slot);
/* 0 = still going, 1 = the player won (every enemy fainted), 2 = the player lost. */
int bs_outcome(const bs_battle* b);

#endif
