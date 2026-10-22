/*
 * shop_ui.c: the reward and shop screen on the bottom screen (upstream's ModifierSelectUiHandler), linked to game-core and
 * reward-core state through the events in shop_ui.h.
 *
 * Layout, top to bottom, on upstream's 320 x 180 canvas: the top line (place and wave, luck, money); the shop's rows (7 items
 * a row, SHOP_OPTIONS_ROW_LIMIT) as an icon and a price; the reward options, each an icon with its name beside it; the buttons
 * (Reroll, Lock Rarities, Continue); and the description window with the name and description of what the cursor is on.
 * Upstream draws the shop's names in 6 px emerald under each icon; on the 3DS that cannot be read, so the name of an item in
 * the shop is in the description window while the cursor is on it. All text is the 8 px pixel font.
 * The icons, names and descriptions are upstream's (gen/gm_item_ui.c, dumped from its modifier types).
 *
 * Not here: Manage Items and Check Team (the held-item transfer and the party view from the shop are not ported).
 */

#include "luck_meter.h"
#include "shop_ui.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gm_item_ui.h"
#include "pr_data.h"
#include "menus.h"
#include "mui.h"

#define FONT TSX_PIXEL_8

#define SHOP_COLS 7 /* SHOP_OPTIONS_ROW_LIMIT */
#define OPT_MAX_COLS 4
#define HEADER_H 12
#define BTN_H 18
#define MARGIN 4

typedef enum { ROW_SHOP, ROW_OPTION, ROW_BUTTONS } row_kind;

typedef struct {
  row_kind kind;
  int first;  /* index of the first item (shop or option) of the row */
  int count;
  int slot0;  /* the row's first slot in touch_slots */
  float y, h;
} shop_row;

#define MAX_ROWS 6

static shop_ui_state S;
static shop_row rows[MAX_ROWS];
static float desc_y, desc_h;

bool shop_ui_hit_test(const shop_ui_rect* rect, float x, float y) {
  if (!rect) return false;
  return (x >= rect->x && x <= (rect->x + rect->w) &&
          y >= rect->y && y <= (rect->y + rect->h));
}

const char* shop_ui_luck_grade(int luck) { return luck_grade(luck); }

/* ---------------------------------------------------------------- what an offer shows */

typedef struct {
  const char* icon;  /* frame of the items atlas, or NULL */
  char name[64];
  char desc[200];
} offer_view;

/* "SUPER_POTION" -> "Super Potion" for an item the dump does not know */
static void humanize(const char* key, char* out, size_t n) {
  size_t k = 0;
  bool start = true;
  for (const char* p = key; *p && k + 1 < n; p++) {
    if (*p == '_') {
      out[k++] = ' ';
      start = true;
    } else {
      char c = *p;
      if (c >= 'A' && c <= 'Z' && !start) c = (char)(c + 32);
      out[k++] = c;
      start = false;
    }
  }
  out[k] = 0;
}

static void fmt_money(char* out, size_t n, double v) {
  char digits[32];
  snprintf(digits, sizeof digits, "%.0f", v < 0 ? 0 : v);
  const int len = (int)strlen(digits);
  size_t k = 0;
  for (int i = 0; i < len && k + 2 < n; i++) {
    if (i && (len - i) % 3 == 0) out[k++] = ',';
    out[k++] = digits[i];
  }
  out[k] = 0;
}

static void money_item_index(const char* key, int* idx) {
  *idx = -1;
  if (!strcmp(key, "NUGGET")) *idx = 0;
  else if (!strcmp(key, "BIG_NUGGET")) *idx = 1;
  else if (!strcmp(key, "RELIC_GOLD")) *idx = 2;
}

