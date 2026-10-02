/*
 * map_ui_test.c: PC test harness for biome map selection UI driven by game-core.
 * Tests GC_SCREEN_BIOME state handling and biome choice selection.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gc/run.h"
#include "internal.h"
#include "menus.h"

/* Stub functions required by mui / rr / pak / snd when building on PC without 3DS libraries */
void mui_sfx(const char* key) {
  (void)key;
}

const char* mui_t(const char* key) {
  /* Simple mock translation for test harness */
  if (strcmp(key, "biome:grass") == 0) return "Grass";
  if (strcmp(key, "biome:forest") == 0) return "Forest";
  if (strcmp(key, "biome:cave") == 0) return "Cave";
  if (strcmp(key, "biome:plains") == 0) return "Plains";
  return key;
}

int mui_text_width(mui_style s, const char* utf8) {
  (void)s;
  return (int)strlen(utf8) * 8;
}

void mui_window(float x, float y, float w, float h) {
  (void)x; (void)y; (void)w; (void)h;
}

int mui_text(mui_style s, float x, float y, const char* utf8) {
  (void)s; (void)x; (void)y; (void)utf8;
  return 0;
}

bool mui_image(const char* key, const char* frame, float x, float y, float ox, float oy) {
  (void)key; (void)frame; (void)x; (void)y; (void)ox; (void)oy;
  return true;
}

/* Biome map selection callback context */
typedef struct {
  int selected_option;
  bool called;
} test_ctx;

static void on_biome_chosen(void* ctx, int option) {
  test_ctx* t = (test_ctx*)ctx;
  t->selected_option = option;
  t->called = true;
  biome_map_close();
}

int main(void) {
  printf("Running map_ui_test...\n");

  gc_run run;
  gc_run_new(&run, GAME_MODES_CLASSIC, "mapuitest_seed", 4242);

  gc_starter team[1];
  memset(&team[0], 0, sizeof team[0]);
  team[0].species = SPECIES_ID_BULBASAUR;
  team[0].tera_type = -1;
  for (int i = 0; i < 6; i++) team[0].ivs[i] = 15;

  const char* why = NULL;
  bool ok = gc_run_set_starters(&run, team, 1, &why);
  assert(ok);

  /* Set wave 10, PLAINS biome, and add MAP item */
  run.wave = 10;
  run.biome = BIOME_ID_PLAINS;
  gc_item_add(&run, "MAP", -1, 0, 1);

  /* Clear wave 1 run queue state and push RP_SELECT_BIOME then RP_NEW_BATTLE */
  gc_internal *in = gc_int(&run);
  gc_rq_clear(&run);
  in->rq.waiting = false;

  gc_rq_push(&run, gc_rp(RP_SELECT_BIOME, 0, 0, 0));
  gc_rq_push(&run, gc_rp(RP_NEW_BATTLE, 0, 0, 0));

  /* Trigger biome selection phase by processing queue */
  gc_run_until_screen(&run);
  const gc_screen* s = gc_run_screen(&run);

  /* Assert game-core run reached GC_SCREEN_BIOME */
  assert(s->kind == GC_SCREEN_BIOME);
  assert(s->n_biomes > 1);
  printf("game-core reached GC_SCREEN_BIOME with %d choices.\n", s->n_biomes);

  /* Open biome map UI driven by gc_screen choices */
  test_ctx t = {-1, false};
  biome_map_args ba = {s->biomes, s->n_biomes};
  biome_map_events be = {on_biome_chosen, &t};

  biome_map_open(&ba, &be);
  assert(biome_map_active());

  /* Simulate user navigation: Press DOWN then ACTION (selects index 1) */
  ok = biome_map_input(MB_DOWN);
  assert(ok);

  ok = biome_map_input(MB_ACTION);
  assert(ok);

  assert(t.called);
  assert(t.selected_option == 1);
  assert(!biome_map_active());
  printf("UI selection returned option index %d.\n", t.selected_option);

  /* Save expected biome before gc_run_choose overwrites screen state */
  uint8_t expected_biome = s->biomes[t.selected_option];

  /* Send chosen option back to game-core */
  gc_choice choice = {GC_CH_BIOME, (int16_t)t.selected_option, 0, 0};
  ok = gc_run_choose(&run, &choice, &why);
  assert(ok);

  /* Verify game-core processed biome transition */
  assert(run.biome == expected_biome);
  assert(run.wave == 11);
  assert(gc_run_screen(&run)->kind == GC_SCREEN_COMMAND);

  /* Check that GC_EV_BIOME event was emitted */
  gc_event ev;
  bool got_ev_biome = false;
  while (gc_run_next_event(&run, &ev)) {
    if (ev.kind == GC_EV_BIOME) {
      got_ev_biome = true;
      assert(ev.a == expected_biome);
    }
  }
  assert(got_ev_biome);

  printf("map_ui_test PASSED: Biome selection UI correctly drove game-core biome transition!\n");
  return 0;
}
