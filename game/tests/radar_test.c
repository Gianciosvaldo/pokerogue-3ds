/* radar_test: the Nemesis Radar (source/radar_screen.c) on the host, over custom-core's real validators and a fake drawing layer (no pixels:
 * the fakes record every text drawn on each screen with its position, every filled rectangle with its colour, and the sprite and icon calls
 * the screen makes; a character is 6 px wide in them, wider than the game's font, so a line that fits here fits there).
 *
 *   make radar_test && build/radar_test [fuzz sessions]
 *
 * Checks: the empty roster, an unusable file and a full roster of 16 (what each screen says; newest first; the tag of each life); long names
 * and unknown fields; the cursor, paging and scrolling; the Scout page for a record that knows everything and for one that knows nothing;
 * the one-line answer to "is a rematch scheduled?" for every life and every schedule state (including a plan and a record that
 * contradict each other); the faint type tint (only for a known type, never covering the text panels); that the sprite is read once the
 * cursor has rested, not on every move; that nothing on either screen offers a fight, an invasion, a purchase or a location; and, over
 * every button on every screen, that scouting changes nothing: the owner's metadata and schedule byte for byte, the ambient random
 * sequence, and the one on_exit call per opening. The screen holds a copy, so scribbling over the owner's data after the screen opened
 * changes nothing it shows. Exit 1 on a failed check. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builders.h"
#include "cc/model.h"
#include "gm_enum_names.h"
#include "i18n.h"
#include "radar_assets.h"
#include "radar_screen.h"
#include "run_text.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

static int checks, failed;
#define CHECK(cond, ...)                                  \
  do {                                                    \
    checks++;                                             \
    if (!(cond)) {                                        \
      failed++;                                           \
      if (failed < 40) {                                  \
        printf("FAIL line %d: %s: ", __LINE__, #cond);    \
        printf(__VA_ARGS__);                              \
        printf("\n");                                     \
      }                                                   \
    }                                                     \
  } while (0)

/* ------------------------------------------------------------------ fakes */

typedef struct {
  float x, y;
  float x0, ox;   /* the x as given and the origin: a wrapped text is centred line by line */
  int screen;     /* 0 top, 1 bottom */
  text_style style;
  char text[200];
  char flat[200]; /* the same with the line breaks as spaces, for a search that does not care where a text wrapped */
} drawn_t;
#define DRAWN_MAX 512
static drawn_t g_drawn[DRAWN_MAX];
static int n_drawn;
typedef struct {
  float x, y, w, h;
  rr_color c;
  int screen;
} rect_t;
#define RECT_MAX 256
static rect_t g_rects[RECT_MAX];
static int n_rects;
static int g_which;       /* the physical screen being drawn: 0 top, 1 bottom */

static ui_screen* g_top;
static int n_set, n_select, n_error, n_leave;
static float g_now;

void ui_set(ui_screen* s) {
  if (g_top && g_top->leave) {
    g_top->leave(g_top);
    n_leave++;
  }
  g_top = s;
  n_set++;
}
void ui_play_select(void) { n_select++; }
void ui_play_error(void) { n_error++; }
float ui_now(void) { return g_now; }
bool ui_space(ui_space_id sp) {
  const bool top = sp == UI_FIELD || sp == UI_FIELD_UI || sp == UI_TOP_CANVAS || sp == UI_TOP_CANVAS_UI || sp == UI_PHYS_TOP;
  return top == (g_which == 0);
}
void rr_rect(float x, float y, float w, float h, rr_color c) {
  if (n_rects < RECT_MAX) g_rects[n_rects++] = (rect_t){x, y, w, h, c, g_which};
}
void ui_window(float x, float y, float w, float h, win_variant v) { (void)x, (void)y, (void)w, (void)h, (void)v; }
void ui_cursor(float x, float y) { (void)x, (void)y; }
bool img_has(const char* key) { (void)key; return false; }
void img_draw(const char* key, const char* frame, float x, float y, float ox, float oy) { (void)key, (void)frame, (void)x, (void)y, (void)ox, (void)oy; }

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
float text_draw(text_style st, float x, float y, const char* s, const text_opts* o) {
  if (n_drawn < DRAWN_MAX) {
    drawn_t* d = &g_drawn[n_drawn++];
    d->style = st;
    d->x = x;
    d->y = y;
    d->screen = g_which;
    snprintf(d->text, sizeof d->text, "%s", s);
    for (size_t k = 0; k <= strlen(d->text); k++) d->flat[k] = d->text[k] == '\n' ? ' ' : d->text[k];
    d->x0 = x;
    d->ox = o ? o->origin_x : 0;
    if (o && o->origin_x > 0) d->x = x - o->origin_x * 6.0f * (float)strlen(s);
  }
  return 6.0f * (float)strlen(s);
}
/* Phaser's basicWordWrap at 6 px a character: break at spaces so a line stays within wrap_w */
void text_wrap(text_style st, const char* s, const text_opts* o, char* out, int n) {
  (void)st;
  const int per = o && o->wrap_w > 0 ? (int)(o->wrap_w / 6.0f) : 1000;
  int k = 0, col = 0;
  const char* p = s;
  while (*p && k < n - 2) {
    const char* e = p;
    while (*e && *e != ' ') e++;
    const int wl = (int)(e - p);
    if (col > 0 && col + 1 + wl > per) {
      out[k++] = '\n';
      col = 0;
    } else if (col > 0) {
      out[k++] = ' ';
      col++;
    }
    for (int i = 0; i < wl && k < n - 2; i++, col++) out[k++] = p[i];
    p = e;
    while (*p == ' ') p++;
  }
  out[k] = 0;
}

const char* i18n_t(char* out, int n, const char* key, ...) {
  if (!strcmp(key, "modifierType:ModifierType.LEFTOVERS.name")) {
    snprintf(out, (size_t)n, "Leftovers");
    return out;
  }
  if (!strncmp(key, "nature:", 7)) {
    snprintf(out, (size_t)n, "Nature-%s", key + 7);
    return out;
  }
  snprintf(out, (size_t)n, "[%s]", key);
  return out;
}
bool i18n_exists(const char* key) { return !strcmp(key, "modifierType:ModifierType.LEFTOVERS.name"); }

static char g_names[8][96];
static int g_name_i;
static const char* named(const char* prefix, int id) {
  char* b = g_names[g_name_i++ & 7];
  if (!strcmp(prefix, "Mon") && id == 150)
    snprintf(b, 96, "Mewtwo-with-a-very-very-very-long-name-that-cannot-fit-on-any-line-of-this-screen");
  else if (!strcmp(prefix, "Mon") && id == 151)
    snprintf(b, 96, "Mon-with-a-name-that-is-forty-chars-long");
  else
    snprintf(b, 96, "%s%d", prefix, id);
  return b;
}
const char* i18n_species_name(int id) { return named("Mon", id); }
const char* i18n_ability_name(int id) { return named("Ability", id); }
const char* i18n_type_name(int t) { return named("Type", t); }
const char* rt_biome_name(int biome, char* out, int n) {
  if (biome == 40)
    snprintf(out, (size_t)n, "%s", "");
  else
    snprintf(out, (size_t)n, "Biome %d", biome);
  return out;
}

