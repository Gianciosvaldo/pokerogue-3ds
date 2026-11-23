/* egglist_screen_test: the Egg List screen (source/egglist_screen.c) on the host, over the real model and a fake drawing layer.
 *
 *   make egglist_screen_test && build/egglist_screen_test
 *
 * Checks: the panel for the egg under the cursor (picture, name, date in the given time zone, hatch message and type line, at upstream's x and y and
 * with the 90 px wrap), the icons (one per egg, upstream's cells and scale, the tier's frame, the Manaphy one), the cursor and the bob of the icon
 * under it, D-pad moves with the sound only when something moved, scrolling past nine rows with the scroll bar (only over 99 eggs), a tap on an
 * icon, the empty list's message, the eggs are copied, B leaves once, the hint and nothing on the top screen. Exit 1 on a failed check. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "egglist_model.h"
#include "egglist_screen.h"
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
  float x, y, wrap_w;
  int screen, style;
  char text[200];
} drawn_t;
#define DRAWN_MAX 256
static drawn_t g_drawn[DRAWN_MAX];
static int n_drawn, g_which;

typedef struct {
  char key[48], frame[24];
  float x, y, scale;
  int screen;
} image_t;
#define IMAGES_MAX 256
static image_t g_images[IMAGES_MAX];
static int n_images;

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
const rr_draw_opts RR_DEFAULT_OPTS = {0, 0, 1, 1, 0, 0};
void rr_rect(float x, float y, float w, float h, rr_color c) {
  (void)x, (void)y, (void)w, (void)h, (void)c;
  if (g_which == 1) n_rects++;
}
void rr_nineslice(const rr_frame* f, float x, float y, float w, float h, int e) {
  (void)f, (void)x, (void)y, (void)w, (void)h, (void)e;
  if (g_which == 1) n_nine++;
}
/* a frame is a pointer into a table keyed by (key, frame): rr_draw finds which one it is drawing */
static struct {
  char key[48], frame[24];
  rr_frame f;
} g_frames[64];
static int n_frames;
const rr_frame* img_frame(const char* key, const char* frame) {
  const char* fr = frame ? frame : "";
  for (int i = 0; i < n_frames; i++)
    if (!strcmp(g_frames[i].key, key) && !strcmp(g_frames[i].frame, fr)) return &g_frames[i].f;
  if (n_frames >= 64) return NULL;
  snprintf(g_frames[n_frames].key, sizeof g_frames[n_frames].key, "%s", key);
  snprintf(g_frames[n_frames].frame, sizeof g_frames[n_frames].frame, "%s", fr);
  return &g_frames[n_frames++].f;
}
static void record(const rr_frame* f, float x, float y, float scale) {
  for (int i = 0; i < n_frames; i++)
    if (&g_frames[i].f == f && n_images < IMAGES_MAX) {
      image_t* im = &g_images[n_images++];
      snprintf(im->key, sizeof im->key, "%s", g_frames[i].key);
      snprintf(im->frame, sizeof im->frame, "%s", g_frames[i].frame);
      im->x = x;
      im->y = y;
      im->scale = scale;
      im->screen = g_which;
    }
}
void rr_draw(const rr_frame* f, float x, float y, const rr_draw_opts* o) { record(f, x, y, o->scale_x); }
void img_draw(const char* key, const char* frame, float x, float y, float ox, float oy) {
  (void)ox, (void)oy;
  record(img_frame(key, frame), x, y, 1.0f);
}
void ui_window(float x, float y, float w, float h, win_variant v) { (void)x, (void)y, (void)w, (void)h, (void)v; }
text_opts text_defaults(void) {
  text_opts o;
  memset(&o, 0, sizeof o);
  o.chars = TEXT_ALL;
  return o;
}
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
  if (n_drawn < DRAWN_MAX) {
    drawn_t* d = &g_drawn[n_drawn++];
    d->x = x;
    d->y = y;
    d->screen = g_which;
    d->style = (int)st;
    d->wrap_w = o ? o->wrap_w : 0.0f;
    snprintf(d->text, sizeof d->text, "%s", s);
  }
  return 6.0f * (float)strlen(s);
}
/* the locale: upstream's English texts for the keys the screen reads */
static const struct {
  const char *key, *text;
} LOCALE[] = {
    {"egg:egg", "Egg"},
    {"egg:defaultTier", "Common"},
    {"egg:greatTier", "Rare"},
    {"egg:ultraTier", "Epic"},
    {"egg:masterTier", "Legendary"},
    {"egg:manaphyTier", "Manaphy"},
    {"egg:hatchWavesMessageSoon", "Sounds can be heard coming from inside! It will hatch soon!"},
    {"egg:hatchWavesMessageClose", "It appears to move occasionally. It may be close to hatching."},
    {"egg:hatchWavesMessageNotClose", "What will hatch from this? It doesn't seem close to hatching."},
    {"egg:hatchWavesMessageLongTime", "It looks like this Egg will take a long time to hatch."},
    {"egg:gachaTypeLegendary", "Legendary Rate Up"},
    {"egg:gachaTypeMove", "Rare Egg Move Rate Up"},
    {"egg:gachaTypeShiny", "Shiny Rate Up"},
    {"egg:eventType", "Mystery Event"},
    {"egg:sameSpeciesEgg", "{{species}} will hatch from this Egg!"},
};
const char* i18n_raw(const char* key) {
  for (size_t i = 0; i < sizeof LOCALE / sizeof LOCALE[0]; i++)
    if (!strcmp(key, LOCALE[i].key)) return LOCALE[i].text;
  return NULL;
}
const char* i18n_t(char* out, int n, const char* key, ...) {
  if (!strcmp(key, "menuUiHandler:noEggs")) snprintf(out, (size_t)n, "You are not hatching\nany Eggs at the moment!");
  else snprintf(out, (size_t)n, "%s", key);
  return out;
}
const char* i18n_species_name(int id) {
  static char buf[32];
  snprintf(buf, sizeof buf, "Species%d", id);
  return buf;
}

