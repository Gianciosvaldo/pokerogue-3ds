/* pokedex_screen_test: the Pokedex list screen (source/pokedex_screen.c) on the host, over the real model and a fake drawing layer.
 *
 *   make pokedex_screen_test && build/pokedex_screen_test
 *
 * Checks: the 81 icons (upstream's cells, half size, the tint of what is known of each species), the cursor and the bob of the icon under it only
 * for a caught line, the panel (number, name or "???", the type labels), D-pad moves with the sound only when the screen stayed in the grid,
 * scrolling a row at a time with the scroll bar, a tap on an icon, B leaves once, the hint, nothing drawn on the top screen, and the profile is
 * read and never written. Exit 1 on a failed check. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gc/data.h"
#include "i18n.h"
#include "pokedex_assets.h"
#include "pokedex_filter_menu.h"
#include "pokedex_model.h"
#include "pokedex_screen.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

static int checks, failed;
#define CHECK(cond, ...)                               \
  do {                                                 \
    checks++;                                          \
    if (!(cond)) {                                     \
      failed++;                                        \
      if (failed < 40) {                               \
        printf("FAIL line %d: %s: ", __LINE__, #cond); \
        printf(__VA_ARGS__);                           \
        printf("\n");                                  \
      }                                                \
    }                                                  \
  } while (0)

/* ------------------------------------------------------------------ fakes */

typedef struct {
  float x, y;
  int screen, style;
  char text[200];
} drawn_t;
#define DRAWN_MAX 256
static drawn_t g_drawn[DRAWN_MAX];
static int n_drawn, g_which;

typedef struct {
  char key[48];
  float x, y;
} image_t;
static image_t g_images[64];
static int n_images;

typedef struct {
  int species;
  float x, y, scale;
  rr_color tint;
} icon_t;
static icon_t g_icons[200];
static int n_icons;
typedef struct {
  int type;
  float x, y;
} label_t;
static label_t g_labels[8];
static int n_labels;

typedef struct { float x, y, w, h; } rect_t;
static rect_t g_rects[32];
static int n_recs;

static ui_screen* g_top;
static int n_select, n_exit, n_rects, n_nine;
static float g_now;