/* radar_assets: the menus layer, faked */
static int n_icon, n_sprite_load, n_sprite_free, n_sprite_draw;
static int g_last_icon_species;
static float g_last_icon_scale;
static bool g_sprite_loaded;
static int g_sprite_species;
bool ra_species_types(int species, int form, int* t1, int* t2) {
  (void)form;
  switch (species) {
    case 25: *t1 = 12, *t2 = -1; return true;   /* electric */
    case 6: *t1 = 9, *t2 = 2; return true;      /* fire / flying */
    case 8000: *t1 = 18, *t2 = -1; return true; /* a type the tint table does not colour */
    default: return species != 8001 && species != 8002 ? (*t1 = 0, *t2 = -1, true) : false;
  }
}
void ra_icon(int species, int form, int gender, bool shiny, float x, float y, float scale, rr_color tint) {
  (void)form, (void)gender, (void)shiny, (void)x, (void)y, (void)tint;
  n_icon++;
  g_last_icon_species = species;
  g_last_icon_scale = scale;
}
bool ra_sprite_load(int species, int form, int gender, bool shiny) {
  (void)form, (void)gender, (void)shiny;
  if (g_sprite_loaded) n_sprite_free++;
  n_sprite_load++;
  g_sprite_loaded = species != 7;      /* no sprite in the packs for species 7 */
  g_sprite_species = species;
  return g_sprite_loaded;
}
void ra_sprite_free(void) {
  if (g_sprite_loaded) n_sprite_free++;
  g_sprite_loaded = false;
}
bool ra_sprite_draw(float x, float y, uint32_t ms, float max_h) {
  (void)x, (void)y, (void)ms, (void)max_h;
  if (!g_sprite_loaded) return false;
  n_sprite_draw++;
  return true;
}
const char* ra_move_name(int move) { return named("Move", move); }
const char* ra_nature_name(int nature, char* out, size_t n) {
  snprintf(out, n, "Nature%d", nature);
  return out;
}

/* ------------------------------------------------------------------ helpers */

static void draw(int which) {
  n_drawn = 0;
  n_rects = 0;
  g_which = which;
  g_top->draw(g_top, which ? RR_BOTTOM : RR_TOP);
}
static bool has_text(const char* needle) {
  for (int i = 0; i < n_drawn; i++)
    if (strstr(g_drawn[i].text, needle) || strstr(g_drawn[i].flat, needle)) return true;
  return false;
}
static const drawn_t* find_text(const char* needle) {
  for (int i = 0; i < n_drawn; i++)
    if (strstr(g_drawn[i].text, needle) || strstr(g_drawn[i].flat, needle)) return &g_drawn[i];
  return NULL;
}

static void press(ui_button b) { g_top->input(g_top, b); }

/* The Scout page shows 12 lines of a longer text: scroll down one line at a time and join every line, so a search does not depend on
 * where the page wrapped or how far it is scrolled. The scroll is put back at the top. */
static char g_page[4096];
static int detail_lines(const char** out, int cap) {
  int n = 0;
  for (int k = 0; k < n_drawn && n < cap; k++) {
    const drawn_t* d = &g_drawn[k];
    if (d->screen == 1 && d->x0 == 12 && d->style == TS_INSTRUCTIONS_TEXT && d->y >= 30 && d->y <= 200) out[n++] = d->text;
  }
  return n;
}
static void collect_page(void) {
  g_page[0] = 0;
  const char* prev_last = "";
  for (int i = 0; i < 60; i++) {
    draw(1);
    const char* ln[32];
    const int n = detail_lines(ln, 32);
    if (n == 0) break;
    if (i > 0 && !strcmp(ln[n - 1], prev_last)) break; /* the page did not move: its end */
    for (int k = i == 0 ? 0 : n - 1; k < n; k++) {
      strncat(g_page, ln[k], sizeof g_page - strlen(g_page) - 1);
      strncat(g_page, " ", sizeof g_page - strlen(g_page) - 1);
    }
    static char keep[200];
    snprintf(keep, sizeof keep, "%s", ln[n - 1]);
    prev_last = keep;
    press(BTN_DOWN);
  }
  for (int i = 0; i < 60; i++) press(BTN_UP);
  draw(1);
}
static bool has_page(const char* needle) { return strstr(g_page, needle) != NULL; }
static void tick(float ms) {
  g_now += ms;
  g_top->update(g_top, ms);
}

/* every line starts and ends on its screen (320 x 240 bottom, the field's 320 x 180 on top), in a readable style, and none is a word the
 * Radar must not say: it starts nothing, so no fight, invasion, purchase or place to go */
static void check_screen(int which) {
  const float w = 320, h = which ? 240 : 180;
  for (int i = 0; i < n_drawn; i++) {
    const drawn_t* d = &g_drawn[i];
    /* line by line: a wrapped text is several lines, each centred or left-aligned on its own, 12 px apart */
    int row = 0;
    for (const char* ln = d->text;; row++) {
      const char* e = strchr(ln, '\n');
      const float len = 6.0f * (float)(e ? (size_t)(e - ln) : strlen(ln));
      const float lx = d->x0 - d->ox * len, ly = d->y + 12.0f * (float)row;
      CHECK(lx >= 0 && ly >= 0 && ly + 12 <= h + 1, "\"%s\" is off the screen at (%.0f, %.0f)", d->text, lx, ly);
      CHECK(lx + len <= w + 1, "\"%s\" runs to x %.0f", d->text, lx + len);
      if (!e) break;
      ln = e + 1;
    }
    CHECK(d->style == TS_WINDOW || d->style == TS_SUMMARY || d->style == TS_INSTRUCTIONS_TEXT, "\"%s\" is in a style that was not readable on screen (%d)", d->text, (int)d->style);
    static const char* const banned[] = {"fight", "Fight", "invade", "invasion", "Invasion", "battle", "Battle", "buy", "Buy", "price", "Price", "voucher", "Voucher", "coin", "Coin", "currency", "attack", "Attack", "location", "Location", "found at", "appears"};
    for (size_t k = 0; k < sizeof banned / sizeof banned[0]; k++)
      CHECK(!strstr(d->text, banned[k]), "\"%s\" offers or implies \"%s\"", d->text, banned[k]);
  }
}
static void draw_both(void) {
  draw(0);
  check_screen(0);
  draw(1);
  check_screen(1);
}

/* the text drawn on the bottom screen, in order of y, as one string per line */
static int y_of(const char* needle) {
  const drawn_t* d = find_text(needle);
  return d ? (int)d->y : -1;
}

static int n_exit;
static void on_exit_cb(void* arg) {
  (void)arg;
  n_exit++;
}

static cc_meta g_meta;
static cc_schedule g_sched;

static void reset_fakes(void) {
  g_top = NULL;
  n_set = n_select = n_error = n_leave = 0;
  n_icon = n_sprite_load = n_sprite_free = n_sprite_draw = 0;
  g_sprite_loaded = false;
  n_exit = 0;
  g_now = 1000;
}