static void offer_info(const gc_offer* o, offer_view* v) {
  memset(v, 0, sizeof *v);
  const char* key = o->key ? o->key : "";
  const gm_item_ui* r = gm_item_ui_find(key, o->arg);
  if (!r && o->arg >= 0) r = gm_item_ui_find(key, -1);
  if (!r) {
    humanize(key, v->name, sizeof v->name);
    return;
  }
  v->icon = r->icon;
  snprintf(v->name, sizeof v->name, "%s", r->name);
  if(!strncmp(key,"TM_",3)){
    const char *dash=strstr(r->name," - ");
    snprintf(v->name,sizeof v->name,"%.*s - %s",dash?(int)(dash-r->name):2,r->name,menu_move_name(o->arg));
    snprintf(v->desc,sizeof v->desc,"Teach %s to a Pokemon. Hold SELECT: move details.",menu_move_name(o->arg));
    return;
  }
  if (r->dynamic) {
    /* the money items: {money} is what the item grants now */
    int mi;
    money_item_index(key, &mi);
    const char* at = strstr(r->desc, "{money}");
    if (at && mi >= 0) {
      char amount[32], money[40];
      fmt_money(amount, sizeof amount, S.money_item_amount[mi]);
      snprintf(money, sizeof money, "\xe2\x82\xbd%s", amount);
      snprintf(v->desc, sizeof v->desc, "%.*s%s%s", (int)(at - r->desc), r->desc, money, at + 7);
    }
  } else {
    snprintf(v->desc, sizeof v->desc, "%s", r->desc);
  }
}

/* ---------------------------------------------------------------- colours */

static rr_color dim(rr_color c, int percent) {
  const uint32_t r = (c >> 24) & 255, g = (c >> 16) & 255, b = (c >> 8) & 255;
  return (rr_color)(((r * percent / 100) << 24) | ((g * percent / 100) << 16) | ((b * percent / 100) << 8) | 0xFF);
}

/* getModifierTierTextTint */
static rr_color tier_color(int tier) {
  static const uint32_t tint[6] = {0xf8f8f8, 0x4998f8, 0xf8d038, 0xdb4343, 0xe331c5, 0xe74c18};
  return MUI_RGB(tint[tier < 0 || tier > 5 ? 0 : tier]);
}

static int text8(float x, float y, const char* t, rr_color c) {
  return mui_text_c(FONT, x, y, t, c, dim(c, 35));
}

static int text8_style(float x, float y, const char* t, mui_style st) {
  return mui_text_c(FONT, x, y, t, mui_style_color(st, false), mui_style_color(st, true));
}

/* ---------------------------------------------------------------- layout */

static void add_slot(shop_target_type type, int index, float x, float y, float w, float h) {
  if (S.n_touch_slots >= SHOP_MAX_TOUCH_SLOTS) return;
  shop_touch_slot* slot = &S.touch_slots[S.n_touch_slots++];
  slot->type = type;
  slot->index = index;
  slot->bounds = (shop_ui_rect){x, y, w, h};
}

static int n_buttons(void) {
  int n = 1; /* Continue */
  if (S.screen_copy.reroll_cost >= 0) n++;
  if (S.has_lock_capsule) n++;
  if (S.party_has_held_item) n++;
  return n;
}

static void button_label(shop_target_type type, char* buf, size_t n, rr_color* c);

