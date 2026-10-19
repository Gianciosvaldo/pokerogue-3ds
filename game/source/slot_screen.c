/* The save slot list (see slot_screen.h). */
#include "slot_screen.h"

#include <stdio.h>
#include <string.h>

#include "gm_enum_names.h"
#include "i18n.h"
#include "party_icon.h"
#include "run_summary.h"
#include "run_text.h"
#include "screen_clip.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

#define VISIBLE 3        /* slots on the 3DS's 240 rows (upstream shows 2 on its 180) */
#define PITCH 76.0f      /* a slot's height plus the gap (valueHeight) */
#define SLOT_W 304
#define SLOT_H 70
#define MAX_LABELS 4     /* Load Game, Rename Run, Delete Run, Cancel */

static struct {
  slots_args a;
  int cursor, top;       /* the highlighted slot, the first slot in view */
  float scroll;          /* px the list is scrolled by now (it eases to top * PITCH) */
  bool done;             /* the caller took over: no more input */
  /* the message box at the bottom (showText) */
  msgbox msg;
  bool msg_on;
  /* the answers an overlay is about to give */
  char labels[MAX_LABELS][48];
  const char* label_ptrs[MAX_LABELS];
  int label_action[MAX_LABELS];
  int n_labels;
} S;

enum { ACT_LOAD, ACT_RENAME, ACT_DELETE, ACT_CANCEL };

static const rsv_info* info_of(int slot) { return &S.a.info[slot]; }

/* ------------------------------------------------------------------ messages */

static void show_message(const char* text, bool prompt) {
  char wrapped[512];
  text_opts o = text_defaults();
  o.wrap_w = 300;
  text_wrap(TS_WINDOW, text, &o, wrapped, sizeof wrapped);
  msgbox_init(&S.msg, TS_WINDOW, 300, 2);
  msgbox_show(&S.msg, wrapped, -1, prompt);
  if (!prompt) S.msg.wait_ms = 1e9f;
  S.msg_on = true;
}

static void hide_message(void) {
  msgbox_clear(&S.msg);
  S.msg_on = false;
}

void slot_screen_message(const char* text) { show_message(text, true); }

/* ------------------------------------------------------------------ the answers */

static void finish_pick(int slot) {
  S.done = true;
  hide_message();
  if (S.a.on_pick) S.a.on_pick(slot, S.a.arg);
}

static void on_overwrite(int yes, void* arg) {
  const int slot = (int)(long)arg;
  if (yes)
    finish_pick(slot);
  else
    hide_message();
}

static void on_delete_confirm(int yes, void* arg) {
  const int slot = (int)(long)arg;
  hide_message();
  if (yes && S.a.on_delete) S.a.on_delete(slot, S.a.arg);
}

static void on_slot_menu(int idx, void* arg) {
  const int slot = (int)(long)arg;
  hide_message();
  if (idx < 0 || idx >= S.n_labels) return;
  switch (S.label_action[idx]) {
    case ACT_LOAD:
      finish_pick(slot);
      break;
    case ACT_RENAME:
      S.done = true; /* the caller has the screens now */
      if (S.a.on_rename) S.a.on_rename(slot, S.a.arg);
      break;
    case ACT_DELETE: {
      char t[96];
      i18n_t(t, sizeof t, "saveSlotSelectUiHandler:deleteData", NULL);
      show_message(t, false);
      ui_push_confirm(on_delete_confirm, arg);
      break;
    }
    default:
      break;
  }
}

static void add_label(const char* key, int action) {
  if (S.n_labels >= MAX_LABELS) return;
  i18n_t(S.labels[S.n_labels], sizeof S.labels[0], key, NULL);
  S.label_ptrs[S.n_labels] = S.labels[S.n_labels];
  S.label_action[S.n_labels++] = action;
}

static void act(void) {
  const int slot = S.cursor;
  const rsv_info* in = info_of(slot);
  if (S.a.mode == SLOTS_LOAD) {
    if (in->status == RSV_EMPTY) {
      ui_play_error();
      return;
    }
    S.n_labels = 0;
    if (in->status == RSV_OK) add_label("menu:loadGame", ACT_LOAD);
    if (in->status == RSV_OK && S.a.on_rename) add_label("saveSlotSelectUiHandler:renameRun", ACT_RENAME);
    add_label("saveSlotSelectUiHandler:deleteRun", ACT_DELETE);
    add_label("menu:cancel", ACT_CANCEL);
    ui_play_select();
    ui_push_options(S.label_ptrs, S.n_labels, true, on_slot_menu, (void*)(long)slot);
    return;
  }
  /* a new run */
  switch (in->status) {
    case RSV_EMPTY:
      ui_play_select();
      finish_pick(slot);
      break;
    case RSV_OK: {
      char t[96];
      i18n_t(t, sizeof t, "saveSlotSelectUiHandler:overwriteData", NULL);
      show_message(t, false);
      ui_play_select();
      ui_push_confirm(on_overwrite, (void*)(long)slot);
      break;
    }
    default:
      ui_play_error();
      show_message("That slot holds a save this version can't read. It is never overwritten: delete it from Load Game first.", true);
      break;
  }
}