static radar_args args_for(const cc_meta* m, const cc_schedule* s) {
  radar_args a;
  memset(&a, 0, sizeof a);
  a.meta = m;
  a.schedule = s;
  a.on_exit = on_exit_cb;
  return a;
}

/* a record: species, wave, biome */
static cc_nemesis rec(cc_record_id id, int species, int wave, int biome) {
  cc_nemesis n = mk_nem(id, 0x0000000500000002ull, 100 + id);
  n.species = (uint16_t)species;
  n.source_wave = (uint16_t)wave;
  n.source_biome = (uint8_t)biome;
  n.fallen_species = 25;
  return n;
}
static void add(const cc_nemesis* n) { g_meta.roster.rec[g_meta.roster.count++] = *n; }
static void fresh_meta(void) {
  cc_meta_init(&g_meta, 5);
  for (int i = 0; i < 40; i++) {
    cc_record_id r;
    cc_alloc_record_id(&g_meta.counters, &r);
  }
  for (int i = 0; i < 8; i++) {
    cc_txn_id t;
    cc_alloc_txn_id(&g_meta.counters, &t);
  }
  for (int i = 0; i < 12; i++) {
    cc_run_uid u;
    cc_alloc_run_uid(&g_meta.counters, &u);
  }
}

/* ------------------------------------------------------------------ tests */

static void test_empty_and_unusable(void) {
  reset_fakes();
  fresh_meta();
  radar_args a = args_for(&g_meta, NULL);
  CHECK(radar_open(&a), "opens with an empty roster");
  draw_both();
  draw(0);
  CHECK(has_text("No enemy is remembered yet."), "top says nothing is remembered");
  CHECK(has_text("and only from defeats it can explain."), "top says where enemies come from");
  draw(1);
  CHECK(has_text("No enemy is remembered."), "bottom says so");
  CHECK(has_text("Rematches are offered during a Custom Run."), "bottom says where rematches are offered");
  CHECK(has_text("0/16"), "the count is 0 of 16");
  CHECK(!has_text("A: scout"), "no Scout button with nothing to scout");
  press(BTN_ACTION);
  CHECK(n_error == 1 && n_exit == 0, "A on an empty list does nothing but the error sound");
  draw(1);
  CHECK(has_text("No enemy is remembered."), "still the list");
  press(BTN_UP), press(BTN_DOWN), press(BTN_LEFT), press(BTN_RIGHT);
  press(BTN_CANCEL);
  CHECK(n_exit == 1, "B leaves once");
  press(BTN_CANCEL), press(BTN_ACTION);
  CHECK(n_exit == 1, "nothing after leaving calls on_exit again");

  /* an unusable file: the reason, no list, no Scout */
  reset_fakes();
  a = args_for(NULL, NULL);
  a.why = "The custom content file failed its checksum. It is left as it is.";
  CHECK(radar_open(&a), "opens without a usable file");
  draw_both();
  draw(0);
  CHECK(has_text("cannot be used"), "top: the file cannot be used");
  CHECK(has_text("failed its checksum"), "top: the reason");
  draw(1);
  CHECK(has_text("failed its checksum") && has_text("left as it is"), "bottom: the reason and that nothing changed");
  CHECK(!has_text("/16"), "no count for a file that cannot be read");
  press(BTN_ACTION);
  CHECK(n_error == 1, "no Scout without a roster");
  press(BTN_CANCEL);
  CHECK(n_exit == 1, "B leaves");

  /* a missing reason is still said */
  reset_fakes();
  a = args_for(NULL, NULL);
  CHECK(radar_open(&a), "opens");
  draw(1);
  CHECK(has_text("cannot be used"), "a default sentence when no reason was given");
}

static void test_open_refusals(void) {
  reset_fakes();
  CHECK(!radar_open(NULL), "NULL");
  CHECK(n_set == 0, "nothing opened");
  fresh_meta();
  add(&(cc_nemesis){0});                               /* not a valid record */
  radar_args a = args_for(&g_meta, NULL);
  CHECK(!radar_open(&a), "an invalid roster is refused");
  fresh_meta();
  cc_nemesis n = rec(3, 25, 27, 3);
  add(&n);
  memset(&g_sched, 0, sizeof g_sched);
  g_sched.state = 99;
  a = args_for(&g_meta, &g_sched);
  CHECK(!radar_open(&a), "an invalid schedule is refused");
  CHECK(n_set == 0, "and nothing was opened for either");
}

static void test_roster_of_16(void) {
  reset_fakes();
  fresh_meta();
  for (int i = 0; i < 16; i++) {
    cc_nemesis n = rec((cc_record_id)(3 + 2 * i), i % 3 == 0 ? 25 : (i % 3 == 1 ? 6 : 150), 27 + i, i % 4);
    n.orig_level = (uint8_t)(20 + i);
    if (i == 5) {
      n.life = CC_LIFE_SCHEDULED;
      n.sched_run_uid = 0x0000000500000003ull;
    }
    if (i == 6) {
      n.life = CC_LIFE_DEFEATED;
      n.sched_run_uid = 0x0000000500000003ull;
      n.challenge_wave = 77;
    }
    if (i == 7) {
      n.life = CC_LIFE_REWARD_CLAIMED;
      n.sched_run_uid = 0x0000000500000003ull;
      n.challenge_wave = 77;
      n.reward_txn = 4;
    }
    add(&n);
  }
  CHECK(cc_meta_validate(&g_meta) == CC_OK, "the fixture roster is valid (%d)", (int)cc_meta_validate(&g_meta));
  radar_args a = args_for(&g_meta, NULL);
  CHECK(radar_open(&a), "opens");
  draw_both();
  draw(1);
  CHECK(has_text("16/16"), "16 of 16");
  /* newest first: the last record (id 33, wave 42, species 25 -> i = 15, 15 % 3 = 0) is at the top of the list */
  CHECK(y_of("Mon25  Lv. 35") >= 0 && y_of("Wave 42") >= 0, "the newest record is the first row");
  CHECK(y_of("Wave 42") < y_of("Wave 41"), "newest above older");
  CHECK(!has_text("Wave 37"), "only five rows are visible");
  CHECK(has_text("11 more below"), "the rest is said to be below: %d", 0);
  CHECK(!has_text("more above"), "nothing above the first row");
  CHECK(has_text("Remembered"), "tag for a stored record");

  /* the long name is cut to fit its column and still says how it starts */
  int long_row = -1;
  for (int i = 0; i < n_drawn; i++)
    if (strstr(g_drawn[i].text, "Mewtwo-with")) long_row = i;
  CHECK(long_row < 0 || (strstr(g_drawn[long_row].text, "..") && strlen(g_drawn[long_row].text) * 6 <= 170), "a long name is cut with dots to its column");

  /* cursor: DOWN moves, selection stays on screen, scrolling follows */
  for (int i = 0; i < 4; i++) press(BTN_DOWN);
  draw(1);
  CHECK(!has_text("more above"), "five rows still fit with the cursor on the fifth");
  press(BTN_DOWN);
  draw(1);
  CHECK(has_text("1 more above") && has_text("10 more below"), "the sixth row scrolls the list by one");
  for (int i = 0; i < 40; i++) press(BTN_DOWN);
  draw(1);
  CHECK(has_text("11 more above") && !has_text("more below"), "the end of the list");
  CHECK(has_text("Wave 27"), "the oldest record is the last row");
  press(BTN_RIGHT);
  draw(1);
  CHECK(has_text("11 more above"), "RIGHT at the end stays");
  for (int i = 0; i < 6; i++) press(BTN_LEFT);
  draw(1);
  CHECK(!has_text("more above"), "LEFT pages back to the top");
  press(BTN_UP);
  draw(1);
  CHECK(!has_text("more above"), "UP at the top stays");
  draw_both();

  /* tags of each life */
  press(BTN_RIGHT);
  press(BTN_RIGHT);
  draw(1);
  CHECK(has_text("Held") || has_text("Beaten") || has_text("Claimed"), "a record in another life carries its tag");
  press(BTN_CANCEL);
  CHECK(n_exit == 1, "B leaves from the list");
}