static void shop_ui_recalc_layout(void) {
  S.n_touch_slots = 0;
  S.n_rows = 0;
  const int n_shop = S.screen_copy.n_shop < 16 ? S.screen_copy.n_shop : 16;
  const int n_opts = S.screen_copy.n_options < GC_MAX_OPTIONS ? S.screen_copy.n_options : GC_MAX_OPTIONS;
  const int shop_rows = (n_shop + SHOP_COLS - 1) / SHOP_COLS;
  const int opt_rows = n_opts > OPT_MAX_COLS ? 2 : (n_opts ? 1 : 0);

  /* the heights, best first: the description window sits at the bottom with the buttons above it, and the rows share what is
   * left; they shrink before the description window does */
  static const float fit[][2] = {{40, 38}, {36, 38}, {34, 38}, {34, 34}, {32, 34}, {30, 32}, {28, 30}, {28, 26}};
  float row_h = 28, d_h = 26;
  const float top = HEADER_H + 2;
  float region = 0;
  for (size_t i = 0; i < sizeof fit / sizeof fit[0]; i++) {
    row_h = fit[i][0];
    d_h = fit[i][1];
    region = MUI_H - 3 - d_h - 2 - BTN_H - 2 - top;
    if ((shop_rows + opt_rows) * (row_h + 1) <= region) break;
  }
  const float shop_h = row_h, opt_h = row_h;
  const float extra = region - (shop_rows + opt_rows) * (row_h + 1);

  float y = top + (extra > 0 ? extra / 2 : 0);
  /* the shop's rows */
  for (int r = 0; r < shop_rows && S.n_rows < MAX_ROWS; r++) {
    const int first = r * SHOP_COLS;
    const int count = n_shop - first < SHOP_COLS ? n_shop - first : SHOP_COLS;
    const float cw = (MUI_W - 2 * MARGIN) / (float)SHOP_COLS;
    const float x0 = (MUI_W - cw * count) / 2.0f;
    shop_row* row = &rows[S.n_rows++];
    *row = (shop_row){ROW_SHOP, first, count, S.n_touch_slots, y, shop_h};
    for (int i = 0; i < count; i++) add_slot(SHOP_TARGET_SHOP, first + i, x0 + cw * i, y, cw, shop_h);
    y += shop_h + 1;
  }
  /* the reward options */
  if (opt_rows) {
    const int per_row = opt_rows == 1 ? n_opts : (n_opts + 1) / 2;
    for (int r = 0; r < opt_rows && S.n_rows < MAX_ROWS; r++) {
      const int first = r * per_row;
      const int count = n_opts - first < per_row ? n_opts - first : per_row;
      const float cw = (MUI_W - 2 * MARGIN) / (float)(per_row < 3 ? 3 : per_row);
      const float x0 = (MUI_W - cw * count) / 2.0f;
      shop_row* row = &rows[S.n_rows++];
      *row = (shop_row){ROW_OPTION, first, count, S.n_touch_slots, y, opt_h};
      for (int i = 0; i < count; i++) add_slot(SHOP_TARGET_REWARD, first + i, x0 + cw * i, y, cw, opt_h);
      y += opt_h + 1;
    }
  }
  /* the buttons: Reroll, Lock Rarities and Manage Items from the left, Continue on the right; each as wide as its label */
  desc_h = d_h;
  desc_y = MUI_H - 3 - d_h;
  const float by = desc_y - 2 - BTN_H;
  if (S.n_rows < MAX_ROWS) {
    shop_row* row = &rows[S.n_rows++];
    *row = (shop_row){ROW_BUTTONS, 0, n_buttons(), S.n_touch_slots, by, BTN_H};
    shop_target_type order[4];
    int nb = 0;
    if (S.screen_copy.reroll_cost >= 0) order[nb++] = SHOP_TARGET_REROLL;
    if (S.has_lock_capsule) order[nb++] = SHOP_TARGET_LOCK;
    if (S.party_has_held_item) order[nb++] = SHOP_TARGET_TRANSFER;
    order[nb++] = SHOP_TARGET_SKIP;
    float w[4], sum = 0;
    for (int i = 0; i < nb; i++) {
      char label[96];
      button_label(order[i], label, sizeof label, NULL);
      w[i] = (float)mui_text_width(FONT, label) + 12;
      if (w[i] < 54) w[i] = 54;
      sum += w[i];
    }
    /* what doesn't fit is taken evenly from each button */
    const float room = MUI_W - 2 * MARGIN - 4 * (nb - 1);
    if (sum > room)
      for (int i = 0; i < nb; i++) w[i] -= (sum - room) / nb;
    float x = MARGIN;
    for (int i = 0; i < nb; i++) {
      const float bx = order[i] == SHOP_TARGET_SKIP ? MUI_W - MARGIN - w[i] : x;
      add_slot(order[i], 0, bx, by, w[i], BTN_H);
      x += w[i] + 4;
    }
  }
}

static const shop_touch_slot* slot_at(int row, int col) {
  if (row < 0 || row >= S.n_rows) return NULL;
  if (col < 0 || col >= rows[row].count) return NULL;
  return &S.touch_slots[rows[row].slot0 + col];
}

