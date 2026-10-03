/*
 * shop_ui.c: Bottom-screen citro2d rendering and touch bounds for the Shop and Reroll UI,
 * linked to game-core & reward-core state machine.
 */

#include "shop_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mui.h"

static shop_ui_state S;

bool shop_ui_hit_test(const shop_ui_rect* rect, float x, float y) {
  if (!rect) return false;
  return (x >= rect->x && x <= (rect->x + rect->w) &&
          y >= rect->y && y <= (rect->y + rect->h));
}

static void shop_ui_add_touch_slot(shop_target_type type, int index, float x, float y, float w, float h) {
  if (S.n_touch_slots >= SHOP_MAX_TOUCH_SLOTS) return;
  shop_touch_slot* slot = &S.touch_slots[S.n_touch_slots++];
  slot->type = type;
  slot->index = index;
  slot->bounds.x = x;
  slot->bounds.y = y;
  slot->bounds.w = w;
  slot->bounds.h = h;
}

static int shop_ui_num_rows(void) {
  int n_shop = S.screen_copy.n_shop;
  int rows = 1; /* Row 0: Reward options */
  if (n_shop > 0) {
    rows += (n_shop <= 4) ? 1 : 2; /* Shop Row 1 (and 2 if > 4 items) */
  }
  rows += 1; /* Row 3 (or 2/3): Action buttons */
  return rows;
}

static int shop_ui_row_max_cols(int row) {
  int num_r = shop_ui_num_rows();
  if (row == 0) {
    return S.screen_copy.n_options > 0 ? S.screen_copy.n_options : 1;
  }
  if (row == num_r - 1) {
    /* Action row: Reroll, Lock (if available), Skip */
    int count = 1; /* Skip button always present */
    if (S.screen_copy.reroll_cost >= 0) count++;
    if (S.has_lock_capsule) count++;
    return count;
  }
  /* Shop rows */
  int n_shop = S.screen_copy.n_shop;
  if (row == 1) {
    return (n_shop > 4) ? 4 : (n_shop > 0 ? n_shop : 1);
  }
  if (row == 2 && n_shop > 4) {
    return n_shop - 4 > 4 ? 4 : n_shop - 4;
  }
  return 1;
}

static void shop_ui_recalc_layout(void) {
  S.n_touch_slots = 0;

  /* 1. Reward Options (Row 0) */
  int n_opts = S.screen_copy.n_options;
  if (n_opts > 0) {
    float start_x = 10.0f;
    float card_w = (300.0f - (n_opts - 1) * 6.0f) / n_opts;
    if (card_w > 70.0f) card_w = 70.0f;
    float total_w = n_opts * card_w + (n_opts - 1) * 6.0f;
    start_x = (320.0f - total_w) / 2.0f;

    for (int i = 0; i < n_opts; i++) {
      float x = start_x + i * (card_w + 6.0f);
      float y = 20.0f;
      float h = 36.0f;
      shop_ui_add_touch_slot(SHOP_TARGET_REWARD, i, x, y, card_w, h);
    }
  }

  /* 2. Shop Options (Row 1 & Row 2) */
  int n_shop = S.screen_copy.n_shop;
  if (n_shop > 0) {
    int row1_count = (n_shop > 4) ? 4 : n_shop;
    float card_w = 70.0f;
    float card_h = 32.0f;

    /* Row 1 */
    float total_w1 = row1_count * card_w + (row1_count - 1) * 6.0f;
    float start_x1 = (320.0f - total_w1) / 2.0f;
    for (int i = 0; i < row1_count; i++) {
      float x = start_x1 + i * (card_w + 6.0f);
      float y = 62.0f;
      shop_ui_add_touch_slot(SHOP_TARGET_SHOP, i, x, y, card_w, card_h);
    }

    /* Row 2 */
    if (n_shop > 4) {
      int row2_count = n_shop - 4;
      if (row2_count > 4) row2_count = 4;
      float total_w2 = row2_count * card_w + (row2_count - 1) * 6.0f;
      float start_x2 = (320.0f - total_w2) / 2.0f;
      for (int i = 0; i < row2_count; i++) {
        float x = start_x2 + i * (card_w + 6.0f);
        float y = 98.0f;
        shop_ui_add_touch_slot(SHOP_TARGET_SHOP, 4 + i, x, y, card_w, card_h);
      }
    }
  }

  /* 3. Action Buttons (Bottom Row) */
  float btn_y = 142.0f;
  float btn_h = 28.0f;

  int action_btn_idx = 0;
  if (S.screen_copy.reroll_cost >= 0) {
    float x = 10.0f;
    float w = 95.0f;
    shop_ui_add_touch_slot(SHOP_TARGET_REROLL, action_btn_idx++, x, btn_y, w, btn_h);
  }

  if (S.has_lock_capsule) {
    float x = (S.screen_copy.reroll_cost >= 0) ? 112.0f : 10.0f;
    float w = 95.0f;
    shop_ui_add_touch_slot(SHOP_TARGET_LOCK, action_btn_idx++, x, btn_y, w, btn_h);
  }

  /* Skip/Continue button */
  float skip_x = 215.0f;
  float skip_w = 95.0f;
  shop_ui_add_touch_slot(SHOP_TARGET_SKIP, action_btn_idx++, skip_x, btn_y, skip_w, btn_h);
}

