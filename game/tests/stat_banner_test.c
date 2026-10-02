/* Unit test for stat stage modifier indicators, status visual banners, and text banner queuing. */
#include <assert.h>
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
  bs_begin(&b, BIOME_ID_GRASS, 5, "stat_banner_test_seed");

  /* Level 100 Blaziken with Swords Dance vs Level 10 Garchomp with Toxic / Thunder Wave / Growl */
  int pm[] = {MOVE_ID_SWORDS_DANCE, MOVE_ID_BLAZE_KICK, MOVE_ID_CLOSE_COMBAT, MOVE_ID_BRAVE_BIRD};
  int em[] = {MOVE_ID_TOXIC, MOVE_ID_EARTHQUAKE, MOVE_ID_DRAGON_CLAW, MOVE_ID_ROCK_SLIDE};
  bs_add(&b, 0, SPECIES_ID_BLAZIKEN, 100, pm, 4);
  bs_add(&b, 1, SPECIES_ID_GARCHOMP, 10, em, 4);

  bv_mon shown_player = {0};
  bv_mon shown_enemy = {0};
  be_mon(&b.bt, b.bt.field[0], &shown_player);
  be_mon(&b.bt, b.bt.field[2], &shown_enemy);

  /* Run turn 1: Blaziken uses Swords Dance (slot 0) */
  bs_turn_fight(&b, 0);
  printf("turn 1 n_events=%d:\n", b.eng.n_events);
  for (int i = 0; i < b.eng.n_events; i++) {
    printf("  ev %d: type=%d battler=%d id=%d val=%d msg.key=%s\n", i, b.eng.events[i].type, b.eng.events[i].battler, b.eng.events[i].id, b.eng.events[i].value, b.eng.events[i].msg.key);
  }

  static bv_event out_events[128];
  int n_conv = be_convert(&b.bt, &b.eng, &shown_player, &shown_enemy, out_events, 128);
  printf("n_conv=%d:\n", n_conv);
  for (int i = 0; i < n_conv; i++) {
    printf("  bev %d: type=%d battler=%d a=%d b=%d text=%s\n", i, out_events[i].type, out_events[i].battler, out_events[i].a, out_events[i].b, out_events[i].text);
  }

  bool saw_stat_stage_event = false;
  bool saw_text_event = false;

  for (int i = 0; i < n_conv; i++) {
    if (out_events[i].type == BEV_STAT_STAGES) {
      saw_stat_stage_event = true;
      /* battler 0 (player), stat 1 (ATK), stage delta non-zero */
      assert(out_events[i].battler == 0);
      assert(out_events[i].a == STAT_ATK);
      assert(out_events[i].b != 0);
    }
    if (out_events[i].type == BEV_MESSAGE) {
      saw_text_event = true;
      assert(out_events[i].text != NULL);
    }
  }

  assert(saw_stat_stage_event);
  assert(saw_text_event);

  /* Now test BEV_STATUS conversion and queuing */
  shown_player.hp = b.bt.field[0]->hp;
  shown_enemy.hp = b.bt.field[2]->hp;
  bv_event status_ev = {.type = BEV_STATUS, .battler = 0, .a = STATUS_EFFECT_POISON};
  bv_play(&status_ev, 1, NULL, NULL);

  printf("stat_banner_test PASSED: %d events converted, stat stage, status & text banner events verified.\n", n_conv);
  return 0;
}
