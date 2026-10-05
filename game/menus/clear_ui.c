/*
 * clear_ui.c: Clear Screen display for PokeRogue 3DS (wave 200 victory).
 */

#include "clear_ui.h"

#include <stdio.h>
#include <string.h>

#include "run_screen.h"
#include "save.h"

static clear_ui_state S;

void clear_ui_open(int wave, int score, double money, int slot_idx) {
  memset(&S, 0, sizeof S);
  S.wave = wave;
  S.score = score;
  S.money = money;
  S.slot_idx = slot_idx;
  S.active = true;

  char name[32], main_p[256], bak_p[256];
  snprintf(name, sizeof name, "session_%d", slot_idx);
  snprintf(main_p, sizeof main_p, "%s/%s.sav", SAVE_DIR, name);
  snprintf(bak_p, sizeof bak_p, "%s/%s.sav.bak", SAVE_DIR, name);
  remove(main_p);
  remove(bak_p);
}

void clear_ui_close(void) {
  if (S.active) {
    S.active = false;
    rs_on_end_screen_dismissed();
  }
}

bool clear_ui_active(void) {
  return S.active;
}

bool clear_ui_input(mui_button b) {
  if (!S.active) return false;
  if (b == MB_ACTION || b == MB_SUBMIT || b == MB_CANCEL) {
    clear_ui_close();
    mui_sfx("ui/confirm");
    return true;
  }
  return false;
}

void clear_ui_update(float dt_ms) {
  (void)dt_ms;
}

void clear_ui_draw(void) {
  if (!S.active) return;

  /* Top Screen */
  rr_target(RR_TOP, 0x102010FF);
  mui_window(20, 20, 360, 200);

  mui_text(TS_SUMMARY_GOLD, 200, 40, "CONGRATULATIONS!");
  mui_text(TS_SUMMARY_ALT, 200, 75, "CLASSIC MODE CLEARED!");

  char stat_buf[128];
  snprintf(stat_buf, sizeof stat_buf, "Wave: %d   Score: %d   Money: ₽%.0f", S.wave, S.score, S.money);
  mui_text(TS_WINDOW, 200, 115, stat_buf);

  mui_text(TS_INSTRUCTIONS_TEXT, 200, 180, "Press A to Return to Title");

  /* Bottom Screen */
  rr_target(RR_BOTTOM, 0x081808FF);
  mui_window(10, 10, 300, 220);

  mui_text(TS_HEADER_LABEL, 150, 50, "VICTORY RECORDED");
  mui_text(TS_SMALLER_WINDOW_ALT, 150, 100, "Vouchers & Stats saved to System Data.");
  mui_text(TS_INSTRUCTIONS_TEXT, 150, 180, "Tap or Press A to Continue");
}