static void test_tags_and_rematch_text(void) {
  cc_nemesis n = rec(3, 25, 27, 3);
  char t[160];
  radar_rematch_text(&n, NULL, t, sizeof t);
  CHECK(!strcmp(t, "No rematch is scheduled."), "stored, no schedule: \"%s\"", t);
  radar_rematch_text(NULL, NULL, t, sizeof t);
  CHECK(t[0] == 0, "no record, no text");
  radar_rematch_text(&n, NULL, t, 0);   /* a zero-length buffer is left alone */

  cc_nemesis held = n;
  held.life = CC_LIFE_SCHEDULED;
  held.sched_run_uid = 0x0000000500000003ull;
  radar_rematch_text(&held, NULL, t, sizeof t);
  CHECK(!strcmp(t, "Held by a run whose plan is not loaded here."), "held, no plan: \"%s\"", t);
  cc_nemesis chal = held;
  chal.life = CC_LIFE_CHALLENGED;
  chal.challenge_wave = 77;
  radar_rematch_text(&chal, NULL, t, sizeof t);
  CHECK(!strcmp(t, "Held by a run whose plan is not loaded here."), "challenged, no plan: \"%s\"", t);
  cc_nemesis won = chal;
  won.life = CC_LIFE_DEFEATED;
  radar_rematch_text(&won, NULL, t, sizeof t);
  CHECK(!strcmp(t, "Rematch won. The reward is not claimed yet."), "defeated: \"%s\"", t);
  won.life = CC_LIFE_REWARD_CLAIMED;
  won.reward_txn = 4;
  radar_rematch_text(&won, NULL, t, sizeof t);
  CHECK(!strcmp(t, "Rematch won. The reward was claimed."), "claimed: \"%s\"", t);

  /* with schedules */
  cc_schedule s;
  memset(&s, 0, sizeof s);
  s.state = CC_SCHED_ACTIVE;
  s.run_uid = 0x0000000500000003ull;
  s.rng_algo = CC_RNG_ALGO_V1;
  s.n_frozen = 1;
  s.frozen_ids[0] = 3;
  s.chosen_id = 3;
  s.n_windows = CC_NUM_WINDOWS;
  for (int i = 0; i < CC_NUM_WINDOWS; i++) s.windows[i].wave = cc_window_waves[i];
  CHECK(cc_schedule_validate(&s) == CC_OK, "fixture schedule is valid");
  radar_rematch_text(&held, &s, t, sizeof t);
  CHECK(!strcmp(t, "Planned for this run. The next window is wave 27."), "planned: \"%s\"", t);
  s.windows[0].state = CC_WIN_SKIPPED;
  s.windows[0].reason = CC_SKIP_BOSS;
  s.windows[1].state = CC_WIN_SKIPPED;
  s.windows[1].reason = CC_SKIP_TRAINER;
  CHECK(cc_schedule_validate(&s) == CC_OK, "two skipped windows are valid");
  radar_rematch_text(&held, &s, t, sizeof t);
  CHECK(!strcmp(t, "Planned for this run. The next window is wave 127."), "planned after two skips: \"%s\"", t);
  s.windows[2].state = CC_WIN_OFFERED;
  s.state = CC_SCHED_OFFERED;
  CHECK(cc_schedule_validate(&s) == CC_OK, "offered is valid");
  radar_rematch_text(&held, &s, t, sizeof t);
  CHECK(!strcmp(t, "Offered at wave 127."), "offered: \"%s\"", t);
  s.windows[2].state = CC_WIN_ACCEPTED;
  s.state = CC_SCHED_ACCEPTED;
  CHECK(cc_schedule_validate(&s) == CC_OK, "accepted is valid");
  radar_rematch_text(&chal, &s, t, sizeof t);
  CHECK(!strcmp(t, "Accepted at wave 127."), "accepted: \"%s\"", t);
  s.windows[2].state = CC_WIN_DECLINED;
  s.state = CC_SCHED_DECLINED;
  radar_rematch_text(&n, &s, t, sizeof t);
  CHECK(!strcmp(t, "Declined. Nothing more is offered this run."), "declined: \"%s\"", t);
  for (int i = 0; i < CC_NUM_WINDOWS; i++) {
    s.windows[i].state = CC_WIN_SKIPPED;
    s.windows[i].reason = CC_SKIP_BOSS;
  }
  s.state = CC_SCHED_EXHAUSTED;
  CHECK(cc_schedule_validate(&s) == CC_OK, "exhausted is valid");
  radar_rematch_text(&n, &s, t, sizeof t);
  CHECK(!strcmp(t, "Not offered this run: every window was skipped."), "exhausted: \"%s\"", t);

  /* a plan that names a record the record's life contradicts */
  s.state = CC_SCHED_ACTIVE;
  for (int i = 0; i < CC_NUM_WINDOWS; i++) {
    s.windows[i].state = CC_WIN_PENDING;
    s.windows[i].reason = 0;
  }
  radar_rematch_text(&n, &s, t, sizeof t);
  CHECK(strstr(t, "disagree") != NULL, "active plan, stored record: \"%s\"", t);
  /* a plan for another record says nothing about this one */
  s.chosen_id = 9;
  radar_rematch_text(&n, &s, t, sizeof t);
  CHECK(!strcmp(t, "No rematch is scheduled."), "another record's plan: \"%s\"", t);
  /* a reward state wins over a plan */
  s.chosen_id = 3;
  radar_rematch_text(&won, &s, t, sizeof t);
  CHECK(!strcmp(t, "Rematch won. The reward was claimed."), "claimed beats the plan: \"%s\"", t);
  /* the words fit a two-line strip of 296 px at 6 px a character */
  CHECK(strlen("The saved plan and this record disagree. No rematch is shown as scheduled.") * 6 < 2 * 296, "the longest sentence fits two lines");
}

