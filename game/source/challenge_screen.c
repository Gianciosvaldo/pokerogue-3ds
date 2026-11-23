/* Challenge Mode's challenge screen (see challenge_select.h). Laid out like the run settings screen: a window of rows on the bottom screen,
 * the highlighted challenge's description in the message strip. */
#include <stdio.h>
#include <string.h>

#include "challenge_select.h"
#include "i18n.h"
#include "screen_clip.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

#define ROW_H 16.0f

extern const char* const menu_type_frames[];   /* PokemonType[t].toLowerCase(): the types atlas' frames (menus/menus_names.c) */

static struct {
  chsel_args a;
  chm_model m;
  char desc_key[64];   /* the description on show, and how long (ms) it has been (its scroll restarts with a new one) */
  float desc_ms;
} C;

static void cs_update(ui_screen* s, float dt) {
  (void)s;
  C.desc_ms += dt;
}

static bool cs_input(ui_screen* s, ui_button b) {
  (void)s;
  switch (b) {
    case BTN_UP:
    case BTN_DOWN:
      chm_move(&C.m, b == BTN_DOWN ? 1 : -1);
      ui_play_select();
      return true;
    case BTN_LEFT:
    case BTN_RIGHT:
      if (chm_change(&C.m, b == BTN_RIGHT ? 1 : -1)) ui_play_select();
      return true;
    case BTN_ACTION: {
      if (!chm_can_start(&C.m)) {   /* "None Selected" */
        ui_play_error();
        return true;
      }
      ui_play_select();
      const chsel_args a = C.a;
      const gc_challenge_set v = C.m.v;
      ui_pop();
      if (a.on_start) a.on_start(&v, a.arg);
      return true;
    }
    case BTN_CANCEL: {
      ui_play_select();
      const chsel_args a = C.a;
      ui_pop();
      if (a.on_back) a.on_back(a.arg);
      return true;
    }
    default:
      return false;
  }
}

static void cs_draw(ui_screen* s, rr_screen which) {
  (void)s;
  if (which != RR_BOTTOM) return;
  if (ui_space(UI_PHYS_BOTTOM)) rr_rect(0, 0, 320, 240, RR_RGBA(0x10, 0x10, 0x18, 255));
  if (!ui_space(UI_BOTTOM_UI)) return;
  const int rows = C.m.n + 2;   /* the title, the challenges, Start */
  const float w = 240, h = 16 + rows * ROW_H, x = 160 - w / 2, y = -48 - h - 8;
  ui_window(x, y, w, h, WIN_NORMAL);
  text_opts right = text_defaults();
  right.origin_x = 1;
  char text[160], key[64];
  i18n_t(text, sizeof text, "challenges:title", NULL);
  text_draw(TS_WINDOW, x + 18, y + 8, text, NULL);
  for (int i = 0; i < C.m.n; i++) {
    const int id = C.m.rows[i];
    const float ry = y + 8 + (i + 1) * ROW_H;
    snprintf(key, sizeof key, "challenges:%s.name", chm_key(id));
    i18n_t(text, sizeof text, key, NULL);
    text_draw(TS_WINDOW, x + 18, ry, text, NULL);
    if (id == CHALLENGES_SINGLE_TYPE) {
      /* SingleTypeChallenge.getValue is PokemonType[value - 1].toLowerCase(), shown as that frame of the types atlas (monoTypeValue);
       * value 0 is PokemonType.UNKNOWN */
      const int v = C.m.v.value[id];
      img_draw("images/types", v > 0 ? menu_type_frames[v - 1] : "unknown", x + w - 10, ry + 1, 1, 0);
      continue;
    }
    snprintf(key, sizeof key, "challenges:%s.value.%d", chm_key(id), (int)C.m.v.value[id]);
    i18n_t(text, sizeof text, key, NULL);
    text_draw(TS_WINDOW, x + w - 10, ry, text, &right);
  }
  /* Start, or None Selected (dim) until a challenge has a value */
  i18n_t(text, sizeof text, chm_can_start(&C.m) ? "common:start" : "challenges:noneSelected", NULL);
  text_draw(TS_WINDOW, x + 18, y + 8 + (C.m.n + 1) * ROW_H, text, NULL);
  ui_cursor(x + 12, y + 8 + (C.m.cursor + 1) * ROW_H + 8);   /* on the challenges: the Start label is not a row of the cursor */
  /* what the highlighted challenge does at its value */
  ui_window(0, -48, 320, 48, WIN_NORMAL);
  const int id = chm_current(&C.m);
  if (id == CHALLENGES_SINGLE_TYPE) {
    /* SingleTypeChallenge.getDescription: descDefault at 0, else desc with the type's name (upstream also colours it: not here) */
    const int v = C.m.v.value[id];
    snprintf(key, sizeof key, v > 0 ? "challenges:singleType.desc" : "challenges:singleType.descDefault");
    if (v > 0) i18n_t(text, sizeof text, key, "type", i18n_type_name(v - 1), NULL);
    else i18n_t(text, sizeof text, key, NULL);
  } else if (id >= 0) {
    /* getDescription: desc.<value>, or the one desc of a challenge that has a single text (Inverse Battle, Flip Stat) */
    snprintf(key, sizeof key, "challenges:%s.desc.%d", chm_key(id), (int)C.m.v.value[id]);
    if (!i18n_exists(key)) snprintf(key, sizeof key, "challenges:%s.desc", chm_key(id));
    i18n_t(text, sizeof text, key, NULL);
  } else {
    key[0] = text[0] = 0;
  }
  if (strcmp(key, C.desc_key)) {
    snprintf(C.desc_key, sizeof C.desc_key, "%s", key);
    C.desc_ms = 0;
  }
  text_opts help = text_defaults();
  help.wrap_w = 296;
  /* a description taller than the strip (Limited Support's "Both") scrolls up and back inside it, as updateDescriptionScroll does; the
   * strip's inside is canvas -44..-4 (physical 196..236) */
  const float overflow = text_height(TS_WINDOW, text, &help) - 35;
  clip_rect(6, 196, 308, 40);
  text_draw(TS_WINDOW, 12, -39 - chm_desc_scroll(overflow, C.desc_ms), text, &help);
  clip_off();
}

/* A tap on a challenge's row puts the cursor on it and steps its value; a tap on Start starts */
static bool cs_touch(ui_screen* s, float x, float y) {
  const int rows = C.m.n + 2;
  const float w = 240, h = 16 + rows * ROW_H, left = 160 - w / 2, top = -48 - h - 8, uy = ui_bottom_ui_y(y);
  if (x < left || x >= left + w || uy < top + 8 + ROW_H || uy >= top + 8 + rows * ROW_H) return false;
  const int row = (int)((uy - top - 8) / ROW_H) - 1;
  if (row >= C.m.n) return cs_input(s, BTN_ACTION);
  C.m.cursor = row;
  chm_step(&C.m);
  ui_play_select();
  return true;
}

static ui_screen g_screen = {"challenge-select", UI_OVERLAY, NULL, NULL, NULL, cs_input, cs_update, cs_draw, NULL, cs_touch};

void chsel_open(const chsel_args* a) {
  memset(&C, 0, sizeof C);
  C.a = *a;
  chm_init(&C.m, &a->values);
  ui_push(&g_screen);
}
