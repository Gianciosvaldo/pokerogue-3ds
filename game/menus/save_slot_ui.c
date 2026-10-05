/*
 * save_slot_ui.c: 5-slot Save Manager UI with non-destructive refusal handling
 * for PokeRogue 3DS.
 */

#include "save_slot_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gc/run.h"
#include "save.h"

static save_slot_ui_state S;

void save_slot_ui_scan_slots(save_slot_info out_slots[SAVE_SLOT_COUNT]) {
  for (int i = 0; i < SAVE_SLOT_COUNT; i++) {
    save_slot_info* info = &out_slots[i];
    memset(info, 0, sizeof(*info));

    char name[32];
    snprintf(name, sizeof name, "session_%d", i);

    char* buf = NULL;
    size_t len = 0;
    bool used_bak = false;
    save_result res = save_read(name, &buf, &len, &used_bak);

    if (res == SAVE_NOT_FOUND) {
      info->exists = false;
      continue;
    }

    info->exists = true;
    info->used_backup = used_bak;

    if (res != SAVE_OK || !buf) {
      info->valid = false;
      snprintf(info->refusal_reason, sizeof info->refusal_reason, "File Read / CRC Error");
      if (buf) free(buf);
      continue;
    }

    gc_run loaded;
    const char* why = NULL;
    if (!gc_run_load(&loaded, buf, len, &why)) {
      info->valid = false;
      snprintf(info->refusal_reason, sizeof info->refusal_reason, "%s", why ? why : "Incompatible Save Format");
      free(buf);
      continue;
    }

    info->valid = true;
    info->wave = loaded.wave;
    info->score = loaded.score;
    info->money = loaded.money;
    if (loaded.n_party > 0) {
      info->lead_species = loaded.party[0].b.species;
      info->lead_level = loaded.party[0].b.level;
    }
    snprintf(info->biome_name, sizeof info->biome_name, "Biome %d", loaded.biome);

    free(buf);
  }
}

void save_slot_ui_open(const save_slot_ui_events* ev) {
  memset(&S, 0, sizeof S);
  if (ev) S.events = *ev;

  save_slot_ui_scan_slots(S.slots);

  S.selected_slot = 0;
  S.selected_sub_opt = 0;
  S.show_sub_menu = false;
  S.show_refusal_dialog = false;
  S.show_delete_confirm = false;
  S.active = true;
}

void save_slot_ui_close(void) {
  S.active = false;
}

bool save_slot_ui_active(void) {
  return S.active;
}

