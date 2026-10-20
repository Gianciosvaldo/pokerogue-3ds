/* history_screen_test: the Run History (source/history_screen.c) on the host, over a fake UI stack and drawing layer (no pixels: the fakes record
 * the texts and icons drawn, the screens pushed and what the Summary screen was asked to show).
 *
 *   make history_screen_test && build/history_screen_test
 *
 * Checks, against history_screen.h and upstream's run-history-ui-handler.ts / run-info-ui-handler.ts: an empty history says "Empty" and
 * A opens nothing, an unreadable one says so; the list draws each run's result (Victory!, Defeated by + the wild Pokemon, a trainer's
 * name resolved from game-core's nesting text, Defeated by Rival + the rival's name), mode and wave, time and party with levels; UP and
 * DOWN wrap and keep the cursor in view; A opens the run's info page (header with the run's name, result, mode, money, luck, the party
 * rows and, below, the highlighted Pokemon's stats and moves), UP/DOWN move on its party, A opens the Summary screen with the run's party at
 * the highlighted Pokemon, B closes the Summary, B on the info goes back to the list and B on the list leaves. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gm_enum_names.h"
#include "history_screen.h"
#include "i18n.h"
#include "menus.h"
#include "party_icon.h"
#include "pr_data.h"
#include "run_history.h"
#include "run_summary.h"
#include "run_text.h"
#include "screen_clip.h"
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

static ui_screen* g_screen;
static ui_screen* g_stack[4];
static int g_depth;
static int n_set, n_select, n_error;
void ui_set(ui_screen* s) {
  g_screen = s;
  g_stack[0] = s;
  g_depth = 1;
  n_set++;
}
void ui_push(ui_screen* s) { g_stack[g_depth++] = s; }
void ui_pop(void) {
  if (g_depth > 1) g_depth--;
}
ui_screen* ui_top(void) { return g_stack[g_depth - 1]; }
void ui_play_select(void) { n_select++; }
void ui_play_error(void) { n_error++; }

static int g_which; /* 0 top, 1 bottom */
bool ui_space(ui_space_id sp) {
  const bool top = sp == UI_FIELD || sp == UI_FIELD_UI || sp == UI_TOP_CANVAS || sp == UI_TOP_CANVAS_UI || sp == UI_PHYS_TOP;
  return top == (g_which == 0);
}
void ui_window(float x, float y, float w, float h, win_variant v) { (void)x, (void)y, (void)w, (void)h, (void)v; }
void rr_rect(float x, float y, float w, float h, rr_color c) { (void)x, (void)y, (void)w, (void)h, (void)c; }
void clip_to_screen(void) {}
void clip_off(void) {}
bool img_has(const char* key) {
  (void)key;
  return false;
}
static int n_type_icons;
void img_draw(const char* key, const char* frame, float x, float y, float ox, float oy) {
  (void)frame, (void)x, (void)y, (void)ox, (void)oy;
  if (!strcmp(key, "images/types")) n_type_icons++;
}
const char* gm_enum_lower(gm_enum e, int value, char* out, int n) {
  (void)e, (void)value;
  snprintf(out, (size_t)n, "town");
  return out;
}
static int n_icons;
static rsv_mon g_icons[32];
void pi_draw(const rsv_mon* m, float x, float y, float scale) {
  (void)x, (void)y, (void)scale;
  if (n_icons < 32) g_icons[n_icons] = *m;
  n_icons++;
}
static int n_summaries;
static rsv_info g_summary_info;
void rsum_draw(const rsv_info* in, float x, float y) {
  (void)x, (void)y;
  g_summary_info = *in;
  n_summaries++;
}
const char* rt_mode_name(int mode, char* out, int n) {
  snprintf(out, (size_t)n, mode == 0 ? "Classic" : "Endless");
  return out;
}
const char* rt_biome_name(int biome, char* out, int n) {
  (void)biome;
  snprintf(out, (size_t)n, "Town");
  return out;
}
const char* rt_number(long v, char* out, int n) {
  snprintf(out, (size_t)n, "%ld", v);
  return out;
}
const char* rt_money(double v, char* out, int n) {
  snprintf(out, (size_t)n, "P%.0f", v);
  return out;
}

