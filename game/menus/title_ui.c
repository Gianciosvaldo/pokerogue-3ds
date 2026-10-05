/*
 * title_ui.c: Title screen UI and options menu for PokeRogue 3DS.
 */

#include "title_ui.h"

#include <stdio.h>
#include <string.h>

static title_ui_state S;

void title_ui_open(bool has_continue, int continue_slot, int continue_wave, const char* continue_biome, const title_ui_events* ev) {
  memset(&S, 0, sizeof S);
  S.has_continue_save = has_continue;
  S.continue_slot = continue_slot;
  S.continue_wave = continue_wave;
  if (continue_biome) {
    snprintf(S.continue_biome, sizeof S.continue_biome, "%s", continue_biome);
  }
  if (ev) S.events = *ev;

  S.selected_opt = has_continue ? TITLE_OPT_CONTINUE : TITLE_OPT_NEW_GAME;
  S.active = true;
}

void title_ui_close(void) {
  S.active = false;
}

bool title_ui_active(void) {
  return S.active;
}

bool title_ui_input(mui_button b) {
  if (!S.active) return false;

  switch (b) {
    case MB_UP:
      if (S.selected_opt > 0) {
        S.selected_opt--;
        if (S.selected_opt == TITLE_OPT_CONTINUE && !S.has_continue_save) {
          S.selected_opt = TITLE_OPT_NEW_GAME;
        }
        mui_sfx("ui/select");
      }
      return true;

    case MB_DOWN:
      if (S.selected_opt < TITLE_OPT_COUNT - 1) {
        S.selected_opt++;
        if (S.selected_opt == TITLE_OPT_CONTINUE && !S.has_continue_save) {
          S.selected_opt = TITLE_OPT_SAVE_SLOTS;
        }
        mui_sfx("ui/select");
      }
      return true;

    case MB_ACTION:
    case MB_SUBMIT:
      mui_sfx("ui/confirm");
      if (S.events.on_select_option) {
        S.events.on_select_option(S.events.ctx, (title_option)S.selected_opt);
      }
      return true;

    default:
      break;
  }

  return false;
}

bool title_ui_touch(float touch_x, float touch_y) {
  if (!S.active) return false;

  float btn_h = 32.0f;
  float start_y = 30.0f;
  float spacing = 12.0f;

  for (int i = 0; i < TITLE_OPT_COUNT; i++) {
    if (i == TITLE_OPT_CONTINUE && !S.has_continue_save) continue;

    float y = start_y + i * (btn_h + spacing);
    if (touch_x >= 40.0f && touch_x <= 280.0f && touch_y >= y && touch_y <= y + btn_h) {
      S.selected_opt = i;
      mui_sfx("ui/confirm");
      if (S.events.on_select_option) {
        S.events.on_select_option(S.events.ctx, (title_option)i);
      }
      return true;
    }
  }

  return false;
}

void title_ui_update(float dt_ms) {
  (void)dt_ms;
}

void title_ui_draw(void) {
  if (!S.active) return;

  /* Top Screen */
  rr_target(RR_TOP, 0x181828FF);

  mui_window(30, 20, 340, 100);

  if (!mui_image("title_logo", "logo", 200, 50, 0.5f, 0.5f)) {
    mui_text_o(TS_SUMMARY_HEADER, 200, 45, 0.5f, 0.0f, "POKEROGUE 3DS");
    mui_text_o(TS_SUMMARY_ALT, 200, 75, 0.5f, 0.0f, "Rogue-lite Pokemon Battle Simulation");
  }

  mui_text_o(TS_SMALLER_WINDOW_ALT, 200, 105, 0.5f, 0.0f, "v1.0.0 (Port Commit 8555c08)");

  mui_text_o(TS_INSTRUCTIONS_TEXT, 200, 220, 0.5f, 0.0f, "D-Pad: Select   A: Confirm");

  /* Bottom Screen */
  rr_target(RR_BOTTOM, 0x202030FF);

  mui_window(10, 10, 300, 220);

  float btn_w = 240.0f;
  float btn_h = 32.0f;
  float start_y = 30.0f;
  float spacing = 12.0f;

  const char* labels[TITLE_OPT_COUNT] = {
    "NEW GAME",
    "CONTINUE",
    "SAVE SLOTS"
  };

  for (int i = 0; i < TITLE_OPT_COUNT; i++) {
    float x = 40.0f;
    float y = start_y + i * (btn_h + spacing);

    bool enabled = (i != TITLE_OPT_CONTINUE || S.has_continue_save);
    bool selected = (S.selected_opt == i);

    mui_window(x, y, btn_w, btn_h);

    if (selected) {
      mui_window(x - 2, y - 2, btn_w + 4, btn_h + 4);
    }

    mui_style style = enabled ? (selected ? TS_SUMMARY_GOLD : TS_WINDOW) : TS_SUMMARY_GRAY;
    mui_text(style, x + 20, y + 8, labels[i]);

    if (i == TITLE_OPT_CONTINUE && S.has_continue_save) {
      char sub_buf[64];
      snprintf(sub_buf, sizeof sub_buf, "Slot %d - Wave %d (%s)", S.continue_slot + 1, S.continue_wave, S.continue_biome[0] ? S.continue_biome : "Unknown");
      mui_text(TS_SMALLER_WINDOW_ALT, x + 100, y + 10, sub_buf);
    }
  }
}