static void test_scout_full_and_empty(void) {
  reset_fakes();
  fresh_meta();
  cc_nemesis full = rec(3, 25, 27, 3);     /* everything known (mk_nem): ability, nature, moves, ivs, one item, final move */
  full.passive = 0;
  full.shiny = 1;
  full.gender = 1;
  cc_nemesis bare = rec(5, 6, 40, 40);     /* biome name unknown to the fake */
  bare.known = CC_NK_FINISHER_VERIFIED | CC_NK_ABILITY | CC_NK_MOVES;   /* the minimum a stored record has */
  memset(bare.ivs, 0, sizeof bare.ivs);
  bare.nature = 0;
  bare.passive = 0;
  bare.n_items = 0;
  memset(bare.items, 0, sizeof bare.items);
  bare.finish_move = 0;
  bare.fallen_species = 0;
  bare.fallen_form = 0;
  add(&full);
  add(&bare);
  CHECK(cc_meta_validate(&g_meta) == CC_OK, "valid fixture (%d)", (int)cc_meta_validate(&g_meta));
  radar_args a = args_for(&g_meta, NULL);
  CHECK(radar_open(&a), "opens");
  /* the newest (bare) first */
  draw(0);
  CHECK(has_text("Mon6  Lv. 30"), "top: name and level");
  CHECK(has_text("Ended a run at wave 40"), "top: where it ended a run");
  CHECK(has_text("an unknown place"), "an unnamed biome is said to be unknown: not invented");
  press(BTN_ACTION);
  draw(1);
  check_screen(1);
  collect_page();
  CHECK(has_text("Scout: Mon6"), "the Scout page names the record");
  CHECK(has_page("Last of yours to fall: not recorded."), "unknown last-to-fall");
  CHECK(has_page("Final move: not recorded."), "unknown final move");
  CHECK(has_page("Nature: not recorded."), "unknown nature");
  CHECK(has_page("IVs: not recorded."), "unknown IVs");
  CHECK(has_page("Held items: not recorded."), "unknown loadout");
  CHECK(has_page("Ability: Ability50."), "the known ability");
  CHECK(has_page("Moves: Move33, Move98."), "the known moves");
  CHECK(!has_page("Passive"), "no passive line when it is not known");
  CHECK(has_page("No rematch is scheduled."), "the rematch line is on the page");
  press(BTN_CANCEL);
  draw(1);
  CHECK(has_text("A: scout") && has_text("Wave 40"), "B on the Scout page is the list again");
  CHECK(n_exit == 0, "and did not leave");

  press(BTN_DOWN);
  press(BTN_ACTION);
  draw(1);
  check_screen(1);
  collect_page();
  CHECK(has_page("Mon25, level 30, shiny, female"), "the Scout page names level, shiny and gender");
  CHECK(has_page("Ended a run at wave 27 in Biome 3."), "where it ended a run");
  CHECK(has_page("Last of yours to fall: Mon25."), "last to fall");
  CHECK(has_page("Final move: Move33."), "final move");
  CHECK(has_page("Passive: none.") || !has_page("Passive: Ability"), "passive is not invented");
  CHECK(has_page("Nature: Nature4."), "nature");
  CHECK(has_page("IVs: HP 10, Atk 11, Def 12, SpA 13, SpD 14, Spe 15."), "IVs");
  CHECK(has_page("Held items: Leftovers x2."), "held items use the game's name and the stack");
  press(BTN_CANCEL);
  press(BTN_CANCEL);
  CHECK(n_exit == 1, "B twice: out of the Scout page, then out of the Radar");
}

static void test_scout_scrolling_and_item_key_fallback(void) {
  reset_fakes();
  fresh_meta();
  cc_nemesis big = rec(3, 25, 27, 3);
  big.n_items = CC_MAX_LOADOUT_ITEMS;
  big.known |= CC_NK_LOADOUT | CC_NK_LOADOUT_TRUNCATED;
  const char* const keys[CC_MAX_LOADOUT_ITEMS] = {"LEFTOVERS", "SOUL_DEW", "FOCUS_BAND", "KINGS_ROCK", "QUICK_CLAW", "LUCKY_EGG", "SCOPE_LENS", "WIDE_LENS"};
  for (int i = 0; i < CC_MAX_LOADOUT_ITEMS; i++) {
    memset(&big.items[i], 0, sizeof big.items[i]);
    strcpy(big.items[i].key, keys[i]);
    big.items[i].arg = -1;
    big.items[i].stack = (uint16_t)(1 + i);
  }
  big.n_moves = CC_MAX_MOVES;
  for (int i = 0; i < CC_MAX_MOVES; i++) big.moves[i] = (uint16_t)(100 + i);
  add(&big);
  CHECK(cc_meta_validate(&g_meta) == CC_OK, "valid fixture (%d)", (int)cc_meta_validate(&g_meta));
  radar_args a = args_for(&g_meta, NULL);
  CHECK(radar_open(&a), "opens");
  press(BTN_ACTION);
  draw(1);
  check_screen(1);
  collect_page();
  CHECK(has_page("Soul Dew x2"), "an item the locale does not name is shown as a readable key: %d", 0);
  CHECK(has_page("and more."), "a truncated loadout says there is more");
  int more_below = has_text("more below");
  if (more_below) {
    press(BTN_DOWN);
    draw(1);
    CHECK(has_text("1 more above"), "the page scrolls");
    for (int i = 0; i < 20; i++) press(BTN_DOWN);
    draw(1);
    CHECK(!has_text("more below"), "to its end");
    for (int i = 0; i < 20; i++) press(BTN_UP);
    draw(1);
    CHECK(!has_text("more above"), "and back");
  }
  check_screen(1);
}

static void test_schedule_windows_on_page(void) {
  reset_fakes();
  fresh_meta();
  cc_nemesis n = rec(3, 25, 27, 3);
  n.life = CC_LIFE_SCHEDULED;
  n.sched_run_uid = 0x0000000500000003ull;
  add(&n);
  CHECK(cc_meta_validate(&g_meta) == CC_OK, "valid fixture (%d)", (int)cc_meta_validate(&g_meta));
  memset(&g_sched, 0, sizeof g_sched);
  g_sched.state = CC_SCHED_ACTIVE;
  g_sched.run_uid = 0x0000000500000003ull;
  g_sched.rng_algo = CC_RNG_ALGO_V1;
  g_sched.n_frozen = 1;
  g_sched.frozen_ids[0] = 3;
  g_sched.chosen_id = 3;
  g_sched.n_windows = CC_NUM_WINDOWS;
  for (int i = 0; i < CC_NUM_WINDOWS; i++) g_sched.windows[i].wave = cc_window_waves[i];
  g_sched.windows[0].state = CC_WIN_SKIPPED;
  g_sched.windows[0].reason = CC_SKIP_BOSS;
  radar_args a = args_for(&g_meta, &g_sched);
  CHECK(radar_open(&a), "opens with a schedule");
  draw_both();
  draw(1);
  CHECK(has_text("Planned for this run."), "the strip shows the real plan");
  CHECK(has_text("Held"), "and the record's tag");
  press(BTN_ACTION);
  draw(1);
  check_screen(1);
  collect_page();
  CHECK(has_page("This run's windows:"), "the Scout page lists the windows");
  CHECK(has_page("Wave 27: skipped: a boss"), "a skipped window and why");
  CHECK(has_page("Wave 77: waiting") && has_page("Wave 127: waiting") && has_page("Wave 177: waiting"), "the others are waiting");

  /* a schedule with no candidate: said on the list, no record is claimed to be planned */
  reset_fakes();
  fresh_meta();
  cc_nemesis stored = rec(3, 25, 27, 3);
  add(&stored);
  memset(&g_sched, 0, sizeof g_sched);
  g_sched.state = CC_SCHED_NO_CANDIDATE;
  g_sched.run_uid = 0x0000000500000003ull;
  a = args_for(&g_meta, &g_sched);
  CHECK(radar_open(&a), "opens with a no-candidate schedule");
  draw(1);
  CHECK(has_text("No rematch is scheduled."), "the record is not scheduled");
}