typedef struct {
  float x, y;
  char text[200];
} drawn_t;
static drawn_t g_drawn[512];
static int n_drawn;
text_opts text_defaults(void) {
  text_opts o;
  memset(&o, 0, sizeof o);
  o.chars = TEXT_ALL;
  return o;
}
float text_draw(text_style st, float x, float y, const char* s, const text_opts* o) {
  (void)st, (void)o;
  if (n_drawn < 512) {
    drawn_t* d = &g_drawn[n_drawn++];
    d->x = x;
    d->y = y;
    snprintf(d->text, sizeof d->text, "%s", s);
  }
  return 6.0f * (float)strlen(s);
}

float text_width(text_style st, const char* s, const text_opts* o) {
  (void)st, (void)o;
  return 6.0f * (float)strlen(s);
}

/* the hints (ui_widgets.c, tested by msgbox_test): the same two functions */
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

const char* i18n_t(char* out, int n, const char* key, ...) {
  const char* t = key;
  const char* who = "";
  va_list ap;
  va_start(ap, key);
  const char* name;
  while ((name = va_arg(ap, const char*)) != NULL) {
    const char* v = va_arg(ap, const char*);
    if (!strcmp(name, "trainerName")) who = v;
  }
  va_end(ap);
  char tmp[256];
  if (!strcmp(key, "saveSlotSelectUiHandler:empty")) t = "Empty";
  if (!strcmp(key, "saveSlotSelectUiHandler:wave")) t = "Wave";
  if (!strcmp(key, "saveSlotSelectUiHandler:lv")) t = "Lv";
  if (!strcmp(key, "runHistory:victory")) t = "Victory!";
  if (!strcmp(key, "runHistory:defeatedWild") || !strcmp(key, "runHistory:defeatedTrainer")) t = "Defeated by ";
  if (!strcmp(key, "runHistory:defeatedRival")) t = "Defeated by Rival";
  if (!strcmp(key, "runHistory:defeated")) t = "Defeated";
  if (!strcmp(key, "runHistory:runInfo")) t = "Run Info";
  if (!strcmp(key, "runHistory:mode")) t = "Mode";
  if (!strcmp(key, "runHistory:luck")) t = "Luck";
  if (!strcmp(key, "trainerNames:rival")) t = "Finn";
  if (!strcmp(key, "trainerNames:rivalFemale")) t = "Ivy";
  if (!strcmp(key, "starterSelectUiHandler:ability")) t = "Ability:";
  if (!strcmp(key, "starterSelectUiHandler:passive")) t = "Passive:";
  if (!strcmp(key, "battle:trainerAppeared") || !strcmp(key, "battle:trainerAppearedDouble")) {
    snprintf(tmp, sizeof tmp, "%s%s would like to battle!", !strcmp(key, "battle:trainerAppearedDouble") ? "[double] " : "", who);
    t = tmp;
  }
  if (!strncmp(key, "pokemonInfo:stat.", 17)) t = key + 17;
  if (!strncmp(key, "nature:", 7)) t = key + 7;
  snprintf(out, (size_t)n, "%s", t);
  return out;
}
const char* i18n_resolve(char* out, int n, const char* text) {
  /* "$t(trainerClasses:aceTrainer) $t(trainersCommon:aceTrainer.male.3)" -> "Ace Trainer Sam" */
  if (strstr(text, "aceTrainer"))
    snprintf(out, (size_t)n, "Ace Trainer Sam");
  else
    snprintf(out, (size_t)n, "%s", text);
  return out;
}
const char* i18n_species_name(int id) {
  static char b[8][32];
  static int i;
  i = (i + 1) % 8;
  snprintf(b[i], sizeof b[i], "Mon%d", id);
  return b[i];
}
const char* i18n_ability_name(int id) {
  static char b[8][32];
  static int i;
  i = (i + 1) % 8;
  snprintf(b[i], sizeof b[i], "Ability%d", id);
  return b[i];
}
const char* i18n_move_name(int id) {
  static char b[8][32];
  static int i;
  i = (i + 1) % 8;
  snprintf(b[i], sizeof b[i], "Move%d", id);
  return b[i];
}

