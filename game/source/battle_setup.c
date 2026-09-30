/* Stand-in battle builder for the demo (see battle_setup.h). */
#include "battle_setup.h"

#include <string.h>

#include "pr_enums.h"

static int rnd(bs_battle* b, int n) { return n > 0 ? pr_rng_integer_in_range(&b->bt.rng, 0, n - 1) : 0; }

void bs_begin(bs_battle* b, int biome, int wave, const char* seed) {
  memset(b, 0, sizeof *b);
  pr_closures_init();
  pr_battle* bt = &b->bt;
  pr_seedstr s;
  pr_seed_from_ascii(&s, seed);
  pr_rng_sow(&bt->rng, &s);
  bt->rng_live = true;
  bt->battle_seed = s;
  pr_rng_sow(&bt->battle_rng, &s);
  bt->battle_rng_live = true;
  bt->wave_seed = s;
  bt->turn = 1;
  bt->wave_index = wave;
  bt->game_mode = GAME_MODES_CLASSIC;
  bt->battle_type = BATTLE_TYPE_WILD;
  bt->max_exp_level = 200;
  bt->biome = (int16_t)biome;
  bt->has_biome = true;
  bt->last_player_involved = bt->last_enemy_involved = -1;
  bt->ignoring_effect_source = -1;
  bt->last_used_pokeball = -1;
  for (int k = 0; k < 4; k++) bt->commands[k].command = bt->pre_commands[k].command = -1;
  b->next_id = 1000;
  b->ai.seed = s;
}

pr_pokemon* bs_add(bs_battle* b, int side, int species, int level, const int* moves, int n_moves) {
  pr_battle* bt = &b->bt;
  int slot = bt->n_party[side];
  if (slot >= PR_MAX_PARTY) return NULL;
  pr_pokemon* p = &b->mon[side][slot];
  memset(p, 0, sizeof *p);
  p->id = b->next_id += 7919;
  p->species = (uint16_t)species;
  p->level = (uint8_t)level;
  for (int i = 0; i < 6; i++) p->ivs[i] = (uint8_t)rnd(b, 32);
  p->nature = (uint8_t)rnd(b, PR_NATURE_COUNT);
  p->is_player = side == 0;
  const pr_species* sp = pr_pokemon_species(p);
  const pr_form* f = pr_pokemon_form(p);
  p->ability = p->base_ability = f->ability1;
  p->ability_override = -1;
  p->custom_passive = -1;
  p->passive_flag = false;
  p->gender = !sp || sp->male_permille < 0 ? -1 : rnd(b, 1000) < sp->male_permille ? 0 : 1;
  p->friendship = sp ? sp->base_friendship : 50;
  p->tera_type = f->type1;
  p->status_sleep_turns = p->status_freeze_turns = -1;
  for (int i = 0; i < n_moves && i < 4; i++) {
    p->moves[i] = (uint16_t)moves[i];
    p->max_pp_override[i] = -1;
  }
  p->n_moves = (uint8_t)(n_moves < 4 ? n_moves : 4);
  p->order = -1;
  p->hits_left = -1;
  p->turn_move_effectiveness = -1;
  pr_update_passive(bt, p);
  pr_calculate_stats(bt, p);
  p->hp = p->stats[STAT_HP];
  bt->party[side][slot] = p;
  bt->n_party[side]++;
  if (slot == 0) {
    p->on_field = true;
    bt->field[side ? 2 : 0] = p;
  }
  return p;
}

static void fight(struct pr_turn_command* c, int slot, int move, int target) {
  memset(c, 0, sizeof *c);
  c->command = COMMAND_FIGHT;
  c->move = (uint16_t)move;
  c->targets[0] = (int8_t)target;
  c->n_targets = 1;
  c->use_mode = MOVE_USE_MODE_NORMAL;
  c->cursor = (int8_t)slot;
}


void bs_turn_fight(bs_battle* b, int slot) {
  pr_battle* bt = &b->bt;
  for (int k = 0; k < 4; k++) bt->commands[k].command = bt->pre_commands[k].command = -1;
  pr_pokemon* me = bt->field[0];
  fight(&bt->commands[0], slot, me->moves[slot], 2);
  pr_ai_enemy_command(bt, &b->ai, 0);
  bt->n_unported = bt->n_deferred = 0;
  pr_run_turn(bt, &b->eng);
}

int bs_outcome(const bs_battle* b) {
  const pr_battle* bt = &b->bt;
  bool enemy_left = false, player_left = false;
  for (int i = 0; i < bt->n_party[1]; i++) enemy_left |= bt->party[1][i]->hp > 0;
  for (int i = 0; i < bt->n_party[0]; i++) player_left |= bt->party[0][i]->hp > 0;
  return !enemy_left ? 1 : !player_left ? 2 : 0;
}