static float slot_cx(int row, int col) {
  const shop_touch_slot* s = slot_at(row, col);
  return s ? s->bounds.x + s->bounds.w / 2 : 0;
}

/* the column of `row` whose centre is nearest to x */
static int nearest_col(int row, float x) {
  int best = 0;
  float bd = 1e9f;
  for (int c = 0; c < rows[row].count; c++) {
    const float d = fabsf(slot_cx(row, c) - x);
    if (d < bd) {
      bd = d;
      best = c;
    }
  }
  return best;
}

/* ---------------------------------------------------------------- state */

void shop_ui_open(const shop_ui_args* args, const shop_ui_events* ev) {
  memset(&S, 0, sizeof S);
  if (args && args->screen) {
    S.screen_copy = *args->screen;
    S.player_money = args->player_money;
    S.has_lock_capsule = args->has_lock_capsule;
    S.lock_capsule_active = args->lock_capsule_active;
    S.n_locked_tiers = args->n_locked_tiers;
    S.party_luck = args->party_luck;
    S.party_has_held_item = args->party_has_held_item;
    S.wave = args->wave;
    snprintf(S.location, sizeof S.location, "%s", args->location);
    memcpy(S.money_item_amount, args->money_item_amount, sizeof S.money_item_amount);
    if (args->n_locked_tiers > 0) {
      memcpy(S.locked_tiers, args->locked_tiers, sizeof(args->locked_tiers));
    }
  }
  if (ev) S.events = *ev;

  shop_ui_recalc_layout();
  /* the cursor starts on the first reward option (the first thing on the screen when there are no options) */
  S.row = 0;
  for (int r = 0; r < S.n_rows; r++)
    if (rows[r].kind == ROW_OPTION) {
      S.row = r;
      break;
    }
  S.col = 0;
  S.active = true;
}

void shop_ui_close(void) {
  S.active = false;
}

bool shop_ui_active(void) {
  return S.active;
}

static void trigger_slot(const shop_touch_slot* slot) {
  switch (slot->type) {
    case SHOP_TARGET_REWARD:
      if (slot->index >= 0 && slot->index < S.screen_copy.n_options && S.events.on_select_reward)
        S.events.on_select_reward(S.events.ctx, slot->index);
      break;
    case SHOP_TARGET_SHOP:
      if (slot->index >= 0 && slot->index < S.screen_copy.n_shop && S.events.on_select_shop)
        S.events.on_select_shop(S.events.ctx, slot->index);
      break;
    case SHOP_TARGET_REROLL:
      if (S.events.on_reroll) S.events.on_reroll(S.events.ctx, S.lock_capsule_active);
      break;
    case SHOP_TARGET_LOCK:
      S.lock_capsule_active = !S.lock_capsule_active;
      break;
    case SHOP_TARGET_SKIP:
      if (S.events.on_skip) S.events.on_skip(S.events.ctx);
      break;
    case SHOP_TARGET_TRANSFER:
      if (S.events.on_manage_items) S.events.on_manage_items(S.events.ctx);
      break;
    default:
      break;
  }
}

bool shop_ui_input(mui_button b) {
  if (!S.active || S.n_rows == 0) return false;

  switch (b) {
    case MB_STATS:S.move_details=true;return true;
    case MB_LEFT:
      if (S.col > 0) {
        S.col--;
        mui_sfx("ui/select");
      }
      return true;

    case MB_RIGHT:
      if (S.col < rows[S.row].count - 1) {
        S.col++;
        mui_sfx("ui/select");
      }
      return true;

    case MB_UP:
    case MB_DOWN: {
      const int to = S.row + (b == MB_UP ? -1 : 1);
      if (to >= 0 && to < S.n_rows) {
        const float cx = slot_cx(S.row, S.col);
        S.row = to;
        S.col = nearest_col(to, cx);
        mui_sfx("ui/select");
      }
      return true;
    }

    case MB_ACTION:
    case MB_SUBMIT: {
      const shop_touch_slot* slot = slot_at(S.row, S.col);
      if (slot) {
        mui_sfx("ui/confirm");
        trigger_slot(slot);
      }
      return true;
    }

    case MB_CANCEL:
      /* to the Continue button, which is the last slot */
      S.row = S.n_rows - 1;
      S.col = rows[S.row].count - 1;
      mui_sfx("ui/select");
      return true;

    default:
      break;
  }

  return false;
}