const char* shop_ui_luck_grade(int luck) {
  if (luck <= 0) return "D";
  if (luck <= 2) return "C";
  if (luck <= 4) return "B";
  if (luck <= 6) return "A";
  if (luck <= 9) return "S";
  if (luck <= 13) return "SS";
  return "SSS";
}

void shop_ui_open(const shop_ui_args* args, const shop_ui_events* ev) {
  memset(&S, 0, sizeof S);
  if (args && args->screen) {
    S.screen_copy = *args->screen;
    S.player_money = args->player_money;
    S.has_lock_capsule = args->has_lock_capsule;
    S.lock_capsule_active = args->lock_capsule_active;
    S.n_locked_tiers = args->n_locked_tiers;
    S.party_luck = args->party_luck;
    if (args->n_locked_tiers > 0) {
      memcpy(S.locked_tiers, args->locked_tiers, sizeof(args->locked_tiers));
    }
  }
  if (ev) S.events = *ev;

  S.row = 0;
  S.col = 0;
  S.active = true;

  shop_ui_recalc_layout();
}

void shop_ui_close(void) {
  S.active = false;
}

bool shop_ui_active(void) {
  return S.active;
}

static void shop_ui_trigger_current(void) {
  int action_row = shop_ui_num_rows() - 1;

  if (S.row == 0) {
    /* Reward Option */
    if (S.col >= 0 && S.col < S.screen_copy.n_options) {
      if (S.events.on_select_reward) {
        S.events.on_select_reward(S.events.ctx, S.col);
      }
    }
  } else if (S.row < action_row) {
    /* Shop Option */
    int shop_idx = (S.row == 1) ? S.col : (4 + S.col);
    if (shop_idx >= 0 && shop_idx < S.screen_copy.n_shop) {
      if (S.events.on_select_shop) {
        S.events.on_select_shop(S.events.ctx, shop_idx);
      }
    }
  } else if (S.row == action_row) {
    /* Action Buttons */
    int btn_map = S.col;
    bool has_reroll = (S.screen_copy.reroll_cost >= 0);
    bool has_lock = S.has_lock_capsule;

    if (has_reroll && btn_map == 0) {
      if (S.events.on_reroll) {
        S.events.on_reroll(S.events.ctx, S.lock_capsule_active);
      }
    } else if ((has_reroll && has_lock && btn_map == 1) || (!has_reroll && has_lock && btn_map == 0)) {
      /* Toggle Lock Capsule */
      S.lock_capsule_active = !S.lock_capsule_active;
    } else {
      /* Skip / Continue */
      if (S.events.on_skip) {
        S.events.on_skip(S.events.ctx);
      }
    }
  }
}

bool shop_ui_input(mui_button b) {
  if (!S.active) return false;

  int num_rows = shop_ui_num_rows();

  switch (b) {
    case MB_LEFT:
      if (S.col > 0) {
        S.col--;
        mui_sfx("ui/select");
      }
      return true;

    case MB_RIGHT: {
      int max_cols = shop_ui_row_max_cols(S.row);
      if (S.col < max_cols - 1) {
        S.col++;
        mui_sfx("ui/select");
      }
      return true;
    }

    case MB_UP:
      if (S.row > 0) {
        S.row--;
        int max_cols = shop_ui_row_max_cols(S.row);
        if (S.col >= max_cols) S.col = max_cols - 1;
        mui_sfx("ui/select");
      }
      return true;

    case MB_DOWN:
      if (S.row < num_rows - 1) {
        S.row++;
        int max_cols = shop_ui_row_max_cols(S.row);
        if (S.col >= max_cols) S.col = max_cols - 1;
        mui_sfx("ui/select");
      }
      return true;

    case MB_ACTION:
    case MB_SUBMIT:
      mui_sfx("ui/confirm");
      shop_ui_trigger_current();
      return true;

    case MB_CANCEL:
      /* Move to skip button or trigger skip */
      S.row = num_rows - 1;
      S.col = shop_ui_row_max_cols(S.row) - 1;
      mui_sfx("ui/select");
      return true;

    default:
      break;
  }

  return false;
}

