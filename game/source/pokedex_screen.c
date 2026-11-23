/* The Pokedex list screen (see pokedex_screen.h). Geometry is upstream's (a 320 x 180 canvas, like the bottom screen). */
#include "pokedex_screen.h"

#include <stdio.h>
#include <string.h>

#include "i18n.h"
#include "pokedex_assets.h"
#include "pokedex_model.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

#define CANVAS_W 320
#define CANVAS_H 180
#define BOX_X 149 /* starterBoxContainer: speciesContainerX 143 + 6 */
#define BOX_Y 9
#define CELL_W 18
#define CELL_H 17
#define ROW0_Y 13 /* calcStarterPosition's yOffset */
#define NAME_X 6
#define NAME_Y 128
#define NUMBER_Y 141
#define TYPE_X 7 /* upstream's labels are 16 x 7 at (10, 158) and (28, 158); at 1:1 pixels they are drawn full size, 35 px apart, like the starter select */
#define TYPE_DX 35
#define TYPE_Y 155
#define HINT_X 10
#define HINT_Y 224
#define SCROLL_X 310 /* BOX_X + 161 */
#define SCROLL_Y 21  /* BOX_Y + 12 */
#define SCROLL_W 5
#define SCROLL_H 155 /* the window's 161 - 6 */

static const char* const HINTS[2] = {"B: back", "D-pad: move"};

static struct {
  pokedex_args args;
  pdx_state L;
  bool left;
} S;

static float cell_x(int i) { return (float)(BOX_X + (i % PDX_COLS) * CELL_W); }
static float cell_y(int i) { return (float)(BOX_Y + ROW0_Y + (i / PDX_COLS) * CELL_H); }

static bool pd_input(ui_screen* s, ui_button b) {
  (void)s;
  if (S.left) return true;
  switch (b) {
    case BTN_UP:
    case BTN_DOWN:
    case BTN_LEFT:
    case BTN_RIGHT: {
      pdx_mode mode = PDX_GRID;
      /* a press that upstream answers by entering the filter bar or the filter text changes nothing here (not ported yet) */
      if (pdx_input(&S.L, (pdx_button)(b - BTN_UP), &mode) && mode == PDX_GRID) ui_play_select();
      return true;
    }
    case BTN_CANCEL:
      ui_play_select();
      S.left = true;
      if (S.args.on_exit) S.args.on_exit(S.args.arg);
      return true;
    default: return false;
  }
}

/* a tap on the bottom screen (physical px): the hint "B: back" leaves, an icon moves the cursor onto it */
static bool pd_touch(ui_screen* s, float x, float y) {
  if (S.left) return true;
  if (y >= HINT_Y - 8) return hint_at(TS_INSTRUCTIONS_TEXT, HINT_X, HINTS, 2, x) == 0 ? pd_input(s, BTN_CANCEL) : false;
  const float gx = x - (float)BOX_X, gy = y - (float)(BOX_Y + ROW0_Y);
  if (gx < 0 || gy < 0) return false;
  const int col = (int)(gx / CELL_W), row = (int)(gy / CELL_H);
  if (col >= PDX_COLS || row >= PDX_ROWS) return false;
  if (!pdx_select_cell(&S.L, row * PDX_COLS + col)) return false;
  ui_play_select();
  return true;
}

static void draw_scroll_bar(void) {
  /* upstream's ScrollBar: shown only when there are more rows than fit; the handle is the share of the rows on screen */
  const int total_rows = (S.L.n + PDX_COLS - 1) / PDX_COLS;
  if (total_rows <= PDX_ROWS) return;
  const rr_frame* f = img_frame("images/ui/scroll_bar", NULL);
  if (f) rr_nineslice(f, SCROLL_X, SCROLL_Y, SCROLL_W, SCROLL_H, 2);
  else rr_rect(SCROLL_X, SCROLL_Y, SCROLL_W, SCROLL_H, RR_RGBA(0x30, 0x30, 0x30, 255));
  const float bottom_h = 2.0f;
  const float body_h = ((float)SCROLL_H - 1.0f - bottom_h) * PDX_ROWS / (float)total_rows;
  const float y = 1.0f + (((float)SCROLL_H - 1.0f - bottom_h) / (float)total_rows) * (float)S.L.scroll;
  rr_rect(SCROLL_X + 1, SCROLL_Y + y, SCROLL_W - 2, body_h, RR_RGBA(0xaa, 0xaa, 0xaa, 255));
  rr_rect(SCROLL_X + 1, SCROLL_Y + y + body_h, SCROLL_W - 2, bottom_h, RR_RGBA(0x70, 0x70, 0x70, 255));
}

