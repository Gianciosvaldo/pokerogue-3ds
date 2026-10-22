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
#include "gm_item_ui.h"
#include "menus.h"
#include "pr/reward.h"
#include "shop_ui.h"
#include "reward_host.h"
#include "ui.h"

static char last_wrapped[512];
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

int mui_text_c(mui_style s, float x, float y, const char* utf8, rr_color c, rr_color shadow) {
  (void)s; (void)x; (void)y; (void)utf8; (void)c; (void)shadow;
  return 0;
}

rr_color mui_style_color(mui_style s, bool shadow) {
  (void)s; (void)shadow;
  return 0;
}

bool mui_image(const char* key, const char* frame, float x, float y, float ox, float oy) {
  (void)key; (void)frame; (void)x; (void)y; (void)ox; (void)oy;
  return true;
}

int mui_wrap(mui_style s, const char* in, float width, char* out, size_t n) {
  (void)s; (void)width;
  snprintf(last_wrapped,sizeof last_wrapped,"%s",in);
  snprintf(out, n, "%s", in);
  return 1;
}

void rr_rect(float x, float y, float w, float h, rr_color c) {
  (void)x; (void)y; (void)w; (void)h; (void)c;
}

int mui_text_o(mui_style s, float x, float y, float ox, float oy, const char* utf8) {
  (void)s; (void)x; (void)y; (void)ox; (void)oy; (void)utf8;
  return 0;
}