/* the Summary screen: what it was asked to show, and the buttons it was given */
static int n_summary_open, n_summary_input, n_summary_update, n_summary_draw, n_mui_tick;
static bool g_summary_active;
static summary_args g_sum_args;
static menu_mon g_sum_party[RH_PARTY];
static summary_events g_sum_ev;
void summary_open(const summary_args* args, const summary_events* ev) {
  n_summary_open++;
  g_sum_args = *args;
  memcpy(g_sum_party, args->party, sizeof(menu_mon) * (size_t)args->n);
  g_sum_ev = *ev;
  g_summary_active = true;
}
void summary_close(void) { g_summary_active = false; }
bool summary_active(void) { return g_summary_active; }
bool summary_input(mui_button b) {
  n_summary_input++;
  (void)b;
  return true;
}
static int n_summary_touch;
static float g_sum_touch_x, g_sum_touch_y;
bool summary_touch(float x, float y) {
  n_summary_touch++;
  g_sum_touch_x = x;
  g_sum_touch_y = y;
  return true;
}
void summary_update(float dt) { (void)dt, n_summary_update++; }
void summary_draw(void) { n_summary_draw++; }
void mui_tick(float dt) { (void)dt, n_mui_tick++; }
void mui_set_origin(rr_screen s, int px, int py) { (void)s, (void)px, (void)py; }

/* ------------------------------------------------------------------ the history the screen reads */

static rh_entry E[8];
static int n_entries;
int rhist_count(void) { return n_entries; }
const rh_entry* rhist_get(int i) { return i >= 0 && i < n_entries ? &E[i] : NULL; }
void rhist_as_info(const rh_entry* e, rsv_info* out) {
  memset(out, 0, sizeof *out);
  out->status = RSV_OK;
  out->wave = e->wave;
  out->n_party = e->n_party;
  snprintf(out->name, sizeof out->name, "%s", e->name);
}

static rh_mon mon(int species, int level, int nature) {
  rh_mon m;
  memset(&m, 0, sizeof m);
  m.species = (uint16_t)species;
  m.level = (uint32_t)level;
  m.hp = (uint32_t)(50 + species);
  m.nature = (uint8_t)nature;
  m.ability = (uint16_t)(30 + species);
  m.has_passive = species % 2 == 0;
  m.passive = (uint16_t)(60 + species);
  m.n_moves = 3;
  for (int i = 0; i < 3; i++) m.moves[i] = (uint16_t)(100 + species + i);
  m.types[0] = 9;
  m.types[1] = species % 2 ? 3 : -1;
  for (int i = 0; i < 6; i++) m.stats[i] = (uint32_t)(100 + species + i);
  m.luck = (uint8_t)(species % 3);
  return m;
}

static void make_history(void) {
  memset(E, 0, sizeof E);
  n_entries = 7;
  for (int i = 0; i < 7; i++) {
    rh_entry* e = &E[i];
    e->time = 1760000000 + i * 86400;
    e->mode = i % 2;
    e->wave = 10 + i * 20;
    e->score = 1000 * i;
    e->money = 500 * i;
    e->n_party = (uint8_t)(1 + i % 6);
    for (int k = 0; k < e->n_party; k++) e->party[k] = mon(10 + i + k, 20 + k, (k * 4) % 25);
  }
  E[0].victory = true;
  snprintf(E[0].name, sizeof E[0].name, "My first win");
  E[1].defeat = RH_DEFEAT_WILD;
  E[1].n_enemy = 2;
  E[1].enemy[0] = mon(200, 44, 0);
  E[1].enemy[1] = mon(201, 45, 0);
  E[2].defeat = RH_DEFEAT_TRAINER;
  snprintf(E[2].trainer, sizeof E[2].trainer, "$t(trainerClasses:aceTrainer) $t(trainersCommon:aceTrainer.male.3)");
  E[3].defeat = RH_DEFEAT_RIVAL;
  E[4].defeat = RH_DEFEAT_RIVAL;
  E[4].trainer_female = true;
  E[5].defeat = RH_DEFEAT_TRAINER;
  E[5].trainer_double = true;
  snprintf(E[5].trainer, sizeof E[5].trainer, "$t(trainerClasses:aceTrainer) pair");
  /* E[6]: a defeat that names nothing */
}

static int n_exit;
static void on_exit_cb(void* arg) { (void)arg, n_exit++; }