bool shop_ui_touch(float touch_x, float touch_y) {
  if (!S.active) return false;

  for (int i = 0; i < S.n_touch_slots; i++) {
    const shop_touch_slot* slot = &S.touch_slots[i];
    if (shop_ui_hit_test(&slot->bounds, touch_x, touch_y)) {
      mui_sfx("ui/confirm");
      switch (slot->type) {
        case SHOP_TARGET_REWARD:
          S.row = 0;
          S.col = slot->index;
          if (S.events.on_select_reward) {
            S.events.on_select_reward(S.events.ctx, slot->index);
          }
          return true;

        case SHOP_TARGET_SHOP:
          if (slot->index < 4) {
            S.row = 1;
            S.col = slot->index;
          } else {
            S.row = 2;
            S.col = slot->index - 4;
          }
          if (S.events.on_select_shop) {
            S.events.on_select_shop(S.events.ctx, slot->index);
          }
          return true;

        case SHOP_TARGET_REROLL:
          S.row = shop_ui_num_rows() - 1;
          S.col = 0;
          if (S.events.on_reroll) {
            S.events.on_reroll(S.events.ctx, S.lock_capsule_active);
          }
          return true;

        case SHOP_TARGET_LOCK:
          S.row = shop_ui_num_rows() - 1;
          S.col = (S.screen_copy.reroll_cost >= 0) ? 1 : 0;
          S.lock_capsule_active = !S.lock_capsule_active;
          return true;

        case SHOP_TARGET_SKIP:
          S.row = shop_ui_num_rows() - 1;
          S.col = shop_ui_row_max_cols(S.row) - 1;
          if (S.events.on_skip) {
            S.events.on_skip(S.events.ctx);
          }
          return true;

        default:
          break;
      }
    }
  }

  return false;
}

void shop_ui_update(float dt_ms) {
  (void)dt_ms;
}

static const char* shop_ui_tier_name(int tier) {
  switch (tier) {
    case 0: return "Common";
    case 1: return "Great";
    case 2: return "Ultra";
    case 3: return "Rogue";
    case 4: return "Master";
    default: return "Common";
  }
}

static mui_style shop_ui_tier_style(int tier) {
  switch (tier) {
    case 0: return TS_WINDOW;
    case 1: return TS_SUMMARY_BLUE;
    case 2: return TS_SUMMARY_GOLD;
    case 3: return TS_SUMMARY_PINK;
    case 4: return TS_PARTY_RED;
    default: return TS_WINDOW;
  }
}