static void test_tint(void) {
  /* the tint is a faint rectangle over the whole top canvas, only for a known type, and the text panel is drawn after... never over the text */
  reset_fakes();
  fresh_meta();
  cc_nemesis known = rec(3, 25, 27, 3);
  cc_nemesis unknown = rec(5, 8001, 27, 3);   /* not in the tables */
  cc_nemesis nocolour = rec(7, 8000, 27, 3);  /* a type the table has no colour for */
  add(&known);
  add(&unknown);
  add(&nocolour);
  radar_args a = args_for(&g_meta, NULL);
  CHECK(radar_open(&a), "opens");
  int tints = 0;
  /* newest first: 8000 (no colour), 8001 (unknown), 25 (known) */
  draw(0);
  for (int i = 0; i < n_rects; i++) tints += g_rects[i].screen == 0 && g_rects[i].w == 320 && g_rects[i].h == 180 && (g_rects[i].c & 0xFF) == 70;
  CHECK(tints == 0, "a type without a colour is not tinted");
  CHECK(!has_text("Type"), "and no type is claimed for it: the line is shown only for a known type");
  press(BTN_DOWN);
  draw(0);
  tints = 0;
  for (int i = 0; i < n_rects; i++) tints += (g_rects[i].c & 0xFF) == 70;
  CHECK(tints == 0, "an unknown species is not tinted");
  CHECK(!has_text("Type"), "and no type is shown for it");
  press(BTN_DOWN);
  draw(0);
  tints = 0;
  rr_color c = 0;
  for (int i = 0; i < n_rects; i++)
    if ((g_rects[i].c & 0xFF) == 70) tints++, c = g_rects[i].c;
  CHECK(tints == 1, "a known type is tinted once, not stacked (%d)", tints);
  CHECK((c & 0xFF) <= 80, "the tint is modest (alpha %u)", (unsigned)(c & 0xFF));
  CHECK(has_text("Type12"), "the type is named when it is known");
  /* a dual type is named whole */
  fresh_meta();
  cc_nemesis dual = rec(3, 6, 27, 3);
  add(&dual);
  a = args_for(&g_meta, NULL);
  CHECK(radar_open(&a), "opens");
  draw(0);
  CHECK(has_text("Type9/Type2"), "both types of a dual-typed species");
}