static void draw_panel(void) {
  pdx_panel p;
  pdx_panel_of(&S.L, &p);
  char number[64], dex[8];
  snprintf(dex, sizeof dex, "%04d", p.dex_number);
  i18n_t(number, sizeof number, "pokedexUiHandler:pokemonNumber", "dexNo", dex, NULL);
  text_draw(TS_SUMMARY, NAME_X, NUMBER_Y, number, NULL);
  const char* name = p.name_shown ? i18n_species_name(p.species) : "???";
  text_draw(TS_SUMMARY, NAME_X, NAME_Y, name ? name : "???", NULL);
  if (p.type1 >= 0) pxa_type(p.type1, TYPE_X, TYPE_Y);
  if (p.type2 >= 0) pxa_type(p.type2, TYPE_X + TYPE_DX, TYPE_Y);
}

static void draw_bottom(void) {
  rr_rect(0, 0, CANVAS_W, CANVAS_H, RR_RGBA(0x00, 0x68, 0x60, 255));
  ui_window(143, 18, 175, 161, WIN_NORMAL);
  img_draw("images/ui/starter_container_bg", NULL, 144, 19, 0, 0);
  draw_panel();
  /* the icons: upstream's sprites at scale 0.5 at (-2, 2) in the cell, tinted by what is known of the species; the one under the cursor, when its
   * line is caught, bobs between 0 and -2 every 200 ms (PokemonIconAnimHelper, ACTIVE) */
  const int bob = ((int)(ui_now() / 200.0f)) & 1;
  const uint16_t cur_species = S.L.cell_species[S.L.cursor];
  const bool cur_caught = S.args.sys->dex[pdx_starter_of(cur_species)].caught_attr != 0;
  for (int i = 0; i < PDX_CELLS; i++) {
    if (!pdx_cell_visible(&S.L, i)) continue;
    const char t = pdx_cell_tint(&S.L, i);
    const rr_color tint = t == 'c' ? RR_RGBA(255, 255, 255, 255) : t == 'g' ? RR_RGBA(0x80, 0x80, 0x80, 255) : RR_RGBA(0, 0, 0, 255);
    pxa_icon(S.L.cell_species[i], cell_x(i) - 2, cell_y(i) + 2 - (i == S.L.cursor && cur_caught && bob ? 2 : 0), 0.5f, tint);
  }
  if (pdx_cell_visible(&S.L, S.L.cursor)) img_draw("images/ui/select_cursor", NULL, cell_x(S.L.cursor) - 1, cell_y(S.L.cursor) + 1, 0, 0);
  draw_scroll_bar();
  for (int i = 0; i < 2; i++) text_draw(TS_INSTRUCTIONS_TEXT, hint_x(TS_INSTRUCTIONS_TEXT, HINT_X, HINTS, i), HINT_Y, HINTS[i], NULL);
}

static void pd_draw(ui_screen* s, rr_screen which) {
  (void)s;
  if (which == RR_TOP) {
    if (ui_space(UI_PHYS_TOP)) rr_rect(0, 0, 400, 240, RR_RGBA(0x10, 0x10, 0x18, 255));
    return;
  }
  if (!ui_space(UI_PHYS_BOTTOM)) return;
  rr_rect(0, 0, 320, 240, RR_RGBA(0x10, 0x10, 0x18, 255));
  draw_bottom();
}

static ui_screen g_screen = {"pokedex", UI_FADE, NULL, NULL, NULL, pd_input, NULL, pd_draw, NULL, pd_touch};

void pokedex_screen_open(const pokedex_args* a) {
  memset(&S, 0, sizeof S);
  S.args = *a;
  pdx_open(&S.L, a->sys);
  ui_set(&g_screen);
}
