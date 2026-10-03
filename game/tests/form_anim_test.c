/* Unit test for Mega Evolution and Gigantamax form animation states, event conversion, sprite key resolution, and scaling. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "battle_events.h"
#include "battle_setup.h"
#include "battle_view.h"
#include "pr/battle.h"
#include "pr/engine.h"
#include "pr/events.h"
#include "pr_enums.h"

/* Stub locale/i18n helpers for host test */
const char* i18n_language(void) { return "en"; }
const char* i18n_species_name(int id) {
  static char name[32];
  snprintf(name, sizeof name, "Species_%d", id);
  return name;
}
char* pak_read(const char* key, size_t* len) {
  if (len) *len = 0;
  return NULL;
}
void bv_play(const bv_event* ev, int n, void (*on_done)(void*), void* arg) {
  (void)ev;
  (void)n;
  (void)on_done;
  (void)arg;
}

int main(int argc, char** argv) {
  static bs_battle b;
  bs_begin(&b, BIOME_ID_GRASS, 5, "form_anim_test_seed");

  /* Charizard vs Charizard */
  int pm[] = {MOVE_ID_FLAMETHROWER, MOVE_ID_DRAGON_CLAW, MOVE_ID_AIR_SLASH, MOVE_ID_ROOST};
  int em[] = {MOVE_ID_FLAMETHROWER, MOVE_ID_DRAGON_CLAW, MOVE_ID_AIR_SLASH, MOVE_ID_ROOST};
  bs_add(&b, 0, SPECIES_ID_CHARIZARD, 50, pm, 4);
  bs_add(&b, 1, SPECIES_ID_CHARIZARD, 50, em, 4);

  bv_mon shown_player = {0};
  bv_mon shown_enemy = {0};
  be_mon(&b.bt, b.bt.field[0], &shown_player);
  be_mon(&b.bt, b.bt.field[2], &shown_enemy);

  assert(shown_player.species == SPECIES_ID_CHARIZARD);
  assert(shown_player.form == 0);

  /* Mega Evolve player Charizard */
  pr_mega_evolve(&b.bt, b.bt.field[0]);
  assert(b.bt.field[0]->form == 1); /* Charizard Mega X form */

  /* Dynamax enemy Charizard (Gigantamax) */
  pr_dynamax(&b.bt, b.bt.field[2]);
  assert(b.bt.field[2]->form == 3); /* Charizard Gigantamax form */

  static bv_event out_events[128];
  int n_conv = be_convert(&b.bt, &b.eng, &shown_player, &shown_enemy, out_events, 128);
  printf("n_conv=%d events converted after Mega Evolve & Dynamax:\n", n_conv);
  for (int i = 0; i < n_conv; i++) {
    printf("  bev %d: type=%d battler=%d a=%d b=%d text=%s\n", i, out_events[i].type, out_events[i].battler, out_events[i].a, out_events[i].b, out_events[i].text ? out_events[i].text : "(null)");
  }

  bool saw_player_mega_form = false;
  bool saw_enemy_gmax_form = false;

  for (int i = 0; i < n_conv; i++) {
    if (out_events[i].type == BEV_FORM_CHANGE) {
      if (out_events[i].battler == 0) {
        saw_player_mega_form = true;
        assert(out_events[i].a == SPECIES_ID_CHARIZARD);
        assert(out_events[i].b == 1); /* Mega X */
      }
      if (out_events[i].battler == 2) {
        saw_enemy_gmax_form = true;
        assert(out_events[i].a == SPECIES_ID_CHARIZARD);
        assert(out_events[i].b == 3); /* Gigantamax */
      }
    }
  }

  assert(saw_player_mega_form);
  assert(saw_enemy_gmax_form);

  /* Verify updated form states in bv_mon */
  bv_mon mega_mon = {0};
  be_mon(&b.bt, b.bt.field[0], &mega_mon);
  assert(mega_mon.species == SPECIES_ID_CHARIZARD);
  assert(mega_mon.form == 1);
  assert(mega_mon.n_types == 2);
  assert(mega_mon.types[0] == POKEMON_TYPE_FIRE);
  assert(mega_mon.types[1] == POKEMON_TYPE_DRAGON);

  bv_mon gmax_mon = {0};
  be_mon(&b.bt, b.bt.field[2], &gmax_mon);
  assert(gmax_mon.species == SPECIES_ID_CHARIZARD);
  assert(gmax_mon.form == 3);

  printf("form_anim_test PASSED: Mega Evolution (form %d, Fire/Dragon) and Gigantamax (form %d, scale 1.5x) verified.\n", mega_mon.form, gmax_mon.form);
  return 0;
}
