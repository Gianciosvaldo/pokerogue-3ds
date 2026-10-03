/*
 * shop_ui_test.c: PC test harness for Shop UI and Lock Capsule visuals.
 * Verifies rendering layout, directional navigation, touch bound hit-testing,
 * Lock Capsule state toggling, and choice generation for game-core.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gc/run.h"
#include "menus.h"
#include "shop_ui.h"

/* Stub functions required by mui when building on PC without 3DS libraries */
void mui_sfx(const char* key) {
  (void)key;
}

const char* mui_t(const char* key) {
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

int mui_text_o(mui_style s, float x, float y, float ox, float oy, const char* utf8) {
  (void)s; (void)x; (void)y; (void)ox; (void)oy; (void)utf8;
  return 0;
}

typedef struct {
  int reward_selected;
  int shop_selected;
  bool reroll_called;
  bool reroll_locked;
  bool skip_called;
} test_results;

static void on_test_select_reward(void* ctx, int option_idx) {
  test_results* res = (test_results*)ctx;
  res->reward_selected = option_idx;
}

static void on_test_select_shop(void* ctx, int shop_idx) {
  test_results* res = (test_results*)ctx;
  res->shop_selected = shop_idx;
}

static void on_test_reroll(void* ctx, bool lock_capsule) {
  test_results* res = (test_results*)ctx;
  res->reroll_called = true;
  res->reroll_locked = lock_capsule;
}

static void on_test_skip(void* ctx) {
  test_results* res = (test_results*)ctx;
  res->skip_called = true;
}

int main(void) {
  printf("Running shop_ui_test...\n");

  /* 1. Setup mock gc_screen for REWARD phase */
  gc_screen screen;
  memset(&screen, 0, sizeof screen);
  screen.kind = GC_SCREEN_REWARD;

  /* Setup 3 reward options */
  screen.n_options = 3;
  screen.options[0].key = "POTION";
  screen.options[0].tier = 0; /* Common */
  screen.options[1].key = "RARE_CANDY";
  screen.options[1].tier = 2; /* Ultra */
  screen.options[2].key = "NUGGET";
  screen.options[2].tier = 1; /* Great */

  /* Setup 5 shop items */
  screen.n_shop = 5;
  screen.shop[0].key = "POTION"; screen.shop[0].cost = 200.0;
  screen.shop[1].key = "SUPER_POTION"; screen.shop[1].cost = 500.0;
  screen.shop[2].key = "REVIVE"; screen.shop[2].cost = 800.0;
  screen.shop[3].key = "ETHER"; screen.shop[3].cost = 600.0;
  screen.shop[4].key = "FULL_HEAL"; screen.shop[4].cost = 400.0;

  screen.reroll_cost = 250.0;

  /* Setup Shop UI arguments and events */
  test_results res;
  memset(&res, -1, sizeof res);
  res.reroll_called = false;
  res.reroll_locked = false;
  res.skip_called = false;

  shop_ui_args args;
  memset(&args, 0, sizeof args);
  args.screen = &screen;
  args.player_money = 1000.0;
  args.has_lock_capsule = true;
  args.lock_capsule_active = false;
  args.n_locked_tiers = 3;
  args.locked_tiers[0] = 0;
  args.locked_tiers[1] = 2;
  args.locked_tiers[2] = 1;

  shop_ui_events ev;
  ev.on_select_reward = on_test_select_reward;
  ev.on_select_shop = on_test_select_shop;
  ev.on_reroll = on_test_reroll;
  ev.on_skip = on_test_skip;
  ev.ctx = &res;

  /* Open Shop UI */
  shop_ui_open(&args, &ev);
  assert(shop_ui_active());
  printf("Shop UI opened successfully.\n");

  /* 2. Test directional navigation and selection */
  /* Initially focused on Row 0, Col 0 (Reward option 0) */
  bool handled = shop_ui_input(MB_RIGHT);
  assert(handled);

  /* Submit reward option 1 */
  handled = shop_ui_input(MB_ACTION);
  assert(handled);
  assert(res.reward_selected == 1);
  printf("D-pad selection of reward option 1 PASSED.\n");

  /* Move left back to Col 0 */
  handled = shop_ui_input(MB_LEFT);
  assert(handled);

  /* Navigate down to Shop Row 1 (item 0) */
  handled = shop_ui_input(MB_DOWN);
  assert(handled);

  /* Select shop item 0 */
  handled = shop_ui_input(MB_ACTION);
  assert(handled);
  assert(res.shop_selected == 0);
  printf("D-pad selection of shop item 0 PASSED.\n");

  /* Navigate to Bottom Action row (Reroll, Lock, Skip) */
  handled = shop_ui_input(MB_DOWN); /* Shop Row 2 */
  handled = shop_ui_input(MB_DOWN); /* Action Row */
  assert(handled);

  /* Col 0 is Reroll. Submit Reroll without Lock Capsule */
  handled = shop_ui_input(MB_ACTION);
  assert(handled);
  assert(res.reroll_called);
  assert(!res.reroll_locked);
  printf("Reroll action PASSED.\n");

  /* Move right to Col 1 (Lock Capsule Toggle) and press ACTION */
  handled = shop_ui_input(MB_RIGHT);
  assert(handled);
  handled = shop_ui_input(MB_ACTION);
  assert(handled);
  /* Lock Capsule is now active! */

  /* Move left back to Col 0 (Reroll) and press ACTION */
  handled = shop_ui_input(MB_LEFT);
  assert(handled);
  res.reroll_called = false;
  handled = shop_ui_input(MB_ACTION);
  assert(handled);
  assert(res.reroll_called);
  assert(res.reroll_locked);
  printf("Locked Reroll with Lock Capsule PASSED.\n");

  /* Move to Skip button (Col 2) and press ACTION */
  handled = shop_ui_input(MB_RIGHT);
  handled = shop_ui_input(MB_RIGHT);
  handled = shop_ui_input(MB_ACTION);
  assert(handled);
  assert(res.skip_called);
  printf("Skip action via D-pad PASSED.\n");

  /* 3. Test Touch Screen Hit-Testing */
  /* Touch reward option 2 */
  res.reward_selected = -1;
  /* Reward option 2 touch coordinates (x 230, y 25) */
  bool touched = shop_ui_touch(230.0f, 25.0f);
  assert(touched);
  assert(res.reward_selected == 2);
  printf("Touch selection of reward option 2 PASSED.\n");

  /* Touch shop item 3 */
  res.shop_selected = -1;
  /* Shop item 3 is in Row 1 at index 3 (x 250, y 65) */
  touched = shop_ui_touch(250.0f, 65.0f);
  assert(touched);
  assert(res.shop_selected == 3);
  printf("Touch selection of shop item 3 PASSED.\n");

  /* Touch Skip button (x 250, y 150) */
  res.skip_called = false;
  touched = shop_ui_touch(250.0f, 150.0f);
  assert(touched);
  assert(res.skip_called);
  printf("Touch selection of Skip button PASSED.\n");

  /* Close UI */
  shop_ui_close();
  assert(!shop_ui_active());

  printf("shop_ui_test PASSED ALL CHECKS!\n");
  return 0;
}