void shop_ui_draw(void) {
  if (!S.active) return;

  /* Background window */
  mui_window(0, 0, 320, 180);

  /* Title bar, Luck Grade & Money balance */
  mui_text(TS_HEADER_LABEL, 10, 4, "REWARDS & SHOP");

  char luck_buf[32];
  snprintf(luck_buf, sizeof luck_buf, "Luck: %s", shop_ui_luck_grade(S.party_luck));
  mui_text(TS_LUCK_VALUE, 140, 4, luck_buf);

  char money_buf[32];
  snprintf(money_buf, sizeof money_buf, "Money: ₽%.0f", S.player_money);
  mui_text_o(TS_MONEY, 310, 4, 1.0f, 0.0f, money_buf);

  /* 1. Draw Reward Options */
  int n_opts = S.screen_copy.n_options;
  for (int i = 0; i < S.n_touch_slots; i++) {
    const shop_touch_slot* slot = &S.touch_slots[i];
    if (slot->type == SHOP_TARGET_REWARD) {
      int idx = slot->index;
      if (idx >= 0 && idx < n_opts) {
        const gc_offer* offer = &S.screen_copy.options[idx];
        float x = slot->bounds.x;
        float y = slot->bounds.y;
        float w = slot->bounds.w;
        float h = slot->bounds.h;

        /* Window box */
        mui_window(x, y, w, h);

        /* Highlight box if focused */
        if (S.row == 0 && S.col == idx) {
          mui_window(x - 2, y - 2, w + 4, h + 4);
        }

        /* Tier label */
        mui_style tstyle = shop_ui_tier_style(offer->tier);
        mui_text(tstyle, x + 4, y + 2, shop_ui_tier_name(offer->tier));

        /* Key / Item Name */
        const char* name = offer->key ? offer->key : "REWARD";
        mui_text(TS_SMALLER_WINDOW_ALT, x + 4, y + 18, name);
      }
    }
  }

  /* 2. Draw Shop Options */
  int n_shop = S.screen_copy.n_shop;
  for (int i = 0; i < S.n_touch_slots; i++) {
    const shop_touch_slot* slot = &S.touch_slots[i];
    if (slot->type == SHOP_TARGET_SHOP) {
      int idx = slot->index;
      if (idx >= 0 && idx < n_shop) {
        const gc_offer* offer = &S.screen_copy.shop[idx];
        float x = slot->bounds.x;
        float y = slot->bounds.y;
        float w = slot->bounds.w;
        float h = slot->bounds.h;

        mui_window(x, y, w, h);

        int current_shop_row = (idx < 4) ? 1 : 2;
        int current_shop_col = (idx < 4) ? idx : (idx - 4);
        if (S.row == current_shop_row && S.col == current_shop_col) {
          mui_window(x - 2, y - 2, w + 4, h + 4);
        }

        const char* name = offer->key ? offer->key : "ITEM";
        mui_style name_style = (S.player_money < offer->cost) ? TS_SUMMARY_GRAY : TS_WINDOW;
        mui_text(name_style, x + 4, y + 2, name);

        char cost_buf[24];
        snprintf(cost_buf, sizeof cost_buf, "₽%.0f", offer->cost);
        mui_text(TS_MONEY_WINDOW, x + 4, y + 16, cost_buf);
      }
    }
  }

  /* 3. Draw Lock Capsule Visuals & Indicator */
  if (S.has_lock_capsule || S.n_locked_tiers > 0) {
    mui_text(TS_HEADER_LABEL, 10, 128, S.lock_capsule_active ? "Lock Capsule: ACTIVE" : "Lock Capsule: READY");
  }

  /* 4. Draw Action Buttons */
  for (int i = 0; i < S.n_touch_slots; i++) {
    const shop_touch_slot* slot = &S.touch_slots[i];
    float x = slot->bounds.x;
    float y = slot->bounds.y;
    float w = slot->bounds.w;
    float h = slot->bounds.h;

    int action_row = shop_ui_num_rows() - 1;

    if (slot->type == SHOP_TARGET_REROLL) {
      mui_window(x, y, w, h);
      if (S.row == action_row && S.col == 0) {
        mui_window(x - 2, y - 2, w + 4, h + 4);
      }
      char reroll_buf[32];
      if (S.screen_copy.reroll_cost >= 0) {
        snprintf(reroll_buf, sizeof reroll_buf, "Reroll ₽%.0f", S.screen_copy.reroll_cost);
      } else {
        snprintf(reroll_buf, sizeof reroll_buf, "Reroll N/A");
      }
      mui_text(TS_WINDOW, x + 6, y + 6, reroll_buf);
    } else if (slot->type == SHOP_TARGET_LOCK) {
      mui_window(x, y, w, h);
      int lock_col = (S.screen_copy.reroll_cost >= 0) ? 1 : 0;
      if (S.row == action_row && S.col == lock_col) {
        mui_window(x - 2, y - 2, w + 4, h + 4);
      }
      mui_text(TS_WINDOW, x + 6, y + 6, S.lock_capsule_active ? "Lock: ON" : "Lock: OFF");
    } else if (slot->type == SHOP_TARGET_SKIP) {
      mui_window(x, y, w, h);
      int skip_col = shop_ui_row_max_cols(action_row) - 1;
      if (S.row == action_row && S.col == skip_col) {
        mui_window(x - 2, y - 2, w + 4, h + 4);
      }
      mui_text(TS_WINDOW, x + 16, y + 6, "Skip / Cont");
    }
  }
}
