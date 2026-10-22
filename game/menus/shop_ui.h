/*
 * Shop UI & Lock Capsule Visuals for PokeRogue 3DS (game/menus/shop_ui.h).
 * Handles bottom-screen rendering, touch bounds, cursor navigation, and
 * state machine linking for the Shop, Reward selection, Lock Capsule, and Reroll UI.
 */
#ifndef GAME_MENUS_SHOP_UI_H
#define GAME_MENUS_SHOP_UI_H

#include <stdbool.h>
#include <stdint.h>

#include "gc/run.h"
#include "mui.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct shop_ui_events {
  /* Callbacks when player makes a selection */
  void (*on_select_reward)(void* ctx, int option_idx);
  void (*on_select_shop)(void* ctx, int shop_idx);
  void (*on_reroll)(void* ctx, bool lock_capsule);
  void (*on_skip)(void* ctx);
  void (*on_manage_items)(void* ctx); /* Manage Items: the party screen in MODIFIER_TRANSFER mode */
  void* ctx;
} shop_ui_events;

typedef struct {
  const gc_screen* screen;  /* pointer to current GC_SCREEN_REWARD screen */
  double player_money;      /* player's current money balance */
  bool has_lock_capsule;    /* whether Lock Capsule is unlocked/owned */
  bool lock_capsule_active; /* whether Lock Capsule tier lock is currently active */
  int8_t locked_tiers[GC_MAX_OPTIONS]; /* tiers locked by Lock Capsule */
  int n_locked_tiers;
  int party_luck;           /* total party luck value */
  bool party_has_held_item; /* a transferable held item in the party: the Manage Items button shows */
  int wave;                 /* the wave and the biome's name, for the top line ("Plains - 147") */
  char location[48];
  /* what the money items grant now (getWaveMoneyAmount with the Amulet Coin's multiplier): NUGGET, BIG_NUGGET, RELIC_GOLD */
  double money_item_amount[3];
} shop_ui_args;

typedef struct {
  float x, y, w, h;
} shop_ui_rect;

typedef enum {
  SHOP_TARGET_NONE = 0,
  SHOP_TARGET_REWARD,
  SHOP_TARGET_SHOP,
  SHOP_TARGET_REROLL,
  SHOP_TARGET_LOCK,
  SHOP_TARGET_SKIP,
  SHOP_TARGET_TRANSFER,
} shop_target_type;

typedef struct {
  shop_target_type type;
  int index; /* option or shop item index */
  shop_ui_rect bounds;
} shop_touch_slot;

#define SHOP_MAX_TOUCH_SLOTS 32

typedef struct {
  bool active, move_details;
  gc_screen screen_copy;
  double player_money;
  bool has_lock_capsule;
  bool lock_capsule_active;
  int8_t locked_tiers[GC_MAX_OPTIONS];
  int n_locked_tiers;
  int party_luck;
  bool party_has_held_item;
  int wave;
  char location[48];
  double money_item_amount[3];

  /* Cursor navigation state: the rows in the order they are drawn (shop rows, reward rows, the buttons) */
  int row;
  int col;    /* column index in current row */
  int n_rows;

  /* Touch slots */
  shop_touch_slot touch_slots[SHOP_MAX_TOUCH_SLOTS];
  int n_touch_slots;

  shop_ui_events events;
} shop_ui_state;

void shop_ui_open(const shop_ui_args* args, const shop_ui_events* ev);
void shop_ui_close(void);
bool shop_ui_active(void);
bool shop_ui_input(mui_button b);
void shop_ui_release(mui_button b);
bool shop_ui_touch(float touch_x, float touch_y);
void shop_ui_update(float dt_ms);
void shop_ui_draw(void);

/* The touch slots as laid out (for the test of the layout) */
int shop_ui_slot_count(void);
const shop_touch_slot* shop_ui_slot(int i);

/* Helper for hit testing touch bounds */
bool shop_ui_hit_test(const shop_ui_rect* rect, float x, float y);

/* Converts party luck integer to letter grade (D through SSS) */
const char* shop_ui_luck_grade(int luck);

#ifdef __cplusplus
}
#endif

#endif
