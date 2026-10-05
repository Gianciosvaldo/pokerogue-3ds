/*
 * game_over_ui.c: Game Over Screen display for PokeRogue 3DS.
 */

#include "game_over_ui.h"

#include <stdio.h>
#include <string.h>

#include "run_screen.h"
#include "save.h"

static game_over_ui_state S;

void game_over_ui_open(int wave, int score, int slot_idx) {
  memset(&S, 0, sizeof S);
  S.wave = wave;
  S.score = score;
  S.slot_idx = slot_idx;
  S.active = true;

  char name[32], main_p[256], bak_p[256];
  snprintf(name, sizeof name, "session_%d", slot_idx);
  snprintf(main_p, sizeof main_p, "%s/%s.sav", SAVE_DIR, name);
  snprintf(bak_p, sizeof bak_p, "%s/%s.sav.bak", SAVE_DIR, name);
  remove(main_p);
  remove(bak_p);
}

void game_over_ui_close(void) {
  if (S.active) {
    S.active = false;
    rs_on_end_screen_dismissed();
  }
}

bool game_over_ui_active(void) {
  return S.active;
}

bool game_over_ui_input(mui_button b) {
  if (!S.active) return false;
  if (b == MB_ACTION || b == MB_SUBMIT || b == MB_CANCEL) {
    game_over_ui_close();
    mui_sfx("ui/confirm");
    return true;
  }
  return false;
}

void game_over_ui_update(float dt_ms) {
  (void)dt_ms;
}

void game_over_ui_draw(void) {
  if (!S.active) return;

  /* Top Screen */
  rr_target(RR_TOP, 0x201010FF);
  mui_window(20, 20, 360, 200);

  mui_text_o(TS_PARTY_RED, 200, 40, 0.5f, 0.0f, "GAME OVER");

  char wave_buf[64];
  snprintf(wave_buf, sizeof wave_buf, "Reached Wave: %d", S.wave);
  mui_text_o(TS_SUMMARY_ALT, 200, 80, 0.5f, 0.0f, wave_buf);

  char score_buf[64];
  snprintf(score_buf, sizeof score_buf, "Final Score: %d", S.score);
  mui_text_o(TS_WINDOW, 200, 115, 0.5f, 0.0f, score_buf);

  mui_text_o(TS_INSTRUCTIONS_TEXT, 200, 180, 0.5f, 0.0f, "Press A to Return to Title");

  /* Bottom Screen */
  rr_target(RR_BOTTOM, 0x180808FF);
  mui_window(10, 10, 300, 220);

  mui_text_o(TS_HEADER_LABEL, 160, 60, 0.5f, 0.0f, "RUN ENDED");
  mui_text_o(TS_SMALLER_WINDOW_ALT, 160, 110, 0.5f, 0.0f, "Save data for this run has been cleared.");
  mui_text_o(TS_INSTRUCTIONS_TEXT, 160, 180, 0.5f, 0.0f, "Tap or Press A to Continue");
}