bool save_slot_ui_input(mui_button b) {
  if (!S.active) return false;

  if (S.show_refusal_dialog) {
    if (b == MB_ACTION || b == MB_SUBMIT || b == MB_CANCEL) {
      S.show_refusal_dialog = false;
      mui_sfx("ui/confirm");
    }
    return true;
  }

  if (S.show_delete_confirm) {
    if (b == MB_LEFT || b == MB_RIGHT || b == MB_UP || b == MB_DOWN) {
      S.selected_sub_opt = S.selected_sub_opt ? 0 : 1;
      mui_sfx("ui/select");
      return true;
    }
    if (b == MB_ACTION || b == MB_SUBMIT) {
      if (S.selected_sub_opt == 0) {
        int slot = S.selected_slot;
        S.show_delete_confirm = false;
        S.show_sub_menu = false;

        char name[32], main_p[256], bak_p[256];
        snprintf(name, sizeof name, "session_%d", slot);
        snprintf(main_p, sizeof main_p, "%s/%s.sav", SAVE_DIR, name);
        snprintf(bak_p, sizeof bak_p, "%s/%s.sav.bak", SAVE_DIR, name);
        remove(main_p);
        remove(bak_p);

        save_slot_ui_scan_slots(S.slots);
        if (S.events.on_delete_slot) {
          S.events.on_delete_slot(S.events.ctx, slot);
        }
        mui_sfx("ui/confirm");
      } else {
        S.show_delete_confirm = false;
        S.selected_sub_opt = 1;
        mui_sfx("ui/confirm");
      }
      return true;
    }
    if (b == MB_CANCEL) {
      S.show_delete_confirm = false;
      mui_sfx("ui/confirm");
      return true;
    }
    return true;
  }

  if (S.show_sub_menu) {
    if (b == MB_UP) {
      if (S.selected_sub_opt > 0) {
        S.selected_sub_opt--;
        mui_sfx("ui/select");
      }
      return true;
    }
    if (b == MB_DOWN) {
      if (S.selected_sub_opt < 2) {
        S.selected_sub_opt++;
        mui_sfx("ui/select");
      }
      return true;
    }
    if (b == MB_CANCEL) {
      S.show_sub_menu = false;
      mui_sfx("ui/confirm");
      return true;
    }
    if (b == MB_ACTION || b == MB_SUBMIT) {
      const save_slot_info* info = &S.slots[S.selected_slot];

      if (S.selected_sub_opt == 0) {
        if (!info->exists) {
          S.show_sub_menu = false;
          if (S.events.on_new_slot) {
            S.events.on_new_slot(S.events.ctx, S.selected_slot);
          }
        } else if (!info->valid) {
          S.show_refusal_dialog = true;
          snprintf(S.refusal_msg, sizeof S.refusal_msg, "Incompatible / Damaged Save:\n%s\nFile retained on SD.", info->refusal_reason);
        } else {
          S.show_sub_menu = false;
          if (S.events.on_load_slot) {
            S.events.on_load_slot(S.events.ctx, S.selected_slot);
          }
        }
        mui_sfx("ui/confirm");
      } else if (S.selected_sub_opt == 1) {
        if (info->exists) {
          S.show_delete_confirm = true;
          S.selected_sub_opt = 1;
          mui_sfx("ui/confirm");
        } else {
          mui_sfx("ui/select");
        }
      } else {
        S.show_sub_menu = false;
        mui_sfx("ui/confirm");
      }
      return true;
    }
    return true;
  }

  switch (b) {
    case MB_UP:
      if (S.selected_slot > 0) {
        S.selected_slot--;
        mui_sfx("ui/select");
      }
      return true;

    case MB_DOWN:
      if (S.selected_slot < SAVE_SLOT_COUNT - 1) {
        S.selected_slot++;
        mui_sfx("ui/select");
      }
      return true;

    case MB_ACTION:
    case MB_SUBMIT:
      S.show_sub_menu = true;
      S.selected_sub_opt = 0;
      mui_sfx("ui/confirm");
      return true;

    case MB_CANCEL:
      if (S.events.on_cancel) {
        save_slot_ui_close();
        S.events.on_cancel(S.events.ctx);
      }
      mui_sfx("ui/confirm");
      return true;

    default:
      break;
  }

  return false;
}

bool save_slot_ui_touch(float touch_x, float touch_y) {
  if (!S.active) return false;
  if (!S.show_sub_menu && !S.show_refusal_dialog && !S.show_delete_confirm) {
    float start_y = 15.0f;
    float slot_h = 36.0f;
    float spacing = 6.0f;

    for (int i = 0; i < SAVE_SLOT_COUNT; i++) {
      float y = start_y + i * (slot_h + spacing);
      if (touch_x >= 10.0f && touch_x <= 310.0f && touch_y >= y && touch_y <= y + slot_h) {
        S.selected_slot = i;
        S.show_sub_menu = true;
        S.selected_sub_opt = 0;
        mui_sfx("ui/confirm");
        return true;
      }
    }
  }
  return false;
}

void save_slot_ui_update(float dt_ms) {
  (void)dt_ms;
}