bool shop_ui_touch(float touch_x, float touch_y) {
  if (!S.active) return false;

  for (int r = 0; r < S.n_rows; r++) {
    for (int c = 0; c < rows[r].count; c++) {
      const shop_touch_slot* slot = &S.touch_slots[rows[r].slot0 + c];
      if (!shop_ui_hit_test(&slot->bounds, touch_x, touch_y)) continue;
      mui_sfx("ui/confirm");
      S.row = r;
      S.col = c;
      trigger_slot(slot);
      return true;
    }
  }
  return false;
}

void shop_ui_update(float dt_ms) {
  (void)dt_ms;
}

/* ---------------------------------------------------------------- drawing */

static bool locale_label(const char* key, char* out, size_t n, const char* fallback) {
  const char* t = mui_t(key);
  const bool ok = t && strcmp(t, key) != 0;
  snprintf(out, n, "%s", ok ? t : fallback);
  return ok;
}

static void draw_focus(const shop_ui_rect* r) {
  const rr_color c = RR_RGBA(0xf8, 0xf8, 0xf8, 255);
  rr_rect(r->x, r->y, r->w, 1, c);
  rr_rect(r->x, r->y + r->h - 1, r->w, 1, c);
  rr_rect(r->x, r->y, 1, r->h, c);
  rr_rect(r->x + r->w - 1, r->y, 1, r->h, c);
  rr_rect(r->x + 1, r->y + 1, r->w - 2, r->h - 2, RR_RGBA(0xf8, 0xf8, 0xf8, 36));
}

static void draw_icon(const char* icon, float x, float y) {
  if (icon) mui_image("items", icon, x, y, 0, 0);
}

/* a name wrapped into the width, drawn from y; returns the lines drawn */
static int draw_wrapped(const char* text, float x, float y, float w, rr_color c, int max_lines) {
  char wrapped[256];
  const int lines = mui_wrap(FONT, text, w, wrapped, sizeof wrapped);
  int n = 0;
  for (char* line = wrapped; line && *line && n < max_lines;) {
    char* nl = strchr(line, '\n');
    char one[96];
    snprintf(one, sizeof one, "%.*s", nl ? (int)(nl - line) : (int)strlen(line), line);
    text8(x, y + 9.0f * n, one, c);
    n++;
    line = nl ? nl + 1 : NULL;
  }
  (void)lines;
  return n;
}

static void draw_shop_item(const shop_touch_slot* slot, bool focus) {
  const gc_offer* o = &S.screen_copy.shop[slot->index];
  offer_view v;
  offer_info(o, &v);
  const float cx = slot->bounds.x + slot->bounds.w / 2;
  const float y = slot->bounds.y;
  if (focus) draw_focus(&slot->bounds);
  draw_icon(v.icon, cx - 16, y - 2);
  char amount[32], price[40];
  fmt_money(amount, sizeof amount, o->cost);
  snprintf(price, sizeof price, "\xe2\x82\xbd%s", amount);
  const int w = mui_text_width(FONT, price);
  text8_style(cx - w / 2.0f, y + slot->bounds.h - 10, price, S.player_money >= o->cost ? TS_MONEY : TS_PARTY_RED);
}