/* ------------------------------------------------------------------ helpers */

static gc_egg eggs[150];
static int one = 1;
static int tz = 0;
static int tz_fn(uint64_t ts) {
  (void)ts;
  return tz;
}
static void exited(void* arg) { n_exit += *(int*)arg; }

static void draw(int which) {
  n_drawn = n_images = n_rects = n_nine = 0;
  g_which = which;
  g_top->draw(g_top, which ? RR_BOTTOM : RR_TOP);
}
static const drawn_t* find_at(float x, float y) {
  for (int i = 0; i < n_drawn; i++)
    if (g_drawn[i].screen == 1 && g_drawn[i].x == x && g_drawn[i].y == y) return &g_drawn[i];
  return NULL;
}
static void press(ui_button b) { g_top->input(g_top, b); }
static int count_images(const char* key) {
  int c = 0;
  for (int i = 0; i < n_images; i++)
    if (!strcmp(g_images[i].key, key)) c++;
  return c;
}
static const image_t* image_at(const char* key, float x, float y) {
  for (int i = 0; i < n_images; i++)
    if (!strcmp(g_images[i].key, key) && g_images[i].x == x && g_images[i].y == y) return &g_images[i];
  return NULL;
}

static void make_eggs(int n) {
  memset(eggs, 0, sizeof eggs);
  for (int i = 0; i < n; i++) {
    eggs[i].id = 1000 + (uint32_t)i;
    eggs[i].tier = (uint8_t)(i % 4);
    eggs[i].species = (uint16_t)(1 + i);
    eggs[i].source_type = (uint8_t)(i % 5);
    eggs[i].hatch_waves = 10 + i;
    eggs[i].timestamp_ms = 1770948693452ull + (uint64_t)i * 86400000ull;
  }
}

