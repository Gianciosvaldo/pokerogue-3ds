/* An entry for the run that just ended (see run_history.h: rhist_record). */
#include <stdio.h>
#include <string.h>

#include "gc/run.h"
#include "pr_data.h"
#include "pr_enums.h"
#include "run_history.h"

/* a party member's fields, as menus' summary shows them (party_screen.c's gc_to_menu_mon reads the same ones) */
static void fill_mon(const gc_mon* gm, rh_mon* o) {
  const pr_pokemon* p = &gm->b;
  memset(o, 0, sizeof *o);
  o->species = p->species;
  o->form = p->form;
  o->gender = p->gender;
  o->shiny = p->shiny;
  o->variant = gm->variant;
  o->level = p->level > 0 ? (uint32_t)p->level : 0;
  o->hp = p->hp > 0 ? (uint32_t)p->hp : 0;
  o->fainted = p->hp <= 0;
  o->nature = p->nature;
  o->ability = p->ability;
  o->passive = p->passive;
  o->has_passive = p->passive_flag;
  o->n_moves = p->n_moves < 4 ? p->n_moves : 4;
  for (int i = 0; i < o->n_moves; i++) {
    o->moves[i] = p->moves[i];
    o->pp_used[i] = p->pp_used[i];
    const int max = p->max_pp_override[i] >= 0 ? p->max_pp_override[i] : pr_moves[p->moves[i]].pp;
    o->pp_max[i] = (uint8_t)(max < 255 ? max : 255);
  }
  int8_t types[PR_MAX_TYPES];
  const int nt = pr_get_types(p, types, 0);
  o->types[0] = nt > 0 ? types[0] : 0;
  o->types[1] = nt > 1 ? types[1] : -1;
  for (int i = 0; i < 6; i++) {
    o->stats[i] = p->stats[i] > 0 ? (uint32_t)p->stats[i] : 0;
    o->ivs[i] = p->ivs[i];
  }
  o->friendship = p->friendship;
  o->tera_type = p->tera_type;
  o->luck = gm->luck > 0 ? (uint8_t)gm->luck : 0;
}

void rhist_record(const gc_run* run, bool victory, int64_t now, const char* name, rh_entry* out) {
  memset(out, 0, sizeof *out);
  out->time = now;
  out->victory = victory;
  out->mode = run->run.mode;
  out->wave = run->wave;
  out->biome = run->biome;
  out->score = run->score;
  out->money = run->money <= 0 ? 0 : run->money > 9e15 ? (int64_t)9e15 : (int64_t)run->money;
  out->n_party = (uint8_t)(run->n_party < RH_PARTY ? run->n_party : RH_PARTY);
  for (int i = 0; i < out->n_party; i++) {
    int idx = run->party_order[i];
    if (idx < 0 || idx >= run->n_party) idx = i; /* the player's order, as the slot list shows it */
    fill_mon(&run->party[idx], &out->party[i]);
  }
  if (name) snprintf(out->name, sizeof out->name, "%s", name);
  if (victory) return;
  /* what beat the player, the way run-history-ui-handler.ts's RunEntryContainer.setup tells the battles apart: wild Pokemon (a mystery
   * encounter without a trainer too) show the Pokemon, a trainer shows its name, the rival only "Rival" (a trainer type from 375 on) */
  const bool trainer = run->trainer_type >= 0 && (run->battle_type == BATTLE_TYPE_TRAINER || run->battle_type == BATTLE_TYPE_MYSTERY_ENCOUNTER);
  if (trainer) {
    out->defeat = run->trainer_type >= TRAINER_TYPE_RIVAL ? RH_DEFEAT_RIVAL : RH_DEFEAT_TRAINER;
    out->trainer_double = run->trainer_variant == TRAINER_VARIANT_DOUBLE;
    out->trainer_female = run->trainer_variant == TRAINER_VARIANT_FEMALE;
    const char* t = run->trainer_names[GC_TNAME_TITLED];
    if (t && strlen(t) < sizeof out->trainer) memcpy(out->trainer, t, strlen(t) + 1);
  } else if (run->battle_type == BATTLE_TYPE_WILD || run->battle_type == BATTLE_TYPE_MYSTERY_ENCOUNTER) {
    out->defeat = RH_DEFEAT_WILD;
    out->n_enemy = (uint8_t)(run->n_enemy < RH_ENEMIES ? run->n_enemy : RH_ENEMIES);
    if (out->n_enemy == 0) out->defeat = RH_DEFEAT_NONE;
    for (int i = 0; i < out->n_enemy; i++) fill_mon(&run->enemy[i], &out->enemy[i]);
  }
}