/* ------------------------------------------------------------------ screen */

static void keep_cursor_in_view(void) {
  if (S.cursor < S.top) S.top = S.cursor;
  if (S.cursor > S.top + VISIBLE - 1) S.top = S.cursor - (VISIBLE - 1);
  if (S.top < 0) S.top = 0;
  if (S.top > RSV_SLOTS - VISIBLE) S.top = RSV_SLOTS - VISIBLE;
}

static bool sl_input(ui_screen* s, ui_button b) {
  (void)s;
  if (S.done) return true;
  if (S.msg_on) {
    if (msgbox_input(&S.msg, b)) {
      if (msgbox_done(&S.msg)) hide_message();
      return true;
    }
    if (msgbox_typing(&S.msg) || S.msg.waiting) return true; /* the text is up: it answers first */
  }
  switch (b) {
    case BTN_UP:
      S.cursor = (S.cursor + RSV_SLOTS - 1) % RSV_SLOTS;
      keep_cursor_in_view();
      ui_play_select();
      return true;
    case BTN_DOWN:
      S.cursor = (S.cursor + 1) % RSV_SLOTS;
      keep_cursor_in_view();
      ui_play_select();
      return true;
    case BTN_ACTION:
      act();
      return true;
    case BTN_CANCEL:
      S.done = true;
      ui_play_select();
      if (S.a.on_back) S.a.on_back(S.a.arg);
      return true;
    default:
      return false;
  }
}

static void sl_update(ui_screen* s, float dt) {
  (void)s;
  if (S.msg_on) msgbox_update(&S.msg, dt);
  const float target = S.top * PITCH;
  const float k = dt / 90.0f > 1 ? 1 : dt / 90.0f;
  S.scroll += (target - S.scroll) * k;
  if (S.scroll - target < 0.5f && target - S.scroll < 0.5f) S.scroll = target;
}

static void draw_level(float right, float y, int level, bool fainted) {
  char t[64];
  char lv[48];
  i18n_t(lv, sizeof lv, "saveSlotSelectUiHandler:lv", NULL);
  snprintf(t, sizeof t, "%s%d", lv, level);
  text_opts o = text_defaults();
  o.origin_x = 1;
  o.font_px = 54;
  o.color = fainted ? RR_RGBA(0xf8, 0x58, 0x58, 255) : RR_RGBA(0xf8, 0xf8, 0xf8, 255);
  o.shadow = RR_RGBA(0x42, 0x42, 0x42, 255);
  text_draw(TS_PARTY, right, y, t, &o);
}

/* one slot window with its summary (the slot's own coordinates: 304 x 70) */
static void draw_slot(float x, float y, const rsv_info* in, bool selected) {
  ui_window(x, y, SLOT_W, SLOT_H, WIN_NORMAL);
  if (selected) {
    const rr_color c = RR_RGBA(0xf8, 0xe8, 0x58, 255);
    rr_rect(x - 2, y - 2, SLOT_W + 4, 2, c);
    rr_rect(x - 2, y + SLOT_H, SLOT_W + 4, 2, c);
    rr_rect(x - 2, y, 2, SLOT_H, c);
    rr_rect(x + SLOT_W, y, 2, SLOT_H, c);
  }
  text_opts mid = text_defaults();
  mid.origin_x = mid.origin_y = 0.5f;
  char buf[160], a[64], b[64];
  switch (in->status) {
    case RSV_EMPTY:
      i18n_t(buf, sizeof buf, "saveSlotSelectUiHandler:empty", NULL);
      text_draw(TS_WINDOW, x + 152, y + 33, buf, &mid);
      return;
    case RSV_INCOMPATIBLE: {
      text_draw(TS_WINDOW, x + 8, y + 5, "Incompatible save", NULL);
      text_opts w = text_defaults();
      w.wrap_w = 288;
      w.max_lines = 3;
      text_draw(TS_WINDOW, x + 8, y + 19, in->why, &w);
      return;
    }
    default:
      break;
  }
  char lvl[48];
  rt_mode_name(in->mode, a, sizeof a);
  i18n_t(lvl, sizeof lvl, "saveSlotSelectUiHandler:wave", NULL);
  snprintf(buf, sizeof buf, "%s - %s %d%s", a, lvl, in->wave, in->from_backup ? " (backup)" : "");
  /* upstream's window: the run's name first, then mode and wave. The 4 lines the window has room for are then name, mode and wave, score,
   * money: the biome (the top screen's background shows it) is the one that goes. A run nobody named keeps its four lines. */
  if (in->name[0]) {
    text_draw(TS_WINDOW, x + 8, y + 5, in->name, NULL);
    text_draw(TS_WINDOW, x + 8, y + 19, buf, NULL);
  } else {
    text_draw(TS_WINDOW, x + 8, y + 5, buf, NULL);
    text_draw(TS_WINDOW, x + 8, y + 19, rt_biome_name(in->biome, a, sizeof a), NULL);
  }
  i18n_t(lvl, sizeof lvl, "menu:score", NULL);
  snprintf(buf, sizeof buf, "%s %s", lvl, rt_number(in->score, b, sizeof b));
  text_draw(TS_WINDOW, x + 8, y + 33, buf, NULL);
  text_draw(TS_MONEY, x + 8, y + 47, rt_money(in->money, a, sizeof a), NULL);
  /* the party: icons at 0.75 in a row, from (144, 16), 26 px apart, with their levels (pokemonIconsContainer) */
  for (int i = 0; i < in->n_party; i++) {
    const float ix = x + 144 + 26 * i;
    pi_draw(&in->party[i], ix + 12, y + 16, 0.75f);
    draw_level(ix + 24, y + 31, in->party[i].level, in->party[i].fainted);
  }
}