static void open_screen(rh_status st) {
  n_exit = 0;
  const hist_args a = {st, on_exit_cb, NULL};
  history_screen_open(&a);
}
static bool press(ui_button b) { return ui_top()->input(ui_top(), b); }
static void draw(int which) {
  n_drawn = n_icons = n_summaries = n_type_icons = 0;
  g_which = which == RR_BOTTOM ? 1 : 0;
  for (int d = 0; d < g_depth; d++) g_stack[d]->draw(g_stack[d], which == RR_BOTTOM ? RR_BOTTOM : RR_TOP);
  g_which = 0;
}
static void settle(void) {
  for (int i = 0; i < 200; i++) g_stack[0]->update(g_stack[0], 33.0f);
}
static const drawn_t* find(const char* s) {
  for (int i = 0; i < n_drawn; i++)
    if (!strcmp(g_drawn[i].text, s)) return &g_drawn[i];
  return NULL;
}
static const drawn_t* find_prefix(const char* s) {
  for (int i = 0; i < n_drawn; i++)
    if (!strncmp(g_drawn[i].text, s, strlen(s))) return &g_drawn[i];
  return NULL;
}
static int count_prefix(const char* s) {
  int n = 0;
  for (int i = 0; i < n_drawn; i++)
    if (!strncmp(g_drawn[i].text, s, strlen(s))) n++;
  return n;
}

/* ------------------------------------------------------------------ the tests */

