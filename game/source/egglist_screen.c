/* The Egg List screen (see egglist_screen.h). Geometry is upstream's (a 320 x 180 canvas, like the bottom screen). */
#include "egglist_screen.h"

#include <stdio.h>
#include <string.h>

#include "egglist_model.h"
#include "i18n.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

#define CANVAS_W 320
#define CANVAS_H 180
#define ICON_X0 113 /* the icon container's position */
#define ICON_Y0 5
#define CELL 18
#define HINT_X 10
#define HINT_Y 224
#define SCROLL_X 310
#define SCROLL_Y 5
#define SCROLL_W 4
#define SCROLL_H 170
#define WRAP_W 90.0f /* wordWrapWidth 540 / 6 */

static const char* const HINTS[2] = {"B: back", "D-pad: move"};

static struct {
  egglist_args args;
  gc_egg eggs[GC_MAX_EGGS];
  int n;
  egl_state L;
  egm_texts T;
  bool left;
} S;

static const char* species_name(int id, void* ud) {
  (void)ud;
  const char* n = i18n_species_name(id);
  return n ? n : "";
}

static void copy_text(char* out, size_t n, const char* key) {
  const char* raw = i18n_raw(key);
  snprintf(out, n, "%s", raw ? raw : key);
}

static void load_texts(void) {
  copy_text(S.T.egg, sizeof S.T.egg, "egg:egg");
  static const char* const tiers[5] = {"egg:defaultTier", "egg:greatTier", "egg:ultraTier", "egg:masterTier", "egg:manaphyTier"};
  for (int i = 0; i < 5; i++) copy_text(S.T.tier[i], sizeof S.T.tier[i], tiers[i]);
  static const char* const waves[4] = {"egg:hatchWavesMessageSoon", "egg:hatchWavesMessageClose", "egg:hatchWavesMessageNotClose", "egg:hatchWavesMessageLongTime"};
  for (int i = 0; i < 4; i++) copy_text(S.T.waves[i], sizeof S.T.waves[i], waves[i]);
  copy_text(S.T.gacha_legendary, sizeof S.T.gacha_legendary, "egg:gachaTypeLegendary");
  copy_text(S.T.gacha_move, sizeof S.T.gacha_move, "egg:gachaTypeMove");
  copy_text(S.T.gacha_shiny, sizeof S.T.gacha_shiny, "egg:gachaTypeShiny");
  copy_text(S.T.event_type, sizeof S.T.event_type, "egg:eventType");
  copy_text(S.T.same_species, sizeof S.T.same_species, "egg:sameSpeciesEgg"); /* "{{species}} will hatch from this Egg!" */
}

static bool el_input(ui_screen* s, ui_button b) {
  (void)s;
  if (S.left) return true;
  switch (b) {
    case BTN_UP:
    case BTN_DOWN:
    case BTN_LEFT:
    case BTN_RIGHT:
      if (S.n > 0 && egl_input(&S.L, (sg_dir)(b - BTN_UP))) ui_play_select();
      return true;
    case BTN_CANCEL:
      ui_play_select();
      S.left = true;
      if (S.args.on_exit) S.args.on_exit(S.args.arg);
      return true;
    default: return false;
  }
}

/* a tap on the bottom screen (physical px): the hint "B: back" leaves, an icon moves the cursor onto it */
static bool el_touch(ui_screen* s, float x, float y) {
  if (S.left) return true;
  if (y >= HINT_Y - 8) return hint_at(TS_INSTRUCTIONS_TEXT, HINT_X, HINTS, 2, x) == 0 ? el_input(s, BTN_CANCEL) : false;
  if (S.n <= 0) return false;
  const float gx = x - (float)ICON_X0, gy = y - (float)ICON_Y0;
  if (gx < 0 || gy < 0) return false;
  const int col = (int)(gx / CELL), row = (int)(gy / CELL);
  if (col >= EGL_COLS || row >= EGL_ROWS) return false;
  const int cell = row * EGL_COLS + col;
  if (egl_cell_egg(&S.L, cell) < 0) return false; /* an empty cell */
  if (egl_select_cell(&S.L, cell)) ui_play_select();
  return true;
}

static void draw_scroll_bar(void) {
  /* upstream's ScrollBar: shown only when there are more rows than fit; the handle is the share of the rows on screen */
  const int total_rows = (S.n + EGL_COLS - 1) / EGL_COLS;
  if (total_rows <= EGL_ROWS) return;
  const rr_frame* f = img_frame("images/ui/scroll_bar", NULL);
  if (f) rr_nineslice(f, SCROLL_X, SCROLL_Y, SCROLL_W, SCROLL_H, 2);
  else rr_rect(SCROLL_X, SCROLL_Y, SCROLL_W, SCROLL_H, RR_RGBA(0x30, 0x30, 0x30, 255));
  const float bottom_h = 2.0f;
  const float body_h = ((float)SCROLL_H - 1.0f - bottom_h) * EGL_ROWS / (float)total_rows;
  const float y = 1.0f + (((float)SCROLL_H - 1.0f - bottom_h) / (float)total_rows) * (float)S.L.grid.scroll;
  rr_rect(SCROLL_X + 1, SCROLL_Y + y, SCROLL_W - 2, body_h, RR_RGBA(0xaa, 0xaa, 0xaa, 255));
  rr_rect(SCROLL_X + 1, SCROLL_Y + y + body_h, SCROLL_W - 2, bottom_h, RR_RGBA(0x70, 0x70, 0x70, 255));
}