static void draw_reward_option(const shop_touch_slot* slot, bool focus) {
  const gc_offer* o = &S.screen_copy.options[slot->index];
  offer_view v;
  offer_info(o, &v);
  const shop_ui_rect* b = &slot->bounds;
  if (focus) draw_focus(b);
  draw_icon(v.icon, b->x + 1, b->y + (b->h - 32) / 2);
  const float tx = b->x + 33, tw = b->w - 33 - 2;
  const bool priced = o->is_purchase || o->cost > 0;
  const int max_lines = priced ? 2 : 3;
  /* the name in its tier's colour, vertically centred with the price under it */
  char wrapped[256];
  const int want = mui_wrap(FONT, v.name, tw, wrapped, sizeof wrapped);
  const int lines = want < max_lines ? want : max_lines;
  const int total = lines + (priced ? 1 : 0);
  const float ty = b->y + (b->h - 9.0f * total) / 2.0f;
  draw_wrapped(v.name, tx, ty, tw, tier_color(o->tier), max_lines);
  if (priced) {
    char amount[32], price[40];
    fmt_money(amount, sizeof amount, o->cost);
    snprintf(price, sizeof price, "\xe2\x82\xbd%s", amount);
    text8_style(tx, ty + 9.0f * lines, price, S.player_money >= o->cost ? TS_MONEY : TS_PARTY_RED);
  }
}

static void button_label(shop_target_type type, char* buf, size_t n, rr_color* c) {
  char label[64];
  if (c) *c = MUI_RGB(0xf8f8f8);
  switch (type) {
    case SHOP_TARGET_REROLL: {
      char amount[32];
      locale_label("modifierSelectUiHandler:reroll", label, sizeof label, "Reroll");
      fmt_money(amount, sizeof amount, S.screen_copy.reroll_cost);
      snprintf(buf, n, "%s \xe2\x82\xbd%s", label, amount);
      if (c) *c = S.player_money >= S.screen_copy.reroll_cost ? mui_style_color(TS_MONEY, false) : mui_style_color(TS_PARTY_RED, false);
      break;
    }
    case SHOP_TARGET_LOCK:
      locale_label("modifierSelectUiHandler:lockRarities", label, sizeof label, "Lock Rarities");
      snprintf(buf, n, "%s: %s", label, S.lock_capsule_active ? "ON" : "OFF");
      if (c && S.lock_capsule_active) *c = mui_style_color(TS_SUMMARY_GOLD, false);
      break;
    case SHOP_TARGET_TRANSFER:
      locale_label("modifierSelectUiHandler:manageItems", buf, n, "Manage Items");
      break;
    default:
      locale_label("modifierSelectUiHandler:continueNextWaveButton", buf, n, "Continue");
      break;
  }
}

static void draw_button(const shop_touch_slot* slot, bool focus) {
  const shop_ui_rect* b = &slot->bounds;
  mui_window(b->x, b->y, b->w, b->h);
  if (focus) draw_focus(b);
  char buf[96];
  rr_color c;
  button_label(slot->type, buf, sizeof buf, &c);
  const int w = mui_text_width(FONT, buf);
  text8(b->x + (b->w - w) / 2.0f, b->y + (b->h - 12) / 2.0f - 1.0f, buf, c); /* the 8 px font's text box is 12 high */
}