void ui_set(ui_screen* s) { g_top = s; }
void ui_play_select(void) { n_select++; }
void ui_play_error(void) {}
float ui_now(void) { return g_now; }
bool ui_space(ui_space_id sp) {
  const bool top = sp == UI_FIELD || sp == UI_FIELD_UI || sp == UI_TOP_CANVAS || sp == UI_TOP_CANVAS_UI || sp == UI_PHYS_TOP;
  return top == (g_which == 0);
}
void rr_rect(float x, float y, float w, float h, rr_color c) {
  (void)x, (void)y, (void)w, (void)h, (void)c;
  if (g_which == 1) {
    n_rects++;
    if (n_recs < 32) g_rects[n_recs++] = (rect_t){x, y, w, h};
  }
}
void rr_nineslice(const rr_frame* f, float x, float y, float w, float h, int e) {
  (void)f, (void)x, (void)y, (void)w, (void)h, (void)e;
  if (g_which == 1) n_nine++;
}
static rr_frame g_frame;
const rr_frame* img_frame(const char* key, const char* frame) {
  (void)key, (void)frame;
  return &g_frame;
}
void img_draw(const char* key, const char* frame, float x, float y, float ox, float oy) {
  (void)frame, (void)ox, (void)oy;
  if (n_images < 64) {
    snprintf(g_images[n_images].key, sizeof g_images[n_images].key, "%s", key);
    g_images[n_images].x = x;
    g_images[n_images].y = y;
    n_images++;
  }
}
void ui_window(float x, float y, float w, float h, win_variant v) { (void)x, (void)y, (void)w, (void)h, (void)v; }
float text_width(text_style st, const char* s, const text_opts* o) {
  (void)st, (void)o;
  return 6.0f * (float)strlen(s);
}
float hint_x(text_style st, float x0, const char* const* texts, int i) {
  float x = x0;
  for (int k = 0; k < i; k++) x += text_width(st, texts[k], NULL) + HINT_GAP;
  return x;
}
int hint_at(text_style st, float x0, const char* const* texts, int n, float x) {
  for (int i = 0; i < n; i++)
    if (x < hint_x(st, x0, texts, i + 1) - HINT_GAP / 2.0f) return i;
  return -1;
}
float text_draw(text_style st, float x, float y, const char* s, const text_opts* o) {
  (void)o;
  if (n_drawn < DRAWN_MAX) {
    drawn_t* d = &g_drawn[n_drawn++];
    d->x = x;
    d->y = y;
    d->screen = g_which;
    d->style = (int)st;
    snprintf(d->text, sizeof d->text, "%s", s);
  }
  return 6.0f * (float)strlen(s);
}
void pxa_icon(int species, float x, float y, float scale, rr_color tint) {
  if (n_icons < 200) g_icons[n_icons++] = (icon_t){species, x, y, scale, tint};
}
void pxa_type(int type, float x, float y) {
  if (n_labels < 8) g_labels[n_labels++] = (label_t){type, x, y};
}
const char* i18n_t(char* out, int n, const char* key, ...) {
  if (!strcmp(key, "pokedexUiHandler:pokemonNumber")) {
    va_list ap;
    va_start(ap, key);
    (void)va_arg(ap, const char*); /* "dexNo" */
    const char* v = va_arg(ap, const char*);
    va_end(ap);
    snprintf(out, (size_t)n, "No. %s", v);
  } else snprintf(out, (size_t)n, "%s", key);
  return out;
}
/* the options overlay: the menu's rows and its callback, driven by the test */
static const char* g_opt_labels[40];
static char g_opt_text[40][64];
static int g_opt_n, n_menus;
static void (*g_opt_done)(int, void*);
void ui_push_options(const char* const* labels, int n, bool can_cancel, void (*on_done)(int idx, void* arg), void* arg) {
  (void)can_cancel, (void)arg;
  g_opt_n = n > 40 ? 40 : n;
  for (int i = 0; i < g_opt_n; i++) {
    snprintf(g_opt_text[i], sizeof g_opt_text[i], "%s", labels[i]);
    g_opt_labels[i] = g_opt_text[i];
  }
  g_opt_done = on_done;
  n_menus++;
}
const char* i18n_type_name(int type) {
  static char buf[16];
  snprintf(buf, sizeof buf, "Type%d", type);
  return buf;
}
const char* i18n_species_name(int id) {
  static char buf[32];
  snprintf(buf, sizeof buf, "Species%d", id);
  return buf;
}

/* ------------------------------------------------------------------ helpers */

static gc_system sys;
static int one = 1;
static bool pokerus_fake(uint16_t id) { return id == 4 || id == 16; }
static const char* name_fake(uint16_t id) { return i18n_species_name(id); }
static void exited(void* arg) { n_exit += *(int*)arg; }

static void draw(int which) {
  n_drawn = n_images = n_rects = n_nine = n_icons = n_labels = n_recs = 0;
  g_which = which;
  g_top->draw(g_top, which ? RR_BOTTOM : RR_TOP);
}
static const drawn_t* find_at(float x, float y) {
  for (int i = 0; i < n_drawn; i++)
    if (g_drawn[i].screen == 1 && g_drawn[i].x == x && g_drawn[i].y == y) return &g_drawn[i];
  return NULL;
}
static const icon_t* icon_at(float x, float y) {
  for (int i = 0; i < n_icons; i++)
    if (g_icons[i].x == x && g_icons[i].y == y) return &g_icons[i];
  return NULL;
}
static const image_t* image(const char* key) {
  for (int i = 0; i < n_images; i++)
    if (!strcmp(g_images[i].key, key)) return &g_images[i];
  return NULL;
}
static void press(ui_button b) { g_top->input(g_top, b); }
static rr_color tint_of(char t) { return t == 'c' ? RR_RGBA(255, 255, 255, 255) : t == 'g' ? RR_RGBA(0x80, 0x80, 0x80, 255) : RR_RGBA(0, 0, 0, 255); }