static void test_sprite_timing(void) {
  reset_fakes();
  fresh_meta();
  for (int i = 0; i < 6; i++) {
    cc_nemesis n = rec((cc_record_id)(3 + 2 * i), i == 2 ? 7 : 25 + i, 27, 3);
    add(&n);
  }
  radar_args a = args_for(&g_meta, NULL);
  CHECK(radar_open(&a), "opens");
  draw(0);
  CHECK(n_sprite_load == 0 && n_icon > 0, "no sprite is read before the cursor rests; the icon stands in");
  tick(100);
  CHECK(n_sprite_load == 0, "100 ms is not enough");
  tick(60);
  CHECK(n_sprite_load == 1, "one load after the cursor rested");
  draw(0);
  CHECK(n_sprite_draw == 1, "the sprite is drawn");
  /* a held direction: a move every 17 ms never loads */
  const int before = n_sprite_load;
  press(BTN_DOWN);
  for (int i = 0; i < 4; i++) {
    tick(17);
    if (i < 3) press(BTN_DOWN);
  }
  CHECK(n_sprite_load == before, "moving fast loads nothing (%d loads)", n_sprite_load - before);
  draw(0);
  CHECK(g_last_icon_scale > 1.5f, "the icon stands in until the new sprite is read");
  tick(200);
  CHECK(n_sprite_load == before + 1, "the sprite of the record the cursor stopped on is read once");
  tick(500);
  CHECK(n_sprite_load == before + 1, "and not read again");
  /* a record the packs have no sprite for: tried once, the icon stays */
  press(BTN_UP);
  tick(200);
  press(BTN_UP);
  tick(200);
  press(BTN_DOWN);
  tick(200);
  const int l2 = n_sprite_load;
  tick(500);
  CHECK(n_sprite_load == l2, "a record without a sprite is not retried every frame");
  draw(0);
  draw(1);
  /* leaving frees the sprite: rest on a record that has one first */
  for (int i = 0; i < 6 && !g_sprite_loaded; i++) {
    press(BTN_DOWN);
    tick(200);
  }
  CHECK(g_sprite_loaded, "a sprite is held before the screen is left");
  const int freed = n_sprite_free;
  ui_screen dummy = {"title", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
  ui_set(&dummy);
  CHECK(n_sprite_free > freed && !g_sprite_loaded, "the sprite is freed when the screen is left");
}

static void test_copy_semantics_and_no_mutation(void) {
  reset_fakes();
  fresh_meta();
  for (int i = 0; i < 4; i++) {
    cc_nemesis n = rec((cc_record_id)(3 + 2 * i), 25, 27 + i, 3);
    add(&n);
  }
  cc_nemesis* h = &g_meta.roster.rec[3];
  h->life = CC_LIFE_SCHEDULED;
  h->sched_run_uid = 0x0000000500000003ull;
  memset(&g_sched, 0, sizeof g_sched);
  g_sched.state = CC_SCHED_ACTIVE;
  g_sched.run_uid = 0x0000000500000003ull;
  g_sched.rng_algo = CC_RNG_ALGO_V1;
  g_sched.n_frozen = 1;
  g_sched.frozen_ids[0] = h->record_id;
  g_sched.chosen_id = h->record_id;
  g_sched.n_windows = CC_NUM_WINDOWS;
  for (int i = 0; i < CC_NUM_WINDOWS; i++) g_sched.windows[i].wave = cc_window_waves[i];
  CHECK(cc_meta_validate(&g_meta) == CC_OK && cc_schedule_validate(&g_sched) == CC_OK, "valid fixture");
  static cc_meta before_m;
  static cc_schedule before_s;
  before_m = g_meta;
  before_s = g_sched;
  srand(31337);
  int ambient[4];
  for (int i = 0; i < 4; i++) ambient[i] = rand();
  srand(31337);

  radar_args a = args_for(&g_meta, &g_sched);
  CHECK(radar_open(&a), "opens");
  draw(1);
  static char shown_before[DRAWN_MAX][200];
  const int n_before = n_drawn;
  for (int i = 0; i < n_drawn; i++) snprintf(shown_before[i], 200, "%s", g_drawn[i].text);
  /* the owner's data goes away (overwritten) after the screen opened: it shows what it copied */
  memset(&g_meta, 0xEE, sizeof g_meta);
  memset(&g_sched, 0xEE, sizeof g_sched);
  draw(1);
  CHECK(n_drawn == n_before, "the same number of lines");
  for (int i = 0; i < n_drawn && i < n_before; i++) CHECK(!strcmp(shown_before[i], g_drawn[i].text), "\"%s\" changed to \"%s\" when the owner's data was overwritten", shown_before[i], g_drawn[i].text);
  draw(0);
  press(BTN_ACTION);
  draw(1);
  CHECK(has_text("Planned for this run."), "the Scout page still has the copied plan");
  g_meta = before_m;
  g_sched = before_s;

  /* every button on both pages, scouting every record: nothing the owner holds changes, nothing random is drawn */
  static const ui_button all[] = {BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_ACTION, BTN_CANCEL, BTN_SUBMIT, BTN_MENU, BTN_STATS, BTN_CYCLE_SHINY};
  radar_args a2 = args_for(&g_meta, &g_sched);
  for (int round = 0; round < 3; round++) {
    n_exit = 0;
    CHECK(radar_open(&a2), "opens again");
    for (int i = 0; i < 400; i++) {
      press(all[(i * 7 + round) % (int)(sizeof all / sizeof all[0])]);
      if (i % 3 == 0) tick(40);
      draw_both();
    }
    CHECK(n_exit <= 1, "on_exit at most once per opening (%d)", n_exit);
  }
  CHECK(memcmp(&before_m, &g_meta, sizeof g_meta) == 0, "scouting left the owner's metadata byte for byte as it was");
  CHECK(memcmp(&before_s, &g_sched, sizeof g_sched) == 0, "scouting left the owner's schedule byte for byte as it was");
  CHECK(g_sched.rng_state == 0 && g_sched.rng_seed == 0, "the policy stream state is untouched");
  for (int i = 0; i < 4; i++) CHECK(rand() == ambient[i], "the ambient random sequence moved (position %d)", i);
}

static void test_input_edges(void) {
  reset_fakes();
  fresh_meta();
  cc_nemesis n = rec(3, 25, 27, 3);
  add(&n);
  radar_args a = args_for(&g_meta, NULL);
  CHECK(radar_open(&a), "opens");
  CHECK(!g_top->input(g_top, BTN_MENU) && !g_top->input(g_top, BTN_STATS), "buttons the screen does not use are not taken");
  press(BTN_DOWN), press(BTN_DOWN), press(BTN_RIGHT);
  draw(1);
  CHECK(has_text("Wave 27"), "a single record stays selected");
  press(BTN_ACTION);
  CHECK(!g_top->input(g_top, BTN_ACTION) && !g_top->input(g_top, BTN_RIGHT), "A and RIGHT do nothing on the Scout page");
  press(BTN_CANCEL);
  press(BTN_CANCEL);
  CHECK(n_exit == 1, "left once");
  /* input after leaving is swallowed and can not call on_exit again, even from the Scout page's keys */
  press(BTN_ACTION);
  press(BTN_CANCEL);
  CHECK(n_exit == 1, "still once");
  /* opened again: a new life */
  CHECK(radar_open(&a), "opens again");
  press(BTN_CANCEL);
  CHECK(n_exit == 2, "a new opening can leave once more");
}

/* ------------------------------------------------------------------ fuzz */

static uint32_t rs = 0x2545F491u;
static uint32_t rnd(void) {
  rs ^= rs << 13;
  rs ^= rs >> 17;
  rs ^= rs << 5;
  return rs;
}

static void fuzz(int sessions) {
  for (int s = 0; s < sessions; s++) {
    reset_fakes();
    fresh_meta();
    const int count = (int)(rnd() % 17);
    for (int i = 0; i < count; i++) {
      cc_nemesis n = rec((cc_record_id)(3 + 2 * i), (int)(1 + rnd() % 8000), (int)(1 + rnd() % 500), (int)(rnd() % 50));
      n.orig_level = (uint8_t)(1 + rnd() % 255);
      n.gender = (int8_t)((int)(rnd() % 3) - 1);
      n.shiny = (uint8_t)(rnd() % 2);
      uint16_t known = CC_NK_FINISHER_VERIFIED | CC_NK_ABILITY | CC_NK_MOVES;
      if (rnd() % 2) known |= CC_NK_IVS | CC_NK_NATURE | CC_NK_PASSIVE | CC_NK_FINISH_MOVE | CC_NK_FALLEN | CC_NK_LOADOUT;
      n.known = known;
      if (!(known & CC_NK_IVS)) memset(n.ivs, 0, sizeof n.ivs), n.nature = 0, n.passive = 0, n.finish_move = 0, n.fallen_species = 0, n.fallen_form = 0, n.n_items = 0, memset(n.items, 0, sizeof n.items);
      if (rnd() % 5 == 0 && count == 0) continue;
      add(&n);
    }
    if (cc_meta_validate(&g_meta) != CC_OK) continue;
    radar_args a = args_for(rnd() % 8 ? &g_meta : NULL, NULL);
    a.why = "reason";
    if (!radar_open(&a)) continue;
    static cc_meta keep;
    keep = g_meta;
    for (int i = 0; i < 200; i++) {
      press((ui_button)(rnd() % BTN_COUNT_));
      if (rnd() % 4 == 0) tick((float)(rnd() % 90));
      if (i % 20 == 0) {
        draw(0);
        check_screen(0);
        draw(1);
        check_screen(1);
      }
    }
    CHECK(n_exit <= 1, "on_exit at most once (%d)", n_exit);
    CHECK(memcmp(&keep, &g_meta, sizeof g_meta) == 0, "the owner's metadata is untouched");
    ui_screen dummy = {"title", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    ui_set(&dummy);
    CHECK(!g_sprite_loaded, "no sprite is left loaded");
  }
}

/* the edges of the list: a page jump that overshoots the top, a move that goes nowhere, a name that is long but not the longest */
static void test_list_edges_and_mid_length_name(void) {
  reset_fakes();
  fresh_meta();
  for (int i = 0; i < 8; i++) {
    cc_nemesis n = rec((cc_record_id)(3 + 2 * i), i == 3 ? 151 : 25, 27 + i, 3);
    add(&n);
  }
  radar_args a = args_for(&g_meta, NULL);
  CHECK(radar_open(&a), "opens");
  for (int i = 0; i < 3; i++) press(BTN_DOWN);
  draw(0);
  CHECK(has_text("Ended a run at wave 31"), "three down: the fourth newest (wave 31)");
  press(BTN_LEFT); /* a page up from the fourth record overshoots the top: it goes to the first */
  draw(0);
  CHECK(has_text("Ended a run at wave 34"), "a page jump past the top stops at the newest record");
  int sounds = n_select;
  press(BTN_UP);
  CHECK(n_select == sounds, "UP on the first record moves nothing and makes no cursor sound");
  press(BTN_RIGHT);
  press(BTN_RIGHT);
  draw(0);
  CHECK(has_text("Ended a run at wave 27"), "two page jumps down stop at the oldest record");
  sounds = n_select;
  press(BTN_DOWN);
  CHECK(n_select == sounds, "DOWN on the last record moves nothing and makes no cursor sound");
  /* the 40-letter name is on the list (cut to its column) and on the top screen (whole) */
  for (int i = 0; i < 8; i++) press(BTN_UP);
  for (int i = 0; i < 4; i++) press(BTN_DOWN);
  draw(0);
  check_screen(0);
  CHECK(has_text("Mon-with-a-name-that-is-forty-chars-long"), "the whole name where the line is wide enough");
  draw(1);
  check_screen(1);
  for (int i = 0; i < n_drawn; i++)
    if (g_drawn[i].screen == 1 && g_drawn[i].x0 == 46 && strstr(g_drawn[i].text, "forty"))
      CHECK(0, "the row's name was not cut to its column: \"%s\"", g_drawn[i].text);
}

/* a record the saved plan does not name shows no windows, and a single item is shown without a count (an unknown ability or moves cannot
 * occur: cc_roster_validate, which radar_open requires, refuses a record whose ability or moves are not known) */
static void test_scout_honesty(void) {
  reset_fakes();
  fresh_meta();
  cc_nemesis held = rec(3, 25, 27, 3);
  held.life = CC_LIFE_SCHEDULED;
  held.sched_run_uid = 0x0000000500000003ull;
  add(&held);
  cc_nemesis u = rec(5, 25, 30, 3);
  u.items[0].stack = 1;
  add(&u);
  CHECK(cc_meta_validate(&g_meta) == CC_OK, "valid fixture (%d)", (int)cc_meta_validate(&g_meta));
  memset(&g_sched, 0, sizeof g_sched);
  g_sched.state = CC_SCHED_ACTIVE;
  g_sched.run_uid = 0x0000000500000003ull;
  g_sched.rng_algo = CC_RNG_ALGO_V1;
  g_sched.n_frozen = 1;
  g_sched.frozen_ids[0] = 3;
  g_sched.chosen_id = 3;
  g_sched.n_windows = CC_NUM_WINDOWS;
  for (int i = 0; i < CC_NUM_WINDOWS; i++) g_sched.windows[i].wave = cc_window_waves[i];
  radar_args a = args_for(&g_meta, &g_sched);
  CHECK(radar_open(&a), "opens");
  press(BTN_ACTION); /* the newest record (id 5) is on top: the plan names the other one */
  draw(1);
  check_screen(1);
  collect_page();
  CHECK(!has_page("This run's windows:") && !has_page("Wave 27: waiting"), "a record the plan does not name lists no windows");
  CHECK(has_page("No rematch is scheduled."), "and says no rematch is scheduled for it");
  CHECK(has_page("Held items: Leftovers.") && !has_page("x1"), "one item is shown without a count");
}

/* a Scout page longer than the screen: scrolls to its last line and not beyond */
static void test_scout_scroll_bounds(void) {
  reset_fakes();
  fresh_meta();
  cc_nemesis big = rec(3, 25, 27, 3);
  big.life = CC_LIFE_SCHEDULED;
  big.sched_run_uid = 0x0000000500000003ull;
  big.n_items = CC_MAX_LOADOUT_ITEMS;
  big.known |= CC_NK_LOADOUT_TRUNCATED;
  const char* const keys[CC_MAX_LOADOUT_ITEMS] = {"LEFTOVERS", "SOUL_DEW", "FOCUS_BAND", "KINGS_ROCK", "QUICK_CLAW", "LUCKY_EGG", "SCOPE_LENS", "WIDE_LENS"};
  for (int i = 0; i < CC_MAX_LOADOUT_ITEMS; i++) {
    memset(&big.items[i], 0, sizeof big.items[i]);
    strcpy(big.items[i].key, keys[i]);
    big.items[i].arg = -1;
    big.items[i].stack = (uint16_t)(1 + i);
  }
  add(&big);
  CHECK(cc_meta_validate(&g_meta) == CC_OK, "valid fixture (%d)", (int)cc_meta_validate(&g_meta));
  memset(&g_sched, 0, sizeof g_sched);
  g_sched.state = CC_SCHED_ACTIVE;
  g_sched.run_uid = 0x0000000500000003ull;
  g_sched.rng_algo = CC_RNG_ALGO_V1;
  g_sched.n_frozen = 1;
  g_sched.frozen_ids[0] = 3;
  g_sched.chosen_id = 3;
  g_sched.n_windows = CC_NUM_WINDOWS;
  for (int i = 0; i < CC_NUM_WINDOWS; i++) g_sched.windows[i].wave = cc_window_waves[i];
  radar_args a = args_for(&g_meta, &g_sched);
  CHECK(radar_open(&a), "opens");
  press(BTN_ACTION);
  draw(1);
  CHECK(has_text("more below") && !has_text("more above"), "the page is longer than the screen and starts at its top");
  const char* ln[32];
  CHECK(detail_lines(ln, 32) == 12, "twelve lines are shown at the top");
  for (int i = 0; i < 80; i++) press(BTN_DOWN);
  draw(1);
  CHECK(!has_text("more below") && has_text("more above"), "at the end there is nothing below");
  CHECK(detail_lines(ln, 32) == 12, "and the screen is still full: it did not scroll past the last line");
  CHECK(has_text("Wave 177: waiting"), "the last line of the page is on the screen");
  for (int i = 0; i < 80; i++) press(BTN_UP);
  draw(1);
  CHECK(!has_text("more above") && detail_lines(ln, 32) == 12, "and back at the top");
  check_screen(1);
}

/* the cursor has to rest: each move starts the wait again */
static void test_rest_restarts(void) {
  reset_fakes();
  fresh_meta();
  for (int i = 0; i < 4; i++) {
    cc_nemesis n = rec((cc_record_id)(3 + 2 * i), 25 + i, 27, 3);
    add(&n);
  }
  radar_args a = args_for(&g_meta, NULL);
  CHECK(radar_open(&a), "opens");
  tick(100);
  press(BTN_DOWN);
  tick(100);
  CHECK(n_sprite_load == 0, "100 ms before the move and 100 ms after it are not a rest of 140 ms");
  tick(60);
  CHECK(n_sprite_load == 1, "160 ms after the move: read once");
}

int main(int argc, char** argv) {
  test_empty_and_unusable();
  test_open_refusals();
  test_roster_of_16();
  test_tags_and_rematch_text();
  test_scout_full_and_empty();
  test_scout_scrolling_and_item_key_fallback();
  test_schedule_windows_on_page();
  test_tint();
  test_sprite_timing();
  test_copy_semantics_and_no_mutation();
  test_input_edges();
  test_list_edges_and_mid_length_name();
  test_scout_honesty();
  test_scout_scroll_bounds();
  test_rest_restarts();
  fuzz(argc > 1 ? atoi(argv[1]) : 400);
  printf("radar_test: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
