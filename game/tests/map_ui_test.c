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

/* the last window drawn, to see where the list is */
static float g_wx, g_wy, g_ww, g_wh;
void mui_window(float x, float y, float w, float h) {
  g_wx = x;
  g_wy = y;
  g_ww = w;
  g_wh = h;
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


  /* ---- taps on the bottom screen's copy of the list (canvas coordinates: the bottom canvas starts 30 px down the screen) */
  int taps = 0;
  {
    const uint8_t three[3] = {BIOME_ID_PLAINS, BIOME_ID_FOREST, BIOME_ID_CAVE};
    test_ctx t2 = {-1, false};
    biome_map_args ba2 = {three, 3};
    biome_map_events be2 = {on_biome_chosen, &t2};
    assert(!biome_map_touch(100, 100)); /* nothing is open */
    taps++;
    biome_map_open(&ba2, &be2);
    /* the top screen's list is where it was, and the bottom screen's sits on the screen's bottom edge, both as wide as their labels */
    biome_map_draw();
    assert(g_wy + g_wh == 180 - 48 && g_wx + g_ww == 319);
    biome_map_draw_bottom();
    const float left = g_wx, top = g_wy, w = g_ww, h = g_wh;
    assert(top + h == 208 && left + w == 319 && h == 4 * 16 - 2);
    const float cx = left + w / 2;
    /* a tap on each row is that row */
    for (int row = 2; row >= 0; row--) {
      t2.called = false;
      biome_map_open(&ba2, &be2);
      assert(biome_map_touch(cx, top + 7 + 16 * row + 8) && t2.called && t2.selected_option == row && !biome_map_active());
      taps++;
    }
    /* the rows' edges: 7 px under the window's top to 16 px per row */
    const struct { float y; int row; } edges[] = {{top + 7, 0}, {top + 7 + 15.5f, 0}, {top + 7 + 16, 1}, {top + 7 + 47.5f, 2}};
    for (size_t i = 0; i < sizeof edges / sizeof *edges; i++) {
      t2.called = false;
      biome_map_open(&ba2, &be2);
      assert(biome_map_touch(cx, edges[i].y) && t2.selected_option == edges[i].row);
      taps++;
    }
    /* the padding, outside the window and the sides take nothing, and the list stays open */
    t2.called = false;
    biome_map_open(&ba2, &be2);
    const struct { float x, y; } misses[] = {{cx, top + 6.5f}, {cx, top + 7 + 48}, {cx, top + h}, {left - 1, top + 20}, {319, top + 20}, {cx, top - 1}, {cx, 100}, {5, 5}};
    for (size_t i = 0; i < sizeof misses / sizeof *misses; i++) {
      assert(!biome_map_touch(misses[i].x, misses[i].y) && !t2.called && biome_map_active());
      taps++;
    }
    /* a tap moves the cursor like the D-pad does, and A after it picks that row */
    assert(biome_map_touch(left, top + 7 + 16 + 8) == true && t2.selected_option == 1);
    taps++;
    assert(!biome_map_touch(cx, top + 20)); /* closed again: nothing */
    taps++;
  }
  printf("%d taps on the bottom list checked.\n", taps);

  printf("map_ui_test PASSED: Biome selection UI correctly drove game-core biome transition!\n");
  return 0;
}