/* the screen against a model driven by the same presses: every visible cell's icon where upstream puts it with its tint, the cursor, the panel */
static void check_all(const pdx_state* m, int bob, const char* what) {
  draw(1);
  int shown = 0;
  for (int i = 0; i < PDX_CELLS; i++) {
    const float x = (float)(149 + (i % 9) * 18 - 2);
    const float y = (float)(9 + 13 + (i / 9) * 17 + 2);
    if (!pdx_cell_visible(m, i)) {
      CHECK(!icon_at(x, y) && !icon_at(x, y - 2), "%s: cell %d is empty", what, i);
      continue;
    }
    const bool bobbing = i == m->cursor && bob && sys.dex[pdx_starter_of(m->cell_species[i])].caught_attr != 0;
    const icon_t* ic = icon_at(x, bobbing ? y - 2 : y);
    CHECK(ic && ic->species == m->cell_species[i] && ic->scale == 0.5f && ic->tint == tint_of(pdx_cell_tint(m, i)), "%s: cell %d icon (%s)", what, i,
          ic ? "wrong" : "none");
    if (ic) shown++;
  }
  CHECK(n_icons == shown, "%s: %d icons drawn, %d found", what, n_icons, shown);
  {
    /* the scroll bar's handle: 311 wide 3, at 1 + (155 - 1 - 2) / rows * scrolled rows below the bar's top (21), 152 * 9 / rows tall */
    const int rows = (m->n + 8) / 9;
    int found = 0;
    for (int i = 0; i < n_recs; i++)
      if (g_rects[i].x == 311 && g_rects[i].w == 3 && g_rects[i].h > 3) {
        const float ey = 21 + 1 + (152.0f / (float)rows) * (float)m->scroll;
        CHECK(g_rects[i].y == ey && g_rects[i].h == 152.0f * 9 / (float)rows, "%s: the scroll handle at %.2f (%.2f tall), expected %.2f", what, g_rects[i].y, g_rects[i].h, ey);
        found++;
      }
    CHECK(found == (rows > 9 ? 1 : 0), "%s: %d scroll handles for %d rows", what, found, rows);
  }
  const float cx = (float)(149 + (m->cursor % 9) * 18 - 1), cy = (float)(9 + 13 + (m->cursor / 9) * 17 + 1);
  const image_t* cur = image("images/ui/select_cursor");
  CHECK(cur && cur->x == cx && cur->y == cy, "%s: the cursor on cell %d", what, m->cursor);
  pdx_panel p;
  pdx_panel_of(m, &p);
  char num[16], name[32];
  snprintf(num, sizeof num, "No. %04d", p.dex_number);
  snprintf(name, sizeof name, "%s", p.name_shown ? i18n_species_name(p.species) : "???");
  const drawn_t* dn = find_at(6, 141);
  const drawn_t* dm = find_at(6, 128);
  CHECK(dn && !strcmp(dn->text, num) && dn->style == TS_SUMMARY, "%s: number %s", what, dn ? dn->text : "(none)");
  CHECK(dm && !strcmp(dm->text, name) && dm->style == TS_SUMMARY, "%s: name %s, expected %s", what, dm ? dm->text : "(none)", name);
  const int nl = (p.type1 >= 0) + (p.type2 >= 0);
  CHECK(n_labels == nl, "%s: %d type labels, expected %d", what, n_labels, nl);
  if (p.type1 >= 0) CHECK(n_labels > 0 && g_labels[0].type == p.type1 && g_labels[0].x == 7 && g_labels[0].y == 155, "%s: first label", what);
  if (p.type2 >= 0) CHECK(n_labels > 1 && g_labels[1].type == p.type2 && g_labels[1].x == 42 && g_labels[1].y == 155, "%s: second label", what);
}