void save_slot_ui_draw(void) {
  if (!S.active) return;

  /* Top Screen Header */
  rr_target(RR_TOP, 0x181828FF);
  mui_window(20, 15, 360, 210);
  mui_text_o(TS_SUMMARY_HEADER, 200, 30, 0.5f, 0.0f, "SAVE SLOTS");

  const save_slot_info* sel = &S.slots[S.selected_slot];
  char sel_buf[128];
  snprintf(sel_buf, sizeof sel_buf, "Selected Slot %d: %s", S.selected_slot + 1,
           !sel->exists ? "EMPTY" : (sel->valid ? sel->biome_name : "CORRUPT / INCOMPATIBLE"));
  mui_text_o(TS_SUMMARY_ALT, 200, 60, 0.5f, 0.0f, sel_buf);

  if (sel->exists && sel->valid) {
    char stat_buf[128];
    snprintf(stat_buf, sizeof stat_buf, "Wave %d   Score: %d   Money: ₽%.0f", sel->wave, sel->score, sel->money);
    mui_text_o(TS_WINDOW, 200, 95, 0.5f, 0.0f, stat_buf);
    if (sel->lead_species > 0) {
      char lead_buf[64];
      snprintf(lead_buf, sizeof lead_buf, "Lead: Species #%d (L%d)", sel->lead_species, sel->lead_level);
      mui_text_o(TS_SMALLER_WINDOW_ALT, 200, 125, 0.5f, 0.0f, lead_buf);
    }
  } else if (sel->exists && !sel->valid) {
    mui_text_o(TS_PARTY_RED, 200, 100, 0.5f, 0.0f, "Refusal: Save File Incompatible");
    mui_text_o(TS_SMALLER_WINDOW_ALT, 200, 130, 0.5f, 0.0f, sel->refusal_reason);
  } else {
    mui_text_o(TS_SUMMARY_GRAY, 200, 100, 0.5f, 0.0f, "Select to Start New Game in Slot");
  }

  /* Bottom Screen List */
  rr_target(RR_BOTTOM, 0x202030FF);
  mui_window(5, 5, 310, 230);

  float start_y = 15.0f;
  float slot_h = 36.0f;
  float spacing = 6.0f;

  for (int i = 0; i < SAVE_SLOT_COUNT; i++) {
    const save_slot_info* info = &S.slots[i];
    float x = 15.0f;
    float y = start_y + i * (slot_h + spacing);
    float w = 280.0f;

    mui_window(x, y, w, slot_h);

    if (S.selected_slot == i) {
      mui_window(x - 2, y - 2, w + 4, slot_h + 4);
    }

    char label[32];
    snprintf(label, sizeof label, "SLOT %d", i + 1);

    if (!info->exists) {
      mui_text(TS_SUMMARY_GRAY, x + 10, y + 10, label);
      mui_text(TS_SUMMARY_GRAY, x + 80, y + 10, "[EMPTY]");
    } else if (!info->valid) {
      mui_text(TS_PARTY_RED, x + 10, y + 10, label);
      mui_text(TS_PARTY_RED, x + 80, y + 10, "[INCOMPATIBLE / CORRUPT]");
    } else {
      mui_text(TS_SUMMARY_GOLD, x + 10, y + 10, label);
      char wave_buf[64];
      snprintf(wave_buf, sizeof wave_buf, "Wave %d - %s", info->wave, info->biome_name);
      mui_text(TS_WINDOW, x + 80, y + 10, wave_buf);
    }
  }

  /* Submenu Overlay */
  if (S.show_sub_menu && !S.show_delete_confirm && !S.show_refusal_dialog) {
    mui_window(50, 40, 220, 140);
    mui_text_o(TS_HEADER_LABEL, 160, 48, 0.5f, 0.0f, "SLOT OPTIONS");

    const char* sub_opts[3] = {
      sel->exists ? (sel->valid ? "RESUME RUN" : "ATTEMPT LOAD") : "START NEW RUN",
      "DELETE SLOT",
      "CANCEL"
    };

    for (int k = 0; k < 3; k++) {
      float sy = 75.0f + k * 32.0f;
      mui_window(65, sy, 190, 26);
      if (S.selected_sub_opt == k) {
        mui_window(63, sy - 2, 194, 30);
      }
      mui_style st = (S.selected_sub_opt == k) ? TS_SUMMARY_GOLD : TS_WINDOW;
      mui_text(st, 80, sy + 5, sub_opts[k]);
    }
  }

  /* Refusal Dialog Modal */
  if (S.show_refusal_dialog) {
    mui_window(30, 40, 260, 140);
    mui_text_o(TS_PARTY_RED, 160, 50, 0.5f, 0.0f, "CANNOT LOAD SAVE");
    mui_text(TS_WINDOW, 40, 80, S.refusal_msg);
    mui_text_o(TS_INSTRUCTIONS_TEXT, 160, 150, 0.5f, 0.0f, "Press A to Dismiss");
  }

  /* Delete Confirm Modal */
  if (S.show_delete_confirm) {
    mui_window(40, 50, 240, 120);
    mui_text_o(TS_PARTY_RED, 160, 60, 0.5f, 0.0f, "DELETE THIS SAVE SLOT?");
    mui_text_o(TS_SMALLER_WINDOW_ALT, 160, 85, 0.5f, 0.0f, "This action cannot be undone.");

    mui_window(60, 115, 80, 28);
    if (S.selected_sub_opt == 0) mui_window(58, 113, 84, 32);
    mui_text(S.selected_sub_opt == 0 ? TS_PARTY_RED : TS_WINDOW, 80, 120, "YES");

    mui_window(180, 115, 80, 28);
    if (S.selected_sub_opt == 1) mui_window(178, 113, 84, 32);
    mui_text(S.selected_sub_opt == 1 ? TS_SUMMARY_GOLD : TS_WINDOW, 205, 120, "NO");
  }
}