int main(void) {
  /* ---- an empty history */
  n_entries = 0;
  open_screen(RH_OK);
  CHECK(g_screen && n_set == 1 && !strcmp(g_screen->name, "run-history"), "the history is the screen");
  draw(RR_BOTTOM);
  CHECK(find("Empty"), "an empty history says Empty (bottom)");
  draw(RR_TOP);
  CHECK(find("Empty"), "... and on the top screen");
  press(BTN_ACTION);
  CHECK(ui_top() == g_screen && g_depth == 1, "A opens nothing");
  press(BTN_UP);
  press(BTN_DOWN);
  draw(RR_BOTTOM);
  CHECK(find("Empty"), "moving on an empty history draws no run");
  CHECK(press(BTN_CANCEL) && n_exit == 1, "B leaves");
  open_screen(RH_UNREADABLE);
  draw(RR_BOTTOM);
  CHECK(find("The run history can't be read.") && !find("Empty"), "an unreadable history says so");

  /* ---- the list */
  make_history();
  open_screen(RH_OK);
  settle();
  draw(RR_BOTTOM);
  CHECK(find("Victory!"), "a win says Victory!");
  CHECK(find("Defeated by ") && count_prefix("Defeated by ") >= 2, "a wild defeat says Defeated by (%d)", count_prefix("Defeated by "));
  CHECK(find("Classic - Wave 10") && find("Endless - Wave 30"), "mode and wave");
  {
    char want[32];
    const time_t t = 1760000000;
    struct tm tmv;
    gmtime_r(&t, &tmv);
    strftime(want, sizeof want, "%Y-%m-%d %H:%M", &tmv);
    CHECK(find(want), "the time reads %s", want);
    CHECK(!strcmp(want, "2025-10-09 08:53"), "the format is year-month-day hour:minute (%s)", want);
  }
  CHECK(find("Lv20"), "a party member's level");
  /* only the rows that fit are drawn; the 4 in view are entries 0..3 (their party icons: 1+2+3+4) */
  CHECK(n_icons >= 1 + 2 + 3 + 4, "party icons of the rows in view: %d", n_icons);
  /* the wild defeat's two Pokemon are icons too; a row has no room for their levels (the info page and the top screen show them) */
  CHECK(!find("Lv44") && !find("Lv45"), "a row shows the wild Pokemon that won as icons only");
  bool enemy_icon = false;
  for (int i = 0; i < n_icons && i < 32; i++)
    if (g_icons[i].species == 200) enemy_icon = true;
  CHECK(enemy_icon, "and their icons");
  CHECK(find("Defeated by Ace Trainer Sam") && find("Defeated by Rival Finn"), "a trainer's name is resolved, the rival is the rival");
  draw(RR_TOP);
  CHECK(n_summaries == 1 && g_summary_info.wave == 10, "the top screen shows the highlighted run in full");
  CHECK(find("Victory!"), "... with its result");

  /* UP / DOWN wrap and the cursor stays in view */
  press(BTN_UP);
  settle();
  draw(RR_BOTTOM);
  CHECK(find("Endless - Wave 70") || find("Classic - Wave 70"), "UP from the first run wraps to the last (wave 70 is drawn)");
  float y_last = -1;
  for (int i = 0; i < n_drawn; i++)
    if (!strcmp(g_drawn[i].text, "Classic - Wave 130") || !strcmp(g_drawn[i].text, "Endless - Wave 130")) y_last = g_drawn[i].y;
  draw(RR_TOP);
  CHECK(g_summary_info.wave == 130, "the top screen follows the cursor (wave %d)", g_summary_info.wave);
  press(BTN_DOWN);
  settle();
  draw(RR_TOP);
  CHECK(g_summary_info.wave == 10, "DOWN from the last wraps to the first");
  for (int i = 0; i < 6; i++) press(BTN_DOWN);
  settle();
  draw(RR_BOTTOM);
  {
    const drawn_t* d = find("Endless - Wave 130") ? find("Endless - Wave 130") : find("Classic - Wave 130");
    CHECK(d && d->y >= 8 && d->y + 33 <= 240, "the highlighted last run is on the screen (y %.1f)", d ? d->y : -1.0f);
    (void)y_last;
  }
  press(BTN_UP);
  press(BTN_UP);
  press(BTN_UP);
  press(BTN_UP);
  press(BTN_UP);
  press(BTN_UP);
  settle();
  draw(RR_BOTTOM);
  {
    const drawn_t* d = find("Classic - Wave 10");
    CHECK(d && d->y >= 8 - 1 && d->y < 60, "back at the first run it is the top row (y %.1f)", d ? d->y : -1.0f);
  }

  /* ---- the info page */
  const int sel = n_select;
  press(BTN_DOWN); /* the second run: a defeat by two wild Pokemon */
  CHECK(press(BTN_ACTION) && ui_top() == g_screen, "A opens the run's info");
  CHECK(n_select > sel, "with the select sound");
  draw(RR_TOP);
  CHECK(find("Run Info") && !find("My first win"), "the header (this run has no name)");
  CHECK(find("Defeated") && find("Wave 30"), "the result window");
  CHECK(find("Mode: Endless") && find("P500"), "mode and money");
  CHECK(find_prefix("Luck: "), "luck");
  CHECK(find("Mon11  Lv20") && find("Mon12  Lv21"), "name and level of the first two (%s)", find_prefix("Mon") ? find_prefix("Mon")->text : "-");
  CHECK(find_prefix("hardy - Ability41") != NULL || find_prefix("hardy - ") != NULL, "nature and ability");
  CHECK(find("Lv44") && find("Lv45"), "the wild Pokemon again");
  draw(RR_BOTTOM);
  CHECK(find_prefix("Mon11  Lv20"), "the bottom screen names the highlighted Pokemon");
  CHECK(find("hpShortened") && find("atkShortened") && find("spdShortened"), "its stats");
  CHECK(find("111") && find("112") && find("116"), "and their values");
  CHECK(find("Move111") && find("Move112") && find("Move113"), "its moves");
  CHECK(find("-"), "the empty move slot shows a dash");
  CHECK(n_type_icons == 2, "its two types (%d)", n_type_icons);
  CHECK(find_prefix("Ability: Ability41") != NULL, "its ability (%s)", find_prefix("Ability") ? find_prefix("Ability")->text : "-");

  /* the info page of a win has its name in the header */
  press(BTN_CANCEL);
  CHECK(ui_top() == g_screen && n_exit == 0, "B on the info goes back to the list");
  press(BTN_UP);
  press(BTN_ACTION);
  draw(RR_TOP);
  CHECK(find("Victory!") && find("My first win"), "the win's info page: result and the run's name");

  /* UP / DOWN move on the party and wrap */
  press(BTN_ACTION); /* the Summary screen for the first Pokemon of E[0] */
  CHECK(n_summary_open == 1 && g_depth == 2 && !strcmp(ui_top()->name, "history-summary"), "A opens the Summary screen over the info page");
  CHECK(g_sum_args.index == 0 && g_sum_args.n == E[0].n_party && g_sum_args.from_party && g_sum_args.mode == SUMMARY_DEFAULT, "for the highlighted Pokemon of the run");
  CHECK(g_sum_party[0].species == E[0].party[0].species && g_sum_party[0].level == E[0].party[0].level && g_sum_party[0].ability == E[0].party[0].ability &&
            g_sum_party[0].nature == E[0].party[0].nature && g_sum_party[0].moves[1].id == E[0].party[0].moves[1] && g_sum_party[0].nmoves == 3 &&
            g_sum_party[0].stats[3] == E[0].party[0].stats[3] && g_sum_party[0].max_hp == E[0].party[0].stats[0] && g_sum_party[0].hp == E[0].party[0].hp &&
            g_sum_party[0].types[0] == 9,
        "its fields reach the Summary screen");
  CHECK(!strcmp(g_sum_party[0].name, i18n_species_name(E[0].party[0].species)), "with its name");
  press(BTN_DOWN);
  CHECK(n_summary_input == 1, "buttons go to the Summary screen while it is up");
  ui_top()->update(ui_top(), 16.0f);
  CHECK(n_summary_update == 1 && n_mui_tick == 1, "and so do the frames");
  draw(RR_BOTTOM);
  CHECK(n_summary_draw == 1, "and its drawing");
  g_sum_ev.on_close(g_sum_ev.ctx);
  CHECK(g_depth == 1 && ui_top() == g_screen, "closing the Summary screen comes back to the info page");

  /* a run with 6 Pokemon: wrap on the party */
  press(BTN_CANCEL);
  press(BTN_DOWN);
  press(BTN_DOWN);
  press(BTN_DOWN);
  press(BTN_DOWN);
  press(BTN_DOWN); /* E[5]: six Pokemon, a defeat by a pair */
  press(BTN_ACTION);
  draw(RR_TOP);
  CHECK(find_prefix("[double] Ace Trainer Sam"), "a pair of trainers: the double's line (%s)", find_prefix("[double]") ? find_prefix("[double]")->text : "-");
  for (int i = 0; i < 5; i++) press(BTN_DOWN);
  press(BTN_ACTION);
  CHECK(g_sum_args.index == 5 && g_sum_args.n == 6, "five DOWNs reach the sixth Pokemon");
  g_sum_ev.on_close(g_sum_ev.ctx);
  press(BTN_DOWN);
  press(BTN_ACTION);
  CHECK(g_sum_args.index == 0, "DOWN from the last Pokemon wraps to the first");
  g_sum_ev.on_close(g_sum_ev.ctx);
  press(BTN_UP);
  press(BTN_ACTION);
  CHECK(g_sum_args.index == 5, "UP from the first wraps to the last");
  g_sum_ev.on_close(g_sum_ev.ctx);

  /* the rival, the female rival, and the run that names nothing */
  press(BTN_CANCEL);
  press(BTN_UP);
  press(BTN_UP);
  press(BTN_ACTION);
  draw(RR_TOP);
  CHECK(find("Finn would like to battle!"), "the rival's name on the info page");
  press(BTN_CANCEL);
  press(BTN_DOWN);
  press(BTN_ACTION);
  draw(RR_TOP);
  CHECK(find("Ivy would like to battle!"), "the female rival's name on the info page");
  press(BTN_CANCEL);
  press(BTN_DOWN);
  press(BTN_DOWN);
  press(BTN_ACTION);
  draw(RR_TOP);
  CHECK(find("Defeated") && !find_prefix("Defeated by"), "a defeat that names nothing says only Defeated");
  press(BTN_CANCEL);
  settle();
  draw(RR_BOTTOM);
  CHECK(find("Defeated"), "... in the list too");

  /* B on the list leaves, once */
  CHECK(press(BTN_CANCEL) && n_exit == 1, "B on the list leaves");

  /* nothing happens to a run with no party on the info page */
  n_entries = 1;
  E[0].n_party = 0;
  open_screen(RH_OK);
  press(BTN_ACTION);
  draw(RR_TOP);
  draw(RR_BOTTOM);
  const int opened = n_summary_open;
  press(BTN_ACTION);
  press(BTN_DOWN);
  press(BTN_UP);
  CHECK(n_summary_open == opened && ui_top() == g_screen, "no Pokemon, no Summary screen");

  /* ---- taps (physical bottom-screen pixels): the list's rows are at y = 8 + 56 i, 52 high, x 4..312 */
  n_entries = 0;
  open_screen(RH_OK);
  {
    ui_screen* sc = ui_top();
    CHECK(sc->touch != NULL, "the history screen takes taps");
    n_select = 0;
    CHECK(!sc->touch(sc, 100, 30) && n_select == 0 && g_depth == 1, "an empty history takes no tap");
  }
  make_history();
  open_screen(RH_OK);
  settle();
  draw(RR_BOTTOM);
  {
    ui_screen* sc = ui_top();
    n_select = 0;
    CHECK(!sc->touch(sc, 2, 30) && !sc->touch(sc, 312, 30), "a tap beside the rows takes nothing");
    CHECK(!sc->touch(sc, 100, 5) && !sc->touch(sc, 100, 62), "... nor above the first row or in the gap between two");
    CHECK(n_select == 0, "(no sound for those)");
    CHECK(sc->touch(sc, 100, 70) && n_select == 1, "a tap on the second run points at it");
    draw(RR_TOP);
    CHECK(g_summary_info.wave == 30, "the top screen shows it (wave %d)", g_summary_info.wave);
    draw(RR_BOTTOM);
    CHECK(!find("A: Summary"), "it is still the list");
    CHECK(sc->touch(sc, 308, 130) && n_select == 2, "a tap on a run that is not pointed points at that one (x 308 is inside the rows)");
    draw(RR_TOP);
    CHECK(g_summary_info.wave == 50, "... the third (wave %d)", g_summary_info.wave);
    CHECK(sc->touch(sc, 100, 236), "the fifth run shows 8 px above the screen's edge and can be pointed at");
    draw(RR_TOP);
    CHECK(g_summary_info.wave == 90, "wave %d", g_summary_info.wave);
    settle();
    CHECK(sc->touch(sc, 100, 10), "once the list has scrolled the rows are where they are drawn: the top one is the second run now");
    draw(RR_TOP);
    CHECK(g_summary_info.wave == 30, "wave %d", g_summary_info.wave);
    n_select = 0;
    const int base = n_summary_open;
    n_summary_touch = 0;
    CHECK(sc->touch(sc, 100, 20) && n_select == 1, "a tap on the pointed run opens its info page");
    draw(RR_BOTTOM);
    CHECK(find("A: Summary") && find("B: Back"), "the info page's bottom screen has the hints");
    CHECK(find("A: Summary")->x == 16 && find("B: Back")->x == 16 + 60 + 18, "(each one after the last: x %.0f and %.0f)", find("A: Summary")->x, find("B: Back")->x);
    CHECK(!sc->touch(sc, 100, 100) && !sc->touch(sc, 100, 60) && !sc->touch(sc, 300, 230) && n_summary_open == base && ui_top() == sc, "between the name and the hints, and beyond the hints, a tap takes nothing");
    CHECK(sc->touch(sc, 30, 30), "a tap on the Pokemon's name goes on to the next one of the run");
    CHECK(sc->touch(sc, 20, 230) && n_summary_open == base + 1 && g_sum_args.index == 1 && g_sum_args.n == 2, "a tap on A: Summary opens the Summary screen at it (index %d)", g_sum_args.index);
    CHECK(!strcmp(ui_top()->name, "history-summary"), "over the info page");
    CHECK(ui_top()->touch(ui_top(), 50, 100) && n_summary_touch == 1 && g_sum_touch_x == 50 && g_sum_touch_y == 70, "taps go to the Summary screen on its canvas (y 100 is 70)");
    g_summary_active = false;
    CHECK(!ui_top()->touch(ui_top(), 50, 100) && n_summary_touch == 1, "... only while it is up");
    g_sum_ev.on_close(g_sum_ev.ctx);
    CHECK(sc->touch(sc, 30, 30), "the name again wraps to the first");
    CHECK(sc->touch(sc, 20, 230) && g_sum_args.index == 0, "(index %d)", g_sum_args.index);
    g_sum_ev.on_close(g_sum_ev.ctx);
    CHECK(sc->touch(sc, 100, 230) && ui_top() == sc, "a tap on B: Back goes back to the list");
    draw(RR_BOTTOM);
    CHECK(!find("A: Summary"), "(the list again)");
    CHECK(n_exit == 0, "without leaving the history");

    /* the name's tap goes DOWN the party, wrapping: four Pokemon in the run that is the fourth row now (the list has scrolled by one) */
    CHECK(sc->touch(sc, 100, 130) && sc->touch(sc, 100, 130), "(the info page of the run at y 130)");
    for (int k = 1; k <= 4; k++) {
      CHECK(sc->touch(sc, 30, 59), "a tap on the name at its lowest pixel counts");
      sc->touch(sc, 20, 230);
      const int want = k % 4;
      CHECK(g_sum_args.n == 4 && g_sum_args.index == want, "tap %d: Pokemon %d of 4 (index %d)", k, want, g_sum_args.index);
      g_sum_ev.on_close(g_sum_ev.ctx);
    }
  }

  printf("history_screen_test: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