int main(void) {
  /* the profile: Bulbasaur caught (so its line is seen), Charmander (4) seen only, Squirtle (7) nothing, Pidgey (16) caught */
  memset(&sys, 0, sizeof sys);
  sys.dex[1].caught_attr = gc_species_full_unlocks_data(1);
  sys.dex[1].seen_attr = sys.dex[1].caught_attr;
  sys.dex[4].seen_attr = 1;
  sys.dex[16].caught_attr = gc_species_full_unlocks_data(16);
  static gc_system before;
  memcpy(&before, &sys, sizeof sys);
  pdx_state m;
  pdx_open(&m, &sys);

  pokedex_args a = {&sys, pokerus_fake, exited, &one};
  pokedex_screen_open(&a);
  CHECK(g_top != NULL, "the screen is up");
  g_now = 0;
  check_all(&m, 0, "open");
  draw(1);
  CHECK(image("images/ui/starter_container_bg") != NULL, "the grid's background image");
  CHECK(n_nine == 1, "the scroll bar over a list longer than nine rows (%d)", n_nine);
  {
    const icon_t* i0 = icon_at(147, 24);
    CHECK(i0 && i0->tint == tint_of('c'), "Bulbasaur is in colour");
    const icon_t* i3 = icon_at(147 + 3 * 18, 24);
    CHECK(i3 && i3->tint == tint_of('g'), "Charmander, seen, is grey");
    const icon_t* i6 = icon_at(147 + 6 * 18, 24);
    CHECK(i6 && i6->tint == tint_of('b'), "Squirtle, not seen, is black");
  }
  {
    pdx_panel p;
    pdx_panel_of(&m, &p);
    CHECK(p.type1 >= 0, "Bulbasaur's types are shown");
  }

  /* the icon under the cursor bobs between 0 and -2 every 200 ms, when its line is caught */
  g_now = 200;
  check_all(&m, 1, "bob");
  g_now = 0;

  /* D-pad: RIGHT to Ivysaur (seen through Bulbasaur), the panel follows; to Charmander (unseen name is shown for a seen entry), then to Squirtle's: "???" */
  int sel = n_select;
  press(BTN_RIGHT);
  pdx_mode mode;
  pdx_input(&m, PDX_RIGHT, &mode);
  CHECK(n_select == sel + 1, "a move plays the sound");
  check_all(&m, 0, "right");
  for (int k = 0; k < 5; k++) {
    press(BTN_RIGHT);
    pdx_input(&m, PDX_RIGHT, &mode);
  }
  check_all(&m, 0, "on Squirtle");
  g_now = 200;
  check_all(&m, 1, "on Squirtle, bob phase"); /* an uncaught line does not bob */
  g_now = 0;
  {
    draw(1);
    CHECK(find_at(6, 128) && !strcmp(find_at(6, 128)->text, "???"), "an unseen species is ???");
    CHECK(n_labels == 0, "and shows no types");
  }

  /* UP in the first row enters upstream's filter bar: the filter menu opens (the grid is as it was) */
  sel = n_select;
  const int menus0 = n_menus;
  press(BTN_UP);
  CHECK(n_select == sel + 1 && n_menus == menus0 + 1, "UP in the first row opens the filter menu");
  g_opt_done(-2, NULL); /* cancelled: closed */
  check_all(&m, 0, "up in the first row");

  /* DOWN scrolls a row at a time past nine rows; the model is driven by the same presses */
  for (int k = 0; k < 14; k++) {
    press(BTN_DOWN);
    pdx_input(&m, PDX_DOWN, &mode);
  }
  CHECK(m.scroll > 0, "the list scrolled");
  check_all(&m, 0, "scrolled");
  for (int k = 0; k < 400; k++) {
    press(BTN_DOWN);
    pdx_input(&m, PDX_DOWN, &mode);
  }
  check_all(&m, 0, "far down");
  for (int k = 0; k < 400; k++) {
    press(BTN_UP);
    pdx_input(&m, PDX_UP, &mode);
  }
  check_all(&m, 0, "back up");

  /* a tap on an icon moves the cursor there (physical px are the grid's: cell 20 is column 2, row 2) */
  pdx_open(&m, &sys);
  pokedex_screen_open(&a);
  sel = n_select;
  CHECK(g_top->touch(g_top, 149 + 2 * 18 + 5, 22 + 2 * 17 + 5), "a tap on an icon is taken");
  pdx_select_cell(&m, 2 * 9 + 2);
  CHECK(n_select == sel + 1, "and plays the sound");
  check_all(&m, 0, "tap");
  CHECK(!g_top->touch(g_top, 10, 100), "a tap left of the grid is not taken");
  CHECK(!g_top->touch(g_top, 149 + 9 * 18 + 2, 40), "nor one right of the nine columns");

  /* a tap on the hint "B: back" leaves; one on the other hint does not */
  {
    const int ex = n_exit;
    CHECK(!g_top->touch(g_top, 80, 224) && n_exit == ex, "a tap on the D-pad hint does nothing");
    CHECK(g_top->touch(g_top, 12, 224) && n_exit == ex + 1, "a tap on the B hint leaves");
    n_exit = 0;
    pokedex_screen_open(&a);
  }


  /* ---- the filter menu: UP in the first row and a tap on the button open it; its rows change the filter and the list is rebuilt */
  {
    pdx_open(&m, &sys);
    m.name = name_fake;
    m.pokerus = pokerus_fake;
    pokedex_screen_open(&a);
    draw(1);
    CHECK(find_at(12, 3) && !strcmp(find_at(12, 3)->text, "Filter"), "the filter button says Filter");
    int menus = n_menus;
    press(BTN_LEFT); /* LEFT at the first cell enters the text filters upstream: not ported, nothing opens */
    CHECK(n_menus == menus, "LEFT at the edge opens nothing");
    for (int k = 0; k < 3; k++) {
      press(BTN_RIGHT);
      pdx_input(&m, PDX_RIGHT, &mode);
    }
    sel = n_select;
    press(BTN_UP);
    CHECK(n_menus == menus + 1 && n_select == sel + 1, "UP in the first row opens the filter menu with the sound");
    CHECK(g_opt_n == 16 && !strcmp(g_opt_labels[0], "Generation: all") && !strcmp(g_opt_labels[2], "Caught: all") && !strcmp(g_opt_labels[3], "Passive: Any") &&
              !strcmp(g_opt_labels[5], "Starter: Any") && !strcmp(g_opt_labels[8], "Seen: Any") && !strcmp(g_opt_labels[12], "Sort by: Number") &&
              !strcmp(g_opt_labels[14], "Reset") && !strcmp(g_opt_labels[15], "Done"),
          "the top menu: %d rows, first '%s'", g_opt_n, g_opt_labels[0]);
    /* Seen: Yes -> the list of seen species, the cursor on its first */
    pdf_filter want;
    pdf_reset(&want);
    want.seen = PDF_ON;
    g_opt_done(8, NULL);
    pdx_apply_filter(&m, &want);
    m.cursor = 0;
    CHECK(n_menus == menus + 2 && !strcmp(g_opt_labels[8], "Seen: Yes"), "a tri-state steps and the menu is shown again (%s)", g_opt_labels[8]);
    CHECK(m.n < 1084 && m.n > 0, "the seen species are %d", m.n);
    check_all(&m, 0, "seen only");
    draw(1);
    CHECK(find_at(12, 3) && !strcmp(find_at(12, 3)->text, "Filter (1)"), "the button counts the active column");
    /* the cursor goes to the first species of a list, even when it was elsewhere */
    g_opt_done(8, NULL); /* Seen: No */
    want.seen = PDF_EXCLUDE;
    pdx_apply_filter(&m, &want);
    m.cursor = 0;
    check_all(&m, 0, "unseen only");
    g_opt_done(8, NULL); /* Seen: Any */
    pdf_reset(&want);
    pdx_apply_filter(&m, &want);
    check_all(&m, 0, "all again");
    /* Generation: a check box list, the last generation cannot be cleared */
    g_opt_done(0, NULL);
    CHECK(g_opt_n == 10 && !strcmp(g_opt_labels[0], "[x] Gen 1") && !strcmp(g_opt_labels[9], "Back"), "the generation list");
    for (int g = 1; g < 9; g++) g_opt_done(g, NULL); /* off: 2..9 */
    CHECK(!strcmp(g_opt_labels[0], "[x] Gen 1") && !strcmp(g_opt_labels[1], "[ ] Gen 2"), "generations 2 to 9 are off");
    g_opt_done(0, NULL); /* the last one stays on */
    CHECK(!strcmp(g_opt_labels[0], "[x] Gen 1"), "the last generation stays on");
    want.gens = 1;
    pdx_apply_filter(&m, &want);
    m.cursor = 0;
    check_all(&m, 0, "generation 1");
    CHECK(m.n == 151 + 0 || m.n > 100, "generation 1 has %d species", m.n);
    g_opt_done(9, NULL); /* Back */
    CHECK(!strcmp(g_opt_labels[0], "Generation: 1 of 9"), "back to the top menu (%s)", g_opt_labels[0]);
    /* Passive and Cost reduction step through the states they offer */
    {
      const char* const p[4] = {"Passive: Unlocked", "Passive: Can unlock now", "Passive: Locked", "Passive: Any"};
      for (int k = 0; k < 4; k++) {
        g_opt_done(3, NULL);
        CHECK(!strcmp(g_opt_labels[3], p[k]), "passive step %d: '%s'", k, g_opt_labels[3]);
      }
      const char* const r[6] = {"Cost reduction: Yes", "Cost reduction: Bought once", "Cost reduction: Bought twice", "Cost reduction: Can unlock now", "Cost reduction: Locked", "Cost reduction: Any"};
      for (int k = 0; k < 6; k++) {
        g_opt_done(4, NULL);
        CHECK(!strcmp(g_opt_labels[4], r[k]), "reduction step %d: '%s'", k, g_opt_labels[4]);
      }
    }
    /* Caught lists five boxes, the fifth "Not caught"; the last box on stays on */
    g_opt_done(2, NULL);
    CHECK(g_opt_n == 6 && !strcmp(g_opt_labels[4], "[x] Not caught"), "the caught list has Not caught");
    for (int k = 0; k < 4; k++) g_opt_done(k, NULL);
    g_opt_done(4, NULL);
    CHECK(!strcmp(g_opt_labels[0], "[ ] Epic shiny") && !strcmp(g_opt_labels[4], "[x] Not caught"), "only Not caught is on and it cannot be cleared");
    for (int k = 0; k < 4; k++) g_opt_done(k, NULL);
    CHECK(!strcmp(g_opt_labels[3], "[x] Normal"), "the others come back on");
    g_opt_done(5, NULL);
    /* Sort and order step; Reset clears everything */
    g_opt_done(12, NULL);
    want.sort = PDF_SORT_COST;
    pdx_apply_filter(&m, &want);
    m.cursor = 0;
    check_all(&m, 0, "sorted by cost");
    CHECK(!strcmp(g_opt_labels[12], "Sort by: Cost"), "the sort steps to cost (%s)", g_opt_labels[12]);
    for (int k = PDF_SORT_CANDY; k <= PDF_SORT_HATCHED; k++) {
      g_opt_done(12, NULL);
      want.sort = (uint8_t)k;
      pdx_apply_filter(&m, &want);
      m.cursor = 0;
      char what[24];
      snprintf(what, sizeof what, "sort %d", k);
      check_all(&m, 0, what);
    }
    CHECK(!strcmp(g_opt_labels[12], "Sort by: Times hatched"), "the last sort is times hatched (%s)", g_opt_labels[12]);
    g_opt_done(12, NULL); /* back to number */
    want.sort = PDF_SORT_COST;
    g_opt_done(12, NULL);
    pdx_apply_filter(&m, &want);
    m.cursor = 0;
    g_opt_done(13, NULL);
    want.descending = true;
    pdx_apply_filter(&m, &want);
    m.cursor = 0;
    check_all(&m, 0, "descending");
    CHECK(!strcmp(g_opt_labels[13], "Order: Descending"), "the order flips");
    /* Pokerus: only the starters the screen was told have it (4 and 16) */
    g_opt_done(11, NULL);
    want.pokerus = PDF_ON;
    pdx_apply_filter(&m, &want);
    m.cursor = 0;
    check_all(&m, 0, "pokerus");
    CHECK(m.n == 2, "two species have Pokerus (%d)", m.n);
    /* an empty list: Pokerus on and the Starter filter off the list (No) leaves nothing, the screen says so and draws no icons, panel or cursor */
    g_opt_done(5, NULL); /* Starter: Yes */
    g_opt_done(5, NULL); /* Starter: No: the two are starters, so nothing fits */
    want.starter = PDF_EXCLUDE;
    pdx_apply_filter(&m, &want);
    m.cursor = 0;
    CHECK(m.n == 0, "no starter lacks being a starter (%d)", m.n);
    draw(1);
    CHECK(n_icons == 0 && image("images/ui/select_cursor") == NULL && n_labels == 0, "an empty list draws no icon, cursor or type label");
    CHECK(find_at(6, 141) == NULL && find_at(6, 128) == NULL, "and no panel");
    {
      int msg = 0;
      for (int i = 0; i < n_drawn; i++)
        if (!strcmp(g_drawn[i].text, "No species fit the filter.")) msg++;
      CHECK(msg == 1, "it says so once (%d)", msg);
    }
    sel = n_select;
    press(BTN_DOWN);
    press(BTN_LEFT);
    CHECK(n_select == sel + 1 || n_select == sel, "presses on an empty list do not crash");
    /* Reset */
    g_opt_done(14, NULL);
    pdf_reset(&want);
    pdx_apply_filter(&m, &want);
    m.cursor = 0;
    check_all(&m, 0, "reset");
    CHECK(!strcmp(g_opt_labels[0], "Generation: all") && !strcmp(g_opt_labels[12], "Sort by: Number") && !strcmp(g_opt_labels[13], "Order: Ascending"), "Reset restores the defaults");
    /* Done closes it: nothing is pushed again */
    menus = n_menus;
    g_opt_done(15, NULL);
    CHECK(n_menus == menus, "Done closes the menu");
    g_opt_done(-2, NULL); /* a cancel on the top menu closes it too */
    CHECK(n_menus == menus, "so does a cancel");
    /* the tap on the button */
    sel = n_select;
    CHECK(g_top->touch(g_top, 20, 8) && n_menus == menus + 1 && n_select == sel + 1, "a tap on the Filter button opens the menu");
    g_opt_done(-2, NULL);
    CHECK(!g_top->touch(g_top, 120, 8) && n_menus == menus + 1, "a tap beside the button opens nothing");
    pokedex_screen_open(&a);
  }

  /* the screen reads the profile and writes nothing */
  CHECK(!memcmp(&before, &sys, sizeof sys), "the profile is untouched");

  /* the top screen: only the background */
  draw(0);
  CHECK(n_icons == 0 && n_images == 0 && n_drawn == 0, "nothing on the top screen");

  /* the hint, B leaves once */
  draw(1);
  {
    int hints = 0;
    for (int i = 0; i < n_drawn; i++)
      if (g_drawn[i].y == 224 && g_drawn[i].style == TS_INSTRUCTIONS_TEXT) hints++;
    CHECK(hints == 2, "two hints (%d)", hints);
  }
  press(BTN_CANCEL);
  press(BTN_CANCEL);
  CHECK(n_exit == 1, "B leaves once (%d)", n_exit);
  press(BTN_DOWN);
  CHECK(n_exit == 1, "and the screen is closed to presses after that");

  printf("pokedex_screen_test: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