static const char* sp_name(int id, void* ud) {
  (void)ud;
  return i18n_species_name(id);
}
static void texts(egm_texts* t) {
  memset(t, 0, sizeof *t);
  snprintf(t->egg, sizeof t->egg, "%s", i18n_raw("egg:egg"));
  static const char* const tiers[5] = {"egg:defaultTier", "egg:greatTier", "egg:ultraTier", "egg:masterTier", "egg:manaphyTier"};
  for (int k = 0; k < 5; k++) snprintf(t->tier[k], sizeof t->tier[k], "%s", i18n_raw(tiers[k]));
  static const char* const waves[4] = {"egg:hatchWavesMessageSoon", "egg:hatchWavesMessageClose", "egg:hatchWavesMessageNotClose", "egg:hatchWavesMessageLongTime"};
  for (int k = 0; k < 4; k++) snprintf(t->waves[k], sizeof t->waves[k], "%s", i18n_raw(waves[k]));
  snprintf(t->gacha_legendary, sizeof t->gacha_legendary, "%s", i18n_raw("egg:gachaTypeLegendary"));
  snprintf(t->gacha_move, sizeof t->gacha_move, "%s", i18n_raw("egg:gachaTypeMove"));
  snprintf(t->gacha_shiny, sizeof t->gacha_shiny, "%s", i18n_raw("egg:gachaTypeShiny"));
  snprintf(t->event_type, sizeof t->event_type, "%s", i18n_raw("egg:eventType"));
  snprintf(t->same_species, sizeof t->same_species, "%s", i18n_raw("egg:sameSpeciesEgg"));
}

/* the panel shows egg `i`: its picture, name, date, hatch message and type line, where upstream puts them (the strings are the model's, checked on their own) */
static void check_panel(int i, int tz_s, const char* what) {
  egm_texts t;
  texts(&t);
  egm_details d;
  egm_describe(&eggs[i], &t, sp_name, NULL, tz_s, &d);
  draw(1);
  const drawn_t* name = find_at(8, 68);
  const drawn_t* date = find_at(8, 91);
  const drawn_t* waves = find_at(8, 108);
  const drawn_t* type = find_at(8, 152);
  CHECK(name && !strcmp(name->text, d.name) && name->style == TS_SUMMARY, "%s: name %s", what, name ? name->text : "(none)");
  CHECK(date && !strcmp(date->text, d.date) && date->style == TS_EGG_LIST, "%s: date %s, expected %s", what, date ? date->text : "(none)", d.date);
  CHECK(waves && !strcmp(waves->text, d.waves) && waves->wrap_w == 90.0f, "%s: hatch message", what);
  CHECK(type && !strcmp(type->text, d.type) && type->wrap_w == 90.0f, "%s: type line %s, expected %s", what, type ? type->text : "(none)", d.type);
  char frame[16];
  snprintf(frame, sizeof frame, "egg_%s", d.key);
  const image_t* pic = image_at("images/egg/egg", 54, 37);
  CHECK(pic && !strcmp(pic->frame, frame), "%s: the egg picture %s, expected %s", what, pic ? pic->frame : "(none)", frame);
}

/* the icon in cell c is drawn where upstream puts it, half size, with the frame of its egg; `bob` shifts the one under the cursor up 2 px */
static void check_icons(int first_egg, int n_cells, int cursor, int bob, const char* what) {
  int found = 0;
  draw(1);
  for (int c = 0; c < EGL_ICONS; c++) {
    const float x = (float)(113 + (c % 11) * 18 - 2), y = (float)(5 + (c / 11) * 18 + 2 - (c == cursor && bob ? 2 : 0));
    const image_t* im = image_at("images/egg/egg_icons", x, y);
    if (c < n_cells) {
      char key[8];
      egm_key(&eggs[first_egg + c], key, sizeof key);
      CHECK(im && !strcmp(im->frame, key) && im->scale == 0.5f, "%s: cell %d shows egg %d as frame %s (%s)", what, c, first_egg + c, key, im ? im->frame : "none");
      if (im) found++;
    } else {
      CHECK(!im, "%s: cell %d is empty", what, c);
    }
  }
  CHECK(count_images("images/egg/egg_icons") == n_cells, "%s: %d icons drawn, expected %d", what, count_images("images/egg/egg_icons"), n_cells);
  CHECK(found == n_cells, "%s: every icon found", what);
  const image_t* cur = image_at("images/ui/select_cursor", (float)(113 + (cursor % 11) * 18 - 1), (float)(5 + (cursor / 11) * 18 + 2));
  CHECK(cur != NULL, "%s: the cursor on cell %d", what, cursor);
  CHECK(count_images("images/ui/select_cursor") == 1, "%s: one cursor", what);
}