static void draw_bottom(void) {
  rr_rect(0, 0, CANVAS_W, CANVAS_H, RR_RGBA(0x00, 0x68, 0x60, 255));
  img_draw("images/ui/egg_list_bg", NULL, 0, 0, 0, 0);
  if (S.n <= 0) {
    char msg[160];
    i18n_t(msg, sizeof msg, "menuUiHandler:noEggs", NULL);
    ui_window(1, 70, 318, 44, WIN_NORMAL);
    text_draw(TS_WINDOW, 12, 78, msg, NULL);
  } else {
    egm_details d;
    egm_describe(&S.eggs[S.L.detail], &S.T, species_name, NULL, S.args.tz_offset ? S.args.tz_offset(S.eggs[S.L.detail].timestamp_ms) : 0, &d);
    ui_window(1, 85, 106, 22, WIN_NORMAL);
    ui_window(1, 102, 106, 50, WIN_NORMAL);
    ui_window(1, 147, 106, 32, WIN_NORMAL);
    ui_window(107, 1, 212, 178, WIN_NORMAL);
    char frame[16];
    snprintf(frame, sizeof frame, "egg_%s", d.key);
    img_draw("images/egg/egg", frame, 54, 37, 0.5f, 0.5f);
    text_draw(TS_SUMMARY, 8, 68, d.name, NULL);
    text_draw(TS_EGG_LIST, 8, 91, d.date, NULL);
    text_opts o = text_defaults();
    o.wrap_w = WRAP_W;
    text_draw(TS_EGG_LIST, 8, 108, d.waves, &o);
    text_draw(TS_EGG_LIST, 8, 152, d.type, &o);
    /* the icons: upstream's sprites at scale 0.5, the one under the cursor bobbing between 0 and -2 every 200 ms (PokemonIconAnimHelper, ACTIVE) */
    const int bob = ((int)(ui_now() / 200.0f)) & 1;
    for (int i = 0; i < egl_cell_count(&S.L); i++) {
      const int e = egl_cell_egg(&S.L, i);
      if (e < 0) continue;
      char key[8];
      egm_key(&S.eggs[e], key, sizeof key);
      const rr_frame* f = img_frame("images/egg/egg_icons", key);
      if (!f) continue;
      rr_draw_opts ro = RR_DEFAULT_OPTS;
      ro.scale_x = ro.scale_y = 0.5f;
      rr_draw(f, (float)(ICON_X0 + (i % EGL_COLS) * CELL - 2), (float)(ICON_Y0 + (i / EGL_COLS) * CELL + 2 - (i == S.L.cursor && bob ? 2 : 0)), &ro);
    }
    const int c = S.L.cursor;
    if (c >= 0) img_draw("images/ui/select_cursor", NULL, (float)(ICON_X0 + 1 + (c % EGL_COLS) * CELL - 2), (float)(ICON_Y0 + (c / EGL_COLS) * CELL + 2), 0, 0);
    draw_scroll_bar();
  }
  for (int i = 0; i < 2; i++) text_draw(TS_INSTRUCTIONS_TEXT, hint_x(TS_INSTRUCTIONS_TEXT, HINT_X, HINTS, i), HINT_Y, HINTS[i], NULL);
}

static void el_draw(ui_screen* s, rr_screen which) {
  (void)s;
  if (which == RR_TOP) {
    if (ui_space(UI_PHYS_TOP)) rr_rect(0, 0, 400, 240, RR_RGBA(0x10, 0x10, 0x18, 255));
    return;
  }
  if (!ui_space(UI_PHYS_BOTTOM)) return;
  rr_rect(0, 0, 320, 240, RR_RGBA(0x10, 0x10, 0x18, 255));
  draw_bottom();
}

static ui_screen g_screen = {"egglist", UI_FADE, NULL, NULL, NULL, el_input, NULL, el_draw, NULL, el_touch};

void egglist_screen_open(const egglist_args* a) {
  memset(&S, 0, sizeof S);
  S.args = *a;
  S.n = a->n < 0 ? 0 : a->n > GC_MAX_EGGS ? GC_MAX_EGGS : a->n;
  if (S.n > 0) memcpy(S.eggs, a->eggs, (size_t)S.n * sizeof S.eggs[0]);
  load_texts();
  if (S.n > 0) egl_open(&S.L, S.n);
  ui_set(&g_screen);
}