static void draw_description(void) {
  mui_window(MARGIN, desc_y, MUI_W - 2 * MARGIN, desc_h);
  const shop_touch_slot* slot = slot_at(S.row, S.col);
  if (!slot) return;
  const float x = MARGIN + 7, w = MUI_W - 2 * MARGIN - 14;
  char name[96] = "", desc[240] = "";
  rr_color nc = MUI_RGB(0xf8f8f8);
  switch (slot->type) {
    case SHOP_TARGET_REWARD:
    case SHOP_TARGET_SHOP: {
      const gc_offer* o = slot->type == SHOP_TARGET_REWARD ? &S.screen_copy.options[slot->index] : &S.screen_copy.shop[slot->index];
      offer_view v;
      offer_info(o, &v);
      snprintf(name, sizeof name, "%s", v.name);
      snprintf(desc, sizeof desc, "%s", v.desc);
      if(S.move_details&&o->key&&!strncmp(o->key,"TM_",3)&&o->arg>0&&o->arg<PR_MOVE_COUNT){
        extern const char* const menu_move_keys[];char key[96];
        snprintf(key,sizeof key,"move:%s.effect",menu_move_keys[o->arg]);
        snprintf(desc,sizeof desc,"%s",mui_t(key));
      }
      if (slot->type == SHOP_TARGET_REWARD) nc = tier_color(o->tier);
      break;
    }
    case SHOP_TARGET_REROLL:
      locale_label("modifierSelectUiHandler:reroll", name, sizeof name, "Reroll");
      locale_label("modifierSelectUiHandler:rerollDesc", desc, sizeof desc, "Spend money to reroll your item options.");
      break;
    case SHOP_TARGET_LOCK:
      locale_label("modifierSelectUiHandler:lockRarities", name, sizeof name, "Lock Rarities");
      locale_label("modifierSelectUiHandler:lockRaritiesDesc", desc, sizeof desc, "Lock item rarities on reroll (affects reroll cost).");
      break;
    case SHOP_TARGET_TRANSFER:
      locale_label("modifierSelectUiHandler:manageItems", name, sizeof name, "Manage Items");
      locale_label("modifierSelectUiHandler:manageItemsDesc", desc, sizeof desc, "Manage items.");
      break;
    default:
      locale_label("modifierSelectUiHandler:continueNextWaveButton", name, sizeof name, "Continue");
      locale_label("modifierSelectUiHandler:continueNextWaveDescription", desc, sizeof desc, "Continue to the next wave.");
      break;
  }
  text8(x, desc_y + 4, name, nc);
  /* the description in what is left of the window; a longer one is cut with "..." */
  const int room = (int)((desc_h - 4 - 9 - 3) / 9);
  char wrapped[512];
  mui_wrap(FONT, desc, w, wrapped, sizeof wrapped);
  int n = 0;
  for (char* line = wrapped; line && *line && n < room;) {
    char* nl = strchr(line, '\n');
    char one[128];
    snprintf(one, sizeof one, "%.*s", nl ? (int)(nl - line) : (int)strlen(line), line);
    if (n == room - 1 && nl && *(nl + 1)) {
      const size_t len = strlen(one);
      if (len > 3) strcpy(one + len - 3, "...");
    }
    text8_style(x, desc_y + 4 + 9 + 9.0f * n, one, TS_WINDOW);
    n++;
    line = nl ? nl + 1 : NULL;
  }
}

void shop_ui_draw(void) {
  if (!S.active) return;

  /* the background */
  mui_window(0, 0, MUI_W, MUI_H);

  /* the top line: place and wave on the left, luck and money on the right */
  char buf[128];
  if (S.location[0]) snprintf(buf, sizeof buf, "%s - %d", S.location, S.wave);
  else snprintf(buf, sizeof buf, "%d", S.wave);
  text8_style(MARGIN + 3, 3, buf, TS_WINDOW);
  char amount[32];
  fmt_money(amount, sizeof amount, S.player_money);
  snprintf(buf, sizeof buf, "\xe2\x82\xbd%s", amount);
  const int mw = mui_text_width(FONT, buf);
  text8_style(MUI_W - MARGIN - 3 - mw, 3, buf, TS_MONEY);
  snprintf(buf, sizeof buf, "Luck: %s", shop_ui_luck_grade(S.party_luck));
  const int lw = mui_text_width(FONT, buf);
  text8_style(MUI_W - MARGIN - 3 - mw - 10 - lw, 3, buf, TS_LUCK_VALUE);

  for (int r = 0; r < S.n_rows; r++) {
    for (int c = 0; c < rows[r].count; c++) {
      const shop_touch_slot* slot = &S.touch_slots[rows[r].slot0 + c];
      const bool focus = r == S.row && c == S.col;
      switch (rows[r].kind) {
        case ROW_SHOP:
          draw_shop_item(slot, focus);
          break;
        case ROW_OPTION:
          draw_reward_option(slot, focus);
          break;
        case ROW_BUTTONS:
          draw_button(slot, focus);
          break;
      }
    }
  }
  draw_description();
}

int shop_ui_slot_count(void) {
  return S.n_touch_slots;
}

const shop_touch_slot* shop_ui_slot(int i) {
  return i >= 0 && i < S.n_touch_slots ? &S.touch_slots[i] : NULL;
}

void shop_ui_release(mui_button b){if(b==MB_STATS)S.move_details=false;}