/* the UI stack and the biome map, for the reward screen's host (source/reward_host.c): the host pushes its screen, and the test taps it */
static ui_screen* g_pushed;
void ui_push(ui_screen* s) { g_pushed = s; }
void ui_pop(void) { g_pushed = NULL; }
ui_screen* ui_top(void) { return g_pushed; }
bool ui_space(ui_space_id sp) { (void)sp; return true; }
void mui_set_origin(rr_screen s, int px, int py) { (void)s; (void)px; (void)py; }
void mui_tick(float dt_ms) { (void)dt_ms; }
void biome_map_open(const biome_map_args* args, const biome_map_events* ev) { (void)args; (void)ev; }
void biome_map_close(void) {}
static bool g_map_on, g_map_hit;
static int g_map_touches, g_map_top_draws, g_map_bottom_draws;
static float g_map_tx, g_map_ty;
bool biome_map_active(void) { return g_map_on; }
bool biome_map_input(mui_button b) { (void)b; return false; }
void biome_map_update(float dt_ms) { (void)dt_ms; }
void biome_map_draw(void) { g_map_top_draws++; }
void biome_map_draw_bottom(void) { g_map_bottom_draws++; }
bool biome_map_touch(float x, float y) {
  g_map_touches++;
  g_map_tx = x;
  g_map_ty = y;
  return g_map_hit;
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

  /* Test Party Luck Grade calculations */
  static const char* grades[]={"D","C","C+","B-","B","B+","A-","A","A+","A++","S","S+","SS","SS+","SSS"};
  for(int i=0;i<15;i++)assert(!strcmp(shop_ui_luck_grade(i),grades[i]));
  assert(!strcmp(shop_ui_luck_grade(-1),"D"));
  assert(!strcmp(shop_ui_luck_grade(18),"SSS"));
  printf("Party Luck Grade mapping (D through SSS) PASSED.\n");

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
  args.party_luck = 14; /* SSS Grade */

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

  /* 2. Test directional navigation and selection. The rows are in the order they are drawn: the shop, the reward options, the
   * buttons; the cursor starts on the first reward option. */
  bool handled = shop_ui_input(MB_RIGHT);
  assert(handled);

  /* Submit reward option 1 */
  handled = shop_ui_input(MB_ACTION);
  assert(handled);
  assert(res.reward_selected == 1);
  printf("D-pad selection of reward option 1 PASSED.\n");

  /* Move left back to option 0 and up to the shop's row (the item nearest above it) */
  handled = shop_ui_input(MB_LEFT);
  assert(handled);
  res.shop_selected = -1;
  handled = shop_ui_input(MB_UP);
  assert(handled);
  handled = shop_ui_input(MB_ACTION);
  assert(handled);
  assert(res.shop_selected == 0);
  /* Up again: there is no row above the shop's; nothing changes */
  handled = shop_ui_input(MB_UP);
  assert(handled);
  res.shop_selected = -1;
  handled = shop_ui_input(MB_ACTION);
  assert(res.shop_selected == 0);
  /* Right along the shop's row to item 4 (the last of five) and no further */
  for (int i = 0; i < 6; i++) shop_ui_input(MB_RIGHT);
  handled = shop_ui_input(MB_ACTION);
  assert(res.shop_selected == 4);
  printf("D-pad selection of shop items PASSED.\n");

  /* Down twice: the reward options, then the buttons (Reroll, Lock Rarities, Continue) */
  shop_ui_input(MB_LEFT);
  shop_ui_input(MB_LEFT);
  shop_ui_input(MB_LEFT);
  shop_ui_input(MB_LEFT);
  shop_ui_input(MB_DOWN);
  res.reward_selected = -1;
  shop_ui_input(MB_ACTION);
  assert(res.reward_selected == 0); /* the first option is under the first shop item */
  handled = shop_ui_input(MB_DOWN);
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

  /* Move to the Continue button (Col 2) and press ACTION */
  handled = shop_ui_input(MB_RIGHT);
  handled = shop_ui_input(MB_RIGHT);
  handled = shop_ui_input(MB_ACTION);
  assert(handled);
  assert(res.skip_called);
  printf("Skip action via D-pad PASSED.\n");

  /* B / cancel goes to the Continue button */
  res.skip_called = false;
  shop_ui_input(MB_UP);
  shop_ui_input(MB_CANCEL);
  shop_ui_input(MB_ACTION);
  assert(res.skip_called);
  printf("Cancel goes to Continue PASSED.\n");

  /* 3. Test Touch Screen Hit-Testing (five shop items and three options: the shop row is at y 24.5..64.5, the options at 65.5..105.5, the buttons at 119..137) */
  res.reward_selected = -1;
  bool touched = shop_ui_touch(264.0f, 85.0f);
  assert(touched);
  assert(res.reward_selected == 2);
  printf("Touch selection of reward option 2 PASSED.\n");

  /* Shop item 3 of five: seven columns of 44.57 centred, so its centre is x 204.6 */
  res.shop_selected = -1;
  touched = shop_ui_touch(205.0f, 44.0f);
  assert(touched);
  assert(res.shop_selected == 3);
  printf("Touch selection of shop item 3 PASSED.\n");

  /* The Continue button (x 246..316) */
  res.skip_called = false;
  touched = shop_ui_touch(280.0f, 128.0f);
  assert(touched);
  assert(res.skip_called);
  printf("Touch selection of Continue button PASSED.\n");

  /* Touching nothing selects nothing */
  assert(!shop_ui_touch(2.0f, 2.0f));

  /* The reward screen's host (rh_touch): a tap at physical bottom-screen (x, y) is the canvas' (x, y - 30) */
  rh_open_shop(&args, &ev);
  assert(g_pushed && g_pushed->touch);
  res.reward_selected = -1;
  assert(g_pushed->touch(g_pushed, 264.0f, 85.0f + 30.0f));
  assert(res.reward_selected == 2);
  res.skip_called = false;
  assert(g_pushed->touch(g_pushed, 280.0f, 128.0f + 30.0f));
  assert(res.skip_called);
  res.reward_selected = -1;
  assert(!g_pushed->touch(g_pushed, 2.0f, 2.0f + 30.0f)); /* nothing there */
  assert(res.reward_selected == -1);
  rh_close();
  assert(g_pushed == NULL);
  printf("The reward screen takes taps through its host PASSED.\n");

  gc_screen tm_screen={.kind=GC_SCREEN_REWARD,.n_options=1,.reroll_cost=-1};
  tm_screen.options[0]=(gc_offer){.key="TM_GREAT",.arg=MOVE_ID_UPROAR};
  shop_ui_args tm_args={.screen=&tm_screen};rh_open_shop(&tm_args,&ev);
  shop_ui_draw();assert(strstr(last_wrapped,"Teach Uproar")&&!strstr(last_wrapped,"(P)"));
  res.reward_selected=-1;assert(g_pushed->input(g_pushed,BTN_STATS));shop_ui_draw();
  assert(!strcmp(last_wrapped,"move:uproar.effect")&&res.reward_selected==-1);
  assert(g_pushed->release);g_pushed->release(g_pushed,BTN_STATS);shop_ui_draw();assert(strstr(last_wrapped,"Teach Uproar"));
  tm_screen.options[0].key="TM_ULTRA";tm_screen.options[0].arg=MOVE_ID_RETURN;rh_open_shop(&tm_args,&ev);shop_ui_draw();
  assert(strstr(last_wrapped,"Teach Return")&&!strstr(last_wrapped,"return.name"));rh_close();

  /* With the biome map up the host hands a tap to its list (canvas y = physical y - 30) and the shop gets none; the list is drawn on
   * the top screen and, for the taps, on the bottom one */
  g_map_on = true;
  g_map_hit = true;
  rh_open_biome_map(&(biome_map_args){0}, &(biome_map_events){0});
  assert(g_pushed && g_pushed->touch);
  res.reward_selected = -1;
  assert(g_pushed->touch(g_pushed, 200.0f, 150.0f));
  assert(g_map_touches == 1 && g_map_tx == 200.0f && g_map_ty == 120.0f && res.reward_selected == -1);
  g_map_hit = false;
  assert(!g_pushed->touch(g_pushed, 10.0f, 30.0f) && g_map_touches == 2 && g_map_ty == 0.0f);
  g_pushed->draw(g_pushed, RR_TOP);
  assert(g_map_top_draws == 1 && g_map_bottom_draws == 0);
  g_pushed->draw(g_pushed, RR_BOTTOM);
  assert(g_map_top_draws == 1 && g_map_bottom_draws == 1);
  g_map_on = false;
  rh_close();
  assert(g_pushed == NULL);
  printf("The biome map takes taps through the reward host PASSED.\n");
  rh_open_shop(&args, &ev); /* the checks below go on with the shop open */

  /* Drawing every state must not crash (the stubs draw nothing) */
  shop_ui_draw();
  shop_ui_input(MB_DOWN);
  shop_ui_draw();

  /* Close UI */
  shop_ui_close();
  assert(!shop_ui_active());

  /* 3b. The layout for many shapes: every slot is inside the screen and no two overlap */
  {
    static const int shapes[][2] = {{0, 3}, {0, 1}, {1, 3}, {5, 3}, {7, 4}, {7, 5}, {8, 4}, {14, 3}, {14, 4}, {14, 5}, {14, 8}, {3, 8}, {0, 0}};
    for (size_t k = 0; k < sizeof shapes / sizeof shapes[0]; k++) {
      for (int caps = 0; caps < 2; caps++) {
        gc_screen sc;
        memset(&sc, 0, sizeof sc);
        sc.kind = GC_SCREEN_REWARD;
        sc.n_shop = shapes[k][0];
        sc.n_options = shapes[k][1];
        for (int i = 0; i < sc.n_shop; i++) {
          sc.shop[i].key = "POTION";
          sc.shop[i].cost = 100.0 * (i + 1);
        }
        for (int i = 0; i < sc.n_options; i++) sc.options[i].key = "TM_COMMON", sc.options[i].arg = 201, sc.options[i].is_purchase = true;
        sc.reroll_cost = caps ? -1.0 : 250.0;
        shop_ui_args a2;
        memset(&a2, 0, sizeof a2);
        a2.screen = &sc;
        a2.has_lock_capsule = caps;
        shop_ui_events e2;
        memset(&e2, 0, sizeof e2);
        shop_ui_open(&a2, &e2);
        const int n = shop_ui_slot_count();
        assert(n == sc.n_shop + sc.n_options + 2); /* Continue, and Reroll or Lock Rarities */
        for (int i = 0; i < n; i++) {
          const shop_ui_rect r = shop_ui_slot(i)->bounds;
          if (r.x < 0 || r.y < 0 || r.x + r.w > 320 || r.y + r.h > 180) {
            fprintf(stderr, "shape %d shop %d options: slot %d out of the screen (%.1f %.1f %.1f %.1f)\n", shapes[k][0], shapes[k][1], i, r.x, r.y, r.w, r.h);
            return 1;
          }
          for (int j = i + 1; j < n; j++) {
            const shop_ui_rect q = shop_ui_slot(j)->bounds;
            if (r.x < q.x + q.w && q.x < r.x + r.w && r.y < q.y + q.h && q.y < r.y + r.h) {
              fprintf(stderr, "shape %d shop %d options: slots %d and %d overlap\n", shapes[k][0], shapes[k][1], i, j);
              return 1;
            }
          }
        }
        /* the buttons are below the rows */
        shop_ui_draw();
        shop_ui_close();
      }
    }
    printf("Layout of 13 shapes x 2 button sets: inside the screen, no overlaps PASSED.\n");
  }

  /* 4. gen/gm_item_ui.c: every item the rolls can make has an icon, a name and a description */
  {
    const gm_item_ui* p = gm_item_ui_find("POTION", -1);
    assert(p && !strcmp(p->icon, "potion") && !strcmp(p->name, "Potion") && p->desc[0] && !p->dynamic);
    const gm_item_ui* n = gm_item_ui_find("NUGGET", -1);
    assert(n && n->dynamic && strstr(n->desc, "{money}"));
    assert(!gm_item_ui_find("NO_SUCH_ITEM", -1));
    assert(!gm_item_ui_find("POTION", 3));
    int plain = 0, gen = 0;
    for (int i = 0; i < PR_ITEM_COUNT_; i++) {
      if (pr_items[i].gen == PR_GEN_NONE) {
        const gm_item_ui* r = gm_item_ui_find(pr_item_keys[i], -1);
        if (!r) {
          fprintf(stderr, "no row for %s\n", pr_item_keys[i]);
          return 1;
        }
        assert(r->icon[0] && r->name[0]);
        plain++;
      }
    }
    for (int i = 0; i < pr_gen_type_count; i++) {
      const gm_item_ui* r = gm_item_ui_find(pr_item_keys[pr_gen_types[i].item], pr_gen_types[i].arg);
      if (!r) {
        fprintf(stderr, "no row for %s arg %d\n", pr_item_keys[pr_gen_types[i].item], pr_gen_types[i].arg);
        return 1;
      }
      /* the dump's English name is the one reward-core has */
      assert(!strcmp(r->name, pr_reward_strings[pr_gen_types[i].name]));
      gen++;
    }
    printf("Item data: %d plain items and %d generated types, all with a row PASSED.\n", plain, gen);
  }

  printf("shop_ui_test PASSED ALL CHECKS!\n");
  return 0;
}
