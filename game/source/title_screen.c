/* The title screen (see title_screen.h). */
#include "title_screen.h"

#include <stdio.h>
#include <string.h>

#include "bgm.h"
#include "gm_enum_names.h"
#include "i18n.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

static struct {
  title_args args;
  optlist list;
  char labels[8][48];
  const char* label_ptrs[8];
  title_choice choice[8];
  char biome[48];
} T;

static bool ts_input(ui_screen* s, ui_button b) {
  (void)s;
  if (!optlist_input(&T.list, b)) return false;
  if (T.list.chosen >= 0) {
    const title_choice c = T.choice[T.list.chosen];
    T.list.chosen = -1;
    if (T.args.on_choose) T.args.on_choose(c, T.args.arg);
  }
  return true;
}

static bool ts_touch(ui_screen* s, float x, float y) {
  (void)s;
  if (!optlist_touch(&T.list, x, ui_bottom_ui_y(y))) return false;
  if (T.list.chosen >= 0) {
    const title_choice c = T.choice[T.list.chosen];
    T.list.chosen = -1;
    if (T.args.on_choose) T.args.on_choose(c, T.args.arg);
  }
  return true;
}

static void ts_draw(ui_screen* s, rr_screen which) {
  (void)s;
  if (which == RR_TOP) {
    if (ui_space(UI_PHYS_TOP)) rr_rect(0, 0, 400, 240, RR_RGBA(0, 0, 0, 255));
    if (ui_space(UI_FIELD)) {
      char k[96];
      snprintf(k, sizeof k, "images/arenas/%s_bg", T.biome);
      if (img_has(k))
        img_draw(k, NULL, 0, 0, 0, 0);
      else
        rr_rect(0, 0, 320, 180, RR_RGBA(0x88, 0xc8, 0xf0, 255));
      /* addImage(width / 2, 8, "logo") with origin (0.5, 0) */
      if (img_has("images/logo")) {
        img_draw("images/logo", NULL, 160, 8, 0.5f, 0);
      } else {
        /* no logo in the packs: the name in the game's own font */
        text_opts o = text_defaults();
        o.origin_x = 0.5f;
        o.font_px = 192;
        text_draw(TS_SUMMARY, 160, 24, "PokeRogue", &o);
      }
    }
  } else {
    if (ui_space(UI_PHYS_BOTTOM)) rr_rect(0, 0, 320, 240, RR_RGBA(0x10, 0x10, 0x18, 255));
    if (ui_space(UI_BOTTOM_UI)) optlist_draw(&T.list);
  }
}

/* TitlePhase.start: the title music (the slot list keeps it playing) */
static void ts_enter(ui_screen* s) {
  (void)s;
  bgm_on_title();
}

static ui_screen g_screen = {"title", UI_FADE, ts_enter, NULL, NULL, ts_input, NULL, ts_draw, NULL, ts_touch};

void title_screen_open(const title_args* a) {
  memset(&T, 0, sizeof T);
  T.args = *a;
  int n = 0;
  if (a->can_continue) {
    i18n_t(T.labels[n], sizeof T.labels[n], "menu:continue", NULL);
    T.choice[n++] = TITLE_CONTINUE;
  }
  i18n_t(T.labels[n], sizeof T.labels[n], "menu:newGame", NULL);
  T.choice[n++] = TITLE_NEW_GAME;
  i18n_t(T.labels[n], sizeof T.labels[n], "menu:loadGame", NULL);
  T.choice[n++] = TITLE_LOAD_GAME;
  i18n_t(T.labels[n], sizeof T.labels[n], "menu:runHistory", NULL);
  T.choice[n++] = TITLE_HISTORY;
  snprintf(T.labels[n], sizeof T.labels[n], "Nemesis Radar"); /* the custom content's own entry: no upstream string */
  T.choice[n++] = TITLE_RADAR;
  snprintf(T.labels[n], sizeof T.labels[n], "Capsule Machine"); /* the custom content's own entry: no upstream string */
  T.choice[n++] = TITLE_CAPSULE;
  for (int i = 0; i < n; i++) T.label_ptrs[i] = T.labels[i];
  optlist_init(&T.list, T.label_ptrs, n);
  T.list.no_cancel = true; /* blockCancelButton: true */
  gm_enum_lower(GM_E_BIOME_ID, a->biome, T.biome, sizeof T.biome); /* "" for -1 or an unknown biome */
  if (!T.biome[0]) snprintf(T.biome, sizeof T.biome, "plains");
  ui_set(&g_screen);
}