int main(void) {
  /* ---- 5 eggs, one of each kind */
  make_eggs(5);
  eggs[2].species = 0; /* a legacy Common egg whose id is divisible by 204: the Manaphy egg */
  eggs[2].tier = 0;
  eggs[2].id = 204 * 9;
  gc_egg before[150];
  memcpy(before, eggs, sizeof eggs);
  egglist_args a = {eggs, 5, tz_fn, exited, &one};
  egglist_screen_open(&a);
  CHECK(g_top != NULL, "the screen is up");
  g_now = 0;
  check_panel(0, 0, "open");
  check_icons(0, 5, 0, 0, "open");
  CHECK(count_images("images/ui/egg_list_bg") == 1, "the background image");
  CHECK(n_nine == 0, "no scroll bar for five eggs");

  /* the eggs are copied: changing the caller's list changes nothing here */
  eggs[0].tier = 3;
  eggs[0].species = 150;
  draw(1);
  CHECK(find_at(8, 68) && !strcmp(find_at(8, 68)->text, "Egg (Common)"), "copied: the name is still the original's");
  CHECK(image_at("images/egg/egg", 54, 37) && !strcmp(image_at("images/egg/egg", 54, 37)->frame, "egg_0"), "copied: and so is the picture");
  memcpy(eggs, before, sizeof before);

  /* the time zone given is the one used */
  tz = -4 * 3600;
  check_panel(0, -4 * 3600, "zone");
  {
    egm_details d;
    egm_texts t;
    texts(&t);
    egm_describe(&eggs[0], &t, sp_name, NULL, 0, &d);
    char utc[32];
    snprintf(utc, sizeof utc, "%s", d.date);
    egm_describe(&eggs[0], &t, sp_name, NULL, -4 * 3600, &d);
    CHECK(strcmp(utc, d.date) != 0, "a zone four hours behind shows another day for 02:51 UTC (%s / %s)", utc, d.date);
    draw(1);
    CHECK(find_at(8, 91) && !strcmp(find_at(8, 91)->text, d.date), "the screen shows the local one");
  }
  tz = 0;

  /* D-pad: RIGHT moves to the next egg, the panel follows; LEFT back; LEFT at the first wraps to the last of the row (the last egg) */
  int sel = n_select;
  press(BTN_RIGHT);
  CHECK(n_select == sel + 1, "a move plays the sound");
  check_panel(1, 0, "right");
  check_icons(0, 5, 1, 0, "right");
  press(BTN_LEFT);
  check_panel(0, 0, "left");
  press(BTN_LEFT);
  check_panel(4, 0, "left wraps to the last egg");
  check_icons(0, 5, 4, 0, "left wraps");
  press(BTN_RIGHT);
  check_panel(0, 0, "right wraps to the first");
  press(BTN_DOWN);
  check_panel(0, 0, "down on the only row does not leave it");
  sel = n_select;
  press(BTN_UP);
  CHECK(n_select == sel, "up on a single row of five: the cursor stays on cell 0, nothing moved, no sound");
  press(BTN_SUBMIT);
  press(BTN_MENU);
  CHECK(n_select == sel && n_exit == 0, "other buttons do nothing");

  /* the icon under the cursor bobs between 0 and -2 every 200 ms */
  g_now = 200;
  check_icons(0, 5, 0, 1, "bob up");
  g_now = 450;
  check_icons(0, 5, 0, 0, "bob down");
  g_now = 0;

  /* a tap on an icon moves the cursor there; on an empty cell or outside the grid it does nothing */
  sel = n_select;
  CHECK(g_top->touch(g_top, 113 + 3 * 18 + 5, 5 + 4), "a tap on an icon");
  check_panel(3, 0, "tap on cell 3");
  CHECK(n_select == sel + 1, "and plays the sound");
  CHECK(!g_top->touch(g_top, 113 + 8 * 18 + 5, 5 + 4), "an empty cell");
  check_panel(3, 0, "empty cell");
  CHECK(!g_top->touch(g_top, 40, 60), "outside the grid");
  CHECK(g_top->touch(g_top, 113 + 3 * 18 + 5, 5 + 4) && n_select == sel + 1, "a tap on the cell the cursor is on is silent");

  /* the top screen only has the background */
  draw(0);
  CHECK(n_drawn == 0 && n_images == 0, "nothing is written on the top screen");
  CHECK(memcmp(eggs, before, sizeof eggs) == 0, "the screen changes nothing of the caller's eggs");
  press(BTN_CANCEL);
  CHECK(n_exit == 1, "B leaves");
  press(BTN_CANCEL);
  g_top->touch(g_top, 12, 224);
  CHECK(n_exit == 1, "and only once");

  /* ---- 12 eggs: a second row; DOWN moves a row, the panel follows */
  make_eggs(12);
  a.n = 12;
  egglist_screen_open(&a);
  check_icons(0, 12, 0, 0, "12 eggs");
  press(BTN_DOWN);
  check_panel(11, 0, "down from cell 0 to the last egg of the second row (clamped to the list)");
  check_icons(0, 12, 11, 0, "down");
  sel = n_select;
  press(BTN_RIGHT);
  CHECK(n_select == sel, "right at the end of the list stays: nothing moved, no sound");
  check_panel(11, 0, "right at the last egg");
  /* a tap far along a row: cell 10 */
  CHECK(g_top->touch(g_top, 113 + 10 * 18 + 2, 5 + 1), "a tap on the eleventh icon");
  check_panel(10, 0, "tap on cell 10");
  /* the hint leaves from a live screen */
  n_exit = 0;
  CHECK(g_top->touch(g_top, 12, 224), "a tap on the hint");
  CHECK(n_exit == 1, "the hint leaves");

  /* ---- 99 eggs fill the grid exactly: no scroll bar yet */
  make_eggs(99);
  a.n = 99;
  egglist_screen_open(&a);
  draw(1);
  CHECK(n_nine == 0, "no scroll bar for 99 eggs (nine rows)");

  /* ---- 100 eggs (the profile's capacity): ten rows, so the scroll bar shows and the grid scrolls a row at a time */
  make_eggs(100);
  a.n = 100;
  egglist_screen_open(&a);
  check_icons(0, 99, 0, 0, "100 eggs");
  draw(1);
  CHECK(n_nine >= 1, "the scroll bar is drawn over 99 eggs");
  for (int k = 0; k < 8; k++) press(BTN_DOWN);
  check_panel(88, 0, "eight rows down");
  check_icons(0, 99, 88, 0, "row 8");
  sel = n_select;
  press(BTN_DOWN);
  CHECK(n_select == sel + 1, "scrolling plays the sound");
  check_panel(99, 0, "scrolled one row: the cursor stays on its cell and the egg under it is the 100th");
  check_icons(11, 89, 88, 0, "scrolled");
  press(BTN_UP);
  check_icons(11, 89, 77, 0, "up moves the cursor a row first");
  for (int k = 0; k < 7; k++) press(BTN_UP);
  check_icons(11, 89, 0, 0, "up to the top row of the scrolled grid");
  press(BTN_UP);
  check_icons(0, 99, 0, 0, "and one more scrolls back");
  check_panel(0, 0, "back at the first egg");

  /* ---- more eggs than the profile holds: 100 are shown */
  make_eggs(150);
  a.n = 150;
  egglist_screen_open(&a);
  draw(1);
  CHECK(count_images("images/egg/egg_icons") == 99, "at most the profile's 100 eggs are listed (99 icons: nine rows of eleven)");
  press(BTN_UP);
  press(BTN_RIGHT);
  check_panel(99, 0, "the 100th egg is the last one: right from it does not reach a 101st");

  /* ---- no eggs: upstream does not open the list, it says so */
  a.n = 0;
  egglist_screen_open(&a);
  draw(1);
  CHECK(find_at(12, 78) && strstr(find_at(12, 78)->text, "any Eggs at the moment!") != NULL, "the empty list's message");
  CHECK(count_images("images/egg/egg_icons") == 0 && count_images("images/ui/select_cursor") == 0, "no icons, no cursor");
  sel = n_select;
  press(BTN_DOWN);
  press(BTN_RIGHT);
  CHECK(n_select == sel, "the D-pad does nothing");
  CHECK(!g_top->touch(g_top, 120, 20), "a tap on the grid does nothing");
  n_exit = 0;
  press(BTN_CANCEL);
  CHECK(n_exit == 1, "B leaves");

  printf("egglist_screen_test: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