/* the highlighted slot in full, on the top screen */
static void draw_preview(const rsv_info* in) {
  if (ui_space(UI_PHYS_TOP)) rr_rect(0, 0, 400, 240, RR_RGBA(0, 0, 0, 255));
  if (!ui_space(UI_FIELD)) return;
  char a[96], buf[200], key[128];
  gm_enum_lower(GM_E_BIOME_ID, in->biome, a, sizeof a);
  if (in->status == RSV_OK && a[0]) {
    snprintf(key, sizeof key, "images/arenas/%s_bg", a);
    if (img_has(key)) img_draw(key, NULL, 0, 0, 0, 0);
  }
  ui_window(12, 10, 296, 160, WIN_NORMAL);
  text_opts mid = text_defaults();
  mid.origin_x = mid.origin_y = 0.5f;
  switch (in->status) {
    case RSV_EMPTY:
      i18n_t(buf, sizeof buf, "saveSlotSelectUiHandler:empty", NULL);
      text_draw(TS_WINDOW, 160, 90, buf, &mid);
      return;
    case RSV_INCOMPATIBLE: {
      text_draw(TS_WINDOW, 26, 24, "Incompatible save", NULL);
      text_opts w = text_defaults();
      w.wrap_w = 268;
      w.max_lines = 4;
      text_draw(TS_WINDOW, 26, 44, in->why, &w);
      text_draw(TS_WINDOW, 26, 100,
                "It is never overwritten or deleted by the game. Delete Run frees the slot.", &w);
      return;
    }
    default:
      break;
  }
  rsum_draw(in, 26, 24);
}

static void sl_draw(ui_screen* s, rr_screen which) {
  (void)s;
  if (which == RR_BOTTOM) {
    if (!ui_space(UI_PHYS_BOTTOM)) return;
    clip_to_screen(); /* the slot below the last one in view starts under the screen's edge */
    rr_rect(0, 0, 320, 240, RR_RGBA(0x00, 0x68, 0x60, 255));
    for (int i = 0; i < RSV_SLOTS; i++) {
      const float y = 8 + i * PITCH - S.scroll;
      if (y < -SLOT_H - 4 || y > 244) continue;
      draw_slot(i == S.cursor ? 2 : 8, y, info_of(i), i == S.cursor);
    }
    if (S.msg_on) {
      const float h = text_lines(TS_WINDOW, S.msg.text, NULL) > 1 ? 42 : 28;
      ui_window(1, 239 - h, 318, h, WIN_NORMAL);
      msgbox_draw(&S.msg, 8, 239 - h + 8);
    }
    clip_off();
  } else {
    draw_preview(info_of(S.cursor));
  }
}

static ui_screen g_screen = {"save-slots", UI_FADE, NULL, NULL, NULL, sl_input, sl_update, sl_draw, NULL};

void slot_screen_open(const slots_args* a) {
  memset(&S, 0, sizeof S);
  S.a = *a;
  S.cursor = a->cursor >= 0 && a->cursor < RSV_SLOTS ? a->cursor : 0;
  keep_cursor_in_view();
  S.scroll = S.top * PITCH;
  msgbox_init(&S.msg, TS_WINDOW, 300, 2);
  ui_set(&g_screen);
}

void slot_screen_refresh(void) {
  hide_message();
  S.done = false; /* a pick that did not lead anywhere (the load failed): the list takes input again */
  keep_cursor_in_view();
}
